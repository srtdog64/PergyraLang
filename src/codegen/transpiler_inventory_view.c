/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend active MIR inventory view.
 */

#include "transpiler.h"
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

ASTNode *
transpiler_mir_routine_source_ast(const MIRRoutine *routine)
{
    return mir_routine_source_ast(routine);
}

ASTNode *
transpiler_mir_routine_source_ast_of_type(
    const MIRRoutine *routine,
    MIRScopeKind expected_kind,
    ASTNodeType expected_ast_type)
{
    ASTNode *source_ast = transpiler_mir_routine_source_ast(routine);

    if (routine == NULL || routine->kind != expected_kind)
        return NULL;
    if (source_ast == NULL || source_ast->type != expected_ast_type)
        return NULL;
    return source_ast;
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
transpiler_active_decl_header(const TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || ctx->mir == NULL || name == NULL)
        return NULL;
    return mir_find_decl_header(ctx->mir, name);
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

ASTNode *
transpiler_active_synthetic_executable_func(const TranspilerCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return mir_find_function_decl(ctx->mir, "__pgy_top_level_exec");
    return NULL;
}

bool
transpiler_active_has_mir(const TranspilerCtx *ctx)
{
    return ctx != NULL && ctx->mir != NULL;
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
