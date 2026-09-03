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
CI_PATH="$ROOT_DIR/.github/workflows/platform_full.yml"
README_PATH="$ROOT_DIR/README.md"
SLOT_COQ="$PROOF_DIR/proofs/SlotCalculus.v"
AXIS_COQ="$PROOF_DIR/proofs/AxisOwnership.v"
WITNESS_COQ="$PROOF_DIR/proofs/WitnessDataRace.v"
METHODOLOGY_COQ="$PROOF_DIR/proofs/VerificationMethodology.v"
PROOF_SPINE_COQ="$PROOF_DIR/proofs/ProofSpine.v"
DELEGATION_BOUNDARY_COQ="$PROOF_DIR/proofs/DelegationBoundaryCore.v"
LOSS_COMPOSITION_COQ="$PROOF_DIR/proofs/LossCompositionCore.v"
EVIDENCE_LIFECYCLE_COQ="$PROOF_DIR/proofs/EvidenceLifecycleCore.v"
EVIDENCE_LIFECYCLE_DOC="$PROOF_DIR/proofs/EvidenceLifecycleCore.md"
RESOURCE_MACHINE_BRIDGE_COQ="$PROOF_DIR/proofs/ResourceMachineBridge.v"
ARCHITECTURE_BOUNDARY_DOC="$PROOF_DIR/proofs/ArchitectureBoundaryCores.md"
SOT_AUTHORITY_COQ="$PROOF_DIR/proofs/SoTAuthority.v"
GUARD_COQ="$PROOF_DIR/proofs/GuardCalculus.v"
WHOLE_PROGRAM_COQ="$PROOF_DIR/proofs/WholeProgramCore.v"
AIR_BINDING_COQ="$PROOF_DIR/proofs/AIRBinding.v"
FORMAL_KERNEL_COQ="$PROOF_DIR/proofs/FormalKernel.v"
BASIS_COMPLETENESS_COQ="$PROOF_DIR/proofs/BasisCompleteness.v"
INTENT_OBLIGATIONS_COQ="$PROOF_DIR/proofs/IntentObligations.v"
INTENT_SPINE_COQ="$PROOF_DIR/proofs/IntentSpine.v"
INTENT_CONFLICT_COQ="$PROOF_DIR/proofs/IntentConflict.v"
AUTHORITY_IRREDUCIBILITY_COQ="$PROOF_DIR/proofs/AuthorityIrreducibility.v"
ASYNC_LIFECYCLE_COQ="$PROOF_DIR/proofs/AsyncLifecycleCore.v"
ASYNC_CONTEXT_COQ="$PROOF_DIR/proofs/AsyncContextCore.v"
ASYNC_MODEL_DOC="$PROOF_DIR/proofs/AsyncModelCores.md"
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
require_file "$CI_PATH" ".github/workflows/platform_full.yml"
require_file "$README_PATH" "README.md"
require_file "$SLOT_COQ" "docs/semantics/proofs/SlotCalculus.v"
require_file "$AXIS_COQ" "docs/semantics/proofs/AxisOwnership.v"
require_file "$WITNESS_COQ" "docs/semantics/proofs/WitnessDataRace.v"
require_file "$METHODOLOGY_COQ" "docs/semantics/proofs/VerificationMethodology.v"
require_file "$PROOF_SPINE_COQ" "docs/semantics/proofs/ProofSpine.v"
require_file "$DELEGATION_BOUNDARY_COQ" "docs/semantics/proofs/DelegationBoundaryCore.v"
require_file "$LOSS_COMPOSITION_COQ" "docs/semantics/proofs/LossCompositionCore.v"
require_file "$EVIDENCE_LIFECYCLE_COQ" "docs/semantics/proofs/EvidenceLifecycleCore.v"
require_file "$EVIDENCE_LIFECYCLE_DOC" "docs/semantics/proofs/EvidenceLifecycleCore.md"
require_file "$RESOURCE_MACHINE_BRIDGE_COQ" "docs/semantics/proofs/ResourceMachineBridge.v"
require_file "$ARCHITECTURE_BOUNDARY_DOC" "docs/semantics/proofs/ArchitectureBoundaryCores.md"
require_file "$SOT_AUTHORITY_COQ" "docs/semantics/proofs/SoTAuthority.v"
require_file "$GUARD_COQ" "docs/semantics/proofs/GuardCalculus.v"
require_file "$WHOLE_PROGRAM_COQ" "docs/semantics/proofs/WholeProgramCore.v"
require_file "$AIR_BINDING_COQ" "docs/semantics/proofs/AIRBinding.v"
require_file "$FORMAL_KERNEL_COQ" "docs/semantics/proofs/FormalKernel.v"
require_file "$BASIS_COMPLETENESS_COQ" "docs/semantics/proofs/BasisCompleteness.v"
require_file "$INTENT_OBLIGATIONS_COQ" "docs/semantics/proofs/IntentObligations.v"
require_file "$INTENT_SPINE_COQ" "docs/semantics/proofs/IntentSpine.v"
require_file "$INTENT_CONFLICT_COQ" "docs/semantics/proofs/IntentConflict.v"
require_file "$AUTHORITY_IRREDUCIBILITY_COQ" "docs/semantics/proofs/AuthorityIrreducibility.v"
require_file "$ASYNC_LIFECYCLE_COQ" "docs/semantics/proofs/AsyncLifecycleCore.v"
require_file "$ASYNC_CONTEXT_COQ" "docs/semantics/proofs/AsyncContextCore.v"
require_file "$ASYNC_MODEL_DOC" "docs/semantics/proofs/AsyncModelCores.md"
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
proofs/SoTAuthority.v
sot_owner_spine_registry.md
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
Evidence strength is not interchangeable:
## Loss Composition
## Derived Mechanism Boundary
`consumer forbidden_to_recover fact from source`
### AST To HIR/DIR/RIR/MIR
### MIR To AIR
### MIR To C And LLVM
### Self-Hosted Tool To C Oracle
## Executable Invariant: Loss Visibility
## Executable Invariant: Preservation Carry
## Proof Obligation: Bounded Approximation Visibility
That syntax is a design sketch only.
TERMS

