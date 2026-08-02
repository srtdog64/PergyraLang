#!/usr/bin/env bash
# One self MIR drives nested inferred generic value-receiver calls through C/LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-inferred-generic-member"
DRIVER_BIN="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_INFERRED_GENERIC_MEMBER_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_DIR="${PGY_SELFHOST_INFERRED_GENERIC_MEMBER_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_inferred_generic_member}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/generic_member_inferred_flow.pgy"
VESSEL_SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/generic_vessel_member_inferred_flow.pgy"
MIR="$WORK_DIR/inferred-generic-member.one.mir.json"
NATIVE_MIR="$WORK_DIR/inferred-generic-member.native.mir.json"
VESSEL_DIR="$WORK_DIR/vessel"
VESSEL_MIR="$VESSEL_DIR/inferred-generic-member.one.mir.json"
VESSEL_NATIVE_MIR="$VESSEL_DIR/inferred-generic-member.native.mir.json"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_file() { [[ -f "$1" ]] || fail "missing file: ${1#"$ROOT_DIR/"}"; }
root_relative() { pgy_selfhost_path_relative_to_root "$1"; }
hash_file() { sha256sum "$1" | awk '{print $1}'; }

assert_owner_ratchet() {
    local owner cap lines total=0
    while IFS='|' read -r owner cap; do
        require_file "$owner"; lines="$(wc -l <"$owner")"
        [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: ${owner#"$ROOT_DIR/"}=$lines/$cap"
        total=$((total + lines))
    done <<EOF
$ROOT_DIR/src/self_hosted/compiler/direct_mir_two_routine_shape_owner.pgy|80
$ROOT_DIR/src/self_hosted/compiler/direct_mir_two_routine_classification_owner.pgy|120
$ROOT_DIR/src/self_hosted/compiler/direct_mir_exact_json_array_cardinality_owner.pgy|90
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_routine_array_shape_owner.pgy|50
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_program_admission_owner.pgy|70
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_specialization_fact_owner.pgy|160
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_member_signature_fact_owner.pgy|190
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_declaration_fact_owner.pgy|170
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_host_kind_fact_owner.pgy|60
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_graph_fact_owner.pgy|180
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_program_identity_owner.pgy|190
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_instruction_admission_owner.pgy|220
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_representation_fact_owner.pgy|100
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_plan_owner.pgy|120
$ROOT_DIR/src/self_hosted/compiler/direct_mir_member_receiver_target_projection_owner.pgy|90
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_c_emission_owner.pgy|100
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_llvm_emission_owner.pgy|100
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_projection_owner.pgy|40
EOF
    [[ "$total" -le 2100 ]] || fail "inferred generic member owner family cap exceeded: $total/2100"
    grep -Fq 'DirectMirTwoRoutineInferredGenericMember()' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_two_routine_classification_owner.pgy" || fail "member classification is not owned"
    grep -Fq 'CompileAdmittedDirectMirInferredGenericMember(' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy" || fail "member projection is not routed"
    [[ "$(grep -Fc 'DirectMirTwoRoutineClassificationFromAdmitted(admitted)' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy")" -eq 1 ]] || fail "two-routine classification is not single-shot"
    [[ "$(grep -c '^import ' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_projection_owner.pgy")" -eq 2 ]] || fail "member target selection imported another planner"
    [[ "$(grep -Ec 'PlanFromAdmitted\(' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_projection_owner.pgy")" -eq 1 ]] || fail "member target selection retried another planner"
    grep -Fq 'internal_single_int_value_class' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_representation_fact_owner.pgy" || fail "internal value-class representation is not explicit"
    grep -Fq 'internal_single_int_mutable_identity_vessel' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_representation_fact_owner.pgy" || fail "mutable-identity vessel representation is not explicit"
    for emitter in direct_mir_inferred_generic_member_c_emission_owner.pgy direct_mir_inferred_generic_member_llvm_emission_owner.pgy; do
        ! grep -Eq 'MirMachineLayerAdmittedJsonInput|MirExpressionGraphSequence|JsonObjectFact' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter reopened admitted MIR"
        ! grep -Fq 'CompilerSymbolCGenericSpecializationName' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter reconstructed the specialization symbol"
        ! grep -Eq '"(class|vessel|Box|Cell)"|fixture' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter dispatched on a host or fixture spelling"
    done
}

project() {
    local input="$1" target="$2" output="$3"
    rm -f "$output" "$output.stdout" "$output.stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$input")" "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr") || {
        cat "$output.stdout" "$output.stderr" >&2 || true
        fail "$target rejected inferred generic member MIR"
    }
    [[ -s "$output" ]] || fail "$target emitted no inferred generic member artifact"
}

reject_mutation() {
    local name="$1" target="${2:-c}" input_dir="${3:-$WORK_DIR}" output
    output="$input_dir/$name.$target.artifact"
    rm -f "$output" "$output.stdout" "$output.stderr"
    if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$input_dir/$name.json")" "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr"); then
        fail "$target accepted inferred generic member mutation: $name"
    fi
    [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
    grep -Eq 'CODEGEN ERROR|MIR .*invalid|inferred.*member|two-routine' "$output.stdout" "$output.stderr" || fail "$target rejection lost its owned diagnostic: $name"
    ! grep -Eq 'Option nominal|generic struct value-flow|struct value-flow|two-routine nominal classification|inferred generic program envelope' "$output.stdout" "$output.stderr" || fail "$target retried another two-routine interpretation: $name"
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

for path in "$SOURCE" "$VESSEL_SOURCE" "$DRIVER_BIN" "$PGY"; do require_file "$path"; done
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "missing C compiler"
command -v "$CLANG" >/dev/null || fail "missing LLVM compiler"
assert_owner_ratchet
mkdir -p "$WORK_DIR"
rm -f "$MIR" "$NATIVE_MIR"
# The gate must produce source MIR exactly once; every projection reuses it.
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified "$(root_relative "$SOURCE")" "$(root_relative "$MIR")") || fail "source-to-MIR rejected inferred generic member fixture"
mir_digest="$(hash_file "$MIR")"
(cd "$ROOT_DIR" && "$PGY" --mir-json "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$NATIVE_MIR") || fail "native MIR oracle rejected inferred generic member fixture"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_inferred_generic_member_mutations.py" "$MIR" "$NATIVE_MIR" compare || fail "native/self member semantic parity drifted"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_inferred_generic_member_mutations.py" "$MIR" "$WORK_DIR"

project "$MIR" c "$WORK_DIR/baseline.c"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "C projection mutated MIR"
project "$MIR" llvm "$WORK_DIR/baseline.ll"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "LLVM projection mutated MIR"
[[ "$(grep -Fo 'Box_Echo_Int(' "$WORK_DIR/baseline.c" | wc -l)" -eq 3 ]] || fail "C lost one method definition and two calls"
[[ "$(grep -Fo '@Box_Echo_Int(' "$WORK_DIR/baseline.ll" | wc -l)" -eq 3 ]] || fail "LLVM lost one method definition and two calls"
grep -Fq 'Box_Echo_Int(_pgy_receiver_0, 41)' "$WORK_DIR/baseline.c" || fail "C lost the inner value receiver"
grep -Fq 'Box_Echo_Int(_pgy_receiver_0, _pgy_inner_0)' "$WORK_DIR/baseline.c" || fail "C flattened inner-to-outer flow"
grep -Fq '@Box_Echo_Int(%Box %box, i32 41)' "$WORK_DIR/baseline.ll" || fail "LLVM lost the inner value receiver"
grep -Fq '@Box_Echo_Int(%Box %box, i32 %inner)' "$WORK_DIR/baseline.ll" || fail "LLVM flattened inner-to-outer flow"
grep -Fq 'define internal i32 @Box_Echo_Int(%Box %self, i32 %value)' "$WORK_DIR/baseline.ll" || fail "LLVM lost value receiver carriage"
compile_and_expect baseline 41

for permutation in routine-order-swap source-local-order-swap specialization-order-swap combined-order-swap specialization-owner-renumber; do
    project "$WORK_DIR/$permutation.json" c "$WORK_DIR/$permutation.c"
    project "$WORK_DIR/$permutation.json" llvm "$WORK_DIR/$permutation.ll"
    cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/$permutation.c" || fail "C artifact drifted for $permutation"
    cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/$permutation.ll" || fail "LLVM artifact drifted for $permutation"
done
for variant in marker-value-seven argument-value-seventy-three semantic-rename collision-names field-local-same-name; do
    project "$WORK_DIR/$variant.json" c "$WORK_DIR/$variant.c"
    project "$WORK_DIR/$variant.json" llvm "$WORK_DIR/$variant.ll"
done
cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/marker-value-seven.c" && fail "C erased changed receiver state"
cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/marker-value-seven.ll" && fail "LLVM erased changed receiver state"
grep -Fq '_pgy_receiver_0 = {7};' "$WORK_DIR/marker-value-seven.c" || fail "C lost changed receiver state"
grep -Fq '%box = insertvalue %Box poison, i32 7, 0' "$WORK_DIR/marker-value-seven.ll" || fail "LLVM lost changed receiver state"
compile_and_expect marker-value-seven 41
compile_and_expect argument-value-seventy-three 73
compile_and_expect semantic-rename 41
compile_and_expect collision-names 41
compile_and_expect field-local-same-name 41
cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/collision-names.c" || fail "C source local spelling leaked into backend temporaries"
cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/collision-names.ll" || fail "LLVM source local spelling leaked into backend temporaries"
[[ "$(grep -Fo 'Crate_Reflect_Int(' "$WORK_DIR/semantic-rename.c" | wc -l)" -eq 3 ]] || fail "C hard-coded Box/Echo identity"
[[ "$(grep -Fo '@Crate_Reflect_Int(' "$WORK_DIR/semantic-rename.ll" | wc -l)" -eq 3 ]] || fail "LLVM hard-coded Box/Echo identity"

for mutation in declaration-kind-drift nominal-kind-drift declaration-name-drift declaration-physical-layout field-name-empty field-type-drift field-kind-drift method-name-drift method-return-drift method-kind-drift method-contract-drift method-source-id-collision field-source-id-collision routine-owner-drift receiver-carriage-drift missing-generic-formal duplicate-generic-formal receiver-name-drift receiver-type-forged receiver-pass-drift receiver-abi-forged value-param-type-drift value-param-carriage-drift value-param-abi-drift routine-return-drift method-body-drift method-return-abi-drift method-generic-scalar-tail method-param-scalar-tail main-generic-scalar-tail main-param-scalar-tail source-local-scalar-tail field-scalar-tail method-scalar-tail parallel-scalar-tail missing-specialization extra-specialization duplicate-specialization-coordinate specialization-owner-zero specialization-owner-disagreement specialization-lane-drift specialization-target-drift specialization-owner-drift specialization-callable-drift specialization-symbol-drift specialization-formal-drift specialization-actual-drift specialization-scalar-tail specialization-formal-tail specialization-actual-tail constructor-target-drift constructor-edge-drift constructor-result-drift constructor-physical-layout nested-receiver-drift nested-method-drift inner-target-kind-drift outer-target-name-drift inner-argument-edge-drift outer-argument-edge-drift nested-root-drift nested-use-drift nested-result-drift output-local-drift output-edge-drift stale-output-use output-abi-drift source-local-type-drift unreachable-main unreachable-method; do reject_mutation "$mutation"; done
for mutation in specialization-symbol-drift inner-target-kind-drift stale-output-use constructor-physical-layout; do reject_mutation "$mutation" llvm; done

mkdir -p "$VESSEL_DIR"
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified "$(root_relative "$VESSEL_SOURCE")" "$(root_relative "$VESSEL_MIR")") || fail "source-to-MIR rejected vessel member fixture"
(cd "$ROOT_DIR" && "$PGY" --mir-json "$(pgy_path_for_compiler "$PGY" "$VESSEL_SOURCE")" >"$VESSEL_NATIVE_MIR") || fail "native MIR oracle rejected vessel member fixture"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_inferred_generic_member_mutations.py" "$VESSEL_MIR" "$VESSEL_NATIVE_MIR" compare || fail "native/self vessel nominal semantics drifted"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_inferred_generic_member_mutations.py" "$VESSEL_MIR" "$VESSEL_DIR"
vessel_digest="$(hash_file "$VESSEL_MIR")"
project "$VESSEL_MIR" c "$VESSEL_DIR/baseline.c"
project "$VESSEL_MIR" llvm "$VESSEL_DIR/baseline.ll"
[[ "$(hash_file "$VESSEL_MIR")" == "$vessel_digest" ]] || fail "vessel projection mutated MIR"
[[ "$(grep -Fo 'Cell_Echo_Int(' "$VESSEL_DIR/baseline.c" | wc -l)" -eq 3 ]] || fail "C lost one vessel method definition and two calls"
grep -Fq 'Cell_Echo_Int(Cell *self, int32_t value)' "$VESSEL_DIR/baseline.c" || fail "C lost mutable-identity receiver formal"
grep -Fq 'Cell _pgy_receiver_0 = {0};' "$VESSEL_DIR/baseline.c" || fail "C lost admitted receiver state initialization"
[[ "$(grep -Fo 'Cell_Echo_Int(&(_pgy_receiver_0),' "$VESSEL_DIR/baseline.c" | wc -l)" -eq 2 ]] || fail "C lost the stable receiver address"
! grep -Fq 'Cell_Echo_Int(_pgy_receiver_0,' "$VESSEL_DIR/baseline.c" || fail "C restored by-value vessel calls"
grep -Fq 'define internal i32 @Cell_Echo_Int(ptr %self, i32 %value)' "$VESSEL_DIR/baseline.ll" || fail "LLVM lost pointer receiver formal"
[[ "$(grep -Fo '%box = alloca %Cell' "$VESSEL_DIR/baseline.ll" | wc -l)" -eq 1 ]] || fail "LLVM lost unique stable receiver storage"
grep -Fq '%box.field.0 = getelementptr inbounds %Cell, ptr %box, i32 0, i32 0' "$VESSEL_DIR/baseline.ll" || fail "LLVM lost admitted receiver field address"
grep -Fq 'store i32 0, ptr %box.field.0, align 4' "$VESSEL_DIR/baseline.ll" || fail "LLVM lost admitted receiver state initialization"
[[ "$(grep -Fo '@Cell_Echo_Int(ptr %box,' "$VESSEL_DIR/baseline.ll" | wc -l)" -eq 2 ]] || fail "LLVM lost shared receiver pointer calls"
! grep -Fq '@Cell_Echo_Int(%Cell %box,' "$VESSEL_DIR/baseline.ll" || fail "LLVM restored aggregate-by-value vessel calls"
compile_and_expect vessel/baseline 42
project "$VESSEL_DIR/combined-order-swap.json" c "$VESSEL_DIR/combined-order-swap.c"
project "$VESSEL_DIR/combined-order-swap.json" llvm "$VESSEL_DIR/combined-order-swap.ll"
cmp -s "$VESSEL_DIR/baseline.c" "$VESSEL_DIR/combined-order-swap.c" || fail "C vessel artifact drifted under order swap"
cmp -s "$VESSEL_DIR/baseline.ll" "$VESSEL_DIR/combined-order-swap.ll" || fail "LLVM vessel artifact drifted under order swap"
for mutation in declaration-kind-drift nominal-kind-drift host-kind-subject routine-owner-drift receiver-carriage-value-drift receiver-pass-drift receiver-abi-forged constructor-physical-layout nested-use-drift stale-output-use specialization-symbol-drift; do reject_mutation "$mutation" c "$VESSEL_DIR"; done
for mutation in host-kind-subject receiver-carriage-value-drift receiver-abi-forged nested-use-drift specialization-symbol-drift; do reject_mutation "$mutation" llvm "$VESSEL_DIR"; done
echo "[$LABEL] PASS: one owner path, class exact 41 plus vessel exact 42, C/LLVM receiver ABI parity, six order invariants, five variants, 81 C negatives, 9 LLVM sentinels"
