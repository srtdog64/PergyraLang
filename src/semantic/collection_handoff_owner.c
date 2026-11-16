/* Hand-off of a local Array<String> (decisions 2026-10-18 and 2026-11-15,
 * docs/truth/decisions/2026-11-15_handed_off_array_storage_is_not_released.md).
 *
 * A named local passed whole to a default-mode formal that the move rule
 * (default_aggregate_move_rule.c) says moves its argument, or passed whole to
 * an Array<String> field of a struct constructor, gives its storage away. The
 * binding then keeps the element ownership it had and is marked handed-off:
 * the caller releases nothing for it. A callee never releases what it was
 * handed either; a record releases a constructor input once, by its recipe.
 *
 * The hand-off is the HAND_OFF action of the call hand-off rule
 * (call_handoff_rule.def) for the formal's class (a constructor field is
 * Keep), a local origin, the site's later use and context. It is issued on
 * the final row, after every other transition has settled, and only when
 * the input is the binding's single definition handed once. It is not
 * issued, and the row keeps its state, when:
 * - the argument is not the bare name of a let-declared local of this
 *   routine (a Clone the move owner landed there is a call, not a name);
 * - the row is not live, or is a member move whose storage returns to its
 *   aggregate;
 * - the binding has another definition (an assignment of a new value);
 * - the site runs again without a new definition of the binding: a loop
 *   around it does not also enclose the binding's declaration, or it is
 *   inside a defer, lambda, async or parallel body; a return or fail
 *   statement runs once;
 * - for a constructor, the binding is given to more than one constructor
 *   input of the routine.
 * The self-host semantic owners issue the same disposition with the same
 * exclusions (ast_collection_call_handoff_owner.pgy,
 * ast_collection_constructor_handoff_owner.pgy), but they decide a moving
 * formal from their own formal effects, and the native move owner lands
 * Clones where the self-host hands off; the registry row
 * semantic.hashmap_collection_ownership records where the two differ. A
 * routine whose body holds a construct this walk cannot enumerate issues no
 * hand-off. A body call whose argument facts are missing fails closed. */
#include "collection_ownership_fact_internal.h"
#include "call_handoff_rule.h"
#include "default_aggregate_move_rule.h"

#include <stdint.h>
#include <stdlib.h>

#include "function_param_flow/function_param_publication_internal.h"
#include "function_param_flow/function_param_publication_owner.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "type_checker.h"
#include "type_checker/type_checker_internal.h"
#include "../parser/ast_api.h"

typedef struct {
    uint32_t binding;
    size_t loop_depth;
    size_t definitions;
} HandoffBinding;

typedef struct {
    uint32_t binding;
    size_t loop_depth;
    unsigned members;
    bool repeats;
    bool terminal;
    bool constructor;
} HandoffSite;

typedef struct {
    SemanticContext *ctx;
    ASTNode *routine;
    HandoffBinding *bindings;
    size_t binding_count, binding_capacity;
    HandoffSite *sites;
    size_t site_count, site_capacity;
    size_t loop_depth;
    size_t detached_depth;
    bool terminal;
    bool incomplete;
    bool failed;
} HandoffWalk;

static bool
handoff_grow(void **rows, size_t *capacity, size_t count, size_t size)
{
    if (count < *capacity) return true;
    size_t next = *capacity == 0 ? 8 : *capacity * 2;
    if (next < *capacity || next > SIZE_MAX / size) return false;
    void *grown = realloc(*rows, next * size);
    if (grown == NULL) return false;
    *rows = grown;
    *capacity = next;
    return true;
}

static HandoffBinding *
handoff_binding(HandoffWalk *walk, uint32_t binding)
{
    for (size_t i = 0; i < walk->binding_count; i++)
        if (walk->bindings[i].binding == binding) return &walk->bindings[i];
    return NULL;
}

