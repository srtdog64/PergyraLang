#include "mir_cfg_contract_validate.h"

#include <stdlib.h>

#include "../common/string_compat.h"
#include "mir_base_helpers.h"
#include "mir_cfg_contract_pin.h"
#include "mir_cfg_contract_control.h"
#include "mir_cfg_contract_cleanup_fact.h"
#include "mir_cfg_contract_cleanup_root_membership.h"
#include "mir_cfg_contract_roots.h"
#include "mir_cfg_contract_cleanup_roots.h"
#include "mir_cfg_contract_edges.h"
#include "mir_cleanup_fact_names.h"

bool
mir_validate_cfg_contract_state(const MIRRoutine *routine,
                               bool require_cleanup,
                               bool require_cleanup_source_mapping,
                               bool require_mapping_for_all_blocks,
                               char **error_message)
{
    bool *hir_block_seen = NULL;
    bool requires_cleanup_for_body = require_cleanup;
    const size_t cfg_block_count = (routine != NULL && routine->hir_routine != NULL
                                   && routine->hir_routine->has_cfg)
                                    ? routine->hir_routine->cfg.block_count
                                    : 0;

    if (routine == NULL)
        return false;

    if (routine->blocks == NULL || routine->block_count == 0) {
        return false;
    }

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (!block->is_cleanup && block->is_pin_region) {
            requires_cleanup_for_body = true;
            break;
        }
    }

    if (!mir_validate_cfg_contract_roots(routine, error_message))
        return false;

    if (!mir_validate_cfg_contract_cleanup_roots(
            routine, requires_cleanup_for_body, error_message))
        return false;

    if (cfg_block_count > 0) {
        hir_block_seen = calloc(cfg_block_count, sizeof(bool));
        if (hir_block_seen == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            return false;
        }
    }

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        bool require_source_mapping = require_mapping_for_all_blocks
            ? (!block->is_cleanup)
            : (block->is_reachable && (!block->is_cleanup || require_cleanup_source_mapping));

        if (cfg_block_count > 0 && require_source_mapping) {
            size_t source_id = block->source_hir_block_id;
            if (source_id == SIZE_MAX || source_id >= cfg_block_count) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] uses invalid source_hir_block_id",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i);
                }
                free(hir_block_seen);
                return false;
            }
            if (hir_block_seen[source_id]) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' source_hir_block_id collision on block[%zu] -> hir[%zu]",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i,
                        source_id);
                }
                free(hir_block_seen);
                return false;
            }
            hir_block_seen[source_id] = true;
        }

        if (block->is_cleanup && (block->has_succ_true || block->has_succ_false)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' cleanup block[%zu] must not have normal CFG successors",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    i);
            }
            free(hir_block_seen);
            return false;
        }

        if (block->is_cleanup
            && !mir_cleanup_block_is_registered_root(routine, i)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' cleanup block[%zu] is not registered as a cleanup root",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    i);
            }
            free(hir_block_seen);
            return false;
        }

        if (block->is_cleanup && block->is_pin_region) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' cleanup block[%zu] must not be a pin region",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    i);
            }
            free(hir_block_seen);
            return false;
        }

        if (block->is_reachable && !block->is_cleanup && block->is_pin_region) {
            if (block->pin_source_name == NULL || block->pin_source_name[0] == '\0') {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' pin-region block[%zu] missing pin source name",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i);
                }
                free(hir_block_seen);
                return false;
            }
            if (block->pin_view_name == NULL || block->pin_view_name[0] == '\0') {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' pin-region block[%zu] missing pin view name",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i);
                }
                free(hir_block_seen);
                return false;
            }
        }

        if (block->is_reachable && !block->is_cleanup
            && (block->has_succ_true || block->has_succ_false)) {
            for (size_t inst_id = 0; inst_id < block->instruction_count; inst_id++) {
                const MIRInstruction *inst = &block->instructions[inst_id];
                if (inst->kind == MIR_INST_LOOP_INIT) {
                    if (inst->arg0 == NULL || inst->expr0 == NULL || inst->expr1 == NULL) {
                        if (error_message != NULL) {
                            *error_message = mir_strdup_fmt(
                                "MIR routine '%s' block[%zu] has incomplete loop-init fact",
                                routine->name != NULL ? routine->name : "(anonymous)",
                                i);
                        }
                        free(hir_block_seen);
                        return false;
                    }
                }
                if (inst->kind == MIR_INST_BRANCH
                    && (inst->branch_shape == MIR_BRANCH_FOR_RANGE
                        || inst->branch_shape == MIR_BRANCH_FOR_IN)
                    && (inst->arg0 == NULL || inst->expr0 == NULL || inst->expr1 == NULL)) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] has incomplete loop-branch fact",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            i);
                    }
                    free(hir_block_seen);
                    return false;
                }
                if (inst->kind == MIR_INST_STMT
                    && mir_stmt_ast_is_cfg_owned_control(inst->ast)) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] keeps CFG-owned control statement as fallback STMT",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            i);
                    }
                    free(hir_block_seen);
                    return false;
                }
            }
        }

        if (block->is_reachable && !block->is_cleanup) {
            if (routine->has_cleanup_block) {
                if (!block->has_cleanup_succ) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] missing cleanup edge from reachable non-cleanup block",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            i);
                    }
                    free(hir_block_seen);
                    return false;
                }
                if (block->cleanup_succ >= routine->block_count
                    || block->cleanup_succ != routine->cleanup_block) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] cleanup edge does not target cleanup block",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            i);
                    }
                    free(hir_block_seen);
                    return false;
                }
                if (!mir_block_has_cleanup_edge_fact(block,
                                                     MIR_CLEANUP_FACT_EDGE)) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] missing cleanup-edge MIR fact",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            i);
                    }
                    free(hir_block_seen);
                    return false;
                }
                if (block->is_pin_region
                    && !mir_block_has_pin_cleanup_edge(block)) {
                    const char *pin_reason =
                        mir_block_pin_cleanup_missing_reason(block);
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' pin-region block[%zu] missing pin-unpin cleanup fact: %s",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            i,
                            pin_reason != NULL ? pin_reason : "unknown pin cleanup mismatch");
                    }
                    free(hir_block_seen);
                    return false;
                }
            } else if (block->has_cleanup_succ || block->has_rollback_succ || block->has_invalidation_succ) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] has exceptional successor without cleanup root",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i);
                }
                free(hir_block_seen);
                return false;
            }
        }

        if (!block->is_cleanup && block->has_rollback_succ && !routine->has_rollback_block) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] has rollback successor but no rollback block",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    i);
            }
            free(hir_block_seen);
            return false;
        }
        if (!block->is_cleanup && block->has_invalidation_succ && !routine->has_invalidation_block) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] has invalidation successor but no invalidation block",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    i);
            }
            free(hir_block_seen);
            return false;
        }

        if (!mir_validate_cfg_contract_block_edges(routine, i, error_message)) {
            free(hir_block_seen);
            return false;
        }

        if (routine->has_rollback_block && block->has_rollback_succ) {
            if (!routine->has_rollback_block || block->rollback_succ != routine->rollback_block) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] rollback successor must target routine rollback block",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i);
                }
                free(hir_block_seen);
                return false;
            }
        }
        if (routine->has_invalidation_block && block->has_invalidation_succ) {
            if (!routine->has_invalidation_block
                || block->invalidation_succ != routine->invalidation_block) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] invalidation successor must target routine invalidation block",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i);
                }
                free(hir_block_seen);
                return false;
            }
        }

    }

    if (routine->has_cleanup_block) {
        const MIRBasicBlock *cleanup = &routine->blocks[routine->cleanup_block];

        if (routine->has_rollback_block
            && (!cleanup->has_rollback_succ || cleanup->rollback_succ != routine->rollback_block)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' cleanup block does not converge to rollback block",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            free(hir_block_seen);
            return false;
        }
        if (routine->has_invalidation_block
            && (!cleanup->has_invalidation_succ
                || cleanup->invalidation_succ != routine->invalidation_block)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' cleanup block does not converge to invalidation block",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            free(hir_block_seen);
            return false;
        }
        if (routine->has_rollback_block && routine->has_invalidation_block) {
            const MIRBasicBlock *rollback = &routine->blocks[routine->rollback_block];
            if (!rollback->has_invalidation_succ
                || rollback->invalidation_succ != routine->invalidation_block
                || !rollback->has_cleanup_succ
                || rollback->cleanup_succ != routine->invalidation_block) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' rollback block does not converge to invalidation block",
                        routine->name != NULL ? routine->name : "(anonymous)");
                }
                free(hir_block_seen);
                return false;
            }
        }
        if (routine->has_rollback_block
            && !mir_block_has_cleanup_edge_fact(&routine->blocks[routine->rollback_block],
                                                MIR_CLEANUP_FACT_EDGE_FROM_ROLLBACK)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' rollback block missing cleanup-edge MIR fact",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            free(hir_block_seen);
            return false;
        }
        if (routine->has_invalidation_block
            && !mir_block_has_cleanup_edge_fact(&routine->blocks[routine->invalidation_block],
                                                MIR_CLEANUP_FACT_EDGE_FROM_INVALIDATION)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' invalidation block missing cleanup-edge MIR fact",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            free(hir_block_seen);
            return false;
        }
    }

    free(hir_block_seen);
    return true;
}
