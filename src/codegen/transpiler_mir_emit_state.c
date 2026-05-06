#define PGY_TRANSPILER_MIR_EMIT_STATE_OWNER
#include "transpiler_mir_emit_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transpiler_decl_lookup.h"
#include "transpiler_context.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_symbols.h"
#include "transpiler_type_render.h"

static void
transpiler_restore_mir_emit_state_local(TranspilerCtx *ctx,
                                        int saved_slot_count,
                                        int saved_typed_count,
                                        const char *saved_return_type,
                                        const ASTNode *saved_func_decl,
                                        ASTNode *saved_host_decl,
                                        TranspilerCtx *saved_render_ctx,
                                        CodeBuf *saved_out)
{
    if (ctx == NULL)
        return;

    transpiler_restore_local_binding_counts_local(ctx, saved_slot_count,
                                                  saved_typed_count, -1);
    if (saved_return_type != NULL) {
        snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
            "%s", saved_return_type);
    }
    ctx->current_func_decl = saved_func_decl;
    transpiler_bind_current_host_decl_local(ctx, saved_host_decl);
    transpiler_type_render_ctx_restore(saved_render_ctx);
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
    state->host_decl = transpiler_current_host_decl_local(ctx);
    state->out = ctx->out;
    state->render_ctx = transpiler_type_render_ctx_current();
    state->func_decl = ctx->current_func_decl;
    snprintf(state->return_type, sizeof(state->return_type), "%s",
        ctx->current_return_type);
}

void
transpiler_restore_mir_emit_state_from_snapshot_local(
    TranspilerCtx *ctx, const TranspilerMirEmitState *state)
{
    if (ctx == NULL || state == NULL)
        return;

    transpiler_restore_mir_emit_state_local(
        ctx, state->slot_count, state->typed_count, state->return_type,
        state->func_decl, state->host_decl, state->render_ctx, state->out);
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

    if (host_decl == NULL) {
        if (ctx->backend_error == NULL) {
            transpiler_set_backend_error(ctx,
                "MIR-only transpiler missing declaration inventory for host-scoped method '%s.%s'",
                self_type_name,
                method->type == AST_FUNC_DECL && method->data.func_decl.name != NULL
                    ? method->data.func_decl.name : "<method>");
        }
        return;
    }

    transpiler_capture_mir_emit_state_local(ctx, &saved_emit_state);

    transpiler_bind_current_host_decl_local(ctx, host_decl);
    if (body_out != NULL)
        ctx->out = body_out;
    register_typed_var(ctx, "self", self_type_name);

    for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
        FuncParam *p = method->data.func_decl.params[j];
        char *type_name;

        if (p == NULL || p->name == NULL
            || strcmp(p->name, "self") == 0
            || p->type == NULL)
            continue;

        type_name = render_type_name(p->type);
        if (type_name != NULL) {
            register_typed_var(ctx, p->name, type_name);
            if (mark_subject_ref_params
                && is_pointer_self_host_type_name(ctx, type_name)) {
                TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                if (entry != NULL)
                    entry->is_subject_ref = true;
            }
            free(type_name);
        }
    }

    ctx->indent++;
    if (method->data.func_decl.body != NULL)
        emit_block(method->data.func_decl.body, ctx);
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

    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
             "%s", type_name != NULL ? type_name : "Void");
}
