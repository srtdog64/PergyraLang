/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST compact/inline print helpers.
 */

#include "ast_print_internal.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <io.h>
#define PGY_DUP _dup
#define PGY_DUP2 _dup2
#define PGY_CLOSE _close
#define PGY_FILENO _fileno
#else
#include <unistd.h>
#define PGY_DUP dup
#define PGY_DUP2 dup2
#define PGY_CLOSE close
#define PGY_FILENO fileno
#endif

static void
print_escaped_string(const char *value)
{
    const unsigned char *p = (const unsigned char *)value;

    printf("\"");
    if (p == NULL) {
        printf("\"");
        return;
    }

    while (*p != '\0') {
        switch (*p) {
        case '\\':
            printf("\\\\");
            break;
        case '"':
            printf("\\\"");
            break;
        case '\n':
            printf("\\n");
            break;
        case '\r':
            printf("\\r");
            break;
        case '\t':
            printf("\\t");
            break;
        default:
            putchar((int)*p);
            break;
        }
        p++;
    }
    printf("\"");
}

const char* ast_print_operator_to_string(PgyTokenType type) {
    switch (type) {
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
        case TOKEN_NOT: return "!";
        case TOKEN_ASSIGN: return "=";
        case TOKEN_COALESCE: return "??";
        default: return "?";
    }
}

void print_generic_params_inline(GenericParams* params);
void print_where_clause_inline(WhereClause* clause);

