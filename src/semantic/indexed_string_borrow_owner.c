/* Exact storage-source liveness for String values borrowed from Array<String>.
 * The semantic identity is a SyntaxNodeId carried by the current collection
 * definition. Symbol pointers are used only for transient scope lookup. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "indexed_string_borrow_owner.h"
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "array_storage_deferred_preservation_owner.h"
#include "collection_ownership_fact.h"
#include "type_checker_internal.h"

#define INDEXED_STRING_BORROW_UNKNOWN_SOURCE UINT32_MAX

static bool
symbol_is_string_array(const Symbol *symbol)
{
    Type *element;
    if (symbol == NULL ||
        !type_is_constructed_named(symbol->type, "Array") ||
        type_constructed_arg_count(symbol->type) != 1)
        return false;
    element = type_constructed_arg(symbol->type, 0);
    return element != NULL && type_equals(element, TYPE_STRING);
}

static uint32_t
collection_storage_source_id(const Symbol *symbol)
{
    if (symbol == NULL)
        return 0;
    if (symbol->collection_storage_source_syntax_id != 0)
        return symbol->collection_storage_source_syntax_id;
    return symbol->decl_syntax_id;
}

static Symbol *
identifier_symbol(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL || ctx == NULL || node->type != AST_IDENTIFIER)
        return NULL;
    /* This query also runs inside value publication. The expression lookup
     * re-enters that issuer; read the already-typed binding without publishing. */
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(node));
    return binding != NULL && binding->decl_syntax_id != 0
        && binding->decl_syntax_id == ast_identifier_binding_syntax_id(node)
        ? binding : NULL;
}

typedef struct IndexedConcatVisit {
    const ASTNode *node;
    const struct IndexedConcatVisit *parent;
} IndexedConcatVisit;

static bool
pending_string_concat_current(const ASTNode *value, const SemanticContext *ctx,
    const IndexedConcatVisit *parent)
{
    if (value == NULL || value->type != AST_BINARY || !publication_generation_ready(ctx)
        || ast_binary_operator(value).type != TOKEN_PLUS) return false;
    for (const IndexedConcatVisit *p = parent; p != NULL; p = p->parent)
        if (p->node == value) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, value, false);
    if (row == NULL || row->node != value || row->id == 0 || row->id != ast_node_stable_id(value)
        || !row->value_present || row->value_shape != AST_BINARY || row->meta
        || row->value_type != TYPE_STRING || row->value_type->kind != TYPE_KIND_PRIMITIVE
        || !row->string_value || !row->origin_ready || !row->builtin_string_concat
        || row->argument_count != 2 || row->arguments == NULL) return false;
    IndexedConcatVisit visit = {value, parent};
    const ASTNode *nodes[2] = {ast_binary_left(value), ast_binary_right(value)};
    for (size_t i = 0; i < 2; i++) {
        const PublishedCallArgument *snapshot = &row->arguments[i];
        if (nodes[i] == NULL || snapshot->node != nodes[i] || snapshot->id == 0
            || snapshot->id != ast_node_stable_id(nodes[i]) || snapshot->shape != nodes[i]->type
            || snapshot->binding != ast_identifier_binding_syntax_id(nodes[i])) return false;
        PublishedNode *operand = publication_node(ctx->function_param_publications, nodes[i], false);
        const Type *type = operand == NULL ? NULL : operand->value_type;
        const Type *effective = type_is_slot_handle(type) ? type_slot_inner_type(type) : type;
        if (operand == NULL || operand->node != nodes[i] || operand->id != snapshot->id
            || !operand->value_present || operand->value_shape != snapshot->shape || type != snapshot->type
            || effective != TYPE_STRING || effective->kind != TYPE_KIND_PRIMITIVE
            || (nodes[i]->type == AST_IDENTIFIER && operand->binding_id != snapshot->binding)
            || (nodes[i]->type == AST_CALL && !publication_call_ready(operand))) return false;
        if (nodes[i]->type == AST_BINARY && !pending_string_concat_current(nodes[i], ctx, &visit)) return false;
    }
    return true;
}

