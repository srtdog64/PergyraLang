#!/usr/bin/env bash
# Native C and LLVM must each match an external decimal oracle; backend
# agreement alone cannot detect a shared rounded-AST error.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here long-literal-exactness "$PGY"
cd "$ROOT_DIR"
WORK="$(mktemp -d .tmp/long-literal-exactness.XXXXXX)"
GOOD="tests/self_hosted/fixtures/long_literal_exact_boundary.pgy"
BAD="tests/self_hosted/fixtures/long_literal_overflow_negative.pgy"
printf '9223372036854775807\n9223372036854775806\n-9223372036854775808\n-9223372036854775808\n-9223372036854775808\n' >"$WORK/expected"

for backend in c llvm; do
    "$PGY" "$GOOD" --native-pipeline --backend="$backend" \
        -o "$WORK/$backend.exe" >"$WORK/$backend.compile.out" \
        2>"$WORK/$backend.compile.err"
    "$WORK/$backend.exe" | tr -d '\r' >"$WORK/$backend.out"
    cmp -s "$WORK/expected" "$WORK/$backend.out" || {
        echo "[long-literal-exactness] $backend differs from exact decimal oracle" >&2
        diff -u "$WORK/expected" "$WORK/$backend.out" >&2 || true
        exit 1
    }

    printf 'stale-output\n' >"$WORK/overflow-$backend.exe"
    set +e
    "$PGY" "$BAD" --native-pipeline --backend="$backend" \
        -o "$WORK/overflow-$backend.exe" >"$WORK/overflow-$backend.out" \
        2>"$WORK/overflow-$backend.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -e "$WORK/overflow-$backend.exe" ]] || {
        echo "[long-literal-exactness] $backend accepted an out-of-range Long" >&2
        exit 1
    }
    grep -Fq 'Long literal is outside the signed 64-bit range' \
        "$WORK/overflow-$backend.err" || {
        echo "[long-literal-exactness] $backend lost range diagnostic" >&2
        exit 1
    }
done

echo '[long-literal-exactness] C/LLVM exact values and overflow refusal: PASS'
