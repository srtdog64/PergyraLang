#include "mir.h"

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../common/arena.h"
#include "../runtime/pgy_abi_spec.h"
#include "../parser/ast_api.h"
#include "mir_branch_source_facts.h"
#include "mir_lower_population.h"
#include "mir_parallel_capture_facts.h"
#include "mir_public_surface.h"
#include "mir_signature_metadata.h"
#include "mir_source_inventory_build.h"
#include "mir_source_local_types.h"
#include "mir_speculation_facts.h"

#include "mir_base_helpers.h"
#include "mir_cleanup.h"
#include "mir_intent.h"
#include "mir_machine_layer.h"
#include "mir_surface_usage.h"
#include "mir_stmt_population.h"
#include "mir_timing.h"
#include "mir_validation.h"

static bool
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

static bool
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

static void
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

static bool
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

static bool
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
        MIRIterationTypeFact *target =
            &routine->iteration_type_facts[i];
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

static void
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

static bool
mir_add_phi_placeholders(MIRRoutine *routine, MIRBasicBlock *block)
{
    if (routine == NULL || block == NULL)
        return false;

    for (size_t i = 0; i < block->source_phi_node_count; i++) {
        MIRInstruction inst;
        memset(&inst, 0, sizeof(inst));
        inst.kind = MIR_INST_PHI;
        inst.name = block->source_phi_nodes[i].name;
        inst.slot_anchor = block->source_phi_nodes[i].name;
        inst.arg0 = "phi";
        if (!mir_commit_instruction(routine, block, &inst))
            return false;
    }
    return true;
}

static bool
mir_add_terminator_instruction(MIRRoutine *routine,
                               MIRBasicBlock *block,
                               HIRBlockTerminatorKind terminator_kind,
                               ASTNode *terminator_condition,
                               ASTNode *terminator_value)
{
    MIRInstruction inst;
    if (routine == NULL || block == NULL)
        return false;
    if (terminator_kind != HIR_BLOCK_BRANCH
        && terminator_kind != HIR_BLOCK_RETURN)
        return true;
    memset(&inst, 0, sizeof(inst));
    inst.kind = (terminator_kind == HIR_BLOCK_BRANCH)
                    ? MIR_INST_BRANCH
                    : MIR_INST_RETURN;
    inst.name = (terminator_kind == HIR_BLOCK_BRANCH) ? "branch" : "return";
    inst.source_terminator_kind = terminator_kind;
    inst.has_source_terminator_kind = true;
    inst.source_terminator_has_value = terminator_value != NULL;
    inst.ast = (terminator_kind == HIR_BLOCK_BRANCH)
                   ? terminator_condition
                   : terminator_value;
    mir_instruction_capture_source_provenance(&inst, inst.ast);
    if (inst.kind == MIR_INST_BRANCH) {
        inst.branch_shape = mir_branch_shape_from_ast(inst.ast);
        inst.requires_source_branch_emit =
            inst.branch_shape == MIR_BRANCH_MATCH_CASE
            || inst.branch_shape == MIR_BRANCH_SELECT_DISPATCH;
    }
    if (inst.kind == MIR_INST_BRANCH
        && inst.branch_shape == MIR_BRANCH_EXPR)
        inst.expr0 = terminator_condition;
    else if (inst.kind == MIR_INST_BRANCH
        && inst.branch_shape == MIR_BRANCH_MATCH_CASE)
        mir_capture_match_case_facts(&inst, terminator_condition,
                                     terminator_value);
    else if (inst.kind == MIR_INST_BRANCH
        && inst.branch_shape == MIR_BRANCH_SELECT_DISPATCH)
        inst.expr0 = mir_select_case_channel(terminator_condition);
    else if (inst.kind == MIR_INST_RETURN)
        inst.expr0 = terminator_value;
    if (inst.kind == MIR_INST_BRANCH
        && inst.ast != NULL
        && inst.ast->type == AST_FOR_LOOP) {
        inst.arg0 = ast_for_variable(inst.ast);
        if (ast_for_iterable(inst.ast) != NULL) {
            inst.expr0 = ast_for_iterable(inst.ast);
            inst.expr1 = ast_for_iterable(inst.ast);
        } else {
            inst.expr0 = ast_for_range_start(inst.ast);
            inst.expr1 = ast_for_range_end(inst.ast);
        }
    }
    return mir_commit_instruction(routine, block, &inst);
}

#include "mir_ssa_rename.h"

#include "mir_liveness_dce.h"
#include "mir_dce.h"

#include "mir_fact_validate.h"

