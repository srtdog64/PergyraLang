#!/usr/bin/env bash
# MIR -> AIR -> verified parallel-capture carrier contract.
#
# The semantic/MIR fact table is the only producer.  C and LLVM consume the
# AIR-bound carrier; a backend must not re-query the MIR disposition table.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LABEL="parallel-capture-projection"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[$LABEL] FAIL: $*" >&2
    exit 1
}

require_text() {
    local path="$1" term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$path" \
        || fail "missing '$term' in $path"
}

require_text src/compiler/verified_projection_plan.h \
    "PgyVerifiedParallelCapturePlan"
require_text src/compiler/verified_projection_plan.h \
    "pgy_verified_parallel_capture_plan_from_air"
require_text src/compiler/verified_parallel_capture_plan.c \
    "pgy_verified_parallel_capture_plan_identity_ready"
require_text src/compiler/verified_parallel_capture_plan.c \
    "Every MIR capture boundary must have certified AIR evidence"
require_text src/compiler/compiler.c \
    "pgy_verified_parallel_capture_plan_from_air"
require_text src/compiler/compiler.c \
    "transpile_from_mir_with_projection_plans"
require_text src/compiler/compiler_llvm.c \
    "pgy_verified_parallel_capture_plan_from_air"
require_text src/compiler/compiler_llvm.c \
    "llvm_codegen_from_mir_with_projection_plans"

for path in \
    src/codegen/transpiler_async_parallel_emit.c \
    src/codegen/transpiler_parallel_join_emit.c \
    src/codegen/llvm_stmt_parallel_async.c \
    src/codegen/llvm_stmt_parallel_join_capture.c; do
    if grep -Fq -- "mir_parallel_capture_disposition_find" "$ROOT_DIR/$path"; then
        fail "$path directly consumes the MIR capture disposition owner"
    fi
    require_text "$path" "pgy_verified_parallel_capture_disposition_find"
done

for path in src/compiler/verified_parallel_capture_plan.c \
            src/compiler/mir_parallel_capture_facts.c; do
    require_text "$path" "MIR_PARALLEL_CAPTURE_SNAPSHOT_COPY"
done

# The old API surface is intentionally still callable for source compatibility,
# but active compiler entrypoints pass NULL and fail closed.  This keeps a
# missing carrier observable instead of silently restoring backend-local reads.
require_text src/codegen/transpiler_entry.c \
    "C backend: verified parallel-capture plan required"
require_text src/codegen/llvm_api.c \
    "LLVM backend: verified parallel-capture plan required"
require_text src/codegen/llvm_runtime_row.c \
    "ctx != NULL && ctx->current_mir_routine == NULL"
require_text src/codegen/llvm_runtime_row.c \
    "module-level ABI materialization phase"

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

echo "[$LABEL] MIR owner -> AIR carrier -> C/LLVM consumers are wired; direct owner lookup is absent"
