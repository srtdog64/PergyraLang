#ifndef PGY_TRANSPILER_EXPR_CORE_BUILTINS_EMIT_H
#define PGY_TRANSPILER_EXPR_CORE_BUILTINS_EMIT_H

#include "../parser/ast_api.h"

char *emit_builtin_allocator(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx);

static const char *
lookup_wrapped_inner_type(TranspilerCtx *ctx, ASTNode *arg, const char *wrapper,
                          char *inner_buf, size_t inner_buf_size)
{
    if (arg != NULL && arg->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, ast_identifier_name(arg));
        size_t wrapper_len = strlen(wrapper);
        if (type_name != NULL && strncmp(type_name, wrapper, wrapper_len) == 0
            && type_name[wrapper_len] == '<') {
            if (slot_inner_type_name_copy(type_name, inner_buf,
                    inner_buf_size)) {
                return inner_buf;
            }
            return NULL;
        }
    }
    return NULL;
}

static const char *
expected_wrapped_inner_type(TranspilerCtx *ctx, const char *wrapper,
                            char *inner_buf, size_t inner_buf_size)
{
    size_t wrapper_len;

    if (ctx == NULL || ctx->expected_type == NULL || wrapper == NULL)
        return NULL;
    wrapper_len = strlen(wrapper);
    if (strncmp(ctx->expected_type, wrapper, wrapper_len) == 0
        && ctx->expected_type[wrapper_len] == '<') {
        if (slot_inner_type_name_copy(ctx->expected_type, inner_buf,
                inner_buf_size)) {
            return inner_buf;
        }
    }
    return NULL;
}

char *
emit_builtin_rc(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    const char *inner = NULL;
    char inner_buf[128];
    ASTNode *arg = ast_call_arg_count(call) > 0 ? ast_call_argument(call, 0) : NULL;

    switch (kind) {
    case BUILTIN_RC_NEW:
        if (ast_call_arg_count(call) != 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: RcNew requires exactly one argument");
            return pergyra_strdup("0");
        }
        {
            const char *expected_inner =
                expected_wrapped_inner_type(ctx, "Rc", inner_buf,
                    sizeof(inner_buf));
            const char *arg_type = NULL;
            if (expected_inner != NULL)
                inner = expected_inner;
            else if (arg != NULL)
                arg_type = infer_expression_type_name(ctx, arg);
            if (expected_inner == NULL && arg_type != NULL
                && strcmp(arg_type, "Unknown") != 0)
                inner = arg_type;
            if (inner == NULL || inner[0] == '\0') {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: RcNew requires concrete Rc<T> metadata or a typed initializer");
                return pergyra_strdup("0");
            }
        }
        break;
    case BUILTIN_RC_CLONE:
    case BUILTIN_RC_DROP:
    case BUILTIN_RC_GET:
    case BUILTIN_RC_DOWNGRADE:
        inner = lookup_wrapped_inner_type(ctx, arg, "Rc", inner_buf,
            sizeof(inner_buf));
        if (inner == NULL || inner[0] == '\0') {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: Rc operation requires concrete Rc<T> metadata");
            return pergyra_strdup("0");
        }
        break;
    case BUILTIN_WEAK_UPGRADE:
    case BUILTIN_WEAK_DROP:
        inner = lookup_wrapped_inner_type(ctx, arg, "Weak", inner_buf,
            sizeof(inner_buf));
        if (inner == NULL || inner[0] == '\0') {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: Weak operation requires concrete Weak<T> metadata");
            return pergyra_strdup("0");
        }
        break;
    default:
        break;
    }

    if (kind == BUILTIN_RC_NEW) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_new_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_CLONE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_clone_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_DROP) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_drop_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_GET) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("(*pgy_rc_get_%s(&%s))", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_DOWNGRADE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_downgrade_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_WEAK_UPGRADE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_weak_upgrade_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_WEAK_DROP) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_weak_drop_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported Rc builtin kind %d", (int)kind);
    return pergyra_strdup("0");
}

