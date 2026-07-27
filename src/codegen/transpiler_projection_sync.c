/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend zone/world projection synchronization owner.
 */

#include "transpiler_projection_sync.h"

#include <string.h>

#include "../common/string_compat.h"
#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_receiver_query.h"
#include "transpiler_inventory_view.h"
#include "transpiler_overlay_zone_bind.h"
#include "transpiler_projection.h"

bool
emit_world_embedded_action_authority_check(
    TranspilerCtx *ctx,
    ASTNode *receiver,
    const MIRDeclMethod *method_meta,
    char **check_out)
{
    const MIRDeclHeader *zone_header;
    const MIRDeclZoneAuthority *authority = NULL;
    const char *zone_slot_name = NULL;
    const char *zone_type_name = NULL;
    const char *source_slot_name = NULL;
    const char *source_type_name = NULL;
    const char *method_within_zone;
    const char *authorized_by;
    ASTNode *host_decl;

    if (check_out == NULL)
        return false;
    *check_out = NULL;
    if (ctx == NULL || receiver == NULL || method_meta == NULL)
        return false;
    if (!transpiler_mir_decl_method_is_action_like(method_meta))
        return true;
    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl == NULL || host_decl->type != AST_WORLD_DECL)
        return true;
    if (transpiler_mir_decl_method_authorized_by_count(method_meta) == 0)
        return true;
    if (transpiler_mir_decl_method_authorized_by_count(method_meta) != 1) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path cannot bind multiple authorities for a world-embedded action");
        return false;
    }
    authorized_by =
        transpiler_mir_decl_method_authorized_by(method_meta, 0);
    if (authorized_by == NULL || strcmp(authorized_by, "self") != 0) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path cannot bind named authority '%s' for a world-embedded action",
            authorized_by != NULL ? authorized_by : "<missing>");
        return false;
    }

    method_within_zone =
        transpiler_mir_decl_method_within_zone(method_meta);
    if (method_within_zone == NULL) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing within-zone metadata for self-authorized action");
        return false;
    }
    if (!transpiler_resolve_world_zone_subject_receiver(ctx, receiver,
            &zone_slot_name, &zone_type_name,
            &source_slot_name, &source_type_name)
        || zone_slot_name == NULL || zone_type_name == NULL
        || source_slot_name == NULL || source_type_name == NULL
        || strcmp(method_within_zone, zone_type_name) != 0) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path cannot bind indirect world action authority to exact zone/subject slots");
        return false;
    }

    zone_header = transpiler_active_decl_header_of_type(
        ctx, AST_ZONE_DECL, zone_type_name);
    if (zone_header == NULL) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing zone declaration header for self-authorized action in '%s'",
            zone_type_name);
        return false;
    }
    for (size_t i = 0;
         i < mir_decl_header_zone_authority_count(zone_header); i++) {
        const MIRDeclZoneAuthority *candidate =
            mir_decl_header_zone_authority(zone_header, i);
        const char *candidate_slot =
            mir_decl_zone_authority_subject_slot_name(candidate);
        if (candidate_slot != NULL
            && strcmp(candidate_slot, source_slot_name) == 0) {
            authority = candidate;
            break;
        }
    }
    if (authority == NULL) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing exact zone authority for self-authorized action '%s.%s'",
            zone_type_name, source_slot_name);
        return false;
    }

    *check_out = pergyra_strdup_printf(
        "PGY_ZONE_AUTHORITY_CHECK(&self->%s, &self->%s.%s, \"%s\", \"%s\"); ",
        zone_slot_name, zone_slot_name, source_slot_name,
        zone_type_name, source_slot_name);
    if (*check_out == NULL) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path failed to materialize the world action authority check");
        return false;
    }
    return true;
}

