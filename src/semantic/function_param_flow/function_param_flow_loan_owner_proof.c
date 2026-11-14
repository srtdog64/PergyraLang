/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * The loan owner proof: the storage root of a place and whether a routine
 * changes a loan's owner while the loan lives. Final eager loan validation
 * consumes the sparse flow owner's indexed facts.
 */

#include "../type_checker/type_checker_internal.h"
#include "../type_checker/type_checker_ownership_internal.h"
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "../slot_analyzer_internal.h"
#include "function_param_flow_loan_order.h"
#include "../default_aggregate_move_internal.h"
#include "../typed_map_query_fact_owner.h"
#include <stdlib.h>
#include <string.h>

bool function_param_flow_loan_storage_root(FunctionParamFlowSummaryStore *store,
    FunctionParamFlowIndexClaims *claims, ASTNode *caller, const ASTNode *place, const ASTNode **root)
{
    if (claims == NULL || !slot_call_argument_storage_root(place, root)) return false;
    for (size_t step = 0; *root != NULL && step <= claims->count; step++) {
        uint32_t binding = semantic_param_publication_binding(store->ctx, *root);
        if (binding == 0) return false;
        FunctionParamFlowIndexClaim *declaration = NULL;
        for (size_t c = 0; c < claims->count; c++)
            if (claims->rows[c].id == binding) { declaration = &claims->rows[c]; break; }
        if (declaration == NULL) {
            for (size_t p = 0; p < ast_func_param_count(caller); p++)
                if (binding == ast_func_param_stable_id(ast_func_param(caller, p))) return true;
            return false;
        }
        /* A binding a case pattern makes holds part of the match subject:
         * its root is the subject's, when a binding roots the subject. A
         * call's result as subject roots nothing the proof could follow. */
        if (declaration->shape == AST_IDENTIFIER) {
            const ASTNode *subject = function_param_flow_claim_match_subject(claims, (size_t)(declaration - claims->rows));
            const ASTNode *subject_root = NULL;
            if (subject == NULL || !slot_call_argument_storage_root(subject, &subject_root) || subject_root == NULL)
                return false;
            *root = subject_root;
            continue;
        }
        if (declaration->shape != AST_LET_DECL || declaration->node == NULL ||
            declaration->node->type != AST_LET_DECL || ast_node_stable_id(declaration->node) != binding ||
            declaration->local_alias != ast_let_is_alias(declaration->node)) return false;
        if (!declaration->local_alias) return true;
        ASTNode *initializer = ast_let_initializer(declaration->node);
        if (initializer == NULL || declaration->child_count != 2 || declaration->children[0] != initializer ||
            declaration->child_ids[0] == 0 || declaration->child_ids[0] != ast_node_stable_id(initializer) ||
            !semantic_param_publication_value(store->ctx, initializer).ok ||
            !slot_call_argument_storage_root(initializer, root) || *root == NULL) return false;
    }
    return *root == NULL;
}

/* A temporary nominal value can stay in the routine yet be changed by the
 * outer call that directly consumes it.  The result-flow walk owns outward
 * lifetime; claim-parent identity and the callee's settled preservation fact
 * own this same-expression mutation edge. */
static bool nominal_result_direct_consumer_preserves(
    FunctionParamFlowSummaryStore *store, ASTNode *caller, ASTNode *result,
    size_t result_field_ordinal)
{
    ASTNode *consumer = NULL;
    size_t actual = SIZE_MAX, formal = SIZE_MAX;
    if (!function_param_flow_call_result_argument_consumer_current(store,
            caller, result, &consumer, &actual))
        return true;
    ASTNode *callee = semantic_param_publication_callee(store->ctx, consumer,
        actual);
    if (callee == NULL)
        return semantic_param_publication_nominal_result_complete(store->ctx,
                consumer) ||
            semantic_param_publication_argument_borrowed_for_call(store->ctx,
                consumer, actual);
    if (!semantic_param_publication_formal_for_actual(
            store->ctx, consumer, actual, &formal) ||
        formal >= ast_func_param_count(callee))
        return false;
    FuncParam *parameter = ast_func_param(callee, formal);
    if (parameter == NULL || parameter->mode == PARAM_MODE_MUT_REF ||
        parameter->mode == PARAM_MODE_OWN)
        return false;
    if (parameter->mode == PARAM_MODE_REF) return true;
    PublishedNode *published = publication_result_current(store->ctx, result);
    const SemanticNominalResultSnapshot *snapshot = published != NULL
        ? published->nominal_result : NULL;
    if (snapshot == NULL || !semantic_nominal_result_snapshot_current(
            store->ctx, result, snapshot) ||
        result_field_ordinal >= snapshot->field_count ||
        snapshot->fields[result_field_ordinal].field_name == NULL)
        return false;
    return function_param_flow_field_preservation_failures(store->ctx,
        callee, formal, snapshot->fields[result_field_ordinal].field_name) == 0;
}

