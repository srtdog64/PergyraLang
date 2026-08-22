#!/usr/bin/env bash
# Int return plus one-or-more Array<String> copyouts, both backends and negatives.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-int-two-array-string-copyout"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_int_two_array_string_copyout"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_int_two_array_string_value_result.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_policy_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_int_two_array_string_value_result_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'let int_return: Bool = signature.return_type ==' "$POLICY" ||
    fail "Int return is not part of the shared ArrayString policy"
grep -Fq 'value_result_layout != layout' "$POLICY" ||
    fail "copyout ABI identity equality is not pinned"
! grep -Fq 'signature.param_count != 4' "$POLICY" || fail "retired exact arity remains"
! grep -Fq 'ordinal < 2' "$POLICY" || fail "retired positional policy remains"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"OwnerGraphNodeIndexOrAppend"' "$MIR" ||
    fail "producer omitted the two-copyout callable"
grep -Fq '"name":"EmitDeclFields"' "$MIR" ||
    fail "producer omitted the production-shaped three-copyout callable"
[[ "$(grep -o '"type":"Array<String>","carriage":"value-result"' "$MIR" | wc -l)" -eq 5 ]] ||
    fail "producer omitted the five Array<String> copyout rows"
printf '0\n0\n0\n3\n1\n1\n1\n' >"$WORK_DIR/expected.run"

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
        grep -Eq 'static int32_t pgy_scalar_routine_[0-9]+\(pgy_as \*pgy_param_0_mutref, pgy_as \*pgy_param_1_mutref' "$artifact" ||
            fail "C omitted the exact Int/two-copyout signature"
        for parameter in 0 1; do
            grep -Fq "pgy_as pgy_param_${parameter} = *pgy_param_${parameter}_mutref;" "$artifact" ||
                fail "C omitted copy-in $parameter"
            [[ "$(grep -Fc "*pgy_param_${parameter}_mutref = pgy_param_${parameter};" "$artifact")" -ge 2 ]] ||
                fail "C omitted early/final copyout $parameter"
        done
        for parameter in 5 6 7; do
            grep -Fq "pgy_as *pgy_param_${parameter}_mutref" "$artifact" ||
                fail "C omitted production copyout $parameter"
        done
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" ||
            fail "C artifact did not compile"
    else
        grep -Eq 'define internal i64 @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0\.mutref, ptr %pgy\.param\.1\.mutref' "$artifact" ||
            fail "LLVM omitted the exact Int/two-copyout signature"
        for parameter in 0 1; do
            grep -Fq "%pgy.param.${parameter}.local = alloca %pgy.array.string" "$artifact" ||
                fail "LLVM omitted copy-in storage $parameter"
            [[ "$(grep -Fc "%pgy.param.${parameter}.copyout." "$artifact")" -ge 4 ]] ||
                fail "LLVM omitted early/final copyout $parameter"
        done
        for parameter in 5 6 7; do
            grep -Fq "%pgy.param.${parameter}.copyout." "$artifact" ||
                fail "LLVM omitted production copyout $parameter"
        done
        "$CLANG" -x ir "$artifact" -o "$bin" >"$WORK_DIR/llvm.compile.out" \
            2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in first-copyout-type second-copyout-carriage copyout-pass \
    copyout-abi copyout-layout-mismatch string-carriage return-type \
    production-copyout-carriage production-copyout-abi \
    production-copyout-layout production-scalar-carriage; do
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

echo "[$LABEL] Int + one-or-more Array<String> copyouts C/LLVM parity + negatives: PASS"
