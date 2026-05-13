#include "ast_print_internal.h"

#include <stdio.h>

bool
ast_print_domain_node(ASTNode *node, int indent)
{
    if (node == NULL)
        return false;

    if (ast_print_intent_node(node, indent))
        return true;
    if (ast_print_event_node(node, indent))
        return true;

    switch (node->type) {
        case AST_ABILITY_DECL:
            printf("Ability: %s", node->data.ability_decl.name);
            print_generic_params_inline(node->data.ability_decl.generic_params);
            if (node->data.ability_decl.where_clause) {
                printf(" ");
                print_where_clause_inline(node->data.ability_decl.where_clause);
            }
            printf("\n");
            for (size_t i = 0; i < node->data.ability_decl.require_count; i++) {
                ast_print(node->data.ability_decl.require_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.ability_decl.method_count; i++) {
                ast_print(node->data.ability_decl.methods[i], indent + 1);
            }
            break;

        case AST_ROLE_DECL:
            printf("Role: %s", node->data.role_decl.name);
            if (node->data.role_decl.for_type != NULL) {
                printf(" for ");
                ast_print_inline(node->data.role_decl.for_type);
            }
            print_generic_params_inline(node->data.role_decl.generic_params);
            print_where_clause_inline(node->data.role_decl.where_clause);
            printf("\n");
            for (size_t i = 0; i < node->data.role_decl.include_count; i++) {
                ast_print(node->data.role_decl.includes[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
                ast_print(node->data.role_decl.impl_abilities[i], indent + 1);
            }
            if (node->data.role_decl.parallel_block != NULL) {
                ast_print_indent(indent + 1);
                printf("Parallel On:\n");
                ast_print(node->data.role_decl.parallel_block, indent + 2);
            }
            break;

        case AST_INCLUDE_STMT:
            printf("Include role %s", node->data.include_stmt.role_name);
            print_generic_params_inline(node->data.include_stmt.type_args);
            printf("\n");
            break;

        case AST_REQUIRE_FIELD:
            printf("Require: %s", node->data.require_field.name);
            if (node->data.require_field.type != NULL) {
                printf(": ");
                ast_print_inline(node->data.require_field.type);
            }
            printf("\n");
            break;

        case AST_IMPL_ABILITY:
            printf("Impl ability: ");
            ast_print_inline(node->data.impl_ability.ability_ref);
            printf("\n");
            for (size_t i = 0; i < node->data.impl_ability.method_count; i++) {
                ast_print(node->data.impl_ability.methods[i], indent + 1);
            }
            break;

        case AST_OVERRIDE_FUNC:
            printf("Override%s\n",
                   node->data.override_func.calls_super ? " (calls super)" : "");
            ast_print(node->data.override_func.func_decl, indent + 1);
            break;

        case AST_PARTY_DECL:
            printf("Party: %s", node->data.party_decl.name);
            if (node->data.party_decl.extends != NULL) {
                printf(" extends ");
                ast_print_inline(node->data.party_decl.extends);
            }
            print_generic_params_inline(node->data.party_decl.generic_params);
            printf("\n");
            for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
                ast_print(node->data.party_decl.role_slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.party_decl.shared_count; i++) {
                ast_print(node->data.party_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.party_decl.method_count; i++) {
                ast_print(node->data.party_decl.methods[i], indent + 1);
            }
            break;

        case AST_ROLE_SLOT:
            printf("RoleSlot: %s", node->data.role_slot.slot_name);
            if (node->data.role_slot.is_array)
                printf("[]");
            if (node->data.role_slot.ability_count > 0) {
                printf(" requires ");
                for (size_t i = 0; i < node->data.role_slot.ability_count; i++) {
                    if (i > 0)
                        printf(", ");
                    ast_print_inline(node->data.role_slot.required_abilities[i]);
                }
            }
            printf("\n");
            break;

        case AST_PARTY_SHARED:
            printf("Shared: %s", node->data.party_shared.name);
            if (node->data.party_shared.type != NULL) {
                printf(": ");
                ast_print_inline(node->data.party_shared.type);
            }
            if (node->data.party_shared.initializer != NULL) {
                printf(" = ");
                ast_print_inline(node->data.party_shared.initializer);
            }
            printf("\n");
            break;

        case AST_CONTEXT_ACCESS:
            printf("ContextAccess: %s(%s",
                   node->data.context_access.method_name,
                   node->data.context_access.role_slot_name);
            if (node->data.context_access.ability_type != NULL) {
                printf(", ");
                ast_print_inline(node->data.context_access.ability_type);
            }
            printf(")\n");
            break;

        case AST_PARTY_INSTANCE:
            printf("PartyInstance: %s\n", node->data.party_instance.party_type);
            for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
                ast_print_indent(indent + 1);
                printf("%s = ",
                       node->data.party_instance.assignments[i].slot_name);
                ast_print_inline(node->data.party_instance.assignments[i].value);
                printf("\n");
            }
            break;

        case AST_ROSTER_DECL:
            printf("Roster: %s", node->data.roster_decl.name);
            print_generic_params_inline(node->data.roster_decl.generic_params);
            printf("\n");
            for (size_t i = 0; i < node->data.roster_decl.party_count; i++) {
                ast_print(node->data.roster_decl.party_slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.roster_decl.shared_count; i++) {
                ast_print(node->data.roster_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.roster_decl.method_count; i++) {
                ast_print(node->data.roster_decl.methods[i], indent + 1);
            }
            break;

        case AST_SYSTEMIC_SLOT:
            printf("SystemicSlot: %s: %s", node->data.roster_slot.slot_name,
                   node->data.roster_slot.party_type);
            if (node->data.roster_slot.is_array)
                printf("[]");
            printf("\n");
            break;

        case AST_WORLD_DECL:
            printf("World: %s\n", node->data.world_decl.name);
            for (size_t i = 0; i < node->data.world_decl.roster_count; i++) {
                ast_print(node->data.world_decl.rosters[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
                ast_print(node->data.world_decl.zones[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.world_decl.activate_count; i++) {
                ast_print(node->data.world_decl.activations[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.world_decl.deactivate_count; i++) {
                ast_print(node->data.world_decl.deactivations[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.world_decl.maintained_zone_count; i++) {
                ast_print(node->data.world_decl.maintained_zones[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.world_decl.state_count; i++) {
                ast_print(node->data.world_decl.states[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.world_decl.shared_count; i++) {
                ast_print(node->data.world_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.world_decl.method_count; i++) {
                ast_print(node->data.world_decl.methods[i], indent + 1);
            }
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
                printf("WorldState: %s: %s ", node->data.world_state.state_name, label);
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

        case AST_RELATION_DECL:
            printf("Relation: %s", node->data.relation_decl.name);
            if (node->data.relation_decl.between_left_kind != RELATION_ENDPOINT_NAMED
                || node->data.relation_decl.between_right_kind != RELATION_ENDPOINT_NAMED
                || node->data.relation_decl.between_left_type != NULL
                || node->data.relation_decl.between_right_type != NULL) {
                printf(" between ");
                switch (node->data.relation_decl.between_left_kind) {
                    case RELATION_ENDPOINT_SUBJECT: printf("subject"); break;
                    case RELATION_ENDPOINT_OBJECT: printf("object"); break;
                    case RELATION_ENDPOINT_CLASS: printf("class"); break;
                    case RELATION_ENDPOINT_TOBJECT: printf("tobject"); break;
                    case RELATION_ENDPOINT_NAMED:
                        ast_print_inline(node->data.relation_decl.between_left_type);
                        break;
                }
                printf("%s, ", node->data.relation_decl.between_left_many ? "[]" : "");
                switch (node->data.relation_decl.between_right_kind) {
                    case RELATION_ENDPOINT_SUBJECT: printf("subject"); break;
                    case RELATION_ENDPOINT_OBJECT: printf("object"); break;
                    case RELATION_ENDPOINT_CLASS: printf("class"); break;
                    case RELATION_ENDPOINT_TOBJECT: printf("tobject"); break;
                    case RELATION_ENDPOINT_NAMED:
                        ast_print_inline(node->data.relation_decl.between_right_type);
                        break;
                }
                printf("%s", node->data.relation_decl.between_right_many ? "[]" : "");
            }
            printf("\n");
            for (size_t i = 0; i < node->data.relation_decl.slot_count; i++) {
                ast_print(node->data.relation_decl.slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.relation_decl.refresh_count; i++) {
                ast_print(node->data.relation_decl.refreshes[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.relation_decl.shared_count; i++) {
                ast_print(node->data.relation_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.relation_decl.method_count; i++) {
                ast_print(node->data.relation_decl.methods[i], indent + 1);
            }
            break;

        case AST_EFFECT_DECL:
            printf("Effect: %s\n", node->data.effect_decl.name);
            for (size_t i = 0; i < node->data.effect_decl.slot_count; i++) {
                ast_print(node->data.effect_decl.slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.effect_decl.refresh_count; i++) {
                ast_print(node->data.effect_decl.refreshes[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.effect_decl.shared_count; i++) {
                ast_print(node->data.effect_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.effect_decl.method_count; i++) {
                ast_print(node->data.effect_decl.methods[i], indent + 1);
            }
            break;

        case AST_ZONE_DECL:
            printf("Zone: %s\n", node->data.zone_decl.name);
            for (size_t i = 0; i < node->data.zone_decl.slot_count; i++) {
                ast_print(node->data.zone_decl.slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
                ast_print(node->data.zone_decl.layer_slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.apply_count; i++) {
                ast_print(node->data.zone_decl.applies[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.link_count; i++) {
                ast_print(node->data.zone_decl.links[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.detach_count; i++) {
                ast_print(node->data.zone_decl.detaches[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++) {
                ast_print(node->data.zone_decl.unlinks[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.refresh_count; i++) {
                ast_print(node->data.zone_decl.refreshes[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.maintained_effect_count; i++) {
                ast_print(node->data.zone_decl.maintained_effects[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.maintained_relation_count; i++) {
                ast_print(node->data.zone_decl.maintained_relations[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.authority_count; i++) {
                ast_print(node->data.zone_decl.authorities[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
                ast_print(node->data.zone_decl.states[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.shared_count; i++) {
                ast_print(node->data.zone_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.zone_decl.method_count; i++) {
                ast_print(node->data.zone_decl.methods[i], indent + 1);
            }
            break;

        case AST_DOMAIN_SLOT:
            printf("%sSlot: %s",
                   node->data.domain_slot.is_subject ? "Subject"
                   : (node->data.domain_slot.is_vessel ? "Vessel"
                      : (node->data.domain_slot.is_tobject ? "TObject" : "Object")),
                   node->data.domain_slot.slot_name);
            if (node->data.domain_slot.type != NULL) {
                printf(": ");
                ast_print_inline(node->data.domain_slot.type);
            }
            if (node->data.domain_slot.initializer != NULL) {
                printf(" = ");
                ast_print_inline(node->data.domain_slot.initializer);
            }
            printf("\n");
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
