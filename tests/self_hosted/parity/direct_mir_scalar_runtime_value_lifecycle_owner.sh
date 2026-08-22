#!/usr/bin/env bash
# Canonical runtime-value representation and last-consumer proof reach C/LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-runtime-value-lifecycle"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_runtime_value_lifecycle"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_runtime_value_lifecycle.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_runtime_value_lifecycle_mutations.py"

REPRESENTATION="$ROOT_DIR/src/self_hosted/compiler/runtime_value_representation_owner.pgy"
CALL_ABI="$ROOT_DIR/src/self_hosted/compiler/runtime_value_call_abi_owner.pgy"
EXPRESSION_READY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_runtime_value_expression_readiness_owner.pgy"
BUILTIN_CALL="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_builtin_call_owner.pgy"
BUILTIN_IDENTITY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_builtin_runtime_call_identity_owner.pgy"
EXPRESSION_ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_admission_owner.pgy"
LIFECYCLE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_runtime_value_lifecycle_owner.pgy"
EXTENSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_extension_readiness_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_runtime_value_expression_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_runtime_value_expression_owner.pgy"
LLVM_PARAMETER_STORAGE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_llvm_runtime_value_parameter_storage_owner.pgy"
BUILTIN_MATERIALIZATION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_builtin_materialization_owner.pgy"
PROGRAM_C_EMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_emission_owner.pgy"
PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for path in "$REPRESENTATION" "$CALL_ABI" "$EXPRESSION_READY" \
        "$BUILTIN_CALL" "$BUILTIN_IDENTITY" "$EXPRESSION_ADMISSION" "$LIFECYCLE" \
        "$EXTENSION" "$C_OWNER" "$LLVM_OWNER" "$BUILTIN_MATERIALIZATION" \
        "$PROGRAM_C_EMISSION" "$LLVM_PARAMETER_STORAGE" "$PLAN" "$MUTATIONS"; do
    [[ -f "$path" ]] || fail "missing owner: ${path#"$ROOT_DIR/"}"
done
grep -Fq 'runtime_header_owns_print' "$BUILTIN_MATERIALIZATION" ||
    fail "C Print materialization ignores the runtime-header symbol owner"
grep -Fq 'CompilerRuntimeValueTypesPresent(plan.local_types, plan.routines.parameter_types)' \
        "$PROGRAM_C_EMISSION" ||
    fail "C program emission omits the runtime-value header receipt"
grep -Fq 'CompilerAbiLayoutOwnershipAllocatorLaneValue()' \
        "$LLVM_PARAMETER_STORAGE" ||
    fail "LLVM runtime-value parameter storage ignores the admitted ownership"
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'CompilerAbiLayoutRowIndex(type_name)' "$REPRESENTATION" ||
    fail "representation does not consume the canonical ABI-layout owner"
grep -Fq 'CompilerAbiLayoutRuntimeValueOnlyMaterialization()' "$REPRESENTATION" ||
    fail "representation admits a non-runtime materialization"
grep -Fq 'field_names: Array<String>' "$REPRESENTATION" ||
    fail "representation omitted canonical layout fields"
grep -Fq 'CompilerRuntimeValueRepresentationMatchesCapture(' \
        "$REPRESENTATION" ||
    fail "representation does not validate the carried MIR layout receipt"
grep -Fq 'CompilerRuntimeValueCallAbiRowReady(' "$CALL_ABI" ||
    fail "runtime-value call join does not validate the carried row"
grep -Fq 'MirAbiLayoutIdFromCapture(captured)' "$CALL_ABI" ||
    fail "runtime-value call join does not validate the layout identity"
grep -Fq 'ArrayLength(fact.parameter_types) == 0 { return ""; }' "$CALL_ABI" ||
    fail "runtime-value call join did not use the admitted empty parameter schema"
! grep -Fq 'ArrayLength(fact.parameter_types) == 0 { return "none"; }' "$CALL_ABI" ||
    fail "runtime-value call join reintroduced serialized none spelling"
grep -Fq 'The terminal CallArgument node owns admission for that call.' \
        "$BUILTIN_CALL" ||
    fail "nonzero runtime call marker is no longer deferred to its argument chain"
grep -Fq 'DirectMirScalarProgramBuiltinRuntimeCallIdentityReady(' "$BUILTIN_CALL" ||
    fail "builtin call admission bypasses the runtime-call identity owner"
grep -Fq 'DirectMirScalarProgramExprRuntimeValueCall()' "$BUILTIN_IDENTITY" &&
    grep -Fq 'signature.runtime_call_abi_id > 0 && carried == 0' \
        "$BUILTIN_IDENTITY" ||
    fail "runtime-value graph identity no longer defers to the instruction row"
grep -Fq 'builtin.call_node < 0 || builtin.call_node >= total_count' \
        "$EXPRESSION_ADMISSION" ||
    fail "zero-argument runtime call dereferences an unvalidated call node"
