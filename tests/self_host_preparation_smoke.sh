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
    "Why first:"
    "Why:"
)
for term in "${tool_terms[@]}"; do
    require_text "docs/self_hosted/03_tool_candidates.md" "$term"
done

require_text "docs/self_hosted/README.md" "05_compiler_core_gap_analysis.md"
require_text "docs/INDEX.md" "self_hosted/05_compiler_core_gap_analysis.md"
require_text "docs/125_source_of_truth_spine.md" "docs/self_hosted/05_compiler_core_gap_analysis.md"
require_text "TODO.md" "Self-host preparation guard"

forbid_text "docs/self_hosted/05_compiler_core_gap_analysis.md" "hard self-host can start from the compiler core"
forbid_text "TODO.md" "hard self-host can start from the compiler core"

echo "[self-host-preparation] hard self-host guard and soft-entry criteria ok"
