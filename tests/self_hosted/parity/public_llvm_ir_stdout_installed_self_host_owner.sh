#!/usr/bin/env bash
# Exact public --emit-llvm stdout streams one installed LLVM artifact without
# whole-artifact buffering or native semantic/libLLVM fallback.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-cc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/public_llvm_ir_stdout"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE="src/self_hosted/mir_lower/fixture/option_struct_value_flow.pgy"
COUNT_FILE="$WORK_DIR/count.txt"

fail() { echo "[self-host-public-llvm-stdout] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }

[[ -x "$PGY" && -x "$SELF_DRIVER" ]] || fail "missing public or installed driver"
command -v "$CC" >/dev/null || fail "missing C compiler"
command -v "$CLANG" >/dev/null || fail "missing clang"
PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"
suffix=""; installed_name="pgy-self-driver"
if [[ "$PGY" == *.exe ]]; then suffix=".exe"; installed_name="pgy-self-driver.exe"; fi
[[ "$SELF_DRIVER" == "$(dirname "$PGY")/$installed_name" ]] ||
    fail "self-host driver is not installed beside the public launcher"

mkdir -p "$WORK_DIR/counting-install" "$WORK_DIR/missing-install"
rm -f "$WORK_DIR"/*.{ll,out,err,exe} "$COUNT_FILE"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN &&
    "$PGY" "$SOURCE" --emit-llvm) >"$WORK_DIR/stdout.ll" 2>"$WORK_DIR/stdout.err"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN &&
    "$PGY" "$SOURCE" --emit-llvm -o "$WORK_REL/file.ll") \
    >"$WORK_DIR/file.out" 2>"$WORK_DIR/file.err"
cmp -s "$WORK_DIR/file.ll" "$WORK_DIR/stdout.ll" ||
    fail "stdout LLVM IR differs from the closed public file form"
"$CLANG" "$WORK_DIR/stdout.ll" -o "$WORK_DIR/stdout-program$suffix" \
    2>"$WORK_DIR/clang.err"
"$WORK_DIR/stdout-program$suffix" | tr -d '\r' >"$WORK_DIR/program.out"
printf '7\n11\n5\n' >"$WORK_DIR/expected.out"
cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/program.out" ||
    fail "stdout LLVM IR did not execute exact 7/11/5"

cp "$PGY" "$WORK_DIR/counting-install/pgy$suffix"
"$CC" -std=c11 -Wall -Wextra -Werror \
    "$ROOT_DIR/tests/self_hosted/parity/fixture/counting_self_host_llvm_driver.c" \
    -o "$WORK_DIR/counting-install/$installed_name"
count_for_driver="$COUNT_FILE"
[[ "$PGY" != *.exe ]] || count_for_driver="$(cygpath -m "$COUNT_FILE")"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN; PGY_DEBUG_PIPELINE_TIMING=1 \
    PGY_SELF_DRIVER_COUNT_FILE="$count_for_driver" \
    "$WORK_DIR/counting-install/pgy$suffix" "$SOURCE" --emit-llvm) \
    >"$WORK_DIR/counting.ll" 2>"$WORK_DIR/counting.err"
printf 'producer\nbackend\n' >"$WORK_DIR/count.expected"
cmp -s "$WORK_DIR/count.expected" "$COUNT_FILE" ||
    fail "stdout did not invoke producer/backend exactly once"
[[ -s "$WORK_DIR/counting.ll" ]] || fail "stdout published no LLVM IR"
! grep -Fq '[pipeline timing]' "$WORK_DIR/counting.err" ||
    fail "stdout re-entered the native compiler pipeline"

reject_stdout() {
    local mode="$1" expected="$2" diagnostic="$3"
    rm -f "$COUNT_FILE" "$WORK_DIR/$mode.ll"
    set +e
    (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN; \
        PGY_SELF_DRIVER_COUNT_FILE="$count_for_driver" \
        PGY_SELF_DRIVER_LLVM_MODE="$mode" \
        "$WORK_DIR/counting-install/pgy$suffix" "$SOURCE" --emit-llvm) \
        >"$WORK_DIR/$mode.ll" 2>"$WORK_DIR/$mode.err"
    local rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -s "$WORK_DIR/$mode.ll" ]] ||
        fail "$mode emitted LLVM payload before failure"
    printf '%b' "$expected" >"$WORK_DIR/$mode.expected"
    cmp -s "$WORK_DIR/$mode.expected" "$COUNT_FILE" ||
        fail "$mode invocation count drifted"
    grep -Fq "$diagnostic" "$WORK_DIR/$mode.err" || fail "$mode lost its diagnostic"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/$mode.err" ||
        fail "$mode re-entered the native pipeline"
}
reject_stdout producer-fail 'producer\n' 'self-host MIR producer failed with code 7'
reject_stdout backend-fail 'producer\nbackend\n' 'self-host LLVM projector failed with code 9'

cp "$PGY" "$WORK_DIR/missing-install/pgy$suffix"
set +e
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN &&
    "$WORK_DIR/missing-install/pgy$suffix" "$SOURCE" --emit-llvm) \
    >"$WORK_DIR/missing.ll" 2>"$WORK_DIR/missing.err"
missing_rc=$?
(cd "$ROOT_DIR" && "$PGY" "$SOURCE" --emit-llvm --runtime=none) \
    >"$WORK_DIR/unsupported.ll" 2>"$WORK_DIR/unsupported.err"
unsupported_rc=$?
set -e
[[ "$missing_rc" -ne 0 && ! -s "$WORK_DIR/missing.ll" ]] ||
    fail "missing sibling emitted native LLVM fallback"
[[ "$unsupported_rc" -ne 0 && ! -s "$WORK_DIR/unsupported.ll" ]] ||
    fail "unsupported stdout emitted native LLVM fallback"
grep -Fq 'self-host driver is unavailable' "$WORK_DIR/missing.err" ||
    fail "missing sibling lost its diagnostic"
grep -Fq 'outside the installed self-host driver contract' \
    "$WORK_DIR/unsupported.err" || fail "unsupported stdout lost its diagnostic"

owner="$ROOT_DIR/src/compiler/self_host_llvm_ir_stdout_owner.c"
require_text "$ROOT_DIR/src/pgy_driver.c" 'driver_self_host_llvm_ir_stdout_request_supported'
require_text "$ROOT_DIR/src/pgy_driver.c" 'return driver_write_self_host_llvm_ir_stdout('
require_text "$owner" 'driver_materialize_self_host_llvm_artifacts('
require_text "$owner" 'unsigned char buffer[16384];'
! grep -Eq 'path_read_file\(|malloc\(|realloc\(|strstr\(|driver_run_pipeline\(|compiler_emit_llvm_ir' "$owner" ||
    fail "stdout owner buffered/reinterpreted LLVM or regained native authority"
require_text "$ROOT_DIR/Makefile" 'self-host-public-llvm-ir-stdout-replacement-test-smoke:'

echo "[self-host-public-llvm-stdout] installed DirectMirLlvm streams exact public stdout"
