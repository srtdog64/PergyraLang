#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_decl_parts_helpers.h"
#include "llvm_inventory_decl_lookup.h"
#include "parser/ast_domain_api.h"

void
llvm_domain_decl_refreshes(ASTNode *stmt,
                           const char **decl_name,
                           ASTNode ***refreshes,
                           size_t *refresh_count)
{
    if (decl_name != NULL)
        *decl_name = NULL;
    if (refreshes != NULL)
        *refreshes = NULL;
    if (refresh_count != NULL)
        *refresh_count = 0;

    if (stmt == NULL)
        return;

    if (decl_name != NULL)
        *decl_name = llvm_decl_node_name(stmt);

    switch (stmt->type) {
    case AST_PARTY_DECL:
        break;
    case AST_ROSTER_DECL:
        break;
    case AST_WORLD_DECL:
        break;
    case AST_RELATION_DECL:
        if (refreshes != NULL)
            *refreshes = ast_relation_refreshes(stmt, refresh_count);
        break;
    case AST_EFFECT_DECL:
        if (refreshes != NULL)
            *refreshes = ast_effect_refreshes(stmt, refresh_count);
        break;
    case AST_ZONE_DECL:
        if (refreshes != NULL)
            *refreshes = ast_zone_refreshes(stmt, refresh_count);
        break;
    default:
        break;
    }
}

#endif
