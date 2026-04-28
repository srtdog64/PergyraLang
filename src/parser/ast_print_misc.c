#include "ast_print_internal.h"

bool
ast_print_needs_trailing_newline(ASTNodeType type)
{
    switch (type) {
        case AST_IDENTIFIER:
        case AST_NUMBER:
        case AST_STRING:
        case AST_BOOLEAN:
        case AST_TYPE:
        case AST_CHANNEL_TYPE:
        case AST_FUTURE_TYPE:
        case AST_CALL:
        case AST_BINARY:
        case AST_UNARY:
        case AST_MEMBER_ACCESS:
        case AST_ARRAY_ACCESS:
        case AST_AWAIT_EXPR:
        case AST_SPAWN_EXPR:
        case AST_EVENT_HANDLER_TYPE:
            return true;
        default:
            return false;
    }
}