bool
semantic_indexed_string_independent_result(const ASTNode *expression,
                                          const SemanticContext *ctx)
{
    const ASTNode *callee;
    const char *name;
    uint32_t builtin_kind = 0;

    if (expression == NULL)
        return false;
    if (expression->type == AST_STRING)
        return true;
    if (expression->type == AST_BINARY) {
        if (!publication_generation_ready(ctx)) return false;
        PublishedNode *concat = publication_node(ctx->function_param_publications, expression, false);
        if (concat == NULL || concat->node != expression || concat->id == 0
            || concat->id != ast_node_stable_id(expression)) return false;
        return semantic_param_publication_inputs_ready(ctx)
            ? semantic_param_publication_checked_local_owner_constructor(ctx, expression)
            : pending_string_concat_current(expression, ctx, NULL);
    }
    if (expression->type != AST_CALL)
        return false;
    if (ast_call_semantic_callee_decl_id(expression) != 0 ||
        ast_call_semantic_callee_value_binding_id(expression) != 0 ||
        !ast_call_semantic_callee_builtin_kind(expression, &builtin_kind))
        return false;
    if (builtin_kind == BUILTIN_TEXT_BUILDER_FINISH)
        return semantic_param_publication_checked_finish_independent(ctx, expression);
    if (!ast_call_semantic_callee_is_stdlib(expression))
        return false;
    callee = ast_call_callee(expression);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return false;
    name = ast_identifier_name(callee);
    /* Consumed only after successful String typing (publication or binding).
     * These checked allocating readers do not inherit freshness from their
     * input-retention classification. Empty/NULL failure results are not an
     * exact-copy or successful-allocation certificate. */
    if (name != NULL && builtin_kind == BUILTIN_NOT_BUILTIN &&
        ast_node_stable_id(expression) != 0 && ast_node_stable_id(callee) != 0 &&
        ((strcmp(name, "StringTrim") == 0 && ast_call_arg_count(expression) == 1) ||
         (strcmp(name, "CharAtN") == 0 && ast_call_arg_count(expression) == 3) ||
         (strcmp(name, "Substring") == 0 && ast_call_arg_count(expression) == 3) ||
         (strcmp(name, "SubstringWithLen") == 0 && ast_call_arg_count(expression) == 4)))
        return true;
    return name != NULL && (strcmp(name, "Concat") == 0
        || strcmp(name, "StringConcat") == 0
        || strcmp(name, "StringJoin") == 0);
}

static uint32_t
borrow_source_for_expression(ASTNode *expression, SemanticContext *ctx)
{
    Symbol *source;
    ASTNode *base;
    if (expression == NULL)
        return INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
    if (semantic_indexed_string_independent_result(expression, ctx))
        return 0;
    uint32_t producer = 0;
    if (expression->type == AST_CALL
        && semantic_collection_owned_string_call_result(expression, ctx, &producer))
        return 0;
    if (expression->type == AST_IDENTIFIER) {
        source = identifier_symbol(expression, ctx);
        if (source != NULL && type_equals(source->type, TYPE_STRING))
            return source->indexed_string_borrow_source_syntax_id;
        return INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
    }
    if (expression->type != AST_ARRAY_ACCESS)
        return INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
    base = ast_array_access_array(expression);
    source = identifier_symbol(base, ctx);
    if (!symbol_is_string_array(source))
        return INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
    return collection_storage_source_id(source);
}

static uint32_t
borrow_source_join(uint32_t current, uint32_t incoming)
{
    if (current == 0)
        return incoming;
    if (incoming == 0 || current == incoming)
        return current;
    return INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
}

static bool
deferred_string_read_failure(ASTNode *site, SemanticContext *ctx, const char *reason)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        site, "Deferred String read requires its Array owner to remain live: %s", reason);
    return false;
}

