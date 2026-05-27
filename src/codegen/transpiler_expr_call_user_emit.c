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
#include "transpiler_generic_param_query.h"
#include "transpiler_generic_specialization_emit.h"
#include "transpiler_slot_target.h"
#include "transpiler_symbols.h"
#include "transpiler_type_render.h"

char *
emit_call_user_function(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    const char *callee_name = ast_identifier_name(callee);
    if (callee->type == AST_IDENTIFIER) {
        ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
        ASTNode *host_method = current_host_method_decl(ctx, callee_name);
        const char *host_name = transpiler_decl_name_local(host_decl);
        if (host_method != NULL && host_name != NULL) {
            CodeBuf *args_buf = codebuf_create();
            codebuf_write(args_buf, "self");
            for (size_t i = 0; i < ast_call_arg_count(call); i++) {
                char *arg = emit_expression(ast_call_argument(call, i), ctx);
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
    if (callee->type == AST_IDENTIFIER) {
        ASTNode *decl = find_function_decl(ctx, callee_name);
        if (transpiler_func_has_generic_params(decl)) {
            const char *specialized_name = ensure_generic_specialization(ctx, decl, call);
            if (specialized_name != NULL)
                callee_str = pergyra_strdup(specialized_name);
        }
    }
    if (callee_str == NULL)
        callee_str = emit_expression(callee, ctx);

    CodeBuf *args_buf = codebuf_create();
    ASTNode *decl = (callee->type == AST_IDENTIFIER)
        ? find_callable_decl(ctx, callee_name) : NULL;
    for (size_t i = 0; i < ast_call_arg_count(call); i++) {
        FuncParam *param = (decl != NULL && decl->type == AST_FUNC_DECL
                            && i < ast_func_param_count(decl))
            ? ast_func_param(decl, i) : NULL;
        ASTNode *intent_param_type = NULL;
        bool handled = false;
        char *arg = NULL;

        if (decl != NULL && decl->type == AST_INTENT_DECL) {
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
                if (binding != NULL && binding->type == AST_INTENT_INVOLVES)
                    intent_param_type = ast_intent_involves_subject_type(binding);
                else if (binding != NULL && binding->type == AST_INTENT_VALUE)
                    intent_param_type = ast_intent_value_type(binding);
            }
        }

        if (param != NULL && param->type != NULL
            && (param->mode == PARAM_MODE_OWN || param->mode == PARAM_MODE_REF)) {
            char *param_type = render_type_name_in_ctx(ctx, param->type);
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
                ASTNode *call_arg = ast_call_argument(call, i);
                arg = emit_expression(call_arg, ctx);
                ctx->suppress_slot_auto_read = saved_suppress;
                (void)transpiler_resolve_slot_target_copy(ctx,
                    call_arg, inner_buf,
                    sizeof(inner_buf), &slot_name, &secure);
                if (i > 0)
                    codebuf_write(args_buf, ", ");
                if (slot_name != NULL) {
                    char *slot_ref = slot_ref_expr(ctx, slot_name, arg);
                    codebuf_write(args_buf, "%s", slot_ref);
                    free(slot_ref);
                    if (secure_param)
                        codebuf_write(args_buf, ", %s_token", slot_name);
                    handled = true;
                }
            }
            free(param_type);
        }

        if (!handled)
            arg = emit_expression(ast_call_argument(call, i), ctx);
        if (!handled && i > 0)
            codebuf_write(args_buf, ", ");
        if (!handled) {
            if (transpiler_call_arg_needs_subject_address(ctx,
                    param, intent_param_type)) {
                ASTNode *arg_node = ast_call_argument(call, i);
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
                    codebuf_destroy(args_buf);
                    return pergyra_strdup("0");
                }
            } else {
                codebuf_write(args_buf, "%s", arg);
            }
        }
        free(arg);
    }

    char *result = strdup_fmt("%s(%s)", callee_str, args_buf->data);
    free(callee_str);
    codebuf_destroy(args_buf);
    return result;
}
