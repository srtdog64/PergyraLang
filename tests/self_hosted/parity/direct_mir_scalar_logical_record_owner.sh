#!/usr/bin/env bash
# Multiple declaration-owned logical records retain distinct target identities.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-logical-record"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_logical_record"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_multi_logical_record.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"
FACT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_fact_owner.pgy"
INDEX_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/program_routine_index_owner.pgy"
EXPR_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_expression_owner.pgy"
TARGET_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_target_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_owner.pgy"
BOOL_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_bool_readiness_owner.pgy"
ROUTE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_route_fact_owner.pgy"
PLAN_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$FACT_OWNER" "$INDEX_OWNER" "$EXPR_OWNER" "$TARGET_OWNER" "$C_OWNER" \
    "$LLVM_OWNER" "$BOOL_OWNER" "$ROUTE_OWNER" "$PLAN_OWNER" "$MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'declarations.field_identities' "$FACT_OWNER" ||
    fail "logical record does not consume declaration field identity"
grep -Fq 'declarations.source_module_paths[declaration_row]' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_declaration_envelope_owner.pgy" ||
    fail "logical record does not cross-seal declaration provenance"
grep -Fq 'admitted.routines.instruction_abi_type_names' "$FACT_OWNER" ||
    fail "local-only record discovery does not consume admitted instruction ABI identity"
grep -Fq 'let instruction_abi_type_names: Array<String>;' "$INDEX_OWNER" ||
    fail "program routine index omits instruction ABI type identity"
grep -Fq 'DirectMirInstructionHasNoPhysicalAbi(' "$FACT_OWNER" ||
    fail "logical record does not cross-seal instruction ABI absence"
grep -Fq 'DirectMirScalarProgramLogicalRecordFieldOrdinal(' "$EXPR_OWNER" ||
    fail "member projection does not consume ordered declaration fields"
constructor_body="$(awk '/^func DirectMirScalarProgramLogicalRecordConstructorFromGraph\(/,/^}/' "$EXPR_OWNER")"
[[ "$constructor_body" == *'record_row < 0'* ]] ||
    fail "logical-record constructor does not consume declaration identity"
[[ "$constructor_body" != *'expected_type'* ]] ||
    fail "nested constructor is narrowed by the enclosing expected type"
! grep -Fq 'while field < ArrayLength(record.field_names)' "$EXPR_OWNER" ||
    fail "member marker reintroduced program-global field spelling lookup"
grep -Fq 'type_name: String, field_name: String' "$FACT_OWNER" ||
    fail "member identity is not scoped by record type"
grep -Fq 'kind == DirectMirScalarProgramExprLogicalRecordMember()' "$BOOL_OWNER" ||
    fail "logical Bool member is not admitted by the non-trapping proof"
grep -Fq 'logical_record: DirectMirScalarProgramLogicalRecordFact' "$ROUTE_OWNER" ||
    fail "route does not carry the logical record fact"
grep -Fq 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v80' "$PLAN_OWNER" ||
    fail "GraphPlan schema does not seal logical record identity"
for owner in "$C_OWNER" "$LLVM_OWNER"; do
    grep -Fq 'DirectMirScalarProgramLogicalRecordTargetFromFact(' "$owner" ||
        fail "target emission bypasses the logical record projection"
    ! grep -Eq 'offset|offsetof' "$owner" ||
        fail "target emission invented a physical record offset"
done
! grep -Fq 'ProbeFact' "$FACT_OWNER" || fail "logical record owner is fixture-keyed"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
for name in ProbeFact UnusedFact LocalTableFact LocalDocumentFact ObjectTableFact ArrayObjectTableFact; do
    grep -Fq "\"name\":\"$name\"" "$MIR" ||
        fail "producer omitted $name declaration"
