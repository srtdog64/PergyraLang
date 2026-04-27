static void
llvm_domain_decl_parts(ASTNode *stmt,
                       const char **decl_name,
                       ASTNode ***slots,
                       size_t *slot_count,
                       ASTNode ***shared_fields,
                       size_t *shared_count,
                       ASTNode ***methods,
                       size_t *method_count,
                       ASTNode ***refreshes,
                       size_t *refresh_count)
{
    *decl_name = NULL;
    *slots = NULL;
    *slot_count = 0;
    *shared_fields = NULL;
    *shared_count = 0;
    *methods = NULL;
    *method_count = 0;
    *refreshes = NULL;
    *refresh_count = 0;

    if (stmt == NULL)
        return;

    switch (stmt->type) {
    case AST_PARTY_DECL:
        *decl_name = stmt->data.party_decl.name;
        *shared_fields = stmt->data.party_decl.shared_fields;
        *shared_count = stmt->data.party_decl.shared_count;
        *methods = stmt->data.party_decl.methods;
        *method_count = stmt->data.party_decl.method_count;
        break;
    case AST_ROSTER_DECL:
        *decl_name = stmt->data.roster_decl.name;
        *shared_fields = stmt->data.roster_decl.shared_fields;
        *shared_count = stmt->data.roster_decl.shared_count;
        *methods = stmt->data.roster_decl.methods;
        *method_count = stmt->data.roster_decl.method_count;
        break;
    case AST_WORLD_DECL:
        *decl_name = stmt->data.world_decl.name;
        *shared_fields = stmt->data.world_decl.shared_fields;
        *shared_count = stmt->data.world_decl.shared_count;
        *methods = stmt->data.world_decl.methods;
        *method_count = stmt->data.world_decl.method_count;
        break;
    case AST_RELATION_DECL:
        *decl_name = stmt->data.relation_decl.name;
        *slots = stmt->data.relation_decl.slots;
        *slot_count = stmt->data.relation_decl.slot_count;
        *shared_fields = stmt->data.relation_decl.shared_fields;
        *shared_count = stmt->data.relation_decl.shared_count;
        *methods = stmt->data.relation_decl.methods;
        *method_count = stmt->data.relation_decl.method_count;
        *refreshes = stmt->data.relation_decl.refreshes;
        *refresh_count = stmt->data.relation_decl.refresh_count;
        break;
    case AST_EFFECT_DECL:
        *decl_name = stmt->data.effect_decl.name;
        *slots = stmt->data.effect_decl.slots;
        *slot_count = stmt->data.effect_decl.slot_count;
        *shared_fields = stmt->data.effect_decl.shared_fields;
        *shared_count = stmt->data.effect_decl.shared_count;
        *methods = stmt->data.effect_decl.methods;
        *method_count = stmt->data.effect_decl.method_count;
        *refreshes = stmt->data.effect_decl.refreshes;
        *refresh_count = stmt->data.effect_decl.refresh_count;
        break;
    case AST_ZONE_DECL:
        *decl_name = stmt->data.zone_decl.name;
        *slots = stmt->data.zone_decl.slots;
        *slot_count = stmt->data.zone_decl.slot_count;
        *shared_fields = stmt->data.zone_decl.shared_fields;
        *shared_count = stmt->data.zone_decl.shared_count;
        *methods = stmt->data.zone_decl.methods;
        *method_count = stmt->data.zone_decl.method_count;
        *refreshes = stmt->data.zone_decl.refreshes;
        *refresh_count = stmt->data.zone_decl.refresh_count;
        break;
    default:
        break;
    }
}

