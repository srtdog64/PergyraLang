static void
llvm_emit_world_embedded_receiver_projection_sync(LLVMGenCtx *ctx,
                                                  ASTNode *receiver)
{
    ASTNode *host_decl;
    const char *zone_slot_name = NULL;
    ASTNode *zone_decl = NULL;
    const char *source_slot_name = NULL;
    LLVMClassTypeEntry *world_cls;
    LLVMClassTypeEntry *zone_cls;
    LLVMVarEntry *self_var;
    LLVMValueRef world_ptr;
    LLVMValueRef zone_ptr;
    LLVMFuncEntry *sync_entry;
    int zone_field_idx;

    if (ctx == NULL || receiver == NULL)
        return;

    host_decl = llvm_current_host_decl(ctx);
    if (host_decl == NULL || host_decl->type != AST_WORLD_DECL)
        return;
    if (!llvm_world_embedded_projection_source_from_assignment(ctx, receiver,
            &zone_slot_name, &zone_decl, &source_slot_name, NULL)
        || zone_slot_name == NULL
        || zone_decl == NULL) {
        return;
    }

    world_cls = llvm_lookup_class(ctx, host_decl->data.world_decl.name);
    zone_cls = llvm_lookup_class(ctx, zone_decl->data.zone_decl.name);
    self_var = llvm_scope_lookup(ctx, "self");
    if (world_cls == NULL || zone_cls == NULL || self_var == NULL
        || zone_cls->sync_function_name == NULL) {
        return;
    }

    sync_entry = llvm_lookup_function(ctx, zone_cls->sync_function_name);
    if (sync_entry == NULL || sync_entry->fn == NULL || sync_entry->fn_type == NULL)
        return;

    world_ptr = self_var->alloca;
    if (self_var->type == LLVMPointerType(world_cls->struct_type, 0)) {
        world_ptr = LLVMBuildLoad2(ctx->builder, self_var->type,
            self_var->alloca, llvm_tmp_name(ctx));
    }

    zone_field_idx = llvm_class_field_index(world_cls, zone_slot_name);
    if (zone_field_idx < 0)
        return;

    zone_ptr = LLVMBuildStructGEP2(ctx->builder, world_cls->struct_type,
        world_ptr, (unsigned)zone_field_idx, llvm_tmp_name(ctx));

    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        char field_name[256];
        int field_idx;
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.slot_name == NULL
            || slot->data.domain_slot.is_subject) {
            continue;
        }

        snprintf(field_name, sizeof(field_name), "__projection_dirty_%s",
            slot->data.domain_slot.slot_name);
        field_idx = llvm_class_field_index(zone_cls, field_name);
        if (field_idx >= 0) {
            LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                zone_cls->struct_type, zone_ptr, (unsigned)field_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), dirty_ptr);
        }

        snprintf(field_name, sizeof(field_name), "__projection_ready_%s",
            slot->data.domain_slot.slot_name);
        field_idx = llvm_class_field_index(zone_cls, field_name);
        if (field_idx >= 0) {
            LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                zone_cls->struct_type, zone_ptr, (unsigned)field_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), ready_ptr);
        }
    }

    {
        LLVMValueRef args[] = { zone_ptr };
        LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
            args, 1, "");
    }

    {
        char dirty_field[256];
        int dirty_idx;
        int derived_idx;
        char world_sync_name[256];
        LLVMFuncEntry *world_sync;

        snprintf(dirty_field, sizeof(dirty_field), "__zone_dirty_%s", zone_slot_name);
        dirty_idx = llvm_class_field_index(world_cls, dirty_field);
        if (dirty_idx >= 0) {
            LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                world_cls->struct_type, world_ptr, (unsigned)dirty_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), dirty_ptr);
        }
        derived_idx = llvm_class_field_index(world_cls, "__world_derived_dirty");
        if (derived_idx >= 0) {
            LLVMValueRef derived_ptr = LLVMBuildStructGEP2(ctx->builder,
                world_cls->struct_type, world_ptr, (unsigned)derived_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), derived_ptr);
        }
        snprintf(world_sync_name, sizeof(world_sync_name), "%s_sync",
            host_decl->data.world_decl.name != NULL
                ? host_decl->data.world_decl.name : "World");
        world_sync = llvm_lookup_function(ctx, world_sync_name);
        if (world_sync != NULL && world_sync->fn != NULL && world_sync->fn_type != NULL) {
            LLVMValueRef world_args[] = { world_ptr };
            LLVMBuildCall2(ctx->builder, world_sync->fn_type, world_sync->fn,
                world_args, 1, "");
        }
    }
}

