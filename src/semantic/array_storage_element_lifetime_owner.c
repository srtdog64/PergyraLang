/* Canonical native plain-element lifetime predicate for public storage release. */
#include "array_storage_element_lifetime_owner.h"
#include "array_storage_deferred_preservation_owner.h"
#include "type_checker_internal.h"
#include "type_checker_resolution_internal.h"
#include "compiler/decl_field_model.h"
#include "function_param_publication_internal.h"
#include "collection_ownership_fact.h"
#include "indexed_string_borrow_owner.h"
#include "diag_codes.h"
#include "type_checker_resolution_metadata_internal.h"
#include "slot_summary.h"
#include "builtin_kind.h"
#include <stdlib.h>
#include <string.h>

static bool
deferred_assignment_source(const IndexedStringDeferredAssignment *event,
    const IndexedStringDeferredRead *row, SemanticContext *ctx, uint32_t *source)
{
    const ASTNode *target = ast_assignment_target(event->assignment);
    if (event->assignment == NULL || event->assignment->type != AST_ASSIGNMENT
        || event->assignment_id == 0 || event->assignment_id != ast_node_stable_id(event->assignment)
        || event->value == NULL || event->value_id == 0
        || event->value_id != ast_node_stable_id(event->value)
        || ast_assignment_value(event->assignment) != event->value
        || target == NULL || target != event->target || target->type != AST_IDENTIFIER
        || event->target_id == 0 || event->target_id != ast_node_stable_id(target)
        || event->binding_id != row->binding_id
        || ast_identifier_binding_syntax_id(target) != row->binding_id
        || event->receipt_sequence != row->sequence) return false;
    if (!row->array_receiver && !row->owner_release)
        return semantic_indexed_string_borrow_source_current((ASTNode *)event->value, ctx, source);
    if (event->value->type == AST_IDENTIFIER)
        return semantic_indexed_string_borrow_storage_source_current((ASTNode *)event->value, ctx, source);
    PublishedNode *value = publication_node(ctx->function_param_publications, event->value, false);
    if (value == NULL || value->id != event->value_id || value->value_shape != event->value->type
        || !value->value_present || value->meta || !type_is_constructed_named(value->value_type, "Array")
        || !type_equals(type_get_constructed_arg(value->value_type, 0), TYPE_STRING)
        || (semantic_param_publication_inputs_ready(ctx)
            && !semantic_param_publication_value(ctx, event->value).ok)) return false;
    *source = event->assignment_id;
    return true;
}

bool
semantic_array_storage_deferred_read_current(const IndexedStringDeferredRead *row,
    SemanticContext *ctx)
{
    if (ctx == NULL || row == NULL || row->function != ctx->current_function_decl
        || row->function_id == 0 || row->body_id == 0 || row->use_id == 0 || row->sequence == 0
        || row->sequence > ctx->indexed_string_next_defer_sequence
        || ast_node_stable_id(row->function) != row->function_id
        || ast_node_stable_id(row->body) != row->body_id
        || ast_node_stable_id(row->use) != row->use_id || (uint32_t)row->use->type != row->use_kind
        || row->assignment_count > row->assignment_capacity
        || (row->assignment_count != 0 && row->assignments == NULL)) return false;
    const ASTNode *bound = row->array_receiver ? ast_array_access_array(row->use) : row->use;
    if (bound != row->bound || row->bound_id == 0
        || ast_node_stable_id(bound) != row->bound_id) return false;
    if (row->binding_id != 0 && (bound == NULL || bound->type != AST_IDENTIFIER
        || ast_identifier_binding_syntax_id(bound) != row->binding_id)) return false;
    uint32_t issued;
    if (!(row->owner_release
        ? semantic_indexed_string_borrow_storage_source_current((ASTNode *)row->use, ctx, &issued)
        : semantic_indexed_string_borrow_source_current((ASTNode *)row->use, ctx, &issued))
        || issued != row->issued_source_id) return false;
    uint32_t joined = issued;
    for (size_t i = 0; i < row->assignment_count; i++) {
        const IndexedStringDeferredAssignment *event = &row->assignments[i];
        uint32_t source;
        if (event->ordinal != i || !deferred_assignment_source(event, row, ctx, &source)
            || source != event->source_id) return false;
        if (joined == 0) joined = source;
        else if (source != 0 && source != joined) joined = UINT32_MAX;
    }
    return joined == row->source_id;
}

