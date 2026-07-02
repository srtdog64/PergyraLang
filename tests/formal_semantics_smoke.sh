#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/beta_checklist_shards.sh"

require_file() {
    local path="$1"
    local label="$2"

    if [[ ! -e "$path" ]]; then
        echo "missing $label" >&2
        exit 1
    fi
}

require_term() {
    local path="$1"
    local label="$2"
    local term="$3"

    if [[ "$path" == "$CHECKLIST_PATH" ]]; then
        if ! pgy_beta_checklist_contains "$term"; then
            echo "$label shards missing required term: $term" >&2
            exit 1
        fi
        return 0
    fi

    if ! grep -Fq -- "$term" "$path"; then
        echo "$label missing required term: $term" >&2
        exit 1
    fi
}

forbid_term() {
    local path="$1"
    local label="$2"
    local term="$3"

    if grep -Fq -- "$term" "$path"; then
        echo "$label contains forbidden term: $term" >&2
        exit 1
    fi
}

require_terms() {
    local path="$1"
    local label="$2"
    local term

    while IFS= read -r term; do
        [[ -z "$term" ]] && continue
        require_term "$path" "$label" "$term"
    done
}

INDEX_PATH="$ROOT_DIR/docs/102_formal_semantics_and_proof_obligations.md"
PROOF_DIR="$ROOT_DIR/docs/semantics"
RIGOR_AUDIT_PATH="$ROOT_DIR/docs/118_slot_model_rigor_audit.md"
CHECKLIST_PATH="$ROOT_DIR/docs/100_beta_readiness_checklist.md"
SEMANTIC_DESIGN_PATH="$ROOT_DIR/docs/14_semantic_analyzer_design.md"
SLOT_MANAGER_PATH="$ROOT_DIR/src/runtime/slot_manager.h"
SLOT_MACROS_PATH="$ROOT_DIR/src/runtime/pgy_runtime_slot_macros.h"
TODO_PATH="$ROOT_DIR/TODO.md"
CI_PATH="$ROOT_DIR/.github/workflows/ci.yml"
README_PATH="$ROOT_DIR/README.md"
SLOT_COQ="$PROOF_DIR/proofs/SlotCalculus.v"
AXIS_COQ="$PROOF_DIR/proofs/AxisOwnership.v"
WITNESS_COQ="$PROOF_DIR/proofs/WitnessDataRace.v"
METHODOLOGY_COQ="$PROOF_DIR/proofs/VerificationMethodology.v"
PROOF_SPINE_COQ="$PROOF_DIR/proofs/ProofSpine.v"
GUARD_COQ="$PROOF_DIR/proofs/GuardCalculus.v"
WHOLE_PROGRAM_COQ="$PROOF_DIR/proofs/WholeProgramCore.v"
AIR_BINDING_COQ="$PROOF_DIR/proofs/AIRBinding.v"
MINIMAL_POSITION_DOC="$PROOF_DIR/20_minimal_verification_position.md"
WITNESS_DOC="$PROOF_DIR/10_ability_witness_evidence.md"
BOUNDARY_WITNESS_HEADER="$ROOT_DIR/src/semantic/boundary_witness.h"
BOUNDARY_WITNESS_SOURCE="$ROOT_DIR/src/semantic/boundary_witness.c"
LOSS_CONTRACT_PATH="$PROOF_DIR/09_abstraction_loss_contracts.md"
EFFECT_SOURCE_PATH="$ROOT_DIR/src/semantic/type_effects.c"
EFFECT_FLOW_PATH="$ROOT_DIR/src/semantic/type_checker_flow_effects.c"
EFFECT_TEST_A_PATH="$ROOT_DIR/src/tests/semantic/test_semantic_effects_part_a_1.cases.h"
EFFECT_TEST_B_PATH="$ROOT_DIR/src/tests/semantic/test_semantic_effects_part_b_2.cases.h"
B0_PROVENANCE_TEST_PATH="$ROOT_DIR/src/tests/semantic/test_semantic_b0_provenance.cases.h"
BUILTINS_STDLIB_BODY_PATH="$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_body.c"
FUNC_DECL_PATH="$ROOT_DIR/src/semantic/type_checker_func_decl.c"

