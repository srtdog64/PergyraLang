#!/usr/bin/env bash
# One self MIR carries a caller-owned Array<Int> through nested member calls.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-constructed-array-member"
DRIVER_BIN="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_CONSTRUCTED_ARRAY_MEMBER_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_DIR="${PGY_SELFHOST_CONSTRUCTED_ARRAY_MEMBER_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_constructed_array_member}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/generic_member_array_return_flow.pgy"
MIR="$WORK_DIR/constructed-array-member.one.mir.json"
NATIVE_MIR="$WORK_DIR/constructed-array-member.native.mir.json"
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
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_member_specialization_pair_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_member_variant_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_specialization_fact_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_substitution_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_signature_fact_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_method_graph_fact_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_main_graph_fact_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_program_identity_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_method_instruction_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_main_instruction_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_abi_admission_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_representation_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_plan_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_c_emission_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_llvm_emission_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_projection_owner.pgy
$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_projection_owner.pgy
EOF
    [[ "$total" -le 2160 ]] || fail "constructed Array member family cap exceeded: $total/2160"
    for shared_owner in direct_mir_array_storage_layout_contract_owner.pgy direct_mir_array_storage_abi_projection_owner.pgy direct_mir_array_storage_symbol_owner.pgy direct_mir_array_storage_c_assertion_owner.pgy; do
        require_file "$ROOT_DIR/src/self_hosted/compiler/$shared_owner"
        [[ "$(wc -l <"$ROOT_DIR/src/self_hosted/compiler/$shared_owner")" -le 140 ]] || fail "shared Array storage owner hard cap exceeded: $shared_owner"
    done
    grep -Fq 'DirectMirConstructedMemberVariantFromPair(pair)' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_projection_owner.pgy" || fail "one-shot family classification is not routed"
    grep -Fq 'CompileAdmittedDirectMirConstructedArrayMember(' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_generic_member_projection_owner.pgy" || fail "Array member projection is not routed"
    for projection in direct_mir_constructed_generic_member_specialization_fact_owner.pgy direct_mir_constructed_array_member_specialization_fact_owner.pgy; do
        ! grep -Eq 'JsonObjectFactArrayObjectTable|FromDocument' "$ROOT_DIR/src/self_hosted/compiler/$projection" || fail "$projection reopened the specialization wire"
    done
    for emitter in direct_mir_constructed_array_member_c_emission_owner.pgy direct_mir_constructed_array_member_llvm_emission_owner.pgy; do
        ! grep -Eq 'MirMachineLayerAdmittedJsonInput|MirExpressionGraphSequence|JsonObjectFact' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter reopened admitted MIR"
        ! grep -Fq 'CompilerSymbolCGenericSpecializationName' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter reconstructed specialization symbols"
    done
    grep -Fq 'direct_mir_array_storage_abi_projection_owner.pgy' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_int_abi_projection_owner.pgy" || fail "Array<Int> target ABI bypassed the storage projection owner"
    ! grep -Fq 'direct_mir_array_storage_layout_contract_owner.pgy' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_int_abi_projection_owner.pgy" || fail "Array<Int> target ABI reopened the storage layout owner"
    grep -Fq 'DirectMirClosedModuleCallAbiFactReady(plan.call_abi)' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_constructed_array_member_plan_owner.pgy" || fail "Array<Int> plan lacks the closed-module call receipt"
}

project() {
    local input="$1" target="$2" output="$3"
    rm -f "$output" "$output.stdout" "$output.stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$input")" "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr") || {
        cat "$output.stdout" "$output.stderr" >&2 || true
        fail "$target rejected constructed Array member MIR"
    }
    [[ -s "$output" ]] || fail "$target emitted no constructed Array artifact"
}

reject_mutation() {
    local name="$1" target="${2:-c}" output
    output="$WORK_DIR/$name.$target.artifact"
    rm -f "$output" "$output.stdout" "$output.stderr"
    if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$WORK_DIR/$name.json")" "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr"); then
        fail "$target accepted constructed Array member mutation: $name"
    fi
    [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
    grep -Eq 'CODEGEN ERROR|MIR .*invalid|constructed.*member|three-routine' "$output.stdout" "$output.stderr" || fail "$target rejection lost its owned diagnostic: $name"
    ! grep -Eq 'Array argument|Array return|struct argument|generic nominal|generic struct|two-routine' "$output.stdout" "$output.stderr" || fail "$target retried another interpretation: $name"
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
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified "$(root_relative "$SOURCE")" "$(root_relative "$MIR")") || fail "source-to-MIR rejected constructed Array member fixture"
mir_digest="$(hash_file "$MIR")"
(cd "$ROOT_DIR" && "$PGY" --mir-json "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$NATIVE_MIR") || fail "native MIR oracle rejected constructed Array member fixture"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_constructed_array_member_mutations.py" "$MIR" "$NATIVE_MIR" compare || fail "native/self constructed Array semantics drifted"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_constructed_array_member_mutations.py" "$MIR" "$WORK_DIR"

