static bool
llvm_domain_slot_is_projection_target(ASTNode *slot,
                                      ASTNode **refreshes,
                                      size_t refresh_count);

static size_t
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

