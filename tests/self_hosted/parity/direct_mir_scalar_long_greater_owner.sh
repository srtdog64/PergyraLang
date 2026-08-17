#!/usr/bin/env bash
# Exact Long comparison identities are consumed by the shared comparison emitters.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-long-greater"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_long_greater"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_long_greater.pgy"
MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_comparison_expression_kind_owner.pgy"
KIND="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_owner.pgy"
READY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_comparison_expression_readiness_owner.pgy"
BOOL_READY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_bool_readiness_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_expression_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_expression_owner.pgy"
PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_long_greater_mutations.py"
VARIANTS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_long_greater_artifact_variants.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
for command in "$CC" "$CLANG" python; do command -v "$command" >/dev/null ||
    fail "missing command: $command"; done
grep -Fq 'DirectMirScalarProgramExprGreaterLong() -> Int { return 76; }' "$OWNER" ||
    fail "Long greater expression identity drifted"
grep -Fq 'DirectMirScalarProgramExprEqualLong() -> Int { return 77; }' "$OWNER" ||
    fail "Long equality expression identity drifted"
grep -Fq 'DirectMirScalarProgramExprNotEqualLong() -> Int { return 80; }' "$OWNER" ||
    fail "Long inequality expression identity drifted"
grep -Fq 'DirectMirScalarProgramExprLessLong() -> Int { return 85; }' "$OWNER" ||
    fail "Long less expression identity drifted"
grep -Fq 'DirectMirScalarProgramComparisonExpressionKindFact(' "$KIND" ||
    fail "expression admission bypasses the comparison owner"
for term in CompilerAbiLayoutLongTypeName CompilerAbiLayoutBoolTypeName; do
    grep -Fq "$term()" "$READY" || fail "comparison readiness omits $term"
done
grep -Fq 'DirectMirScalarProgramExprGreaterLong()' "$BOOL_READY" ||
    fail "nested Bool readiness omits Long greater"
grep -Fq 'DirectMirScalarProgramExprEqualLong()' "$BOOL_READY" ||
    fail "nested Bool readiness omits Long equality"
grep -Fq 'DirectMirScalarProgramExprNotEqualLong()' "$BOOL_READY" ||
    fail "nested Bool readiness omits Long inequality"
grep -Fq 'DirectMirScalarProgramExprLessLong()' "$BOOL_READY" ||
    fail "nested Bool readiness omits Long less"
grep -Fq 'DirectMirScalarProgramExprGreaterLong()' "$C_OWNER" ||
    fail "C comparison emitter omits Long greater"
grep -Fq 'DirectMirScalarProgramExprEqualLong()' "$C_OWNER" ||
    fail "C comparison emitter omits Long equality"
grep -Fq 'DirectMirScalarProgramExprNotEqualLong()' "$C_OWNER" ||
    fail "C comparison emitter omits Long inequality"
grep -Fq 'DirectMirScalarProgramExprLessLong()' "$C_OWNER" ||
    fail "C comparison emitter omits Long less"
grep -Fq 'DirectMirScalarProgramExprGreaterLong()' "$LLVM_OWNER" ||
    fail "LLVM comparison emitter omits Long greater"
grep -Fq 'DirectMirScalarProgramExprEqualLong()' "$LLVM_OWNER" ||
    fail "LLVM comparison emitter omits Long equality"
grep -Fq 'DirectMirScalarProgramExprNotEqualLong()' "$LLVM_OWNER" ||
    fail "LLVM comparison emitter omits Long inequality"
grep -Fq 'DirectMirScalarProgramExprLessLong()' "$LLVM_OWNER" ||
    fail "LLVM comparison emitter omits Long less"
grep -Fq 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v78' "$PLAN" ||
    fail "GraphPlan schema did not advance with typed Long comparison"
[[ ! -e "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_int_comparison_expression_kind_owner.pgy" ]] ||
    fail "retired Int-only comparison owner remains"

mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$MIR_REL") >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" ||
    fail "MIR production failed"
grep -Fq '"kind":"greater","text":"left > right"' "$MIR" ||
    fail "producer omitted the Long greater graph"
grep -Fq '"kind":"equality","text":"left == right"' "$MIR" ||
    fail "producer omitted the Long equality graph"
grep -Fq '"kind":"inequality","text":"left != right"' "$MIR" ||
    fail "producer omitted the Long inequality graph"
grep -Fq '"kind":"less","text":"left < right"' "$MIR" ||
    fail "producer omitted the Long less graph"
printf '1\n0\n1\n1\n' >"$WORK_DIR/expected-true.run"
printf '0\n1\n0\n0\n' >"$WORK_DIR/expected-false.run"
for backend in c llvm; do
    extension=c; [[ "$backend" == llvm ]] && extension=ll
    artifact_rel="$WORK_REL/program.$extension"; artifact="$ROOT_DIR/$artifact_rel"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$MIR_REL" \
        -o "$artifact_rel") >"$WORK_DIR/$backend.out" \
        2>"$WORK_DIR/$backend.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq ' > ' "$artifact" || fail "C artifact omitted Long greater"
        grep -Fq ' == ' "$artifact" || fail "C artifact omitted Long equality"
        grep -Fq ' != ' "$artifact" || fail "C artifact omitted Long inequality"
        grep -Fq ' < ' "$artifact" || fail "C artifact omitted Long less"
    else
        grep -Fq 'icmp sgt i64' "$artifact" ||
            fail "LLVM artifact omitted signed Long greater"
        grep -Fq 'icmp eq i64' "$artifact" ||
            fail "LLVM artifact omitted Long equality"
        grep -Fq 'icmp ne i64' "$artifact" ||
            fail "LLVM artifact omitted Long inequality"
        grep -Fq 'icmp slt i64' "$artifact" ||
            fail "LLVM artifact omitted signed Long less"
    fi
    for mode in true false; do
        variant="$WORK_DIR/$backend-$mode.$extension"
        python "$VARIANTS" "$artifact" "$backend" "$mode" "$variant"
        bin="$WORK_DIR/$backend-$mode.exe"
        if [[ "$backend" == c ]]; then
            command=("$CC" -x c -std=c11 -fwrapv "$variant")
            if pgy_selfhost_emitted_c_uses_runtime_headers "$variant"; then
                command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
            fi
            command+=(-lm -o "$bin"); "${command[@]}" ||
                fail "$backend-$mode compile failed"
        else
            "$CLANG" -x ir "$variant" -o "$bin" ||
                fail "$backend-$mode compile failed"
        fi
        "$bin" | tr -d '\r' >"$WORK_DIR/$backend-$mode.run"
        cmp -s "$WORK_DIR/expected-$mode.run" "$WORK_DIR/$backend-$mode.run" ||
            fail "$backend $mode Long comparison result drifted"
    done
done

for mutation in wrong-right-type wrong-expression-kind \
        wrong-equality-right-type wrong-equality-expression-kind \
        wrong-inequality-right-type wrong-inequality-expression-kind \
        wrong-less-left-type wrong-less-right-type \
        wrong-less-expression-kind; do
    invalid_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$invalid_rel"
    for backend in c llvm; do
        output="$ROOT_DIR/$WORK_REL/$mutation.$backend"; rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$invalid_rel" -o "$WORK_REL/$mutation.$backend") \
                >"$WORK_DIR/$mutation.$backend.out" \
                2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$output" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] exact Long greater/equality/inequality/less C/LLVM true/false + type/kind negatives: PASS"
