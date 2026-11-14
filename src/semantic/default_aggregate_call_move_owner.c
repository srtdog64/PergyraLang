/* Call-local effects for the default aggregate move walk: copied getter
 * results, indexed-assignment backing loans, callee moves and overlapping
 * moving actuals. Control-flow state joining remains in the walk owner. */
#include "default_aggregate_move_internal.h"
#include "default_aggregate_move_rule.h"
#include "collection_ownership_fact.h"
#include "loan_copy_landing_owner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtin_kind.h"
#include "diag_codes.h"
#include "function_param_flow/function_param_publication_internal.h"
#include "function_param_flow/function_param_publication_owner.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "slot_summary.h"
#include "type_checker/type_checker_builtins_stdlib_collections_internal.h"
#include "type_checker/type_checker_call_constructor_internal.h"
#include "type_checker/type_checker_internal.h"
#include "type_checker/type_checker_ownership_internal.h"

static bool
call_copied(const MoveWalk *walk, const ASTNode *call)
{
    for (size_t i = 0; i < walk->copied_count; i++)
        if (walk->copied[i] == call) return true;
    return false;
}

bool
move_walk_note_copied_argument(MoveWalk *walk, const ASTNode *call)
{
    const ASTNode *argument = semantic_loan_copy_landing_copied_call(
        walk->ctx, call);
    if (argument == NULL) return true;
    if (walk->copied_count == walk->copied_capacity) {
        size_t capacity = walk->copied_capacity == 0
            ? 8 : walk->copied_capacity * 2;
        const ASTNode **grown = realloc(
            walk->copied, capacity * sizeof(*grown));
        if (grown == NULL) { walk->failed = true; return false; }
        walk->copied = grown;
        walk->copied_capacity = capacity;
    }
    walk->copied[walk->copied_count++] = argument;
    return true;
}

static bool
assignment_loan_overlaps(const MoveAssignmentBackingLoan *loan,
                         uint32_t binding, const char *fields)
{
    return loan != NULL && loan->binding == binding
        && (move_walk_path_contains(loan->fields, fields)
            || move_walk_path_contains(fields, loan->fields));
}

MoveAssignmentBackingLoan *
move_walk_assignment_loan_for_path(MoveWalk *walk,
    const MoveState *state, const ASTNode *path,
    MoveAssignmentBackingLoan *loans)
{
    char fields[512];
    uint32_t binding = 0;
    if (loans == NULL
        || !move_walk_access_path(path, &binding, fields, sizeof(fields)))
        return NULL;
    MoveCopyTraversal traversal;
    MoveCopyReach reach;
    if (!move_walk_copy_begin(walk, state, binding, fields, &traversal)) return NULL;
    while (move_walk_copy_next(&traversal, &reach)) {
        for (MoveAssignmentBackingLoan *loan = loans;
             loan != NULL; loan = loan->previous) {
            if (assignment_loan_overlaps(
                    loan, reach.binding, reach.fields)) {
                move_walk_copy_end(&traversal);
                return loan;
            }
        }
    }
    move_walk_copy_end(&traversal);
    return NULL;
}

void
move_walk_refuse_assignment_loan_change(MoveWalk *walk,
    const MoveAssignmentBackingLoan *loan, ASTNode *site)
{
    if (loan == NULL || !move_walk_reported_once(walk, site)) return;
    const ASTNode *root = loan->receiver;
    while (root != NULL
        && (root->type == AST_MEMBER_ACCESS
            || root->type == AST_ARRAY_ACCESS))
        root = root->type == AST_MEMBER_ACCESS
            ? ast_member_object(root) : ast_array_access_array(root);
    char display[512];
    const char *name = root != NULL && root->type == AST_IDENTIFIER
        ? ast_identifier_name(root) : NULL;
    if (name != NULL
        && strlen(name) + strlen(loan->fields) < sizeof(display)) {
        strcpy(display, name);
        strcat(display, loan->fields);
        name = display;
    }
    semantic_error_with_hints(walk->ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_RECLAIM_SOURCE_OR_DROP_VIEW, site,
        "Cannot change '%s' while an indexed assignment keeps its backing address.\n"
        "Reason:\n"
        "- the assignment evaluates its target before its value\n"
        "- this operation may write, grow, release or transfer the same backing before the store\n"
        "Fix:\n"
        "- finish the backing-changing operation before starting the indexed assignment\n"
        "- or compute the index and value first, then perform the indexed store",
        name != NULL ? name : "collection backing");
}

