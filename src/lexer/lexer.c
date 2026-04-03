/*
 * Copyright (c) 2025 Pergyra Language Project
 * Lexer implementation - tokenizes Pergyra source code
 */

#include "lexer.h"
#include "../common/string_compat.h"
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
    {"subject",  TOKEN_CLASS},
    {"struct",   TOKEN_STRUCT},
    {"extern",   TOKEN_EXTERN},
    {"with",     TOKEN_WITH},
    {"as",       TOKEN_AS},
    {"parallel", TOKEN_PARALLEL},
    {"for",      TOKEN_FOR},
    {"in",       TOKEN_IN},
    {"if",       TOKEN_IF},
    {"else",     TOKEN_ELSE},
    {"while",    TOKEN_WHILE},
    {"return",   TOKEN_RETURN},
    {"break",    TOKEN_BREAK},
    {"continue", TOKEN_CONTINUE},
    {"enum",     TOKEN_ENUM},
    {"export",   TOKEN_EXPORT},
    {"namespace", TOKEN_NAMESPACE},
    {"true",     TOKEN_TRUE},
    {"false",    TOKEN_FALSE},
    {"public",   TOKEN_PUBLIC},
    {"private",  TOKEN_PRIVATE},
    {"where",    TOKEN_WHERE},
    {"type",     TOKEN_TYPE},
    {"trait",    TOKEN_TRAIT},
    {"impl",     TOKEN_IMPL},
    {"async",    TOKEN_ASYNC},
    {"await",    TOKEN_AWAIT},
    {"actor",    TOKEN_ACTOR},
    {"channel",  TOKEN_CHANNEL},
    {"select",   TOKEN_SELECT},
    {"case",     TOKEN_CASE},
    {"default",  TOKEN_DEFAULT},
    {"spawn",    TOKEN_SPAWN},
    {"event",    TOKEN_EVENT},
    {"match",    TOKEN_MATCH},
    {"import",   TOKEN_IMPORT},
    {"unsafe",   TOKEN_UNSAFE},
    {"defer",    TOKEN_DEFER},
    {"bind",     TOKEN_BIND},
    {"ability",  TOKEN_ABILITY},
    {"role",     TOKEN_ROLE},
    {"include",  TOKEN_INCLUDE},
    {"require",  TOKEN_REQUIRE},
    {"override", TOKEN_OVERRIDE},
    {"super",    TOKEN_SUPER},
    {"secure",   TOKEN_SECURE},
    {"party",    TOKEN_PARTY},
    {"slot",     TOKEN_SLOT},
    {"shared",   TOKEN_SHARED},
    {"context",  TOKEN_CONTEXT},
    {"extends",  TOKEN_EXTENDS},
    {"dyn",      TOKEN_DYN},
    {"systemic", TOKEN_SYSTEMIC},
    {"world",    TOKEN_WORLD},
    {"own",      TOKEN_OWN},
    {"ref",      TOKEN_REF},
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

/* Peek at character N positions ahead */
static char peek_ahead(const Lexer* lexer, size_t offset) {
    if (lexer == NULL || lexer->current[offset] == '\0') return '\0';
    return lexer->current[offset];
}

