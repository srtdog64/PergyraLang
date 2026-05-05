/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM domain projection target lookup.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_domain_projection_target_helpers.h"

#include <stdbool.h>
#include <string.h>

bool
llvm_domain_slot_is_projection_target(ASTNode *slot,
                                      ASTNode **refreshes,
                                      size_t refresh_count)
{
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
        || slot->data.domain_slot.slot_name == NULL) {
        return false;
    }

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH
            || refresh->data.zone_refresh.object_slot_name == NULL) {
            continue;
        }
        if (strcmp(slot->data.domain_slot.slot_name,
                   refresh->data.zone_refresh.object_slot_name) == 0) {
            return true;
        }
    }

    return false;
}

#endif /* PGY_LLVM_ENABLED */
