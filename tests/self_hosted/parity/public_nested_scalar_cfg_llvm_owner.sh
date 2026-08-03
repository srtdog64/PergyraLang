#!/usr/bin/env bash
# Public LLVM file/stdout publication for the active general scalar CFG rung.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-public-nested-scalar-cfg-llvm"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
SOURCE="src/self_hosted/mir_lower/fixture/nested_if_in_loop.pgy"
WORK_REL=".tmp/self_hosted/public_nested_scalar_cfg_llvm"
WORK_DIR="$ROOT_DIR/$WORK_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
PGY="$(pgy_select_optional_exe_binary "$PGY")"
DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$PGY" && -x "$DRIVER" ]] || fail "public or installed driver is missing"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && \
    "$PGY" --mir-json "$SOURCE") >"$WORK_DIR/public.mir.json" \
    2>"$WORK_DIR/mir.err" || fail "public source-MIR production failed"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm \
    "$WORK_REL/public.mir.json" -o "$WORK_REL/direct.ll") \
    >"$WORK_DIR/direct.out" 2>"$WORK_DIR/direct.err" ||
    fail "installed LLVM projection failed"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$SOURCE" --emit-llvm \
    -o "$WORK_REL/public.ll") >"$WORK_DIR/public.out" \
    2>"$WORK_DIR/public.err" || fail "public LLVM file publication failed"
cmp -s "$WORK_DIR/direct.ll" "$WORK_DIR/public.ll" ||
    fail "public file LLVM differs from the installed projection"
! grep -Fq '[pipeline timing]' "$WORK_DIR/public.err" ||
    fail "public file publication re-entered the native compiler pipeline"

(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$SOURCE" --emit-llvm) \
    >"$WORK_DIR/stdout.ll" 2>"$WORK_DIR/stdout.err" ||
    fail "public LLVM stdout publication failed"
cmp -s "$WORK_DIR/direct.ll" "$WORK_DIR/stdout.ll" ||
    fail "public stdout LLVM differs from the installed projection"
! grep -Fq '[pipeline timing]' "$WORK_DIR/stdout.err" ||
    fail "public stdout publication re-entered the native compiler pipeline"

"$CLANG" -x ir "$WORK_DIR/public.ll" -o "$WORK_DIR/program.exe" \
    >"$WORK_DIR/clang.out" 2>"$WORK_DIR/clang.err" || {
        cat "$WORK_DIR/clang.err" >&2; fail "public LLVM did not compile";
    }
"$WORK_DIR/program.exe" | tr -d '\r' >"$WORK_DIR/program.out"
printf '1\n1\n' >"$WORK_DIR/expected.out"
cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/program.out" ||
    fail "public LLVM did not execute exact 1/1"

echo "[$LABEL] public file/stdout use installed scalar CFG LLVM and execute 1/1"
