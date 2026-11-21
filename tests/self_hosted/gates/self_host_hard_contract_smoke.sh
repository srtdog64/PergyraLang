#!/usr/bin/env bash
# Gates the hard self-host contract: SoT closure is a substitution pass
# condition, C/LLVM remain the oracle pair, and active hard rungs stay wired
# into the preparation gate.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

fail() {
    echo "[self-host-hard-contract] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing $rel"
}

bash "$ROOT_DIR/tests/ir/mir_json_expression_graph_owner_smoke.sh"

require_file \
    "src/self_hosted/compiler/direct_mir/direct_mir_exact_json_array_cardinality_owner.pgy"
source_size_lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" src/self_hosted/compiler/direct_mir/direct_mir_exact_json_array_cardinality_owner.pgy)" || fail "source-size measurement failed"
[[ "$source_size_lines" -le 90 ]] || \
    fail "exact JSON-array cardinality owner hard cap exceeded"

echo "[self-host-hard-contract] hard substitution contract is wired"
