#include "transpiler_call_subject_arg_policy.h"

#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_symbols.h"
#include "transpiler_type_render.h"

static bool
type_node_is_pointer_self_host(TranspilerCtx *ctx, ASTNode *type_node)
{
    char *type_name;
    bool result = false;

    if (ctx == NULL || type_node == NULL)
        return false;

    type_name = render_type_name(type_node);
    if (type_name != NULL)
        result = is_pointer_self_host_type_name(ctx, type_name);
    free(type_name);
    return result;
}

bool
transpiler_call_arg_needs_subject_address(TranspilerCtx *ctx,
                                          FuncParam *param,
                                          ASTNode *intent_param_type)
{
    if (ctx == NULL)
        return false;

    if (param != NULL && param->type != NULL
        && param->name != NULL && strcmp(param->name, "self") != 0
        && type_node_is_pointer_self_host(ctx, param->type)) {
        return true;
    }

    return type_node_is_pointer_self_host(ctx, intent_param_type);
}

bool
transpiler_call_arg_can_take_subject_address(ASTNode *arg_node)
{
    if (arg_node == NULL)
        return false;
    return arg_node->type == AST_IDENTIFIER
        || arg_node->type == AST_MEMBER_ACCESS
        || arg_node->type == AST_ARRAY_ACCESS;
}

bool
transpiler_call_arg_is_subject_ref(TranspilerCtx *ctx, ASTNode *arg_node)
{
    TypedVarEntry *entry;

    if (ctx == NULL || arg_node == NULL || arg_node->type != AST_IDENTIFIER)
        return false;

    entry = lookup_typed_entry(ctx, ast_identifier_name(arg_node));
    return entry != NULL && entry->is_subject_ref;
}
