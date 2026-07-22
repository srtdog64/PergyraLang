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
#include "../semantic/capability_analyze.h"
#include "air.h"
#include "dir.h"
#include "rir.h"
#include "mir.h"
#include "hir.h"
#include "module_loader.h"
#include "path_utils.h"
#include "runtime_none_contract.h"
#include "forin_desugar.h"
#include "llvm_runner.h"
#include "c_runner.h"
#include "driver_diag.h"
#include "machine_layer_manifest.h"
#include "verified_region_plan.h"

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
    DriverPhaseTimings timings;
    int exit_code;

    /* One level up from PGY_DEBUG_MIR_TIMING: the pipeline must answer
     * "which phase did the time go to" without a debugger. */
    if (getenv("PGY_DEBUG_PIPELINE_TIMING") == NULL)
        return driver_run_pipeline_timed(flags, NULL);

    exit_code = driver_run_pipeline_timed(flags, &timings);
    fprintf(stderr,
            "[pipeline timing] module_load=%.3f semantic=%.3f hir=%.3f"
            " dir=%.3f dir_validate=%.3f rir=%.3f rir_enrich=%.3f"
            " rir_validate=%.3f rir_dir_validate=%.3f air_synth=%.3f"
            " mir_lower=%.3f mir_validate=%.3f air_mir=%.3f backend=%.3f"
            " total=%.3f\n",
            timings.module_load, timings.semantic, timings.hir_lower,
            timings.dir_lower, timings.dir_validate, timings.rir_lower,
            timings.rir_enrich, timings.rir_validate,
            timings.rir_dir_validate, timings.air_synthesize,
            timings.mir_lower, timings.mir_validate,
            timings.air_mir_evidence, timings.backend, timings.total);
    return exit_code;
}

