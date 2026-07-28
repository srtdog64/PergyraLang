/*
 * LLVM zone bind lowering for effect/relation layers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_zone_bind_lowering.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_domain_runtime_facts.h"
#include "llvm_inventory_decl_lookup.h"
#include "parser/ast_api.h"

static bool
llvm_zone_bind_sync_name(char *out, size_t out_size, const char *name)
{
    int written;

    if (out == NULL || out_size == 0 || name == NULL)
        return false;
    written = snprintf(out, out_size, "%s_sync", name);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_zone_bind_projection_field_name(char *out,
                                     size_t out_size,
                                     const char *kind,
                                     const char *projection_name)
{
    int written;

    if (out == NULL || out_size == 0 || kind == NULL || projection_name == NULL)
        return false;
    written = snprintf(out, out_size, "__projection_%s_%s", kind,
        projection_name);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_zone_bind_mark_projection_source_dirty(
    LLVMGenCtx *ctx,
    LLVMClassTypeEntry *layer_cls,
    LLVMValueRef layer_ptr,
    const LLVMDomainRuntimeProjectionView *projection_view,
    uint32_t source_slot_syntax_id)
{
    if (ctx == NULL || layer_cls == NULL || layer_ptr == NULL
        || projection_view == NULL || !projection_view->valid
        || source_slot_syntax_id == 0) {
        return false;
    }
    for (size_t i = 0; i < projection_view->directive_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *projection =
            llvm_domain_runtime_projection_anchor(projection_view, i);
        const char *projection_name;
        char dirty_field[256];
        char ready_field[256];
        int dirty_idx;
        int ready_idx;

        if (projection == NULL || projection->projection_slot_name == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM domain binding projection runtime anchor is missing");
            return false;
        }
        if (projection->source_slot_syntax_id != source_slot_syntax_id)
            continue;
        projection_name = projection->projection_slot_name;
        if (!llvm_zone_bind_projection_field_name(dirty_field,
                sizeof(dirty_field), "dirty", projection_name)
            || !llvm_zone_bind_projection_field_name(ready_field,
                sizeof(ready_field), "ready", projection_name)) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM domain binding projection state name is invalid");
            return false;
        }
        dirty_idx = llvm_class_field_index(layer_cls, dirty_field);
        ready_idx = llvm_class_field_index(layer_cls, ready_field);
        if (dirty_idx < 0 || ready_idx < 0) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM domain binding projection state layout is incomplete");
            return false;
        }
        {
            LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                layer_cls->struct_type, layer_ptr, (unsigned)dirty_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                dirty_ptr);
        }
        {
            LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                layer_cls->struct_type, layer_ptr, (unsigned)ready_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0),
                ready_ptr);
        }
    }
    return true;
}

static bool
llvm_find_zone_layer_slot(LLVMGenCtx *ctx,
                          ASTNode *zone_decl,
                          const char *slot_name,
                          bool is_relation,
                          const char **layer_type_name_out,
                          bool *is_pool_out,
                          int *pool_capacity_out)
{
    const char *zone_name;
    LLVMHostedZoneLayerSlotView layer_view;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || slot_name == NULL) {
        return false;
    }

    zone_name = llvm_decl_node_name(zone_decl);
    layer_view =
        llvm_hosted_zone_layer_slot_view_from_decl(ctx, zone_name, zone_decl);
    if (llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing zone layer-slot metadata for zone bind emission");
        return false;
    }

    for (size_t i = 0; i < layer_view.count; i++) {
        const char *candidate_name =
            llvm_hosted_zone_layer_slot_view_name(&layer_view, i);
        if (llvm_hosted_zone_layer_slot_view_is_relation(&layer_view, i)
                == is_relation
            && candidate_name != NULL
            && strcmp(candidate_name, slot_name) == 0) {
            if (layer_type_name_out != NULL) {
                *layer_type_name_out =
                    llvm_hosted_zone_layer_slot_view_type_name(&layer_view, i);
            }
            if (is_pool_out != NULL)
                *is_pool_out =
                    llvm_hosted_zone_layer_slot_view_is_pool(&layer_view, i);
            if (pool_capacity_out != NULL)
                *pool_capacity_out =
                    llvm_hosted_zone_layer_slot_view_pool_capacity(
                        &layer_view, i);
            return layer_type_name_out == NULL || *layer_type_name_out != NULL;
        }
    }

    return false;
}

void
llvm_zone_bind_effect_layer(ASTNode *zone_decl, LLVMClassTypeEntry *zone_cls,
                            LLVMValueRef sync_fn, LLVMGenCtx *ctx,
                            const char *layer_slot_name,
                            const char *target_slot_name)
{
    ASTNode *effect_decl;
    LLVMClassTypeEntry *effect_cls;
    const PgyDomainParticipantRoleFact *bearer_role;
    LLVMDomainRuntimeProjectionView projection_view;
    const char *effect_name;
    const char *effect_type_name = NULL;
    const char *target_binding_name;
    int layer_idx;
    int target_idx;
    int subject_idx;
    LLVMValueRef self_ptr;
    LLVMValueRef layer_ptr;
    LLVMValueRef target_ptr;
    LLVMValueRef target_value;
    LLVMTypeRef target_ty;
    bool is_pool = false;
    int pool_capacity = 1;

    if (zone_decl == NULL || zone_cls == NULL || sync_fn == NULL || ctx == NULL
        || layer_slot_name == NULL || target_slot_name == NULL) {
        return;
    }

    if (!llvm_find_zone_layer_slot(ctx, zone_decl, layer_slot_name, false,
            &effect_type_name, &is_pool, &pool_capacity)) {
        return;
    }
    effect_decl = llvm_find_named_domain_decl(ctx, AST_EFFECT_DECL,
        effect_type_name);
    if (effect_decl == NULL)
        return;
    effect_name = llvm_decl_node_name(effect_decl);
    bearer_role = llvm_domain_runtime_require_participant_role(ctx,
        effect_name, PGY_DOMAIN_PARTICIPANT_EFFECT_BEARER);
    projection_view = llvm_domain_runtime_projection_view(ctx, effect_name);
    effect_cls = llvm_lookup_class(ctx, effect_name);
    target_binding_name = bearer_role != NULL
        ? bearer_role->field_name : NULL;
    if (bearer_role == NULL || !projection_view.valid
        || effect_cls == NULL || target_binding_name == NULL) {
        return;
    }

    layer_idx = llvm_class_field_index(zone_cls, layer_slot_name);
    target_idx = llvm_class_field_index(zone_cls, target_slot_name);
    subject_idx = llvm_class_field_index(effect_cls, target_binding_name);
    if (layer_idx < 0 || target_idx < 0 || subject_idx < 0) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM effect binding layout is missing an exact runtime fact field");
        return;
    }
    self_ptr = LLVMGetParam(sync_fn, 0);
    layer_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
        self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
    target_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
        self_ptr, (unsigned)target_idx, llvm_tmp_name(ctx));
    target_ty = llvm_class_field_type_at_index(zone_cls, target_idx);
    if (target_ty == NULL)
        return;
    target_value = LLVMBuildLoad2(ctx->builder,
        target_ty, target_ptr, llvm_tmp_name(ctx));
    if (is_pool) {
        LLVMValueRef tmp_effect = llvm_create_entry_alloca(ctx,
            effect_cls->struct_type, "effect.pool.tmp");
        LLVMValueRef subject_ptr;
        char sync_name[256];
        LLVMFuncEntry *sync_entry;
        LLVMValueRef sync_args[1];
        LLVMValueRef effect_value;
        LLVMTypeRef pool_ty =
            llvm_class_field_type_at_index(zone_cls, layer_idx);
        LLVMValueRef count_ptr;
        LLVMValueRef count_val;
        LLVMValueRef has_space;
        LLVMBasicBlockRef insert_bb;
        LLVMBasicBlockRef skip_bb;
        LLVMBasicBlockRef cont_bb;

        if (pool_ty == NULL)
            return;

        LLVMBuildStore(ctx->builder, LLVMConstNull(effect_cls->struct_type),
            tmp_effect);
        subject_ptr = LLVMBuildStructGEP2(ctx->builder, effect_cls->struct_type,
            tmp_effect, (unsigned)subject_idx, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, target_value, subject_ptr);
        if (!llvm_zone_bind_mark_projection_source_dirty(ctx, effect_cls,
                tmp_effect, &projection_view,
                bearer_role->field_syntax_id)) {
            return;
        }

        if (!llvm_zone_bind_sync_name(sync_name, sizeof(sync_name), effect_name))
            return;
        sync_entry = llvm_lookup_function(ctx, sync_name);
        if (sync_entry != NULL) {
            sync_args[0] = tmp_effect;
            LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                sync_args, 1, "");
        }

        effect_value = LLVMBuildLoad2(ctx->builder, effect_cls->struct_type,
            tmp_effect, llvm_tmp_name(ctx));
        count_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, layer_ptr, 2,
            llvm_tmp_name(ctx));
        count_val = LLVMBuildLoad2(ctx->builder,
            LLVMInt8TypeInContext(ctx->context), count_ptr, llvm_tmp_name(ctx));
        has_space = LLVMBuildICmp(ctx->builder, LLVMIntULT, count_val,
            LLVMConstInt(LLVMInt8TypeInContext(ctx->context),
                pool_capacity > 0 ? (unsigned)pool_capacity : 1,
                0),
            llvm_tmp_name(ctx));

        insert_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
            "effect.pool.insert");
        skip_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
            "effect.pool.skip");
        cont_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
            "effect.pool.cont");
        LLVMBuildCondBr(ctx->builder, has_space, insert_bb, skip_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, insert_bb);
        {
            LLVMValueRef idx32 = LLVMBuildZExt(ctx->builder, count_val,
                ctx->type_i32, llvm_tmp_name(ctx));
            LLVMValueRef items_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty,
                layer_ptr, 0, llvm_tmp_name(ctx));
            LLVMValueRef active_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty,
                layer_ptr, 1, llvm_tmp_name(ctx));
            LLVMTypeRef items_arr_ty = LLVMStructGetTypeAtIndex(pool_ty, 0);
            LLVMTypeRef active_arr_ty = LLVMStructGetTypeAtIndex(pool_ty, 1);
            LLVMValueRef item_slot;
            LLVMValueRef active_slot;
            LLVMValueRef next_count;
            LLVMValueRef cap_ptr;
            LLVMValueRef item_ixs[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                idx32
            };
            LLVMValueRef active_ixs[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                idx32
            };

            item_slot = LLVMBuildInBoundsGEP2(ctx->builder, items_arr_ty,
                items_ptr, item_ixs, 2, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, effect_value, item_slot);

            active_slot = LLVMBuildInBoundsGEP2(ctx->builder, active_arr_ty,
                active_ptr, active_ixs, 2, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                active_slot);

            next_count = LLVMBuildAdd(ctx->builder, count_val,
                LLVMConstInt(LLVMInt8TypeInContext(ctx->context), 1, 0),
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, next_count, count_ptr);
            cap_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, layer_ptr, 3,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt8TypeInContext(ctx->context),
                    pool_capacity > 0 ? (unsigned)pool_capacity : 1,
                    0),
                cap_ptr);
        }
        LLVMBuildBr(ctx->builder, cont_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, skip_bb);
        LLVMBuildBr(ctx->builder, cont_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
        return;
    }
    {
        LLVMValueRef subject_ptr = LLVMBuildStructGEP2(ctx->builder,
            effect_cls->struct_type, layer_ptr, (unsigned)subject_idx,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, target_value, subject_ptr);
    }
    if (!llvm_zone_bind_mark_projection_source_dirty(ctx, effect_cls,
            layer_ptr, &projection_view, bearer_role->field_syntax_id)) {
        return;
    }
    {
        char sync_name[256];
        LLVMFuncEntry *sync_entry;
        if (!llvm_zone_bind_sync_name(sync_name, sizeof(sync_name), effect_name))
            return;
        sync_entry = llvm_lookup_function(ctx, sync_name);
        if (sync_entry != NULL) {
            LLVMValueRef sync_args[] = { layer_ptr };
            LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                sync_args, 1, "");
        }
    }
}

void
llvm_zone_bind_relation_layer(ASTNode *zone_decl, LLVMClassTypeEntry *zone_cls,
                              LLVMValueRef sync_fn, LLVMGenCtx *ctx,
                              const char *layer_slot_name,
                              const char *left_slot_name,
                              const char *right_slot_name)
{
    ASTNode *relation_decl;
    LLVMClassTypeEntry *relation_cls;
    const PgyDomainParticipantRoleFact *source_role;
    const PgyDomainParticipantRoleFact *target_role;
    LLVMDomainRuntimeProjectionView projection_view;
    const char *relation_name;
    const char *relation_type_name = NULL;
    const char *left_binding_name;
    const char *right_binding_name;
    int layer_idx;
    int left_idx;
    int right_idx;
    int left_subject_idx;
    int right_subject_idx;
    LLVMValueRef self_ptr;
    LLVMValueRef layer_ptr;
    LLVMValueRef left_ptr;
    LLVMValueRef right_ptr;
    LLVMValueRef left_value;
    LLVMValueRef right_value;
    LLVMTypeRef left_ty;
    LLVMTypeRef right_ty;

    if (zone_decl == NULL || zone_cls == NULL || sync_fn == NULL || ctx == NULL
        || layer_slot_name == NULL || left_slot_name == NULL
        || right_slot_name == NULL) {
        return;
    }

    if (!llvm_find_zone_layer_slot(ctx, zone_decl, layer_slot_name, true,
            &relation_type_name, NULL, NULL)) {
        return;
    }
    relation_decl = llvm_find_named_domain_decl(ctx, AST_RELATION_DECL,
        relation_type_name);
    if (relation_decl == NULL)
        return;
    relation_name = llvm_decl_node_name(relation_decl);
    source_role = llvm_domain_runtime_require_participant_role(ctx,
        relation_name, PGY_DOMAIN_PARTICIPANT_RELATION_SOURCE);
    target_role = llvm_domain_runtime_require_participant_role(ctx,
        relation_name, PGY_DOMAIN_PARTICIPANT_RELATION_TARGET);
    projection_view = llvm_domain_runtime_projection_view(ctx,
        relation_name);
    relation_cls = llvm_lookup_class(ctx, relation_name);
    left_binding_name = source_role != NULL ? source_role->field_name : NULL;
    right_binding_name = target_role != NULL ? target_role->field_name : NULL;
    if (source_role == NULL || target_role == NULL
        || !projection_view.valid || relation_cls == NULL
        || left_binding_name == NULL
        || right_binding_name == NULL) {
        return;
    }

    layer_idx = llvm_class_field_index(zone_cls, layer_slot_name);
    left_idx = llvm_class_field_index(zone_cls, left_slot_name);
    right_idx = llvm_class_field_index(zone_cls, right_slot_name);
    left_subject_idx = llvm_class_field_index(relation_cls, left_binding_name);
    right_subject_idx = llvm_class_field_index(relation_cls, right_binding_name);
    if (layer_idx < 0 || left_idx < 0 || right_idx < 0
        || left_subject_idx < 0 || right_subject_idx < 0) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM relation binding layout is missing an exact runtime fact field");
        return;
    }

    self_ptr = LLVMGetParam(sync_fn, 0);
    layer_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
        self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
    left_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
        self_ptr, (unsigned)left_idx, llvm_tmp_name(ctx));
    right_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
        self_ptr, (unsigned)right_idx, llvm_tmp_name(ctx));
    left_ty = llvm_class_field_type_at_index(zone_cls, left_idx);
    right_ty = llvm_class_field_type_at_index(zone_cls, right_idx);
    if (left_ty == NULL || right_ty == NULL)
        return;
    left_value = LLVMBuildLoad2(ctx->builder,
        left_ty, left_ptr, llvm_tmp_name(ctx));
    right_value = LLVMBuildLoad2(ctx->builder,
        right_ty, right_ptr, llvm_tmp_name(ctx));
    {
        LLVMValueRef subject_ptr = LLVMBuildStructGEP2(ctx->builder,
            relation_cls->struct_type, layer_ptr, (unsigned)left_subject_idx,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, left_value, subject_ptr);
    }
    {
        LLVMValueRef subject_ptr = LLVMBuildStructGEP2(ctx->builder,
            relation_cls->struct_type, layer_ptr, (unsigned)right_subject_idx,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, right_value, subject_ptr);
    }
    if (!llvm_zone_bind_mark_projection_source_dirty(ctx, relation_cls,
            layer_ptr, &projection_view, source_role->field_syntax_id)
        || !llvm_zone_bind_mark_projection_source_dirty(ctx, relation_cls,
            layer_ptr, &projection_view, target_role->field_syntax_id)) {
        return;
    }
    {
        char sync_name[256];
        LLVMFuncEntry *sync_entry;
        if (!llvm_zone_bind_sync_name(sync_name, sizeof(sync_name), relation_name))
            return;
        sync_entry = llvm_lookup_function(ctx, sync_name);
        if (sync_entry != NULL) {
            LLVMValueRef sync_args[] = { layer_ptr };
            LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                sync_args, 1, "");
        }
    }
}

#endif
