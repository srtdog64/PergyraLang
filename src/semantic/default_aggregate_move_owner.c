/* Default-mode aggregate move owner.
 *
 * A default-mode parameter of aggregate type is a shallow copy: the callee
 * and the caller share its Array storage. When the callee's parameter-flow
 * summary says it changes that storage in place or lets it out, the call
 * moves the argument (docs/truth/decisions/2026-10-18_default_aggregate_move.md).
 *
 * This owner runs once the summaries are final. It walks each routine body
 * in execution order, records the access path an argument names when the
 * call moves it, and refuses a later use of that path, of a part of it, or
 * of a value containing it, until an assignment re-initializes the path.
 * Branches merge by union; a loop body is walked twice so a use in the next
 * iteration sees a move made later in the body. */

#include "default_aggregate_move_owner.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "function_param_flow_summary_internal.h"
#include "function_param_publication_owner.h"
#include "slot_summary.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"

typedef struct {
    uint32_t binding;
    char *fields;          /* ".a.b" below the binding; "" is the whole value */
    const char *callee;
    const char *param;
    /* The read-and-return call that lent this path, or NULL for a move. */
    ASTNode *lending_call;
} MovedPath;

typedef struct {
    MovedPath *rows;
    size_t count, capacity;
} MoveState;

typedef struct {
    SemanticContext *ctx;
    ASTNode *routine;
    bool failed;
    /* Each refused use is reported once even though loops are walked twice. */
    const ASTNode **reported;
    size_t reported_count, reported_capacity;
    /* Loan verdicts of lending calls, each checked once. */
    struct { const ASTNode *call; bool admitted; } *lent;
    size_t lent_count, lent_capacity;
} MoveWalk;

static void state_free(MoveState *state)
{
    for (size_t i = 0; i < state->count; i++) free(state->rows[i].fields);
    free(state->rows);
    *state = (MoveState){0};
}

static bool state_add(MoveWalk *walk, MoveState *state, uint32_t binding, const char *fields,
    const char *callee, const char *param, ASTNode *lending_call)
{
    for (size_t i = 0; i < state->count; i++)
        if (state->rows[i].binding == binding && strcmp(state->rows[i].fields, fields) == 0) {
            /* A move on any path is a move, whatever lent it elsewhere. */
            if (lending_call == NULL) state->rows[i].lending_call = NULL;
            return true;
        }
    if (state->count == state->capacity) {
        size_t capacity = state->capacity == 0 ? 8 : state->capacity * 2;
        MovedPath *grown = realloc(state->rows, capacity * sizeof(*grown));
        if (grown == NULL) { walk->failed = true; return false; }
        state->rows = grown; state->capacity = capacity;
    }
    char *copy = malloc(strlen(fields) + 1);
    if (copy == NULL) { walk->failed = true; return false; }
    strcpy(copy, fields);
    state->rows[state->count++] = (MovedPath){binding, copy, callee, param, lending_call};
    return true;
}

static bool state_copy(MoveWalk *walk, MoveState *out, const MoveState *in)
{
    *out = (MoveState){0};
    for (size_t i = 0; i < in->count; i++)
        if (!state_add(walk, out, in->rows[i].binding, in->rows[i].fields,
                in->rows[i].callee, in->rows[i].param, in->rows[i].lending_call)) return false;
    return true;
}

static bool state_union(MoveWalk *walk, MoveState *into, const MoveState *from)
{
    for (size_t i = 0; i < from->count; i++)
        if (!state_add(walk, into, from->rows[i].binding, from->rows[i].fields,
                from->rows[i].callee, from->rows[i].param, from->rows[i].lending_call)) return false;
    return true;
}

/* `prefix` names the same value as `fields` or a value containing it. */
static bool path_contains(const char *prefix, const char *fields)
{
    size_t n = strlen(prefix);
    return strncmp(prefix, fields, n) == 0 && (fields[n] == '\0' || fields[n] == '.');
}

/* Access path of an identifier, member chain or element access. An element
 * names its whole Array. Returns false for any other expression. */
