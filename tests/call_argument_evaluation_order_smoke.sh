#!/usr/bin/env bash
# Call arguments evaluate left to right on native C/LLVM and on the default
# C/LLVM route, for user and builtin calls alike. C leaves the order
# unspecified and GCC evaluated right to left, so native C disagreed with
# LLVM whenever one argument had an effect another argument saw.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here call-argument-evaluation-order "$PGY"
cd "$ROOT_DIR"
WORK="$(mktemp -d .tmp/call-argument-order.XXXXXX)"
FIXTURE="tests/self_hosted/fixtures/call_argument_evaluation_order.pgy"
printf 'a\nb\nc\n3\n22\n12\nx\ny\nxy\nhello\np\nq\ne\n23\n' >"$WORK/expected"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here call-argument-evaluation-order "$DRIVER"

for leg in native-c native-llvm default-c default-llvm; do
    backend="${leg#*-}"
    route=()
    [[ "$leg" == native-* ]] && route=(--native-pipeline)
    PGY_SELF_DRIVER_BIN="$DRIVER" "$PGY" "$FIXTURE" ${route[@]+"${route[@]}"} --backend="$backend" \
        -o "$WORK/$leg.exe" >"$WORK/$leg.compile.log" 2>&1 || {
            cat "$WORK/$leg.compile.log" >&2
            echo "[call-argument-order] $leg did not compile the fixture" >&2
            exit 1
        }
    "$WORK/$leg.exe" | tr -d '\r' >"$WORK/$leg.out"
    cmp -s "$WORK/expected" "$WORK/$leg.out" || {
        echo "[call-argument-order] $leg evaluated arguments in another order" >&2
        diff -u "$WORK/expected" "$WORK/$leg.out" >&2 || true
        exit 1
    }
done

# Method calls and ArraySet: the default LLVM route does not admit this
# program's shape yet (a refusal, not a wrong order), so it is not a leg.
METHOD_FIXTURE="tests/self_hosted/fixtures/call_argument_evaluation_order_method.pgy"
printf 'a\nb\n111\nc\nd\ne\n6\n' >"$WORK/method.expected"
for leg in native-c native-llvm default-c; do
    backend="${leg#*-}"
    route=()
    [[ "$leg" == native-* ]] && route=(--native-pipeline)
    PGY_SELF_DRIVER_BIN="$DRIVER" "$PGY" "$METHOD_FIXTURE" ${route[@]+"${route[@]}"} --backend="$backend" \
        -o "$WORK/method-$leg.exe" >"$WORK/method-$leg.compile.log" 2>&1 || {
            cat "$WORK/method-$leg.compile.log" >&2
            echo "[call-argument-order] $leg did not compile the method fixture" >&2
            exit 1
        }
    "$WORK/method-$leg.exe" | tr -d '\r' >"$WORK/method-$leg.out"
    cmp -s "$WORK/method.expected" "$WORK/method-$leg.out" || {
        echo "[call-argument-order] $leg evaluated method or ArraySet arguments in another order" >&2
        diff -u "$WORK/method.expected" "$WORK/method-$leg.out" >&2 || true
        exit 1
    }
done

echo '[call-argument-order] user, builtin and method call arguments evaluate left to right on native and default C/LLVM: PASS'
