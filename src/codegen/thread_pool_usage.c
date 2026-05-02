/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Shared C/LLVM runtime thread-pool dependency analysis.
 */

#include "thread_pool_usage.h"

#include <stddef.h>

bool
pgy_ast_uses_thread_pool(const ASTNode *node)
{
    if (node == NULL)
        return false;

    switch (node->type) {
    case AST_PARALLEL_BLOCK:
    case AST_ASYNC_BLOCK:
    case AST_SPAWN_EXPR:
    case AST_AWAIT_EXPR:
    case AST_TASK_GROUP:
        return true;
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++) {
            if (pgy_ast_uses_thread_pool(node->data.block.statements[i]))
                return true;
        }
        return false;
    case AST_LET_DECL:
        return pgy_ast_uses_thread_pool(node->data.let_decl.type)
            || pgy_ast_uses_thread_pool(node->data.let_decl.initializer);
    case AST_RETURN:
        return pgy_ast_uses_thread_pool(node->data.return_stmt.value);
    case AST_CALL:
        if (pgy_ast_uses_thread_pool(node->data.call.callee))
            return true;
        for (size_t i = 0; i < node->data.call.arg_count; i++) {
            if (pgy_ast_uses_thread_pool(node->data.call.arguments[i]))
                return true;
        }
        return false;
    case AST_BINARY:
        return pgy_ast_uses_thread_pool(node->data.binary.left)
            || pgy_ast_uses_thread_pool(node->data.binary.right);
    case AST_UNARY:
        return pgy_ast_uses_thread_pool(node->data.unary.operand);
    case AST_ASSIGNMENT:
        return pgy_ast_uses_thread_pool(node->data.assignment.target)
            || pgy_ast_uses_thread_pool(node->data.assignment.value);
    case AST_MEMBER_ACCESS:
        return pgy_ast_uses_thread_pool(node->data.member.object);
    case AST_ARRAY_ACCESS:
        return pgy_ast_uses_thread_pool(node->data.array_access.array)
            || pgy_ast_uses_thread_pool(node->data.array_access.index);
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < node->data.array_literal.count; i++) {
            if (pgy_ast_uses_thread_pool(node->data.array_literal.elements[i]))
                return true;
        }
        return false;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < node->data.tuple_literal.count; i++) {
            if (pgy_ast_uses_thread_pool(node->data.tuple_literal.elements[i]))
                return true;
        }
        return false;
    case AST_IF_STMT:
        return pgy_ast_uses_thread_pool(node->data.if_stmt.condition)
            || pgy_ast_uses_thread_pool(node->data.if_stmt.then_branch)
            || pgy_ast_uses_thread_pool(node->data.if_stmt.else_branch);
    case AST_FOR_LOOP:
        return pgy_ast_uses_thread_pool(node->data.for_loop.range_start)
            || pgy_ast_uses_thread_pool(node->data.for_loop.range_end)
            || pgy_ast_uses_thread_pool(node->data.for_loop.iterable)
            || pgy_ast_uses_thread_pool(node->data.for_loop.body);
    case AST_WHILE_LOOP:
        return pgy_ast_uses_thread_pool(node->data.while_loop.condition)
            || pgy_ast_uses_thread_pool(node->data.while_loop.body);
    case AST_MATCH_STMT:
        if (pgy_ast_uses_thread_pool(node->data.match_stmt.subject)
            || pgy_ast_uses_thread_pool(node->data.match_stmt.default_body))
            return true;
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
            if (pgy_ast_uses_thread_pool(node->data.match_stmt.cases[i]))
                return true;
        }
        return false;
    case AST_MATCH_CASE:
        return pgy_ast_uses_thread_pool(node->data.match_case.pattern)
            || pgy_ast_uses_thread_pool(node->data.match_case.guard)
            || pgy_ast_uses_thread_pool(node->data.match_case.body);
    case AST_SELECT_STMT:
        for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
            if (pgy_ast_uses_thread_pool(node->data.select_stmt.cases[i]))
                return true;
        }
        return pgy_ast_uses_thread_pool(node->data.select_stmt.default_case);
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return pgy_ast_uses_thread_pool(node->data.event_op.event)
            || pgy_ast_uses_thread_pool(node->data.event_op.handler);
    case AST_EVENT_INVOKE:
        if (pgy_ast_uses_thread_pool(node->data.event_invoke.event))
            return true;
        for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
            if (pgy_ast_uses_thread_pool(node->data.event_invoke.arguments[i]))
                return true;
        }
        return false;
    default:
        return false;
    }
}

static bool
pgy_mir_instruction_uses_thread_pool(const MIRInstruction *inst)
{
    if (inst == NULL)
        return false;

    return pgy_ast_uses_thread_pool(inst->ast)
        || pgy_ast_uses_thread_pool(inst->expr0)
        || pgy_ast_uses_thread_pool(inst->expr1);
}

bool
pgy_mir_routine_uses_thread_pool(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];

        for (size_t j = 0; j < block->instruction_count; j++) {
            if (pgy_mir_instruction_uses_thread_pool(&block->instructions[j]))
                return true;
        }

        /*
         * Compatibility fallback for source-only MIR blocks. New lowering
         * should make feature-use decisions from instruction-carried facts.
         */
        if (pgy_ast_uses_thread_pool(block->source_terminator_condition)
            || pgy_ast_uses_thread_pool(block->source_terminator_value)) {
            return true;
        }

        for (size_t j = 0; j < block->source_statement_count; j++) {
            if (pgy_ast_uses_thread_pool(block->source_statements[j]))
                return true;
        }
    }

    return false;
}
