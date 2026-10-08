#!/usr/bin/env bash
# Adequacy gate for the two parallel proof cores.
#
# coq_kernel_check.sh proves the theorems. It cannot tell you the theorems are
# still ABOUT the runtime: the model transcribes specific code, and if that code
# changes the proofs keep compiling while quietly describing something that no
# longer exists. This gate binds each modelled decision to the source line it
# was read from, so a drift shows up here rather than as a silent overclaim.
#
# What is anchored, and why that anchor:
#   - the chunk partition arithmetic, verbatim -- ParallelReductionCore's tiling
#     theorem is a statement about exactly these two expressions;
#   - help-first await (pgy_await consults the queue before parking) -- the
#     hypothesis under which ParallelSchedulingCore proves progress;
#   - the capacity-bounded compensation spare -- queued-work starvation only;
#     dependency cycles with an empty queue remain a modeled deadlock;
#   - the reduce loop bound, which must be the FULL index count and not the
#     chunk count -- an index-order fold is what makes the join worker-count
#     invariant, and folding per chunk instead would need op associative.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

SCHED="docs/semantics/proofs/ParallelSchedulingCore.v"
REDUCE="docs/semantics/proofs/ParallelReductionCore.v"
DOC="docs/semantics/proofs/ParallelModelCores.md"

fail() {
    echo "[parallel-model] $*" >&2
    exit 1
}

require_file() {
    [[ -f "$ROOT_DIR/$1" ]] || fail "missing file: $1"
}

require_text() {
    local rel="$1" text="$2" why="$3"
    grep -Fq -- "$text" "$ROOT_DIR/$rel" ||
        fail "$rel no longer contains \"$text\" -- $why"
}

require_file "$SCHED"
require_file "$REDUCE"
require_file "$DOC"

# ---- the scheduling core states every claim it is cited for ----------------
for term in \
    "Theorem help_first_progress" \
    "Theorem park_only_deadlocks" \
    "Theorem cyclic_await_deadlocks" \
    "Theorem cyclic_await_breaks_spawn_tree" \
    "Theorem help_in_cyclic_wait_self_deadlocks" \
    "Theorem compensation_moves_where_the_others_stick" \
    "Lemma help_first_never_parks_with_work" \
    "Lemma help_first_preserves_queue_runner" \
    "Lemma help_first_preserves_desc_stacks" \
    "Theorem help_first_progress_from_run" \
    "Theorem run_preserves_await_targets" \
    "Theorem run_preserves_worker_bound" \
    "Theorem compensation_at_capacity_is_stuck" \
    "Theorem cyclic_await_deadlocks_under_compensation" \
    "Example help_first_progress_is_not_vacuous"; do
    require_text "$SCHED" "$term" "the scheduling scorecard cites it"
done

# ---- the reduction core states every claim it is cited for -----------------
for term in \
    "Theorem join_schedule_invariant" \
    "Theorem chunk_tiles" \
    "Theorem join_chunk_count_invariant" \
    "Example completion_order_matters" \
    "Example chunk_count_matters" \
    "Corollary index_fold_survives_nonassociative"; do
    require_text "$REDUCE" "$term" "the reduction scorecard cites it"
done

# A progress theorem with contradictory hypotheses proves nothing, and a
# refutation with no counterexample is an assertion. Both are load-bearing.
require_text "$SCHED" "help_first_progress_is_not_vacuous" \
    "without a satisfying instance the progress hypotheses could be inconsistent"

# ---- the model still describes the runtime --------------------------------
CHUNK="src/runtime/pgy_parallel_chunk.h"
require_text "$CHUNK" "(index < remainder ? index : remainder)" \
    "ParallelReductionCore models chunk_lo as base*i + min(i, remainder)"
require_text "$CHUNK" "lo + base + (index < remainder ? 1 : 0)" \
    "ParallelReductionCore models chunk_hi as lo + base + (i < remainder)"

require_text "src/runtime/pgy_parallel_task_ops.h" "pgy_pool_help_run_one" \
    "ParallelSchedulingCore proves progress for the HELP-FIRST await only"
require_text "src/runtime/pgy_parallel_task_ops.h" "pthread_cond_wait" \
    "the model's StPark rule is this fallback; without it the model is wrong"
require_text "src/runtime/pgy_parallel_pool_lifecycle.h" "pgy_pool_spawn_spare_locked" \
    "PolCompensate models this spare worker"
require_text "src/runtime/pgy_parallel_pool_lifecycle.h" "#define PGY_POOL_SPARE_FACTOR 4" \
    "the model's total-worker cap is the default base plus four-times spares"
require_text "src/runtime/pgy_parallel_pool_lifecycle.h" \
    "pool->max_spares = worker_count * PGY_POOL_SPARE_FACTOR;" \
    "spare admission must remain bounded by the fixed base worker count"
require_text "$SCHED" "task_present (mkCfg q (pre ++ WRun (h :: rest) :: post) dn) tg" \
    "a wait on a nonexistent task must not be admitted"

# The fold must run to the index count, never the chunk count: that is exactly
# the difference between join_chunk_count_invariant and chunk_count_matters.
require_text "src/codegen/transpiler_parallel_join_reduce_emit.c" \
    "_pj_i < _pj_n_%u" \
    "an index-order fold is what makes the join worker-count invariant"

echo "[parallel-model] ok (2 proof cores bound to the chunk arithmetic," \
     "help-first await, compensation spare, and index-order fold)"
