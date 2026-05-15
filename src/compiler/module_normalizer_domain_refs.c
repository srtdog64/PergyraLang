#include "module_normalizer_internal.h"

#include "../parser/ast_api.h"

bool
module_normalizer_normalize_domain_ref_node(ASTNode *node,
                                            ModuleRenameScope *scope,
                                            ModuleShadowNames *shadow)
{
    if (node == NULL)
        return false;

    switch (node->type) {
        case AST_WORLD_DECL:
        {
            size_t count = 0;
            ASTNode **children = ast_world_rosters(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_world_zones(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_world_activations(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_world_deactivations(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_world_maintained_zones(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_world_states(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_world_shared_fields(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_world_methods(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            return true;
        }

        case AST_RELATION_DECL:
        {
            size_t count = 0;
            ASTNode **children = ast_relation_slots(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_relation_shared_fields(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_relation_methods(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            return true;
        }

        case AST_EFFECT_DECL:
        {
            size_t count = 0;
            ASTNode **children = ast_effect_slots(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_effect_shared_fields(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_effect_methods(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            return true;
        }

        case AST_ZONE_DECL:
        {
            size_t count = 0;
            ASTNode **children = ast_zone_slots(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_zone_layer_slots(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_zone_applies(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_zone_links(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_zone_detaches(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_zone_unlinks(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_zone_refreshes(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_zone_maintained_effects(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_zone_maintained_relations(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_zone_maintained_states(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_zone_authorities(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_zone_states(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_zone_shared_fields(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            children = ast_zone_methods(node, &count);
            for (size_t i = 0; i < count; i++)
                module_normalizer_normalize_node_refs(children[i], scope, shadow);
            return true;
        }

        case AST_DOMAIN_SLOT:
            module_normalizer_normalize_node_refs(ast_domain_slot_type(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_domain_slot_initializer(node), scope, shadow);
            return true;

        case AST_WORLD_ZONE:
        case AST_WORLD_ACTIVATE:
        case AST_WORLD_DEACTIVATE:
        case AST_WORLD_MAINTAIN:
        case AST_WORLD_STATE:
        case AST_ZONE_LAYER_SLOT:
        case AST_ZONE_APPLY:
        case AST_ZONE_LINK:
        case AST_ZONE_DETACH:
        case AST_ZONE_UNLINK:
        case AST_ZONE_REFRESH:
        case AST_ZONE_MAINTAIN_EFFECT:
        case AST_ZONE_MAINTAIN_RELATION:
        case AST_ZONE_MAINTAIN_STATE:
        case AST_ZONE_AUTHORITY:
        case AST_ZONE_STATE:
            return true;

        default:
            return false;
    }
}
