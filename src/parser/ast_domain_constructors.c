/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST domain, intent, and event constructors.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdlib.h>
// Ability declaration
ASTNode* ast_create_ability_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_ABILITY_DECL);
    node->data.ability_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.ability_decl.require_fields = NULL;
    node->data.ability_decl.require_count = 0;
    node->data.ability_decl.methods = NULL;
    node->data.ability_decl.method_count = 0;
    node->data.ability_decl.generic_params = NULL;
    node->data.ability_decl.where_clause = NULL;
    node->data.ability_decl.access = ACCESS_PUBLIC;
    node->data.ability_decl.has_explicit_access = false;
    node->data.ability_decl.is_innate = false;
    node->data.ability_decl.doc_comment = NULL;
    node->is_exported = true;
    return node;
}

// Role declaration
ASTNode* ast_create_role_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_ROLE_DECL);
    node->data.role_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.role_decl.for_type = NULL;
    node->data.role_decl.includes = NULL;
    node->data.role_decl.include_count = 0;
    node->data.role_decl.impl_abilities = NULL;
    node->data.role_decl.impl_count = 0;
    node->data.role_decl.parallel_block = NULL;
    node->data.role_decl.generic_params = NULL;
    node->data.role_decl.where_clause = NULL;
    node->data.role_decl.doc_comment = NULL;
    return node;
}

// Include statement
ASTNode* ast_create_include_statement(const char* role_name) {
    ASTNode* node = ast_create_node(AST_INCLUDE_STMT);
    node->data.include_stmt.role_name = role_name ? pergyra_strdup(role_name) : NULL;
    node->data.include_stmt.type_args = NULL;
    return node;
}

// Require field
ASTNode* ast_create_require_field(const char* name) {
    ASTNode* node = ast_create_node(AST_REQUIRE_FIELD);
    node->data.require_field.name = name ? pergyra_strdup(name) : NULL;
    node->data.require_field.type = NULL;
    return node;
}

// Impl ability block
ASTNode* ast_create_impl_ability(ASTNode* ability_ref) {
    ASTNode* node = ast_create_node(AST_IMPL_ABILITY);
    node->data.impl_ability.ability_ref = ability_ref;
    node->data.impl_ability.methods = NULL;
    node->data.impl_ability.method_count = 0;
    return node;
}

// Override function
ASTNode* ast_create_override_func(ASTNode* func_decl) {
    ASTNode* node = ast_create_node(AST_OVERRIDE_FUNC);
    node->data.override_func.func_decl = func_decl;
    node->data.override_func.calls_super = false;
    return node;
}

// Roster declaration
ASTNode* ast_create_roster_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_ROSTER_DECL);
    node->data.roster_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.roster_decl.party_slots = NULL;
    node->data.roster_decl.party_count = 0;
    node->data.roster_decl.shared_fields = NULL;
    node->data.roster_decl.shared_count = 0;
    node->data.roster_decl.methods = NULL;
    node->data.roster_decl.method_count = 0;
    node->data.roster_decl.generic_params = NULL;
    node->data.roster_decl.doc_comment = NULL;
    return node;
}

// Roster slot
ASTNode* ast_create_roster_slot(const char* slot_name, const char* party_type) {
    ASTNode* node = ast_create_node(AST_SYSTEMIC_SLOT);
    node->data.roster_slot.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.roster_slot.party_type = party_type ? pergyra_strdup(party_type) : NULL;
    node->data.roster_slot.is_array = false;
    return node;
}

// World declaration
ASTNode* ast_create_world_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_WORLD_DECL);
    node->data.world_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.world_decl.rosters = NULL;
    node->data.world_decl.roster_count = 0;
    node->data.world_decl.zones = NULL;
    node->data.world_decl.zone_count = 0;
    node->data.world_decl.shared_fields = NULL;
    node->data.world_decl.shared_count = 0;
    node->data.world_decl.methods = NULL;
    node->data.world_decl.method_count = 0;
    node->data.world_decl.activations = NULL;
    node->data.world_decl.activate_count = 0;
    node->data.world_decl.deactivations = NULL;
    node->data.world_decl.deactivate_count = 0;
    node->data.world_decl.maintained_zones = NULL;
    node->data.world_decl.maintained_zone_count = 0;
    node->data.world_decl.states = NULL;
    node->data.world_decl.state_count = 0;
    node->data.world_decl.doc_comment = NULL;
    return node;
}

