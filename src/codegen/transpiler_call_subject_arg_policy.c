#include "transpiler_call_subject_arg_policy.h"

#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "../compiler/mir_decl_headers.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_inventory_view.h"
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
transpiler_call_arg_can_take_subject_address(TranspilerCtx *ctx, ASTNode *arg_node)
{
    if (arg_node == NULL)
        return false;
    if (arg_node->type == AST_CALL) {
        ASTNode *callee = ast_call_callee(arg_node);
        if (callee == NULL || callee->type != AST_IDENTIFIER
            || ast_call_semantic_callee_value_binding_id(arg_node) != 0)
            return false;
        const MIRDeclHeader *header = transpiler_active_host_decl_header(
            ctx, ast_identifier_name(callee));
        uint32_t target = ast_call_semantic_callee_decl_id(arg_node);
        /* The constructor owner materializes a fresh subject in a C compound
         * literal. Its storage lasts for the enclosing block, not an inner
         * statement expression. An ordinary function result is not this cell. */
        return header != NULL
            && mir_decl_header_nominal_kind_or(header, NOMINAL_DECL_CLASS)
                == NOMINAL_DECL_SUBJECT
            && (target == 0 || target == mir_decl_header_source_syntax_id(header));
    }
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
