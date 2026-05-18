/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend pipeline helpers split from llvm_backend.c.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

static void
llvm_pipeline_debug_stage(const char *stage)
{
    if (stage != NULL && getenv("PGY_DEBUG_LLVM_STAGE") != NULL)
        fprintf(stderr, "[llvm stage] %s\n", stage);
}

bool
llvm_emit_program_from_mir(const MIRProgram *mir, LLVMGenCtx *ctx)
{
    LLVMMIRRoutineInventory routine_inventory;

    if (mir == NULL || ctx == NULL)
        return false;
    llvm_active_routine_inventory(ctx, &routine_inventory);

    /* MIR-only backend entry:
     *
     * function/intent/method bodies lower from MIR routines,
     * and declaration / top-level orchestration state is read from
     * MIR-carried inventory rather than the original HIR program.
     *
     * Remaining debt is not "original HIR dependency" anymore; it is
     * that declaration inventory is still AST-carried inside MIRProgram
     * instead of a dedicated declaration IR.
     */
    llvm_pipeline_debug_stage("emit_program_from_mir:declare_runtime");
    llvm_declare_runtime(ctx);

    llvm_pipeline_debug_stage("emit_program_from_mir:register_decl_items");
    llvm_register_active_nominal_types(ctx);
    if (ctx->has_error)
        return false;

    llvm_pipeline_debug_stage("emit_program_from_mir:emit_domain_passes");
    llvm_emit_domain_passes(ctx);

    llvm_pipeline_debug_stage("emit_program_from_mir:forward_declare_funcs");
    if (!llvm_forward_declare_function_routines_from_inventory(
            ctx, &routine_inventory))
        return false;

    llvm_pipeline_debug_stage("emit_program_from_mir:forward_declare_intents");
    llvm_forward_declare_intent_routines_from_inventory(
        ctx, &routine_inventory);
    if (ctx->has_error)
        return false;

    llvm_pipeline_debug_stage("emit_program_from_mir:emit_function_routines");
    if (!llvm_emit_function_routines_from_inventory(ctx, &routine_inventory))
        return false;

    llvm_pipeline_debug_stage("emit_program_from_mir:emit_residual_decls");
    if (!llvm_validate_function_routine_bodies_from_inventory(
            ctx, &routine_inventory))
        return false;
    llvm_emit_intent_routines_from_inventory(ctx, &routine_inventory);
    if (ctx->has_error)
        return false;

    if (!llvm_emit_class_method_bodies_from_inventory(ctx))
        return false;

    llvm_pipeline_debug_stage("emit_program_from_mir:emit_main_wrapper");
    llvm_emit_main_wrapper(ctx);
    llvm_pipeline_debug_stage("emit_program_from_mir:end");
    return !ctx->has_error;
}

#endif
