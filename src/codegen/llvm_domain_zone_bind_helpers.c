/*
 * LLVM zone bind lowering for effect/relation layers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_zone_bind_helpers.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"
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

static const char *
llvm_domain_slot_view_bindable_name(const LLVMHostedDomainSlotView *slot_view,
                                    size_t nth)
{
    size_t seen = 0;

    if (slot_view == NULL)
        return NULL;

    for (size_t i = 0; i < slot_view->count; i++) {
        if (!llvm_hosted_domain_slot_view_is_binding_like(slot_view, i))
            continue;
        if (seen == nth)
            return llvm_hosted_domain_slot_view_name(slot_view, i);
        seen++;
    }

    return NULL;
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
    size_t effect_refresh_count = 0;
    ASTNode **effect_refreshes =
        ast_effect_refreshes(effect_decl, &effect_refresh_count);
    effect_name = llvm_decl_node_name(effect_decl);
    LLVMHostedDomainSlotView effect_slot_view =
        llvm_hosted_domain_slot_view_from_decl(ctx, effect_name, effect_decl);
    if (llvm_hosted_domain_slot_view_missing_mir_metadata(
            &effect_slot_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing zone effect bind domain-slot metadata for '%s'",
            effect_name != NULL ? effect_name : "(anonymous-effect)");
        return;
    }
    effect_cls = llvm_lookup_class(ctx, effect_name);
    target_binding_name = llvm_domain_slot_view_bindable_name(
        &effect_slot_view, 0);
    if (effect_cls == NULL || target_binding_name == NULL) {
        return;
    }

    layer_idx = llvm_class_field_index(zone_cls, layer_slot_name);
    target_idx = llvm_class_field_index(zone_cls, target_slot_name);
    subject_idx = llvm_class_field_index(effect_cls, target_binding_name);
    if (layer_idx < 0 || target_idx < 0 || subject_idx < 0)
        return;
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
    for (size_t i = 0; i < effect_refresh_count; i++) {
        ASTNode *refresh = effect_refreshes[i];
        const char *projection_name;
        const char *source_name;
        char dirty_field[256];
        char ready_field[256];
        int dirty_idx;
        int ready_idx;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;
        projection_name = ast_zone_refresh_object_slot_name(refresh);
        source_name = ast_zone_refresh_source_slot_name(refresh);
        if (projection_name == NULL || source_name == NULL
            || strcmp(source_name, target_binding_name) != 0) {
            continue;
        }

        if (!llvm_zone_bind_projection_field_name(dirty_field,
                sizeof(dirty_field), "dirty", projection_name))
            continue;
        if (!llvm_zone_bind_projection_field_name(ready_field,
                sizeof(ready_field), "ready", projection_name))
            continue;
        dirty_idx = llvm_class_field_index(effect_cls, dirty_field);
        ready_idx = llvm_class_field_index(effect_cls, ready_field);
        if (dirty_idx >= 0) {
            LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                effect_cls->struct_type, layer_ptr, (unsigned)dirty_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                dirty_ptr);
        }
        if (ready_idx >= 0) {
            LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                effect_cls->struct_type, layer_ptr, (unsigned)ready_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0),
                ready_ptr);
        }
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
    size_t relation_refresh_count = 0;
    ASTNode **relation_refreshes =
        ast_relation_refreshes(relation_decl, &relation_refresh_count);
    relation_name = llvm_decl_node_name(relation_decl);
    LLVMHostedDomainSlotView relation_slot_view =
        llvm_hosted_domain_slot_view_from_decl(ctx, relation_name,
                                               relation_decl);
    if (llvm_hosted_domain_slot_view_missing_mir_metadata(
            &relation_slot_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing zone relation bind domain-slot metadata for '%s'",
            relation_name != NULL ? relation_name : "(anonymous-relation)");
        return;
    }
    relation_cls = llvm_lookup_class(ctx, relation_name);
    left_binding_name = llvm_domain_slot_view_bindable_name(
        &relation_slot_view, 0);
    right_binding_name = llvm_domain_slot_view_bindable_name(
        &relation_slot_view, 1);
    if (relation_cls == NULL || left_binding_name == NULL
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
    for (size_t i = 0; i < relation_refresh_count; i++) {
        ASTNode *refresh = relation_refreshes[i];
        const char *projection_name;
        const char *source_name;
        char dirty_field[256];
        char ready_field[256];
        int dirty_idx;
        int ready_idx;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;
        projection_name = ast_zone_refresh_object_slot_name(refresh);
        source_name = ast_zone_refresh_source_slot_name(refresh);
        if (projection_name == NULL || source_name == NULL)
            continue;
        if (strcmp(source_name, left_binding_name) != 0
            && strcmp(source_name, right_binding_name) != 0) {
            continue;
        }

        if (!llvm_zone_bind_projection_field_name(dirty_field,
                sizeof(dirty_field), "dirty", projection_name))
            continue;
        if (!llvm_zone_bind_projection_field_name(ready_field,
                sizeof(ready_field), "ready", projection_name))
            continue;
        dirty_idx = llvm_class_field_index(relation_cls, dirty_field);
        ready_idx = llvm_class_field_index(relation_cls, ready_field);
        if (dirty_idx >= 0) {
            LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                relation_cls->struct_type, layer_ptr, (unsigned)dirty_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                dirty_ptr);
        }
        if (ready_idx >= 0) {
            LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                relation_cls->struct_type, layer_ptr, (unsigned)ready_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0),
                ready_ptr);
        }
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
