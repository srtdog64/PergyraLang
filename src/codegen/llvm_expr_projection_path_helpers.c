/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_projection_path_helpers.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "../common/string_compat.h"

static LLVMValueRef
llvm_projection_error_recovery(LLVMGenCtx *ctx, ASTNode *node,
                               const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s", message != NULL ? message
                : "LLVM projection lowering requires complete metadata");
    }
    return NULL;
}

static char *
llvm_expr_projection_join_path(LLVMGenCtx *ctx,
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

static LLVMHostedFieldView
llvm_projection_field_view(LLVMGenCtx *ctx, ASTNode *decl)
{
    const char *decl_name = llvm_decl_node_name(decl);
    LLVMHostedFieldView view =
        llvm_hosted_class_field_view_from_decl(ctx, decl_name, decl);

    if (llvm_hosted_field_view_missing_mir_metadata(&view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing class-field projection metadata for '%s'",
            decl_name != NULL ? decl_name : "(anonymous-class)");
    }
    return view;
}

static size_t
llvm_projection_field_count(LLVMGenCtx *ctx, ASTNode *decl)
{
    LLVMHostedFieldView view;

    if (decl == NULL)
        return 0;
    view = llvm_projection_field_view(ctx, decl);
    return view.count;
}

static const char *
llvm_projection_field_name(LLVMGenCtx *ctx, ASTNode *decl, size_t index)
{
    LLVMHostedFieldView view;

    if (decl == NULL)
        return NULL;
    view = llvm_projection_field_view(ctx, decl);
    return llvm_hosted_field_view_name(&view, index);
}

static const char *
llvm_projection_field_type_name(LLVMGenCtx *ctx, ASTNode *decl, size_t index)
{
    const MIRDeclField *field;
    LLVMHostedFieldView view;
    ASTNode *type_node = NULL;

    if (decl == NULL)
        return NULL;
    view = llvm_projection_field_view(ctx, decl);
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

static int
llvm_resolve_projection_source_path_rec(LLVMGenCtx *ctx, ASTNode *source_decl,
                                        const char *field_name, unsigned depth,
                                        char **path_out)
{
    size_t field_count;
    int match_count = 0;
    char *resolved_path = NULL;

    if (path_out != NULL)
        *path_out = NULL;
    if (ctx == NULL || source_decl == NULL || field_name == NULL || depth > 8)
        return 0;

    field_count = llvm_projection_field_count(ctx, source_decl);
    for (size_t i = 0; i < field_count; i++) {
        const char *candidate_name =
            llvm_projection_field_name(ctx, source_decl, i);
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
        ASTNode *vessel_decl;
        const char *candidate_name =
            llvm_projection_field_name(ctx, source_decl, i);
        const char *field_type_name =
            llvm_projection_field_type_name(ctx, source_decl, i);
        char *nested_path = NULL;
        char *prefixed_path;
        int nested_status;

        if (candidate_name == NULL || field_type_name == NULL) {
            continue;
        }

        vessel_decl = llvm_find_projection_nominal_decl(ctx,
            field_type_name);
        if (vessel_decl == NULL || vessel_decl->type != AST_CLASS_DECL
            || ast_class_nominal_kind(vessel_decl) != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = llvm_resolve_projection_source_path_rec(
            ctx, vessel_decl, field_name, depth + 1, &nested_path);
        if (nested_status != 1) {
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        prefixed_path = llvm_expr_projection_join_path(
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
llvm_load_projection_path_value(LLVMGenCtx *ctx,
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
    int path_status;

    if (source_cls == NULL || source_ptr == NULL)
        return llvm_projection_error_recovery(ctx, source_decl,
            "LLVM projection path load requires source class metadata and storage");

    path_status = llvm_resolve_projection_source_path_rec(ctx, source_decl,
        field_name, 0, &path);
    if (path_status != 1 || path == NULL) {
        if (path_status == 2) {
            llvm_set_error_at_with_hints(ctx, source_decl,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM projection source field '%s' is ambiguous across vessel paths",
                field_name != NULL ? field_name : "<unnamed>");
            return NULL;
        }
        llvm_set_error_at_with_hints(ctx, source_decl,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM projection source field '%s' is missing from source metadata",
            field_name != NULL ? field_name : "<unnamed>");
        return NULL;
    }

    current_decl = source_decl;
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
            return llvm_projection_error_recovery(ctx, current_decl,
                "LLVM projection path segment is not present in class metadata");
        field_type = llvm_class_field_type_at_index(current_cls, field_index);
        if (field_type == NULL)
            return llvm_projection_error_recovery(ctx, current_decl,
                "LLVM projection path segment requires field type metadata");

        field_ptr = LLVMBuildStructGEP2(ctx->builder, current_cls->struct_type,
            current_ptr, (unsigned)field_index, llvm_tmp_name(ctx));
        if (dot == NULL) {
            return LLVMBuildLoad2(ctx->builder, field_type, field_ptr,
                llvm_tmp_name(ctx));
        }

        for (size_t i = 0;
             i < llvm_projection_field_count(ctx, current_decl);
             i++) {
            const char *candidate_name =
                llvm_projection_field_name(ctx, current_decl, i);
            const char *field_type_name =
                llvm_projection_field_type_name(ctx, current_decl, i);
            if (candidate_name == NULL
                || strcmp(candidate_name, segment) != 0
                || field_type_name == NULL) {
                continue;
            }
            ASTNode *next_decl = llvm_find_projection_nominal_decl(ctx,
                                                                   field_type_name);
            LLVMClassTypeEntry *next_cls = llvm_lookup_class(ctx,
                                                             field_type_name);
            if (next_decl == NULL || next_cls == NULL)
                return llvm_projection_error_recovery(ctx, current_decl,
                    "LLVM projection nested path requires vessel class metadata");
            current_decl = next_decl;
            current_cls = next_cls;
            current_ptr = field_ptr;
            advanced = 1;
            break;
        }
        if (!advanced) {
            return llvm_projection_error_recovery(ctx, current_decl,
                "LLVM projection nested path requires field declaration metadata");
        }
        cursor = dot + 1;
    }

    return llvm_projection_error_recovery(ctx, source_decl,
        "LLVM projection path did not resolve to a loadable value");
}

LLVMValueRef
llvm_emit_subject_projection(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *target_arg;
    ASTNode *source_arg;
    const char *source_class_name;
    LLVMClassTypeEntry *target_cls;
    LLVMClassTypeEntry *source_cls;
    ASTNode *source_decl;
    LLVMVarEntry source_var;
    bool has_source_var;
    LLVMValueRef source_base;
    LLVMValueRef projected;

    if (node == NULL || ast_call_arg_count(node) != 2)
        return llvm_projection_error_recovery(ctx, node,
            "LLVM subject projection requires target and source arguments");

    target_arg = ast_call_argument(node, 0);
    source_arg = ast_call_argument(node, 1);
    const char *target_name = ast_identifier_name(target_arg);
    const char *source_name = ast_identifier_name(source_arg);
    if (target_arg == NULL || target_arg->type != AST_IDENTIFIER
        || target_name == NULL
        || source_arg == NULL || source_arg->type != AST_IDENTIFIER
        || source_name == NULL) {
        return llvm_projection_error_recovery(ctx, node,
            "LLVM subject projection requires identifier target and source arguments");
    }

    target_cls = llvm_lookup_class(ctx, target_name);
    has_source_var = llvm_scope_lookup_snapshot(ctx, source_name, &source_var);
    source_class_name = llvm_lookup_var_class(ctx, source_name);
    source_cls = source_class_name != NULL
        ? llvm_lookup_class(ctx, source_class_name) : NULL;
    source_decl = source_class_name != NULL
        ? llvm_find_projection_nominal_decl(ctx, source_class_name) : NULL;
    if (target_cls == NULL || !has_source_var
        || source_cls == NULL || source_decl == NULL)
        return llvm_projection_error_recovery(ctx, node,
            "LLVM subject projection requires target/source class metadata and source storage");

    source_base = source_var.alloca;
    if (source_var.type == LLVMPointerType(source_cls->struct_type, 0)) {
        source_base = LLVMBuildLoad2(ctx->builder, source_var.type,
            source_var.alloca, llvm_tmp_name(ctx));
    }

    projected = LLVMConstNull(target_cls->struct_type);
    for (int i = 0; i < llvm_class_field_count(target_cls); i++) {
        const char *target_field_name =
            llvm_class_field_name_at(target_cls, i);
        int target_field_index =
            llvm_class_field_struct_index_at(target_cls, i);
        LLVMValueRef field_value;

        if (target_field_name == NULL || target_field_index < 0)
            continue;

        field_value = llvm_load_projection_path_value(ctx, source_decl, source_cls,
            source_base, target_field_name);
        if (ctx->has_error || field_value == NULL)
            return NULL;
        projected = LLVMBuildInsertValue(ctx->builder, projected, field_value,
            (unsigned)target_field_index, llvm_tmp_name(ctx));
    }

    return projected;
}

#endif
