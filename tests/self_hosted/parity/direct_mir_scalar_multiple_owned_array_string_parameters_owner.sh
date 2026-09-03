#!/usr/bin/env bash
# Multiple independent last-use ArrayString moves share one admitted row set.
# Multiple independent single-block owner moves consume one sealed row set and duplicate-local use fails closed.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-multiple-owned-array-string-parameters"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_multiple_owned_array_string_parameters"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_multiple_owned_array_string_parameters.pgy"
NEGATIVE_SOURCE_REL="tests/self_hosted/fixtures/direct_mir_duplicate_owned_array_string_parameter_move.pgy"
MIR_REL="$WORK_REL/program.mir.json"
FACT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_fact_owner.pgy"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_admission_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'caller_routines: Array<Int>' "$FACT" || fail "move row set is missing"
grep -Fq 'ArrayPush(caller_routines, routine)' "$ADMISSION" ||
    fail "admission does not append each independent move"
! grep -Fq 'candidates != 1' "$ADMISSION" || fail "one-row ceiling returned"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
[[ "$(grep -Fc '"carriage":"owner-handle"' "$ROOT_DIR/$MIR_REL")" -eq 1 ]] ||
    fail "producer changed the shared owner-handle signature"
printf 'released-two\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.out" \
        2>"$WORK_DIR/$backend.err" || fail "$backend rejected two moves"
    [[ -s "$artifact" ]] || fail "$backend emitted no artifact"
    if [[ "$backend" == c ]]; then
        [[ "$(grep -Fc 'pgy_scalar_routine_1(pgy_local_' "$artifact")" -eq 2 ]] ||
            fail "C caller omitted an owner transfer"
        ! grep -Eq 'pgy_as_drop_owned\(&pgy_local_(0|1)\)' "$artifact" ||
            fail "C caller retained cleanup for a moved local"
        command=("$CC" -x c -std=c11 "$artifact")
        pgy_selfhost_emitted_c_uses_runtime_headers "$artifact" &&
        command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C compile failed"
    else
        [[ "$(grep -Fc 'call void @pgy.scalar.routine.1(%pgy.array.string ' "$artifact")" -eq 2 ]] ||
            fail "LLVM caller omitted an owner transfer"
        ! grep -Eq 'pgy_as_drop_owned\(ptr %pgy.local.(0|1)\)' "$artifact" ||
            fail "LLVM caller retained cleanup for a moved local"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "LLVM compile failed"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

negative_mir_rel="$WORK_REL/duplicate-local.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$NEGATIVE_SOURCE_REL" -o "$negative_mir_rel") ||
    fail "duplicate-local MIR production failed before move admission"
for backend in c llvm; do
    output_rel="$WORK_REL/duplicate-local.$backend"
    rm -f "$ROOT_DIR/$output_rel"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$negative_mir_rel" -o "$output_rel") \
        >"$WORK_DIR/duplicate-local.$backend.out" \
        2>"$WORK_DIR/duplicate-local.$backend.err"; then
        fail "$backend accepted duplicate local retirement"
    fi
    [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
        fail "$backend published a duplicate-local artifact"
done

echo "[$LABEL] two independent owner moves and cleanup retirement: PASS"
