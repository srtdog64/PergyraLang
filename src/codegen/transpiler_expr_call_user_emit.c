#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"
#include "transpiler.h"
#include "transpiler_call_subject_arg_policy.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_generic_specialization_emit.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_signature.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_slot_target.h"
#include "transpiler_symbols.h"
#include "transpiler_type_render.h"

static char *
transpiler_user_call_emit_part(TranspilerCtx *ctx,
                               ASTNode *expr,
                               const char *call_name,
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
        "C backend: user call %s could not lower %s expression",
        call_name != NULL ? call_name : "(anonymous-call)",
        role != NULL ? role : "operand");
    return NULL;
}

char *
emit_call_user_function(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    const char *callee_name = ast_identifier_name(callee);
    ASTNode *decl = (callee->type == AST_IDENTIFIER)
        ? find_callable_decl(ctx, callee_name) : NULL;
    if (callee->type == AST_IDENTIFIER) {
        ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
        const char *host_name = transpiler_decl_name_local(host_decl);
        const MIRDeclMethod *host_method_meta =
            transpiler_find_host_method_metadata_in_context(
                ctx, host_name, callee_name);
        if (host_method_meta == NULL
            && host_name != NULL
            && decl == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing hosted self-call method metadata for '%s.%s'",
                host_name,
                callee_name != NULL ? callee_name : "(anonymous)");
            return NULL;
        }
        if (host_method_meta != NULL && host_name != NULL) {
            CodeBuf *args_buf = codebuf_create();
            codebuf_write(args_buf, "self");
            for (size_t i = 0; i < ast_call_arg_count(call); i++) {
                ASTNode *arg_node = ast_call_argument(call, i);
                const char *arg_type =
                    transpiler_expr_infer_type_name(ctx, arg_node);
                char *arg;
                if (arg_type != NULL && strcmp(arg_type, "Void") == 0) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ALIGN_ARG_TYPE,
                        "C backend: hosted method '%s.%s' cannot consume a Void expression as argument %zu",
                        host_name,
                        callee_name != NULL ? callee_name : "<call>",
                        i + 1);
                    codebuf_destroy(args_buf);
                    return NULL;
                }
                arg = transpiler_user_call_emit_part(ctx, arg_node,
                    callee_name, "hosted method argument");
                if (arg == NULL) {
                    codebuf_destroy(args_buf);
                    return NULL;
                }
                codebuf_write(args_buf, ", %s", arg);
                free(arg);
            }
            {
                char *result = strdup_fmt("%s_%s(%s)",
                    host_name, callee_name, args_buf->data);
                codebuf_destroy(args_buf);
                return result;
            }
        }
    }

    char *callee_str = NULL;
    const MIRRoutine *intent_routine = NULL;
    IntentBindingMetadataView binding_metadata = {0};
    size_t binding_meta_count = 0;
    const MIRRoutine *callee_routine = NULL;
    bool callee_has_mir_signature = false;
    bool callee_is_generic_func = false;
    bool callee_is_extern_func = false;
    bool mir_requires_routine = false;
    bool mir_only_intent = false;
    if (callee->type == AST_IDENTIFIER) {
        if (decl != NULL && decl->type == AST_FUNC_DECL) {
            callee_routine = transpiler_find_mir_function(ctx, decl);
            callee_is_generic_func =
                transpiler_mir_or_ast_function_is_generic(callee_routine,
                    decl);
        }
        if (decl != NULL && decl->type == AST_FUNC_DECL
            && callee_is_generic_func) {
            callee_is_generic_func = true;
            const char *specialized_name =
                ensure_generic_specialization(ctx, decl, call);
            if (specialized_name != NULL)
                callee_str = pergyra_strdup(specialized_name);
        }
    }
    if (callee_str == NULL)
        callee_str = transpiler_user_call_emit_part(ctx, callee,
            callee_name, "callee");
    if (callee_str == NULL)
        return NULL;

    if (decl != NULL && decl->type == AST_FUNC_DECL)
        callee_is_extern_func = transpiler_decl_is_extern_function(ctx, decl);

    if (decl != NULL && decl->type == AST_FUNC_DECL
        && !callee_is_generic_func && !callee_is_extern_func
        && transpiler_active_has_mir(ctx)) {
        if (callee_routine == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing user-call routine for '%s'",
                callee_name != NULL ? callee_name : "(anonymous-call)");
            free(callee_str);
            return NULL;
        }
        if (!transpiler_mir_routine_signature_metadata_complete_for(ctx,
                callee_routine, decl,
                TRANSPILER_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES,
                "MIR-only C path missing user-call signature metadata for '%s'",
                NULL,
                "MIR-only C path missing user-call parameter type-name metadata for '%s'")) {
            free(callee_str);
            return NULL;
        }
        callee_has_mir_signature = true;
    }

    if (decl != NULL && decl->type == AST_INTENT_DECL) {
        size_t intent_step_count = ast_intent_decl_step_count(decl);
        mir_requires_routine = transpiler_active_has_mir(ctx) && intent_step_count > 0;
        intent_routine = transpiler_find_mir_intent(ctx, decl);
        if (mir_requires_routine && intent_routine == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing intent routine for call target '%s'",
                callee_name != NULL ? callee_name : "(anonymous-intent)");
            free(callee_str);
            return NULL;
        }
        mir_only_intent = intent_routine != NULL;
        if (intent_routine != NULL) {
            binding_meta_count = transpiler_collect_mir_intent_bindings(
                intent_routine, &binding_metadata);
        }
        if (mir_only_intent) {
            for (size_t i = 0; i < binding_meta_count; i++) {
                if (!intent_binding_metadata_view_has_complete_row(
                        &binding_metadata, i)) {
                    transpiler_set_mir_inventory_missing(ctx,
                        "MIR-only C path has incomplete ordered intent binding metadata for call target '%s'",
                        callee_name != NULL ? callee_name : "(anonymous-intent)");
                    free(callee_str);
                    intent_binding_metadata_view_dispose(&binding_metadata);
                    return NULL;
                }
                if (!intent_binding_metadata_kind_is_supported(
                        intent_binding_metadata_view_kind_at(
                            &binding_metadata, i))) {
                    transpiler_set_mir_inventory_missing(ctx,
                        "MIR-only C path has invalid ordered intent binding metadata for call target '%s'",
                        callee_name != NULL ? callee_name : "(anonymous-intent)");
                    free(callee_str);
                    intent_binding_metadata_view_dispose(&binding_metadata);
                    return NULL;
                }
            }
            if (ast_call_arg_count(call) != binding_meta_count) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ALIGN_ARG_TYPE,
                    "C backend: MIR-backed intent call '%s' expects %zu argument(s), got %zu",
                    callee_name != NULL ? callee_name : "(anonymous-intent)",
                    binding_meta_count,
                    ast_call_arg_count(call));
                free(callee_str);
                intent_binding_metadata_view_dispose(&binding_metadata);
                return NULL;
            }
        }
    }

    CodeBuf *args_buf = codebuf_create();
    for (size_t i = 0; i < ast_call_arg_count(call); i++) {
        ASTNode *arg_node = ast_call_argument(call, i);
        FuncParam *param = NULL;
        const char *param_type_name = NULL;
        ASTNode *intent_param_type = NULL;
        const char *intent_param_type_name = NULL;
        bool handled = false;
        char *arg = NULL;

        if (decl != NULL && decl->type == AST_FUNC_DECL) {
            if (callee_has_mir_signature) {
                if (i < transpiler_mir_routine_param_count(callee_routine)) {
                    param = transpiler_mir_routine_param(callee_routine, i);
                    param_type_name =
                        transpiler_mir_routine_param_type_name(
                            callee_routine, i);
                }
            } else if ((callee_is_generic_func
                        || callee_is_extern_func)
                       && i < ast_func_param_count(decl)) {
                param = ast_func_param(decl, i);
            }
        }

        if (decl != NULL && decl->type == AST_INTENT_DECL) {
            if (mir_only_intent) {
                if (i >= binding_meta_count) {
                    transpiler_set_mir_inventory_missing(ctx,
                        "MIR-only C path missing ordered intent binding metadata for call target '%s'",
                        callee_name != NULL ? callee_name : "(anonymous-intent)");
                } else if (intent_binding_metadata_view_row_is_kind(
                               &binding_metadata, i, "participant")) {
                    intent_param_type_name =
                        intent_binding_metadata_view_type_at(
                            &binding_metadata, i);
                } else if (intent_binding_metadata_view_row_is_kind(
                               &binding_metadata, i, "value")) {
                    intent_param_type_name =
                        intent_binding_metadata_view_type_at(
                            &binding_metadata, i);
                } else {
                    transpiler_set_mir_inventory_missing(ctx,
                        "MIR-only C path has invalid ordered intent binding metadata for call target '%s'",
                        callee_name != NULL ? callee_name : "(anonymous-intent)");
                }
            } else {
                size_t explicit_binding_count = ast_intent_decl_binding_count(decl);
                size_t involve_count = ast_intent_decl_involve_count(decl);
                size_t value_count = ast_intent_decl_value_count(decl);
                size_t binding_count = explicit_binding_count > 0
                    ? explicit_binding_count
                    : (involve_count + value_count);
                ASTNode **bindings = ast_intent_decl_bindings(decl, NULL);
                ASTNode **involves = ast_intent_decl_involves(decl, NULL);
                ASTNode **values = ast_intent_decl_values(decl, NULL);
                if (i < binding_count) {
                    ASTNode *binding = explicit_binding_count > 0
                        ? bindings[i]
                        : (i < involve_count
                            ? involves[i]
                            : values[i - involve_count]);
                    if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
                        intent_param_type = ast_intent_involves_subject_type(binding);
                    } else if (binding != NULL && binding->type == AST_INTENT_VALUE) {
                        intent_param_type = ast_intent_value_type(binding);
                    }
                }
            }
            if (ctx->backend_error != NULL) {
                free(callee_str);
                intent_binding_metadata_view_dispose(&binding_metadata);
                codebuf_destroy(args_buf);
                return NULL;
            }
        }

        if (param != NULL && (param_type_name != NULL || param->type != NULL)
            && (param->mode == PARAM_MODE_OWN || param->mode == PARAM_MODE_REF)) {
            char *param_type_owned = NULL;
            const char *param_type = param_type_name;
            if (param_type == NULL) {
                param_type_owned = render_type_name_in_ctx(ctx, param->type);
                param_type = param_type_owned;
            }
            bool slot_param = param_type != NULL
                && (strncmp(param_type, "Slot<", 5) == 0
                    || strncmp(param_type, "SecureSlot<", 11) == 0);
            bool secure_param = param_type != NULL
                && strncmp(param_type, "SecureSlot<", 11) == 0;
            if (slot_param) {
                char inner_buf[128];
                const char *slot_name = NULL;
                bool secure = false;
                bool saved_suppress = ctx->suppress_slot_auto_read;
                ctx->suppress_slot_auto_read = true;
                arg = transpiler_user_call_emit_part(ctx, arg_node,
                    callee_name, "slot argument");
                ctx->suppress_slot_auto_read = saved_suppress;
                if (arg == NULL) {
                    free(param_type_owned);
                    free(callee_str);
                    intent_binding_metadata_view_dispose(&binding_metadata);
                    codebuf_destroy(args_buf);
                    return NULL;
                }
                (void)transpiler_resolve_slot_target_copy(ctx,
                    arg_node, inner_buf,
                    sizeof(inner_buf), &slot_name, &secure);
                if (i > 0)
                    codebuf_write(args_buf, ", ");
                if (slot_name != NULL) {
                    char *slot_ref = slot_ref_expr(ctx, slot_name, arg);
                    if (slot_ref == NULL) {
                        free(arg);
                        free(param_type_owned);
                        free(callee_str);
                        intent_binding_metadata_view_dispose(&binding_metadata);
                        codebuf_destroy(args_buf);
                        return NULL;
                    }
                    codebuf_write(args_buf, "%s", slot_ref);
                    free(slot_ref);
                    if (secure_param)
                        codebuf_write(args_buf, ", %s_token", slot_name);
                    handled = true;
                } else {
                    free(arg);
                    arg = NULL;
                }
            }
            free(param_type_owned);
        }

        char *param_type_for_ctx = NULL;
        if (param_type_name != NULL)
            param_type_for_ctx = pergyra_strdup(param_type_name);
        else if (param != NULL && param->type != NULL)
            param_type_for_ctx = render_type_name_in_ctx(ctx, param->type);
        else if (intent_param_type_name != NULL)
            param_type_for_ctx = pergyra_strdup(intent_param_type_name);
        else if (intent_param_type != NULL)
            param_type_for_ctx = render_type_name_in_ctx(ctx, intent_param_type);

        if (!handled) {
            const char *arg_type =
                transpiler_expr_infer_type_name(ctx, arg_node);
            if (arg_type != NULL && strcmp(arg_type, "Void") == 0) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ALIGN_ARG_TYPE,
                    "C backend: call '%s' cannot consume a Void expression as argument %zu",
                    callee_name != NULL ? callee_name : "<call>",
                    i + 1);
                free(param_type_for_ctx);
                free(callee_str);
                intent_binding_metadata_view_dispose(&binding_metadata);
                codebuf_destroy(args_buf);
                return NULL;
            }
            const char *saved_expected_type = ctx->expected_type;
            if (param_type_for_ctx != NULL)
                ctx->expected_type = param_type_for_ctx;
            arg = transpiler_user_call_emit_part(ctx, arg_node,
                callee_name, "argument");
            ctx->expected_type = saved_expected_type;
            if (arg == NULL) {
                free(param_type_for_ctx);
                free(callee_str);
                intent_binding_metadata_view_dispose(&binding_metadata);
                codebuf_destroy(args_buf);
                return NULL;
            }
        }
        free(param_type_for_ctx);
        if (!handled && i > 0)
            codebuf_write(args_buf, ", ");
        if (!handled && param != NULL && param->mode == PARAM_MODE_MUT_REF) {
            codebuf_write(args_buf, "&%s", arg);
        } else if (!handled) {
            if (transpiler_call_arg_needs_subject_address(ctx,
                    param, param_type_name,
                    intent_param_type, intent_param_type_name)) {
                if (transpiler_call_arg_is_subject_ref(ctx, arg_node))
                    codebuf_write(args_buf, "%s", arg);
                else if (transpiler_call_arg_can_take_subject_address(arg_node))
                    codebuf_write(args_buf, "&%s", arg);
                else {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
                        "C backend: subject argument %zu for '%s' requires addressable storage",
                        i + 1, callee_name != NULL ? callee_name : "<call>");
                    free(arg);
                    free(callee_str);
                    intent_binding_metadata_view_dispose(&binding_metadata);
                    codebuf_destroy(args_buf);
                    return NULL;
                }
            } else {
                codebuf_write(args_buf, "%s", arg);
            }
        }
        free(arg);
    }

    char *result = strdup_fmt("%s(%s)", callee_str, args_buf->data);
    free(callee_str);
    intent_binding_metadata_view_dispose(&binding_metadata);
    codebuf_destroy(args_buf);
    return result;
}
