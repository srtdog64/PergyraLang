/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM domain projection slot counting.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_domain_projection_count_helpers.h"
#include "llvm_inventory_decl_lookup.h"

#include <string.h>

static bool
llvm_zone_refresh_view_has_projection_target(
    const LLVMHostedZoneRefreshView *refresh_view,
    const char *slot_name)
{
    if (refresh_view == NULL || slot_name == NULL)
        return false;

    for (size_t i = 0; i < refresh_view->count; i++) {
        const char *target_name =
            llvm_hosted_zone_refresh_view_object_slot_name(refresh_view, i);
        if (target_name != NULL && strcmp(target_name, slot_name) == 0)
            return true;
    }
    return false;
}

bool
llvm_domain_slot_view_is_projection_slot_in_zone_refresh_view(
    const LLVMHostedDomainSlotView *slot_view,
    size_t index,
    const LLVMHostedZoneRefreshView *refresh_view)
{
    const char *slot_name;

    if (slot_view == NULL || index >= slot_view->count)
        return false;

    slot_name = llvm_hosted_domain_slot_view_name(slot_view, index);
    if (slot_name == NULL)
        return false;

    return llvm_hosted_domain_slot_view_is_tobject_like(slot_view, index)
        || llvm_zone_refresh_view_has_projection_target(refresh_view,
            slot_name);
}

size_t
llvm_count_domain_projection_slots_in_zone_refresh_view(
    const LLVMHostedDomainSlotView *slot_view,
    const LLVMHostedZoneRefreshView *refresh_view)
{
    size_t projection_count = 0;

    if (slot_view == NULL)
        return 0;

    for (size_t i = 0; i < slot_view->count; i++) {
        if (llvm_domain_slot_view_is_projection_slot_in_zone_refresh_view(
                slot_view, i, refresh_view)) {
            projection_count++;
        }
    }

    return projection_count;
}

#endif /* PGY_LLVM_ENABLED */
