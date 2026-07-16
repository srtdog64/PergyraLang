#include "ast_print_internal.h"

#include <stdio.h>
#include <string.h>

void
ast_print_number_text(const ASTNode *node, char *buffer, size_t capacity)
{
    if (buffer == NULL || capacity == 0)
        return;

    buffer[0] = '\0';
    if (node == NULL || node->type != AST_NUMBER)
        return;

    int written = snprintf(buffer, capacity, "%g", node->data.number.value);
    if (written < 0 || (size_t)written >= capacity) {
        buffer[0] = '\0';
        return;
    }

    if (ast_number_is_float(node)
        && strpbrk(buffer, ".eE") == NULL
        && (size_t)written + 2 < capacity) {
        buffer[written] = '.';
        buffer[written + 1] = '0';
        buffer[written + 2] = '\0';
    }
}

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
