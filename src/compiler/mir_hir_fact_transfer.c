#include "mir_hir_fact_transfer.h"

#include "mir.h"
#include "mir_call_fact.h"
#include "hir_collection_owned_local_transition_adapter.h"

#include "../common/string_compat.h"
#include "../semantic/semantic.h"
#include "../parser/ast_api.h"

#include <stdlib.h>

static bool mir_collection_evidence_error(char **error)
{
    if (error != NULL && *error == NULL)
        *error = pergyra_strdup("MIR collection effect evidence is missing, stale, or inconsistent");
    return false;
}

bool mir_capture_collection_effect_evidence(MIRProgram *mir,
    const SemanticResult *semantic, char **error)
{
    if (mir == NULL || semantic == NULL || !semantic->success ||
        mir->collection_effect_snapshot != NULL || mir->collection_effect_source_program != NULL ||
        !semantic_collection_effect_snapshot_current(semantic->collection_effect_snapshot,
            semantic->annotated_ast, NULL) ||
        !semantic_collection_effect_snapshot_retain(semantic->collection_effect_snapshot))
        return mir_collection_evidence_error(error);
    mir->collection_effect_snapshot = semantic->collection_effect_snapshot;
    mir->collection_effect_source_program = semantic->annotated_ast;
    return true;
}

void mir_free_collection_effect_evidence(MIRProgram *mir)
{
    if (mir == NULL) return;
    semantic_collection_effect_snapshot_destroy(mir->collection_effect_snapshot);
    mir->collection_effect_snapshot = NULL;
    mir->collection_effect_source_program = NULL;
}

