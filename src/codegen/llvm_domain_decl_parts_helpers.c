#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_decl_parts_helpers.h"
#include "parser/ast_domain_api.h"

void
llvm_domain_decl_parts(ASTNode *stmt,
                       const char **decl_name,
                       ASTNode ***slots,
                       size_t *slot_count,
                       ASTNode ***shared_fields,
                       size_t *shared_count,
                       ASTNode ***refreshes,
                       size_t *refresh_count)
{
    *decl_name = NULL;
    *slots = NULL;
    *slot_count = 0;
    *shared_fields = NULL;
    *shared_count = 0;
    *refreshes = NULL;
    *refresh_count = 0;

    if (stmt == NULL)
        return;

    switch (stmt->type) {
    case AST_PARTY_DECL:
        *decl_name = ast_party_name(stmt);
        *shared_fields = ast_party_shared_fields(stmt, shared_count);
        break;
    case AST_ROSTER_DECL:
        *decl_name = ast_roster_name(stmt);
        *shared_fields = ast_roster_shared_fields(stmt, shared_count);
        break;
    case AST_WORLD_DECL:
        *decl_name = ast_world_name(stmt);
        *shared_fields = ast_world_shared_fields(stmt, shared_count);
        break;
    case AST_RELATION_DECL:
        *decl_name = ast_relation_name(stmt);
        *slots = ast_relation_slots(stmt, slot_count);
        *shared_fields = ast_relation_shared_fields(stmt, shared_count);
        *refreshes = ast_relation_refreshes(stmt, refresh_count);
        break;
    case AST_EFFECT_DECL:
        *decl_name = ast_effect_name(stmt);
        *slots = ast_effect_slots(stmt, slot_count);
        *shared_fields = ast_effect_shared_fields(stmt, shared_count);
        *refreshes = ast_effect_refreshes(stmt, refresh_count);
        break;
    case AST_ZONE_DECL:
        *decl_name = ast_zone_name(stmt);
        *slots = ast_zone_slots(stmt, slot_count);
        *shared_fields = ast_zone_shared_fields(stmt, shared_count);
        *refreshes = ast_zone_refreshes(stmt, refresh_count);
        break;
    default:
        break;
    }
}

#endif
