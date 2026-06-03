/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM domain projection slot counting.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_domain_projection_count_helpers.h"
#include "llvm_domain_projection_target_helpers.h"
#include "parser/ast_api.h"

bool
llvm_domain_slot_view_is_projection_slot(
    const LLVMHostedDomainSlotView *slot_view,
    size_t index,
    ASTNode **refreshes,
    size_t refresh_count)
{
    const char *slot_name;

    if (slot_view == NULL || index >= slot_view->count)
        return false;

    slot_name = llvm_hosted_domain_slot_view_name(slot_view, index);
    if (slot_name == NULL)
        return false;

    return llvm_hosted_domain_slot_view_is_tobject_like(slot_view, index)
        || llvm_domain_slot_is_projection_target(slot_name, refreshes,
            refresh_count);
}

size_t
llvm_count_domain_projection_slots_in_view(
    const LLVMHostedDomainSlotView *slot_view,
    ASTNode **refreshes,
    size_t refresh_count)
{
    size_t projection_count = 0;

    if (slot_view == NULL)
        return 0;

    for (size_t i = 0; i < slot_view->count; i++) {
        if (llvm_domain_slot_view_is_projection_slot(slot_view, i, refreshes,
                refresh_count)) {
            projection_count++;
        }
    }

    return projection_count;
}

#endif /* PGY_LLVM_ENABLED */
