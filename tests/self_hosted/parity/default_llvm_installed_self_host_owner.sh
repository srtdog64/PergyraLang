#!/usr/bin/env bash
# Public LLVM binaries consume exactly one installed self-host MIR artifact and
# one textual LLVM projection. clang is the final host boundary; native
# semantic/AIR/libLLVM and implicit runtime linkage are forbidden.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
WORK_DIR="$ROOT_DIR/.tmp/self_hosted/default_llvm_installed"
SOURCE="src/self_hosted/mir_lower/fixture/option_struct_value_flow.pgy"
MEMBER_SOURCE="src/self_hosted/mir_lower/fixture/generic_member_inferred_flow.pgy"
VESSEL_MEMBER_SOURCE="src/self_hosted/mir_lower/fixture/generic_vessel_member_inferred_flow.pgy"
PASSIVE_NOMINAL_SOURCE="src/self_hosted/mir_lower/fixture/nominal_tobject.pgy"
SUBJECT_SOURCE="src/self_hosted/mir_lower/fixture/nominal_subject.pgy"
CONSTRUCTED_MEMBER_SOURCE="src/self_hosted/mir_lower/fixture/generic_member_constructed_return_flow.pgy"
CONSTRUCTED_ARRAY_MEMBER_SOURCE="src/self_hosted/mir_lower/fixture/generic_member_array_return_flow.pgy"
CONSTRUCTED_RECORD_ARRAY_MEMBER_SOURCE="src/self_hosted/mir_lower/fixture/generic_member_record_array_return_flow.pgy"
COUNT_FILE="$WORK_DIR/count.txt"
COUNT_FILE_FOR_DRIVER="$COUNT_FILE"

fail() {
    echo "[self-host-default-llvm] $*" >&2
    exit 1
}

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$SELF_DRIVER" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${SELF_DRIVER}.exe"; then
    SELF_DRIVER="${SELF_DRIVER}.exe"
fi
[[ -x "$PGY" ]] || fail "missing public pgy launcher: $PGY"
[[ -x "$SELF_DRIVER" ]] || fail "missing installed self-host driver: $SELF_DRIVER"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v clang >/dev/null 2>&1 || fail "missing LLVM IR-capable clang"

PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"
suffix=""
installed_name="pgy-self-driver"
if [[ "$PGY" == *.exe ]]; then
    suffix=".exe"
    installed_name="pgy-self-driver.exe"
    COUNT_FILE_FOR_DRIVER="$(cygpath -m "$COUNT_FILE")"
fi
[[ "$SELF_DRIVER" == "$(dirname "$PGY")/$installed_name" ]] ||
    fail "self-host driver is not installed beside the public launcher"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR/counting-install" "$WORK_DIR/missing-install"

(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN &&
    "$PGY" "$SOURCE" --backend=llvm \
        -o ".tmp/self_hosted/default_llvm_installed/real-program$suffix") \
    >"$WORK_DIR/real.out" 2>"$WORK_DIR/real.err"
"$WORK_DIR/real-program$suffix" | tr -d '\r' >"$WORK_DIR/real-program.out"
printf '7\n11\n5\n' >"$WORK_DIR/real.expected"
cmp -s "$WORK_DIR/real.expected" "$WORK_DIR/real-program.out" ||
    fail "installed self-host LLVM artifact produced the wrong Option nominal output"

for member_case in "$MEMBER_SOURCE|member|41" \
    "$VESSEL_MEMBER_SOURCE|vessel-member|42" \
    "$PASSIVE_NOMINAL_SOURCE|passive-nominal|12" \
    "$SUBJECT_SOURCE|subject|7" "src/self_hosted/mir_lower/fixture/nominal_vessel.pgy|vessel|13" "src/self_hosted/mir_lower/fixture/ability_decl.pgy|ability|7" \
    "$CONSTRUCTED_MEMBER_SOURCE|constructed-member|43" \
    "$CONSTRUCTED_ARRAY_MEMBER_SOURCE|constructed-array-member|44" \
    "$CONSTRUCTED_RECORD_ARRAY_MEMBER_SOURCE|constructed-record-array-member|45"; do
    IFS='|' read -r member_source member_stem member_expected <<< "$member_case"
    (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && "$PGY" "$member_source" \
        --backend=llvm -o ".tmp/self_hosted/default_llvm_installed/$member_stem-program$suffix") \
        >"$WORK_DIR/$member_stem.out" 2>"$WORK_DIR/$member_stem.err"
    "$WORK_DIR/$member_stem-program$suffix" | tr -d '\r' >"$WORK_DIR/$member_stem-program.out"
    printf '%s\n' "$member_expected" >"$WORK_DIR/$member_stem.expected"
    cmp -s "$WORK_DIR/$member_stem.expected" "$WORK_DIR/$member_stem-program.out" ||
        fail "installed self-host LLVM artifact lost $member_stem flow"
done

