/* A formal's String stored into an `inout` output lands as an owned copy. */
#include "formal_string_store_landing_owner.h"
#include "type_checker_internal.h"
#include "type_checker_call_constructor_internal.h"
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "collection_ownership_fact.h"
#include "../parser/ast_api.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* The formal a path is rooted at, by its scope binding: the value argument
 * is not checked yet, so its identifiers carry no binding identity. */
static FuncParam *store_landing_root_formal(const ASTNode *node, SemanticContext *ctx)
{
    while (node != NULL && (node->type == AST_MEMBER_ACCESS || node->type == AST_ARRAY_ACCESS))
        node = node->type == AST_MEMBER_ACCESS ? ast_member_object(node) : ast_array_access_array(node);
    ASTNode *decl = ctx->current_function_decl;
    if (node == NULL || node->type != AST_IDENTIFIER || ast_identifier_name(node) == NULL ||
        decl == NULL || decl->type != AST_FUNC_DECL) return NULL;
    Symbol *symbol = scope_lookup(ctx->scope, ast_identifier_name(node));
    if (symbol == NULL || !symbol->is_parameter || symbol->decl_syntax_id == 0) return NULL;
    for (size_t p = 0; p < ast_func_param_count(decl); p++) {
        FuncParam *param = ast_func_param(decl, p);
        if (param != NULL && ast_func_param_stable_id(param) == symbol->decl_syntax_id) return param;
    }
    return NULL;
}

/* The declared type of a path of a formal: a field of a nominal value or an
 * Array element, each step from its declaration. NULL when any step is not
 * one of those. */
static const Type *store_landing_path_type(const ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL) return NULL;
    if (node->type == AST_IDENTIFIER) {
        Symbol *symbol = ast_identifier_name(node) == NULL ? NULL : scope_lookup(ctx->scope, ast_identifier_name(node));
        return symbol == NULL ? NULL : symbol->type;
    }
    if (node->type == AST_ARRAY_ACCESS) {
        const Type *array = store_landing_path_type(ast_array_access_array(node), ctx);
        return type_is_constructed_named(array, "Array") ? type_get_constructed_arg(array, 0) : NULL;
    }
    if (node->type != AST_MEMBER_ACCESS || ast_member_name(node) == NULL) return NULL;
    const Type *object = store_landing_path_type(ast_member_object(node), ctx);
    ASTNode *decl = object == NULL || object->kind != TYPE_KIND_CLASS ? NULL
        : semantic_host_decl_for_type(ctx, object);
    if (decl == NULL || decl->type != AST_CLASS_DECL) return NULL;
    for (size_t i = 0;; i++) {
        const char *name = NULL;
        Type *field = constructor_decl_field_resolved_type_at(decl, i, &name, ctx);
        if (name == NULL) return NULL;
        if (strcmp(name, ast_member_name(node)) == 0) return field;
    }
}

/* What a landing copies, while the store call is checked: a String path of
 * a by-value or `ref` formal. */
static bool store_landing_value_landable(const ASTNode *value, SemanticContext *ctx)
{
    FuncParam *source = store_landing_root_formal(value, ctx);
    return source != NULL && (source->mode == PARAM_MODE_DEFAULT || source->mode == PARAM_MODE_REF) &&
        type_equals(store_landing_path_type(value, ctx), TYPE_STRING);
}

/* The element type of a store receiver that is an Array path, from its
 * declarations: a planned push of a String the value does not name as a
 * formal path is still a String push, though the value is not checked yet. */
static const Type *store_landing_receiver_element(const ASTNode *receiver, SemanticContext *ctx)
{
    const Type *array = store_landing_path_type(receiver, ctx);
    return type_is_constructed_named(array, "Array") ? type_get_constructed_arg(array, 0) : NULL;
}

static bool store_landing_planned(const ASTNode *call, const SemanticContext *ctx)
{
    for (size_t i = 0; i < ctx->string_store_landing_plan_count; i++)
        if (ctx->string_store_landing_plan[i] == ast_node_stable_id(call)) return true;
    return false;
}

bool semantic_formal_string_store_landing_owned_push(ASTNode *call, SemanticContext *ctx)
{
    return call != NULL && ctx != NULL && ast_call_arg_count(call) == 2 &&
        store_landing_planned(call, ctx) &&
        (store_landing_value_landable(ast_call_argument(call, 1), ctx) ||
         type_equals(store_landing_receiver_element(ast_call_argument(call, 0), ctx), TYPE_STRING)) &&
        semantic_collection_owned_string_push_admissible(ast_call_argument(call, 0), ctx);
}

