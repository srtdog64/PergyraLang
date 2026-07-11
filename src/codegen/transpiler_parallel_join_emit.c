/*
 * Copyright (c) 2026 Pergyra Language Project
 * C emission for the join-form parallel block (docs/181 SS1, rungs 0+1):
 *   parallel (x in xs)     [join with all] { body }   element mode
 *   parallel (i in lo..hi) [join with all] { body }   index mode (R1)
 *
 * One wrapper function, N runtime tasks: the call site reads the fan-out
 * length (collection length, or hi-lo clamped at zero), builds one
 * capture context per task (captures by pointer exactly like the arms
 * form; the element/index travels by value), fans out through the worker
 * pool, and joins on every handle. Array captures are admitted from the
 * checker-sealed index-disjointness fact list only; the rungs keep the
 * remaining capture surface narrow and fail closed on the kinds they do
 * not carry yet (slots, callable locals).
 */

#include "transpiler_async_parallel_emit.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../parser/ast_api.h"
#include "../parser/ast_analysis.h"
#include "../semantic/diag_codes.h"
#include "codegen_type_mapping.h"
#include "transpiler_context.h"
#include "transpiler_symbols.h"
#include "transpiler_parallel_capture.h"
#include "transpiler_type_require.h"

/* "Array<Int>" -> "Int" (the checker already guaranteed this shape). */
static bool
join_collection_inner_suffix(const char *type_name, char *out,
                             size_t out_size)
{
    size_t len;

    if (type_name == NULL || strncmp(type_name, "Array<", 6) != 0)
        return false;
    len = strlen(type_name);
    if (len < 8 || type_name[len - 1] != '>' || len - 7 >= out_size)
        return false;
    memcpy(out, type_name + 6, len - 7);
    out[len - 7] = '\0';
    return true;
}

static void
join_set_error(TranspilerCtx *ctx, const char *fmt, const char *arg)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        fmt, arg != NULL ? arg : "(anonymous)");
}

