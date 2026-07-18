#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[abstraction-loss-contract] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing required file: $rel"
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" || fail "$rel missing term: $term"
}

for rel in \
    "docs/semantics/09_abstraction_loss_contracts.md" \
    "docs/semantics/loss_contract_manifest.md" \
    "docs/semantics/pass_contract_manifest.md" \
    "docs/semantics/proofs/LossCompositionCore.v" \
    "docs/semantics/proofs/ArchitectureBoundaryCores.md" \
    "docs/semantics/README.md" \
    "docs/102_formal_semantics_and_proof_obligations.md" \
    "docs/104_air_compiler_architecture.md" \
    "docs/37_compiler_contracts.md" \
    "docs/125_source_of_truth_spine.md"; do
    require_file "$rel"
done

require_text "docs/semantics/09_abstraction_loss_contracts.md" 'Status: `beta-proof-obligation`'
require_text "docs/semantics/09_abstraction_loss_contracts.md" "Stable surface: compiler and tooling abstraction boundaries."
require_text "docs/semantics/09_abstraction_loss_contracts.md" "Loss is not automatically a bug. Hidden loss is the bug."
require_text "docs/semantics/09_abstraction_loss_contracts.md" "An abstraction loss contract has seven fields:"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "Loss budget classes:"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "Compression budget classes:"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "proof-gated erasure contract"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "Zone is a semantic boundary"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "World/Zone/Intent/Slot are source-level semantic axes, not backend-level"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "The chain World -> Zone -> Roster -> Role -> Intent -> Slot"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "Missing evidence never becomes guessed static success."
require_text "docs/semantics/09_abstraction_loss_contracts.md" "Evidence strength is not interchangeable:"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "## Loss Composition"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "## Derived Mechanism Boundary"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "## Erasure Evidence And Executable Decision"
require_text "docs/semantics/09_abstraction_loss_contracts.md" '`VerifiedProjectionPlan` owner consumes the AIR evidence certificate together'
require_text "docs/semantics/09_abstraction_loss_contracts.md" "makes the only executable"
require_text "docs/semantics/09_abstraction_loss_contracts.md" 'The owner artifact is `pgy.air.graph.v1`.'
require_text "docs/semantics/09_abstraction_loss_contracts.md" "These are not erasure decision points:"
require_text "docs/semantics/09_abstraction_loss_contracts.md" '`tests/air_erasure`: it is the independent physical-residue oracle'
require_text "docs/semantics/09_abstraction_loss_contracts.md" "## Evidence-Amortization Cache Rule"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "owner fact -> preflight/evidence view -> hot-path use -> cleanup/invalidation"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "The stable example is Slot Pin/Lease."
require_text "docs/semantics/09_abstraction_loss_contracts.md" "It is an evidence-amortized language."
require_text "docs/semantics/09_abstraction_loss_contracts.md" '`compression_budget` and'
require_text "docs/semantics/09_abstraction_loss_contracts.md" '`consumer forbidden_to_recover fact from source`'
require_text "docs/semantics/09_abstraction_loss_contracts.md" '`K compression budget fact`'
require_text "docs/semantics/09_abstraction_loss_contracts.md" "### AST To HIR/DIR/RIR/MIR"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "### MIR To AIR"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "### MIR To C And LLVM"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "### Self-Hosted Tool To C Oracle"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "## Executable Invariant: Loss Visibility"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "## Executable Invariant: Preservation Carry"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "## Proof Obligation: Bounded Approximation Visibility"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "That syntax is a design sketch only."
require_text "docs/semantics/09_abstraction_loss_contracts.md" "documentation does not call the boundary lossless unless the budget is"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "Current manifest coverage is 4/5 gate-enforced"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "pass_contract_manifest.md"
require_text "docs/semantics/proofs/LossCompositionCore.v" "Theorem local_budgets_do_not_imply_path_budget"
require_text "docs/semantics/proofs/LossCompositionCore.v" "Theorem derived_mechanism_requires_observational_equivalence"
require_text "docs/semantics/proofs/ArchitectureBoundaryCores.md" "A local loss budget does not establish a path budget"

require_text "docs/semantics/loss_contract_manifest.md" "5 canonical boundaries, **4 gate-enforced**"
require_text "docs/semantics/loss_contract_manifest.md" "tests/ast_to_mir_loss_contract_smoke.sh"
require_text "docs/semantics/loss_contract_manifest.md" "parser_to_ast"
require_text "docs/semantics/loss_contract_manifest.md" "ast_to_mir"
require_text "docs/semantics/loss_contract_manifest.md" "mir_to_air"
require_text "docs/semantics/loss_contract_manifest.md" "mir_to_backends"
require_text "docs/semantics/loss_contract_manifest.md" "selfhost_to_oracle"

