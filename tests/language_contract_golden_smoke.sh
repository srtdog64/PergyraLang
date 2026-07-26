#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[language-contract-golden] $*" >&2
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
    docs/semantics/16_language_contract_golden_spine.md \
    docs/semantics/17_proof_carrying_pipeline.md \
    docs/semantics/18_machine_neutral_compute.md \
    docs/semantics/19_theoretical_foundations.md \
    docs/139_golden_adt_verification_methodology.md \
    docs/semantics/README.md \
    docs/semantics/proofs/IRMinimality.v \
    docs/semantics/proofs/WitnessDataRace.v \
    docs/semantics/proofs/SlotCalculus.v \
    docs/semantics/proofs/ProofCarryingIR.v \
    docs/semantics/proofs/ProofCarryingIR.md \
    docs/semantics/proofs/VerificationMethodology.v \
    docs/semantics/proofs/VerificationMethodology.md \
    docs/semantics/proofs/ProofSpine.v \
    docs/semantics/proofs/ProofSpine.md \
    tests/formal_semantics_smoke.sh \
    tests/verification_methodology_smoke.sh \
    tests/proof_spine_smoke.sh \
    tests/proof_carrying_adequacy_smoke.sh \
    tests/slot_calculus_adequacy_smoke.sh \
    tests/axis_keyword_adequacy_smoke.sh \
    tests/ir_minimality_adequacy_smoke.sh \
    tests/backend_fail_closed_smoke.sh \
    tests/mir_declaration_inventory_smoke.sh \
    tests/intent_compression_contract_smoke.sh \
    tests/air_erasure/baseline.json \
    tests/air_erasure/gate.ps1 \
    tests/raw_escape_contract_smoke.sh \
    tests/proof_carrying_pipeline_smoke.sh \
    tests/abi_ownership_shape_smoke.sh \
    tests/self_hosted/parity/semantic_parity.sh \
    tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh \
    tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh \
    src/self_hosted/semantic/diagnostic_owner.pgy \
    src/self_hosted/semantic/fixture/valid_logical_bool.pgy \
    src/self_hosted/semantic/expected/valid_logical_bool.diag \
    src/self_hosted/semantic/fixture/bad_logical_int.pgy \
    src/self_hosted/semantic/expected/bad_logical_int.diag \
    src/self_hosted/semantic/fixture/bad_logical_right.pgy \
    src/self_hosted/semantic/expected/bad_logical_right.diag \
    src/self_hosted/semantic/fixture/bad_value_param_arraypush.pgy \
    src/self_hosted/semantic/expected/bad_value_param_arraypush.diag \
    tests/cases/backend_compare/inout_caller_mutation/main.pgy \
    tests/cases/backend_compare/inout_nested_return_copyout/main.pgy \
    src/parser/parser_decl.c \
    src/parser/parser_async.c \
    src/parser/parser_type.c \
    src/semantic/type_checker_collection_mutation_contract.c \
    src/semantic/type_checker_helpers_late.c \
    docs/136_abi_niche_and_explicit_layout.md \
    docs/145_bit_layout_boundary_matrix.md \
    Makefile; do
    require_file "$rel"
done