require_file "$INDEX_PATH" "docs/102_formal_semantics_and_proof_obligations.md"
require_file "$PROOF_DIR" "docs/semantics proof folder"
require_file "$RIGOR_AUDIT_PATH" "docs/118_slot_model_rigor_audit.md"
require_file "$CHECKLIST_PATH" "docs/100_beta_readiness_checklist.md"
require_file "$SEMANTIC_DESIGN_PATH" "docs/14_semantic_analyzer_design.md"
require_file "$SLOT_MANAGER_PATH" "src/runtime/slot_manager.h"
require_file "$SLOT_MACROS_PATH" "src/runtime/pgy_runtime_slot_macros.h"
require_file "$TODO_PATH" "TODO.md"
require_file "$CI_PATH" ".github/workflows/ci.yml"
require_file "$README_PATH" "README.md"
require_file "$SLOT_COQ" "docs/semantics/proofs/SlotCalculus.v"
require_file "$AXIS_COQ" "docs/semantics/proofs/AxisOwnership.v"
require_file "$WITNESS_COQ" "docs/semantics/proofs/WitnessDataRace.v"
require_file "$METHODOLOGY_COQ" "docs/semantics/proofs/VerificationMethodology.v"
require_file "$PROOF_SPINE_COQ" "docs/semantics/proofs/ProofSpine.v"
require_file "$GUARD_COQ" "docs/semantics/proofs/GuardCalculus.v"
require_file "$WHOLE_PROGRAM_COQ" "docs/semantics/proofs/WholeProgramCore.v"
require_file "$AIR_BINDING_COQ" "docs/semantics/proofs/AIRBinding.v"
require_file "$MINIMAL_POSITION_DOC" "docs/semantics/20_minimal_verification_position.md"
require_file "$WITNESS_DOC" "docs/semantics/10_ability_witness_evidence.md"
require_file "$BOUNDARY_WITNESS_HEADER" "src/semantic/boundary_witness.h"
require_file "$BOUNDARY_WITNESS_SOURCE" "src/semantic/boundary_witness.c"
require_file "$LOSS_CONTRACT_PATH" "docs/semantics/09_abstraction_loss_contracts.md"
require_file "$EFFECT_SOURCE_PATH" "src/semantic/type_effects.c"
require_file "$EFFECT_FLOW_PATH" "src/semantic/type_checker_flow_effects.c"
require_file "$EFFECT_TEST_A_PATH" "src/tests/semantic/test_semantic_effects_part_a_1.cases.h"
require_file "$EFFECT_TEST_B_PATH" "src/tests/semantic/test_semantic_effects_part_b_2.cases.h"
require_file "$B0_PROVENANCE_TEST_PATH" "src/tests/semantic/test_semantic_b0_provenance.cases.h"
require_file "$BUILTINS_STDLIB_BODY_PATH" "src/semantic/type_checker_builtins_stdlib_body.c"
require_file "$FUNC_DECL_PATH" "src/semantic/type_checker_func_decl.c"

require_terms "$PROOF_DIR/README.md" "docs/semantics/README.md" <<'TERMS'
Status: `beta-proof-obligation`
Every stable beta feature must be represented in this folder
Stable proof scope:
Out of beta proof scope:
Regression tests, smoke tests, and backend compare runs are proof evidence, not proof itself.
Borrow-checker-equivalent safety: only through the combined ownership
Slot alone is not advertised as a borrow checker.
08_slot_capability_calculus.md
09_abstraction_loss_contracts.md
proofs/SlotCalculus.v
proofs/VerificationMethodology.v
proofs/ProofSpine.v
mechanized evidence for those modeled invariants only
Generic contracts
Ownership: anchored slot-handle boundary subset only.
Runtime observability
Backends: MIR-equivalent C and LLVM behavior
AIR abstraction safety
Slot capability calculus
Abstraction loss contracts
Full quantum resource model.
Higher-kinded types and full FP functor/applicative/monad laws.
GPU/Spray, Skia/render graph
TERMS

