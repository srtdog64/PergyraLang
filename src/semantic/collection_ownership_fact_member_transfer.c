#include "collection_ownership_fact_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_release_owner.h"
#include "symbol_table.h"
#include "type_checker.h"
#include "type_checker_internal.h"
#include "builtin_kind.h"
#include "function_param_publication_internal.h"
#include "function_param_flow_summary_internal.h"
#include "indexed_string_borrow_owner.h"
#include "../parser/ast_api.h"

/* Identity of one old-risk owner and one private ArrayPush; neither grants
 * String independence, element ownership, nor deep cleanup authority. */
struct PendingPrivateStringPush {
    const ASTNode *call, *source, *receiver, *caller, *receiver_let, *receiver_init;
    const ASTNode *dependency, *actual, *formal_root;
    uint32_t call_id, source_id, receiver_id, caller_id, receiver_let_id;
    uint32_t receiver_init_id, risk_binding_id, receiver_binding_id;
    uint32_t dependency_id, callee_id, actual_id, formal_root_id, formal_binding_id;
    uint32_t projected_receiver_id, projected_index_id, projected_field_id, projected_decl_id;
    uint32_t risk_origin_id, risk_source_binding_id;
    uint32_t receiver_origin_id, receiver_source_binding_id;
    uint32_t effect_kind, effect_receiver_id, effect_source_id;
    PgyCollectionOrigin risk_origin;
    PgyCollectionOrigin receiver_origin;
    PgyStringArrayOwnership receiver_elements;
    PgyCollectionDisposition receiver_disposition;
    uint32_t caller_formal_id, dependency_formal_id;
    size_t caller_formal, dependency_actual, dependency_formal;
    ASTNodeType source_shape;
    bool named, prepared, independent;
};

void semantic_indexed_string_private_push_destroy(void *receipt)
{
    free(receipt);
}

bool collection_private_push_recorded(SemanticContext *ctx,
    const ASTNode *call, const ASTNode *receiver, uint32_t binding_id)
{
    PublishedNode *row = ctx == NULL || ctx->function_param_publications == NULL ||
        call == NULL ? NULL :
        publication_node(ctx->function_param_publications, call, false);
    const PendingPrivateStringPush *r = row == NULL ? NULL : row->private_string_push;
    return r != NULL && row->node == call && row->id == ast_node_stable_id(call) &&
        r->call == call && r->call_id == row->id && ast_call_arg_count(call) == 2 &&
        ast_call_argument(call, 0) == receiver &&
        ast_call_argument(call, 1) == r->source && r->receiver == receiver &&
        r->receiver_id == ast_node_stable_id(receiver) &&
        r->receiver_binding_id == binding_id && binding_id != 0;
}

static bool private_push_same(const PendingPrivateStringPush *a,
    const PendingPrivateStringPush *b)
{
    return a->call == b->call && a->source == b->source &&
        a->receiver == b->receiver && a->caller == b->caller &&
        a->receiver_let == b->receiver_let && a->receiver_init == b->receiver_init &&
        a->dependency == b->dependency && a->actual == b->actual &&
        a->formal_root == b->formal_root && a->call_id == b->call_id &&
        a->source_id == b->source_id && a->receiver_id == b->receiver_id &&
        a->caller_id == b->caller_id && a->receiver_let_id == b->receiver_let_id &&
        a->receiver_init_id == b->receiver_init_id &&
        a->risk_binding_id == b->risk_binding_id &&
        a->receiver_binding_id == b->receiver_binding_id &&
        a->dependency_id == b->dependency_id && a->callee_id == b->callee_id &&
        a->actual_id == b->actual_id && a->formal_root_id == b->formal_root_id &&
        a->formal_binding_id == b->formal_binding_id &&
        a->caller_formal_id == b->caller_formal_id &&
        a->dependency_formal_id == b->dependency_formal_id &&
        a->projected_receiver_id == b->projected_receiver_id &&
        a->projected_index_id == b->projected_index_id &&
        a->projected_field_id == b->projected_field_id &&
        a->projected_decl_id == b->projected_decl_id &&
        a->risk_origin_id == b->risk_origin_id &&
        a->risk_source_binding_id == b->risk_source_binding_id &&
        a->risk_origin == b->risk_origin &&
        a->receiver_origin_id == b->receiver_origin_id &&
        a->receiver_source_binding_id == b->receiver_source_binding_id &&
        a->receiver_origin == b->receiver_origin &&
        a->receiver_elements == b->receiver_elements &&
        a->receiver_disposition == b->receiver_disposition &&
        a->caller_formal == b->caller_formal &&
        a->dependency_actual == b->dependency_actual &&
        a->dependency_formal == b->dependency_formal &&
        a->source_shape == b->source_shape && a->named == b->named;
}

