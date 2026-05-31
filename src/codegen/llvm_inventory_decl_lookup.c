/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM MIR-backed declaration lookup helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "host_decl_compat.h"
#include "llvm_internal.h"
#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"

ASTNode *
llvm_bind_current_host_decl(LLVMGenCtx *ctx, ASTNode *host_decl)
{
    ASTNode *saved_decl = NULL;

    if (ctx == NULL)
        return NULL;
    saved_decl = ctx->current_host_decl;
    ctx->current_host_decl = host_decl;
    ctx->current_class_name =
        host_decl != NULL ? llvm_decl_node_name(host_decl) : NULL;
    return saved_decl;
}

void
llvm_restore_current_host_decl(LLVMGenCtx *ctx, ASTNode *saved_decl)
{
    if (ctx == NULL)
        return;
    ctx->current_host_decl = saved_decl;
    ctx->current_class_name =
        saved_decl != NULL ? llvm_decl_node_name(saved_decl) : NULL;
}

void
llvm_active_inventory(const LLVMGenCtx *ctx,
                      ASTNodeType decl_type,
                      ASTNode ***nodes_out,
                      size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL)
        mir_active_inventory(ctx->mir, decl_type, &nodes, &count);

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

const char *
llvm_decl_node_name(ASTNode *node)
{
    const char *host_name;

    if (node == NULL)
        return NULL;

    host_name = pgy_host_decl_compat_name(node);
    if (host_name != NULL)
        return host_name;

    switch (node->type) {
    case AST_FUNC_DECL:
        return ast_declaration_name(node);
    case AST_INTENT_DECL:
        return ast_intent_decl_name(node);
    case AST_ABILITY_DECL:
        return ast_ability_name(node);
    case AST_EVENT_DECL:
        return ast_event_name(node);
    case AST_TYPE_ALIAS:
        return ast_type_alias_name(node);
    default:
        return NULL;
    }
}

ASTNode *
llvm_find_decl_in_active_inventory(const LLVMGenCtx *ctx,
                                   ASTNodeType decl_type,
                                   const char *name)
{
    const MIRDeclHeader *decl_header = NULL;
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;

    if (ctx->mir != NULL) {
        decl_header = mir_find_decl_header(ctx->mir, name);
        if (decl_header != NULL)
            return mir_decl_header_ast_type_or(
                       decl_header, AST_PROGRAM) == decl_type
                ? mir_decl_header_source_ast(decl_header)
                : NULL;
    }

    llvm_active_inventory(ctx, decl_type, &nodes, &count);
    for (size_t i = 0; i < count; i++) {
        ASTNode *node = nodes != NULL ? nodes[i] : NULL;
        const char *node_name;
        if (node == NULL || node->type != decl_type)
            continue;
        node_name = llvm_decl_node_name(node);
        if (node_name != NULL && strcmp(node_name, name) == 0)
            return node;
    }

    return NULL;
}

bool
llvm_param_is_implicit_self(const FuncParam *param)
{
    return param != NULL
        && param->type == NULL
        && param->name != NULL
        && strcmp(param->name, "self") == 0;
}

bool
llvm_is_host_decl_type(ASTNodeType decl_type)
{
    return pgy_host_decl_compat_is_type(decl_type);
}

const MIRDeclHeader *
llvm_find_decl_header_in_context(const LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || ctx->mir == NULL || name == NULL)
        return NULL;
    return mir_find_decl_header(ctx->mir, name);
}

const MIRDeclHeader *
llvm_find_host_decl_header_in_context(const LLVMGenCtx *ctx, const char *name)
{
    const MIRDeclHeader *decl_header =
        llvm_find_decl_header_in_context(ctx, name);

    if (decl_header == NULL
        || !llvm_is_host_decl_type(mir_decl_header_ast_type_or(
            decl_header, AST_PROGRAM))) {
        return NULL;
    }
    return decl_header;
}

const MIRDeclField *
llvm_find_decl_field_in_context(const LLVMGenCtx *ctx,
                                const char *host_name,
                                const char *field_name)
{
    const MIRDeclHeader *decl_header;

    if (ctx == NULL || host_name == NULL || field_name == NULL)
        return NULL;
    decl_header = llvm_find_host_decl_header_in_context(ctx, host_name);
    for (size_t i = 0; decl_header != NULL
         && i < mir_decl_header_field_count(decl_header); i++) {
        const MIRDeclField *field = mir_decl_header_field(decl_header, i);
        const char *name = mir_decl_field_name(field);
        if (name != NULL && strcmp(name, field_name) == 0)
            return field;
    }
    return NULL;
}

