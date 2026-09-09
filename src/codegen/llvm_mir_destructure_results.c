/* Physical storage for the SSA owner's positional destructure outputs. */
#ifdef PGY_LLVM_ENABLED
#include "llvm_mir_destructure_results.h"
#include "llvm_internal_api.h"
#include "llvm_mir_type_helpers.h"
#include "../compiler/mir_destructure_type_facts.h"
#include "../compiler/mir_nominal_abi_layout.h"
#include <stdint.h>
#include <stdlib.h>

bool
llvm_mir_allocate_destructure_results(const MIRRoutine *routine,
    LLVMGenCtx *ctx, LLVMMirVar **vars, size_t *capacity, size_t *count)
{
    if (routine == NULL || ctx == NULL || vars == NULL || capacity == NULL || count == NULL)
        return false;
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *block = &routine->blocks[b];
        if (!block->is_reachable)
            continue;
        for (size_t i = 0; i < block->instruction_count; i++) {
            const MIRInstruction *inst = &block->instructions[i];
            if (inst->kind != MIR_INST_DESTRUCTURE)
                continue;
            if (inst->destructure_result_names == NULL || inst->destructure_binding_count == 0) {
                llvm_set_mir_inventory_missing(ctx, "LLVM MIR destructure is missing SSA output identities");
                return false;
            }
            for (size_t d = 0; d < inst->destructure_binding_count; d++) {
                const char *name = inst->destructure_result_names[d];
                const MIRDestructureTypeFact *fact = mir_routine_destructure_type_fact(
                    routine, inst->source_stable_id, d);
                if (name == NULL || fact == NULL || fact->binding_type_name == NULL
                    || fact->binding_count != inst->destructure_binding_count
                    || llvm_mir_get_var_entry(*vars, *count, name) != NULL) {
                    llvm_set_mir_inventory_missing(ctx, "LLVM MIR destructure output identity/type is missing or duplicated");
                    return false;
                }
                const MIRTypeLayout *layout = mir_program_abi_layout_for_type_name(
                    routine->program, fact->binding_type_name);
                LLVMTypeRef type = layout != NULL
                    ? llvm_mir_type_from_abi_layout(ctx, layout)
                    : pergyra_type_to_llvm(ctx, fact->binding_type_name);
                if (ctx->has_error || type == NULL) {
                    llvm_set_mir_inventory_missing(ctx, "LLVM MIR destructure output type cannot be materialized");
                    return false;
                }
                if (*count == *capacity) {
                    if (*capacity > SIZE_MAX / 2 / sizeof(**vars)) {
                        llvm_set_mir_inventory_missing(ctx, "LLVM MIR destructure storage inventory is too large");
                        return false;
                    }
                    size_t next = *capacity == 0 ? 64 : *capacity * 2;
                    LLVMMirVar *grown = realloc(*vars, next * sizeof(*grown));
                    if (grown == NULL) {
                        llvm_set_mir_inventory_missing(ctx, "LLVM MIR destructure storage allocation failed");
                        return false;
                    }
                    *vars = grown;
                    *capacity = next;
                }
                LLVMValueRef storage = llvm_create_entry_alloca(ctx, type, name);
                if (storage == NULL) {
                    llvm_set_mir_inventory_missing(ctx, "LLVM MIR destructure output allocation failed");
                    return false;
                }
                (*vars)[(*count)++] = (LLVMMirVar){name, fact->binding_type_name, storage, type};
            }
        }
    }
    return true;
}

bool
llvm_mir_store_destructure_results(const MIRInstruction *inst,
    LLVMGenCtx *ctx, LLVMMirVar *vars, size_t count)
{
    if (ctx->has_error)
        return false;
    if (inst->destructure_result_names == NULL) {
        llvm_set_mir_inventory_missing(ctx, "LLVM MIR destructure is missing SSA output identities");
        return false;
    }
    /* The destructure emitter just defined these exact positional bindings.
     * Publish to preallocated SSA storage now, never recover a later missing
     * use from whichever same-spelled binding happens to be in scope. */
    for (size_t d = 0; d < inst->destructure_binding_count; d++) {
        LLVMVarEntry value;
        LLVMMirVar *output = llvm_mir_get_var_entry(vars, count, inst->destructure_result_names[d]);
        if (output == NULL || !llvm_scope_lookup_snapshot(ctx, inst->destructure_binding_names[d], &value)
            || value.alloca == NULL || value.type != output->type) {
            llvm_set_mir_inventory_missing(ctx, "LLVM MIR destructure output storage contradicts its admitted type");
            return false;
        }
        LLVMBuildStore(ctx->builder,
            LLVMBuildLoad2(ctx->builder, value.type, value.alloca, llvm_tmp_name(ctx)), output->alloca);
    }
    return true;
}
#endif