IndexedStringPushRecord semantic_indexed_string_record_private_push(
    SemanticContext *ctx, ASTNode *call, ASTNode *source,
    ASTNode *receiver, uint32_t risk_binding_id)
{
    if (ctx == NULL || ctx->has_error || !semantic_param_publication_pending(ctx) ||
        call == NULL || call->type != AST_CALL || source == NULL ||
        receiver == NULL || receiver->type != AST_IDENTIFIER ||
        ast_call_arg_count(call) != 2 || ast_call_argument(call, 0) != receiver ||
        ast_call_argument(call, 1) != source ||
        !collection_call_is_unshadowed_builtin(call, "ArrayPush") ||
        ast_call_semantic_callee_decl_id(call) != 0 ||
        ast_call_semantic_callee_value_binding_id(call) != 0)
        return INDEXED_STRING_PUSH_NOT_APPLICABLE;
    uint32_t builtin = 0;
    /* Stdlib dispatch marks the completed call only after checking its
     * arguments. This pending identity gains no permission until replay. */
    if (!ast_call_semantic_callee_builtin_kind(call, &builtin) ||
        builtin != BUILTIN_NOT_BUILTIN) return INDEXED_STRING_PUSH_NOT_APPLICABLE;
    ASTNode *caller = ctx->current_function_decl;
    uint32_t caller_id = caller == NULL ? 0 : ast_node_stable_id(caller);
    const PgyCollectionOwnershipFact *risk = semantic_collection_ownership_fact_find(
        ctx, caller_id, risk_binding_id);
    if (caller == NULL || caller->type != AST_FUNC_DECL || caller_id == 0 ||
        risk_binding_id == 0 || risk == NULL ||
        risk->function_syntax_id != caller_id ||
        risk->binding_syntax_id != risk_binding_id ||
        risk->disposition != PGY_COLLECTION_DISPOSITION_LIVE ||
        risk->element_ownership == PGY_STRING_ARRAY_BORROWED_ELEMENTS)
        return INDEXED_STRING_PUSH_FAILED;
    Symbol *receiver_binding = NULL;
    const PgyCollectionOwnershipFact *receiver_fact =
        receiver_collection_fact(receiver, ctx, &receiver_binding);
    uint32_t receiver_id = receiver_binding == NULL ? 0 : receiver_binding->decl_syntax_id;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    PublishedNode *receiver_let = publication_node_by_id(store, receiver_id);
    const ASTNode *local = receiver_let == NULL ? NULL : receiver_let->node;
    const ASTNode *initial = local != NULL && local->type == AST_LET_DECL
        ? ast_let_initializer(local) : NULL;
    PublishedNode *source_row = publication_node(store, source, false);
    PublishedNode *receiver_row = publication_node(store, receiver, false);
    bool shape = source->type == AST_MEMBER_ACCESS ||
        source->type == AST_ARRAY_ACCESS || source->type == AST_CALL ||
        source->type == AST_IDENTIFIER;
    if (!shape) return INDEXED_STRING_PUSH_NOT_APPLICABLE;
    if (receiver_binding == NULL || receiver_binding->is_parameter ||
        receiver_binding->is_host_field || receiver_binding->is_consumed ||
        !receiver_binding->has_exclusive_array_storage ||
        receiver_binding->has_escaped_array_storage ||
        receiver_binding->has_pending_inout_preservation ||
        receiver_binding->has_pending_array_result ||
        !type_is_constructed_named(receiver_binding->type, "Array") ||
        !type_equals(type_get_constructed_arg(receiver_binding->type, 0), TYPE_STRING) ||
        receiver_id == 0 || receiver_id != ast_identifier_binding_syntax_id(receiver) ||
        local == NULL || local->type != AST_LET_DECL || ast_let_is_alias(local) ||
        ast_node_stable_id(local) != receiver_id ||
        initial == NULL || initial->type != AST_ARRAY_LITERAL ||
        receiver_row == NULL || !receiver_row->value_present || receiver_row->meta ||
        receiver_row->id != ast_node_stable_id(receiver) ||
        receiver_row->caller != caller || receiver_row->caller_id != caller_id ||
        receiver_row->value_type != receiver_binding->type ||
        source_row == NULL || !source_row->value_present || source_row->meta ||
        source_row->id != ast_node_stable_id(source) ||
        source_row->caller != caller || source_row->caller_id != caller_id ||
        source_row->value_type != TYPE_STRING ||
        !source_row->indexed_string_source_present ||
        source_row->indexed_string_source_id != UINT32_MAX)
        return INDEXED_STRING_PUSH_NOT_APPLICABLE;
    bool borrowed_receiver = receiver_fact != NULL &&
        receiver_fact->origin == PGY_COLLECTION_ORIGIN_BORROWED_LITERAL &&
        receiver_fact->element_ownership == PGY_STRING_ARRAY_BORROWED_ELEMENTS &&
        receiver_id != risk_binding_id && ast_array_literal_count(initial) != 0;
    bool empty_receiver = receiver_fact != NULL &&
        receiver_fact->origin == PGY_COLLECTION_ORIGIN_EMPTY_LITERAL &&
        receiver_fact->element_ownership == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN &&
        receiver_id == risk_binding_id && ast_array_literal_count(initial) == 0;
    if ((!borrowed_receiver && !empty_receiver) ||
        receiver_fact->function_syntax_id != caller_id ||
        receiver_fact->binding_syntax_id != receiver_id ||
        receiver_fact->origin_syntax_id != ast_node_stable_id(initial) ||
        receiver_fact->source_binding_syntax_id != 0 ||
        receiver_fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE)
        return INDEXED_STRING_PUSH_NOT_APPLICABLE;
    PendingPrivateStringPush r = {0};
    r.call = call; r.source = source; r.receiver = receiver; r.caller = caller;
    r.receiver_let = local; r.receiver_init = initial;
    r.call_id = ast_node_stable_id(call); r.source_id = ast_node_stable_id(source);
    r.receiver_id = ast_node_stable_id(receiver); r.caller_id = caller_id;
    r.receiver_let_id = receiver_id; r.receiver_init_id = ast_node_stable_id(initial);
    r.risk_binding_id = risk_binding_id; r.receiver_binding_id = receiver_id;
    r.risk_origin_id = risk->origin_syntax_id;
    r.risk_source_binding_id = risk->source_binding_syntax_id;
    r.risk_origin = risk->origin;
    r.receiver_origin_id = receiver_fact->origin_syntax_id;
    r.receiver_source_binding_id = receiver_fact->source_binding_syntax_id;
    r.receiver_origin = receiver_fact->origin;
    r.receiver_elements = receiver_fact->element_ownership;
    r.receiver_disposition = receiver_fact->disposition;
    r.source_shape = source->type;
    if (r.call_id == 0 || r.source_id == 0 || r.receiver_id == 0 ||
        r.receiver_init_id == 0) return INDEXED_STRING_PUSH_FAILED;
    if (source->type == AST_MEMBER_ACCESS || source->type == AST_ARRAY_ACCESS) {
        if (!semantic_param_publication_pending_projection_formal_root(
                ctx, caller, source, &r.formal_root, &r.caller_formal))
            return INDEXED_STRING_PUSH_NOT_APPLICABLE;
    } else {
        const ASTNode *dependency = source;
        if (source->type == AST_IDENTIFIER) {
            PublishedNode *let = publication_node_by_id(store,
                ast_identifier_binding_syntax_id(source));
            const IndexedStringLetLineage *lineage = let == NULL ? NULL : &let->indexed_string_let;
            if (lineage == NULL || !lineage->present || !lineage->available ||
                !lineage->dependent || lineage->declaration != let->node ||
                lineage->initializer == NULL || lineage->initializer->type != AST_CALL ||
                lineage->declaration_id != ast_identifier_binding_syntax_id(source) ||
                lineage->initializer_id != ast_node_stable_id(lineage->initializer) ||
                lineage->caller != caller || lineage->caller_id != caller_id ||
                !source_row->indexed_string_let.present)
                return INDEXED_STRING_PUSH_NOT_APPLICABLE;
            dependency = lineage->initializer;
        }
        PublishedNode *dep_row = publication_node(store, dependency, false);
        if (dependency == NULL || dependency->type != AST_CALL || dep_row == NULL ||
            !dep_row->call_present || !dep_row->declared_callable ||
            dep_row->declaration == 0 || dep_row->selected_callable == NULL ||
            dep_row->selected_callable_id != dep_row->declaration ||
            dep_row->selected_callable_id != ast_node_stable_id(dep_row->selected_callable) ||
            dep_row->arguments == NULL ||
            dep_row->argument_count != ast_call_arg_count(dependency))
            return INDEXED_STRING_PUSH_NOT_APPLICABLE;
        r.named = true; r.dependency = dependency;
        r.dependency_id = ast_node_stable_id(dependency);
        r.callee_id = dep_row->declaration;
        size_t matches = 0;
        for (size_t a = 0; a < dep_row->argument_count; a++) {
            const PublishedCallArgument *arg = &dep_row->arguments[a];
            const ASTNode *root = NULL;
            size_t formal = SIZE_MAX;
            if (!semantic_param_publication_pending_projection_formal_root(
                    ctx, caller, arg->node, &root, &formal)) continue;
            if (arg->node != ast_call_argument(dependency, a) ||
                arg->id == 0 || arg->id != ast_node_stable_id(arg->node) ||
                arg->formal_ordinal >= ast_func_param_count(dep_row->selected_callable) ||
                arg->formal_id == 0 ||
                arg->formal_id != ast_func_param_stable_id(
                    ast_func_param(dep_row->selected_callable, arg->formal_ordinal)))
                return INDEXED_STRING_PUSH_FAILED;
            matches++;
            r.actual = arg->node; r.actual_id = arg->id;
            r.dependency_actual = a; r.dependency_formal = arg->formal_ordinal;
            r.dependency_formal_id = arg->formal_id;
            r.formal_root = root; r.caller_formal = formal;
        }
        /* Ambiguous or unrelated named results retain the old b53 source
         * obligation. This receipt never guesses a later formal. */
        if (matches != 1) return INDEXED_STRING_PUSH_NOT_APPLICABLE;
    }
    PublishedNode *projected = publication_node(store,
        r.named ? r.actual : r.source, false);
    if (projected == NULL || !projected->projection.present)
        return INDEXED_STRING_PUSH_FAILED;
    r.projected_receiver_id = projected->projection.receiver_id;
    r.projected_index_id = projected->projection.index_id;
    r.projected_field_id = projected->projection.field_id;
    r.projected_decl_id = projected->projection.declaration_id;
    r.formal_root_id = ast_node_stable_id(r.formal_root);
    r.formal_binding_id = ast_identifier_binding_syntax_id(r.formal_root);
    r.caller_formal_id = ast_func_param_stable_id(
        ast_func_param(caller, r.caller_formal));
    if (r.formal_root_id == 0 || r.formal_binding_id == 0 ||
        r.caller_formal_id != r.formal_binding_id ||
        (r.named && (r.dependency_id == 0 || r.actual_id == 0)))
        return INDEXED_STRING_PUSH_FAILED;
    PublishedNode *call_row = publication_node(store, call, true);
    if (call_row == NULL) return INDEXED_STRING_PUSH_FAILED;
    if (call_row->private_string_push != NULL)
        return private_push_same(call_row->private_string_push, &r)
            ? INDEXED_STRING_PUSH_RECORDED : INDEXED_STRING_PUSH_FAILED;
    if (store->private_string_push_count == SIZE_MAX)
        return INDEXED_STRING_PUSH_FAILED;
    PendingPrivateStringPush *owned = malloc(sizeof(*owned));
    if (owned == NULL) return INDEXED_STRING_PUSH_FAILED;
    *owned = r;
    call_row->private_string_push = owned;
    store->private_string_push_count++;
    return INDEXED_STRING_PUSH_RECORDED;
}

