/*
 * Copyright (c) 2026 Pergyra Language Project
 * Index-discipline analysis for the join-form parallel block
 * (docs/181 R1): every occurrence of an admitted array name inside the
 * replicated body must be exactly `name[binding]` -- the one shape whose
 * reads and writes land on the task-owned element (index-disjointness
 * evidence). The rule is alias-robust: even if two admitted names alias
 * the same backing storage, every access still lands on element i within
 * task i, so no cross-task overlap can arise.
 *
 * Fail-closed walk: `name[binding]` is admitted only through node kinds
 * this switch knows. For every other kind the exhaustive free-ref walker
 * decides whether the subtree mentions the name at all -- an unknown
 * statement shape can over-reject, never over-admit.
 */

#include "ast_analysis.h"

#include <string.h>

static bool
index_walk_identifier_named(const ASTNode *node, const char *name)
{
    return node != NULL && node->type == AST_IDENTIFIER
        && node->data.identifier.name != NULL
        && strcmp(node->data.identifier.name, name) == 0;
}

bool
ast_identifier_only_indexed_by(const ASTNode *node, const char *name,
                               const char *index_name)
{
    if (node == NULL)
        return true;
    if (name == NULL || index_name == NULL)
        return false;

    switch (node->type) {
    case AST_IDENTIFIER:
        /* A bare occurrence outside `name[index]` breaks the discipline. */
        return !index_walk_identifier_named(node, name);
    case AST_ARRAY_ACCESS: {
        const ASTNode *arr = node->data.array_access.array;
        const ASTNode *idx = node->data.array_access.index;

        if (index_walk_identifier_named(arr, name))
            return index_walk_identifier_named(idx, index_name);
        return ast_identifier_only_indexed_by(arr, name, index_name)
            && ast_identifier_only_indexed_by(idx, name, index_name);
    }
    case AST_ASSIGNMENT:
        return ast_identifier_only_indexed_by(
                   node->data.assignment.target, name, index_name)
            && ast_identifier_only_indexed_by(
                   node->data.assignment.value, name, index_name);
    case AST_BINARY:
        return ast_identifier_only_indexed_by(
                   node->data.binary.left, name, index_name)
            && ast_identifier_only_indexed_by(
                   node->data.binary.right, name, index_name);
    case AST_UNARY:
        return ast_identifier_only_indexed_by(
                   node->data.unary.operand, name, index_name);
    case AST_CALL: {
        /* The callee position is walked too: `name(...)` or a bare `name`
         * argument (ArrayPush(name, v)) fails through the identifier case. */
        if (!ast_identifier_only_indexed_by(
                node->data.call.callee, name, index_name))
            return false;
        for (size_t i = 0; i < node->data.call.arg_count; i++) {
            if (!ast_identifier_only_indexed_by(
                    node->data.call.arguments[i], name, index_name))
                return false;
        }
        return true;
    }
    case AST_MEMBER_ACCESS:
        /* `name.Method` is a whole-array use; the object walk rejects it. */
        return ast_identifier_only_indexed_by(
            node->data.member.object, name, index_name);
    case AST_CHANNEL_SEND:
        return ast_identifier_only_indexed_by(
                   node->data.channel_send.channel, name, index_name)
            && ast_identifier_only_indexed_by(
                   node->data.channel_send.value, name, index_name);
    case AST_CHANNEL_RECV:
        return ast_identifier_only_indexed_by(
            node->data.channel_recv.channel, name, index_name);
    case AST_LET_DECL:
        /* Body-local rebinding of the admitted name is not modelled; the
         * initializer walk keeps any use of the outer name disciplined
         * and the checker's shadow rules own the binding question. */
        return ast_identifier_only_indexed_by(
            node->data.let_decl.initializer, name, index_name);
    case AST_BLOCK: {
        for (size_t i = 0; i < node->data.block.count; i++) {
            if (!ast_identifier_only_indexed_by(
                    node->data.block.statements[i], name, index_name))
                return false;
        }
        return true;
    }
    case AST_IF_STMT:
        return ast_identifier_only_indexed_by(
                   node->data.if_stmt.condition, name, index_name)
            && ast_identifier_only_indexed_by(
                   node->data.if_stmt.then_branch, name, index_name)
            && ast_identifier_only_indexed_by(
                   node->data.if_stmt.else_branch, name, index_name);
    case AST_WHILE_LOOP:
        return ast_identifier_only_indexed_by(
                   node->data.while_loop.condition, name, index_name)
            && ast_identifier_only_indexed_by(
                   node->data.while_loop.body, name, index_name);
    case AST_FOR_LOOP:
        return ast_identifier_only_indexed_by(
                   node->data.for_loop.range_start, name, index_name)
            && ast_identifier_only_indexed_by(
                   node->data.for_loop.range_end, name, index_name)
            && ast_identifier_only_indexed_by(
                   node->data.for_loop.iterable, name, index_name)
            && ast_identifier_only_indexed_by(
                   node->data.for_loop.body, name, index_name);
    default:
        return !ast_contains_free_identifier_ref(node, name);
    }
}
