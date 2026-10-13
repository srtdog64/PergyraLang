#!/usr/bin/env bash
# MIR -> AIR -> verified parallel-capture carrier contract.
#
# The semantic/MIR fact table is the only producer.  C and LLVM consume the
# AIR-bound carrier; a backend must not re-query the MIR disposition table.
set -euo pipefail

# Subject of this gate:
#   the parallel capture projection changed.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, a self-host coverage gap would read as a regression in
# the subject above. Declared per harness because the compiler is
# reached through make and nested scripts, and the variable is the
# same declared opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LABEL="parallel-capture-projection"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[$LABEL] FAIL: $*" >&2
    exit 1
}

# Exercise the carrier through both production backends.  This fixture has a
# real snapshot capture, so a declaration-only LLVM success is insufficient.
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
if [[ -x "$PGY" ]] && pgy_binary_is_runnable_here "$PGY"; then
    TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
    if pgy_binary_expects_windows_paths "$PGY"; then
        TMP_BASE="$ROOT_DIR/.tmp"
        mkdir -p "$TMP_BASE"
    fi
    WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_capture_projection.XXXXXX")"
    trap 'rm -rf "$WORK_DIR"' EXIT
    cp "$ROOT_DIR/tests/cases/parallel_snapshot/snapshot_read.pgy" \
       "$WORK_DIR/main.pgy"
    for backend in c llvm; do
        src="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/main.pgy")"
        out="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/main_${backend}.exe")"
        if ! (cd "$ROOT_DIR" && "$PGY" "$src" --backend="$backend" -o "$out") \
                >"$WORK_DIR/${backend}.log" 2>&1; then
            if [[ "$backend" == "llvm" ]] \
                    && pgy_selfhost_log_reports_no_llvm \
                        "$WORK_DIR/${backend}.log"; then
                echo "[$LABEL] LLVM snapshot fixture skipped; compiler built without LLVM backend support"
                continue
            fi
            echo "[$LABEL] $backend fixture failed" >&2
            cat "$WORK_DIR/${backend}.log" >&2
            exit 1
        fi
        got="$($out | tr -d '\r')"
        if [[ "$got" != $'42\n1' ]]; then
            fail "$backend fixture printed '$got', expected 42/1"
        fi
    done
    echo "[$LABEL] C/LLVM snapshot fixture compiled and ran through the carrier"
else
    echo "[$LABEL] SKIP executable parity probe; compiler is unavailable here"
fi
