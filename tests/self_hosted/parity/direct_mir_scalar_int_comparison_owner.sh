#!/usr/bin/env bash
# One typed Int comparison and wrap-semantics fact family consumed by C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-int-comparison"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_int_comparison"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_int_comparison.pgy"
MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_comparison_expression_kind_owner.pgy"
KIND_ID="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_id_owner.pgy"
KIND="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_owner.pgy"
PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
COMPARISON_READY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_comparison_expression_readiness_owner.pgy"
BOOL_READY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_bool_readiness_owner.pgy"
NON_TRAPPING="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_non_trapping_comparison_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_expression_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_expression_owner.pgy"
LLVM_MATH="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_int_math_materialization_owner.pgy"
LLVM_WRAP="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_int_wrap_expression_owner.pgy"
INT_DOMAIN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_int_literal_domain_owner.pgy"
INT_FORMAT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_int_format_projection_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_int_comparison_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'DirectMirScalarProgramComparisonExpressionKindFact(' "$KIND" || fail "classification consumer bypassed the owner"
! grep -Fq 'source_kind == AstExpressionNodeEquality() && bools' "$KIND" || fail "Bool equality remained outside the typed comparison owner"
grep -Fq 'DirectMirScalarProgramExprNotEqualBool() -> Int { return 89; }' "$KIND_ID" || fail "Bool inequality identity is not append-only 89"
grep -Fq 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v80' "$PLAN" || fail "Bool inequality did not advance the GraphPlan schema"
for term in AstExpressionNodeEquality AstExpressionNodeInequality AstExpressionNodeLess AstExpressionNodeGreaterEqual AstExpressionNodeGreater AstExpressionNodeLessEqual; do
    grep -Fq "$term()" "$OWNER" || fail "comparison owner omitted $term"
done
for term in DirectMirScalarProgramExprNotEqualInt DirectMirScalarProgramExprGreaterEqualInt DirectMirScalarProgramExprGreaterInt DirectMirScalarProgramExprLessEqualInt DirectMirScalarProgramExprEqualBool DirectMirScalarProgramExprNotEqualBool; do
    grep -Fq "$term()" "$COMPARISON_READY" || fail "comparison readiness omitted $term"
done
for term in DirectMirScalarProgramExprNotEqualInt DirectMirScalarProgramExprGreaterEqualInt DirectMirScalarProgramExprGreaterInt DirectMirScalarProgramExprLessEqualInt; do
    grep -Fq "$term()" "$NON_TRAPPING" || fail "non-trapping comparison owner omitted $term"
done
for term in DirectMirScalarProgramExprEqualBool DirectMirScalarProgramExprNotEqualBool; do
    grep -Fq "$term()" "$BOOL_READY" || fail "nested Bool readiness omitted $term"
done
grep -Fq 'DirectMirScalarProgramNonTrappingComparisonKind(kind)' "$BOOL_READY" &&
    grep -Fq 'DirectMirScalarProgramNonTrappingComparisonNode(facts, node)' "$BOOL_READY" ||
    fail "nested Bool readiness bypasses the comparison owner"
grep -Fq '" != "' "$C_OWNER" || fail "C inequality projection is missing"
grep -Fq '" >= "' "$C_OWNER" || fail "C greater-equal projection is missing"
grep -Fq '" > "' "$C_OWNER" || fail "C greater projection is missing"
grep -Fq '" <= "' "$C_OWNER" || fail "C less-equal projection is missing"
grep -Fq '" = icmp ne i64 "' "$LLVM_OWNER" || fail "LLVM inequality projection is missing"
grep -Fq '" = icmp ne i1 "' "$LLVM_OWNER" || fail "LLVM Bool inequality projection is missing"
grep -Fq '" = icmp sge i64 "' "$LLVM_OWNER" || fail "LLVM greater-equal projection is missing"
grep -Fq '" = icmp sgt i64 "' "$LLVM_OWNER" || fail "LLVM greater projection is missing"
grep -Fq '" = icmp sle i64 "' "$LLVM_OWNER" || fail "LLVM less-equal projection is missing"
for term in 'opcode = "add"' 'opcode = "sub"' 'trunc i64 ' ' to i32' 'sext i32 '; do
    grep -Fq "$term" "$LLVM_WRAP" || fail "LLVM Int wrap owner omitted $term"
