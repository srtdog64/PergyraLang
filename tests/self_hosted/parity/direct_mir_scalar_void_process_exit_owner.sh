#!/usr/bin/env bash
# Void scalar callable preserves ordered Log then Exit in C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-void-process-exit"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_void_process_exit"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_void_scalar_process_exit.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_void_process_exit_mutations.py"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_role_plan_owner.pgy"
SIGNATURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_owner.pgy"
ROUTE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_route_envelope_owner.pgy"
PROCESS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_process_exit_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'signature.return_type == "Void"' "$SIGNATURE" ||
    fail "direct scalar signature owner omitted Void"
grep -Fq 'DirectMirScalarProgramCallableParameterRolePlanFromSignature(' "$POLICY" ||
    fail "direct scalar signature owner omitted the shared parameter-role plan"
! grep -Fq 'signature.param_count < 1' "$POLICY" ||
    fail "Void scalar signature owner still rejects zero parameters"
grep -Fq 'DirectMirScalarProgramComposableCallableSignatureReady(' "$ROUTE" ||
    fail "callable route envelope does not consume the composable role plan"
grep -Fq 'DirectMirScalarCfgOpProcessExit()' "$PROCESS" ||
    fail "process-exit operation inventory is missing"
grep -Fq 'DirectMirScalarProgramBlockEndsWithProcessExit(' "$PROCESS" ||
    fail "terminal return readiness has no exact process-exit receipt"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"name":"FailClosed"' "$MIR" || fail "producer omitted callable"
grep -Fq '"name":"RequireProbe"' "$MIR" ||
    fail "producer omitted zero-parameter Void callable"
grep -Fq '"name":"RequiredName"' "$MIR" ||
    fail "producer omitted non-Void process-exit terminal callable"
grep -Fq '"arg0":"Exit"' "$MIR" || fail "producer omitted Exit statement"
printf 'fail-closed: probe\n' >"$WORK_DIR/expected.run"

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
        grep -Eq '^static void pgy_scalar_routine_[0-9]+\(void\)' "$artifact" ||
            fail "C omitted zero-parameter Void signature"
        grep -Fq 'exit((int)(7LL));' "$artifact" ||
            fail "C process-exit projection drifted"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >/dev/null 2>"$WORK_DIR/c.compile.err" ||
            fail "C artifact did not compile"
    else
        grep -Eq '^define internal void @pgy\.scalar\.routine\.[0-9]+\(\)' "$artifact" ||
            fail "LLVM omitted zero-parameter Void signature"
        grep -Fq 'declare void @exit(i32)' "$artifact" ||
            fail "LLVM process-exit declaration drifted"
        grep -Fq '  unreachable' "$artifact" ||
            fail "LLVM process-exit terminal omitted unreachable"
        grep -Eq '%pgy\.exit\.code\.[0-9]+ = trunc i64 7 to i32' "$artifact" ||
            fail "LLVM process-exit value projection drifted"
        "$CLANG" -x ir "$artifact" -o "$bin" >/dev/null \
            2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    set +e
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    status=$?
    set -e
    [[ "$status" -eq 7 ]] || fail "$backend exited with $status instead of 7"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend output/order drifted"
done

for mutation in scalar-carriage exit-value-type \
        zero-void-return-type zero-void-phantom-param \
        nonvoid-terminal-not-exit; do
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

echo "[$LABEL] zero-or-more-parameter Void scalar callable + ordered Log/Exit C/LLVM parity: PASS"
