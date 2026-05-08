#include "rir.h"
#include "rir_internal.h"
#include "io_boundary_builtin.h"

#include <string.h>

#define type_name rir_type_name
#define expr_name rir_expr_name
#define call_name rir_call_name

static bool
rir_walk_call(RIRScope *scope, ASTNode *node)
{
    const char *name;
    bool member_slot_op_handled = false;
    if (node == NULL || node->type != AST_CALL)
        return true;

    if (node->data.call.callee != NULL
        && node->data.call.callee->type == AST_MEMBER_ACCESS) {
        ASTNode *member = node->data.call.callee;
        ASTNode *receiver = member->data.member.object;
        const char *method = member->data.member.name;
        const char *receiver_name = expr_name(receiver);

        if (receiver_name != NULL && method != NULL) {
            if (strcmp(method, "Read") == 0) {
                if (!add_op(scope, RIR_OP_READ, receiver_name, NULL, NULL, node))
                    return false;
                member_slot_op_handled = true;
            } else if (strcmp(method, "Write") == 0) {
                const char *value_name = node->data.call.arg_count >= 1
                    ? expr_name(node->data.call.arguments[0])
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
        ASTNode **args = node->data.call.arguments;
        size_t argc = node->data.call.arg_count;

        if (pgy_compiler_io_boundary_builtin_is_stable(name)) {
            const char *first_arg = argc >= 1 ? expr_name(args[0]) : NULL;
            if (!add_op(scope, RIR_OP_IO, name, first_arg, NULL, node))
                return false;
        } else if (strcmp(name, "Read") == 0 && argc >= 1) {
            if (!add_op(scope, RIR_OP_READ, expr_name(args[0]), NULL, NULL, node))
                return false;
        } else if (strcmp(name, "Write") == 0 && argc >= 1) {
            if (!add_op(scope, RIR_OP_WRITE, expr_name(args[0]), argc >= 2 ? expr_name(args[1]) : NULL, NULL, node))
                return false;
        } else if (strcmp(name, "Release") == 0 && argc >= 1) {
            if (!add_op(scope, RIR_OP_RELEASE, expr_name(args[0]), NULL, NULL, node))
                return false;
        } else if (strcmp(name, "Move") == 0 && argc >= 1) {
            if (!add_op(scope, RIR_OP_MOVE, expr_name(args[0]), NULL, NULL, node))
                return false;
        } else if (strcmp(name, "ToObject") == 0 && argc >= 2) {
            if (!add_op(scope, RIR_OP_PROJECT_REFRESH, expr_name(args[1]), expr_name(args[0]), NULL, node))
                return false;
        } else if (strcmp(name, "ToTObject") == 0 && argc >= 2) {
            if (!add_op(scope, RIR_OP_PROJECT_PUBLISH, expr_name(args[1]), expr_name(args[0]), NULL, node))
                return false;
        }
    }

    if (!rir_walk_node(scope, node->data.call.callee))
        return false;
    for (size_t i = 0; i < node->data.call.arg_count; i++) {
        if (!rir_walk_node(scope, node->data.call.arguments[i]))
            return false;
    }
    return true;
}

bool
rir_walk_node(RIRScope *scope, ASTNode *node)
{
    if (scope == NULL || node == NULL)
        return true;

    switch (node->type) {
        case AST_BLOCK:
            for (size_t i = 0; i < node->data.block.count; i++) {
                if (!rir_walk_node(scope, node->data.block.statements[i]))
                    return false;
            }
            return true;

        case AST_LET_DECL: {
            const char *init_name = NULL;
            RIRResourceState state = RIR_STATE_UNINIT;
            if (node->data.let_decl.initializer != NULL
                && node->data.let_decl.initializer->type == AST_CALL) {
                init_name = call_name(node->data.let_decl.initializer);
            }

            if (init_name != NULL) {
                if (strcmp(init_name, "ClaimSlot") == 0) {
                    state = RIR_STATE_OWNED;
                    if (!add_op(scope, RIR_OP_CLAIM, node->data.let_decl.name, "Slot", NULL,
                                node->data.let_decl.initializer))
                        return false;
                } else if (strcmp(init_name, "ClaimSecureSlot") == 0) {
                    state = RIR_STATE_OWNED;
                    if (!add_op(scope, RIR_OP_CLAIM, node->data.let_decl.name, "SecureSlot", NULL,
                                node->data.let_decl.initializer))
                        return false;
                } else if (strcmp(init_name, "ClaimDeviceSlot") == 0) {
                    state = RIR_STATE_OWNED;
                    if (!add_op(scope, RIR_OP_CLAIM, node->data.let_decl.name, "DeviceSlot", NULL,
                                node->data.let_decl.initializer))
                        return false;
                } else if (strcmp(init_name, "ViewRead") == 0
                           && node->data.let_decl.initializer->data.call.arg_count >= 1) {
                    if (!add_op(scope,
                                RIR_OP_BORROW_READ,
                                expr_name(node->data.let_decl.initializer->data.call.arguments[0]),
                                node->data.let_decl.name,
                                NULL,
                                node->data.let_decl.initializer))
                        return false;
                } else if (strcmp(init_name, "ViewWrite") == 0
                           && node->data.let_decl.initializer->data.call.arg_count >= 1) {
                    if (!add_op(scope,
                                RIR_OP_BORROW_WRITE,
                                expr_name(node->data.let_decl.initializer->data.call.arguments[0]),
                                node->data.let_decl.name,
                                NULL,
                                node->data.let_decl.initializer))
                        return false;
                }
            }

            if (!add_resource_fact(scope,
                                   node->data.let_decl.name,
                                   node->data.let_decl.type,
                                   state,
                                   node))
                return false;
            return rir_walk_node(scope, node->data.let_decl.initializer);
        }

        case AST_WITH_STMT:
            if (!add_resource_fact(scope,
                                   node->data.with_stmt.alias,
                                   node->data.with_stmt.slot_type,
                                   RIR_STATE_OWNED,
                                   node))
                return false;
            if (!add_op(scope, RIR_OP_CLAIM,
                        node->data.with_stmt.alias,
                        node->data.with_stmt.is_secure ? "SecureSlot" : "Slot",
                        NULL,
                        node))
                return false;
            return rir_walk_node(scope, node->data.with_stmt.body);

        case AST_ASSIGNMENT:
            if (!rir_walk_node(scope, node->data.assignment.target))
                return false;
            return rir_walk_node(scope, node->data.assignment.value);

        case AST_AWAIT_EXPR:
            if (!add_op(scope, RIR_OP_AWAIT_REMOTE,
                        expr_name(node->data.await_expr.expression),
                        NULL, NULL, node))
                return false;
            return rir_walk_node(scope, node->data.await_expr.expression);

        case AST_SPAWN_EXPR:
            if (!add_op(scope, RIR_OP_SPAWN, "spawn", NULL, NULL, node))
                return false;
            if (!rir_walk_node(scope, node->data.spawn_expr.function))
                return false;
            for (size_t i = 0; i < node->data.spawn_expr.arg_count; i++) {
                if (!rir_walk_node(scope, node->data.spawn_expr.arguments[i]))
                    return false;
            }
            return true;

        case AST_CHANNEL_SEND:
            if (!add_op(scope,
                        RIR_OP_CHANNEL_SEND,
                        expr_name(node->data.channel_send.channel),
                        expr_name(node->data.channel_send.value),
                        NULL,
                        node))
                return false;
            if (!rir_walk_node(scope, node->data.channel_send.channel))
                return false;
            return rir_walk_node(scope, node->data.channel_send.value);

        case AST_CHANNEL_RECV:
            if (!add_op(scope,
                        RIR_OP_CHANNEL_RECV,
                        expr_name(node->data.channel_recv.channel),
                        NULL,
                        NULL,
                        node))
                return false;
            return rir_walk_node(scope, node->data.channel_recv.channel);

        case AST_SELECT_STMT:
            if (!add_op(scope, RIR_OP_CHANNEL_SELECT, "select", NULL, NULL, node))
                return false;
            for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
                if (!rir_walk_node(scope, node->data.select_stmt.cases[i]))
                    return false;
            }
            return rir_walk_node(scope, node->data.select_stmt.default_case);

        case AST_CALL:
            return rir_walk_call(scope, node);

        case AST_IF_STMT:
            if (!rir_walk_node(scope, node->data.if_stmt.condition))
                return false;
            if (!rir_walk_node(scope, node->data.if_stmt.then_branch))
                return false;
            return rir_walk_node(scope, node->data.if_stmt.else_branch);

        case AST_FOR_LOOP:
            if (!rir_walk_node(scope, node->data.for_loop.range_start))
                return false;
            if (!rir_walk_node(scope, node->data.for_loop.range_end))
                return false;
            if (!rir_walk_node(scope, node->data.for_loop.iterable))
                return false;
            return rir_walk_node(scope, node->data.for_loop.body);

        case AST_WHILE_LOOP:
            if (!rir_walk_node(scope, node->data.while_loop.condition))
                return false;
            return rir_walk_node(scope, node->data.while_loop.body);

        case AST_RETURN:
            return rir_walk_node(scope, node->data.return_stmt.value);

        case AST_MATCH_STMT:
            if (!rir_walk_node(scope, node->data.match_stmt.subject))
                return false;
            for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
                if (!rir_walk_node(scope, node->data.match_stmt.cases[i]))
                    return false;
            }
            if (!rir_walk_node(scope, node->data.match_stmt.default_body))
                return false;
            return true;

        case AST_MATCH_CASE:
            if (!rir_walk_node(scope, node->data.match_case.pattern))
                return false;
            if (!rir_walk_node(scope, node->data.match_case.guard))
                return false;
            return rir_walk_node(scope, node->data.match_case.body);

        case AST_PARALLEL_BLOCK:
            if (!add_op(scope, RIR_OP_PARALLEL, "parallel", NULL, NULL, node))
                return false;
            for (size_t i = 0; i < node->data.parallel.task_count; i++) {
                if (!rir_walk_node(scope, node->data.parallel.tasks[i]))
                    return false;
            }
            return true;

        case AST_ASYNC_BLOCK:
            if (!add_op(scope, RIR_OP_ASYNC, "async", NULL, NULL, node))
                return false;
            for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
                if (!rir_walk_node(scope, node->data.async_block.statements[i]))
                    return false;
            }
            return true;

        case AST_TASK_GROUP:
            if (!add_op(scope, RIR_OP_TASK_GROUP, "task-group", NULL, NULL, node))
                return false;
            for (size_t i = 0; i < node->data.task_group.task_count; i++) {
                if (!rir_walk_node(scope, node->data.task_group.tasks[i]))
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
    if (scope == NULL || node == NULL)
        return true;

    switch (node->type) {
        case AST_IF_STMT:
            return rir_walk_block_node(scope, node->data.if_stmt.condition);
        case AST_FOR_LOOP:
            return rir_walk_block_node(scope, node->data.for_loop.range_start)
                   && rir_walk_block_node(scope, node->data.for_loop.range_end)
                   && rir_walk_block_node(scope, node->data.for_loop.iterable);
        case AST_WHILE_LOOP:
            return rir_walk_block_node(scope, node->data.while_loop.condition);
        case AST_MATCH_STMT:
            if (!rir_walk_block_node(scope, node->data.match_stmt.subject))
                return false;
            for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
                ASTNode *match_case = node->data.match_stmt.cases[i];
                if (match_case == NULL)
                    continue;
                if (!rir_walk_block_node(scope, match_case->data.match_case.pattern)
                    || !rir_walk_block_node(scope, match_case->data.match_case.guard)) {
                    return false;
                }
            }
            return true;
        case AST_BLOCK:
            for (size_t i = 0; i < node->data.block.count; i++) {
                if (!rir_walk_block_node(scope, node->data.block.statements[i]))
                    return false;
            }
            return true;
        default:
            return rir_walk_node(scope, node);
    }
}
