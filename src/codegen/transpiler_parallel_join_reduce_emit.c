/*
 * Copyright (c) 2026 Pergyra Language Project
 * Post-fan-out half of the C join emitter (docs/182 SS5): join walk +
 * expression-form result materialization. Split from
 * transpiler_parallel_join_emit.c after R3/R4 grew this half past the
 * fan-out mechanics it shared a file with.
 */

#include "transpiler_parallel_join_reduce_emit.h"

#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_context.h"

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
    /* Duration gives are Long-backed (docs/181 SS2.3): the i32 checked
     * lane would silently truncate them. */
    bool long_lane = strcmp(give_name, "Long") == 0
                  || strcmp(give_name, "Duration") == 0;

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

void
transpiler_pjoin_emit_join_await(ASTNode *node, TranspilerCtx *ctx,
                                 const char *give_suffix,
                                 unsigned int pid)
{
    if (give_suffix != NULL && ast_parallel_join_is_any(node)) {
        /* R3 slice 2 (docs/182 SS2.1): the slice-1 order-await had a
         * hole -- a loser parked on a channel at an index BEFORE the
         * winner stalled the walk forever even though a decision
         * existed. Repair: wait for the decision first (bounded by
         * the first give; n==0 skips straight to the empty panic),
         * then cancel every handle (queued tasks skip at the pool's
         * pre-run safe point, parked tasks wake through the
         * cancellable channel waits, compute-loop tasks retire at the
         * slice-3 back-edge safe points), then await them all exactly
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
        return;
    }
    write_indent(ctx);
    codebuf_write(ctx->out,
        "for (size_t _pj_i = 0; _pj_i < _pj_n_%u; _pj_i++) "
        "pgy_lane_await(_pj_hs_%u[_pj_i]);\n", pid, pid);
}

void
transpiler_pjoin_emit_result_materialize(ASTNode *node,
                                         TranspilerCtx *ctx,
                                         const char *give_suffix,
                                         const char *give_c_type,
                                         const char *res_name,
                                         unsigned int pid)
{
    const char *rop;

    if (give_suffix == NULL || ast_parallel_join_is_any(node))
        return;
    rop = ast_parallel_join_reduce_op(node);
    if (rop == NULL) {
        /* Index order, never completion order: the collection walk
         * is the loop below, not the workers' finish sequence. */
        write_indent(ctx);
        codebuf_write(ctx->out,
            "for (size_t _pj_i = 0; _pj_i < _pj_n_%u; _pj_i++) "
            "pgy_array_push_%s(&%s, _pj_ctxs_%u[_pj_i].__join_result);\n",
            pid, give_suffix, res_name, pid);
        return;
    }
    /* R4 fold, same index-order rule: a fixed left fold over the
     * per-task slots keeps Float results deterministic and byte-equal
     * with the LLVM twin. */
    {
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
        join_reduce_write_step(ctx, rop, give_suffix, res_name, pid);
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}
