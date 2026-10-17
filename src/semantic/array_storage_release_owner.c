/* Public storage release admission. Element resources never belong to this
 * operation; unresolved metadata and descriptor aliases refuse release. */
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "collection_ownership_fact.h"
#include "type_checker_internal.h"
#include "type_checker_resolution_internal.h"
#include "collection_owned_element_requirement_owner.h"
#include "builtin_kind.h"
#include "diag_codes.h"
#include "slot_summary.h"
#include "array_storage_deferred_preservation_owner.h"
#include "function_param_publication_internal.h"
#include "function_param_flow_summary_internal.h"
#include "indexed_string_borrow_owner.h"

#include <string.h>

bool semantic_indexed_string_borrow_finalize(SemanticContext *ctx)
{
    if (ctx == NULL || ctx->has_error || !semantic_param_publication_ready(ctx)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    size_t issued = 0;
    for (size_t i = 0; i < store->node_capacity; i++) {
        PublishedNode *row = &store->nodes[i];
        if (!row->indexed_string_shallow_retention) continue;
        uint32_t source = UINT32_MAX;
        if (issued == SIZE_MAX || row->node == NULL || row->id == 0
            || row->id != ast_node_stable_id(row->node)
            || !semantic_indexed_string_borrow_source_current((ASTNode *)row->node, ctx, &source)
            || source != 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                (ASTNode *)row->node,
                "A shallow String store has no current independent-result proof");
            return false;
        }
        issued++;
    }
    if (issued != store->indexed_string_shallow_retention_count) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            ctx->program_root, "A shallow String store receipt was withdrawn");
        return false;
    }
    return true;
}

bool semantic_indexed_string_borrow_owner_transfer_allowed(ASTNode *receiver, SemanticContext *ctx)
{
    if (ctx == NULL || ctx->has_error) return false;
    bool tracked = false;
    uint32_t source_id = 0;
    if (!semantic_indexed_string_borrow_array_receiver_source(receiver, ctx,
            &tracked, &source_id)) return false;
    if (!tracked) return true;
    for (Scope *s = ctx->scope; s != NULL; s = s->kind == SCOPE_FUNCTION ? NULL : s->parent) {
        if (s->indexed_string_deferred_read_count > s->indexed_string_deferred_read_capacity
            || (s->indexed_string_deferred_read_count != 0 && s->indexed_string_deferred_reads == NULL))
            return semantic_array_storage_deferred_read_failure(receiver, ctx, "missing active read storage");
        for (size_t i = 0; i < s->indexed_string_deferred_read_count; i++) {
            const IndexedStringDeferredRead *row = &s->indexed_string_deferred_reads[i];
            if (!semantic_array_storage_deferred_read_current(row, ctx))
                return semantic_array_storage_deferred_read_failure(receiver, ctx, "stale deferred read identity");
            if (row->source_id == 0) continue;
            if (!row->owner_release && !row->nested && ctx->in_defer_cleanup
                && row->body == ctx->indexed_string_defer_body)
                continue; /* Earlier reads in this cleanup obey ordinary sequential use. */
            if (source_id == 0 || source_id == UINT32_MAX
                || row->source_id == UINT32_MAX || row->source_id == source_id)
                return semantic_array_storage_deferred_read_failure(receiver, ctx,
                    "a later cleanup still reads this source");
        }
    }
    return true;
}

unsigned
semantic_array_storage_pending_call_failures(SemanticContext *ctx,
    const ArrayStoragePendingCallReceipt *source)
{
    if (!semantic_array_storage_pending_call_current(ctx, source))
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    if (source->payload)
        return semantic_array_storage_payload_callee_preserves(ctx, source->argument,
            source->callee_decl, source->formal_ordinal, source->binding_type,
            source->source_id) ? 0 : FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    return function_param_flow_array_preservation_failures(
        ctx, source->callee_decl, source->formal_ordinal);
}