static bool
record_deferred_string_read(ASTNode *use, uint32_t source_id, Symbol *binding,
    bool array_receiver, bool invalidation_snapshot, bool owner_release, SemanticContext *ctx)
{
    if (ctx == NULL || !ctx->in_defer_cleanup || ctx->future_lifecycle_unreachable_depth != 0)
        return true;
    if (source_id == 0 && binding == NULL) return true;
    Scope *owner = ctx->indexed_string_defer_scope;
    bool active = false;
    for (Scope *s = ctx->scope; s != NULL; s = s->kind == SCOPE_FUNCTION ? NULL : s->parent) if (s == owner && s->kind == SCOPE_FUNCTION) active = true;
    uint32_t use_id = ast_node_stable_id(use);
    uint32_t body_id = ast_node_stable_id(ctx->indexed_string_defer_body);
    uint32_t function_id = ast_node_stable_id(ctx->current_function_decl);
    if (!active || owner == NULL || use_id == 0 || body_id == 0 || function_id == 0
        || ctx->indexed_string_defer_sequence == 0
        || owner->indexed_string_deferred_read_count > owner->indexed_string_deferred_read_capacity
        || (owner->indexed_string_deferred_read_count != 0 && owner->indexed_string_deferred_reads == NULL))
        return deferred_string_read_failure(use, ctx, "missing lexical receipt identity");
    uint32_t binding_id = binding != NULL ? binding->decl_syntax_id : 0;
    const ASTNode *bound = array_receiver ? ast_array_access_array(use) : use;
    if (binding != NULL && (binding_id == 0 || (!invalidation_snapshot
        && (bound == NULL || bound->type != AST_IDENTIFIER
            || ast_identifier_binding_syntax_id(bound) != binding_id))))
        return deferred_string_read_failure(use, ctx, "missing deferred binding identity");
    for (size_t i = 0; i < owner->indexed_string_deferred_read_count; i++) {
        IndexedStringDeferredRead *row = &owner->indexed_string_deferred_reads[i];
        if (row->function_id != function_id || row->body_id != body_id || row->use_id != use_id
            || row->invalidation_snapshot != invalidation_snapshot
            || row->owner_release != owner_release
            || (invalidation_snapshot && row->binding_id != binding_id)) continue;
        if (row->use != use || row->binding_id != binding_id || row->array_receiver != array_receiver)
            return deferred_string_read_failure(use, ctx, "changed deferred read identity");
        row->source_id = borrow_source_join(row->source_id, source_id);
        if (invalidation_snapshot && row->sequence != ctx->indexed_string_defer_sequence) {
            row->prior_invalidated = binding->indexed_string_borrow_invalidated;
            row->sequence = ctx->indexed_string_defer_sequence;
        }
        return true;
    }
    if (owner->indexed_string_deferred_read_count == owner->indexed_string_deferred_read_capacity) {
        size_t capacity = owner->indexed_string_deferred_read_capacity;
        size_t next = capacity == 0 ? 8 : capacity * 2;
        if (next <= capacity || next > SIZE_MAX / sizeof(IndexedStringDeferredRead))
            return deferred_string_read_failure(use, ctx, "receipt capacity overflow");
        IndexedStringDeferredRead *rows = realloc(owner->indexed_string_deferred_reads,
            next * sizeof(*rows));
        if (rows == NULL) return deferred_string_read_failure(use, ctx, "receipt allocation failed");
        owner->indexed_string_deferred_reads = rows;
        owner->indexed_string_deferred_read_capacity = next;
    }
    owner->indexed_string_deferred_reads[owner->indexed_string_deferred_read_count++] =
        (IndexedStringDeferredRead){.function = ctx->current_function_decl,
            .body = ctx->indexed_string_defer_body, .use = use,
            .function_id = function_id, .body_id = body_id, .use_id = use_id,
            .use_kind = (uint32_t)use->type, .binding_id = binding_id, .source_id = source_id,
            .sequence = ctx->indexed_string_defer_sequence,
            .array_receiver = array_receiver, .nested = ctx->indexed_string_nested_defer,
            .invalidation_snapshot = invalidation_snapshot,
            .owner_release = owner_release,
            .prior_invalidated = binding != NULL && binding->indexed_string_borrow_invalidated};
    return true;
}

