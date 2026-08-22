#!/usr/bin/env bash
# Record-Array copyout plus its direct element value C/LLVM parity.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-logical-record-array-element-input"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_logical_record_array_element_input"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_logical_record_array_element_input.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_value_result_policy_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_logical_record_array_element_input_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'let element_record_input: Bool = signature.param_count == 2' "$POLICY" ||
    fail "policy omitted the bounded element-input signature"
grep -Fq 'record_array_element_type == record_value_type' "$POLICY" ||
    fail "policy did not join the element declaration identity"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"GraphRow"' "$MIR" ||
    fail "producer omitted the element declaration"
grep -Fq '"name":"OtherRow"' "$MIR" ||
    fail "producer omitted the distinct negative declaration"
grep -Fq '"type":"Array<GraphRow>","carriage":"value-result"' "$MIR" ||
    fail "producer omitted the record-Array copyout"
grep -Fq '"type":"GraphRow","carriage":"value"' "$MIR" ||
    fail "producer omitted the direct element input"
printf 'record-array-element-input-ready\n' >"$WORK_DIR/expected.run"

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
        grep -Eq 'static void pgy_scalar_routine_[0-9]+\(pgy_GraphRow_array \*pgy_param_0_mutref, pgy_scalar_logical_record_value_[0-9]+ pgy_param_1\) \{' "$artifact" ||
            fail "C artifact omitted the element-input signature"
        grep -Fq 'pgy_GraphRow_array pgy_param_0 = *pgy_param_0_mutref;' "$artifact" ||
            fail "C artifact omitted record-Array copy-in"
        grep -Fq '*pgy_param_0_mutref = pgy_param_0;' "$artifact" ||
            fail "C artifact omitted record-Array copy-out"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/c.compile.out" \
            2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Eq 'define internal void @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0\.mutref, %pgy\.scalar\.logical\.record\.value\.[0-9]+ %pgy\.param\.1\)' "$artifact" ||
            fail "LLVM artifact omitted the element-input signature"
        grep -Fq '%pgy.param.0.array.copyout.' "$artifact" ||
            fail "LLVM artifact omitted record-Array copy-out"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in record-array-carriage record-array-physical-abi \
    record-input-carriage record-input-unknown-declaration \
    record-array-missing-element unknown-return-type; do
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

echo "[$LABEL] record-Array copyout + direct element input C/LLVM parity + negatives: PASS"