bool
semantic_array_storage_pending_call_elements_required(
    const ArrayStoragePendingCallReceipt *source)
{
    return source != NULL && (source->payload
        || (type_is_constructed_named(source->binding_type, "Array")
            && type_equals(type_get_constructed_arg(source->binding_type, 0), TYPE_STRING)));
}

bool
semantic_array_storage_pending_call_consumer_current(SemanticContext *ctx,
    const ArrayStoragePendingCallReceipt *source, const ASTNode *consumer,
    uint32_t consumer_id, uint32_t binding_id, size_t sequence)
{
    return source != NULL && consumer != NULL && consumer->type == AST_CALL
        && source->call == consumer && source->call_id == consumer_id
        && source->binding_id == binding_id && source->sequence < sequence
        && source->ordinal < ast_call_arg_count(source->call)
        && source->argument == ast_call_argument(source->call, source->ordinal)
        && semantic_array_storage_pending_call_current(ctx, source)
        && semantic_array_storage_pending_preserved(ctx, binding_id, sequence)
        && (!semantic_array_storage_pending_call_elements_required(source)
            || semantic_array_storage_pending_owned_elements(ctx, binding_id, sequence));
}

bool
semantic_array_storage_has_pending(const Symbol *binding)
{
    return binding != NULL && (binding->has_pending_inout_preservation
        || binding->has_pending_array_result);
}

bool
semantic_array_storage_independent_call_result(const ASTNode *call,
                                                SemanticContext *ctx)
{
    if (ctx == NULL || call == NULL || call->type != AST_CALL
        || !publication_generation_ready(ctx)) return false;
    PublishedNode *row = semantic_param_publication_inputs_ready(ctx)
        ? publication_result_current(ctx, call)
        : semantic_param_publication_pending(ctx)
            ? publication_node(ctx->function_param_publications, call, false) : NULL;
    if (row == NULL || !publication_call_ready(row) || !row->value_present
        || row->meta || row->value_shape != AST_CALL
        || !type_is_constructed_named(row->value_type, "Array")
        || row->declaration != 0 || row->value_binding != 0
        || row->nominal_result != NULL || !row->kind_present
        || row->target_shape != AST_IDENTIFIER || row->target_binding != 0
        || row->arguments == NULL)
        return false;
    bool map = row->target_name != NULL && strcmp(row->target_name, "ArrayMap") == 0;
    bool filter = row->target_name != NULL && strcmp(row->target_name, "ArrayFilter") == 0;
    bool hof = row->stdlib && row->builtin == BUILTIN_NOT_BUILTIN && (map || filter);
    if (row->argument_count != (hof ? 2u : 1u)) return false;
    for (size_t i = 0; i < row->argument_count; i++) {
        const PublishedCallArgument *arg = &row->arguments[i];
        PublishedNode *value = publication_node(ctx->function_param_publications, arg->node, false);
        if (value == NULL || !value->value_present || value->meta
            || value->id != arg->id || value->value_shape != arg->shape
            || value->value_type == NULL || value->value_type == TYPE_UNKNOWN
            || value->value_type != arg->type
            || (arg->shape == AST_IDENTIFIER
                && (arg->binding == 0 || value->binding_id != arg->binding
                    || arg->binding != ast_identifier_binding_syntax_id(arg->node)))
            || (arg->shape == AST_CALL && !publication_call_ready(value)))
            return false;
    }
    const Type *input = row->arguments[0].type;
    /* Only these exact runtime contracts allocate independent outer storage.
     * Callback results do not establish ownership of any String elements. */
    if (hof) {
        const Type *callback = row->arguments[1].type;
        const Type *element = type_get_constructed_arg(input, 0);
        return element != NULL && type_is_constructed_named(input, "Array")
            && type_equals(input, row->value_type)
            && callback->kind == TYPE_KIND_FUNCTION
            && type_function_param_count(callback) == 1
            && type_equals(type_function_param_type(callback, 0), element)
            && type_equals(type_function_return_type(callback), filter ? TYPE_BOOL : element);
    }
    if (row->builtin == BUILTIN_CLONE && row->stdlib)
        return type_equals(input, row->value_type);
    return row->builtin == BUILTIN_NOT_BUILTIN && row->stdlib
        && row->target_name != NULL && strcmp(row->target_name, "MapKeys") == 0
        && type_is_constructed_named(input, "HashMap")
        && type_equals(type_get_constructed_arg(row->value_type, 0),
            type_get_constructed_arg(input, 0));
}

