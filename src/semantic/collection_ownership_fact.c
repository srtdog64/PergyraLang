#include "collection_ownership_fact_internal.h"
#include "indexed_string_borrow_owner.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "symbol_table.h"
#include "type_checker.h"
#include "type_checker/type_checker_internal.h"
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_defer_release_owner.h"
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "function_param_flow/function_param_publication_owner.h"
#include "../parser/ast_api.h"

bool
collection_array_string_type(const Type *type)
{
    Type *inner;
    if (!type_is_constructed_named(type, "Array"))
        return false;
    inner = type_get_constructed_arg(type, 0);
    return inner != NULL && type_equals(inner, TYPE_STRING);
}

uint32_t
current_function_syntax_id(const SemanticContext *ctx)
{
    return ctx != NULL && ctx->current_function_decl != NULL
        ? ast_node_stable_id(ctx->current_function_decl) : 0;
}

static bool
array_literal_is_empty(const ASTNode *expr)
{
    return expr != NULL && expr->type == AST_ARRAY_LITERAL
        && ast_array_literal_count(expr) == 0;
}

static bool
array_literal_is_entirely_borrowed_string_values(const ASTNode *expr)
{
    size_t count;
    if (expr == NULL || expr->type != AST_ARRAY_LITERAL)
        return false;
    count = ast_array_literal_count(expr);
    if (count == 0)
        return false;
    for (size_t i = 0; i < count; i++) {
        ASTNode *element = ast_array_literal_element(expr, i);
        /* String literals have permanent backing. An indexed String is a
         * shallow descriptor borrowed from its source Array. Neither gives
         * this new Array deep-release authority. Calls and identifiers remain
         * UNKNOWN because they may carry an owned result or mixed lineage. */
        if (element == NULL || (element->type != AST_STRING
                && element->type != AST_ARRAY_ACCESS))
            return false;
    }
    return true;
}

static bool
array_literal_is_entirely_owned_string_values(const ASTNode *expr,
                                               SemanticContext *ctx)
{
    size_t count;
    if (expr == NULL || expr->type != AST_ARRAY_LITERAL || ctx == NULL)
        return false;
    count = ast_array_literal_count(expr);
    if (count == 0)
        return false;
    for (size_t i = 0; i < count; i++) {
        ASTNode *element = ast_array_literal_element(expr, i);
        /* A literal String has permanent borrowed backing.  Only a typed
         * independently allocated String result can issue owned-literal
         * element authority for the whole Array. */
        if (element == NULL || element->type == AST_STRING
            || !semantic_indexed_string_independent_result(element, ctx))
            return false;
    }
    return true;
}

static bool
collection_ownership_fact_reserve(SemanticContext *ctx, size_t needed)
{
    size_t capacity;
    PgyCollectionOwnershipFact *grown;

    if (ctx == NULL)
        return false;
    if (needed <= ctx->collection_ownership_fact_capacity)
        return true;
    capacity = ctx->collection_ownership_fact_capacity == 0
        ? 8 : ctx->collection_ownership_fact_capacity;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*grown))
        return false;
    grown = realloc(ctx->collection_ownership_facts,
                    capacity * sizeof(*grown));
    if (grown == NULL)
        return false;
    ctx->collection_ownership_facts = grown;
    ctx->collection_ownership_fact_capacity = capacity;
    return true;
}

const PgyCollectionOwnershipFact *
semantic_collection_ownership_fact_find(const SemanticContext *ctx,
                                        uint32_t function_syntax_id,
                                        uint32_t binding_syntax_id)
{
    if (ctx == NULL || function_syntax_id == 0 || binding_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < ctx->collection_ownership_fact_count; i++) {
        const PgyCollectionOwnershipFact *fact =
            &ctx->collection_ownership_facts[i];
        if (fact->function_syntax_id == function_syntax_id
            && fact->binding_syntax_id == binding_syntax_id)
            return fact;
    }
    return NULL;
}

static PgyCollectionOwnershipFact *
collection_ownership_fact_find_mutable(SemanticContext *ctx,
                                       uint32_t function_syntax_id,
                                       uint32_t binding_syntax_id)
{
    return (PgyCollectionOwnershipFact *)
        semantic_collection_ownership_fact_find(
            ctx, function_syntax_id, binding_syntax_id);
}

static Symbol *
receiver_symbol(ASTNode *receiver, SemanticContext *ctx)
{
    if (receiver == NULL || ctx == NULL || receiver->type != AST_IDENTIFIER)
        return NULL;
    return scope_lookup(ctx->scope, ast_identifier_name(receiver));
}

