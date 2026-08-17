#!/usr/bin/env bash
# Collection-bearing logical records join declaration identity and array ABIs.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-logical-record-collection-fields"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_logical_record_collection_fields"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_logical_record_collection_fields.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"
INDEX_MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_logical_record_collection_index_mutations.py"

FACT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_fact_owner.pgy"
JOIN_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_collection_abi_owner.pgy"
INT_FACT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_int_value_result_fact_owner.pgy"
INT_LITERAL_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_int_empty_literal_admission_owner.pgy"
TYPED_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_typed_readiness_owner.pgy"
LEAF_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_leaf_operand_owner.pgy"
EXPRESSION_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_admission_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$FACT_OWNER" "$JOIN_OWNER" "$INT_FACT_OWNER" \
        "$INT_LITERAL_OWNER" "$TYPED_OWNER" "$LEAF_OWNER" \
        "$EXPRESSION_OWNER" "$C_OWNER" "$LLVM_OWNER" \
        "$MUTATIONS" "$INDEX_MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'DirectMirScalarProgramLogicalRecordTerminalFieldTypeReady(' "$FACT_OWNER" ||
    fail "logical record does not own terminal collection field identity"
for type_name in ArrayInt ArrayString; do
    grep -Fq "CompilerAbiLayout${type_name}TypeName()" "$FACT_OWNER" ||
        fail "logical record omits $type_name field identity"
done
grep -Fq 'DirectMirScalarProgramLogicalRecordCollectionAbiReady(' "$JOIN_OWNER" ||
    fail "logical record collection ABI join is missing"
grep -Fq 'MirCapturedRequiredAbiLayoutRowAdmission(' "$INT_FACT_OWNER" ||
    fail "Array<Int> local instruction ABI is not captured"
grep -Fq 'DirectMirArrayLiteralEmptyReady(sequence)' "$INT_LITERAL_OWNER" ||
    fail "Array<Int> empty literal admission is not exact"
! grep -Fq 'DirectMirArrayIntLiteralElements(' "$INT_LITERAL_OWNER" ||
    fail "bounded empty Array<Int> admission widened to populated storage"
grep -Fq 'plan.program.array_int_value_result.present' "$TYPED_OWNER" ||
    fail "typed plan accepts Array<Int> without a present ABI receipt"
grep -Fq 'DirectMirScalarProgramArrayIntValueResultFactReady(' "$TYPED_OWNER" ||
    fail "typed plan does not join the admitted Array<Int> ABI fact"
grep -Fq 'wire_expr0_carriage: Bool' "$LEAF_OWNER" ||
    fail "leaf operand owner does not name the expr0-only carriage boundary"
grep -Fq 'if wire_expr0_carriage {' "$LEAF_OWNER" ||
    fail "leaf operand owner reads expr0 carriage for every graph lane"
grep -Fq 'graph_field == "expr0_graph"' "$EXPRESSION_OWNER" ||
    fail "program expression admission does not preserve LocalRef graph lane identity"
for owner in "$C_OWNER" "$LLVM_OWNER"; do
    ! grep -Eq 'offset|offsetof' "$owner" ||
        fail "logical record target invented collection field offsets"
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
for type_name in 'Array<Int>' 'Array<String>'; do
    grep -Fq "\"abi_type_name\":\"$type_name\"" "$MIR" ||
        fail "producer omitted $type_name instruction ABI"
done
grep -Fq '"return":"CollectionIndex"' "$MIR" ||
    fail "producer omitted collection-record return identity"
grep -Fq '"uses":["selected.1","target_index.1","value_index.1"]' "$MIR" ||
    fail "producer omitted the receiver/index/value local-use ordering falsifier"
printf '7\n1\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq 'bool field_0; pgy_as field_1; pgy_as field_2; pgy_as field_3; pgy_ai field_4;' "$artifact" ||
            fail "C artifact omitted ordered collection fields"
        grep -Fq 'pgy_ai pgy_local_' "$artifact" ||
            fail "C artifact omitted Array<Int> local storage"
        grep -Fq ').field_0' "$artifact" ||
            fail "C artifact omitted collection-record member identity"
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
        grep -Fq '%pgy.scalar.logical.record.value.0 = type { i1, %pgy.array.string, %pgy.array.string, %pgy.array.string, %pgy.array.int, %pgy.array.string, %pgy.array.string, %pgy.array.string, %pgy.array.string }' "$artifact" ||
            fail "LLVM artifact omitted ordered collection fields"
        grep -Fq 'alloca %pgy.array.int' "$artifact" ||
            fail "LLVM artifact omitted Array<Int> local storage"
        grep -Fq 'extractvalue %pgy.scalar.logical.record.value.0' "$artifact" ||
            fail "LLVM artifact omitted collection-record member identity"
        runtime_object="$WORK_DIR/pgy-runtime-lib.o"
        if [[ ! -f "$runtime_object" ]]; then
            "$CLANG" -std=c11 -DPGY_LLVM_ENABLED \
                -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
                -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" \
                -o "$runtime_object" \
                >"$WORK_DIR/llvm.runtime.compile.out" \
                2>"$WORK_DIR/llvm.runtime.compile.err" || {
                    cat "$WORK_DIR/llvm.runtime.compile.err" >&2
                    fail "LLVM runtime object did not compile"
                }
        fi
        "$CLANG" -x ir "$artifact" -x none "$runtime_object" \
            -pthread -lm -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" 2>"$WORK_DIR/$backend.compile.err" || {
                cat "$WORK_DIR/$backend.compile.err" >&2
                fail "LLVM artifact did not compile"
            }
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in logical-record-collection-cross-identity \
        logical-record-collection-index-use-order \
        logical-record-collection-index-missing-formal \
        logical-record-collection-index-wrong-formal \
        logical-record-collection-index-wrong-index-type \
        logical-record-collection-index-field-type \
        logical-record-array-int-abi-layout \
        logical-record-array-string-abi-layout; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    mutation_script="$MUTATIONS"
    mutation_kind="$mutation"
    if [[ "$mutation" == logical-record-collection-index-* ]]; then
        mutation_script="$INDEX_MUTATIONS"
        mutation_kind="${mutation#logical-record-collection-index-}"
    fi
    python "$mutation_script" "$MIR" "$mutation_kind" \
        "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$output" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] collection identity + ABI join + C/LLVM parity/negatives: PASS"
