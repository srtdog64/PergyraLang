#!/usr/bin/env bash
# Bool-returning ArrayBool-bearing collection copyout C/LLVM parity.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-bool-mixed-collection-value-result"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_bool_mixed_collection_value_result"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_bool_mixed_collection_value_result.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_bool_mixed_collection_value_result_policy_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_bool_mixed_collection_value_result_mutations.py"
TARGET="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_mutation_target_owner.pgy"
OPCODES="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_op_code_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'return array_bool_count >= 1;' "$POLICY" ||
    fail "policy omitted the Array<Bool> owner cardinality"
grep -Fq 'DirectMirScalarCfgScalarTypeSupported(type_name)' "$POLICY" ||
    fail "policy omitted scalar value validation"
if grep -Fq 'signature.param_count != 11' "$POLICY" ||
    grep -Fq 'ordinal == 1' "$POLICY"; then
    fail "retired 5+1+2 positional policy remains"
fi
grep -Fq 'DirectMirScalarCfgOpArrayBoolPush() -> Int { return 39; }' "$OPCODES" ||
    fail "Array<Bool> push identity is not target-neutral"
grep -Fq 'DirectMirScalarCfgOpArrayBoolSet() -> Int { return 42; }' "$OPCODES" ||
    fail "Array<Bool> set identity is not target-neutral"
! grep -Fq 'DirectMirScalarCfgOpLocalArrayBool' "$TARGET" ||
    fail "target owner retained local-only Array<Bool> operation identity"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"AppendLaneRows"' "$MIR" ||
    fail "producer omitted the exact callable"
grep -Fq '"name":"ReadRequiredBool"' "$MIR" ||
    fail "producer omitted the single Array<Bool> callable"
[[ "$(grep -Fo '"type":"Array<Int>","carriage":"value-result"' "$MIR" | wc -l)" == 5 ]] ||
    fail "producer omitted an Array<Int> copyout"
[[ "$(grep -Fo '"type":"Array<Bool>","carriage":"value-result"' "$MIR" | wc -l)" == 2 ]] ||
    fail "producer omitted the complex and single Array<Bool> copyouts"
[[ "$(grep -Fo '"type":"Array<String>","carriage":"value-result"' "$MIR" | wc -l)" == 2 ]] ||
    fail "producer omitted an Array<String> copyout"
grep -Fq '"expr0":"ArrayPush(node_flags, false)"' "$MIR" ||
    fail "producer omitted the Array<Bool> value-result push"
grep -Fq '"expr0":"ArraySet(node_flags, 0, true)"' "$MIR" ||
    fail "producer omitted the Array<Bool> value-result set"