bool
semantic_array_storage_independent_physical_hof_result(const ASTNode *call,
    SemanticContext *ctx)
{
    if (!semantic_array_storage_independent_call_result(call, ctx)) return false;
    PublishedNode *row = publication_result_current(ctx, call);
    if (row == NULL || row->argument_count != 2) return false;
    const PublishedCallArgument *callback = &row->arguments[1];
    if (callback->shape != AST_IDENTIFIER || callback->binding == 0) return false;
    /* The existing top-level declaration index excludes local function values
     * and captured closures; a matching function Type alone is insufficient. */
    ASTNode *decl = semantic_host_index_find_decl_by_name(ctx, AST_FUNC_DECL,
        ast_identifier_name(callback->node));
    if (decl == NULL || ast_node_stable_id(decl) != callback->binding
        || decl->is_async_decl || ast_func_body(decl) == NULL
        || !publication_routine_header_current(ctx, decl)) return false;
    const PendingRoutine *routine = publication_routine(
        ctx->function_param_publications, callback->binding);
    return routine != NULL && routine->decl == decl && routine->id == callback->binding
        && routine->function_type == callback->type && row->stdlib
        && row->builtin == BUILTIN_NOT_BUILTIN && row->target_name != NULL
        && (strcmp(row->target_name, "ArrayMap") == 0
            || strcmp(row->target_name, "ArrayFilter") == 0)
        && publication_physical_scalar(type_get_constructed_arg(row->value_type, 0));
}

/* The release/birth owner replays the exact LET result, not a later ledger's
 * inference from a matching name or a still-present pending bit. */
bool
semantic_array_storage_result_birth_current(SemanticContext *ctx,
    const ArrayStorageResultBirthReceipt *birth)
{
    if (ctx == NULL || birth == NULL || birth->caller == NULL || birth->call == NULL
        || birth->call->type != AST_CALL || birth->caller_id == 0 || birth->call_id == 0
        || birth->binding_id == 0 || birth->caller_id != ast_node_stable_id(birth->caller)
        || birth->call_id != ast_node_stable_id(birth->call)
        || !type_is_constructed_named(birth->type, "Array")
        || !semantic_param_publication_routine_inputs_current(ctx, birth->caller, true))
        return false;
    SemanticPublishedValue value = semantic_param_publication_value(ctx, birth->call);
    if (!value.ok || value.meta || !type_equals(value.type, birth->type)) return false;
    if (birth->callee_id == 0) {
        if (!semantic_array_storage_independent_call_result(birth->call, ctx)) return false;
    } else {
        const Type *result_type = NULL;
        ASTNode *callee = semantic_param_publication_declared_result(ctx, birth->call, &result_type);
        if (callee == NULL || ast_node_stable_id(callee) != birth->callee_id
            || !type_equals(result_type, birth->type)) return false;
    }
    SlotFunctionLookup lookup = {ctx, ctx->program_root};
    FunctionParamFlowSummaryStore *flow = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index;
    if (flow == NULL || !function_param_flow_program_point_index_build(flow, birth->caller, &index)
        || !function_param_flow_index_claims_current(ctx, index)) return false;
    size_t found = 0;
    for (size_t i = 0; i < index->claims->count; i++) {
        const FunctionParamFlowIndexClaim *claim = &index->claims->rows[i];
        if (claim->id != birth->binding_id) continue;
        if (claim->shape != AST_LET_DECL || claim->node == NULL
            || ast_let_initializer(claim->node) != birth->call) return false;
        found++;
    }
    return found == 1;
}

