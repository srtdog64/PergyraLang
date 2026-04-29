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

static bool
test_air_synthesizes_intent_and_boundary(void)
{
    ASTNode intent_ast = { .line = 12, .column = 5 };
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "ShipOrder", .ast = &intent_ast },
    };
    const char *authorized_by[] = { "shipper" };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "reserve",
            .where_type_name = "WarehouseZone",
            .authorized_by = authorized_by,
            .authorized_by_count = 1,
        },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    RIRFact facts[] = {
        {
            .kind = RIR_FACT_AUTHORITY,
            .name = "shipper",
            .resource_kind = RIR_RESOURCE_AUTHORITY_HANDLE,
            .state = RIR_STATE_AUTHORIZED,
        },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_ZONE,
            .owner_name = "ShipOrder",
            .name = "WarehouseZone",
            .facts = facts,
            .fact_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = air_synthesize(NULL, &dir, &rir, &error);
    bool ok = air != NULL
        && air->intent_count == 1
        && air->boundary_count == 1
        && air->drift_count == 0
        && air->strict_evidence
        && strcmp(air->intents[0].intent_owner, "ShipOrder") == 0
        && strcmp(air->intents[0].step_name, "reserve") == 0
        && air->intents[0].ast == &intent_ast
        && air->intents[0].sync_class == AIR_SYNC_SYNC
        && air->boundaries[0].kind == AIR_BOUNDARY_ZONE
        && air->boundaries[0].ast == &intent_ast
        && air->boundaries[0].authority_required
        && air->boundaries[0].authority_name_count == 1
        && strcmp(air->boundaries[0].authority_names[0], "shipper") == 0
        && air->boundaries[0].has_rir_boundary_evidence
        && air->boundaries[0].has_rir_authority_evidence;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_detects_sync_async_drift(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool checked = air_check_drift(&air, &error);
    bool ok = checked
        && air.drift_count == 1
        && air.drifts[0].kind == AIR_DRIFT_SYNC_ASYNC_CONFLICT
        && strstr(air.drifts[0].message, "PGY_SEM_INTENT_BOUNDARY_DRIFT") != NULL;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_accepts_async_boundary_match(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "handoff",
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .failure_class = AIR_FAILURE_COMPENSABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_WORLD,
            .owner_name = "ShipOrder",
            .source_name = "remote",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool ok = air_check_drift(&air, &error) && air.drift_count == 0;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_reports_missing_boundary(void)
{
    const char *authority_names[] = { "shipper" };
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .authority_required = true,
            .authority_names = authority_names,
            .authority_name_count = 1,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .strict_evidence = true,
    };
    char *error = NULL;
    bool found = false;
    bool checked = air_check_drift(&air, &error);
    for (size_t i = 0; i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && strstr(air.drifts[i].message,
                      "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING") != NULL) {
            found = true;
            break;
        }
    }
    bool ok = checked && air.drift_count >= 1 && found;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_requires_hir_for_implementation_boundary(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "dispatch",
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_PARALLEL,
            .owner_name = "ShipOrder",
            .source_name = "spawn",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .has_rir_boundary_evidence = true,
            .rir_boundary_evidence_scope = "spawn",
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .strict_evidence = true,
    };
    char *error = NULL;
    bool found_hir_evidence_drift = false;
    bool checked = air_check_drift(&air, &error);

    for (size_t i = 0; i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && strstr(air.drifts[i].message,
                      "AIR implementation boundary has no matching HIR CFG evidence") != NULL
            && strstr(air.drifts[i].message, "spawn") != NULL
            && strstr(air.drifts[i].message, "parallel") != NULL) {
            found_hir_evidence_drift = true;
            break;
        }
    }

    bool ok = checked
        && air.drift_count == 1
        && found_hir_evidence_drift;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_task_group_boundary_requires_rir_and_hir_evidence(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "CoordinateWork",
            .step_name = "coordinate",
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_PARALLEL,
            .owner_name = "CoordinateWork",
            .source_name = "task-group",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .has_hir_routine_evidence = true,
            .has_hir_cfg_evidence = true,
            .hir_routine_evidence_name = "coordinate",
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .strict_evidence = true,
    };
    char *error = NULL;
    bool checked = air_check_drift(&air, &error);
    bool found_rir_evidence_drift = false;
    for (size_t i = 0; i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && air.drifts[i].message != NULL
            && strstr(air.drifts[i].message, "no matching RIR boundary evidence") != NULL
            && strstr(air.drifts[i].message, "task-group") != NULL) {
            found_rir_evidence_drift = true;
            break;
        }
    }
    bool ok = checked && air.drift_count == 1 && found_rir_evidence_drift;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_recheck_clears_owned_drift_messages(void)
{
    const char *authority_names[] = { "shipper" };
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .authority_required = true,
            .authority_names = authority_names,
            .authority_name_count = 1,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .strict_evidence = true,
    };
    char *error = NULL;
    bool first = air_check_drift(&air, &error);
    size_t first_count = air.drift_count;
    const char *first_message = first_count > 0 ? air.drifts[0].message : NULL;
    bool second = air_check_drift(&air, &error);
    bool ok = first
        && second
        && first_count >= 1
        && first_message != NULL
        && air.drift_count == first_count
        && air.drifts[0].message != NULL
        && strstr(air.drifts[0].message,
                  "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING") != NULL;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_invalid_boundary_inventory(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .authority_required = true,
            .authority_name_count = 1,
            .authority_names = NULL,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .strict_evidence = true,
    };
    char *error = NULL;
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "authority count without names") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_missing_inventory_arrays(void)
{
    AIRProgram missing_intents = {
        .intent_count = 1,
    };
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRProgram missing_boundaries = {
        .intents = intents,
        .intent_count = 1,
        .boundary_count = 1,
    };
    AIRProgram missing_drifts = {
        .intents = intents,
        .intent_count = 1,
        .drift_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&missing_intents, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "intent count without intent array") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&missing_boundaries, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "boundary count without boundary array") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&missing_drifts, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "drift count without drift array") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_boundary_step_mismatch(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 7,
            .sync_class = AIR_SYNC_SYNC,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "step index does not match intent") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_boundary_owner_mismatch(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "OtherIntent",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "owner does not match intent") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_boundary_sync_shape_mismatch(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "handoff",
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .failure_class = AIR_FAILURE_COMPENSABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_WORLD,
            .owner_name = "ShipOrder",
            .source_name = "remote",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "invalid sync class sync for world boundary") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_invalid_drift_inventory(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
        },
    };
    AIRDrift drifts[] = {
        {
            .kind = AIR_DRIFT_NONE,
            .intent_index = 0,
            .boundary_index = 0,
            .message = "stale placeholder",
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .drifts = drifts,
        .drift_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "drift node 0 has invalid kind") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_authority_evidence_shape_mismatch(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    const char *authority_names[] = { "shipper" };
    AIRBoundaryNode authority_without_boundary[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .authority_required = true,
            .authority_names = authority_names,
            .authority_name_count = 1,
            .has_rir_authority_evidence = true,
            .rir_authority_evidence_name = "shipper",
        },
    };
    AIRBoundaryNode authority_on_non_authority[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .has_rir_boundary_evidence = true,
            .rir_boundary_evidence_scope = "WarehouseZone",
            .has_rir_authority_evidence = true,
            .rir_authority_evidence_name = "shipper",
        },
    };
    AIRBoundaryNode undeclared_authority[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .authority_required = true,
            .authority_names = authority_names,
            .authority_name_count = 1,
            .has_rir_boundary_evidence = true,
            .rir_boundary_evidence_scope = "WarehouseZone",
            .has_rir_authority_evidence = true,
            .rir_authority_evidence_name = "carrier",
        },
    };
    AIRProgram missing_boundary = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = authority_without_boundary,
        .boundary_count = 1,
    };
    AIRProgram non_authority = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = authority_on_non_authority,
        .boundary_count = 1,
    };
    AIRProgram mismatched_authority = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = undeclared_authority,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&missing_boundary, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "authority evidence without boundary evidence") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&non_authority, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "authority evidence on non-authority boundary") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&mismatched_authority, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "authority evidence for undeclared participant") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_cfg_evidence_without_routine(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "Work",
            .step_name = "run",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_EXECUTION,
            .owner_name = "Work",
            .source_name = "with",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .has_hir_cfg_evidence = true,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .strict_evidence = true,
    };
    char *error = NULL;
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "HIR CFG evidence without routine evidence") != NULL;
    free(error);
    return ok;
}

