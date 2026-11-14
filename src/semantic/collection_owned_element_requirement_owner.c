/* Exact owned String-element entry obligations for the native bootstrap.
 * Typed operations record seeds, forwarding edges and caller snapshots once.
 * Finalization after Pass 2 makes source order irrelevant without rescanning
 * callee bodies or turning unknown provenance into ownership permission. */
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "indexed_string_borrow_owner.h"
#include "collection_owned_element_requirement_owner.h"
#include "collection_owned_element_requirement_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "collection_ownership_fact.h"
#include "collection_ownership_fact_internal.h"
#include "default_aggregate_move_rule.h"
#include "diag_codes.h"
#include "formal_string_store_landing_owner.h"
#include "function_param_flow/function_param_publication_owner.h"
#include "function_param_flow/function_param_publication_internal.h"
#include "function_param_flow/function_param_flow_loan_order.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "loan_copy_landing_owner.h"
#include "type_checker/type_checker_internal.h"
#include "type_checker/type_checker_resolution_metadata_internal.h"
#include "../parser/ast_api.h"

bool
collection_owned_requirement_failure(
    SemanticContext *ctx, ASTNode *site, const char *detail)
{
    if (ctx != NULL)
        semantic_error(ctx, site,
            "Owned String-element entry requirements failed closed: %s",
            detail);
    return false;
}

bool
collection_owned_is_string_array(const Type *type)
{
    Type *inner;
    if (!type_is_constructed_named(type, "Array"))
        return false;
    inner = type_get_constructed_arg(type, 0);
    return inner != NULL && type_equals(inner, TYPE_STRING);
}

void *
collection_owned_requirement_rows_reserve(
    void *rows, size_t *capacity, size_t needed, size_t row_size)
{
    size_t next;
    void *grown;
    if (needed <= *capacity)
        return rows;
    next = *capacity == 0 ? 8 : *capacity;
    while (next < needed) {
        if (next > SIZE_MAX / 2)
            return NULL;
        next *= 2;
    }
    if (next > SIZE_MAX / row_size)
        return NULL;
    grown = realloc(rows, next * row_size);
    if (grown == NULL)
        return NULL;
    *capacity = next;
    return grown;
}

bool
collection_owned_requirement_parameter_row(
    CollectionOwnedElementRequirementStore *store,
    uint32_t function_id, uint32_t parameter_id, size_t *row_out)
{
    OwnedElementParameter *grown;
    if (function_id == 0 || parameter_id == 0)
        return false;
    for (size_t i = 0; i < store->parameter_count; i++) {
        if (store->parameters[i].function_id == function_id
            && store->parameters[i].parameter_id == parameter_id) {
            *row_out = i;
            return true;
        }
    }
    if (store->parameter_count == SIZE_MAX)
        return false;
    grown = collection_owned_requirement_rows_reserve(store->parameters,
        &store->parameter_capacity, store->parameter_count + 1,
        sizeof(*store->parameters));
    if (grown == NULL)
        return false;
    store->parameters = grown;
    *row_out = store->parameter_count++;
    store->parameters[*row_out] = (OwnedElementParameter){
        .function_id = function_id,
        .parameter_id = parameter_id};
    return true;
}

static bool
collection_owned_requirement_bind_parameter_identity(
    CollectionOwnedElementRequirementStore *store, size_t row,
    ASTNode *function, FuncParam *parameter, size_t formal_ordinal)
{
    if (store == NULL || row >= store->parameter_count
        || function == NULL || function->type != AST_FUNC_DECL
        || parameter == NULL
        || formal_ordinal >= ast_func_param_count(function)
        || ast_func_param(function, formal_ordinal) != parameter
        || ast_node_stable_id(function) == 0
        || ast_node_stable_id(function) != store->parameters[row].function_id
        || ast_func_param_stable_id(parameter) == 0
        || ast_func_param_stable_id(parameter)
            != store->parameters[row].parameter_id)
        return false;
    OwnedElementParameter *owned = &store->parameters[row];
    if (owned->identity_present)
        return owned->function == function
            && owned->parameter == parameter
            && owned->formal_ordinal == formal_ordinal
            && owned->parameter_mode == parameter->mode;
    owned->function = function;
    owned->parameter = parameter;
    owned->formal_ordinal = formal_ordinal;
    owned->parameter_mode = parameter->mode;
    owned->identity_present = true;
    return true;
}

static FuncParam *
formal_for_binding(ASTNode *function, uint32_t binding_id, ParamMode mode)
{
    if (function == NULL || function->type != AST_FUNC_DECL
        || binding_id == 0)
        return NULL;
    for (size_t i = 0; i < ast_func_param_count(function); i++) {
        FuncParam *parameter = ast_func_param(function, i);
        if (parameter != NULL && parameter->mode == mode
            && ast_func_param_stable_id(parameter) == binding_id)
            return parameter;
    }
    return NULL;
}

bool
semantic_collection_owned_element_parameter_carrier_current(
    ASTNode *value, ASTNode *function_decl, SemanticContext *ctx,
    size_t *formal_index_out, ParamMode *formal_mode_out,
    bool *parameter_carrier_out)
{
    if (formal_index_out != NULL) *formal_index_out = SIZE_MAX;
    if (formal_mode_out != NULL) *formal_mode_out = PARAM_MODE_DEFAULT;
    if (parameter_carrier_out != NULL) *parameter_carrier_out = false;
    if (value == NULL || value->type != AST_IDENTIFIER || ctx == NULL
        || function_decl == NULL || function_decl->type != AST_FUNC_DECL)
        return false;

    FunctionParamPublicationStore *publication =
        ctx->function_param_publications;
    ASTNode *cursor = value;
    size_t limit = publication != NULL ? publication->node_count : 0;
    for (size_t depth = 0; depth <= limit; depth++) {
        const char *name = ast_identifier_name(cursor);
        Symbol *binding = name != NULL ? scope_lookup(ctx->scope, name) : NULL;
        uint32_t binding_id = ast_identifier_binding_syntax_id(cursor);
        if (binding == NULL || binding_id == 0
            || binding->decl_syntax_id != binding_id
            || !collection_owned_is_string_array(binding->type))
            return false;

        size_t matched = SIZE_MAX;
        for (size_t i = 0; i < ast_func_param_count(function_decl); i++) {
            FuncParam *formal = ast_func_param(function_decl, i);
            if (formal == NULL
                || ast_func_param_stable_id(formal) != binding_id)
                continue;
            if (matched != SIZE_MAX)
                return false;
            matched = i;
        }
        if (matched != SIZE_MAX) {
            FuncParam *formal = ast_func_param(function_decl, matched);
            if (parameter_carrier_out != NULL)
                *parameter_carrier_out = true;
            if (!binding->is_parameter || binding->is_consumed
                || formal == NULL || binding->param_mode != formal->mode
                || (formal->mode != PARAM_MODE_DEFAULT
                    && formal->mode != PARAM_MODE_OWN
                    && formal->mode != PARAM_MODE_MUT_REF))
                return false;
            if (formal_index_out != NULL) *formal_index_out = matched;
            if (formal_mode_out != NULL) *formal_mode_out = formal->mode;
            return true;
        }

        if (publication == NULL || !semantic_param_publication_pending(ctx)
            || binding->is_parameter || binding->is_host_field
            || binding->is_consumed)
            return false;
        PublishedNode *definition = publication_node_by_id(
            publication, binding_id);
        ASTNode *declaration = definition != NULL
            ? (ASTNode *)definition->node : NULL;
        ASTNode *initializer = declaration != NULL
            && declaration->type == AST_LET_DECL
            ? ast_let_initializer(declaration) : NULL;
        PublishedNode *value_row = publication_node(
            publication, cursor, false);
        PublishedNode *initializer_row = initializer != NULL
            ? publication_node(publication, initializer, false) : NULL;
        uint32_t function_id = ast_node_stable_id(function_decl);
        if (declaration == NULL
            || ast_node_stable_id(declaration) != binding_id
            || ast_let_is_alias(declaration)
            || initializer == NULL || initializer->type != AST_IDENTIFIER
            || value_row == NULL || value_row->node != cursor
            || value_row->id != ast_node_stable_id(cursor)
            || !value_row->value_present || value_row->meta
            || value_row->binding_id != binding_id
            || value_row->caller != function_decl
            || value_row->caller_id != function_id
            || !type_equals(value_row->value_type, binding->type)
            || initializer_row == NULL
            || initializer_row->node != initializer
            || initializer_row->id != ast_node_stable_id(initializer)
            || !initializer_row->value_present || initializer_row->meta
            || initializer_row->binding_id == 0
            || initializer_row->binding_id
                != ast_identifier_binding_syntax_id(initializer)
            || initializer_row->caller != function_decl
            || initializer_row->caller_id != function_id
            || !type_equals(initializer_row->value_type, binding->type)
            /* The indexed-storage owner joins actual replacements. Permission
             * to replace (`var`) does not invalidate an unchanged descriptor;
             * an unknown or different generation cannot follow its birth. */
            || !initializer_row->indexed_array_source_present
            || initializer_row->indexed_array_source_id == 0
            || initializer_row->indexed_array_source_id == UINT32_MAX
            || binding->collection_storage_source_syntax_id
                != initializer_row->indexed_array_source_id)
            return false;
        cursor = initializer;
    }
    return false;
}