bool semantic_indexed_string_prepare_private_push(SemanticContext *ctx, const ASTNode *call)
{
    if (ctx == NULL || ctx->has_error || !semantic_param_publication_ready(ctx)) return false;
    PublishedNode *row = publication_result_current(ctx, call);
    PendingPrivateStringPush *r = row == NULL ? NULL : row->private_string_push;
    if (r == NULL || r->call != call || r->call_id != ast_node_stable_id(call)) return false;
    if (r->prepared) return true;
    if (r->named && (r->dependency == NULL || r->dependency->type != AST_CALL ||
        r->dependency_id != ast_node_stable_id(r->dependency) ||
        !semantic_param_publication_checked_call(ctx, r->dependency) ||
        function_param_flow_declared_result_frame_current(ctx->function_param_flow_summaries,
            (ASTNode *)r->dependency, NULL) == NULL)) return false;
    uint32_t prepared_source = UINT32_MAX;
    /* Only this unprepared receipt may materialize; its answer grants nothing here. */
    if (r->named && !function_param_flow_string_result_source_prepare(
            ctx, r->dependency, &prepared_source) && ctx->function_param_flow_summaries->failed) return false;
    FunctionParamFlowProgramPointIndex *caller_index = NULL;
    if (!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries,
            (ASTNode *)r->caller, &caller_index) || caller_index == NULL) return false;
    uint32_t kind = 0, receiver = 0, source = 0;
    if (!ast_call_semantic_collection_effect(call, &kind, &receiver, &source) ||
        kind != PGY_COLLECTION_EFFECT_SHALLOW_MUTATION ||
        receiver != r->receiver_binding_id || source != 0) return false;
    /* Capture the post-effect cleanup state, not the empty initializer's state. */
    const PgyCollectionOwnershipFact *receiver_fact = semantic_collection_ownership_fact_find(
        ctx, r->caller_id, r->receiver_binding_id);
    if (receiver_fact == NULL || receiver_fact->function_syntax_id != r->caller_id ||
        receiver_fact->binding_syntax_id != r->receiver_binding_id ||
        receiver_fact->return_elements_owned ||
        receiver_fact->return_elements_source != NULL ||
        receiver_fact->return_elements_source_id != 0) return false;
    r->independent = r->named && function_param_flow_string_result_independent_current(
        ctx, r->dependency);
    if (!r->independent) {
        if (!(r->named ? function_param_flow_string_result_nonlocal_current(ctx,
                r->dependency, (ASTNode *)r->caller, r->caller_formal) :
                function_param_flow_nonlocal_input_current(ctx,
                    (ASTNode *)r->caller, r->caller_formal))) return false;
    }
    r->effect_kind = kind;
    r->effect_receiver_id = receiver;
    r->effect_source_id = source;
    r->prepared = true;
    return true;
}

