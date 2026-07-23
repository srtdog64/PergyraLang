#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_domain_lookup.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_host_methods.h"
#include "llvm_inventory_internal.h"
#include "llvm_backend_type_map_internal.h"
#include "llvm_mir_slice_fact.h"
#include "llvm_stmt_source_local_fallback.h"
#include "llvm_stmt_type_infer_helpers.h"
#include "codegen_match_variant_policy.h"
#include "../parser/ast_api.h"

#include <stdarg.h>

static bool
llvm_stmt_call_type_reasonf(char *out, size_t out_size, const char *fmt, ...)
{
    va_list ap;
    int written;

    if (out == NULL || out_size == 0 || fmt == NULL)
        return false;
    va_start(ap, fmt);
    written = vsnprintf(out, out_size, fmt, ap);
    va_end(ap);
    return written >= 0 && (size_t)written < out_size;
}

static LLVMTypeRef
llvm_stmt_host_method_return_type(LLVMGenCtx *ctx, const char *host_type_name,
                                  const char *method_name)
{
    ASTNode *ret_ty = NULL;
    const MIRDeclMethod *method_meta = NULL;
    const MIRRoutine *method_routine = NULL;
    const MIRCallableSig *return_callable_sig = NULL;

    if (ctx == NULL || host_type_name == NULL || method_name == NULL)
        return NULL;
    method_meta = llvm_find_host_method_metadata_in_context(
        ctx, host_type_name, method_name);
    method_routine = llvm_mir_decl_method_routine(ctx, method_meta);
    return_callable_sig = method_routine != NULL
        ? llvm_mir_routine_return_callable_sig(method_routine)
        : NULL;
    if (!llvm_mir_decl_method_metadata_complete_for(ctx, method_meta,
            host_type_name, method_name,
            LLVM_MIR_DECL_METHOD_REQUIRE_RETURN_TYPE_NAME,
            "MIR-only LLVM path missing method type inference return type-name metadata for '%s.%s'",
            NULL)) {
        return NULL;
    }
    {
        const char *ret_name =
            llvm_mir_decl_method_return_type_name(method_meta);
        if (return_callable_sig != NULL) {
            LLVMTypeRef llvm_ret = llvm_mir_callable_sig_to_llvm(
                ctx, return_callable_sig);
            if (llvm_ret != NULL && !ctx->has_error)
                return llvm_ret;
            return NULL;
        }
        if (ret_name != NULL) {
            LLVMTypeRef llvm_ret = pergyra_type_to_llvm(ctx, ret_name);
            if (llvm_ret != NULL && !ctx->has_error)
                return llvm_ret;
            return NULL;
        }
    }
    ret_ty = llvm_mir_decl_method_return_type(method_meta);
    if (ret_ty == NULL && method_meta == NULL) {
        /* MIR-only: method return shape is owned by MIR metadata; the non-MIR
         * AST method lookup is retired, so a missing row fails closed. */
        if (llvm_callable_decl_exists(ctx, method_name))
            return NULL;
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing method return metadata for '%s.%s'",
            host_type_name != NULL ? host_type_name : "(anonymous)",
            method_name != NULL ? method_name : "(anonymous)");
        return NULL;
    }
    if (ret_ty != NULL) {
        if (llvm_active_has_mir(ctx)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing method type inference return ABI fact for '%s.%s'",
                host_type_name,
                method_name);
            return NULL;
        }
        LLVMTypeRef llvm_ret = ast_type_to_llvm(ctx, ret_ty);
        if (llvm_ret != NULL && !ctx->has_error)
            return llvm_ret;
    }
    return NULL;
}

static LLVMTypeRef
llvm_stmt_callable_entry_return_type(LLVMGenCtx *ctx,
                                     const LLVMCallableVarEntry *entry)
{
    ASTNode *return_type;

    if (ctx == NULL || entry == NULL)
        return NULL;
    if (entry->value_callable_sig != NULL) {
        const char *return_name =
            entry->value_callable_sig->return_type_name;
        return return_name != NULL
            ? pergyra_type_to_llvm(ctx, return_name)
            : ctx->type_void;
    }
    if (entry->return_callable_sig != NULL)
        return llvm_mir_callable_sig_to_llvm(ctx,
            entry->return_callable_sig);
    if (entry->type_node != NULL
        && entry->type_node->type == AST_EVENT_HANDLER_TYPE) {
        return_type = ast_event_handler_return_type(entry->type_node);
    } else {
        if (entry->return_type_name != NULL)
            return pergyra_type_to_llvm(ctx, entry->return_type_name);
        return_type = entry->return_type;
    }
    if (return_type == NULL)
        return ctx->type_void;
    return ast_type_to_llvm(ctx, return_type);
}