static bool mir_collection_instruction_current(const MIRProgram *mir, const MIRRoutine *routine,
    const MIRBasicBlock *block, const MIRInstruction *inst,
    const PgyCollectionEffectView *view, size_t caller, bool *seen, bool *write_seen)
{
    if (view->source_use_count != 0 || view->nonowner_risk_count != 0 || view->string_definition_count != 0) {
        const ASTNode *place = NULL, *value = NULL;
        bool initialization = inst->kind == MIR_INST_DEF && inst->ast != NULL && inst->ast->type == AST_LET_DECL;
        if (initialization) { place = inst->ast; value = inst->expr0; }
        else if (inst->kind == MIR_INST_ASSIGN) { place = inst->expr0; value = inst->expr1; }
        else if (inst->kind == MIR_INST_DEF && inst->ast != NULL && inst->ast->type == AST_ASSIGNMENT) {
            place = inst->expr1; value = inst->expr0;
        }
        bool definition = initialization || inst->kind == MIR_INST_ASSIGN ||
            (inst->kind == MIR_INST_DEF && inst->ast != NULL && inst->ast->type == AST_ASSIGNMENT);
        size_t write_ordinal = SIZE_MAX;
        if (definition &&
            !semantic_collection_effect_snapshot_source_definition(
                mir->collection_effect_snapshot, routine->source_syntax_id,
                initialization ? ast_node_stable_id(place) : place != NULL && place->type == AST_IDENTIFIER
                    ? ast_identifier_binding_syntax_id(place) : inst->binding_syntax_id,
                initialization, inst->ast, place, value, &write_ordinal)) return false;
        if (write_ordinal != SIZE_MAX) {
            if (write_ordinal >= view->string_write_count || write_seen == NULL || write_seen[write_ordinal]) return false;
            write_seen[write_ordinal] = true;
        }
        if (inst->kind == MIR_INST_RETURN) {
            const ASTNode *statement = NULL;
            if (block->source_hir_block_id != SIZE_MAX) {
                if (block->source_hir_block_id >= routine->hir_routine->cfg.block_count ||
                    routine->hir_routine->cfg.blocks == NULL) return false;
                const HIRBasicBlock *source =
                    &routine->hir_routine->cfg.blocks[block->source_hir_block_id];
                if (source->id != block->source_hir_block_id) return false;
                if (source->terminator_kind == HIR_BLOCK_RETURN) {
                    if (source->terminator_value != inst->expr0 || inst->ast != inst->expr0 ||
                        source->statement_count != block->source_statement_inventory.count ||
                        (source->statement_count != 0 &&
                            (source->statements == NULL || block->source_statement_inventory.items == NULL)))
                        return false;
                    if (source->statement_count != 0) {
                        size_t last = source->statement_count - 1;
                        const ASTNode *candidate = source->statements[last];
                        if (candidate != block->source_statement_inventory.items[last]) return false;
                        if (candidate != NULL && candidate->type == AST_RETURN) {
                            if (ast_return_value(candidate) != inst->expr0) return false;
                            statement = candidate;
                        }
                    }
                }
            }
            if (!semantic_collection_effect_snapshot_source_return(
                    mir->collection_effect_snapshot, routine->source_syntax_id,
                    statement, inst->expr0)) return false;
        }
        ASTNode *roots[2];
        if (!mir_instruction_executable_roots(routine, inst, roots, NULL, NULL)) return false;
        for (size_t r = 0; r < 2; r++) {
            if (roots[r] == NULL) continue;
            /* ASSIGN exposes its target as a root too, but its exact target
             * role was admitted above; it is not a standalone evaluation. */
            bool target_role = definition && roots[r] == place;
            if (!target_role && (inst->kind != MIR_INST_RETURN || roots[r] != inst->expr0) &&
                !semantic_collection_effect_snapshot_source_evaluation(
                    mir->collection_effect_snapshot, routine->source_syntax_id, roots[r])) return false;
            if (!hir_collection_source_expression_current(
                    mir->collection_effect_snapshot, routine->hir_routine, roots[r])) return false;
        }
    }
    const ASTNode *call = inst->expr0;
    size_t expected = SIZE_MAX;
    if (call != NULL && call->type == AST_CALL) {
        if (caller != SIZE_MAX && !semantic_collection_effect_snapshot_call(
                mir->collection_effect_snapshot, caller, call, &expected)) return false;
        /* A registered call cannot migrate into a routine without caller claims. */
        for (size_t e = 0; e < view->effect_count; e++)
            if ((view->effects[e].call == call || view->effects[e].id == ast_node_stable_id(call)) && (expected != e ||
                view->effects[e].caller != caller)) return false;
    }
    if (expected == SIZE_MAX) return true;
    const PgyCollectionExpectedEffect *e = &view->effects[expected];
    if (seen[expected] || routine->source_syntax_id != view->callers[caller].decl_id ||
        inst->kind != MIR_INST_STMT || inst->ast != call || inst->source_stable_id != e->id ||
        !inst->has_collection_ownership_receipt ||
        inst->collection_ownership_effect_kind != e->kind ||
        inst->collection_ownership_receiver_binding_id != e->receiver ||
        inst->collection_ownership_source_binding_id != e->source) return false;
    seen[expected] = true;
    return true;
}

/* ABI-specialized stack views retain the canonical body's borrowed carriers.
 * Display names and substituted signature text are not collection identity. */
static bool mir_collection_body_borrow_current(const MIRRoutine *view, const MIRRoutine *source)
{
    return source != NULL && view->program == source->program && view->id == source->id &&
        view->ast == source->ast && view->hir_routine == source->hir_routine &&
        view->source_syntax_id == source->source_syntax_id && view->rir_scope == source->rir_scope &&
        view->kind == source->kind && view->owner_ast_type == source->owner_ast_type &&
        view->blocks == source->blocks && view->block_count == source->block_count &&
        view->block_capacity == source->block_capacity && view->entry_block == source->entry_block &&
        view->cleanup_block == source->cleanup_block && view->has_cleanup_block == source->has_cleanup_block &&
        view->rollback_block == source->rollback_block && view->has_rollback_block == source->has_rollback_block &&
        view->invalidation_block == source->invalidation_block &&
        view->has_invalidation_block == source->has_invalidation_block &&
        view->instruction_count == source->instruction_count &&
        view->cleanup_instruction_count == source->cleanup_instruction_count &&
        view->collection_ownership_facts == source->collection_ownership_facts &&
        view->collection_ownership_fact_count == source->collection_ownership_fact_count &&
        view->collection_ownership_fact_capacity == source->collection_ownership_fact_capacity &&
        view->source_local_types == source->source_local_types &&
        view->source_local_type_count == source->source_local_type_count &&
        view->source_local_type_capacity == source->source_local_type_capacity &&
        view->resource_flow_symbols == source->resource_flow_symbols &&
        view->resource_flow_symbol_count == source->resource_flow_symbol_count &&
        view->function_param_flow_summaries == source->function_param_flow_summaries &&
        view->function_param_flow_summary_count == source->function_param_flow_summary_count &&
        view->loop_flow_summaries == source->loop_flow_summaries &&
        view->loop_flow_summary_count == source->loop_flow_summary_count &&
        view->loop_flow_states == source->loop_flow_states && view->loop_flow_state_count == source->loop_flow_state_count &&
        view->iteration_type_facts == source->iteration_type_facts &&
        view->iteration_type_fact_count == source->iteration_type_fact_count &&
        view->destructure_type_facts == source->destructure_type_facts &&
        view->destructure_type_fact_count == source->destructure_type_fact_count &&
        view->match_binding_type_facts == source->match_binding_type_facts &&
        view->match_binding_type_fact_count == source->match_binding_type_fact_count &&
        view->ssa_bindings == source->ssa_bindings && view->ssa_binding_count == source->ssa_binding_count &&
        view->value_summaries == source->value_summaries && view->value_summary_count == source->value_summary_count &&
        view->params == source->params && view->param_count == source->param_count &&
        view->return_type == source->return_type;
}