require_terms "$LOSS_CONTRACT_PATH" "docs/semantics/09_abstraction_loss_contracts.md" <<'TERMS'
Status: `beta-proof-obligation`
Stable surface: compiler and tooling abstraction boundaries.
Loss is not automatically a bug. Hidden loss is the bug.
An abstraction loss contract has seven fields:
Loss budget classes:
`consumer forbidden_to_recover fact from source`
### AST To HIR/DIR/RIR/MIR
### MIR To AIR
### MIR To C And LLVM
### Self-Hosted Tool To C Oracle
## Theorem: Loss Visibility
## Theorem: Preservation Carry
## Theorem: Bounded Approximation Soundness
That syntax is a design sketch only.
TERMS

require_terms "$PROOF_DIR/00_proof_contract.md" "docs/semantics/00_proof_contract.md" <<'TERMS'
## Semantic Domains
## Core Judgments
## Static Boundary vs Runtime Existence
static rejection  = unsafe transition across a known boundary
runtime validate  = dynamic existence/state of a resource handle
static proofs cover unsafe transitions; runtime proofs cover dynamic
### Type Preservation
### Progress
### Failure Separation
### Backend Observational Equivalence
`PanicState`
TERMS

require_terms "$PROOF_DIR/01_intent_world_zone.md" "docs/semantics/01_intent_world_zone.md" <<'TERMS'
Keywords: `intent`, `world`, `zone`, `subject`, `authority`, `handoff`.
## Theorem: Authority Soundness
## Theorem: Intent Step Progress
## Theorem: World/Zone Frontier Termination
TERMS

require_terms "$PROOF_DIR/02_relation_effect_projection.md" "docs/semantics/02_relation_effect_projection.md" <<'TERMS'
Keywords: `relation`, `effect`, `projection`, `refresh`, `publish`, `bind`.
## Theorem: Projection Freshness
## Theorem: Effect Conflict Soundness
Beta-stable partial order contract:
closure        = collapse >= nondeterministic
join(a, b)     = closure(a) union closure(b)
meet(a, b)     = closure(a) intersection closure(b)
authority      = secure requires authority provenance
resource edge  = secure | remote | collapse touch a resource boundary
conflict       = secure conflicts with remote | collapse | nondeterministic
type_effect_mask_conflicts
## Theorem: Projection Diagnostic Completeness
TERMS

require_terms "$EFFECT_SOURCE_PATH" "src/semantic/type_effects.c" <<'TERMS'
type_effect_mask_closure
type_effect_mask_join
type_effect_mask_meet
type_effect_mask_requires_authority
type_effect_mask_touches_resource_boundary
type_effect_mask_conflicts
EFFECT_COLLAPSE
EFFECT_NONDETERMINISTIC
EFFECT_SECURE
EFFECT_REMOTE
TERMS

require_terms "$EFFECT_FLOW_PATH" "src/semantic/type_checker_flow_effects.c" <<'TERMS'
semantic_warning_with_hints
PGY_CODE_SEM_EFFECT_CONFLICT
PGY_CAUSE_EFFECT_INCOMPATIBLE_COMBO
PGY_FIX_SPLIT_EFFECT_FAMILIES
type_effect_mask_conflicts
Control-flow branch/join combines conflicting effect classes
Reason:
Fix:
TERMS

require_terms "$EFFECT_TEST_A_PATH" "src/tests/semantic/test_semantic_effects_part_a_1.cases.h" <<'TERMS'
effect-partial-order: collapse is a superset of nondeterministic by closure
effect-partial-order: secure and remote are incomparable
effect-partial-order: joined secure|remote is a superset of each side
type_effect_mask_requires_authority
type_effect_mask_touches_resource_boundary
type_effect_mask_conflicts
TERMS

require_terms "$EFFECT_TEST_B_PATH" "src/tests/semantic/test_semantic_effects_part_b_2.cases.h" <<'TERMS'
effect-partial-order: disjoint branch effects join into combined contract
effect-conflict: secure and remote combination emits warning
effect-conflict: secure and collapse combination emits warning
effect-conflict: non-adjacent branch combination still emits warning
TERMS

require_terms "$B0_PROVENANCE_TEST_PATH" "src/tests/semantic/test_semantic_b0_provenance.cases.h" <<'TERMS'
branch effect conflict warning reports reason and fix
if branch effect conflict warning reports branch provenance
then branch contributes 'secure'
else branch contributes 'remote'
TERMS

