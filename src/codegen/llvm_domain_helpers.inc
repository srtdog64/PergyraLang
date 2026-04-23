static ASTNode *
llvm_find_role_decl(LLVMGenCtx *ctx, const char *role_name)
{
    if (ctx == NULL || role_name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_ROLE_DECL, role_name);
}

static ASTNode *
llvm_find_role_operator_method(LLVMGenCtx *ctx, ASTNode *role,
                               PgyTokenType op, int depth)
{
    if (ctx == NULL || role == NULL || role->type != AST_ROLE_DECL || depth > 16)
        return NULL;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;
        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            ASTNode *method = impl->data.impl_ability.methods[j];
            if (method != NULL && method->type == AST_FUNC_DECL
                && llvm_operator_method_name_matches(op, method->data.func_decl.name)) {
                return method;
            }
        }
    }

    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *inc = role->data.role_decl.includes[i];
        ASTNode *included = llvm_find_role_decl(ctx, inc->data.include_stmt.role_name);
        ASTNode *method = llvm_find_role_operator_method(ctx, included, op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

static ASTNode *
llvm_find_world_state_decl(ASTNode *world_decl, const char *state_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < world_decl->data.world_decl.state_count; i++) {
        ASTNode *state = world_decl->data.world_decl.states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static ASTNode *
llvm_find_world_zone_slot_decl(ASTNode *world_decl, const char *slot_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

static bool
llvm_world_has_zone_slot(ASTNode *world_decl, const char *slot_name)
{
    return llvm_find_world_zone_slot_decl(world_decl, slot_name) != NULL;
}

static void
llvm_domain_decl_parts(ASTNode *stmt,
                       const char **decl_name,
                       ASTNode ***slots,
                       size_t *slot_count,
                       ASTNode ***shared_fields,
                       size_t *shared_count,
                       ASTNode ***methods,
                       size_t *method_count,
                       ASTNode ***refreshes,
                       size_t *refresh_count)
{
    *decl_name = NULL;
    *slots = NULL;
    *slot_count = 0;
    *shared_fields = NULL;
    *shared_count = 0;
    *methods = NULL;
    *method_count = 0;
    *refreshes = NULL;
    *refresh_count = 0;

    if (stmt == NULL)
        return;

    switch (stmt->type) {
    case AST_PARTY_DECL:
        *decl_name = stmt->data.party_decl.name;
        *shared_fields = stmt->data.party_decl.shared_fields;
        *shared_count = stmt->data.party_decl.shared_count;
        *methods = stmt->data.party_decl.methods;
        *method_count = stmt->data.party_decl.method_count;
        break;
    case AST_ROSTER_DECL:
        *decl_name = stmt->data.roster_decl.name;
        *shared_fields = stmt->data.roster_decl.shared_fields;
        *shared_count = stmt->data.roster_decl.shared_count;
        *methods = stmt->data.roster_decl.methods;
        *method_count = stmt->data.roster_decl.method_count;
        break;
    case AST_WORLD_DECL:
        *decl_name = stmt->data.world_decl.name;
        *shared_fields = stmt->data.world_decl.shared_fields;
        *shared_count = stmt->data.world_decl.shared_count;
        *methods = stmt->data.world_decl.methods;
        *method_count = stmt->data.world_decl.method_count;
        break;
    case AST_RELATION_DECL:
        *decl_name = stmt->data.relation_decl.name;
        *slots = stmt->data.relation_decl.slots;
        *slot_count = stmt->data.relation_decl.slot_count;
        *shared_fields = stmt->data.relation_decl.shared_fields;
        *shared_count = stmt->data.relation_decl.shared_count;
        *methods = stmt->data.relation_decl.methods;
        *method_count = stmt->data.relation_decl.method_count;
        *refreshes = stmt->data.relation_decl.refreshes;
        *refresh_count = stmt->data.relation_decl.refresh_count;
        break;
    case AST_EFFECT_DECL:
        *decl_name = stmt->data.effect_decl.name;
        *slots = stmt->data.effect_decl.slots;
        *slot_count = stmt->data.effect_decl.slot_count;
        *shared_fields = stmt->data.effect_decl.shared_fields;
        *shared_count = stmt->data.effect_decl.shared_count;
        *methods = stmt->data.effect_decl.methods;
        *method_count = stmt->data.effect_decl.method_count;
        *refreshes = stmt->data.effect_decl.refreshes;
        *refresh_count = stmt->data.effect_decl.refresh_count;
        break;
    case AST_ZONE_DECL:
        *decl_name = stmt->data.zone_decl.name;
        *slots = stmt->data.zone_decl.slots;
        *slot_count = stmt->data.zone_decl.slot_count;
        *shared_fields = stmt->data.zone_decl.shared_fields;
        *shared_count = stmt->data.zone_decl.shared_count;
        *methods = stmt->data.zone_decl.methods;
        *method_count = stmt->data.zone_decl.method_count;
        *refreshes = stmt->data.zone_decl.refreshes;
        *refresh_count = stmt->data.zone_decl.refresh_count;
        break;
    default:
        break;
    }
}

static bool
llvm_domain_slot_is_projection_target(ASTNode *slot,
                                      ASTNode **refreshes,
                                      size_t refresh_count);

static size_t
llvm_count_domain_projection_slots(ASTNode **slots, size_t slot_count,
                                   ASTNode **refreshes, size_t refresh_count)
{
    size_t projection_count = 0;

    if (slots == NULL)
        return 0;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && (slot->data.domain_slot.is_tobject
                || llvm_domain_slot_is_projection_target(slot, refreshes, refresh_count))) {
            projection_count++;
        }
    }

    return projection_count;
}

static bool
llvm_domain_slot_is_projection_target(ASTNode *slot,
                                      ASTNode **refreshes,
                                      size_t refresh_count)
{
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
        || slot->data.domain_slot.slot_name == NULL) {
        return false;
    }

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH
            || refresh->data.zone_refresh.object_slot_name == NULL) {
            continue;
        }
        if (strcmp(slot->data.domain_slot.slot_name,
                   refresh->data.zone_refresh.object_slot_name) == 0) {
            return true;
        }
    }

    return false;
}

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

