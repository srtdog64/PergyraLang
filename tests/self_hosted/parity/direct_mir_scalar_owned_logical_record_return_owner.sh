#!/usr/bin/env bash
# Exact owner-handle logical-record transfer/return C and LLVM boundary.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-owned-logical-record-return"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_owned_logical_record_return"; WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_owned_logical_record_return.pgy"; MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_logical_record_return_policy_owner.pgy"; SIGNATURE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_owner.pgy"
CALL_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_call_expression_admission_owner.pgy"; LLVM_CALL_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_direct_call_expression_owner.pgy"
LLVM_RECORD_ARGUMENT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_readonly_ref_parameter_argument_owner.pgy"; LLVM_RECORD_MEMBER_ARGUMENT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_readonly_ref_member_argument_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_owned_logical_record_return_mutations.py"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'signature.param_count != 1' "$POLICY" || fail "parameter count is not exact"
grep -Fq '== "owner-handle"' "$POLICY" || fail "owner carriage is not exact"
grep -Fq 'type_names[0] != signature.return_type' "$POLICY" || fail "return identity is not exact"
grep -Fq 'signature.parameters.carriages[0] == "value-result"' "$SIGNATURE_OWNER" ||
    fail "same-record return boundary is not explicit"
grep -Fq 'callables.parameter_carriages[parameter_row] != "owner-handle"' "$CALL_OWNER" ||
    fail "direct-call admission omitted the admitted owner handle"
grep -Fq 'carriages[ordinal] == "owner-handle"' "$LLVM_CALL_OWNER" ||
    fail "LLVM direct-call projection omitted the owner handle"
grep -Fq 'routines.parameter_carriages[source_row] == "owner-handle"' \
    "$LLVM_RECORD_ARGUMENT_OWNER" ||
    fail "LLVM readonly borrow omitted the source owner handle"
grep -Fq 'routines.parameter_carriages[source_row] == "owner-handle"' \
    "$LLVM_RECORD_MEMBER_ARGUMENT_OWNER" ||
    fail "LLVM member borrow omitted the source owner handle"
mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") \
    >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"Schedule"' "$MIR" || fail "producer omitted callable"
grep -Fq '"type":"ScheduleGraph","carriage":"owner-handle"' "$MIR" ||
    fail "producer omitted owner handle"
grep -Fq '"return":"ScheduleGraph"' "$MIR" || fail "producer omitted return identity"
grep -Fq '"text":"Schedule(graph)"' "$MIR" || fail "producer omitted owner call"
grep -Fq '"text":"ScheduleGraphReady(graph)"' "$MIR" ||
    fail "producer omitted owner-handle to readonly-ref borrow"
grep -Fq '"text":"!graph.valid"' "$MIR" ||
    fail "producer omitted owner-handle member condition"
grep -Fq '"text":"ScheduleStatusReady(graph.status)"' "$MIR" ||
    fail "producer omitted owner-handle member readonly borrow"
printf 'owned-logical-record-return-ready\n' >"$WORK_DIR/expected.run"

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
        grep -Eq 'static pgy_scalar_logical_record_value_[0-9]+ pgy_scalar_routine_[0-9]+\(pgy_scalar_logical_record_value_[0-9]+ pgy_param_0\)' "$artifact" ||
            fail "C omitted the by-value owner signature"
        grep -Fq 'return pgy_param_0;' "$artifact" || fail "C omitted same-record return"
        grep -Eq 'pgy_scalar_routine_[0-9]+\(pgy_local_[0-9]+\)' "$artifact" ||
            fail "C omitted the owner-handle direct call"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin"); "${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" ||
            fail "C artifact did not compile"
    else
        grep -Eq 'define internal %pgy\.scalar\.logical\.record\.value\.[0-9]+ @pgy\.scalar\.routine\.[0-9]+\(%pgy\.scalar\.logical\.record\.value\.[0-9]+ %pgy\.param\.0\)' "$artifact" ||
            fail "LLVM omitted the by-value owner signature"
        grep -Eq 'ret %pgy\.scalar\.logical\.record\.value\.[0-9]+ %pgy\.param\.0' "$artifact" ||
            fail "LLVM omitted same-record return"
        grep -Eq 'call %pgy\.scalar\.logical\.record\.value\.[0-9]+ @pgy\.scalar\.routine\.[0-9]+\(%pgy\.scalar\.logical\.record\.value\.[0-9]+ ' "$artifact" ||
            fail "LLVM omitted the owner-handle direct call"
        grep -Eq 'store %pgy\.scalar\.logical\.record\.value\.[0-9]+ %pgy\.param\.0, ptr %pgy\.readonly\.value\.[0-9]+' "$artifact" ||
            fail "LLVM omitted owner-handle readonly borrow storage"
        grep -Eq 'getelementptr inbounds %pgy\.scalar\.logical\.record\.value\.[0-9]+, ptr %pgy\.readonly\.receiver\.[0-9]+' "$artifact" ||
            fail "LLVM omitted owner-handle member readonly borrow"
        "$CLANG" -x ir "$artifact" -o "$bin" >"$WORK_DIR/llvm.compile.out" \
            2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in parameter-type carriage-invalid carriage-copyout \
    parameter-pass parameter-resource parameter-abi return-type return-abi \
    parameter-count call-target-missing call-target-foreign; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"; rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" 2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] owner-handle logical-record return C/LLVM parity + negatives: PASS"