/* Recheck an issued call handoff without consulting the mutable deferral
 * ledger. Its descriptor and element verdicts remain separate there. */
bool
semantic_array_storage_pending_call_current(SemanticContext *ctx,
    const ArrayStoragePendingCallReceipt *pending)
{
    if (ctx == NULL || pending == NULL || pending->call == NULL
        || pending->call->type != AST_CALL || pending->argument == NULL
        || pending->callee_decl == NULL || pending->callee_id == 0
        || pending->call_id != ast_node_stable_id(pending->call)
        || pending->callee_id != ast_node_stable_id(pending->callee_decl)
        || pending->ordinal >= ast_call_arg_count(pending->call)
        || pending->argument != ast_call_argument(pending->call, pending->ordinal)
        || pending->argument_id != ast_node_stable_id(pending->argument)
        || (!pending->payload && (pending->argument->type != AST_IDENTIFIER
            || ast_identifier_binding_syntax_id(pending->argument) != pending->binding_id))
        || !ast_call_semantic_callee_declared_callable(pending->call)
        || ast_call_semantic_callee_decl_id(pending->call) != pending->callee_id
        || ast_call_semantic_callee_value_binding_id(pending->call) != 0
        || !semantic_param_publication_checked_argument(ctx, pending->call,
            pending->ordinal)
        || !semantic_param_publication_routine_inputs_current(ctx,
            pending->callee_decl, true))
        return false;
    size_t formal = 0;
    if (!semantic_param_publication_formal_for_actual(ctx, pending->call,
            pending->ordinal, &formal) || formal != pending->formal_ordinal ||
        pending->formal_id == 0 || pending->formal_id != ast_func_param_stable_id(
            ast_func_param(pending->callee_decl, formal))) return false;
    if (pending->payload) return true;
    SemanticPublishedValue value = semantic_param_publication_value(ctx, pending->argument);
    return value.ok && value.type != NULL
        && type_equals(value.type, pending->binding_type);
}

bool
semantic_array_storage_return_source_current(SemanticContext *ctx,
    const ArrayStorageOwnedReturnReceipt *receipt,
    ASTNode **callee_out, const Type **callee_result_out)
{
    if (callee_out == NULL || callee_result_out == NULL) return false;
    *callee_out = NULL;
    *callee_result_out = NULL;
    if (ctx == NULL || receipt == NULL || receipt->decl == NULL
        || receipt->decl->type != AST_FUNC_DECL || receipt->decl_id == 0
        || receipt->decl_id != ast_node_stable_id(receipt->decl)
        || receipt->statement == NULL || receipt->statement->type != AST_RETURN
        || receipt->statement_id == 0
        || receipt->statement_id != ast_node_stable_id(receipt->statement)
        || receipt->value == NULL || ast_return_value(receipt->statement) != receipt->value
        || receipt->value_id == 0
        || receipt->value_id != ast_node_stable_id(receipt->value)) return false;
    SemanticPublishedValue value = semantic_param_publication_value(ctx, receipt->value);
    if (!value.ok || value.meta || !type_equals(value.type, receipt->type)) return false;
    switch (receipt->source) {
    case ARRAY_RESULT_LITERAL:
        return receipt->value->type == AST_ARRAY_LITERAL
            && receipt->binding_id == 0 && receipt->callee_id == 0;
    case ARRAY_RESULT_INDEPENDENT_CALL:
        return receipt->value->type == AST_CALL && receipt->binding_id == 0
            && receipt->callee_id == 0
            && semantic_array_storage_independent_call_result(receipt->value, ctx);
    case ARRAY_RESULT_OWNED_BINDING:
        return receipt->value->type == AST_IDENTIFIER && receipt->binding_id != 0
            && receipt->callee_id == 0
            && semantic_param_publication_binding(ctx, receipt->value) == receipt->binding_id
            && semantic_array_storage_pending_preserved(ctx,
                receipt->binding_id, receipt->sequence);
    case ARRAY_RESULT_DECLARED_CALL: {
        const Type *result = NULL;
        ASTNode *callee = receipt->value->type == AST_CALL
            ? semantic_param_publication_declared_result(ctx, receipt->value, &result) : NULL;
        if (receipt->binding_id != 0 || callee == NULL || receipt->callee_id == 0
            || ast_node_stable_id(callee) != receipt->callee_id
            || !type_equals(result, receipt->type)) return false;
        *callee_out = callee;
        *callee_result_out = result;
        return true;
    }
    case ARRAY_RESULT_UNPROVED: return false;
    }
    return false;
}

