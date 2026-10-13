#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate: the native machine-layer C pipeline. The delegated
# self-host driver takes no machine declaration on the emit-C argv, so the
# default C replacement cannot carry these fixtures; judge the native
# backend directly.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[machine-layer-pipeline] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY_EXPLICIT=0
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY_EXPLICIT=1
fi
PGY="$(pgy_select_optional_exe_binary "$PGY")"
if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[machine-layer-pipeline] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    fail "missing compiler binary: $PGY"
fi
if ! pgy_binary_is_runnable_here "$PGY"; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[machine-layer-pipeline] SKIP compiler binary is not runnable here: $PGY"
        exit 0
    fi
    pgy_require_runnable_binary_here "machine-layer-pipeline" "$PGY"
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
if pgy_binary_expects_windows_paths "$PGY"; then
    TMP_BASE="$ROOT_DIR/.tmp"
    mkdir -p "$TMP_BASE"
fi
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_machine_layer.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

SOURCE_REMOTE="$ROOT_DIR/tests/cases/backend_compare/device_slot_remote/main.pgy"
SOURCE_READ="$ROOT_DIR/tests/cases/backend_compare/device_slot_machine_layer/main.pgy"
MIR_JSON="$WORK_DIR/mir.json"
AIR_JSON="$WORK_DIR/air.json"
RIR_JSON="$WORK_DIR/rir.json"
RIR_READ_JSON="$WORK_DIR/rir_read.json"
MACHINE_MANIFEST_JSON="$WORK_DIR/machine-layer-declaration.json"
MIR_READ_JSON="$WORK_DIR/mir_read.json"
AIR_READ_JSON="$WORK_DIR/air_read.json"
C_OUT="$WORK_DIR/machine.c"
LLVM_OUT="$WORK_DIR/machine.ll"
C_REMOTE_OUT="$WORK_DIR/remote.c"
LLVM_REMOTE_OUT="$WORK_DIR/remote.ll"
LLVM_AVAILABLE=1

compiler_path() {
    pgy_path_for_compiler "$PGY" "$1"
}

"$PGY" --test-native-mir-json-oracle "$(compiler_path "$SOURCE_REMOTE")" \
    >"$MIR_JSON" 2>"$WORK_DIR/mir.err" || {
    cat "$WORK_DIR/mir.err" >&2
    fail "MIR pipeline rejected the existing DeviceSlot fixture"
}
"$PGY" --machine-manifest-json >"$MACHINE_MANIFEST_JSON" 2>"$WORK_DIR/machine_manifest.err" || {
    cat "$WORK_DIR/machine_manifest.err" >&2
    fail "native machine declaration JSON export failed"
}
"$PGY" --native-pipeline --rir-json "$(compiler_path "$SOURCE_REMOTE")" >"$RIR_JSON" 2>"$WORK_DIR/rir.err" || {
    cat "$WORK_DIR/rir.err" >&2
    fail "RIR JSON pipeline rejected the SubmitDeviceRead fixture"
}
"$PGY" --native-pipeline --air-json "$(compiler_path "$SOURCE_REMOTE")" >"$AIR_JSON" 2>"$WORK_DIR/air.err" || {
    cat "$WORK_DIR/air.err" >&2
    fail "AIR pipeline rejected the existing DeviceSlot fixture"
}
"$PGY" --test-native-mir-json-oracle "$(compiler_path "$SOURCE_READ")" \
    >"$MIR_READ_JSON" 2>"$WORK_DIR/mir_read.err" || {
    cat "$WORK_DIR/mir_read.err" >&2
    fail "MIR pipeline rejected the direct DeviceRead fixture"
}
"$PGY" --native-pipeline --rir-json "$(compiler_path "$SOURCE_READ")" >"$RIR_READ_JSON" 2>"$WORK_DIR/rir_read.err" || {
    cat "$WORK_DIR/rir_read.err" >&2
    fail "RIR JSON pipeline rejected the direct DeviceRead fixture"
}
"$PGY" --native-pipeline --air-json "$(compiler_path "$SOURCE_READ")" >"$AIR_READ_JSON" 2>"$WORK_DIR/air_read.err" || {
    cat "$WORK_DIR/air_read.err" >&2
    fail "AIR pipeline rejected the direct DeviceRead fixture"
}
"$PGY" "$(compiler_path "$SOURCE_READ")" --backend=c --emit-c -o "$(compiler_path "$C_OUT")" \
    >"$WORK_DIR/c.out" 2>"$WORK_DIR/c.err" || {
    cat "$WORK_DIR/c.err" >&2
    fail "C backend rejected the direct DeviceRead fixture"
}
if ! "$PGY" "$(compiler_path "$SOURCE_READ")" --backend=llvm --emit-llvm \
        -o "$(compiler_path "$LLVM_OUT")" \
        >"$WORK_DIR/llvm.out" 2>"$WORK_DIR/llvm.err"; then
    if grep -Eiq 'compiled without LLVM backend support|LLVM backend.*(not enabled|disabled|unavailable|not built)' \
            "$WORK_DIR/llvm.err"; then
        LLVM_AVAILABLE=0
        echo "[machine-layer-pipeline] llvm-leg skipped (compiler built without LLVM backend support)"
    else
        cat "$WORK_DIR/llvm.err" >&2
        fail "LLVM backend rejected the direct DeviceRead fixture"
    fi
