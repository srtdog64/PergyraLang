#!/usr/bin/env bash
# Gates the hard self-host contract: SoT closure is a substitution pass
# condition, C/LLVM remain the oracle pair, and active hard rungs stay wired
# into the preparation gate.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[self-host-hard-contract] $*" >&2
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

source "$ROOT_DIR/tests/self_hosted/compiler_world_manifest.sh"

require_file "docs/self_hosted/10_hard_self_host_contract.md"
require_file "docs/self_hosted/11_compiler_world_architecture.md"
require_file "docs/self_hosted/12_intent_zone_self_host_architecture.md"
require_file "tests/self_host_hard_contract_smoke.sh"
require_file "tests/self_hosted/compiler_world_manifest.sh"

pgy_compiler_world_require_manifest_paths "$ROOT_DIR" ||
    fail "compiler world path manifest is incomplete"

for term in \
    "Hard self-host is active as staged substitution" \
    "SoT is not a separate cleanup project during hard self-host. It is a pass" \
    "condition." \
    "the C compiler remains the primary oracle during hard substitution" \
    "LLVM remains the second oracle whenever the current build enables it" \
    "Bridge code is allowed. Fallback is not." \
    "self-hosted code rereading source AST text" \
    "missing MIR fact" \
    "Hard substitution"; do
    require_text "docs/self_hosted/10_hard_self_host_contract.md" "$term"
done

require_text "docs/INDEX.md" "self_hosted/10_hard_self_host_contract.md"
require_text "docs/self_hosted/README.md" "10_hard_self_host_contract.md"
require_text "docs/self_hosted/00_agent_entry.md" \
    "Keep the C compiler as the oracle during soft, partial, and hard substitution work."
require_text "src/self_hosted/README.md" \
    "The C compiler remains the oracle during soft, partial, and hard substitution."
require_text "tests/self_hosted/parity/README.md" \
    "Hard substitution rungs are parity gates promoted to pass conditions"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    '`src/self_hosted/` is for Pergyra source and owner documentation.'
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    '`tests/self_hosted/` is for parity scripts, committed fixtures, expected'
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "## Compiler World Rule"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "src/self_hosted/compiler/world.pgy"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "PgyCompilerWorld"
require_text "docs/self_hosted/11_compiler_world_architecture.md" \
    'PgyCompilerWorld` is the self-host compiler owner.'
require_text "src/self_hosted/compiler/world.pgy" \
    "world PgyCompilerWorld"
require_text "src/self_hosted/compiler/world.pgy" \
    "intent CompilePergyraProgram"
require_text "src/self_hosted/compiler/world.pgy" \
    "intent ProveSelfHostedParity"
require_text "src/self_hosted/compiler/stage_intents.pgy" \
    "intent FrontendPipeline"
require_text "src/self_hosted/compiler/stage_intents.pgy" \
    "intent BackendPipeline"
require_text "tests/self_hosted/README.md" \
    '`src/self_hosted/` is for Pergyra source owners.'
require_text "src/self_hosted/PROGRESS.md" \
    "Hard self-host contract"

require_text "Makefile" "self-host-hard-contract-test-smoke"
require_text "Makefile" "self-host-compiler-world-contract-test-smoke"
require_text "Makefile" "self-host-preparation-contract-test-smoke"
require_text "Makefile" "self-host-preparation-parity-test-smoke"
require_text "Makefile" "self-host-preparation-impact-test-smoke"
require_text "Makefile" "self-host-preparation-impact-changed-paths-test-smoke"
require_text "Makefile" "self-host-completeness-smoke"
require_text "Makefile" "self-host-completeness-incremental-cache-parity-test-smoke"
require_text "Makefile" "tests/self_host_hard_contract_smoke.sh"
require_text "Makefile" "tests/self_host_compiler_world_contract_smoke.sh"
require_text "Makefile" "tests/self_hosted/parity/completeness_ledger.sh"
require_text "Makefile" "tests/self_hosted/parity/completeness_incremental_cache_parity.sh"
require_text "Makefile" "self-host-preparation-test-smoke:"
require_text "tests/self_hosted/parity/completeness_incremental_cache_parity.sh" \
    "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/completeness_incremental_cache_parity.sh" \
    "self-host-incremental-cache-parity"
