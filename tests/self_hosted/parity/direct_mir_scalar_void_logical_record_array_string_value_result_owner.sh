#!/usr/bin/env bash
# Exact Void logical-record value plus Array<String> copyout C/LLVM boundary.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-void-record-array-string-copyout"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_void_record_array_string_copyout"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_void_logical_record_array_string_value_result.pgy"
MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_void_logical_record_array_string_value_result_policy_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_void_logical_record_array_string_value_result_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'string_count != 0 && string_count != 3 && string_count != 4' "$POLICY" || fail "policy omitted exact String-tail family"
grep -Fq 'CompilerAbiLayoutArrayStringTypeName()' "$POLICY" || fail "policy omitted ArrayString copyout"
grep -Fq 'DirectMirScalarProgramLogicalRecordTypeReady(' "$POLICY" || fail "policy omitted declaration-keyed record value"

mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || {
    cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
    fail "MIR production failed"
}
grep -Fq '"name":"StoreState"' "$MIR" || fail "producer omitted callable"
grep -Fq '"type":"Array<String>","carriage":"value-result"' "$MIR" || fail "producer omitted ArrayString copyout"
grep -Fq '"type":"StateEnvProbe","carriage":"value"' "$MIR" || fail "producer omitted logical-record value"
printf 'void-record-array-string-copyout-ready\n0\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"; artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" 2>"$WORK_DIR/$backend.project.err" || {
        cat "$WORK_DIR/$backend.project.out" "$WORK_DIR/$backend.project.err" >&2
        fail "$backend projection failed"
    }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        signature="$(grep -E '^static void pgy_scalar_routine_[0-9]+\(' "$artifact" | grep -F 'pgy_param_0_mutref' | head -n 1)"
        [[ "$signature" =~ pgy_as[[:space:]]+\*pgy_param_0_mutref ]] || fail "C omitted ArrayString copyout ordinal 0"
        [[ "$signature" =~ pgy_scalar_logical_record_value_[0-9]+[[:space:]]+pgy_param_1 ]] || fail "C omitted record value ordinal 1"
        grep -Fq 'pgy_as pgy_param_0 = *pgy_param_0_mutref;' "$artifact" || fail "C omitted copy-in"
        [[ "$(grep -Fc '*pgy_param_0_mutref = pgy_param_0;' "$artifact")" -ge 2 ]] || fail "C omitted early/final copyout"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread); fi
        command+=(-lm -o "$bin"); "${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Eq 'define internal void @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0\.mutref, %pgy\.scalar\.logical\.record\.value\.[0-9]+ %pgy\.param\.1\)' "$artifact" || fail "LLVM omitted exact signature"
        grep -Fq '%pgy.param.0.local = alloca %pgy.array.string' "$artifact" || fail "LLVM omitted copy-in"
        [[ "$(grep -Fc '%pgy.param.0.copyout.' "$artifact")" -ge 4 ]] || fail "LLVM omitted early/final copyout"
        "$CLANG" -x ir "$artifact" -o "$bin" >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" || fail "$backend runtime output drifted"
done

for mutation in copyout-carriage copyout-type copyout-pass copyout-abi copyout-layout record-type record-carriage record-abi return-type parameter-count; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"; rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" 2>"$WORK_DIR/$mutation.$backend.err"; then fail "$backend accepted $mutation"; fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] Void record value + ArrayString copyout C/LLVM parity + negatives: PASS"
