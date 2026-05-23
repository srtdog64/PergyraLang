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
    size_t       type_resolution_metadata_entries;
    size_t       type_resolution_metadata_hits;
    size_t       type_resolution_metadata_dead_ends;
    size_t       type_resolution_dag_generic_contract_evidence_count;
    size_t       type_resolution_dag_ability_consumer_evidence_count;
    /* Compatibility metric names kept for existing smoke/stat parsers. New
     * code should consume the type_resolution_dag_*_evidence_count fields. */
    size_t       type_resolution_stage_compat_generic_contract_count;
    size_t       type_resolution_stage_compat_ability_consumer_count;
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
