/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend active MIR inventory view.
 */

#include "transpiler.h"
#include "../compiler/mir_decl_headers.h"
#include "host_decl_compat.h"
#include "thread_pool_usage.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    const MIRDeclHeader *header;
} TranspilerDeclHeaderLookupEntry;

struct TranspilerDeclHeaderLookupProjection
{
    const MIRProgram *mir;
    const MIRDeclHeader *headers;
    size_t header_count;
    TranspilerDeclHeaderLookupEntry *entries;
    size_t capacity;
    bool failed;
};

static uint64_t
transpiler_decl_header_lookup_hash(ASTNodeType ast_type, const char *name)
{
    uint64_t hash = UINT64_C(1469598103934665603);

    hash ^= (uint64_t)(unsigned)ast_type;
    hash *= UINT64_C(1099511628211);
    if (name == NULL)
        return hash;
    for (const unsigned char *p = (const unsigned char *)name;
         *p != '\0'; p++) {
        hash ^= (uint64_t)*p;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void
transpiler_decl_header_lookup_projection_clear(
    TranspilerDeclHeaderLookupProjection *projection)
{
    if (projection == NULL)
        return;
    free(projection->entries);
    memset(projection, 0, sizeof(*projection));
}

TranspilerDeclHeaderLookupProjection *
transpiler_decl_header_lookup_projection_create(void)
{
    return calloc(1, sizeof(TranspilerDeclHeaderLookupProjection));
}

void
transpiler_decl_header_lookup_projection_destroy(
    TranspilerDeclHeaderLookupProjection *projection)
{
    if (projection == NULL)
        return;
    transpiler_decl_header_lookup_projection_clear(projection);
    free(projection);
}

static bool
transpiler_decl_header_lookup_projection_build(
    TranspilerDeclHeaderLookupProjection *projection,
    const MIRProgram *mir)
{
    size_t capacity = 8;

    if (projection == NULL || mir == NULL)
        return false;
    transpiler_decl_header_lookup_projection_clear(projection);
    projection->mir = mir;
    projection->headers = mir->decl_headers;
    projection->header_count = mir->decl_header_count;
    if (mir->decl_header_count > SIZE_MAX / 2) {
        projection->failed = true;
        return false;
    }
    size_t needed = mir->decl_header_count * 2;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            projection->failed = true;
            return false;
        }
        capacity *= 2;
    }
    projection->entries = calloc(capacity, sizeof(*projection->entries));
    if (projection->entries == NULL) {
        projection->failed = true;
        return false;
    }
    projection->capacity = capacity;
    for (size_t i = 0; i < mir->decl_header_count; i++) {
        const MIRDeclHeader *header = &mir->decl_headers[i];
        if (header->name == NULL)
            continue;
        size_t slot = (size_t)(transpiler_decl_header_lookup_hash(
            header->ast_type, header->name) & (capacity - 1));
        size_t probe = 0;
        while (projection->entries[slot].header != NULL) {
            const MIRDeclHeader *saved = projection->entries[slot].header;
            if (saved->ast_type == header->ast_type
                && strcmp(saved->name, header->name) == 0) {
                /* The former linear lookup returned the first row. MIR
                 * validation separately rejects a duplicate authority. */
                break;
            }
            if (++probe == capacity) {
                projection->failed = true;
                return false;
            }
            slot = (slot + 1) & (capacity - 1);
        }
        if (projection->entries[slot].header == NULL)
            projection->entries[slot].header = header;
    }
    return true;
}

