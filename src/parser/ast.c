/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST (Abstract Syntax Tree) implementation
 */

#include "ast.h"
#include "../common/string_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============= AST 노드 생성 함수들 =============

static void ast_destroy_generic_params(GenericParams* params);
static void ast_destroy_where_clause(WhereClause* clause);
void ast_destroy_structured_comment(StructuredComment* comment);

static const char *
nominal_decl_kind_name(NominalDeclKind kind)
{
    switch (kind) {
    case NOMINAL_DECL_SUBJECT:
        return "Subject";
    case NOMINAL_DECL_VESSEL:
        return "Vessel";
    case NOMINAL_DECL_STRUCT:
        return "Struct";
    case NOMINAL_DECL_OBJECT:
        return "Object";
    case NOMINAL_DECL_DTO:
        return "Dto";
    case NOMINAL_DECL_CLASS:
    default:
        return "Class";
    }
}

// 기본 노드 생성
static ASTNode* ast_create_node(ASTNodeType type) {
    ASTNode* node = calloc(1, sizeof(ASTNode));
    if (!node) return NULL;
    node->type = type;
    node->is_exported = false;
    node->is_async_decl = false;
    return node;
}

// 프로그램 노드
ASTNode* ast_create_program(void) {
    ASTNode* node = ast_create_node(AST_PROGRAM);
    node->data.program.statements = NULL;
    node->data.program.count = 0;
    return node;
}

// 함수 선언
ASTNode* ast_create_function(const char* name) {
    ASTNode* node = ast_create_node(AST_FUNC_DECL);
    node->data.func_decl.name = pergyra_strdup(name);
    node->data.func_decl.params = NULL;
    node->data.func_decl.param_count = 0;
    node->data.func_decl.return_type = NULL;
    node->data.func_decl.body = NULL;
    node->data.func_decl.generic_params = NULL;
    node->data.func_decl.where_clause = NULL;
    node->data.func_decl.has_effects_clause = false;
    node->data.func_decl.declared_effects = 0;
    node->data.func_decl.access = ACCESS_PUBLIC;
    node->data.func_decl.is_action = false;
    node->data.func_decl.required_abilities = NULL;
    node->data.func_decl.required_ability_count = 0;
    node->data.func_decl.within_zone = NULL;
    node->data.func_decl.causes_effect = NULL;
    node->data.func_decl.authorized_by = NULL;
    node->data.func_decl.authorized_by_count = 0;
    return node;
}

// 클래스 선언
ASTNode* ast_create_class(const char* name) {
    ASTNode* node = ast_create_node(AST_CLASS_DECL);
    node->data.class_decl.name = pergyra_strdup(name);
    node->data.class_decl.fields = NULL;
    node->data.class_decl.field_count = 0;
    node->data.class_decl.methods = NULL;
    node->data.class_decl.method_count = 0;
    node->data.class_decl.generic_params = NULL;
    node->data.class_decl.where_clause = NULL;
    node->data.class_decl.is_struct = false;
    node->data.class_decl.nominal_kind = NOMINAL_DECL_CLASS;
    return node;
}

ASTNode* ast_create_subject(const char* name) {
    ASTNode* node = ast_create_class(name);
    if (node) {
        node->data.class_decl.nominal_kind = NOMINAL_DECL_SUBJECT;
    }
    return node;
}

ASTNode* ast_create_vessel(const char* name) {
    ASTNode* node = ast_create_struct(name);
    if (node) {
        node->data.class_decl.nominal_kind = NOMINAL_DECL_VESSEL;
    }
    return node;
}

// 구조체 선언
ASTNode* ast_create_struct(const char* name) {
    ASTNode* node = ast_create_class(name);
    if (node) {
        node->data.class_decl.is_struct = true;
        node->data.class_decl.nominal_kind = NOMINAL_DECL_STRUCT;
    }
    return node;
}

ASTNode* ast_create_object(const char* name) {
    ASTNode* node = ast_create_struct(name);
    if (node) {
        node->data.class_decl.nominal_kind = NOMINAL_DECL_OBJECT;
    }
    return node;
}

ASTNode* ast_create_dto(const char* name) {
    ASTNode* node = ast_create_struct(name);
    if (node) {
        node->data.class_decl.nominal_kind = NOMINAL_DECL_DTO;
    }
    return node;
}

ASTNode* ast_create_extern_block(const char* abi) {
    ASTNode* node = ast_create_node(AST_EXTERN_BLOCK);
    if (!node) return NULL;
    node->data.extern_block.abi = pergyra_strdup(abi);
    node->data.extern_block.declarations = NULL;
    node->data.extern_block.count = 0;
    return node;
}

// let 선언
ASTNode* ast_create_let_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_LET_DECL);
    node->data.let_decl.name = pergyra_strdup(name);
    node->data.let_decl.type = NULL;
    node->data.let_decl.initializer = NULL;
    node->data.let_decl.is_mutable = false;
    return node;
}

// with 문
ASTNode* ast_create_with_statement(void) {
    ASTNode* node = ast_create_node(AST_WITH_STMT);
    node->data.with_stmt.slot_type = NULL;
    node->data.with_stmt.alias = NULL;
    node->data.with_stmt.body = NULL;
    node->data.with_stmt.is_secure = false;
    node->data.with_stmt.security_level = NULL;
    return node;
}

// parallel 블록
ASTNode* ast_create_parallel_block(void) {
    ASTNode* node = ast_create_node(AST_PARALLEL_BLOCK);
    node->data.parallel.tasks = NULL;
    node->data.parallel.task_count = 0;
    return node;
}

// 블록
ASTNode* ast_create_block(void) {
    ASTNode* node = ast_create_node(AST_BLOCK);
    node->data.block.statements = NULL;
    node->data.block.count = 0;
    return node;
}

// for 루프
ASTNode* ast_create_for_loop(void) {
    ASTNode* node = ast_create_node(AST_FOR_LOOP);
    node->data.for_loop.variable = NULL;
    node->data.for_loop.range_start = NULL;
    node->data.for_loop.range_end = NULL;
    node->data.for_loop.iterable = NULL;
    node->data.for_loop.body = NULL;
    return node;
}

// while 루프
ASTNode* ast_create_while_loop(void) {
    ASTNode* node = ast_create_node(AST_WHILE_LOOP);
    node->data.while_loop.condition = NULL;
    node->data.while_loop.body = NULL;
    return node;
}

// match 문
ASTNode* ast_create_match_statement(void) {
    ASTNode* node = ast_create_node(AST_MATCH_STMT);
    node->data.match_stmt.subject = NULL;
    node->data.match_stmt.cases = NULL;
    node->data.match_stmt.case_count = 0;
    node->data.match_stmt.default_body = NULL;
    return node;
}

// match case
ASTNode* ast_create_match_case(void) {
    ASTNode* node = ast_create_node(AST_MATCH_CASE);
    node->data.match_case.pattern = NULL;
    node->data.match_case.guard = NULL;
    node->data.match_case.body = NULL;
    return node;
}

// Ability declaration
ASTNode* ast_create_ability_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_ABILITY_DECL);
    node->data.ability_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.ability_decl.require_fields = NULL;
    node->data.ability_decl.require_count = 0;
    node->data.ability_decl.methods = NULL;
    node->data.ability_decl.method_count = 0;
    node->data.ability_decl.doc_comment = NULL;
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
ASTNode* ast_create_impl_ability(const char* ability_name) {
    ASTNode* node = ast_create_node(AST_IMPL_ABILITY);
    node->data.impl_ability.ability_name = ability_name ? pergyra_strdup(ability_name) : NULL;
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

// Systemic declaration
ASTNode* ast_create_systemic_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_SYSTEMIC_DECL);
    node->data.systemic_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.systemic_decl.party_slots = NULL;
    node->data.systemic_decl.party_count = 0;
    node->data.systemic_decl.shared_fields = NULL;
    node->data.systemic_decl.shared_count = 0;
    node->data.systemic_decl.methods = NULL;
    node->data.systemic_decl.method_count = 0;
    node->data.systemic_decl.generic_params = NULL;
    node->data.systemic_decl.doc_comment = NULL;
    return node;
}

// Systemic slot
ASTNode* ast_create_systemic_slot(const char* slot_name, const char* party_type) {
    ASTNode* node = ast_create_node(AST_SYSTEMIC_SLOT);
    node->data.systemic_slot.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.systemic_slot.party_type = party_type ? pergyra_strdup(party_type) : NULL;
    node->data.systemic_slot.is_array = false;
    return node;
}

// World declaration
ASTNode* ast_create_world_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_WORLD_DECL);
    node->data.world_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.world_decl.systemics = NULL;
    node->data.world_decl.systemic_count = 0;
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