bool mir_validate_collection_effect_evidence(const MIRProgram *mir,
    const MIRRoutine *only, char **error)
{
    if (mir == NULL || (mir->routine_count != 0 && mir->routines == NULL))
        return mir_collection_evidence_error(error);
    MIRRoutineInventory inventory;
    mir_routine_inventory_from_program(mir, &inventory);
    const MIRRoutine *canonical = only == NULL ? NULL : mir_routine_inventory_get(&inventory, only->id);
    if (only != NULL && !mir_collection_body_borrow_current(only, canonical))
        return mir_collection_evidence_error(error);
    for (size_t r = 0; r < inventory.count; r++) {
        const MIRRoutine *routine = mir_routine_inventory_get(&inventory, r);
        if (routine == NULL || (routine->block_count != 0 && routine->blocks == NULL))
            return mir_collection_evidence_error(error);
        if (mir->collection_effect_snapshot != NULL && routine->id != r)
            return mir_collection_evidence_error(error);
        for (size_t b = 0; b < routine->block_count; b++)
            if (routine->blocks[b].instruction_count != 0 && routine->blocks[b].instructions == NULL)
                return mir_collection_evidence_error(error);
    }
    if (mir->collection_effect_snapshot == NULL)
        return mir_collection_evidence_error(error);
    PgyCollectionEffectView view = {0};
    bool current = only == NULL ? semantic_collection_effect_snapshot_current(
        mir->collection_effect_snapshot, mir->collection_effect_source_program, &view) :
        semantic_collection_effect_snapshot_routine_current(mir->collection_effect_snapshot,
            mir->collection_effect_source_program, only->source_syntax_id, &view);
    if (!current) return mir_collection_evidence_error(error);
    bool *seen = view.effect_count == 0 ? NULL : calloc(view.effect_count, sizeof(*seen));
    if (view.effect_count != 0 && seen == NULL) return mir_collection_evidence_error(error);
    if (view.string_write_count > SIZE_MAX / sizeof(bool)) { free(seen); return mir_collection_evidence_error(error); }
    bool *write_seen = view.string_write_count == 0 ? NULL : calloc(view.string_write_count, sizeof(*write_seen));
    if (view.string_write_count != 0 && write_seen == NULL) { free(seen); return mir_collection_evidence_error(error); }
    bool valid = true;
    for (size_t c = 0; c < view.caller_count && valid; c++) {
        if (only != NULL && view.callers[c].decl_id != only->source_syntax_id) continue;
        size_t matches = 0;
        for (size_t r = 0; r < inventory.count; r++) {
            const MIRRoutine *routine = mir_routine_inventory_get(&inventory, r);
            if (routine->source_syntax_id != view.callers[c].decl_id) continue;
            matches++;
            if (routine->program != mir || routine->ast != view.callers[c].decl ||
                routine->hir_routine == NULL || routine->hir_routine->ast != routine->ast ||
                routine->hir_routine->source_syntax_id != routine->source_syntax_id ||
                routine->hir_routine->body != view.callers[c].body) valid = false;
        }
        if (matches != 1) valid = false;
    }
    for (size_t r = 0; r < inventory.count && valid; r++) {
        const MIRRoutine *routine = mir_routine_inventory_get(&inventory, r);
        if (only != NULL && routine != canonical) continue;
        if (only != NULL) routine = only;
        if (routine->program != mir || routine->ast == NULL || routine->source_syntax_id == 0 ||
            ast_node_stable_id(routine->ast) != routine->source_syntax_id || routine->hir_routine == NULL ||
            routine->hir_routine->ast != routine->ast ||
            routine->hir_routine->source_syntax_id != routine->source_syntax_id ||
            routine->hir_routine->body != (routine->ast->type == AST_FUNC_DECL ?
                ast_func_body(routine->ast) : NULL)) { valid = false; break; }
        size_t caller = SIZE_MAX;
        for (size_t c = 0; c < view.caller_count; c++)
            if (view.callers[c].decl_id == routine->source_syntax_id) caller = c;
        for (size_t b = 0; b < routine->block_count && valid; b++)
            for (size_t i = 0; i < routine->blocks[b].instruction_count; i++)
                if (!mir_collection_instruction_current(mir, routine, &routine->blocks[b],
                    &routine->blocks[b].instructions[i], &view, caller, seen, write_seen)) { valid = false; break; }
    }
    for (size_t e = 0; e < view.effect_count && valid; e++)
        if ((only == NULL || view.callers[view.effects[e].caller].decl_id == only->source_syntax_id) && !seen[e])
            valid = false;
    free(seen);
    free(write_seen);
    return valid || mir_collection_evidence_error(error);
}

