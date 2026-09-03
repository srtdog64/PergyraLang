#!/usr/bin/env bash
# Bind the bounded Rocq async lifecycle/context models to their live owners.
# coq_kernel_check.sh proves the theorems; this gate prevents those theorems
# from silently describing code that no longer exists.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

LIFECYCLE_PROOF="docs/semantics/proofs/AsyncLifecycleCore.v"
CONTEXT_PROOF="docs/semantics/proofs/AsyncContextCore.v"
MODEL_DOC="docs/semantics/proofs/AsyncModelCores.md"
LIFECYCLE_OWNER="src/semantic/type_checker_future_lifecycle.c"
STATE_CARRIER="src/semantic/symbol_table.h"
FLOW_MERGE="src/semantic/type_checker_flow_resource_merge.c"
AWAIT_OWNER="src/semantic/type_checker_expr.c"
CANCEL_OWNER="src/semantic/type_checker_builtins_stdlib_body.c"
CONTEXT_OWNER="src/runtime/pgy_runtime_context.h"

fail() {
    echo "[async-model] $*" >&2
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

for required in \
    "$LIFECYCLE_PROOF" "$CONTEXT_PROOF" "$MODEL_DOC" \
    "$LIFECYCLE_OWNER" "$STATE_CARRIER" "$FLOW_MERGE" \
    "$AWAIT_OWNER" "$CANCEL_OWNER" "$CONTEXT_OWNER"; do
    require_file "$required"
done

# ---- every externally cited lifecycle result is still a theorem ------------
for term in \
    "Theorem suspend_preserves_lifetime" \
    "Theorem suspend_does_not_discharge_live" \
    "Theorem cancel_is_request_only" \
    "Theorem cancel_cannot_close_scope" \
    "Theorem await_consumes_exactly_one_live_handle" \
    "Theorem own_transfer_consumes_exactly_one_live_handle" \
    "Theorem retirement_requires_await_or_transfer" \
    "Theorem retired_handle_cannot_be_consumed_again" \
    "Theorem live_trace_to_closed_scope_has_retirement" \
    "Theorem alternative_path_disagreement_fails_closed" \
    "Theorem parallel_retirement_contributes" \
    "Example alternative_and_parallel_merges_are_distinct"; do
    require_text "$LIFECYCLE_PROOF" "$term" \
        "the structured-lifecycle proof boundary cites it"
done

# ---- every externally cited context result is still a theorem --------------
for term in \
    "Theorem capture_preserves_exact_authority" \
    "Theorem capture_preserves_exact_budget_owner" \
    "Theorem capture_preserves_instance_identity" \
    "Theorem lane_resume_preserves_context" \
    "Theorem lane_resume_cannot_widen_masks" \
    "Theorem suspension_resume_preserves_authority_and_budget" \
    "Theorem task_return_restores_surrounding_context" \
    "Example executor_default_can_change_authority_identity"; do
    require_text "$CONTEXT_PROOF" "$term" \
        "the runtime-context proof boundary cites it"
done

# ---- lifecycle model still describes semantic admission --------------------
for state in \
    PGY_FUTURE_LIFECYCLE_NONE \
    PGY_FUTURE_LIFECYCLE_LIVE \
    PGY_FUTURE_LIFECYCLE_RETIRED \
    PGY_FUTURE_LIFECYCLE_DIVERGED; do
    require_text "$STATE_CARRIER" "$state" \
        "AsyncLifecycleCore models this exact state family"
done

require_text "$LIFECYCLE_OWNER" \
    "symbol->future_lifecycle_state = PGY_FUTURE_LIFECYCLE_LIVE;" \
    "spawn/own-parameter admission must create a live obligation"
require_text "$LIFECYCLE_OWNER" \
    "symbol->future_lifecycle_state = PGY_FUTURE_LIFECYCLE_RETIRED;" \
    "await/explicit own transfer must retire the obligation"
require_text "$LIFECYCLE_OWNER" \
    "source handle is not live on every incoming path" \
    "a second or path-diverged transfer must fail closed"
require_text "$LIFECYCLE_OWNER" \
    "the handle is still live" \
    "scope exit must reject a live obligation"
require_text "$LIFECYCLE_OWNER" \
    "only some normal paths joined or transferred the handle" \
    "scope exit must reject a diverged obligation"
require_text "$AWAIT_OWNER" "semantic_future_complete(awaited_sym);" \
    "await is the direct completion consumer"

cancel_block="$(awk '
    /case STDLIB_BODY_CANCEL:/ { inside = 1 }
    /case STDLIB_BODY_IS_CANCELLED:/ { inside = 0 }
    inside { print }
' "$ROOT_DIR/$CANCEL_OWNER")"
[[ -n "$cancel_block" ]] || fail "could not locate the Cancel semantic block"
grep -Fq "return TYPE_BOOL;" <<<"$cancel_block" ||
    fail "Cancel no longer returns only its request status"
if grep -Fq "semantic_future_complete" <<<"$cancel_block"; then
    fail "Cancel silently became a join/retirement operation"
fi

require_text "$FLOW_MERGE" \
    "merge_resource_states(dst, src, false);" \
    "alternative CFG paths must use agreement/fail-closed merge"
require_text "$FLOW_MERGE" \
    "merge_resource_states(dst, src, true);" \
    "simultaneous parallel arms must use their distinct join merge"
require_text "$FLOW_MERGE" \
    "return PGY_FUTURE_LIFECYCLE_DIVERGED;" \
    "alternative lifecycle disagreement must fail closed"

# ---- context model still describes task capture and execution ---------------
for assignment in \
    "task_context->capabilities = parent->capabilities;" \
    "task_context->budget_owner = parent->budget_owner;" \
    "task_context->instance_id = parent->instance_id;"; do
    require_text "$CONTEXT_OWNER" "$assignment" \
        "AsyncContextCore requires exact parent context capture"
done

for carrier in \
    src/runtime/pgy_parallel.h \
    src/runtime/pgy_parallel_spawn.h \
    src/runtime/pgy_parallel_blocking.h \
    src/runtime/pgy_parallel_coroutine.h \
    src/runtime/pgy_runtime_lib_mn_exports.h; do
    require_text "$carrier" "pgy_runtime_context_capture_task" \
        "every task carrier must use the parent-capture owner"
    if grep -Fq "pgy_runtime_context_init(" "$ROOT_DIR/$carrier"; then
        fail "$carrier reopened executor environment grants or a fresh budget"
    fi
done

require_text "src/runtime/pgy_parallel.h" \
    "pgy_runtime_context_bind(previous_context)" \
    "task return must restore the surrounding context"
require_text "src/runtime/pgy_parallel_coroutine.h" \
    "pgy_runtime_context_bind(&current->runtime_context)" \
    "coroutine yield must rebind the captured task context"
require_text "src/runtime/pgy_parallel_task_ops.h" \
    "pgy_runtime_context_bind(&current->runtime_context)" \
    "coroutine await must rebind the captured task context"

for boundary in \
    "Neither core proves termination" \
    "whole-language verification" \
    "Both add zero assumptions"; do
    require_text "$MODEL_DOC" "$boundary" \
        "the model claim must remain explicitly bounded"
done

echo "[async-model] ok (lifecycle containment and runtime-context carriage" \
     "proofs remain bound to their live owners)"