require_text "docs/semantics/README.md" "16_language_contract_golden_spine.md"
require_text "docs/semantics/README.md" "18_machine_neutral_compute.md"
require_text "docs/semantics/README.md" "19_theoretical_foundations.md"
require_text "docs/semantics/16_language_contract_golden_spine.md" "language-contract-golden-test-smoke"
require_text "docs/semantics/16_language_contract_golden_spine.md" "semantic fallback is not a compatibility feature"
require_text "docs/semantics/16_language_contract_golden_spine.md" "authority evidence discharges an effect-derived obligation"
require_text "docs/semantics/16_language_contract_golden_spine.md" '`inout` is value-result mutation'
require_text "docs/semantics/16_language_contract_golden_spine.md" "logical operators produce Bool"
require_text "docs/semantics/16_language_contract_golden_spine.md" "proof-gated erasure"
require_text "docs/semantics/16_language_contract_golden_spine.md" "raw/FFI/explicit layout stays boundary-scoped"
require_text "docs/semantics/16_language_contract_golden_spine.md" "Proof-carrying IR"
require_text "docs/semantics/16_language_contract_golden_spine.md" "Verification methodology"
require_text "docs/semantics/16_language_contract_golden_spine.md" "Proof spine"
require_text "docs/semantics/16_language_contract_golden_spine.md" "Architecture boundary"
require_text "docs/semantics/16_language_contract_golden_spine.md" "authorization is not delegability"
require_text "docs/semantics/16_language_contract_golden_spine.md" "explicit projection binding"
require_text "docs/semantics/16_language_contract_golden_spine.md" "Machine-neutral compute"
require_text "docs/semantics/16_language_contract_golden_spine.md" "Theoretical foundations"
require_text "docs/semantics/16_language_contract_golden_spine.md" "self-hosted work starts with verifier/tool parity"
require_text "docs/semantics/17_proof_carrying_pipeline.md" "pgy.proof-carrying-ir.v1"
require_text "docs/semantics/17_proof_carrying_pipeline.md" "valid certificate + valid owner payloads"
require_text "docs/semantics/18_machine_neutral_compute.md" "C and LLVM are the first validation projections"
require_text "docs/semantics/18_machine_neutral_compute.md" "machine-neutral fact ownership"
require_text "docs/semantics/18_machine_neutral_compute.md" "Projection Fact Envelope"
require_text "docs/semantics/18_machine_neutral_compute.md" "Pergyra is an intent/evidence language whose backends are projection consumers."
require_text "docs/semantics/18_machine_neutral_compute.md" "Dataflow architecture"
require_text "docs/semantics/18_machine_neutral_compute.md" "Actor model"
require_text "docs/semantics/18_machine_neutral_compute.md" "Systolic / tensor / NPU architecture"
require_text "docs/semantics/18_machine_neutral_compute.md" "Capability machine"
require_text "docs/semantics/18_machine_neutral_compute.md" "Neuromorphic / event-driven systems"
require_text "docs/semantics/18_machine_neutral_compute.md" "its Projection Planner consumes owner facts and emits a"
require_text "docs/semantics/18_machine_neutral_compute.md" 'VerifiedProjectionPlan`; the backend consumes that plan instead of AIR or'
require_text "docs/semantics/18_machine_neutral_compute.md" "A CPU fallback is not an implicit escape hatch."
require_text "docs/semantics/18_machine_neutral_compute.md" "loss/quantization acceptance"
require_text "docs/semantics/19_theoretical_foundations.md" "theory-lineage; not whole-language proof"
require_text "docs/semantics/19_theoretical_foundations.md" "A citation is a lineage anchor, not an implementation theorem"
require_text "docs/semantics/19_theoretical_foundations.md" "Channel<T> alone is not a session type"
require_text "docs/semantics/19_theoretical_foundations.md" "Dataflow alone is too thin"
require_text "docs/semantics/19_theoretical_foundations.md" "Pergyra Abstract Machine Obligation"
require_text "docs/semantics/19_theoretical_foundations.md" "Backend simulation"
require_text "docs/semantics/19_theoretical_foundations.md" "Effects as Sessions"
require_text "docs/semantics/19_theoretical_foundations.md" "RustBelt"
require_text "tests/proof_carrying_pipeline_smoke.sh" "delete-required-fact"
require_text "tests/proof_carrying_pipeline_smoke.sh" "negative certificate deletion was accepted"

