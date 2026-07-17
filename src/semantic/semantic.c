/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Semantic Analyzer — unified entry point
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "../common/string_compat.h"
#include "semantic.h"
#include "diag_payload.h"
#include "type_checker.h"
#include "slot_analyzer.h"
#include "slot_analyzer_internal.h"
#include "lifecycle_analyze.h"
#include "../compiler/import_resolver.h"

static bool
semantic_loaded_modules_append(char ***module_names,
                               size_t *count,
                               size_t *capacity,
                               const char *module_name)
{
    char **grown;
    size_t new_capacity;
    char *copy;

    if (module_names == NULL || count == NULL || capacity == NULL
        || module_name == NULL) {
        return false;
    }

    if (*count >= *capacity) {
        if (*capacity == 0) {
            new_capacity = 8;
        } else {
            if (*capacity > SIZE_MAX / 2)
                return false;
            new_capacity = *capacity * 2;
        }
        if (new_capacity > SIZE_MAX / sizeof(char *)) {
            return false;
        }
        grown = realloc(*module_names, sizeof(char *) * new_capacity);
        if (grown == NULL)
            return false;
        *module_names = grown;
        *capacity = new_capacity;
    }

    copy = pergyra_strdup(module_name);
    if (copy == NULL)
        return false;
    (*module_names)[(*count)++] = copy;
    return true;
}

static bool
semantic_program_has_module_name(char **module_names, size_t module_count,
                                 const char *module_name)
{
    for (size_t i = 0; i < module_count; i++) {
        if (module_names[i] != NULL && module_name != NULL
            && strcmp(module_names[i], module_name) == 0) {
            return true;
        }
    }

    return false;
}

static void
semantic_preload_stdlib_uses(ASTNode *ast)
{
    char  **loaded_modules = NULL;
    size_t  loaded_count = 0;
    size_t  loaded_capacity = 0;
    PgyArena path_arena;

    if (ast == NULL || ast->type != AST_PROGRAM)
        return;

    /* Per-pass scratch arena for module path assembly.  Each iteration's
     * path string is short-lived and never escapes: it is consumed by
     * import_resolver_load_program and then unused.  Using a function-local
     * arena batches the allocations and removes N malloc/free pairs. */
    pgy_arena_init(&path_arena, 0);

    for (size_t i = 0; i < ast_program_statement_count(ast); i++) {
        ASTNode *stmt = ast_program_statement(ast, i);
        char *module_path = NULL;
        ASTNode *loaded = NULL;
        char *error_message = NULL;

        if (stmt == NULL || stmt->type != AST_USE_DECL
            || ast_use_module_name(stmt) == NULL) {
            continue;
        }

        if (semantic_program_has_module_name(loaded_modules, loaded_count,
                ast_use_module_name(stmt))) {
            continue;
        }

        module_path = pgy_arena_fmt(&path_arena, "%s/stdlib/%s.pgy",
            PGY_PROJECT_ROOT, ast_use_module_name(stmt));
        if (module_path == NULL)
            continue;

        loaded = import_resolver_load_program(module_path, &error_message);
        /* module_path is arena-owned: no free here. */
        free(error_message);
        if (loaded == NULL || loaded->type != AST_PROGRAM) {
            ast_destroy(loaded);
            continue;
        }

        (void)semantic_loaded_modules_append(&loaded_modules,
                                             &loaded_count,
                                             &loaded_capacity,
                                             ast_use_module_name(stmt));

        for (size_t j = 0; j < ast_program_statement_count(loaded); j++) {
            ASTNode *imported_stmt = ast_program_statement(loaded, j);
            bool explicit_private = false;

            if (imported_stmt == NULL
                || imported_stmt->type == AST_IMPORT_DECL
                || imported_stmt->type == AST_USE_DECL) {
                continue;
            }

            explicit_private = imported_stmt->has_explicit_access
                && (imported_stmt->access == ACCESS_PRIVATE
                    || imported_stmt->access == ACCESS_PROTECTED);
            imported_stmt->is_exported = !explicit_private;

            if (!ast_program_append_statement(ast, imported_stmt)) {
                break;
            }
            (void)ast_program_detach_statement(loaded, j);
        }

        ast_destroy(loaded);
    }

    for (size_t i = 0; i < loaded_count; i++)
        free(loaded_modules[i]);
    free(loaded_modules);
    pgy_arena_destroy(&path_arena);
}

