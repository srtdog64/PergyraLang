void
emit_func_decl_named(ASTNode *node, const char *emitted_name,
                     CodeBuf *buf, TranspilerCtx *ctx)
{
    const MIRRoutine *mir_routine = NULL;
    bool opened_body = false;
    if (ctx != NULL && ctx->mir != NULL) {
        char reason[256];
        if (transpiler_can_emit_function_from_mir_with_reason(
                ctx, node, &mir_routine, reason, sizeof(reason))) {
            emit_func_decl_from_mir_named(node, mir_routine, emitted_name, buf, ctx);
        } else if (ctx->backend_error == NULL) {
            const char *func_name = (node != NULL && node->type == AST_FUNC_DECL
                                     && node->data.func_decl.name != NULL)
                ? node->data.func_decl.name
                : "(anonymous)";
            /* Classify the failure by inspecting the reason prefix so
             * downstream consumers can route on a stable code. The reason
             * text itself is preserved as the human-readable message. */
            const char *reason_code  = NULL;
            const char *reason_cause = NULL;
            const char *reason_fix   = NULL;
            if (strstr(reason, "unresolved identifier") != NULL) {
                reason_code  = PGY_CODE_MIR_UNRESOLVED_LOCAL;
                reason_cause = PGY_CAUSE_MIR_LOCAL_UNRESOLVED;
                reason_fix   = PGY_FIX_REPORT_COMPILER_BUG;
            } else if (strstr(reason, "topology") != NULL
                       || strstr(reason, "no routine") != NULL
                       || strstr(reason, "no declaration AST") != NULL) {
                reason_code  = PGY_CODE_MIR_TOPOLOGY_INVALID;
                reason_cause = PGY_CAUSE_MIR_TOPOLOGY_INVALID;
                reason_fix   = PGY_FIX_REPORT_COMPILER_BUG;
            } else if (strstr(reason, "unsupported MIR signature") != NULL
                       || strstr(reason, "wrong MIR kind") != NULL) {
                reason_code  = PGY_CODE_MIR_SIGNATURE_UNSUPPORTED;
                reason_cause = PGY_CAUSE_MIR_SIGNATURE_UNSUPPORTED;
                reason_fix   = PGY_FIX_SIMPLIFY_FUNCTION_SIGNATURE;
            } else if (strstr(reason, "too many MIR SSA locals") != NULL) {
                reason_code  = PGY_CODE_MIR_SSA_LIMIT;
                reason_cause = PGY_CAUSE_MIR_SSA_CAPACITY_EXCEEDED;
                reason_fix   = PGY_FIX_REFACTOR_OR_RAISE_LIMIT;
            }
            transpiler_set_backend_error_with_hints(ctx, reason_code,
                reason_cause, reason_fix,
                "MIR-only C path missing routine for function '%s': %s",
                func_name,
                reason[0] != '\0' ? reason : "unknown reason");
        }
        return;
    }
    if (transpiler_can_emit_function_from_mir(ctx, node, &mir_routine)) {
        emit_func_decl_from_mir_named(node, mir_routine, emitted_name, buf, ctx);
        return;
    }
    const char *name = emitted_name != NULL ? emitted_name : node->data.func_decl.name;
    TranspilerMirEmitState saved_emit_state;
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;
    transpiler_capture_mir_emit_state_local(ctx, &saved_emit_state);
    ctx->out = buf;
    transpiler_type_render_ctx_bind(ctx);
    if (node->data.func_decl.return_type != NULL) {
        {
            char *rendered = render_type_name(node->data.func_decl.return_type);
            snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                "%s", rendered);
            free(rendered);
        }
    } else {
        snprintf(ctx->current_return_type, sizeof(ctx->current_return_type), "Void");
    }

    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        const char *pt = NULL;
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "function parameter '%s' of '%s'",
            p != NULL && p->name != NULL ? p->name : "(anonymous)",
            name != NULL ? name : "(anonymous)");
        pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
        if (pt == NULL)
            goto emit_func_decl_named_fail;
        if (p == NULL || p->name == NULL)
            goto emit_func_decl_named_fail;
        if (i > 0) codebuf_write(params_sig, ", ");
        if (p->type != NULL)
            type_name = render_type_name(p->type);
        boundary_slot = type_name != NULL
            && (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0)
            && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
        secure_slot = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;
        if (boundary_slot) {
            const char *inner = slot_inner_type_name(type_name);
            codebuf_write(params_sig, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(params_sig, ", PgyToken_%s %s_token", inner, p->name);
        } else if (p->type != NULL && p->type->type == AST_EVENT_HANDLER_TYPE) {
            decl = pergyra_ast_typed_declarator(p->type, p->name);
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
    header_decl = pergyra_func_signature_declarator(node->data.func_decl.return_type,
        name, params_sig != NULL ? params_sig->data : "void");
    codebuf_write(ctx->out, "\n%s\n{\n", header_decl);
    opened_body = true;
    free(header_decl);
    header_decl = NULL;
    codebuf_destroy(params_sig);
    params_sig = NULL;

    ctx->indent++;
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        if (p == NULL || p->name == NULL || p->type == NULL)
            continue;
        char *type_name = render_type_name(p->type);
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
            if (strncmp(type_name, "Slot<", 5) == 0)
                register_slot_var(ctx, p->name, slot_inner_type_name(type_name), false, boundary_slot);
            else if (strncmp(type_name, "SecureSlot<", 11) == 0)
                register_slot_var(ctx, p->name, slot_inner_type_name(type_name), true, boundary_slot);
            free(type_name);
        }
    }
    if (node->data.func_decl.body != NULL)
        emit_block(node->data.func_decl.body, ctx);
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
    emit_func_decl_named(node, node->data.func_decl.name, ctx->out, ctx);
}

