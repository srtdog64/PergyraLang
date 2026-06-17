#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[self-host-preparation] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing $rel"
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

forbid_text() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        fail "$rel contains forbidden term: $term"
    fi
}

required_files=(
    "docs/self_hosted/README.md"
    "docs/self_hosted/00_agent_entry.md"
    "docs/self_hosted/01_staged_roadmap.md"
    "docs/self_hosted/02_required_language_surface.md"
    "docs/self_hosted/03_tool_candidates.md"
    "docs/self_hosted/04_beta_exit_handoff.md"
    "docs/self_hosted/05_compiler_core_gap_analysis.md"
    "docs/125_source_of_truth_spine.md"
    "docs/INDEX.md"
    "TODO.md"
    "src/self_hosted/README.md"
    "src/self_hosted/PROGRESS.md"
    "src/self_hosted/tools/diagnostic_catalog_checker/intent.md"
    "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy"
    "src/self_hosted/tools/diagnostic_catalog_checker/expected/clean.json"
    "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_code.json"
    "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_input.json"
    "src/self_hosted/parity/README.md"
    "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh"
    "src/self_hosted/semantic/intent.md"
    "src/self_hosted/semantic/main.pgy"
    "src/self_hosted/parity/semantic_parity.sh"
)

for rel in "${required_files[@]}"; do
    require_file "$rel"
done

core_gap_terms=(
    "The beta stable subset is intentionally narrow."
    "This is enough for compiler-adjacent tools."
    "It is not yet enough for a"
    "compiler core rewrite"
    "Non-Negotiable Pre-Hard-Self-Host Capabilities"
    "What Can Be Self-Hosted First"
    "What Must Not Be Done First"
    "compiler.lex"
    "compiler.codegen.llvm"
    "Hard self-host may be considered only when all are true"
)
for term in "${core_gap_terms[@]}"; do
    require_text "docs/self_hosted/05_compiler_core_gap_analysis.md" "$term"
done

substrate_terms=(
    "Stable graph-heavy compiler data structures"
    "Stable collection ergonomics"
    "Arena-backed scratch/result/persistent allocation lanes"
    "Scoped unsafe/raw escape policy"
    "Runtime-none/minimal-runtime policy"
)
for term in "${substrate_terms[@]}"; do
    require_text "docs/self_hosted/02_required_language_surface.md" "$term"
done

handoff_terms=(
    "hard-self-host gap analysis"
    "graph-heavy collections"
    "scoped unsafe/raw escape"
    "debug-info strategy"
    "The first self-host work package should be the **Diagnostic Catalog Checker**."
    "The second self-host work package should be the **AIR Graph JSON Validator**."
    "Self-hosted code must not copy the C-era helper-file pattern."
)
for term in "${handoff_terms[@]}"; do
    require_text "docs/self_hosted/04_beta_exit_handoff.md" "$term"
done

agent_terms=(
    "Do not start a full compiler rewrite before beta closure."
    "Keep the C compiler as the oracle during soft and partial self-hosting."
    "Every self-hosted component must have an intent-verification pair"
    "The smallest acceptable unit is:"
    "one parity check against the C implementation."
)
for term in "${agent_terms[@]}"; do
    require_text "docs/self_hosted/00_agent_entry.md" "$term"
done

roadmap_terms=(
    "Soft Self-Host"
    "Partial Self-Host"
    "Hard self-host"
    "metadata dead-end"
)
for term in "${roadmap_terms[@]}"; do
    require_text "docs/self_hosted/01_staged_roadmap.md" "$term"
done

tool_terms=(
    "## 1. Diagnostic Catalog Checker"
    "## 2. AIR Graph JSON Validator"
    "stable JSON or diagnostics output"
    "\"schema\": \"pgy.selfhost.diagnostic-catalog.v1\""
    "\"schema\": \"pgy.selfhost.air-graph-validator.v1\""
    "\"input_schema\": \"pgy.air.graph.v1\""
    "\"findings\": []"
    "Why first:"
    "Why:"
)
for term in "${tool_terms[@]}"; do
    require_text "docs/self_hosted/03_tool_candidates.md" "$term"
done

