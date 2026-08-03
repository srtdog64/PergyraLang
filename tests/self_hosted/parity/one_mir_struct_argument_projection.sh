#!/usr/bin/env bash
# One source-produced MIR graph drives runtime-free C and LLVM for a nested
# value struct passed by value.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-struct-argument"
DRIVER_BIN="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_STRUCT_ARGUMENT_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_DIR="${PGY_SELFHOST_STRUCT_ARGUMENT_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_struct_argument}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/struct_literal_call_argument.pgy"
MIR="$WORK_DIR/struct_argument.one.mir.json"
NATIVE_MIR="$WORK_DIR/struct_argument.native.mir.json"
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
        require_file "$owner"
        lines="$(wc -l < "$owner")"
        [[ "$lines" -le "$cap" ]] ||
            fail "owner hard cap exceeded: ${owner#"$ROOT_DIR/"}=$lines/$cap"
        total=$((total + lines))
    done <<EOF
$ROOT_DIR/src/self_hosted/compiler/direct_mir_nominal_declaration_abi_fact_owner.pgy|220
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_argument_graph_fact_owner.pgy|260
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_argument_program_admission_owner.pgy|95
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_argument_program_identity_owner.pgy|230
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_argument_plan_owner.pgy|215
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_argument_c_emission_owner.pgy|165
$ROOT_DIR/src/self_hosted/compiler/direct_mir_struct_argument_llvm_emission_owner.pgy|110
$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy|90
EOF
    [[ "$total" -le 1385 ]] || fail "struct owner family cap exceeded: $total/1385"
    grep -Fq 'CompileAdmittedDirectMirThreeRoutine(' \
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy" ||
        fail "multi-routine root does not route one three-routine decision"
    grep -Fq 'classification.kind == DirectMirThreeRoutineStructArgument()' \
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_projection_owner.pgy" ||
        fail "three-routine owner does not classify struct argument MIR"
    grep -Fq 'JsonArrayObjectFactCount(admitted.document.declarations) == 0' \
        "$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_argument_program_identity_owner.pgy" ||
        fail "Array and struct candidates are not disjoint"
}

project() {
    local input="$1" target="$2" output="$3" stdout="$3.stdout" stderr="$3.stderr"
    rm -f "$output" "$stdout" "$stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
        "$(root_relative "$input")" -o "$(root_relative "$output")" \
        >"$stdout" 2>"$stderr") || {
        cat "$stdout" "$stderr" >&2 || true
        fail "$target rejected admitted struct argument MIR"
    }
    [[ -s "$output" ]] || fail "$target emitted no struct artifact"
}

reject_mutation() {
    local name="$1" target output
    for target in c llvm; do
        output="$WORK_DIR/$name.$target.artifact"
        rm -f "$output" "$output.stdout" "$output.stderr"
        if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
            "$(root_relative "$WORK_DIR/$name.json")" -o \
            "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr"); then
            fail "$target accepted struct mutation: $name"
        fi
        [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
        grep -Eq 'CODEGEN ERROR|MIR .*invalid|direct MIR struct' \
            "$output.stdout" "$output.stderr" || {
            cat "$output.stdout" "$output.stderr" >&2 || true
            fail "$target rejection did not stay fail-closed: $name"
        }
    done
}

require_file "$SOURCE"
require_file "$DRIVER_BIN"
require_file "$PGY"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "missing C compiler"
command -v "$CLANG" >/dev/null || fail "missing LLVM compiler"
assert_owner_ratchet
mkdir -p "$WORK_DIR"

rm -f "$MIR" "$NATIVE_MIR"
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified \
    "$(root_relative "$SOURCE")" -o "$(root_relative "$MIR")") ||
    fail "source-to-MIR producer rejected struct fixture"
mir_digest="$(hash_file "$MIR")"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
    "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$NATIVE_MIR") ||
    fail "native MIR oracle rejected struct fixture"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_struct_argument_mutations.py" \
    "$MIR" "$NATIVE_MIR" compare || fail "native/self struct ABI parity drifted"

"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_struct_argument_mutations.py" \
    "$MIR" "$WORK_DIR"
project "$MIR" c "$WORK_DIR/baseline.c"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "C projection mutated MIR"
project "$MIR" llvm "$WORK_DIR/baseline.ll"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "LLVM projection mutated MIR"

grep -Fq 'static int32_t Twice(int32_t value)' "$WORK_DIR/baseline.c" || fail "C flattened Twice"
grep -Fq 'static int32_t Width(Line line)' "$WORK_DIR/baseline.c" || fail "C flattened Width"
grep -Fq '_Static_assert(offsetof(Line, end) == 8' "$WORK_DIR/baseline.c" || fail "C lost layout receipt"
grep -Fq 'Twice(1)' "$WORK_DIR/baseline.c" || fail "C lost nested call"
grep -Fq 'Width(pgy_argument)' "$WORK_DIR/baseline.c" || fail "C lost struct call"
grep -Fq 'define internal i32 @Width(%Line %value)' "$WORK_DIR/baseline.ll" || fail "LLVM flattened Width"
grep -Fq 'call i32 @Twice(i32 1)' "$WORK_DIR/baseline.ll" || fail "LLVM lost nested call"
grep -Fq 'call i32 @Width(%Line %pgy.outer.1)' "$WORK_DIR/baseline.ll" || fail "LLVM lost struct call"

"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/baseline.c" -o "$WORK_DIR/baseline-c.exe"
"$CLANG" "$WORK_DIR/baseline.ll" -o "$WORK_DIR/baseline-llvm.exe" 2>"$WORK_DIR/llvm.compile.log"
[[ "$("$WORK_DIR/baseline-c.exe")" == "6" ]] || fail "C output is not 6"
[[ "$("$WORK_DIR/baseline-llvm.exe")" == "6" ]] || fail "LLVM output is not 6"

for permutation in routine-order-cycle declaration-order-cycle routine-declaration-order-cycle; do
    project "$WORK_DIR/$permutation.json" c "$WORK_DIR/$permutation.c"
    project "$WORK_DIR/$permutation.json" llvm "$WORK_DIR/$permutation.ll"
    cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/$permutation.c" || fail "C $permutation drifted"
    cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/$permutation.ll" || fail "LLVM $permutation drifted"
done

for mutation in declaration-id-duplicate missing-declaration-abi line-field-order \
    vec2-field-type repaired-line-layout-offset repaired-vec2-layout-offset \
    missing-line-param-abi line-param-receipt-crosswire line-param-carriage \
    line-param-pass width-call-unresolved twice-call-argument-edge \
    width-call-argument-edge width-member-path twice-operation; do
    reject_mutation "$mutation"
done

echo "[$LABEL] PASS: one MIR, nominal ABI cross-seal, C/LLVM exact 6, permutations, 15 negatives"