// World systemic instance
ASTNode* ast_create_world_systemic(const char* slot_name, const char* systemic_type) {
    ASTNode* node = ast_create_node(AST_WORLD_SYSTEMIC);
    node->data.world_systemic.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.world_systemic.systemic_type = systemic_type ? pergyra_strdup(systemic_type) : NULL;
    node->data.world_systemic.initializer = NULL;
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
    node->data.domain_slot.is_dto = false;
    node->data.domain_slot.is_binding = is_subject;
    node->data.domain_slot.initializer = NULL;
    return node;
}

ASTNode* ast_create_zone_layer_slot(const char* slot_name, const char* layer_type, bool is_relation) {
    ASTNode* node = ast_create_node(AST_ZONE_LAYER_SLOT);
    node->data.zone_layer_slot.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.zone_layer_slot.layer_type = layer_type ? pergyra_strdup(layer_type) : NULL;
    node->data.zone_layer_slot.is_relation = is_relation;
    return node;
}

ASTNode* ast_create_zone_apply(const char* effect_slot_name, const char* target_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_APPLY);
    node->data.zone_apply.effect_slot_name =
        effect_slot_name ? pergyra_strdup(effect_slot_name) : NULL;
    node->data.zone_apply.target_slot_name =
        target_slot_name ? pergyra_strdup(target_slot_name) : NULL;
    node->data.zone_apply.state_name = NULL;
    node->data.zone_apply.actor_slot_name = NULL;
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
    node->data.zone_link.actor_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_detach(const char* effect_slot_name, const char* target_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_DETACH);
    node->data.zone_detach.effect_slot_name =
        effect_slot_name ? pergyra_strdup(effect_slot_name) : NULL;
    node->data.zone_detach.target_slot_name =
        target_slot_name ? pergyra_strdup(target_slot_name) : NULL;
    node->data.zone_detach.state_name = NULL;
    node->data.zone_detach.actor_slot_name = NULL;
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
    node->data.zone_unlink.actor_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_refresh(const char* object_slot_name, const char* source_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_REFRESH);
    node->data.zone_refresh.object_slot_name =
        object_slot_name ? pergyra_strdup(object_slot_name) : NULL;
    node->data.zone_refresh.source_slot_name =
        source_slot_name ? pergyra_strdup(source_slot_name) : NULL;
    node->data.zone_refresh.actor_slot_name = NULL;
    node->data.zone_refresh.requires_dto = false;
    return node;
}

ASTNode* ast_create_zone_maintain_effect(const char* effect_slot_name, const char* target_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_MAINTAIN_EFFECT);
    node->data.zone_maintain_effect.effect_slot_name =
        effect_slot_name ? pergyra_strdup(effect_slot_name) : NULL;
    node->data.zone_maintain_effect.target_slot_name =
        target_slot_name ? pergyra_strdup(target_slot_name) : NULL;
    node->data.zone_maintain_effect.actor_slot_name = NULL;
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
    node->data.zone_maintain_relation.actor_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_maintain_state(const char* state_name) {
    ASTNode* node = ast_create_node(AST_ZONE_MAINTAIN_STATE);
    node->data.zone_maintain_state.state_name =
        state_name ? pergyra_strdup(state_name) : NULL;
    node->data.zone_maintain_state.actor_slot_name = NULL;
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

// Party declaration
ASTNode* ast_create_party_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_PARTY_DECL);
    node->data.party_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.party_decl.role_slots = NULL;
    node->data.party_decl.role_count = 0;
    node->data.party_decl.shared_fields = NULL;
    node->data.party_decl.shared_count = 0;
    node->data.party_decl.methods = NULL;
    node->data.party_decl.method_count = 0;
    node->data.party_decl.extends = NULL;
    node->data.party_decl.generic_params = NULL;
    node->data.party_decl.doc_comment = NULL;
    return node;
}

// Role slot in party
ASTNode* ast_create_role_slot(const char* slot_name) {
    ASTNode* node = ast_create_node(AST_ROLE_SLOT);
    node->data.role_slot.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.role_slot.required_abilities = NULL;
    node->data.role_slot.ability_count = 0;
    node->data.role_slot.is_array = false;
    return node;
}

// Party shared field
ASTNode* ast_create_party_shared(const char* name) {
    ASTNode* node = ast_create_node(AST_PARTY_SHARED);
    node->data.party_shared.name = name ? pergyra_strdup(name) : NULL;
    node->data.party_shared.type = NULL;
    node->data.party_shared.initializer = NULL;
    node->data.party_shared.access = ACCESS_PUBLIC;
    return node;
}

// Context access
ASTNode* ast_create_context_access(const char* method_name, const char* slot_name) {
    ASTNode* node = ast_create_node(AST_CONTEXT_ACCESS);
    node->data.context_access.method_name = method_name ? pergyra_strdup(method_name) : NULL;
    node->data.context_access.role_slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.context_access.ability_type = NULL;
    return node;
}

// Party instance creation
ASTNode* ast_create_party_instance(const char* party_type) {
    ASTNode* node = ast_create_node(AST_PARTY_INSTANCE);
    node->data.party_instance.party_type = party_type ? pergyra_strdup(party_type) : NULL;
    node->data.party_instance.assignments = NULL;
    node->data.party_instance.assignment_count = 0;
    return node;
}

// Event declaration
ASTNode* ast_create_event_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_EVENT_DECL);
    node->data.event_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.event_decl.params = NULL;
    node->data.event_decl.param_count = 0;
    node->data.event_decl.return_type = NULL;
    node->data.event_decl.access = ACCESS_PUBLIC;
    return node;
}

// Event subscribe
ASTNode* ast_create_event_subscribe(ASTNode* event, ASTNode* handler) {
    ASTNode* node = ast_create_node(AST_EVENT_SUBSCRIBE);
    node->data.event_op.event = event;
    node->data.event_op.handler = handler;
    return node;
}

// Event unsubscribe
ASTNode* ast_create_event_unsubscribe(ASTNode* event, ASTNode* handler) {
    ASTNode* node = ast_create_node(AST_EVENT_UNSUBSCRIBE);
    node->data.event_op.event = event;
    node->data.event_op.handler = handler;
    return node;
}

// Event invoke
ASTNode* ast_create_event_invoke(ASTNode* event) {
    ASTNode* node = ast_create_node(AST_EVENT_INVOKE);
    node->data.event_invoke.event = event;
    node->data.event_invoke.arguments = NULL;
    node->data.event_invoke.arg_count = 0;
    return node;
}

// Event handler type
ASTNode* ast_create_event_handler_type(void) {
    ASTNode* node = ast_create_node(AST_EVENT_HANDLER_TYPE);
    node->data.event_handler_type.param_types = NULL;
    node->data.event_handler_type.param_count = 0;
    node->data.event_handler_type.return_type = NULL;
    return node;
}

// Lambda expression
ASTNode* ast_create_lambda_expression(void) {
    ASTNode* node = ast_create_node(AST_LAMBDA_EXPR);
    node->data.lambda_expr.params = NULL;
    node->data.lambda_expr.param_count = 0;
    node->data.lambda_expr.body = NULL;
    node->data.lambda_expr.return_type = NULL;
    node->data.lambda_expr.is_async = false;
    return node;
}

ASTNode* ast_create_import_declaration(const char* path) {
    ASTNode* node = ast_create_node(AST_IMPORT_DECL);
    node->data.import_decl.path = pergyra_strdup(path);
    return node;
}

ASTNode* ast_create_namespace_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_NAMESPACE_DECL);
    if (!node) return NULL;
    node->data.namespace_decl.name = pergyra_strdup(name);
    node->data.namespace_decl.statements = NULL;
    node->data.namespace_decl.count = 0;
    return node;
}

ASTNode* ast_create_unsafe_block(ASTNode* body) {
    ASTNode* node = ast_create_node(AST_UNSAFE_BLOCK);
    node->data.unsafe_block.body = body;
    return node;
}

ASTNode* ast_create_defer_statement(ASTNode* body) {
    ASTNode* node = ast_create_node(AST_DEFER_STMT);
    node->data.defer_stmt.body = body;
    return node;
}

ASTNode* ast_create_bind_statement(const char* party_var, const char* slot_name, const char* role_name) {
    ASTNode* node = ast_create_node(AST_BIND_STMT);
    node->data.bind_stmt.party_var = pergyra_strdup(party_var);
    node->data.bind_stmt.slot_name = pergyra_strdup(slot_name);
    node->data.bind_stmt.role_name = pergyra_strdup(role_name);
    return node;
}

// if 문
ASTNode* ast_create_if_statement(void) {
    ASTNode* node = ast_create_node(AST_IF_STMT);
    node->data.if_stmt.condition = NULL;
    node->data.if_stmt.then_branch = NULL;
    node->data.if_stmt.else_branch = NULL;
    return node;
}

// return 문
ASTNode* ast_create_return_statement(void) {
    ASTNode* node = ast_create_node(AST_RETURN);
    node->data.return_stmt.value = NULL;
    return node;
}

// 표현식 노드들

// 이항 연산
ASTNode* ast_create_binary(ASTNode* left, Token op, ASTNode* right) {
    ASTNode* node = ast_create_node(AST_BINARY);
    node->data.binary.left = left;
    node->data.binary.op = op;
    node->data.binary.right = right;
    return node;
}

