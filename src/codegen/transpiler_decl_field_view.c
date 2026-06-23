/*
 * Copyright (c) 2026 Pergyra Language Project
 * Hosted declaration field view lowering.
 */

#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "host_decl_compat.h"
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

static size_t
transpiler_decl_header_shared_field_count(const MIRDeclHeader *header)
{
    return transpiler_decl_header_field_count_by_kind(
        header, MIR_DECL_FIELD_SHARED);
}

static const MIRDeclField *
transpiler_decl_header_shared_field(const MIRDeclHeader *header, size_t index)
{
    return transpiler_decl_header_field_by_kind(
        header, MIR_DECL_FIELD_SHARED, index);
}

TranspilerHostedFieldView
transpiler_hosted_class_field_view_from_decl(const TranspilerCtx *ctx,
                                             const char *host_name,
                                             ASTNode *decl)
{
    TranspilerHostedFieldView view;
    PgyHostClassFieldsCompatView compat =
        pgy_host_class_fields_compat_view_from_decl(decl);
    const MIRDeclHeader *header = NULL;

    view.decl_header = NULL;
    view.ast_compat_count = compat.count;
    view.count = compat.count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata =
        transpiler_active_has_mir(ctx) && compat.count > 0;

    header = transpiler_active_host_decl_header(ctx, host_name);
    if (header != NULL
        && mir_decl_header_ast_type_or(header, AST_PROGRAM) == AST_CLASS_DECL) {
        view.decl_header = header;
        view.count = mir_decl_header_field_count(header);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
transpiler_hosted_field_view_missing_mir_metadata(
    const TranspilerHostedFieldView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
transpiler_hosted_field_view_metadata(const TranspilerHostedFieldView *view,
                                      size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return mir_decl_header_field(view->decl_header, index);
}

const char *
transpiler_hosted_field_view_name(const TranspilerHostedFieldView *view,
                                  size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_field_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    return NULL;
}

ASTNode *
transpiler_hosted_field_view_type(const TranspilerHostedFieldView *view,
                                  size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_field_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type(field);
    if (view->requires_mir_metadata)
        return NULL;
    return NULL;
}

bool
transpiler_hosted_field_view_find_index(
    const TranspilerHostedFieldView *view,
    const char *field_name,
    size_t *index_out)
{
    if (index_out != NULL)
        *index_out = 0;
    if (view == NULL || field_name == NULL)
        return false;

    for (size_t i = 0; i < view->count; i++) {
        const char *name = transpiler_hosted_field_view_name(view, i);
        if (name != NULL && strcmp(name, field_name) == 0) {
            if (index_out != NULL)
                *index_out = i;
            return true;
        }
    }
    return false;
}

bool
transpiler_hosted_field_view_is_subject_like(
    const TranspilerHostedFieldView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_field_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (field != NULL)
        return transpiler_mir_decl_field_is_subject_like(field);
    if (view->requires_mir_metadata)
        return false;
    return false;
}

TranspilerHostedSharedFieldView
transpiler_hosted_shared_field_view_from_decl(const TranspilerCtx *ctx,
                                              const char *host_name,
                                              ASTNode *decl)
{
    TranspilerHostedSharedFieldView view;
    PgyHostSharedFieldsCompatView compat =
        pgy_host_shared_fields_compat_view_from_decl(decl);
    const MIRDeclHeader *header = NULL;

    view.decl_header = NULL;
    view.ast_compat_count = compat.count;
    view.count = compat.count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata =
        transpiler_active_has_mir(ctx) && compat.count > 0;

    header = transpiler_active_host_decl_header(ctx, host_name);
    if (header != NULL
        && transpiler_is_host_decl_type(mir_decl_header_ast_type_or(
            header, AST_PROGRAM))) {
        view.decl_header = header;
        view.count = transpiler_decl_header_shared_field_count(header);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
transpiler_hosted_shared_field_view_missing_mir_metadata(
    const TranspilerHostedSharedFieldView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclField *
transpiler_hosted_shared_field_view_metadata(
    const TranspilerHostedSharedFieldView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return transpiler_decl_header_shared_field(view->decl_header, index);
}

const char *
transpiler_hosted_shared_field_view_name(
    const TranspilerHostedSharedFieldView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_shared_field_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_name(field);
    if (view->requires_mir_metadata)
        return NULL;
    return NULL;
}

ASTNode *
transpiler_hosted_shared_field_view_type(
    const TranspilerHostedSharedFieldView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_shared_field_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_type(field);
    if (view->requires_mir_metadata)
        return NULL;
    return NULL;
}

ASTNode *
transpiler_hosted_shared_field_view_initializer(
    const TranspilerHostedSharedFieldView *view,
    size_t index)
{
    const MIRDeclField *field =
        transpiler_hosted_shared_field_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (field != NULL)
        return mir_decl_field_initializer(field);
    if (view->requires_mir_metadata)
        return NULL;
    return NULL;
}