uint32_t
receiver_binding_syntax_id(ASTNode *receiver, const Symbol *binding)
{
    uint32_t annotated_id = ast_identifier_binding_syntax_id(receiver);
    return annotated_id != 0 ? annotated_id
                             : (binding != NULL ? binding->decl_syntax_id : 0);
}

PgyCollectionOwnershipFact *
receiver_collection_fact(ASTNode *receiver, SemanticContext *ctx,
                         Symbol **binding_out)
{
    Symbol *binding = receiver_symbol(receiver, ctx);
    uint32_t binding_id = receiver_binding_syntax_id(receiver, binding);
    if (binding_out != NULL)
        *binding_out = binding;
    return collection_ownership_fact_find_mutable(
        ctx, current_function_syntax_id(ctx), binding_id);
}

bool
semantic_collection_ownership_initialize_binding(
    Symbol *binding,
    const ASTNode *initializer,
    const Type *binding_type,
    SemanticContext *ctx)
{
    PgyCollectionOwnershipFact fact;
    PgyCollectionOwnershipFact *source_fact = NULL;
    Symbol *source = NULL;
    bool move_source_binding = false;

    if (binding == NULL || !collection_array_string_type(binding_type))
        return true;
    memset(&fact, 0, sizeof(fact));
    fact.function_syntax_id = current_function_syntax_id(ctx);
    fact.binding_syntax_id = binding->decl_syntax_id;
    fact.origin_syntax_id = initializer != NULL
        ? ast_node_stable_id(initializer) : 0;
    fact.element_ownership = PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN;
    fact.disposition = PGY_COLLECTION_DISPOSITION_LIVE;
    fact.origin = PGY_COLLECTION_ORIGIN_UNKNOWN;

    /* Standalone semantic probes have no routine carrier.  They must not
     * manufacture a row that HIR cannot attach. */
    if (fact.function_syntax_id == 0)
        return true;
    if (fact.binding_syntax_id == 0)
        return false;

    if (array_literal_is_empty(initializer)) {
        /* The origin is exact, but the current element state remains UNKNOWN
         * until mutation/call-effect receipts own every later transition. */
        fact.origin = PGY_COLLECTION_ORIGIN_EMPTY_LITERAL;
    } else if (collection_call_is_unshadowed_builtin(initializer, "MapKeys")) {
        fact.element_ownership = PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT;
        fact.origin = PGY_COLLECTION_ORIGIN_MAP_KEYS;
    } else if (collection_call_is_unshadowed_builtin(initializer, "Clone")) {
        fact.element_ownership = PGY_STRING_ARRAY_OWNED_ELEMENTS;
        fact.origin = PGY_COLLECTION_ORIGIN_CLONE;
    } else if (array_literal_is_entirely_borrowed_string_values(
                   initializer)) {
        fact.element_ownership = PGY_STRING_ARRAY_BORROWED_ELEMENTS;
        fact.origin = PGY_COLLECTION_ORIGIN_BORROWED_LITERAL;
    } else if (array_literal_is_entirely_owned_string_values(
                   initializer, ctx)) {
        fact.element_ownership = PGY_STRING_ARRAY_OWNED_ELEMENTS;
        fact.origin = PGY_COLLECTION_ORIGIN_OWNED_LITERAL;
    } else if (initializer != NULL
               && initializer->type == AST_MEMBER_ACCESS) {
        PgyCollectionMemberPath path;
        /* Ordinary value extraction is a move, not a hidden borrow or copy.
         * The source field identity stays stable so a later access fails at
         * the source rather than forbidding valid mutation through the owner. */
        if (collection_member_move_path_identity(initializer, ctx, &path)) {
            Symbol *root = scope_lookup(ctx->scope,
                ast_identifier_name(path.root));
            fact.origin = PGY_COLLECTION_ORIGIN_MEMBER_MOVE;
            fact.source_binding_syntax_id = path.root_binding_id;
            fact.origin_syntax_id = path.field_ids[path.field_count - 1];
            fact.source_birth_serial = root != NULL ? root->birth_serial : 0;
            fact.member_move_source = initializer;
            fact.member_move_source_id = ast_node_stable_id(initializer);
            fact.member_move_elements_current = true;
        }
    } else if (initializer != NULL
               && initializer->type == AST_IDENTIFIER && ctx != NULL) {
        source = scope_lookup(ctx->scope, ast_identifier_name(initializer));
        fact.source_binding_syntax_id =
            ast_identifier_binding_syntax_id(initializer);
        if (fact.source_binding_syntax_id == 0 && source != NULL)
            fact.source_binding_syntax_id = source->decl_syntax_id;
        source_fact = collection_ownership_fact_find_mutable(
            ctx, fact.function_syntax_id, fact.source_binding_syntax_id);
        if (source_fact != NULL) {
            if (!semantic_array_storage_admit_move_before_defer_release(
                    (ASTNode *)initializer, ctx))
                return true;
            if (source_fact->disposition !=
                    PGY_COLLECTION_DISPOSITION_LIVE) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_MOVE_FROM_RELEASED,
                    PGY_CAUSE_MOVE_FROM_RELEASED,
                    PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE,
                    initializer,
                    "Array<String> binding '%s' was already moved or released",
                    source != NULL && source->name != NULL
                        ? source->name : "<source>");
                return true;
            }
            if (source_fact->origin ==
                    PGY_COLLECTION_ORIGIN_EMPTY_LITERAL ||
                source_fact->origin == PGY_COLLECTION_ORIGIN_BINDING) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                    initializer,
                    "Array<String> binding '%s' cannot move until its current transition is carried to the destination",
                    source != NULL && source->name != NULL
                        ? source->name : "<source>");
                return true;
            }
            fact.element_ownership = source_fact->element_ownership;
            fact.origin = PGY_COLLECTION_ORIGIN_BINDING;
            move_source_binding = true;
        } else {
            /* Parameter/return carriage is a separate ownership rung.  Until
             * it owns a stable source row, an unresolved identifier is an
             * explicit unknown origin rather than a dangling source edge. */
            fact.source_binding_syntax_id = 0;
        }
    }

    /* Birth classification must be reproduced on every semantic replay.
     * The first pass and a loop recheck therefore ask the same element-path
     * owner for the literal/call result premise before comparing the live
     * birth.  Reusing the stored classification here would hide a stale
     * publication; skipping the producer made a previously admitted owned
     * literal become UNKNOWN only on replay. */
    bool owned_literal = semantic_array_storage_initialize_return_elements(
        &fact, binding, initializer, ctx);
    if (owned_literal) {
        fact.element_ownership = PGY_STRING_ARRAY_OWNED_ELEMENTS;
        fact.origin = PGY_COLLECTION_ORIGIN_OWNED_LITERAL;
    }

    source_fact = collection_ownership_fact_find_mutable(
        ctx, fact.function_syntax_id, fact.binding_syntax_id);
    if (source_fact != NULL) {
        /* A loop recheck may reuse a live birth, never revive a retired one. */
        if (source_fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
            || source_fact->origin_syntax_id != fact.origin_syntax_id
            || source_fact->source_binding_syntax_id != fact.source_binding_syntax_id
            || source_fact->element_ownership != fact.element_ownership
            || source_fact->origin != fact.origin
            || source_fact->member_move_source != fact.member_move_source
            || source_fact->member_move_source_id != fact.member_move_source_id)
            return false;
        /* The rechecked birth moved the field of the source's current
         * incarnation. */
        source_fact->source_birth_serial = fact.source_birth_serial;
        source_fact->member_move_elements_current =
            fact.member_move_elements_current;
    } else {
        if (!collection_ownership_fact_reserve(
                ctx, ctx->collection_ownership_fact_count + 1))
            return false;
        ctx->collection_ownership_facts[ctx->collection_ownership_fact_count++] = fact;
    }
    if (move_source_binding) {
        source_fact = collection_ownership_fact_find_mutable(
            ctx, fact.function_syntax_id, fact.source_binding_syntax_id);
        if (source_fact == NULL || source == NULL || initializer == NULL
            || initializer->type != AST_IDENTIFIER
            || source->decl_syntax_id != fact.source_binding_syntax_id)
            return false;
        if ((semantic_array_storage_has_pending(source)
                || source_fact->return_elements_source != NULL
                || semantic_array_storage_has_pending_elements(ctx, source->decl_syntax_id)) &&
            !semantic_array_storage_record_pending_drop(ctx, (ASTNode *)initializer,
                source, ARRAY_STORAGE_RELEASE_MOVE_OWNED_STRINGS)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                (ASTNode *)initializer,
                "Array<String> move requires current preserved storage from '%s'",
                source->name != NULL ? source->name : "<array>");
            return false;
        }
        source_fact->disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
        source->is_consumed = true;
    }
    return true;
}