/* No assignment, writing call or move in `caller` reaches `binding`. The
 * owner may still be lent on to a `ref` formal. With `lends_on`, a by-value
 * formal that only reads it and returns it or a part of it lends it on too,
 * unless the owner is a local and that call's result leaves the routine
 * (2026-10-19_by_value_getter_lends_when_used_again). With
 * `own_descriptor`, a push or set with `binding` itself as the receiver
 * changes only that binding's own descriptor. A claim run outside `span`
 * changes nothing under the loan. A `carrier` holds the loan rather than
 * owning other loans: handing it on is the loan's own escape, which the
 * loan frame refuses unless the loan may leave. */
static bool direct_place_path_current(FunctionParamFlowSummaryStore *store,
    ASTNode *caller, const ASTNode *place, uint32_t binding,
    char *fields, size_t fields_size)
{
    uint32_t place_binding = 0;
    ASTNode *published_caller = NULL, *root = NULL;
    return store != NULL && caller != NULL && place != NULL && fields != NULL
        && move_walk_access_path(place, &place_binding, fields, fields_size)
        && place_binding == binding
        && semantic_param_publication_projection_root_current(store->ctx,
            place, &published_caller, &root)
        && published_caller == caller && root != NULL
        && root->type == AST_IDENTIFIER
        && ast_identifier_binding_syntax_id(root) == binding;
}

/* Only two current direct paths of the same typed root can prove they are
 * disjoint. An alias-derived, stale or otherwise unproved path remains
 * overlapping. */
static bool loan_place_overlaps(FunctionParamFlowSummaryStore *store,
    ASTNode *caller, uint32_t binding, const ASTNode *owner_place,
    const FunctionParamFlowResultPath *owner_path, const ASTNode *candidate)
{
    if (function_param_flow_result_path_valid(owner_path) &&
        owner_path->present) {
        FunctionParamFlowResultPath candidate_path = {0};
        bool current = function_param_flow_place_path_current(store->ctx,
            caller, candidate, binding, &candidate_path);
        if (!current) return true;
        FunctionParamFlowResultPath relative = {0};
        bool carries = false;
        return !function_param_flow_result_path_select(owner_path,
            &candidate_path, &relative, &carries) || carries;
    }
    char owner_fields[512], candidate_fields[512];
    bool owner_current = owner_place != NULL &&
        direct_place_path_current(store, caller, owner_place, binding,
            owner_fields, sizeof(owner_fields));
    bool candidate_current = owner_current && owner_fields[0] != '\0' &&
        direct_place_path_current(store, caller, candidate, binding,
            candidate_fields, sizeof(candidate_fields));
    return !candidate_current ||
        move_walk_path_contains(owner_fields, candidate_fields) ||
        move_walk_path_contains(candidate_fields, owner_fields);
}

static bool loan_assignment_preserves_origin(
    const FunctionParamFlowLoanSpan *span, const ASTNode *assignment)
{
    for (size_t i = 0; span != NULL && assignment != NULL &&
            i < span->preserving_assignment_count; i++)
        if (span->preserving_assignments[i] == assignment) return true;
    return false;
}

