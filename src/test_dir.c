/*
 * Copyright (c) 2026 Pergyra Language Project
 * DIR lowering test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/ast_api.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/dir.h"
#include "compiler/hir.h"

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
    ASTNode *function = ast_program_statement(ast, 0);
    PgyResourceFlowFact facts[2] = {
        {
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
    HIRProgram *hir = NULL;

    if (function != NULL) {
        facts[0].function_syntax_id = ast_node_stable_id(function);
        facts[1].function_syntax_id = ast_node_stable_id(function);
    }
    TEST("HIR owns semantic ResourceFlowUniverse rows by stable identity");
    if (!parser_has_error(parser) && function != NULL) {
        hir = hir_lower_with_resource_flow_facts(ast, facts, 2, &error);
        if (hir != NULL && hir_validate(hir, &error))
            dir = dir_lower_with_hir_facts(ast, hir, &error);
    }
    EXPECT(hir != NULL && dir != NULL && dir_validate(dir, &error));
    EXPECT(hir != NULL && hir->routine_count == 1
           && hir->routines[0].resource_flow_symbol_count == 2
           && hir->routines[0].resource_flow_symbols[0].name != facts[0].name
           && strcmp(hir->routines[0].resource_flow_symbols[0].name,
                     "slot") == 0
           && hir->routines[0].resource_flow_symbols[0].stable_index == 0
           && hir->routines[0].resource_flow_symbols[1].stable_index == 1);
    dir_destroy(dir);
    dir = NULL;
    hir_destroy(hir);
    hir = NULL;
    free(error);
    error = NULL;

    TEST("HIR rejects a missing ResourceFlowUniverse fact array");
    hir = hir_lower_with_resource_flow_facts(ast, NULL, 1, &error);
    EXPECT(hir == NULL
           && error != NULL
           && strstr(error, "ResourceFlowUniverse") != NULL);
    hir_destroy(hir);
    free(error);
    error = NULL;

    TEST("HIR rejects duplicate routine-local resource identities");
    facts[1].stable_index = facts[0].stable_index;
    hir = hir_lower_with_resource_flow_facts(ast, facts, 2, &error);
    EXPECT(hir != NULL && !hir_validate(hir, &error));
    hir_destroy(hir);
    free(error);
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);

    printf("[dir-resource-flow] HIR owns validated routine-local rows; DIR carries no duplicate snapshot\n");
}

static void
test_dir_domain_runtime_topology(void)
{
    const char *source =
        "subject Player { let hp: Int; }\n"
        "relation TrustedLink for source: Player, target: Player { }\n"
        "zone BattleZone {\n"
        "    subject slot player: Player\n"
        "    subject slot enemy: Player\n"
        "    relation slot trust: TrustedLink\n"
        "    link trust between player, enemy\n"
        "}\n"
        "func Main() -> Void { return; }\n";
    DIRProgram *dir = lower_dir_from_source(source);
    DIRDomainTopologyRow *row = NULL;
    char *error = NULL;
    uint32_t saved_left_id = 0;
    size_t saved_count = 0;
    bool carried = false;
    bool rejected_bad_identity = false;
    bool rejected_missing_row = false;

    if (dir != NULL && dir->domain_topology_row_count == 1) {
        row = &dir->domain_topology_rows[0];
        carried = row->kind == DIR_DOMAIN_TOPOLOGY_LINK_RELATION
            && row->owner_source_syntax_id != 0
            && row->source_syntax_id != 0
            && row->layer_slot_source_syntax_id != 0
            && row->left_slot_source_syntax_id != 0
            && row->right_slot_source_syntax_id != 0
            && strcmp(row->layer_slot_name, "trust") == 0
            && strcmp(row->left_slot_name, "player") == 0
            && strcmp(row->right_slot_name, "enemy") == 0
            && dir_validate(dir, &error);
        free(error);
        error = NULL;

        saved_left_id = row->left_slot_source_syntax_id;
        row->left_slot_source_syntax_id = 0;
        rejected_bad_identity = !dir_validate(dir, &error)
            && error != NULL
            && strstr(error, "invalid link-relation shape") != NULL;
        free(error);
        error = NULL;
        row->left_slot_source_syntax_id = saved_left_id;

        saved_count = dir->domain_topology_row_count;
        dir->domain_topology_row_count = 0;
        rejected_missing_row = !dir_validate(dir, &error)
            && error != NULL
            && strstr(error, "does not match source-owned count") != NULL;
        dir->domain_topology_row_count = saved_count;
    }

    TEST("DIR owns link topology with stable slot identities");
    EXPECT(carried);
    TEST("DIR rejects damaged or missing domain topology rows");
    EXPECT(rejected_bad_identity && rejected_missing_row);
    free(error);
    dir_destroy(dir);
}

#include "tests/dir/test_dir_lowering.cases.h"

int
main(void)
{
    printf("=== Pergyra DIR Lowering Test Suite ===\n");
    test_dir_lowering();
    test_dir_resource_flow_universe_carriage();
    test_dir_domain_runtime_topology();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
