#!/usr/bin/env bash
# One self MIR drives inferred generic Pair flow through real C and LLVM calls.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-inferred-generic-nominal"
DRIVER_BIN="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_INFERRED_GENERIC_NOMINAL_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_DIR="${PGY_SELFHOST_INFERRED_GENERIC_NOMINAL_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_inferred_generic_nominal}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/generic_struct_field_inferred_value_flow.pgy"
MIR="$WORK_DIR/inferred-generic.one.mir.json"
NATIVE_MIR="$WORK_DIR/inferred-generic.native.mir.json"
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
        [[ "$lines" -le "$cap" ]] ||
            fail "owner hard cap exceeded: ${owner#"$ROOT_DIR/"}=$lines/$cap"
        total=$((total + lines))
    done <<EOF
$ROOT_DIR/src/self_hosted/compiler/direct_mir_two_routine_classification_owner.pgy|100
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_routine_program_envelope_owner.pgy|60
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_nominal_graph_shape_owner.pgy|90
$ROOT_DIR/src/self_hosted/compiler/direct_mir_two_int_nominal_abi_shape_owner.pgy|40
$ROOT_DIR/src/self_hosted/compiler/direct_mir_instruction_abi_absence_owner.pgy|50
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_specialization_fact_owner.pgy|180
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_nominal_program_admission_owner.pgy|90
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_nominal_graph_fact_owner.pgy|170
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_nominal_program_identity_owner.pgy|220
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_nominal_instruction_admission_owner.pgy|190
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_nominal_abi_fact_owner.pgy|100
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_nominal_plan_owner.pgy|130
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_nominal_c_emission_owner.pgy|120
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_nominal_llvm_emission_owner.pgy|100
$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_nominal_projection_owner.pgy|40
EOF
    [[ "$total" -le 1800 ]] ||
        fail "inferred generic nominal owner family cap exceeded: $total/1800"
    lines="$(wc -l <"$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy")"
    [[ "$lines" -le 90 ]] || fail "multi-routine root cap exceeded: $lines/90"
    grep -Fq 'DirectMirTwoRoutineClassificationFromAdmitted(admitted)' \
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy" ||
        fail "two-routine structural classification is not routed"
    grep -Fq 'CompileAdmittedDirectMirInferredGenericNominal(' \
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy" ||
        fail "inferred generic nominal projection is not routed"
    for emitter in \
        direct_mir_inferred_generic_nominal_c_emission_owner.pgy \
        direct_mir_inferred_generic_nominal_llvm_emission_owner.pgy; do
        ! grep -Eq 'MirMachineLayerAdmittedJsonInput|MirExpressionGraphSequence|JsonObjectFact' \
            "$ROOT_DIR/src/self_hosted/compiler/$emitter" ||
            fail "$emitter reopened admitted JSON or expression graphs"
        ! grep -Fq 'CompilerSymbolCGenericSpecializationName' \
            "$ROOT_DIR/src/self_hosted/compiler/$emitter" ||
            fail "$emitter reconstructed the specialization symbol"
    done
}

project() {
    local input="$1" target="$2" output="$3"
    rm -f "$output" "$output.stdout" "$output.stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
        "$(root_relative "$input")" -o "$(root_relative "$output")" \
        >"$output.stdout" 2>"$output.stderr") ||
        { cat "$output.stdout" "$output.stderr" >&2 || true; fail "$target rejected inferred generic nominal MIR"; }
    [[ -s "$output" ]] || fail "$target emitted no inferred generic artifact"
}

reject_mutation() {
    local name="$1" target="${2:-c}" output
    output="$WORK_DIR/$name.$target.artifact"
    rm -f "$output" "$output.stdout" "$output.stderr"
    if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
        "$(root_relative "$WORK_DIR/$name.json")" -o "$(root_relative "$output")" \
        >"$output.stdout" 2>"$output.stderr"); then
        fail "$target accepted inferred generic mutation: $name"
    fi
    [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
    grep -Eq 'CODEGEN ERROR|MIR .*invalid|inferred generic|two-routine' \
        "$output.stdout" "$output.stderr" ||
        fail "$target rejection lost its owned diagnostic: $name"
    ! grep -Eq 'struct value-flow|Option nominal' \
        "$output.stdout" "$output.stderr" ||
        fail "$target retried a closed nominal interpretation: $name"
}