static bool
test_air_check_drift_is_verify_compatibility_wrapper(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool checked = air_check_drift(&air, &error);
    bool ok = checked
        && air.drift_count == 1
        && air.drifts[0].kind == AIR_DRIFT_SYNC_ASYNC_CONFLICT;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_collects_hir_and_rir_evidence(void)
{
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "ShipOrder", .ast = NULL },
    };
    const char *authorized_by[] = { "shipper" };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "reserve",
            .where_type_name = "WarehouseZone",
            .authorized_by = authorized_by,
            .authorized_by_count = 1,
        },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    HIRBasicBlock reserve_blocks[] = {
        { .id = 0, .terminator_kind = HIR_BLOCK_RETURN, .is_reachable = true },
    };
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_FUNCTION,
            .owner_name = "ShipOrder",
            .name = "reserve",
            .has_cfg = true,
            .cfg = {
                .blocks = reserve_blocks,
                .block_count = 1,
                .entry_block = 0,
            },
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    RIRFact facts[] = {
        {
            .kind = RIR_FACT_AUTHORITY,
            .name = "shipper",
            .resource_kind = RIR_RESOURCE_AUTHORITY_HANDLE,
            .state = RIR_STATE_AUTHORIZED,
        },
    };
    RIROp ops[] = {
        { .kind = RIR_OP_AUTHORIZE, .subject = "shipper" },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_ZONE,
            .owner_name = "ShipOrder",
            .name = "WarehouseZone",
            .facts = facts,
            .fact_count = 1,
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    const char *dir_owner_before = dir.nodes[0].name;
    const char *dir_step_before = dir.intents[0].steps[0].name;
    const char *dir_where_before = dir.intents[0].steps[0].where_type_name;
    const char *dir_authority_before = dir.intents[0].steps[0].authorized_by[0];
    const char *hir_owner_before = hir.routines[0].owner_name;
    const char *hir_name_before = hir.routines[0].name;
    RIRScopeKind rir_kind_before = rir.scopes[0].kind;
    const char *rir_owner_before = rir.scopes[0].owner_name;
    const char *rir_name_before = rir.scopes[0].name;
    RIROpKind rir_op_before = rir.scopes[0].ops[0].kind;
    const char *rir_op_subject_before = rir.scopes[0].ops[0].subject;
    RIRFactKind rir_fact_before = rir.scopes[0].facts[0].kind;
    const char *rir_fact_name_before = rir.scopes[0].facts[0].name;
    char *error = NULL;
    AIRProgram *air = air_synthesize(&hir, &dir, &rir, &error);
    bool ok = air != NULL
        && air->hir_routine_evidence_count == 1
        && air->hir_cfg_evidence_count == 1
        && air->rir_boundary_evidence_count == 1
        && air->rir_authority_evidence_count == 2
        && air->boundaries[0].has_hir_routine_evidence
        && air->boundaries[0].has_hir_cfg_evidence
        && air->boundaries[0].has_rir_boundary_evidence
        && air->boundaries[0].has_rir_authority_evidence
        && air->boundaries[0].hir_routine_evidence_name != NULL
        && strcmp(air->boundaries[0].hir_routine_evidence_name, "reserve") == 0
        && air->boundaries[0].rir_boundary_evidence_scope != NULL
        && strcmp(air->boundaries[0].rir_boundary_evidence_scope, "WarehouseZone") == 0
        && air->boundaries[0].rir_authority_evidence_name != NULL
        && strcmp(air->boundaries[0].rir_authority_evidence_name, "shipper") == 0
        && dir.node_count == 1
        && dir.intent_count == 1
        && dir.intents[0].step_count == 1
        && dir.nodes[0].name == dir_owner_before
        && dir.intents[0].steps[0].name == dir_step_before
        && dir.intents[0].steps[0].where_type_name == dir_where_before
        && dir.intents[0].steps[0].authorized_by[0] == dir_authority_before
        && hir.routine_count == 1
        && hir.routines[0].owner_name == hir_owner_before
        && hir.routines[0].name == hir_name_before
        && rir.scope_count == 1
        && rir.scopes[0].kind == rir_kind_before
        && rir.scopes[0].owner_name == rir_owner_before
        && rir.scopes[0].name == rir_name_before
        && rir.scopes[0].op_count == 1
        && rir.scopes[0].ops[0].kind == rir_op_before
        && rir.scopes[0].ops[0].subject == rir_op_subject_before
        && rir.scopes[0].fact_count == 1
        && rir.scopes[0].facts[0].kind == rir_fact_before
        && rir.scopes[0].facts[0].name == rir_fact_name_before;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_rejects_mismatched_authority_evidence(void)
{
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "ShipOrder", .ast = NULL },
    };
    const char *authorized_by[] = { "shipper" };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "reserve",
            .where_type_name = "WarehouseZone",
            .authorized_by = authorized_by,
            .authorized_by_count = 1,
        },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    RIRFact facts[] = {
        {
            .kind = RIR_FACT_AUTHORITY,
            .name = "observer",
            .resource_kind = RIR_RESOURCE_AUTHORITY_HANDLE,
            .state = RIR_STATE_AUTHORIZED,
        },
    };
    RIROp ops[] = {
        { .kind = RIR_OP_AUTHORIZE, .subject = "observer" },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_ZONE,
            .owner_name = "ShipOrder",
            .name = "WarehouseZone",
            .facts = facts,
            .fact_count = 1,
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = air_synthesize(NULL, &dir, &rir, &error);
    bool found = false;
    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && strstr(air->drifts[i].message,
                          "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING") != NULL
                && strstr(air->drifts[i].message,
                          "expected authority participant(s): shipper") != NULL) {
                found = true;
                break;
            }
        }
    }
    bool ok = air != NULL
        && air->boundaries[0].has_rir_boundary_evidence
        && !air->boundaries[0].has_rir_authority_evidence
        && air->drift_count >= 1
        && found;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_dump_prints_evidence_provenance(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .authority_required = true,
            .has_hir_routine_evidence = true,
            .has_hir_cfg_evidence = true,
            .has_rir_boundary_evidence = true,
            .has_rir_authority_evidence = true,
            .hir_routine_evidence_name = "reserve",
            .rir_boundary_evidence_scope = "WarehouseZone",
            .rir_authority_evidence_name = "shipper",
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .strict_evidence = true,
        .has_hir_input = true,
    };
    char buffer[1024];
    FILE *out = tmpfile();
    size_t bytes;
    bool ok;

    if (out == NULL)
        return false;
    air_dump(&air, out);
    fflush(out);
    rewind(out);
    bytes = fread(buffer, 1, sizeof(buffer) - 1, out);
    buffer[bytes] = '\0';
    fclose(out);

    ok = strstr(buffer, "strict_evidence=yes hir_input=yes") != NULL
        && strstr(buffer, "evidence hir=yes(reserve) hir_cfg=yes") != NULL
        && strstr(buffer, "rir_boundary=yes(WarehouseZone)") != NULL
        && strstr(buffer, "rir_authority=yes(shipper)") != NULL;
    return ok;
}

