#!/usr/bin/env bash
# One source-produced three-routine MIR graph drives runtime-free C and LLVM.
# The caller owns Array<Int> backing storage and passes the aggregate by value.
set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 ||
    ! command -v tr >/dev/null 2>&1 ||
    ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-array-argument"
DRIVER_BIN="${PGY_SELFHOST_ARRAY_ARGUMENT_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
DRIVER_BIN="$(pgy_select_optional_exe_binary "$DRIVER_BIN")"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
WORK_DIR="${PGY_SELFHOST_ARRAY_ARGUMENT_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_array_argument}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/array_literal_call_argument.pgy"
MIR="$WORK_DIR/array_argument.one.mir.json"
NATIVE_MIR="$WORK_DIR/array_argument.native.mir.json"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_file() { [[ -f "$1" ]] || fail "missing file: ${1#"$ROOT_DIR/"}"; }
root_relative() { pgy_selfhost_path_relative_to_root "$1"; }
hash_file() { sha256sum "$1" | awk '{print $1}'; }

assert_owner_ratchet() {
    local param="$ROOT_DIR/src/self_hosted/compiler/direct_mir_routine_param_fact_owner.pgy"
    local signature="$ROOT_DIR/src/self_hosted/compiler/direct_mir_routine_signature_fact_owner.pgy"
    local graph="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_argument_graph_fact_owner.pgy"
    local identity="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_argument_program_identity_owner.pgy"
    local plan="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_argument_plan_owner.pgy"
    local emission="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_argument_emission_owner.pgy"
    local multi="$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy"
    local terminal="$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_terminal_projection_owner.pgy"
    local three="$ROOT_DIR/src/self_hosted/compiler/direct_mir_three_routine_projection_owner.pgy"
    local owner cap lines term
    while IFS='|' read -r owner cap; do
        require_file "$owner"
        lines="$(wc -l < "$owner")"
        [[ "$lines" -le "$cap" ]] ||
            fail "owner hard cap exceeded: ${owner#"$ROOT_DIR/"}=$lines/$cap"
    done <<EOF
$param|180
$signature|195
$graph|225
$identity|245
$plan|310
$emission|265
$multi|90
$terminal|35
EOF
    for term in BuildMirDocumentFactIndex CompileMirJsonToCVerified \
        GenerateCFromVerifiedSemanticArtifact llvm_codegen_ \
        driver_run_pipeline MirPhiParameterEntryExists; do
        ! grep -Fq -- "$term" "$param" "$signature" "$graph" \
            "$identity" "$plan" "$emission" "$multi" "$terminal" ||
            fail "Array argument owner reopened a forbidden path: $term"
    done
    grep -Fq 'MirCapturedRequiredAbiLayoutRowAdmission' "$param" ||
        fail "formal parameter ABI is not admitted from MIR"
    grep -Fq 'DirectMirArrayIntCapturedAbiReady' "$identity" ||
        fail "Array argument identity does not consume canonical ABI"
    grep -Fq 'DirectMirArrayArgumentPlanMutationRejected' "$plan" ||
        fail "Array argument plan lacks a repaired mutation falsifier"
    grep -Fq 'CompileAdmittedDirectMirThreeRoutine(' "$terminal" ||
        fail "terminal multi-routine owner does not route one three-routine decision"
    grep -Fq 'classification.kind == DirectMirThreeRoutineArrayArgument()' \
        "$three" || fail "three-routine owner does not classify Array argument MIR"
    ! grep -Fq '@pgy_' "$emission" ||
        fail "runtime-free Array argument emission references Pergyra runtime"
    local source_mir_flag="--emit-mir-json-verified"
    local source_mir_call='"$DRIVER_BIN" '
    source_mir_call="${source_mir_call}${source_mir_flag}"
    [[ "$(grep -Fo -- "$source_mir_call" "$0" | wc -l | tr -d ' ')" == 1 ]] ||
        fail "gate must produce source MIR exactly once"
}

project() {
    local input="$1" target="$2" output="$3" stdout stderr
    stdout="$output.stdout"
    stderr="$output.stderr"
    rm -f "$output" "$stdout" "$stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
        "$(root_relative "$input")" -o "$(root_relative "$output")" \
        >"$stdout" 2>"$stderr") || {
        cat "$stdout" "$stderr" >&2 || true
        fail "$target rejected admitted Array argument MIR"
    }
    [[ -s "$output" ]] || fail "$target emitted no Array argument artifact"
}

reject_mutation() {
    local name="$1" diagnostic="$2" target output stdout stderr
    for target in c llvm; do
        output="$WORK_DIR/$name.$target.artifact"
        stdout="$output.stdout"
        stderr="$output.stderr"
        rm -f "$output" "$stdout" "$stderr"
        if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
            "$(root_relative "$WORK_DIR/$name.json")" -o \
            "$(root_relative "$output")" >"$stdout" 2>"$stderr"); then
            fail "$target accepted Array argument mutation: $name"
        fi
        [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
        grep -Fq "$diagnostic" "$stdout" "$stderr" || {
            cat "$stdout" "$stderr" >&2 || true
            fail "$target rejection did not distinguish $name"
        }
    done
}

