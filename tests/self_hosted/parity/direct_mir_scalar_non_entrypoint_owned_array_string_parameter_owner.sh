#!/usr/bin/env bash
# A non-entrypoint single-block local moves into an owner-handle ArrayString
# parameter; both backends retire caller cleanup and reject later use.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-non-entrypoint-owned-array-string-parameter"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_non_entrypoint_owned_array_string_parameter"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_nested_caller_owned_array_string_parameter.pgy"
NEGATIVE_SOURCE_REL="tests/self_hosted/fixtures/direct_mir_nested_caller_owned_array_string_parameter_use_after_move.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
FACT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_fact_owner.pgy"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_admission_owner.pgy"
USE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_use_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
! grep -Fq 'fact.caller_routines[row] != 0' "$FACT" ||
    fail "move fact restored the entrypoint-only caller restriction"
grep -Fq 'storage.routines.block_counts[routine] != 1' "$USE_OWNER" ||
    fail "last-use proof does not consume the resolved caller routine"
grep -Fq 'DirectMirScalarProgramRoutinePartitionStart(' "$USE_OWNER" ||
    fail "last-use proof does not consume routine-owned flat partitions"
grep -Fq 'storage.routines.local_counts[routine]' "$ADMISSION" ||
    fail "move admission does not validate the caller-owned local partition"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"name":"BuildAndRelease"' "$MIR" ||
    fail "producer omitted the non-entrypoint caller"
printf 'nested-released\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" ||
        fail "$backend rejected the non-entrypoint caller move"
    [[ -s "$artifact" ]] || fail "$backend emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq 'pgy_scalar_routine_1(pgy_local_0)' "$artifact" ||
            fail "C caller omitted the owner transfer"
        ! grep -Fq 'pgy_as_drop_owned(&pgy_local_0)' "$artifact" ||
            fail "C caller retained cleanup after the move"
        command=("$CC" -x c -std=c11 "$artifact")
        pgy_selfhost_emitted_c_uses_runtime_headers "$artifact" &&
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C compile failed"
    else
        grep -Fq '@pgy.scalar.routine.1(%pgy.array.string ' "$artifact" ||
            fail "LLVM caller omitted the owner transfer"
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
    "$NEGATIVE_SOURCE_REL" -o "$negative_mir_rel") ||
    fail "use-after-move MIR production failed"
for backend in c llvm; do
    output_rel="$WORK_REL/use-after-move.$backend"
    rm -f "$ROOT_DIR/$output_rel"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$negative_mir_rel" -o "$output_rel") \
        >"$WORK_DIR/use-after-move.$backend.out" \
        2>"$WORK_DIR/use-after-move.$backend.err"; then
        fail "$backend accepted non-entrypoint use-after-move"
    fi
    [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
        fail "$backend published a use-after-move artifact"
done

echo "[$LABEL] non-entrypoint owner move retirement C/LLVM + negative: PASS"
