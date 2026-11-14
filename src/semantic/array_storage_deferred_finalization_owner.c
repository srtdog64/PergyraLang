/* Final settlement of deferred Array preservation receipts. */
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_deferred_preservation_internal.h"
#include "type_checker/type_checker_internal.h"
#include "slot_analyzer_internal.h"
#include "slot_summary.h"
#include "diag_codes.h"
#include "function_param_flow/function_param_publication_owner.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "collection_ownership_fact.h"
#include "collection_owned_element_requirement_owner.h"
#include "formal_string_store_landing_owner.h"
#include "loan_copy_landing_owner.h"
#include "builtin_kind.h"

#include <stdlib.h>
#include <string.h>

/* A binding move hands on whatever its source's elements are. When neither
 * the source's fact nor any result or String push into it can make them
 * owned (a literal or unknown call result), the destination never deep-drops
 * them, so the move needs a current descriptor only, not an owned-element
 * proof. */
static bool
array_storage_move_source_elements_unowned(SemanticContext *ctx,
    const ArrayStorageDeferredPreservationStore *store, uint32_t binding_id)
{
    size_t facts = 0;
    for (size_t i = 0; i < ctx->collection_ownership_fact_count; i++) {
        const PgyCollectionOwnershipFact *fact = &ctx->collection_ownership_facts[i];
        if (fact->binding_syntax_id != binding_id)
            continue;
        if (fact->element_ownership == PGY_STRING_ARRAY_OWNED_ELEMENTS
            || fact->element_ownership == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT)
            return false;
        facts++;
    }
    if (facts != 1)
        return false;
    for (size_t i = 0; i < store->result_count; i++)
        if (store->results[i].birth.binding_id == binding_id
            && semantic_array_storage_owned_string_result_current(ctx,
                store->results[i].birth.call))
            return false;
    for (size_t i = 0; i < store->owned_string_result_count; i++)
        if (store->owned_string_results[i].binding_id == binding_id)
            return false;
    return true;
}