bool
semantic_array_storage_deferred_read_assignment(IndexedStringDeferredRead *row,
    ASTNode *assignment, ASTNode *value, uint32_t source_id, SemanticContext *ctx)
{
    if (!semantic_array_storage_deferred_read_current(row, ctx)) return false;
    const ASTNode *target = ast_assignment_target(assignment);
    IndexedStringDeferredAssignment event = {.assignment = assignment, .value = value,
        .target = target, .assignment_id = ast_node_stable_id(assignment),
        .value_id = ast_node_stable_id(value), .target_id = ast_node_stable_id(target),
        .binding_id = row->binding_id, .source_id = source_id,
        .ordinal = row->assignment_count, .receipt_sequence = row->sequence};
    uint32_t current;
    if (!deferred_assignment_source(&event, row, ctx, &current) || current != source_id) return false;
    if (row->assignment_count == row->assignment_capacity) {
        size_t capacity = row->assignment_capacity == 0 ? 4 : row->assignment_capacity * 2;
        if (capacity <= row->assignment_capacity || capacity > SIZE_MAX / sizeof(event)) return false;
        IndexedStringDeferredAssignment *events = realloc(row->assignments, capacity * sizeof(event));
        if (events == NULL) return false;
        row->assignments = events;
        row->assignment_capacity = capacity;
    }
    row->assignments[row->assignment_count++] = event;
    if (row->source_id == 0) row->source_id = source_id;
    else if (source_id != 0 && source_id != row->source_id) row->source_id = UINT32_MAX;
    return true;
}

bool semantic_array_storage_deferred_read_failure(ASTNode *site,
    SemanticContext *ctx, const char *reason)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        site, "Deferred String read requires its Array owner to remain live: %s", reason);
    return false;
}

bool semantic_array_storage_record_deferred_string_read(ASTNode *use,
    uint32_t source_id, Symbol *binding, bool array_receiver,
    bool owner_release, SemanticContext *ctx)
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
        return semantic_array_storage_deferred_read_failure(use, ctx, "missing lexical receipt identity");
    uint32_t binding_id = binding != NULL ? binding->decl_syntax_id : 0;
    const ASTNode *bound = array_receiver ? ast_array_access_array(use) : use;
    if (binding != NULL && (binding_id == 0 || bound == NULL || bound->type != AST_IDENTIFIER
        || ast_identifier_binding_syntax_id(bound) != binding_id))
        return semantic_array_storage_deferred_read_failure(use, ctx, "missing deferred binding identity");
    for (size_t i = 0; i < owner->indexed_string_deferred_read_count; i++) {
        IndexedStringDeferredRead *row = &owner->indexed_string_deferred_reads[i];
        if (row->function_id != function_id || row->body_id != body_id || row->use_id != use_id
            || row->owner_release != owner_release) continue;
        if (row->use != use || row->binding_id != binding_id || row->array_receiver != array_receiver)
            return semantic_array_storage_deferred_read_failure(use, ctx, "changed deferred read identity");
        if (row->issued_source_id != source_id)
            return semantic_array_storage_deferred_read_failure(use, ctx, "changed issued read source");
        return semantic_array_storage_deferred_read_current(row, ctx);
    }
    if (owner->indexed_string_deferred_read_count == owner->indexed_string_deferred_read_capacity) {
        size_t capacity = owner->indexed_string_deferred_read_capacity;
        size_t next = capacity == 0 ? 8 : capacity * 2;
        if (next <= capacity || next > SIZE_MAX / sizeof(IndexedStringDeferredRead))
            return semantic_array_storage_deferred_read_failure(use, ctx, "receipt capacity overflow");
        IndexedStringDeferredRead *rows = realloc(owner->indexed_string_deferred_reads,
            next * sizeof(*rows));
        if (rows == NULL) return semantic_array_storage_deferred_read_failure(use, ctx, "receipt allocation failed");
        owner->indexed_string_deferred_reads = rows;
        owner->indexed_string_deferred_read_capacity = next;
    }
    owner->indexed_string_deferred_reads[owner->indexed_string_deferred_read_count++] =
        (IndexedStringDeferredRead){.function = ctx->current_function_decl,
            .body = ctx->indexed_string_defer_body, .use = use, .bound = bound,
            .function_id = function_id, .body_id = body_id, .use_id = use_id,
            .use_kind = (uint32_t)use->type, .bound_id = ast_node_stable_id(bound),
            .binding_id = binding_id, .source_id = source_id,
            .issued_source_id = source_id,
            .sequence = ctx->indexed_string_defer_sequence,
            .array_receiver = array_receiver, .nested = ctx->indexed_string_nested_defer,
            .owner_release = owner_release};
    return true;
}

