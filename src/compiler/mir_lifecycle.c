#include "mir.h"
#include "mir_base_helpers.h"
#include "mir_signature_metadata.h"
#include "mir_source_local_types.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/arena.h"

void
mir_destroy(MIRProgram *mir)
{
    if (mir == NULL)
        return;
    if (mir->routines != NULL) {
        for (size_t i = 0; i < mir->routine_count; i++) {
            MIRRoutine *routine = &mir->routines[i];
            if (routine->blocks != NULL) {
                for (size_t j = 0; j < routine->block_count; j++) {
                    free(routine->blocks[j].predecessors);
                    free(routine->blocks[j].source_statement_inventory.items);
                    free((void *)routine->blocks[j].source_local_defs);
                    free(routine->blocks[j].source_dom_tree_children);
                    if (routine->blocks[j].source_phi_nodes != NULL) {
                        for (size_t m = 0; m < routine->blocks[j].source_phi_node_count; m++)
                            free(routine->blocks[j].source_phi_nodes[m].incoming_predecessors);
                    }
                    free(routine->blocks[j].source_phi_nodes);
                    if (routine->blocks[j].instructions != NULL) {
                        for (size_t k = 0; k < routine->blocks[j].instruction_count; k++) {
                            free((void *)routine->blocks[j].instructions[k].result_name);
                            for (size_t m = 0; m < routine->blocks[j].instructions[k].use_count; m++)
                                free((void *)routine->blocks[j].instructions[k].uses[m]);
                            free((void *)routine->blocks[j].instructions[k].uses);
                            if (routine->blocks[j].instructions[k].phi_incomings != NULL) {
                                for (size_t m = 0; m < routine->blocks[j].instructions[k].phi_incoming_count; m++)
                                    free((void *)routine->blocks[j].instructions[k].phi_incomings[m].value_name);
                            }
                            free(routine->blocks[j].instructions[k].phi_incomings);
                        }
                    }
                    for (size_t k = 0; k < routine->blocks[j].renamed_local_count; k++)
                        free((void *)routine->blocks[j].renamed_locals[k]);
                    free((void *)routine->blocks[j].renamed_locals);
                    for (size_t k = 0; k < routine->blocks[j].ssa_entry_value_count; k++)
                        free((void *)routine->blocks[j].ssa_entry_values[k]);
                    free((void *)routine->blocks[j].ssa_entry_values);
                    for (size_t k = 0; k < routine->blocks[j].ssa_exit_value_count; k++)
                        free((void *)routine->blocks[j].ssa_exit_values[k]);
                    free((void *)routine->blocks[j].ssa_exit_values);
                    free((void *)routine->blocks[j].use_names);
                    free((void *)routine->blocks[j].def_names);
                    free((void *)routine->blocks[j].live_in_names);
                    free((void *)routine->blocks[j].live_out_names);
                    free(routine->blocks[j].ssa_entry_versions);
                    free(routine->blocks[j].ssa_exit_versions);
                    free(routine->blocks[j].instructions);
                }
            }
            if (routine->value_summaries != NULL) {
                for (size_t j = 0; j < routine->value_summary_count; j++)
                    free((void *)routine->value_summaries[j].name);
            }
            free(routine->value_summaries);
            mir_routine_signature_type_names_clear(routine);
            mir_routine_source_local_type_names_clear(routine);
            free(routine->blocks);
            pgy_arena_destroy(&routine->scratch);
        }
    }
    free(mir->externs);
    if (mir->decl_headers != NULL) {
        for (size_t i = 0; i < mir->decl_header_count; i++) {
            if (mir->decl_headers[i].method_metadata != NULL) {
                for (size_t j = 0;
                     j < mir->decl_headers[i].method_metadata_count;
                     j++) {
                    MIRDeclMethod *method =
                        &mir->decl_headers[i].method_metadata[j];
                    if (method->param_type_names != NULL) {
                        for (size_t k = 0; k < method->param_count; k++)
                            free(method->param_type_names[k]);
                    }
                    free(method->param_type_names);
                    free(method->return_type_name);
                }
            }
            free(mir->decl_headers[i].generic_metadata);
            free(mir->decl_headers[i].method_metadata);
            free(mir->decl_headers[i].field_metadata);
            for (size_t v = 0;
                 v < mir->decl_headers[i].variant_metadata_count; v++) {
                const MIRDeclEnumVariant *variant =
                    &mir->decl_headers[i].variant_metadata[v];
                if (variant->param_type_names != NULL) {
                    for (size_t p = 0; p < variant->param_count; p++)
                        free((void *)variant->param_type_names[p]);
                }
                free((void *)variant->param_type_names);
            }
            free(mir->decl_headers[i].variant_metadata);
        }
    }
    free(mir->decl_headers);
    free(mir->types);
    free(mir->abilities);
    free(mir->roles);
    free(mir->parties);
    free(mir->rosters);
    free(mir->worlds);
    free(mir->relations);
    free(mir->effects);
    free(mir->zones);
    free(mir->events);
    free(mir->intents);
    free(mir->functions);
    free(mir->routines);
    free(mir);
}

