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
    ASTNode *lambda; /* lambda node receiving recorded captures */
} CaptureState;

/* Stage A: a value-type local is captured by copy (snapshot at closure
 * creation). Primitives (Int/Long/Bool/Float/Double/String) have no aliasing
 * or lifetime link, so the copy can never dangle. Anything else (slot, token,
 * authority, constructed/aggregate types) is not yet capturable. */
static bool
capture_symbol_is_copy_value(const Symbol *sym)
{
    return sym != NULL
        && sym->kind == SYMBOL_VARIABLE
        && sym->type != NULL
        && sym->type->kind == TYPE_KIND_PRIMITIVE
        && sym->type->name != NULL;
}

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
    name = ast_identifier_name(node);
    if (name == NULL || capture_state_has_local(state, name)
        || scope_lookup_current(lambda_scope, name) != NULL)
        return false;

    outer = lambda_scope->parent != NULL ? scope_lookup(lambda_scope->parent, name) : NULL;
    if (outer == NULL)
        return false;

    owner = find_symbol_scope(lambda_scope->parent, outer);
    if (symbol_is_capturable_global(outer, owner))
        return false;

    /* Stage A (docs/135): value-type locals are capture-by-copy candidates.
     * Record the capture on the lambda node so the closure environment ABI can
     * consume it. The recording is dormant until both backends emit the
     * environment: until then we still fail closed, because emitting a hoisted
     * body that references an uncaptured local would generate broken C/LLVM (a
     * silent codegen trap). The reject flips to allow once the env path lands. */
    if (capture_symbol_is_copy_value(outer) && state->lambda != NULL)
        (void)ast_lambda_add_capture(state->lambda, name, outer->type->name,
            LAMBDA_CAPTURE_COPY);

    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_MOVE_INTO_ASYNC_FUNCTION,
        node,
        "Lambda capture of local '%s' is not yet enabled.\n"
        "Reason:\n"
        "- value-type locals (Int/Long/Bool/Float/Double/String) are capture-by-copy candidates but the closure environment ABI is not yet wired in both backends\n"
        "- capturing slot, token, authority, or aggregate values would make lifetime and cleanup ownership implicit\n"
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
    size_t pattern_count = 0;
    ASTNode **patterns = ast_match_case_patterns(node, &pattern_count);
    if (reject_lambda_captures(ast_match_case_pattern(node), ctx, state))
        return true;
    if (reject_list(patterns, pattern_count, ctx, state))
        return true;
    return reject_lambda_captures(ast_match_case_guard(node), ctx, state)
        || reject_lambda_captures(ast_match_case_body(node), ctx, state);
}

static bool reject_block_from(ASTNode *const *items, size_t count, size_t index,
    SemanticContext *ctx, const CaptureState *state);

static bool
reject_block_after_destructure_names(ASTNode *const *items, size_t count,
    size_t index, const ASTNode *destructure, size_t name_index,
    SemanticContext *ctx, const CaptureState *state)
{
    if (name_index >= ast_let_destructure_name_count(destructure))
        return reject_block_from(items, count, index, ctx, state);

    CaptureLocal local = {
        ast_let_destructure_name(destructure, name_index),
        state != NULL ? state->locals : NULL
    };
    CaptureState next = {
        state != NULL ? state->lambda_scope : NULL,
        &local,
    };
    return reject_block_after_destructure_names(items, count, index,
        destructure, name_index + 1, ctx, &next);
}

