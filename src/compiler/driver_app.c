/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "driver_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "../lexer/lexer.h"
#include "../semantic/semantic.h"
#include "air.h"
#include "dir.h"
#include "rir.h"
#include "mir.h"
#include "hir.h"
#include "module_loader.h"
#include "path_utils.h"
#include "runtime_none_contract.h"
#include "llvm_runner.h"
#include "c_runner.h"
#include "driver_diag.h"

/* Path utilities are now in path_utils.h/c */

static double
driver_now_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER counter;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
#endif
}

static void
driver_debug_stage(const char *stage)
{
    if (stage != NULL && getenv("PGY_DEBUG_PIPELINE_STAGE") != NULL)
        fprintf(stderr, "[driver stage] %s\n", stage);
}

static int
run_token_dump(const char *source, const char *path)
{
    Lexer *lexer = lexer_create(source);
    if (lexer == NULL) {
        fprintf(stderr, "pgy: lexer init failed for '%s'\n", path);
        return 1;
    }

    printf("=== tokens: %s ===\n", path);
    int n = 0;
    Token tok;
    do {
        tok = lexer_next_token(lexer);
        printf("%4d  ", ++n);
        token_print(&tok);
        if (tok.type == TOKEN_ERROR) {
            fprintf(stderr, "pgy: lex error: %s\n", lexer_get_error(lexer));
            lexer_destroy(lexer);
            return 1;
        }
    } while (tok.type != TOKEN_EOF);

    printf("  %d tokens total\n", n);
    lexer_destroy(lexer);
    return 0;
}

int
driver_run_pipeline(const DriverFlags *flags)
{
    return driver_run_pipeline_timed(flags, NULL);
}