cp "$PGY" "$WORK_DIR/counting-install/pgy$suffix"
"$CC" -std=c11 -Wall -Wextra -Werror \
    "$ROOT_DIR/tests/self_hosted/parity/fixture/counting_self_host_llvm_driver.c" \
    -o "$WORK_DIR/counting-install/$installed_name"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN &&
    PGY_DEBUG_PIPELINE_TIMING=1 \
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
    "$WORK_DIR/counting-install/pgy$suffix" "$SOURCE" --backend=llvm --run \
        -o ".tmp/self_hosted/default_llvm_installed/shim-program$suffix") \
    >"$WORK_DIR/shim.out" 2>"$WORK_DIR/shim.err"
printf 'producer\nbackend\n' >"$WORK_DIR/count.expected"
cmp -s "$WORK_DIR/count.expected" "$COUNT_FILE" ||
    fail "public LLVM path did not invoke producer/backend exactly once"
grep -Fxq "self-host-llvm-shim" "$WORK_DIR/shim.out" ||
    fail "--run did not execute the self-host LLVM artifact"
! grep -Fq "[pipeline timing]" "$WORK_DIR/shim.err" ||
    fail "public LLVM path re-entered the native compiler pipeline"

run_failure() {
    local mode="$1" expected_count="$2" expected_error="$3"
    local output="$WORK_DIR/$mode-program$suffix"
    rm -f "$COUNT_FILE"
    printf 'stale-binary-must-not-survive\n' >"$output"
    set +e
    (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN &&
        PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
        PGY_SELF_DRIVER_LLVM_MODE="$mode" \
        "$WORK_DIR/counting-install/pgy$suffix" "$SOURCE" --backend=llvm \
            -o ".tmp/self_hosted/default_llvm_installed/$mode-program$suffix") \
        >"$WORK_DIR/$mode.out" 2>"$WORK_DIR/$mode.err"
    local rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -e "$output" ]] ||
        fail "$mode did not fail closed before publishing a binary"
    printf '%b' "$expected_count" >"$WORK_DIR/$mode-count.expected"
    cmp -s "$WORK_DIR/$mode-count.expected" "$COUNT_FILE" ||
        fail "$mode invocation count drifted"
    grep -Fq "$expected_error" "$WORK_DIR/$mode.err" ||
        fail "$mode did not report its owned boundary"
    ! grep -Fq "[pipeline timing]" "$WORK_DIR/$mode.err" ||
        fail "$mode re-entered the native compiler pipeline"
}

run_failure "producer-fail" 'producer\n' \
    "self-host MIR producer failed with code 7"
run_failure "backend-fail" 'producer\nbackend\n' \
    "self-host LLVM projector failed with code 9"
run_failure "malformed" 'producer\nbackend\n' \
    "self-host LLVM compile failed"
run_failure "runtime-ref" 'producer\nbackend\n' \
    "self-host LLVM compile failed"

cp "$PGY" "$WORK_DIR/missing-install/pgy$suffix"
set +e
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN &&
    "$WORK_DIR/missing-install/pgy$suffix" missing-input.pgy --backend=llvm \
        -o ".tmp/self_hosted/default_llvm_installed/missing-program$suffix") \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN &&
    "$PGY" "$SOURCE" --backend=llvm --runtime=none \
        -o ".tmp/self_hosted/default_llvm_installed/unsupported-program$suffix") \
    >"$WORK_DIR/unsupported.out" 2>"$WORK_DIR/unsupported.err"
unsupported_rc=$?
set -e
[[ "$missing_rc" -ne 0 ]] || fail "missing driver used native LLVM fallback"
grep -Fq "self-host driver is unavailable" "$WORK_DIR/missing.err" ||
    fail "missing driver did not fail before source processing"
[[ "$unsupported_rc" -ne 0 ]] || fail "unsupported LLVM options used fallback"
grep -Fq "outside the installed self-host driver contract" \
    "$WORK_DIR/unsupported.err" || fail "unsupported LLVM options did not fail closed"

runner_body="$(sed -n '/^llvm_runner_execute_installed_self_host_llvm(/,/^#ifdef PGY_LLVM_ENABLED/p' \
    "$ROOT_DIR/src/compiler/llvm_runner.c")"
[[ "$(grep -Fc 'driver_materialize_self_host_llvm_artifacts(' <<< "$runner_body")" == "1" ]] ||
    fail "public LLVM runner must materialize one owned artifact pair"
grep -Fq "compiler_compile_link_self_host_llvm_artifact(" <<< "$runner_body" ||
    fail "public LLVM runner lacks the clang-only host boundary"
! grep -Eq 'driver_run_pipeline\(|compiler_build_native_llvm\(|llvm_codegen_' <<< "$runner_body" ||
    fail "public LLVM runner reintroduced native semantic/libLLVM ownership"
! grep -Eq 'compiler_runtime_object|PGY_INTENT_OBSERVABILITY' <<< "$runner_body" ||
    fail "runtime-free public LLVM runner attached an implicit runtime"
! grep -Eq 'path_read_file\(|strstr\(' \
    "$ROOT_DIR/src/compiler/self_host_llvm_driver.c" ||
    fail "native LLVM materializer inferred runtime policy from artifact text"

echo "[self-host-default-llvm] installed MIR+LLVM artifacts own public runtime-free LLVM compile/run"
