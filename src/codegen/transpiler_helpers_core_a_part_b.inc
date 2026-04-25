static bool
is_pointer_self_host_type_name(TranspilerCtx *ctx, const char *type_name)
{
    ASTNode *decl;

    if (type_name == NULL)
        return false;
    if (is_subject_type_name(ctx, type_name))
        return true;
    decl = find_class_decl(ctx, type_name);
    if (decl != NULL
        && decl->type == AST_CLASS_DECL
        && decl->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL)
        return true;
    return find_relation_decl(ctx, type_name) != NULL
        || find_effect_decl(ctx, type_name) != NULL
        || find_zone_decl(ctx, type_name) != NULL
        || find_world_decl(ctx, type_name) != NULL;
}

static bool
current_class_uses_self_cell(TranspilerCtx *ctx)
{
    ASTNode *host_decl = NULL;
    const char *class_name = NULL;

    if (ctx == NULL)
        return false;
    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl != NULL && host_decl->type == AST_CLASS_DECL)
        class_name = host_decl->data.class_decl.name;
    return ctx != NULL
        && class_name != NULL
        && is_pointer_self_host_type_name(ctx, class_name);
}

static bool
current_class_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_CLASS_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
        ClassField *field = decl->data.class_decl.fields[i];
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
current_zone_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_ZONE_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = decl->data.zone_decl.slots[i];
        if (slot != NULL && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = decl->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.zone_decl.shared_count; i++) {
        ASTNode *shared = decl->data.zone_decl.shared_fields[i];
        if (shared != NULL && shared->data.party_shared.name != NULL
            && strcmp(shared->data.party_shared.name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
current_relation_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_RELATION_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
        ASTNode *slot = decl->data.relation_decl.slots[i];
        if (slot != NULL && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.relation_decl.shared_count; i++) {
        ASTNode *shared = decl->data.relation_decl.shared_fields[i];
        if (shared != NULL && shared->data.party_shared.name != NULL
            && strcmp(shared->data.party_shared.name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
current_effect_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_EFFECT_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
        ASTNode *slot = decl->data.effect_decl.slots[i];
        if (slot != NULL && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.effect_decl.shared_count; i++) {
        ASTNode *shared = decl->data.effect_decl.shared_fields[i];
        if (shared != NULL && shared->data.party_shared.name != NULL
            && strcmp(shared->data.party_shared.name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
domain_slot_is_projection_target_local(ASTNode *slot,
                                       ASTNode **refreshes,
                                       size_t refresh_count);

static bool
domain_slot_is_projection_target_local(ASTNode *slot,
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
find_nth_bindable_domain_slot_local(ASTNode **slots, size_t slot_count,
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

static void
emit_zone_bind_effect_layer(CodeBuf *out, ASTNode *zone, const char *layer_slot_name,
                            const char *target_slot_name, TranspilerCtx *ctx)
{
    ASTNode *layer_slot;
    ASTNode *effect_decl;
    ASTNode *target_slot;

    if (out == NULL || zone == NULL || layer_slot_name == NULL
        || target_slot_name == NULL || ctx == NULL) {
        return;
    }

    layer_slot = NULL;
    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && !slot->data.zone_layer_slot.is_relation
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, layer_slot_name) == 0) {
            layer_slot = slot;
            break;
        }
    }
    if (layer_slot == NULL)
        return;

    effect_decl = find_effect_decl(ctx, layer_slot->data.zone_layer_slot.layer_type);
    if (effect_decl == NULL)
        return;

    target_slot = find_nth_bindable_domain_slot_local(effect_decl->data.effect_decl.slots,
        effect_decl->data.effect_decl.slot_count,
        effect_decl->data.effect_decl.refreshes,
        effect_decl->data.effect_decl.refresh_count, 0);
    if (target_slot == NULL || target_slot->data.domain_slot.slot_name == NULL)
        return;

    if (layer_slot->data.zone_layer_slot.is_pool) {
        write_indent(ctx);
        codebuf_write(out, "{\n");
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(out, "%s _pgy_%s_instance = (%s){0};\n",
            effect_decl->data.effect_decl.name,
            layer_slot_name,
            effect_decl->data.effect_decl.name);
        write_indent(ctx);
        codebuf_write(out, "_pgy_%s_instance.%s = self->%s;\n",
            layer_slot_name,
            target_slot->data.domain_slot.slot_name,
            target_slot_name);
        for (size_t i = 0; i < effect_decl->data.effect_decl.refresh_count; i++) {
            ASTNode *refresh = effect_decl->data.effect_decl.refreshes[i];
            const char *projection_name;
            const char *source_name;

            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            projection_name = refresh->data.zone_refresh.object_slot_name;
            source_name = refresh->data.zone_refresh.source_slot_name;
            if (projection_name == NULL || source_name == NULL
                || strcmp(source_name, target_slot->data.domain_slot.slot_name) != 0) {
                continue;
            }
            write_indent(ctx);
            codebuf_write(out,
                "_pgy_%s_instance.__projection_dirty_%s = true;\n",
                layer_slot_name, projection_name);
            write_indent(ctx);
            codebuf_write(out,
                "_pgy_%s_instance.__projection_ready_%s = false;\n",
                layer_slot_name, projection_name);
        }
        write_indent(ctx);
        codebuf_write(out, "%s_sync(&_pgy_%s_instance);\n",
            effect_decl->data.effect_decl.name,
            layer_slot_name);
        write_indent(ctx);
        codebuf_write(out, "PGY_EFFECT_POOL_APPLY(self->%s, _pgy_%s_instance);\n",
            layer_slot_name,
            layer_slot_name);
        write_indent(ctx);
        codebuf_write(out, "self->__layer_active_%s = PGY_EFFECT_POOL_ACTIVE_COUNT(self->%s) > 0;\n",
            layer_slot_name,
            layer_slot_name);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(out, "}\n");
        return;
    }

    write_indent(ctx);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name,
        target_slot->data.domain_slot.slot_name,
        target_slot_name);
    for (size_t i = 0; i < effect_decl->data.effect_decl.refresh_count; i++) {
        ASTNode *refresh = effect_decl->data.effect_decl.refreshes[i];
        const char *projection_name;
        const char *source_name;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;
        projection_name = refresh->data.zone_refresh.object_slot_name;
        source_name = refresh->data.zone_refresh.source_slot_name;
        if (projection_name == NULL || source_name == NULL
            || strcmp(source_name, target_slot->data.domain_slot.slot_name) != 0) {
            continue;
        }
        write_indent(ctx);
        codebuf_write(out,
            "self->%s.__projection_dirty_%s = true;\n",
            layer_slot_name, projection_name);
        write_indent(ctx);
        codebuf_write(out,
            "self->%s.__projection_ready_%s = false;\n",
            layer_slot_name, projection_name);
    }
    write_indent(ctx);
    codebuf_write(out, "%s_sync(&self->%s);\n",
        effect_decl->data.effect_decl.name,
        layer_slot_name);
}

static void
emit_zone_bind_relation_layer(CodeBuf *out, ASTNode *zone, const char *layer_slot_name,
                              const char *left_slot_name, const char *right_slot_name,
                              TranspilerCtx *ctx)
{
    ASTNode *layer_slot;
    ASTNode *relation_decl;
    ASTNode *left_target;
    ASTNode *right_target;

    if (out == NULL || zone == NULL || layer_slot_name == NULL
        || left_slot_name == NULL || right_slot_name == NULL || ctx == NULL) {
        return;
    }

    layer_slot = NULL;
    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.is_relation
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, layer_slot_name) == 0) {
            layer_slot = slot;
            break;
        }
    }
    if (layer_slot == NULL)
        return;

    relation_decl = find_relation_decl(ctx, layer_slot->data.zone_layer_slot.layer_type);
    if (relation_decl == NULL)
        return;

    left_target = find_nth_bindable_domain_slot_local(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count, 0);
    right_target = find_nth_bindable_domain_slot_local(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count, 1);
    if (left_target == NULL || right_target == NULL
        || left_target->data.domain_slot.slot_name == NULL
        || right_target->data.domain_slot.slot_name == NULL) {
        return;
    }

    write_indent(ctx);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name, left_target->data.domain_slot.slot_name, left_slot_name);
    write_indent(ctx);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name, right_target->data.domain_slot.slot_name, right_slot_name);
    for (size_t i = 0; i < relation_decl->data.relation_decl.refresh_count; i++) {
        ASTNode *refresh = relation_decl->data.relation_decl.refreshes[i];
        const char *projection_name;
        const char *source_name;

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
        write_indent(ctx);
        codebuf_write(out,
            "self->%s.__projection_dirty_%s = true;\n",
            layer_slot_name, projection_name);
        write_indent(ctx);
        codebuf_write(out,
            "self->%s.__projection_ready_%s = false;\n",
            layer_slot_name, projection_name);
    }
    write_indent(ctx);
    codebuf_write(out, "%s_sync(&self->%s);\n",
        relation_decl->data.relation_decl.name,
        layer_slot_name);
}

static ASTNode **
current_overlay_refresh_list(TranspilerCtx *ctx, size_t *refresh_count_out)
{
    ASTNode *decl;

    if (refresh_count_out != NULL)
        *refresh_count_out = 0;
    if (ctx == NULL)
        return NULL;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type == AST_RELATION_DECL) {
        if (refresh_count_out != NULL)
            *refresh_count_out = decl->data.relation_decl.refresh_count;
        return decl->data.relation_decl.refreshes;
    }
    if (decl != NULL && decl->type == AST_EFFECT_DECL) {
        if (refresh_count_out != NULL)
            *refresh_count_out = decl->data.effect_decl.refresh_count;
        return decl->data.effect_decl.refreshes;
    }
    if (decl != NULL && decl->type == AST_ZONE_DECL) {
        if (refresh_count_out != NULL)
            *refresh_count_out = decl->data.zone_decl.refresh_count;
        return decl->data.zone_decl.refreshes;
    }

    return NULL;
}

static const char *
assignment_target_root_slot_name(ASTNode *target)
{
    if (target == NULL)
        return NULL;
    if (target->type == AST_IDENTIFIER)
        return target->data.identifier.name;
    if (target->type == AST_MEMBER_ACCESS) {
        if (target->data.member.object != NULL
            && target->data.member.object->type == AST_IDENTIFIER
            && target->data.member.object->data.identifier.name != NULL
            && strcmp(target->data.member.object->data.identifier.name, "self") == 0) {
            return target->data.member.name;
        }
        return assignment_target_root_slot_name(target->data.member.object);
    }
    return NULL;
}

static const char *
assignment_target_root_subfield_name(ASTNode *target)
{
    if (target == NULL || target->type != AST_MEMBER_ACCESS)
        return NULL;

    if (target->data.member.object != NULL
        && target->data.member.object->type == AST_MEMBER_ACCESS) {
        ASTNode *inner = target->data.member.object;
        if (inner->data.member.object != NULL
            && inner->data.member.object->type == AST_IDENTIFIER
            && inner->data.member.object->data.identifier.name != NULL
            && strcmp(inner->data.member.object->data.identifier.name, "self") == 0) {
            return target->data.member.name;
        }
        return assignment_target_root_subfield_name(inner);
    }

    return NULL;
}

static ASTNode *
current_overlay_domain_slot_decl(TranspilerCtx *ctx, const char *slot_name)
{
    ASTNode *decl;

    if (ctx == NULL || slot_name == NULL)
        return NULL;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type == AST_RELATION_DECL) {
        for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
            ASTNode *slot = decl->data.relation_decl.slots[i];
            if (slot != NULL && slot->type == AST_DOMAIN_SLOT
                && slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
                return slot;
            }
        }
    }
    if (decl != NULL && decl->type == AST_EFFECT_DECL) {
        for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
            ASTNode *slot = decl->data.effect_decl.slots[i];
            if (slot != NULL && slot->type == AST_DOMAIN_SLOT
                && slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
                return slot;
            }
        }
    }
    if (decl != NULL && decl->type == AST_ZONE_DECL)
        return transpiler_find_zone_domain_slot(decl, slot_name);

    return NULL;
}

