#!/usr/bin/env bash
# Declaration-keyed record-Array by-value parameter, indexed read, and record return parity.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-logical-record-array-value-parameter"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_logical_record_array_value_parameter"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_logical_record_array_value_parameter.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_value_parameter_policy_owner.pgy"
INDEX="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_index_expression_owner.pgy"
C_VALUE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_logical_value_expression_owner.pgy"
LLVM_VALUE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_value_expression_owner.pgy"
PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
DIRECT_CALL_READINESS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_direct_call_readiness_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_logical_record_array_value_parameter_mutations.py"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'return record_array_count == 1;' "$POLICY" ||
    fail "value-parameter policy does not pin one record Array"
grep -Fq 'DirectMirScalarProgramLogicalRecordTypeReady(' "$POLICY" ||
    fail "value-parameter policy does not join the record return owner"
grep -Fq 'DirectMirScalarProgramExprLogicalRecordArrayIndex()' "$INDEX" ||
    fail "indexed record-array expression has no stable identity"
grep -Fq 'DirectMirScalarProgramExprLogicalRecordArrayIndex()' \
    "$DIRECT_CALL_READINESS" ||
    fail "direct-call readiness omits logical-record Array index values"
grep -Fq 'IndexedRowIndent(rows[0])' "$ROOT_DIR/$SOURCE_REL" ||
    fail "fixture omits direct logical-record Array index value call"
grep -Fq 'DirectMirScalarProgramLogicalRecordArrayTargetFromType(' "$C_VALUE" ||
    fail "C indexed read bypasses the record-array target owner"
grep -Fq 'DirectMirScalarProgramLogicalRecordArrayTargetFromType(' "$LLVM_VALUE" ||
    fail "LLVM indexed read bypasses the record-array target owner"
grep -Fq 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v78' "$PLAN" ||
    fail "GraphPlan schema did not advance for the new expression identity"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"IndexedRow"' "$MIR" ||
    fail "producer omitted the record declaration"
grep -Fq '"type":"Array<IndexedRow>","carriage":"value"' "$MIR" ||
    fail "producer omitted the by-value record-array identity"
grep -Fq '"abi_type_name":"Array<IndexedRow>","abi_layout_id":0,"abi_layout_required":false' "$MIR" ||
    fail "producer attached a public physical ABI to the compiler-owned array"
grep -Fq '"kind":"index","text":"rows[0]"' "$MIR" ||
    fail "producer omitted the indexed record-array expression"
grep -Fq '"expr0":"IndexedRowIndent(rows[0])"' "$MIR" ||
    fail "producer omitted the indexed record-array direct-call argument"
grep -Fq '"name":"ProjectArena"' "$MIR" ||
    fail "producer omitted the record-returning projection"
grep -Fq '"return":"IndexedArena"' "$MIR" ||
    fail "producer omitted the declaration-keyed record return"
grep -Fq '"expr0":"IndexedArena(indents, labels)"' "$MIR" ||
    fail "producer omitted the record constructor"
grep -Fq '"name":"ProjectionReady"' "$MIR" ||
    fail "producer omitted the Bool-returning record-Array callable"
grep -Fq '"return":"Bool"' "$MIR" ||
    fail "producer omitted the exact Bool return identity"
grep -Fq '"expr0":"(ArrayLength(rows) != count)"' "$MIR" ||
    fail "producer omitted the nominal record-Array length expression"