static void
semantic_run_legacy_slot_resource_analysis(ASTNode *ast, SemanticContext *ctx)
{
    SlotAnalyzer *sa;

    if (ctx == NULL || ctx->has_error)
        return;

    /*
     * Domain-lifecycle analysis (N-state engine). Consumes parser-owned
     * AST_LIFECYCLE_DECL nodes; lifecycle-free programs remain a no-op.
     */
    if (!lifecycle_analyze_program(ast, ctx))
        return;

    /*
     * Compatibility seam: CFG/MIR owns beta body-safety truth. The legacy
     * slot analyzer remains here only for conservative slot escape/leak
     * provenance until those warnings are fully backed by CFG/MIR facts.
     */
    sa = slot_analyzer_create(ctx);
    if (sa != NULL) {
        if (!slot_analyze_program(ast, sa) && !ctx->has_error) {
            semantic_error(ctx, ast,
                "Slot resource-boundary analysis failed before it could "
                "produce a specific diagnostic");
        }
        slot_analyzer_destroy(sa);
        return;
    }

    semantic_error(ctx, ast,
        "Slot resource-boundary analysis could not allocate state");
}

static bool
semantic_snapshot_lifecycle_state_spaces(SemanticResult *result)
{
    int count;

    if (result == NULL)
        return false;

    count = lc_registry_count();
    if (count <= 0)
        return true;

    result->lifecycle_state_spaces =
        (LcSpec *)calloc((size_t)count, sizeof(LcSpec));
    if (result->lifecycle_state_spaces == NULL)
        return false;

    result->lifecycle_state_space_count = (size_t)count;
    for (int i = 0; i < count; i++) {
        const LcSpec *src = lc_registry_at(i);
        LcSpec *dst = &result->lifecycle_state_spaces[i];
        if (src == NULL)
            return false;

        *dst = *src;
        for (int s = 0; s < dst->state_count && s < LC_MAX_STATES; s++)
            dst->state_names[s] = dst->state_buf[s];
        for (int o = 0; o < dst->op_count && o < LC_MAX_OPS; o++)
            dst->op_names[o] = dst->op_buf[o];
    }
    return true;
}

SemanticResult *
semantic_analyze(ASTNode *ast)
{
    /* Default: no advisories. Batch/CI compiles pay zero cost (docs/140). */
    return semantic_analyze_ex(ast, false);
}

/* PGY_DEBUG_SEMANTIC_TIMING: the one-level-down sibling of
 * PGY_DEBUG_PIPELINE_TIMING (which itself sits above PGY_DEBUG_MIR_TIMING).
 * The semantic stage must answer "which pass did the time go to" without a
 * debugger. clock() is wallclock on Windows MSVCRT, matching mir.c. */
