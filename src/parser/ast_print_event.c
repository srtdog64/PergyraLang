#include "ast_print_internal.h"

#include <stdio.h>

bool
ast_print_event_node(ASTNode *node, int indent)
{
    if (node == NULL)
        return false;

    switch (node->type) {
        case AST_EVENT_DECL:
            printf("Event: %s\n", node->data.event_decl.name);
            if (node->data.event_decl.param_count > 0) {
                ast_print_indent(indent + 1);
                printf("Parameters:\n");
                for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
                    ast_print(node->data.event_decl.params[i], indent + 2);
                }
            }
            if (node->data.event_decl.return_type != NULL) {
                ast_print_indent(indent + 1);
                printf("Returns: ");
                ast_print_inline(node->data.event_decl.return_type);
                printf("\n");
            }
            break;

        case AST_EVENT_SUBSCRIBE:
            printf("EventSubscribe: ");
            ast_print_inline(node->data.event_op.event);
            printf(" += ");
            ast_print_inline(node->data.event_op.handler);
            printf("\n");
            break;

        case AST_EVENT_UNSUBSCRIBE:
            printf("EventUnsubscribe: ");
            ast_print_inline(node->data.event_op.event);
            printf(" -= ");
            ast_print_inline(node->data.event_op.handler);
            printf("\n");
            break;

        case AST_EVENT_INVOKE:
            printf("EventInvoke: ");
            ast_print_inline(node->data.event_invoke.event);
            printf("(");
            for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_inline(node->data.event_invoke.arguments[i]);
            }
            printf(")\n");
            break;

        case AST_EVENT_HANDLER_TYPE:
            printf("func(");
            for (size_t i = 0; i < node->data.event_handler_type.param_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_inline(node->data.event_handler_type.param_types[i]);
            }
            printf(")");
            if (node->data.event_handler_type.return_type != NULL) {
                printf(" -> ");
                ast_print_inline(node->data.event_handler_type.return_type);
            }
            break;

        default:
            return false;
    }

    return true;
}
