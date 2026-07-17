/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * HIR public dump/query/pass runner surface.
 */

#include "hir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
void
hir_dump(const HIRProgram *hir, FILE *out)
{
    if (out == NULL)
        out = stdout;

    if (hir == NULL) {
        fprintf(out, "HIR: (null)\n");
        return;
    }

    fprintf(out,
            "HIR Program\n"
            "  items: %zu\n"
            "  decls: %zu\n"
            "  routines: %zu\n"
            "  externs: %zu\n"
            "  types: %zu\n"
            "  abilities: %zu\n"
            "  roles: %zu\n"
            "  parties: %zu\n"
            "  rosters: %zu\n"
            "  worlds: %zu\n"
            "  subjects: %zu\n"
            "  events: %zu\n"
            "  functions: %zu\n"
            "  executables: %zu\n"
            "  has_resource_flow_facts: %s\n"
            "  has_function_param_flow_facts: %s\n"
            "  has_main: %s\n",
            hir->item_count,
            hir->decl_count,
            hir->routine_count,
            hir->extern_count,
            hir->type_count,
            hir->ability_count,
            hir->role_count,
            hir->party_count,
            hir->roster_count,
            hir->world_count,
            hir->subject_count,
            hir->event_count,
            hir->function_count,
            hir->executable_count,
            hir->has_resource_flow_facts ? "true" : "false",
            hir->has_function_param_flow_facts ? "true" : "false",
            hir->has_main_function ? "true" : "false");

    for (size_t i = 0; i < hir->item_count; i++) {
        const HIRTopLevelItem *item = &hir->items[i];
        fprintf(out, "  [%02zu] %-10s", i, hir_top_level_kind_name(item->kind));
        if (item->name != NULL)
            fprintf(out, " %s", item->name);
        fprintf(out, "\n");
    }

    if (hir->routine_count > 0) {
        fprintf(out, "  routines:\n");
        for (size_t i = 0; i < hir->routine_count; i++) {
            const HIRRoutine *routine = &hir->routines[i];
            fprintf(out,
                    "    [%02zu] id=%u source=%u %-8s %-18s phase=%s calls=%zu callees=%zu hosted=%s action=%s exported=%s reachable=%s cf=%s\n",
                    i,
                    routine->routine_id,
                    routine->source_syntax_id,
                    hir_top_level_kind_name(routine->kind),
                    routine->name != NULL ? routine->name : "(anonymous)",
                    hir_phase_name(HIR_PHASE_ROUTINE),
                    routine->direct_call_count,
                    routine->callee_routine_count,
                    routine->is_hosted ? "true" : "false",
                    routine->is_action_like ? "true" : "false",
                    routine->is_exported ? "true" : "false",
                    routine->is_entry_reachable ? "true" : "false",
                    routine->has_control_flow ? "true" : "false");
            if (getenv("PGY_DEBUG_RESOURCE_FLOW_FACTS") != NULL) {
                fprintf(out,
                        "         resource-flow-symbols=%zu\n",
                        routine->resource_flow_symbol_count);
                for (size_t j = 0;
                     j < routine->resource_flow_symbol_count;
                     j++) {
                    const HIRResourceFlowSymbol *symbol =
                        &routine->resource_flow_symbols[j];
                    fprintf(out,
                            "           resource-flow[%02zu] stable=%zu decl=%u parameter=%s parameter-index=%zu line=%u column=%u kind=%u name=%s\n",
                            j,
                            symbol->stable_index,
                            symbol->declaration_syntax_id,
                            symbol->is_parameter ? "true" : "false",
                            symbol->parameter_index,
                            symbol->line,
                            symbol->column,
                            symbol->symbol_kind,
                            symbol->name != NULL ? symbol->name : "<missing>");
                }
            }
            if (getenv("PGY_DEBUG_FUNCTION_PARAM_FLOW") != NULL) {
                fprintf(out,
                        "         function-param-flow-summaries=%zu\n",
                        routine->function_param_flow_summary_count);
                for (size_t j = 0;
                     j < routine->function_param_flow_summary_count;
                     j++) {
                    const HIRFunctionParamFlowSummary *summary =
                        &routine->function_param_flow_summaries[j];
                    fprintf(out,
                            "           function-param-flow[%02zu] parameter-index=%zu mask=%u\n",
                            j,
                            summary->parameter_index,
                            summary->mask);
                }
            }
            if (routine->signature_type_ref_count > 0) {
                fprintf(out, "         types=");
                for (size_t j = 0; j < routine->signature_type_ref_count; j++) {
                    if (j > 0)
                        fprintf(out, ",");
                    fprintf(out, "%s", routine->signature_type_refs[j]);
                }
                fprintf(out, "\n");
            }
            if (routine->has_cfg) {
                fprintf(out,
                        "         cfg=blocks:%zu entry:%zu\n",
                        routine->cfg.block_count,
                        routine->cfg.entry_block);
                for (size_t j = 0; j < routine->cfg.block_count; j++) {
                    const HIRBasicBlock *block = &routine->cfg.blocks[j];
                    fprintf(out,
                        "           block[%02zu] preds=%zu df=%zu succ=%s%s%s loop=%s depth=%zu reach=%s rpo=%zu idom=%s%zu stmts=%zu pin=%s\n",
                        j,
                        block->predecessor_count,
                        block->dominance_frontier_count,
                            block->has_succ_true ? "T" : "",
                            block->has_succ_false ? "F" : "",
                            (!block->has_succ_true && !block->has_succ_false) ? "-" : "",
                            block->is_loop_header ? "true" : "false",
                            block->loop_depth,
                            block->is_reachable ? "true" : "false",
                            block->rpo_index,
                        block->has_immediate_dominator ? "" : "-",
                        block->has_immediate_dominator ? block->immediate_dominator : 0,
                        block->statement_count,
                        block->is_pin_region ? "true" : "false");
                    if (block->is_pin_region) {
                        fprintf(out,
                            "             pin source=%s view=%s mode=%s\n",
                            block->pin_source_name != NULL ? block->pin_source_name : "<expr>",
                            block->pin_view_name != NULL ? block->pin_view_name : "<view>",
                            block->pin_view_is_write ? "write" : "read");
                    }
                    for (size_t s = 0; s < block->statement_count; s++) {
                        ASTNode *stmt = block->statements[s];
                        fprintf(out,
                            "             stmt[%02zu] type=%d line=%u\n",
                            s,
                            stmt != NULL ? (int)stmt->type : -1,
                            stmt != NULL ? stmt->line : 0);
                    }
                }
            }
        }
    }
}