static bool
call_argument_changes_assignment_backing(MoveWalk *walk,
    ASTNode *call, size_t ordinal)
{
    if (!semantic_param_publication_checked_call(walk->ctx, call)
        || !semantic_param_publication_checked_argument(
            walk->ctx, call, ordinal))
        return true;
    if (semantic_stdlib_collection_argument_mutates_storage(call, ordinal))
        return true;
    SemanticPublishedValue argument = semantic_param_publication_value(
        walk->ctx, ast_call_argument(call, ordinal));
    if (!argument.ok || argument.meta) return true;
    if (type_equals(argument.type, TYPE_STRING)
        || semantic_classify_ownership_type(argument.type, walk->ctx)
            == OWNERSHIP_TYPE_COPY_ONLY)
        return false;
    uint32_t kind = 0;
    if (ast_call_semantic_callee_builtin_kind(call, &kind)
        && kind == BUILTIN_CLONE)
        return false;
    ASTNode *callee = NULL;
    size_t formal = 0;
    bool body_call = false;
    if (!semantic_param_publication_body_actual_current(walk->ctx,
            walk->routine, call, ordinal, &body_call, &callee, &formal))
        return true;
    if (!body_call)
        return !semantic_param_publication_argument_borrowed_for_call(
            walk->ctx, call, ordinal);
    FuncParam *param = ast_func_param(callee, formal);
    if (param == NULL) return true;
    unsigned summary = function_param_flow_summary_for_call(
        &(SlotFunctionLookup){walk->ctx, walk->ctx->program_root},
        call, ordinal);
    unsigned changes = SLOT_PARAM_SUMMARY_WRITE | SLOT_PARAM_SUMMARY_RELEASE
        | SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE
        | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
    if (param->mode == PARAM_MODE_MUT_REF || param->mode == PARAM_MODE_OWN
        || (summary & changes) != 0)
        return true;
    return type_is_constructed_named(argument.type, "Array")
        && !function_param_flow_preserves_array_storage(
            walk->ctx, callee, formal);
}

void
move_walk_check_assignment_loan_call(MoveWalk *walk,
    const MoveState *state, ASTNode *call)
{
    if (walk->assignment_loan == NULL) return;
    for (size_t p = 0; p < ast_call_arg_count(call); p++) {
        ASTNode *argument = ast_call_argument(call, p);
        MoveAssignmentBackingLoan *loan = move_walk_assignment_loan_for_path(
            walk, state, argument, walk->assignment_loan);
        if (walk->failed) return;
        if (loan != NULL
            && call_argument_changes_assignment_backing(walk, call, p))
            move_walk_refuse_assignment_loan_change(walk, loan, argument);
    }
}

static ParamMode
routine_param_mode(const ASTNode *routine, uint32_t binding, bool *found)
{
    *found = false;
    for (size_t p = 0;
         routine != NULL && p < ast_func_param_count(routine); p++) {
        FuncParam *param = ast_func_param(routine, p);
        if (param != NULL
            && ast_func_param_stable_id(param) == binding) {
            *found = true;
            return param->mode;
        }
    }
    return PARAM_MODE_DEFAULT;
}

/* Copy landing reads the publication-owned prepared flow generation. It never
 * opens a per-local demand: that would make program traversal order another
 * summary owner and can expand one recursive closure past its fixed budget.
 * Unknown or failed facts remain false and preserve the borrow refusal. */
static const ASTNode *
local_ref_carrier_for_move_landing(MoveWalk *walk, const ASTNode *argument,
    bool parameter)
{
    /* Do not open the routine's parameter-flow demand merely because an own
     * call has an aggregate expression.  Only a named local can be the
     * immutable carrier whose initializer is eligible for this landing.  In
     * particular, probing constructor/call expressions here can poison an
     * otherwise independent fail-closed summary and suppress its later exact
     * borrow diagnostic. */
    if (walk == NULL || !walk->landing_only || parameter || argument == NULL ||
        argument->type != AST_IDENTIFIER)
        return NULL;
    return function_param_flow_local_ref_carrier_copy_source_current(
        walk->ctx, walk->routine, argument);
}