static bool
test_air_world_boundary_requires_transfer_evidence(void)
{
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "Checkout", .ast = NULL },
    };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "Handoff",
            .transfer_from_alias = "cart",
            .transfer_to_alias = "payment",
        },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    RIROp unrelated_ops[] = {
        { .kind = RIR_OP_AUTHORIZE, .subject = "buyer" },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .name = "Checkout",
            .ops = unrelated_ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = air_synthesize(NULL, &dir, &rir, &error);
    bool found = false;

    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && strstr(air->drifts[i].message,
                          "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING") != NULL
                && strstr(air->drifts[i].message, "implementation boundary 'payment'") != NULL) {
                found = true;
                break;
            }
        }
    }

    bool ok = air != NULL
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_WORLD
        && !air->boundaries[0].has_rir_boundary_evidence
        && found;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_world_boundary_accepts_transfer_evidence(void)
{
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "Checkout", .ast = NULL },
    };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "Handoff",
            .transfer_from_alias = "cart",
            .transfer_to_alias = "payment",
        },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    RIROp transfer_ops[] = {
        { .kind = RIR_OP_MOVE, .subject = "cart", .arg0 = "payment", .arg1 = "Handoff" },
        { .kind = RIR_OP_CLAIM, .subject = "payment", .arg0 = "cart", .arg1 = "Handoff" },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .name = "Checkout",
            .ops = transfer_ops,
            .op_count = 2,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = air_synthesize(NULL, &dir, &rir, &error);
    bool ok = air != NULL
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_WORLD
        && air->boundaries[0].has_rir_boundary_evidence
        && air->drift_count == 0;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_world_boundary_rejects_mismatched_transfer_ast(void)
{
    ASTNode step_ast = { .type = AST_INTENT_STEP, .line = 31, .column = 5 };
    ASTNode unrelated_ast = { .type = AST_INTENT_STEP, .line = 44, .column = 9 };
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "Checkout", .ast = NULL },
    };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "Handoff",
            .transfer_from_alias = "cart",
            .transfer_to_alias = "payment",
            .ast = &step_ast,
        },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    RIROp transfer_ops[] = {
        {
            .kind = RIR_OP_MOVE,
            .subject = "cart",
            .arg0 = "payment",
            .arg1 = "Handoff",
            .ast = &unrelated_ast,
        },
        {
            .kind = RIR_OP_CLAIM,
            .subject = "payment",
            .arg0 = "cart",
            .arg1 = "Handoff",
            .ast = &unrelated_ast,
        },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .name = "Checkout",
            .ops = transfer_ops,
            .op_count = 2,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = air_synthesize(NULL, &dir, &rir, &error);
    bool found_missing_transfer_drift = false;

    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && air->drifts[i].boundary_index < air->boundary_count
                && air->boundaries[air->drifts[i].boundary_index].kind == AIR_BOUNDARY_WORLD
                && strstr(air->drifts[i].message, "implementation boundary 'payment'") != NULL) {
                found_missing_transfer_drift = true;
                break;
            }
        }
    }

    bool ok = air != NULL
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_WORLD
        && air->boundaries[0].ast == &step_ast
        && !air->boundaries[0].has_rir_boundary_evidence
        && found_missing_transfer_drift;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_lowers_from_source_without_drift(void)
{
    const char *source =
        "subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }\n"
        "ability Payable { func Pay() -> Void; }\n"
        "role BuyerPay for Buyer {\n"
        "    impl ability Payable { func Pay() -> Void { return; } }\n"
        "}\n"
        "effect PaymentEffect for bearer: Buyer { }\n"
        "zone PaymentZone {\n"
        "    subject slot buyer: Buyer\n"
        "    effect slot paymentFx: PaymentEffect\n"
        "    authority buyer requires Payable\n"
        "}\n"
        "intent Purchase(payment: PaymentZone, buyer: Buyer) {\n"
        "    step pay {\n"
        "        where: PaymentZone;\n"
        "        using: payment;\n"
        "        who: buyer;\n"
        "        requires: Payable;\n"
        "        authorized by: buyer;\n"
        "        causes: PaymentEffect;\n"
        "    }\n"
        "}\n";
    AIRProgram *air = lower_air_from_source(source);
    bool ok = air != NULL
        && air->intent_count == 1
        && air->boundary_count == 1
        && air->drift_count == 0
        && air->intents[0].ast != NULL
        && air->intents[0].ast->line > 0
        && air->boundaries[0].ast != NULL
        && air->boundaries[0].ast->line > 0
        && air->rir_authority_evidence_count > 0
        && air->boundaries[0].has_rir_boundary_evidence
        && air->boundaries[0].has_rir_authority_evidence;
    air_destroy(air);
    return ok;
}