char *
emit_builtin_box(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    const char *inner = NULL;
    char inner_buf[128];
    ASTNode *arg = ast_call_arg_count(call) > 0 ? ast_call_argument(call, 0) : NULL;

    switch (kind) {
    case BUILTIN_BOX:
        if (ast_call_arg_count(call) != 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: Box requires exactly one argument");
            return pergyra_strdup("0");
        }
        if (arg->type == AST_NUMBER) inner = "Int";
        else if (arg->type == AST_STRING) inner = "String";
        else if (arg->type == AST_BOOLEAN) inner = "Bool";
        else if (arg->type == AST_CALL
                 && ast_call_callee(arg) != NULL
                 && ast_call_callee(arg)->type == AST_IDENTIFIER) {
            const char *callee_name = ast_identifier_name(ast_call_callee(arg));
            ASTNode *class_decl = find_class_decl(ctx, callee_name);
            if (class_decl != NULL && class_decl->type == AST_CLASS_DECL)
                inner = callee_name;
            else {
                const char *arg_type = infer_expression_type_name(ctx, arg);
                if (arg_type != NULL && strcmp(arg_type, "Unknown") != 0)
                    inner = arg_type;
            }
        }
        else if (arg->type == AST_IDENTIFIER) {
            const char *arg_type = lookup_typed_var(ctx, ast_identifier_name(arg));
            if (arg_type != NULL && strcmp(arg_type, "Unknown") != 0)
                inner = arg_type;
        } else {
            const char *arg_type = infer_expression_type_name(ctx, arg);
            if (arg_type != NULL && strcmp(arg_type, "Unknown") != 0)
                inner = arg_type;
        }
        if (inner == NULL || inner[0] == '\0') {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: Box requires concrete Box<T> metadata or a typed initializer");
            return pergyra_strdup("0");
        }
        break;
    case BUILTIN_BOX_GET:
    case BUILTIN_BOX_SET:
    case BUILTIN_BOX_DROP:
    case BUILTIN_BOX_IS_VALID:
        inner = lookup_wrapped_inner_type(ctx, arg, "Box", inner_buf,
            sizeof(inner_buf));
        if (inner == NULL || inner[0] == '\0') {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: Box operation requires concrete Box<T> metadata");
            return pergyra_strdup("0");
        }
        break;
    case BUILTIN_BOX_ARRAY:
        if (ast_call_arg_count(call) < 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: BoxArray requires an array argument");
            return pergyra_strdup("0");
        }
        {
            const char *arr_type =
                infer_expression_type_name(ctx, ast_call_argument(call, 0));
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = arr_type;
            if (inner == NULL || inner[0] == '\0') {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: BoxArray requires concrete Array<T> metadata");
                return pergyra_strdup("0");
            }
        }
        break;
    default:
        break;
    }

    if (kind == BUILTIN_BOX) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_new_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_GET) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_get_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_SET) {
        char *box_expr = emit_expression(arg, ctx);
        char *value = emit_expression(ast_call_argument(call, 1), ctx);
        char *result = strdup_fmt("pgy_box_set_%s(&%s, %s)", inner, box_expr, value);
        free(box_expr);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_DROP) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_drop_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_IS_VALID) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_is_valid_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_ARRAY) {
        if (ast_call_arg_count(call) < 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: BoxArray requires an array argument");
            return pergyra_strdup("0");
        }
        char *arr_expr = emit_expression(ast_call_argument(call, 0), ctx);
        char elem_buf[128];
        const char *elem = NULL;
        if (slot_inner_type_name_copy(inner, elem_buf, sizeof(elem_buf)))
            elem = elem_buf;
        if (elem == NULL || elem[0] == '\0') {
            free(arr_expr);
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: BoxArray requires concrete Array<T> metadata");
            return pergyra_strdup("0");
        }
        char *result = strdup_fmt("pgy_box_new_Array_%s(%s)", elem, arr_expr);
        free(arr_expr);
        return result;
    }

    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported Box builtin kind %d", (int)kind);
    return pergyra_strdup("0");
}

#endif /* PGY_TRANSPILER_EXPR_CORE_BUILTINS_EMIT_H */
