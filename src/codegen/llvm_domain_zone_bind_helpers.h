static ASTNode *
llvm_find_nth_bindable_domain_slot(ASTNode **slots, size_t slot_count,
                                   ASTNode **refreshes, size_t refresh_count,
                                   size_t nth)
{
    size_t seen = 0;
    (void)refreshes;
    (void)refresh_count;

    if (slots == NULL)
        return NULL;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_binding) {
            continue;
        }
        if (seen == nth)
            return slot;
        seen++;
    }

    return NULL;
}

static ASTNode *
llvm_find_named_domain_decl_local(LLVMGenCtx *ctx, ASTNodeType decl_type, const char *name)
{
    return llvm_find_decl_in_active_inventory(ctx, decl_type, name);
}

static ASTNode *
llvm_find_zone_layer_slot(ASTNode *zone_decl, const char *slot_name, bool is_relation)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.is_relation == is_relation
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

static void
llvm_zone_bind_effect_layer(ASTNode *zone_decl, LLVMClassTypeEntry *zone_cls,
                            LLVMValueRef sync_fn, LLVMGenCtx *ctx,
                            const char *layer_slot_name, const char *target_slot_name)
{
    ASTNode *layer_slot;
    ASTNode *effect_decl;
    ASTNode *target_slot;
    LLVMClassTypeEntry *effect_cls;
    int layer_idx;
    int target_idx;
    int subject_idx;
    LLVMValueRef self_ptr;
    LLVMValueRef layer_ptr;
    LLVMValueRef target_ptr;
    LLVMValueRef target_value;
    bool is_pool = false;

    if (zone_decl == NULL || zone_cls == NULL || sync_fn == NULL || ctx == NULL
        || layer_slot_name == NULL || target_slot_name == NULL) {
        return;
    }

    layer_slot = llvm_find_zone_layer_slot(zone_decl, layer_slot_name, false);
    if (layer_slot == NULL)
        return;
    effect_decl = llvm_find_named_domain_decl_local(ctx, AST_EFFECT_DECL,
        layer_slot->data.zone_layer_slot.layer_type);
    if (effect_decl == NULL)
        return;
    target_slot = llvm_find_nth_bindable_domain_slot(effect_decl->data.effect_decl.slots,
        effect_decl->data.effect_decl.slot_count,
        effect_decl->data.effect_decl.refreshes,
        effect_decl->data.effect_decl.refresh_count, 0);
    effect_cls = llvm_lookup_class(ctx, effect_decl->data.effect_decl.name);
    if (target_slot == NULL || effect_cls == NULL
        || target_slot->data.domain_slot.slot_name == NULL) {
        return;
    }

    layer_idx = llvm_class_field_index(zone_cls, layer_slot_name);
    target_idx = llvm_class_field_index(zone_cls, target_slot_name);
    subject_idx = llvm_class_field_index(effect_cls, target_slot->data.domain_slot.slot_name);
    if (layer_idx < 0 || target_idx < 0 || subject_idx < 0)
        return;
    is_pool = layer_slot->data.zone_layer_slot.is_pool;

    self_ptr = LLVMGetParam(sync_fn, 0);
    layer_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
        self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
    target_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
        self_ptr, (unsigned)target_idx, llvm_tmp_name(ctx));
    target_value = LLVMBuildLoad2(ctx->builder,
        zone_cls->fields[target_idx].field_type, target_ptr, llvm_tmp_name(ctx));
    if (is_pool) {
        LLVMValueRef tmp_effect = llvm_create_entry_alloca(ctx,
            effect_cls->struct_type, "effect.pool.tmp");
        LLVMValueRef subject_ptr;
        char sync_name[256];
        LLVMFuncEntry *sync_entry;
        LLVMValueRef sync_args[1];
        LLVMValueRef effect_value;
        LLVMTypeRef pool_ty = zone_cls->fields[layer_idx].field_type;
        LLVMValueRef count_ptr;
        LLVMValueRef count_val;
        LLVMValueRef has_space;
        LLVMBasicBlockRef insert_bb;
        LLVMBasicBlockRef skip_bb;
        LLVMBasicBlockRef cont_bb;

        LLVMBuildStore(ctx->builder, LLVMConstNull(effect_cls->struct_type), tmp_effect);
        subject_ptr = LLVMBuildStructGEP2(ctx->builder, effect_cls->struct_type,
            tmp_effect, (unsigned)subject_idx, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, target_value, subject_ptr);

        snprintf(sync_name, sizeof(sync_name), "%s_sync", effect_decl->data.effect_decl.name);
        sync_entry = llvm_lookup_function(ctx, sync_name);
        if (sync_entry != NULL) {
            sync_args[0] = tmp_effect;
            LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                sync_args, 1, "");
        }

        effect_value = LLVMBuildLoad2(ctx->builder, effect_cls->struct_type,
            tmp_effect, llvm_tmp_name(ctx));
        count_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, layer_ptr, 2, llvm_tmp_name(ctx));
        count_val = LLVMBuildLoad2(ctx->builder, LLVMInt8TypeInContext(ctx->context),
            count_ptr, llvm_tmp_name(ctx));
        has_space = LLVMBuildICmp(ctx->builder, LLVMIntULT, count_val,
            LLVMConstInt(LLVMInt8TypeInContext(ctx->context),
                layer_slot->data.zone_layer_slot.pool_capacity > 0
                    ? (unsigned)layer_slot->data.zone_layer_slot.pool_capacity : 1,
                0),
            llvm_tmp_name(ctx));

        insert_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "effect.pool.insert");
        skip_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "effect.pool.skip");
        cont_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "effect.pool.cont");
        LLVMBuildCondBr(ctx->builder, has_space, insert_bb, skip_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, insert_bb);
        {
            LLVMValueRef idx32 = LLVMBuildZExt(ctx->builder, count_val, ctx->type_i32, llvm_tmp_name(ctx));
            LLVMValueRef items_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, layer_ptr, 0, llvm_tmp_name(ctx));
            LLVMValueRef active_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, layer_ptr, 1, llvm_tmp_name(ctx));
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
            item_slot = LLVMBuildInBoundsGEP2(ctx->builder, items_arr_ty, items_ptr,
                item_ixs, 2, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, effect_value, item_slot);

            LLVMValueRef active_ixs[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                idx32
            };
            active_slot = LLVMBuildInBoundsGEP2(ctx->builder, active_arr_ty, active_ptr,
                active_ixs, 2, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), active_slot);

            next_count = LLVMBuildAdd(ctx->builder, count_val,
                LLVMConstInt(LLVMInt8TypeInContext(ctx->context), 1, 0),
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, next_count, count_ptr);
            cap_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, layer_ptr, 3, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt8TypeInContext(ctx->context),
                    layer_slot->data.zone_layer_slot.pool_capacity > 0
                        ? (unsigned)layer_slot->data.zone_layer_slot.pool_capacity : 1,
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
        LLVMValueRef subject_ptr = LLVMBuildStructGEP2(ctx->builder, effect_cls->struct_type,
            layer_ptr, (unsigned)subject_idx, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, target_value, subject_ptr);
    }
    for (size_t i = 0; i < effect_decl->data.effect_decl.refresh_count; i++) {
        ASTNode *refresh = effect_decl->data.effect_decl.refreshes[i];
        const char *projection_name;
        const char *source_name;
        char dirty_field[256];
        char ready_field[256];
        int dirty_idx;
        int ready_idx;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;
        projection_name = refresh->data.zone_refresh.object_slot_name;
        source_name = refresh->data.zone_refresh.source_slot_name;
        if (projection_name == NULL || source_name == NULL
            || strcmp(source_name, target_slot->data.domain_slot.slot_name) != 0) {
            continue;
        }

        snprintf(dirty_field, sizeof(dirty_field), "__projection_dirty_%s",
            projection_name);
        snprintf(ready_field, sizeof(ready_field), "__projection_ready_%s",
            projection_name);
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
        snprintf(sync_name, sizeof(sync_name), "%s_sync", effect_decl->data.effect_decl.name);
        sync_entry = llvm_lookup_function(ctx, sync_name);
        if (sync_entry != NULL) {
            LLVMValueRef sync_args[] = { layer_ptr };
            LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                sync_args, 1, "");
        }
    }
}