// World roster instance
ASTNode* ast_create_world_roster(const char* slot_name, const char* roster_type) {
    ASTNode* node = ast_create_node(AST_WORLD_SYSTEMIC);
    node->data.world_roster.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.world_roster.roster_type = roster_type ? pergyra_strdup(roster_type) : NULL;
    node->data.world_roster.initializer = NULL;
    return node;
}

ASTNode* ast_create_world_zone(const char* slot_name, const char* zone_type) {
    ASTNode* node = ast_create_node(AST_WORLD_ZONE);
    node->data.world_zone.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.world_zone.zone_type = zone_type ? pergyra_strdup(zone_type) : NULL;
    node->data.world_zone.initializer = NULL;
    return node;
}

ASTNode* ast_create_world_activate(const char* zone_slot_name) {
    ASTNode* node = ast_create_node(AST_WORLD_ACTIVATE);
    node->data.world_activate.zone_slot_name =
        zone_slot_name ? pergyra_strdup(zone_slot_name) : NULL;
    node->data.world_activate.state_name = NULL;
    return node;
}

ASTNode* ast_create_world_deactivate(const char* zone_slot_name) {
    ASTNode* node = ast_create_node(AST_WORLD_DEACTIVATE);
    node->data.world_deactivate.zone_slot_name =
        zone_slot_name ? pergyra_strdup(zone_slot_name) : NULL;
    node->data.world_deactivate.state_name = NULL;
    return node;
}

ASTNode* ast_create_world_maintain(const char* zone_slot_name) {
    ASTNode* node = ast_create_node(AST_WORLD_MAINTAIN);
    node->data.world_maintain.zone_slot_name =
        zone_slot_name ? pergyra_strdup(zone_slot_name) : NULL;
    node->data.world_maintain.state_name = NULL;
    return node;
}

ASTNode* ast_create_world_state(const char* state_name, const char* zone_slot_name,
                                WorldStateSourceKind source_kind,
                                const char* detail_name) {
    ASTNode* node = ast_create_node(AST_WORLD_STATE);
    node->data.world_state.state_name =
        state_name ? pergyra_strdup(state_name) : NULL;
    node->data.world_state.zone_slot_name =
        zone_slot_name ? pergyra_strdup(zone_slot_name) : NULL;
    node->data.world_state.source_kind = source_kind;
    node->data.world_state.detail_name =
        detail_name ? pergyra_strdup(detail_name) : NULL;
    node->data.world_state.input_names = NULL;
    node->data.world_state.input_count = 0;
    return node;
}

ASTNode* ast_create_world_state_compose(const char* state_name,
                                        WorldStateSourceKind source_kind,
                                        const char** input_names,
                                        size_t input_count) {
    ASTNode* node = ast_create_node(AST_WORLD_STATE);
    node->data.world_state.state_name =
        state_name ? pergyra_strdup(state_name) : NULL;
    node->data.world_state.zone_slot_name = NULL;
    node->data.world_state.source_kind = source_kind;
    node->data.world_state.detail_name = NULL;
    node->data.world_state.input_names = NULL;
    node->data.world_state.input_count = input_count;
    if (input_count > 0) {
        node->data.world_state.input_names = calloc(input_count, sizeof(char*));
        for (size_t i = 0; i < input_count; i++) {
            node->data.world_state.input_names[i] =
                input_names != NULL && input_names[i] != NULL
                    ? pergyra_strdup(input_names[i]) : NULL;
        }
    }
    return node;
}

