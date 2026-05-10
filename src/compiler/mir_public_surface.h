#ifndef PGY_MIR_PUBLIC_SURFACE_H
#define PGY_MIR_PUBLIC_SURFACE_H

static bool
mir_validate_non_cfg_fallback_state(const MIRRoutine *routine,
                                    char **error_message)
{
    if (routine == NULL)
        return true;
    if (routine->non_cfg_body_fallback_count > 0
        && !routine->used_non_cfg_body_fallback) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' records non-CFG body fallback count without fallback flag",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        return false;
    }
    if (routine->used_non_cfg_body_fallback
        && routine->non_cfg_body_fallback_count == 0) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' records non-CFG body fallback flag without fallback count",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        return false;
    }
    if (routine->hir_routine != NULL
        && routine->hir_routine->has_cfg
        && routine->used_non_cfg_body_fallback) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' is CFG-backed but used non-CFG body fallback",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        return false;
    }
    return true;
}
static bool
mir_validate_program_inventory_shape(const MIRProgram *mir,
                                     char **error_message)
{
    if (mir == NULL)
        return true;
    if (mir->routine_count > 0 && mir->routines == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR program records %zu routine(s) without routine inventory",
                mir->routine_count);
        }
        return false;
    }
    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        if (routine->block_count > 0 && routine->blocks == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' records %zu block(s) without block inventory",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->block_count);
            }
            return false;
        }
        if (routine->value_summary_count > 0
            && routine->value_summaries == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' records %zu value summaries without value-summary inventory",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->value_summary_count);
            }
            return false;
        }
    }
    return true;
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

    if (!mir_validate_decl_header_metadata(mir, error_message))
        return false;
    if (!mir_validate_program_inventory_shape(mir, error_message))
        return false;
    if (!mir_validate_inventory_surface_usage(mir, error_message))
        return false;

    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];

        if (!mir_validate_non_cfg_fallback_state(routine, error_message))
            return false;
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

        if (!mir_validate_routine_emission_facts(routine, error_message))
            return false;

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

void
mir_routine_inventory_from_program(const MIRProgram *mir,
                                   MIRRoutineInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->routines = NULL;
    inventory->count = 0;
    if (mir != NULL) {
        inventory->routines = mir->routines;
        inventory->count = mir->routine_count;
    }
}

const MIRRoutine *
mir_routine_inventory_get(const MIRRoutineInventory *inventory, size_t index)
{
    if (inventory == NULL || inventory->routines == NULL
        || index >= inventory->count) {
        return NULL;
    }
    return &inventory->routines[index];
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

#endif /* PGY_MIR_PUBLIC_SURFACE_H */
