#!/usr/bin/env bash
# Plain C compilation must materialize one installed self-host C artifact and
# hand only that admitted artifact to the host compiler/linker.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
WORK_DIR="$ROOT_DIR/.tmp/self_hosted/default_c_compile_installed"
SOURCE="src/self_hosted/mir_lower/fixture/option_struct_value_flow.pgy"
MEMBER_SOURCE="src/self_hosted/mir_lower/fixture/generic_member_inferred_flow.pgy"
CONSTRUCTED_MEMBER_SOURCE="src/self_hosted/mir_lower/fixture/generic_member_constructed_return_flow.pgy"
CONSTRUCTED_ARRAY_MEMBER_SOURCE="src/self_hosted/mir_lower/fixture/generic_member_array_return_flow.pgy"
COUNT_FILE="$WORK_DIR/count.txt"
COUNT_FILE_FOR_DRIVER="$COUNT_FILE"

fail() {
    echo "[self-host-default-c-compile] $*" >&2
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
    "$PGY" "$SOURCE" --backend=c -o ".tmp/self_hosted/default_c_compile_installed/real-program$suffix") \
    >"$WORK_DIR/real.out" 2>"$WORK_DIR/real.err"
"$WORK_DIR/real-program$suffix" | tr -d '\r' >"$WORK_DIR/real-program.out"
printf '7\n11\n5\n' >"$WORK_DIR/real.expected"
cmp -s "$WORK_DIR/real.expected" "$WORK_DIR/real-program.out" ||
    fail "installed self-host C artifact produced the wrong Option nominal binary"

for member_case in "$MEMBER_SOURCE|member|41" \
    "$CONSTRUCTED_MEMBER_SOURCE|constructed-member|43" \
    "$CONSTRUCTED_ARRAY_MEMBER_SOURCE|constructed-array-member|44"; do
    IFS='|' read -r member_source member_stem member_expected <<< "$member_case"
    (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && "$PGY" "$member_source" \
        --backend=c -o ".tmp/self_hosted/default_c_compile_installed/$member_stem-program$suffix") \
        >"$WORK_DIR/$member_stem.out" 2>"$WORK_DIR/$member_stem.err"
    "$WORK_DIR/$member_stem-program$suffix" | tr -d '\r' >"$WORK_DIR/$member_stem-program.out"
    printf '%s\n' "$member_expected" >"$WORK_DIR/$member_stem.expected"
    cmp -s "$WORK_DIR/$member_stem.expected" "$WORK_DIR/$member_stem-program.out" ||
        fail "installed self-host C artifact lost $member_stem flow"
done

cp "$PGY" "$WORK_DIR/counting-install/pgy$suffix"
"$CC" -std=c11 -Wall -Wextra -Werror \
    "$ROOT_DIR/tests/self_hosted/parity/fixture/counting_self_host_c_driver.c" \
    -o "$WORK_DIR/counting-install/$installed_name"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN &&
    PGY_DEBUG_PIPELINE_TIMING=1 \
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
    "$WORK_DIR/counting-install/pgy$suffix" "$SOURCE" --backend=c --run \
        -o ".tmp/self_hosted/default_c_compile_installed/shim-program$suffix") \
    >"$WORK_DIR/shim.out" 2>"$WORK_DIR/shim.err"
[[ "$(wc -l < "$COUNT_FILE" | tr -d ' ')" == "1" ]] ||
    fail "installed self-host driver was not invoked exactly once"
grep -Fxq "self-host-shim" "$WORK_DIR/shim.out" ||
    fail "--run did not execute the self-host driver's C artifact"
! grep -Fq "[pipeline timing]" "$WORK_DIR/shim.err" ||
    fail "plain self-host C compile re-entered the native compiler pipeline"

cp "$PGY" "$WORK_DIR/missing-install/pgy$suffix"
set +e
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN &&
    "$WORK_DIR/missing-install/pgy$suffix" "missing-input.pgy" --backend=c \
        -o ".tmp/self_hosted/default_c_compile_installed/missing-program$suffix") \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN &&
    "$PGY" "$SOURCE" --backend=c --runtime=none \
        -o ".tmp/self_hosted/default_c_compile_installed/unsupported-program$suffix") \
    >"$WORK_DIR/unsupported.out" 2>"$WORK_DIR/unsupported.err"
unsupported_rc=$?
set -e
[[ "$missing_rc" -ne 0 ]] || fail "missing self-host driver used a native fallback"
grep -Fq "self-host driver is unavailable" "$WORK_DIR/missing.err" ||
    fail "missing self-host driver did not fail before source processing"
[[ "$unsupported_rc" -ne 0 ]] || fail "unsupported options used a native fallback"
grep -Fq "outside the installed self-host driver contract" \
    "$WORK_DIR/unsupported.err" || fail "unsupported options did not fail closed"

runner_body="$(sed -n '/^c_runner_execute_installed_self_host_c(/,/^c_runner_execute(/p' \
    "$ROOT_DIR/src/compiler/c_runner.c")"
[[ "$(grep -Fc 'driver_materialize_self_host_c_artifact(' <<< "$runner_body")" == "1" ]] ||
    fail "plain C runner must materialize exactly one self-host artifact"
grep -Fq "compiler_compile_link_self_host_c_artifact(" <<< "$runner_body" ||
    fail "plain C runner is not wired to the host-only compile/link boundary"
! grep -Eq 'driver_run_pipeline\(|compiler_build_native\(' <<< "$runner_body" ||
    fail "plain C runner reintroduced the native semantic/codegen pipeline"

echo "[self-host-default-c-compile] installed self-host artifact owns plain C compile; host-only link is closed"
