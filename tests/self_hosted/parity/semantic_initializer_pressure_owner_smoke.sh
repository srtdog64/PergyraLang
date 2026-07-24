#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy"

fail() {
    echo "[self-host-parity:semantic-initializer-pressure] $*" >&2
    exit 1
}

grep -Fq -- "func SemanticAstInitializerTypePressureRow(" "$OWNER" ||
    fail "row pressure formatting owner is missing"
grep -Fq -- "if enabled {" "$OWNER" ||
    fail "row pressure formatting is not guarded"
for stage in start environment graph-root verdict done; do
    grep -Fq -- "observe_pressure, \"row:$stage\", i" "$OWNER" ||
        fail "row pressure stage is not routed through the owner: $stage"
done
if grep -Fq -- 'Concat("row:' "$OWNER"; then
    fail "row pressure call site constructs a marker before the observation guard"
fi

echo "[self-host-parity:semantic-initializer-pressure] row marker allocation is observation-owned"
