/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend expression type inference owner.
 */

#include "transpiler_expr_type_infer.h"
#undef infer_expression_type_name

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_match_variant_policy.h"
#include "codegen_builtin_type_table.h"
#include "transpiler_channel_type_query.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_expr_call_type_infer.h"
#include "transpiler_enum.h"
#include "transpiler_generic_binding_query.h"
#include "transpiler_expr_type_infer_call_policy.h"
#include "transpiler_future_type_query.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_signature.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_nominal.h"
#include "transpiler_option_context.h"
#include "transpiler_symbols.h"
#include "codegen_type_mapping.h"
#include "transpiler_type_render.h"

#include "transpiler_type_require.h"

#include "codegen_slot_type_policy.h"
#include "../parser/ast_api.h"

const char *
transpiler_infer_arena_copy_type_name(TranspilerCtx *ctx,
                                      const char *type_name)
{
    if (ctx == NULL || type_name == NULL)
        return type_name != NULL ? type_name : "Unknown";
    {
        char *copied = pgy_arena_strdup(&ctx->arena, type_name);
        return copied != NULL ? copied : "Unknown";
    }
}

const char *
transpiler_infer_arena_format_type_name(TranspilerCtx *ctx,
                                        const char *prefix,
                                        const char *inner)
{
    if (ctx == NULL || prefix == NULL || inner == NULL)
        return "Unknown";
    {
        char *formatted = pgy_arena_fmt(&ctx->arena, "%s<%s>", prefix, inner);
        return formatted != NULL ? formatted : "Unknown";
    }
}

const char *
transpiler_infer_slot_inner_type_name(TranspilerCtx *ctx,
                                      const char *type_name)
{
    char inner_buf[128];
    char *copied;

    if (ctx == NULL || type_name == NULL)
        return "Unknown";
    if (!slot_inner_type_name_copy(type_name, inner_buf, sizeof(inner_buf)))
        return "Unknown";
    if (inner_buf[0] == '\0')
        return "Unknown";
    copied = pgy_arena_strdup(&ctx->arena, inner_buf);
    return copied != NULL ? copied : "Unknown";
}

static const char *
transpiler_promote_numeric_type_name(const char *left_type,
                                     const char *right_type)
{
    if ((left_type != NULL && strcmp(left_type, "Double") == 0)
        || (right_type != NULL && strcmp(right_type, "Double") == 0))
        return "Double";
    if ((left_type != NULL && strcmp(left_type, "Float") == 0)
        || (right_type != NULL && strcmp(right_type, "Float") == 0))
        return "Float";
    if ((left_type != NULL && strcmp(left_type, "Long") == 0)
        || (right_type != NULL && strcmp(right_type, "Long") == 0))
        return "Long";
    return "Int";
}

