/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM backend active MIR/DIR inventory accessors.
 *
 * Like every other llvm_*.c TU, this file is conditional on
 * PGY_LLVM_ENABLED. Without the guard, ci-macos-c-only (and any
 * other LLVM_ENABLED=0 build) tries to compile against LLVMGenCtx /
 * LLVMMIRDeclHeaderInventory which only exist under
 * llvm_internal.h's own PGY_LLVM_ENABLED block, and the build
 * fails at self-host-preparation-test-smoke with "unknown type
 * name 'LLVMGenCtx'". The other llvm_mir_*.c TUs already wear
 * this guard for the same reason.
 */

#ifdef PGY_LLVM_ENABLED

#include <string.h>

#include "llvm_internal.h"
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

const MIRRoutine *
llvm_active_function_routine_by_name(const LLVMGenCtx *ctx,
                                     const char *target)
{
    LLVMMIRRoutineInventory inventory;
    size_t name_len;

    if (ctx == NULL || target == NULL) {
        return NULL;
    }

    name_len = strlen(target);
    llvm_active_routine_inventory(ctx, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine = llvm_routine_inventory_get(&inventory, i);
        const char *routine_name = llvm_mir_routine_name(routine);
        if (routine == NULL
            || llvm_mir_routine_kind(routine) != MIR_SCOPE_FUNCTION
            || routine_name == NULL) {
            continue;
        }
        if (strcmp(routine_name, target) == 0)
            return routine;
    }
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine = llvm_routine_inventory_get(&inventory, i);
        const char *routine_name = llvm_mir_routine_name(routine);
        if (routine == NULL
            || llvm_mir_routine_kind(routine) != MIR_SCOPE_FUNCTION
            || routine_name == NULL) {
            continue;
        }
        if (strncmp(routine_name, target, name_len) == 0
            && (routine_name[name_len] == '_'
                || routine_name[name_len] == '\0')) {
            return routine;
        }
    }
    return NULL;
}

void
llvm_active_decl_header_inventory(
    const LLVMGenCtx *ctx,
    LLVMMIRDeclHeaderInventory *inventory)
{
    MIRDeclHeaderInventory mir_inventory;

    if (inventory == NULL)
        return;
    inventory->headers = NULL;
    inventory->count = 0;
    if (ctx == NULL || ctx->mir == NULL)
        return;

    mir_decl_header_inventory_from_program(ctx->mir, &mir_inventory);
    inventory->headers = mir_inventory.headers;
    inventory->count = mir_inventory.count;
}

const MIRDeclHeader *
llvm_decl_header_inventory_get(
    const LLVMMIRDeclHeaderInventory *inventory,
    size_t index)
{
    if (inventory == NULL || inventory->headers == NULL
        || index >= inventory->count) {
        return NULL;
    }
    return &inventory->headers[index];
}

MIRScopeKind
llvm_mir_routine_kind(const MIRRoutine *routine)
{
    return mir_routine_kind(routine);
}

const char *
llvm_mir_routine_name(const MIRRoutine *routine)
{
    return mir_routine_name(routine);
}

const char *
llvm_mir_routine_owner_name(const MIRRoutine *routine)
{
    return mir_routine_owner_name(routine);
}

ASTNodeType
llvm_mir_routine_owner_ast_type(const MIRRoutine *routine)
{
    return mir_routine_owner_ast_type(routine);
}

bool
llvm_mir_routine_has_signature(const MIRRoutine *routine)
{
    return mir_routine_has_signature(routine);
}

size_t
llvm_mir_routine_generic_param_count(const MIRRoutine *routine)
{
    return mir_routine_generic_param_count(routine);
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

const MIRCallableSig *
llvm_mir_routine_param_callable_sig(const MIRRoutine *routine, size_t index)
{
    return mir_routine_param_callable_sig(routine, index);
}

MIRParamCarriage
llvm_mir_routine_param_carriage(const MIRRoutine *routine, size_t index)
{
    return mir_routine_param_carriage(routine, index);
}

MIRParamResourceKind
llvm_mir_routine_param_resource_kind(const MIRRoutine *routine, size_t index)
{
    return mir_routine_param_resource_kind(routine, index);
}

bool
llvm_mir_routine_param_passes_indirect(const MIRRoutine *routine,
                                       size_t index)
{
    return mir_routine_param_passes_indirect(routine, index);
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

const MIRCallableSig *
llvm_mir_routine_return_callable_sig(const MIRRoutine *routine)
{
    return mir_routine_return_callable_sig(routine);
}

const char *
llvm_mir_routine_within_zone(const MIRRoutine *routine)
{
    return mir_routine_within_zone(routine);
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

bool
llvm_active_has_mir(const LLVMGenCtx *ctx)
{
    return ctx != NULL && ctx->mir != NULL;
}

const MIRProgram *
llvm_active_mir_identity(const LLVMGenCtx *ctx)
{
    return llvm_active_has_mir(ctx) ? ctx->mir : NULL;
}

const char *
llvm_active_source_path(const LLVMGenCtx *ctx)
{
    return (ctx != NULL && ctx->mir != NULL) ? ctx->mir->source_path : NULL;
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
llvm_active_uses_thread_pool(const LLVMGenCtx *ctx)
{
    if (ctx == NULL || ctx->mir == NULL)
        return false;
    return pgy_mir_program_uses_thread_pool(ctx->mir);
}

#endif /* PGY_LLVM_ENABLED */