require_terms "$EVIDENCE_LIFECYCLE_COQ" "docs/semantics/proofs/EvidenceLifecycleCore.v" <<'TERMS'
Inductive EvidenceClass
Inductive EvidenceDisposition
Definition project_evidence
Definition ReceiptJustified
Theorem missing_admission_fails_closed
Theorem identity_reference_carries_authority
Theorem validity_summarizes_to_receipt
Theorem construction_erasure_preserves_established_authority
Theorem construction_erasure_requires_discharge_and_last_consumer
Theorem materialization_requires_explicit_runtime_need
Theorem receipt_justified_iff_redecision_when_authority_required
Theorem no_redecision_means_no_receipt_justification
Theorem justified_validity_receipt_carries_authority
Theorem compression_step_transitive
Theorem compression_trace_nonincreasing
Theorem admitted_interpretation_does_not_reopen
TERMS

require_terms "$EVIDENCE_LIFECYCLE_DOC" "docs/semantics/proofs/EvidenceLifecycleCore.md" <<'TERMS'
## Objective card
not a second authority
does not close any SoT registry row
Semantic entropy
representation_units
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

require_terms "$FORMAL_KERNEL_COQ" "docs/semantics/proofs/FormalKernel.v" <<'TERMS'
Pergyra Formal Semantics -- Formal Kernel Vocabulary Binding
Inductive SourceKeyword
Inductive KernelPrimitive
Inductive KernelFact
Definition keyword_meaning
Definition InterpretsByPrimitive
Definition InterpretsByFact
Theorem every_keyword_has_kernel_meaning
Theorem intent_decomposes_to_coordination_and_compensation
Theorem world_zone_share_boundary_kernel
Theorem authority_effect_not_aliases
Theorem projection_is_loss_budgeted
Theorem no_keyword_permits_whole_language_claim
TERMS

require_terms "$BASIS_COMPLETENESS_COQ" "docs/semantics/proofs/BasisCompleteness.v" <<'TERMS'
Target: docs/172 M2
Record Bigraph
Record AxisConfig
Definition encode
Theorem encode_wf
Theorem decode_encode
Theorem encode_decode
Theorem world_separation
Corollary cross_world_needs_channel
Example direct_cross_world_illformed
TERMS

