/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST generic and where-clause print helpers.
 */

#include "ast_print_internal.h"
#include <stdio.h>
void
print_generic_params_inline(GenericParams* params)
{
    if (params == NULL || params->count == 0) {
        return;
    }

    printf("<");
    for (size_t i = 0; i < params->count; i++) {
        GenericParam* param = params->params[i];
        if (i > 0)
            printf(", ");
        if (param == NULL) {
            printf("?");
            continue;
        }
        printf("%s", param->name != NULL ? param->name : "?");
        if (param->constraint != NULL) {
            printf(": ");
            ast_print_inline(param->constraint);
        }
        if (param->default_type != NULL) {
            printf(" = ");
            ast_print_inline(param->default_type);
        }
    }
    printf(">");
}

void
print_where_clause_inline(WhereClause* clause)
{
    if (clause == NULL || clause->count == 0)
        return;

    printf(" where ");
    for (size_t i = 0; i < clause->count; i++) {
        TypeConstraint* constraint = clause->constraints[i];
        if (i > 0)
            printf(", ");
        if (constraint == NULL) {
            printf("?");
            continue;
        }

        printf("%s", constraint->type_param != NULL ? constraint->type_param : "?");
        if (constraint->bound_count > 0) {
            printf(": ");
            for (size_t j = 0; j < constraint->bound_count; j++) {
                if (j > 0)
                    printf(" + ");
                ast_print_inline(constraint->bounds[j]);
            }
        }
    }
}
