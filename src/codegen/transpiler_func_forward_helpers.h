#ifndef PGY_TRANSPILER_FUNC_FORWARD_HELPERS_H
#define PGY_TRANSPILER_FUNC_FORWARD_HELPERS_H

#include "../common/string_compat.h"
#include "../parser/ast_api.h"

static char *
infer_spawn_return_type_name(TranspilerCtx *ctx, ASTNode *spawn_expr)
{
    ASTNode *target = ast_spawn_function(spawn_expr);
    const char *function_name = NULL;
    ASTNode *call = NULL;

    if (target == NULL)
        return pergyra_strdup("Void");

    if (target->type == AST_CALL
        && ast_call_callee(target) != NULL
        && ast_call_callee(target)->type == AST_IDENTIFIER) {
        call = target;
        function_name = ast_identifier_name(ast_call_callee(target));
    } else if (target->type == AST_IDENTIFIER) {
        function_name = ast_identifier_name(target);
    } else if (target->type == AST_FUNC_DECL) {
        if (ast_func_return_type(target) != NULL)
            return render_type_name(ast_func_return_type(target));
        return pergyra_strdup("Void");
    }

    if (function_name == NULL)
        return pergyra_strdup("Void");

    ASTNode *decl = find_function_decl(ctx, function_name);
    if (decl != NULL && ast_func_return_type(decl) != NULL) {
        if (call != NULL && func_has_generic_params(decl)) {
            GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
            size_t binding_count = 0;
            if (infer_generic_call_bindings(ctx, decl, call, bindings, &binding_count))
                return render_type_name_with_bindings(ctx, ast_func_return_type(decl),
                    bindings, binding_count);
        }
        return render_type_name(ast_func_return_type(decl));
    }

    return pergyra_strdup("Void");
}

static bool
is_remote_future_expr(TranspilerCtx *ctx, ASTNode *expr)
{
    if (expr == NULL) return false;
    if (expr->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, ast_identifier_name(expr));
        return type_name != NULL && strncmp(type_name, "RemoteFuture<", 13) == 0;
    }
    return false;
}

static bool
lookup_future_inner_type_copy(TranspilerCtx *ctx, ASTNode *expr,
                              char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (expr == NULL)
        return pergyra_str_copy(out, out_size, "Void");

    if (expr->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx,
            ast_identifier_name(expr));
        if (type_name != NULL
            && (strncmp(type_name, "Future<", 7) == 0
                || strncmp(type_name, "RemoteFuture<", 13) == 0)) {
            return slot_inner_type_name_copy(type_name, out, out_size);
        }
    }

    if (expr != NULL && expr->type == AST_SPAWN_EXPR) {
        char *owned = infer_spawn_return_type_name(ctx, expr);
        bool ok = owned != NULL && pergyra_str_copy(out, out_size, owned);
        free(owned);
        return ok;
    }

    return pergyra_str_copy(out, out_size, "Void");
}

bool
pergyra_ast_type_to_c_copy(ASTNode *type_node, char *out, size_t out_size)
{
    char *type_name;
    bool ok;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (type_node == NULL)
        return pergyra_str_copy(out, out_size, "void");

    if (type_node->type == AST_EVENT_HANDLER_TYPE)
        return pergyra_str_copy(out, out_size, "void *");

    type_name = render_type_name(type_node);
    ok = pergyra_type_to_c_copy(type_name, out, out_size);
    free(type_name);
    if (!ok)
        out[0] = '\0';
    return ok;
}

static int
find_generic_param_index(ASTNode *decl, const char *name)
{
    if (!func_has_generic_params(decl) || name == NULL)
        return -1;

    GenericParams *generic_params = ast_func_generic_params(decl);
    size_t generic_count = ast_generic_param_count(generic_params);
    for (size_t i = 0; i < generic_count; i++) {
        GenericParam *param = ast_generic_param_at(generic_params, i);
        if (ast_generic_param_name(param) != NULL
            && strcmp(ast_generic_param_name(param), name) == 0)
            return (int)i;
    }

    return -1;
}

static bool

infer_generic_call_bindings(TranspilerCtx *ctx, ASTNode *decl, ASTNode *call,
                            GenericBindingEntry *bindings, size_t *binding_count)
{
    if (!func_has_generic_params(decl)
        || call == NULL
        || call->type != AST_CALL
        || bindings == NULL
        || binding_count == NULL) {
        return false;
    }

    GenericParams *generic_params = ast_func_generic_params(decl);
    size_t generic_count = ast_generic_param_count(generic_params);
    memset(bindings, 0, sizeof(GenericBindingEntry) * generic_count);

    for (size_t i = 0; i < generic_count; i++) {
        GenericParam *param = ast_generic_param_at(generic_params, i);
        if (ast_generic_param_name(param) != NULL) {
            pergyra_str_copy(bindings[i].name,
                sizeof(bindings[i].name), ast_generic_param_name(param));
        }
    }

    for (size_t i = 0; i < ast_func_param_count(decl)
        && i < ast_call_arg_count(call); i++) {
        FuncParam *param = ast_func_param(decl, i);
        if (param == NULL || param->type == NULL || param->type->type != AST_TYPE)
            continue;
        if (ast_type_generic_args(param->type) != NULL)
            continue;

        int generic_index = find_generic_param_index(decl, ast_type_name(param->type));
        if (generic_index < 0)
            continue;

        const char *arg_type = infer_expression_type_name(ctx,
            ast_call_argument(call, i));
        if (arg_type == NULL)
            continue;

        if (bindings[generic_index].concrete_type[0] != '\0'
            && strcmp(bindings[generic_index].concrete_type, arg_type) != 0) {
            return false;
        }

        pergyra_str_copy(bindings[generic_index].concrete_type,
            sizeof(bindings[generic_index].concrete_type), arg_type);
    }

    for (size_t i = 0; i < generic_count; i++) {
        if (bindings[i].name[0] == '\0' || bindings[i].concrete_type[0] == '\0')
            return false;
    }

    *binding_count = generic_count;
    return true;
}

void
emit_func_forward_decl_named(ASTNode *node, const char *emitted_name,
                             CodeBuf *buf, TranspilerCtx *ctx);

static void
emit_func_decl_named(ASTNode *node, const char *emitted_name,
                     CodeBuf *buf, TranspilerCtx *ctx);
#endif /* PGY_TRANSPILER_FUNC_FORWARD_HELPERS_H */
