/* Public storage release admission. Element resources never belong to this
 * operation; unresolved metadata and descriptor aliases refuse release. */
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "collection_ownership_fact_internal.h"
#include "type_checker_internal.h"
#include "type_checker_resolution_internal.h"
#include "collection_owned_element_requirement_owner.h"
#include "indexed_string_named_call_owner.h"
#include "builtin_kind.h"
#include "diag_codes.h"
#include "slot_summary.h"
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_defer_release_owner.h"
#include "function_param_publication_internal.h"
#include "function_param_flow_summary_internal.h"
#include "indexed_string_borrow_owner.h"
#include "loan_copy_landing_owner.h"
#include "indexed_string_field_lineage_internal.h"
#include "indexed_string_nonowner_risk_internal.h"
#include "indexed_string_definition_recipe.h"
#include "indexed_string_permanent_element.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    if (source == NULL || consumer == NULL || consumer->type != AST_CALL
        || source->call != consumer || source->call_id != consumer_id
        || source->binding_id != binding_id || source->sequence >= sequence)
        return false;
    /* The consumer's own handoff of the Array is the call being admitted,
     * not a prior one; a defer that releases it is admitted at each exit.
     * A String element's handoff is judged with its own receipt, whose
     * verdict is whether the callee keeps the element. */
    size_t prefix = source->payload ? sequence : source->sequence;
    if (source->ordinal >= ast_call_arg_count(source->call)
        || source->argument != ast_call_argument(source->call, source->ordinal)
        || !semantic_array_storage_pending_call_current(ctx, source)
        || !semantic_array_storage_pending_preserved(ctx, binding_id, prefix))
        return false;
    if (!semantic_array_storage_pending_call_elements_required(source))
        return true;
    /* Earlier calls may have left borrowed elements in the array. A callee
     * that only reads or writes it in place takes no element ownership, so
     * it needs no owned-element proof; release or escape does, and so does
     * one that keeps the descriptor, as its own handoff left out above. */
    if (!source->payload && source->callee_decl != NULL
        && (semantic_array_storage_pending_call_failures(ctx, source)
            & FUNCTION_PARAM_ARRAY_DESCRIPTOR_UNPROVED) == 0) {
        unsigned effects = function_param_flow_summary_for_param(ctx,
            source->callee_decl, source->formal_ordinal);
        if ((effects & (SLOT_PARAM_SUMMARY_RELEASE | SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                | SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0)
            return true;
    }
    /* A String element read by a callee that keeps nothing and cannot touch
     * the Array is borrowed for the call only, whoever owns the element. */
    if (source->payload && source->callee_decl != NULL &&
        semantic_indexed_string_named_call_reads_only(ctx, source->call,
            source->callee_decl, source->ordinal))
        return true;
    /* One that returns it lends the element of this Array to the result. */
    PublishedNode *row = source->payload && source->callee_decl != NULL
        ? publication_result_current(ctx, source->call) : NULL;
    if (row != NULL && row->caller != NULL &&
        semantic_indexed_string_named_call_lends(ctx, (ASTNode *)row->caller, source->call,
            source->callee_decl, source->ordinal, binding_id))
        return true;
    return semantic_array_storage_pending_owned_elements(ctx, binding_id, prefix);
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
    if (ctx == NULL) return;
    if (type_equals(type, TYPE_STRING) &&
        semantic_loan_copy_landing_refusal_tail_current(ctx)) {
        semantic_indexed_string_borrow_call_argument(source, call, callee_decl,
            ordinal, constructor, ctx);
        return;
    }
    if (ctx->has_error) return;
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
    /* An own handoff in a defer cleanup takes the Array when the routine exits. */
    if (mode == PARAM_MODE_OWN && named_descriptor && ctx->in_defer_cleanup
        && !semantic_array_storage_record_defer_release(source, string_array, ctx))
        return;
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
    /* Replacing an inout aggregate field with fresh storage separates the
     * old descriptor from the caller's field, but it does not by itself
     * prove that an own callee retires that detached storage.  Keep the
     * carrier borrowed unless this exact callee/formal has a current release
     * summary.  The ordinary ownership-boundary checker then reports every
     * unsafe actual; this owner only withdraws the provisional grant. */
    if (mode == PARAM_MODE_OWN && array && binding != NULL
        && binding->array_storage_member_move_source != NULL) {
        size_t owned_formal = 0;
        bool mapped = exact_named
            && (semantic_param_publication_pending(ctx)
                ? semantic_param_publication_selected_formal_for_actual(
                    ctx, call, callee_decl, ordinal, &owned_formal)
                : semantic_param_publication_formal_for_actual(
                    ctx, call, ordinal, &owned_formal));
        bool releases = mapped
            && semantic_collection_owned_parameter_releases_storage(
                ctx, callee_decl, owned_formal);
        /* Publication is sealed only after every routine body has been
         * checked.  An exact selected formal may therefore lack its release
         * summary solely because that body appears later in source order.
         * Keep the exact replacement grant provisional; the owned-element
         * finalizer validates the release summary and fails closed. */
        bool release_check_deferred = mapped
            && semantic_param_publication_pending(ctx);
        if (!releases && !release_check_deferred) {
            binding->has_exclusive_array_storage = false;
            binding->has_escaped_array_storage = true;
        }
    }
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
    /* The linked consumer follows this handoff; its prefix holds the
     * earlier calls. A defer release is admitted again at each exit. */
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
    } else if (initializer != NULL && initializer->type == AST_MEMBER_ACCESS
        && !collection_array_string_type(binding->type)) {
        PgyCollectionMemberPath path;
        if (collection_member_move_path_identity(initializer, ctx, &path)) {
            Symbol *root = scope_lookup(ctx->scope,
                ast_identifier_name(path.root));
            if (root != NULL && root->is_parameter
                && root->param_mode == PARAM_MODE_MUT_REF
                && root->decl_syntax_id == path.root_binding_id) {
                binding->array_storage_member_move_source = initializer;
                binding->array_storage_member_move_source_id =
                    ast_node_stable_id(initializer);
                binding->array_storage_member_move_root_binding_id =
                    path.root_binding_id;
                binding->array_storage_member_move_root_birth_serial =
                    root->birth_serial;
            }
        }
    }
    semantic_array_storage_escape((ASTNode *)initializer, binding->type, ctx);
}

