
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

static void
append_overlay_method_projection_invalidations(CodeBuf *buf,
                                               TranspilerCtx *ctx,
                                               const char *source_slot_name,
                                               const char *host_type_name,
                                               ASTNode *node,
                                               int depth)
{
    if (buf == NULL || ctx == NULL || source_slot_name == NULL
        || host_type_name == NULL || node == NULL || depth > 8) {
        return;
    }

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                node->data.block.statements[i], depth + 1);
        }
        break;
    case AST_IF_STMT:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.if_stmt.then_branch, depth + 1);
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.if_stmt.else_branch, depth + 1);
        break;
    case AST_FOR_LOOP:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.for_loop.body, depth + 1);
        break;
    case AST_WHILE_LOOP:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.while_loop.body, depth + 1);
        break;
    case AST_MATCH_STMT:
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                node->data.match_stmt.cases[i], depth + 1);
        }
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.match_stmt.default_body, depth + 1);
        break;
    case AST_MATCH_CASE:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.match_case.body, depth + 1);
        break;
    case AST_SELECT_STMT:
        for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                node->data.select_stmt.cases[i], depth + 1);
        }
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.select_stmt.default_case, depth + 1);
        break;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                node->data.async_block.statements[i], depth + 1);
        }
        break;
    case AST_ASSIGNMENT: {
        const char *field_name = method_assignment_projection_field_name(
            ctx, host_type_name, node->data.assignment.target);
        if (field_name != NULL) {
            char *invalidation = emit_current_overlay_projection_invalidation(
                ctx, source_slot_name, field_name);
            if (invalidation != NULL) {
                codebuf_write(buf, "%s", invalidation);
                free(invalidation);
            }
        }
        break;
    }
    case AST_CALL:
        if (node->data.call.callee != NULL
            && node->data.call.callee->type == AST_MEMBER_ACCESS
            && node->data.call.callee->data.member.object != NULL
            && node->data.call.callee->data.member.object->type == AST_IDENTIFIER
            && node->data.call.callee->data.member.object->data.identifier.name != NULL
            && node->data.call.callee->data.member.name != NULL) {
            ASTNode *host_decl = find_class_decl(ctx, host_type_name);
            ClassField *field = NULL;

            if (host_decl != NULL && host_decl->type == AST_CLASS_DECL) {
                field = find_host_field_by_name_local(host_decl,
                    node->data.call.callee->data.member.object->data.identifier.name);
            }
            if (field != NULL && field->is_vessel_field
                && field->type != NULL
                && field->type->type == AST_TYPE
                && field->type->data.type.name != NULL) {
                ASTNode *method_decl = find_nominal_host_method_decl(
                    ctx, field->type->data.type.name,
                    node->data.call.callee->data.member.name);
                if (method_decl != NULL) {
                    append_overlay_method_projection_invalidations(
                        buf, ctx, source_slot_name,
                        field->type->data.type.name,
                        method_decl->data.func_decl.body, depth + 1);
                }
            }
        }
        break;
    default:
        break;
    }
}