static LLVMTypeRef
llvm_stmt_contextual_option_type(LLVMGenCtx *ctx)
{
    LLVMTypeRef candidate = NULL;

    if (ctx == NULL)
        return NULL;
    if (ctx->expected_type_name != NULL
        && pgy_classify_type(ctx->expected_type_name) == PGY_TK_OPTION) {
        candidate = pergyra_type_to_llvm(ctx, ctx->expected_type_name);
        if (candidate != NULL)
            return candidate;
    }
    candidate = ctx->current_ret_type;
    if (candidate != NULL
        && LLVMGetTypeKind(candidate) == LLVMStructTypeKind
        && LLVMCountStructElementTypes(candidate) == 2
        && LLVMStructGetTypeAtIndex(candidate, 0) == ctx->type_i32) {
        return candidate;
    }
    return NULL;
}

static LLVMTypeRef
llvm_stmt_contextual_result_type(LLVMGenCtx *ctx)
{
    LLVMTypeRef candidate = NULL;

    if (ctx == NULL)
        return NULL;
    if (ctx->expected_type_name != NULL
        && pgy_classify_type(ctx->expected_type_name) == PGY_TK_RESULT) {
        candidate = pergyra_type_to_llvm(ctx, ctx->expected_type_name);
        if (candidate != NULL)
            return candidate;
    }
    candidate = ctx->current_ret_type;
    if (candidate != NULL
        && LLVMGetTypeKind(candidate) == LLVMStructTypeKind
        && LLVMCountStructElementTypes(candidate) == 3
        && LLVMStructGetTypeAtIndex(candidate, 0) == ctx->type_i32) {
        return candidate;
    }
    return NULL;
}

static LLVMTypeRef
llvm_stmt_infer_scalar_math_return_type(LLVMGenCtx *ctx, ASTNode *call,
                                        const char *callee)
{
    LLVMTypeRef ty0;
    LLVMTypeRef ty1;
    LLVMTypeRef ty2;
    size_t argc;

    if (ctx == NULL || call == NULL || call->type != AST_CALL
        || callee == NULL) {
        return NULL;
    }
    argc = ast_call_arg_count(call);
    if (strcmp(callee, "Abs") == 0 && argc == 1)
        return llvm_stmt_infer_expr_type(ctx, ast_call_argument(call, 0));
    if ((strcmp(callee, "Min") == 0 || strcmp(callee, "Max") == 0)
        && argc == 2) {
        bool active_mir = llvm_active_has_mir(ctx);
        bool prev_err = ctx->has_error;
        ty0 = llvm_stmt_infer_expr_type(ctx, ast_call_argument(call, 0));
        if (ctx->has_error && !prev_err) {
            if (active_mir)
                return NULL;
            ctx->has_error = false;
            ty0 = NULL;
        }
        ty1 = llvm_stmt_infer_expr_type(ctx, ast_call_argument(call, 1));
        if (ctx->has_error && !prev_err) {
            if (active_mir)
                return NULL;
            ctx->has_error = false;
            ty1 = NULL;
        }
        if (active_mir && (ty0 == NULL || ty1 == NULL)) {
            return llvm_stmt_unknown_expr_type(ctx, call,
                "Min/Max call requires typed operands in MIR-backed inference");
        }
        if (ty0 != NULL && ty1 != NULL)
            return llvm_stmt_promote_numeric_type(ctx, ty0, ty1);
        if (ty0 != NULL)
            return ty0;
        if (ty1 != NULL)
            return ty1;
        return ctx->type_i32;
    }
    if (strcmp(callee, "Clamp") == 0 && argc == 3) {
        ty0 = llvm_stmt_infer_expr_type(ctx, ast_call_argument(call, 0));
        ty1 = llvm_stmt_infer_expr_type(ctx, ast_call_argument(call, 1));
        ty2 = llvm_stmt_infer_expr_type(ctx, ast_call_argument(call, 2));
        return llvm_stmt_promote_numeric_type(ctx,
            llvm_stmt_promote_numeric_type(ctx, ty0, ty1), ty2);
    }
    if ((strcmp(callee, "E") == 0 || strcmp(callee, "PI") == 0)
        && argc == 0) {
        return ctx->type_f32;
    }
    if ((strcmp(callee, "CheckedAdd") == 0 || strcmp(callee, "CheckedMul") == 0)
        && argc == 2) {
        return ctx->type_i32;
    }
    return NULL;
}

