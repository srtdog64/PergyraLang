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
 * hand-off. A body call whose argument facts are missing fails closed.
 *
 * The same walk writes the call hand-off ledger (call_handoff_ledger.h,
 * slice S2): one class row per default-mode Array<String> formal, one action
 * row per site of a tracked local row with the lookup inputs this owner used,
 * and one copy row per compiler Clone of the body. A compiler Clone of a
 * tracked local at an argument is a site with origin Fresh; it never hands
 * the local off and is not counted as the local's constructor input. */
#include "collection_ownership_fact_internal.h"
#include "call_handoff_ledger.h"
#include "call_handoff_rule.h"
#include "default_aggregate_move_rule.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "function_param_flow/function_param_publication_internal.h"
#include "function_param_flow/function_param_publication_owner.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "type_checker.h"
#include "type_checker/type_checker_internal.h"
#include "../common/string_compat.h"
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
    bool compiler_clone;
    bool selected;
    const ASTNode *call;
    size_t parameter_index;
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
    /* The ledger rows of this routine; a Clone site whose call edge could
     * not be read leaves the routine's ledger incomplete. */
    PgyCallHandoffLedger *ledger;
    bool ledger_incomplete;
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

/* The operand of a compiler Clone (a checker-built `Clone(x)`), or NULL. */
static ASTNode *
handoff_compiler_clone_operand(const ASTNode *node)
{
    if (node == NULL || node->type != AST_CALL || !ast_call_compiler_builtin(node)
        || ast_call_arg_count(node) != 1)
        return NULL;
    const ASTNode *callee = ast_call_callee(node);
    const char *name = callee != NULL && callee->type == AST_IDENTIFIER
        ? ast_identifier_name(callee) : NULL;
    return name != NULL && strcmp(name, "Clone") == 0
        ? ast_call_argument(node, 0) : NULL;
}

/* The spelling of the routine or constructor a call names, or NULL. */
static const char *
handoff_callee_name(const ASTNode *call)
{
    const ASTNode *callee = ast_call_callee(call);
    if (callee == NULL) return NULL;
    if (callee->type == AST_IDENTIFIER) return ast_identifier_name(callee);
    if (callee->type == AST_MEMBER_ACCESS) return ast_member_name(callee);
    return NULL;
}

/* `members` is the effect class of the formal that takes the argument, and
 * `parameter_index` its formal (a constructor's field) ordinal. The argument
 * is a bare name, or a compiler Clone of one. */
static void
handoff_note_site(HandoffWalk *walk, const ASTNode *argument, bool constructor,
    unsigned members, const ASTNode *call, size_t parameter_index)
{
    const ASTNode *operand = handoff_compiler_clone_operand(argument);
    const ASTNode *name = operand != NULL ? operand : argument;
    if (name == NULL || name->type != AST_IDENTIFIER) return;
    uint32_t binding = ast_identifier_binding_syntax_id(name);
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
        .compiler_clone = operand != NULL,
        .call = call,
        .parameter_index = parameter_index,
    };
}

/* The class row of a callee formal the walk read, once per formal. An Array
 * formal whose moved descriptor no keep explains counts the move as GROW
 * (NATIVE_ARRAY_FORMAL_DESCRIPTOR_ONLY, default_aggregate_move_rule.c). */
static void
handoff_note_class(HandoffWalk *walk, const ASTNode *callee, size_t formal,
    unsigned members)
{
    PgyCallHandoffLedger *ledger = walk->ledger;
    uint32_t callee_id = ast_node_stable_id(callee);
    for (size_t i = 0; ledger != NULL && i < ledger->class_count; i++)
        if (ledger->classes[i].function_syntax_id == callee_id
            && ledger->classes[i].parameter_index == (uint32_t)formal)
            return;
    if (ledger == NULL) return;
    if (!handoff_grow((void **)&ledger->classes, &ledger->class_capacity,
            ledger->class_count, sizeof(*ledger->classes))) {
        walk->failed = true;
        return;
    }
    const FuncParam *param = ast_func_param(callee, formal);
    const char *type_name = param != NULL && param->type != NULL
        ? ast_type_name(param->type) : NULL;
    ledger->classes[ledger->class_count++] = (PgyCallHandoffClassRow){
        .function_syntax_id = callee_id,
        .parameter_index = (uint32_t)formal,
        .members = members,
        .divergence = type_name != NULL && strcmp(type_name, "Array") == 0
                && (members & PGY_CALL_HANDOFF_MEMBER_GROW) != 0u
            ? PGY_CALL_HANDOFF_DIVERGENCE_ID_NATIVE_ARRAY_FORMAL_DESCRIPTOR_ONLY
            : -1,
    };
}

