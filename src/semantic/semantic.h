/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Semantic Analyzer — unified interface
 * Runs all three passes and returns an annotated AST.
 */

#ifndef PERGYRA_SEMANTIC_H
#define PERGYRA_SEMANTIC_H

#include <stdbool.h>
#include "../parser/ast.h"
#include "../semantic/type_checker.h"

/*
 * Result of semantic analysis
 */
typedef struct
{
    bool         success;
    ASTNode*     annotated_ast;     /* Same AST, with Type* attached */
    Diagnostic** diagnostics;
    size_t       diagnostic_count;
    size_t       error_count;
    size_t       warning_count;
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

#endif /* PERGYRA_SEMANTIC_H */
