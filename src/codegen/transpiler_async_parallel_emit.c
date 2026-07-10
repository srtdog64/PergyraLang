#include "transpiler_async_parallel_emit.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../parser/ast_api.h"
#include "../parser/ast_analysis.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_symbols.h"
#include "transpiler_parallel_capture.h"
#include "transpiler_type_declarator.h"
#include "codegen_type_mapping.h"
#include "transpiler_type_require.h"

static bool
transpiler_capture_surface_desc(char *out, size_t out_size,
                                const char *kind,
                                const char *capture_name)
{
    int written;

    if (out == NULL || out_size == 0 || kind == NULL)
        return false;

    written = snprintf(out, out_size, "%s capture '%s'",
        kind,
        capture_name != NULL ? capture_name : "(anonymous)");

    return written >= 0 && (size_t)written < out_size;
}

static void
transpiler_capture_surface_desc_too_long(TranspilerCtx *ctx,
                                         const char *kind)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s capture diagnostic surface is too long for C backend emission",
        kind != NULL ? kind : "parallel/async");
}

static bool
transpiler_capture_reject_shared_collection(TranspilerCtx *ctx,
                                            const char *kind,
                                            const char *capture_name,
                                            const char *type_name)
{
    const char *storage_kind;

    if (type_name == NULL)
        return false;

    storage_kind =
        codegen_worker_boundary_storage_kind_from_type_name(type_name, false);
    if (storage_kind == NULL)
        return false;
    /* Slice views are fixed {data,len} spans: no realloc/rehash hazard.
     * Their parallel-capture policy is owned by the semantic disjoint-
     * split admission (docs/178 rung 0); a slice that reaches codegen has
     * already passed it. This helper's only caller is the parallel
     * emitter -- async blocks reject all captures separately. */
    if (strcmp(storage_kind, "Slice") == 0)
        return false;

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        "%s capture '%s' cannot share mutable collection '%s' by pointer; use a channel/result boundary or copy before spawning",
        kind != NULL ? kind : "parallel/async",
        capture_name != NULL ? capture_name : "(anonymous)",
        type_name);
    return true;
}

static void
transpiler_write_capture_address(TranspilerCtx *ctx, const char *name)
{
    const char *ssa_name;
    char *c_name;

    if (ctx == NULL || name == NULL)
        return;

    ssa_name = transpiler_resolve_active_ssa_name(ctx, name);
    if (ssa_name == NULL) {
        codebuf_write(ctx->out, "&%s", name);
        return;
    }

    c_name = transpiler_make_c_ssa_name(ctx, ssa_name);
    codebuf_write(ctx->out, "&%s", c_name != NULL ? c_name : name);
    free(c_name);
}

static void
transpiler_write_capture_value(TranspilerCtx *ctx, const char *name)
{
    const char *ssa_name;
    char *c_name;

    if (ctx == NULL || name == NULL)
        return;

    ssa_name = transpiler_resolve_active_ssa_name(ctx, name);
    if (ssa_name == NULL) {
        codebuf_write(ctx->out, "%s", name);
        return;
    }

    c_name = transpiler_make_c_ssa_name(ctx, ssa_name);
    codebuf_write(ctx->out, "%s", c_name != NULL ? c_name : name);
    free(c_name);
}

typedef struct TranspilerParallelWrapperState {
    CodeBuf *out;
    int indent;
    bool in_parallel_wrapper;
    int slot_count;
    int typed_count;
    char slot_names[MAX_SLOT_VARS][64];
    char typed_names[MAX_SLOT_VARS][64];
    bool typed_snapshot[MAX_SLOT_VARS];
} TranspilerParallelWrapperState;

