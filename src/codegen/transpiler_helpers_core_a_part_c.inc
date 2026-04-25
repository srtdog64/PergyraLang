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

static const char *
zone_subject_slot_type_name(ASTNode *zone_decl, const char *slot_name)
{
    ASTNode *slot = transpiler_find_zone_domain_slot(zone_decl, slot_name);
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

    zone_decl = transpiler_resolve_world_zone_decl(ctx, world_decl, zone_slot_name);
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


        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n", layer_name);
        emit_zone_bind_effect_layer(ctx->out, zone_decl, layer_name, receiver_slot_name, ctx);
    }
}

static char *
emit_world_embedded_action_effect_sync(TranspilerCtx *ctx,
                                       ASTNode *receiver,
                                       ASTNode *method_decl)
{
    ASTNode *world_decl;
    ASTNode *zone_decl;
    ASTNode *effect_decl;
    CodeBuf *buf;
    const char *zone_slot_name = NULL;
    const char *zone_type_name = NULL;
    const char *source_slot_name = NULL;
    const char *source_type_name = NULL;
    const char *effect_name;

    if (ctx == NULL || receiver == NULL || method_decl == NULL
        || method_decl->type != AST_FUNC_DECL
        || method_decl->is_async_decl
        || !method_decl->data.func_decl.is_action
        || method_decl->data.func_decl.within_zone == NULL
        || method_decl->data.func_decl.causes_effect == NULL) {
        return NULL;
    }

    world_decl = transpiler_current_host_decl_local(ctx);
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return NULL;

    if (!resolve_world_zone_subject_receiver(ctx, receiver,
            &zone_slot_name, &zone_type_name,
            &source_slot_name, &source_type_name)
        || zone_slot_name == NULL || zone_type_name == NULL
        || source_slot_name == NULL
        || strcmp(method_decl->data.func_decl.within_zone, zone_type_name) != 0) {
        return NULL;
    }

    zone_decl = find_zone_decl(ctx, zone_type_name);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return NULL;

    effect_name = method_decl->data.func_decl.causes_effect;
    effect_decl = find_effect_decl(ctx, effect_name);
    if (effect_decl == NULL || effect_decl->type != AST_EFFECT_DECL)
        return NULL;

    buf = codebuf_create();
    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer_slot = zone_decl->data.zone_decl.layer_slots[i];
        ASTNode *target_slot;
        const char *layer_name;
        int tmp_id;

        if (layer_slot == NULL || layer_slot->type != AST_ZONE_LAYER_SLOT
            || layer_slot->data.zone_layer_slot.is_relation
            || layer_slot->data.zone_layer_slot.layer_type == NULL
            || strcmp(layer_slot->data.zone_layer_slot.layer_type, effect_name) != 0) {
            continue;
        }

        layer_name = layer_slot->data.zone_layer_slot.slot_name;
        if (layer_name == NULL)
            continue;

        target_slot = find_nth_bindable_domain_slot_local(
            effect_decl->data.effect_decl.slots,
            effect_decl->data.effect_decl.slot_count,
            effect_decl->data.effect_decl.refreshes,
            effect_decl->data.effect_decl.refresh_count, 0);
        if (target_slot == NULL || target_slot->data.domain_slot.slot_name == NULL)
            continue;

        codebuf_write(buf, "self->%s.__layer_active_%s = true; ",
            zone_slot_name, layer_name);
        codebuf_write(buf,
            "self->%s.__layer_epoch_%s++; "
            "self->%s.__layer_cause_%s = 11; ",
            zone_slot_name, layer_name,
            zone_slot_name, layer_name);
        for (size_t si = 0; si < zone_decl->data.zone_decl.state_count; si++) {
            ASTNode *state = zone_decl->data.zone_decl.states[si];
            if (state == NULL || state->type != AST_ZONE_STATE
                || state->data.zone_state.is_relation
                || state->data.zone_state.state_name == NULL
                || state->data.zone_state.layer_slot_name == NULL
                || strcmp(state->data.zone_state.layer_slot_name, layer_name) != 0) {
                continue;
            }
            codebuf_write(buf,
                "self->%s.__state_epoch_%s++; "
                "self->%s.__state_cause_%s = 11; ",
                zone_slot_name, state->data.zone_state.state_name,
                zone_slot_name, state->data.zone_state.state_name);
        }

        if (layer_slot->data.zone_layer_slot.is_pool) {
            tmp_id = ++ctx->tmp_counter;
            codebuf_write(buf,
                "{ %s _pgy_world_effect_%d = (%s){0}; "
                "_pgy_world_effect_%d.%s = self->%s.%s; ",
                effect_decl->data.effect_decl.name, tmp_id,
                effect_decl->data.effect_decl.name,
                tmp_id, target_slot->data.domain_slot.slot_name,
                zone_slot_name, source_slot_name);
            for (size_t ri = 0; ri < effect_decl->data.effect_decl.refresh_count; ri++) {
                ASTNode *refresh = effect_decl->data.effect_decl.refreshes[ri];
                const char *projection_name;
                const char *refresh_source;
                if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                    continue;
                projection_name = refresh->data.zone_refresh.object_slot_name;
                refresh_source = refresh->data.zone_refresh.source_slot_name;
                if (projection_name == NULL || refresh_source == NULL
                    || strcmp(refresh_source, target_slot->data.domain_slot.slot_name) != 0) {
                    continue;
                }
                codebuf_write(buf,
                    "_pgy_world_effect_%d.__projection_dirty_%s = true; "
                    "_pgy_world_effect_%d.__projection_ready_%s = false; ",
                    tmp_id, projection_name,
                    tmp_id, projection_name);
            }
            codebuf_write(buf,
                "%s_sync(&_pgy_world_effect_%d); "
                "PGY_EFFECT_POOL_APPLY(self->%s.%s, _pgy_world_effect_%d); "
                "self->%s.__layer_active_%s = PGY_EFFECT_POOL_ACTIVE_COUNT(self->%s.%s) > 0; } ",
                effect_decl->data.effect_decl.name, tmp_id,
                zone_slot_name, layer_name, tmp_id,
                zone_slot_name, layer_name,
                zone_slot_name, layer_name);
            continue;
        }

        codebuf_write(buf, "self->%s.%s.%s = self->%s.%s; ",
            zone_slot_name,
            layer_name,
            target_slot->data.domain_slot.slot_name,
            zone_slot_name,
            source_slot_name);
        for (size_t ri = 0; ri < effect_decl->data.effect_decl.refresh_count; ri++) {
            ASTNode *refresh = effect_decl->data.effect_decl.refreshes[ri];
            const char *projection_name;
            const char *refresh_source;
            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            projection_name = refresh->data.zone_refresh.object_slot_name;
            refresh_source = refresh->data.zone_refresh.source_slot_name;
            if (projection_name == NULL || refresh_source == NULL
                || strcmp(refresh_source, target_slot->data.domain_slot.slot_name) != 0) {
                continue;
            }
            codebuf_write(buf,
                "self->%s.%s.__projection_dirty_%s = true; "
                "self->%s.%s.__projection_ready_%s = false; ",
                zone_slot_name, layer_name, projection_name,
                zone_slot_name, layer_name, projection_name);
        }
        codebuf_write(buf, "%s_sync(&self->%s.%s); ",
            effect_decl->data.effect_decl.name,
            zone_slot_name,
            layer_name);
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

static ASTNode *
find_world_state_decl(ASTNode *world_decl, const char *state_name)
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
