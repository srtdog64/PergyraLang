#!/usr/bin/env bash
# Exact Array<Int> value-result ABI reaches both GraphPlan target boundaries.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-array-int-value-result"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_array_int_value_result"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_two_routine_array_int_value_result.pgy"
CALL_REL="tests/self_hosted/fixtures/direct_mir_two_routine_array_int_value_result_call.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"
ROLE_MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_array_int_value_result_mutations.py"

FACT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_int_value_result_fact_owner.pgy"
CALLABLE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_role_plan_owner.pgy"
SIGNATURE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_owner.pgy"
TARGET_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_int_value_result_target_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_array_int_value_result_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_int_value_result_owner.pgy"
CALL_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_call_expression_admission_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$FACT_OWNER" "$CALLABLE_OWNER" "$SIGNATURE_OWNER" "$TARGET_OWNER" "$C_OWNER" "$LLVM_OWNER" \
    "$CALL_OWNER" "$MUTATIONS" "$ROLE_MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'DirectMirArrayIntCapturedAbiReady(fact.abi)' "$FACT_OWNER" ||
    fail "value-result fact does not consume the complete parameter ABI"
grep -Fq 'DirectMirScalarProgramArrayIntValueResultParameterReady(' "$CALLABLE_OWNER" &&
    grep -Fq 'DirectMirScalarProgramCallableParameterSupported(' "$CALLABLE_OWNER" ||
    fail "parameter-role plan omitted the ArrayInt copyout role"
grep -Fq 'array_int_value_result_count' "$CALLABLE_OWNER" ||
    fail "parameter-role plan omitted ArrayInt copyout cardinality"
! grep -Fq 'signature.param_count ==' "$CALLABLE_OWNER" ||
    fail "callable owner reintroduced an exact arity"
grep -Fq 'DirectMirScalarProgramComposableCallableSignatureReady(' \
    "$SIGNATURE_OWNER" || fail "final signature omitted the parameter-role plan"
! grep -Fq 'json_string_value_result' "$SIGNATURE_OWNER" ||
    fail "final signature retained the exact JSON-shaped fallback"
grep -Fq 'DirectMirScalarProgramArrayIntValueResultTargetFromFact(' \
    "$TARGET_OWNER" || fail "value-result ABI has no target projection"
for owner in "$C_OWNER" "$LLVM_OWNER"; do
    grep -Fq 'DirectMirScalarProgramArrayIntValueResultAt(' "$owner" ||
        fail "target boundary does not query the sealed parameter identity"
done
grep -Fq 'parameter_carriages[parameter_row] != "value-result"' "$CALL_OWNER" ||
    fail "caller-side value-result identity is not admitted explicitly"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"type":"Array<Int>","carriage":"value-result"' "$MIR" ||
    fail "producer emitted no value-result Array<Int> parameter"
grep -Fq '"name":"ReadSpan"' "$MIR" ||
    fail "producer omitted the shorter scalar/copyout callable"
grep -Fq '"name":"ValueBounds"' "$MIR" ||
    fail "producer omitted the composed record/scalar/copyout callable"
grep -Fq '"type":"SpanTable","carriage":"readonly-ref"' "$MIR" ||
    fail "producer omitted the composed readonly-record role"
grep -Fq '"size":32,"align":8' "$MIR" ||
    fail "producer omitted the Array<Int> physical ABI receipt"