void semantic_array_storage_deferred_string_read_assignment(Symbol *binding,
    uint32_t source_id, bool array_receiver, ASTNode *assignment,
    ASTNode *value, SemanticContext *ctx)
{
    for (Scope *s = ctx->scope; s != NULL; s = s->kind == SCOPE_FUNCTION ? NULL : s->parent) {
        if (s->indexed_string_deferred_read_count > s->indexed_string_deferred_read_capacity
            || (s->indexed_string_deferred_read_count != 0 && s->indexed_string_deferred_reads == NULL)) {
            (void)semantic_array_storage_deferred_read_failure(assignment, ctx, "missing assignment read storage");
            return;
        }
        for (size_t i = 0; i < s->indexed_string_deferred_read_count; i++) {
            IndexedStringDeferredRead *row = &s->indexed_string_deferred_reads[i];
            if (row->binding_id == binding->decl_syntax_id
                && (row->array_receiver || row->owner_release) == array_receiver
                && !semantic_array_storage_deferred_read_assignment(row, assignment, value, source_id, ctx)) {
                (void)semantic_array_storage_deferred_read_failure(assignment, ctx, "stale assignment lineage");
                return;
            }
        }
    }
}

bool semantic_indexed_string_borrow_deferred_release_current(SemanticContext *ctx,
    uint32_t binding_id, uint32_t source_id, bool *reserved)
{
    if (reserved == NULL) return false;
    *reserved = false;
    if (ctx == NULL || ctx->has_error || binding_id == 0) return false;
    bool current_binding = false;
    for (Scope *s = ctx->scope; s != NULL; s = s->kind == SCOPE_FUNCTION ? NULL : s->parent) {
        for (size_t j = 0; j < s->symbol_count; j++) {
            Symbol *binding = s->symbols[j];
            uint32_t current_source = 0;
            if (binding != NULL && binding->decl_syntax_id == binding_id
                && semantic_indexed_string_borrow_array_binding_source(binding, &current_source)
                && current_source == source_id) current_binding = true;
        }
        if (s->indexed_string_deferred_read_count > s->indexed_string_deferred_read_capacity
            || (s->indexed_string_deferred_read_count != 0 && s->indexed_string_deferred_reads == NULL)) return false;
        for (size_t i = 0; i < s->indexed_string_deferred_read_count; i++) {
            const IndexedStringDeferredRead *row = &s->indexed_string_deferred_reads[i];
            if (!semantic_array_storage_deferred_read_current(row, ctx)) return false;
            if (!row->owner_release) continue;
            if (source_id == 0 || source_id == UINT32_MAX || row->source_id == UINT32_MAX) return false;
            if (row->source_id == source_id) *reserved = true;
        }
    }
    return current_binding;
}