void
hir_dump_mode(const HIRProgram *hir, FILE *out, HIRDumpMode mode)
{
    if (out == NULL)
        out = stdout;
    if (hir == NULL) {
        fprintf(out, "HIR: (null)\n");
        return;
    }

    if (mode == HIR_DUMP_SUMMARY) {
        hir_dump(hir, out);
        return;
    }

    fprintf(out,
            "HIR %s view\n"
            "  decls: %zu\n"
            "  routines: %zu\n",
            mode == HIR_DUMP_CFG ? "cfg"
            : mode == HIR_DUMP_DOM ? "dom"
            : "ssa",
            hir->decl_count,
            hir->routine_count);

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
        fprintf(out,
                "  [%02zu] %s %s reachable=%s calls=%zu blocks=%zu live=%zu dead=%zu returns=%zu normal-exits=%zu phi=%zu blocks-with-phi=%zu\n",
                i,
                hir_top_level_kind_name(routine->kind),
                routine->name != NULL ? routine->name : "(anonymous)",
                routine->is_entry_reachable ? "true" : "false",
                routine->direct_call_count,
                routine->has_cfg ? routine->cfg.block_count : 0,
                routine->reachable_block_count,
                routine->dead_block_count,
                routine->return_block_count,
                routine->normal_exit_block_count,
                routine->phi_candidate_count,
                routine->phi_candidate_block_count);

        if (!routine->has_cfg)
            continue;

        for (size_t j = 0; j < routine->cfg.block_count; j++) {
            const HIRBasicBlock *block = &routine->cfg.blocks[j];
            fprintf(out,
                    "    block[%02zu] reach=%s preds=%zu succ=%s%s%s",
                    j,
                    block->is_reachable ? "true" : "false",
                    block->predecessor_count,
                    block->has_succ_true ? "T" : "",
                    block->has_succ_false ? "F" : "",
                    (!block->has_succ_true && !block->has_succ_false) ? "-" : "");
            if (mode == HIR_DUMP_DOM || mode == HIR_DUMP_SSA) {
                fprintf(out,
                        " rpo=%zu idom=%s%zu df=%zu loop=%s depth=%zu",
                        block->rpo_index,
                        block->has_immediate_dominator ? "" : "-",
                        block->has_immediate_dominator ? block->immediate_dominator : 0,
                        block->dominance_frontier_count,
                        block->is_loop_header ? "true" : "false",
                        block->loop_depth);
            }
            if (mode == HIR_DUMP_SSA) {
                fprintf(out,
                        " defs=%zu phi=%zu dom-children=%zu",
                        block->local_def_count,
                        block->phi_node_count,
                        block->dom_tree_child_count);
            }
            fprintf(out, "\n");

            if (mode == HIR_DUMP_SSA) {
                if (block->local_def_count > 0) {
                    fprintf(out, "      defs=");
                    for (size_t k = 0; k < block->local_def_count; k++) {
                        if (k > 0)
                            fprintf(out, ",");
                        fprintf(out, "%s", block->local_defs[k]);
                    }
                    fprintf(out, "\n");
                }
                if (block->phi_node_count > 0) {
                    fprintf(out, "      phi =");
                    for (size_t k = 0; k < block->phi_node_count; k++) {
                        if (k > 0)
                            fprintf(out, ",");
                        fprintf(out, "%s", block->phi_nodes[k].name);
                    }
                    fprintf(out, "\n");
                }
            }
        }
    }
}

