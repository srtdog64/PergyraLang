/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST domain-node destruction implementation.
 */

#include "ast_destroy_internal.h"
#include <stdlib.h>

bool
ast_destroy_domain_node(ASTNode* node) {
    if (node == NULL) return false;

    switch (node->type) {
        case AST_ROSTER_DECL:
            free(node->data.roster_decl.name);
            for (size_t i = 0; i < node->data.roster_decl.party_count; i++)
                ast_destroy(node->data.roster_decl.party_slots[i]);
            free(node->data.roster_decl.party_slots);
            for (size_t i = 0; i < node->data.roster_decl.shared_count; i++)
                ast_destroy(node->data.roster_decl.shared_fields[i]);
            free(node->data.roster_decl.shared_fields);
            for (size_t i = 0; i < node->data.roster_decl.method_count; i++)
                ast_destroy(node->data.roster_decl.methods[i]);
            free(node->data.roster_decl.methods);
            ast_destroy_generic_params(node->data.roster_decl.generic_params);
            ast_destroy_structured_comment(node->data.roster_decl.doc_comment);
            break;

        case AST_SYSTEMIC_SLOT:
            free(node->data.roster_slot.slot_name);
            free(node->data.roster_slot.party_type);
            break;

        case AST_WORLD_DECL:
            free(node->data.world_decl.name);
            for (size_t i = 0; i < node->data.world_decl.roster_count; i++)
                ast_destroy(node->data.world_decl.rosters[i]);
            free(node->data.world_decl.rosters);
            for (size_t i = 0; i < node->data.world_decl.zone_count; i++)
                ast_destroy(node->data.world_decl.zones[i]);
            free(node->data.world_decl.zones);
            for (size_t i = 0; i < node->data.world_decl.shared_count; i++)
                ast_destroy(node->data.world_decl.shared_fields[i]);
            free(node->data.world_decl.shared_fields);
            for (size_t i = 0; i < node->data.world_decl.method_count; i++)
                ast_destroy(node->data.world_decl.methods[i]);
            free(node->data.world_decl.methods);
            for (size_t i = 0; i < node->data.world_decl.activate_count; i++)
                ast_destroy(node->data.world_decl.activations[i]);
            free(node->data.world_decl.activations);
            for (size_t i = 0; i < node->data.world_decl.deactivate_count; i++)
                ast_destroy(node->data.world_decl.deactivations[i]);
            free(node->data.world_decl.deactivations);
            for (size_t i = 0; i < node->data.world_decl.maintained_zone_count; i++)
                ast_destroy(node->data.world_decl.maintained_zones[i]);
            free(node->data.world_decl.maintained_zones);
            for (size_t i = 0; i < node->data.world_decl.state_count; i++)
                ast_destroy(node->data.world_decl.states[i]);
            free(node->data.world_decl.states);
            ast_destroy_structured_comment(node->data.world_decl.doc_comment);
            break;

        case AST_WORLD_SYSTEMIC:
            free(node->data.world_roster.slot_name);
            free(node->data.world_roster.roster_type);
            ast_destroy(node->data.world_roster.initializer);
            break;

        case AST_WORLD_ZONE:
            free(node->data.world_zone.slot_name);
            free(node->data.world_zone.zone_type);
            ast_destroy(node->data.world_zone.initializer);
            break;

        case AST_WORLD_ACTIVATE:
            free(node->data.world_activate.zone_slot_name);
            free(node->data.world_activate.state_name);
            break;

        case AST_WORLD_DEACTIVATE:
            free(node->data.world_deactivate.zone_slot_name);
            free(node->data.world_deactivate.state_name);
            break;

        case AST_WORLD_MAINTAIN:
            free(node->data.world_maintain.zone_slot_name);
            free(node->data.world_maintain.state_name);
            break;

        case AST_WORLD_STATE:
            free(node->data.world_state.state_name);
            free(node->data.world_state.zone_slot_name);
            free(node->data.world_state.detail_name);
            if (node->data.world_state.input_names != NULL) {
                for (size_t i = 0; i < node->data.world_state.input_count; i++)
                    free(node->data.world_state.input_names[i]);
                free(node->data.world_state.input_names);
            }
            break;

        case AST_INTENT_DECL:
            free(node->data.intent_decl.name);
            for (size_t i = 0; i < node->data.intent_decl.involve_count; i++)
                ast_destroy(node->data.intent_decl.involves[i]);
            free(node->data.intent_decl.involves);
            for (size_t i = 0; i < node->data.intent_decl.value_count; i++)
                ast_destroy(node->data.intent_decl.values[i]);
            free(node->data.intent_decl.values);
            free(node->data.intent_decl.bindings);
            for (size_t i = 0; i < node->data.intent_decl.step_count; i++)
                ast_destroy(node->data.intent_decl.steps[i]);
            free(node->data.intent_decl.steps);
            ast_destroy(node->data.intent_decl.return_type);
            ast_destroy(node->data.intent_decl.priority_expr);
            ast_destroy(node->data.intent_decl.success_expr);
            ast_destroy(node->data.intent_decl.failure_expr);
            free(node->data.intent_decl.success_terminal.step_name);
            ast_destroy(node->data.intent_decl.success_terminal.expr);
            for (size_t i = 0;
                 i < node->data.intent_decl.failure_terminal_count;
                 i++) {
                free(node->data.intent_decl.failure_terminals[i].step_name);
                ast_destroy(node->data.intent_decl.failure_terminals[i].expr);
            }
            free(node->data.intent_decl.failure_terminals);
            ast_destroy_structured_comment(node->data.intent_decl.doc_comment);
            for (size_t i = 0; i < node->data.intent_decl.default_who_count; i++)
                free(node->data.intent_decl.default_who_names[i]);
            free(node->data.intent_decl.default_who_names);
            ast_destroy(node->data.intent_decl.default_where_type);
            break;

        case AST_INTENT_INVOLVES:
            free(node->data.intent_involves.alias);
            ast_destroy(node->data.intent_involves.subject_type);
            break;
        case AST_INTENT_VALUE:
            free(node->data.intent_value.alias);
            ast_destroy(node->data.intent_value.value_type);
            break;

        case AST_INTENT_STEP:
            free(node->data.intent_step.name);
            free(node->data.intent_step.predecessor_step_name);
            ast_destroy(node->data.intent_step.where_type);
            ast_destroy(node->data.intent_step.using_expr);
            ast_destroy(node->data.intent_step.intent_expr);
            free(node->data.intent_step.transfer_from_alias);
            free(node->data.intent_step.transfer_to_alias);
            for (size_t i = 0; i < node->data.intent_step.who_count; i++)
                free(node->data.intent_step.who_names[i]);
            free(node->data.intent_step.who_names);
            for (size_t i = 0; i < node->data.intent_step.on_expr_count; i++)
                ast_destroy(node->data.intent_step.on_exprs[i]);
            free(node->data.intent_step.on_exprs);
            free(node->data.intent_step.outcome_binding_name);
            free(node->data.intent_step.outcome_binding_type_name);
            free(node->data.intent_step.success_branch.variant_name);
            free(node->data.intent_step.success_branch.payload_name);
            free(node->data.intent_step.success_branch.enum_type_name);
            free(node->data.intent_step.success_branch.payload_type_name);
            free(node->data.intent_step.failure_branch.variant_name);
            free(node->data.intent_step.failure_branch.payload_name);
            free(node->data.intent_step.failure_branch.enum_type_name);
            free(node->data.intent_step.failure_branch.payload_type_name);
            for (size_t i = 0; i < node->data.intent_step.compensate_expr_count; i++)
                ast_destroy(node->data.intent_step.compensate_exprs[i]);
            free(node->data.intent_step.compensate_exprs);
            ast_destroy(node->data.intent_step.pre_expr);
            ast_destroy(node->data.intent_step.guard_expr);
            ast_destroy(node->data.intent_step.post_expr);
            ast_destroy(node->data.intent_step.invariant_expr);
            for (size_t i = 0; i < node->data.intent_step.required_ability_count; i++)
                ast_destroy(node->data.intent_step.required_abilities[i]);
            free(node->data.intent_step.required_abilities);
            free(node->data.intent_step.causes_effect);
            for (size_t i = 0; i < node->data.intent_step.authorized_by_count; i++)
                free(node->data.intent_step.authorized_by[i]);
            free(node->data.intent_step.authorized_by);
            ast_destroy(node->data.intent_step.expect_expr);
            break;

        case AST_RELATION_DECL:
            free(node->data.relation_decl.name);
            for (size_t i = 0; i < node->data.relation_decl.slot_count; i++)
                ast_destroy(node->data.relation_decl.slots[i]);
            free(node->data.relation_decl.slots);
            for (size_t i = 0; i < node->data.relation_decl.refresh_count; i++)
                ast_destroy(node->data.relation_decl.refreshes[i]);
            free(node->data.relation_decl.refreshes);
            for (size_t i = 0; i < node->data.relation_decl.shared_count; i++)
                ast_destroy(node->data.relation_decl.shared_fields[i]);
            free(node->data.relation_decl.shared_fields);
            for (size_t i = 0; i < node->data.relation_decl.method_count; i++)
                ast_destroy(node->data.relation_decl.methods[i]);
            free(node->data.relation_decl.methods);
            ast_destroy(node->data.relation_decl.between_left_type);
            ast_destroy(node->data.relation_decl.between_right_type);
            ast_destroy_structured_comment(node->data.relation_decl.doc_comment);
            break;

        case AST_EFFECT_DECL:
            free(node->data.effect_decl.name);
            for (size_t i = 0; i < node->data.effect_decl.slot_count; i++)
                ast_destroy(node->data.effect_decl.slots[i]);
            free(node->data.effect_decl.slots);
            for (size_t i = 0; i < node->data.effect_decl.refresh_count; i++)
                ast_destroy(node->data.effect_decl.refreshes[i]);
            free(node->data.effect_decl.refreshes);
            for (size_t i = 0; i < node->data.effect_decl.shared_count; i++)
                ast_destroy(node->data.effect_decl.shared_fields[i]);
            free(node->data.effect_decl.shared_fields);
            for (size_t i = 0; i < node->data.effect_decl.method_count; i++)
                ast_destroy(node->data.effect_decl.methods[i]);
            free(node->data.effect_decl.methods);
            ast_destroy_structured_comment(node->data.effect_decl.doc_comment);
            break;

        case AST_ZONE_DECL:
            free(node->data.zone_decl.name);
            for (size_t i = 0; i < node->data.zone_decl.slot_count; i++)
                ast_destroy(node->data.zone_decl.slots[i]);
            free(node->data.zone_decl.slots);
            for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++)
                ast_destroy(node->data.zone_decl.layer_slots[i]);
            free(node->data.zone_decl.layer_slots);
            for (size_t i = 0; i < node->data.zone_decl.apply_count; i++)
                ast_destroy(node->data.zone_decl.applies[i]);
            free(node->data.zone_decl.applies);
            for (size_t i = 0; i < node->data.zone_decl.link_count; i++)
                ast_destroy(node->data.zone_decl.links[i]);
            free(node->data.zone_decl.links);
            for (size_t i = 0; i < node->data.zone_decl.detach_count; i++)
                ast_destroy(node->data.zone_decl.detaches[i]);
            free(node->data.zone_decl.detaches);
            for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++)
                ast_destroy(node->data.zone_decl.unlinks[i]);
            free(node->data.zone_decl.unlinks);
            for (size_t i = 0; i < node->data.zone_decl.refresh_count; i++)
                ast_destroy(node->data.zone_decl.refreshes[i]);
            free(node->data.zone_decl.refreshes);
            for (size_t i = 0; i < node->data.zone_decl.maintained_effect_count; i++)
                ast_destroy(node->data.zone_decl.maintained_effects[i]);
            free(node->data.zone_decl.maintained_effects);
            for (size_t i = 0; i < node->data.zone_decl.maintained_relation_count; i++)
                ast_destroy(node->data.zone_decl.maintained_relations[i]);
            free(node->data.zone_decl.maintained_relations);
            for (size_t i = 0; i < node->data.zone_decl.maintained_state_count; i++)
                ast_destroy(node->data.zone_decl.maintained_states[i]);
            free(node->data.zone_decl.maintained_states);
            for (size_t i = 0; i < node->data.zone_decl.authority_count; i++)
                ast_destroy(node->data.zone_decl.authorities[i]);
            free(node->data.zone_decl.authorities);
            for (size_t i = 0; i < node->data.zone_decl.state_count; i++)
                ast_destroy(node->data.zone_decl.states[i]);
            free(node->data.zone_decl.states);
            for (size_t i = 0; i < node->data.zone_decl.shared_count; i++)
                ast_destroy(node->data.zone_decl.shared_fields[i]);
            free(node->data.zone_decl.shared_fields);
            for (size_t i = 0; i < node->data.zone_decl.method_count; i++)
                ast_destroy(node->data.zone_decl.methods[i]);
            free(node->data.zone_decl.methods);
            ast_destroy_structured_comment(node->data.zone_decl.doc_comment);
            break;

        case AST_DOMAIN_SLOT:
            free(node->data.domain_slot.slot_name);
            ast_destroy(node->data.domain_slot.type);
            ast_destroy(node->data.domain_slot.initializer);
            break;

        case AST_ZONE_LAYER_SLOT:
            free(node->data.zone_layer_slot.slot_name);
            free(node->data.zone_layer_slot.layer_type);
            break;

        case AST_ZONE_APPLY:
            free(node->data.zone_apply.effect_slot_name);
            free(node->data.zone_apply.target_slot_name);
            free(node->data.zone_apply.state_name);
            free(node->data.zone_apply.participant_slot_name);
            break;

        case AST_ZONE_LINK:
            free(node->data.zone_link.relation_slot_name);
            free(node->data.zone_link.left_slot_name);
            free(node->data.zone_link.right_slot_name);
            free(node->data.zone_link.state_name);
            free(node->data.zone_link.participant_slot_name);
            break;

        case AST_ZONE_DETACH:
            free(node->data.zone_detach.effect_slot_name);
            free(node->data.zone_detach.target_slot_name);
            free(node->data.zone_detach.state_name);
            free(node->data.zone_detach.participant_slot_name);
            break;

        case AST_ZONE_UNLINK:
            free(node->data.zone_unlink.relation_slot_name);
            free(node->data.zone_unlink.left_slot_name);
            free(node->data.zone_unlink.right_slot_name);
            free(node->data.zone_unlink.state_name);
            free(node->data.zone_unlink.participant_slot_name);
            break;

        case AST_ZONE_REFRESH:
            free(node->data.zone_refresh.object_slot_name);
            free(node->data.zone_refresh.source_slot_name);
            free(node->data.zone_refresh.participant_slot_name);
            for (size_t i = 0; i < node->data.zone_refresh.field_map_count; i++) {
                free(node->data.zone_refresh.mapped_target_fields[i]);
                free(node->data.zone_refresh.mapped_source_fields[i]);
            }
            free(node->data.zone_refresh.mapped_target_fields);
            free(node->data.zone_refresh.mapped_source_fields);
            break;

        case AST_ZONE_MAINTAIN_EFFECT:
            free(node->data.zone_maintain_effect.effect_slot_name);
            free(node->data.zone_maintain_effect.target_slot_name);
            free(node->data.zone_maintain_effect.participant_slot_name);
            break;

        case AST_ZONE_MAINTAIN_RELATION:
            free(node->data.zone_maintain_relation.relation_slot_name);
            free(node->data.zone_maintain_relation.left_slot_name);
            free(node->data.zone_maintain_relation.right_slot_name);
            free(node->data.zone_maintain_relation.participant_slot_name);
            break;

        case AST_ZONE_MAINTAIN_STATE:
            free(node->data.zone_maintain_state.state_name);
            free(node->data.zone_maintain_state.participant_slot_name);
            break;

        case AST_ZONE_AUTHORITY:
            free(node->data.zone_authority.subject_slot_name);
            for (size_t i = 0; i < node->data.zone_authority.ability_count; i++)
                ast_destroy(node->data.zone_authority.required_abilities[i]);
            free(node->data.zone_authority.required_abilities);
            break;

        case AST_ZONE_STATE:
            free(node->data.zone_state.state_name);
            free(node->data.zone_state.layer_slot_name);
            free(node->data.zone_state.left_or_target_slot_name);
            free(node->data.zone_state.right_slot_name);
            break;

        case AST_PARTY_DECL:
            free(node->data.party_decl.name);
            for (size_t i = 0; i < node->data.party_decl.role_count; i++)
                ast_destroy(node->data.party_decl.role_slots[i]);
            free(node->data.party_decl.role_slots);
            for (size_t i = 0; i < node->data.party_decl.shared_count; i++)
                ast_destroy(node->data.party_decl.shared_fields[i]);
            free(node->data.party_decl.shared_fields);
            for (size_t i = 0; i < node->data.party_decl.method_count; i++)
                ast_destroy(node->data.party_decl.methods[i]);
            free(node->data.party_decl.methods);
            ast_destroy(node->data.party_decl.extends);
            ast_destroy_generic_params(node->data.party_decl.generic_params);
            ast_destroy_where_clause(node->data.party_decl.where_clause);
            ast_destroy_structured_comment(node->data.party_decl.doc_comment);
            break;

        case AST_ROLE_SLOT:
            free(node->data.role_slot.slot_name);
            for (size_t i = 0; i < node->data.role_slot.ability_count; i++)
                ast_destroy(node->data.role_slot.required_abilities[i]);
            free(node->data.role_slot.required_abilities);
            break;

        case AST_PARTY_SHARED:
            free(node->data.party_shared.name);
            ast_destroy(node->data.party_shared.type);
            ast_destroy(node->data.party_shared.initializer);
            break;

        case AST_CONTEXT_ACCESS:
            free(node->data.context_access.method_name);
            free(node->data.context_access.role_slot_name);
            ast_destroy(node->data.context_access.ability_type);
            break;

        case AST_PARTY_INSTANCE:
            free(node->data.party_instance.party_type);
            for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
                free(node->data.party_instance.assignments[i].slot_name);
                ast_destroy(node->data.party_instance.assignments[i].value);
            }
            free(node->data.party_instance.assignments);
            break;

        case AST_ABILITY_DECL:
            free(node->data.ability_decl.name);
            for (size_t i = 0; i < node->data.ability_decl.require_count; i++)
                ast_destroy(node->data.ability_decl.require_fields[i]);
            free(node->data.ability_decl.require_fields);
            for (size_t i = 0; i < node->data.ability_decl.method_count; i++)
                ast_destroy(node->data.ability_decl.methods[i]);
            free(node->data.ability_decl.methods);
            ast_destroy_generic_params(node->data.ability_decl.generic_params);
            ast_destroy_where_clause(node->data.ability_decl.where_clause);
            ast_destroy_structured_comment(node->data.ability_decl.doc_comment);
            break;

        case AST_ROLE_DECL:
            free(node->data.role_decl.name);
            ast_destroy(node->data.role_decl.for_type);
            for (size_t i = 0; i < node->data.role_decl.include_count; i++)
                ast_destroy(node->data.role_decl.includes[i]);
            free(node->data.role_decl.includes);
            for (size_t i = 0; i < node->data.role_decl.impl_count; i++)
                ast_destroy(node->data.role_decl.impl_abilities[i]);
            free(node->data.role_decl.impl_abilities);
            ast_destroy(node->data.role_decl.parallel_block);
            ast_destroy_generic_params(node->data.role_decl.generic_params);
            ast_destroy_where_clause(node->data.role_decl.where_clause);
            ast_destroy_structured_comment(node->data.role_decl.doc_comment);
            break;

        case AST_INCLUDE_STMT:
            free(node->data.include_stmt.role_name);
            ast_destroy_generic_params(node->data.include_stmt.type_args);
            break;

        case AST_REQUIRE_FIELD:
            free(node->data.require_field.name);
            ast_destroy(node->data.require_field.type);
            break;

        case AST_IMPL_ABILITY:
            ast_destroy(node->data.impl_ability.ability_ref);
            for (size_t i = 0; i < node->data.impl_ability.method_count; i++)
                ast_destroy(node->data.impl_ability.methods[i]);
            free(node->data.impl_ability.methods);
            break;

        case AST_OVERRIDE_FUNC:
            ast_destroy(node->data.override_func.func_decl);
            break;

        case AST_EVENT_DECL:
            free(node->data.event_decl.name);
            for (size_t i = 0; i < node->data.event_decl.param_count; i++)
                ast_destroy(node->data.event_decl.params[i]);
            free(node->data.event_decl.params);
            ast_destroy(node->data.event_decl.return_type);
            break;

        case AST_EVENT_SUBSCRIBE:
        case AST_EVENT_UNSUBSCRIBE:
            ast_destroy(node->data.event_op.event);
            ast_destroy(node->data.event_op.handler);
            break;

        case AST_EVENT_INVOKE:
            ast_destroy(node->data.event_invoke.event);
            for (size_t i = 0; i < node->data.event_invoke.arg_count; i++)
                ast_destroy(node->data.event_invoke.arguments[i]);
            free(node->data.event_invoke.arguments);
            break;

        case AST_EVENT_HANDLER_TYPE:
            for (size_t i = 0; i < node->data.event_handler_type.param_count; i++)
                ast_destroy(node->data.event_handler_type.param_types[i]);
            free(node->data.event_handler_type.param_types);
            ast_destroy(node->data.event_handler_type.return_type);
            break;

        default:
            return false;
    }

    return true;
}
