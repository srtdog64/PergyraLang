#!/usr/bin/env bash
# Parity gate for the self-host runtime-call ABI row manifest projection.
#
# Runtime helper and target-library call spellings are not considered a
# self-host truth surface until their runnable projection emits a stable
# artifact and both C/LLVM compiler legs agree on that artifact when LLVM is
# available.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:runtime-call-abi-rows] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:runtime-call-abi-rows] missing compiler binary: $PGY" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/runtime_call_abi_rows}"
HARNESS_PATHS_FILE="$BUILD_DIR/runtime_call_abi_rows_harness_paths.txt"
mkdir -p "$BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:runtime-call-abi-rows" \
    "$BUILD_DIR" \
    "runtime-call-abi-rows" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 2 ]]; then
    echo "[self-host-parity:runtime-call-abi-rows] TestHarness manifest expected 2 runtime call ABI row paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_FILE="$ROOT_DIR/${harness_paths[1]}"
for path in "$TOOL_SOURCE" "$EXPECTED_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:runtime-call-abi-rows] missing input: $path" >&2
        exit 1
    fi
done

TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")"
C_BIN="$BUILD_DIR/runtime_call_abi_rows_c.exe"
C_COMPILE_LOG="$BUILD_DIR/runtime_call_abi_rows_c.compile.log"
C_OUT="$BUILD_DIR/runtime_call_abi_rows_c.out"
C_ERR="$BUILD_DIR/runtime_call_abi_rows_c.err"

if ! (cd "$ROOT_DIR" && "$PGY" "$TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$C_BIN")" >"$C_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:runtime-call-abi-rows] C compile failed" >&2
    cat "$C_COMPILE_LOG" >&2
    exit 1
fi

set +e
(cd "$ROOT_DIR" && "$C_BIN" 2>"$C_ERR" | pgy_selfhost_normalize_text_artifact >"$C_OUT")
C_RC=$?
set -e
if [[ "$C_RC" -ne 0 ]]; then
    echo "[self-host-parity:runtime-call-abi-rows] C-compiled manifest failed" >&2
    cat "$C_OUT" "$C_ERR" >&2
    exit 1
fi

pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:runtime-call-abi-rows" \
    "$BUILD_DIR" \
    "$EXPECTED_FILE" \
    "$C_OUT" \
    "runtime_call_abi"

assert_llvm_leg "self-host-parity:runtime-call-abi-rows" "$TOOL_ARG" "$BUILD_DIR"

echo "[self-host-parity:runtime-call-abi-rows] parity ok"