static void
ast_print_compact(ASTNode* node)
{
    if (node == NULL) {
        printf("(null)");
        return;
    }

    switch (node->type) {
        case AST_IDENTIFIER:
            printf("%s", node->data.identifier.name);
            break;

        case AST_NUMBER:
            printf("%g", node->data.number.value);
            break;

        case AST_STRING:
            print_escaped_string(node->data.string.value);
            break;

        case AST_BOOLEAN:
            printf("%s", node->data.boolean.value ? "true" : "false");
            break;

        case AST_TYPE:
            printf("%s", node->data.type.name);
            if (node->data.type.generic_args)
                print_generic_params_inline(node->data.type.generic_args);
            break;

        case AST_CHANNEL_TYPE:
            printf("Channel<");
            ast_print_compact(node->data.channel_type.element_type);
            printf(">");
            if (node->data.channel_type.capacity != NULL) {
                printf("[");
                ast_print_compact(node->data.channel_type.capacity);
                printf("]");
            }
            break;

        case AST_FUTURE_TYPE:
            printf("Future<");
            ast_print_compact(node->data.future_type.value_type);
            printf(">");
            break;

        case AST_CALL:
            ast_print_compact(node->data.call.callee);
            printf("(");
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (i > 0)
                    printf(", ");
                if (node->data.call.arg_names != NULL
                    && node->data.call.arg_names[i] != NULL)
                    printf("%s: ", node->data.call.arg_names[i]);
                ast_print_compact(node->data.call.arguments[i]);
            }
            printf(")");
            break;

        case AST_BINARY:
            printf("(");
            ast_print_compact(node->data.binary.left);
            printf(" %s ", ast_print_operator_to_string(node->data.binary.op.type));
            ast_print_compact(node->data.binary.right);
            printf(")");
            break;

        case AST_UNARY:
            printf("(%s", ast_print_operator_to_string(node->data.unary.op.type));
            ast_print_compact(node->data.unary.operand);
            printf(")");
            break;

        case AST_MEMBER_ACCESS:
            ast_print_compact(node->data.member.object);
            printf(".%s", node->data.member.name);
            break;

        case AST_ARRAY_ACCESS:
            ast_print_compact(node->data.array_access.array);
            printf("[");
            ast_print_compact(node->data.array_access.index);
            printf("]");
            break;

        case AST_ASSIGNMENT:
            ast_print_compact(node->data.assignment.target);
            printf(" = ");
            ast_print_compact(node->data.assignment.value);
            break;

        case AST_AWAIT_EXPR:
            printf("await ");
            ast_print_compact(node->data.await_expr.expression);
            break;

        case AST_CHANNEL_SEND:
            ast_print_compact(node->data.channel_send.channel);
            printf(" <- ");
            ast_print_compact(node->data.channel_send.value);
            break;

        case AST_CHANNEL_RECV:
            printf("<-");
            ast_print_compact(node->data.channel_recv.channel);
            break;

        case AST_SPAWN_EXPR:
            printf("spawn ");
            ast_print_compact(node->data.spawn_expr.function);
            break;

        case AST_CONTEXT_ACCESS:
            printf("%s(%s",
                   node->data.context_access.method_name,
                   node->data.context_access.role_slot_name);
            if (node->data.context_access.ability_type != NULL) {
                printf(", ");
                ast_print_compact(node->data.context_access.ability_type);
            }
            printf(")");
            break;

        case AST_EVENT_INVOKE:
            ast_print_compact(node->data.event_invoke.event);
            printf("(");
            for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_compact(node->data.event_invoke.arguments[i]);
            }
            printf(")");
            break;

        case AST_EVENT_HANDLER_TYPE:
            printf("func(");
            for (size_t i = 0; i < node->data.event_handler_type.param_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_compact(node->data.event_handler_type.param_types[i]);
            }
            printf(")");
            if (node->data.event_handler_type.return_type != NULL) {
                printf(" -> ");
                ast_print_compact(node->data.event_handler_type.return_type);
            }
            break;

        case AST_REQUIRE_FIELD:
            printf("%s", node->data.require_field.name);
            if (node->data.require_field.type != NULL) {
                printf(": ");
                ast_print_compact(node->data.require_field.type);
            }
            break;

        case AST_ROLE_SLOT:
            printf("%s", node->data.role_slot.slot_name);
            if (node->data.role_slot.is_array)
                printf("[]");
            break;

        case AST_PARTY_SHARED:
            printf("%s", node->data.party_shared.name);
            if (node->data.party_shared.type != NULL) {
                printf(": ");
                ast_print_compact(node->data.party_shared.type);
            }
            if (node->data.party_shared.initializer != NULL) {
                printf(" = ");
                ast_print_compact(node->data.party_shared.initializer);
            }
            break;

        case AST_LET_DESTRUCTURE:
            printf("let (");
            for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
                if (i > 0)
                    printf(", ");
                printf("%s", node->data.let_destructure.names[i] != NULL
                                ? node->data.let_destructure.names[i]
                                : "?");
            }
            printf(")");
            if (node->data.let_destructure.initializer != NULL) {
                printf(" = ");
                ast_print_compact(node->data.let_destructure.initializer);
            }
            break;

        case AST_LET_DECL:
            printf("let %s", node->data.let_decl.name);
            if (node->data.let_decl.type != NULL) {
                printf(": ");
                ast_print_compact(node->data.let_decl.type);
            }
            if (node->data.let_decl.initializer != NULL) {
                printf(" = ");
                ast_print_compact(node->data.let_decl.initializer);
            }
            break;

        case AST_RETURN:
            printf("return");
            if (node->data.return_stmt.value != NULL) {
                printf(" ");
                ast_print_compact(node->data.return_stmt.value);
            }
            break;

        case AST_BLOCK:
            printf("{...}");
            break;

        case AST_PARALLEL_BLOCK:
            printf("parallel {...}");
            break;

        case AST_MATCH_CASE:
            printf("case ");
            if (node->data.match_case.patterns != NULL
                && node->data.match_case.pattern_count > 0) {
                for (size_t i = 0; i < node->data.match_case.pattern_count; i++) {
                    if (i > 0)
                        printf(" | ");
                    ast_print_compact(node->data.match_case.patterns[i]);
                }
            } else {
                ast_print_compact(node->data.match_case.pattern);
            }
            if (node->data.match_case.guard != NULL) {
                printf(" if ");
                ast_print_compact(node->data.match_case.guard);
            }
            break;

        case AST_MATCH_STMT:
            printf("match ");
            ast_print_compact(node->data.match_stmt.subject);
            printf(" {...}");
            break;

        case AST_FOR_LOOP:
            if (node->data.for_loop.label != NULL)
                printf("%s: ", node->data.for_loop.label);
            printf("for %s in ", node->data.for_loop.variable);
            ast_print_compact(node->data.for_loop.range_start);
            printf("..");
            ast_print_compact(node->data.for_loop.range_end);
            break;

        case AST_WHILE_LOOP:
            if (node->data.while_loop.label != NULL)
                printf("%s: ", node->data.while_loop.label);
            printf("while ");
            ast_print_compact(node->data.while_loop.condition);
            break;

        case AST_IF_STMT:
            printf("if ");
            ast_print_compact(node->data.if_stmt.condition);
            break;

        case AST_BREAK:
            printf("break");
            if (node->data.break_stmt.label != NULL)
                printf(" %s", node->data.break_stmt.label);
            break;

        case AST_CONTINUE:
            printf("continue");
            if (node->data.continue_stmt.label != NULL)
                printf(" %s", node->data.continue_stmt.label);
            break;

        case AST_PARTY_INSTANCE:
            printf("%s{...}", node->data.party_instance.party_type);
            break;

        case AST_ARRAY_LITERAL:
            printf("[");
            for (size_t i = 0; i < node->data.array_literal.count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_compact(node->data.array_literal.elements[i]);
            }
            printf("]");
            break;

        case AST_MAP_LITERAL:
            printf("{");
            for (size_t i = 0; i < node->data.map_literal.count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_compact(node->data.map_literal.keys[i]);
                printf(": ");
                ast_print_compact(node->data.map_literal.values[i]);
            }
            printf("}");
            break;

        case AST_CAST:
            ast_print_compact(node->data.cast.operand);
            printf(" as %s", node->data.cast.target_type != NULL
                ? node->data.cast.target_type : "?");
            break;

        case AST_TYPE_TEST:
            ast_print_compact(node->data.type_test.operand);
            printf(" is %s", node->data.type_test.target_type != NULL
                ? node->data.type_test.target_type : "?");
            break;

        case AST_LAMBDA_EXPR:
            printf("%slambda(", node->data.lambda_expr.is_async ? "async " : "");
            for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_compact(node->data.lambda_expr.params[i]);
            }
            printf(")");
            if (node->data.lambda_expr.return_type != NULL) {
                printf(" -> ");
                ast_print_compact(node->data.lambda_expr.return_type);
            }
            break;

        default:
            printf("<node:%d>", node->type);
            break;
    }
}