static bool
projection_target_mentions_source_field(TranspilerCtx *ctx,
                                        ASTNode *refresh,
                                        const char *target_slot_name,
                                        const char *source_field_name)
{
    ASTNode *slot_decl;
    ASTNode *target_decl;
    const char *target_type_name;

    if (ctx == NULL || target_slot_name == NULL)
        return false;
    if (source_field_name == NULL)
        return true;

    slot_decl = current_overlay_domain_slot_decl(ctx, target_slot_name);
    if (slot_decl == NULL
        || slot_decl->data.domain_slot.type == NULL
        || slot_decl->data.domain_slot.type->type != AST_TYPE
        || slot_decl->data.domain_slot.type->data.type.name == NULL) {
        return true;
    }

    target_type_name = slot_decl->data.domain_slot.type->data.type.name;
    target_decl = find_class_decl(ctx, target_type_name);
    if (target_decl == NULL || target_decl->type != AST_CLASS_DECL)
        return true;

    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *field = target_decl->data.class_decl.fields[i];
        const char *mapped_source_name = field != NULL ? field->name : NULL;
        if (refresh != NULL && refresh->type == AST_ZONE_REFRESH && field != NULL
            && field->name != NULL) {
            for (size_t j = 0; j < refresh->data.zone_refresh.field_map_count; j++) {
                const char *mapped_target =
                    refresh->data.zone_refresh.mapped_target_fields[j];
                const char *mapped_source =
                    refresh->data.zone_refresh.mapped_source_fields[j];
                if (mapped_target != NULL && mapped_source != NULL
                    && strcmp(mapped_target, field->name) == 0) {
                    mapped_source_name = mapped_source;
                    break;
                }
            }
        }
        if (mapped_source_name != NULL
            && strcmp(mapped_source_name, source_field_name) == 0) {
            return true;
        }
    }

    return false;
}