done
grep -Fq '"name":"EmptyObjectTable"' "$MIR" || fail "producer omitted zero-parameter record callable"
grep -Fq '"abi_type_name":"ProbeFact","abi_layout_id":0,"abi_layout_required":false,"abi_layout":null' "$MIR" ||
    fail "producer omitted the logical-record ABI-absence receipt"
for name in ObjectTableFact ArrayObjectTableFact; do
    grep -Fq "\"abi_type_name\":\"$name\",\"abi_layout_id\":0,\"abi_layout_required\":false,\"abi_layout\":null" "$MIR" ||
        fail "producer omitted $name ABI-absence receipt"
done
grep -Fq '"type":"ObjectTableFact","carriage":"readonly-ref","resource":"none","pass":"indirect"' "$MIR" ||
    fail "producer omitted readonly logical-record carriage"
grep -Fq 'LocalDocumentFact(LocalTableFact(true, json, 2, 8), 0)' "$MIR" ||
    fail "producer omitted the nested logical-record constructor graph"
grep -Fq 'LocalTableReady(document.declarations)' "$ROOT_DIR/$SOURCE_REL" ||
    fail "fixture omits readonly-ref logical-record member direct call"
grep -Fq 'ObjectTableEnd(MakeObjectTable(json, 5))' "$ROOT_DIR/$SOURCE_REL" ||
    fail "fixture omits nested logical-record value direct call"
grep -Fq 'DirectMirScalarProgramExprDirectCall()' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_direct_call_readiness_owner.pgy" ||
    fail "direct-call readiness omits nested value results"
grep -Fq 'DirectMirScalarProgramExprLogicalRecordConstructor()' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_direct_call_readiness_owner.pgy" ||
    fail "direct-call readiness omits logical-record constructor values"
grep -Fq 'ObjectTableEnd(ObjectTableFact(true, json, 5, 8))' \
    "$ROOT_DIR/$SOURCE_REL" ||
    fail "fixture omits direct logical-record constructor value call"
printf 'ok\n3\nobject\n7\n8\n9\nlocal\n9\n8\nmember-ref\n' >"$WORK_DIR/expected.run"
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
        [[ "$(grep -Fc '} pgy_scalar_logical_record_value_' "$artifact")" == 5 ]] ||
            fail "C artifact did not emit five distinct logical record types"
        for row in 0 1 2 3 4; do
            grep -Fq "pgy_scalar_logical_record_value_$row" "$artifact" ||
                fail "C artifact omitted logical record type $row"
        done
        grep -Fq ').field_3' "$artifact" ||
            fail "C artifact omitted the String member projection"
        grep -Eq 'pgy_scalar_logical_record_value_[0-9]+ pgy_scalar_routine_[0-9]+\(void\)' "$artifact" ||
            fail "C artifact omitted the zero-parameter logical-record signature"
        grep -Eq 'const pgy_scalar_logical_record_value_[0-9]+ \*pgy_param_0' "$artifact" ||
            fail "C artifact omitted the readonly logical-record parameter"
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
        [[ "$(grep -Ec '^%pgy\.scalar\.logical\.record\.value\.[0-4] = type' "$artifact")" == 5 ]] ||
            fail "LLVM artifact did not emit five distinct logical record types"
        grep -Fq 'extractvalue %pgy.scalar.logical.record.value.' "$artifact" ||
            fail "LLVM artifact omitted member projection"
        grep -Eq 'define internal %pgy\.scalar\.logical\.record\.value\.[0-9]+ @pgy\.scalar\.routine\.[0-9]+\(\)' "$artifact" ||
            fail "LLVM artifact omitted the zero-parameter logical-record signature"
        grep -Eq 'define internal i1 @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0\)' "$artifact" ||
            fail "LLVM artifact omitted the readonly logical-record parameter"
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

for mutation in logical-record-field-order logical-record-instruction-layout \
    logical-record-cross-identity logical-record-local-declaration-identity \
        logical-record-readonly-carriage; do
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
echo "[$LABEL] multi-record identity + ABI absence + C/LLVM parity/negatives: PASS"