static void
deferred_string_read_assignment(Symbol *binding, uint32_t source_id,
    bool array_receiver, SemanticContext *ctx)
{
    for (Scope *s = ctx->scope; s != NULL; s = s->kind == SCOPE_FUNCTION ? NULL : s->parent) {
        if (s->indexed_string_deferred_read_count > s->indexed_string_deferred_read_capacity
            || (s->indexed_string_deferred_read_count != 0 && s->indexed_string_deferred_reads == NULL)) {
            (void)deferred_string_read_failure(NULL, ctx, "missing assignment read storage");
            return;
        }
        for (size_t i = 0; i < s->indexed_string_deferred_read_count; i++) {
            IndexedStringDeferredRead *row = &s->indexed_string_deferred_reads[i];
            if (!row->invalidation_snapshot && row->binding_id == binding->decl_syntax_id
                && (row->array_receiver || row->owner_release) == array_receiver)
                row->source_id = borrow_source_join(row->source_id, source_id);
        }
    }
}

static bool
deferred_string_receipt_current(const IndexedStringDeferredRead *row, SemanticContext *ctx)
{
    if (row->function != ctx->current_function_decl || row->function_id == 0 || row->body_id == 0 || row->use_id == 0 || row->sequence == 0
        || ast_node_stable_id(row->function) != row->function_id
        || ast_node_stable_id(row->body) != row->body_id
        || ast_node_stable_id(row->use) != row->use_id || (uint32_t)row->use->type != row->use_kind)
        return false;
    const ASTNode *bound = row->array_receiver ? ast_array_access_array(row->use) : row->use;
    if (!row->invalidation_snapshot && row->binding_id != 0 && (bound == NULL || bound->type != AST_IDENTIFIER
        || ast_identifier_binding_syntax_id(bound) != row->binding_id)) return false;
    if (!row->owner_release && !row->invalidation_snapshot) {
        uint32_t issued_source;
        return semantic_indexed_string_borrow_source_current((ASTNode *)row->use, ctx, &issued_source);
    }
    if (row->invalidation_snapshot && (row->owner_release || row->array_receiver
        || row->binding_id == 0 || row->binding_id == ast_identifier_binding_syntax_id(row->use))) return false;
    PublishedNode *value = ctx->function_param_publications != NULL
        ? publication_node(ctx->function_param_publications, (ASTNode *)row->use, false) : NULL;
    return publication_generation_ready(ctx) && value != NULL && value->value_present
        && value->id == row->use_id && value->value_shape == AST_IDENTIFIER && !value->meta
        && value->binding_id == ast_identifier_binding_syntax_id(row->use)
        && type_is_constructed_named(value->value_type, "Array")
        && type_equals(type_get_constructed_arg(value->value_type, 0), TYPE_STRING)
        && (!semantic_param_publication_inputs_ready(ctx) || semantic_param_publication_value(ctx, row->use).ok);
}

bool
semantic_indexed_string_borrow_deferred_release_current(SemanticContext *ctx,
    uint32_t binding_id, uint32_t source_id, bool *reserved)
{
    if (reserved == NULL) return false;
    *reserved = false;
    if (ctx == NULL || ctx->has_error || binding_id == 0) return false;
    bool current_binding = false;
    for (Scope *s = ctx->scope; s != NULL; s = s->kind == SCOPE_FUNCTION ? NULL : s->parent) {
        for (size_t j = 0; j < s->symbol_count; j++) {
            Symbol *binding = s->symbols[j];
            if (symbol_is_string_array(binding) && binding->decl_syntax_id == binding_id
                && collection_storage_source_id(binding) == source_id) current_binding = true;
        }
        if (s->indexed_string_deferred_read_count > s->indexed_string_deferred_read_capacity
            || (s->indexed_string_deferred_read_count != 0 && s->indexed_string_deferred_reads == NULL)) return false;
        for (size_t i = 0; i < s->indexed_string_deferred_read_count; i++) {
            const IndexedStringDeferredRead *row = &s->indexed_string_deferred_reads[i];
            if (!deferred_string_receipt_current(row, ctx)) return false;
            if (!row->owner_release) continue;
            if (source_id == 0 || source_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE
                || row->source_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE) return false;
            if (row->source_id == source_id) *reserved = true;
        }
    }
    return current_binding;
}

