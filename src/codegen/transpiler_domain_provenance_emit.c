/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend domain propagation provenance and projection recompute helpers.
 */

#include "transpiler_domain_provenance_emit.h"

#include <stdlib.h>
#include <string.h>

#include "domain_frontier_policy.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_projection.h"

void
emit_hidden_provenance_fields(TranspilerCtx *ctx,
                              const char *prefix,
                              const char *name)
{
    if (ctx == NULL || ctx->out == NULL || prefix == NULL || name == NULL)
        return;

    codebuf_write(ctx->out, "    uint32_t __%s_epoch_%s;\n", prefix, name);
    codebuf_write(ctx->out, "    int __%s_cause_%s;\n", prefix, name);
}

void
emit_hidden_provenance_stamp(TranspilerCtx *ctx,
                             const char *self_expr,
                             const char *prefix,
                             const char *name,
                             int cause)
{
    if (ctx == NULL || self_expr == NULL || prefix == NULL || name == NULL)
        return;

    write_indent(ctx);
    codebuf_write(ctx->out, "%s->__%s_epoch_%s++;\n", self_expr, prefix, name);
    write_indent(ctx);
    codebuf_write(ctx->out, "%s->__%s_cause_%s = %d;\n",
        self_expr, prefix, name, cause);
}

void
emit_domain_projection_sync_loop(TranspilerCtx *ctx,
                                 ASTNode **slots,
                                 size_t slot_count,
                                 ASTNode **refreshes,
                                 size_t refresh_count,
                                 const char *loop_prefix,
                                 bool early_return_if_clean)
{
    bool emitted_condition = false;

    if (ctx == NULL || ctx->out == NULL || slots == NULL || refreshes == NULL
        || loop_prefix == NULL || refresh_count == 0) {
        return;
    }

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *slot_name = ast_domain_slot_name(slot);
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || ast_domain_slot_is_subject(slot)
            || slot_name == NULL) {
            continue;
        }
        emitted_condition = true;
        break;
    }
    if (!emitted_condition)
        return;

    emitted_condition = false;
    write_indent(ctx);
    codebuf_write(ctx->out, "if (");
    if (early_return_if_clean)
        codebuf_write(ctx->out, "!(");
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *slot_name = ast_domain_slot_name(slot);
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || ast_domain_slot_is_subject(slot)
            || slot_name == NULL) {
            continue;
        }
        if (emitted_condition)
            codebuf_write(ctx->out, " || ");
        codebuf_write(ctx->out, "self->__projection_dirty_%s",
            slot_name);
        emitted_condition = true;
    }
    if (early_return_if_clean)
        codebuf_write(ctx->out, ")) return;\n");
    else
        codebuf_write(ctx->out, ") {\n");

    if (!early_return_if_clean)
        ctx->indent++;

    write_indent(ctx);
    codebuf_write(ctx->out, "size_t _pgy_%s_pass = 0;\n", loop_prefix);
    write_indent(ctx);
    codebuf_write(ctx->out, "size_t _pgy_%s_pass_limit = %zu;\n",
        loop_prefix, pgy_domain_projection_frontier_pass_limit(refresh_count));
    write_indent(ctx);
    codebuf_write(ctx->out, "bool _pgy_%s_continue = true;\n", loop_prefix);
    write_indent(ctx);
    codebuf_write(ctx->out,
        "while (_pgy_%s_continue && _pgy_%s_pass < _pgy_%s_pass_limit) {\n",
        loop_prefix, loop_prefix, loop_prefix);
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_%s_continue = false;\n", loop_prefix);
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_%s_pass++;\n", loop_prefix);

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        ASTNode *target_slot = NULL;
        ASTNode *source_slot = NULL;
        ASTNode *target_decl = NULL;
        ASTNode *source_decl = NULL;
        const char *target_type_name = NULL;
        const char *source_type_name = NULL;
        const char *target_slot_name;
        char *literal;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;

        target_slot_name = ast_zone_refresh_object_slot_name(refresh);
        if (target_slot_name == NULL
            || ast_zone_refresh_source_slot_name(refresh) == NULL) {
            continue;
        }

        for (size_t j = 0; j < slot_count; j++) {
            ASTNode *slot = slots[j];
            const char *slot_name = ast_domain_slot_name(slot);
            if (slot == NULL || slot->type != AST_DOMAIN_SLOT
                || slot_name == NULL) {
                continue;
            }
            if (strcmp(slot_name, target_slot_name) == 0)
                target_slot = slot;
            if (strcmp(slot_name,
                       ast_zone_refresh_source_slot_name(refresh)) == 0) {
                source_slot = slot;
            }
        }
        ASTNode *target_slot_type = ast_domain_slot_type(target_slot);
        ASTNode *source_slot_type = ast_domain_slot_type(source_slot);
        if (target_slot == NULL || source_slot == NULL
            || target_slot_type == NULL
            || source_slot_type == NULL
            || target_slot_type->type != AST_TYPE
            || source_slot_type->type != AST_TYPE) {
            continue;
        }

        target_type_name = ast_type_name(target_slot_type);
        source_type_name = ast_type_name(source_slot_type);
        target_decl = find_class_decl(ctx, target_type_name);
        source_decl = find_class_decl(ctx, source_type_name);
        {
            const char *source_expr = transpiler_scratch_fmt(ctx, "self->%s",
                ast_zone_refresh_source_slot_name(refresh));
            literal = emit_projection_literal(ctx, target_decl, source_decl,
                refresh, target_type_name, source_expr);
        }

        write_indent(ctx);
        codebuf_write(ctx->out, "if (self->__projection_dirty_%s) {\n",
            target_slot_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_ready_%s = false;\n",
            target_slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->%s = %s;\n", target_slot_name, literal);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_ready_%s = true;\n",
            target_slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_dirty_%s = false;\n",
            target_slot_name);
        emit_hidden_provenance_stamp(ctx, "self", "projection",
            target_slot_name, PGY_PROP_CAUSE_REFRESH);

        for (size_t dep_i = 0; dep_i < refresh_count; dep_i++) {
            ASTNode *dependent = refreshes[dep_i];
            const char *dependent_target;
            const char *dependent_source;

            if (dependent == NULL || dependent->type != AST_ZONE_REFRESH)
                continue;
            dependent_target = ast_zone_refresh_object_slot_name(dependent);
            dependent_source = ast_zone_refresh_source_slot_name(dependent);
            if (dependent_target == NULL || dependent_source == NULL
                || strcmp(dependent_source, target_slot_name) != 0) {
                continue;
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__projection_dirty_%s = true;\n",
                dependent_target);
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__projection_ready_%s = false;\n",
                dependent_target);
            write_indent(ctx);
            codebuf_write(ctx->out, "_pgy_%s_continue = true;\n", loop_prefix);
        }

        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
        free(literal);
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "if (_pgy_%s_continue) {\n", loop_prefix);
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out,
        "PGY_PANIC(\"%s\");\n", PGY_FRONTIER_REASON_PROJECTION_OVERFLOW);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    if (!early_return_if_clean) {
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}
