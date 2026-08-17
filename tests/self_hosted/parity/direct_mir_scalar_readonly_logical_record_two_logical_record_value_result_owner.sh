#!/usr/bin/env bash
# String + readonly record + two logical-record copyouts C/LLVM boundary.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-readonly-record-two-record-copyouts"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_readonly_record_two_record_copyouts"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_readonly_logical_record_two_logical_record_value_result.pgy"
MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_value_result_policy_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_readonly_logical_record_two_logical_record_value_result_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'let readonly: Bool = carriage == "readonly-ref"' "$POLICY" || fail "readonly role is missing"
grep -Fq 'return copyout_count >= 1;' "$POLICY" || fail "positive copyout cardinality is missing"

mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") \
    >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"BuildExecutionPlan"' "$MIR" || fail "producer omitted callable"
[[ "$(grep -Fo '"carriage":"value-result"' "$MIR" | wc -l)" -ge 2 ]] ||
    fail "producer omitted two record copyouts"
printf 'readonly-record-two-record-copyouts-ready\n1\n2\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"; artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$MIR_REL" -o "$artifact_rel") \
        >"$WORK_DIR/$backend.project.out" 2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        signature="$(grep -E '^static bool pgy_scalar_routine_[0-9]+\(' "$artifact" | grep -F 'pgy_param_2_mutref' | head -n 1)"
        [[ "$signature" == *'const char* pgy_param_0'* ]] || fail "C omitted String ordinal 0"
        [[ "$signature" =~ const[[:space:]]+pgy_scalar_logical_record_value_[0-9]+[[:space:]]+\*pgy_param_1 ]] || fail "C omitted readonly record ordinal 1"
        [[ "$signature" =~ pgy_scalar_logical_record_value_[0-9]+[[:space:]]+\*pgy_param_2_mutref ]] || fail "C omitted record copyout ordinal 2"
        [[ "$signature" =~ pgy_scalar_logical_record_value_[0-9]+[[:space:]]+\*pgy_param_3_mutref ]] || fail "C omitted record copyout ordinal 3"
        for ordinal in 2 3; do
            grep -Eq "pgy_scalar_logical_record_value_[0-9]+ pgy_param_$ordinal = \\*pgy_param_${ordinal}_mutref;" "$artifact" || fail "C omitted copy-in $ordinal"
            [[ "$(grep -Fc "*pgy_param_${ordinal}_mutref = pgy_param_$ordinal;" "$artifact")" -ge 2 ]] || fail "C omitted copyout $ordinal"
        done
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin"); "${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Eq 'define internal i1 @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, ptr %pgy\.param\.1, ptr %pgy\.param\.2\.mutref, ptr %pgy\.param\.3\.mutref\)' "$artifact" || fail "LLVM omitted the exact signature"
        for ordinal in 2 3; do
            grep -Eq "%pgy\.param\.$ordinal\.local = alloca %pgy\.scalar\.logical\.record\.value\.[0-9]+" "$artifact" || fail "LLVM omitted copy-in $ordinal"
            [[ "$(grep -Fc "%pgy.param.$ordinal.record.copyout." "$artifact")" -ge 4 ]] || fail "LLVM omitted copyout $ordinal"
        done
        "$CLANG" -x ir "$artifact" -o "$bin" >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" || fail "$backend runtime output drifted"
done

positive_rel="$WORK_REL/first-copyout-value.mir.json"
python "$MUTATIONS" "$MIR" first-copyout-value "$ROOT_DIR/$positive_rel"
for backend in c llvm; do
    output_rel="$WORK_REL/first-copyout-value.$backend"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$positive_rel" -o "$output_rel") \
        >"$WORK_DIR/first-copyout-value.$backend.out" \
        2>"$WORK_DIR/first-copyout-value.$backend.err" ||
        fail "$backend rejected composable value + copyout records"
    [[ -s "$ROOT_DIR/$output_rel" ]] || fail "$backend omitted composable value artifact"
done

for mutation in string-type string-carriage readonly-type readonly-carriage readonly-pass \
    first-copyout-carriage first-copyout-pass first-copyout-abi \
    second-copyout-carriage second-copyout-pass second-copyout-abi; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"; rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" 2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] readonly record + two record copyouts C/LLVM parity + negatives: PASS"
