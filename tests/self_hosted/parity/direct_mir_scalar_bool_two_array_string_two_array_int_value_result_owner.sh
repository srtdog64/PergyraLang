#!/usr/bin/env bash
# Bool with two ArrayString/two ArrayInt copyouts, backends and negatives.
# one carried target projection owns scalar preamble C/LLVM storage and complete cross-family row rejection
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-bool-two-array-string-two-array-int-copyout"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_bool_two_array_string_two_array_int_copyout"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_bool_two_array_string_two_array_int_value_result.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_bool_two_array_string_two_array_int_value_result_policy_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_bool_two_array_string_two_array_int_value_result_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'array_string_count == 2 && array_int_count == 2' "$POLICY" ||
    fail "2+2 collection cardinality is not exact"
grep -Fq 'array_string_layout != array_int_layout' "$POLICY" ||
    fail "cross-family ABI identity is not separated"
! grep -Fq 'ordinal == 0 || ordinal == 1' "$POLICY" ||
    fail "retired positional collection policy remains"
! grep -Fq 'signature.param_count != 8' "$POLICY" ||
    fail "retired exact parameter-count policy remains"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"GraphAddEdge"' "$MIR" || fail "producer omitted callable"
grep -Fq '"name":"ReadRequires"' "$MIR" || fail "producer omitted interleaved callable"
[[ "$(grep -o '"type":"Array<String>","carriage":"value-result"' "$MIR" | wc -l)" -eq 4 ]] ||
    fail "producer omitted ArrayString copyouts"
[[ "$(grep -o '"type":"Array<Int>","carriage":"value-result"' "$MIR" | wc -l)" -eq 4 ]] ||
    fail "producer omitted ArrayInt copyouts"
printf 'mixed-four-copyouts-ready\nmixed-interleaved-ready\n1\n1\n1\n1\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Eq 'static bool pgy_scalar_routine_[0-9]+\(pgy_as \*pgy_param_0_mutref, pgy_as \*pgy_param_1_mutref, pgy_ai \*pgy_param_2_mutref, pgy_ai \*pgy_param_3_mutref' "$artifact" ||
            fail "C omitted the exact mixed-copyout signature"
        for parameter in 0 1 2 3; do
            [[ "$(grep -Fc "*pgy_param_${parameter}_mutref = pgy_param_${parameter};" "$artifact")" -ge 2 ]] ||
                fail "C omitted early/final copyout $parameter"
        done
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" ||
            fail "C artifact did not compile"
    else
        grep -Eq 'define internal i1 @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0\.mutref, ptr %pgy\.param\.1\.mutref, ptr %pgy\.param\.2\.mutref, ptr %pgy\.param\.3\.mutref' "$artifact" ||
            fail "LLVM omitted the exact mixed-copyout signature"
        for parameter in 0 1; do
            [[ "$(grep -Fc "%pgy.param.${parameter}.copyout." "$artifact")" -ge 4 ]] ||
                fail "LLVM omitted early/final copyout $parameter"
        done
        for parameter in 2 3; do
            [[ "$(grep -Fc "%pgy.param.${parameter}.copyout." "$artifact")" -ge 4 ]] ||
                fail "LLVM omitted early/final ArrayInt copyout $parameter"
        done
        grep -Fq '%pgy.param.4.copyout.' "$artifact" || fail "LLVM omitted interleaved ArrayInt copyout 4"
        grep -Fq '%pgy.param.5.copyout.' "$artifact" || fail "LLVM omitted interleaved ArrayInt copyout 5"
        "$CLANG" -x ir "$artifact" -o "$bin" >"$WORK_DIR/llvm.compile.out" \
            2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in interleaved-array-string-carriage interleaved-array-int-abi \
    interleaved-scalar-carriage interleaved-unknown-family \
    array-string-type array-string-carriage array-string-abi \
    array-string-layout-mismatch array-int-type array-int-carriage \
    array-int-abi array-int-layout-mismatch cross-family-layout \
    cross-family-layout-row string-carriage unknown-return-type; do
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

echo "[$LABEL] Bool 2+2 mixed copyouts C/LLVM parity + negatives: PASS"
