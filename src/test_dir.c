/*
 * Copyright (c) 2026 Pergyra Language Project
 * DIR lowering test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/dir.h"

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("ok\n"); g_pass++; } \
        else      { printf("FAIL (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

static DIRProgram *
lower_dir_from_source(const char *source)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    char *dir_error = NULL;
    DIRProgram *dir = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success)
        dir = dir_lower(sem->annotated_ast, &dir_error);

    if (dir == NULL && dir_error != NULL)
        fprintf(stderr, "DIR lowering error: %s\n", dir_error);

    free(dir_error);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return dir;
}

static bool
dir_has_node(const DIRProgram *dir, DIRNodeKind kind, const char *name)
{
    if (dir == NULL || name == NULL)
        return false;

    for (size_t i = 0; i < dir->node_count; i++) {
        if (dir->nodes[i].kind == kind
            && dir->nodes[i].name != NULL
            && strcmp(dir->nodes[i].name, name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
dir_has_edge(const DIRProgram *dir,
             DIREdgeKind kind,
             size_t from_kind_filter,
             size_t to_kind_filter,
             const char *label,
             const char *target_name)
{
    if (dir == NULL)
        return false;

    for (size_t i = 0; i < dir->edge_count; i++) {
        const DIREdge *edge = &dir->edges[i];
        if (edge->kind != kind)
            continue;
        if (from_kind_filter != SIZE_MAX
            && dir->nodes[edge->from_node_id].kind != (DIRNodeKind)from_kind_filter)
            continue;
        if (to_kind_filter != SIZE_MAX) {
            if (edge->to_node_id == SIZE_MAX)
                continue;
            if (dir->nodes[edge->to_node_id].kind != (DIRNodeKind)to_kind_filter)
                continue;
        }
        if (label != NULL) {
            if (edge->label == NULL || strcmp(edge->label, label) != 0)
                continue;
        }
        if (target_name != NULL) {
            if (edge->target_name == NULL || strcmp(edge->target_name, target_name) != 0)
                continue;
        }
        return true;
    }

    return false;
}

static void
test_dir_resource_flow_universe_carriage(void)
{
    const char *source = "func Main() -> Void { Log(1); }\n";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    PgyResourceFlowFact facts[2] = {
        {
            .function_syntax_id = 42,
            .stable_index = 0,
            .declaration_syntax_id = 100,
            .line = 1,
            .column = 25,
            .symbol_kind = 7,
            .is_parameter = false,
            .parameter_index = 0,
            .name = (char *)"slot",
        },
        {
            .function_syntax_id = 42,
            .stable_index = 1,
            .declaration_syntax_id = 101,
            .line = 1,
            .column = 30,
            .symbol_kind = 7,
            .is_parameter = false,
            .parameter_index = 0,
            .name = (char *)"future",
        },
    };
    char *error = NULL;
    DIRProgram *dir = NULL;

    TEST("DIR carries semantic ResourceFlowUniverse rows by stable identity");
    if (!parser_has_error(parser)) {
        dir = dir_lower_with_resource_flow_facts(
            ast, facts, 2, &error);
    }
    EXPECT(dir != NULL
           && dir->has_resource_flow_facts
           && dir->resource_flow_fact_count == 2
           && dir_validate(dir, &error));
    EXPECT(dir != NULL
           && dir->resource_flow_facts[0].name != facts[0].name
           && strcmp(dir->resource_flow_facts[0].name, "slot") == 0
           && dir->resource_flow_facts[0].stable_index == 0
           && dir->resource_flow_facts[1].stable_index == 1);
    dir_destroy(dir);
    dir = NULL;
    free(error);
    error = NULL;

    TEST("DIR rejects a missing ResourceFlowUniverse fact array");
    dir = dir_lower_with_resource_flow_facts(ast, NULL, 1, &error);
    EXPECT(dir == NULL
           && error != NULL
           && strstr(error, "ResourceFlowUniverse") != NULL);
    dir_destroy(dir);
    free(error);
    error = NULL;

    TEST("DIR rejects duplicate function/stable resource identities");
    facts[1].stable_index = facts[0].stable_index;
    dir = dir_lower_with_resource_flow_facts(ast, facts, 2, &error);
    EXPECT(dir != NULL && !dir_validate(dir, &error));
    dir_destroy(dir);
    free(error);
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);

    printf("[dir-resource-flow] semantic ResourceFlowUniverse rows are copied, validated, and fail closed\n");
}

#include "tests/dir/test_dir_lowering.cases.h"

int
main(void)
{
    printf("=== Pergyra DIR Lowering Test Suite ===\n");
    test_dir_lowering();
    test_dir_resource_flow_universe_carriage();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
