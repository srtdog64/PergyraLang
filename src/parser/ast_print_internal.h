#ifndef PERGYRA_AST_PRINT_INTERNAL_H
#define PERGYRA_AST_PRINT_INTERNAL_H

#include "ast.h"

#include <stdbool.h>

void ast_print_indent(int level);
void ast_print_inline(ASTNode *node);
const char *ast_print_operator_to_string(PgyTokenType type);
bool ast_print_needs_trailing_newline(ASTNodeType type);
void print_generic_params_inline(GenericParams *params);
void print_where_clause_inline(WhereClause *clause);
void print_intent_step_contract_sources(const ASTNode *node, int indent);
bool ast_print_domain_node(ASTNode *node, int indent);
bool ast_print_event_node(ASTNode *node, int indent);
bool ast_print_expr_node(ASTNode *node, int indent);
bool ast_print_intent_node(ASTNode *node, int indent);
bool ast_print_world_node(ASTNode *node, int indent);
bool ast_print_zone_node(ASTNode *node, int indent);

#endif