static FuncParam *
own_formal_for_binding(ASTNode *function, uint32_t binding_id)
{
    return formal_for_binding(function, binding_id, PARAM_MODE_OWN);
}

void
semantic_collection_owned_element_requirements_destroy(
    CollectionOwnedElementRequirementStore *store)
{
    if (store == NULL)
        return;
    free(store->parameters);
    free(store->edges);
    free(store->actuals);
    free(store->member_reads);
    free(store->member_pushes);
    free(store->aggregate_actuals);
    free(store->aggregate_returns);
    free(store->aggregate_requirement_memos);
    free(store->aggregate_assignment_rows);
    free(store->aggregate_assignment_buckets);
    free(store->aggregate_return_next_plus_one);
    free(store->aggregate_return_buckets);
    free(store->aggregate_actual_next_plus_one);
    free(store->aggregate_actual_buckets);
    free(store->aggregate_proof_memos);
    free(store->aggregate_proof_memo_buckets);
    free(store);
}

bool
collection_owned_require_prior_member_reads(
    CollectionOwnedElementRequirementStore *store,
    uint32_t source_function_id, uint32_t source_formal_id,
    const ASTNode *member_source, uint32_t member_source_id,
    uint32_t root_binding_id, uint32_t leaf_field_id,
    uint64_t source_birth_serial, const uint32_t *field_ids,
    size_t field_count)
{
    if (store == NULL || source_function_id == 0 || source_formal_id == 0
        || member_source == NULL || member_source_id == 0
        || ast_node_stable_id(member_source) != member_source_id
        || root_binding_id == 0 || leaf_field_id == 0
        || source_birth_serial == 0 || field_ids == NULL
        || field_count == 0
        || field_count > PGY_COLLECTION_MEMBER_PATH_LIMIT)
        return false;
    for (size_t i = 0; i < store->member_read_count; i++) {
        OwnedElementMemberRead *read = &store->member_reads[i];
        if (read->source_function_id != source_function_id
            || read->source_formal_id != source_formal_id
            || read->member_source != member_source
            || read->member_source_id != member_source_id
            || read->root_binding_id != root_binding_id
            || read->leaf_field_id != leaf_field_id
            || read->source_birth_serial != source_birth_serial
            || read->field_count != field_count
            || memcmp(read->field_ids, field_ids,
                field_count * sizeof(*field_ids)) != 0)
            continue;
        read->required = true;
    }
    return true;
}

bool
semantic_collection_owned_element_requirement_record_member_read(
    ASTNode *call, ASTNode *callee_decl, size_t argument_index,
    ASTNode *argument, ParamMode parameter_mode, SemanticContext *ctx)
{
    CollectionOwnedElementRequirementStore *store = ctx != NULL
        ? ctx->collection_owned_element_requirements : NULL;
    ASTNode *source_function = ctx != NULL
        ? ctx->current_function_decl : NULL;
    Symbol *binding = ctx != NULL && argument != NULL
        && argument->type == AST_IDENTIFIER
        ? scope_lookup(ctx->scope, ast_identifier_name(argument)) : NULL;
    uint32_t binding_id = argument != NULL
        ? ast_identifier_binding_syntax_id(argument) : 0;
    const PgyCollectionOwnershipFact *fact = binding != NULL
        && source_function != NULL
        ? semantic_collection_ownership_fact_find(ctx,
            ast_node_stable_id(source_function), binding_id) : NULL;
    size_t target_formal = SIZE_MAX;
    size_t source_formal = SIZE_MAX;
    ParamMode source_mode = PARAM_MODE_DEFAULT;
    uint32_t field_ids[PGY_COLLECTION_MEMBER_PATH_LIMIT] = {0};
    size_t field_count = 0;
    bool selected = ctx != NULL && call != NULL && callee_decl != NULL
        && semantic_param_publication_selected_formal_for_actual(
            ctx, call, callee_decl, argument_index, &target_formal);
    bool member_current = fact != NULL && binding != NULL
        && collection_member_move_call_entry_parameter_path(
            fact, binding, ctx, &source_formal, &source_mode,
            field_ids, &field_count);
    FuncParam *target_parameter = selected && callee_decl != NULL
        && target_formal < ast_func_param_count(callee_decl)
        ? ast_func_param(callee_decl, target_formal) : NULL;
    FuncParam *source_parameter = source_function != NULL
        && source_formal < ast_func_param_count(source_function)
        ? ast_func_param(source_function, source_formal) : NULL;

    if (ctx == NULL || store == NULL || store->program != ctx->program_root
        || (parameter_mode != PARAM_MODE_DEFAULT
            && parameter_mode != PARAM_MODE_REF)
        || call == NULL || call->type != AST_CALL
        || callee_decl == NULL || callee_decl->type != AST_FUNC_DECL
        || argument == NULL || argument->type != AST_IDENTIFIER
        || argument_index >= ast_call_arg_count(call)
        || ast_call_argument(call, argument_index) != argument
        || ast_node_stable_id(call) == 0
        || ast_node_stable_id(callee_decl) == 0
        || ast_call_semantic_callee_decl_id(call)
            != ast_node_stable_id(callee_decl)
        || ast_call_semantic_callee_value_binding_id(call) != 0
        || !ast_call_semantic_callee_declared_callable(call)
        || ast_call_semantic_callee_is_stdlib(call)
        || binding == NULL || binding_id == 0
        || binding->decl_syntax_id != binding_id
        || !selected || target_parameter == NULL
        || target_parameter->mode != parameter_mode
        || ast_func_param_stable_id(target_parameter) == 0
        || source_function == NULL
        || source_function->type != AST_FUNC_DECL
        || ast_node_stable_id(source_function) == 0
        || !member_current || source_parameter == NULL
        || source_parameter->mode != source_mode
        || ast_func_param_stable_id(source_parameter) == 0
        || fact == NULL || fact->member_move_source == NULL
        || fact->member_move_source_id == 0
        || ast_node_stable_id(fact->member_move_source)
            != fact->member_move_source_id
        || fact->source_binding_syntax_id == 0
        || fact->origin_syntax_id == 0
        || fact->source_birth_serial == 0
        || field_count == 0
        || field_count > PGY_COLLECTION_MEMBER_PATH_LIMIT)
        return collection_owned_requirement_failure(ctx, call,
            "missing moved-member declared-read identity");
    for (size_t i = 0; i < store->member_read_count; i++) {
        OwnedElementMemberRead *read = &store->member_reads[i];
        if (read->call != call || read->actual_ordinal != argument_index)
            continue;
        bool same = read->call_id == ast_node_stable_id(call)
            && read->callee_decl == callee_decl
            && read->callee_id == ast_node_stable_id(callee_decl)
            && read->argument == argument
            && read->argument_id == ast_node_stable_id(argument)
            && read->argument_binding_id == binding_id
            && read->target_formal_ordinal == target_formal
            && read->target_formal_id
                == ast_func_param_stable_id(target_parameter)
            && read->target_mode == parameter_mode
            && read->source_function == source_function
            && read->source_function_id
                == ast_node_stable_id(source_function)
            && read->source_formal_ordinal == source_formal
            && read->source_formal_id
                == ast_func_param_stable_id(source_parameter)
            && read->source_mode == source_mode
            && read->member_source == fact->member_move_source
            && read->member_source_id == fact->member_move_source_id
            && read->root_binding_id == fact->source_binding_syntax_id
            && read->leaf_field_id == fact->origin_syntax_id
            && read->source_birth_serial == fact->source_birth_serial
            && read->field_count == field_count
            && memcmp(read->field_ids, field_ids,
                field_count * sizeof(*field_ids)) == 0;
        if (!same)
            return collection_owned_requirement_failure(ctx, call,
                "moved-member declared-read identity changed");
        return true;
    }
    OwnedElementMemberRead *grown = collection_owned_requirement_rows_reserve(
        store->member_reads, &store->member_read_capacity,
        store->member_read_count + 1, sizeof(*store->member_reads));
    if (grown == NULL)
        return collection_owned_requirement_failure(ctx, call,
            "moved-member declared-read allocation failed");
    store->member_reads = grown;
    store->member_reads[store->member_read_count++] =
        (OwnedElementMemberRead){
            .call = call,
            .callee_decl = callee_decl,
            .argument = argument,
            .source_function = source_function,
            .member_source = fact->member_move_source,
            .call_id = ast_node_stable_id(call),
            .callee_id = ast_node_stable_id(callee_decl),
            .argument_id = ast_node_stable_id(argument),
            .argument_binding_id = binding_id,
            .target_formal_id = ast_func_param_stable_id(target_parameter),
            .source_function_id = ast_node_stable_id(source_function),
            .source_formal_id = ast_func_param_stable_id(source_parameter),
            .member_source_id = fact->member_move_source_id,
            .root_binding_id = fact->source_binding_syntax_id,
            .leaf_field_id = fact->origin_syntax_id,
            .source_birth_serial = fact->source_birth_serial,
            .actual_ordinal = argument_index,
            .target_formal_ordinal = target_formal,
            .source_formal_ordinal = source_formal,
            .field_count = field_count,
            .target_mode = parameter_mode,
            .source_mode = source_mode};
    memcpy(store->member_reads[store->member_read_count - 1].field_ids,
        field_ids, field_count * sizeof(*field_ids));
    return true;
}