require_terms "$INTENT_OBLIGATIONS_COQ" "docs/semantics/proofs/IntentObligations.v" <<'TERMS'
Pergyra Intent Obligation Unit Correction
Inductive IntentBucket
Inductive VerifierFamily
Inductive IntentSubfact
Definition bucket_claim
Definition subfact_bucket
Definition all_verifier_families_emitted
Theorem intent_binder_emits_all_verifier_families
Theorem verifier_families_are_nonexpressibility_units
Theorem purpose_trace_outside_nonexpressibility_claim
Theorem verifier_subfacts_are_claim_units
Theorem purpose_trace_subfacts_outside_claim
Theorem intent_binder_inherits_verifier_family_strength
Theorem no_atomic_intent_fact
Theorem int0_precedes_participant_declared_used
TERMS

require_terms "$INTENT_SPINE_COQ" "docs/semantics/proofs/IntentSpine.v" <<'TERMS'
Target: docs/173 SS0-b + INT-5 -- the intent fact kernel.
Record ParticipantFact
Record CoordinationFact
Record CompensationFact
Record IntentSpine
Definition participants_covered
Definition deps_wf
Definition comp_covered
Definition intent_checked
Theorem checked_intent_guard_free
Theorem no_dep_cycle
Theorem facts_share_spine
Theorem one_intent_from_facts
Theorem intent_determined_by_facts
Theorem library_bucket_obligation_free
Corollary checked_intent_erasable
TERMS

require_terms "$INTENT_CONFLICT_COQ" "docs/semantics/proofs/IntentConflict.v" <<'TERMS'
Target: docs/173 INT-4 -- the cross-intent conflict kernel.
Record IntentDecl
Definition overlap
Definition conflict_guard
Definition sep_when_active
Inductive trace_ok
Inductive no_conflict_fires
Theorem separated_trace_conflict_free
Example conflict_guard_real
Example priority_waives_only_one_order
TERMS

require_terms "$AUTHORITY_IRREDUCIBILITY_COQ" "docs/semantics/proofs/AuthorityIrreducibility.v" <<'TERMS'
Target: docs/semantics/22 SS1.5
Record Config
Inductive reach
Definition authorized
Theorem delegation_distinguishes
Theorem authority_beyond_cap_zone
TERMS

# The machine-layer corner (docs/19): Region is address evidence; contact_step
# is the explicit authority/lifetime/mode-gated machine state transition/event.
MACHINE_LAYER_COQ="$PROOF_DIR/proofs/MachineLayerCore.v"
require_file "$MACHINE_LAYER_COQ" "docs/semantics/proofs/MachineLayerCore.v"
require_terms "$MACHINE_LAYER_COQ" "docs/semantics/proofs/MachineLayerCore.v" <<'TERMS'
Theorem grant_yields_valid_region
Theorem carve_preserves_validity
Theorem carve_disjoint
Record TypeLayout
Theorem place_grounds_slot
Theorem place_preserves_layout_identity
Theorem chain_grant_carve_place_grounded
Theorem no_wild_slot
Theorem place_rejects_volatile
Theorem place_oversize_fail_closed
Theorem placed_slots_disjoint
Theorem declared_grant_id_unique
Theorem declared_grant_hardware_adequate
Theorem declared_grants_nonoverlap
Theorem declared_grant_address_bounded
Theorem valid_region_address_bounded
Theorem valid_region_has_declared_hardware_adequacy
Inductive ContactOp
Definition contact_mode_allowed
Inductive LeaseState
Record ContactEvent
ce_base
ce_prov
Record ContactConfig
Definition memory_write
Definition contact_event_for
Definition contact_apply
Definition contact_has_cap
Definition contact_lease_live
Definition revoke_contact_lease
Theorem revoke_contact_lease_revokes
Inductive contact_step
Theorem contact_step_constructible
Definition sample_grant
Definition sample_declaration
Theorem sample_plain_read_contact
Theorem contact_step_requires_valid_region
Theorem contact_step_requires_hardware_adequacy
Theorem contact_step_requires_capability
Theorem contact_step_requires_live_lease
Theorem contact_step_requires_mode
Theorem volatile_contact_requires_volatile
Theorem atomic_contact_requires_atomic
Theorem contact_step_preserves_authority
Theorem contact_step_preserves_lease
Theorem contact_step_emits_event
Theorem contact_step_reads_current_value
Theorem contact_step_volatile_reads_current_value
Theorem contact_step_writes_value
Theorem contact_step_volatile_writes_value
Theorem contact_step_atomic_rmw_reads_before_write
Theorem contact_step_atomic_rmw_writes_value
Theorem contact_step_fence_preserves_memory
Theorem cap_gate_fail_closed
Theorem revoked_lease_fail_closed
Theorem contact_mode_fail_closed
Theorem sample_plain_region_rejects_volatile_read
Theorem sample_revoked_region_rejects_read
Theorem no_ambient_machine_contact
0 admits / 0 axioms
Negative scope
TERMS
forbid_term "$MACHINE_LAYER_COQ" "docs/semantics/proofs/MachineLayerCore.v" "place_guarded"
forbid_term "$MACHINE_LAYER_COQ" "docs/semantics/proofs/MachineLayerCore.v" "MachineContactCore"
forbid_term "$MACHINE_LAYER_COQ" "docs/semantics/proofs/MachineLayerCore.v" "without the metal capability no region operation produces a slot"
require_terms "$PROOF_DIR/proofs/MachineLayerCore.md" "docs/semantics/proofs/MachineLayerCore.md" <<'TERMS'
Address evidence
Actual contact is explicit
contact_step
MachineDeclaration
contact_step_emits_event
revoked_lease_fail_closed
does not yet claim refinement to live board/MMU behavior
Research lineage
TERMS
forbid_term "$PROOF_DIR/proofs/MachineLayerCore.md" "docs/semantics/proofs/MachineLayerCore.md" "??"
forbid_term "$PROOF_DIR/proofs/MachineLayerCore.md" "docs/semantics/proofs/MachineLayerCore.md" "MachineContactCore"

