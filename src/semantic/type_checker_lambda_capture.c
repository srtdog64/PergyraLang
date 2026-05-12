#include "type_checker_internal.h"
#include "diag_codes.h"

#include <string.h>

typedef struct CaptureLocal {
    const char *name;
    const struct CaptureLocal *parent;
} CaptureLocal;

typedef struct {
    Scope *lambda_scope;
    const CaptureLocal *locals;
} CaptureState;

static bool
capture_state_has_local(const CaptureState *state, const char *name)
{
    if (state == NULL || name == NULL)
        return false;
    for (const CaptureLocal *cur = state->locals; cur != NULL; cur = cur->parent) {
        if (cur->name != NULL && strcmp(cur->name, name) == 0)
            return true;
    }
    return false;
}

static Scope *
find_symbol_scope(Scope *scope, const Symbol *symbol)
{
    for (Scope *cur = scope; cur != NULL; cur = cur->parent) {
        for (size_t i = 0; i < cur->symbol_count; i++) {
            if (cur->symbols[i] == symbol)
                return cur;
        }
    }
    return NULL;
}

static bool
symbol_is_capturable_global(const Symbol *sym, const Scope *owner)
{
    if (sym == NULL || owner == NULL || owner->kind != SCOPE_GLOBAL)
        return false;
    return sym->kind == SYMBOL_FUNCTION
        || sym->kind == SYMBOL_CLASS
        || sym->kind == SYMBOL_ABILITY
        || sym->kind == SYMBOL_ROLE
        || sym->kind == SYMBOL_PARTY
        || sym->kind == SYMBOL_ROSTER
        || sym->kind == SYMBOL_WORLD
        || sym->kind == SYMBOL_INTENT
        || sym->kind == SYMBOL_RELATION
        || sym->kind == SYMBOL_EFFECT
        || sym->kind == SYMBOL_ZONE;
}

static bool
reject_identifier_capture(ASTNode *node, SemanticContext *ctx,
    const CaptureState *state)
{
    const char *name;
    Symbol *outer;
    Scope *owner;
    Scope *lambda_scope = state != NULL ? state->lambda_scope : NULL;

    if (node == NULL || node->type != AST_IDENTIFIER
        || ctx == NULL || lambda_scope == NULL)
        return false;
    name = node->data.identifier.name;
    if (name == NULL || capture_state_has_local(state, name)
        || scope_lookup_current(lambda_scope, name) != NULL)
        return false;

    outer = lambda_scope->parent != NULL ? scope_lookup(lambda_scope->parent, name) : NULL;
    if (outer == NULL)
        return false;

    owner = find_symbol_scope(lambda_scope->parent, outer);
    if (symbol_is_capturable_global(outer, owner))
        return false;

    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_MOVE_INTO_ASYNC_FUNCTION,
        node,
        "Lambda capture of local value '%s' is reserved but not implemented.\n"
        "Reason:\n"
        "- beta lambdas lower to standalone callable bodies without a closure environment\n"
        "- capturing local, slot, token, authority, or other boundary values would make lifetime and cleanup ownership implicit\n"
        "Fix:\n"
        "- pass '%s' as an explicit lambda parameter\n"
        "- or move the logic into a named function/action with an explicit contract",
        name, name);
    return true;
}

static bool reject_lambda_captures(ASTNode *node, SemanticContext *ctx,
    const CaptureState *state);

static bool
reject_list(ASTNode *const *items, size_t count, SemanticContext *ctx,
    const CaptureState *state)
{
    for (size_t i = 0; i < count; i++) {
        if (reject_lambda_captures(items[i], ctx, state))
            return true;
    }
    return false;
}

static bool
reject_match_case_captures(ASTNode *node, SemanticContext *ctx,
    const CaptureState *state)
{
    if (reject_lambda_captures(node->data.match_case.pattern, ctx, state))
        return true;
    if (reject_list(node->data.match_case.patterns,
            node->data.match_case.pattern_count, ctx, state))
        return true;
    return reject_lambda_captures(node->data.match_case.guard, ctx, state)
        || reject_lambda_captures(node->data.match_case.body, ctx, state);
}