bool
mir_copy_function_param_flow_summaries(MIRRoutine *routine,
                                       const HIRRoutine *hir_routine,
                                       char **error_message)
{
    size_t count;

    if (routine == NULL || hir_routine == NULL)
        return false;
    count = hir_routine->function_param_flow_summary_count;
    if (count == 0)
        return true;
    if (routine->ast == NULL || routine->ast->type != AST_FUNC_DECL
        || routine->source_syntax_id == 0
        || count > routine->param_count) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR function parameter flow summary has invalid routine identity or parameter count");
        return false;
    }

    routine->function_param_flow_summaries = calloc(
        count, sizeof(*routine->function_param_flow_summaries));
    if (routine->function_param_flow_summaries == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return false;
    }
    routine->function_param_flow_summary_capacity = count;
    for (size_t i = 0; i < count; i++) {
        const HIRFunctionParamFlowSummary *summary =
            &hir_routine->function_param_flow_summaries[i];
        if (summary->parameter_index >= routine->param_count) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR function parameter flow summary has out-of-range parameter index");
            free(routine->function_param_flow_summaries);
            routine->function_param_flow_summaries = NULL;
            routine->function_param_flow_summary_capacity = 0;
            return false;
        }
        for (size_t j = 0; j < routine->function_param_flow_summary_count; j++) {
            if (routine->function_param_flow_summaries[j].parameter_index
                == summary->parameter_index) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "MIR function parameter flow summaries share parameter identity");
                free(routine->function_param_flow_summaries);
                routine->function_param_flow_summaries = NULL;
                routine->function_param_flow_summary_capacity = 0;
                routine->function_param_flow_summary_count = 0;
                return false;
            }
        }
        routine->function_param_flow_summaries[
            routine->function_param_flow_summary_count].parameter_index =
            summary->parameter_index;
        routine->function_param_flow_summaries[
            routine->function_param_flow_summary_count].mask = summary->mask;
        routine->function_param_flow_summary_count++;
    }
    return true;
}

