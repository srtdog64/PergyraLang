#include "transpiler_expr_call_member_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "codegen_slot_type_policy.h"
#include "transpiler_call_subject_arg_policy.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_receiver_query.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_generic_class_specialization.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_inventory_view.h"
#include "transpiler_nominal.h"
#include "transpiler_overlay_host_fields.h"
#include "transpiler_overlay_projection.h"
#include "transpiler_projection.h"
#include "transpiler_projection_field_path.h"
#include "transpiler_projection_method_invalidation.h"
#include "transpiler_projection_sync.h"
#include "transpiler_type_mapping.h"
#include "transpiler_role_ability_helpers.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_require.h"
#include "transpiler_type_render.h"

static char *
transpiler_member_call_emit_part(TranspilerCtx *ctx,
                                 ASTNode *expr,
                                 const char *method_name,
                                 const char *role)
{
    char *rendered = emit_expression(expr, ctx);

    if (rendered != NULL)
        return rendered;

    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: member call %s could not lower %s expression",
        method_name != NULL ? method_name : "(anonymous-method)",
        role != NULL ? role : "operand");
    return NULL;
}

char *
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
                    char *party_expr = emit_expression(party_node, ctx);
                    CodeBuf *args_buf;
                    char *result;
                    if (party_expr == NULL) {
                        free(ability_name);
                        return NULL;
                    }
                    args_buf = codebuf_create();
                    codebuf_write(args_buf, "%s.%s", party_expr, slot_name);
                    for (size_t i = 0; i < ast_call_arg_count(call); i++) {
                        char *arg = transpiler_member_call_emit_part(ctx,
                            ast_call_argument(call, i), method,
                            "party ability argument");
                        if (arg == NULL) {
                            codebuf_destroy(args_buf);
                            free(ability_name);
                            free(party_expr);
                            return NULL;
                        }
                        codebuf_write(args_buf, ", %s", arg);
                        free(arg);
                    }

                    result = strdup_fmt("%s.%s_%s_vt->%s(%s)",
                                        party_expr, slot_name, ability_name,
                                        method, args_buf->data);
                    codebuf_destroy(args_buf);
                    free(ability_name);
                    free(party_expr);
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
                const MIRDeclMethod *method_meta =
                    transpiler_find_host_method_metadata_in_context(
                        ctx, owned_type_name, method);
                if (method_meta == NULL) {
                    ASTNode *spec_base =
                        transpiler_generic_class_spec_base_decl(ctx,
                            owned_type_name);
                    const char *spec_base_name = spec_base != NULL
                        ? transpiler_decl_name_local(spec_base)
                        : NULL;
                    if (spec_base_name != NULL)
                        method_meta =
                            transpiler_find_host_method_metadata_in_context(
                                ctx, spec_base_name, method);
                }
                ASTNode *method_decl = NULL;
                if (method_meta == NULL) {
                    if (transpiler_active_has_mir(ctx)) {
                        transpiler_set_mir_inventory_missing(ctx,
                            "MIR-only C path missing member-call method metadata for '%s.%s'",
                            owned_type_name != NULL ? owned_type_name : "(anonymous)",
                            method != NULL ? method : "(anonymous)");
                        codebuf_destroy(args_buf);
                        return NULL;
                    }
                    method_decl = find_nominal_host_method_decl(
                        ctx, owned_type_name, method);
                }

                if (!pergyra_str_copy(stable_type_name,
                        sizeof(stable_type_name), owned_type_name)) {
                    transpiler_set_backend_error_with_hints(
                        ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                        "C backend: nominal receiver type name is too long for method '%s'",
                        method != NULL ? method : "(anonymous)");
                    codebuf_destroy(args_buf);
                    return NULL;
                }
                owned_type_name = stable_type_name;
                use_self_cell = is_pointer_self_host_type_name(ctx, owned_type_name);

                if (!transpiler_mir_decl_method_metadata_complete_for(ctx,
                        method_meta,
                        owned_type_name,
                        method,
                        TRANSPILER_MIR_DECL_METHOD_REQUIRE_PARAM_TYPE_NAMES,
                        NULL,
                        "MIR-only C path missing member-call parameter type-name metadata for '%s.%s'")) {
                    codebuf_destroy(args_buf);
                    return NULL;
                }

                if (is_self_ident && use_self_cell) {
                    codebuf_write(args_buf, "self");
                } else {
                    char *obj_expr = transpiler_member_call_emit_part(ctx,
                        obj, method, "receiver");
                    bool already_pointer = false;
                    if (obj_expr == NULL) {
                        codebuf_destroy(args_buf);
                        return NULL;
                    }
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
                    char *arg = transpiler_member_call_emit_part(ctx,
                        arg_node, method, "argument");
                    bool pass_by_ptr = false;
                    if (arg == NULL) {
                        codebuf_destroy(args_buf);
                        return NULL;
                    }
                    if (method_meta != NULL) {
                        size_t param_index = i;
                        size_t param_count =
                            transpiler_mir_decl_method_param_count(
                                method_meta);
                        if (param_count > 0) {
                            FuncParam *first =
                                transpiler_mir_decl_method_param(
                                    method_meta, 0);
                            if (first != NULL && first->name != NULL
                                && strcmp(first->name, "self") == 0)
                                param_index++;
                        }
                        if (param_index < param_count) {
                            FuncParam *param =
                                transpiler_mir_decl_method_param(
                                    method_meta, param_index);
                            const char *ptn =
                                transpiler_mir_decl_method_param_type_name(
                                    method_meta, param_index);
                            char *owned_ptn = (ptn == NULL
                                    && param != NULL && param->type != NULL)
                                ? render_type_name_in_ctx(ctx, param->type)
                                : NULL;
                            if (ptn == NULL)
                                ptn = owned_ptn;
                            if (ptn != NULL && is_pointer_self_host_type_name(ctx, ptn))
                                pass_by_ptr = true;
                            free(owned_ptn);
                        }
                    } else if (method_meta == NULL && method_decl != NULL
                        && !transpiler_active_has_mir(ctx)) {
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
                                ? render_type_name_in_ctx(ctx, param->type)
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
                        else if (transpiler_call_arg_can_take_subject_address(
                                     arg_node)) {
                            codebuf_write(args_buf, ", &%s", arg);
                        } else {
                            transpiler_set_backend_error_with_hints(ctx,
                                PGY_CODE_C_TYPE_UNSUPPORTED,
                                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                                PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
                                "C backend: subject argument %zu for method '%s' requires addressable storage",
                                i + 1, method != NULL ? method : "<method>");
                            free(arg);
                            codebuf_destroy(args_buf);
                            return NULL;
                        }
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
                            ASTNode *zone_decl =
                                transpiler_find_named_decl_local(
                                    ctx, AST_ZONE_DECL, zone_type_name);
                            if (zone_decl != NULL)
                                transpiler_bind_current_host_decl_local(ctx, zone_decl);
                            ctx->current_overlay_receiver_expr =
                                strdup_fmt("(&self->%s)", zone_slot_name);
                            source_slot_name = zone_subject_slot_name;
                        }
                    }

                    if (method_decl == NULL && source_slot_name != NULL
                        && !transpiler_active_has_mir(ctx)) {
                        method_decl = find_nominal_host_method_decl(
                            ctx, owned_type_name, method);
                    }
                    invalidation =
                        emit_current_overlay_method_projection_invalidation(
                            ctx, source_slot_name, owned_type_name,
                            method_meta, method_decl);
                    if (ctx->current_overlay_receiver_expr != NULL
                        && ctx->current_overlay_receiver_expr != saved_receiver_expr) {
                        free((char *)ctx->current_overlay_receiver_expr);
                    }
                    ctx->current_overlay_receiver_expr = saved_receiver_expr;
                    transpiler_bind_current_host_decl_local(ctx, saved_host_decl);
                    action_sync = emit_world_embedded_action_effect_sync(
                        ctx, obj, method_meta);
                    post_sync = emit_world_embedded_receiver_projection_sync(ctx, obj);
                    codebuf_destroy(args_buf);
                    if (invalidation != NULL || action_sync != NULL || post_sync != NULL) {
                        char *ret_type_name = NULL;
                        char *wrapped = NULL;
                        const char *prefix = invalidation != NULL ? invalidation : "";
                        const char *effect_suffix = action_sync != NULL ? action_sync : "";
                        const char *suffix = post_sync != NULL ? post_sync : "";

                        {
                            const char *ret_type_name_fact =
                                transpiler_mir_decl_method_return_type_name(
                                    method_meta);
                            ASTNode *ret_type =
                                transpiler_mir_decl_method_return_type(
                                    method_meta);
                            if (!transpiler_mir_decl_method_metadata_complete_for(
                                    ctx,
                                    method_meta,
                                    owned_type_name,
                                    method,
                                    TRANSPILER_MIR_DECL_METHOD_REQUIRE_RETURN_TYPE_NAME,
                                    "MIR-only C path missing member-call return type-name metadata for '%s.%s'",
                                    NULL)) {
                                free(invalidation);
                                free(action_sync);
                                free(post_sync);
                                free(result);
                                return NULL;
                            }
                            if (ret_type_name_fact != NULL) {
                                ret_type_name =
                                    pergyra_strdup(ret_type_name_fact);
                            } else if (ret_type == NULL && method_meta == NULL
                                && method_decl != NULL
                                && !transpiler_active_has_mir(ctx))
                                ret_type = ast_func_return_type(method_decl);
                            if (ret_type_name == NULL && ret_type != NULL)
                                ret_type_name =
                                    render_type_name_in_ctx(ctx, ret_type);
                        }

                        if (ret_type_name != NULL
                            && strcmp(ret_type_name, "Void") != 0) {
                            char ret_c_type_buf[256];
                            const char *ret_c_type = NULL;
                            int tmp_id = ++ctx->tmp_counter;
                            if (transpiler_require_type_name_c_type_copy(
                                    ctx, ret_type_name,
                                    "method call post-sync return",
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
                                wrapped = NULL;
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
            char *obj_expr = transpiler_member_call_emit_part(ctx,
                obj, method, "slot receiver");
            ctx->suppress_slot_auto_read = saved_suppress;
            if (obj_expr == NULL)
                return NULL;
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
                return NULL;
            }
            char *slot_ref = slot_ref_expr(ctx, obj_name, obj_expr);
            if (slot_ref == NULL) {
                free(obj_expr);
                return NULL;
            }

            if (pgy_codegen_call_name_is_write(method)
                && ast_call_arg_count(call) >= 1) {
                char *val_expr = transpiler_member_call_emit_part(ctx,
                    ast_call_argument(call, 0), method, "write value");
                char *result;
                if (val_expr == NULL) {
                    free(slot_ref);
                    free(obj_expr);
                    return NULL;
                }
                if (is_secure && ast_call_arg_count(call) >= 2) {
                    char *tok = transpiler_member_call_emit_part(ctx,
                        ast_call_argument(call, 1), method, "write token");
                    if (tok == NULL) {
                        free(val_expr);
                        free(slot_ref);
                        free(obj_expr);
                        return NULL;
                    }
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
                        return NULL;
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
                        return NULL;
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
                        return NULL;
                    }
                    result = strdup_fmt("pgy_secure_release_%s(%s, &%s)",
                                        inner, slot_ref, token_name);
                } else {
                    result = strdup_fmt("pgy_release_%s(%s)", inner, slot_ref);
                }
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
            const char *receiver_type =
                transpiler_expr_infer_type_name(ctx, obj);
            if (method != NULL
                && strcmp(method, "Slice") == 0
                && receiver_type != NULL
                && transpiler_type_name_is_array_or_slice(receiver_type)
                && ast_call_arg_count(call) == 2) {
                char inner_buf[128];
                const char *inner = NULL;
                char *start_expr = transpiler_member_call_emit_part(ctx,
                    ast_call_argument(call, 0), method, "slice start");
                char *len_expr = NULL;
                char *result = NULL;
                int tmp_id = ++ctx->tmp_counter;
                if (start_expr == NULL)
                    return NULL;
                len_expr = transpiler_member_call_emit_part(ctx,
                    ast_call_argument(call, 1), method, "slice length");
                if (len_expr == NULL) {
                    free(start_expr);
                    return NULL;
                }
                if (slot_inner_type_name_copy(receiver_type, inner_buf,
                        sizeof(inner_buf)))
                    inner = inner_buf;

                if (inner == NULL) {
                    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slice element type for receiver '%s'",
                        receiver_type);
                    free(start_expr);
                    free(len_expr);
                    return NULL;
                }

                if (transpiler_type_name_is_array(receiver_type)) {
                    char *obj_expr = transpiler_member_call_emit_part(ctx,
                        obj, method, "slice receiver");
                    if (obj_expr == NULL) {
                        free(start_expr);
                        free(len_expr);
                        return NULL;
                    }
                    result = strdup_fmt(
                        "({ PgyArray_%s _pgy_arr_%d = %s; pgy_array_slice_%s(&_pgy_arr_%d, (size_t)(%s), (size_t)(%s)); })",
                        inner, tmp_id, obj_expr, inner, tmp_id, start_expr, len_expr);
                    free(obj_expr);
                } else {
                    char *obj_expr = transpiler_member_call_emit_part(ctx,
                        obj, method, "slice receiver");
                    if (obj_expr == NULL) {
                        free(start_expr);
                        free(len_expr);
                        return NULL;
                    }
                    result = strdup_fmt(
                        "({ PgySlice_%s _pgy_slice_%d = %s; size_t _pgy_start_%d = (size_t)(%s); size_t _pgy_len_%d = (size_t)(%s); if (_pgy_start_%d > _pgy_slice_%d.length || _pgy_len_%d > _pgy_slice_%d.length - _pgy_start_%d || (_pgy_len_%d > 0 && _pgy_slice_%d.data == NULL)) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, PGY_RUNTIME_PANIC_REASON_SLICE_OUT_OF_BOUNDS); (PgySlice_%s){ _pgy_len_%d == 0 ? NULL : _pgy_slice_%d.data + _pgy_start_%d, _pgy_len_%d }; })",
                        inner, tmp_id, obj_expr,
                        tmp_id, start_expr,
                        tmp_id, len_expr,
                        tmp_id, tmp_id, tmp_id, tmp_id, tmp_id, tmp_id, tmp_id,
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
