/*
 * Copyright (c) 2026 Pergyra Language Project
 * AST-owned structural analysis helpers.
 */

#include "ast_analysis.h"

#include "../common/intent_observability_abi.h"

#include <string.h>

static bool ast_array_contains_identifier_call(ASTNode *const *nodes,
                                               size_t count,
                                               ASTIdentifierPredicate predicate,
                                               void *userdata);

static bool
ast_params_contain_identifier_call(FuncParam *const *params,
                                   size_t count,
                                   ASTIdentifierPredicate predicate,
                                   void *userdata)
{
    for (size_t i = 0; i < count; i++) {
        if (params[i] == NULL)
            continue;
        if (ast_contains_identifier_call(params[i]->type, predicate, userdata)
            || ast_contains_identifier_call(params[i]->default_value, predicate, userdata)) {
            return true;
        }
    }
    return false;
}

static bool
ast_fields_contain_identifier_call(ClassField *const *fields,
                                   size_t count,
                                   ASTIdentifierPredicate predicate,
                                   void *userdata)
{
    for (size_t i = 0; i < count; i++) {
        if (fields[i] != NULL
            && ast_contains_identifier_call(fields[i]->type, predicate, userdata)) {
            return true;
        }
    }
    return false;
}

static bool
ast_decl_methods_contain_identifier_call(const ASTNode *node,
                                         ASTIdentifierPredicate predicate,
                                         void *userdata)
{
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (node == NULL)
        return false;

    switch (node->type) {
    case AST_CLASS_DECL:
        methods = node->data.class_decl.methods;
        method_count = node->data.class_decl.method_count;
        break;
    case AST_ENUM_DECL:
        methods = node->data.enum_decl.methods;
        method_count = node->data.enum_decl.method_count;
        break;
    case AST_ABILITY_DECL:
        methods = node->data.ability_decl.methods;
        method_count = node->data.ability_decl.method_count;
        break;
    case AST_IMPL_ABILITY:
        methods = node->data.impl_ability.methods;
        method_count = node->data.impl_ability.method_count;
        break;
    case AST_PARTY_DECL:
        methods = node->data.party_decl.methods;
        method_count = node->data.party_decl.method_count;
        break;
    case AST_ROSTER_DECL:
        methods = node->data.roster_decl.methods;
        method_count = node->data.roster_decl.method_count;
        break;
    case AST_WORLD_DECL:
        methods = node->data.world_decl.methods;
        method_count = node->data.world_decl.method_count;
        break;
    case AST_RELATION_DECL:
        methods = node->data.relation_decl.methods;
        method_count = node->data.relation_decl.method_count;
        break;
    case AST_EFFECT_DECL:
        methods = node->data.effect_decl.methods;
        method_count = node->data.effect_decl.method_count;
        break;
    case AST_ZONE_DECL:
        methods = node->data.zone_decl.methods;
        method_count = node->data.zone_decl.method_count;
        break;
    default:
        return false;
    }

    return ast_array_contains_identifier_call(methods, method_count, predicate, userdata);
}

static bool
ast_call_matches_identifier_predicate(const ASTNode *node,
                                      ASTIdentifierPredicate predicate,
                                      void *userdata)
{
    return node != NULL
        && node->type == AST_CALL
        && node->data.call.callee != NULL
        && node->data.call.callee->type == AST_IDENTIFIER
        && predicate != NULL
        && predicate(node->data.call.callee->data.identifier.name, userdata);
}

static bool
ast_array_contains_identifier_call(ASTNode *const *nodes,
                                   size_t count,
                                   ASTIdentifierPredicate predicate,
                                   void *userdata)
{
    for (size_t i = 0; i < count; i++) {
        if (ast_contains_identifier_call(nodes[i], predicate, userdata))
            return true;
    }
    return false;
}

