#ifndef PGY_TRANSPILER_PROJECTION_SYNC_HELPERS_H
#define PGY_TRANSPILER_PROJECTION_SYNC_HELPERS_H

#include "parser/ast_api.h"
#include "transpiler_projection_method_invalidation.h"

static const char *
zone_subject_slot_type_name(ASTNode *zone_decl, const char *slot_name)
{
    ASTNode *slot = transpiler_find_zone_domain_slot(zone_decl, slot_name);
    ASTNode *slot_type;
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
        || !ast_domain_slot_is_subject(slot)) {
        return NULL;
    }
    slot_type = ast_domain_slot_type(slot);
    if (slot_type == NULL || slot_type->type != AST_TYPE)
        return NULL;
    return ast_type_name(slot_type);
}

static ASTNode *
find_subject_host_method_decl(TranspilerCtx *ctx, const char *type_name,
                              const char *method_name)
{
    ASTNode *decl;
    ASTNode *method;

    if (ctx == NULL || type_name == NULL || method_name == NULL)
        return NULL;

    decl = find_class_decl(ctx, type_name);
    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return NULL;

    method = find_nominal_host_method_decl(ctx, type_name, method_name);
    if (method == NULL || method->type != AST_FUNC_DECL)
        return NULL;
    return method;
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

    if (receiver->type == AST_IDENTIFIER && ast_identifier_name(receiver) != NULL) {
        slot_name = ast_identifier_name(receiver);
        type_name = zone_subject_slot_type_name(zone_decl, slot_name);
        if (type_name == NULL) {
            const char *var_type = lookup_typed_var(ctx, slot_name);
            if (var_type != NULL && is_subject_type_name(ctx, var_type))
                type_name = var_type;
        }
    } else if (receiver->type == AST_MEMBER_ACCESS
               && ast_member_object(receiver) != NULL
               && ast_member_object(receiver)->type == AST_IDENTIFIER
               && ast_identifier_name(ast_member_object(receiver)) != NULL
               && strcmp(ast_identifier_name(ast_member_object(receiver)), "self") == 0
               && ast_member_name(receiver) != NULL) {
        slot_name = ast_member_name(receiver);
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

    zone_expr = ast_member_object(receiver);
    slot_name = ast_member_name(receiver);
    if (zone_expr == NULL || slot_name == NULL)
        return false;

    if (zone_expr->type == AST_IDENTIFIER && ast_identifier_name(zone_expr) != NULL) {
        zone_slot_name = ast_identifier_name(zone_expr);
    } else if (zone_expr->type == AST_MEMBER_ACCESS
               && ast_member_object(zone_expr) != NULL
               && ast_member_object(zone_expr)->type == AST_IDENTIFIER
               && ast_identifier_name(ast_member_object(zone_expr)) != NULL
               && strcmp(ast_identifier_name(ast_member_object(zone_expr)), "self") == 0
               && ast_member_name(zone_expr) != NULL) {
        zone_slot_name = ast_member_name(zone_expr);
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

    zone_type_name = ast_zone_name(zone_decl);
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
    active_zone_name = ast_zone_name(host_decl);

    callee = ast_call_callee(call);
    if (callee == NULL || callee->type != AST_MEMBER_ACCESS)
        return;

    receiver = ast_member_object(callee);
    method_name = ast_member_name(callee);
    if (receiver == NULL || method_name == NULL)
        return;

    if (!resolve_zone_subject_receiver(ctx, receiver,
            &receiver_slot_name, &receiver_type_name)) {
        return;
    }

    method_decl = find_subject_host_method_decl(ctx, receiver_type_name, method_name);
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

    if (!resolve_world_zone_subject_receiver(ctx, receiver,
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

static ASTNode *
find_world_state_decl(ASTNode *world_decl, const char *state_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    size_t state_count = 0;
    ASTNode **states = ast_world_states(world_decl, &state_count);
    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && ast_world_state_name(state) != NULL
            && strcmp(ast_world_state_name(state), state_name) == 0) {
            return state;
        }
    }
    return NULL;
}

#endif /* PGY_TRANSPILER_PROJECTION_SYNC_HELPERS_H */
