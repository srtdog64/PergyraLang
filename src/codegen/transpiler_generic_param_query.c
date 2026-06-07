/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared C backend generic-parameter declaration queries.
 */

#include "transpiler_generic_param_query.h"

#include "../parser/ast_api.h"
#include "../common/string_compat.h"

#include "transpiler_type_render.h"

bool
transpiler_func_has_generic_params(ASTNode *node)
{
    GenericParams *generic_params;

    if (node == NULL || node->type != AST_FUNC_DECL)
        return false;
    generic_params = ast_declaration_generic_params(node);
    return ast_generic_param_count(generic_params) > 0;
}

bool
transpiler_class_has_generic_params(ASTNode *node)
{
    GenericParams *generic_params;

    if (node == NULL || node->type != AST_CLASS_DECL)
        return false;
    generic_params = ast_declaration_generic_params(node);
    return ast_generic_param_count(generic_params) > 0;
}

char *
transpiler_generic_param_effective_arg_name_in_ctx(TranspilerCtx *ctx,
                                                   GenericParam *formal,
                                                   GenericParam *arg)
{
    ASTNode *arg_constraint;
    ASTNode *formal_default;

    arg_constraint = ast_generic_param_constraint(arg);
    formal_default = ast_generic_param_default_type(formal);
    if (arg_constraint != NULL && arg_constraint->type == AST_TYPE)
        return render_type_name_in_ctx(ctx, arg_constraint);
    if (ast_generic_param_name(arg) != NULL)
        return pergyra_strdup(ast_generic_param_name(arg));
    if (formal_default != NULL && formal_default->type == AST_TYPE)
        return render_type_name_in_ctx(ctx, formal_default);
    return NULL;
}

char *
transpiler_generic_param_effective_arg_name(GenericParam *formal,
                                            GenericParam *arg)
{
    return transpiler_generic_param_effective_arg_name_in_ctx(
        NULL, formal, arg);
}
