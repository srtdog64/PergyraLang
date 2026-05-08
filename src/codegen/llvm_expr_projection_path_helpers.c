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

ASTNode *
llvm_find_projection_nominal_decl(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, name);
}

static size_t
llvm_projection_field_count(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.field_count;
    return 0;
}

static ClassField *
llvm_projection_field_at(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        if (index < decl->data.class_decl.field_count)
            return decl->data.class_decl.fields[index];
        return NULL;
    }
    return NULL;
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

    field_count = llvm_projection_field_count(source_decl);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = llvm_projection_field_at(source_decl, i);
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            if (path_out != NULL)
                *path_out = pergyra_strdup(field_name);
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = llvm_projection_field_at(source_decl, i);
        ASTNode *vessel_decl;
        char *nested_path = NULL;
        char *prefixed_path;
        size_t prefix_len;
        int nested_status;

        if (field == NULL || !field->is_vessel_field
            || field->type == NULL || field->type->type != AST_TYPE
            || field->type->data.type.name == NULL) {
            continue;
        }

        vessel_decl = llvm_find_projection_nominal_decl(ctx,
            field->type->data.type.name);
        if (vessel_decl == NULL || vessel_decl->type != AST_CLASS_DECL
            || vessel_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = llvm_resolve_projection_source_path_rec(
            ctx, vessel_decl, field_name, depth + 1, &nested_path);
        if (nested_status != 1) {
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        {
            size_t field_len = strlen(field->name);
            size_t nested_len = strlen(nested_path);
            if (nested_len > ((size_t)-1) - field_len - 2)
                continue;
            prefix_len = field_len + nested_len + 2;
        }
        prefixed_path = pgy_arena_alloc(&ctx->scratch, prefix_len);
        if (prefixed_path != NULL)
            snprintf(prefixed_path, prefix_len, "%s.%s", field->name, nested_path);
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
        LLVMClassFieldInfo *field_info = NULL;
        LLVMValueRef field_ptr;

        if (dot != NULL)
            *dot = '\0';

        field_index = llvm_class_field_index(current_cls, segment);
        if (field_index < 0)
            return llvm_projection_error_recovery(ctx, current_decl,
                "LLVM projection path segment is not present in class metadata");
        for (int i = 0; i < current_cls->field_count; i++) {
            if (current_cls->fields[i].index == field_index) {
                field_info = &current_cls->fields[i];
                break;
            }
        }
        if (field_info == NULL || field_info->field_type == NULL)
            return llvm_projection_error_recovery(ctx, current_decl,
                "LLVM projection path segment requires field type metadata");

        field_ptr = LLVMBuildStructGEP2(ctx->builder, current_cls->struct_type,
            current_ptr, (unsigned)field_index, llvm_tmp_name(ctx));
        if (dot == NULL) {
            return LLVMBuildLoad2(ctx->builder, field_info->field_type,
                field_ptr, llvm_tmp_name(ctx));
        }

        for (size_t i = 0; i < llvm_projection_field_count(current_decl); i++) {
            ClassField *field = llvm_projection_field_at(current_decl, i);
            if (field == NULL || field->name == NULL
                || strcmp(field->name, segment) != 0
                || field->type == NULL || field->type->type != AST_TYPE
                || field->type->data.type.name == NULL) {
                continue;
            }
            current_decl = llvm_find_projection_nominal_decl(ctx,
                field->type->data.type.name);
            current_cls = llvm_lookup_class(ctx, field->type->data.type.name);
            if (current_decl == NULL || current_cls == NULL)
                return llvm_projection_error_recovery(ctx, field->type,
                    "LLVM projection nested path requires vessel class metadata");
            current_ptr = field_ptr;
            break;
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
    LLVMVarEntry *source_var;
    LLVMValueRef source_base;
    LLVMValueRef projected;

    if (node == NULL || node->data.call.arg_count != 2)
        return llvm_projection_error_recovery(ctx, node,
            "LLVM subject projection requires target and source arguments");

    target_arg = node->data.call.arguments[0];
    source_arg = node->data.call.arguments[1];
    if (target_arg == NULL || target_arg->type != AST_IDENTIFIER
        || target_arg->data.identifier.name == NULL
        || source_arg == NULL || source_arg->type != AST_IDENTIFIER
        || source_arg->data.identifier.name == NULL) {
        return llvm_projection_error_recovery(ctx, node,
            "LLVM subject projection requires identifier target and source arguments");
    }

    target_cls = llvm_lookup_class(ctx, target_arg->data.identifier.name);
    source_var = llvm_scope_lookup(ctx, source_arg->data.identifier.name);
    source_class_name = llvm_lookup_var_class(ctx, source_arg->data.identifier.name);
    source_cls = source_class_name != NULL
        ? llvm_lookup_class(ctx, source_class_name) : NULL;
    source_decl = source_class_name != NULL
        ? llvm_find_projection_nominal_decl(ctx, source_class_name) : NULL;
    if (target_cls == NULL || source_var == NULL || source_cls == NULL || source_decl == NULL)
        return llvm_projection_error_recovery(ctx, node,
            "LLVM subject projection requires target/source class metadata and source storage");

    source_base = source_var->alloca;
    if (source_var->type == LLVMPointerType(source_cls->struct_type, 0)) {
        source_base = LLVMBuildLoad2(ctx->builder, source_var->type,
            source_var->alloca, llvm_tmp_name(ctx));
    }

    projected = LLVMConstNull(target_cls->struct_type);
    for (int i = 0; i < target_cls->field_count; i++) {
        LLVMClassFieldInfo *target_field = &target_cls->fields[i];
        LLVMValueRef field_value;

        if (target_field->field_name == NULL)
            continue;

        field_value = llvm_load_projection_path_value(ctx, source_decl, source_cls,
            source_base, target_field->field_name);
        if (ctx->has_error || field_value == NULL)
            return NULL;
        projected = LLVMBuildInsertValue(ctx->builder, projected, field_value,
            (unsigned)target_field->index, llvm_tmp_name(ctx));
    }

    return projected;
}

#endif
