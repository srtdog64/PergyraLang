/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM hosted declaration zone/world/role slot view lowering.
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

static ASTNode **
llvm_domain_slot_compat_slots(ASTNode *decl, size_t *count_out)
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
llvm_domain_slot_decl_type(ASTNodeType type)
{
    return type == AST_RELATION_DECL
        || type == AST_EFFECT_DECL
        || type == AST_ZONE_DECL;
}

LLVMHostedZoneLayerSlotView
llvm_hosted_zone_layer_slot_view_from_decl(const LLVMGenCtx *ctx,
                                           const char *host_name,
                                           ASTNode *decl)
{
    LLVMHostedZoneLayerSlotView view;
    size_t compat_count = 0;
    const MIRDeclHeader *header = NULL;

    if (decl != NULL && decl->type == AST_ZONE_DECL)
        (void)ast_zone_layer_slots(decl, &compat_count);

    view.decl_header = NULL;
    view.ast_compat_count = compat_count;
    view.count = compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = llvm_active_has_mir(ctx)
        && compat_count > 0;

    header = llvm_find_host_decl_header_in_context(ctx, host_name);
    if (header != NULL
        && mir_decl_header_ast_type_or(header, AST_PROGRAM)
            == AST_ZONE_DECL) {
        view.decl_header = header;
        view.count = llvm_decl_header_field_count_by_kind(
            header, MIR_DECL_FIELD_ZONE_LAYER_SLOT);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
llvm_hosted_zone_layer_slot_view_missing_mir_metadata(
    const LLVMHostedZoneLayerSlotView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
llvm_hosted_zone_layer_slot_view_metadata(
    const LLVMHostedZoneLayerSlotView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return llvm_decl_header_field_by_kind(
        view->decl_header, MIR_DECL_FIELD_ZONE_LAYER_SLOT, index);
}

const char *
llvm_hosted_zone_layer_slot_view_name(
    const LLVMHostedZoneLayerSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_zone_layer_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    return NULL;
}

const char *
llvm_hosted_zone_layer_slot_view_type_name(
    const LLVMHostedZoneLayerSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_zone_layer_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    return NULL;
}

bool
llvm_hosted_zone_layer_slot_view_is_relation(
    const LLVMHostedZoneLayerSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_zone_layer_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (field != NULL)
        return mir_decl_field_is_relation_layer(field);
    if (view->requires_mir_metadata)
        return false;
    return false;
}

bool
llvm_hosted_zone_layer_slot_view_is_pool(
    const LLVMHostedZoneLayerSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_zone_layer_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (field != NULL)
        return mir_decl_field_is_pool_layer(field);
    if (view->requires_mir_metadata)
        return false;
    return false;
}

int
llvm_hosted_zone_layer_slot_view_pool_capacity(
    const LLVMHostedZoneLayerSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_zone_layer_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return 0;
    if (field != NULL)
        return mir_decl_field_pool_capacity(field);
    if (view->requires_mir_metadata)
        return 0;
    return 0;
}

LLVMHostedDomainSlotView
llvm_hosted_domain_slot_view_from_decl(const LLVMGenCtx *ctx,
                                       const char *host_name,
                                       ASTNode *decl)
{
    LLVMHostedDomainSlotView view;
    size_t compat_count = 0;
    const MIRDeclHeader *header = NULL;

    (void)llvm_domain_slot_compat_slots(decl, &compat_count);

    view.decl_header = NULL;
    view.ast_compat_count = compat_count;
    view.count = compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = llvm_active_has_mir(ctx)
        && compat_count > 0;

    header = llvm_find_host_decl_header_in_context(ctx, host_name);
    if (header != NULL
        && llvm_domain_slot_decl_type(
            mir_decl_header_ast_type_or(header, AST_PROGRAM))) {
        view.decl_header = header;
        view.count = llvm_decl_header_field_count_by_kind(
            header, MIR_DECL_FIELD_DOMAIN_SLOT);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
llvm_hosted_domain_slot_view_missing_mir_metadata(
    const LLVMHostedDomainSlotView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
llvm_hosted_domain_slot_view_metadata(
    const LLVMHostedDomainSlotView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return llvm_decl_header_field_by_kind(
        view->decl_header, MIR_DECL_FIELD_DOMAIN_SLOT, index);
}

const char *
llvm_hosted_domain_slot_view_name(const LLVMHostedDomainSlotView *view,
                                  size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_domain_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    return NULL;
}

ASTNode *
llvm_hosted_domain_slot_view_type(const LLVMHostedDomainSlotView *view,
                                  size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_domain_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type(field);
    return NULL;
}

const char *
llvm_hosted_domain_slot_view_type_name(const LLVMHostedDomainSlotView *view,
                                       size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_domain_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type_name(field);
    return NULL;
}

bool
llvm_hosted_domain_slot_view_is_subject_like(
    const LLVMHostedDomainSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_domain_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (field != NULL)
        return mir_decl_field_is_subject_like(field);
    return false;
}

bool
llvm_hosted_domain_slot_view_is_tobject_like(
    const LLVMHostedDomainSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_domain_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (field != NULL)
        return mir_decl_field_is_tobject_like(field);
    return false;
}

bool
llvm_hosted_domain_slot_view_is_binding_like(
    const LLVMHostedDomainSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_domain_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (field != NULL)
        return mir_decl_field_is_binding_like(field);
    return false;
}

LLVMHostedWorldZoneSlotView
llvm_hosted_world_zone_slot_view_from_decl(const LLVMGenCtx *ctx,
                                           const char *host_name,
                                           ASTNode *decl)
{
    LLVMHostedWorldZoneSlotView view;
    size_t compat_count = 0;
    const MIRDeclHeader *header = NULL;

    if (decl != NULL && decl->type == AST_WORLD_DECL)
        (void)ast_world_zones(decl, &compat_count);

    view.decl_header = NULL;
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
            header, MIR_DECL_FIELD_WORLD_ZONE_SLOT);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
llvm_hosted_world_zone_slot_view_missing_mir_metadata(
    const LLVMHostedWorldZoneSlotView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
llvm_hosted_world_zone_slot_view_metadata(
    const LLVMHostedWorldZoneSlotView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return llvm_decl_header_field_by_kind(
        view->decl_header, MIR_DECL_FIELD_WORLD_ZONE_SLOT, index);
}

const char *
llvm_hosted_world_zone_slot_view_name(
    const LLVMHostedWorldZoneSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_world_zone_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    return NULL;
}

const char *
llvm_hosted_world_zone_slot_view_type_name(
    const LLVMHostedWorldZoneSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_world_zone_slot_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    return NULL;
}


#endif /* PGY_LLVM_ENABLED */
