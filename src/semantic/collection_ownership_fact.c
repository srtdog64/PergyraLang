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

static ASTNode *
collection_direct_let_for_binding(ASTNode *body, uint32_t binding_syntax_id)
{
    if (body == NULL || body->type != AST_BLOCK || binding_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < ast_block_statement_count(body); i++) {
        ASTNode *stmt = ast_block_statement(body, i);
        if (stmt != NULL && stmt->type == AST_LET_DECL
            && ast_node_stable_id(stmt) == binding_syntax_id)
            return stmt;
    }
    return NULL;
}

static bool
collection_direct_result_allocator_binding(ASTNode *body,
                                           uint32_t binding_syntax_id)
{
    ASTNode *decl = collection_direct_let_for_binding(
        body, binding_syntax_id);
    return decl != NULL && !ast_let_is_mutable(decl)
        && collection_call_is_unshadowed_builtin(
            ast_let_initializer(decl), "AllocatorResult");
}

static bool
collection_callable_type_returns_owned_string(const ASTNode *call,
                                              SemanticContext *ctx,
                                              uint32_t *producer_id_out)
{
    ASTNode *callee;
    ASTNode *decl;
    Symbol *symbol;
    const char *name;
    uint32_t producer_id;

    if (producer_id_out != NULL)
        *producer_id_out = 0;
    if (call == NULL || call->type != AST_CALL || ctx == NULL)
        return false;
    producer_id = ast_call_semantic_callee_decl_id(call);
    callee = ast_call_callee(call);
    name = callee != NULL && callee->type == AST_IDENTIFIER
        ? ast_identifier_name(callee) : NULL;
    if (producer_id == 0 || name == NULL)
        return false;
    decl = semantic_find_callable_decl_by_name(ctx, name);
    symbol = scope_lookup(ctx->scope, name);
    if (decl == NULL || decl->type != AST_FUNC_DECL
        || ast_node_stable_id(decl) != producer_id
        || symbol == NULL || symbol->kind != SYMBOL_FUNCTION
        || symbol->decl_syntax_id != producer_id
        || symbol->type == NULL || symbol->type->kind != TYPE_KIND_FUNCTION
        || !type_function_has_body_summary(symbol->type)
        || !type_equals(type_function_return_type(symbol->type), TYPE_STRING)
        || (type_function_body_summary(symbol->type)
            & BODY_SUMMARY_RETURNS_OWNED_STRING) == 0) {
        return false;
    }
    if (producer_id_out != NULL)
        *producer_id_out = producer_id;
    return true;
}

static bool
collection_owned_string_expression_ready(ASTNode *body,
                                         const ASTNode *expression,
                                         SemanticContext *ctx,
                                         unsigned depth)
{
    ASTNode *decl;
    ASTNode *allocator;
    uint32_t binding_id;

    if (body == NULL || expression == NULL || depth > 8)
        return false;
    if (expression->type == AST_IDENTIFIER) {
        binding_id = ast_identifier_binding_syntax_id(expression);
        decl = collection_direct_let_for_binding(body, binding_id);
        return decl != NULL && !ast_let_is_mutable(decl)
            && collection_owned_string_expression_ready(
                body, ast_let_initializer(decl), ctx, depth + 1);
    }
    if (expression->type != AST_CALL)
        return false;
    if (collection_call_is_unshadowed_builtin(expression, "Concat"))
        return ast_call_arg_count(expression) == 2;
    if (collection_call_is_unshadowed_builtin(
            expression, "TextBuilderFinish")) {
        if (ast_call_arg_count(expression) != 2)
            return false;
        allocator = ast_call_argument(expression, 1);
        return allocator != NULL && allocator->type == AST_IDENTIFIER
            && collection_direct_result_allocator_binding(
                body, ast_identifier_binding_syntax_id(allocator));
    }
    return collection_callable_type_returns_owned_string(
        expression, ctx, NULL);
}

