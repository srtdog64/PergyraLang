#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_decl_parts_helpers.h"
#include "llvm_inventory_decl_lookup.h"
#include "parser/ast_domain_api.h"

void
llvm_domain_decl_parts(ASTNode *stmt,
                       const char **decl_name,
                       ASTNode ***slots,
                       size_t *slot_count,
                       ASTNode ***refreshes,
                       size_t *refresh_count)
{
    *decl_name = NULL;
    *slots = NULL;
    *slot_count = 0;
    *refreshes = NULL;
    *refresh_count = 0;

    if (stmt == NULL)
        return;

    *decl_name = llvm_decl_node_name(stmt);

    switch (stmt->type) {
    case AST_PARTY_DECL:
        break;
    case AST_ROSTER_DECL:
        break;
    case AST_WORLD_DECL:
        break;
    case AST_RELATION_DECL:
        *slots = ast_relation_slots(stmt, slot_count);
        *refreshes = ast_relation_refreshes(stmt, refresh_count);
        break;
    case AST_EFFECT_DECL:
        *slots = ast_effect_slots(stmt, slot_count);
        *refreshes = ast_effect_refreshes(stmt, refresh_count);
        break;
    case AST_ZONE_DECL:
        *slots = ast_zone_slots(stmt, slot_count);
        *refreshes = ast_zone_refreshes(stmt, refresh_count);
        break;
    default:
        break;
    }
}

#endif
