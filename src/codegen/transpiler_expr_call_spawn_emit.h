#ifndef PGY_TRANSPILER_EXPR_CALL_SPAWN_EMIT_H
#define PGY_TRANSPILER_EXPR_CALL_SPAWN_EMIT_H

#include "../parser/ast_api.h"
#include "transpiler_call_result_option_builtin_emit.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_overlay_host_fields.h"
#include "transpiler_projection_field_path.h"
#include "transpiler_role_ability_helpers.h"

static char *
emit_call_member_style(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    /* Method-call style slot operations: slot.Write(val), slot.Read(), slot.Release() */
    if (callee->type == AST_MEMBER_ACCESS) {
        const char *method = ast_member_name(callee);
        ASTNode *obj = ast_member_object(callee);

        if (obj != NULL && obj->type == AST_MEMBER_ACCESS && method != NULL) {
            ASTNode *party_node = ast_member_object(obj);
            const char *slot_name = ast_member_name(obj);

            if (party_node != NULL && party_node->type == AST_IDENTIFIER
                && slot_name != NULL) {
                const char *party_var = ast_identifier_name(party_node);
                const char *party_type = lookup_typed_var(ctx, party_var);
                ASTNode *party_decl = find_party_decl(ctx, party_type);
                char *ability_name =
                    transpiler_party_slot_method_ability_tag(
                        ctx, party_decl, slot_name, method);

                if (ability_name != NULL) {
                    CodeBuf *args_buf = codebuf_create();
                    codebuf_write(args_buf, "%s.%s", party_var, slot_name);
                    for (size_t i = 0; i < ast_call_arg_count(call); i++) {
                        char *arg = emit_expression(ast_call_argument(call, i), ctx);
                        codebuf_write(args_buf, ", %s", arg);
                        free(arg);
                    }

                    char *result = strdup_fmt("%s.%s_%s_vt->%s(%s)",
                                              party_var, slot_name, ability_name,
                                              method, args_buf->data);
                    codebuf_destroy(args_buf);
                    free(ability_name);
                    return result;
                }
            }
        }

        bool is_slot_method = pgy_codegen_call_name_is_slot_operation(method);

        if (obj != NULL && method != NULL) {
            const char *type_name = transpiler_resolve_nominal_host_expr_type_name(ctx, obj);
            if (type_name != NULL && is_nominal_host_type_name(ctx, type_name)) {
                CodeBuf *args_buf = codebuf_create();
                char stable_type_name[128];
                const char *owned_type_name = type_name;
                bool use_self_cell = is_pointer_self_host_type_name(ctx, owned_type_name);
                bool is_self_ident = (obj->type == AST_IDENTIFIER
                    && ast_identifier_name(obj) != NULL
                    && strcmp(ast_identifier_name(obj), "self") == 0);
                ASTNode *method_decl = find_nominal_host_method_decl(ctx, owned_type_name, method);

                snprintf(stable_type_name, sizeof(stable_type_name), "%s",
                    owned_type_name);
                owned_type_name = stable_type_name;
                use_self_cell = is_pointer_self_host_type_name(ctx, owned_type_name);

                if (is_self_ident && use_self_cell) {
                    codebuf_write(args_buf, "self");
                } else {
                    char *obj_expr = emit_expression(obj, ctx);
                    /* Check if receiver is already a pointer (subject-ref param) */
                    bool already_pointer = false;
                    if (obj->type == AST_IDENTIFIER) {
                        TypedVarEntry *entry = lookup_typed_entry(ctx,
                            ast_identifier_name(obj));
                        if (entry != NULL && entry->is_subject_ref)
                            already_pointer = true;
                    }
                    if (use_self_cell && !already_pointer)
                        codebuf_write(args_buf, "&%s", obj_expr);
                    else
                        codebuf_write(args_buf, "%s", obj_expr);
                    free(obj_expr);
                }

                for (size_t i = 0; i < ast_call_arg_count(call); i++) {
                    ASTNode *arg_node = ast_call_argument(call, i);
                    char *arg = emit_expression(arg_node, ctx);
                    bool pass_by_ptr = false;
                    if (method_decl != NULL) {
                        size_t param_index = i;
                        if (ast_func_param_count(method_decl) > 0) {
                            FuncParam *first = ast_func_param(method_decl, 0);
                            if (first != NULL && first->name != NULL
                                && strcmp(first->name, "self") == 0)
                                param_index++;
                        }
                        if (param_index < ast_func_param_count(method_decl)) {
                            FuncParam *param = ast_func_param(method_decl, param_index);
                            char *ptn = (param != NULL && param->type != NULL)
                                ? render_type_name(param->type)
                                : NULL;
                            if (ptn != NULL && is_pointer_self_host_type_name(ctx, ptn))
                                pass_by_ptr = true;
                            free(ptn);
                        }
                    }
                    if (pass_by_ptr) {
                        bool already_pointer = false;
                        bool is_self_arg = (arg_node != NULL
                            && arg_node->type == AST_IDENTIFIER
                            && ast_identifier_name(arg_node) != NULL
                            && strcmp(ast_identifier_name(arg_node), "self") == 0);
                        if (arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
                            TypedVarEntry *entry = lookup_typed_entry(ctx,
                                ast_identifier_name(arg_node));
                            if (entry != NULL && entry->is_subject_ref)
                                already_pointer = true;
                        }
                        if (is_self_arg && current_class_uses_self_cell(ctx))
                            codebuf_write(args_buf, ", self");
                        else if (already_pointer)
                            codebuf_write(args_buf, ", %s", arg);
                        else
                            codebuf_write(args_buf, ", &%s", arg);
                    } else {
                        codebuf_write(args_buf, ", %s", arg);
                    }
                    free(arg);
                }

                {
                    char *result = strdup_fmt("%s_%s(%s)",
                        owned_type_name, method, args_buf->data);
                    const char *source_slot_name =
                        assignment_target_root_slot_name(obj);
                    char *invalidation = NULL;
                    char *action_sync = NULL;
                    char *post_sync = NULL;
                    ASTNode *saved_host_decl = transpiler_current_host_decl_local(ctx);
                    const char *saved_receiver_expr = ctx->current_overlay_receiver_expr;

                    if (saved_host_decl != NULL && saved_host_decl->type == AST_WORLD_DECL) {
                        const char *zone_slot_name = NULL;
                        const char *zone_type_name = NULL;
                        const char *zone_subject_slot_name = NULL;
                        const char *zone_subject_type_name = NULL;

                        if (transpiler_resolve_world_zone_subject_receiver(ctx,
                                obj,
                                &zone_slot_name, &zone_type_name,
                                &zone_subject_slot_name, &zone_subject_type_name)
                            && zone_slot_name != NULL
                            && zone_type_name != NULL
                            && zone_subject_slot_name != NULL
                            && zone_subject_type_name != NULL
                            && strcmp(zone_subject_type_name, owned_type_name) == 0) {
                            ASTNode *zone_decl = find_zone_decl(ctx, zone_type_name);
                            if (zone_decl != NULL)
                                transpiler_bind_current_host_decl_local(ctx, zone_decl);
                            ctx->current_overlay_receiver_expr =
                                strdup_fmt("(&self->%s)", zone_slot_name);
                            source_slot_name = zone_subject_slot_name;
                        }
                    }

                    invalidation =
                        emit_current_overlay_method_projection_invalidation(
                            ctx, source_slot_name, owned_type_name, method_decl);
                    if (ctx->current_overlay_receiver_expr != NULL
                        && ctx->current_overlay_receiver_expr != saved_receiver_expr) {
                        free((char *)ctx->current_overlay_receiver_expr);
                    }
                    ctx->current_overlay_receiver_expr = saved_receiver_expr;
                    transpiler_bind_current_host_decl_local(ctx, saved_host_decl);
                    action_sync = emit_world_embedded_action_effect_sync(
                        ctx, obj, method_decl);
                    post_sync = emit_world_embedded_receiver_projection_sync(ctx, obj);
                    codebuf_destroy(args_buf);
                    if (invalidation != NULL || action_sync != NULL || post_sync != NULL) {
                        char *ret_type_name = NULL;
                        char *wrapped = NULL;
                        const char *prefix = invalidation != NULL ? invalidation : "";
                        const char *effect_suffix = action_sync != NULL ? action_sync : "";
                        const char *suffix = post_sync != NULL ? post_sync : "";

                        if (ast_func_return_type(method_decl) != NULL)
                            ret_type_name = render_type_name(
                                ast_func_return_type(method_decl));

                        if (ret_type_name != NULL
                            && strcmp(ret_type_name, "Void") != 0) {
                            char ret_c_type_buf[256];
                            const char *ret_c_type = NULL;
                            int tmp_id = ++ctx->tmp_counter;
                            if (pergyra_type_to_c_copy(ret_type_name,
                                    ret_c_type_buf, sizeof(ret_c_type_buf))) {
                                ret_c_type = ret_c_type_buf;
                            }
                            if (ret_c_type == NULL) {
                                transpiler_set_backend_error_with_hints(ctx,
                                    PGY_CODE_C_TYPE_UNSUPPORTED,
                                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                                    "C method call post-sync wrapper cannot render return type '%s'",
                                    ret_type_name);
                                wrapped = pergyra_strdup("0");
                            } else {
                            wrapped = strdup_fmt(
                                "({ %s _pgy_call_%d = %s; %s%s%s_pgy_call_%d; })",
                                ret_c_type, tmp_id,
                                result, prefix, effect_suffix, suffix, tmp_id);
                            }
                        } else {
                            wrapped = strdup_fmt("({ %s; %s%s%s})",
                                result, prefix, effect_suffix, suffix);
                        }

                        free(ret_type_name);
                        free(invalidation);
                        free(action_sync);
                        free(post_sync);
                        free(result);
                        return wrapped;
                    }
                    free(action_sync);
                    free(post_sync);
                    return result;
                }
            }
        }

        if (is_slot_method && obj->type == AST_IDENTIFIER) {
            char inner_buf[128];
            const char *inner = NULL;
            const char *obj_name = ast_identifier_name(obj);
            bool is_secure = lookup_slot_is_secure(ctx, obj_name);
            bool saved_suppress = ctx->suppress_slot_auto_read;
            ctx->suppress_slot_auto_read = true;
            char *obj_expr = emit_expression(obj, ctx);
            ctx->suppress_slot_auto_read = saved_suppress;
            if (lookup_slot_type_copy(ctx, obj_name,
                    inner_buf, sizeof(inner_buf))) {
                inner = inner_buf;
            }
            if (inner == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slot payload type for method call on '%s'",
                    obj_name != NULL
                        ? obj_name
                        : "<slot>");
                free(obj_expr);
                return pergyra_strdup("0");
            }
            char *slot_ref = slot_ref_expr(ctx, obj_name, obj_expr);

            if (pgy_codegen_call_name_is_write(method)
                && ast_call_arg_count(call) >= 1) {
                char *val_expr = emit_expression(ast_call_argument(call, 0), ctx);
                char *result;
                if (is_secure && ast_call_arg_count(call) >= 2) {
                    char *tok = emit_expression(ast_call_argument(call, 1), ctx);
                    result = strdup_fmt("pgy_secure_write_%s(%s, %s, &%s)",
                                        inner, slot_ref, val_expr, tok);
                    free(tok);
                } else if (is_secure) {
                    const char *token_name = require_slot_token_name(
                        ctx, obj_name, "SecureSlot method Write");
                    if (token_name == NULL) {
                        free(val_expr);
                        free(slot_ref);
                        free(obj_expr);
                        return pergyra_strdup("0");
                    }
                    result = strdup_fmt("pgy_secure_write_%s(%s, %s, &%s)",
                                        inner, slot_ref, val_expr, token_name);
                } else {
                    result = strdup_fmt("pgy_write_%s(%s, %s)",
                                        inner, slot_ref, val_expr);
                }
                free(val_expr);
                free(slot_ref);
                free(obj_expr);
                return result;
            } else if (pgy_codegen_call_name_is_read(method)) {
                char *result;
                if (is_secure) {
                    const char *token_name = require_slot_token_name(
                        ctx, obj_name, "SecureSlot method Read");
                    if (token_name == NULL) {
                        free(slot_ref);
                        free(obj_expr);
                        return pergyra_strdup("0");
                    }
                    result = strdup_fmt("pgy_secure_read_%s(%s, &%s)",
                                        inner, slot_ref, token_name);
                } else {
                    result = strdup_fmt("pgy_read_%s(%s)", inner, slot_ref);
                }
                free(slot_ref);
                free(obj_expr);
                return result;
            } else if (pgy_codegen_call_name_is_release(method)) {
                char *result;
                if (is_secure) {
                    const char *token_name = require_slot_token_name(
                        ctx, obj_name, "SecureSlot method Release");
                    if (token_name == NULL) {
                        free(slot_ref);
                        free(obj_expr);
                        return pergyra_strdup("0");
                    }
                    result = strdup_fmt("pgy_secure_release_%s(%s, &%s)",
                                        inner, slot_ref, token_name);
                } else {
                    result = strdup_fmt("pgy_release_%s(%s)", inner, slot_ref);
                }
                /* Mark as released */
                for (int ri = 0; ri < ctx->slot_var_count; ri++) {
                    if (strcmp(ctx->slot_vars[ri].name,
                              obj_name) == 0) {
                        ctx->slot_vars[ri].released = true;
                        break;
                    }
                }
                free(slot_ref);
                free(obj_expr);
                return result;
            }
            free(slot_ref);
            free(obj_expr);
        }

        {
            const char *receiver_type = infer_expression_type_name(ctx, obj);
            if (method != NULL
                && strcmp(method, "Slice") == 0
                && receiver_type != NULL
                && (strncmp(receiver_type, "Array<", 6) == 0
                    || strncmp(receiver_type, "Slice<", 6) == 0)
                && ast_call_arg_count(call) == 2) {
                char inner_buf[128];
                const char *inner = NULL;
                char *start_expr = emit_expression(ast_call_argument(call, 0), ctx);
                char *len_expr = emit_expression(ast_call_argument(call, 1), ctx);
                char *result = NULL;
                int tmp_id = ++ctx->tmp_counter;
                if (slot_inner_type_name_copy(receiver_type, inner_buf,
                        sizeof(inner_buf)))
                    inner = inner_buf;

                if (inner == NULL) {
                    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slice element type for receiver '%s'",
                        receiver_type);
                    free(start_expr);
                    free(len_expr);
                    return pergyra_strdup("0");
                }

                if (strncmp(receiver_type, "Array<", 6) == 0) {
                    char *obj_expr = emit_expression(obj, ctx);
                    result = strdup_fmt(
                        "({ PgyArray_%s _pgy_arr_%d = %s; pgy_array_slice_%s(&_pgy_arr_%d, (size_t)(%s), (size_t)(%s)); })",
                        inner, tmp_id, obj_expr, inner, tmp_id, start_expr, len_expr);
                    free(obj_expr);
                } else {
                    char *obj_expr = emit_expression(obj, ctx);
                    result = strdup_fmt(
                        "({ PgySlice_%s _pgy_slice_%d = %s; size_t _pgy_start_%d = (size_t)(%s); size_t _pgy_len_%d = (size_t)(%s); if (_pgy_start_%d > _pgy_slice_%d.length || _pgy_len_%d > _pgy_slice_%d.length - _pgy_start_%d) PGY_PANIC(\"Slice out of bounds\"); (PgySlice_%s){ _pgy_len_%d == 0 ? NULL : _pgy_slice_%d.data + _pgy_start_%d, _pgy_len_%d }; })",
                        inner, tmp_id, obj_expr,
                        tmp_id, start_expr,
                        tmp_id, len_expr,
                        tmp_id, tmp_id, tmp_id, tmp_id, tmp_id,
                        inner, tmp_id, tmp_id, tmp_id, tmp_id);
                    free(obj_expr);
                }

                free(start_expr);
                free(len_expr);
                return result;
            }
        }
    }

    return NULL;
}

