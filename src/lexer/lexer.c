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

#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/*
 * Keyword table for fast keyword lookup
 */
typedef struct
{
    const char *text;
    TokenType   type;
} Keyword;

static const Keyword keywords[] = {
    {"let",        TOKEN_LET},
    {"claim_slot", TOKEN_CLAIM_SLOT},
    {"write",      TOKEN_WRITE},
    {"read",       TOKEN_READ},
    {"release",    TOKEN_RELEASE},
    {"with",       TOKEN_WITH},
    {"as",         TOKEN_AS},
    {"parallel",   TOKEN_PARALLEL},
    {"log",        TOKEN_LOG},
    {"true",       TOKEN_BOOL_TRUE},
    {"false",      TOKEN_BOOL_FALSE},
    {NULL,         TOKEN_ERROR}
};

/*
 * Fast character classification using inline assembly
 */
static inline bool
IsAlphaFast(char c)
{
    bool result;
    
    __asm__ volatile (
        "cmpb $'A', %%al\n\t"
        "jl 1f\n\t"
        "cmpb $'Z', %%al\n\t"
        "jle 2f\n\t"
        "cmpb $'a', %%al\n\t"
        "jl 1f\n\t"
        "cmpb $'z', %%al\n\t"
        "jle 2f\n\t"
        "cmpb $'_', %%al\n\t"
        "je 2f\n\t"
        "1: movb $0, %%dl\n\t"
        "jmp 3f\n\t"
        "2: movb $1, %%dl\n\t"
        "3: movb %%dl, %0"
        : "=m" (result)
        : "a" (c)
        : "dl"
    );
    
    return result;
}

static inline bool
IsDigitFast(char c)
{
    bool result;
    
    __asm__ volatile (
        "cmpb $'0', %%al\n\t"
        "jl 1f\n\t"
        "cmpb $'9', %%al\n\t"
        "jle 2f\n\t"
        "1: movb $0, %%dl\n\t"
        "jmp 3f\n\t"
        "2: movb $1, %%dl\n\t"
        "3: movb %%dl, %0"
        : "=m" (result)
        : "a" (c)
        : "dl"
    );
    
    return result;
}

static inline bool
IsAlnumFast(char c)
{
    return IsAlphaFast(c) || IsDigitFast(c);
}

/*
 * Create a new lexer instance
 */
Lexer *
LexerCreate(const char *source)
{
    Lexer *lexer;
    
    if (source == NULL)
        return NULL;
    
    lexer = malloc(sizeof(Lexer));
    if (lexer == NULL)
        return NULL;
    
    lexer->source = source;
    lexer->current = source;
    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->hasError = false;
    lexer->errorMsg[0] = '\0';
    
    return lexer;
}

/*
 * Destroy lexer instance and free resources
 */
void
LexerDestroy(Lexer *lexer)
{
    if (lexer != NULL)
        free(lexer);
}

/*
 * Peek at current character without advancing
 */
static char
Peek(Lexer *lexer)
{
    return *lexer->current;
}

/*
 * Peek at next character without advancing
 */
static char
PeekNext(Lexer *lexer)
{
    if (*lexer->current == '\0')
        return '\0';
    return *(lexer->current + 1);
}

/*
 * Advance to next character and update position
 */
static char
Advance(Lexer *lexer)
{
    char c;
    
    if (*lexer->current == '\0')
        return '\0';
    
    c = *lexer->current;
    lexer->current++;
    lexer->position++;
    
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    
    return c;
}

/*
 * Skip whitespace characters
 */
static void
SkipWhitespace(Lexer *lexer)
{
    char c;
    
    for (;;) {
        c = Peek(lexer);
        if (c == ' ' || c == '\t' || c == '\r') {
            Advance(lexer);
        } else if (c == '\n') {
            Advance(lexer);
            break;
        } else {
            break;
        }
    }
}

/*
 * Skip comment blocks
 */
static void
SkipComment(Lexer *lexer)
{
    if (Peek(lexer) == '/' && PeekNext(lexer) == '/') {
        /* Single line comment */
        while (Peek(lexer) != '\n' && Peek(lexer) != '\0')
            Advance(lexer);
    } else if (Peek(lexer) == '/' && PeekNext(lexer) == '*') {
        /* Multi-line comment */
        Advance(lexer); /* '/' */
        Advance(lexer); /* '*' */
        
        for (;;) {
            if (Peek(lexer) == '\0')
                break;
            if (Peek(lexer) == '*' && PeekNext(lexer) == '/') {
                Advance(lexer); /* '*' */
                Advance(lexer); /* '/' */
                break;
            }
            Advance(lexer);
        }
    }
}

