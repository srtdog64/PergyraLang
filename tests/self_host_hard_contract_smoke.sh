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

require_file "docs/self_hosted/10_hard_self_host_contract.md"
require_file "tests/self_host_hard_contract_smoke.sh"

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
require_text "src/self_hosted/parity/README.md" \
    "Hard substitution rungs are parity gates promoted to pass conditions"
require_text "src/self_hosted/PROGRESS.md" \
    "Hard self-host contract"

require_text "Makefile" "self-host-hard-contract-test-smoke"
require_text "Makefile" "tests/self_host_hard_contract_smoke.sh"
require_text "Makefile" "self-host-preparation-test-smoke:"

active_stages=(lexer parser semantic codegen)
for stage in "${active_stages[@]}"; do
    require_file "src/self_hosted/$stage/main.pgy"
    require_file "src/self_hosted/$stage/intent.md"
    require_file "src/self_hosted/parity/${stage}_parity.sh"
    require_text "Makefile" "self-host-${stage}-parity-test-smoke"
    require_text "Makefile" "src/self_hosted/parity/${stage}_parity.sh"
    require_text "src/self_hosted/$stage/intent.md" "## Oracle"
done

for rel in \
    "src/self_hosted/parity/codegen_bootstrap.sh" \
    "src/self_hosted/parity/mir_json_parity.sh"; do
    require_file "$rel"
    require_text "$rel" "C oracle"
done

require_text "src/self_hosted/parity/codegen_bootstrap.sh" "FIXPOINT"
require_text "src/self_hosted/parity/mir_json_parity.sh" \
    "MIR JSON facts"
require_text "src/self_hosted/parity/mir_json_parity.sh" \
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