ASTNode *
llvm_mir_decl_field_type(const MIRDeclField *field)
{
    return mir_decl_field_type(field);
}

const char *
llvm_mir_decl_field_type_name(const MIRDeclField *field)
{
    return mir_decl_field_type_name(field);
}

LLVMHostedFieldView
llvm_hosted_class_field_view_from_decl(const LLVMGenCtx *ctx,
                                       const char *host_name,
                                       ASTNode *decl)
{
    LLVMHostedFieldView view;
    PgyHostClassFieldsCompatView compat =
        pgy_host_class_fields_compat_view_from_decl(decl);
    const MIRDeclHeader *header = NULL;

    view.decl_header = NULL;
    view.ast_compat_fields = compat.fields;
    view.ast_compat_count = compat.count;
    view.count = compat.count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = ctx != NULL && ctx->mir != NULL
        && compat.count > 0;

    header = llvm_find_host_decl_header_in_context(ctx, host_name);
    if (header != NULL
        && mir_decl_header_ast_type_or(header, AST_PROGRAM) == AST_CLASS_DECL) {
        view.decl_header = header;
        view.count = mir_decl_header_field_count(header);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
llvm_hosted_field_view_missing_mir_metadata(
    const LLVMHostedFieldView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
llvm_hosted_field_view_metadata(const LLVMHostedFieldView *view,
                                size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return mir_decl_header_field(view->decl_header, index);
}

const char *
llvm_hosted_field_view_name(const LLVMHostedFieldView *view, size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_field_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_fields != NULL
        && view->ast_compat_fields[index] != NULL) {
        return view->ast_compat_fields[index]->name;
    }
    return NULL;
}

ASTNode *
llvm_hosted_field_view_type(const LLVMHostedFieldView *view, size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_field_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_fields != NULL
        && view->ast_compat_fields[index] != NULL) {
        return view->ast_compat_fields[index]->type;
    }
    return NULL;
}

bool
llvm_hosted_field_view_find_index(const LLVMHostedFieldView *view,
                                  const char *field_name,
                                  size_t *index_out)
{
    if (index_out != NULL)
        *index_out = 0;
    if (view == NULL || field_name == NULL)
        return false;

    for (size_t i = 0; i < view->count; i++) {
        const char *name = llvm_hosted_field_view_name(view, i);
        if (name != NULL && strcmp(name, field_name) == 0) {
            if (index_out != NULL)
                *index_out = i;
            return true;
        }
    }
    return false;
}

bool
llvm_hosted_field_view_is_subject_like(const LLVMHostedFieldView *view,
                                       size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_field_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (field != NULL)
        return mir_decl_field_is_subject_like(field);
    if (view->requires_mir_metadata)
        return false;
    return view->ast_compat_fields != NULL
        && view->ast_compat_fields[index] != NULL
        && view->ast_compat_fields[index]->is_vessel_field;
}

static size_t
llvm_decl_header_field_count_by_kind(const MIRDeclHeader *header,
                                     MIRDeclFieldKind kind)
{
    size_t count = 0;

    for (size_t i = 0; i < mir_decl_header_field_count(header); i++) {
        const MIRDeclField *field = mir_decl_header_field(header, i);
        if (mir_decl_field_kind_or(field, MIR_DECL_FIELD_UNKNOWN) == kind) {
            count++;
        }
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
        if (mir_decl_field_kind_or(field, MIR_DECL_FIELD_UNKNOWN) != kind) {
            continue;
        }
        if (matched_index == index)
            return field;
        matched_index++;
    }
    return NULL;
}

static size_t
llvm_decl_header_shared_field_count(const MIRDeclHeader *header)
{
    return llvm_decl_header_field_count_by_kind(
        header, MIR_DECL_FIELD_SHARED);
}

static const MIRDeclField *
llvm_decl_header_shared_field(const MIRDeclHeader *header, size_t index)
{
    return llvm_decl_header_field_by_kind(
        header, MIR_DECL_FIELD_SHARED, index);
}

LLVMHostedSharedFieldView
llvm_hosted_shared_field_view_from_decl(const LLVMGenCtx *ctx,
                                        const char *host_name,
                                        ASTNode *decl)
{
    LLVMHostedSharedFieldView view;
    PgyHostSharedFieldsCompatView compat =
        pgy_host_shared_fields_compat_view_from_decl(decl);
    const MIRDeclHeader *header = NULL;

    view.decl_header = NULL;
    view.ast_compat_fields = compat.fields;
    view.ast_compat_count = compat.count;
    view.count = compat.count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = ctx != NULL && ctx->mir != NULL
        && compat.count > 0;

    header = llvm_find_host_decl_header_in_context(ctx, host_name);
    if (header != NULL) {
        view.decl_header = header;
        view.count = llvm_decl_header_shared_field_count(header);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
llvm_hosted_shared_field_view_missing_mir_metadata(
    const LLVMHostedSharedFieldView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
llvm_hosted_shared_field_view_metadata(
    const LLVMHostedSharedFieldView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return llvm_decl_header_shared_field(view->decl_header, index);
}

ASTNode *
llvm_hosted_shared_field_view_source_ast(
    const LLVMHostedSharedFieldView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_shared_field_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_source_ast(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_fields != NULL)
        return view->ast_compat_fields[index];
    return NULL;
}

const char *
llvm_hosted_shared_field_view_name(
    const LLVMHostedSharedFieldView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_shared_field_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_fields != NULL
        && view->ast_compat_fields[index] != NULL) {
        return ast_party_shared_name(view->ast_compat_fields[index]);
    }
    return NULL;
}

ASTNode *
llvm_hosted_shared_field_view_type(
    const LLVMHostedSharedFieldView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_shared_field_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_fields != NULL
        && view->ast_compat_fields[index] != NULL) {
        return ast_party_shared_type(view->ast_compat_fields[index]);
    }
    return NULL;
}

LLVMHostedZoneLayerSlotView
llvm_hosted_zone_layer_slot_view_from_decl(const LLVMGenCtx *ctx,
                                           const char *host_name,
                                           ASTNode *decl)
{
    LLVMHostedZoneLayerSlotView view;
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
    view.requires_mir_metadata = ctx != NULL && ctx->mir != NULL
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

ASTNode *
llvm_hosted_zone_layer_slot_view_source_ast(
    const LLVMHostedZoneLayerSlotView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_zone_layer_slot_view_metadata(view, index);

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
    if (view->ast_compat_slots != NULL
        && view->ast_compat_slots[index] != NULL) {
        return ast_zone_layer_slot_name(view->ast_compat_slots[index]);
    }
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
    if (view->ast_compat_slots != NULL
        && view->ast_compat_slots[index] != NULL) {
        return ast_zone_layer_slot_layer_type(view->ast_compat_slots[index]);
    }
    return NULL;
}

ASTNode *
llvm_find_host_decl_in_active_inventory(const LLVMGenCtx *ctx, const char *name)
{
    const MIRDeclHeader *decl_header = NULL;
    const ASTNodeType *host_types = NULL;
    size_t host_type_count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;

    decl_header = llvm_find_host_decl_header_in_context(ctx, name);
    if (decl_header != NULL)
        return mir_decl_header_source_ast(decl_header);
    if (ctx->mir != NULL)
        return NULL;

    host_types = pgy_host_decl_compat_types(&host_type_count);
    for (size_t i = 0; host_types != NULL && i < host_type_count; i++) {
        ASTNode *decl = llvm_find_decl_in_active_inventory(
            ctx, host_types[i], name);
        if (decl != NULL)
            return decl;
    }

    return NULL;
}

ASTNode *
llvm_current_host_decl(const LLVMGenCtx *ctx)
{
    ASTNode *decl = NULL;

    if (ctx == NULL)
        return NULL;

    if (ctx->current_host_decl != NULL)
        return ctx->current_host_decl;

    if (ctx->current_func_decl != NULL
        && ctx->current_func_decl->type == AST_FUNC_DECL
        && ast_func_within_zone(ctx->current_func_decl) != NULL) {
        decl = llvm_find_decl_in_active_inventory(
            ctx, AST_ZONE_DECL, ast_func_within_zone(ctx->current_func_decl));
        if (decl != NULL)
            return decl;
    }

    return NULL;
}

const char *
llvm_current_host_decl_name(const LLVMGenCtx *ctx)
{
    ASTNode *decl = NULL;

    if (ctx == NULL)
        return NULL;

    decl = llvm_current_host_decl(ctx);
    if (decl == NULL) {
        if (ctx->current_func_decl != NULL
            && ctx->current_func_decl->type == AST_FUNC_DECL
            && ast_func_within_zone(ctx->current_func_decl) != NULL) {
            return ast_func_within_zone(ctx->current_func_decl);
        }
        return NULL;
    }

    return llvm_is_host_decl_type(decl->type)
        ? llvm_decl_node_name(decl)
        : NULL;
}

#endif /* PGY_LLVM_ENABLED */