grep -Fq 'node_runtime_call_abi_ids[node]' "$EXPRESSION_READY" ||
    fail "expression readiness omitted the sealed runtime-call identity"
grep -Fq 'DirectMirScalarCfgRoutinePartitionLocalOwner(' "$LIFECYCLE" ||
    fail "lifecycle does not consume the routine partition owner"
grep -Fq 'plan.routines.operation_starts[routine]' "$LIFECYCLE" ||
    fail "lifecycle reintroduced a program-global per-local operation scan"
! grep -Eq 'routine.*name|source_name *==' "$LIFECYCLE" ||
    fail "lifecycle reopened routine or source spelling"
grep -Fq 'DirectMirScalarProgramRuntimeValueLifecycleReady(plan)' "$EXTENSION" ||
    fail "final GraphPlan readiness omitted runtime-value lifetime"
grep -Fq 'return 22;' "$EXTENSION" ||
    fail "runtime-value lifetime has no stable readiness code"
for path in "$C_OWNER" "$LLVM_OWNER"; do
    grep -Fq 'CompilerRuntimeValueCallAbiFactForId(' "$path" ||
        fail "$(basename "$path") re-inferred a runtime call"
done
grep -Fq 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v79' "$PLAN" ||
    fail "GraphPlan schema omitted runtime-call identity carriage"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
for term in '"type":"Allocator"' '"type":"TextBuilder"' \
        '"source":"AllocatorResult"' '"source":"AllocatorDestroy"' \
        '"source":"TextBuilderNew"' '"source":"TextBuilderAppend"' \
        '"source":"TextBuilderFinish"'; do
    grep -Fq "$term" "$MIR" || fail "producer omitted $term"
done
for term in '"abi_layout_id":722594115' \
        '"abi_layout_id":647731664' \
        '"call_target_kind":"direct"' \
        '"binding_kind":"none"'; do
    grep -Fq "$term" "$MIR" || fail "producer omitted $term"
done
! grep -Fq '"runtime_call_abi_required":true,"runtime_call_abi":{"owner":"Allocator"' "$MIR" ||
    fail "runtime-value call row reused the resource required-bit schema"

runtime_obj="$WORK_DIR/runtime.o"
"$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" \
    >"$WORK_DIR/runtime.compile.out" 2>"$WORK_DIR/runtime.compile.err" ||
    fail "runtime ABI object did not compile"

printf 'runtime-value\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    extension=c; [[ "$backend" == llvm ]] && extension=ll
    artifact_rel="$WORK_REL/program.$extension"
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
    for symbol in pgy_allocator_result pgy_allocator_destroy \
            pgy_text_builder_new pgy_text_builder_append pgy_text_builder_finish; do
        grep -Fq "$symbol" "$artifact" || fail "$backend omitted $symbol"
    done
    if [[ "$backend" == c ]]; then
        grep -Fq 'PgyAllocator pgy_local_' "$artifact" ||
            fail "C omitted Allocator local materialization"
        grep -Fq 'PgyTextBuilder pgy_local_' "$artifact" ||
            fail "C omitted TextBuilder local materialization"
        grep -Fq 'pgy_print(' "$artifact" ||
            fail "C omitted the runtime-header Print call"
        ! grep -Fq 'static void pgy_print(' "$artifact" ||
            fail "C duplicated the runtime-header Print definition"
        command=("$CC" -x c -std=c11 -fwrapv "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C artifact did not compile"
    else
        grep -Fq '%pgy.runtime.allocator = type' "$artifact" ||
            fail "LLVM omitted Allocator representation"
        grep -Fq '%pgy.runtime.text_builder = type' "$artifact" ||
            fail "LLVM omitted TextBuilder representation"
        "$CLANG" -x ir "$artifact" -x none "$runtime_obj" -pthread -lm \
            -o "$bin" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in wrong-layout wrong-graph-runtime-id wrong-runtime-row \
        wrong-runtime-parameter-carriage foreign-local \
        missing-terminal-destroy use-after-terminal; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        rm -f "$ROOT_DIR/$output_rel"
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
for backend in c llvm; do
    grep -Fq 'program_readiness=22' \
        "$WORK_DIR/missing-terminal-destroy.$backend.out" \
        "$WORK_DIR/missing-terminal-destroy.$backend.err" ||
        fail "$backend missing terminal did not reach lifecycle readiness"
    grep -Fq 'program_readiness=22' \
        "$WORK_DIR/use-after-terminal.$backend.out" \
        "$WORK_DIR/use-after-terminal.$backend.err" ||
        fail "$backend use-after-terminal did not reach lifecycle readiness"
done

echo "[$LABEL] C/LLVM runtime-value representation, lifecycle, and negatives: PASS"