bool
mir_copy_resource_flow_symbols(MIRRoutine *routine,
                               const HIRRoutine *hir_routine,
                               char **error_message)
{
    size_t count;

    if (routine == NULL || hir_routine == NULL)
        return false;
    count = hir_routine->resource_flow_symbol_count;
    if (count == 0)
        return true;
    if (hir_routine->resource_flow_symbols == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR ResourceFlowUniverse has incomplete HIR storage");
        return false;
    }
    routine->resource_flow_symbols = calloc(
        count, sizeof(*routine->resource_flow_symbols));
    if (routine->resource_flow_symbols == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return false;
    }
    routine->resource_flow_symbol_capacity = count;
    for (size_t i = 0; i < count; i++) {
        const HIRResourceFlowSymbol *source =
            &hir_routine->resource_flow_symbols[i];
        MIRResourceFlowSymbol *target = &routine->resource_flow_symbols[i];
        if (source->name == NULL || source->name[0] == '\0') {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR ResourceFlowUniverse row has no name");
            goto fail;
        }
        for (size_t j = 0; j < i; j++) {
            const MIRResourceFlowSymbol *prior =
                &routine->resource_flow_symbols[j];
            if (prior->stable_index == source->stable_index
                || (source->is_parameter && prior->is_parameter
                    && prior->parameter_index == source->parameter_index)) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "MIR ResourceFlowUniverse rows have duplicate identity");
                goto fail;
            }
        }
        *target = (MIRResourceFlowSymbol){
            .stable_index = source->stable_index,
            .declaration_syntax_id = source->declaration_syntax_id,
            .line = source->line,
            .column = source->column,
            .symbol_kind = source->symbol_kind,
            .is_parameter = source->is_parameter,
            .parameter_index = source->parameter_index,
            .name = pergyra_strdup(source->name)
        };
        if (target->name == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            goto fail;
        }
    }
    routine->resource_flow_symbol_count = count;
    return true;

fail:
    for (size_t i = 0; i < count; i++)
        free(routine->resource_flow_symbols[i].name);
    free(routine->resource_flow_symbols);
    routine->resource_flow_symbols = NULL;
    routine->resource_flow_symbol_count = 0;
    routine->resource_flow_symbol_capacity = 0;
    return false;
}

void
mir_free_resource_flow_symbols(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    for (size_t i = 0; i < routine->resource_flow_symbol_count; i++)
        free(routine->resource_flow_symbols[i].name);
    free(routine->resource_flow_symbols);
    routine->resource_flow_symbols = NULL;
    routine->resource_flow_symbol_count = 0;
    routine->resource_flow_symbol_capacity = 0;
}

static bool
mir_loop_flow_resource_index_known(const MIRRoutine *routine,
                                   size_t stable_index)
{
    if (routine == NULL)
        return false;
    for (size_t i = 0; i < routine->resource_flow_symbol_count; i++) {
        if (routine->resource_flow_symbols[i].stable_index == stable_index)
            return true;
    }
    return false;
}

bool
mir_copy_loop_flow_facts(MIRRoutine *routine,
                         const HIRRoutine *hir_routine,
                         char **error_message)
{
    size_t summary_count;
    size_t state_count;

    if (routine == NULL || hir_routine == NULL)
        return false;
    summary_count = hir_routine->loop_flow_summary_count;
    state_count = hir_routine->loop_flow_state_count;
    if (summary_count == 0) {
        if (state_count != 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR LoopFlowSummary states exist without summaries");
            return false;
        }
        return true;
    }
    if (routine->source_syntax_id == 0
        || hir_routine->source_syntax_id != routine->source_syntax_id
        || hir_routine->loop_flow_summaries == NULL
        || (state_count > 0 && hir_routine->loop_flow_states == NULL)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR LoopFlowSummary has incomplete routine identity or storage");
        return false;
    }

    routine->loop_flow_states = state_count > 0
        ? calloc(state_count, sizeof(*routine->loop_flow_states))
        : NULL;
    routine->loop_flow_summaries = calloc(
        summary_count, sizeof(*routine->loop_flow_summaries));
    if ((state_count > 0 && routine->loop_flow_states == NULL)
        || routine->loop_flow_summaries == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        free(routine->loop_flow_states);
        free(routine->loop_flow_summaries);
        routine->loop_flow_states = NULL;
        routine->loop_flow_summaries = NULL;
        return false;
    }
    routine->loop_flow_state_capacity = state_count;
    routine->loop_flow_summary_capacity = summary_count;
    for (size_t i = 0; i < state_count; i++) {
        const PgyLoopFlowStateFact *state = &hir_routine->loop_flow_states[i];
        if (!mir_loop_flow_resource_index_known(routine, state->stable_index)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR LoopFlowSummary state references an unknown ResourceFlow index");
            goto fail;
        }
        routine->loop_flow_states[i] = *state;
    }
    for (size_t i = 0; i < summary_count; i++) {
        const PgyLoopFlowSummaryFact *summary =
            &hir_routine->loop_flow_summaries[i];
        if (summary->function_syntax_id != routine->source_syntax_id
            || summary->loop_syntax_id == 0
            || summary->kind > 1u
            || summary->entry_state_start > state_count
            || summary->entry_state_count
                > state_count - summary->entry_state_start
            || summary->exit_state_start > state_count
            || summary->exit_state_count
                > state_count - summary->exit_state_start) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR LoopFlowSummary has an invalid identity or state range");
            goto fail;
        }
        routine->loop_flow_summaries[i] = *summary;
    }
    routine->loop_flow_state_count = state_count;
    routine->loop_flow_summary_count = summary_count;
    return true;

