#!/usr/bin/env bash
# Parity gate for completeness_impact_planner.

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
        echo "[self-host-parity:completeness-impact-planner] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:completeness-impact-planner] missing compiler binary: $PGY" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/completeness_impact_planner}"
HARNESS_PATHS_FILE="$BUILD_DIR/completeness_impact_planner_paths.txt"
mkdir -p "$BUILD_DIR"

pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:completeness-impact-planner" \
    "$BUILD_DIR" \
    "self-host-completeness-impact-planner-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done < "$HARNESS_PATHS_FILE"

if [[ "${#harness_paths[@]}" -ne 11 ]]; then
    echo "[self-host-parity:completeness-impact-planner] expected 11 path rows, got ${#harness_paths[@]}" >&2
    cat "$HARNESS_PATHS_FILE" >&2
    exit 1
fi

TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_CLEAN="$ROOT_DIR/${harness_paths[1]}"
EXPECTED_UNMATCHED="$ROOT_DIR/${harness_paths[2]}"
EXPECTED_RUN_GROUPS="$ROOT_DIR/${harness_paths[3]}"
CLEAN_ARGS=("${harness_paths[@]:4:6}")
UNMATCHED_ARG="${harness_paths[10]}"

for path in "$TOOL_SOURCE" "$EXPECTED_CLEAN" "$EXPECTED_UNMATCHED" "$EXPECTED_RUN_GROUPS"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:completeness-impact-planner] missing input: $path" >&2
        exit 1
    fi
done

TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")"
CLEAN_BIN="$BUILD_DIR/completeness_impact_planner_c.exe"
CLEAN_COMPILE_LOG="$BUILD_DIR/completeness_impact_planner_c.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$TOOL_ARG" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$CLEAN_BIN")" >"$CLEAN_COMPILE_LOG" 2>&1); then
    echo "[self-host-parity:completeness-impact-planner] C backend compile failed" >&2
    cat "$CLEAN_COMPILE_LOG" >&2
    exit 1
fi
if ! pgy_require_runnable_binary_here "self-host-parity:completeness-impact-planner" "$CLEAN_BIN"; then
    exit 1
fi

CLEAN_OUT="$BUILD_DIR/clean.out"
set +e
(cd "$ROOT_DIR" && "$CLEAN_BIN" "${CLEAN_ARGS[@]}" 2>/dev/null | pgy_selfhost_normalize_text_artifact >"$CLEAN_OUT")
CLEAN_RC=$?
set -e
if [[ "$CLEAN_RC" -ne 0 ]]; then
    echo "[self-host-parity:completeness-impact-planner] clean run failed rc=$CLEAN_RC" >&2
    cat "$CLEAN_OUT" >&2
    exit 1
fi
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:completeness-impact-planner" \
    "$BUILD_DIR" \
    "$EXPECTED_CLEAN" \
    "$CLEAN_OUT" \
    "run_output"

RUN_GROUPS_OUT="$BUILD_DIR/run_groups.out"
set +e
(cd "$ROOT_DIR" && "$CLEAN_BIN" --run-groups "${CLEAN_ARGS[@]}" 2>/dev/null | pgy_selfhost_normalize_text_artifact >"$RUN_GROUPS_OUT")
RUN_GROUPS_RC=$?
set -e
if [[ "$RUN_GROUPS_RC" -ne 0 ]]; then
    echo "[self-host-parity:completeness-impact-planner] run-groups run failed rc=$RUN_GROUPS_RC" >&2
    cat "$RUN_GROUPS_OUT" >&2
    exit 1
fi
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:completeness-impact-planner" \
    "$BUILD_DIR" \
    "$EXPECTED_RUN_GROUPS" \
    "$RUN_GROUPS_OUT" \
    "run_group_plan"

UNMATCHED_OUT="$BUILD_DIR/unmatched.out"
set +e
(cd "$ROOT_DIR" && "$CLEAN_BIN" "$UNMATCHED_ARG" 2>/dev/null | pgy_selfhost_normalize_text_artifact >"$UNMATCHED_OUT")
UNMATCHED_RC=$?
set -e
if [[ "$UNMATCHED_RC" -ne 1 ]]; then
    echo "[self-host-parity:completeness-impact-planner] unmatched path expected rc=1, got rc=$UNMATCHED_RC" >&2
    cat "$UNMATCHED_OUT" >&2
    exit 1
fi
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-parity:completeness-impact-planner" \
    "$BUILD_DIR" \
    "$EXPECTED_UNMATCHED" \
    "$UNMATCHED_OUT" \
    "run_output"

assert_llvm_leg "self-host-parity:completeness-impact-planner" "$TOOL_ARG" "$BUILD_DIR" "${CLEAN_ARGS[@]}"
echo "[self-host-parity:completeness-impact-planner] parity ok"
