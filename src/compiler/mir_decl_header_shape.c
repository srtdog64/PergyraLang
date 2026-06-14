#include "mir_decl_header_shape.h"

#include "../parser/ast_api.h"

size_t
mir_decl_header_ast_domain_method_count(ASTNode *ast)
{
    size_t method_count = 0;

    if (ast == NULL)
        return 0;
    switch (ast->type) {
    case AST_WORLD_DECL:
        (void) ast_world_methods(ast, &method_count);
        break;
    case AST_RELATION_DECL:
        (void) ast_relation_methods(ast, &method_count);
        break;
    case AST_EFFECT_DECL:
        (void) ast_effect_methods(ast, &method_count);
        break;
    case AST_ZONE_DECL:
        (void) ast_zone_methods(ast, &method_count);
        break;
    default:
        break;
    }
    return method_count;
}

size_t
mir_decl_header_ast_field_count(ASTNode *ast)
{
    size_t count = 0;
    size_t extra = 0;

    if (ast == NULL)
        return 0;
    switch (ast->type) {
    case AST_CLASS_DECL:
        (void) ast_class_fields(ast, &count);
        return count;
    case AST_PARTY_DECL:
        return ast_party_role_count(ast) + ast_party_shared_count(ast);
    case AST_ROSTER_DECL:
        return ast_roster_party_count(ast) + ast_roster_shared_count(ast);
    case AST_WORLD_DECL:
        (void) ast_world_rosters(ast, &count);
        (void) ast_world_zones(ast, &extra);
        count += extra;
        (void) ast_world_shared_fields(ast, &extra);
        return count + extra;
    case AST_RELATION_DECL:
        (void) ast_relation_slots(ast, &count);
        (void) ast_relation_shared_fields(ast, &extra);
        return count + extra;
    case AST_EFFECT_DECL:
        (void) ast_effect_slots(ast, &count);
        (void) ast_effect_shared_fields(ast, &extra);
        return count + extra;
    case AST_ZONE_DECL:
        (void) ast_zone_slots(ast, &count);
        (void) ast_zone_layer_slots(ast, &extra);
        count += extra;
        (void) ast_zone_shared_fields(ast, &extra);
        return count + extra;
    default:
        return 0;
    }
}
