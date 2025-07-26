/*
 * Copyright (c) 2025 Pergyra Language Project
 * Lexer implementation - tokenizes Pergyra source code
 */

#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* Keyword table for efficient lookup */
typedef struct {
    const char* keyword;
    TokenType   type;
} KeywordEntry;

static const KeywordEntry keywords[] = {
    {"let",      TOKEN_LET},
    {"func",     TOKEN_FUNC},
    {"class",    TOKEN_CLASS},
    {"struct",   TOKEN_STRUCT},
    {"with",     TOKEN_WITH},
    {"as",       TOKEN_AS},
    {"Parallel", TOKEN_PARALLEL},
    {"for",      TOKEN_FOR},
    {"in",       TOKEN_IN},
    {"if",       TOKEN_IF},
    {"else",     TOKEN_ELSE},
    {"while",    TOKEN_WHILE},
    {"return",   TOKEN_RETURN},
    {"true",     TOKEN_TRUE},
    {"false",    TOKEN_FALSE},
    {"public",   TOKEN_PUBLIC},
    {"private",  TOKEN_PRIVATE},
    {"where",    TOKEN_WHERE},
    {"type",     TOKEN_TYPE},
    {"trait",    TOKEN_TRAIT},
    {"impl",     TOKEN_IMPL},
    {NULL,       TOKEN_EOF}
};

/* Create a new lexer */
Lexer* lexer_create(const char* source) {
    Lexer* lexer = calloc(1, sizeof(Lexer));
    if (!lexer) return NULL;
    
    lexer->source = source;
    lexer->current = source;
    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->hasError = false;
    
    return lexer;
}

/* Destroy lexer */
void lexer_destroy(Lexer* lexer) {
    if (lexer) {
        free(lexer);
    }
}

/* Check if at end of source */
static bool is_at_end(const Lexer* lexer) {
    return *lexer->current == '\0';
}

/* Advance to next character */
static char advance(Lexer* lexer) {
    if (is_at_end(lexer)) return '\0';
    
    char c = *lexer->current++;
    lexer->position++;
    
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    
    return c;
}

/* Peek at current character without consuming */
static char peek(const Lexer* lexer) {
    if (is_at_end(lexer)) return '\0';
    return *lexer->current;
}

/* Peek at next character */
static char peek_next(const Lexer* lexer) {
    if (is_at_end(lexer) || lexer->current[1] == '\0') return '\0';
    return lexer->current[1];
}

/* Skip whitespace and comments */
static void skip_whitespace(Lexer* lexer) {
    while (true) {
        char c = peek(lexer);
        
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;
                
            case '\n':
                advance(lexer);
                break;
                
            case '/':
                if (peek_next(lexer) == '/') {
                    // Single-line comment
                    advance(lexer); // /
                    advance(lexer); // /
                    while (peek(lexer) != '\n' && !is_at_end(lexer)) {
                        advance(lexer);
                    }
                } else if (peek_next(lexer) == '*') {
                    // Multi-line comment
                    advance(lexer); // /
                    advance(lexer); // *
                    while (!is_at_end(lexer)) {
                        if (peek(lexer) == '*' && peek_next(lexer) == '/') {
                            advance(lexer); // *
                            advance(lexer); // /
                            break;
                        }
                        advance(lexer);
                    }
                } else {
                    return;
                }
                break;
                
            default:
                return;
        }
    }
}

/* Check if character is valid identifier start */
static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

/* Check if character is valid identifier continuation */
static bool is_alnum(char c) {
    return is_alpha(c) || (c >= '0' && c <= '9');
}

/* Create a token */
static Token make_token(Lexer* lexer, TokenType type, const char* start, size_t length) {
    Token token;
    token.type = type;
    token.text = strndup(start, length);
    token.length = length;
    token.line = lexer->line;
    token.column = lexer->column - length;
    
    return token;
}

/* Create an error token */
static Token error_token(Lexer* lexer, const char* message) {
    lexer->hasError = true;
    strncpy(lexer->errorMsg, message, sizeof(lexer->errorMsg) - 1);
    
    Token token;
    token.type = TOKEN_ERROR;
    token.text = strdup(message);
    token.length = strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    
    return token;
}

/* Scan identifier or keyword */
static Token scan_identifier(Lexer* lexer) {
    const char* start = lexer->current - 1;
    
    while (is_alnum(peek(lexer))) {
        advance(lexer);
    }
    
    size_t length = lexer->current - start;
    
    // Check if it's a keyword
    for (const KeywordEntry* entry = keywords; entry->keyword != NULL; entry++) {
        if (strlen(entry->keyword) == length &&
            strncmp(start, entry->keyword, length) == 0) {
            return make_token(lexer, entry->type, start, length);
        }
    }
    
    // Check for built-in functions (PascalCase)
    if (length > 0 && isupper(start[0])) {
        const char* builtins[] = {
            "ClaimSlot", "ClaimSecureSlot", "Write", "Read", 
            "Release", "Log", "Panic", NULL
        };
        
        for (int i = 0; builtins[i] != NULL; i++) {
            if (strlen(builtins[i]) == length &&
                strncmp(start, builtins[i], length) == 0) {
                return make_token(lexer, TOKEN_IDENTIFIER, start, length);
            }
        }
    }
    
    return make_token(lexer, TOKEN_IDENTIFIER, start, length);
}

