#!/usr/bin/env bash
# Rung 1 parity for the self-hosted linter.
#
# C backend output is mandatory and byte-compared against the committed JSON
# oracle. LLVM output is also byte-compared when the active compiler supports
# the LLVM backend; LLVM-disabled builds skip only that half.

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
        echo "[self-host-parity:linter] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:linter] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/linter/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/linter}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/linter/expected/diagnostics.json"
FIXTURE_FILE="$ROOT_DIR/src/self_hosted/tools/linter/fixture.pgy"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$FIXTURE_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:linter] missing input: $path" >&2
        exit 1
    fi
done

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"

run_linter_backend() {
    local backend="$1"
    local out_bin="$PERGYRA_TOOL_BUILD_DIR/linter-$backend"
    local source_arg
    local out_arg
    source_arg="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")"
    out_arg="$(pgy_path_for_compiler "$PGY" "$out_bin")"
    (cd "$ROOT_DIR" && "$PGY" "$source_arg" --backend="$backend" --run -o "$out_arg" 2>&1)
}

normalize_json_line() {
    tr -d '\r' | grep -F '[{"range"' | tail -n 1
}

set +e
C_OUT="$(run_linter_backend c)"
C_RC=$?
set -e
if [[ "$C_RC" -ne 0 ]]; then
    echo "[self-host-parity:linter] C backend expected rc=0, got rc=$C_RC" >&2
    printf '%s\n' "$C_OUT" >&2
    exit 1
fi
C_JSON="$(printf '%s\n' "$C_OUT" | normalize_json_line)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:linter:c" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_JSON_FILE" \
    "$C_JSON" \
    "diagnostics"

set +e
LLVM_OUT="$(run_linter_backend llvm)"
LLVM_RC=$?
set -e
if [[ "$LLVM_RC" -ne 0 ]]; then
    if grep -Eiq '(llvm.*(not enabled|disabled|unavailable|unsupported|not built)|without LLVM backend support)' \
            <<<"$LLVM_OUT"; then
        echo "[self-host-parity:linter] rung-1 parity ok (c rc=0; llvm skipped)"
        exit 0
    fi
    echo "[self-host-parity:linter] LLVM backend expected rc=0, got rc=$LLVM_RC" >&2
    printf '%s\n' "$LLVM_OUT" >&2
    exit 1
fi
LLVM_JSON="$(printf '%s\n' "$LLVM_OUT" | normalize_json_line)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:linter:llvm" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_JSON_FILE" \
    "$LLVM_JSON" \
    "diagnostics"

echo "[self-host-parity:linter] rung-1 parity ok (c rc=0; llvm rc=0; diagnostics artifact-equal)"
