#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROOF="$ROOT_DIR/docs/semantics/proofs/MachineLayerCore.v"
DOC="$ROOT_DIR/docs/semantics/proofs/MachineLayerCore.md"

fail() {
    echo "[machine-layer] $*" >&2
    exit 1
}

[[ -f "$PROOF" ]] || fail "missing MachineLayerCore.v"
[[ -f "$DOC" ]] || fail "missing MachineLayerCore.md"

for term in \
    "Record TypeLayout" \
    "Record MachineDeclaration" \
    "Record ContactEvent" \
    "ce_base" \
    "ce_prov" \
    "Definition memory_write" \
    "Definition contact_event_for" \
    "Definition contact_apply" \
    "Inductive contact_step" \
    "Theorem contact_step_constructible" \
    "Theorem sample_plain_read_contact" \
    "Theorem sample_plain_region_rejects_volatile_read" \
    "Theorem sample_revoked_region_rejects_read" \
    "Theorem contact_step_emits_event" \
    "Theorem contact_step_reads_current_value" \
    "Theorem contact_step_volatile_reads_current_value" \
    "Theorem contact_step_writes_value" \
    "Theorem contact_step_volatile_writes_value" \
    "Theorem contact_step_atomic_rmw_reads_before_write" \
    "Theorem contact_step_atomic_rmw_writes_value" \
    "Theorem contact_step_fence_preserves_memory" \
    "Theorem cap_gate_fail_closed" \
    "Theorem revoked_lease_fail_closed" \
    "Theorem contact_mode_fail_closed"; do
    grep -Fq -- "$term" "$PROOF" || fail "proof missing: $term"
done

grep -Fq -- "Actual contact is explicit" "$DOC" ||
    fail "design doc does not state the operation boundary"
grep -Fq -- "does not yet claim refinement to live board/MMU behavior" "$DOC" ||
    fail "design doc overclaims implementation closure"
grep -Fq -- "Research lineage" "$DOC" ||
    fail "design doc is missing implementation references"

if grep -Fq -- "place_guarded" "$PROOF" ||
   grep -Fq -- "place_guarded" "$DOC"; then
    fail "legacy boolean place_guarded path returned"
fi
if grep -Fq -- "MachineContactCore" "$PROOF" ||
   grep -Fq -- "MachineContactCore" "$DOC"; then
    fail "legacy MachineContactCore identity returned"
fi
if grep -Fq -- "??" "$DOC"; then
    fail "MachineLayerCore.md contains encoding corruption"
fi

if command -v rocq >/dev/null 2>&1; then
    (cd "$ROOT_DIR" && rocq compile "$PROOF")
elif command -v coqc >/dev/null 2>&1; then
    (cd "$ROOT_DIR" && coqc "$PROOF")
elif [[ "${PGY_ALLOW_MISSING_COQ:-0}" == "1" ]]; then
    echo "[machine-layer] proof DECLARED SKIP -- no prover (looked for rocq, coqc), PGY_ALLOW_MISSING_COQ=1; the model was NOT checked on this runner."
else
    fail "rocq/coqc is required for the focused machine-layer gate; set PGY_ALLOW_MISSING_COQ=1 only when a dedicated proof job checks the same corpus"
fi

echo "[machine-layer] GREEN -- address evidence, machine state transition, positive/negative witnesses, and no-legacy gate"