static void
transpiler_parallel_wrapper_state_enter(
    TranspilerCtx *ctx,
    TranspilerParallelWrapperState *state,
    char capture_slot_names[MAX_SLOT_VARS][64],
    int capture_slot_count,
    char capture_typed_names[MAX_SLOT_VARS][64],
    int capture_typed_count,
    const bool capture_typed_snapshot[MAX_SLOT_VARS])
{
    if (ctx == NULL || state == NULL)
        return;

    state->out = ctx->out;
    state->indent = ctx->indent;
    state->in_parallel_wrapper = ctx->in_parallel_wrapper;
    state->slot_count = ctx->par_capture_slot_count;
    state->typed_count = ctx->par_capture_typed_count;
    memcpy(state->slot_names, ctx->par_capture_slot_names,
           sizeof(state->slot_names));
    memcpy(state->typed_names, ctx->par_capture_typed_names,
           sizeof(state->typed_names));
    memcpy(state->typed_snapshot, ctx->par_capture_typed_snapshot,
           sizeof(state->typed_snapshot));

    ctx->out = ctx->helpers;
    ctx->indent = 1;
    ctx->in_parallel_wrapper = true;
    memcpy(ctx->par_capture_slot_names, capture_slot_names,
           sizeof(state->slot_names));
    memcpy(ctx->par_capture_typed_names, capture_typed_names,
           sizeof(state->typed_names));
    ctx->par_capture_slot_count = capture_slot_count;
    ctx->par_capture_typed_count = capture_typed_count;
    if (capture_typed_snapshot != NULL) {
        memcpy(ctx->par_capture_typed_snapshot, capture_typed_snapshot,
               sizeof(state->typed_snapshot));
    } else {
        memset(ctx->par_capture_typed_snapshot, 0,
               sizeof(ctx->par_capture_typed_snapshot));
    }
}

static void
transpiler_parallel_wrapper_state_restore(
    TranspilerCtx *ctx,
    const TranspilerParallelWrapperState *state)
{
    if (ctx == NULL || state == NULL)
        return;

    ctx->out = state->out;
    ctx->indent = state->indent;
    ctx->in_parallel_wrapper = state->in_parallel_wrapper;
    memcpy(ctx->par_capture_slot_names, state->slot_names,
           sizeof(state->slot_names));
    memcpy(ctx->par_capture_typed_names, state->typed_names,
           sizeof(state->typed_names));
    memcpy(ctx->par_capture_typed_snapshot, state->typed_snapshot,
           sizeof(state->typed_snapshot));
    ctx->par_capture_slot_count = state->slot_count;
    ctx->par_capture_typed_count = state->typed_count;
}

