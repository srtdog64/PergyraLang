/*
 * Copyright (c) 2026 Pergyra Language Project
 * HIR validation pass.
 */

#include "hir.h"

#include <stdio.h>
#include <string.h>

#include "../common/string_compat.h"
#include "hir_region_escape_validate.h"

static char *
hir_validate_strdup_fmt(const char *fmt, const char *routine_name, size_t block_id)
{
    char buffer[512];
    snprintf(buffer,
             sizeof(buffer),
             fmt,
             routine_name != NULL ? routine_name : "(anonymous)",
             block_id);
    return pergyra_strdup(buffer);
}

static char *
hir_validate_strdup_edge_fmt(const char *fmt,
                             const char *routine_name,
                             size_t block_id,
                             size_t edge_id)
{
    char buffer[512];
    snprintf(buffer,
             sizeof(buffer),
             fmt,
             routine_name != NULL ? routine_name : "(anonymous)",
             block_id,
             edge_id);
    return pergyra_strdup(buffer);
}

static bool
hir_validate_successor(const HIRRoutine *routine,
                       size_t block_index,
                       const char *edge_name,
                       size_t successor,
                       char **error_message)
{
    if (successor < routine->cfg.block_count) {
        const HIRBasicBlock *target = &routine->cfg.blocks[successor];
        for (size_t i = 0; i < target->predecessor_count; i++) {
            if (target->predecessors != NULL && target->predecessors[i] == block_index)
                return true;
        }

        if (error_message != NULL) {
            char buffer[512];
            snprintf(buffer,
                     sizeof(buffer),
                     "HIR routine '%s' block[%zu] %s successor %zu is missing reciprocal predecessor",
                     routine->name != NULL ? routine->name : "(anonymous)",
                     block_index,
                     edge_name != NULL ? edge_name : "CFG",
                     successor);
            *error_message = pergyra_strdup(buffer);
        }
        return false;
    }

    if (error_message != NULL) {
        *error_message = hir_validate_strdup_edge_fmt(
            "HIR routine '%s' block[%zu] has out-of-range successor %zu",
            routine->name,
            block_index,
            successor);
    }
    return false;
}

static bool
hir_validate_predecessors(const HIRRoutine *routine,
                          const HIRBasicBlock *block,
                          size_t block_index,
                          char **error_message)
{
    if (block->predecessor_count == 0)
        return true;
    if (block->predecessors == NULL) {
        if (error_message != NULL) {
            *error_message = hir_validate_strdup_fmt(
                "HIR routine '%s' block[%zu] has predecessor count without predecessor array",
                routine->name,
                block_index);
        }
        return false;
    }
    if (block->predecessor_count > block->predecessor_capacity) {
        if (error_message != NULL) {
            *error_message = hir_validate_strdup_fmt(
                "HIR routine '%s' block[%zu] has predecessor count above predecessor capacity",
                routine->name,
                block_index);
        }
        return false;
    }
    for (size_t i = 0; i < block->predecessor_count; i++) {
        if (block->predecessors[i] < routine->cfg.block_count)
            continue;
        if (error_message != NULL) {
            *error_message = hir_validate_strdup_edge_fmt(
                "HIR routine '%s' block[%zu] has out-of-range predecessor %zu",
                routine->name,
                block_index,
                block->predecessors[i]);
        }
        return false;
    }
    return true;
}

