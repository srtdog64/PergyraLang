#!/usr/bin/env bash
# Exact readonly record + scalar inputs -> owned ArrayBool C/LLVM boundary.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-readonly-record-array-bool-return"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_readonly_record_array_bool_return"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_readonly_logical_record_array_bool_return.pgy"
MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_readonly_logical_record_array_bool_return_policy_owner.pgy"
ABI_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_bool_abi_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_readonly_logical_record_array_bool_return_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'signature.param_count != 3' "$POLICY" || fail "parameter count is not exact"
grep -Fq 'carriage = "readonly-ref"' "$POLICY" || fail "record carriage is not exact"
grep -Fq 'owned_return_present: Bool' "$ABI_OWNER" || fail "ABI fact omits owned return"

mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") \
    >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"Reachable"' "$MIR" || fail "producer omitted callable"
grep -Fq '"type":"ReachabilityGraph","carriage":"readonly-ref"' "$MIR" ||
    fail "producer omitted readonly record"
grep -Fq '"return":"Array<Bool>"' "$MIR" || fail "producer omitted return type"
grep -Fq '"kind":"return"' "$MIR" || fail "producer omitted return instruction"
printf 'readonly-record-array-bool-return-ready\n' >"$WORK_DIR/expected.run"

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
        grep -Eq 'static pgy_ab pgy_scalar_routine_[0-9]+\(const pgy_scalar_logical_record_value_[0-9]+ \*pgy_param_0, long long pgy_param_1, bool pgy_param_2\)' "$artifact" ||
            fail "C omitted the exact return signature"
        grep -Eq 'pgy_ab pgy_local_[0-9]+ = \{0\};' "$artifact" ||
            fail "C omitted ArrayBool return storage"
        grep -Eq 'return pgy_local_[0-9]+;' "$artifact" ||
            fail "C omitted owned ArrayBool return"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin"); "${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" ||
            fail "C artifact did not compile"
    else
        grep -Eq 'define internal %pgy\.array\.bool @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, i64 %pgy\.param\.1, i1 %pgy\.param\.2\)' "$artifact" ||
            fail "LLVM omitted the exact return signature"
        grep -Fq ' = load %pgy.array.bool, ptr %pgy.local.' "$artifact" ||
            fail "LLVM omitted ArrayBool return storage load"
        grep -Eq 'ret %pgy\.array\.bool %pgy\.expr\.[0-9]+\.[0-9]+' "$artifact" ||
            fail "LLVM omitted owned ArrayBool return"
        ! grep -Fq '@pgy_ab_get' "$artifact" ||
            fail "LLVM emitted an unused ArrayBool get helper"
        ! grep -Fq 'pgy_runtime_panic_out_of_bounds_export' "$artifact" ||
            fail "LLVM emitted an unused bounds-panic dependency"
        "$CLANG" -x ir "$artifact" -o "$bin" >"$WORK_DIR/llvm.compile.out" \
            2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in record-type record-carriage record-pass record-abi int-type \
    int-carriage bool-type bool-carriage return-type return-abi-missing \
    return-abi-layout parameter-count; do
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

echo "[$LABEL] readonly record + ArrayBool owned return C/LLVM parity + negatives: PASS"
