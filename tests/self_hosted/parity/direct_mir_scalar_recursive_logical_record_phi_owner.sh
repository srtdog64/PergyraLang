#!/usr/bin/env bash
# Recursive logical records and Bool/record joins retain one typed owner chain.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-recursive-logical-record-phi"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_recursive_logical_record_phi"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_recursive_logical_record_phi.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"

FACT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_fact_owner.pgy"
EXPR_READY_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_expression_readiness_owner.pgy"
PHI_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_phi_operation_admission_owner.pgy"
PHI_KIND_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_expression_owner.pgy"
OP_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_op_code_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_owner.pgy"
PLAN_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$FACT_OWNER" "$EXPR_READY_OWNER" "$PHI_OWNER" "$PHI_KIND_OWNER" "$OP_OWNER" \
    "$C_OWNER" "$LLVM_OWNER" "$PLAN_OWNER" "$MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'DirectMirScalarProgramLogicalRecordCandidateDependenciesReady(' "$FACT_OWNER" ||
    fail "logical record dependency closure is missing"
grep -Fq 'signature.parameters.type_names[parameter]' "$FACT_OWNER" ||
    fail "logical record roots omit callable parameters"
grep -Fq 'fact.field_counts[row] <= 0' "$FACT_OWNER" ||
    fail "logical record fact still assumes a fixed field count"
grep -Fq 'dependency < row' "$FACT_OWNER" ||
    fail "logical record fact does not require dependency-first order"
! grep -Fq 'fact.field_counts[row] != 4' "$FACT_OWNER" ||
    fail "logical record fact reintroduced the four-field shape"
! grep -Fq 'ArrayLength(arguments) != 4' "$EXPR_READY_OWNER" ||
    fail "logical record expressions reintroduced the four-field shape"
grep -Fq 'logical_record: DirectMirScalarProgramLogicalRecordFact' "$PHI_OWNER" ||
    fail "phi admission omits the logical-record fact"
grep -Fq 'DirectMirScalarCfgOpPhiValue()' "$PHI_KIND_OWNER" ||
    fail "phi kind owner omits the target-neutral value join"
grep -Fq 'func DirectMirScalarCfgOpPhiValue() -> Int { return 29; }' "$OP_OWNER" ||
    fail "value phi operation identity drifted"
for owner in "$C_OWNER" "$LLVM_OWNER"; do
    grep -Fq 'LogicalRecordTypeName(fact, type_name)' "$owner" ||
        fail "nested logical record target type is not fact-derived"
    ! grep -Eq 'offset|offsetof' "$owner" ||
        fail "nested record target invented a physical layout"
done
grep -Fq 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v80' "$PLAN_OWNER" ||
    fail "GraphPlan schema does not seal recursive records and value phi"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
for name in LeafFact AlternateLeafFact DocumentFact; do
    grep -Fq "\"name\":\"$name\"" "$MIR" ||
        fail "producer omitted $name declaration"
done
[[ "$(grep -o '"kind":"phi"' "$MIR" | wc -l)" -ge 3 ]] ||
    fail "producer omitted Bool/record phi rows"
grep -Fq '"abi_type_name":"DocumentFact","abi_layout_id":0,"abi_layout_required":false,"abi_layout":null' "$MIR" ||
    fail "producer omitted recursive record ABI absence"

printf 'root-default\n4\ndocument\nroot-selected\nalternate-selected\n9\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
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
        [[ "$(grep -Fc '} pgy_scalar_logical_record_value_' "$artifact")" == 3 ]] ||
            fail "C artifact did not emit three dependency-ordered record types"
        grep -Eq 'pgy_scalar_logical_record_value_0 field_1; pgy_scalar_logical_record_value_1 field_2;' "$artifact" ||
            fail "C artifact omitted nested record fields"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || {
                cat "$WORK_DIR/$backend.compile.err" >&2
                fail "C artifact did not compile"
            }
    else
        grep -Fq '%pgy.scalar.logical.record.value.2 = type { i1, %pgy.scalar.logical.record.value.0, %pgy.scalar.logical.record.value.1, i64, ptr }' "$artifact" ||
            fail "LLVM artifact omitted nested record fields"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || {
                cat "$WORK_DIR/$backend.compile.err" >&2
                fail "LLVM artifact did not compile"
            }
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in logical-record-recursive-cycle \
        logical-record-nested-cross-identity \
        logical-record-value-phi-identity; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$output" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] recursive identity + value phi + C/LLVM parity/negatives: PASS"