ASTNode* ast_create_intent_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_INTENT_DECL);
    node->data.intent_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.intent_decl.involves = NULL;
    node->data.intent_decl.involve_count = 0;
    node->data.intent_decl.involve_capacity = 0;
    node->data.intent_decl.values = NULL;
    node->data.intent_decl.value_count = 0;
    node->data.intent_decl.value_capacity = 0;
    node->data.intent_decl.bindings = NULL;
    node->data.intent_decl.binding_count = 0;
    node->data.intent_decl.binding_capacity = 0;
    node->data.intent_decl.steps = NULL;
    node->data.intent_decl.step_count = 0;
    node->data.intent_decl.step_capacity = 0;
    node->data.intent_decl.is_concurrent = false;
    node->data.intent_decl.rollback_policy = INTENT_ROLLBACK_FULL;
    node->data.intent_decl.priority_expr = NULL;
    node->data.intent_decl.success_expr = NULL;
    node->data.intent_decl.failure_expr = NULL;
    node->data.intent_decl.doc_comment = NULL;
    node->data.intent_decl.default_who_names = NULL;
    node->data.intent_decl.default_who_count = 0;
    node->data.intent_decl.default_who_capacity = 0;
    node->data.intent_decl.default_where_type = NULL;
    return node;
}

ASTNode* ast_create_intent_involves(const char* alias) {
    ASTNode* node = ast_create_node(AST_INTENT_INVOLVES);
    node->data.intent_involves.alias = alias ? pergyra_strdup(alias) : NULL;
    node->data.intent_involves.subject_type = NULL;
    return node;
}

ASTNode* ast_create_intent_value(const char* alias) {
    ASTNode* node = ast_create_node(AST_INTENT_VALUE);
    node->data.intent_value.alias = alias ? pergyra_strdup(alias) : NULL;
    node->data.intent_value.value_type = NULL;
    return node;
}

ASTNode* ast_create_intent_step(const char* name) {
    ASTNode* node = ast_create_node(AST_INTENT_STEP);
    node->data.intent_step.name = name ? pergyra_strdup(name) : NULL;
    node->data.intent_step.where_type = NULL;
    node->data.intent_step.using_expr = NULL;
    node->data.intent_step.intent_expr = NULL;
    node->data.intent_step.transfer_from_alias = NULL;
    node->data.intent_step.transfer_to_alias = NULL;
    node->data.intent_step.who_names = NULL;
    node->data.intent_step.who_count = 0;
    node->data.intent_step.on_exprs = NULL;
    node->data.intent_step.on_expr_count = 0;
    node->data.intent_step.on_expr_capacity = 0;
    node->data.intent_step.compensate_exprs = NULL;
    node->data.intent_step.compensate_expr_count = 0;
    node->data.intent_step.compensate_expr_capacity = 0;
    node->data.intent_step.pre_expr = NULL;
    node->data.intent_step.guard_expr = NULL;
    node->data.intent_step.post_expr = NULL;
    node->data.intent_step.invariant_expr = NULL;
    node->data.intent_step.required_abilities = NULL;
    node->data.intent_step.required_ability_count = 0;
    node->data.intent_step.causes_effect = NULL;
    node->data.intent_step.authorized_by = NULL;
    node->data.intent_step.authorized_by_count = 0;
    node->data.intent_step.expect_expr = NULL;
    node->data.intent_step.inherited_who_from_intent = false;
    node->data.intent_step.inherited_where_from_intent = false;
    node->data.intent_step.inherited_who_from_action = false;
    node->data.intent_step.inherited_where_from_action = false;
    node->data.intent_step.inherited_requires_from_action = false;
    node->data.intent_step.inherited_causes_from_action = false;
    node->data.intent_step.inherited_authorized_by_from_action = false;
    node->data.intent_step.derived_authorized_by_from_zone = false;
    node->data.intent_step.derived_where_from_using = false;
    node->data.intent_step.derived_where_from_transfer = false;
    node->data.intent_step.derived_using_from_transfer = false;
    return node;
}

ASTNode* ast_create_relation_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_RELATION_DECL);
    node->data.relation_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.relation_decl.slots = NULL;
    node->data.relation_decl.slot_count = 0;
    node->data.relation_decl.refreshes = NULL;
    node->data.relation_decl.refresh_count = 0;
    node->data.relation_decl.shared_fields = NULL;
    node->data.relation_decl.shared_count = 0;
    node->data.relation_decl.methods = NULL;
    node->data.relation_decl.method_count = 0;
    node->data.relation_decl.doc_comment = NULL;
    node->data.relation_decl.between_left_kind = RELATION_ENDPOINT_NAMED;
    node->data.relation_decl.between_right_kind = RELATION_ENDPOINT_NAMED;
    node->data.relation_decl.between_left_type = NULL;
    node->data.relation_decl.between_right_type = NULL;
    node->data.relation_decl.between_left_many = false;
    node->data.relation_decl.between_right_many = false;
    return node;
}

