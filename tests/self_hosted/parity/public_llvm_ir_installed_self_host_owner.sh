#!/usr/bin/env bash
# Exact public --emit-llvm -o is published from installed source-MIR and LLVM
# actions. Native semantic/libLLVM fallback is forbidden for this file form.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/public_llvm_ir_installed"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE="src/self_hosted/mir_lower/fixture/option_struct_value_flow.pgy"
COUNT_FILE="$WORK_DIR/count.txt"

fail() { echo "[self-host-public-llvm-ir] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }

PGY="$(pgy_select_optional_exe_binary "$PGY")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "$SELF_DRIVER")"
[[ -x "$PGY" && -x "$SELF_DRIVER" ]] || fail "missing public or installed driver"
command -v "$CC" >/dev/null || fail "missing C compiler"
command -v "$CLANG" >/dev/null || fail "missing clang"
PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"
suffix=""; installed_name="pgy-self-driver"
if [[ "$PGY" == *.exe ]]; then
    suffix=".exe"; installed_name="pgy-self-driver.exe"
fi
[[ "$SELF_DRIVER" == "$(dirname "$PGY")/$installed_name" ]] ||
    fail "self-host driver is not installed beside the public launcher"

mkdir -p "$WORK_DIR/counting-install" "$WORK_DIR/missing-install"
rm -f "$WORK_DIR"/*.{ll,json,out,err,exe} "$COUNT_FILE"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN &&
    "$PGY" --mir-json "$SOURCE") >"$WORK_DIR/public.mir.json"
(cd "$ROOT_DIR" && "$SELF_DRIVER" --mir-json-backend=llvm \
    "$WORK_REL/public.mir.json" -o "$WORK_REL/direct.ll") \
    >"$WORK_DIR/direct.out" 2>"$WORK_DIR/direct.err"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN &&
    "$PGY" "$SOURCE" --emit-llvm -o "$WORK_REL/public.ll") \
    >"$WORK_DIR/public.out" 2>"$WORK_DIR/public.err"
cmp -s "$WORK_DIR/direct.ll" "$WORK_DIR/public.ll" ||
    fail "public LLVM IR differs from direct installed projection"
! grep -Eq '@pgy_|pgy_runtime_' "$WORK_DIR/public.ll" ||
    fail "runtime-free LLVM IR regained a runtime dependency"
"$CLANG" "$WORK_DIR/public.ll" -o "$WORK_DIR/public-program$suffix" \
    2>"$WORK_DIR/clang.err"
"$WORK_DIR/public-program$suffix" | tr -d '\r' >"$WORK_DIR/program.out"
printf '7\n11\n5\n' >"$WORK_DIR/expected.out"
cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/program.out" ||
    fail "published LLVM IR did not execute exact 7/11/5"

cp "$PGY" "$WORK_DIR/counting-install/pgy$suffix"
"$CC" -std=c11 -Wall -Wextra -Werror \
    "$ROOT_DIR/tests/self_hosted/parity/fixture/counting_self_host_llvm_driver.c" \
    -o "$WORK_DIR/counting-install/$installed_name"
# The shared helper asks the binary what it expects instead of trusting .exe.
count_for_driver="$(pgy_path_for_compiler "$PGY" "$COUNT_FILE")"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && PGY_DEBUG_PIPELINE_TIMING=1 \
    PGY_SELF_DRIVER_COUNT_FILE="$count_for_driver" \
    "$WORK_DIR/counting-install/pgy$suffix" "$SOURCE" --emit-llvm \
    -o "$WORK_REL/counting.ll") >"$WORK_DIR/counting.out" \
    2>"$WORK_DIR/counting.err"
printf 'producer\nbackend\n' >"$WORK_DIR/count.expected"
cmp -s "$WORK_DIR/count.expected" "$COUNT_FILE" ||
    fail "public file form did not invoke producer/backend exactly once"
[[ -s "$WORK_DIR/counting.ll" ]] || fail "counting path published no LLVM IR"
! grep -Fq '[pipeline timing]' "$WORK_DIR/counting.err" ||
    fail "public file form re-entered the native compiler pipeline"

reject_driver_failure() {
    local mode="$1" expected="$2" diagnostic="$3"
    local output="$WORK_DIR/$mode.ll"
    rm -f "$COUNT_FILE"; printf 'stale\n' >"$output"
    set +e
    (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN; \
        PGY_SELF_DRIVER_COUNT_FILE="$count_for_driver" \
        PGY_SELF_DRIVER_LLVM_MODE="$mode" \
        "$WORK_DIR/counting-install/pgy$suffix" "$SOURCE" --emit-llvm \
        -o "$WORK_REL/$mode.ll") >"$WORK_DIR/$mode.out" 2>"$WORK_DIR/$mode.err"
    local rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -e "$output" ]] ||
        fail "$mode retained a stale/partial LLVM artifact"
    printf '%b' "$expected" >"$WORK_DIR/$mode.expected"
    cmp -s "$WORK_DIR/$mode.expected" "$COUNT_FILE" ||
        fail "$mode invocation count drifted"
    grep -Fq "$diagnostic" "$WORK_DIR/$mode.err" ||
        fail "$mode lost its owned diagnostic"
}
reject_driver_failure producer-fail 'producer\n' 'self-host MIR producer failed with code 7'
reject_driver_failure backend-fail 'producer\nbackend\n' 'self-host LLVM projector failed with code 9'

cp "$PGY" "$WORK_DIR/missing-install/pgy$suffix"
rm -f "$WORK_DIR/missing.ll" "$WORK_DIR/unsupported.ll"
set +e
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN &&
    "$WORK_DIR/missing-install/pgy$suffix" "$SOURCE" --emit-llvm \
    -o "$WORK_REL/missing.ll") >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
(cd "$ROOT_DIR" && "$PGY" "$SOURCE" --emit-llvm --runtime=none \
    -o "$WORK_REL/unsupported.ll") >"$WORK_DIR/unsupported.out" \
    2>"$WORK_DIR/unsupported.err"
unsupported_rc=$?
set -e
[[ "$missing_rc" -ne 0 && ! -e "$WORK_DIR/missing.ll" ]] ||
    fail "missing sibling used native LLVM fallback"
grep -Fq 'self-host driver is unavailable' "$WORK_DIR/missing.err" ||
    fail "missing sibling lost its boundary diagnostic"
[[ "$unsupported_rc" -ne 0 && ! -e "$WORK_DIR/unsupported.ll" ]] ||
    fail "unsupported file form used native LLVM fallback"
grep -Fq 'outside the installed self-host driver contract' \
    "$WORK_DIR/unsupported.err" || fail "unsupported file form lost selector diagnostic"

cp "$ROOT_DIR/$SOURCE" "$WORK_DIR/same-source.pgy"
cp "$WORK_DIR/same-source.pgy" "$WORK_DIR/same-source.before"
set +e
(cd "$ROOT_DIR" && "$PGY" "$WORK_REL/same-source.pgy" --emit-llvm \
    -o "$WORK_REL/same-source.pgy") >/dev/null 2>"$WORK_DIR/same.err"
same_rc=$?
set -e
[[ "$same_rc" -ne 0 ]] && cmp -s "$WORK_DIR/same-source.before" \
    "$WORK_DIR/same-source.pgy" || fail "source/output identity destroyed its source"

require_text "$ROOT_DIR/src/pgy_driver.c" 'if (flags.emit_llvm_ir && flags.output_path != NULL) {'
require_text "$ROOT_DIR/src/pgy_driver.c" 'return driver_publish_self_host_llvm_ir_file('
require_text "$ROOT_DIR/src/compiler/driver_self_host_llvm_selection_owner.c" \
    'driver_self_host_llvm_ir_file_request_supported('
owner="$ROOT_DIR/src/compiler/self_host_llvm_ir_artifact_owner.c"
require_text "$owner" 'driver_materialize_self_host_llvm_artifacts('
! grep -Eq 'driver_run_pipeline\(|compiler_emit_llvm_ir|compiler_build_native_llvm\(' "$owner" ||
    fail "LLVM IR publication owner regained native compiler authority"
require_text "$ROOT_DIR/Makefile" 'self-host-public-llvm-ir-replacement-test-smoke:'

echo "[self-host-public-llvm-ir] installed source-MIR and DirectMirLlvm own public file emission"