require_terms "$DELEGATION_BOUNDARY_COQ" "docs/semantics/proofs/DelegationBoundaryCore.v" <<'TERMS'
Pergyra Formal Semantics -- Delegation Boundary Core
Inductive Delegability
Record SourceDeclaration
Record EnforcementEvidence
Inductive AutomatedPermit
Theorem automated_permit_requires_complete_mediation
Theorem automated_permit_requires_delegable_judgment
Theorem runtime_permit_requires_retained_guard
Theorem missing_evidence_never_permits_automation
Theorem human_required_blocks_automated_permit
Theorem non_delegable_blocks_automated_permit
Theorem authorization_does_not_imply_delegability
Theorem declared_purpose_does_not_establish_actual_purpose
0 admits / 0 axioms
Negative scope
TERMS

require_terms "$LOSS_COMPOSITION_COQ" "docs/semantics/proofs/LossCompositionCore.v" <<'TERMS'
Pergyra Formal Semantics -- Loss Composition Core
Inductive DerivedMechanism
Record LossVector
Definition loss_compose
Definition PathBudgetAllows
Theorem derived_mechanism_requires_observational_equivalence
Theorem derived_mechanism_requires_cost_budget
Theorem loss_compose_associative
Theorem composed_budget_implies_each_component_bounded
Theorem local_budgets_do_not_imply_path_budget
0 admits / 0 axioms
TERMS

require_terms "$RESOURCE_MACHINE_BRIDGE_COQ" "docs/semantics/proofs/ResourceMachineBridge.v" <<'TERMS'
Pergyra Formal Semantics -- Resource/Machine Bridge
Record ResourceGrant
Record MachinePlacement
Record ProjectionBinding
Inductive GroundedContact
Theorem grounded_contact_requires_resource_authority
Theorem grounded_contact_requires_machine_evidence
Theorem grounded_contact_requires_explicit_projection
Theorem resource_identity_does_not_determine_machine_address
Theorem machine_address_does_not_determine_resource_authority
0 admits / 0 axioms
TERMS

require_terms "$ARCHITECTURE_BOUNDARY_DOC" "docs/semantics/proofs/ArchitectureBoundaryCores.md" <<'TERMS'
attributed declaration
resource id does not establish an address.
An address does not establish
A local loss budget does not establish a path budget.
mechanized model soundness != implementation adequacy
complete proof spine != whole-language verification
TERMS

OPTION_TRY_COQ="$PROOF_DIR/proofs/OptionTry.v"
require_file "$OPTION_TRY_COQ" "docs/semantics/proofs/OptionTry.v"
require_terms "$OPTION_TRY_COQ" "docs/semantics/proofs/OptionTry.v" <<'TERMS'
Theorem try_ritual_equiv
Theorem try_none_propagates
Theorem try_bind_assoc
Theorem try_result_err_carries
0 admits / 0 axioms
TERMS

