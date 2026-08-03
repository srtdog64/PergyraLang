#!/usr/bin/env bash
# One self MIR carries a Point through Wrap<Point>, Array<Point>, index, and x.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-constructed-record-array-member"
DRIVER_BIN="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_CONSTRUCTED_RECORD_ARRAY_MEMBER_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_DIR="${PGY_SELFHOST_CONSTRUCTED_RECORD_ARRAY_MEMBER_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_constructed_record_array_member}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/generic_member_record_array_return_flow.pgy"
MIR="$WORK_DIR/record-array-member.one.mir.json"
NATIVE_MIR="$WORK_DIR/record-array-member.native.mir.json"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_file() { [[ -f "$1" ]] || fail "missing file: ${1#"$ROOT_DIR/"}"; }
root_relative() { pgy_selfhost_path_relative_to_root "$1"; }
hash_file() { sha256sum "$1" | awk '{print $1}'; }

assert_owner_ratchet() {
    local owner lines total=0
    while IFS= read -r owner; do
        require_file "$owner"
        lines="$(wc -l <"$owner")"
        [[ "$lines" -le 220 ]] || fail "owner hard cap exceeded: ${owner#"$ROOT_DIR/"}=$lines/220"
        total=$((total + lines))
    done <<EOF
$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_mixed_constructed_member_shape_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_program_admission_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_specialization_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_main_graph_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_source_identity_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_identity_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_instruction_envelope_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_main_admission_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_array_abi_absence_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_point_abi_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_representation_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_plan_join_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_closed_module_call_abi_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_plan_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_abi_projection_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_c_emission_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_llvm_emission_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_projection_owner.pgy
EOF
    [[ "$total" -le 2100 ]] || fail "constructed record Array family cap exceeded: $total/2100"
    for shared_owner in direct_mir_array_storage_layout_contract_owner.pgy direct_mir_array_storage_abi_projection_owner.pgy direct_mir_array_storage_symbol_owner.pgy direct_mir_array_storage_c_assertion_owner.pgy; do
        require_file "$ROOT_DIR/src/self_hosted/compiler/$shared_owner"
        [[ "$(wc -l <"$ROOT_DIR/src/self_hosted/compiler/$shared_owner")" -le 140 ]] || fail "shared Array storage owner hard cap exceeded: $shared_owner"
    done
    for aggregate_owner_cap in direct_mir_aggregate_value_flow_fact_owner.pgy:220 direct_mir_aggregate_value_flow_target_projection_owner.pgy:100; do
        local aggregate_owner="${aggregate_owner_cap%%:*}" aggregate_cap="${aggregate_owner_cap##*:}"
        require_file "$ROOT_DIR/src/self_hosted/compiler/$aggregate_owner"
        [[ "$(wc -l <"$ROOT_DIR/src/self_hosted/compiler/$aggregate_owner")" -le "$aggregate_cap" ]] || fail "aggregate owner hard cap exceeded: $aggregate_owner"
    done
    grep -Fq 'DirectMirThreeRoutineMixedConstructedGenericMember()' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_classification_owner.pgy" || fail "mixed classification is not owned"
    grep -Fq 'CompileAdmittedDirectMirConstructedRecordArrayMember(' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_projection_owner.pgy" || fail "record Array projection is not routed"
    for emitter in direct_mir_constructed_record_array_member_c_emission_owner.pgy direct_mir_constructed_record_array_member_llvm_emission_owner.pgy; do
        ! grep -Eq 'MirMachineLayerAdmittedJsonInput|MirExpressionGraphSequence|JsonObjectFact' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter reopened admitted MIR"
        ! grep -Eq 'malloc\(|realloc\(|free\(|pgy_array_[A-Za-z0-9_]*\(' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter owns a runtime fallback"
    done
    grep -Fq 'direct_mir_array_storage_abi_projection_owner.pgy' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_abi_projection_owner.pgy" || fail "record Array target ABI bypassed the storage projection owner"
    ! grep -Fq 'direct_mir_array_storage_layout_contract_owner.pgy' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_abi_projection_owner.pgy" || fail "record Array target ABI reopened the storage layout owner"
    grep -Fq 'DirectMirAggregateValueFlowFactReady(plan.aggregate_flow)' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_plan_owner.pgy" || fail "record Array plan bypassed the aggregate value-flow fact"
    grep -Fq 'DirectMirAggregateValueFlowTypedArrayAbsenceNominalElementAbi()' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_plan_owner.pgy" || fail "record Array plan lost typed-absence nominal ABI provenance"
    ! grep -Eq 'MirMachineLayerAdmittedJsonInput|JsonObjectFact|ClassificationFact|target_projection|CompilerTargetCpu' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_aggregate_value_flow_fact_owner.pgy" || fail "aggregate value-flow owner reopened family or target input"
    ! grep -Fq 'direct_mir_closed_module_call_abi_owner.pgy' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_record_array_member_plan_owner.pgy" || fail "record Array plan reconstructed the shared call receipt"
    for emitter in direct_mir_constructed_record_array_member_c_emission_owner.pgy direct_mir_constructed_record_array_member_llvm_emission_owner.pgy; do
        grep -Fq 'direct_mir_aggregate_value_flow_target_projection_owner.pgy' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter bypassed the aggregate target projection"
        ! grep -Eq 'plan\.(representation|call_abi|storage_count|selected_index|value_carriage)|plan\.aggregate_flow\.selected_index|define internal|\[1 x|i64 1,|ptr null|, 1, 1, NULL|\[0\] =' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter re-owned aggregate decisions"
    done
}