static bool
array_storage_plain_at_depth(const Type *type, SemanticContext *ctx, unsigned depth)
{
    if (type == NULL || depth >= 32)
        return false;
    if (type_equals(type, TYPE_INT) || type_equals(type, TYPE_LONG) || type_equals(type, TYPE_FLOAT)
        || type_equals(type, TYPE_DOUBLE) || type_equals(type, TYPE_BOOL) || type_equals(type, TYPE_DURATION)) return true;
    if (type_is_constructed_named(type, "Result"))
        return type_constructed_arg_count(type) == 2 && array_storage_plain_at_depth(type_get_constructed_arg(type, 0), ctx, depth + 1)
            && array_storage_plain_at_depth(type_get_constructed_arg(type, 1), ctx, depth + 1);
    if (type_is_constructed_named(type, "Option"))
        return type_constructed_arg_count(type) == 1
            && array_storage_plain_at_depth(type_get_constructed_arg(type, 0), ctx, depth + 1);
    if (type->kind == TYPE_KIND_ENUM) {
        ASTNode *decl = semantic_find_enum_decl_by_name(ctx, type->name);
        size_t count = 0;
        if (decl == NULL)
            return false;
        (void)ast_enum_variants(decl, &count);
        for (size_t v = 0; v < count; v++) {
            for (size_t p = 0; p < ast_enum_variant_param_count(decl, v); p++) {
                Type *payload = semantic_type_resolution_lookup_metadata_type_ref(ctx, ast_enum_variant_param(decl, v, p));
                if (!array_storage_plain_at_depth(payload, ctx, depth + 1))
                    return false;
            }
        }
        return count != 0;
    }
    if (type->kind != TYPE_KIND_CLASS || type->nominal_flavor != TYPE_NOMINAL_STRUCT) return false;
    ASTNode *decl = semantic_host_decl_for_type(ctx, type);
    if (decl == NULL || decl->type != AST_CLASS_DECL) return false;
    PgyDeclField *fields = NULL;
    size_t count = 0;
    if (!pgy_class_decl_field_model_try_build(decl, &fields, &count)) return false;
    bool plain = true;
    for (size_t i = 0; i < count && plain; i++) {
        Type *field = semantic_type_resolution_lookup_metadata_type_ref(
            ctx, fields[i].type_ast);
        plain = array_storage_plain_at_depth(field, ctx, depth + 1);
    }
    free(fields);
    return plain;
}

bool
semantic_array_storage_plain_element(const Type *type, SemanticContext *ctx)
{
    return array_storage_plain_at_depth(type, ctx, 0);
}

static PublishedNode *
array_storage_element_value_current(ASTNode *value, SemanticContext *ctx)
{
    if (value == NULL || !publication_generation_ready(ctx)) return NULL;
    PublishedNode *row = publication_node(ctx->function_param_publications, value, false);
    if (row == NULL || row->node != value || row->id == 0
        || row->id != ast_node_stable_id(value) || !row->value_present || row->meta
        || row->value_shape != value->type || row->value_type == NULL
        || row->value_type == TYPE_UNKNOWN
        || (value->type == AST_IDENTIFIER && row->binding_id != ast_identifier_binding_syntax_id(value))
        || (value->type == AST_CALL && !publication_call_ready(row))) return NULL;
    if (semantic_param_publication_inputs_ready(ctx)) {
        SemanticPublishedValue current = semantic_param_publication_value(ctx, value);
        if (!current.ok || current.meta || !type_equals(current.type, row->value_type)) return NULL;
    }
    return row;
}

