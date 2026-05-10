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
            if (!append_call_name(names, count, capacity, type_node->data.type.name))
                return false;
            if (type_node->data.type.generic_args != NULL) {
                for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
                    GenericParam *arg = type_node->data.type.generic_args->params[i];
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

    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *param = node->data.func_decl.params[i];
        if (param != NULL && !hir_collect_type_refs(param->type, names, count, capacity))
            return false;
    }

    if (!hir_collect_type_refs(node->data.func_decl.return_type, names, count, capacity))
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
    if (node == NULL || node->type != AST_INTENT_DECL)
        return true;

    for (size_t i = 0; i < node->data.intent_decl.binding_count; i++) {
        ASTNode *binding = node->data.intent_decl.bindings[i];
        if (binding != NULL
            && binding->type == AST_INTENT_INVOLVES
            && !hir_collect_type_refs(binding->data.intent_involves.subject_type,
                                      names,
                                      count,
                                      capacity)) {
            return false;
        }
        if (binding != NULL
            && binding->type == AST_INTENT_VALUE
            && !hir_collect_type_refs(binding->data.intent_value.value_type,
                                      names,
                                      count,
                                      capacity)) {
            return false;
        }
    }

    for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
        ASTNode *step = node->data.intent_decl.steps[i];
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        if (!hir_collect_type_refs(step->data.intent_step.where_type, names, count, capacity))
            return false;
        if (step->data.intent_step.causes_effect != NULL
            && !append_call_name(names, count, capacity, step->data.intent_step.causes_effect)) {
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
            return hir_ast_contains_control_flow(node->data.return_stmt.value);
        case AST_LET_DECL:
            return hir_ast_contains_control_flow(node->data.let_decl.initializer);
        case AST_ASSIGNMENT:
            return hir_ast_contains_control_flow(node->data.assignment.target)
                   || hir_ast_contains_control_flow(node->data.assignment.value);
        case AST_BINARY:
            return hir_ast_contains_control_flow(node->data.binary.left)
                   || hir_ast_contains_control_flow(node->data.binary.right);
        case AST_UNARY:
            return hir_ast_contains_control_flow(node->data.unary.operand);
        case AST_CALL:
            if (hir_ast_contains_control_flow(node->data.call.callee))
                return true;
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (hir_ast_contains_control_flow(node->data.call.arguments[i]))
                    return true;
            }
            return false;
        case AST_MEMBER_ACCESS:
            return hir_ast_contains_control_flow(node->data.member.object);
        case AST_ARRAY_ACCESS:
            return hir_ast_contains_control_flow(node->data.array_access.array)
                   || hir_ast_contains_control_flow(node->data.array_access.index);
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < node->data.array_literal.count; i++) {
                if (hir_ast_contains_control_flow(node->data.array_literal.elements[i]))
                    return true;
            }
            return false;
        case AST_MATCH_CASE:
            return hir_ast_contains_control_flow(node->data.match_case.pattern)
                   || hir_ast_contains_control_flow(node->data.match_case.guard)
                   || hir_ast_contains_control_flow(node->data.match_case.body);
        case AST_ASYNC_BLOCK:
            for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
                if (hir_ast_contains_control_flow(node->data.async_block.statements[i]))
                    return true;
            }
            return false;
        case AST_TASK_GROUP:
            for (size_t i = 0; i < node->data.task_group.task_count; i++) {
                if (hir_ast_contains_control_flow(node->data.task_group.tasks[i]))
                    return true;
            }
            return false;
        case AST_PARALLEL_BLOCK:
            for (size_t i = 0; i < node->data.parallel.task_count; i++) {
                if (hir_ast_contains_control_flow(node->data.parallel.tasks[i]))
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
            if (node->data.call.callee != NULL
                && node->data.call.callee->type == AST_IDENTIFIER
                && !append_call_name(names,
                                     count,
                                     capacity,
                                     node->data.call.callee->data.identifier.name)) {
                return false;
            }
            if (!hir_collect_direct_calls(node->data.call.callee, names, count, capacity))
                return false;
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (!hir_collect_direct_calls(node->data.call.arguments[i], names, count, capacity))
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
            return hir_collect_direct_calls(node->data.return_stmt.value, names, count, capacity);
        case AST_LET_DECL:
            return hir_collect_direct_calls(node->data.let_decl.initializer, names, count, capacity);
        case AST_ASSIGNMENT:
            return hir_collect_direct_calls(node->data.assignment.target, names, count, capacity)
                   && hir_collect_direct_calls(node->data.assignment.value, names, count, capacity);
        case AST_BINARY:
            return hir_collect_direct_calls(node->data.binary.left, names, count, capacity)
                   && hir_collect_direct_calls(node->data.binary.right, names, count, capacity);
        case AST_UNARY:
            return hir_collect_direct_calls(node->data.unary.operand, names, count, capacity);
        case AST_MEMBER_ACCESS:
            return hir_collect_direct_calls(node->data.member.object, names, count, capacity);
        case AST_ARRAY_ACCESS:
            return hir_collect_direct_calls(node->data.array_access.array, names, count, capacity)
                   && hir_collect_direct_calls(node->data.array_access.index, names, count, capacity);
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < node->data.array_literal.count; i++) {
                if (!hir_collect_direct_calls(node->data.array_literal.elements[i], names, count, capacity))
                    return false;
            }
            return true;
        case AST_IF_STMT:
            return hir_collect_direct_calls(node->data.if_stmt.condition, names, count, capacity)
                   && hir_collect_direct_calls(node->data.if_stmt.then_branch, names, count, capacity)
                   && hir_collect_direct_calls(node->data.if_stmt.else_branch, names, count, capacity);
        case AST_FOR_LOOP:
            return hir_collect_direct_calls(node->data.for_loop.range_start, names, count, capacity)
                   && hir_collect_direct_calls(node->data.for_loop.range_end, names, count, capacity)
                   && hir_collect_direct_calls(node->data.for_loop.iterable, names, count, capacity)
                   && hir_collect_direct_calls(node->data.for_loop.body, names, count, capacity);
        case AST_WHILE_LOOP:
            return hir_collect_direct_calls(node->data.while_loop.condition, names, count, capacity)
                   && hir_collect_direct_calls(node->data.while_loop.body, names, count, capacity);
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
            for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
                if (!hir_collect_direct_calls(node->data.select_stmt.cases[i], names, count, capacity))
                    return false;
            }
            return hir_collect_direct_calls(node->data.select_stmt.default_case, names, count, capacity);
        case AST_ASYNC_BLOCK:
            for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
                if (!hir_collect_direct_calls(node->data.async_block.statements[i], names, count, capacity))
                    return false;
            }
            return true;
        case AST_SPAWN_EXPR:
            if (!hir_collect_direct_calls(node->data.spawn_expr.function, names, count, capacity))
                return false;
            for (size_t i = 0; i < node->data.spawn_expr.arg_count; i++) {
                if (!hir_collect_direct_calls(node->data.spawn_expr.arguments[i], names, count, capacity))
                    return false;
            }
            return true;
        case AST_TASK_GROUP:
            for (size_t i = 0; i < node->data.task_group.task_count; i++) {
                if (!hir_collect_direct_calls(node->data.task_group.tasks[i], names, count, capacity))
                    return false;
            }
            return true;
        case AST_PARALLEL_BLOCK:
            for (size_t i = 0; i < node->data.parallel.task_count; i++) {
                if (!hir_collect_direct_calls(node->data.parallel.tasks[i], names, count, capacity))
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