static bool access_path(const ASTNode *node, uint32_t *binding, char *buf, size_t size)
{
    if (node == NULL) return false;
    if (node->type == AST_IDENTIFIER) {
        *binding = ast_identifier_binding_syntax_id(node);
        if (size == 0) return false;
        buf[0] = '\0';
        return *binding != 0;
    }
    if (node->type == AST_ARRAY_ACCESS) return access_path(ast_array_access_array(node), binding, buf, size);
    if (node->type != AST_MEMBER_ACCESS || ast_member_name(node) == NULL) return false;
    if (!access_path(ast_member_object(node), binding, buf, size)) return false;
    size_t used = strlen(buf), name = strlen(ast_member_name(node));
    if (used + name + 2 > size) return false;
    buf[used] = '.';
    memcpy(buf + used + 1, ast_member_name(node), name + 1);
    return true;
}

static bool reported_once(MoveWalk *walk, const ASTNode *node)
{
    for (size_t i = 0; i < walk->reported_count; i++) if (walk->reported[i] == node) return false;
    if (walk->reported_count == walk->reported_capacity) {
        size_t capacity = walk->reported_capacity == 0 ? 8 : walk->reported_capacity * 2;
        const ASTNode **grown = realloc(walk->reported, capacity * sizeof(*grown));
        if (grown == NULL) { walk->failed = true; return false; }
        walk->reported = grown; walk->reported_capacity = capacity;
    }
    walk->reported[walk->reported_count++] = node;
    return true;
}

/* A path a read-and-return call lent may be used again when that loan stays
 * in the routine, or leaves it only as the formal's own return, and nothing
 * changes its owner. */
static bool lent_use_admitted(MoveWalk *walk, const MovedPath *moved)
{
    if (moved->lending_call == NULL) return false;
    for (size_t i = 0; i < walk->lent_count; i++)
        if (walk->lent[i].call == moved->lending_call) return walk->lent[i].admitted;
    bool admitted = function_param_flow_lent_argument_admitted(walk->ctx, walk->routine,
        moved->lending_call, moved->binding);
    if (walk->lent_count == walk->lent_capacity) {
        size_t capacity = walk->lent_capacity == 0 ? 8 : walk->lent_capacity * 2;
        void *grown = realloc(walk->lent, capacity * sizeof(*walk->lent));
        if (grown == NULL) { walk->failed = true; return false; }
        walk->lent = grown; walk->lent_capacity = capacity;
    }
    walk->lent[walk->lent_count].call = moved->lending_call;
    walk->lent[walk->lent_count++].admitted = admitted;
    return admitted;
}

static void refuse_use(MoveWalk *walk, const ASTNode *node, const MovedPath *moved)
{
    if (lent_use_admitted(walk, moved) || !reported_once(walk, node)) return;
    const ASTNode *root = node;
    while (root != NULL && (root->type == AST_MEMBER_ACCESS || root->type == AST_ARRAY_ACCESS))
        root = root->type == AST_MEMBER_ACCESS ? ast_member_object(root) : ast_array_access_array(root);
    char display[512];
    const char *name = root != NULL && root->type == AST_IDENTIFIER ? ast_identifier_name(root) : NULL;
    if (name != NULL && strlen(name) + strlen(moved->fields) < sizeof(display)) {
        strcpy(display, name);
        strcat(display, moved->fields);
        name = display;
    }
    semantic_error_with_hints(walk->ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED, PGY_CAUSE_MOVE_FROM_RELEASED,
        PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE, (ASTNode *)node,
        "'%s' was moved into '%s' and cannot be used again.\n"
        "Reason:\n"
        "- parameter '%s' of '%s' changes or returns the storage it receives\n"
        "- the caller's copy shares that storage, so it may already be freed or changed\n"
        "Fix:\n"
        "- reassign the value from the call result, as in x = %s(x)\n"
        "- or pass a fresh value to the call",
        name != NULL ? name : "value", moved->callee, moved->param, moved->callee, moved->callee);
}

/* A use of `node` conflicts with a moved path when either contains the other. */
static void check_use(MoveWalk *walk, const MoveState *state, const ASTNode *node)
{
    char fields[512];
    uint32_t binding = 0;
    if (!access_path(node, &binding, fields, sizeof(fields))) return;
    for (size_t i = 0; i < state->count; i++) {
        const MovedPath *moved = &state->rows[i];
        if (moved->binding == binding &&
            (path_contains(moved->fields, fields) || path_contains(fields, moved->fields))) {
            refuse_use(walk, node, moved);
            return;
        }
    }
}