require_text "src/self_hosted/compiler/incremental_fact_graph_owner.pgy" \
    "CompilerIncrementalCacheParitySuiteName"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "Normal compiler builds must not imply the heavy self-host parity bundle."
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "self-host-completeness-smoke"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "self-host-completeness-incremental-cache-parity-test-smoke"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "self-host-preparation-impact-test-smoke"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "self-host-preparation-impact-changed-paths-test-smoke"
require_file "src/self_hosted/compiler/completeness_ledger_owner.pgy"
require_file "tests/self_hosted/parity/completeness_ledger.sh"
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    "CompilerCompletenessLedgerSchema"
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    "CompilerCompletenessSourceInventory"
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    "CompilerCompletenessLexParsePassMin"
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    "CompilerCompletenessLexParseSemanticPassMin"
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    "CompilerCompletenessFullPipelinePassMin"
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    "EmitCompilerCompletenessFullPipelineBaseline"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" \
    "EmitCompilerCompletenessSourceInventory();"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" \
    "EmitCompilerCompletenessFullPipelineBaseline();"
forbid_text "src/self_hosted/compiler/test_harness_manifest.pgy" \
    "EmitSelfHostCompletenessSources"
forbid_text "src/self_hosted/compiler/test_harness_manifest.pgy" \
    "EmitSelfHostCompletenessFullPipelineBaseline"
require_text "tests/self_hosted/parity/completeness_ledger.sh" \
    "Out-of-subset codegen is a measured failure, not a skip."
require_text "tests/self_hosted/parity/completeness_ledger.sh" \
    "pipeline identity regressed"
require_text "tests/self_hosted/parity/completeness_ledger.sh" \
    "lex_parse_semantic"
require_text "tests/self_hosted/parity/completeness_ledger.sh" \
    "PGY_SELFHOST_COMPLETENESS_STAGES"
require_text "tests/self_hosted/parity/completeness_ledger.sh" \
    "focused ledger ok"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "platform CI step lists must not set this variable directly."
for rel in \
    "Makefile" \
    "scripts/ci_linux_steps.sh" \
    "scripts/ci_windows_steps.sh" \
    "scripts/ci_macos_steps.sh"; do
    require_file "$rel"
    forbid_text "$rel" "PGY_SELFHOST_COMPLETENESS_STAGES"
done

active_stages=(lexer parser semantic codegen)
for stage in "${active_stages[@]}"; do
    require_file "src/self_hosted/$stage/main.pgy"
    require_file "src/self_hosted/$stage/intent.md"
    require_file "tests/self_hosted/parity/${stage}_parity.sh"
    require_text "Makefile" "self-host-${stage}-parity-test-smoke"
    require_text "Makefile" "tests/self_hosted/parity/${stage}_parity.sh"
    require_text "src/self_hosted/$stage/intent.md" "## Oracle"
done

for rel in \
    "tests/self_hosted/parity/codegen_bootstrap.sh" \
    "tests/self_hosted/parity/mir_json_parity.sh"; do
    require_file "$rel"
    require_text "$rel" "C oracle"
done

require_file "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh"
require_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" \
    "Pergyra-origin backend parity fuzz smoke"
require_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" \
    "generator parity ok"

require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "FIXPOINT"
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "codegen compiles mir_lower"
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "mir_lower_via_codegen.c"
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "fuzz backend parity generator"
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "fuzz_generator_via_codegen.c"
require_text "tests/self_hosted/parity/mir_json_parity.sh" \
    "MIR JSON facts"
require_text "tests/self_hosted/parity/mir_json_parity.sh" \
    "must not read transitional ast compatibility text"
require_text "src/self_hosted/mir_lower/main.pgy" \
    "source_type"
require_text "src/self_hosted/mir_lower/main.pgy" \
    "source_locals"
forbid_text "src/self_hosted/mir_lower/main.pgy" \
    "JsonFieldString(json, kp, inst_end, \"\\\"ast\\\":\")"
forbid_text "src/self_hosted/mir_lower/main.pgy" \
    "StringLength(ast)"

echo "[self-host-hard-contract] hard substitution contract is wired"
