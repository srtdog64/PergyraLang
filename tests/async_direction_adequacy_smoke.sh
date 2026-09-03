#!/usr/bin/env bash
# Adequacy gate for the four docs/204 direction cores:
#   AsyncScopeCore.v, CapabilityFlowCore.v, SuspensionRevalidationCore.v,
#   DeterministicSubsetCore.v.
#
# coq_kernel_check.sh proves the theorems. It cannot tell you the theorems are
# still ABOUT the contract and the runtime: each core transcribes a shape from
# docs/113, src/runtime, or docs/178, and if that shape changes the proofs
# keep compiling while quietly describing something that no longer exists.
# This gate binds every transcribed shape to the line it was read from.
#
# The lifecycle/context cores (AsyncLifecycleCore.v, AsyncContextCore.v) have
# their own gate, tests/async_model_adequacy_smoke.sh. The two gates overlap
# only where the models meet: the runtime context owner.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

SCOPE="docs/semantics/proofs/AsyncScopeCore.v"
CAPS="docs/semantics/proofs/CapabilityFlowCore.v"
SUSP="docs/semantics/proofs/SuspensionRevalidationCore.v"
DET="docs/semantics/proofs/DeterministicSubsetCore.v"
DOC="docs/semantics/proofs/AsyncDirectionCores.md"

