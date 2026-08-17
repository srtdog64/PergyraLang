#!/usr/bin/env bash
# Exact Int/Long cast identities are admitted once and consumed by C/LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-int-to-long-cast"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_int_to_long_cast"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_int_to_long_cast.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_int_to_long_cast_mutations.py"
VARIANTS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_int_to_long_cast_artifact_variants.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

KIND="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_numeric_cast_expression_kind_owner.pgy"
READY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_numeric_cast_expression_readiness_owner.pgy"
C_EXPR="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_numeric_cast_expression_owner.pgy"
LLVM_EXPR="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_numeric_cast_expression_owner.pgy"
PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"

grep -Fq 'DirectMirScalarProgramExprTypeNameLong() -> Int { return 83; }' "$KIND" ||
    fail "Long type-name expression identity drifted"
grep -Fq 'DirectMirScalarProgramExprCastIntToLong() -> Int { return 84; }' "$KIND" ||
    fail "Int-to-Long cast expression identity drifted"
grep -Fq 'DirectMirScalarProgramExprTypeNameInt() -> Int { return 86; }' "$KIND" ||
    fail "Int type-name expression identity drifted"
grep -Fq 'DirectMirScalarProgramExprCastLongToInt() -> Int { return 87; }' "$KIND" ||
    fail "Long-to-Int cast expression identity drifted"
grep -Fq 'source_kind == AstExpressionNodeTypeName()' "$KIND" ||
    fail "Long type-name source identity is missing"
grep -Fq 'source_kind == AstExpressionNodeCast()' "$KIND" ||
    fail "Int-to-Long source/type join is missing"
grep -Fq 'facts.node_kinds[right] == DirectMirScalarProgramExprTypeNameLong()' "$READY" ||
    fail "cast readiness does not consume the exact Long type-name row"
grep -Fq 'facts.node_kinds[right] == DirectMirScalarProgramExprTypeNameInt()' "$READY" ||
    fail "cast readiness does not consume the exact Int type-name row"
grep -Fq 'DirectMirScalarProgramExprCastIntToLong()' "$C_EXPR" ||
    fail "C Int-to-Long cast consumer is missing"
grep -Fq 'DirectMirScalarProgramExprCastIntToLong()' "$LLVM_EXPR" ||
    fail "LLVM Int-to-Long cast consumer is missing"
grep -Fq 'DirectMirScalarProgramExprCastLongToInt()' "$C_EXPR" ||
    fail "C Long-to-Int cast consumer is missing"
grep -Fq 'DirectMirScalarProgramExprCastLongToInt()' "$LLVM_EXPR" ||
    fail "LLVM Long-to-Int cast consumer is missing"
grep -Fq 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v78' "$PLAN" ||
    fail "GraphPlan schema did not advance with exact Int/Long casts"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"kind":"type_name","text":"Long"' "$MIR" ||
    fail "producer omitted the Long type-name row"
grep -Fq '"kind":"cast","text":"value as Long"' "$MIR" ||
    fail "producer omitted the Int-to-Long cast graph"
grep -Fq '"kind":"type_name","text":"Int"' "$MIR" ||
    fail "producer omitted the Int type-name row"
grep -Fq '"kind":"cast","text":"value as Int"' "$MIR" ||
    fail "producer omitted the Long-to-Int cast graph"

printf '%s\n' '-17' '536870919' >"$WORK_DIR/expected-ordinary.run"
printf '%s\n' '-2147483648' '2147483648' >"$WORK_DIR/expected-boundary.run"
for backend in c llvm; do
    extension=c; [[ "$backend" == llvm ]] && extension=ll
    artifact_rel="$WORK_REL/program.$extension"
    artifact="$ROOT_DIR/$artifact_rel"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        [[ "$(grep -Fc 'return ((long long)(pgy_param_0));' "$artifact")" -eq 2 ]] ||
            fail "C omitted one of the exact Int/Long casts"
    else
        grep -Fq 'ret i64 %pgy.param.0' "$artifact" ||
            fail "LLVM did not preserve the Int value over the shared i64 ABI"
        ! grep -Eq '\b(sext|zext|trunc)\b' "$artifact" ||
            fail "LLVM changed the shared-i64 cast into a width conversion"
    fi
    for mode in ordinary boundary; do
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
            fail "$backend $mode Int/Long cast drifted"
    done
done

for mutation in \
        wrong-source-type wrong-target-type malformed-type-name-shape non-cast-use \
        long-to-int-wrong-source-type long-to-int-wrong-target-type \
        long-to-int-malformed-type-name-shape long-to-int-non-cast-use; do
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

echo "[$LABEL] exact Int/Long cast C/LLVM parity + source/target/shape negatives: PASS"
