#!/usr/bin/env bash
# Two record values plus one record copyout C/LLVM parity and negatives.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-logical-record-inputs-value-result"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_logical_record_inputs_value_result"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_logical_record_inputs_value_result.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_inputs_value_result_policy_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_logical_record_inputs_value_result_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'signature.return_type != CompilerAbiLayoutBoolTypeName()' "$POLICY" ||
    fail "policy omitted the exact Bool return"
grep -Fq 'first == second ||' "$POLICY" ||
    fail "policy omitted distinct record identities"
grep -Fq 'ordinal == 2 &&' "$POLICY" ||
    fail "policy omitted the exact copyout ordinal"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"AppendStatementFacts"' "$MIR" ||
    fail "producer omitted the exact callable"
grep -Fq '"type":"StatementOrder","carriage":"value-result"' "$MIR" ||
    fail "producer omitted the record copyout"
printf 'record-inputs-copyout-ready\n' >"$WORK_DIR/expected.run"

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
        grep -Eq 'static bool pgy_scalar_routine_[0-9]+\(pgy_scalar_logical_record_value_[0-9]+ pgy_param_0, pgy_scalar_logical_record_value_[0-9]+ pgy_param_1, pgy_scalar_logical_record_value_[0-9]+ \*pgy_param_2_mutref\)' "$artifact" ||
            fail "C artifact omitted the exact record signature"
        grep -Eq 'pgy_scalar_logical_record_value_[0-9]+ pgy_param_2 = \*pgy_param_2_mutref;' "$artifact" ||
            fail "C omitted record copy-in"
        [[ "$(grep -Fc '*pgy_param_2_mutref = pgy_param_2;' "$artifact")" -ge 2 ]] ||
            fail "C omitted an early/final record copy-out"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/c.compile.out" \
            2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Eq 'define internal i1 @pgy\.scalar\.routine\.[0-9]+\(%pgy\.scalar\.logical\.record\.value\.[0-9]+ %pgy\.param\.0, %pgy\.scalar\.logical\.record\.value\.[0-9]+ %pgy\.param\.1, ptr %pgy\.param\.2\.mutref\)' "$artifact" ||
            fail "LLVM artifact omitted the exact record signature"
        grep -Eq '%pgy\.param\.2\.local = alloca %pgy\.scalar\.logical\.record\.value\.[0-9]+' "$artifact" ||
            fail "LLVM omitted record copy-in storage"
        [[ "$(grep -Fc '%pgy.param.2.record.copyout.' "$artifact")" -ge 4 ]] ||
            fail "LLVM omitted an early/final record copy-out"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in first-record-type first-record-carriage \
    copyout-type copyout-carriage copyout-pass copyout-abi return-type; do
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

echo "[$LABEL] two record values + record copyout C/LLVM parity + negatives: PASS"
