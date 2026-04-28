#ifndef PERGYRA_AST_DESTROY_INTERNAL_H
#define PERGYRA_AST_DESTROY_INTERNAL_H

#include "ast.h"
#include <stdbool.h>

void ast_destroy_generic_params(GenericParams* params);
void ast_destroy_where_clause(WhereClause* clause);
bool ast_destroy_domain_node(ASTNode* node);

#endif