static bool
test_air_synthesizes_spawn_boundary_from_step_ast(void)
{
    ASTNode intent_ast = { .line = 20, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("dispatch");
    ASTNode *call = ast_create_call(ast_create_identifier("Worker"));
    ASTNode *spawn = ast_create_spawn_expression(call);
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "ShipOrder", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "dispatch", .ast = step_ast },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    char *error = NULL;
    AIRProgram *air;
    bool found_sync_drift = false;
    bool found_evidence_drift = false;
    bool ok;

    if (step_ast == NULL || call == NULL || spawn == NULL) {
        ast_destroy(step_ast);
        if (spawn == NULL)
            ast_destroy(call);
        ast_destroy(spawn);
        return false;
    }
    step_ast->line = 21;
    step_ast->column = 5;
    spawn->line = 22;
    spawn->column = 9;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = spawn;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(NULL, &dir, NULL, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_SYNC_ASYNC_CONFLICT)
                found_sync_drift = true;
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING)
                found_evidence_drift = true;
        }
    }

    ok = air != NULL
        && air->intent_count == 1
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_PARALLEL
        && strcmp(air->boundaries[0].source_name, "spawn") == 0
        && air->boundaries[0].ast == spawn
        && air->boundaries[0].sync_class == AIR_SYNC_ASYNC
        && found_sync_drift
        && found_evidence_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_rejects_unmatched_top_level_intent_hir_evidence(void)
{
    ASTNode intent_ast = { .line = 24, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("dispatch");
    ASTNode *call = ast_create_call(ast_create_identifier("Worker"));
    ASTNode *spawn = ast_create_spawn_expression(call);
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "ShipOrder", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "dispatch", .ast = step_ast },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_INTENT,
            .name = "OtherIntent",
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    RIROp ops[] = {
        { .kind = RIR_OP_SPAWN, .subject = "spawn", .ast = spawn },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .name = "spawn",
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air;
    bool found_hir_routine_drift = false;
    bool found_hir_evidence_drift = false;
    bool ok;

    if (step_ast == NULL || call == NULL || spawn == NULL) {
        ast_destroy(step_ast);
        if (spawn == NULL)
            ast_destroy(call);
        ast_destroy(spawn);
        return false;
    }
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = spawn;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(&hir, &dir, &rir, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && strstr(air->drifts[i].message,
                          "AIR boundary has no matching HIR routine evidence") != NULL) {
                found_hir_routine_drift = true;
            }
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && strstr(air->drifts[i].message,
                          "AIR implementation boundary has no matching HIR CFG evidence") != NULL) {
                found_hir_evidence_drift = true;
            }
        }
    }

    ok = air != NULL
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_PARALLEL
        && !air->boundaries[0].has_hir_routine_evidence
        && air->boundaries[0].has_rir_boundary_evidence
        && found_hir_routine_drift
        && found_hir_evidence_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_synthesizes_io_boundary_without_sync_drift(void)
{
    ASTNode intent_ast = { .line = 30, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("load");
    ASTNode *call = ast_create_call(ast_create_identifier("ReadFile"));
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "LoadConfig", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "load", .ast = step_ast },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    char *error = NULL;
    AIRProgram *air;
    bool found_sync_drift = false;
    bool ok;

    if (step_ast == NULL || call == NULL) {
        ast_destroy(step_ast);
        ast_destroy(call);
        return false;
    }
    step_ast->line = 31;
    step_ast->column = 5;
    call->line = 32;
    call->column = 9;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(call);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = call;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(NULL, &dir, NULL, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_SYNC_ASYNC_CONFLICT)
                found_sync_drift = true;
        }
    }

    ok = air != NULL
        && air->intent_count == 1
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_IO
        && strcmp(air->boundaries[0].source_name, "ReadFile") == 0
        && air->boundaries[0].ast == call
        && air->boundaries[0].sync_class == AIR_SYNC_EITHER
        && !found_sync_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_await_boundary_accepts_exact_rir_evidence(void)
{
    ASTNode intent_ast = { .line = 34, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("join");
    ASTNode *await_expr = ast_create_await_expression(ast_create_identifier("task"));
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "JoinWork", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "join", .ast = step_ast },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    ASTNode *join_statements[] = { await_expr };
    HIRBasicBlock join_blocks[] = {
        {
            .id = 0,
            .statements = join_statements,
            .statement_count = 1,
            .terminator_kind = HIR_BLOCK_RETURN,
            .is_reachable = true,
        },
    };
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_INTENT,
            .owner_name = "JoinWork",
            .name = "join",
            .has_cfg = true,
            .cfg = {
                .blocks = join_blocks,
                .block_count = 1,
                .entry_block = 0,
            },
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    RIROp ops[] = {
        {
            .kind = RIR_OP_AWAIT_REMOTE,
            .subject = "task",
            .ast = await_expr,
        },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .owner_name = "JoinWork",
            .name = "join",
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air;
    bool found_evidence_drift = false;
    bool ok;

    if (step_ast == NULL || await_expr == NULL) {
        ast_destroy(step_ast);
        ast_destroy(await_expr);
        return false;
    }
    await_expr->line = 35;
    await_expr->column = 9;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(await_expr);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = await_expr;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(&hir, &dir, &rir, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING)
                found_evidence_drift = true;
        }
    }

    ok = air != NULL
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_PARALLEL
        && strcmp(air->boundaries[0].source_name, "await") == 0
        && air->boundaries[0].ast == await_expr
        && air->boundaries[0].has_hir_routine_evidence
        && air->boundaries[0].has_hir_cfg_evidence
        && air->boundaries[0].has_rir_boundary_evidence
        && !found_evidence_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_await_boundary_rejects_generic_rir_scope_evidence(void)
{
    ASTNode intent_ast = { .line = 36, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("join");
    ASTNode *await_expr = ast_create_await_expression(ast_create_identifier("task"));
    ASTNode unrelated_await_ast = { .type = AST_AWAIT_EXPR, .line = 37, .column = 9 };
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "JoinWork", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "join", .ast = step_ast },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    ASTNode *join_statements[] = { await_expr };
    HIRBasicBlock join_blocks[] = {
        {
            .id = 0,
            .statements = join_statements,
            .statement_count = 1,
            .terminator_kind = HIR_BLOCK_RETURN,
            .is_reachable = true,
        },
    };
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_INTENT,
            .owner_name = "JoinWork",
            .name = "join",
            .has_cfg = true,
            .cfg = {
                .blocks = join_blocks,
                .block_count = 1,
                .entry_block = 0,
            },
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    RIROp ops[] = {
        {
            .kind = RIR_OP_AWAIT_REMOTE,
            .subject = "task",
            .ast = &unrelated_await_ast,
        },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .owner_name = "JoinWork",
            .name = "await",
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air;
    bool found_rir_evidence_drift = false;
    bool ok;

    if (step_ast == NULL || await_expr == NULL) {
        ast_destroy(step_ast);
        ast_destroy(await_expr);
        return false;
    }
    await_expr->line = 36;
    await_expr->column = 9;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(await_expr);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = await_expr;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(&hir, &dir, &rir, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && air->drifts[i].message != NULL
                && strstr(air->drifts[i].message, "no matching RIR boundary evidence") != NULL) {
                found_rir_evidence_drift = true;
                break;
            }
        }
    }

    ok = air != NULL
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_PARALLEL
        && strcmp(air->boundaries[0].source_name, "await") == 0
        && air->boundaries[0].ast == await_expr
        && air->boundaries[0].has_hir_routine_evidence
        && air->boundaries[0].has_hir_cfg_evidence
        && !air->boundaries[0].has_rir_boundary_evidence
        && found_rir_evidence_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_channel_boundary_accepts_exact_rir_op_evidence(void)
{
    ASTNode intent_ast = { .line = 36, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("coordinate");
    ASTNode *send = ast_create_channel_send(ast_create_identifier("ch"),
                                            ast_create_number("1"));
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "CoordinateWork", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "coordinate", .ast = step_ast },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    ASTNode *statements[] = { send };
    HIRBasicBlock blocks[] = {
        {
            .id = 0,
            .statements = statements,
            .statement_count = 1,
            .terminator_kind = HIR_BLOCK_RETURN,
            .is_reachable = true,
        },
    };
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_INTENT,
            .owner_name = "CoordinateWork",
            .name = "coordinate",
            .has_cfg = true,
            .cfg = {
                .blocks = blocks,
                .block_count = 1,
                .entry_block = 0,
            },
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    RIROp ops[] = {
        { .kind = RIR_OP_CHANNEL_SEND, .subject = "ch", .arg0 = "1", .ast = send },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .name = "CoordinateWork",
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = NULL;
    bool found_evidence_drift = false;
    bool ok;

    if (step_ast == NULL || send == NULL) {
        ast_destroy(step_ast);
        ast_destroy(send);
        return false;
    }
    send->line = 37;
    send->column = 9;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(send);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = send;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(&hir, &dir, &rir, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING)
                found_evidence_drift = true;
        }
    }

    ok = air != NULL
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_CHANNEL
        && strcmp(air->boundaries[0].source_name, "channel-send") == 0
        && air->boundaries[0].has_hir_cfg_evidence
        && air->boundaries[0].has_rir_boundary_evidence
        && !found_evidence_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_hir_evidence_accepts_nested_execution_boundary_ast(void)
{
    ASTNode intent_ast = { .line = 38, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("read");
    ASTNode *with_stmt = ast_create_with_statement();
    ASTNode *read_call = ast_create_call(ast_create_identifier("ReadFile"));
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "NestedIO", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "read", .ast = step_ast },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    ASTNode *statements[] = { with_stmt };
    HIRBasicBlock blocks[] = {
        {
            .id = 0,
            .statements = statements,
            .statement_count = 1,
            .terminator_kind = HIR_BLOCK_RETURN,
            .is_reachable = true,
        },
    };
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_INTENT,
            .owner_name = "NestedIO",
            .name = "read",
            .has_cfg = true,
            .cfg = {
                .blocks = blocks,
                .block_count = 1,
                .entry_block = 0,
            },
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    RIROp ops[] = {
        { .kind = RIR_OP_IO, .subject = "ReadFile", .ast = read_call },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .name = "NestedIO",
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = NULL;
    bool found_io = false;
    bool found_with = false;
    bool found_evidence_drift = false;
    bool ok;

    if (step_ast == NULL || with_stmt == NULL || read_call == NULL) {
        ast_destroy(step_ast);
        ast_destroy(with_stmt);
        ast_destroy(read_call);
        return false;
    }
    with_stmt->line = 39;
    with_stmt->column = 9;
    read_call->line = 40;
    read_call->column = 13;
    with_stmt->data.with_stmt.body = ast_create_block();
    if (with_stmt->data.with_stmt.body == NULL) {
        ast_destroy(step_ast);
        ast_destroy(with_stmt);
        ast_destroy(read_call);
        return false;
    }
    ast_add_statement(with_stmt->data.with_stmt.body, read_call);
    read_call = NULL;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(with_stmt);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = with_stmt;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(&hir, &dir, &rir, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_EXECUTION
                && strcmp(boundary->source_name, "with") == 0
                && boundary->has_hir_cfg_evidence) {
                found_with = true;
            }
            if (boundary->kind == AIR_BOUNDARY_IO
                && strcmp(boundary->source_name, "ReadFile") == 0
                && boundary->has_hir_cfg_evidence
                && boundary->has_rir_boundary_evidence) {
                found_io = true;
            }
        }
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING)
                found_evidence_drift = true;
        }
    }

    ok = air != NULL
        && air->boundary_count == 2
        && found_with
        && found_io
        && !found_evidence_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_synthesizes_stable_io_boundary_builtin_set(void)
{
    const char *io_names[] = {
        "FileOpen",
        "FileRead",
        "FileWrite",
        "FileClose",
        "ReadFile",
        "WriteFile",
        "Input",
        "ReadLine",
        "Now",
        "Sleep",
    };
    ASTNode intent_ast = { .line = 40, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("touch");
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "ExternalEffect", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "touch", .ast = step_ast },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    char *error = NULL;
    AIRProgram *air;
    bool ok = true;

    if (step_ast == NULL)
        return false;
    step_ast->line = 41;
    step_ast->column = 5;
    step_ast->data.intent_step.on_exprs =
        (ASTNode **)calloc(sizeof(io_names) / sizeof(io_names[0]), sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        return false;
    }
    step_ast->data.intent_step.on_expr_count = sizeof(io_names) / sizeof(io_names[0]);
    for (size_t i = 0; i < sizeof(io_names) / sizeof(io_names[0]); i++) {
        ASTNode *call = ast_create_call(ast_create_identifier(io_names[i]));
        if (call == NULL) {
            ast_destroy(step_ast);
            return false;
        }
        call->line = 42 + (int)i;
        call->column = 9;
        step_ast->data.intent_step.on_exprs[i] = call;
    }

    air = air_synthesize(NULL, &dir, NULL, &error);
    ok = air != NULL
        && air->intent_count == 1
        && air->boundary_count == sizeof(io_names) / sizeof(io_names[0]);
    if (ok) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            ok = air->boundaries[i].kind == AIR_BOUNDARY_IO
                && air->boundaries[i].source_name != NULL
                && strcmp(air->boundaries[i].source_name, io_names[i]) == 0
                && air->boundaries[i].sync_class == AIR_SYNC_EITHER
                && air->boundaries[i].ast == step_ast->data.intent_step.on_exprs[i];
            if (!ok)
                break;
        }
    }
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_hir_evidence_accepts_loop_condition_boundary_ast(void)
{
    ASTNode intent_ast = { .line = 42, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("poll");
    ASTNode *while_stmt = ast_create_while_loop();
    ASTNode *read_call = ast_create_call(ast_create_identifier("ReadFile"));
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "LoopIO", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "poll", .ast = step_ast },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    ASTNode *statements[] = { while_stmt };
    HIRBasicBlock blocks[] = {
        {
            .id = 0,
            .statements = statements,
            .statement_count = 1,
            .terminator_kind = HIR_BLOCK_RETURN,
            .is_reachable = true,
        },
    };
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_INTENT,
            .owner_name = "LoopIO",
            .name = "poll",
            .has_cfg = true,
            .cfg = {
                .blocks = blocks,
                .block_count = 1,
                .entry_block = 0,
            },
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    RIROp ops[] = {
        { .kind = RIR_OP_IO, .subject = "ReadFile", .ast = read_call },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .name = "LoopIO",
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = NULL;
    bool found_io = false;
    bool found_evidence_drift = false;
    bool ok;

    if (step_ast == NULL || while_stmt == NULL || read_call == NULL) {
        ast_destroy(step_ast);
        ast_destroy(while_stmt);
        ast_destroy(read_call);
        return false;
    }
    while_stmt->line = 43;
    while_stmt->column = 9;
    read_call->line = 43;
    read_call->column = 16;
    while_stmt->data.while_loop.condition = read_call;
    while_stmt->data.while_loop.body = ast_create_block();
    if (while_stmt->data.while_loop.body == NULL) {
        ast_destroy(step_ast);
        ast_destroy(while_stmt);
        return false;
    }
    read_call = NULL;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(while_stmt);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = while_stmt;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(&hir, &dir, &rir, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_IO
                && strcmp(boundary->source_name, "ReadFile") == 0
                && boundary->has_hir_cfg_evidence
                && boundary->has_rir_boundary_evidence) {
                found_io = true;
            }
        }
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING)
                found_evidence_drift = true;
        }
    }

    ok = air != NULL
        && air->boundary_count == 1
        && found_io
        && !found_evidence_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_synthesizes_stable_execution_boundary_set(void)
{
    ASTNode intent_ast = { .line = 50, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("coordinate");
    ASTNode *parallel = ast_create_parallel_block();
    ASTNode *async_block = ast_create_async_block();
    ASTNode *await_expr = ast_create_await_expression(ast_create_identifier("task"));
    ASTNode *task_group = ast_create_task_group(true);
    ASTNode *send = ast_create_channel_send(ast_create_identifier("ch"),
                                            ast_create_number("1"));
    ASTNode *recv = ast_create_channel_recv(ast_create_identifier("ch"));
    ASTNode *select_stmt = ast_create_select_statement();
    ASTNode *with_stmt = ast_create_with_statement();
    ASTNode *unsafe_block = ast_create_unsafe_block(ast_create_block());
    ASTNode *defer_stmt = ast_create_defer_statement(ast_create_block());
    ASTNode *pin_block = ast_create_block();
    ASTNode *nested_read = ast_create_call(ast_create_identifier("ReadFile"));
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "CoordinateWork", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "coordinate", .ast = step_ast },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    AIRProgram *air = NULL;
    char *error = NULL;
    bool found_parallel = false;
    bool found_async = false;
    bool found_await = false;
    bool found_task_group = false;
    bool found_send = false;
    bool found_recv = false;
    bool found_select = false;
    bool found_with = false;
    bool found_unsafe = false;
    bool found_defer = false;
    bool found_pin = false;
    bool found_nested_io = false;
    bool ok;

    if (step_ast == NULL || parallel == NULL || async_block == NULL || await_expr == NULL
        || task_group == NULL
        || send == NULL || recv == NULL || select_stmt == NULL
        || with_stmt == NULL || unsafe_block == NULL || defer_stmt == NULL
        || pin_block == NULL || nested_read == NULL) {
        ast_destroy(step_ast);
        ast_destroy(parallel);
        ast_destroy(async_block);
        ast_destroy(await_expr);
        ast_destroy(task_group);
        ast_destroy(send);
        ast_destroy(recv);
        ast_destroy(select_stmt);
        ast_destroy(with_stmt);
        ast_destroy(unsafe_block);
        ast_destroy(defer_stmt);
        ast_destroy(pin_block);
        ast_destroy(nested_read);
        return false;
    }
    parallel->line = 51;
    async_block->line = 52;
    await_expr->line = 53;
    task_group->line = 54;
    send->line = 55;
    recv->line = 56;
    select_stmt->line = 57;
    with_stmt->line = 58;
    unsafe_block->line = 59;
    defer_stmt->line = 60;
    pin_block->line = 61;
    nested_read->line = 62;
    pin_block->data.block.is_pin_block = true;
    with_stmt->data.with_stmt.body = ast_create_block();
    if (with_stmt->data.with_stmt.body == NULL) {
        ast_destroy(step_ast);
        ast_destroy(parallel);
        ast_destroy(async_block);
        ast_destroy(await_expr);
        ast_destroy(task_group);
        ast_destroy(send);
        ast_destroy(recv);
        ast_destroy(select_stmt);
        ast_destroy(with_stmt);
        ast_destroy(unsafe_block);
        ast_destroy(defer_stmt);
        ast_destroy(pin_block);
        ast_destroy(nested_read);
        return false;
    }
    ast_add_statement(with_stmt->data.with_stmt.body, nested_read);
    nested_read = NULL;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(11, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(parallel);
        ast_destroy(async_block);
        ast_destroy(await_expr);
        ast_destroy(task_group);
        ast_destroy(send);
        ast_destroy(recv);
        ast_destroy(select_stmt);
        ast_destroy(with_stmt);
        ast_destroy(unsafe_block);
        ast_destroy(defer_stmt);
        ast_destroy(pin_block);
        ast_destroy(nested_read);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = parallel;
    step_ast->data.intent_step.on_exprs[1] = async_block;
    step_ast->data.intent_step.on_exprs[2] = await_expr;
    step_ast->data.intent_step.on_exprs[3] = task_group;
    step_ast->data.intent_step.on_exprs[4] = send;
    step_ast->data.intent_step.on_exprs[5] = recv;
    step_ast->data.intent_step.on_exprs[6] = select_stmt;
    step_ast->data.intent_step.on_exprs[7] = with_stmt;
    step_ast->data.intent_step.on_exprs[8] = unsafe_block;
    step_ast->data.intent_step.on_exprs[9] = defer_stmt;
    step_ast->data.intent_step.on_exprs[10] = pin_block;
    step_ast->data.intent_step.on_expr_count = 11;

    air = air_synthesize(NULL, &dir, NULL, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_PARALLEL
                && strcmp(boundary->source_name, "parallel") == 0)
                found_parallel = true;
            if (boundary->kind == AIR_BOUNDARY_PARALLEL
                && strcmp(boundary->source_name, "async") == 0)
                found_async = true;
            if (boundary->kind == AIR_BOUNDARY_PARALLEL
                && strcmp(boundary->source_name, "await") == 0)
                found_await = true;
            if (boundary->kind == AIR_BOUNDARY_PARALLEL
                && strcmp(boundary->source_name, "task-group") == 0)
                found_task_group = true;
            if (boundary->kind == AIR_BOUNDARY_CHANNEL
                && strcmp(boundary->source_name, "channel-send") == 0)
                found_send = true;
            if (boundary->kind == AIR_BOUNDARY_CHANNEL
                && strcmp(boundary->source_name, "channel-recv") == 0)
                found_recv = true;
            if (boundary->kind == AIR_BOUNDARY_CHANNEL
                && strcmp(boundary->source_name, "select") == 0)
                found_select = true;
            if (boundary->kind == AIR_BOUNDARY_EXECUTION
                && strcmp(boundary->source_name, "with") == 0)
                found_with = true;
            if (boundary->kind == AIR_BOUNDARY_EXECUTION
                && strcmp(boundary->source_name, "unsafe") == 0)
                found_unsafe = true;
            if (boundary->kind == AIR_BOUNDARY_EXECUTION
                && strcmp(boundary->source_name, "defer") == 0)
                found_defer = true;
            if (boundary->kind == AIR_BOUNDARY_EXECUTION
                && strcmp(boundary->source_name, "pin") == 0)
                found_pin = true;
            if (boundary->kind == AIR_BOUNDARY_IO
                && boundary->ast != NULL
                && boundary->ast->line == 62
                && strcmp(boundary->source_name, "ReadFile") == 0)
                found_nested_io = true;
        }
    }
    ok = air != NULL
        && air->boundary_count == 12
        && found_parallel
        && found_async
        && found_await
        && found_task_group
        && found_send
        && found_recv
        && found_select
        && found_with
        && found_unsafe
        && found_defer
        && found_pin
        && found_nested_io;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_parsed_io_boundary_accepts_exact_rir_evidence(void)
{
    const char *source =
        "subject Loader { let hp: Int; }\n"
        "zone LoadZone {\n"
        "    subject slot loader: Loader\n"
        "}\n"
        "intent Load(load: LoadZone, loader: Loader) {\n"
        "    step read {\n"
        "        where: LoadZone;\n"
        "        using: load;\n"
        "        who: loader;\n"
        "        on: ReadFile(\"missing.txt\");\n"
        "        expect: true;\n"
        "    }\n"
        "    success: true;\n"
        "    failure: false;\n"
        "}\n";
    AIRProgram *air = lower_air_from_source(source);
    bool found_io = false;
    bool found_io_evidence = false;
    bool found_io_drift = false;

    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && air->drifts[i].boundary_index < air->boundary_count) {
                const AIRBoundaryNode *boundary =
                    &air->boundaries[air->drifts[i].boundary_index];
                if (air->drifts[i].boundary_index < air->boundary_count) {
                    if (boundary->kind == AIR_BOUNDARY_IO
                        && boundary->source_name != NULL
                        && strcmp(boundary->source_name, "ReadFile") == 0) {
                        found_io_drift = true;
                    }
                }
            }
        }
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_IO
                && boundary->source_name != NULL
                && strcmp(boundary->source_name, "ReadFile") == 0
                && boundary->sync_class == AIR_SYNC_EITHER
                && boundary->ast != NULL
                && boundary->ast->type == AST_CALL
                && boundary->ast->line > 0) {
                found_io = true;
                found_io_evidence = boundary->has_hir_cfg_evidence
                    && boundary->has_rir_boundary_evidence;
            }
        }
    }

    bool ok = air != NULL
        && air->boundary_count >= 2
        && found_io
        && found_io_evidence
        && !found_io_drift;
    air_destroy(air);
    return ok;
}