// 단항 연산
ASTNode* ast_create_unary(Token op, ASTNode* operand) {
    ASTNode* node = ast_create_node(AST_UNARY);
    node->data.unary.op = op;
    node->data.unary.operand = operand;
    return node;
}

// 함수 호출
ASTNode* ast_create_call(ASTNode* callee) {
    ASTNode* node = ast_create_node(AST_CALL);
    node->data.call.callee = callee;
    node->data.call.arguments = NULL;
    node->data.call.arg_count = 0;
    return node;
}

// 멤버 접근
ASTNode* ast_create_member_access(ASTNode* object, const char* member) {
    ASTNode* node = ast_create_node(AST_MEMBER_ACCESS);
    node->data.member.object = object;
    node->data.member.name = pergyra_strdup(member);
    return node;
}

// 배열 접근
ASTNode* ast_create_array_access(ASTNode* array, ASTNode* index) {
    ASTNode* node = ast_create_node(AST_ARRAY_ACCESS);
    node->data.array_access.array = array;
    node->data.array_access.index = index;
    return node;
}

// 할당
ASTNode* ast_create_assignment(ASTNode* target, ASTNode* value) {
    ASTNode* node = ast_create_node(AST_ASSIGNMENT);
    node->data.assignment.target = target;
    node->data.assignment.value = value;
    return node;
}

// 리터럴

// 숫자
ASTNode* ast_create_number(const char* value) {
    ASTNode* node = ast_create_node(AST_NUMBER);
    node->data.number.value = strtod(value, NULL);
    return node;
}

// 문자열
ASTNode* ast_create_string(const char* value) {
    ASTNode* node = ast_create_node(AST_STRING);
    // 따옴표 제거
    size_t len = strlen(value);
    if (len >= 2 && value[0] == '"' && value[len-1] == '"') {
        node->data.string.value = pergyra_strndup(value + 1, len - 2);
    } else {
        node->data.string.value = pergyra_strdup(value);
    }
    return node;
}

// 불린
ASTNode* ast_create_boolean(bool value) {
    ASTNode* node = ast_create_node(AST_BOOLEAN);
    node->data.boolean.value = value;
    return node;
}

// 식별자
ASTNode* ast_create_identifier(const char* name) {
    ASTNode* node = ast_create_node(AST_IDENTIFIER);
    node->data.identifier.name = pergyra_strdup(name);
    return node;
}

// 타입
ASTNode* ast_create_type(const char* name) {
    ASTNode* node = ast_create_node(AST_TYPE);
    node->data.type.name = pergyra_strdup(name);
    node->data.type.generic_args = NULL;
    return node;
}

ASTNode* ast_create_async_function(const char* name, bool is_async) {
    ASTNode* node = ast_create_node(AST_FUNC_DECL);
    if (!node) return NULL;
    node->is_async_decl = is_async;

    /* AST_FUNC_DECL uses a shared union for sync/async declarations.
     * Initialize action-only fields explicitly so async funcs never
     * inherit stale subject-action metadata through overlapping storage. */
    node->data.func_decl.is_action = false;
    node->data.func_decl.required_abilities = NULL;
    node->data.func_decl.required_ability_count = 0;
    node->data.func_decl.within_zone = NULL;
    node->data.func_decl.causes_effect = NULL;
    node->data.func_decl.authorized_by = NULL;
    node->data.func_decl.authorized_by_count = 0;

    node->data.async_func_decl.name = pergyra_strdup(name);
    node->data.async_func_decl.params = NULL;
    node->data.async_func_decl.param_count = 0;
    node->data.async_func_decl.return_type = NULL;
    node->data.async_func_decl.body = NULL;
    node->data.async_func_decl.generic_params = NULL;
    node->data.async_func_decl.where_clause = NULL;
    node->data.async_func_decl.has_effects_clause = false;
    node->data.async_func_decl.declared_effects = 0;
    node->data.async_func_decl.access = ACCESS_PUBLIC;
    node->data.async_func_decl.is_async = is_async;
    node->data.async_func_decl.doc_comment = NULL;
    return node;
}

ASTNode* ast_create_actor(const char* name) {
    ASTNode* node = ast_create_node(AST_ACTOR_DECL);
    if (!node) return NULL;

    node->data.actor_decl.name = pergyra_strdup(name);
    node->data.actor_decl.fields = NULL;
    node->data.actor_decl.field_count = 0;
    node->data.actor_decl.methods = NULL;
    node->data.actor_decl.method_count = 0;
    node->data.actor_decl.generic_params = NULL;
    node->data.actor_decl.from_subject_profile_surface = false;
    node->data.actor_decl.doc_comment = NULL;
    return node;
}

ASTNode* ast_create_await_expression(ASTNode* expression) {
    ASTNode* node = ast_create_node(AST_AWAIT_EXPR);
    if (!node) return NULL;
    node->data.await_expr.expression = expression;
    return node;
}

ASTNode* ast_create_channel_send(ASTNode* channel, ASTNode* value) {
    ASTNode* node = ast_create_node(AST_CHANNEL_SEND);
    if (!node) return NULL;
    node->data.channel_send.channel = channel;
    node->data.channel_send.value = value;
    return node;
}

ASTNode* ast_create_channel_recv(ASTNode* channel) {
    ASTNode* node = ast_create_node(AST_CHANNEL_RECV);
    if (!node) return NULL;
    node->data.channel_recv.channel = channel;
    return node;
}

ASTNode* ast_create_select_statement(void) {
    ASTNode* node = ast_create_node(AST_SELECT_STMT);
    if (!node) return NULL;
    node->data.select_stmt.cases = NULL;
    node->data.select_stmt.case_count = 0;
    node->data.select_stmt.default_case = NULL;
    return node;
}

ASTNode* ast_create_async_block(void) {
    ASTNode* node = ast_create_node(AST_ASYNC_BLOCK);
    if (!node) return NULL;
    node->data.async_block.statements = NULL;
    node->data.async_block.statement_count = 0;
    return node;
}

ASTNode* ast_create_spawn_expression(ASTNode* function) {
    ASTNode* node = ast_create_node(AST_SPAWN_EXPR);
    if (!node) return NULL;
    node->data.spawn_expr.function = function;
    node->data.spawn_expr.arguments = NULL;
    node->data.spawn_expr.arg_count = 0;
    return node;
}

ASTNode* ast_create_channel_type(ASTNode* element_type) {
    ASTNode* node = ast_create_node(AST_CHANNEL_TYPE);
    if (!node) return NULL;
    node->data.channel_type.element_type = element_type;
    node->data.channel_type.capacity = NULL;
    return node;
}

ASTNode* ast_create_future_type(ASTNode* value_type) {
    ASTNode* node = ast_create_node(AST_FUTURE_TYPE);
    if (!node) return NULL;
    node->data.future_type.value_type = value_type;
    return node;
}

ASTNode* ast_create_task_group(bool wait_all) {
    ASTNode* node = ast_create_node(AST_TASK_GROUP);
    if (!node) return NULL;
    node->data.task_group.tasks = NULL;
    node->data.task_group.task_count = 0;
    node->data.task_group.wait_all = wait_all;
    return node;
}

// ============= AST 조작 함수들 =============

// 문장 추가 (프로그램/블록)
void ast_add_statement(ASTNode* parent, ASTNode* statement) {
    if (parent->type == AST_PROGRAM) {
        parent->data.program.count++;
        parent->data.program.statements = realloc(
            parent->data.program.statements,
            parent->data.program.count * sizeof(ASTNode*)
        );
        parent->data.program.statements[parent->data.program.count - 1] = statement;
    } else if (parent->type == AST_BLOCK) {
        parent->data.block.count++;
        parent->data.block.statements = realloc(
            parent->data.block.statements,
            parent->data.block.count * sizeof(ASTNode*)
        );
        parent->data.block.statements[parent->data.block.count - 1] = statement;
    } else if (parent->type == AST_EXTERN_BLOCK) {
        parent->data.extern_block.count++;
        parent->data.extern_block.declarations = realloc(
            parent->data.extern_block.declarations,
            parent->data.extern_block.count * sizeof(ASTNode*)
        );
        parent->data.extern_block.declarations[parent->data.extern_block.count - 1] = statement;
    } else if (parent->type == AST_NAMESPACE_DECL) {
        parent->data.namespace_decl.count++;
        parent->data.namespace_decl.statements = realloc(
            parent->data.namespace_decl.statements,
            parent->data.namespace_decl.count * sizeof(ASTNode*)
        );
        parent->data.namespace_decl.statements[parent->data.namespace_decl.count - 1] = statement;
    }
}