void
ast_print_inline(ASTNode* node)
{
    ast_print_compact(node);
}

/*
 * Capture the inline/compact rendering of `node` into a malloc'd string,
 * without modifying the printer: temporarily redirect stdout (fd level) to a
 * tmpfile, run the existing printf-based printer, then restore stdout and read
 * the buffer back. This keeps `--ast`/`--tokens` output byte-identical (the
 * printer is untouched) while letting the MIR JSON serializer embed lossless
 * expression text. Caller frees the result.
 */
char *
ast_capture_inline(ASTNode* node)
{
    fflush(stdout);
    int saved = PGY_DUP(PGY_FILENO(stdout));
    if (saved < 0) {
        return NULL;
    }
    FILE *tmp = tmpfile();
    if (tmp == NULL) {
        PGY_CLOSE(saved);
        return NULL;
    }
    fflush(stdout);
    PGY_DUP2(PGY_FILENO(tmp), PGY_FILENO(stdout));
    ast_print_inline(node);
    fflush(stdout);
    PGY_DUP2(saved, PGY_FILENO(stdout));
    PGY_CLOSE(saved);

    long n = ftell(tmp);
    if (n < 0) {
        fclose(tmp);
        return NULL;
    }
    rewind(tmp);
    char *buf = (char *)malloc((size_t)n + 1);
    if (buf == NULL) {
        fclose(tmp);
        return NULL;
    }
    size_t rd = fread(buf, 1, (size_t)n, tmp);
    buf[rd] = '\0';
    fclose(tmp);
    return buf;
}