bool
semantic_collection_owned_element_requirements_begin(
    ASTNode *program, SemanticContext *ctx)
{
    if (ctx == NULL || program == NULL || program->type != AST_PROGRAM
        || ast_node_stable_id(program) == 0)
        return collection_owned_requirement_failure(ctx, program, "missing program identity");
    semantic_collection_owned_element_requirements_destroy(
        ctx->collection_owned_element_requirements);
    ctx->collection_owned_element_requirements = calloc(1,
        sizeof(*ctx->collection_owned_element_requirements));
    if (ctx->collection_owned_element_requirements == NULL)
        return collection_owned_requirement_failure(ctx, program, "store allocation failed");
    ctx->collection_owned_element_requirements->program = program;
    return true;
}

bool
semantic_collection_owned_element_requirement_record_deep_drop(
    ASTNode *call, ASTNode *receiver, const Type *array_type,
    SemanticContext *ctx)
{
    Symbol *binding;
    uint32_t binding_id;
    size_t row;
    CollectionOwnedElementRequirementStore *store;

    /* Only the resolved stdlib deep-drop branch calls this entrypoint. A
     * same-spelling source function never reaches this semantic operation. */
    if (ctx == NULL || receiver == NULL
        || receiver->type != AST_IDENTIFIER || !collection_owned_is_string_array(array_type))
        return collection_owned_requirement_failure(ctx, call, "missing typed deep-drop receiver");
    binding = scope_lookup(ctx->scope, ast_identifier_name(receiver));
    if (binding == NULL)
        return collection_owned_requirement_failure(ctx, call, "missing deep-drop receiver binding");
    if (!binding->is_parameter
        || binding->param_mode != PARAM_MODE_OWN)
        return true;
    binding_id = ast_identifier_binding_syntax_id(receiver);
    store = ctx->collection_owned_element_requirements;
    if (store == NULL || store->program != ctx->program_root
        || call == NULL || call->type != AST_CALL
        || ast_node_stable_id(call) == 0
        || ast_call_semantic_callee_decl_id(call) != 0
        || ast_call_semantic_callee_declared_callable(call)
        || ast_call_semantic_callee_value_binding_id(call) != 0
        || binding_id == 0 || binding_id != binding->decl_syntax_id
        || own_formal_for_binding(ctx->current_function_decl, binding_id)
            == NULL)
        return collection_owned_requirement_failure(ctx, call, "missing deep-drop formal identity");
    if (!collection_owned_requirement_parameter_row(store,
            ast_node_stable_id(ctx->current_function_decl), binding_id, &row))
        return collection_owned_requirement_failure(ctx, call, "seed allocation or identity failed");
    store->parameters[row].requirements |= 3u;
    return true;
}

bool
semantic_collection_owned_element_requirement_record_storage_drop(
    ASTNode *call, ASTNode *receiver, SemanticContext *ctx)
{
    Symbol *binding = lookup_identifier_symbol(receiver, ctx);
    if (binding == NULL)
        return collection_owned_requirement_failure(ctx, call, "missing ArrayDrop binding identity");
    if (!binding->is_parameter)
        return true;
    CollectionOwnedElementRequirementStore *store = ctx->collection_owned_element_requirements;
    size_t row;
    if (store == NULL || binding->param_mode != PARAM_MODE_OWN
        || own_formal_for_binding(ctx->current_function_decl, binding->decl_syntax_id) == NULL
        || !collection_owned_requirement_parameter_row(store, ast_node_stable_id(ctx->current_function_decl),
            binding->decl_syntax_id, &row))
        return collection_owned_requirement_failure(ctx, call, "missing ArrayDrop own-formal identity");
    store->parameters[row].requirements |= 2u;
    return true;
}

bool
semantic_collection_owned_element_requirement_record_parameter_mutation(
    ASTNode *call, ASTNode *receiver, ASTNode *function_decl,
    size_t formal_index, SemanticContext *ctx)
{
    CollectionOwnedElementRequirementStore *store = ctx != NULL
        ? ctx->collection_owned_element_requirements : NULL;
    FuncParam *formal = function_decl != NULL
        && function_decl->type == AST_FUNC_DECL
        && formal_index < ast_func_param_count(function_decl)
        ? ast_func_param(function_decl, formal_index) : NULL;
    uint32_t function_id = function_decl != NULL
        ? ast_node_stable_id(function_decl) : 0;
    uint32_t formal_id = formal != NULL
        ? ast_func_param_stable_id(formal) : 0;
    size_t carrier_formal = SIZE_MAX;
    ParamMode carrier_mode = PARAM_MODE_DEFAULT;
    bool parameter_carrier = false;
    bool carrier_current = false;
    size_t row;

    if (semantic_param_publication_pending(ctx)) {
        carrier_current =
            semantic_collection_owned_element_parameter_carrier_current(
                receiver, function_decl, ctx, &carrier_formal, &carrier_mode,
                &parameter_carrier);
    } else if (formal != NULL && semantic_param_publication_inputs_ready(ctx)) {
        carrier_current =
            semantic_param_publication_array_parameter_value_current(
                ctx, receiver, function_decl, formal_index, formal->mode);
        if (carrier_current) {
            carrier_formal = formal_index;
            carrier_mode = formal->mode;
            parameter_carrier = true;
        }
    }

    if (store == NULL || store->program != ctx->program_root
        || call == NULL || call->type != AST_CALL
        || ast_node_stable_id(call) == 0
        || function_id == 0 || formal == NULL
        || (formal->mode != PARAM_MODE_OWN
            && formal->mode != PARAM_MODE_MUT_REF
            && formal->mode != PARAM_MODE_DEFAULT) || formal_id == 0
        || !carrier_current
        || !parameter_carrier || carrier_formal != formal_index
        || carrier_mode != formal->mode)
        return collection_owned_requirement_failure(ctx, call,
            "missing owned-mutation formal identity");
    if (!collection_owned_requirement_parameter_row(store,
            function_id, formal_id, &row))
        return collection_owned_requirement_failure(ctx, call,
            "owned-mutation requirement allocation failed");
    if (!collection_owned_requirement_bind_parameter_identity(
            store, row, function_decl, formal, formal_index))
        return collection_owned_requirement_failure(ctx, call,
            "owned-mutation formal identity changed");
    store->parameters[row].requirements |= 1u;
    return true;
}

bool
semantic_collection_owned_inout_actuals_issued(
    SemanticContext *ctx, ASTNode *function_decl, size_t formal_index)
{
    CollectionOwnedElementRequirementStore *store = ctx != NULL
        ? ctx->collection_owned_element_requirements : NULL;
    FuncParam *formal = function_decl != NULL
        && function_decl->type == AST_FUNC_DECL
        && formal_index < ast_func_param_count(function_decl)
        ? ast_func_param(function_decl, formal_index) : NULL;
    uint32_t function_id = function_decl != NULL
        ? ast_node_stable_id(function_decl) : 0;
    uint32_t formal_id = formal != NULL
        ? ast_func_param_stable_id(formal) : 0;
    size_t target = SIZE_MAX;
    bool concrete = false;

    if (store == NULL || store->program != ctx->program_root
        || formal == NULL || formal->mode != PARAM_MODE_MUT_REF
        || function_id == 0 || formal_id == 0
        || !collection_owned_is_string_array(
            semantic_type_resolution_lookup_resolved_type(ctx, formal->type)))
        return false;
    for (size_t i = 0; i < store->parameter_count; i++) {
        if (store->parameters[i].function_id != function_id
            || store->parameters[i].parameter_id != formal_id)
            continue;
        if (target != SIZE_MAX)
            return false;
        target = i;
    }
    if (target == SIZE_MAX)
        return false;
    for (size_t i = 0; i < store->actual_count; i++) {
        const OwnedElementActualSnapshot *actual = &store->actuals[i];
        if (actual->target != target
            || actual->parameter_mode != PARAM_MODE_MUT_REF)
            continue;
        if (actual->forwarded_formal)
            continue;
        concrete = true;
        if (actual->argument == NULL || actual->argument_id == 0
            || ast_node_stable_id(actual->argument) != actual->argument_id
            || !actual->element_use.owned
            || actual->element_use.function_id == 0
            || ((actual->element_use.dependency_call == NULL)
                != (actual->element_use.dependency_call_id == 0))
            || (actual->element_use.dependency_call != NULL
                && ast_node_stable_id(actual->element_use.dependency_call)
                    != actual->element_use.dependency_call_id)
            || actual->element_ownership ==
                PGY_STRING_ARRAY_BORROWED_ELEMENTS)
            return false;
    }
    return concrete;
}

bool
semantic_collection_owned_parameter_releases_storage(
    SemanticContext *ctx, ASTNode *function_decl, size_t formal_index)
{
    CollectionOwnedElementRequirementStore *store = ctx != NULL
        ? ctx->collection_owned_element_requirements : NULL;
    FuncParam *formal = function_decl != NULL
        && function_decl->type == AST_FUNC_DECL
        && formal_index < ast_func_param_count(function_decl)
        ? ast_func_param(function_decl, formal_index) : NULL;
    uint32_t function_id = function_decl != NULL
        ? ast_node_stable_id(function_decl) : 0;
    uint32_t parameter_id = formal != NULL
        ? ast_func_param_stable_id(formal) : 0;
    if (store == NULL || store->program != ctx->program_root
        || function_id == 0 || parameter_id == 0
        || formal->mode != PARAM_MODE_OWN)
        return false;
    for (size_t i = 0; i < store->parameter_count; i++) {
        const OwnedElementParameter *row = &store->parameters[i];
        if (row->function_id == function_id
            && row->parameter_id == parameter_id)
            return (row->requirements & 2u) != 0;
    }
    return false;
}

