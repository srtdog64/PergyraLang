/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "intent_observability_usage.h"
#include "transpiler_builtin_type_table.h"

#include "../compiler/mir.h"

static bool pgy_ast_uses_intent_observability(const ASTNode *node);

static bool
pgy_mir_symbol_uses_intent_observability(const char *name)
{
    return pgy_builtin_is_intent_observability(name);
}

static bool
pgy_mir_instruction_uses_intent_observability(const MIRInstruction *inst)
{
    if (inst == NULL)
        return false;

    if (pgy_mir_symbol_uses_intent_observability(inst->name)
        || pgy_mir_symbol_uses_intent_observability(inst->arg0)
        || pgy_mir_symbol_uses_intent_observability(inst->arg1)
        || pgy_mir_symbol_uses_intent_observability(inst->slot_anchor)
        || pgy_mir_symbol_uses_intent_observability(inst->result_name)) {
        return true;
    }

    /*
     * MIR_STMT still carries generic AST-backed statements for calls whose
     * callee has not been materialized as an instruction fact yet. Keep this
     * fallback until statement call facts are part of MIR lowering.
     */
    return pgy_ast_uses_intent_observability(inst->ast)
        || pgy_ast_uses_intent_observability(inst->expr0)
        || pgy_ast_uses_intent_observability(inst->expr1);
}

static bool
pgy_ast_array_uses_intent_observability(ASTNode *const *nodes, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (pgy_ast_uses_intent_observability(nodes[i]))
            return true;
    }
    return false;
}

static bool
pgy_params_use_intent_observability(FuncParam *const *params, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (params[i] == NULL)
            continue;
        if (pgy_ast_uses_intent_observability(params[i]->type)
            || pgy_ast_uses_intent_observability(params[i]->default_value)) {
            return true;
        }
    }
    return false;
}

static bool
pgy_fields_use_intent_observability(ClassField *const *fields, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (fields[i] != NULL
            && pgy_ast_uses_intent_observability(fields[i]->type)) {
            return true;
        }
    }
    return false;
}

static bool
pgy_decl_methods_use_intent_observability(const ASTNode *node)
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

    return pgy_ast_array_uses_intent_observability(methods, method_count);
}