/* An enum value is copied with its payloads, so one whose payloads hold
 * storage shares it as a struct does. */
static bool aggregate_type(SemanticContext *ctx, const Type *type)
{
    if (type == NULL || type == TYPE_UNKNOWN) return false;
    if (type->kind == TYPE_KIND_CLASS) return type->nominal_flavor == TYPE_NOMINAL_STRUCT;
    if (type->kind == TYPE_KIND_ENUM)
        return semantic_classify_ownership_type(type, ctx) == OWNERSHIP_TYPE_BORROW_TRACKED;
    return type->kind == TYPE_KIND_TUPLE || type->kind == TYPE_KIND_CONSTRUCTED;
}

/* The callee changes, keeps, returns or releases storage it receives through
 * this default-mode parameter. For an Array formal the descriptor-transfer
 * owner answers exactly: element values that escape (Strings are immutable)
 * are the String owners' concern, not a move. Any other aggregate moves
 * unless its summary only reads it and the same owner proves that no store
 * through its access paths changes the storage it shares with the caller.
 * A summary that is missing or not final counts as a change. */
static bool formal_moves(SemanticContext *ctx, ASTNode *callee, size_t formal)
{
    FuncParam *param = ast_func_param(callee, formal);
    if (param == NULL || param->mode != PARAM_MODE_DEFAULT || ast_func_body(callee) == NULL) return false;
    unsigned mask = function_param_flow_summary_for_param(ctx, callee, formal);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (store == NULL || store->failed) return true;
    size_t index = function_param_flow_find(store, ast_node_stable_id(callee), formal);
    if (index == SIZE_MAX || store->entries[index].state != FUNCTION_PARAM_FLOW_COMPLETE) return true;
    if ((mask & SLOT_PARAM_SUMMARY_RELEASE) != 0) return true;
    const char *type_name = param->type == NULL ? NULL : ast_type_name(param->type);
    if (type_name != NULL && strcmp(type_name, "Array") == 0)
        return store->entries[index].array_storage_unproved;
    return (mask & ~SLOT_PARAM_SUMMARY_READ) != 0 || store->entries[index].array_storage_unproved;
}

/* The callee only reads this default-mode argument and returns it or a part
 * of it. A call the caller follows with another use of the argument lends
 * it instead of moving it (2026-10-19_by_value_getter_lends_when_used_again).
 * The loan proof refuses the lend when that call or a later one changes the
 * storage. An Array formal keeps the descriptor-transfer answer. */