forbid_term "$BUILTINS_STDLIB_BODY_PATH" "src/semantic/type_checker_builtins_stdlib_body.c" "EFFECT_NONDETERMINISTIC | EFFECT_COLLAPSE"
forbid_term "$FUNC_DECL_PATH" "src/semantic/type_checker_func_decl.c" "semantic_warning_code(ctx, PGY_CODE_SEM_EFFECT_CONFLICT"

require_terms "$PROOF_DIR/03_generics_modules_dag.md" "docs/semantics/03_generics_modules_dag.md" <<'TERMS'
Keywords and surfaces: `where`, `ability`, generic parameters, default type arguments, module imports/exports, type-resolution DAG.
## Theorem: Generic Contract Soundness
## Theorem: DAG Soundness
## Theorem: Module Visibility Non-Interference
TERMS

require_terms "$PROOF_DIR/04_ownership_abi.md" "docs/semantics/04_ownership_abi.md" <<'TERMS'
Keywords and surfaces: `own`, `ref`, anchored slot handles, slot boundaries, runtime ABI ownership.
## Theorem Boundary: Slot Runtime Safety Is Not Borrow Safety
`Slot runtime safety`
`Borrow-checker-equivalent safety`
## Theorem: Anchored Ownership Safety
## Theorem: Secure Token Unforgeability
## Theorem: Authority Transfer Single-Owner
## Theorem: Arena Lifetime Non-Escape
## Theorem: ABI Ownership Parity
TERMS

require_terms "$PROOF_DIR/05_parallel_execution.md" "docs/semantics/05_parallel_execution.md" <<'TERMS'
Keywords: `parallel`
## Theorem: Parallel Conflict Soundness
## Theorem: Execution Backend Parity
TERMS

require_terms "$WITNESS_COQ" "docs/semantics/proofs/WitnessDataRace.v" <<'TERMS'
Definition op_guard
OpAcqW
OpAcqR
well_typed_data_race_free
read-write
TERMS

require_terms "$GUARD_COQ" "docs/semantics/proofs/GuardCalculus.v" <<'TERMS'
Theorem no_silent_ub
Theorem coverage_is_local
Theorem guarded_more_permissive_at_equal_safety
Corollary pergyra_no_silent_ub
Proven,    true => UB
OpSecureToken
OpLifecycle
TERMS

require_terms "$WHOLE_PROGRAM_COQ" "docs/semantics/proofs/WholeProgramCore.v" <<'TERMS'
Theorem step_requires_guard
Theorem guard_enables_step
Theorem step_iff_guard
Theorem step_preserves_wf
Theorem whole_program_safety
| ActRun (t : task)
Qed.
TERMS

require_terms "$AIR_BINDING_COQ" "docs/semantics/proofs/AIRBinding.v" <<'TERMS'
Record AIRFacts
Theorem guard_air_faithful
Theorem gate_locality
Theorem delegate_use_release_air_independent
air_zone_gate
air_effect_gate
air_acquire_gate
air_comp_targets
air_dep_graph
TERMS

require_terms "$MINIMAL_POSITION_DOC" "docs/semantics/20_minimal_verification_position.md" <<'TERMS'
UB-Completeness As The Proof Obligation
Status: `beta-proof-obligation`
no_silent_ub
coverage_is_local
guarded_more_permissive_at_equal_safety
The Honest Ledger
amortized-cost
never "zero-cost"
an abort, never
Rust As Shipped Is Already Hybrid
phrase this as "Rust-equivalent safety"
unfilled row is exactly the `Unhandled` verdict
TERMS

require_terms "$WITNESS_DOC" "docs/semantics/10_ability_witness_evidence.md" <<'TERMS'
Boundary Witness Refinement Gate
src/semantic/boundary_witness.{h,c}
PgyBoundaryWitnessSummary
ResourceConsumeSnapshot
read/write overlap
whole-C-program proof
TERMS

require_terms "$BOUNDARY_WITNESS_HEADER" "src/semantic/boundary_witness.h" <<'TERMS'
PgyBoundaryWitnessSummary
pgy_boundary_witness_guard_accepts
semantic_boundary_witness_record_acq_read
semantic_boundary_witness_record_acq_write
TERMS