static void
handoff_note_definition(HandoffWalk *walk, uint32_t binding, bool declaration)
{
    HandoffBinding *row = handoff_binding(walk, binding);
    if (row != NULL) {
        row->definitions++;
        return;
    }
    if (!handoff_grow((void **)&walk->bindings, &walk->binding_capacity,
            walk->binding_count, sizeof(*walk->bindings))) {
        walk->failed = true;
        return;
    }
    /* An assignment to a name this walk has not seen declared (a formal)
     * counts as a definition that is not the declaration. */
    walk->bindings[walk->binding_count++] = (HandoffBinding){
        .binding = binding,
        .loop_depth = walk->loop_depth,
        .definitions = declaration ? 1 : 2,
    };
}

/* `members` is the effect class of the formal that takes the argument. */
static void
handoff_note_site(HandoffWalk *walk, const ASTNode *argument, bool constructor,
    unsigned members)
{
    if (argument == NULL || argument->type != AST_IDENTIFIER) return;
    uint32_t binding = ast_identifier_binding_syntax_id(argument);
    if (binding == 0) return;
    if (!handoff_grow((void **)&walk->sites, &walk->site_capacity,
            walk->site_count, sizeof(*walk->sites))) {
        walk->failed = true;
        return;
    }
    walk->sites[walk->site_count++] = (HandoffSite){
        .binding = binding,
        .loop_depth = walk->loop_depth,
        .members = members,
        .repeats = walk->detached_depth > 0,
        .terminal = walk->terminal,
        .constructor = constructor,
    };
}

/* A struct constructor's Array<String> field inputs (a field formal is Keep),
 * or the arguments of a body call to a default-mode formal with its class. */
static void
handoff_note_call(HandoffWalk *walk, ASTNode *call)
{
    uint32_t function_id = ast_node_stable_id(walk->routine);
    PublishedNode *published = publication_result_current(walk->ctx, call);
    const SemanticNominalResultSnapshot *snapshot = published != NULL
        ? published->nominal_result : NULL;
    if (snapshot != NULL) {
        if (!snapshot->is_struct
            || !semantic_nominal_result_snapshot_current(walk->ctx, call, snapshot)
            || snapshot->field_count != ast_call_arg_count(call))
            return;
        for (size_t p = 0; p < snapshot->field_count; p++)
            if (snapshot->fields[p].string_array_elements_present)
                handoff_note_site(walk, ast_call_argument(call, p), true,
                    PGY_CALL_HANDOFF_MEMBER_KEEP);
        return;
    }
    /* A call in a detached body runs again or elsewhere: it hands nothing
     * off, so its edge is not queried. */
    if (walk->detached_depth > 0) return;
    for (size_t p = 0; p < ast_call_arg_count(call); p++) {
        ASTNode *argument = ast_call_argument(call, p);
        if (argument == NULL || argument->type != AST_IDENTIFIER
            || semantic_collection_ownership_fact_find(walk->ctx, function_id,
                ast_identifier_binding_syntax_id(argument)) == NULL)
            continue;
        /* The same published aggregate argument the move owner judges. */
        SemanticPublishedValue value = semantic_param_publication_value(
            walk->ctx, argument);
        if (!value.ok || value.meta
            || !semantic_default_aggregate_value(walk->ctx, value.type))
            continue;
        bool body_call = false;
        ASTNode *callee = NULL;
        size_t formal = 0;
        if (!semantic_param_publication_body_actual_current(walk->ctx,
                walk->routine, call, p, &body_call, &callee, &formal)) {
            semantic_error(walk->ctx, call,
                "Collection hand-off requires a current body-call argument fact");
            walk->failed = true;
            return;
        }
        if (!body_call) continue;
        unsigned members = 0u;
        if (!semantic_default_aggregate_formal_class(walk->ctx, callee, formal,
                &members))
            continue;
        handoff_note_site(walk, argument, false, members);
    }
}

static void handoff_walk(HandoffWalk *walk, ASTNode *node);