bool
semantic_collection_owned_string_call_result(
    const ASTNode *expression,
    SemanticContext *ctx,
    uint32_t *producer_syntax_id_out)
{
    return collection_callable_type_returns_owned_string(
        expression, ctx, producer_syntax_id_out);
}

void
semantic_collection_record_owned_string_result_summary(
    ASTNode *function_decl,
    SemanticContext *ctx)
{
    ASTNode *body;
    bool saw_return = false;

    if (function_decl == NULL || function_decl->type != AST_FUNC_DECL
        || ctx == NULL || !ctx->tracking_function_effects
        || !type_equals(ctx->current_return, TYPE_STRING)) {
        return;
    }
    body = ast_func_body(function_decl);
    if (body == NULL || body->type != AST_BLOCK)
        return;
    for (size_t i = 0; i < ast_block_statement_count(body); i++) {
        ASTNode *stmt = ast_block_statement(body, i);
        if (stmt == NULL)
            return;
        if (stmt->type == AST_RETURN) {
            ASTNode *value = ast_return_value(stmt);
            saw_return = true;
            if (!collection_owned_string_expression_ready(
                    body, value, ctx, 0)) {
                return;
            }
            continue;
        }
        /* A nested control owner could carry an additional return.  This
         * bounded summary refuses it instead of guessing that the direct
         * top-level returns are exhaustive. */
        if (stmt->type != AST_LET_DECL && stmt->type != AST_CALL
            && stmt->type != AST_ASSIGNMENT && stmt->type != AST_DEFER_STMT) {
            return;
        }
    }
    if (saw_return) {
        semantic_record_body_summary(
            ctx, BODY_SUMMARY_RETURNS_OWNED_STRING);
    }
}

static bool
array_literal_is_empty(const ASTNode *expr)
{
    return expr != NULL && expr->type == AST_ARRAY_LITERAL
        && ast_array_literal_count(expr) == 0;
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

static bool
collection_member_move_identity(const ASTNode *member_access,
                                SemanticContext *ctx,
                                uint32_t *root_binding_id_out,
                                uint32_t *field_syntax_id_out)
{
    ASTNode *root;
    Symbol *binding;
    ASTNode *decl;
    const char *field_name;
    uint32_t root_binding_id;

    if (root_binding_id_out != NULL)
        *root_binding_id_out = 0;
    if (field_syntax_id_out != NULL)
        *field_syntax_id_out = 0;
    if (member_access == NULL || member_access->type != AST_MEMBER_ACCESS
        || ctx == NULL)
        return false;
    root = ast_member_object(member_access);
    if (root == NULL || root->type != AST_IDENTIFIER)
        return false;
    binding = scope_lookup(ctx->scope, ast_identifier_name(root));
    root_binding_id = receiver_binding_syntax_id(root, binding);
    field_name = ast_member_name(member_access);
    decl = binding != NULL
        ? semantic_host_decl_for_type(ctx, binding->type) : NULL;
    if (root_binding_id == 0 || field_name == NULL || decl == NULL
        || decl->type != AST_CLASS_DECL)
        return false;
    for (size_t i = 0; i < projection_source_field_count(decl); i++) {
        PgyDeclField field = projection_source_field_at(decl, i);
        if (field.name != NULL && strcmp(field.name, field_name) == 0
            && field.declaration_syntax_id != 0) {
            if (root_binding_id_out != NULL)
                *root_binding_id_out = root_binding_id;
            if (field_syntax_id_out != NULL)
                *field_syntax_id_out = field.declaration_syntax_id;
            return true;
        }
    }
    return false;
}

static bool
collection_binding_is_current_parameter(const SemanticContext *ctx,
                                        uint32_t binding_syntax_id)
{
    ASTNode *function_decl;

    if (ctx == NULL || binding_syntax_id == 0)
        return false;
    function_decl = ctx->current_function_decl;
    if (function_decl == NULL || function_decl->type != AST_FUNC_DECL)
        return false;
    for (size_t i = 0; i < ast_func_param_count(function_decl); i++) {
        FuncParam *param = ast_func_param(function_decl, i);
        if (param != NULL
            && ast_func_param_stable_id(param) == binding_syntax_id)
            return true;
    }
    return false;
}

bool
semantic_collection_reject_moved_member_use(ASTNode *member_access,
                                             SemanticContext *ctx)
{
    uint32_t root_binding_id;
    uint32_t field_syntax_id;
    const char *field_name;

    if (!collection_member_move_identity(member_access, ctx,
            &root_binding_id, &field_syntax_id))
        return false;
    for (size_t i = 0; i < ctx->collection_ownership_fact_count; i++) {
        const PgyCollectionOwnershipFact *fact =
            &ctx->collection_ownership_facts[i];
        if (fact->function_syntax_id != current_function_syntax_id(ctx)
            || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
            || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
            || fact->source_binding_syntax_id != root_binding_id
            || fact->origin_syntax_id != field_syntax_id)
            continue;
        field_name = ast_member_name(member_access);
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_MOVE_FROM_RELEASED,
            PGY_CAUSE_MOVE_FROM_RELEASED,
            PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE,
            member_access,
            "Field '%s' was moved into collection binding syntax %u and cannot be used again.\n"
            "Reason:\n"
            "- field extraction transfers the collection descriptor and its storage lifetime\n"
            "- using the source field again would recreate a shallow alias\n"
            "Fix:\n"
            "- use Clone(source.%s) when both values must remain live\n"
            "- or keep using only the moved local binding",
            field_name != NULL ? field_name : "<field>",
            fact->binding_syntax_id,
            field_name != NULL ? field_name : "<field>");
        return true;
    }
    return false;
}

