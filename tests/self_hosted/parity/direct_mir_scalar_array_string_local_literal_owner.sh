#!/usr/bin/env bash
# Exact local-SSA String element and one-use carriage in Array<String> literal.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-array-string-local-literal"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_array_string_local_literal_element"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_array_string_local_literal_element.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_admission_owner.pgy"
OPERAND="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_operand_admission_owner.pgy"
READINESS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_readiness_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_array_string_local_literal_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
for pair in "$ADMISSION:120" "$OPERAND:120" "$READINESS:110" \
        "$MUTATIONS:80"; do
    owner="${pair%:*}"; cap="${pair##*:}"
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
    [[ "$(wc -l <"$owner")" -le "$cap" ]] ||
        fail "owner hard cap exceeded: ${owner#"$ROOT_DIR/"}"
done
grep -Fq 'DirectMirScalarCfgLeafOperandFromOwners(' "$OPERAND" ||
    fail "operand owner omits the existing local identity join"
grep -Fq 'value_types.local_types[local_row]' "$OPERAND" ||
    fail "operand owner omits the local String type join"
grep -Fq 'local_row + local_offset, operand.use_count' "$OPERAND" ||
    fail "operand owner omits local row/use carriage"
grep -Fq 'use_offset = use_offset + single.consumed_uses' "$ADMISSION" ||
    fail "literal admission omits exact use consumption"
grep -Fq 'DirectMirScalarProgramExprLocal()' "$READINESS" ||
    fail "literal readiness omits the normalized local operand"
for owner in "$ADMISSION" "$OPERAND" "$READINESS"; do
    for term in ParserExpressionLongLiteral source_text.1 source_json native_retry; do
        ! grep -Fq "$term" "$owner" || fail "forbidden spelling branch: $term"
    done
done

mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
mir_sha="$(sha256sum "$MIR" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$mir_sha" == "90477CF8FF28AED4EFA2F026DCF58F37CC592A869847705F063560A362CDA56E" ]] ||
    fail "source MIR identity changed: $mir_sha"
grep -Fq '"expr0":"[source_text]"' "$MIR" ||
    fail "producer omitted the local literal"
grep -Fq '"uses":["source_text.1"]' "$MIR" ||
    fail "producer omitted the local SSA use"
printf 'red!\nblue!\narray-string-local-literal-ready\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Eq '\(const char \*\[\]\)\{pgy_local_[0-9]+\}' "$artifact" ||
            fail "C literal lost the admitted local String operand"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-Werror=free-nonheap-object -lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" ||
            fail "C artifact did not compile"
    else
        grep -Eq 'call void @pgy_as_push\(ptr %pgy\.expr\.[0-9]+\.[0-9]+\.backing, ptr %pgy\.expr\.[0-9]+\.[0-9]+' "$artifact" ||
            fail "LLVM literal lost the admitted local String operand"
        "$CLANG" -x ir "$artifact" -o "$bin" >"$WORK_DIR/llvm.compile.out" \
            2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in local-missing-use local-wrong-use local-extra-use \
    local-wrong-type local-forged-formal local-wrong-element-kind \
    local-reordered-spine local-wrong-root; do
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
            fail "$backend published an artifact for $mutation"
    done
done

echo "[$LABEL] local String Array literal C/LLVM parity + negatives: PASS"
