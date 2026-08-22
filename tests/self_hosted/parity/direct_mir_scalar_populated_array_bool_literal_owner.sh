#!/usr/bin/env bash
# Canonical Bool leaves populate Array<Bool> without a source-text fallback.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-populated-array-bool-literal"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_populated_array_bool_literal"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_populated_array_bool_literal.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_populated_array_bool_literal_mutations.py"
PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_bool_populated_literal_admission_owner.pgy"
READINESS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_bool_populated_literal_readiness_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_array_bool_populated_literal_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_bool_populated_literal_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
for owner in "$PLAN" "$ADMISSION" "$READINESS" "$C_OWNER" "$LLVM_OWNER" "$MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
grep -Fq 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v79' "$PLAN" ||
    fail "GraphPlan schema does not carry populated Array<Bool>"
grep -Fq 'AstExpressionNodeBoolLiteral()' "$ADMISSION" ||
    fail "admission does not consume Bool literal identity"
grep -Fq 'DirectMirScalarProgramNestedArrayLiteralSeedMarkerReady(' "$ADMISSION" ||
    fail "admission bypasses the canonical array seed"
grep -Fq 'SemanticExpressionBindingNone()' "$ADMISSION" ||
    fail "admission does not close binding identity"
grep -Fq 'ArrayLength(operands) < 1' "$READINESS" ||
    fail "readiness lost the one-or-more element contract"
grep -Fq 'CompilerAbiLayoutArrayBoolTypeName()' "$READINESS" ||
    fail "readiness lost the Array<Bool> ABI type"
grep -Fq 'out.data = (bool *)malloc(' "$C_OWNER" ||
    fail "C owner does not allocate typed Bool storage"
grep -Fq 'store i1 ' "$LLVM_OWNER" ||
    fail "LLVM owner does not store typed Bool elements"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"expr0":"[true, false, true]"' "$MIR" ||
    fail "producer omitted the populated Array<Bool> canary"
[[ "$(grep -o '"kind":"bool_literal"' "$MIR" | wc -l)" -ge 3 ]] ||
    fail "producer omitted ordered Bool leaves"
printf 'first\nsecond\nthird\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    extension="$backend"; [[ "$backend" == llvm ]] && extension="ll"
    artifact_rel="$WORK_REL/program.$extension"
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
        grep -Fq 'static pgy_ab pgy_array_bool_populated_literal_' "$artifact" ||
            fail "C artifact omitted the populated Array<Bool> owner"
        grep -Fq 'out.data[0] = true;' "$artifact" ||
            fail "C artifact omitted the first Bool store"
        grep -Fq 'out.data[1] = false;' "$artifact" ||
            fail "C artifact omitted the second Bool store"
        command=("$CC" -x c -std=c11 -fwrapv "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
    else
        grep -Fq 'call ptr @malloc(i64 3)' "$artifact" ||
            fail "LLVM artifact omitted exact Bool storage"
        grep -Fq 'store i1 true' "$artifact" ||
            fail "LLVM artifact omitted true element stores"
        grep -Fq 'store i1 false' "$artifact" ||
            fail "LLVM artifact omitted false element stores"
        runtime_obj="$WORK_DIR/runtime.o"
        "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" \
            -I"$ROOT_DIR/src/runtime" \
            -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" \
            >"$WORK_DIR/runtime.compile.out" \
            2>"$WORK_DIR/runtime.compile.err" || fail "runtime ABI compile failed"
        command=("$CLANG" -x ir "$artifact" -x none "$runtime_obj" \
            -pthread -lm -o "$bin")
    fi
    "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
        2>"$WORK_DIR/$backend.compile.err" || {
            cat "$WORK_DIR/$backend.compile.err" >&2
            fail "$backend artifact did not compile"
        }
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output or Bool order drifted"
done

for mutation in wrong-literal-kind wrong-literal-spelling broken-spine \
    unexpected-use array-bool-abi; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$mutated_rel" -o "$output_rel") \
                >"$WORK_DIR/$mutation.$backend.out" \
                2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published an artifact for $mutation"
    done
done

echo "[$LABEL] populated Array<Bool> literal C/LLVM parity: PASS"