/* A struct constructor's Array<String> field inputs (a field formal is Keep),
 * or the arguments of a body call to a default-mode formal with its class.
 * A compiler Clone argument is a site only for the ledger: its call edge and
 * class are read without demanding an analysis or failing the walk, and an
 * unreadable one leaves the ledger incomplete. */
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
                    PGY_CALL_HANDOFF_MEMBER_KEEP, call, p);
        return;
    }
    /* A call in a detached body runs again or elsewhere: it hands nothing
     * off, so its edge is not queried. */
    if (walk->detached_depth > 0) return;
    for (size_t p = 0; p < ast_call_arg_count(call); p++) {
        ASTNode *argument = ast_call_argument(call, p);
        ASTNode *operand = handoff_compiler_clone_operand(argument);
        ASTNode *name = operand != NULL ? operand : argument;
        if (name == NULL || name->type != AST_IDENTIFIER
            || semantic_collection_ownership_fact_find(walk->ctx, function_id,
                ast_identifier_binding_syntax_id(name)) == NULL)
            continue;
        /* The same published aggregate argument the move owner judges. A
         * Clone operand is the tracked Array<String> local itself. */
        if (operand == NULL) {
            SemanticPublishedValue value = semantic_param_publication_value(
                walk->ctx, argument);
            if (!value.ok || value.meta
                || !semantic_default_aggregate_value(walk->ctx, value.type))
                continue;
        }
        bool body_call = false;
        ASTNode *callee = NULL;
        size_t formal = 0;
        if (!semantic_param_publication_body_actual_current(walk->ctx,
                walk->routine, call, p, &body_call, &callee, &formal)) {
            if (operand != NULL) {
                walk->ledger_incomplete = true;
                continue;
            }
            semantic_error(walk->ctx, call,
                "Collection hand-off requires a current body-call argument fact");
            walk->failed = true;
            return;
        }
        if (!body_call) continue;
        unsigned members = 0u;
        if (operand != NULL) {
            FuncParam *param = ast_func_param(callee, formal);
            if (param == NULL || param->mode != PARAM_MODE_DEFAULT
                || ast_func_body(callee) == NULL)
                continue;
            if (!semantic_default_aggregate_formal_class_settled(walk->ctx,
                    callee, formal, &members)) {
                walk->ledger_incomplete = true;
                continue;
            }
        } else if (!semantic_default_aggregate_formal_class(walk->ctx, callee,
                formal, &members))
            continue;
        handoff_note_class(walk, callee, formal, members);
        handoff_note_site(walk, argument, false, members, call, formal);
    }
}

/* One copy row per compiler Clone of the body, in walk order; the call that
 * takes the copy names it when the Clone is that call's argument. */
static void
handoff_note_clone(HandoffWalk *walk, const ASTNode *clone)
{
    const ASTNode *operand = handoff_compiler_clone_operand(clone);
    PgyCallHandoffLedger *ledger = walk->ledger;
    if (operand == NULL || ledger == NULL) return;
    if (!handoff_grow((void **)&ledger->clones, &ledger->clone_capacity,
            ledger->clone_count, sizeof(*ledger->clones))) {
        walk->failed = true;
        return;
    }
    ledger->clones[ledger->clone_count++] = (PgyCallHandoffCloneRow){
        .function_syntax_id = ast_node_stable_id(walk->routine),
        .site = ast_node_stable_id(operand),
        .binding_syntax_id = operand->type == AST_IDENTIFIER
            ? ast_identifier_binding_syntax_id(operand) : 0,
        .callee = NULL,
        .argument_index = -1,
    };
}

static void
handoff_attribute_clones(HandoffWalk *walk, const ASTNode *call)
{
    PgyCallHandoffLedger *ledger = walk->ledger;
    const char *callee = handoff_callee_name(call);
    for (size_t p = 0; ledger != NULL && p < ast_call_arg_count(call); p++) {
        const ASTNode *operand = handoff_compiler_clone_operand(
            ast_call_argument(call, p));
        if (operand == NULL) continue;
        for (size_t i = ledger->clone_count; i > 0; i--) {
            PgyCallHandoffCloneRow *row = &ledger->clones[i - 1];
            if (row->site != ast_node_stable_id(operand)
                || row->function_syntax_id != ast_node_stable_id(walk->routine))
                continue;
            if (row->callee == NULL && callee != NULL) {
                row->callee = pergyra_strdup(callee);
                if (row->callee == NULL) walk->failed = true;
            }
            row->argument_index = (long)p;
            break;
        }
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
        if (walk->failed) return;
        handoff_note_clone(walk, node);
        handoff_attribute_clones(walk, node);
        if (!walk->failed) handoff_note_call(walk, node);
        return;
    default:
        handoff_walk_children(walk, node);
        return;
    }
}

