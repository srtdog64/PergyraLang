#include "hir_analysis.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"

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
append_direct_call(HIRRoutine *routine, const char *name, uint32_t decl_id)
{
    const char **names;
    uint32_t *decl_ids;
    size_t next_capacity;

    if (routine == NULL || name == NULL || *name == '\0')
        return routine != NULL;
    for (size_t i = 0; i < routine->direct_call_count; i++) {
        if ((decl_id != 0 && routine->direct_call_decl_ids[i] == decl_id)
            || (decl_id == 0 && routine->direct_call_decl_ids[i] == 0
                && routine->direct_calls[i] != NULL
                && strcmp(routine->direct_calls[i], name) == 0)) {
            return true;
        }
    }

    if (routine->direct_call_count == routine->direct_call_capacity) {
        next_capacity = routine->direct_call_capacity;
        if (!hir_analysis_next_capacity(&next_capacity, 8,
                                        sizeof(const char *))
            || next_capacity > SIZE_MAX / sizeof(uint32_t)) {
            return false;
        }
        names = malloc(next_capacity * sizeof(const char *));
        decl_ids = malloc(next_capacity * sizeof(uint32_t));
        if (names == NULL || decl_ids == NULL) {
            free((void *)names);
            free(decl_ids);
            return false;
        }
        if (routine->direct_call_count > 0) {
            memcpy((void *)names, routine->direct_calls,
                   routine->direct_call_count * sizeof(const char *));
            memcpy(decl_ids, routine->direct_call_decl_ids,
                   routine->direct_call_count * sizeof(uint32_t));
        }
        free((void *)routine->direct_calls);
        free(routine->direct_call_decl_ids);
        routine->direct_calls = names;
        routine->direct_call_decl_ids = decl_ids;
        routine->direct_call_capacity = next_capacity;
    }

    routine->direct_calls[routine->direct_call_count] = name;
    routine->direct_call_decl_ids[routine->direct_call_count] = decl_id;
    routine->direct_call_count++;
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
                size_t generic_count = ast_generic_param_count(generic_args);
                for (size_t i = 0; i < generic_count; i++) {
                    GenericParam *arg = ast_generic_param_at(generic_args, i);
                    if (ast_generic_param_constraint(arg) != NULL
                        && !hir_collect_type_refs(ast_generic_param_constraint(arg),
                                                  names, count, capacity)) {
                        return false;
                    }
                }
            }
            return true;

        case AST_CHANNEL_TYPE:
            return hir_collect_type_refs(ast_channel_type_element_type(type_node),
                                         names,
                                         count,
                                         capacity);

        case AST_FUTURE_TYPE:
            return hir_collect_type_refs(ast_future_type_value_type(type_node),
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

    if (ast_func_within_zone(node) != NULL
        && !append_call_name(names, count, capacity, ast_func_within_zone(node))) {
        return false;
    }

    if (ast_func_causes_effect(node) != NULL
        && !append_call_name(names, count, capacity, ast_func_causes_effect(node))) {
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
            return true;
        default:
            break;
    }

    switch (node->type) {
        case AST_BLOCK:
            for (size_t i = 0; i < ast_block_statement_count(node); i++) {
                if (hir_ast_contains_control_flow(ast_block_statement(node, i)))
                    return true;
            }
            return false;
        case AST_RETURN:
            return hir_ast_contains_control_flow(ast_return_value(node));
        case AST_LET_DECL:
            return hir_ast_contains_control_flow(ast_let_initializer(node));
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
            return hir_ast_contains_control_flow(ast_match_case_pattern(node))
                   || hir_ast_contains_control_flow(ast_match_case_guard(node))
                   || hir_ast_contains_control_flow(ast_match_case_body(node));
        case AST_ASYNC_BLOCK:
            for (size_t i = 0; i < ast_async_block_statement_count(node); i++) {
                if (hir_ast_contains_control_flow(ast_async_block_statement(node, i)))
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
hir_collect_direct_calls(ASTNode *node, HIRRoutine *routine)
{
    if (node == NULL)
        return true;

    switch (node->type) {
        case AST_CALL:
            if (ast_call_callee(node) != NULL
                && ast_call_callee(node)->type == AST_IDENTIFIER
                && !append_direct_call(
                    routine,
                    ast_identifier_name(ast_call_callee(node)),
                    ast_call_semantic_callee_decl_id(node))) {
                return false;
            }
            if (!hir_collect_direct_calls(ast_call_callee(node), routine))
                return false;
            for (size_t i = 0; i < ast_call_arg_count(node); i++) {
                if (!hir_collect_direct_calls(ast_call_argument(node, i), routine))
                    return false;
            }
            return true;
        case AST_BLOCK:
            for (size_t i = 0; i < ast_block_statement_count(node); i++) {
                if (!hir_collect_direct_calls(ast_block_statement(node, i), routine))
                    return false;
            }
            return true;
        case AST_RETURN:
            return hir_collect_direct_calls(ast_return_value(node), routine);
        case AST_LET_DECL:
            return hir_collect_direct_calls(ast_let_initializer(node), routine);
        case AST_ASSIGNMENT:
            return hir_collect_direct_calls(ast_assignment_target(node), routine)
                   && hir_collect_direct_calls(ast_assignment_value(node), routine);
        case AST_BINARY:
            return hir_collect_direct_calls(ast_binary_left(node), routine)
                   && hir_collect_direct_calls(ast_binary_right(node), routine);
        case AST_UNARY:
            return hir_collect_direct_calls(ast_unary_operand(node), routine);
    case AST_MEMBER_ACCESS:
            return hir_collect_direct_calls(ast_member_object(node), routine);
    case AST_ARRAY_ACCESS:
            return hir_collect_direct_calls(ast_array_access_array(node), routine)
                   && hir_collect_direct_calls(ast_array_access_index(node), routine);
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < ast_array_literal_count(node); i++) {
                if (!hir_collect_direct_calls(ast_array_literal_element(node, i), routine))
                    return false;
            }
            return true;
        case AST_IF_STMT:
            return hir_collect_direct_calls(ast_if_condition(node), routine)
                   && hir_collect_direct_calls(ast_if_then_branch(node), routine)
                   && hir_collect_direct_calls(ast_if_else_branch(node), routine);
        case AST_FOR_LOOP:
            return hir_collect_direct_calls(ast_for_range_start(node), routine)
                   && hir_collect_direct_calls(ast_for_range_end(node), routine)
                   && hir_collect_direct_calls(ast_for_iterable(node), routine)
                   && hir_collect_direct_calls(ast_for_body(node), routine);
        case AST_WHILE_LOOP:
            return hir_collect_direct_calls(ast_while_condition(node), routine)
                   && hir_collect_direct_calls(ast_while_body(node), routine);
        case AST_MATCH_STMT:
            if (!hir_collect_direct_calls(ast_match_subject(node), routine))
                return false;
            for (size_t i = 0; i < ast_match_case_count(node); i++) {
                if (!hir_collect_direct_calls(ast_match_case_at(node, i), routine))
                    return false;
            }
            return hir_collect_direct_calls(ast_match_default_body(node), routine);
        case AST_MATCH_CASE:
            return hir_collect_direct_calls(ast_match_case_pattern(node), routine)
                   && hir_collect_direct_calls(ast_match_case_guard(node), routine)
                   && hir_collect_direct_calls(ast_match_case_body(node), routine);
        case AST_SELECT_STMT:
            for (size_t i = 0; i < ast_select_case_count(node); i++) {
                if (!hir_collect_direct_calls(ast_select_case(node, i), routine))
                    return false;
            }
            return hir_collect_direct_calls(ast_select_default_case(node), routine);
        case AST_ASYNC_BLOCK:
            for (size_t i = 0; i < ast_async_block_statement_count(node); i++) {
                if (!hir_collect_direct_calls(ast_async_block_statement(node, i), routine))
                    return false;
            }
            return true;
        case AST_SPAWN_EXPR:
            if (!hir_collect_direct_calls(ast_spawn_function(node), routine))
                return false;
            for (size_t i = 0; i < ast_spawn_arg_count(node); i++) {
                if (!hir_collect_direct_calls(ast_spawn_argument(node, i), routine))
                    return false;
            }
            return true;
        case AST_PARALLEL_BLOCK:
            for (size_t i = 0; i < ast_parallel_task_count(node); i++) {
                if (!hir_collect_direct_calls(ast_parallel_task(node, i), routine))
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