static char *
emit_current_overlay_projection_invalidation(TranspilerCtx *ctx,
                                            const char *source_slot_name,
                                            const char *source_field_name)
{
    ASTNode **refreshes;
    size_t refresh_count = 0;
    CodeBuf *buf;
    const char *receiver_expr;


    if (ctx == NULL || source_slot_name == NULL)
        return NULL;

    receiver_expr = ctx->current_overlay_receiver_expr != NULL
        ? ctx->current_overlay_receiver_expr
        : "self";

    refreshes = current_overlay_refresh_list(ctx, &refresh_count);
    if (refreshes == NULL || refresh_count == 0)
        return NULL;

    buf = codebuf_create();
    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        const char *target_name;
        const char *refresh_source_name;

        if (refresh == NULL)
            continue;
        target_name = refresh->data.zone_refresh.object_slot_name;
        refresh_source_name = refresh->data.zone_refresh.source_slot_name;
        if (target_name == NULL || refresh_source_name == NULL
            || strcmp(refresh_source_name, source_slot_name) != 0) {
            continue;
        }
        if (!projection_target_mentions_source_field(ctx, refresh,
                target_name, source_field_name))
            continue;
        codebuf_write(buf,
            "%s->__projection_dirty_%s = true; "
            "%s->__projection_ready_%s = false; ",
            receiver_expr,
            target_name,
            receiver_expr,
            target_name);
    }

    if (buf->len == 0) {
        codebuf_destroy(buf);
        return NULL;
    }

    {
        char *result = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
        return result;
    }
}