/* Scan number literal */
static Token scan_number(Lexer* lexer) {
    const char* start = lexer->current - 1;
    
    while (isdigit(peek(lexer))) {
        advance(lexer);
    }
    
    // Look for decimal part
    if (peek(lexer) == '.' && isdigit(peek_next(lexer))) {
        advance(lexer); // consume '.'
        
        while (isdigit(peek(lexer))) {
            advance(lexer);
        }
    }
    
    size_t length = lexer->current - start;
    return make_token(lexer, TOKEN_NUMBER, start, length);
}

/* Scan string literal */
static Token scan_string(Lexer* lexer) {
    const char* start = lexer->current - 1;
    
    while (peek(lexer) != '"' && !is_at_end(lexer)) {
        if (peek(lexer) == '\n') lexer->line++;
        advance(lexer);
    }
    
    if (is_at_end(lexer)) {
        return error_token(lexer, "Unterminated string");
    }
    
    advance(lexer); // closing "
    
    size_t length = lexer->current - start;
    return make_token(lexer, TOKEN_STRING, start, length);
}

/* Get next token */
Token lexer_next_token(Lexer* lexer) {
    skip_whitespace(lexer);
    
    if (is_at_end(lexer)) {
        return make_token(lexer, TOKEN_EOF, "", 0);
    }
    
    const char* start = lexer->current;
    char c = advance(lexer);
    
    // Identifiers and keywords
    if (is_alpha(c)) {
        return scan_identifier(lexer);
    }
    
    // Numbers
    if (isdigit(c)) {
        return scan_number(lexer);
    }
    
    // Single-character tokens
    switch (c) {
        case '(': return make_token(lexer, TOKEN_LPAREN, start, 1);
        case ')': return make_token(lexer, TOKEN_RPAREN, start, 1);
        case '{': return make_token(lexer, TOKEN_LBRACE, start, 1);
        case '}': return make_token(lexer, TOKEN_RBRACE, start, 1);
        case '[': return make_token(lexer, TOKEN_LBRACKET, start, 1);
        case ']': return make_token(lexer, TOKEN_RBRACKET, start, 1);
        case ',': return make_token(lexer, TOKEN_COMMA, start, 1);
        case '.': 
            if (peek(lexer) == '.') {
                advance(lexer);
                return make_token(lexer, TOKEN_DOT, start, 2); // .. for ranges
            }
            return make_token(lexer, TOKEN_DOT, start, 1);
        case ';': return make_token(lexer, TOKEN_SEMICOLON, start, 1);
        case ':': return make_token(lexer, TOKEN_COLON, start, 1);
        case '+': return make_token(lexer, TOKEN_PLUS, start, 1);
        case '*': return make_token(lexer, TOKEN_STAR, start, 1);
        case '/': return make_token(lexer, TOKEN_SLASH, start, 1);
        case '%': return make_token(lexer, TOKEN_PERCENT, start, 1);
        case '"': return scan_string(lexer);
        
        // Multi-character tokens
        case '-':
            if (peek(lexer) == '>') {
                advance(lexer);
                return make_token(lexer, TOKEN_ARROW, start, 2);
            }
            return make_token(lexer, TOKEN_MINUS, start, 1);
            
        case '=':
            if (peek(lexer) == '=') {
                advance(lexer);
                return make_token(lexer, TOKEN_EQUAL, start, 2);
            }
            return make_token(lexer, TOKEN_ASSIGN, start, 1);
            
        case '!':
            if (peek(lexer) == '=') {
                advance(lexer);
                return make_token(lexer, TOKEN_NOT_EQUAL, start, 2);
            }
            return make_token(lexer, TOKEN_NOT, start, 1);
            
        case '<':
            if (peek(lexer) == '=') {
                advance(lexer);
                return make_token(lexer, TOKEN_LESS_EQUAL, start, 2);
            }
            return make_token(lexer, TOKEN_LESS, start, 1);
            
        case '>':
            if (peek(lexer) == '=') {
                advance(lexer);
                return make_token(lexer, TOKEN_GREATER_EQUAL, start, 2);
            }
            return make_token(lexer, TOKEN_GREATER, start, 1);
            
        case '&':
            if (peek(lexer) == '&') {
                advance(lexer);
                return make_token(lexer, TOKEN_AND, start, 2);
            }
            break;
            
        case '|':
            if (peek(lexer) == '|') {
                advance(lexer);
                return make_token(lexer, TOKEN_OR, start, 2);
            }
            break;
    }
    
    return error_token(lexer, "Unexpected character");
}

/* Check for errors */
bool lexer_has_error(const Lexer* lexer) {
    return lexer->hasError;
}

/* Get error message */
const char* lexer_get_error(const Lexer* lexer) {
    return lexer->errorMsg;
}

/* Convert token type to string */
const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_LET: return "LET";
        case TOKEN_FUNC: return "FUNC";
        case TOKEN_CLASS: return "CLASS";
        case TOKEN_STRUCT: return "STRUCT";
        case TOKEN_WITH: return "WITH";
        case TOKEN_AS: return "AS";
        case TOKEN_PARALLEL: return "PARALLEL";
        case TOKEN_FOR: return "FOR";
        case TOKEN_IN: return "IN";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_PUBLIC: return "PUBLIC";
        case TOKEN_PRIVATE: return "PRIVATE";
        case TOKEN_WHERE: return "WHERE";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_EOF: return "EOF";
        default: return "UNKNOWN";
    }
}

/* Print token for debugging */
void token_print(const Token* token) {
    printf("Token{type: %s, text: \"%s\", line: %d, col: %d}\n",
           token_type_to_string(token->type),
           token->text,
           token->line,
           token->column);
}
