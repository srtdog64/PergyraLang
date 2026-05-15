#include "hir_analysis.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
hir_analysis_next_capacity(size_t *capacity, size_t initial, size_t elem_size)
{
    if (capacity == NULL || initial == 0 || elem_size == 0)
        return false;

    size_t current = *capacity;
    size_t next_capacity = initial;
    if (current != 0) {
        if (current > SIZE_MAX / 2)
            return false;
        next_capacity = current * 2;
    }
    if (next_capacity > SIZE_MAX / elem_size)
        return false;

    *capacity = next_capacity;
    return true;
}

static bool
append_call_name(const char ***names, size_t *count, size_t *capacity, const char *name)
{
    if (names == NULL || count == NULL || capacity == NULL)
        return false;
    if (name == NULL || *name == '\0')
        return true;

    for (size_t i = 0; i < *count; i++) {
        if ((*names)[i] != NULL && strcmp((*names)[i], name) == 0)
            return true;
    }

    if (*count == *capacity) {
        size_t next_capacity = *capacity;
        if (!hir_analysis_next_capacity(&next_capacity, 8, sizeof(const char *)))
            return false;
        const char **grown = realloc((void *)*names, next_capacity * sizeof(const char *));
        if (grown == NULL)
            return false;
        *names = grown;
        *capacity = next_capacity;
    }
    (*names)[*count] = name;
    (*count)++;
    return true;
}

static bool
hir_collect_type_refs(ASTNode *type_node, const char ***names, size_t *count, size_t *capacity)
{
    if (type_node == NULL)
        return true;

    switch (type_node->type) {
        case AST_TYPE:
            if (!append_call_name(names, count, capacity, ast_type_name(type_node)))
                return false;
            if (ast_type_generic_args(type_node) != NULL) {
                GenericParams *generic_args = ast_type_generic_args(type_node);
                for (size_t i = 0; i < generic_args->count; i++) {
                    GenericParam *arg = generic_args->params[i];
                    if (arg != NULL
                        && arg->constraint != NULL
                        && !hir_collect_type_refs(arg->constraint, names, count, capacity)) {
                        return false;
                    }
                }
            }
            return true;

        case AST_CHANNEL_TYPE:
            return hir_collect_type_refs(type_node->data.channel_type.element_type,
                                         names,
                                         count,
                                         capacity);

        case AST_FUTURE_TYPE:
            return hir_collect_type_refs(type_node->data.future_type.value_type,
                                         names,
                                         count,
                                         capacity);

        default:
            return true;
    }
}

bool
hir_collect_func_signature_refs(ASTNode *node,
                                const char ***names,
                                size_t *count,
                                size_t *capacity)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return true;

    for (size_t i = 0; i < ast_func_param_count(node); i++) {
        FuncParam *param = ast_func_param(node, i);
        if (param != NULL && !hir_collect_type_refs(param->type, names, count, capacity))
            return false;
    }

    if (!hir_collect_type_refs(ast_func_return_type(node), names, count, capacity))
        return false;

    if (node->data.func_decl.within_zone != NULL
        && !append_call_name(names, count, capacity, node->data.func_decl.within_zone)) {
        return false;
    }

    if (node->data.func_decl.causes_effect != NULL
        && !append_call_name(names, count, capacity, node->data.func_decl.causes_effect)) {
        return false;
    }

    return true;
}

bool
hir_collect_intent_signature_refs(ASTNode *node,
                                  const char ***names,
                                  size_t *count,
                                  size_t *capacity)
{
    ASTNode **bindings;
    ASTNode **steps;
    size_t binding_count;
    size_t step_count;

    if (node == NULL || node->type != AST_INTENT_DECL)
        return true;

    bindings = ast_intent_decl_bindings(node, &binding_count);
    steps = ast_intent_decl_steps(node, &step_count);

    for (size_t i = 0; i < binding_count; i++) {
        ASTNode *binding = bindings[i];
        if (binding != NULL
            && binding->type == AST_INTENT_INVOLVES
            && !hir_collect_type_refs(ast_intent_involves_subject_type(binding),
                                      names,
                                      count,
                                      capacity)) {
            return false;
        }
        if (binding != NULL
            && binding->type == AST_INTENT_VALUE
            && !hir_collect_type_refs(ast_intent_value_type(binding),
                                      names,
                                      count,
                                      capacity)) {
            return false;
        }
    }

    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = steps[i];
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        if (!hir_collect_type_refs(ast_intent_step_where_type(step), names, count, capacity))
            return false;
        if (ast_intent_step_causes_effect(step) != NULL
            && !append_call_name(names, count, capacity, ast_intent_step_causes_effect(step))) {
            return false;
        }
    }

    return true;
}

