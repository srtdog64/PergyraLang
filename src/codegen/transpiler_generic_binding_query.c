/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared C backend generic call binding queries.
 */

#include "transpiler_generic_binding_query.h"

#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "transpiler_expr_stdlib_collection_support.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_type_render.h"

static int
transpiler_find_generic_param_index(ASTNode *decl, const char *name)
{
    GenericParams *generic_params;
    size_t generic_count;

    if (!transpiler_func_has_generic_params(decl) || name == NULL)
        return -1;

    generic_params = ast_func_generic_params(decl);
    generic_count = ast_generic_param_count(generic_params);
    for (size_t i = 0; i < generic_count; i++) {
        GenericParam *param = ast_generic_param_at(generic_params, i);
        if (ast_generic_param_name(param) != NULL
            && strcmp(ast_generic_param_name(param), name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

bool
transpiler_infer_generic_call_bindings(TranspilerCtx *ctx,
                                       ASTNode *decl,
                                       ASTNode *call,
                                       GenericBindingEntry *bindings,
                                       size_t *binding_count)
{
    GenericParams *generic_params;
    size_t generic_count;

    if (!transpiler_func_has_generic_params(decl)
        || call == NULL
        || call->type != AST_CALL
        || bindings == NULL
        || binding_count == NULL) {
        return false;
    }

    generic_params = ast_func_generic_params(decl);
    generic_count = ast_generic_param_count(generic_params);
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
        int generic_index;
        const char *arg_type;

        if (param == NULL || param->type == NULL
            || param->type->type != AST_TYPE) {
            continue;
        }
        if (ast_type_generic_args(param->type) != NULL)
            continue;

        generic_index = transpiler_find_generic_param_index(decl,
            ast_type_name(param->type));
        if (generic_index < 0)
            continue;

        arg_type = transpiler_expr_infer_type_name(ctx,
            ast_call_argument(call, i));
        if (arg_type == NULL)
            continue;

        if (bindings[generic_index].concrete_type[0] != '\0'
            && strcmp(bindings[generic_index].concrete_type,
                arg_type) != 0) {
            return false;
        }

        pergyra_str_copy(bindings[generic_index].concrete_type,
            sizeof(bindings[generic_index].concrete_type), arg_type);
    }

    for (size_t i = 0; i < generic_count; i++) {
        if (bindings[i].name[0] == '\0'
            || bindings[i].concrete_type[0] == '\0') {
            return false;
        }
    }

    *binding_count = generic_count;
    return true;
}

TranspilerGenericBindingSnapshot
transpiler_generic_binding_snapshot(TranspilerCtx *ctx)
{
    TranspilerGenericBindingSnapshot snapshot;

    snapshot.binding_count = ctx != NULL ? ctx->generic_binding_count : 0;
    return snapshot;
}

void
transpiler_generic_binding_restore(
    TranspilerCtx *ctx,
    TranspilerGenericBindingSnapshot snapshot)
{
    if (ctx == NULL)
        return;
    ctx->generic_binding_count = snapshot.binding_count;
}

char *
transpiler_render_type_name_with_bindings(TranspilerCtx *ctx,
                                          ASTNode *type_node,
                                          GenericBindingEntry *bindings,
                                          size_t binding_count)
{
    TranspilerGenericBindingSnapshot snapshot;
    char *result;

    if (ctx == NULL)
        return NULL;

    snapshot = transpiler_generic_binding_snapshot(ctx);
    for (size_t i = 0;
        i < binding_count && ctx->generic_binding_count < MAX_GENERIC_BINDINGS;
        i++) {
        ctx->generic_bindings[ctx->generic_binding_count++] = bindings[i];
    }

    result = render_type_name_in_ctx(ctx, type_node);
    transpiler_generic_binding_restore(ctx, snapshot);
    return result;
}
