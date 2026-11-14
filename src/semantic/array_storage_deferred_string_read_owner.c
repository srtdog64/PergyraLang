/* Lexical receipts for deferred reads of borrowed String elements. The
 * storage owner stays live until cleanup; assignment events preserve the
 * exact source lineage instead of re-inferring it at release time. */
#include "array_storage_element_lifetime_owner.h"
#include "array_storage_deferred_preservation_owner.h"

#include <stdlib.h>

#include "diag_codes.h"
#include "function_param_flow/function_param_publication_internal.h"
#include "indexed_string_borrow_owner.h"
#include "type_checker/type_checker_internal.h"

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