bool
semantic_collection_ownership_finalize_call_result_births(
    SemanticContext *ctx)
{
    if (ctx == NULL)
        return false;
    for (size_t i = 0; i < ctx->collection_ownership_fact_count; i++) {
        PgyCollectionOwnershipFact *fact =
            &ctx->collection_ownership_facts[i];
        if (fact->origin != PGY_COLLECTION_ORIGIN_UNKNOWN
            || fact->element_ownership
                != PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
            || !fact->return_elements_owned)
            continue;

        const ASTNode *call = fact->return_elements_source;
        if (call == NULL || call->type != AST_CALL
            || !ast_call_semantic_callee_declared_callable(call)
            || ast_call_semantic_callee_value_binding_id(call) != 0
            || ast_call_semantic_callee_decl_id(call) == 0)
            continue;
        uint32_t call_id = call != NULL ? ast_node_stable_id(call) : 0;
        uint32_t callee_id = call != NULL
            ? ast_call_semantic_callee_decl_id(call) : 0;
        if ((fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
                && fact->disposition
                    != PGY_COLLECTION_DISPOSITION_RETIRED)
            || fact->source_binding_syntax_id != 0 || call_id == 0
            || fact->origin_syntax_id != call_id
            || fact->return_elements_source_id != call_id)
            return false;

        const Type *result_type = NULL;
        ASTNode *callee = semantic_param_publication_declared_result(
            ctx, call, &result_type);
        if (callee == NULL || !collection_array_string_type(result_type)
            || !semantic_array_storage_fresh_owned_string_result_current(
                ctx, call)) {
            if (fact->call_result_effect_pending) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                    call,
                    "Array<String> call result cannot be released without a grounded owned-element result summary");
            }
            continue;
        }
        if (ast_node_stable_id(callee) != callee_id)
            return false;

        fact->origin = PGY_COLLECTION_ORIGIN_CALL_RESULT;
        fact->element_ownership = PGY_STRING_ARRAY_OWNED_ELEMENTS;
        fact->origin_syntax_id = callee_id;
    }
    return true;
}

