/* Currentness and preservation queries for issued Array call receipts. */
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "collection_ownership_fact_internal.h"
#include "type_checker/type_checker_internal.h"
#include "type_checker/type_checker_resolution_internal.h"
#include "collection_owned_element_requirement_owner.h"
#include "indexed_string_named_call_owner.h"
#include "builtin_kind.h"
#include "diag_codes.h"
#include "slot_summary.h"
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_defer_release_owner.h"
#include "function_param_flow/function_param_publication_internal.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
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
    unsigned failures = function_param_flow_array_preservation_failures(
        ctx, source->callee_decl, source->formal_ordinal);
    if (failures != 0 && getenv("PGY_DEBUG_ARRAY_PRESERVATION") != NULL) {
        const char *callee = ast_declaration_name(source->callee_decl);
        fprintf(stderr,
            "pgy: array-preservation-unproved callee=%s formal=%zu"
            " call_line=%u call_column=%u descriptor=%d elements=%d\n",
            callee != NULL ? callee : "<unknown>", source->formal_ordinal,
            source->call != NULL ? source->call->line : 0,
            source->call != NULL ? source->call->column : 0,
            (failures & FUNCTION_PARAM_ARRAY_DESCRIPTOR_UNPROVED) != 0,
            (failures & FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED) != 0);
    }
    return failures;
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
    /* Owning the Array's elements does not prove that another argument of
     * this same call leaves those elements unchanged.  The named-call owner
     * above checks that exact non-interference condition.  When it cannot
     * admit the payload, the caller must detach the String at this boundary;
     * accepting the collection's ownership receipt here would let an inout
     * sibling replace and retire the element before the scalar is read. */
    if (source->payload)
        return false;
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
    if (row == NULL || !publication_call_ready(ctx, row) || !row->value_present
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
            || (arg->shape == AST_CALL && !publication_call_ready(ctx, value)))
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
    if (flow == NULL || !function_param_flow_program_point_index_build(flow, birth->caller, &index))
        return false;
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
        || pending->caller_decl == NULL || pending->caller_id == 0
        || pending->callee_decl == NULL || pending->callee_id == 0
        || pending->call_id != ast_node_stable_id(pending->call)
        || pending->caller_id != ast_node_stable_id(pending->caller_decl)
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
            pending->caller_decl, true)
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
    case ARRAY_RESULT_DEFAULT_FORMAL: {
        if (receipt->value->type != AST_IDENTIFIER || receipt->binding_id == 0
            || receipt->callee_id != 0
            || semantic_param_publication_binding(ctx, receipt->value)
                != receipt->binding_id)
            return false;
        size_t matches = 0;
        for (size_t i = 0; i < ast_func_param_count(receipt->decl); i++) {
            FuncParam *formal = ast_func_param(receipt->decl, i);
            if (formal != NULL
                && ast_func_param_stable_id(formal) == receipt->binding_id
                && formal->mode == PARAM_MODE_DEFAULT)
                matches++;
        }
        return matches == 1;
    }
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
