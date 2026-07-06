#!/usr/bin/env bash
# Rung 1 parity for the AIR graph referential-integrity checker.
#
# Pergyra is the origin
# (src/self_hosted/tools/air_graph_ref_integrity/main.pgy).
# TestHarness-projected expected artifacts own the clean and dangling-output
# oracles. Shell only runs the process and checks the dangling fixture rc=1.

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
        echo "[self-host-parity:air-ref-integrity] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:air-ref-integrity] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/air_graph_ref_integrity}"
HARNESS_PATHS_FILE="$PERGYRA_TOOL_BUILD_DIR/air_graph_ref_integrity_harness_paths.txt"
mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:air-ref-integrity" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "air-graph-ref-integrity-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 6 ]]; then
    echo "[self-host-parity:air-ref-integrity] TestHarness manifest expected 6 paths, got ${#harness_paths[@]}" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
AIR_GRAPH_SCAN_OWNER="$ROOT_DIR/${harness_paths[1]}"
EXPECTED_JSON_FILE="$ROOT_DIR/${harness_paths[2]}"
FIXTURE_REL="${harness_paths[3]}"
DANGLING_FIXTURE_REL="${harness_paths[4]}"
EXPECTED_DANGLING_JSON_FILE="$ROOT_DIR/${harness_paths[5]}"
FIXTURE_FILE="$ROOT_DIR/$FIXTURE_REL"
DANGLING_FIXTURE_FILE="$ROOT_DIR/$DANGLING_FIXTURE_REL"

if [[ -z "${harness_paths[5]}" ]]; then
    echo "[self-host-parity:air-ref-integrity] missing expected dangling JSON row" >&2
    exit 1
fi

for path in "$PERGYRA_TOOL_SOURCE" "$AIR_GRAPH_SCAN_OWNER" "$EXPECTED_JSON_FILE" "$EXPECTED_DANGLING_JSON_FILE" "$FIXTURE_FILE" "$DANGLING_FIXTURE_FILE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:air-ref-integrity] missing input: $path" >&2
        exit 1
    fi
done

PERGYRA_TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL_SOURCE")"
CLEAN_BIN="$PERGYRA_TOOL_BUILD_DIR/air_graph_ref_integrity_c.exe"
CLEAN_COMPILE_LOG="$PERGYRA_TOOL_BUILD_DIR/air_graph_ref_integrity_c.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$CLEAN_BIN")" >"$CLEAN_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:air-ref-integrity] C backend compile failed" >&2
    cat "$CLEAN_COMPILE_LOG" >&2
    exit 1
fi
if ! pgy_require_runnable_binary_here "self-host-parity:air-ref-integrity" "$CLEAN_BIN"; then
    exit 1
fi

# Clean run.
set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$CLEAN_BIN" "$FIXTURE_REL" 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:air-ref-integrity] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi

PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | tr -d '\r' \
    | grep -F 'pgy.selfhost.air-ref-integrity.v1' \
    | tail -n 1)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:air-ref-integrity" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_JSON_FILE" \
    "$PERGYRA_JSON" \
    "air_json"

# Negative fixture: one dangling endpoint, expect rc=1.
set +e
NEG_OUT="$(cd "$ROOT_DIR" && "$CLEAN_BIN" "$DANGLING_FIXTURE_REL" 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:air-ref-integrity] dangling fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
NEG_JSON="$(printf '%s\n' "$NEG_OUT" \
    | tr -d '\r' \
    | grep -F 'pgy.selfhost.air-ref-integrity.v1' \
    | tail -n 1)"
pgy_selfhost_compare_expected_text_artifact_with_owner \
    "self-host-parity:air-ref-integrity" \
    "$PERGYRA_TOOL_BUILD_DIR" \
    "$EXPECTED_DANGLING_JSON_FILE" \
    "$NEG_JSON" \
    "air_json"

assert_llvm_leg "self-host-parity:air-ref-integrity" "$PERGYRA_TOOL_ARG" "$PERGYRA_TOOL_BUILD_DIR" "$FIXTURE_REL"
echo "[self-host-parity:air-ref-integrity] rung-1 parity ok (expected-json clean+dangling; dangling fixture rc=1)"