for path in "$SOURCE" "$DRIVER_BIN" "$PGY"; do require_file "$path"; done
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "missing C compiler"
command -v "$CLANG" >/dev/null || fail "missing LLVM compiler"
assert_owner_ratchet
mkdir -p "$WORK_DIR"
rm -f "$MIR" "$NATIVE_MIR"
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified \
    "$(root_relative "$SOURCE")" -o "$(root_relative "$MIR")") ||
    fail "source-to-MIR producer rejected inferred generic fixture"
mir_digest="$(hash_file "$MIR")"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
    "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$NATIVE_MIR") ||
    fail "native MIR oracle rejected inferred generic fixture"
"$PYTHON_BIN" \
    "$ROOT_DIR/tests/self_hosted/parity/one_mir_inferred_generic_nominal_mutations.py" \
    "$MIR" "$NATIVE_MIR" compare ||
    fail "native/self inferred generic graph or ABI parity drifted"
"$PYTHON_BIN" \
    "$ROOT_DIR/tests/self_hosted/parity/one_mir_inferred_generic_nominal_mutations.py" \
    "$MIR" "$WORK_DIR"

project "$MIR" c "$WORK_DIR/baseline.c"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "C projection mutated MIR"
project "$MIR" llvm "$WORK_DIR/baseline.ll"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "LLVM projection mutated MIR"
[[ "$(grep -Fo 'Identity_Int(' "$WORK_DIR/baseline.c" | wc -l)" -eq 3 ]] ||
    fail "C did not preserve one specialization definition and two calls"
[[ "$(grep -Fo '@Identity_Int(' "$WORK_DIR/baseline.ll" | wc -l)" -eq 3 ]] ||
    fail "LLVM did not preserve one specialization definition and two calls"
grep -Fq 'Pair pgy_pair = {Identity_Int(41), Identity_Int(1)};' \
    "$WORK_DIR/baseline.c" || fail "C flattened the inferred Pair initializer"
grep -Fq '%pgy.pair.1 = insertvalue %Pair' "$WORK_DIR/baseline.ll" ||
    fail "LLVM lost the inferred Pair aggregate"
grep -Fq '%pgy.pair.right = extractvalue %Pair %pgy.pair.1, 1' \
    "$WORK_DIR/baseline.ll" || fail "LLVM lost pair.right member flow"
! grep -Eq 'return[[:space:]]+42|pgy_sum[[:space:]]*=[[:space:]]*42' \
    "$WORK_DIR/baseline.c" "$WORK_DIR/baseline.ll" ||
    fail "generated artifact hard-coded the expected output"

"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/baseline.c" \
    -o "$WORK_DIR/baseline-c.exe"
"$CLANG" "$WORK_DIR/baseline.ll" -o "$WORK_DIR/baseline-llvm.exe" \
    2>"$WORK_DIR/llvm.compile.log"
printf '42\n' >"$WORK_DIR/expected.out"
"$WORK_DIR/baseline-c.exe" | tr -d '\r' >"$WORK_DIR/baseline-c.out"
"$WORK_DIR/baseline-llvm.exe" | tr -d '\r' >"$WORK_DIR/baseline-llvm.out"
cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/baseline-c.out" ||
    fail "C output is not exact 42"
cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/baseline-llvm.out" ||
    fail "LLVM output is not exact 42"

for permutation in routine-order-swap specialization-order-swap \
    combined-order-swap specialization-owner-renumber; do
    project "$WORK_DIR/$permutation.json" c "$WORK_DIR/$permutation.c"
    project "$WORK_DIR/$permutation.json" llvm "$WORK_DIR/$permutation.ll"
    cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/$permutation.c" ||
        fail "C artifact drifted for $permutation"
    cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/$permutation.ll" ||
        fail "LLVM artifact drifted for $permutation"
done

for mutation in \
    missing-generic-formal duplicate-generic-formal generic-param-drift \
    generic-param-abi-drift generic-return-drift generic-receiver-drift \
    generic-body-drift generic-return-abi-drift missing-specialization \
    extra-specialization duplicate-specialization-coordinate \
    specialization-lane-drift specialization-owner-id-disagreement \
    specialization-target-drift specialization-owner-drift \
    specialization-callable-drift specialization-formal-drift \
    specialization-actual-drift specialization-symbol-drift \
    graph-call-target-drift flattened-generic-call call-argument-edge-drift \
    literal-kind-drift missing-pair-receipt repaired-pair-offset \
    pair-field-order pair-field-type stale-output-use missing-output-use \
    output-member-drift unreachable-main; do
    reject_mutation "$mutation"
done
reject_mutation specialization-symbol-drift llvm
echo "[$LABEL] PASS: one MIR, inferred Identity<Int> C/LLVM calls, exact 42, four metamorphics, 32 negatives"
