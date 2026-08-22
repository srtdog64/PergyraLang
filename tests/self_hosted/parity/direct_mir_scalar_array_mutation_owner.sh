#!/usr/bin/env bash
# Array mutations consume receiver, index, and value facts once in source order.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-array-mutation"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_array_mutation"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_array_mutation.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_array_mutation_mutations.py"
BOOL_MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_array_bool_push_mutations.py"
POP_MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_array_value_result_pop_mutations.py"
TARGET_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_mutation_target_owner.pgy"
STORAGE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_array_mutation_storage_owner.pgy"
READINESS_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_mutation_readiness_owner.pgy"
TYPED_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_typed_readiness_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_array_mutation_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_mutation_owner.pgy"
PLAN_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
LEAF_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_leaf_operand_owner.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
for owner in "$TARGET_OWNER" "$STORAGE_OWNER" "$READINESS_OWNER" "$TYPED_OWNER" "$C_OWNER" "$LLVM_OWNER" "$PLAN_OWNER" "$LEAF_OWNER" "$MUTATIONS" "$BOOL_MUTATIONS" "$POP_MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
grep -Fq 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v79' "$PLAN_OWNER" ||
    fail "GraphPlan schema does not carry the secondary index expression row"
grep -Fq 'operation_name != "ArrayPop"' \
    "$TARGET_OWNER" || fail "target owner does not classify one mutation family"
grep -Fq 'secondary_expression_row' "$STORAGE_OWNER" ||
    fail "operation storage omits the explicit ArraySet index row"
grep -Fq 'index_row, CompilerAbiLayoutIntTypeName(), routine' "$READINESS_OWNER" ||
    fail "ArraySet readiness does not require an Int index"
grep -Fq 'plan.operation_left_locals[row] >= 0' "$TYPED_OWNER" ||
    fail "typed readiness still classifies a formal mutation as a local target"
grep -Fq 'pgy_set_' "$C_OWNER" ||
    fail "C mutation owner does not materialize ordered index/value temporaries"
grep -Fq 'DirectMirScalarCfgCOperand(plan, -1, target_local, "")' "$C_OWNER" ||
    fail "C mutation owner does not consume the program-global local row"
if grep -Fq 'DirectMirScalarCfgCLocal(plan, target_local)' "$C_OWNER"; then
    fail "C mutation owner reinterprets a local row as a value row"
fi
grep -Fq 'DirectMirScalarProgramLlvmLocalArraySetMaterialization' "$LLVM_OWNER" ||
    fail "LLVM mutation owner omits checked local ArraySet definitions"
grep -Fq 'DirectMirScalarCfgLlvmOperandLocal(' "$LLVM_OWNER" ||
    fail "LLVM mutation owner does not consume the program-global local row"
if grep -Fq 'DirectMirScalarCfgLlvmLocal(plan, target_local)' "$LLVM_OWNER"; then
    fail "LLVM mutation owner reinterprets a local row as a value row"
fi
grep -Fq 'while consumed_use < use_row' "$LEAF_OWNER" ||
    fail "expression owner cannot reuse an already-consumed receiver ValueId"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
[[ "$(grep -o '"arg0":"ArraySet"' "$MIR" | wc -l)" -eq 5 ]] ||
    fail "producer did not emit local and value-result ArraySet rows"
[[ "$(grep -o '"arg0":"ArrayPop"' "$MIR" | wc -l)" -eq 4 ]] ||
    fail "producer did not emit local and value-result typed ArrayPop rows"
grep -Fq '"expr1":"NextIndex()"' "$MIR" ||
    fail "producer did not preserve the ArraySet index expression lane"
grep -Fq '"local_ref":"parameter:' "$MIR" ||
    fail "producer omitted the value-result mutation receiver identity"
grep -Fq '"local_ref":"parameter:22:1"' "$MIR" ||
    fail "producer did not preserve the second same-type parameter identity"
grep -Fq 'ArrayPush(reachable_values, reachable_fact.reachable)' "$MIR" ||
    fail "producer omitted the logical-record Bool push"
python - "$MIR" <<'PY'
import json, sys
document = json.load(open(sys.argv[1], encoding="utf-8"))
row = next(r for r in document["routines"] if r["name"] == "AppendSecond")
push = next(i for b in row["blocks"] for i in b["instructions"]
            if i.get("arg0") == "ArrayPush")
