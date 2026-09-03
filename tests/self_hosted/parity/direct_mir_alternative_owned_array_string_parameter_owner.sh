#!/usr/bin/env bash
# Both if/else arms retire one caller ArrayString; incomplete coverage fails.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-alternative-owned-array-string-parameter"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_alternative_owned_array_string_parameter"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE="tests/self_hosted/fixtures/direct_mir_alternative_owned_array_string_parameter.pgy"
ONE_SIDED="tests/self_hosted/fixtures/direct_mir_one_sided_owned_array_string_parameter.pgy"
LATER_USE="tests/self_hosted/fixtures/direct_mir_alternative_owned_array_string_parameter_use_after_move.pgy"
DUPLICATE_ARM="tests/self_hosted/fixtures/direct_mir_duplicate_arm_owned_array_string_parameter.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"
COVERAGE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_coverage_admission_owner.pgy"
PLAN_READY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_plan_readiness_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'DirectMirScalarProgramOwnedArrayStringAlternativeMovePairReady(' \
    "$COVERAGE" || fail "alternative coverage owner is missing"
grep -Fq 'DirectMirScalarProgramOwnedArrayStringMoveReadyForPlan(' \
    "$PLAN_READY" || fail "sealed plan does not recheck move coverage"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE" -o "$MIR_REL") \
    || fail "positive MIR production failed"
[[ "$(wc -c <"$MIR" | tr -d ' ')" == 16171 ]] ||
    fail "positive MIR size drifted"
printf 'branch-released\nbranch-released\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend emitted no artifact"
    if [[ "$backend" == c ]]; then
        [[ "$(grep -Fc 'pgy_scalar_routine_1(pgy_local_0);' "$artifact")" == 2 ]] ||
            fail "C did not emit both alternative transfers"
        grep -Fq 'pgy_as_drop_owned(&pgy_param_0)' "$artifact" ||
            fail "C callee omitted terminal cleanup"
        ! grep -Fq 'pgy_as_drop_owned(&pgy_local_0)' "$artifact" ||
            fail "C caller retained cleanup after all-path move"
        command=("$CC" -x c -std=c11 "$artifact")
        pgy_selfhost_emitted_c_uses_runtime_headers "$artifact" &&
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        command+=(-lm -o "$bin")
        "${command[@]}" || fail "C compile failed"
    else
        [[ "$(grep -Fc '@pgy.scalar.routine.1(%pgy.array.string ' "$artifact")" == 3 ]] ||
            fail "LLVM did not declare and emit both alternative transfers"
        grep -Fq 'pgy_as_drop_owned(ptr %pgy.param.0.local)' "$artifact" ||
            fail "LLVM callee omitted terminal cleanup"
        ! grep -Fq 'pgy_as_drop_owned(ptr %pgy.local.0)' "$artifact" ||
            fail "LLVM caller retained cleanup after all-path move"
        "$CLANG" -x ir "$artifact" -o "$bin" || fail "LLVM compile failed"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for item in "one-sided:$ONE_SIDED" "later-use:$LATER_USE" \
        "duplicate-arm:$DUPLICATE_ARM"; do
    name="${item%%:*}"
    source="${item#*:}"
    negative_rel="$WORK_REL/$name.mir.json"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "$source" -o "$negative_rel") || fail "$name MIR production failed"
    for backend in c llvm; do
        output_rel="$WORK_REL/$name.$backend"
        rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$negative_rel" -o "$output_rel") >"$WORK_DIR/$name.$backend.out" \
            2>"$WORK_DIR/$name.$backend.err"; then
            fail "$backend accepted $name"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $name"
    done
done

for mutation in owned-array-string-alternative-missing-edge \
        owned-array-string-parameter-carriage owned-array-string-parameter-pass \
        owned-array-string-parameter-abi-layout; do
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
            fail "$backend published $mutation"
    done
done

echo "[$LABEL] all-path C/LLVM retirement + fail-closed negatives: PASS"