static Token make_token(Lexer* lexer, TokenType type, const char* start, size_t length);

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
                if (peek_next(lexer) == '/' && peek_ahead(lexer, 2) != '/') {
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

/* Scan structured doc comment line after first '/' is consumed. */
static Token scan_doc_comment(Lexer* lexer, const char* line_start) {
    advance(lexer); // second '/'
    advance(lexer); // third '/'

    while (peek(lexer) == ' ' || peek(lexer) == '\t') {
        advance(lexer);
    }

    const char* start = lexer->current;
    while (peek(lexer) != '\n' && !is_at_end(lexer)) {
        advance(lexer);
    }

    (void)line_start;
    return make_token(lexer, TOKEN_DOC_COMMENT, start, (size_t)(lexer->current - start));
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
    token.text = pergyra_strndup(start, length);
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
    token.text = pergyra_strdup(message);
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
        case '+':
            if (peek(lexer) == '=') {
                advance(lexer);
                return make_token(lexer, TOKEN_SUBSCRIBE, start, 2); /* += */
            }
            return make_token(lexer, TOKEN_PLUS, start, 1);
        case '*': return make_token(lexer, TOKEN_STAR, start, 1);
        case '/':
            if (peek(lexer) == '/' && peek_next(lexer) == '/') {
                return scan_doc_comment(lexer, start);
            }
            return make_token(lexer, TOKEN_SLASH, start, 1);
        case '%': return make_token(lexer, TOKEN_PERCENT, start, 1);
        case '?': return make_token(lexer, TOKEN_QUESTION, start, 1);
        case '"': return scan_string(lexer);
        
        // Multi-character tokens
        case '-':
            if (peek(lexer) == '>') {
                advance(lexer);
                return make_token(lexer, TOKEN_ARROW, start, 2);
            }
            if (peek(lexer) == '=') {
                advance(lexer);
                return make_token(lexer, TOKEN_UNSUBSCRIBE, start, 2); /* -= */
            }
            return make_token(lexer, TOKEN_MINUS, start, 1);
            
        case '=':
            if (peek(lexer) == '=') {
                advance(lexer);
                return make_token(lexer, TOKEN_EQUAL, start, 2);
            }
            if (peek(lexer) == '>') {
                advance(lexer);
                return make_token(lexer, TOKEN_LAMBDA, start, 2); /* => */
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
            if (peek(lexer) == '-') {
                advance(lexer);
                return make_token(lexer, TOKEN_CHANNEL_OP, start, 2);
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
            if (peek(lexer) == '>') {
                advance(lexer);
                return make_token(lexer, TOKEN_PIPE_ARROW, start, 2);
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
        case TOKEN_EXTERN: return "EXTERN";
        case TOKEN_WITH: return "WITH";
        case TOKEN_AS: return "AS";
        case TOKEN_PARALLEL: return "PARALLEL";
        case TOKEN_SLOT: return "SLOT";
        case TOKEN_FOR: return "FOR";
        case TOKEN_IN: return "IN";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_BREAK: return "BREAK";
        case TOKEN_CONTINUE: return "CONTINUE";
        case TOKEN_ENUM: return "ENUM";
        case TOKEN_EXPORT: return "EXPORT";
        case TOKEN_NAMESPACE: return "NAMESPACE";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_PUBLIC: return "PUBLIC";
        case TOKEN_PRIVATE: return "PRIVATE";
        case TOKEN_WHERE: return "WHERE";
        case TOKEN_ASYNC: return "ASYNC";
        case TOKEN_AWAIT: return "AWAIT";
        case TOKEN_ACTOR: return "ACTOR";
        case TOKEN_CHANNEL: return "CHANNEL";
        case TOKEN_SELECT: return "SELECT";
        case TOKEN_CASE: return "CASE";
        case TOKEN_DEFAULT: return "DEFAULT";
        case TOKEN_SPAWN: return "SPAWN";
        case TOKEN_EVENT: return "EVENT";
        case TOKEN_IMPORT: return "IMPORT";
        case TOKEN_UNSAFE: return "UNSAFE";
        case TOKEN_DEFER: return "DEFER";
        case TOKEN_BIND: return "BIND";
        case TOKEN_DYN: return "DYN";
        case TOKEN_SUBSCRIBE: return "+=";
        case TOKEN_UNSUBSCRIBE: return "-=";
        case TOKEN_LAMBDA: return "=>";
        case TOKEN_ASSIGN: return "=";
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_EQUAL: return "==";
        case TOKEN_NOT_EQUAL: return "!=";
        case TOKEN_LESS: return "<";
        case TOKEN_LESS_EQUAL: return "<=";
        case TOKEN_GREATER: return ">";
        case TOKEN_GREATER_EQUAL: return ">=";
        case TOKEN_AND: return "&&";
        case TOKEN_OR: return "||";
        case TOKEN_NOT: return "!";
        case TOKEN_ARROW: return "->";
        case TOKEN_CHANNEL_OP: return "<-";
        case TOKEN_DOT: return ".";
        case TOKEN_COMMA: return ",";
        case TOKEN_COLON: return ":";
        case TOKEN_SEMICOLON: return ";";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_LBRACKET: return "[";
        case TOKEN_RBRACKET: return "]";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_DOC_COMMENT: return "DOC_COMMENT";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
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