require_terms "$PROOF_DIR/06_backend_parity.md" "docs/semantics/06_backend_parity.md" <<'TERMS'
Surfaces: MIR, declaration inventory, C backend, LLVM backend, runtime ABI.
## Theorem: MIR Source-of-Truth
## Theorem: Backend Observational Equivalence
## Theorem: Runtime Panic Parity
## Theorem: Structured Backend Failure
TERMS

require_terms "$PROOF_DIR/07_air_abstraction_safety.md" "docs/semantics/07_air_abstraction_safety.md" <<'TERMS'
Stable surface: AIR (Abstraction Intent Representation)
## Theorem: AIR Synthesis Read-Only
## Theorem: Intent Node Coverage
## Theorem: Boundary Closure
## Theorem: Drift Detection Soundness
## Theorem: Codegen Non-Impact
PGY_SEM_INTENT_BOUNDARY_DRIFT
TERMS

require_terms "$PROOF_DIR/08_slot_capability_calculus.md" "docs/semantics/08_slot_capability_calculus.md" <<'TERMS'
Status: `IN PROGRESS / PROOF-SKETCH`
## Stable Surface
## Negative Claim: Slot Is Not A Borrow Checker
Slot = runtime capability + generation + token + pin-state safety.
borrow-checker-equivalent is the static ownership/CFG layer above Slot.
static rejection  = unsafe transition across a known boundary
runtime validate  = dynamic existence/state of a resource handle
Runtime validation covers generation freshness
the ownership/CFG/AIR layers prove which boundary transitions are accepted
## Semantic Domains
## Theorem: ABA Safety
## Theorem: Token Unforgeability
## Theorem: Pin Non-Eviction
## Bridge Obligation: Borrow-Checker-Equivalent Safety
NoEscape(view, region)
NoSuspend(view, region)
WriteExclusive(slot, region)
not completed mechanized proof
Source-level `pin slot as view: ReadView<T>|WriteView<T> { ... }` now reaches
HIR and MIR as explicit pin-region metadata
pin-unpin-cleanup-edge
PgyPinnedSlotView_*
PgyPinnedSecureSlotView_*
pgy_pin_read_*
pgy_pin_write_*
pgy_unpin_*
TERMS

require_terms "$CHECKLIST_PATH" "docs/100_beta_readiness_checklist.md" <<'TERMS'
docs/102_formal_semantics_and_proof_obligations.md
docs/semantics/
Do not advertise mechanized proof for beta
Do not advertise "Slot as borrow checker"
Canonical semantic split
runtime validation covers dynamic existence/state
TERMS

require_terms "$TODO_PATH" "TODO.md" <<'TERMS'
docs/102_formal_semantics_and_proof_obligations.md
docs/semantics/
proof evidence
beta proof line honest
TERMS

require_terms "$INDEX_PATH" "docs/102_formal_semantics_and_proof_obligations.md" <<'TERMS'
docs/semantics/README.md
docs/semantics/08_slot_capability_calculus.md
docs/semantics/09_abstraction_loss_contracts.md
docs/semantics/proofs/SlotCalculus.v
docs/118_slot_model_rigor_audit.md
TERMS

forbid_term "$SLOT_COQ" "docs/semantics/proofs/SlotCalculus.v" "Status: Beta"
forbid_term "$SLOT_COQ" "docs/semantics/proofs/SlotCalculus.v" "Level 4 Mechanized Proof"
forbid_term "$SLOT_COQ" "docs/semantics/proofs/SlotCalculus.v" "Safe Core Mechanized Proof"

require_terms "$SLOT_COQ" "docs/semantics/proofs/SlotCalculus.v" <<'TERMS'
Status: proof-sketch; not beta-closure evidence unless checked by CI
Negative scope: this file does not prove Rust-style borrow checking
Require Import Coq.Arith.PeanoNat.
Lemma stale_handle_read_impossible
Lemma handle_read_requires_issued_token
Lemma unissued_token_read_impossible
Lemma handle_pin_requires_issued_token
Lemma unissued_token_pin_impossible
Definition FreshSlotId
Lemma zero_slot_id_claim_impossible
Lemma max_slot_id_claim_impossible
Lemma tampered_view_unpin_impossible
Lemma double_unpin_impossible
Lemma released_slot_read_impossible
Lemma released_slot_write_impossible
Lemma released_slot_pin_impossible
Lemma released_slot_release_impossible
Lemma pin_non_eviction
Qed.
TERMS

