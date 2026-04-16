static LLVMValueRef
llvm_emit_call(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.call.callee == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    /* Method call: obj.method(args) */
    if (node->data.call.callee->type == AST_MEMBER_ACCESS)
        return llvm_emit_member_call(node, ctx);

    /* Get callee name */
    const char *callee_name = NULL;
    if (node->data.call.callee->type == AST_IDENTIFIER)
        callee_name = node->data.call.callee->data.identifier.name;

    if (callee_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (strcmp(callee_name, "Clone") == 0 && node->data.call.arg_count == 1) {
        return llvm_emit_expression(node->data.call.arguments[0], ctx);
    }

    {
        LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, callee_name);
        if (variant != NULL) {
            ASTNode *enum_decl = llvm_find_enum_decl(ctx, variant->enum_name);
            LLVMClassTypeEntry *enum_cls = llvm_lookup_class(ctx, variant->enum_name);
            if (enum_decl != NULL && enum_cls != NULL) {
                size_t variant_index = (size_t)variant->value;
                size_t param_count =
                    (enum_decl->data.enum_decl.variant_param_counts != NULL)
                    ? enum_decl->data.enum_decl.variant_param_counts[variant_index] : 0;
                LLVMValueRef enum_val = LLVMGetUndef(enum_cls->struct_type);
                enum_val = LLVMBuildInsertValue(ctx->builder, enum_val,
                    LLVMConstInt(ctx->type_i32,
                        (unsigned long long)variant->value, 0),
                    0, llvm_tmp_name(ctx));

                if (param_count > 0) {
                    int field_idx = llvm_class_field_index(enum_cls, callee_name);
                    if (field_idx > 0) {
                        LLVMTypeRef payload_ty = enum_cls->fields[field_idx].field_type;
                        LLVMValueRef payload = LLVMGetUndef(payload_ty);
                        LLVMClassTypeEntry *payload_cls =
                            llvm_lookup_class_by_type(ctx, payload_ty);

                        for (size_t i = 0; i < param_count
                             && i < node->data.call.arg_count; i++) {
                            LLVMValueRef arg = llvm_emit_expression(
                                node->data.call.arguments[i], ctx);
                            if (arg == NULL)
                                continue;
                            if (payload_cls != NULL
                                && i < (size_t)payload_cls->field_count
                                && payload_cls->fields[i].field_type != LLVMTypeOf(arg)) {
                                LLVMTypeRef target_ty = payload_cls->fields[i].field_type;
                                if ((target_ty == ctx->type_i32 || target_ty == ctx->type_i64)
                                    && (LLVMTypeOf(arg) == ctx->type_i32
                                        || LLVMTypeOf(arg) == ctx->type_i64)) {
                                    arg = (LLVMGetIntTypeWidth(target_ty)
                                        > LLVMGetIntTypeWidth(LLVMTypeOf(arg)))
                                        ? LLVMBuildSExt(ctx->builder, arg, target_ty,
                                            llvm_tmp_name(ctx))
                                        : LLVMBuildTrunc(ctx->builder, arg, target_ty,
                                            llvm_tmp_name(ctx));
                                }
                            }
                            payload = LLVMBuildInsertValue(ctx->builder, payload, arg,
                                (unsigned)i, llvm_tmp_name(ctx));
                        }
                        enum_val = LLVMBuildInsertValue(ctx->builder, enum_val,
                            payload, (unsigned)field_idx, llvm_tmp_name(ctx));
                    }
                }
                return enum_val;
            }
        }
    }

    {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, callee_name);
        if (cls != NULL) {
            LLVMValueRef object = LLVMConstNull(cls->struct_type);
            for (size_t i = 0; i < node->data.call.arg_count
                 && i < (size_t)cls->field_count; i++) {
                LLVMValueRef arg = llvm_emit_expression(
                    node->data.call.arguments[i], ctx);
                if (arg == NULL)
                    continue;
                object = LLVMBuildInsertValue(ctx->builder, object, arg,
                    (unsigned)cls->fields[i].index, llvm_tmp_name(ctx));
            }
            {
                ASTNode *party_decl = llvm_find_named_domain_decl(ctx, AST_PARTY_DECL,
                    callee_name);
                ASTNode *roster_decl = llvm_find_named_domain_decl(ctx, AST_ROSTER_DECL,
                    callee_name);
                ASTNode *relation_decl = llvm_find_named_domain_decl(ctx, AST_RELATION_DECL,
                    callee_name);
                ASTNode *effect_decl = llvm_find_named_domain_decl(ctx, AST_EFFECT_DECL,
                    callee_name);
                ASTNode *zone_decl = llvm_find_named_domain_decl(ctx, AST_ZONE_DECL,
                    callee_name);
                ASTNode *world_decl = llvm_find_named_domain_decl(ctx, AST_WORLD_DECL,
                    callee_name);
                ASTNode **shared_fields = NULL;
                size_t shared_count = 0;
                if (party_decl != NULL) {
                    shared_fields = party_decl->data.party_decl.shared_fields;
                    shared_count = party_decl->data.party_decl.shared_count;
                } else if (roster_decl != NULL) {
                    shared_fields = roster_decl->data.roster_decl.shared_fields;
                    shared_count = roster_decl->data.roster_decl.shared_count;
                } else if (relation_decl != NULL) {
                    shared_fields = relation_decl->data.relation_decl.shared_fields;
                    shared_count = relation_decl->data.relation_decl.shared_count;
                } else if (effect_decl != NULL) {
                    shared_fields = effect_decl->data.effect_decl.shared_fields;
                    shared_count = effect_decl->data.effect_decl.shared_count;
                } else if (zone_decl != NULL) {
                    shared_fields = zone_decl->data.zone_decl.shared_fields;
                    shared_count = zone_decl->data.zone_decl.shared_count;
                } else if (world_decl != NULL) {
                    shared_fields = world_decl->data.world_decl.shared_fields;
                    shared_count = world_decl->data.world_decl.shared_count;
                }
                for (size_t i = 0; i < shared_count; i++) {
                    ASTNode *shared = shared_fields[i];
                    int field_idx;
                    LLVMValueRef init_val;
                    if (shared == NULL || shared->data.party_shared.name == NULL
                        || shared->data.party_shared.initializer == NULL) {
                        continue;
                    }
                    field_idx = llvm_class_field_index(cls, shared->data.party_shared.name);
                    if (field_idx < 0 || (size_t)field_idx < node->data.call.arg_count)
                        continue;
                    init_val = llvm_emit_expression(shared->data.party_shared.initializer, ctx);
                    if (init_val == NULL)
                        continue;
                    object = LLVMBuildInsertValue(ctx->builder, object, init_val,
                        (unsigned)field_idx, llvm_tmp_name(ctx));
                }
                if (relation_decl != NULL || effect_decl != NULL || zone_decl != NULL) {
                    ASTNode **slots = NULL;
                    size_t slot_count = 0;
                    ASTNode **refreshes = NULL;
                    size_t refresh_count = 0;

                    if (relation_decl != NULL) {
                        slots = relation_decl->data.relation_decl.slots;
                        slot_count = relation_decl->data.relation_decl.slot_count;
                        refreshes = relation_decl->data.relation_decl.refreshes;
                        refresh_count = relation_decl->data.relation_decl.refresh_count;
                    } else if (effect_decl != NULL) {
                        slots = effect_decl->data.effect_decl.slots;
                        slot_count = effect_decl->data.effect_decl.slot_count;
                        refreshes = effect_decl->data.effect_decl.refreshes;
                        refresh_count = effect_decl->data.effect_decl.refresh_count;
                    } else if (zone_decl != NULL) {
                        slots = zone_decl->data.zone_decl.slots;
                        slot_count = zone_decl->data.zone_decl.slot_count;
                        refreshes = zone_decl->data.zone_decl.refreshes;
                        refresh_count = zone_decl->data.zone_decl.refresh_count;
                    }

                    for (size_t i = 0; i < slot_count; i++) {
                        ASTNode *slot = slots != NULL ? slots[i] : NULL;
                        const char *slot_name = slot != NULL
                            ? slot->data.domain_slot.slot_name
                            : NULL;
                        bool projection_slot = false;

                        if (slot == NULL || slot->type != AST_DOMAIN_SLOT || slot_name == NULL)
                            continue;

                        if (slot->data.domain_slot.is_tobject) {
                            projection_slot = true;
                        } else {
                            for (size_t ri = 0; ri < refresh_count; ri++) {
                                ASTNode *refresh = refreshes != NULL ? refreshes[ri] : NULL;
                                if (refresh == NULL
                                    || refresh->type != AST_ZONE_REFRESH
                                    || refresh->data.zone_refresh.object_slot_name == NULL) {
                                    continue;
                                }
                                if (strcmp(slot_name,
                                           refresh->data.zone_refresh.object_slot_name) == 0) {
                                    projection_slot = true;
                                    break;
                                }
                            }
                        }

                        if (projection_slot) {
                            char dirty_field[256];
                            int dirty_idx;

                            snprintf(dirty_field, sizeof(dirty_field),
                                "__projection_dirty_%s", slot_name);
                            dirty_idx = llvm_class_field_index(cls, dirty_field);
                            if (dirty_idx < 0)
                                continue;
                            object = LLVMBuildInsertValue(ctx->builder, object,
                                LLVMConstInt(ctx->type_i1, 1, 0),
                                (unsigned)dirty_idx, llvm_tmp_name(ctx));
                        }
                    }
                }
                if (world_decl != NULL && world_decl->type == AST_WORLD_DECL) {
                    int derived_idx = llvm_class_field_index(cls, "__world_derived_dirty");
                    if (derived_idx >= 0) {
                        object = LLVMBuildInsertValue(ctx->builder, object,
                            LLVMConstInt(ctx->type_i1, 1, 0),
                            (unsigned)derived_idx, llvm_tmp_name(ctx));
                    }
                    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
                        ASTNode *zone = world_decl->data.world_decl.zones[i];
                        char dirty_field[256];
                        int dirty_idx;
                        const char *slot_name = zone != NULL
                            ? zone->data.world_zone.slot_name
                            : NULL;
                        if (slot_name == NULL)
                            continue;
                        snprintf(dirty_field, sizeof(dirty_field), "__zone_dirty_%s", slot_name);
                        dirty_idx = llvm_class_field_index(cls, dirty_field);
                        if (dirty_idx < 0)
                            continue;
                        object = LLVMBuildInsertValue(ctx->builder, object,
                            LLVMConstInt(ctx->type_i1, 1, 0),
                            (unsigned)dirty_idx, llvm_tmp_name(ctx));
                    }
                }
            }
            return object;
        }
    }

    if ((strcmp(callee_name, "ToTObject") == 0 || strcmp(callee_name, "ToObject") == 0)
        && node->data.call.arg_count == 2) {
        return llvm_emit_subject_projection(node, ctx);
    }

    if (strcmp(callee_name, "HasProjection") == 0 && node->data.call.arg_count == 1) {
        ASTNode *decl = llvm_find_named_domain_decl(ctx, AST_RELATION_DECL,
            ctx->current_class_name);
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, ctx->current_class_name);
        const char *slot_name = NULL;
        int field_idx;
        LLVMValueRef base_ptr;
        LLVMValueRef gep;
        if (decl == NULL) {
            decl = llvm_find_named_domain_decl(ctx, AST_EFFECT_DECL,
                ctx->current_class_name);
        }
        if (decl == NULL) {
            decl = llvm_find_named_domain_decl(ctx, AST_ZONE_DECL,
                ctx->current_class_name);
        }
        if (decl == NULL || cls == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        if (node->data.call.arguments[0]->type == AST_IDENTIFIER)
            slot_name = node->data.call.arguments[0]->data.identifier.name;
        else if (node->data.call.arguments[0]->type == AST_STRING)
            slot_name = node->data.call.arguments[0]->data.string.value;
        if (slot_name == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        {
            char field_name[256];
            snprintf(field_name, sizeof(field_name), "__projection_ready_%s", slot_name);
            field_idx = llvm_class_field_index(cls, field_name);
        }
        if (field_idx < 0)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        base_ptr = llvm_current_self_base_ptr(ctx, cls);
        if (base_ptr == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
            (unsigned)field_idx, llvm_tmp_name(ctx));
        return LLVMBuildLoad2(ctx->builder, ctx->type_i1, gep, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "HasLayer") == 0 && node->data.call.arg_count == 1) {
        ASTNode *zone_decl = llvm_find_named_domain_decl(ctx, AST_ZONE_DECL,
            ctx->current_class_name);
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, ctx->current_class_name);
        const char *layer_name = NULL;
        int field_idx;
        LLVMValueRef base_ptr;
        LLVMValueRef gep;
        if (zone_decl == NULL || cls == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        if (node->data.call.arguments[0]->type == AST_IDENTIFIER)
            layer_name = node->data.call.arguments[0]->data.identifier.name;
        else if (node->data.call.arguments[0]->type == AST_STRING)
            layer_name = node->data.call.arguments[0]->data.string.value;
        if (layer_name == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        {
            char field_name[256];
            snprintf(field_name, sizeof(field_name), "__layer_active_%s", layer_name);
            field_idx = llvm_class_field_index(cls, field_name);
        }
        if (field_idx < 0)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        base_ptr = llvm_current_self_base_ptr(ctx, cls);
        if (base_ptr == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
            (unsigned)field_idx, llvm_tmp_name(ctx));
        return LLVMBuildLoad2(ctx->builder, ctx->type_i1, gep, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "HasState") == 0 && node->data.call.arg_count >= 1) {
        ASTNode *zone_decl = llvm_find_named_domain_decl(ctx, AST_ZONE_DECL,
            ctx->current_class_name);
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, ctx->current_class_name);
        const char *state_name = NULL;
        ASTNode *state_decl;
        int field_idx;
        LLVMValueRef base_ptr;
        LLVMValueRef gep;
        if (zone_decl == NULL || cls == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        if (node->data.call.arguments[0]->type == AST_IDENTIFIER)
            state_name = node->data.call.arguments[0]->data.identifier.name;
        else if (node->data.call.arguments[0]->type == AST_STRING)
            state_name = node->data.call.arguments[0]->data.string.value;
        state_decl = llvm_find_zone_state_decl(ctx, zone_decl, state_name);
        if (state_decl == NULL || state_name == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        {
            char field_name[256];
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
            field_idx = llvm_class_field_index(cls, field_name);
        }
        if (field_idx < 0)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        base_ptr = llvm_current_self_base_ptr(ctx, cls);
        if (base_ptr == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
            (unsigned)field_idx, llvm_tmp_name(ctx));
        return LLVMBuildLoad2(ctx->builder, ctx->type_i1, gep, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "HasZone") == 0 && node->data.call.arg_count == 1) {
        ASTNode *world_decl = llvm_find_named_domain_decl(ctx, AST_WORLD_DECL,
            ctx->current_class_name);
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, ctx->current_class_name);
        const char *name = NULL;
        ASTNode *state_decl = NULL;
        int field_idx = -1;
        LLVMValueRef base_ptr;
        LLVMValueRef gep;
        if (world_decl == NULL || cls == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        if (node->data.call.arguments[0]->type == AST_IDENTIFIER)
            name = node->data.call.arguments[0]->data.identifier.name;
        else if (node->data.call.arguments[0]->type == AST_STRING)
            name = node->data.call.arguments[0]->data.string.value;
        if (name == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        state_decl = llvm_find_world_state_decl(ctx, world_decl, name);
        if (state_decl != NULL) {
            if (state_decl->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
                || state_decl->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
                LLVMValueRef result = LLVMConstInt(ctx->type_i1,
                    state_decl->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL ? 1 : 0, 0);
                base_ptr = llvm_current_self_base_ptr(ctx, cls);
                if (base_ptr == NULL)
                    return LLVMConstInt(ctx->type_i1, 0, 0);
                for (size_t i = 0; i < state_decl->data.world_state.input_count; i++) {
                    const char *input_name = state_decl->data.world_state.input_names[i];
                    int input_idx = -1;
                    char field_name[256];
                    LLVMValueRef input_ptr;
                    LLVMValueRef input_val;
                    if (input_name == NULL)
                        continue;
                    if (llvm_world_has_zone_slot(world_decl, input_name)) {
                        snprintf(field_name, sizeof(field_name), "__zone_active_%s", input_name);
                    } else {
                        snprintf(field_name, sizeof(field_name), "__zone_state_%s", input_name);
                    }
                    input_idx = llvm_class_field_index(cls, field_name);
                    if (input_idx < 0)
                        continue;
                    input_ptr = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
                        (unsigned)input_idx, llvm_tmp_name(ctx));
                    input_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                        input_ptr, llvm_tmp_name(ctx));
                    if (state_decl->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL)
                        result = LLVMBuildAnd(ctx->builder, result, input_val, llvm_tmp_name(ctx));
                    else
                        result = LLVMBuildOr(ctx->builder, result, input_val, llvm_tmp_name(ctx));
                }
                return result;
            }
            char field_name[256];
            snprintf(field_name, sizeof(field_name), "__zone_state_%s", name);
            field_idx = llvm_class_field_index(cls, field_name);
        } else if (llvm_world_has_zone_slot(world_decl, name)) {
            char field_name[256];
            snprintf(field_name, sizeof(field_name), "__zone_active_%s", name);
            field_idx = llvm_class_field_index(cls, field_name);
        }
        if (field_idx < 0)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        base_ptr = llvm_current_self_base_ptr(ctx, cls);
        if (base_ptr == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
            (unsigned)field_idx, llvm_tmp_name(ctx));
        return LLVMBuildLoad2(ctx->builder, ctx->type_i1, gep, llvm_tmp_name(ctx));
    }

    if ((strcmp(callee_name, "HasZoneProjection") == 0
         || strcmp(callee_name, "HasZoneLayer") == 0
         || strcmp(callee_name, "HasZoneState") == 0)
        && node->data.call.arg_count == 2) {
        ASTNode *world_decl = llvm_find_named_domain_decl(ctx, AST_WORLD_DECL,
            ctx->current_class_name);
        LLVMClassTypeEntry *world_cls = llvm_lookup_class(ctx, ctx->current_class_name);
        const char *zone_name = NULL;
        const char *detail_name = NULL;
        ASTNode *zone_decl;
        LLVMClassTypeEntry *zone_cls;
        int zone_idx;
        int field_idx = -1;
        LLVMValueRef world_ptr;
        LLVMValueRef zone_ptr;
        LLVMValueRef gep;

        if (world_decl == NULL || world_cls == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        if (node->data.call.arguments[0]->type == AST_IDENTIFIER)
            zone_name = node->data.call.arguments[0]->data.identifier.name;
        else if (node->data.call.arguments[0]->type == AST_STRING)
            zone_name = node->data.call.arguments[0]->data.string.value;
        if (node->data.call.arguments[1]->type == AST_IDENTIFIER)
            detail_name = node->data.call.arguments[1]->data.identifier.name;
        else if (node->data.call.arguments[1]->type == AST_STRING)
            detail_name = node->data.call.arguments[1]->data.string.value;
        if (zone_name == NULL || detail_name == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);

        zone_decl = llvm_resolve_world_zone_decl(ctx, world_decl, zone_name);
        zone_cls = zone_decl != NULL && zone_decl->data.zone_decl.name != NULL
            ? llvm_lookup_class(ctx, zone_decl->data.zone_decl.name)
            : NULL;
        zone_idx = llvm_class_field_index(world_cls, zone_name);
        if (zone_decl == NULL || zone_cls == NULL || zone_idx < 0)
            return LLVMConstInt(ctx->type_i1, 0, 0);

        if (strcmp(callee_name, "HasZoneProjection") == 0) {
            ASTNode *slot = llvm_find_zone_domain_slot_decl(zone_decl, detail_name);
            if (slot != NULL && !slot->data.domain_slot.is_subject) {
                char field_name[256];
                snprintf(field_name, sizeof(field_name), "__projection_ready_%s", detail_name);
                field_idx = llvm_class_field_index(zone_cls, field_name);
            }
        } else if (strcmp(callee_name, "HasZoneLayer") == 0) {
            if (llvm_find_zone_layer_slot_decl(zone_decl, detail_name) != NULL) {
                char field_name[256];
                snprintf(field_name, sizeof(field_name), "__layer_active_%s", detail_name);
                field_idx = llvm_class_field_index(zone_cls, field_name);
            }
        } else {
            if (llvm_find_zone_state_decl(ctx, zone_decl, detail_name) != NULL) {
                char field_name[256];
                snprintf(field_name, sizeof(field_name), "__state_%s", detail_name);
                field_idx = llvm_class_field_index(zone_cls, field_name);
            }
        }

        if (field_idx < 0)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        world_ptr = llvm_current_self_base_ptr(ctx, world_cls);
        if (world_ptr == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        zone_ptr = LLVMBuildStructGEP2(ctx->builder, world_cls->struct_type, world_ptr,
            (unsigned)zone_idx, llvm_tmp_name(ctx));
        gep = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type, zone_ptr,
            (unsigned)field_idx, llvm_tmp_name(ctx));
        return LLVMBuildLoad2(ctx->builder, ctx->type_i1, gep, llvm_tmp_name(ctx));
    }

    /* Built-in: Log */
    if (strcmp(callee_name, "Log") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *arg_node = node->data.call.arguments[0];
        bool multiline_log = false;
        LLVMTypeRef arg_type = NULL;
        const char *log_fn_name = "pgy_log_int";
        LLVMValueRef arg = NULL;

        if (arg_node != NULL && arg_node->type == AST_STRING
            && arg_node->data.string.value != NULL) {
            const char *raw = arg_node->data.string.value;
            multiline_log = (strchr(raw, '\n') != NULL) || (strchr(raw, '\r') != NULL);
            if (multiline_log) {
                char *normalized = llvm_normalize_banner_string_literal(raw);
                if (normalized != NULL) {
                    arg = LLVMBuildGlobalStringPtr(ctx->builder, normalized,
                                                  llvm_tmp_name(ctx));
                    free(normalized);
                } else {
                    arg = LLVMBuildGlobalStringPtr(ctx->builder, raw,
                                                  llvm_tmp_name(ctx));
                }
            } else {
                arg = LLVMBuildGlobalStringPtr(ctx->builder, raw,
                                              llvm_tmp_name(ctx));
            }
            arg_type = ctx->type_i8ptr;
        } else {
            arg = llvm_emit_expression(arg_node, ctx);
            if (arg != NULL)
                arg_type = LLVMTypeOf(arg);
        }

        if (arg == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        /* Select the right log function based on arg type */
        if (multiline_log) {
            log_fn_name = "pgy_log_banner";
        } else if (arg_type == ctx->type_i64)       log_fn_name = "pgy_log_long";
        else if (arg_type == ctx->type_f32)         log_fn_name = "pgy_log_float";
        else if (arg_type == ctx->type_f64)         log_fn_name = "pgy_log_double";
        else if (arg_type == ctx->type_i1)          log_fn_name = "pgy_log_bool";
        else if (arg_type == ctx->type_i8ptr)       log_fn_name = "pgy_log_string";

        LLVMFuncEntry *log_fn = llvm_lookup_function(ctx, log_fn_name);
        if (log_fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { arg };
        LLVMBuildCall2(ctx->builder, log_fn->fn_type, log_fn->fn,
                       args, 1, "");
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "LogRaw") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *arg_node = node->data.call.arguments[0];
        LLVMValueRef arg = NULL;

        if (arg_node != NULL && arg_node->type == AST_STRING
            && arg_node->data.string.value != NULL) {
            arg = LLVMBuildGlobalStringPtr(ctx->builder, arg_node->data.string.value,
                                          llvm_tmp_name(ctx));
        } else {
            arg = llvm_emit_expression(arg_node, ctx);
            if (arg == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
        }

        LLVMTypeRef arg_type = LLVMTypeOf(arg);
        const char *log_fn_name = "pgy_log_int";

        if (arg_type == ctx->type_i64)        log_fn_name = "pgy_log_long";
        else if (arg_type == ctx->type_f32)   log_fn_name = "pgy_log_float";
        else if (arg_type == ctx->type_f64)   log_fn_name = "pgy_log_double";
        else if (arg_type == ctx->type_i1)    log_fn_name = "pgy_log_bool";
        else if (arg_type == ctx->type_i8ptr) log_fn_name = "pgy_log_string";

        LLVMFuncEntry *log_fn = llvm_lookup_function(ctx, log_fn_name);
        if (log_fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { arg };
        LLVMBuildCall2(ctx->builder, log_fn->fn_type, log_fn->fn,
                       args, 1, "");
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Built-in: LogBanner */
    if (strcmp(callee_name, "LogBanner") == 0
        || strcmp(callee_name, "LogBlock") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *arg = node->data.call.arguments[0];
        if (arg == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef log_arg = NULL;
        if (arg->type == AST_STRING) {
            char *normalized = llvm_normalize_banner_string_literal(
                arg->data.string.value);
            if (normalized != NULL) {
                log_arg = LLVMBuildGlobalStringPtr(ctx->builder, normalized,
                                                   llvm_tmp_name(ctx));
                free(normalized);
            }
        } else {
            log_arg = llvm_emit_expression(arg, ctx);
            if (log_arg != NULL)
                log_arg = llvm_coerce_value_to_string(log_arg, ctx);
        }

        if (log_arg == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMFuncEntry *log_fn = llvm_lookup_function(ctx, "pgy_log_banner");
        if (log_fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { log_arg };
        LLVMBuildCall2(ctx->builder, log_fn->fn_type, log_fn->fn,
                       args, 1, "");
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Built-in: ClaimSlot<T>() — handled mostly in let_decl, but standalone */
    if (strcmp(callee_name, "ClaimSlot") == 0
        || strcmp(callee_name, "ClaimSecureSlot") == 0) {
        llvm_set_error_at(ctx, node,
            "LLVM standalone %s requires an explicitly typed binding; use 'let value: %s<T> = %s()'",
            callee_name,
            strcmp(callee_name, "ClaimSecureSlot") == 0 ? "SecureSlot" : "Slot",
            callee_name);
        return NULL;
    }

    if (strcmp(callee_name, "ClaimDeviceSlot") == 0) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_claim_device_Int");
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                              NULL, 0, llvm_tmp_name(ctx));
    }

    /* Built-in: Write(slot, value) */
    if (strcmp(callee_name, "Write") == 0) {
        if (node->data.call.arg_count < 2)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        /* Resolve slot inner type */
        const char *inner = "Int";
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = node->data.call.arguments[0];
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name),
            is_secure ? "pgy_secure_write_%s" : "pgy_write_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL) {
            if (is_secure)
                llvm_direct_secure_slot_write(ctx, slot_var, val);
            else
                llvm_direct_slot_write(ctx, slot_var, val);
            return LLVMConstInt(ctx->type_i32, 0, 0);
        }

        if (is_secure) {
            LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, source_name);
            if (token_var == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            LLVMValueRef args[] = { slot_var->alloca, val, token_var->alloca };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        } else {
            LLVMValueRef args[] = { slot_var->alloca, val };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Built-in: Read(slot) */
    if (strcmp(callee_name, "Read") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        const char *inner = "Int";
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = node->data.call.arguments[0];
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name),
            is_secure ? "pgy_secure_read_%s" : "pgy_read_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL) {
            if (is_secure)
                return llvm_direct_secure_slot_read(ctx, slot_var, inner);
            return llvm_direct_slot_read(ctx, slot_var, inner);
        }

        if (is_secure) {
            LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, source_name);
            if (token_var == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            LLVMValueRef args[] = { slot_var->alloca, token_var->alloca };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                   args, 2, llvm_tmp_name(ctx));
        } else {
            LLVMValueRef args[] = { slot_var->alloca };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                   args, 1, llvm_tmp_name(ctx));
        }
    }

    /* Built-in: Release(slot) */
    if (strcmp(callee_name, "Release") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        const char *inner = "Int";
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = node->data.call.arguments[0];
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name),
            is_secure ? "pgy_secure_release_%s" : "pgy_release_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL && is_secure)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        if (fn == NULL) {
            if (is_secure)
                llvm_direct_secure_slot_release(ctx, slot_var);
            else
                llvm_direct_slot_release(ctx, slot_var);
        } else if (is_secure) {
            LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, source_name);
            if (token_var == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            LLVMValueRef args[] = { slot_var->alloca, token_var->alloca };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        } else {
            LLVMValueRef args[] = { slot_var->alloca };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }

        /* Mark slot as explicitly released */
        if (source_name != NULL) {
            const char *sname = source_name;
            for (int ri = 0; ri < ctx->slot_var_count; ri++) {
                if (strcmp(ctx->slot_vars[ri].var_name, sname) == 0) {
                    ctx->slot_vars[ri].released = true;
                    break;
                }
            }
        }

        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "DeviceWrite") == 0) {
        if (node->data.call.arg_count < 2)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *slot_arg = node->data.call.arguments[0];
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (inner == NULL && slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            llvm_set_error_at(ctx, slot_arg,
                "LLVM DeviceWrite on '%s' requires a concrete DeviceSlot<T> inner type",
                slot_arg->data.identifier.name);
            return NULL;
        }
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_device_write_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (fn == NULL || val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca, val };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "DeviceRead") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *slot_arg = node->data.call.arguments[0];
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (inner == NULL && slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            llvm_set_error_at(ctx, slot_arg,
                "LLVM DeviceRead on '%s' requires a concrete DeviceSlot<T> inner type",
                slot_arg->data.identifier.name);
            return NULL;
        }
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_device_read_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                              args, 1, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "ReleaseDeviceSlot") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *slot_arg = node->data.call.arguments[0];
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (inner == NULL && slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            llvm_set_error_at(ctx, slot_arg,
                "LLVM ReleaseDeviceSlot on '%s' requires a concrete DeviceSlot<T> inner type",
                slot_arg->data.identifier.name);
            return NULL;
        }
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_release_device_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        if (slot_arg->type == AST_IDENTIFIER)
            llvm_mark_device_slot_released(ctx, slot_arg->data.identifier.name);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "SubmitDeviceRead") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *slot_arg = node->data.call.arguments[0];
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (inner == NULL && slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            llvm_set_error_at(ctx, slot_arg,
                "LLVM SubmitDeviceRead on '%s' requires a concrete DeviceSlot<T> inner type",
                slot_arg->data.identifier.name);
            return NULL;
        }
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_submit_device_read_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                              args, 1, llvm_tmp_name(ctx));
    }

    /* Event invocation: OnHit(42) → OnHit_INVOKE(&OnHit, 42) */
    {
        LLVMEventTypeEntry *evt = llvm_lookup_event(ctx, callee_name);
        if (evt != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INVOKE", callee_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMValueRef ev_ptr = LLVMGetNamedGlobal(ctx->module, callee_name);
            if (ev_ptr == NULL) {
                LLVMVarEntry *ev = llvm_scope_lookup(ctx, callee_name);
                if (ev != NULL) ev_ptr = ev->alloca;
            }
            if (fn != NULL && ev_ptr != NULL) {
                size_t ac = node->data.call.arg_count;
                LLVMValueRef *args = calloc(ac + 1, sizeof(LLVMValueRef));
                args[0] = ev_ptr;
                for (size_t j = 0; j < ac; j++)
                    args[j + 1] = llvm_emit_expression(
                        node->data.call.arguments[j], ctx);
                LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, (unsigned)(ac + 1), "");
                free(args);
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }
        }
    }

    /* Built-in: Abs(x) → select(x < 0, -x, x) */
    if (strcmp(callee_name, "Abs") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef x = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef zero = LLVMConstInt(ctx->type_i32, 0, 0);
        LLVMValueRef neg = LLVMBuildNeg(ctx->builder, x, llvm_tmp_name(ctx));
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, x, zero,
                                          llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, cmp, neg, x, llvm_tmp_name(ctx));
    }

    /* Built-in: Min(a, b) → select(a < b, a, b) */
    if (strcmp(callee_name, "Min") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef a = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef b = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, a, b,
                                          llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, cmp, a, b, llvm_tmp_name(ctx));
    }

    /* Built-in: Max(a, b) → select(a > b, a, b) */
    if (strcmp(callee_name, "Max") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef a = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef b = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGT, a, b,
                                          llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, cmp, a, b, llvm_tmp_name(ctx));
    }

    /* Built-in: ArrayLength(arr) */
    if (strcmp(callee_name, "ArrayLength") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef arr = llvm_emit_expression(node->data.call.arguments[0], ctx);
        if (arr != NULL && LLVMGetTypeKind(LLVMTypeOf(arr)) == LLVMStructTypeKind) {
            LLVMValueRef len = llvm_array_length_i64(ctx, arr);
            return LLVMBuildTrunc(ctx->builder, len, ctx->type_i32, llvm_tmp_name(ctx));
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ArrayPush") == 0 && node->data.call.arg_count == 2) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        if (arr_arg == NULL || arr_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, arr_arg->data.identifier.name);
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, arr_arg->data.identifier.name);
        if (arr_var == NULL || entry == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
        if (suffix == NULL || strcmp(suffix, "Unknown") == 0)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(value) != entry->elem_type) {
            if ((entry->elem_type == ctx->type_i32 || entry->elem_type == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
            else if ((entry->elem_type == ctx->type_f32 || entry->elem_type == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
        }

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_array_push_%s", suffix);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn != NULL) {
            LLVMValueRef args[] = { arr_var->alloca, value };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ArraySet") == 0 && node->data.call.arg_count == 3) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        if (arr_arg == NULL || arr_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, arr_arg->data.identifier.name);
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, arr_arg->data.identifier.name);
        LLVMValueRef idx = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[2], ctx);
        if (arr_var == NULL || entry == NULL || idx == NULL || value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef arr = LLVMBuildLoad2(ctx->builder, arr_var->type, arr_var->alloca,
            llvm_tmp_name(ctx));
        LLVMValueRef data_ptr = llvm_array_data_ptr(ctx, arr);
        if (LLVMTypeOf(value) != entry->elem_type) {
            if ((entry->elem_type == ctx->type_i32 || entry->elem_type == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
            else if ((entry->elem_type == ctx->type_f32 || entry->elem_type == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
        }
        LLVMValueRef gep = LLVMBuildGEP2(ctx->builder, entry->elem_type, data_ptr, &idx, 1,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, gep);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ArrayPop") == 0 && node->data.call.arg_count == 1) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        if (arr_arg == NULL || arr_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, arr_arg->data.identifier.name);
        if (arr_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef arr = LLVMBuildLoad2(ctx->builder, arr_var->type, arr_var->alloca,
            llvm_tmp_name(ctx));
        LLVMValueRef len = llvm_array_length_i64(ctx, arr);
        LLVMValueRef has_any = LLVMBuildICmp(ctx->builder, LLVMIntUGT, len,
            LLVMConstInt(ctx->type_i64, 0, 0), llvm_tmp_name(ctx));
        LLVMValueRef dec = LLVMBuildSub(ctx->builder, len,
            LLVMConstInt(ctx->type_i64, 1, 0), llvm_tmp_name(ctx));
        LLVMValueRef next_len = LLVMBuildSelect(ctx->builder, has_any, dec, len,
            llvm_tmp_name(ctx));
        arr = LLVMBuildInsertValue(ctx->builder, arr, next_len, 1, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, arr, arr_var->alloca);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ListNew") == 0 && node->data.call.arg_count == 0) {
        LLVMTypeRef list_ty = ctx->type_i32;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_list_new_raw_export");
        if (ctx->current_ret_type != NULL
            && LLVMGetTypeKind(ctx->current_ret_type) == LLVMStructTypeKind) {
            list_ty = ctx->current_ret_type;
        }
        tmp = llvm_create_entry_alloca(ctx, list_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(list_ty), tmp);
        if (fn != NULL) {
            LLVMTypeRef elem_ty = LLVMInt32TypeInContext(ctx->context);
            if (LLVMCountStructElementTypes(list_ty) > 0) {
                LLVMTypeRef fields[3];
                LLVMGetStructElementTypes(list_ty, fields);
                elem_ty = LLVMGetElementType(fields[0]);
            }
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        return LLVMBuildLoad2(ctx->builder, list_ty, tmp, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "SetNew") == 0 && node->data.call.arg_count == 0) {
        LLVMTypeRef set_ty = ctx->type_i32;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_set_new_raw_export");
        if (ctx->current_ret_type != NULL
            && LLVMGetTypeKind(ctx->current_ret_type) == LLVMStructTypeKind) {
            set_ty = ctx->current_ret_type;
        }
        tmp = llvm_create_entry_alloca(ctx, set_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(set_ty), tmp);
        if (fn != NULL) {
            LLVMTypeRef elem_ty = LLVMInt32TypeInContext(ctx->context);
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        return LLVMBuildLoad2(ctx->builder, set_ty, tmp, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "SetAdd") == 0 && node->data.call.arg_count == 2) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (set_arg == NULL || set_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        set_var = llvm_scope_lookup(ctx, set_arg->data.identifier.name);
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        if (set_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_lookup_function(ctx, "pgy_set_add_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "SetHas") == 0 && node->data.call.arg_count == 2) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (set_arg == NULL || set_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        set_var = llvm_scope_lookup(ctx, set_arg->data.identifier.name);
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        if (set_var == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_lookup_function(ctx, "pgy_set_has_raw_export");
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3,
                                  llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "SetRemove") == 0 && node->data.call.arg_count == 2) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (set_arg == NULL || set_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        set_var = llvm_scope_lookup(ctx, set_arg->data.identifier.name);
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        if (set_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_lookup_function(ctx, "pgy_set_remove_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "SetSize") == 0 && node->data.call.arg_count == 1) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        LLVMFuncEntry *fn;
        if (set_arg == NULL || set_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        set_var = llvm_scope_lookup(ctx, set_arg->data.identifier.name);
        fn = llvm_lookup_function(ctx, "pgy_set_size_raw_export");
        if (set_var == NULL || fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
                                  llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "ListPush") == 0 && node->data.call.arg_count == 2) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        if (list_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_lookup_function(ctx, "pgy_list_push_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ListGet") == 0 && node->data.call.arg_count == 2) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        if (list_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        idx = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (idx == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(idx) != ctx->type_i32)
            idx = LLVMBuildTruncOrBitCast(ctx->builder, idx, ctx->type_i32, llvm_tmp_name(ctx));
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(elem_ty), tmp);
        fn = llvm_lookup_function(ctx, "pgy_list_get_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                idx,
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        }
        return LLVMBuildLoad2(ctx->builder, elem_ty, tmp, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "ListSet") == 0 && node->data.call.arg_count == 3) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        if (list_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        idx = llvm_emit_expression(node->data.call.arguments[1], ctx);
        value = llvm_emit_expression(node->data.call.arguments[2], ctx);
        if (idx == NULL || value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(idx) != ctx->type_i32)
            idx = LLVMBuildTruncOrBitCast(ctx->builder, idx, ctx->type_i32, llvm_tmp_name(ctx));
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_lookup_function(ctx, "pgy_list_set_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                idx,
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ListSize") == 0 && node->data.call.arg_count == 1) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        fn = llvm_lookup_function(ctx, "pgy_list_size_raw_export");
        if (list_var == NULL || fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "ListRemove") == 0 && node->data.call.arg_count == 2) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        if (list_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        idx = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (idx == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(idx) != ctx->type_i32)
            idx = LLVMBuildTruncOrBitCast(ctx->builder, idx, ctx->type_i32, llvm_tmp_name(ctx));
        fn = llvm_lookup_function(ctx, "pgy_list_remove_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                idx,
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "QueuePush") == 0 && node->data.call.arg_count == 2) {
        ASTNode *queue_arg = node->data.call.arguments[0];
        LLVMVarEntry *queue_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (queue_arg == NULL || queue_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        queue_var = llvm_scope_lookup(ctx, queue_arg->data.identifier.name);
        inner_name = llvm_lookup_queue_inner(ctx, queue_arg->data.identifier.name);
        if (queue_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_lookup_function(ctx, "pgy_queue_push_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "QueuePop") == 0 && node->data.call.arg_count == 1) {
        ASTNode *queue_arg = node->data.call.arguments[0];
        LLVMVarEntry *queue_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (queue_arg == NULL || queue_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        queue_var = llvm_scope_lookup(ctx, queue_arg->data.identifier.name);
        inner_name = llvm_lookup_queue_inner(ctx, queue_arg->data.identifier.name);
        if (queue_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(elem_ty), tmp);
        fn = llvm_lookup_function(ctx, "pgy_queue_pop_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        return LLVMBuildLoad2(ctx->builder, elem_ty, tmp, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "QueueSize") == 0 && node->data.call.arg_count == 1) {
        ASTNode *queue_arg = node->data.call.arguments[0];
        LLVMVarEntry *queue_var;
        LLVMFuncEntry *fn;
        if (queue_arg == NULL || queue_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        queue_var = llvm_scope_lookup(ctx, queue_arg->data.identifier.name);
        fn = llvm_lookup_function(ctx, "pgy_queue_size_raw_export");
        if (queue_var == NULL || fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "QueueEmpty") == 0 && node->data.call.arg_count == 1) {
        ASTNode *queue_arg = node->data.call.arguments[0];
        LLVMVarEntry *queue_var;
        LLVMFuncEntry *fn;
        if (queue_arg == NULL || queue_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i1, 1, 0);
        queue_var = llvm_scope_lookup(ctx, queue_arg->data.identifier.name);
        fn = llvm_lookup_function(ctx, "pgy_queue_empty_raw_export");
        if (queue_var == NULL || fn == NULL)
            return LLVMConstInt(ctx->type_i1, 1, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "MapSet") == 0 && node->data.call.arg_count == 3) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        value_name = llvm_lookup_map_value(ctx, map_arg->data.identifier.name);
        if (map_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        value_ty = pergyra_type_to_llvm(ctx, value_name != NULL ? value_name : "Int");
        key = llvm_emit_expression(node->data.call.arguments[1], ctx);
        value = llvm_emit_expression(node->data.call.arguments[2], ctx);
        if (key == NULL || value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(value) != value_ty) {
            if ((value_ty == ctx->type_i32 || value_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, value_ty, llvm_tmp_name(ctx));
            else if ((value_ty == ctx->type_f32 || value_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, value_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_lookup_function(ctx,
            key_name != NULL && strcmp(key_name, "Int") == 0
                ? "pgy_map_set_raw_i32_export"
                : "pgy_map_set_raw_export");
        if (fn != NULL) {
            if (key_name != NULL && strcmp(key_name, "Int") == 0) {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    key,
                    LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, value_ty)
                };
                LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
            } else {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    key,
                    LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, value_ty)
                };
                LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "MapGet") == 0 && node->data.call.arg_count == 2) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        value_name = llvm_lookup_map_value(ctx, map_arg->data.identifier.name);
        if (map_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        value_ty = pergyra_type_to_llvm(ctx, value_name != NULL ? value_name : "Int");
        key = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (key == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(value_ty), tmp);
        fn = llvm_lookup_function(ctx,
            key_name != NULL && strcmp(key_name, "Int") == 0
                ? "pgy_map_get_raw_i32_export"
                : "pgy_map_get_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key,
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, value_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        }
        return LLVMBuildLoad2(ctx->builder, value_ty, tmp, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "MapHas") == 0 && node->data.call.arg_count == 2) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        LLVMValueRef key;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        key = llvm_emit_expression(node->data.call.arguments[1], ctx);
        fn = llvm_lookup_function(ctx,
            key_name != NULL && strcmp(key_name, "Int") == 0
                ? "pgy_map_has_raw_i32_export"
                : "pgy_map_has_raw_export");
        if (map_var == NULL || key == NULL || fn == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key
            };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "MapRemove") == 0 && node->data.call.arg_count == 2) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        value_name = llvm_lookup_map_value(ctx, map_arg->data.identifier.name);
        if (map_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        value_ty = pergyra_type_to_llvm(ctx, value_name != NULL ? value_name : "Int");
        key = llvm_emit_expression(node->data.call.arguments[1], ctx);
        fn = llvm_lookup_function(ctx,
            key_name != NULL && strcmp(key_name, "Int") == 0
                ? "pgy_map_remove_raw_i32_export"
                : "pgy_map_remove_raw_export");
        if (key == NULL || fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key,
                llvm_sizeof_type_i64(ctx, value_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "MapSize") == 0 && node->data.call.arg_count == 1) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        fn = llvm_lookup_function(ctx, "pgy_map_size_raw_export");
        if (map_var == NULL || fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "MapKeys") == 0 && node->data.call.arg_count == 1) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        LLVMTypeRef array_ty;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            return LLVMConstNull(ctx->array_type_String);
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        array_ty = (key_name != NULL && strcmp(key_name, "Int") == 0)
            ? ctx->array_type_Int
            : ctx->array_type_String;
        if (map_var == NULL)
            return LLVMConstNull(array_ty);
        tmp = llvm_create_entry_alloca(ctx, array_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(array_ty), tmp);
        fn = llvm_lookup_function(ctx,
            key_name != NULL && strcmp(key_name, "Int") == 0
                ? "pgy_map_keys_raw_i32_export"
                : "pgy_map_keys_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        return LLVMBuildLoad2(ctx->builder, array_ty, tmp, llvm_tmp_name(ctx));
    }

    if ((strcmp(callee_name, "ViewRead") == 0
         || strcmp(callee_name, "ViewWrite") == 0
         || strcmp(callee_name, "Move") == 0)
        && node->data.call.arg_count == 1) {
        return llvm_emit_expression(node->data.call.arguments[0], ctx);
    }

    /* Built-in: StringLength(s) → call strlen */
    if (strcmp(callee_name, "StringLength") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef s = llvm_emit_expression(node->data.call.arguments[0], ctx);
        /* Declare strlen if not already */
        LLVMFuncEntry *strlen_fn = llvm_lookup_function(ctx, "strlen");
        if (strlen_fn == NULL) {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i64, params, 1, 0);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, "strlen", ft);
            llvm_register_function(ctx, "strlen", fn, ft, ctx->type_i64);
            strlen_fn = llvm_lookup_function(ctx, "strlen");
        }
        LLVMValueRef args[] = { s };
        LLVMValueRef len = LLVMBuildCall2(ctx->builder, strlen_fn->fn_type,
            strlen_fn->fn, args, 1, llvm_tmp_name(ctx));
        return LLVMBuildTrunc(ctx->builder, len, ctx->type_i32, llvm_tmp_name(ctx));
    }

    if ((strcmp(callee_name, "Contains") == 0
         || strcmp(callee_name, "StringContains") == 0)
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringContains");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if ((strcmp(callee_name, "Replace") == 0
         || strcmp(callee_name, "StringReplace") == 0)
        && node->data.call.arg_count == 3) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringReplace");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 3);
    }

    if (strcmp(callee_name, "Substring") == 0
        && node->data.call.arg_count == 3) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "Substring");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 3);
    }

    if ((strcmp(callee_name, "Trim") == 0
         || strcmp(callee_name, "StringTrim") == 0)
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringTrim");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if ((strcmp(callee_name, "Upper") == 0
         || strcmp(callee_name, "ToUpper") == 0)
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToUpper");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if ((strcmp(callee_name, "Lower") == 0
         || strcmp(callee_name, "ToLower") == 0)
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToLower");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if ((strcmp(callee_name, "Concat") == 0
         || strcmp(callee_name, "StringConcat") == 0)
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringConcat");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if (strcmp(callee_name, "ReadFile") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_read_file");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "ToString") == 0
        && node->data.call.arg_count == 1) {
        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return llvm_coerce_value_to_string(value, ctx);
    }

    if (strcmp(callee_name, "ToInt") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToInt");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "ToFloat") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToFloat");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "Random") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "Random");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "WriteFile") == 0
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_write_file");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if (strcmp(callee_name, "Input") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_input");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentLastTrace") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_last_trace_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentLastFailure") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_last_failure_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentLastName") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_last_name_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentLastHandle") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_last_handle_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentLastTraceId") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_last_trace_id_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentLastStepCount") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_last_step_count_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentLastFailed") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_last_failed_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentHistoryCount") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_count_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentHistoryStepName") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_name_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepZone") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_zone_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepPhase") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_phase_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepParticipant") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_participant_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepSlot") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_slot_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepFromZone") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_from_zone_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepFromSlot") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_from_slot_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepToZone") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_to_zone_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepToSlot") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_to_slot_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepOk") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_ok_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepFailure") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_failure_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveCount") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_count_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentActiveName") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_name_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveHandle") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_handle_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveParentHandle") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_parent_handle_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveTraceId") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_trace_id_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActivePriority") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_priority_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveSubjectCount") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_subject_count_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveStepCount") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_count_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveConcurrent") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_concurrent_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveFailed") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_failed_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveFailure") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_failure_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveTrace") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_trace_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveStepName") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_name_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepZone") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_zone_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepPhase") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_phase_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepParticipant") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_participant_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepSlot") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_slot_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepFromZone") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_from_zone_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepFromSlot") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_from_slot_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepToZone") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_to_zone_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepToSlot") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_to_slot_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepOk") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_ok_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepFailure") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_failure_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentCurrentHandle") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_current_handle_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentRecentCount") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_count_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }
    if (strcmp(callee_name, "IntentRecentHandle") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_handle_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }
    if (strcmp(callee_name, "IntentRecentTraceId") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_trace_id_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentRecentName") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_name_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentRecentTrace") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_trace_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentRecentFailure") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_failure_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentRecentStepCount") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_step_count_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentRecentFailed") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_failed_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "SeedRandom") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "SeedRandom");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "FileOpen") == 0
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_open");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if (strcmp(callee_name, "FileRead") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_read");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "FileWrite") == 0
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_write");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if (strcmp(callee_name, "FileClose") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_close");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    /* Built-in: Print(s) → printf("%s", s) */
    if (strcmp(callee_name, "Print") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef vt = LLVMTypeOf(val);
        LLVMFuncEntry *pf = llvm_lookup_function(ctx, "printf");
        if (pf != NULL) {
            if (vt == ctx->type_i8ptr) {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(ctx->builder,
                    "%s", ".fmt_s");
                LLVMValueRef args[] = { fmt, val };
                LLVMBuildCall2(ctx->builder, pf->fn_type, pf->fn, args, 2, "");
            } else {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(ctx->builder,
                    "%d", ".fmt_d");
                LLVMValueRef args[] = { fmt, val };
                LLVMBuildCall2(ctx->builder, pf->fn_type, pf->fn, args, 2, "");
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ReadLine") == 0 && node->data.call.arg_count == 0) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_input");
        if (fn != NULL) {
            LLVMValueRef empty = LLVMBuildGlobalStringPtr(ctx->builder, "", ".readline_empty");
            LLVMValueRef args[] = { empty };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }
    }

    if (strcmp(callee_name, "Now") == 0 && node->data.call.arg_count == 0) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_now_ms");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, "");
    }

    if (strcmp(callee_name, "Sleep") == 0 && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_sleep_ms");
        if (fn != NULL) {
            LLVMValueRef arg = llvm_emit_expression(node->data.call.arguments[0], ctx);
            LLVMValueRef args[] = { arg };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Built-in: Ok(value) → { .ok=true, .value=value } */
    if (strcmp(callee_name, "Ok") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, LLVMTypeOf(val), ctx->type_i8ptr }, 3, 0);
        LLVMValueRef r = LLVMGetUndef(result_ty);
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r, val, 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstNull(ctx->type_i8ptr), 2, llvm_tmp_name(ctx));
        return r;
    }

    /* Built-in: Err(value) → { .ok=false, .value=value } */
    if (strcmp(callee_name, "Err") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, ctx->type_i32, ctx->type_i8ptr }, 3, 0);
        LLVMValueRef r = LLVMGetUndef(result_ty);
        if (LLVMTypeOf(val) != ctx->type_i8ptr) {
            if (LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMPointerTypeKind) {
                val = LLVMBuildBitCast(ctx->builder, val, ctx->type_i8ptr, llvm_tmp_name(ctx));
            } else {
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }
        }
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 1, 0), 0, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r, val, 2, llvm_tmp_name(ctx));
        return r;
    }

    /* Built-in: IsOk(result) → extract ok field */
    if (strcmp(callee_name, "IsOk") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
    }

    /* Built-in: IsErr(result) → !ok */
    if (strcmp(callee_name, "IsErr") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx));
    }

    /* Built-in: Unwrap(result) → extract value field */
    if (strcmp(callee_name, "Unwrap") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return LLVMBuildExtractValue(ctx->builder, r, 1, llvm_tmp_name(ctx));
    }

    /* Built-in: UnwrapOr(result, default) → ok ? value : default */
    if (strcmp(callee_name, "UnwrapOr") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef def = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        LLVMValueRef ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
        LLVMValueRef val = LLVMBuildExtractValue(ctx->builder, r, 1, llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, ok, val, def, llvm_tmp_name(ctx));
    }

    /* Built-in: Some(value) → { .tag=PgyOptionSome, .value=value } */
    if (strcmp(callee_name, "Some") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef option_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, LLVMTypeOf(val) }, 2, 0);
        LLVMValueRef o = LLVMGetUndef(option_ty);
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        o = LLVMBuildInsertValue(ctx->builder, o, val, 1, llvm_tmp_name(ctx));
        return o;
    }

    /* Built-in: None() → { .tag=PgyOptionNone, .value=zero } */
    if (strcmp(callee_name, "None") == 0 && node->data.call.arg_count == 0) {
        LLVMTypeRef value_ty = ctx->type_i32;
        if (LLVMGetTypeKind(ctx->current_ret_type) == LLVMStructTypeKind
            && LLVMCountStructElementTypes(ctx->current_ret_type) == 2) {
            LLVMTypeRef fields[2];
            LLVMGetStructElementTypes(ctx->current_ret_type, fields);
            if (fields[0] == ctx->type_i32)
                value_ty = fields[1];
        }
        LLVMTypeRef option_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, value_ty }, 2, 0);
        LLVMValueRef o = LLVMGetUndef(option_ty);
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstInt(ctx->type_i32, 1, 0), 0, llvm_tmp_name(ctx));
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstNull(value_ty), 1, llvm_tmp_name(ctx));
        return o;
    }

    /* Built-in: IsSome(option) / IsNone(option) */
    if ((strcmp(callee_name, "IsSome") == 0 || strcmp(callee_name, "IsNone") == 0)
        && node->data.call.arg_count == 1) {
        LLVMValueRef o = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, o, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32,
                strcmp(callee_name, "IsSome") == 0 ? 0 : 1, 0),
            llvm_tmp_name(ctx));
    }

    /* Built-in: UnwrapOption(option) → extract value field */
    if (strcmp(callee_name, "UnwrapOption") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef o = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return LLVMBuildExtractValue(ctx->builder, o, 1, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "Cancel") == 0 && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_task_cancel_export");
        LLVMValueRef task = llvm_emit_expression(node->data.call.arguments[0], ctx);
        if (fn != NULL) {
            LLVMValueRef args[] = { task };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                args, 1, llvm_tmp_name(ctx));
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "IsCancelled") == 0 && node->data.call.arg_count == 0) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_task_is_cancelled_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                NULL, 0, llvm_tmp_name(ctx));
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "TrySend") == 0 && node->data.call.arg_count == 2) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char fname[128];
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
                snprintf(fname, sizeof(fname), "pgy_channel_try_send_%s",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, val };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 2, llvm_tmp_name(ctx));
                }
            }
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "TrySendStatus") == 0 && node->data.call.arg_count == 2) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char closed_name[128];
                char send_name[128];
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
                snprintf(closed_name, sizeof(closed_name), "pgy_channel_closed_%s",
                    inner != NULL ? inner : "Int");
                snprintf(send_name, sizeof(send_name), "pgy_channel_try_send_%s",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *closed_fn = llvm_lookup_function(ctx, closed_name);
                LLVMFuncEntry *send_fn = llvm_lookup_function(ctx, send_name);
                if (closed_fn != NULL && send_fn != NULL) {
                    LLVMValueRef closed_args[] = { ch_var->alloca };
                    LLVMValueRef send_args[] = { ch_var->alloca, val };
                    LLVMValueRef closed = LLVMBuildCall2(ctx->builder, closed_fn->fn_type,
                        closed_fn->fn, closed_args, 1, llvm_tmp_name(ctx));
                    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, send_fn->fn_type,
                        send_fn->fn, send_args, 2, llvm_tmp_name(ctx));
                    LLVMValueRef has_value = LLVMBuildOr(ctx->builder, closed, ok,
                        llvm_tmp_name(ctx));
                    return llvm_build_option_value(ctx, ctx->type_i1, has_value, ok);
                }
            }
        }
        return llvm_build_option_value(ctx, ctx->type_i1,
            LLVMConstInt(ctx->type_i1, 0, 0), LLVMConstInt(ctx->type_i1, 0, 0));
    }

    if (strcmp(callee_name, "SendTimeout") == 0 && node->data.call.arg_count == 3) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char fname[128];
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
                LLVMValueRef timeout = llvm_emit_expression(node->data.call.arguments[2], ctx);
                if (LLVMTypeOf(timeout) != ctx->type_i64) {
                    timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                        ctx->type_i64, llvm_tmp_name(ctx));
                }
                snprintf(fname, sizeof(fname), "pgy_channel_send_timeout_%s",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, val, timeout };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 3, llvm_tmp_name(ctx));
                }
            }
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "SendTimeoutStatus") == 0 && node->data.call.arg_count == 3) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char closed_name[128];
                char send_name[128];
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
                LLVMValueRef timeout = llvm_emit_expression(node->data.call.arguments[2], ctx);
                if (LLVMTypeOf(timeout) != ctx->type_i64) {
                    timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                        ctx->type_i64, llvm_tmp_name(ctx));
                }
                snprintf(closed_name, sizeof(closed_name), "pgy_channel_closed_%s",
                    inner != NULL ? inner : "Int");
                snprintf(send_name, sizeof(send_name), "pgy_channel_send_timeout_%s",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *closed_fn = llvm_lookup_function(ctx, closed_name);
                LLVMFuncEntry *send_fn = llvm_lookup_function(ctx, send_name);
                if (closed_fn != NULL && send_fn != NULL) {
                    LLVMValueRef closed_args[] = { ch_var->alloca };
                    LLVMValueRef send_args[] = { ch_var->alloca, val, timeout };
                    LLVMValueRef closed_before = LLVMBuildCall2(ctx->builder,
                        closed_fn->fn_type, closed_fn->fn, closed_args, 1, llvm_tmp_name(ctx));
                    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, send_fn->fn_type,
                        send_fn->fn, send_args, 3, llvm_tmp_name(ctx));
                    LLVMValueRef closed_after = LLVMBuildCall2(ctx->builder,
                        closed_fn->fn_type, closed_fn->fn, closed_args, 1, llvm_tmp_name(ctx));
                    LLVMValueRef failed = LLVMBuildNot(ctx->builder, ok, llvm_tmp_name(ctx));
                    LLVMValueRef failed_and_closed = LLVMBuildAnd(ctx->builder, failed,
                        closed_after, llvm_tmp_name(ctx));
                    LLVMValueRef closed = LLVMBuildOr(ctx->builder, closed_before,
                        failed_and_closed, llvm_tmp_name(ctx));
                    LLVMValueRef has_value = LLVMBuildOr(ctx->builder, closed, ok,
                        llvm_tmp_name(ctx));
                    return llvm_build_option_value(ctx, ctx->type_i1, has_value, ok);
                }
            }
        }
        return llvm_build_option_value(ctx, ctx->type_i1,
            LLVMConstInt(ctx->type_i1, 0, 0), LLVMConstInt(ctx->type_i1, 0, 0));
    }

    if ((strcmp(callee_name, "TryRecv") == 0 && node->data.call.arg_count == 1)
        || (strcmp(callee_name, "RecvTimeout") == 0 && node->data.call.arg_count == 2)) {
        ASTNode *channel = node->data.call.arguments[0];
        const char *inner = "Int";
        LLVMVarEntry *ch_var = NULL;
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            ch_var = llvm_scope_lookup(ctx, name);
            {
                const char *lookup_inner = llvm_lookup_channel_inner(ctx, name);
                if (lookup_inner != NULL)
                    inner = lookup_inner;
            }
        }

        LLVMTypeRef value_ty = pergyra_type_to_llvm(ctx, inner);
        if (ch_var != NULL) {
            LLVMValueRef tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstNull(value_ty), tmp);

            char fname[128];
            if (strcmp(callee_name, "TryRecv") == 0) {
                snprintf(fname, sizeof(fname), "pgy_channel_try_recv_%s", inner);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, tmp };
                    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 2, llvm_tmp_name(ctx));
                    LLVMValueRef value = LLVMBuildLoad2(ctx->builder, value_ty, tmp,
                        llvm_tmp_name(ctx));
                    return llvm_build_option_value(ctx, value_ty, ok, value);
                }
            } else {
                LLVMValueRef timeout = llvm_emit_expression(node->data.call.arguments[1], ctx);
                if (LLVMTypeOf(timeout) != ctx->type_i64) {
                    timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                        ctx->type_i64, llvm_tmp_name(ctx));
                }
                snprintf(fname, sizeof(fname), "pgy_channel_recv_timeout_%s", inner);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, tmp, timeout };
                    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 3, llvm_tmp_name(ctx));
                    LLVMValueRef value = LLVMBuildLoad2(ctx->builder, value_ty, tmp,
                        llvm_tmp_name(ctx));
                    return llvm_build_option_value(ctx, value_ty, ok, value);
                }
            }
        }

        return llvm_build_option_value(ctx, value_ty,
            LLVMConstInt(ctx->type_i1, 0, 0), LLVMConstNull(value_ty));
    }

    if ((strcmp(callee_name, "ChannelReady") == 0
         || strcmp(callee_name, "ChannelLength") == 0
         || strcmp(callee_name, "ChannelCapacity") == 0
         || strcmp(callee_name, "ChannelSpace") == 0
         || strcmp(callee_name, "ChannelFull") == 0
         || strcmp(callee_name, "ChannelClosed") == 0)
        && node->data.call.arg_count == 1) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char fname[128];
                snprintf(fname, sizeof(fname), "pgy_channel_%s_%s",
                    strcmp(callee_name, "ChannelReady") == 0 ? "ready" :
                    strcmp(callee_name, "ChannelLength") == 0 ? "length" :
                    strcmp(callee_name, "ChannelCapacity") == 0 ? "capacity" :
                    strcmp(callee_name, "ChannelSpace") == 0 ? "space" :
                    strcmp(callee_name, "ChannelFull") == 0 ? "full" :
                    "closed",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 1, llvm_tmp_name(ctx));
                }
            }
        }

        if (strcmp(callee_name, "ChannelLength") == 0
            || strcmp(callee_name, "ChannelCapacity") == 0
            || strcmp(callee_name, "ChannelSpace") == 0)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (node->data.call.callee->type == AST_IDENTIFIER) {
        const char *host_name = ctx->current_class_name;
        ASTNode *host_method = llvm_current_host_method_decl(ctx, callee_name);
        if (host_name != NULL && host_method != NULL) {
            char full_name[256];
            snprintf(full_name, sizeof(full_name), "%s_%s", host_name, callee_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
            if (fn != NULL) {
                size_t argc = node->data.call.arg_count;
                LLVMValueRef *args = calloc(argc + 1, sizeof(LLVMValueRef));
                if (args == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                args[0] = llvm_current_self_call_arg(ctx);
                for (size_t i = 0; i < argc; i++) {
                    LLVMValueRef arg_val = llvm_emit_expression(node->data.call.arguments[i], ctx);
                    size_t logical_idx = 0;
                    for (size_t pk = 0; pk < host_method->data.func_decl.param_count; pk++) {
                        FuncParam *p = host_method->data.func_decl.params[pk];
                        const char *ptn = NULL;
                        LLVMClassTypeEntry *param_cls = NULL;
                        if (p->type == NULL && strcmp(p->name, "self") == 0)
                            continue;
                        if (logical_idx == i) {
                            if (p->type != NULL && p->type->type == AST_TYPE)
                                ptn = p->type->data.type.name;
                            param_cls = ptn != NULL ? llvm_lookup_class(ctx, ptn) : NULL;
                            if (param_cls != NULL && param_cls->is_pointer_self_host
                                && node->data.call.arguments[i] != NULL
                                && node->data.call.arguments[i]->type == AST_IDENTIFIER) {
                                const char *arg_name =
                                    node->data.call.arguments[i]->data.identifier.name;
                                LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
                                if (arg_var != NULL) {
                                    if (LLVMGetTypeKind(arg_var->type) == LLVMPointerTypeKind)
                                        arg_val = LLVMBuildLoad2(ctx->builder,
                                            arg_var->type, arg_var->alloca, llvm_tmp_name(ctx));
                                    else
                                        arg_val = arg_var->alloca;
                                }
                            }
                            break;
                        }
                        logical_idx++;
                    }
                    args[i + 1] = arg_val;
                }
                {
                    LLVMValueRef result;
                    if (fn->ret_type == ctx->type_void) {
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, (unsigned)(argc + 1), "");
                        result = LLVMConstInt(ctx->type_i32, 0, 0);
                    } else {
                        result = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, (unsigned)(argc + 1), llvm_tmp_name(ctx));
                    }
                    free(args);
                    return result;
                }
            }
        }
    }

    size_t argc = node->data.call.arg_count;
    ASTNode *decl = llvm_find_function_decl(ctx, callee_name);
    ASTNode *intent_decl = decl == NULL ? llvm_find_intent_decl(ctx, callee_name) : NULL;
    unsigned emitted_argc = 0;
    LLVMValueRef *args = NULL;

    if (decl != NULL)
        args = llvm_build_boundary_call_args(ctx, decl, node->data.call.arguments,
            argc, &emitted_argc);
    if (args == NULL && intent_decl != NULL) {
        args = calloc(argc > 0 ? argc : 1, sizeof(LLVMValueRef));
        if (args != NULL) {
            for (size_t i = 0; i < argc; i++) {
                const char *type_name = NULL;
                bool pointer_self = false;
                ASTNode *arg_node = node->data.call.arguments[i];

                if (i < intent_decl->data.intent_decl.involve_count) {
                    ASTNode *involves = intent_decl->data.intent_decl.involves[i];
                    if (involves != NULL && involves->type == AST_INTENT_INVOLVES
                        && involves->data.intent_involves.subject_type != NULL
                        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
                        type_name = involves->data.intent_involves.subject_type->data.type.name;
                    }
                } else if (i < intent_decl->data.intent_decl.involve_count
                        + intent_decl->data.intent_decl.value_count) {
                    size_t value_index = i - intent_decl->data.intent_decl.involve_count;
                    ASTNode *value = intent_decl->data.intent_decl.values[value_index];
                    if (value != NULL && value->data.intent_value.value_type != NULL
                        && value->data.intent_value.value_type->type == AST_TYPE) {
                        type_name = value->data.intent_value.value_type->data.type.name;
                    }
                }
                pointer_self = llvm_type_name_uses_pointer_self(ctx, type_name);
                if (pointer_self) {
                    if (arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
                        const char *arg_name = arg_node->data.identifier.name;
                        LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
                        if (arg_var != NULL) {
                            if (LLVMGetTypeKind(arg_var->type) == LLVMPointerTypeKind)
                                args[i] = LLVMBuildLoad2(ctx->builder, arg_var->type,
                                    arg_var->alloca, llvm_tmp_name(ctx));
                            else
                                args[i] = arg_var->alloca;
                        } else if (ctx->current_class_name != NULL) {
                            LLVMClassTypeEntry *host_cls = llvm_lookup_class(ctx, ctx->current_class_name);
                            if (host_cls != NULL) {
                                int field_idx = llvm_class_field_index(host_cls, arg_name);
                                if (field_idx >= 0) {
                                    LLVMValueRef base_ptr = llvm_current_self_base_ptr(ctx, host_cls);
                                    if (base_ptr != NULL) {
                                        args[i] = LLVMBuildStructGEP2(ctx->builder,
                                            host_cls->struct_type, base_ptr,
                                            (unsigned)field_idx, llvm_tmp_name(ctx));
                                    }
                                }
                            }
                        }
                    } else if (arg_node != NULL && arg_node->type == AST_MEMBER_ACCESS) {
                        args[i] = llvm_emit_member_lvalue_ptr(arg_node, ctx, NULL);
                    }
                }
                if (args[i] == NULL)
                    args[i] = llvm_emit_expression(arg_node, ctx);
            }
            emitted_argc = (unsigned)argc;
        }
    }
    if (args == NULL) {
        args = calloc(argc > 0 ? argc : 1, sizeof(LLVMValueRef));
        for (size_t i = 0; i < argc; i++)
            args[i] = llvm_emit_expression(node->data.call.arguments[i], ctx);
        emitted_argc = (unsigned)argc;
    }

    LLVMFuncEntry *func = llvm_resolve_callee_entry(ctx, callee_name, args, argc);
    if (func == NULL && decl != NULL && decl->type == AST_FUNC_DECL) {
        llvm_forward_declare_func(decl, ctx);
        func = llvm_lookup_function(ctx, callee_name);
    }
    if (func == NULL && intent_decl != NULL) {
        LLVMTypeRef *param_types = NULL;
        LLVMTypeRef fn_type;
        LLVMValueRef fn;

        if (intent_decl->data.intent_decl.involve_count
            + intent_decl->data.intent_decl.value_count > 0) {
            size_t param_count = intent_decl->data.intent_decl.involve_count
                + intent_decl->data.intent_decl.value_count;
            param_types = calloc(param_count,
                sizeof(LLVMTypeRef));
            for (size_t i = 0; i < param_count; i++) {
                LLVMTypeRef pt = ctx->type_i8ptr;
                const char *type_name = NULL;

                if (i < intent_decl->data.intent_decl.involve_count) {
                    ASTNode *involves = intent_decl->data.intent_decl.involves[i];
                    if (involves != NULL && involves->type == AST_INTENT_INVOLVES
                        && involves->data.intent_involves.subject_type != NULL
                        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
                        type_name = involves->data.intent_involves.subject_type->data.type.name;
                        pt = ast_type_to_llvm(ctx, involves->data.intent_involves.subject_type);
                        if (llvm_type_name_uses_pointer_self(ctx, type_name))
                            pt = LLVMPointerType(pt, 0);
                    }
                } else {
                    size_t value_index = i - intent_decl->data.intent_decl.involve_count;
                    ASTNode *value = intent_decl->data.intent_decl.values[value_index];
                    if (value != NULL && value->data.intent_value.value_type != NULL) {
                        pt = ast_type_to_llvm(ctx, value->data.intent_value.value_type);
                    }
                }
                param_types[i] = pt;
            }
        }

        fn_type = LLVMFunctionType(ctx->type_i1, param_types,
            (unsigned)(intent_decl->data.intent_decl.involve_count
                + intent_decl->data.intent_decl.value_count), 0);
        fn = LLVMAddFunction(ctx->module, callee_name, fn_type);
        llvm_register_function(ctx, callee_name, fn, fn_type, ctx->type_i1);
        free(param_types);
        func = llvm_lookup_function(ctx, callee_name);
    }
    if (func == NULL) {
        LLVMVarEntry *callee_var = NULL;
        LLVMValueRef fn_ptr = NULL;
        LLVMTypeRef fn_type = NULL;
        LLVMValueRef result;

        if (node->data.call.callee->type == AST_IDENTIFIER)
            callee_var = llvm_scope_lookup(ctx, callee_name);
        if (callee_var != NULL) {
            ASTNode *callable_type = llvm_lookup_callable_var(ctx, callee_name);
            LLVMTypeRef callable_ptr_ty = NULL;
            if (LLVMGetTypeKind(callee_var->type) == LLVMPointerTypeKind)
                fn_type = LLVMGetElementType(callee_var->type);
            if (fn_type == NULL || LLVMGetTypeKind(fn_type) != LLVMFunctionTypeKind) {
                if (callable_type != NULL)
                    fn_type = llvm_function_signature_from_event_type(ctx, callable_type);
            }
            if (fn_type != NULL && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind)
                callable_ptr_ty = LLVMPointerType(fn_type, 0);
            if ((fn_type == NULL || LLVMGetTypeKind(fn_type) != LLVMFunctionTypeKind)
                && ctx->current_function != NULL) {
                const char *current_name = LLVMGetValueName(ctx->current_function);
                ASTNode *current_decl = current_name != NULL
                    ? llvm_find_function_decl(ctx, current_name) : NULL;
                if (current_decl != NULL && current_decl->type == AST_FUNC_DECL) {
                    for (size_t i = 0; i < current_decl->data.func_decl.param_count; i++) {
                        FuncParam *p = current_decl->data.func_decl.params[i];
                        if (p == NULL || p->name == NULL || p->type == NULL)
                            continue;
                        if (strcmp(p->name, callee_name) != 0)
                            continue;
                        if (p->type->type == AST_EVENT_HANDLER_TYPE) {
                            size_t pc = p->type->data.event_handler_type.param_count;
                            LLVMTypeRef *pts = NULL;
                            LLVMTypeRef ret = ctx->type_void;
                            if (p->type->data.event_handler_type.return_type != NULL)
                                ret = ast_type_to_llvm(ctx,
                                    p->type->data.event_handler_type.return_type);
                            if (pc > 0) {
                                pts = calloc(pc, sizeof(LLVMTypeRef));
                                if (pts != NULL) {
                                    for (size_t pi = 0; pi < pc; pi++) {
                                        pts[pi] = ast_type_to_llvm(ctx,
                                            p->type->data.event_handler_type.param_types[pi]);
                                    }
                                }
                            }
                            fn_type = LLVMFunctionType(ret, pts, (unsigned)pc, 0);
                            free(pts);
                        } else {
                            LLVMTypeRef declared_ptr_ty = ast_type_to_llvm(ctx, p->type);
                            if (LLVMGetTypeKind(declared_ptr_ty) == LLVMPointerTypeKind)
                                fn_type = LLVMGetElementType(declared_ptr_ty);
                            else
                                fn_type = declared_ptr_ty;
                        }
                        break;
                    }
                }
            }
            if (fn_type != NULL
                && LLVMGetTypeKind(fn_type) == LLVMPointerTypeKind) {
                callable_ptr_ty = fn_type;
                fn_type = LLVMGetElementType(fn_type);
            }
            if (callable_ptr_ty == NULL
                && fn_type != NULL
                && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind) {
                callable_ptr_ty = LLVMPointerType(fn_type, 0);
            }
            fn_ptr = llvm_emit_expression(node->data.call.callee, ctx);
            if (fn_ptr != NULL && callable_ptr_ty != NULL
                && LLVMTypeOf(fn_ptr) != callable_ptr_ty) {
                fn_ptr = LLVMBuildBitCast(ctx->builder, fn_ptr, callable_ptr_ty,
                    llvm_tmp_name(ctx));
            }
            if (fn_type == NULL && LLVMGetTypeKind(LLVMTypeOf(fn_ptr)) == LLVMFunctionTypeKind)
                fn_type = LLVMTypeOf(fn_ptr);
            if (fn_type != NULL && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind) {
                if (LLVMGetReturnType(fn_type) == ctx->type_void) {
                    LLVMBuildCall2(ctx->builder, fn_type, fn_ptr,
                        args, emitted_argc, "");
                    result = LLVMConstInt(ctx->type_i32, 0, 0);
                } else {
                    result = LLVMBuildCall2(ctx->builder, fn_type, fn_ptr,
                        args, emitted_argc, llvm_tmp_name(ctx));
                }
                free(args);
                return result;
            }
        }

        fprintf(stderr, "[llvm] warning: unknown function '%s'\n", callee_name);
        free(args);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    for (size_t i = 0; decl == NULL && i < argc; i++) {
        ASTNode *arg_node = node->data.call.arguments[i];
        unsigned param_count = LLVMCountParams(func->fn);
        LLVMTypeRef param_ty = (i < param_count)
            ? LLVMTypeOf(LLVMGetParam(func->fn, (unsigned)i))
            : NULL;
        if (param_ty != NULL
            && LLVMGetTypeKind(param_ty) == LLVMPointerTypeKind) {
            if (arg_node->type == AST_IDENTIFIER) {
                LLVMVarEntry *v = llvm_scope_lookup(ctx,
                    arg_node->data.identifier.name);
                ASTNode *callable_type = llvm_lookup_callable_var(ctx,
                    arg_node->data.identifier.name);
                if (v != NULL) {
                    if (callable_type != NULL) {
                        LLVMTypeRef callable_sig =
                            llvm_function_signature_from_event_type(ctx, callable_type);
                        LLVMTypeRef callable_ptr_ty = callable_sig != NULL
                            ? LLVMPointerType(callable_sig, 0)
                            : v->type;
                        args[i] = LLVMBuildLoad2(ctx->builder, callable_ptr_ty,
                            v->alloca, llvm_tmp_name(ctx));
                    } else if (LLVMGetTypeKind(v->type) == LLVMPointerTypeKind) {
                        args[i] = LLVMBuildLoad2(ctx->builder, v->type,
                            v->alloca, llvm_tmp_name(ctx));
                    } else {
                        args[i] = v->alloca;
                    }
                }
            } else if (arg_node->type == AST_MEMBER_ACCESS) {
                args[i] = llvm_emit_member_lvalue_ptr(arg_node, ctx, NULL);
            }
            if (args[i] == NULL && arg_node->type == AST_CALL) {
                LLVMValueRef maybe_value = llvm_emit_expression(arg_node, ctx);
                if (maybe_value != NULL
                    && LLVMGetTypeKind(LLVMTypeOf(maybe_value)) == LLVMPointerTypeKind) {
                    args[i] = maybe_value;
                }
            }
        }
    }

    LLVMValueRef result;
    if (func->ret_type == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                       args, emitted_argc, "");
        result = LLVMConstInt(ctx->type_i32, 0, 0);
    } else {
        result = LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                                args, emitted_argc, llvm_tmp_name(ctx));
    }

    free(args);
    return result;
}