void
emit_parallel_join_block(ASTNode *node, TranspilerCtx *ctx)
{
    const char *elem_name = ast_parallel_join_element(node);
    ASTNode *coll = ast_parallel_join_collection(node);
    ASTNode *range_end = ast_parallel_join_range_end(node);
    bool index_mode = range_end != NULL;
    ASTNode *body = ast_parallel_task(node, 0);
    const char *coll_name = NULL;
    TypedVarEntry *coll_entry;
    char inner_suffix[64];
    char elem_c_type[128];
    unsigned int pid;

    if (!ast_parallel_dispositions_sealed(node)) {
        join_set_error(ctx,
            "parallel join block reached the C emitter without checker-sealed capture dispositions%s",
            "");
        return;
    }
    if (elem_name == NULL || body == NULL || coll == NULL
        || (!index_mode && coll->type != AST_IDENTIFIER)) {
        join_set_error(ctx,
            "parallel join block reached the C emitter in a non-admitted shape%s",
            "");
        return;
    }
    if (index_mode) {
        /* The binding is the Int index; endpoints are Int expressions. */
        if (!transpiler_require_type_name_c_type_copy(ctx, "Int",
                "parallel join index", elem_c_type, sizeof(elem_c_type)))
            return;
    } else {
        coll_name = ast_identifier_name(coll);
        coll_entry = lookup_typed_entry(ctx, coll_name);
        if (coll_entry == NULL
            || !join_collection_inner_suffix(coll_entry->type_name,
                    inner_suffix, sizeof(inner_suffix))) {
            join_set_error(ctx,
                "parallel join collection '%s' requires concrete Array<T> metadata",
                coll_name);
            return;
        }
        if (!transpiler_require_type_name_c_type_copy(ctx, inner_suffix,
                "parallel join element", elem_c_type, sizeof(elem_c_type)))
            return;
    }

    pid = ctx->parallel_id++;

    /* ---------------------------------------------------------------
     * Captures: same pointer discipline as the arms form; the element
     * binding itself is NOT a capture (it is a wrapper-local fed from
     * the per-element context field).
     * --------------------------------------------------------------- */
    char capture_slot_names[MAX_SLOT_VARS][64] = {{0}};
    char capture_typed_names[MAX_SLOT_VARS][64] = {{0}};
    TranspilerParallelCallableCapture capture_typed_callables[MAX_SLOT_VARS] = {{0}};
    int capture_slot_count = 0;
    int capture_typed_count = 0;

    transpiler_parallel_collect_stmt_captures(body, ctx,
        capture_slot_names, &capture_slot_count,
        capture_typed_names, capture_typed_callables, &capture_typed_count);

    if (capture_slot_count > 0) {
        join_set_error(ctx,
            "parallel join rung 0 does not carry slot captures yet ('%s'); a later rung will (docs/181 SS1.4)",
            capture_slot_names[0]);
        return;
    }
    for (int i = 0; i < capture_typed_count; i++) {
        if (capture_typed_callables[i].is_callable) {
            join_set_error(ctx,
                "parallel join rung 0 does not carry callable captures yet ('%s'); a later rung will (docs/181 SS1.4)",
                capture_typed_names[i]);
            return;
        }
        /* Array captures cross the boundary only with the checker-sealed
         * index-disjointness fact (docs/181 R1); anything else is the
         * shared-mutable-collection hazard the arms form rejects too. */
        {
            TypedVarEntry *entry =
                lookup_typed_entry(ctx, capture_typed_names[i]);
            const char *tn = entry != NULL ? entry->type_name : NULL;

            if (tn != NULL && transpiler_type_name_is_array(tn)
                && !ast_parallel_join_index_array_admitted(node,
                        capture_typed_names[i])) {
                join_set_error(ctx,
                    "parallel join capture '%s' shares a mutable array without index-disjointness evidence; the checker fact is missing",
                    capture_typed_names[i]);
                return;
            }
        }
    }

    /* ---------------------------------------------------------------
     * 1) Per-element context struct.
     * --------------------------------------------------------------- */
    codebuf_write(ctx->helpers, "typedef struct {\n");
    for (int i = 0; i < capture_typed_count; i++) {
        char surface_desc[256];
        char c_type_buf[128];

        snprintf(surface_desc, sizeof(surface_desc),
                 "parallel join capture '%s'", capture_typed_names[i]);
        if (!transpiler_require_type_name_c_type_copy(ctx,
                lookup_typed_entry(ctx, capture_typed_names[i]) != NULL
                    ? lookup_typed_entry(ctx, capture_typed_names[i])->type_name
                    : NULL,
                surface_desc, c_type_buf, sizeof(c_type_buf)))
            return;
        codebuf_write(ctx->helpers, "    %s *%s;\n", c_type_buf,
                      capture_typed_names[i]);
    }
    codebuf_write(ctx->helpers,
        "    %s __join_elem;\n"
        "} _pgy_pjoin_ctx_%u;\n\n", elem_c_type, pid);

    /* ---------------------------------------------------------------
     * 2) The one replicated wrapper.
     * --------------------------------------------------------------- */
    codebuf_write(ctx->helpers,
        "static void *_pgy_pjoin_%u(void *_arg) {\n"
        "    _pgy_pjoin_ctx_%u *_pctx = (_pgy_pjoin_ctx_%u *)_arg;\n"
        "    %s %s = _pctx->__join_elem;\n"
        "    (void)%s;\n",
        pid, pid, pid, elem_c_type, elem_name, elem_name);

    {
        TranspilerParallelWrapperState wrapper_state;
        char no_slots[MAX_SLOT_VARS][64] = {{0}};

        transpiler_parallel_wrapper_state_enter(ctx, &wrapper_state,
            no_slots, 0, capture_typed_names, capture_typed_count, NULL);
        emit_statement(body, ctx);
        transpiler_parallel_wrapper_state_restore(ctx, &wrapper_state);
    }

    codebuf_write(ctx->helpers,
        "    return NULL;\n"
        "}\n\n");

    /* ---------------------------------------------------------------
     * 3) Call site: length snapshot, fan-out, all-join.
     * --------------------------------------------------------------- */
    {
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;

        if (index_mode) {
            char *lo_expr = emit_expression(coll, ctx);
            char *hi_expr = lo_expr != NULL
                ? emit_expression(range_end, ctx)
                : NULL;

            if (lo_expr == NULL || hi_expr == NULL) {
                join_set_error(ctx,
                    "parallel join could not lower a range endpoint expression%s",
                    "");
                free(lo_expr);
                free(hi_expr);
                return;
            }
            write_indent(ctx);
            codebuf_write(ctx->out,
                "%s _pj_lo_%u = (%s);\n", elem_c_type, pid, lo_expr);
            write_indent(ctx);
            codebuf_write(ctx->out,
                "%s _pj_hi_%u = (%s);\n", elem_c_type, pid, hi_expr);
            free(lo_expr);
            free(hi_expr);
            write_indent(ctx);
            codebuf_write(ctx->out,
                "size_t _pj_n_%u = _pj_hi_%u > _pj_lo_%u"
                " ? (size_t)(_pj_hi_%u - _pj_lo_%u) : 0;\n",
                pid, pid, pid, pid, pid);
        } else {
            char *coll_expr = emit_expression(coll, ctx);

            if (coll_expr == NULL) {
                join_set_error(ctx,
                    "parallel join could not lower collection expression '%s'",
                    coll_name);
                return;
            }
            write_indent(ctx);
            codebuf_write(ctx->out,
                "PgyArray_%s *_pj_src_%u = &%s;\n", inner_suffix, pid,
                coll_expr);
            free(coll_expr);
            write_indent(ctx);
            codebuf_write(ctx->out,
                "size_t _pj_n_%u = _pj_src_%u->length;\n", pid, pid);
        }
        write_indent(ctx);
        codebuf_write(ctx->out,
            "_pgy_pjoin_ctx_%u *_pj_ctxs_%u = (_pgy_pjoin_ctx_%u *)malloc("
            "sizeof(_pgy_pjoin_ctx_%u) * (_pj_n_%u ? _pj_n_%u : 1));\n",
            pid, pid, pid, pid, pid, pid);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyTaskHandle *_pj_hs_%u = (PgyTaskHandle *)malloc("
            "sizeof(PgyTaskHandle) * (_pj_n_%u ? _pj_n_%u : 1));\n",
            pid, pid, pid);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (_pj_ctxs_%u == NULL || _pj_hs_%u == NULL) {\n", pid, pid);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,\n");
        write_indent(ctx);
        codebuf_write(ctx->out,
            "                      PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");

        write_indent(ctx);
        codebuf_write(ctx->out,
            "for (size_t _pj_i = 0; _pj_i < _pj_n_%u; _pj_i++) {\n", pid);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out,
            "_pj_ctxs_%u[_pj_i] = (_pgy_pjoin_ctx_%u){ ", pid, pid);
        for (int i = 0; i < capture_typed_count; i++) {
            transpiler_write_capture_address(ctx, capture_typed_names[i]);
            codebuf_write(ctx->out, ", ");
        }
        if (index_mode) {
            codebuf_write(ctx->out,
                "(%s)(_pj_lo_%u + (%s)_pj_i) };\n",
                elem_c_type, pid, elem_c_type);
        } else {
            codebuf_write(ctx->out,
                "pgy_array_get_%s(_pj_src_%u, _pj_i) };\n", inner_suffix,
                pid);
        }
        write_indent(ctx);
        codebuf_write(ctx->out,
            "_pj_hs_%u[_pj_i] = pgy_lane_spawn_dispatch(PGY_LANE_WORKER_POOL, "
            "_pgy_pjoin_%u, &_pj_ctxs_%u[_pj_i]);\n", pid, pid, pid);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");

        write_indent(ctx);
        codebuf_write(ctx->out,
            "for (size_t _pj_i = 0; _pj_i < _pj_n_%u; _pj_i++) "
            "pgy_lane_await(_pj_hs_%u[_pj_i]);\n", pid, pid);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "free(_pj_ctxs_%u);\n", pid);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "free(_pj_hs_%u);\n", pid);

        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}
