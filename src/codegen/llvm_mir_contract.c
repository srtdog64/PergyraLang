/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM MIR emission contract owner.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

#include <stdlib.h>

LLVMGenResult *
llvm_validate_mir_for_codegen(const MIRProgram *mir)
{
    LLVMMIRRoutineInventory routine_inventory;

    if (mir == NULL) {
        return llvm_result_error_with_hints("MIR program is NULL",
            PGY_CODE_MIR_TOPOLOGY_INVALID,
            PGY_CAUSE_MIR_TOPOLOGY_INVALID,
            PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING);
    }

    llvm_mir_routine_inventory_from_program(mir, &routine_inventory);
    for (size_t i = 0; i < routine_inventory.count; i++) {
        const MIRRoutine *routine =
            llvm_routine_inventory_get(&routine_inventory, i);
        char *topology_error = NULL;

        if (routine == NULL) {
            return llvm_result_error_with_hints("MIR routine inventory is invalid",
                PGY_CODE_MIR_TOPOLOGY_INVALID,
                PGY_CAUSE_MIR_TOPOLOGY_INVALID,
                PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING);
        }
        const char *routine_name = llvm_mir_routine_name(routine);
        if (routine_name == NULL) {
            return llvm_result_error_with_hints("MIR routine is missing name",
                PGY_CODE_MIR_TOPOLOGY_INVALID,
                PGY_CAUSE_MIR_TOPOLOGY_INVALID,
                PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING);
        }

        if (!mir_validate_emission_contract(routine, false, false, &topology_error)) {
            LLVMGenResult *res = topology_error != NULL
                ? llvm_result_error_fmt_with_hints(
                    PGY_CODE_MIR_TOPOLOGY_INVALID,
                    PGY_CAUSE_MIR_TOPOLOGY_INVALID,
                    PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING,
                    "MIR routine '%s' emission topology invalid: %s",
                    routine_name != NULL ? routine_name : "(anonymous)",
                    topology_error)
                : llvm_result_error_with_hints(
                    "MIR emission topology validation failed",
                    PGY_CODE_MIR_TOPOLOGY_INVALID,
                    PGY_CAUSE_MIR_TOPOLOGY_INVALID,
                    PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING);
            free(topology_error);
            return res;
        }
        free(topology_error);
    }
    return NULL;
}

bool
llvm_mir_validate_cleanup_contract(const MIRRoutine *routine,
                                   LLVMGenCtx *ctx)
{
    const char *routine_name;

    if (routine == NULL)
        return false;

    routine_name = llvm_mir_routine_name(routine);
    if (routine_name == NULL)
        routine_name = "<routine>";

    char *topology_error = NULL;
    if (!mir_validate_emission_contract(routine,
                                        routine->has_cleanup_block,
                                        false,
                                        &topology_error)) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR contract invalid for %s: %s",
            routine_name,
            topology_error != NULL
                ? topology_error
                : "emission contract validation failed");
        free(topology_error);
        return false;
    }
    free(topology_error);
    return true;
}

#endif
