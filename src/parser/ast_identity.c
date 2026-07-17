/*
 * Copyright (c) 2026 Pergyra Language Project
 * AST stable identity assignment.
 */

#include "ast.h"

#include <stdint.h>

typedef struct {
    uint64_t next_id;
    bool exhausted;
} AstIdentityState;

static void ast_assign_node(ASTNode *node, AstIdentityState *next_id);

static uint32_t
ast_take_stable_id(AstIdentityState *next_id)
{
    if (next_id == NULL || next_id->exhausted)
        return 0;
    if (next_id->next_id == 0 || next_id->next_id > UINT32_MAX) {
        next_id->exhausted = true;
        return 0;
    }
    return (uint32_t)next_id->next_id++;
}

static void
ast_assign_array(ASTNode **nodes, size_t count, AstIdentityState *next_id)
{
    for (size_t i = 0; i < count; i++)
        ast_assign_node(nodes != NULL ? nodes[i] : NULL, next_id);
}

static void
ast_assign_generic_params(GenericParams *params, AstIdentityState *next_id)
{
    if (params == NULL)
        return;
    for (size_t i = 0; i < params->count; i++) {
        GenericParam *param = params->params != NULL ? params->params[i] : NULL;
        if (param == NULL)
            continue;
        ast_assign_node(param->constraint, next_id);
        ast_assign_node(param->default_type, next_id);
    }
}

static void
ast_assign_where_clause(WhereClause *where, AstIdentityState *next_id)
{
    if (where == NULL)
        return;
    for (size_t i = 0; i < where->count; i++) {
        TypeConstraint *constraint =
            where->constraints != NULL ? where->constraints[i] : NULL;
        if (constraint == NULL)
            continue;
        ast_assign_array(constraint->bounds, constraint->bound_count, next_id);
    }
}

static void
ast_assign_params(FuncParam **params, size_t count, AstIdentityState *next_id)
{
    for (size_t i = 0; i < count; i++) {
        FuncParam *param = params != NULL ? params[i] : NULL;
        if (param == NULL)
            continue;
        ast_assign_node(param->type, next_id);
        ast_assign_node(param->default_value, next_id);
    }
}

static void
ast_assign_fields(ClassField **fields, size_t count, AstIdentityState *next_id)
{
    for (size_t i = 0; i < count; i++) {
        ClassField *field = fields != NULL ? fields[i] : NULL;
        if (field == NULL)
            continue;
        field->stable_id = ast_take_stable_id(next_id);
        ast_assign_node(field->type, next_id);
    }
}

static void
ast_assign_enum_variant_params(ASTNode *node, AstIdentityState *next_id)
{
    if (node == NULL || node->type != AST_ENUM_DECL)
        return;
    for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
        ASTNode **params = node->data.enum_decl.variant_params != NULL
            ? node->data.enum_decl.variant_params[i]
            : NULL;
        size_t count = node->data.enum_decl.variant_param_counts != NULL
            ? node->data.enum_decl.variant_param_counts[i]
            : 0;
        ast_assign_array(params, count, next_id);
    }
}

static void
ast_assign_decl_methods(ASTNode *node, AstIdentityState *next_id)
{
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (node == NULL)
        return;

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
        return;
    }

    ast_assign_array(methods, method_count, next_id);
}

