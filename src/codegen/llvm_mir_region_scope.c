/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM MIR verified-region scope lifecycle emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_internal_api.h"
#include "../compiler/verified_region_plan.h"

void
llvm_mir_region_scope_begin(LLVMGenCtx *ctx, const MIRRoutine *routine)
{
    uint32_t scope_id = 0;
    LLVMFuncEntry *create_fn;
    LLVMBasicBlockRef entry_block;
    LLVMValueRef args[2];

    if (ctx == NULL)
        return;

    /* Region state is function-local codegen state.  A nested/on-demand
     * routine without a region row must not inherit the caller's alloca. */
    ctx->region_alloca = NULL;
    ctx->region_owner_function = NULL;
    ctx->region_scope_id = 0;
    ctx->region_scope_active = false;

    if (routine == NULL || ctx->region_plan == NULL
        || ctx->type_region == NULL)
        return;
    if (!pgy_verified_region_plan_scope_for_function_id(
            ctx->region_plan, routine->source_syntax_id, &scope_id))
        return;
    create_fn = llvm_lookup_function(ctx, "pgy_region_create_export");
    if (create_fn == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM region plan requires runtime function '%s'",
            "pgy_region_create_export");
        return;
    }
    entry_block = ctx->builder != NULL
        ? LLVMGetInsertBlock(ctx->builder)
        : NULL;
    if (ctx->current_function == NULL || entry_block == NULL
        || LLVMGetBasicBlockParent(entry_block) != ctx->current_function
        || LLVMGetEntryBasicBlock(ctx->current_function) != entry_block) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR region scope begin requires the owning function entry block");
        return;
    }
    ctx->region_alloca = LLVMBuildAlloca(ctx->builder, ctx->type_region,
                                         "__pgy_region");
    ctx->region_owner_function = ctx->current_function;
    args[0] = ctx->region_alloca;
    args[1] = LLVMConstInt(ctx->type_i64, 0, 0);
    LLVMBuildCall2(ctx->builder, create_fn->fn_type, create_fn->fn,
                   args, 2, "");
    ctx->region_scope_id = scope_id;
    ctx->region_scope_active = true;
}

void
llvm_mir_region_scope_destroy(LLVMGenCtx *ctx)
{
    LLVMFuncEntry *destroy_fn;
    LLVMBasicBlockRef allocation_block;
    LLVMValueRef args[1];

    if (ctx == NULL)
        return;
    if (!ctx->region_scope_active) {
        if (ctx->region_alloca != NULL
            || ctx->region_owner_function != NULL
            || ctx->region_scope_id != 0) {
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR inactive region scope retained ambient function state");
        }
        return;
    }
    if (ctx->region_alloca == NULL || ctx->region_owner_function == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR active region scope is missing its entry allocation owner");
        return;
    }
    allocation_block = LLVMGetInstructionParent(ctx->region_alloca);
    if (ctx->current_function != ctx->region_owner_function
        || allocation_block == NULL
        || LLVMGetBasicBlockParent(allocation_block)
            != ctx->region_owner_function
        || LLVMGetEntryBasicBlock(ctx->region_owner_function)
            != allocation_block) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR region destroy escaped its owning function entry block");
        return;
    }
    destroy_fn = llvm_lookup_function(ctx, "pgy_region_destroy_export");
    if (destroy_fn == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM region plan requires runtime function '%s'",
            "pgy_region_destroy_export");
        return;
    }
    args[0] = ctx->region_alloca;
    LLVMBuildCall2(ctx->builder, destroy_fn->fn_type, destroy_fn->fn,
                   args, 1, "");
}

void
llvm_mir_region_scope_end(LLVMGenCtx *ctx)
{
    if (ctx == NULL)
        return;
    ctx->region_alloca = NULL;
    ctx->region_owner_function = NULL;
    ctx->region_scope_id = 0;
    ctx->region_scope_active = false;
}

#endif /* PGY_LLVM_ENABLED */
