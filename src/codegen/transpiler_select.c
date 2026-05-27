/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend select statement emission.
 */

#include <stdlib.h>
#include <string.h>

#include "transpiler.h"
#include "transpiler_channel_type_query.h"
#include "transpiler_context.h"
#include "transpiler_type_require.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

static bool
select_case_parts(ASTNode *case_node, ASTNode **channel_out,
                  const char **bind_name_out, ASTNode **body_out)
{
    if (case_node == NULL || case_node->type != AST_BLOCK
        || ast_block_statement_count(case_node) == 0)
        return false;

    ASTNode *first = ast_block_statement(case_node, 0);
    ASTNode *body = ast_block_statement_count(case_node) >= 2
        ? ast_block_statement(case_node, 1) : NULL;

    if (first->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = ast_channel_recv_channel(first);
        if (bind_name_out != NULL)
            *bind_name_out = NULL;
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    if (first->type == AST_ASSIGNMENT
        && ast_assignment_target(first) != NULL
        && ast_assignment_target(first)->type == AST_IDENTIFIER
        && ast_assignment_value(first) != NULL
        && ast_assignment_value(first)->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = ast_channel_recv_channel(ast_assignment_value(first));
        if (bind_name_out != NULL)
            *bind_name_out = ast_identifier_name(ast_assignment_target(first));
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    return false;
}

static bool
select_channel_inner_type_copy(ASTNode *channel, TranspilerCtx *ctx,
                               char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (channel == NULL || channel->type != AST_IDENTIFIER)
        return false;
    return channel_inner_type_name_copy(ctx, channel, out, out_size)
        && out[0] != '\0'
        && strcmp(out, "Unknown") != 0;
}

static void
select_set_missing_channel_type_error(TranspilerCtx *ctx, ASTNode *channel)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "cannot derive receive type for select case channel '%s'",
        channel != NULL && channel->type == AST_IDENTIFIER
            && ast_identifier_name(channel) != NULL
                ? ast_identifier_name(channel)
                : "(anonymous)");
}

static void
select_write_case_guard(TranspilerCtx *ctx, size_t offset, size_t index,
                        ASTNode *channel, const char *bind_name,
                        const char *inner)
{
    const char *prefix = offset == 0 ? "if" : "} else if";
    const char *channel_name = channel != NULL && channel->type == AST_IDENTIFIER
        ? ast_identifier_name(channel)
        : NULL;

    if (channel_name == NULL) {
        codebuf_write(ctx->out, "%s (1) { /* select case %zu */\n",
                      prefix, index);
        return;
    }

    if (bind_name != NULL) {
        codebuf_write(ctx->out,
            "%s (pgy_channel_try_recv_%s(&%s, &_sel_recv_%zu)) { /* select case %zu */\n",
            prefix, inner, channel_name, index, index);
        return;
    }

    codebuf_write(ctx->out,
        "%s (pgy_channel_ready_%s(&%s)) { /* select case %zu */\n",
        prefix, inner, channel_name, index);
}

static void
select_emit_unbound_consume(ASTNode *channel, const char *inner,
                            TranspilerCtx *ctx)
{
    char *channel_expr = emit_expression(channel, ctx);
    write_indent(ctx);
    codebuf_write(ctx->out, "(void)pgy_channel_recv_val_%s(&%s);\n",
                  inner, channel_expr);
    free(channel_expr);
}