static bool
resolve_world_embedded_projection_invalidation(TranspilerCtx *ctx,
                                               ASTNode *target,
                                               const char **zone_slot_name_out,
                                               const char **zone_type_name_out,
                                               const char **source_slot_name_out,
                                               const char **source_field_name_out)
{
    ASTNode *cursor = target;
    const char *source_field_name = NULL;

    if (zone_slot_name_out != NULL)
        *zone_slot_name_out = NULL;
    if (zone_type_name_out != NULL)
        *zone_type_name_out = NULL;
    if (source_slot_name_out != NULL)
        *source_slot_name_out = NULL;
    if (source_field_name_out != NULL)
        *source_field_name_out = NULL;

    if (ctx == NULL || target == NULL)
        return false;

    while (cursor != NULL && cursor->type == AST_MEMBER_ACCESS) {
        const char *zone_slot_name = NULL;
        const char *zone_type_name = NULL;
        const char *source_slot_name = NULL;
        const char *source_type_name = NULL;

        if (resolve_world_zone_subject_receiver(ctx, cursor,
                &zone_slot_name, &zone_type_name,
                &source_slot_name, &source_type_name)
            && zone_slot_name != NULL
            && zone_type_name != NULL
            && source_slot_name != NULL) {
            if (zone_slot_name_out != NULL)
                *zone_slot_name_out = zone_slot_name;
            if (zone_type_name_out != NULL)
                *zone_type_name_out = zone_type_name;
            if (source_slot_name_out != NULL)
                *source_slot_name_out = source_slot_name;
            if (source_field_name_out != NULL)
                *source_field_name_out = source_field_name;
            return true;
        }

        source_field_name = cursor->data.member.name;
        cursor = cursor->data.member.object;
    }

    return false;
}

