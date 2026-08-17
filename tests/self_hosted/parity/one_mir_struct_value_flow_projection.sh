#!/usr/bin/env bash
# One source MIR drives C and LLVM for Pair return, reassignment, and use.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-struct-value-flow"
DRIVER_BIN="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_STRUCT_VALUE_FLOW_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_DIR="${PGY_SELFHOST_STRUCT_VALUE_FLOW_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_struct_value_flow}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/struct_literal_value_flow.pgy"
MIR="$WORK_DIR/value_flow.one.mir.json"
NATIVE_MIR="$WORK_DIR/value_flow.native.mir.json"
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
        require_file "$owner"; lines="$(wc -l < "$owner")"
        [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: ${owner#"$ROOT_DIR/"}=$lines/$cap"
        total=$((total + lines))
    done <<EOF
$ROOT_DIR/src/self_hosted/compiler/direct_mir_nominal_abi_row_equality_owner.pgy|40
$ROOT_DIR/src/self_hosted/compiler/direct_mir_nominal_declaration_abi_fact_owner.pgy|220
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_value_flow_program_admission_owner.pgy|95
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_value_flow_program_identity_owner.pgy|230
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_value_flow_graph_fact_owner.pgy|260
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_value_flow_instruction_admission_owner.pgy|200
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_value_flow_abi_fact_owner.pgy|180
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_value_flow_plan_owner.pgy|230
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_value_flow_c_emission_owner.pgy|180
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_value_flow_llvm_emission_owner.pgy|140
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_value_flow_projection_owner.pgy|40
$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy|110
EOF
    [[ "$total" -le 1560 ]] || fail "value-flow owner family cap exceeded: $total/1560"
    grep -Fq 'JsonArrayObjectFactCount(admitted.document.declarations) == 0' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_return_program_identity_owner.pgy" || fail "Array return still claims nominal programs"
    grep -Fq 'DirectMirTwoRoutineNominalProgramCandidate(admitted)' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy" || fail "classified nominal candidate is not routed"
}

project() {
    local input="$1" target="$2" output="$3"
    rm -f "$output" "$output.stdout" "$output.stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$input")" -o "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr") || { cat "$output.stdout" "$output.stderr" >&2 || true; fail "$target rejected admitted value-flow MIR"; }
    [[ -s "$output" ]] || fail "$target emitted no value-flow artifact"
}

reject_mutation() {
    local name="$1" target output
    for target in c llvm; do
        output="$WORK_DIR/$name.$target.artifact"; rm -f "$output" "$output.stdout" "$output.stderr"
        if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$WORK_DIR/$name.json")" -o "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr"); then fail "$target accepted value-flow mutation: $name"; fi
        [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
        grep -Eq 'CODEGEN ERROR|MIR .*invalid|direct MIR struct value-flow' "$output.stdout" "$output.stderr" || fail "$target rejection did not stay fail-closed: $name"
    done
}

for path in "$SOURCE" "$DRIVER_BIN" "$PGY"; do require_file "$path"; done
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "missing C compiler"
command -v "$CLANG" >/dev/null || fail "missing LLVM compiler"
assert_owner_ratchet
mkdir -p "$WORK_DIR"; rm -f "$MIR" "$NATIVE_MIR"
# The gate must produce source MIR exactly once; all projections reuse it.
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified "$(root_relative "$SOURCE")" -o "$(root_relative "$MIR")") || fail "source-to-MIR producer rejected value-flow fixture"
mir_digest="$(hash_file "$MIR")"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$NATIVE_MIR") || fail "native MIR oracle rejected value-flow fixture"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_struct_value_flow_mutations.py" "$MIR" "$NATIVE_MIR" compare || fail "native/self semantic-slot ABI parity drifted"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_struct_value_flow_mutations.py" "$MIR" "$WORK_DIR"
project "$MIR" c "$WORK_DIR/baseline.c"; [[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "C projection mutated MIR"
project "$MIR" llvm "$WORK_DIR/baseline.ll"; [[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "LLVM projection mutated MIR"
grep -Fq 'static Pair BuildPair(int32_t base)' "$WORK_DIR/baseline.c" || fail "C flattened Pair producer"
grep -Fq 'BuildPair(pgy_latest.right)' "$WORK_DIR/baseline.c" || fail "C lost latest Pair call"
grep -Fq 'define internal %Pair @BuildPair(i32 %base)' "$WORK_DIR/baseline.ll" || fail "LLVM flattened Pair producer"
grep -Fq '%pgy.built = call %Pair @BuildPair(i32 %pgy.argument)' "$WORK_DIR/baseline.ll" || fail "LLVM lost aggregate return call"
"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/baseline.c" -o "$WORK_DIR/baseline-c.exe"
"$CLANG" "$WORK_DIR/baseline.ll" -o "$WORK_DIR/baseline-llvm.exe" 2>"$WORK_DIR/llvm.compile.log"
[[ "$("$WORK_DIR/baseline-c.exe")" == "11" ]] || fail "C output is not 11"
[[ "$("$WORK_DIR/baseline-llvm.exe")" == "11" ]] || fail "LLVM output is not 11"
project "$WORK_DIR/routine-order-swap.json" c "$WORK_DIR/routine-order-swap.c"
project "$WORK_DIR/routine-order-swap.json" llvm "$WORK_DIR/routine-order-swap.ll"
cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/routine-order-swap.c" || fail "C routine order drifted"
cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/routine-order-swap.ll" || fail "LLVM routine order drifted"
for mutation in missing-return-receipt wrong-return-receipt-id missing-initial-receipt missing-assignment-receipt missing-result-receipt stale-latest-use repaired-all-layout-offsets producer-call-unresolved producer-call-member result-member-path pair-field-order pair-field-type extra-call-target; do reject_mutation "$mutation"; done
echo "[$LABEL] PASS: one MIR, nominal return/local ABI cross-seal, C/LLVM exact 11, routine permutation, 13 negatives"