GENERIC_CARRIAGE_COQ="$PROOF_DIR/proofs/GenericAxisCarriage.v"
require_file "$GENERIC_CARRIAGE_COQ" "docs/semantics/proofs/GenericAxisCarriage.v"
require_terms "$GENERIC_CARRIAGE_COQ" "docs/semantics/proofs/GenericAxisCarriage.v" <<'TERMS'
Target: docs/151
Theorem carriage_monotone
Theorem descent_is_declared
Theorem erase_declared_scope
Theorem carriage_no_conjuring
Corollary hiding_requires_declaration
Negative scope: this file proves the carriage LAW, not the carriage MODE.
0 admits / 0 axioms
TERMS

# WO-F1 (a): reading confluence -- complete axis readings converge.
READING_CONFLUENCE_COQ="$PROOF_DIR/proofs/ReadingConfluence.v"
require_file "$READING_CONFLUENCE_COQ" "docs/semantics/proofs/ReadingConfluence.v"
require_terms "$READING_CONFLUENCE_COQ" "docs/semantics/proofs/ReadingConfluence.v" <<'TERMS'
Theorem reading_reads_owner
Theorem reading_silent_iff_owner_missing
Theorem read_order_irrelevant
Corollary complete_reading_total
Theorem incomplete_readings_can_disagree
0 admits / 0 axioms
Negative scope
TERMS

# WO-F1 (b): binary adequacy -- the two-valued surface verdict decides
# exactly the calculus guard (accept iff guard, reject iff not-guard).
BINARY_ADEQUACY_COQ="$PROOF_DIR/proofs/BinaryAdequacy.v"
require_file "$BINARY_ADEQUACY_COQ" "docs/semantics/proofs/BinaryAdequacy.v"
require_terms "$BINARY_ADEQUACY_COQ" "docs/semantics/proofs/BinaryAdequacy.v" <<'TERMS'
Theorem accept_adequate
Corollary reject_adequate
Corollary guard_decidable
Corollary verdict_is_binary
0 admits / 0 axioms
Negative scope
TERMS

# GuardCalculus <-> implementation binding (docs/155 SS3): each op class's
# fail-close is witnessed by a NAMED runtime panic class, and the SAME
# vocabulary must exist in the runtime registry -- renaming either side
# turns this smoke RED (the model<->code correspondence cannot drift
# silently). This locks vocabulary, not guard-firing correctness (that is
# the failclosed gate fixtures' job).
GUARD_WITNESS_COQ="$PROOF_DIR/proofs/GuardWitnessBinding.v"
PANIC_CONTRACT_PATH="$ROOT_DIR/src/runtime/pgy_runtime_panic_contract.h"
require_file "$GUARD_WITNESS_COQ" "docs/semantics/proofs/GuardWitnessBinding.v"
require_file "$PANIC_CONTRACT_PATH" "src/runtime/pgy_runtime_panic_contract.h"
require_terms "$GUARD_WITNESS_COQ" "docs/semantics/proofs/GuardWitnessBinding.v" <<'TERMS'
Theorem guarded_ops_witnessed
Theorem can_be_bad_has_witness
Theorem witness_disjoint
Theorem unwitnessed_cannot_be_bad
Corollary unwitnessed_is_proven
pgy_runtime_panic_contract.h
0 admits / 0 axioms
Negative scope
TERMS
for pc_row in \
    "divide-by-zero" \
    "arithmetic-overflow" \
    "out-of-bounds" \
    "invalid-secure-token" \
    "invalid-lifecycle-state" \
    "released-slot" \
    "double-release"; do
    require_term "$GUARD_WITNESS_COQ" "docs/semantics/proofs/GuardWitnessBinding.v" "\"$pc_row\""
    require_term "$PANIC_CONTRACT_PATH" "src/runtime/pgy_runtime_panic_contract.h" "\"$pc_row\""
done

