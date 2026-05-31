#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_stmt_type_infer_helpers.h"
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

static LLVMTypeRef
llvm_stmt_expected_array_elem_type(LLVMGenCtx *ctx)
{
    const char *inner;
    char inner_buf[256];

    if (ctx == NULL || ctx->expected_type_name == NULL)
        return NULL;
    switch (pgy_classify_type(ctx->expected_type_name)) {
    case PGY_TK_ARRAY:
    case PGY_TK_SLICE:
        break;
    default:
        return NULL;
    }
    if (!llvm_constructed_arg_name_copy(ctx->expected_type_name, 0,
            inner_buf, sizeof(inner_buf))) {
        return NULL;
    }
    inner = inner_buf;
    if (strcmp(inner, "Unknown") == 0)
        return NULL;
    return pergyra_type_to_llvm(ctx, inner_buf);
}

LLVMClassTypeEntry *
llvm_stmt_lookup_class_by_type(LLVMGenCtx *ctx, LLVMTypeRef type)
{
    return llvm_lookup_class_by_struct_type(ctx, type);
}

static LLVMTypeRef
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
    case AST_IDENTIFIER: {
        const char *name = ast_identifier_name(expr);
        LLVMVarEntry *var = llvm_scope_lookup(ctx, name);
        if (var != NULL)
            return var->type;
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
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "identifier requires registered LLVM local metadata");
    }
    case AST_ASSIGNMENT:
        if (ast_assignment_target(expr) != NULL
            && ast_assignment_target(expr)->type == AST_IDENTIFIER) {
            LLVMVarEntry *var = llvm_scope_lookup(ctx,
                ast_identifier_name(ast_assignment_target(expr)));
            if (var != NULL)
                return var->type;
        }
        if (ast_assignment_value(expr) != NULL)
            return llvm_stmt_infer_expr_type(ctx, ast_assignment_value(expr));
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "assignment is missing a value expression");
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
            if (name != NULL && llvm_scope_lookup(ctx, name) != NULL) {
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
            if (strcmp(method_name, "Slice") == 0 && receiver_name != NULL) {
                LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, receiver_name);
                if (entry != NULL) {
                    const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
                    if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
                        return llvm_slice_struct_type(ctx, suffix);
                }
            }
            if (receiver_name != NULL) {
                const char *class_name = llvm_lookup_var_class(ctx, receiver_name);
                if (class_name != NULL) {
                    ASTNode *method_decl = llvm_find_nominal_host_method_decl(
                        ctx, class_name, method_name);
                    if (method_decl != NULL
                        && method_decl->type == AST_FUNC_DECL) {
                        ASTNode *ret_ty = ast_func_return_type(method_decl);
                        if (ret_ty != NULL) {
                            LLVMTypeRef llvm_ret = ast_type_to_llvm(ctx, ret_ty);
                            if (llvm_ret != NULL && !ctx->has_error)
                                return llvm_ret;
                            ctx->has_error = false;
                        }
                    }
                }
            }
            if (strcmp(method_name, "Slice") == 0
                && receiver->type == AST_CALL
                && ast_call_callee(receiver) != NULL
                && ast_call_callee(receiver)->type == AST_IDENTIFIER
                && ast_identifier_name(ast_call_callee(receiver)) != NULL) {
                ASTNode *decl = llvm_stmt_find_function_decl_by_name(
                    ctx, ast_identifier_name(ast_call_callee(receiver)));
                ASTNode *return_type = ast_func_return_type(decl);
                const char *return_type_name = ast_type_name(return_type);
                GenericParams *return_generic_args =
                    ast_type_generic_args(return_type);
                if (return_type != NULL
                    && return_type->type == AST_TYPE
                    && return_type_name != NULL
                    && (strcmp(return_type_name, "Array") == 0
                        || strcmp(return_type_name, "Slice") == 0)
                    && ast_generic_param_at(return_generic_args, 0) != NULL) {
                    char *elem_name = llvm_stmt_render_type_arg_scratch(
                        ast_generic_param_at(return_generic_args, 0),
                        &ctx->scratch);
                    if (elem_name == NULL) {
                        return llvm_stmt_unknown_expr_type(ctx, expr,
                            "Slice() receiver return type is missing its element type");
                    }
                    LLVMTypeRef slice_ty = llvm_slice_struct_type(ctx, elem_name);
                    return slice_ty;
                }
            }
        }
        if (ast_call_callee(expr) != NULL
            && ast_call_callee(expr)->type == AST_IDENTIFIER
            && ast_identifier_name(ast_call_callee(expr)) != NULL) {
            const char *callee = ast_identifier_name(ast_call_callee(expr));
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
            {
                LLVMTypeRef builtin_type =
                    llvm_stmt_infer_scalar_builtin_type(ctx, callee);
                if (builtin_type != NULL)
                    return builtin_type;
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
            if (llvm_stmt_call_returns_collection_size(callee))
                return ctx->type_i32;
            if (llvm_stmt_call_returns_collection_bool(callee))
                return ctx->type_i1;
            if (llvm_stmt_call_returns_domain_bool(callee))
                return ctx->type_i1;
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
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "call result requires registered function or expected type metadata");
    case AST_BINARY: {
        PgyTokenType op = ast_binary_operator(expr).type;
        LLVMTypeRef left_ty = NULL;
        LLVMTypeRef right_ty = NULL;
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

LLVMTypeRef
llvm_stmt_resolve_array_elem_type(LLVMGenCtx *ctx, ASTNode *expr,
                                  LLVMValueRef data_ptr)
{
    LLVMTypeRef elem_type = llvm_stmt_expected_array_elem_type(ctx);
    (void)data_ptr;

    if (expr == NULL)
        return elem_type != NULL ? elem_type
            : llvm_stmt_unknown_expr_type(ctx, expr,
                "array element type requires expected Array<T> metadata");

    if (expr->type == AST_IDENTIFIER && ast_identifier_name(expr) != NULL) {
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(
            ctx, ast_identifier_name(expr));
        if (entry != NULL && entry->elem_type != NULL)
            return entry->elem_type;
    }

    if (expr->type == AST_ARRAY_LITERAL
        && ast_array_literal_count(expr) > 0
        && ast_array_literal_element(expr, 0) != NULL) {
        LLVMTypeRef inferred = llvm_stmt_infer_expr_type(
            ctx, ast_array_literal_element(expr, 0));
        if (inferred != NULL)
            return inferred;
    }

    if (expr->type == AST_CALL
        && ast_call_callee(expr) != NULL
        && ast_call_callee(expr)->type == AST_MEMBER_ACCESS
        && ast_member_name(ast_call_callee(expr)) != NULL
        && strcmp(ast_member_name(ast_call_callee(expr)), "Slice") == 0
        && ast_member_object(ast_call_callee(expr)) != NULL) {
        ASTNode *receiver = ast_member_object(ast_call_callee(expr));
        if (receiver->type == AST_IDENTIFIER && ast_identifier_name(receiver) != NULL) {
            LLVMArrayVarEntry *entry = llvm_lookup_array_var(
                ctx, ast_identifier_name(receiver));
            if (entry != NULL && entry->elem_type != NULL)
                return entry->elem_type;
        }
        if (receiver->type == AST_CALL
            && ast_call_callee(receiver) != NULL
            && ast_call_callee(receiver)->type == AST_IDENTIFIER
            && ast_identifier_name(ast_call_callee(receiver)) != NULL) {
            ASTNode *decl = llvm_stmt_find_function_decl_by_name(
                ctx, ast_identifier_name(ast_call_callee(receiver)));
            ASTNode *ret = ast_func_return_type(decl);
            if (ret != NULL && ret->type == AST_TYPE) {
                const char *ret_name = ast_type_name(ret);
                GenericParams *generic_args = ast_type_generic_args(ret);
                if (ret_name != NULL
                    && (strcmp(ret_name, "Array") == 0
                        || strcmp(ret_name, "Slice") == 0)
                    && ast_generic_param_at(generic_args, 0) != NULL) {
                    char *elem_name = llvm_stmt_render_type_arg_scratch(
                        ast_generic_param_at(generic_args, 0),
                        &ctx->scratch);
                    if (elem_name == NULL || elem_name[0] == '\0')
                        return llvm_stmt_unknown_expr_type(ctx, receiver,
                            "array return element type is unresolved");
                    LLVMTypeRef declared = pergyra_type_to_llvm(ctx, elem_name);
                    if (declared != NULL)
                        return declared;
                }
            }
        }
        return llvm_stmt_resolve_array_elem_type(
            ctx, receiver, NULL);
    }

    if (expr->type == AST_CALL
        && ast_call_callee(expr) != NULL
        && ast_call_callee(expr)->type == AST_IDENTIFIER
        && ast_identifier_name(ast_call_callee(expr)) != NULL) {
        ASTNode *decl = llvm_stmt_find_function_decl_by_name(
            ctx, ast_identifier_name(ast_call_callee(expr)));
        ASTNode *ret = ast_func_return_type(decl);
        if (ret != NULL && ret->type == AST_TYPE) {
            const char *ret_name = ast_type_name(ret);
            GenericParams *generic_args = ast_type_generic_args(ret);
            if (ret_name != NULL
                && (strcmp(ret_name, "Array") == 0
                    || strcmp(ret_name, "Slice") == 0)
                && ast_generic_param_at(generic_args, 0) != NULL) {
                GenericParam *gp = ast_generic_param_at(generic_args, 0);
                /* Prefer the simple type name ("String", "Int"); fall back
                 * to the explicit constraint node if given. */
                if (ast_generic_param_name(gp) != NULL) {
                    LLVMTypeRef declared = pergyra_type_to_llvm(
                        ctx, ast_generic_param_name(gp));
                    if (declared != NULL)
                        return declared;
                }
                if (ast_generic_param_constraint(gp) != NULL) {
                    LLVMTypeRef declared = ast_type_to_llvm(
                        ctx, ast_generic_param_constraint(gp));
                    if (declared != NULL)
                        return declared;
                }
            }
        }
    }

    if (elem_type != NULL)
        return elem_type;
    return llvm_stmt_unknown_expr_type(ctx, expr,
        "array or slice element type requires registered Array<T>/Slice<T> metadata");
}


#endif /* PGY_LLVM_ENABLED */