static char *
emit_current_overlay_method_projection_invalidation(TranspilerCtx *ctx,
                                                    const char *source_slot_name,
                                                    const char *host_type_name,
                                                    ASTNode *method_decl)
{
    CodeBuf *buf;

    if (ctx == NULL || source_slot_name == NULL || host_type_name == NULL
        || method_decl == NULL || method_decl->type != AST_FUNC_DECL
        || method_decl->data.func_decl.body == NULL) {
        return NULL;
    }

    buf = codebuf_create();
    append_overlay_method_projection_invalidations(
        buf, ctx, source_slot_name, host_type_name,
        method_decl->data.func_decl.body, 0);

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
current_world_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_WORLD_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
        ASTNode *slot = decl->data.world_decl.rosters[i];
        if (slot != NULL && slot->data.world_roster.slot_name != NULL
            && strcmp(slot->data.world_roster.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
        ASTNode *slot = decl->data.world_decl.zones[i];
        if (slot != NULL && slot->data.world_zone.slot_name != NULL
            && strcmp(slot->data.world_zone.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.world_decl.shared_count; i++) {
        ASTNode *shared = decl->data.world_decl.shared_fields[i];
        if (shared != NULL && shared->data.party_shared.name != NULL
            && strcmp(shared->data.party_shared.name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

static ASTNode *
find_zone_state_decl(ASTNode *zone_decl, const char *state_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone_decl->data.zone_decl.state_count; i++) {
        ASTNode *state = zone_decl->data.zone_decl.states[i];
        if (state != NULL && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            return state;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_domain_slot(ASTNode *zone_decl, const char *slot_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_layer_slot(ASTNode *zone_decl, const char *slot_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
resolve_world_zone_decl(TranspilerCtx *ctx, ASTNode *world_decl, const char *slot_name);

static const char *
zone_subject_slot_type_name(ASTNode *zone_decl, const char *slot_name)
{
    ASTNode *slot = find_zone_domain_slot(zone_decl, slot_name);
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
        || !slot->data.domain_slot.is_subject
        || slot->data.domain_slot.type == NULL
        || slot->data.domain_slot.type->type != AST_TYPE) {
        return NULL;
    }
    return slot->data.domain_slot.type->data.type.name;
}

static ASTNode *
find_subject_host_method_decl(TranspilerCtx *ctx, const char *type_name,
                              const char *method_name)
{
    ASTNode *decl;

    if (ctx == NULL || type_name == NULL || method_name == NULL)
        return NULL;

    decl = find_class_decl(ctx, type_name);
    if (decl != NULL && decl->type == AST_CLASS_DECL) {
        for (size_t i = 0; i < decl->data.class_decl.method_count; i++) {
            ASTNode *method = decl->data.class_decl.methods[i];
            if (method != NULL && method->type == AST_FUNC_DECL
                && method->data.func_decl.name != NULL
                && strcmp(method->data.func_decl.name, method_name) == 0) {
                return method;
            }
        }
    }

    return NULL;
}

static bool
resolve_zone_subject_receiver(TranspilerCtx *ctx, ASTNode *receiver,
                              const char **slot_name_out,
                              const char **type_name_out)
{
    ASTNode *zone_decl;
    const char *slot_name = NULL;
    const char *type_name = NULL;

    if (slot_name_out != NULL)
        *slot_name_out = NULL;
    if (type_name_out != NULL)
        *type_name_out = NULL;

    if (ctx == NULL || receiver == NULL)
        return false;

    zone_decl = transpiler_current_host_decl_local(ctx);
    if (zone_decl != NULL && zone_decl->type != AST_ZONE_DECL)
        zone_decl = NULL;
    if (zone_decl == NULL)
        return false;

    if (receiver->type == AST_IDENTIFIER && receiver->data.identifier.name != NULL) {
        slot_name = receiver->data.identifier.name;
        type_name = zone_subject_slot_type_name(zone_decl, slot_name);
        if (type_name == NULL) {
            const char *var_type = lookup_typed_var(ctx, slot_name);
            if (var_type != NULL && is_subject_type_name(ctx, var_type))
                type_name = var_type;
        }
    } else if (receiver->type == AST_MEMBER_ACCESS
               && receiver->data.member.object != NULL
               && receiver->data.member.object->type == AST_IDENTIFIER
               && receiver->data.member.object->data.identifier.name != NULL
               && strcmp(receiver->data.member.object->data.identifier.name, "self") == 0
               && receiver->data.member.name != NULL) {
        slot_name = receiver->data.member.name;
        type_name = zone_subject_slot_type_name(zone_decl, slot_name);
    }

    if (slot_name == NULL || type_name == NULL)
        return false;

    if (slot_name_out != NULL)
        *slot_name_out = slot_name;
    if (type_name_out != NULL)
        *type_name_out = type_name;
    return true;
}

static bool
resolve_world_zone_subject_receiver(TranspilerCtx *ctx, ASTNode *receiver,
                                    const char **zone_slot_name_out,
                                    const char **zone_type_name_out,
                                    const char **slot_name_out,
                                    const char **type_name_out)
{
    ASTNode *world_decl;
    ASTNode *zone_decl;
    ASTNode *zone_expr;
    const char *zone_slot_name = NULL;
    const char *zone_type_name = NULL;
    const char *slot_name = NULL;
    const char *type_name = NULL;

    if (zone_slot_name_out != NULL)
        *zone_slot_name_out = NULL;
    if (zone_type_name_out != NULL)
        *zone_type_name_out = NULL;
    if (slot_name_out != NULL)
        *slot_name_out = NULL;
    if (type_name_out != NULL)
        *type_name_out = NULL;

    if (ctx == NULL
        || receiver == NULL || receiver->type != AST_MEMBER_ACCESS) {
        return false;
    }

    zone_expr = receiver->data.member.object;
    slot_name = receiver->data.member.name;
    if (zone_expr == NULL || slot_name == NULL)
        return false;

    if (zone_expr->type == AST_IDENTIFIER && zone_expr->data.identifier.name != NULL) {
        zone_slot_name = zone_expr->data.identifier.name;
    } else if (zone_expr->type == AST_MEMBER_ACCESS
               && zone_expr->data.member.object != NULL
               && zone_expr->data.member.object->type == AST_IDENTIFIER
               && zone_expr->data.member.object->data.identifier.name != NULL
               && strcmp(zone_expr->data.member.object->data.identifier.name, "self") == 0
               && zone_expr->data.member.name != NULL) {
        zone_slot_name = zone_expr->data.member.name;
    } else {
        return false;
    }

    world_decl = transpiler_current_host_decl_local(ctx);
    if (world_decl != NULL && world_decl->type != AST_WORLD_DECL)
        world_decl = NULL;
    if (world_decl == NULL)
        return false;

    zone_decl = resolve_world_zone_decl(ctx, world_decl, zone_slot_name);
    if (zone_decl == NULL)
        return false;

    zone_type_name = zone_decl->data.zone_decl.name;
    type_name = zone_subject_slot_type_name(zone_decl, slot_name);
    if (zone_type_name == NULL || type_name == NULL)
        return false;

    if (zone_slot_name_out != NULL)
        *zone_slot_name_out = zone_slot_name;
    if (zone_type_name_out != NULL)
        *zone_type_name_out = zone_type_name;
    if (slot_name_out != NULL)
        *slot_name_out = slot_name;
    if (type_name_out != NULL)
        *type_name_out = type_name;
    return true;
}

static void
emit_zone_action_effect_runtime(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *callee;
    ASTNode *receiver;
    ASTNode *host_decl;
    ASTNode *zone_decl;
    ASTNode *method_decl;
    const char *method_name;
    const char *receiver_slot_name = NULL;
    const char *receiver_type_name = NULL;
    const char *effect_name;
    const char *active_zone_name = NULL;

    if (ctx == NULL || call == NULL
        || call->type != AST_CALL) {
        return;
    }

    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl == NULL || host_decl->type != AST_ZONE_DECL)
        return;
    active_zone_name = host_decl->data.zone_decl.name;

    callee = call->data.call.callee;
    if (callee == NULL || callee->type != AST_MEMBER_ACCESS)
        return;

    receiver = callee->data.member.object;
    method_name = callee->data.member.name;
    if (receiver == NULL || method_name == NULL)
        return;

    if (!resolve_zone_subject_receiver(ctx, receiver,
            &receiver_slot_name, &receiver_type_name)) {
        return;
    }

    method_decl = find_subject_host_method_decl(ctx, receiver_type_name, method_name);
    if (method_decl == NULL || method_decl->type != AST_FUNC_DECL
        || method_decl->is_async_decl
        || !method_decl->data.func_decl.is_action
        || method_decl->data.func_decl.within_zone == NULL
        || method_decl->data.func_decl.causes_effect == NULL
        || strcmp(method_decl->data.func_decl.within_zone, active_zone_name) != 0) {
        return;
    }

    zone_decl = host_decl;
    if (zone_decl == NULL)
        return;

    effect_name = method_decl->data.func_decl.causes_effect;
    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer_slot = zone_decl->data.zone_decl.layer_slots[i];
        const char *layer_name;

        if (layer_slot == NULL || layer_slot->type != AST_ZONE_LAYER_SLOT
            || layer_slot->data.zone_layer_slot.is_relation
            || layer_slot->data.zone_layer_slot.layer_type == NULL
            || strcmp(layer_slot->data.zone_layer_slot.layer_type, effect_name) != 0) {
            continue;
        }

        layer_name = layer_slot->data.zone_layer_slot.slot_name;
        if (layer_name == NULL)
            continue;
