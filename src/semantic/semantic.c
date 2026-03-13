/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Semantic Analyzer — unified entry point
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "semantic.h"
#include "type_checker.h"
#include "slot_analyzer.h"

SemanticResult *
semantic_analyze(ASTNode *ast)
{
    SemanticResult *result = calloc(1, sizeof(SemanticResult));
    if (result == NULL)
        return NULL;

    SemanticContext *ctx = semantic_context_create();
    if (ctx == NULL) {
        free(result);
        return NULL;
    }

    /* Pass 1 + 2: Symbol collection and type checking */
    type_check_program(ast, ctx);

    /* Pass 3: Slot lifetime analysis */
    if (!ctx->has_error) {
        SlotAnalyzer *sa = slot_analyzer_create(ctx);
        if (sa != NULL) {
            slot_analyze_program(ast, sa);
            slot_analyzer_destroy(sa);
        }
    }

    /* Transfer diagnostics to result */
    result->success          = !ctx->has_error;
    result->annotated_ast    = ast;
    result->diagnostic_count = ctx->diagnostic_count;
    result->diagnostics      = ctx->diagnostics;

    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        if (ctx->diagnostics[i]->level == DIAG_ERROR)
            result->error_count++;
        else
            result->warning_count++;
    }

    /*
     * Transfer ownership of diagnostics array to result.
     * Set ctx pointer to NULL so context_destroy doesn't double-free.
     */
    ctx->diagnostics      = NULL;
    ctx->diagnostic_count = 0;
    ctx->diagnostic_capacity = 0;

    semantic_context_destroy(ctx);
    return result;
}

void
semantic_result_destroy(SemanticResult *result)
{
    if (result == NULL)
        return;

    for (size_t i = 0; i < result->diagnostic_count; i++) {
        free(result->diagnostics[i]->message);
        free(result->diagnostics[i]);
    }
    free(result->diagnostics);
    free(result);
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
