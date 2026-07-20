#include "rir.h"
#include "rir_internal.h"
#include "io_boundary_builtin.h"
#include "parser/ast_api.h"

#include <string.h>

#define type_name rir_type_name
#define expr_name rir_expr_name
#define call_name rir_call_name

static bool rir_walk_node_in_statement(RIRScope *scope,
                                       ASTNode *node,
                                       uint32_t source_statement_syntax_id);
static bool rir_walk_block_node_in_statement(
                                       RIRScope *scope,
                                       ASTNode *node,
                                       uint32_t source_statement_syntax_id);

#define add_op(scope, kind, subject, arg0, arg1, ast) \
    add_op_with_source_statement((scope), (kind), (subject), (arg0), (arg1), \
                                 (ast), source_statement_syntax_id)
#define add_op_with_machine_contact(scope, kind, subject, arg0, arg1, contact, ast) \
    add_op_with_machine_contact_source_statement( \
        (scope), (kind), (subject), (arg0), (arg1), (contact), (ast), \
        source_statement_syntax_id)

static bool
rir_walk_call(RIRScope *scope,
              ASTNode *node,
              uint32_t source_statement_syntax_id)
{
    const char *name;
    bool member_slot_op_handled = false;
    if (node == NULL || node->type != AST_CALL)
        return true;

    if (ast_call_callee(node) != NULL
        && ast_call_callee(node)->type == AST_MEMBER_ACCESS) {
        ASTNode *member = ast_call_callee(node);
        ASTNode *receiver = ast_member_object(member);
        const char *method = ast_member_name(member);
        const char *receiver_name = expr_name(receiver);

        if (receiver_name != NULL && method != NULL) {
            if (strcmp(method, "Read") == 0) {
                if (!add_op(scope, RIR_OP_READ, receiver_name, NULL, NULL, node))
                    return false;
                member_slot_op_handled = true;
            } else if (strcmp(method, "Write") == 0) {
                const char *value_name = ast_call_arg_count(node) >= 1
                    ? expr_name(ast_call_argument(node, 0))
                    : NULL;
                if (!add_op(scope, RIR_OP_WRITE, receiver_name, value_name, NULL, node))
                    return false;
                member_slot_op_handled = true;
            } else if (strcmp(method, "Release") == 0) {
                if (!add_op(scope, RIR_OP_RELEASE, receiver_name, NULL, NULL, node))
                    return false;
                member_slot_op_handled = true;
            } else if (strcmp(method, "Move") == 0) {
                if (!add_op(scope, RIR_OP_MOVE, receiver_name, NULL, NULL, node))
                    return false;
                member_slot_op_handled = true;
            }
        }
    }

    name = member_slot_op_handled ? NULL : call_name(node);
    if (name != NULL) {
        size_t argc = ast_call_arg_count(node);

        if (pgy_compiler_io_boundary_builtin_is_stable(name)) {
            const char *first_arg =
                argc >= 1 ? expr_name(ast_call_argument(node, 0)) : NULL;
            if (!add_op(scope, RIR_OP_IO, name, first_arg, NULL, node))
                return false;
        } else if (strcmp(name, "Read") == 0 && argc >= 1) {
            if (!add_op(scope, RIR_OP_READ,
                        expr_name(ast_call_argument(node, 0)), NULL, NULL, node))
                return false;
        } else if (strcmp(name, "DeviceRead") == 0 && argc >= 1) {
            if (!add_op_with_machine_contact(
                    scope, RIR_OP_READ,
                    expr_name(ast_call_argument(node, 0)), NULL, NULL,
                    RIR_MACHINE_CONTACT_READ, node))
                return false;
        } else if (strcmp(name, "DeviceWrite") == 0 && argc >= 2) {
            if (!add_op_with_machine_contact(
                    scope, RIR_OP_WRITE,
                    expr_name(ast_call_argument(node, 0)),
                    expr_name(ast_call_argument(node, 1)), NULL,
                    RIR_MACHINE_CONTACT_WRITE, node))
                return false;
        } else if (strcmp(name, "ReleaseDeviceSlot") == 0 && argc >= 1) {
            if (!add_op_with_machine_contact(
                    scope, RIR_OP_RELEASE,
                    expr_name(ast_call_argument(node, 0)), NULL, NULL,
                    RIR_MACHINE_CONTACT_RELEASE, node))
                return false;
        } else if (strcmp(name, "SubmitDeviceRead") == 0 && argc >= 1) {
            if (!add_op_with_machine_contact(
                    scope, RIR_OP_AWAIT_REMOTE,
                    expr_name(ast_call_argument(node, 0)), NULL, NULL,
                    RIR_MACHINE_CONTACT_SUBMIT_READ, node))
                return false;
        } else if (strcmp(name, "Write") == 0 && argc >= 1) {
            if (!add_op(scope, RIR_OP_WRITE,
                        expr_name(ast_call_argument(node, 0)),
                        argc >= 2 ? expr_name(ast_call_argument(node, 1)) : NULL,
                        NULL, node))
                return false;
        } else if (strcmp(name, "Release") == 0 && argc >= 1) {
            if (!add_op(scope, RIR_OP_RELEASE,
                        expr_name(ast_call_argument(node, 0)), NULL, NULL, node))
                return false;
        } else if (strcmp(name, "Move") == 0 && argc >= 1) {
            if (!add_op(scope, RIR_OP_MOVE,
                        expr_name(ast_call_argument(node, 0)), NULL, NULL, node))
                return false;
        } else if (strcmp(name, "ToObject") == 0 && argc >= 2) {
            if (!add_op(scope, RIR_OP_PROJECT_REFRESH,
                        expr_name(ast_call_argument(node, 1)),
                        expr_name(ast_call_argument(node, 0)), NULL, node))
                return false;
        } else if (strcmp(name, "ToTObject") == 0 && argc >= 2) {
            if (!add_op(scope, RIR_OP_PROJECT_PUBLISH,
                        expr_name(ast_call_argument(node, 1)),
                        expr_name(ast_call_argument(node, 0)), NULL, node))
                return false;
        }
    }

    if (!rir_walk_node_in_statement(
            scope, ast_call_callee(node), source_statement_syntax_id))
        return false;
    for (size_t i = 0; i < ast_call_arg_count(node); i++) {
        if (!rir_walk_node_in_statement(
                scope, ast_call_argument(node, i),
                source_statement_syntax_id))
            return false;
    }
    return true;
}