static bool
hir_validate_resource_flow_symbols(const HIRRoutine *routine,
                                   char **error_message)
{
    if (routine == NULL)
        return false;
    if (routine->resource_flow_symbol_count == 0)
        return routine->resource_flow_symbols == NULL
            || routine->resource_flow_symbol_capacity != 0;
    if (routine->resource_flow_symbols == NULL
        || routine->resource_flow_symbol_count
            > routine->resource_flow_symbol_capacity) {
        if (error_message != NULL)
            *error_message = hir_validate_strdup_fmt(
                "HIR routine '%s' has incomplete resource-flow symbol storage",
                routine->name,
                0);
        return false;
    }
    for (size_t i = 0; i < routine->resource_flow_symbol_count; i++) {
        const HIRResourceFlowSymbol *symbol =
            &routine->resource_flow_symbols[i];
        if (symbol->name == NULL || symbol->name[0] == '\0') {
            if (error_message != NULL)
                *error_message = hir_validate_strdup_fmt(
                    "HIR routine '%s' resource-flow symbol[%zu] has no name",
                    routine->name,
                    i);
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            if (routine->resource_flow_symbols[j].stable_index
                == symbol->stable_index) {
                if (error_message != NULL)
                    *error_message = hir_validate_strdup_fmt(
                        "HIR routine '%s' resource-flow symbols share stable index %zu",
                        routine->name,
                        symbol->stable_index);
                return false;
            }
            if (symbol->is_parameter
                && routine->resource_flow_symbols[j].is_parameter
                && routine->resource_flow_symbols[j].parameter_index
                    == symbol->parameter_index) {
                if (error_message != NULL)
                    *error_message = hir_validate_strdup_fmt(
                        "HIR routine '%s' resource-flow parameters share index %zu",
                        routine->name,
                        symbol->parameter_index);
                return false;
            }
        }
    }
    return true;
}

static bool
hir_validate_function_param_flow_summaries(const HIRRoutine *routine,
                                           char **error_message)
{
    if (routine == NULL)
        return false;
    if (routine->function_param_flow_summary_count == 0)
        return routine->function_param_flow_summaries == NULL
            || routine->function_param_flow_summary_capacity != 0;
    if (routine->function_param_flow_summaries == NULL
        || routine->function_param_flow_summary_count
            > routine->function_param_flow_summary_capacity) {
        if (error_message != NULL)
            *error_message = hir_validate_strdup_fmt(
                "HIR routine '%s' has incomplete function parameter flow summary storage",
                routine->name,
                0);
        return false;
    }
    for (size_t i = 0;
         i < routine->function_param_flow_summary_count;
         i++) {
        const HIRFunctionParamFlowSummary *summary =
            &routine->function_param_flow_summaries[i];
        if (summary->parameter_index >= routine->parameter_count) {
            if (error_message != NULL)
                *error_message = hir_validate_strdup_fmt(
                    "HIR routine '%s' function parameter flow summary[%zu] has out-of-range parameter index",
                    routine->name,
                    i);
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            if (routine->function_param_flow_summaries[j].parameter_index
                == summary->parameter_index) {
                if (error_message != NULL)
                    *error_message = hir_validate_strdup_fmt(
                        "HIR routine '%s' function parameter flow summaries share index %zu",
                        routine->name,
                        summary->parameter_index);
                return false;
            }
        }
    }
    return true;
}

static bool
hir_validate_loop_flow_summaries(const HIRRoutine *routine,
                                 char **error_message)
{
    if (routine == NULL)
        return false;
    if (routine->loop_flow_state_count > 0
        && (routine->loop_flow_states == NULL
            || routine->loop_flow_state_count
                > routine->loop_flow_state_capacity)) {
        if (error_message != NULL)
            *error_message = hir_validate_strdup_fmt(
                "HIR routine '%s' has incomplete loop-flow state storage",
                routine->name, 0);
        return false;
    }
    if (routine->loop_flow_summary_count == 0) {
        if (routine->loop_flow_state_count != 0) {
            if (error_message != NULL)
                *error_message = hir_validate_strdup_fmt(
                    "HIR routine '%s' has loop-flow states without summaries",
                    routine->name, 0);
            return false;
        }
        return routine->loop_flow_summaries == NULL
            || routine->loop_flow_summary_capacity != 0;
    }
    if (routine->loop_flow_summaries == NULL
        || routine->loop_flow_summary_count
            > routine->loop_flow_summary_capacity) {
        if (error_message != NULL)
            *error_message = hir_validate_strdup_fmt(
                "HIR routine '%s' has incomplete loop-flow summary storage",
                routine->name, 0);
        return false;
    }
    for (size_t i = 0; i < routine->loop_flow_summary_count; i++) {
        const HIRLoopFlowSummaryFact *summary =
            &routine->loop_flow_summaries[i];
        if (summary->function_syntax_id != routine->source_syntax_id
            || summary->loop_syntax_id == 0
            || summary->kind > 1u
            || summary->entry_state_start > routine->loop_flow_state_count
            || summary->entry_state_count
                > routine->loop_flow_state_count - summary->entry_state_start
            || summary->exit_state_start > routine->loop_flow_state_count
            || summary->exit_state_count
                > routine->loop_flow_state_count - summary->exit_state_start) {
            if (error_message != NULL)
                *error_message = hir_validate_strdup_fmt(
                    "HIR routine '%s' has an invalid loop-flow summary[%zu]",
                    routine->name, i);
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            if (routine->loop_flow_summaries[j].loop_syntax_id
                == summary->loop_syntax_id) {
                if (error_message != NULL)
                    *error_message = hir_validate_strdup_fmt(
                        "HIR routine '%s' loop-flow summaries share loop SyntaxNodeId %u",
                        routine->name,
                        summary->loop_syntax_id);
                return false;
            }
        }
    }
    for (size_t i = 0; i < routine->loop_flow_state_count; i++) {
        const HIRLoopFlowStateFact *state = &routine->loop_flow_states[i];
        for (size_t j = 0; j < i; j++) {
            /* A state row is a snapshot, so repeated stable indices are
             * expected across entry/exit and across loops.  Only storage and
             * value-domain checks belong here; identity uniqueness is owned by
             * ResourceFlowUniverse validation. */
            (void)state;
            (void)j;
        }
    }
    return true;
}

