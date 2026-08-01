/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR synthesis and drift test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler/air.h"
#include "compiler/air_erasure_squiggle.h"
#include "compiler/air_internal.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/pgy_runtime_capability.h"
#include "runtime/pgy_frontier_policy.h"
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

/* docs/140 slice 5: BLUE policy — SUMMARIZE (erasable-by-policy) is the blue
 * signal; ERASE has nothing left to flag and RETAIN is inherent. */
static bool
test_air_erasure_squiggle_policy(void)
{
    bool ok = true;
    ok = ok && air_compression_squiggle_class(
                   AIR_COMPRESSION_SUMMARIZE, AIR_RETAIN_CAUSE_POLICY)
               == SQUIGGLE_BLUE;
    ok = ok && air_compression_squiggle_class(
                   AIR_COMPRESSION_ERASE, AIR_RETAIN_CAUSE_NONE)
               == SQUIGGLE_NONE;
    ok = ok && air_compression_squiggle_class(
                   AIR_COMPRESSION_RETAIN, AIR_RETAIN_CAUSE_INHERENT)
               == SQUIGGLE_NONE;
    ok = ok && air_compression_squiggle_class(
                   AIR_COMPRESSION_FORBID, AIR_RETAIN_CAUSE_NONE)
               == SQUIGGLE_NONE;
    ok = ok && air_compression_squiggle_class(
                   AIR_COMPRESSION_UNKNOWN, AIR_RETAIN_CAUSE_NONE)
               == SQUIGGLE_NONE;
    return ok;
}

static AIRProgram *lower_air_from_source(const char *source);

static void
test_air_carries_function_param_flow_summary(void)
{
    static const char *source =
        "subject Vec2 { let x: Int; let y: Int; }\n"
        "func Recur(ref slot: Slot<Vec2>) -> Void {\n"
        "  Recur(slot);\n"
        "  Write(slot, Vec2(1, 2));\n"
        "}\n"
        "func Main() -> Void {\n"
        "  let slot: Slot<Vec2> = Vec2(0, 0);\n"
        "  Recur(slot);\n"
        "  Release(slot);\n"
        "}\n";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    HIRProgram *hir = NULL;
    DIRProgram *dir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram mir;
    MIRRoutine mir_routine;
    MIRFunctionParamFlowSummary mir_flow_row;
    AIRProgram *air = NULL;
    char *error = NULL;
    bool carried = false;
    bool validated = false;
    bool negative_rejected = false;

    memset(&mir, 0, sizeof(mir));
    memset(&mir_routine, 0, sizeof(mir_routine));
    memset(&mir_flow_row, 0, sizeof(mir_flow_row));
    mir_flow_row.parameter_index = 0;
    mir_flow_row.mask = 0x5u;
    mir_routine.name = "Recur";
    mir_routine.owner_ast_type = AST_FUNC_DECL;
    mir_routine.has_signature = true;
    mir_routine.param_count = 1;
    mir_routine.source_syntax_id = 42u;
    mir_routine.function_param_flow_summaries = &mir_flow_row;
    mir_routine.function_param_flow_summary_count = 1;
    mir_routine.function_param_flow_summary_capacity = 1;
    mir.routines = &mir_routine;
    mir.routine_count = 1;
    mir.has_function_param_flow_facts = true;

    if (!parser_has_error(parser) && sem != NULL && sem->success) {
        hir = hir_lower_with_resource_and_param_flow_facts(
            sem->annotated_ast,
            sem->resource_flow_facts,
            sem->resource_flow_fact_count,
            sem->function_param_flow_facts,
            sem->function_param_flow_fact_count,
            &error);
        dir = dir_lower(sem->annotated_ast, &error);
        rir = rir_lower(sem->annotated_ast, &error);
        if (hir != NULL && rir != NULL)
            (void)rir_enrich_with_hir_flow(rir, hir, &error);
        if (hir != NULL && dir != NULL && rir != NULL)
            air = air_synthesize(hir, dir, rir, &error);
        if (air != NULL)
            carried = air_collect_mir_evidence(air, &mir, &error);
        if (carried)
            validated = air_verify(air, &error);
    }

    if (air != NULL && carried && validated) {
        carried = air_function_param_flow_summary_count(air) > 0;
        for (size_t i = 0; i < air_function_param_flow_summary_count(air); i++) {
            const AIRFunctionParamFlowSummary *row =
                air_function_param_flow_summary_at(air, i);
            if (row != NULL && row->source_syntax_id != 0
                && row->routine != NULL
                && row->parameter_index < row->parameter_count) {
                carried = true;
                break;
            }
            carried = false;
        }
        if (carried) {
            AIRFunctionParamFlowSummary *row =
                &air->function_param_flow_summaries[0];
            const size_t original_index = row->parameter_index;
            row->parameter_index = row->parameter_count;
            negative_rejected = !air_verify(air, &error);
            row->parameter_index = original_index;
            free(error);
            error = NULL;
        }
    } else {
        carried = false;
    }
    TEST("AIR carries MIR function parameter flow summaries by stable identity");
    EXPECT(carried && validated && negative_rejected);
    free(error);
    air_destroy(air);
    rir_destroy(rir);
    dir_destroy(dir);
    hir_destroy(hir);
    semantic_result_destroy(sem);
    parser_destroy(parser);
    lexer_destroy(lexer);
}

