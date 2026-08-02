#!/usr/bin/env bash
# One self MIR drives real C/LLVM generic specialization and nominal value flow.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-generic-struct-value-flow"
DRIVER_BIN="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_GENERIC_STRUCT_VALUE_FLOW_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_DIR="${PGY_SELFHOST_GENERIC_STRUCT_VALUE_FLOW_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_generic_struct_value_flow}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/generic_struct_field_value_flow.pgy"
MIR="$WORK_DIR/generic-struct.one.mir.json"
NATIVE_MIR="$WORK_DIR/generic-struct.native.mir.json"
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
$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_classification_owner.pgy|110
$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_projection_owner.pgy|80
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_routine_signature_fact_owner.pgy|180
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_specialization_fact_owner.pgy|200
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_struct_value_flow_program_admission_owner.pgy|100
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_struct_value_flow_program_identity_owner.pgy|250
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_struct_value_flow_graph_shape_owner.pgy|150
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_struct_value_flow_graph_fact_owner.pgy|180
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_struct_value_flow_instruction_admission_owner.pgy|240
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_struct_value_flow_abi_fact_owner.pgy|130
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_struct_value_flow_plan_owner.pgy|140
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_struct_value_flow_c_emission_owner.pgy|150
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_struct_value_flow_llvm_emission_owner.pgy|130
$ROOT_DIR/src/self_hosted/compiler/direct_mir_generic_struct_value_flow_projection_owner.pgy|40
EOF
    [[ "$total" -le 2000 ]] ||
        fail "generic nominal value-flow owner family cap exceeded: $total/2000"
    lines="$(wc -l <"$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy")"
    [[ "$lines" -le 80 ]] || fail "multi-routine root cap exceeded: $lines/80"
    grep -Fq 'CompileAdmittedDirectMirThreeRoutine(' \
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy" ||
        fail "three-routine classifier is not routed"
    ! grep -Eq 'DirectMir(Array|Struct)ArgumentProgramCandidate' \
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy" ||
        fail "multi-routine root restored candidate retries"
    for emitter in \
        direct_mir_generic_struct_value_flow_c_emission_owner.pgy \
        direct_mir_generic_struct_value_flow_llvm_emission_owner.pgy; do
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
        "$(root_relative "$input")" "$(root_relative "$output")" \
        >"$output.stdout" 2>"$output.stderr") ||
        { cat "$output.stdout" "$output.stderr" >&2 || true; fail "$target rejected admitted generic nominal MIR"; }
    [[ -s "$output" ]] || fail "$target emitted no generic nominal artifact"
}

reject_mutation() {
    local name="$1" target="${2:-c}" output
    output="$WORK_DIR/$name.$target.artifact"
    rm -f "$output" "$output.stdout" "$output.stderr"
    if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
        "$(root_relative "$WORK_DIR/$name.json")" "$(root_relative "$output")" \
        >"$output.stdout" 2>"$output.stderr"); then
        fail "$target accepted generic nominal mutation: $name"
    fi
    [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
    grep -Eq 'CODEGEN ERROR|MIR .*invalid|generic nominal|three-routine' \
        "$output.stdout" "$output.stderr" ||
        fail "$target rejection lost its owned diagnostic: $name"
    ! grep -Eq 'Array argument program envelope|struct argument program envelope' \
        "$output.stdout" "$output.stderr" ||
        fail "$target retried another three-routine interpretation: $name"
}

for path in "$SOURCE" "$DRIVER_BIN" "$PGY"; do require_file "$path"; done
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "missing C compiler"
command -v "$CLANG" >/dev/null || fail "missing LLVM compiler"
assert_owner_ratchet
mkdir -p "$WORK_DIR"
rm -f "$MIR" "$NATIVE_MIR"
# The gate must produce source MIR exactly once; both targets reuse that fact.
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified \
    "$(root_relative "$SOURCE")" "$(root_relative "$MIR")") ||
    fail "source-to-MIR producer rejected generic nominal fixture"