fail:
    free(routine->loop_flow_states);
    free(routine->loop_flow_summaries);
    routine->loop_flow_states = NULL;
    routine->loop_flow_summaries = NULL;
    routine->loop_flow_state_capacity = 0;
    routine->loop_flow_summary_capacity = 0;
    routine->loop_flow_state_count = 0;
    routine->loop_flow_summary_count = 0;
    return false;
}

bool
mir_copy_iteration_type_facts(MIRRoutine *routine,
                              const HIRRoutine *hir_routine,
                              char **error_message)
{
    size_t count;

    if (routine == NULL || hir_routine == NULL)
        return false;
    count = hir_routine->iteration_type_fact_count;
    if (count == 0)
        return true;
    if (routine->source_syntax_id == 0
        || hir_routine->source_syntax_id != routine->source_syntax_id
        || hir_routine->iteration_type_facts == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR iteration type facts have incomplete routine identity or storage");
        return false;
    }
    routine->iteration_type_facts = calloc(
        count, sizeof(*routine->iteration_type_facts));
    if (routine->iteration_type_facts == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return false;
    }
    routine->iteration_type_fact_capacity = count;
    for (size_t i = 0; i < count; i++) {
        const HIRIterationTypeFact *source =
            &hir_routine->iteration_type_facts[i];
        MIRIterationTypeFact *target = &routine->iteration_type_facts[i];
        if (source->function_syntax_id != routine->source_syntax_id
            || source->iteration_syntax_id == 0
            || source->binding_type_name == NULL
            || source->iterable_type_name == NULL
            || source->binding_type_name[0] == '\0'
            || source->iterable_type_name[0] == '\0'
            || mir_routine_iteration_type_fact(routine,
                                               source->iteration_syntax_id)
                != NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR iteration type facts have invalid or duplicate identity");
            goto fail;
        }
        target->function_syntax_id = source->function_syntax_id;
        target->iteration_syntax_id = source->iteration_syntax_id;
        target->binding_type_name = pergyra_strdup(source->binding_type_name);
        target->iterable_type_name = pergyra_strdup(source->iterable_type_name);
        target->collection_hoisted = source->collection_hoisted;
        if (target->binding_type_name == NULL
            || target->iterable_type_name == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            goto fail;
        }
        routine->iteration_type_fact_count++;
    }
    return true;

fail:
    for (size_t i = 0; i < count; i++) {
        free(routine->iteration_type_facts[i].binding_type_name);
        free(routine->iteration_type_facts[i].iterable_type_name);
    }
    free(routine->iteration_type_facts);
    routine->iteration_type_facts = NULL;
    routine->iteration_type_fact_count = 0;
    routine->iteration_type_fact_capacity = 0;
    return false;
}

void
mir_free_iteration_type_facts(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    for (size_t i = 0; i < routine->iteration_type_fact_count; i++) {
        free(routine->iteration_type_facts[i].binding_type_name);
        free(routine->iteration_type_facts[i].iterable_type_name);
    }
    free(routine->iteration_type_facts);
    routine->iteration_type_facts = NULL;
    routine->iteration_type_fact_count = 0;
    routine->iteration_type_fact_capacity = 0;
}
