#ifndef PERGYRA_MIR_DCE_H
#define PERGYRA_MIR_DCE_H

static bool
mir_free_instruction_payload(MIRInstruction *inst)
{
    if (inst == NULL)
        return true;
    free((void *)inst->result_name);
    inst->result_name = NULL;
    for (size_t i = 0; i < inst->use_count; i++)
        free((void *)inst->uses[i]);
    free((void *)inst->uses);
    inst->uses = NULL;
    inst->use_count = 0;
    if (inst->phi_incomings != NULL) {
        for (size_t i = 0; i < inst->phi_incoming_count; i++)
            free((void *)inst->phi_incomings[i].value_name);
    }
    free(inst->phi_incomings);
    inst->phi_incomings = NULL;
    inst->phi_incoming_count = 0;
    return true;
}

static void
mir_reset_routine_analysis(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    routine->live_value_count = 0;
    routine->has_liveness = false;
    routine->has_use_def_summary = false;
    for (size_t i = 0; i < routine->value_summary_count; i++)
        free((void *)routine->value_summaries[i].name);
    free(routine->value_summaries);
    routine->value_summaries = NULL;
    routine->value_summary_count = 0;

    for (size_t i = 0; i < routine->block_count; i++) {
        MIRBasicBlock *block = &routine->blocks[i];
        mir_clear_block_name_set(&block->def_names, &block->def_name_count);
        mir_clear_block_name_set(&block->use_names, &block->use_name_count);
        mir_clear_block_name_set(&block->live_in_names, &block->live_in_name_count);
        mir_clear_block_name_set(&block->live_out_names, &block->live_out_name_count);
    }
}

static bool
mir_recompute_analysis(MIRRoutine *routine)
{
    mir_reset_routine_analysis(routine);
    return mir_compute_liveness(routine);
}

static bool
mir_remove_instruction(MIRBasicBlock *block, size_t index)
{
    if (block == NULL || index >= block->instruction_count)
        return false;
    mir_free_instruction_payload(&block->instructions[index]);
    if (index + 1 < block->instruction_count) {
        memmove(&block->instructions[index],
                &block->instructions[index + 1],
                (block->instruction_count - index - 1) * sizeof(MIRInstruction));
    }
    block->instruction_count--;
    if (block->instruction_count == 0) {
        free(block->instructions);
        block->instructions = NULL;
    } else {
        MIRInstruction *shrunk = realloc(block->instructions,
                                         block->instruction_count * sizeof(MIRInstruction));
        if (shrunk != NULL)
            block->instructions = shrunk;
    }
    return true;
}

static bool
mir_instruction_is_dead_value(const MIRRoutine *routine, const MIRInstruction *inst)
{
    int idx;
    const MIRValueSummary *summary;

    if (routine == NULL || inst == NULL || inst->result_name == NULL)
        return false;
    if (inst->kind != MIR_INST_DEF && inst->kind != MIR_INST_PHI)
        return false;
    idx = mir_find_value_summary(routine, inst->result_name);
    if (idx < 0)
        return false;
    summary = &routine->value_summaries[idx];
    /* AST-backed DEFs (let / assignment) remain conservatively preserved.
     * Value-summary provenance is now richer, but loop-carried seed values
     * still are not distinguished well enough to reopen dead local removal
     * without changing runtime behavior. */
    if (inst->ast != NULL)
        return false;
    return summary->use_count == 0
           && summary->live_in_block_count == 0
           && summary->live_out_block_count == 0
           && !summary->reaches_cleanup;
}

static bool
mir_stmt_is_semantic_carrier(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_STMT || inst->name == NULL)
        return false;

    if (strncmp(inst->name, "Intent", 6) == 0)
        return true;

    return false;
}