bool
semantic_array_storage_return_elements_owned(ASTNode *value,
    SemanticContext *ctx, const ASTNode **result_source_out)
{
    if (result_source_out == NULL) return false;
    *result_source_out = NULL;
    PublishedNode *row = array_storage_element_value_current(value, ctx);
    if (row == NULL || !type_is_constructed_named(row->value_type, "Array")
        || !type_equals(type_get_constructed_arg(row->value_type, 0), TYPE_STRING)) return false;
    if (value->type == AST_IDENTIFIER) {
        Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(value));
        uint32_t binding_id = ast_identifier_binding_syntax_id(value);
        const PgyCollectionOwnershipFact *fact = semantic_collection_ownership_fact_find(ctx,
            ast_node_stable_id(ctx->current_function_decl), binding_id);
        if (binding == NULL || binding->is_parameter || binding->is_host_field
            || binding->is_consumed || binding_id == 0 || binding->decl_syntax_id != binding_id
            || !type_equals(binding->type, row->value_type) || fact == NULL
            || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
            || !fact->return_elements_owned) return false;
        if ((fact->return_elements_source == NULL) != (fact->return_elements_source_id == 0)) return false;
        if (fact->return_elements_source != NULL) {
            if (fact->return_elements_source_id == 0
                || fact->return_elements_source_id != ast_node_stable_id(fact->return_elements_source)
                || array_storage_element_value_current((ASTNode *)fact->return_elements_source, ctx) == NULL) return false;
            if (fact->return_elements_source->type == AST_ARRAY_LITERAL) {
                const ASTNode *nested = NULL;
                if (!semantic_array_storage_return_elements_owned(
                        (ASTNode *)fact->return_elements_source, ctx, &nested)
                    || nested != NULL) return false;
            }
            *result_source_out = fact->return_elements_source;
        }
        return true;
    }
    if (value->type == AST_ARRAY_LITERAL) {
        for (size_t i = 0; i < ast_array_literal_count(value); i++) {
            ASTNode *child = ast_array_literal_element(value, i);
            PublishedNode *element = array_storage_element_value_current(child, ctx);
            uint32_t producer = 0;
            if (element == NULL || element->value_type != TYPE_STRING
                || !semantic_collection_owned_string_call_result(child, ctx, &producer)) return false;
        }
        return true;
    }
    if (value->type == AST_CALL && !row->stdlib && row->kind_present
        && row->builtin == BUILTIN_NOT_BUILTIN && row->declaration != 0
        && row->value_binding == 0 && row->declared_callable) {
        *result_source_out = value;
        return true;
    }
    if (value->type != AST_CALL || row->declaration != 0 || row->value_binding != 0
        || row->target_shape != AST_IDENTIFIER || row->target_binding != 0
        || row->target_name == NULL || row->argument_count != 1 || !row->kind_present) return false;
    bool map_keys = row->stdlib && row->builtin == BUILTIN_NOT_BUILTIN
        && strcmp(row->target_name, "MapKeys") == 0;
    bool clone = row->stdlib && row->builtin == BUILTIN_CLONE
        && strcmp(row->target_name, "Clone") == 0;
    if (!map_keys && !clone) return false;
    PublishedNode *argument = array_storage_element_value_current(ast_call_argument(value, 0), ctx);
    return argument != NULL && type_equals(argument->value_type, row->arguments[0].type);
}

