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
test_air_recheck_clears_owned_drift_messages(void)
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
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_FUNCTION,
            .owner_name = "ShipOrder",
            .name = "reserve",
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
        && air->rir_boundary_evidence_count == 1
        && air->rir_authority_evidence_count == 2
        && air->boundaries[0].has_hir_routine_evidence
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

    ok = strstr(buffer, "evidence hir=yes(reserve)") != NULL
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
test_air_parsed_io_boundary_reports_missing_evidence(void)
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
    bool found = false;
    bool found_io = false;
    bool found_io_drift = false;

    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && strstr(air->drifts[i].message,
                          "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING") != NULL) {
                found = true;
                if (air->drifts[i].boundary_index < air->boundary_count) {
                    const AIRBoundaryNode *boundary =
                        &air->boundaries[air->drifts[i].boundary_index];
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
                && boundary->ast->line > 0) {
                found_io = true;
            }
        }
    }

    bool ok = air != NULL
        && air->boundary_count >= 2
        && air->drift_count >= 1
        && found
        && found_io
        && found_io_drift;
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

    TEST("AIR recheck clears owned drift messages");
    EXPECT(test_air_recheck_clears_owned_drift_messages());

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

    TEST("AIR lowers parsed intent source without drift");
    EXPECT(test_air_lowers_from_source_without_drift());

    TEST("AIR synthesis captures spawn boundary from intent step AST");
    EXPECT(test_air_synthesizes_spawn_boundary_from_step_ast());

    TEST("AIR synthesis captures IO boundary without sync drift");
    EXPECT(test_air_synthesizes_io_boundary_without_sync_drift());

    TEST("AIR parsed IO boundary reports missing evidence");
    EXPECT(test_air_parsed_io_boundary_reports_missing_evidence());

    TEST("AIR parsed transfer emits zone and world boundaries");
    EXPECT(test_air_parsed_transfer_emits_zone_and_world_boundaries());

    printf("\nAIR tests: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
