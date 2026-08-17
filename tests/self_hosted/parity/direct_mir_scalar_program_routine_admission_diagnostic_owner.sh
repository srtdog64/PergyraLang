#!/usr/bin/env bash
# Routine admission preserves the exact rejecting stage before semantic widening.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-program-routine-admission-diagnostic"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_program_routine_admission_diagnostic"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_scalar_int_multiply.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_program_routine_admission_diagnostic_mutations.py"
DIAGNOSTIC_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_expression_diagnostic_owner.pgy"
FAILURE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_admission_failure_owner.pgy"
ROUTINE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_routine_admission_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
grep -Fq 'func DirectMirScalarCfgProgramRoutineAdmissionFailed(' "$DIAGNOSTIC_OWNER" ||
    fail "routine-stage diagnostic owner is missing"
grep -Fq 'struct DirectMirScalarProgramExpressionAdmissionFailure' "$FAILURE_OWNER" ||
    fail "expression failure receipt is missing"
grep -Fq 'stage=' "$DIAGNOSTIC_OWNER" && grep -Fq 'node=' "$DIAGNOSTIC_OWNER" ||
    fail "expression diagnostic does not consume stage/node"
for stage in statement instruction_kind; do
    grep -Fq "\"$stage\", routine_ordinal, block, global_row, source" "$ROUTINE_OWNER" ||
        fail "$stage rejection is not attributed"
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"

for mode in statement-stage instruction-kind-stage; do
    expected="${mode%-stage}"
    [[ "$mode" == "instruction-kind-stage" ]] && expected="instruction_kind"
    mutated_rel="$WORK_REL/$mode.mir.json"
    python "$MUTATIONS" "$MIR" "$mode" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        artifact_rel="$WORK_REL/$mode.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$artifact_rel") >"$WORK_DIR/$mode.$backend.out" \
            2>"$WORK_DIR/$mode.$backend.err"; then
            fail "$backend accepted $mode"
        fi
        [[ ! -e "$ROOT_DIR/$artifact_rel" ]] ||
            fail "$backend published an artifact for $mode"
        grep -Fq "routine admission stage is invalid: stage=$expected" \
            "$WORK_DIR/$mode.$backend.out" "$WORK_DIR/$mode.$backend.err" ||
            fail "$backend lost the $expected diagnostic"
    done
done

for row in "leaf-operand-stage|leaf-operand|0" \
        "expression-kind-stage|expression-kind|2"; do
    IFS='|' read -r mode stage node <<<"$row"
    mutated_rel="$WORK_REL/$mode.mir.json"
    python "$MUTATIONS" "$MIR" "$mode" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        artifact_rel="$WORK_REL/$mode.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$artifact_rel") >"$WORK_DIR/$mode.$backend.out" \
            2>"$WORK_DIR/$mode.$backend.err"; then
            fail "$backend accepted $mode"
        fi
        [[ ! -e "$ROOT_DIR/$artifact_rel" ]] ||
            fail "$backend published an artifact for $mode"
        grep -Fq "expression admission is invalid: stage=$stage node=$node" \
            "$WORK_DIR/$mode.$backend.out" "$WORK_DIR/$mode.$backend.err" ||
            fail "$backend lost $stage node $node"
    done
done

echo "[$LABEL] routine and nested expression stage/node receipts: PASS"
