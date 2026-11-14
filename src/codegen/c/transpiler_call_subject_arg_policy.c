#include "transpiler_call_subject_arg_policy.h"

#include <stdlib.h>
#include <string.h>

#include "../../parser/ast_api.h"
#include "../../compiler/mir/mir_decl_headers.h"
#include "../../common/string_compat.h"
#include "../../semantic/diag_codes.h"
#include "../codegen_subject_argument_policy.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_format.h"
#include "transpiler_inventory_view.h"
#include "transpiler_intent_participant.h"
#include "transpiler_overlay_host_fields.h"
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
        /* The constructor owner materializes a fresh subject in a C compound
         * literal. Its storage lasts for the enclosing block, not an inner
         * statement expression. An ordinary function result is not this cell. */
        return pgy_codegen_is_subject_constructor(
            transpiler_active_mir_identity(ctx), arg_node);
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

/* The emitted C expression is the address of the value: a parameter the
 * prologue binds indirectly (its MIR carriage passes it by address, as for a
 * subject parameter), or self in a pointer-self host body. A store into
 * by-value storage (an Option payload, a constructor field) dereferences it. */
bool
transpiler_call_arg_is_carried_by_address(TranspilerCtx *ctx,
                                          ASTNode *arg_node)
{
    if (ctx == NULL || arg_node == NULL || arg_node->type != AST_IDENTIFIER)
        return false;
    if (strcmp(ast_identifier_name(arg_node), "self") == 0)
        return current_class_uses_self_cell(ctx)
            || transpiler_host_decl_uses_pointer_self(
                transpiler_current_host_decl_local(ctx));
    return transpiler_call_arg_is_indirect_ref(ctx, arg_node);
}

char *
transpiler_call_arg_address(TranspilerCtx *ctx, ASTNode *arg_node,
                            const char *arg, const char *callee_name,
                            size_t index)
{
    if (transpiler_call_arg_is_carried_by_address(ctx, arg_node))
        return pergyra_strdup(arg);
    if (transpiler_call_arg_can_take_subject_address(ctx, arg_node))
        return strdup_fmt("&(%s)", arg);
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
        "C backend: indirect argument %zu for '%s' requires addressable storage",
        index + 1, callee_name != NULL ? callee_name : "<call>");
    return NULL;
}