bool function_param_flow_loan_owner_unchanged(FunctionParamFlowSummaryStore *store,
    FunctionParamFlowIndexClaims *claims, ASTNode *caller, uint32_t binding,
    const ASTNode *owner_place, const FunctionParamFlowResultPath *owner_path,
    bool lends_on, bool own_descriptor,
    bool carrier, const FunctionParamFlowLoanSpan *span)
{
    bool owner_formal = false;
    for (size_t p = 0; p < ast_func_param_count(caller); p++)
        if (ast_func_param_stable_id(ast_func_param(caller, p)) == binding) owner_formal = true;
    for (size_t c = 0; c < claims->count; c++) {
        ASTNode *node = claims->rows[c].node;
        if (span != NULL && function_param_flow_claim_outside_loan(claims, c, span, ast_func_body(caller))) continue;
        /* A match case pattern binds parts of the subject and evaluates
         * nothing; its bindings carry what the subject carries, and each
         * such carrier has a proof of its own. */
        if (node->type == AST_CALL && function_param_flow_claim_in_match_pattern(claims, c)) continue;
        if (node->type == AST_ASSIGNMENT) {
            const ASTNode *target = NULL;
            const ASTNode *assigned = ast_assignment_target(node);
            if (!function_param_flow_loan_storage_root(store, claims, caller,
                    assigned, &target))
                return false;
            bool same_owner = target != NULL &&
                semantic_param_publication_binding(store->ctx, target) == binding &&
                loan_place_overlaps(store, caller, binding, owner_place,
                    owner_path, assigned);
            if (same_owner && !loan_assignment_preserves_origin(span, node))
                return false;
        }
        if (node->type != AST_CALL) continue;
        for (size_t p = 0; p < ast_call_arg_count(node); p++) {
            if (!semantic_param_publication_checked_argument(store->ctx, node, p))
                return false;
            if (semantic_param_publication_string_argument_read_only(store->ctx, node, p) ||
                semantic_typed_map_query_receiver_read(store->ctx, node, p)) continue;
            const ASTNode *root = NULL;
            if (!function_param_flow_loan_storage_root(store, claims, caller,
                    ast_call_argument(node, p), &root))
                return false;
            if (root == NULL || semantic_param_publication_binding(store->ctx, root) != binding) continue;
            ASTNode *argument_node = ast_call_argument(node, p);
            if (!loan_place_overlaps(store, caller, binding, owner_place,
                    owner_path,
                    argument_node)) continue;
            ASTNode *callee = semantic_param_publication_callee(store->ctx, node, p);
            size_t formal = 0;
            /* A String or copy-only value read through a path of the owner
             * changes nothing of it, whoever receives it. A path is never an
             * `inout` or `own` argument. */
            SemanticPublishedValue read = semantic_param_publication_value(store->ctx, argument_node);
            if ((argument_node->type == AST_MEMBER_ACCESS || argument_node->type == AST_ARRAY_ACCESS) &&
                read.ok && !read.meta && (type_equals(read.type, TYPE_STRING) ||
                    semantic_classify_ownership_type(read.type, store->ctx) == OWNERSHIP_TYPE_COPY_ONLY)) continue;
            /* A builtin that borrows its argument for the call, or a Clone of
             * it, only reads it, except the receivers the retention registry
             * lists for writers: ArrayPush, ArraySet, ArrayPushOwnedString, Write. */
            uint32_t kind = 0;
            bool clone = callee == NULL && ast_call_semantic_callee_builtin_kind(node, &kind) && kind == BUILTIN_CLONE;
            if (callee == NULL && (clone || semantic_param_publication_argument_borrowed_for_call(store->ctx, node, p))) {
                size_t stored = 0;
                if (p == 0 && (semantic_param_publication_checked_array_store(store->ctx, node, &stored) ||
                        kind == (uint32_t)BUILTIN_ARRAY_PUSH_OWNED_STRING) &&
                    !(own_descriptor && argument_node->type == AST_IDENTIFIER))
                    return false;
                if (p == 0 && semantic_param_publication_checked_slot_write(store->ctx, node))
                    return false;
                continue;
            }
            /* A constructor stores the path in the value it returns and
             * changes nothing of the owner. A change through that value is a
             * change of a local copy, refused while the loan lives
             * (default_aggregate_copy_owner), and the value stays in the
             * routine, where no callee writes or keeps it. Built from a
             * carrier, the value holds the loan, whose flow the loan frame
             * follows wherever the value goes. */
            if (callee == NULL && semantic_param_publication_nominal_result_complete(store->ctx, node) &&
                nominal_result_direct_consumer_preserves(store, caller, node,
                    p) && (carrier || function_param_flow_call_result_stays(
                        store->ctx, caller, node))) continue;
            if (callee == NULL || !semantic_param_publication_formal_for_actual(store->ctx, node, p, &formal))
                return false;
            /* A write changes the owner under the loan; so does a move into an
             * `own` formal, or into a by-value formal that keeps or releases
             * it or stores into storage it holds. */
            ParamMode mode = ast_func_param(callee, formal)->mode;
            unsigned summary = function_param_flow_summary_for_call(
                &(SlotFunctionLookup){store->ctx, store->program_root}, node, p);
            SemanticPublishedValue argument = semantic_param_publication_value(store->ctx, ast_call_argument(node, p));
            if (mode == PARAM_MODE_MUT_REF || mode == PARAM_MODE_OWN ||
                (summary & SLOT_PARAM_SUMMARY_WRITE) != 0)
                return false;
            unsigned moving = SLOT_PARAM_SUMMARY_RELEASE | SLOT_PARAM_SUMMARY_CALL_ESCAPE |
                SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE | (lends_on ? 0u : SLOT_PARAM_SUMMARY_RETURN_ESCAPE);
            bool storage = argument.ok && !type_equals(argument.type, TYPE_STRING) &&
                semantic_classify_ownership_type(argument.type, store->ctx) != OWNERSHIP_TYPE_COPY_ONLY;
            if (mode != PARAM_MODE_REF && ((summary & moving) != 0 || !argument.ok ||
                (storage && !function_param_flow_preserves_array_storage(
                    store->ctx, callee, formal))))
                return false;
            /* A read-and-return call on a local whose result leaves the
             * routine hands the local on: it changes the owner of every
             * other loan of it. */
            if (lends_on && !carrier && mode != PARAM_MODE_REF) {
                unsigned outward_effects = SLOT_PARAM_SUMMARY_NONE;
                SlotFunctionLookup lookup = {store->ctx, store->program_root};
                if (!function_param_flow_callee_outward_effects_current(
                        &lookup, node, p, &outward_effects))
                    return false;
                bool result_stays = function_param_flow_call_result_stays(
                    store->ctx, caller, node);
                if ((outward_effects & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0 &&
                    !owner_formal && !result_stays)
                    return false;
            }
        }
    }
    return true;
}
