/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Transpiler unit test suite
 * Build: make test-transpile
 * Run:   ./bin/test_transpile
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common/string_compat.h"
#include "codegen/transpiler.h"
#include "codegen/transpiler_call_subject_arg_policy.h"
#include "codegen/transpiler_expr_type_infer.h"
#include "codegen/transpiler_domain_nominal_emit.h"
#include "codegen/transpiler_let_type_register_emit.h"
#include "codegen/transpiler_roster_decl_emit.h"
#include "codegen/transpiler_symbols.h"
#include "codegen/transpiler_mir_phi_type_owner.h"
#include "codegen/transpiler_mir_func_ssa_locals_emit.h"
#include "codegen/transpiler_mir_match_payload_emit.h"
#include "codegen/transpiler_type_declarator.h"
#include "codegen/codegen_type_mapping.h"
#include "codegen/transpiler_type_render.h"
#include "codegen/transpiler_world_select_event_emit.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/rir.h"
#include "compiler/mir.h"
#include "compiler/air.h"
#include "compiler/air_evidence_certificate.h"
#include "compiler/mir_ability_ref.h"
#include "compiler/mir_decl_headers.h"
#include "compiler/verified_projection_plan.h"
#include "semantic/type_system.h"
#include "semantic/type_checker.h"
#include "compiler/dir.h"