static bool
mir_build_blocks_from_hir(MIRRoutine *routine, const HIRRoutine *hir_routine)
{
    if (routine == NULL)
        return false;

    if (hir_routine == NULL || !hir_routine->has_cfg || hir_routine->cfg.block_count == 0) {
        MIRBasicBlock block;
        memset(&block, 0, sizeof(block));
        block.id = 0;
        block.is_entry = true;
        block.is_reachable = true;
        block.source_hir_block_id = SIZE_MAX;
        mir_block_record_source_location(&block, NULL);
        if (!mir_seed_non_cfg_block_source_inventory(&block, routine->ast)) {
            free(block.source_statement_inventory.items);
            return false;
        }
        routine->entry_block = 0;
        if (!append_block(routine, block)) {
            free(block.source_statement_inventory.items);
            return false;
        }
        return true;
    }

    routine->entry_block = hir_routine->cfg.entry_block;
    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        const HIRBasicBlock *src = &hir_routine->cfg.blocks[i];
        const ASTNode *source_node = NULL;
        MIRBasicBlock block;
        memset(&block, 0, sizeof(block));
        block.id = i;
        block.is_entry = (i == hir_routine->cfg.entry_block);
        block.is_reachable = src->is_reachable;
        block.is_pin_region = src->is_pin_region;
        block.is_select_case_body = src->is_select_case_body;
        block.pin_view_is_write = src->pin_view_is_write;
        block.pin_source_name = src->pin_source_name;
        block.pin_view_name = src->pin_view_name;
        block.pin_block_ast = src->pin_block_ast;
        block.source_hir_block_id = src->id;
        if (src->statement_count > 0)
            source_node = src->statements[0];
        else if (src->terminator_condition != NULL)
            source_node = src->terminator_condition;
        else if (src->terminator_value != NULL)
            source_node = src->terminator_value;
        double t_bb = mir_timing_now();
        mir_block_record_source_location(&block, source_node);
        mir_timing_add(MIR_TIMING_BB_SOURCE_LOC, mir_timing_now() - t_bb);
        t_bb = mir_timing_now();
        block.succ_true = src->succ_true;
        block.succ_false = src->succ_false;
        block.has_succ_true = src->has_succ_true;
        block.has_succ_false = src->has_succ_false;
        if (!copy_indices(&block.predecessors,
                          &block.predecessor_count,
                          src->predecessors,
                          src->predecessor_count)) {
            free(block.predecessors);
            return false;
        }
        block.predecessor_capacity = block.predecessor_count;
        if (!mir_copy_ast_nodes(&block.source_statement_inventory.items,
                                &block.source_statement_inventory.count,
                                src->statements,
                                src->statement_count)
            || !mir_copy_names(&block.source_local_defs,
                               &block.source_local_def_count,
                               src->local_defs,
                               src->local_def_count)
            || !copy_indices(&block.source_dom_tree_children,
                             &block.source_dom_tree_child_count,
                             src->dom_tree_children,
                             src->dom_tree_child_count)
            || !mir_copy_phi_nodes(&block.source_phi_nodes,
                                   &block.source_phi_node_count,
                                   src->phi_nodes,
                                   src->phi_node_count)) {
            free(block.predecessors);
            free(block.source_statement_inventory.items);
            free((void *)block.source_local_defs);
            free(block.source_dom_tree_children);
            if (block.source_phi_nodes != NULL) {
                for (size_t j = 0; j < block.source_phi_node_count; j++)
                    free(block.source_phi_nodes[j].incoming_predecessors);
            }
            free(block.source_phi_nodes);
            return false;
        }
        mir_timing_add(MIR_TIMING_BB_COPIES, mir_timing_now() - t_bb);
        t_bb = mir_timing_now();
        if (!append_block(routine, block))
            return false;
        mir_timing_add(MIR_TIMING_BB_APPEND, mir_timing_now() - t_bb);
    }

    {
        double t_pt = mir_timing_now();
        for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
            const HIRBasicBlock *src = &hir_routine->cfg.blocks[i];
            if (!mir_add_phi_placeholders(routine, &routine->blocks[i]))
                return false;
            if (!mir_add_terminator_instruction(routine,
                                                &routine->blocks[i],
                                                src->terminator_kind,
                                                src->terminator_condition,
                                                src->terminator_value))
                return false;
        }
        mir_timing_add(MIR_TIMING_BB_PHI_TERM, mir_timing_now() - t_pt);
    }

    return true;
}

#include "mir_decl_headers.h"
#include "mir_cfg_contract_validate.h"
#include "mir_abi_layout.h"

