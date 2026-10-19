/* A formal's String stored into an `inout` output lands as an owned copy. */
#include "formal_string_store_landing_owner.h"
#include "type_checker_internal.h"
#include "type_checker_call_constructor_internal.h"
#include "../parser/ast_api.h"
#include <stdint.h>
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

bool semantic_formal_string_store_landing(ASTNode *call, size_t receiver_ordinal,
    size_t value_ordinal, SemanticContext *ctx)
{
    size_t count = 0;
    ASTNode **args = call == NULL || ctx == NULL ? NULL : ast_call_arguments(call, &count);
    if (args == NULL || receiver_ordinal >= count || value_ordinal >= count) return true;
    ASTNode *value = args[value_ordinal];
    FuncParam *receiver = store_landing_root_formal(args[receiver_ordinal], ctx);
    FuncParam *source = store_landing_root_formal(value, ctx);
    if (receiver == NULL || receiver->mode != PARAM_MODE_MUT_REF || source == NULL ||
        (source->mode != PARAM_MODE_DEFAULT && source->mode != PARAM_MODE_REF) ||
        !type_equals(store_landing_path_type(value, ctx), TYPE_STRING)) return true;
    if (ctx->synthetic_stable_id_next == 0)
        ctx->synthetic_stable_id_next = ast_stable_id_ceiling(ctx->program_root);
    if (ctx->synthetic_stable_id_next == 0 || ctx->synthetic_stable_id_next > UINT32_MAX - 2) return false;
    ASTNode *callee = ast_create_identifier("Clone");
    ASTNode *clone = callee == NULL ? NULL : ast_create_call(callee);
    if (clone == NULL) return false;
    ast_add_argument(clone, value);
    if (ast_call_arg_count(clone) != 1) return false;
    callee->line = clone->line = value->line;
    callee->column = clone->column = value->column;
    callee->stable_id = ctx->synthetic_stable_id_next++;
    clone->stable_id = ctx->synthetic_stable_id_next++;
    args[value_ordinal] = clone;
    return true;
}