/* -----------------------------------------------------------------
 * Test runner
 * ----------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;
static MIRProgram *g_last_mir = NULL;

#define TEST(name) \
    do { g_last_mir = NULL; printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("ok\n"); g_pass++; } \
        else      { printf("fail (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

#define EXPECT_STR_CONTAINS(haystack, needle) \
    do { \
        const char *_pgy_haystack = (haystack); \
        const char *_pgy_needle = (needle); \
        EXPECT(_pgy_haystack != NULL && _pgy_needle != NULL \
               && strstr(_pgy_haystack, _pgy_needle) != NULL); \
    } while (0)

#define EXPECT_STR_NOT_CONTAINS(haystack, needle) \
    do { \
        const char *_pgy_haystack = (haystack); \
        const char *_pgy_needle = (needle); \
        EXPECT(_pgy_haystack != NULL && _pgy_needle != NULL \
               && strstr(_pgy_haystack, _pgy_needle) == NULL); \
    } while (0)

typedef struct TestProjectionEvidence {
    const MIRProgram *mir;
    const ASTNode *source_program;
    const PgyCollectionEffectSnapshot *source_snapshot;
    bool ready;
    PgyVerifiedProjectionPlanRow c_plan;
    PgyVerifiedProjectionPlanRow llvm_plan;
    PgyVerifiedParallelCapturePlan parallel_plan;
    PgySpawnLanePlan spawn_plan;
} TestProjectionEvidence;

static void
dispose_test_projection_evidence(TestProjectionEvidence *evidence)
{
    if (evidence == NULL)
        return;
    pgy_verified_parallel_capture_plan_dispose(&evidence->parallel_plan);
    pgy_verified_spawn_lane_plan_dispose(&evidence->spawn_plan);
    memset(evidence, 0, sizeof(*evidence));
}

static bool
issue_test_native_projection_evidence(const SemanticResult *sem,
                                      const HIRProgram *hir,
                                      const DIRProgram *dir,
                                      const RIRProgram *rir,
                                      const MIRProgram *mir,
                                      TestProjectionEvidence *evidence)
{
    AIRProgram *air = NULL;
    char *stage_error = NULL;
    const char *plan_error = NULL;
    const char *failed_stage = NULL;
    bool ok = false;

    if (evidence == NULL || sem == NULL || !sem->success
        || hir == NULL || dir == NULL || rir == NULL || mir == NULL
        || sem->annotated_ast == NULL || sem->collection_effect_snapshot == NULL
        || mir->collection_effect_source_program != sem->annotated_ast
        || mir->collection_effect_snapshot != sem->collection_effect_snapshot) {
        fprintf(stderr, "native test evidence: missing exact semantic/MIR source\n");
        return false;
    }
    memset(evidence, 0, sizeof(*evidence));
    failed_stage = "RIR validation";
    if (!rir_validate(rir, &stage_error)
        || !rir_validate_against_dir(rir, dir, &stage_error))
        goto done;
    failed_stage = "MIR validation";
    if (!mir_validate(mir, &stage_error))
        goto done;
    failed_stage = "AIR synthesis";
    air = air_synthesize(hir, dir, rir, &stage_error);
    if (air == NULL)
        goto done;
    failed_stage = "AIR DAG evidence";
    if (!air_collect_dag_evidence(air, sem, &stage_error)
        || !air_verify(air, &stage_error) || air_drift_count(air) != 0)
        goto done;
    failed_stage = "AIR MIR evidence";
    if (!air_collect_mir_evidence(air, mir, &stage_error)
        || !air_verify(air, &stage_error) || air_drift_count(air) != 0)
        goto done;
    failed_stage = "C projection plan";
    if (!pgy_verified_projection_plan_intent_observability_with_air(
            air, mir, PGY_PROJECTION_TARGET_C, &evidence->c_plan, &plan_error))
        goto done;
#ifdef PGY_LLVM_ENABLED
    failed_stage = "LLVM projection plan";
    if (!pgy_verified_projection_plan_intent_observability_with_air(
            air, mir, PGY_PROJECTION_TARGET_LLVM, &evidence->llvm_plan,
            &plan_error))
        goto done;
#endif
    failed_stage = "parallel capture plan";
    if (!pgy_verified_parallel_capture_plan_from_air(
            air, mir, &evidence->parallel_plan, &plan_error))
        goto done;
    failed_stage = "spawn lane plan";
    if (!pgy_verified_spawn_lane_plan_from_air(
            air, &evidence->spawn_plan, &plan_error))
        goto done;
    evidence->mir = mir;
    evidence->source_program = sem->annotated_ast;
    evidence->source_snapshot = sem->collection_effect_snapshot;
    evidence->ready = true;
    ok = true;

done:
    if (!ok) {
        fprintf(stderr, "native test evidence %s failed: %s\n",
                failed_stage != NULL ? failed_stage : "issuance",
                stage_error != NULL ? stage_error
                    : (plan_error != NULL ? plan_error : "missing evidence"));
        dispose_test_projection_evidence(evidence);
    }
    air_destroy(air);
    free(stage_error);
    return ok;
}


#include "tests/transpile/test_transpile_helpers_1.cases.h"
#include "tests/transpile/test_transpile_helpers_2.cases.h"
#include "tests/transpile/test_transpile_core_part_0.cases.h"
#include "tests/transpile/test_transpile_core_part_a_1.cases.h"
#include "tests/transpile/test_transpile_core_part_a_2.cases.h"
#include "tests/transpile/test_transpile_core_part_b_1.cases.h"
#include "tests/transpile/test_transpile_core_part_b_2.cases.h"
#include "tests/transpile/test_transpile_program_part_a.cases.h"
#include "tests/transpile/test_transpile_program_part_b.cases.h"
#include "tests/transpile/test_transpile_generic_method_specialization.cases.h"
#include "tests/transpile/test_transpile_domain_part_a.cases.h"
#include "tests/transpile/test_transpile_parallel_family.cases.h"
#include "tests/transpile/test_transpile_domain_async_part_a_1.cases.h"
#include "tests/transpile/test_transpile_domain_async_part_a_2.cases.h"
#include "tests/transpile/test_transpile_domain_async_part_b.cases.h"
#include "tests/transpile/test_transpile_stdlib_part_a.cases.h"
#include "tests/transpile/test_transpile_stdlib_part_b_1.cases.h"
#include "tests/transpile/test_transpile_stdlib_part_b_2.cases.h"
#include "tests/transpile/test_transpile_mir_part_a_1.cases.h"
#include "tests/transpile/test_transpile_mir_part_a_2.cases.h"
#include "tests/transpile/test_transpile_mir_part_b.cases.h"
#include "tests/transpile/test_transpile_mir_part_c.cases.h"
#include "tests/transpile/test_transpile_mir_source_order.cases.h"
#include "tests/transpile/test_transpile_mir_phi_type.cases.h"

int
main(int argc, char **argv)
{
    printf("=== Pergyra C Transpiler Test Suite ===\n");

    type_system_init();

    if (argc == 2
        && strcmp(argv[1], "generic-method-specialization") == 0) {
        test_generic_method_specialization_fact();
        test_generic_sealed_binding_fact();
        test_generic_direct_specialization_fact();
        printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
        type_system_cleanup();
        return (g_fail > 0) ? 1 : 0;
    }

    test_codebuf();
    test_type_mapping();
    test_expression_emit();
    test_statement_emit();
    test_program_emit();
    test_generic_method_specialization_fact();
    test_generic_sealed_binding_fact();
    test_generic_direct_specialization_fact();
    test_ability_role_emit();
    test_party_emit();
    test_roster_world_emit();
    test_parallel_family_emit();
    test_parallel_execution_emit();
    test_slot_sugar();
    test_stdlib_and_enum_emit();
    test_transpiler_reentry_stability();
    test_mir_vertical_slice_emit();
    test_mir_select_dispatch_emit();
    test_intent_observability_emit();
    test_source_order_mir_emit();
    test_mir_phi_type_projection();
    test_mir_value_result_entry_storage();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);

    type_system_cleanup();
    return (g_fail > 0) ? 1 : 0;
}