bool
semantic_collection_owned_element_requirement_record_argument(
    ASTNode *call, ASTNode *callee_decl, size_t argument_index, size_t formal_index,
    ASTNode *argument, const Type *argument_type, ParamMode parameter_mode,
    SemanticContext *ctx)
{
    CollectionOwnedElementRequirementStore *store;
    FuncParam *target_parameter;
    uint32_t function_id;
    uint32_t caller_id;
    uint32_t binding_id = 0;
    size_t target;
    PgyStringArrayOwnership ownership = PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN;

    bool exclusive_storage = false;
    uint32_t pending_binding_id = 0;
    size_t pending_sequence = 0;
    ArrayStorageReturnElementReceipt element_use = {0};
    bool forwarded_formal = false;
    bool detached_member_requires_release = false;
    ASTNode *aggregate_member_function = NULL;
    const ASTNode *aggregate_member_source = NULL;
    uint32_t aggregate_member_function_id = 0;
    uint32_t aggregate_member_formal_id = 0;
    uint32_t aggregate_member_source_id = 0;
    uint32_t aggregate_member_source_binding_id = 0;
    uint32_t aggregate_member_leaf_field_id = 0;
    uint64_t aggregate_member_source_birth_serial = 0;
    uint32_t aggregate_member_field_ids[PGY_COLLECTION_MEMBER_PATH_LIMIT] = {0};
    size_t aggregate_member_formal_ordinal = SIZE_MAX;
    size_t aggregate_member_field_count = 0;
    size_t aggregate_member_pending_sequence = SIZE_MAX;
    ParamMode aggregate_member_formal_mode = PARAM_MODE_DEFAULT;
    bool aggregate_member_premise = false;
    bool array = type_is_constructed_named(argument_type, "Array");
    bool string_array = collection_owned_is_string_array(argument_type);
    /* Every own Array handoff needs a final currency snapshot for its call,
     * formal, type and storage identity. Element-ownership evidence remains
     * String-specific below. */
    bool own_transfer = parameter_mode == PARAM_MODE_OWN && array;
    bool inout_string = parameter_mode == PARAM_MODE_MUT_REF && string_array;
    bool default_string = parameter_mode == PARAM_MODE_DEFAULT && string_array;
    Symbol *detached_binding = ctx != NULL && argument != NULL
        && argument->type == AST_IDENTIFIER
        ? scope_lookup(ctx->scope, ast_identifier_name(argument)) : NULL;
    uint32_t detached_binding_id = detached_binding != NULL
        ? ast_identifier_binding_syntax_id(argument) : 0;
    /* The same finalizer owns the release proof for a detached field of any
     * Array element type.  String-element requirements reach this store
     * already; a plain Array needs this exact storage-only route so a
     * later-declared callee cannot turn a provisional detach grant into
     * unproved cleanup authority. */
    bool detached_member_own = parameter_mode == PARAM_MODE_OWN && array
        && detached_binding != NULL && detached_binding_id != 0
        && detached_binding_id == detached_binding->decl_syntax_id
        && !detached_binding->is_host_field && !detached_binding->is_consumed
        && !detached_binding->has_escaped_array_storage
        && detached_binding->has_exclusive_array_storage
        && detached_binding->array_storage_member_move_source != NULL
        && detached_binding->array_storage_member_move_source_id != 0
        && detached_binding->array_storage_member_move_root_binding_id != 0;
    if (!collection_owned_record_default_aggregate_actual(call, callee_decl,
            argument_index, formal_index, argument, argument_type,
            parameter_mode, ctx))
        return false;
    if ((!own_transfer && !inout_string && !default_string
            && !detached_member_own)
        || !array)
        return true;
    store = ctx != NULL ? ctx->collection_owned_element_requirements : NULL;
    function_id = ast_node_stable_id(callee_decl);
    caller_id = ctx != NULL ? ast_node_stable_id(ctx->current_function_decl) : 0;
    /* An intent has typed value bindings rather than FuncParam rows.  Its
     * transitive storage effect is owned by function_param_flow_intent_call,
     * while each action call records its ordinary routine requirements when
     * the intent body is checked.  Validate this exact intent actual here but
     * do not manufacture a second parameter identity or requirement graph. */
    if (callee_decl != NULL && callee_decl->type == AST_INTENT_DECL) {
        size_t binding_count = 0;
        ASTNode **bindings = ast_intent_decl_bindings(
            callee_decl, &binding_count);
        ASTNode *binding = formal_index < binding_count
            ? bindings[formal_index] : NULL;
        if (store == NULL || store->program != ctx->program_root
            || call == NULL || call->type != AST_CALL || argument == NULL
            || ast_node_stable_id(call) == 0 || function_id == 0
            || ast_call_semantic_callee_decl_id(call) != function_id
            || !ast_call_semantic_callee_declared_callable(call)
            || ast_call_semantic_callee_value_binding_id(call) != 0
            || ast_call_semantic_callee_is_stdlib(call)
            || argument_index >= ast_call_arg_count(call)
            || ast_call_argument(call, argument_index) != argument
            || ast_node_stable_id(argument) == 0
            || binding == NULL || binding->type != AST_INTENT_VALUE
            || ast_node_stable_id(binding) == 0
            || parameter_mode != PARAM_MODE_DEFAULT)
            return collection_owned_requirement_failure(ctx, call,
                "missing intent collection-call value-binding identity");
        return true;
    }
    if (store == NULL || store->program != ctx->program_root
        || call == NULL || call->type != AST_CALL || argument == NULL
        || ast_node_stable_id(call) == 0
        || ((own_transfer || inout_string) && caller_id == 0)
        || callee_decl == NULL || callee_decl->type != AST_FUNC_DECL
        || function_id == 0
        || ast_call_semantic_callee_decl_id(call) != function_id
        || !ast_call_semantic_callee_declared_callable(call)
        || ast_call_semantic_callee_value_binding_id(call) != 0
        || ast_call_semantic_callee_is_stdlib(call)
        || formal_index >= ast_func_param_count(callee_decl)
        || argument_index >= ast_call_arg_count(call)
        || ast_call_argument(call, argument_index) != argument
        || ast_node_stable_id(argument) == 0)
        return collection_owned_requirement_failure(ctx, call, "missing collection-call declaration identity");
    target_parameter = ast_func_param(callee_decl, formal_index);
    if (target_parameter == NULL || target_parameter->mode != parameter_mode
        || ast_func_param_stable_id(target_parameter) == 0
        || target_parameter->type == NULL
        || ast_node_stable_id(target_parameter->type) == 0)
        return collection_owned_requirement_failure(ctx, call, "missing collection-call formal identity");
    /* Own temporaries remain governed by the existing named-source boundary.
     * A default Array<String> temporary is retained as an exact call actual;
     * finalization asks for its immutable element receipt only when the
     * callee's mutation requirement is active. */
    if (argument->type != AST_IDENTIFIER && own_transfer)
        return true;
    if (argument->type != AST_IDENTIFIER && inout_string)
        return collection_owned_requirement_failure(
            ctx, call, "inout Array<String> actual is not a named place");
    /* Parameter placeholders may still be UNKNOWN for forward declarations.
     * Only an exact typed deep-drop seed will activate this target later. */
    if (!collection_owned_requirement_parameter_row(store, function_id,
            ast_func_param_stable_id(target_parameter), &target))
        return collection_owned_requirement_failure(ctx, call, "target allocation failed");
    if (!collection_owned_requirement_bind_parameter_identity(
            store, target, callee_decl, target_parameter, formal_index))
        return collection_owned_requirement_failure(ctx, call,
            "collection-call target formal identity changed");
    if (argument->type == AST_IDENTIFIER) {
        Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(argument));
        const PgyCollectionOwnershipFact *fact;
        binding_id = ast_identifier_binding_syntax_id(argument);
        if (binding == NULL || binding_id == 0
            || binding_id != binding->decl_syntax_id)
            return collection_owned_requirement_failure(ctx, call, "missing actual binding identity");
        if (own_transfer && collection_owned_is_string_array(argument_type)
            && !semantic_indexed_string_borrow_owner_transfer_allowed(argument, ctx)) return false;
        if (own_transfer && (binding->is_host_field || (binding->is_parameter
            && binding->param_mode != PARAM_MODE_OWN)))
            return collection_owned_requirement_failure(ctx, call, "borrowed or hosted array cannot be transferred by own");
        /* An own handoff consumes storage even when the callee has no
         * element-drop seed. Do not forward a known stored descriptor alias. */
        if (own_transfer && binding->has_escaped_array_storage) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                argument, "An own Array argument cannot transfer escaped array storage from '%s'",
                binding->name != NULL ? binding->name : "<array>");
            return false;
        }
        fact = semantic_collection_ownership_fact_find(ctx, caller_id, binding_id);
        exclusive_storage = binding->has_exclusive_array_storage;
        detached_member_requires_release = exclusive_storage
            && binding->array_storage_member_move_source != NULL
            && binding->array_storage_member_move_source_id != 0
            && binding->array_storage_member_move_root_binding_id != 0;
        if (semantic_array_storage_has_pending(binding)
            || (fact != NULL && fact->return_elements_source != NULL)
            || semantic_array_storage_has_pending_elements(ctx, binding_id)) {
            pending_binding_id = binding->decl_syntax_id;
            pending_sequence = semantic_array_storage_pending_sequence(ctx);
        }
        if (fact != NULL) {
            ownership = fact->element_ownership;
        } else if (own_transfer && !binding->is_parameter
            && collection_owned_is_string_array(argument_type)) {
            return collection_owned_requirement_failure(ctx, call, "missing local collection fact");
        }
        if (inout_string && fact != NULL) {
            size_t source_formal = SIZE_MAX;
            ParamMode source_mode = PARAM_MODE_DEFAULT;
            size_t source_field_count = 0;
            uint32_t source_fields[PGY_COLLECTION_MEMBER_PATH_LIMIT] = {0};
            ASTNode *source_function = ctx->current_function_decl;
            bool current_member =
                collection_member_move_pending_call_parameter_path(
                    fact, binding, ctx, call, callee_decl,
                    argument_index, formal_index,
                    &source_formal, &source_mode,
                    source_fields, &source_field_count,
                    &aggregate_member_pending_sequence);
            FuncParam *source_parameter = current_member
                && source_function != NULL
                && source_formal < ast_func_param_count(source_function)
                ? ast_func_param(source_function, source_formal) : NULL;
            if (current_member && source_parameter != NULL
                && source_field_count != 0
                && source_field_count <= PGY_COLLECTION_MEMBER_PATH_LIMIT
                && ast_node_stable_id(source_function) != 0
                && ast_func_param_stable_id(source_parameter) != 0
                && source_parameter->mode == source_mode
                && fact->member_move_source != NULL
                && fact->member_move_source_id != 0
                && ast_node_stable_id(fact->member_move_source)
                    == fact->member_move_source_id) {
                aggregate_member_function = source_function;
                aggregate_member_source = fact->member_move_source;
                aggregate_member_function_id =
                    ast_node_stable_id(source_function);
                aggregate_member_formal_id =
                    ast_func_param_stable_id(source_parameter);
                aggregate_member_source_id = fact->member_move_source_id;
                aggregate_member_source_binding_id =
                    fact->source_binding_syntax_id;
                aggregate_member_leaf_field_id = fact->origin_syntax_id;
                aggregate_member_source_birth_serial =
                    fact->source_birth_serial;
                memcpy(aggregate_member_field_ids, source_fields,
                    source_field_count * sizeof(*source_fields));
                aggregate_member_formal_ordinal = source_formal;
                aggregate_member_field_count = source_field_count;
                aggregate_member_formal_mode = source_mode;
                aggregate_member_premise = true;
                if (!collection_owned_require_prior_member_reads(store,
                        aggregate_member_function_id,
                        aggregate_member_formal_id,
                        aggregate_member_source,
                        aggregate_member_source_id,
                        aggregate_member_source_binding_id,
                        aggregate_member_leaf_field_id,
                        aggregate_member_source_birth_serial,
                        aggregate_member_field_ids,
                        aggregate_member_field_count))
                    return collection_owned_requirement_failure(ctx, call,
                        "moved-member read prefix identity changed");
            }
        }
        size_t source_formal = SIZE_MAX;
        ParamMode source_mode = PARAM_MODE_DEFAULT;
        bool parameter_carrier = false;
        bool default_forward = default_string
            && ctx->current_function_decl != NULL
            && semantic_collection_owned_element_parameter_carrier_current(
                argument, ctx->current_function_decl, ctx,
                &source_formal, &source_mode, &parameter_carrier);
        if (default_string && parameter_carrier && !default_forward)
            return collection_owned_requirement_failure(ctx, call,
                "default Array<String> carrier identity is not current");
        if (default_forward
            || (binding->is_parameter
                && (binding->param_mode == PARAM_MODE_OWN
                    || (inout_string
                        && binding->param_mode == PARAM_MODE_MUT_REF)))) {
            forwarded_formal = true;
            size_t source;
            OwnedElementForwardingEdge *grown;
            ASTNode *source_function = ctx->current_function_decl;
            FuncParam *source_parameter = default_forward
                && source_formal < ast_func_param_count(source_function)
                ? ast_func_param(source_function, source_formal)
                : formal_for_binding(source_function, binding_id,
                    binding->param_mode);
            uint32_t source_id = source_parameter != NULL
                ? ast_func_param_stable_id(source_parameter) : 0;
            size_t source_ordinal = default_forward ? source_formal : SIZE_MAX;
            if (!default_forward && source_parameter != NULL) {
                for (size_t i = 0;
                     i < ast_func_param_count(source_function); i++)
                    if (ast_func_param(source_function, i) == source_parameter) {
                        source_ordinal = i;
                        break;
                    }
            }
            if (source_parameter == NULL || source_id == 0
                || source_ordinal == SIZE_MAX
                || (default_forward && source_parameter->mode != source_mode)
                || !collection_owned_requirement_parameter_row(
                    store, caller_id, source_id, &source)
                || !collection_owned_requirement_bind_parameter_identity(
                    store, source, source_function, source_parameter,
                    source_ordinal))
                return collection_owned_requirement_failure(ctx, call, "forwarding source identity failed");
            if (store->edge_count == SIZE_MAX)
                return collection_owned_requirement_failure(ctx, call, "forwarding edge allocation failed");
            grown = collection_owned_requirement_rows_reserve(store->edges,
                &store->edge_capacity, store->edge_count + 1,
                sizeof(*store->edges));
            if (grown == NULL)
                return collection_owned_requirement_failure(ctx, call, "forwarding edge allocation failed");
            store->edges = grown;
            store->edges[store->edge_count++] =
                (OwnedElementForwardingEdge){source, target};
        }
    }
    /* A direct aggregate field does not have an Array binding whose element
     * receipt can be replayed.  Preserve the already-owned typed member path
     * instead; finalization delegates its proof to the aggregate member owner
     * after every caller and return edge has been published. */
    if (default_string && argument->type == AST_MEMBER_ACCESS
        && !forwarded_formal) {
        PgyCollectionMemberPath member_path;
        size_t source_formal = SIZE_MAX;
        ParamMode source_mode = PARAM_MODE_DEFAULT;
        ASTNode *source_function = ctx->current_function_decl;
        bool parameter_member = collection_current_parameter_member_path(
            argument, ctx, &source_formal, &source_mode, &member_path);
        bool local_member = !parameter_member
            && collection_current_local_member_path(
                argument, ctx, &member_path);
        FuncParam *source_parameter = parameter_member
            && source_function != NULL
            && source_formal < ast_func_param_count(source_function)
            ? ast_func_param(source_function, source_formal) : NULL;
        if ((parameter_member || local_member)
            && source_function != NULL
            && source_function->type == AST_FUNC_DECL
            && ast_node_stable_id(source_function) != 0
            && member_path.root != NULL
            && member_path.root_binding_id != 0
            && member_path.field_count != 0
            && member_path.field_count <= PGY_COLLECTION_MEMBER_PATH_LIMIT
            && (!parameter_member
                || (source_parameter != NULL
                    && source_parameter->mode == source_mode
                    && ast_func_param_stable_id(source_parameter)
                        == member_path.root_binding_id))) {
            aggregate_member_function = source_function;
            aggregate_member_source = argument;
            aggregate_member_function_id =
                ast_node_stable_id(source_function);
            aggregate_member_formal_id = parameter_member
                ? ast_func_param_stable_id(source_parameter) : 0;
            aggregate_member_source_id = ast_node_stable_id(argument);
            aggregate_member_source_binding_id =
                member_path.root_binding_id;
            aggregate_member_leaf_field_id =
                member_path.field_ids[member_path.field_count - 1];
            aggregate_member_source_birth_serial = 0;
            memcpy(aggregate_member_field_ids, member_path.field_ids,
                member_path.field_count * sizeof(*member_path.field_ids));
            aggregate_member_formal_ordinal = parameter_member
                ? source_formal : SIZE_MAX;
            aggregate_member_field_count = member_path.field_count;
            aggregate_member_formal_mode = source_mode;
            aggregate_member_premise = true;
        }
    }
    if ((own_transfer || default_string)
        && collection_owned_is_string_array(argument_type)
        && !forwarded_formal
        && !semantic_array_storage_capture_element_use(
            argument, ctx, &element_use)
        && own_transfer)
        return collection_owned_requirement_failure(ctx, call,
            "missing current element-use identity");
    if (inout_string && !forwarded_formal)
        (void)semantic_array_storage_capture_element_use(
            argument, ctx, &element_use);
    if (store->actual_count == SIZE_MAX)
        return collection_owned_requirement_failure(ctx, call, "actual snapshot allocation failed");
    OwnedElementActualSnapshot *grown = collection_owned_requirement_rows_reserve(store->actuals,
        &store->actual_capacity, store->actual_count + 1,
        sizeof(*store->actuals));
    if (grown == NULL)
        return collection_owned_requirement_failure(ctx, call, "actual snapshot allocation failed");
    store->actuals = grown;
    store->actuals[store->actual_count++] = (OwnedElementActualSnapshot){
        .call = call, .call_id = ast_node_stable_id(call),
        .callee_decl = callee_decl, .callee_id = function_id,
        .ordinal = argument_index, .formal_ordinal = formal_index, .formal = target_parameter,
        .formal_id = ast_func_param_stable_id(target_parameter),
        .formal_type = target_parameter->type,
        .formal_type_id = ast_node_stable_id(target_parameter->type),
        .argument = argument, .argument_id = ast_node_stable_id(argument),
        .argument_type = argument_type, .target = target,
        .parameter_mode = parameter_mode,
        .element_ownership = ownership, .exclusive_storage = exclusive_storage,
        .pending_binding_id = pending_binding_id,
        .element_use = element_use,
        .forwarded_formal = forwarded_formal,
        .aggregate_member_function = aggregate_member_function,
        .aggregate_member_source = aggregate_member_source,
        .aggregate_member_function_id = aggregate_member_function_id,
        .aggregate_member_formal_id = aggregate_member_formal_id,
        .aggregate_member_source_id = aggregate_member_source_id,
        .aggregate_member_source_binding_id =
            aggregate_member_source_binding_id,
        .aggregate_member_leaf_field_id = aggregate_member_leaf_field_id,
        .aggregate_member_source_birth_serial =
            aggregate_member_source_birth_serial,
        .aggregate_member_formal_ordinal = aggregate_member_formal_ordinal,
        .aggregate_member_field_count = aggregate_member_field_count,
        .aggregate_member_pending_sequence =
            aggregate_member_pending_sequence,
        .aggregate_member_formal_mode = aggregate_member_formal_mode,
        .aggregate_member_premise = aggregate_member_premise,
        .detached_member_requires_release = detached_member_requires_release,
        .pending_sequence = pending_sequence};
    if (aggregate_member_premise)
        memcpy(store->actuals[store->actual_count - 1]
                .aggregate_member_field_ids,
            aggregate_member_field_ids,
            aggregate_member_field_count
                * sizeof(*aggregate_member_field_ids));
    if (own_transfer && collection_owned_is_string_array(argument_type))
        semantic_indexed_string_borrow_invalidate_deep_drop(argument, ctx);
    return !ctx->has_error;
}

