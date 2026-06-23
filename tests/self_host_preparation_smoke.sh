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
    "docs/self_hosted/10_hard_self_host_contract.md"
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
    "src/self_hosted/lexer/intent.md"
    "src/self_hosted/lexer/main.pgy"
    "src/self_hosted/lexer/char_owner.pgy"
    "src/self_hosted/lexer/token_owner.pgy"
    "src/self_hosted/lexer/scan_owner.pgy"
    "src/self_hosted/parity/lexer_parity.sh"
    "src/self_hosted/parser/intent.md"
    "src/self_hosted/parser/main.pgy"
    "src/self_hosted/parser/error_owner.pgy"
    "src/self_hosted/parser/cursor_owner.pgy"
    "src/self_hosted/parser/tree_text_owner.pgy"
    "src/self_hosted/parser/type_name_owner.pgy"
    "src/self_hosted/parser/expr_string_owner.pgy"
    "src/self_hosted/parser/expr_primary_owner.pgy"
    "src/self_hosted/parser/expr_precedence_owner.pgy"
    "src/self_hosted/parity/parser_parity.sh"
    "src/self_hosted/semantic/intent.md"
    "src/self_hosted/semantic/main.pgy"
    "src/self_hosted/semantic/text_scan_owner.pgy"
    "src/self_hosted/semantic/diagnostic_owner.pgy"
    "src/self_hosted/semantic/env_owner.pgy"
    "src/self_hosted/semantic/expr_type_owner.pgy"
    "src/self_hosted/semantic/call_check_owner.pgy"
    "src/self_hosted/semantic/body_check_owner.pgy"
    "src/self_hosted/semantic/program_check_owner.pgy"
    "src/self_hosted/parity/semantic_parity.sh"
    "tests/self_host_substrate_contract_smoke.sh"
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
    "Stable arbitrary data tree representation"
    "Arena-backed scratch/result/persistent allocation lanes"
    "Stable scoped unsafe/raw escape policy"
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
    "Keep the C compiler as the oracle during soft, partial, and hard substitution work."
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
    "SoT closure is a pass condition"
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
require_text "docs/self_hosted/README.md" "10_hard_self_host_contract.md"
require_text "docs/self_hosted/README.md" "src/self_hosted/"
require_text "docs/INDEX.md" "self_hosted/05_compiler_core_gap_analysis.md"
require_text "docs/INDEX.md" "self_hosted/10_hard_self_host_contract.md"
require_text "docs/125_source_of_truth_spine.md" "docs/self_hosted/05_compiler_core_gap_analysis.md"
require_text "TODO.md" "Self-host preparation guard"