printf 'logical-record-array-value-parameter-ready\n' >"$WORK_DIR/expected.run"

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
        grep -Eq 'pgy_IndexedRow_array pgy_param_0, long long pgy_param_1' "$artifact" ||
            fail "C signature omitted the by-value record Array"
        grep -Eq '\(pgy_param_0\)\.data\[[^]]+\]' "$artifact" ||
            fail "C artifact omitted the record-array indexed load"
        grep -Fq ').field_0' "$artifact" ||
            fail "C artifact omitted the indexed record member read"
        grep -Eq 'static pgy_scalar_logical_record_value_[0-9]+ pgy_scalar_routine_[0-9]+\(pgy_IndexedRow_array pgy_param_0, long long pgy_param_1\)' "$artifact" ||
            fail "C artifact omitted the record-returning record-Array signature"
        grep -Eq 'static bool pgy_scalar_routine_[0-9]+\(pgy_IndexedRow_array pgy_param_0, long long pgy_param_1\)' "$artifact" ||
            fail "C artifact omitted the Bool-returning record-Array signature"
        grep -Fq '((long long)(pgy_param_0).len)' "$artifact" ||
            fail "C artifact omitted the record-Array length projection"
        grep -Eq 'return \(pgy_scalar_logical_record_value_[0-9]+\)\{ \.field_0 = pgy_local_[0-9]+, \.field_1 = pgy_local_[0-9]+ \};' "$artifact" ||
            fail "C artifact omitted the declaration-keyed record constructor return"
        grep -Eq 'pgy_ai_push\(&pgy_local_[0-9]+, \(long long\)\(' "$artifact" ||
            fail "C artifact omitted the local ArrayInt push"
        grep -Eq 'pgy_as_push\(&pgy_local_[0-9]+, ' "$artifact" ||
            fail "C artifact omitted the local ArrayString push"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/c.compile.out" \
            2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Eq 'define internal %pgy\.array\.int @pgy\.scalar\.routine\.[0-9]+\(%pgy\.scalar\.logical\.record\.array\.[0-9]+ %pgy\.param\.0, i64 %pgy\.param\.1\)' "$artifact" ||
            fail "LLVM signature omitted the by-value record Array"
        grep -Eq 'extractvalue %pgy\.scalar\.logical\.record\.array\.[0-9]+ %pgy\.param\.0, 0' "$artifact" ||
            fail "LLVM artifact omitted record-array data projection"
        grep -Eq 'getelementptr inbounds %pgy\.scalar\.logical\.record\.value\.[0-9]+, ptr ' "$artifact" ||
            fail "LLVM artifact omitted the declaration-keyed indexed slot"
        grep -Eq 'extractvalue %pgy\.scalar\.logical\.record\.value\.[0-9]+ .*?, 0' "$artifact" ||
            fail "LLVM artifact omitted the indexed record member read"
        grep -Eq 'define internal %pgy\.scalar\.logical\.record\.value\.[0-9]+ @pgy\.scalar\.routine\.[0-9]+\(%pgy\.scalar\.logical\.record\.array\.[0-9]+ %pgy\.param\.0, i64 %pgy\.param\.1\)' "$artifact" ||
            fail "LLVM artifact omitted the record-returning record-Array signature"
        grep -Eq 'define internal i1 @pgy\.scalar\.routine\.[0-9]+\(%pgy\.scalar\.logical\.record\.array\.[0-9]+ %pgy\.param\.0, i64 %pgy\.param\.1\)' "$artifact" ||
            fail "LLVM artifact omitted the Bool-returning record-Array signature"
        grep -Eq 'extractvalue %pgy\.scalar\.logical\.record\.array\.[0-9]+ %pgy\.param\.0, 1' "$artifact" ||
            fail "LLVM artifact omitted the record-Array length projection"
        grep -Eq 'ret %pgy\.scalar\.logical\.record\.value\.[0-9]+ %pgy\.expr\.[0-9]+\.[0-9]+' "$artifact" ||
            fail "LLVM artifact omitted the declaration-keyed record return"
        grep -Eq 'call void @pgy_ai_push\(ptr %pgy\.local\.[0-9]+, i64 ' "$artifact" ||
            fail "LLVM artifact omitted the local ArrayInt push"
        grep -Eq 'call void @pgy_as_push\(ptr %pgy\.local\.[0-9]+, ptr ' "$artifact" ||
            fail "LLVM artifact omitted the local ArrayString push"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in record-array-value-carriage record-array-missing-element \
    record-array-length-missing-element \
    record-array-physical-abi record-array-missing-member \
    record-return-missing-declaration record-return-field-type \
    record-array-bool-return-type; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published an artifact for $mutation"
    done
done

echo "[$LABEL] record-array value parameter/index/member/record-return C/LLVM parity + negatives: PASS"
