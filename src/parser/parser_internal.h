/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Parser internal declarations — shared across parser_*.c files.
 * NOT part of the public API. Include parser.h first.
 */

#ifndef PERGYRA_PARSER_INTERNAL_H
#define PERGYRA_PARSER_INTERNAL_H

#include "parser.h"
#include "ast.h"
#include "../common/string_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

/* --- Generic / type utilities --- */
GenericParams  *parse_generic_params(Parser *parser);
WhereClause    *parse_where_clause(Parser *parser);
ASTNode        *parse_type(Parser *parser);
ASTNode        *parse_type_constraint(Parser *parser);
void            skip_generic_arguments(Parser *parser);
Token           consume_name_token(Parser *parser, const char *message);

/* --- Expressions (parser_expr.c) --- */
ASTNode *parse_logical_or(Parser *parser);
ASTNode *parse_logical_and(Parser *parser);
ASTNode *parse_equality(Parser *parser);
ASTNode *parse_comparison(Parser *parser);
ASTNode *parse_addition(Parser *parser);
ASTNode *parse_multiplication(Parser *parser);
ASTNode *parse_unary(Parser *parser);
ASTNode *finish_call(Parser *parser, ASTNode *callee);
ASTNode *parse_lambda_expression(Parser *parser);

/* --- Statements (parser_stmt.c) --- */
ASTNode *parse_if_statement(Parser *parser);
ASTNode *parse_while_statement(Parser *parser);
ASTNode *parse_for_loop(Parser *parser);
ASTNode *parse_match_statement(Parser *parser);
ASTNode *parse_return_statement(Parser *parser);
ASTNode *parse_unsafe_block(Parser *parser);
ASTNode *parse_defer_statement(Parser *parser);

/* --- Declarations (parser_decl.c) --- */
ASTNode *parse_function_declaration(Parser *parser);
ASTNode *parse_class_declaration(Parser *parser);
ASTNode *parse_struct_declaration(Parser *parser);
ASTNode *parse_type_declaration(Parser *parser, bool is_struct);
ASTNode *parse_extern_block(Parser *parser);

/* --- Domain types (parser_domain.c) --- */
ASTNode *parse_ability_declaration(Parser *parser);
ASTNode *parse_role_declaration(Parser *parser);
ASTNode *parse_party_declaration(Parser *parser);
ASTNode *parse_systemic_declaration(Parser *parser);
ASTNode *parse_world_declaration(Parser *parser);
ASTNode *parse_event_declaration(Parser *parser);

#endif /* PERGYRA_PARSER_INTERNAL_H */