bool
semantic_collection_admit_owned_string_push(ASTNode *receiver,
                                             SemanticContext *ctx)
{
    Symbol *binding = NULL;
    PgyCollectionOwnershipFact *fact =
        receiver_collection_fact(receiver, ctx, &binding);
    const char *name = binding != NULL && binding->name != NULL
        ? binding->name : "<array>";

    if (fact == NULL)
        return true;
    if (fact->element_ownership != PGY_STRING_ARRAY_BORROWED_ELEMENTS
        && !(fact->element_ownership == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
             && fact->origin == PGY_COLLECTION_ORIGIN_MEMBER_MOVE))
        return true;
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE,
        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        receiver,
        "ArrayPushOwnedString cannot mix an owned String into '%s' without uniform element ownership.\n"
        "Reason:\n"
        "- the existing elements are borrowed or have no deep-release proof\n"
        "- the resulting array would have no sound whole-array cleanup policy\n"
        "Fix:\n"
        "- Clone the collection into an owned snapshot before the owned push\n"
        "- or use ArrayPush for a borrowed String element",
        name);
    return false;
}

bool
semantic_collection_restore_moved_member(ASTNode *target,
                                         ASTNode *value,
                                         SemanticContext *ctx)
{
    uint32_t root_binding_id;
    uint32_t field_syntax_id;
    uint32_t value_binding_id;
    Symbol *value_binding;

    if (value == NULL || value->type != AST_IDENTIFIER || ctx == NULL
        || !collection_member_move_identity(target, ctx,
            &root_binding_id, &field_syntax_id))
        return false;
    value_binding = scope_lookup(ctx->scope, ast_identifier_name(value));
    value_binding_id = receiver_binding_syntax_id(value, value_binding);
    if (value_binding == NULL || value_binding_id == 0)
        return false;

    for (size_t i = 0; i < ctx->collection_ownership_fact_count; i++) {
        PgyCollectionOwnershipFact *fact =
            &ctx->collection_ownership_facts[i];
        if (fact->function_syntax_id != current_function_syntax_id(ctx)
            || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
            || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
            || fact->binding_syntax_id != value_binding_id
            || fact->source_binding_syntax_id != root_binding_id
            || fact->origin_syntax_id != field_syntax_id)
            continue;
        /* Exact move-back restores the field and consumes the temporary.
         * No descriptor is copied and no second owner remains live. */
        fact->disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
        value_binding->is_consumed = true;
        return true;
    }
    return false;
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
    } else if (array_literal_is_entirely_borrowed_string_literals(
                   initializer)) {
        fact.element_ownership = PGY_STRING_ARRAY_BORROWED_ELEMENTS;
        fact.origin = PGY_COLLECTION_ORIGIN_BORROWED_LITERAL;
    } else if (initializer != NULL
               && initializer->type == AST_MEMBER_ACCESS) {
        uint32_t root_binding_id = 0;
        uint32_t field_syntax_id = 0;
        /* Ordinary value extraction is a move, not a hidden borrow or copy.
         * The source field identity stays stable so a later access fails at
         * the source rather than forbidding valid mutation through the owner. */
        if (collection_member_move_identity(initializer, ctx,
                &root_binding_id, &field_syntax_id)) {
            fact.origin = PGY_COLLECTION_ORIGIN_MEMBER_MOVE;
            fact.source_binding_syntax_id = root_binding_id;
            fact.origin_syntax_id = field_syntax_id;
        }
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
                || source_fact->origin ==
                    PGY_COLLECTION_ORIGIN_EMPTY_LITERAL
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
        "Tracked Array<String> binding '%s' cannot be shallow-copied into '%s'.\n"
        "Reason:\n"
        "- both descriptors would share one element-storage lifetime\n"
        "- a later ownership-producing mutation or release could leave an alias dangling\n"
        "Fix:\n"
        "- keep one binding as the owner\n"
        "- or materialize a separately owned snapshot",
        source != NULL && source->name != NULL ? source->name : "<source>",
        binding->name != NULL ? binding->name : "<binding>");
    return true;
}

