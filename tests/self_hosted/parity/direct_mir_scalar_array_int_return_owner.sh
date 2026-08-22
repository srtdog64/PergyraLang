#!/usr/bin/env bash
# Array<Int> returns reuse the admitted storage ABI without mutref copy-out.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-array-int-return"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_array_int_return"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_array_int_return.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_policy_owner.pgy"
SIGNATURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_owner.pgy"
FACT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_int_value_result_fact_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$POLICY" "$SIGNATURE" "$FACT" "$MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'type_name == CompilerAbiLayoutArrayIntTypeName() ||' "$POLICY" ||
    fail "callable return policy omits Array<Int>"
grep -Fq 'array_int_return || nominal_passthrough' "$SIGNATURE" ||
    fail "callable signature shape omits Array<Int> returns"
grep -Fq 'instruction_kinds[global] != "return"' "$FACT" ||
    fail "Array<Int> ABI fact rejects return receipts"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"return":"Array<Int>"' "$MIR" ||
    fail "producer emitted no Array<Int> callable return"
grep -Fq '"kind":"return"' "$MIR" || fail "producer emitted no return row"
grep -Fq '"abi_type_name":"Array<Int>"' "$MIR" ||
    fail "producer omitted the Array<Int> return ABI"
printf '0\n' >"$WORK_DIR/expected.run"

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
        grep -Eq 'static pgy_ai pgy_scalar_routine_[0-9]+\(int32_t pgy_param_0\)' "$artifact" ||
            fail "C signature omitted the Array<Int> return carrier"
        grep -Eq 'static int32_t pgy_scalar_routine_[0-9]+\(pgy_ai pgy_param_0\)' "$artifact" ||
            fail "C consumer omitted the by-value returned carrier"
        ! grep -Eq 'static pgy_ai pgy_scalar_routine_[0-9]+\([^)]*mutref' "$artifact" ||
            fail "C Array<Int> return became a mutref"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C artifact did not compile"
    else
        grep -Eq 'define internal %pgy\.array\.int @pgy\.scalar\.routine\.[0-9]+\(i64 %pgy\.param\.0\)' "$artifact" ||
            fail "LLVM signature omitted the Array<Int> return carrier"
        grep -Eq 'define internal i64 @pgy\.scalar\.routine\.[0-9]+\(%pgy\.array\.int %pgy\.param\.0\)' "$artifact" ||
            fail "LLVM consumer omitted the by-value returned carrier"
        ! grep -Eq 'define internal %pgy\.array\.int [^(]+\([^)]*mutref' "$artifact" ||
            fail "LLVM Array<Int> return became a mutref"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" 2>"$WORK_DIR/$backend.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in array-int-return-abi-layout array-int-return-kind \
        array-int-value-abi-layout; do
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

echo "[$LABEL] Array<Int> return ABI/call C/LLVM parity + negatives: PASS"
