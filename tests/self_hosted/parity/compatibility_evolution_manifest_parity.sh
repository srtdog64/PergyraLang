#!/usr/bin/env bash
# Parity gate for the self-host compatibility-evolution manifest projection.

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
        echo "[self-host-parity:compatibility-evolution] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:compatibility-evolution] missing compiler binary: $PGY" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/compatibility_evolution}"
HARNESS_PATHS_FILE="$BUILD_DIR/compatibility_evolution_harness_paths.txt"
mkdir -p "$BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:compatibility-evolution" \
    "$BUILD_DIR" \
    "compatibility-evolution" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 2 ]]; then
    echo "[self-host-parity:compatibility-evolution] TestHarness manifest expected 2 compatibility evolution paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_FILE="$ROOT_DIR/${harness_paths[1]}"
for path in "$TOOL_SOURCE" "$EXPECTED_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:compatibility-evolution] missing input: $path" >&2
        exit 1
    fi
done

TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")"
C_BIN="$BUILD_DIR/compatibility_evolution_c.exe"
C_COMPILE_LOG="$BUILD_DIR/compatibility_evolution_c.compile.log"
C_OUT="$BUILD_DIR/compatibility_evolution_c.out"
C_ERR="$BUILD_DIR/compatibility_evolution_c.err"

if ! (cd "$ROOT_DIR" && "$PGY" "$TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$C_BIN")" >"$C_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:compatibility-evolution] C compile failed" >&2
    cat "$C_COMPILE_LOG" >&2
    exit 1
fi

set +e
(cd "$ROOT_DIR" && "$C_BIN" 2>"$C_ERR" | pgy_selfhost_normalize_text_artifact >"$C_OUT")
C_RC=$?
set -e
if [[ "$C_RC" -ne 0 ]]; then
    echo "[self-host-parity:compatibility-evolution] C-compiled manifest failed" >&2
    cat "$C_OUT" "$C_ERR" >&2
    exit 1
fi

pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:compatibility-evolution" \
    "$BUILD_DIR" \
    "$EXPECTED_FILE" \
    "$C_OUT" \
    "compatibility_evolution"

assert_receipt_mutation_rejected() {
    local mode="$1"
    local stem="${mode#--self-test-receipt-}"
    local out="$BUILD_DIR/compatibility_evolution_receipt_${stem}.out"
    local err="$BUILD_DIR/compatibility_evolution_receipt_${stem}.err"
    if ! (cd "$ROOT_DIR" && "$C_BIN" "$mode" >"$out" 2>"$err"); then
        echo "[self-host-parity:compatibility-evolution] receipt self-test failed: $mode" >&2
        cat "$out" "$err" >&2
        exit 1
    fi
    local actual
    actual="$(tr -d '\r' <"$out")"
    if [[ "$actual" != "receipt_rejected=$mode" ]]; then
        echo "[self-host-parity:compatibility-evolution] receipt mutation was not rejected: $mode" >&2
        cat "$out" "$err" >&2
        exit 1
    fi
}

assert_receipt_mutation_rejected --self-test-receipt-diagnostic-crosswire
assert_receipt_mutation_rejected --self-test-receipt-missing-row
assert_receipt_mutation_rejected --self-test-receipt-duplicate-surface

assert_llvm_leg "self-host-parity:compatibility-evolution" "$TOOL_ARG" "$BUILD_DIR"

echo "[self-host-parity:compatibility-evolution] parity ok"