/* A default struct formal may move storage from only some top-level fields.
 * The parameter-flow field verdict is the existing authority for that split;
 * keeping one whole-value move here would discard it and falsely consume
 * preserved sibling fields.  Own formals remain whole-value transfers. */
static bool
record_default_struct_field_moves(MoveWalk *walk, MoveState *state,
    ASTNode *argument, ASTNode *callee, size_t formal,
    const char *callee_name, const char *param_name, ASTNode *call,
    bool lends, bool copy_on_reuse, bool outward_borrow_escape)
{
    SemanticPublishedValue value = semantic_param_publication_value(
        walk->ctx, argument);
    ASTNode *decl = value.ok && !value.meta && value.type != NULL
        && value.type->kind == TYPE_KIND_CLASS
        ? semantic_host_decl_for_type(walk->ctx, value.type) : NULL;
    char root[512], path[512];
    uint32_t binding = 0;
    if (decl == NULL || decl->type != AST_CLASS_DECL
        || !move_walk_access_path(argument, &binding, root, sizeof(root)))
        return false;

    size_t moved_fields = 0;
    for (size_t i = 0; ; i++) {
        const char *name = NULL;
        Type *field_type = constructor_decl_field_resolved_type_at(
            decl, i, &name, walk->ctx);
        if (name == NULL) break;
        if (!semantic_default_aggregate_value(walk->ctx, field_type))
            continue;
        if (function_param_flow_field_preservation_failures(
                walk->ctx, callee, formal, name) == 0)
            continue;
        if ((size_t)snprintf(path, sizeof(path), "%s.%s", root, name)
            >= sizeof(path))
            return false;
        moved_fields++;
    }
    if (moved_fields == 0) return false;

    for (size_t i = 0; !walk->failed; i++) {
        const char *name = NULL;
        Type *field_type = constructor_decl_field_resolved_type_at(
            decl, i, &name, walk->ctx);
        if (name == NULL) break;
        if (!semantic_default_aggregate_value(walk->ctx, field_type))
            continue;
        if (function_param_flow_field_preservation_failures(
                walk->ctx, callee, formal, name) == 0)
            continue;
        if ((size_t)snprintf(path, sizeof(path), "%s.%s", root, name)
            >= sizeof(path)) {
            walk->failed = true;
            break;
        }
        if (!move_walk_state_add(walk, state, binding, path,
                callee_name, callee, param_name, lends ? call : NULL, 0,
                NULL, argument, copy_on_reuse, outward_borrow_escape))
            break;
        if (!lends)
            move_walk_copy_changed_at(walk, state, argument, binding,
                path, copy_on_reuse);
    }
    return true;
}

