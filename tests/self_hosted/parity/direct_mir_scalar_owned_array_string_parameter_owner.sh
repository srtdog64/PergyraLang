#!/usr/bin/env bash
# One last-use caller local moves into an owner-handle ArrayString parameter and C/LLVM reject use-after-move.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-owned-array-string-parameter"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_owned_array_string_parameter"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_owned_array_string_parameter.pgy"
NEGATIVE_SOURCE="tests/self_hosted/fixtures/direct_mir_owned_array_string_parameter_use_after_move.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"
FACT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_fact_owner.pgy"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_admission_owner.pgy"
USE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_use_owner.pgy"
COVERAGE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_coverage_admission_owner.pgy"
CLEANUP="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_array_string_cleanup_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_cleanup_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$FACT" "$ADMISSION" "$USE_OWNER" "$COVERAGE" "$CLEANUP" "$C_OWNER" \
        "$LLVM_OWNER" "$MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'DirectMirScalarProgramOwnedArrayStringMoveFact' "$FACT" ||
    fail "move fact is missing"
grep -Fq 'caller_routines: Array<Int>' "$FACT" ||
    fail "move fact does not own an ordered row set"
grep -Fq 'DirectMirScalarProgramOwnedArrayStringMoveIsLastUse(' "$COVERAGE" ||
    fail "move admission omits last-use proof"
! grep -Fq 'candidates != 1' "$ADMISSION" ||
    fail "move admission restored its one-row ceiling"
grep -Fq 'DirectMirScalarProgramOwnedArrayStringMoveRetiresLocal(' "$CLEANUP" ||
    fail "cleanup policy does not consume the move fact"
for owner in "$C_OWNER" "$LLVM_OWNER"; do
    grep -Fq 'DirectMirScalarProgramArrayStringLocalCleanupRequired(' "$owner" ||
        fail "backend cleanup bypasses move retirement: ${owner#"$ROOT_DIR/"}"
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"type":"Array<String>","carriage":"owner-handle"' "$MIR" ||
    fail "producer omitted owner-handle Array<String>"
printf 'released\n' >"$WORK_DIR/expected.run"

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
    [[ -s "$artifact" ]] || fail "$backend emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq 'pgy_scalar_routine_1(pgy_local_0)' "$artifact" ||
            fail "C caller omitted the owner transfer"
        grep -Fq 'pgy_as_drop_owned(&pgy_param_0)' "$artifact" ||
            fail "C callee omitted terminal ownership"
        ! grep -Fq 'pgy_as_drop_owned(&pgy_local_0)' "$artifact" ||
            fail "C caller retained cleanup after the move"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C compile failed"
    else
        grep -Fq '@pgy.scalar.routine.1(%pgy.array.string ' "$artifact" ||
            fail "LLVM caller omitted the owner transfer"
        grep -Fq 'pgy_as_drop_owned(ptr %pgy.param.0.local)' "$artifact" ||
            fail "LLVM callee omitted terminal ownership"
        ! grep -Fq 'pgy_as_drop_owned(ptr %pgy.local.0)' "$artifact" ||
            fail "LLVM caller retained cleanup after the move"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" 2>"$WORK_DIR/$backend.compile.err" ||
            fail "LLVM compile failed"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

negative_mir_rel="$WORK_REL/use-after-move.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$NEGATIVE_SOURCE" -o "$negative_mir_rel") ||
    fail "use-after-move MIR production failed"
for mutation in use-after-move owned-array-string-parameter-carriage \
        owned-array-string-parameter-pass \
        owned-array-string-parameter-abi-layout \
        owned-array-string-parameter-call-target; do
    mutated_rel="$negative_mir_rel"
    if [[ "$mutation" != use-after-move ]]; then
        mutated_rel="$WORK_REL/$mutation.mir.json"
        python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    fi
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

echo "[$LABEL] owner-handle move retirement C/LLVM parity + negatives: PASS"