/* -----------------------------------------------------------------
 * Class declaration emitter
 * ----------------------------------------------------------------- */

#include "transpiler_generic_class_specialization_emit.h"

#include "transpiler_class_decl_emit.h"

/* -----------------------------------------------------------------
 * with slot<T> as s { }
 * ----------------------------------------------------------------- */

void
emit_with_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    const char *alias = node->data.with_stmt.alias;
    bool is_secure    = node->data.with_stmt.is_secure;
    int saved_slot_count = ctx->slot_var_count;
    int saved_typed_count = ctx->typed_var_count;

    const char *inner = NULL;
    if (node->data.with_stmt.slot_type != NULL)
        inner = node->data.with_stmt.slot_type->data.type.name;
    if (inner == NULL) {
        if (ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "cannot emit with-slot alias '%s': missing explicit slot type",
                alias != NULL ? alias : "(anonymous)");
        }
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

    if (node->data.with_stmt.body != NULL)
        emit_block(node->data.with_stmt.body, ctx);

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


#include "transpiler_async_parallel_emit.h"

#include "transpiler_enum_decl_emit.h"

/* -----------------------------------------------------------------
 * Control flow
 * ----------------------------------------------------------------- */

#include "transpiler_control_flow_emit.h"

#include "transpiler_match_emit.h"

void
emit_return_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    transpiler_emit_defers_from(ctx, 0);
    write_indent(ctx);
    if (node->data.return_stmt.value != NULL) {
        if (ctx->current_return_type[0] != '\0'
            && strncmp(ctx->current_return_type, "Option<", 7) == 0) {
            const char *inner = slot_inner_type_name(ctx->current_return_type);
            ASTNode *value = node->data.return_stmt.value;
            if (value->type == AST_CALL
                && value->data.call.callee != NULL
                && value->data.call.callee->type == AST_IDENTIFIER) {
                const char *callee_name = value->data.call.callee->data.identifier.name;
                if (strcmp(callee_name, "Some") == 0 && value->data.call.arg_count == 1) {
                    char *arg = emit_expression(value->data.call.arguments[0], ctx);
                    codebuf_write(ctx->out, "return Some_%s(%s);\n", inner, arg);
                    free(arg);
                    return;
                }
                if (strcmp(callee_name, "None") == 0 && value->data.call.arg_count == 0) {
                    codebuf_write(ctx->out, "return None_%s();\n", inner);
                    return;
                }
            }
            if (value->type == AST_IDENTIFIER
                && value->data.identifier.name != NULL
                && strcmp(value->data.identifier.name, "None") == 0) {
                codebuf_write(ctx->out, "return None_%s();\n", inner);
                return;
            }
        }
        const char *saved_expected_type = ctx->expected_type;
        char *val;
        if (ctx->current_return_type[0] != '\0'
            && strcmp(ctx->current_return_type, "Void") != 0
            && strcmp(ctx->current_return_type, "void") != 0) {
            ctx->expected_type = ctx->current_return_type;
        }
        val = emit_expression(node->data.return_stmt.value, ctx);
        ctx->expected_type = saved_expected_type;
        codebuf_write(ctx->out, "return %s;\n", val);
        free(val);
    } else {
        codebuf_write(ctx->out, "return;\n");
    }
}