void
emit_zone_action_effect_runtime(CodeBuf *out, ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *callee;
    ASTNode *receiver;
    ASTNode *host_decl;
    ASTNode *zone_decl;
    const MIRDeclMethod *method_meta;
    const char *method_name;
    const char *receiver_slot_name = NULL;
    const char *receiver_type_name = NULL;
    const char *effect_name;
    const char *method_within_zone;
    const char *active_zone_name = NULL;
    bool method_is_async;
    bool method_is_action;

    if (out == NULL || ctx == NULL || call == NULL
        || call->type != AST_CALL) {
        return;
    }

    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl == NULL || host_decl->type != AST_ZONE_DECL)
        return;
    active_zone_name = transpiler_decl_name_local(host_decl);
    if (active_zone_name == NULL)
        return;

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

    method_meta = transpiler_find_host_method_metadata_in_context(ctx,
        receiver_type_name, method_name);
    method_is_async = transpiler_mir_decl_method_is_async(method_meta);
    method_is_action = transpiler_mir_decl_method_is_action_like(method_meta);
    method_within_zone = transpiler_mir_decl_method_within_zone(method_meta);
    effect_name = transpiler_mir_decl_method_causes_effect(method_meta);
    if (method_meta == NULL) {
        if (transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing zone action method metadata");
            return;
        }
        return;
    }
    if (method_meta != NULL && transpiler_active_has_mir(ctx)
        && method_is_action && effect_name != NULL
        && method_within_zone == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing zone action within-zone metadata for effect '%s'",
            effect_name);
        return;
    }
    if (method_is_async || !method_is_action
        || method_within_zone == NULL
        || effect_name == NULL
        || strcmp(method_within_zone, active_zone_name) != 0) {
        return;
    }

    zone_decl = host_decl;
    if (zone_decl == NULL)
        return;

    TranspilerHostedZoneLayerSlotView layer_view =
        transpiler_hosted_zone_layer_slot_view_from_decl(
            ctx, active_zone_name, zone_decl);
    if (transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(
            &layer_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing zone action effect layer metadata for '%s'",
            active_zone_name != NULL ? active_zone_name : "(anonymous-zone)");
        return;
    }
    for (size_t i = 0; i < layer_view.count; i++) {
        const char *layer_name;
        const char *layer_type =
            transpiler_hosted_zone_layer_slot_view_type_name(
                &layer_view, i);

        if (transpiler_hosted_zone_layer_slot_view_is_relation(&layer_view, i)
            || layer_type == NULL
            || strcmp(layer_type, effect_name) != 0) {
            continue;
        }

        layer_name =
            transpiler_hosted_zone_layer_slot_view_name(&layer_view, i);
        if (layer_name == NULL)
            continue;

        write_indent_to(out, ctx->indent);
        codebuf_write(out, "self->__layer_active_%s = true;\n", layer_name);
        emit_zone_bind_effect_layer(out, zone_decl, layer_name,
                                    receiver_slot_name, ctx);
    }
}