static LLVMTypeRef
llvm_stmt_infer_member_call_type(LLVMGenCtx *ctx, ASTNode *expr)
{
    ASTNode *callee_node = ast_call_callee(expr);
    ASTNode *receiver = ast_member_object(callee_node);
    const char *method_name = ast_member_name(callee_node);
    const char *receiver_name =
        receiver->type == AST_IDENTIFIER ? ast_identifier_name(receiver) : NULL;
    const char *inner = llvm_stmt_lookup_slot_or_view_inner(ctx, receiver_name);

    if (inner != NULL && llvm_stmt_slot_call_returns_value(method_name))
        return pergyra_type_to_llvm(ctx, inner);
    if (inner != NULL && llvm_stmt_call_is_slot_builtin(method_name))
        return ctx->type_void;
    if (strcmp(method_name, "Slice") == 0) {
        LLVMTypeRef elem_type =
            llvm_stmt_resolve_array_elem_type(ctx, receiver, NULL);
        const char *suffix = llvm_type_to_suffix(ctx, elem_type);
        if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
            return llvm_slice_struct_type(ctx, suffix);
    }
    if (receiver_name != NULL) {
        const char *class_name = llvm_lookup_var_class(ctx, receiver_name);
        if (class_name == NULL)
            class_name = llvm_current_field_class_name(ctx, receiver_name);
        if (class_name == NULL)
            class_name = llvm_current_zone_slot_type_name(ctx, receiver_name);
        if (class_name == NULL)
            class_name = llvm_expr_custom_type_name(receiver, ctx);
        if (class_name == NULL) {
            LLVMClassTypeEntry *e = llvm_stmt_source_local_class(ctx, receiver);
            if (e != NULL && e->class_name != NULL)
                class_name = e->class_name;
        }
        if (class_name == NULL) {
            if (llvm_active_has_mir(ctx)) {
                /* Fail closed instead of clearing the source-of-truth diagnostic. */
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing member-call receiver type metadata for '%s.%s'",
                    receiver_name != NULL ? receiver_name : "(anonymous)",
                    method_name != NULL ? method_name : "(anonymous)");
                return NULL;
            }
            {
                bool prev_err = ctx->has_error;
                LLVMTypeRef recv_ty = llvm_stmt_infer_expr_type(ctx, receiver);
                if (ctx->has_error && !prev_err) {
                    ctx->has_error = false;
                    recv_ty = NULL;
                }
                if (recv_ty != NULL) {
                    LLVMClassTypeEntry *recv_cls =
                        llvm_stmt_lookup_class_by_type(ctx, recv_ty);
                    if (recv_cls != NULL && recv_cls->class_name != NULL)
                        class_name = recv_cls->class_name;
                }
            }
        }
        if (class_name != NULL) {
            LLVMTypeRef method_ret =
                llvm_stmt_host_method_return_type(ctx, class_name, method_name);
            if (method_ret != NULL)
                return method_ret;
        }
    }
    if (receiver_name == NULL) {
        const char *recv_class =
            llvm_stmt_infer_nominal_name_from_init(ctx, receiver);
        if (recv_class == NULL) {
            LLVMTypeRef recv_ty = llvm_stmt_infer_expr_type(ctx, receiver);
            LLVMClassTypeEntry *recv_cls =
                !ctx->has_error && recv_ty != NULL
                    ? llvm_stmt_lookup_class_by_type(ctx, recv_ty)
                    : NULL;
            if (recv_cls != NULL)
                recv_class = recv_cls->class_name;
        }
        if (recv_class != NULL) {
            LLVMTypeRef method_ret =
                llvm_stmt_host_method_return_type(ctx, recv_class, method_name);
            if (method_ret != NULL)
                return method_ret;
        }
    }
    if (receiver_name == NULL && receiver->type == AST_MEMBER_ACCESS) {
        const char *recv_class = llvm_expr_custom_type_name(receiver, ctx);
        if (recv_class != NULL) {
            LLVMTypeRef method_ret =
                llvm_stmt_host_method_return_type(ctx, recv_class, method_name);
            if (method_ret != NULL)
                return method_ret;
        }
    }
    return NULL;
}

