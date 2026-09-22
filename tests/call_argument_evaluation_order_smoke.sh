#!/usr/bin/env bash
# Call arguments evaluate left to right on native C and LLVM. C leaves the
# order unspecified and GCC evaluated right to left, so native C disagreed
# with LLVM whenever one argument had an effect another argument saw. The
# default C route (self-host codegen) still emits arguments in place and is
# not a leg yet.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here call-argument-evaluation-order "$PGY"
cd "$ROOT_DIR"
WORK="$(mktemp -d .tmp/call-argument-order.XXXXXX)"
FIXTURE="tests/self_hosted/fixtures/call_argument_evaluation_order.pgy"
printf 'a\nb\nc\n3\n22\n12\n' >"$WORK/expected"

for backend in c llvm; do
    "$PGY" "$FIXTURE" --native-pipeline --backend="$backend" \
        -o "$WORK/$backend.exe" >"$WORK/$backend.compile.log" 2>&1 || {
            cat "$WORK/$backend.compile.log" >&2
            echo "[call-argument-order] $backend did not compile the fixture" >&2
            exit 1
        }
    "$WORK/$backend.exe" | tr -d '\r' >"$WORK/$backend.out"
    cmp -s "$WORK/expected" "$WORK/$backend.out" || {
        echo "[call-argument-order] $backend evaluated arguments in another order" >&2
        diff -u "$WORK/expected" "$WORK/$backend.out" >&2 || true
        exit 1
    }
done

echo '[call-argument-order] native C and LLVM evaluate call arguments left to right: PASS'
