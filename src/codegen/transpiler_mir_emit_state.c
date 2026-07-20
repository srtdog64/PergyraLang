#define PGY_TRANSPILER_MIR_EMIT_STATE_OWNER
#include "transpiler_mir_emit_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transpiler_decl_lookup.h"
#include "transpiler_context.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_inventory_view.h"
#include "transpiler_symbols.h"
#include "transpiler_type_render.h"
#include "semantic/diag_codes.h"

static bool
transpiler_mir_emit_copy_return_type(char *out, size_t out_size,
                                     const char *type_name)
{
    size_t len;

    if (out == NULL || out_size == 0 || type_name == NULL)
        return false;

    len = strlen(type_name);
    if (len >= out_size)
        return false;

    memcpy(out, type_name, len + 1);
    return true;
}

static void
transpiler_mir_emit_return_type_too_long(TranspilerCtx *ctx)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "function return type is too long for C backend emit state");
}

static void
transpiler_restore_mir_emit_state_local(TranspilerCtx *ctx,
                                        int saved_slot_count,
                                        int saved_typed_count,
                                        int saved_indent,
                                        const char *saved_return_type,
                                        ASTNode *saved_return_callable_type,
                                        const ASTNode *saved_func_decl,
                                        ASTNode *saved_host_decl,
                                        CodeBuf *saved_out)
{
    if (ctx == NULL)
        return;

    transpiler_restore_local_binding_counts_local(ctx, saved_slot_count,
                                                  saved_typed_count, -1);
    ctx->indent = saved_indent;
    if (saved_return_type != NULL) {
        if (!transpiler_mir_emit_copy_return_type(ctx->current_return_type,
                sizeof(ctx->current_return_type), saved_return_type)) {
            transpiler_mir_emit_return_type_too_long(ctx);
            return;
        }
    }
    ctx->current_return_callable_type = saved_return_callable_type;
    ctx->current_func_decl = saved_func_decl;
    transpiler_bind_current_host_decl_local(ctx, saved_host_decl);
    ctx->out = saved_out;
}

void
transpiler_capture_mir_emit_state_local(TranspilerCtx *ctx,
                                        TranspilerMirEmitState *state)
{
    if (ctx == NULL || state == NULL)
        return;

    state->slot_count = ctx->slot_var_count;
    state->typed_count = ctx->typed_var_count;
    state->indent = ctx->indent;
    state->host_decl = transpiler_current_host_decl_local(ctx);
    state->out = ctx->out;
    state->func_decl = ctx->current_func_decl;
    state->return_callable_type = ctx->current_return_callable_type;
    state->mir_routine = ctx->active_mir_routine;
    if (!transpiler_mir_emit_copy_return_type(state->return_type,
            sizeof(state->return_type), ctx->current_return_type)) {
        transpiler_mir_emit_return_type_too_long(ctx);
        state->return_type[0] = '\0';
    }
}

void
transpiler_restore_mir_emit_state_from_snapshot_local(
    TranspilerCtx *ctx, const TranspilerMirEmitState *state)
{
    if (ctx == NULL || state == NULL)
        return;

    transpiler_restore_mir_emit_state_local(
        ctx, state->slot_count, state->typed_count, state->indent,
        state->return_type,
        state->return_callable_type,
        state->func_decl, state->host_decl, state->out);
    ctx->active_mir_routine = state->mir_routine;
}

void
transpiler_emit_host_method_body_local(TranspilerCtx *ctx, ASTNode *host_decl,
                                       const char *self_type_name,
                                       ASTNode *method, CodeBuf *body_out,
                                       bool mark_subject_ref_params)
{
    TranspilerMirEmitState saved_emit_state;

    if (ctx == NULL || method == NULL || self_type_name == NULL)
        return;

    if (transpiler_active_has_mir(ctx)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path attempted AST hosted method body emission for '%s.%s'",
            self_type_name,
            method->type == AST_FUNC_DECL && ast_declaration_name(method) != NULL
                ? ast_declaration_name(method) : "<method>");
        return;
    }

    if (host_decl == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only transpiler missing declaration inventory for host-scoped method '%s.%s'",
            self_type_name,
            method->type == AST_FUNC_DECL && ast_declaration_name(method) != NULL
                ? ast_declaration_name(method) : "<method>");
        return;
    }

    transpiler_capture_mir_emit_state_local(ctx, &saved_emit_state);

    transpiler_bind_current_host_decl_local(ctx, host_decl);
    ctx->current_func_decl = method;
    if (body_out != NULL)
        ctx->out = body_out;
    register_typed_var(ctx, "self", self_type_name);

    size_t param_count = ast_func_param_count(method);
    for (size_t j = 0; j < param_count; j++) {
        FuncParam *p = ast_func_param(method, j);
        char *type_name;

        if (p == NULL || p->name == NULL
            || strcmp(p->name, "self") == 0
            || p->type == NULL)
            continue;

        type_name = render_type_name_in_ctx(ctx, p->type);
        if (type_name != NULL) {
            register_typed_var(ctx, p->name, type_name);
            if (mark_subject_ref_params
                && is_pointer_self_host_type_name(ctx, type_name)) {
                TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                if (entry != NULL)
                    entry->is_indirect_ref = true;
            }
            free(type_name);
        }
    }

    ctx->indent++;
    ASTNode *body = ast_func_body(method);
    if (body != NULL)
        emit_block(body, ctx);
    ctx->indent--;

    transpiler_restore_mir_emit_state_from_snapshot_local(
        ctx, &saved_emit_state);
}

void
transpiler_restore_local_binding_counts_local(TranspilerCtx *ctx,
                                              int saved_slot_count,
                                              int saved_typed_count,
                                              int saved_alias_count)
{
    if (ctx == NULL)
        return;

    ctx->slot_var_count = saved_slot_count;
    ctx->typed_var_count = saved_typed_count;
    if (saved_alias_count >= 0)
        ctx->alias_var_count = saved_alias_count;
}

void
transpiler_bind_function_emit_host_local(TranspilerCtx *ctx,
                                         ASTNode *host_decl,
                                         const ASTNode *func_decl)
{
    if (ctx == NULL)
        return;

    transpiler_bind_current_host_decl_local(ctx, host_decl);
    if (func_decl != NULL && func_decl->type == AST_FUNC_DECL)
        ctx->current_func_decl = func_decl;
}

void
transpiler_set_current_return_type_local(TranspilerCtx *ctx,
                                         const char *type_name)
{
    if (ctx == NULL)
        return;

    if (!transpiler_mir_emit_copy_return_type(ctx->current_return_type,
            sizeof(ctx->current_return_type),
            type_name != NULL ? type_name : "Void"))
        transpiler_mir_emit_return_type_too_long(ctx);
    ctx->current_return_callable_type = NULL;
}
