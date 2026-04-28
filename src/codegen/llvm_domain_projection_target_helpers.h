#ifndef PGY_LLVM_DOMAIN_PROJECTION_TARGET_HELPERS_H
#define PGY_LLVM_DOMAIN_PROJECTION_TARGET_HELPERS_H

static bool
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

#endif
