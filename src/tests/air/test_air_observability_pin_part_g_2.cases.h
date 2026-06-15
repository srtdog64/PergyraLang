static bool
test_air_strict_evidence_rejects_frontier_counter_only(void)
{
    AIRProgram air = {
        .runtime_frontier_policy_evidence_count = 1,
        .strict_evidence = true,
    };
    char *error = NULL;
    bool found = false;
    bool ok = air_verify(&air, &error);

    for (size_t i = 0; ok && i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && strstr(air.drifts[i].message,
                      "AIR has no runtime frontier policy evidence") != NULL) {
            found = true;
            break;
        }
    }
    ok = ok && found;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_runtime_frontier_policy_names_publish_order(void)
{
    bool ok = PGY_FRONTIER_PASS_LIMIT_FACT_COUNT == 10
        && PGY_FRONTIER_OVERFLOW_REASON_FACT_COUNT == 5
        && PGY_FRONTIER_POLICY_FACT_COUNT == 15
        && pgy_frontier_embedded_zone_member_count(2, 3) == 5
        && pgy_frontier_embedded_zone_member_count((size_t)UINT32_MAX, 1)
            == (size_t)UINT32_MAX
        && pgy_frontier_publish_order_is_valid(
            PGY_FRONTIER_PUBLISH_WRITE_VALUE,
            PGY_FRONTIER_PUBLISH_READY)
        && pgy_frontier_publish_order_is_valid(
            PGY_FRONTIER_PUBLISH_READY,
            PGY_FRONTIER_PUBLISH_CLEAR_DIRTY)
        && !pgy_frontier_publish_order_is_valid(
            PGY_FRONTIER_PUBLISH_CLEAR_DIRTY,
            PGY_FRONTIER_PUBLISH_READY)
        && !pgy_frontier_publish_order_is_valid(
            (PgyFrontierPublishPhase)99,
            PGY_FRONTIER_PUBLISH_READY);
    return ok;
}

