#ifndef PGY_SRC_CODEGEN_TRANSPILER_MIR_PHI_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_MIR_PHI_EMIT_H

/* C backend MIR phi-copy emission owner. */

static bool
transpiler_mir_ssa_name_is_pin_view(const MIRBasicBlock *pred_block,
                                    const MIRBasicBlock *target_block,
                                    const char *name)
{
    char base[128];
    size_t version = 0;

    if (name == NULL)
        return false;
    if (!transpiler_parse_versioned_name(name, base, sizeof(base), &version)) {
        snprintf(base, sizeof(base), "%s", name);
    }

    return (pred_block != NULL
            && pred_block->pin_view_name != NULL
            && strcmp(base, pred_block->pin_view_name) == 0)
        || (target_block != NULL
            && target_block->pin_view_name != NULL
            && strcmp(base, target_block->pin_view_name) == 0);
}

static bool
transpiler_emit_mir_phi_copies(CodeBuf *buf, TranspilerCtx *ctx, int indent,
                               size_t pred_block_index,
                               const MIRBasicBlock *pred_block,
                               const MIRBasicBlock *target_block)
{
    const char *pred_bases[TRANSPILE_SSA_MAP_CAPACITY];
    const char *pred_versions[TRANSPILE_SSA_MAP_CAPACITY];
    const char *target_bases[TRANSPILE_SSA_MAP_CAPACITY];
    const char *target_versions[TRANSPILE_SSA_MAP_CAPACITY];
    size_t pred_count = 0;
    size_t target_count = 0;

    if (buf == NULL || pred_block == NULL || target_block == NULL)
        return false;

    for (size_t i = 0; i < target_block->instruction_count; i++) {
        const MIRInstruction *inst = &target_block->instructions[i];
        if (inst->kind != MIR_INST_PHI || inst->result_name == NULL)
            continue;
        if (transpiler_mir_ssa_name_is_pin_view(pred_block, target_block,
                                                inst->result_name)) {
            continue;
        }
        for (size_t j = 0; j < inst->phi_incoming_count; j++) {
            const MIRPhiIncoming *incoming = &inst->phi_incomings[j];
            if (incoming->predecessor_block != pred_block_index
                || incoming->value_name == NULL
                || strcmp(inst->result_name, incoming->value_name) == 0
                || transpiler_mir_ssa_name_is_pin_view(pred_block,
                                                       target_block,
                                                       incoming->value_name)) {
                continue;
            }
            {
                char *lhs = transpiler_render_ssa_name(ctx, inst->result_name);
                char *rhs = transpiler_render_ssa_name(ctx, incoming->value_name);
                write_indent_to(buf, indent);
                codebuf_write(buf, "%s = %s;\n", lhs, rhs);
                free(lhs);
                free(rhs);
            }
            break;
        }
    }

    if (!transpiler_collect_ssa_name_entries(
            pred_block->ssa_exit_value_count > 0
                ? pred_block->ssa_exit_values
                : pred_block->ssa_entry_values,
            pred_block->ssa_exit_value_count > 0
                ? pred_block->ssa_exit_value_count
                : pred_block->ssa_entry_value_count,
            pred_bases,
            pred_versions,
            TRANSPILE_SSA_MAP_CAPACITY,
            &pred_count)) {
        return false;
    }
    if (!transpiler_collect_ssa_name_entries(target_block->ssa_entry_values,
                                             target_block->ssa_entry_value_count,
                                             target_bases,
                                             target_versions,
                                             TRANSPILE_SSA_MAP_CAPACITY,
                                             &target_count)) {
        transpiler_free_ssa_name_entries(pred_bases, pred_count);
        return false;
    }

    for (size_t i = 0; i < target_count; i++) {
        const char *lhs_version = target_versions[i];
        const char *rhs_version = NULL;

        if (lhs_version == NULL || target_bases[i] == NULL)
            continue;
        if (transpiler_mir_ssa_name_is_pin_view(pred_block, target_block,
                                                lhs_version)) {
            continue;
        }
        for (size_t j = 0; j < pred_count; j++) {
            if (pred_bases[j] != NULL
                && strcmp(pred_bases[j], target_bases[i]) == 0) {
                rhs_version = pred_versions[j];
                break;
            }
        }
        if (rhs_version == NULL || strcmp(lhs_version, rhs_version) == 0)
            continue;
        if (transpiler_mir_ssa_name_is_pin_view(pred_block, target_block,
                                                rhs_version)) {
            continue;
        }
        {
            char *lhs = transpiler_render_ssa_name(ctx, lhs_version);
            char *rhs = transpiler_render_ssa_name(ctx, rhs_version);
            write_indent_to(buf, indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);
        }
    }

    transpiler_free_ssa_name_entries(pred_bases, pred_count);
    transpiler_free_ssa_name_entries(target_bases, target_count);
    return true;
}
#endif /* PGY_SRC_CODEGEN_TRANSPILER_MIR_PHI_EMIT_H */
