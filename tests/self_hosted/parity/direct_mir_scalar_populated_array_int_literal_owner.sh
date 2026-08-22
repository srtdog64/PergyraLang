#!/usr/bin/env bash
# Ordered literals, zero-calls, and exact value parameters populate Array<Int>.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-populated-array-int-literal"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_populated_array_int_literal"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_populated_array_int_literal_return.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_populated_array_int_literal_mutations.py"
PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_int_populated_literal_admission_owner.pgy"
OPERAND_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_int_populated_literal_operand_admission_owner.pgy"
READINESS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_int_populated_literal_readiness_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_array_int_populated_literal_materialization_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_int_populated_literal_expression_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
for owner in "$PLAN" "$ADMISSION" "$OPERAND_OWNER" "$READINESS" "$C_OWNER" "$LLVM_OWNER" "$MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
grep -Fq 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v79' "$PLAN" ||
    fail "GraphPlan schema does not carry the populated Array<Int> kind"
grep -Fq 'AstExpressionNodeIntegerLiteral()' "$OPERAND_OWNER" ||
    fail "Array<Int> literal does not admit canonical Int literals"
grep -Fq 'DirectMirScalarProgramZeroArgumentDirectCallFactFromGraph(' "$OPERAND_OWNER" ||
    fail "Array<Int> literal does not consume zero-call identity"
grep -Fq 'DirectMirScalarCfgWireExprRefAt(' "$OPERAND_OWNER" ||
    fail "Array<Int> literal does not consume parameter LocalRef"
grep -Fq 'DirectMirScalarCfgLeafOperandFromOwners(' "$OPERAND_OWNER" ||
    fail "Array<Int> literal does not consume local SSA use identity"
grep -Fq 'DirectMirScalarProgramExpressionKindFactFromSource(' "$OPERAND_OWNER" ||
    fail "Array<Int> expression operand bypasses common classification"
grep -Fq 'DirectMirScalarProgramExprSubtractInt()' "$OPERAND_OWNER" ||
    fail "Array<Int> literal omits exact Int subtraction operands"
grep -Fq 'DirectMirScalarProgramNestedArrayIntLiteralOperandRows(' "$ADMISSION" ||
    fail "nested Array<Int> literal bypasses the populated literal owner"
grep -Fq 'normalized_types[operand] != CompilerAbiLayoutIntTypeName()' "$ADMISSION" ||
    fail "nested Array<Int> literal omits normalized operand type identity"
grep -Fq 'DirectMirRoutineValueParameterOrdinalAtName(' "$OPERAND_OWNER" ||
    fail "Array<Int> literal does not consume exact parameter ordinal"
grep -Fq 'DirectMirScalarProgramExprIntLiteral()' "$READINESS" ||
    fail "Array<Int> literal readiness omits Int literal operands"
grep -Fq 'DirectMirScalarProgramExprDirectCall()' "$READINESS" ||
    fail "Array<Int> literal readiness omits zero-call operands"
grep -Fq 'DirectMirScalarProgramExprParameter()' "$READINESS" ||
    fail "Array<Int> literal readiness omits parameter operands"
grep -Fq 'DirectMirScalarProgramExprSubtractInt()' "$READINESS" ||
    fail "Array<Int> literal readiness omits subtraction operands"
grep -Fq 'DirectMirScalarProgramExprIntLiteral()' "$C_OWNER" ||
    fail "C literal owner does not consume normalized literal operands"
grep -Fq 'ArrayLength(values) < 1' "$LLVM_OWNER" ||
    fail "LLVM literal owner does not accept the one-or-more operand contract"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"name":"Tags"' "$MIR" || fail "producer omitted Tags"
grep -Fq '"name":"Mixed"' "$MIR" || fail "producer omitted Mixed"
grep -Fq '"name":"TagPair"' "$MIR" || fail "producer omitted TagPair"
grep -Fq 'TagPair([FirstTag(), 4, 5], [SecondTag()], [local])' "$MIR" ||
    fail "producer omitted nested zero-call/local Array<Int> fields"
grep -Fq '"expr0":"[1, 0]"' "$MIR" ||
    fail "producer omitted the literal Array<Int> canary"
