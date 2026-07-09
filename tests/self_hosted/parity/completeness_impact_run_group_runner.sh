#!/usr/bin/env bash
# Bounded runner smoke for completeness_impact_planner run_group_plan output.
#
# The runner consumes the Pergyra-owned TSV plan. It must not reconstruct
# changed-path impact decisions from path classes or proof-gate names.

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
        echo "[self-host-completeness-impact-runner] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-completeness-impact-runner] missing compiler binary: $PGY" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/completeness_impact_runner}"
HARNESS_PATHS_FILE="$BUILD_DIR/completeness_impact_planner_paths.txt"
PLAN_FILE="$BUILD_DIR/run_groups.tsv"
mkdir -p "$BUILD_DIR"

pgy_selfhost_read_test_harness_manifest \
    "self-host-completeness-impact-runner" \
    "$BUILD_DIR" \
    "self-host-completeness-impact-planner-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done < "$HARNESS_PATHS_FILE"

if [[ "${#harness_paths[@]}" -ne 10 ]]; then
    echo "[self-host-completeness-impact-runner] expected 10 path rows, got ${#harness_paths[@]}" >&2
    cat "$HARNESS_PATHS_FILE" >&2
    exit 1
fi

TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
CLEAN_ARGS=("${harness_paths[@]:4:5}")
for path in "$TOOL_SOURCE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-completeness-impact-runner] missing input: $path" >&2
        exit 1
    fi
done

RUNNER_ARGS=("${CLEAN_ARGS[@]}")
if [[ -n "${PGY_SELFHOST_IMPACT_CHANGED_PATHS_FILE:-}" ]]; then
    if [[ ! -f "$PGY_SELFHOST_IMPACT_CHANGED_PATHS_FILE" ]]; then
        echo "[self-host-completeness-impact-runner] changed-path file missing: $PGY_SELFHOST_IMPACT_CHANGED_PATHS_FILE" >&2
        exit 1
    fi
    RUNNER_ARGS=()
    while IFS= read -r changed_path || [[ -n "$changed_path" ]]; do
        changed_path="${changed_path%$'\r'}"
        [[ -n "$changed_path" ]] || continue
        RUNNER_ARGS+=("$changed_path")
    done < "$PGY_SELFHOST_IMPACT_CHANGED_PATHS_FILE"
elif [[ -n "${PGY_SELFHOST_IMPACT_CHANGED_PATHS:-}" ]]; then
    RUNNER_ARGS=()
    IFS=',' read -r -a changed_path_items <<< "$PGY_SELFHOST_IMPACT_CHANGED_PATHS"
    for changed_path in "${changed_path_items[@]}"; do
        changed_path="${changed_path%$'\r'}"
        [[ -n "$changed_path" ]] || continue
        RUNNER_ARGS+=("$changed_path")
    done
fi

if [[ "${#RUNNER_ARGS[@]}" -eq 0 ]]; then
    echo "[self-host-completeness-impact-runner] no changed paths provided" >&2
    exit 1
fi

PLANNER_BIN="$BUILD_DIR/completeness_impact_planner_c.exe"
PLANNER_COMPILE_LOG="$BUILD_DIR/completeness_impact_planner_c.compile.log"
if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$PLANNER_BIN")" >"$PLANNER_COMPILE_LOG" 2>&1); then
    echo "[self-host-completeness-impact-runner] planner C backend compile failed" >&2
    cat "$PLANNER_COMPILE_LOG" >&2
    exit 1
fi
if ! pgy_require_runnable_binary_here "self-host-completeness-impact-runner" "$PLANNER_BIN"; then
    exit 1
fi

if ! (cd "$ROOT_DIR" && "$PLANNER_BIN" --run-groups "${RUNNER_ARGS[@]}" | tr -d '\r' >"$PLAN_FILE"); then
    echo "[self-host-completeness-impact-runner] planner run-group projection failed" >&2
    cat "$PLAN_FILE" >&2 || true
    exit 1
fi

MAKE_BIN="${PGY_SELFHOST_IMPACT_RUNNER_MAKE:-${MAKE:-}}"
if [[ -z "$MAKE_BIN" ]]; then
    if command -v mingw32-make >/dev/null 2>&1; then
        MAKE_BIN="$(command -v mingw32-make)"
    elif command -v make >/dev/null 2>&1; then
        MAKE_BIN="$(command -v make)"
    else
        echo "[self-host-completeness-impact-runner] missing make" >&2
        exit 1
    fi
