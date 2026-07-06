#!/usr/bin/env bash
# Rung 2 parity for the stdlib dispatch inventory checker (2026-05-27).
#
# Pergyra is the origin
# (src/self_hosted/tools/stdlib_dispatch_inventory_checker/main.pgy).
# Asserts:
#   - clean repo: rc=0, JSON byte-equal vs expected/clean.json
#   - synthetic drift fixture (delete one LLVM entry): rc=1, count_drift
#     finding
# See tests/self_hosted/parity/README.md.

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
        echo "[self-host-parity:stdlib-dispatch-inventory] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:stdlib-dispatch-inventory] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/stdlib_dispatch_inventory_checker}"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/stdlib_dispatch_inventory_harness_paths.txt"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:stdlib-dispatch-inventory" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "stdlib-dispatch-inventory-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 5 ]]; then
    echo "[self-host-parity:stdlib-dispatch-inventory] TestHarness manifest expected 5 stdlib-dispatch paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"
C_DISPATCH="${harness_paths[2]}"
C_UNARY_DISPATCH="${harness_paths[3]}"
LLVM_DISPATCH="${harness_paths[4]}"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$ROOT_DIR/$C_DISPATCH" "$ROOT_DIR/$C_UNARY_DISPATCH" "$ROOT_DIR/$LLVM_DISPATCH"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:stdlib-dispatch-inventory] missing input: $path" >&2
        exit 1
    fi
done

PERGYRA_TOOL_INPUT="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"

CLEAN_BIN="$PERGYRA_TOOL_BUILD_DIR/stdlib_dispatch_inventory_c.exe"
CLEAN_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/stdlib_dispatch_inventory_c.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_INPUT" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$CLEAN_BIN")" >"$CLEAN_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:stdlib-dispatch-inventory] C backend compile failed" >&2
    cat "$CLEAN_COMPILE_LOG" >&2
    exit 1
fi
if ! pgy_require_runnable_binary_here "self-host-parity:stdlib-dispatch-inventory" "$CLEAN_BIN"; then
    exit 1
fi

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$CLEAN_BIN" 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:stdlib-dispatch-inventory] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.stdlib-dispatch-inventory.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:stdlib-dispatch-inventory] schema header missing" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | tr -d '\r' \
    | grep -F 'pgy.selfhost.stdlib-dispatch-inventory.v1' \
    | tail -n 1)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:stdlib-dispatch-inventory" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_JSON_FILE" \
    "$PERGYRA_JSON" \
    "run_output"

# Synthetic drift fixture - strip multiple LLVM entries to exceed the
# tolerance band, expect rc=1.
NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-sdi.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/src/codegen"
mkdir -p "$NEG_ROOT/.tmp"
cp "$ROOT_DIR/$C_DISPATCH" "$NEG_ROOT/$C_DISPATCH"
cp "$ROOT_DIR/$C_UNARY_DISPATCH" "$NEG_ROOT/$C_UNARY_DISPATCH"
# Delete enough `"stdlib ` lines in LLVM dispatch to push raw drift above
# the tool's tolerance band (currently 5). Strip 12 to be safely past.
awk 'BEGIN{stripped=0} /"stdlib /{ if(stripped<12){stripped++; next} } {print}' \
    "$ROOT_DIR/$LLVM_DISPATCH" > "$NEG_ROOT/$LLVM_DISPATCH"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$CLEAN_BIN" 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:stdlib-dispatch-inventory] drift fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"kind":"count_drift"' <<<"$NEG_OUT"; then
    echo "[self-host-parity:stdlib-dispatch-inventory] drift fixture expected count_drift finding" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

assert_llvm_leg "self-host-parity:stdlib-dispatch-inventory" "$PERGYRA_TOOL_INPUT" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:stdlib-dispatch-inventory] rung-2 parity ok (expected-json clean; drift-fixture rc=1)"