require_text "src/self_hosted/README.md" "This directory is not the compiler core."
require_text "src/self_hosted/README.md" "A tool that does not yet pass this contract is a *scaffold*"
require_text "src/self_hosted/PROGRESS.md" "Compiler-internal substitution"
require_text "src/self_hosted/PROGRESS.md" "Hard self-host contract"
require_text "src/self_hosted/PROGRESS.md" "Peripheral Audit Tools (Not Counted In Coverage)"
require_text "src/self_hosted/PROGRESS.md" "Do **not** add peripheral audit tools to the substitution percentage."
require_text "src/self_hosted/PROGRESS.md" "not yet a self-hosted compiler AST model"
require_text "src/self_hosted/PROGRESS.md" "FFI remains intentionally absent from the compiler-pass path"
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
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_code.json" "\"codes\":"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_code.json" "\"missing\":1"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_code.json" "\"kind\":\"missing\""
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_code.json" "PGY_FAKE_DRIFT_FOR_SELFHOST"
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_input.json" "\"kind\":\"input_error\""
require_text "src/self_hosted/tools/diagnostic_catalog_checker/expected/missing_input.json" "src/semantic/diag_codes.h"
require_text "src/self_hosted/tools/air_graph_json_validator/intent.md" "Args -> ENV -> 0x20"
require_text "src/self_hosted/tools/air_graph_json_validator/main.pgy" "env_effect_sites"
require_text "src/self_hosted/tools/air_graph_json_validator/fixture/cap_env.json" "\"op\":\"Args\",\"effect\":\"ENV\",\"capability_mask\":\"0x20\""
require_text "src/self_hosted/tools/air_graph_json_validator/expected/clean.json" "\"env_effect_sites\":1"
require_text "src/self_hosted/parity/README.md" "Compiler-core self-host migration from this folder is allowed only as a verified"
require_text "src/self_hosted/parity/air_graph_json_validator_parity.sh" "counts.env_effect_sites parity"
require_text "src/self_hosted/parity/air_graph_json_validator_parity.sh" "cap_env fixture drifted"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "Rung 2 parity"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "tests/diagnostic_registry_smoke.sh"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "pgy.selfhost.diagnostic-catalog.v1"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "counts.documented parity"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "counts.missing parity"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "counts.orphans parity"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "clean JSON parity"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "missing-code fixture"
require_text "src/self_hosted/parity/diagnostic_catalog_checker_parity.sh" "missing-input fixture"
require_text "src/self_hosted/lexer/main.pgy" "import \"char_owner.pgy\""
require_text "src/self_hosted/lexer/main.pgy" "import \"token_owner.pgy\""
require_text "src/self_hosted/lexer/main.pgy" "import \"scan_owner.pgy\""
require_text "src/self_hosted/lexer/char_owner.pgy" "func IsAlphaCode"
require_text "src/self_hosted/lexer/token_owner.pgy" "func KeywordType"
require_text "src/self_hosted/lexer/token_owner.pgy" "func TokenLine"
require_text "src/self_hosted/lexer/scan_owner.pgy" "func LexContent"
require_text "src/self_hosted/parser/main.pgy" "import \"error_owner.pgy\""
require_text "src/self_hosted/parser/main.pgy" "import \"cursor_owner.pgy\""
require_text "src/self_hosted/parser/main.pgy" "import \"tree_text_owner.pgy\""
require_text "src/self_hosted/parser/main.pgy" "import \"type_name_owner.pgy\""
require_text "src/self_hosted/parser/main.pgy" "import \"expr_string_owner.pgy\""
require_text "src/self_hosted/parser/main.pgy" "import \"expr_primary_owner.pgy\""
require_text "src/self_hosted/parser/main.pgy" "import \"expr_precedence_owner.pgy\""
require_text "src/self_hosted/parser/error_owner.pgy" "func Fail"
require_text "src/self_hosted/parser/cursor_owner.pgy" "func SkipWhitespaceAndComments"
require_text "src/self_hosted/parser/cursor_owner.pgy" "func ReadIdent"
require_text "src/self_hosted/parser/cursor_owner.pgy" "func MatchKeyword"
require_text "src/self_hosted/parser/cursor_owner.pgy" "func ReadNumber"
require_text "src/self_hosted/parser/cursor_owner.pgy" "func Expect"
require_text "src/self_hosted/parser/tree_text_owner.pgy" "func IndentStr"
require_text "src/self_hosted/parser/tree_text_owner.pgy" "func AppendLine"
require_text "src/self_hosted/parser/tree_text_owner.pgy" "func AppendImplicitMain"
require_text "src/self_hosted/parser/type_name_owner.pgy" "func IsPrimitiveType"
require_text "src/self_hosted/parser/type_name_owner.pgy" "func ReadType"
require_text "src/self_hosted/parser/expr_string_owner.pgy" "func DesugarStringInterpolation"
require_text "src/self_hosted/parser/expr_string_owner.pgy" "func DesugarDollarStringInterpolation"
require_text "src/self_hosted/parser/expr_primary_owner.pgy" "func ParsePrimary"
require_text "src/self_hosted/parser/expr_precedence_owner.pgy" "func ParseUnary"
require_text "src/self_hosted/parser/expr_precedence_owner.pgy" "func ParseExpr"
require_text "src/self_hosted/semantic/intent.md" "function return types"
require_text "src/self_hosted/semantic/intent.md" "C compiler accept/reject oracle"
require_text "src/self_hosted/lib/diagnostic.pgy" "namespace SelfHostDiagnostic"
require_text "src/self_hosted/lib/diagnostic.pgy" "RenderError"
require_text "src/self_hosted/lib/diagnostic.pgy" "Diagnostic: "
require_text "src/self_hosted/semantic/main.pgy" "Status: rung-2"
require_text "src/self_hosted/semantic/main.pgy" "import \"program_check_owner.pgy\""
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "pgy.selfhost.semantic.v1"
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "SelfHostDiagnostic_RenderError"
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "let_type_mismatch"
require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "return_type_mismatch"
require_text "src/self_hosted/semantic/expr_type_owner.pgy" "func ExprType"
require_text "src/self_hosted/semantic/call_check_owner.pgy" "func CheckCall"
require_text "src/self_hosted/semantic/body_check_owner.pgy" "func CheckBody"
require_text "src/self_hosted/semantic/program_check_owner.pgy" "func CheckProgram"
require_text "src/self_hosted/parity/semantic_parity.sh" "Rung 2 parity"
require_text "src/self_hosted/parity/semantic_parity.sh" "C compiler"
require_text "src/self_hosted/parity/semantic_parity.sh" "PGY_SELFHOST_SEMANTIC_BACKENDS"
require_text "src/self_hosted/parity/semantic_parity.sh" "src/self_hosted/semantic/\"*.pgy"
require_text "src/self_hosted/parity/semantic_parity.sh" "raw semantic text leaked"
require_text "src/self_hosted/parity/semantic_parity.sh" "JSON semantic output leaked"

forbid_text "docs/self_hosted/05_compiler_core_gap_analysis.md" "hard self-host can start from the compiler core"
forbid_text "TODO.md" "hard self-host can start from the compiler core"

echo "[self-host-preparation] hard self-host guard and soft-entry criteria ok"
