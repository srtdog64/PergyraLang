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

#ifndef PERGYRA_LEXER_H
#define PERGYRA_LEXER_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Token types for Pergyra language lexical analysis
 */
typedef enum
{
    /* Keywords */
    TOKEN_LET,
    TOKEN_CLAIM_SLOT,
    TOKEN_WRITE,
    TOKEN_READ,
    TOKEN_RELEASE,
    TOKEN_WITH,
    TOKEN_AS,
    TOKEN_PARALLEL,
    TOKEN_LOG,
    
    /* Operators */
    TOKEN_ASSIGN,       /* = */
    TOKEN_LPAREN,       /* ( */
    TOKEN_RPAREN,       /* ) */
    TOKEN_LBRACE,       /* { */
    TOKEN_RBRACE,       /* } */
    TOKEN_LANGLE,       /* < */
    TOKEN_RANGLE,       /* > */
    TOKEN_DOT,          /* . */
    TOKEN_SEMICOLON,    /* ; */
    TOKEN_COMMA,        /* , */
    
    /* Literals */
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_BOOL_TRUE,
    TOKEN_BOOL_FALSE,
    
    /* Identifiers */
    TOKEN_IDENTIFIER,
    TOKEN_TYPE_NAME,
    
    /* Special */
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_NEWLINE
} TokenType;

/*
 * Token structure containing lexical information
 */
typedef struct
{
    TokenType   type;
    char       *text;
    size_t      length;
    uint32_t    line;
    uint32_t    column;
    union {
        int64_t intValue;
        double  floatValue;
        bool    boolValue;
    } value;
} Token;

/*
 * Lexer state structure
 */
typedef struct
{
    const char *source;
    const char *current;
    size_t      position;
    uint32_t    line;
    uint32_t    column;
    bool        hasError;
    char        errorMsg[256];
} Lexer;

/*
 * Function prototypes
 */
Lexer      *LexerCreate(const char *source);
void        LexerDestroy(Lexer *lexer);
Token       LexerNextToken(Lexer *lexer);
bool        LexerHasError(const Lexer *lexer);
const char *LexerGetError(const Lexer *lexer);

/*
 * Token utility functions
 */
const char *TokenTypeToString(TokenType type);
void        TokenPrint(const Token *token);

#endif /* PERGYRA_LEXER_H */