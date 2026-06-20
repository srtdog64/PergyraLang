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

static const char *
domain_slot_view_type_name(const TranspilerHostedDomainSlotView *slot_view,
                           size_t index)
{
    ASTNode *slot_type;
    const char *type_name =
        transpiler_hosted_domain_slot_view_type_name(slot_view, index);

    if (type_name != NULL)
        return type_name;

    slot_type = transpiler_hosted_domain_slot_view_type(slot_view, index);
    if (slot_type == NULL || slot_type->type != AST_TYPE)
        return NULL;
    return ast_type_name(slot_type);
}

static bool
domain_slot_view_find_index(const TranspilerHostedDomainSlotView *slot_view,
                            const char *slot_name,
                            size_t *index_out)
{
    if (slot_view == NULL || slot_name == NULL)
        return false;

    for (size_t i = 0; i < slot_view->count; i++) {
        const char *candidate =
            transpiler_hosted_domain_slot_view_name(slot_view, i);
        if (candidate != NULL && strcmp(candidate, slot_name) == 0) {
            if (index_out != NULL)
                *index_out = i;
            return true;
        }
    }
    return false;
}

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
emit_zone_projection_sync_loop_from_mir_refresh_view(
    TranspilerCtx *ctx,
    const TranspilerHostedDomainSlotView *slot_view,
    const TranspilerHostedZoneRefreshView *refresh_view,
    const char *loop_prefix,
    bool early_return_if_clean)
{
    bool emitted_condition = false;
    size_t refresh_count;

    if (ctx == NULL || ctx->out == NULL || slot_view == NULL
        || refresh_view == NULL || loop_prefix == NULL
        || refresh_view->count == 0) {
        return;
    }

    refresh_count = refresh_view->count;

    for (size_t i = 0; i < slot_view->count; i++) {
        const char *slot_name =
            transpiler_hosted_domain_slot_view_name(slot_view, i);
        if (slot_name == NULL
            || transpiler_hosted_domain_slot_view_is_subject_like(
                slot_view, i)) {
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
    for (size_t i = 0; i < slot_view->count; i++) {
        const char *slot_name =
            transpiler_hosted_domain_slot_view_name(slot_view, i);
        if (slot_name == NULL
            || transpiler_hosted_domain_slot_view_is_subject_like(
                slot_view, i)) {
            continue;
        }
        if (emitted_condition)
            codebuf_write(ctx->out, " || ");
        codebuf_write(ctx->out, "self->__projection_dirty_%s", slot_name);
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
        const MIRDeclZoneRefresh *refresh =
            transpiler_hosted_zone_refresh_view_metadata(refresh_view, i);
        const char *target_slot_name;
        const char *source_slot_name;
        size_t target_index = 0;
        size_t source_index = 0;
        const char *target_type_name = NULL;
        const char *source_type_name = NULL;
        char *literal;

        if (refresh == NULL)
            continue;

        target_slot_name =
            transpiler_hosted_zone_refresh_view_object_slot_name(
                refresh_view, i);
        source_slot_name =
            transpiler_hosted_zone_refresh_view_source_slot_name(
                refresh_view, i);
        if (target_slot_name == NULL || source_slot_name == NULL)
            continue;
        if (!domain_slot_view_find_index(slot_view, target_slot_name,
                &target_index)
            || !domain_slot_view_find_index(slot_view, source_slot_name,
                &source_index)) {
            continue;
        }

        target_type_name =
            domain_slot_view_type_name(slot_view, target_index);
        source_type_name =
            domain_slot_view_type_name(slot_view, source_index);
        if (target_type_name == NULL || source_type_name == NULL)
            continue;

        {
            const char *source_expr = transpiler_scratch_fmt(ctx, "self->%s",
                source_slot_name);
            literal = emit_projection_literal_by_zone_refresh_metadata(
                ctx, target_type_name, source_type_name, refresh, source_expr);
        }

        write_indent(ctx);
        codebuf_write(ctx->out, "if (self->__projection_dirty_%s) {\n",
            target_slot_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_ready_%s = false;\n",
            target_slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->%s = %s;\n",
            target_slot_name, literal);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_ready_%s = true;\n",
            target_slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_dirty_%s = false;\n",
            target_slot_name);
        emit_hidden_provenance_stamp(ctx, "self", "projection",
            target_slot_name, PGY_PROP_CAUSE_REFRESH);

        for (size_t dep_i = 0; dep_i < refresh_count; dep_i++) {
            const char *dependent_target =
                transpiler_hosted_zone_refresh_view_object_slot_name(
                    refresh_view, dep_i);
            const char *dependent_source =
                transpiler_hosted_zone_refresh_view_source_slot_name(
                    refresh_view, dep_i);

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