require_text "docs/semantics/pass_contract_manifest.md" 'Status: `beta-proof-obligation`'
require_text "docs/semantics/pass_contract_manifest.md" "<!-- BEGIN pass-contract-manifest -->"
require_text "docs/semantics/pass_contract_manifest.md" "<!-- END pass-contract-manifest -->"
require_text "docs/semantics/pass_contract_manifest.md" "parser_to_ast_loss | src/parser/ast.c | tests/abstraction_loss_contract_smoke.sh | manifest-tracked-doc-only"
require_text "docs/semantics/pass_contract_manifest.md" "parser_to_ast loss is still documentation-only"
require_text "docs/semantics/pass_contract_manifest.md" "mir_cfg_body_safety | src/compiler/mir_fact_surface_validate.c | tests/cfg_body_dataflow_smoke.sh | gate-backed"
require_text "docs/semantics/pass_contract_manifest.md" "air_boundary_evidence | src/compiler/air_validate_global_evidence.c | tests/air_drift_smoke.sh | gate-backed"
require_text "docs/semantics/pass_contract_manifest.md" "air_abstraction_compression | src/compiler/air_boundary.c | tests/air_json_schema_smoke.sh | gate-backed"
require_text "docs/semantics/pass_contract_manifest.md" "dag_type_resolution | src/semantic/type_checker_resolution_metadata.c | tests/type_resolution_resolver_inventory_smoke.sh | gate-backed"
require_text "docs/semantics/pass_contract_manifest.md" "mir_decl_bootstrap_parity | src/codegen/llvm_decl.c | tests/mir_declaration_inventory_smoke.sh | gate-backed"
require_text "docs/semantics/pass_contract_manifest.md" "abi_slot_pin_layout | src/compiler/mir_abi_layout.c | tests/abi_ownership_shape_smoke.sh | gate-backed"
require_text "docs/semantics/pass_contract_manifest.md" "boundary_owner_migration | docs/semantics/boundary_migration_manifest.md | tests/boundary_migration_contract_smoke.sh | gate-backed"
require_text "docs/semantics/pass_contract_manifest.md" "merged_program_syntax_identity | src/parser/ast_identity.c | tests/stable_identity_contract_smoke.sh | gate-backed-partial"
require_text "docs/semantics/pass_contract_manifest.md" "semantic_declaration_identity | src/semantic/symbol_table.c | tests/semantic_declaration_identity_smoke.sh | gate-backed-partial"
require_text "docs/semantics/pass_contract_manifest.md" "hir_routine_identity | src/compiler/hir_callgraph.c | tests/hir_routine_identity_smoke.sh | gate-backed-partial"
require_text "docs/semantics/pass_contract_manifest.md" "required_facts"
require_text "docs/semantics/pass_contract_manifest.md" "preserved_facts"
require_text "docs/semantics/pass_contract_manifest.md" "invalidated_facts"
require_text "docs/semantics/pass_contract_manifest.md" "stable_diagnostics"
require_text "docs/semantics/pass_contract_manifest.md" "forbidden_reads"
require_text "docs/semantics/pass_contract_manifest.md" "unowned_ast_rescan"
require_text "docs/semantics/pass_contract_manifest.md" "backend_ast_semantic_read"
require_text "docs/semantics/pass_contract_manifest.md" "backend_local_layout_guess"
require_text "docs/semantics/pass_contract_manifest.md" "compat_success_without_fact"
require_text "docs/semantics/pass_contract_manifest.md" "backend_source_axis_physicalization"
require_text "docs/semantics/pass_contract_manifest.md" "compression_budget"
require_text "docs/semantics/pass_contract_manifest.md" "proof_gated_erasure_vocabulary"

for rel in \
    "src/parser/ast.c" \
    "src/compiler/mir_fact_surface_validate.c" \
    "src/compiler/air_validate_global_evidence.c" \
    "src/compiler/air_boundary.c" \
    "src/semantic/type_checker_resolution_metadata.c" \
    "src/codegen/llvm_decl.c" \
    "src/compiler/mir_abi_layout.c" \
    "tests/cfg_body_dataflow_smoke.sh" \
    "tests/air_drift_smoke.sh" \
    "tests/air_json_schema_smoke.sh" \
    "tests/type_resolution_resolver_inventory_smoke.sh" \
    "tests/mir_declaration_inventory_smoke.sh" \
    "tests/abi_ownership_shape_smoke.sh"; do
    require_file "$rel"
done

require_text "docs/semantics/README.md" "09_abstraction_loss_contracts.md"
require_text "docs/semantics/README.md" "pass_contract_manifest.md"
require_text "docs/semantics/README.md" "proofs/IRMinimality.v"
require_text "docs/semantics/14_air_erasure_measurement.md" "## 0a. Evidence And Decision Point"
require_text "docs/semantics/14_air_erasure_measurement.md" "That is the single semantic evidence classification, not an executable backend"
require_text "docs/semantics/14_air_erasure_measurement.md" "whether the certified plan matched reality"
require_text "docs/semantics/14_air_erasure_measurement.md" "## 5a. Evidence-Amortized Hot Path"
require_text "docs/semantics/14_air_erasure_measurement.md" "This optimization path is cacheable"
require_text "docs/semantics/14_air_erasure_measurement.md" "Host-toolchain residue must be normalized."
require_text "docs/semantics/14_air_erasure_measurement.md" "Retained-runtime attribution (hard)"
require_text "tests/air_erasure/baseline.json" "retained_runtime_attribution"
require_text "tests/air_erasure/gate.ps1" "Retained-runtime attribution"
require_text "docs/102_formal_semantics_and_proof_obligations.md" "docs/semantics/09_abstraction_loss_contracts.md"
require_text "docs/104_air_compiler_architecture.md" "The general version of this rule is the abstraction loss contract"
require_text "docs/37_compiler_contracts.md" "### Loss Contracts"
require_text "docs/37_compiler_contracts.md" "Compression uses the same owner discipline."
require_text "docs/37_compiler_contracts.md" '`compression_budget` plus'
require_text "docs/125_source_of_truth_spine.md" "## 9. Loss Contract Rule"
require_text "docs/125_source_of_truth_spine.md" "what fact is intentionally lost"
require_text "docs/125_source_of_truth_spine.md" "which later layer is forbidden from rereading the older source"
require_text "docs/125_source_of_truth_spine.md" "compression_budget"
require_text "docs/125_source_of_truth_spine.md" "optimizer guesswork"
require_text "docs/125_source_of_truth_spine.md" "not a required runtime object graph"

echo "[abstraction-loss-contract] contract vocabulary and proof-pack links ok"
