#!/usr/bin/env bash
# Exact by-value Array<Int>/Array<Bool> parameter ABI reaches both GraphPlan targets.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-array-int-value-parameter"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_array_int_value_parameter"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_array_int_value_parameter.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_policy_owner.pgy"
ROLE_PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_role_plan_owner.pgy"
SIGNATURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_owner.pgy"
FACT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_int_value_result_fact_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$POLICY" "$ROLE_PLAN" "$SIGNATURE" "$FACT" "$MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'carriages[ordinal] == "value"' "$POLICY" ||
    fail "callable policy omits by-value Array<Int> carriage"
grep -Fq 'DirectMirScalarProgramAbiValueParameterReady(' "$ROLE_PLAN" ||
    fail "parameter-role plan omits by-value ABI parameters"
grep -Fq 'abi_value_count' "$ROLE_PLAN" ||
    fail "parameter-role plan omits ABI-value cardinality"
grep -Fq 'DirectMirScalarProgramCallableParameterRolePlanReady(' "$SIGNATURE" ||
    fail "final signature omits the shared parameter-role plan"
grep -Fq 'carriage != "value" && carriage != "value-result"' "$FACT" ||
    fail "Array<Int> ABI fact does not distinguish value and value-result"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"type":"Array<Int>","carriage":"value"' "$MIR" ||
    fail "producer emitted no by-value Array<Int> parameter"
grep -Fq '"type":"Array<Bool>","carriage":"value"' "$MIR" ||
    fail "producer emitted no by-value Array<Bool> parameter"
grep -Fq '"size":32,"align":8' "$MIR" ||
    fail "producer omitted the Array<Int> physical ABI receipt"
printf '7\n' >"$WORK_DIR/expected.run"

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
        grep -Fq 'pgy_scalar_routine_1(pgy_ai pgy_param_0, pgy_ab pgy_param_1)' "$artifact" ||
            fail "C signature omitted the by-value collection carriers"
        grep -Fq 'pgy_local_2 = pgy_param_0;' "$artifact" ||
            fail "C callee did not consume the by-value parameter"
        grep -Fq 'pgy_scalar_routine_1(pgy_local_0, pgy_local_1)' "$artifact" ||
            fail "C caller did not pass the collection carriers by value"
        ! grep -Eq 'pgy_param_[01]_mutref' "$artifact" ||
            fail "C by-value collection fell through the value-result path"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C artifact did not compile"
    else
        grep -Fq '@pgy.scalar.routine.1(%pgy.array.int %pgy.param.0, %pgy.array.bool %pgy.param.1)' "$artifact" ||
            fail "LLVM signature omitted the by-value collection carriers"
        grep -Fq 'store %pgy.array.int %pgy.param.0, ptr %pgy.local.2' "$artifact" ||
            fail "LLVM callee did not consume the by-value parameter"
        grep -Fq '@pgy.scalar.routine.1(%pgy.array.int ' "$artifact" ||
            fail "LLVM caller did not pass the collection carriers by value"
        ! grep -Eq '%pgy.param.[01].mutref' "$artifact" ||
            fail "LLVM by-value collection fell through the value-result path"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" 2>"$WORK_DIR/$backend.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in array-int-value-abi-layout array-int-value-carriage \
        array-int-value-pass-shape array-bool-value-abi-layout \
        array-bool-value-carriage array-bool-value-pass-shape; do
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

echo "[$LABEL] by-value Array<Int>/Array<Bool> ABI/call C/LLVM parity + negatives: PASS"
