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
    bool    has_error;
    char    error_msg[512];
    
    /* Parsing context */
    bool    in_parallel_block;
    bool    in_with_statement;
    int     scope_depth;
} Parser;

/*
 * Parser lifecycle functions
 */
Parser  *parser_create(Lexer *lexer);
void     parser_destroy(Parser *parser);

/*
 * Main parsing function
 */
ASTNode *parser_parse_program(Parser *parser);

/*
 * Statement parsing functions
 */
ASTNode *parser_parse_statement(Parser *parser);
ASTNode *parser_parse_let_declaration(Parser *parser);
ASTNode *parser_parse_with_statement(Parser *parser);
ASTNode *parser_parse_parallel_block(Parser *parser);
ASTNode *parser_parse_expression_statement(Parser *parser);
ASTNode *parser_parse_block(Parser *parser);

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

/*
 * Token management functions
 */
bool  parser_match(Parser *parser, TokenType type);
bool  parser_check(Parser *parser, TokenType type);
Token parser_advance(Parser *parser);
Token parser_consume(Parser *parser, TokenType type, const char *message);

/*
 * Error handling functions
 */
bool        parser_has_error(const Parser *parser);
const char *parser_get_error(const Parser *parser);
void        parser_error(Parser *parser, const char *message);
void        parser_synchronize(Parser *parser);

/*
 * Utility functions
 */
bool parser_is_at_end(const Parser *parser);
bool parser_is_statement_start(TokenType type);
bool parser_is_expression_start(TokenType type);

#endif /* PERGYRA_PARSER_H */