fi

EXECUTE_MODE="${PGY_SELFHOST_IMPACT_RUNNER_EXECUTE:-1}"
MAX_GROUPS="${PGY_SELFHOST_IMPACT_RUNNER_MAX_GROUPS:-1}"
if [[ "$EXECUTE_MODE" != "0" && "$EXECUTE_MODE" != "1" ]]; then
    echo "[self-host-completeness-impact-runner] PGY_SELFHOST_IMPACT_RUNNER_EXECUTE must be 0 or 1" >&2
    exit 1
fi
MAX_GROUPS_ALL=0
if [[ "$MAX_GROUPS" == "all" ]]; then
    MAX_GROUPS_ALL=1
elif ! [[ "$MAX_GROUPS" =~ ^[0-9]+$ ]]; then
    echo "[self-host-completeness-impact-runner] PGY_SELFHOST_IMPACT_RUNNER_MAX_GROUPS must be a non-negative integer or all" >&2
    exit 1
fi

schema_seen=0
group_count=0
executed_count=0
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    if [[ "$line" == schema=* ]]; then
        if [[ "$line" != "schema=pgy.selfhost.completeness-impact-run-groups.v1" ]]; then
            echo "[self-host-completeness-impact-runner] unexpected schema row: $line" >&2
            exit 1
        fi
        schema_seen=1
        continue
    fi
    if [[ "$line" == finding$'\t'* ]]; then
        echo "[self-host-completeness-impact-runner] planner emitted finding row: $line" >&2
        exit 1
    fi

    IFS=$'\t' read -r proof_gate source_env source_value stage_env stage_value impact_ids extra <<< "$line"
    if [[ -n "${extra:-}" || -z "${proof_gate:-}" || -z "${source_env:-}" || -z "${source_value:-}" \
        || -z "${stage_env:-}" || -z "${stage_value:-}" || -z "${impact_ids:-}" ]]; then
        echo "[self-host-completeness-impact-runner] malformed run group row: $line" >&2
        exit 1
    fi
    if ! grep -Fq "$proof_gate:" "$ROOT_DIR/Makefile"; then
        echo "[self-host-completeness-impact-runner] proof gate is not a Make target: $proof_gate" >&2
        exit 1
    fi

    env_args=()
    if [[ "$source_env" == "-" || "$stage_env" == "-" ]]; then
        if [[ "$source_env" != "-" || "$stage_env" != "-" || "$stage_value" != "-" ]]; then
            echo "[self-host-completeness-impact-runner] partial env-disabled run group: $line" >&2
            exit 1
        fi
    else
        if [[ "$source_env" != "PGY_SELFHOST_COMPLETENESS_SOURCES" || "$stage_env" != "PGY_SELFHOST_COMPLETENESS_STAGES" ]]; then
            echo "[self-host-completeness-impact-runner] unknown run group env field: $line" >&2
            exit 1
        fi
        env_args+=("$source_env=$source_value")
        env_args+=("$stage_env=$stage_value")
    fi

    group_count=$((group_count + 1))
    if [[ "$EXECUTE_MODE" == "1" && ( "$MAX_GROUPS_ALL" == "1" || "$executed_count" -lt "$MAX_GROUPS" ) ]]; then
        echo "[self-host-completeness-impact-runner] executing $proof_gate impact_ids=$impact_ids"
        env MAKEFLAGS= PGY_BIN="$PGY" "${env_args[@]}" "$MAKE_BIN" --no-print-directory -C "$ROOT_DIR" "$proof_gate"
        executed_count=$((executed_count + 1))
    fi
done < "$PLAN_FILE"

if [[ "$schema_seen" -ne 1 ]]; then
    echo "[self-host-completeness-impact-runner] schema row missing" >&2
    cat "$PLAN_FILE" >&2
    exit 1
fi
if [[ "$group_count" -eq 0 ]]; then
    echo "[self-host-completeness-impact-runner] run group plan is empty" >&2
    cat "$PLAN_FILE" >&2
    exit 1
fi

echo "[self-host-completeness-impact-runner] run-group plan ok groups=$group_count executed=$executed_count"
