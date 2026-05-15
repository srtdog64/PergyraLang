#ifdef PGY_LLVM_ENABLED
#include "llvm_domain_world_sync_internal.h"
#include "parser/ast_api.h"

static bool
llvm_world_sync_directive_field_name(char *out,
                                     size_t out_size,
                                     const char *kind,
                                     const char *slot_name)
{
    int written;

    if (out == NULL || out_size == 0 || kind == NULL || slot_name == NULL)
        return false;
    written = snprintf(out, out_size, "__%s_%s", kind, slot_name);
    return written >= 0 && (size_t)written < out_size;
}

ASTNode *
llvm_world_sync_find_state_decl(ASTNode *world_decl, const char *state_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    size_t state_count = 0;
    ASTNode **states = ast_world_states(world_decl, &state_count);
    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && ast_world_state_name(state) != NULL
            && strcmp(ast_world_state_name(state), state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

bool
llvm_world_sync_has_zone_slot(ASTNode *world_decl, const char *slot_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || slot_name == NULL)
        return false;

    size_t zone_count = 0;
    ASTNode **zones = ast_world_zones(world_decl, &zone_count);
    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *zone = zones[i];
        const char *zone_slot_name = ast_world_zone_slot_name(zone);
        if (zone_slot_name != NULL && strcmp(zone_slot_name, slot_name) == 0) {
            return true;
        }
    }

    return false;
}

static const char *
llvm_world_sync_resolve_zone_slot(ASTNode *stmt,
                                  const char *slot_name,
                                  const char *state_name)
{
    if (slot_name != NULL)
        return slot_name;
    if (state_name != NULL) {
        ASTNode *state = llvm_world_sync_find_state_decl(stmt, state_name);
        if (state != NULL)
            return ast_world_state_zone_slot_name(state);
        if (llvm_world_sync_has_zone_slot(stmt, state_name))
            return state_name;
    }
    return NULL;
}

static void
llvm_world_sync_store_zone_active(LLVMClassTypeEntry *decl_cls,
                                  LLVMValueRef sync_fn,
                                  LLVMGenCtx *ctx,
                                  const char *slot_name,
                                  LLVMValueRef active_val,
                                  int cause)
{
    char active_field[256];
    int active_idx;
    LLVMValueRef self_ptr;
    LLVMValueRef active_ptr;

    if (slot_name == NULL)
        return;

    if (!llvm_world_sync_directive_field_name(active_field,
            sizeof(active_field), "zone_active", slot_name))
        return;
    active_idx = llvm_class_field_index(decl_cls, active_field);
    if (active_idx < 0)
        return;

    self_ptr = LLVMGetParam(sync_fn, 0);
    active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
        self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, active_val, active_ptr);
    llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "zone", slot_name, cause);
}

void
llvm_world_sync_emit_directives(ASTNode *stmt,
                                LLVMClassTypeEntry *decl_cls,
                                LLVMValueRef sync_fn,
                                LLVMGenCtx *ctx)
{
    size_t activate_count = 0;
    ASTNode **activations = ast_world_activations(stmt, &activate_count);
    for (size_t i = 0; i < activate_count; i++) {
        ASTNode *act = activations[i];
        const char *slot_name = act != NULL
            ? llvm_world_sync_resolve_zone_slot(stmt,
                ast_world_directive_zone_slot_name(act),
                ast_world_directive_state_name(act))
            : NULL;
        llvm_world_sync_store_zone_active(decl_cls, sync_fn, ctx, slot_name,
            LLVMConstInt(ctx->type_i1, 1, 0), PGY_PROP_CAUSE_WORLD_ACTIVATE);
    }

    size_t maintained_zone_count = 0;
    ASTNode **maintained_zones =
        ast_world_maintained_zones(stmt, &maintained_zone_count);
    for (size_t i = 0; i < maintained_zone_count; i++) {
        ASTNode *mnt = maintained_zones[i];
        const char *slot_name = mnt != NULL
            ? llvm_world_sync_resolve_zone_slot(stmt,
                ast_world_directive_zone_slot_name(mnt),
                ast_world_directive_state_name(mnt))
            : NULL;
        llvm_world_sync_store_zone_active(decl_cls, sync_fn, ctx, slot_name,
            LLVMConstInt(ctx->type_i1, 1, 0), PGY_PROP_CAUSE_WORLD_MAINTAIN);
    }

    size_t deactivate_count = 0;
    ASTNode **deactivations = ast_world_deactivations(stmt, &deactivate_count);
    for (size_t i = 0; i < deactivate_count; i++) {
        ASTNode *act = deactivations[i];
        const char *slot_name = act != NULL
            ? llvm_world_sync_resolve_zone_slot(stmt,
                ast_world_directive_zone_slot_name(act),
                ast_world_directive_state_name(act))
            : NULL;
        llvm_world_sync_store_zone_active(decl_cls, sync_fn, ctx, slot_name,
            LLVMConstInt(ctx->type_i1, 0, 0), PGY_PROP_CAUSE_WORLD_DEACTIVATE);
    }
}

#endif /* PGY_LLVM_ENABLED */
