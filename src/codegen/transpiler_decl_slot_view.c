/*
 * Copyright (c) 2026 Pergyra Language Project
 * Hosted declaration zone/world/role slot view lowering.
 */

#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "transpiler_decl_lookup.h"

static size_t
transpiler_decl_header_field_count_by_kind(const MIRDeclHeader *header,
                                           MIRDeclFieldKind kind)
{
    size_t count = 0;

    for (size_t i = 0; i < mir_decl_header_field_count(header); i++) {
        const MIRDeclField *field = mir_decl_header_field(header, i);
        if (mir_decl_field_kind_or(field, MIR_DECL_FIELD_UNKNOWN) == kind)
            count++;
    }
    return count;
}

static const MIRDeclField *
transpiler_decl_header_field_by_kind(const MIRDeclHeader *header,
                                     MIRDeclFieldKind kind,
                                     size_t index)
{
    size_t matched_index = 0;

    for (size_t i = 0; i < mir_decl_header_field_count(header); i++) {
        const MIRDeclField *field = mir_decl_header_field(header, i);
        if (mir_decl_field_kind_or(field, MIR_DECL_FIELD_UNKNOWN) != kind)
            continue;
        if (matched_index == index)
            return field;
        matched_index++;
    }
    return NULL;
}
static ASTNode **
transpiler_domain_slot_compat_slots(ASTNode *decl, size_t *count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (decl == NULL)
        return NULL;
    switch (decl->type) {
    case AST_RELATION_DECL:
        return ast_relation_slots(decl, count_out);
    case AST_EFFECT_DECL:
        return ast_effect_slots(decl, count_out);
    case AST_ZONE_DECL:
        return ast_zone_slots(decl, count_out);
    default:
        return NULL;
    }
}

static bool
transpiler_domain_slot_decl_type(ASTNodeType type)
{
    return type == AST_RELATION_DECL
        || type == AST_EFFECT_DECL
        || type == AST_ZONE_DECL;
}

