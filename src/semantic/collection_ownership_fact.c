#include "collection_ownership_fact.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "symbol_table.h"
#include "type_checker.h"
#include "type_checker_internal.h"
#include "../parser/ast_api.h"

static bool
is_array_string(const Type *type)
{
    Type *inner;
    if (!type_is_constructed_named(type, "Array"))
        return false;
    inner = type_get_constructed_arg(type, 0);
    return inner != NULL && type_equals(inner, TYPE_STRING);
}

static uint32_t
current_function_syntax_id(const SemanticContext *ctx)
{
    return ctx != NULL && ctx->current_function_decl != NULL
        ? ast_node_stable_id(ctx->current_function_decl) : 0;
}

static bool
collection_call_is_unshadowed_builtin(const ASTNode *expr,
                                      const char *expected_name)
{
    ASTNode *callee;
    const char *name;

    if (expr == NULL || expr->type != AST_CALL || expected_name == NULL)
        return false;
    callee = ast_call_callee(expr);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return false;
    name = ast_identifier_name(callee);
    return name != NULL && strcmp(name, expected_name) == 0
        && ast_identifier_binding_syntax_id(callee) == 0;
}

static bool
array_literal_is_entirely_borrowed_string_literals(const ASTNode *expr)
{
    size_t count;
    if (expr == NULL || expr->type != AST_ARRAY_LITERAL)
        return false;
    count = ast_array_literal_count(expr);
    if (count == 0)
        return false;
    for (size_t i = 0; i < count; i++) {
        ASTNode *element = ast_array_literal_element(expr, i);
        if (element == NULL || element->type != AST_STRING)
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

static bool
collection_ownership_fact_append(SemanticContext *ctx,
                                 const PgyCollectionOwnershipFact *fact)
{
    if (ctx == NULL || fact == NULL || fact->function_syntax_id == 0
        || fact->binding_syntax_id == 0
        || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
        || semantic_collection_ownership_fact_find(
            ctx, fact->function_syntax_id,
            fact->binding_syntax_id) != NULL)
        return false;
    if (!collection_ownership_fact_reserve(
            ctx, ctx->collection_ownership_fact_count + 1))
        return false;
    ctx->collection_ownership_facts[
        ctx->collection_ownership_fact_count++] = *fact;
    return true;
}

static Symbol *
receiver_symbol(ASTNode *receiver, SemanticContext *ctx)
{
    if (receiver == NULL || ctx == NULL || receiver->type != AST_IDENTIFIER)
        return NULL;
    return scope_lookup(ctx->scope, ast_identifier_name(receiver));
}

static uint32_t
receiver_binding_syntax_id(ASTNode *receiver, const Symbol *binding)
{
    uint32_t annotated_id = ast_identifier_binding_syntax_id(receiver);
    return annotated_id != 0 ? annotated_id
                             : (binding != NULL ? binding->decl_syntax_id : 0);
}

static PgyCollectionOwnershipFact *
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
    const PgyCollectionOwnershipFact *source_fact = NULL;
    Symbol *source = NULL;
    bool reject_shallow_owned_copy = false;

    if (binding == NULL || !is_array_string(binding_type))
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

    if (collection_call_is_unshadowed_builtin(initializer, "MapKeys")) {
        fact.element_ownership = PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT;
        fact.origin = PGY_COLLECTION_ORIGIN_MAP_KEYS;
    } else if (array_literal_is_entirely_borrowed_string_literals(
                   initializer)) {
        fact.element_ownership = PGY_STRING_ARRAY_BORROWED_ELEMENTS;
        fact.origin = PGY_COLLECTION_ORIGIN_BORROWED_LITERAL;
    } else if (initializer != NULL
               && initializer->type == AST_IDENTIFIER && ctx != NULL) {
        source = scope_lookup(ctx->scope, ast_identifier_name(initializer));
        fact.source_binding_syntax_id =
            ast_identifier_binding_syntax_id(initializer);
        if (fact.source_binding_syntax_id == 0 && source != NULL)
            fact.source_binding_syntax_id = source->decl_syntax_id;
        source_fact = semantic_collection_ownership_fact_find(
            ctx, fact.function_syntax_id, fact.source_binding_syntax_id);
        if (source_fact != NULL) {
            fact.element_ownership = source_fact->element_ownership;
            fact.origin = PGY_COLLECTION_ORIGIN_BINDING;
            reject_shallow_owned_copy =
                source_fact->disposition == PGY_COLLECTION_DISPOSITION_RETIRED
                || source_fact->element_ownership ==
                    PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
                || source_fact->element_ownership ==
                    PGY_STRING_ARRAY_OWNED_ELEMENTS;
        } else {
            /* Parameter/return carriage is a separate ownership rung.  Until
             * it owns a stable source row, an unresolved identifier is an
             * explicit unknown origin rather than a dangling source edge. */
            fact.source_binding_syntax_id = 0;
        }
    }

    if (!collection_ownership_fact_append(ctx, &fact))
        return false;
    if (!reject_shallow_owned_copy)
        return true;

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE,
        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        initializer,
        "Owned Array<String> binding '%s' cannot be shallow-copied into '%s'.\n"
        "Reason:\n"
        "- both descriptors would point at the same owned string elements\n"
        "- releasing either binding would leave the other dangling or cause a double free\n"
        "Fix:\n"
        "- keep one binding as the owner\n"
        "- or materialize a separately owned snapshot",
        source != NULL && source->name != NULL ? source->name : "<source>",
        binding->name != NULL ? binding->name : "<binding>");
    return true;
}

static bool
reject_missing_collection_fact(ASTNode *receiver,
                               const Symbol *binding,
                               const char *operation,
                               SemanticContext *ctx)
{
    if (binding == NULL || !is_array_string(binding->type))
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

    if (fact == NULL) {
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
        /* General String producer/transfer receipts are the next ownership
         * rung.  Until they exist, retain the legacy admission instead of
         * forging an owned carrier row from type or spelling. */
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
    fact->disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
    return true;
}

void
pgy_collection_ownership_facts_destroy(PgyCollectionOwnershipFact *facts,
                                       size_t count)
{
    (void)count;
    free(facts);
}