static bool
test_air_collects_mir_pin_cleanup_evidence(void)
{
    ASTNode pin_ast;
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock blocks[2];
    MIRInstruction insts[2];
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;
    memset(&pin_ast, 0, sizeof(pin_ast));
    pin_ast.type = AST_BLOCK;
    pin_ast.data.block.is_pin_block = true;

    air->intents = (AIRIntentNode *)calloc(1, sizeof(AIRIntentNode));
    air->boundaries = (AIRBoundaryNode *)calloc(1, sizeof(AIRBoundaryNode));
    if (air->intents == NULL || air->boundaries == NULL) {
        air_destroy(air);
        return false;
    }
    air->intent_count = 1;
    air->boundary_count = 1;
    air->intents[0].intent_owner = "ScoreIntent";
    air->intents[0].step_name = "pin_scores";
    air->intents[0].step_index = 0;
    air->intents[0].sync_class = AIR_SYNC_SYNC;
    air->intents[0].failure_class = AIR_FAILURE_RECOVERABLE;
    air->boundaries[0].kind = AIR_BOUNDARY_EXECUTION;
    air->boundaries[0].owner_name = "ScoreIntent";
    air->boundaries[0].source_name = "pin";
    air->boundaries[0].intent_index = 0;
    air->boundaries[0].step_index = 0;
    air->boundaries[0].sync_class = AIR_SYNC_SYNC;
    air->boundaries[0].ast = &pin_ast;

    memset(insts, 0, sizeof(insts));
    insts[0].kind = MIR_INST_CLEANUP_EDGE;
    insts[0].name = "cleanup-edge";
    insts[0].slot_anchor = "cleanup";
    insts[0].arg0 = "cleanup";
    insts[1].kind = MIR_INST_CLEANUP_EDGE;
    insts[1].name = "pin-unpin-cleanup-edge";
    insts[1].slot_anchor = "scores";
    insts[1].arg0 = "view";
    insts[1].arg1 = "read";
    insts[1].ast = &pin_ast;

    memset(blocks, 0, sizeof(blocks));
    blocks[0].is_reachable = true;
    blocks[0].is_pin_region = true;
    blocks[0].pin_source_name = "scores";
    blocks[0].pin_view_name = "view";
    blocks[0].pin_block_ast = &pin_ast;
    blocks[0].has_cleanup_succ = true;
    blocks[0].cleanup_succ = 1;
    blocks[0].instructions = insts;
    blocks[0].instruction_count = 2;
    blocks[1].id = 1;
    blocks[1].is_cleanup = true;
    blocks[1].is_reachable = true;

    memset(&routine, 0, sizeof(routine));
    routine.name = "pin_scores";
    routine.blocks = blocks;
    routine.block_count = 2;
    routine.has_cleanup_block = true;
    routine.cleanup_block = 1;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = air_collect_mir_evidence(air, &mir, &error)
        && air_validate(air, &error)
        && air->mir_cleanup_evidence_count == 1
        && air->mir_pin_cleanup_evidence_count == 1
        && air->evidence_count == 2
        && air->evidence_nodes[0].kind == AIR_EVIDENCE_MIR_CLEANUP
        && air->evidence_nodes[1].kind == AIR_EVIDENCE_MIR_PIN_CLEANUP
        && air->evidence_nodes[1].boundary_index == 0
        && strcmp(air->evidence_nodes[1].provider_name, "pin_scores") == 0
        && strcmp(air->evidence_nodes[1].subject_name, "scores") == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_requires_pin_cleanup_ast_provenance(void)
{
    ASTNode pin_ast;
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock blocks[2];
    MIRInstruction insts[2];
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;
    memset(&pin_ast, 0, sizeof(pin_ast));
    pin_ast.type = AST_BLOCK;
    pin_ast.data.block.is_pin_block = true;

    air->intents = (AIRIntentNode *)calloc(1, sizeof(AIRIntentNode));
    air->boundaries = (AIRBoundaryNode *)calloc(1, sizeof(AIRBoundaryNode));
    if (air->intents == NULL || air->boundaries == NULL) {
        air_destroy(air);
        return false;
    }
    air->intent_count = 1;
    air->boundary_count = 1;
    air->intents[0].intent_owner = "ScoreIntent";
    air->intents[0].step_name = "pin_scores";
    air->intents[0].step_index = 0;
    air->intents[0].sync_class = AIR_SYNC_SYNC;
    air->intents[0].failure_class = AIR_FAILURE_RECOVERABLE;
    air->boundaries[0].kind = AIR_BOUNDARY_EXECUTION;
    air->boundaries[0].owner_name = "ScoreIntent";
    air->boundaries[0].source_name = "pin";
    air->boundaries[0].intent_index = 0;
    air->boundaries[0].step_index = 0;
    air->boundaries[0].sync_class = AIR_SYNC_SYNC;

    memset(insts, 0, sizeof(insts));
    insts[0].kind = MIR_INST_CLEANUP_EDGE;
    insts[0].name = "cleanup-edge";
    insts[0].slot_anchor = "cleanup";
    insts[0].arg0 = "cleanup";
    insts[1].kind = MIR_INST_CLEANUP_EDGE;
    insts[1].name = "pin-unpin-cleanup-edge";
    insts[1].slot_anchor = "scores";
    insts[1].arg0 = "view";
    insts[1].arg1 = "read";
    insts[1].ast = &pin_ast;

    memset(blocks, 0, sizeof(blocks));
    blocks[0].is_reachable = true;
    blocks[0].is_pin_region = true;
    blocks[0].pin_source_name = "scores";
    blocks[0].pin_view_name = "view";
    blocks[0].pin_block_ast = &pin_ast;
    blocks[0].has_cleanup_succ = true;
    blocks[0].cleanup_succ = 1;
    blocks[0].instructions = insts;
    blocks[0].instruction_count = 2;
    blocks[1].id = 1;
    blocks[1].is_cleanup = true;
    blocks[1].is_reachable = true;

    memset(&routine, 0, sizeof(routine));
    routine.name = "pin_scores";
    routine.blocks = blocks;
    routine.block_count = 2;
    routine.has_cleanup_block = true;
    routine.cleanup_block = 1;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = air_collect_mir_evidence(air, &mir, &error)
        && air_validate(air, &error)
        && air->mir_cleanup_evidence_count == 1
        && air->mir_pin_cleanup_evidence_count == 0
        && air->evidence_count == 1;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_rejects_orphan_mir_pin_cleanup_evidence(void)
{
    ASTNode pin_ast;
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock block;
    MIRInstruction inst;
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;
    memset(&pin_ast, 0, sizeof(pin_ast));
    pin_ast.type = AST_BLOCK;
    pin_ast.data.block.is_pin_block = true;

    air->intents = (AIRIntentNode *)calloc(1, sizeof(AIRIntentNode));
    air->boundaries = (AIRBoundaryNode *)calloc(1, sizeof(AIRBoundaryNode));
    if (air->intents == NULL || air->boundaries == NULL) {
        air_destroy(air);
        return false;
    }
    air->intent_count = 1;
    air->boundary_count = 1;
    air->intents[0].intent_owner = "ScoreIntent";
    air->intents[0].step_name = "pin_scores";
    air->intents[0].step_index = 0;
    air->intents[0].sync_class = AIR_SYNC_SYNC;
    air->intents[0].failure_class = AIR_FAILURE_RECOVERABLE;
    air->boundaries[0].kind = AIR_BOUNDARY_EXECUTION;
    air->boundaries[0].owner_name = "ScoreIntent";
    air->boundaries[0].source_name = "pin";
    air->boundaries[0].intent_index = 0;
    air->boundaries[0].step_index = 0;
    air->boundaries[0].sync_class = AIR_SYNC_SYNC;
    air->boundaries[0].ast = &pin_ast;

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_CLEANUP_EDGE;
    inst.name = "pin-unpin-cleanup-edge";
    inst.slot_anchor = "scores";
    inst.ast = &pin_ast;

    memset(&block, 0, sizeof(block));
    block.is_reachable = true;
    block.is_pin_region = true;
    block.pin_source_name = "scores";
    block.pin_view_name = "view";
    block.pin_block_ast = &pin_ast;
    block.instructions = &inst;
    block.instruction_count = 1;

    memset(&routine, 0, sizeof(routine));
    routine.name = "pin_scores";
    routine.blocks = &block;
    routine.block_count = 1;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = air_collect_mir_evidence(air, &mir, &error)
        && air_validate(air, &error)
        && air->has_mir_input
        && air->mir_cleanup_evidence_count == 0
        && air->mir_pin_cleanup_evidence_count == 0
        && air->evidence_count == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_strict_rejects_pin_cleanup_without_cleanup_root(void)
{
    ASTNode pin_ast;
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock block;
    MIRInstruction inst;
    char *error = NULL;
    bool found = false;
    bool ok;

    if (air == NULL)
        return false;
    memset(&pin_ast, 0, sizeof(pin_ast));
    pin_ast.type = AST_BLOCK;
    pin_ast.data.block.is_pin_block = true;

    air->intents = (AIRIntentNode *)calloc(1, sizeof(AIRIntentNode));
    air->boundaries = (AIRBoundaryNode *)calloc(1, sizeof(AIRBoundaryNode));
    if (air->intents == NULL || air->boundaries == NULL) {
        air_destroy(air);
        return false;
    }
    air->intent_count = 1;
    air->boundary_count = 1;
    air->strict_evidence = true;
    air->intents[0].intent_owner = "ScoreIntent";
    air->intents[0].step_name = "pin_scores";
    air->intents[0].step_index = 0;
    air->intents[0].sync_class = AIR_SYNC_SYNC;
    air->intents[0].failure_class = AIR_FAILURE_RECOVERABLE;
    air->boundaries[0].kind = AIR_BOUNDARY_EXECUTION;
    air->boundaries[0].owner_name = "ScoreIntent";
    air->boundaries[0].source_name = "pin";
    air->boundaries[0].intent_index = 0;
    air->boundaries[0].step_index = 0;
    air->boundaries[0].sync_class = AIR_SYNC_SYNC;
    air->boundaries[0].ast = &pin_ast;

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_CLEANUP_EDGE;
    inst.name = "pin-unpin-cleanup-edge";
    inst.slot_anchor = "scores";
    inst.arg0 = "view";
    inst.arg1 = "read";
    inst.ast = &pin_ast;

    memset(&block, 0, sizeof(block));
    block.is_reachable = true;
    block.is_pin_region = true;
    block.pin_source_name = "scores";
    block.pin_view_name = "view";
    block.pin_block_ast = &pin_ast;
    block.instructions = &inst;
    block.instruction_count = 1;

    memset(&routine, 0, sizeof(routine));
    routine.name = "pin_scores";
    routine.blocks = &block;
    routine.block_count = 1;
    routine.has_cleanup_block = false;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = air_collect_mir_evidence(air, &mir, &error)
        && air->has_mir_input
        && air->mir_cleanup_evidence_count == 0
        && air->mir_pin_cleanup_evidence_count == 0
        && air_verify(air, &error);
    for (size_t i = 0; ok && i < air->drift_count; i++) {
        if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && strstr(air->drifts[i].message,
                      "AIR pin boundary has no matching MIR pin cleanup evidence") != NULL) {
            found = true;
            break;
        }
    }
    ok = ok && found;
    test_air_clear_stack_drifts(air);
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_rejects_unanchored_mir_pin_cleanup_evidence(void)
{
    ASTNode pin_ast;
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock blocks[2];
    MIRInstruction insts[2];
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;
    memset(&pin_ast, 0, sizeof(pin_ast));
    pin_ast.type = AST_BLOCK;
    pin_ast.data.block.is_pin_block = true;

    air->intents = (AIRIntentNode *)calloc(1, sizeof(AIRIntentNode));
    air->boundaries = (AIRBoundaryNode *)calloc(1, sizeof(AIRBoundaryNode));
    if (air->intents == NULL || air->boundaries == NULL) {
        air_destroy(air);
        return false;
    }
    air->intent_count = 1;
    air->boundary_count = 1;
    air->intents[0].intent_owner = "ScoreIntent";
    air->intents[0].step_name = "pin_scores";
    air->intents[0].step_index = 0;
    air->intents[0].sync_class = AIR_SYNC_SYNC;
    air->intents[0].failure_class = AIR_FAILURE_RECOVERABLE;
    air->boundaries[0].kind = AIR_BOUNDARY_EXECUTION;
    air->boundaries[0].owner_name = "ScoreIntent";
    air->boundaries[0].source_name = "pin";
    air->boundaries[0].intent_index = 0;
    air->boundaries[0].step_index = 0;
    air->boundaries[0].sync_class = AIR_SYNC_SYNC;
    air->boundaries[0].ast = &pin_ast;

    memset(insts, 0, sizeof(insts));
    insts[0].kind = MIR_INST_CLEANUP_EDGE;
    insts[0].name = "cleanup-edge";
    insts[0].slot_anchor = "cleanup";
    insts[0].arg0 = "cleanup";
    insts[1].kind = MIR_INST_CLEANUP_EDGE;
    insts[1].name = "pin-unpin-cleanup-edge";
    insts[1].ast = &pin_ast;

    memset(blocks, 0, sizeof(blocks));
    blocks[0].is_reachable = true;
    blocks[0].is_pin_region = true;
    blocks[0].pin_source_name = "scores";
    blocks[0].pin_view_name = "view";
    blocks[0].pin_block_ast = &pin_ast;
    blocks[0].has_cleanup_succ = true;
    blocks[0].cleanup_succ = 1;
    blocks[0].instructions = insts;
    blocks[0].instruction_count = 2;
    blocks[1].id = 1;
    blocks[1].is_cleanup = true;
    blocks[1].is_reachable = true;

    memset(&routine, 0, sizeof(routine));
    routine.name = "pin_scores";
    routine.blocks = blocks;
    routine.block_count = 2;
    routine.has_cleanup_block = true;
    routine.cleanup_block = 1;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = air_collect_mir_evidence(air, &mir, &error)
        && air_validate(air, &error)
        && air->has_mir_input
        && air->mir_cleanup_evidence_count == 1
        && air->mir_pin_cleanup_evidence_count == 0;
    free(error);
    air_destroy(air);
    return ok;
}
