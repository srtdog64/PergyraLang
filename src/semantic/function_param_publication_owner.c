/* One all-typed publication boundary. Pending rows own diagnostic strings,
 * borrow compilation-owned AST/Type identity, and never retain scope Symbols. */
#include "function_param_publication_owner.h"
#include "typed_map_query_fact_owner.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"
#include "slot_analyzer_internal.h"
#include "callable_capability_inference.h"
#include "region_retention_summary.h"
#include "indexed_string_borrow_owner.h"
#include "../common/string_compat.h"
#include <stdlib.h>
#include <string.h>
static PendingRoutine *publication_routine(FunctionParamPublicationStore *store, uint32_t id);
static bool publication_physical_scalar(const Type *type)
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
static bool publication_generation_ready(const SemanticContext *ctx)
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
static PublishedNode *publication_node(FunctionParamPublicationStore *store,
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
    return true;
}
static bool publication_call_ready(const PublishedNode *row)
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
static PublishedNode *publication_current_value(const SemanticContext *ctx, const ASTNode *value)
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
uint32_t semantic_param_publication_binding(const SemanticContext *ctx, const ASTNode *value)
{
    return value != NULL && value->type == AST_IDENTIFIER && semantic_param_publication_value(ctx, value).ok ?
        ast_identifier_binding_syntax_id(value) : 0;
}
bool semantic_param_publication_record_routine(SemanticContext *ctx, ASTNode *decl,
    size_t count, Type **types, Type *function_type)
{
    if (!semantic_param_publication_pending(ctx) || decl == NULL || ast_node_stable_id(decl) == 0 ||
        decl->type != AST_FUNC_DECL || function_type == NULL || count != ast_func_param_count(decl) ||
        count != type_function_param_count(function_type) || (count != 0 && types == NULL)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    PublishedNode *node = publication_node(store, decl, true);
    if (node == NULL) return false;
    for (size_t p = 0; p < count; p++)
        if (types[p] == NULL || !type_equals(types[p], type_function_param_type(function_type, p)) ||
            ast_func_param(decl, p) == NULL || ast_func_param_stable_id(ast_func_param(decl, p)) == 0 ||
            ast_func_param(decl, p)->mode != type_function_param_mode(function_type, p)) return false;
    if (node->routine_index != 0) {
        if (node->routine_index > store->routine_count) return false;
        PendingRoutine *prior = &store->routines[node->routine_index - 1];
        if (prior->decl != decl || prior->function_type != function_type || prior->count != count) return false;
        if (prior->body != ast_func_body(decl) || prior->body_id != ast_node_stable_id(prior->body)) return false;
        for (size_t p = 0; p < count; p++) if (prior->types[p] != types[p] ||
            prior->formals[p].id != ast_func_param_stable_id(ast_func_param(decl, p)) ||
            prior->formals[p].mode != ast_func_param(decl, p)->mode) return false;
        return true;
    }
    if (count > SIZE_MAX / sizeof(*types) || count > SIZE_MAX / sizeof(PublishedFormal) ||
        !semantic_param_publication_reserve_storage((void **)&store->routines,
            &store->routine_capacity, store->routine_count, sizeof(*store->routines))) return false;
    Type **copy = count == 0 ? NULL : malloc(count * sizeof(*copy));
    PublishedFormal *formals = count == 0 ? NULL : malloc(count * sizeof(*formals));
    if (count != 0 && (copy == NULL || formals == NULL)) { free(copy); free(formals); return false; }
    if (count != 0) memcpy(copy, types, count * sizeof(*copy));
    for (size_t p = 0; p < count; p++) formals[p] = (PublishedFormal){
        ast_func_param_stable_id(ast_func_param(decl, p)), ast_func_param(decl, p)->mode,
        ast_func_param(decl, p)->type, ast_node_stable_id(ast_func_param(decl, p)->type),
        types[p]->kind, type_is_anchored_resource_handle(types[p])};
    char *module = pergyra_strdup(ctx->current_module_path);
    if (ctx->current_module_path != NULL && module == NULL) { free(copy); free(formals); return false; }
    store->routines[store->routine_count++] = (PendingRoutine){decl, function_type, copy, count,
        ast_node_stable_id(decl), module, formals, ast_func_body(decl), ast_node_stable_id(ast_func_body(decl)),
        type_function_return_type(function_type), ast_func_return_type(decl),
        ast_node_stable_id(ast_func_return_type(decl)), decl->is_async_decl};
    node->routine_index = store->routine_count;
    return true;
}
bool semantic_param_publication_routine_inputs_current(const SemanticContext *ctx, const ASTNode *decl, bool require_body)
{
    if (!publication_generation_ready(ctx)) return false;
    const PendingRoutine *row = publication_routine(ctx->function_param_publications, ast_node_stable_id(decl));
    if (row == NULL || decl == NULL || decl->type != AST_FUNC_DECL || row->decl != decl ||
        row->id != ast_node_stable_id(decl) || row->count != ast_func_param_count(decl) ||
        row->count != type_function_param_count(row->function_type) || row->body != ast_func_body(decl) ||
        row->body_id != ast_node_stable_id(row->body) ||
        (require_body && (row->body == NULL || row->body_id == 0)) ||
        !type_function_has_body_summary(row->function_type)) return false;
    for (size_t p = 0; p < row->count; p++) {
        FuncParam *param = ast_func_param(decl, p);
        if (param == NULL || row->formals[p].id != ast_func_param_stable_id(param) ||
            row->formals[p].type_node != param->type ||
            row->formals[p].type_node_id != ast_node_stable_id(param->type) ||
            row->formals[p].mode != param->mode || param->mode != type_function_param_mode(row->function_type, p) ||
            !type_equals(row->types[p], type_function_param_type(row->function_type, p))) return false;
    }
    return true;
}
bool semantic_param_publication_parameter_is_anchored_handle(
    const SemanticContext *ctx, const ASTNode *decl, size_t ordinal, bool *anchored)
{
    if (!semantic_param_publication_inputs_ready(ctx) || anchored == NULL) return false;
    const PendingRoutine *row = publication_routine(ctx->function_param_publications, ast_node_stable_id(decl));
    if (!semantic_param_publication_routine_inputs_current(ctx, decl, true) || ordinal >= row->count) return false;
    const PublishedFormal *formal = &row->formals[ordinal];
    if (formal->id == 0 || formal->type_node == NULL || formal->type_node_id == 0 ||
        row->types[ordinal] == TYPE_UNKNOWN || row->types[ordinal] != type_function_param_type(row->function_type, ordinal) ||
        formal->kind != row->types[ordinal]->kind || formal->anchored != type_is_anchored_resource_handle(row->types[ordinal])) return false;
    *anchored = formal->anchored;
    return true;
}
bool semantic_param_publication_routine_relation_header_ready(
    const SemanticContext *ctx, const ASTNode *decl)
{
    if (!semantic_param_publication_inputs_ready(ctx)) return false;
    const PendingRoutine *row = publication_routine(ctx->function_param_publications, ast_node_stable_id(decl));
    if (!semantic_param_publication_routine_inputs_current(ctx, decl, true) || row->async || decl->is_async_decl ||
        row->return_type == NULL || row->return_type == TYPE_UNKNOWN ||
        row->return_type != type_function_return_type(row->function_type) ||
        row->return_node != ast_func_return_type(decl) ||
        row->return_node_id != ast_node_stable_id(row->return_node)) return false;
    for (size_t p = 0; p < row->count; p++) {
        bool anchored;
        if (!semantic_param_publication_parameter_is_anchored_handle(ctx, decl, p, &anchored)) return false;
    }
    return true;
}
bool semantic_param_publication_routine_no_output_header_ready(const SemanticContext *ctx, const ASTNode *decl)
{
    if (!semantic_param_publication_routine_relation_header_ready(ctx, decl)) return false;
    for (size_t p = 0; p < ast_func_param_count(decl); p++)
        if (ast_func_param(decl, p)->mode != PARAM_MODE_REF && ast_func_param(decl, p)->mode != PARAM_MODE_DEFAULT) return false;
    return true;
}
bool semantic_param_publication_routine_return_dependency_is_scalar(
    const SemanticContext *ctx, const ASTNode *decl)
{
    if (!semantic_param_publication_routine_no_output_header_ready(ctx, decl)) return false;
    const PendingRoutine *row = publication_routine(ctx->function_param_publications, ast_node_stable_id(decl));
    return row->return_type == TYPE_VOID || publication_physical_scalar(row->return_type);
}
bool semantic_param_publication_value_is_physical_scalar(const SemanticContext *ctx, const ASTNode *value)
{
    PublishedNode *row = publication_current_value(ctx, value);
    return row != NULL && row->physical_scalar;
}
static bool publication_ref_call_tuple(const ASTNode *call, const ASTNode *site,
    const ASTNode *callee, size_t ordinal)
{
    return call != NULL && call->type == AST_CALL && site != NULL && callee != NULL &&
        callee->type == AST_FUNC_DECL && ast_node_stable_id(call) != 0 &&
        ast_node_stable_id(site) != 0 && ast_node_stable_id(callee) != 0 &&
        ordinal < ast_func_param_count(callee) && ordinal < ast_call_arg_count(call) &&
        ast_call_argument(call, ordinal) == site &&
        ast_call_semantic_callee_decl_id(call) == ast_node_stable_id(callee) &&
        ast_call_semantic_callee_value_binding_id(call) == 0 && ast_func_param(callee, ordinal) != NULL &&
        ast_func_param(callee, ordinal)->mode == PARAM_MODE_REF;
}
bool semantic_param_publication_record_call(SemanticContext *ctx, ASTNode *call, ASTNode *site,
    ASTNode *callee, size_t ordinal, const Type *type, const char *borrowed,
    const char *destination, const char *fix)
{
    if (!semantic_param_publication_pending(ctx) || !publication_ref_call_tuple(call, site, callee, ordinal) ||
        (ctx->current_function_decl != NULL && ast_node_stable_id(ctx->current_function_decl) == 0) ||
        type == NULL || borrowed == NULL) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    if (!semantic_param_publication_reserve_storage((void **)&store->calls, &store->call_capacity,
            store->call_count, sizeof(*store->calls))) return false;
    PendingRefCall row = {call, site, callee, ctx->current_function_decl, ordinal, type,
        pergyra_strdup(borrowed), pergyra_strdup(destination), pergyra_strdup(fix),
        ast_node_stable_id(call), ast_node_stable_id(site), ast_node_stable_id(callee),
        ctx->current_function_decl == NULL ? 0 : ast_node_stable_id(ctx->current_function_decl),
        pergyra_strdup(ctx->current_module_path), ast_call_callee(call),
        ast_node_stable_id(ast_call_callee(call)), ast_call_arg_count(call)};
    if (row.call_target == NULL || row.call_target_id == 0 ||
        (row.call_target->type != AST_IDENTIFIER && row.call_target->type != AST_MEMBER_ACCESS) ||
        row.borrowed == NULL || (destination != NULL && row.destination == NULL) || (fix != NULL && row.fix == NULL) ||
        (ctx->current_module_path != NULL && row.module == NULL)) {
        free(row.borrowed); free(row.destination); free(row.fix); free(row.module); return false;
    }
    store->calls[store->call_count++] = row;
    return true;
}
bool semantic_param_publication_record_body_call(SemanticContext *ctx, ASTNode *call)
{
    if (ctx == NULL || !ctx->tracking_function_effects)
        return true; /* Top-level calls have no routine body dependency. */
    const ASTNode *current = callable_capability_current_declaration(ctx);
    if (call == NULL || call->type != AST_CALL) return false;
    const ASTNode *callee = ast_call_callee(call);
    ASTNode *decl = callee != NULL && callee->type == AST_IDENTIFIER
        ? semantic_find_callable_decl_by_name(ctx, ast_identifier_name(callee)) : NULL;
    if ((current != NULL && current->type != AST_FUNC_DECL) ||
        ast_call_semantic_callee_value_binding_id(call) != 0 ||
        (decl != NULL && decl->type != AST_FUNC_DECL)) {
        semantic_record_body_summary(ctx, BODY_SUMMARY_MAY_ESCAPE_REF);
        return true;
    }
    if (ctx->current_function_decl == NULL) return true;
    if (current != ctx->current_function_decl || ast_node_stable_id(current) == 0) return false;
    if (!semantic_param_publication_pending(ctx) || ast_node_stable_id(call) == 0 ||
        ast_call_semantic_callee_decl_id(call) == 0) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    if (!semantic_param_publication_reserve_storage((void **)&store->body_calls, &store->body_call_capacity,
            store->body_call_count, sizeof(*store->body_calls))) return false;
    store->body_calls[store->body_call_count++] = (PendingBodyCall){call, ctx->current_function_decl,
        ast_node_stable_id(call), ast_node_stable_id(ctx->current_function_decl),
        ast_call_semantic_callee_decl_id(call), 0, 0};
    return true;
}
static PendingRoutine *publication_routine(FunctionParamPublicationStore *store, uint32_t id)
{
    if (store == NULL || id == 0 || store->node_capacity == 0) return NULL;
    size_t slot = (id * (size_t)2654435761u) & (store->node_capacity - 1);
    while (store->nodes[slot].id != 0 && store->nodes[slot].id != id)
        slot = (slot + 1) & (store->node_capacity - 1);
    PublishedNode *node = &store->nodes[slot];
    if (node->id != id || node->routine_index == 0 || node->routine_index > store->routine_count) return NULL;
    PendingRoutine *row = &store->routines[node->routine_index - 1];
    return row->id == id && row->decl == node->node && ast_node_stable_id(row->decl) == id ? row : NULL;
}
ASTNode *semantic_param_publication_direct_callee(const SemanticContext *ctx, const ASTNode *call)
{
    if (!semantic_param_publication_checked_call(ctx, call)) return NULL;
    PublishedNode *node = publication_node(ctx->function_param_publications, call, false);
    /* Member receivers need an admitted self/argument ordinal mapping. */
    if (node == NULL || node->declaration == 0 || node->value_binding != 0 ||
        node->target == NULL || node->target->type != AST_IDENTIFIER) return NULL;
    PendingRoutine *row = publication_routine(ctx->function_param_publications, node->declaration);
    if (row == NULL || !semantic_param_publication_routine_inputs_current(ctx, row->decl, true)) return NULL;
    return row->decl;
}
ASTNode *semantic_param_publication_callee(const SemanticContext *ctx, const ASTNode *call, size_t ordinal)
{
    if (!semantic_param_publication_checked_argument(ctx, call, ordinal)) return NULL;
    ASTNode *decl = semantic_param_publication_direct_callee(ctx, call);
    return decl != NULL && ordinal < ast_func_param_count(decl) ? decl : NULL;
}
bool semantic_param_publication_admitted_view(const SemanticContext *ctx, SemanticParamPublicationView *view)
{
    if (view == NULL || !semantic_param_publication_inputs_ready(ctx)) return false;
    const FunctionParamPublicationStore *store = ctx->function_param_publications;
    *view = (SemanticParamPublicationView){store->program, store->program_id, store->routines,
        store->routine_count, store->calls, store->call_count, store->body_calls, store->body_call_count};
    return true;
}
bool semantic_param_publication_view_current(const SemanticContext *ctx, const SemanticParamPublicationView *view)
{
    if (view == NULL || !semantic_param_publication_inputs_ready(ctx)) return false;
    const FunctionParamPublicationStore *store = ctx->function_param_publications;
    return view->program == store->program && view->program_id == store->program_id &&
        view->routines == store->routines && view->routine_count == store->routine_count &&
        view->calls == store->calls && view->call_count == store->call_count &&
        view->body_calls == store->body_calls && view->body_call_count == store->body_call_count;
}
bool semantic_param_publication_finalize(SemanticContext *ctx)
{
    if (!semantic_param_publication_pending(ctx) || ctx->function_param_flow_summaries != NULL) return false;
    if (!semantic_typed_map_query_facts_ready(ctx)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    for (size_t n = 0; n < store->node_capacity; n++) {
        PublishedNode *row = &store->nodes[n];
        if (row->id == 0) continue;
        if (row->id != ast_node_stable_id(row->node) ||
            (row->call_present && !publication_call_ready(row))) return false;
    }
    for (size_t r = 0; r < store->routine_count; r++) {
        PendingRoutine *row = &store->routines[r];
        if (!semantic_param_publication_routine_inputs_current(ctx, row->decl, false)) return false;
    }
    for (size_t c = 0; c < store->call_count; c++) {
        PendingRefCall *row = &store->calls[c];
        if (row->call_id != ast_node_stable_id(row->call) || row->site_id != ast_node_stable_id(row->site) ||
            !publication_ref_call_tuple(row->call, row->site, row->callee, row->ordinal) ||
            row->call_target != ast_call_callee(row->call) ||
            row->call_target_id != ast_node_stable_id(row->call_target) ||
            row->argument_count != ast_call_arg_count(row->call) ||
            row->callee_id != ast_node_stable_id(row->callee) ||
            publication_routine(store, row->callee_id) == NULL ||
            (row->caller != NULL && row->caller_id != ast_node_stable_id(row->caller))) return false;
    }
    for (size_t b = 0; b < store->body_call_count; b++) {
        PendingBodyCall *row = &store->body_calls[b];
        PendingRoutine *caller = publication_routine(store, row->caller_id);
        PendingRoutine *callee = publication_routine(store, row->callee_id);
        if (row->call_id != ast_node_stable_id(row->call) || row->call->type != AST_CALL ||
            row->caller_id != ast_node_stable_id(row->caller) ||
            ast_call_semantic_callee_decl_id(row->call) != row->callee_id ||
            ast_call_semantic_callee_value_binding_id(row->call) != 0 ||
            caller == NULL || caller->decl != row->caller || callee == NULL) return false;
        row->caller_index = (size_t)(caller - store->routines);
        row->callee_index = (size_t)(callee - store->routines);
    }
    store->pending = false; store->inputs_ready = true;
    SemanticParamPublicationView view;
    if (!semantic_param_publication_admitted_view(ctx, &view)) return false;
    if (!semantic_consume_param_publication(ctx, &view)) return false;
    store->finalized = !ctx->has_error;
    return !ctx->has_error;
}
