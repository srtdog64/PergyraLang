/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM MIR emission contract owner.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

#include <stdlib.h>

#include "../compiler/mir_cleanup_fact_names.h"
#include "../compiler/mir_cfg_contract_cleanup_fact.h"
#include "../compiler/mir_cfg_contract_pin.h"
#include "../compiler/mir_fact_validate.h"

bool
llvm_mir_validate_cleanup_contract(const MIRRoutine *routine,
                                   LLVMGenCtx *ctx)
{
    const char *routine_name = routine != NULL && routine->name != NULL
        ? routine->name
        : "<routine>";

    if (routine == NULL)
        return false;
    {
        char *topology_error = NULL;
        if (!mir_validate_emission_topology(routine,
                                            routine->has_cleanup_block,
                                            false,
                                            &topology_error)) {
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR contract invalid for %s: %s",
                routine_name,
                topology_error != NULL ? topology_error : "topology validation failed");
            free(topology_error);
            return false;
        }
        free(topology_error);
    }
    {
        char *fact_error = NULL;
        if (!mir_validate_routine_emission_facts(routine, &fact_error)) {
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR contract invalid for %s: %s",
                routine_name,
                fact_error != NULL ? fact_error : "emission fact validation failed");
            free(fact_error);
            return false;
        }
        free(fact_error);
    }
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        const char *cleanup_fact;

        if (block->instruction_count > 0 && block->instructions == NULL) {
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR contract invalid for %s: block %zu has instruction count without instruction inventory",
                routine_name,
                i);
            return false;
        }
        if (block->has_cleanup_succ
            && block->cleanup_succ >= routine->block_count) {
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR contract invalid for %s: block %zu has bad cleanup successor",
                routine_name,
                i);
            return false;
        }
        if (block->has_rollback_succ
            && block->rollback_succ >= routine->block_count) {
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR contract invalid for %s: block %zu has bad rollback successor",
                routine_name,
                i);
            return false;
        }
        if (block->has_invalidation_succ
            && block->invalidation_succ >= routine->block_count) {
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR contract invalid for %s: block %zu has bad invalidation successor",
                routine_name,
                i);
            return false;
        }
        if ((block->has_cleanup_succ
             || block->has_rollback_succ
             || block->has_invalidation_succ)
            && !routine->has_cleanup_block) {
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR contract invalid for %s: block %zu has cleanup edge without cleanup root",
                routine_name,
                i);
            return false;
        }
        if (!block->has_cleanup_succ) {
            if (block->is_reachable && block->is_pin_region) {
                llvm_set_mir_topology_invalid(ctx,
                    "LLVM MIR contract invalid for %s: pin block %zu has no cleanup successor",
                    routine_name,
                    i);
                return false;
            }
            continue;
        }

        cleanup_fact = mir_cleanup_edge_fact_name_for_block(routine, i);
        if (!mir_block_has_expected_cleanup_edge_fact(routine, i)) {
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR contract invalid for %s: block %zu missing %s fact",
                routine_name,
                i,
                cleanup_fact);
            return false;
        }
        if (block->is_pin_region && !mir_block_has_pin_cleanup_edge(block)) {
            const char *reason = mir_block_pin_cleanup_missing_reason(block);
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR contract invalid for %s: pin block %zu missing pin cleanup fact (%s)",
                routine_name,
                i,
                reason != NULL ? reason : "unknown");
            return false;
        }
    }
    return true;
}

#endif
