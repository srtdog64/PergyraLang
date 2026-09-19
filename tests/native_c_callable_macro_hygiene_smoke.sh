#!/usr/bin/env bash
# A Pergyra-owned callable keeps its declared identifier even when a host
# runtime header defines the same spelling as a function-like macro.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

# Subject of this gate:
#   native C translation-unit ownership of non-extern callable identifiers.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/windows_c_symbol_collision.pgy"
TMP_BASE="${TMPDIR:-${TEMP:-$ROOT_DIR/.tmp}}"
mkdir -p "$TMP_BASE"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy-native-c-macro.XXXXXX")"
trap 'rm -rf -- "$WORK_DIR"' EXIT

fail() {
    echo "[native-c-macro] $*" >&2
    exit 1
}

pgy_require_runnable_binary_here "native-c-macro" "$PGY" || exit 1
grep -Fq 'transpiler_emit_owned_callable_macro_hygiene(' \
    "$ROOT_DIR/src/codegen/transpiler.c" ||
    fail "C program owner lost callable macro hygiene"
grep -Fq 'transpiler_active_externs(ctx, &externs, &exten_count);' \
    "$ROOT_DIR/src/codegen/transpiler.c" ||
    fail "extern ABI declarations lost their separate inventory"

suffix=""
pgy_binary_expects_windows_paths "$PGY" && suffix=".exe"
program="$WORK_DIR/callable$suffix"
emitted="$WORK_DIR/callable.c"
source_arg="$(pgy_path_for_compiler "$PGY" "$SOURCE")"
program_arg="$(pgy_path_for_compiler "$PGY" "$program")"
emitted_arg="$(pgy_path_for_compiler "$PGY" "$emitted")"

(cd "$ROOT_DIR" && "$PGY" "$source_arg" --native-pipeline \
    --backend=c -o "$program_arg") >"$WORK_DIR/compile.out" \
    2>"$WORK_DIR/compile.err" || {
        cat "$WORK_DIR/compile.out" "$WORK_DIR/compile.err" >&2
        fail "C compilation rejected the owned FindResource declaration"
    }
[[ -x "$program" ]] || fail "C backend published no executable"
"$program" | tr -d '\r' >"$WORK_DIR/run.out"
printf '42\n' >"$WORK_DIR/expected.out"
cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/run.out" ||
    fail "owned callable runtime result drifted"

(cd "$ROOT_DIR" && "$PGY" "$source_arg" --native-pipeline \
    --emit-c -o "$emitted_arg") >"$WORK_DIR/emit.out" \
    2>"$WORK_DIR/emit.err" || fail "C emission failed"
grep -Fq '#ifdef FindResource' "$emitted" ||
    fail "emitted C did not test the colliding macro"
grep -Fq '#undef FindResource' "$emitted" ||
    fail "emitted C did not remove the colliding macro"

echo "[native-c-macro] owned FindResource callable survives host headers: PASS"