bool
semantic_collection_record_call_effect(
    ASTNode *call,
    ASTNode *receiver,
    PgyCollectionOwnershipEffectKind kind,
    SemanticContext *ctx)
{
    return semantic_collection_record_call_effect_from(
        call, receiver, kind, 0, ctx);
}

bool
semantic_collection_record_call_effect_from(
    ASTNode *call,
    ASTNode *receiver,
    PgyCollectionOwnershipEffectKind kind,
    uint32_t source_syntax_id,
    SemanticContext *ctx)
{
    Symbol *binding = NULL;
    PgyCollectionOwnershipFact *fact;
    uint32_t binding_id;

    if (call == NULL || call->type != AST_CALL || receiver == NULL
        || kind <= PGY_COLLECTION_EFFECT_NONE
        || kind > PGY_COLLECTION_EFFECT_DROP
        || (source_syntax_id != 0
            && kind != PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH)) {
        return false;
    }
    fact = receiver_collection_fact(receiver, ctx, &binding);
    if (fact == NULL || fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL)
        return true;
    binding_id = receiver_binding_syntax_id(receiver, binding);
    return binding_id != 0
        && ast_call_set_semantic_collection_effect(
            call, (uint32_t)kind, binding_id, source_syntax_id);
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
        if (binding != NULL && binding->is_parameter
            && is_array_string(binding->type)
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
        if (fact->origin == PGY_COLLECTION_ORIGIN_MEMBER_MOVE) {
            /* Aggregate-parameter element carriage is not owned by this
             * local-transition rung yet.  Preserve the pre-existing boundary
             * admission without forging OWNED: direct local aggregate moves
             * still fail below, while the later parameter/return rung must
             * replace this explicit UNKNOWN boundary with a stable fact. */
            if (collection_binding_is_current_parameter(
                    ctx, fact->source_binding_syntax_id))
                return true;
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
        || !is_array_string(target_type) || !is_array_string(value_type)) {
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
    if (source_fact == NULL
        || (source_fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL
            && source_fact->disposition !=
                PGY_COLLECTION_DISPOSITION_RETIRED
            && source_fact->element_ownership !=
                PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
            && source_fact->element_ownership !=
                PGY_STRING_ARRAY_OWNED_ELEMENTS)) {
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