static void
ast_assign_node(ASTNode *node, AstIdentityState *next_id)
{
    if (node == NULL)
        return;

    node->stable_id = ast_take_stable_id(next_id);
    if (next_id == NULL || next_id->exhausted)
        return;

    switch (node->type) {
    case AST_PROGRAM:
        ast_assign_array(node->data.program.statements,
                         node->data.program.count, next_id);
        break;
    case AST_BLOCK:
        ast_assign_array(node->data.block.statements,
                         node->data.block.count, next_id);
        break;
    case AST_FUNC_DECL:
        ast_assign_params(node->data.func_decl.params,
                          node->data.func_decl.param_count, next_id);
        ast_assign_node(node->data.func_decl.return_type, next_id);
        ast_assign_node(node->data.func_decl.body, next_id);
        ast_assign_generic_params(node->data.func_decl.generic_params, next_id);
        ast_assign_where_clause(node->data.func_decl.where_clause, next_id);
        ast_assign_array(node->data.func_decl.required_abilities,
                         node->data.func_decl.required_ability_count, next_id);
        break;
    case AST_CLASS_DECL:
        ast_assign_fields(node->data.class_decl.fields,
                          node->data.class_decl.field_count, next_id);
        ast_assign_array(node->data.class_decl.field_destructures,
                         node->data.class_decl.field_destructure_count,
                         next_id);
        ast_assign_decl_methods(node, next_id);
        ast_assign_generic_params(node->data.class_decl.generic_params, next_id);
        ast_assign_where_clause(node->data.class_decl.where_clause, next_id);
        break;
    case AST_EXTERN_BLOCK:
        ast_assign_array(node->data.extern_block.declarations,
                         node->data.extern_block.count, next_id);
        break;
    case AST_LET_DECL:
        ast_assign_node(node->data.let_decl.type, next_id);
        ast_assign_node(node->data.let_decl.initializer, next_id);
        break;
    case AST_LET_DESTRUCTURE:
        ast_assign_node(node->data.let_destructure.initializer, next_id);
        break;
    case AST_TYPE_ALIAS:
        ast_assign_node(node->data.type_alias.target_type, next_id);
        break;
    case AST_WITH_STMT:
        ast_assign_node(node->data.with_stmt.slot_type, next_id);
        ast_assign_node(node->data.with_stmt.body, next_id);
        break;
    case AST_PARALLEL_BLOCK:
        ast_assign_array(node->data.parallel.tasks,
                         node->data.parallel.task_count, next_id);
        ast_assign_node(node->data.parallel.join_collection, next_id);
        ast_assign_node(node->data.parallel.join_range_end, next_id);
        break;
    case AST_FOR_LOOP:
        ast_assign_node(node->data.for_loop.range_start, next_id);
        ast_assign_node(node->data.for_loop.range_end, next_id);
        ast_assign_node(node->data.for_loop.iterable, next_id);
        ast_assign_node(node->data.for_loop.body, next_id);
        break;
    case AST_WHILE_LOOP:
        ast_assign_node(node->data.while_loop.condition, next_id);
        ast_assign_node(node->data.while_loop.body, next_id);
        break;
    case AST_IF_STMT:
        ast_assign_node(node->data.if_stmt.condition, next_id);
        ast_assign_node(node->data.if_stmt.then_branch, next_id);
        ast_assign_node(node->data.if_stmt.else_branch, next_id);
        break;
    case AST_RETURN:
        ast_assign_node(node->data.return_stmt.value, next_id);
        break;
    case AST_GIVE_STMT:
        ast_assign_node(node->data.give_stmt.value, next_id);
        break;
    case AST_ENUM_DECL:
        ast_assign_enum_variant_params(node, next_id);
        ast_assign_decl_methods(node, next_id);
        break;
    case AST_SELECT_STMT:
        ast_assign_array(node->data.select_stmt.cases,
                         node->data.select_stmt.case_count, next_id);
        ast_assign_node(node->data.select_stmt.default_case, next_id);
        break;
    case AST_MATCH_STMT:
        ast_assign_node(node->data.match_stmt.subject, next_id);
        ast_assign_array(node->data.match_stmt.cases,
                         node->data.match_stmt.case_count, next_id);
        ast_assign_node(node->data.match_stmt.default_body, next_id);
        break;
    case AST_MATCH_CASE:
        ast_assign_node(node->data.match_case.pattern, next_id);
        ast_assign_array(node->data.match_case.patterns,
                         node->data.match_case.pattern_count, next_id);
        ast_assign_node(node->data.match_case.guard, next_id);
        ast_assign_node(node->data.match_case.body, next_id);
        break;
    case AST_BINARY:
        ast_assign_node(node->data.binary.left, next_id);
        ast_assign_node(node->data.binary.right, next_id);
        break;
    case AST_UNARY:
        ast_assign_node(node->data.unary.operand, next_id);
        break;
    case AST_CALL:
        ast_assign_node(node->data.call.callee, next_id);
        ast_assign_array(node->data.call.arguments,
                         node->data.call.arg_count, next_id);
        ast_assign_generic_params(node->data.call.generic_args, next_id);
        break;
    case AST_MEMBER_ACCESS:
        ast_assign_node(node->data.member.object, next_id);
        break;
    case AST_ARRAY_ACCESS:
        ast_assign_node(node->data.array_access.array, next_id);
        ast_assign_node(node->data.array_access.index, next_id);
        break;
    case AST_ARRAY_LITERAL:
        ast_assign_array(node->data.array_literal.elements,
                         node->data.array_literal.count, next_id);
        break;
    case AST_TUPLE_LITERAL:
        ast_assign_array(node->data.tuple_literal.elements,
                         node->data.tuple_literal.count, next_id);
        break;
    case AST_MAP_LITERAL:
        ast_assign_array(node->data.map_literal.keys,
                         node->data.map_literal.count, next_id);
        ast_assign_array(node->data.map_literal.values,
                         node->data.map_literal.count, next_id);
        break;
    case AST_CAST:
        ast_assign_node(node->data.cast.operand, next_id);
        break;
    case AST_TYPE_TEST:
        ast_assign_node(node->data.type_test.operand, next_id);
        break;
    case AST_ASSIGNMENT:
        ast_assign_node(node->data.assignment.target, next_id);
        ast_assign_node(node->data.assignment.value, next_id);
        break;
    case AST_TYPE:
        ast_assign_generic_params(node->data.type.generic_args, next_id);
        ast_assign_array(node->data.type.tuple_elements,
                         node->data.type.tuple_element_count, next_id);
        break;
    case AST_ASYNC_BLOCK:
        ast_assign_array(node->data.async_block.statements,
                         node->data.async_block.statement_count, next_id);
        break;
    case AST_AWAIT_EXPR:
        ast_assign_node(node->data.await_expr.expression, next_id);
        break;
    case AST_CHANNEL_SEND:
        ast_assign_node(node->data.channel_send.channel, next_id);
        ast_assign_node(node->data.channel_send.value, next_id);
        break;
    case AST_CHANNEL_RECV:
        ast_assign_node(node->data.channel_recv.channel, next_id);
        break;
    case AST_CHANNEL_TYPE:
        ast_assign_node(node->data.channel_type.element_type, next_id);
        ast_assign_node(node->data.channel_type.capacity, next_id);
        break;
    case AST_FUTURE_TYPE:
        ast_assign_node(node->data.future_type.value_type, next_id);
        break;
    case AST_SPAWN_EXPR:
        ast_assign_node(node->data.spawn_expr.function, next_id);
        ast_assign_array(node->data.spawn_expr.arguments,
                         node->data.spawn_expr.arg_count, next_id);
        break;
    case AST_ABILITY_DECL:
        ast_assign_array(node->data.ability_decl.require_fields,
                         node->data.ability_decl.require_count, next_id);
        ast_assign_decl_methods(node, next_id);
        ast_assign_generic_params(node->data.ability_decl.generic_params, next_id);
        ast_assign_where_clause(node->data.ability_decl.where_clause, next_id);
        break;
    case AST_ROLE_DECL:
        ast_assign_node(node->data.role_decl.for_type, next_id);
        ast_assign_array(node->data.role_decl.includes,
                         node->data.role_decl.include_count, next_id);
        ast_assign_array(node->data.role_decl.impl_abilities,
                         node->data.role_decl.impl_count, next_id);
        ast_assign_node(node->data.role_decl.parallel_block, next_id);
        ast_assign_generic_params(node->data.role_decl.generic_params, next_id);
        ast_assign_where_clause(node->data.role_decl.where_clause, next_id);
        break;
    case AST_INCLUDE_STMT:
        ast_assign_generic_params(node->data.include_stmt.type_args, next_id);
        break;
    case AST_REQUIRE_FIELD:
        ast_assign_node(node->data.require_field.type, next_id);
        break;
    case AST_IMPL_ABILITY:
        ast_assign_node(node->data.impl_ability.ability_ref, next_id);
        ast_assign_decl_methods(node, next_id);
        break;
    case AST_OVERRIDE_FUNC:
        ast_assign_node(node->data.override_func.func_decl, next_id);
        break;
    case AST_PARTY_DECL:
        ast_assign_array(node->data.party_decl.role_slots,
                         node->data.party_decl.role_count, next_id);
        ast_assign_array(node->data.party_decl.shared_fields,
                         node->data.party_decl.shared_count, next_id);
        ast_assign_decl_methods(node, next_id);
        ast_assign_node(node->data.party_decl.extends, next_id);
        ast_assign_generic_params(node->data.party_decl.generic_params, next_id);
        break;
    case AST_ROLE_SLOT:
        ast_assign_array(node->data.role_slot.required_abilities,
                         node->data.role_slot.ability_count, next_id);
        break;
    case AST_PARTY_SHARED:
        ast_assign_node(node->data.party_shared.type, next_id);
        ast_assign_node(node->data.party_shared.initializer, next_id);
        break;
    case AST_CONTEXT_ACCESS:
        ast_assign_node(node->data.context_access.ability_type, next_id);
        break;
    case AST_PARTY_INSTANCE:
        for (size_t i = 0; i < node->data.party_instance.assignment_count; i++)
            ast_assign_node(node->data.party_instance.assignments[i].value, next_id);
        break;
    case AST_ROSTER_DECL:
        ast_assign_array(node->data.roster_decl.party_slots,
                         node->data.roster_decl.party_count, next_id);
        ast_assign_array(node->data.roster_decl.shared_fields,
                         node->data.roster_decl.shared_count, next_id);
        ast_assign_decl_methods(node, next_id);
        ast_assign_generic_params(node->data.roster_decl.generic_params, next_id);
        break;
    case AST_WORLD_DECL:
        ast_assign_array(node->data.world_decl.rosters,
                         node->data.world_decl.roster_count, next_id);
        ast_assign_array(node->data.world_decl.zones,
                         node->data.world_decl.zone_count, next_id);
        ast_assign_array(node->data.world_decl.shared_fields,
                         node->data.world_decl.shared_count, next_id);
        ast_assign_decl_methods(node, next_id);
        ast_assign_array(node->data.world_decl.activations,
                         node->data.world_decl.activate_count, next_id);
        ast_assign_array(node->data.world_decl.deactivations,
                         node->data.world_decl.deactivate_count, next_id);
        ast_assign_array(node->data.world_decl.maintained_zones,
                         node->data.world_decl.maintained_zone_count, next_id);
        ast_assign_array(node->data.world_decl.states,
                         node->data.world_decl.state_count, next_id);
        break;
    case AST_WORLD_SYSTEMIC:
        ast_assign_node(node->data.world_roster.initializer, next_id);
        break;
    case AST_WORLD_ZONE:
        ast_assign_node(node->data.world_zone.initializer, next_id);
        break;
    case AST_INTENT_DECL:
        ast_assign_array(node->data.intent_decl.involves,
                         node->data.intent_decl.involve_count, next_id);
        ast_assign_array(node->data.intent_decl.values,
                         node->data.intent_decl.value_count, next_id);
        ast_assign_array(node->data.intent_decl.bindings,
                         node->data.intent_decl.binding_count, next_id);
        ast_assign_array(node->data.intent_decl.steps,
                         node->data.intent_decl.step_count, next_id);
        ast_assign_node(node->data.intent_decl.priority_expr, next_id);
        ast_assign_node(node->data.intent_decl.success_expr, next_id);
        ast_assign_node(node->data.intent_decl.failure_expr, next_id);
        ast_assign_node(node->data.intent_decl.default_where_type, next_id);
        break;
    case AST_INTENT_INVOLVES:
        ast_assign_node(node->data.intent_involves.subject_type, next_id);
        break;
    case AST_INTENT_VALUE:
        ast_assign_node(node->data.intent_value.value_type, next_id);
        break;
    case AST_INTENT_STEP:
        ast_assign_node(node->data.intent_step.where_type, next_id);
        ast_assign_node(node->data.intent_step.using_expr, next_id);
        ast_assign_node(node->data.intent_step.intent_expr, next_id);
        ast_assign_array(node->data.intent_step.on_exprs,
                         node->data.intent_step.on_expr_count, next_id);
        ast_assign_array(node->data.intent_step.compensate_exprs,
                         node->data.intent_step.compensate_expr_count, next_id);
        ast_assign_node(node->data.intent_step.pre_expr, next_id);
        ast_assign_node(node->data.intent_step.guard_expr, next_id);
        ast_assign_node(node->data.intent_step.post_expr, next_id);
        ast_assign_node(node->data.intent_step.invariant_expr, next_id);
        ast_assign_array(node->data.intent_step.required_abilities,
                         node->data.intent_step.required_ability_count, next_id);
        ast_assign_node(node->data.intent_step.expect_expr, next_id);
        break;
    case AST_RELATION_DECL:
        ast_assign_array(node->data.relation_decl.slots,
                         node->data.relation_decl.slot_count, next_id);
        ast_assign_array(node->data.relation_decl.refreshes,
                         node->data.relation_decl.refresh_count, next_id);
        ast_assign_array(node->data.relation_decl.shared_fields,
                         node->data.relation_decl.shared_count, next_id);
        ast_assign_decl_methods(node, next_id);
        ast_assign_node(node->data.relation_decl.between_left_type, next_id);
        ast_assign_node(node->data.relation_decl.between_right_type, next_id);
        break;
    case AST_EFFECT_DECL:
        ast_assign_array(node->data.effect_decl.slots,
                         node->data.effect_decl.slot_count, next_id);
        ast_assign_array(node->data.effect_decl.refreshes,
                         node->data.effect_decl.refresh_count, next_id);
        ast_assign_array(node->data.effect_decl.shared_fields,
                         node->data.effect_decl.shared_count, next_id);
        ast_assign_decl_methods(node, next_id);
        break;
    case AST_ZONE_DECL:
        ast_assign_array(node->data.zone_decl.slots,
                         node->data.zone_decl.slot_count, next_id);
        ast_assign_array(node->data.zone_decl.layer_slots,
                         node->data.zone_decl.layer_slot_count, next_id);
        ast_assign_array(node->data.zone_decl.applies,
                         node->data.zone_decl.apply_count, next_id);
        ast_assign_array(node->data.zone_decl.links,
                         node->data.zone_decl.link_count, next_id);
        ast_assign_array(node->data.zone_decl.detaches,
                         node->data.zone_decl.detach_count, next_id);
        ast_assign_array(node->data.zone_decl.unlinks,
                         node->data.zone_decl.unlink_count, next_id);
        ast_assign_array(node->data.zone_decl.refreshes,
                         node->data.zone_decl.refresh_count, next_id);
        ast_assign_array(node->data.zone_decl.maintained_effects,
                         node->data.zone_decl.maintained_effect_count, next_id);
        ast_assign_array(node->data.zone_decl.maintained_relations,
                         node->data.zone_decl.maintained_relation_count, next_id);
        ast_assign_array(node->data.zone_decl.maintained_states,
                         node->data.zone_decl.maintained_state_count, next_id);
        ast_assign_array(node->data.zone_decl.authorities,
                         node->data.zone_decl.authority_count, next_id);
        ast_assign_array(node->data.zone_decl.states,
                         node->data.zone_decl.state_count, next_id);
        ast_assign_array(node->data.zone_decl.shared_fields,
                         node->data.zone_decl.shared_count, next_id);
        ast_assign_decl_methods(node, next_id);
        break;
    case AST_DOMAIN_SLOT:
        ast_assign_node(node->data.domain_slot.type, next_id);
        ast_assign_node(node->data.domain_slot.initializer, next_id);
        break;
    case AST_ZONE_AUTHORITY:
        ast_assign_array(node->data.zone_authority.required_abilities,
                         node->data.zone_authority.ability_count, next_id);
        break;
    case AST_EVENT_DECL:
        ast_assign_array(node->data.event_decl.params,
                         node->data.event_decl.param_count, next_id);
        ast_assign_node(node->data.event_decl.return_type, next_id);
        break;
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        ast_assign_node(node->data.event_op.event, next_id);
        ast_assign_node(node->data.event_op.handler, next_id);
        break;
    case AST_EVENT_INVOKE:
        ast_assign_node(node->data.event_invoke.event, next_id);
        ast_assign_array(node->data.event_invoke.arguments,
                         node->data.event_invoke.arg_count, next_id);
        break;
    case AST_EVENT_HANDLER_TYPE:
        ast_assign_array(node->data.event_handler_type.param_types,
                         node->data.event_handler_type.param_count, next_id);
        ast_assign_node(node->data.event_handler_type.return_type, next_id);
        break;
    case AST_LAMBDA_EXPR:
        ast_assign_array(node->data.lambda_expr.params,
                         node->data.lambda_expr.param_count, next_id);
        ast_assign_node(node->data.lambda_expr.body, next_id);
        ast_assign_node(node->data.lambda_expr.return_type, next_id);
        break;
    case AST_NAMESPACE_DECL:
        ast_assign_array(node->data.namespace_decl.statements,
                         node->data.namespace_decl.count, next_id);
        break;
    case AST_UNSAFE_BLOCK:
        ast_assign_node(node->data.unsafe_block.body, next_id);
        break;
    case AST_TRANSACTION_BLOCK:
        ast_assign_node(node->data.transaction_block.body, next_id);
        break;
    case AST_FAIL_STMT:
        ast_assign_node(node->data.fail_stmt.reason, next_id);
        break;
    case AST_DEFER_STMT:
        ast_assign_node(node->data.defer_stmt.body, next_id);
        break;
    default:
        break;
    }
}

bool
ast_assign_stable_ids(ASTNode *root)
{
    AstIdentityState next_id = {
        .next_id = 1,
        .exhausted = false,
    };

    ast_assign_node(root, &next_id);
    return !next_id.exhausted;
}

uint32_t
ast_node_stable_id(const ASTNode *node)
{
    return node != NULL ? node->stable_id : 0;
}
