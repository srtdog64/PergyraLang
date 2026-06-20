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
require_text "docs/semantics/09_abstraction_loss_contracts.md" '`consumer forbidden_to_recover fact from source`'
require_text "docs/semantics/09_abstraction_loss_contracts.md" "### AST To HIR/DIR/RIR/MIR"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "### MIR To AIR"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "### MIR To C And LLVM"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "### Self-Hosted Tool To C Oracle"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "## Theorem: Loss Visibility"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "## Theorem: Preservation Carry"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "## Theorem: Bounded Approximation Soundness"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "That syntax is a design sketch only."
require_text "docs/semantics/09_abstraction_loss_contracts.md" "documentation does not call the boundary lossless unless the budget is"
require_text "docs/semantics/09_abstraction_loss_contracts.md" "Current manifest coverage is 3/5 gate-enforced"

require_text "docs/semantics/loss_contract_manifest.md" "5 canonical boundaries, **3 gate-enforced**"
require_text "docs/semantics/loss_contract_manifest.md" "parser_to_ast"
require_text "docs/semantics/loss_contract_manifest.md" "ast_to_mir"
require_text "docs/semantics/loss_contract_manifest.md" "mir_to_air"
require_text "docs/semantics/loss_contract_manifest.md" "mir_to_backends"
require_text "docs/semantics/loss_contract_manifest.md" "selfhost_to_oracle"

require_text "docs/semantics/README.md" "09_abstraction_loss_contracts.md"
require_text "docs/semantics/README.md" "proofs/IRMinimality.v"
require_text "docs/102_formal_semantics_and_proof_obligations.md" "docs/semantics/09_abstraction_loss_contracts.md"
require_text "docs/104_air_compiler_architecture.md" "The general version of this rule is the abstraction loss contract"
require_text "docs/37_compiler_contracts.md" "### Loss Contracts"
require_text "docs/125_source_of_truth_spine.md" "## 9. Loss Contract Rule"
require_text "docs/125_source_of_truth_spine.md" "what fact is intentionally lost"
require_text "docs/125_source_of_truth_spine.md" "which later layer is forbidden from rereading the older source"

echo "[abstraction-loss-contract] contract vocabulary and proof-pack links ok"