bool
semantic_array_storage_deferral_finalize(SemanticContext *ctx)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (store == NULL || !store->active || !semantic_param_publication_ready(ctx))
        return false;
    store->active = false;
    /* The final owner supplies one all-typed store to every consumer. */
    for (size_t i = 0; i < store->call_count; i++) {
        ArrayStoragePendingCall *pending = &store->calls[i];
        unsigned failures = semantic_array_storage_pending_call_failures(ctx, &pending->source);
        pending->preserved = (failures & FUNCTION_PARAM_ARRAY_DESCRIPTOR_UNPROVED) == 0;
        pending->elements_preserved = (failures & FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED) == 0;
    }
    ASTNode *first_owned_string_copy = NULL;
    for (size_t i = 0; i < store->owned_string_result_count; i++) {
        ArrayStoragePendingOwnedStringResult *result =
            &store->owned_string_results[i];
        if (array_storage_pending_owned_string_result_current(ctx, result, true))
            continue;
        if (!semantic_collection_owned_string_result_cycle_withdrawn_current(
                result->result_call, ctx)) {
            if (!semantic_formal_string_store_landing_select_deferred_result_copy(
                    ctx, result->push_call)) {
                if (!ctx->has_error)
                    semantic_error(ctx, result->push_call,
                        "Deferred owned String result copy landing selection failed");
                return false;
            }
            if (first_owned_string_copy == NULL)
                first_owned_string_copy = result->result_call;
            continue;
        }
        if (!ctx->has_error)
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                result->push_call,
                "Deferred owned String result consumer lost its exact producer or parameter ownership proof");
        return false;
    }
    if (first_owned_string_copy != NULL) {
        semantic_loan_copy_landing_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, first_owned_string_copy,
            "A deferred String result without a final owned-source proof is copied where it is stored");
        return false;
    }
    store->finalized = true;
    store->sealing = true;
    for (size_t i = 0; i < store->result_count; i++) {
        ArrayStoragePendingResult *result = &store->results[i];
        if (!array_storage_pending_result_proof(ctx, result))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE,
                result->birth.call, "Array call result storage has no current ownership transfer proof");
    }
    for (size_t i = 0; i < store->call_count; i++) {
        const ArrayStoragePendingCallReceipt *source = &store->calls[i].source;
        if (source->payload || source->callee_decl == NULL
            || source->formal_id == 0
            || !semantic_array_storage_default_formal_result_current(
                ctx, source->callee_decl, source->formal_id))
            continue;
        if (!semantic_array_storage_default_formal_call_result_consumed_current(
                ctx, source))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                source->call,
                "Owned Array transfer result is discarded; bind, return, or explicitly consume the result");
    }
    store->sealing = false;
    /* Every call was checked above in this same typed generation. Recording
     * is disabled, and this synchronous loop cannot mutate those inputs.
     * Reuse its verdicts only here; later public queries must replay currency. */
    store->consuming_checked_calls = true;
    for (size_t i = 0; i < store->drop_count; i++) {
        const ArrayStoragePendingDrop *drop = &store->drops[i];
        if (drop->operation == ARRAY_STORAGE_CALL_AFTER_PENDING &&
            drop->receiver != NULL && drop->receiver->type == AST_CALL &&
            drop->call_index < store->call_count) {
            const ArrayStoragePendingCallReceipt *source = &store->calls[drop->call_index].source;
            if (semantic_array_storage_pending_call_consumer_current(ctx, source,
                    drop->receiver, drop->receiver_id, drop->binding_id, drop->sequence))
                continue;
        }
        bool current_identifier = drop->receiver != NULL
            && drop->receiver->type == AST_IDENTIFIER
            && ast_node_stable_id(drop->receiver) == drop->receiver_id
            && ast_identifier_binding_syntax_id(drop->receiver)
                == drop->binding_id;
        bool descriptor_preserved = current_identifier
            && semantic_array_storage_pending_preserved(
                ctx, drop->binding_id, drop->sequence);
        bool element_use_current = descriptor_preserved
            && (drop->operation != ARRAY_STORAGE_RELEASE_DROP_OWNED_STRINGS
                || array_storage_pending_drop_element_source_current(ctx, store, drop));
        bool pending_elements_preserved = element_use_current
            && semantic_array_storage_pending_owned_elements(
                ctx, drop->binding_id, drop->sequence);
        bool elements_preserved = descriptor_preserved
            && ((drop->operation != ARRAY_STORAGE_RELEASE_DROP_OWNED_STRINGS
                    && drop->operation != ARRAY_STORAGE_RELEASE_MOVE_OWNED_STRINGS
                    && drop->operation != ARRAY_STORAGE_MUTATE_OWNED_STRINGS)
                || pending_elements_preserved
                || (drop->operation == ARRAY_STORAGE_RELEASE_MOVE_OWNED_STRINGS
                    && array_storage_move_source_elements_unowned(ctx, store,
                        drop->binding_id)));
        if (elements_preserved)
            continue;
        /* A String element handed to a callee that may keep it, with no proof
         * that the loan stays, is copied where it is passed, as any String
         * argument no proof admits is (2026-10-21_string_argument_no_proof_admits_is_copied). */
        if (drop->operation == ARRAY_STORAGE_CALL_AFTER_PENDING && drop->call_index < store->call_count &&
            store->calls[drop->call_index].source.payload &&
            !semantic_loan_copy_landing_select_carrier(ctx, store->calls[drop->call_index].source.argument))
            semantic_error(ctx, drop->receiver, "String element copy landing selection failed");
        if (drop->operation == ARRAY_STORAGE_CALL_AFTER_PENDING && drop->call_index < store->call_count &&
            !store->calls[drop->call_index].source.payload && !array_storage_same_call_arguments_copied(ctx, store, drop))
            semantic_error(ctx, drop->receiver, "Array argument copy landing selection failed");
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
            drop->receiver, "%s requires preserved array storage; a prior call may retain or rebind the descriptor",
            drop->operation == ARRAY_STORAGE_RELEASE_DROP_OWNED_STRINGS
                ? "ArrayDropOwnedStrings"
                : drop->operation == ARRAY_STORAGE_RELEASE_MOVE_OWNED_STRINGS
                    ? "Array<String> move"
                    : drop->operation == ARRAY_STORAGE_MUTATE_STORAGE
                        ? "Array mutation"
                    : drop->operation == ARRAY_STORAGE_MUTATE_OWNED_STRINGS
                        ? "ArrayPushOwnedString"
                        : drop->operation == ARRAY_STORAGE_CALL_AFTER_PENDING
                            ? "Array argument call" : "ArrayDrop");
    }
    store->consuming_checked_calls = false;
    return true;
}