bool
semantic_collection_owned_member_constructor_source(
    const ASTNode *value,
    SemanticContext *ctx)
{
    if (value == NULL || value->type != AST_IDENTIFIER || ctx == NULL
        || ctx->current_function_decl == NULL)
        return true;
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(value));
    uint32_t binding_id = ast_identifier_binding_syntax_id(value);
    uint32_t function_id = ast_node_stable_id(ctx->current_function_decl);
    PgyCollectionOwnershipFact *fact = collection_ownership_fact_find_mutable(
        ctx, function_id, binding_id);
    if (fact == NULL || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE)
        return true;
    if (binding == NULL || binding_id == 0
        || binding->decl_syntax_id != binding_id
        || binding->is_parameter || binding->is_host_field
        || binding->is_consumed || !collection_array_string_type(binding->type)
        || fact->function_syntax_id != function_id
        || fact->binding_syntax_id != binding_id
        || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
        || !fact->return_elements_owned
        || !collection_member_move_path_matches_fact(
            fact, fact->member_move_source, ctx, false))
        return false;
    return true;
}

static bool
reject_missing_collection_fact(ASTNode *receiver,
                               const Symbol *binding,
                               const char *operation,
                               SemanticContext *ctx)
{
    if (binding == NULL || !collection_array_string_type(binding->type))
        return false;
    /* Parameter provenance is owned by the separate interprocedural
     * carriage rung. Do not manufacture a local fact or fail an existing
     * inout mutation before that consumer is migrated. */
    if (binding->is_parameter)
        return false;
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE,
        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        receiver,
        "%s cannot proceed without the stable collection ownership fact for '%s'",
        operation != NULL ? operation : "Collection operation",
        binding->name != NULL ? binding->name : "<array>");
    return true;
}

bool
semantic_collection_reject_unsafe_owned_string_mutation(
    ASTNode *receiver,
    const char *operation,
    SemanticContext *ctx)
{
    Symbol *binding = NULL;
    PgyCollectionOwnershipFact *fact =
        receiver_collection_fact(receiver, ctx, &binding);
    if (fact == NULL)
        return reject_missing_collection_fact(
            receiver, binding, operation, ctx);
    if (fact->element_ownership != PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
        && fact->element_ownership != PGY_STRING_ARRAY_OWNED_ELEMENTS)
        return false;
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE,
        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        receiver,
        "%s cannot apply shallow String-element mutation to owned snapshot '%s'.\n"
        "Reason:\n"
        "- this Array<String> owns every stored string pointer\n"
        "- the ordinary mutation does not transfer or retire element ownership\n"
        "Fix:\n"
        "- use read-only operations such as ArraySort or ArrayReverse\n"
        "- or use an ownership-aware operation when one is available",
        operation != NULL ? operation : "Array mutation",
        binding != NULL && binding->name != NULL
            ? binding->name : "<array>");
    return true;
}

