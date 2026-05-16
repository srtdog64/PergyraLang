#include "ast_print_internal.h"

#include <stdio.h>

bool
ast_print_zone_node(ASTNode *node, int indent)
{
    if (node == NULL)
        return false;

    switch (node->type) {
        case AST_ZONE_DECL:
            printf("Zone: %s\n", node->data.zone_decl.name);
            for (size_t i = 0; i < node->data.zone_decl.slot_count; i++)
                ast_print(node->data.zone_decl.slots[i], indent + 1);
            for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++)
                ast_print(node->data.zone_decl.layer_slots[i], indent + 1);
            for (size_t i = 0; i < node->data.zone_decl.apply_count; i++)
                ast_print(node->data.zone_decl.applies[i], indent + 1);
            for (size_t i = 0; i < node->data.zone_decl.link_count; i++)
                ast_print(node->data.zone_decl.links[i], indent + 1);
            for (size_t i = 0; i < node->data.zone_decl.detach_count; i++)
                ast_print(node->data.zone_decl.detaches[i], indent + 1);
            for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++)
                ast_print(node->data.zone_decl.unlinks[i], indent + 1);
            for (size_t i = 0; i < node->data.zone_decl.refresh_count; i++)
                ast_print(node->data.zone_decl.refreshes[i], indent + 1);
            for (size_t i = 0; i < node->data.zone_decl.maintained_effect_count; i++)
                ast_print(node->data.zone_decl.maintained_effects[i], indent + 1);
            for (size_t i = 0; i < node->data.zone_decl.maintained_relation_count; i++)
                ast_print(node->data.zone_decl.maintained_relations[i], indent + 1);
            for (size_t i = 0; i < node->data.zone_decl.authority_count; i++)
                ast_print(node->data.zone_decl.authorities[i], indent + 1);
            for (size_t i = 0; i < node->data.zone_decl.state_count; i++)
                ast_print(node->data.zone_decl.states[i], indent + 1);
            for (size_t i = 0; i < node->data.zone_decl.shared_count; i++)
                ast_print(node->data.zone_decl.shared_fields[i], indent + 1);
            for (size_t i = 0; i < node->data.zone_decl.method_count; i++)
                ast_print(node->data.zone_decl.methods[i], indent + 1);
            break;

        case AST_ZONE_LAYER_SLOT:
            if (node->data.zone_layer_slot.is_pool) {
                printf("%sPool: %s: %s capacity %d\n",
                       node->data.zone_layer_slot.is_relation ? "Relation" : "Effect",
                       node->data.zone_layer_slot.slot_name,
                       node->data.zone_layer_slot.layer_type,
                       node->data.zone_layer_slot.pool_capacity);
            } else {
                printf("%sSlot: %s: %s\n",
                       node->data.zone_layer_slot.is_relation ? "Relation" : "Effect",
                       node->data.zone_layer_slot.slot_name,
                       node->data.zone_layer_slot.layer_type);
            }
            break;

        case AST_ZONE_APPLY:
            if (node->data.zone_apply.state_name != NULL) {
                printf("ApplyState: %s", node->data.zone_apply.state_name);
            } else {
                printf("Apply: %s -> %s",
                       node->data.zone_apply.effect_slot_name,
                       node->data.zone_apply.target_slot_name);
            }
            if (node->data.zone_apply.participant_slot_name != NULL)
                printf(" by %s", node->data.zone_apply.participant_slot_name);
            printf("\n");
            break;

        case AST_ZONE_LINK:
            if (node->data.zone_link.state_name != NULL) {
                printf("LinkState: %s", node->data.zone_link.state_name);
            } else {
                printf("Link: %s between %s, %s",
                       node->data.zone_link.relation_slot_name,
                       node->data.zone_link.left_slot_name,
                       node->data.zone_link.right_slot_name);
            }
            if (node->data.zone_link.participant_slot_name != NULL)
                printf(" by %s", node->data.zone_link.participant_slot_name);
            printf("\n");
            break;

        case AST_ZONE_DETACH:
            if (node->data.zone_detach.state_name != NULL) {
                printf("DetachState: %s", node->data.zone_detach.state_name);
            } else {
                printf("Detach: %s from %s",
                       node->data.zone_detach.effect_slot_name,
                       node->data.zone_detach.target_slot_name);
            }
            if (node->data.zone_detach.participant_slot_name != NULL)
                printf(" by %s", node->data.zone_detach.participant_slot_name);
            printf("\n");
            break;

        case AST_ZONE_UNLINK:
            if (node->data.zone_unlink.state_name != NULL) {
                printf("UnlinkState: %s", node->data.zone_unlink.state_name);
            } else {
                printf("Unlink: %s between %s, %s",
                       node->data.zone_unlink.relation_slot_name,
                       node->data.zone_unlink.left_slot_name,
                       node->data.zone_unlink.right_slot_name);
            }
            if (node->data.zone_unlink.participant_slot_name != NULL)
                printf(" by %s", node->data.zone_unlink.participant_slot_name);
            printf("\n");
            break;

        case AST_ZONE_REFRESH:
            printf("%s: %s from %s",
                   node->data.zone_refresh.derive_target_kind ? "Bind"
                   : (node->data.zone_refresh.requires_dto ? "Publish" : "Refresh"),
                   node->data.zone_refresh.object_slot_name,
                   node->data.zone_refresh.source_slot_name);
            if (node->data.zone_refresh.participant_slot_name != NULL)
                printf(" by %s", node->data.zone_refresh.participant_slot_name);
            printf("\n");
            break;

        case AST_ZONE_MAINTAIN_EFFECT:
            printf("MaintainEffect: %s on %s",
                   node->data.zone_maintain_effect.effect_slot_name,
                   node->data.zone_maintain_effect.target_slot_name);
            if (node->data.zone_maintain_effect.participant_slot_name != NULL)
                printf(" by %s", node->data.zone_maintain_effect.participant_slot_name);
            printf("\n");
            break;

        case AST_ZONE_MAINTAIN_RELATION:
            printf("MaintainRelation: %s between %s, %s",
                   node->data.zone_maintain_relation.relation_slot_name,
                   node->data.zone_maintain_relation.left_slot_name,
                   node->data.zone_maintain_relation.right_slot_name);
            if (node->data.zone_maintain_relation.participant_slot_name != NULL)
                printf(" by %s", node->data.zone_maintain_relation.participant_slot_name);
            printf("\n");
            break;

        case AST_ZONE_MAINTAIN_STATE:
            printf("MaintainState: %s",
                   node->data.zone_maintain_state.state_name);
            if (node->data.zone_maintain_state.participant_slot_name != NULL)
                printf(" by %s", node->data.zone_maintain_state.participant_slot_name);
            printf("\n");
            break;

        case AST_ZONE_AUTHORITY:
            printf("Authority: %s\n",
                   node->data.zone_authority.subject_slot_name);
            if (node->data.zone_authority.ability_count > 0) {
                ast_print_indent(indent + 1);
                printf("Requires:");
                for (size_t i = 0; i < node->data.zone_authority.ability_count; i++) {
                    printf("%s", i == 0 ? " " : ", ");
                    ast_print_inline(node->data.zone_authority.required_abilities[i]);
                }
                printf("\n");
            }
            break;

        case AST_ZONE_STATE:
            printf("State: %s: %s %s %s",
                   node->data.zone_state.state_name,
                   node->data.zone_state.is_relation ? "relation" : "effect",
                   node->data.zone_state.layer_slot_name,
                   node->data.zone_state.is_relation ? "between" : "on");
            printf(" %s",
                   node->data.zone_state.left_or_target_slot_name);
            if (node->data.zone_state.is_relation
                && node->data.zone_state.right_slot_name != NULL) {
                printf(", %s", node->data.zone_state.right_slot_name);
            }
            printf("\n");
            break;

        default:
            return false;
    }

    return true;
}