static bool
array_storage_member_move_root_current(const Symbol *binding,
                                       SemanticContext *ctx)
{
    if (binding == NULL || ctx == NULL
        || binding->array_storage_member_move_source == NULL
        || binding->array_storage_member_move_source_id == 0
        || binding->array_storage_member_move_root_binding_id == 0)
        return false;
    PgyCollectionMemberPath path;
    if (!collection_member_move_path_identity(
            binding->array_storage_member_move_source, ctx, &path)
        || path.root_binding_id !=
            binding->array_storage_member_move_root_binding_id)
        return false;
    Symbol *root = scope_lookup(ctx->scope,
        ast_identifier_name(path.root));
    return root != NULL && root->is_parameter
        && root->param_mode == PARAM_MODE_MUT_REF
        && root->decl_syntax_id == path.root_binding_id
        && (binding->array_storage_member_move_root_birth_serial == 0
            || root->birth_serial == 0
            || binding->array_storage_member_move_root_birth_serial
                == root->birth_serial);
}

void
semantic_array_storage_prepare_member_replacement(ASTNode *target,
    ASTNode *value, SemanticContext *ctx)
{
    if (ctx == NULL || target == NULL || target->type != AST_MEMBER_ACCESS
        || value == NULL || value->type != AST_ARRAY_LITERAL
        || ast_array_literal_count(value) != 0)
        return;
    for (Scope *scope = ctx->scope; scope != NULL;
         scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *binding = scope->symbols[i];
            if (binding == NULL || binding->is_consumed
                || !type_is_constructed_named(binding->type, "Array")
                || !array_storage_member_move_root_current(binding, ctx)
                || !collection_member_move_path_matches_source(
                    binding->array_storage_member_move_source,
                    binding->array_storage_member_move_source_id,
                    binding->array_storage_member_move_root_binding_id,
                    target, ctx, false))
                continue;
            /* The old backing now has one local carrier and the inout field
             * has a distinct empty backing. Branch joins retain this grant
             * only when every live path performed the exact replacement. */
            binding->has_escaped_array_storage = false;
            binding->has_exclusive_array_storage = true;
        }
    }
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
    if (getenv("PGY_DEBUG_FUNCTION_PARAM_FLOW") != NULL) {
        const char *routine = ctx->current_function_decl == NULL ? NULL
            : ast_declaration_name(ctx->current_function_decl);
        fprintf(stderr,
            "pgy: array-mutation-storage-unproved routine=%s receiver=%s"
            " line=%u column=%u consumed=%d escaped=%d exclusive=%d\n",
            routine != NULL ? routine : "<unknown>",
            binding != NULL && binding->name != NULL ? binding->name : "<unknown>",
            receiver != NULL ? receiver->line : 0,
            receiver != NULL ? receiver->column : 0,
            binding != NULL && binding->is_consumed,
            binding != NULL && binding->has_escaped_array_storage,
            binding != NULL && binding->has_exclusive_array_storage);
    }
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        receiver, "Array mutation requires current returned-storage ownership; the result's source or transfer is unproved");
    return false;
}
