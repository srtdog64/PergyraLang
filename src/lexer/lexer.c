/*
 * Copyright (c) 2025 Pergyra Language Project
 * Lexer implementation - tokenizes Pergyra source code
 */

#include "lexer.h"
#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* Keyword table for efficient lookup */
typedef struct {
    const char* keyword;
    PgyTokenType   type;
} KeywordEntry;

static const KeywordEntry keywords[] = {
    {"let",      TOKEN_LET},
    {"func",     TOKEN_FUNC},
    {"subject",  TOKEN_SUBJECT},
    {"class",    TOKEN_CLASS},
    {"struct",   TOKEN_STRUCT},
    {"object",   TOKEN_OBJECT},
    {"tobject",  TOKEN_TOBJECT},
    {"vessel",   TOKEN_VESSEL},
    {"intent",   TOKEN_INTENT},
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
    {"innate",   TOKEN_INNATE},
    {"where",    TOKEN_WHERE},
    {"type",     PGY_TOKEN_TYPE},
    {"impl",     TOKEN_IMPL},
    {"async",    TOKEN_ASYNC},
    {"await",    TOKEN_AWAIT},
    {"channel",  TOKEN_CHANNEL},
    {"select",   TOKEN_SELECT},
    {"case",     TOKEN_CASE},
    {"default",  TOKEN_DEFAULT},
    {"spawn",    TOKEN_SPAWN},
    {"event",    TOKEN_EVENT},
    {"match",    TOKEN_MATCH},
    {"import",   TOKEN_IMPORT},
    {"use",      TOKEN_USE},
    {"unsafe",   TOKEN_UNSAFE},
    {"defer",    TOKEN_DEFER},
    {"bind",     TOKEN_BIND},
    {"ability",  TOKEN_ABILITY},
    {"role",     TOKEN_ROLE},
    {"include",  TOKEN_INCLUDE},
    {"override", TOKEN_OVERRIDE},
    {"secure",   TOKEN_SECURE},
    {"remote",   TOKEN_REMOTE},
    {"nondeterministic", TOKEN_NONDETERMINISTIC},
    {"collapse", TOKEN_COLLAPSE},
    {"local",    TOKEN_LOCAL},
    {"party",    TOKEN_PARTY},
    {"roster",   TOKEN_ROSTER},
    {"world",    TOKEN_WORLD},
    {"relation", TOKEN_RELATION},
    {"effect",   TOKEN_EFFECT},
    {"zone",     TOKEN_ZONE},

    {"slot",     TOKEN_SLOT},
    {"shared",   TOKEN_SHARED},
    {"extends",  TOKEN_EXTENDS},
    {"dyn",      TOKEN_DYN},
    /* NOTE: `context` is NOT a reserved keyword — it is an ordinary identifier */
    
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

static Token make_token(Lexer* lexer, PgyTokenType type, const char* start, size_t length);

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
static Token make_token(Lexer* lexer, PgyTokenType type, const char* start, size_t length) {
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
    snprintf(lexer->errorMsg, sizeof(lexer->errorMsg),
        "%s\nCode: %s\nReason: %s\nFix: %s",
        message,
        PGY_CODE_LEX_INVALID_TOKEN,
        PGY_CAUSE_LEX_INVALID_TOKEN,
        PGY_FIX_REMOVE_OR_ESCAPE_CHARACTER);
    
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
    bool is_float = false;

    while (isdigit(peek(lexer))) {
        advance(lexer);
    }

    // Look for decimal part
    if (peek(lexer) == '.' && isdigit(peek_next(lexer))) {
        advance(lexer); // consume '.'
        is_float = true;

        while (isdigit(peek(lexer))) {
            advance(lexer);
        }
    }

    // Optional type suffix: 'L' for Long (64-bit int).
    // Floats currently have no suffix; Long suffix on float is rejected
    // downstream via the existing "number literal malformed" path.
    if (!is_float && peek(lexer) == 'L') {
        advance(lexer);
    }

    size_t length = lexer->current - start;
    return make_token(lexer, TOKEN_NUMBER, start, length);
}

/* Scan string literal */
static Token scan_string(Lexer* lexer) {
    const char* start = lexer->current - 1;

    while (!is_at_end(lexer)) {
        char c = peek(lexer);

        if (c == '\\') {
            advance(lexer);
            if (!is_at_end(lexer))
                advance(lexer);
            continue;
        }

        if (c == '"')
            break;

        advance(lexer);
    }

    if (is_at_end(lexer)) {
        return error_token(lexer, "Unterminated string");
    }

    advance(lexer); // closing "

    size_t length = lexer->current - start;
    return make_token(lexer, TOKEN_STRING, start, length);
}

/* Scan multiline/raw string literal: """...""" */
static Token
scan_multiline_string(Lexer* lexer, const char* start)
{
    while (!is_at_end(lexer)) {
        char c = peek(lexer);

        if (c == '\\') {
            advance(lexer);
            if (!is_at_end(lexer))
                advance(lexer);
            continue;
        }

        if (c == '"' &&
            peek_next(lexer) == '"' &&
            peek_ahead(lexer, 2) == '"') {
            advance(lexer);
            advance(lexer);
            advance(lexer);
            size_t length = lexer->current - start;
            return make_token(lexer, TOKEN_MULTILINE_STRING, start, length);
        }

        advance(lexer);
    }

    return error_token(lexer, "Unterminated multiline string");
}

/* Scan interpolated string: f"Hello {name}" */
static Token
scan_interpolated_string(Lexer* lexer, const char* start)
{
    while (!is_at_end(lexer)) {
        char c = peek(lexer);

        if (c == '\\') {
            advance(lexer);
            if (!is_at_end(lexer))
                advance(lexer);
            continue;
        }

        if (c == '"')
            break;

        advance(lexer);
    }

    if (is_at_end(lexer)) {
        return error_token(lexer, "Unterminated interpolated string");
    }

    advance(lexer); // closing "

    size_t length = lexer->current - start;
    return make_token(lexer, TOKEN_INTERPOLATED_STRING, start, length);
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
        // Check for interpolated string: f"..."
        if (c == 'f' && !is_at_end(lexer) && peek(lexer) == '"') {
            advance(lexer); // consume the "
            const char* start = lexer->current - 1; // point to "
            return scan_interpolated_string(lexer, start);
        }
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
        case ':':
            if (peek(lexer) == '=') {
                advance(lexer);
                return make_token(lexer, TOKEN_COLON_ASSIGN, start, 2);
            }
            return make_token(lexer, TOKEN_COLON, start, 1);
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
        case '"':
            if (peek(lexer) == '"' && peek_next(lexer) == '"') {
                advance(lexer);
                advance(lexer);
                return scan_multiline_string(lexer, start);
            }
            return scan_string(lexer);
        
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
            return make_token(lexer, TOKEN_PATTERN_OR, start, 1);

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
