#ifndef PGY_SRC_CODEGEN_TRANSPILER_HELPERS_CORE_B_H
#define PGY_SRC_CODEGEN_TRANSPILER_HELPERS_CORE_B_H

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

    if (ast_call_arg_count(call) != 2)
        return pergyra_strdup("/* ToTObject: invalid args */");

    target_arg = ast_call_argument(call, 0);
    source_arg = ast_call_argument(call, 1);
    if (target_arg == NULL || target_arg->type != AST_IDENTIFIER
        || target_arg->data.identifier.name == NULL) {
        return pergyra_strdup("/* ToTObject: need target tobject type */");
    }
    if (source_arg == NULL || source_arg->type != AST_IDENTIFIER)
        return pergyra_strdup("/* ToTObject: need named subject source */");

    target_decl = find_class_decl(ctx, target_arg->data.identifier.name);
    if (target_decl == NULL || !ast_class_is_struct(target_decl))
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
        && ast_func_generic_params(node) != NULL
        && ast_func_generic_params(node)->count > 0;
}

static bool
infer_generic_call_bindings(TranspilerCtx *ctx, ASTNode *decl, ASTNode *call,
                            GenericBindingEntry *bindings, size_t *binding_count);

static char *
render_type_name_with_bindings(TranspilerCtx *ctx, ASTNode *type_node,
                               GenericBindingEntry *bindings, size_t binding_count);

#include "transpiler_collection_runtime_suffix.h"
#include "transpiler_specialization_helpers.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_func_forward_metadata.h"
#include "transpiler_func_forward_helpers.h"
#include "transpiler_generic_specialization_emit.h"
#endif /* PGY_SRC_CODEGEN_TRANSPILER_HELPERS_CORE_B_H */
