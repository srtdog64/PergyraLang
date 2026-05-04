/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Semantic diagnostic data shared by semantic analysis, LSP, and tooling.
 */

#ifndef PERGYRA_DIAGNOSTIC_TYPES_H
#define PERGYRA_DIAGNOSTIC_TYPES_H

#include <stdint.h>
#include "../common/diagnostic_layer.h"

typedef struct DiagnosticPayloadSnapshot DiagnosticPayloadSnapshot;

/*
 * Diagnostic severity
 */
typedef enum
{
    DIAG_ERROR,
    DIAG_WARNING
} DiagnosticLevel;

/*
 * One compiler message.
 *
 * `code` is a stable identifier for downstream error routing
 * (e.g. "PGY_SEM_TYPE_MISMATCH"). It points to a string literal
 * owned by the compiler text segment and is not freed by diagnostic
 * destruction. NULL means the site has not yet been assigned a
 * stable code (legacy path). Once assigned, a code's meaning
 * does not change across versions.
 */
typedef struct Diagnostic
{
    DiagnosticLevel level;
    uint32_t        line;
    uint32_t        col;
    char           *message;
    const char     *code;         /* non-owning pointer to static string */
    DiagnosticLayer layer;

    /* Optional routing hints. Both NULL when the site did not set them;
     * both non-owning (static literal).
     *
     *  - `cause_ir`    identifies the IR-level origin of the diagnostic,
     *                  format `<stage>:<subsystem>:<condition>`, e.g.
     *                  "semantic:assignability_check" or
     *                  "llvm:result_spec:capacity_exceeded". Disambiguates
     *                  the same `code` fired from different pipeline paths.
     *
     *  - `fix_source`  short stable token describing the source-level
     *                  repair action (e.g. "annotate-or-convert",
     *                  "reuse-shared-error-enum"). Independent from the
     *                  free-text `message` so message wording can evolve
     *                  without breaking tooling that routes on this tag. */
    const char *cause_ir;
    const char *fix_source;
    DiagnosticPayloadSnapshot *payload; /* owned; optional structured payload snapshot */
} Diagnostic;

#endif /* PERGYRA_DIAGNOSTIC_TYPES_H */
