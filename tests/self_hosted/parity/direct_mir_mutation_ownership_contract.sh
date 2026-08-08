#!/usr/bin/env bash
# One-shot assembly consumes own; repeated updates use inout; refs do not escape.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-parity:direct-mir-mutation-ownership"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
BUILD_DIR="${PGY_SELFHOST_MUTATION_OWNERSHIP_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/direct_mir_mutation_ownership}"
POSITIVE="$ROOT_DIR/tests/self_hosted/fixtures/direct_mir_array_argument_mutation_ownership.pgy"
NEGATIVE="$ROOT_DIR/tests/self_hosted/fixtures/borrowed_ref_constructor_store_reject.pgy"
pgy_require_runnable_binary_here "$LABEL" "$PGY" || fail "PGY_BIN is not runnable"
[[ -f "$POSITIVE" && -f "$NEGATIVE" ]] || fail "ownership fixture is missing"
"$BASH" \
    "$ROOT_DIR/tests/self_hosted/parity/direct_mir_mutation_ownership_signature_contract.sh"
BUILD_DIR="$BUILD_DIR/run_$$"
mkdir -p "$BUILD_DIR"; POSITIVE_BIN="$BUILD_DIR/positive.exe"
(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$POSITIVE")" --native-pipeline \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$POSITIVE_BIN")" \
    >"$BUILD_DIR/positive.compile.log" 2>&1) || {
    cat "$BUILD_DIR/positive.compile.log" >&2
    fail "owned mutation import closure did not compile"
}
POSITIVE_BIN="$(pgy_select_optional_exe_binary "$POSITIVE_BIN")"
pgy_require_runnable_binary_here "$LABEL:positive" "$POSITIVE_BIN" ||
    fail "owned mutation fixture is not runnable"
(cd "$ROOT_DIR" && "$POSITIVE_BIN" >"$BUILD_DIR/positive.out") ||
    fail "owned mutation fixture failed"
grep -Fxq 'pgy.selfhost.direct-mir-array-argument-plan.v1' \
    "$BUILD_DIR/positive.out" || fail "owned mutation fixture output drifted"

if (cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$NEGATIVE")" --native-pipeline \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$BUILD_DIR/negative.exe")" \
    >"$BUILD_DIR/negative.compile.log" 2>&1); then
    fail "borrowed ref constructor store was accepted"
fi
[[ ! -e "$BUILD_DIR/negative.exe" ]] ||
    fail "negative fixture emitted an artifact before rejection"
grep -Fq "cannot escape through constructor field store 'BorrowedMutationEnvelope.plan'" \
    "$BUILD_DIR/negative.compile.log" || {
    cat "$BUILD_DIR/negative.compile.log" >&2
    fail "borrowed ref constructor-store diagnostic drifted"
}

echo "[$LABEL] owned mutation/assembly closure and borrowed-ref negative PASS"