static bool
test_air_parsed_transfer_emits_zone_and_world_boundaries(void)
{
    const char *source =
        "subject Buyer { let hp: Int; action Promote(self) -> Void { hp = hp + 1; } }\n"
        "zone CartZone {\n"
        "    subject slot buyer: Buyer\n"
        "    authority buyer\n"
        "}\n"
        "zone PaymentZone {\n"
        "    subject slot buyer: Buyer\n"
        "    authority buyer\n"
        "}\n"
        "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
        "    step Handoff {\n"
        "        where: PaymentZone;\n"
        "        using: payment;\n"
        "        transfer: cart -> payment;\n"
        "        who: buyer;\n"
        "        authorized by: buyer;\n"
        "        on: buyer.Promote();\n"
        "        expect: true;\n"
        "    }\n"
        "    success: true;\n"
        "    failure: false;\n"
        "}\n";
    AIRProgram *air = lower_air_from_source(source);
    bool found_zone = false;
    bool found_world = false;
    bool found_zone_evidence = false;
    bool found_world_evidence = false;

    if (air != NULL) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            if (air->boundaries[i].kind == AIR_BOUNDARY_ZONE
                && strcmp(air->boundaries[i].source_name, "PaymentZone") == 0) {
                found_zone = true;
                found_zone_evidence = air->boundaries[i].has_rir_boundary_evidence
                    && air->boundaries[i].has_rir_authority_evidence
                    && air->boundaries[i].rir_boundary_evidence_scope != NULL
                    && air->boundaries[i].rir_authority_evidence_name != NULL
                    && strcmp(air->boundaries[i].rir_authority_evidence_name, "buyer") == 0;
            }
            if (air->boundaries[i].kind == AIR_BOUNDARY_WORLD
                && strcmp(air->boundaries[i].source_name, "payment") == 0) {
                found_world = true;
                found_world_evidence = air->boundaries[i].has_rir_boundary_evidence
                    && air->boundaries[i].has_rir_authority_evidence
                    && air->boundaries[i].rir_boundary_evidence_scope != NULL
                    && air->boundaries[i].rir_authority_evidence_name != NULL
                    && strcmp(air->boundaries[i].rir_authority_evidence_name, "buyer") == 0;
            }
        }
    }

    bool ok = air != NULL
        && air->drift_count == 0
        && air->boundary_count >= 2
        && found_zone
        && found_world
        && found_zone_evidence
        && found_world_evidence;
    air_destroy(air);
    return ok;
}

