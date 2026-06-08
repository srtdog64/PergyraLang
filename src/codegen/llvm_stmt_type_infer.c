#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_host_methods.h"
#include "llvm_stmt_type_infer_helpers.h"
#include "codegen_match_variant_policy.h"
#include "../parser/ast_api.h"

#include <stdarg.h>
static bool
llvm_stmt_type_reasonf(char *out, size_t out_size, const char *fmt, ...)
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

LLVMClassTypeEntry *
llvm_stmt_lookup_class_by_type(LLVMGenCtx *ctx, LLVMTypeRef type)
{
    return llvm_lookup_class_by_struct_type(ctx, type);
}

LLVMTypeRef
llvm_stmt_unknown_expr_type(LLVMGenCtx *ctx, ASTNode *expr, const char *reason)
{
    if (ctx == NULL)
        return NULL;
    if (!ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, expr,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM expression type inference requires a concrete type: %s",
            reason != NULL ? reason : "unknown expression");
    }
    return NULL;
}

static LLVMTypeRef
llvm_stmt_host_method_return_type(LLVMGenCtx *ctx, const char *host_type_name,
                                  const char *method_name)
{
    ASTNode *ret_ty = NULL;
    const MIRDeclMethod *method_meta = NULL;

    if (ctx == NULL || host_type_name == NULL || method_name == NULL)
        return NULL;

    method_meta = llvm_find_host_method_metadata_in_context(
        ctx, host_type_name, method_name);
    if (!llvm_mir_decl_method_metadata_complete_for(ctx,
            method_meta,
            host_type_name,
            method_name,
            LLVM_MIR_DECL_METHOD_REQUIRE_RETURN_TYPE_NAME,
            "MIR-only LLVM path missing method type inference return type-name metadata for '%s.%s'",
            NULL)) {
        return NULL;
    }
    {
        const char *ret_name =
            llvm_mir_decl_method_return_type_name(method_meta);
        if (ret_name != NULL) {
            LLVMTypeRef llvm_ret = pergyra_type_to_llvm(ctx, ret_name);
            if (llvm_ret != NULL && !ctx->has_error)
                return llvm_ret;
            return NULL;
        }
    }
    ret_ty = llvm_mir_decl_method_return_type(method_meta);
    if (ret_ty == NULL && method_meta == NULL) {
        if (llvm_active_has_mir(ctx)) {
            /* A registered global function is not a host method; let the
             * caller's fallback chain resolve it through function metadata. */
            ASTNode *cd = llvm_find_callable_decl(ctx, method_name);
            if (cd != NULL && cd->type == AST_FUNC_DECL)
                return NULL;
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing method return metadata for '%s.%s'",
                host_type_name != NULL ? host_type_name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)");
            return NULL;
        }
        ASTNode *method_decl = llvm_find_nominal_host_method_decl(
            ctx, host_type_name, method_name);
        if (method_decl != NULL && method_decl->type == AST_FUNC_DECL)
            ret_ty = ast_func_return_type(method_decl);
    }
    if (ret_ty != NULL) {
        LLVMTypeRef llvm_ret = ast_type_to_llvm(ctx, ret_ty);
        if (llvm_ret != NULL && !ctx->has_error)
            return llvm_ret;
        return NULL;
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
    if (entry->type_node != NULL
        && entry->type_node->type == AST_EVENT_HANDLER_TYPE) {
        return_type = ast_event_handler_return_type(entry->type_node);
    } else {
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
        bool prev_err = ctx->has_error;
        ty0 = llvm_stmt_infer_expr_type(ctx, ast_call_argument(call, 0));
        if (ctx->has_error && !prev_err) {
            ctx->has_error = false;
            ty0 = NULL;
        }
        ty1 = llvm_stmt_infer_expr_type(ctx, ast_call_argument(call, 1));
        if (ctx->has_error && !prev_err) {
            ctx->has_error = false;
            ty1 = NULL;
        }
        /* Closure #87b: when one side can't be inferred (e.g. an as-yet
         * unregistered local in MIR-only mode), promote from the
         * resolvable side. Falling back to Int32 keeps deeper Min/Max
         * chains (biome_simulator, campaign_graph_fsm) from cascading
         * into the strict identifier-metadata error. */
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
    return NULL;
}

LLVMTypeRef
llvm_stmt_infer_expr_type(LLVMGenCtx *ctx, ASTNode *expr)
{
    const char *nominal_name;
    LLVMClassTypeEntry *nominal_cls;

    if (ctx == NULL)
        return NULL;
    if (expr == NULL)
        return llvm_stmt_unknown_expr_type(ctx, expr, "missing expression");

    nominal_name = llvm_stmt_infer_nominal_name_from_init(ctx, expr);
    nominal_cls = nominal_name != NULL ? llvm_lookup_class(ctx, nominal_name) : NULL;
    if (nominal_cls != NULL)
        return nominal_cls->struct_type;

    switch (expr->type) {
    case AST_LET_DECL: {
        ASTNode *type_ann = ast_let_type(expr);
        ASTNode *initializer = ast_let_initializer(expr);
        if (type_ann != NULL)
            return ast_type_to_llvm(ctx, type_ann);
        if (initializer != NULL)
            return llvm_stmt_infer_expr_type(ctx, initializer);
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "let declaration requires an annotation or initializer type");
    }
    case AST_STRING:
        return ctx->type_i8ptr;
    case AST_BOOLEAN:
        return ctx->type_i1;
    case AST_NUMBER: {
        double val = ast_number_value(expr);
        if (ast_number_is_long(expr))
            return ctx->type_i64;
        if (ast_number_is_float(expr))
            return ctx->type_f32;
        if (val == (int64_t)val
            && val >= -2147483648.0
            && val <= 2147483647.0) {
            return ctx->type_i32;
        }
        if (val == (double)(int64_t)val
            && val >= -9.2233720368547758e+18
            && val <=  9.2233720368547758e+18) {
            return ctx->type_i64;
        }
        return ctx->type_f64;
    }
    case AST_ARRAY_LITERAL: {
        LLVMTypeRef elem_type = NULL;
        const char *suffix = NULL;
        char suffix_buf[256];
        if (ast_array_literal_count(expr) > 0
            && ast_array_literal_element(expr, 0) != NULL) {
            elem_type = llvm_stmt_infer_expr_type(ctx,
                ast_array_literal_element(expr, 0));
            suffix = llvm_type_to_suffix(ctx, elem_type);
            if (suffix == NULL || strcmp(suffix, "Unknown") == 0)
                return llvm_stmt_unknown_expr_type(ctx, expr,
                    "array literal element type is unresolved");
        } else if (ctx->expected_type_name != NULL
                   && pgy_classify_type(ctx->expected_type_name)
                        == PGY_TK_ARRAY) {
            if (llvm_constructed_arg_name_copy(ctx->expected_type_name, 0,
                    suffix_buf, sizeof(suffix_buf))) {
                suffix = suffix_buf;
            }
        }
        if (suffix == NULL || suffix[0] == '\0'
            || strcmp(suffix, "Unknown") == 0)
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "empty array literal requires an explicit Array<T> context");
        if (strlen(suffix) >= sizeof(suffix_buf))
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "array literal element type name is too long");
        memcpy(suffix_buf, suffix, strlen(suffix) + 1);
        suffix = suffix_buf;
        return llvm_array_struct_type(ctx, suffix);
    }
    case AST_TUPLE_LITERAL: {
        size_t count = ast_tuple_literal_count(expr);
        LLVMTypeRef *fields;

        if (count < 2)
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "tuple literal requires at least two elements");
        if (count > UINT_MAX)
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "tuple literal exceeds LLVM struct field limit");
        if (count > SIZE_MAX / sizeof(*fields))
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "tuple literal field allocation would overflow");
        fields = pgy_arena_calloc(&ctx->scratch, count * sizeof(*fields));
        if (fields == NULL)
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "tuple literal field allocation failed");
        for (size_t i = 0; i < count; i++) {
            fields[i] = llvm_stmt_infer_expr_type(ctx,
                ast_tuple_literal_element(expr, i));
            if (ctx->has_error || fields[i] == NULL)
                return NULL;
        }
        return LLVMStructTypeInContext(ctx->context, fields,
            (unsigned)count, 0);
    }
    case AST_LAMBDA_EXPR: {
        LLVMTypeRef lambda_type = llvm_stmt_lambda_signature_type(ctx, expr);
        if (ctx->has_error || lambda_type == NULL)
            return NULL;
        return lambda_type;
    }
    case AST_ARRAY_ACCESS: {
        ASTNode *array = ast_array_access_array(expr);
        LLVMTypeRef elem_type =
            llvm_stmt_resolve_array_elem_type(ctx, array, NULL);
        if (elem_type != NULL)
            return elem_type;
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "indexed collection access requires registered Array<T>/Slice<T> metadata");
    }
    case AST_IDENTIFIER: {
        const char *name = ast_identifier_name(expr);
        LLVMVarEntry var;
        bool has_var = llvm_scope_lookup_snapshot(ctx, name, &var);
        if (pgy_codegen_match_variant_lookup(name)
                == PGY_MATCH_VARIANT_NONE_CTOR) {
            if (ctx->expected_type_name != NULL
                && pgy_classify_type(ctx->expected_type_name)
                    == PGY_TK_OPTION) {
                LLVMTypeRef expected = pergyra_type_to_llvm(
                    ctx, ctx->expected_type_name);
                if (expected != NULL)
                    return expected;
            }
            if (ctx->current_ret_type != NULL
                && LLVMGetTypeKind(ctx->current_ret_type)
                    == LLVMStructTypeKind
                && LLVMCountStructElementTypes(ctx->current_ret_type) == 2
                && LLVMStructGetTypeAtIndex(ctx->current_ret_type, 0)
                    == ctx->type_i32) {
                return ctx->current_ret_type;
            }
        }
        if (has_var)
            return var.type;
        if (name != NULL && strcmp(name, "self") != 0
            && llvm_current_host_class_name(ctx) != NULL) {
            LLVMClassTypeEntry *host_cls = llvm_lookup_class(
                ctx, llvm_current_host_class_name(ctx));
            int field_idx = host_cls != NULL
                ? llvm_class_field_index(host_cls, name)
                : -1;
        if (field_idx >= 0)
                return llvm_class_field_type_at_index(host_cls, field_idx);
        }
        {
            const char *custom_type = llvm_expr_custom_type_name(expr, ctx);
            if (custom_type != NULL) {
                LLVMTypeRef ty = pergyra_type_to_llvm(ctx, custom_type);
                if (ty != NULL)
                    return ty;
            }
        }
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "identifier requires registered LLVM local metadata");
    }
    case AST_ASSIGNMENT:
        if (ast_assignment_target(expr) != NULL
            && ast_assignment_target(expr)->type == AST_IDENTIFIER) {
            LLVMVarEntry var;
            const char *target_name =
                ast_identifier_name(ast_assignment_target(expr));
            if (llvm_scope_lookup_snapshot(ctx, target_name, &var))
                return var.type;
        }
        if (ast_assignment_value(expr) != NULL)
            return llvm_stmt_infer_expr_type(ctx, ast_assignment_value(expr));
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "assignment is missing a value expression");
    case AST_UNARY: {
        PgyTokenType op = ast_unary_operator(expr).type;
        if (op == TOKEN_NOT)
            return ctx->type_i1;
        if (op == TOKEN_MINUS)
            return llvm_stmt_infer_expr_type(ctx, ast_unary_operand(expr));
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "unsupported unary operator has no inferred LLVM type");
    }
    case AST_CHANNEL_RECV:
        if (ast_channel_recv_channel(expr) != NULL
            && ast_channel_recv_channel(expr)->type == AST_IDENTIFIER) {
            ASTNode *channel = ast_channel_recv_channel(expr);
            const char *name =
                ast_identifier_name(channel);
            const char *inner = llvm_resolve_channel_target_inner(ctx, expr,
                channel, "channel receive expression");
            if (inner != NULL)
                return pergyra_type_to_llvm(ctx, inner);
            if (name != NULL && llvm_scope_contains(ctx, name)) {
                char reason[256];
                if (!llvm_stmt_type_reasonf(reason, sizeof(reason),
                        "channel receive '%s' has no registered Channel<T> metadata",
                        name)) {
                    return llvm_stmt_unknown_expr_type(ctx, expr,
                        "channel receive has no registered Channel<T> metadata");
                }
                return llvm_stmt_unknown_expr_type(ctx, expr, reason);
            }
        }
        /* If the enclosing let/return already supplied a concrete expected
         * value type, consume it. Otherwise a Channel<T> receive without
         * channel metadata is a source-of-truth gap, not an implicit Int. */
        if (ctx->expected_type_name != NULL
            && pgy_classify_type(ctx->expected_type_name) != PGY_TK_CHANNEL) {
            LLVMTypeRef expected = pergyra_type_to_llvm(
                ctx, ctx->expected_type_name);
            if (expected != NULL)
                return expected;
        }
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "channel receive requires registered Channel<T> metadata");
    case AST_MEMBER_ACCESS: {
        const char *base_name = llvm_stmt_infer_nominal_name_from_init(
            ctx, ast_member_object(expr));
        LLVMClassTypeEntry *base_cls = base_name != NULL
            ? llvm_lookup_class(ctx, base_name) : NULL;
        if (base_cls != NULL) {
            int field_idx = llvm_class_field_index(base_cls, ast_member_name(expr));
            if (field_idx >= 0)
                return llvm_class_field_type_at_index(base_cls, field_idx);
        }
        {
            char reason[256];
            ASTNode *member_object = ast_member_object(expr);
            const char *field_name = ast_member_name(expr) != NULL
                ? ast_member_name(expr) : "<field>";
            const char *base_expr_name =
                (member_object != NULL
                 && member_object->type == AST_IDENTIFIER
                 && ast_identifier_name(member_object) != NULL)
                    ? ast_identifier_name(member_object)
                    : NULL;
            if (!llvm_stmt_type_reasonf(reason, sizeof(reason),
                    "member access '%s:%s.%s' did not resolve to a known field",
                    base_expr_name != NULL ? base_expr_name : "<expr>",
                    base_name != NULL ? base_name : "<unknown>",
                    field_name)) {
                return llvm_stmt_unknown_expr_type(ctx, expr,
                    "member access did not resolve to a known field");
            }
            return llvm_stmt_unknown_expr_type(ctx, expr, reason);
        }
    }
    case AST_CALL:
        if (ast_call_callee(expr) != NULL
            && ast_call_callee(expr)->type == AST_MEMBER_ACCESS
            && ast_member_name(ast_call_callee(expr)) != NULL
            && ast_member_object(ast_call_callee(expr)) != NULL) {
            ASTNode *receiver = ast_member_object(ast_call_callee(expr));
            const char *method_name = ast_member_name(ast_call_callee(expr));
            const char *receiver_name = receiver->type == AST_IDENTIFIER
                ? ast_identifier_name(receiver) : NULL;
            const char *inner = llvm_stmt_lookup_slot_or_view_inner(
                ctx, receiver_name);
            if (inner != NULL && llvm_stmt_slot_call_returns_value(method_name))
                return pergyra_type_to_llvm(ctx, inner);
            if (inner != NULL
                && llvm_stmt_call_is_slot_builtin(method_name)) {
                return ctx->type_void;
            }
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
                    class_name = llvm_current_zone_slot_type_name(ctx,
                        receiver_name);
                if (class_name == NULL)
                    class_name = llvm_expr_custom_type_name(receiver, ctx);
                if (class_name != NULL) {
                    LLVMTypeRef method_ret =
                        llvm_stmt_host_method_return_type(
                            ctx, class_name, method_name);
                    if (method_ret != NULL)
                        return method_ret;
                }
            }
            if (receiver_name == NULL) {
                const char *recv_class =
                    llvm_stmt_infer_nominal_name_from_init(ctx, receiver);
                if (recv_class == NULL) {
                    LLVMTypeRef recv_ty =
                        llvm_stmt_infer_expr_type(ctx, receiver);
                    LLVMClassTypeEntry *recv_cls =
                        !ctx->has_error && recv_ty != NULL
                            ? llvm_stmt_lookup_class_by_type(ctx, recv_ty)
                            : NULL;
                    if (recv_cls != NULL)
                        recv_class = recv_cls->class_name;
                }
                if (recv_class != NULL) {
                    LLVMTypeRef method_ret =
                        llvm_stmt_host_method_return_type(
                            ctx, recv_class, method_name);
                    if (method_ret != NULL)
                        return method_ret;
                }
            }
            if (receiver_name == NULL
                && receiver->type == AST_MEMBER_ACCESS) {
                const char *recv_class =
                    llvm_expr_custom_type_name(receiver, ctx);
                if (recv_class != NULL) {
                    LLVMTypeRef method_ret =
                        llvm_stmt_host_method_return_type(
                            ctx, recv_class, method_name);
                    if (method_ret != NULL)
                        return method_ret;
                }
            }
        }
        if (ast_call_callee(expr) != NULL
            && ast_call_callee(expr)->type == AST_IDENTIFIER
            && ast_identifier_name(ast_call_callee(expr)) != NULL) {
            const char *callee = ast_identifier_name(ast_call_callee(expr));
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
                LLVMEnumVariantEntry *variant =
                    llvm_lookup_enum_variant(ctx, callee);
                if (variant != NULL) {
                    LLVMClassTypeEntry *enum_cls =
                        llvm_lookup_class(ctx, variant->enum_name);
                    if (enum_cls != NULL && enum_cls->struct_type != NULL)
                        return enum_cls->struct_type;
                    if (llvm_enum_type_exists(ctx, variant->enum_name))
                        return ctx->type_i32;
                }
            }
            if (strcmp(callee, "SliceCopy") == 0
                && ast_call_arg_count(expr) == 1) {
                LLVMTypeRef slice_ty = llvm_stmt_infer_expr_type(ctx,
                    ast_call_argument(expr, 0));
                if (slice_ty == ctx->slice_type_Int)
                    return ctx->array_type_Int;
                if (slice_ty == ctx->slice_type_Long)
                    return ctx->array_type_Long;
                if (slice_ty == ctx->slice_type_Float)
                    return ctx->array_type_Float;
                if (slice_ty == ctx->slice_type_Double)
                    return ctx->array_type_Double;
                if (slice_ty == ctx->slice_type_Bool)
                    return ctx->array_type_Bool;
                if (slice_ty == ctx->slice_type_String)
                    return ctx->array_type_String;
                return llvm_stmt_unknown_expr_type(ctx, expr,
                    "SliceCopy requires concrete Slice<T> operand");
            }
            if (llvm_stmt_call_is_slot_builtin(callee)
                && ast_call_arg_count(expr) >= 1
                && ast_call_argument(expr, 0) != NULL
                && ast_call_argument(expr, 0)->type == AST_IDENTIFIER) {
                const char *receiver_name =
                    ast_identifier_name(ast_call_argument(expr, 0));
                const char *inner = llvm_stmt_lookup_slot_or_view_inner(
                    ctx, receiver_name);
                if (inner != NULL && llvm_stmt_slot_call_returns_value(callee))
                    return pergyra_type_to_llvm(ctx, inner);
                if (inner != NULL)
                    return ctx->type_void;
                /* Closure #87d: slot inner not yet registered at type-infer
                 * time (with-slot block inside zone method, MIR registers
                 * the slot later). For Read/Write the return type is i32
                 * for Read, void for Write — match the actual emit path. */
                if (llvm_stmt_slot_call_returns_value(callee))
                    return ctx->type_i32;
                return ctx->type_void;
            }
            {
                LLVMTypeRef builtin_type =
                    llvm_stmt_infer_scalar_builtin_type(ctx, callee);
                if (builtin_type != NULL)
                    return builtin_type;
            }
            /* Closure #87: scalar math (Min/Max/Clamp/Abs) must be
             * resolved before host-method dispatch — otherwise inside a
             * method body that mutates an aggregate field (e.g.
             * battle_simulator: `self.defender.health.current = Max(...)`)
             * the LLVM type-infer tries to dispatch `Max` as
             * `<host>.Max` and emits the strict "missing method return
             * metadata" error. The math helper short-circuits to the
             * argument-promoted numeric type, which is what the actual
             * emit path uses. */
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
                /* Closure #87c: known builtins (slot ops, Log/Print/ToString,
                 * Clone) must not be dispatched as `<host_class>.<builtin>`.
                 * Doing so raised strict "missing method return metadata"
                 * inside zone methods of campaign_graph_fsm where
                 * `Read(commandBudget)` was reinterpreted as
                 * CampaignWorld.Read. The slot builtin's typed return is
                 * handled either by the slot/view-inner shortcut earlier
                 * or by the fallback at the end of this function. */
                LLVMTypeRef method_ret = llvm_stmt_host_method_return_type(
                    ctx, llvm_current_host_class_name(ctx), callee);
                if (method_ret != NULL)
                    return method_ret;
            }
            /* Closure #73: Clone(x) returns x's type (identity pass-through
             * matches llvm_expr_call_dispatch.c:92). Without this fallthrough
             * tightened-mode type inference fails with "concrete type required"
             * even though emit lowers Clone to its argument verbatim. */
            if (strcmp(callee, "Clone") == 0 && ast_call_arg_count(expr) >= 1
                && ast_call_argument(expr, 0) != NULL) {
                LLVMTypeRef arg_type = llvm_stmt_infer_expr_type(ctx,
                    ast_call_argument(expr, 0));
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
            LLVMFuncEntry *fn = llvm_stmt_lookup_visible_function(ctx, callee);
            if (fn != NULL)
                return fn->ret_type;
            {
                LLVMTypeRef declared_type =
                    llvm_stmt_lookup_declared_call_return_type(ctx, callee);
                if (declared_type != NULL)
                    return declared_type;
            }
            if (llvm_find_intent_decl(ctx, callee) != NULL)
                return ctx->type_i1;
            {
                LLVMTypeRef math_type =
                    llvm_stmt_infer_scalar_math_return_type(ctx, expr, callee);
                if (math_type != NULL)
                    return math_type;
            }
            if (llvm_stmt_call_returns_collection_value(callee)
                && ast_call_arg_count(expr) >= 1
                && ast_call_argument(expr, 0) != NULL
                && ast_call_argument(expr, 0)->type == AST_IDENTIFIER) {
                const char *inner = llvm_stmt_lookup_collection_get_inner(
                    ctx, callee,
                    ast_identifier_name(ast_call_argument(expr, 0)));
                if (inner != NULL)
                    return pergyra_type_to_llvm(ctx, inner);
            }
            {
                LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, callee);
                if (cls != NULL && cls->struct_type != NULL)
                    return cls->struct_type;
            }
        }
        /* Domain helper calls can be emitted before their final lowered helper
         * entry is visible in the LLVM function inventory. Prefer the
         * enclosing concrete let/return context when it exists; otherwise fail
         * through the typed inference seam instead of inventing Int. */
        if (ctx->expected_type_name != NULL) {
            LLVMTypeRef expected = pergyra_type_to_llvm(
                ctx, ctx->expected_type_name);
            if (expected != NULL)
                return expected;
        }
        {
            char reason[256];
            const char *callee = NULL;
            ASTNode *callee_node = ast_call_callee(expr);
            if (callee_node != NULL && callee_node->type == AST_IDENTIFIER)
                callee = ast_identifier_name(callee_node);
            if (callee_node != NULL && callee_node->type == AST_MEMBER_ACCESS)
                callee = ast_member_name(callee_node);
            if (!llvm_stmt_type_reasonf(reason, sizeof(reason),
                    "call '%s' requires registered function or expected type metadata",
                    callee != NULL ? callee : "<expr>")) {
                return llvm_stmt_unknown_expr_type(ctx, expr,
                    "call result requires registered function or expected type metadata");
            }
            return llvm_stmt_unknown_expr_type(ctx, expr, reason);
        }
    case AST_BINARY: {
        PgyTokenType op = ast_binary_operator(expr).type;
        LLVMTypeRef left_ty = NULL;
        LLVMTypeRef right_ty = NULL;
        if (op == TOKEN_COALESCE) {
            LLVMTypeRef fields[2];
            left_ty = llvm_stmt_infer_expr_type(ctx, ast_binary_left(expr));
            if (ctx->has_error || left_ty == NULL)
                return NULL;
            if (LLVMGetTypeKind(left_ty) == LLVMStructTypeKind
                && LLVMCountStructElementTypes(left_ty) == 2) {
                LLVMGetStructElementTypes(left_ty, fields);
                return fields[1];
            }
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "coalesce operator requires concrete Option<T> left operand");
        }
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL
            || op == TOKEN_AND || op == TOKEN_OR) {
            return ctx->type_i1;
        }
        if (op == TOKEN_PLUS) {
            left_ty = llvm_stmt_infer_expr_type(ctx, ast_binary_left(expr));
            right_ty = llvm_stmt_infer_expr_type(ctx, ast_binary_right(expr));
            if (left_ty == ctx->type_i8ptr || right_ty == ctx->type_i8ptr)
                return ctx->type_i8ptr;
            return llvm_stmt_promote_numeric_type(ctx, left_ty, right_ty);
        }
        if (op == TOKEN_MINUS || op == TOKEN_STAR || op == TOKEN_SLASH
            || op == TOKEN_PERCENT) {
            left_ty = llvm_stmt_infer_expr_type(ctx, ast_binary_left(expr));
            right_ty = llvm_stmt_infer_expr_type(ctx, ast_binary_right(expr));
            return llvm_stmt_promote_numeric_type(ctx, left_ty, right_ty);
        }
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "unsupported binary operator has no inferred LLVM type");
    }
    case AST_SPAWN_EXPR:
        return ctx->type_task_handle;
    case AST_AWAIT_EXPR:
        return llvm_stmt_infer_await_expr_type(ctx, expr);
    case AST_ASYNC_BLOCK:
        return ctx->type_task_handle;
    case AST_TASK_GROUP:
        return ctx->type_void;
    case AST_SELECT_STMT:
    case AST_CHANNEL_SEND:
    case AST_RETURN:
    case AST_BREAK:
    case AST_CONTINUE:
    case AST_BLOCK:
    case AST_IF_STMT:
    case AST_WHILE_LOOP:
    case AST_FOR_LOOP:
    case AST_PARALLEL_BLOCK:
    case AST_DEFER_STMT:
        return ctx->type_void;
    default:
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "expression requires typed MIR result facts");
    }
}

#endif /* PGY_LLVM_ENABLED */