static double
semantic_timing_now(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

SemanticResult *
semantic_analyze_ex(ASTNode *ast, bool emit_advisories)
{
    bool timing = getenv("PGY_DEBUG_SEMANTIC_TIMING") != NULL;
    double t_mark = 0.0;
    double t_preload = 0.0;
    double t_type_check = 0.0;
    double t_legacy_slot = 0.0;

    SemanticResult *result = calloc(1, sizeof(SemanticResult));
    if (result == NULL)
        return NULL;

    /* Open the Type registry before anything can allocate a Type. It is
     * handed to the result below, and freed by semantic_result_destroy --
     * i.e. after every IR and the backend have stopped reading types. */
    result->owned_types = type_registry_begin();
    if (result->owned_types == NULL) {
        free(result);
        return NULL;
    }

    SemanticContext *ctx = semantic_context_create();
    if (ctx == NULL) {
        type_registry_destroy(result->owned_types);
        free(result);
        return NULL;
    }
    ctx->emit_advisories = emit_advisories;

    if (timing)
        t_mark = semantic_timing_now();
    semantic_preload_stdlib_uses(ast);
    if (timing) {
        t_preload = semantic_timing_now() - t_mark;
        t_mark = semantic_timing_now();
    }

    /* Pass 1 + 2: Symbol collection and type checking */
    type_check_program(ast, ctx);
    if (timing) {
        t_type_check = semantic_timing_now() - t_mark;
        t_mark = semantic_timing_now();
    }

    semantic_run_legacy_slot_resource_analysis(ast, ctx);
    if (!function_param_flow_summary_snapshot(ctx)) {
        semantic_error(ctx, ast,
            "Function parameter flow summary snapshot allocation failed");
    }
    if (ctx->loop_flow_summary_capture_failed) {
        semantic_error(ctx, ast,
            "Loop flow summary snapshot allocation failed");
    }
    if (timing) {
        t_legacy_slot = semantic_timing_now() - t_mark;
        fprintf(stderr,
                "[semantic timing] stdlib_preload=%.3f type_check=%.3f"
                " legacy_slot_lifecycle=%.3f\n",
                t_preload, t_type_check, t_legacy_slot);
    }

    if (!semantic_snapshot_lifecycle_state_spaces(result)) {
        semantic_error(ctx, ast,
            "Lifecycle state-space fact snapshot allocation failed");
    }

    /* Transfer diagnostics to result */
    result->success          = !ctx->has_error;
    result->annotated_ast    = ast;
    result->diagnostic_count = ctx->diagnostic_count;
    result->diagnostics      = ctx->diagnostics;
    result->type_resolution_metadata_entries =
        ctx->type_resolution_metadata.count;
    result->type_resolution_metadata_hits =
        ctx->type_resolution_metadata_hits;
    result->type_resolution_metadata_dead_ends =
        ctx->type_resolution_metadata_dead_ends;
    result->type_resolution_dag_generic_contract_evidence_count =
        ctx->type_resolution_dag_generic_contract_evidence_count;
    result->type_resolution_dag_ability_consumer_evidence_count =
        ctx->type_resolution_dag_ability_consumer_evidence_count;
    result->boundary_witness_summary = ctx->boundary_witness_summary;
    result->parallel_capture_boundaries =
        ctx->parallel_capture_boundaries;
    result->parallel_capture_boundary_count =
        ctx->parallel_capture_boundary_count;
    result->resource_flow_facts = ctx->resource_flow_facts;
    result->resource_flow_fact_count = ctx->resource_flow_fact_count;
    result->function_param_flow_facts = ctx->function_param_flow_facts;
    result->function_param_flow_fact_count =
        ctx->function_param_flow_fact_count;
    result->iteration_type_facts = ctx->iteration_type_facts;
    result->iteration_type_fact_count = ctx->iteration_type_fact_count;
    result->loop_flow_summary_facts = ctx->loop_flow_summary_facts;
    result->loop_flow_summary_fact_count =
        ctx->loop_flow_summary_fact_count;
    result->loop_flow_state_facts = ctx->loop_flow_state_facts;
    result->loop_flow_state_fact_count = ctx->loop_flow_state_fact_count;
    result->program_capabilities = ctx->program_capabilities;
    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        DiagnosticLevel lvl = ctx->diagnostics[i]->level;
        if (lvl == DIAG_ERROR)
            result->error_count++;
        else if (lvl == DIAG_WARNING)
            result->warning_count++;
        else
            result->advisory_count++;
    }

    /*
     * Transfer ownership of diagnostics array to result.
     * Set ctx pointer to NULL so context_destroy doesn't double-free.
     */
    ctx->diagnostics      = NULL;
    ctx->diagnostic_count = 0;
    ctx->diagnostic_capacity = 0;
    ctx->parallel_capture_boundaries = NULL;
    ctx->parallel_capture_boundary_count = 0;
    ctx->parallel_capture_boundary_capacity = 0;
    ctx->resource_flow_facts = NULL;
    ctx->resource_flow_fact_count = 0;
    ctx->resource_flow_fact_capacity = 0;
    ctx->function_param_flow_facts = NULL;
    ctx->function_param_flow_fact_count = 0;
    ctx->function_param_flow_fact_capacity = 0;
    ctx->iteration_type_facts = NULL;
    ctx->iteration_type_fact_count = 0;
    ctx->iteration_type_fact_capacity = 0;
    ctx->loop_flow_summary_facts = NULL;
    ctx->loop_flow_summary_fact_count = 0;
    ctx->loop_flow_summary_fact_capacity = 0;
    ctx->loop_flow_state_facts = NULL;
    ctx->loop_flow_state_fact_count = 0;
    ctx->loop_flow_state_fact_capacity = 0;

    semantic_context_destroy(ctx);
    /* Close the registry: types created from here on (there should be none)
     * would have no owner. The result keeps the list itself. */
    type_registry_end(result->owned_types);
    return result;
}

