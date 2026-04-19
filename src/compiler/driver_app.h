/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_DRIVER_APP_H
#define PGY_DRIVER_APP_H

#include <stdbool.h>
#include "compiler.h"

typedef enum
{
    BACKEND_C,
    BACKEND_LLVM
} BackendKind;

typedef enum
{
    DIAG_FORMAT_TEXT,   /* Default: human-readable "[ERROR] line:col - msg" */
    DIAG_FORMAT_JSON    /* Machine-readable: JSON array of diagnostic objects */
} DiagnosticFormat;

typedef struct
{
    double module_load;
    double semantic;
    double hir_lower;
    double dir_lower;
    double dir_validate;
    double rir_lower;
    double rir_enrich;
    double rir_validate;
    double rir_dir_validate;
    double mir_lower;
    double mir_validate;
    double backend;
    double backend_codegen;
    double backend_native_compile;
    double backend_link;
    double total;
} DriverPhaseTimings;

typedef struct
{
    const char *source_path;
    const char *output_path;
    bool        emit_c_only;
    bool        emit_llvm_ir;
    bool        do_run;
    bool        dump_tokens;
    bool        dump_ast;
    bool        dump_dir;
    bool        dump_rir;
    bool        dump_mir;
    bool        dump_hir;
    HIRDumpMode hir_dump_mode;
    bool        verbose;
    bool        repl;
    BackendKind backend;
    PgyOptProfile opt_profile;
    DiagnosticFormat diag_format;  /* --error-format=text|json (default text) */
} DriverFlags;

int  driver_run_pipeline(const DriverFlags *flags);
int  driver_run_pipeline_timed(const DriverFlags *flags,
                               DriverPhaseTimings *timings);
int  driver_run_scaffold_command(int argc, char *argv[]);
void driver_print_usage(void);

/* Emit a single diagnostic as a JSON array to stderr. For error sites
 * outside the SemanticResult accumulator (module loader, backend codegen,
 * linker). `stage` is a short tag; location is best-effort extracted from
 * the message if it contains "line N[, column M]". */
void driver_emit_single_diag_json(const char *stage, const char *message);

/* Variant that attaches a stable diagnostic code (e.g. "PGY_MIR_UNRESOLVED_LOCAL").
 * If `code` is NULL, the emitted JSON omits the "code" field (legacy shape).
 * Otherwise the JSON includes "code": "<code>". */
void driver_emit_single_diag_json_with_code(const char *stage,
                                             const char *code,
                                             const char *message);

/* Full variant that also attaches the optional routing tags `cause_ir`
 * (IR-level origin) and `fix_source` (source-level repair action). Any
 * of {code, cause_ir, fix_source} may be NULL — omitted fields are
 * dropped from the JSON. All string inputs must be valid UTF-8 and live
 * at least until this call returns. */
void driver_emit_single_diag_json_full(const char *stage,
                                        const char *code,
                                        const char *cause_ir,
                                        const char *fix_source,
                                        const char *message);

/* Pick the effective stage tag for a diagnostic given the default (what the
 * runner knows about where it was invoked) and the code prefix (what the
 * failing site knows about why). Hybrid routing:
 *   PGY_MIR_*   → "mir_validation"
 *   PGY_LLVM_*  → "llvm_codegen"
 *   PGY_SEM_*   → "semantic"
 *   PGY_PARSE_* → "parse"
 *   else        → default_stage
 * Unknown prefixes fall through to default_stage, so future prefix churn
 * cannot silently mis-route. */
const char *driver_route_stage(const char *default_stage, const char *code);

#endif /* PGY_DRIVER_APP_H */
