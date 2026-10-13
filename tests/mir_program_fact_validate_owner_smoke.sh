#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ORCHESTRATOR="$ROOT_DIR/src/compiler/mir_program_validate.c"
FACT_VALIDATOR="$ROOT_DIR/src/compiler/mir_program_fact_validate.c"

fail() {
    echo "[mir-program-fact-validate-owner] $*" >&2
    exit 1
}

for owner in "$ORCHESTRATOR" "$FACT_VALIDATOR"; do
    [[ -f "$owner" ]] || fail "missing owner: $owner"
done
for owner in "$ORCHESTRATOR" "$FACT_VALIDATOR"; do
    lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "$owner")" || fail "source-size measurement failed"
    (( lines <= 699 )) || fail "owner exceeds 699 LOC: $owner ($lines)"
done

echo "[mir-program-fact-validate-owner] owner files present within the 699 LOC cap"
