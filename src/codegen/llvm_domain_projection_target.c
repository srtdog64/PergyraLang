/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM domain projection target lookup.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_domain_projection_target_helpers.h"
#include "parser/ast_api.h"

#include <stdbool.h>
#include <string.h>

bool
llvm_domain_slot_is_projection_target(ASTNode *slot,
                                      ASTNode **refreshes,
                                      size_t refresh_count)
{
    const char *slot_name = ast_domain_slot_name(slot);
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
        || slot_name == NULL) {
        return false;
    }

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH
            || ast_zone_refresh_object_slot_name(refresh) == NULL) {
            continue;
        }
        if (strcmp(slot_name, ast_zone_refresh_object_slot_name(refresh)) == 0) {
            return true;
        }
    }

    return false;
}

#endif /* PGY_LLVM_ENABLED */