bool
hir_ast_contains_control_flow(ASTNode *node)
{
    if (node == NULL)
        return false;

    switch (node->type) {
        case AST_IF_STMT:
        case AST_FOR_LOOP:
        case AST_WHILE_LOOP:
        case AST_SELECT_STMT:
        case AST_MATCH_STMT:
        case AST_PARALLEL_BLOCK:
        case AST_WITH_STMT:
        case AST_AWAIT_EXPR:
        case AST_SPAWN_EXPR:
        case AST_TASK_GROUP:
            return true;
        default:
            break;
    }

    switch (node->type) {
        case AST_BLOCK:
            for (size_t i = 0; i < node->data.block.count; i++) {
                if (hir_ast_contains_control_flow(node->data.block.statements[i]))
                    return true;
            }
            return false;
        case AST_RETURN:
            return hir_ast_contains_control_flow(ast_return_value(node));
        case AST_LET_DECL:
            return hir_ast_contains_control_flow(node->data.let_decl.initializer);
        case AST_ASSIGNMENT:
            return hir_ast_contains_control_flow(ast_assignment_target(node))
                   || hir_ast_contains_control_flow(ast_assignment_value(node));
        case AST_BINARY:
            return hir_ast_contains_control_flow(ast_binary_left(node))
                   || hir_ast_contains_control_flow(ast_binary_right(node));
        case AST_UNARY:
            return hir_ast_contains_control_flow(ast_unary_operand(node));
        case AST_CALL:
            if (hir_ast_contains_control_flow(ast_call_callee(node)))
                return true;
            for (size_t i = 0; i < ast_call_arg_count(node); i++) {
                if (hir_ast_contains_control_flow(ast_call_argument(node, i)))
                    return true;
            }
            return false;
    case AST_MEMBER_ACCESS:
            return hir_ast_contains_control_flow(ast_member_object(node));
    case AST_ARRAY_ACCESS:
            return hir_ast_contains_control_flow(ast_array_access_array(node))
                   || hir_ast_contains_control_flow(ast_array_access_index(node));
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < ast_array_literal_count(node); i++) {
                if (hir_ast_contains_control_flow(ast_array_literal_element(node, i)))
                    return true;
            }
            return false;
        case AST_MATCH_CASE:
            return hir_ast_contains_control_flow(node->data.match_case.pattern)
                   || hir_ast_contains_control_flow(node->data.match_case.guard)
                   || hir_ast_contains_control_flow(node->data.match_case.body);
        case AST_ASYNC_BLOCK:
            for (size_t i = 0; i < ast_async_block_statement_count(node); i++) {
                if (hir_ast_contains_control_flow(ast_async_block_statement(node, i)))
                    return true;
            }
            return false;
        case AST_TASK_GROUP:
            for (size_t i = 0; i < ast_task_group_task_count(node); i++) {
                if (hir_ast_contains_control_flow(ast_task_group_task(node, i)))
                    return true;
            }
            return false;
        case AST_PARALLEL_BLOCK:
            for (size_t i = 0; i < ast_parallel_task_count(node); i++) {
                if (hir_ast_contains_control_flow(ast_parallel_task(node, i)))
                    return true;
            }
            return false;
        default:
            return false;
    }
}