bool
ast_contains_identifier_call(const ASTNode *node,
                             ASTIdentifierPredicate predicate,
                             void *userdata)
{
    if (node == NULL || predicate == NULL)
        return false;
    if (ast_call_matches_identifier_predicate(node, predicate, userdata))
        return true;

    switch (node->type) {
    case AST_PROGRAM:
        return ast_array_contains_identifier_call(
            node->data.program.statements, node->data.program.count, predicate, userdata);
    case AST_BLOCK:
        return ast_array_contains_identifier_call(
            node->data.block.statements, node->data.block.count, predicate, userdata);
    case AST_FUNC_DECL:
        return ast_params_contain_identifier_call(
                node->data.func_decl.params, node->data.func_decl.param_count, predicate, userdata)
            || ast_contains_identifier_call(node->data.func_decl.return_type, predicate, userdata)
            || ast_contains_identifier_call(node->data.func_decl.body, predicate, userdata);
    case AST_CLASS_DECL:
        return ast_fields_contain_identifier_call(
                node->data.class_decl.fields, node->data.class_decl.field_count, predicate, userdata)
            || ast_decl_methods_contain_identifier_call(node, predicate, userdata);
    case AST_EXTERN_BLOCK:
        return ast_array_contains_identifier_call(
            node->data.extern_block.declarations, node->data.extern_block.count, predicate, userdata);
    case AST_LET_DECL:
        return ast_contains_identifier_call(node->data.let_decl.type, predicate, userdata)
            || ast_contains_identifier_call(node->data.let_decl.initializer, predicate, userdata);
    case AST_LET_DESTRUCTURE:
        return ast_contains_identifier_call(node->data.let_destructure.initializer, predicate, userdata);
    case AST_TYPE_ALIAS:
        return ast_contains_identifier_call(node->data.type_alias.target_type, predicate, userdata);
    case AST_WITH_STMT:
        return ast_contains_identifier_call(node->data.with_stmt.slot_type, predicate, userdata)
            || ast_contains_identifier_call(node->data.with_stmt.body, predicate, userdata);
    case AST_PARALLEL_BLOCK:
        return ast_array_contains_identifier_call(
            node->data.parallel.tasks, node->data.parallel.task_count, predicate, userdata);
    case AST_FOR_LOOP:
        return ast_contains_identifier_call(node->data.for_loop.range_start, predicate, userdata)
            || ast_contains_identifier_call(node->data.for_loop.range_end, predicate, userdata)
            || ast_contains_identifier_call(node->data.for_loop.iterable, predicate, userdata)
            || ast_contains_identifier_call(node->data.for_loop.body, predicate, userdata);
    case AST_WHILE_LOOP:
        return ast_contains_identifier_call(node->data.while_loop.condition, predicate, userdata)
            || ast_contains_identifier_call(node->data.while_loop.body, predicate, userdata);
    case AST_IF_STMT:
        return ast_contains_identifier_call(node->data.if_stmt.condition, predicate, userdata)
            || ast_contains_identifier_call(node->data.if_stmt.then_branch, predicate, userdata)
            || ast_contains_identifier_call(node->data.if_stmt.else_branch, predicate, userdata);
    case AST_RETURN:
        return ast_contains_identifier_call(node->data.return_stmt.value, predicate, userdata);
    case AST_GIVE_STMT:
        return ast_contains_identifier_call(node->data.give_stmt.value, predicate, userdata);
    case AST_ENUM_DECL:
        return ast_decl_methods_contain_identifier_call(node, predicate, userdata);
    case AST_SELECT_STMT:
        return ast_array_contains_identifier_call(
                node->data.select_stmt.cases, node->data.select_stmt.case_count, predicate, userdata)
            || ast_contains_identifier_call(node->data.select_stmt.default_case, predicate, userdata);
    case AST_MATCH_STMT:
        return ast_contains_identifier_call(node->data.match_stmt.subject, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.match_stmt.cases, node->data.match_stmt.case_count, predicate, userdata)
            || ast_contains_identifier_call(node->data.match_stmt.default_body, predicate, userdata);
    case AST_MATCH_CASE:
        return ast_contains_identifier_call(node->data.match_case.pattern, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.match_case.patterns, node->data.match_case.pattern_count,
                predicate, userdata)
            || ast_contains_identifier_call(node->data.match_case.guard, predicate, userdata)
            || ast_contains_identifier_call(node->data.match_case.body, predicate, userdata);
    case AST_BINARY:
        return ast_contains_identifier_call(node->data.binary.left, predicate, userdata)
            || ast_contains_identifier_call(node->data.binary.right, predicate, userdata);
    case AST_UNARY:
        return ast_contains_identifier_call(node->data.unary.operand, predicate, userdata);
    case AST_CALL:
        return ast_contains_identifier_call(node->data.call.callee, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.call.arguments, node->data.call.arg_count, predicate, userdata);
    case AST_MEMBER_ACCESS:
        return ast_contains_identifier_call(node->data.member.object, predicate, userdata);
    case AST_ARRAY_ACCESS:
        return ast_contains_identifier_call(node->data.array_access.array, predicate, userdata)
            || ast_contains_identifier_call(node->data.array_access.index, predicate, userdata);
    case AST_ARRAY_LITERAL:
        return ast_array_contains_identifier_call(
            node->data.array_literal.elements, node->data.array_literal.count, predicate, userdata);
    case AST_TUPLE_LITERAL:
        return ast_array_contains_identifier_call(
            node->data.tuple_literal.elements, node->data.tuple_literal.count, predicate, userdata);
    case AST_MAP_LITERAL:
        return ast_array_contains_identifier_call(
            node->data.map_literal.keys, node->data.map_literal.count, predicate, userdata)
            || ast_array_contains_identifier_call(
            node->data.map_literal.values, node->data.map_literal.count, predicate, userdata);
    case AST_SET_LITERAL:
        return ast_array_contains_identifier_call(
            node->data.set_literal.elements, node->data.set_literal.count, predicate, userdata);
    case AST_CAST:
        return ast_contains_identifier_call(node->data.cast.operand, predicate, userdata);
    case AST_TYPE_TEST:
        return ast_contains_identifier_call(node->data.type_test.operand, predicate, userdata);
    case AST_ASSIGNMENT:
        return ast_contains_identifier_call(node->data.assignment.target, predicate, userdata)
            || ast_contains_identifier_call(node->data.assignment.value, predicate, userdata);
    case AST_TYPE:
        return ast_array_contains_identifier_call(
            node->data.type.tuple_elements, node->data.type.tuple_element_count, predicate, userdata);
    case AST_AWAIT_EXPR:
        return ast_contains_identifier_call(node->data.await_expr.expression, predicate, userdata);
    case AST_CHANNEL_SEND:
        return ast_contains_identifier_call(node->data.channel_send.channel, predicate, userdata)
            || ast_contains_identifier_call(node->data.channel_send.value, predicate, userdata);
    case AST_CHANNEL_RECV:
        return ast_contains_identifier_call(node->data.channel_recv.channel, predicate, userdata);
    case AST_CHANNEL_TYPE:
        return ast_contains_identifier_call(node->data.channel_type.element_type, predicate, userdata)
            || ast_contains_identifier_call(node->data.channel_type.capacity, predicate, userdata);
    case AST_FUTURE_TYPE:
        return ast_contains_identifier_call(node->data.future_type.value_type, predicate, userdata);
    case AST_ASYNC_BLOCK:
        return ast_array_contains_identifier_call(
            node->data.async_block.statements, node->data.async_block.statement_count,
            predicate, userdata);
    case AST_SPAWN_EXPR:
        return ast_contains_identifier_call(node->data.spawn_expr.function, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.spawn_expr.arguments, node->data.spawn_expr.arg_count, predicate, userdata);
    case AST_TASK_GROUP:
        return ast_array_contains_identifier_call(
            node->data.task_group.tasks, node->data.task_group.task_count, predicate, userdata);
    case AST_ABILITY_DECL:
        return ast_array_contains_identifier_call(
                node->data.ability_decl.require_fields, node->data.ability_decl.require_count,
                predicate, userdata)
            || ast_decl_methods_contain_identifier_call(node, predicate, userdata);
    case AST_ROLE_DECL:
        return ast_contains_identifier_call(node->data.role_decl.for_type, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.role_decl.includes, node->data.role_decl.include_count, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.role_decl.impl_abilities, node->data.role_decl.impl_count,
                predicate, userdata)
            || ast_contains_identifier_call(node->data.role_decl.parallel_block, predicate, userdata);
    case AST_REQUIRE_FIELD:
        return ast_contains_identifier_call(node->data.require_field.type, predicate, userdata);
    case AST_IMPL_ABILITY:
        return ast_contains_identifier_call(node->data.impl_ability.ability_ref, predicate, userdata)
            || ast_decl_methods_contain_identifier_call(node, predicate, userdata);
    case AST_OVERRIDE_FUNC:
        return ast_contains_identifier_call(node->data.override_func.func_decl, predicate, userdata);
    case AST_PARTY_DECL:
        return ast_array_contains_identifier_call(
                node->data.party_decl.role_slots, node->data.party_decl.role_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.party_decl.shared_fields, node->data.party_decl.shared_count,
                predicate, userdata)
            || ast_decl_methods_contain_identifier_call(node, predicate, userdata)
            || ast_contains_identifier_call(node->data.party_decl.extends, predicate, userdata);
    case AST_PARTY_SHARED:
        return ast_contains_identifier_call(node->data.party_shared.type, predicate, userdata)
            || ast_contains_identifier_call(node->data.party_shared.initializer, predicate, userdata);
    case AST_PARTY_INSTANCE:
        for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
            if (ast_contains_identifier_call(
                    node->data.party_instance.assignments[i].value, predicate, userdata)) {
                return true;
            }
        }
        return false;
    case AST_ROSTER_DECL:
        return ast_array_contains_identifier_call(
                node->data.roster_decl.party_slots, node->data.roster_decl.party_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.roster_decl.shared_fields, node->data.roster_decl.shared_count,
                predicate, userdata)
            || ast_decl_methods_contain_identifier_call(node, predicate, userdata);
    case AST_WORLD_DECL:
        return ast_array_contains_identifier_call(
                node->data.world_decl.rosters, node->data.world_decl.roster_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.world_decl.zones, node->data.world_decl.zone_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.world_decl.shared_fields, node->data.world_decl.shared_count,
                predicate, userdata)
            || ast_decl_methods_contain_identifier_call(node, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.world_decl.activations, node->data.world_decl.activate_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.world_decl.deactivations, node->data.world_decl.deactivate_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.world_decl.maintained_zones,
                node->data.world_decl.maintained_zone_count, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.world_decl.states, node->data.world_decl.state_count, predicate, userdata);
    case AST_WORLD_SYSTEMIC:
        return ast_contains_identifier_call(
            ast_world_roster_initializer(node), predicate, userdata);
    case AST_WORLD_ZONE:
        return ast_contains_identifier_call(
            ast_world_zone_initializer(node), predicate, userdata);
    case AST_INTENT_DECL:
        return ast_array_contains_identifier_call(
                node->data.intent_decl.involves, node->data.intent_decl.involve_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.intent_decl.values, node->data.intent_decl.value_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.intent_decl.bindings, node->data.intent_decl.binding_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.intent_decl.steps, node->data.intent_decl.step_count,
                predicate, userdata)
            || ast_contains_identifier_call(node->data.intent_decl.priority_expr, predicate, userdata)
            || ast_contains_identifier_call(node->data.intent_decl.success_expr, predicate, userdata)
            || ast_contains_identifier_call(node->data.intent_decl.failure_expr, predicate, userdata)
            || ast_contains_identifier_call(
                node->data.intent_decl.default_where_type, predicate, userdata);
    case AST_INTENT_INVOLVES:
        return ast_contains_identifier_call(node->data.intent_involves.subject_type, predicate, userdata);
    case AST_INTENT_VALUE:
        return ast_contains_identifier_call(node->data.intent_value.value_type, predicate, userdata);
    case AST_INTENT_STEP:
        return ast_contains_identifier_call(node->data.intent_step.where_type, predicate, userdata)
            || ast_contains_identifier_call(node->data.intent_step.using_expr, predicate, userdata)
            || ast_contains_identifier_call(node->data.intent_step.intent_expr, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.intent_step.on_exprs, node->data.intent_step.on_expr_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.intent_step.compensate_exprs,
                node->data.intent_step.compensate_expr_count, predicate, userdata)
            || ast_contains_identifier_call(node->data.intent_step.pre_expr, predicate, userdata)
            || ast_contains_identifier_call(node->data.intent_step.guard_expr, predicate, userdata)
            || ast_contains_identifier_call(node->data.intent_step.post_expr, predicate, userdata)
            || ast_contains_identifier_call(node->data.intent_step.invariant_expr, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.intent_step.required_abilities,
                node->data.intent_step.required_ability_count, predicate, userdata)
            || ast_contains_identifier_call(node->data.intent_step.expect_expr, predicate, userdata);
    case AST_RELATION_DECL:
        return ast_array_contains_identifier_call(
                node->data.relation_decl.slots, node->data.relation_decl.slot_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.relation_decl.refreshes, node->data.relation_decl.refresh_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.relation_decl.shared_fields, node->data.relation_decl.shared_count,
                predicate, userdata)
            || ast_decl_methods_contain_identifier_call(node, predicate, userdata)
            || ast_contains_identifier_call(
                node->data.relation_decl.between_left_type, predicate, userdata)
            || ast_contains_identifier_call(
                node->data.relation_decl.between_right_type, predicate, userdata);
    case AST_EFFECT_DECL:
        return ast_array_contains_identifier_call(
                node->data.effect_decl.slots, node->data.effect_decl.slot_count, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.effect_decl.refreshes, node->data.effect_decl.refresh_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.effect_decl.shared_fields, node->data.effect_decl.shared_count,
                predicate, userdata)
            || ast_decl_methods_contain_identifier_call(node, predicate, userdata);
    case AST_ZONE_DECL:
        return ast_array_contains_identifier_call(
                node->data.zone_decl.slots, node->data.zone_decl.slot_count, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.zone_decl.layer_slots, node->data.zone_decl.layer_slot_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.zone_decl.applies, node->data.zone_decl.apply_count, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.zone_decl.links, node->data.zone_decl.link_count, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.zone_decl.detaches, node->data.zone_decl.detach_count, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.zone_decl.unlinks, node->data.zone_decl.unlink_count, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.zone_decl.refreshes, node->data.zone_decl.refresh_count, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.zone_decl.maintained_effects,
                node->data.zone_decl.maintained_effect_count, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.zone_decl.maintained_relations,
                node->data.zone_decl.maintained_relation_count, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.zone_decl.maintained_states,
                node->data.zone_decl.maintained_state_count, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.zone_decl.authorities, node->data.zone_decl.authority_count,
                predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.zone_decl.states, node->data.zone_decl.state_count, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.zone_decl.shared_fields, node->data.zone_decl.shared_count,
                predicate, userdata)
            || ast_decl_methods_contain_identifier_call(node, predicate, userdata);
    case AST_DOMAIN_SLOT:
        return ast_contains_identifier_call(node->data.domain_slot.type, predicate, userdata)
            || ast_contains_identifier_call(node->data.domain_slot.initializer, predicate, userdata);
    case AST_ZONE_AUTHORITY:
        return ast_array_contains_identifier_call(
            node->data.zone_authority.required_abilities,
            node->data.zone_authority.ability_count, predicate, userdata);
    case AST_EVENT_DECL:
        return ast_array_contains_identifier_call(
                node->data.event_decl.params, node->data.event_decl.param_count,
                predicate, userdata)
            || ast_contains_identifier_call(node->data.event_decl.return_type, predicate, userdata);
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return ast_contains_identifier_call(node->data.event_op.event, predicate, userdata)
            || ast_contains_identifier_call(node->data.event_op.handler, predicate, userdata);
    case AST_EVENT_INVOKE:
        return ast_contains_identifier_call(node->data.event_invoke.event, predicate, userdata)
            || ast_array_contains_identifier_call(
                node->data.event_invoke.arguments, node->data.event_invoke.arg_count,
                predicate, userdata);
    case AST_EVENT_HANDLER_TYPE:
        return ast_array_contains_identifier_call(
                node->data.event_handler_type.param_types,
                node->data.event_handler_type.param_count, predicate, userdata)
            || ast_contains_identifier_call(node->data.event_handler_type.return_type,
                                           predicate, userdata);
    case AST_LAMBDA_EXPR:
        return ast_array_contains_identifier_call(
                node->data.lambda_expr.params, node->data.lambda_expr.param_count,
                predicate, userdata)
            || ast_contains_identifier_call(node->data.lambda_expr.body, predicate, userdata)
            || ast_contains_identifier_call(node->data.lambda_expr.return_type, predicate, userdata);
    case AST_NAMESPACE_DECL:
        return ast_array_contains_identifier_call(
            node->data.namespace_decl.statements, node->data.namespace_decl.count,
            predicate, userdata);
    case AST_UNSAFE_BLOCK:
        return ast_contains_identifier_call(node->data.unsafe_block.body, predicate, userdata);
    case AST_TRANSACTION_BLOCK:
        return ast_contains_identifier_call(node->data.transaction_block.body, predicate, userdata);
    case AST_FAIL_STMT:
        return ast_contains_identifier_call(node->data.fail_stmt.reason, predicate, userdata);
    case AST_DEFER_STMT:
        return ast_contains_identifier_call(node->data.defer_stmt.body, predicate, userdata);
    default:
        return false;
    }
}

static bool
ast_identifier_is_intent_surface(const char *name, void *userdata)
{
    (void)userdata;
    return pgy_intent_observability_name_is_builtin(name);
}

bool
ast_uses_intent_observability_surface(const ASTNode *node)
{
    return ast_contains_identifier_call(node, ast_identifier_is_intent_surface, NULL);
}
