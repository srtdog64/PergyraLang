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
    return node != NULL
        && node->type == AST_FUNC_DECL
        && ast_func_generic_params(node) != NULL
        && ast_func_generic_params(node)->count > 0;
}

bool
transpiler_class_has_generic_params(ASTNode *node)
{
    return node != NULL
        && node->type == AST_CLASS_DECL
        && ast_class_generic_params(node) != NULL
        && ast_generic_param_count(ast_class_generic_params(node)) > 0;
}

char *
transpiler_generic_param_effective_arg_name(GenericParam *formal,
                                            GenericParam *arg)
{
    ASTNode *arg_constraint;
    ASTNode *formal_default;

    arg_constraint = ast_generic_param_constraint(arg);
    formal_default = ast_generic_param_default_type(formal);
    if (arg_constraint != NULL && arg_constraint->type == AST_TYPE)
        return render_type_name(arg_constraint);
    if (ast_generic_param_name(arg) != NULL)
        return pergyra_strdup(ast_generic_param_name(arg));
    if (formal_default != NULL && formal_default->type == AST_TYPE)
        return render_type_name(formal_default);
    return NULL;
}