void
semantic_indexed_string_borrow_restore_defer(SemanticContext *ctx)
{
    if (ctx == NULL) return;
    Scope *owner = ctx->indexed_string_defer_scope;
    if (owner == NULL) return;
    if (owner->indexed_string_deferred_read_count > owner->indexed_string_deferred_read_capacity
        || (owner->indexed_string_deferred_read_count != 0 && owner->indexed_string_deferred_reads == NULL)) {
        (void)deferred_string_read_failure(ctx->indexed_string_defer_body, ctx, "missing cleanup restore storage");
        return;
    }
    for (size_t i = owner->indexed_string_deferred_read_count; i > 0; i--) {
        const IndexedStringDeferredRead *row = &owner->indexed_string_deferred_reads[i - 1];
        if (!row->invalidation_snapshot || row->body != ctx->indexed_string_defer_body
            || row->sequence != ctx->indexed_string_defer_sequence) continue;
        if (!deferred_string_receipt_current(row, ctx)) {
            (void)deferred_string_read_failure(ctx->indexed_string_defer_body, ctx, "stale cleanup restore receipt");
            return;
        }
        for (Scope *s = ctx->scope; s != NULL; s = s->kind == SCOPE_FUNCTION ? NULL : s->parent)
            for (size_t j = 0; j < s->symbol_count; j++) {
                Symbol *binding = s->symbols[j];
                if (binding != NULL && binding->decl_syntax_id == row->binding_id)
                    binding->indexed_string_borrow_invalidated = row->prior_invalidated;
            }
    }
}

bool
semantic_indexed_string_borrow_owner_transfer_allowed(ASTNode *receiver, SemanticContext *ctx)
{
    if (ctx == NULL || ctx->has_error) return false;
    Symbol *owner = identifier_symbol(receiver, ctx);
    if (!symbol_is_string_array(owner)) return true;
    uint32_t source_id = collection_storage_source_id(owner);
    for (Scope *s = ctx->scope; s != NULL; s = s->kind == SCOPE_FUNCTION ? NULL : s->parent) {
        if (s->indexed_string_deferred_read_count > s->indexed_string_deferred_read_capacity
            || (s->indexed_string_deferred_read_count != 0 && s->indexed_string_deferred_reads == NULL))
            return deferred_string_read_failure(receiver, ctx, "missing active read storage");
        for (size_t i = 0; i < s->indexed_string_deferred_read_count; i++) {
            const IndexedStringDeferredRead *row = &s->indexed_string_deferred_reads[i];
            if (!deferred_string_receipt_current(row, ctx))
                return deferred_string_read_failure(receiver, ctx, "stale deferred read identity");
            if (row->invalidation_snapshot) continue;
            if (row->source_id == 0) continue;
            if (!row->owner_release && !row->nested && ctx->in_defer_cleanup
                && row->body == ctx->indexed_string_defer_body)
                continue; /* Earlier reads in this cleanup obey the ordinary sequential use check. */
            if (source_id == 0 || source_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE
                || row->source_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE || row->source_id == source_id)
                return deferred_string_read_failure(receiver, ctx, "a later cleanup still reads this source");
        }
    }
    return true;
}

bool
semantic_indexed_string_borrow_publish_source(ASTNode *expression,
    SemanticContext *ctx)
{
    PublishedNode *row = ctx != NULL && ctx->function_param_publications != NULL
        ? publication_node(ctx->function_param_publications, expression, false) : NULL;
    if (row == NULL || !row->value_present) return false;
    row->indexed_string_source_present = row->string_value;
    row->indexed_string_source_id = row->string_value
        ? borrow_source_for_expression(expression, ctx) : 0;
    if (row->string_value) {
        bool array_receiver = expression->type == AST_ARRAY_ACCESS;
        Symbol *binding = identifier_symbol(array_receiver ? ast_array_access_array(expression) : expression, ctx);
        if (!record_deferred_string_read(expression, row->indexed_string_source_id,
                binding, array_receiver, false, false, ctx)) return false;
    }
    return true;
}

