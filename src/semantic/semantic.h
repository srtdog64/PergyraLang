/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Semantic analyzer unified interface.
 * Runs semantic passes and returns an annotated AST plus source-of-truth
 * counters consumed by later compiler stages.
 */

#ifndef PERGYRA_SEMANTIC_H
#define PERGYRA_SEMANTIC_H

#include <stdbool.h>
#include <stddef.h>
#include "../parser/ast.h"
#include "diagnostic_types.h"
#include "boundary_witness.h"
#include "lifecycle_state.h"
#include "parallel_capture_facts.h"
#include "resource_flow_fact.h"
#include "loop_flow_fact.h"
#include "function_param_flow_fact.h"
#include "iteration_type_fact.h"
#include "destructure_type_fact.h"
#include "match_binding_type_fact.h"
#include "region_escape_fact.h"
#include "type_system.h"

/*
 * Result of semantic analysis
 */
typedef struct SemanticResult
{
    bool         success;
    ASTNode*     annotated_ast;     /* Same AST, with Type* attached */
    Diagnostic** diagnostics;
    size_t       diagnostic_count;
    size_t       error_count;
    size_t       warning_count;
    size_t       advisory_count;   /* DIAG_ADVISORY — non-blocking, docs/140 */
    size_t       type_resolution_metadata_entries;
    size_t       type_resolution_metadata_hits;
    size_t       type_resolution_metadata_dead_ends;
    size_t       type_resolution_dag_generic_contract_evidence_count;
    size_t       type_resolution_dag_ability_consumer_evidence_count;
    PgyBoundaryWitnessSummary boundary_witness_summary;
    /* Sound, interprocedurally-inferred capability manifest: the union of every
     * PGY_CAP_* the program can exercise (the `--capability-manifest` artifact).
     * Best-effort lower bound w.r.t. dynamic dispatch; the runtime gate backstops. */
    uint32_t     program_capabilities;
    /* Snapshot of semantic lifecycle state-space facts. This is the source of
     * truth for AIR state-space exploration/fuzz manifests: downstream stages
     * must consume these facts instead of rebuilding lifecycle transitions from
     * AST source text. */
    LcSpec      *lifecycle_state_spaces;
    size_t       lifecycle_state_space_count;
    /* Stable boundary-ID concurrency facts. Backends consume the MIR
     * projection of this table, never checker annotations on AST nodes. */
    SemanticParallelCaptureBoundaryFact *parallel_capture_boundaries;
    size_t       parallel_capture_boundary_count;
    /* Function-local ResourceFlowUniverse rows copied before semantic scope
     * teardown. HIR consumes these stable identities instead of rebuilding
     * them from Symbol pointers. */
    PgyResourceFlowFact *resource_flow_facts;
    size_t       resource_flow_fact_count;
    /* Immutable whole-loop transfer rows.  State ranges are keyed by the
     * function-local ResourceFlowUniverse stable index. */
    PgyLoopFlowSummaryFact *loop_flow_summary_facts;
    size_t                  loop_flow_summary_fact_count;
    PgyLoopFlowStateFact *loop_flow_state_facts;
    size_t                 loop_flow_state_fact_count;
    /* Immutable snapshot of demanded interprocedural parameter summaries. */
    PgyFunctionParamFlowFact *function_param_flow_facts;
    size_t       function_param_flow_fact_count;
    /* Semantic-owned for-loop header facts. MIR source-local capture consumes
     * these rows; it must not infer a binding type from AST expressions. */
    PgyIterationTypeFact *iteration_type_facts;
    size_t       iteration_type_fact_count;
    /* Positional destructure binding types keyed by stable syntax identity. */
    PgyDestructureTypeFact *destructure_type_facts;
    size_t       destructure_type_fact_count;
    /* Positional match-pattern binding types keyed by stable case identity. */
    PgyMatchBindingTypeFact *match_binding_type_facts;
    size_t       match_binding_type_fact_count;
    /* Semantic-owned bounded region escape facts. The driver converts these
     * stable rows into the verified plan; it must not rescan the AST. */
    PgyRegionEscapeFact *region_escape_facts;
    size_t               region_escape_fact_count;
    /* Owns every Type this analysis allocated (WO-SEC-2). Borrowed Type*
     * held by symbols, IRs, or the backend stay valid until this result is
     * destroyed, which the driver does last. */
    TypeRegistry *owned_types;
} SemanticResult;

/* -----------------------------------------------------------------
 * Entry point
 *
 * Usage:
 *   ASTNode* ast = parser_parse(source);
 *   SemanticResult* result = semantic_analyze(ast);
 *
 *   if (!result->success) {
 *       semantic_result_print(result);
 *       return 1;
 *   }
 *
 *   codegen_generate(result->annotated_ast);
 *   semantic_result_destroy(result);
 * ----------------------------------------------------------------- */

SemanticResult* semantic_analyze(ASTNode* ast);
/* Like semantic_analyze, but with non-blocking meaning-axis advisories (docs/140)
 * enabled. Off by default (semantic_analyze) so batch/CI compiles pay nothing;
 * the LSP/editor path passes true. */
SemanticResult* semantic_analyze_ex(ASTNode* ast, bool emit_advisories);
void            semantic_result_destroy(SemanticResult* result);
void            semantic_result_print(const SemanticResult* result);
size_t          semantic_result_type_resolution_metadata_entries(
                    const SemanticResult* result);
size_t          semantic_result_type_resolution_metadata_hits(
                    const SemanticResult* result);
size_t          semantic_result_type_resolution_metadata_dead_ends(
                    const SemanticResult* result);
size_t          semantic_result_dag_generic_contract_evidence_count(
                    const SemanticResult* result);
size_t          semantic_result_dag_ability_consumer_evidence_count(
                    const SemanticResult* result);
size_t          semantic_result_lifecycle_state_space_count(
                    const SemanticResult* result);
const LcSpec*   semantic_result_lifecycle_state_space_at(
                    const SemanticResult* result,
                    size_t index);
size_t          semantic_result_parallel_capture_boundary_count(
                    const SemanticResult *result);
const SemanticParallelCaptureBoundaryFact *
                semantic_result_parallel_capture_boundary_at(
                    const SemanticResult *result,
                    size_t index);

/*
 * Emit diagnostics as a JSON array to stderr.
 * Each entry: {
 *   "severity": "error" | "warning",
 *   "stage": "semantic",
 *   "layer": "type" | "resource" | "concurrency" | "domain" | ...,
 *   "code": "PGY_..."?,
 *   "cause_ir": "<stage>:<subsystem>:<condition>"?,
 *   "fix_source": "<stable-fix-token>"?,
 *   "location": {"line": N, "column": M},
 *   "message": "..."
 * }
 * Trailing newline appended. Produces an empty array ("[]") if there are
 * no diagnostics. Parseable with any standard JSON library.
 */
void            semantic_result_print_json(const SemanticResult* result);

#endif /* PERGYRA_SEMANTIC_H */
