#!/usr/bin/env bash
# Complete role-plan proof for a logical-record return and three copyouts.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-composable-logical-record-return"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_composable_logical_record_return"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_composable_logical_record_return.pgy"
MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_composable_logical_record_return_mutations.py"
SIGNATURE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_owner.pgy"
ARRAY_STRING_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_abi_owner.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
for tool in "$CC" "$CLANG" python; do command -v "$tool" >/dev/null 2>&1 || fail "missing tool: $tool"; done
grep -Fq 'DirectMirScalarProgramLogicalRecordTypeReady(' "$SIGNATURE_OWNER" || fail "logical-record return bypasses the role plan"
grep -Fq 'logical_record_return && !composable_callable' "$SIGNATURE_OWNER" || fail "specialized record policy still overrides the composable plan"
grep -Fq 'DirectMirScalarProgramComposableCallableSignatureReady(signature, logical_record, payload_free_enum)' "$ARRAY_STRING_OWNER" || fail "Array<String> return ABI bypasses the composable plan"
mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || { cat "$WORK_DIR/producer.err" >&2; fail "MIR production failed"; }
grep -Fq '"name":"AdvanceProbe"' "$MIR" || fail "producer omitted callable"
grep -Fq '"name":"EmptyActuals"' "$MIR" || fail "producer omitted composable Array<String> return"
[[ "$(grep -Fo '"type":"Array<String>","carriage":"value-result"' "$MIR" | wc -l)" -eq 3 ]] || fail "producer omitted the three copyouts"
printf 'composable-cursor-ready\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"; artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" 2>"$WORK_DIR/$backend.project.err" || { cat "$WORK_DIR/$backend.project.err" >&2; fail "$backend projection failed"; }
    [[ -s "$artifact" ]] || fail "$backend emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Eq '^static pgy_scalar_logical_record_value_[0-9]+ pgy_scalar_routine_[0-9]+\(pgy_scalar_logical_record_value_[0-9]+ pgy_param_0' "$artifact" || fail "C omitted the logical-record value signature"
        for ordinal in 6 7 8; do grep -Fq "pgy_as *pgy_param_${ordinal}_mutref" "$artifact" || fail "C omitted copyout $ordinal"; done
        command=("$CC" -x c -std=c11 "$artifact"); if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread); fi; command+=(-lm -o "$bin"); "${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Eq '^define internal %pgy\.scalar\.logical\.record\.value\.[0-9]+ @pgy\.scalar\.routine\.[0-9]+\(%pgy\.scalar\.logical\.record\.value\.[0-9]+ %pgy\.param\.0' "$artifact" || fail "LLVM omitted the logical-record value signature"
        for ordinal in 6 7 8; do grep -Fq "%pgy.param.$ordinal.copyout." "$artifact" || fail "LLVM omitted copyout $ordinal"; done
        "$CLANG" -x ir "$artifact" -o "$bin" >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"; cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" || fail "$backend runtime drifted"
done
for mutation in copyout-pass record-carriage return-identity; do
    mutated_rel="$WORK_REL/$mutation.mir.json"; python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do output_rel="$WORK_REL/$mutation.$backend"; if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" 2>"$WORK_DIR/$mutation.$backend.err"; then fail "$backend accepted $mutation"; fi; done
done
echo "[$LABEL] logical-record return + five records + three copyouts C/LLVM parity/negatives: PASS"
