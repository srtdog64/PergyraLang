#!/usr/bin/env bash
# MIR must retain the HIR function-parameter summary rows by routine identity;
# malformed storage or a missing owner flag must fail validation.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bash "$ROOT_DIR/tests/mir_program_fact_validate_owner_smoke.sh"
MIR_TEST="${MIR_TEST_BIN:-$ROOT_DIR/bin/test_mir}"

if [[ ! -x "$MIR_TEST" ]]; then
    echo "[mir-function-param-flow] missing MIR test binary: $MIR_TEST" >&2
    exit 1
fi

OUTPUT="$($MIR_TEST)"
grep -Fq -- "MIR carries HIR function parameter flow summaries by stable identity" <<<"$OUTPUT"

echo "[mir-function-param-flow] HIR summary rows reach MIR with stable identity and fail-closed validation"
