#!/usr/bin/env bash
# Exact Set<String> value/value-result ABI and runtime calls reach C/LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-set-string-value-parameter"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_set_string_value_parameter"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_set_string_value_parameter.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"
ABI="$ROOT_DIR/src/self_hosted/compiler/abi_layout_row_owner.pgy"
RUNTIME="$ROOT_DIR/src/self_hosted/codegen/runtime_abi/set_runtime_owner.pgy"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_policy_owner.pgy"
ROLE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_role_plan_owner.pgy"
VALUE_RESULT_POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_set_string_value_result_policy_owner.pgy"
VALUE_RESULT_TARGET="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_set_string_value_result_target_owner.pgy"
C_VALUE_RESULT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_set_string_value_result_owner.pgy"
LLVM_VALUE_RESULT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_set_string_value_result_owner.pgy"
SIGNATURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_collection_builtin_signature_owner.pgy"
READINESS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_collection_expression_readiness_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_set_string_expression_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_set_string_expression_owner.pgy"
LLVM_DECL="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_foreign_declaration_owner.pgy"
MATERIALIZATION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_runtime_materialization_requirement_owner.pgy"
C_EMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_emission_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$ABI" "$RUNTIME" "$POLICY" "$ROLE" \
        "$VALUE_RESULT_POLICY" "$VALUE_RESULT_TARGET" \
        "$C_VALUE_RESULT" "$LLVM_VALUE_RESULT" "$SIGNATURE" \
        "$READINESS" "$C_OWNER" "$LLVM_OWNER" "$LLVM_DECL" \
        "$MATERIALIZATION" "$C_EMISSION" "$MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

for term in 'CompilerAbiLayoutSetStringTypeName()' \
        'CompilerAbiLayoutSetStringCValueType()' \
        'CompilerAbiLayoutFieldOrderSetBuffer()'; do
    grep -Fq "$term" "$ABI" || fail "Set<String> ABI row omits: $term"
done
grep -Fq 'type_name == CompilerAbiLayoutSetStringTypeName()' "$POLICY" ||
    fail "callable policy omits by-value Set<String>"
grep -Fq 'signature.parameters.type_names[ordinal] == CompilerAbiLayoutSetStringTypeName()' "$ROLE" ||
    fail "parameter-role plan omits by-value Set<String>"
grep -Fq 'DirectMirScalarProgramSetStringValueResultParameterReady(' "$POLICY" ||
    fail "callable policy bypasses the Set<String> value-result owner"
grep -Fq 'set_string_value_result_count' "$ROLE" ||
    fail "parameter-role plan omits Set<String> value-result"
grep -Fq 'DirectMirScalarProgramSetStringValueResultAt(' "$VALUE_RESULT_TARGET" ||
    fail "GraphPlan target owner omits Set<String> value-result"
grep -Fq 'DirectMirScalarProgramCSetStringValueResultCopyOut(' "$C_VALUE_RESULT" ||
    fail "C lifecycle owner omits Set<String> copy-out"
grep -Fq 'DirectMirScalarProgramLlvmSetStringValueResultCopyOut(' "$LLVM_VALUE_RESULT" ||
    fail "LLVM lifecycle owner omits Set<String> copy-out"
for name in SetNew SetAdd SetHas; do
    grep -Fq "name == \"$name\"" "$SIGNATURE" ||
        fail "builtin signature owner omits $name"
done
for term in DirectMirScalarProgramExprSetStringNew DirectMirScalarProgramExprSetStringAdd \
        DirectMirScalarProgramExprSetStringHas; do
    grep -Fq "$term" "$READINESS" || fail "expression readiness omits: $term"
done
for term in CollectionSetRuntimeLlvmNewRawFn CollectionSetRuntimeLlvmAddStringRawFn \
        CollectionSetRuntimeLlvmHasStringRawFn; do
    grep -Fq "$term" "$LLVM_OWNER" || fail "LLVM expression owner omits: $term"
    grep -Fq "$term" "$LLVM_DECL" || fail "LLVM declarations omit: $term"
done
grep -Fq 'CollectionSetRuntimeFactFromTypeName(' "$C_OWNER" ||
    fail "C expression owner bypasses the canonical Set runtime fact"
grep -Fq 'func DirectMirScalarProgramSetStringRuntimeHeaderRequired(' \
    "$MATERIALIZATION" || fail "Set<String> runtime-header requirement is missing"
grep -Fq 'DirectMirScalarProgramSetStringRuntimeHeaderRequired(' "$C_EMISSION" ||
    fail "C emission bypasses the Set<String> runtime-header requirement"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"type":"Set<String>","carriage":"value","resource":"none","pass":"direct","abi_type_name":"Set<String>","abi_layout_id":0,"abi_layout_required":false,"abi_layout":null' "$MIR" ||
    fail "producer omitted the by-value Set<String> no-layout receipt"
