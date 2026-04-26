/* transpiler_helpers_core_b split into sub-1000 LOC include chunks.
 * Keep this shim for the existing include order. */
static char *
emit_builtin_to_dto(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *target_arg;
    ASTNode *source_arg;
    ASTNode *target_decl;
    ASTNode *source_decl;
    const char *source_type_name;
    char *source_expr;
    char *result;

    if (call->data.call.arg_count != 2)
        return pergyra_strdup("/* ToTObject: invalid args */");

    target_arg = call->data.call.arguments[0];
    source_arg = call->data.call.arguments[1];
    if (target_arg == NULL || target_arg->type != AST_IDENTIFIER
        || target_arg->data.identifier.name == NULL) {
        return pergyra_strdup("/* ToTObject: need target tobject type */");
    }
    if (source_arg == NULL || source_arg->type != AST_IDENTIFIER)
        return pergyra_strdup("/* ToTObject: need named subject source */");

    target_decl = find_class_decl(ctx, target_arg->data.identifier.name);
    if (target_decl == NULL || !target_decl->data.class_decl.is_struct)
        return pergyra_strdup("/* ToTObject: target must be tobject/struct */");

    source_type_name = infer_expression_type_name(ctx, source_arg);
    source_decl = find_class_decl(ctx, source_type_name);
    if (source_decl == NULL)
        return pergyra_strdup("/* ToTObject: source subject type not found */");

    source_expr = emit_expression(source_arg, ctx);
    result = emit_projection_literal(ctx, target_decl, source_decl, NULL,
        target_arg->data.identifier.name, source_expr);
    free(source_expr);
    return result;
}

static bool
func_has_generic_params(ASTNode *node)
{
    return node != NULL
        && node->type == AST_FUNC_DECL
        && node->data.func_decl.generic_params != NULL
        && node->data.func_decl.generic_params->count > 0;
}

static const char *
lookup_generic_binding(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;

    for (int i = ctx->generic_binding_count - 1; i >= 0; i--) {
        if (strcmp(ctx->generic_bindings[i].name, name) == 0)
            return ctx->generic_bindings[i].concrete_type;
    }

    return NULL;
}

static bool
infer_generic_call_bindings(TranspilerCtx *ctx, ASTNode *decl, ASTNode *call,
                            GenericBindingEntry *bindings, size_t *binding_count);

static char *
render_type_name_with_bindings(TranspilerCtx *ctx, ASTNode *type_node,
                               GenericBindingEntry *bindings, size_t binding_count);

#include "transpiler_specialization_helpers.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_func_forward_helpers.h"
#include "transpiler_generic_specialization_emit.h"