static void
handoff_walk_children(HandoffWalk *walk, ASTNode *node)
{
    bool supported = false;
    size_t count = function_param_flow_claim_child_count(node, &supported);
    if (!supported || count == SIZE_MAX) {
        walk->incomplete = true;
        return;
    }
    for (size_t c = 0; c < count && !walk->failed; c++)
        handoff_walk(walk, function_param_flow_claim_child(node, c));
}

static void
handoff_walk(HandoffWalk *walk, ASTNode *node)
{
    if (node == NULL || walk->failed) return;
    switch (node->type) {
    case AST_FUNC_DECL: case AST_CLASS_DECL: case AST_ENUM_DECL: case AST_TYPE:
        return;
    case AST_BLOCK:
        for (size_t s = 0; s < ast_block_statement_count(node); s++)
            handoff_walk(walk, ast_block_statement(node, s));
        return;
    case AST_LET_DECL:
        handoff_note_definition(walk, ast_node_stable_id(node), true);
        handoff_walk(walk, ast_let_initializer(node));
        return;
    case AST_ASSIGNMENT: {
        ASTNode *target = ast_assignment_target(node);
        ASTNode *value = ast_assignment_value(node);
        if (target != NULL && target->type == AST_IDENTIFIER) {
            uint32_t binding = ast_identifier_binding_syntax_id(target);
            bool same = value != NULL && value->type == AST_IDENTIFIER
                && ast_identifier_binding_syntax_id(value) == binding;
            if (binding != 0 && !same)
                handoff_note_definition(walk, binding, false);
        }
        handoff_walk_children(walk, node);
        return;
    }
    case AST_WHILE_LOOP: case AST_FOR_LOOP:
        walk->loop_depth++;
        handoff_walk_children(walk, node);
        walk->loop_depth--;
        return;
    case AST_DEFER_STMT: case AST_LAMBDA_EXPR: case AST_ASYNC_BLOCK:
    case AST_PARALLEL_BLOCK:
        walk->detached_depth++;
        handoff_walk_children(walk, node);
        walk->detached_depth--;
        return;
    case AST_RETURN: case AST_FAIL_STMT: {
        bool previous = walk->terminal;
        walk->terminal = true;
        handoff_walk_children(walk, node);
        walk->terminal = previous;
        return;
    }
    case AST_CALL:
        handoff_walk_children(walk, node);
        if (!walk->failed) handoff_note_call(walk, node);
        return;
    default:
        handoff_walk_children(walk, node);
        return;
    }
}

/* Whether the rule hands `binding` off at the site `at`, and the binding has
 * the one definition a hand-off is recorded for. The site is used again
 * when it runs again without a new definition, or when the binding is given
 * to another constructor input. This walk reads no later use: the move owner
 * lands a Clone at a moved argument that is used again (then the argument
 * is a call, not a site) or refuses the program. */
static bool
handoff_site_admitted(const HandoffWalk *walk, const HandoffSite *at)
{
    const HandoffBinding *binding = NULL;
    for (size_t i = 0; i < walk->binding_count; i++)
        if (walk->bindings[i].binding == at->binding)
            binding = &walk->bindings[i];
    if (binding == NULL || binding->definitions != 1)
        return false;
    size_t inputs = 0;
    for (size_t i = 0; at->constructor && i < walk->site_count; i++)
        if (walk->sites[i].constructor && walk->sites[i].binding == at->binding)
            inputs++;
    bool once = !at->repeats
        && (at->terminal || at->loop_depth == binding->loop_depth)
        && (!at->constructor || inputs == 1);
    return pgy_call_handoff_action(at->members, PGY_CALL_HANDOFF_ORIGIN_LOCAL,
            once ? PGY_CALL_HANDOFF_LATER_ONCE : PGY_CALL_HANDOFF_LATER_USED_AGAIN,
            at->repeats ? PGY_CALL_HANDOFF_CONTEXT_DETACHED
                : PGY_CALL_HANDOFF_CONTEXT_SYNC,
            PGY_CALL_HANDOFF_ELEMENTS_AS_GIVEN, PGY_CALL_HANDOFF_DIVERGENCE_NONE)
        == PGY_CALL_HANDOFF_ACTION_HAND_OFF;
}

