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

#include "transpiler_builtin_type_table.h"
#include "transpiler_channel_type_query.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_enum.h"
#include "transpiler_generic_binding_query.h"
#include "transpiler_expr_type_infer_call_policy.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_nominal.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

#include "codegen_slot_type_policy.h"
#include "../parser/ast_api.h"

const char *transpiler_contextual_option_type_name(TranspilerCtx *ctx);
bool transpiler_contextual_option_inner_type_copy(TranspilerCtx *ctx,
                                                  char *out,
                                                  size_t out_size);

static const char *
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

static const char *
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

static const char *
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
        return ast_number_value(expr) == (int64_t)ast_number_value(expr)
            ? "Int"
            : "Float";
    case AST_STRING:
        return "String";
    case AST_BOOLEAN:
        return "Bool";
    case AST_ARRAY_LITERAL: {
        const char *inner = NULL;
        if (ast_array_literal_count(expr) > 0) {
            inner = infer_expression_type_name(ctx, ast_array_literal_element(expr, 0));
        } else if (ctx != NULL
                   && ctx->expected_type != NULL
                   && strncmp(ctx->expected_type, "Array<", 6) == 0) {
            inner = transpiler_infer_slot_inner_type_name(ctx,
                ctx->expected_type);
        }
        if (inner == NULL || inner[0] == '\0')
            inner = "Unknown";
        return transpiler_infer_arena_format_type_name(ctx, "Array", inner);
    }
    case AST_ARRAY_ACCESS: {
        const char *array_type = infer_expression_type_name(ctx, ast_array_access_array(expr));
        if (strncmp(array_type, "Array<", 6) == 0 || strncmp(array_type, "Slice<", 6) == 0)
            return transpiler_infer_slot_inner_type_name(ctx, array_type);
        return "Unknown";
    }
    case AST_IDENTIFIER: {
        const char *identifier_name = ast_identifier_name(expr);
        if (identifier_name == NULL)
            return "Unknown";
        if (strcmp(identifier_name, "None") == 0) {
            const char *context_type = transpiler_contextual_option_type_name(ctx);
            return context_type != NULL ? context_type : "Option<Unknown>";
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
    case AST_MEMBER_ACCESS: {
        const char *resolved = transpiler_resolve_nominal_host_expr_type_name(ctx, expr);
        if (resolved != NULL)
            return resolved;
        if (ast_member_object(expr) != NULL && ast_member_name(expr) != NULL) {
            const char *obj_type = infer_expression_type_name(ctx, ast_member_object(expr));
            if (obj_type != NULL) {

                ASTNode *obj_decl = find_class_decl(ctx, obj_type);
                if (obj_decl != NULL) {
                    size_t field_count = 0;
                    ClassField **fields = ast_class_fields(obj_decl, &field_count);
                    for (size_t fi = 0; fi < field_count; fi++) {
                        ClassField *f = fields != NULL ? fields[fi] : NULL;
                        if (f != NULL && f->name != NULL && f->type != NULL
                            && strcmp(f->name, ast_member_name(expr)) == 0) {
                            char *ft = render_type_name_in_ctx(ctx, f->type);
                            if (ft != NULL) {
                                const char *copied =
                                    transpiler_infer_arena_copy_type_name(ctx, ft);
                                free(ft);
                                return copied != NULL ? copied : "Unknown";
                            }
                        }
                    }
                }
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
        if (ast_call_callee(expr) != NULL
            && ast_call_callee(expr)->type == AST_MEMBER_ACCESS
            && ast_member_name(ast_call_callee(expr)) != NULL) {
            ASTNode *receiver = ast_member_object(ast_call_callee(expr));
            const char *method_name = ast_member_name(ast_call_callee(expr));
            const char *receiver_type = infer_expression_type_name(ctx, receiver);
            if (receiver_type != NULL && method_name != NULL) {
                if (pgy_codegen_type_name_is_slot_or_view(receiver_type)
                    && pgy_codegen_call_name_is_read(method_name)) {
                    return transpiler_infer_slot_inner_type_name(ctx,
                        receiver_type);
                }
                if (pgy_codegen_type_name_is_device_slot(receiver_type)
                    && pgy_codegen_call_name_is_read(method_name)) {
                    return transpiler_infer_slot_inner_type_name(ctx,
                        receiver_type);
                }
                if (pgy_codegen_type_name_is_slot_family(receiver_type)
                    && (pgy_codegen_call_name_is_write(method_name)
                        || pgy_codegen_call_name_is_release(method_name))) {
                    return "Void";
                }
                if ((strncmp(receiver_type, "Array<", 6) == 0
                    || strncmp(receiver_type, "Slice<", 6) == 0)
                    && strcmp(method_name, "Slice") == 0) {
                    const char *inner = transpiler_infer_slot_inner_type_name(
                        ctx, receiver_type);
                    if (inner == NULL || inner[0] == '\0')
                        return "Unknown";
                    return transpiler_infer_arena_format_type_name(
                        ctx, "Slice", inner);
                }
            }
            ASTNode *method_decl = NULL;
            receiver_type = transpiler_resolve_nominal_host_expr_type_name(ctx, receiver);
            if (receiver_type != NULL) {
                method_decl = find_nominal_host_method_decl(ctx, receiver_type,
                    method_name);
            }
            ASTNode *method_return_type = ast_func_return_type(method_decl);
            if (method_return_type != NULL) {
                char *resolved = render_type_name_in_ctx(ctx,
                    method_return_type);
                const char *copied =
                    transpiler_infer_arena_copy_type_name(ctx, resolved);
                free(resolved);
                return copied != NULL ? copied : "Unknown";
            }
        }
        if (ast_call_callee(expr) != NULL
            && ast_call_callee(expr)->type == AST_IDENTIFIER
            && ast_identifier_name(ast_call_callee(expr)) != NULL) {
            const char *name = ast_identifier_name(ast_call_callee(expr));
            size_t argc = ast_call_arg_count(expr);
            ASTNode *arg0 = ast_call_argument(expr, 0);
            const char *simple_type = NULL;
            TranspilerInferCallOp op = transpiler_infer_call_lookup(name);
            if (transpiler_infer_call_is_numeric_passthrough(op)) {
                if (argc >= 1) {
                    const char *arg_type = infer_expression_type_name(ctx,
                        arg0);
                    if (arg_type != NULL && strcmp(arg_type, "Unknown") != 0)
                        return arg_type;
                }
                return "Int";
            }
            if (op == TRANS_INFER_CALL_MAP_GET && argc >= 1) {
                const char *map_type = infer_expression_type_name(ctx,
                    arg0);
                if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
                    char value_buf[64];
                    copy_constructed_arg_name_at(map_type, 1,
                        value_buf, sizeof(value_buf));
                    if (value_buf[0] == '\0')
                        return "Unknown";
                    return transpiler_infer_arena_copy_type_name(ctx, value_buf);
                }
                return "Unknown";
            }
            if (op == TRANS_INFER_CALL_MAP_KEYS && argc >= 1) {
                const char *map_type = infer_expression_type_name(ctx,
                    arg0);
                if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
                    char key_buf[64];
                    copy_constructed_arg_name_at(map_type, 0,
                        key_buf, sizeof(key_buf));
                    if (key_buf[0] == '\0')
                        return "Unknown";
                    return transpiler_infer_arena_format_type_name(
                        ctx, "Array", key_buf);
                }
                return "Unknown";
            }
            if (strcmp(name, "SliceCopy") == 0 && argc == 1) {
                const char *slice_type = infer_expression_type_name(ctx,
                    arg0);
                if (slice_type != NULL && strncmp(slice_type, "Slice<", 6) == 0) {
                    const char *inner = transpiler_infer_slot_inner_type_name(
                        ctx, slice_type);
                    if (inner == NULL || inner[0] == '\0')
                        return "Unknown";
                    return transpiler_infer_arena_format_type_name(
                        ctx, "Array", inner);
                }
                return "Unknown";
            }
            if (op == TRANS_INFER_CALL_LIST_GET && argc >= 1) {
                const char *list_type = infer_expression_type_name(ctx,
                    arg0);
                if (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                    return transpiler_infer_slot_inner_type_name(ctx,
                        list_type);
                return "Unknown";
            }
            if (pgy_codegen_call_name_is_claim_device_slot(name)) {
                if (ctx != NULL
                    && ctx->active_type_hint != NULL
                    && strncmp(ctx->active_type_hint, "DeviceSlot<", 11) == 0) {
                    return ctx->active_type_hint;
                }
                return "Unknown";
            }
            if (op == TRANS_INFER_CALL_VIEW_READ && argc >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    arg0);
                const char *inner = transpiler_infer_slot_inner_type_name(ctx,
                    slot_type);
                if (inner == NULL || inner[0] == '\0')
                    return "Unknown";
                return transpiler_infer_arena_format_type_name(
                    ctx, "ReadView", inner);
            }
            if (op == TRANS_INFER_CALL_VIEW_WRITE && argc >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    arg0);
                const char *inner = transpiler_infer_slot_inner_type_name(ctx,
                    slot_type);
                if (inner == NULL || inner[0] == '\0')
                    return "Unknown";
                return transpiler_infer_arena_format_type_name(
                    ctx, "WriteView", inner);
            }
            if (op == TRANS_INFER_CALL_MEASURE
                || op == TRANS_INFER_CALL_QUBIT_STATE)
                return "Int";
            if (pgy_codegen_call_name_is_read(name) && argc >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    arg0);
                if (pgy_codegen_type_name_is_slot_family(slot_type)) {
                    const char *inner = transpiler_infer_slot_inner_type_name(
                        ctx, slot_type);
                    return (inner != NULL && inner[0] != '\0') ? inner : "Unknown";
                }
            }
            if ((pgy_codegen_call_name_is_write(name)
                 || pgy_codegen_call_name_is_release(name))
                && argc >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    arg0);
                if (pgy_codegen_type_name_is_slot_family(slot_type)) {
                    return "Void";
                }
            }
            if (op == TRANS_INFER_CALL_DEVICE_READ && argc >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    arg0);
                if (strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                    const char *inner = transpiler_infer_slot_inner_type_name(
                        ctx, slot_type);
                    return (inner != NULL && inner[0] != '\0') ? inner : "Unknown";
                }
            }
            if (op == TRANS_INFER_CALL_SUBMIT_DEVICE_READ && argc >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    arg0);
                if (strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                    const char *inner = transpiler_infer_slot_inner_type_name(
                        ctx, slot_type);
                    if (inner == NULL || inner[0] == '\0')
                        return "Unknown";
                    return transpiler_infer_arena_format_type_name(
                        ctx, "RemoteFuture", inner);
                }
                return "Unknown";
            }
            if (transpiler_infer_call_returns_channel_option(op)
                && argc >= 1) {
                if (transpiler_infer_call_returns_channel_status(op)) {
                    return "Option<Bool>";
                } else {
                    char inner_buf[128];
                    const char *inner = inner_buf;
                    (void)channel_inner_type_name_copy(ctx,
                        arg0, inner_buf,
                        sizeof(inner_buf));
                    if (inner == NULL || inner[0] == '\0')
                        return "Unknown";
                    return transpiler_infer_arena_format_type_name(
                        ctx, "Option", inner);
                }
            }
            if (op == TRANS_INFER_CALL_SOME && argc == 1) {
                const char *inner = infer_expression_type_name(ctx, arg0);
                char inner_buf[128];
                if (inner == NULL || inner[0] == '\0'
                    || strcmp(inner, "Unknown") == 0) {
                    if (transpiler_contextual_option_inner_type_copy(ctx,
                            inner_buf, sizeof(inner_buf))) {
                        inner = inner_buf;
                    }
                }
                if (inner == NULL || inner[0] == '\0')
                    inner = "Unknown";
                return transpiler_infer_arena_format_type_name(
                    ctx, "Option", inner);
            }
            if (op == TRANS_INFER_CALL_NONE_CTOR) {
                const char *context_type = transpiler_contextual_option_type_name(ctx);
                return context_type != NULL ? context_type : "Option<Unknown>";
            }
            if ((op == TRANS_INFER_CALL_IS_SOME
                 || op == TRANS_INFER_CALL_IS_NONE)
                && argc == 1)
                return "Bool";
            simple_type = pgy_builtin_simple_return_type(name);
            if (simple_type != NULL)
                return simple_type;
            if (op == TRANS_INFER_CALL_UNWRAP_OPTION && argc == 1) {
                const char *opt_type = infer_expression_type_name(ctx, arg0);
                if (strncmp(opt_type, "Option<", 7) == 0) {
                    char inner_buf[128];
                    if (slot_inner_type_name_copy(opt_type, inner_buf,
                            sizeof(inner_buf))) {
                        return transpiler_infer_arena_copy_type_name(ctx,
                            inner_buf);
                    }
                }
            }
            if (op == TRANS_INFER_CALL_TO_TOBJECT && argc >= 1
                && arg0 != NULL
                && arg0->type == AST_IDENTIFIER
                && ast_identifier_name(arg0) != NULL) {
                return ast_identifier_name(arg0);
            }
            if (op == TRANS_INFER_CALL_TO_OBJECT && argc >= 1
                && arg0 != NULL
                && arg0->type == AST_IDENTIFIER
                && ast_identifier_name(arg0) != NULL) {
                return ast_identifier_name(arg0);
            }
            if (transpiler_has_known_nominal_type(ctx, name)) {
                return name;
            }
            if (find_intent_decl(ctx, name) != NULL)
                return "Bool";

            {
                ASTNode *host_method = current_host_method_decl(ctx, name);
                ASTNode *host_return_type = ast_func_return_type(host_method);
                if (host_return_type != NULL) {
                    char *resolved = render_type_name_in_ctx(ctx,
                        host_return_type);
                    const char *copied =
                        transpiler_infer_arena_copy_type_name(ctx, resolved);
                    free(resolved);
                    return copied != NULL ? copied : "Unknown";
                }
            }

            {
                ASTNode *decl = find_function_decl(ctx, name);
                ASTNode *return_type = ast_func_return_type(decl);
                if (return_type != NULL) {
                    char *resolved = NULL;
                    if (transpiler_func_has_generic_params(decl)) {
                        GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
                        size_t binding_count = 0;
                        if (transpiler_infer_generic_call_bindings(ctx, decl,
                                expr, bindings, &binding_count)) {
                            resolved =
                                transpiler_render_type_name_with_bindings(ctx,
                                return_type, bindings, binding_count);
                        }
                    }
                    if (resolved == NULL)
                        resolved = render_type_name_in_ctx(ctx, return_type);
                    const char *copied =
                        transpiler_infer_arena_copy_type_name(ctx, resolved);
                    free(resolved);
                    return copied != NULL ? copied : "Unknown";
                }
            }
        }
        return "Unknown";
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
    return infer_expression_type_name(ctx, expr);
}