ASTNode* ast_create_effect_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_EFFECT_DECL);
    node->data.effect_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.effect_decl.slots = NULL;
    node->data.effect_decl.slot_count = 0;
    node->data.effect_decl.refreshes = NULL;
    node->data.effect_decl.refresh_count = 0;
    node->data.effect_decl.shared_fields = NULL;
    node->data.effect_decl.shared_count = 0;
    node->data.effect_decl.methods = NULL;
    node->data.effect_decl.method_count = 0;
    node->data.effect_decl.doc_comment = NULL;
    return node;
}

ASTNode* ast_create_zone_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_ZONE_DECL);
    node->data.zone_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.zone_decl.slots = NULL;
    node->data.zone_decl.slot_count = 0;
    node->data.zone_decl.layer_slots = NULL;
    node->data.zone_decl.layer_slot_count = 0;
    node->data.zone_decl.applies = NULL;
    node->data.zone_decl.apply_count = 0;
    node->data.zone_decl.links = NULL;
    node->data.zone_decl.link_count = 0;
    node->data.zone_decl.detaches = NULL;
    node->data.zone_decl.detach_count = 0;
    node->data.zone_decl.unlinks = NULL;
    node->data.zone_decl.unlink_count = 0;
    node->data.zone_decl.refreshes = NULL;
    node->data.zone_decl.refresh_count = 0;
    node->data.zone_decl.maintained_effects = NULL;
    node->data.zone_decl.maintained_effect_count = 0;
    node->data.zone_decl.maintained_relations = NULL;
    node->data.zone_decl.maintained_relation_count = 0;
    node->data.zone_decl.maintained_states = NULL;
    node->data.zone_decl.maintained_state_count = 0;
    node->data.zone_decl.authorities = NULL;
    node->data.zone_decl.authority_count = 0;
    node->data.zone_decl.states = NULL;
    node->data.zone_decl.state_count = 0;
    node->data.zone_decl.shared_fields = NULL;
    node->data.zone_decl.shared_count = 0;
    node->data.zone_decl.methods = NULL;
    node->data.zone_decl.method_count = 0;
    node->data.zone_decl.doc_comment = NULL;
    return node;
}

ASTNode* ast_create_domain_slot(const char* slot_name, bool is_subject) {
    ASTNode* node = ast_create_node(AST_DOMAIN_SLOT);
    node->data.domain_slot.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.domain_slot.type = NULL;
    node->data.domain_slot.is_subject = is_subject;
    node->data.domain_slot.is_vessel = false;
    node->data.domain_slot.is_tobject = false;
    node->data.domain_slot.is_binding = is_subject;
    node->data.domain_slot.initializer = NULL;
    return node;
}

ASTNode* ast_create_zone_layer_slot(const char* slot_name, const char* layer_type, bool is_relation) {
    ASTNode* node = ast_create_node(AST_ZONE_LAYER_SLOT);
    node->data.zone_layer_slot.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.zone_layer_slot.layer_type = layer_type ? pergyra_strdup(layer_type) : NULL;
    node->data.zone_layer_slot.is_relation = is_relation;
    node->data.zone_layer_slot.is_pool = false;
    node->data.zone_layer_slot.pool_capacity = 0;
    return node;
}

