/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Pergyra Language Project nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PERGYRA_PARSER_H
#define PERGYRA_PARSER_H

#include "ast.h"
#include "../lexer/lexer.h"

/*
 * Parser state structure
 */
typedef struct
{
    Lexer  *lexer;
    Token   current_token;
    Token   previous_token;
    PgyTokenStreamHandle token_stream;
    bool    has_error;
    bool    emit_recovered_errors;
    /* Owned, heap-exact. A diagnostic is the compiler's product, so it must
     * not be silently clipped: the previous fixed 512-byte buffer dropped
     * the tail of any message carrying a long identifier or literal, with
     * nothing in the output saying so. Read it through parser_get_error(),
     * which is total (never NULL). */
    char   *error_msg;
    
    /* Parsing context */
    bool    in_parallel_block;
    bool    in_with_statement;
    bool    in_async_context;
    bool    in_select_statement;
    bool    in_extern_block;
    bool    in_abstract_method_context;
    bool    no_struct_literal;
    bool    no_cast;
    bool    next_decl_exported;
    bool    last_func_decl_async;
    int     scope_depth;
    int     recursion_depth;
    int     binary_op_count;
    int     expr_root_depth;
    int     error_count;
    bool    panic_mode;
    StructuredComment *pending_doc_comment;
    const char *source_path;
    char   **decl_hint_names;
    ASTNodeType *decl_hint_types;
    NominalDeclKind *decl_hint_nominal_kinds;
    size_t   decl_hint_count;
    size_t   decl_hint_capacity;
    /* Transient generic-arg slot: set by parser_parse_primary when it
     * consumes `<T, U>` on a builtin callee (e.g. ClaimSecureSlot<Int>).
     * Consumed by finish_call to attach to the AST_CALL node. Always
     * NULL outside a single parse step. */
    GenericParams *pending_call_generic_args;
} Parser;

/*
 * Parser lifecycle functions
 */
Parser  *parser_create(Lexer *lexer);
void     parser_destroy(Parser *parser);
void     parser_set_recovered_error_output(Parser *parser, bool enabled);

/*
 * Main parsing function
 */
ASTNode *parser_parse_program(Parser *parser);
ASTNode *parser_parse_program_for_module_composition(Parser *parser);
bool parser_finalize_composed_intent_parameter_roles(ASTNode *program,
                                                     char **error_message);

/*
 * Statement parsing functions
 */
ASTNode *parser_parse_statement(Parser *parser);
ASTNode *parser_parse_let_declaration(Parser *parser);
ASTNode *parser_parse_with_statement(Parser *parser);
ASTNode *parser_parse_parallel_block(Parser *parser);
ASTNode *parser_parse_expression_statement(Parser *parser);
ASTNode *parser_parse_block(Parser *parser);
ASTNode *parser_parse_async_function(Parser *parser);
ASTNode *parser_parse_async_block(Parser *parser);
ASTNode *parser_parse_select_statement(Parser *parser);

/*
 * Expression parsing functions (precedence-based)
 */
ASTNode *parser_parse_expression(Parser *parser);
ASTNode *parser_parse_assignment(Parser *parser);
ASTNode *parser_parse_call(Parser *parser);
ASTNode *parser_parse_member_access(Parser *parser);
ASTNode *parser_parse_primary(Parser *parser);

/*
 * Specialized expression parsing
 */
ASTNode *parser_parse_function_call(Parser *parser, const char *function_name);
ASTNode *parser_parse_slot_operation(Parser *parser);
ASTNode *parser_parse_type_parameter(Parser *parser);
ASTNode *parser_parse_await_expression(Parser *parser);
ASTNode *parser_parse_channel_expression(Parser *parser);
ASTNode *parser_parse_spawn_expression(Parser *parser);

/*
 * Token management functions
 */
bool  parser_match(Parser *parser, PgyTokenType type);
bool  parser_check(Parser *parser, PgyTokenType type);
Token parser_advance(Parser *parser);
Token parser_consume(Parser *parser, PgyTokenType type, const char *message);
void parser_consume_statement_terminator(Parser *parser, const char *message);

/*
 * Error handling functions
 */
bool        parser_has_error(const Parser *parser);
const char *parser_get_error(const Parser *parser);
void        parser_error(Parser *parser, const char *format, ...);
void        parser_synchronize(Parser *parser);

/*
 * Utility functions
 */
bool parser_is_at_end(const Parser *parser);
bool parser_is_statement_start(PgyTokenType type);
bool parser_is_expression_start(PgyTokenType type);
bool parser_lookup_decl_hint(Parser *parser, const char *name,
                             ASTNodeType *node_type_out,
                             NominalDeclKind *nominal_kind_out);

#endif /* PERGYRA_PARSER_H */