static void
array_storage_invalidate_exclusivity(ASTNode *source, const Type *type,
    bool stored_alias, SemanticContext *ctx)
{
    if (ctx == NULL || source == NULL || source->type != AST_IDENTIFIER
        || !type_is_constructed_named(type, "Array")) return;
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(source));
    if (binding != NULL) {
        binding->has_exclusive_array_storage = false;
        if (stored_alias) binding->has_escaped_array_storage = true;
    }
}

void
semantic_array_storage_escape(ASTNode *source, const Type *type,
                              SemanticContext *ctx)
{
    array_storage_invalidate_exclusivity(source, type, true, ctx);
}

void
semantic_array_storage_call_argument(ASTNode *source, const Type *type,
    const Type *result_type, ParamMode mode, bool constructor,
    ASTNode *call, ASTNode *callee_decl, size_t ordinal, SemanticContext *ctx)
{
    if (ctx == NULL || ctx->has_error) return;
    if (type_equals(type, TYPE_STRING)) {
        semantic_indexed_string_borrow_call_argument(source, call, callee_decl,
            ordinal, constructor, ctx);
        return;
    }
    Symbol *binding = source != NULL && source->type == AST_IDENTIFIER && ctx != NULL
        ? scope_lookup(ctx->scope, ast_identifier_name(source)) : NULL;
    uint32_t binding_id = binding != NULL ? binding->decl_syntax_id : 0;
    bool array = type_is_constructed_named(type, "Array");
    bool string_array = array
        && type_equals(type_get_constructed_arg(type, 0), TYPE_STRING);
    bool named_descriptor = binding != NULL && binding_id != 0
        && ast_identifier_binding_syntax_id(source) == binding_id
        && type_equals(type, binding->type)
        && !binding->is_host_field && !binding->is_consumed
        && !binding->has_escaped_array_storage;
    bool pending_same_binding = binding != NULL && binding_id != 0
        && source != NULL && ast_identifier_binding_syntax_id(source) == binding_id
        && !binding->is_host_field && !binding->is_consumed
        && !binding->has_escaped_array_storage
        && semantic_array_storage_has_pending(binding)
        && semantic_array_storage_deferral_active(ctx);
    bool plain_result = type_equals(result_type, TYPE_VOID)
        || semantic_array_storage_plain_element(result_type, ctx);
    bool exact_named = call != NULL && call->type == AST_CALL
        && callee_decl != NULL && callee_decl->type == AST_FUNC_DECL
        && ast_func_body(callee_decl) != NULL && !callee_decl->is_async_decl
        && ast_node_stable_id(callee_decl) != 0
        && ast_call_semantic_callee_decl_id(call) == ast_node_stable_id(callee_decl)
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && ast_call_semantic_callee_declared_callable(call);
    /* Descriptor preservation comes from the callee body, not its result
     * shape. Release authority remains a separate, stricter decision. */
    bool exact_user_inout = mode == PARAM_MODE_MUT_REF
        && callee_decl != NULL && ast_node_stable_id(callee_decl) != 0
        && ast_call_semantic_callee_decl_id(call) == ast_node_stable_id(callee_decl)
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && array
        && (semantic_array_storage_plain_element(type_get_constructed_arg(type, 0), ctx)
            || string_array);
    bool exact_user_borrow = (mode == PARAM_MODE_REF || mode == PARAM_MODE_DEFAULT)
        && array
        && named_descriptor && exact_named;
    /* The callee's summary reads call identities that its own body check
     * records; a later declaration has none yet. Pass 2 records the handoff
     * and keeps exclusivity provisional until every body is checked. */
    bool reserved = false;
    uint32_t source_id = binding != NULL
        ? binding->collection_storage_source_syntax_id : 0;
    if (source_id == 0) source_id = binding_id;
    if (string_array && named_descriptor
        && !semantic_indexed_string_borrow_deferred_release_current(ctx,
            binding_id, source_id, &reserved)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            source, "Array handoff has no current deferred release identity");
        return;
    }
    Symbol *handoff = (exact_user_inout || exact_user_borrow)
        && binding != NULL && semantic_array_storage_deferral_active(ctx) ? binding : NULL;
    size_t formal = 0;
    bool preserving = (exact_user_inout || exact_user_borrow) && (handoff != NULL
        ? semantic_array_storage_record_pending_call(ctx, handoff, call, callee_decl, ordinal)
        : (semantic_param_publication_formal_for_actual(ctx, call, ordinal, &formal) &&
            function_param_flow_preserves_array_storage(ctx, callee_decl, formal)));
    /* The linked consumer follows this handoff, so its prefix includes the
     * callee that can outlive an already registered cleanup. */
    if (handoff != NULL && (pending_same_binding || reserved)
        && (!preserving
            || !semantic_array_storage_record_pending_call_consumer(ctx, handoff, call, ordinal))) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            source, "Array handoff has no current preservation consumer");
        return;
    }
    /* A recorded named handoff preserves only the issued premise until the
     * independent element facet is checked. It never creates an owned one. */
    if ((mode == PARAM_MODE_DEFAULT || mode == PARAM_MODE_MUT_REF)
        && !(string_array && handoff != NULL))
        semantic_array_storage_revoke_return_elements(source, ctx);
    if (constructor || (mode != PARAM_MODE_OWN && !preserving))
        /* A call without current preservation proof may retain the shallow
         * descriptor even when its result is Void. Deep cleanup must see it. */
        array_storage_invalidate_exclusivity(source, type, true, ctx);
    else if (mode != PARAM_MODE_OWN && !plain_result)
        array_storage_invalidate_exclusivity(source, type, false, ctx);
}

