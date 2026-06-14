/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend active MIR inventory view.
 */

#include "transpiler.h"
#include "host_decl_compat.h"
#include "intent_observability_usage.h"
#include "thread_pool_usage.h"

void
transpiler_active_routine_inventory(const TranspilerCtx *ctx,
                                    TranspilerMIRRoutineInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->routines = NULL;
    inventory->count = 0;
    if (ctx != NULL && ctx->mir != NULL)
        transpiler_mir_routine_inventory_from_program(ctx->mir, inventory);
}

void
transpiler_mir_routine_inventory_from_program(
    const MIRProgram *mir,
    TranspilerMIRRoutineInventory *inventory)
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
transpiler_routine_inventory_get(
    const TranspilerMIRRoutineInventory *inventory,
    size_t index)
{
    if (inventory == NULL || inventory->routines == NULL
        || index >= inventory->count) {
        return NULL;
    }
    return &inventory->routines[index];
}

MIRScopeKind
transpiler_mir_routine_kind(const MIRRoutine *routine)
{
    return mir_routine_kind(routine);
}

const char *
transpiler_mir_routine_name(const MIRRoutine *routine)
{
    return mir_routine_name(routine);
}

const char *
transpiler_mir_routine_owner_name(const MIRRoutine *routine)
{
    return mir_routine_owner_name(routine);
}

ASTNodeType
transpiler_mir_routine_owner_ast_type(const MIRRoutine *routine)
{
    return mir_routine_owner_ast_type(routine);
}

bool
transpiler_mir_routine_has_signature(const MIRRoutine *routine)
{
    return mir_routine_has_signature(routine);
}

size_t
transpiler_mir_routine_generic_param_count(const MIRRoutine *routine)
{
    return mir_routine_generic_param_count(routine);
}

size_t
transpiler_mir_routine_param_count(const MIRRoutine *routine)
{
    return mir_routine_param_count(routine);
}

FuncParam *
transpiler_mir_routine_param(const MIRRoutine *routine, size_t index)
{
    return mir_routine_param(routine, index);
}

const char *
transpiler_mir_routine_param_type_name(const MIRRoutine *routine, size_t index)
{
    return mir_routine_param_type_name(routine, index);
}

ASTNode *
transpiler_mir_routine_return_type(const MIRRoutine *routine)
{
    return mir_routine_return_type(routine);
}

const char *
transpiler_mir_routine_return_type_name(const MIRRoutine *routine)
{
    return mir_routine_return_type_name(routine);
}

const char *
transpiler_mir_routine_source_local_type_name(const MIRRoutine *routine,
                                              const char *local_name)
{
    return mir_routine_source_local_type_name(routine, local_name);
}

size_t
transpiler_mir_routine_source_local_type_count(const MIRRoutine *routine)
{
    return mir_routine_source_local_type_count(routine);
}

const char *
transpiler_mir_routine_source_local_name_at(const MIRRoutine *routine,
                                            size_t index)
{
    return mir_routine_source_local_name_at(routine, index);
}

const char *
transpiler_mir_routine_source_local_type_name_at(const MIRRoutine *routine,
                                                 size_t index)
{
    return mir_routine_source_local_type_name_at(routine, index);
}

const char *
transpiler_mir_routine_within_zone(const MIRRoutine *routine)
{
    return mir_routine_within_zone(routine);
}

size_t
transpiler_active_routine_count(const TranspilerCtx *ctx)
{
    TranspilerMIRRoutineInventory inventory;
    transpiler_active_routine_inventory(ctx, &inventory);
    return inventory.count;
}

void
transpiler_active_inventory(const TranspilerCtx *ctx,
                            ASTNodeType decl_type,
                            ASTNode ***nodes_out,
                            size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL)
        mir_active_inventory(ctx->mir, decl_type, &nodes, &count);

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

const MIRDeclHeader *
transpiler_active_decl_header_of_type(const TranspilerCtx *ctx,
                                      ASTNodeType decl_type,
                                      const char *name)
{
    if (ctx == NULL || ctx->mir == NULL || name == NULL)
        return NULL;
    return mir_find_decl_header_of_type(ctx->mir, decl_type, name);
}

const MIRDeclHeader *
transpiler_active_host_decl_header(const TranspilerCtx *ctx, const char *name)
{
    const ASTNodeType *host_types = NULL;
    size_t host_type_count = 0;

    if (ctx == NULL || ctx->mir == NULL || name == NULL)
        return NULL;

    host_types = pgy_host_decl_compat_types(&host_type_count);
    for (size_t i = 0; host_types != NULL && i < host_type_count; i++) {
        const MIRDeclHeader *header = transpiler_active_decl_header_of_type(
            ctx, host_types[i], name);
        if (header != NULL)
            return header;
    }
    return NULL;
}

void
transpiler_active_externs(const TranspilerCtx *ctx,
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

void
transpiler_active_executables(const TranspilerCtx *ctx,
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

bool
transpiler_active_has_mir(const TranspilerCtx *ctx)
{
    return ctx != NULL && ctx->mir != NULL;
}

const char *
transpiler_active_source_path(const TranspilerCtx *ctx)
{
    return (ctx != NULL && ctx->mir != NULL) ? ctx->mir->source_path : NULL;
}

const MIRProgram *
transpiler_active_mir_identity(const TranspilerCtx *ctx)
{
    return transpiler_active_has_mir(ctx) ? ctx->mir : NULL;
}

bool
transpiler_active_has_main_function(const TranspilerCtx *ctx)
{
    return ctx != NULL && mir_program_has_main_function(ctx->mir);
}

const char *
transpiler_active_main_function_name(const TranspilerCtx *ctx)
{
    if (ctx == NULL || ctx->mir == NULL)
        return NULL;
    return mir_program_main_function_name(ctx->mir);
}

bool
transpiler_active_has_top_level_exec(const TranspilerCtx *ctx)
{
    return ctx != NULL && mir_program_has_top_level_exec(ctx->mir);
}

bool
transpiler_active_uses_intent_observability(const TranspilerCtx *ctx)
{
    if (ctx == NULL || ctx->mir == NULL)
        return false;
    return pgy_mir_program_uses_intent_observability(ctx->mir);
}

bool
transpiler_active_uses_thread_pool(const TranspilerCtx *ctx)
{
    if (ctx == NULL || ctx->mir == NULL)
        return false;
    return pgy_mir_program_uses_thread_pool(ctx->mir);
}

bool
transpiler_active_can_emit_intent_cleanup_from_mir(
    const TranspilerCtx *ctx,
    const ASTNode *intent_decl)
{
    return transpiler_can_emit_intent_cleanup_from_mir_with_reason_for_test(
        intent_decl, transpiler_active_mir_identity(ctx), NULL, 0);
}
