#!/usr/bin/env bash

set -euo pipefail

if ! command -v grep >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

SCRIPT_PATH="${BASH_SOURCE[0]}"
SCRIPT_DIR="$(cd "${SCRIPT_PATH%/*}" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

fail() {
    echo "[ownership-relocation-cleanup-contract] FAIL: $*" >&2
    exit 1
}

require_text() {
    local path="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$path" ||
        fail "$path is missing contract term: $term"
}

forbid_text() {
    local path="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$path"; then
        fail "$path conflates an orthogonal capability with ownership class: $term"
    fi
}

# The positioning SoT must keep transfer, executor demand, physical relocation,
# and cleanup obligation distinct. This is a wording ratchet, not executable
# evidence for arbitrary !Move/!Forget support.
for term in \
    "ownership transfer" \
    "executor relocation demand" \
    "physical address relocation" \
    "cleanup/abandonment obligation" \
    'MOVE_ONLY` therefore does not mean Rust-style `!Move' \
    '`requires_movability` is a site demand' \
    "orthogonal capability" \
    "installed self-host path also does not yet reach" \
    "full C/LLVM self-host parity gate is therefore red" \
    "function-exit must-await rule" \
    "Unknown required facts fail closed"; do
    require_text "docs/106_ownership_model_comparison.md" "$term"
done

for term in \
    "not a general must-await or guaranteed-finalizer rule" \
    'structured `parallel` owns join-before-continuation'; do
    require_text "docs/113_memory_concurrency_model.md" "$term"
done

for term in \
    "not a stable-address or immovable allocation" \
    "not a pin lease" \
    "not a guaranteed-finalizer handle" \
    "semantic capability facts" \
    "transfer success"; do
    require_text "docs/56_tobject_boundary_snapshot_policy.md" "$term"
done

# OwnershipTypeClass owns transfer/borrow policy. Relocation and mandatory
# cleanup may later live under the same semantic authority, but must not be
# smuggled in as enum members without their own fact and consumers.
require_text "src/semantic/type_checker_ownership_internal.h" \
    "transfer/borrow classifier"
require_text "src/semantic/type_checker_ownership_internal.h" \
    "orthogonal fact under the same semantic ownership"
for term in \
    "OWNERSHIP_TYPE_RELOCATABLE" \
    "OWNERSHIP_TYPE_IMMOVABLE" \
    "OWNERSHIP_TYPE_MUST_FINALIZE" \
    "OWNERSHIP_TYPE_FORGETTABLE"; do
    forbid_text "src/semantic/type_checker_ownership_internal.h" "$term"
done

# The executable fact remains explicitly site-level in both native and
# self-host owners. The rows name bounded contradiction cases; execution is
# owned by the parity gate, whose current LLVM failure remains documented.
require_text "src/compiler/execution_lane.h" \
    "It is not a Rust-style type relocation capability"
require_text "src/compiler/execution_lane.c" \
    "if (e->requires_movability)"
require_text "src/compiler/execution_lane.c" \
    "return PGY_LANE_REJECT;"
require_text "src/self_hosted/sea/execution_lane.pgy" \
    "negative_pin_requires_movability"
require_text "src/self_hosted/sea/execution_lane.pgy" \
    "negative_raw_slot_requires_movability"
require_text "src/self_hosted/sea/execution_lane.pgy" \
    "negative_parallel_raw_channel_requires_movability"
require_text "docs/146_sea_execution_lanes.md" \
    "The full gate is currently red"

echo "[ownership-relocation-cleanup-contract] PASS"