void
semantic_array_storage_initialize(Symbol *binding, const ASTNode *initializer,
                                  SemanticContext *ctx)
{
    if (binding == NULL || !type_is_constructed_named(binding->type, "Array"))
        return;
    /* A call birth is provisional until the return owner seals its actual
     * sources. A missing receipt never retains the old UNKNOWN permission. */
    binding->has_pending_array_result = false;
    binding->has_exclusive_array_storage = ctx->current_function_decl != NULL
        && initializer != NULL && initializer->type == AST_ARRAY_LITERAL;
    if (initializer != NULL && initializer->type == AST_ARRAY_LITERAL) {
        binding->has_escaped_array_storage = false;
    } else if (initializer != NULL && initializer->type == AST_CALL) {
        binding->has_pending_array_result = true;
        bool recorded = ctx->current_function_decl != NULL
            && semantic_array_storage_record_pending_result(ctx, binding,
                (ASTNode *)initializer);
        binding->has_exclusive_array_storage = recorded;
        binding->has_escaped_array_storage = !recorded;
    } else if (initializer != NULL && initializer->type == AST_IDENTIFIER) {
        Symbol *source = scope_lookup(ctx->scope, ast_identifier_name(initializer));
        /* Only a consumed source proves a transfer; a plain let copies the
         * descriptor. Neither path erases a prior escape. */
        binding->has_escaped_array_storage = source == NULL
            || ast_identifier_binding_syntax_id(initializer) != source->decl_syntax_id
            || source->has_escaped_array_storage || !source->is_consumed;
        binding->has_exclusive_array_storage = !binding->has_escaped_array_storage
            && source->has_exclusive_array_storage;
    }
    semantic_array_storage_escape((ASTNode *)initializer, binding->type, ctx);
}