require_file "$SOURCE"
require_file "$DRIVER_BIN"
require_file "$PGY"
pgy_require_runnable_binary_here "$LABEL" "$DRIVER_BIN" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM IR compiler: $CLANG"
[[ -n "$PYTHON_BIN" ]] || fail "python3/python is required for typed mutations"
assert_owner_ratchet
mkdir -p "$WORK_DIR"

rm -f "$MIR"
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified \
    "$(root_relative "$SOURCE")" -o "$(root_relative "$MIR")") ||
    fail "source-to-MIR producer rejected array_literal_call_argument.pgy"
mir_digest="$(hash_file "$MIR")"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
    "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$NATIVE_MIR") ||
    fail "native MIR oracle rejected array_literal_call_argument.pgy"
"$PYTHON_BIN" \
    "$ROOT_DIR/tests/self_hosted/parity/one_mir_array_argument_mutations.py" \
    "$MIR" "$NATIVE_MIR" compare-param ||
    fail "native/self formal parameter ABI receipt parity drifted"
project "$MIR" c "$WORK_DIR/baseline.c"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "C projection mutated MIR"
project "$MIR" llvm "$WORK_DIR/baseline.llvm"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "LLVM projection mutated MIR"

grep -Fq 'static int32_t Double(int32_t value)' "$WORK_DIR/baseline.c" ||
    fail "C flattened or widened the nested scalar callee"
grep -Fq 'static int32_t SumPair(pgy_ai values)' "$WORK_DIR/baseline.c" ||
    fail "C did not pass the Array aggregate by value"
grep -Fq 'Double(4)' "$WORK_DIR/baseline.c" || fail "C flattened the nested call"
grep -Fq 'SumPair(pgy_argument)' "$WORK_DIR/baseline.c" || fail "C flattened the Array call"
grep -Fq 'define internal i32 @Double(i32 %value)' "$WORK_DIR/baseline.llvm" ||
    fail "LLVM flattened or widened the nested scalar callee"
grep -Fq 'define internal i32 @SumPair({ ptr, i64, i64, ptr } %values)' \
    "$WORK_DIR/baseline.llvm" || fail "LLVM did not pass the aggregate by value"
grep -Fq 'call i32 @Double(i32 4)' "$WORK_DIR/baseline.llvm" ||
    fail "LLVM flattened the nested call"
grep -Fq 'call i32 @SumPair({ ptr, i64, i64, ptr } %pgy.argument.3)' \
    "$WORK_DIR/baseline.llvm" || fail "LLVM flattened the Array call"
! grep -Fq '@pgy_' "$WORK_DIR/baseline.llvm" || fail "LLVM reopened Pergyra runtime"

"$CC" -x c -std=c11 "$WORK_DIR/baseline.c" -o "$WORK_DIR/baseline.c.exe" \
    >"$WORK_DIR/c.compile.log" 2>&1 || fail "C artifact did not compile"
"$CLANG" -x ir "$WORK_DIR/baseline.llvm" -o "$WORK_DIR/baseline.llvm.exe" \
    >"$WORK_DIR/llvm.compile.log" 2>&1 || fail "LLVM artifact did not compile"
(cd "$ROOT_DIR" && "$WORK_DIR/baseline.c.exe") |
    pgy_selfhost_normalize_text_artifact >"$WORK_DIR/c.run"
(cd "$ROOT_DIR" && "$WORK_DIR/baseline.llvm.exe") |
    pgy_selfhost_normalize_text_artifact >"$WORK_DIR/llvm.run"
printf '11\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/c.run" || fail "C output drifted"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/llvm.run" || fail "LLVM output drifted"

"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_array_argument_mutations.py" \
    "$MIR" "$WORK_DIR"
for target in c llvm; do
    project "$WORK_DIR/routine-order-cycle.json" "$target" \
        "$WORK_DIR/routine-order-cycle.$target"
    cmp -s "$WORK_DIR/baseline.$target" "$WORK_DIR/routine-order-cycle.$target" ||
        fail "$target artifact depends on routine row order"
done

reject_mutation entrypoint-name "entrypoint identity is invalid"
for mutation in array-param-type array-param-carriage array-param-resource \
    array-param-pass missing-param-abi repaired-param-abi; do
    reject_mutation "$mutation" "signature or ABI identity is invalid"
done
reject_mutation nested-call-unresolved "call target is unresolved"
reject_mutation nested-call-argument-edge "main graph is invalid"
reject_mutation array-call-argument-edge "main graph is invalid"
reject_mutation parameter-use "consumer graph is invalid"
reject_mutation unexpected-instruction-use "routine facts are invalid"
reject_mutation forged-return-result "instructions are invalid"
reject_mutation unreachable-callee "routine facts are invalid"
reject_mutation callee-successor "routine facts are invalid"
reject_mutation stray-instruction-abi "instruction ABI is invalid"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "negative gates mutated MIR"
echo "[$LABEL] native/self parameter ABI, caller-owned by-value Array<Int> C/LLVM parity, cyclic row order, and sixteen negatives are fail-closed (sha256=$mir_digest)"
