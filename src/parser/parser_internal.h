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
GenericParams  *parse_type_arguments(Parser *parser);
WhereClause    *parse_where_clause(Parser *parser);
ASTNode        *parse_type(Parser *parser);
ASTNode        *parse_type_constraint(Parser *parser);
void            skip_generic_arguments(Parser *parser);
Token           parser_peek_next(Parser *parser);
bool            parser_check_name_token(Parser *parser);
bool            parser_match_name_token(Parser *parser);
Token           consume_name_token(Parser *parser, const char *message);
bool            parser_check_decl_name_token(Parser *parser);
Token           consume_decl_name_token(Parser *parser, const char *message);
bool            parser_check_binding_name_token(Parser *parser);
Token           consume_binding_name_token(Parser *parser, const char *message);
bool            parser_append_destructure_name(Parser *parser, ASTNode *node,
                                               const char *name);
bool            parser_check_expr_name_token(Parser *parser);
bool            parser_match_expr_name_token(Parser *parser);
Token           consume_member_name_token(Parser *parser, const char *message);
void            parser_collect_doc_comments(Parser *parser);
void            parser_discard_pending_doc_comment(Parser *parser);
StructuredComment *parser_take_pending_doc_comment(Parser *parser);
bool            parser_attach_pending_doc_comment(Parser *parser,
                                                  ASTNode *node);
ASTNode        *parser_finalize_statement(Parser *parser, ASTNode *node);
void            parser_reject_reserved_cast_after_expression(Parser *parser);
void            parser_register_decl_hint(Parser *parser, ASTNode *node);
bool            parser_lookup_decl_hint(Parser *parser, const char *name,
                                        ASTNodeType *node_type_out,
                                        NominalDeclKind *nominal_kind_out);

/* --- Expressions (parser_expr.c) --- */
ASTNode *parse_logical_or(Parser *parser);
ASTNode *parse_logical_and(Parser *parser);
ASTNode *parse_equality(Parser *parser);
ASTNode *parse_comparison(Parser *parser);
ASTNode *parse_addition(Parser *parser);
ASTNode *parse_multiplication(Parser *parser);
ASTNode *parse_unary(Parser *parser);
ASTNode *finish_call(Parser *parser, ASTNode *callee);
bool     parser_prepend_call_argument(Parser *parser, ASTNode *call,
                                      ASTNode *argument);
bool     parser_append_call_argument(Parser *parser, ASTNode *call,
                                     const char *arg_name, ASTNode *arg);
bool     parser_is_lambda_start(Parser *parser);
ASTNode *parse_lambda_expression(Parser *parser);
bool     parser_append_expr_node_with_capacity(Parser *parser,
                                               ASTNode ***items,
                                               size_t *count,
                                               size_t *capacity,
                                               ASTNode *item);
bool     is_multiline_string_token(const char *value);
ASTNode *parse_interpolation_body(const char *raw, bool is_fstring);

/* --- Parser local surface helpers (parser_pin.c / parser_zone_context.c) --- */
bool     parser_is_exportable_decl(ASTNode *node);
ASTNode *parser_parse_export_declaration(Parser *parser);
ASTNode *parser_parse_enum_declaration_after_keyword(Parser *parser);
bool     parser_starts_named_declaration(Parser *parser,
                                         PgyTokenType keyword);
bool     parser_check_pin_block_start(Parser *parser);
ASTNode *parser_parse_pin_block(Parser *parser);
bool     parser_check_within_context_block_start(Parser *parser);
ASTNode *parser_parse_within_context_block(Parser *parser);

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
bool     parser_decl_match_contextual_keyword(Parser *parser,
                                              const char *keyword);
bool     parser_decl_check_contextual_keyword(Parser *parser,
                                              const char *keyword);
bool     parser_decl_parse_next_function_clause(Parser *parser, ASTNode *func,
                                                bool is_action,
                                                bool *matched_out);
void     parse_optional_effect_clause(Parser *parser, bool *has_clause_out,
                                      uint32_t *mask_out);
ASTNode *parse_class_declaration(Parser *parser);
ASTNode *parse_subject_declaration(Parser *parser);
ASTNode *parse_vessel_declaration(Parser *parser);
ASTNode *parse_struct_declaration(Parser *parser);
ASTNode *parse_object_declaration(Parser *parser);
ASTNode *parse_tobject_declaration(Parser *parser);
ASTNode *parse_action_declaration(Parser *parser);
ASTNode *parse_type_declaration(Parser *parser, NominalDeclKind decl_kind);
ASTNode *parse_type_alias_declaration(Parser *parser);
ASTNode *parse_record_type_alias_struct(Parser *parser, Token name);
ASTNode *parse_extern_block(Parser *parser);

/* --- Domain types (parser_domain.c) --- */
ASTNode *parse_ability_declaration(Parser *parser, bool is_innate);
ASTNode *parse_role_declaration(Parser *parser);
ASTNode *parse_party_declaration(Parser *parser);
ASTNode *parse_roster_declaration(Parser *parser);
ASTNode *parse_world_declaration(Parser *parser);
ASTNode *parse_intent_declaration(Parser *parser);
ASTNode *parse_intent_step(Parser *parser);
bool parser_intent_match_keyword(Parser *parser, const char *keyword);
bool intent_append_node(ASTNode ***items, size_t *count, size_t *capacity,
                        ASTNode *node);
void intent_append_binding(ASTNode *intent, ASTNode *node);
bool intent_append_name(char ***items, size_t *count, size_t *capacity,
                        const char *name);
bool intent_has_involves_alias(ASTNode *intent, const char *alias);
bool intent_has_value_alias(ASTNode *intent, const char *alias);
void parse_intent_apply_defaults(ASTNode *intent);
void parse_intent_name_list(Parser *parser, char ***items, size_t *count,
                            size_t *capacity, const char *message);
void parse_intent_param_list(Parser *parser, ASTNode *intent);
ASTNode *parse_relation_declaration(Parser *parser);
ASTNode *parse_effect_declaration(Parser *parser);
ASTNode *parse_zone_declaration(Parser *parser);
ASTNode *parse_event_declaration(Parser *parser);

#endif /* PERGYRA_PARSER_INTERNAL_H */
