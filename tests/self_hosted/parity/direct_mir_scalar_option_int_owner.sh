#!/usr/bin/env bash
# Option<Int> callables over direct scalars and logical-record inputs.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-option-int"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_option_int"; WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_option_int_callables.pgy"; MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"; PARAM_MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_direct_scalar_mutations.py"; RECORD_MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_option_int_logical_record_mutations.py"
ABI_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_option_int_abi_owner.pgy"; PARAM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_role_plan_owner.pgy"
LOCAL_TYPE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_type_family_owner.pgy"; SIGNATURE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_owner.pgy"
RECORD_POLICY="$PARAM_OWNER"; C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_option_int_owner.pgy"; LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_option_int_owner.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$ABI_OWNER" "$PARAM_OWNER" "$LOCAL_TYPE_OWNER" "$SIGNATURE_OWNER" "$RECORD_POLICY" "$C_OWNER" "$LLVM_OWNER" "$MUTATIONS" "$PARAM_MUTATIONS" "$RECORD_MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'MirCapturedRequiredAbiLayoutRowAdmission(' "$ABI_OWNER" ||
    fail "Option<Int> program path does not consume the MIR ABI receipt"
grep -Fq 'DirectMirScalarProgramOptionIntAbiFromParameter(' "$ABI_OWNER" || fail "Option<Int> parameter path bypasses the MIR ABI receipt"
grep -Fq 'own parameter: DirectMirRoutineParamFact' "$ABI_OWNER" ||
    fail "Option<Int> parameter ABI projection does not own the forwarded receipt"
grep -Fq 'DirectMirOptionMatchAbiFactFromCapture(abi_capture)' "$ABI_OWNER" ||
    fail "Option<Int> parameter ABI projection omits the named transfer binding"
! grep -Fq 'DirectMirOptionMatchAbiFactFromCapture(parameter.abi)' "$ABI_OWNER" ||
    fail "Option<Int> parameter ABI projection forwarded an unnamed boundary value"
grep -Fq 'DirectMirScalarProgramComposableCallableSignatureReady(' "$SIGNATURE_OWNER" ||
    fail "Option<Int> signature does not consume the parameter-role plan"
grep -Fq 'CompilerAbiLayoutOptionIntTypeName()' "$LOCAL_TYPE_OWNER" ||
    fail "source-local type owner omits Option<Int>"
! grep -Fq 'signature.param_count ==' "$PARAM_OWNER" ||
    fail "direct scalar parameter owner reintroduced an exact arity"
grep -Fq 'logical_record_input_count' "$RECORD_POLICY" ||
    fail "parameter-role plan omitted record-input cardinality"
! grep -Fq 'signature.param_count ==' "$RECORD_POLICY" ||
    fail "logical-record input owner reintroduced an exact arity"
grep -Fq 'DirectMirScalarProgramLogicalRecordParameterReady(' "$RECORD_POLICY" ||
    fail "logical-record input owner duplicated the admitted record role"
grep -Fq 'DirectMirScalarProgramDirectScalarParameterReady(' "$RECORD_POLICY" ||
    fail "logical-record input owner duplicated the direct scalar role"
grep -Fq 'claim_count != 1' "$RECORD_POLICY" ||
    fail "parameter-role plan omitted unique role admission"
grep -Fq 'DirectMirOptionMatchAbiProjectionFromFact(' "$C_OWNER" ||
    fail "C Option<Int> representation does not consume the ABI projection"
grep -Fq 'DirectMirOptionMatchAbiProjectionFromFact(' "$LLVM_OWNER" ||
    fail "LLVM Option<Int> representation does not consume the ABI projection"
for owner in "$C_OWNER" "$LLVM_OWNER"; do
    grep -Fq 'fact.some_tag' "$owner" ||
        fail "target owner guessed the Some discriminant"
    grep -Fq 'fact.none_tag' "$owner" ||
        fail "target owner guessed the None discriminant"
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"abi_type_name":"Option<Int>"' "$MIR" ||
    fail "producer emitted no Option<Int> ABI receipt"
grep -Fq '"name":"FindFrom"' "$MIR" ||
    fail "producer omitted mixed direct scalar Option<Int> callable"
grep -Fq '"name":"FindField"' "$MIR" ||
    fail "producer omitted readonly-record Option<Int> callable"
grep -Fq '"name":"FindValue"' "$MIR" ||
    fail "producer omitted value-record Option<Int> callable"
grep -Fq '"name":"RelayOption"' "$MIR" || fail "producer omitted by-value Option<Int> callable"
grep -Fq '"type":"Option<Int>","carriage":"value"' "$MIR" || fail "producer omitted by-value Option<Int> parameter ABI"
grep -Fq '"type":"JsonIndexView","carriage":"value"' "$MIR" ||
    fail "producer omitted value-record parameter role"
