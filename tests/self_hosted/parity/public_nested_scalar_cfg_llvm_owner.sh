#!/usr/bin/env bash
# Public LLVM file/stdout publication for the general scalar CFG rung.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-public-scalar-cfg-llvm"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
[[ -x "$PGY" && -x "$DRIVER" ]] || fail "public or installed driver is missing"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"

run_case() {
    local source="$1" stem="$2" expected="$3"
    local work_rel=".tmp/self_hosted/public_scalar_cfg_llvm/$stem"
    local work_dir="$ROOT_DIR/$work_rel"
    mkdir -p "$work_dir"; rm -f "$work_dir"/*
    (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && "$PGY" --mir-json "$source") \
        >"$work_dir/public.mir.json" 2>"$work_dir/mir.err" ||
        fail "$stem public source-MIR production failed"
    (cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm \
        "$work_rel/public.mir.json" -o "$work_rel/direct.ll") ||
        fail "$stem installed LLVM projection failed"
    (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" "$source" --emit-llvm -o "$work_rel/public.ll") \
        >"$work_dir/public.out" 2>"$work_dir/public.err" ||
        fail "$stem public LLVM file publication failed"
    cmp -s "$work_dir/direct.ll" "$work_dir/public.ll" ||
        fail "$stem public file LLVM differs from installed projection"
    ! grep -Fq '[pipeline timing]' "$work_dir/public.err" ||
        fail "$stem public file re-entered the native compiler pipeline"
    (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" "$source" --emit-llvm) >"$work_dir/stdout.ll" \
        2>"$work_dir/stdout.err" || fail "$stem public LLVM stdout failed"
    cmp -s "$work_dir/direct.ll" "$work_dir/stdout.ll" ||
        fail "$stem public stdout LLVM differs from installed projection"
    ! grep -Fq '[pipeline timing]' "$work_dir/stdout.err" ||
        fail "$stem public stdout re-entered the native compiler pipeline"
    "$CLANG" -x ir "$work_dir/public.ll" -o "$work_dir/program.exe" \
        >"$work_dir/clang.out" 2>"$work_dir/clang.err" ||
        fail "$stem public LLVM did not compile"
    "$work_dir/program.exe" | tr -d '\r' >"$work_dir/program.out"
    printf '%s' "$expected" >"$work_dir/expected.out"
    cmp -s "$work_dir/expected.out" "$work_dir/program.out" ||
        fail "$stem public LLVM execution drifted"
}

run_case src/self_hosted/mir_lower/fixture/nested_if_in_loop.pgy nested $'1\n1\n'
run_case src/self_hosted/mir_lower/fixture/break_after_stmt.pgy break-exit $'3\n3\n'
run_case src/self_hosted/mir_lower/fixture/multiple_break_exit.pgy multi-break $'2\n'
run_case src/self_hosted/mir_lower/fixture/for_break_exit.pgy for-break $'3\n'
run_case examples/break_continue.pgy continue-backedge $'42\n'
echo "[$LABEL] public file/stdout use installed scalar CFG LLVM through continue backedges"