project() {
    local input="$1" target="$2" output="$3"
    rm -f "$output" "$output.stdout" "$output.stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$input")" -o "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr") || {
        cat "$output.stdout" "$output.stderr" >&2 || true
        fail "$target rejected constructed record Array MIR"
    }
    [[ -s "$output" ]] || fail "$target emitted no constructed record Array artifact"
}

reject_mutation() {
    local name="$1" target="${2:-c}" output
    output="$WORK_DIR/$name.$target.artifact"
    rm -f "$output" "$output.stdout" "$output.stderr"
    if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$WORK_DIR/$name.json")" -o "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr"); then
        fail "$target accepted constructed record Array mutation: $name"
    fi
    [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
    grep -Eq 'CODEGEN ERROR|MIR .*invalid|constructed record Array|mixed constructed member|three-routine' "$output.stdout" "$output.stderr" || fail "$target rejection lost its owned diagnostic: $name"
    ! grep -Eq 'Array argument|Array return|struct argument|generic nominal|two-routine' "$output.stdout" "$output.stderr" || fail "$target retried another interpretation: $name"
}

compile_and_expect() {
    local stem="$1" expected="$2"
    "$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/$stem.c" -o "$WORK_DIR/$stem-c.exe"
    "$CLANG" "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem-llvm.exe" 2>"$WORK_DIR/$stem.llvm.log"
    printf '%s\n' "$expected" >"$WORK_DIR/$stem.expected"
    "$WORK_DIR/$stem-c.exe" | tr -d '\r' >"$WORK_DIR/$stem-c.out"
    "$WORK_DIR/$stem-llvm.exe" | tr -d '\r' >"$WORK_DIR/$stem-llvm.out"
    cmp -s "$WORK_DIR/$stem.expected" "$WORK_DIR/$stem-c.out" || fail "C output is not exact $expected"
    cmp -s "$WORK_DIR/$stem.expected" "$WORK_DIR/$stem-llvm.out" || fail "LLVM output is not exact $expected"
}

for path in "$SOURCE" "$DRIVER_BIN" "$PGY"; do require_file "$path"; done
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "missing C compiler"
command -v "$CLANG" >/dev/null || fail "missing LLVM compiler"
assert_owner_ratchet
mkdir -p "$WORK_DIR"
rm -f "$MIR" "$NATIVE_MIR"
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified "$(root_relative "$SOURCE")" -o "$(root_relative "$MIR")") || fail "source-to-MIR rejected record Array fixture"
mir_digest="$(hash_file "$MIR")"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$NATIVE_MIR") || fail "native MIR oracle rejected record Array fixture"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_constructed_record_array_member_mutations.py" "$MIR" "$NATIVE_MIR" compare || fail "native/self record Array semantics drifted"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_constructed_record_array_member_mutations.py" "$MIR" "$WORK_DIR"