bool
semantic_indexed_string_borrow_source_current(ASTNode *expression,
    SemanticContext *ctx, uint32_t *source_id)
{
    if (expression == NULL || source_id == NULL || !publication_generation_ready(ctx))
        return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, expression, false);
    if (row == NULL || row->id == 0 || row->id != ast_node_stable_id(expression)
        || !row->value_present || row->meta || row->value_shape != expression->type
        || !row->string_value || !type_equals(row->value_type, TYPE_STRING)
        || !row->indexed_string_source_present
        || (expression->type == AST_IDENTIFIER
            && row->binding_id != ast_identifier_binding_syntax_id(expression))) return false;
    bool concat = row->indexed_string_source_id == 0 && expression->type == AST_BINARY;
    if (concat && !semantic_indexed_string_independent_result(expression, ctx)) return false;
    if (!concat && semantic_param_publication_inputs_ready(ctx)
        && !semantic_param_publication_value(ctx, expression).ok) return false;
    if (row->indexed_string_source_id == 0 && expression->type == AST_CALL) {
        uint32_t producer = 0;
        if (!semantic_indexed_string_independent_result(expression, ctx)
            && !semantic_collection_owned_string_call_result(expression, ctx, &producer)) return false;
    }
    *source_id = row->indexed_string_source_id;
    return true;
}

void
semantic_indexed_string_borrow_call_argument(ASTNode *expression,
    ASTNode *call, ASTNode *callee, size_t ordinal, bool constructor,
    SemanticContext *ctx)
{
    uint32_t source_id = INDEXED_STRING_BORROW_UNKNOWN_SOURCE;
    if (ctx == NULL || ctx->has_error || ctx->current_function_decl == NULL) return;
    (void)semantic_indexed_string_borrow_source_current(expression, ctx, &source_id);
    if (source_id == 0) return;
    /* A shallow store has no callee that can prove non-retention. This also
     * covers a borrowed formal whose payload owner is in the caller's frame. */
    if (call == NULL && callee == NULL
        && source_id != INDEXED_STRING_BORROW_UNKNOWN_SOURCE) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            expression, "A borrowed String element cannot be retained beyond its Array owner without a copy");
        return;
    }
    bool named = !constructor && callee != NULL && callee->type == AST_FUNC_DECL
        && ast_func_body(callee) != NULL && !callee->is_async_decl
        && ast_call_semantic_callee_declared_callable(call)
        && ast_call_semantic_callee_decl_id(call) == ast_node_stable_id(callee)
        && ast_call_semantic_callee_value_binding_id(call) == 0;
    for (Scope *scope = ctx->scope; scope != NULL; scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *owner = scope->symbols[i];
            if (!symbol_is_string_array(owner)
                || (source_id != INDEXED_STRING_BORROW_UNKNOWN_SOURCE
                    && collection_storage_source_id(owner) != source_id)) continue;
            PgyCollectionOwnershipFact *fact = (PgyCollectionOwnershipFact *)
                semantic_collection_ownership_fact_find(ctx,
                    ast_node_stable_id(ctx->current_function_decl), owner->decl_syntax_id);
            if (owner->is_consumed || (fact != NULL &&
                fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE)) {
                bool reserved = false;
                if (!semantic_indexed_string_borrow_deferred_release_current(ctx,
                        owner->decl_syntax_id, collection_storage_source_id(owner), &reserved)) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                        expression, "A deferred Array release has no current String element source");
                    return;
                }
                if (!reserved) continue;
            }
            if (fact == NULL || fact->element_ownership == PGY_STRING_ARRAY_BORROWED_ELEMENTS) continue;
            if (named && source_id != INDEXED_STRING_BORROW_UNKNOWN_SOURCE) {
                if (!semantic_array_storage_record_pending_payload(ctx, owner, call, callee, ordinal) ||
                    !semantic_array_storage_record_pending_call_consumer(ctx, owner, call, ordinal)) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                        expression, "String element handoff has no current source-lifetime receipt");
                    fact->return_elements_owned = false;
                }
            } else {
                /* A retained scalar element invalidates the String lifetime,
                 * even though the Array descriptor itself never escaped. */
                fact->return_elements_owned = false;
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                    expression, "A String element cannot cross an opaque boundary without a current non-retention proof");
            }
        }
    }
}

