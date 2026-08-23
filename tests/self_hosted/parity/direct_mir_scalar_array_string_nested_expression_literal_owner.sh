#!/usr/bin/env bash
# Ordered normalized String expressions carried by an Array<String> literal.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-array-string-nested-expression-literal"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_array_string_nested_expression_literal"; WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_array_string_nested_expression_literal.pgy"; MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_array_string_nested_expression_literal_mutations.py"
SEED="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_nested_array_literal_seed_owner.pgy"; NESTED="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_nested_literal_owner.pgy"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_admission_owner.pgy"; READINESS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_readiness_owner.pgy"
PARAMETER_CARRIAGE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_parameter_carriage_owner.pgy"; C_LITERAL="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_array_string_literal_expression_owner.pgy"; LLVM_LITERAL="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_literal_expression_owner.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
for pair in "$SEED:35" "$NESTED:70" "$ADMISSION:445" \
        "$READINESS:110" "$PARAMETER_CARRIAGE:40" \
        "$C_LITERAL:45" "$LLVM_LITERAL:90" "$MUTATIONS:80"; do
    owner="${pair%:*}"; cap="${pair##*:}"
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
    [[ "$(wc -l <"$owner")" -le "$cap" ]] || fail "owner hard cap exceeded: ${owner#"$ROOT_DIR/"}"
done
grep -Fq 'func DirectMirScalarProgramNestedArrayLiteralSeedMarkerReady(' "$SEED" || fail "common nested array seed owner is missing"
grep -Fq 'normalized_types[operand] != CompilerAbiLayoutStringTypeName()' "$NESTED" || fail "nested String literal omits normalized operand type identity"
! grep -Fq 'expected_type' "$NESTED" || fail "nested String literal reused outer type"
grep -Fq 'while sequence.arena.topology.node_kinds[cursor] ==' "$NESTED" || fail "nested String literal omits the ordered source spine"
grep -Fq 'node != sequence.roots[0] ||' "$NESTED" || fail "nested String literal bypasses the single-leaf owner"
grep -Fq 'DirectMirScalarProgramNestedArrayStringLiteralOperandRows(' "$ADMISSION" || fail "general expression admission omits nested String literal carriage"
grep -Fq 'nary_operands, ArrayLength(kinds),' "$ADMISSION" || fail "mixed String literal omits ordered n-ary carriage"
! rg -F 'DirectMirScalarProgramNestedSingleArrayStringLiteralOperandRow(' \
    "$ROOT_DIR/src/self_hosted/compiler" >/dev/null ||
    fail "retired single-element nested String owner returned"
grep -Fq 'facts.node_types[operand] ==' "$READINESS" || fail "literal readiness omits normalized String expression operands"
grep -Fq 'routines.parameter_carriages[parameter]' "$PARAMETER_CARRIAGE" ||
    fail "mixed String literal omits routine-owned parameter carriage"
grep -Fq '(owned == 1 && ArrayLength(operands) == 1)' "$PARAMETER_CARRIAGE" ||
    fail "mixed String literal weakened the owner-handle singleton boundary"
! rg -F 'DirectMirScalarProgramNestedArrayIntLiteralSeedMarkerReady' \
    "$ROOT_DIR/src/self_hosted/compiler" >/dev/null ||
    fail "retired type-specific array seed owner returned"
grep -Fq 'DirectMirScalarProgramCStringArrayValuesFn()' "$C_LITERAL" ||
    fail "C populated literal omits growable backing materialization"
! grep -Fq '.storage = alloca [' "$LLVM_LITERAL" ||
    fail "LLVM populated literal retained stack-backed array storage"
mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"expr0":"[Concat(indent, Concat(callable_label, Concat(routine,' "$MIR" ||
    fail "producer omitted nested String array literal"
grep -Fq '"uses":["callable_label.1","routine.1"]' "$MIR" ||
    fail "producer omitted nested literal local-use receipt"
grep -Fq 'FormatMixedStringLiteral(prefix, first, second, third)' "$MIR" ||
    fail "producer omitted mixed String array element"
grep -Fq '"expr0":"[\"Program:\\n\", prefix, FormatMixedStringLiteral(prefix, first, second, third)]"' "$MIR" ||
    fail "producer omitted mixed value-parameter element"
grep -Fq '"uses":["first.1","second.1","third.1"]' "$MIR" ||
    fail "producer omitted mixed literal ordered-use receipt"
printf '>func Example\n\ntail\n\nProgram:\n\n>\n>row\n\ndone\n\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    extension=c; [[ "$backend" == llvm ]] && extension=ll
    artifact_rel="$WORK_REL/program.$extension"
    artifact="$ROOT_DIR/$artifact_rel"
    binary="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Eq 'pgy_as_from_values\(\(const char \*\[\]\)\{pgy_concat\(' "$artifact" ||
            fail "C literal lost the normalized Concat operand"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-Werror=free-nonheap-object -lm -o "$binary")
        "${command[@]}" || fail "C artifact did not compile"
    else
        grep -Eq 'call void @pgy_as_push\(ptr %pgy\.expr\.[0-9]+\.[0-9]+\.backing, ptr %pgy\.expr\.[0-9]+\.[0-9]+' "$artifact" ||
            fail "LLVM literal lost the normalized Concat operand"
        "$CLANG" -x ir "$artifact" -o "$binary" ||
            fail "LLVM artifact did not compile"
    fi
    "$binary" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done
for mutation in nested-missing-use nested-wrong-use nested-wrong-element-kind \
        nested-wrong-seed-parent nested-wrong-root nested-wrong-parameter-ref \
        mixed-missing-use mixed-wrong-use mixed-wrong-spine \
        mixed-wrong-element-kind mixed-owned-parameter; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
    done
done
echo "[$LABEL] nested String Array literal C/LLVM parity + negatives: PASS"
