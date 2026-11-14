/* Replays sealed immutable aggregate carriers to the formal-relative member
 * path captured while their lexical scopes were alive. */
#include "collection_ownership_fact_internal.h"

#include <string.h>

#include "default_aggregate_move_rule.h"
#include "function_param_flow/function_param_publication_internal.h"
#include "type_checker/type_checker_internal.h"
#include "../parser/ast_api.h"

bool
collection_published_member_move_parameter_path(
    const ASTNode *member_access, SemanticContext *ctx,
    ASTNode **function_out, size_t *formal_out, ParamMode *mode_out,
    PgyCollectionMemberPath *path_out)
{
    FunctionParamPublicationStore *store = ctx != NULL
        ? ctx->function_param_publications : NULL;
    ASTNode *function = NULL;
    PgyCollectionMemberPath path;
    uint32_t carrier_binding;
    const Type *carrier_type;

    if (function_out != NULL) *function_out = NULL;
    if (formal_out != NULL) *formal_out = SIZE_MAX;
    if (mode_out != NULL) *mode_out = PARAM_MODE_DEFAULT;
    if (path_out != NULL) memset(path_out, 0, sizeof(*path_out));
    if (store == NULL || path_out == NULL
        || !collection_published_typed_member_path_current(
            member_access, ctx, &function, &path)
        || function == NULL || function->type != AST_FUNC_DECL
        || path.root_binding_id == 0 || path.field_count == 0
        || path.root_type == NULL || path.root_type == TYPE_UNKNOWN
        || !collection_array_string_type(path.result_type))
        return false;

    carrier_binding = path.root_binding_id;
    carrier_type = path.root_type;
    for (size_t depth = 0; depth <= store->node_count; depth++) {
        size_t found = SIZE_MAX;
        ParamMode found_mode = PARAM_MODE_DEFAULT;
        for (size_t i = 0; i < ast_func_param_count(function); i++) {
            FuncParam *formal = ast_func_param(function, i);
            if (formal == NULL
                || ast_func_param_stable_id(formal) != carrier_binding)
                continue;
            if (found != SIZE_MAX
                || (formal->mode != PARAM_MODE_DEFAULT
                    && formal->mode != PARAM_MODE_MUT_REF))
                return false;
            found = i;
            found_mode = formal->mode;
        }
        if (found != SIZE_MAX) {
            if (function_out != NULL) *function_out = function;
            if (formal_out != NULL) *formal_out = found;
            if (mode_out != NULL) *mode_out = found_mode;
            *path_out = path;
            return true;
        }

        PublishedNode *definition = publication_node_by_id(
            store, carrier_binding);
        ASTNode *declaration = definition != NULL
            ? (ASTNode *)definition->node : NULL;
        ASTNode *initializer = declaration != NULL
            && declaration->type == AST_LET_DECL
            ? ast_let_initializer(declaration) : NULL;
        PublishedNode *initializer_row = initializer != NULL
            ? publication_current_value(ctx, initializer) : NULL;
        if (declaration == NULL || declaration->type != AST_LET_DECL
            || ast_node_stable_id(declaration) != carrier_binding
            || ast_let_is_mutable(declaration) || ast_let_is_alias(declaration)
            || initializer == NULL || initializer_row == NULL
            || initializer_row->node != initializer
            || initializer_row->id != ast_node_stable_id(initializer)
            || initializer_row->caller != function
            || initializer_row->caller_id != ast_node_stable_id(function)
            || initializer_row->value_type == NULL
            || initializer_row->value_type == TYPE_UNKNOWN
            || !type_equals(initializer_row->value_type, carrier_type)
            || !semantic_default_aggregate_value(ctx, carrier_type)
            || type_is_constructed_named(carrier_type, "Array"))
            return false;

        if (initializer->type == AST_IDENTIFIER) {
            carrier_binding = ast_identifier_binding_syntax_id(initializer);
            if (carrier_binding == 0)
                return false;
            continue;
        }
        if (initializer->type != AST_MEMBER_ACCESS)
            return false;

        ASTNode *prefix_function = NULL;
        PgyCollectionMemberPath prefix;
        if (!collection_published_typed_member_path_current(
                initializer, ctx, &prefix_function, &prefix)
            || prefix_function != function || prefix.field_count == 0
            || !type_equals(prefix.result_type, carrier_type)
            || prefix.field_count + path.field_count
                > PGY_COLLECTION_MEMBER_PATH_LIMIT)
            return false;
        memmove(path.field_ids + prefix.field_count, path.field_ids,
            path.field_count * sizeof(*path.field_ids));
        memcpy(path.field_ids, prefix.field_ids,
            prefix.field_count * sizeof(*path.field_ids));
        path.field_count += prefix.field_count;
        carrier_binding = prefix.root_binding_id;
        carrier_type = prefix.root_type;
    }
    return false;
}