void
semantic_indexed_string_borrow_initialize_binding(
    Symbol *binding, ASTNode *initializer, SemanticContext *ctx)
{
    Symbol *source;
    if (binding == NULL)
        return;
    if (type_is_constructed_named(binding->type, "Array")) {
        binding->collection_storage_source_syntax_id =
            binding->decl_syntax_id;
        source = identifier_symbol(initializer, ctx);
        if (source != NULL &&
            type_is_constructed_named(source->type, "Array")) {
            uint32_t source_id = collection_storage_source_id(source);
            if (source_id != 0)
                binding->collection_storage_source_syntax_id = source_id;
        }
    }
    if (type_equals(binding->type, TYPE_STRING)) {
        binding->indexed_string_borrow_source_syntax_id =
            borrow_source_for_expression(initializer, ctx);
        binding->indexed_string_borrow_invalidated = false;
    }
}

void
semantic_indexed_string_borrow_record_assignment(
    Symbol *binding, ASTNode *value, ASTNode *assignment,
    SemanticContext *ctx)
{
    Symbol *source;
    if (binding == NULL)
        return;
    if (type_is_constructed_named(binding->type, "Array")) {
        uint32_t source_id = ast_node_stable_id(assignment);
        uint32_t current_id = collection_storage_source_id(binding);
        source = identifier_symbol(value, ctx);
        if (source != NULL &&
            type_is_constructed_named(source->type, "Array"))
            source_id = collection_storage_source_id(source);
        binding->collection_storage_source_syntax_id =
            borrow_source_join(current_id, source_id);
        deferred_string_read_assignment(binding, source_id, true, ctx);
        return;
    }
    if (type_equals(binding->type, TYPE_STRING)) {
        uint32_t incoming = borrow_source_for_expression(value, ctx);
        binding->indexed_string_borrow_source_syntax_id = borrow_source_join(
            binding->indexed_string_borrow_source_syntax_id, incoming);
        deferred_string_read_assignment(binding, incoming, false, ctx);
    }
}

void
semantic_indexed_string_borrow_invalidate_deep_drop(
    ASTNode *receiver, SemanticContext *ctx)
{
    Symbol *owner = identifier_symbol(receiver, ctx);
    uint32_t source_id;
    if (!symbol_is_string_array(owner) || ctx == NULL)
        return;
    source_id = collection_storage_source_id(owner);
    if (source_id == 0)
        return;
    if (ctx->in_defer_cleanup && !record_deferred_string_read(receiver, source_id,
            owner, false, false, true, ctx)) return;
    for (Scope *scope = ctx->scope; scope != NULL; scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *candidate = scope->symbols[i];
            if (candidate != NULL && type_equals(candidate->type, TYPE_STRING) &&
                (source_id == INDEXED_STRING_BORROW_UNKNOWN_SOURCE
                 || candidate->indexed_string_borrow_source_syntax_id ==
                        INDEXED_STRING_BORROW_UNKNOWN_SOURCE
                 || candidate->indexed_string_borrow_source_syntax_id ==
                        source_id)) {
                if (ctx->in_defer_cleanup && !record_deferred_string_read(receiver,
                        candidate->indexed_string_borrow_source_syntax_id, candidate, false, true, false, ctx)) return;
                candidate->indexed_string_borrow_invalidated = true;
            }
        }
    }
}

bool
semantic_indexed_string_borrow_validate_use(
    Symbol *binding, ASTNode *use, SemanticContext *ctx)
{
    if (binding != NULL && type_equals(binding->type, TYPE_STRING)
        && !record_deferred_string_read(use, binding->indexed_string_borrow_source_syntax_id,
            binding, false, false, false, ctx)) return false;
    if (binding == NULL ||
        binding->indexed_string_borrow_source_syntax_id == 0 ||
        !binding->indexed_string_borrow_invalidated)
        return true;
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, use,
        "String '%s' borrows an Array<String> element that was released.\n"
        "Reason:\n"
        "- ArrayDropOwnedStrings retired the exact storage source that owns this String payload\n"
        "- using the indexed alias afterwards would read freed memory\n"
        "Fix:\n"
        "- copy the String with Concat(\"\", value) before the deep drop\n"
        "- or keep the Array<String> owner live until the final alias use",
        binding->name != NULL ? binding->name : "<string>");
    return false;
}