static const MIRDeclHeader *
transpiler_decl_header_lookup_projected(const TranspilerCtx *ctx,
                                        ASTNodeType ast_type,
                                        const char *name)
{
    TranspilerDeclHeaderLookupProjection *projection;

    if (ctx == NULL || ctx->mir == NULL || name == NULL
        || ctx->decl_header_lookup_projection == NULL) {
        return NULL;
    }
    projection = ctx->decl_header_lookup_projection;
    if (projection->mir != ctx->mir
        || projection->headers != ctx->mir->decl_headers
        || projection->header_count != ctx->mir->decl_header_count) {
        if (!transpiler_decl_header_lookup_projection_build(
                projection, ctx->mir)) {
            return NULL;
        }
    }
    if (projection->failed || projection->entries == NULL
        || projection->capacity == 0) {
        return NULL;
    }
    size_t slot = (size_t)(transpiler_decl_header_lookup_hash(ast_type, name)
        & (projection->capacity - 1));
    for (size_t probe = 0; probe < projection->capacity; probe++) {
        const MIRDeclHeader *header = projection->entries[slot].header;
        if (header == NULL)
            return NULL;
        if (header->ast_type == ast_type
            && strcmp(header->name, name) == 0) {
            return header;
        }
        slot = (slot + 1) & (projection->capacity - 1);
    }
    return NULL;
}

static bool
transpiler_decl_header_lookup_projection_verify_enabled(void)
{
    static int initialized = 0;
    static bool enabled = false;

    if (!initialized) {
        const char *value = getenv(
            "PGY_VERIFY_C_DECL_HEADER_LOOKUP_PROJECTION");
        enabled = value != NULL && strcmp(value, "1") == 0;
        initialized = 1;
    }
    return enabled;
}

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

MIRRoutineSourceLookup
transpiler_active_routine_lookup_by_source_syntax_id(
    const TranspilerCtx *ctx,
    uint32_t source_syntax_id)
{
    TranspilerMIRRoutineInventory inventory;
    MIRRoutineInventory core_inventory;
    MIRRoutineSourceLookup invalid;

    if (ctx == NULL || source_syntax_id == 0) {
        invalid.status = MIR_ROUTINE_SOURCE_LOOKUP_INVALID;
        invalid.routine = NULL;
        return invalid;
    }
    transpiler_active_routine_inventory(ctx, &inventory);
    core_inventory.routines = inventory.routines;
    core_inventory.count = inventory.count;
    return mir_routine_inventory_find_unique_by_source_syntax_id(
        &core_inventory, source_syntax_id);
}

MIRScopeKind
transpiler_mir_routine_kind(const MIRRoutine *routine)
{
    return mir_routine_kind(routine);
}

uint32_t
transpiler_mir_routine_source_syntax_id(const MIRRoutine *routine)
{
    return mir_routine_source_syntax_id(routine);
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

MIRParamCarriage
transpiler_mir_routine_param_carriage(const MIRRoutine *routine, size_t index)
{
    return mir_routine_param_carriage(routine, index);
}

MIRParamResourceKind
transpiler_mir_routine_param_resource_kind(const MIRRoutine *routine,
                                            size_t index)
{
    return mir_routine_param_resource_kind(routine, index);
}

bool
transpiler_mir_routine_param_passes_indirect(const MIRRoutine *routine,
                                             size_t index)
{
    return mir_routine_param_passes_indirect(routine, index);
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

/* Row 607: lossless callable (EventHandler) signature views. */
const MIRCallableSig *
transpiler_mir_routine_param_callable_sig(const MIRRoutine *routine,
                                          size_t index)
{
    return mir_routine_param_callable_sig(routine, index);
}

const MIRCallableSig *
transpiler_mir_routine_return_callable_sig(const MIRRoutine *routine)
{
    return mir_routine_return_callable_sig(routine);
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
transpiler_active_decl_header_inventory(
    const TranspilerCtx *ctx,
    MIRDeclHeaderInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->headers = NULL;
    inventory->count = 0;
    if (ctx != NULL && ctx->mir != NULL)
        mir_decl_header_inventory_from_program(ctx->mir, inventory);
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
    const MIRDeclHeader *projected;

    if (ctx == NULL || ctx->mir == NULL || name == NULL)
        return NULL;
    projected = transpiler_decl_header_lookup_projected(ctx, decl_type, name);
    if (transpiler_decl_header_lookup_projection_verify_enabled()
        && projected != mir_find_decl_header_of_type(
            ctx->mir, decl_type, name)) {
        return NULL;
    }
    return projected;
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
