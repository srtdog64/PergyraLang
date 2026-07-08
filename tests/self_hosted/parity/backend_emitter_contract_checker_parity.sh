#!/usr/bin/env bash
# Parity gate for the self-host backend-emitter contract checker.

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
        echo "[self-host-parity:backend-emitter-contract] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:backend-emitter-contract] missing compiler binary: $PGY" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/backend_emitter_contract}"
HARNESS_PATHS_FILE="$BUILD_DIR/backend_emitter_contract_harness_paths.txt"
mkdir -p "$BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:backend-emitter-contract" \
    "$BUILD_DIR" \
    "backend-emitter-contract-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 4 ]]; then
    echo "[self-host-parity:backend-emitter-contract] TestHarness manifest expected 4 backend contract paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_FILE="$ROOT_DIR/${harness_paths[1]}"
MISSING_EXPECTED_FILE="$ROOT_DIR/${harness_paths[2]}"
FORBIDDEN_EXPECTED_FILE="$ROOT_DIR/${harness_paths[3]}"
for path in "$TOOL_SOURCE" "$EXPECTED_FILE" "$MISSING_EXPECTED_FILE" "$FORBIDDEN_EXPECTED_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:backend-emitter-contract] missing input: $path" >&2
        exit 1
    fi
done

TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")"
C_BIN="$BUILD_DIR/backend_emitter_contract_c.exe"
C_COMPILE_LOG="$BUILD_DIR/backend_emitter_contract_c.compile.log"
C_OUT="$BUILD_DIR/backend_emitter_contract_c.out"
C_ERR="$BUILD_DIR/backend_emitter_contract_c.err"
C_MISSING_OUT="$BUILD_DIR/backend_emitter_contract_c_missing.out"
C_FORBIDDEN_OUT="$BUILD_DIR/backend_emitter_contract_c_forbidden.out"

if ! (cd "$ROOT_DIR" && "$PGY" "$TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$C_BIN")" >"$C_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:backend-emitter-contract] C compile failed" >&2
    cat "$C_COMPILE_LOG" >&2
    exit 1
fi

set +e
(cd "$ROOT_DIR" && "$C_BIN" 2>"$C_ERR" | pgy_selfhost_normalize_text_artifact >"$C_OUT")
C_RC=$?
set -e
if [[ "$C_RC" -ne 0 ]]; then
    echo "[self-host-parity:backend-emitter-contract] C-compiled checker failed" >&2
    cat "$C_OUT" "$C_ERR" >&2
    exit 1
fi
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:backend-emitter-contract" \
    "$BUILD_DIR" \
    "$EXPECTED_FILE" \
    "$C_OUT" \
    "run_output"

set +e
(cd "$ROOT_DIR" && "$C_BIN" --self-test-missing-required 2>/dev/null | pgy_selfhost_normalize_text_artifact >"$C_MISSING_OUT")
C_MISSING_RC=$?
set -e
if [[ "$C_MISSING_RC" -ne 1 ]]; then
    echo "[self-host-parity:backend-emitter-contract] missing-required self-test should fail closed (rc=1), got rc=$C_MISSING_RC" >&2
    cat "$C_MISSING_OUT" >&2
    exit 1
fi
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:backend-emitter-contract" \
    "$BUILD_DIR" \
    "$MISSING_EXPECTED_FILE" \
    "$C_MISSING_OUT" \
    "run_output"

set +e
(cd "$ROOT_DIR" && "$C_BIN" --self-test-forbidden-hit 2>/dev/null | pgy_selfhost_normalize_text_artifact >"$C_FORBIDDEN_OUT")
C_FORBIDDEN_RC=$?
set -e
if [[ "$C_FORBIDDEN_RC" -ne 1 ]]; then
    echo "[self-host-parity:backend-emitter-contract] forbidden-hit self-test should fail closed (rc=1), got rc=$C_FORBIDDEN_RC" >&2
    cat "$C_FORBIDDEN_OUT" >&2
    exit 1
fi
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:backend-emitter-contract" \
    "$BUILD_DIR" \
    "$FORBIDDEN_EXPECTED_FILE" \
    "$C_FORBIDDEN_OUT" \
    "run_output"

assert_llvm_leg "self-host-parity:backend-emitter-contract" "$TOOL_ARG" "$BUILD_DIR"

echo "[self-host-parity:backend-emitter-contract] parity ok"
