#include "fmt_layout.h"

bool
fmt_token_is_binary_operator(PgyTokenType type)
{
    switch (type) {
    case TOKEN_ASSIGN:
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_STAR:
    case TOKEN_SLASH:
    case TOKEN_PERCENT:
    case TOKEN_EQUAL:
    case TOKEN_NOT_EQUAL:
    case TOKEN_LESS:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER:
    case TOKEN_GREATER_EQUAL:
    case TOKEN_AND:
    case TOKEN_AMP:
    case TOKEN_OR:
    case TOKEN_PATTERN_OR:
    case TOKEN_ARROW:
    case TOKEN_CHANNEL_OP:
    case TOKEN_PIPE_ARROW:
        return true;
    default:
        return false;
    }
}

bool
fmt_token_needs_space(Token prev, Token current)
{
    if (prev.type == TOKEN_EOF || prev.type == TOKEN_NEWLINE)
        return false;
    if (current.type == TOKEN_EOF || current.type == TOKEN_NEWLINE)
        return false;

    if (current.type == TOKEN_RPAREN
        || current.type == TOKEN_RBRACKET
        || current.type == TOKEN_COMMA
        || current.type == TOKEN_SEMICOLON
        || current.type == TOKEN_COLON
        || current.type == TOKEN_DOT) {
        return false;
    }

    if (prev.type == TOKEN_LPAREN
        || prev.type == TOKEN_LBRACKET
        || prev.type == TOKEN_DOT) {
        return false;
    }

    if (current.type == TOKEN_LPAREN) {
        return prev.type == TOKEN_IF
            || prev.type == TOKEN_WHILE
            || prev.type == TOKEN_MATCH
            || prev.type == TOKEN_SELECT;
    }

    if (current.type == TOKEN_LBRACKET)
        return false;

    if (prev.type == TOKEN_COMMA || prev.type == TOKEN_COLON)
        return true;

    if (fmt_token_is_binary_operator(prev.type)
        || fmt_token_is_binary_operator(current.type)) {
        return true;
    }

    return true;
}

bool
fmt_token_starts_toplevel_decl(PgyTokenType type)
{
    switch (type) {
    case TOKEN_FUNC:
    case TOKEN_CLASS:
    case TOKEN_STRUCT:
    case TOKEN_ABILITY:
    case TOKEN_ROLE:
    case TOKEN_PARTY:
    case TOKEN_ROSTER:
    case TOKEN_WORLD:
    case TOKEN_ENUM:
    case TOKEN_EXTERN:
    case TOKEN_NAMESPACE:
    case TOKEN_IMPORT:
    case TOKEN_USE:
    case TOKEN_EVENT:
        return true;
    default:
        return false;
    }
}

bool
fmt_token_is_case_label(PgyTokenType type)
{
    return type == TOKEN_CASE || type == TOKEN_DEFAULT;
}

void
fmt_indent(FmtCtx *ctx)
{
    for (int i = 0; i < ctx->indent; i++)
        fprintf(ctx->out, "    ");
    ctx->at_line_start = false;
}

void
fmt_newline(FmtCtx *ctx)
{
    fprintf(ctx->out, "\n");
    ctx->at_line_start = true;
}