fail() {
    echo "[async-direction] $*" >&2
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

reject_text() {
    local rel="$1" text="$2" why="$3"
    if grep -Fq -- "$text" "$ROOT_DIR/$rel"; then
        fail "$rel still contains forbidden stale claim \"$text\" -- $why"
    fi
}

for f in "$SCOPE" "$CAPS" "$SUSP" "$DET" "$DOC"; do
    require_file "$f"
done

# ---- each core states every claim it is cited for -------------------------
for term in \
    "Theorem contained_no_orphan" \
    "Theorem no_running_task_in_closed_scope" \
    "Theorem run_no_orphan" \
    "Theorem cancel_reaches_descendants" \
    "Theorem background_only_via_detach" \
    "Theorem orphan_reachable_unstructured" \
    "Theorem structured_run_exists"; do
    require_text "$SCOPE" "$term" "the scope scorecard cites it"
done

for term in \
    "Theorem run_bounded" \
    "Theorem child_within_parent" \
    "Theorem loan_uniquely_held" \
    "Theorem return_restores" \
    "Theorem lend_then_return_round_trip" \
    "Theorem tls_default_forges" \
    "Corollary structured_spawn_cannot_forge"; do
    require_text "$CAPS" "$term" "the capability scorecard cites it"
done

for term in \
    "Theorem gen_monotone" \
    "Theorem despawn_advances" \
    "Theorem resolve_sound" \
    "Theorem stale_never_resolves" \
    "Theorem resolved_means_same_incarnation" \
    "Theorem unchecked_deref_hits_new_occupant" \
    "Theorem revalidation_not_vacuous"; do
    require_text "$SUSP" "$term" "the suspension scorecard cites it"
done

for term in \
    "Theorem commute" \
    "Theorem run_permutation" \
    "Theorem deterministic_subset" \
    "Theorem footprints_not_vacuous" \
    "Example write_conflict_is_schedule_dependent" \
    "Example conflicting_pair_not_independent"; do
    require_text "$DET" "$term" "the determinism scorecard cites it"
done

# A refutation with no counterexample is an assertion; a discipline with no
# satisfying run is vacuous. Both halves are load-bearing in every core.
require_text "$SCOPE" "unstructured_step" \
    "the contract-text refutation needs the unstructured relation"
require_text "$CAPS" "tls_step" \
    "the executor-default refutation needs the tls relation"
require_text "$DET" "every equality between states is stated pointwise" \
    "no function extensionality: the axiom budget stays at SlotCalculus's two Parameters"

# ---- the models still describe the contract and the runtime ---------------
# AsyncScopeCore's unstructured relation is THIS sentence of the contract.
require_text "docs/113_memory_concurrency_model.md" \
    "must be retired on every normal path" \
    "AsyncScopeCore's close guard is this rule lifted from one handle to a scope tree"
require_text "docs/113_memory_concurrency_model.md" \
    "retains its own block join contract" \
    "AsyncScopeCore generalises parallel's join-before-continuation to every scope"
require_text "docs/113_memory_concurrency_model.md" \
    "a lifetime structure" \
    "docs/204 section 2.1: scope constructs own lifetime, async stays a suspension marker"
# The dormant runtime skeleton the structured scope will consume.
require_text "src/runtime/async/async_scope.h" "AsyncScopeWaitAll" \
    "AsyncScopeCore's close rule is this join"

# CapabilityFlowCore: the per-thread binding is the tls_step refutation's
# world; the parent-capture owner is the structured world. Both must exist,
# because the theorem set speaks about both.
require_text "src/runtime/pgy_runtime_context.h" \
    "_Thread_local PgyRuntimeContext *g_pgy_runtime_context_current" \
    "tls_step models an executor reading this binding without parent capture"
require_text "src/runtime/pgy_runtime_context.h" \
    "pgy_runtime_context_capture_task" \
    "child_within_parent and run_bounded describe a spawn that captures the parent"
require_text "src/runtime/pgy_parallel_spawn.h" \
    "pgy_runtime_context_capture_task" \
    "the spawn path must capture the parent; without it tls_default_forges describes the tree"
require_text "src/runtime/pgy_runtime_capability.h" "PGY_CAP_ALL" \
    "CapabilityFlowCore's default_all is this mask"

# The counterexamples describe forbidden or historical relations, never the
# current implementation. Keep that distinction executable so a proof comment
# cannot silently reverse the live owner facts.
reject_text "$SCOPE" "the current contract admits an orphan" \
    "the current Future lifecycle checker rejects live handles at scope exit"
reject_text "$SCOPE" "a still-live named future is not rejected" \
    "that sentence described the pre-cf66092b rule only"
reject_text "$CAPS" "the CURRENT runtime" \
    "the current runtime captures the parent task context"
reject_text "$CAPS" "no spawn path propagates it" \
    "all current task carriers call pgy_runtime_context_capture_task"
reject_text "docs/204_concurrency_direction_pscc_review.md" \
    "§3.5 — 지금은 나르지" \
    "exact parent-context carriage is landed; only evidence-specific edges remain directional"
require_text "$CAPS" "Split is owned by Disjointness" \
    "CapabilityFlowCore must not absorb docs/178's split evidence"
require_text "$DOC" "Split is outside this core" \
    "the companion boundary must expose the omitted split discipline"

# SuspensionRevalidationCore models the generational handle.
require_text "src/runtime/slot_manager.h" "generation" \
    "SuspensionRevalidationCore's SlotRef carries this field"
require_text "src/runtime/slot_manager.h" "PgyPinnedView" \
    "SuspensionRevalidationCore's LiveRef is this view"

# DeterministicSubsetCore composes with the index-order fold.
require_text "docs/semantics/proofs/ParallelReductionCore.v" \
    "Theorem join_schedule_invariant" \
    "DeterministicSubsetCore leaves the reduce leg to this theorem"
require_text "docs/178_parallel_boundary_evidence.md" "Disjointness" \
    "DeterministicSubsetCore's independent is this evidence at footprint granularity"

# ---- the companion doc names every core and keeps its claim bounded --------
for name in AsyncScopeCore CapabilityFlowCore SuspensionRevalidationCore DeterministicSubsetCore; do
    require_text "$DOC" "$name" "the companion doc must describe every core"
done
for boundary in \
    "add zero axioms" \
    "not implementation verification"; do
    require_text "$DOC" "$boundary" "the model claim must remain explicitly bounded"
done

echo "[async-direction] ok (4 proof cores bound to docs/113, the AsyncScope skeleton," \
     "the runtime context owner, the generational handle, and the index-order fold)"
