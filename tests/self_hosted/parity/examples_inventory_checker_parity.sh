#!/usr/bin/env bash
# Rung 2 parity for the examples inventory checker.
# Third consumer of TextScan.CountLines; triggered the lift to lib.
# Asserts: clean exit, JSON byte-equal, DirWalk-owned count drift fixture rc=1.
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
        echo "[self-host-parity:examples-inventory] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:examples-inventory] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/examples_inventory_checker}"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/examples_inventory_harness_paths.txt"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:examples-inventory" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "examples-inventory-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 3 ]]; then
    echo "[self-host-parity:examples-inventory] TestHarness manifest expected 3 examples-inventory paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[1]}"
EXPECTED_DRIFT_JSON_FILE="$ROOT_DIR/${harness_paths[2]}"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$EXPECTED_DRIFT_JSON_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:examples-inventory] missing input: $path" >&2
        exit 1
    fi
done

PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"

CLEAN_BIN="$PERGYRA_TOOL_BUILD_DIR/examples_inventory_c.exe"
CLEAN_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/examples_inventory_c.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$CLEAN_BIN")" >"$CLEAN_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:examples-inventory] C backend compile failed" >&2
    cat "$CLEAN_COMPILE_LOG" >&2
    exit 1
fi
if ! pgy_require_runnable_binary_here "self-host-parity:examples-inventory" "$CLEAN_BIN"; then
    exit 1
fi

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$CLEAN_BIN" 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:examples-inventory] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.examples-inventory.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:examples-inventory] schema header missing" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | tr -d '\r' \
    | grep -F 'pgy.selfhost.examples-inventory.v1' \
    | tail -n 1)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:examples-inventory" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_JSON_FILE" \
    "$PERGYRA_JSON" \
    "run_output"

# Synthetic count-drift fixture - mirror top-level examples/*.pgy and omit one.
# DirWalk remains the inventory owner; the expected_examples contract detects
# the drift without a separate manifest source of truth.
NEG_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-selfhost-exi.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/examples"
mkdir -p "$NEG_ROOT/.tmp"

DROP_TARGET=""
while IFS= read -r source; do
    rel="examples/$(basename "$source")"
    if [[ -z "$DROP_TARGET" ]]; then
        DROP_TARGET="$rel"
        continue
    fi
    cp "$source" "$NEG_ROOT/$rel"
done < <(find "$ROOT_DIR/examples" -maxdepth 1 -type f -name '*.pgy' | sort)

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$CLEAN_BIN" 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:examples-inventory] count-drift fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
NEG_JSON="$(printf '%s\n' "$NEG_OUT" \
    | tr -d '\r' \
    | grep -F 'pgy.selfhost.examples-inventory.v1' \
    | tail -n 1)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:examples-inventory" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_DRIFT_JSON_FILE" \
    "$NEG_JSON" \
    "run_output"

assert_llvm_leg "self-host-parity:examples-inventory" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR"
echo "[self-host-parity:examples-inventory] rung-2 parity ok (expected-json clean+count-drift; count-drift fixture rc=1)"