void
mir_dump(const MIRProgram *mir, FILE *out)
{
    if (out == NULL)
        out = stdout;
    if (mir == NULL) {
        fprintf(out, "MIR: (null)\n");
        return;
    }

    fprintf(out,
            "MIR Program\n  routines: %zu\n  noncfg-fallbacks: total=%zu routines=%zu recorded=%s\n",
            mir->routine_count,
            mir->non_cfg_body_fallback_total,
            mir->non_cfg_body_fallback_routine_count,
            mir->has_non_cfg_body_fallback_inventory ? "yes" : "no");
    if (mir->routine_count > 0 && mir->routines == NULL) {
        fprintf(out, "  invalid: routine count without routine inventory\n");
        return;
    }
    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        fprintf(out,
                "  routine[%02zu] %-8s %s blocks=%zu instructions=%zu cleanup-block=%s rollback-block=%s invalidation-block=%s phi=%zu renamed=%zu cleanup-edges=%zu uses=%zu live=%zu dce=%zu noncfg=%zu\n",
                i,
                mir_scope_kind_name(routine->kind),
                routine->name != NULL ? routine->name : "(anonymous)",
                routine->block_count,
                routine->instruction_count,
                routine->has_cleanup_block ? "yes" : "no",
                routine->has_rollback_block ? "yes" : "no",
                routine->has_invalidation_block ? "yes" : "no",
                routine->phi_inserted_count,
                routine->renamed_value_count,
                routine->cleanup_edge_count,
                routine->use_edge_count,
                routine->live_value_count,
                routine->dce_removed_count,
                routine->non_cfg_body_fallback_count);
        if (routine->has_use_def_summary) {
            fprintf(out, "    values=%zu\n", routine->value_summary_count);
            if (routine->value_summary_count > 0
                && routine->value_summaries == NULL) {
                fprintf(out, "      invalid: value-summary count without value-summary inventory\n");
            } else {
                for (size_t j = 0; j < routine->value_summary_count; j++) {
                    const MIRValueSummary *summary = &routine->value_summaries[j];
                    fprintf(out,
                            "      value[%02zu] %s slot=%s def=b%zu:i%zu uses=%zu liveIn=%zu liveOut=%zu writes=%zu rewrite=%s xblock=%s phi=%s cleanup=%s\n",
                            j,
                            summary->name != NULL ? summary->name : "(anonymous)",
                            summary->slot_anchor != NULL ? summary->slot_anchor : "-",
                            summary->def_block,
                            summary->def_inst,
                            summary->use_count,
                            summary->live_in_block_count,
                            summary->live_out_block_count,
                            summary->ast_write_count,
                            summary->has_ast_reassignment ? "yes" : "no",
                            summary->crosses_block_boundary ? "yes" : "no",
                            summary->used_by_phi ? "yes" : "no",
                            summary->reaches_cleanup ? "yes" : "no");
                    if (summary->first_use_block != SIZE_MAX || summary->last_use_block != SIZE_MAX) {
                        fprintf(out,
                                "        use-range=%s%zu..%s%zu\n",
                                summary->first_use_block == SIZE_MAX ? "-" : "b",
                                summary->first_use_block == SIZE_MAX ? 0 : summary->first_use_block,
                                summary->last_use_block == SIZE_MAX ? "-" : "b",
                                summary->last_use_block == SIZE_MAX ? 0 : summary->last_use_block);
                    }
                }
            }
        }
        if (routine->block_count > 0 && routine->blocks == NULL) {
            fprintf(out, "    invalid: block count without block inventory\n");
            continue;
        }
        for (size_t j = 0; j < routine->block_count; j++) {
            const MIRBasicBlock *block = &routine->blocks[j];
            char *label = mir_strdup_fmt("_pgy_mir_bb_%s_%zu",
                                         routine->name != NULL ? routine->name : "(anonymous)",
                                         j);
            char *source_ast_loc = NULL;
            char *source_ast_id = NULL;
            if (label == NULL) {
                fprintf(out, "    invalid: block label allocation failed\n");
                continue;
            }
            if (mir_block_has_source_location(block)) {
                source_ast_loc = mir_strdup_fmt("line %u:%u",
                                                mir_block_source_line(block),
                                                mir_block_source_column(block));
                source_ast_id = mir_strdup_fmt("line-%u-col-%u",
                                               mir_block_source_line(block),
                                               mir_block_source_column(block));
                if (source_ast_loc == NULL || source_ast_id == NULL) {
                    fprintf(out, "    invalid: block source-location allocation failed\n");
                    free(source_ast_loc);
                    free(source_ast_id);
                    free(label);
                    continue;
                }
            }
            fprintf(out,
                    "    block[%02zu] label=%s reachable=%s cleanup=%s "
                    "source-hir=%zu source-ast=%s source-ast-id=%s preds=%zu succT=%s succF=%s cleanupSucc=%s instructions=%zu defs=%zu uses=%zu liveIn=%zu liveOut=%zu\n",
                    j,
                    label,
                    block->is_reachable ? "yes" : "no",
                    block->is_cleanup ? "yes" : "no",
                    (size_t)(block->source_hir_block_id),
                    source_ast_loc != NULL ? source_ast_loc : "<none>",
                    source_ast_id != NULL ? source_ast_id : "<none>",
                    block->predecessor_count,
                    block->has_succ_true ? "yes" : "no",
                    block->has_succ_false ? "yes" : "no",
                    block->has_cleanup_succ ? "yes" : "no",
                    block->instruction_count,
                    block->def_name_count,
                    block->use_name_count,
                    block->live_in_name_count,
                    block->live_out_name_count);
            if (block->has_rollback_succ || block->has_invalidation_succ) {
                fprintf(out,
                        "      exceptional rollback=%s invalidation=%s\n",
                        block->has_rollback_succ ? "yes" : "no",
                        block->has_invalidation_succ ? "yes" : "no");
            }
            if (block->ssa_entry_value_count > 0) {
                fprintf(out, "      entry:");
                for (size_t k = 0; k < block->ssa_entry_value_count; k++)
                    fprintf(out, " %s", block->ssa_entry_values[k]);
                fprintf(out, "\n");
            }
            if (block->renamed_local_count > 0) {
                fprintf(out, "      renamed:");
                for (size_t k = 0; k < block->renamed_local_count; k++)
                    fprintf(out, " %s", block->renamed_locals[k]);
                fprintf(out, "\n");
            }
            if (block->ssa_exit_value_count > 0) {
                fprintf(out, "      exit:");
                for (size_t k = 0; k < block->ssa_exit_value_count; k++)
                    fprintf(out, " %s", block->ssa_exit_values[k]);
                fprintf(out, "\n");
            }
            if (block->def_name_count > 0) {
                fprintf(out, "      defs:");
                for (size_t k = 0; k < block->def_name_count; k++)
                    fprintf(out, " %s", block->def_names[k]);
                fprintf(out, "\n");
            }
            if (block->use_name_count > 0) {
                fprintf(out, "      uses:");
                for (size_t k = 0; k < block->use_name_count; k++)
                    fprintf(out, " %s", block->use_names[k]);
                fprintf(out, "\n");
            }
            if (block->live_in_name_count > 0) {
                fprintf(out, "      live-in:");
                for (size_t k = 0; k < block->live_in_name_count; k++)
                    fprintf(out, " %s", block->live_in_names[k]);
                fprintf(out, "\n");
            }
            if (block->live_out_name_count > 0) {
                fprintf(out, "      live-out:");
                for (size_t k = 0; k < block->live_out_name_count; k++)
                    fprintf(out, " %s", block->live_out_names[k]);
                fprintf(out, "\n");
            }
            if (block->instruction_count > 0 && block->instructions == NULL) {
                fprintf(out, "      invalid: instruction count without instruction inventory\n");
                free(source_ast_loc);
                free(source_ast_id);
                free(label);
                continue;
            }
            for (size_t k = 0; k < block->instruction_count; k++) {
                const MIRInstruction *inst = &block->instructions[k];
                fprintf(out,
                        "      inst[%02zu] %-12s slot=%s name=%s result=%s arg0=%s arg1=%s",
                        k,
                        mir_inst_kind_name(inst->kind),
                        inst->slot_anchor != NULL ? inst->slot_anchor : "-",
                        inst->name != NULL ? inst->name : "-",
                        inst->result_name != NULL ? inst->result_name : "-",
                        inst->arg0 != NULL ? inst->arg0 : "-",
                        inst->arg1 != NULL ? inst->arg1 : "-");
                if (inst->phi_incoming_count > 0) {
                    fprintf(out, " incoming=");
                    for (size_t m = 0; m < inst->phi_incoming_count; m++) {
                        fprintf(out,
                                "%s%zu:%s",
                                m == 0 ? "" : ",",
                                inst->phi_incomings[m].predecessor_block,
                                inst->phi_incomings[m].value_name != NULL
                                    ? inst->phi_incomings[m].value_name
                                    : "-");
                    }
                }
                if (inst->use_count > 0) {
                    fprintf(out, " uses=");
                    for (size_t m = 0; m < inst->use_count; m++)
                        fprintf(out, "%s%s", m == 0 ? "" : ",", inst->uses[m]);
                }
                if (mir_instruction_has_source_location(inst)) {
                    ASTNodeType ast_type = (ASTNodeType)
                        mir_instruction_source_ast_type_or(inst, AST_PROGRAM);
                    fprintf(out, " ast=%s ast-type=%d line=%u",
                            mir_source_ast_type_name(ast_type),
                            (int)ast_type,
                            mir_instruction_source_line(inst));
                }
                if (inst->kind == MIR_INST_BRANCH)
                    fprintf(out, " branch-shape=%s",
                            mir_branch_shape_name(inst->branch_shape));
                if (inst->requires_source_branch_emit)
                    fprintf(out, " source-branch-emit");
                if (inst->requires_source_statement_emit)
                    fprintf(out, " source-stmt-emit");
                if (inst->requires_source_local_decl_emit)
                    fprintf(out, " source-local-decl-emit");
                if (inst->requires_channel_receive_statement_emit)
                    fprintf(out, " channel-recv-stmt-emit");
                if (inst->requires_select_receive_statement_emit)
                    fprintf(out, " select-recv-stmt-emit");
                fprintf(out, "\n");
            }
            free(source_ast_loc);
            free(source_ast_id);
            free(label);
        }
    }
}
