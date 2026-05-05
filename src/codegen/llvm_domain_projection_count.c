/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM domain projection slot counting.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_domain_projection_count_helpers.h"
#include "llvm_domain_projection_target_helpers.h"

size_t
llvm_count_domain_projection_slots(ASTNode **slots, size_t slot_count,
                                   ASTNode **refreshes, size_t refresh_count)
{
    size_t projection_count = 0;

    if (slots == NULL)
        return 0;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && (slot->data.domain_slot.is_tobject
                || llvm_domain_slot_is_projection_target(slot, refreshes, refresh_count))) {
            projection_count++;
        }
    }

    return projection_count;
}

#endif /* PGY_LLVM_ENABLED */
