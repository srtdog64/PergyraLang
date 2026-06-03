/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM hosted world-roster / roster / role slot view lowering.
 * Split from llvm_inventory_slot_view.c (2026-06 owner-size closure).
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"

static size_t
llvm_decl_header_field_count_by_kind(const MIRDeclHeader *header,
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
llvm_decl_header_field_by_kind(const MIRDeclHeader *header,
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
llvm_decl_header_roster_slot_count(const MIRDeclHeader *header)
{
    return llvm_decl_header_field_count_by_kind(
        header, MIR_DECL_FIELD_ROSTER_SLOT);
}

static const MIRDeclField *
llvm_decl_header_roster_slot(const MIRDeclHeader *header, size_t index)
{
    return llvm_decl_header_field_by_kind(
        header, MIR_DECL_FIELD_ROSTER_SLOT, index);
}

LLVMHostedWorldRosterSlotView
llvm_hosted_world_roster_slot_view_from_decl(const LLVMGenCtx *ctx,
                                             const char *host_name,
                                             ASTNode *decl)
{
    LLVMHostedWorldRosterSlotView view;
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
    view.requires_mir_metadata = llvm_active_has_mir(ctx)
        && compat_count > 0;

    header = llvm_find_host_decl_header_in_context(ctx, host_name);
    if (header != NULL
        && mir_decl_header_ast_type_or(header, AST_PROGRAM)
            == AST_WORLD_DECL) {
        view.decl_header = header;
        view.count = llvm_decl_header_field_count_by_kind(
            header, MIR_DECL_FIELD_WORLD_ROSTER_SLOT);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
llvm_hosted_world_roster_slot_view_missing_mir_metadata(
    const LLVMHostedWorldRosterSlotView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
llvm_hosted_world_roster_slot_view_metadata(
    const LLVMHostedWorldRosterSlotView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return llvm_decl_header_field_by_kind(
        view->decl_header, MIR_DECL_FIELD_WORLD_ROSTER_SLOT, index);
}

ASTNode *
llvm_hosted_world_roster_slot_view_source_ast(
    const LLVMHostedWorldRosterSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_world_roster_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_source_ast(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_slots != NULL)
        return view->ast_compat_slots[index];
    return NULL;
}

const char *
llvm_hosted_world_roster_slot_view_name(
    const LLVMHostedWorldRosterSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_world_roster_slot_view_metadata(view, index);

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
llvm_hosted_world_roster_slot_view_type_name(
    const LLVMHostedWorldRosterSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_world_roster_slot_view_metadata(view, index);

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

LLVMHostedRosterSlotView
llvm_hosted_roster_slot_view_from_decl(const LLVMGenCtx *ctx,
                                       const char *host_name,
                                       ASTNode *decl)
{
    LLVMHostedRosterSlotView view;
    const MIRDeclHeader *header = NULL;
    size_t compat_count = 0;

    if (decl != NULL && decl->type == AST_ROSTER_DECL)
        compat_count = ast_roster_party_count(decl);

    view.decl_header = NULL;
    view.ast_compat_decl = decl;
    view.ast_compat_count = compat_count;
    view.count = compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = llvm_active_has_mir(ctx)
        && compat_count > 0;

    header = llvm_find_host_decl_header_in_context(ctx, host_name);
    if (header != NULL
        && mir_decl_header_ast_type_or(header, AST_PROGRAM)
            == AST_ROSTER_DECL) {
        view.decl_header = header;
        view.count = llvm_decl_header_roster_slot_count(header);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
llvm_hosted_roster_slot_view_missing_mir_metadata(
    const LLVMHostedRosterSlotView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
llvm_hosted_roster_slot_view_metadata(
    const LLVMHostedRosterSlotView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return llvm_decl_header_roster_slot(view->decl_header, index);
}

ASTNode *
llvm_hosted_roster_slot_view_source_ast(
    const LLVMHostedRosterSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_roster_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_source_ast(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_decl != NULL
        && view->ast_compat_decl->type == AST_ROSTER_DECL) {
        return ast_roster_party(view->ast_compat_decl, index);
    }
    return NULL;
}

const char *
llvm_hosted_roster_slot_view_name(
    const LLVMHostedRosterSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_roster_slot_view_metadata(view, index);
    ASTNode *slot;

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    slot = llvm_hosted_roster_slot_view_source_ast(view, index);
    return slot != NULL ? ast_roster_slot_name(slot) : NULL;
}

const char *
llvm_hosted_roster_slot_view_type_name(
    const LLVMHostedRosterSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_roster_slot_view_metadata(view, index);
    ASTNode *slot;

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    slot = llvm_hosted_roster_slot_view_source_ast(view, index);
    return slot != NULL ? ast_roster_slot_party_type(slot) : NULL;
}

LLVMHostedRoleSlotView
llvm_hosted_role_slot_view_from_decl(const LLVMGenCtx *ctx,
                                     const char *host_name,
                                     ASTNode *decl)
{
    LLVMHostedRoleSlotView view;
    const MIRDeclHeader *header = NULL;
    size_t compat_count = 0;

    if (decl != NULL && decl->type == AST_PARTY_DECL)
        compat_count = ast_party_role_count(decl);

    view.decl_header = NULL;
    view.ast_compat_decl = decl;
    view.ast_compat_count = compat_count;
    view.count = compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = llvm_active_has_mir(ctx)
        && compat_count > 0;

    header = llvm_find_host_decl_header_in_context(ctx, host_name);
    if (header != NULL
        && mir_decl_header_ast_type_or(header, AST_PROGRAM)
            == AST_PARTY_DECL) {
        view.decl_header = header;
        view.count = llvm_decl_header_field_count_by_kind(
            header, MIR_DECL_FIELD_ROLE_SLOT);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
llvm_hosted_role_slot_view_missing_mir_metadata(
    const LLVMHostedRoleSlotView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
llvm_hosted_role_slot_view_metadata(
    const LLVMHostedRoleSlotView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return llvm_decl_header_field_by_kind(
        view->decl_header, MIR_DECL_FIELD_ROLE_SLOT, index);
}

ASTNode *
llvm_hosted_role_slot_view_source_ast(
    const LLVMHostedRoleSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_role_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_source_ast(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_decl != NULL
        && view->ast_compat_decl->type == AST_PARTY_DECL) {
        return ast_party_role(view->ast_compat_decl, index);
    }
    return NULL;
}

const char *
llvm_hosted_role_slot_view_name(
    const LLVMHostedRoleSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_role_slot_view_metadata(view, index);
    ASTNode *slot;

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    slot = llvm_hosted_role_slot_view_source_ast(view, index);
    return slot != NULL ? ast_role_slot_name(slot) : NULL;
}

bool
llvm_hosted_role_slot_view_is_dynamic(
    const LLVMHostedRoleSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_role_slot_view_metadata(view, index);
    ASTNode *slot;

    if (view == NULL || index >= view->count)
        return false;
    if (field != NULL)
        return mir_decl_field_is_dynamic(field);
    if (view->requires_mir_metadata)
        return false;
    slot = llvm_hosted_role_slot_view_source_ast(view, index);
    return slot != NULL && ast_role_slot_is_dynamic(slot);
}

size_t
llvm_hosted_role_slot_view_required_ability_count(
    const LLVMHostedRoleSlotView *view,
    size_t index)
{
    ASTNode *slot = llvm_hosted_role_slot_view_source_ast(view, index);

    return slot != NULL ? ast_role_slot_required_ability_count(slot) : 0;
}

ASTNode *
llvm_hosted_role_slot_view_required_ability(
    const LLVMHostedRoleSlotView *view,
    size_t index,
    size_t ability_index)
{
    ASTNode *slot = llvm_hosted_role_slot_view_source_ast(view, index);

    return slot != NULL
        ? ast_role_slot_required_ability(slot, ability_index)
        : NULL;
}

#endif /* PGY_LLVM_ENABLED */