project "$MIR" c "$WORK_DIR/baseline.c"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "C projection mutated MIR"
project "$MIR" llvm "$WORK_DIR/baseline.ll"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "LLVM projection mutated MIR"
grep -Fq 'Point *data; size_t length; size_t capacity; void *allocator;' "$WORK_DIR/baseline.c" || fail "C lost stable four-field Array ABI"
grep -Fq 'return (pgy_Point_array){_pgy_array_storage_0, 1, 1, NULL};' "$WORK_DIR/baseline.c" || fail "C lost allocator provenance slot"
grep -Fq 'RecordArrayWrapper_Wrap_Point(RecordArrayWrapper self, Point value, Point *_pgy_array_storage_0)' "$WORK_DIR/baseline.c" || fail "C lost collision-proof Point by-value/caller-storage ABI"
grep -Fq 'RecordArrayWrapper_Echo_Array_Point_(RecordArrayWrapper self, pgy_Point_array value)' "$WORK_DIR/baseline.c" || fail "C lost Array<Point> by-value Echo ABI"
grep -Fq 'RecordArrayWrapper_Echo_Array_Point_(pgy_receiver, pgy_inner)' "$WORK_DIR/baseline.c" || fail "C flattened nested member calls"
[[ "$(grep -Fc 'Point _pgy_array_storage_0[1];' "$WORK_DIR/baseline.c")" -eq 1 ]] || fail "C storage owner is not unique"
! grep -Eq '(^|[^A-Za-z0-9_])__[A-Za-z0-9_]' "$WORK_DIR/baseline.c" || fail "C emitted a reserved double-underscore identifier"
grep -Fq '_Static_assert(sizeof(pgy_Point_array) == 32' "$WORK_DIR/baseline.c" || fail "C lost record Array storage size receipt"
grep -Fq 'offsetof(pgy_Point_array, allocator) == 24' "$WORK_DIR/baseline.c" || fail "C lost record Array allocator offset receipt"
grep -Fq 'Point pgy_first = pgy_result.data[0];' "$WORK_DIR/baseline.c" || fail "C flattened first SSA"
! grep -Eq 'pgy_ai|malloc|realloc|free|pgy_array_[A-Za-z0-9_]*\(|printf\("%lld\\n", \(long long\)45\)' "$WORK_DIR/baseline.c" || fail "C reintroduced fallback or constant output"
[[ "$(grep -Fc 'alloca [1 x %Point]' "$WORK_DIR/baseline.ll")" -eq 1 ]] || fail "LLVM storage owner is not unique"
grep -Fq '@RecordArrayWrapper_Wrap_Point(%RecordArrayWrapper %receiver, %Point %point, ptr %array.data)' "$WORK_DIR/baseline.ll" || fail "LLVM lost Point by-value/caller storage"
grep -Fq 'store %Point %value, ptr %pgy.array.storage, align 4' "$WORK_DIR/baseline.ll" || fail "LLVM Wrap no longer fills caller storage"
grep -Fq '%array.0 = insertvalue { ptr, i64, i64, ptr } poison, ptr %pgy.array.storage, 0' "$WORK_DIR/baseline.ll" || fail "LLVM lost Array data slot"
grep -Fq '%array.1 = insertvalue { ptr, i64, i64, ptr } %array.0, i64 1, 1' "$WORK_DIR/baseline.ll" || fail "LLVM lost Array length slot"
grep -Fq '%array.2 = insertvalue { ptr, i64, i64, ptr } %array.1, i64 1, 2' "$WORK_DIR/baseline.ll" || fail "LLVM lost Array capacity slot"
grep -Fq '%array.3 = insertvalue { ptr, i64, i64, ptr } %array.2, ptr null, 3' "$WORK_DIR/baseline.ll" || fail "LLVM lost Array allocator slot"
grep -Fq 'ret { ptr, i64, i64, ptr } %array.3' "$WORK_DIR/baseline.ll" || fail "LLVM returned an incomplete Array shell"
! grep -Fq '{ ptr, i64, i64 }' "$WORK_DIR/baseline.ll" || fail "LLVM regressed to a three-field Array"
grep -Fq 'define internal { ptr, i64, i64, ptr } @RecordArrayWrapper_Echo_Array_Point_(%RecordArrayWrapper %self, { ptr, i64, i64, ptr } %value)' "$WORK_DIR/baseline.ll" || fail "LLVM lost aggregate-by-value Echo ABI"
grep -Fq '%result = call { ptr, i64, i64, ptr } @RecordArrayWrapper_Echo_Array_Point_(%RecordArrayWrapper %receiver, { ptr, i64, i64, ptr } %inner)' "$WORK_DIR/baseline.ll" || fail "LLVM flattened Echo call"
grep -Fq '%result.data = extractvalue { ptr, i64, i64, ptr } %result, 0' "$WORK_DIR/baseline.ll" || fail "LLVM lost result data extraction"
grep -Fq '%first.ptr = getelementptr inbounds %Point, ptr %result.data, i64 0' "$WORK_DIR/baseline.ll" || fail "LLVM lost index-zero projection"
grep -Fq '%first = load %Point, ptr %first.ptr, align 4' "$WORK_DIR/baseline.ll" || fail "LLVM lost index load"
grep -Fq '%field = extractvalue %Point %first, 0' "$WORK_DIR/baseline.ll" || fail "LLVM lost field projection"
grep -Fq '@printf(ptr @.pgy.int.line.format, i64 %print)' "$WORK_DIR/baseline.ll" || fail "LLVM output no longer consumes projected field"
! grep -Eq '@pgy_|malloc|realloc|free' "$WORK_DIR/baseline.ll" || fail "LLVM reintroduced runtime allocation"
compile_and_expect baseline 45