static bool formal_lends(SemanticContext *ctx, ASTNode *callee, size_t formal)
{
    FuncParam *param = ast_func_param(callee, formal);
    if (param == NULL || param->mode != PARAM_MODE_DEFAULT || ast_func_body(callee) == NULL) return false;
    const char *type_name = param->type == NULL ? NULL : ast_type_name(param->type);
    if (type_name != NULL && strcmp(type_name, "Array") == 0) return false;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (store == NULL || store->failed) return false;
    size_t index = function_param_flow_find(store, ast_node_stable_id(callee), formal);
    if (index == SIZE_MAX || store->entries[index].state != FUNCTION_PARAM_FLOW_COMPLETE) return false;
    unsigned mask = store->entries[index].mask;
    return (mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0 &&
        (mask & ~(SLOT_PARAM_SUMMARY_READ | SLOT_PARAM_SUMMARY_RETURN_ESCAPE)) == 0;
}

static ParamMode routine_param_mode(const ASTNode *routine, uint32_t binding, bool *found)
{
    *found = false;
    for (size_t p = 0; routine != NULL && p < ast_func_param_count(routine); p++) {
        FuncParam *param = ast_func_param(routine, p);
        if (param != NULL && ast_func_param_stable_id(param) == binding) { *found = true; return param->mode; }
    }
    return PARAM_MODE_DEFAULT;
}

static bool walk_node(MoveWalk *walk, MoveState *state, ASTNode *node, bool *terminated);

static void apply_call_moves(MoveWalk *walk, MoveState *state, ASTNode *call)
{
    for (size_t p = 0; p < ast_call_arg_count(call); p++) {
        ASTNode *arg = ast_call_argument(call, p);
        char fields[512];
        uint32_t binding = 0;
        if (!access_path(arg, &binding, fields, sizeof(fields))) continue;
        ASTNode *callee = semantic_param_publication_callee(walk->ctx, call, p);
        size_t formal = 0;
        if (callee == NULL || !semantic_param_publication_formal_for_actual(walk->ctx, call, p, &formal)) continue;
        SemanticPublishedValue value = semantic_param_publication_value(walk->ctx, arg);
        if (!value.ok || value.meta || !aggregate_type(walk->ctx, value.type) ||
            !formal_moves(walk->ctx, callee, formal)) continue;
        FuncParam *param = ast_func_param(callee, formal);
        const char *callee_name = ast_declaration_name(callee) != NULL ? ast_declaration_name(callee) : "callee";
        const char *param_name = param != NULL && param->name != NULL ? param->name : "parameter";
        bool parameter = false;
        ParamMode mode = routine_param_mode(walk->routine, binding, &parameter);
        /* A formal or a local is lent. The loan proof refuses a later use
         * when the loan leaves the routine, or when another call hands a
         * local on while the loan lives. */
        bool lends = formal_lends(walk->ctx, callee, formal);
        /* A `ref` formal can be lent on, never moved: nothing may change it
         * while its loan lives. An `inout` one can change, so it is neither. */
        if (parameter && mode == PARAM_MODE_REF && lends) continue;
        if (parameter && mode != PARAM_MODE_DEFAULT && mode != PARAM_MODE_OWN) {
            if (reported_once(walk, arg))
                semantic_error_with_hints(walk->ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE, arg,
                    "Borrowed parameter storage cannot be moved into '%s'.\n"
                    "Reason:\n"
                    "- parameter '%s' of '%s' changes or returns the storage it receives\n"
                    "- this function only borrows the argument from its own caller\n"
                    "Fix:\n"
                    "- take the parameter by value instead of ref or inout\n"
                    "- or pass a fresh value to the call",
                    callee_name, param_name, callee_name);
            continue;
        }
        for (size_t i = 0; i < state->count; i++) {
            const MovedPath *moved = &state->rows[i];
            if (moved->binding == binding &&
                (path_contains(moved->fields, fields) || path_contains(fields, moved->fields))) {
                refuse_use(walk, arg, moved);
                break;
            }
        }
        if (!state_add(walk, state, binding, fields, callee_name, param_name, lends ? call : NULL)) return;
    }
}

static void reinitialize(MoveWalk *walk, MoveState *state, ASTNode *target)
{
    char fields[512];
    uint32_t binding = 0;
    if (!access_path(target, &binding, fields, sizeof(fields))) return;
    for (size_t i = 0; i < state->count; i++) {
        MovedPath *moved = &state->rows[i];
        /* Writing a part of a moved value writes into freed storage. */
        if (moved->binding == binding && path_contains(moved->fields, fields) &&
            strcmp(moved->fields, fields) != 0) {
            refuse_use(walk, target, moved);
            return;
        }
    }
    size_t kept = 0;
    for (size_t i = 0; i < state->count; i++) {
        MovedPath *moved = &state->rows[i];
        if (moved->binding == binding && path_contains(fields, moved->fields)) { free(moved->fields); continue; }
        state->rows[kept++] = *moved;
    }
    state->count = kept;
}

/* Uses inside a nested expression, then the index expressions of a path. */
static bool walk_expression(MoveWalk *walk, MoveState *state, ASTNode *node)
{
    if (node == NULL || walk->failed) return !walk->failed;
    char fields[512];
    uint32_t binding = 0;
    if (access_path(node, &binding, fields, sizeof(fields))) {
        check_use(walk, state, node);
        for (ASTNode *at = node; at != NULL && at->type != AST_IDENTIFIER;
             at = at->type == AST_MEMBER_ACCESS ? ast_member_object(at) : ast_array_access_array(at))
            if (at->type == AST_ARRAY_ACCESS && !walk_expression(walk, state, ast_array_access_index(at))) return false;
        return !walk->failed;
    }
    bool terminated = false;
    return walk_node(walk, state, node, &terminated);
}

static bool walk_children(MoveWalk *walk, MoveState *state, ASTNode *node, bool *terminated)
{
    bool supported = false;
    size_t count = function_param_flow_claim_child_count(node, &supported);
    if (!supported || count == SIZE_MAX) {
        /* A construct whose children this owner cannot enumerate may use a
         * moved value; refuse while any move is live rather than skip it. */
        if (state->count != 0) refuse_use(walk, node, &state->rows[0]);
        return !walk->failed;
    }
    for (size_t c = 0; c < count && !*terminated; c++) {
        ASTNode *child = function_param_flow_claim_child(node, c);
        if (child != NULL && !walk_node(walk, state, child, terminated)) return false;
    }
    return !walk->failed;
}

/* A nested body that may run later or elsewhere sees the current moves; its
 * own moves do not flow back. */
static bool walk_detached(MoveWalk *walk, const MoveState *state, ASTNode *node)
{
    MoveState copy;
    if (!state_copy(walk, &copy, state)) return false;
    bool terminated = false;
    bool ok = walk_children(walk, &copy, node, &terminated);
    state_free(&copy);
    return ok;
}

static bool walk_branches(MoveWalk *walk, MoveState *state, ASTNode **arms, size_t count,
    bool fallthrough, bool *terminated)
{
    MoveState merged = {0};
    bool any_live = false;
    if (fallthrough) { if (!state_copy(walk, &merged, state)) return false; any_live = true; }
    for (size_t a = 0; a < count; a++) {
        MoveState arm;
        if (!state_copy(walk, &arm, state)) { state_free(&merged); return false; }
        bool arm_terminated = false;
        if (arms[a] != NULL && !walk_node(walk, &arm, arms[a], &arm_terminated)) {
            state_free(&arm); state_free(&merged); return false;
        }
        if (!arm_terminated) { any_live = true; if (!state_union(walk, &merged, &arm)) { state_free(&arm); state_free(&merged); return false; } }
        state_free(&arm);
    }
    state_free(state);
    *state = merged;
    *terminated = !any_live;
    return !walk->failed;
}

static bool walk_loop(MoveWalk *walk, MoveState *state, ASTNode *condition, ASTNode *body)
{
    MoveState entry;
    if (!state_copy(walk, &entry, state)) return false;
    for (int pass = 0; pass < 2; pass++) {
        if (!walk_expression(walk, state, condition)) { state_free(&entry); return false; }
        bool terminated = false;
        if (body != NULL && !walk_node(walk, state, body, &terminated)) { state_free(&entry); return false; }
    }
    bool ok = state_union(walk, state, &entry);
    state_free(&entry);
    return ok;
}

static bool walk_node(MoveWalk *walk, MoveState *state, ASTNode *node, bool *terminated)
{
    if (node == NULL || walk->failed) return !walk->failed;
    switch (node->type) {
    case AST_BLOCK:
        for (size_t s = 0; s < ast_block_statement_count(node) && !*terminated; s++)
            if (!walk_node(walk, state, ast_block_statement(node, s), terminated)) return false;
        return true;
    case AST_IDENTIFIER: case AST_MEMBER_ACCESS: case AST_ARRAY_ACCESS: {
        char fields[512];
        uint32_t binding = 0;
        if (access_path(node, &binding, fields, sizeof(fields))) return walk_expression(walk, state, node);
        /* Not a binding path (a callable name, a call result's field): only
         * its operand expressions can use a moved value. */
        if (node->type == AST_MEMBER_ACCESS) return walk_expression(walk, state, ast_member_object(node));
        if (node->type == AST_ARRAY_ACCESS)
            return walk_expression(walk, state, ast_array_access_array(node)) &&
                walk_expression(walk, state, ast_array_access_index(node));
        return true;
    }
    case AST_CALL: {
        ASTNode *callee = ast_call_callee(node);
        if (callee != NULL && callee->type == AST_MEMBER_ACCESS && !walk_expression(walk, state, ast_member_object(callee)))
            return false;
        for (size_t p = 0; p < ast_call_arg_count(node); p++)
            if (!walk_expression(walk, state, ast_call_argument(node, p))) return false;
        apply_call_moves(walk, state, node);
        /* A call that never returns ends this path, as a return does: what
         * it moved is never used after it. */
        SemanticPublishedValue result = semantic_param_publication_value(walk->ctx, node);
        if (result.ok && type_equals(result.type, TYPE_NEVER)) *terminated = true;
        return !walk->failed;
    }
    case AST_ASSIGNMENT:
        if (!walk_expression(walk, state, ast_assignment_value(node))) return false;
        for (ASTNode *at = ast_assignment_target(node); at != NULL &&
             (at->type == AST_MEMBER_ACCESS || at->type == AST_ARRAY_ACCESS);
             at = at->type == AST_MEMBER_ACCESS ? ast_member_object(at) : ast_array_access_array(at))
            if (at->type == AST_ARRAY_ACCESS && !walk_expression(walk, state, ast_array_access_index(at))) return false;
        reinitialize(walk, state, ast_assignment_target(node));
        return !walk->failed;
    case AST_LET_DECL:
        return walk_expression(walk, state, ast_let_initializer(node));
    case AST_RETURN:
        if (!walk_expression(walk, state, ast_return_value(node))) return false;
        *terminated = true;
        return true;
    case AST_BREAK: case AST_CONTINUE: case AST_FAIL_STMT:
        if (node->type == AST_FAIL_STMT && !walk_children(walk, state, node, terminated)) return false;
        *terminated = true;
        return true;
    case AST_IF_STMT: {
        if (!walk_expression(walk, state, ast_if_condition(node))) return false;
        ASTNode *arms[2] = {ast_if_then_branch(node), ast_if_else_branch(node)};
        return walk_branches(walk, state, arms, arms[1] != NULL ? 2 : 1, arms[1] == NULL, terminated);
    }
    case AST_WHILE_LOOP:
        return walk_loop(walk, state, ast_while_condition(node), ast_while_body(node));
    case AST_FOR_LOOP:
        if (!walk_expression(walk, state, ast_for_range_start(node)) ||
            !walk_expression(walk, state, ast_for_range_end(node)) ||
            !walk_expression(walk, state, ast_for_iterable(node))) return false;
        return walk_loop(walk, state, NULL, ast_for_body(node));
    case AST_MATCH_STMT: {
        if (!walk_expression(walk, state, ast_match_subject(node))) return false;
        size_t count = ast_match_case_count(node);
        ASTNode **arms = calloc(count + 1, sizeof(*arms));
        if (arms == NULL) { walk->failed = true; return false; }
        for (size_t c = 0; c < count; c++) arms[c] = ast_match_case_at(node, c);
        arms[count] = ast_match_default_body(node);
        bool ok = walk_branches(walk, state, arms, count + 1, false, terminated);
        free(arms);
        return ok;
    }
    case AST_MATCH_CASE:
        if (!walk_expression(walk, state, ast_match_case_guard(node))) return false;
        return walk_node(walk, state, ast_match_case_body(node), terminated);
    case AST_LAMBDA_EXPR: case AST_DEFER_STMT: case AST_ASYNC_BLOCK: case AST_PARALLEL_BLOCK:
        return walk_detached(walk, state, node);
    case AST_FUNC_DECL: case AST_CLASS_DECL: case AST_ENUM_DECL: case AST_TYPE:
        return true;
    default:
        return walk_children(walk, state, node, terminated);
    }
}

bool semantic_default_aggregate_move_finalize(SemanticContext *ctx)
{
    SemanticParamPublicationView view;
    if (ctx == NULL || ctx->has_error) return ctx != NULL && !ctx->has_error;
    if (!semantic_param_publication_admitted_view(ctx, &view)) {
        semantic_error(ctx, ctx->program_root, "Default aggregate move check requires the admitted parameter view");
        return false;
    }
    ASTNode *previous_decl = ctx->current_function_decl;
    const char *previous_module = ctx->current_module_path;
    MoveWalk walk = {.ctx = ctx};
    for (size_t r = 0; r < view.routine_count && !walk.failed; r++) {
        const PendingRoutine *row = &view.routines[r];
        if (row->decl == NULL || ast_func_body(row->decl) == NULL) continue;
        ctx->current_function_decl = row->decl;
        ctx->current_module_path = row->module;
        walk.routine = row->decl;
        MoveState state = {0};
        bool terminated = false;
        (void)walk_node(&walk, &state, ast_func_body(row->decl), &terminated);
        state_free(&state);
    }
    free(walk.reported);
    free(walk.lent);
    ctx->current_function_decl = previous_decl;
    ctx->current_module_path = previous_module;
    if (walk.failed) semantic_error(ctx, ctx->program_root, "Default aggregate move check allocation failed");
    return !ctx->has_error;
}