static bool
reject_block_from(ASTNode *const *items, size_t count, size_t index,
    SemanticContext *ctx, const CaptureState *state)
{
    if (items == NULL || index >= count)
        return false;

    ASTNode *stmt = items[index];
    if (stmt != NULL && stmt->type == AST_LET_DECL) {
        if (reject_lambda_captures(ast_let_initializer(stmt), ctx, state))
            return true;
        CaptureLocal local = { ast_let_name(stmt),
            state != NULL ? state->locals : NULL };
        CaptureState next = {
            state != NULL ? state->lambda_scope : NULL,
            &local,
        };
        return reject_block_from(items, count, index + 1, ctx, &next);
    }
    if (stmt != NULL && stmt->type == AST_LET_DESTRUCTURE) {
        if (reject_lambda_captures(ast_let_destructure_initializer(stmt),
                ctx, state))
            return true;
        return reject_block_after_destructure_names(items, count, index + 1,
            stmt, 0, ctx, state);
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
        {
            size_t statement_count = 0;
            ASTNode **statements = ast_block_statements(node, &statement_count);
            return reject_block_from(statements, statement_count, 0, ctx, state);
        }
    case AST_ASYNC_BLOCK:
        {
            size_t statement_count = 0;
            ASTNode **statements =
                ast_async_block_statements(node, &statement_count);
            return reject_block_from(statements, statement_count, 0, ctx, state);
        }
    case AST_PARALLEL_BLOCK:
        {
            size_t task_count = 0;
            ASTNode **tasks = ast_parallel_tasks(node, &task_count);
            return reject_list(tasks, task_count, ctx, state);
        }
    case AST_TASK_GROUP:
        {
            size_t task_count = 0;
            ASTNode **tasks = ast_task_group_tasks(node, &task_count);
            return reject_list(tasks, task_count, ctx, state);
        }
    case AST_WITH_STMT:
        return reject_lambda_captures(ast_with_slot_type(node), ctx, state)
            || reject_lambda_captures(ast_with_body(node), ctx, state);
    case AST_LET_DECL:
        return reject_lambda_captures(ast_let_initializer(node), ctx, state);
    case AST_LET_DESTRUCTURE:
        return reject_lambda_captures(
            ast_let_destructure_initializer(node), ctx, state);
    case AST_RETURN:
        return reject_lambda_captures(ast_return_value(node), ctx, state);
    case AST_BINARY:
        return reject_lambda_captures(ast_binary_left(node), ctx, state)
            || reject_lambda_captures(ast_binary_right(node), ctx, state);
    case AST_UNARY:
        return reject_lambda_captures(ast_unary_operand(node), ctx, state);
    case AST_CALL:
        {
            size_t arg_count = 0;
            ASTNode **args = ast_call_arguments(node, &arg_count);
            return reject_lambda_captures(ast_call_callee(node), ctx, state)
                || reject_list(args, arg_count, ctx, state);
        }
    case AST_MEMBER_ACCESS:
        return reject_lambda_captures(ast_member_object(node), ctx, state);
    case AST_ARRAY_ACCESS:
        return reject_lambda_captures(ast_array_access_array(node), ctx, state)
            || reject_lambda_captures(ast_array_access_index(node), ctx, state);
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(node); i++) {
            if (reject_lambda_captures(ast_array_literal_element(node, i), ctx, state))
                return true;
        }
        return false;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < ast_tuple_literal_count(node); i++) {
            if (reject_lambda_captures(ast_tuple_literal_element(node, i), ctx, state))
                return true;
        }
        return false;
    case AST_ASSIGNMENT:
        return reject_lambda_captures(ast_assignment_target(node), ctx, state)
            || reject_lambda_captures(ast_assignment_value(node), ctx, state);
    case AST_IF_STMT:
        return reject_lambda_captures(ast_if_condition(node), ctx, state)
            || reject_lambda_captures(ast_if_then_branch(node), ctx, state)
            || reject_lambda_captures(ast_if_else_branch(node), ctx, state);
    case AST_WHILE_LOOP:
        return reject_lambda_captures(ast_while_condition(node), ctx, state)
            || reject_lambda_captures(ast_while_body(node), ctx, state);
    case AST_FOR_LOOP:
        if (reject_lambda_captures(ast_for_range_start(node), ctx, state)
            || reject_lambda_captures(ast_for_range_end(node), ctx, state)
            || reject_lambda_captures(ast_for_iterable(node), ctx, state)) {
            return true;
        }
        if (ast_for_variable(node) == NULL)
            return reject_lambda_captures(ast_for_body(node), ctx, state);
        {
            CaptureLocal local = { ast_for_variable(node), state->locals };
            CaptureState next = { state->lambda_scope, &local };
            return reject_lambda_captures(ast_for_body(node), ctx, &next);
        }
    case AST_MATCH_STMT:
        return reject_lambda_captures(ast_match_subject(node), ctx, state)
            || reject_list(ast_match_cases(node, NULL), ast_match_case_count(node),
                ctx, state)
            || reject_lambda_captures(ast_match_default_body(node), ctx, state);
    case AST_MATCH_CASE:
        return reject_match_case_captures(node, ctx, state);
    case AST_SELECT_STMT:
        {
            size_t case_count = 0;
            ASTNode **cases = ast_select_cases(node, &case_count);
            return reject_list(cases, case_count, ctx, state)
                || reject_lambda_captures(ast_select_default_case(node), ctx, state);
        }
    case AST_AWAIT_EXPR:
        return reject_lambda_captures(ast_await_expression(node), ctx, state);
    case AST_CHANNEL_SEND:
        return reject_lambda_captures(ast_channel_send_channel(node), ctx, state)
            || reject_lambda_captures(ast_channel_send_value(node), ctx, state);
    case AST_CHANNEL_RECV:
        return reject_lambda_captures(ast_channel_recv_channel(node), ctx, state);
    case AST_SPAWN_EXPR:
        {
            size_t arg_count = 0;
            ASTNode **args = ast_spawn_arguments(node, &arg_count);
            return reject_lambda_captures(ast_spawn_function(node), ctx, state)
                || reject_list(args, arg_count, ctx, state);
        }
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return reject_lambda_captures(ast_event_op_event(node), ctx, state)
            || reject_lambda_captures(ast_event_op_handler(node), ctx, state);
    case AST_EVENT_INVOKE:
        return reject_lambda_captures(ast_event_invoke_event(node), ctx, state)
            || reject_list(ast_event_invoke_arguments(node, NULL),
                ast_event_invoke_arg_count(node), ctx, state);
    case AST_UNSAFE_BLOCK:
        return reject_lambda_captures(ast_unsafe_block_body(node), ctx, state);
    case AST_TRANSACTION_BLOCK:
        return reject_lambda_captures(ast_transaction_block_body(node), ctx, state);
    case AST_DEFER_STMT:
        return reject_lambda_captures(ast_defer_body(node), ctx, state);
    default:
        return false;
    }
}

bool
semantic_reject_lambda_unsupported_captures(ASTNode *lambda, SemanticContext *ctx)
{
    CaptureState state = {
        ctx != NULL ? ctx->scope : NULL,
        NULL,
        lambda,
    };
    return reject_lambda_captures(ast_lambda_body(lambda), ctx, &state);
}