static bool
test_air_parsed_transfer_reports_zone_missing_authority_evidence(void)
{
    const char *source =
        "subject Buyer { let hp: Int; action Promote(self) -> Void { hp = hp + 1; } }\n"
        "zone CartZone {\n"
        "    subject slot buyer: Buyer\n"
        "}\n"
        "zone PaymentZone {\n"
        "    subject slot buyer: Buyer\n"
        "}\n"
        "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
        "    step Handoff {\n"
        "        where: PaymentZone;\n"
        "        using: payment;\n"
        "        transfer: cart -> payment;\n"
        "        who: buyer;\n"
        "        authorized by: buyer;\n"
        "        on: buyer.Promote();\n"
        "        expect: true;\n"
        "    }\n"
        "    success: true;\n"
        "    failure: false;\n"
        "}\n";
    AIRProgram *air = lower_air_from_source(source);
    bool found_zone = false;
    bool found_zone_authority_drift = false;
    bool found_world_transfer_evidence = false;

    if (air != NULL) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_ZONE
                && boundary->source_name != NULL
                && strcmp(boundary->source_name, "PaymentZone") == 0
                && boundary->has_rir_boundary_evidence
                && !boundary->has_rir_authority_evidence) {
                found_zone = true;
            }
            if (boundary->kind == AIR_BOUNDARY_WORLD
                && boundary->source_name != NULL
                && strcmp(boundary->source_name, "payment") == 0
                && boundary->has_rir_boundary_evidence) {
                found_world_transfer_evidence = true;
            }
        }
        for (size_t i = 0; i < air->drift_count; i++) {
            const AIRDrift *drift = &air->drifts[i];
            if (drift->kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && drift->boundary_index < air->boundary_count
                && air->boundaries[drift->boundary_index].kind == AIR_BOUNDARY_ZONE
                && drift->message != NULL
                && strstr(drift->message, "expected authority participant(s): buyer") != NULL) {
                found_zone_authority_drift = true;
            }
        }
    }

    bool ok = air != NULL
        && air->drift_count >= 1
        && found_zone
        && found_zone_authority_drift
        && found_world_transfer_evidence;
    air_destroy(air);
    return ok;
}

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

    TEST("AIR verify rejects invalid drift inventory");
    EXPECT(test_air_verify_rejects_invalid_drift_inventory());

    TEST("AIR verify rejects authority evidence shape mismatch");
    EXPECT(test_air_verify_rejects_authority_evidence_shape_mismatch());

    TEST("AIR verify rejects CFG evidence without routine evidence");
    EXPECT(test_air_verify_rejects_cfg_evidence_without_routine());

    TEST("AIR check_drift remains verify compatibility wrapper");
    EXPECT(test_air_check_drift_is_verify_compatibility_wrapper());

    TEST("AIR synthesis collects HIR/RIR evidence without mutation");
    EXPECT(test_air_collects_hir_and_rir_evidence());

    TEST("AIR strict evidence rejects mismatched authority participant");
    EXPECT(test_air_rejects_mismatched_authority_evidence());

    TEST("AIR dump prints evidence provenance");
    EXPECT(test_air_dump_prints_evidence_provenance());

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

    printf("\nAIR tests: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
