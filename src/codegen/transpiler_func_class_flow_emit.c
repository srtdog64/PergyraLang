#include "transpiler_func_class_flow_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

#include "transpiler_context.h"
#include "transpiler_defer_emit.h"
#include "transpiler_func_flow_policy.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_mir_emission_contract.h"
#include "transpiler_mir_emit_state.h"
#include "transpiler_mir_func_emit.h"
#include "transpiler_mir_reason_classifier.h"
#include "transpiler_symbols.h"
#include "transpiler_type_declarator.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

void
emit_func_decl_named(ASTNode *node, const char *emitted_name,
                     CodeBuf *buf, TranspilerCtx *ctx)
{
    const MIRRoutine *mir_routine = NULL;
    bool opened_body = false;
    if (transpiler_active_has_mir(ctx)) {
        char reason[256];
        if (transpiler_can_emit_function_from_mir_with_reason(
                ctx, node, &mir_routine, reason, sizeof(reason))) {
            emit_func_decl_from_mir_named(node, mir_routine, emitted_name, buf, ctx);
        } else if (ctx->backend_error == NULL) {
            const char *decl_name = ast_declaration_name(node);
            const char *func_name = (node != NULL && node->type == AST_FUNC_DECL
                                     && decl_name != NULL)
                ? decl_name
                : "(anonymous)";
            TranspilerMirReasonDiagnostic diag =
                transpiler_classify_mir_function_reason(reason);
            transpiler_set_backend_error_with_hints(ctx, diag.code,
                diag.cause, diag.fix,
                "MIR-only C path missing routine for function '%s': %s",
                func_name,
                reason[0] != '\0' ? reason : "unknown reason");
        }
        return;
    }
    if (transpiler_can_emit_function_from_mir_with_reason(
            ctx, node, &mir_routine, NULL, 0)) {
        emit_func_decl_from_mir_named(node, mir_routine, emitted_name, buf, ctx);
        return;
    }
    const char *name = emitted_name != NULL ? emitted_name : ast_declaration_name(node);
    TranspilerMirEmitState saved_emit_state;
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;
    transpiler_capture_mir_emit_state_local(ctx, &saved_emit_state);
    ctx->out = buf;
    if (ast_func_return_type(node) != NULL) {
        ASTNode *return_type = ast_func_return_type(node);
        {
            char *rendered = render_type_name_in_ctx(
                ctx, return_type);
            if (rendered == NULL
                || !transpiler_func_copy_current_return_type(ctx, rendered)) {
                free(rendered);
                transpiler_func_format_too_long(ctx, "function return type");
                goto emit_func_decl_named_fail;
            }
            free(rendered);
        }
        ctx->current_return_callable_type =
            return_type->type == AST_EVENT_HANDLER_TYPE ? return_type : NULL;
    } else {
        if (!transpiler_func_copy_current_return_type(ctx, "Void")) {
            transpiler_func_format_too_long(ctx, "function return type");
            goto emit_func_decl_named_fail;
        }
        ctx->current_return_callable_type = NULL;
    }

    for (size_t i = 0; i < ast_func_param_count(node); i++) {
        FuncParam *p = ast_func_param(node, i);
        char pt[256];
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        char surface_desc[256];
        bool event_handler_param = false;
        if (!transpiler_func_parameter_surface_desc(surface_desc,
                sizeof(surface_desc),
                p != NULL ? p->name : NULL, name)) {
            transpiler_func_format_too_long(
                ctx, "function parameter diagnostic surface");
            goto emit_func_decl_named_fail;
        }
        if (p == NULL || p->name == NULL)
            goto emit_func_decl_named_fail;
        event_handler_param =
            p->type != NULL && p->type->type == AST_EVENT_HANDLER_TYPE;
        if (!event_handler_param
            && !transpiler_require_ast_c_type_copy(ctx,
                p != NULL ? p->type : NULL,
                surface_desc,
                pt,
                sizeof(pt))) {
            goto emit_func_decl_named_fail;
        }
        if (i > 0) codebuf_write(params_sig, ", ");
        if (p->type != NULL)
            type_name = render_type_name_in_ctx(ctx, p->type);
        boundary_slot = type_name != NULL
            && (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0)
            && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
        secure_slot = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;
        if (boundary_slot) {
            char inner_buf[128];
            const char *inner = inner_buf;
            if (!slot_inner_type_name_copy(type_name, inner_buf,
                    sizeof(inner_buf))) {
                free(decl);
                free(type_name);
                transpiler_func_format_too_long(ctx,
                    "function parameter slot payload type");
                goto emit_func_decl_named_fail;
            }
            codebuf_write(params_sig, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(params_sig, ", PgyToken_%s %s_token", inner, p->name);
        } else if (event_handler_param) {
            decl = pergyra_ast_typed_declarator_in_ctx(ctx, p->type, p->name);
            if (decl == NULL)
                goto emit_func_decl_named_fail;
            codebuf_write(params_sig, "%s", decl);
        } else if (p->name != NULL && strcmp(p->name, "self") != 0
                   && type_name != NULL
                   && is_pointer_self_host_type_name(ctx, type_name)) {
            /* Subject parameters are passed by pointer (reference semantics) */
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else {
            codebuf_write(params_sig, "%s %s", pt, p->name);
        }
        free(decl);
        free(type_name);
    }
    header_decl = pergyra_func_signature_declarator_in_ctx(
        ctx, ast_func_return_type(node),
        name, params_sig != NULL ? params_sig->data : "void");
    if (header_decl == NULL)
        goto emit_func_decl_named_fail;
    codebuf_write(ctx->out, "\n%s\n{\n", header_decl);
    opened_body = true;
    free(header_decl);
    header_decl = NULL;
    codebuf_destroy(params_sig);
    params_sig = NULL;

    ctx->indent++;
    for (size_t i = 0; i < ast_func_param_count(node); i++) {
        FuncParam *p = ast_func_param(node, i);
        if (p == NULL || p->name == NULL || p->type == NULL)
            continue;
        char *type_name = render_type_name_in_ctx(ctx, p->type);
        if (type_name != NULL) {
            bool boundary_slot = (strncmp(type_name, "Slot<", 5) == 0
                               || strncmp(type_name, "SecureSlot<", 11) == 0)
                && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
            register_typed_var(ctx, p->name, type_name);
            /* Mark pointer-self parameters (subject/relation/effect/zone/world) for -> access */
            if (p->name != NULL && strcmp(p->name, "self") != 0
                && is_pointer_self_host_type_name(ctx, type_name)) {
                TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                if (entry != NULL)
                    entry->is_subject_ref = true;
            }
            if (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0) {
                char inner_buf[128];
                bool secure_slot = strncmp(type_name, "SecureSlot<", 11) == 0;
                if (!slot_inner_type_name_copy(type_name, inner_buf,
                        sizeof(inner_buf))) {
                    free(type_name);
                    transpiler_func_format_too_long(ctx,
                        "function parameter slot payload type");
                    goto emit_func_decl_named_fail;
                }
                register_slot_var(ctx, p->name, inner_buf, secure_slot,
                    boundary_slot);
            }
            free(type_name);
        }
    }
    if (ast_func_body(node) != NULL)
        emit_block(ast_func_body(node), ctx);
    if (ctx->backend_error != NULL)
        goto emit_func_decl_named_fail;

    /* Non-void fallthrough should be rejected by CFG/body-flow before backend
     * emission. Keep a hard runtime invariant here only as defense in depth;
     * never synthesize a value that could hide a missing-return bug. */
    if (strcmp(ctx->current_return_type, "Void") != 0
        && strcmp(ctx->current_return_type, "void") != 0) {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PGY_PANIC(\"non-void function reached end without return\");\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "__builtin_unreachable();\n");
    }

    ctx->indent--;

    codebuf_write(ctx->out, "}\n");
    transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
    return;

emit_func_decl_named_fail:
    if (opened_body)
        ctx->indent--;
    free(header_decl);
    if (params_sig != NULL)
        codebuf_destroy(params_sig);
    transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
}

void
emit_func_decl(ASTNode *node, TranspilerCtx *ctx)
{
    emit_func_decl_named(node, ast_declaration_name(node), ctx->out, ctx);
}

/* -----------------------------------------------------------------
 * Class declaration emitter
 * ----------------------------------------------------------------- */

void
emit_with_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    const char *alias = ast_with_alias(node);
    bool is_secure    = ast_with_is_secure(node);
    int saved_slot_count = ctx->slot_var_count;
    int saved_typed_count = ctx->typed_var_count;

    const char *inner = NULL;
    if (ast_with_slot_type(node) != NULL)
        inner = ast_type_name(ast_with_slot_type(node));
    if (inner == NULL) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot emit with-slot alias '%s': missing explicit slot type",
            alias != NULL ? alias : "(anonymous)");
        return;
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    /* Register alias in slot variable table */
    register_slot_var(ctx, alias, inner, is_secure, false);

    if (is_secure) {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyToken_%s %s_token;\n", inner, alias);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
            inner, alias, inner, alias);
        write_indent(ctx);
        codebuf_write(ctx->out, "(void)%s;\n", alias);
    } else {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgySlot_%s %s = pgy_claim_%s();\n",
            inner, alias, inner);
        write_indent(ctx);
        codebuf_write(ctx->out, "(void)%s;\n", alias);
    }

    if (ast_with_body(node) != NULL)
        emit_block(ast_with_body(node), ctx);

    /* Auto-release */
    write_indent(ctx);
    if (is_secure) {
        codebuf_write(ctx->out,
            "pgy_secure_release_%s(&%s, &%s_token);\n",
            inner, alias, alias);
    } else {
        codebuf_write(ctx->out,
            "pgy_release_%s(&%s);\n", inner, alias);
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    transpiler_restore_local_binding_counts_local(ctx, saved_slot_count,
                                                  saved_typed_count, -1);
}

