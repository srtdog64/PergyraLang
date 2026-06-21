#include "mir.h"

#include <string.h>

#include "../common/string_compat.h"
#include "mir_base_helpers.h"
#include "mir_cfg_contract_validate.h"
#include "mir_fact_validate.h"
#include "mir_public_surface.h"
#include "mir_validation.h"

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
    MIRRoutineInventory inventory;
    if (mir == NULL)
        return true;
    mir_routine_inventory_from_program(mir, &inventory);
    if (inventory.count > 0 && inventory.routines == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR program records %zu routine(s) without routine inventory",
                inventory.count);
        }
        return false;
    }
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine = mir_routine_inventory_get(&inventory, i);
        if (routine == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR program routine inventory row[%zu] is invalid", i);
            }
            return false;
        }
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
        if (routine->source_local_type_count
            > routine->source_local_type_capacity) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' records %zu source-local type facts above capacity %zu",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->source_local_type_count,
                    routine->source_local_type_capacity);
            }
            return false;
        }
        if (routine->source_local_type_count > 0
            && routine->source_local_types == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' records %zu source-local type facts without source-local type inventory",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->source_local_type_count);
            }
            return false;
        }
        for (size_t j = 0; j < routine->source_local_type_count; j++) {
            const MIRSourceLocalType *fact = &routine->source_local_types[j];
            if (fact->name == NULL || fact->type_name == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' source-local type fact[%zu] is incomplete",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        j);
                }
                return false;
            }
            if (fact->is_callable) {
                if (fact->callable_return_type_name == NULL
                    || (fact->callable_param_count > 0
                        && fact->callable_param_type_names == NULL)) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' source-local callable type fact[%zu] is incomplete",
                            routine->name != NULL
                                ? routine->name
                                : "(anonymous)",
                            j);
                    }
                    return false;
                }
                for (size_t k = 0; k < fact->callable_param_count; k++) {
                    if (fact->callable_param_type_names[k] == NULL) {
                        if (error_message != NULL) {
                            *error_message = mir_strdup_fmt(
                                "MIR routine '%s' source-local callable type fact[%zu] has missing parameter metadata",
                                routine->name != NULL
                                    ? routine->name
                                    : "(anonymous)",
                                j);
                        }
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

static bool
mir_validate_non_cfg_fallback_inventory(const MIRProgram *mir,
                                        char **error_message)
{
    size_t fallback_total = 0;
    size_t fallback_routines = 0;

    if (mir == NULL)
        return true;
    if (!mir->has_non_cfg_body_fallback_inventory)
        return true;

    mir_count_non_cfg_body_fallback_inventory(mir,
                                              &fallback_total,
                                              &fallback_routines);
    if (mir->non_cfg_body_fallback_total != fallback_total
        || mir->non_cfg_body_fallback_routine_count != fallback_routines) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR program non-CFG fallback inventory is stale (recorded total=%zu routines=%zu, actual total=%zu routines=%zu)",
                mir->non_cfg_body_fallback_total,
                mir->non_cfg_body_fallback_routine_count,
                fallback_total,
                fallback_routines);
        }
        return false;
    }
    return true;
}

bool
mir_validate(const MIRProgram *mir, char **error_message)
{
    MIRRoutineInventory inventory;
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

    mir_routine_inventory_from_program(mir, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine = mir_routine_inventory_get(&inventory, i);
        if (routine == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR program routine inventory row[%zu] is invalid", i);
            }
            return false;
        }

        if (!mir_validate_non_cfg_fallback_state(routine, error_message))
            return false;
        if (!mir_validate_cfg_contract_state(routine, false, true, true,
                                             error_message)) {
            return false;
        }

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

            if (!mir_validate_block_liveness_sets(routine, block, j,
                                                  error_message)) {
                return false;
            }
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

    if (!mir_validate_non_cfg_fallback_inventory(mir, error_message))
        return false;

    return true;
}