static char *
emit_assignment_projection_invalidation(TranspilerCtx *ctx, ASTNode *target)
{
    const char *root_slot_name;
    const char *root_subfield_name;
    char *invalidation;

    if (ctx == NULL || target == NULL)
        return NULL;

    root_slot_name = assignment_target_root_slot_name(target);
    root_subfield_name = assignment_target_root_subfield_name(target);
    invalidation = emit_current_overlay_projection_invalidation(
        ctx, root_slot_name, root_subfield_name);
    if (invalidation != NULL)
        return invalidation;

    {
        ASTNode *saved_host_decl = transpiler_current_host_decl_local(ctx);
        const char *saved_receiver_expr = ctx->current_overlay_receiver_expr;

        if (saved_host_decl != NULL && saved_host_decl->type == AST_WORLD_DECL) {
            const char *zone_slot_name = NULL;
            const char *zone_type_name = NULL;
            const char *source_slot_name = NULL;
            const char *source_field_name = NULL;

            if (resolve_world_embedded_projection_invalidation(ctx, target,
                    &zone_slot_name, &zone_type_name,
                    &source_slot_name, &source_field_name)
                && zone_slot_name != NULL
                && zone_type_name != NULL
                && source_slot_name != NULL) {
                ASTNode *zone_decl = find_zone_decl(ctx, zone_type_name);
                if (zone_decl != NULL)
                    transpiler_bind_current_host_decl_local(ctx, zone_decl);
                ctx->current_overlay_receiver_expr =
                    strdup_fmt("(&self->%s)", zone_slot_name);
                invalidation = emit_current_overlay_projection_invalidation(
                    ctx, source_slot_name, source_field_name);
                if (ctx->current_overlay_receiver_expr != NULL
                    && ctx->current_overlay_receiver_expr != saved_receiver_expr) {
                    free((char *)ctx->current_overlay_receiver_expr);
                }
                ctx->current_overlay_receiver_expr = saved_receiver_expr;
                transpiler_bind_current_host_decl_local(ctx, saved_host_decl);
                return invalidation;
            }
        }
    }

    return NULL;
}

static char *
emit_world_embedded_assignment_sync(TranspilerCtx *ctx, ASTNode *target)
{
    ASTNode *host_decl;
    const char *zone_slot_name = NULL;
    const char *zone_type_name = NULL;
    const char *source_slot_name = NULL;
    const char *source_field_name = NULL;

    if (ctx == NULL || target == NULL)
        return NULL;

    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl == NULL || host_decl->type != AST_WORLD_DECL)
        return NULL;

    if (!resolve_world_embedded_projection_invalidation(ctx, target,
            &zone_slot_name, &zone_type_name,
            &source_slot_name, &source_field_name)
        || zone_slot_name == NULL
        || zone_type_name == NULL
        || source_slot_name == NULL) {
        return NULL;
    }

    return strdup_fmt("%s_sync(&self->%s); ", zone_type_name, zone_slot_name);
}

