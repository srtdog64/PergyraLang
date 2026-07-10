#include "transpiler_call_subject_arg_policy.h"

#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_intent_participant.h"
#include "transpiler_symbols.h"
#include "transpiler_type_render.h"

static bool
type_node_is_pointer_self_host(TranspilerCtx *ctx, ASTNode *type_node)
{
    char *type_name;
    bool result = false;

    if (ctx == NULL || type_node == NULL)
        return false;

    type_name = render_type_name_in_ctx(ctx, type_node);
    if (type_name != NULL)
        result = is_pointer_self_host_type_name(ctx, type_name);
    free(type_name);
    return result;
}

bool
transpiler_call_arg_needs_subject_address(TranspilerCtx *ctx,
                                          FuncParam *param,
                                          const char *param_type_name,
                                          ASTNode *intent_param_type,
                                          const char *intent_param_type_name)
{
    if (ctx == NULL)
        return false;

    if (param != NULL && param->name != NULL
        && strcmp(param->name, "self") != 0) {
        if (param_type_name != NULL)
            return is_pointer_self_host_type_name(ctx, param_type_name);
        if (param->type != NULL
            && type_node_is_pointer_self_host(ctx, param->type)) {
            return true;
        }
    }

    if (intent_param_type_name != NULL)
        return intent_type_name_uses_pointer_self(ctx, intent_param_type_name);

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
transpiler_call_arg_is_indirect_ref(TranspilerCtx *ctx, ASTNode *arg_node)
{
    TypedVarEntry *entry;

    if (ctx == NULL || arg_node == NULL || arg_node->type != AST_IDENTIFIER)
        return false;

    entry = lookup_typed_entry(ctx, ast_identifier_name(arg_node));
    return entry != NULL && entry->is_indirect_ref;
}
