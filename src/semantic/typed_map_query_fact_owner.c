/* Exact checked Map query facts; call spelling alone grants nothing. */
#include "typed_map_query_fact_owner.h"
#include "function_param_publication_owner.h"
#include "type_checker_internal.h"
#include "type_checker_collection_policy.h"
#include <stdlib.h>
#include <string.h>
typedef struct {
    ASTNode *call;
    uint32_t id;
    const ASTNode *callee, *argument;
    uint32_t callee_id, argument_id;
    PgyTypedMapQueryKind kind;
    Type *receiver;
    Type *result;
} TypedMapQueryRow;
struct TypedMapQueryFactStore { TypedMapQueryRow *rows; size_t count, capacity; };

bool semantic_typed_map_query_record(SemanticContext *ctx, ASTNode *call,
    PgyTypedMapQueryKind kind, Type *receiver, Type *result)
{
    if (!semantic_param_publication_pending(ctx) || call == NULL ||
        call->type != AST_CALL || ast_node_stable_id(call) == 0 || !type_is_constructed_named(receiver, "HashMap") ||
        type_constructed_arg_count(receiver) != 2 || result == NULL || result == TYPE_UNKNOWN)
        return false;
    uint32_t builtin;
    const ASTNode *callee = ast_call_callee(call);
    const char *name = callee != NULL && callee->type == AST_IDENTIFIER ? ast_identifier_name(callee) : NULL;
    if (name == NULL || ast_call_semantic_callee_decl_id(call) != 0 ||
        ast_call_semantic_callee_value_binding_id(call) != 0 ||
        !ast_call_semantic_callee_builtin_kind(call, &builtin) || builtin != BUILTIN_NOT_BUILTIN ||
        strcmp(name, kind == PGY_TYPED_MAP_QUERY_INT ? "MapGet" : "MapKeys") != 0) return false;
    Type *key = type_constructed_arg(receiver, 0);
    Type *value = type_constructed_arg(receiver, 1);
    const ASTNode *argument = ast_call_argument(call, 0);
    if (argument == NULL || ast_node_stable_id(argument) == 0 || ast_node_stable_id(callee) == 0 ||
        !type_checker_hashmap_key_supported(key) ||
        (kind == PGY_TYPED_MAP_QUERY_INT && (result != TYPE_INT || value != TYPE_INT || ast_call_arg_count(call) != 2)) ||
        (kind == PGY_TYPED_MAP_QUERY_KEYS && (!type_is_constructed_named(result, "Array") ||
            type_constructed_arg_count(result) != 1 || !type_equals(type_constructed_arg(result, 0), key) ||
            ast_call_arg_count(call) != 1)) ||
        (kind != PGY_TYPED_MAP_QUERY_INT && kind != PGY_TYPED_MAP_QUERY_KEYS))
        return false;
    TypedMapQueryFactStore *store = ctx->typed_map_query_facts;
    if (store == NULL) {
        store = calloc(1, sizeof(*store));
        if (store == NULL) return false;
        ctx->typed_map_query_facts = store;
    }
    for (size_t i = 0; i < store->count; i++) {
        if (store->rows[i].id != ast_node_stable_id(call)) continue;
        return store->rows[i].call == call && store->rows[i].callee == callee &&
            store->rows[i].argument == argument && store->rows[i].kind == kind &&
            type_equals(store->rows[i].receiver, receiver) && type_equals(store->rows[i].result, result);
    }
    if (store->count == store->capacity) {
        size_t next = store->capacity == 0 ? 32 : store->capacity * 2;
        if (next < store->capacity || next > SIZE_MAX / sizeof(*store->rows)) return false;
        void *rows = realloc(store->rows, next * sizeof(*store->rows));
        if (rows == NULL) return false;
        store->rows = rows; store->capacity = next;
    }
    store->rows[store->count++] = (TypedMapQueryRow){call, ast_node_stable_id(call),
        callee, argument, ast_node_stable_id(callee), ast_node_stable_id(argument), kind, receiver, result};
    return true;
}
static bool typed_map_query_row_ready(const TypedMapQueryRow *row)
{
    const ASTNode *call = row->call;
    uint32_t builtin;
    if (call == NULL || row->id == 0 || row->id != ast_node_stable_id(call) ||
        call->type != AST_CALL || !ast_call_semantic_callee_is_stdlib(call) ||
        ast_call_semantic_callee_decl_id(call) != 0 || ast_call_semantic_callee_value_binding_id(call) != 0 ||
        !ast_call_semantic_callee_builtin_kind(call, &builtin) || builtin != BUILTIN_NOT_BUILTIN) return false;
    const ASTNode *callee = ast_call_callee(call);
    if (callee == NULL || callee->type != AST_IDENTIFIER) return false;
    const char *name = ast_identifier_name(callee);
        if (row->callee != callee || row->callee_id != ast_node_stable_id(callee) ||
            row->argument != ast_call_argument(call, 0) ||
            row->argument_id != ast_node_stable_id(ast_call_argument(call, 0)) ||
            !type_is_constructed_named(row->receiver, "HashMap") ||
            type_constructed_arg_count(row->receiver) != 2 ||
            !type_checker_hashmap_key_supported(type_constructed_arg(row->receiver, 0))) return false;
        return name != NULL &&
            ((row->kind == PGY_TYPED_MAP_QUERY_INT && ast_call_arg_count(call) == 2 && strcmp(name, "MapGet") == 0 &&
                row->result == TYPE_INT && type_constructed_arg(row->receiver, 1) == TYPE_INT) ||
             (row->kind == PGY_TYPED_MAP_QUERY_KEYS && ast_call_arg_count(call) == 1 && strcmp(name, "MapKeys") == 0 &&
                type_is_constructed_named(row->result, "Array") && type_constructed_arg_count(row->result) == 1 &&
                type_equals(type_constructed_arg(row->result, 0), type_constructed_arg(row->receiver, 0))));
}
bool semantic_typed_map_query_facts_ready(const SemanticContext *ctx)
{
    if (!semantic_param_publication_pending(ctx)) return false;
    if (ctx->typed_map_query_facts == NULL) return true;
    for (size_t i = 0; i < ctx->typed_map_query_facts->count; i++)
        if (!typed_map_query_row_ready(&ctx->typed_map_query_facts->rows[i])) return false;
    return true;
}
bool semantic_typed_map_query_receiver_read(const SemanticContext *ctx, const ASTNode *call, size_t ordinal)
{
    if (ordinal != 0 || !semantic_param_publication_inputs_ready(ctx) ||
        ctx->typed_map_query_facts == NULL) return false;
    for (size_t i = 0; i < ctx->typed_map_query_facts->count; i++) {
        const TypedMapQueryRow *row = &ctx->typed_map_query_facts->rows[i];
        if (row->call == call) return typed_map_query_row_ready(row);
    }
    return false;
}
void semantic_typed_map_query_destroy(SemanticContext *ctx)
{
    if (ctx == NULL || ctx->typed_map_query_facts == NULL) return;
    free(ctx->typed_map_query_facts->rows); free(ctx->typed_map_query_facts);
    ctx->typed_map_query_facts = NULL;
}

bool semantic_typed_map_query_result_independent(const SemanticContext *ctx, const ASTNode *call)
{
    if (!semantic_typed_map_query_receiver_read(ctx, call, 0) ||
        !semantic_param_publication_checked_argument(ctx, call, 0)) return false;
    for (size_t i = 0; i < ast_call_arg_count(call); i++) {
        if (!semantic_param_publication_checked_argument(ctx, call, i)) return false;
        const ASTNode *root = ast_call_argument(call, i);
        while (root != NULL && (root->type == AST_MEMBER_ACCESS || root->type == AST_ARRAY_ACCESS))
            root = root->type == AST_MEMBER_ACCESS ? ast_member_object(root) : ast_array_access_array(root);
        if (root == NULL || !semantic_param_publication_value(ctx, root).ok) return false;
    }
    /* The checked rows cover only physical Int MapGet and independently
       allocated MapKeys snapshots; no String MapGet row can enter here. */
    return true;
}