#include "transpiler_expr_call_user_emit.h"

char *
emit_call(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode    *callee = ast_call_callee(call);
    BuiltinKind bk     = BUILTIN_NOT_BUILTIN;

    if (callee->type == AST_IDENTIFIER) {
        const char *callee_name = ast_identifier_name(callee);
        bk = builtin_resolve(callee_name);
        if ((bk == BUILTIN_BOX || bk == BUILTIN_RC_NEW)
            && find_class_decl(ctx, callee_name) != NULL) {
            bk = BUILTIN_NOT_BUILTIN;
        }
    }

    bool handled = false;
    char *result = emit_call_builtin_dispatch(call, bk, ctx, &handled);
    if (handled)
        return result;

    result = emit_call_domain_constructor(call, callee, ctx);
    if (result != NULL)
        return result;
    result = emit_call_result_option_builtin(call, callee, ctx);
    if (result != NULL)
        return result;
    result = emit_call_stdlib_builtin(call, callee, ctx);
    if (result != NULL)
        return result;
    result = emit_call_event_builtin(call, callee, ctx);
    if (result != NULL)
        return result;
    result = emit_call_member_style(call, callee, ctx);
    if (result != NULL)
        return result;
    return emit_call_user_function(call, callee, ctx);
}

#endif /* PGY_TRANSPILER_EXPR_CALL_SPAWN_EMIT_H */