static bool reject_block_from(ASTNode *const *items, size_t count, size_t index,
    SemanticContext *ctx, const CaptureState *state);

static bool
reject_block_after_destructure_names(ASTNode *const *items, size_t count,
    size_t index, char *const *names, size_t name_count, size_t name_index,
    SemanticContext *ctx, const CaptureState *state)
{
    if (name_index >= name_count)
        return reject_block_from(items, count, index, ctx, state);

    CaptureLocal local = { names[name_index], state != NULL ? state->locals : NULL };
    CaptureState next = {
        state != NULL ? state->lambda_scope : NULL,
        &local,
    };
    return reject_block_after_destructure_names(items, count, index, names,
        name_count, name_index + 1, ctx, &next);
}

static bool
reject_block_from(ASTNode *const *items, size_t count, size_t index,
    SemanticContext *ctx, const CaptureState *state)
{
    if (items == NULL || index >= count)
        return false;

    ASTNode *stmt = items[index];
    if (stmt != NULL && stmt->type == AST_LET_DECL) {
        if (reject_lambda_captures(stmt->data.let_decl.initializer, ctx, state))
            return true;
        CaptureLocal local = { stmt->data.let_decl.name,
            state != NULL ? state->locals : NULL };
        CaptureState next = {
            state != NULL ? state->lambda_scope : NULL,
            &local,
        };
        return reject_block_from(items, count, index + 1, ctx, &next);
    }
    if (stmt != NULL && stmt->type == AST_LET_DESTRUCTURE) {
        if (reject_lambda_captures(stmt->data.let_destructure.initializer,
                ctx, state))
            return true;
        return reject_block_after_destructure_names(items, count, index + 1,
            stmt->data.let_destructure.names,
            stmt->data.let_destructure.name_count, 0, ctx, state);
    }
    if (reject_lambda_captures(stmt, ctx, state))
        return true;
    return reject_block_from(items, count, index + 1, ctx, state);
}

