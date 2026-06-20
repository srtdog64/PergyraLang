/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend call expression type inference owner.
 */

#include "transpiler_expr_call_type_infer.h"

#include <stdlib.h>
#include <string.h>

#include "codegen_slot_type_policy.h"
#include "transpiler_builtin_type_table.h"
#include "transpiler_channel_type_query.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_expr_type_infer_call_policy.h"
#include "transpiler_future_type_query.h"
#include "transpiler_generic_binding_query.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_mir_signature.h"
#include "transpiler_nominal.h"
#include "transpiler_option_context.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

#include "../parser/ast_api.h"

const char *
transpiler_expr_infer_call_type_name(TranspilerCtx *ctx, ASTNode *expr)
{
    if (expr == NULL || expr->type != AST_CALL)
        return "Unknown";

    if (ast_call_callee(expr) != NULL
        && ast_call_callee(expr)->type == AST_MEMBER_ACCESS
        && ast_member_name(ast_call_callee(expr)) != NULL) {
        ASTNode *receiver = ast_member_object(ast_call_callee(expr));
        const char *method_name = ast_member_name(ast_call_callee(expr));
        const char *receiver_type =
            transpiler_expr_infer_type_name(ctx, receiver);
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
            if (transpiler_type_name_is_array_or_slice(receiver_type)
                && strcmp(method_name, "Slice") == 0) {
                const char *inner =
                    transpiler_infer_slot_inner_type_name(ctx, receiver_type);
                if (inner == NULL || inner[0] == '\0')
                    return "Unknown";
                return transpiler_infer_arena_format_type_name(ctx, "Slice",
                                                               inner);
            }
        }
        {
            ASTNode *method_decl = NULL;
            const MIRDeclMethod *method_meta = NULL;
            ASTNode *method_return_type = NULL;
            receiver_type =
                transpiler_resolve_nominal_host_expr_type_name(ctx, receiver);
            if (receiver_type != NULL) {
                const char *method_return_type_name = NULL;
                method_meta = transpiler_find_host_method_metadata_in_context(
                    ctx, receiver_type, method_name);
                if (!transpiler_mir_decl_method_metadata_complete_for(ctx,
                        method_meta,
                        receiver_type,
                        method_name,
                        TRANSPILER_MIR_DECL_METHOD_REQUIRE_RETURN_TYPE_NAME,
                        "MIR-only C path missing member-call inference return type-name metadata for '%s.%s'",
                        NULL)) {
                    return "Unknown";
                }
                method_return_type_name =
                    transpiler_mir_decl_method_return_type_name(method_meta);
                if (method_return_type_name != NULL)
                    return transpiler_infer_arena_copy_type_name(
                        ctx, method_return_type_name);
                method_return_type =
                    transpiler_mir_decl_method_return_type(method_meta);
                if (method_return_type == NULL && method_meta == NULL
                    && !transpiler_active_has_mir(ctx)) {
                    method_decl = find_nominal_host_method_decl(
                        ctx, receiver_type, method_name);
                    method_return_type = ast_func_return_type(method_decl);
                }
            }
            if (method_return_type != NULL) {
                char *resolved = render_type_name_in_ctx(ctx,
                                                         method_return_type);
                const char *copied =
                    transpiler_infer_arena_copy_type_name(ctx, resolved);
                free(resolved);
                return copied != NULL ? copied : "Unknown";
            }
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
                const char *arg_type =
                    transpiler_expr_infer_type_name(ctx, arg0);
                if (arg_type != NULL && strcmp(arg_type, "Unknown") != 0)
                    return arg_type;
            }
            return "Int";
        }
        if (op == TRANS_INFER_CALL_MAP_GET && argc >= 1) {
            const char *map_type = transpiler_expr_infer_type_name(ctx, arg0);
            if (transpiler_type_name_is_hashmap(map_type)) {
                char value_buf[64];
                copy_constructed_arg_name_at(map_type, 1, value_buf,
                                             sizeof(value_buf));
                if (value_buf[0] == '\0')
                    return "Unknown";
                return transpiler_infer_arena_copy_type_name(ctx, value_buf);
            }
            return "Unknown";
        }
        if ((op == TRANS_INFER_CALL_MAP_KEYS
             || op == TRANS_INFER_CALL_SET_VALUES)
            && argc >= 1) {
            const char *collection_type =
                transpiler_expr_infer_type_name(ctx, arg0);
            char inner_buf[64];
            if ((op == TRANS_INFER_CALL_MAP_KEYS
                 && transpiler_type_name_is_hashmap(collection_type))
                || (op == TRANS_INFER_CALL_SET_VALUES
                    && transpiler_type_name_is_set(collection_type))) {
                copy_constructed_arg_name_at(collection_type, 0, inner_buf,
                                             sizeof(inner_buf));
                return inner_buf[0] != '\0'
                    ? transpiler_infer_arena_format_type_name(ctx, "Array",
                                                              inner_buf)
                    : "Unknown";
            }
            return "Unknown";
        }
        if (strcmp(name, "SliceCopy") == 0 && argc == 1) {
            const char *slice_type =
                transpiler_expr_infer_type_name(ctx, arg0);
            if (transpiler_type_name_is_slice(slice_type)) {
                const char *inner =
                    transpiler_infer_slot_inner_type_name(ctx, slice_type);
                if (inner == NULL || inner[0] == '\0')
                    return "Unknown";
                return transpiler_infer_arena_format_type_name(ctx, "Array",
                                                               inner);
            }
            return "Unknown";
        }
        if (op == TRANS_INFER_CALL_LIST_GET && argc >= 1) {
            const char *list_type = transpiler_expr_infer_type_name(ctx, arg0);
            if (transpiler_type_name_is_list(list_type))
                return transpiler_infer_slot_inner_type_name(ctx, list_type);
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
            const char *slot_type =
                transpiler_expr_infer_type_name(ctx, arg0);
            const char *inner =
                transpiler_infer_slot_inner_type_name(ctx, slot_type);
            if (inner == NULL || inner[0] == '\0')
                return "Unknown";
            return transpiler_infer_arena_format_type_name(ctx, "ReadView",
                                                           inner);
        }
        if (op == TRANS_INFER_CALL_VIEW_WRITE && argc >= 1) {
            const char *slot_type =
                transpiler_expr_infer_type_name(ctx, arg0);
            const char *inner =
                transpiler_infer_slot_inner_type_name(ctx, slot_type);
            if (inner == NULL || inner[0] == '\0')
                return "Unknown";
            return transpiler_infer_arena_format_type_name(ctx, "WriteView",
                                                           inner);
        }
        if (pgy_codegen_call_name_is_read(name) && argc >= 1) {
            const char *slot_type =
                transpiler_expr_infer_type_name(ctx, arg0);
            if (pgy_codegen_type_name_is_slot_family(slot_type)) {
                const char *inner =
                    transpiler_infer_slot_inner_type_name(ctx, slot_type);
                return (inner != NULL && inner[0] != '\0') ? inner : "Unknown";
            }
        }
        if ((pgy_codegen_call_name_is_write(name)
             || pgy_codegen_call_name_is_release(name))
            && argc >= 1) {
            const char *slot_type =
                transpiler_expr_infer_type_name(ctx, arg0);
            if (pgy_codegen_type_name_is_slot_family(slot_type))
                return "Void";
        }
        if (op == TRANS_INFER_CALL_DEVICE_READ && argc >= 1) {
            const char *slot_type =
                transpiler_expr_infer_type_name(ctx, arg0);
            if (strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                const char *inner =
                    transpiler_infer_slot_inner_type_name(ctx, slot_type);
                return (inner != NULL && inner[0] != '\0') ? inner : "Unknown";
            }
        }
        if (op == TRANS_INFER_CALL_SUBMIT_DEVICE_READ && argc >= 1) {
            const char *slot_type =
                transpiler_expr_infer_type_name(ctx, arg0);
            if (strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                const char *inner =
                    transpiler_infer_slot_inner_type_name(ctx, slot_type);
                if (inner == NULL || inner[0] == '\0')
                    return "Unknown";
                return transpiler_infer_arena_format_type_name(
                    ctx, "RemoteFuture", inner);
            }
            return "Unknown";
        }
        if (transpiler_infer_call_returns_channel_option(op) && argc >= 1) {
            if (transpiler_infer_call_returns_channel_status(op)) {
                return "Option<Bool>";
            } else {
                char inner_buf[128];
                const char *inner = inner_buf;
                (void)channel_inner_type_name_copy(ctx, arg0, inner_buf,
                                                   sizeof(inner_buf));
                if (inner == NULL || inner[0] == '\0')
                    return "Unknown";
                return transpiler_infer_arena_format_type_name(ctx, "Option",
                                                               inner);
            }
        }
        if (op == TRANS_INFER_CALL_SOME && argc == 1) {
            const char *inner = transpiler_expr_infer_type_name(ctx, arg0);
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
            return transpiler_infer_arena_format_type_name(ctx, "Option",
                                                           inner);
        }
        if (op == TRANS_INFER_CALL_NONE_CTOR) {
            char inner_buf[128];
            if (transpiler_contextual_option_inner_type_copy(ctx,
                    inner_buf, sizeof(inner_buf))) {
                return transpiler_infer_arena_format_type_name(ctx, "Option",
                                                               inner_buf);
            }
            return "Unknown";
        }
        if ((op == TRANS_INFER_CALL_IS_SOME
             || op == TRANS_INFER_CALL_IS_NONE)
            && argc == 1) {
            return "Bool";
        }
        simple_type = pgy_builtin_simple_return_type(name);
        if (simple_type != NULL)
            return simple_type;
        if (op == TRANS_INFER_CALL_UNWRAP_OPTION && argc == 1) {
            const char *opt_type = transpiler_expr_infer_type_name(ctx, arg0);
            if (transpiler_type_name_is_option(opt_type)) {
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
        if (transpiler_has_known_nominal_type(ctx, name))
            return name;
        {
            ASTNode *decl = find_callable_decl(ctx, name);
            {
                ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
                const char *host_name = transpiler_decl_name_local(host_decl);
                const MIRDeclMethod *host_method_meta =
                    transpiler_find_host_method_metadata_in_context(
                        ctx, host_name, name);
                ASTNode *host_method = NULL;
                ASTNode *host_return_type = NULL;
                const char *host_return_type_name =
                    transpiler_mir_decl_method_return_type_name(
                        host_method_meta);
                if (!transpiler_mir_decl_method_metadata_complete_for(ctx,
                        host_method_meta,
                        host_name,
                        name,
                        TRANSPILER_MIR_DECL_METHOD_REQUIRE_RETURN_TYPE_NAME,
                        "MIR-only C path missing hosted self-call inference return type-name metadata for '%s.%s'",
                        NULL)) {
                    return "Unknown";
                }
                if (host_return_type_name != NULL) {
                    return transpiler_infer_arena_copy_type_name(
                        ctx, host_return_type_name);
                }
                host_return_type =
                    transpiler_mir_decl_method_return_type(host_method_meta);
                if (host_method_meta == NULL) {
                    host_method = current_host_method_decl(ctx, name);
                    if (host_method != NULL && transpiler_active_has_mir(ctx)) {
                        transpiler_set_mir_inventory_missing(ctx,
                            "MIR-only C path missing hosted self-call inference method metadata for '%s.%s'",
                            host_name != NULL ? host_name : "(anonymous-host)",
                            name != NULL ? name : "(anonymous)");
                        return "Unknown";
                    }
                    if (host_method != NULL)
                        host_return_type = ast_func_return_type(host_method);
                }
                if (host_return_type != NULL) {
                    char *resolved =
                        render_type_name_in_ctx(ctx, host_return_type);
                    const char *copied =
                        transpiler_infer_arena_copy_type_name(ctx, resolved);
                    free(resolved);
                    return copied != NULL ? copied : "Unknown";
                }
            }

            if (decl != NULL && decl->type == AST_INTENT_DECL)
                return "Bool";
            if (decl == NULL || decl->type != AST_FUNC_DECL)
                return "Unknown";
            {
                ASTNode *return_type = NULL;
                const MIRRoutine *routine =
                    transpiler_find_mir_function(ctx, decl);
                bool generic_call =
                    transpiler_mir_or_ast_function_is_generic(routine, decl);
                bool extern_func =
                    transpiler_decl_is_extern_function(ctx, decl);
                if (!generic_call && !extern_func
                    && transpiler_active_has_mir(ctx)) {
                    const char *return_type_name = NULL;
                    if (routine == NULL) {
                        transpiler_set_mir_inventory_missing(ctx,
                            "MIR-only C path missing function inference routine for '%s'",
                            name != NULL ? name : "(anonymous-call)");
                        return "Unknown";
                    }
                    if (!transpiler_mir_routine_signature_metadata_complete_for(ctx,
                            routine,
                            decl,
                            TRANSPILER_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME,
                            "MIR-only C path missing function inference signature metadata for '%s'",
                            "MIR-only C path missing function inference return type-name metadata for '%s'",
                            NULL)) {
                        return "Unknown";
                    }
                    return_type_name =
                        transpiler_mir_routine_return_type_name(routine);
                    if (return_type_name != NULL) {
                        return transpiler_infer_arena_copy_type_name(
                            ctx, return_type_name);
                    }
                    return_type =
                        transpiler_mir_routine_return_type(routine);
                } else if (!transpiler_active_has_mir(ctx)
                           || generic_call || extern_func) {
                    return_type = ast_func_return_type(decl);
                } else {
                    return_type = NULL;
                }
                if (return_type != NULL) {
                    char *resolved = NULL;
                    if (generic_call) {
                        GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
                        size_t binding_count = 0;
                        if (transpiler_infer_generic_call_bindings(ctx,
                                decl, expr, bindings, &binding_count)) {
                            resolved =
                                transpiler_render_type_name_with_bindings(
                                    ctx, return_type, bindings,
                                    binding_count);
                        }
                    }
                    if (resolved == NULL)
                        resolved = render_type_name_in_ctx(ctx, return_type);
                    {
                        const char *copied =
                            transpiler_infer_arena_copy_type_name(ctx,
                                                                  resolved);
                        free(resolved);
                        return copied != NULL ? copied : "Unknown";
                    }
                }
            }
        }
    }

    return "Unknown";
}
