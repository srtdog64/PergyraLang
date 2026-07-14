#include "ast_print_internal.h"

#include <stdio.h>

bool
ast_print_expr_node(ASTNode *node, int indent)
{
    (void)indent;

    if (node == NULL)
        return false;

    switch (node->type) {
        case AST_IDENTIFIER:
            printf("%s", node->data.identifier.name);
            break;

        case AST_NUMBER:
            printf("%g", node->data.number.value);
            break;

        case AST_STRING:
            printf("\"%s\"", node->data.string.value);
            break;

        case AST_BOOLEAN:
            printf("%s", node->data.boolean.value ? "true" : "false");
            break;

        case AST_TYPE:
            printf("%s", node->data.type.name);
            if (node->data.type.generic_args)
                print_generic_params_inline(node->data.type.generic_args);
            break;

        case AST_CALL:
            ast_print_inline(node->data.call.callee);
            printf(ast_call_uses_braced_initializer_syntax(node)
                ? " { " : "(");
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (i > 0)
                    printf(", ");
                if (node->data.call.arg_names != NULL
                    && node->data.call.arg_names[i] != NULL)
                    printf("%s: ", node->data.call.arg_names[i]);
                ast_print_inline(node->data.call.arguments[i]);
            }
            printf(ast_call_uses_braced_initializer_syntax(node)
                ? " }" : ")");
            break;

        case AST_BINARY:
            printf("(");
            ast_print_inline(node->data.binary.left);
            printf(" %s ", ast_print_operator_to_string(node->data.binary.op.type));
            ast_print_inline(node->data.binary.right);
            printf(")");
            break;

        case AST_UNARY:
            printf("(%s", ast_print_operator_to_string(node->data.unary.op.type));
            ast_print_inline(node->data.unary.operand);
            printf(")");
            break;

        case AST_MEMBER_ACCESS:
            ast_print_inline(node->data.member.object);
            printf(".%s", node->data.member.name);
            break;

        case AST_ARRAY_ACCESS:
            ast_print_inline(node->data.array_access.array);
            printf("[");
            ast_print_inline(node->data.array_access.index);
            printf("]");
            break;

        case AST_AWAIT_EXPR:
            printf("await ");
            ast_print_inline(node->data.await_expr.expression);
            break;

        case AST_CHANNEL_TYPE:
            printf("Channel<");
            ast_print_inline(node->data.channel_type.element_type);
            printf(">");
            if (node->data.channel_type.capacity != NULL) {
                printf("[");
                ast_print_inline(node->data.channel_type.capacity);
                printf("]");
            }
            break;

        case AST_FUTURE_TYPE:
            printf("Future<");
            ast_print_inline(node->data.future_type.value_type);
            printf(">");
            break;

        case AST_SPAWN_EXPR:
            printf("spawn ");
            ast_print_inline(node->data.spawn_expr.function);
            break;

        default:
            return false;
    }

    return true;
}