static LLVMValueRef
llvm_build_domain_projection_value(LLVMGenCtx *ctx,
                                   LLVMClassTypeEntry *target_cls,
                                   LLVMClassTypeEntry *source_cls,
                                   ASTNode *source_decl,
                                   ASTNode *refresh,
                                   LLVMValueRef source_ptr);

static ASTNode *
llvm_find_projection_nominal_decl_local(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, name);
}

static size_t
llvm_projection_field_count_local(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.field_count;
    return 0;
}

static ClassField *
llvm_projection_field_at_local(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        if (index < decl->data.class_decl.field_count)
            return decl->data.class_decl.fields[index];
        return NULL;
    }
    return NULL;
}

static int
llvm_resolve_projection_source_path_rec(LLVMGenCtx *ctx, ASTNode *source_decl,
                                        const char *field_name, unsigned depth,
                                        char **path_out)
{
    size_t field_count;
    int match_count = 0;
    char *resolved_path = NULL;

    if (path_out != NULL)
        *path_out = NULL;
    if (ctx == NULL || source_decl == NULL || field_name == NULL || depth > 8)
        return 0;

    field_count = llvm_projection_field_count_local(source_decl);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = llvm_projection_field_at_local(source_decl, i);
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            if (path_out != NULL)
                *path_out = pergyra_strdup(field_name);
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = llvm_projection_field_at_local(source_decl, i);
        ASTNode *vessel_decl;
        char *nested_path = NULL;
        char *prefixed_path;
        size_t prefix_len;
        int nested_status;

        if (field == NULL || !field->is_vessel_field
            || field->type == NULL || field->type->type != AST_TYPE
            || field->type->data.type.name == NULL) {
            continue;
        }

        vessel_decl = llvm_find_projection_nominal_decl_local(
            ctx, field->type->data.type.name);
        if (vessel_decl == NULL || vessel_decl->type != AST_CLASS_DECL
            || vessel_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = llvm_resolve_projection_source_path_rec(
            ctx, vessel_decl, field_name, depth + 1, &nested_path);
        if (nested_status != 1) {
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        prefix_len = strlen(field->name) + strlen(nested_path) + 2;
        prefixed_path = pgy_arena_alloc(&ctx->scratch, prefix_len);
        if (prefixed_path != NULL)
            snprintf(prefixed_path, prefix_len, "%s.%s", field->name, nested_path);
        if (prefixed_path == NULL)
            continue;

        match_count++;
        if (match_count == 1) {
            resolved_path = prefixed_path;
        } else {
            resolved_path = NULL;
        }
    }

    if (match_count == 1) {
        if (path_out != NULL)
            *path_out = resolved_path;
        return 1;
    }

    return match_count > 1 ? 2 : 0;
}

static LLVMValueRef
llvm_load_projection_path_value(LLVMGenCtx *ctx,
                                ASTNode *source_decl,
                                LLVMClassTypeEntry *source_cls,
                                LLVMValueRef source_ptr,
                                const char *field_name)
{
    char *path = NULL;
    char *cursor;
    ASTNode *current_decl;
    LLVMClassTypeEntry *current_cls;
    LLVMValueRef current_ptr;

    if (llvm_resolve_projection_source_path_rec(ctx, source_decl, field_name, 0, &path) != 1
        || path == NULL || source_cls == NULL || source_ptr == NULL) {
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    current_decl = source_decl;
    current_cls = source_cls;
    current_ptr = source_ptr;
    cursor = path;
    while (cursor != NULL && *cursor != '\0') {
        char *dot = strchr(cursor, '.');
        char *segment = cursor;
        int field_index;
        LLVMClassFieldInfo *field_info = NULL;
        LLVMValueRef field_ptr;

        if (dot != NULL)
            *dot = '\0';

        field_index = llvm_class_field_index(current_cls, segment);
        if (field_index < 0)
            break;
        for (int i = 0; i < current_cls->field_count; i++) {
            if (current_cls->fields[i].index == field_index) {
                field_info = &current_cls->fields[i];
                break;
            }
        }
        if (field_info == NULL || field_info->field_type == NULL)
            break;

        field_ptr = LLVMBuildStructGEP2(ctx->builder, current_cls->struct_type,
            current_ptr, (unsigned)field_index, llvm_tmp_name(ctx));
        if (dot == NULL) {
            LLVMValueRef field_value = LLVMBuildLoad2(ctx->builder, field_info->field_type,
                field_ptr, llvm_tmp_name(ctx));
            return field_value;
        }

        for (size_t i = 0; i < llvm_projection_field_count_local(current_decl); i++) {
            ClassField *field = llvm_projection_field_at_local(current_decl, i);
            if (field == NULL || field->name == NULL
                || strcmp(field->name, segment) != 0
                || field->type == NULL || field->type->type != AST_TYPE
                || field->type->data.type.name == NULL) {
                continue;
            }
            current_decl = llvm_find_projection_nominal_decl_local(
                ctx, field->type->data.type.name);
            current_cls = llvm_lookup_class(ctx, field->type->data.type.name);
            current_ptr = field_ptr;
            break;
        }
        cursor = dot + 1;
    }

    return LLVMConstInt(ctx->type_i32, 0, 0);
}

static LLVMValueRef
llvm_build_domain_projection_value(LLVMGenCtx *ctx,
                                   LLVMClassTypeEntry *target_cls,
                                   LLVMClassTypeEntry *source_cls,
                                   ASTNode *source_decl,
                                   ASTNode *refresh,
                                   LLVMValueRef source_ptr)
{
    LLVMValueRef projected;

    if (ctx == NULL || target_cls == NULL || source_cls == NULL || source_ptr == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    projected = LLVMConstNull(target_cls->struct_type);
    for (int i = 0; i < target_cls->field_count; i++) {
        LLVMClassFieldInfo *target_field = &target_cls->fields[i];
        const char *source_field_name = target_field->field_name;
        LLVMValueRef field_value;

        if (target_field->field_name == NULL)
            continue;

        if (refresh != NULL && refresh->type == AST_ZONE_REFRESH) {
            for (size_t j = 0; j < refresh->data.zone_refresh.field_map_count; j++) {
                const char *mapped_target =
                    refresh->data.zone_refresh.mapped_target_fields[j];
                const char *mapped_source =
                    refresh->data.zone_refresh.mapped_source_fields[j];
                if (mapped_target != NULL && mapped_source != NULL
                    && strcmp(mapped_target, target_field->field_name) == 0) {
                    source_field_name = mapped_source;
                    break;
                }
            }
        }

        field_value = llvm_load_projection_path_value(ctx, source_decl, source_cls,
            source_ptr, source_field_name);
        projected = LLVMBuildInsertValue(ctx->builder, projected, field_value,
            (unsigned)target_field->index, llvm_tmp_name(ctx));
    }

    return projected;
}

static void
llvm_emit_domain_projection_sync_body(ASTNode *stmt,
                                      LLVMClassTypeEntry *decl_cls,
                                      LLVMValueRef sync_fn,
                                      LLVMGenCtx *ctx)
{
    ASTNode **refreshes = NULL;
    size_t refresh_count = 0;
    const char *unused_name = NULL;
    ASTNode **slots = NULL;
    size_t slot_count = 0;
    ASTNode **unused_shared = NULL;
    size_t unused_shared_count = 0;
    ASTNode **unused_methods = NULL;
    size_t unused_method_count = 0;

    llvm_domain_decl_parts(stmt, &unused_name, &slots, &slot_count,
        &unused_shared, &unused_shared_count, &unused_methods, &unused_method_count,
        &refreshes, &refresh_count);

    if (stmt == NULL || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        LLVMClassTypeEntry *target_cls;
        LLVMClassTypeEntry *source_cls;
        int target_index;
        int source_index;
        int dirty_index;
        int ready_index;
        LLVMValueRef self_ptr;
        const char *target_slot_name;
        const char *source_slot_name;
        const char *target_type_name;
        const char *source_type_name;
        ASTNode *target_slot_decl = NULL;
        ASTNode *source_slot_decl = NULL;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;

        target_slot_name = refresh->data.zone_refresh.object_slot_name;
        source_slot_name = refresh->data.zone_refresh.source_slot_name;
        if (target_slot_name == NULL || source_slot_name == NULL)
            continue;

        for (size_t j = 0; j < slot_count; j++) {
            ASTNode *slot = slots[j];
            if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
                continue;
            if (slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, target_slot_name) == 0) {
                target_slot_decl = slot;
            }
            if (slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, source_slot_name) == 0) {
                source_slot_decl = slot;
            }
        }
        if (target_slot_decl == NULL || source_slot_decl == NULL
            || target_slot_decl->data.domain_slot.type == NULL
            || source_slot_decl->data.domain_slot.type == NULL
            || target_slot_decl->data.domain_slot.type->type != AST_TYPE
            || source_slot_decl->data.domain_slot.type->type != AST_TYPE)
            continue;

        target_type_name = target_slot_decl->data.domain_slot.type->data.type.name;
        source_type_name = source_slot_decl->data.domain_slot.type->data.type.name;
        if (target_type_name == NULL || source_type_name == NULL)
            continue;

        target_cls = llvm_lookup_class(ctx, target_type_name);
        source_cls = llvm_lookup_class(ctx, source_type_name);
        target_index = llvm_class_field_index(decl_cls, target_slot_name);
        source_index = llvm_class_field_index(decl_cls, source_slot_name);
        {
            char field_name[256];
            snprintf(field_name, sizeof(field_name), "__projection_dirty_%s",
                target_slot_name);
            dirty_index = llvm_class_field_index(decl_cls, field_name);
            snprintf(field_name, sizeof(field_name), "__projection_ready_%s",
                target_slot_name);
            ready_index = llvm_class_field_index(decl_cls, field_name);
        }
        if (target_cls == NULL || source_cls == NULL
            || target_index < 0 || source_index < 0 || dirty_index < 0)
            continue;

        self_ptr = LLVMGetParam(sync_fn, 0);
        {
            LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                decl_cls->struct_type, self_ptr, (unsigned)dirty_index,
                llvm_tmp_name(ctx));
            LLVMValueRef dirty_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                dirty_ptr, llvm_tmp_name(ctx));
            LLVMBasicBlockRef sync_bb = LLVMAppendBasicBlockInContext(ctx->context,
                sync_fn, "projection.sync");
            LLVMBasicBlockRef cont_bb = LLVMAppendBasicBlockInContext(ctx->context,
                sync_fn, "projection.cont");

            LLVMBuildCondBr(ctx->builder, dirty_val, sync_bb, cont_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, sync_bb);
            {
                LLVMValueRef target_ptr = LLVMBuildStructGEP2(ctx->builder,
                    decl_cls->struct_type, self_ptr, (unsigned)target_index,
                    llvm_tmp_name(ctx));
                LLVMValueRef source_ptr = LLVMBuildStructGEP2(ctx->builder,
                    decl_cls->struct_type, self_ptr, (unsigned)source_index,
                    llvm_tmp_name(ctx));
                LLVMValueRef projected;

                if (ready_index >= 0) {
                    LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                        decl_cls->struct_type, self_ptr, (unsigned)ready_index,
                        llvm_tmp_name(ctx));
                    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0),
                        ready_ptr);
                }

                projected = llvm_build_domain_projection_value(ctx, target_cls,
                    source_cls, llvm_find_projection_nominal_decl_local(ctx, source_type_name),
                    refresh, source_ptr);
                LLVMBuildStore(ctx->builder, projected, target_ptr);

                if (ready_index >= 0) {
                    LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                        decl_cls->struct_type, self_ptr, (unsigned)ready_index,
                        llvm_tmp_name(ctx));
                    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                        ready_ptr);
                }
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0),
                    dirty_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr,
                    "projection", target_slot_name, PGY_PROP_CAUSE_REFRESH);
            }
            LLVMBuildBr(ctx->builder, cont_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
        }
    }
}