static bool
pgy_ast_uses_intent_observability(const ASTNode *node)
{
    if (node == NULL)
        return false;

    switch (node->type) {
    case AST_PROGRAM:
        return pgy_ast_array_uses_intent_observability(
            node->data.program.statements, node->data.program.count);
    case AST_BLOCK:
        return pgy_ast_array_uses_intent_observability(
            node->data.block.statements, node->data.block.count);
    case AST_FUNC_DECL:
        return pgy_params_use_intent_observability(
                node->data.func_decl.params, node->data.func_decl.param_count)
            || pgy_ast_uses_intent_observability(node->data.func_decl.return_type)
            || pgy_ast_uses_intent_observability(node->data.func_decl.body);
    case AST_CLASS_DECL:
        return pgy_fields_use_intent_observability(
                node->data.class_decl.fields, node->data.class_decl.field_count)
            || pgy_decl_methods_use_intent_observability(node);
    case AST_EXTERN_BLOCK:
        return pgy_ast_array_uses_intent_observability(
            node->data.extern_block.declarations, node->data.extern_block.count);
    case AST_LET_DECL:
        return pgy_ast_uses_intent_observability(node->data.let_decl.type)
            || pgy_ast_uses_intent_observability(node->data.let_decl.initializer);
    case AST_LET_DESTRUCTURE:
        return pgy_ast_uses_intent_observability(node->data.let_destructure.initializer);
    case AST_TYPE_ALIAS:
        return pgy_ast_uses_intent_observability(node->data.type_alias.target_type);
    case AST_WITH_STMT:
        return pgy_ast_uses_intent_observability(node->data.with_stmt.slot_type)
            || pgy_ast_uses_intent_observability(node->data.with_stmt.body);
    case AST_PARALLEL_BLOCK:
        return pgy_ast_array_uses_intent_observability(
            node->data.parallel.tasks, node->data.parallel.task_count);
    case AST_FOR_LOOP:
        return pgy_ast_uses_intent_observability(node->data.for_loop.range_start)
            || pgy_ast_uses_intent_observability(node->data.for_loop.range_end)
            || pgy_ast_uses_intent_observability(node->data.for_loop.iterable)
            || pgy_ast_uses_intent_observability(node->data.for_loop.body);
    case AST_WHILE_LOOP:
        return pgy_ast_uses_intent_observability(node->data.while_loop.condition)
            || pgy_ast_uses_intent_observability(node->data.while_loop.body);
    case AST_IF_STMT:
        return pgy_ast_uses_intent_observability(node->data.if_stmt.condition)
            || pgy_ast_uses_intent_observability(node->data.if_stmt.then_branch)
            || pgy_ast_uses_intent_observability(node->data.if_stmt.else_branch);
    case AST_RETURN:
        return pgy_ast_uses_intent_observability(node->data.return_stmt.value);
    case AST_ENUM_DECL:
        return pgy_decl_methods_use_intent_observability(node);
    case AST_SELECT_STMT:
        return pgy_ast_array_uses_intent_observability(
                node->data.select_stmt.cases, node->data.select_stmt.case_count)
            || pgy_ast_uses_intent_observability(node->data.select_stmt.default_case);
    case AST_MATCH_STMT:
        return pgy_ast_uses_intent_observability(node->data.match_stmt.subject)
            || pgy_ast_array_uses_intent_observability(
                node->data.match_stmt.cases, node->data.match_stmt.case_count)
            || pgy_ast_uses_intent_observability(node->data.match_stmt.default_body);
    case AST_MATCH_CASE:
        return pgy_ast_uses_intent_observability(node->data.match_case.pattern)
            || pgy_ast_array_uses_intent_observability(
                node->data.match_case.patterns, node->data.match_case.pattern_count)
            || pgy_ast_uses_intent_observability(node->data.match_case.guard)
            || pgy_ast_uses_intent_observability(node->data.match_case.body);
    case AST_BINARY:
        return pgy_ast_uses_intent_observability(node->data.binary.left)
            || pgy_ast_uses_intent_observability(node->data.binary.right);
    case AST_UNARY:
        return pgy_ast_uses_intent_observability(node->data.unary.operand);
    case AST_CALL:
        if (node->data.call.callee != NULL
            && node->data.call.callee->type == AST_IDENTIFIER
            && pgy_builtin_is_intent_observability(
                node->data.call.callee->data.identifier.name)) {
            return true;
        }
        return pgy_ast_uses_intent_observability(node->data.call.callee)
            || pgy_ast_array_uses_intent_observability(
                node->data.call.arguments, node->data.call.arg_count);
    case AST_MEMBER_ACCESS:
        return pgy_ast_uses_intent_observability(node->data.member.object);
    case AST_ARRAY_ACCESS:
        return pgy_ast_uses_intent_observability(node->data.array_access.array)
            || pgy_ast_uses_intent_observability(node->data.array_access.index);
    case AST_ARRAY_LITERAL:
        return pgy_ast_array_uses_intent_observability(
            node->data.array_literal.elements, node->data.array_literal.count);
    case AST_TUPLE_LITERAL:
        return pgy_ast_array_uses_intent_observability(
            node->data.tuple_literal.elements, node->data.tuple_literal.count);
    case AST_ASSIGNMENT:
        return pgy_ast_uses_intent_observability(node->data.assignment.target)
            || pgy_ast_uses_intent_observability(node->data.assignment.value);
    case AST_TYPE:
        return pgy_ast_array_uses_intent_observability(
            node->data.type.tuple_elements, node->data.type.tuple_element_count);
    case AST_AWAIT_EXPR:
        return pgy_ast_uses_intent_observability(node->data.await_expr.expression);
    case AST_CHANNEL_SEND:
        return pgy_ast_uses_intent_observability(node->data.channel_send.channel)
            || pgy_ast_uses_intent_observability(node->data.channel_send.value);
    case AST_CHANNEL_RECV:
        return pgy_ast_uses_intent_observability(node->data.channel_recv.channel);
    case AST_CHANNEL_TYPE:
        return pgy_ast_uses_intent_observability(node->data.channel_type.element_type)
            || pgy_ast_uses_intent_observability(node->data.channel_type.capacity);
    case AST_FUTURE_TYPE:
        return pgy_ast_uses_intent_observability(node->data.future_type.value_type);
    case AST_ASYNC_BLOCK:
        return pgy_ast_array_uses_intent_observability(
            node->data.async_block.statements, node->data.async_block.statement_count);
    case AST_SPAWN_EXPR:
        return pgy_ast_uses_intent_observability(node->data.spawn_expr.function)
            || pgy_ast_array_uses_intent_observability(
                node->data.spawn_expr.arguments, node->data.spawn_expr.arg_count);
    case AST_TASK_GROUP:
        return pgy_ast_array_uses_intent_observability(
            node->data.task_group.tasks, node->data.task_group.task_count);
    case AST_ABILITY_DECL:
        return pgy_ast_array_uses_intent_observability(
                node->data.ability_decl.require_fields, node->data.ability_decl.require_count)
            || pgy_decl_methods_use_intent_observability(node);
    case AST_ROLE_DECL:
        return pgy_ast_uses_intent_observability(node->data.role_decl.for_type)
            || pgy_ast_array_uses_intent_observability(
                node->data.role_decl.includes, node->data.role_decl.include_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.role_decl.impl_abilities, node->data.role_decl.impl_count)
            || pgy_ast_uses_intent_observability(node->data.role_decl.parallel_block);
    case AST_REQUIRE_FIELD:
        return pgy_ast_uses_intent_observability(node->data.require_field.type);
    case AST_IMPL_ABILITY:
        return pgy_ast_uses_intent_observability(node->data.impl_ability.ability_ref)
            || pgy_decl_methods_use_intent_observability(node);
    case AST_OVERRIDE_FUNC:
        return pgy_ast_uses_intent_observability(node->data.override_func.func_decl);
    case AST_PARTY_DECL:
        return pgy_ast_array_uses_intent_observability(
                node->data.party_decl.role_slots, node->data.party_decl.role_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.party_decl.shared_fields, node->data.party_decl.shared_count)
            || pgy_decl_methods_use_intent_observability(node)
            || pgy_ast_uses_intent_observability(node->data.party_decl.extends);
    case AST_PARTY_SHARED:
        return pgy_ast_uses_intent_observability(node->data.party_shared.type)
            || pgy_ast_uses_intent_observability(node->data.party_shared.initializer);
    case AST_PARTY_INSTANCE:
        for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
            if (pgy_ast_uses_intent_observability(
                    node->data.party_instance.assignments[i].value)) {
                return true;
            }
        }
        return false;
    case AST_ROSTER_DECL:
        return pgy_ast_array_uses_intent_observability(
                node->data.roster_decl.party_slots, node->data.roster_decl.party_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.roster_decl.shared_fields, node->data.roster_decl.shared_count)
            || pgy_decl_methods_use_intent_observability(node);
    case AST_WORLD_DECL:
        return pgy_ast_array_uses_intent_observability(
                node->data.world_decl.rosters, node->data.world_decl.roster_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.world_decl.zones, node->data.world_decl.zone_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.world_decl.shared_fields, node->data.world_decl.shared_count)
            || pgy_decl_methods_use_intent_observability(node)
            || pgy_ast_array_uses_intent_observability(
                node->data.world_decl.activations, node->data.world_decl.activate_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.world_decl.deactivations, node->data.world_decl.deactivate_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.world_decl.maintained_zones, node->data.world_decl.maintained_zone_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.world_decl.states, node->data.world_decl.state_count);
    case AST_WORLD_SYSTEMIC:
        return pgy_ast_uses_intent_observability(node->data.world_roster.initializer);
    case AST_WORLD_ZONE:
        return pgy_ast_uses_intent_observability(node->data.world_zone.initializer);
    case AST_INTENT_DECL:
        return pgy_ast_array_uses_intent_observability(
                node->data.intent_decl.involves, node->data.intent_decl.involve_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.intent_decl.values, node->data.intent_decl.value_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.intent_decl.bindings, node->data.intent_decl.binding_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.intent_decl.steps, node->data.intent_decl.step_count)
            || pgy_ast_uses_intent_observability(node->data.intent_decl.priority_expr)
            || pgy_ast_uses_intent_observability(node->data.intent_decl.success_expr)
            || pgy_ast_uses_intent_observability(node->data.intent_decl.failure_expr)
            || pgy_ast_uses_intent_observability(node->data.intent_decl.default_where_type);
    case AST_INTENT_INVOLVES:
        return pgy_ast_uses_intent_observability(node->data.intent_involves.subject_type);
    case AST_INTENT_VALUE:
        return pgy_ast_uses_intent_observability(node->data.intent_value.value_type);
    case AST_INTENT_STEP:
        return pgy_ast_uses_intent_observability(node->data.intent_step.where_type)
            || pgy_ast_uses_intent_observability(node->data.intent_step.using_expr)
            || pgy_ast_uses_intent_observability(node->data.intent_step.intent_expr)
            || pgy_ast_array_uses_intent_observability(
                node->data.intent_step.on_exprs, node->data.intent_step.on_expr_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.intent_step.compensate_exprs, node->data.intent_step.compensate_expr_count)
            || pgy_ast_uses_intent_observability(node->data.intent_step.pre_expr)
            || pgy_ast_uses_intent_observability(node->data.intent_step.guard_expr)
            || pgy_ast_uses_intent_observability(node->data.intent_step.post_expr)
            || pgy_ast_uses_intent_observability(node->data.intent_step.invariant_expr)
            || pgy_ast_array_uses_intent_observability(
                node->data.intent_step.required_abilities, node->data.intent_step.required_ability_count)
            || pgy_ast_uses_intent_observability(node->data.intent_step.expect_expr);
    case AST_RELATION_DECL:
        return pgy_ast_array_uses_intent_observability(
                node->data.relation_decl.slots, node->data.relation_decl.slot_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.relation_decl.refreshes, node->data.relation_decl.refresh_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.relation_decl.shared_fields, node->data.relation_decl.shared_count)
            || pgy_decl_methods_use_intent_observability(node)
            || pgy_ast_uses_intent_observability(node->data.relation_decl.between_left_type)
            || pgy_ast_uses_intent_observability(node->data.relation_decl.between_right_type);
    case AST_EFFECT_DECL:
        return pgy_ast_array_uses_intent_observability(
                node->data.effect_decl.slots, node->data.effect_decl.slot_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.effect_decl.refreshes, node->data.effect_decl.refresh_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.effect_decl.shared_fields, node->data.effect_decl.shared_count)
            || pgy_decl_methods_use_intent_observability(node);
    case AST_ZONE_DECL:
        return pgy_ast_array_uses_intent_observability(
                node->data.zone_decl.slots, node->data.zone_decl.slot_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.zone_decl.layer_slots, node->data.zone_decl.layer_slot_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.zone_decl.applies, node->data.zone_decl.apply_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.zone_decl.links, node->data.zone_decl.link_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.zone_decl.detaches, node->data.zone_decl.detach_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.zone_decl.unlinks, node->data.zone_decl.unlink_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.zone_decl.refreshes, node->data.zone_decl.refresh_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.zone_decl.maintained_effects, node->data.zone_decl.maintained_effect_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.zone_decl.maintained_relations, node->data.zone_decl.maintained_relation_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.zone_decl.maintained_states, node->data.zone_decl.maintained_state_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.zone_decl.authorities, node->data.zone_decl.authority_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.zone_decl.states, node->data.zone_decl.state_count)
            || pgy_ast_array_uses_intent_observability(
                node->data.zone_decl.shared_fields, node->data.zone_decl.shared_count)
            || pgy_decl_methods_use_intent_observability(node);
    case AST_DOMAIN_SLOT:
        return pgy_ast_uses_intent_observability(node->data.domain_slot.type)
            || pgy_ast_uses_intent_observability(node->data.domain_slot.initializer);
    case AST_ZONE_AUTHORITY:
        return pgy_ast_array_uses_intent_observability(
            node->data.zone_authority.required_abilities,
            node->data.zone_authority.ability_count);
    case AST_EVENT_DECL:
        return pgy_ast_array_uses_intent_observability(
                node->data.event_decl.params, node->data.event_decl.param_count)
            || pgy_ast_uses_intent_observability(node->data.event_decl.return_type);
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return pgy_ast_uses_intent_observability(node->data.event_op.event)
            || pgy_ast_uses_intent_observability(node->data.event_op.handler);
    case AST_EVENT_INVOKE:
        return pgy_ast_uses_intent_observability(node->data.event_invoke.event)
            || pgy_ast_array_uses_intent_observability(
                node->data.event_invoke.arguments, node->data.event_invoke.arg_count);
    case AST_EVENT_HANDLER_TYPE:
        return pgy_ast_array_uses_intent_observability(
                node->data.event_handler_type.param_types,
                node->data.event_handler_type.param_count)
            || pgy_ast_uses_intent_observability(node->data.event_handler_type.return_type);
    case AST_LAMBDA_EXPR:
        return pgy_ast_array_uses_intent_observability(
                node->data.lambda_expr.params, node->data.lambda_expr.param_count)
            || pgy_ast_uses_intent_observability(node->data.lambda_expr.body)
            || pgy_ast_uses_intent_observability(node->data.lambda_expr.return_type);
    case AST_NAMESPACE_DECL:
        return pgy_ast_array_uses_intent_observability(
            node->data.namespace_decl.statements, node->data.namespace_decl.count);
    case AST_UNSAFE_BLOCK:
        return pgy_ast_uses_intent_observability(node->data.unsafe_block.body);
    case AST_DEFER_STMT:
        return pgy_ast_uses_intent_observability(node->data.defer_stmt.body);
    default:
        return false;
    }
}

