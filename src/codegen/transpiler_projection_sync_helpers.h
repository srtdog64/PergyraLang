#ifndef PGY_TRANSPILER_PROJECTION_SYNC_HELPERS_H
#define PGY_TRANSPILER_PROJECTION_SYNC_HELPERS_H

#include "parser/ast_api.h"
#include "transpiler_domain_receiver_query.h"
#include "transpiler_projection_method_invalidation.h"

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
    active_zone_name = ast_zone_name(host_decl);

    callee = ast_call_callee(call);
    if (callee == NULL || callee->type != AST_MEMBER_ACCESS)
        return;

    receiver = ast_member_object(callee);
    method_name = ast_member_name(callee);
    if (receiver == NULL || method_name == NULL)
        return;

    if (!transpiler_resolve_zone_subject_receiver(ctx, receiver,
            &receiver_slot_name, &receiver_type_name)) {
        return;
    }

    method_decl = transpiler_find_subject_host_method_decl(ctx,
        receiver_type_name, method_name);
    if (method_decl == NULL || method_decl->type != AST_FUNC_DECL
        || method_decl->is_async_decl
        || !ast_func_is_action(method_decl)
        || ast_func_within_zone(method_decl) == NULL
        || ast_func_causes_effect(method_decl) == NULL
        || strcmp(ast_func_within_zone(method_decl), active_zone_name) != 0) {
        return;
    }

    zone_decl = host_decl;
    if (zone_decl == NULL)
        return;

    effect_name = ast_func_causes_effect(method_decl);
    size_t layer_slot_count = 0;
    ASTNode **layer_slots = ast_zone_layer_slots(zone_decl, &layer_slot_count);
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *layer_slot = layer_slots[i];
        const char *layer_name;

        if (layer_slot == NULL || layer_slot->type != AST_ZONE_LAYER_SLOT
            || ast_zone_layer_slot_is_relation(layer_slot)
            || ast_zone_layer_slot_layer_type(layer_slot) == NULL
            || strcmp(ast_zone_layer_slot_layer_type(layer_slot), effect_name) != 0) {
            continue;
        }

        layer_name = ast_zone_layer_slot_name(layer_slot);
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
        || !ast_func_is_action(method_decl)
        || ast_func_within_zone(method_decl) == NULL
        || ast_func_causes_effect(method_decl) == NULL) {
        return NULL;
    }

    world_decl = transpiler_current_host_decl_local(ctx);
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return NULL;

    if (!transpiler_resolve_world_zone_subject_receiver(ctx, receiver,
            &zone_slot_name, &zone_type_name,
            &source_slot_name, &source_type_name)
        || zone_slot_name == NULL || zone_type_name == NULL
        || source_slot_name == NULL
        || strcmp(ast_func_within_zone(method_decl), zone_type_name) != 0) {
        return NULL;
    }

    zone_decl = find_zone_decl(ctx, zone_type_name);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return NULL;

    effect_name = ast_func_causes_effect(method_decl);
    effect_decl = find_effect_decl(ctx, effect_name);
    if (effect_decl == NULL || effect_decl->type != AST_EFFECT_DECL)
        return NULL;

    buf = codebuf_create();
    size_t layer_slot_count = 0;
    ASTNode **layer_slots = ast_zone_layer_slots(zone_decl, &layer_slot_count);
    size_t state_count = 0;
    ASTNode **states = ast_zone_states(zone_decl, &state_count);
    size_t effect_slot_count = 0;
    ASTNode **effect_slots = ast_effect_slots(effect_decl, &effect_slot_count);
    size_t effect_refresh_count = 0;
    ASTNode **effect_refreshes =
        ast_effect_refreshes(effect_decl, &effect_refresh_count);
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *layer_slot = layer_slots[i];
        ASTNode *target_slot;
        const char *layer_name;
        int tmp_id;

        if (layer_slot == NULL || layer_slot->type != AST_ZONE_LAYER_SLOT
            || ast_zone_layer_slot_is_relation(layer_slot)
            || ast_zone_layer_slot_layer_type(layer_slot) == NULL
            || strcmp(ast_zone_layer_slot_layer_type(layer_slot), effect_name) != 0) {
            continue;
        }

        layer_name = ast_zone_layer_slot_name(layer_slot);
        if (layer_name == NULL)
            continue;

        target_slot = find_nth_bindable_domain_slot_local(effect_slots,
            effect_slot_count, effect_refreshes, effect_refresh_count, 0);
        const char *target_slot_name = ast_domain_slot_name(target_slot);
        if (target_slot == NULL || target_slot_name == NULL)
            continue;

        codebuf_write(buf, "self->%s.__layer_active_%s = true; ",
            zone_slot_name, layer_name);
        codebuf_write(buf,
            "self->%s.__layer_epoch_%s++; "
            "self->%s.__layer_cause_%s = 11; ",
            zone_slot_name, layer_name,
            zone_slot_name, layer_name);
        for (size_t si = 0; si < state_count; si++) {
            ASTNode *state = states[si];
            if (state == NULL || state->type != AST_ZONE_STATE
                || ast_zone_state_is_relation(state)
                || ast_zone_state_name(state) == NULL
                || ast_zone_state_layer_slot_name(state) == NULL
                || strcmp(ast_zone_state_layer_slot_name(state), layer_name) != 0) {
                continue;
            }
            codebuf_write(buf,
                "self->%s.__state_epoch_%s++; "
                "self->%s.__state_cause_%s = 11; ",
                zone_slot_name, ast_zone_state_name(state),
                zone_slot_name, ast_zone_state_name(state));
        }

        if (ast_zone_layer_slot_is_pool(layer_slot)) {
            tmp_id = ++ctx->tmp_counter;
            codebuf_write(buf,
                "{ %s _pgy_world_effect_%d = (%s){0}; "
                "_pgy_world_effect_%d.%s = self->%s.%s; ",
                ast_effect_name(effect_decl), tmp_id,
                ast_effect_name(effect_decl),
                tmp_id, target_slot_name,
                zone_slot_name, source_slot_name);
            for (size_t ri = 0; ri < effect_refresh_count; ri++) {
                ASTNode *refresh = effect_refreshes[ri];
                const char *projection_name;
                const char *refresh_source;
                if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                    continue;
                projection_name = ast_zone_refresh_object_slot_name(refresh);
                refresh_source = ast_zone_refresh_source_slot_name(refresh);
                if (projection_name == NULL || refresh_source == NULL
                    || strcmp(refresh_source, target_slot_name) != 0) {
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
                ast_effect_name(effect_decl), tmp_id,
                zone_slot_name, layer_name, tmp_id,
                zone_slot_name, layer_name,
                zone_slot_name, layer_name);
            continue;
        }

        codebuf_write(buf, "self->%s.%s.%s = self->%s.%s; ",
            zone_slot_name,
            layer_name,
            target_slot_name,
            zone_slot_name,
            source_slot_name);
        for (size_t ri = 0; ri < effect_refresh_count; ri++) {
            ASTNode *refresh = effect_refreshes[ri];
            const char *projection_name;
            const char *refresh_source;
            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            projection_name = ast_zone_refresh_object_slot_name(refresh);
            refresh_source = ast_zone_refresh_source_slot_name(refresh);
            if (projection_name == NULL || refresh_source == NULL
                || strcmp(refresh_source, target_slot_name) != 0) {
                continue;
            }
            codebuf_write(buf,
                "self->%s.%s.__projection_dirty_%s = true; "
                "self->%s.%s.__projection_ready_%s = false; ",
                zone_slot_name, layer_name, projection_name,
                zone_slot_name, layer_name, projection_name);
        }
        codebuf_write(buf, "%s_sync(&self->%s.%s); ",
            ast_effect_name(effect_decl),
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

#endif /* PGY_TRANSPILER_PROJECTION_SYNC_HELPERS_H */
