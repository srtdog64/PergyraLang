#!/usr/bin/env bash
# One self MIR drives inferred generic return/assignment through C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-inferred-generic-scalar"
DRIVER_BIN="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_INFERRED_GENERIC_SCALAR_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_DIR="${PGY_SELFHOST_INFERRED_GENERIC_SCALAR_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_inferred_generic_scalar}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/generic_return_assignment_inferred_flow.pgy"
MIR="$WORK_DIR/inferred-generic-scalar.one.mir.json"
NATIVE_MIR="$WORK_DIR/inferred-generic-scalar.native.mir.json"
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
$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_shape_owner.pgy|40
$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_classification_owner.pgy|110
$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_projection_owner.pgy|80
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_routine_program_envelope_owner.pgy|60
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_nominal_graph_shape_owner.pgy|90
$ROOT_DIR/src/self_hosted/compiler/direct_mir_instruction_abi_absence_owner.pgy|50
$ROOT_DIR/src/self_hosted/compiler/direct_mir_mixed_lane_generic_specialization_fact_owner.pgy|180
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_scalar_program_admission_owner.pgy|80
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_scalar_graph_fact_owner.pgy|170
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_scalar_program_identity_owner.pgy|220
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_scalar_instruction_admission_owner.pgy|230
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_scalar_abi_fact_owner.pgy|80
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_scalar_plan_owner.pgy|140
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_scalar_c_emission_owner.pgy|100
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_scalar_llvm_emission_owner.pgy|90
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_scalar_projection_owner.pgy|40
EOF
    [[ "$total" -le 1800 ]] || fail "inferred generic scalar owner family cap exceeded: $total/1800"
    grep -Fq 'DirectMirThreeRoutineInferredGenericScalar()' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_classification_owner.pgy" || fail "mixed-lane classification is not owned"
    grep -Fq 'CompileAdmittedDirectMirInferredGenericScalar(' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_projection_owner.pgy" || fail "mixed-lane projection is not routed"
    for emitter in direct_mir_inferred_generic_scalar_c_emission_owner.pgy direct_mir_inferred_generic_scalar_llvm_emission_owner.pgy; do
        ! grep -Eq 'MirMachineLayerAdmittedJsonInput|MirExpressionGraphSequence|JsonObjectFact' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter reopened admitted MIR facts"
        ! grep -Fq 'CompilerSymbolCGenericSpecializationName' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter reconstructed the specialization symbol"
    done
}

project() {
    local input="$1" target="$2" output="$3"
    rm -f "$output" "$output.stdout" "$output.stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$input")" -o "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr") || {
        cat "$output.stdout" "$output.stderr" >&2 || true
        fail "$target rejected inferred generic scalar MIR"
    }
    [[ -s "$output" ]] || fail "$target emitted no inferred generic scalar artifact"
}

reject_mutation() {
    local name="$1" target="${2:-c}" output
    output="$WORK_DIR/$name.$target.artifact"
    rm -f "$output" "$output.stdout" "$output.stderr"
    if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$WORK_DIR/$name.json")" -o "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr"); then
        fail "$target accepted inferred generic scalar mutation: $name"
    fi
    [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
    grep -Eq 'CODEGEN ERROR|MIR .*invalid|inferred scalar|three-routine' "$output.stdout" "$output.stderr" || fail "$target rejection lost its owned diagnostic: $name"
    ! grep -Eq 'Array argument|generic struct value-flow|struct argument' "$output.stdout" "$output.stderr" || fail "$target retried Array argument projection: $name"
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
# The gate must produce source MIR exactly once; every projection reuses it.
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified "$(root_relative "$SOURCE")" -o "$(root_relative "$MIR")") || fail "source-to-MIR producer rejected inferred generic scalar fixture"
mir_digest="$(hash_file "$MIR")"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$NATIVE_MIR") || fail "native MIR oracle rejected inferred generic scalar fixture"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_inferred_generic_scalar_mutations.py" "$MIR" "$NATIVE_MIR" compare || fail "native/self inferred scalar graph or ABI parity drifted"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_inferred_generic_scalar_mutations.py" "$MIR" "$WORK_DIR"