// parallel 태스크 추가
void ast_add_parallel_task(ASTNode* parallel, ASTNode* task) {
    if (parallel->type != AST_PARALLEL_BLOCK) return;
    
    parallel->data.parallel.task_count++;
    parallel->data.parallel.tasks = realloc(
        parallel->data.parallel.tasks,
        parallel->data.parallel.task_count * sizeof(ASTNode*)
    );
    parallel->data.parallel.tasks[parallel->data.parallel.task_count - 1] = task;
}

// 함수 인자 추가
void ast_add_argument(ASTNode* call, ASTNode* arg) {
    if (call->type != AST_CALL) return;
    
    call->data.call.arg_count++;
    call->data.call.arguments = realloc(
        call->data.call.arguments,
        call->data.call.arg_count * sizeof(ASTNode*)
    );
    call->data.call.arguments[call->data.call.arg_count - 1] = arg;
}

// ============= AST 메모리 해제 =============

static void
ast_destroy_generic_params(GenericParams* params) {
    if (params == NULL) return;

    for (size_t i = 0; i < params->count; i++) {
        GenericParam* param = params->params[i];
        if (param == NULL) continue;
        free(param->name);
        ast_destroy(param->constraint);
        ast_destroy(param->default_type);
        free(param);
    }

    free(params->params);
    free(params);
}

static void
ast_destroy_where_clause(WhereClause* clause) {
    if (clause == NULL) return;

    for (size_t i = 0; i < clause->count; i++) {
        TypeConstraint* constraint = clause->constraints[i];
        if (constraint == NULL) continue;
        free(constraint->type_param);
        for (size_t j = 0; j < constraint->bound_count; j++) {
            ast_destroy(constraint->bounds[j]);
        }
        free(constraint->bounds);
        free(constraint);
    }

    free(clause->constraints);
    free(clause);
}

void
ast_destroy_structured_comment(StructuredComment* comment) {
    while (comment != NULL) {
        StructuredComment* next = comment->next;
        for (size_t i = 0; i < comment->tag_count; i++) {
            if (comment->tags[i] == NULL) continue;
            free(comment->tags[i]->content);
            free(comment->tags[i]);
        }
        free(comment->tags);
        free(comment);
        comment = next;
    }
}

void ast_destroy(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_PROGRAM:
            for (size_t i = 0; i < node->data.program.count; i++) {
                ast_destroy(node->data.program.statements[i]);
            }
            free(node->data.program.statements);
            break;
            
        case AST_FUNC_DECL:
            free(node->data.func_decl.name);
            for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
                free(node->data.func_decl.params[i]->name);
                ast_destroy(node->data.func_decl.params[i]->type);
                ast_destroy(node->data.func_decl.params[i]->default_value);
                free(node->data.func_decl.params[i]);
            }
            free(node->data.func_decl.params);
            ast_destroy(node->data.func_decl.return_type);
            ast_destroy(node->data.func_decl.body);
            ast_destroy_generic_params(node->data.func_decl.generic_params);
            ast_destroy_where_clause(node->data.func_decl.where_clause);
            for (size_t i = 0; i < node->data.func_decl.required_ability_count; i++)
                free(node->data.func_decl.required_abilities[i]);
            free(node->data.func_decl.required_abilities);
            free(node->data.func_decl.within_zone);
            free(node->data.func_decl.causes_effect);
            for (size_t i = 0; i < node->data.func_decl.authorized_by_count; i++)
                free(node->data.func_decl.authorized_by[i]);
            free(node->data.func_decl.authorized_by);
            ast_destroy_structured_comment(node->data.func_decl.doc_comment);
            break;
            
        case AST_CLASS_DECL:
            free(node->data.class_decl.name);
            for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
                free(node->data.class_decl.fields[i]->name);
                ast_destroy(node->data.class_decl.fields[i]->type);
                free(node->data.class_decl.fields[i]);
            }
            free(node->data.class_decl.fields);
            for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
                ast_destroy(node->data.class_decl.methods[i]);
            }
            free(node->data.class_decl.methods);
            ast_destroy_generic_params(node->data.class_decl.generic_params);
            ast_destroy_where_clause(node->data.class_decl.where_clause);
            ast_destroy_structured_comment(node->data.class_decl.doc_comment);
            break;

        case AST_EXTERN_BLOCK:
            free(node->data.extern_block.abi);
            for (size_t i = 0; i < node->data.extern_block.count; i++) {
                ast_destroy(node->data.extern_block.declarations[i]);
            }
            free(node->data.extern_block.declarations);
            break;
            
        case AST_LET_DECL:
            free(node->data.let_decl.name);
            ast_destroy(node->data.let_decl.type);
            ast_destroy(node->data.let_decl.initializer);
            break;

        case AST_LET_DESTRUCTURE:
            for (size_t i = 0; i < node->data.let_destructure.name_count; i++)
                free(node->data.let_destructure.names[i]);
            free(node->data.let_destructure.names);
            ast_destroy(node->data.let_destructure.initializer);
            break;

        case AST_WITH_STMT:
            ast_destroy(node->data.with_stmt.slot_type);
            free(node->data.with_stmt.alias);
            ast_destroy(node->data.with_stmt.body);
            free(node->data.with_stmt.security_level);
            break;
            
        case AST_PARALLEL_BLOCK:
            for (size_t i = 0; i < node->data.parallel.task_count; i++) {
                ast_destroy(node->data.parallel.tasks[i]);
            }
            free(node->data.parallel.tasks);
            break;
            
        case AST_BLOCK:
            for (size_t i = 0; i < node->data.block.count; i++) {
                ast_destroy(node->data.block.statements[i]);
            }
            free(node->data.block.statements);
            break;
            
        case AST_FOR_LOOP:
            free(node->data.for_loop.variable);
            ast_destroy(node->data.for_loop.range_start);
            ast_destroy(node->data.for_loop.range_end);
            ast_destroy(node->data.for_loop.iterable);
            ast_destroy(node->data.for_loop.body);
            break;
            
        case AST_WHILE_LOOP:
            ast_destroy(node->data.while_loop.condition);
            ast_destroy(node->data.while_loop.body);
            break;

        case AST_MATCH_STMT:
            ast_destroy(node->data.match_stmt.subject);
            for (size_t i = 0; i < node->data.match_stmt.case_count; i++)
                ast_destroy(node->data.match_stmt.cases[i]);
            free(node->data.match_stmt.cases);
            ast_destroy(node->data.match_stmt.default_body);
            break;

        case AST_MATCH_CASE:
            ast_destroy(node->data.match_case.pattern);
            ast_destroy(node->data.match_case.guard);
            ast_destroy(node->data.match_case.body);
            break;

        case AST_IF_STMT:
            ast_destroy(node->data.if_stmt.condition);
            ast_destroy(node->data.if_stmt.then_branch);
            ast_destroy(node->data.if_stmt.else_branch);
            break;
            
        case AST_RETURN:
            ast_destroy(node->data.return_stmt.value);
            break;
            
        case AST_BINARY:
            ast_destroy(node->data.binary.left);
            ast_destroy(node->data.binary.right);
            break;
            
        case AST_UNARY:
            ast_destroy(node->data.unary.operand);
            break;
            
        case AST_CALL:
            ast_destroy(node->data.call.callee);
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                ast_destroy(node->data.call.arguments[i]);
            }
            free(node->data.call.arguments);
            break;
            
        case AST_MEMBER_ACCESS:
            ast_destroy(node->data.member.object);
            free(node->data.member.name);
            break;
            
        case AST_ARRAY_ACCESS:
            ast_destroy(node->data.array_access.array);
            ast_destroy(node->data.array_access.index);
            break;
            
        case AST_ASSIGNMENT:
            ast_destroy(node->data.assignment.target);
            ast_destroy(node->data.assignment.value);
            break;
            
        case AST_STRING:
            free(node->data.string.value);
            break;
            
        case AST_IDENTIFIER:
            free(node->data.identifier.name);
            break;
            
        case AST_TYPE:
            free(node->data.type.name);
            ast_destroy_generic_params(node->data.type.generic_args);
            break;

        case AST_ASYNC_BLOCK:
            for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
                ast_destroy(node->data.async_block.statements[i]);
            }
            free(node->data.async_block.statements);
            break;

        case AST_ACTOR_DECL:
            free(node->data.actor_decl.name);
            for (size_t i = 0; i < node->data.actor_decl.field_count; i++) {
                free(node->data.actor_decl.fields[i]->name);
                ast_destroy(node->data.actor_decl.fields[i]->type);
                free(node->data.actor_decl.fields[i]);
            }
            free(node->data.actor_decl.fields);
            for (size_t i = 0; i < node->data.actor_decl.method_count; i++) {
                ast_destroy(node->data.actor_decl.methods[i]);
            }
            free(node->data.actor_decl.methods);
            ast_destroy_generic_params(node->data.actor_decl.generic_params);
            ast_destroy_structured_comment(node->data.actor_decl.doc_comment);
            break;

        case AST_AWAIT_EXPR:
            ast_destroy(node->data.await_expr.expression);
            break;

        case AST_CHANNEL_SEND:
            ast_destroy(node->data.channel_send.channel);
            ast_destroy(node->data.channel_send.value);
            break;

        case AST_CHANNEL_RECV:
            ast_destroy(node->data.channel_recv.channel);
            break;

        case AST_SELECT_STMT:
            for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
                ast_destroy(node->data.select_stmt.cases[i]);
            }
            free(node->data.select_stmt.cases);
            ast_destroy(node->data.select_stmt.default_case);
            break;

        case AST_SPAWN_EXPR:
            ast_destroy(node->data.spawn_expr.function);
            for (size_t i = 0; i < node->data.spawn_expr.arg_count; i++) {
                ast_destroy(node->data.spawn_expr.arguments[i]);
            }
            free(node->data.spawn_expr.arguments);
            break;

        case AST_CHANNEL_TYPE:
            ast_destroy(node->data.channel_type.element_type);
            ast_destroy(node->data.channel_type.capacity);
            break;

        case AST_FUTURE_TYPE:
            ast_destroy(node->data.future_type.value_type);
            break;

        case AST_TASK_GROUP:
            for (size_t i = 0; i < node->data.task_group.task_count; i++) {
                ast_destroy(node->data.task_group.tasks[i]);
            }
            free(node->data.task_group.tasks);
            break;

        case AST_SYSTEMIC_DECL:
            free(node->data.systemic_decl.name);
            for (size_t i = 0; i < node->data.systemic_decl.party_count; i++)
                ast_destroy(node->data.systemic_decl.party_slots[i]);
            free(node->data.systemic_decl.party_slots);
            for (size_t i = 0; i < node->data.systemic_decl.shared_count; i++)
                ast_destroy(node->data.systemic_decl.shared_fields[i]);
            free(node->data.systemic_decl.shared_fields);
            for (size_t i = 0; i < node->data.systemic_decl.method_count; i++)
                ast_destroy(node->data.systemic_decl.methods[i]);
            free(node->data.systemic_decl.methods);
            ast_destroy_generic_params(node->data.systemic_decl.generic_params);
            ast_destroy_structured_comment(node->data.systemic_decl.doc_comment);
            break;

        case AST_SYSTEMIC_SLOT:
            free(node->data.systemic_slot.slot_name);
            free(node->data.systemic_slot.party_type);
            break;

        case AST_WORLD_DECL:
            free(node->data.world_decl.name);
            for (size_t i = 0; i < node->data.world_decl.systemic_count; i++)
                ast_destroy(node->data.world_decl.systemics[i]);
            free(node->data.world_decl.systemics);
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
            free(node->data.world_systemic.slot_name);
            free(node->data.world_systemic.systemic_type);
            ast_destroy(node->data.world_systemic.initializer);
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
            free(node->data.zone_apply.actor_slot_name);
            break;

        case AST_ZONE_LINK:
            free(node->data.zone_link.relation_slot_name);
            free(node->data.zone_link.left_slot_name);
            free(node->data.zone_link.right_slot_name);
            free(node->data.zone_link.state_name);
            free(node->data.zone_link.actor_slot_name);
            break;

        case AST_ZONE_DETACH:
            free(node->data.zone_detach.effect_slot_name);
            free(node->data.zone_detach.target_slot_name);
            free(node->data.zone_detach.state_name);
            free(node->data.zone_detach.actor_slot_name);
            break;

        case AST_ZONE_UNLINK:
            free(node->data.zone_unlink.relation_slot_name);
            free(node->data.zone_unlink.left_slot_name);
            free(node->data.zone_unlink.right_slot_name);
            free(node->data.zone_unlink.state_name);
            free(node->data.zone_unlink.actor_slot_name);
            break;

        case AST_ZONE_REFRESH:
            free(node->data.zone_refresh.object_slot_name);
            free(node->data.zone_refresh.source_slot_name);
            free(node->data.zone_refresh.actor_slot_name);
            break;

        case AST_ZONE_MAINTAIN_EFFECT:
            free(node->data.zone_maintain_effect.effect_slot_name);
            free(node->data.zone_maintain_effect.target_slot_name);
            free(node->data.zone_maintain_effect.actor_slot_name);
            break;

        case AST_ZONE_MAINTAIN_RELATION:
            free(node->data.zone_maintain_relation.relation_slot_name);
            free(node->data.zone_maintain_relation.left_slot_name);
            free(node->data.zone_maintain_relation.right_slot_name);
            free(node->data.zone_maintain_relation.actor_slot_name);
            break;

        case AST_ZONE_MAINTAIN_STATE:
            free(node->data.zone_maintain_state.state_name);
            free(node->data.zone_maintain_state.actor_slot_name);
            break;

        case AST_ZONE_AUTHORITY:
            free(node->data.zone_authority.subject_slot_name);
            for (size_t i = 0; i < node->data.zone_authority.ability_count; i++)
                free(node->data.zone_authority.required_abilities[i]);
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
            free(node->data.impl_ability.ability_name);
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

        case AST_LAMBDA_EXPR:
            for (size_t i = 0; i < node->data.lambda_expr.param_count; i++)
                ast_destroy(node->data.lambda_expr.params[i]);
            free(node->data.lambda_expr.params);
            ast_destroy(node->data.lambda_expr.body);
            ast_destroy(node->data.lambda_expr.return_type);
            break;

        case AST_IMPORT_DECL:
            free(node->data.import_decl.path);
            break;

        case AST_NAMESPACE_DECL:
            free(node->data.namespace_decl.name);
            for (size_t i = 0; i < node->data.namespace_decl.count; i++) {
                ast_destroy(node->data.namespace_decl.statements[i]);
            }
            free(node->data.namespace_decl.statements);
            break;

        case AST_UNSAFE_BLOCK:
            ast_destroy(node->data.unsafe_block.body);
            break;

        case AST_DEFER_STMT:
            ast_destroy(node->data.defer_stmt.body);
            break;

        case AST_BIND_STMT:
            free(node->data.bind_stmt.party_var);
            free(node->data.bind_stmt.slot_name);
            free(node->data.bind_stmt.role_name);
            break;

        default:
            break;
    }

    free(node);
}