int
driver_run_pipeline_timed(const DriverFlags *flags, DriverPhaseTimings *timings)
{
    ASTNode *ast = NULL;
    PgySourceModuleGraph *module_graph = NULL;
    SemanticResult *sem = NULL;
    DIRProgram *dir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    HIRProgram *hir = NULL;
    HIRSemanticProjectionFailure hir_projection_failure =
        HIR_SEMANTIC_PROJECTION_NONE;
    AIRProgram *air = NULL;
    CompilerIRBundle bundle;
    PgyRegionPlan region_plan = {0};
    int exit_code = 1;
    char *load_error = NULL;
    char *hir_error = NULL;
    char *compatibility_manifest_path = NULL;
    double phase_start = 0.0;
    double total_start = driver_now_seconds();
    memset(&bundle, 0, sizeof(bundle));
    if (timings != NULL)
        memset(timings, 0, sizeof(*timings));

    if (flags != NULL && flags->machine_layer_physical_manifest != NULL) {
        const char *machine_manifest_error = NULL;
        if (!pgy_machine_layer_physical_manifest_bind(
                flags->machine_layer_physical_manifest,
                &machine_manifest_error)) {
            fprintf(stderr, "pgy: machine-layer target declaration rejected: %s\n",
                    machine_manifest_error != NULL
                        ? machine_manifest_error
                        : "invalid physical declaration");
            return 1;
        }
    }

    if (flags->dump_machine_manifest_json) {
        pgy_machine_layer_manifest_dump_json(stdout);
        return 0;
    }

    if (flags->dump_tokens) {
        char *source = path_read_file(flags->source_path);
        if (source == NULL)
            return 1;
        int rc = run_token_dump(source, flags->source_path);
        free(source);
        return rc;
    }

#ifdef PGY_PROJECT_ROOT
    compatibility_manifest_path = path_join_dup(
        PGY_PROJECT_ROOT,
        "src/self_hosted/compiler/expected/compatibility_evolution.txt");
    if (!driver_diag_compatibility_manifest_validate_file(
            compatibility_manifest_path, &load_error)) {
        driver_emit_stage_fail(flags, "compatibility",
            "compatibility evolution manifest validation failed",
            load_error != NULL ? load_error
                               : "compatibility evolution manifest is invalid");
        goto cleanup;
    }
    free(load_error);
    load_error = NULL;
#endif

    if (flags->verbose)
        printf("pgy: loading modules\n");

    driver_debug_stage("module_load");
    phase_start = driver_now_seconds();
    ast = module_loader_load_program_with_graph(
        flags->source_path, &module_graph, &load_error);
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
    if (!module_loader_validate_graph(module_graph, &load_error)) {
        driver_emit_stage_fail(flags, "module_load",
            "module graph validation failed",
            load_error != NULL ? load_error
                               : "unanchored source module graph");
        goto cleanup;
    }

    if (flags->dump_ast) {
        ast_print(ast, 0);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->verbose)
        printf("pgy: semantic analysis\n");

    /* Compile-path desugar (post-parse, post `--ast` dump so parser-parity is
     * untouched): hoist non-identifier for-in iterables into a synthetic local
     * so both backends see an identifier iterable evaluated exactly once. */
    forin_desugar_program(ast);

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

    /* Text mode: print diagnostics now (plain-text UX preserves mid-run warnings).
     * JSON mode: defer so exactly one JSON array lands on stderr per run.
     * Backend runners emit their own error array on failure; we emit the
     * semantic array only at terminal points (semantic-fail here, or the
     * pipeline-success site near the end of this function). */
    /* Machine-readable stage dumps own stdout as an artifact boundary. Do not
     * prefix the RIR JSON document with the human semantic summary; errors
     * still route to stderr through the normal fail-closed path. */
    if ((flags == NULL || flags->diag_format != DIAG_FORMAT_JSON)
        && (flags == NULL || !flags->dump_rir_json)) {
        semantic_result_print(sem);
    }

    if (flags->dump_capability_manifest) {
        /* Sound, interprocedurally-inferred capability manifest. Per-function
         * `with caps` violations (declared >= used) are ordinary semantic
         * errors, already printed above; the exit code reflects them. */
        capability_manifest_print(sem->program_capabilities, stdout);
        exit_code = sem->success ? 0 : 1;
        goto cleanup;
    }

    if (!sem->success) {
        if (flags != NULL && flags->diag_format == DIAG_FORMAT_JSON) {
            semantic_result_print_json(sem);  /* terminal: error array */
        } else {
            fprintf(stderr, "pgy: %zu error(s); aborting\n", sem->error_count);
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
    hir = hir_lower_with_semantic_facts(
        sem, &hir_projection_failure, &hir_error);
    if (timings != NULL)
        timings->hir_lower = driver_now_seconds() - phase_start;
    if (hir == NULL) {
        if (hir_projection_failure == HIR_SEMANTIC_PROJECTION_LOOP_FLOW) {
            driver_emit_stage_fail(flags, "hir_lower",
                "HIR loop-flow fact attachment failed",
                hir_error != NULL ? hir_error
                                  : "invalid LoopFlowSummary facts");
        } else if (hir_projection_failure
                   == HIR_SEMANTIC_PROJECTION_ITERATION_TYPE) {
            driver_emit_stage_fail(flags, "hir_lower",
                "HIR iteration type fact attachment failed",
                hir_error != NULL ? hir_error
                                  : "invalid iteration type facts");
        } else if (hir_projection_failure
                   == HIR_SEMANTIC_PROJECTION_DESTRUCTURE_TYPE) {
            driver_emit_stage_fail(flags, "hir_lower",
                "HIR destructure type fact attachment failed",
                hir_error != NULL ? hir_error
                                  : "invalid destructure type facts");
        } else if (hir_projection_failure
                   == HIR_SEMANTIC_PROJECTION_VALIDATE) {
            driver_emit_stage_fail(flags, "hir_validate",
                "HIR validation failed",
                hir_error != NULL ? hir_error : "invalid HIR");
        } else {
            driver_emit_stage_fail(flags, "hir_lower",
                "HIR lowering failed", hir_error);
        }
        goto cleanup;
    }
    driver_debug_stage("dir_lower");
    phase_start = driver_now_seconds();
    dir = dir_lower_with_hir_resource_flow_facts(
        sem->annotated_ast, hir, &hir_error);
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
    if (!air_collect_dag_evidence(air, sem, &hir_error)
        || !air_verify(air, &hir_error)) {
        driver_emit_stage_fail(flags, "air_dag_evidence",
            "AIR DAG evidence collection failed",
            hir_error != NULL ? hir_error : "invalid AIR/DAG evidence");
        goto cleanup;
    }
    if (timings != NULL)
        timings->air_synthesize = driver_now_seconds() - phase_start;
    if (air_drift_count(air) > 0 && !flags->dump_air_json) {
        driver_emit_air_drift_fail(flags, air);
        goto cleanup;
    }

    driver_debug_stage("mir_lower");
    phase_start = driver_now_seconds();
    MIRLowerRequest mir_request;
    mir_lower_request_init(&mir_request, hir, rir, sem);
    mir = mir_lower(&mir_request, &hir_error);
    if (timings != NULL)
        timings->mir_lower = driver_now_seconds() - phase_start;
    if (mir == NULL) {
        driver_emit_stage_fail(flags, "mir_lower",
            "MIR lowering failed", hir_error);
        goto cleanup;
    }
    /* Opt-in debug line directives: hand the source path to the backend only
     * when --debug-lines is set; NULL keeps generated output unchanged. */
    if (flags->emit_debug_lines)
        mir->source_path = flags->source_path;
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

    driver_debug_stage("air_mir_evidence");
    phase_start = driver_now_seconds();
    if (!air_collect_mir_evidence(air, mir, &hir_error)) {
        driver_emit_stage_fail(flags, "air_mir_evidence",
            "AIR MIR evidence collection failed",
            hir_error != NULL ? hir_error : "invalid AIR/MIR evidence");
        goto cleanup;
    }
    if (!air_verify(air, &hir_error)) {
        driver_emit_stage_fail(flags, "air_verify",
            "AIR verification failed after MIR evidence collection",
            hir_error != NULL ? hir_error : "invalid AIR graph");
        goto cleanup;
    }
    if (timings != NULL)
        timings->air_mir_evidence = driver_now_seconds() - phase_start;
    if (flags->dump_air && !flags->dump_air_json) {
        air_dump(air, stdout);
        exit_code = 0;
        goto cleanup;
    }
    if (flags->dump_air_json) {
        air_dump_json(air, stdout);
        exit_code = 0;
        goto cleanup;
    }
    if (air_drift_count(air) > 0) {
        driver_emit_air_drift_fail(flags, air);
        goto cleanup;
    }

    bundle.hir = hir;
    bundle.dir = dir;
    bundle.rir = rir;
    bundle.mir = mir;

    if (flags->dump_dir) {
        dir_dump(dir, stdout);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->dump_rir && !flags->dump_rir_json) {
        rir_dump(rir, stdout);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->dump_rir_json) {
        rir_dump_json(rir, stdout);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->dump_mir) {
        mir_dump(mir, stdout);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->dump_mir_json) {
        mir_dump_json(mir, stdout);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->dump_hir) {
        hir_dump_mode(hir, stdout, flags->hir_dump_mode);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->check_only) {
        exit_code = 0;
        goto cleanup;
    }

    /* Region selection is an AIR-admitted driver fact. Semantic analysis owns
     * the bounded escape rows; the driver only gates and materializes them.
     * Both MIR backends receive the verified immutable plan and must use its
     * per-site lookup. A missing/empty result remains the HEAP default. */
    driver_debug_stage("region_plan");
    {
        const char *region_error = NULL;
        PgyRegionEscapeResult region_escape = {
            sem->region_escape_facts,
            sem->region_escape_fact_count
        };
        if (!pgy_verified_region_plan_from_escape(
                (const PgyAirVerification *)air,
                &region_escape,
                &region_plan,
                &region_error)) {
            driver_emit_stage_fail(flags, "region_plan",
                "verified region plan construction failed",
                region_error != NULL
                    ? region_error
                    : "invalid AIR region evidence");
            goto cleanup;
        }
    }
    bundle.region_plan = &region_plan;

    /* Dispatch to backend runner */
    driver_debug_stage(flags->backend == BACKEND_LLVM && !flags->emit_c_only
                       ? "backend_llvm"
                       : "backend_c");
    phase_start = driver_now_seconds();
    if (flags->backend == BACKEND_LLVM && !flags->emit_c_only) {
        CompilerBackendTimings backend_timings = {0};
        exit_code = llvm_runner_execute(flags, &bundle, air,
                                        timings != NULL ? &backend_timings : NULL);
        if (timings != NULL) {
            timings->backend_codegen = backend_timings.codegen;
            timings->backend_native_compile = backend_timings.native_compile;
            timings->backend_link = backend_timings.link;
        }
    } else {
        CompilerBackendTimings backend_timings = {0};
        exit_code = c_runner_execute(flags, &bundle, air,
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
     * this guard correctly suppresses emit for those too; they are a
     * separate pre-existing gap tracked outside this fix. */
    if (sem != NULL && exit_code == 0
        && flags != NULL && flags->diag_format == DIAG_FORMAT_JSON) {
        semantic_result_print_json(sem);
    }

cleanup:
    /* A dump is not complete until its buffered bytes reach the configured
     * output boundary. Keep the debug total honest about output and teardown. */
    if (timings != NULL)
        (void)fflush(stdout);
    free(load_error);
    module_loader_destroy_graph(module_graph);
    free(compatibility_manifest_path);
    free(hir_error);
    pgy_verified_region_plan_dispose(&region_plan);
    dir_destroy(dir);
    air_destroy(air);
    mir_destroy(mir);
    rir_destroy(rir);
    hir_destroy(hir);
    semantic_result_destroy(sem);
    ast_destroy(ast);
    if (timings != NULL)
        timings->total = driver_now_seconds() - total_start;
    return exit_code;
}