bool
semantic_array_storage_return_elements_current(ASTNode *value,
    SemanticContext *ctx, const ArrayStorageReturnElementReceipt *receipt,
    const ASTNode **result_source_out)
{
    if (result_source_out == NULL || receipt == NULL || !receipt->owned
        || receipt->function_id == 0 || value == NULL) return false;
    *result_source_out = NULL;
    PublishedNode *row = array_storage_element_value_current(value, ctx);
    if (row == NULL || !type_is_constructed_named(row->value_type, "Array")
        || !type_equals(type_get_constructed_arg(row->value_type, 0), TYPE_STRING))
        return false;
    const ASTNode *source = NULL;
    if (value->type == AST_IDENTIFIER) {
        if (receipt->binding_id == 0
            || ast_identifier_binding_syntax_id(value) != receipt->binding_id) return false;
        const PgyCollectionOwnershipFact *fact = semantic_collection_ownership_fact_find(
            ctx, receipt->function_id, receipt->binding_id);
        if (fact == NULL || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
            || !fact->return_elements_owned
            || fact->return_elements_source != receipt->dependency_call
            || fact->return_elements_source_id != receipt->dependency_call_id) return false;
        source = fact->return_elements_source;
    } else if (receipt->binding_id != 0
        || !semantic_array_storage_return_elements_owned(value, ctx, &source)) return false;
    if (source != receipt->dependency_call
        || (source != NULL ? ast_node_stable_id(source) : 0) != receipt->dependency_call_id
        || (source != NULL && source->type != AST_CALL
            && source->type != AST_ARRAY_LITERAL)
        || (source != NULL
            && array_storage_element_value_current((ASTNode *)source, ctx) == NULL))
        return false;
    if (source != NULL && source->type == AST_ARRAY_LITERAL) {
        const ASTNode *nested = NULL;
        if (!semantic_array_storage_return_elements_owned((ASTNode *)source, ctx, &nested)
            || nested != NULL) return false;
        source = NULL;
    }
    *result_source_out = source;
    return true;
}

bool
semantic_array_storage_capture_element_use(ASTNode *value,
    SemanticContext *ctx, ArrayStorageReturnElementReceipt *receipt)
{
    if (receipt == NULL) return false;
    *receipt = (ArrayStorageReturnElementReceipt){0};
    PublishedNode *row = array_storage_element_value_current(value, ctx);
    uint32_t function_id = ctx != NULL
        ? ast_node_stable_id(ctx->current_function_decl) : 0;
    if (row == NULL || function_id == 0
        || !type_is_constructed_named(row->value_type, "Array")
        || !type_equals(type_get_constructed_arg(row->value_type, 0), TYPE_STRING)
        || (value->type == AST_CALL && (row->caller != ctx->current_function_decl
            || row->caller_id != function_id)))
        return false;
    receipt->function_id = function_id;
    if (value->type == AST_IDENTIFIER) {
        uint32_t binding_id = ast_identifier_binding_syntax_id(value);
        Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(value));
        if (binding_id == 0 || binding == NULL || binding->decl_syntax_id != binding_id
            || binding->is_consumed || !type_equals(binding->type, row->value_type)) return false;
        receipt->binding_id = binding_id;
        const PgyCollectionOwnershipFact *fact = semantic_collection_ownership_fact_find(
            ctx, receipt->function_id, binding_id);
        if (fact != NULL) {
            receipt->owned = fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
                && fact->return_elements_owned;
            receipt->dependency_call = fact->return_elements_source;
            receipt->dependency_call_id = fact->return_elements_source_id;
        }
    } else if (value->type == AST_CALL) {
        const ASTNode *source = NULL;
        receipt->owned = semantic_array_storage_return_elements_owned(value, ctx, &source);
        receipt->dependency_call = value;
        receipt->dependency_call_id = ast_node_stable_id(value);
    } else {
        const ASTNode *source = NULL;
        receipt->owned = semantic_array_storage_return_elements_owned(value, ctx, &source);
        receipt->dependency_call = source;
        receipt->dependency_call_id = ast_node_stable_id(source);
    }
    return (receipt->dependency_call == NULL) == (receipt->dependency_call_id == 0)
        && (receipt->dependency_call == NULL || receipt->dependency_call->type == AST_CALL
            || receipt->dependency_call->type == AST_ARRAY_LITERAL);
}