static void
llvm_zone_bind_relation_layer(ASTNode *zone_decl, LLVMClassTypeEntry *zone_cls,
                              LLVMValueRef sync_fn, LLVMGenCtx *ctx,
                              const char *layer_slot_name,
                              const char *left_slot_name,
                              const char *right_slot_name)
{
    ASTNode *layer_slot;
    ASTNode *relation_decl;
    ASTNode *left_target;
    ASTNode *right_target;
    LLVMClassTypeEntry *relation_cls;
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

    if (zone_decl == NULL || zone_cls == NULL || sync_fn == NULL || ctx == NULL
        || layer_slot_name == NULL || left_slot_name == NULL || right_slot_name == NULL) {
        return;
    }

    layer_slot = llvm_find_zone_layer_slot(zone_decl, layer_slot_name, true);
    if (layer_slot == NULL)
        return;
    relation_decl = llvm_find_named_domain_decl_local(ctx, AST_RELATION_DECL,
        layer_slot->data.zone_layer_slot.layer_type);
    if (relation_decl == NULL)
        return;
    left_target = llvm_find_nth_bindable_domain_slot(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count, 0);
    right_target = llvm_find_nth_bindable_domain_slot(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count, 1);
    relation_cls = llvm_lookup_class(ctx, relation_decl->data.relation_decl.name);
    if (left_target == NULL || right_target == NULL || relation_cls == NULL
        || left_target->data.domain_slot.slot_name == NULL
        || right_target->data.domain_slot.slot_name == NULL) {
        return;
    }

    layer_idx = llvm_class_field_index(zone_cls, layer_slot_name);
    left_idx = llvm_class_field_index(zone_cls, left_slot_name);
    right_idx = llvm_class_field_index(zone_cls, right_slot_name);
    left_subject_idx = llvm_class_field_index(relation_cls, left_target->data.domain_slot.slot_name);
    right_subject_idx = llvm_class_field_index(relation_cls, right_target->data.domain_slot.slot_name);
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
    left_value = LLVMBuildLoad2(ctx->builder,
        zone_cls->fields[left_idx].field_type, left_ptr, llvm_tmp_name(ctx));
    right_value = LLVMBuildLoad2(ctx->builder,
        zone_cls->fields[right_idx].field_type, right_ptr, llvm_tmp_name(ctx));
    {
        LLVMValueRef subject_ptr = LLVMBuildStructGEP2(ctx->builder, relation_cls->struct_type,
            layer_ptr, (unsigned)left_subject_idx, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, left_value, subject_ptr);
    }
    {
        LLVMValueRef subject_ptr = LLVMBuildStructGEP2(ctx->builder, relation_cls->struct_type,
            layer_ptr, (unsigned)right_subject_idx, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, right_value, subject_ptr);
    }
    for (size_t i = 0; i < relation_decl->data.relation_decl.refresh_count; i++) {
        ASTNode *refresh = relation_decl->data.relation_decl.refreshes[i];
        const char *projection_name;
        const char *source_name;
        char dirty_field[256];
        char ready_field[256];
        int dirty_idx;
        int ready_idx;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;
        projection_name = refresh->data.zone_refresh.object_slot_name;
        source_name = refresh->data.zone_refresh.source_slot_name;
        if (projection_name == NULL || source_name == NULL)
            continue;
        if (strcmp(source_name, left_target->data.domain_slot.slot_name) != 0
            && strcmp(source_name, right_target->data.domain_slot.slot_name) != 0) {
            continue;
        }

        snprintf(dirty_field, sizeof(dirty_field), "__projection_dirty_%s",
            projection_name);
        snprintf(ready_field, sizeof(ready_field), "__projection_ready_%s",
            projection_name);
        dirty_idx = llvm_class_field_index(relation_cls, dirty_field);
        ready_idx = llvm_class_field_index(relation_cls, ready_field);
        if (dirty_idx >= 0) {
            LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                relation_cls->struct_type, layer_ptr, (unsigned)dirty_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), dirty_ptr);
        }
        if (ready_idx >= 0) {
            LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                relation_cls->struct_type, layer_ptr, (unsigned)ready_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), ready_ptr);
        }
    }
    {
        char sync_name[256];
        LLVMFuncEntry *sync_entry;
        snprintf(sync_name, sizeof(sync_name), "%s_sync", relation_decl->data.relation_decl.name);
        sync_entry = llvm_lookup_function(ctx, sync_name);
        if (sync_entry != NULL) {
            LLVMValueRef sync_args[] = { layer_ptr };
            LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                sync_args, 1, "");
        }
    }
}