// ============= AST 출력 (디버깅용) =============

static void print_indent(int level) {
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
}

static const char* ast_operator_to_string(TokenType type) {
    switch (type) {
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_EQUAL: return "==";
        case TOKEN_NOT_EQUAL: return "!=";
        case TOKEN_LESS: return "<";
        case TOKEN_LESS_EQUAL: return "<=";
        case TOKEN_GREATER: return ">";
        case TOKEN_GREATER_EQUAL: return ">=";
        case TOKEN_AND: return "&&";
        case TOKEN_OR: return "||";
        case TOKEN_NOT: return "!";
        case TOKEN_ASSIGN: return "=";
        default: return "?";
    }
}

static void ast_print_inline(ASTNode* node);
static void print_generic_params_inline(GenericParams* params);
static void print_where_clause_inline(WhereClause* clause);

static void
ast_print_compact(ASTNode* node)
{
    if (node == NULL) {
        printf("(null)");
        return;
    }

    switch (node->type) {
        case AST_IDENTIFIER:
            printf("%s", node->data.identifier.name);
            break;

        case AST_NUMBER:
            printf("%g", node->data.number.value);
            break;

        case AST_STRING:
            printf("\"%s\"", node->data.string.value);
            break;

        case AST_BOOLEAN:
            printf("%s", node->data.boolean.value ? "true" : "false");
            break;

        case AST_TYPE:
            printf("%s", node->data.type.name);
            if (node->data.type.generic_args)
                print_generic_params_inline(node->data.type.generic_args);
            break;

        case AST_CHANNEL_TYPE:
            printf("Channel<");
            ast_print_compact(node->data.channel_type.element_type);
            printf(">");
            if (node->data.channel_type.capacity != NULL) {
                printf("[");
                ast_print_compact(node->data.channel_type.capacity);
                printf("]");
            }
            break;

        case AST_FUTURE_TYPE:
            printf("Future<");
            ast_print_compact(node->data.future_type.value_type);
            printf(">");
            break;

        case AST_CALL:
            ast_print_compact(node->data.call.callee);
            printf("(");
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_compact(node->data.call.arguments[i]);
            }
            printf(")");
            break;

        case AST_BINARY:
            printf("(");
            ast_print_compact(node->data.binary.left);
            printf(" %s ", ast_operator_to_string(node->data.binary.op.type));
            ast_print_compact(node->data.binary.right);
            printf(")");
            break;

        case AST_UNARY:
            printf("(%s", ast_operator_to_string(node->data.unary.op.type));
            ast_print_compact(node->data.unary.operand);
            printf(")");
            break;

        case AST_MEMBER_ACCESS:
            ast_print_compact(node->data.member.object);
            printf(".%s", node->data.member.name);
            break;

        case AST_ARRAY_ACCESS:
            ast_print_compact(node->data.array_access.array);
            printf("[");
            ast_print_compact(node->data.array_access.index);
            printf("]");
            break;

        case AST_ASSIGNMENT:
            ast_print_compact(node->data.assignment.target);
            printf(" = ");
            ast_print_compact(node->data.assignment.value);
            break;

        case AST_AWAIT_EXPR:
            printf("await ");
            ast_print_compact(node->data.await_expr.expression);
            break;

        case AST_CHANNEL_SEND:
            ast_print_compact(node->data.channel_send.channel);
            printf(" <- ");
            ast_print_compact(node->data.channel_send.value);
            break;

        case AST_CHANNEL_RECV:
            printf("<-");
            ast_print_compact(node->data.channel_recv.channel);
            break;

        case AST_SPAWN_EXPR:
            printf("spawn ");
            ast_print_compact(node->data.spawn_expr.function);
            break;

        case AST_CONTEXT_ACCESS:
            printf("%s(%s",
                   node->data.context_access.method_name,
                   node->data.context_access.role_slot_name);
            if (node->data.context_access.ability_type != NULL) {
                printf(", ");
                ast_print_compact(node->data.context_access.ability_type);
            }
            printf(")");
            break;

        case AST_EVENT_INVOKE:
            ast_print_compact(node->data.event_invoke.event);
            printf("(");
            for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_compact(node->data.event_invoke.arguments[i]);
            }
            printf(")");
            break;

        case AST_EVENT_HANDLER_TYPE:
            printf("EventHandler(");
            for (size_t i = 0; i < node->data.event_handler_type.param_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_compact(node->data.event_handler_type.param_types[i]);
            }
            printf(")");
            if (node->data.event_handler_type.return_type != NULL) {
                printf(" -> ");
                ast_print_compact(node->data.event_handler_type.return_type);
            }
            break;

        case AST_REQUIRE_FIELD:
            printf("%s", node->data.require_field.name);
            if (node->data.require_field.type != NULL) {
                printf(": ");
                ast_print_compact(node->data.require_field.type);
            }
            break;

        case AST_ROLE_SLOT:
            printf("%s", node->data.role_slot.slot_name);
            if (node->data.role_slot.is_array)
                printf("[]");
            break;

        case AST_PARTY_SHARED:
            printf("%s", node->data.party_shared.name);
            if (node->data.party_shared.type != NULL) {
                printf(": ");
                ast_print_compact(node->data.party_shared.type);
            }
            if (node->data.party_shared.initializer != NULL) {
                printf(" = ");
                ast_print_compact(node->data.party_shared.initializer);
            }
            break;

        case AST_LET_DECL:
            printf("let %s", node->data.let_decl.name);
            if (node->data.let_decl.type != NULL) {
                printf(": ");
                ast_print_compact(node->data.let_decl.type);
            }
            if (node->data.let_decl.initializer != NULL) {
                printf(" = ");
                ast_print_compact(node->data.let_decl.initializer);
            }
            break;

        case AST_RETURN:
            printf("return");
            if (node->data.return_stmt.value != NULL) {
                printf(" ");
                ast_print_compact(node->data.return_stmt.value);
            }
            break;

        case AST_BLOCK:
            printf("{...}");
            break;

        case AST_PARALLEL_BLOCK:
            printf("parallel {...}");
            break;

        case AST_MATCH_CASE:
            printf("case ");
            ast_print_compact(node->data.match_case.pattern);
            if (node->data.match_case.guard != NULL) {
                printf(" if ");
                ast_print_compact(node->data.match_case.guard);
            }
            break;

        case AST_MATCH_STMT:
            printf("match ");
            ast_print_compact(node->data.match_stmt.subject);
            printf(" {...}");
            break;

        case AST_FOR_LOOP:
            printf("for %s in ", node->data.for_loop.variable);
            ast_print_compact(node->data.for_loop.range_start);
            printf("..");
            ast_print_compact(node->data.for_loop.range_end);
            break;

        case AST_WHILE_LOOP:
            printf("while ");
            ast_print_compact(node->data.while_loop.condition);
            break;

        case AST_IF_STMT:
            printf("if ");
            ast_print_compact(node->data.if_stmt.condition);
            break;

        case AST_PARTY_INSTANCE:
            printf("%s{...}", node->data.party_instance.party_type);
            break;

        case AST_LAMBDA_EXPR:
            printf("%slambda(", node->data.lambda_expr.is_async ? "async " : "");
            for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_compact(node->data.lambda_expr.params[i]);
            }
            printf(")");
            if (node->data.lambda_expr.return_type != NULL) {
                printf(" -> ");
                ast_print_compact(node->data.lambda_expr.return_type);
            }
            break;

        default:
            printf("<node:%d>", node->type);
            break;
    }
}

