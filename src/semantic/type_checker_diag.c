/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker diagnostics — owned diagnostic snapshots and emission.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "diag_payload.h"
#include "type_checker.h"

static void
diag_payload_snapshot_destroy_fields(DiagnosticPayloadSnapshot *snapshot)
{
    if (snapshot == NULL)
        return;
    free(snapshot->value_label);
    free(snapshot->provenance_label);
    free(snapshot->replacement_label);
    free(snapshot->transfer_label);
    free(snapshot->borrowed_name);
    free(snapshot->consumer_name);
    free(snapshot->secondary_name);
    free(snapshot->kind_label);
    free(snapshot->extra);
}

DiagnosticPayloadSnapshot *
diag_payload_snapshot_create(const DiagPayload *p)
{
    DiagnosticPayloadSnapshot *snapshot;

    if (p == NULL)
        return NULL;

    snapshot = calloc(1, sizeof(*snapshot));
    if (snapshot == NULL)
        return NULL;

    snapshot->value_label = p->value_label != NULL ? pergyra_strdup(p->value_label) : NULL;
    snapshot->provenance_label = p->provenance_label != NULL ? pergyra_strdup(p->provenance_label) : NULL;
    snapshot->replacement_label = p->replacement_label != NULL ? pergyra_strdup(p->replacement_label) : NULL;
    snapshot->transfer_label = p->transfer_label != NULL ? pergyra_strdup(p->transfer_label) : NULL;
    snapshot->borrowed_name = p->borrowed_name != NULL ? pergyra_strdup(p->borrowed_name) : NULL;
    snapshot->consumer_name = p->consumer_name != NULL ? pergyra_strdup(p->consumer_name) : NULL;
    snapshot->secondary_name = p->secondary_name != NULL ? pergyra_strdup(p->secondary_name) : NULL;
    snapshot->kind_label = p->kind_label != NULL ? pergyra_strdup(p->kind_label) : NULL;
    snapshot->extra = p->extra != NULL ? pergyra_strdup(p->extra) : NULL;

    return snapshot;
}

void
diag_payload_snapshot_destroy(DiagnosticPayloadSnapshot *snapshot)
{
    if (snapshot == NULL)
        return;
    diag_payload_snapshot_destroy_fields(snapshot);
    free(snapshot);
}

static void
emit_diagnostic_full(SemanticContext *ctx, DiagnosticLevel level,
                     const char *code, const char *cause_ir,
                     const char *fix_source, const ASTNode *node,
                     const DiagPayload *payload,
                     const char *fmt, va_list ap)
{
    char *message = NULL;
    va_list ap_copy;
    int len;
    va_copy(ap_copy, ap);
    len = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);
    if (len < 0)
        return;
    message = malloc((size_t)len + 1);
    if (message == NULL)
        return;
    vsnprintf(message, (size_t)len + 1, fmt, ap);

    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        Diagnostic *existing = ctx->diagnostics[i];
        if (existing == NULL)
            continue;
        if (existing->level != level)
            continue;
        if (existing->line != (node ? node->line : 0)
            || existing->col != (node ? node->column : 0)) {
            continue;
        }
        if (existing->message != NULL && strcmp(existing->message, message) == 0) {
            /* Upgrade legacy entry's metadata if the new call provides
             * fields the existing entry lacks. Keeps richer routing info
             * when a plain semantic_error hits the same site first. */
            if (existing->code == NULL && code != NULL)
                existing->code = code;
            if (existing->cause_ir == NULL && cause_ir != NULL)
                existing->cause_ir = cause_ir;
            if (existing->fix_source == NULL && fix_source != NULL)
                existing->fix_source = fix_source;
            if (existing->payload == NULL && payload != NULL)
                existing->payload = diag_payload_snapshot_create(payload);
            free(message);
            return;
        }
    }

    if (ctx->diagnostic_count >= ctx->diagnostic_capacity) {
        size_t new_cap = ctx->diagnostic_capacity * 2;
        Diagnostic **grown = realloc(ctx->diagnostics,
                                     new_cap * sizeof(Diagnostic *));
        if (grown == NULL) {
            free(message);
            return;
        }
        ctx->diagnostics         = grown;
        ctx->diagnostic_capacity = new_cap;
    }

    Diagnostic *d = calloc(1, sizeof(Diagnostic));
    if (d == NULL) {
        free(message);
        return;
    }

    d->level      = level;
    d->line       = node ? node->line   : 0;
    d->col        = node ? node->column : 0;
    d->message    = message;
    d->code       = code;        /* non-owning; static literal or NULL */
    d->cause_ir   = cause_ir;    /* non-owning; static literal or NULL */
    d->fix_source = fix_source;  /* non-owning; static literal or NULL */
    d->payload    = diag_payload_snapshot_create(payload);

    ctx->diagnostics[ctx->diagnostic_count++] = d;

    if (level == DIAG_ERROR)
        ctx->has_error = true;
}

static void
emit_diagnostic(SemanticContext *ctx, DiagnosticLevel level,
                const char *code, const ASTNode *node,
                const char *fmt, va_list ap)
{
    emit_diagnostic_full(ctx, level, code, NULL, NULL, node, NULL, fmt, ap);
}

void
semantic_error(SemanticContext *ctx, const ASTNode *node,
               const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic(ctx, DIAG_ERROR, NULL, node, fmt, ap);
    va_end(ap);
}

void
semantic_error_code(SemanticContext *ctx, const char *code,
                    const ASTNode *node, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic(ctx, DIAG_ERROR, code, node, fmt, ap);
    va_end(ap);
}

void
semantic_warning(SemanticContext *ctx, const ASTNode *node,
                 const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic(ctx, DIAG_WARNING, NULL, node, fmt, ap);
    va_end(ap);
}

void
semantic_warning_code(SemanticContext *ctx, const char *code,
                      const ASTNode *node, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic(ctx, DIAG_WARNING, code, node, fmt, ap);
    va_end(ap);
}

void
semantic_error_with_hints(SemanticContext *ctx,
                          const char *code,
                          const char *cause_ir,
                          const char *fix_source,
                          const ASTNode *node,
                          const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic_full(ctx, DIAG_ERROR, code, cause_ir, fix_source,
                         node, NULL, fmt, ap);
    va_end(ap);
}

void
semantic_warning_with_hints(SemanticContext *ctx,
                            const char *code,
                            const char *cause_ir,
                            const char *fix_source,
                            const ASTNode *node,
                            const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic_full(ctx, DIAG_WARNING, code, cause_ir, fix_source,
                         node, NULL, fmt, ap);
    va_end(ap);
}

void
semantic_emit_payload(SemanticContext *ctx,
                      const DiagPayload *p,
                      const char *fmt, ...)
{
    va_list ap;
    if (ctx == NULL || p == NULL || fmt == NULL)
        return;
    va_start(ap, fmt);
    emit_diagnostic_full(ctx, DIAG_ERROR, p->code, p->cause_ir,
                         p->fix_source, p->site, p, fmt, ap);
    va_end(ap);
}

void
semantic_print_diagnostics(SemanticContext *ctx)
{
    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        Diagnostic *d = ctx->diagnostics[i];
        const char *level = (d->level == DIAG_ERROR) ? "ERROR" : "WARNING";
        if (d->line == 0 || d->col == 0) {
            fprintf(stderr, "[%s] - %s\n", level, d->message);
        } else {
            fprintf(stderr, "[%s] %u:%u - %s\n",
                    level, d->line, d->col, d->message);
        }
    }
}