printf 'bool-mixed-collection-copyout-ready\nbool-single-copyout-ready\n1\n' >"$WORK_DIR/expected.run"

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
        grep -Eq 'static bool pgy_scalar_routine_[0-9]+\(pgy_ai \*pgy_param_0_mutref, pgy_ab \*pgy_param_1_mutref, pgy_ai \*pgy_param_2_mutref, pgy_as \*pgy_param_3_mutref, pgy_ai \*pgy_param_4_mutref, pgy_ai \*pgy_param_5_mutref, pgy_ai \*pgy_param_6_mutref, pgy_as \*pgy_param_7_mutref, bool pgy_param_8, bool pgy_param_9, const char\* pgy_param_10\)' "$artifact" ||
            fail "C artifact omitted the exact mixed signature"
        for parameter in 0 2 4 5 6; do
            grep -Fq "pgy_ai pgy_param_${parameter} = *pgy_param_${parameter}_mutref;" "$artifact" ||
                fail "C omitted Array<Int> copy-in $parameter"
            grep -Fq "*pgy_param_${parameter}_mutref = pgy_param_${parameter};" "$artifact" ||
                fail "C omitted Array<Int> copy-out $parameter"
        done
        grep -Fq 'pgy_ab pgy_param_1 = *pgy_param_1_mutref;' "$artifact" ||
            fail "C omitted Array<Bool> copy-in"
        grep -Fq '*pgy_param_1_mutref = pgy_param_1;' "$artifact" ||
            fail "C omitted Array<Bool> copy-out"
        grep -Fq 'pgy_ab pgy_param_4 = *pgy_param_4_mutref;' "$artifact" ||
            fail "C omitted single Array<Bool> copy-in"
        grep -Fq '*pgy_param_4_mutref = pgy_param_4;' "$artifact" ||
            fail "C omitted single Array<Bool> copy-out"
        grep -Fq 'pgy_ab_push(&pgy_param_1, (long long)(false));' "$artifact" ||
            fail "C omitted Array<Bool> value-result push"
        grep -Fq 'pgy_ab_set(&pgy_param_1' "$artifact" ||
            fail "C omitted Array<Bool> value-result set"
        for parameter in 3 7; do
            grep -Fq "pgy_as pgy_param_${parameter} = *pgy_param_${parameter}_mutref;" "$artifact" ||
                fail "C omitted Array<String> copy-in $parameter"
            grep -Fq "*pgy_param_${parameter}_mutref = pgy_param_${parameter};" "$artifact" ||
                fail "C omitted Array<String> copy-out $parameter"
        done
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/c.compile.out" \
            2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Eq 'define internal i1 @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0\.mutref, ptr %pgy\.param\.1\.mutref, ptr %pgy\.param\.2\.mutref, ptr %pgy\.param\.3\.mutref, ptr %pgy\.param\.4\.mutref, ptr %pgy\.param\.5\.mutref, ptr %pgy\.param\.6\.mutref, ptr %pgy\.param\.7\.mutref, i1 %pgy\.param\.8, i1 %pgy\.param\.9, ptr %pgy\.param\.10\)' "$artifact" ||
            fail "LLVM artifact omitted the exact mixed signature"
        for parameter in 0 2 4 5 6; do
            grep -Fq "%pgy.param.${parameter} = load %pgy.array.int" "$artifact" ||
                fail "LLVM omitted Array<Int> copy-in $parameter"
            grep -Fq "store %pgy.array.int %pgy.param.${parameter}, ptr %pgy.param.${parameter}.mutref" "$artifact" ||
                fail "LLVM omitted Array<Int> copy-out $parameter"
        done
        grep -Fq '%pgy.param.1 = load %pgy.array.bool' "$artifact" ||
            fail "LLVM omitted Array<Bool> copy-in"
        grep -Fq '%pgy.param.1.local = alloca %pgy.array.bool' "$artifact" ||
            fail "LLVM omitted mutable Array<Bool> copy-in storage"
        grep -Eq '%pgy\.param\.1\.copyout\.[0-9]+ = load %pgy\.array\.bool, ptr %pgy\.param\.1\.local' "$artifact" ||
            fail "LLVM omitted latest Array<Bool> mutation value"
        grep -Eq 'store %pgy\.array\.bool %pgy\.param\.1\.copyout\.[0-9]+, ptr %pgy\.param\.1\.mutref' "$artifact" ||
            fail "LLVM omitted Array<Bool> copy-out"
        grep -Fq '%pgy.param.4.local = alloca %pgy.array.bool' "$artifact" ||
            fail "LLVM omitted single Array<Bool> copy-in storage"
        grep -Fq 'call void @pgy_ab_push(ptr %pgy.param.1.local, i1 false)' "$artifact" ||
            fail "LLVM omitted Array<Bool> value-result push"
        grep -Fq 'call void @pgy_ab_set(ptr %pgy.param.1.local' "$artifact" ||
            fail "LLVM omitted Array<Bool> value-result set"
        for parameter in 3 7; do
            grep -Fq "%pgy.param.${parameter}.local = alloca %pgy.array.string" "$artifact" ||
                fail "LLVM omitted Array<String> copy-in $parameter"
            grep -Fq ", ptr %pgy.param.${parameter}.mutref, align 8" "$artifact" ||
                fail "LLVM omitted Array<String> copy-out $parameter"
        done
        runtime_obj="$WORK_DIR/runtime.o"
        "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" \
            -I"$ROOT_DIR/src/runtime" \
            -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" \
            >"$WORK_DIR/runtime.compile.out" \
            2>"$WORK_DIR/runtime.compile.err" ||
            fail "runtime ABI object did not compile"
        "$CLANG" -x ir "$artifact" -x none "$runtime_obj" -pthread -lm -o "$bin" \
            >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in single-array-bool-carriage single-array-bool-abi \
    single-prefix-carriage array-bool-carriage array-bool-abi \
    array-bool-type bool-carriage return-type \
    array-bool-push-receiver-missing array-bool-push-receiver-foreign \
    array-bool-push-value-type array-bool-set-receiver-missing \
    array-bool-set-receiver-foreign array-bool-set-value-type; do
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

echo "[$LABEL] Bool ArrayBool-bearing collection copyout parity + negatives: PASS"