static void
ast_print_inline(ASTNode* node)
{
    ast_print_compact(node);
}

static void
print_generic_params_inline(GenericParams* params)
{
    if (params == NULL || params->count == 0) {
        return;
    }

    printf("<");
    for (size_t i = 0; i < params->count; i++) {
        GenericParam* param = params->params[i];
        if (i > 0)
            printf(", ");
        if (param == NULL) {
            printf("?");
            continue;
        }
        printf("%s", param->name != NULL ? param->name : "?");
        if (param->constraint != NULL) {
            printf(": ");
            ast_print_inline(param->constraint);
        }
        if (param->default_type != NULL) {
            printf(" = ");
            ast_print_inline(param->default_type);
        }
    }
    printf(">");
}

static void
print_where_clause_inline(WhereClause* clause)
{
    if (clause == NULL || clause->count == 0)
        return;

    printf(" where ");
    for (size_t i = 0; i < clause->count; i++) {
        TypeConstraint* constraint = clause->constraints[i];
        if (i > 0)
            printf(", ");
        if (constraint == NULL) {
            printf("?");
            continue;
        }

        printf("%s", constraint->type_param != NULL ? constraint->type_param : "?");
        if (constraint->bound_count > 0) {
            printf(": ");
            for (size_t j = 0; j < constraint->bound_count; j++) {
                if (j > 0)
                    printf(" + ");
                ast_print_inline(constraint->bounds[j]);
            }
        }
    }
}

static void
print_func_params(FuncParam** params, size_t count, int indent)
{
    print_indent(indent);
    printf("Parameters:\n");
    for (size_t i = 0; i < count; i++) {
        FuncParam* param = params[i];
        print_indent(indent + 1);
        if (param == NULL) {
            printf("?\n");
            continue;
        }
        printf("%s", param->name != NULL ? param->name : "?");
        if (param->type != NULL) {
            printf(": ");
            ast_print_inline(param->type);
        }
        if (param->default_value != NULL) {
            printf(" = ");
            ast_print_inline(param->default_value);
        }
        printf("\n");
    }
}

static bool
ast_print_needs_trailing_newline(ASTNodeType type)
{
    switch (type) {
        case AST_IDENTIFIER:
        case AST_NUMBER:
        case AST_STRING:
        case AST_BOOLEAN:
        case AST_TYPE:
        case AST_CHANNEL_TYPE:
        case AST_FUTURE_TYPE:
        case AST_CALL:
        case AST_BINARY:
        case AST_UNARY:
        case AST_MEMBER_ACCESS:
        case AST_ARRAY_ACCESS:
        case AST_AWAIT_EXPR:
        case AST_SPAWN_EXPR:
        case AST_EVENT_HANDLER_TYPE:
            return true;
        default:
            return false;
    }
}

