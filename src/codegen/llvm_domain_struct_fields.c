/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM domain struct field helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_struct_fields.h"
#include "llvm_domain_projection_target_helpers.h"

#include <stdbool.h>
#include <stdio.h>

static bool
llvm_domain_struct_projection_field_name(char *out,
                                         size_t out_size,
                                         const char *kind,
                                         const char *slot_name)
{
    int written;

    if (out == NULL || out_size == 0 || kind == NULL || slot_name == NULL)
        return false;
    written = snprintf(out, out_size, "__projection_%s_%s", kind, slot_name);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_domain_struct_add_projection_field(LLVMGenCtx *ctx,
                                        LLVMClassTypeEntry *entry,
                                        LLVMTypeRef field_type,
                                        int field_index,
                                        const char *kind,
                                        const char *slot_name)
{
    char field_name[256];

    if (!llvm_domain_struct_projection_field_name(field_name,
            sizeof(field_name), kind, slot_name)) {
        llvm_set_error(ctx,
            "LLVM projection field name is too long for slot '%s'", slot_name);
        return false;
    }
    llvm_class_add_field(entry, pergyra_strdup(field_name), field_type,
        field_index);
    return true;
}

LLVMTypeRef
llvm_domain_required_ast_type(LLVMGenCtx *ctx,
                              ASTNode *field_node,
                              ASTNode *type_node,
                              const char *field_kind)
{
    if (ctx == NULL)
        return NULL;
    if (type_node != NULL) {
        LLVMTypeRef type = ast_type_to_llvm(ctx, type_node);
        if (ctx->has_error || type == NULL)
            return NULL;
        return type;
    }

    llvm_set_error_at_with_hints(ctx, field_node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM domain %s requires explicit type metadata; silent i32 fallback is not allowed",
        field_kind != NULL ? field_kind : "field");
    return NULL;
}

LLVMTypeRef
llvm_domain_required_class_struct_type(LLVMGenCtx *ctx,
                                       ASTNode *field_node,
                                       const char *type_name,
                                       const char *field_kind)
{
    LLVMClassTypeEntry *field_cls;

    if (ctx == NULL)
        return NULL;
    if (type_name == NULL || type_name[0] == '\0') {
        llvm_set_error_at_with_hints(ctx, field_node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM domain %s requires concrete class type metadata; silent i32 fallback is not allowed",
            field_kind != NULL ? field_kind : "field");
        return NULL;
    }

    field_cls = llvm_lookup_class(ctx, type_name);
    if (field_cls != NULL && field_cls->struct_type != NULL)
        return field_cls->struct_type;

    llvm_set_error_at_with_hints(ctx, field_node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM domain %s requires registered class metadata for '%s'; silent i32 fallback is not allowed",
        field_kind != NULL ? field_kind : "field",
        type_name);
    return NULL;
}

LLVMTypeRef
llvm_zone_effect_pool_struct_type(LLVMGenCtx *ctx, LLVMTypeRef effect_ty, int capacity)
{
    LLVMTypeRef fields[4];
    LLVMTypeRef i8_ty;
    unsigned cap;

    if (ctx == NULL || effect_ty == NULL)
        return NULL;

    if (capacity <= 0)
        capacity = 1;
    cap = (unsigned)capacity;
    i8_ty = LLVMInt8TypeInContext(ctx->context);

    fields[0] = LLVMArrayType(effect_ty, cap);
    fields[1] = LLVMArrayType(ctx->type_i1, cap);
    fields[2] = i8_ty;
    fields[3] = i8_ty;
    return LLVMStructTypeInContext(ctx->context, fields, 4, 0);
}

void
llvm_domain_add_projection_state_fields(LLVMGenCtx *ctx,
                                        LLVMClassTypeEntry *entry,
                                        LLVMTypeRef *ftypes,
                                        int *field_index,
                                        ASTNode **slots,
                                        size_t slot_count,
                                        ASTNode **refreshes,
                                        size_t refresh_count)
{
    if (ctx == NULL || entry == NULL || ftypes == NULL || field_index == NULL)
        return;

    for (size_t j = 0; j < slot_count; j++) {
        ASTNode *slot = slots[j];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || (!slot->data.domain_slot.is_tobject
                && !llvm_domain_slot_is_projection_target(slot, refreshes, refresh_count))
            || slot->data.domain_slot.slot_name == NULL) {
            continue;
        }
        if (!llvm_domain_struct_add_projection_field(ctx, entry,
                ftypes[*field_index], *field_index, "ready",
                slot->data.domain_slot.slot_name))
            return;
        (*field_index)++;
        if (!llvm_domain_struct_add_projection_field(ctx, entry,
                ftypes[*field_index], *field_index, "dirty",
                slot->data.domain_slot.slot_name))
            return;
        (*field_index)++;
        if (!llvm_domain_struct_add_projection_field(ctx, entry,
                ftypes[*field_index], *field_index, "epoch",
                slot->data.domain_slot.slot_name))
            return;
        (*field_index)++;
        if (!llvm_domain_struct_add_projection_field(ctx, entry,
                ftypes[*field_index], *field_index, "cause",
                slot->data.domain_slot.slot_name))
            return;
        (*field_index)++;
    }
}

#endif
