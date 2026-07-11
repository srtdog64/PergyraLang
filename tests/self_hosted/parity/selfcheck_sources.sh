#!/usr/bin/env bash
# Real-source self-application for the Pergyra-origin semantic checker.
#
# The semantic_parity.sh gate proves the checker on bounded toy fixtures. This
# script is the next rung: it runs the compiled checker on ACTUAL self-host
# production sources and asserts each owner-selected semantic target is accepted
# (a clean `Status: ok` diagnostic).
#
# Source inventory ownership lives in completeness_ledger_owner.pgy and is
# projected through TestHarness. This runner executes those rows; it must not
# keep a parallel shell-owned source list.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
source "$ROOT_DIR/tests/portable_process_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    if [[ -z "${PGY_BIN:-}" ]]; then
        echo "[self-host-selfcheck] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-selfcheck] missing compiler binary: $PGY" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/semantic_selfcheck}"
RUN_ID="${PGY_SELFHOST_RUN_ID:-$$}"
HARNESS_PATHS_FILE="$BUILD_DIR/semantic_harness_paths.txt"
SEMANTIC_TARGET_MANIFEST="$BUILD_DIR/semantic_targets.txt"
CHECK_TIMEOUT_SEC="${PGY_SELFHOST_SELFCHECK_TIMEOUT_SEC:-60}"
TIMEOUT_EXIT_CODE=124
mkdir -p "$BUILD_DIR"

read_manifest() {
    local suite="$1"
    local out_file="$2"
    pgy_selfhost_read_test_harness_manifest \
        "self-host-selfcheck" \
        "$BUILD_DIR/manifest" \
        "$suite" \
        "$out_file"
}

read_manifest "semantic-parity-paths" "$HARNESS_PATHS_FILE"
read_manifest "self-host-completeness-semantic-targets" "$SEMANTIC_TARGET_MANIFEST"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done < "$HARNESS_PATHS_FILE"

if [[ "${#harness_paths[@]}" -lt 7 ]]; then
    echo "[self-host-selfcheck] semantic path manifest too short: ${#harness_paths[@]}" >&2
    cat "$HARNESS_PATHS_FILE" >&2
    exit 1
fi

TOOL_SOURCE="$ROOT_DIR/${harness_paths[0]}"
if [[ ! -f "$TOOL_SOURCE" ]]; then
    echo "[self-host-selfcheck] semantic checker source missing: $TOOL_SOURCE" >&2
    exit 1
fi

SELF_TARGET_ROWS=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    SELF_TARGET_ROWS+=("$line")
done < "$SEMANTIC_TARGET_MANIFEST"

self_source_count="${#SELF_TARGET_ROWS[@]}"
if [[ "$self_source_count" -eq 0 ]]; then
    echo "[self-host-selfcheck] empty semantic target manifest" >&2
    exit 1
fi

BACKENDS="${PGY_SELFHOST_SEMANTIC_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    TOOL_BIN="$BUILD_DIR/main_selfcheck_${backend}_${RUN_ID}.exe"
    compile_log="$BUILD_DIR/main_selfcheck_${backend}_${RUN_ID}.compile.log"
    echo "[self-host-selfcheck] compiling checker backend=$backend..."
    rm -f "$TOOL_BIN"
    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")" \
        --backend="$backend" -o "$(pgy_path_for_compiler "$PGY" "$TOOL_BIN")" \
        >"$compile_log" 2>&1); then
        echo "[self-host-selfcheck] backend=$backend checker compile failed" >&2
        cat "$compile_log" >&2
        exit 1
    fi

    source_index=0
    for row in "${SELF_TARGET_ROWS[@]}"; do
        if [[ "$row" != *$'\t'* ]]; then
            echo "[self-host-selfcheck] malformed semantic target row: $row" >&2
            exit 1
        fi
        src="${row%%$'\t'*}"
        target="${row#*$'\t'}"
        source_index=$((source_index + 1))
        if [[ "$src" == "$target" ]]; then
            label="$src"
        else
            label="$src -> $target"
        fi
        echo "[self-host-selfcheck] backend=$backend checking $source_index/$self_source_count $label"
        out_file="$BUILD_DIR/selfcheck_${backend}_${source_index}.out"
        err_file="$BUILD_DIR/selfcheck_${backend}_${source_index}.err"
        set +e
        (cd "$ROOT_DIR" && pgy_run_with_timeout \
            "$CHECK_TIMEOUT_SEC" "$out_file" "$err_file" \
            "$TOOL_BIN" --check "$target")
        rc="$?"
        set -e
        if [[ "$rc" -eq "$TIMEOUT_EXIT_CODE" ]]; then
            echo "[self-host-selfcheck] backend=$backend $label: timed out after ${CHECK_TIMEOUT_SEC}s" >&2
            cat "$out_file" "$err_file" >&2
            exit 1
        fi
        if [[ "$rc" -ne 0 ]]; then
            echo "[self-host-selfcheck] backend=$backend $label: checker exited $rc" >&2
            cat "$out_file" "$err_file" >&2
            exit 1
        fi
        out="$(tr -d '\r' < "$out_file")"
        if ! grep -Fq 'Diagnostic: pgy.selfhost.semantic.v1' <<<"$out"; then
            echo "[self-host-selfcheck] backend=$backend $label: no diagnostic block" >&2
            printf '%s\n' "$out" >&2
            exit 1
        fi
        if ! grep -Fq 'Status: ok' <<<"$out"; then
            echo "[self-host-selfcheck] backend=$backend $label: checker rejected real source" >&2
            printf '%s\n' "$out" >&2
            exit 1
        fi
    done
    echo "[self-host-selfcheck] backend=$backend ok: $self_source_count real sources accepted"
done

echo "[self-host-selfcheck] real-source self-application ok: $self_source_count sources; backends=$BACKENDS"
