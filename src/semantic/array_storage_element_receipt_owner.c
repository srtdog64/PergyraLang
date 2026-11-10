/* Array element ownership receipts and exact-use replay. */
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
        || (value->type == AST_CALL && !publication_call_ready(ctx, row))) return NULL;
    if (semantic_param_publication_inputs_ready(ctx)) {
        SemanticPublishedValue current = semantic_param_publication_value(ctx, value);
        if (!current.ok || current.meta || !type_equals(current.type, row->value_type)) return NULL;
    }
    return row;
}

static bool
array_storage_owned_string_formal_transfer_current(ASTNode *value,
    const PublishedNode *row, SemanticContext *ctx)
{
    if (value == NULL || value->type != AST_IDENTIFIER || row == NULL
        || ctx == NULL || !type_equals(row->value_type, TYPE_STRING)
        || !row->owned_string_formal_literal_transfer)
        return false;
    uint32_t binding_id = ast_identifier_binding_syntax_id(value);
    const ASTNode *caller = row->caller;
    if (binding_id == 0 || row->binding_id != binding_id
        || caller == NULL || caller->type != AST_FUNC_DECL
        || row->caller_id == 0
        || row->caller_id != ast_node_stable_id(caller)
        || (ctx->current_function_decl != NULL
            && ctx->current_function_decl != caller))
        return false;
    for (size_t i = 0; i < ast_func_param_count(caller); i++) {
        FuncParam *formal = ast_func_param(caller, i);
        if (formal != NULL
            && ast_func_param_stable_id(formal) == binding_id)
            return formal->mode == PARAM_MODE_OWN;
    }
    return false;
}

bool
semantic_array_storage_transfer_owned_string_literal_element(
    ASTNode *value, SemanticContext *ctx)
{
    if (value == NULL || ctx == NULL || value->type != AST_IDENTIFIER)
        return true;
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(value));
    if (binding == NULL || !binding->is_parameter
        || binding->param_mode != PARAM_MODE_OWN
        || !type_equals(binding->type, TYPE_STRING))
        return true;
    uint32_t binding_id = ast_identifier_binding_syntax_id(value);
    PublishedNode *row = array_storage_element_value_current(value, ctx);
    if (binding_id == 0 || binding->decl_syntax_id != binding_id
        || binding->is_host_field || binding->is_consumed || row == NULL
        || !type_equals(row->value_type, TYPE_STRING)
        || row->binding_id != binding_id
        || row->caller == NULL || row->caller != ctx->current_function_decl
        || row->caller_id == 0
        || row->caller_id != ast_node_stable_id(row->caller)
        || binding->indexed_string_borrow_source_syntax_id != 0
        || binding->indexed_string_borrow_invalidated) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED,
            PGY_CAUSE_MOVE_FROM_RELEASED,
            PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE, value,
            "Owned String formal '%s' has no current single-transfer payload for this array literal",
            binding->name != NULL ? binding->name : "<string>");
        return false;
    }
    row->owned_string_formal_literal_transfer = true;
    binding->is_consumed = true;
    binding->is_used = true;
    return true;
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
                || (!semantic_collection_owned_string_call_result(
                        child, ctx, &producer)
                    && !array_storage_owned_string_formal_transfer_current(
                        child, element, ctx)))
                return false;
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
        const ASTNode *initializer =
            semantic_collection_owned_string_empty_local_initializer_at_call_entry(
                value, binding, ctx->current_function_decl, ctx);
        if (initializer != NULL) {
            receipt->empty_local_initializer = initializer;
            receipt->empty_local_initializer_id =
                ast_node_stable_id(initializer);
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
semantic_array_storage_transferred_element_use_current(ASTNode *value,
    SemanticContext *ctx, const ArrayStorageReturnElementReceipt *receipt)
{
    if (receipt == NULL || !receipt->owned || receipt->function_id == 0
        || (receipt->dependency_call == NULL)
            != (receipt->dependency_call_id == 0))
        return false;
    PublishedNode *row = array_storage_element_value_current(value, ctx);
    if (row == NULL || !type_is_constructed_named(row->value_type, "Array")
        || !type_equals(type_get_constructed_arg(
            row->value_type, 0), TYPE_STRING))
        return false;
    if (value->type == AST_IDENTIFIER) {
        if (receipt->binding_id == 0
            || receipt->binding_id
                != ast_identifier_binding_syntax_id(value))
            return false;
        const PgyCollectionOwnershipFact *fact =
            semantic_collection_ownership_fact_find(
                ctx, receipt->function_id, receipt->binding_id);
        /* The constructor retires the source permission after capturing it.
         * Its immutable snapshot may replay only the same dependency. */
        if (fact == NULL
            || fact->return_elements_source != receipt->dependency_call
            || fact->return_elements_source_id
                != receipt->dependency_call_id)
            return false;
    } else if (receipt->binding_id != 0) {
        return false;
    }
    if (receipt->dependency_call == NULL)
        return true;
    if (receipt->dependency_call_id
        != ast_node_stable_id(receipt->dependency_call))
        return false;
    if (receipt->dependency_call->type == AST_CALL)
        return semantic_array_storage_owned_string_result_current(
            ctx, receipt->dependency_call);
    const ASTNode *nested = NULL;
    return receipt->dependency_call->type == AST_ARRAY_LITERAL
        && semantic_array_storage_return_elements_owned(
            (ASTNode *)receipt->dependency_call, ctx, &nested)
        && nested == NULL;
}