static const char *
infer_expression_type_name(TranspilerCtx *ctx, ASTNode *expr)
{
    if (expr == NULL)
        return "Unknown";

    switch (expr->type) {
    case AST_NUMBER:
        if (ast_number_is_long(expr))
            return "Long";
        if (ast_number_is_float(expr))
            return "Float";
        return ast_number_value(expr) == (int64_t)ast_number_value(expr)
            ? "Int"
            : "Float";
    case AST_STRING:
        return "String";
    case AST_BOOLEAN:
        return "Bool";
    case AST_ARRAY_LITERAL: {
        const char *inner = NULL;
        const char *expected_type = ctx != NULL ? ctx->expected_type : NULL;
        /* A `[...]` literal is a sequence; its concrete constructor comes from
         * the binding's expected type (Array by default, List/Queue when so
         * declared). */
        const char *ctor = "Array";
        if (expected_type != NULL) {
            const char *alias_target =
                transpiler_type_alias_target_type_name_from_headers(
                    ctx, expected_type);
            if (alias_target != NULL)
                expected_type = alias_target;
        }
        if (expected_type != NULL) {
            if (transpiler_type_name_is_list(expected_type))
                ctor = "List";
            else if (transpiler_type_name_is_queue(expected_type))
                ctor = "Queue";
        }
        if (ast_array_literal_count(expr) > 0) {
            inner = infer_expression_type_name(ctx, ast_array_literal_element(expr, 0));
        } else if (ctx != NULL
                   && expected_type != NULL
                   && (transpiler_type_name_is_array(expected_type)
                       || transpiler_type_name_is_list(expected_type)
                       || transpiler_type_name_is_queue(expected_type))) {
            inner = transpiler_infer_slot_inner_type_name(ctx,
                expected_type);
        }
        if (inner == NULL || inner[0] == '\0')
            inner = "Unknown";
        return transpiler_infer_arena_format_type_name(ctx, ctor, inner);
    }
    case AST_CAST: {
        const char *target = ast_cast_target_type(expr);
        return target != NULL ? target : "Unknown";
    }
    case AST_TYPE_TEST:
        return "Bool";
    case AST_MAP_LITERAL: {
        const char *k = "Unknown";
        const char *v = "Unknown";
        if (ast_map_literal_count(expr) > 0) {
            k = infer_expression_type_name(ctx, ast_map_literal_key(expr, 0));
            v = infer_expression_type_name(ctx, ast_map_literal_value(expr, 0));
        } else if (ctx != NULL && ctx->expected_type != NULL
                   && transpiler_type_name_is_hashmap(ctx->expected_type)) {
            char *kept = pgy_arena_fmt(&ctx->arena, "%s", ctx->expected_type);
            return kept != NULL ? kept : "Unknown";
        }
        if (k == NULL || k[0] == '\0')
            k = "Unknown";
        if (v == NULL || v[0] == '\0')
            v = "Unknown";
        {
            char *formatted = pgy_arena_fmt(&ctx->arena,
                "HashMap<%s, %s>", k, v);
            return formatted != NULL ? formatted : "Unknown";
        }
    }
    case AST_SET_LITERAL: {
        const char *e = "Unknown";
        if (ast_set_literal_count(expr) > 0) {
            e = infer_expression_type_name(ctx, ast_set_literal_element(expr, 0));
        } else if (ctx != NULL && ctx->expected_type != NULL
                   && transpiler_type_name_is_hashmap(ctx->expected_type)) {
            char *kept = pgy_arena_fmt(&ctx->arena, "%s", ctx->expected_type);
            return kept != NULL ? kept : "Unknown";
        } else if (ctx != NULL && ctx->expected_type != NULL
                   && transpiler_type_name_is_set(ctx->expected_type)) {
            char *kept = pgy_arena_fmt(&ctx->arena, "%s", ctx->expected_type);
            return kept != NULL ? kept : "Unknown";
        }
        if (e == NULL || e[0] == '\0')
            e = "Unknown";
        {
            char *formatted = pgy_arena_fmt(&ctx->arena, "Set<%s>", e);
            return formatted != NULL ? formatted : "Unknown";
        }
    }
    case AST_ARRAY_ACCESS: {
        const char *array_type = infer_expression_type_name(ctx, ast_array_access_array(expr));
        if (transpiler_type_name_is_array_or_slice(array_type))
            return transpiler_infer_slot_inner_type_name(ctx, array_type);
        return "Unknown";
    }
    case AST_IDENTIFIER: {
        const char *identifier_name = ast_identifier_name(expr);
        if (identifier_name == NULL)
            return "Unknown";
        if (pgy_codegen_match_variant_lookup(identifier_name)
                == PGY_MATCH_VARIANT_NONE_CTOR) {
            char inner_buf[128];
            if (transpiler_contextual_option_inner_type_copy(ctx,
                    inner_buf, sizeof(inner_buf))) {
                return transpiler_infer_arena_format_type_name(ctx, "Option",
                                                               inner_buf);
            }
            return "Unknown";
        }
        ASTNode *alias_expr = lookup_alias_expr(ctx, identifier_name);
        if (alias_expr != NULL)
            return infer_expression_type_name(ctx, alias_expr);
        const char *type_name = lookup_typed_var(ctx, identifier_name);
        if (type_name != NULL)
            return type_name;
        type_name = transpiler_current_field_type_name(ctx, identifier_name);
        if (type_name != NULL)
            return type_name;
        {
            char enum_variant[128];
            if (lookup_enum_variant_qualified_name_copy(ctx,
                    identifier_name,
                    enum_variant, sizeof(enum_variant))) {
                size_t len = strcspn(enum_variant, "_");
                char enum_name[128];
                if (len >= sizeof(enum_name))
                    len = sizeof(enum_name) - 1;
                memcpy(enum_name, enum_variant, len);
                enum_name[len] = '\0';
                return transpiler_infer_arena_copy_type_name(ctx, enum_name);
            }
        }
        return "Unknown";
    }
    case AST_CHANNEL_RECV: {
        ASTNode *channel = ast_channel_recv_channel(expr);
        char inner_buf[128];
        if (ctx == NULL)
            return "Unknown";
        if (channel_inner_type_name_copy(ctx, channel, inner_buf,
                sizeof(inner_buf))
            && inner_buf[0] != '\0'
            && strcmp(inner_buf, "Unknown") != 0) {
            return transpiler_infer_arena_copy_type_name(ctx, inner_buf);
        }
        return "Unknown";
    }
    case AST_SPAWN_EXPR: {
        const char *inner = infer_spawn_return_type_name_scratch(ctx, expr);
        if (inner == NULL || inner[0] == '\0'
            || strcmp(inner, "Unknown") == 0)
            return "Unknown";
        return transpiler_infer_arena_format_type_name(ctx, "Future", inner);
    }
    case AST_AWAIT_EXPR: {
        ASTNode *awaited = ast_await_expression(expr);
        char inner_buf[128];
        const char *inner;

        if (ctx == NULL)
            return "Unknown";
        if (!lookup_future_inner_type_copy(ctx, awaited, inner_buf,
                sizeof(inner_buf))
            || inner_buf[0] == '\0'
            || strcmp(inner_buf, "Unknown") == 0) {
            return "Unknown";
        }
        inner = transpiler_infer_arena_copy_type_name(ctx, inner_buf);
        if (is_remote_future_expr(ctx, awaited))
            return transpiler_infer_arena_format_type_name(ctx, "Result", inner);
        return inner;
    }
    case AST_MEMBER_ACCESS: {
        const char *resolved = transpiler_resolve_nominal_host_expr_type_name(ctx, expr);
        if (resolved != NULL)
            return resolved;
        if (ast_member_object(expr) != NULL && ast_member_name(expr) != NULL) {
            const char *obj_type = infer_expression_type_name(ctx, ast_member_object(expr));
            if (obj_type != NULL) {
                const char *member_type =
                    transpiler_lookup_nominal_host_member_type_name(
                        ctx, obj_type, ast_member_name(expr));
                if (member_type != NULL)
                    return member_type;
            }
        }
        return "Unknown";
    }
    case AST_BINARY: {
        PgyTokenType op = ast_binary_operator(expr).type;
        const char *left_type = infer_expression_type_name(ctx, ast_binary_left(expr));
        const char *right_type = infer_expression_type_name(ctx, ast_binary_right(expr));
        if (op == TOKEN_PLUS) {
            if ((left_type != NULL && strcmp(left_type, "String") == 0)
                || (right_type != NULL && strcmp(right_type, "String") == 0)) {
                return "String";
            }
            {
                ASTNode *cursor = ast_binary_left(expr);
                while (cursor != NULL && cursor->type == AST_BINARY
                       && ast_binary_operator(cursor).type == TOKEN_PLUS) {
                    cursor = ast_binary_left(cursor);
                }
                if (cursor != NULL) {
                    const char *leaf_type = infer_expression_type_name(ctx, cursor);
                    if (leaf_type != NULL && strcmp(leaf_type, "String") == 0)
                        return "String";
                }
            }
            return transpiler_promote_numeric_type_name(left_type, right_type);
        }
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL
            || op == TOKEN_AND || op == TOKEN_OR) {
            return "Bool";
        }
        if (op == TOKEN_MINUS || op == TOKEN_STAR
            || op == TOKEN_SLASH || op == TOKEN_PERCENT) {
            return transpiler_promote_numeric_type_name(left_type, right_type);
        }
        return "Unknown";
    }
    case AST_CALL:
        return transpiler_expr_infer_call_type_name(ctx, expr);
    case AST_UNARY:
        if (ast_unary_operator(expr).type == TOKEN_NOT)
            return "Bool";
        return infer_expression_type_name(ctx, ast_unary_operand(expr));
    default:
        return "Unknown";
    }
}

const char *
transpiler_expr_infer_type_name(TranspilerCtx *ctx, ASTNode *expr)
{
    const char *name = infer_expression_type_name(ctx, expr);

    /* Inside a generic specialization window, inferred names may still be
     * spelled in the declaration's type parameters ("T", "Option<T>").
     * Every suffix derivation downstream (Some_/IsSome_/UnwrapOption_)
     * consumes this result, so substitute the active bindings HERE — the
     * one choke point — instead of at each consumer. */
    if (ctx == NULL || ctx->generic_binding_count <= 0 || name == NULL)
        return name;
    {
        char subst[256];
        const char *applied = transpiler_type_name_apply_generic_bindings(
            ctx, name, subst, sizeof(subst));
        if (applied == name)
            return name;
        return transpiler_infer_arena_copy_type_name(ctx, applied);
    }
}