bool semantic_formal_string_store_landing(ASTNode *call, size_t receiver_ordinal,
    size_t value_ordinal, SemanticContext *ctx)
{
    size_t count = 0;
    ASTNode **args = call == NULL || ctx == NULL ? NULL : ast_call_arguments(call, &count);
    if (args == NULL || receiver_ordinal >= count || value_ordinal >= count) return true;
    ASTNode *value = args[value_ordinal];
    FuncParam *receiver = store_landing_root_formal(args[receiver_ordinal], ctx);
    /* Into an `inout` output a formal's String always leaves; into anything
     * else only an earlier analysis's flow facts can say it does, and they
     * plan only values Clone copies whole. */
    if (!store_landing_planned(call, ctx) &&
        !(receiver != NULL && receiver->mode == PARAM_MODE_MUT_REF && store_landing_value_landable(value, ctx)))
        return true;
    if (ctx->synthetic_stable_id_next == 0)
        ctx->synthetic_stable_id_next = ast_stable_id_ceiling(ctx->program_root);
    if (ctx->synthetic_stable_id_next == 0 || ctx->synthetic_stable_id_next > UINT32_MAX - 2) return false;
    ASTNode *callee = ast_create_identifier("Clone");
    ASTNode *clone = callee == NULL ? NULL : ast_create_call(callee);
    if (clone == NULL || !ast_call_set_compiler_builtin(clone)) return false;
    ast_add_argument(clone, value);
    if (ast_call_arg_count(clone) != 1) return false;
    callee->line = clone->line = value->line;
    callee->column = clone->column = value->column;
    callee->stable_id = ctx->synthetic_stable_id_next++;
    clone->stable_id = ctx->synthetic_stable_id_next++;
    args[value_ordinal] = clone;
    return true;
}

static void store_landing_record(SemanticContext *ctx, uint32_t id)
{
    for (size_t i = 0; i < ctx->string_store_landing_selected_count; i++)
        if (ctx->string_store_landing_selected[i] == id) return;
    for (size_t i = 0; i < ctx->string_store_landing_plan_count; i++)
        if (ctx->string_store_landing_plan[i] == id) return;
    if (ctx->string_store_landing_selected_count == ctx->string_store_landing_selected_capacity) {
        size_t capacity = ctx->string_store_landing_selected_capacity == 0 ? 16
            : ctx->string_store_landing_selected_capacity * 2;
        uint32_t *grown = realloc(ctx->string_store_landing_selected, capacity * sizeof(*grown));
        if (grown == NULL) return;
        ctx->string_store_landing_selected = grown;
        ctx->string_store_landing_selected_capacity = capacity;
    }
    ctx->string_store_landing_selected[ctx->string_store_landing_selected_count++] = id;
}

void semantic_formal_store_landing_select_held(SemanticContext *ctx, const ASTNode *call, size_t value_ordinal)
{
    if (ctx == NULL || call == NULL || value_ordinal >= ast_call_arg_count(call) || ast_node_stable_id(call) == 0) return;
    SemanticPublishedValue value = semantic_param_publication_value(ctx, ast_call_argument(call, value_ordinal));
    if (!value.ok || value.meta || publication_physical_scalar(value.type) ||
        !semantic_param_publication_clone_independent(ctx, value.type)) return;
    store_landing_record(ctx, ast_node_stable_id(call));
}

void semantic_formal_string_store_landing_select_checked(SemanticContext *ctx, ASTNode *call,
    size_t value_ordinal)
{
    ASTNode *value = call == NULL || value_ordinal >= ast_call_arg_count(call) ? NULL
        : ast_call_argument(call, value_ordinal);
    if (store_landing_value_landable(value, ctx) && ast_node_stable_id(call) != 0)
        store_landing_record(ctx, ast_node_stable_id(call));
}

void semantic_formal_string_store_landing_select(SemanticContext *ctx, const ASTNode *call,
    size_t ordinal, const ASTNode *root, const ASTNode *routine)
{
    /* Only a String a store builtin keeps, rooted at a by-value or `ref`
     * formal of the routine whose summary records the escape. */
    size_t value_ordinal = 0;
    if (root == NULL && call != NULL && ordinal < ast_call_arg_count(call)) {
        root = ast_call_argument(call, ordinal);
        while (root != NULL && (root->type == AST_MEMBER_ACCESS || root->type == AST_ARRAY_ACCESS))
            root = root->type == AST_MEMBER_ACCESS ? ast_member_object(root) : ast_array_access_array(root);
        if (root != NULL && root->type != AST_IDENTIFIER) root = NULL;
    }
    PublishedNode *row = publication_result_current(ctx, call);
    SemanticPublishedValue value = semantic_param_publication_value(ctx, ast_call_argument(call, ordinal));
    if (row == NULL || root == NULL || routine == NULL || routine->type != AST_FUNC_DECL ||
        !value.ok || value.meta || !type_equals(value.type, TYPE_STRING) || ast_node_stable_id(call) == 0) return;
    if (!semantic_param_publication_checked_array_store(ctx, call, &value_ordinal)) {
        if (row->target_name == NULL || strcmp(row->target_name, "MapSet") != 0 || !row->stdlib ||
            row->argument_count != 3) return;
        value_ordinal = 2;
    }
    if (ordinal != value_ordinal) return;
    bool formal = false;
    for (size_t p = 0; p < ast_func_param_count(routine); p++) {
        FuncParam *param = ast_func_param(routine, p);
        if (param != NULL && ast_func_param_stable_id(param) == ast_identifier_binding_syntax_id(root) &&
            (param->mode == PARAM_MODE_DEFAULT || param->mode == PARAM_MODE_REF)) formal = true;
    }
    if (formal) store_landing_record(ctx, ast_node_stable_id(call));
}