# Proof/refinement anchors: the formal gate must actually run a prover over
# every registered proof, and adequacy smokes must bind the small models back to
# live compiler/runtime symbols.
#
# This used to pin the literal `coqc "$coq_proof"`. Two things changed. Rocq 9
# renamed the CLI (`rocq compile` replaces coqc) and its official image ships
# only the new name, so the invocation is resolved into $coq_compile instead of
# hardcoding one binary. And "type-check when available" turned out to mean the
# gate passed green on runners with no prover at all (macOS CI was skipping the
# whole corpus), so the absence of a prover is now fatal unless the skip is
# declared -- that stronger contract is pinned here too. The shared
# PergyraCore root also makes the sibling-module load path load-bearing.
require_text "tests/formal_semantics_smoke.sh" 'docs/semantics/proofs/PergyraCore.v'
require_text "tests/formal_semantics_smoke.sh" '-Q . "" "$coq_proof_base"'
require_text "tests/formal_semantics_smoke.sh" "PGY_ALLOW_MISSING_COQ"
require_text "tests/slot_calculus_adequacy_smoke.sh" "SlotCalculus.v model <-> slot_manager.h runtime consistent"
require_text "tests/axis_keyword_adequacy_smoke.sh" "Coq keyword_axis (AxisOwnership.v section 8) = docs/42 axis"
require_text "tests/ir_minimality_adequacy_smoke.sh" "Coq reads-from model is bound to live compiler dependency shape"
require_text "docs/semantics/proofs/IRMinimality.v" "Theorem air_is_minimal_witness_set"
require_text "docs/semantics/proofs/IRMinimality.v" "Theorem functor_hkt_not_adequate"
require_text "docs/semantics/proofs/WitnessDataRace.v" "Theorem well_typed_data_race_free"
require_text "docs/semantics/proofs/WitnessDataRace.v" "remaining refinement obligation"
require_text "docs/semantics/proofs/SlotCalculus.v" "Negative scope: this file does not prove Rust-style borrow checking"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "Theorem valid_certificate_allows_backend_consumption"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "Theorem missing_air_authority_fails_closed"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "Theorem compat_success_policy_fails_closed"
require_text "tests/proof_carrying_adequacy_smoke.sh" "checker-core model is bound to live certificate gate"
require_text "docs/139_golden_adt_verification_methodology.md" "A fact is useful only when its owner, consumer, oracle, and regression gate are named."
require_text "docs/139_golden_adt_verification_methodology.md" "Golden tests, differential tests, property tests, ADT owners, and mechanized"
require_text "docs/139_golden_adt_verification_methodology.md" "Hard self-hosting must not mean"
require_text "docs/semantics/proofs/VerificationMethodology.v" "Theorem golden_only_not_model_soundness"
require_text "docs/semantics/proofs/VerificationMethodology.v" "Theorem hard_self_host_requires_differential"
require_text "docs/semantics/proofs/VerificationMethodology.v" "Theorem materialization_requires_trace_and_capability"
require_text "docs/semantics/proofs/VerificationMethodology.md" "This is not whole-compiler verification."
require_text "tests/verification_methodology_smoke.sh" "verification-methodology"
require_text "docs/semantics/proofs/ProofSpine.v" "Theorem complete_spine_connects_unified_machine"
require_text "docs/semantics/proofs/ProofSpine.v" "Theorem complete_spine_is_not_whole_language_verification"
require_text "docs/semantics/proofs/ProofSpine.md" "complete proof spine != whole-language verification"
require_text "tests/proof_spine_smoke.sh" "proof-spine"

# Fallback and SoT anchors.
require_text "tests/backend_fail_closed_smoke.sh" "C backend reintroduced silent numeric fallback"
require_text "tests/backend_fail_closed_smoke.sh" "LLVM return lowering reintroduced AST return-type fallback"
require_text "tests/mir_declaration_inventory_smoke.sh" "source_ast"
require_text "tests/mir_declaration_inventory_smoke.sh" "fallback"

# Authority/effect/intent compression anchors.
require_text "tests/intent_compression_contract_smoke.sh" "AIR_EVIDENCE_RIR_AUTHORITY"
require_text "tests/intent_compression_contract_smoke.sh" "AIR_EVIDENCE_RIR_EFFECT_PROPAGATION"
require_text "tests/intent_compression_contract_smoke.sh" "zone-authority approval provenance must not be inferred from who"

