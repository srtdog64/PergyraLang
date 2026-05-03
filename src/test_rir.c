/*
 * Copyright (c) 2026 Pergyra Language Project
 * RIR lowering test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/dir.h"
#include "compiler/hir.h"
#include "compiler/rir.h"

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("ok\n"); g_pass++; } \
        else      { printf("FAIL (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

static bool
lower_rir_from_source(const char *source, HIRProgram **hir_out, RIRProgram **rir_out)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    char *hir_error = NULL;
    char *rir_error = NULL;
    *hir_out = NULL;
    *rir_out = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success) {
        *hir_out = hir_lower(sem->annotated_ast, &hir_error);
        *rir_out = rir_lower(sem->annotated_ast, &rir_error);
        if (*hir_out != NULL && *rir_out != NULL)
            (void)rir_enrich_with_hir_flow(*rir_out, *hir_out, &rir_error);
    }

    if ((*hir_out == NULL || *rir_out == NULL) && (hir_error != NULL || rir_error != NULL)) {
        if (hir_error != NULL)
            fprintf(stderr, "HIR lowering error: %s\n", hir_error);
        if (rir_error != NULL)
            fprintf(stderr, "RIR lowering error: %s\n", rir_error);
    }

    free(hir_error);
    free(rir_error);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return *hir_out != NULL && *rir_out != NULL;
}

static bool
lower_dir_rir_from_source(const char *source,
                          DIRProgram **dir_out,
                          HIRProgram **hir_out,
                          RIRProgram **rir_out)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    char *dir_error = NULL;
    char *hir_error = NULL;
    char *rir_error = NULL;
    *dir_out = NULL;
    *hir_out = NULL;
    *rir_out = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success) {
        *dir_out = dir_lower(sem->annotated_ast, &dir_error);
        *hir_out = hir_lower(sem->annotated_ast, &hir_error);
        *rir_out = rir_lower(sem->annotated_ast, &rir_error);
        if (*hir_out != NULL && *rir_out != NULL)
            (void)rir_enrich_with_hir_flow(*rir_out, *hir_out, &rir_error);
    }

    if ((*dir_out == NULL || *hir_out == NULL || *rir_out == NULL)
        && (dir_error != NULL || hir_error != NULL || rir_error != NULL)) {
        if (dir_error != NULL)
            fprintf(stderr, "DIR lowering error: %s\n", dir_error);
        if (hir_error != NULL)
            fprintf(stderr, "HIR lowering error: %s\n", hir_error);
        if (rir_error != NULL)
            fprintf(stderr, "RIR lowering error: %s\n", rir_error);
    }

    free(dir_error);
    free(hir_error);
    free(rir_error);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return *dir_out != NULL && *hir_out != NULL && *rir_out != NULL;
}

static bool
scope_has_op(const RIRScope *scope, RIROpKind kind)
{
    for (size_t i = 0; i < scope->op_count; i++) {
        if (scope->ops[i].kind == kind)
            return true;
    }
    return false;
}

static bool
scope_has_op_subject(const RIRScope *scope, RIROpKind kind, const char *subject)
{
    for (size_t i = 0; i < scope->op_count; i++) {
        if (scope->ops[i].kind == kind
            && scope->ops[i].subject != NULL
            && strcmp(scope->ops[i].subject, subject) == 0)
            return true;
    }
    return false;
}

static bool
scope_has_resource_fact(const RIRScope *scope, const char *name, RIRResourceKind kind)
{
    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->kind == RIR_FACT_RESOURCE
            && fact->name != NULL
            && strcmp(fact->name, name) == 0
            && fact->resource_kind == kind) {
            return true;
        }
    }
    return false;
}

static bool
scope_has_fact_slot_anchor(const RIRScope *scope,
                           RIRFactKind kind,
                           const char *name,
                           const char *slot_anchor)
{
    if (scope == NULL)
        return false;
    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->kind == kind
            && fact->name != NULL
            && strcmp(fact->name, name) == 0
            && fact->slot_anchor != NULL
            && strcmp(fact->slot_anchor, slot_anchor) == 0) {
            return true;
        }
    }
    return false;
}

static bool
scope_has_projection_fact_kind(const RIRScope *scope,
                               const char *name,
                               RIRResourceKind kind,
                               RIRResourceState state)
{
    if (scope == NULL)
        return false;
    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->kind == RIR_FACT_PROJECTION
            && fact->name != NULL
            && strcmp(fact->name, name) == 0
            && fact->resource_kind == kind
            && fact->state == state) {
            return true;
        }
    }
    return false;
}

static bool
scope_has_op_slot_anchor(const RIRScope *scope, RIROpKind kind, const char *slot_anchor)
{
    if (scope == NULL)
        return false;
    for (size_t i = 0; i < scope->op_count; i++) {
        const RIROp *op = &scope->ops[i];
        if (op->kind == kind
            && op->slot_anchor != NULL
            && strcmp(op->slot_anchor, slot_anchor) == 0) {
            return true;
        }
    }
    return false;
}

static const RIRStateSummary *
scope_find_state_summary(const RIRScope *scope, const char *name)
{
    if (scope == NULL)
        return NULL;
    for (size_t i = 0; i < scope->state_summary_count; i++) {
        if (scope->state_summaries[i].name != NULL
            && strcmp(scope->state_summaries[i].name, name) == 0) {
            return &scope->state_summaries[i];
        }
    }
    return NULL;
}

static const RIRScope *
find_scope(const RIRProgram *rir, const char *name, RIRScopeKind kind)
{
    if (rir == NULL)
        return NULL;
    for (size_t i = 0; i < rir->scope_count; i++) {
        if (rir->scopes[i].kind == kind
            && rir->scopes[i].name != NULL
            && strcmp(rir->scopes[i].name, name) == 0) {
            return &rir->scopes[i];
        }
    }
    return NULL;
}

static bool
scope_has_flow_semantics(const RIRScope *scope, unsigned int flags)
{
    if (scope == NULL)
        return false;
    for (size_t i = 0; i < scope->flow_block_count; i++) {
        const RIRFlowBlock *block = &scope->flow_blocks[i];
        if ((block->entry_semantics & flags) == flags
            || (block->exit_semantics & flags) == flags) {
            return true;
        }
    }
    return false;
}

static bool
scope_has_conservative_semantics(const RIRScope *scope, unsigned int flags)
{
    return scope != NULL && (scope->conservative_semantics & flags) == flags;
}

#include "tests/rir/test_rir_lowering.cases.h"

int
main(void)
{
    printf("=== Pergyra RIR Lowering Test Suite ===\n");
    test_rir_lowering();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