void
semantic_result_destroy(SemanticResult *result)
{
    if (result == NULL)
        return;

    for (size_t i = 0; i < result->diagnostic_count; i++) {
        free(result->diagnostics[i]->message);
        diag_payload_snapshot_destroy(result->diagnostics[i]->payload);
        free(result->diagnostics[i]);
    }
    free(result->diagnostics);
    free(result->lifecycle_state_spaces);
    semantic_parallel_capture_facts_clear(
        result->parallel_capture_boundaries,
        result->parallel_capture_boundary_count);
    pgy_resource_flow_facts_destroy(
        result->resource_flow_facts,
        result->resource_flow_fact_count);
    pgy_function_param_flow_facts_destroy(
        result->function_param_flow_facts);
    pgy_iteration_type_facts_destroy(
        result->iteration_type_facts,
        result->iteration_type_fact_count);
    free(result->loop_flow_summary_facts);
    free(result->loop_flow_state_facts);
    /* Last: the IRs and the backend borrow Type* from here, and the driver
     * destroys them before this result. */
    type_registry_destroy(result->owned_types);
    free(result);
}

size_t
semantic_result_type_resolution_metadata_entries(
        const SemanticResult *result)
{
    return result != NULL ? result->type_resolution_metadata_entries : 0;
}

size_t
semantic_result_type_resolution_metadata_hits(
        const SemanticResult *result)
{
    return result != NULL ? result->type_resolution_metadata_hits : 0;
}

size_t
semantic_result_type_resolution_metadata_dead_ends(
        const SemanticResult *result)
{
    return result != NULL ? result->type_resolution_metadata_dead_ends : 0;
}

size_t
semantic_result_dag_generic_contract_evidence_count(
        const SemanticResult *result)
{
    return result != NULL
        ? result->type_resolution_dag_generic_contract_evidence_count
        : 0;
}

size_t
semantic_result_dag_ability_consumer_evidence_count(
        const SemanticResult *result)
{
    return result != NULL
        ? result->type_resolution_dag_ability_consumer_evidence_count
        : 0;
}

size_t
semantic_result_lifecycle_state_space_count(const SemanticResult *result)
{
    return result != NULL ? result->lifecycle_state_space_count : 0;
}

const LcSpec *
semantic_result_lifecycle_state_space_at(const SemanticResult *result,
                                         size_t index)
{
    if (result == NULL || index >= result->lifecycle_state_space_count)
        return NULL;
    return &result->lifecycle_state_spaces[index];
}

size_t
semantic_result_parallel_capture_boundary_count(const SemanticResult *result)
{
    return result != NULL ? result->parallel_capture_boundary_count : 0;
}

const SemanticParallelCaptureBoundaryFact *
semantic_result_parallel_capture_boundary_at(const SemanticResult *result,
                                             size_t index)
{
    if (result == NULL || index >= result->parallel_capture_boundary_count)
        return NULL;
    return &result->parallel_capture_boundaries[index];
}

void
semantic_result_print(const SemanticResult *result)
{
    if (result == NULL)
        return;

    for (size_t i = 0; i < result->diagnostic_count; i++) {
        Diagnostic *d = result->diagnostics[i];
        const char *level = (d->level == DIAG_ERROR) ? "ERROR" : "WARNING";
        fprintf(stderr, "[%s] %u:%u - %s\n",
                level, d->line, d->col, d->message);
    }

    fprintf(stderr, "\n%zu error(s), %zu warning(s)\n",
            result->error_count, result->warning_count);
}
