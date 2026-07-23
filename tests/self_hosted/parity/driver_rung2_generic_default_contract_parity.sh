#!/usr/bin/env bash
# Executable source-pipeline closure for declaration generic defaults.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/portable_text_mutation_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_generic_default_contract_parity_owner.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
CC="${CC:-cc}"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver_rung2_generic_default_contract}"
SOURCE_REL="tests/cases/backend_compare/generic_default_contracts/main.pgy"
SOURCE="$ROOT_DIR/$SOURCE_REL"
DRIVER_BIN="$BUILD_DIR/driver_c.exe"
SELF_MIR="$BUILD_DIR/self.mir.json"
SELF_C="$BUILD_DIR/self.c"
SELF_PROGRAM="$BUILD_DIR/self.exe"
NATIVE_PROGRAM="$BUILD_DIR/native.exe"
EXPECTED_OUTPUT=$'save=9\nbox=7'
mkdir -p "$BUILD_DIR"

if ! (cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER_BIN")" \
    >"$BUILD_DIR/driver.compile.log" 2>&1); then
    echo "[self-host-parity:generic-defaults] driver compile failed" >&2
    cat "$BUILD_DIR/driver.compile.log" >&2
    exit 1
fi

(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified "$SOURCE_REL") \
    | tr -d '\r' >"$SELF_MIR"
pgy_selfhost_verify_driver_rung2_generic_default_contract \
    c generic_default_contracts "$SELF_MIR" "$DRIVER_BIN"

(cd "$ROOT_DIR" && "$DRIVER_BIN" "$SOURCE_REL" --emit-c-verified) \
    | tr -d '\r' >"$SELF_C"
pgy_selfhost_verify_driver_rung2_generic_default_contract_emitted_c \
    c generic_default_contracts "$SELF_C"

if ! "$CC" -x c -std=c11 -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -pthread "$SELF_C" -o "$SELF_PROGRAM" \
    >"$BUILD_DIR/self.cc.log" 2>&1; then
    echo "[self-host-parity:generic-defaults] emitted C compile failed" >&2
    cat "$BUILD_DIR/self.cc.log" >&2
    exit 1
fi
if ! (cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$SOURCE")" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$NATIVE_PROGRAM")" \
    >"$BUILD_DIR/native.compile.log" 2>&1); then
    echo "[self-host-parity:generic-defaults] native oracle compile failed" >&2
    cat "$BUILD_DIR/native.compile.log" >&2
    exit 1
fi

self_output="$("$SELF_PROGRAM" | tr -d '\r')"
native_output="$("$NATIVE_PROGRAM" | tr -d '\r')"
if [[ "$self_output" != "$EXPECTED_OUTPUT" ||
    "$native_output" != "$EXPECTED_OUTPUT" ]]; then
    echo "[self-host-parity:generic-defaults] runtime output drifted" >&2
    printf 'self:\n%s\nnative:\n%s\n' "$self_output" "$native_output" >&2
    exit 1
fi

echo "[self-host-parity:generic-defaults] source/MIR/C/runtime parity ok"
