#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
WORK_DIR="$ROOT_DIR/.tmp/semantic_termination_security"
VALID_SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/forloop.pgy"
BUDGET_OUT="$WORK_DIR/budget.out"
BUDGET_ERR="$WORK_DIR/budget.err"
NUL_SOURCE="$WORK_DIR/embedded_nul.pgy"
NUL_OUT="$WORK_DIR/nul.out"
NUL_ERR="$WORK_DIR/nul.err"

fail() {
    echo "[semantic-termination-security] $*" >&2
    exit 1
}

mkdir -p "$WORK_DIR"

if PGY_SEMANTIC_STEP_BUDGET=1 "$PGY" --mir-json "$VALID_SOURCE" \
    >"$BUDGET_OUT" 2>"$BUDGET_ERR"; then
    fail "step-budget exhaustion was accepted"
fi
grep -Fq "semantic analysis exceeded its termination-contract step budget" \
    "$BUDGET_ERR" || fail "step-budget diagnostic was not emitted"

PGY_SEMANTIC_STEP_BUDGET=0 "$PGY" --mir-json "$VALID_SOURCE" \
    >"$BUDGET_OUT" 2>"$BUDGET_ERR" ||
    fail "explicitly disabled step budget rejected a valid program"

printf 'func Main() -> Void {\n    Log("visible");\000Log("hidden");\n}\n' \
    >"$NUL_SOURCE"
if "$PGY" --mir-json "$NUL_SOURCE" >"$NUL_OUT" 2>"$NUL_ERR"; then
    fail "embedded NUL source was accepted"
fi
grep -Fq "contains an embedded NUL byte" "$NUL_ERR" ||
    fail "embedded NUL diagnostic was not emitted"

echo "[semantic-termination-security] deterministic budget and embedded-NUL rejection ok"