bool
hir_collect_direct_calls(ASTNode *node,
                         const char ***names,
                         size_t *count,
                         size_t *capacity)
{
    if (node == NULL)
        return true;

    switch (node->type) {
        case AST_CALL:
            if (ast_call_callee(node) != NULL
                && ast_call_callee(node)->type == AST_IDENTIFIER
                && !append_call_name(names,
                                     count,
                                     capacity,
                                     ast_call_callee(node)->data.identifier.name)) {
                return false;
            }
            if (!hir_collect_direct_calls(ast_call_callee(node), names, count, capacity))
                return false;
            for (size_t i = 0; i < ast_call_arg_count(node); i++) {
                if (!hir_collect_direct_calls(ast_call_argument(node, i), names, count, capacity))
                    return false;
            }
            return true;
        case AST_BLOCK:
            for (size_t i = 0; i < node->data.block.count; i++) {
                if (!hir_collect_direct_calls(node->data.block.statements[i], names, count, capacity))
                    return false;
            }
            return true;
        case AST_RETURN:
            return hir_collect_direct_calls(ast_return_value(node), names, count, capacity);
        case AST_LET_DECL:
            return hir_collect_direct_calls(node->data.let_decl.initializer, names, count, capacity);
        case AST_ASSIGNMENT:
            return hir_collect_direct_calls(ast_assignment_target(node), names, count, capacity)
                   && hir_collect_direct_calls(ast_assignment_value(node), names, count, capacity);
        case AST_BINARY:
            return hir_collect_direct_calls(ast_binary_left(node), names, count, capacity)
                   && hir_collect_direct_calls(ast_binary_right(node), names, count, capacity);
        case AST_UNARY:
            return hir_collect_direct_calls(ast_unary_operand(node), names, count, capacity);
    case AST_MEMBER_ACCESS:
            return hir_collect_direct_calls(ast_member_object(node), names, count, capacity);
    case AST_ARRAY_ACCESS:
            return hir_collect_direct_calls(ast_array_access_array(node), names, count, capacity)
                   && hir_collect_direct_calls(ast_array_access_index(node), names, count, capacity);
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < ast_array_literal_count(node); i++) {
                if (!hir_collect_direct_calls(ast_array_literal_element(node, i), names, count, capacity))
                    return false;
            }
            return true;
        case AST_IF_STMT:
            return hir_collect_direct_calls(ast_if_condition(node), names, count, capacity)
                   && hir_collect_direct_calls(ast_if_then_branch(node), names, count, capacity)
                   && hir_collect_direct_calls(ast_if_else_branch(node), names, count, capacity);
        case AST_FOR_LOOP:
            return hir_collect_direct_calls(ast_for_range_start(node), names, count, capacity)
                   && hir_collect_direct_calls(ast_for_range_end(node), names, count, capacity)
                   && hir_collect_direct_calls(ast_for_iterable(node), names, count, capacity)
                   && hir_collect_direct_calls(ast_for_body(node), names, count, capacity);
        case AST_WHILE_LOOP:
            return hir_collect_direct_calls(ast_while_condition(node), names, count, capacity)
                   && hir_collect_direct_calls(ast_while_body(node), names, count, capacity);
        case AST_MATCH_STMT:
            if (!hir_collect_direct_calls(node->data.match_stmt.subject, names, count, capacity))
                return false;
            for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
                if (!hir_collect_direct_calls(node->data.match_stmt.cases[i], names, count, capacity))
                    return false;
            }
            return hir_collect_direct_calls(node->data.match_stmt.default_body, names, count, capacity);
        case AST_MATCH_CASE:
            return hir_collect_direct_calls(node->data.match_case.pattern, names, count, capacity)
                   && hir_collect_direct_calls(node->data.match_case.guard, names, count, capacity)
                   && hir_collect_direct_calls(node->data.match_case.body, names, count, capacity);
        case AST_SELECT_STMT:
            for (size_t i = 0; i < ast_select_case_count(node); i++) {
                if (!hir_collect_direct_calls(ast_select_case(node, i), names, count, capacity))
                    return false;
            }
            return hir_collect_direct_calls(ast_select_default_case(node), names, count, capacity);
        case AST_ASYNC_BLOCK:
            for (size_t i = 0; i < ast_async_block_statement_count(node); i++) {
                if (!hir_collect_direct_calls(ast_async_block_statement(node, i), names, count, capacity))
                    return false;
            }
            return true;
        case AST_SPAWN_EXPR:
            if (!hir_collect_direct_calls(ast_spawn_function(node), names, count, capacity))
                return false;
            for (size_t i = 0; i < ast_spawn_arg_count(node); i++) {
                if (!hir_collect_direct_calls(ast_spawn_argument(node, i), names, count, capacity))
                    return false;
            }
            return true;
        case AST_TASK_GROUP:
            for (size_t i = 0; i < ast_task_group_task_count(node); i++) {
                if (!hir_collect_direct_calls(ast_task_group_task(node, i), names, count, capacity))
                    return false;
            }
            return true;
        case AST_PARALLEL_BLOCK:
            for (size_t i = 0; i < ast_parallel_task_count(node); i++) {
                if (!hir_collect_direct_calls(ast_parallel_task(node, i), names, count, capacity))
                    return false;
            }
            return true;
        case AST_EVENT_SUBSCRIBE:
        case AST_EVENT_UNSUBSCRIBE:
        case AST_EVENT_INVOKE:
        case AST_LAMBDA_EXPR:
        default:
            return true;
    }
}