TRINITY_DOC="$ROOT_DIR/docs/155_declare_gate_failclose.md"
require_file "$TRINITY_DOC" "docs/155_declare_gate_failclose.md"
require_terms "$TRINITY_DOC" "docs/155_declare_gate_failclose.md" <<'TERMS'
의미를 선언하라. 드리프트를 게이트하라. 잔차는 fail-close하라.
자기적용
BDFL 시퀀스 결정 (2026-07-04)
"검증 완성"의 조작적 정의 (분위기 금지 — 체크리스트)
조합 안전성 스코프
fail-close는 결정-불가능의 짝이지 미구현의 변명이 아니다
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
docs/semantics/proofs/MachineLayerCore.v
docs/semantics/proofs/DelegationBoundaryCore.v
docs/semantics/proofs/LossCompositionCore.v
docs/semantics/proofs/ResourceMachineBridge.v
docs/semantics/proofs/ArchitectureBoundaryCores.md
docs/semantics/proofs/ProofSpine.v
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
Inductive Register
Definition keyword_register
Definition KeywordCombinationWellFormed
Theorem keyword_register_axis_sound
Theorem any_keyword_subset_well_formed
Theorem surface_union_preserves_well_formed
Theorem bounded_surface_axis_allowed
Theorem same_fact_keywords_share_axis
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
NodeMachineLayerCore
NodeDelegationBoundaryCore
NodeLossCompositionCore
NodeResourceMachineBridge
Theorem complete_spine_connects_architecture_boundary
Theorem complete_spine_connects_formal_kernel
Theorem complete_spine_connects_basis_selection
Theorem complete_spine_connects_certificate_pipeline
Theorem complete_spine_connects_methodology
NodeSoTAuthority
Theorem complete_spine_connects_sot_authority
NodeAsyncLifecycleCore
NodeAsyncContextCore
StructuredAsyncConnected
Theorem complete_spine_connects_structured_async
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

require_terms "$SOT_AUTHORITY_COQ" "docs/semantics/proofs/SoTAuthority.v" <<'TERMS'
Pergyra single-source-of-truth authority model
Definition AuthorityComplete
Definition AuthorityUnique
Definition RequiredFactsConsumed
Definition NoSemanticFallback
Definition RungClosed
Theorem closed_required_fact_has_exactly_one_authority
Theorem current_array_literal_rung_closed
Theorem owned_plus_fallback_bridge_is_not_closed
Theorem duplicate_semantic_producer_is_not_closed
Theorem missing_required_fact_is_not_closed
Inductive SpineFact
Inductive SpineOwner
Definition spine_authority
Theorem every_spine_fact_has_declared_authority
Theorem declared_spine_authority_unique
Theorem declared_owner_does_not_imply_rung_closed
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

require_terms "$CI_PATH" ".github/workflows/platform_full.yml" <<'TERMS'
sudo apt-get install -y gcc make llvm-dev llvm libomp-dev coq
make PGY_BACKEND_COMPARE_JOBS=1 ci-linux
TERMS

require_terms "$ROOT_DIR/Makefile" "Makefile" <<'TERMS'
scripts/ci_step_runner.sh scripts/ci_linux_steps.sh
TERMS

echo "formal semantics smoke: ok"

# Coq/Rocq machine-check.
#
# The proof list stays explicit rather than becoming a glob: several adequacy
# smokes (proof-carrying, slot-calculus, ir-minimality, ...) pin specific proofs
# into this file by literal text, so the registration is itself a checked
# contract. The inventory check below closes the other half of that contract --
# a .v on disk that nobody registered here would never be machine-checked while
# this gate reported green.
#
# Keep the proof inventory compatible with the macOS system Bash 3.2.
coq_proofs="\
docs/semantics/proofs/PergyraCore.v \
docs/semantics/proofs/PergyraCoreComposition.v \
docs/semantics/proofs/PergyraCoreZoneBridge.v \
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
docs/semantics/proofs/MachineLayerCore.v \
docs/semantics/proofs/AuthorityDelegationCore.v \
docs/semantics/proofs/DelegationBoundaryCore.v \
docs/semantics/proofs/LossCompositionCore.v \
docs/semantics/proofs/EvidenceLifecycleCore.v \
docs/semantics/proofs/ResourceMachineBridge.v \
docs/semantics/proofs/UnifiedCore.v \
docs/semantics/proofs/CompensationCore.v \
docs/semantics/proofs/CoordinationCore.v \
docs/semantics/proofs/VerificationMethodology.v \
docs/semantics/proofs/SoTAuthority.v \
docs/semantics/proofs/ModuleAuthority.v \
docs/semantics/proofs/ProofSpine.v \
docs/semantics/proofs/GuardCalculus.v \
docs/semantics/proofs/WholeProgramCore.v \
docs/semantics/proofs/AIRBinding.v \
docs/semantics/proofs/FormalKernel.v \
docs/semantics/proofs/BasisCompleteness.v \
docs/semantics/proofs/IntentObligations.v \
docs/semantics/proofs/IntentSpine.v \
docs/semantics/proofs/IntentConflict.v \
docs/semantics/proofs/AuthorityIrreducibility.v \
docs/semantics/proofs/OptionTry.v \
docs/semantics/proofs/GenericAxisCarriage.v \
docs/semantics/proofs/ReadingConfluence.v \
docs/semantics/proofs/BinaryAdequacy.v \
docs/semantics/proofs/GuardWitnessBinding.v \
docs/semantics/proofs/AsyncLifecycleCore.v \
docs/semantics/proofs/AsyncContextCore.v \
docs/semantics/proofs/AsyncScopeCore.v \
docs/semantics/proofs/CapabilityFlowCore.v \
docs/semantics/proofs/SuspensionRevalidationCore.v \
docs/semantics/proofs/DeterministicSubsetCore.v \
docs/semantics/proofs/ParallelSchedulingCore.v \
docs/semantics/proofs/ParallelReductionCore.v \
docs/semantics/proofs/PergyraMulCost.v"