int
driver_run_pipeline_timed(const DriverFlags *flags, DriverPhaseTimings *timings)
{
    ASTNode *ast = NULL;
    SemanticResult *sem = NULL;
    DIRProgram *dir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    HIRProgram *hir = NULL;
    AIRProgram *air = NULL;
    CompilerIRBundle bundle;
    int exit_code = 1;
    char *load_error = NULL;
    char *hir_error = NULL;
    double phase_start = 0.0;
    double total_start = driver_now_seconds();
    memset(&bundle, 0, sizeof(bundle));
    if (timings != NULL)
        memset(timings, 0, sizeof(*timings));

    if (flags->dump_tokens) {
        char *source = path_read_file(flags->source_path);
        if (source == NULL)
            return 1;
        int rc = run_token_dump(source, flags->source_path);
        free(source);
        return rc;
    }

    if (flags->verbose)
        printf("pgy: loading modules\n");

    driver_debug_stage("module_load");
    phase_start = driver_now_seconds();
    ast = module_loader_load_program(flags->source_path, &load_error);
    if (timings != NULL)
        timings->module_load = driver_now_seconds() - phase_start;
    if (ast == NULL) {
        const char *msg = load_error != NULL ? load_error : "module loading failed";
        if (flags->diag_format == DIAG_FORMAT_JSON) {
            const char *code = driver_diag_code_from_message(msg);
            const char *cause_ir = driver_diag_cause_from_code(code);
            const char *fix_source = driver_diag_fix_from_code(code);
            /* Distinguish parse errors from other module-load failures so AI
             * consumers can route syntax vs I/O vs resolution issues. Module
             * loader prefixes parse errors with "parse error in '<path>':". */
            const char *stage =
                (strncmp(msg, "parse error in", 14) == 0) ? "parse"
                                                          : "module_load";
            stage = driver_route_stage(stage, code);
            driver_emit_single_diag_json_full(stage,
                                              code,
                                              cause_ir,
                                              fix_source,
                                              msg);
        } else {
            fprintf(stderr, "pgy: %s\n", msg);
        }
        goto cleanup;
    }

    if (flags->dump_ast) {
        ast_print(ast, 0);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->verbose)
        printf("pgy: semantic analysis\n");

    driver_debug_stage("semantic");
    phase_start = driver_now_seconds();
    sem = semantic_analyze(ast);
    if (timings != NULL)
        timings->semantic = driver_now_seconds() - phase_start;
    if (sem == NULL) {
        driver_emit_stage_fail(flags, "semantic",
            "semantic analysis failed", "out of memory during semantic analysis");
        goto cleanup;
    }

    /* Text mode: print diagnostics now (legacy UX preserves mid-run warnings).
     * JSON mode: defer so exactly one JSON array lands on stderr per run.
     * Backend runners emit their own error array on failure; we emit the
     * semantic array only at terminal points (semantic-fail here, or the
     * pipeline-success site near the end of this function). */
    if (flags == NULL || flags->diag_format != DIAG_FORMAT_JSON) {
        semantic_result_print(sem);
    }
    if (!sem->success) {
        if (flags != NULL && flags->diag_format == DIAG_FORMAT_JSON) {
            semantic_result_print_json(sem);  /* terminal: error array */
        } else {
            fprintf(stderr, "pgy: %zu error(s) ??aborting\n", sem->error_count);
        }
        goto cleanup;
    }

    if (flags != NULL && flags->runtime_mode == RUNTIME_NONE) {
        char runtime_none_error[512];
        runtime_none_error[0] = '\0';
        if (!runtime_none_validate_ast(sem->annotated_ast,
                                       runtime_none_error,
                                       sizeof(runtime_none_error))) {
            driver_emit_stage_fail(flags,
                                   "driver",
                                   "--runtime=none rejected",
                                   runtime_none_error);
            goto cleanup;
        }
        driver_emit_stage_fail(
            flags,
            "driver",
            "--runtime=none rejected",
            "PGY_DRIVER_RUNTIME_NONE_UNSUPPORTED: --runtime=none accepted the source surface but freestanding backend lowering is not implemented yet. "
            "Reason: beta no-runtime mode must not silently link the default scheduler/arena runtime. "
            "Fix: use --runtime=default until freestanding C/LLVM lowering is implemented.");
        goto cleanup;
    }

    driver_debug_stage("hir_lower");
    phase_start = driver_now_seconds();
    hir = hir_lower(sem->annotated_ast, &hir_error);
    if (timings != NULL)
        timings->hir_lower = driver_now_seconds() - phase_start;
    if (hir == NULL) {
        driver_emit_stage_fail(flags, "hir_lower",
            "HIR lowering failed", hir_error);
        goto cleanup;
    }

    driver_debug_stage("dir_lower");
    phase_start = driver_now_seconds();
    dir = dir_lower(sem->annotated_ast, &hir_error);
    if (timings != NULL)
        timings->dir_lower = driver_now_seconds() - phase_start;
    if (dir == NULL) {
        driver_emit_stage_fail(flags, "dir_lower",
            "DIR lowering failed", hir_error);
        goto cleanup;
    }
    driver_debug_stage("dir_validate");
    phase_start = driver_now_seconds();
    if (!dir_validate(dir, &hir_error)) {
        if (timings != NULL)
            timings->dir_validate = driver_now_seconds() - phase_start;
        driver_emit_stage_fail(flags, "dir_validate",
            "DIR validation failed",
            hir_error != NULL ? hir_error : "invalid DIR");
        goto cleanup;
    }
    if (timings != NULL)
        timings->dir_validate = driver_now_seconds() - phase_start;

    driver_debug_stage("rir_lower");
    phase_start = driver_now_seconds();
    rir = rir_lower(sem->annotated_ast, &hir_error);
    if (timings != NULL)
        timings->rir_lower = driver_now_seconds() - phase_start;
    if (rir == NULL) {
        driver_emit_stage_fail(flags, "rir_lower",
            "RIR lowering failed", hir_error);
        goto cleanup;
    }
    driver_debug_stage("rir_enrich");
    phase_start = driver_now_seconds();
    if (!rir_enrich_with_hir_flow(rir, hir, &hir_error)) {
        if (timings != NULL)
            timings->rir_enrich = driver_now_seconds() - phase_start;
        driver_emit_stage_fail(flags, "rir_enrich",
            "RIR flow enrichment failed", hir_error);
        goto cleanup;
    }
    if (timings != NULL)
        timings->rir_enrich = driver_now_seconds() - phase_start;
    driver_debug_stage("rir_validate");
    phase_start = driver_now_seconds();
    if (!rir_validate(rir, &hir_error)) {
        if (timings != NULL)
            timings->rir_validate = driver_now_seconds() - phase_start;
        driver_emit_stage_fail(flags, "rir_validate",
            "RIR validation failed",
            hir_error != NULL ? hir_error : "invalid RIR");
        goto cleanup;
    }
    if (timings != NULL)
        timings->rir_validate = driver_now_seconds() - phase_start;
    driver_debug_stage("rir_dir_validate");
    phase_start = driver_now_seconds();
    if (!rir_validate_against_dir(rir, dir, &hir_error)) {
        if (timings != NULL)
            timings->rir_dir_validate = driver_now_seconds() - phase_start;
        driver_emit_stage_fail(flags, "rir_dir_validate",
            "RIR/DIR validation failed",
            hir_error != NULL ? hir_error : "invalid RIR/DIR contract");
        goto cleanup;
    }
    if (timings != NULL)
        timings->rir_dir_validate = driver_now_seconds() - phase_start;

    driver_debug_stage("air_synthesize");
    phase_start = driver_now_seconds();
    air = air_synthesize(hir, dir, rir, &hir_error);
    if (air == NULL) {
        driver_emit_stage_fail(flags, "air_synthesize",
            "AIR synthesis failed", hir_error);
        goto cleanup;
    }
    if (flags->dump_air) {
        air_dump(air, stdout);
        exit_code = 0;
        goto cleanup;
    }
    if (air->drift_count > 0) {
        driver_emit_air_drift_fail(flags, air);
        goto cleanup;
    }

    driver_debug_stage("mir_lower");
    phase_start = driver_now_seconds();
    mir = mir_lower(hir, rir, &hir_error);
    if (timings != NULL)
        timings->mir_lower = driver_now_seconds() - phase_start;
    if (mir == NULL) {
        driver_emit_stage_fail(flags, "mir_lower",
            "MIR lowering failed", hir_error);
        goto cleanup;
    }
    driver_debug_stage("mir_validate");
    phase_start = driver_now_seconds();
    if (!mir_validate(mir, &hir_error)) {
        if (timings != NULL)
            timings->mir_validate = driver_now_seconds() - phase_start;
        driver_emit_stage_fail(flags, "mir_validate",
            "MIR validation failed",
            hir_error != NULL ? hir_error : "invalid MIR");
        goto cleanup;
    }
    if (timings != NULL)
        timings->mir_validate = driver_now_seconds() - phase_start;

    bundle.hir = hir;
    bundle.dir = dir;
    bundle.rir = rir;
    bundle.mir = mir;

    if (flags->dump_dir) {
        dir_dump(dir, stdout);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->dump_rir) {
        rir_dump(rir, stdout);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->dump_mir) {
        mir_dump(mir, stdout);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->dump_hir) {
        hir_dump_mode(hir, stdout, flags->hir_dump_mode);
        exit_code = 0;
        goto cleanup;
    }

    /* Dispatch to backend runner */
    driver_debug_stage(flags->backend == BACKEND_LLVM && !flags->emit_c_only
                       ? "backend_llvm"
                       : "backend_c");
    phase_start = driver_now_seconds();
    if (flags->backend == BACKEND_LLVM && !flags->emit_c_only) {
        CompilerBackendTimings backend_timings = {0};
        exit_code = llvm_runner_execute(flags, &bundle,
                                        timings != NULL ? &backend_timings : NULL);
        if (timings != NULL) {
            timings->backend_codegen = backend_timings.codegen;
            timings->backend_native_compile = backend_timings.native_compile;
            timings->backend_link = backend_timings.link;
        }
    } else {
        CompilerBackendTimings backend_timings = {0};
        exit_code = c_runner_execute(flags, &bundle,
                                     timings != NULL ? &backend_timings : NULL);
        if (timings != NULL) {
            timings->backend_codegen = backend_timings.codegen;
            timings->backend_native_compile = backend_timings.native_compile;
            timings->backend_link = backend_timings.link;
        }
    }
    if (timings != NULL)
        timings->backend = driver_now_seconds() - phase_start;

    /* Terminal semantic-JSON emit for the "full success" path. Backend
     * runners emit their own error array on failure (exit_code != 0) and
     * we skip here to avoid a second JSON array on stderr. HIR/DIR/RIR/MIR
     * mid-pipeline failures also keep exit_code == 1 (initial value) so
     * this guard correctly suppresses emit for those too ??they are a
     * separate pre-existing gap tracked outside this fix. */
    if (sem != NULL && exit_code == 0
        && flags != NULL && flags->diag_format == DIAG_FORMAT_JSON) {
        semantic_result_print_json(sem);
    }

cleanup:
    if (timings != NULL)
        timings->total = driver_now_seconds() - total_start;
    free(load_error);
    free(hir_error);
    dir_destroy(dir);
    air_destroy(air);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
    semantic_result_destroy(sem);
    ast_destroy(ast);
    return exit_code;
}

