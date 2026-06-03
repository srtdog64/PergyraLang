/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM backend active MIR/DIR inventory accessors.
 */

#include <string.h>

#include "llvm_internal.h"
#include "intent_observability_usage.h"
#include "thread_pool_usage.h"

void
llvm_active_nominal_inventory(const LLVMGenCtx *ctx,
                              ASTNode ***nodes_out,
                              size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL)
        mir_active_inventory(ctx->mir, AST_CLASS_DECL, &nodes, &count);

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

    llvm_mir_routine_inventory_from_program(ctx->mir, inventory);
}

void
llvm_mir_routine_inventory_from_program(const MIRProgram *mir,
                                        LLVMMIRRoutineInventory *inventory)
{
    MIRRoutineInventory mir_inventory;

    if (inventory == NULL)
        return;
    inventory->routines = NULL;
    inventory->count = 0;

    if (mir == NULL)
        return;

    mir_routine_inventory_from_program(mir, &mir_inventory);
    inventory->routines = mir_inventory.routines;
    inventory->count = mir_inventory.count;
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

ASTNode *
llvm_mir_routine_source_ast(const MIRRoutine *routine)
{
    return mir_routine_source_ast(routine);
}

bool
llvm_mir_routine_has_signature(const MIRRoutine *routine)
{
    return mir_routine_has_signature(routine);
}

size_t
llvm_mir_routine_param_count(const MIRRoutine *routine)
{
    return mir_routine_param_count(routine);
}

FuncParam *
llvm_mir_routine_param(const MIRRoutine *routine, size_t index)
{
    return mir_routine_param(routine, index);
}

const char *
llvm_mir_routine_param_type_name(const MIRRoutine *routine, size_t index)
{
    return mir_routine_param_type_name(routine, index);
}

ASTNode *
llvm_mir_routine_return_type(const MIRRoutine *routine)
{
    return mir_routine_return_type(routine);
}

const char *
llvm_mir_routine_return_type_name(const MIRRoutine *routine)
{
    return mir_routine_return_type_name(routine);
}

ASTNode *
llvm_mir_routine_source_ast_of_type(const MIRRoutine *routine,
                                    MIRScopeKind expected_kind,
                                    ASTNodeType expected_ast_type)
{
    ASTNode *source_ast = llvm_mir_routine_source_ast(routine);

    if (routine == NULL || routine->kind != expected_kind)
        return NULL;
    if (source_ast == NULL || source_ast->type != expected_ast_type)
        return NULL;
    return source_ast;
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
llvm_active_has_mir(const LLVMGenCtx *ctx)
{
    return ctx != NULL && ctx->mir != NULL;
}

bool
llvm_active_has_main_function(const LLVMGenCtx *ctx)
{
    return ctx != NULL && mir_program_has_main_function(ctx->mir);
}

const char *
llvm_active_main_function_name(const LLVMGenCtx *ctx)
{
    if (ctx == NULL || ctx->mir == NULL)
        return NULL;
    return mir_program_main_function_name(ctx->mir);
}

bool
llvm_active_has_top_level_exec(const LLVMGenCtx *ctx)
{
    return ctx != NULL && mir_program_has_top_level_exec(ctx->mir);
}

bool
llvm_active_uses_intent_observability(const LLVMGenCtx *ctx)
{
    if (ctx == NULL || ctx->mir == NULL)
        return false;
    return pgy_mir_program_uses_intent_observability(ctx->mir);
}

bool
llvm_active_uses_thread_pool(const LLVMGenCtx *ctx)
{
    if (ctx == NULL || ctx->mir == NULL)
        return false;
    return pgy_mir_program_uses_thread_pool(ctx->mir);
}
