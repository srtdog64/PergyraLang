/*
 * Copyright (c) 2025 Pergyra Language Project
 * HIR lowering test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/hir.h"

static int g_pass = 0;
static int g_fail = 0;

typedef struct {
    size_t seen;
    size_t with_calls;
} HIRPassCounter;

typedef struct {
    size_t reachable_blocks;
    size_t blocks_with_phi;
    size_t blocks_with_dom_children;
} HIRBlockCounter;

#define TEST(name) \
    do { printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("??n"); g_pass++; } \
        else      { printf("?? (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

static HIRProgram *
lower_from_source(const char *source)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    char *hir_error = NULL;
    HIRProgram *hir = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success)
        hir = hir_lower(sem->annotated_ast, &hir_error);

    if (hir == NULL && hir_error != NULL)
        fprintf(stderr, "HIR lowering error: %s\n", hir_error);

    free(hir_error);
    parser_destroy(parser);
    lexer_destroy(lexer);
    /* HIR still references the annotated AST, so this helper intentionally
     * keeps semantic/AST memory alive for the duration of the test process.
     * The real driver destroys HIR before semantic/AST teardown. */
    return hir;
}

static bool
count_routines_pass(const HIRProgram *hir, const HIRRoutine *routine, void *userdata, char **error_message)
{
    (void)hir;
    (void)error_message;
    HIRPassCounter *counter = (HIRPassCounter *)userdata;
    counter->seen++;
    if (routine->direct_call_count > 0)
        counter->with_calls++;
    return true;
}

static bool
count_blocks_pass(const HIRProgram *hir,
                  const HIRRoutine *routine,
                  const HIRBasicBlock *block,
                  void *userdata,
                  char **error_message)
{
    (void)hir;
    (void)routine;
    (void)error_message;
    HIRBlockCounter *counter = (HIRBlockCounter *)userdata;
    if (block->is_reachable)
        counter->reachable_blocks++;
    if (block->phi_node_count > 0)
        counter->blocks_with_phi++;
    if (block->dom_tree_child_count > 0)
        counter->blocks_with_dom_children++;
    return true;
}

#include "tests/hir/test_hir_lowering.cases.h"

int
main(void)
{
    printf("=== Pergyra HIR Lowering Test Suite ===\n");
    test_hir_lowering();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