done
grep -Fq '%negated = sub i32 0, %narrow' "$LLVM_MATH" || fail "LLVM wrap Abs is missing"
grep -Fq '2147483647' "$INT_DOMAIN" || fail "Int positive bound is not owner-checked"
grep -Fq '2147483648' "$INT_DOMAIN" || fail "Int negative bound is not owner-checked"
grep -Fq 'StringRuntimeCInt32LineFormat()' "$INT_FORMAT" || fail "C Int format is not 32-bit"
grep -Fq 'StringRuntimeCIntLineFormat()' "$INT_FORMAT" || fail "LLVM widened Int format is missing"
! grep -Fq 'nsw' "$LLVM_OWNER" || fail "LLVM expression reintroduced poison-on-overflow"
! grep -Fq 'nsw' "$LLVM_MATH" || fail "LLVM Abs reintroduced poison-on-overflow"

mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") \
    >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"kind":"inequality"' "$MIR" || fail "producer omitted inequality"
grep -Fq '"name":"BoolComparisonCode"' "$MIR" || fail "producer omitted Bool comparison routine"
grep -Fq '"kind":"greater_equal"' "$MIR" || fail "producer omitted greater-equal"
grep -Fq '"kind":"greater"' "$MIR" || fail "producer omitted greater"
grep -Fq '"kind":"less_equal"' "$MIR" || fail "producer omitted less-equal"
printf '1\n1\n-2147483648\n2147483647\n-2147483648\n-2147483648\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"; artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$MIR_REL" -o "$artifact_rel") \
        >"$WORK_DIR/$backend.project.out" 2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq 'printf("%d\n"' "$artifact" || fail "C artifact did not select the Int32 format"
        grep -Fq ' != ' "$artifact" || fail "C artifact omitted inequality"
        grep -Fq ' >= ' "$artifact" || fail "C artifact omitted greater-equal"
        grep -Fq ' > ' "$artifact" || fail "C artifact omitted greater"
        grep -Fq ' <= ' "$artifact" || fail "C artifact omitted less-equal"
        command=("$CC" -x c -std=c11 -fwrapv "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread); fi
        command+=(-lm -o "$bin"); "${command[@]}" >/dev/null 2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Fq 'c"%lld\0A\00"' "$artifact" || fail "LLVM widened carrier format is missing"
        grep -Fq 'icmp ne i64' "$artifact" || fail "LLVM artifact omitted inequality"
        grep -Fq 'icmp ne i1' "$artifact" || fail "LLVM artifact omitted Bool inequality"
        grep -Fq 'icmp sge i64' "$artifact" || fail "LLVM artifact omitted greater-equal"
        grep -Fq 'icmp sgt i64' "$artifact" || fail "LLVM artifact omitted greater"
        grep -Fq 'icmp sle i64' "$artifact" || fail "LLVM artifact omitted less-equal"
        grep -Fq '.wide = add i64' "$artifact" || fail "LLVM artifact omitted widened Int addition"
        grep -Fq '.narrow = trunc i64' "$artifact" || fail "LLVM artifact omitted Int narrowing"
        grep -Fq ' = sext i32 ' "$artifact" || fail "LLVM artifact omitted Int sign normalization"
        ! grep -Fq 'add nsw i64' "$artifact" || fail "LLVM expression marked wrap addition nsw"
        ! grep -Fq 'sub nsw i32' "$artifact" || fail "LLVM Abs marked wrap subtraction nsw"
        "$CLANG" -x ir "$artifact" -o "$bin" >/dev/null 2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" || fail "$backend runtime output drifted"
done

for mutation in mixed-type bool-mixed-type out-of-range-literal; do
    invalid_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$invalid_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$invalid_rel" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" 2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation comparison/counter facts"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation facts"
    done
done

echo "[$LABEL] typed comparison/wrap C/LLVM parity + mixed-type negatives: PASS"
