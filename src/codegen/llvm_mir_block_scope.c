/* MIR block-entry SSA scope reconstruction. */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_block_scope.h"

#include "llvm_internal_api.h"
#include "llvm_mir_scope_bind.h"

#include <stdio.h>
#include <string.h>

static void
llvm_mir_bind_versioned_local_scope(LLVMGenCtx *ctx,
                                    LLVMMirVar *vars,
                                    size_t var_count,
                                    const char *versioned_name,
                                    const char *type_name)
{
    char base_name[128];
    LLVMMirVar *entry;

    if (ctx == NULL || versioned_name == NULL)
        return;
    if (!llvm_mir_base_name_from_versioned(versioned_name, base_name,
            sizeof(base_name))) {
        return;
    }
    entry = llvm_mir_get_var_entry(vars, var_count, versioned_name);
    if (entry == NULL) {
        /* Phi results reuse the first version's alloca as the merged base. */
        char first_ver[128];
        int written = snprintf(first_ver, sizeof(first_ver), "%s.1",
            base_name);
        if (written > 0 && (size_t)written < sizeof(first_ver)) {
            LLVMMirVar *base_entry =
                llvm_mir_get_var_entry(vars, var_count, first_ver);
            if (base_entry != NULL && base_entry->alloca != NULL) {
                llvm_mir_bind_base_local_scope(ctx, base_name,
                    base_entry->alloca, base_entry->type, type_name);
            }
        }
        return;
    }
    if (strcmp(base_name, versioned_name) != 0
        && llvm_lookup_channel_inner(ctx, base_name) != NULL) {
        return;
    }
    if (strcmp(base_name, versioned_name) != 0
        && llvm_lookup_slot_inner(ctx, base_name) != NULL
        && llvm_lookup_slot_inner(ctx, versioned_name) != NULL) {
        return;
    }
    llvm_mir_bind_base_local_scope(ctx, base_name, entry->alloca,
        entry->type, type_name);
}

void
llvm_mir_seed_block_entry_scope(const MIRBasicBlock *mir_block,
                                LLVMGenCtx *ctx,
                                LLVMMirVar *vars,
                                size_t var_count)
{
    if (mir_block == NULL)
        return;
    for (size_t i = 0; i < mir_block->ssa_entry_value_count; i++) {
        llvm_mir_bind_versioned_local_scope(ctx, vars, var_count,
            mir_block->ssa_entry_values[i], NULL);
    }
    for (size_t i = 0; i < mir_block->live_in_name_count; i++) {
        llvm_mir_bind_versioned_local_scope(ctx, vars, var_count,
            mir_block->live_in_names[i], NULL);
    }
}

void
llvm_mir_seed_block_phi_scope(const MIRBasicBlock *mir_block,
                              LLVMGenCtx *ctx,
                              LLVMMirVar *vars,
                              size_t var_count)
{
    if (mir_block == NULL)
        return;
    for (size_t i = 0; i < mir_block->instruction_count; i++) {
        const MIRInstruction *inst = &mir_block->instructions[i];
        if (inst->kind == MIR_INST_PHI && inst->result_name != NULL)
            llvm_mir_bind_versioned_local_scope(ctx, vars, var_count,
                inst->result_name, inst->arg1);
    }
}

#endif /* PGY_LLVM_ENABLED */