static const HandoffBinding *
handoff_site_binding(const HandoffWalk *walk, const HandoffSite *at)
{
    const HandoffBinding *binding = NULL;
    for (size_t i = 0; i < walk->binding_count; i++)
        if (walk->bindings[i].binding == at->binding)
            binding = &walk->bindings[i];
    return binding;
}

/* The rule's inputs and answer for the site `at` of a declared binding. The
 * site is used again when it runs again without a new definition, or when
 * the binding is given to another constructor input. This walk reads no
 * later use: the move owner lands a Clone at a moved argument that is used
 * again (then the site is a Clone site, origin Fresh) or refuses the
 * program. A Clone site's later use is its repetition alone; it is not one
 * of the binding's constructor inputs. */
static PgyCallHandoffAction
handoff_site_action(const HandoffWalk *walk, const HandoffSite *at,
    const HandoffBinding *binding, PgyCallHandoffLater *later_out,
    PgyCallHandoffContext *context_out)
{
    size_t inputs = 0;
    for (size_t i = 0; at->constructor && i < walk->site_count; i++)
        if (walk->sites[i].constructor && !walk->sites[i].compiler_clone
            && walk->sites[i].binding == at->binding)
            inputs++;
    bool once = !at->repeats
        && (at->terminal || at->loop_depth == binding->loop_depth)
        && (!at->constructor || at->compiler_clone || inputs == 1);
    *later_out = once ? PGY_CALL_HANDOFF_LATER_ONCE
        : PGY_CALL_HANDOFF_LATER_USED_AGAIN;
    *context_out = at->repeats ? PGY_CALL_HANDOFF_CONTEXT_DETACHED
        : PGY_CALL_HANDOFF_CONTEXT_SYNC;
    return pgy_call_handoff_action(at->members,
        at->compiler_clone ? PGY_CALL_HANDOFF_ORIGIN_FRESH
            : PGY_CALL_HANDOFF_ORIGIN_LOCAL,
        *later_out, *context_out, PGY_CALL_HANDOFF_ELEMENTS_AS_GIVEN,
        PGY_CALL_HANDOFF_DIVERGENCE_NONE);
}

/* Whether the rule hands `binding` off at the site `at`, and the binding has
 * the one definition a hand-off is recorded for. A Clone site hands over
 * the copy, never the binding. */
static bool
handoff_site_admitted(const HandoffWalk *walk, const HandoffSite *at)
{
    const HandoffBinding *binding = handoff_site_binding(walk, at);
    if (binding == NULL || binding->definitions != 1 || at->compiler_clone)
        return false;
    PgyCallHandoffLater later;
    PgyCallHandoffContext context;
    return handoff_site_action(walk, at, binding, &later, &context)
        == PGY_CALL_HANDOFF_ACTION_HAND_OFF;
}

typedef struct {
    PgyCollectionHandoff *rows;
    size_t count, capacity;
} HandoffSelection;

/* Calls first: a binding a callee took is no longer a live constructor
 * input, so a constructor never selects a binding a call selected. */
static bool
handoff_select(HandoffWalk *walk, bool constructor,
    HandoffSelection *selection, size_t routine_start)
{
    uint32_t function_id = ast_node_stable_id(walk->routine);
    for (size_t i = 0; i < walk->site_count; i++) {
        HandoffSite *at = &walk->sites[i];
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
        at->selected = true;
    }
    return true;
}

/* One action row per site of a tracked local row of the routine, in walk
 * order, after the selection marked the site that hands each row off. */
static bool
handoff_ledger_actions(const HandoffWalk *walk)
{
    PgyCallHandoffLedger *ledger = walk->ledger;
    uint32_t function_id = ast_node_stable_id(walk->routine);
    for (size_t i = 0; i < walk->site_count; i++) {
        const HandoffSite *at = &walk->sites[i];
        const HandoffBinding *binding = handoff_site_binding(walk, at);
        if (binding == NULL
            || semantic_collection_ownership_fact_find(walk->ctx, function_id,
                at->binding) == NULL)
            continue;
        PgyCallHandoffLater later;
        PgyCallHandoffContext context;
        PgyCallHandoffAction action = handoff_site_action(walk, at, binding,
            &later, &context);
        const char *callee = handoff_callee_name(at->call);
        if (!handoff_grow((void **)&ledger->actions, &ledger->action_capacity,
                ledger->action_count, sizeof(*ledger->actions)))
            return false;
        PgyCallHandoffActionRow *row = &ledger->actions[ledger->action_count];
        *row = (PgyCallHandoffActionRow){
            .function_syntax_id = function_id,
            .call_site = ast_node_stable_id(at->call),
            .binding_syntax_id = at->binding,
            .parameter_index = (uint32_t)at->parameter_index,
            .callee = callee != NULL ? pergyra_strdup(callee) : NULL,
            .compiler_clone = at->compiler_clone,
            .members = at->members,
            .origin = at->compiler_clone ? PGY_CALL_HANDOFF_ORIGIN_FRESH
                : PGY_CALL_HANDOFF_ORIGIN_LOCAL,
            .later = later,
            .context = context,
            .elements = PGY_CALL_HANDOFF_ELEMENTS_AS_GIVEN,
            .action = action,
            .divergence = -1,
            .handed_off = at->selected,
        };
        if (callee != NULL && row->callee == NULL) return false;
        ledger->action_count++;
    }
    return true;
}