bool
pgy_mir_program_uses_intent_observability(const MIRProgram *mir)
{
    if (mir == NULL)
        return false;

    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        for (size_t b = 0; b < routine->block_count; b++) {
            const MIRBasicBlock *block = &routine->blocks[b];
            for (size_t j = 0; j < block->instruction_count; j++) {
                const MIRInstruction *inst = &block->instructions[j];
                if (pgy_mir_instruction_uses_intent_observability(inst))
                    return true;
            }
            if (pgy_ast_uses_intent_observability(block->source_ast)
                || pgy_ast_uses_intent_observability(block->source_terminator_condition)
                || pgy_ast_uses_intent_observability(block->source_terminator_value)
                || pgy_ast_array_uses_intent_observability(
                    block->source_statements, block->source_statement_count)) {
                return true;
            }
        }
        if (pgy_ast_uses_intent_observability(routine->ast))
            return true;
    }

    return pgy_ast_array_uses_intent_observability(mir->types, mir->type_count)
        || pgy_ast_array_uses_intent_observability(mir->abilities, mir->ability_count)
        || pgy_ast_array_uses_intent_observability(mir->roles, mir->role_count)
        || pgy_ast_array_uses_intent_observability(mir->parties, mir->party_count)
        || pgy_ast_array_uses_intent_observability(mir->rosters, mir->roster_count)
        || pgy_ast_array_uses_intent_observability(mir->worlds, mir->world_count)
        || pgy_ast_array_uses_intent_observability(mir->relations, mir->relation_count)
        || pgy_ast_array_uses_intent_observability(mir->effects, mir->effect_count)
        || pgy_ast_array_uses_intent_observability(mir->zones, mir->zone_count)
        || pgy_ast_array_uses_intent_observability(mir->events, mir->event_count)
        || pgy_ast_array_uses_intent_observability(mir->intents, mir->intent_count)
        || pgy_ast_array_uses_intent_observability(mir->functions, mir->function_count)
        || pgy_ast_array_uses_intent_observability(mir->externs, mir->extern_count);
}
