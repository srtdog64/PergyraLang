/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend select statement emission.
 */

#include <stdlib.h>
#include <string.h>

#include "transpiler.h"
#include "transpiler_context.h"
#include "transpiler_symbols.h"
#include "../semantic/diag_codes.h"

static bool
select_case_parts(ASTNode *case_node, ASTNode **channel_out,
                  const char **bind_name_out, ASTNode **body_out)
{
    if (case_node == NULL || case_node->type != AST_BLOCK
        || case_node->data.block.count == 0)
        return false;

    ASTNode *first = case_node->data.block.statements[0];
    ASTNode *body = case_node->data.block.count >= 2
        ? case_node->data.block.statements[1] : NULL;

    if (first->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = first->data.channel_recv.channel;
        if (bind_name_out != NULL)
            *bind_name_out = NULL;
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    if (first->type == AST_ASSIGNMENT
        && first->data.assignment.target != NULL
        && first->data.assignment.target->type == AST_IDENTIFIER
        && first->data.assignment.value != NULL
        && first->data.assignment.value->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = first->data.assignment.value->data.channel_recv.channel;
        if (bind_name_out != NULL)
            *bind_name_out = first->data.assignment.target->data.identifier.name;
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    return false;
}

void
emit_select_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    size_t case_count = node->data.select_stmt.case_count;

    codebuf_write(ctx->out, "\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "/* select */\n");

    if (case_count == 0) {
        if (node->data.select_stmt.default_case != NULL)
            emit_statement(node->data.select_stmt.default_case, ctx);
        return;
    }

    for (size_t i = 0; i < case_count; i++) {
        ASTNode *c = node->data.select_stmt.cases[i];
        ASTNode *channel = NULL;
        ASTNode *body = NULL;
        const char *bind_name = NULL;
        bool valid_case = select_case_parts(c, &channel, &bind_name, &body);
        const char *inner = NULL;

        if (!valid_case || bind_name == NULL || channel == NULL
            || channel->type != AST_IDENTIFIER)
            continue;

        {
            const char *type_name = lookup_typed_var(ctx, channel->data.identifier.name);
            if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
                inner = slot_inner_type_name(type_name);
        }
        if (inner == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "cannot derive receive type for select case channel '%s'",
                channel->data.identifier.name != NULL
                    ? channel->data.identifier.name
                    : "(anonymous)");
            return;
        }

        write_indent(ctx);
        codebuf_write(ctx->out, "%s _sel_recv_%zu;\n",
                      pergyra_type_to_c(inner), i);
    }

    {
        int select_id = ctx->tmp_counter++;
        write_indent(ctx);
        codebuf_write(ctx->out, "static unsigned int _sel_rr_%d = 0;\n", select_id);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "unsigned int _sel_start_%d = _sel_rr_%d++ %% %zu;\n",
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
                ASTNode *c = node->data.select_stmt.cases[i];
                ASTNode *channel = NULL;
                ASTNode *body = NULL;
                const char *bind_name = NULL;
                bool valid_case = select_case_parts(c, &channel, &bind_name, &body);
                const char *inner = NULL;

                if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
                    const char *type_name =
                        lookup_typed_var(ctx, channel->data.identifier.name);
                    if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
                        inner = slot_inner_type_name(type_name);
                }
                if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER
                    && inner == NULL) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                        "cannot derive receive type for select case channel '%s'",
                        channel->data.identifier.name != NULL
                            ? channel->data.identifier.name
                            : "(anonymous)");
                    return;
                }

                write_indent(ctx);
                if (offset == 0) {
                    if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
                        if (bind_name != NULL)
                            codebuf_write(ctx->out,
                                "if (pgy_channel_try_recv_%s(&%s, &_sel_recv_%zu)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i, i);
                        else
                            codebuf_write(ctx->out,
                                "if (pgy_channel_ready_%s(&%s)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i);
                    } else {
                        codebuf_write(ctx->out, "if (1) { /* select case %zu */\n", i);
                    }
                } else {
                    if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
                        if (bind_name != NULL)
                            codebuf_write(ctx->out,
                                "} else if (pgy_channel_try_recv_%s(&%s, &_sel_recv_%zu)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i, i);
                        else
                            codebuf_write(ctx->out,
                                "} else if (pgy_channel_ready_%s(&%s)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i);
                    } else {
                        codebuf_write(ctx->out, "} else if (1) { /* select case %zu */\n", i);
                    }
                }
                ctx->indent++;
                if (valid_case) {
                    if (bind_name != NULL) {
                        write_indent(ctx);
                        codebuf_write(ctx->out, "%s %s = _sel_recv_%zu;\n",
                                      pergyra_type_to_c(inner), bind_name, i);
                    } else if (channel != NULL) {
                        char *recv = emit_channel_recv(ast_create_channel_recv(channel), ctx);
                        write_indent(ctx);
                        codebuf_write(ctx->out, "(void)%s;\n", recv);
                        free(recv);
                    }
                    if (body != NULL)
                        emit_statement(body, ctx);
                } else if (c != NULL) {
                    emit_statement(c, ctx);
                }
                ctx->indent--;
            }

            if (node->data.select_stmt.default_case != NULL) {
                write_indent(ctx);
                codebuf_write(ctx->out, "} else { /* default */\n");
                ctx->indent++;
                emit_statement(node->data.select_stmt.default_case, ctx);
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
