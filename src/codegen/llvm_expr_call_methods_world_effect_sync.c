#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_methods_world_effect_sync.h"

#include <stdio.h>
#include <string.h>

#include "llvm_expr_assignment_projection.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_host_methods.h"
#include "llvm_inventory_internal.h"
#include "parser/ast_api.h"

static ASTNode *
llvm_call_find_domain_decl(LLVMGenCtx *ctx, ASTNodeType decl_type, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;

    return llvm_find_decl_in_active_inventory(ctx, decl_type, name);
}

static bool
llvm_world_effect_sync_field_name(char *out,
    size_t out_size,
    const char *kind,
    const char *name)
{
    int written;

    if (out == NULL || out_size == 0 || kind == NULL || name == NULL)
        return false;

    written = snprintf(out, out_size, "__%s_%s", kind, name);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_world_effect_sync_function_name(char *out,
    size_t out_size,
    const char *effect_name)
{
    int written;

    if (out == NULL || out_size == 0 || effect_name == NULL)
        return false;

    written = snprintf(out, out_size, "%s_sync", effect_name);
    return written >= 0 && (size_t)written < out_size;
}

static const char *
llvm_call_find_first_effect_subject_slot_name(LLVMGenCtx *ctx,
                                              ASTNode *effect_decl)
{
    const char *effect_name;
    LLVMHostedDomainSlotView slot_view;

    if (effect_decl == NULL || effect_decl->type != AST_EFFECT_DECL)
        return NULL;

    effect_name = llvm_decl_node_name(effect_decl);
    slot_view = llvm_hosted_domain_slot_view_from_decl(ctx, effect_name,
        effect_decl);
    if (llvm_hosted_domain_slot_view_missing_mir_metadata(&slot_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing effect target-slot metadata");
        return NULL;
    }

    for (size_t i = 0; i < slot_view.count; i++) {
        if (llvm_hosted_domain_slot_view_is_subject_like(&slot_view, i)) {
            return llvm_hosted_domain_slot_view_name(&slot_view, i);
        }
    }
    return NULL;
}

void
llvm_emit_world_embedded_action_effect_sync(LLVMGenCtx *ctx,
                                            ASTNode *receiver,
                                            const MIRDeclMethod *method_meta,
                                            ASTNode *method_decl)
{
    ASTNode *host_decl;
    const char *zone_slot_name = NULL;
    ASTNode *zone_decl = NULL;
    const char *source_slot_name = NULL;
    ASTNode *effect_decl;
    const char *target_slot_name;
    LLVMClassTypeEntry *world_cls;
    LLVMClassTypeEntry *zone_cls;
    LLVMClassTypeEntry *effect_cls;
    LLVMVarEntry self_var;
    LLVMValueRef world_ptr;
    LLVMValueRef zone_ptr;
    LLVMHostedZoneLayerSlotView layer_view;
    LLVMHostedZoneRefreshView refresh_view;
    size_t state_count = 0;
    ASTNode **states = NULL;
    const char *effect_name;
    const char *method_within_zone;
    const char *world_name;
    const char *zone_name;
    bool method_is_async;
    bool method_is_action;

    if (ctx == NULL || receiver == NULL)
        return;

    method_is_async = llvm_mir_decl_method_is_async(method_meta);
    method_is_action = llvm_mir_decl_method_is_action_like(method_meta);
    method_within_zone = llvm_mir_decl_method_within_zone(method_meta);
    effect_name = llvm_mir_decl_method_causes_effect(method_meta);
    if (method_meta == NULL) {
        if (llvm_active_has_mir(ctx)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing world effect sync method metadata");
            return;
        }
        method_is_async = method_decl != NULL && method_decl->is_async_decl;
        method_is_action = ast_func_is_action(method_decl);
        method_within_zone = ast_func_within_zone(method_decl);
        effect_name = ast_func_causes_effect(method_decl);
    }

    if (method_is_async || !method_is_action
        || method_within_zone == NULL || effect_name == NULL) {
        return;
    }

    host_decl = llvm_current_host_decl(ctx);
    if (host_decl == NULL || host_decl->type != AST_WORLD_DECL)
        return;

    if (!llvm_world_embedded_projection_source_from_assignment(ctx, receiver,
            &zone_slot_name, &zone_decl, &source_slot_name, NULL)
        || zone_slot_name == NULL || zone_decl == NULL || source_slot_name == NULL
        || (zone_name = llvm_decl_node_name(zone_decl)) == NULL
        || strcmp(method_within_zone, zone_name) != 0) {
        return;
    }

    effect_decl = llvm_call_find_domain_decl(ctx, AST_EFFECT_DECL, effect_name);
    target_slot_name = llvm_call_find_first_effect_subject_slot_name(ctx,
        effect_decl);
    world_name = llvm_decl_node_name(host_decl);
    if (world_name == NULL)
        return;
    world_cls = llvm_lookup_class(ctx, world_name);
    zone_cls = llvm_lookup_class(ctx, zone_name);
    effect_cls = llvm_lookup_class(ctx, effect_name);
    if (effect_decl == NULL || target_slot_name == NULL || world_cls == NULL
        || zone_cls == NULL || effect_cls == NULL
        || !llvm_scope_lookup_snapshot(ctx, "self", &self_var)) {
        return;
    }

    world_ptr = self_var.alloca;
    if (self_var.type == LLVMPointerType(world_cls->struct_type, 0)) {
        world_ptr = LLVMBuildLoad2(ctx->builder, self_var.type,
            self_var.alloca, llvm_tmp_name(ctx));
    }
    {
        int zone_idx = llvm_class_field_index(world_cls, zone_slot_name);
        if (zone_idx < 0)
            return;
        zone_ptr = LLVMBuildStructGEP2(ctx->builder, world_cls->struct_type,
            world_ptr, (unsigned)zone_idx, llvm_tmp_name(ctx));
    }

    layer_view = llvm_hosted_zone_layer_slot_view_from_decl(ctx, zone_name, zone_decl);
    if (llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing zone layer-slot metadata for world effect sync");
        return;
    }
    states = ast_zone_states(zone_decl, &state_count);
    refresh_view = llvm_hosted_zone_refresh_view_from_decl(ctx, effect_name,
        effect_decl);
    if (llvm_hosted_zone_refresh_view_missing_mir_metadata(&refresh_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing world effect sync refresh metadata for '%s'",
            effect_name != NULL ? effect_name : "(anonymous-effect)");
        return;
    }

    for (size_t i = 0; i < layer_view.count; i++) {
        const char *layer_name;
        const char *layer_type;
        int layer_capacity;
        int active_idx;
        int layer_idx;
        int source_idx;
        int target_idx;
        LLVMValueRef layer_ptr;
        LLVMValueRef source_ptr;
        LLVMValueRef source_value;

        layer_name = llvm_hosted_zone_layer_slot_view_name(&layer_view, i);
        layer_type = llvm_hosted_zone_layer_slot_view_type_name(&layer_view, i);
        if (llvm_hosted_zone_layer_slot_view_is_relation(&layer_view, i)
            || layer_type == NULL
            || strcmp(layer_type, effect_name) != 0) {
            continue;
        }
        layer_capacity =
            llvm_hosted_zone_layer_slot_view_pool_capacity(&layer_view, i);
        if (layer_capacity <= 0)
            layer_capacity = 1;

        if (layer_name == NULL)
            continue;

        {
            char active_field[256];
            if (!llvm_world_effect_sync_field_name(active_field,
                    sizeof(active_field), "layer_active", layer_name)) {
                llvm_set_error(ctx, "layer active field name is too long");
                return;
            }
            active_idx = llvm_class_field_index(zone_cls, active_field);
        }
        if (active_idx >= 0) {
            LLVMValueRef active_ptr = LLVMBuildStructGEP2(ctx->builder,
                zone_cls->struct_type, zone_ptr, (unsigned)active_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), active_ptr);
        }
        {
            char epoch_field[256];
            char cause_field[256];
            int epoch_idx;
            int cause_idx;
            if (!llvm_world_effect_sync_field_name(epoch_field,
                    sizeof(epoch_field), "layer_epoch", layer_name)) {
                llvm_set_error(ctx, "layer epoch field name is too long");
                return;
            }
            if (!llvm_world_effect_sync_field_name(cause_field,
                    sizeof(cause_field), "layer_cause", layer_name)) {
                llvm_set_error(ctx, "layer cause field name is too long");
                return;
            }
            epoch_idx = llvm_class_field_index(zone_cls, epoch_field);
            cause_idx = llvm_class_field_index(zone_cls, cause_field);
            if (epoch_idx >= 0) {
                LLVMValueRef epoch_ptr = LLVMBuildStructGEP2(ctx->builder,
                    zone_cls->struct_type, zone_ptr, (unsigned)epoch_idx,
                    llvm_tmp_name(ctx));
                LLVMValueRef epoch_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                    epoch_ptr, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMBuildAdd(ctx->builder, epoch_val,
                        LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx)),
                    epoch_ptr);
            }
            if (cause_idx >= 0) {
                LLVMValueRef cause_ptr = LLVMBuildStructGEP2(ctx->builder,
                    zone_cls->struct_type, zone_ptr, (unsigned)cause_idx,
                    llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 11, 0), cause_ptr);
            }
        }
        for (size_t si = 0; si < state_count; si++) {
            ASTNode *state = states[si];
            char state_epoch_field[256];
            char state_cause_field[256];
            int state_epoch_idx;
            int state_cause_idx;
            if (state == NULL || state->type != AST_ZONE_STATE
                || ast_zone_state_is_relation(state)
                || ast_zone_state_name(state) == NULL
                || ast_zone_state_layer_slot_name(state) == NULL
                || strcmp(ast_zone_state_layer_slot_name(state), layer_name) != 0) {
                continue;
            }
            if (!llvm_world_effect_sync_field_name(state_epoch_field,
                    sizeof(state_epoch_field), "state_epoch",
                    ast_zone_state_name(state))) {
                llvm_set_error(ctx, "state epoch field name is too long");
                return;
            }
            if (!llvm_world_effect_sync_field_name(state_cause_field,
                    sizeof(state_cause_field), "state_cause",
                    ast_zone_state_name(state))) {
                llvm_set_error(ctx, "state cause field name is too long");
                return;
            }
            state_epoch_idx = llvm_class_field_index(zone_cls, state_epoch_field);
            state_cause_idx = llvm_class_field_index(zone_cls, state_cause_field);
            if (state_epoch_idx >= 0) {
                LLVMValueRef state_epoch_ptr = LLVMBuildStructGEP2(ctx->builder,
                    zone_cls->struct_type, zone_ptr, (unsigned)state_epoch_idx,
                    llvm_tmp_name(ctx));
                LLVMValueRef state_epoch_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                    state_epoch_ptr, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMBuildAdd(ctx->builder, state_epoch_val,
                        LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx)),
                    state_epoch_ptr);
            }
            if (state_cause_idx >= 0) {
                LLVMValueRef state_cause_ptr = LLVMBuildStructGEP2(ctx->builder,
                    zone_cls->struct_type, zone_ptr, (unsigned)state_cause_idx,
                    llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 11, 0),
                    state_cause_ptr);
            }
        }

        layer_idx = llvm_class_field_index(zone_cls, layer_name);
        source_idx = llvm_class_field_index(zone_cls, source_slot_name);
        target_idx = llvm_class_field_index(effect_cls, target_slot_name);
        if (layer_idx < 0 || source_idx < 0 || target_idx < 0)
            continue;

        layer_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
            zone_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
        source_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
            zone_ptr, (unsigned)source_idx, llvm_tmp_name(ctx));
        LLVMTypeRef source_ty =
            llvm_class_field_type_at_index(zone_cls, source_idx);
        if (source_ty == NULL)
            return;
        source_value = LLVMBuildLoad2(ctx->builder,
            source_ty, source_ptr, llvm_tmp_name(ctx));

        if (llvm_hosted_zone_layer_slot_view_is_pool(&layer_view, i)) {
            LLVMValueRef tmp_effect = llvm_create_entry_alloca(ctx,
                effect_cls->struct_type, "world.effect.pool.tmp");
            LLVMValueRef subject_ptr;
            char sync_name[256];
            LLVMFuncEntry *sync_entry;
            LLVMValueRef effect_value;
            LLVMTypeRef pool_ty =
                llvm_class_field_type_at_index(zone_cls, layer_idx);
            LLVMValueRef count_ptr;
            LLVMValueRef count_val;
            LLVMValueRef has_space;
            LLVMValueRef current_fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
            LLVMBasicBlockRef insert_bb;
            LLVMBasicBlockRef skip_bb;
            LLVMBasicBlockRef cont_bb;

            if (pool_ty == NULL)
                return;

            LLVMBuildStore(ctx->builder, LLVMConstNull(effect_cls->struct_type), tmp_effect);
            subject_ptr = LLVMBuildStructGEP2(ctx->builder, effect_cls->struct_type,
                tmp_effect, (unsigned)target_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, source_value, subject_ptr);
            if (!llvm_world_effect_sync_function_name(sync_name,
                    sizeof(sync_name), effect_name)) {
                llvm_set_error(ctx, "effect sync function name is too long");
                return;
            }
            sync_entry = llvm_lookup_function(ctx, sync_name);
            if (sync_entry != NULL) {
                LLVMValueRef sync_args[] = { tmp_effect };
                LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                    sync_args, 1, "");
            }
            effect_value = LLVMBuildLoad2(ctx->builder, effect_cls->struct_type,
                tmp_effect, llvm_tmp_name(ctx));
            count_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, layer_ptr, 2,
                llvm_tmp_name(ctx));
            count_val = LLVMBuildLoad2(ctx->builder, LLVMInt8TypeInContext(ctx->context),
                count_ptr, llvm_tmp_name(ctx));
            has_space = LLVMBuildICmp(ctx->builder, LLVMIntULT, count_val,
                LLVMConstInt(LLVMInt8TypeInContext(ctx->context),
                    (unsigned)layer_capacity,
                    0),
                llvm_tmp_name(ctx));
            insert_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
                "world.effect.pool.insert");
            skip_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
                "world.effect.pool.skip");
            cont_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
                "world.effect.pool.cont");
            LLVMBuildCondBr(ctx->builder, has_space, insert_bb, skip_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, insert_bb);
            {
                LLVMValueRef idx32 = LLVMBuildZExt(ctx->builder, count_val, ctx->type_i32,
                    llvm_tmp_name(ctx));
                LLVMValueRef items_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty,
                    layer_ptr, 0, llvm_tmp_name(ctx));
                LLVMValueRef active_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty,
                    layer_ptr, 1, llvm_tmp_name(ctx));
                LLVMTypeRef items_arr_ty = LLVMStructGetTypeAtIndex(pool_ty, 0);
                LLVMTypeRef active_arr_ty = LLVMStructGetTypeAtIndex(pool_ty, 1);
                LLVMValueRef item_ixs[] = {
                    LLVMConstInt(ctx->type_i32, 0, 0),
                    idx32
                };
                LLVMValueRef active_ixs[] = {
                    LLVMConstInt(ctx->type_i32, 0, 0),
                    idx32
                };
                LLVMValueRef item_slot = LLVMBuildInBoundsGEP2(ctx->builder,
                    items_arr_ty, items_ptr, item_ixs, 2, llvm_tmp_name(ctx));
                LLVMValueRef active_slot = LLVMBuildInBoundsGEP2(ctx->builder,
                    active_arr_ty, active_ptr, active_ixs, 2, llvm_tmp_name(ctx));
                LLVMValueRef next_count = LLVMBuildAdd(ctx->builder, count_val,
                    LLVMConstInt(LLVMInt8TypeInContext(ctx->context), 1, 0),
                    llvm_tmp_name(ctx));
                LLVMValueRef cap_ptr;
                LLVMBuildStore(ctx->builder, effect_value, item_slot);
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), active_slot);
                LLVMBuildStore(ctx->builder, next_count, count_ptr);
                cap_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, layer_ptr, 3,
                    llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt8TypeInContext(ctx->context),
                        (unsigned)layer_capacity,
                        0),
                    cap_ptr);
            }
            LLVMBuildBr(ctx->builder, cont_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, skip_bb);
            LLVMBuildBr(ctx->builder, cont_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
            continue;
        }

        {
            LLVMValueRef subject_ptr = LLVMBuildStructGEP2(ctx->builder,
                effect_cls->struct_type, layer_ptr, (unsigned)target_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, source_value, subject_ptr);
        }
        for (size_t ri = 0; ri < refresh_view.count; ri++) {
            const char *projection_name;
            const char *refresh_source;
            char dirty_field[256];
            char ready_field[256];
            int dirty_idx;
            int ready_idx;
            projection_name =
                llvm_hosted_zone_refresh_view_object_slot_name(
                    &refresh_view, ri);
            refresh_source =
                llvm_hosted_zone_refresh_view_source_slot_name(
                    &refresh_view, ri);
            if (projection_name == NULL || refresh_source == NULL
                || strcmp(refresh_source, target_slot_name) != 0) {
                continue;
            }
            if (!llvm_world_effect_sync_field_name(dirty_field,
                    sizeof(dirty_field), "projection_dirty",
                    projection_name)) {
                llvm_set_error(ctx, "projection dirty field name is too long");
                return;
            }
            if (!llvm_world_effect_sync_field_name(ready_field,
                    sizeof(ready_field), "projection_ready",
                    projection_name)) {
                llvm_set_error(ctx, "projection ready field name is too long");
                return;
            }
            dirty_idx = llvm_class_field_index(effect_cls, dirty_field);
            ready_idx = llvm_class_field_index(effect_cls, ready_field);
            if (dirty_idx >= 0) {
                LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                    effect_cls->struct_type, layer_ptr, (unsigned)dirty_idx,
                    llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), dirty_ptr);
            }
            if (ready_idx >= 0) {
                LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                    effect_cls->struct_type, layer_ptr, (unsigned)ready_idx,
                    llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), ready_ptr);
            }
        }
        {
            char sync_name[256];
            LLVMFuncEntry *sync_entry;
            if (!llvm_world_effect_sync_function_name(sync_name,
                    sizeof(sync_name), effect_name)) {
                llvm_set_error(ctx, "effect sync function name is too long");
                return;
            }
            sync_entry = llvm_lookup_function(ctx, sync_name);
            if (sync_entry != NULL) {
                LLVMValueRef sync_args[] = { layer_ptr };
                LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                    sync_args, 1, "");
            }
        }
    }
}

#endif