mir_digest="$(hash_file "$MIR")"
(cd "$ROOT_DIR" && "$PGY" --mir-json \
    "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$NATIVE_MIR") ||
    fail "native MIR oracle rejected generic nominal fixture"
"$PYTHON_BIN" \
    "$ROOT_DIR/tests/self_hosted/parity/one_mir_generic_struct_value_flow_mutations.py" \
    "$MIR" "$NATIVE_MIR" compare ||
    fail "native/self generic nominal graph or ABI parity drifted"
"$PYTHON_BIN" \
    "$ROOT_DIR/tests/self_hosted/parity/one_mir_generic_struct_value_flow_mutations.py" \
    "$MIR" "$WORK_DIR"

project "$MIR" c "$WORK_DIR/baseline.c"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "C projection mutated MIR"
project "$MIR" llvm "$WORK_DIR/baseline.ll"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "LLVM projection mutated MIR"
[[ "$(grep -Fo 'Identity_Int(' "$WORK_DIR/baseline.c" | wc -l)" -eq 5 ]] ||
    fail "C did not preserve one specialization definition and four calls"
[[ "$(grep -Fo '@Identity_Int(' "$WORK_DIR/baseline.ll" | wc -l)" -eq 5 ]] ||
    fail "LLVM did not preserve one specialization definition and four calls"
grep -Fq 'static Pair BuildPair(int32_t base)' "$WORK_DIR/baseline.c" ||
    fail "C flattened the nominal producer"
grep -Fq 'define internal %Pair @BuildPair(i32 %base)' "$WORK_DIR/baseline.ll" ||
    fail "LLVM flattened the nominal producer"
grep -Fq '%pgy.argument = extractvalue %Pair %pgy.initial.1, 1' \
    "$WORK_DIR/baseline.ll" || fail "LLVM lost pair.right argument flow"
! grep -Eq 'return[[:space:]]+7|pgy_sum[[:space:]]*=[[:space:]]*7' \
    "$WORK_DIR/baseline.c" "$WORK_DIR/baseline.ll" ||
    fail "generated artifact hard-coded the expected output"

"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/baseline.c" \
    -o "$WORK_DIR/baseline-c.exe"
"$CLANG" "$WORK_DIR/baseline.ll" -o "$WORK_DIR/baseline-llvm.exe" \
    2>"$WORK_DIR/llvm.compile.log"
printf '7\n' >"$WORK_DIR/expected.out"
"$WORK_DIR/baseline-c.exe" | tr -d '\r' >"$WORK_DIR/baseline-c.out"
"$WORK_DIR/baseline-llvm.exe" | tr -d '\r' >"$WORK_DIR/baseline-llvm.out"
cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/baseline-c.out" ||
    fail "C output is not exact 7"
cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/baseline-llvm.out" ||
    fail "LLVM output is not exact 7"

for permutation in routine-order-swap specialization-order-swap; do
    project "$WORK_DIR/$permutation.json" c "$WORK_DIR/$permutation.c"
    project "$WORK_DIR/$permutation.json" llvm "$WORK_DIR/$permutation.ll"
    cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/$permutation.c" ||
        fail "C artifact drifted for $permutation"
    cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/$permutation.ll" ||
        fail "LLVM artifact drifted for $permutation"
done

for mutation in \
    missing-generic-formal duplicate-generic-formal generic-param-drift \
    generic-return-drift generic-receiver-drift generic-body-drift \
    generic-return-abi-drift \
    missing-specialization duplicate-specialization-coordinate \
    specialization-lane-drift specialization-target-drift \
    specialization-owner-drift specialization-callable-drift \
    specialization-actual-drift specialization-symbol-drift \
    graph-actual-drift graph-call-target-drift flattened-generic-call \
    missing-return-receipt missing-initial-receipt missing-built-receipt \
    repaired-pair-offset pair-field-order pair-field-type stale-producer-use \
    stale-output-use producer-member-drift output-member-drift; do
    reject_mutation "$mutation"
done
reject_mutation specialization-symbol-drift llvm
echo "[$LABEL] PASS: one MIR, real Identity<Int> C/LLVM calls, exact 7, two permutations, 29 negatives"
