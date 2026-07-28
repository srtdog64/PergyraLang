#!/usr/bin/env bash
set -euo pipefail

# Frontend-only closure. The separate execution gate owns exact result/type MIR
# carriage and dynamic expect; typed variant branching and compensation remain
# a later rung. This gate proves the source spelling is not collapsed.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PARSER="${PGY_SELFHOST_PREBUILT_PARSER:-}"
BUILD_DIR="${PGY_SELFHOST_INTENT_OUTCOME_FRONTEND_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_outcome_frontend}"
FIXTURE="tests/self_hosted/parity/fixture/intent_outcome_frontend.pgy"

fail() {
    echo "[self-host-intent-outcome-frontend] $*" >&2
    exit 1
}

mkdir -p "$BUILD_DIR"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-intent-outcome-frontend" "$PGY" \
    || fail "PGY_BIN is not runnable"

if [[ -n "$PARSER" ]]; then
    PARSER="$(pgy_select_optional_exe_binary "$PARSER")"
    pgy_require_runnable_binary_here "self-host-intent-outcome-frontend" "$PARSER" \
        || fail "prebuilt parser is not runnable"
else
    PARSER="$BUILD_DIR/parser.exe"
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/parser/main.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$PARSER")" \
        >"$BUILD_DIR/parser.compile.log" 2>&1) \
        || { cat "$BUILD_DIR/parser.compile.log" >&2; fail "parser build failed"; }
fi

AST_OUT="$BUILD_DIR/positive.ast"
(cd "$ROOT_DIR" && "$PARSER" "$FIXTURE" >"$AST_OUT" 2>"$BUILD_DIR/positive.err") \
    || { cat "$BUILD_DIR/positive.err" >&2; fail "outcome spelling was rejected"; }

grep -Fq '      Outcome: outcome' "$AST_OUT" \
    || fail "outcome binding was not preserved as its own typed-AST row"
grep -Fq '      On: worker.Finish()' "$AST_OUT" \
    || fail "on action expression was changed or lost"
grep -Fq '      Compensate: ToString(outcome)' "$AST_OUT" \
    || fail "compensation expression was lost"
grep -Fq '      Post: (outcome + 1)' "$AST_OUT" \
    || fail "post expression was lost"
grep -Fq '      Expect: (outcome == 7)' "$AST_OUT" \
    || fail "expect expression was lost"
if [[ "$(grep -Fc '      Outcome:' "$AST_OUT")" -ne 1 ]]; then
    fail "legacy on: spelling fabricated an outcome binding"
fi
if [[ "$(grep -Fc '      On: worker.Finish()' "$AST_OUT")" -ne 2 ]]; then
    fail "legacy on: compatibility drifted"
fi

INVALID="$BUILD_DIR/invalid-binding.pgy"
sed 's/on outcome:/on 7:/' "$ROOT_DIR/$FIXTURE" >"$INVALID"
if (cd "$ROOT_DIR" && "$PARSER" "${INVALID#"$ROOT_DIR"/}" \
        >"$BUILD_DIR/invalid.out" 2>"$BUILD_DIR/invalid.err"); then
    fail "non-identifier outcome binding was accepted"
fi
if grep -Fq 'Outcome:' "$BUILD_DIR/invalid.out" "$BUILD_DIR/invalid.err"; then
    fail "invalid outcome binding emitted a partial AST"
fi

DUPLICATE="$BUILD_DIR/duplicate-on.pgy"
sed 's/on outcome: worker.Finish();/on first: worker.Finish();\
        on second: worker.Finish();/' "$ROOT_DIR/$FIXTURE" >"$DUPLICATE"
if (cd "$ROOT_DIR" && "$PARSER" "${DUPLICATE#"$ROOT_DIR"/}" \
        >"$BUILD_DIR/duplicate.out" 2>"$BUILD_DIR/duplicate.err"); then
    fail "duplicate bound on clauses were accepted"
fi
if grep -Fq 'Outcome:' "$BUILD_DIR/duplicate.out" "$BUILD_DIR/duplicate.err"; then
    fail "duplicate bound on clauses emitted a partial AST"
fi

echo "[self-host-intent-outcome-frontend] lossless on-binding AST + negative: PASS"