static bool
test_air_rejects_mir_evidence_rebind(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(*air));
    MIRProgram mir = {0};
    char *error = NULL;
    bool first = air != NULL
        && air_collect_mir_evidence(air, &mir, &error);
    bool anchored = air != NULL
        && air->mir_evidence_collection_started
        && air->mir_evidence_bound
        && air->mir_evidence_binding_fingerprint != 0;
    free(error);
    error = NULL;
    bool second = air != NULL
        && air_collect_mir_evidence(air, &mir, &error);
    bool rejected = !second && error != NULL
        && strstr(error, "already anchored") != NULL;
    free(error);
    air_destroy(air);
    return first && anchored && rejected;
}

/* docs/140 slice 5b: the collector turns SUMMARIZE AIR nodes into BLUE squiggle
 * sites. Lowered from real source — a zone-bound intent step summarizes (its
 * domain meaning is compressed to a digest, i.e. erasable). */
static bool
test_air_collect_erasure_squiggles(void)
{
    const char *source =
        "subject Widget { let value: Int;\n"
        "    action Inspect(self) -> Bool { return true; } }\n"
        "zone CalmZone { subject slot w: Widget }\n"
        "intent Observe(calm: CalmZone, w: Widget) {\n"
        "    step Check {\n"
        "        where: CalmZone;\n"
        "        using: calm;\n"
        "        who: w;\n"
        "        expect: true;\n"
        "    }\n"
        "}\n"
        "func Main() -> Void { Log(\"x\"); }\n";
    AIRProgram *air = lower_air_from_source(source);
    AIRErasureSquiggle out[8];
    size_t n;
    bool has_located = false;

    if (air == NULL)
        return false;
    n = air_collect_erasure_squiggles(air, out, 8);
    for (size_t i = 0; i < n; i++) {
        if (out[i].line > 0u && out[i].source_name != NULL)
            has_located = true;
    }
    air_destroy(air);
    /* At least one erasable (summarized) node, with a real source location. */
    return n >= 1 && has_located;
}

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


#include "tests/air/test_air_core_part_a_1.cases.h"
#include "tests/air/test_air_core_part_a_2.cases.h"
#include "tests/air/test_air_core_evidence_part_k.cases.h"
#include "tests/air/test_air_core_part_h.cases.h"
#include "tests/air/test_air_evidence_part_b_1.cases.h"
#include "tests/air/test_air_evidence_part_b_2.cases.h"
#include "tests/air/test_air_cleanup_transfer_part_c_1.cases.h"
#include "tests/air/test_air_cleanup_transfer_part_c_2.cases.h"
#include "tests/air/test_air_rir_propagation_part_j.cases.h"
#include "tests/air/test_air_cleanup_transfer_part_d.cases.h"
#include "tests/air/test_air_boundary_part_d_1.cases.h"
#include "tests/air/test_air_boundary_part_d_2.cases.h"
#include "tests/air/test_air_boundary_part_e.cases.h"
#include "tests/air/test_air_parsed_part_e.cases.h"
#include "tests/air/test_air_strict_part_f.cases.h"
#include "tests/air/test_air_observability_pin_part_g_1.cases.h"
#include "tests/air/test_air_observability_pin_part_g_2.cases.h"
#include "tests/air/test_air_mir_terminator_part_h.cases.h"
#include "tests/air/test_air_counter_strict_part_i.cases.h"