/* The inout call itself may replace the caller's element-ownership fact, so
 * finalization cannot replay the pre-call premise through the post-call fact.
 * Validate the immutable receipt issued at the exact argument instead.  The
 * receipt still fails closed on caller, binding, dependency and publication
 * drift; a borrowed premise was never issued as owned in the first place. */
static bool
collection_owned_inout_element_premise_current(
    SemanticContext *ctx, const OwnedElementActualSnapshot *actual)
{
    const ArrayStorageReturnElementReceipt *receipt;
    PublishedNode *row;
    const ASTNode *dependency;

    if (ctx == NULL || actual == NULL || actual->argument == NULL)
        return false;
    receipt = &actual->element_use;
    if (!receipt->owned || receipt->function_id == 0
        || (receipt->dependency_call == NULL)
            != (receipt->dependency_call_id == 0))
        return false;
    row = publication_node(ctx->function_param_publications,
        actual->argument, false);
    if (row == NULL || row->node != actual->argument
        || row->id == 0 || row->id != actual->argument_id
        || row->id != ast_node_stable_id(actual->argument)
        || !row->value_present || row->meta || row->value_type == NULL
        || row->value_type == TYPE_UNKNOWN
        || !type_equals(row->value_type, actual->argument_type)
        || row->caller == NULL || row->caller_id != receipt->function_id
        || row->caller_id != ast_node_stable_id(row->caller))
        return false;
    if (actual->argument->type != AST_IDENTIFIER
        || receipt->binding_id == 0
        || receipt->binding_id
            != ast_identifier_binding_syntax_id(actual->argument)
        || row->binding_id != receipt->binding_id)
        return false;
    dependency = receipt->dependency_call;
    if (dependency == NULL)
        return true;
    if (receipt->dependency_call_id != ast_node_stable_id(dependency)
        || (dependency->type != AST_CALL
            && dependency->type != AST_ARRAY_LITERAL))
        return false;
    if (dependency->type == AST_CALL)
        return semantic_array_storage_owned_string_result_current(
            ctx, dependency);
    const ASTNode *nested = NULL;
    return semantic_array_storage_return_elements_owned(
            (ASTNode *)dependency, ctx, &nested)
        && nested == NULL;
}