# Inventory: every proof on disk must be registered above, or it silently never
# gets machine-checked.
for coq_proof_abs in "$ROOT_DIR"/docs/semantics/proofs/*.v; do
    coq_proof_rel="docs/semantics/proofs/$(basename "$coq_proof_abs")"
    case " $coq_proofs " in
        *" $coq_proof_rel "*) ;;
        *)
            echo "formal semantics Coq smoke: FAIL -- $coq_proof_rel exists but is" \
                 "not registered in this script's proof list; it would never be" \
                 "machine-checked." >&2
            exit 1
            ;;
    esac
done

coq_proof_count=$(find "$ROOT_DIR/docs/semantics/proofs" -maxdepth 1 -name '*.v' \
    | wc -l | tr -d '[:space:]')
if [ "$coq_proof_count" -eq 0 ]; then
    echo "formal semantics Coq smoke: FAIL -- no .v proofs found under" \
         "docs/semantics/proofs (an empty corpus must not pass silently)" >&2
    exit 1
fi

# Rocq 9 renamed the CLI: `rocq compile` replaces `coqc`. The Rocq Platform
# installer still ships the legacy name, so a local run never notices -- but the
# official rocq/rocq-prover image ships only the new one, while Ubuntu's apt
# `coq` (8.x) ships only the legacy one. Detect instead of assuming, or this
# gate fails on a prover that is sitting right there.
coq_compile=""
if command -v rocq >/dev/null 2>&1; then
    coq_compile="rocq compile"
elif command -v coqc >/dev/null 2>&1; then
    coq_compile="coqc"
fi

# A runner with no prover used to take a quiet skip branch and the gate still
# reported green -- macOS CI ships no Coq, so it has been skipping the whole
# corpus while passing. A missing prover is now fatal; a runner that genuinely
# has none must declare it with PGY_ALLOW_MISSING_COQ=1, and that skip is
# announced with the count of proofs left unchecked.
if [ -z "$coq_compile" ]; then
    if [ "${PGY_ALLOW_MISSING_COQ:-0}" = "1" ]; then
        echo "formal semantics Coq smoke: DECLARED SKIP -- no prover" \
             "(looked for rocq, coqc), PGY_ALLOW_MISSING_COQ=1"
        echo "  ${coq_proof_count} proofs were NOT machine-checked on this runner."
    else
        echo "formal semantics Coq smoke: FAIL -- no prover found (looked for" \
             "rocq, coqc); ${coq_proof_count} proofs would go unchecked." >&2
        echo "  Install Coq/Rocq, or set PGY_ALLOW_MISSING_COQ=1 to declare" \
             "the skip explicitly." >&2
        exit 1
    fi
else
    coq_timeout="${PGY_COQ_SMOKE_TIMEOUT_SECONDS:-60}"
    for coq_proof in $coq_proofs; do
        coq_proof_base="$(basename "$coq_proof")"
        if command -v timeout >/dev/null 2>&1; then
            (cd "$ROOT_DIR/docs/semantics/proofs" && \
                timeout "$coq_timeout" $coq_compile -Q . "" "$coq_proof_base")
        else
            (cd "$ROOT_DIR/docs/semantics/proofs" && \
                $coq_compile -Q . "" "$coq_proof_base")
        fi
    done
    echo "formal semantics Coq smoke: ok (${coq_proof_count} proofs machine-checked" \
         "with '$coq_compile')"
fi