/* -----------------------------------------------------------------
 * Parallel block
 * ----------------------------------------------------------------- */


/* -----------------------------------------------------------------
 * Control flow
 * ----------------------------------------------------------------- */

void
emit_return_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *value = ast_return_value(node);

    transpiler_emit_defers_from(ctx, 0);
    transpiler_emit_mut_ref_writebacks(ctx);
    write_indent(ctx);
    if (value != NULL) {
        if (ctx->current_return_type[0] != '\0'
            && transpiler_type_name_is_option(ctx->current_return_type)) {
            char inner_buf[128];
            const char *inner = inner_buf;
            if (!slot_inner_type_name_copy(ctx->current_return_type, inner_buf,
                    sizeof(inner_buf))) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "return Option<T> requires concrete payload type during C emission");
                return;
            }
            if (value->type == AST_CALL
                && ast_call_callee(value) != NULL
                && ast_call_callee(value)->type == AST_IDENTIFIER) {
                const char *callee_name =
                    ast_identifier_name(ast_call_callee(value));
                TranspilerReturnOptionCtorOp op =
                    transpiler_return_option_ctor_lookup(callee_name);
                if (op == TRANS_RETURN_OPTION_CTOR_SOME
                    && ast_call_arg_count(value) == 1) {
                    char *arg = emit_expression(ast_call_argument(value, 0), ctx);
                    codebuf_write(ctx->out, "return Some_%s(%s);\n", inner, arg);
                    free(arg);
                    return;
                }
                if (op == TRANS_RETURN_OPTION_CTOR_NONE_VALUE
                    && ast_call_arg_count(value) == 0) {
                    codebuf_write(ctx->out, "return None_%s();\n", inner);
                    return;
                }
            }
            if (value->type == AST_IDENTIFIER
                && transpiler_return_option_ctor_lookup(
                    ast_identifier_name(value))
                    == TRANS_RETURN_OPTION_CTOR_NONE_VALUE) {
                codebuf_write(ctx->out, "return None_%s();\n", inner);
                return;
            }
        }
        const char *saved_expected_type = ctx->expected_type;
        ASTNode *saved_expected_callable_type = ctx->expected_callable_type;
        char *val;
        if (ctx->current_return_type[0] != '\0'
            && strcmp(ctx->current_return_type, "Void") != 0
            && strcmp(ctx->current_return_type, "void") != 0) {
            ctx->expected_type = ctx->current_return_type;
            ctx->expected_callable_type =
                transpiler_func_current_return_callable_type(ctx);
        }
        val = emit_expression(value, ctx);
        ctx->expected_callable_type = saved_expected_callable_type;
        ctx->expected_type = saved_expected_type;
        codebuf_write(ctx->out, "return %s;\n", val);
        free(val);
    } else {
        codebuf_write(ctx->out, "return;\n");
    }
}
