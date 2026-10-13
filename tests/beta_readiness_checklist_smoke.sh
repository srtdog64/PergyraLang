#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[beta-readiness-checklist] $*" >&2
    exit 1
}

require_max_lines() {
    local rel="$1"
    local max_lines="$2"
    local line_count

    line_count="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "$ROOT_DIR/$rel")" || fail "source-size measurement failed: $rel"
    [[ "$line_count" -le "$max_lines" ]] ||
        fail "$rel has $line_count lines; expected <= $max_lines"
}

require_max_lines "docs/100_beta_readiness_checklist.md" 120
for rel in \
    "docs/100a_beta_active_status.md" \
    "docs/100b_beta_p0_semantics_systems_air.md" \
    "docs/100c_beta_dag_mir_abi_runtime.md" \
    "docs/100d_beta_execution_log.md"; do
    require_max_lines "$rel" 2600
done

echo "beta readiness checklist smoke: ok"
