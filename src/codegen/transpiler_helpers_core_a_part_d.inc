
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

static const char *