void
move_walk_apply_call_moves(MoveWalk *walk, MoveState *state, ASTNode *call)
{
    for (size_t p = 0; p < ast_call_arg_count(call); p++) {
        ASTNode *arg = ast_call_argument(call, p);
        char fields[512];
        uint32_t binding = 0;
        if (!move_walk_access_path(arg, &binding, fields, sizeof(fields)))
            continue;
        SemanticPublishedValue value = semantic_param_publication_value(
            walk->ctx, arg);
        if (!value.ok || value.meta
            || !semantic_default_aggregate_value(walk->ctx, value.type))
            continue;
        ASTNode *callee = NULL;
        size_t formal = 0;
        bool body_call = false;
        if (!semantic_param_publication_body_actual_current(walk->ctx,
                walk->routine, call, p, &body_call, &callee, &formal)) {
            if (!walk->ctx->has_error) {
                const ASTNode *target = ast_call_callee(call);
                const char *name = target != NULL && target->type == AST_IDENTIFIER
                    ? ast_identifier_name(target)
                    : target != NULL && target->type == AST_MEMBER_ACCESS
                        ? ast_member_name(target) : NULL;
                semantic_error(walk->ctx, call,
                    "Default aggregate move check requires a current body-call argument fact for '%s'",
                    name != NULL ? name : "call");
                walk->failed = true;
            }
            return;
        }
        if (!body_call)
            continue;
        FuncParam *param = ast_func_param(callee, formal);
        if (param == NULL) continue;
        bool parameter = false;
        ParamMode mode = routine_param_mode(
            walk->routine, binding, &parameter);
        bool prepublication_ref_loan = parameter && mode == PARAM_MODE_REF;
        const ASTNode *local_ref_carrier =
            (param->mode == PARAM_MODE_OWN ||
                param->mode == PARAM_MODE_DEFAULT)
            ? local_ref_carrier_for_move_landing(walk, arg, parameter)
            : NULL;
        bool prepublication_inout_move = parameter
            && mode == PARAM_MODE_MUT_REF
            && fields[0] == '\0'
            && param->mode == PARAM_MODE_DEFAULT;
        /* Do not demand unrelated default-call summaries for borrowed
         * parameters in the early consumer. A named local is different: it
         * is the caller-owned value on which the ordinary moved-then-used
         * rule must select a Clone before downstream storage finalization. */
        if (walk->landing_only && param->mode != PARAM_MODE_OWN
            && param->mode == PARAM_MODE_DEFAULT && !prepublication_ref_loan)
            prepublication_ref_loan =
                function_param_flow_value_borrows_ref_formal_current(
                    walk->ctx, walk->routine, arg);
        if (walk->landing_only && param->mode != PARAM_MODE_OWN
            && !(param->mode == PARAM_MODE_DEFAULT
                && (prepublication_ref_loan
                    || prepublication_inout_move
                    || !parameter)))
            continue;
        /* An inout aggregate changes only the fields its summary says it
         * writes.  Preserve sibling fields instead of turning a field-local
         * mutation into a whole-value storage change. */
        if (!walk->landing_only && param->mode == PARAM_MODE_MUT_REF
            && function_param_flow_array_preservation_failures(
                walk->ctx, callee, formal) != 0)
            move_walk_copy_inout_changed(
                walk, state, arg, callee, formal);
        if (!semantic_default_aggregate_formal_moves(
                walk->ctx, callee, formal))
            continue;
        bool lends = semantic_default_aggregate_formal_lends(
            walk->ctx, callee, formal);
        SlotFunctionLookup flow_lookup = {
            walk->ctx, walk->ctx->program_root};
        FunctionParamFlowSummaryStore *flow_store =
            function_param_flow_store_get(&flow_lookup);
        bool exact_owned_restore = lends &&
            param->mode == PARAM_MODE_DEFAULT &&
            (!parameter || mode == PARAM_MODE_DEFAULT ||
                mode == PARAM_MODE_OWN) &&
            flow_store != NULL &&
            function_param_flow_call_result_restores_actual_current(
                flow_store, walk->routine, call, p);
        /* Copying the actual does not turn an opaque call/channel escape
         * into an owned transfer. Keep that fail-closed boundary for the
         * complete checker; only a bounded return/write/release move can be
         * repaired by the local moved-then-used landing. */
        unsigned outward_effects = SLOT_PARAM_SUMMARY_NONE;
        bool outward_current = flow_store != NULL
            && function_param_flow_formal_outward_effects_current(
                flow_store, callee, formal, &outward_effects);
        bool borrow_handoff = param->mode == PARAM_MODE_DEFAULT
            && semantic_default_aggregate_formal_borrow_handoff(
                walk->ctx, callee, formal);
        if ((walk->landing_only || walk->planning_only) && !parameter
            && param->mode == PARAM_MODE_DEFAULT
            && (!outward_current || borrow_handoff))
            continue;
        /* A local aggregate can be only a carrier of a ref formal's backing.
         * Its last use is not an ownership transfer: handing that carrier to
         * an own formal, or to a default formal that consumes rather than
         * returns it, would let the caller's storage escape.  The backing
         * frame owns that provenance, and the immutable local definition is
         * the only place a Clone can make the later named handoff independent.
         * A direct ref argument and a lending default formal are deliberately
         * excluded because neither is a new ownership boundary. */
        if (walk->landing_only &&
            (param->mode == PARAM_MODE_OWN ||
                (param->mode == PARAM_MODE_DEFAULT && !lends)) &&
            local_ref_carrier != NULL &&
            semantic_loan_copy_landing_carrier_copies(
                walk->ctx, local_ref_carrier)) {
            if (!semantic_loan_copy_landing_select_carrier(
                    walk->ctx, local_ref_carrier))
                walk->failed = true;
            continue;
        }
        if (walk->landing_only && param->mode == PARAM_MODE_OWN &&
            local_ref_carrier != NULL) {
            FunctionParamFlowLoanEscapes copies = {0};
            if (function_param_flow_local_ref_copy_cut_current(walk->ctx,
                    walk->routine, arg, &copies)) {
                for (size_t c = 0; c < copies.count; c++)
                    if (!semantic_loan_copy_landing_select_carrier(
                            walk->ctx, copies.at[c]))
                        walk->failed = true;
                continue;
            }
        }
        /* Automatic copy-on-reuse belongs only to the default aggregate
         * value contract. An explicit `own` formal is an authority transfer;
         * inserting Clone at that boundary would both change the contract
         * and violate the named-value boundary. */
        bool copy_on_reuse = param->mode == PARAM_MODE_DEFAULT
            && outward_current && !borrow_handoff;
        bool outward_borrow_escape = param->mode == PARAM_MODE_DEFAULT
            && (!outward_current || borrow_handoff);
        const char *callee_name = ast_declaration_name(callee) != NULL
            ? ast_declaration_name(callee) : "callee";
        const char *param_name = param != NULL && param->name != NULL
            ? param->name : "parameter";
        /* A true call/channel escape is owned by the parameter-flow summary.
         * A borrowed caller carries that fact outward; treating it as a local
         * move here would either invent a copy or refuse the handoff before
         * the actual later mutation/drop that makes it unsafe. */
        if (parameter && mode != PARAM_MODE_DEFAULT &&
            mode != PARAM_MODE_OWN &&
            semantic_default_aggregate_formal_borrow_handoff(
                walk->ctx, callee, formal))
            continue;
        /* A whole inout place handed to a consuming default formal follows
         * the ordinary move rule: a later use selects an independent Clone,
         * but a last use transfers without one.  A lending formal is handled
         * below by the same sparse-loan proof as an inout member; recording
         * it as a move would make the exit checker invent a storage change. */
        if (prepublication_inout_move && !lends) {
            unsigned summary = function_param_flow_summary_for_param(
                walk->ctx, callee, formal);
            const unsigned escapes = SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                | SLOT_PARAM_SUMMARY_CALL_ESCAPE
                | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
            if (walk->landing_only
                && (summary & escapes) == SLOT_PARAM_SUMMARY_RETURN_ESCAPE) {
                if (!semantic_loan_copy_landing_select_carrier(
                        walk->ctx, arg))
                    walk->failed = true;
                continue;
            }
            if (!move_walk_state_add(walk, state, binding, fields,
                    callee_name, callee, param_name, NULL, 0,
                    NULL, arg, copy_on_reuse, outward_borrow_escape))
                return;
            move_walk_copy_changed(walk, state, arg, copy_on_reuse);
            continue;
        }
        /* Before publication, ordinary default-mode moves still belong to
         * the complete move checker. Pull forward only explicit-own copy
         * repair and read-and-return loans; otherwise an opaque retaining
         * call could be rewritten into a copy and erase its required
         * escaped-storage refusal. */
        if (walk->landing_only && parameter
            && param->mode != PARAM_MODE_OWN && !lends
            && !prepublication_inout_move)
            continue;
        if (parameter && mode == PARAM_MODE_REF && lends
            && !exact_owned_restore) {
            /* A direct ref formal is borrowed authority, not a caller-owned
             * value on which automatic copy-on-reuse may establish a new
             * transfer when the callee changes its backing.  A true lending
             * call that preserves the backing may still use the existing
             * sparse-loan proof and copy its returned carrier independently.
             * An exact immutable local carrier of a ref formal remains
             * handled by the local-carrier copy-cut owner above. */
            /* Returning a scalar element may make element preservation
             * unproved without changing the aggregate's backing descriptor.
             * A direct ref may lend that scalar through the existing sparse
             * loan proof.  Structural growth remains refused: it withdraws
             * descriptor preservation, as AddBlock(ref_rows) does. */
            unsigned preservation_failures =
                function_param_flow_array_preservation_failures(
                    walk->ctx, callee, formal);
            bool preserves_backing =
                (preservation_failures &
                    FUNCTION_PARAM_ARRAY_DESCRIPTOR_UNPROVED) == 0;
            if (walk->landing_only && preserves_backing) {
                move_walk_prepare_lent_argument(walk, binding, fields,
                    callee_name, param_name, call, arg);
                continue;
            }
            if (walk->landing_only)
                continue;
            if (preserves_backing)
                continue;
        } else if (walk->landing_only && prepublication_ref_loan && lends
            && !exact_owned_restore) {
            move_walk_prepare_lent_argument(walk, binding, fields,
                callee_name, param_name, call, arg);
            continue;
        }
        /* An inout place may be handed through a read-and-return default
         * formal without a deep Clone when the sparse loan owner proves the
         * result is restored and no overlapping owner changes while it is
         * away.  This applies to both the whole formal and an exact member.
         * The landing pass still selects a copy when that proof needs one;
         * the final pass admits only the fully proved loan. */
        bool inout_loan = parameter && mode == PARAM_MODE_MUT_REF && lends;
        if (inout_loan) {
            if (walk->landing_only) {
                move_walk_prepare_lent_argument(walk, binding, fields,
                    callee_name, param_name, call, arg);
                continue;
            }
            FunctionParamFlowLoanFailure failure = {0};
            if (function_param_flow_lent_argument_admitted(walk->ctx,
                    walk->routine, call, arg, binding, &failure))
                continue;
        }
        if (parameter && mode != PARAM_MODE_DEFAULT
            && mode != PARAM_MODE_OWN) {
            /* A non-returning move of borrowed storage cannot be repaired by
             * copying: it would either transfer an opaque borrow or discard
             * an inout mutation. Only the read-and-return case above has a
             * restoration proof and may select an independent copy. */
            if (lends && !semantic_loan_copy_landing_select_carrier(
                    walk->ctx, arg))
                walk->failed = true;
            if (move_walk_reported_once(walk, arg))
                semantic_error_with_hints(walk->ctx,
                    PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE,
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
        if (lends && call_copied(walk, call)) continue;
        /* A local or by-value owner at its last use moves into a lending
         * call without a copy.  The landing pass must still retain that
         * move row: if the source is used later, this owner is the one that
         * selects the independent argument Clone before array-storage
         * finalization consumes the call summary.  An ordinary local's true
         * last use still adds no copy. */
        if (param->mode == PARAM_MODE_DEFAULT
            && record_default_struct_field_moves(
                walk, state, arg, callee, formal, callee_name,
                param_name, call, lends, copy_on_reuse,
                outward_borrow_escape))
            continue;
        if (!move_walk_state_add(walk, state, binding, fields,
                callee_name, callee, param_name, lends ? call : NULL, 0,
                NULL, arg, copy_on_reuse, outward_borrow_escape))
            return;
        if (!lends)
            move_walk_copy_changed(
                walk, state, arg, copy_on_reuse);
    }
}

void
move_walk_apply_constructor_moves(MoveWalk *walk, MoveState *state,
                                  ASTNode *call)
{
    PublishedNode *published = publication_result_current(walk->ctx, call);
    const SemanticNominalResultSnapshot *snapshot = published != NULL
        ? published->nominal_result : NULL;
    if (snapshot == NULL
        || !semantic_nominal_result_snapshot_current(
            walk->ctx, call, snapshot)
        || snapshot->field_count != ast_call_arg_count(call))
        return;

    /* The first type-check pass can see a direct ref argument, but it cannot
     * yet know that an immutable local carries exactly one ref formal's
     * backing.  The sealed backing frames own that later fact.  When this
     * nominal result leaves only through the routine return, copy the local
     * at its definition so the returned owner is independent.  Results that
     * stay in the routine are left alone, and an unsupported Clone keeps the
     * ordinary fail-closed escape diagnostic in the final pass. */
    if (walk->landing_only) {
        bool has_local_ref_carrier = false;
        for (size_t p = 0; p < snapshot->field_count; p++) {
            ASTNode *argument = ast_call_argument(call, p);
            if (function_param_flow_local_ref_carrier_copy_source_current(
                    walk->ctx, walk->routine, argument) != NULL) {
                has_local_ref_carrier = true;
                break;
            }
        }
        /* The return-flow query can select the exact copy boundary for a
         * carrier.  Do not run that mutating query for an unrelated nominal
         * value: in particular, a copy-only constructor stored by ArraySet
         * owns no carrier and needs no landing. */
        if (has_local_ref_carrier
            && function_param_flow_call_result_returns_current(
                walk->ctx, walk->routine, call)) {
            for (size_t p = 0; p < snapshot->field_count; p++) {
                ASTNode *argument = ast_call_argument(call, p);
                const ASTNode *copy =
                    function_param_flow_local_ref_carrier_copy_source_current(
                        walk->ctx, walk->routine, argument);
                if (copy != NULL &&
                    !semantic_loan_copy_landing_select_carrier(
                        walk->ctx, copy)) {
                    walk->failed = true;
                    return;
                }
            }
        }
    }
    const char *constructor = snapshot->decl_name != NULL
        ? snapshot->decl_name : "constructor";
    for (size_t p = 0; p < snapshot->field_count; p++) {
        const SemanticNominalResultField *field = &snapshot->fields[p];
        ASTNode *argument = ast_call_argument(call, p);
        char path[512];
        uint32_t binding = 0;
        bool owned_elements = field->string_array_elements.owned
            || (walk->landing_only
                && move_walk_copy_restored_argument(walk, argument));
        if (!field->string_array_elements_present
            || !owned_elements
            || !semantic_collection_owned_member_constructor_source(
                argument, walk->ctx)
            || !move_walk_access_path(
                argument, &binding, path, sizeof(path)))
            continue;
        /* A copy selected earlier in this walk is the constructor's fresh
         * field storage in the next landed analysis.  Do not reintroduce a
         * move of the original path; later constructor sites can then be
         * judged in this same sealed generation. */
        if (walk->landing_only
            && semantic_loan_copy_landing_selected(
                walk->ctx, argument))
            continue;
        if (!move_walk_state_add(walk, state, binding, path,
                constructor, NULL,
                field->field_name != NULL ? field->field_name : "field",
                NULL, 0, NULL, argument, true, false))
            return;
    }
}

bool
move_walk_reject_overlapping_moving_arguments(MoveWalk *walk,
                                              ASTNode *call)
{
    for (size_t moving = 0; moving < ast_call_arg_count(call); moving++) {
        ASTNode *moving_argument = ast_call_argument(call, moving);
        SemanticPublishedValue moving_value =
            semantic_param_publication_value(walk->ctx, moving_argument);
        uint32_t moving_binding = 0;
        char moving_fields[512];
        if (!moving_value.ok || moving_value.meta
            || !semantic_default_aggregate_value(walk->ctx, moving_value.type)
            || !move_walk_access_path(moving_argument, &moving_binding,
                moving_fields, sizeof(moving_fields)))
            continue;
        ASTNode *callee = NULL;
        size_t formal = 0;
        bool body_call = false;
        if (!semantic_param_publication_body_actual_current(walk->ctx,
                walk->routine, call, moving, &body_call, &callee, &formal)) {
            if (!walk->ctx->has_error) {
                const ASTNode *target = ast_call_callee(call);
                const char *name = target != NULL && target->type == AST_IDENTIFIER
                    ? ast_identifier_name(target)
                    : target != NULL && target->type == AST_MEMBER_ACCESS
                        ? ast_member_name(target) : NULL;
                semantic_error(walk->ctx, call,
                    "Default aggregate overlap check requires a current body-call argument fact for '%s'",
                    name != NULL ? name : "call");
                walk->failed = true;
            }
            return false;
        }
        if (!body_call || !semantic_default_aggregate_formal_moves(
                walk->ctx, callee, formal))
            continue;
        for (size_t other = 0;
             other < ast_call_arg_count(call); other++) {
            if (other == moving) continue;
            ASTNode *other_argument = ast_call_argument(call, other);
            SemanticPublishedValue other_value =
                semantic_param_publication_value(walk->ctx, other_argument);
            uint32_t other_binding = 0;
            char other_fields[512];
            if (!other_value.ok || other_value.meta
                || !semantic_default_aggregate_value(
                    walk->ctx, other_value.type)
                || !move_walk_access_path(other_argument, &other_binding,
                    other_fields, sizeof(other_fields))
                || moving_binding != other_binding
                || (!move_walk_path_contains(moving_fields, other_fields)
                    && !move_walk_path_contains(
                        other_fields, moving_fields)))
                continue;
            if (move_walk_reported_once(walk, other_argument)) {
                semantic_error_with_hints(walk->ctx,
                    PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, other_argument,
                    "A call cannot pass overlapping aggregate storage to moving parameter %zu and argument %zu.\n"
                    "Reason:\n"
                    "- the moving parameter may grow, release, return or retain that storage\n"
                    "- the overlapping argument would then observe the same storage through another path\n"
                    "Fix:\n"
                    "- pass distinct aggregate owners\n"
                    "- or materialize one independent Clone before the call",
                    moving + 1, other + 1);
            }
            return false;
        }
    }
    return true;
}