static bool
hir_domain_runtime_text_present(const char *text)
{
    return text != NULL && text[0] != '\0';
}

static bool
hir_validate_domain_runtime_facts(const HIRProgram *hir,
                                  char **error_message)
{
    if (!hir->has_domain_runtime_facts) {
        if (hir->domain_participant_role_facts != NULL
            || hir->domain_participant_role_fact_count != 0
            || hir->domain_projection_member_assignment_facts != NULL
            || hir->domain_projection_member_assignment_fact_count != 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "HIR domain runtime storage exists without semantic projection marker");
            return false;
        }
        return true;
    }
    if ((hir->domain_participant_role_fact_count == 0
         && hir->domain_participant_role_facts != NULL)
        || (hir->domain_participant_role_fact_count != 0
            && hir->domain_participant_role_facts == NULL)
        || (hir->domain_projection_member_assignment_fact_count == 0
            && hir->domain_projection_member_assignment_facts != NULL)
        || (hir->domain_projection_member_assignment_fact_count != 0
            && hir->domain_projection_member_assignment_facts == NULL)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "HIR domain runtime semantic snapshot has incomplete storage");
        return false;
    }

    for (size_t i = 0; i < hir->domain_participant_role_fact_count; i++) {
        const PgyDomainParticipantRoleFact *fact =
            &hir->domain_participant_role_facts[i];
        if (fact->program_syntax_id == 0
            || fact->program_syntax_id != hir->source_program_syntax_id
            || fact->owner_syntax_id == 0
            || fact->field_syntax_id == 0
            || (unsigned)fact->role
                > (unsigned)PGY_DOMAIN_PARTICIPANT_RELATION_TARGET
            || !hir_domain_runtime_text_present(fact->owner_name)
            || !hir_domain_runtime_text_present(fact->field_name)
            || !hir_domain_runtime_text_present(fact->field_type_name)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "HIR domain participant-role fact has incomplete exact identity, name, or type");
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const PgyDomainParticipantRoleFact *prior =
                &hir->domain_participant_role_facts[j];
            if ((prior->program_syntax_id == fact->program_syntax_id
                 && prior->owner_syntax_id == fact->owner_syntax_id
                 && prior->role == fact->role)
                || prior->field_syntax_id == fact->field_syntax_id) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "HIR domain participant-role facts duplicate stable identity");
                return false;
            }
        }
    }

    for (size_t i = 0;
         i < hir->domain_projection_member_assignment_fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *fact =
            &hir->domain_projection_member_assignment_facts[i];
        if (fact->program_syntax_id == 0
            || fact->program_syntax_id != hir->source_program_syntax_id
            || fact->owner_syntax_id == 0
            || fact->directive_syntax_id == 0
            || fact->projection_slot_syntax_id == 0
            || fact->source_slot_syntax_id == 0
            || fact->target_decl_syntax_id == 0
            || fact->target_field_syntax_id == 0
            || fact->source_decl_syntax_id == 0
            || (unsigned)fact->operation
                > (unsigned)PGY_DOMAIN_PROJECTION_BIND
            || !hir_domain_runtime_text_present(fact->owner_name)
            || !hir_domain_runtime_text_present(fact->projection_slot_name)
            || !hir_domain_runtime_text_present(fact->source_slot_name)
            || !hir_domain_runtime_text_present(fact->target_field_name)
            || !hir_domain_runtime_text_present(
                fact->target_field_type_name)
            || !hir_domain_runtime_text_present(fact->source_path)
            || !hir_domain_runtime_text_present(fact->source_leaf_type_name)
            || fact->source_path_segment_count == 0
            || fact->source_path_segments == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "HIR domain projection assignment has incomplete exact identity, name, type, or path");
            return false;
        }
        for (size_t s = 0; s < fact->source_path_segment_count; s++) {
            const PgyDomainProjectionPathSegmentFact *segment =
                &fact->source_path_segments[s];
            if (segment->field_syntax_id == 0
                || !hir_domain_runtime_text_present(segment->field_name)
                || !hir_domain_runtime_text_present(
                    segment->field_type_name)) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "HIR domain projection assignment has incomplete source-path identity");
                return false;
            }
        }
        if (strcmp(fact->source_path_segments[
                       fact->source_path_segment_count - 1]
                       .field_type_name,
                   fact->source_leaf_type_name) != 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "HIR domain projection assignment source path has leaf-type drift");
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const PgyDomainProjectionMemberAssignmentFact *prior =
                &hir->domain_projection_member_assignment_facts[j];
            if (prior->program_syntax_id == fact->program_syntax_id
                && prior->owner_syntax_id == fact->owner_syntax_id
                && prior->directive_syntax_id == fact->directive_syntax_id
                && prior->projection_slot_syntax_id
                    == fact->projection_slot_syntax_id
                && prior->target_field_syntax_id
                    == fact->target_field_syntax_id) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "HIR domain projection assignments duplicate stable member identity");
                return false;
            }
        }
    }
    return true;
}

