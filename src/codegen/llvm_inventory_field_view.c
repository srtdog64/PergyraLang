/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM hosted declaration field view lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "host_decl_compat.h"
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
    view.requires_mir_metadata = llvm_active_has_mir(ctx)
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
    view.requires_mir_metadata = llvm_active_has_mir(ctx)
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

ASTNode *
llvm_hosted_shared_field_view_initializer(
    const LLVMHostedSharedFieldView *view,
    size_t index)
{
    const MIRDeclField *field =
        llvm_hosted_shared_field_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_initializer(field);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_fields != NULL
        && view->ast_compat_fields[index] != NULL) {
        return ast_party_shared_initializer(view->ast_compat_fields[index]);
    }
    return NULL;
}

#endif /* PGY_LLVM_ENABLED */
