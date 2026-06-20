/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_projection_value_helpers.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "../common/string_compat.h"
#include "../compiler/mir_decl_headers.h"

static LLVMHostedFieldView
llvm_domain_projection_field_view_by_name(LLVMGenCtx *ctx,
                                          const char *decl_name)
{
    LLVMHostedFieldView view =
        llvm_hosted_class_field_view_from_decl(ctx, decl_name, NULL);

    if (llvm_active_has_mir(ctx) && !view.uses_mir_metadata) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing class-field domain projection metadata for '%s'",
            decl_name != NULL ? decl_name : "(anonymous-class)");
        return view;
    }
    if (llvm_hosted_field_view_missing_mir_metadata(&view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing class-field domain projection metadata for '%s'",
            decl_name != NULL ? decl_name : "(anonymous-class)");
    }
    return view;
}

static bool
llvm_domain_projection_type_is_vessel(LLVMGenCtx *ctx, const char *type_name)
{
    const MIRDeclHeader *header;
    ASTNode *decl;

    if (ctx == NULL || type_name == NULL)
        return false;

    header = llvm_find_decl_header_in_context_of_type(
        ctx, AST_CLASS_DECL, type_name);
    if (header != NULL) {
        return mir_decl_header_nominal_kind_or(
            header, NOMINAL_DECL_CLASS) == NOMINAL_DECL_VESSEL;
    }
    if (llvm_active_has_mir(ctx))
        return false;

    decl = llvm_find_projection_nominal_decl(ctx, type_name);
    return decl != NULL
        && decl->type == AST_CLASS_DECL
        && ast_class_nominal_kind(decl) == NOMINAL_DECL_VESSEL;
}

static size_t
llvm_domain_projection_field_count_by_name(LLVMGenCtx *ctx,
                                           const char *decl_name)
{
    LLVMHostedFieldView view;

    if (decl_name == NULL)
        return 0;
    view = llvm_domain_projection_field_view_by_name(ctx, decl_name);
    return view.count;
}

static const char *
llvm_domain_projection_field_name_by_name(LLVMGenCtx *ctx,
                                          const char *decl_name,
                                          size_t index)
{
    LLVMHostedFieldView view;

    if (decl_name == NULL)
        return NULL;
    view = llvm_domain_projection_field_view_by_name(ctx, decl_name);
    return llvm_hosted_field_view_name(&view, index);
}

static const char *
llvm_domain_projection_field_type_name_by_name(LLVMGenCtx *ctx,
                                               const char *decl_name,
                                               size_t index)
{
    const MIRDeclField *field;
    LLVMHostedFieldView view;
    ASTNode *type_node = NULL;

    if (decl_name == NULL)
        return NULL;
    view = llvm_domain_projection_field_view_by_name(ctx, decl_name);
    field = llvm_hosted_field_view_metadata(&view, index);
    if (field != NULL) {
        const char *type_name = llvm_mir_decl_field_type_name(field);
        if (type_name != NULL)
            return type_name;
        type_node = llvm_mir_decl_field_type(field);
    } else {
        type_node = llvm_hosted_field_view_type(&view, index);
    }

    return type_node != NULL ? ast_type_name(type_node) : NULL;
}

static LLVMValueRef
llvm_domain_projection_value_error(LLVMGenCtx *ctx, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s", message != NULL ? message
                : "LLVM domain projection value lowering failed");
    }
    return NULL;
}

static char *
llvm_domain_projection_join_path(LLVMGenCtx *ctx,
                                 const char *field_name,
                                 const char *nested_path)
{
    size_t field_len;
    size_t nested_len;
    size_t path_len;
    char *path;
    int written;

    if (ctx == NULL || field_name == NULL || nested_path == NULL)
        return NULL;

    field_len = strlen(field_name);
    nested_len = strlen(nested_path);
    if (nested_len > ((size_t)-1) - field_len - 2)
        return NULL;

    path_len = field_len + nested_len + 2;
    path = pgy_arena_alloc(&ctx->scratch, path_len);
    if (path == NULL)
        return NULL;

    written = snprintf(path, path_len, "%s.%s", field_name, nested_path);
    if (written < 0 || (size_t)written >= path_len)
        return NULL;

    return path;
}