grep -Fq '"name":"relayed","type":"Option<Int>"' "$MIR" ||
    fail "producer omitted the Option<Int> source-local boundary"

printf '11\n5\n6\n7\n' >"$WORK_DIR/expected.run"
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
        grep -Fq 'typedef struct {' "$artifact" ||
            fail "C artifact omitted the Option<Int> representation"
        grep -Eq '^static pgy_scalar_option_int pgy_scalar_routine_[0-9]+\(const char\* pgy_param_0, const char\* pgy_param_1, int32_t pgy_param_2\)' "$artifact" ||
            fail "C Option<Int> direct scalar signature drifted"
        grep -Eq '^static pgy_scalar_option_int pgy_scalar_routine_[0-9]+\(const pgy_scalar_logical_record_value_[0-9]+ \*pgy_param_0, const char\* pgy_param_1\)' "$artifact" ||
            fail "C Option<Int> readonly-record signature drifted"
        grep -Eq '^static pgy_scalar_option_int pgy_scalar_routine_[0-9]+\(pgy_scalar_option_int pgy_param_0\)' "$artifact" || fail "C Option<Int> value signature drifted"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" ||
            fail "C artifact did not compile"
    else
        grep -Fq '%pgy.scalar.option.int = type { i32, i32 }' "$artifact" ||
            fail "LLVM artifact omitted the admitted Option<Int> representation"
        grep -Eq '^define internal %pgy\.scalar\.option\.int @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, ptr %pgy\.param\.1, i64 %pgy\.param\.2\)' "$artifact" ||
            fail "LLVM Option<Int> direct scalar signature drifted"
        grep -Eq '^define internal %pgy\.scalar\.option\.int @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, ptr %pgy\.param\.1\)' "$artifact" ||
            fail "LLVM Option<Int> readonly-record signature drifted"
        grep -Eq '^define internal %pgy\.scalar\.option\.int @pgy\.scalar\.routine\.[0-9]+\(%pgy\.scalar\.option\.int %pgy\.param\.0\)' "$artifact" || fail "LLVM Option<Int> value signature drifted"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in carriage pass-shape resource abi-required; do
    mutated_rel="$WORK_REL/option-record-$mutation.mir.json"
    mutated="$ROOT_DIR/$mutated_rel"
    python "$RECORD_MUTATIONS" "$MIR" "$mutation" "$mutated"
    for backend in c llvm; do
        output_rel="$WORK_REL/option-record-$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/option-record-$mutation.$backend.out" \
            2>"$WORK_DIR/option-record-$mutation.$backend.err"; then
            fail "$backend accepted Option<Int> record $mutation"
        fi
        [[ ! -e "$output" ]] ||
            fail "$backend published Option<Int> record $mutation"
    done
done

mutated_rel="$WORK_REL/option-abi-layout.mir.json"
mutated="$ROOT_DIR/$mutated_rel"
python "$MUTATIONS" "$MIR" option-abi-layout "$mutated"
[[ -s "$mutated" ]] || fail "could not create Option<Int> ABI mutation"
for backend in c llvm; do
    output_rel="$WORK_REL/option-abi-layout.$backend"
    output="$ROOT_DIR/$output_rel"
    rm -f "$output"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$mutated_rel" -o "$output_rel") \
        >"$WORK_DIR/mutated.$backend.out" \
        2>"$WORK_DIR/mutated.$backend.err"; then
        fail "$backend accepted a mutated Option<Int> ABI layout"
    fi
    [[ ! -e "$output" ]] ||
        fail "$backend published an artifact for a mutated Option<Int> ABI"
done

for mutation in carriage pass resource abi-required; do
    mutated_rel="$WORK_REL/option-direct-$mutation.mir.json"
    mutated="$ROOT_DIR/$mutated_rel"
    python "$PARAM_MUTATIONS" "$MIR" "$mutation" "$mutated" FindFrom
    [[ -s "$mutated" ]] || fail "could not create $mutation mutation"
    for backend in c llvm; do
        output_rel="$WORK_REL/option-direct-$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/option-direct-$mutation.$backend.out" \
            2>"$WORK_DIR/option-direct-$mutation.$backend.err"; then
            fail "$backend accepted Option<Int> $mutation"
        fi
        [[ ! -e "$output" ]] ||
            fail "$backend published Option<Int> $mutation"
    done
done

for mutation in abi-layout carriage pass-shape; do
    mutated_rel="$WORK_REL/option-value-$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "option-int-value-$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/option-value-$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$mutated_rel" -o "$output_rel") >"$WORK_DIR/option-value-$mutation.$backend.out" 2>"$WORK_DIR/option-value-$mutation.$backend.err"; then fail "$backend accepted Option<Int> value $mutation"; fi
    done
done
echo "[$LABEL] direct-scalar/value-or-readonly-record Option<Int> C/LLVM parity + negatives: PASS"