/* A reused inout place cannot be wrapped in Clone at the call.  When the
 * exact call-entry receipt says this local still comes from its
 * initializer, select that initializer only if the local itself is read later
 * or its source is an inout parameter whose storage must be restored to the
 * caller.  A last-use local keeps the ordinary borrowed-element refusal. */
static bool
collection_owned_select_reused_inout_initializer_copy(
    SemanticContext *ctx, const OwnedElementActualSnapshot *actual,
    const ASTNode **selected_initializer)
{
    FunctionParamPublicationStore *publication;
    FunctionParamFlowSummaryStore *flow;
    FunctionParamFlowProgramPointIndex *index = NULL;
    PublishedNode *argument_row;
    PublishedNode *binding_row;
    PublishedNode *initializer_row;
    const ASTNode *declaration;
    const ASTNode *initializer;
    uint32_t binding_id;
    size_t source_formal = SIZE_MAX;

    if (selected_initializer != NULL)
        *selected_initializer = NULL;
    if (ctx == NULL || actual == NULL || actual->argument == NULL
        || actual->argument->type != AST_IDENTIFIER
        || actual->argument_id == 0
        || actual->argument_id != ast_node_stable_id(actual->argument)
        || actual->element_use.empty_local_initializer != NULL
        || !semantic_param_publication_inputs_ready(ctx))
        return false;
    publication = ctx->function_param_publications;
    argument_row = publication != NULL
        ? publication_node(publication, actual->argument, false) : NULL;
    binding_id = semantic_param_publication_binding(ctx, actual->argument);
    binding_row = publication != NULL && binding_id != 0
        ? publication_node_by_id(publication, binding_id) : NULL;
    declaration = binding_row != NULL ? binding_row->node : NULL;
    initializer = declaration != NULL && declaration->type == AST_LET_DECL
        ? ast_let_initializer(declaration) : NULL;
    initializer_row = publication != NULL && initializer != NULL
        ? publication_node(publication, initializer, false) : NULL;
    if (publication == NULL || argument_row == NULL
        || argument_row->node != actual->argument
        || argument_row->id != actual->argument_id
        || !argument_row->value_present || argument_row->meta
        || argument_row->binding_id != binding_id
        || argument_row->caller == NULL
        || argument_row->caller->type != AST_FUNC_DECL
        || argument_row->caller_id == 0
        || argument_row->caller_id
            != ast_node_stable_id(argument_row->caller)
        || argument_row->caller_id != actual->element_use.function_id
        || binding_id == 0
        || binding_id != ast_identifier_binding_syntax_id(actual->argument)
        || binding_row == NULL || binding_row->id != binding_id
        || declaration == NULL || declaration->type != AST_LET_DECL
        || ast_node_stable_id(declaration) != binding_id
        || ast_let_is_alias(declaration)
        || initializer == NULL || ast_node_stable_id(initializer) == 0
        || initializer_row == NULL || initializer_row->node != initializer
        || initializer_row->id != ast_node_stable_id(initializer)
        || !initializer_row->value_present || initializer_row->meta
        || initializer_row->caller != argument_row->caller
        || initializer_row->caller_id != argument_row->caller_id
        || !type_equals(initializer_row->value_type, actual->argument_type)
        || semantic_loan_copy_landing_satisfied(ctx, initializer)
        || !semantic_loan_copy_landing_carrier_copies(ctx, initializer))
        return false;
    flow = function_param_flow_store_get(
        &(SlotFunctionLookup){ctx, ctx->program_root});
    bool index_ready = flow != NULL && !flow->failed && !flow->solving
        && !flow->validating_current
        && function_param_flow_program_point_index_build(
            flow, (ASTNode *)argument_row->caller, &index)
        && index != NULL && index->claims != NULL;
    bool still_initializer = index_ready
        && function_param_flow_binding_still_initializer_at_call(
            ctx, index->claims, actual->call, binding_id,
            ast_func_body(argument_row->caller));
    bool read_after = index_ready
        && function_param_flow_binding_read_after_call(
            ctx, index->claims, actual->call, binding_id,
            ast_func_body(argument_row->caller));
    for (size_t i = 0;
         i < ast_func_param_count(argument_row->caller); i++) {
        FuncParam *formal = ast_func_param(argument_row->caller, i);
        if (formal != NULL && initializer_row->binding_id != 0
            && ast_func_param_stable_id(formal)
                == initializer_row->binding_id) {
            source_formal = i;
            break;
        }
    }
    bool inout_source_must_restore = source_formal != SIZE_MAX
        && semantic_param_publication_array_parameter_value_current(
            ctx, initializer, argument_row->caller, source_formal,
            PARAM_MODE_MUT_REF);
    if (!still_initializer
        || (!read_after && !inout_source_must_restore)
        || !semantic_loan_copy_landing_select_carrier(ctx, initializer))
        return false;
    if (selected_initializer != NULL)
        *selected_initializer = initializer;
    return true;
}