/*
 * Check if text matches any keyword
 */
static TokenType
CheckKeyword(const char *text, size_t length)
{
    int i;
    
    for (i = 0; keywords[i].text != NULL; i++) {
        if (strlen(keywords[i].text) == length && 
            strncmp(keywords[i].text, text, length) == 0) {
            return keywords[i].type;
        }
    }
    
    return TOKEN_IDENTIFIER;
}

/*
 * Create identifier token
 */
static Token
MakeIdentifierToken(Lexer *lexer, const char *start)
{
    Token token = {0};
    
    token.text = (char *)start;
    token.length = lexer->current - start;
    token.line = lexer->line;
    token.column = lexer->column - token.length;
    
    /* Check for keywords */
    token.type = CheckKeyword(start, token.length);
    
    /* Check for type names (start with uppercase) */
    if (token.type == TOKEN_IDENTIFIER && isupper(*start))
        token.type = TOKEN_TYPE_NAME;
    
    return token;
}

/*
 * Create number token
 */
static Token
MakeNumberToken(Lexer *lexer, const char *start)
{
    Token  token = {0};
    bool   isFloat = false;
    size_t i;
    
    token.text = (char *)start;
    token.length = lexer->current - start;
    token.line = lexer->line;
    token.column = lexer->column - token.length;
    
    /* Check if it's a floating point number */
    for (i = 0; i < token.length; i++) {
        if (start[i] == '.') {
            isFloat = true;
            break;
        }
    }
    
    if (isFloat) {
        token.type = TOKEN_FLOAT;
        token.value.floatValue = strtod(start, NULL);
    } else {
        token.type = TOKEN_INTEGER;
        token.value.intValue = strtoll(start, NULL, 10);
    }
    
    return token;
}

/*
 * Create string token
 */
static Token
MakeStringToken(Lexer *lexer, const char *start, char quote)
{
    Token token = {0};
    
    token.type = TOKEN_STRING;
    token.text = (char *)start + 1; /* Skip opening quote */
    token.length = (lexer->current - start) - 2; /* Skip both quotes */
    token.line = lexer->line;
    token.column = lexer->column - (lexer->current - start);
    
    return token;
}

/*
 * Create error token
 */
static Token
MakeErrorToken(Lexer *lexer, const char *message)
{
    Token token = {0};
    
    token.type = TOKEN_ERROR;
    token.text = (char *)message;
    token.length = strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    
    lexer->hasError = true;
    strncpy(lexer->errorMsg, message, sizeof(lexer->errorMsg) - 1);
    
    return token;
}

/*
 * Get next token from input stream
 */
Token
LexerNextToken(Lexer *lexer)
{
    const char *start;
    char        c;
    char        quote;
    
    if (lexer == NULL)
        return (Token){TOKEN_ERROR, "Invalid lexer", 13, 0, 0, {0}};
    
    SkipWhitespace(lexer);
    
    /* Handle comments */
    if (Peek(lexer) == '/' && 
        (PeekNext(lexer) == '/' || PeekNext(lexer) == '*')) {
        SkipComment(lexer);
        SkipWhitespace(lexer);
    }
    
    start = lexer->current;
    c = Advance(lexer);
    
    /* End of file */
    if (c == '\0')
        return (Token){TOKEN_EOF, "", 0, lexer->line, lexer->column, {0}};
    
    /* Newline */
    if (c == '\n')
        return (Token){TOKEN_NEWLINE, "\n", 1, lexer->line - 1, 1, {0}};
    
    /* Identifiers and keywords */
    if (IsAlphaFast(c)) {
        while (IsAlnumFast(Peek(lexer)))
            Advance(lexer);
        return MakeIdentifierToken(lexer, start);
    }
    
    /* Numbers */
    if (IsDigitFast(c)) {
        while (IsDigitFast(Peek(lexer)))
            Advance(lexer);
        
        /* Handle decimal point */
        if (Peek(lexer) == '.' && IsDigitFast(PeekNext(lexer))) {
            Advance(lexer); /* '.' */
            while (IsDigitFast(Peek(lexer)))
                Advance(lexer);
        }
        
        return MakeNumberToken(lexer, start);
    }
    
    /* String literals */
    if (c == '"' || c == '\'') {
        quote = c;
        while (Peek(lexer) != quote && Peek(lexer) != '\0') {
            if (Peek(lexer) == '\n')
                lexer->line++;
            Advance(lexer);
        }
        
        if (Peek(lexer) == '\0')
            return MakeErrorToken(lexer, "Unterminated string");
        
        Advance(lexer); /* Closing quote */
        return MakeStringToken(lexer, start, quote);
    }
    
    /* Single character tokens */
    switch (c) {
    case '=':
        return (Token){TOKEN_ASSIGN, "=", 1, 
                      lexer->line, lexer->column - 1, {0}};
    case '(':
        return (Token){TOKEN_LPAREN, "(", 1, 
                      lexer->line, lexer->column - 1, {0}};
    case ')':
        return (Token){TOKEN_RPAREN, ")", 1, 
                      lexer->line, lexer->column - 1, {0}};
    case '{':
        return (Token){TOKEN_LBRACE, "{", 1, 
                      lexer->line, lexer->column - 1, {0}};
    case '}':
        return (Token){TOKEN_RBRACE, "}", 1, 
                      lexer->line, lexer->column - 1, {0}};
    case '<':
        return (Token){TOKEN_LANGLE, "<", 1, 
                      lexer->line, lexer->column - 1, {0}};
    case '>':
        return (Token){TOKEN_RANGLE, ">", 1, 
                      lexer->line, lexer->column - 1, {0}};
    case '.':
        return (Token){TOKEN_DOT, ".", 1, 
                      lexer->line, lexer->column - 1, {0}};
    case ';':
        return (Token){TOKEN_SEMICOLON, ";", 1, 
                      lexer->line, lexer->column - 1, {0}};
    case ',':
        return (Token){TOKEN_COMMA, ",", 1, 
                      lexer->line, lexer->column - 1, {0}};
    default:
        return MakeErrorToken(lexer, "Unexpected character");
    }
}