void ast_print(ASTNode* node, int indent) {
    if (!node) {
        print_indent(indent);
        printf("(null)\n");
        return;
    }
    
    print_indent(indent);
    if (node->is_exported)
        printf("[export] ");
    
    switch (node->type) {
        case AST_PROGRAM:
            printf("Program:\n");
            for (size_t i = 0; i < node->data.program.count; i++) {
                ast_print(node->data.program.statements[i], indent + 1);
            }
            break;
            
        case AST_FUNC_DECL:
            printf("%s: %s\n",
                   (!node->is_async_decl && node->data.func_decl.is_action)
                       ? "Action" : "Function",
                   node->data.func_decl.name);
            if (node->data.func_decl.generic_params) {
                print_indent(indent + 1);
                printf("Generic params: ");
                print_generic_params_inline(node->data.func_decl.generic_params);
                printf("\n");
            }
            if (node->data.func_decl.where_clause) {
                print_indent(indent + 1);
                printf("Constraints:");
                print_where_clause_inline(node->data.func_decl.where_clause);
                printf("\n");
            }
            print_func_params(node->data.func_decl.params,
                              node->data.func_decl.param_count,
                              indent + 1);
            if (node->data.func_decl.return_type) {
                print_indent(indent + 1);
                printf("Returns: ");
                ast_print_inline(node->data.func_decl.return_type);
                printf("\n");
            }
            if (!node->is_async_decl
                && node->data.func_decl.required_ability_count > 0) {
                print_indent(indent + 1);
                printf("Requires:");
                for (size_t i = 0; i < node->data.func_decl.required_ability_count; i++) {
                    printf("%s%s", i == 0 ? " " : ", ",
                           node->data.func_decl.required_abilities[i]);
                }
                printf("\n");
            }
            if (!node->is_async_decl
                && node->data.func_decl.within_zone != NULL) {
                print_indent(indent + 1);
                printf("Within: %s\n", node->data.func_decl.within_zone);
            }
            if (!node->is_async_decl
                && node->data.func_decl.causes_effect != NULL) {
                print_indent(indent + 1);
                printf("Causes: %s\n", node->data.func_decl.causes_effect);
            }
            if (!node->is_async_decl
                && node->data.func_decl.authorized_by_count > 0) {
                print_indent(indent + 1);
                printf("Authorized by:");
                for (size_t i = 0; i < node->data.func_decl.authorized_by_count; i++) {
                    printf("%s%s", i == 0 ? " " : ", ",
                           node->data.func_decl.authorized_by[i]);
                }
                printf("\n");
            }
            if (node->data.func_decl.body) {
                print_indent(indent + 1);
                printf("Body:\n");
                ast_print(node->data.func_decl.body, indent + 2);
            }
            break;

        case AST_CLASS_DECL:
            printf("%s: %s\n",
                nominal_decl_kind_name(node->data.class_decl.nominal_kind),
                node->data.class_decl.name);
            if (node->data.class_decl.generic_params) {
                print_indent(indent + 1);
                printf("Generic params: ");
                print_generic_params_inline(node->data.class_decl.generic_params);
                printf("\n");
            }
            if (node->data.class_decl.where_clause) {
                print_indent(indent + 1);
                printf("Constraints:");
                print_where_clause_inline(node->data.class_decl.where_clause);
                printf("\n");
            }
            if (node->data.class_decl.field_count > 0) {
                print_indent(indent + 1);
                printf("Fields:\n");
                for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
                    print_indent(indent + 2);
                    printf("%s: ", node->data.class_decl.fields[i]->name);
                    ast_print_inline(node->data.class_decl.fields[i]->type);
                    printf("\n");
                }
            }
            if (node->data.class_decl.method_count > 0) {
                print_indent(indent + 1);
                printf("Methods:\n");
                for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
                    ast_print(node->data.class_decl.methods[i], indent + 2);
                }
            }
            break;

        case AST_EXTERN_BLOCK:
            printf("Extern Block (%s):\n", node->data.extern_block.abi);
            for (size_t i = 0; i < node->data.extern_block.count; i++) {
                ast_print(node->data.extern_block.declarations[i], indent + 1);
            }
            break;
            
        case AST_LET_DECL:
            printf("Let: %s", node->data.let_decl.name);
            if (node->data.let_decl.type) {
                printf(" : ");
                ast_print_inline(node->data.let_decl.type);
            }
            printf(" = ");
            ast_print_inline(node->data.let_decl.initializer);
            printf("\n");
            break;
            
        case AST_WITH_STMT:
            printf("With %s<", node->data.with_stmt.is_secure ? "SecureSlot" : "slot");
            ast_print_inline(node->data.with_stmt.slot_type);
            printf("> as %s", node->data.with_stmt.alias);
            if (node->data.with_stmt.security_level != NULL) {
                printf(" [security=%s]", node->data.with_stmt.security_level);
            }
            printf("\n");
            ast_print(node->data.with_stmt.body, indent + 1);
            break;
            
        case AST_PARALLEL_BLOCK:
            printf("Parallel:\n");
            for (size_t i = 0; i < node->data.parallel.task_count; i++) {
                ast_print(node->data.parallel.tasks[i], indent + 1);
            }
            break;
            
        case AST_IDENTIFIER:
            printf("%s", node->data.identifier.name);
            break;
            
        case AST_NUMBER:
            printf("%g", node->data.number.value);
            break;
            
        case AST_STRING:
            printf("\"%s\"", node->data.string.value);
            break;
            
        case AST_BOOLEAN:
            printf("%s", node->data.boolean.value ? "true" : "false");
            break;
            
        case AST_TYPE:
            printf("%s", node->data.type.name);
            if (node->data.type.generic_args) {
                print_generic_params_inline(node->data.type.generic_args);
            }
            break;

        case AST_CALL:
            ast_print_inline(node->data.call.callee);
            printf("(");
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (i > 0) printf(", ");
                ast_print_inline(node->data.call.arguments[i]);
            }
            printf(")");
            break;
            
        case AST_BINARY:
            printf("(");
            ast_print_inline(node->data.binary.left);
            printf(" %s ", ast_operator_to_string(node->data.binary.op.type));
            ast_print_inline(node->data.binary.right);
            printf(")");
            break;

        case AST_BLOCK:
            printf("Block:\n");
            for (size_t i = 0; i < node->data.block.count; i++) {
                ast_print(node->data.block.statements[i], indent + 1);
            }
            break;

        case AST_FOR_LOOP:
            printf("For: %s in ", node->data.for_loop.variable);
            ast_print_inline(node->data.for_loop.range_start);
            printf("..");
            ast_print_inline(node->data.for_loop.range_end);
            printf("\n");
            ast_print(node->data.for_loop.body, indent + 1);
            break;

        case AST_WHILE_LOOP:
            printf("While: ");
            ast_print_inline(node->data.while_loop.condition);
            printf("\n");
            ast_print(node->data.while_loop.body, indent + 1);
            break;

        case AST_IF_STMT:
            printf("If: ");
            ast_print_inline(node->data.if_stmt.condition);
            printf("\n");
            print_indent(indent + 1);
            printf("Then:\n");
            ast_print(node->data.if_stmt.then_branch, indent + 2);
            if (node->data.if_stmt.else_branch != NULL) {
                print_indent(indent + 1);
                printf("Else:\n");
                ast_print(node->data.if_stmt.else_branch, indent + 2);
            }
            break;

        case AST_RETURN:
            printf("Return");
            if (node->data.return_stmt.value != NULL) {
                printf(": ");
                ast_print_inline(node->data.return_stmt.value);
            }
            printf("\n");
            break;

        case AST_UNARY:
            printf("(%s", ast_operator_to_string(node->data.unary.op.type));
            ast_print_inline(node->data.unary.operand);
            printf(")");
            break;

        case AST_MEMBER_ACCESS:
            ast_print_inline(node->data.member.object);
            printf(".%s", node->data.member.name);
            break;

        case AST_ARRAY_ACCESS:
            ast_print_inline(node->data.array_access.array);
            printf("[");
            ast_print_inline(node->data.array_access.index);
            printf("]");
            break;

        case AST_ASSIGNMENT:
            printf("Assign: ");
            ast_print_inline(node->data.assignment.target);
            printf(" = ");
            ast_print_inline(node->data.assignment.value);
            printf("\n");
            break;

        case AST_AWAIT_EXPR:
            printf("await ");
            ast_print_inline(node->data.await_expr.expression);
            break;

        case AST_CHANNEL_SEND:
            printf("ChannelSend: ");
            ast_print_inline(node->data.channel_send.channel);
            printf(" <- ");
            ast_print_inline(node->data.channel_send.value);
            printf("\n");
            break;

        case AST_CHANNEL_RECV:
            printf("ChannelRecv: ");
            ast_print_inline(node->data.channel_recv.channel);
            printf("\n");
            break;

        case AST_SELECT_STMT:
            printf("Select:\n");
            for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
                ast_print(node->data.select_stmt.cases[i], indent + 1);
            }
            if (node->data.select_stmt.default_case != NULL) {
                print_indent(indent + 1);
                printf("Default:\n");
                ast_print(node->data.select_stmt.default_case, indent + 2);
            }
            break;

        case AST_MATCH_STMT:
            printf("Match: ");
            ast_print_inline(node->data.match_stmt.subject);
            printf("\n");
            for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
                ast_print(node->data.match_stmt.cases[i], indent + 1);
            }
            if (node->data.match_stmt.default_body != NULL) {
                print_indent(indent + 1);
                printf("Default:\n");
                ast_print(node->data.match_stmt.default_body, indent + 2);
            }
            break;

        case AST_MATCH_CASE:
            printf("Case: ");
            ast_print_inline(node->data.match_case.pattern);
            if (node->data.match_case.guard != NULL) {
                printf(" if ");
                ast_print_inline(node->data.match_case.guard);
            }
            printf("\n");
            ast_print(node->data.match_case.body, indent + 1);
            break;

        case AST_CHANNEL_TYPE:
            printf("Channel<");
            ast_print_inline(node->data.channel_type.element_type);
            printf(">");
            if (node->data.channel_type.capacity != NULL) {
                printf("[");
                ast_print_inline(node->data.channel_type.capacity);
                printf("]");
            }
            break;

        case AST_FUTURE_TYPE:
            printf("Future<");
            ast_print_inline(node->data.future_type.value_type);
            printf(">");
            break;

        case AST_ASYNC_BLOCK:
            printf("AsyncBlock:\n");
            for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
                ast_print(node->data.async_block.statements[i], indent + 1);
            }
            break;

        case AST_SPAWN_EXPR:
            printf("spawn ");
            ast_print_inline(node->data.spawn_expr.function);
            break;

        case AST_TASK_GROUP:
            printf("TaskGroup (%s):\n", node->data.task_group.wait_all ? "all" : "any");
            for (size_t i = 0; i < node->data.task_group.task_count; i++) {
                ast_print(node->data.task_group.tasks[i], indent + 1);
            }
            break;

        case AST_ABILITY_DECL:
            printf("Ability: %s\n", node->data.ability_decl.name);
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
                print_indent(indent + 1);
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
            printf("Impl ability: %s\n", node->data.impl_ability.ability_name);
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
                print_indent(indent + 1);
                printf("%s = ",
                       node->data.party_instance.assignments[i].slot_name);
                ast_print_inline(node->data.party_instance.assignments[i].value);
                printf("\n");
            }
            break;

        case AST_SYSTEMIC_DECL:
            printf("Systemic: %s", node->data.systemic_decl.name);
            print_generic_params_inline(node->data.systemic_decl.generic_params);
            printf("\n");
            for (size_t i = 0; i < node->data.systemic_decl.party_count; i++) {
                ast_print(node->data.systemic_decl.party_slots[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.systemic_decl.shared_count; i++) {
                ast_print(node->data.systemic_decl.shared_fields[i], indent + 1);
            }
            for (size_t i = 0; i < node->data.systemic_decl.method_count; i++) {
                ast_print(node->data.systemic_decl.methods[i], indent + 1);
            }
            break;

        case AST_SYSTEMIC_SLOT:
            printf("SystemicSlot: %s: %s", node->data.systemic_slot.slot_name,
                   node->data.systemic_slot.party_type);
            if (node->data.systemic_slot.is_array)
                printf("[]");
            printf("\n");
            break;

        case AST_WORLD_DECL:
            printf("World: %s\n", node->data.world_decl.name);
            for (size_t i = 0; i < node->data.world_decl.systemic_count; i++) {
                ast_print(node->data.world_decl.systemics[i], indent + 1);
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
                   node->data.world_systemic.slot_name,
                   node->data.world_systemic.systemic_type);
            if (node->data.world_systemic.initializer != NULL) {
                printf(" = ");
                ast_print_inline(node->data.world_systemic.initializer);
            }
            printf("\n");
            break;

        case AST_WORLD_ZONE:
            printf("WorldZone: %s: %s",
                   node->data.world_zone.slot_name,
                   node->data.world_zone.zone_type);
            if (node->data.world_zone.initializer != NULL) {
                printf(" = ");
                ast_print_inline(node->data.world_zone.initializer);
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
            printf("Relation: %s\n", node->data.relation_decl.name);
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
                   : (node->data.domain_slot.is_dto ? "Dto" : "Object"),
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
            printf("%sSlot: %s: %s\n",
                   node->data.zone_layer_slot.is_relation ? "Relation" : "Effect",
                   node->data.zone_layer_slot.slot_name,
                   node->data.zone_layer_slot.layer_type);
            break;

        case AST_ZONE_APPLY:
            if (node->data.zone_apply.state_name != NULL) {
                printf("ApplyState: %s", node->data.zone_apply.state_name);
            } else {
                printf("Apply: %s -> %s",
                       node->data.zone_apply.effect_slot_name,
                       node->data.zone_apply.target_slot_name);
            }
            if (node->data.zone_apply.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_apply.actor_slot_name);
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
            if (node->data.zone_link.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_link.actor_slot_name);
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
            if (node->data.zone_detach.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_detach.actor_slot_name);
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
            if (node->data.zone_unlink.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_unlink.actor_slot_name);
            printf("\n");
            break;

        case AST_ZONE_REFRESH:
            printf("%s: %s from %s",
                   node->data.zone_refresh.requires_dto ? "Publish" : "Refresh",
                   node->data.zone_refresh.object_slot_name,
                   node->data.zone_refresh.source_slot_name);
            if (node->data.zone_refresh.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_refresh.actor_slot_name);
            printf("\n");
            break;

        case AST_ZONE_MAINTAIN_EFFECT:
            printf("MaintainEffect: %s on %s",
                   node->data.zone_maintain_effect.effect_slot_name,
                   node->data.zone_maintain_effect.target_slot_name);
            if (node->data.zone_maintain_effect.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_maintain_effect.actor_slot_name);
            printf("\n");
            break;

        case AST_ZONE_MAINTAIN_RELATION:
            printf("MaintainRelation: %s between %s, %s",
                   node->data.zone_maintain_relation.relation_slot_name,
                   node->data.zone_maintain_relation.left_slot_name,
                   node->data.zone_maintain_relation.right_slot_name);
            if (node->data.zone_maintain_relation.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_maintain_relation.actor_slot_name);
            printf("\n");
            break;

        case AST_ZONE_MAINTAIN_STATE:
            printf("MaintainState: %s",
                   node->data.zone_maintain_state.state_name);
            if (node->data.zone_maintain_state.actor_slot_name != NULL)
                printf(" by %s", node->data.zone_maintain_state.actor_slot_name);
            printf("\n");
            break;

        case AST_ZONE_AUTHORITY:
            printf("Authority: %s\n",
                   node->data.zone_authority.subject_slot_name);
            if (node->data.zone_authority.ability_count > 0) {
                print_indent(indent + 1);
                printf("Requires:");
                for (size_t i = 0; i < node->data.zone_authority.ability_count; i++) {
                    printf("%s%s",
                           i == 0 ? " " : ", ",
                           node->data.zone_authority.required_abilities[i]);
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

        case AST_ACTOR_DECL:
            printf("Actor: %s\n", node->data.actor_decl.name);
            if (node->data.actor_decl.field_count > 0) {
                print_indent(indent + 1);
                printf("Fields:\n");
                for (size_t i = 0; i < node->data.actor_decl.field_count; i++) {
                    print_indent(indent + 2);
                    printf("%s: ", node->data.actor_decl.fields[i]->name);
                    ast_print_inline(node->data.actor_decl.fields[i]->type);
                    printf("\n");
                }
            }
            for (size_t i = 0; i < node->data.actor_decl.method_count; i++) {
                ast_print(node->data.actor_decl.methods[i], indent + 1);
            }
            break;

        case AST_EVENT_DECL:
            printf("Event: %s\n", node->data.event_decl.name);
            if (node->data.event_decl.param_count > 0) {
                print_indent(indent + 1);
                printf("Parameters:\n");
                for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
                    ast_print(node->data.event_decl.params[i], indent + 2);
                }
            }
            if (node->data.event_decl.return_type != NULL) {
                print_indent(indent + 1);
                printf("Returns: ");
                ast_print_inline(node->data.event_decl.return_type);
                printf("\n");
            }
            break;

        case AST_EVENT_SUBSCRIBE:
            printf("EventSubscribe: ");
            ast_print_inline(node->data.event_op.event);
            printf(" += ");
            ast_print_inline(node->data.event_op.handler);
            printf("\n");
            break;

        case AST_EVENT_UNSUBSCRIBE:
            printf("EventUnsubscribe: ");
            ast_print_inline(node->data.event_op.event);
            printf(" -= ");
            ast_print_inline(node->data.event_op.handler);
            printf("\n");
            break;

        case AST_EVENT_INVOKE:
            printf("EventInvoke: ");
            ast_print_inline(node->data.event_invoke.event);
            printf("(");
            for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_inline(node->data.event_invoke.arguments[i]);
            }
            printf(")\n");
            break;

        case AST_EVENT_HANDLER_TYPE:
            printf("EventHandler(");
            for (size_t i = 0; i < node->data.event_handler_type.param_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_inline(node->data.event_handler_type.param_types[i]);
            }
            printf(")");
            if (node->data.event_handler_type.return_type != NULL) {
                printf(" -> ");
                ast_print_inline(node->data.event_handler_type.return_type);
            }
            break;

        case AST_LAMBDA_EXPR:
            printf("%slambda(", node->data.lambda_expr.is_async ? "async " : "");
            for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
                if (i > 0)
                    printf(", ");
                ast_print_inline(node->data.lambda_expr.params[i]);
            }
            printf(")");
            if (node->data.lambda_expr.return_type != NULL) {
                printf(" -> ");
                ast_print_inline(node->data.lambda_expr.return_type);
            }
            printf("\n");
            ast_print(node->data.lambda_expr.body, indent + 1);
            break;

        case AST_IMPORT_DECL:
            printf("Import: \"%s\"\n", node->data.import_decl.path);
            break;

        case AST_NAMESPACE_DECL:
            printf("Namespace: %s\n", node->data.namespace_decl.name);
            for (size_t i = 0; i < node->data.namespace_decl.count; i++) {
                ast_print(node->data.namespace_decl.statements[i], indent + 1);
            }
            break;
            
        default:
            printf("AST node type %d\n", node->type);
            break;
    }
    
    if (indent == 0 || ast_print_needs_trailing_newline(node->type))
        printf("\n");
}
