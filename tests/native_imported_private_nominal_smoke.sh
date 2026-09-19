#!/usr/bin/env bash
# An exported API may carry a private nominal opaquely. Import normalization,
# generic ABI naming and enum construction must agree without making the
# private constructor visible to the consumer.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

# Subject of this gate:
#   native imported-private nominal carriage through Result and exported ADT.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
BACKENDS="${PGY_NATIVE_BOUNDARY_BACKENDS:-c}"
FIXTURES="$ROOT_DIR/tests/self_hosted/fixtures/imported_private_nominal"
TMP_BASE="${TMPDIR:-${TEMP:-$ROOT_DIR/.tmp}}"
mkdir -p "$TMP_BASE"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy-native-private-nominal.XXXXXX")"
trap 'rm -rf -- "$WORK_DIR"' EXIT

fail() {
    echo "[native-private-nominal] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$ROOT_DIR/$1" ||
        fail "missing owner contract in $1: $2"
}

pgy_require_runnable_binary_here "native-private-nominal" "$PGY" || exit 1
require_text src/compiler/module_normalizer_refs.c \
    'ast_enum_variant_param(node, i, j)'
require_text src/codegen/transpiler_type_mapping.c \
    'if (!sanitize_c_suffix(inner, suffix, sizeof(suffix)))'
require_text src/codegen/transpiler_type_result_mapping_helpers.c \
    'return sanitize_c_suffix(inner, out, out_size);'
require_text src/codegen/llvm_stmt_type_infer_helpers.c \
    'variant = llvm_lookup_enum_variant_qualified(ctx, owner, member);'

suffix=""
pgy_binary_expects_windows_paths "$PGY" && suffix=".exe"
printf '47\n1\n' >"$WORK_DIR/result.expected"
printf '53\n1\n' >"$WORK_DIR/outcome.expected"

for stem in result_main outcome_main; do
    source_arg="$(pgy_path_for_compiler "$PGY" "$FIXTURES/$stem.pgy")"
    for backend in $BACKENDS; do
        program="$WORK_DIR/$stem-$backend$suffix"
        program_arg="$(pgy_path_for_compiler "$PGY" "$program")"
        (cd "$ROOT_DIR" && "$PGY" "$source_arg" --native-pipeline \
            "--backend=$backend" -o "$program_arg") \
            >"$WORK_DIR/$stem-$backend.compile.out" \
            2>"$WORK_DIR/$stem-$backend.compile.err" || {
                cat "$WORK_DIR/$stem-$backend.compile.out" \
                    "$WORK_DIR/$stem-$backend.compile.err" >&2
                fail "$stem $backend compilation failed"
            }
        [[ -x "$program" ]] ||
            fail "$stem $backend published no executable"
        "$program" | tr -d '\r' >"$WORK_DIR/$stem-$backend.run.out"
        cmp -s "$WORK_DIR/${stem%_main}.expected" \
            "$WORK_DIR/$stem-$backend.run.out" ||
            fail "$stem $backend runtime output drifted"
    done
done

result_c="$WORK_DIR/result.c"
(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$FIXTURES/result_main.pgy")" \
    --native-pipeline --emit-c \
    -o "$(pgy_path_for_compiler "$PGY" "$result_c")") \
    >"$WORK_DIR/result.emit.out" 2>"$WORK_DIR/result.emit.err" ||
    fail "Result C emission failed"
grep -Fq 'PgyResult_imp0_ValidatedRenderPlan_RenderPlanError' "$result_c" ||
    fail "Result ABI lost its canonical imported-private suffix"
if grep -Fq 'PgyResult___imp0_ValidatedRenderPlan_RenderPlanError' "$result_c"; then
    fail "Result ABI reintroduced a second raw generic suffix owner"
fi

for backend in $BACKENDS; do
    program="$WORK_DIR/forged-$backend$suffix"
    program_arg="$(pgy_path_for_compiler "$PGY" "$program")"
    if (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$FIXTURES/forged_private_constructor.pgy")" \
        --native-pipeline "--backend=$backend" -o "$program_arg") \
        >"$WORK_DIR/forged-$backend.out" \
        2>"$WORK_DIR/forged-$backend.err"; then
        fail "$backend exposed the private constructor"
    fi
    [[ ! -e "$program" ]] ||
        fail "$backend published an artifact for the forged constructor"
    grep -Fq "Undefined function 'ValidatedRenderPlan'" \
        "$WORK_DIR/forged-$backend.out" "$WORK_DIR/forged-$backend.err" ||
        fail "$backend lost the private-constructor diagnostic"
done

echo "[native-private-nominal] $BACKENDS carry opaque private values and reject constructor forgery: PASS"
