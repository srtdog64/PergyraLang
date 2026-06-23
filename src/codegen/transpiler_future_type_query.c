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
#include "transpiler_inventory_view.h"
#include "transpiler_mir_signature.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

static const char *
future_owned_type_to_scratch(TranspilerCtx *ctx, char *owned,
                             const char *fallback)
{
    const char *result = NULL;

    if (owned != NULL)
        result = transpiler_scratch_strdup(ctx, owned);
    free(owned);
    if (result != NULL)
        return result;
    return fallback != NULL ? fallback : "Unknown";
}

const char *
infer_spawn_return_type_name_scratch(TranspilerCtx *ctx, ASTNode *spawn_expr)
{
    ASTNode *target = ast_spawn_function(spawn_expr);
    const char *function_name = NULL;
    ASTNode *call = NULL;

    if (target == NULL)
        return "Unknown";

    if (target->type == AST_CALL
        && ast_call_callee(target) != NULL
        && ast_call_callee(target)->type == AST_IDENTIFIER) {
        call = target;
        function_name = ast_identifier_name(ast_call_callee(target));
    } else if (target->type == AST_IDENTIFIER) {
        function_name = ast_identifier_name(target);
    } else if (target->type == AST_FUNC_DECL) {
        if (ast_func_return_type(target) != NULL)
            return future_owned_type_to_scratch(ctx,
                render_type_name_in_ctx(ctx, ast_func_return_type(target)),
                "Unknown");
        return "Void";
    }

    if (function_name == NULL)
        return "Unknown";

    ASTNode *decl = find_function_decl(ctx, function_name);
    if (decl != NULL) {
        ASTNode *return_type = NULL;
        const MIRRoutine *routine =
            transpiler_find_mir_function(ctx, decl);
        bool generic_call = call != NULL
            && transpiler_mir_or_ast_function_is_generic(routine, decl);
        bool extern_func = transpiler_decl_is_extern_function(ctx, decl);
        if (!generic_call && !extern_func && transpiler_active_has_mir(ctx)) {
            if (routine == NULL) {
                transpiler_set_mir_inventory_missing(ctx,
                    "MIR-only C path missing spawn return routine for '%s'",
                    function_name != NULL ? function_name : "<function>");
                return "Unknown";
            }
            if (!transpiler_mir_routine_signature_metadata_complete_for(ctx,
                    routine, decl,
                    TRANSPILER_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME,
                    "MIR-only C path missing spawn return signature metadata for '%s'",
                    "MIR-only C path missing spawn return type-name metadata for '%s'",
                    NULL)) {
                return "Unknown";
            }
            const char *return_type_name =
                transpiler_mir_routine_return_type_name(routine);
            if (return_type_name != NULL)
                return transpiler_scratch_strdup(ctx, return_type_name);
            return_type = transpiler_mir_routine_return_type(routine);
        } else if (generic_call || extern_func) {
            return_type = ast_func_return_type(decl);
        } else {
            return_type = NULL;
        }
        if (return_type != NULL) {
            if (generic_call) {
                GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
                size_t binding_count = 0;
                if (transpiler_infer_generic_call_bindings(ctx, decl, call,
                        bindings, &binding_count)) {
                    return future_owned_type_to_scratch(ctx,
                        transpiler_render_type_name_with_bindings(ctx,
                            return_type, bindings, binding_count),
                        "Unknown");
                }
            }
            return future_owned_type_to_scratch(ctx,
                render_type_name_in_ctx(ctx, return_type), "Unknown");
        }
        return "Void";
    }

    return "Unknown";
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
        return transpiler_type_name_is_remote_future(type_name);
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
        return false;

    if (expr->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx,
            ast_identifier_name(expr));
        if (type_name != NULL
            && transpiler_type_name_is_any_future(type_name)) {
            return slot_inner_type_name_copy(type_name, out, out_size);
        }
    }

    if (expr != NULL && expr->type == AST_SPAWN_EXPR) {
        if (!infer_spawn_return_type_name_copy(ctx, expr, out, out_size))
            return false;
        return out[0] != '\0' && strcmp(out, "Unknown") != 0;
    }

    return false;
}
