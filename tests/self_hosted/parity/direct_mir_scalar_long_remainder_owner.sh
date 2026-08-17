#!/usr/bin/env bash
# Int and Long remainder consume one checked runtime ABI in C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-long-remainder"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_long_remainder"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_long_remainder.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_long_remainder_mutations.py"
VARIANTS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_long_remainder_artifact_variants.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

KIND_ID="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_case_math_expression_kind_owner.pgy"
KIND="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_owner.pgy"
READY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_case_math_expression_readiness_owner.pgy"
RUNTIME="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_case_math_runtime_requirement_owner.pgy"
PROJECTION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_case_math_runtime_projection_owner.pgy"
C_EXPR="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_case_math_expression_owner.pgy"
LLVM_EXPR="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_case_math_expression_owner.pgy"
C_MATERIAL="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_int_math_materialization_owner.pgy"
LLVM_MATERIAL="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_int_math_materialization_owner.pgy"
DIVISION_OWNER="$ROOT_DIR/src/self_hosted/codegen/runtime_abi/checked_division_runtime_owner.pgy"
PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"

grep -Fq 'DirectMirScalarProgramExprModuloLong() -> Int { return 75; }' "$KIND_ID" ||
    fail "Long remainder expression identity drifted"
grep -Fq 'kind != DirectMirScalarProgramExprModuloInt()' "$READY" ||
    fail "Int remainder did not join the checked expression owner"
grep -Fq 'source_kind == AstExpressionNodeModulo() && longs' "$KIND" ||
    fail "Long remainder source/type join is missing"
grep -Fq 'kind == DirectMirScalarProgramExprModuloLong()' "$READY" ||
    fail "Long remainder readiness is missing"
for path in "$RUNTIME" "$PROJECTION" "$C_EXPR" "$LLVM_EXPR"; do
    grep -Fq 'long_modulo' "$path" || fail "$(basename "$path") omitted long_modulo"
done
grep -Fq 'DirectMirScalarProgramExprModuloInt()' "$RUNTIME" ||
    fail "runtime requirement omitted Int remainder"
grep -Fq 'DirectMirScalarProgramExprModuloInt()' "$C_EXPR" ||
    fail "C checked remainder omitted Int"
grep -Fq 'DirectMirScalarProgramExprModuloInt()' "$LLVM_EXPR" ||
    fail "LLVM checked remainder omitted Int"
! grep -Fq 'Concat(" % "' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_expression_owner.pgy" ||
    fail "C reintroduced raw Int remainder"
! grep -Fq ' = srem i64 ' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_expression_owner.pgy" ||
    fail "LLVM reintroduced raw Int remainder"
grep -Fq 'pgy_runtime_panic_checked_inline.h' "$C_MATERIAL" ||
    fail "C omitted the checked arithmetic runtime owner"
grep -Fq 'declare i64 @' "$LLVM_MATERIAL" ||
    fail "LLVM omitted the checked arithmetic runtime declaration"
grep -Fq 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v78' "$PLAN" ||
    fail "GraphPlan schema did not advance with Long remainder"
modulo_guard="$(sed -n '/func CheckedDivisionRuntimeCModuloGuard()/,/^}/p' "$DIVISION_OWNER")"
grep -Fq 'return 0;' <<<"$modulo_guard" ||
    fail "self-host C modulo lost INT64_MIN remainder semantics"
! grep -Fq 'ARITHMETIC_OVERFLOW' <<<"$modulo_guard" ||
    fail "self-host C modulo still reports division overflow"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
[[ "$(grep -Fo '"kind":"modulo","text":"left % right"' "$MIR" | wc -l)" -eq 2 ]] ||
    fail "producer omitted the Int/Long remainder graphs"

runtime_obj="$WORK_DIR/runtime.o"
"$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" \
    >"$WORK_DIR/runtime.compile.out" 2>"$WORK_DIR/runtime.compile.err" ||
    fail "runtime ABI object did not compile"

compile_artifact() {
    local backend="$1" artifact="$2" bin="$3" label="$4"
    if [[ "$backend" == c ]]; then
        local command=("$CC" -x c -std=c11 -fwrapv "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$label.compile.out" \
            2>"$WORK_DIR/$label.compile.err" || fail "$label C compile failed"
    else
        "$CLANG" -x ir "$artifact" -x none "$runtime_obj" -pthread -lm \
            -o "$bin" >"$WORK_DIR/$label.compile.out" \
            2>"$WORK_DIR/$label.compile.err" || fail "$label LLVM compile failed"
    fi
}

printf '2\n' >"$WORK_DIR/expected-positive.run"
printf '0\n' >"$WORK_DIR/expected-minimum.run"
for backend in c llvm; do
    extension=c; [[ "$backend" == llvm ]] && extension=ll
    artifact_rel="$WORK_REL/program.$extension"
    artifact="$ROOT_DIR/$artifact_rel"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    grep -Fq 'pgy_checked_mod_i64_export' "$artifact" ||
        fail "$backend omitted the canonical Long remainder ABI"
    if [[ "$backend" == c ]]; then
        ! grep -Eq 'return .* % .*;' "$artifact" ||
            fail "C reintroduced raw signed remainder"
    else
        ! grep -Fq 'srem i64' "$artifact" ||
            fail "LLVM reintroduced raw signed remainder"
    fi
    [[ "$backend" != llvm ]] || grep -Fq 'declare i64 @pgy_checked_mod_i64_export(i64, i64)' "$artifact" ||
        fail "LLVM omitted the exact Long remainder declaration"
    for mode in positive minimum-minus-one zero-divisor \
            int-positive int-minimum-minus-one int-zero-divisor; do
        variant="$WORK_DIR/$backend-$mode.$extension"
        python "$VARIANTS" "$artifact" "$backend" "$mode" "$variant"
        variant_bin="$WORK_DIR/$backend-$mode.exe"
        compile_artifact "$backend" "$variant" "$variant_bin" "$backend-$mode"
        if [[ "$mode" == positive || "$mode" == int-positive ]]; then
            "$variant_bin" | tr -d '\r' >"$WORK_DIR/$backend-$mode.run"
            cmp -s "$WORK_DIR/expected-positive.run" "$WORK_DIR/$backend-$mode.run" ||
                fail "$backend positive remainder drifted"
        elif [[ "$mode" == minimum-minus-one || \
                "$mode" == int-minimum-minus-one ]]; then
            "$variant_bin" | tr -d '\r' >"$WORK_DIR/$backend-$mode.run"
            cmp -s "$WORK_DIR/expected-minimum.run" "$WORK_DIR/$backend-$mode.run" ||
                fail "$backend INT64_MIN % -1 drifted"
        else
            if "$variant_bin" >"$WORK_DIR/$backend-$mode.run" \
                    2>"$WORK_DIR/$backend-$mode.err"; then
                fail "$backend accepted Long remainder by zero"
            fi
            grep -Fq 'class=divide-by-zero reason=integer division or modulo by zero' \
                "$WORK_DIR/$backend-$mode.err" || fail "$backend zero panic drifted"
        fi
    done
done

for mutation in wrong-right-type wrong-expression-kind; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
                2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published $mutation"
    done
done

echo "[$LABEL] checked Int/Long remainder C/LLVM parity + MIN/-1/zero: PASS"
