#!/usr/bin/env bash
# One source MIR drives C and LLVM for Option<Pair> aggregate value flow.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-option-struct-value-flow"
DRIVER_BIN="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_OPTION_STRUCT_VALUE_FLOW_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_DIR="${PGY_SELFHOST_OPTION_STRUCT_VALUE_FLOW_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_option_struct_value_flow}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/option_struct_value_flow.pgy"
MIR="$WORK_DIR/option-struct.one.mir.json"
NATIVE_MIR="$WORK_DIR/option-struct.native.mir.json"
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
$ROOT_DIR/src/self_hosted/compiler/direct_mir_two_routine_nominal_classification_owner.pgy|150
$ROOT_DIR/src/self_hosted/compiler/direct_mir_two_routine_nominal_program_admission_owner.pgy|80
$ROOT_DIR/src/self_hosted/compiler/direct_mir_two_routine_nominal_projection_owner.pgy|55
$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_struct_value_flow_program_admission_owner.pgy|30
$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_struct_value_flow_program_identity_owner.pgy|230
$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_struct_value_flow_graph_shape_owner.pgy|140
$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_struct_value_flow_graph_fact_owner.pgy|180
$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_struct_value_flow_instruction_admission_owner.pgy|280
$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_struct_value_flow_abi_fact_owner.pgy|220
$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_struct_value_flow_plan_owner.pgy|180
$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_struct_value_flow_c_emission_owner.pgy|190
$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_struct_value_flow_llvm_emission_owner.pgy|220
$ROOT_DIR/src/self_hosted/compiler/direct_mir_option_struct_value_flow_projection_owner.pgy|40
EOF
    [[ "$total" -le 1800 ]] ||
        fail "Option nominal value-flow owner family cap exceeded: $total/1800"
    lines="$(wc -l <"$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy")"
    [[ "$lines" -le 80 ]] || fail "multi-routine root cap exceeded: $lines/80"
    grep -Fq 'DirectMirTwoRoutineNominalProgramCandidate(admitted)' \
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy" ||
        fail "classified nominal dispatch is not routed"
    ! grep -Fq 'DirectMirStructValueFlowProgramCandidate' \
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy" ||
        fail "broad plain-struct retry returned"
    for emitter in \
        direct_mir_option_struct_value_flow_c_emission_owner.pgy \
        direct_mir_option_struct_value_flow_llvm_emission_owner.pgy; do
        ! grep -Eq 'MirMachineLayerAdmittedJsonInput|MirExpressionGraphSequence|JsonObjectFact' \
            "$ROOT_DIR/src/self_hosted/compiler/$emitter" ||
            fail "$emitter reopened admitted JSON or expression graphs"
    done
}

project() {
    local input="$1" target="$2" output="$3"
    rm -f "$output" "$output.stdout" "$output.stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
        "$(root_relative "$input")" -o "$(root_relative "$output")" \
        >"$output.stdout" 2>"$output.stderr") ||
        { cat "$output.stdout" "$output.stderr" >&2 || true; fail "$target rejected admitted Option nominal MIR"; }
    [[ -s "$output" ]] || fail "$target emitted no Option nominal artifact"
}

reject_mutation() {
    local name="$1" target="${2:-c}" output
    output="$WORK_DIR/$name.$target.artifact"
    rm -f "$output" "$output.stdout" "$output.stderr"
    if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
        "$(root_relative "$WORK_DIR/$name.json")" -o "$(root_relative "$output")" \
        >"$output.stdout" 2>"$output.stderr"); then
        fail "$target accepted Option nominal mutation: $name"
    fi
    [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
    grep -Eq 'CODEGEN ERROR|MIR .*invalid|Option nominal|two-routine nominal' \
        "$output.stdout" "$output.stderr" ||
        fail "$target rejection lost its owned diagnostic: $name"
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
    "$(root_relative "$SOURCE")" -o "$(root_relative "$MIR")") ||
    fail "source-to-MIR producer rejected Option nominal fixture"
mir_digest="$(hash_file "$MIR")"
(cd "$ROOT_DIR" && "$PGY" --mir-json \
    "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$NATIVE_MIR") ||
    fail "native MIR oracle rejected Option nominal fixture"
"$PYTHON_BIN" \
    "$ROOT_DIR/tests/self_hosted/parity/one_mir_option_struct_value_flow_mutations.py" \
    "$MIR" "$NATIVE_MIR" compare ||
    fail "native/self Option nominal ABI parity drifted"
"$PYTHON_BIN" \
    "$ROOT_DIR/tests/self_hosted/parity/one_mir_option_struct_value_flow_mutations.py" \
    "$MIR" "$WORK_DIR"

project "$MIR" c "$WORK_DIR/baseline.c"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "C projection mutated MIR"
project "$MIR" llvm "$WORK_DIR/baseline.ll"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "LLVM projection mutated MIR"
grep -Fq 'static Option_Pair BuildPair(int32_t base)' "$WORK_DIR/baseline.c" ||
    fail "C flattened Option<Pair> producer"
grep -Fq 'define internal %Option_Pair @BuildPair(i32 %base)' \
    "$WORK_DIR/baseline.ll" || fail "LLVM flattened Option<Pair> producer"
grep -Fq '%pgy.chained.payload = extractvalue %Option_Pair %pgy.built, 1' \
    "$WORK_DIR/baseline.ll" || fail "LLVM flattened chained built unwrap"
! grep -Eq 'pgy_option_|pgy_runtime_|UnwrapOption|Some\(|None\(' \
    "$WORK_DIR/baseline.c" "$WORK_DIR/baseline.ll" ||
    fail "generated artifact reopened an Option runtime path"

"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/baseline.c" \
    -o "$WORK_DIR/baseline-c.exe"
"$CLANG" "$WORK_DIR/baseline.ll" -o "$WORK_DIR/baseline-llvm.exe" \
    2>"$WORK_DIR/llvm.compile.log"
printf '7\n11\n5\n' >"$WORK_DIR/expected.out"
"$WORK_DIR/baseline-c.exe" | tr -d '\r' >"$WORK_DIR/baseline-c.out"
"$WORK_DIR/baseline-llvm.exe" | tr -d '\r' >"$WORK_DIR/baseline-llvm.out"
cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/baseline-c.out" ||
    fail "C output is not exact 7/11/5"
cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/baseline-llvm.out" ||
    fail "LLVM output is not exact 7/11/5"

project "$WORK_DIR/routine-order-swap.json" c "$WORK_DIR/routine-order-swap.c"
project "$WORK_DIR/routine-order-swap.json" llvm "$WORK_DIR/routine-order-swap.ll"
cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/routine-order-swap.c" ||
    fail "C routine order drifted"
cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/routine-order-swap.ll" ||
    fail "LLVM routine order drifted"
for mutation in \
    missing-return-receipt missing-initial-receipt missing-none-receipt \
    missing-latest-receipt missing-built-receipt missing-pair-receipt \
    missing-unwrapped-receipt repaired-option-tag-geometry \
    repaired-option-payload-offset repaired-option-tags \
    repaired-pair-offset pair-field-order pair-field-type \
    malformed-producer-some stale-picked-use producer-call-unresolved \
    stale-built-use flattened-chained-use unsupported-return; do
    reject_mutation "$mutation"
done
reject_mutation repaired-option-tags llvm
echo "[$LABEL] PASS: one MIR, Option<Pair> ABI cross-seal, C/LLVM exact 7/11/5, routine permutation, 20 negatives"