fi
"$PGY" "$(compiler_path "$SOURCE_REMOTE")" --backend=c --emit-c -o "$(compiler_path "$C_REMOTE_OUT")" \
    >"$WORK_DIR/c_remote.out" 2>"$WORK_DIR/c_remote.err" || {
    cat "$WORK_DIR/c_remote.err" >&2
    fail "C backend rejected the SubmitDeviceRead fixture"
}
if [[ "$LLVM_AVAILABLE" -eq 1 ]]; then
    "$PGY" "$(compiler_path "$SOURCE_REMOTE")" --backend=llvm --emit-llvm \
        -o "$(compiler_path "$LLVM_REMOTE_OUT")" \
        >"$WORK_DIR/llvm_remote.out" 2>"$WORK_DIR/llvm_remote.err" || {
        cat "$WORK_DIR/llvm_remote.err" >&2
        fail "LLVM backend rejected the SubmitDeviceRead fixture"
    }
fi

require_file_text() {
    local path="$1"
    local term="$2"
    grep -Fq -- "$term" "$path" || fail "$path missing: $term"
}

for term in \
    '"machine_layer"' \
    '"machine_contact_kind"' \
    '"operation":"claim"' \
    '"operation":"write"' \
    '"operation":"submit-read"' \
    '"operation":"release"' \
    '"manifest":"pergyra.abstract-device-slot.v1"' \
    '"physical_grant":"device-slot0"' \
    '"physical_base":268435456' \
    '"physical_size":4096' \
    '"physical_mode":"volatile"' \
    '"runtime_operation":"Claim"' \
    '"runtime_operation":"Write"' \
    '"runtime_operation":"SubmitRead"' \
    '"runtime_operation":"Release"' \
    '"hardware_adequate":true' \
    '"authority_required":true' \
    '"live_lease_required":true'; do
    require_file_text "$MIR_JSON" "$term"
done

require_file_text "$MIR_READ_JSON" '"operation":"read"'
require_file_text "$MIR_READ_JSON" '"manifest":"pergyra.abstract-device-slot.v1"'
require_file_text "$MIR_READ_JSON" '"physical_mode":"volatile"'
require_file_text "$MIR_READ_JSON" '"runtime_operation":"Read"'

# RIR JSON is the single producer-facing machine-contact artifact. It must
# carry the typed contact rows before MIR/AIR projection; consumers must not
# reconstruct them by rescanning source spellings.
for term in \
    '"rir_version": 1' \
    '"resource_identity_verified": true' \
    '"machine_contact":"claim"' \
    '"machine_contact":"write"' \
    '"machine_contact":"submit-read"' \
    '"machine_contact":"release"'; do
    require_file_text "$RIR_JSON" "$term"
done
for term in \
    '"rir_version": 1' \
    '"resource_identity_verified": true' \
    '"machine_contact":"claim"' \
    '"machine_contact":"read"' \
    '"machine_contact":"write"' \
    '"machine_contact":"release"'; do
    require_file_text "$RIR_READ_JSON" "$term"
done
for artifact in "$RIR_JSON" "$RIR_READ_JSON"; do
    [[ "$(head -c 1 "$artifact")" == "{" ]] ||
        fail "$artifact is not a clean JSON artifact boundary"