static ASTNode *
llvm_call_find_domain_decl(LLVMGenCtx *ctx, ASTNodeType decl_type, const char *name)
{
    ASTNode **items = NULL;
    size_t count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;

    llvm_active_inventory(ctx, decl_type, &items, &count);
    for (size_t i = 0; i < count; i++) {
        ASTNode *item = items[i];
        const char *item_name = NULL;
        if (item == NULL || item->type != decl_type)
            continue;
        switch (decl_type) {
        case AST_EFFECT_DECL:
            item_name = item->data.effect_decl.name;
            break;
        case AST_ZONE_DECL:
            item_name = item->data.zone_decl.name;
            break;
        default:
            break;
        }
        if (item_name != NULL && strcmp(item_name, name) == 0)
            return item;
    }
    return NULL;
}

static ASTNode *
llvm_call_find_first_effect_subject_slot(ASTNode *effect_decl)
{
    if (effect_decl == NULL || effect_decl->type != AST_EFFECT_DECL)
        return NULL;
    for (size_t i = 0; i < effect_decl->data.effect_decl.slot_count; i++) {
        ASTNode *slot = effect_decl->data.effect_decl.slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.is_subject
            && slot->data.domain_slot.slot_name != NULL) {
            return slot;
        }
    }
    return NULL;
}