ASTNode* ast_create_zone_apply(const char* effect_slot_name, const char* target_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_APPLY);
    node->data.zone_apply.effect_slot_name =
        effect_slot_name ? pergyra_strdup(effect_slot_name) : NULL;
    node->data.zone_apply.target_slot_name =
        target_slot_name ? pergyra_strdup(target_slot_name) : NULL;
    node->data.zone_apply.state_name = NULL;
    node->data.zone_apply.participant_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_link(const char* relation_slot_name, const char* left_slot_name, const char* right_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_LINK);
    node->data.zone_link.relation_slot_name =
        relation_slot_name ? pergyra_strdup(relation_slot_name) : NULL;
    node->data.zone_link.left_slot_name =
        left_slot_name ? pergyra_strdup(left_slot_name) : NULL;
    node->data.zone_link.right_slot_name =
        right_slot_name ? pergyra_strdup(right_slot_name) : NULL;
    node->data.zone_link.state_name = NULL;
    node->data.zone_link.participant_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_detach(const char* effect_slot_name, const char* target_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_DETACH);
    node->data.zone_detach.effect_slot_name =
        effect_slot_name ? pergyra_strdup(effect_slot_name) : NULL;
    node->data.zone_detach.target_slot_name =
        target_slot_name ? pergyra_strdup(target_slot_name) : NULL;
    node->data.zone_detach.state_name = NULL;
    node->data.zone_detach.participant_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_unlink(const char* relation_slot_name, const char* left_slot_name, const char* right_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_UNLINK);
    node->data.zone_unlink.relation_slot_name =
        relation_slot_name ? pergyra_strdup(relation_slot_name) : NULL;
    node->data.zone_unlink.left_slot_name =
        left_slot_name ? pergyra_strdup(left_slot_name) : NULL;
    node->data.zone_unlink.right_slot_name =
        right_slot_name ? pergyra_strdup(right_slot_name) : NULL;
    node->data.zone_unlink.state_name = NULL;
    node->data.zone_unlink.participant_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_refresh(const char* object_slot_name, const char* source_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_REFRESH);
    node->data.zone_refresh.object_slot_name =
        object_slot_name ? pergyra_strdup(object_slot_name) : NULL;
    node->data.zone_refresh.source_slot_name =
        source_slot_name ? pergyra_strdup(source_slot_name) : NULL;
    node->data.zone_refresh.participant_slot_name = NULL;
    node->data.zone_refresh.requires_dto = false;
    node->data.zone_refresh.derive_target_kind = false;
    node->data.zone_refresh.mapped_target_fields = NULL;
    node->data.zone_refresh.mapped_source_fields = NULL;
    node->data.zone_refresh.field_map_count = 0;
    return node;
}

ASTNode* ast_create_zone_maintain_effect(const char* effect_slot_name, const char* target_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_MAINTAIN_EFFECT);
    node->data.zone_maintain_effect.effect_slot_name =
        effect_slot_name ? pergyra_strdup(effect_slot_name) : NULL;
    node->data.zone_maintain_effect.target_slot_name =
        target_slot_name ? pergyra_strdup(target_slot_name) : NULL;
    node->data.zone_maintain_effect.participant_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_maintain_relation(const char* relation_slot_name, const char* left_slot_name, const char* right_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_MAINTAIN_RELATION);
    node->data.zone_maintain_relation.relation_slot_name =
        relation_slot_name ? pergyra_strdup(relation_slot_name) : NULL;
    node->data.zone_maintain_relation.left_slot_name =
        left_slot_name ? pergyra_strdup(left_slot_name) : NULL;
    node->data.zone_maintain_relation.right_slot_name =
        right_slot_name ? pergyra_strdup(right_slot_name) : NULL;
    node->data.zone_maintain_relation.participant_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_maintain_state(const char* state_name) {
    ASTNode* node = ast_create_node(AST_ZONE_MAINTAIN_STATE);
    node->data.zone_maintain_state.state_name =
        state_name ? pergyra_strdup(state_name) : NULL;
    node->data.zone_maintain_state.participant_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_authority(const char* subject_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_AUTHORITY);
    node->data.zone_authority.subject_slot_name =
        subject_slot_name ? pergyra_strdup(subject_slot_name) : NULL;
    node->data.zone_authority.required_abilities = NULL;
    node->data.zone_authority.ability_count = 0;
    return node;
}

ASTNode* ast_create_zone_state(const char* state_name, bool is_relation,
                               const char* layer_slot_name,
                               const char* left_or_target_slot_name,
                               const char* right_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_STATE);
    node->data.zone_state.state_name =
        state_name ? pergyra_strdup(state_name) : NULL;
    node->data.zone_state.is_relation = is_relation;
    node->data.zone_state.layer_slot_name =
        layer_slot_name ? pergyra_strdup(layer_slot_name) : NULL;
    node->data.zone_state.left_or_target_slot_name =
        left_or_target_slot_name ? pergyra_strdup(left_or_target_slot_name) : NULL;
    node->data.zone_state.right_slot_name =
        right_slot_name ? pergyra_strdup(right_slot_name) : NULL;
    return node;
}
