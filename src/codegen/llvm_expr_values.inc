static LLVMValueRef
llvm_emit_member_lvalue_ptr(ASTNode *node, LLVMGenCtx *ctx, LLVMTypeRef *out_field_type)
{
    ASTNode *obj_node;
    const char *field_name;
    const char *class_name;
    LLVMClassTypeEntry *cls;
    LLVMValueRef base_ptr = NULL;
    int field_idx;

    if (out_field_type != NULL)
        *out_field_type = NULL;
    if (node == NULL || node->type != AST_MEMBER_ACCESS)
        return NULL;

    obj_node = node->data.member.object;
    field_name = node->data.member.name;
    if (obj_node == NULL || field_name == NULL)
        return NULL;

    class_name = llvm_expr_custom_type_name(obj_node, ctx);
    if (class_name == NULL)
        return NULL;

    cls = llvm_lookup_class(ctx, class_name);
    if (cls == NULL)
        return NULL;

    if (obj_node->type == AST_IDENTIFIER) {
        const char *var_name = obj_node->data.identifier.name;
        base_ptr = llvm_identifier_base_ptr(ctx, var_name, cls);
        if (base_ptr == NULL)
            return NULL;
    } else if (obj_node->type == AST_MEMBER_ACCESS) {
        base_ptr = llvm_emit_member_lvalue_ptr(obj_node, ctx, NULL);
        if (base_ptr == NULL)
            return NULL;
    } else {
        return NULL;
    }

    field_idx = llvm_class_field_index(cls, field_name);
    if (field_idx < 0)
        return NULL;

    if (out_field_type != NULL)
        *out_field_type = cls->fields[field_idx].field_type;
    return LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
        (unsigned)field_idx, llvm_tmp_name(ctx));
}

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
        const char *mapped_source = refresh->data.zone_refresh.mapped_source_fields[i];
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

        snprintf(field_name, sizeof(field_name), "__projection_dirty_%s", target_slot);
        field_idx = llvm_class_field_index(host_cls, field_name);
        if (field_idx >= 0) {
            LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                host_cls->struct_type, host_ptr, (unsigned)field_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), dirty_ptr);
        }

        snprintf(field_name, sizeof(field_name), "__projection_ready_%s", target_slot);
        field_idx = llvm_class_field_index(host_cls, field_name);
        if (field_idx >= 0) {
            LLVMValueRef ready_ptr = LLVMBuildStructGEP2(ctx->builder,
                host_cls->struct_type, host_ptr, (unsigned)field_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), ready_ptr);
        }
    }
}

static bool
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
                       && strcmp(receiver->data.member.object->data.identifier.name, "self") == 0
                       && receiver->data.member.name != NULL) {
                zone_slot_name = receiver->data.member.name;
            }

            if (zone_slot_name != NULL) {
                zone_decl = llvm_resolve_world_zone_decl(ctx, world_decl, zone_slot_name);
                if (zone_decl != NULL
                    && llvm_find_zone_domain_slot_decl(zone_decl, slot_name) != NULL) {
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

static void
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
        refreshes, refresh_count, host_cls, host_ptr,
        source_slot, source_field);
}

static void
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
    if (sync_entry == NULL || sync_entry->fn == NULL || sync_entry->fn_type == NULL)
        return;

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

