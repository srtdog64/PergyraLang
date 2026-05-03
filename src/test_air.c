/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR synthesis and drift test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler/air.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/pgy_runtime_observability_schema.h"
#include "semantic/semantic.h"

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-64s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("ok\n"); g_pass++; } \
        else      { printf("fail (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

static void
test_air_clear_stack_drifts(AIRProgram *air)
{
    if (air == NULL)
        return;
    for (size_t i = 0; i < air->drift_count; i++)
        free((char *)air->drifts[i].message);
    free(air->drifts);
    air->drifts = NULL;
    air->drift_count = 0;
}

static AIRProgram *
lower_air_from_source(const char *source)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    DIRProgram *dir = NULL;
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    AIRProgram *air = NULL;
    char *error = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success) {
        dir = dir_lower(sem->annotated_ast, &error);
        hir = hir_lower(sem->annotated_ast, &error);
        rir = rir_lower(sem->annotated_ast, &error);
        if (hir != NULL && rir != NULL)
            (void)rir_enrich_with_hir_flow(rir, hir, &error);
        if (dir != NULL && hir != NULL && rir != NULL)
            air = air_synthesize(hir, dir, rir, &error);
        if (air != NULL
            && (!air_collect_dag_evidence(air, sem, &error)
                || !air_verify(air, &error))) {
            air_destroy(air);
            air = NULL;
        }
    }

    if (air == NULL && error != NULL)
        fprintf(stderr, "AIR source lowering error: %s\n", error);

    free(error);
    dir_destroy(dir);
    rir_destroy(rir);
    hir_destroy(hir);
    semantic_result_destroy(sem);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return air;
}


#include "tests/air/test_air_core_part_a.cases.h"
#include "tests/air/test_air_core_part_h.cases.h"
#include "tests/air/test_air_evidence_part_b.cases.h"
#include "tests/air/test_air_cleanup_transfer_part_c.cases.h"
#include "tests/air/test_air_cleanup_transfer_part_d.cases.h"
#include "tests/air/test_air_boundary_part_d.cases.h"
#include "tests/air/test_air_parsed_part_e.cases.h"
#include "tests/air/test_air_strict_part_f.cases.h"
#include "tests/air/test_air_observability_pin_part_g.cases.h"
#include "tests/air/test_air_mir_terminator_part_h.cases.h"