static RIROpKind
rir_await_op_kind_for_operand(RIRScope *scope, ASTNode *operand)
{
    const char *name;
    const RIRFact *fact;

    if (operand != NULL && operand->type == AST_SPAWN_EXPR)
        return RIR_OP_AWAIT_LOCAL;

    name = expr_name(operand);
    fact = rir_scope_find_fact_by_name_kind(scope, RIR_FACT_RESOURCE, name);
    if (fact != NULL
        && fact->resource_kind == RIR_RESOURCE_LOCAL_FUTURE_HANDLE) {
        return RIR_OP_AWAIT_LOCAL;
    }
    return RIR_OP_AWAIT_REMOTE;
}

static const char *
rir_await_subject_for_operand(ASTNode *operand)
{
    if (operand != NULL && operand->type == AST_SPAWN_EXPR)
        return "spawn";
    return expr_name(operand);
}

bool
rir_walk_node(RIRScope *scope, ASTNode *node)
{
    return rir_walk_node_in_statement(
        scope, node, ast_node_stable_id(node));
}

static bool
rir_walk_node_in_statement(RIRScope *scope,
                           ASTNode *node,
                           uint32_t source_statement_syntax_id)
{
    if (scope == NULL || node == NULL)
        return true;

    switch (node->type) {
        case AST_BLOCK:
            for (size_t i = 0; i < ast_block_statement_count(node); i++) {
                ASTNode *statement = ast_block_statement(node, i);
                if (!rir_walk_node_in_statement(
                        scope, statement, ast_node_stable_id(statement)))
                    return false;
            }
            return true;

        case AST_LET_DECL: {
            const char *init_name = NULL;
            RIRResourceState state = RIR_STATE_UNINIT;
            ASTNode *initializer = ast_let_initializer(node);
            const char *name = ast_let_name(node);
            if (initializer != NULL && initializer->type == AST_CALL) {
                init_name = call_name(initializer);
            }

            if (init_name != NULL) {
                if (strcmp(init_name, "ClaimSlot") == 0) {
                    state = RIR_STATE_OWNED;
                    if (!add_op(scope, RIR_OP_CLAIM, name, "Slot", NULL,
                                initializer))
                        return false;
                } else if (strcmp(init_name, "ClaimSecureSlot") == 0) {
                    state = RIR_STATE_OWNED;
                    if (!add_op(scope, RIR_OP_CLAIM, name, "SecureSlot", NULL,
                                initializer))
                        return false;
                } else if (strcmp(init_name, "ClaimDeviceSlot") == 0) {
                    state = RIR_STATE_OWNED;
                    if (!add_op_with_machine_contact(
                            scope, RIR_OP_CLAIM, name, "DeviceSlot", NULL,
                            RIR_MACHINE_CONTACT_CLAIM, initializer))
                        return false;
                } else if (strcmp(init_name, "ViewRead") == 0
                           && ast_call_arg_count(initializer) >= 1) {
                    if (!add_op(scope,
                                RIR_OP_BORROW_READ,
                                expr_name(ast_call_argument(
                                    initializer, 0)),
                                name,
                                NULL,
                                initializer))
                        return false;
                } else if (strcmp(init_name, "ViewWrite") == 0
                           && ast_call_arg_count(initializer) >= 1) {
                    if (!add_op(scope,
                                RIR_OP_BORROW_WRITE,
                                expr_name(ast_call_argument(
                                    initializer, 0)),
                                name,
                                NULL,
                                initializer))
                        return false;
                }
            }

            if (!add_resource_fact(scope,
                                   name,
                                   ast_let_type(node),
                                   state,
                                   ast_node_stable_id(node),
                                   node))
                return false;
            return rir_walk_node_in_statement(
                scope, initializer, source_statement_syntax_id);
        }

        case AST_WITH_STMT:
            if (!add_resource_fact(scope,
                                   ast_with_alias(node),
                                   ast_with_slot_type(node),
                                   RIR_STATE_OWNED,
                                   ast_node_stable_id(node),
                                   node))
                return false;
            if (!add_op(scope, RIR_OP_CLAIM,
                        ast_with_alias(node),
                        ast_with_is_secure(node) ? "SecureSlot" : "Slot",
                        NULL,
                        node))
                return false;
            if (!rir_walk_node_in_statement(
                    scope, ast_with_body(node), source_statement_syntax_id))
                return false;
            return add_op(scope, RIR_OP_RELEASE,
                          ast_with_alias(node),
                          NULL,
                          NULL,
                          node);

        case AST_ASSIGNMENT:
            if (!rir_walk_node_in_statement(
                    scope, ast_assignment_target(node),
                    source_statement_syntax_id))
                return false;
            return rir_walk_node_in_statement(
                scope, ast_assignment_value(node), source_statement_syntax_id);

        case AST_AWAIT_EXPR:
            if (!add_op(scope,
                        rir_await_op_kind_for_operand(
                            scope,
                            ast_await_expression(node)),
                        rir_await_subject_for_operand(
                            ast_await_expression(node)),
                        NULL, NULL, node))
                return false;
            return rir_walk_node_in_statement(
                scope, ast_await_expression(node),
                source_statement_syntax_id);

        case AST_SPAWN_EXPR:
            if (!add_op(scope, RIR_OP_SPAWN, "spawn", NULL, NULL, node))
                return false;
            if (!rir_walk_node_in_statement(
                    scope, ast_spawn_function(node),
                    source_statement_syntax_id))
                return false;
            for (size_t i = 0; i < ast_spawn_arg_count(node); i++) {
                if (!rir_walk_node_in_statement(
                        scope, ast_spawn_argument(node, i),
                        source_statement_syntax_id))
                    return false;
            }
            return true;

        case AST_CHANNEL_SEND:
            if (!add_op(scope,
                        RIR_OP_CHANNEL_SEND,
                        expr_name(ast_channel_send_channel(node)),
                        expr_name(ast_channel_send_value(node)),
                        NULL,
                        node))
                return false;
            if (!rir_walk_node_in_statement(
                    scope, ast_channel_send_channel(node),
                    source_statement_syntax_id))
                return false;
            return rir_walk_node_in_statement(
                scope, ast_channel_send_value(node),
                source_statement_syntax_id);

        case AST_CHANNEL_RECV:
            if (!add_op(scope,
                        RIR_OP_CHANNEL_RECV,
                        expr_name(ast_channel_recv_channel(node)),
                        NULL,
                        NULL,
                        node))
                return false;
            return rir_walk_node_in_statement(
                scope, ast_channel_recv_channel(node),
                source_statement_syntax_id);

        case AST_BINARY:
            return rir_walk_node_in_statement(
                       scope, ast_binary_left(node),
                       source_statement_syntax_id)
                && rir_walk_node_in_statement(
                       scope, ast_binary_right(node),
                       source_statement_syntax_id);

        case AST_UNARY:
            return rir_walk_node_in_statement(
                scope, ast_unary_operand(node), source_statement_syntax_id);

        case AST_MEMBER_ACCESS:
            return rir_walk_node_in_statement(
                scope, ast_member_object(node), source_statement_syntax_id);

        case AST_ARRAY_ACCESS:
            return rir_walk_node_in_statement(
                       scope, ast_array_access_array(node),
                       source_statement_syntax_id)
                && rir_walk_node_in_statement(
                       scope, ast_array_access_index(node),
                       source_statement_syntax_id);

        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < ast_array_literal_count(node); i++) {
                if (!rir_walk_node_in_statement(
                        scope, ast_array_literal_element(node, i),
                        source_statement_syntax_id))
                    return false;
            }
            return true;

        case AST_TUPLE_LITERAL:
            for (size_t i = 0; i < ast_tuple_literal_count(node); i++) {
                if (!rir_walk_node_in_statement(
                        scope, ast_tuple_literal_element(node, i),
                        source_statement_syntax_id))
                    return false;
            }
            return true;

        case AST_MAP_LITERAL:
            for (size_t i = 0; i < ast_map_literal_count(node); i++) {
                if (!rir_walk_node_in_statement(
                        scope, ast_map_literal_key(node, i),
                        source_statement_syntax_id)
                    || !rir_walk_node_in_statement(
                        scope, ast_map_literal_value(node, i),
                        source_statement_syntax_id)) {
                    return false;
                }
            }
            return true;

        case AST_CAST:
            return rir_walk_node_in_statement(
                scope, ast_cast_operand(node), source_statement_syntax_id);

        case AST_TYPE_TEST:
            return rir_walk_node_in_statement(
                scope, ast_type_test_operand(node),
                source_statement_syntax_id);

        case AST_SELECT_STMT:
            if (!add_op(scope, RIR_OP_CHANNEL_SELECT, "select", NULL, NULL, node))
                return false;
            for (size_t i = 0; i < ast_select_case_count(node); i++) {
                if (!rir_walk_node_in_statement(
                        scope, ast_select_case(node, i),
                        source_statement_syntax_id))
                    return false;
            }
            return rir_walk_node_in_statement(
                scope, ast_select_default_case(node),
                source_statement_syntax_id);

        case AST_CALL:
            return rir_walk_call(scope, node, source_statement_syntax_id);

        case AST_IF_STMT:
            if (!rir_walk_node_in_statement(
                    scope, ast_if_condition(node),
                    source_statement_syntax_id))
                return false;
            if (!rir_walk_node_in_statement(
                    scope, ast_if_then_branch(node),
                    source_statement_syntax_id))
                return false;
            return rir_walk_node_in_statement(
                scope, ast_if_else_branch(node), source_statement_syntax_id);

        case AST_FOR_LOOP:
            if (!rir_walk_node_in_statement(
                    scope, ast_for_range_start(node),
                    source_statement_syntax_id))
                return false;
            if (!rir_walk_node_in_statement(
                    scope, ast_for_range_end(node),
                    source_statement_syntax_id))
                return false;
            if (!rir_walk_node_in_statement(
                    scope, ast_for_iterable(node),
                    source_statement_syntax_id))
                return false;
            return rir_walk_node_in_statement(
                scope, ast_for_body(node), source_statement_syntax_id);

        case AST_WHILE_LOOP:
            if (!rir_walk_node_in_statement(
                    scope, ast_while_condition(node),
                    source_statement_syntax_id))
                return false;
            return rir_walk_node_in_statement(
                scope, ast_while_body(node), source_statement_syntax_id);

        case AST_RETURN:
            return rir_walk_node_in_statement(
                scope, ast_return_value(node), source_statement_syntax_id);

        case AST_MATCH_STMT:
            if (!rir_walk_node_in_statement(
                    scope, ast_match_subject(node),
                    source_statement_syntax_id))
                return false;
            for (size_t i = 0; i < ast_match_case_count(node); i++) {
                if (!rir_walk_node_in_statement(
                        scope, ast_match_case_at(node, i),
                        source_statement_syntax_id))
                    return false;
            }
            if (!rir_walk_node_in_statement(
                    scope, ast_match_default_body(node),
                    source_statement_syntax_id))
                return false;
            return true;

        case AST_MATCH_CASE:
            if (!rir_walk_node_in_statement(
                    scope, ast_match_case_pattern(node),
                    source_statement_syntax_id))
                return false;
            if (!rir_walk_node_in_statement(
                    scope, ast_match_case_guard(node),
                    source_statement_syntax_id))
                return false;
            return rir_walk_node_in_statement(
                scope, ast_match_case_body(node), source_statement_syntax_id);

        case AST_PARALLEL_BLOCK:
            if (!add_op(scope, RIR_OP_PARALLEL, "parallel", NULL, NULL, node))
                return false;
            for (size_t i = 0; i < ast_parallel_task_count(node); i++) {
                if (!rir_walk_node_in_statement(
                        scope, ast_parallel_task(node, i),
                        source_statement_syntax_id))
                    return false;
            }
            return true;

        case AST_ASYNC_BLOCK:
            if (!add_op(scope, RIR_OP_ASYNC, "async", NULL, NULL, node))
                return false;
            for (size_t i = 0; i < ast_async_block_statement_count(node); i++) {
                if (!rir_walk_node_in_statement(
                        scope, ast_async_block_statement(node, i),
                        source_statement_syntax_id))
                    return false;
            }
            return true;

        default:
            return true;
    }
}