bool
semantic_array_storage_element_use_current(ASTNode *value,
    SemanticContext *ctx, const ArrayStorageReturnElementReceipt *receipt)
{
    if (receipt == NULL || !receipt->owned || receipt->function_id == 0
        || (receipt->dependency_call == NULL) != (receipt->dependency_call_id == 0)) return false;
    PublishedNode *row = array_storage_element_value_current(value, ctx);
    if (row == NULL || (value->type == AST_CALL && (row->caller == NULL
            || row->caller_id != receipt->function_id
            || row->caller_id != ast_node_stable_id(row->caller)))
        || !type_is_constructed_named(row->value_type, "Array")
        || !type_equals(type_get_constructed_arg(row->value_type, 0), TYPE_STRING)) return false;
    if (value->type == AST_IDENTIFIER) {
        if (receipt->binding_id == 0
            || receipt->binding_id != ast_identifier_binding_syntax_id(value)) return false;
        const PgyCollectionOwnershipFact *fact = semantic_collection_ownership_fact_find(
            ctx, receipt->function_id, receipt->binding_id);
        /* A move may retire the binding; it cannot rewrite its issued element
         * premise or dependency into an owned one after the consumer. */
        if (fact == NULL || !fact->return_elements_owned
            || fact->return_elements_source != receipt->dependency_call
            || fact->return_elements_source_id != receipt->dependency_call_id) return false;
    } else if (receipt->binding_id != 0) return false;
    else if (value->type == AST_CALL) {
        if (receipt->dependency_call != value
            || receipt->dependency_call_id != ast_node_stable_id(value)) return false;
    } else {
        const ASTNode *source = NULL;
        if (!semantic_array_storage_return_elements_owned(value, ctx, &source)
            || source != receipt->dependency_call) return false;
    }
    if (receipt->dependency_call == NULL) return true;
    if (receipt->dependency_call_id != ast_node_stable_id(receipt->dependency_call)) return false;
    if (receipt->dependency_call->type == AST_CALL)
        return semantic_array_storage_owned_string_result_current(ctx, receipt->dependency_call);
    const ASTNode *nested = NULL;
    return receipt->dependency_call->type == AST_ARRAY_LITERAL
        && semantic_array_storage_return_elements_owned(
            (ASTNode *)receipt->dependency_call, ctx, &nested) && nested == NULL;
}

bool
semantic_array_storage_payload_call_capture(SemanticContext *ctx,
    Symbol *binding, ASTNode *call, ASTNode *argument, ASTNode *callee_decl,
    size_t ordinal, uint32_t *source_id)
{
    if (source_id == NULL || ctx == NULL || ctx->current_function_decl == NULL
        || binding == NULL || binding->is_consumed || binding->decl_syntax_id == 0
        || !type_is_constructed_named(binding->type, "Array")
        || !type_equals(type_get_constructed_arg(binding->type, 0), TYPE_STRING)
        || callee_decl == NULL || callee_decl->type != AST_FUNC_DECL
        || callee_decl->is_async_decl || ast_func_body(callee_decl) == NULL) return false;
    uint32_t binding_source = binding->collection_storage_source_syntax_id != 0
        ? binding->collection_storage_source_syntax_id : binding->decl_syntax_id;
    uint32_t current_source = 0;
    size_t formal_ordinal = 0;
    if (!semantic_param_publication_selected_formal_for_actual(ctx,
            call, callee_decl, ordinal, &formal_ordinal)) return false;
    FuncParam *formal = ast_func_param(callee_decl, formal_ordinal);
    if (formal == NULL || formal->type == NULL || binding_source == UINT32_MAX
        || !type_equals(semantic_type_resolution_lookup_resolved_type(ctx, formal->type), TYPE_STRING)
        || !semantic_indexed_string_borrow_source_current(argument, ctx, &current_source)
        || current_source == 0 || current_source != binding_source) return false;
    *source_id = current_source;
    return true;
}