# Mutability surface anchors: &mut must remain rejected and value-result
# mutation must stay spelled inout with a caller-visible golden.
require_text "src/parser/parser_decl.c" "mutation is spelled 'inout', never '&mut'"
require_text "src/parser/parser_async.c" "mutation is spelled 'inout', never '&mut'"
require_text "src/parser/parser_type.c" 'caller-visible mutation is value-result and spelled with the `inout`'
require_text "src/semantic/type_checker_helpers_late.c" "lost update"
require_text "tests/cases/backend_compare/inout_caller_mutation/main.pgy" "func Grow(inout xs: Array<Int>) -> Void"
require_text "tests/cases/backend_compare/inout_caller_mutation/main.pgy" "ArrayPush(xs, 9)"
require_text "tests/cases/backend_compare/inout_nested_return_copyout/main.pgy" "return AppendAndConfirm(xs);"
require_text "src/semantic/symbol_table.h" "is_parameter"
require_text "src/semantic/symbol_table.h" "param_mode"
require_text "src/semantic/type_checker_func_decl.c" "p->is_parameter = true"
require_text "src/semantic/type_checker_builtins_internal.h" "reject_non_inout_param_collection_mutator_receiver"
require_text "src/semantic/type_checker_collection_mutation_contract.c" "Collection mutator"
require_text "src/semantic/type_checker_collection_mutation_contract.c" "cannot target non-inout parameter"
require_text "src/semantic/type_checker_builtins_stdlib_map.c" "MapSet"
require_text "src/semantic/type_checker_assignment.c" "array index assignment"

# Logical Bool anchors: self-hosted semantic parity has exact diagnostics for
# the same contract the C compiler rejects.
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "EmitSemanticVerdictPayloadFixtureManifest"
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "SemanticVerdictPayloadFixtureManifestRows"
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" 'Concat(Concat(base, ":"), status)'
require_text "src/self_hosted/semantic/fixture/valid_logical_bool.pgy" "let r: Bool = true && false"
require_text "src/self_hosted/semantic/expected/valid_logical_bool.diag" "Status: ok"
require_text "src/self_hosted/semantic/fixture/bad_logical_int.pgy" "let r: Bool = 1 && 2"
require_text "src/self_hosted/semantic/expected/bad_logical_int.diag" "Code: logical_operand_not_bool"
require_text "src/self_hosted/semantic/fixture/bad_logical_right.pgy" "let r: Bool = true && 3"
require_text "src/self_hosted/semantic/expected/bad_logical_right.diag" "Reason: logical operators require Bool operands"
require_text "src/self_hosted/semantic/fixture/bad_value_param_arraypush.pgy" "func Grow(xs: Array<Int>) -> Void"
require_text "src/self_hosted/semantic/fixture/bad_value_param_arraypush.pgy" "ArrayPush(xs, 9)"
require_text "src/self_hosted/semantic/expected/bad_value_param_arraypush.diag" "Code: value_param_collection_mutation"
require_text "src/self_hosted/semantic/expected/bad_value_param_arraypush.diag" "Fix: spell the parameter as inout"

# Proof-gated erasure and raw/layout anchors.
require_text "tests/air_erasure/gate.ps1" "provable fixture must compile to ZERO"
require_text "tests/air_erasure/baseline.json" "provable_fixtures_must_be_clean"
require_text "tests/raw_escape_contract_smoke.sh" "scoped unsafe(raw) capability"
require_text "tests/abi_ownership_shape_smoke.sh" "MIR_ABI_REPR_NICHE_RESERVED"
require_text "docs/136_abi_niche_and_explicit_layout.md" "MIR ABI fact must be the only backend input"
require_text "docs/136_abi_niche_and_explicit_layout.md" "unsafe(ffi, layout)"
require_text "docs/145_bit_layout_boundary_matrix.md" "There is no default bit order"
require_text "docs/145_bit_layout_boundary_matrix.md" "Language Comparison And Pergyra Gaps"
require_text "docs/124_syntax_pattern_matrix.md" "Hidden logical-bit cast defaults"

# Self-hosting starts with verifier/tool parity, not a second compiler claim.
require_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "Clean JSON parity is a run-output artifact verdict owned by the Pergyra"
require_text "tests/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "pgy_selfhost_compare_expected_text_artifact_with_owner"
require_text "tests/self_hosted/parity/semantic_parity.sh" "compare_semantic_verdict_with_owner"
require_text "tests/self_hosted/parity/semantic_parity.sh" "C oracle code drift"
require_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" "PGY_FUZZ_BACKEND_RUN_ORACLE"

require_text "Makefile" "language-contract-golden-test-smoke:"
require_text "Makefile" "verification-methodology-test-smoke:"
require_text "Makefile" "proof-spine-test-smoke:"
require_text "Makefile" "proof-carrying-pipeline-test-smoke:"
require_text "Makefile" "proof-carrying-adequacy-test-smoke:"

echo "[language-contract-golden] golden spine ok"