grep -Fq '"expr0":"[selected]"' "$MIR" ||
    fail "producer omitted the parameter Array<Int> canary"
grep -Fq '"expr0":"[root_id]"' "$MIR" ||
    fail "producer omitted the local SSA Array<Int> canary"
grep -Fq '"expr0":"[(0 - 1), (5 - 2), (9 - 4), (2 - 8)]"' "$MIR" ||
    fail "producer omitted the computed Array<Int> canary"
[[ "$(grep -o '"call_target_name":"[A-Za-z]*Tag"' "$MIR" | wc -l)" -ge 4 ]] ||
    fail "producer omitted ordered tag calls"
printf 'first\nsecond\nthird\n0\n2\n1\n0\n-1\n3\n5\n-6\n9\nfirst\nsecond\n1\n4\n5\n2\n3\n7\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    extension="$backend"
    [[ "$backend" == llvm ]] && extension="ll"
    artifact_rel="$WORK_REL/program.$extension"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" \
                "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq 'static pgy_ai pgy_array_int_populated_literal_' "$artifact" ||
            fail "C artifact omitted the populated Array<Int> owner"
        grep -Fq 'out.data[0] = (int32_t)1;' "$artifact" ||
            fail "C artifact omitted the first literal element store"
        grep -Eq 'out\.data\[[0-9]+\] = \(int32_t\)pgy_scalar_routine_[0-9]+\(\);' "$artifact" ||
            fail "C artifact omitted an ordered zero-call element store"
        grep -Fq '(const int64_t *pgy_values)' "$artifact" ||
            fail "C artifact omitted parameter literal carriage"
        grep -Fq '(int64_t[]){pgy_param_1}' "$artifact" ||
            fail "C artifact did not select the exact parameter ordinal"
        grep -Fq '(0LL - 1LL)' "$artifact" ||
            fail "C artifact omitted computed Int literal elements"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C artifact did not compile"
    else
        grep -Fq 'call ptr @malloc(i64 8)' "$artifact" ||
            fail "LLVM artifact omitted exact two-element Array<Int> storage"
        grep -Fq 'call ptr @malloc(i64 12)' "$artifact" ||
            fail "LLVM artifact omitted exact three-element Array<Int> storage"
        [[ "$(grep -c ' = trunc i64 .* to i32' "$artifact")" -ge 7 ]] ||
            fail "LLVM artifact omitted ordered Int element projections"
        grep -Fq ' = trunc i64 %pgy.param.1 to i32' "$artifact" ||
            fail "LLVM artifact did not select the exact parameter ordinal"
        [[ "$(grep -c ' = sub i64 ' "$artifact")" -ge 4 ]] ||
            fail "LLVM artifact omitted computed Int literal elements"
        grep -Fq 'declare void @pgy_runtime_panic_out_of_bounds_export(ptr)' \
            "$artifact" || fail "LLVM artifact omitted bounds panic ABI"
        runtime_obj="$WORK_DIR/runtime.o"
        "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" \
            -I"$ROOT_DIR/src/runtime" \
            -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" \
            >"$WORK_DIR/runtime.compile.out" \
            2>"$WORK_DIR/runtime.compile.err" ||
            fail "runtime ABI object did not compile"
        "$CLANG" -x ir "$artifact" -x none "$runtime_obj" -pthread -lm \
            -o "$bin" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output or operand order drifted"
done

for mutation in missing-target wrong-target-type wrong-literal-type \
    broken-literal-spine wrong-parameter-owner wrong-parameter-ordinal \
    wrong-parameter-type wrong-expression-element-type \
    wrong-expression-element-kind malformed-expression-element-edge \
    nested-missing-target nested-nonzero-parameter nested-wrong-return \
    nested-broken-spine nested-multi-int-spine nested-local-missing-use \
    local-missing-use array-int-abi; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$mutated_rel" -o "$output_rel") \
                >"$WORK_DIR/$mutation.$backend.out" \
                2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published an artifact for $mutation"
    done
done

echo "[$LABEL] populated Array<Int> literal/parameter/expression C/LLVM parity: PASS"
