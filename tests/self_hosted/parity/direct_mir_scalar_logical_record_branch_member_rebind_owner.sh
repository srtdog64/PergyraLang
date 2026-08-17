#!/usr/bin/env bash
# CFG-dominating record member rebinds across mutually exclusive branches.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-logical-record-branch-member-rebind"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_logical_record_branch_member_rebind"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_logical_record_bool_first_rebind.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_logical_record_branch_member_rebind_mutations.py"
OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_member_rebind_owner.pgy"
PATH_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_member_path_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
for command in "$CC" "$CLANG" python; do command -v "$command" >/dev/null ||
    fail "missing command: $command"; done
for file in "$MUTATIONS" "$OWNER"; do [[ -f "$file" ]] ||
    fail "missing owner: ${file#"$ROOT_DIR/"}"; done
grep -Fq 'func DirectMirScalarProgramLogicalRecordMemberRebindUsePrefix(' "$OWNER" ||
    fail "member owner omitted the CFG-aware use-prefix fact"
grep -Fq 'MirRoutineLatestDominatingLocalValueRow(' "$OWNER" ||
    fail "member owner omitted canonical dominance"
grep -Fq 'capture.arg1 == "default_param"' "$OWNER" ||
    fail "member owner omitted by-value parameter copy identity"
grep -Fq 'required_carriage = "value"' "$PATH_OWNER" ||
    fail "member owner does not join default_param to value carriage"
! grep -Fq 'previous_results' "$OWNER" ||
    fail "member owner restored linear instruction-order predecessor state"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"expr1":"state.ok"' "$MIR" ||
    fail "producer omitted the branch-local Bool member write"
grep -Fq '"expr1":"state.last_row"' "$MIR" ||
    fail "producer omitted the alternate Int member write"
grep -Fq '"name":"DisableStateCopy"' "$MIR" ||
    fail "producer omitted the by-value record copy canary"
grep -Fq '"arg0":"state","arg1":"default_param"' "$MIR" ||
    fail "producer omitted the by-value parameter member write"
printf 'record-bool-rebind-ready\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    binary="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend emitted no artifact"
    if [[ "$backend" == c ]]; then
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$binary")
        "${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" ||
            fail "C artifact did not compile"
    else
        "$CLANG" -x ir "$artifact" -o "$binary" \
            >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$binary" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in non-dominating-prefix missing-target-local-ref \
    foreign-target-local-ref wrong-rhs-type wrong-default-carriage \
    wrong-default-binding; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$output" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] branch-exclusive member rebind C/LLVM parity + negatives: PASS"