bool
semantic_collection_admit_owned_string_drop(
    ASTNode *receiver,
    SemanticContext *ctx)
{
    Symbol *binding = NULL;
    PgyCollectionOwnershipFact *fact =
        receiver_collection_fact(receiver, ctx, &binding);
    const char *name = binding != NULL && binding->name != NULL
        ? binding->name : "<array>";
    bool current_inout_member_move = fact != NULL
        && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
        && binding != NULL && !binding->is_consumed
        && collection_member_move_from_current_inout_parameter(
            fact, binding, ctx)
        && collection_member_move_release_path_current(
            fact, binding, receiver, ctx);

    if (receiver == NULL)
        return false;
    if (receiver->type != AST_IDENTIFIER) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver,
            "ArrayDropOwnedStrings requires a named Array<String> owner.\n"
            "Reason:\n"
            "- a field or temporary expression carries no stable element-lifetime fact\n"
            "Fix:\n"
            "- move the value into one named binding with proved provenance first");
        return false;
    }

    /* Element ownership alone cannot retire storage retained by a field or
     * container. Unknown storage production is a separate bootstrap boundary. */
    if (binding != NULL && binding->has_escaped_array_storage
        && !current_inout_member_move) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver, "ArrayDropOwnedStrings cannot release escaped array storage in '%s'",
            name);
        return false;
    }
    if ((semantic_array_storage_has_pending(binding)
            || (fact != NULL && fact->return_elements_source != NULL)
            || (binding != NULL && semantic_array_storage_has_pending_elements(ctx,
                binding->decl_syntax_id))) &&
        (ast_identifier_binding_syntax_id(receiver) != binding->decl_syntax_id ||
         !semantic_array_storage_record_pending_drop(ctx, receiver, binding,
             ARRAY_STORAGE_RELEASE_DROP_OWNED_STRINGS))) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver, "ArrayDropOwnedStrings requires current preserved array storage in '%s'",
            name);
        return false;
    }
    if (fact == NULL) {
        if (binding != NULL && binding->is_parameter
            && collection_array_string_type(binding->type)
            && binding->param_mode != PARAM_MODE_OWN) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                receiver,
                "ArrayDropOwnedStrings cannot release parameter '%s' without an own boundary.\n"
                "Reason:\n"
                "- default, ref, and inout parameters do not transfer string-element lifetime\n"
                "- a deep drop could free borrowed or caller-owned elements\n"
                "Fix:\n"
                "- spell the parameter as 'own %s: Array<String>' when the callee consumes it\n"
                "- or leave deep release with the caller's ownership fact",
                name, name);
            return false;
        }
        if (reject_missing_collection_fact(
                receiver, binding, "ArrayDropOwnedStrings", ctx))
            return false;
        return true;
    }
    if (fact->element_ownership ==
            PGY_STRING_ARRAY_BORROWED_ELEMENTS) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver,
            "ArrayDropOwnedStrings cannot release borrowed string literals in '%s'.\n"
            "Reason:\n"
            "- the array descriptor owns only its backing storage\n"
            "- its string elements were not allocated for this binding\n"
            "Fix:\n"
            "- use an owned-string producer such as MapKeys<String>\n"
            "- or omit the deep drop for borrowed elements",
            name);
        return false;
    }
    if (fact->element_ownership == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN) {
        if (fact->origin == PGY_COLLECTION_ORIGIN_EMPTY_LITERAL
            && !fact->return_elements_owned) {
            if (binding != NULL
                && semantic_array_storage_has_pending_owned_string_result(
                    ctx, binding->decl_syntax_id))
                return true;
            if (!ctx->has_error)
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, receiver,
                    "ArrayDropOwnedStrings cannot release '%s': shallow mutation withdrew its owned String-element premise",
                    name);
            return false;
        }
        if (fact->origin == PGY_COLLECTION_ORIGIN_UNKNOWN
            && fact->return_elements_source != NULL
            && !fact->return_elements_owned) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                receiver,
                "ArrayDropOwnedStrings cannot release '%s': shallow mutation withdrew its owned String-element premise",
                name);
            return false;
        }
        if (fact->origin == PGY_COLLECTION_ORIGIN_MEMBER_MOVE) {
            /* UNKNOWN is not promoted to OWNED.  The native bootstrap may
             * execute only an exact current-inout member move whose direct
             * body path deep-drops the carrier and restores that same empty
             * descriptor.  The self-host aggregate analyzer still owns
             * element provenance and call-site admission. */
            if (current_inout_member_move) {
                fact->disposition =
                    PGY_COLLECTION_DISPOSITION_RELEASED_MEMBER_MOVE;
                return true;
            }
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                receiver,
                "ArrayDropOwnedStrings cannot deep-release moved aggregate field '%s' without element ownership proof.\n"
                "Reason:\n"
                "- field extraction transfers descriptor/storage ownership only\n"
                "- its String elements may still be borrowed\n"
                "Fix:\n"
                "- let ordinary storage cleanup retire the moved local\n"
                "- or Clone the field into an owned snapshot before deep release",
                name);
            return false;
        }
        if (fact->origin == PGY_COLLECTION_ORIGIN_BINDING) {
            if (fact->return_elements_owned && fact->return_elements_source != NULL
                && binding != NULL && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE)
                return true; /* The recorded deep-drop obligation checks the exact dependency. */
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                receiver,
                "ArrayDropOwnedStrings cannot deep-release shallow alias '%s' without element ownership proof.\n"
                "Reason:\n"
                "- the alias carries descriptor identity but no independent String-element lifetime\n"
                "- releasing through it could double-free or invalidate the source binding\n"
                "Fix:\n"
                "- keep deep release with the proved source owner\n"
                "- or Clone into an independent owned snapshot before release",
                name);
            return false;
        }
        /* This native bootstrap-oracle boundary still admits general UNKNOWN
         * provenance. Admission here is not owned-element proof and must not
         * be projected as an OWNED carrier row from type or spelling. */
        return true;
    }
    if (fact->disposition == PGY_COLLECTION_DISPOSITION_RETIRED) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver,
            "ArrayDropOwnedStrings cannot release retired binding '%s' twice",
            name);
        return false;
    }
    if (fact->element_ownership != PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
        && fact->element_ownership != PGY_STRING_ARRAY_OWNED_ELEMENTS)
        return false;
    if (current_inout_member_move) {
        fact->disposition =
            PGY_COLLECTION_DISPOSITION_RELEASED_MEMBER_MOVE;
        return true;
    }
    fact->disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
    return true;
}