static bool
handoff_ledger_mark_incomplete(PgyCallHandoffLedger *ledger, uint32_t function_id)
{
    if (!handoff_grow((void **)&ledger->incomplete_function_syntax_ids,
            &ledger->incomplete_capacity, ledger->incomplete_count,
            sizeof(*ledger->incomplete_function_syntax_ids)))
        return false;
    ledger->incomplete_function_syntax_ids[ledger->incomplete_count++] =
        function_id;
    return true;
}

static void
handoff_ledger_truncate(PgyCallHandoffLedger *ledger, size_t actions,
    size_t clones)
{
    while (ledger->action_count > actions)
        free(ledger->actions[--ledger->action_count].callee);
    while (ledger->clone_count > clones)
        free(ledger->clones[--ledger->clone_count].callee);
}

/* Every routine is walked, also when the program has no tracked row: its
 * copy rows still belong in the ledger. */
bool
semantic_collection_ownership_select_handoffs(SemanticContext *ctx,
    PgyCollectionHandoff **rows_out, size_t *count_out,
    PgyCallHandoffLedger *ledger)
{
    SemanticParamPublicationView view;
    if (ctx == NULL || rows_out == NULL || count_out == NULL || *rows_out != NULL
        || ledger == NULL || !semantic_param_publication_admitted_view(ctx, &view))
        return false;
    *count_out = 0;
    ASTNode *previous_decl = ctx->current_function_decl;
    const char *previous_module = ctx->current_module_path;
    HandoffSelection selection = {0};
    bool ok = true;
    for (size_t r = 0; r < view.routine_count && ok; r++) {
        const PendingRoutine *row = &view.routines[r];
        if (row->decl == NULL || ast_func_body(row->decl) == NULL) continue;
        ctx->current_function_decl = row->decl;
        ctx->current_module_path = row->module;
        size_t actions = ledger->action_count, clones = ledger->clone_count;
        HandoffWalk walk = {.ctx = ctx, .routine = row->decl, .ledger = ledger};
        handoff_walk(&walk, ast_func_body(row->decl));
        ok = !walk.failed;
        size_t routine_start = selection.count;
        if (ok && !walk.incomplete)
            ok = handoff_select(&walk, false, &selection, routine_start)
                && handoff_select(&walk, true, &selection, routine_start);
        /* A body the walk could not enumerate publishes no action or copy
         * row. A Clone site whose class or edge could not be read is left
         * out alone: the routine's other rows, its hand-offs among them,
         * stay. Either way the routine is listed as incomplete. */
        if (ok && walk.incomplete)
            handoff_ledger_truncate(ledger, actions, clones);
        else if (ok)
            ok = handoff_ledger_actions(&walk);
        if (ok && (walk.incomplete || walk.ledger_incomplete))
            ok = handoff_ledger_mark_incomplete(ledger,
                ast_node_stable_id(row->decl));
        free(walk.bindings);
        free(walk.sites);
    }
    ctx->current_function_decl = previous_decl;
    ctx->current_module_path = previous_module;
    if (!ok) {
        free(selection.rows);
        pgy_call_handoff_ledger_clear(ledger);
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

void
pgy_call_handoff_ledger_clear(PgyCallHandoffLedger *ledger)
{
    if (ledger == NULL) return;
    for (size_t i = 0; i < ledger->action_count; i++)
        free(ledger->actions[i].callee);
    for (size_t i = 0; i < ledger->clone_count; i++)
        free(ledger->clones[i].callee);
    free(ledger->classes);
    free(ledger->actions);
    free(ledger->clones);
    free(ledger->incomplete_function_syntax_ids);
    *ledger = (PgyCallHandoffLedger){0};
}

bool
pgy_call_handoff_ledger_incomplete(const PgyCallHandoffLedger *ledger,
    uint32_t function_syntax_id)
{
    for (size_t i = 0; ledger != NULL && i < ledger->incomplete_count; i++)
        if (ledger->incomplete_function_syntax_ids[i] == function_syntax_id)
            return true;
    return false;
}
