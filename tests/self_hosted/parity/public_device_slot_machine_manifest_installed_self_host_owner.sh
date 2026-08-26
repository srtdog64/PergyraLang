#!/usr/bin/env bash
# Public source-C carries the installed machine declaration into the existing
# Pergyra MIR machine-layer projection. Missing or malformed physical evidence
# must fail before publication and must never retry the native compiler.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
WORK_REL=".tmp/self_hosted/public-device-slot-machine-manifest"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE="tests/cases/backend_compare/device_slot_machine_layer/main.pgy"

device_manifest_fail() {
    echo "[self-host-public-device-manifest] $*" >&2
    exit 1
}

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$SELF_DRIVER" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${SELF_DRIVER}.exe"; then
    SELF_DRIVER="${SELF_DRIVER}.exe"
fi
[[ -x "$PGY" && -x "$SELF_DRIVER" ]] ||
    device_manifest_fail "installed compiler pair is missing"
command -v "$CC" >/dev/null 2>&1 || device_manifest_fail "missing C compiler"

PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"
suffix=""
installed_name="pgy-self-driver"
if [[ "$PGY" == *.exe ]]; then
    suffix=".exe"
    installed_name="pgy-self-driver.exe"
fi
REAL_MANIFEST="$(pgy_self_driver_machine_manifest_path "$SELF_DRIVER")"
[[ -s "$REAL_MANIFEST" ]] ||
    device_manifest_fail "installed machine declaration companion is missing"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR/counting-install" "$WORK_DIR/missing-install" \
    "$WORK_DIR/corrupt-install"

(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
    "$PGY" "$SOURCE" --emit-c -o "$WORK_REL/installed.c") \
    >"$WORK_DIR/installed.out" 2>"$WORK_DIR/installed.err"
(cd "$ROOT_DIR" && "$PGY" "$SOURCE" --native-pipeline --emit-c \
    -o "$WORK_REL/native.c") >"$WORK_DIR/native.out" 2>"$WORK_DIR/native.err"
grep -Fq 'pgy_machine_layer_require_mapping();' "$WORK_DIR/installed.c" ||
    device_manifest_fail "installed C lost the admitted machine mapping"

pgy_selfhost_select_emitted_c_compile_profile ||
    device_manifest_fail "emitted C compile profile is invalid"
compile_command=("$CC" -x c -std=c11)
compile_command+=("${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}")
compile_command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
"${compile_command[@]}" "$WORK_DIR/installed.c" -o "$WORK_DIR/installed$suffix"
"${compile_command[@]}" "$WORK_DIR/native.c" -o "$WORK_DIR/native$suffix"
"$WORK_DIR/installed$suffix" | tr -d '\r' >"$WORK_DIR/installed.run"
"$WORK_DIR/native$suffix" | tr -d '\r' >"$WORK_DIR/native.run"
cmp -s "$WORK_DIR/installed.run" "$WORK_DIR/native.run" ||
    device_manifest_fail "installed/native DeviceSlot execution differs"
printf '0\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/installed.run" ||
    device_manifest_fail "DeviceSlot execution output drifted"

cp "$PGY" "$WORK_DIR/counting-install/pgy$suffix"
"$CC" -std=c11 -Wall -Wextra -Werror \
    "$ROOT_DIR/tests/self_hosted/parity/fixture/counting_self_host_c_driver.c" \
    -o "$WORK_DIR/counting-install/$installed_name"
COUNT_FILE="$WORK_DIR/count.txt"
COUNT_FILE_FOR_DRIVER="$COUNT_FILE"
if [[ "$suffix" == ".exe" ]]; then
    COUNT_FILE_FOR_DRIVER="$(pgy_path_for_compiler "$PGY" "$COUNT_FILE")"
fi
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
    PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
    "$WORK_DIR/counting-install/pgy$suffix" "$SOURCE" --emit-c \
    -o "$WORK_REL/counting.c") >"$WORK_DIR/counting.out" \
    2>"$WORK_DIR/counting.err"
[[ "$(wc -l <"$COUNT_FILE" | tr -d ' ')" == "1" ]] ||
    device_manifest_fail "installed child was not invoked exactly once"

for case_name in missing corrupt; do
    install_dir="$WORK_DIR/$case_name-install"
    cp "$PGY" "$install_dir/pgy$suffix"
    cp "$SELF_DRIVER" "$install_dir/$installed_name"
done
CORRUPT_DRIVER="$WORK_DIR/corrupt-install/$installed_name"
printf '{"schema":"wrong"}\n' >"$(pgy_self_driver_machine_manifest_path "$CORRUPT_DRIVER")"

set +e
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
    PGY_DEBUG_PIPELINE_TIMING=1 "$WORK_DIR/missing-install/pgy$suffix" \
    "$SOURCE" --emit-c -o "$WORK_REL/missing.c") \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
    PGY_DEBUG_PIPELINE_TIMING=1 "$WORK_DIR/corrupt-install/pgy$suffix" \
    "$SOURCE" --emit-c -o "$WORK_REL/corrupt.c") \
    >"$WORK_DIR/corrupt.out" 2>"$WORK_DIR/corrupt.err"
corrupt_rc=$?
set -e
for case_name in missing corrupt; do
    rc_name="${case_name}_rc"
    [[ "${!rc_name}" -ne 0 && ! -e "$WORK_DIR/$case_name.c" ]] ||
        device_manifest_fail "$case_name companion published a C artifact"
    grep -Fq 'source C machine declaration is invalid' \
        "$WORK_DIR/$case_name.out" "$WORK_DIR/$case_name.err" ||
        device_manifest_fail "$case_name companion lost its typed diagnostic"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/$case_name.err" ||
        device_manifest_fail "$case_name companion retried the native pipeline"
done

grep -Fq 'child_argv[4] = "--machine-manifest-json";' \
    "$ROOT_DIR/src/compiler/self_host_driver.c" ||
    device_manifest_fail "public child request omitted machine manifest carriage"
grep -Fq 'SourceCManifestVerified(' \
    "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy" ||
    device_manifest_fail "typed source-C request lost manifest admission"
! grep -Fq 'driver_run_pipeline(' "$ROOT_DIR/src/compiler/self_host_driver.c" ||
    device_manifest_fail "source-C adapter regained a native fallback"

echo "[self-host-public-device-manifest] installed companion owns DeviceSlot source-C projection; missing/corrupt evidence fails closed"
