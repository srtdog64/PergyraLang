/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_projection_value_helpers.h"

#include <stdio.h>
#include <string.h>

#include "host_decl_compat.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "../common/string_compat.h"

ASTNode *
llvm_find_domain_projection_nominal_decl(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, name);
}

static size_t
llvm_domain_projection_field_count(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    if (decl->type == AST_CLASS_DECL) {
        PgyHostClassFieldsCompatView view =
            pgy_host_class_fields_compat_view_from_decl(decl);
        return view.count;
    }
    return 0;
}

static ClassField *
llvm_domain_projection_field_at(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        PgyHostClassFieldsCompatView view =
            pgy_host_class_fields_compat_view_from_decl(decl);
        if (index < view.count && view.fields != NULL)
            return view.fields[index];
        return NULL;
    }
    return NULL;
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
llvm_resolve_domain_projection_source_path_rec(LLVMGenCtx *ctx,
                                               ASTNode *source_decl,
                                               const char *field_name,
                                               unsigned depth,
                                               char **path_out)
{
    size_t field_count;
    int match_count = 0;
    char *resolved_path = NULL;

    if (path_out != NULL)
        *path_out = NULL;
    if (ctx == NULL || source_decl == NULL || field_name == NULL || depth > 8)
        return 0;

    field_count = llvm_domain_projection_field_count(source_decl);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = llvm_domain_projection_field_at(source_decl, i);
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            if (path_out != NULL)
                *path_out = pergyra_strdup(field_name);
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = llvm_domain_projection_field_at(source_decl, i);
        ASTNode *vessel_decl;
        char *nested_path = NULL;
        char *prefixed_path;
        int nested_status;
        const char *field_type_name = field != NULL ? ast_type_name(field->type) : NULL;

        if (field == NULL || !field->is_vessel_field
            || field_type_name == NULL) {
            continue;
        }

        vessel_decl = llvm_find_domain_projection_nominal_decl(
            ctx, field_type_name);
        if (vessel_decl == NULL || vessel_decl->type != AST_CLASS_DECL
            || ast_class_nominal_kind(vessel_decl) != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = llvm_resolve_domain_projection_source_path_rec(
            ctx, vessel_decl, field_name, depth + 1, &nested_path);
        if (nested_status != 1) {
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        prefixed_path = llvm_domain_projection_join_path(
            ctx, field->name, nested_path);
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
llvm_load_domain_projection_path_value(LLVMGenCtx *ctx,
                                       ASTNode *source_decl,
                                       LLVMClassTypeEntry *source_cls,
                                       LLVMValueRef source_ptr,
                                       const char *field_name)
{
    char *path = NULL;
    char *cursor;
    ASTNode *current_decl;
    LLVMClassTypeEntry *current_cls;
    LLVMValueRef current_ptr;

    {
        int path_status = llvm_resolve_domain_projection_source_path_rec(ctx,
            source_decl, field_name, 0, &path);
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

    current_decl = source_decl;
    current_cls = source_cls;
    current_ptr = source_ptr;
    cursor = path;
    while (cursor != NULL && *cursor != '\0') {
        char *dot = strchr(cursor, '.');
        char *segment = cursor;
        int field_index;
        LLVMClassFieldInfo *field_info = NULL;
        LLVMValueRef field_ptr;

        if (dot != NULL)
            *dot = '\0';

        field_index = llvm_class_field_index(current_cls, segment);
        if (field_index < 0)
            return llvm_domain_projection_value_error(ctx,
                "LLVM domain projection path segment is missing from metadata");
        for (int i = 0; i < current_cls->field_count; i++) {
            if (current_cls->fields[i].index == field_index) {
                field_info = &current_cls->fields[i];
                break;
            }
        }
        if (field_info == NULL || field_info->field_type == NULL)
            return llvm_domain_projection_value_error(ctx,
                "LLVM domain projection field metadata is incomplete");

        field_ptr = LLVMBuildStructGEP2(ctx->builder, current_cls->struct_type,
            current_ptr, (unsigned)field_index, llvm_tmp_name(ctx));
        if (dot == NULL) {
            return LLVMBuildLoad2(ctx->builder, field_info->field_type,
                field_ptr, llvm_tmp_name(ctx));
        }

        for (size_t i = 0; i < llvm_domain_projection_field_count(current_decl); i++) {
            ClassField *field = llvm_domain_projection_field_at(current_decl, i);
            const char *field_type_name = field != NULL ? ast_type_name(field->type) : NULL;
            if (field == NULL || field->name == NULL
                || strcmp(field->name, segment) != 0
                || field_type_name == NULL) {
                continue;
            }
            current_decl = llvm_find_domain_projection_nominal_decl(
                ctx, field_type_name);
            current_cls = llvm_lookup_class(ctx, field_type_name);
            current_ptr = field_ptr;
            if (current_decl == NULL || current_cls == NULL)
                return llvm_domain_projection_value_error(ctx,
                    "LLVM domain projection nested vessel metadata is missing");
            break;
        }
        cursor = dot + 1;
    }

    return llvm_domain_projection_value_error(ctx,
        "LLVM domain projection path did not produce a value");
}

LLVMValueRef
llvm_build_domain_projection_value(LLVMGenCtx *ctx,
                                   LLVMClassTypeEntry *target_cls,
                                   LLVMClassTypeEntry *source_cls,
                                   ASTNode *source_decl,
                                   ASTNode *refresh,
                                   LLVMValueRef source_ptr)
{
    LLVMValueRef projected;

    if (ctx == NULL || target_cls == NULL || source_cls == NULL || source_ptr == NULL)
        return llvm_domain_projection_value_error(ctx,
            "LLVM domain projection requires target/source metadata and source storage");

    projected = LLVMConstNull(target_cls->struct_type);
    for (int i = 0; i < target_cls->field_count; i++) {
        LLVMClassFieldInfo *target_field = &target_cls->fields[i];
        const char *source_field_name = target_field->field_name;
        LLVMValueRef field_value;

        if (target_field->field_name == NULL)
            continue;

        if (refresh != NULL && refresh->type == AST_ZONE_REFRESH) {
            for (size_t j = 0; j < ast_zone_refresh_field_map_count(refresh); j++) {
                const char *mapped_target =
                    ast_zone_refresh_mapped_target_field(refresh, j);
                const char *mapped_source =
                    ast_zone_refresh_mapped_source_field(refresh, j);
                if (mapped_target != NULL && mapped_source != NULL
                    && strcmp(mapped_target, target_field->field_name) == 0) {
                    source_field_name = mapped_source;
                    break;
                }
            }
        }

        field_value = llvm_load_domain_projection_path_value(ctx, source_decl,
            source_cls, source_ptr, source_field_name);
        if (field_value == NULL)
            return NULL;
        projected = LLVMBuildInsertValue(ctx->builder, projected, field_value,
            (unsigned)target_field->index, llvm_tmp_name(ctx));
    }

    return projected;
}

#endif