static bool
mir_call_is_whitelisted_pure_query(const char *callee)
{
    if (callee == NULL)
        return false;
    return strcmp(callee, "HasState") == 0
        || strcmp(callee, "HasLayer") == 0
        || strcmp(callee, "HasProjection") == 0
        || strcmp(callee, "HasZone") == 0
        || strcmp(callee, "HasZoneProjection") == 0
        || strcmp(callee, "HasZoneLayer") == 0
        || strcmp(callee, "HasZoneState") == 0
        || strcmp(callee, "ChannelLength") == 0
        || strcmp(callee, "ChannelCapacity") == 0
        || strcmp(callee, "ChannelSpace") == 0
        || strcmp(callee, "ChannelFull") == 0
        || strcmp(callee, "ChannelClosed") == 0;
}

static bool
mir_stmt_has_side_effect(const ASTNode *stmt)
{
    if (stmt == NULL)
        return false;
    if (stmt->type == AST_IF_STMT
        || stmt->type == AST_FOR_LOOP
        || stmt->type == AST_WHILE_LOOP
        || stmt->type == AST_MATCH_STMT
        || stmt->type == AST_DEFER_STMT
        || stmt->type == AST_ASYNC_BLOCK
        || stmt->type == AST_PARALLEL_BLOCK
        || stmt->type == AST_SELECT_STMT
        || stmt->type == AST_SPAWN_EXPR
        || stmt->type == AST_AWAIT_EXPR
        || stmt->type == AST_CHANNEL_SEND
        || stmt->type == AST_CHANNEL_RECV
        || stmt->type == AST_EVENT_SUBSCRIBE
        || stmt->type == AST_EVENT_UNSUBSCRIBE
        || stmt->type == AST_EVENT_INVOKE)
        return true;
    if (stmt->type == AST_ASSIGNMENT)
        return true;
    if (stmt->type == AST_LET_DECL)
        /* A let that reached MIR as a STMT (rather than being merged into
         * a DEF) still defines a binding whose downstream uses cannot be
         * discovered from MIR_INST_STMT alone. Keep it so the transpiler
         * emits the declaration + initializer side effects. */
        return true;
    if (stmt->type == AST_LET_DESTRUCTURE)
        /* Defines new bindings whose downstream uses DCE cannot see via
         * MIR_INST_STMT alone (the pre-declared SSA locals live in header).
         * Conservatively always keep destructuring statements. */
        return true;
    if (stmt->type == AST_BIND_STMT)
        return true;
    if (stmt->type == AST_UNSAFE_BLOCK)
        return true;
    if (stmt->type == AST_INTENT_STEP)
        return true;
    if (stmt->type == AST_WITH_STMT)
        return true;
    if (stmt->type == AST_CALL
        && stmt->data.call.callee != NULL
        && stmt->data.call.callee->type == AST_IDENTIFIER
        && stmt->data.call.callee->data.identifier.name != NULL) {
        const char *callee = stmt->data.call.callee->data.identifier.name;
        if (mir_call_is_whitelisted_pure_query(callee))
            return false;
        return true;
    }
    if (stmt->type == AST_CALL)
        return true;
    return false;
}

static bool
mir_instruction_is_dead_stmt(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_STMT)
        return false;
    if (mir_stmt_is_semantic_carrier(inst))
        return false;
    if (inst->ast == NULL)
        return true;
    return !mir_stmt_has_side_effect(inst->ast);
}

static bool
mir_run_dce_on_routine(MIRRoutine *routine, bool *changed_out)
{
    bool changed = false;

    if (changed_out != NULL)
        *changed_out = false;
    if (routine == NULL)
        return false;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t inst_id = block->instruction_count; inst_id-- > 0;) {
            MIRInstruction *inst = &block->instructions[inst_id];
            if (mir_instruction_is_dead_value(routine, inst)
                || mir_instruction_is_dead_stmt(inst)) {
                if (!mir_remove_instruction(block, inst_id))
                    return false;
                routine->dce_removed_count++;
                changed = true;
            }
        }
    }

    if (changed_out != NULL)
        *changed_out = changed;
    return true;
}

#endif