for permutation in routine-order-reverse specialization-order-swap declaration-order-swap declaration-method-order-swap source-local-order-swap combined-order generic-formal-rename; do
    project "$WORK_DIR/$permutation.json" c "$WORK_DIR/$permutation.c"
    project "$WORK_DIR/$permutation.json" llvm "$WORK_DIR/$permutation.ll"
    cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/$permutation.c" || fail "C artifact drifted for $permutation"
    cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/$permutation.ll" || fail "LLVM artifact drifted for $permutation"
done
for variant in field-value-seventy-five collision-names hidden-storage-collision; do
    project "$WORK_DIR/$variant.json" c "$WORK_DIR/$variant.c"
    project "$WORK_DIR/$variant.json" llvm "$WORK_DIR/$variant.ll"
done
compile_and_expect field-value-seventy-five 75
compile_and_expect collision-names 45
compile_and_expect hidden-storage-collision 45
grep -Fq 'Point *_pgy_array_storage_1)' "$WORK_DIR/hidden-storage-collision.c" || fail "C storage symbol did not avoid the source parameter"
cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/collision-names.c" || fail "C source local spelling leaked"
cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/collision-names.ll" || fail "LLVM source local spelling leaked"

for mutation in missing-record record-kind-drift wrapper-kind-drift missing-method field-type-drift missing-specialization duplicate-specialization-ordinal specialization-owner-drift outer-actual-drift inner-actual-drift inner-symbol-drift array-element-edge-drift identity-body-drift point-literal-edge-drift nested-point-kind-drift inner-target-drift index-value-drift output-field-drift stale-result-use stale-first-use forged-array-receipt forged-wrapper-receipt point-receipt-id-drift first-receipt-missing point-offset-drift point-runtime-drift point-representation-drift point-only-offset-drift first-only-field-name-drift source-identity-collision main-name-drift main-expr1-drift output-envelope-drift extra-root-fact unreachable-wrap; do reject_mutation "$mutation"; done
for mutation in outer-actual-drift inner-symbol-drift nested-point-kind-drift forged-array-receipt point-offset-drift point-runtime-drift point-representation-drift point-only-offset-drift source-identity-collision output-envelope-drift; do reject_mutation "$mutation" llvm; done
echo "[$LABEL] PASS: one MIR, Point through caller-owned Array member flow, C/LLVM exact 45, stable four-field ABI, seven invariants, three variants, 35 C negatives, 10 LLVM sentinels"
