/* One all-typed publication boundary. Pending rows own diagnostic strings,
 * borrow compilation-owned AST/Type identity, and never retain scope Symbols. */
#include "function_param_publication_internal.h"
#include "typed_map_query_fact_owner.h"
#include "type_checker_internal.h"
#include "type_checker_resolution_internal.h"
#include "type_checker_ownership_internal.h"
#include "slot_analyzer_internal.h"
#include "function_param_flow_summary_internal.h"
#include "callable_capability_inference.h"
#include "region_retention_summary.h"
#include "indexed_string_borrow_owner.h"
#include "collection_ownership_fact.h"
#include "../common/string_compat.h"
#include <stdlib.h>
#include <string.h>
bool publication_physical_scalar(const Type *type)
{
    return type != NULL && type->kind == TYPE_KIND_PRIMITIVE &&
        (type == TYPE_INT || type == TYPE_LONG || type == TYPE_FLOAT ||
         type == TYPE_DOUBLE || type == TYPE_BOOL || type == TYPE_DURATION);
}
bool semantic_param_publication_begin(SemanticContext *ctx)
{
    if (ctx == NULL || ctx->program_root == NULL || ctx->program_root->type != AST_PROGRAM ||
        ast_node_stable_id(ctx->program_root) == 0 || ctx->function_param_publications != NULL ||
        ctx->function_param_flow_summaries != NULL) return false;
    ctx->function_param_publications = calloc(1, sizeof(*ctx->function_param_publications));
    if (ctx->function_param_publications == NULL) return false;
    ctx->function_param_publications->pending = true;
    ctx->function_param_publications->program = ctx->program_root;
    ctx->function_param_publications->program_id = ast_node_stable_id(ctx->program_root);
    ctx->function_param_publications->statement_count = ast_program_statement_count(ctx->program_root);
    return true;
}
bool publication_generation_ready(const SemanticContext *ctx)
{
    if (ctx == NULL || ctx->function_param_publications == NULL) return false;
    const FunctionParamPublicationStore *store = ctx->function_param_publications;
    return ctx->program_root == store->program && store->program_id != 0 &&
        ast_node_stable_id(ctx->program_root) == store->program_id &&
        ast_program_statement_count(ctx->program_root) == store->statement_count;
}
bool semantic_param_publication_pending(const SemanticContext *ctx)
{ return publication_generation_ready(ctx) && ctx->function_param_publications->pending; }
bool semantic_param_publication_inputs_ready(const SemanticContext *ctx)
{ return publication_generation_ready(ctx) && ctx->function_param_publications->inputs_ready; }
bool semantic_param_publication_ready(const SemanticContext *ctx)
{ return semantic_param_publication_inputs_ready(ctx) && ctx->function_param_publications->finalized; }
PublishedNode *publication_node(FunctionParamPublicationStore *store,
    const ASTNode *node, bool insert)
{
    uint32_t id = ast_node_stable_id(node);
    if (store == NULL || node == NULL || id == 0) return NULL;
    if (insert && (store->node_capacity == 0 || store->node_count >= store->node_capacity / 2)) {
        size_t capacity = store->node_capacity == 0 ? 64 : store->node_capacity * 2;
        if (capacity < store->node_capacity || capacity > SIZE_MAX / sizeof(PublishedNode)) return NULL;
        PublishedNode *rows = calloc(capacity, sizeof(*rows));
        if (rows == NULL) return NULL;
        for (size_t i = 0; i < store->node_capacity; i++) if (store->nodes[i].id != 0) {
            size_t slot = (store->nodes[i].id * (size_t)2654435761u) & (capacity - 1);
            while (rows[slot].id != 0) slot = (slot + 1) & (capacity - 1);
            rows[slot] = store->nodes[i];
        }
        free(store->nodes); store->nodes = rows; store->node_capacity = capacity;
    }
    if (store->node_capacity == 0) return NULL;
    size_t slot = (id * (size_t)2654435761u) & (store->node_capacity - 1);
    while (store->nodes[slot].id != 0 && store->nodes[slot].id != id)
        slot = (slot + 1) & (store->node_capacity - 1);
    PublishedNode *row = &store->nodes[slot];
    if (row->id != 0) return row->node == node ? row : NULL;
    if (!insert) return NULL;
    row->id = id; row->node = node; store->node_count++; return row;
}
bool semantic_param_publication_begin_value(SemanticContext *ctx, const ASTNode *value)
{
    if (!semantic_param_publication_pending(ctx)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, value, true);
    if (row == NULL) return false;
    semantic_param_publication_retire_value(row);
    return true;
}
bool semantic_param_publication_record_value(SemanticContext *ctx, ASTNode *value,
    const Type *type, bool meta)
{
    if (!semantic_param_publication_pending(ctx) || type == NULL || type == TYPE_UNKNOWN ||
        (type->kind == TYPE_KIND_PRIMITIVE && type->name != NULL &&
         (strcmp(type->name, "Unknown") == 0 || strcmp(type->name, "<unknown>") == 0))) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, value, true);
    if (row == NULL) return false;
    if (row->value_type_name != NULL || row->nominal_result != NULL) {
        char *name = pergyra_strdup(type->name);
        if (name == NULL) return false;
        free(row->value_type_name); row->value_type_name = name;
    }
    /* Category owns source value semantics. Origin separately owns whether
     * this value can escape without a backing loan or fresh copy landing. */
    row->category = (uint32_t)semantic_classify_ownership_type(type, ctx);
    row->value_shape = value->type;
    row->string_value = type_equals(type, TYPE_STRING);
    bool copy = publication_physical_scalar(type);
    row->value_present = true; row->copy_escape = copy;
    row->physical_scalar = copy;
    row->string_array_type = type_is_constructed_named(type, "Array") &&
        type_equals(type_get_constructed_arg(type, 0), TYPE_STRING) ? type : NULL;
    row->origin_ready = copy;
    if (row->string_value && value->type == AST_STRING &&
        semantic_indexed_string_independent_result(value))
        row->origin_ready = row->copy_escape = true;
    row->binding_id = value->type == AST_IDENTIFIER ? ast_identifier_binding_syntax_id(value) : 0;
    row->meta |= meta; return true;
}
bool semantic_param_publication_record_checked_numeric_cast(SemanticContext *ctx, ASTNode *cast)
{
    if (!semantic_param_publication_pending(ctx) || cast == NULL || cast->type != AST_CAST ||
        ast_cast_target_type(cast) == NULL) return false;
    PublishedNode *operand = publication_node(ctx->function_param_publications, ast_cast_operand(cast), false);
    if (operand == NULL || !operand->value_present || !operand->physical_scalar ||
        operand->value_shape != ast_cast_operand(cast)->type) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, cast, true);
    if (row == NULL) return false;
    char *name = pergyra_strdup(ast_cast_target_type(cast));
    if (name == NULL) return false;
    free(row->target_name); row->target_name = name;
    row->target = ast_cast_operand(cast); row->target_id = ast_node_stable_id(row->target);
    row->cast_present = row->target_id != 0;
    return row->cast_present;
}
bool semantic_param_publication_checked_numeric_cast(const SemanticContext *ctx, const ASTNode *cast)
{
    if (cast == NULL || cast->type != AST_CAST ||
        !semantic_param_publication_value_is_physical_scalar(ctx, cast)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, cast, false);
    const char *target = ast_cast_target_type(cast);
    return row != NULL && row->cast_present && row->target == ast_cast_operand(cast) &&
        row->target_id != 0 && row->target_id == ast_node_stable_id(row->target) &&
        row->target_name != NULL && target != NULL && strcmp(row->target_name, target) == 0 &&
        semantic_param_publication_value_is_physical_scalar(ctx, row->target);
}
bool semantic_param_publication_record_checked_result_relation(SemanticContext *ctx, ASTNode *call,
    const Type *argument_type, const Type *result_type, uint32_t selected_decl, size_t payload_ordinal)
{
    if (!semantic_param_publication_pending(ctx) || call == NULL || call->type != AST_CALL ||
        selected_decl != 0 || payload_ordinal != 0 || ast_call_arg_count(call) == 0 ||
        argument_type == NULL || result_type == NULL || argument_type == TYPE_UNKNOWN || result_type == TYPE_UNKNOWN ||
        argument_type->name == NULL || result_type->name == NULL) return false;
    const char *result_name = ast_call_semantic_value_type_name(call);
    if (result_name == NULL || strcmp(result_name, result_type->name) != 0) return false;
    ASTNode *argument = ast_call_argument(call, 0);
    PublishedNode *row = publication_node(ctx->function_param_publications, call, true);
    if (row == NULL) return false;
    PublishedNode *arg = publication_node(ctx->function_param_publications, argument, false);
    if (arg == NULL || !arg->value_present || arg->value_shape != argument->type) return false;
    char *a = pergyra_strdup(argument_type->name), *r = pergyra_strdup(result_type->name);
    char *av = pergyra_strdup(argument_type->name), *rv = pergyra_strdup(result_type->name);
    if (a == NULL || r == NULL || av == NULL || rv == NULL) { free(a); free(r); free(av); free(rv); return false; }
    free(row->relation_argument_type); free(row->relation_result_type); free(arg->value_type_name); free(row->value_type_name);
    row->relation_argument_type = a; row->relation_result_type = r; arg->value_type_name = av; row->value_type_name = rv;
    row->relation_argument = argument; row->relation_argument_id = ast_node_stable_id(argument);
    row->relation_argument_count = ast_call_arg_count(call);
    row->result_relation = row->relation_argument_id != 0;
    return row->result_relation;
}
bool semantic_param_publication_record_execution(SemanticContext *ctx,
    const ASTNode *statement, bool reachable)
{
    if (!semantic_param_publication_pending(ctx)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, statement, true);
    if (row == NULL) return false;
    row->execution_present = true; row->reachable |= reachable; return true;
}
bool semantic_param_publication_record_checked_call(SemanticContext *ctx, ASTNode *call)
{
    if (!semantic_param_publication_pending(ctx) || call == NULL || call->type != AST_CALL) return false;
    const ASTNode *target = ast_call_callee(call);
    uint32_t builtin = 0;
    bool kind_present = ast_call_semantic_callee_builtin_kind(call, &builtin);
    if (target == NULL || ast_node_stable_id(target) == 0) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, true);
    if (row == NULL) return false;
    size_t count = ast_call_arg_count(call);
    if (!row->call_present) {
        if (count > SIZE_MAX / sizeof(*row->arguments)) return false;
        free(row->arguments); free(row->target_name);
        row->arguments = count == 0 ? NULL : calloc(count, sizeof(*row->arguments));
        row->target_name = NULL;
        if (count != 0 && row->arguments == NULL) return false;
        const char *name = target->type == AST_IDENTIFIER ? ast_identifier_name(target) : NULL;
        row->target_name = pergyra_strdup(name);
        if (name != NULL && row->target_name == NULL) return false;
    } else if (row->argument_count != count || row->target != target) return false;
    row->target = target; row->target_id = ast_node_stable_id(target); row->builtin = builtin;
    row->target_shape = target->type; row->target_binding = ast_identifier_binding_syntax_id(target);
    row->kind_present = kind_present;
    row->caller = callable_capability_current_declaration(ctx);
    row->caller_id = ast_node_stable_id(row->caller);
    if (row->caller != NULL && row->caller_id == 0) return false;
    row->execution_present = true;
    row->reachable |= ctx->future_lifecycle_unreachable_depth == 0;
    row->declaration = ast_call_semantic_callee_decl_id(call);
    row->value_binding = ast_call_semantic_callee_value_binding_id(call);
    row->stdlib = ast_call_semantic_callee_is_stdlib(call);
    row->local = kind_present && row->declaration == 0 && row->value_binding == 0 &&
        (builtin != BUILTIN_NOT_BUILTIN || row->stdlib) && builtin != BUILTIN_WRITE && row->target_name != NULL &&
        slot_builtin_call_is_local_non_escape(row->target_name);
    for (size_t i = 0; i < count; i++) {
        const ASTNode *arg = ast_call_argument(call, i);
        if (arg == NULL || ast_node_stable_id(arg) == 0) return false;
        row->arguments[i] = (PublishedCallArgument){arg, ast_node_stable_id(arg), arg->type, ast_identifier_binding_syntax_id(arg)};
    }
    row->argument_count = count; row->call_present = true;
    if (row->value_present && row->string_value && semantic_indexed_string_independent_result(call))
        row->origin_ready = row->copy_escape = true;
    row->private_array_push = false;
    /* The flags are call-time receipts, not live Symbol authority. In
     * particular ref->Void calls need the separate whole closed-call proof. */
    if (!ctx->has_error && count == 2 && row->stdlib && row->kind_present &&
        row->builtin == BUILTIN_NOT_BUILTIN && row->declaration == 0 && row->value_binding == 0 &&
        row->target_shape == AST_IDENTIFIER && row->target_binding == 0 &&
        row->target_name != NULL && strcmp(row->target_name, "ArrayPush") == 0) {
        ASTNode *receiver = ast_call_argument(call, 0);
        Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
            ? lookup_identifier_symbol(receiver, ctx) : NULL;
        const PgyCollectionOwnershipFact *fact = binding == NULL ? NULL :
            semantic_collection_ownership_fact_find(ctx, row->caller_id, binding->decl_syntax_id);
        uint32_t effect = 0, effect_binding = 0, source = 0;
        FunctionParamPublicationStore *store = ctx->function_param_publications;
        uint32_t id = binding == NULL ? 0 : binding->decl_syntax_id;
        size_t slot = (id * (size_t)2654435761u) & (store->node_capacity - 1);
        while (store->nodes[slot].id != 0 && store->nodes[slot].id != id)
            slot = (slot + 1) & (store->node_capacity - 1);
        const ASTNode *local = id != 0 && store->nodes[slot].id == id ? store->nodes[slot].node : NULL;
        const ASTNode *initializer = local != NULL && local->type == AST_LET_DECL ? ast_let_initializer(local) : NULL;
        PublishedNode *initial_value = publication_node(store, initializer, false);
        PublishedNode *receiver_value = publication_node(store, receiver, false);
        if (binding != NULL && !binding->is_parameter && !binding->is_host_field &&
            binding->has_exclusive_array_storage && !binding->has_escaped_array_storage &&
            !binding->has_pending_inout_preservation && id == ast_identifier_binding_syntax_id(receiver) &&
            type_is_constructed_named(binding->type, "Array") &&
            type_equals(type_get_constructed_arg(binding->type, 0), TYPE_STRING) &&
            local != NULL && !ast_let_is_alias(local) && initializer != NULL &&
            ast_let_type(local) != NULL && ast_let_type(local)->type == AST_TYPE &&
            ast_node_stable_id(ast_let_type(local)) != 0 &&
            initializer->type == AST_ARRAY_LITERAL && ast_array_literal_count(initializer) == 0 &&
            initial_value != NULL && initial_value->value_present && initial_value->string_array_type != NULL &&
            receiver_value != NULL && receiver_value->value_present && receiver_value->string_array_type != NULL &&
            fact != NULL && fact->origin == PGY_COLLECTION_ORIGIN_EMPTY_LITERAL &&
            fact->origin_syntax_id == ast_node_stable_id(initializer) &&
            fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE &&
            ast_call_semantic_collection_effect(call, &effect, &effect_binding, &source) && effect_binding == id &&
            (effect == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION || effect == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH)) {
            row->private_binding = local; row->private_binding_id = id;
            row->private_binding_type_node = ast_let_type(local);
            row->private_binding_type_id = ast_node_stable_id(row->private_binding_type_node);
            row->private_initializer = initializer; row->private_initializer_id = ast_node_stable_id(initializer);
            row->private_effect = effect; row->private_effect_source = source;
            row->private_exclusive = true; row->private_escaped = false; row->private_pending = false;
            row->private_array_push = true;
        }
    }
    return true;
}
bool publication_call_ready(const PublishedNode *row)
{
    uint32_t builtin = 0;
    if (row == NULL || !row->call_present || row->node->type != AST_CALL || row->target == NULL ||
        row->id != ast_node_stable_id(row->node) || row->target != ast_call_callee(row->node) ||
        row->caller_id != ast_node_stable_id(row->caller) ||
        row->target_id != ast_node_stable_id(row->target) ||
        row->target_shape != row->target->type || row->target_binding != ast_identifier_binding_syntax_id(row->target) ||
        ast_call_semantic_callee_builtin_kind(row->node, &builtin) != row->kind_present ||
        (row->kind_present && builtin != row->builtin) ||
        row->declaration != ast_call_semantic_callee_decl_id(row->node) ||
        row->value_binding != ast_call_semantic_callee_value_binding_id(row->node) ||
        row->stdlib != ast_call_semantic_callee_is_stdlib(row->node) ||
        row->argument_count != ast_call_arg_count(row->node) ||
        (row->argument_count != 0 && row->arguments == NULL)) return false;
    for (size_t i = 0; i < row->argument_count; i++) {
        const PublishedCallArgument *arg = &row->arguments[i];
        if (arg->node != ast_call_argument(row->node, i) || arg->node == NULL || arg->shape != arg->node->type ||
            arg->id != ast_node_stable_id(arg->node) || arg->binding != ast_identifier_binding_syntax_id(arg->node)) return false;
    }
    return row->target_name == NULL || (row->target->type == AST_IDENTIFIER &&
        ast_identifier_name(row->target) != NULL && strcmp(row->target_name, ast_identifier_name(row->target)) == 0);
}
PublishedNode *publication_current_value(const SemanticContext *ctx, const ASTNode *value)
{
    if (!semantic_param_publication_inputs_ready(ctx)) return NULL;
    PublishedNode *row = publication_node(ctx->function_param_publications, value, false);
    if (row == NULL || !row->value_present || row->value_shape != value->type ||
        (value->type == AST_CALL && !semantic_param_publication_checked_call(ctx, value)) || (value->type == AST_IDENTIFIER &&
        row->binding_id != ast_identifier_binding_syntax_id(value))) return NULL;
    return row;
}
bool semantic_param_publication_record_nominal_result(SemanticContext *ctx,
    ASTNode *call, SemanticNominalResultSnapshot *snapshot)
{
    if (snapshot == NULL || (snapshot->field_count != 0 && snapshot->fields == NULL) || snapshot->result_type_name == NULL ||
        snapshot->field_count != ast_call_arg_count(call) ||
        !semantic_param_publication_record_checked_call(ctx, call)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    if (row == NULL || row->nominal_result != NULL) return false;
    for (size_t i = 0; i < row->argument_count; i++) {
        PublishedNode *arg = publication_node(ctx->function_param_publications, row->arguments[i].node, false);
        if (arg == NULL || !arg->value_present ||
            arg->value_shape != arg->node->type || snapshot->fields[i].argument_type_name == NULL ||
            (arg->node->type == AST_IDENTIFIER && arg->binding_id != ast_identifier_binding_syntax_id(arg->node)) ||
            (arg->node->type == AST_CALL && !publication_call_ready(arg))) return false;
        char *name = pergyra_strdup(snapshot->fields[i].argument_type_name);
        if (name == NULL) return false;
        free(arg->value_type_name); arg->value_type_name = name;
    }
    row->nominal_result = snapshot;
    return true;
}
bool semantic_param_publication_checked_result_relation(const SemanticContext *ctx, const ASTNode *call,
    size_t ordinal, bool *result_depends, bool *external_retains)
{
    if (result_depends == NULL || external_retains == NULL) return false;
    PublishedNode *row = publication_current_value(ctx, call);
    if (row == NULL || ordinal >= row->argument_count || (row->nominal_result != NULL &&
        (row->value_type_name == NULL ||
         !semantic_nominal_result_snapshot_current(ctx, call, row->nominal_result) ||
         strcmp(row->value_type_name, row->nominal_result->result_type_name) != 0))) return false;
    for (size_t i = 0; i < row->argument_count; i++) {
        PublishedNode *value = publication_current_value(ctx, row->arguments[i].node);
        if (value == NULL ||
            (row->nominal_result != NULL && (value->value_type_name == NULL ||
             strcmp(value->value_type_name, row->nominal_result->fields[i].argument_type_name) != 0 ||
             (value->node->type == AST_CALL && ast_call_semantic_value_type_name(value->node) != NULL &&
              strcmp(value->value_type_name, ast_call_semantic_value_type_name(value->node)) != 0)))) return false;
    }
    if (row->nominal_result != NULL) {
        *result_depends = true; *external_retains = false; return true;
    }
    PublishedNode *arg = publication_current_value(ctx, ast_call_argument(call, 0));
    const char *result = ast_call_semantic_value_type_name(call);
    if (row == NULL || arg == NULL || !row->result_relation || !row->kind_present ||
        row->builtin != BUILTIN_NOT_BUILTIN || !row->stdlib || row->declaration != 0 || row->value_binding != 0 ||
        row->argument_count != row->relation_argument_count || row->relation_argument != arg->node || row->relation_argument_id != arg->id ||
        row->relation_argument_type == NULL || row->relation_result_type == NULL || arg->value_type_name == NULL ||
        row->value_type_name == NULL || result == NULL || strcmp(row->relation_argument_type, arg->value_type_name) != 0 ||
        strcmp(row->relation_result_type, row->value_type_name) != 0 || strcmp(result, row->relation_result_type) != 0 ||
        (arg->node->type == AST_CALL && (ast_call_semantic_value_type_name(arg->node) == NULL ||
         strcmp(arg->value_type_name, ast_call_semantic_value_type_name(arg->node)) != 0))) return false;
    *result_depends = ordinal == 0; *external_retains = false;
    return true;
}
SemanticPublishedValue semantic_param_publication_value(const SemanticContext *ctx, const ASTNode *value)
{
    PublishedNode *row = publication_current_value(ctx, value);
    if (row == NULL) return (SemanticPublishedValue){0};
    return (SemanticPublishedValue){true, row->copy_escape, row->meta, row->category, row->origin_ready};
}
bool semantic_param_publication_execution(const SemanticContext *ctx,
    const ASTNode *statement, bool *reachable)
{
    if (reachable == NULL || !semantic_param_publication_inputs_ready(ctx)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, statement, false);
    if (row == NULL || !row->execution_present) return false;
    *reachable = row->reachable; return true;
}
bool semantic_param_publication_checked_argument(const SemanticContext *ctx, const ASTNode *call, size_t ordinal)
{
    return semantic_param_publication_checked_call(ctx, call) && ordinal < ast_call_arg_count(call);
}
bool semantic_param_publication_checked_call(const SemanticContext *ctx, const ASTNode *call)
{
    if (!semantic_param_publication_inputs_ready(ctx)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    return publication_call_ready(row);
}
bool semantic_param_publication_argument_borrowed_for_call(const SemanticContext *ctx, const ASTNode *call, size_t ordinal)
{
    if (!semantic_param_publication_checked_argument(ctx, call, ordinal)) return false;
    if (!semantic_param_publication_value(ctx, ast_call_argument(call, ordinal)).ok) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    PgyRegionRetentionKind retention = PGY_REGION_RETENTION_UNKNOWN;
    if (row->declaration != 0 || row->value_binding != 0 || !row->kind_present) return false;
    return (row->builtin != BUILTIN_NOT_BUILTIN ?
        semantic_region_retention_summary_for_builtin(row->builtin, ordinal, &retention) :
        semantic_region_retention_summary_for_stdlib(call, ordinal, &retention)) && retention == PGY_REGION_RETENTION_BORROWED_FOR_CALL;
}
bool semantic_param_publication_local_call(const SemanticContext *ctx, const ASTNode *call, size_t ordinal)
{
    if (!semantic_param_publication_checked_argument(ctx, call, ordinal)) return false;
    return publication_node(ctx->function_param_publications, call, false)->local ||
        semantic_param_publication_argument_borrowed_for_call(ctx, call, ordinal);
}
bool semantic_param_publication_string_argument_read_only(
    const SemanticContext *ctx, const ASTNode *call, size_t ordinal)
{
    if (!semantic_param_publication_argument_borrowed_for_call(ctx, call, ordinal) ||
        ast_call_callee(call) == NULL || ast_call_callee(call)->type != AST_IDENTIFIER) return false;
    PublishedNode *value = publication_current_value(ctx, ast_call_argument(call, ordinal));
    /* Registered String operands are const-input reads or copied inputs.
     * Mutation-capable receivers such as Write's Slot are not String values. */
    return value != NULL && value->string_value;
}
uint32_t semantic_param_publication_binding(const SemanticContext *ctx, const ASTNode *value)
{
    return value != NULL && value->type == AST_IDENTIFIER && semantic_param_publication_value(ctx, value).ok ?
        ast_identifier_binding_syntax_id(value) : 0;
}
bool semantic_param_publication_record_assignment_replacement(SemanticContext *ctx,
    ASTNode *assignment, uint32_t binding, const Type *target_type)
{
    if (!semantic_param_publication_pending(ctx) || assignment == NULL ||
        assignment->type != AST_ASSIGNMENT || binding == 0 || target_type == NULL ||
        target_type == TYPE_UNKNOWN || type_is_slot_handle(target_type) ||
        type_is_resource_handle(target_type)) return false;
    ASTNode *target = ast_assignment_target(assignment), *value = ast_assignment_value(assignment);
    const char *type_name = ast_assignment_semantic_binding_type_name(assignment);
    if (target == NULL || target->type != AST_IDENTIFIER || value == NULL ||
        ast_identifier_binding_syntax_id(target) != binding || ast_node_stable_id(target) == 0 ||
        ast_node_stable_id(value) == 0 || type_name == NULL || target_type->name == NULL ||
        strcmp(type_name, target_type->name) != 0) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, assignment, true);
    char *name = pergyra_strdup(type_name);
    if (row == NULL || name == NULL) { free(name); return false; }
    free(row->target_name); row->target_name = name;
    row->target = target; row->target_id = ast_node_stable_id(target);
    row->target_shape = target->type; row->target_binding = binding;
    row->assignment_rhs = value; row->assignment_rhs_id = ast_node_stable_id(value);
    row->assignment_rhs_shape = value->type;
    row->assignment_rhs_binding = ast_identifier_binding_syntax_id(value);
    row->assignment_replacement = true;
    return true;
}
bool semantic_param_publication_assignment_replacement(const SemanticContext *ctx, const ASTNode *assignment)
{
    PublishedNode *row = publication_current_value(ctx, assignment);
    if (row == NULL || !row->assignment_replacement || assignment->type != AST_ASSIGNMENT ||
        row->target == NULL || row->target != ast_assignment_target(assignment) ||
        row->target->type != AST_IDENTIFIER || row->target_shape != row->target->type ||
        row->target_id == 0 || row->target_id != ast_node_stable_id(row->target) ||
        row->target_binding == 0 || row->target_binding != semantic_param_publication_binding(ctx, row->target) ||
        row->assignment_rhs == NULL || row->assignment_rhs != ast_assignment_value(assignment) ||
        row->assignment_rhs_id == 0 || row->assignment_rhs_id != ast_node_stable_id(row->assignment_rhs) ||
        row->assignment_rhs_shape != row->assignment_rhs->type ||
        row->assignment_rhs_binding != ast_identifier_binding_syntax_id(row->assignment_rhs) ||
        !semantic_param_publication_value(ctx, row->assignment_rhs).ok || row->target_name == NULL ||
        ast_assignment_semantic_binding_type_name(assignment) == NULL ||
        strcmp(row->target_name, ast_assignment_semantic_binding_type_name(assignment)) != 0) return false;
    return true;
}
bool semantic_param_publication_value_call_target(const SemanticContext *ctx, const ASTNode *call)
{
    if (!semantic_param_publication_checked_call(ctx, call)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    if (row == NULL || row->value_binding != 0 || row->target == NULL ||
        row->target->type != AST_IDENTIFIER) return false;
    if (row->nominal_result != NULL) {
        bool depends = false, retains = true;
        return semantic_nominal_result_snapshot_current(ctx, call, row->nominal_result) &&
            (row->argument_count == 0 ||
             (semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) && !retains));
    }
    return row->kind_present && row->declaration == 0 &&
        (row->builtin != BUILTIN_NOT_BUILTIN || row->stdlib);
}
bool semantic_param_publication_private_array_push(const SemanticContext *ctx,
    const ASTNode *call, const ASTNode *owner)
{
    if (!semantic_param_publication_checked_call(ctx, call)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    if (row == NULL || !row->private_array_push || row->caller == NULL ||
        (owner != NULL && row->caller != owner) ||
        row->caller_id != ast_node_stable_id(row->caller) || !row->private_exclusive ||
        row->private_escaped || row->private_pending || row->argument_count != 2 ||
        !row->stdlib || !row->kind_present || row->builtin != BUILTIN_NOT_BUILTIN ||
        row->declaration != 0 || row->value_binding != 0 || row->target_binding != 0 ||
        row->target_name == NULL || strcmp(row->target_name, "ArrayPush") != 0) return false;
    const ASTNode *local = row->private_binding, *initial = row->private_initializer;
    const ASTNode *receiver = ast_call_argument(call, 0);
    if (local == NULL || local->type != AST_LET_DECL || ast_let_is_alias(local) ||
        ast_node_stable_id(local) != row->private_binding_id || initial == NULL ||
        ast_let_type(local) != row->private_binding_type_node ||
        row->private_binding_type_node == NULL || row->private_binding_type_node->type != AST_TYPE ||
        ast_node_stable_id(row->private_binding_type_node) != row->private_binding_type_id ||
        ast_let_initializer(local) != initial || initial->type != AST_ARRAY_LITERAL ||
        ast_node_stable_id(initial) != row->private_initializer_id || ast_array_literal_count(initial) != 0 ||
        receiver == NULL || receiver->type != AST_IDENTIFIER ||
        ast_identifier_binding_syntax_id(receiver) != row->private_binding_id) return false;
    PublishedNode *iv = publication_current_value(ctx, initial), *rv = publication_current_value(ctx, receiver);
    if (iv == NULL || rv == NULL || iv->meta || rv->meta ||
        !type_is_constructed_named(iv->string_array_type, "Array") ||
        !type_equals(type_get_constructed_arg(iv->string_array_type, 0), TYPE_STRING) ||
        !type_is_constructed_named(rv->string_array_type, "Array") ||
        !type_equals(type_get_constructed_arg(rv->string_array_type, 0), TYPE_STRING)) return false;
    const PgyCollectionOwnershipFact *fact = semantic_collection_ownership_fact_find(
        ctx, row->caller_id, row->private_binding_id);
    uint32_t effect = 0, binding = 0, source = 0;
    return fact != NULL && fact->origin == PGY_COLLECTION_ORIGIN_EMPTY_LITERAL &&
        fact->origin_syntax_id == row->private_initializer_id &&
        fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE &&
        ast_call_semantic_collection_effect(call, &effect, &binding, &source) &&
        binding == row->private_binding_id && effect == row->private_effect && source == row->private_effect_source &&
        (effect == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION || effect == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH);
}
