const char *
mir_scope_kind_name(MIRScopeKind kind)
{
    switch (kind) {
        case MIR_SCOPE_FUNCTION: return "function";
        case MIR_SCOPE_METHOD: return "method";
        case MIR_SCOPE_INTENT: return "intent";
        default: return "unknown";
    }
}

const char *
mir_inst_kind_name(MIRInstKind kind)
{
    switch (kind) {
        case MIR_INST_DEF: return "def";
        case MIR_INST_RESOURCE_OP: return "resource-op";
        case MIR_INST_PHI: return "phi";
        case MIR_INST_BRANCH: return "branch";
        case MIR_INST_RETURN: return "return";
        case MIR_INST_CLEANUP_EDGE: return "cleanup";
        case MIR_INST_STMT: return "stmt";
        default: return "unknown";
    }
}

void
mir_destroy(MIRProgram *mir)
{
    if (mir == NULL)
        return;
    for (size_t i = 0; i < mir->routine_count; i++) {
        MIRRoutine *routine = &mir->routines[i];
        for (size_t j = 0; j < routine->block_count; j++) {
            free(routine->blocks[j].predecessors);
            free(routine->blocks[j].source_statements);
            free((void *)routine->blocks[j].source_local_defs);
            free(routine->blocks[j].source_dom_tree_children);
            if (routine->blocks[j].source_phi_nodes != NULL) {
                for (size_t m = 0; m < routine->blocks[j].source_phi_node_count; m++)
                    free(routine->blocks[j].source_phi_nodes[m].incoming_predecessors);
            }
            free(routine->blocks[j].source_phi_nodes);
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
        for (size_t j = 0; j < routine->value_summary_count; j++)
            free((void *)routine->value_summaries[j].name);
        free(routine->value_summaries);
        free(routine->blocks);
        pgy_arena_destroy(&routine->scratch);
    }
    free(mir->externs);
    for (size_t i = 0; i < mir->decl_header_count; i++)
        free(mir->decl_headers[i].method_metadata);
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

bool
mir_validate(const MIRProgram *mir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR program is null");
        return false;
    }

    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];

        if (!mir_validate_cfg_contract_state(routine, false, true, true, error_message))
            return false;

        if (routine->block_count > 0 && !routine->has_liveness) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' is missing liveness information",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        if (routine->block_count > 0 && !routine->has_use_def_summary) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' is missing use-def summary",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        if (routine->block_count > 0 && !routine->has_dce) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' is missing DCE pass state",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }

        for (size_t j = 0; j < routine->value_summary_count; j++) {
            const MIRValueSummary *summary = &routine->value_summaries[j];
            if (summary->slot_anchor == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' value summary '%s' is missing slot anchor",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        summary->name != NULL ? summary->name : "(anonymous)");
                }
                return false;
            }
        }

        for (size_t j = 0; j < routine->block_count; j++) {
            const MIRBasicBlock *block = &routine->blocks[j];

            /* Entry block must have no predecessors */
            if (j == routine->entry_block && block->predecessor_count > 0) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' entry block[%zu] has %zu predecessors (expected 0)",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        j,
                        block->predecessor_count);
                }
                return false;
            }

            if (!mir_validate_block_liveness_sets(routine, block, j, error_message))
                return false;
            for (size_t k = 0; k < block->instruction_count; k++) {
                const MIRInstruction *inst = &block->instructions[k];
                if (inst->kind == MIR_INST_PHI
                    && inst->phi_incoming_count != block->predecessor_count) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] phi has %zu incoming edges but %zu predecessors",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            inst->phi_incoming_count,
                            block->predecessor_count);
                    }
                    return false;
                }
                /* RESOURCE_OP instructions must have a non-null rir_op */
                if (inst->kind == MIR_INST_RESOURCE_OP && inst->rir_op == NULL) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] instruction[%zu] RESOURCE_OP has null rir_op",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            k);
                    }
                    return false;
                }
                if ((inst->kind == MIR_INST_RESOURCE_OP
                     || (inst->kind == MIR_INST_CLEANUP_EDGE && inst->rir_op != NULL))
                    && inst->slot_anchor == NULL) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] instruction[%zu] is missing slot anchor",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            k);
                    }
                    return false;
                }
                if (inst->rir_op != NULL
                    && inst->slot_anchor != NULL
                    && inst->rir_op->slot_anchor != NULL
                    && strcmp(inst->slot_anchor, inst->rir_op->slot_anchor) != 0) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] instruction[%zu] slot anchor '%s' diverges from RIR '%s'",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            k,
                            inst->slot_anchor,
                            inst->rir_op->slot_anchor);
                    }
                    return false;
                }
            }
            if (!mir_validate_instruction_uses(routine, block, j, error_message))
                return false;
        }
    }

    return true;
}

bool
mir_validate_emission_topology(const MIRRoutine *routine,
                              bool require_cleanup,
                              bool require_cleanup_source_mapping,
                              char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    return mir_validate_cfg_contract_state(routine,
                                          require_cleanup,
                                          require_cleanup_source_mapping,
                                          false,
                                          error_message);
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

    fprintf(out, "MIR Program\n  routines: %zu\n", mir->routine_count);
    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        fprintf(out,
                "  routine[%02zu] %-8s %s blocks=%zu instructions=%zu cleanup-block=%s rollback-block=%s invalidation-block=%s phi=%zu renamed=%zu cleanup-edges=%zu uses=%zu live=%zu dce=%zu\n",
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
                routine->dce_removed_count);
        if (routine->has_use_def_summary) {
            fprintf(out, "    values=%zu\n", routine->value_summary_count);
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
        for (size_t j = 0; j < routine->block_count; j++) {
            const MIRBasicBlock *block = &routine->blocks[j];
        const char *label_name = NULL;
        const ASTNode *source_stmt = NULL;
        char source_ast_loc[64] = "<none>";
        char source_ast_id[32] = "<none>";
        char label[128];
        bool has_source_stmt = false;
        if (routine->name != NULL)
            snprintf(label, sizeof(label), "_pgy_mir_bb_%s_%zu", routine->name, j);
        else
            snprintf(label, sizeof(label), "_pgy_mir_bb_%s_%zu", "(anonymous)", j);
        if (block->source_statement_count > 0)
            source_stmt = block->source_statements[0];
        else if (block->source_terminator_condition != NULL)
            source_stmt = block->source_terminator_condition;
        else if (block->source_terminator_value != NULL)
            source_stmt = block->source_terminator_value;
        if (source_stmt != NULL) {
            if (source_stmt != NULL)
                snprintf(source_ast_loc, sizeof(source_ast_loc), "line %u:%u",
                    source_stmt->line, source_stmt->column);
            snprintf(source_ast_id, sizeof(source_ast_id), "%p", (const void *)source_stmt);
            has_source_stmt = true;
        }
        label_name = label;
        fprintf(out,
                "    block[%02zu] label=%s reachable=%s cleanup=%s "
                "source-hir=%zu source-ast=%s source-ast-id=%s preds=%zu succT=%s succF=%s cleanupSucc=%s instructions=%zu defs=%zu uses=%zu liveIn=%zu liveOut=%zu\n",
                j,
                label_name,
                block->is_reachable ? "yes" : "no",
                block->is_cleanup ? "yes" : "no",
                (size_t)(block->source_hir_block_id),
                source_ast_loc,
                has_source_stmt ? source_ast_id : "<none>",
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
                if (inst->ast != NULL) {
                    fprintf(out, " ast-type=%d line=%u",
                            (int)inst->ast->type,
                            inst->ast->line);
                }
                fprintf(out, "\n");
            }
        }
    }
}