bool
semantic_collection_reject_unsafe_string_array_assignment(
    ASTNode *target,
    ASTNode *value,
    const Type *target_type,
    const Type *value_type,
    SemanticContext *ctx)
{
    Symbol *target_binding;
    Symbol *source_binding;
    const PgyCollectionOwnershipFact *source_fact;
    uint32_t target_id;
    uint32_t source_id;

    if (target == NULL || value == NULL || ctx == NULL
        || target->type != AST_IDENTIFIER || value->type != AST_IDENTIFIER
        || !collection_array_string_type(target_type) || !collection_array_string_type(value_type)) {
        return false;
    }
    target_binding = scope_lookup(ctx->scope, ast_identifier_name(target));
    source_binding = scope_lookup(ctx->scope, ast_identifier_name(value));
    target_id = receiver_binding_syntax_id(target, target_binding);
    source_id = receiver_binding_syntax_id(value, source_binding);
    if (target_id == 0 || source_id == 0 || target_id == source_id)
        return false;
    source_fact = semantic_collection_ownership_fact_find(
        ctx, current_function_syntax_id(ctx), source_id);
    if (source_fact == NULL) {
        return false;
    }
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE,
        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        value,
        "Tracked Array<String> binding '%s' cannot be shallow-assigned into '%s'.\n"
        "Reason:\n"
        "- both descriptors would share one element-storage lifetime\n"
        "- a later ownership-producing mutation or release could leave an alias dangling\n"
        "Fix:\n"
        "- keep one binding as the owner\n"
        "- or materialize a separately owned snapshot",
        source_binding != NULL && source_binding->name != NULL
            ? source_binding->name : "<source>",
        target_binding != NULL && target_binding->name != NULL
            ? target_binding->name : "<target>");
    return true;
}

void
pgy_collection_ownership_facts_destroy(PgyCollectionOwnershipFact *facts,
                                       size_t count)
{
    (void)count;
    free(facts);
}