bool
hir_validate(const HIRProgram *hir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;

    if (hir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("HIR validation requires program");
        return false;
    }

    HIRRoutineInventory inventory;
    hir_routine_inventory_from_program(hir, &inventory);

    if (!hir_validate_region_escape_facts(hir, &inventory, error_message))
        return false;
    if (!hir_validate_domain_runtime_facts(hir, error_message))
        return false;

    for (size_t i = 0; i < inventory.count; i++) {
        const HIRRoutine *routine = hir_routine_inventory_get(&inventory, i);
        if (routine == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("HIR validation has invalid routine inventory");
            return false;
        }
        if (i >= UINT32_MAX || routine->routine_id != (uint32_t)(i + 1)) {
            if (error_message != NULL) {
                *error_message = pergyra_strdup(
                    "HIR routine inventory has non-canonical RoutineId");
            }
            return false;
        }
        if (routine->kind != HIR_TOPLEVEL_EXECUTABLE
            && routine->source_syntax_id == 0) {
            if (error_message != NULL) {
                *error_message = pergyra_strdup(
                    "HIR source-backed routine is missing SyntaxNodeId");
            }
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const HIRRoutine *prior = hir_routine_inventory_get(
                &inventory, j);
            if (routine->source_syntax_id != 0 && prior != NULL
                && prior->source_syntax_id == routine->source_syntax_id) {
                if (error_message != NULL) {
                    *error_message = pergyra_strdup(
                        "HIR routines share one source SyntaxNodeId");
                }
                return false;
            }
        }
        if (routine->direct_call_count > 0
            && (routine->direct_calls == NULL
                || routine->direct_call_decl_ids == NULL)) {
            if (error_message != NULL) {
                *error_message = pergyra_strdup(
                    "HIR direct-call facts are incomplete");
            }
            return false;
        }
        if (routine->callee_routine_count > 0
            && routine->callee_routine_ids == NULL) {
            if (error_message != NULL) {
                *error_message = pergyra_strdup(
                    "HIR callgraph edges are missing RoutineId storage");
            }
            return false;
        }
        for (size_t j = 0; j < routine->callee_routine_count; j++) {
            uint32_t callee_id = routine->callee_routine_ids[j];
            const HIRRoutine *callee = callee_id > 0
                && callee_id <= inventory.count
                ? hir_routine_inventory_get(&inventory,
                    (size_t)callee_id - 1)
                : NULL;
            if (callee == NULL || callee->routine_id != callee_id) {
                if (error_message != NULL) {
                    *error_message = pergyra_strdup(
                        "HIR callgraph edge references an invalid RoutineId");
                }
                return false;
            }
        }
        if (!hir_validate_resource_flow_symbols(routine, error_message))
            return false;
        if (!hir_validate_function_param_flow_summaries(routine,
                                                        error_message))
            return false;
        if (!hir_validate_loop_flow_summaries(routine, error_message))
            return false;
        if (!routine->has_cfg) {
            if (routine->cfg.blocks != NULL || routine->cfg.block_count != 0) {
                if (error_message != NULL) {
                    *error_message = pergyra_strdup(
                        "HIR routine has CFG blocks but is not marked as CFG-backed");
                }
                return false;
            }
            continue;
        }

        if (routine->cfg.blocks == NULL || routine->cfg.block_count == 0) {
            if (error_message != NULL) {
                *error_message = pergyra_strdup(
                    "HIR CFG-backed routine has no CFG blocks");
            }
            return false;
        }
        if (routine->cfg.entry_block >= routine->cfg.block_count) {
            if (error_message != NULL) {
                *error_message = pergyra_strdup(
                    "HIR CFG-backed routine has invalid entry block");
            }
            return false;
        }

        for (size_t j = 0; j < routine->cfg.block_count; j++) {
            const HIRBasicBlock *block = &routine->cfg.blocks[j];
            if (block->id != j) {
                if (error_message != NULL) {
                    *error_message = hir_validate_strdup_edge_fmt(
                        "HIR routine '%s' block[%zu] has mismatched block id %zu",
                        routine->name,
                        j,
                        block->id);
                }
                return false;
            }
            if (!hir_validate_predecessors(routine, block, j, error_message))
                return false;
        }

        for (size_t j = 0; j < routine->cfg.block_count; j++) {
            const HIRBasicBlock *block = &routine->cfg.blocks[j];
            if (block->has_succ_true
                && !hir_validate_successor(routine, j, "true",
                                           block->succ_true, error_message)) {
                return false;
            }
            if (block->has_succ_false
                && !hir_validate_successor(routine, j, "false",
                                           block->succ_false, error_message)) {
                return false;
            }

            if (!block->is_pin_region)
                continue;
            if (block->pin_source_name == NULL || block->pin_source_name[0] == '\0') {
                if (error_message != NULL) {
                    *error_message = hir_validate_strdup_fmt(
                        "HIR routine '%s' pin-region block[%zu] missing pin source name",
                        routine->name,
                        j);
                }
                return false;
            }
            if (block->pin_view_name == NULL || block->pin_view_name[0] == '\0') {
                if (error_message != NULL) {
                    *error_message = hir_validate_strdup_fmt(
                        "HIR routine '%s' pin-region block[%zu] missing pin view name",
                        routine->name,
                        j);
                }
                return false;
            }
        }
    }

    if (hir->has_function_param_flow_facts) {
        size_t summary_count = 0;
        for (size_t i = 0; i < inventory.count; i++) {
            const HIRRoutine *routine = hir_routine_inventory_get(&inventory, i);
            if (routine != NULL)
                summary_count += routine->function_param_flow_summary_count;
        }
        if (summary_count == 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "HIR declares function parameter flow facts but carries no summaries");
            return false;
        }
    }

    if (hir->has_loop_flow_facts) {
        size_t summary_count = 0;
        for (size_t i = 0; i < inventory.count; i++) {
            const HIRRoutine *routine = hir_routine_inventory_get(&inventory, i);
            if (routine != NULL)
                summary_count += routine->loop_flow_summary_count;
        }
        if (summary_count == 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "HIR declares loop-flow facts but carries no summaries");
            return false;
        }
    }

    return true;
}