typedef struct {
    PgyCollectionHandoff *rows;
    size_t count, capacity;
} HandoffSelection;

/* Calls first: a binding a callee took is no longer a live constructor
 * input, so a constructor never selects a binding a call selected. */
static bool
handoff_select(const HandoffWalk *walk, bool constructor,
    HandoffSelection *selection, size_t routine_start)
{
    uint32_t function_id = ast_node_stable_id(walk->routine);
    for (size_t i = 0; i < walk->site_count; i++) {
        const HandoffSite *at = &walk->sites[i];
        if (at->constructor != constructor || !handoff_site_admitted(walk, at))
            continue;
        const PgyCollectionOwnershipFact *fact =
            semantic_collection_ownership_fact_find(walk->ctx, function_id,
                at->binding);
        if (fact == NULL || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
            || fact->origin == PGY_COLLECTION_ORIGIN_MEMBER_MOVE)
            continue;
        bool selected = false;
        for (size_t s = routine_start; s < selection->count; s++)
            selected = selected || selection->rows[s].binding_syntax_id == at->binding;
        if (selected) continue;
        if (!handoff_grow((void **)&selection->rows, &selection->capacity,
                selection->count, sizeof(*selection->rows)))
            return false;
        selection->rows[selection->count++] = (PgyCollectionHandoff){
            .function_syntax_id = function_id,
            .binding_syntax_id = at->binding,
        };
    }
    return true;
}

bool
semantic_collection_ownership_select_handoffs(SemanticContext *ctx,
    PgyCollectionHandoff **rows_out, size_t *count_out)
{
    SemanticParamPublicationView view;
    if (ctx == NULL || rows_out == NULL || count_out == NULL || *rows_out != NULL
        || !semantic_param_publication_admitted_view(ctx, &view))
        return false;
    *count_out = 0;
    if (ctx->collection_ownership_fact_count == 0)
        return true;
    ASTNode *previous_decl = ctx->current_function_decl;
    const char *previous_module = ctx->current_module_path;
    HandoffSelection selection = {0};
    bool ok = true;
    for (size_t r = 0; r < view.routine_count && ok; r++) {
        const PendingRoutine *row = &view.routines[r];
        if (row->decl == NULL || ast_func_body(row->decl) == NULL) continue;
        ctx->current_function_decl = row->decl;
        ctx->current_module_path = row->module;
        HandoffWalk walk = {.ctx = ctx, .routine = row->decl};
        handoff_walk(&walk, ast_func_body(row->decl));
        ok = !walk.failed;
        size_t routine_start = selection.count;
        if (ok && !walk.incomplete)
            ok = handoff_select(&walk, false, &selection, routine_start)
                && handoff_select(&walk, true, &selection, routine_start);
        free(walk.bindings);
        free(walk.sites);
    }
    ctx->current_function_decl = previous_decl;
    ctx->current_module_path = previous_module;
    if (!ok) {
        free(selection.rows);
        return false;
    }
    *rows_out = selection.rows;
    *count_out = selection.count;
    return true;
}

/* Every selected row must still be the live, non-member-move row the
 * selection saw; anything else fails closed. */
bool
semantic_collection_ownership_apply_handoffs(SemanticContext *ctx,
    const PgyCollectionHandoff *rows, size_t count)
{
    if (ctx == NULL || (count != 0 && rows == NULL)) return false;
    for (size_t i = 0; i < count; i++) {
        PgyCollectionOwnershipFact *fact = (PgyCollectionOwnershipFact *)
            semantic_collection_ownership_fact_find(ctx,
                rows[i].function_syntax_id, rows[i].binding_syntax_id);
        if (fact == NULL || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
            || fact->origin == PGY_COLLECTION_ORIGIN_MEMBER_MOVE)
            return false;
        fact->disposition = PGY_COLLECTION_DISPOSITION_HANDED_OFF;
    }
    return true;
}