TranspilerHostedZoneLayerSlotView
transpiler_hosted_zone_layer_slot_view_from_decl(const TranspilerCtx *ctx,
                                                 const char *host_name,
                                                 ASTNode *decl)
{
    TranspilerHostedZoneLayerSlotView view;
    ASTNode **compat_slots = NULL;
    size_t compat_count = 0;
    const MIRDeclHeader *header = NULL;

    if (decl != NULL && decl->type == AST_ZONE_DECL)
        compat_slots = ast_zone_layer_slots(decl, &compat_count);

    view.decl_header = NULL;
    view.ast_compat_slots = compat_slots;
    view.ast_compat_count = compat_count;
    view.count = compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata =
        transpiler_active_has_mir(ctx) && compat_count > 0;

    header = transpiler_active_host_decl_header(ctx, host_name);
    if (header != NULL
        && mir_decl_header_ast_type_or(header, AST_PROGRAM)
            == AST_ZONE_DECL) {
        view.decl_header = header;
        view.count = transpiler_decl_header_field_count_by_kind(
            header, MIR_DECL_FIELD_ZONE_LAYER_SLOT);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(
    const TranspilerHostedZoneLayerSlotView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
transpiler_hosted_zone_layer_slot_view_metadata(
    const TranspilerHostedZoneLayerSlotView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return transpiler_decl_header_field_by_kind(
        view->decl_header, MIR_DECL_FIELD_ZONE_LAYER_SLOT, index);
}

const char *
transpiler_hosted_zone_layer_slot_view_name(
    const TranspilerHostedZoneLayerSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_zone_layer_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_slots != NULL
        && view->ast_compat_slots[index] != NULL) {
        return ast_zone_layer_slot_name(view->ast_compat_slots[index]);
    }
    return NULL;
}

ASTNode *
transpiler_hosted_zone_layer_slot_view_type(
    const TranspilerHostedZoneLayerSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_zone_layer_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type(field);
    if (view->requires_mir_metadata)
        return NULL;
    return NULL;
}

const char *
transpiler_hosted_zone_layer_slot_view_type_name(
    const TranspilerHostedZoneLayerSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_zone_layer_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_slots != NULL
        && view->ast_compat_slots[index] != NULL) {
        return ast_zone_layer_slot_layer_type(view->ast_compat_slots[index]);
    }
    return NULL;
}

bool
transpiler_hosted_zone_layer_slot_view_is_relation(
    const TranspilerHostedZoneLayerSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_zone_layer_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (field != NULL)
        return mir_decl_field_is_relation_layer(field);
    if (view->requires_mir_metadata)
        return false;
    return view->ast_compat_slots != NULL
        && ast_zone_layer_slot_is_relation(view->ast_compat_slots[index]);
}

bool
transpiler_hosted_zone_layer_slot_view_is_pool(
    const TranspilerHostedZoneLayerSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_zone_layer_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (field != NULL)
        return mir_decl_field_is_pool_layer(field);
    if (view->requires_mir_metadata)
        return false;
    return view->ast_compat_slots != NULL
        && ast_zone_layer_slot_is_pool(view->ast_compat_slots[index]);
}

int
transpiler_hosted_zone_layer_slot_view_pool_capacity(
    const TranspilerHostedZoneLayerSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_zone_layer_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return 0;
    if (field != NULL)
        return mir_decl_field_pool_capacity(field);
    if (view->requires_mir_metadata)
        return 0;
    if (view->ast_compat_slots != NULL)
        return ast_zone_layer_slot_pool_capacity(view->ast_compat_slots[index]);
    return 0;
}

TranspilerHostedZoneStateView
transpiler_hosted_zone_state_view_from_decl(const TranspilerCtx *ctx,
                                            const char *host_name,
                                            ASTNode *decl)
{
    TranspilerHostedZoneStateView view;
    const MIRDeclHeader *header = NULL;

    view.decl_header = NULL;
    view.count = 0;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata =
        transpiler_active_has_mir(ctx)
        && decl != NULL
        && decl->type == AST_ZONE_DECL;

    header = transpiler_active_host_decl_header(ctx, host_name);
    if (header != NULL
        && mir_decl_header_ast_type_or(header, AST_PROGRAM)
            == AST_ZONE_DECL) {
        view.decl_header = header;
        view.count = mir_decl_header_zone_state_count(header);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
transpiler_hosted_zone_state_view_missing_mir_metadata(
    const TranspilerHostedZoneStateView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && !view->uses_mir_metadata;
}

const MIRDeclZoneState *
transpiler_hosted_zone_state_view_metadata(
    const TranspilerHostedZoneStateView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return mir_decl_header_zone_state(view->decl_header, index);
}

const char *
transpiler_hosted_zone_state_view_name(
    const TranspilerHostedZoneStateView *view,
    size_t index)
{
    const MIRDeclZoneState *state =
        transpiler_hosted_zone_state_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (state != NULL)
        return mir_decl_zone_state_name(state);
    return NULL;
}

const char *
transpiler_hosted_zone_state_view_layer_slot_name(
    const TranspilerHostedZoneStateView *view,
    size_t index)
{
    const MIRDeclZoneState *state =
        transpiler_hosted_zone_state_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (state != NULL)
        return mir_decl_zone_state_layer_slot_name(state);
    return NULL;
}

const char *
transpiler_hosted_zone_state_view_left_or_target_slot_name(
    const TranspilerHostedZoneStateView *view,
    size_t index)
{
    const MIRDeclZoneState *state =
        transpiler_hosted_zone_state_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (state != NULL)
        return mir_decl_zone_state_left_or_target_slot_name(state);
    return NULL;
}

const char *
transpiler_hosted_zone_state_view_right_slot_name(
    const TranspilerHostedZoneStateView *view,
    size_t index)
{
    const MIRDeclZoneState *state =
        transpiler_hosted_zone_state_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (state != NULL)
        return mir_decl_zone_state_right_slot_name(state);
    return NULL;
}

bool
transpiler_hosted_zone_state_view_is_relation(
    const TranspilerHostedZoneStateView *view,
    size_t index)
{
    const MIRDeclZoneState *state =
        transpiler_hosted_zone_state_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (state != NULL)
        return mir_decl_zone_state_is_relation(state);
    return false;
}

bool
transpiler_hosted_zone_state_view_rows_complete(
    const TranspilerHostedZoneStateView *view)
{
    if (view == NULL || !view->uses_mir_metadata)
        return true;
    for (size_t i = 0; i < view->count; i++) {
        const char *state_name =
            transpiler_hosted_zone_state_view_name(view, i);
        const char *state_layer =
            transpiler_hosted_zone_state_view_layer_slot_name(view, i);
        const char *state_target =
            transpiler_hosted_zone_state_view_left_or_target_slot_name(
                view, i);
        const char *state_right =
            transpiler_hosted_zone_state_view_right_slot_name(view, i);

        if (state_name == NULL || state_layer == NULL
            || state_target == NULL
            || (transpiler_hosted_zone_state_view_is_relation(view, i)
                && state_right == NULL)) {
            return false;
        }
    }
    return true;
}

TranspilerHostedDomainSlotView
transpiler_hosted_domain_slot_view_from_decl(const TranspilerCtx *ctx,
                                             const char *host_name,
                                             ASTNode *decl)
{
    TranspilerHostedDomainSlotView view;
    ASTNode **compat_slots = NULL;
    size_t compat_count = 0;
    const MIRDeclHeader *header = NULL;

    compat_slots = transpiler_domain_slot_compat_slots(decl, &compat_count);

    view.decl_header = NULL;
    view.ast_compat_slots = compat_slots;
    view.ast_compat_count = compat_count;
    view.count = compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata =
        transpiler_active_has_mir(ctx) && compat_count > 0;

    header = transpiler_active_host_decl_header(ctx, host_name);
    if (header != NULL
        && transpiler_domain_slot_decl_type(
            mir_decl_header_ast_type_or(header, AST_PROGRAM))) {
        view.decl_header = header;
        view.count = transpiler_decl_header_field_count_by_kind(
            header, MIR_DECL_FIELD_DOMAIN_SLOT);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
transpiler_hosted_domain_slot_view_missing_mir_metadata(
    const TranspilerHostedDomainSlotView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
transpiler_hosted_domain_slot_view_metadata(
    const TranspilerHostedDomainSlotView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return transpiler_decl_header_field_by_kind(
        view->decl_header, MIR_DECL_FIELD_DOMAIN_SLOT, index);
}

const char *
transpiler_hosted_domain_slot_view_name(
    const TranspilerHostedDomainSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_domain_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    return NULL;
}

ASTNode *
transpiler_hosted_domain_slot_view_type(
    const TranspilerHostedDomainSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_domain_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type(field);
    return NULL;
}

const char *
transpiler_hosted_domain_slot_view_type_name(
    const TranspilerHostedDomainSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_domain_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type_name(field);
    return NULL;
}

bool
transpiler_hosted_domain_slot_view_is_subject_like(
    const TranspilerHostedDomainSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_domain_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (field != NULL)
        return mir_decl_field_is_subject_like(field);
    return false;
}

bool
transpiler_hosted_domain_slot_view_is_tobject_like(
    const TranspilerHostedDomainSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_domain_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (field != NULL)
        return mir_decl_field_is_tobject_like(field);
    return false;
}

bool
transpiler_hosted_domain_slot_view_is_binding_like(
    const TranspilerHostedDomainSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_domain_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (field != NULL)
        return mir_decl_field_is_binding_like(field);
    return false;
}

TranspilerHostedWorldZoneSlotView
transpiler_hosted_world_zone_slot_view_from_decl(const TranspilerCtx *ctx,
                                                 const char *host_name,
                                                 ASTNode *decl)
{
    TranspilerHostedWorldZoneSlotView view;
    ASTNode **compat_slots = NULL;
    size_t compat_count = 0;
    const MIRDeclHeader *header = NULL;

    if (decl != NULL && decl->type == AST_WORLD_DECL)
        compat_slots = ast_world_zones(decl, &compat_count);

    view.decl_header = NULL;
    view.ast_compat_slots = compat_slots;
    view.ast_compat_count = compat_count;
    view.count = compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata =
        transpiler_active_has_mir(ctx) && compat_count > 0;

    header = transpiler_active_host_decl_header(ctx, host_name);
    if (header != NULL
        && mir_decl_header_ast_type_or(header, AST_PROGRAM)
            == AST_WORLD_DECL) {
        view.decl_header = header;
        view.count = transpiler_decl_header_field_count_by_kind(
            header, MIR_DECL_FIELD_WORLD_ZONE_SLOT);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
transpiler_hosted_world_zone_slot_view_missing_mir_metadata(
    const TranspilerHostedWorldZoneSlotView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
transpiler_hosted_world_zone_slot_view_metadata(
    const TranspilerHostedWorldZoneSlotView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return transpiler_decl_header_field_by_kind(
        view->decl_header, MIR_DECL_FIELD_WORLD_ZONE_SLOT, index);
}

const char *
transpiler_hosted_world_zone_slot_view_name(
    const TranspilerHostedWorldZoneSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_world_zone_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_slots != NULL
        && view->ast_compat_slots[index] != NULL) {
        return ast_world_zone_slot_name(view->ast_compat_slots[index]);
    }
    return NULL;
}

const char *
transpiler_hosted_world_zone_slot_view_type_name(
    const TranspilerHostedWorldZoneSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_world_zone_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_slots != NULL
        && view->ast_compat_slots[index] != NULL) {
        return ast_world_zone_type_name(view->ast_compat_slots[index]);
    }
    return NULL;
}
