/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM backend active MIR/DIR inventory accessors.
 */

#include <string.h>

#include "llvm_internal.h"

void
llvm_active_nominal_inventory(const LLVMGenCtx *ctx,
                              ASTNode ***nodes_out,
                              size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL) {
        nodes = ctx->mir->types;
        count = ctx->mir->type_count;
    }

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

void
llvm_active_routine_inventory(const LLVMGenCtx *ctx,
                              LLVMMIRRoutineInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->routines = NULL;
    inventory->count = 0;

    if (ctx == NULL || ctx->mir == NULL)
        return;

    inventory->routines = ctx->mir->routines;
    inventory->count = ctx->mir->routine_count;
}

void
llvm_mir_routine_inventory_from_program(const MIRProgram *mir,
                                        LLVMMIRRoutineInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->routines = NULL;
    inventory->count = 0;

    if (mir == NULL)
        return;

    inventory->routines = mir->routines;
    inventory->count = mir->routine_count;
}

const MIRRoutine *
llvm_routine_inventory_get(const LLVMMIRRoutineInventory *inventory,
                           size_t index)
{
    if (inventory == NULL || inventory->routines == NULL
        || index >= inventory->count) {
        return NULL;
    }
    return &inventory->routines[index];
}

void
llvm_active_domain_inventory(const LLVMGenCtx *ctx,
                             LLVMDomainInventory *inventory)
{
    if (inventory == NULL)
        return;
    memset(inventory, 0, sizeof(*inventory));
    llvm_active_inventory(ctx, AST_ABILITY_DECL,
        &inventory->abilities, &inventory->ability_count);
    llvm_active_inventory(ctx, AST_RELATION_DECL,
        &inventory->relations, &inventory->relation_count);
    llvm_active_inventory(ctx, AST_EFFECT_DECL,
        &inventory->effects, &inventory->effect_count);
    llvm_active_inventory(ctx, AST_ZONE_DECL,
        &inventory->zones, &inventory->zone_count);
    llvm_active_inventory(ctx, AST_WORLD_DECL,
        &inventory->worlds, &inventory->world_count);
    llvm_active_inventory(ctx, AST_PARTY_DECL,
        &inventory->parties, &inventory->party_count);
    llvm_active_inventory(ctx, AST_ROSTER_DECL,
        &inventory->rosters, &inventory->roster_count);
    llvm_active_inventory(ctx, AST_ROLE_DECL,
        &inventory->roles, &inventory->role_count);
    llvm_active_inventory(ctx, AST_EVENT_DECL,
        &inventory->events, &inventory->event_count);
}

void
llvm_active_executables(const LLVMGenCtx *ctx,
                        ASTNode ***nodes_out,
                        size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    /* MIR-only: top-level exec is represented by __pgy_top_level_exec. */
    (void)ctx;

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

void
llvm_active_externs(const LLVMGenCtx *ctx,
                    ASTNode ***nodes_out,
                    size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL)
        mir_active_externs(ctx->mir, &nodes, &count);

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

ASTNode *
llvm_active_synthetic_executable_func(const LLVMGenCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return mir_find_function_decl(ctx->mir, "__pgy_top_level_exec");
    return NULL;
}

bool
llvm_active_has_main_function(const LLVMGenCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return ctx->mir->has_main_function;
    return false;
}

bool
llvm_active_has_top_level_exec(const LLVMGenCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return ctx->mir->has_top_level_exec;
    return false;
}