void
emit_parallel_block(ASTNode *node, TranspilerCtx *ctx)
{
    size_t count = ast_parallel_task_count(node);
    if (count == 0)
        return;

    /* Capture dispositions are checker facts (docs/178, docs/180 §6): an
     * unsealed node never ran the checker, and re-deriving the analysis
     * here is exactly the C/LLVM drift surface the migration removed. */
    if (!ast_parallel_dispositions_sealed(node)) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "parallel block reached the C emitter without checker-sealed capture dispositions");
        return;
    }

    unsigned int pid = ctx->parallel_id++;

    /* ---------------------------------------------------------------
     * 1) Generate a context struct that holds pointers to all local
     *    variables currently in scope (slots + non-duplicate typed vars).
     *    Wrapper functions access outer variables through this struct.
     * --------------------------------------------------------------- */
    char capture_slot_names[MAX_SLOT_VARS][64] = {{0}};
    char capture_typed_names[MAX_SLOT_VARS][64] = {{0}};
    TranspilerParallelCallableCapture capture_typed_callables[MAX_SLOT_VARS] = {{0}};
    int capture_slot_count = 0;
    int capture_typed_count = 0;

    for (size_t i = 0; i < count; i++) {
        transpiler_parallel_collect_stmt_captures(ast_parallel_task(node, i), ctx,
            capture_slot_names, &capture_slot_count,
            capture_typed_names, capture_typed_callables, &capture_typed_count);
    }

    bool has_captures = (capture_slot_count > 0 || capture_typed_count > 0);

    /* docs/178 Copy evidence, consumed as checker facts: a `<name>__snap`
     * value member is materialized for every snapshot row the checker
     * sealed on this node. Reader arms consume the pre-parallel value; the
     * writer arm keeps the shared pointer (exclusive live location). This
     * emitter performs no writer or eligibility analysis of its own. */
    bool capture_typed_snap_needed[MAX_SLOT_VARS] = {0};
    size_t capture_typed_snap_writer[MAX_SLOT_VARS] = {0};
    for (int ci = 0; ci < capture_typed_count; ci++) {
        const ASTParallelSnapshotRow *row =
            ast_parallel_snapshot_row_find(node, capture_typed_names[ci]);
        if (row == NULL)
            continue;
        capture_typed_snap_needed[ci] = true;
        capture_typed_snap_writer[ci] = row->writer_task;
    }

    if (has_captures) {
        codebuf_write(ctx->helpers,
            "typedef struct {\n");
        for (int i = 0; i < capture_slot_count; i++) {
            const char *name = capture_slot_names[i];
            char inner_buf[128];
            const char *inner = NULL;
            bool secure = lookup_slot_is_secure(ctx, name);
            if (lookup_slot_type_copy(ctx, name, inner_buf,
                    sizeof(inner_buf))) {
                inner = inner_buf;
            }
            if (inner == NULL || inner[0] == '\0') {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "parallel capture '%s' requires concrete Slot<T> metadata",
                    name);
                return;
            }
            codebuf_write(ctx->helpers,
                secure ? "    PgySecureSlot_%s *%s;\n" : "    PgySlot_%s *%s;\n",
                inner, name);
        }
        for (int i = 0; i < capture_typed_count; i++) {
            TypedVarEntry *entry = lookup_typed_entry(ctx, capture_typed_names[i]);
            char surface_desc[256];
            char c_type_buf[128];
            const char *c_type = NULL;
            const char *type_name = entry != NULL ? entry->type_name : NULL;
            if (!transpiler_capture_surface_desc(surface_desc,
                    sizeof(surface_desc), "parallel",
                    capture_typed_names[i])) {
                transpiler_capture_surface_desc_too_long(ctx, "parallel");
                return;
            }
            if (transpiler_capture_reject_shared_collection(ctx, "parallel",
                    capture_typed_names[i], type_name)) {
                return;
            }

            /* Function-typed locals need a pointer-to-function-pointer
             * declarator so `(*_pctx->name)` inside the wrapper yields the
             * function pointer (not a primitive deref). */
            if (capture_typed_callables[i].is_callable) {
                char ptr_name[sizeof(capture_typed_names[i]) + 1];
                ptr_name[0] = '*';
                memcpy(ptr_name + 1, capture_typed_names[i],
                       sizeof(capture_typed_names[i]));
                ptr_name[sizeof(ptr_name) - 1] = '\0';
                char *decl = pergyra_func_pointer_declarator_from_type_names_in_ctx(
                    ctx,
                    capture_typed_callables[i].return_type_name,
                    capture_typed_callables[i].param_count,
                    capture_typed_callables[i].param_type_names,
                    ptr_name);
                if (decl != NULL) {
                    codebuf_write(ctx->helpers, "    %s;\n", decl);
                    free(decl);
                    continue;
                }
                return;
            }

            if (transpiler_require_type_name_c_type_copy(
                ctx,
                type_name,
                surface_desc,
                c_type_buf,
                sizeof(c_type_buf))) {
                c_type = c_type_buf;
            }
            if (c_type == NULL)
                return;
            codebuf_write(ctx->helpers,
                "    %s *%s;\n", c_type,
                capture_typed_names[i]);
            if (capture_typed_snap_needed[i]) {
                codebuf_write(ctx->helpers,
                    "    %s %s__snap;\n", c_type,
                    capture_typed_names[i]);
            }
        }
        codebuf_write(ctx->helpers,
            "} _pgy_par_ctx_%u;\n\n", pid);
    }

    /* ---------------------------------------------------------------
     * 2) Generate static wrapper functions for each task.
     *    Variable references inside the wrapper go through _pctx->.
     * --------------------------------------------------------------- */
    for (size_t i = 0; i < count; i++) {
        codebuf_write(ctx->helpers,
            "static void *_pgy_par_%zu_%u(void *_arg) {\n",
            i, pid);
        if (has_captures) {
            codebuf_write(ctx->helpers,
                "    _pgy_par_ctx_%u *_pctx = "
                "(_pgy_par_ctx_%u *)_arg;\n",
                pid, pid);
        } else {
            codebuf_write(ctx->helpers, "    (void)_arg;\n");
        }

        TranspilerParallelWrapperState wrapper_state;
        bool arm_snapshot[MAX_SLOT_VARS] = {0};
        for (int ci = 0; ci < capture_typed_count; ci++) {
            arm_snapshot[ci] = capture_typed_snap_needed[ci]
                && i != capture_typed_snap_writer[ci];
        }
        transpiler_parallel_wrapper_state_enter(
            ctx, &wrapper_state, capture_slot_names, capture_slot_count,
            capture_typed_names, capture_typed_count, arm_snapshot);

        emit_statement(ast_parallel_task(node, i), ctx);

        transpiler_parallel_wrapper_state_restore(ctx, &wrapper_state);

        codebuf_write(ctx->helpers,
            "    return NULL;\n"
            "}\n\n");
    }

    /* ---------------------------------------------------------------
     * 3) Emit context initialization + spawn + await at call site.
     * --------------------------------------------------------------- */
    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    if (has_captures) {
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_par_ctx_%u _pctx%u = { ", pid, pid);
        bool first = true;
        for (int i = 0; i < capture_slot_count; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            transpiler_write_capture_address(ctx, capture_slot_names[i]);
            first = false;
        }
        for (int i = 0; i < capture_typed_count; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            transpiler_write_capture_address(ctx, capture_typed_names[i]);
            first = false;
            if (capture_typed_snap_needed[i]) {
                codebuf_write(ctx->out, ", ");
                transpiler_write_capture_value(ctx, capture_typed_names[i]);
            }
        }
        codebuf_write(ctx->out, " };\n");
    }

    for (size_t i = 0; i < count; i++) {
        write_indent(ctx);
        if (has_captures) {
            codebuf_write(ctx->out,
                "PgyTaskHandle _ph_%zu = pgy_lane_spawn_dispatch(PGY_LANE_WORKER_POOL, _pgy_par_%zu_%u, &_pctx%u);\n",
                i, i, pid, pid);
        } else {
            codebuf_write(ctx->out,
                "PgyTaskHandle _ph_%zu = pgy_lane_spawn_dispatch(PGY_LANE_WORKER_POOL, _pgy_par_%zu_%u, NULL);\n",
                i, i, pid);
        }
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (_ph_%zu.task == NULL) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \"parallel task spawn failed\");\n",
            i);
    }
    for (size_t i = 0; i < count; i++) {
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_lane_await(_ph_%zu);\n", i);
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

void
emit_async_block(ASTNode *node, TranspilerCtx *ctx)
{
    unsigned int pid = ctx->parallel_id++;
    char capture_slot_names[MAX_SLOT_VARS][64] = {{0}};
    char capture_typed_names[MAX_SLOT_VARS][64] = {{0}};
    TranspilerParallelCallableCapture capture_typed_callables[MAX_SLOT_VARS] = {{0}};
    int capture_slot_count = 0;
    int capture_typed_count = 0;

    for (size_t i = 0; i < ast_async_block_statement_count(node); i++) {
        transpiler_parallel_collect_stmt_captures(ast_async_block_statement(node, i), ctx,
            capture_slot_names, &capture_slot_count,
            capture_typed_names, capture_typed_callables, &capture_typed_count);
    }

    if (capture_slot_count > 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            "async block cannot capture Slot<T> local '%s' by pointer; use a named task boundary or explicit handoff",
            capture_slot_names[0]);
        return;
    }
    if (capture_typed_count > 0) {
        TypedVarEntry *entry = lookup_typed_entry(ctx, capture_typed_names[0]);
        const char *type_name = entry != NULL ? entry->type_name : NULL;
        const char *storage_kind =
            codegen_worker_boundary_storage_kind_from_type_name(type_name, true);
        if (storage_kind != NULL
            && strcmp(storage_kind, "Channel") == 0) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                "async block cannot capture Channel<T> local '%s' by pointer; use parallel or a named task boundary with explicit handoff",
                capture_typed_names[0]);
            return;
        }
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            "async block cannot capture non-Channel local '%s' of type '%s' by pointer; use a named task boundary or explicit value handoff",
            capture_typed_names[0],
            type_name != NULL ? type_name : "Unknown");
        return;
    }

    codebuf_write(ctx->helpers, "static void *_pgy_async_%u(void *_arg) {\n", pid);
    codebuf_write(ctx->helpers, "    (void)_arg;\n");

    TranspilerParallelWrapperState wrapper_state;
    transpiler_parallel_wrapper_state_enter(
        ctx, &wrapper_state, capture_slot_names, capture_slot_count,
        capture_typed_names, capture_typed_count, NULL);

    for (size_t i = 0; i < ast_async_block_statement_count(node); i++)
        emit_statement(ast_async_block_statement(node, i), ctx);

    transpiler_parallel_wrapper_state_restore(ctx, &wrapper_state);

    codebuf_write(ctx->helpers, "    return NULL;\n}\n\n");

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out,
        "PgyTaskHandle _ah_%u = pgy_lane_spawn_dispatch(PGY_LANE_LOCAL_ASYNC, _pgy_async_%u, NULL);\n",
        pid, pid);
    write_indent(ctx);
    codebuf_write(ctx->out,
        "if (_ah_%u.task == NULL) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \"async block spawn failed\");\n",
        pid);
    write_indent(ctx);
    codebuf_write(ctx->out, "pgy_lane_detach(_ah_%u);\n", pid);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}