void
semantic_array_storage_assignment(ASTNode *target, const Type *target_type,
    ASTNode *value, const Type *value_type, SemanticContext *ctx)
{
    Symbol *binding = target != NULL && target->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(target, ctx) : NULL;
    Symbol *source = value != NULL && value->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(value, ctx) : NULL;
    if (binding != NULL && binding == source)
        return;
    if (type_equals(value_type, TYPE_STRING)
        && (binding == NULL || binding->is_host_field
            || (binding->is_parameter && binding->param_mode == PARAM_MODE_MUT_REF)))
        semantic_indexed_string_borrow_call_argument(value, NULL, NULL, 0, false, ctx);
    semantic_array_storage_escape(value, value_type, ctx);
    bool alias = source != NULL || (value != NULL
        && (value->type == AST_MEMBER_ACCESS || value->type == AST_ARRAY_ACCESS));
    array_storage_invalidate_exclusivity(target, target_type, alias, ctx);
    if (binding == NULL || ctx->in_defer_cleanup
        || !type_is_constructed_named(target_type, "Array")
        || scope_lookup_current(ctx->scope, binding->name) != binding)
        return;
    uint32_t builtin = 0;
    bool fresh = value != NULL && ((value->type == AST_ARRAY_LITERAL
        && ast_array_literal_count(value) == 0)
        || (value->type == AST_CALL
            && ast_call_semantic_callee_builtin_kind(value, &builtin)
            && builtin == BUILTIN_CLONE));
    /* Only an exact independent producer at this lexical frontier replaces
     * the escaped storage. No element fact or exclusive grant is invented;
     * outer bindings written in branch/loop/defer scopes stay unproved. */
    if (fresh)
        binding->has_escaped_array_storage = false;
}

bool
semantic_array_storage_admit_drop(ASTNode *receiver, Type *array_type,
                                 SemanticContext *ctx)
{
    Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(receiver, ctx) : NULL;
    bool owns = binding != NULL && !binding->is_host_field
        && !binding->is_consumed && !binding->has_escaped_array_storage
        && binding->has_exclusive_array_storage
        && (!binding->is_parameter || binding->param_mode == PARAM_MODE_OWN);
    if (!type_is_constructed_named(array_type, "Array") || !owns) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
            receiver, "ArrayDrop requires one named, exclusive owned Array<T> binding; borrowed or aliased storage cannot be released");
        return false;
    }
    if (!semantic_array_storage_plain_element(type_get_constructed_arg(array_type, 0), ctx)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            receiver, "ArrayDrop requires plain value elements without independent resource lifetimes; use ArrayDropOwnedStrings for owned strings");
        return false;
    }
    /* Exclusivity that rests on a recorded inout handoff is decided after
     * Pass 2, which refuses this release there if a handoff is unproved. */
    if (semantic_array_storage_has_pending(binding)
        && !semantic_array_storage_record_pending_drop(ctx, receiver, binding,
            ARRAY_STORAGE_RELEASE_DROP_STORAGE)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
            receiver, "ArrayDrop requires one named, exclusive owned Array<T> binding; borrowed or aliased storage cannot be released");
        return false;
    }
    return consume_array_storage_binding(receiver, ctx);
}

bool
semantic_array_storage_admit_mutation(ASTNode *receiver, SemanticContext *ctx)
{
    Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(receiver, ctx) : NULL;
    if (binding == NULL || !binding->has_pending_array_result) return true;
    if (!binding->is_consumed && !binding->has_escaped_array_storage
        && binding->has_exclusive_array_storage
        && semantic_array_storage_record_pending_drop(ctx, receiver, binding,
            ARRAY_STORAGE_MUTATE_STORAGE)) return true;
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        receiver, "Array mutation requires current returned-storage ownership; the result's source or transfer is unproved");
    return false;
}