/*
 * Check if lexer has encountered an error
 */
bool
LexerHasError(const Lexer *lexer)
{
    return lexer != NULL ? lexer->hasError : true;
}

/*
 * Get error message from lexer
 */
const char *
LexerGetError(const Lexer *lexer)
{
    return lexer != NULL ? lexer->errorMsg : "Invalid lexer";
}

/*
 * Convert token type to string representation
 */
const char *
TokenTypeToString(TokenType type)
{
    switch (type) {
    case TOKEN_LET:        return "LET";
    case TOKEN_CLAIM_SLOT: return "CLAIM_SLOT";
    case TOKEN_WRITE:      return "WRITE";
    case TOKEN_READ:       return "READ";
    case TOKEN_RELEASE:    return "RELEASE";
    case TOKEN_WITH:       return "WITH";
    case TOKEN_AS:         return "AS";
    case TOKEN_PARALLEL:   return "PARALLEL";
    case TOKEN_LOG:        return "LOG";
    case TOKEN_ASSIGN:     return "ASSIGN";
    case TOKEN_LPAREN:     return "LPAREN";
    case TOKEN_RPAREN:     return "RPAREN";
    case TOKEN_LBRACE:     return "LBRACE";
    case TOKEN_RBRACE:     return "RBRACE";
    case TOKEN_LANGLE:     return "LANGLE";
    case TOKEN_RANGLE:     return "RANGLE";
    case TOKEN_DOT:        return "DOT";
    case TOKEN_SEMICOLON:  return "SEMICOLON";
    case TOKEN_COMMA:      return "COMMA";
    case TOKEN_INTEGER:    return "INTEGER";
    case TOKEN_FLOAT:      return "FLOAT";
    case TOKEN_STRING:     return "STRING";
    case TOKEN_BOOL_TRUE:  return "BOOL_TRUE";
    case TOKEN_BOOL_FALSE: return "BOOL_FALSE";
    case TOKEN_IDENTIFIER: return "IDENTIFIER";
    case TOKEN_TYPE_NAME:  return "TYPE_NAME";
    case TOKEN_EOF:        return "EOF";
    case TOKEN_ERROR:      return "ERROR";
    case TOKEN_NEWLINE:    return "NEWLINE";
    default:               return "UNKNOWN";
    }
}

/*
 * Print token information for debugging
 */
void
TokenPrint(const Token *token)
{
    size_t i;
    
    if (token == NULL)
        return;
    
    printf("Token{type: %s, text: \"", TokenTypeToString(token->type));
    
    /* Print text with length limit */
    for (i = 0; i < token->length && i < 50; i++)
        printf("%c", token->text[i]);
    
    printf("\", line: %u, col: %u", token->line, token->column);
    
    /* Print value based on token type */
    switch (token->type) {
    case TOKEN_INTEGER:
        printf(", value: %lld", token->value.intValue);
        break;
    case TOKEN_FLOAT:
        printf(", value: %f", token->value.floatValue);
        break;
    case TOKEN_BOOL_TRUE:
    case TOKEN_BOOL_FALSE:
        printf(", value: %s", token->value.boolValue ? "true" : "false");
        break;
    default:
        break;
    }
    
    printf("}\n");
}