#!/usr/bin/env bash
# One self MIR carries Option<Int> through two heterogeneous member specializations.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_pipeline_step_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-constructed-generic-member"
DRIVER_BIN="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_CONSTRUCTED_GENERIC_MEMBER_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_DIR="${PGY_SELFHOST_CONSTRUCTED_GENERIC_MEMBER_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_constructed_generic_member}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/generic_member_constructed_return_flow.pgy"
MIR="$WORK_DIR/constructed-generic-member.one.mir.json"
NATIVE_MIR="$WORK_DIR/constructed-generic-member.native.mir.json"
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
$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_classification_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_classification_fact_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_constructed_member_shape_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_projection_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_program_admission_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_declaration_fact_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_signature_fact_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_specialization_fact_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_substitution_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_method_graph_fact_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_main_graph_fact_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_program_identity_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_method_instruction_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_main_instruction_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_option_abi_admission_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_representation_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_plan_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_c_emission_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_llvm_emission_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_projection_owner.pgy
EOF
    [[ "$total" -le 2400 ]] || fail "constructed generic member owner family cap exceeded: $total/2400"
    grep -Fq 'DirectMirThreeRoutineConstructedGenericMember()' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_classification_owner.pgy" || fail "three-routine classification is not owned"
    grep -Fq 'CompileAdmittedDirectMirConstructedGenericMember(' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_projection_owner.pgy" || fail "constructed member projection is not routed"
    for emitter in direct_mir_constructed_generic_member_c_emission_owner.pgy direct_mir_constructed_generic_member_llvm_emission_owner.pgy; do
        ! grep -Eq 'MirMachineLayerAdmittedJsonInput|MirExpressionGraphSequence|JsonObjectFact' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter reopened admitted MIR"
        ! grep -Fq 'CompilerSymbolCGenericSpecializationName' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter reconstructed specialization symbols"
    done
}

project() {
    local input="$1" target="$2" output="$3"
    rm -f "$output" "$output.stdout" "$output.stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$input")" -o "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr") || {
        cat "$output.stdout" "$output.stderr" >&2 || true
        fail "$target rejected constructed generic member MIR"
    }
    [[ -s "$output" ]] || fail "$target emitted no constructed generic member artifact"
}

reject_mutation() {
    local name="$1" target="${2:-c}" output
    output="$WORK_DIR/$name.$target.artifact"
    rm -f "$output" "$output.stdout" "$output.stderr"
    if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$WORK_DIR/$name.json")" -o "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr"); then
        fail "$target accepted constructed generic member mutation: $name"
    fi
    [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
    grep -Eq 'CODEGEN ERROR|MIR .*invalid|constructed generic member|three-routine' "$output.stdout" "$output.stderr" || fail "$target rejection lost its owned diagnostic: $name"
    ! grep -Eq 'Array argument|Array return|struct argument|generic nominal|generic struct|inferred generic member envelope|two-routine' "$output.stdout" "$output.stderr" || fail "$target retried another interpretation: $name"
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
# Produce source MIR once. Every target, permutation, and falsifier derives from it.
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified "$(root_relative "$SOURCE")" -o "$(root_relative "$MIR")") || fail "source-to-MIR rejected constructed member fixture"
mir_digest="$(hash_file "$MIR")"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$NATIVE_MIR") || fail "native MIR oracle rejected constructed member fixture"
pgy_selfhost_driver_rung2_canonicalize 0 "$DRIVER_BIN" --canonicalize-mir-json "$(root_relative "$MIR")" "$WORK_DIR/self.canonical.json"
pgy_selfhost_driver_rung2_canonicalize 0 "$DRIVER_BIN" --canonicalize-oracle-mir-json "$(root_relative "$NATIVE_MIR")" "$WORK_DIR/native.canonical.json"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_constructed_generic_member_mutations.py" "$MIR" "$NATIVE_MIR" compare "$WORK_DIR/self.canonical.json" "$WORK_DIR/native.canonical.json" || fail "native/self constructed member semantic parity drifted"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_constructed_generic_member_mutations.py" "$MIR" "$WORK_DIR"