static int
llvm_resolve_domain_projection_source_path_by_name(LLVMGenCtx *ctx,
                                                   const char *source_type_name,
                                                   const char *field_name,
                                                   unsigned depth,
                                                   char **path_out)
{
    size_t field_count;
    int match_count = 0;
    char *resolved_path = NULL;

    if (path_out != NULL)
        *path_out = NULL;
    if (ctx == NULL || source_type_name == NULL || field_name == NULL
        || depth > 8)
        return 0;

    field_count = llvm_domain_projection_field_count_by_name(
        ctx, source_type_name);
    for (size_t i = 0; i < field_count; i++) {
        const char *candidate_name =
            llvm_domain_projection_field_name_by_name(ctx, source_type_name, i);
        if (candidate_name != NULL && strcmp(candidate_name, field_name) == 0) {
            if (path_out != NULL) {
                size_t len = strlen(field_name) + 1;
                char *path = pgy_arena_alloc(&ctx->scratch, len);
                if (path == NULL)
                    return 0;
                memcpy(path, field_name, len);
                *path_out = path;
            }
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        const char *candidate_name =
            llvm_domain_projection_field_name_by_name(ctx, source_type_name, i);
        const char *field_type_name =
            llvm_domain_projection_field_type_name_by_name(
                ctx, source_type_name, i);
        char *nested_path = NULL;
        char *prefixed_path;
        int nested_status;

        if (candidate_name == NULL || field_type_name == NULL) {
            continue;
        }

        if (!llvm_domain_projection_type_is_vessel(ctx, field_type_name)) {
            continue;
        }

        nested_status = llvm_resolve_domain_projection_source_path_by_name(
            ctx, field_type_name, field_name, depth + 1, &nested_path);
        if (nested_status != 1) {
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        prefixed_path = llvm_domain_projection_join_path(
            ctx, candidate_name, nested_path);
        if (prefixed_path == NULL)
            continue;

        match_count++;
        if (match_count == 1)
            resolved_path = prefixed_path;
        else
            resolved_path = NULL;
    }

    if (match_count == 1) {
        if (path_out != NULL)
            *path_out = resolved_path;
        return 1;
    }

    return match_count > 1 ? 2 : 0;
}

LLVMValueRef
llvm_load_domain_projection_path_value_by_name(LLVMGenCtx *ctx,
                                               const char *source_type_name,
                                               LLVMClassTypeEntry *source_cls,
                                               LLVMValueRef source_ptr,
                                               const char *field_name)
{
    char *path = NULL;
    char *cursor;
    const char *current_type_name;
    LLVMClassTypeEntry *current_cls;
    LLVMValueRef current_ptr;

    {
        int path_status = llvm_resolve_domain_projection_source_path_by_name(
            ctx, source_type_name, field_name, 0, &path);
        if (path_status != 1 || path == NULL) {
            return llvm_domain_projection_value_error(ctx,
                path_status == 2
                    ? "LLVM domain projection source path is ambiguous"
                    : "LLVM domain projection source field path is missing");
        }
    }
    if (source_cls == NULL || source_ptr == NULL) {
        return llvm_domain_projection_value_error(ctx,
            "LLVM domain projection requires source class metadata and storage");
    }

    current_type_name = source_type_name;
    current_cls = source_cls;
    current_ptr = source_ptr;
    cursor = path;
    while (cursor != NULL && *cursor != '\0') {
        char *dot = strchr(cursor, '.');
        char *segment = cursor;
        int field_index;
        LLVMTypeRef field_type;
        LLVMValueRef field_ptr;
        int advanced = 0;

        if (dot != NULL)
            *dot = '\0';

        field_index = llvm_class_field_index(current_cls, segment);
        if (field_index < 0)
            return llvm_domain_projection_value_error(ctx,
                "LLVM domain projection path segment is missing from metadata");
        field_type = llvm_class_field_type_at_index(current_cls, field_index);
        if (field_type == NULL)
            return llvm_domain_projection_value_error(ctx,
                "LLVM domain projection field metadata is incomplete");

        field_ptr = LLVMBuildStructGEP2(ctx->builder, current_cls->struct_type,
            current_ptr, (unsigned)field_index, llvm_tmp_name(ctx));
        if (dot == NULL) {
            return LLVMBuildLoad2(ctx->builder, field_type, field_ptr,
                llvm_tmp_name(ctx));
        }

        for (size_t i = 0;
             i < llvm_domain_projection_field_count_by_name(
                 ctx, current_type_name);
             i++) {
            const char *candidate_name =
                llvm_domain_projection_field_name_by_name(
                    ctx, current_type_name, i);
            const char *field_type_name =
                llvm_domain_projection_field_type_name_by_name(
                    ctx, current_type_name, i);
            if (candidate_name == NULL
                || strcmp(candidate_name, segment) != 0
                || field_type_name == NULL) {
                continue;
            }
            LLVMClassTypeEntry *next_cls = llvm_lookup_class(ctx,
                                                             field_type_name);
            if (next_cls == NULL)
                return llvm_domain_projection_value_error(ctx,
                    "LLVM domain projection nested vessel metadata is missing");
            current_type_name = field_type_name;
            current_cls = next_cls;
            current_ptr = field_ptr;
            advanced = 1;
            break;
        }
        if (!advanced) {
            return llvm_domain_projection_value_error(ctx,
                "LLVM domain projection nested path metadata is missing");
        }
        cursor = dot + 1;
    }

    return llvm_domain_projection_value_error(ctx,
        "LLVM domain projection path did not produce a value");
}

static const char *
llvm_domain_projection_mir_refresh_mapped_source(
    const MIRDeclZoneRefresh *refresh,
    const char *target_field_name)
{
    if (refresh == NULL || target_field_name == NULL)
        return NULL;

    for (size_t j = 0; j < mir_decl_zone_refresh_field_map_count(refresh); j++) {
        const char *mapped_target =
            mir_decl_zone_refresh_mapped_target_field(refresh, j);
        const char *mapped_source =
            mir_decl_zone_refresh_mapped_source_field(refresh, j);
        if (mapped_target != NULL && mapped_source != NULL
            && strcmp(mapped_target, target_field_name) == 0) {
            return mapped_source;
        }
    }
    return NULL;
}

static LLVMValueRef
llvm_build_domain_projection_value_internal(
    LLVMGenCtx *ctx,
    LLVMClassTypeEntry *target_cls,
    LLVMClassTypeEntry *source_cls,
    const char *source_type_name,
    const MIRDeclZoneRefresh *mir_refresh,
    const LLVMHostedZoneRefreshView *refresh_view,
    size_t refresh_index,
    LLVMValueRef source_ptr)
{
    LLVMValueRef projected;

    if (ctx == NULL || target_cls == NULL || source_cls == NULL
        || source_type_name == NULL || source_ptr == NULL)
        return llvm_domain_projection_value_error(ctx,
            "LLVM domain projection requires target/source metadata and source storage");

    projected = LLVMConstNull(target_cls->struct_type);
    for (int i = 0; i < llvm_class_field_count(target_cls); i++) {
        const char *target_field_name =
            llvm_class_field_name_at(target_cls, i);
        int target_field_index =
            llvm_class_field_struct_index_at(target_cls, i);
        const char *source_field_name = target_field_name;
        LLVMValueRef field_value;

        if (target_field_name == NULL || target_field_index < 0)
            continue;

        source_field_name =
            llvm_domain_projection_mir_refresh_mapped_source(mir_refresh,
                target_field_name);
        if (source_field_name == NULL && refresh_view != NULL) {
            source_field_name =
                llvm_hosted_zone_refresh_view_mapped_source_field(
                    refresh_view, refresh_index, target_field_name);
        }
        if (source_field_name == NULL)
            source_field_name = target_field_name;

        field_value = llvm_load_domain_projection_path_value_by_name(
            ctx, source_type_name, source_cls, source_ptr, source_field_name);
        if (field_value == NULL)
            return NULL;
        projected = LLVMBuildInsertValue(ctx->builder, projected, field_value,
            (unsigned)target_field_index, llvm_tmp_name(ctx));
    }

    return projected;
}

LLVMValueRef
llvm_build_domain_projection_value_from_zone_refresh_metadata(
    LLVMGenCtx *ctx,
    LLVMClassTypeEntry *target_cls,
    LLVMClassTypeEntry *source_cls,
    const char *source_type_name,
    const MIRDeclZoneRefresh *refresh,
    LLVMValueRef source_ptr)
{
    return llvm_build_domain_projection_value_internal(ctx,
        target_cls,
        source_cls,
        source_type_name,
        refresh,
        NULL,
        0,
        source_ptr);
}

LLVMValueRef
llvm_build_domain_projection_value_from_zone_refresh_view(
    LLVMGenCtx *ctx,
    LLVMClassTypeEntry *target_cls,
    LLVMClassTypeEntry *source_cls,
    const char *source_type_name,
    const LLVMHostedZoneRefreshView *refresh_view,
    size_t refresh_index,
    LLVMValueRef source_ptr)
{
    return llvm_build_domain_projection_value_internal(ctx,
        target_cls,
        source_cls,
        source_type_name,
        llvm_hosted_zone_refresh_view_metadata(refresh_view, refresh_index),
        refresh_view,
        refresh_index,
        source_ptr);
}

#endif