char *
emit_world_embedded_action_effect_sync(TranspilerCtx *ctx,
                                       ASTNode *receiver,
                                       const MIRDeclMethod *method_meta)
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
    const char *effect_type_name;
    const char *method_within_zone;
    bool method_is_async;
    bool method_is_action;

    if (ctx == NULL || receiver == NULL)
        return NULL;

    method_is_async = transpiler_mir_decl_method_is_async(method_meta);
    method_is_action = transpiler_mir_decl_method_is_action_like(method_meta);
    method_within_zone = transpiler_mir_decl_method_within_zone(method_meta);
    effect_name = transpiler_mir_decl_method_causes_effect(method_meta);
    if (method_meta == NULL) {
        if (transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing world effect sync method metadata");
            return NULL;
        }
        return NULL;
    }
    if (method_meta != NULL && transpiler_active_has_mir(ctx)
        && method_is_action && effect_name != NULL
        && method_within_zone == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing world effect sync within-zone metadata for effect '%s'",
            effect_name);
        return NULL;
    }
    if (method_is_async || !method_is_action
        || method_within_zone == NULL || effect_name == NULL)
        return NULL;

    world_decl = transpiler_current_host_decl_local(ctx);
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return NULL;

    if (!transpiler_resolve_world_zone_subject_receiver(ctx, receiver,
            &zone_slot_name, &zone_type_name,
            &source_slot_name, &source_type_name)
        || zone_slot_name == NULL || zone_type_name == NULL
        || source_slot_name == NULL
        || strcmp(method_within_zone, zone_type_name) != 0) {
        return NULL;
    }

    zone_decl = transpiler_resolve_world_zone_decl(ctx, world_decl,
                                                   zone_slot_name);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return NULL;

    effect_decl = transpiler_find_named_decl_local(ctx, AST_EFFECT_DECL,
                                                   effect_name);
    if (effect_decl == NULL || effect_decl->type != AST_EFFECT_DECL)
        return NULL;
    effect_type_name = transpiler_decl_name_local(effect_decl);
    if (effect_type_name == NULL)
        return NULL;

    TranspilerHostedZoneLayerSlotView layer_view =
        transpiler_hosted_zone_layer_slot_view_from_decl(
            ctx, zone_type_name, zone_decl);
    if (transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(
            &layer_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing world embedded effect layer metadata for '%s'",
            zone_type_name != NULL ? zone_type_name : "(anonymous-zone)");
        return NULL;
    }
    buf = codebuf_create();
    TranspilerHostedZoneStateView state_view =
        transpiler_hosted_zone_state_view_from_decl(ctx, zone_type_name,
                                                    zone_decl);
    if (transpiler_hosted_zone_state_view_missing_mir_metadata(&state_view)
        || !transpiler_hosted_zone_state_view_rows_complete(&state_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing world embedded effect state metadata for '%s'",
            zone_type_name != NULL ? zone_type_name : "(anonymous-zone)");
        codebuf_destroy(buf);
        return NULL;
    }
    TranspilerHostedZoneRefreshView effect_refresh_view =
        transpiler_hosted_zone_refresh_view_from_decl(ctx, effect_type_name,
                                                      effect_decl);
    TranspilerHostedDomainSlotView effect_slot_view =
        transpiler_hosted_domain_slot_view_from_decl(ctx, effect_type_name,
                                                     effect_decl);
    if (transpiler_hosted_zone_refresh_view_missing_mir_metadata(
            &effect_refresh_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing world embedded effect refresh metadata for '%s'",
            effect_type_name);
        codebuf_destroy(buf);
        return NULL;
    }
    if (transpiler_hosted_domain_slot_view_missing_mir_metadata(
            &effect_slot_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing world embedded effect domain-slot metadata for '%s'",
            effect_type_name);
        codebuf_destroy(buf);
        return NULL;
    }
    for (size_t i = 0; i < layer_view.count; i++) {
        const char *layer_name;
        const char *target_slot_name;
        const char *layer_type =
            transpiler_hosted_zone_layer_slot_view_type_name(
                &layer_view, i);
        int tmp_id;

        if (transpiler_hosted_zone_layer_slot_view_is_relation(&layer_view, i)
            || layer_type == NULL
            || strcmp(layer_type, effect_name) != 0) {
            continue;
        }

        layer_name =
            transpiler_hosted_zone_layer_slot_view_name(&layer_view, i);
        if (layer_name == NULL)
            continue;

        target_slot_name = transpiler_domain_slot_view_bindable_name(
            &effect_slot_view, 0);
        if (target_slot_name == NULL)
            continue;

        codebuf_write(buf, "self->%s.__layer_active_%s = true; ",
            zone_slot_name, layer_name);
        codebuf_write(buf,
            "self->%s.__layer_epoch_%s++; "
            "self->%s.__layer_cause_%s = 11; ",
            zone_slot_name, layer_name,
            zone_slot_name, layer_name);
        for (size_t si = 0; si < state_view.count; si++) {
            const char *state_name =
                transpiler_hosted_zone_state_view_name(&state_view, si);
            const char *state_layer_name =
                transpiler_hosted_zone_state_view_layer_slot_name(
                    &state_view, si);

            if (transpiler_hosted_zone_state_view_is_relation(&state_view, si)
                || state_name == NULL
                || state_layer_name == NULL
                || strcmp(state_layer_name, layer_name) != 0) {
                continue;
            }
            codebuf_write(buf,
                "self->%s.__state_epoch_%s++; "
                "self->%s.__state_cause_%s = 11; ",
                zone_slot_name, state_name,
                zone_slot_name, state_name);
        }

        if (transpiler_hosted_zone_layer_slot_view_is_pool(&layer_view, i)) {
            tmp_id = ++ctx->tmp_counter;
            codebuf_write(buf,
                "{ %s _pgy_world_effect_%d = (%s){0}; "
                "_pgy_world_effect_%d.%s = self->%s.%s; ",
                effect_type_name, tmp_id,
                effect_type_name,
                tmp_id, target_slot_name,
                zone_slot_name, source_slot_name);
            for (size_t ri = 0; ri < effect_refresh_view.count; ri++) {
                const char *projection_name;
                const char *refresh_source;
                projection_name =
                    transpiler_hosted_zone_refresh_view_object_slot_name(
                        &effect_refresh_view, ri);
                refresh_source =
                    transpiler_hosted_zone_refresh_view_source_slot_name(
                        &effect_refresh_view, ri);
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
                effect_type_name, tmp_id,
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
        for (size_t ri = 0; ri < effect_refresh_view.count; ri++) {
            const char *projection_name;
            const char *refresh_source;
            projection_name =
                transpiler_hosted_zone_refresh_view_object_slot_name(
                    &effect_refresh_view, ri);
            refresh_source =
                transpiler_hosted_zone_refresh_view_source_slot_name(
                    &effect_refresh_view, ri);
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
            effect_type_name,
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
