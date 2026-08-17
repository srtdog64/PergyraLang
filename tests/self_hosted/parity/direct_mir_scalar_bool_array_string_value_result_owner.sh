#!/usr/bin/env bash
# Bool return preserves exact one-or-more Array<String> value-result copy-outs.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-bool-array-string-value-result"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_bool_array_string_value_result"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_bool_four_array_string_value_result.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_policy_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_bool_array_string_value_result_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'DirectMirScalarProgramArrayStringValueResultSignatureReady(' "$POLICY" ||
    fail "signature owner is missing"
grep -Fq 'return value_result_count >= 1;' "$POLICY" ||
    fail "Bool copy-out cardinality is not type-shaped"
if grep -Fq 'signature.param_count == 7' "$POLICY" ||
    grep -Fq 'return value_result_count == 4' "$POLICY"; then
    fail "retired Bool/four-copyout shape remains"
fi
if grep -Fq 'DirectMirScalarProgramVoidArrayStringValueResultSignatureReady(' "$POLICY"; then
    fail "retired Void-only signature owner remains"
fi

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"CaptureFields"' "$MIR" ||
    fail "producer omitted CaptureFields"
grep -Fq '"name":"ReadStringArray"' "$MIR" ||
    fail "producer omitted ReadStringArray"
grep -Fq '"return":"Bool"' "$MIR" || fail "producer omitted Bool return"
[[ "$(grep -o '"type":"Array<String>","carriage":"value-result"' "$MIR" | wc -l)" -eq 5 ]] ||
    fail "producer did not emit the four-copyout and single-copyout rows"
printf 'bool-copyout-ready\n0\n0\n0\n0\nsingle-copyout-ready\n1\n' >"$WORK_DIR/expected.run"

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
        grep -Eq 'static bool pgy_scalar_routine_[0-9]+\(' "$artifact" ||
            fail "C omitted the Bool callable signature"
        for parameter in 3 4 5 6; do
            grep -Fq "pgy_as *pgy_param_${parameter}_mutref" "$artifact" ||
                fail "C signature omitted copy-out parameter $parameter"
            grep -Fq "pgy_as pgy_param_${parameter} = *pgy_param_${parameter}_mutref;" "$artifact" ||
                fail "C omitted copy-in for parameter $parameter"
            [[ "$(grep -Fc "*pgy_param_${parameter}_mutref = pgy_param_${parameter};" "$artifact")" -ge 2 ]] ||
                fail "C omitted an early/final copy-out for parameter $parameter"
        done
        grep -Fq 'return false;' "$artifact" || fail "C lost the early Bool return"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >/dev/null 2>"$WORK_DIR/c.compile.err" ||
            fail "C artifact did not compile"
    else
        grep -Eq 'define internal i1 @pgy\.scalar\.routine\.[0-9]+\(' "$artifact" ||
            fail "LLVM omitted the Bool callable signature"
        for parameter in 3 4 5 6; do
            grep -Fq "ptr %pgy.param.${parameter}.mutref" "$artifact" ||
                fail "LLVM signature omitted copy-out parameter $parameter"
            grep -Fq "%pgy.param.${parameter}.local = alloca %pgy.array.string" "$artifact" ||
                fail "LLVM omitted copy-in storage for parameter $parameter"
            [[ "$(grep -Fc "%pgy.param.${parameter}.copyout." "$artifact")" -ge 4 ]] ||
                fail "LLVM omitted an early/final copy-out for parameter $parameter"
        done
        grep -Fq 'ret i1 false' "$artifact" || fail "LLVM lost the early Bool return"
        "$CLANG" -x ir "$artifact" -o "$bin" >/dev/null \
            2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    if ! "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"; then
        fail "$backend runtime execution failed"
    fi
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in unsupported-return-type fourth-copyout-carriage \
    single-copyout-carriage single-copyout-abi-layout \
    single-prefix-carriage; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published an artifact for $mutation"
    done
done

echo "[$LABEL] Bool return + one-or-more Array<String> copy-outs: PASS"