grep -Fq '"type":"Set<String>","carriage":"value-result","resource":"none","pass":"direct","abi_type_name":"Set<String>","abi_layout_id":0,"abi_layout_required":false,"abi_layout":null' "$MIR" ||
    fail "producer omitted the value-result Set<String> no-layout receipt"
for target in SetNew SetAdd SetHas AddPath ContainsPath; do
    grep -Fq "\"call_target_name\":\"$target\"" "$MIR" ||
        fail "producer omitted call target $target"
done
printf 'set-string-value-ready\n' >"$WORK_DIR/expected.run"

runtime_obj="$WORK_DIR/runtime.o"
"$CLANG" -std=c11 -DPGY_LLVM_ENABLED \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" \
    >"$WORK_DIR/runtime.compile.out" 2>"$WORK_DIR/runtime.compile.err" ||
    fail "canonical runtime object did not compile"

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
        grep -Eq 'static bool pgy_scalar_routine_[0-9]+\(PgySet_String pgy_param_0, const char\* pgy_param_1\)' "$artifact" ||
            fail "C signature omitted the by-value Set<String> carrier"
        grep -Fq '#include "pgy_runtime.h"' "$artifact" ||
            fail "C omitted the canonical Set<String> runtime header"
        grep -Fq 'pgy_set_new_string()' "$artifact" ||
            fail "C omitted canonical Set<String> construction"
        grep -Fq 'pgy_set_add_string(&pgy_local_' "$artifact" ||
            grep -Fq 'pgy_set_add_string(&pgy_param_0' "$artifact" ||
            fail "C omitted addressable Set<String> value-result mutation"
        grep -Fq 'pgy_set_has_string(&pgy_param_0, pgy_param_1)' "$artifact" ||
            fail "C omitted by-value Set<String> lookup"
        grep -Eq 'static void pgy_scalar_routine_[0-9]+\(PgySet_String \*pgy_param_0_mutref, const char\* pgy_param_1\)' "$artifact" ||
            fail "C signature omitted the Set<String> value-result pointer"
        grep -Fq 'PgySet_String pgy_param_0 = *pgy_param_0_mutref;' "$artifact" ||
            fail "C omitted Set<String> value-result copy-in"
        grep -Fq '*pgy_param_0_mutref = pgy_param_0;' "$artifact" ||
            fail "C omitted Set<String> value-result copy-out"
        grep -Eq 'pgy_scalar_routine_[0-9]+\(&pgy_local_[0-9]+, "alpha"\)' "$artifact" ||
            fail "C direct call omitted the Set<String> address"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
    else
        grep -Eq 'define internal i1 @pgy\.scalar\.routine\.[0-9]+\(\{ ptr, ptr, i64, i64 \} %pgy\.param\.0, ptr %pgy\.param\.1\)' "$artifact" ||
            fail "LLVM signature omitted the by-value Set<String> carrier"
        grep -Fq 'call void @pgy_set_new_raw_export' "$artifact" ||
            fail "LLVM omitted canonical Set<String> construction"
        grep -Fq 'call void @pgy_set_add_string_raw_export' "$artifact" ||
            fail "LLVM omitted canonical Set<String> mutation"
        grep -Fq 'call i1 @pgy_set_has_string_raw_export' "$artifact" ||
            fail "LLVM omitted canonical Set<String> lookup"
        grep -Eq 'define internal void @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0\.mutref, ptr %pgy\.param\.1\)' "$artifact" ||
            fail "LLVM signature omitted the Set<String> value-result pointer"
        grep -Fq '%pgy.param.0.local = alloca { ptr, ptr, i64, i64 }, align 8' "$artifact" ||
            fail "LLVM omitted Set<String> value-result local storage"
        grep -Fq 'ptr %pgy.param.0.local' "$artifact" ||
            fail "LLVM Set mutation bypassed value-result local storage"
        grep -Fq 'ptr %pgy.param.0.mutref, align 8' "$artifact" ||
            fail "LLVM omitted Set<String> value-result copy-out"
        grep -Eq 'call void @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.local\.[0-9]+, ptr @pgy\.scalar\.string\.' "$artifact" ||
            fail "LLVM direct call omitted the Set<String> address"
        command=("$CLANG" -x ir "$artifact" -x none "$runtime_obj" \
            -pthread -lm -o "$bin")
    fi
    "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
        2>"$WORK_DIR/$backend.compile.err" || {
            cat "$WORK_DIR/$backend.compile.err" >&2
            fail "$backend artifact did not compile"
        }
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in set-string-value-carriage set-string-value-pass-shape \
        set-string-value-type set-string-value-abi-required \
        set-string-value-result-carriage \
        set-string-value-result-pass-shape \
        set-string-value-result-resource set-string-value-result-type \
        set-string-value-result-abi-required \
        set-string-has-call-target; do
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
            fail "$backend published an artifact for $mutation"
    done
done

echo "[$LABEL] Set<String> value/value-result C/LLVM parity + negatives: PASS"
