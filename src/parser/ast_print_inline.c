/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST compact/inline print helpers.
 */

#include "ast_print_internal.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Every byte of the inline printer flows through one sink: stdout for
 * ast_print_inline (same format strings, byte-identical output), a
 * growing malloc buffer for ast_capture_inline. The previous capture
 * route left the printf printer untouched and captured it by dup2-ing
 * stdout into a FRESH tmpfile() per call -- a disk temp file plus two
 * fd swaps per MIR instruction, which was the dominant wallclock of
 * mir_lower on Windows (docs/183: 7.5s of an 11s selfhost compile).
 * The printer was already stdout-global, so a file-static sink keeps
 * the same non-reentrant contract without threading a context through
 * the recursion. */
typedef struct {
    bool to_buffer;
    bool failed;
    char *buf;
    size_t len;
    size_t cap;
} InlineSink;

static InlineSink g_inline_sink;

static bool
inline_sink_reserve(size_t extra)
{
    if (g_inline_sink.len + extra + 1 <= g_inline_sink.cap)
        return true;
    size_t next = g_inline_sink.cap == 0 ? 64 : g_inline_sink.cap;
    while (next < g_inline_sink.len + extra + 1) {
        if (next > SIZE_MAX / 2) {
            g_inline_sink.failed = true;
            return false;
        }
        next *= 2;
    }
    char *grown = realloc(g_inline_sink.buf, next);
    if (grown == NULL) {
        g_inline_sink.failed = true;
        return false;
    }
    g_inline_sink.buf = grown;
    g_inline_sink.cap = next;
    return true;
}

static void
inline_emitf(const char *fmt, ...)
{
    va_list args;

    if (!g_inline_sink.to_buffer) {
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        return;
    }
    if (g_inline_sink.failed)
        return;
    va_start(args, fmt);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (needed < 0) {
        g_inline_sink.failed = true;
        return;
    }
    if (!inline_sink_reserve((size_t)needed))
        return;
    va_start(args, fmt);
    vsnprintf(g_inline_sink.buf + g_inline_sink.len, (size_t)needed + 1,
              fmt, args);
    va_end(args);
    g_inline_sink.len += (size_t)needed;
}

static void
inline_emitc(int ch)
{
    if (!g_inline_sink.to_buffer) {
        putchar(ch);
        return;
    }
    if (g_inline_sink.failed || !inline_sink_reserve(1))
        return;
    g_inline_sink.buf[g_inline_sink.len++] = (char)ch;
    g_inline_sink.buf[g_inline_sink.len] = '\0';
}