void
driver_print_usage(void)
{
    printf(
        "Usage:\n"
        "  pgy <source.pgy>              compile to native binary\n"
        "  pgy <source.pgy> -o <out>     name the emitted native binary\n"
        "  pgy <source.pgy> --emit-c     stop after generating C\n"
        "  pgy <source.pgy> --emit-c -o <out.c>\n"
        "  pgy <source.pgy> --emit-llvm -o <out.ll>\n"
#ifdef PGY_LLVM_ENABLED
        "  (LLVM + --run): if -o ends with .o/.obj, executable target becomes .exe on Windows\n"
#endif
        "  pgy <source.pgy> --run        compile + run\n"
        "  pgy <source.pgy> --opt=dev|release   (default: release)\n"
        "  pgy <source.pgy> --runtime=default|none  runtime contract mode (none is beta-gated)\n"
        "  pgy <source.pgy> --error-format=json|text  (default: text; json for structured tooling)\n"
        "  pgy scaffold <kind> <target> create starter files\n"
        "  pgy new <project-dir>         scaffold a starter project\n"
        "\n"
        "Project design order:\n"
        "  intent -> world -> zone -> subject\n"
        "\n"
        "Host scaffold kinds:\n"
        "  subject  active host / who performs the contract\n"
        "  class    passive tool or thing with hosted func\n"
        "  object   passive view or state target\n"
        "  tobject  boundary packet\n"
        "  pgy --tokens <source.pgy>     dump token stream\n"
        "  pgy --ast    <source.pgy>     dump merged/normalized AST\n"
        "  pgy --dir    <source.pgy>     dump lowered DIR summary\n"
        "  pgy --rir    <source.pgy>     dump lowered RIR summary\n"
        "  pgy --air    <source.pgy>     dump AIR verification summary\n"
        "  pgy --mir    <source.pgy>     dump lowered MIR summary\n"
        "  pgy --hir     <source.pgy>     dump lowered HIR summary\n"
        "  pgy --hir-cfg <source.pgy>     dump HIR CFG view\n"
        "  pgy --hir-dom <source.pgy>     dump HIR dominance view\n"
        "  pgy --hir-ssa <source.pgy>     dump HIR SSA-prep view\n"
#ifdef PGY_LLVM_ENABLED
        "  default backend: LLVM\n"
        "  pgy <source.pgy> --backend=llvm   use LLVM native backend\n"
#else
        "  default backend: C\n"
#endif
#ifdef PGY_LLVM_ENABLED
        "  pgy <source.pgy> --emit-llvm      emit LLVM IR text\n"
#endif
        "  pgy --repl                    interactive REPL\n"
        "  pgy --help\n");
}