bool
rir_walk_block_node(RIRScope *scope, ASTNode *node)
{
    return rir_walk_block_node_in_statement(
        scope, node, ast_node_stable_id(node));
}

static bool
rir_walk_block_node_in_statement(RIRScope *scope,
                                 ASTNode *node,
                                 uint32_t source_statement_syntax_id)
{
    if (scope == NULL || node == NULL)
        return true;

    switch (node->type) {
        case AST_IF_STMT:
            return rir_walk_block_node_in_statement(
                scope, ast_if_condition(node), source_statement_syntax_id);
        case AST_FOR_LOOP:
            return rir_walk_block_node_in_statement(
                       scope, ast_for_range_start(node),
                       source_statement_syntax_id)
                   && rir_walk_block_node_in_statement(
                       scope, ast_for_range_end(node),
                       source_statement_syntax_id)
                   && rir_walk_block_node_in_statement(
                       scope, ast_for_iterable(node),
                       source_statement_syntax_id);
        case AST_WHILE_LOOP:
            return rir_walk_block_node_in_statement(
                scope, ast_while_condition(node),
                source_statement_syntax_id);
        case AST_MATCH_STMT:
            if (!rir_walk_block_node_in_statement(
                    scope, ast_match_subject(node),
                    source_statement_syntax_id))
                return false;
            for (size_t i = 0; i < ast_match_case_count(node); i++) {
                ASTNode *match_case = ast_match_case_at(node, i);
                if (match_case == NULL)
                    continue;
                if (!rir_walk_block_node_in_statement(
                        scope, ast_match_case_pattern(match_case),
                        source_statement_syntax_id)
                    || !rir_walk_block_node_in_statement(
                        scope, ast_match_case_guard(match_case),
                        source_statement_syntax_id)) {
                    return false;
                }
            }
            return true;
        case AST_BLOCK:
            for (size_t i = 0; i < ast_block_statement_count(node); i++) {
                ASTNode *statement = ast_block_statement(node, i);
                if (!rir_walk_block_node_in_statement(
                        scope, statement, ast_node_stable_id(statement)))
                    return false;
            }
            return true;
        default:
            return rir_walk_node_in_statement(
                scope, node, source_statement_syntax_id);
    }
}
