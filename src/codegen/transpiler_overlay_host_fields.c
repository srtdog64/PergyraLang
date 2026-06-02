/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend overlay host-field lookup helpers.
 */

#include "transpiler_overlay_host_fields.h"

#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_host_self_policy.h"

static bool
domain_slot_view_has_field(TranspilerCtx *ctx,
                           ASTNode *decl,
                           const char *field_name)
{
    const char *host_name;
    TranspilerHostedDomainSlotView slot_view;

    if (ctx == NULL || decl == NULL || field_name == NULL)
        return false;

    host_name = transpiler_decl_name_local(decl);
    slot_view = transpiler_hosted_domain_slot_view_from_decl(ctx, host_name,
        decl);
    if (transpiler_hosted_domain_slot_view_missing_mir_metadata(
            &slot_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing domain-slot existence metadata for '%s'",
            host_name != NULL ? host_name : "(anonymous-domain)");
        return false;
    }

    for (size_t i = 0; i < slot_view.count; i++) {
        const char *slot_name =
            transpiler_hosted_domain_slot_view_name(&slot_view, i);
        if (slot_name != NULL && strcmp(slot_name, field_name) == 0)
            return true;
    }

    return false;
}

static bool
roster_slot_view_has_field(TranspilerCtx *ctx,
                           ASTNode *roster,
                           const char *field_name)
{
    const char *roster_name;
    TranspilerHostedRosterSlotView roster_view;

    if (ctx == NULL || roster == NULL || field_name == NULL)
        return false;

    roster_name = transpiler_decl_name_local(roster);
    roster_view = transpiler_hosted_roster_slot_view_from_decl(
        ctx, roster_name, roster);
    if (transpiler_hosted_roster_slot_view_missing_mir_metadata(
            &roster_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing roster-slot existence metadata for '%s'",
            roster_name != NULL ? roster_name : "(anonymous-roster)");
        return false;
    }

    for (size_t i = 0; i < roster_view.count; i++) {
        const char *slot_name =
            transpiler_hosted_roster_slot_view_name(&roster_view, i);
        if (slot_name != NULL && strcmp(slot_name, field_name) == 0)
            return true;
    }

    return false;
}

static bool
class_field_view_has_field(TranspilerCtx *ctx,
                           ASTNode *decl,
                           const char *field_name)
{
    const char *host_name;
    TranspilerHostedFieldView field_view;

    if (ctx == NULL || decl == NULL || decl->type != AST_CLASS_DECL
        || field_name == NULL)
        return false;

    host_name = transpiler_decl_name_local(decl);
    field_view = transpiler_hosted_class_field_view_from_decl(
        ctx, host_name, decl);
    if (transpiler_hosted_field_view_missing_mir_metadata(&field_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing class-field existence metadata for '%s'",
            host_name != NULL ? host_name : "(anonymous-class)");
        return false;
    }

    return transpiler_hosted_field_view_find_index(
        &field_view, field_name, NULL);
}

static bool
host_shared_view_has_field(TranspilerCtx *ctx,
                           ASTNode *decl,
                           const char *field_name)
{
    const char *host_name;
    TranspilerHostedSharedFieldView shared_view;

    if (ctx == NULL || decl == NULL || field_name == NULL)
        return false;

    host_name = transpiler_decl_name_local(decl);
    shared_view = transpiler_hosted_shared_field_view_from_decl(
        ctx, host_name, decl);
    if (transpiler_hosted_shared_field_view_missing_mir_metadata(
            &shared_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing shared-field existence metadata for '%s'",
            host_name != NULL ? host_name : "(anonymous-domain)");
        return false;
    }

    for (size_t i = 0; i < shared_view.count; i++) {
        const char *shared_name =
            transpiler_hosted_shared_field_view_name(&shared_view, i);
        if (shared_name != NULL && strcmp(shared_name, field_name) == 0)
            return true;
    }
    return false;
}

static bool
zone_layer_view_has_field(TranspilerCtx *ctx,
                          ASTNode *decl,
                          const char *field_name)
{
    const char *zone_name;
    TranspilerHostedZoneLayerSlotView layer_view;

    if (ctx == NULL || decl == NULL || field_name == NULL)
        return false;

    zone_name = transpiler_decl_name_local(decl);
    layer_view = transpiler_hosted_zone_layer_slot_view_from_decl(
        ctx, zone_name, decl);
    if (transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(
            &layer_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing zone layer-slot existence metadata for '%s'",
            zone_name != NULL ? zone_name : "(anonymous-zone)");
        return false;
    }

    for (size_t i = 0; i < layer_view.count; i++) {
        const char *layer_name =
            transpiler_hosted_zone_layer_slot_view_name(&layer_view, i);
        if (layer_name != NULL && strcmp(layer_name, field_name) == 0)
            return true;
    }
    return false;
}

bool
current_class_uses_self_cell(TranspilerCtx *ctx)
{
    ASTNode *host_decl = NULL;
    const char *class_name = NULL;

    if (ctx == NULL)
        return false;
    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl != NULL && host_decl->type == AST_CLASS_DECL)
        class_name = transpiler_decl_name_local(host_decl);
    return class_name != NULL
        && is_pointer_self_host_type_name(ctx, class_name);
}

bool
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

    return class_field_view_has_field(ctx, decl, field_name);
}

bool
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

    if (domain_slot_view_has_field(ctx, decl, field_name))
        return true;
    if (zone_layer_view_has_field(ctx, decl, field_name))
        return true;
    if (host_shared_view_has_field(ctx, decl, field_name))
        return true;

    return false;
}

bool
current_party_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;
    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_PARTY_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    if (host_shared_view_has_field(ctx, decl, field_name))
        return true;

    return false;
}

bool
current_roster_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;
    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_ROSTER_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    if (roster_slot_view_has_field(ctx, decl, field_name))
        return true;

    if (host_shared_view_has_field(ctx, decl, field_name))
        return true;

    return false;
}

bool
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

    if (domain_slot_view_has_field(ctx, decl, field_name))
        return true;
    if (host_shared_view_has_field(ctx, decl, field_name))
        return true;

    return false;
}

bool
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

    if (domain_slot_view_has_field(ctx, decl, field_name))
        return true;
    if (host_shared_view_has_field(ctx, decl, field_name))
        return true;

    return false;
}
