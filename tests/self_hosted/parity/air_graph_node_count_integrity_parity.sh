#!/usr/bin/env bash
# Rung 1 parity for the AIR graph node-count integrity checker.
#
# Pergyra is the origin
# (src/self_hosted/tools/air_graph_node_count_integrity/main.pgy).
# This is the first AIR consumer on the live `pgy --air-json` dump (reused from
# the drift-guarded validator fixture). TestHarness-projected expected artifacts
# own clean and corrupted-summary JSON; shell only runs the process, mutates the
# negative fixture, and checks rc=1.

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
        echo "[self-host-parity:air-node-count] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:air-node-count] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/air_graph_node_count_integrity}"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/air_graph_node_count_harness_paths.txt"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:air-node-count" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "air-graph-node-count-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 8 ]]; then
    echo "[self-host-parity:air-node-count] TestHarness manifest expected 8 paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
AIR_GRAPH_SCAN_OWNER="$ROOT_DIR/${harness_paths[1]}"
EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[2]}"
FIXTURE_REL="${harness_paths[3]}"
FIXTURE_FILE="$ROOT_DIR/$FIXTURE_REL"
NEG_FIXTURE_REL="${harness_paths[4]}"
CORRUPT_FIELD="${harness_paths[5]}"
CORRUPT_VALUE="${harness_paths[6]}"
EXPECTED_NEG_JSON_FILE="$ROOT_DIR/${harness_paths[7]}"

if [[ -z "$NEG_FIXTURE_REL" || -z "$CORRUPT_FIELD" || -z "$CORRUPT_VALUE" || -z "${harness_paths[7]}" ]]; then
    echo "[self-host-parity:air-node-count] invalid TestHarness corrupt fixture row" >&2
    exit 1
fi

for path in "$PERGYRA_TOOL_SOURCE" "$AIR_GRAPH_SCAN_OWNER" "$EXPECTED_JSON_FILE" "$EXPECTED_NEG_JSON_FILE" "$FIXTURE_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:air-node-count] missing input: $path" >&2
        exit 1
    fi
done

PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"
CLEAN_BIN="$PERGYRA_TOOL_BUILD_DIR/air_graph_node_count_c.exe"
CLEAN_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/air_graph_node_count_c.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$CLEAN_BIN")" >"$CLEAN_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:air-node-count] C backend compile failed" >&2
    cat "$CLEAN_COMPILE_LOG" >&2
    exit 1
fi
if ! pgy_require_runnable_binary_here "self-host-parity:air-node-count" "$CLEAN_BIN"; then
    exit 1
fi

# Clean run.
set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$CLEAN_BIN" "$FIXTURE_REL" 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:air-node-count] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | tr -d '\r' \
    | grep -F 'pgy.selfhost.air-node-count.v1' \
    | tail -n 1)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:air-node-count" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_JSON_FILE" \
    "$PERGYRA_JSON" \
    "air_json"

# Negative fixture: corrupt a summary count so id_count != declared.
NEG_FIXTURE_FILE="$ROOT_DIR/$NEG_FIXTURE_REL"
mkdir -p "$(dirname "$NEG_FIXTURE_FILE")"
sed -E "s/\"${CORRUPT_FIELD}\":[0-9]+/\"${CORRUPT_FIELD}\":${CORRUPT_VALUE}/" "$FIXTURE_FILE" \
    > "$NEG_FIXTURE_FILE"

set +e
NEG_OUT="$(cd "$ROOT_DIR" && "$CLEAN_BIN" "$NEG_FIXTURE_REL" 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:air-node-count] corrupted fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
NEG_JSON="$(printf '%s\n' "$NEG_OUT" \
    | tr -d '\r' \
    | grep -F 'pgy.selfhost.air-node-count.v1' \
    | tail -n 1)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:air-node-count" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_NEG_JSON_FILE" \
    "$NEG_JSON" \
    "air_json"

assert_llvm_leg "self-host-parity:air-node-count" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR" "$FIXTURE_REL"
echo "[self-host-parity:air-node-count] rung-1 parity ok (expected-json clean+corrupt; corrupted fixture rc=1)"