static bool
collection_owned_inout_aggregate_member_premise_current(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store,
    const OwnedElementActualSnapshot *actual)
{
    if (ctx == NULL || store == NULL || actual == NULL
        || !actual->aggregate_member_premise
        || actual->argument == NULL
        || actual->argument->type != AST_IDENTIFIER
        || actual->element_use.binding_id == 0
        || actual->element_use.binding_id
            != ast_identifier_binding_syntax_id(actual->argument)
        || actual->aggregate_member_function == NULL
        || actual->aggregate_member_function->type != AST_FUNC_DECL
        || actual->aggregate_member_function_id == 0
        || ast_node_stable_id(actual->aggregate_member_function)
            != actual->aggregate_member_function_id
        || actual->aggregate_member_formal_ordinal
            >= ast_func_param_count(actual->aggregate_member_function)
        || actual->aggregate_member_field_count == 0
        || actual->aggregate_member_field_count
            > PGY_COLLECTION_MEMBER_PATH_LIMIT
        || actual->aggregate_member_source == NULL
        || actual->aggregate_member_source_id == 0
        || ast_node_stable_id(actual->aggregate_member_source)
            != actual->aggregate_member_source_id
        || actual->aggregate_member_source_binding_id == 0
        || actual->aggregate_member_leaf_field_id == 0
        || actual->aggregate_member_source_birth_serial == 0
        || actual->aggregate_member_pending_sequence == SIZE_MAX) {
        return collection_owned_requirement_failure(ctx,
            actual != NULL ? actual->argument : NULL,
            "aggregate member element-premise identity changed");
    }
    FuncParam *source_parameter = ast_func_param(
        actual->aggregate_member_function,
        actual->aggregate_member_formal_ordinal);
    if (source_parameter == NULL
        || source_parameter->mode != actual->aggregate_member_formal_mode
        || ast_func_param_stable_id(source_parameter)
            != actual->aggregate_member_formal_id)
        return collection_owned_requirement_failure(ctx, actual->argument,
            "aggregate member element-premise formal changed");
    /* The inout call may replace the local collection fact.  The exact
     * pre-call edge and member path above are immutable receipts; replaying
     * the post-call fact as MEMBER_MOVE would reject that legitimate
     * replacement.  Keep storage and element-prefix preservation separate
     * and fail closed if the issued call-entry receipt is no longer current. */
    if (!semantic_array_storage_pending_call_entry_current(ctx,
            actual->element_use.binding_id, actual->call,
            actual->callee_decl, actual->ordinal,
            actual->formal_ordinal,
            actual->aggregate_member_pending_sequence)
        || !semantic_array_storage_pending_preserved(ctx,
            actual->element_use.binding_id,
            actual->aggregate_member_pending_sequence)
        || !semantic_array_storage_pending_owned_elements(ctx,
            actual->element_use.binding_id,
            actual->aggregate_member_pending_sequence))
        return false;
    return collection_owned_validate_aggregate_member_requirement(
        ctx, store, actual->aggregate_member_function_id,
        actual->aggregate_member_formal_id,
        actual->aggregate_member_field_ids,
        actual->aggregate_member_field_count, actual->argument);
}

static bool
collection_owned_required_member_reads_current(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store)
{
    for (size_t i = 0; i < store->member_read_count; i++) {
        const OwnedElementMemberRead *read = &store->member_reads[i];
        PgyCollectionMemberPath path;
        ASTNode *source_function = NULL;
        size_t source_formal = SIZE_MAX;
        ParamMode source_mode = PARAM_MODE_DEFAULT;
        size_t target_formal = SIZE_MAX;
        if (!read->required)
            continue;
        bool source_current = collection_published_member_move_parameter_path(
            read->member_source, ctx, &source_function, &source_formal,
            &source_mode, &path);
        FuncParam *target_parameter = read->callee_decl != NULL
            && read->target_formal_ordinal
                < ast_func_param_count(read->callee_decl)
            ? ast_func_param(read->callee_decl,
                read->target_formal_ordinal) : NULL;
        FuncParam *source_parameter = read->source_function != NULL
            && read->source_formal_ordinal
                < ast_func_param_count(read->source_function)
            ? ast_func_param(read->source_function,
                read->source_formal_ordinal) : NULL;
        if (read->call == NULL || read->call->type != AST_CALL
            || read->call_id == 0
            || ast_node_stable_id(read->call) != read->call_id
            || read->callee_decl == NULL
            || read->callee_decl->type != AST_FUNC_DECL
            || read->callee_id == 0
            || ast_node_stable_id(read->callee_decl) != read->callee_id
            || ast_call_semantic_callee_decl_id(read->call)
                != read->callee_id
            || ast_call_semantic_callee_value_binding_id(read->call) != 0
            || !ast_call_semantic_callee_declared_callable(read->call)
            || ast_call_semantic_callee_is_stdlib(read->call)
            || read->actual_ordinal >= ast_call_arg_count(read->call)
            || ast_call_argument(read->call, read->actual_ordinal)
                != read->argument
            || read->argument == NULL
            || read->argument->type != AST_IDENTIFIER
            || read->argument_id == 0
            || ast_node_stable_id(read->argument) != read->argument_id
            || read->argument_binding_id == 0
            || ast_identifier_binding_syntax_id(read->argument)
                != read->argument_binding_id
            || !semantic_param_publication_checked_argument(
                ctx, read->call, read->actual_ordinal)
            || !semantic_param_publication_formal_for_actual(ctx,
                read->call, read->actual_ordinal, &target_formal)
            || target_formal != read->target_formal_ordinal
            || target_parameter == NULL
            || target_parameter->mode != read->target_mode
            || (read->target_mode != PARAM_MODE_DEFAULT
                && read->target_mode != PARAM_MODE_REF)
            || ast_func_param_stable_id(target_parameter)
                != read->target_formal_id
            || read->source_function == NULL
            || read->source_function->type != AST_FUNC_DECL
            || read->source_function_id == 0
            || ast_node_stable_id(read->source_function)
                != read->source_function_id
            || source_parameter == NULL
            || source_parameter->mode != read->source_mode
            || ast_func_param_stable_id(source_parameter)
                != read->source_formal_id
            || !source_current
            || source_function != read->source_function
            || source_formal != read->source_formal_ordinal
            || source_mode != read->source_mode
            || path.root_binding_id != read->root_binding_id
            || read->member_source_id == 0
            || ast_node_stable_id(read->member_source)
                != read->member_source_id
            || read->leaf_field_id == 0
            || read->field_count == 0
            || read->field_count > PGY_COLLECTION_MEMBER_PATH_LIMIT
            || path.field_count != read->field_count
            || path.field_ids[path.field_count - 1]
                != read->leaf_field_id
            || read->source_birth_serial == 0
            || memcmp(path.field_ids, read->field_ids,
                read->field_count * sizeof(*read->field_ids)) != 0)
            return collection_owned_requirement_failure(ctx, read->call,
                "moved-member declared-read identity changed before finalization");
        if (!semantic_param_publication_routine_inputs_current(
                ctx, read->callee_decl, true)
            || function_param_flow_array_preservation_failures(ctx,
                read->callee_decl, read->target_formal_ordinal) != 0
            || (function_param_flow_summary_for_param(ctx,
                    read->callee_decl, read->target_formal_ordinal)
                & ~SLOT_PARAM_SUMMARY_READ) != 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                read->call,
                "A moved aggregate Array<String> member used by a later owned-element mutation requires this intervening call to be read-only");
            return false;
        }
    }
    return true;
}

