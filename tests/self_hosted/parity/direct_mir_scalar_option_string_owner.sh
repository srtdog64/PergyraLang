#!/usr/bin/env bash
# Option<String> consumes one persisted MIR ABI receipt in both GraphPlan targets.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-option-string"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_option_string"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_four_routine_option_string.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"
RECORD_MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_option_int_logical_record_mutations.py"
ABI_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_option_string_abi_owner.pgy"
PARAM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_role_plan_owner.pgy"
SIGNATURE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_option_string_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_option_string_owner.pgy"
BUILTIN_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_option_string_builtin_signature_owner.pgy"
CALL_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_call_expression_admission_owner.pgy"
CALLABLE_INVENTORY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_inventory_owner.pgy"
EXPRESSION_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_admission_owner.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$ABI_OWNER" "$PARAM_OWNER" "$SIGNATURE_OWNER" "$C_OWNER" "$LLVM_OWNER" "$BUILTIN_OWNER" "$CALL_OWNER" "$CALLABLE_INVENTORY" "$EXPRESSION_OWNER" "$MUTATIONS" "$RECORD_MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'MirCapturedRequiredAbiLayoutRowAdmission(' "$ABI_OWNER" ||
    fail "Option<String> path does not consume the MIR ABI receipt"
grep -Fq 'own parameter: DirectMirRoutineParamFact' "$ABI_OWNER" ||
    fail "Option<String> parameter ABI projection does not own the forwarded receipt"
grep -Fq 'DirectMirOptionStringAbiFactFromCapture(abi_capture)' "$ABI_OWNER" ||
    fail "Option<String> parameter ABI projection omits the named transfer binding"
! grep -Fq 'DirectMirOptionStringAbiFactFromCapture(parameter.abi)' "$ABI_OWNER" ||
    fail "Option<String> parameter ABI projection forwarded an unnamed boundary value"
grep -Fq 'let option_return: Bool' "$SIGNATURE_OWNER" ||
    fail "Option return family is not composed once"
grep -Fq 'DirectMirScalarProgramCallableParameterRolePlanReady(' "$SIGNATURE_OWNER" ||
    fail "Option return family does not consume the parameter-role plan"
grep -Fq 'DirectMirScalarProgramLogicalRecordParameterReady(' "$PARAM_OWNER" ||
    fail "parameter-role plan omitted readonly logical records"
grep -Fq 'DirectMirScalarProgramAbiValueParameterReady(' "$PARAM_OWNER" ||
    fail "parameter-role plan omitted by-value Option<String>"
grep -Fq 'DirectMirOptionStringAbiFactReady(fact)' "$C_OWNER" ||
    fail "C target does not consume the admitted Option<String> fact"
grep -Fq 'DirectMirOptionStringAbiFactReady(fact)' "$LLVM_OWNER" ||
    fail "LLVM target does not consume the admitted Option<String> fact"
for owner in "$C_OWNER" "$LLVM_OWNER"; do
    grep -Fq 'fact.some_tag' "$owner" || fail "target guessed Some tag"
    grep -Fq 'fact.none_tag' "$owner" || fail "target guessed None tag"
done
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"abi_type_name":"Option<String>"' "$MIR" ||
    fail "producer emitted no Option<String> ABI receipt"
grep -Fq '"name":"FindKind"' "$MIR" ||
    fail "producer omitted readonly-record Option<String> callable"
grep -Fq '"type":"JsonStringView","carriage":"readonly-ref"' "$MIR" ||
    fail "producer omitted readonly-record parameter role"
grep -Fq '"type":"Option<String>","carriage":"value"' "$MIR" ||
    fail "producer omitted by-value Option<String> parameter role"
grep -Fq '"call_target_name":"OptionStringIsMissing"' "$MIR" ||
    fail "producer omitted the nested Option<String> None call identity"
grep -Fq 'func DirectMirScalarProgramDirectCallArgumentExpectedType(' \
    "$CALL_OWNER" || fail "direct-call owner omitted argument expected types"
grep -Fq 'func DirectMirScalarCfgProgramCallableParameterTypeBySyntaxId(' \
    "$CALLABLE_INVENTORY" || fail "callable inventory omitted parameter type lookup"
grep -Fq 'DirectMirScalarProgramDirectCallArgumentExpectedType(' \
    "$EXPRESSION_OWNER" || fail "None admission bypasses the direct-call type"

printf 'option-string\nrecord-string\nmissing\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" \
                "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq 'pgy_scalar_option_string' "$artifact" ||
            fail "C artifact omitted Option<String> representation"
        grep -Eq '^static pgy_scalar_option_string pgy_scalar_routine_[0-9]+\(const pgy_scalar_logical_record_value_[0-9]+ \*pgy_param_0, const char\* pgy_param_1\)' "$artifact" ||
            fail "C Option<String> readonly-record signature drifted"
        grep -Fq 'pgy_scalar_option_string pgy_param_1' "$artifact" ||
            fail "C by-value Option<String> signature drifted"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || {
                cat "$WORK_DIR/$backend.compile.err" >&2
                fail "C artifact did not compile"
            }
    else
        grep -Fq '%pgy.scalar.option.string = type { i32, ptr }' "$artifact" ||
            fail "LLVM artifact omitted Option<String> representation"
        grep -Eq '^define internal %pgy\.scalar\.option\.string @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, ptr %pgy\.param\.1\)' "$artifact" ||
            fail "LLVM Option<String> readonly-record signature drifted"
        grep -Fq '%pgy.scalar.option.string %pgy.param.1' "$artifact" ||
            fail "LLVM by-value Option<String> signature drifted"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || {
                cat "$WORK_DIR/$backend.compile.err" >&2
                fail "LLVM artifact did not compile"
            }
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done
for mutation in carriage pass-shape resource abi-required; do
    mutated_rel="$WORK_REL/option-string-record-$mutation.mir.json"
    mutated="$ROOT_DIR/$mutated_rel"
    python "$RECORD_MUTATIONS" "$MIR" "$mutation" "$mutated" FindKind
    for backend in c llvm; do
        output_rel="$WORK_REL/option-string-record-$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/record-$mutation.$backend.out" \
            2>"$WORK_DIR/record-$mutation.$backend.err"; then
            fail "$backend accepted Option<String> record $mutation"
        fi
        [[ ! -e "$output" ]] ||
            fail "$backend published Option<String> record $mutation"
    done
done

mutated_rel="$WORK_REL/option-string-abi-layout.mir.json"
mutated="$ROOT_DIR/$mutated_rel"
python "$MUTATIONS" "$MIR" option-string-abi-layout "$mutated"
[[ -s "$mutated" ]] || fail "could not create Option<String> ABI mutation"
for backend in c llvm; do
    output_rel="$WORK_REL/option-string-abi-layout.$backend"
    output="$ROOT_DIR/$output_rel"
    rm -f "$output"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$mutated_rel" -o "$output_rel") \
        >"$WORK_DIR/mutated.$backend.out" \
        2>"$WORK_DIR/mutated.$backend.err"; then
        fail "$backend accepted a mutated Option<String> ABI layout"
    fi
    [[ ! -e "$output" ]] ||
        fail "$backend published an artifact for mutated Option<String> ABI"
done

for mutation in option-string-value-abi-layout option-string-value-carriage \
        option-string-value-pass-shape option-string-none-call-target; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >/dev/null 2>&1; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] scalar/readonly-record Option<String> C/LLVM parity + negatives: PASS"
