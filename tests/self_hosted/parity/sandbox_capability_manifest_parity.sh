#!/usr/bin/env bash
# Parity gate for the self-host sandbox capability/frame-budget projection.

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
        echo "[self-host-parity:sandbox-capability] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:sandbox-capability] missing compiler binary: $PGY" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/sandbox_capability}"
HARNESS_PATHS_FILE="$BUILD_DIR/sandbox_capability_harness_paths.txt"
mkdir -p "$BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:sandbox-capability" \
    "$BUILD_DIR" \
    "sandbox-capability-frame-budget" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 3 ]]; then
    echo "[self-host-parity:sandbox-capability] TestHarness manifest expected 3 sandbox-capability paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_FILE="$ROOT_DIR/${harness_paths[1]}"
NEGATIVE_EXPECTED_FILE="$ROOT_DIR/${harness_paths[2]}"
for path in "$TOOL_SOURCE" "$EXPECTED_FILE" "$NEGATIVE_EXPECTED_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:sandbox-capability] missing input: $path" >&2
        exit 1
    fi
done

TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")"
C_BIN="$BUILD_DIR/sandbox_capability_c.exe"
C_COMPILE_LOG="$BUILD_DIR/sandbox_capability_c.compile.log"
C_OUT="$BUILD_DIR/sandbox_capability_c.out"
C_ERR="$BUILD_DIR/sandbox_capability_c.err"
C_NEG_OUT="$BUILD_DIR/sandbox_capability_c_negative.out"
C_NEG_ERR="$BUILD_DIR/sandbox_capability_c_negative.err"

if ! (cd "$ROOT_DIR" && "$PGY" "$TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$C_BIN")" >"$C_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:sandbox-capability] C compile failed" >&2
    cat "$C_COMPILE_LOG" >&2
    exit 1
fi

set +e
(cd "$ROOT_DIR" && "$C_BIN" 2>"$C_ERR" | pgy_selfhost_normalize_text_artifact >"$C_OUT")
C_RC=$?
set -e
if [[ "$C_RC" -ne 0 ]]; then
    echo "[self-host-parity:sandbox-capability] C-compiled manifest failed" >&2
    cat "$C_OUT" "$C_ERR" >&2
    exit 1
fi

pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:sandbox-capability" \
    "$BUILD_DIR" \
    "$EXPECTED_FILE" \
    "$C_OUT" \
    "sandbox_capability"

set +e
(cd "$ROOT_DIR" && "$C_BIN" --self-test-missing-budget 2>"$C_NEG_ERR" | pgy_selfhost_normalize_text_artifact >"$C_NEG_OUT")
C_NEG_RC=$?
set -e
if [[ "$C_NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:sandbox-capability] missing-budget self-test should fail closed (rc=1), got rc=$C_NEG_RC" >&2
    cat "$C_NEG_OUT" "$C_NEG_ERR" >&2
    exit 1
fi

pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:sandbox-capability" \
    "$BUILD_DIR" \
    "$NEGATIVE_EXPECTED_FILE" \
    "$C_NEG_OUT" \
    "run_output"

assert_llvm_leg "self-host-parity:sandbox-capability" "$TOOL_ARG" "$BUILD_DIR"

LLVM_NEG_BIN="$BUILD_DIR/sandbox_capability_llvm_negative.exe"
LLVM_NEG_COMPILE_LOG="$BUILD_DIR/sandbox_capability_llvm_negative.compile.log"
LLVM_NEG_OUT="$BUILD_DIR/sandbox_capability_llvm_negative.out"
LLVM_NEG_ERR="$BUILD_DIR/sandbox_capability_llvm_negative.err"
# Native pipeline: harness tool build, same subject reasoning as
# assert_llvm_leg (the delegated projector rejects its multi-routine shape).
if ! (cd "$ROOT_DIR" && "$PGY" "$TOOL_ARG" --native-pipeline --backend=llvm \
    -o "$(pgy_path_for_compiler "$PGY" "$LLVM_NEG_BIN")" >"$LLVM_NEG_COMPILE_LOG" 2>&1); then
    if pgy_selfhost_log_reports_no_llvm "$LLVM_NEG_COMPILE_LOG"; then
        echo "[self-host-parity:sandbox-capability] missing-budget llvm-leg skipped (compiler built without LLVM backend support)"
    else
        echo "[self-host-parity:sandbox-capability] missing-budget LLVM compile failed" >&2
        cat "$LLVM_NEG_COMPILE_LOG" >&2
        exit 1
    fi
else
    set +e
    (cd "$ROOT_DIR" && "$LLVM_NEG_BIN" --self-test-missing-budget 2>"$LLVM_NEG_ERR" | pgy_selfhost_normalize_text_artifact >"$LLVM_NEG_OUT")
    LLVM_NEG_RC=$?
    set -e
    if [[ "$LLVM_NEG_RC" -ne 1 ]]; then
        echo "[self-host-parity:sandbox-capability] missing-budget LLVM self-test should fail closed (rc=1), got rc=$LLVM_NEG_RC" >&2
        cat "$LLVM_NEG_OUT" "$LLVM_NEG_ERR" >&2
        exit 1
    fi
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "self-host-parity:sandbox-capability" \
        "$BUILD_DIR" \
        "$NEGATIVE_EXPECTED_FILE" \
        "$LLVM_NEG_OUT" \
        "run_output"
fi

echo "[self-host-parity:sandbox-capability] parity ok"