require_text "docs/self_hosted/README.md" "05_compiler_core_gap_analysis.md"
require_text "docs/self_hosted/README.md" "src/self_hosted/"
require_text "docs/INDEX.md" "self_hosted/05_compiler_core_gap_analysis.md"
require_text "docs/125_source_of_truth_spine.md" "docs/self_hosted/05_compiler_core_gap_analysis.md"
require_text "TODO.md" "Self-host preparation guard"

require_text "src/self_hosted/README.md" "This directory is not the compiler core."
require_text "src/self_hosted/README.md" "A tool that does not yet pass this contract is a *scaffold*"
require_text "src/self_hosted/PROGRESS.md" "Compiler-internal substitution"
require_text "src/self_hosted/PROGRESS.md" "Peripheral Audit Tools (Not Counted In Coverage)"
require_text "src/self_hosted/PROGRESS.md" "Do **not** add peripheral audit tools to the substitution percentage."
require_text "src/self_hosted/tools/diagnostic_catalog_checker/intent.md" "pgy.selfhost.diagnostic-catalog.v1"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/intent.md" "tests/diagnostic_registry_smoke.sh"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy" "Status: rung-2"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy" "ReadFile(\"src/semantic/diag_codes.h\")"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy" "ReadFile(\"docs/72_diagnostic_codes.md\")"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy" "FileExists(header_path)"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy" "\\\"kind\\\":\\\"input_error\\\""
require_text "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy" "\\\"documented\\\":"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy" "CountMissingCodes"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy" "StringIndexOf"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy" "\\\"missing\\\":"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy" "CountDuplicateLiterals"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy" "CountOrphanDocs"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/main.pgy" "Exit(1)"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/clean.json" "\"codes\":"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/clean.json" "\"documented\":"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/clean.json" "\"missing\":0"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/clean.json" "\"duplicates\":0"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/clean.json" "\"orphans\":0"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_code.json" "\"ok\":false"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_code.json" "\"codes\":67"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_code.json" "\"missing\":1"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_code.json" "\"kind\":\"missing\""
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_code.json" "PGY_FAKE_DRIFT_FOR_SELFHOST"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_input.json" "\"kind\":\"input_error\""
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_input.json" "src/semantic/diag_codes.h"
require_text "src/self_hosted/parity/README.md" "No compiler-core self-host migration is allowed"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "Rung 2 parity"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "tests/diagnostic_registry_smoke.sh"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "pgy.selfhost.diagnostic-catalog.v1"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "counts.documented parity"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "counts.missing parity"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "counts.orphans parity"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "clean JSON parity"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "missing-code fixture"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "missing-input fixture"
require_text "src/self_hosted/semantic/intent.md" "function return types"
require_text "src/self_hosted/semantic/intent.md" "C compiler accept/reject oracle"
require_text "src/self_hosted/lib/diagnostic.pgy" "namespace SelfHostDiagnostic"
require_text "src/self_hosted/lib/diagnostic.pgy" "RenderError"
require_text "src/self_hosted/lib/diagnostic.pgy" "Diagnostic: "
require_text "src/self_hosted/semantic/main.pgy" "Status: rung-2"
require_text "src/self_hosted/semantic/main.pgy" "pgy.selfhost.semantic.v1"
require_text "src/self_hosted/semantic/main.pgy" "SelfHostDiagnostic_RenderError"
require_text "src/self_hosted/semantic/main.pgy" "let_type_mismatch"
require_text "src/self_hosted/semantic/main.pgy" "return_type_mismatch"
require_text "src/self_hosted/parity/semantic_parity.sh" "Rung 2 parity"
require_text "src/self_hosted/parity/semantic_parity.sh" "C compiler"
require_text "src/self_hosted/parity/semantic_parity.sh" "PGY_SELFHOST_SEMANTIC_BACKENDS"
require_text "src/self_hosted/parity/semantic_parity.sh" "raw semantic text leaked"
require_text "src/self_hosted/parity/semantic_parity.sh" "JSON semantic output leaked"

forbid_text "docs/self_hosted/05_compiler_core_gap_analysis.md" "hard self-host can start from the compiler core"
forbid_text "TODO.md" "hard self-host can start from the compiler core"

echo "[self-host-preparation] hard self-host guard and soft-entry criteria ok"
