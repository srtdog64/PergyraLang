/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Lexer token string/debug helpers.
 */

#include "lexer_keywords.h"

#include <stdio.h>

const char* token_type_to_string(PgyTokenType type) {
    const char *keyword_identity = lexer_keyword_debug_name(type);
    if (keyword_identity != NULL) {
        return keyword_identity;
    }

    switch (type) {
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
        case TOKEN_AMP: return "&";
        case TOKEN_OR: return "||";
        case TOKEN_PATTERN_OR: return "|";
        case TOKEN_NOT: return "!";
        case TOKEN_ARROW: return "->";
        case TOKEN_CHANNEL_OP: return "<-";
        case TOKEN_QUESTION: return "?";
        case TOKEN_OPTIONAL_CHAIN: return "?.";
        case TOKEN_COALESCE: return "??";
        case TOKEN_PIPE_ARROW: return "|>";
        case TOKEN_ELLIPSIS: return "...";
        case TOKEN_DOT: return ".";
        case TOKEN_AT: return "@";
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
        case TOKEN_MULTILINE_STRING: return "MULTILINE_STRING";
        case TOKEN_INTERPOLATED_STRING: return "INTERPOLATED_STRING";
        case TOKEN_NEWLINE: return "NEWLINE";
        case TOKEN_DOC_COMMENT: return "DOC_COMMENT";
        case TOKEN_DOC_TAG_WHAT: return "DOC_TAG_WHAT";
        case TOKEN_DOC_TAG_WHY: return "DOC_TAG_WHY";
        case TOKEN_DOC_TAG_ALT: return "DOC_TAG_ALT";
        case TOKEN_DOC_TAG_NEXT: return "DOC_TAG_NEXT";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void token_print(const Token* token) {
    printf("Token{type: %s, text: \"%s\", line: %d, col: %d}\n",
           token_type_to_string(token->type),
           token->text,
           token->line,
           token->column);
}
