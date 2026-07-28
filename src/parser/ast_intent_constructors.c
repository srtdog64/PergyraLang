/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST intent constructors.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

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
    node->data.intent_decl.retry_count = 0;
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
    node->data.intent_step.outcome_binding_name = NULL;
    node->data.intent_step.outcome_binding_length = 0;
    node->data.intent_step.outcome_binding_line = 0;
    node->data.intent_step.outcome_binding_column = 0;
    node->data.intent_step.outcome_binding_type_name = NULL;
    node->data.intent_step.outcome_action_decl_syntax_id = 0;
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
    node->data.intent_step.derived_who_from_on_receiver = false;
    node->data.intent_step.derived_who_from_single_participant = false;
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
    node->data.intent_step.derived_using_from_where = false;
    return node;
}
