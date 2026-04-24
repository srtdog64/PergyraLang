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
        ASTNode *host_decl = llvm_current_host_decl(ctx);
        const char *host_name = llvm_decl_node_name(host_decl);
        ASTNode *decl = (host_decl != NULL
            && (host_decl->type == AST_RELATION_DECL
                || host_decl->type == AST_EFFECT_DECL
                || host_decl->type == AST_ZONE_DECL))
            ? host_decl : NULL;
        LLVMClassTypeEntry *cls = host_name != NULL
            ? llvm_lookup_class(ctx, host_name) : NULL;
        const char *slot_name = NULL;
        int field_idx;
        LLVMValueRef base_ptr;
        LLVMValueRef gep;
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
        ASTNode *host_decl = llvm_current_host_decl(ctx);
        const char *host_name = llvm_decl_node_name(host_decl);
        ASTNode *zone_decl = host_decl != NULL && host_decl->type == AST_ZONE_DECL
            ? host_decl : NULL;
        LLVMClassTypeEntry *cls = host_name != NULL
            ? llvm_lookup_class(ctx, host_name) : NULL;
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
        ASTNode *host_decl = llvm_current_host_decl(ctx);
        const char *host_name = llvm_decl_node_name(host_decl);
        ASTNode *zone_decl = host_decl != NULL && host_decl->type == AST_ZONE_DECL
            ? host_decl : NULL;
        LLVMClassTypeEntry *cls = host_name != NULL
            ? llvm_lookup_class(ctx, host_name) : NULL;
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
        ASTNode *host_decl = llvm_current_host_decl(ctx);
        const char *host_name = llvm_decl_node_name(host_decl);
        ASTNode *world_decl = host_decl != NULL && host_decl->type == AST_WORLD_DECL
            ? host_decl : NULL;
        LLVMClassTypeEntry *cls = host_name != NULL
            ? llvm_lookup_class(ctx, host_name) : NULL;
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
        ASTNode *host_decl = llvm_current_host_decl(ctx);
        const char *host_name = llvm_decl_node_name(host_decl);
        ASTNode *world_decl = (host_decl != NULL && host_decl->type == AST_WORLD_DECL)
            ? host_decl
            : llvm_find_named_domain_decl(ctx, AST_WORLD_DECL, host_name);
        LLVMClassTypeEntry *world_cls = llvm_lookup_class(ctx, host_name);
        const char *zone_name = NULL;
        const char *detail_name = NULL;
        ASTNode *zone_decl;
        LLVMClassTypeEntry *zone_cls;
        int zone_idx;
        int field_idx = -1;
        LLVMValueRef world_ptr;
        LLVMValueRef zone_ptr;
        LLVMValueRef gep;

        if (host_name == NULL || world_decl == NULL || world_cls == NULL)
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
                char *normalized = llvm_normalize_banner_string_literal_scratch(
                    raw, &ctx->scratch);
                if (normalized != NULL) {
                    arg = LLVMBuildGlobalStringPtr(ctx->builder, normalized,
                                                  llvm_tmp_name(ctx));
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
            char *normalized = llvm_normalize_banner_string_literal_scratch(
                arg->data.string.value, &ctx->scratch);
            if (normalized != NULL) {
                log_arg = LLVMBuildGlobalStringPtr(ctx->builder, normalized,
                                                   llvm_tmp_name(ctx));
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
        llvm_set_error_at_with_hints(ctx, node, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM standalone %s requires an explicitly typed binding; use 'let value: %s<T> = %s()'",
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
            llvm_set_error_at_with_hints(ctx, slot_arg, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM DeviceWrite on '%s' requires a concrete DeviceSlot<T> inner type",
                slot_arg->data.identifier.name);
            return NULL;
        }
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_device_write_%s", inner);