require_terms "$AXIS_COQ" "docs/semantics/proofs/AxisOwnership.v" <<'TERMS'
Target: docs/42 Keyword Orthogonality -- Axis Fact-Ownership
Theorem ownership_unique
Lemma owner_of
Definition AxisUpdate
Theorem axis_updates_commute
Theorem axis_update_idempotent
Definition keyword_axis
Definition keyword_fact
Theorem keyword_axis_sound
Remaining obligations
TERMS

require_terms "$METHODOLOGY_COQ" "docs/semantics/proofs/VerificationMethodology.v" <<'TERMS'
Pergyra Verification Methodology Core
Inductive Method
Inductive Claim
Definition permits
Theorem golden_only_not_model_soundness
Theorem golden_only_not_hard_self_host_slice
Theorem smoke_only_not_hard_self_host_slice
Theorem mechanized_model_not_implementation_parity
Theorem differential_not_model_soundness
Theorem hard_self_host_requires_differential
Theorem hard_self_host_requires_verifier
Theorem layout_niche_requires_typestate
Theorem materialization_requires_trace_and_capability
Theorem verifier_with_owner_permits_fact_consumption
TERMS

require_terms "$PROOF_SPINE_COQ" "docs/semantics/proofs/ProofSpine.v" <<'TERMS'
Pergyra Proof Spine
Inductive ProofNode
Inductive SpineClaim
Definition ProofSpineComplete
Definition PermitsClaim
Theorem complete_spine_has_node
Theorem complete_spine_connects_runtime_safety
Theorem complete_spine_connects_unified_machine
Theorem complete_spine_connects_certificate_pipeline
Theorem complete_spine_connects_methodology
Theorem complete_spine_is_not_whole_language_verification
Inductive RemainingObligation
Theorem whole_language_ready_requires_pin_exceptional_cleanup
Theorem whole_language_ready_requires_parser_to_ast_manifest
Theorem whole_language_ready_requires_behavior_judgment_map
Theorem whole_language_ready_requires_transitive_frontier_scheduler
Theorem whole_language_ready_requires_air_mir_live_owner_binding
Theorem whole_language_ready_requires_windows_llvm_runner_parity
Theorem open_obligation_blocks_whole_language_ready
TERMS

for stale_doc in \
    "$PROOF_DIR/00_proof_contract.md" \
    "$PROOF_DIR/08_slot_capability_calculus.md" \
    "$PROOF_DIR/README.md"; do
    forbid_term "$stale_doc" "$stale_doc" "proof sketch for one invariant"
    forbid_term "$stale_doc" "$stale_doc" "proof sketch for two invariants"
    forbid_term "$stale_doc" "$stale_doc" "minimal mechanized sketch for two invariants"
done

require_terms "$SEMANTIC_DESIGN_PATH" "docs/14_semantic_analyzer_design.md" <<'TERMS'
Slot Resource-Boundary Analyzer
Slot analysis is not Rust-style lifetime analysis.
Slot is the source-level modular resource boundary.
TERMS

require_terms "$SLOT_MANAGER_PATH" "src/runtime/slot_manager.h" <<'TERMS'
source-level resource boundary
pointer/address ownership
backend
TERMS

require_terms "$SLOT_MACROS_PATH" "src/runtime/pgy_runtime_slot_macros.h" <<'TERMS'
source-level resource boundary
pointer/address ownership
backend
TERMS

require_terms "$RIGOR_AUDIT_PATH" "docs/118_slot_model_rigor_audit.md" <<'TERMS'
Slot Is Not a Borrow Checker
Slot Is A Modular Resource Boundary
Pergyra does not expose memory as address ownership.
Pergyra exposes memory as a modular resource boundary.
A Slot is the stable language-level boundary; the backend handle below it is replaceable.
Slot = address abstraction + ownership boundary + capability gate + replaceable backend handle
The borrow-checker-equivalent in Pergyra is not Slot
Current `WriteView<T>` same-slot exclusivity is enforced
source-level typed-view pin blocks reject suspension and transport boundaries
Active source-level typed-view pin blocks; `docs/74`; diagnostics + backend compare
The block-scoped source `pin` surface is active for typed views.
pin_read_view_block
pin_secure_read_view_block
pin_mixed_read_view_sequence
pin_write_view_block
pin_secure_write_view_block
straight-line typed-view read/write parity across
The comparison to Rust 1.0 at launch is a scope-bounded analogy
TERMS

