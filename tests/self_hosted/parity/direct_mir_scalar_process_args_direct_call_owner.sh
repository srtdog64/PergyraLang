#!/usr/bin/env bash
# Registry-owned process Args nested in one direct call, C/LLVM exact parity.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-process-args-direct-call"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_process_args_direct_call"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_process_args_direct_call.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_process_args_direct_call_mutations.py"

KIND="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_external_runtime_expression_kind_owner.pgy"
SIGNATURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_collection_builtin_signature_owner.pgy"
REQUIREMENT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_host_io_runtime_requirement_owner.pgy"
C_SIGNATURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_signature_owner.pgy"
LLVM_SIGNATURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_llvm_signature_owner.pgy"
C_RUNTIME="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_process_args_materialization_owner.pgy"
LLVM_RUNTIME="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_process_args_materialization_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'func DirectMirScalarProgramExprProcessArgs() -> Int { return 94; }' "$KIND" ||
    fail "process Args expression identity is missing"
grep -Fq 'if name == "Args"' "$SIGNATURE" ||
    fail "Args does not consume the builtin signature owner"
grep -Fq '"host-io", "args"' "$REQUIREMENT" ||
    fail "Args does not consume the host-io runtime ABI operation"
grep -Fq 'HostIORuntimeCArgsFn()' "$REQUIREMENT" ||
    fail "Args does not consume the host-io runtime ABI row"
grep -Fq 'HostIORuntimeCEntrypointSignature(true)' "$C_SIGNATURE" ||
    fail "C entrypoint omitted process arguments"
grep -Fq 'define i32 @main(i32 %argc, ptr %argv)' "$LLVM_SIGNATURE" ||
    fail "LLVM entrypoint omitted process arguments"
grep -Fq 'memcpy(owned, source, length + 1)' "$C_RUNTIME" ||
    fail "C Args does not own copied argument strings"
grep -Fq '%copied = call ptr @memcpy' "$LLVM_RUNTIME" ||
    fail "LLVM Args does not own copied argument strings"
# Process Args consumes projected ArrayString storage alignment and rejects layout drift.
grep -Fq 'DirectMirScalarProgramArrayStringAbiProjectionReadyForFact(' "$C_RUNTIME" ||
    fail "C Args does not validate the ArrayString target projection"
grep -Fq 'DirectMirScalarProgramArrayStringAbiProjectionReadyForFact(' "$LLVM_RUNTIME" ||
    fail "LLVM Args does not validate the ArrayString target projection"
grep -Fq 'projection.storage.align' "$LLVM_RUNTIME" ||
    fail "LLVM Args does not consume projected ArrayString storage alignment"
! grep -Fq '%array = alloca %pgy.array.string, align 8' "$LLVM_RUNTIME" ||
    fail "LLVM Args restored literal ArrayString alloca alignment"
! grep -Fq 'store %pgy.array.string zeroinitializer, ptr %array, align 8' "$LLVM_RUNTIME" ||
    fail "LLVM Args restored literal ArrayString store alignment"
! grep -Fq '%result = load %pgy.array.string, ptr %array, align 8' "$LLVM_RUNTIME" ||
    fail "LLVM Args restored literal ArrayString load alignment"
grep -Fq 'DirectMirScalarProgramCProcessArgsBlock(plan, runtime, projection)' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_string_collection_materialization_owner.pgy" ||
    fail "C Args projection is not carried from the collection materializer"
grep -Fq 'DirectMirScalarProgramLlvmProcessArgsBlock(plan, runtime, abi)' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_collection_materialization_owner.pgy" ||
    fail "LLVM Args projection is not carried from the collection materializer"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"call_target_name":"Args"' "$MIR" ||
    fail "producer omitted the Args call identity"
grep -Fq '"call_target_name":"ConsumeArgs"' "$MIR" ||
    fail "producer omitted the outer direct-call identity"
printf 'alpha\n2\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    extension=c; [[ "$backend" == llvm ]] && extension=ll
    artifact_rel="$WORK_REL/program.$extension"
    artifact="$ROOT_DIR/$artifact_rel"
    binary="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" \
                "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq 'int main(int argc, char **argv)' "$artifact" ||
            fail "C artifact omitted argc/argv"
        grep -Fq 'static pgy_as pgy_selfhost_args(void)' "$artifact" ||
            fail "C artifact omitted the Args owner"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$binary")
        "${command[@]}" >"$WORK_DIR/c.compile.out" \
            2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Fq 'define i32 @main(i32 %argc, ptr %argv)' "$artifact" ||
            fail "LLVM artifact omitted argc/argv"
        grep -Fq 'define internal %pgy.array.string @pgy_selfhost_args()' \
            "$artifact" || fail "LLVM artifact omitted the Args owner"
        "$CLANG" -x ir "$artifact" -o "$binary" \
            >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$binary" alpha beta | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in args-target-name args-target-syntax outer-target-syntax \
    array-layout-align; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published $mutation"
    done
done

echo "[$LABEL] process Args nested direct-call C/LLVM parity + negatives: PASS"