project "$MIR" c "$WORK_DIR/baseline.c"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "C projection mutated MIR"
project "$MIR" llvm "$WORK_DIR/baseline.ll"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "LLVM projection mutated MIR"
[[ "$(grep -Fo 'Wrapper_Wrap_Int(' "$WORK_DIR/baseline.c" | wc -l)" -eq 2 ]] || fail "C lost Wrap definition/call"
[[ "$(grep -Fo 'Wrapper_Echo_Option_Int_(' "$WORK_DIR/baseline.c" | wc -l)" -eq 2 ]] || fail "C lost Echo definition/call"
grep -Fq 'pgy_direct_option_int _pgy_wrapped_0 = {0, value};' "$WORK_DIR/baseline.c" || fail "C flattened Some construction"
grep -Fq 'Wrapper_Echo_Option_Int_(_pgy_receiver_0, _pgy_inner_option_0)' "$WORK_DIR/baseline.c" || fail "C flattened inner-to-outer flow"
grep -Fq 'if (_pgy_result_option_0.tag != 0)' "$WORK_DIR/baseline.c" || fail "C lost checked unwrap"
grep -Fq '%inner = call { i32, i32 } @Wrapper_Wrap_Int(%Wrapper %receiver, i32 43)' "$WORK_DIR/baseline.ll" || fail "LLVM lost inner member call"
grep -Fq '%result = call { i32, i32 } @Wrapper_Echo_Option_Int_(%Wrapper %receiver, { i32, i32 } %inner)' "$WORK_DIR/baseline.ll" || fail "LLVM flattened inner-to-outer flow"
grep -Fq '%tag = extractvalue { i32, i32 } %result, 0' "$WORK_DIR/baseline.ll" || fail "LLVM lost Option tag extraction"
grep -Fq 'br i1 %is_some, label %unwrap.ok, label %unwrap.none' "$WORK_DIR/baseline.ll" || fail "LLVM lost checked unwrap branch"
! grep -Fq '@pgy_' "$WORK_DIR/baseline.ll" || fail "LLVM reintroduced runtime projection"
compile_and_expect baseline 43

for permutation in routine-order-reverse routine-order-rotate source-local-order-swap specialization-order-swap declaration-method-order-swap combined-order specialization-owner-renumber generic-formal-rename; do
    project "$WORK_DIR/$permutation.json" c "$WORK_DIR/$permutation.c"
    project "$WORK_DIR/$permutation.json" llvm "$WORK_DIR/$permutation.ll"
    cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/$permutation.c" || fail "C artifact drifted for $permutation"
    cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/$permutation.ll" || fail "LLVM artifact drifted for $permutation"
done
for variant in semantic-rename marker-value-seven argument-value-seventy-three collision-names field-local-same-name; do
    project "$WORK_DIR/$variant.json" c "$WORK_DIR/$variant.c"
    project "$WORK_DIR/$variant.json" llvm "$WORK_DIR/$variant.ll"
done
compile_and_expect semantic-rename 43
compile_and_expect marker-value-seven 43
compile_and_expect argument-value-seventy-three 73
compile_and_expect collision-names 43
compile_and_expect field-local-same-name 43
cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/collision-names.c" || fail "C source local spelling leaked into backend temporaries"
cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/collision-names.ll" || fail "LLVM source local spelling leaked into backend temporaries"
cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/marker-value-seven.c" && fail "C erased receiver state change"
cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/marker-value-seven.ll" && fail "LLVM erased receiver state change"

for mutation in declaration-kind-drift nominal-kind-drift missing-method method-kind-drift method-contract-drift field-type-drift field-kind-drift wrap-return-drift echo-return-drift missing-generic-formal receiver-carriage-drift value-param-type-drift method-return-abi-drift missing-specialization extra-specialization duplicate-specialization-ordinal specialization-owner-disagreement specialization-lane-drift specialization-target-drift outer-actual-drift inner-symbol-drift specialization-formal-drift specialization-scalar-tail some-target-drift some-edge-drift identity-body-drift constructor-target-drift inner-target-drift outer-target-drift inner-edge-drift outer-edge-drift unwrap-target-drift unwrap-edge-drift stale-output-use result-type-drift result-abi-not-required result-layout-drift constructor-physical-layout unreachable-wrap unreachable-main; do reject_mutation "$mutation"; done
for mutation in inner-symbol-drift some-target-drift outer-target-drift result-layout-drift constructor-physical-layout; do reject_mutation "$mutation" llvm; done
echo "[$LABEL] PASS: one MIR, two heterogeneous member specializations, C/LLVM exact 43, eight invariants, five variants, 40 C negatives, 5 LLVM sentinels"