int
main(void)
{
    printf("=== AIR Test ===\n");

    TEST("AIR synthesis creates intent and boundary nodes");
    EXPECT(test_air_synthesizes_intent_and_boundary());

    TEST("AIR drift checker reports sync/async mismatch");
    EXPECT(test_air_detects_sync_async_drift());

    TEST("AIR drift checker accepts matching async boundary");
    EXPECT(test_air_accepts_async_boundary_match());

    TEST("AIR strict evidence reports missing RIR boundary");
    EXPECT(test_air_strict_evidence_reports_missing_boundary());

    TEST("AIR strict evidence requires HIR for implementation boundary");
    EXPECT(test_air_strict_evidence_requires_hir_for_implementation_boundary());

    TEST("AIR strict evidence rejects stale legacy summary flags");
    EXPECT(test_air_strict_evidence_prefers_inventory_over_legacy_flags());

    TEST("AIR strict evidence rejects legacy flags with real input");
    EXPECT(test_air_strict_evidence_rejects_legacy_flags_with_real_input());

    TEST("AIR task group boundary requires RIR and HIR evidence");
    EXPECT(test_air_task_group_boundary_requires_rir_and_hir_evidence());

    TEST("AIR recheck clears owned drift messages");
    EXPECT(test_air_recheck_clears_owned_drift_messages());

    TEST("AIR verify rejects invalid boundary inventory");
    EXPECT(test_air_verify_rejects_invalid_boundary_inventory());

    TEST("AIR verify rejects missing inventory arrays");
    EXPECT(test_air_verify_rejects_missing_inventory_arrays());

    TEST("AIR verify rejects boundary step mismatch");
    EXPECT(test_air_verify_rejects_boundary_step_mismatch());

    TEST("AIR verify rejects boundary owner mismatch");
    EXPECT(test_air_verify_rejects_boundary_owner_mismatch());

    TEST("AIR verify rejects boundary sync shape mismatch");
    EXPECT(test_air_verify_rejects_boundary_sync_shape_mismatch());

    TEST("AIR verify rejects world boundary without transfer provenance");
    EXPECT(test_air_verify_rejects_world_boundary_without_transfer_provenance());

    TEST("AIR verify rejects invalid drift inventory");
    EXPECT(test_air_verify_rejects_invalid_drift_inventory());

    TEST("AIR verify rejects invalid evidence inventory");
    EXPECT(test_air_verify_rejects_invalid_evidence_inventory());

    TEST("AIR verify rejects duplicate evidence nodes");
    EXPECT(test_air_verify_rejects_duplicate_evidence_nodes());

    TEST("AIR verify rejects evidence boundary shape mismatch");
    EXPECT(test_air_verify_rejects_evidence_boundary_shape_mismatch());

    TEST("AIR verify rejects empty boundary evidence");
    EXPECT(test_air_verify_rejects_empty_boundary_evidence());

    TEST("AIR verify rejects authority evidence shape mismatch");
    EXPECT(test_air_verify_rejects_authority_evidence_shape_mismatch());

    TEST("AIR verify rejects CFG evidence without routine evidence");
    EXPECT(test_air_verify_rejects_cfg_evidence_without_routine());

    TEST("AIR verify rejects empty evidence provenance");
    EXPECT(test_air_verify_rejects_empty_evidence_provenance());

    TEST("AIR check_drift remains verify compatibility wrapper");
    EXPECT(test_air_check_drift_is_verify_compatibility_wrapper());

    TEST("AIR synthesis collects HIR/RIR evidence without mutation");
    EXPECT(test_air_collects_hir_and_rir_evidence());

    TEST("AIR strict evidence rejects mismatched authority participant");
    EXPECT(test_air_rejects_mismatched_authority_evidence());

    TEST("AIR dump prints evidence provenance");
    EXPECT(test_air_dump_prints_evidence_provenance());

    TEST("AIR JSON dump prints stable graph schema");
    EXPECT(test_air_dump_json_prints_stable_graph_schema());

    TEST("AIR synthesis collects observability schema evidence");
    EXPECT(test_air_synthesis_collects_observability_schema_evidence());

    TEST("AIR rejects invalid observability schema provider");
    EXPECT(test_air_rejects_invalid_observability_schema_provider());

    TEST("AIR rejects empty observability schema evidence");
    EXPECT(test_air_rejects_empty_observability_schema_evidence());

    TEST("AIR collects MIR pin cleanup evidence");
    EXPECT(test_air_collects_mir_pin_cleanup_evidence());

    TEST("AIR rejects orphan MIR pin cleanup evidence");
    EXPECT(test_air_rejects_orphan_mir_pin_cleanup_evidence());

    TEST("AIR rejects unanchored MIR pin cleanup evidence");
    EXPECT(test_air_rejects_unanchored_mir_pin_cleanup_evidence());

    TEST("AIR rejects mismatched MIR pin cleanup evidence");
    EXPECT(test_air_rejects_mismatched_mir_pin_cleanup_evidence());

    TEST("AIR rejects pin cleanup evidence without slot subject");
    EXPECT(test_air_rejects_pin_cleanup_evidence_without_slot_subject());

    TEST("AIR strict evidence requires MIR pin cleanup");
    EXPECT(test_air_strict_evidence_requires_mir_pin_cleanup());

    TEST("AIR rejects MIR pin cleanup evidence fact-count mismatch");
    EXPECT(test_air_rejects_pin_cleanup_evidence_fact_count_mismatch());

    TEST("AIR collects MIR cleanup block evidence");
    EXPECT(test_air_collects_mir_cleanup_block_evidence());

    TEST("AIR collects MIR terminator evidence");
    EXPECT(test_air_collects_mir_terminator_evidence());

    TEST("AIR rejects empty MIR terminator evidence");
    EXPECT(test_air_rejects_empty_mir_terminator_evidence());

    TEST("AIR strict evidence requires MIR terminator evidence");
    EXPECT(test_air_strict_evidence_requires_mir_terminator_evidence());

    TEST("AIR ignores orphan MIR cleanup root evidence");
    EXPECT(test_air_ignores_orphan_mir_cleanup_root_evidence());

    TEST("AIR rejects empty MIR cleanup evidence");
    EXPECT(test_air_rejects_empty_mir_cleanup_evidence());

    TEST("AIR collects DAG generic ability evidence");
    EXPECT(test_air_collects_dag_generic_ability_evidence());

    TEST("AIR reports DAG fallback drift");
    EXPECT(test_air_reports_dag_fallback_drift());

    TEST("AIR collects RIR effect relation propagation evidence");
    EXPECT(test_air_collects_rir_effect_relation_propagation_evidence());

    TEST("AIR reports missing effect relation propagation evidence");
    EXPECT(test_air_reports_missing_effect_relation_propagation_evidence());

    TEST("AIR rejects empty RIR propagation evidence");
    EXPECT(test_air_rejects_empty_rir_propagation_evidence());

    TEST("AIR rejects invalid DAG evidence provider");
    EXPECT(test_air_rejects_invalid_dag_evidence_provider());

    TEST("AIR rejects empty DAG evidence");
    EXPECT(test_air_rejects_empty_dag_evidence());

    TEST("AIR rejects invalid DAG evidence subject");
    EXPECT(test_air_rejects_invalid_dag_evidence_subject());

    TEST("AIR world boundary requires transfer evidence");
    EXPECT(test_air_world_boundary_requires_transfer_evidence());

    TEST("AIR world boundary accepts transfer evidence");
    EXPECT(test_air_world_boundary_accepts_transfer_evidence());

    TEST("AIR world boundary rejects mismatched transfer AST evidence");
    EXPECT(test_air_world_boundary_rejects_mismatched_transfer_ast());

    TEST("AIR lowers parsed intent source without drift");
    EXPECT(test_air_lowers_from_source_without_drift());

    TEST("AIR synthesis captures spawn boundary from intent step AST");
    EXPECT(test_air_synthesizes_spawn_boundary_from_step_ast());

    TEST("AIR synthesis captures boundary from let initializer");
    EXPECT(test_air_synthesizes_boundary_from_let_initializer());

    TEST("AIR synthesis captures boundary from event handler payload");
    EXPECT(test_air_synthesizes_boundary_from_event_handler_payload());

    TEST("AIR rejects unmatched top-level intent HIR evidence");
    EXPECT(test_air_rejects_unmatched_top_level_intent_hir_evidence());

    TEST("AIR synthesis captures IO boundary without sync drift");
    EXPECT(test_air_synthesizes_io_boundary_without_sync_drift());

    TEST("AIR await boundary accepts exact RIR evidence");
    EXPECT(test_air_await_boundary_accepts_exact_rir_evidence());

    TEST("AIR await boundary rejects generic RIR scope evidence");
    EXPECT(test_air_await_boundary_rejects_generic_rir_scope_evidence());

    TEST("AIR channel boundary accepts exact RIR op evidence");
    EXPECT(test_air_channel_boundary_accepts_exact_rir_op_evidence());

    TEST("AIR HIR evidence accepts nested execution boundary AST");
    EXPECT(test_air_hir_evidence_accepts_nested_execution_boundary_ast());

    TEST("AIR HIR evidence accepts loop condition boundary AST");
    EXPECT(test_air_hir_evidence_accepts_loop_condition_boundary_ast());

    TEST("AIR synthesis captures stable IO boundary builtin set");
    EXPECT(test_air_synthesizes_stable_io_boundary_builtin_set());

    TEST("AIR synthesis captures stable execution boundary set");
    EXPECT(test_air_synthesizes_stable_execution_boundary_set());

    TEST("AIR parsed IO boundary accepts exact RIR evidence");
    EXPECT(test_air_parsed_io_boundary_accepts_exact_rir_evidence());

    TEST("AIR parsed transfer emits zone and world boundaries");
    EXPECT(test_air_parsed_transfer_emits_zone_and_world_boundaries());

    TEST("AIR parsed transfer reports zone missing authority evidence");
    EXPECT(test_air_parsed_transfer_reports_zone_missing_authority_evidence());

    TEST("AIR parsed on-receiver action contract provenance");
    EXPECT(test_air_parsed_on_receiver_action_contract_provenance());

    printf("\nAIR tests: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