static bool private_push_current_body(SemanticContext *ctx,
    const ASTNode *call)
{
    if (ctx == NULL || ctx->has_error || call == NULL ||
        !semantic_param_publication_ready(ctx)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    PublishedNode *row = publication_node(store, call, false);
    const PendingPrivateStringPush *r = row == NULL ? NULL : row->private_string_push;
    if (r == NULL || !r->prepared || row->node != call || r->call != call ||
        r->call_id == 0 || r->call_id != ast_node_stable_id(call) ||
        r->caller == NULL || row->caller != r->caller ||
        r->caller_id == 0 || r->caller_id != ast_node_stable_id(r->caller) ||
        r->source == NULL || r->source_id == 0 ||
        r->source_id != ast_node_stable_id(r->source) ||
        r->source_shape != r->source->type ||
        r->receiver == NULL || r->receiver_id == 0 ||
        r->receiver_id != ast_node_stable_id(r->receiver) ||
        ast_call_arg_count(call) != 2 ||
        ast_call_argument(call, 0) != r->receiver ||
        ast_call_argument(call, 1) != r->source ||
        r->receiver_binding_id == 0 ||
        ast_identifier_binding_syntax_id(r->receiver) != r->receiver_binding_id ||
        r->receiver_let == NULL || r->receiver_let_id != r->receiver_binding_id ||
        ast_node_stable_id(r->receiver_let) != r->receiver_let_id ||
        r->receiver_let->type != AST_LET_DECL ||
        ast_let_is_alias(r->receiver_let) ||
        r->receiver_init == NULL || r->receiver_init_id == 0 ||
        r->receiver_init_id != ast_node_stable_id(r->receiver_init) ||
        r->receiver_init->type != AST_ARRAY_LITERAL ||
        ast_let_initializer(r->receiver_let) != r->receiver_init ||
        r->risk_binding_id == 0 ||
        r->formal_root == NULL || r->formal_root_id == 0 ||
        r->formal_root_id != ast_node_stable_id(r->formal_root) ||
        r->formal_binding_id != ast_identifier_binding_syntax_id(r->formal_root) ||
        r->caller_formal >= ast_func_param_count(r->caller) ||
        r->caller_formal_id == 0 ||
        r->caller_formal_id != ast_func_param_stable_id(
            ast_func_param(r->caller, r->caller_formal)) ||
        r->caller_formal_id != r->formal_binding_id ||
        !semantic_param_publication_checked_call(ctx, call) ||
        !semantic_param_publication_checked_argument(ctx, call, 0) ||
        !semantic_param_publication_checked_argument(ctx, call, 1) ||
        !semantic_param_publication_private_array_push(ctx, call, r->caller) ||
        !function_param_flow_typed_expression_children_current(ctx,
            (ASTNode *)r->caller, call) ||
        !semantic_param_publication_routine_inputs_current(ctx, (ASTNode *)r->caller, true))
        return false;
    const PgyCollectionOwnershipFact *risk = semantic_collection_ownership_fact_find(
        ctx, r->caller_id, r->risk_binding_id);
    if (risk == NULL || risk->function_syntax_id != r->caller_id ||
        risk->binding_syntax_id != r->risk_binding_id ||
        risk->disposition != PGY_COLLECTION_DISPOSITION_LIVE ||
        risk->element_ownership == PGY_STRING_ARRAY_BORROWED_ELEMENTS ||
        risk->origin != r->risk_origin ||
        risk->origin_syntax_id != r->risk_origin_id ||
        risk->source_binding_syntax_id != r->risk_source_binding_id)
        return false;
    const PgyCollectionOwnershipFact *receiver_fact = semantic_collection_ownership_fact_find(
        ctx, r->caller_id, r->receiver_binding_id);
    if (receiver_fact == NULL || receiver_fact->function_syntax_id != r->caller_id ||
        receiver_fact->binding_syntax_id != r->receiver_binding_id ||
        receiver_fact->origin != r->receiver_origin ||
        receiver_fact->element_ownership != r->receiver_elements ||
        receiver_fact->disposition != r->receiver_disposition ||
        receiver_fact->origin_syntax_id != r->receiver_origin_id ||
        receiver_fact->source_binding_syntax_id != r->receiver_source_binding_id ||
        receiver_fact->return_elements_owned ||
        receiver_fact->return_elements_source != NULL ||
        receiver_fact->return_elements_source_id != 0 ||
        r->receiver_origin_id != r->receiver_init_id ||
        (r->receiver_origin == PGY_COLLECTION_ORIGIN_EMPTY_LITERAL &&
            (r->receiver_binding_id != r->risk_binding_id ||
             ast_array_literal_count(r->receiver_init) != 0)) ||
        (r->receiver_origin == PGY_COLLECTION_ORIGIN_BORROWED_LITERAL &&
            (r->receiver_binding_id == r->risk_binding_id ||
             ast_array_literal_count(r->receiver_init) == 0)) ||
        (r->receiver_origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL &&
         r->receiver_origin != PGY_COLLECTION_ORIGIN_BORROWED_LITERAL)) return false;
    uint32_t kind = 0, receiver = 0, source = 0;
    if (r->effect_kind != PGY_COLLECTION_EFFECT_SHALLOW_MUTATION ||
        r->effect_receiver_id != r->receiver_binding_id || r->effect_source_id != 0 ||
        !ast_call_semantic_collection_effect(call, &kind, &receiver, &source) ||
        kind != r->effect_kind || receiver != r->effect_receiver_id ||
        source != r->effect_source_id) return false;
    if (!r->independent) {
        if ((r->receiver_origin == PGY_COLLECTION_ORIGIN_EMPTY_LITERAL &&
             !function_param_flow_private_string_receiver_current(ctx,
                 (ASTNode *)r->caller, r->receiver_binding_id)) ||
            !function_param_flow_no_store_graph(ctx->function_param_flow_summaries,
                (ASTNode *)r->caller)) return false;
    }
    SemanticPublishedValue typed = semantic_param_publication_value(ctx, r->source);
    if (!typed.ok || typed.meta || typed.type != TYPE_STRING) return false;
    PublishedNode *source_row = publication_current_value(ctx, r->source);
    if (source_row == NULL || !source_row->indexed_string_source_present ||
        source_row->indexed_string_source_id != UINT32_MAX)
        return false;
    PublishedNode *projected = publication_current_value(ctx,
        r->named ? r->actual : r->source);
    if (projected == NULL || !projected->projection.present ||
        projected->projection.receiver_id != r->projected_receiver_id ||
        projected->projection.index_id != r->projected_index_id ||
        projected->projection.field_id != r->projected_field_id ||
        projected->projection.declaration_id != r->projected_decl_id)
        return false;
    ASTNode *actual_caller = NULL, *actual_root = NULL;
    if (!r->named) {
        if (r->source->type != AST_MEMBER_ACCESS &&
            r->source->type != AST_ARRAY_ACCESS) return false;
        return semantic_param_publication_projection_root_current(ctx,
            r->source, &actual_caller, &actual_root) &&
            actual_caller == r->caller && actual_root == r->formal_root &&
            function_param_flow_nonlocal_input_current(ctx,
                (ASTNode *)r->caller, r->caller_formal);
    }
    const ASTNode *dependency = r->source;
    if (r->source->type == AST_IDENTIFIER &&
        !semantic_indexed_string_borrow_result_dependency_current(
            ctx, (ASTNode *)r->source, &dependency)) return false;
    if (dependency != r->dependency || dependency == NULL ||
        dependency->type != AST_CALL || r->dependency_id == 0 ||
        r->dependency_id != ast_node_stable_id(dependency) ||
        r->callee_id == 0 ||
        ast_call_semantic_callee_decl_id(dependency) != r->callee_id ||
        r->dependency_actual >= ast_call_arg_count(dependency) ||
        ast_call_argument(dependency, r->dependency_actual) != r->actual ||
        r->actual_id == 0 || r->actual_id != ast_node_stable_id(r->actual) ||
        !semantic_param_publication_checked_call(ctx, dependency) ||
        !semantic_param_publication_checked_argument(ctx, dependency,
            r->dependency_actual)) return false;
    PublishedNode *dep_row = publication_result_current(ctx, dependency);
    size_t selected = SIZE_MAX;
    if (dep_row == NULL || dep_row->selected_callable == NULL ||
        dep_row->selected_callable_id != r->callee_id ||
        r->dependency_actual >= dep_row->argument_count ||
        dep_row->arguments[r->dependency_actual].node != r->actual ||
        dep_row->arguments[r->dependency_actual].id != r->actual_id ||
        dep_row->arguments[r->dependency_actual].formal_ordinal != r->dependency_formal ||
        dep_row->arguments[r->dependency_actual].formal_id != r->dependency_formal_id ||
        !semantic_param_publication_formal_for_actual(ctx, dependency,
            r->dependency_actual, &selected) || selected != r->dependency_formal ||
        selected >= ast_func_param_count(dep_row->selected_callable) ||
        r->dependency_formal_id != ast_func_param_stable_id(
            ast_func_param(dep_row->selected_callable, selected)) ||
        !semantic_param_publication_projection_root_current(ctx, r->actual,
            &actual_caller, &actual_root) ||
        actual_caller != r->caller || actual_root != r->formal_root)
        return false;
    /* The one-time preparation already produced the frame. A current query
     * must not rebuild a withdrawn epoch or reinterpret UNKNOWN as zero. */
    if (r->independent) {
        uint32_t source = UINT32_MAX;
        return function_param_flow_string_result_independent_current(ctx, dependency) &&
            semantic_indexed_string_borrow_source_current(
                (ASTNode *)r->source, ctx, &source) && source == 0;
    }
    return function_param_flow_string_result_nonlocal_current(ctx,
        dependency, (ASTNode *)r->caller, r->caller_formal);
}

bool semantic_indexed_string_private_push_current(SemanticContext *ctx,
    const ASTNode *call)
{
    bool previous = false;
    if (!function_param_flow_current_begin(ctx, &previous)) return false;
    bool current = private_push_current_body(ctx, call);
    function_param_flow_current_end(ctx, previous);
    return current;
}

bool
collection_binding_is_current_parameter(const SemanticContext *ctx,
                                        uint32_t binding_syntax_id)
{
    if (ctx == NULL || binding_syntax_id == 0) return false;
    ASTNode *function_decl = ctx->current_function_decl;
    if (function_decl == NULL || function_decl->type != AST_FUNC_DECL) return false;
    for (size_t i = 0; i < ast_func_param_count(function_decl); i++) {
        FuncParam *param = ast_func_param(function_decl, i);
        if (param != NULL && ast_func_param_stable_id(param) == binding_syntax_id)
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
