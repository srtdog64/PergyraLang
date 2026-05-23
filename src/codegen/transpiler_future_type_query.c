/*
 * Copyright (c) 2026 Pergyra Language Project
 * Future/RemoteFuture type query helpers for C backend lowering.
 */

#include "transpiler_future_type_query.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_context.h"
#include "transpiler_generic_binding_query.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

static char *
infer_spawn_return_type_name_owned(TranspilerCtx *ctx, ASTNode *spawn_expr)
{
    ASTNode *target = ast_spawn_function(spawn_expr);
    const char *function_name = NULL;
    ASTNode *call = NULL;

    if (target == NULL)
        return pergyra_strdup("Void");

    if (target->type == AST_CALL
        && ast_call_callee(target) != NULL
        && ast_call_callee(target)->type == AST_IDENTIFIER) {
        call = target;
        function_name = ast_identifier_name(ast_call_callee(target));
    } else if (target->type == AST_IDENTIFIER) {
        function_name = ast_identifier_name(target);
    } else if (target->type == AST_FUNC_DECL) {
        if (ast_func_return_type(target) != NULL)
            return render_type_name(ast_func_return_type(target));
        return pergyra_strdup("Void");
    }

    if (function_name == NULL)
        return pergyra_strdup("Void");

    ASTNode *decl = find_function_decl(ctx, function_name);
    if (decl != NULL && ast_func_return_type(decl) != NULL) {
        if (call != NULL && transpiler_func_has_generic_params(decl)) {
            GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
            size_t binding_count = 0;
            if (transpiler_infer_generic_call_bindings(ctx, decl, call,
                    bindings, &binding_count)) {
                return transpiler_render_type_name_with_bindings(ctx,
                    ast_func_return_type(decl), bindings, binding_count);
            }
        }
        return render_type_name(ast_func_return_type(decl));
    }

    return pergyra_strdup("Void");
}

const char *
infer_spawn_return_type_name_scratch(TranspilerCtx *ctx, ASTNode *spawn_expr)
{
    char *owned = infer_spawn_return_type_name_owned(ctx, spawn_expr);
    const char *result = owned != NULL
        ? transpiler_scratch_strdup(ctx, owned)
        : NULL;

    free(owned);
    return result != NULL ? result : "Void";
}

bool
infer_spawn_return_type_name_copy(TranspilerCtx *ctx,
                                  ASTNode *spawn_expr,
                                  char *out,
                                  size_t out_size)
{
    const char *type_name;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    type_name = infer_spawn_return_type_name_scratch(ctx, spawn_expr);
    return pergyra_str_copy(out, out_size, type_name);
}

bool
is_remote_future_expr(TranspilerCtx *ctx, ASTNode *expr)
{
    if (expr == NULL)
        return false;
    if (expr->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, ast_identifier_name(expr));
        return type_name != NULL && strncmp(type_name, "RemoteFuture<", 13) == 0;
    }
    return false;
}

bool
lookup_future_inner_type_copy(TranspilerCtx *ctx, ASTNode *expr,
                              char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (expr == NULL)
        return pergyra_str_copy(out, out_size, "Void");

    if (expr->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx,
            ast_identifier_name(expr));
        if (type_name != NULL
            && (strncmp(type_name, "Future<", 7) == 0
                || strncmp(type_name, "RemoteFuture<", 13) == 0)) {
            return slot_inner_type_name_copy(type_name, out, out_size);
        }
    }

    if (expr != NULL && expr->type == AST_SPAWN_EXPR) {
        return infer_spawn_return_type_name_copy(ctx, expr, out, out_size);
    }

    return pergyra_str_copy(out, out_size, "Void");
}