project "$MIR" c "$WORK_DIR/baseline.c"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "C projection mutated MIR"
project "$MIR" llvm "$WORK_DIR/baseline.ll"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "LLVM projection mutated MIR"
grep -Fq 'ArrayWrapper_Wrap_Int(ArrayWrapper self, int32_t value, int32_t *_pgy_array_storage_0)' "$WORK_DIR/baseline.c" || fail "C lost collision-proof caller-storage Wrap ABI"
grep -Fq 'pgy_ai ArrayWrapper_Echo_Array_Int_(ArrayWrapper self, pgy_ai value)' "$WORK_DIR/baseline.c" || fail "C lost by-value Echo ABI"
[[ "$(grep -Fc 'int32_t _pgy_array_storage_0[1];' "$WORK_DIR/baseline.c")" -eq 1 ]] || fail "C storage owner is not unique"
! grep -Eq '(^|[^A-Za-z0-9_])__[A-Za-z0-9_]' "$WORK_DIR/baseline.c" || fail "C emitted a reserved double-underscore identifier"
grep -Fq '_Static_assert(sizeof(pgy_ai) == 32' "$WORK_DIR/baseline.c" || fail "C lost Array storage size receipt"
grep -Fq 'offsetof(pgy_ai, allocator) == 24' "$WORK_DIR/baseline.c" || fail "C lost Array allocator offset receipt"
grep -Fq 'ArrayWrapper_Echo_Array_Int_(pgy_receiver, pgy_inner)' "$WORK_DIR/baseline.c" || fail "C flattened nested member flow"
! grep -Eq 'pgy_array_new_Int|malloc|free' "$WORK_DIR/baseline.c" || fail "C reintroduced runtime allocation"
[[ "$(grep -Fc 'alloca [1 x i32]' "$WORK_DIR/baseline.ll")" -eq 1 ]] || fail "LLVM storage owner is not unique"
grep -Fq '@ArrayWrapper_Wrap_Int(%ArrayWrapper %receiver, i32 44, ptr %array.data)' "$WORK_DIR/baseline.ll" || fail "LLVM lost hidden caller storage"
grep -Fq '@ArrayWrapper_Echo_Array_Int_(%ArrayWrapper %receiver, { ptr, i64, i64, ptr } %inner)' "$WORK_DIR/baseline.ll" || fail "LLVM flattened nested aggregate flow"
grep -Fq '%result.data = extractvalue { ptr, i64, i64, ptr } %result, 0' "$WORK_DIR/baseline.ll" || fail "LLVM lost Array data extraction"
! grep -Eq '@pgy_|malloc|free' "$WORK_DIR/baseline.ll" || fail "LLVM reintroduced runtime allocation"
compile_and_expect baseline 44

for permutation in routine-order-reverse specialization-order-swap declaration-method-order-swap source-local-order-swap combined-order generic-formal-rename; do
    project "$WORK_DIR/$permutation.json" c "$WORK_DIR/$permutation.c"
    project "$WORK_DIR/$permutation.json" llvm "$WORK_DIR/$permutation.ll"
    cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/$permutation.c" || fail "C artifact drifted for $permutation"
    cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/$permutation.ll" || fail "LLVM artifact drifted for $permutation"
done
for variant in argument-value-seventy-three collision-names hidden-storage-collision; do
    project "$WORK_DIR/$variant.json" c "$WORK_DIR/$variant.c"
    project "$WORK_DIR/$variant.json" llvm "$WORK_DIR/$variant.ll"
done
compile_and_expect argument-value-seventy-three 73
compile_and_expect collision-names 44
compile_and_expect hidden-storage-collision 44
grep -Fq 'int32_t *_pgy_array_storage_1)' "$WORK_DIR/hidden-storage-collision.c" || fail "C storage symbol did not avoid the source parameter"
cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/collision-names.c" || fail "C source local spelling leaked"
cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/collision-names.ll" || fail "LLVM source local spelling leaked"

for mutation in declaration-kind-drift missing-method field-type-drift wrap-return-drift echo-return-drift receiver-carriage-drift missing-specialization duplicate-specialization-ordinal specialization-lane-drift specialization-target-drift outer-actual-drift inner-symbol-drift array-element-edge-drift identity-body-drift inner-target-drift outer-edge-drift index-value-drift stale-output-use result-type-drift result-abi-not-required result-layout-offset-drift result-runtime-drift result-discriminant-drift result-tag-drift result-niche-drift constructor-physical-layout unreachable-wrap; do reject_mutation "$mutation"; done
for mutation in outer-actual-drift inner-symbol-drift inner-target-drift result-layout-offset-drift result-runtime-drift result-tag-drift constructor-physical-layout; do reject_mutation "$mutation" llvm; done
echo "[$LABEL] PASS: one MIR, caller-owned Array through two member specializations, C/LLVM exact 44, six invariants, three variants, 27 C negatives, 7 LLVM sentinels"
