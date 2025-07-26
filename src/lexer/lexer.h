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
    TOKEN_FUNC,
    TOKEN_CLASS,
    TOKEN_STRUCT,
    TOKEN_WITH,
    TOKEN_AS,
    TOKEN_PARALLEL,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_RETURN,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_PUBLIC,
    TOKEN_PRIVATE,
    TOKEN_WHERE,
    TOKEN_TYPE,
    TOKEN_TRAIT,
    TOKEN_IMPL,
    
    /* Operators */
    TOKEN_ASSIGN,       /* = */
    TOKEN_PLUS,         /* + */
    TOKEN_MINUS,        /* - */
    TOKEN_STAR,         /* * */
    TOKEN_SLASH,        /* / */
    TOKEN_PERCENT,      /* % */
    TOKEN_EQUAL,        /* == */
    TOKEN_NOT_EQUAL,    /* != */
    TOKEN_LESS,         /* < */
    TOKEN_LESS_EQUAL,   /* <= */
    TOKEN_GREATER,      /* > */
    TOKEN_GREATER_EQUAL,/* >= */
    TOKEN_AND,          /* && */
    TOKEN_OR,           /* || */
    TOKEN_NOT,          /* ! */
    TOKEN_ARROW,        /* -> */
    TOKEN_DOT,          /* . */
    TOKEN_COMMA,        /* , */
    TOKEN_COLON,        /* : */
    TOKEN_SEMICOLON,    /* ; */
    
    /* Delimiters */
    TOKEN_LPAREN,       /* ( */
    TOKEN_RPAREN,       /* ) */
    TOKEN_LBRACE,       /* { */
    TOKEN_RBRACE,       /* } */
    TOKEN_LBRACKET,     /* [ */
    TOKEN_RBRACKET,     /* ] */
    
    /* Literals */
    TOKEN_NUMBER,       /* Combined int/float */
    TOKEN_STRING,
    
    /* Identifiers */
    TOKEN_IDENTIFIER,
    
    /* Special */
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_NEWLINE,
    
    /* Structured Comments */
    TOKEN_DOC_COMMENT,      /* /// */
    TOKEN_DOC_TAG_WHAT,     /* [What]: */
    TOKEN_DOC_TAG_WHY,      /* [Why]: */
    TOKEN_DOC_TAG_ALT,      /* [Alt]: */
    TOKEN_DOC_TAG_NEXT      /* [Next]: */
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
Lexer      *lexer_create(const char *source);
void        lexer_destroy(Lexer *lexer);
Token       lexer_next_token(Lexer *lexer);
bool        lexer_has_error(const Lexer *lexer);
const char *lexer_get_error(const Lexer *lexer);

/*
 * Token utility functions
 */
const char *token_type_to_string(TokenType type);
void        token_print(const Token *token);

#endif /* PERGYRA_LEXER_H */