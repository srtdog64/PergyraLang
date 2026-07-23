/*
 * Copyright (c) 2025 Pergyra Language Project
 * HIR lowering test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "parser/ast_api.h"
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
        if (cond) { printf("ok\n"); g_pass++; } \
        else      { printf("FAIL (line %d)\n", __LINE__); g_fail++; } \
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

static void
test_hir_function_param_flow_carriage(void)
{
    const char *source =
        "func Param(value: Int) -> Int { return value; }\n";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    ASTNode *function_decl = NULL;
    PgyFunctionParamFlowFact fact;
    HIRProgram *hir = NULL;
    char *error_message = NULL;
    bool carried = false;

    if (sem != NULL && sem->annotated_ast != NULL) {
        for (size_t i = 0;
             i < ast_program_statement_count(sem->annotated_ast);
             i++) {
            ASTNode *statement = ast_program_statement(
                sem->annotated_ast, i);
            if (statement != NULL && statement->type == AST_FUNC_DECL) {
                function_decl = statement;
                break;
            }
        }
    }
    memset(&fact, 0, sizeof(fact));
    fact.function_syntax_id = ast_node_stable_id(function_decl);
    fact.parameter_index = 0;
    fact.mask = 0x5u;
    if (!parser_has_error(parser) && sem != NULL && sem->success
        && function_decl != NULL && fact.function_syntax_id != 0) {
        hir = hir_lower_with_resource_and_param_flow_facts(
            sem->annotated_ast, NULL, 0, &fact, 1, &error_message);
        if (hir != NULL && hir_validate(hir, &error_message)) {
            for (size_t i = 0; i < hir->routine_count; i++) {
                const HIRRoutine *routine = &hir->routines[i];
                if (routine->source_syntax_id == fact.function_syntax_id
                    && routine->function_param_flow_summary_count == 1
                    && routine->function_param_flow_summaries[0].parameter_index == 0
                    && routine->function_param_flow_summaries[0].mask == fact.mask) {
                    carried = true;
                    break;
                }
            }
        }
    }
    TEST("HIR carries function parameter flow summaries by stable SyntaxNodeId");
    EXPECT(carried);
    if (hir != NULL)
        hir_destroy(hir);
    free(error_message);
    semantic_result_destroy(sem);
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
}

static void
test_hir_region_escape_fact_carriage(void)
{
    const char *source =
        "func Main() -> Void { Print(\"a\" + \"b\"); }\n";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    HIRProgram *projected = NULL;
    HIRProgram *unprojected = NULL;
    char *projected_error = NULL;
    char *unprojected_error = NULL;
    bool carried = false;
    bool identity_rejected = false;

    if (!parser_has_error(parser) && sem != NULL && sem->success
        && sem->region_escape_fact_count == 1) {
        projected = hir_lower_with_semantic_facts(
            sem, NULL, &projected_error);
        if (projected != NULL && hir_validate(projected, &projected_error)) {
            const PgyRegionEscapeFact *fact = projected->region_escape_facts;
            carried = projected->has_region_escape_facts
                && projected->region_escape_fact_count == 1
                && fact != NULL
                && fact->allocation_site_id != 0
                && fact->scope_id != 0
                && fact->function_syntax_id != 0
                && fact != sem->region_escape_facts;
        }

        unprojected = hir_lower(sem->annotated_ast, &unprojected_error);
        if (unprojected != NULL) {
            PgyRegionEscapeFact forged = sem->region_escape_facts[0];
            forged.function_syntax_id = UINT32_MAX;
            identity_rejected = !hir_attach_region_escape_facts(
                unprojected, &forged, 1, &unprojected_error);
        }
    }
    TEST("HIR carries semantic region rows as owned stable facts");
    EXPECT(carried);
    TEST("HIR rejects region rows with unknown function identity");
    EXPECT(identity_rejected);
    if (projected != NULL)
        hir_destroy(projected);
    if (unprojected != NULL)
        hir_destroy(unprojected);
    free(projected_error);
    free(unprojected_error);
    semantic_result_destroy(sem);
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
}

#include "tests/hir/test_hir_lowering_part_a.cases.h"
#include "tests/hir/test_hir_lowering_part_b.cases.h"

int
main(void)
{
    printf("=== Pergyra HIR Lowering Test Suite ===\n");
    test_hir_lowering_part_a();
    test_hir_lowering_part_b();
    test_hir_function_param_flow_carriage();
    test_hir_region_escape_fact_carriage();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
