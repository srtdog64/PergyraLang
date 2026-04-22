/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * DiagPayload — structured diagnostic payload (P5 design seed).
 *
 * Many borrow-escape / generic-mismatch / authority-mismatch helpers
 * currently take 8-15 positional `const char *` arguments and feed them
 * into one giant printf format string.  That pattern is fragile to
 * placeholder drift: a single off-by-one in the call site swaps two
 * fields in the rendered message.
 *
 * DiagPayload separates "what data does this diagnostic carry" from
 * "how is it rendered".  Callers fill a payload with named fields, then
 * `semantic_emit_payload(...)` does the rendering.  Renderers can be
 * swapped (text vs JSON-detail) without touching call sites.
 *
 * This is the seed.  Today only the borrowed-container-store helper is
 * converted (proof of concept).  Other 10-param helpers will migrate
 * incrementally; see docs/72_diagnostic_codes.md.
 *
 * Naming:
 *   DiagPayload           — opaque-ish struct, fields have stable names
 *   diag_payload_init     — zero-init helper
 *   semantic_emit_payload — render + emit through ctx
 */

#ifndef PGY_DIAG_PAYLOAD_H
#define PGY_DIAG_PAYLOAD_H

#include <stdbool.h>
#include <stddef.h>

#include "type_checker.h"

/* All fields are optional; renderers must tolerate NULL with sensible
 * defaults ("<unknown>", omitted line, etc).  String pointers are
 * borrowed — caller owns the storage for the duration of the emit. */
typedef struct DiagPayload
{
    /* Routing — required for stable downstream consumption. */
    const char *code;          /* "PGY_SEM_..." */
    const char *cause_ir;      /* "semantic:..." */
    const char *fix_source;    /* "align-..." */

    /* Source location anchor. */
    const ASTNode *site;

    /* Common semantic context fields. */
    const char *value_label;        /* canonical: "slot handle (anchored)", "subject", ... */
    const char *provenance_label;   /* "subject provenance", "boundary provenance", ... */
    const char *replacement_label;  /* "a projection/value result" */
    const char *transfer_label;     /* "transfer", "store", "send" */
    const char *borrowed_name;      /* original borrowed binding name */
    const char *consumer_name;      /* function/scope receiving the value */
    const char *secondary_name;     /* container/callee/destination name */
    const char *kind_label;         /* "container kind", "channel direction", ... */

    /* Free-form extension slot (rare cases that don't fit above). */
    const char *extra;
} DiagPayload;

/* Stable, result-owned snapshot of a DiagPayload.
 * All strings are owned copies so the snapshot can outlive scratch
 * formatting/storage used while constructing the diagnostic. */
struct DiagnosticPayloadSnapshot
{
    char *value_label;
    char *provenance_label;
    char *replacement_label;
    char *transfer_label;
    char *borrowed_name;
    char *consumer_name;
    char *secondary_name;
    char *kind_label;
    char *extra;
};

static inline void
diag_payload_init(DiagPayload *p)
{
    if (p == NULL)
        return;
    p->code = NULL;
    p->cause_ir = NULL;
    p->fix_source = NULL;
    p->site = NULL;
    p->value_label = NULL;
    p->provenance_label = NULL;
    p->replacement_label = NULL;
    p->transfer_label = NULL;
    p->borrowed_name = NULL;
    p->consumer_name = NULL;
    p->secondary_name = NULL;
    p->kind_label = NULL;
    p->extra = NULL;
}

/* Emit the diagnostic via ctx.  `fmt` is the human-readable format
 * string and may reference any DiagPayload field — the renderer will
 * substitute NULL fields with stable placeholders.
 *
 * For now this is a thin wrapper over semantic_error_with_hints; future
 * work may add a structured-output renderer that consumes the payload
 * directly without re-rendering through fmt. */
void semantic_emit_payload(SemanticContext *ctx,
                           const DiagPayload *p,
                           const char *fmt, ...);

DiagnosticPayloadSnapshot *
diag_payload_snapshot_create(const DiagPayload *p);

void
diag_payload_snapshot_destroy(DiagnosticPayloadSnapshot *snapshot);

#endif /* PGY_DIAG_PAYLOAD_H */