MIRProgram *
mir_lower(const HIRProgram *hir, const RIRProgram *rir,
          const SemanticResult *semantic, char **error_message)
{
    const char *debug_mir_lower;
    MIRProgram *mir;
    if (error_message != NULL)
        *error_message = NULL;
    if (hir == NULL || semantic == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR lowering requires HIR and semantic facts");
        return NULL;
    }

    debug_mir_lower = getenv("PGY_DEBUG_MIR_LOWER");

    mir_abi_table_init();

    mir = calloc(1, sizeof(MIRProgram));
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return NULL;
    }
    mir->has_function_param_flow_facts = hir->has_function_param_flow_facts;
    mir->has_resource_flow_facts = hir->has_resource_flow_facts;
    mir->has_loop_flow_facts = hir->has_loop_flow_facts;
    if (!mir_import_parallel_capture_facts(mir, semantic, error_message)) {
        mir_destroy(mir);
        return NULL;
    }

#define MIR_COPY_AST_LIST(field, count_field) \
    do { \
        mir->count_field = hir->count_field; \
        if (hir->count_field > 0) { \
            mir->field = calloc(hir->count_field, sizeof(ASTNode *)); \
            if (mir->field == NULL) { \
                if (error_message != NULL) \
                    *error_message = pergyra_strdup("out of memory"); \
                mir_destroy(mir); \
                return NULL; \
            } \
            memcpy(mir->field, hir->field, hir->count_field * sizeof(ASTNode *)); \
        } \
    } while (0)

    MIR_COPY_AST_LIST(externs, extern_count);
    MIR_COPY_AST_LIST(types, type_count);
    MIR_COPY_AST_LIST(abilities, ability_count);
    MIR_COPY_AST_LIST(roles, role_count);
    MIR_COPY_AST_LIST(parties, party_count);
    MIR_COPY_AST_LIST(rosters, roster_count);
    MIR_COPY_AST_LIST(worlds, world_count);
    MIR_COPY_AST_LIST(relations, relation_count);
    MIR_COPY_AST_LIST(effects, effect_count);
    MIR_COPY_AST_LIST(zones, zone_count);
    MIR_COPY_AST_LIST(events, event_count);
    MIR_COPY_AST_LIST(intents, intent_count);
    MIR_COPY_AST_LIST(functions, function_count);
    mir->has_top_level_exec = false;
    mir->has_main_function = false;
    mir->main_function_name = NULL;
    for (size_t i = 0; i < mir->function_count; i++) {
        ASTNode *fn = mir->functions[i];
        const char *fn_name = ast_declaration_name(fn);
        if (fn == NULL || fn->type != AST_FUNC_DECL
            || fn_name == NULL) {
            continue;
        }
        if (strcmp(fn_name, "__pgy_top_level_exec") == 0)
            mir->has_top_level_exec = true;
        if (strcmp(fn_name, "Main") == 0) {
            mir->has_main_function = true;
            mir->main_function_name = fn_name;
        } else if (strcmp(fn_name, "main") == 0) {
            mir->has_main_function = true;
            if (mir->main_function_name == NULL)
                mir->main_function_name = fn_name;
        }
    }
    mir_program_record_inventory_surface_usage(mir);