void
emit_select_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    size_t case_count = ast_select_case_count(node);

    codebuf_write(ctx->out, "\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "/* select */\n");

    if (case_count == 0) {
        if (ast_select_default_case(node) != NULL)
            emit_statement(ast_select_default_case(node), ctx);
        return;
    }

    for (size_t i = 0; i < case_count; i++) {
        ASTNode *c = ast_select_case(node, i);
        ASTNode *channel = NULL;
        ASTNode *body = NULL;
        const char *bind_name = NULL;
        bool valid_case = select_case_parts(c, &channel, &bind_name, &body);
        char inner_buf[128];
        const char *inner = inner_buf;

        if (!valid_case || bind_name == NULL || channel == NULL
            || channel->type != AST_IDENTIFIER)
            continue;

        if (!select_channel_inner_type_copy(channel, ctx, inner_buf,
                sizeof(inner_buf))) {
            select_set_missing_channel_type_error(ctx, channel);
            return;
        }

        {
            char inner_c_type_buf[256];
            const char *inner_c_type = NULL;
            if (transpiler_require_type_name_c_type_copy(ctx, inner,
                    "select receive payload", inner_c_type_buf,
                    sizeof(inner_c_type_buf))) {
                inner_c_type = inner_c_type_buf;
            }
            if (inner_c_type == NULL) {
                select_set_missing_channel_type_error(ctx, channel);
                return;
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "%s _sel_recv_%zu;\n",
                          inner_c_type, i);
        }
    }

    {
        int select_id = ctx->tmp_counter++;
        write_indent(ctx);
        codebuf_write(ctx->out,
                      "static _Atomic unsigned int _sel_rr_%d = 0;\n",
                      select_id);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "unsigned int _sel_start_%d = "
            "atomic_fetch_add_explicit(&_sel_rr_%d, 1u, "
            "memory_order_relaxed) %% %zu;\n",
            select_id, select_id, case_count);
        write_indent(ctx);
        codebuf_write(ctx->out, "switch (_sel_start_%d) {\n", select_id);
        ctx->indent++;

        for (size_t start = 0; start < case_count; start++) {
            write_indent(ctx);
            codebuf_write(ctx->out, "case %zu:\n", start);
            ctx->indent++;

            for (size_t offset = 0; offset < case_count; offset++) {
                size_t i = (start + offset) % case_count;
                ASTNode *c = ast_select_case(node, i);
                ASTNode *channel = NULL;
                ASTNode *body = NULL;
                const char *bind_name = NULL;
                bool valid_case = select_case_parts(c, &channel, &bind_name, &body);
                char inner_buf[128];
                const char *inner = inner_buf;

                if (!select_channel_inner_type_copy(channel, ctx, inner_buf,
                        sizeof(inner_buf))) {
                    inner = NULL;
                }
                if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER
                    && inner == NULL) {
                    select_set_missing_channel_type_error(ctx, channel);
                    return;
                }

                write_indent(ctx);
                select_write_case_guard(ctx, offset, i, channel,
                                        valid_case ? bind_name : NULL, inner);
                ctx->indent++;
                if (valid_case) {
                    if (bind_name != NULL) {
                        char inner_c_type_buf[256];
                        const char *inner_c_type = NULL;
                        if (transpiler_require_type_name_c_type_copy(ctx,
                                inner, "select bound receive payload",
                                inner_c_type_buf, sizeof(inner_c_type_buf))) {
                            inner_c_type = inner_c_type_buf;
                        }
                        if (inner_c_type == NULL) {
                            select_set_missing_channel_type_error(ctx, channel);
                            return;
                        }
                        write_indent(ctx);
                        codebuf_write(ctx->out, "%s %s = _sel_recv_%zu;\n",
                                      inner_c_type, bind_name, i);
                    } else if (channel != NULL) {
                        if (inner == NULL) {
                            select_set_missing_channel_type_error(ctx, channel);
                            return;
                        }
                        select_emit_unbound_consume(channel, inner, ctx);
                    }
                    if (body != NULL)
                        emit_statement(body, ctx);
                } else if (c != NULL) {
                    emit_statement(c, ctx);
                }
                ctx->indent--;
            }

            if (ast_select_default_case(node) != NULL) {
                write_indent(ctx);
                codebuf_write(ctx->out, "} else { /* default */\n");
                ctx->indent++;
                emit_statement(ast_select_default_case(node), ctx);
                ctx->indent--;
            }

            write_indent(ctx);
            codebuf_write(ctx->out, "}\n");
            write_indent(ctx);
            codebuf_write(ctx->out, "break;\n");
            ctx->indent--;
        }

        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}