LLVMTypeRef
llvm_stmt_infer_call_expr_type(LLVMGenCtx *ctx, ASTNode *expr)
{
    ASTNode *callee_node = ast_call_callee(expr);

    if (callee_node != NULL && callee_node->type == AST_MEMBER_ACCESS
        && ast_member_name(callee_node) != NULL
        && ast_member_object(callee_node) != NULL) {
        ASTNode *owner = ast_member_object(callee_node);
        if (owner->type == AST_IDENTIFIER
            && ast_identifier_name(owner) != NULL) {
            LLVMTypeRef qualified_ret =
                llvm_stmt_lookup_qualified_call_return_type(ctx,
                    ast_identifier_name(owner), ast_member_name(callee_node));
            if (qualified_ret != NULL || ctx->has_error)
                return qualified_ret;
        }
        LLVMTypeRef member_type = llvm_stmt_infer_member_call_type(ctx, expr);
        if (member_type != NULL || ctx->has_error)
            return member_type;
    }
    if (callee_node != NULL && callee_node->type == AST_IDENTIFIER
        && ast_identifier_name(callee_node) != NULL) {
        const char *callee = ast_identifier_name(callee_node);
        PgyCodegenMatchVariantKind match_variant =
            pgy_codegen_match_variant_lookup(callee);
        if (match_variant == PGY_MATCH_VARIANT_SOME
            || match_variant == PGY_MATCH_VARIANT_NONE_CTOR) {
            LLVMTypeRef option_ty = llvm_stmt_contextual_option_type(ctx);
            if (option_ty != NULL)
                return option_ty;
        }
        if (match_variant == PGY_MATCH_VARIANT_OK
            || match_variant == PGY_MATCH_VARIANT_ERR) {
            LLVMTypeRef result_ty = llvm_stmt_contextual_result_type(ctx);
            if (result_ty != NULL)
                return result_ty;
        }
        {
            LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, callee);
            if (variant != NULL) {
                LLVMClassTypeEntry *enum_cls =
                    llvm_lookup_class(ctx, variant->enum_name);
                if (enum_cls != NULL && enum_cls->struct_type != NULL)
                    return enum_cls->struct_type;
                if (llvm_enum_type_exists(ctx, variant->enum_name))
                    return ctx->type_i32;
            }
        }
        if (strcmp(callee, "SliceCopy") == 0 && ast_call_arg_count(expr) == 1) {
            LLVMTypeRef slice_ty =
                llvm_stmt_infer_expr_type(ctx, ast_call_argument(expr, 0));
            LLVMTypeRef array_ty =
                llvm_mir_slice_fact_array_type_from_slice_type(ctx, slice_ty);
            if (array_ty != NULL)
                return array_ty;
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "SliceCopy requires concrete Slice<T> operand");
        }
        if (strcmp(callee, "SetValues") == 0
            && ast_call_arg_count(expr) == 1
            && ast_call_argument(expr, 0) != NULL
            && ast_call_argument(expr, 0)->type == AST_IDENTIFIER) {
            const char *inner = llvm_lookup_set_inner(ctx,
                ast_identifier_name(ast_call_argument(expr, 0)));
            if (inner != NULL)
                return llvm_array_struct_type(ctx, inner);
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "SetValues requires registered Set<T> metadata");
        }
        if (llvm_stmt_call_is_slot_builtin(callee)
            && ast_call_arg_count(expr) >= 1
            && ast_call_argument(expr, 0) != NULL
            && ast_call_argument(expr, 0)->type == AST_IDENTIFIER) {
            const char *receiver_name =
                ast_identifier_name(ast_call_argument(expr, 0));
            const char *inner =
                llvm_stmt_lookup_slot_or_view_inner(ctx, receiver_name);
            if (inner != NULL && llvm_stmt_slot_call_returns_value(callee))
                return pergyra_type_to_llvm(ctx, inner);
            if (inner != NULL)
                return ctx->type_void;
            if (ctx->expected_type_name != NULL) {
                LLVMTypeRef expected =
                    pergyra_type_to_llvm(ctx, ctx->expected_type_name);
                if (expected != NULL)
                    return expected;
            }
            return llvm_stmt_unknown_expr_type(ctx, expr,
                llvm_stmt_slot_call_returns_value(callee)
                    ? "slot Read requires registered Slot<T>/view metadata"
                    : "slot operation requires registered Slot<T>/view metadata");
        }
        {
            LLVMTypeRef builtin_type =
                llvm_stmt_infer_builtin_return_type(ctx, callee);
            if (builtin_type != NULL)
                return builtin_type;
        }
        {
            LLVMTypeRef math_type =
                llvm_stmt_infer_scalar_math_return_type(ctx, expr, callee);
            if (math_type != NULL)
                return math_type;
        }
        if (llvm_current_host_class_name(ctx) != NULL
            && !llvm_stmt_call_is_slot_builtin(callee)
            && strcmp(callee, "Log") != 0
            && strcmp(callee, "Print") != 0
            && strcmp(callee, "ToString") != 0
            && strcmp(callee, "Clone") != 0) {
            LLVMTypeRef method_ret = llvm_stmt_host_method_return_type(
                ctx, llvm_current_host_class_name(ctx), callee);
            if (method_ret != NULL)
                return method_ret;
        }
        if (strcmp(callee, "Clone") == 0 && ast_call_arg_count(expr) >= 1
            && ast_call_argument(expr, 0) != NULL) {
            LLVMTypeRef arg_type =
                llvm_stmt_infer_expr_type(ctx, ast_call_argument(expr, 0));
            if (arg_type != NULL)
                return arg_type;
        }
        {
            LLVMCallableVarEntry *callable =
                llvm_lookup_callable_entry(ctx, callee);
            LLVMTypeRef ret_type =
                llvm_stmt_callable_entry_return_type(ctx, callable);
            if (ret_type != NULL && !ctx->has_error)
                return ret_type;
        }
        {
            LLVMFuncEntry *fn = llvm_stmt_lookup_visible_function(ctx, callee);
            if (fn != NULL)
                return fn->ret_type;
        }
        {
            LLVMTypeRef declared_type =
                llvm_stmt_lookup_declared_call_return_type(ctx, callee);
            if (declared_type != NULL)
                return declared_type;
        }
        if (llvm_intent_decl_exists(ctx, callee))
            return ctx->type_i1;
        if (llvm_stmt_call_returns_collection_value(callee)
            && ast_call_arg_count(expr) >= 1
            && ast_call_argument(expr, 0) != NULL
            && ast_call_argument(expr, 0)->type == AST_IDENTIFIER) {
            const char *inner = llvm_stmt_lookup_collection_get_inner(ctx,
                callee, ast_identifier_name(ast_call_argument(expr, 0)));
            if (inner != NULL)
                return pergyra_type_to_llvm(ctx, inner);
        }
        {
            LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, callee);
            if (cls != NULL && cls->struct_type != NULL)
                return cls->struct_type;
        }
    }
    /* A runtime call parameter is the final ABI owner for its argument type.
     * Use that scoped fact instead of teaching this inference layer the names
     * of dependent-return helpers such as Option/Result unwrap operations. */
    if (ctx->expected_abi_type != NULL)
        return ctx->expected_abi_type;
    /* Domain helper result types are owned by typed inference. */
    if (ctx->expected_type_name != NULL) {
        LLVMTypeRef expected = pergyra_type_to_llvm(ctx, ctx->expected_type_name);
        if (expected != NULL)
            return expected;
    }
    {
        char reason[256];
        const char *callee = NULL;
        if (callee_node != NULL && callee_node->type == AST_IDENTIFIER)
            callee = ast_identifier_name(callee_node);
        if (callee_node != NULL && callee_node->type == AST_MEMBER_ACCESS)
            callee = ast_member_name(callee_node);
        if (!llvm_stmt_call_type_reasonf(reason, sizeof(reason),
                "call '%s' requires registered function or expected type metadata",
                callee != NULL ? callee : "<expr>")) {
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "call result requires registered function or expected type metadata");
        }
        return llvm_stmt_unknown_expr_type(ctx, expr, reason);
    }
}

#endif /* PGY_LLVM_ENABLED */
