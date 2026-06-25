#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[proof-spine] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -e "$ROOT_DIR/$rel" ]] || fail "missing file: $rel"
}

require_text() {
    local rel="$1"
    local text="$2"
    grep -Fq -- "$text" "$ROOT_DIR/$rel" ||
        fail "$rel missing text: $text"
}

for rel in \
    docs/semantics/proofs/SlotCalculus.v \
    docs/semantics/proofs/AxisOwnership.v \
    docs/semantics/proofs/IntentStepSoundness.v \
    docs/semantics/proofs/IRMinimality.v \
    docs/semantics/proofs/WitnessDataRace.v \
    docs/semantics/proofs/CheckedArith.v \
    docs/semantics/proofs/ZoneCrossingCore.v \
    docs/semantics/proofs/EffectAuthorityCore.v \
    docs/semantics/proofs/SlotLifecycleCore.v \
    docs/semantics/proofs/AuthorityDelegationCore.v \
    docs/semantics/proofs/UnifiedCore.v \
    docs/semantics/proofs/CompensationCore.v \
    docs/semantics/proofs/CoordinationCore.v \
    docs/semantics/proofs/ProofCarryingIR.v \
    docs/semantics/proofs/VerificationMethodology.v \
    docs/semantics/proofs/ProofSpine.v \
    docs/semantics/proofs/ProofSpine.md \
    docs/semantics/README.md \
    docs/semantics/16_language_contract_golden_spine.md \
    tests/formal_semantics_smoke.sh; do
    require_file "$rel"
done

require_text "docs/semantics/proofs/SlotCalculus.v" "Lemma pin_non_eviction"
require_text "docs/semantics/proofs/AxisOwnership.v" "Theorem ownership_unique"
require_text "docs/semantics/proofs/IntentStepSoundness.v" "Theorem intent_step_preservation"
require_text "docs/semantics/proofs/IRMinimality.v" "Theorem air_is_minimal_witness_set"
require_text "docs/semantics/proofs/WitnessDataRace.v" "Theorem well_typed_data_race_free"
require_text "docs/semantics/proofs/CheckedArith.v" "Theorem div_none_iff"
require_text "docs/semantics/proofs/ZoneCrossingCore.v" "Theorem fail_closed_crossing"
require_text "docs/semantics/proofs/EffectAuthorityCore.v" "Theorem fail_closed_emit"
require_text "docs/semantics/proofs/SlotLifecycleCore.v" "Theorem no_op_after_release"
require_text "docs/semantics/proofs/AuthorityDelegationCore.v" "Theorem no_privilege_escalation"
require_text "docs/semantics/proofs/UnifiedCore.v" "Theorem authority_conservation"
require_text "docs/semantics/proofs/CompensationCore.v" "Theorem do_then_rollback_restores"
require_text "docs/semantics/proofs/CoordinationCore.v" "Theorem reachable_dep_closed"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "Theorem valid_certificate_allows_backend_consumption"
require_text "docs/semantics/proofs/VerificationMethodology.v" "Theorem hard_self_host_requires_differential"

for term in \
    "Inductive ProofNode" \
    "Inductive SpineClaim" \
    "Inductive RemainingObligation" \
    "Definition ProofSpineComplete" \
    "Definition PermitsClaim" \
    "Definition WholeLanguageVerificationReady" \
    "Theorem complete_spine_has_node" \
    "Theorem complete_spine_connects_runtime_safety" \
    "Theorem complete_spine_connects_axis_ownership" \
    "Theorem complete_spine_connects_intent_core" \
    "Theorem complete_spine_connects_unified_machine" \
    "Theorem complete_spine_connects_certificate_pipeline" \
    "Theorem complete_spine_connects_methodology" \
    "Theorem complete_spine_is_not_whole_language_verification" \
    "Theorem whole_language_ready_requires_pin_exceptional_cleanup" \
    "Theorem whole_language_ready_requires_parser_to_ast_manifest" \
    "Theorem whole_language_ready_requires_behavior_judgment_map" \
    "Theorem whole_language_ready_requires_transitive_frontier_scheduler" \
    "Theorem whole_language_ready_requires_air_mir_live_owner_binding" \
    "Theorem whole_language_ready_requires_windows_llvm_runner_parity" \
    "Theorem open_obligation_blocks_whole_language_ready"; do
    require_text "docs/semantics/proofs/ProofSpine.v" "$term"
done

require_text "docs/semantics/proofs/ProofSpine.md" "complete proof spine != whole-language verification"
require_text "docs/semantics/proofs/ProofSpine.md" "ObligationPinExceptionalCleanup"
require_text "docs/semantics/proofs/ProofSpine.md" "DropOnce / ReleaseAfterUnpin"
require_text "docs/semantics/proofs/ProofSpine.md" "ObligationParserToAstManifest"
require_text "docs/semantics/proofs/ProofSpine.md" "ObligationBehaviorJudgmentDiagnosticMap"
require_text "docs/semantics/proofs/ProofSpine.md" "ObligationTransitiveFrontierScheduler"
require_text "docs/semantics/proofs/ProofSpine.md" "ObligationAirMirLiveOwnerFactBinding"
require_text "docs/semantics/proofs/ProofSpine.md" "live AIR/MIR owner facts"
require_text "docs/semantics/proofs/ProofSpine.md" "ObligationWindowsLlvmRunnerParity"
require_text "docs/semantics/README.md" "proofs/ProofSpine.v"
require_text "docs/semantics/16_language_contract_golden_spine.md" "Proof spine"
require_text "tests/formal_semantics_smoke.sh" "docs/semantics/proofs/ProofSpine.v"

if command -v coqc >/dev/null 2>&1; then
    coq_timeout="${PGY_COQ_SMOKE_TIMEOUT_SECONDS:-60}"
    if command -v timeout >/dev/null 2>&1; then
        (cd "$ROOT_DIR" && timeout "$coq_timeout" coqc docs/semantics/proofs/ProofSpine.v)
    else
        (cd "$ROOT_DIR" && coqc docs/semantics/proofs/ProofSpine.v)
    fi
    echo "[proof-spine] Coq spine ok"
else
    echo "[proof-spine] Coq spine skipped (coqc not found)"
fi

echo "[proof-spine] proof pack spine ok"
