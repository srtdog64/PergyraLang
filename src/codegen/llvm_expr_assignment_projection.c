/*
 * LLVM projection invalidation and world-embedded sync for assignments.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_assignment_projection.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"

static bool
llvm_host_projection_source_from_assignment(ASTNode *host_decl,
                                            ASTNode *target,
                                            const char **source_slot_out,
                                            const char **source_field_out)
{
    ASTNode **slots = NULL;
    size_t slot_count = 0;
    ASTNode *cursor = target;
    const char *source_field = NULL;

    if (source_slot_out != NULL)
        *source_slot_out = NULL;
    if (source_field_out != NULL)
        *source_field_out = NULL;
    if (host_decl == NULL || target == NULL)
        return false;

    switch (host_decl->type) {
    case AST_ZONE_DECL:
        slots = host_decl->data.zone_decl.slots;
        slot_count = host_decl->data.zone_decl.slot_count;
        break;
    case AST_RELATION_DECL:
        slots = host_decl->data.relation_decl.slots;
        slot_count = host_decl->data.relation_decl.slot_count;
        break;
    case AST_EFFECT_DECL:
        slots = host_decl->data.effect_decl.slots;
        slot_count = host_decl->data.effect_decl.slot_count;
        break;
    default:
        return false;
    }

    if (target->type == AST_IDENTIFIER && target->data.identifier.name != NULL) {
        for (size_t i = 0; i < slot_count; i++) {
            ASTNode *slot = slots[i];
            if (slot != NULL && slot->type == AST_DOMAIN_SLOT
                && slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name,
                          target->data.identifier.name) == 0) {
                if (source_slot_out != NULL)
                    *source_slot_out = target->data.identifier.name;
                return true;
            }
        }
    }

    while (cursor != NULL && cursor->type == AST_MEMBER_ACCESS) {
        ASTNode *obj = cursor->data.member.object;
        if (source_field == NULL)
            source_field = cursor->data.member.name;
        if (obj != NULL && obj->type == AST_IDENTIFIER
            && obj->data.identifier.name != NULL) {
            for (size_t i = 0; i < slot_count; i++) {
                ASTNode *slot = slots[i];
                if (slot != NULL && slot->type == AST_DOMAIN_SLOT
                    && slot->data.domain_slot.slot_name != NULL
                    && strcmp(slot->data.domain_slot.slot_name,
                              obj->data.identifier.name) == 0) {
                    if (source_slot_out != NULL)
                        *source_slot_out = obj->data.identifier.name;
                    if (source_field_out != NULL)
                        *source_field_out = source_field;
                    return true;
                }
            }
        }
        cursor = obj;
    }

    return false;
}

static bool
llvm_refresh_mentions_source_field(ASTNode *refresh, const char *source_field)
{
    if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
        return false;
    if (source_field == NULL)
        return true;
    if (refresh->data.zone_refresh.field_map_count == 0)
        return true;

    for (size_t i = 0; i < refresh->data.zone_refresh.field_map_count; i++) {
        const char *mapped_source =
            refresh->data.zone_refresh.mapped_source_fields[i];
        if (mapped_source != NULL && strcmp(mapped_source, source_field) == 0)
            return true;
    }
    return false;
}

static void
llvm_emit_projection_invalidations_for_host(LLVMGenCtx *ctx,
                                            ASTNode **refreshes,
                                            size_t refresh_count,
                                            LLVMClassTypeEntry *host_cls,
                                            LLVMValueRef host_ptr,
                                            const char *source_slot,
                                            const char *source_field)
{
    if (ctx == NULL || refreshes == NULL || refresh_count == 0
        || host_cls == NULL || host_ptr == NULL || source_slot == NULL) {
        return;
    }

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        const char *target_slot;
        const char *refresh_source;
        char field_name[256];
        int field_idx;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;
        target_slot = refresh->data.zone_refresh.object_slot_name;
        refresh_source = refresh->data.zone_refresh.source_slot_name;
        if (target_slot == NULL || refresh_source == NULL
            || strcmp(refresh_source, source_slot) != 0
            || !llvm_refresh_mentions_source_field(refresh, source_field)) {
            continue;
        }

        snprintf(field_name, sizeof(field_name), "__projection_dirty_%s",
            target_slot);
        field_idx = llvm_class_field_index(host_cls, field_name);
        if (field_idx >= 0) {
            LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                host_cls->struct_type, host_ptr, (unsigned)field_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                dirty_ptr);
        }

        snprintf(field_name, sizeof(field_name), "__projection_ready_%s",
            target_slot);
        field_idx = llvm_class_field_index(host_cls, field_name);
        if (field_idx >= 0) {
            LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                host_cls->struct_type, host_ptr, (unsigned)field_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0),
                ready_ptr);
        }
    }
}

bool
llvm_world_embedded_projection_source_from_assignment(LLVMGenCtx *ctx,
                                                      ASTNode *target,
                                                      const char **zone_slot_out,
                                                      ASTNode **zone_decl_out,
                                                      const char **source_slot_out,
                                                      const char **source_field_out)
{
    ASTNode *world_decl;
    ASTNode *cursor = target;
    const char *source_field = NULL;

    if (zone_slot_out != NULL)
        *zone_slot_out = NULL;
    if (zone_decl_out != NULL)
        *zone_decl_out = NULL;
    if (source_slot_out != NULL)
        *source_slot_out = NULL;
    if (source_field_out != NULL)
        *source_field_out = NULL;

    if (ctx == NULL || target == NULL)
        return false;

    world_decl = llvm_current_host_decl(ctx);
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return false;

    while (cursor != NULL && cursor->type == AST_MEMBER_ACCESS) {
        ASTNode *receiver = cursor->data.member.object;
        const char *slot_name = cursor->data.member.name;
        const char *zone_slot_name = NULL;
        ASTNode *zone_decl = NULL;

        if (receiver != NULL && slot_name != NULL) {
            if (receiver->type == AST_IDENTIFIER
                && receiver->data.identifier.name != NULL) {
                zone_slot_name = receiver->data.identifier.name;
            } else if (receiver->type == AST_MEMBER_ACCESS
                       && receiver->data.member.object != NULL
                       && receiver->data.member.object->type == AST_IDENTIFIER
                       && receiver->data.member.object->data.identifier.name != NULL
                       && strcmp(receiver->data.member.object->data.identifier.name,
                           "self") == 0
                       && receiver->data.member.name != NULL) {
                zone_slot_name = receiver->data.member.name;
            }

            if (zone_slot_name != NULL) {
                zone_decl = llvm_resolve_world_zone_decl(ctx, world_decl,
                    zone_slot_name);
                if (zone_decl != NULL
                    && llvm_find_zone_domain_slot_decl(zone_decl,
                        slot_name) != NULL) {
                    if (zone_slot_out != NULL)
                        *zone_slot_out = zone_slot_name;
                    if (zone_decl_out != NULL)
                        *zone_decl_out = zone_decl;
                    if (source_slot_out != NULL)
                        *source_slot_out = slot_name;
                    if (source_field_out != NULL)
                        *source_field_out = source_field;
                    return true;
                }
            }
        }

        source_field = cursor->data.member.name;
        cursor = cursor->data.member.object;
    }

    return false;
}

void
llvm_emit_host_projection_invalidations(LLVMGenCtx *ctx, ASTNode *target)
{
    ASTNode *host_decl;
    ASTNode **refreshes = NULL;
    size_t refresh_count = 0;
    const char *source_slot = NULL;
    const char *source_field = NULL;
    LLVMClassTypeEntry *host_cls;
    LLVMVarEntry *self_var;
    LLVMValueRef host_ptr;

    if (ctx == NULL || target == NULL)
        return;

    host_decl = llvm_current_host_decl(ctx);
    if (host_decl == NULL)
        return;

    switch (host_decl->type) {
    case AST_ZONE_DECL:
        refreshes = host_decl->data.zone_decl.refreshes;
        refresh_count = host_decl->data.zone_decl.refresh_count;
        break;
    case AST_RELATION_DECL:
        refreshes = host_decl->data.relation_decl.refreshes;
        refresh_count = host_decl->data.relation_decl.refresh_count;
        break;
    case AST_EFFECT_DECL:
        refreshes = host_decl->data.effect_decl.refreshes;
        refresh_count = host_decl->data.effect_decl.refresh_count;
        break;
    case AST_WORLD_DECL: {
        const char *zone_slot = NULL;
        ASTNode *zone_decl = NULL;
        LLVMClassTypeEntry *world_cls;
        ASTNode **zone_refreshes;
        size_t zone_refresh_count;
        int zone_field_idx;

        if (!llvm_world_embedded_projection_source_from_assignment(ctx, target,
                &zone_slot, &zone_decl, &source_slot, &source_field)
            || zone_slot == NULL
            || zone_decl == NULL
            || source_slot == NULL) {
            return;
        }

        world_cls = llvm_lookup_class(ctx, host_decl->data.world_decl.name);
        self_var = llvm_scope_lookup(ctx, "self");
        if (world_cls == NULL || self_var == NULL)
            return;

        host_cls = llvm_lookup_class(ctx, zone_decl->data.zone_decl.name);
        zone_refreshes = zone_decl->data.zone_decl.refreshes;
        zone_refresh_count = zone_decl->data.zone_decl.refresh_count;
        if (host_cls == NULL || zone_refreshes == NULL || zone_refresh_count == 0)
            return;

        host_ptr = self_var->alloca;
        if (self_var->type == LLVMPointerType(world_cls->struct_type, 0)) {
            host_ptr = LLVMBuildLoad2(ctx->builder, self_var->type,
                self_var->alloca, llvm_tmp_name(ctx));
        }

        zone_field_idx = llvm_class_field_index(world_cls, zone_slot);
        if (zone_field_idx < 0)
            return;

        host_ptr = LLVMBuildStructGEP2(ctx->builder, world_cls->struct_type,
            host_ptr, (unsigned)zone_field_idx, llvm_tmp_name(ctx));
        llvm_emit_projection_invalidations_for_host(ctx,
            zone_refreshes, zone_refresh_count, host_cls, host_ptr,
            source_slot, source_field);
        return;
    }
    default:
        return;
    }

    if (!llvm_host_projection_source_from_assignment(host_decl, target,
            &source_slot, &source_field)
        || source_slot == NULL
        || refreshes == NULL
        || refresh_count == 0) {
        return;
    }

    host_cls = llvm_lookup_class(ctx, llvm_decl_node_name(host_decl));
    self_var = llvm_scope_lookup(ctx, "self");
    if (host_cls == NULL || self_var == NULL)
        return;

    host_ptr = self_var->alloca;
    if (self_var->type == LLVMPointerType(host_cls->struct_type, 0)) {
        host_ptr = LLVMBuildLoad2(ctx->builder, self_var->type,
            self_var->alloca, llvm_tmp_name(ctx));
    }
    llvm_emit_projection_invalidations_for_host(ctx,
        refreshes, refresh_count, host_cls, host_ptr, source_slot,
        source_field);
}

void
llvm_emit_world_embedded_assignment_sync(LLVMGenCtx *ctx, ASTNode *target)
{
    const char *zone_slot = NULL;
    ASTNode *zone_decl = NULL;
    const char *source_slot = NULL;
    ASTNode *world_decl;
    LLVMClassTypeEntry *world_cls;
    LLVMClassTypeEntry *zone_cls;
    LLVMVarEntry *self_var;
    LLVMValueRef world_ptr;
    LLVMValueRef zone_ptr;
    LLVMFuncEntry *sync_entry;
    int zone_field_idx;

    if (ctx == NULL || target == NULL)
        return;

    if (!llvm_world_embedded_projection_source_from_assignment(ctx, target,
            &zone_slot, &zone_decl, &source_slot, NULL)
        || zone_slot == NULL
        || zone_decl == NULL
        || source_slot == NULL) {
        return;
    }

    world_decl = llvm_current_host_decl(ctx);
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return;

    world_cls = llvm_lookup_class(ctx, world_decl->data.world_decl.name);
    zone_cls = llvm_lookup_class(ctx, zone_decl->data.zone_decl.name);
    self_var = llvm_scope_lookup(ctx, "self");
    if (world_cls == NULL || zone_cls == NULL || self_var == NULL
        || zone_cls->sync_function_name == NULL) {
        return;
    }

    sync_entry = llvm_lookup_function(ctx, zone_cls->sync_function_name);
    if (sync_entry == NULL || sync_entry->fn == NULL
        || sync_entry->fn_type == NULL) {
        return;
    }

    world_ptr = self_var->alloca;
    if (self_var->type == LLVMPointerType(world_cls->struct_type, 0)) {
        world_ptr = LLVMBuildLoad2(ctx->builder, self_var->type,
            self_var->alloca, llvm_tmp_name(ctx));
    }

    zone_field_idx = llvm_class_field_index(world_cls, zone_slot);
    if (zone_field_idx < 0)
        return;

    zone_ptr = LLVMBuildStructGEP2(ctx->builder, world_cls->struct_type,
        world_ptr, (unsigned)zone_field_idx, llvm_tmp_name(ctx));
    {
        LLVMValueRef args[] = { zone_ptr };
        LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
            args, 1, "");
    }
}

#endif