assert push["local_ref"] == f"parameter:{row['source_syntax_id']}:1"
PY
printf 'index\nint-value\n10\n1\n7\n2\n8\n2\n9\n8\n1\nstring-value\nb\nb!\n1\n0\n1\n0-set\n0\nlate\n' >"$WORK_DIR/expected.run"
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
        grep -Fq 'static void pgy_ai_set(' "$artifact" ||
            fail "C artifact omitted checked Array<Int> set"
        grep -Fq 'static void pgy_as_set(' "$artifact" ||
            fail "C artifact omitted checked Array<String> set"
        grep -Fq 'pgy_local_0.length = pgy_local_0.length - 1;' "$artifact" ||
            fail "C artifact omitted local Array<Int> pop"
        grep -Fq 'pgy_local_4.length = pgy_local_4.length - 1;' "$artifact" ||
            fail "C artifact omitted local Array<String> pop"
        grep -Fq 'pgy_ai_set(&pgy_param_1' "$artifact" ||
            fail "C artifact omitted Array<Int> value-result set"
        grep -Fq 'pgy_ai_push(&pgy_param_1' "$artifact" ||
            fail "C artifact omitted exact Array<Int> value-result push"
        grep -Fq 'pgy_as_set(&pgy_param_0' "$artifact" ||
            fail "C artifact omitted Array<String> value-result set"
        grep -Fq 'pgy_ab_push(&pgy_local_' "$artifact" || fail "C artifact omitted local Array<Bool> push"
        grep -Fq 'pgy_ab_set(&pgy_local_' "$artifact" || fail "C artifact omitted local Array<Bool> set"
        index_line="$(grep -n 'pgy_set_1_index =' "$artifact" | head -n1 | cut -d: -f1)"
        value_line="$(grep -n 'pgy_set_1_value =' "$artifact" | head -n1 | cut -d: -f1)"
        set_line="$(grep -n 'pgy_ai_set(&pgy_local_0' "$artifact" | head -n1 | cut -d: -f1)"
        [[ -n "$index_line" && -n "$value_line" && -n "$set_line" &&
            "$index_line" -lt "$value_line" && "$value_line" -lt "$set_line" ]] ||
            fail "C ArraySet index/value evaluation order drifted"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C artifact did not compile"
    else
        grep -Fq 'define internal void @pgy_ai_set(' "$artifact" ||
            fail "LLVM artifact omitted checked Array<Int> set"
        grep -Fq 'define internal void @pgy_as_set(' "$artifact" ||
            fail "LLVM artifact omitted checked Array<String> set"
        grep -Fq '%pgy.pop.' "$artifact" ||
            fail "LLVM artifact omitted local Array pop"
        grep -Fq 'call void @pgy_ai_set(ptr %pgy.param.1.local' "$artifact" ||
            fail "LLVM artifact omitted Array<Int> value-result set"
        grep -Fq 'call void @pgy_ai_push(ptr %pgy.param.1.local' "$artifact" ||
            fail "LLVM artifact omitted exact Array<Int> value-result push"
        grep -Fq 'call void @pgy_as_set(ptr %pgy.param.0.local' "$artifact" ||
            fail "LLVM artifact omitted Array<String> value-result set"
        grep -Fq 'call void @pgy_ab_push(ptr %pgy.local.' "$artifact" || fail "LLVM artifact omitted local Array<Bool> push"
        grep -Fq 'call void @pgy_ab_set(ptr %pgy.local.' "$artifact" || fail "LLVM artifact omitted local Array<Bool> set"
        grep -Fq 'declare void @pgy_runtime_panic_out_of_bounds_export(ptr)' \
            "$artifact" || fail "LLVM artifact omitted bounds panic ABI"
        index_line="$(grep -n 'call i64 @pgy.scalar.routine.1()' "$artifact" | head -n1 | cut -d: -f1)"
        value_line="$(grep -n 'call i64 @pgy.scalar.routine.2()' "$artifact" | head -n1 | cut -d: -f1)"
        set_line="$(grep -n 'call void @pgy_ai_set(ptr %pgy.local.0' "$artifact" | head -n1 | cut -d: -f1)"
        [[ -n "$index_line" && -n "$value_line" && -n "$set_line" &&
            "$index_line" -lt "$value_line" && "$value_line" -lt "$set_line" ]] ||
            fail "LLVM ArraySet index/value evaluation order drifted"
        runtime_obj="$WORK_DIR/runtime.o"
        "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
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
        fail "$backend runtime output or evaluation order drifted"
done
for mutation in missing-receiver wrong-receiver-type wrong-index-type \
    wrong-value-type missing-index-graph broken-index-spine \
    duplicate-consumed-use missing-pop-receiver pop-expression-graph \
    array-int-abi array-string-abi missing-parameter-receiver \
    wrong-parameter-owner wrong-parameter-ordinal \
    missing-parameter-push-receiver wrong-parameter-push-owner \
    wrong-parameter-push-ordinal wrong-parameter-push-value-type \
    missing-string-parameter-receiver wrong-string-parameter-owner \
    wrong-string-parameter-ordinal wrong-string-parameter-value-type \
    int-pop-missing-receiver int-pop-wrong-owner int-pop-wrong-ordinal int-pop-value-carriage \
    string-pop-missing-receiver string-pop-wrong-owner string-pop-wrong-ordinal string-pop-value-carriage \
    missing-bool-push-receiver wrong-bool-push-receiver-type wrong-bool-push-value-type \
    missing-bool-set-receiver wrong-bool-set-receiver-type wrong-bool-set-value-type; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    mutation_owner="$MUTATIONS"
    [[ "$mutation" == *-bool-* ]] && mutation_owner="$BOOL_MUTATIONS"
    [[ "$mutation" == int-pop-* || "$mutation" == string-pop-* ]] && mutation_owner="$POP_MUTATIONS"
    python "$mutation_owner" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
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
echo "[$LABEL] local/value-result Push/Set/Pop parity and negatives: PASS"