for path in "$README_PATH" "$TODO_PATH"; do
    forbid_term "$path" "$path" "Slot Lifetime Analyzer"
    forbid_term "$path" "$path" "slot???"
    forbid_term "$path" "$path" "Slot is Pergyra's borrow checker"
    forbid_term "$path" "$path" "Slot proves borrow safety"
    forbid_term "$path" "$path" "Slot proves Rust-style borrow checking"
    forbid_term "$path" "$path" "Pergyra provides Rust-level memory safety"
    forbid_term "$path" "$path" "Pergyra guarantees Rust-level memory safety"
    forbid_term "$path" "$path" "pin blocks statically reject crossing await"
    forbid_term "$path" "$path" "WriteView<T> exclusive is not enforced"
done

global_forbid_terms="$(mktemp)"
global_forbid_files="$(mktemp)"
global_forbid_matches="$(mktemp)"
cat >"$global_forbid_terms" <<'TERMS'
Slot Lifetime Analyzer
slot???
Slot is Pergyra's borrow checker
Slot proves borrow safety
Slot proves Rust-style borrow checking
Pergyra provides Rust-level memory safety
Pergyra guarantees Rust-level memory safety
pin blocks reject crossing await
pin blocks statically reject crossing await
WriteView<T> exclusive is not enforced
WriteView<T> is not enforced
TERMS

while IFS= read -r -d '' path; do
    case "$path" in
        "$CHECKLIST_PATH"|"$RIGOR_AUDIT_PATH"|"$PROOF_DIR/08_slot_capability_calculus.md"|"$PROOF_DIR/README.md")
            continue
            ;;
    esac
    printf '%s\0' "$path" >>"$global_forbid_files"
done < <(find "$ROOT_DIR/docs" -name '*.md' -print0)

if [[ -s "$global_forbid_files" ]] &&
    xargs -0 grep -nF -f "$global_forbid_terms" -- \
        <"$global_forbid_files" >"$global_forbid_matches"; then
    cat "$global_forbid_matches" >&2
    echo "docs contain forbidden Slot safety claim(s)" >&2
    exit 1
fi

require_terms "$CI_PATH" ".github/workflows/ci.yml" <<'TERMS'
sudo apt-get install -y gcc make llvm-dev llvm coq
make ci-linux
TERMS

echo "formal semantics smoke: ok"

if command -v coqc >/dev/null 2>&1; then
    coq_timeout="${PGY_COQ_SMOKE_TIMEOUT_SECONDS:-60}"
    for coq_proof in \
        docs/semantics/proofs/SlotCalculus.v \
        docs/semantics/proofs/AxisOwnership.v \
        docs/semantics/proofs/IntentStepSoundness.v \
        docs/semantics/proofs/IRMinimality.v \
        docs/semantics/proofs/WitnessDataRace.v \
        docs/semantics/proofs/CheckedArith.v \
        docs/semantics/proofs/ProofCarryingIR.v \
        docs/semantics/proofs/ZoneCrossingCore.v \
        docs/semantics/proofs/EffectAuthorityCore.v \
        docs/semantics/proofs/SlotLifecycleCore.v \
        docs/semantics/proofs/AuthorityDelegationCore.v \
        docs/semantics/proofs/UnifiedCore.v \
        docs/semantics/proofs/CompensationCore.v \
        docs/semantics/proofs/CoordinationCore.v \
        docs/semantics/proofs/VerificationMethodology.v \
        docs/semantics/proofs/ProofSpine.v; do
        if command -v timeout >/dev/null 2>&1; then
            (cd "$ROOT_DIR" && timeout "$coq_timeout" coqc "$coq_proof")
        else
            (cd "$ROOT_DIR" && coqc "$coq_proof")
        fi
    done
    echo "formal semantics Coq smoke: ok"
else
    echo "formal semantics Coq smoke: skipped (coqc not found)"
fi
