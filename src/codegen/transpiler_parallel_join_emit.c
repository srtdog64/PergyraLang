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
#include "../compiler/mir_parallel_capture_facts.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../parser/ast_api.h"
#include "../parser/ast_analysis.h"
#include "../semantic/diag_codes.h"
#include "codegen_type_mapping.h"
#include "transpiler_context.h"
#include "transpiler_format.h"
#include "transpiler_symbols.h"
#include "transpiler_parallel_capture.h"
#include "transpiler_type_require.h"

/* R4 fold-step spelling per (combinator, give type). Int/Long ride the
 * same checked-arith exports as surface '+'/'*', so the fold cannot
 * reintroduce unchecked overflow; Float/Double are plain IEEE ops; and
 * min/max keep the accumulator on NaN (ordered compare), matching the
 * LLVM fcmp-olt/select twin exactly. */
static void
join_reduce_write_step(TranspilerCtx *ctx, const char *rop,
                       const char *give_name, const char *res_name,
                       unsigned int pid)
{
    bool int_lane = strcmp(give_name, "Int") == 0;
    bool long_lane = strcmp(give_name, "Long") == 0;

    write_indent(ctx);
    if (strcmp(rop, "sum") == 0 && (int_lane || long_lane)) {
        codebuf_write(ctx->out,
            "    %s = pgy_checked_add_%s_export(%s, _pj_v_%u);\n",
            res_name, int_lane ? "i32" : "i64", res_name, pid);
    } else if (strcmp(rop, "product") == 0 && (int_lane || long_lane)) {
        codebuf_write(ctx->out,
            "    %s = pgy_checked_mul_%s_export(%s, _pj_v_%u);\n",
            res_name, int_lane ? "i32" : "i64", res_name, pid);
    } else if (strcmp(rop, "sum") == 0) {
        codebuf_write(ctx->out, "    %s = %s + _pj_v_%u;\n",
            res_name, res_name, pid);
    } else if (strcmp(rop, "product") == 0) {
        codebuf_write(ctx->out, "    %s = %s * _pj_v_%u;\n",
            res_name, res_name, pid);
    } else if (strcmp(rop, "min") == 0) {
        codebuf_write(ctx->out,
            "    %s = _pj_v_%u < %s ? _pj_v_%u : %s;\n",
            res_name, pid, res_name, pid, res_name);
    } else {
        codebuf_write(ctx->out,
            "    %s = _pj_v_%u > %s ? _pj_v_%u : %s;\n",
            res_name, pid, res_name, pid, res_name);
    }
}