static char *
emit_world_embedded_receiver_projection_sync(TranspilerCtx *ctx, ASTNode *receiver)
{
    ASTNode *host_decl;
    ASTNode *zone_decl;
    CodeBuf *buf;
    const char *zone_slot_name = NULL;
    const char *zone_type_name = NULL;
    const char *source_slot_name = NULL;
    const char *source_type_name = NULL;

    if (ctx == NULL || receiver == NULL)
        return NULL;

    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl == NULL || host_decl->type != AST_WORLD_DECL)
        return NULL;

    if (!resolve_world_zone_subject_receiver(ctx, receiver,
            &zone_slot_name, &zone_type_name,
            &source_slot_name, &source_type_name)
        || zone_slot_name == NULL
        || zone_type_name == NULL) {
        return NULL;
    }

    zone_decl = find_zone_decl(ctx, zone_type_name);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return NULL;

    buf = codebuf_create();
    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.slot_name == NULL
            || slot->data.domain_slot.is_subject) {
            continue;
        }
        codebuf_write(buf,
            "self->%s.__projection_dirty_%s = true; "
            "self->%s.__projection_ready_%s = false; ",
            zone_slot_name, slot->data.domain_slot.slot_name,
            zone_slot_name, slot->data.domain_slot.slot_name);
    }
    codebuf_write(buf,
        "%s_sync(&self->%s); "
        "self->__zone_dirty_%s = true; "
        "self->__world_derived_dirty = true; "
        "%s_sync(self); ",
        zone_type_name, zone_slot_name,
        zone_slot_name,
        host_decl->data.world_decl.name != NULL
            ? host_decl->data.world_decl.name : "World");

    {
        char *result = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
        return result;
    }
}

static bool
host_projection_relevant_field_exists(TranspilerCtx *ctx,
                                      const char *host_type_name,
                                      const char *field_name)
{
    ASTNode *host_decl;

    if (ctx == NULL || host_type_name == NULL || field_name == NULL)
        return false;

    host_decl = find_class_decl(ctx, host_type_name);
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL)
        return false;

    for (size_t i = 0; i < host_decl->data.class_decl.field_count; i++) {
        ClassField *field = host_decl->data.class_decl.fields[i];
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return !field->is_vessel_field;
        }
    }

    return false;
}

static ClassField *
find_host_field_by_name_local(ASTNode *host_decl, const char *field_name)
{
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL
        || field_name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < host_decl->data.class_decl.field_count; i++) {
        ClassField *field = host_decl->data.class_decl.fields[i];
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return field;
        }
    }

    return NULL;
}

static const char *
method_assignment_projection_field_name(TranspilerCtx *ctx,
                                        const char *host_type_name,
                                        ASTNode *target)
{
    ASTNode *cursor = target;
    ASTNode *host_decl;

    if (ctx == NULL || host_type_name == NULL || target == NULL)
        return NULL;

    host_decl = find_class_decl(ctx, host_type_name);
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL)
        return NULL;

    if (target->type == AST_IDENTIFIER
        && host_projection_relevant_field_exists(ctx, host_type_name,
            target->data.identifier.name)) {
        return target->data.identifier.name;
    }

    while (cursor != NULL && cursor->type == AST_MEMBER_ACCESS) {
        ASTNode *obj = cursor->data.member.object;
        if (obj != NULL && obj->type == AST_IDENTIFIER
            && obj->data.identifier.name != NULL) {
            ClassField *field = find_host_field_by_name_local(
                host_decl, obj->data.identifier.name);
            if (field != NULL) {
                if (field->is_vessel_field)
                    return cursor->data.member.name;
                return obj->data.identifier.name;
            }
        }
        cursor = obj;
    }

    return NULL;
}