const HIRDecl *
hir_find_decl(const HIRProgram *hir, const char *name, HIRTopLevelKind kind)
{
    if (hir == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < hir->decl_count; i++) {
        const HIRDecl *decl = &hir->decls[i];
        if (decl->kind == kind && decl->name != NULL && strcmp(decl->name, name) == 0)
            return decl;
    }
    return NULL;
}

const HIRRoutine *
hir_find_routine(const HIRProgram *hir, const char *name, HIRTopLevelKind kind)
{
    const HIRRoutine *match = NULL;

    if (hir == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
        if (routine->kind == kind
            && routine->name != NULL
            && strcmp(routine->name, name) == 0) {
            if (match != NULL)
                return NULL;
            match = routine;
        }
    }
    return match;
}

const HIRRoutine *
hir_find_routine_by_id(const HIRProgram *hir, uint32_t routine_id)
{
    const HIRRoutine *routine;

    if (hir == NULL || routine_id == 0 || routine_id > hir->routine_count)
        return NULL;
    routine = &hir->routines[(size_t)routine_id - 1];
    return routine->routine_id == routine_id ? routine : NULL;
}

size_t
hir_resource_flow_symbol_count(const HIRRoutine *routine)
{
    return routine != NULL ? routine->resource_flow_symbol_count : 0;
}

const HIRResourceFlowSymbol *
hir_resource_flow_symbol_at(const HIRRoutine *routine, size_t index)
{
    if (routine == NULL || index >= routine->resource_flow_symbol_count)
        return NULL;
    return &routine->resource_flow_symbols[index];
}

void
hir_routine_inventory_from_program(const HIRProgram *hir,
                                   HIRRoutineInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->routines = NULL;
    inventory->count = 0;
    if (hir != NULL && hir->routines != NULL) {
        inventory->routines = hir->routines;
        inventory->count = hir->routine_count;
    }
}

const HIRRoutine *
hir_routine_inventory_get(const HIRRoutineInventory *inventory, size_t index)
{
    if (inventory == NULL || inventory->routines == NULL
        || index >= inventory->count)
        return NULL;
    return &inventory->routines[index];
}

void
hir_mutable_routine_inventory_from_program(
        HIRProgram *hir,
        HIRMutableRoutineInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->routines = NULL;
    inventory->count = 0;
    if (hir != NULL && hir->routines != NULL) {
        inventory->routines = hir->routines;
        inventory->count = hir->routine_count;
    }
}

HIRRoutine *
hir_mutable_routine_inventory_get(
        const HIRMutableRoutineInventory *inventory,
        size_t index)
{
    if (inventory == NULL || inventory->routines == NULL
        || index >= inventory->count)
        return NULL;
    return &inventory->routines[index];
}

bool
hir_run_routine_pass(HIRProgram *hir, HIRRoutinePass *pass, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;

    if (hir == NULL || pass == NULL || pass->run == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("HIR routine pass requires program and callback");
        return false;
    }

    pass->routines_visited = 0;
    pass->routines_matched = 0;

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
        bool kind_ok = ((routine->kind == HIR_TOPLEVEL_FUNCTION && pass->filter.include_functions)
                        || (routine->kind == HIR_TOPLEVEL_INTENT && pass->filter.include_intents));
        if (!kind_ok)
            continue;

        pass->routines_visited++;

        if (pass->filter.require_control_flow && !routine->has_control_flow)
            continue;
        if (pass->filter.require_action_like && !routine->is_action_like)
            continue;
        if (pass->filter.require_cfg && !routine->has_cfg)
            continue;
        if (pass->filter.require_entry_reachable && !routine->is_entry_reachable)
            continue;

        pass->routines_matched++;
        if (!pass->run(hir, routine, pass->userdata, error_message))
            return false;
    }

    return true;
}

bool
hir_run_block_pass(HIRProgram *hir, HIRBlockPass *pass, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;

    if (hir == NULL || pass == NULL || pass->run == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("HIR block pass requires program and callback");
        return false;
    }

    pass->routines_visited = 0;
    pass->blocks_visited = 0;
    pass->blocks_matched = 0;

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
        bool kind_ok = ((routine->kind == HIR_TOPLEVEL_FUNCTION && pass->filter.include_functions)
                        || (routine->kind == HIR_TOPLEVEL_INTENT && pass->filter.include_intents));
        if (!kind_ok)
            continue;

        pass->routines_visited++;

        if (pass->filter.require_cfg && !routine->has_cfg)
            continue;
        if (pass->filter.require_entry_reachable && !routine->is_entry_reachable)
            continue;
        if (!routine->has_cfg)
            continue;

        for (size_t j = 0; j < routine->cfg.block_count; j++) {
            const HIRBasicBlock *block = &routine->cfg.blocks[j];
            pass->blocks_visited++;
            if (block->is_reachable && !pass->filter.include_reachable_blocks)
                continue;
            if (!block->is_reachable && !pass->filter.include_dead_blocks)
                continue;
            pass->blocks_matched++;
            if (!pass->run(hir, routine, block, pass->userdata, error_message))
                return false;
        }
    }

    return true;
}
