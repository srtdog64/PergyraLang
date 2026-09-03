#!/usr/bin/env bash
# Exact Void + Array<String> value-result lifecycle reaches C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-array-string-value-result-void"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_array_string_value_result_void"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_array_string_value_result_void.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"

ABI_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_abi_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_array_string_value_result_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_value_result_owner.pgy"
ROUTINE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_routine_admission_owner.pgy"
BUILTIN_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_collection_builtin_signature_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$ABI_OWNER" "$C_OWNER" "$LLVM_OWNER" "$ROUTINE_OWNER" "$BUILTIN_OWNER" \
    "$MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'value_result_routine_ordinals' "$ABI_OWNER" ||
    fail "Array<String> ABI does not carry value-result identity"
grep -Fq 'DirectMirScalarCfgVoidReturnExpressionRow()' "$ROUTINE_OWNER" ||
    fail "Void return edges are not represented by the GraphPlan owner"
grep -Fq 'DirectMirScalarProgramCArrayStringValueResultCopyOut(' "$C_OWNER" ||
    fail "C copy-out owner is missing"
grep -Fq 'DirectMirScalarProgramLlvmArrayStringValueResultCopyOut(' "$LLVM_OWNER" ||
    fail "LLVM copy-out owner is missing"
grep -Fq 'name == "ArrayPushOwnedString"' "$BUILTIN_OWNER" ||
    fail "owned String push is missing from the canonical builtin projection"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"type":"Array<String>","carriage":"value-result"' "$MIR" ||
    fail "producer emitted no value-result Array<String> parameter"
grep -Fq '"source_type":"AST_RETURN_VOID"' "$MIR" ||
    fail "producer emitted no explicit Void return"

printf '2\n2\nowned\nowned\n' >"$WORK_DIR/expected.run"
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
        grep -Fq 'pgy_as *pgy_param_1_mutref' "$artifact" ||
            fail "C signature omitted the value-result pointer"
        grep -Fq 'pgy_as pgy_param_1 = *pgy_param_1_mutref;' "$artifact" ||
            fail "C callable omitted Array<String> copy-in"
        [[ "$(grep -Fc '*pgy_param_1_mutref = pgy_param_1;' "$artifact")" -ge 3 ]] ||
            fail "C callable omitted early/fallthrough copy-out"
        grep -Fq 'pgy_as_push(&pgy_param_1,' "$artifact" ||
            fail "C callable did not mutate its copy-in carrier"
        grep -Fq 'pgy_as_push_owned(&pgy_param_0, pgy_param_1)' "$artifact" ||
            fail "C owned push did not preserve the owned-string ABI"
        grep -Fq 'pgy_scalar_routine_1(2LL, &pgy_local_' "$artifact" ||
            fail "C caller did not pass an addressable local"
        grep -Fq 'pgy_scalar_routine_1((pgy_param_0 - 1LL), &pgy_param_1)' \
            "$artifact" || fail "C recursive call did not forward the carrier"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C artifact did not compile"
    else
        grep -Fq '%pgy.array.string = type { ptr, i64, i64, ptr }' "$artifact" ||
            fail "LLVM artifact omitted Array<String> representation"
        grep -Fq 'ptr %pgy.param.1.mutref' "$artifact" ||
            fail "LLVM signature omitted the value-result pointer"
        grep -Fq '%pgy.param.1.local = alloca { ptr, i64, i64, ptr }, align 8' "$artifact" ||
            fail "LLVM callable omitted addressable copy-in storage"
        [[ "$(grep -Fc '.copyout.' "$artifact")" -ge 6 ]] ||
            fail "LLVM callable omitted early/fallthrough copy-out"
        grep -Fq 'call void @pgy_as_push(ptr %pgy.param.1.local' "$artifact" ||
            fail "LLVM callable did not mutate its copy-in carrier"
        grep -Fq 'call void @pgy_as_push_owned(ptr %pgy.param.0.local, ptr %pgy.param.1)' "$artifact" ||
            fail "LLVM owned push did not preserve the owned-string ABI"
        grep -Fq 'call void @pgy.scalar.routine.1(i64 2, ptr %pgy.local.' "$artifact" ||
            fail "LLVM caller did not pass an addressable local"
        grep -Fq 'ptr %pgy.param.1.local)' "$artifact" ||
            fail "LLVM recursive call did not forward the carrier"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" 2>"$WORK_DIR/$backend.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in array-string-value-result-abi-layout \
    array-string-value-result-carriage owned-string-push-carriage; do
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

echo "[$LABEL] Void + Array<String> value-result copy lifecycle and negatives: PASS"