done
[[ "$(head -c 1 "$MACHINE_MANIFEST_JSON")" == "{" ]] ||
    fail "$MACHINE_MANIFEST_JSON is not a clean JSON artifact boundary"
for term in \
    '"schema":"pgy.machine-layer.declaration.v1"' \
    '"id":"pergyra.abstract-device-slot.v1"' \
    '"id":"pergyra.machine-declaration.host-sim.v1"' \
    '"board_id":"host-sim-board"' \
    '"boot_contract":"host-sim-boot.v1"' \
    '"linker_contract":"host-sim-linker.v1"' \
    '"device_grant":"device-slot0"' \
    '"base":268435456' \
    '"size":4096' \
    '"mode":"volatile"' \
    '"runtime_operation":"SubmitRead"'; do
    require_file_text "$MACHINE_MANIFEST_JSON" "$term"
done

for term in \
    '"machine_layer_sites"' \
    '"operation":"claim"' \
    '"operation":"write"' \
    '"operation":"submit-read"' \
    '"operation":"release"' \
    '"manifest":"pergyra.abstract-device-slot.v1"' \
    '"physical_grant":"device-slot0"' \
    '"physical_base":268435456' \
    '"physical_size":4096' \
    '"physical_mode":"volatile"' \
    '"runtime_operation":"Claim"' \
    '"runtime_operation":"Write"' \
    '"runtime_operation":"SubmitRead"' \
    '"runtime_operation":"Release"'; do
    require_file_text "$AIR_JSON" "$term"
done

require_file_text "$AIR_READ_JSON" '"operation":"read"'
require_file_text "$AIR_READ_JSON" '"manifest":"pergyra.abstract-device-slot.v1"'
require_file_text "$AIR_READ_JSON" '"physical_mode":"volatile"'
require_file_text "$AIR_READ_JSON" '"runtime_operation":"Read"'

for term in pgy_claim_device_ pgy_device_read_ pgy_device_write_ pgy_release_device_; do
    require_file_text "$C_OUT" "$term"
done
require_file_text "$C_OUT" 'pgy_machine_layer_runtime_bind_mapping_export'
require_file_text "$C_OUT" 'UINT32_C('
require_file_text "$C_OUT" 'machine-layer runtime bind rejected'
if [[ "$LLVM_AVAILABLE" -eq 1 ]]; then
    for term in pgy_claim_device_ pgy_device_read_ pgy_device_write_ pgy_release_device_; do
        require_file_text "$LLVM_OUT" "$term"
    done
    require_file_text "$LLVM_OUT" 'pgy_machine_layer_runtime_bind_mapping_export'
    require_file_text "$LLVM_OUT" 'i32 1'
    require_file_text "$LLVM_OUT" 'machine_layer_bind_fail'
fi
if grep -Fq -- 'pgy_machine_layer_runtime_bind_export(UINT64_C' "$C_OUT" \
    || { [[ "$LLVM_AVAILABLE" -eq 1 ]] \
         && grep -Fq -- 'call i32 @pgy_machine_layer_runtime_bind_export' "$LLVM_OUT"; }; then
    fail 'backend emitted the legacy fingerprint-only machine-layer bind path'
fi
# Physical projections must remain distinct in the emitted LLVM type graph.
# Opaque pointers alone cannot carry the distinction, so the named aggregate
# and the DeviceSlot alloca are the negative/positive ratchet here.
require_file_text "$C_OUT" 'PgyDeviceSlot_Int'
if [[ "$LLVM_AVAILABLE" -eq 1 ]]; then
    require_file_text "$LLVM_OUT" '%PgySlot_Int = type'
    require_file_text "$LLVM_OUT" '%PgyDeviceSlot_Int = type'
    require_file_text "$LLVM_OUT" 'alloca %PgyDeviceSlot_Int'
fi
require_file_text "$C_REMOTE_OUT" pgy_submit_device_read_
if [[ "$LLVM_AVAILABLE" -eq 1 ]]; then
    require_file_text "$LLVM_REMOTE_OUT" pgy_submit_device_read_
fi

echo "[machine-layer-pipeline] GREEN -- RIR JSON/MIR/AIR facts and C/LLVM contact lowering are connected; distinct C/LLVM physical projection rows, checked host-sim MachineDeclaration provenance envelope, self-host startup mapping, and fail-closed manifest validation"
