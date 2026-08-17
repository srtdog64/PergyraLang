#!/usr/bin/env bash
# Long addition consumes the language-owned wrap contract in C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-long-addition"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_long_addition"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_long_addition.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_long_addition_mutations.py"
VARIANTS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_long_addition_artifact_variants.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

KIND="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_id_owner.pgy"
SOURCE_KIND="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_owner.pgy"
READY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_readiness_owner.pgy"
C_EXPR="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_expression_owner.pgy"
LLVM_EXPR="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_expression_owner.pgy"
PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"

grep -Fq 'DirectMirScalarProgramExprAddLong() -> Int { return 78; }' "$KIND" ||
    fail "Long addition expression identity drifted"
grep -Fq 'source_kind == AstExpressionNodeAdd() && longs' "$SOURCE_KIND" ||
    fail "Long addition source/type join is missing"
grep -Fq 'kind == DirectMirScalarProgramExprAddLong()' "$READY" ||
    fail "Long addition readiness is missing"
grep -Fq 'kind == DirectMirScalarProgramExprAddLong()' "$C_EXPR" ||
    fail "C Long addition consumer is missing"
grep -Fq 'kind == DirectMirScalarProgramExprAddLong()' "$LLVM_EXPR" ||
    fail "LLVM Long addition consumer is missing"
grep -Fq 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v78' "$PLAN" ||
    fail "GraphPlan schema did not advance with Long addition"
! grep -Fq 'pgy_checked_add_i64_export' "$C_EXPR" ||
    fail "C changed default Long addition into checked arithmetic"
! grep -Fq 'pgy_checked_add_i64_export' "$LLVM_EXPR" ||
    fail "LLVM changed default Long addition into checked arithmetic"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"kind":"add","text":"left + right"' "$MIR" ||
    fail "producer omitted the Long addition graph"

printf '42\n' >"$WORK_DIR/expected-ordinary.run"
printf '%s\n' '-9223372036854775808' >"$WORK_DIR/expected-overflow.run"
for backend in c llvm; do
    extension=c; [[ "$backend" == llvm ]] && extension=ll
    artifact_rel="$WORK_REL/program.$extension"
    artifact="$ROOT_DIR/$artifact_rel"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    ! grep -Fq 'pgy_checked_add_i64_export' "$artifact" ||
        fail "$backend emitted the opt-in checked-add surface"
    if [[ "$backend" == c ]]; then
        grep -Fq 'return (pgy_param_0 + pgy_param_1);' "$artifact" ||
            fail "C omitted raw Long wrap addition"
    else
        grep -Fq ' = add i64 ' "$artifact" || fail "LLVM omitted Long addition"
        ! grep -Fq 'add nsw i64' "$artifact" ||
            fail "LLVM made Long wrap addition undefined on overflow"
    fi
    for mode in ordinary overflow; do
        variant="$WORK_DIR/$backend-$mode.$extension"
        python "$VARIANTS" "$artifact" "$backend" "$mode" "$variant"
        bin="$WORK_DIR/$backend-$mode.exe"
        if [[ "$backend" == c ]]; then
            command=("$CC" -x c -std=c11 -fwrapv "$variant")
            if pgy_selfhost_emitted_c_uses_runtime_headers "$variant"; then
                command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
            fi
            command+=(-lm -o "$bin")
            "${command[@]}" >"$WORK_DIR/$backend-$mode.compile.out" \
                2>"$WORK_DIR/$backend-$mode.compile.err" ||
                fail "$backend $mode compile failed"
        else
            "$CLANG" -x ir "$variant" -pthread -lm -o "$bin" \
                >"$WORK_DIR/$backend-$mode.compile.out" \
                2>"$WORK_DIR/$backend-$mode.compile.err" ||
                fail "$backend $mode compile failed"
        fi
        "$bin" | tr -d '\r' >"$WORK_DIR/$backend-$mode.run"
        cmp -s "$WORK_DIR/expected-$mode.run" "$WORK_DIR/$backend-$mode.run" ||
            fail "$backend $mode Long addition drifted"
    done
done

for mutation in wrong-right-type wrong-expression-kind; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
                2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] defined Long wrap addition C/LLVM parity + type/kind negatives: PASS"