static bool
reject_lambda_captures(ASTNode *node, SemanticContext *ctx,
    const CaptureState *state)
{
    if (node == NULL || ctx == NULL || state == NULL || state->lambda_scope == NULL)
        return false;

    switch (node->type) {
    case AST_IDENTIFIER:
        return reject_identifier_capture(node, ctx, state);
    case AST_BLOCK:
        return reject_block_from(node->data.block.statements,
            node->data.block.count, 0, ctx, state);
    case AST_ASYNC_BLOCK:
        return reject_block_from(node->data.async_block.statements,
            node->data.async_block.statement_count, 0, ctx, state);
    case AST_PARALLEL_BLOCK:
        return reject_list(node->data.parallel.tasks,
            node->data.parallel.task_count, ctx, state);
    case AST_TASK_GROUP:
        return reject_list(node->data.task_group.tasks,
            node->data.task_group.task_count, ctx, state);
    case AST_WITH_STMT:
        return reject_lambda_captures(node->data.with_stmt.slot_type, ctx, state)
            || reject_lambda_captures(node->data.with_stmt.body, ctx, state);
    case AST_LET_DECL:
        return reject_lambda_captures(node->data.let_decl.initializer, ctx, state);
    case AST_LET_DESTRUCTURE:
        return reject_lambda_captures(node->data.let_destructure.initializer,
            ctx, state);
    case AST_RETURN:
        return reject_lambda_captures(node->data.return_stmt.value, ctx, state);
    case AST_BINARY:
        return reject_lambda_captures(node->data.binary.left, ctx, state)
            || reject_lambda_captures(node->data.binary.right, ctx, state);
    case AST_UNARY:
        return reject_lambda_captures(node->data.unary.operand, ctx, state);
    case AST_CALL:
        return reject_lambda_captures(node->data.call.callee, ctx, state)
            || reject_list(node->data.call.arguments, node->data.call.arg_count,
                ctx, state);
    case AST_MEMBER_ACCESS:
        return reject_lambda_captures(node->data.member.object, ctx, state);
    case AST_ARRAY_ACCESS:
        return reject_lambda_captures(node->data.array_access.array, ctx, state)
            || reject_lambda_captures(node->data.array_access.index, ctx, state);
    case AST_ARRAY_LITERAL:
        return reject_list(node->data.array_literal.elements,
            node->data.array_literal.count, ctx, state);
    case AST_TUPLE_LITERAL:
        return reject_list(node->data.tuple_literal.elements,
            node->data.tuple_literal.count, ctx, state);
    case AST_ASSIGNMENT:
        return reject_lambda_captures(node->data.assignment.target, ctx, state)
            || reject_lambda_captures(node->data.assignment.value, ctx, state);
    case AST_IF_STMT:
        return reject_lambda_captures(node->data.if_stmt.condition, ctx, state)
            || reject_lambda_captures(node->data.if_stmt.then_branch, ctx, state)
            || reject_lambda_captures(node->data.if_stmt.else_branch, ctx, state);
    case AST_WHILE_LOOP:
        return reject_lambda_captures(node->data.while_loop.condition, ctx, state)
            || reject_lambda_captures(node->data.while_loop.body, ctx, state);
    case AST_FOR_LOOP:
        if (reject_lambda_captures(node->data.for_loop.range_start, ctx, state)
            || reject_lambda_captures(node->data.for_loop.range_end, ctx, state)
            || reject_lambda_captures(node->data.for_loop.iterable, ctx, state)) {
            return true;
        }
        if (node->data.for_loop.variable == NULL)
            return reject_lambda_captures(node->data.for_loop.body, ctx, state);
        {
            CaptureLocal local = { node->data.for_loop.variable, state->locals };
            CaptureState next = { state->lambda_scope, &local };
            return reject_lambda_captures(node->data.for_loop.body, ctx, &next);
        }
    case AST_MATCH_STMT:
        return reject_lambda_captures(node->data.match_stmt.subject, ctx, state)
            || reject_list(node->data.match_stmt.cases, node->data.match_stmt.case_count,
                ctx, state)
            || reject_lambda_captures(node->data.match_stmt.default_body, ctx, state);
    case AST_MATCH_CASE:
        return reject_match_case_captures(node, ctx, state);
    case AST_SELECT_STMT:
        return reject_list(node->data.select_stmt.cases, node->data.select_stmt.case_count,
            ctx, state)
            || reject_lambda_captures(node->data.select_stmt.default_case, ctx, state);
    case AST_AWAIT_EXPR:
        return reject_lambda_captures(node->data.await_expr.expression, ctx, state);
    case AST_CHANNEL_SEND:
        return reject_lambda_captures(node->data.channel_send.channel, ctx, state)
            || reject_lambda_captures(node->data.channel_send.value, ctx, state);
    case AST_CHANNEL_RECV:
        return reject_lambda_captures(node->data.channel_recv.channel, ctx, state);
    case AST_SPAWN_EXPR:
        return reject_lambda_captures(node->data.spawn_expr.function, ctx, state)
            || reject_list(node->data.spawn_expr.arguments, node->data.spawn_expr.arg_count,
                ctx, state);
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return reject_lambda_captures(node->data.event_op.event, ctx, state)
            || reject_lambda_captures(node->data.event_op.handler, ctx, state);
    case AST_EVENT_INVOKE:
        return reject_lambda_captures(node->data.event_invoke.event, ctx, state)
            || reject_list(node->data.event_invoke.arguments,
                node->data.event_invoke.arg_count, ctx, state);
    case AST_UNSAFE_BLOCK:
        return reject_lambda_captures(node->data.unsafe_block.body, ctx, state);
    case AST_DEFER_STMT:
        return reject_lambda_captures(node->data.defer_stmt.body, ctx, state);
    default:
        return false;
    }
}

bool
semantic_reject_lambda_unsupported_captures(ASTNode *body, SemanticContext *ctx)
{
    CaptureState state = {
        ctx != NULL ? ctx->scope : NULL,
        NULL,
    };
    return reject_lambda_captures(body, ctx, &state);
}
