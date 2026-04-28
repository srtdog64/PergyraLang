/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM domain struct field helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_struct_fields.h"
#include "llvm_domain_projection_target_helpers.h"

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
        char field_name[256];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || (!slot->data.domain_slot.is_tobject
                && !llvm_domain_slot_is_projection_target(slot, refreshes, refresh_count))
            || slot->data.domain_slot.slot_name == NULL) {
            continue;
        }
        snprintf(field_name, sizeof(field_name), "__projection_ready_%s",
            slot->data.domain_slot.slot_name);
        llvm_class_add_field(entry, pergyra_strdup(field_name),
            ftypes[*field_index], *field_index);
        (*field_index)++;
        snprintf(field_name, sizeof(field_name), "__projection_dirty_%s",
            slot->data.domain_slot.slot_name);
        llvm_class_add_field(entry, pergyra_strdup(field_name),
            ftypes[*field_index], *field_index);
        (*field_index)++;
        snprintf(field_name, sizeof(field_name), "__projection_epoch_%s",
            slot->data.domain_slot.slot_name);
        llvm_class_add_field(entry, pergyra_strdup(field_name),
            ftypes[*field_index], *field_index);
        (*field_index)++;
        snprintf(field_name, sizeof(field_name), "__projection_cause_%s",
            slot->data.domain_slot.slot_name);
        llvm_class_add_field(entry, pergyra_strdup(field_name),
            ftypes[*field_index], *field_index);
        (*field_index)++;
    }
}

#endif