bool
semantic_array_storage_payload_callee_preserves(SemanticContext *ctx,
    ASTNode *argument, ASTNode *callee_decl, size_t ordinal,
    const Type *array_type, uint32_t source_id)
{
    if (ctx == NULL || argument == NULL || callee_decl == NULL
        || callee_decl->type != AST_FUNC_DECL || ordinal >= ast_func_param_count(callee_decl)
        || source_id == 0 || source_id == UINT32_MAX
        || !type_is_constructed_named(array_type, "Array")
        || !type_equals(type_get_constructed_arg(array_type, 0), TYPE_STRING)) return false;
    FuncParam *formal = ast_func_param(callee_decl, ordinal);
    uint32_t current_source = 0;
    const unsigned retaining = SLOT_PARAM_SUMMARY_WRITE | SLOT_PARAM_SUMMARY_RELEASE
        | SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE
        | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
    return formal != NULL && formal->type != NULL
        && type_equals(semantic_type_resolution_lookup_resolved_type(ctx, formal->type), TYPE_STRING)
        && semantic_indexed_string_borrow_source_current(argument, ctx, &current_source)
        && current_source == source_id
        && (function_param_flow_summary_for_param(ctx, callee_decl, ordinal) & retaining) == 0;
}

void
semantic_array_storage_initialize_return_elements(PgyCollectionOwnershipFact *fact,
    const Symbol *binding, const ASTNode *initializer, SemanticContext *ctx)
{
    if (fact == NULL) return;
    fact->return_elements_owned = false;
    fact->return_elements_source = NULL;
    fact->return_elements_source_id = 0;
    if (binding == NULL || ctx == NULL || initializer == NULL || binding->is_parameter
        || !type_is_constructed_named(binding->type, "Array")
        || !type_equals(type_get_constructed_arg(binding->type, 0), TYPE_STRING)) return;
    if (initializer->type == AST_IDENTIFIER) {
        uint32_t source_id = ast_identifier_binding_syntax_id(initializer);
        Symbol *source = scope_lookup(ctx->scope, ast_identifier_name(initializer));
        const PgyCollectionOwnershipFact *prior = semantic_collection_ownership_fact_find(ctx,
            fact->function_syntax_id, source_id);
        if (source == NULL || source->decl_syntax_id != source_id || source->is_parameter
            || !type_equals(source->type, binding->type) || prior == NULL
            || prior->disposition != PGY_COLLECTION_DISPOSITION_LIVE) return;
        fact->return_elements_owned = prior->return_elements_owned;
        fact->return_elements_source = prior->return_elements_source;
        fact->return_elements_source_id = prior->return_elements_source_id;
        return;
    }
    const ASTNode *dependency = NULL;
    fact->return_elements_owned = semantic_array_storage_return_elements_owned(
        (ASTNode *)initializer, ctx, &dependency);
    if (fact->return_elements_owned && initializer->type == AST_CALL)
        dependency = initializer;
    else if (fact->return_elements_owned && initializer->type == AST_ARRAY_LITERAL
        && ast_array_literal_count(initializer) != 0)
        dependency = initializer;
    fact->return_elements_source = dependency;
    fact->return_elements_source_id = ast_node_stable_id(dependency);
}

void
semantic_array_storage_revoke_return_elements(ASTNode *value, SemanticContext *ctx)
{
    if (value == NULL || value->type != AST_IDENTIFIER || ctx == NULL) return;
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(value));
    uint32_t id = ast_identifier_binding_syntax_id(value);
    if (binding == NULL || id == 0 || binding->decl_syntax_id != id
        || !type_is_constructed_named(binding->type, "Array")
        || !type_equals(type_get_constructed_arg(binding->type, 0), TYPE_STRING)) return;
    PgyCollectionOwnershipFact *fact = (PgyCollectionOwnershipFact *)semantic_collection_ownership_fact_find(
        ctx, ast_node_stable_id(ctx->current_function_decl), id);
    if (fact != NULL) fact->return_elements_owned = false;
}

bool
semantic_array_storage_record_return_element_effect(ASTNode *receiver,
    uint32_t kind, bool current, SemanticContext *ctx)
{
    /* A terminal drop retires the value; it does not rewrite the premise
     * captured before that consumption. Shallow mutation still revokes it. */
    if (current && kind != PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
        && kind != PGY_COLLECTION_EFFECT_DROP)
        semantic_array_storage_revoke_return_elements(receiver, ctx);
    return current;
}
