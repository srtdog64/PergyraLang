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
#include "semantic/builtin_kind.h"
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

/* A summary row whose function identity names no HIR routine must stop
 * lowering; HIR may not drop the row or attach it to a routine by name. */
static void
test_hir_function_param_flow_rejects_unknown_routine(void)
{
    const char *source =
        "func Param(value: Int) -> Int { return value; }\n";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    PgyFunctionParamFlowFact fact;
    HIRProgram *hir = NULL;
    char *error_message = NULL;
    bool rejected = false;

    memset(&fact, 0, sizeof(fact));
    fact.function_syntax_id = UINT32_MAX;
    fact.parameter_index = 0;
    fact.mask = 0x5u;
    if (!parser_has_error(parser) && sem != NULL && sem->success) {
        hir = hir_lower_with_resource_and_param_flow_facts(
            sem->annotated_ast, NULL, 0, &fact, 1, &error_message);
        rejected = hir == NULL && error_message != NULL
            && strstr(error_message,
                   "Function parameter flow fact references an unknown HIR routine")
                != NULL;
    }
    TEST("HIR rejects a function parameter flow fact for an unknown routine");
    EXPECT(rejected);
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

static void
test_hir_map_keys_transfer_carriage(void)
{
    const char *sources[] = {
        "func Main() -> Void { let values: HashMap<String, Int> = MapNew();"
        " MapSet(values, \"owned\", 1); let keys: Array<String> = MapKeys(values);"
        " let alias: Array<String> = keys; Log(ArrayLength(alias)); }",
        "func Main() -> Void { let values: HashMap<String, Int> = MapNew();"
        " MapSet(values, \"owned\", 1); let keys: Array<String> = MapKeys(values);"
        " let alias: Array<String> = keys; Log(ArrayLength(alias));"
        " ArrayDropOwnedStrings(alias); }"
    };
    const char *fault_names[] = {
        "HIR transfer rejects missing source identity",
        "HIR transfer rejects self source identity",
        "HIR transfer rejects foreign source identity",
        "HIR transfer rejects mismatched source ownership",
        "HIR transfer rejects invalid destination disposition",
        "HIR transfer rejects a live transfer source",
        "HIR transfer rejects an unwitnessed final disposition flip",
        "HIR transfer rejects a missing deep-drop receipt",
        "HIR transfer rejects a noncanonical deep-drop call",
        "HIR transfer rejects a wrong deep-drop operation receipt",
        "HIR transfer rejects a stale receiver identity",
        "HIR transfer rejects a deep-drop source identity",
        "HIR transfer rejects removal of the actual deep-drop",
        "HIR transfer rejects duplicate actual deep-drop",
        "HIR transfer rejects missing typed birth result"
    };
    for (size_t variant = 0; variant < 2; variant++) {
        Lexer *lexer = lexer_create(sources[variant]);
        Parser *parser = parser_create(lexer);
        ASTNode *ast = parser_parse_program(parser);
        SemanticResult *sem = semantic_analyze(ast);
        size_t destination = SIZE_MAX, source = SIZE_MAX;
        if (!parser_has_error(parser) && sem != NULL && sem->success) {
            for (size_t i = 0; i < sem->collection_ownership_fact_count; i++) {
                if (sem->collection_ownership_facts[i].origin
                    == PGY_COLLECTION_ORIGIN_BINDING)
                    destination = i;
            }
            if (destination != SIZE_MAX) {
                for (size_t i = 0; i < sem->collection_ownership_fact_count; i++) {
                    if (sem->collection_ownership_facts[i].binding_syntax_id
                        == sem->collection_ownership_facts[destination]
                            .source_binding_syntax_id)
                        source = i;
                }
            }
        }
        bool admitted = destination != SIZE_MAX && source != SIZE_MAX;
        TEST(variant == 0 ? "HIR carries retired MapKeys source to live destination"
                          : "HIR carries retired MapKeys source to deep-dropped destination");
        bool carried = false;
        if (admitted) {
            PgyCollectionOwnershipFact *facts = sem->collection_ownership_facts;
            admitted = facts[source].disposition == PGY_COLLECTION_DISPOSITION_RETIRED
                && facts[destination].disposition == (variant == 0
                    ? PGY_COLLECTION_DISPOSITION_LIVE : PGY_COLLECTION_DISPOSITION_RETIRED)
                && facts[source].element_ownership == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
                && facts[destination].element_ownership == facts[source].element_ownership;
            char *error = NULL;
            HIRSemanticProjectionFailure failure = HIR_SEMANTIC_PROJECTION_NONE;
            HIRProgram *hir = admitted
                ? hir_lower_with_semantic_facts(sem, &failure, &error) : NULL;
            if (hir != NULL && failure == HIR_SEMANTIC_PROJECTION_NONE) {
                for (size_t r = 0; r < hir->routine_count; r++) {
                    const HIRRoutine *routine = &hir->routines[r];
                    bool has_source = false, has_destination = false;
                    for (size_t i = 0; i < routine->collection_ownership_fact_count; i++) {
                        const HIRCollectionOwnershipFact *row = &routine->collection_ownership_facts[i];
                        if (row->binding_syntax_id == facts[source].binding_syntax_id)
                            has_source = row->disposition == facts[source].disposition;
                        if (row->binding_syntax_id == facts[destination].binding_syntax_id)
                            has_destination = row->source_binding_syntax_id == facts[source].binding_syntax_id
                                && row->disposition == facts[destination].disposition
                                && row->element_ownership == facts[destination].element_ownership;
                    }
                    carried = carried || (has_source && has_destination);
                }
            }
            hir_destroy(hir);
            free(error);
        }
        EXPECT(admitted && carried);
        if (admitted) {
            PgyCollectionOwnershipFact *facts = sem->collection_ownership_facts;
            PgyCollectionOwnershipFact saved_source = facts[source];
            PgyCollectionOwnershipFact saved_destination = facts[destination];
            for (size_t fault = 0; fault < (variant == 0 ? 7u : 15u); fault++) {
                char *error = NULL;
                HIRProgram *hir = hir_lower(sem->annotated_ast, &error);
                ASTNode *drop = NULL, *birth = NULL;
                HIRBasicBlock *drop_block = NULL;
                size_t drop_index = 0;
                if (hir != NULL) {
                    for (size_t r = 0; r < hir->routine_count; r++) {
                        for (size_t b = 0; b < hir->routines[r].cfg.block_count; b++) {
                            HIRBasicBlock *block = &hir->routines[r].cfg.blocks[b];
                            for (size_t s = 0; s < block->statement_count; s++) {
                                ASTNode *node = block->statements[s];
                                if (node != NULL && node->type == AST_CALL
                                    && ast_call_semantic_collection_effect(node, NULL, NULL, NULL)) {
                                    drop = node; drop_block = block; drop_index = s;
                                }
                                if (node != NULL && node->type == AST_LET_DECL
                                    && ast_node_stable_id(node) == facts[source].binding_syntax_id)
                                    birth = ast_let_initializer(node);
                            }
                        }
                    }
                }
                ASTNode saved_drop;
                if (drop != NULL) saved_drop = *drop;
                char *saved_type = birth != NULL ? birth->data.call.semantic_value_type_name : NULL;
                if (fault == 0) facts[destination].source_binding_syntax_id = 0;
                if (fault == 1) facts[destination].source_binding_syntax_id = facts[destination].binding_syntax_id;
                if (fault == 2) facts[destination].source_binding_syntax_id = UINT32_MAX;
                if (fault == 3) facts[destination].element_ownership = PGY_STRING_ARRAY_BORROWED_ELEMENTS;
                if (fault == 4) facts[destination].disposition = (PgyCollectionDisposition)99;
                if (fault == 5) facts[source].disposition = PGY_COLLECTION_DISPOSITION_LIVE;
                if (fault == 6) facts[destination].disposition = variant == 0
                    ? PGY_COLLECTION_DISPOSITION_RETIRED : PGY_COLLECTION_DISPOSITION_LIVE;
                if (drop != NULL) {
                    if (fault == 7) drop->data.call.semantic_collection_effect_set = false;
                    if (fault == 8) drop->data.call.semantic_callee_is_stdlib = false;
                    if (fault == 9) drop->data.call.semantic_collection_effect_kind = PGY_COLLECTION_EFFECT_SHALLOW_MUTATION;
                    if (fault == 10) drop->data.call.semantic_collection_receiver_binding_id = UINT32_MAX;
                    if (fault == 11) drop->data.call.semantic_collection_source_binding_id = facts[source].binding_syntax_id;
                    if (fault == 12) drop_block->statements[drop_index] = NULL;
                    if (fault == 13) {
                        ASTNode **grown = realloc(drop_block->statements,
                            (drop_block->statement_count + 1) * sizeof(*grown));
                        if (grown != NULL) {
                            drop_block->statements = grown;
                            drop_block->statements[drop_block->statement_count++] = drop;
                            drop_block->statement_capacity = drop_block->statement_count;
                        }
                    }
                }
                if (fault == 14 && birth != NULL) birth->data.call.semantic_value_type_name = NULL;
                bool rejected = hir != NULL && !hir_attach_collection_ownership_facts(hir,
                    facts, sem->collection_ownership_fact_count, &error) && error != NULL;
                if (drop != NULL) *drop = saved_drop;
                if (birth != NULL) birth->data.call.semantic_value_type_name = saved_type;
                facts[source] = saved_source;
                facts[destination] = saved_destination;
                TEST(fault_names[fault]);
                EXPECT(rejected);
                hir_destroy(hir);
                free(error);
            }
        }
        semantic_result_destroy(sem);
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}

static void
test_hir_owned_snapshot_cfg_generations(void)
{
    const char *sources[] = {
        "func Main() -> Void { let values: HashMap<String, Int> = MapNew();"
        " let index: Int = 0; while index < 2 { let keys: Array<String> = MapKeys(values);"
        " Log(ArrayLength(keys)); index = index + 1; } }",
        "func Main() -> Void { let values: HashMap<String, Int> = MapNew();"
        " let index: Int = 0; while index < 2 { let keys: Array<String> = MapKeys(values);"
        " Log(ArrayLength(keys)); ArrayDropOwnedStrings(keys); index = index + 1; } }",
        "func Main() -> Void { let values: HashMap<String, Int> = MapNew();"
        " if MapSize(values) == 0 { let keys: Array<String> = MapKeys(values);"
        " Log(ArrayLength(keys)); ArrayDropOwnedStrings(keys); } }"
    };
    const char *names[] = {
        "HIR permits canonical fresh loop births without explicit deep-drop",
        "HIR permits balanced per-iteration snapshot deep-drop",
        "HIR permits an unborn branch-local snapshot on the other exit"
    };
    for (size_t variant = 0; variant < 3; variant++) {
        Lexer *lexer = lexer_create(sources[variant]);
        Parser *parser = parser_create(lexer);
        ASTNode *ast = parser_parse_program(parser);
        SemanticResult *sem = semantic_analyze(ast);
        char *error = NULL;
        HIRSemanticProjectionFailure failure = HIR_SEMANTIC_PROJECTION_NONE;
        HIRProgram *hir = !parser_has_error(parser) && sem != NULL && sem->success
            ? hir_lower_with_semantic_facts(sem, &failure, &error) : NULL;
        TEST(names[variant]);
        EXPECT(hir != NULL && failure == HIR_SEMANTIC_PROJECTION_NONE);
        hir_destroy(hir);
        free(error);
        if (variant == 1 && sem != NULL && sem->success) {
            error = NULL;
            hir = hir_lower(sem->annotated_ast, &error);
            bool removed = false;
            if (hir != NULL) {
                for (size_t r = 0; r < hir->routine_count; r++) {
                    for (size_t b = 0; b < hir->routines[r].cfg.block_count; b++) {
                        HIRBasicBlock *block = &hir->routines[r].cfg.blocks[b];
                        for (size_t s = 0; s < block->statement_count; s++) {
                            uint32_t kind = 0;
                            if (ast_call_semantic_collection_effect(block->statements[s], &kind, NULL, NULL)
                                && kind == PGY_COLLECTION_EFFECT_DROP) {
                                block->statements[s] = NULL;
                                removed = true;
                            }
                        }
                    }
                }
            }
            bool rejected = hir != NULL && removed && !hir_attach_collection_ownership_facts(
                hir, sem->collection_ownership_facts, sem->collection_ownership_fact_count, &error);
            TEST("HIR rejects a removed per-iteration drop with retired final facts");
            EXPECT(rejected && error != NULL);
            hir_destroy(hir);
            free(error);
        }
        semantic_result_destroy(sem);
        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}

static void
test_hir_clone_checked_result_carriage(void)
{
    const char *source = "func Main() -> Void { let values: Array<String> = [\"borrowed\"];"
        " let cloned: Array<String> = Clone(values); ArrayDropOwnedStrings(cloned); }";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    ASTNode *clone = NULL;
    if (sem != NULL && sem->success) {
        ASTNode *function = ast_program_statement(sem->annotated_ast, 0);
        clone = ast_let_initializer(ast_block_statement(ast_func_body(function), 1));
    }
    uint32_t builtin = 0;
    const char *actual_type = ast_call_semantic_value_type_name(clone);
    TEST("Clone publishes checked argument result and canonical builtin identity");
    EXPECT(clone != NULL && ast_call_semantic_callee_is_stdlib(clone)
        && ast_call_semantic_callee_builtin_kind(clone, &builtin) && builtin == BUILTIN_CLONE
        && actual_type != NULL && strcmp(actual_type, "Array<String>") == 0);
    char *error = NULL;
    HIRSemanticProjectionFailure failure = HIR_SEMANTIC_PROJECTION_NONE;
    HIRProgram *hir = clone != NULL ? hir_lower_with_semantic_facts(sem, &failure, &error) : NULL;
    TEST("HIR admits deep-dropped Clone of a borrowed literal array");
    EXPECT(hir != NULL && failure == HIR_SEMANTIC_PROJECTION_NONE);
    hir_destroy(hir); free(error);
    if (clone != NULL) {
        for (size_t fault = 0; fault < 2; fault++) {
            bool saved_stdlib = clone->data.call.semantic_callee_is_stdlib;
            char *saved_type = clone->data.call.semantic_value_type_name;
            if (fault == 0) clone->data.call.semantic_callee_is_stdlib = false;
            if (fault == 1) clone->data.call.semantic_value_type_name = NULL;
            error = NULL;
            hir = hir_lower_with_semantic_facts(sem, &failure, &error);
            TEST(fault == 0 ? "HIR rejects missing canonical Clone identity"
                            : "HIR rejects missing actual Clone result type");
            EXPECT(hir == NULL && error != NULL);
            clone->data.call.semantic_callee_is_stdlib = saved_stdlib;
            clone->data.call.semantic_value_type_name = saved_type;
            hir_destroy(hir); free(error);
        }
    }
    semantic_result_destroy(sem); ast_destroy(ast);
    parser_destroy(parser); lexer_destroy(lexer);
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
    test_hir_function_param_flow_rejects_unknown_routine();
    test_hir_region_escape_fact_carriage();
    test_hir_map_keys_transfer_carriage();
    test_hir_owned_snapshot_cfg_generations();
    test_hir_clone_checked_result_carriage();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