int
main(void)
{
    printf("=== AIR Test ===\n");

    TEST("AIR synthesis creates intent and boundary nodes");
    EXPECT(test_air_synthesizes_intent_and_boundary());

    test_air_carries_function_param_flow_summary();

    TEST("AIR rejects a second MIR evidence binding");
    EXPECT(test_air_rejects_mir_evidence_rebind());

    TEST("AIR who inference does not imply authority");
    EXPECT(test_air_who_inference_does_not_imply_authority());

    TEST("AIR compression erases pure sync intent facts");
    EXPECT(test_air_compression_erases_pure_sync_intent());

    TEST("AIR erasure squiggle policy: SUMMARIZE is BLUE (docs/140 slice 5)");
    EXPECT(test_air_erasure_squiggle_policy());

    TEST("AIR erasure collector emits BLUE site for an erasable node (slice 5b)");
    EXPECT(test_air_collect_erasure_squiggles());

    TEST("AIR compression summarizes static zone and retains authority");
    EXPECT(test_air_compression_summarizes_static_zone_and_retains_authority());

    TEST("AIR compression retains world transfer boundary");
    EXPECT(test_air_compression_retains_world_transfer_boundary());

    TEST("AIR drift checker reports sync/async mismatch");
    EXPECT(test_air_detects_sync_async_drift());

    TEST("AIR drift checker accepts matching async boundary");
    EXPECT(test_air_accepts_async_boundary_match());

    TEST("AIR strict evidence reports missing RIR boundary");
    EXPECT(test_air_strict_evidence_reports_missing_boundary());

    TEST("AIR strict evidence requires HIR for implementation boundary");
    EXPECT(test_air_strict_evidence_requires_hir_for_implementation_boundary());

    TEST("AIR strict evidence rejects stale boundary summary flags");
    EXPECT(test_air_strict_evidence_prefers_inventory_over_summary_flags());

    TEST("AIR evidence helper ignores summary flags with real input");
    EXPECT(test_air_has_evidence_ignores_summary_flags_with_real_input());

    TEST("AIR strict evidence rejects summary flags with real input");
    EXPECT(test_air_strict_evidence_rejects_summary_flags_with_real_input());

    TEST("AIR verify rejects boundary summary without inventory");
    EXPECT(test_air_verify_rejects_summary_without_inventory());

    TEST("AIR parallel boundary requires RIR and HIR evidence");
    EXPECT(test_air_parallel_boundary_requires_rir_and_hir_evidence());

    TEST("AIR recheck clears owned drift messages");
    EXPECT(test_air_recheck_clears_owned_drift_messages());

    TEST("AIR verify rejects invalid boundary inventory");
    EXPECT(test_air_verify_rejects_invalid_boundary_inventory());

    TEST("AIR verify rejects authority name/flag drift");
    EXPECT(test_air_verify_rejects_authority_name_flag_drift());

    TEST("AIR verify rejects source provenance shape drift");
    EXPECT(test_air_verify_rejects_source_provenance_shape_drift());

    TEST("AIR verify rejects missing inventory arrays");
    EXPECT(test_air_verify_rejects_missing_inventory_arrays());

    TEST("AIR rejects missing slot site inventory");
    EXPECT(test_air_rejects_missing_slot_site_inventory());

    TEST("AIR rejects empty slot site fact");
    EXPECT(test_air_rejects_empty_slot_site_fact());

    TEST("AIR rejects missing required ability inventory");
    EXPECT(test_air_rejects_missing_required_ability_inventory());

    TEST("AIR rejects empty required ability fact");
    EXPECT(test_air_rejects_empty_required_ability_fact());

    TEST("AIR rejects missing effect site inventory");
    EXPECT(test_air_rejects_missing_effect_site_inventory());

    TEST("AIR rejects effect site name/mask mismatch");
    EXPECT(test_air_rejects_effect_site_name_mask_mismatch());

    TEST("AIR rejects effect site outside program capability mask");
    EXPECT(test_air_rejects_effect_site_outside_program_capability_mask());

    TEST("AIR validates capability machine slot/effect facts");
    EXPECT(test_air_validates_capability_machine_slot_effect_facts());

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

    TEST("AIR append merges duplicate evidence nodes");
    EXPECT(test_air_append_merges_duplicate_evidence_nodes());

    TEST("AIR append keeps boundary evidence idempotent");
    EXPECT(test_air_append_idempotent_boundary_evidence_nodes());

    TEST("AIR append rejects boundary evidence duplicate fallback");
    EXPECT(test_air_append_rejects_boundary_evidence_duplicate_fallback());

    TEST("AIR append rejects unknown evidence kind");
    EXPECT(test_air_append_rejects_unknown_evidence_kind());

    TEST("AIR append rejects empty evidence provenance");
    EXPECT(test_air_append_rejects_empty_evidence_provenance());

    TEST("AIR append rejects empty evidence counts");
    EXPECT(test_air_append_rejects_empty_evidence_counts());

    TEST("AIR append rejects fallback evidence counts");
    EXPECT(test_air_append_rejects_fallback_evidence_counts());

    TEST("AIR append rejects boundary evidence without boundary index");
    EXPECT(test_air_append_rejects_boundary_evidence_without_boundary_index());

    TEST("AIR append rejects global evidence with boundary index");
    EXPECT(test_air_append_rejects_global_evidence_with_boundary_index());

    TEST("AIR verify rejects evidence boundary shape mismatch");
    EXPECT(test_air_verify_rejects_evidence_boundary_shape_mismatch());

    TEST("AIR rejects RIR boundary/authority counter mismatch");
    EXPECT(test_air_rejects_rir_boundary_authority_counter_mismatch());

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

    TEST("AIR synthesis collects all RIR authority evidence");
    EXPECT(test_air_collects_all_rir_authority_evidence());

    TEST("AIR synthesis keeps local intent authority aliases");
    EXPECT(test_air_collects_local_intent_authority_alias_evidence());

    TEST("AIR strict evidence rejects mismatched authority participant");
    EXPECT(test_air_rejects_mismatched_authority_evidence());

    TEST("AIR strict evidence requires all authority participants");
    EXPECT(test_air_requires_all_authority_participant_evidence());

    TEST("AIR dump prints evidence provenance");
    EXPECT(test_air_dump_prints_evidence_provenance());

    TEST("AIR JSON dump prints stable graph schema");
    EXPECT(test_air_dump_json_prints_stable_graph_schema());

    TEST("AIR synthesis collects observability schema evidence");
    EXPECT(test_air_synthesis_collects_observability_schema_evidence());

    TEST("AIR collects singleton global evidence idempotently");
    EXPECT(test_air_collects_singleton_global_evidence_idempotently());

    TEST("AIR rejects conflicting singleton global evidence");
    EXPECT(test_air_rejects_conflicting_singleton_global_evidence());

    TEST("AIR rejects invalid observability schema provider");
    EXPECT(test_air_rejects_invalid_observability_schema_provider());

    TEST("AIR rejects empty observability schema evidence");
    EXPECT(test_air_rejects_empty_observability_schema_evidence());

    TEST("AIR strict evidence requires observability schema");
    EXPECT(test_air_strict_evidence_requires_observability_schema());

    TEST("AIR strict evidence rejects observability counter only");
    EXPECT(test_air_strict_evidence_rejects_observability_counter_only());

    TEST("AIR rejects invalid runtime frontier policy provider");
    EXPECT(test_air_rejects_invalid_runtime_frontier_policy_provider());

    TEST("AIR rejects empty runtime frontier policy evidence");
    EXPECT(test_air_rejects_empty_runtime_frontier_policy_evidence());

    TEST("AIR strict evidence requires runtime frontier policy");
    EXPECT(test_air_strict_evidence_requires_runtime_frontier_policy());

    TEST("AIR strict evidence rejects frontier counter only");
    EXPECT(test_air_strict_evidence_rejects_frontier_counter_only());

    TEST("AIR runtime frontier policy names PGY_FRONTIER_PUBLISH_READY and PGY_FRONTIER_PUBLISH_CLEAR_DIRTY");
    EXPECT(test_air_runtime_frontier_policy_names_publish_order());

    TEST("AIR collects MIR pin cleanup evidence");
    EXPECT(test_air_collects_mir_pin_cleanup_evidence());

    TEST("AIR requires MIR pin cleanup AST provenance");
    EXPECT(test_air_requires_pin_cleanup_ast_provenance());

    TEST("AIR rejects orphan MIR pin cleanup evidence");
    EXPECT(test_air_rejects_orphan_mir_pin_cleanup_evidence());

    TEST("AIR strict evidence rejects pin cleanup without cleanup root");
    EXPECT(test_air_strict_rejects_pin_cleanup_without_cleanup_root());

    TEST("AIR rejects unanchored MIR pin cleanup evidence");
    EXPECT(test_air_rejects_unanchored_mir_pin_cleanup_evidence());

    TEST("AIR rejects mismatched MIR pin cleanup evidence");
    EXPECT(test_air_rejects_mismatched_mir_pin_cleanup_evidence());

    TEST("AIR rejects pin cleanup evidence without slot subject");
    EXPECT(test_air_rejects_pin_cleanup_evidence_without_slot_subject());

    TEST("AIR rejects pin cleanup evidence without source provenance");
    EXPECT(test_air_rejects_pin_cleanup_evidence_without_source_provenance());

    TEST("AIR strict evidence requires MIR pin cleanup");
    EXPECT(test_air_strict_evidence_requires_mir_pin_cleanup());

    TEST("AIR rejects MIR pin cleanup evidence fact-count mismatch");
    EXPECT(test_air_rejects_pin_cleanup_evidence_fact_count_mismatch());

    TEST("AIR rejects MIR pin cleanup without global cleanup evidence");
    EXPECT(test_air_rejects_pin_cleanup_without_global_cleanup_evidence());

    TEST("AIR collects MIR cleanup block evidence");
    EXPECT(test_air_collects_mir_cleanup_block_evidence());

    TEST("AIR collects MIR terminator evidence");
    EXPECT(test_air_collects_mir_terminator_evidence());

    TEST("AIR collects MIR value-capture lane evidence");
    EXPECT(test_air_collects_mir_value_capture_lane_evidence());

    TEST("AIR rejects empty MIR terminator evidence");
    EXPECT(test_air_rejects_empty_mir_terminator_evidence());

    TEST("AIR rejects MIR evidence without routine provider");
    EXPECT(test_air_rejects_mir_evidence_without_routine_provider());

    TEST("AIR rejects malformed MIR routine inventory");
    EXPECT(test_air_rejects_malformed_mir_routine_inventory());

    TEST("AIR collects MIR select receive evidence");
    EXPECT(test_air_collects_mir_select_receive_evidence());

    TEST("AIR rejects empty MIR select receive evidence");
    EXPECT(test_air_rejects_empty_mir_select_receive_evidence());

    TEST("AIR rejects MIR evidence counter mismatch");
    EXPECT(test_air_rejects_mir_evidence_counter_mismatch());

    TEST("AIR collects Void fallthrough terminator evidence");
    EXPECT(test_air_collects_void_fallthrough_terminator_evidence());

    TEST("AIR strict evidence requires MIR terminator evidence");
    EXPECT(test_air_strict_evidence_requires_mir_terminator_evidence());

    TEST("AIR strict evidence rejects MIR terminator counter only");
    EXPECT(test_air_strict_evidence_rejects_mir_terminator_counter_only());

    TEST("AIR strict evidence rejects MIR cleanup counter only");
    EXPECT(test_air_strict_evidence_rejects_mir_cleanup_counter_only());

    TEST("AIR strict evidence rejects MIR select receive counter only");
    EXPECT(test_air_strict_evidence_rejects_mir_select_receive_counter_only());

    TEST("AIR ignores orphan MIR cleanup root evidence");
    EXPECT(test_air_ignores_orphan_mir_cleanup_root_evidence());

    TEST("AIR ignores unreachable MIR cleanup root evidence");
    EXPECT(test_air_ignores_unreachable_mir_cleanup_root_evidence());

    TEST("AIR ignores unreachable MIR cleanup source evidence");
    EXPECT(test_air_ignores_unreachable_mir_cleanup_source_evidence());

    TEST("AIR rejects empty MIR cleanup evidence");
    EXPECT(test_air_rejects_empty_mir_cleanup_evidence());

    TEST("AIR collects DAG generic ability evidence");
    EXPECT(test_air_collects_dag_generic_ability_evidence());

    TEST("AIR collects lifecycle state-space inventory");
    EXPECT(test_air_collects_lifecycle_state_space_inventory());

    TEST("AIR rejects DAG metadata hits without inventory");
    EXPECT(test_air_rejects_dag_hits_without_metadata_inventory());

    TEST("AIR rejects DAG dead-end before evidence append");
    EXPECT(test_air_rejects_dag_dead_end_before_evidence_append());

    TEST("AIR strict evidence rejects DAG counter only");
    EXPECT(test_air_strict_evidence_rejects_dag_counter_only());

    TEST("AIR collects RIR effect relation propagation evidence");
    EXPECT(test_air_collects_rir_effect_relation_propagation_evidence());

    TEST("AIR rejects RIR evidence without scope provider");
    EXPECT(test_air_rejects_rir_evidence_without_scope_provider());

    TEST("AIR reports missing effect relation propagation evidence");
    EXPECT(test_air_reports_missing_effect_relation_propagation_evidence());

    TEST("AIR strict evidence rejects RIR propagation counter only");
    EXPECT(test_air_strict_evidence_rejects_rir_propagation_counter_only());

    TEST("AIR rejects empty RIR propagation evidence");
    EXPECT(test_air_rejects_empty_rir_propagation_evidence());

    TEST("AIR rejects RIR propagation evidence counter mismatch");
    EXPECT(test_air_rejects_rir_propagation_counter_mismatch());

    TEST("AIR rejects RIR propagation evidence key mismatch");
    EXPECT(test_air_rejects_rir_propagation_key_mismatch());

    TEST("AIR rejects MIR pin cleanup counter mismatch");
    EXPECT(test_air_rejects_pin_cleanup_counter_mismatch());

    TEST("AIR strict evidence rejects MIR pin cleanup counter only");
    EXPECT(test_air_strict_evidence_rejects_pin_cleanup_counter_only());

    TEST("AIR rejects invalid DAG evidence provider");
    EXPECT(test_air_rejects_invalid_dag_evidence_provider());

    TEST("AIR rejects empty DAG evidence");
    EXPECT(test_air_rejects_empty_dag_evidence());

    TEST("AIR rejects DAG dead-end evidence");
    EXPECT(test_air_rejects_dag_dead_end_evidence());

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

    TEST("AIR parallel boundary requires RIR source provenance");
    EXPECT(test_air_parallel_boundary_requires_rir_source_provenance());

    TEST("AIR HIR CFG requires boundary source provenance");
    EXPECT(test_air_hir_cfg_requires_boundary_source_provenance());

    TEST("AIR rejects HIR CFG evidence without source provenance");
    EXPECT(test_air_rejects_hir_cfg_evidence_without_source_provenance());

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

    TEST("AIR parsed transfer reports world missing transfer evidence");
    EXPECT(test_air_parsed_transfer_reports_world_missing_transfer_evidence());

    TEST("AIR parsed transfer reports zone missing authority evidence");
    EXPECT(test_air_parsed_transfer_reports_zone_missing_authority_evidence());

    TEST("AIR parsed on-receiver action contract provenance");
    EXPECT(test_air_parsed_on_receiver_action_contract_provenance());

    TEST("AIR action self authority uses exact intent RIR evidence");
    EXPECT(test_air_parsed_action_self_authority_uses_intent_rir_evidence());

    printf("\nAIR tests: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
