/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Lexer token string/debug helpers.
 */

#include "lexer.h"

#include <stdio.h>

const char* token_type_to_string(PgyTokenType type) {
    switch (type) {
        case TOKEN_LET: return "LET";
        case TOKEN_FUNC: return "FUNC";
        case TOKEN_SUBJECT: return "SUBJECT";
        case TOKEN_CLASS: return "CLASS";
        case TOKEN_STRUCT: return "STRUCT";
        case TOKEN_OBJECT: return "OBJECT";
        case TOKEN_TOBJECT: return "TOBJECT";
        case TOKEN_VESSEL: return "VESSEL";
        case TOKEN_INTENT: return "INTENT";
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
        case TOKEN_REFLECT: return "REFLECT";
        case TOKEN_INNATE: return "INNATE";
        case TOKEN_WHERE: return "WHERE";
        case TOKEN_ASYNC: return "ASYNC";
        case TOKEN_AWAIT: return "AWAIT";
        case TOKEN_CHANNEL: return "CHANNEL";
        case TOKEN_SELECT: return "SELECT";
        case TOKEN_CASE: return "CASE";
        case TOKEN_DEFAULT: return "DEFAULT";
        case TOKEN_SPAWN: return "SPAWN";
        case TOKEN_EVENT: return "EVENT";
        case TOKEN_IMPORT: return "IMPORT";
        case TOKEN_USE: return "USE";
        case TOKEN_UNSAFE: return "UNSAFE";
        case TOKEN_DEFER: return "DEFER";
        case TOKEN_BIND: return "BIND";
        case TOKEN_ABILITY: return "ABILITY";
        case TOKEN_ROLE: return "ROLE";
        case TOKEN_PARTY: return "PARTY";
        case TOKEN_SHARED: return "SHARED";
        case TOKEN_EXTENDS: return "EXTENDS";
        case TOKEN_ROSTER: return "ROSTER";
        case TOKEN_WORLD: return "WORLD";
        case TOKEN_RELATION: return "RELATION";
        case TOKEN_EFFECT: return "EFFECT";
        case TOKEN_ZONE: return "ZONE";
        case TOKEN_INCLUDE: return "INCLUDE";
        case TOKEN_OVERRIDE: return "OVERRIDE";
        case TOKEN_SECURE: return "SECURE";
        case TOKEN_REMOTE: return "REMOTE";
        case TOKEN_NONDETERMINISTIC: return "NONDETERMINISTIC";
        case TOKEN_COLLAPSE: return "COLLAPSE";
        case TOKEN_LOCAL: return "LOCAL";
        case TOKEN_MATCH: return "MATCH";
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
        case TOKEN_COLON_ASSIGN: return ":=";
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
        case TOKEN_DOC_COMMENT: return "DOC_COMMENT";
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