static void
llvm_emit_domain_projection_sync(ASTNode *stmt,
                                 const char *decl_name,
                                 LLVMClassTypeEntry *decl_cls,
                                 LLVMValueRef sync_fn,
                                 LLVMGenCtx *ctx)
{
    LLVMValueRef saved_fn;
    LLVMTypeRef saved_ret;
    ASTNode *saved_host_decl;
    LLVMBasicBlockRef bb;

    if (stmt == NULL || decl_name == NULL || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    saved_fn = ctx->current_function;
    saved_ret = ctx->current_ret_type;
    saved_host_decl = llvm_bind_current_host_decl(ctx, stmt);
    bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb);
    ctx->current_function = sync_fn;
    ctx->current_ret_type = ctx->type_void;

    llvm_scope_push(ctx);
    {
        LLVMTypeRef self_ptr_t = LLVMPointerType(decl_cls->struct_type, 0);
        LLVMValueRef sa = llvm_create_entry_alloca(ctx, self_ptr_t, "self.addr");
        LLVMBuildStore(ctx->builder, LLVMGetParam(sync_fn, 0), sa);
        llvm_scope_declare(ctx, "self", sa, self_ptr_t);
        llvm_register_var_class(ctx, "self", decl_name);
    }

    llvm_emit_domain_projection_sync_body(stmt, decl_cls, sync_fn, ctx);

    LLVMBuildRetVoid(ctx->builder);
    llvm_scope_pop(ctx);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    llvm_restore_current_host_decl(ctx, saved_host_decl);

    if (saved_fn != NULL) {
        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
        if (last != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last);
    }
}