static void
llvm_emit_world_embedded_action_effect_sync(LLVMGenCtx *ctx,
                                            ASTNode *receiver,
                                            ASTNode *method_decl)
{
    ASTNode *host_decl;
    const char *zone_slot_name = NULL;
    ASTNode *zone_decl = NULL;
    const char *source_slot_name = NULL;
    ASTNode *effect_decl;
    ASTNode *target_slot;
    LLVMClassTypeEntry *world_cls;
    LLVMClassTypeEntry *zone_cls;
    LLVMClassTypeEntry *effect_cls;
    LLVMVarEntry *self_var;
    LLVMValueRef world_ptr;
    LLVMValueRef zone_ptr;
    const char *effect_name;

    if (ctx == NULL || receiver == NULL || method_decl == NULL
        || method_decl->type != AST_FUNC_DECL
        || method_decl->is_async_decl
        || !method_decl->data.func_decl.is_action
        || method_decl->data.func_decl.within_zone == NULL
        || method_decl->data.func_decl.causes_effect == NULL) {
        return;
    }

    host_decl = llvm_current_host_decl(ctx);
    if (host_decl == NULL || host_decl->type != AST_WORLD_DECL)
        return;

    if (!llvm_world_embedded_projection_source_from_assignment(ctx, receiver,
            &zone_slot_name, &zone_decl, &source_slot_name, NULL)
        || zone_slot_name == NULL || zone_decl == NULL || source_slot_name == NULL
        || zone_decl->data.zone_decl.name == NULL
        || strcmp(method_decl->data.func_decl.within_zone,
                  zone_decl->data.zone_decl.name) != 0) {
        return;
    }

    effect_name = method_decl->data.func_decl.causes_effect;
    effect_decl = llvm_call_find_domain_decl(ctx, AST_EFFECT_DECL, effect_name);
    target_slot = llvm_call_find_first_effect_subject_slot(effect_decl);
    world_cls = llvm_lookup_class(ctx, host_decl->data.world_decl.name);
    zone_cls = llvm_lookup_class(ctx, zone_decl->data.zone_decl.name);
    effect_cls = llvm_lookup_class(ctx, effect_name);
    self_var = llvm_scope_lookup(ctx, "self");
    if (effect_decl == NULL || target_slot == NULL || world_cls == NULL
        || zone_cls == NULL || effect_cls == NULL || self_var == NULL) {
        return;
    }

    world_ptr = self_var->alloca;
    if (self_var->type == LLVMPointerType(world_cls->struct_type, 0)) {
        world_ptr = LLVMBuildLoad2(ctx->builder, self_var->type,
            self_var->alloca, llvm_tmp_name(ctx));
    }
    {
        int zone_idx = llvm_class_field_index(world_cls, zone_slot_name);
        if (zone_idx < 0)
            return;
        zone_ptr = LLVMBuildStructGEP2(ctx->builder, world_cls->struct_type,
            world_ptr, (unsigned)zone_idx, llvm_tmp_name(ctx));
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer_slot = zone_decl->data.zone_decl.layer_slots[i];
        const char *layer_name;
        int active_idx;
        int layer_idx;
        int source_idx;
        int target_idx;
        LLVMValueRef layer_ptr;
        LLVMValueRef source_ptr;
        LLVMValueRef source_value;

        if (layer_slot == NULL || layer_slot->type != AST_ZONE_LAYER_SLOT
            || layer_slot->data.zone_layer_slot.is_relation
            || layer_slot->data.zone_layer_slot.layer_type == NULL
            || strcmp(layer_slot->data.zone_layer_slot.layer_type, effect_name) != 0) {
            continue;
        }

        layer_name = layer_slot->data.zone_layer_slot.slot_name;
        if (layer_name == NULL)
            continue;

        {
            char active_field[256];
            snprintf(active_field, sizeof(active_field), "__layer_active_%s", layer_name);
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
            snprintf(epoch_field, sizeof(epoch_field), "__layer_epoch_%s", layer_name);
            snprintf(cause_field, sizeof(cause_field), "__layer_cause_%s", layer_name);
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
        for (size_t si = 0; si < zone_decl->data.zone_decl.state_count; si++) {
            ASTNode *state = zone_decl->data.zone_decl.states[si];
            char state_epoch_field[256];
            char state_cause_field[256];
            int state_epoch_idx;
            int state_cause_idx;
            if (state == NULL || state->type != AST_ZONE_STATE
                || state->data.zone_state.is_relation
                || state->data.zone_state.state_name == NULL
                || state->data.zone_state.layer_slot_name == NULL
                || strcmp(state->data.zone_state.layer_slot_name, layer_name) != 0) {
                continue;
            }
            snprintf(state_epoch_field, sizeof(state_epoch_field), "__state_epoch_%s",
                state->data.zone_state.state_name);
            snprintf(state_cause_field, sizeof(state_cause_field), "__state_cause_%s",
                state->data.zone_state.state_name);
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
        target_idx = llvm_class_field_index(effect_cls,
            target_slot->data.domain_slot.slot_name);
        if (layer_idx < 0 || source_idx < 0 || target_idx < 0)
            continue;

        layer_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
            zone_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
        source_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
            zone_ptr, (unsigned)source_idx, llvm_tmp_name(ctx));
        source_value = LLVMBuildLoad2(ctx->builder,
            zone_cls->fields[source_idx].field_type, source_ptr, llvm_tmp_name(ctx));

        if (layer_slot->data.zone_layer_slot.is_pool) {
            LLVMValueRef tmp_effect = llvm_create_entry_alloca(ctx,
                effect_cls->struct_type, "world.effect.pool.tmp");
            LLVMValueRef subject_ptr;
            char sync_name[256];
            LLVMFuncEntry *sync_entry;
            LLVMValueRef effect_value;
            LLVMTypeRef pool_ty = zone_cls->fields[layer_idx].field_type;
            LLVMValueRef count_ptr;
            LLVMValueRef count_val;
            LLVMValueRef has_space;
            LLVMValueRef current_fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
            LLVMBasicBlockRef insert_bb;
            LLVMBasicBlockRef skip_bb;
            LLVMBasicBlockRef cont_bb;

            LLVMBuildStore(ctx->builder, LLVMConstNull(effect_cls->struct_type), tmp_effect);
            subject_ptr = LLVMBuildStructGEP2(ctx->builder, effect_cls->struct_type,
                tmp_effect, (unsigned)target_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, source_value, subject_ptr);
            snprintf(sync_name, sizeof(sync_name), "%s_sync", effect_name);
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
                    layer_slot->data.zone_layer_slot.pool_capacity > 0
                        ? (unsigned)layer_slot->data.zone_layer_slot.pool_capacity : 1,
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
                        layer_slot->data.zone_layer_slot.pool_capacity > 0
                            ? (unsigned)layer_slot->data.zone_layer_slot.pool_capacity : 1,
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
        for (size_t ri = 0; ri < effect_decl->data.effect_decl.refresh_count; ri++) {
            ASTNode *refresh = effect_decl->data.effect_decl.refreshes[ri];
            const char *projection_name;
            const char *refresh_source;
            char dirty_field[256];
            char ready_field[256];
            int dirty_idx;
            int ready_idx;
            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            projection_name = refresh->data.zone_refresh.object_slot_name;
            refresh_source = refresh->data.zone_refresh.source_slot_name;
            if (projection_name == NULL || refresh_source == NULL
                || strcmp(refresh_source, target_slot->data.domain_slot.slot_name) != 0) {
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
            snprintf(sync_name, sizeof(sync_name), "%s_sync", effect_name);
            sync_entry = llvm_lookup_function(ctx, sync_name);
            if (sync_entry != NULL) {
                LLVMValueRef sync_args[] = { layer_ptr };
                LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                    sync_args, 1, "");
            }
        }
    }
}

static LLVMValueRef
llvm_emit_member_call(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *obj_node = node->data.call.callee->data.member.object;
    const char *method_name = node->data.call.callee->data.member.name;

    /* Dynamic vtable dispatch: party.slot.method()
     * AST: call(member(member(party_var, slot_name), method_name)) */
    if (obj_node != NULL && obj_node->type == AST_MEMBER_ACCESS
        && method_name != NULL) {
        ASTNode *party_node = obj_node->data.member.object;
        const char *slot_name = obj_node->data.member.name;

        if (party_node != NULL && party_node->type == AST_IDENTIFIER
            && slot_name != NULL) {
            const char *party_var = party_node->data.identifier.name;
            const char *party_class = llvm_lookup_var_class(ctx, party_var);
            LLVMClassTypeEntry *cls = party_class
                ? llvm_lookup_class(ctx, party_class) : NULL;

            if (cls != NULL) {
                /* Find vtable pointer field: slot_name + "_vtable" */
                char vt_field[256];
                snprintf(vt_field, sizeof(vt_field), "%s_vtable", slot_name);
                int vt_idx = -1;
                for (int fi = 0; fi < cls->field_count; fi++) {
                    if (strcmp(cls->fields[fi].field_name, vt_field) == 0) {
                        vt_idx = cls->fields[fi].index;
                        break;
                    }
                }

                if (vt_idx >= 0) {
                    /* Find which ability this slot requires, then look up
                     * the method index in that ability's vtable struct */
                    int method_idx = -1;
                    LLVMClassTypeEntry *vt_cls = NULL;

                    /* Search for *_vtable class that has this method */
                    for (int ci = 0; ci < ctx->class_type_count; ci++) {
                        const char *cn = ctx->class_types[ci].class_name;
                        if (cn != NULL && strstr(cn, "_vtable") != NULL) {
                            for (int fi = 0; fi < ctx->class_types[ci].field_count; fi++) {
                                if (strcmp(ctx->class_types[ci].fields[fi].field_name,
                                          method_name) == 0) {
                                    vt_cls = &ctx->class_types[ci];
                                    method_idx = ctx->class_types[ci].fields[fi].index;
                                    break;
                                }
                            }
                            if (method_idx >= 0)
                                break;
                        }
                    }

                    if (vt_cls != NULL && method_idx >= 0) {
                        LLVMVarEntry *pvar = llvm_scope_lookup(ctx, party_var);
                        if (pvar != NULL) {
                            /* Load vtable pointer from party struct */
                            LLVMValueRef vt_ptr_field = LLVMBuildStructGEP2(
                                ctx->builder, cls->struct_type, pvar->alloca,
                                (unsigned)vt_idx, llvm_tmp_name(ctx));
                            LLVMValueRef vt_raw = LLVMBuildLoad2(ctx->builder,
                                ctx->type_i8ptr, vt_ptr_field, llvm_tmp_name(ctx));

                            /* Cast to vtable struct pointer */
                            LLVMTypeRef vt_ptr_ty = LLVMPointerType(
                                vt_cls->struct_type, 0);
                            LLVMValueRef vt_typed = LLVMBuildBitCast(
                                ctx->builder, vt_raw, vt_ptr_ty,
                                llvm_tmp_name(ctx));

                            /* GEP to method function pointer */
                            LLVMValueRef fn_ptr_field = LLVMBuildStructGEP2(
                                ctx->builder, vt_cls->struct_type, vt_typed,
                                (unsigned)method_idx, llvm_tmp_name(ctx));

                            size_t argc = node->data.call.arg_count;
                            LLVMTypeRef fn_ptr_ty = vt_cls->fields[method_idx].field_type;
                            LLVMTypeRef fn_type = LLVMGetElementType(fn_ptr_ty);
                            LLVMTypeRef ret_type = LLVMGetReturnType(fn_type);

                            /* Load the function pointer from vtable */
                            LLVMValueRef fn_ptr = LLVMBuildLoad2(ctx->builder,
                                fn_ptr_ty, fn_ptr_field, llvm_tmp_name(ctx));

                            /* Build args: self (party_alloca as i8*) + user args */
                            LLVMValueRef *args = pgy_arena_calloc(&ctx->scratch,
                                (argc + 1) * sizeof(LLVMValueRef));
                            args[0] = LLVMBuildBitCast(ctx->builder,
                                pvar->alloca, ctx->type_i8ptr,
                                llvm_tmp_name(ctx));
                            for (size_t ai = 0; ai < argc; ai++)
                                args[ai + 1] = llvm_emit_expression(
                                    node->data.call.arguments[ai], ctx);

                            LLVMValueRef result;
                            if (ret_type == ctx->type_void) {
                                LLVMBuildCall2(ctx->builder, fn_type, fn_ptr,
                                    args, (unsigned)(argc + 1), "");
                                result = LLVMConstInt(ctx->type_i32, 0, 0);
                            } else {
                                result = LLVMBuildCall2(ctx->builder, fn_type,
                                    fn_ptr, args, (unsigned)(argc + 1),
                                    llvm_tmp_name(ctx));
                            }
                            return result;
                        }
                    }
                }
            }
        }
    }

    if (obj_node != NULL && obj_node->type == AST_IDENTIFIER
        && method_name != NULL
        && (strcmp(method_name, "Write") == 0
            || strcmp(method_name, "Read") == 0
            || strcmp(method_name, "Release") == 0)) {
        const char *slot_name = obj_node->data.identifier.name;
        const char *inner = llvm_lookup_slot_inner(ctx, slot_name);
        bool is_secure = llvm_lookup_slot_is_secure(ctx, slot_name);
        LLVMVarEntry *slot_var = inner != NULL ? llvm_scope_lookup(ctx, slot_name) : NULL;
        if (inner != NULL && slot_var != NULL) {
            if (strcmp(method_name, "Write") == 0 && node->data.call.arg_count >= 1) {
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
                if (val == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, slot_name);
                    if (token_var == NULL)
                        return LLVMConstInt(ctx->type_i32, 0, 0);
                    {
                        char fn_name[64];
                        LLVMFuncEntry *fn;
                        snprintf(fn_name, sizeof(fn_name), "pgy_secure_write_%s", inner);
                        fn = llvm_lookup_function(ctx, fn_name);
                        if (fn != NULL) {
                            LLVMValueRef args[] = { slot_var->alloca, val, token_var->alloca };
                            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
                        } else {
                            llvm_direct_secure_slot_write(ctx, slot_var, val);
                        }
                    }
                } else {
                    char fn_name[64];
                    LLVMFuncEntry *fn;
                    snprintf(fn_name, sizeof(fn_name), "pgy_write_%s", inner);
                    fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef args[] = { slot_var->alloca, val };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                    } else {
                        llvm_direct_slot_write(ctx, slot_var, val);
                    }
                }
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }

            if (strcmp(method_name, "Read") == 0) {
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, slot_name);
                    if (token_var == NULL)
                        return LLVMConstInt(ctx->type_i32, 0, 0);
                    {
                        char fn_name[64];
                        LLVMFuncEntry *fn;
                        snprintf(fn_name, sizeof(fn_name), "pgy_secure_read_%s", inner);
                        fn = llvm_lookup_function(ctx, fn_name);
                        if (fn != NULL) {
                            LLVMValueRef args[] = { slot_var->alloca, token_var->alloca };
                            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                args, 2, llvm_tmp_name(ctx));
                        }
                        return llvm_direct_secure_slot_read(ctx, slot_var, inner);
                    }
                }

                {
                    char fn_name[64];
                    LLVMFuncEntry *fn;
                    snprintf(fn_name, sizeof(fn_name), "pgy_read_%s", inner);
                    fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef args[] = { slot_var->alloca };
                        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, 1, llvm_tmp_name(ctx));
                    }
                    return llvm_direct_slot_read(ctx, slot_var, inner);
                }
            }

            if (strcmp(method_name, "Release") == 0) {
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, slot_name);
                    if (token_var == NULL)
                        return LLVMConstInt(ctx->type_i32, 0, 0);
                    {
                        char fn_name[64];
                        LLVMFuncEntry *fn;
                        snprintf(fn_name, sizeof(fn_name), "pgy_secure_release_%s", inner);
                        fn = llvm_lookup_function(ctx, fn_name);
                        if (fn != NULL) {
                            LLVMValueRef args[] = { slot_var->alloca, token_var->alloca };
                            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                        } else {
                            llvm_direct_secure_slot_release(ctx, slot_var);
                        }
                    }
                } else {
                    char fn_name[64];
                    LLVMFuncEntry *fn;
                    snprintf(fn_name, sizeof(fn_name), "pgy_release_%s", inner);
                    fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef args[] = { slot_var->alloca };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
                    } else {
                        llvm_direct_slot_release(ctx, slot_var);
                    }
                }
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }
        }
    }

    if (obj_node != NULL && method_name != NULL
        && strcmp(method_name, "Slice") == 0
        && node->data.call.arg_count == 2) {
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=begin\n");
        LLVMValueRef receiver = llvm_emit_expression(obj_node, ctx);
        LLVMValueRef start = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef len = llvm_emit_expression(node->data.call.arguments[1], ctx);
        const char *suffix = NULL;
        LLVMTypeRef elem_type = NULL;
        LLVMTypeRef slice_type = NULL;
        LLVMValueRef data_ptr;
        LLVMValueRef start64;
        LLVMValueRef len64;
        LLVMValueRef offset_ptr;
        LLVMValueRef result;
        unsigned field_count = 0;

        if (receiver == NULL || start == NULL || len == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=operands-ready\n");

        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=receiver-type kind=%d\n",
                (int)LLVMGetTypeKind(LLVMTypeOf(receiver)));
        if (LLVMGetTypeKind(LLVMTypeOf(receiver)) == LLVMStructTypeKind)
            field_count = LLVMCountStructElementTypes(LLVMTypeOf(receiver));
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=field-count count=%u\n", field_count);
        data_ptr = llvm_array_data_ptr(ctx, receiver);
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=data-ptr\n");
        if (LLVMGetTypeKind(LLVMTypeOf(data_ptr)) != LLVMPointerTypeKind)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_type = llvm_stmt_resolve_array_elem_type(ctx, obj_node, data_ptr);
        if (elem_type == NULL)
            elem_type = ctx->type_i32;
        suffix = llvm_type_to_suffix(ctx, elem_type);
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=elem ptr-kind=%d elem-kind=%d suffix=%s\n",
                (int)LLVMGetTypeKind(LLVMTypeOf(data_ptr)),
                elem_type != NULL ? (int)LLVMGetTypeKind(elem_type) : -1,
                suffix != NULL ? suffix : "(null)");
        if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
            slice_type = llvm_slice_struct_type(ctx, suffix);
        else
            slice_type = LLVMStructTypeInContext(ctx->context,
                (LLVMTypeRef[]){ LLVMPointerType(elem_type, 0), ctx->type_i64 }, 2, 0);
        start64 = (LLVMTypeOf(start) == ctx->type_i64)
            ? start
            : LLVMBuildSExt(ctx->builder, start, ctx->type_i64, llvm_tmp_name(ctx));
        len64 = (LLVMTypeOf(len) == ctx->type_i64)
            ? len
            : LLVMBuildSExt(ctx->builder, len, ctx->type_i64, llvm_tmp_name(ctx));

        /* Emit slices directly in LLVM IR instead of calling the runtime
         * aggregate-return helper. This keeps Array<T>.Slice(...) and
         * Slice<T>.Slice(...) on the same lowering path and avoids Windows-
         * specific ABI sensitivity around returning PgySlice_* structs from
         * external C helpers. */
        offset_ptr = LLVMBuildGEP2(ctx->builder, elem_type, data_ptr, &start64, 1,
            llvm_tmp_name(ctx));
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=manual-emit\n");
        result = LLVMGetUndef(slice_type);
        result = LLVMBuildInsertValue(ctx->builder, result, offset_ptr, 0, llvm_tmp_name(ctx));
        result = LLVMBuildInsertValue(ctx->builder, result, len64, 1, llvm_tmp_name(ctx));
        return result;
    }

    if (obj_node != NULL && obj_node->type == AST_IDENTIFIER
        && method_name != NULL) {
        if (llvm_is_upper_ident(obj_node)) {
            size_t fn_len = strlen(obj_node->data.identifier.name) + 1 + strlen(method_name) + 1;
            char *full_name = pgy_arena_alloc(&ctx->scratch, fn_len);
            if (full_name == NULL)