static LLVMValueRef
llvm_emit_assignment(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.assignment.target == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (node->data.assignment.target->type == AST_ARRAY_ACCESS) {
        ASTNode *array_node = node->data.assignment.target->data.array_access.array;
        if (array_node != NULL && array_node->type == AST_IDENTIFIER) {
            const char *name = array_node->data.identifier.name;
            LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, name);
            LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, name);
            LLVMValueRef idx = llvm_emit_expression(
                node->data.assignment.target->data.array_access.index, ctx);
            LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
            if (arr_var != NULL && entry != NULL && idx != NULL && val != NULL) {
                LLVMValueRef arr = LLVMBuildLoad2(ctx->builder, arr_var->type,
                    arr_var->alloca, llvm_tmp_name(ctx));
                LLVMValueRef data_ptr = llvm_array_data_ptr(ctx, arr);
                LLVMValueRef gep = LLVMBuildGEP2(ctx->builder, entry->elem_type,
                    data_ptr, &idx, 1, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, val, gep);
                return val;
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (node->data.assignment.target->type == AST_MEMBER_ACCESS) {
        LLVMTypeRef field_type = NULL;
        LLVMValueRef gep = llvm_emit_member_lvalue_ptr(
            node->data.assignment.target, ctx, &field_type);
        LLVMValueRef val;
        if (gep == NULL || field_type == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        val = llvm_emit_expression(node->data.assignment.value, ctx);
        if (val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(val) != field_type) {
            if ((field_type == ctx->type_i32 || field_type == ctx->type_i64)
                && (LLVMTypeOf(val) == ctx->type_f32 || LLVMTypeOf(val) == ctx->type_f64)) {
                val = LLVMBuildFPToSI(ctx->builder, val, field_type, llvm_tmp_name(ctx));
            } else if ((field_type == ctx->type_f32 || field_type == ctx->type_f64)
                && (LLVMTypeOf(val) == ctx->type_i32 || LLVMTypeOf(val) == ctx->type_i64)) {
                val = LLVMBuildSIToFP(ctx->builder, val, field_type, llvm_tmp_name(ctx));
            } else if ((field_type == ctx->type_i32 || field_type == ctx->type_i64)
                && (LLVMTypeOf(val) == ctx->type_i32 || LLVMTypeOf(val) == ctx->type_i64)) {
                val = (LLVMGetIntTypeWidth(field_type) > LLVMGetIntTypeWidth(LLVMTypeOf(val)))
                    ? LLVMBuildSExt(ctx->builder, val, field_type, llvm_tmp_name(ctx))
                    : LLVMBuildTrunc(ctx->builder, val, field_type, llvm_tmp_name(ctx));
            }
        }
        LLVMBuildStore(ctx->builder, val, gep);
        llvm_emit_host_projection_invalidations(ctx, node->data.assignment.target);
        llvm_emit_world_embedded_assignment_sync(ctx, node->data.assignment.target);
        return val;
    }

    const char *name = NULL;
    if (node->data.assignment.target->type == AST_IDENTIFIER)
        name = node->data.assignment.target->data.identifier.name;

    if (name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMVarEntry *var = llvm_scope_lookup(ctx, name);
    if (var == NULL && llvm_current_host_class_name(ctx) != NULL) {
        LLVMClassTypeEntry *cls =
            llvm_lookup_class(ctx, llvm_current_host_class_name(ctx));
        LLVMVarEntry *self_var = llvm_scope_lookup(ctx, "self");
        if (cls != NULL && self_var != NULL) {
            int field_idx = llvm_class_field_index(cls, name);
            if (field_idx >= 0) {
                LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
                LLVMValueRef base_ptr;
                LLVMValueRef gep;
                if (val == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                base_ptr = self_var->alloca;
                if (self_var->type == LLVMPointerType(cls->struct_type, 0))
                    base_ptr = LLVMBuildLoad2(ctx->builder, self_var->type,
                        self_var->alloca, llvm_tmp_name(ctx));
                gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
                    (unsigned)field_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, val, gep);
                llvm_emit_host_projection_invalidations(ctx, node->data.assignment.target);
                llvm_emit_world_embedded_assignment_sync(ctx, node->data.assignment.target);
                return val;
            }
        }
    }
    if (var == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    {
        const char *slot_inner = llvm_lookup_slot_inner(ctx, name);
        if (slot_inner != NULL) {
            LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
            if (val == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            char fn_name[64];
            snprintf(fn_name, sizeof(fn_name), "pgy_write_%s", slot_inner);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            if (fn != NULL) {
                LLVMValueRef args[] = { var->alloca, val };
                LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
            } else {
                if (llvm_lookup_slot_is_secure(ctx, name))
                    llvm_direct_secure_slot_write(ctx, var, val);
                else
                    llvm_direct_slot_write(ctx, var, val);
            }
            return val;
        }
    }

    {
        LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
        if (val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMBuildStore(ctx->builder, val, var->alloca);
        return val;
    }
}

static LLVMValueRef
llvm_emit_member_access(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *obj_node = node->data.member.object;
    const char *field_name = node->data.member.name;

    if (obj_node == NULL || field_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (llvm_is_upper_ident(obj_node)) {
        LLVMEnumVariantEntry *variant =
            llvm_lookup_enum_variant_qualified(ctx,
                obj_node->data.identifier.name, field_name);
        if (variant != NULL)
            return LLVMConstInt(ctx->type_i32,
                (unsigned long long)variant->value, 0);
    }

    {
        const char *class_name = llvm_expr_custom_type_name(obj_node, ctx);
        LLVMClassTypeEntry *cls;
        int field_idx;

        if (class_name == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        cls = llvm_lookup_class(ctx, class_name);
        if (cls == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        field_idx = llvm_class_field_index(cls, field_name);
        if (field_idx < 0)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        if (obj_node->type == AST_IDENTIFIER) {
            const char *var_name = obj_node->data.identifier.name;
            LLVMProjectionBorrowEntry *projection_borrow =
                llvm_lookup_projection_borrow(ctx, var_name);
            if (projection_borrow != NULL) {
                LLVMClassTypeEntry *source_cls;
                ASTNode *source_decl;
                LLVMVarEntry *source_var;
                const char *source_class_name = llvm_lookup_var_class(ctx,
                    projection_borrow->source_name);
                if (source_class_name == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                source_cls = llvm_lookup_class(ctx, source_class_name);
                source_decl = llvm_find_projection_nominal_decl(ctx, source_class_name);
                source_var = llvm_scope_lookup(ctx, projection_borrow->source_name);
                if (source_cls == NULL || source_decl == NULL || source_var == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                {
                    LLVMValueRef source_base = source_var->alloca;
                    if (source_var->type == LLVMPointerType(source_cls->struct_type, 0)) {
                        source_base = LLVMBuildLoad2(ctx->builder, source_var->type,
                            source_var->alloca, llvm_tmp_name(ctx));
                    }
                    return llvm_load_projection_path_value(ctx, source_decl, source_cls,
                        source_base, field_name);
                }
            }
            {
                LLVMValueRef base_ptr = llvm_identifier_base_ptr(ctx, var_name, cls);
                if (base_ptr != NULL) {
                    LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
                        cls->struct_type, base_ptr, (unsigned)field_idx,
                        llvm_tmp_name(ctx));
                    LLVMTypeRef field_type = cls->fields[field_idx].field_type;
                    return LLVMBuildLoad2(ctx->builder, field_type, gep,
                        llvm_tmp_name(ctx));
                }
            }
        }

        {
            LLVMValueRef obj_val = llvm_emit_expression(obj_node, ctx);
            if (obj_val == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);

            if (LLVMTypeOf(obj_val) == LLVMPointerType(cls->struct_type, 0)) {
                LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
                    cls->struct_type, obj_val, (unsigned)field_idx,
                    llvm_tmp_name(ctx));
                LLVMTypeRef field_type = cls->fields[field_idx].field_type;
                return LLVMBuildLoad2(ctx->builder, field_type, gep,
                    llvm_tmp_name(ctx));
            }

            if (LLVMTypeOf(obj_val) == cls->struct_type) {
                return LLVMBuildExtractValue(ctx->builder, obj_val,
                    (unsigned)field_idx, llvm_tmp_name(ctx));
            }
        }
    }

    return LLVMConstInt(ctx->type_i32, 0, 0);
}