project "$MIR" c "$WORK_DIR/baseline.c"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "C projection mutated MIR"
project "$MIR" llvm "$WORK_DIR/baseline.ll"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "LLVM projection mutated MIR"
[[ "$(grep -Fo 'Identity_Int(' "$WORK_DIR/baseline.c" | wc -l)" -eq 3 ]] || fail "C lost one specialization definition and two calls"
[[ "$(grep -Fo '@Identity_Int(' "$WORK_DIR/baseline.ll" | wc -l)" -eq 3 ]] || fail "LLVM lost one specialization definition and two calls"
[[ "$(grep -Fo 'ReturnIdentity(' "$WORK_DIR/baseline.c" | wc -l)" -eq 2 ]] || fail "C lost the real wrapper"
[[ "$(grep -Fo '@ReturnIdentity(' "$WORK_DIR/baseline.ll" | wc -l)" -eq 2 ]] || fail "LLVM lost the real wrapper"
grep -Fq 'int32_t result = 0;' "$WORK_DIR/baseline.c" || fail "C lost initial SSA materialization"
grep -Fq 'result = Identity_Int(41);' "$WORK_DIR/baseline.c" || fail "C lost inferred assignment"
grep -Fq '%pgy.initial = add i32 0, 0' "$WORK_DIR/baseline.ll" || fail "LLVM lost initial SSA materialization"
grep -Fq '%pgy.latest = call i32 @Identity_Int(i32 41)' "$WORK_DIR/baseline.ll" || fail "LLVM lost inferred assignment"
grep -Fq '%pgy.returned = call i32 @ReturnIdentity(i32 %pgy.latest)' "$WORK_DIR/baseline.ll" || fail "LLVM used a stale SSA value"
! grep -Eq 'return[[:space:]]+41|pgy.returned[[:space:]]*=[[:space:]]*41' "$WORK_DIR/baseline.c" "$WORK_DIR/baseline.ll" || fail "artifact hard-coded expected output"
compile_and_expect baseline 41

for permutation in routine-order-reverse routine-order-rotate specialization-order-swap combined-order-rotate specialization-owner-renumber; do
    project "$WORK_DIR/$permutation.json" c "$WORK_DIR/$permutation.c"
    project "$WORK_DIR/$permutation.json" llvm "$WORK_DIR/$permutation.ll"
    cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/$permutation.c" || fail "C artifact drifted for $permutation"
    cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/$permutation.ll" || fail "LLVM artifact drifted for $permutation"
done
for variant in initial-value-seven assigned-value-forty-three; do
    project "$WORK_DIR/$variant.json" c "$WORK_DIR/$variant.c"
    project "$WORK_DIR/$variant.json" llvm "$WORK_DIR/$variant.ll"
done
cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/initial-value-seven.c" && fail "C erased the changed initial value"
cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/initial-value-seven.ll" && fail "LLVM erased the changed initial value"
compile_and_expect initial-value-seven 41
compile_and_expect assigned-value-forty-three 43

for mutation in missing-generic-formal duplicate-generic-formal generic-param-type-drift generic-param-abi-drift generic-return-type-drift generic-receiver-drift generic-body-drift generic-return-abi-drift wrapper-param-type-drift wrapper-param-abi-drift wrapper-return-drift wrapper-call-target-drift wrapper-call-argument-edge-drift wrapper-argument-name-drift wrapper-return-abi-drift missing-specialization extra-specialization duplicate-specialization-coordinate specialization-lane-drift specialization-owner-equality specialization-owner-zero specialization-ordinal-drift specialization-target-drift specialization-owner-drift specialization-callable-drift specialization-formal-drift specialization-actual-drift specialization-symbol-drift missing-source-local source-local-type-drift initial-kind-drift initial-result-drift initial-abi-drift initial-type-text-drift assignment-source-drift assignment-result-alias assignment-local-drift assignment-carriage-drift assignment-target-text-drift assignment-target-drift assignment-call-target-drift assignment-call-edge-drift assignment-literal-kind-drift assignment-abi-drift duplicate-result-definition foreign-latest-result output-wrapper-target-drift output-local-drift output-call-edge-drift stale-output-use missing-output-use output-abi-type-drift output-abi-layout-drift unreachable-main unreachable-wrapper; do
    reject_mutation "$mutation"
done
for mutation in specialization-symbol-drift stale-output-use wrapper-call-target-drift; do reject_mutation "$mutation" llvm; done
echo "[$LABEL] PASS: one MIR, mixed-lane inference, C/LLVM exact 41, five metamorphics, two value variants, 55 C negatives, 3 LLVM sentinels"