bool
semantic_collection_owned_element_requirements_finalize(SemanticContext *ctx)
{
    CollectionOwnedElementRequirementStore *store = ctx != NULL
        ? ctx->collection_owned_element_requirements : NULL;
    size_t remaining;
    bool changed = true;
    if (store == NULL || store->program != ctx->program_root)
        return collection_owned_requirement_failure(ctx, ctx != NULL ? ctx->program_root : NULL,
            "missing analyzed-program store");
    if (store->edge_count == SIZE_MAX)
        return collection_owned_requirement_failure(ctx, store->program, "forwarding bound overflow");
    remaining = store->edge_count + 1;
    while (changed && remaining > 0) {
        changed = false;
        for (size_t i = 0; i < store->edge_count; i++) {
            OwnedElementForwardingEdge edge = store->edges[i];
            unsigned merged = store->parameters[edge.source].requirements
                | store->parameters[edge.target].requirements;
            if (merged != store->parameters[edge.source].requirements) {
                store->parameters[edge.source].requirements = merged;
                changed = true;
            }
        }
        remaining--;
    }
    if (changed)
        return collection_owned_requirement_failure(ctx, store->program, "forwarding did not converge");
    for (size_t i = 0; i < store->parameter_count; i++) {
        OwnedElementParameter *parameter = &store->parameters[i];
        if ((parameter->requirements & 1u) == 0
            || !parameter->identity_present
            || parameter->parameter_mode != PARAM_MODE_DEFAULT)
            continue;
        if (parameter->function == NULL
            || parameter->function->type != AST_FUNC_DECL
            || ast_node_stable_id(parameter->function)
                != parameter->function_id
            || parameter->parameter == NULL
            || parameter->formal_ordinal
                >= ast_func_param_count(parameter->function)
            || ast_func_param(parameter->function,
                    parameter->formal_ordinal)
                != parameter->parameter
            || ast_func_param_stable_id(parameter->parameter)
                != parameter->parameter_id
            || parameter->parameter->mode != PARAM_MODE_DEFAULT)
            return collection_owned_requirement_failure(ctx,
                parameter->function,
                "default Array<String> mutation formal identity changed");
        if (!semantic_default_aggregate_formal_moves(ctx,
                parameter->function, parameter->formal_ordinal)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                parameter->function,
                "Owned String mutation requires default Array<String> parameter syntax %u of callable syntax %u to move at every call",
                parameter->parameter_id, parameter->function_id);
            return false;
        }
    }
    if (!collection_owned_required_member_reads_current(ctx, store))
        return false;
    if (!collection_owned_validate_member_pushes(ctx, store))
        return false;
    for (size_t i = 0; i < store->actual_count; i++) {
        OwnedElementActualSnapshot actual = store->actuals[i];
        SemanticPublishedValue current;
        size_t mapped_formal = 0;
        if (actual.target >= store->parameter_count || actual.call == NULL
            || actual.call->type != AST_CALL || actual.call_id == 0
            || ast_node_stable_id(actual.call) != actual.call_id
            || !semantic_param_publication_checked_argument(ctx, actual.call, actual.ordinal)
            || actual.argument == NULL || actual.argument_id == 0
            || ast_call_argument(actual.call, actual.ordinal) != actual.argument
            || actual.argument_id != ast_node_stable_id(actual.argument)
            || actual.callee_decl == NULL || actual.callee_decl->type != AST_FUNC_DECL
            || actual.callee_id == 0
            || ast_node_stable_id(actual.callee_decl) != actual.callee_id
            || ast_call_semantic_callee_decl_id(actual.call) != actual.callee_id
            || !ast_call_semantic_callee_declared_callable(actual.call)
            || ast_call_semantic_callee_value_binding_id(actual.call) != 0
            || ast_call_semantic_callee_is_stdlib(actual.call)
            || !semantic_param_publication_formal_for_actual(ctx, actual.call,
                actual.ordinal, &mapped_formal) || mapped_formal != actual.formal_ordinal
            || actual.formal_ordinal >= ast_func_param_count(actual.callee_decl)
            || ast_func_param(actual.callee_decl, actual.formal_ordinal) != actual.formal
            || actual.formal == NULL || actual.formal_id == 0
            || ast_func_param_stable_id(actual.formal) != actual.formal_id
            || actual.formal->mode != actual.parameter_mode
            || (actual.parameter_mode != PARAM_MODE_OWN
                && actual.parameter_mode != PARAM_MODE_MUT_REF
                && actual.parameter_mode != PARAM_MODE_DEFAULT)
            || actual.formal->type != actual.formal_type
            || actual.formal_type_id == 0
            || ast_node_stable_id(actual.formal_type) != actual.formal_type_id
            || store->parameters[actual.target].function_id != actual.callee_id
            || store->parameters[actual.target].parameter_id != actual.formal_id)
            return collection_owned_requirement_failure(ctx, actual.call,
                "owned-call identity or formal changed before finalization");
        current = semantic_param_publication_value(ctx, actual.argument);
        if (!current.ok || current.meta || current.type == NULL
            || !type_is_constructed_named(current.type, "Array")
            || !type_equals(current.type, actual.argument_type))
            return collection_owned_requirement_failure(ctx, actual.argument,
                "owned-call actual type changed before finalization");
        OwnedElementParameter target = store->parameters[actual.target];
        if (actual.parameter_mode == PARAM_MODE_DEFAULT) {
            if ((target.requirements & 1u) == 0
                || actual.forwarded_formal)
                continue;
            if (!semantic_default_aggregate_formal_moves(ctx,
                    actual.callee_decl, actual.formal_ordinal)) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                    actual.call,
                    "Owned String mutation requires default Array<String> parameter syntax %u of callable syntax %u to move at this call",
                    target.parameter_id, target.function_id);
                return false;
            }
            bool elements_current =
                semantic_array_storage_element_use_current(
                    actual.argument, ctx, &actual.element_use);
            if (!elements_current && actual.aggregate_member_premise
                && actual.argument->type == AST_MEMBER_ACCESS)
                elements_current =
                    collection_owned_validate_direct_member_actual_elements(
                        ctx, store, &actual);
            if (!elements_current
                || actual.element_ownership ==
                    PGY_STRING_ARRAY_BORROWED_ELEMENTS) {
                /* The requirement remains authoritative: a current round
                 * with shallow elements is still refused. For one exact
                 * empty local, ask the String-store owner to select all
                 * reached pushes for owned replay; the fixed point will
                 * reissue this immutable argument receipt on the next round. */
                (void)semantic_formal_string_store_landing_select_empty_local_receiver(
                    ctx, actual.argument, &actual.element_use);
                if (ctx->has_error)
                    return false;
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                    actual.argument,
                    "A moving default Array<String> actual requires current owned elements at callable syntax %u parameter syntax %u",
                    target.function_id, target.parameter_id);
                return false;
            }
            continue;
        }
        if (actual.parameter_mode == PARAM_MODE_MUT_REF) {
            if ((target.requirements & 1u) == 0 || actual.forwarded_formal)
                continue;
            bool elements_current =
                collection_owned_inout_element_premise_current(ctx, &actual);
            if (!elements_current && actual.aggregate_member_premise)
                elements_current =
                    collection_owned_inout_aggregate_member_premise_current(
                        ctx, store, &actual);
            if (!elements_current
                || actual.element_ownership ==
                    PGY_STRING_ARRAY_BORROWED_ELEMENTS) {
                /* The call-entry receipt is the only authority that can prove
                 * this exact local began as an empty owned array after the
                 * inout result has replaced its current collection fact. Ask
                 * the existing String-store owner to select the reached
                 * shallow pushes; the fixed point rechecks this requirement
                 * against their owned replay in the next round. */
                (void)semantic_formal_string_store_landing_select_empty_local_receiver(
                    ctx, actual.argument, &actual.element_use);
                if (ctx->has_error)
                    return false;
                const ASTNode *selected_initializer = NULL;
                if (collection_owned_select_reused_inout_initializer_copy(
                        ctx, &actual, &selected_initializer)) {
                    semantic_loan_copy_landing_error_with_hints(ctx,
                        PGY_CODE_SEM_BORROW_ESCAPE,
                        PGY_CAUSE_BORROW_ESCAPE,
                        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                        selected_initializer,
                        "An inout Array<String> local used after the call requires an independent owned copy at its current initializer");
                    semantic_loan_copy_landing_refusal_tail_seal(ctx);
                    return false;
                }
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                    actual.argument,
                    "An inout Array<String> replacement for '%s' parameter '%s' requires current owned elements at callable syntax %u parameter syntax %u",
                    ast_declaration_name(actual.callee_decl),
                    actual.formal->name != NULL ? actual.formal->name : "<parameter>",
                    target.function_id, target.parameter_id);
                return false;
            }
            continue;
        }
        if (actual.detached_member_requires_release
            && !semantic_collection_owned_parameter_releases_storage(
                ctx, actual.callee_decl, actual.formal_ordinal)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                actual.argument,
                "A detached aggregate field may cross own parameter syntax %u only when callable syntax %u retires its storage",
                actual.formal_id, actual.callee_id);
            return false;
        }
        const ASTNode *dependency = actual.element_use.dependency_call;
        bool storage_dependency = dependency != NULL && dependency->type == AST_CALL;
        if (storage_dependency && !semantic_array_storage_owned_result_current(ctx, dependency)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                actual.argument, "An own Array actual requires current returned storage");
            return false;
        }
        if ((target.requirements & 1u) != 0 && !actual.forwarded_formal && collection_owned_is_string_array(actual.argument_type)
            && !semantic_array_storage_element_use_current(actual.argument, ctx, &actual.element_use)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                actual.argument, "An own Array<String> actual requires current owned elements");
            return false;
        }
        /* UNKNOWN stays bootstrap debt. This ratchet grants no permission:
         * it only falsifies a known borrowed actual at a required boundary. */
        if (actual.pending_binding_id != 0 &&
            (actual.argument == NULL || actual.argument->type != AST_IDENTIFIER
             || actual.argument_id == 0
             || actual.argument_id != ast_node_stable_id(actual.argument)
             || ast_identifier_binding_syntax_id(actual.argument) != actual.pending_binding_id
             || !semantic_array_storage_pending_preserved(ctx,
                 actual.pending_binding_id, actual.pending_sequence))) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                actual.argument, "An own Array handoff requires preserved storage after prior calls");
            return false;
        }
        if (collection_owned_is_string_array(actual.argument_type) && actual.pending_binding_id != 0
            && !semantic_array_storage_pending_owned_elements(ctx,
                actual.pending_binding_id, actual.pending_sequence)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                actual.argument, "An own Array<String> handoff requires current owned elements after prior result births");
            return false;
        }
        bool exclusive = actual.exclusive_storage || storage_dependency;
        if ((target.requirements & 2u) != 0 && !exclusive) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                actual.argument,
                "ArrayDrop release obligation in '%s' requires exclusive owned storage at callable syntax %u parameter syntax %u",
                ast_declaration_name(actual.callee_decl), target.function_id,
                target.parameter_id);
            return false;
        }
        if ((target.requirements & 1u) == 0 || actual.element_ownership !=
                PGY_STRING_ARRAY_BORROWED_ELEMENTS)
            continue;
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            actual.argument,
            "Borrowed String elements cannot cross own parameter syntax %u of callable syntax %u.\n"
            "Reason:\n"
            "- the exact callee or its own-formal forwarding path deep-releases String elements\n"
            "- storage transfer does not transfer a borrowed element lifetime\n"
            "Fix:\n"
            "- Clone into a named owned snapshot before this call\n"
            "- or produce uniformly owned elements with ArrayPushOwnedString",
            target.parameter_id, target.function_id);
        return false;
    }
    return true;
}