/* "Array<Int>" -> "Int" (the checker already guaranteed this shape). */
static bool
join_collection_inner_suffix(const char *type_name, char *out,
                             size_t out_size)
{
    size_t len;

    if (!transpiler_type_name_is_array(type_name))
        return false;
    copy_constructed_arg_name_at(type_name, 0, out, out_size);
    len = strlen(out);
    if (len == 0 || len >= out_size)
        return false;
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

/* Shared emission for both forms; expression mode (give_suffix != NULL)
 * adds a per-task result slot to the context struct, wires `give` to it,
 * and pushes the results into `res_name` in index order after the join. */
static void
emit_parallel_join_common(ASTNode *node, TranspilerCtx *ctx,
                          const char *give_suffix,
                          const char *give_c_type,
                          const char *res_name)
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
    const MIRParallelCaptureBoundaryFact *capture_boundary =
        mir_parallel_capture_boundary_find(
            transpiler_active_mir_identity(ctx), ast_node_stable_id(node));

    if (capture_boundary == NULL || !capture_boundary->sealed
        || capture_boundary->task_count != ast_parallel_task_count(node)) {
        join_set_error(ctx,
            "parallel join block reached the C emitter without a matching sealed MIR capture boundary%s",
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
        /* Array captures cross the boundary only with a checker-sealed
         * fact row: R1 index-disjointness or R5 snapshot-read. Anything
         * else is the shared-mutable-collection hazard the arms form
         * rejects too. */
        {
            TypedVarEntry *entry =
                lookup_typed_entry(ctx, capture_typed_names[i]);
            const char *tn = entry != NULL ? entry->type_name : NULL;

            if (tn != NULL && transpiler_type_name_is_array(tn)
                && mir_parallel_capture_disposition_find(
                    capture_boundary, capture_typed_names[i],
                    MIR_PARALLEL_CAPTURE_JOIN_INDEX_DISJOINT) == NULL
                && mir_parallel_capture_disposition_find(
                    capture_boundary, capture_typed_names[i],
                    MIR_PARALLEL_CAPTURE_JOIN_READONLY) == NULL) {
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
    codebuf_write(ctx->wrappers, "typedef struct {\n");
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
        codebuf_write(ctx->wrappers, "    %s *%s;\n", c_type_buf,
                      capture_typed_names[i]);
    }
    codebuf_write(ctx->wrappers,
        "    %s __join_elem;\n", elem_c_type);
    if (give_suffix != NULL && ast_parallel_join_is_any(node)) {
        /* R3 any-join: shared decision cell + shared winner-result cell
         * (both call-site locals), instead of a per-task slot. */
        codebuf_write(ctx->wrappers,
            "    int32_t *__join_any;\n"
            "    %s *__join_any_res;\n", give_c_type);
    } else if (give_suffix != NULL) {
        codebuf_write(ctx->wrappers,
            "    %s __join_result;\n", give_c_type);
    }
    codebuf_write(ctx->wrappers,
        "} _pgy_pjoin_ctx_%u;\n\n", pid);

    /* ---------------------------------------------------------------
     * 2) The one replicated wrapper.
     * --------------------------------------------------------------- */
    codebuf_write(ctx->wrappers,
        "static void *_pgy_pjoin_%u(void *_arg) {\n"
        "    _pgy_pjoin_ctx_%u *_pctx = (_pgy_pjoin_ctx_%u *)_arg;\n",
        pid, pid, pid);
    if (give_suffix != NULL && ast_parallel_join_is_any(node)) {
        /* Entry safe point (docs/181 SS2.4): a task that starts after
         * the decision retires immediately; queued tasks are also
         * skipped by the pool's own pre-run cancel check. */
        codebuf_write(ctx->wrappers,
            "    if (__atomic_load_n(_pctx->__join_any, __ATOMIC_ACQUIRE)"
            " != 0) return NULL;\n");
    }
    codebuf_write(ctx->wrappers,
        "    %s %s = _pctx->__join_elem;\n"
        "    (void)%s;\n",
        elem_c_type, elem_name, elem_name);

    {
        TranspilerParallelWrapperState wrapper_state;
        char no_slots[MAX_SLOT_VARS][64] = {{0}};
        bool saved_give = ctx->in_pjoin_give;
        bool saved_any = ctx->in_pjoin_any;

        transpiler_parallel_wrapper_state_enter(ctx, &wrapper_state,
            no_slots, 0, capture_typed_names, capture_typed_count, NULL);
        ctx->in_pjoin_give = give_suffix != NULL;
        ctx->in_pjoin_any = give_suffix != NULL
            && ast_parallel_join_is_any(node);
        emit_statement(body, ctx);
        ctx->in_pjoin_give = saved_give;
        ctx->in_pjoin_any = saved_any;
        transpiler_parallel_wrapper_state_restore(ctx, &wrapper_state);
    }

    codebuf_write(ctx->wrappers,
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

        /* R5 alias fail-close (docs/181): a snapshot-read capture whose
         * backing IS an index-written array would let free-index reads
         * race the per-index writes (`let b = a;` copies the handle,
         * not the buffer). One pointer compare per (written, read) pair
         * at fan-out entry; the LLVM twin emits the same check into the
         * same panic export pair, so class and reason match across
         * backends. */
        for (int wi = 0; wi < capture_typed_count; wi++) {
            if (mir_parallel_capture_disposition_find(
                    capture_boundary, capture_typed_names[wi],
                    MIR_PARALLEL_CAPTURE_JOIN_INDEX_DISJOINT) == NULL)
                continue;
            for (int ri = 0; ri < capture_typed_count; ri++) {
                if (mir_parallel_capture_disposition_find(
                        capture_boundary, capture_typed_names[ri],
                        MIR_PARALLEL_CAPTURE_JOIN_READONLY) == NULL)
                    continue;
                write_indent(ctx);
                codebuf_write(ctx->out, "if ((");
                transpiler_write_capture_address(ctx,
                    capture_typed_names[wi]);
                codebuf_write(ctx->out, ")->data != NULL && (");
                transpiler_write_capture_address(ctx,
                    capture_typed_names[wi]);
                codebuf_write(ctx->out, ")->data == (void *)(");
                transpiler_write_capture_address(ctx,
                    capture_typed_names[ri]);
                codebuf_write(ctx->out,
                    ")->data) pgy_runtime_panic_authority_mismatch_export("
                    "\"parallel join read-only capture aliases an index-written array\");\n");
            }
        }

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
                "(%s)(_pj_lo_%u + (%s)_pj_i)",
                elem_c_type, pid, elem_c_type);
        } else {
            codebuf_write(ctx->out,
                "pgy_array_get_%s(_pj_src_%u, _pj_i)", inner_suffix,
                pid);
        }
        if (give_suffix != NULL && ast_parallel_join_is_any(node))
            codebuf_write(ctx->out, ", &_pj_any_%u, &_pj_any_res_%u",
                pid, pid);
        codebuf_write(ctx->out, " };\n");
        write_indent(ctx);
        codebuf_write(ctx->out,
            "_pj_hs_%u[_pj_i] = pgy_lane_spawn_dispatch(PGY_LANE_WORKER_POOL, "
            "_pgy_pjoin_%u, &_pj_ctxs_%u[_pj_i]);\n", pid, pid, pid);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");

        if (give_suffix != NULL && ast_parallel_join_is_any(node)) {
            /* R3 slice 2 (docs/182 SS2.1): the slice-1 order-await had a
             * hole -- a loser parked on a channel at an index BEFORE the
             * winner stalled the walk forever even though a decision
             * existed. Repair: wait for the decision first (bounded by
             * the first give; n==0 skips straight to the empty panic),
             * then cancel every handle (queued tasks skip at the pool's
             * pre-run safe point, parked tasks wake through the
             * cancellable channel waits), then await them all exactly
             * once so context lifetimes stay join-bounded. First-give-
             * wins is decided by the CAS, never by this walk. */
            write_indent(ctx);
            codebuf_write(ctx->out,
                "if (_pj_n_%u != 0) while (__atomic_load_n(&_pj_any_%u,"
                " __ATOMIC_ACQUIRE) == 0) sched_yield();\n", pid, pid);
            write_indent(ctx);
            codebuf_write(ctx->out,
                "for (size_t _pj_i = 0; _pj_i < _pj_n_%u; _pj_i++) "
                "pgy_lane_cancel(_pj_hs_%u[_pj_i]);\n", pid, pid);
            write_indent(ctx);
            codebuf_write(ctx->out,
                "for (size_t _pj_i = 0; _pj_i < _pj_n_%u; _pj_i++) "
                "pgy_lane_await(_pj_hs_%u[_pj_i]);\n", pid, pid);
            /* Nothing decided after a full join <=> the fan-out was
             * empty. "First of nothing" is a domain violation, same
             * fail-closed rationale as empty min/max (docs/181 R4). */
            write_indent(ctx);
            codebuf_write(ctx->out,
                "if (__atomic_load_n(&_pj_any_%u, __ATOMIC_ACQUIRE) == 0)"
                " pgy_runtime_panic_out_of_bounds_export("
                "\"any-join over an empty parallel fan-out\");\n", pid);
        } else {
            write_indent(ctx);
            codebuf_write(ctx->out,
                "for (size_t _pj_i = 0; _pj_i < _pj_n_%u; _pj_i++) "
                "pgy_lane_await(_pj_hs_%u[_pj_i]);\n", pid, pid);
        }
        if (give_suffix != NULL && !ast_parallel_join_is_any(node)) {
            const char *rop = ast_parallel_join_reduce_op(node);

            if (rop == NULL) {
                /* Index order, never completion order: the collection walk
                 * is the loop below, not the workers' finish sequence. */
                write_indent(ctx);
                codebuf_write(ctx->out,
                    "for (size_t _pj_i = 0; _pj_i < _pj_n_%u; _pj_i++) "
                    "pgy_array_push_%s(&%s, _pj_ctxs_%u[_pj_i].__join_result);\n",
                    pid, give_suffix, res_name, pid);
            } else {
                /* R4 fold, same index-order rule: a fixed left fold over
                 * the per-task slots keeps Float results deterministic
                 * and byte-equal with the LLVM twin. */
                bool seeded = strcmp(rop, "min") == 0
                           || strcmp(rop, "max") == 0;

                if (seeded) {
                    /* min/max over nothing has no answer; an identity
                     * extreme would leak a fake domain value, so the
                     * empty fan-out fails closed through the same panic
                     * export the LLVM twin calls (docs/181 R4). */
                    write_indent(ctx);
                    codebuf_write(ctx->out,
                        "extern void pgy_runtime_panic_out_of_bounds_export(const char *);\n");
                    write_indent(ctx);
                    codebuf_write(ctx->out,
                        "if (_pj_n_%u == 0) pgy_runtime_panic_out_of_bounds_export("
                        "\"min/max reduce over an empty parallel join range\");\n",
                        pid);
                    write_indent(ctx);
                    codebuf_write(ctx->out,
                        "%s = _pj_ctxs_%u[0].__join_result;\n",
                        res_name, pid);
                }
                write_indent(ctx);
                codebuf_write(ctx->out,
                    "for (size_t _pj_i = %s; _pj_i < _pj_n_%u; _pj_i++) {\n",
                    seeded ? "1" : "0", pid);
                write_indent(ctx);
                codebuf_write(ctx->out,
                    "    %s _pj_v_%u = _pj_ctxs_%u[_pj_i].__join_result;\n",
                    give_c_type, pid, pid);
                join_reduce_write_step(ctx, rop, give_suffix, res_name,
                                       pid);
                write_indent(ctx);
                codebuf_write(ctx->out, "}\n");
            }
        }
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

void
emit_parallel_join_block(ASTNode *node, TranspilerCtx *ctx)
{
    emit_parallel_join_common(node, ctx, NULL, NULL, NULL);
}

char *
transpiler_emit_parallel_join_expr_parts(ASTNode *node, TranspilerCtx *ctx)
{
    const char *give = ast_parallel_join_give_type(node);
    char give_c_type[128];
    char res_name[48];
    CodeBuf *saved_out;
    CodeBuf *tmp;
    int saved_indent;
    char *result;

    if (give == NULL || give[0] == '\0') {
        join_set_error(ctx,
            "parallel block in expression position lacks the checker-sealed give-result fact; the statement form cannot produce a value%s",
            "");
        return NULL;
    }
    if (!transpiler_require_type_name_c_type_copy(ctx, give,
            "parallel join give result", give_c_type, sizeof(give_c_type)))
        return NULL;
    /* The common emitter consumes this id; peeking keeps names aligned. */
    unsigned int res_pid = ctx->parallel_id;
    snprintf(res_name, sizeof(res_name), "_pj_res_%u", res_pid);

    tmp = codebuf_create();
    if (tmp == NULL) {
        join_set_error(ctx,
            "out of memory while lowering parallel join expression%s", "");
        return NULL;
    }
    saved_out = ctx->out;
    saved_indent = ctx->indent;
    ctx->out = tmp;
    ctx->indent = 1;
    emit_parallel_join_common(node, ctx, give, give_c_type, res_name);
    ctx->out = saved_out;
    ctx->indent = saved_indent;
    if (ctx->backend_error != NULL) {
        codebuf_destroy(tmp);
        return NULL;
    }
    {
        const char *rop = ast_parallel_join_reduce_op(node);

        if (rop != NULL) {
            /* R4: one scalar. sum/product start from the identity;
             * min/max are seeded from slot 0 after the empty check (the
             * zero init only quiets may-be-uninitialized). */
            result = strdup_fmt("({ %s %s = %s;\n%s    %s; })",
                give_c_type, res_name,
                strcmp(rop, "product") == 0 ? "1" : "0",
                tmp->data != NULL ? tmp->data : "", res_name);
        } else if (ast_parallel_join_is_any(node)) {
            /* R3: shared decision cell + winner-result cell; the zero
             * init of the result only quiets may-be-uninitialized (it
             * is read only after a decision, and no decision panics). */
            result = strdup_fmt(
                "({ int32_t _pj_any_%u = 0; %s _pj_any_res_%u = 0;\n"
                "%s    _pj_any_res_%u; })",
                res_pid, give_c_type, res_pid,
                tmp->data != NULL ? tmp->data : "", res_pid);
        } else {
            result = strdup_fmt("({ PgyArray_%s %s = {0};\n%s    %s; })",
                give, res_name, tmp->data != NULL ? tmp->data : "",
                res_name);
        }
    }
    codebuf_destroy(tmp);
    return result;
}