#undef MIR_COPY_AST_LIST

    for (size_t i = 0; i < hir->function_count; i++) {
        if (!mir_record_decl_header(mir, hir->functions[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->type_count; i++) {
        if (!mir_record_decl_header(mir, hir->types[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->ability_count; i++) {
        if (!mir_record_decl_header(mir, hir->abilities[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->intent_count; i++) {
        if (!mir_record_decl_header(mir, hir->intents[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->party_count; i++) {
        if (!mir_record_decl_header(mir, hir->parties[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->role_count; i++) {
        if (!mir_record_decl_header(mir, hir->roles[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->roster_count; i++) {
        if (!mir_record_decl_header(mir, hir->rosters[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->world_count; i++) {
        if (!mir_record_decl_header(mir, hir->worlds[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->relation_count; i++) {
        if (!mir_record_decl_header(mir, hir->relations[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->effect_count; i++) {
        if (!mir_record_decl_header(mir, hir->effects[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->zone_count; i++) {
        if (!mir_record_decl_header(mir, hir->zones[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->event_count; i++) {
        if (!mir_record_decl_header(mir, hir->events[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }

    HIRRoutineInventory hir_inventory;
    hir_routine_inventory_from_program(hir, &hir_inventory);
    for (size_t i = 0; i < hir_inventory.count; i++) {
        const HIRRoutine *hir_routine =
            hir_routine_inventory_get(&hir_inventory, i);
        MIRRoutine routine;
        const HIRBasicBlock *cfg_blocks_before = NULL;
        size_t cfg_block_count_before = 0;
        if (hir_routine == NULL) {
            if (error_message != NULL)
                *error_message =
                    pergyra_strdup("invalid HIR routine inventory");
            mir_destroy(mir);
            return NULL;
        }
        memset(&routine, 0, sizeof(routine));
        pgy_arena_init(&routine.scratch, 0);
        routine.id = mir->routine_count;
        routine.kind = mir_scope_kind_from_hir(hir_routine);
        routine.name = hir_routine->name;
        routine.ast = hir_routine->ast;
        routine.is_action_like = hir_routine->is_action_like;
        routine.hir_routine = hir_routine;
        routine.source_syntax_id = hir_routine->source_syntax_id;
        {
            double t0 = mir_timing_now();
            routine.rir_scope = mir_find_matching_rir_scope(rir, hir_routine);
            mir_timing_add(MIR_TIMING_RIR_MATCH, mir_timing_now() - t0);
        }
        routine.owner_name = routine.rir_scope != NULL
            ? routine.rir_scope->owner_name
            : hir_routine->owner_name;
        routine.owner_ast_type = hir_routine->owner_ast_type;
        if (routine.ast != NULL && routine.ast->type == AST_FUNC_DECL) {
            routine.generic_param_count = ast_generic_param_count(
                ast_declaration_generic_params(routine.ast));
            routine.params =
                ast_func_params(routine.ast, &routine.param_count);
            routine.return_type = ast_func_return_type(routine.ast);
            routine.within_zone = ast_func_within_zone(routine.ast);
            routine.has_signature = true;
            double t_sig = mir_timing_now();
            bool sig_ok = mir_routine_signature_metadata_capture(mir, &routine);
            mir_timing_add(MIR_TIMING_SIGNATURE, mir_timing_now() - t_sig);
            if (!sig_ok) {
                mir_routine_signature_metadata_clear(&routine);
                pgy_arena_destroy(&routine.scratch);
                if (error_message != NULL)
                    *error_message = pergyra_strdup("out of memory");
                mir_destroy(mir);
                return NULL;
            }
            if (!mir_copy_iteration_type_facts(&routine, hir_routine,
                                               error_message)) {
                mir_routine_signature_metadata_clear(&routine);
                mir_free_iteration_type_facts(&routine);
                pgy_arena_destroy(&routine.scratch);
                mir_destroy(mir);
                return NULL;
            }
            double t_loc = mir_timing_now();
            bool loc_ok = mir_routine_source_local_type_names_capture(mir, &routine);
            mir_timing_add(MIR_TIMING_SOURCE_LOCAL_TYPES,
                           mir_timing_now() - t_loc);
            if (!loc_ok) {
                mir_routine_source_local_type_names_clear(&routine);
                mir_free_iteration_type_facts(&routine);
                mir_routine_signature_metadata_clear(&routine);
                pgy_arena_destroy(&routine.scratch);
                if (error_message != NULL)
                    *error_message = pergyra_strdup("out of memory");
                mir_destroy(mir);
                return NULL;
            }
        }
        if (!mir_copy_resource_flow_symbols(
                &routine, hir_routine, error_message)) {
            mir_routine_signature_metadata_clear(&routine);
            mir_routine_source_local_type_names_clear(&routine);
            mir_free_iteration_type_facts(&routine);
            pgy_arena_destroy(&routine.scratch);
            mir_free_resource_flow_symbols(&routine);
            mir_destroy(mir);
            return NULL;
        }
        if (!mir_copy_function_param_flow_summaries(
                &routine, hir_routine, error_message)) {
            mir_routine_signature_metadata_clear(&routine);
            mir_routine_source_local_type_names_clear(&routine);
            mir_free_iteration_type_facts(&routine);
            pgy_arena_destroy(&routine.scratch);
            mir_free_resource_flow_symbols(&routine);
            mir_destroy(mir);
            return NULL;
        }
        if (!mir_copy_loop_flow_facts(&routine, hir_routine, error_message)) {
            free(routine.function_param_flow_summaries);
            mir_routine_signature_metadata_clear(&routine);
            mir_routine_source_local_type_names_clear(&routine);
            mir_free_iteration_type_facts(&routine);
            pgy_arena_destroy(&routine.scratch);
            mir_free_resource_flow_symbols(&routine);
            mir_destroy(mir);
            return NULL;
        }
        cfg_blocks_before =
            hir_routine->has_cfg ? hir_routine->cfg.blocks : NULL;
        cfg_block_count_before =
            hir_routine->has_cfg ? hir_routine->cfg.block_count : 0;

        bool routine_ok = true;
        double t_step;
#define MIR_TIMED_STEP(slot, call) \
        do { \
            if (routine_ok) { \
                t_step = mir_timing_now(); \
                routine_ok = (call); \
                mir_timing_add((slot), mir_timing_now() - t_step); \
            } \
        } while (0)
        MIR_TIMED_STEP(MIR_TIMING_BUILD_BLOCKS,
                       mir_build_blocks_from_hir(&routine, hir_routine));
        MIR_TIMED_STEP(MIR_TIMING_CLEANUP_BLOCK,
                       mir_append_cleanup_block(&routine, routine.rir_scope));
        MIR_TIMED_STEP(MIR_TIMING_POPULATE_INSTS,
                       mir_populate_instructions(&routine));
        MIR_TIMED_STEP(MIR_TIMING_SSA_RENAME,
                       mir_apply_ssa_rename(&routine));
        MIR_TIMED_STEP(MIR_TIMING_STMT_INSTS,
                       mir_populate_stmt_instructions(&routine));
        MIR_TIMED_STEP(MIR_TIMING_STMT_INSTS,
                       mir_enrich_machine_layer_facts(&routine));
        MIR_TIMED_STEP(MIR_TIMING_SPECULATION,
                       mir_capture_speculation_facts(&routine));
        MIR_TIMED_STEP(MIR_TIMING_USE_EDGES,
                       mir_populate_use_edges(&routine));
        MIR_TIMED_STEP(MIR_TIMING_CLEANUP_EDGES,
                       mir_materialize_cleanup_edges(&routine));
        MIR_TIMED_STEP(MIR_TIMING_RECOMPUTE,
                       mir_recompute_analysis(&routine));
#undef MIR_TIMED_STEP
        if (getenv("PGY_DEBUG_MIR_TIMING") != NULL) {
            double routine_total = mir_timing_total();
            static double prev_total;
            if (routine_total - prev_total > 0.05) {
                fprintf(stderr,
                    "[mir timing]   routine '%s': +%.3fs (blocks=%zu)\n",
                    routine.name != NULL ? routine.name : "(anonymous)",
                    routine_total - prev_total, routine.block_count);
            }
            prev_total = routine_total;
        }
        if (!routine_ok || !append_routine(mir, routine)) {
            free(routine.function_param_flow_summaries);
            free(routine.loop_flow_summaries);
            free(routine.loop_flow_states);
            mir_free_iteration_type_facts(&routine);
            mir_free_resource_flow_symbols(&routine);
            mir_routine_signature_metadata_clear(&routine);
            pgy_arena_destroy(&routine.scratch);
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }

        if (hir_routine->has_cfg
            && (hir_routine->cfg.blocks != cfg_blocks_before
                || hir_routine->cfg.block_count != cfg_block_count_before)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "HIR CFG storage changed during MIR lowering for routine '%s' (before_count=%zu after_count=%zu)",
                    routine.name != NULL ? routine.name : "(anonymous)",
                    cfg_block_count_before,
                    hir_routine->cfg.block_count);
            }
            mir_destroy(mir);
            return NULL;
        }

        if (debug_mir_lower != NULL && debug_mir_lower[0] != '\0' && routine.kind == MIR_SCOPE_INTENT) {
            fprintf(stdout,
                "[MIR LOWER] Intent '%s' after build: has_cleanup=%d, blocks=%zu\n",
                routine.name ? routine.name : "(null)",
                routine.has_cleanup_block, routine.block_count);
            for (size_t b = 0; b < routine.block_count; b++) {
                fprintf(stdout,
                    "  block[%zu] has_cleanup_succ=%d has_rollback_succ=%d has_invalidation_succ=%d\n",
                    b, routine.blocks[b].has_cleanup_succ,
                    routine.blocks[b].has_rollback_succ,
                    routine.blocks[b].has_invalidation_succ);
            }
        }
    }

    mir_link_decl_method_routines(mir);

    {
        double t0 = mir_timing_now();
        bool dce_ok = mir_run_dce_pass(mir, error_message);
        mir_timing_add(MIR_TIMING_DCE, mir_timing_now() - t0);
        if (!dce_ok) {
            mir_destroy(mir);
            return NULL;
        }
    }
    mir_refresh_non_cfg_body_fallback_inventory(mir);

    if (getenv("PGY_DEBUG_MIR_TIMING") != NULL)
        mir_timing_report();

    return mir;
}