static void
print_escaped_string(const char *value)
{
    const unsigned char *p = (const unsigned char *)value;

    inline_emitf("\"");
    if (p == NULL) {
        inline_emitf("\"");
        return;
    }

    while (*p != '\0') {
        switch (*p) {
        case '\\':
            inline_emitf("\\\\");
            break;
        case '"':
            inline_emitf("\\\"");
            break;
        case '\n':
            inline_emitf("\\n");
            break;
        case '\r':
            inline_emitf("\\r");
            break;
        case '\t':
            inline_emitf("\\t");
            break;
        default:
            inline_emitc((int)*p);
            break;
        }
        p++;
    }
    inline_emitf("\"");
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

/* Generic-parameter and where-clause inline printers live in this file
 * so they flow through the same sink: the indented dump printers call
 * them too (stdout mode), and the capture path must not let their bytes
 * escape to the real stdout mid-buffer. */
void
print_generic_params_inline(GenericParams* params)
{
    if (params == NULL || params->count == 0) {
        return;
    }

    inline_emitf("<");
    for (size_t i = 0; i < params->count; i++) {
        GenericParam* param = params->params[i];
        if (i > 0)
            inline_emitf(", ");
        if (param == NULL) {
            inline_emitf("?");
            continue;
        }
        inline_emitf("%s", param->name != NULL ? param->name : "?");
        if (param->constraint != NULL) {
            inline_emitf(": ");
            ast_print_inline(param->constraint);
        }
        if (param->default_type != NULL) {
            inline_emitf(" = ");
            ast_print_inline(param->default_type);
        }
    }
    inline_emitf(">");
}

static void
print_call_type_arguments_inline(GenericParams *params)
{
    if (params == NULL || params->count == 0)
        return;

    inline_emitf("<");
    for (size_t i = 0; i < params->count; i++) {
        GenericParam *param = params->params[i];
        if (i > 0)
            inline_emitf(", ");
        if (param == NULL) {
            inline_emitf("?");
        } else if (param->constraint != NULL) {
            ast_print_inline(param->constraint);
        } else {
            inline_emitf("%s", param->name != NULL ? param->name : "?");
        }
    }
    inline_emitf(">");
}

void
print_where_clause_inline(WhereClause* clause)
{
    if (clause == NULL || clause->count == 0)
        return;

    inline_emitf(" where ");
    for (size_t i = 0; i < clause->count; i++) {
        TypeConstraint* constraint = clause->constraints[i];
        if (i > 0)
            inline_emitf(", ");
        if (constraint == NULL) {
            inline_emitf("?");
            continue;
        }

        inline_emitf("%s", constraint->type_param != NULL ? constraint->type_param : "?");
        if (constraint->bound_count > 0) {
            inline_emitf(": ");
            for (size_t j = 0; j < constraint->bound_count; j++) {
                if (j > 0)
                    inline_emitf(" + ");
                ast_print_inline(constraint->bounds[j]);
            }
        }
    }
}

static void
ast_print_compact(ASTNode* node)
{
    if (node == NULL) {
        inline_emitf("(null)");
        return;
    }

    switch (node->type) {
        case AST_IDENTIFIER:
            inline_emitf("%s", node->data.identifier.name);
            break;

        case AST_NUMBER:
            inline_emitf("%g", node->data.number.value);
            break;

        case AST_STRING:
            print_escaped_string(node->data.string.value);
            break;

        case AST_BOOLEAN:
            inline_emitf("%s", node->data.boolean.value ? "true" : "false");
            break;

        case AST_TYPE:
            inline_emitf("%s", node->data.type.name);
            if (node->data.type.generic_args)
                print_generic_params_inline(node->data.type.generic_args);
            break;

        case AST_CHANNEL_TYPE:
            inline_emitf("Channel<");
            ast_print_compact(node->data.channel_type.element_type);
            inline_emitf(">");
            if (node->data.channel_type.capacity != NULL) {
                inline_emitf("[");
                ast_print_compact(node->data.channel_type.capacity);
                inline_emitf("]");
            }
            break;

        case AST_FUTURE_TYPE:
            inline_emitf("Future<");
            ast_print_compact(node->data.future_type.value_type);
            inline_emitf(">");
            break;

        case AST_CALL:
            ast_print_compact(node->data.call.callee);
            print_call_type_arguments_inline(node->data.call.generic_args);
            inline_emitf(ast_call_uses_braced_initializer_syntax(node)
                ? " { " : "(");
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (i > 0)
                    inline_emitf(", ");
                if (node->data.call.arg_names != NULL
                    && node->data.call.arg_names[i] != NULL)
                    inline_emitf("%s: ", node->data.call.arg_names[i]);
                ast_print_compact(node->data.call.arguments[i]);
            }
            inline_emitf(ast_call_uses_braced_initializer_syntax(node)
                ? " }" : ")");
            break;

        case AST_BINARY:
            inline_emitf("(");
            ast_print_compact(node->data.binary.left);
            inline_emitf(" %s ", ast_print_operator_to_string(node->data.binary.op.type));
            ast_print_compact(node->data.binary.right);
            inline_emitf(")");
            break;

        case AST_UNARY:
            inline_emitf("(%s", ast_print_operator_to_string(node->data.unary.op.type));
            ast_print_compact(node->data.unary.operand);
            inline_emitf(")");
            break;

        case AST_MEMBER_ACCESS:
            ast_print_compact(node->data.member.object);
            inline_emitf(".%s", node->data.member.name);
            break;

        case AST_ARRAY_ACCESS:
            ast_print_compact(node->data.array_access.array);
            inline_emitf("[");
            ast_print_compact(node->data.array_access.index);
            inline_emitf("]");
            break;

        case AST_ASSIGNMENT:
            ast_print_compact(node->data.assignment.target);
            inline_emitf(" = ");
            ast_print_compact(node->data.assignment.value);
            break;

        case AST_AWAIT_EXPR:
            inline_emitf("await ");
            ast_print_compact(node->data.await_expr.expression);
            break;

        case AST_CHANNEL_SEND:
            ast_print_compact(node->data.channel_send.channel);
            inline_emitf(" <- ");
            ast_print_compact(node->data.channel_send.value);
            break;

        case AST_CHANNEL_RECV:
            inline_emitf("<-");
            ast_print_compact(node->data.channel_recv.channel);
            break;

        case AST_SPAWN_EXPR:
            inline_emitf("spawn ");
            ast_print_compact(node->data.spawn_expr.function);
            break;

        case AST_CONTEXT_ACCESS:
            inline_emitf("%s(%s",
                   node->data.context_access.method_name,
                   node->data.context_access.role_slot_name);
            if (node->data.context_access.ability_type != NULL) {
                inline_emitf(", ");
                ast_print_compact(node->data.context_access.ability_type);
            }
            inline_emitf(")");
            break;

        case AST_EVENT_INVOKE:
            ast_print_compact(node->data.event_invoke.event);
            inline_emitf("(");
            for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
                if (i > 0)
                    inline_emitf(", ");
                ast_print_compact(node->data.event_invoke.arguments[i]);
            }
            inline_emitf(")");
            break;

        case AST_EVENT_HANDLER_TYPE:
            inline_emitf("func(");
            for (size_t i = 0; i < node->data.event_handler_type.param_count; i++) {
                if (i > 0)
                    inline_emitf(", ");
                ast_print_compact(node->data.event_handler_type.param_types[i]);
            }
            inline_emitf(")");
            if (node->data.event_handler_type.return_type != NULL) {
                inline_emitf(" -> ");
                ast_print_compact(node->data.event_handler_type.return_type);
            }
            break;

        case AST_REQUIRE_FIELD:
            inline_emitf("%s", node->data.require_field.name);
            if (node->data.require_field.type != NULL) {
                inline_emitf(": ");
                ast_print_compact(node->data.require_field.type);
            }
            break;

        case AST_ROLE_SLOT:
            inline_emitf("%s", node->data.role_slot.slot_name);
            if (node->data.role_slot.is_array)
                inline_emitf("[]");
            break;

        case AST_PARTY_SHARED:
            inline_emitf("%s", node->data.party_shared.name);
            if (node->data.party_shared.type != NULL) {
                inline_emitf(": ");
                ast_print_compact(node->data.party_shared.type);
            }
            if (node->data.party_shared.initializer != NULL) {
                inline_emitf(" = ");
                ast_print_compact(node->data.party_shared.initializer);
            }
            break;

        case AST_LET_DESTRUCTURE:
            inline_emitf("let (");
            for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
                if (i > 0)
                    inline_emitf(", ");
                inline_emitf("%s", node->data.let_destructure.names[i] != NULL
                                ? node->data.let_destructure.names[i]
                                : "?");
            }
            inline_emitf(")");
            if (node->data.let_destructure.initializer != NULL) {
                inline_emitf(" = ");
                ast_print_compact(node->data.let_destructure.initializer);
            }
            break;

        case AST_LET_DECL:
            inline_emitf("let %s", node->data.let_decl.name);
            if (node->data.let_decl.type != NULL) {
                inline_emitf(": ");
                ast_print_compact(node->data.let_decl.type);
            }
            if (node->data.let_decl.initializer != NULL) {
                inline_emitf(" = ");
                ast_print_compact(node->data.let_decl.initializer);
            }
            break;

        case AST_RETURN:
            inline_emitf("return");
            if (node->data.return_stmt.value != NULL) {
                inline_emitf(" ");
                ast_print_compact(node->data.return_stmt.value);
            }
            break;

        case AST_GIVE_STMT:
            inline_emitf("give ");
            ast_print_compact(node->data.give_stmt.value);
            break;

        case AST_BLOCK:
            inline_emitf("{...}");
            break;

        case AST_PARALLEL_BLOCK:
            inline_emitf("parallel {...}");
            break;

        case AST_MATCH_CASE:
            inline_emitf("case ");
            if (node->data.match_case.patterns != NULL
                && node->data.match_case.pattern_count > 0) {
                for (size_t i = 0; i < node->data.match_case.pattern_count; i++) {
                    if (i > 0)
                        inline_emitf(" | ");
                    ast_print_compact(node->data.match_case.patterns[i]);
                }
            } else {
                ast_print_compact(node->data.match_case.pattern);
            }
            if (node->data.match_case.guard != NULL) {
                inline_emitf(" if ");
                ast_print_compact(node->data.match_case.guard);
            }
            break;

        case AST_MATCH_STMT:
            inline_emitf("match ");
            ast_print_compact(node->data.match_stmt.subject);
            inline_emitf(" {...}");
            break;

        case AST_FOR_LOOP:
            if (node->data.for_loop.label != NULL)
                inline_emitf("%s: ", node->data.for_loop.label);
            inline_emitf("for %s in ", node->data.for_loop.variable);
            ast_print_compact(node->data.for_loop.range_start);
            inline_emitf("..");
            ast_print_compact(node->data.for_loop.range_end);
            break;

        case AST_WHILE_LOOP:
            if (node->data.while_loop.label != NULL)
                inline_emitf("%s: ", node->data.while_loop.label);
            inline_emitf("while ");
            ast_print_compact(node->data.while_loop.condition);
            break;

        case AST_IF_STMT:
            inline_emitf("if ");
            ast_print_compact(node->data.if_stmt.condition);
            break;

        case AST_BREAK:
            inline_emitf("break");
            if (node->data.break_stmt.label != NULL)
                inline_emitf(" %s", node->data.break_stmt.label);
            break;

        case AST_CONTINUE:
            inline_emitf("continue");
            if (node->data.continue_stmt.label != NULL)
                inline_emitf(" %s", node->data.continue_stmt.label);
            break;

        case AST_PARTY_INSTANCE:
            inline_emitf("%s{...}", node->data.party_instance.party_type);
            break;

        case AST_ARRAY_LITERAL:
            inline_emitf("[");
            for (size_t i = 0; i < node->data.array_literal.count; i++) {
                if (i > 0)
                    inline_emitf(", ");
                ast_print_compact(node->data.array_literal.elements[i]);
            }
            inline_emitf("]");
            break;

        case AST_MAP_LITERAL:
            if (node->data.map_literal.count == 0) {
                inline_emitf("{:}");
                break;
            }
            inline_emitf("{");
            for (size_t i = 0; i < node->data.map_literal.count; i++) {
                if (i > 0)
                    inline_emitf(", ");
                ast_print_compact(node->data.map_literal.keys[i]);
                inline_emitf(": ");
                ast_print_compact(node->data.map_literal.values[i]);
            }
            inline_emitf("}");
            break;

        case AST_SET_LITERAL:
            inline_emitf("{");
            for (size_t i = 0; i < node->data.set_literal.count; i++) {
                if (i > 0)
                    inline_emitf(", ");
                ast_print_compact(node->data.set_literal.elements[i]);
            }
            inline_emitf("}");
            break;

        case AST_CAST:
            ast_print_compact(node->data.cast.operand);
            inline_emitf(" as %s", node->data.cast.target_type != NULL
                ? node->data.cast.target_type : "?");
            break;

        case AST_TYPE_TEST:
            ast_print_compact(node->data.type_test.operand);
            inline_emitf(" is %s", node->data.type_test.target_type != NULL
                ? node->data.type_test.target_type : "?");
            break;

        case AST_LAMBDA_EXPR:
            inline_emitf("%slambda(", node->data.lambda_expr.is_async ? "async " : "");
            for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
                if (i > 0)
                    inline_emitf(", ");
                ast_print_compact(node->data.lambda_expr.params[i]);
            }
            inline_emitf(")");
            if (node->data.lambda_expr.return_type != NULL) {
                inline_emitf(" -> ");
                ast_print_compact(node->data.lambda_expr.return_type);
            }
            break;

        default:
            inline_emitf("<node:%d>", node->type);
            break;
    }
}

void
ast_print_inline(ASTNode* node)
{
    ast_print_compact(node);
}

/*
 * Capture the inline/compact rendering of `node` into a malloc'd string.
 * Same printer, same format strings as ast_print_inline -- the sink just
 * points at a malloc buffer instead of stdout. Caller frees the result.
 * Returns "" for a render that emits nothing and NULL only on OOM, the
 * same contract as the old tmpfile-based capture.
 */
char *
ast_capture_inline(ASTNode* node)
{
    InlineSink saved = g_inline_sink;
    char *result;

    memset(&g_inline_sink, 0, sizeof(g_inline_sink));
    g_inline_sink.to_buffer = true;
    ast_print_compact(node);
    if (g_inline_sink.failed) {
        free(g_inline_sink.buf);
        result = NULL;
    } else if (g_inline_sink.buf == NULL) {
        result = calloc(1, 1);
    } else {
        result = g_inline_sink.buf;
    }
    g_inline_sink = saved;
    return result;
}
