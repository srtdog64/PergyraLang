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
#include <stddef.h>

/*
 * Token types for Pergyra language lexical analysis
 */
typedef enum
{
    /* Keywords */
    TOKEN_LET,
    TOKEN_FUNC,
    TOKEN_SUBJECT,
    TOKEN_CLASS,
    TOKEN_STRUCT,
    TOKEN_OBJECT,
    TOKEN_TOBJECT,
    TOKEN_VESSEL,
    TOKEN_INTENT,
    TOKEN_EXTERN,
    TOKEN_WITH,
    TOKEN_AS,
    TOKEN_PARALLEL,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_RETURN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_PUBLIC,
    TOKEN_PRIVATE,
    TOKEN_INNATE,
    TOKEN_WHERE,
    PGY_TOKEN_TYPE,
    TOKEN_IMPL,
    
    /* Enum */
    TOKEN_ENUM,

    /* Module keywords */
    TOKEN_EXPORT,
    TOKEN_NAMESPACE,

    /* Ownership qualifiers */
    TOKEN_OWN,          /* own — take ownership */
    TOKEN_REF,          /* ref — borrow */

    /* Compile-time reflection */
    TOKEN_REFLECT,      /* reflect — compile-time reflection, yields projection */

    /* Role/Ability keywords */
    TOKEN_ABILITY,
    TOKEN_ROLE,
    TOKEN_INCLUDE,
    TOKEN_OVERRIDE,
    TOKEN_SECURE,
    TOKEN_REMOTE,
    TOKEN_NONDETERMINISTIC,
    TOKEN_COLLAPSE,
    TOKEN_LOCAL,

    /* Party keywords */
    TOKEN_PARTY,
    TOKEN_SLOT,
    TOKEN_SHARED,
    TOKEN_EXTENDS,
    TOKEN_DYN,

    /* Roster and World keywords */
    TOKEN_ROSTER,
    TOKEN_WORLD,
    TOKEN_RELATION,
    TOKEN_EFFECT,
    TOKEN_ZONE,
    
    /* Async keywords */
    TOKEN_ASYNC,
    TOKEN_AWAIT,
    TOKEN_CHANNEL,
    TOKEN_SELECT,
    TOKEN_CASE,
    TOKEN_DEFAULT,
    TOKEN_SPAWN,
    TOKEN_MATCH,

    /* Module keywords */
    TOKEN_IMPORT,
    TOKEN_USE,

    /* Safety keywords */
    TOKEN_UNSAFE,
    TOKEN_TRANSACTION,
    TOKEN_COMPENSATE,
    TOKEN_FAIL,
    TOKEN_DEFER,
    TOKEN_BIND,

    /* Event keywords */
    TOKEN_EVENT,
    TOKEN_SUBSCRIBE,    /* += */
    TOKEN_UNSUBSCRIBE,  /* -= */
    TOKEN_LAMBDA,       /* => */

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
    TOKEN_AMP,          /* & (ability intersection) */
    TOKEN_OR,           /* || */
    TOKEN_PATTERN_OR,   /* | */
    TOKEN_NOT,          /* ! */
    TOKEN_ARROW,        /* -> */
    TOKEN_CHANNEL_OP,   /* <- */
    TOKEN_QUESTION,     /* ? (try/propagate) */
    TOKEN_OPTIONAL_CHAIN, /* ?. (reserved optional chaining) */
    TOKEN_COALESCE,     /* ?? (Option coalescing) */
    TOKEN_PIPE_ARROW,   /* |> (pipe) */
    TOKEN_ELLIPSIS,     /* ... (reserved spread/rest) */
    TOKEN_DOT,          /* . */
    TOKEN_AT,           /* @ (reserved attribute marker) */
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
    TOKEN_MULTILINE_STRING,   /* """...""" */
    TOKEN_INTERPOLATED_STRING,  /* f"Hello {name}" */

    /* Identifiers */
    TOKEN_IDENTIFIER,
    
    /* Special */
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_NEWLINE,
    
    /* Structured Comments */
    TOKEN_DOC_COMMENT,      /* /// */
    /* Domain / declaration-grade surfaces now have dedicated tokens.
       Clause words may still be parser-recognized identifiers. */

    /* Doc comment tags — produced by scan_doc_comment() */
    TOKEN_DOC_TAG_WHAT,     /* [What]: */
    TOKEN_DOC_TAG_WHY,      /* [Why]: */
    TOKEN_DOC_TAG_ALT,      /* [Alt]: */
    TOKEN_DOC_TAG_NEXT      /* [Next]: */
} PgyTokenType;

/*
 * Token structure containing lexical information
 */
typedef struct
{
    PgyTokenType   type;
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

typedef struct LexerTokenTextOwner LexerTokenTextOwner;

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
    /* Parse-lifetime owner shared by non-destructive lexer cursor copies. */
    LexerTokenTextOwner *token_text_owner;
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
const char *token_type_to_string(PgyTokenType type);
void        token_print(const Token *token);

#endif /* PERGYRA_LEXER_H */
