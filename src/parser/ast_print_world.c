#include "ast_print_internal.h"

#include <stdio.h>

bool
ast_print_world_node(ASTNode *node, int indent)
{
    if (node == NULL)
        return false;

    switch (node->type) {
        case AST_WORLD_DECL:
            printf("World: %s\n", node->data.world_decl.name);
            for (size_t i = 0; i < node->data.world_decl.roster_count; i++)
                ast_print(node->data.world_decl.rosters[i], indent + 1);
            for (size_t i = 0; i < node->data.world_decl.zone_count; i++)
                ast_print(node->data.world_decl.zones[i], indent + 1);
            for (size_t i = 0; i < node->data.world_decl.activate_count; i++)
                ast_print(node->data.world_decl.activations[i], indent + 1);
            for (size_t i = 0; i < node->data.world_decl.deactivate_count; i++)
                ast_print(node->data.world_decl.deactivations[i], indent + 1);
            for (size_t i = 0; i < node->data.world_decl.maintained_zone_count; i++)
                ast_print(node->data.world_decl.maintained_zones[i], indent + 1);
            for (size_t i = 0; i < node->data.world_decl.state_count; i++)
                ast_print(node->data.world_decl.states[i], indent + 1);
            for (size_t i = 0; i < node->data.world_decl.shared_count; i++)
                ast_print(node->data.world_decl.shared_fields[i], indent + 1);
            for (size_t i = 0; i < node->data.world_decl.method_count; i++)
                ast_print(node->data.world_decl.methods[i], indent + 1);
            break;

        case AST_WORLD_SYSTEMIC:
            printf("WorldSystemic: %s: %s",
                   ast_world_roster_slot_name(node),
                   ast_world_roster_type_name(node));
            if (ast_world_roster_initializer(node) != NULL) {
                printf(" = ");
                ast_print_inline(ast_world_roster_initializer(node));
            }
            printf("\n");
            break;

        case AST_WORLD_ZONE:
            printf("WorldZone: %s: %s",
                   ast_world_zone_slot_name(node),
                   ast_world_zone_type_name(node));
            if (ast_world_zone_initializer(node) != NULL) {
                printf(" = ");
                ast_print_inline(ast_world_zone_initializer(node));
            }
            printf("\n");
            break;

        case AST_WORLD_ACTIVATE:
            if (node->data.world_activate.state_name != NULL)
                printf("ActivateState: %s\n", node->data.world_activate.state_name);
            else
                printf("ActivateZone: %s\n", node->data.world_activate.zone_slot_name);
            break;

        case AST_WORLD_DEACTIVATE:
            if (node->data.world_deactivate.state_name != NULL)
                printf("DeactivateState: %s\n", node->data.world_deactivate.state_name);
            else
                printf("DeactivateZone: %s\n", node->data.world_deactivate.zone_slot_name);
            break;

        case AST_WORLD_MAINTAIN:
            if (node->data.world_maintain.state_name != NULL)
                printf("MaintainZoneState: %s\n", node->data.world_maintain.state_name);
            else
                printf("MaintainZone: %s\n", node->data.world_maintain.zone_slot_name);
            break;

        case AST_WORLD_STATE:
            if (node->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
                || node->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
                const char *label =
                    node->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
                        ? "all" : "any";
                printf("WorldState: %s: %s ",
                       node->data.world_state.state_name,
                       label);
                for (size_t i = 0; i < node->data.world_state.input_count; i++) {
                    if (i > 0)
                        printf(", ");
                    printf("%s", node->data.world_state.input_names[i]);
                }
            } else {
                printf("WorldState: %s: zone %s",
                       node->data.world_state.state_name,
                       node->data.world_state.zone_slot_name);
            }
            if (node->data.world_state.detail_name != NULL) {
                const char *label = "zone";
                switch (node->data.world_state.source_kind) {
                    case WORLD_STATE_SOURCE_PROJECTION: label = "projection"; break;
                    case WORLD_STATE_SOURCE_LAYER: label = "layer"; break;
                    case WORLD_STATE_SOURCE_STATE: label = "state"; break;
                    case WORLD_STATE_SOURCE_ZONE:
                    default: break;
                }
                printf(" %s %s", label, node->data.world_state.detail_name);
            }
            printf("\n");
            break;

        default:
            return false;
    }

    return true;
}