printf '11\n' >"$WORK_DIR/expected.run"
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
        grep -Fq 'pgy_ai *pgy_param_3_mutref' "$artifact" ||
            fail "C signature omitted the value-result pointer"
        grep -Fq 'pgy_ai *pgy_param_2_mutref' "$artifact" ||
            fail "C signature omitted the shorter value-result pointer"
        grep -Eq 'const pgy_scalar_logical_record_value_[0-9]+ \*pgy_param_0, const char\* pgy_param_1, pgy_ai \*pgy_param_2_mutref' "$artifact" ||
            fail "C artifact omitted the composed parameter-role signature"
        grep -Fq 'pgy_ai pgy_param_2 = *pgy_param_2_mutref;' "$artifact" ||
            fail "C shorter callable omitted Array<Int> copy-in"
        grep -Fq '*pgy_param_2_mutref = pgy_param_2;' "$artifact" ||
            fail "C shorter callable omitted Array<Int> copy-out"
        grep -Fq 'pgy_ai pgy_param_3 = *pgy_param_3_mutref;' "$artifact" ||
            fail "C callable omitted Array<Int> copy-in"
        grep -Fq '*pgy_param_3_mutref = pgy_param_3;' "$artifact" ||
            fail "C callable omitted Array<Int> copy-out"
        grep -Fq '_Static_assert(sizeof(pgy_ai) == 32' "$artifact" ||
            fail "C artifact omitted the Array<Int> size receipt"
        ! grep -Fq 'static void pgy_ai_push(' "$artifact" ||
            fail "C materialized an unused Array<Int> push helper"
        ! grep -Fq 'static int32_t pgy_ai_get(' "$artifact" ||
            fail "C materialized an unused Array<Int> get helper"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C artifact did not compile"
    else
        grep -Fq '%pgy.array.int = type { ptr, i64, i64, ptr }' "$artifact" ||
            fail "LLVM artifact omitted the Array<Int> representation"
        grep -Fq 'ptr %pgy.param.3.mutref' "$artifact" ||
            fail "LLVM signature omitted the value-result pointer"
        grep -Fq 'ptr %pgy.param.2.mutref' "$artifact" ||
            fail "LLVM signature omitted the shorter value-result pointer"
        grep -Eq 'define internal i1 @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, ptr %pgy\.param\.1, ptr %pgy\.param\.2\.mutref\)' "$artifact" ||
            fail "LLVM artifact omitted the composed parameter-role signature"
        grep -Fq 'load %pgy.array.int, ptr %pgy.param.2.mutref' "$artifact" ||
            fail "LLVM shorter callable omitted Array<Int> copy-in"
        grep -Eq '%pgy\.param\.2\.copyout\.[0-9]+ = load %pgy\.array\.int, ptr %pgy\.param\.2\.local' \
            "$artifact" || fail "LLVM shorter callable omitted Array<Int> copy-out load"
        grep -Eq 'store %pgy\.array\.int %pgy\.param\.2\.copyout\.[0-9]+, ptr %pgy\.param\.2\.mutref' \
            "$artifact" || fail "LLVM shorter callable omitted Array<Int> copy-out store"
        grep -Fq 'load %pgy.array.int, ptr %pgy.param.3.mutref' "$artifact" ||
            fail "LLVM callable omitted Array<Int> copy-in"
        grep -Eq '%pgy\.param\.3\.copyout\.[0-9]+ = load %pgy\.array\.int, ptr %pgy\.param\.3\.local' \
            "$artifact" || fail "LLVM callable omitted Array<Int> copy-out load"
        grep -Eq 'store %pgy\.array\.int %pgy\.param\.3\.copyout\.[0-9]+, ptr %pgy\.param\.3\.mutref' \
            "$artifact" || fail "LLVM callable omitted Array<Int> copy-out store"
        ! grep -Fq 'define internal void @pgy_ai_push(' "$artifact" ||
            fail "LLVM materialized an unused Array<Int> push helper"
        ! grep -Fq 'define internal i64 @pgy_ai_get(' "$artifact" ||
            fail "LLVM materialized an unused Array<Int> get helper"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" 2>"$WORK_DIR/$backend.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in array-int-value-result-abi-layout; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published an artifact for $mutation"
    done
done

for mutation in carriage pass-shape resource abi-required \
    composed-record-pass composed-copyout-carriage; do
    mutated_rel="$WORK_REL/array-int-value-result-$mutation.mir.json"
    python "$ROLE_MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/array-int-value-result-$mutation.$backend"
        rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published an artifact for $mutation"
    done
done

call_mir_rel="$WORK_REL/call.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$CALL_REL" \
    -o "$call_mir_rel") >"$WORK_DIR/call.producer.out" \
    2>"$WORK_DIR/call.producer.err" || fail "caller MIR production failed"
grep -Fq 'ObserveWindow(text, 0, 1, ends)' "$ROOT_DIR/$CALL_REL" ||
    fail "caller fixture omitted value Array<Int> to value-result forwarding"
printf 'x\ny\n' >"$WORK_DIR/call.expected.run"
for backend in c llvm; do
    output_rel="$WORK_REL/call.$backend"
    bin="$WORK_DIR/call-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$call_mir_rel" -o "$output_rel") >"$WORK_DIR/call.$backend.out" \
        2>"$WORK_DIR/call.$backend.err" || fail "$backend value-result call projection failed"
    if [[ "$backend" == c ]]; then
        grep -Fq '&pgy_local_0' "$ROOT_DIR/$output_rel" ||
            fail "C value-result call omitted the local address"
        grep -Fq '&pgy_param_1' "$ROOT_DIR/$output_rel" ||
            fail "C value-result call omitted the value-parameter local header"
        command=("$CC" -x c -std=c11 "$ROOT_DIR/$output_rel")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$ROOT_DIR/$output_rel"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}"
    else
        grep -Fq 'ptr %pgy.local.0' "$ROOT_DIR/$output_rel" ||
            fail "LLVM value-result call omitted the local address"
        grep -Fq 'ptr %pgy.param.1' "$ROOT_DIR/$output_rel" ||
            fail "LLVM value-result call omitted the value-parameter local header"
        "$CLANG" -x ir "$ROOT_DIR/$output_rel" -o "$bin"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/call.$backend.run"
    cmp -s "$WORK_DIR/call.expected.run" "$WORK_DIR/call.$backend.run" ||
        fail "$backend value-result call output drifted"
done

call_bad_rel="$WORK_REL/call-nonaddressable.mir.json"
python "$ROLE_MUTATIONS" "$ROOT_DIR/$call_mir_rel" call-nonaddressable \
    "$ROOT_DIR/$call_bad_rel"
for backend in c llvm; do
    output_rel="$WORK_REL/call-nonaddressable.$backend"
    rm -f "$ROOT_DIR/$output_rel"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$call_bad_rel" -o "$output_rel") >"$WORK_DIR/call-bad.$backend.out" \
        2>"$WORK_DIR/call-bad.$backend.err"; then
        fail "$backend accepted a non-addressable value-result actual"
    fi
    [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published a non-addressable call"
done

echo "[$LABEL] C/LLVM value-result ABI, copy boundary, and negatives: PASS"
