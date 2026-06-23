/*
 * Copyright (c) 2026 Pergyra Language Project
 * C-side hosted world-roster / roster / role slot view lowering.
 * Split from transpiler_decl_slot_view.c (2026-06 owner-size closure).
 */

#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "transpiler_decl_lookup.h"

/* Local copies of static helpers needed by the moved view helpers. */
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

static size_t
transpiler_decl_header_role_slot_count(const MIRDeclHeader *header)
{
    return transpiler_decl_header_field_count_by_kind(
        header, MIR_DECL_FIELD_ROLE_SLOT);
}

static const MIRDeclField *
transpiler_decl_header_role_slot(const MIRDeclHeader *header, size_t index)
{
    return transpiler_decl_header_field_by_kind(
        header, MIR_DECL_FIELD_ROLE_SLOT, index);
}

static size_t
transpiler_decl_header_roster_slot_count(const MIRDeclHeader *header)
{
    return transpiler_decl_header_field_count_by_kind(
        header, MIR_DECL_FIELD_ROSTER_SLOT);
}

static const MIRDeclField *
transpiler_decl_header_roster_slot(const MIRDeclHeader *header, size_t index)
{
    return transpiler_decl_header_field_by_kind(
        header, MIR_DECL_FIELD_ROSTER_SLOT, index);
}

TranspilerHostedWorldRosterSlotView
transpiler_hosted_world_roster_slot_view_from_decl(const TranspilerCtx *ctx,
                                                   const char *host_name,
                                                   ASTNode *decl)
{
    TranspilerHostedWorldRosterSlotView view;
    ASTNode **compat_slots = NULL;
    size_t compat_count = 0;
    const MIRDeclHeader *header = NULL;

    if (decl != NULL && decl->type == AST_WORLD_DECL)
        compat_slots = ast_world_rosters(decl, &compat_count);

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
            header, MIR_DECL_FIELD_WORLD_ROSTER_SLOT);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
transpiler_hosted_world_roster_slot_view_missing_mir_metadata(
    const TranspilerHostedWorldRosterSlotView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
transpiler_hosted_world_roster_slot_view_metadata(
    const TranspilerHostedWorldRosterSlotView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return transpiler_decl_header_field_by_kind(
        view->decl_header, MIR_DECL_FIELD_WORLD_ROSTER_SLOT, index);
}

const char *
transpiler_hosted_world_roster_slot_view_name(
    const TranspilerHostedWorldRosterSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_world_roster_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_slots != NULL
        && view->ast_compat_slots[index] != NULL) {
        return ast_world_roster_slot_name(view->ast_compat_slots[index]);
    }
    return NULL;
}

const char *
transpiler_hosted_world_roster_slot_view_type_name(
    const TranspilerHostedWorldRosterSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_world_roster_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_slots != NULL
        && view->ast_compat_slots[index] != NULL) {
        return ast_world_roster_type_name(view->ast_compat_slots[index]);
    }
    return NULL;
}

TranspilerHostedRosterSlotView
transpiler_hosted_roster_slot_view_from_decl(const TranspilerCtx *ctx,
                                             const char *host_name,
                                             ASTNode *decl)
{
    TranspilerHostedRosterSlotView view;
    size_t compat_count = 0;
    const MIRDeclHeader *header = NULL;

    if (decl != NULL && decl->type == AST_ROSTER_DECL)
        compat_count = ast_roster_party_count(decl);

    view.decl_header = NULL;
    view.ast_compat_count = compat_count;
    view.count = compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata =
        transpiler_active_has_mir(ctx) && compat_count > 0;

    header = transpiler_active_host_decl_header(ctx, host_name);
    if (header != NULL
        && mir_decl_header_ast_type_or(header, AST_PROGRAM)
            == AST_ROSTER_DECL) {
        view.decl_header = header;
        view.count = transpiler_decl_header_roster_slot_count(header);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
transpiler_hosted_roster_slot_view_missing_mir_metadata(
    const TranspilerHostedRosterSlotView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
transpiler_hosted_roster_slot_view_metadata(
    const TranspilerHostedRosterSlotView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return transpiler_decl_header_roster_slot(view->decl_header, index);
}

const char *
transpiler_hosted_roster_slot_view_name(
    const TranspilerHostedRosterSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_roster_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    return NULL;
}

const char *
transpiler_hosted_roster_slot_view_type_name(
    const TranspilerHostedRosterSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_roster_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type_name(field);
    return NULL;
}

TranspilerHostedRoleSlotView
transpiler_hosted_role_slot_view_from_decl(const TranspilerCtx *ctx,
                                           const char *host_name,
                                           ASTNode *decl)
{
    TranspilerHostedRoleSlotView view;
    size_t compat_count = 0;
    const MIRDeclHeader *header = NULL;

    if (decl != NULL && decl->type == AST_PARTY_DECL)
        compat_count = ast_party_role_count(decl);

    view.decl_header = NULL;
    view.ast_compat_count = compat_count;
    view.count = compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata =
        transpiler_active_has_mir(ctx) && compat_count > 0;

    header = transpiler_active_host_decl_header(ctx, host_name);
    if (header != NULL
        && mir_decl_header_ast_type_or(header, AST_PROGRAM)
            == AST_PARTY_DECL) {
        view.decl_header = header;
        view.count = transpiler_decl_header_role_slot_count(header);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
transpiler_hosted_role_slot_view_missing_mir_metadata(
    const TranspilerHostedRoleSlotView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
transpiler_hosted_role_slot_view_metadata(
    const TranspilerHostedRoleSlotView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return transpiler_decl_header_role_slot(view->decl_header, index);
}

const char *
transpiler_hosted_role_slot_view_name(
    const TranspilerHostedRoleSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_role_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    return NULL;
}

bool
transpiler_hosted_role_slot_view_is_dynamic(
    const TranspilerHostedRoleSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_role_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (field != NULL)
        return mir_decl_field_is_dynamic(field);
    return false;
}

size_t
transpiler_hosted_role_slot_view_required_ability_count(
    const TranspilerHostedRoleSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_role_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return 0;
    if (field != NULL)
        return mir_decl_field_required_ability_count(field);
    return 0;
}

const MIRAbilityRef *
transpiler_hosted_role_slot_view_required_ability_ref(
    const TranspilerHostedRoleSlotView *view,
    size_t index,
    size_t ability_index)
{
    const MIRDeclField *field =
        transpiler_hosted_role_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_required_ability_ref(field, ability_index);
    return NULL;
}
