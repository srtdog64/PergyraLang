#!/usr/bin/env bash
# Option<Int> try-let control flow from one admitted semantic graph.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-option-int-try-let"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_option_int_try_let"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_option_int_try_let.pgy"
MIR_REL="$WORK_REL/producer.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_option_int_try_let_mutations.py"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_option_int_try_admission_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_option_int_try_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_option_int_try_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for path in "$MUTATIONS" "$ADMISSION" "$C_OWNER" "$LLVM_OWNER"; do
    [[ -f "$path" ]] || fail "missing owner: ${path#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'AstExpressionNodeTry()' "$ADMISSION" ||
    fail "try admission omitted the semantic root kind"
grep -Fq 'CompilerAbiLayoutOptionIntTypeName()' "$ADMISSION" ||
    fail "try admission omitted the Option<Int> ABI identity"
grep -Fq 'signature.parameters.carriages[ordinal] == "value-result"' "$ADMISSION" ||
    fail "try admission omitted the first-rung copy-out rejection"
! grep -Fq 'JsonValueEnd' "$ADMISSION" ||
    fail "try admission reintroduced raw JSON expression parsing"
! grep -Fq 'PickTryValue' "$ADMISSION" ||
    fail "try admission branched on the fixture routine spelling"
grep -Fq 'DirectMirScalarProgramExprIsSomeInt()' "$C_OWNER" ||
    fail "C try projection omitted the admitted Option<Int> test"
grep -Fq 'pgy.try.none.' "$LLVM_OWNER" ||
    fail "LLVM try projection omitted the None return edge"
grep -Fq 'pgy.try.some.' "$LLVM_OWNER" ||
    fail "LLVM try projection omitted the Some unwrap edge"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
mir_sha="$(sha256sum "$MIR" | awk '{print toupper($1)}')"
[[ "$mir_sha" == "CA0108FA272F2B4B293E0F6D89504B69828F273E5C5A0AC970D7444EE326D61B" ]] ||
    fail "source MIR identity changed: $mir_sha"
grep -Fq '"kind":"try"' "$MIR" || fail "producer omitted the try graph"
grep -Fq '"return":"Option<Int>"' "$MIR" ||
    fail "producer omitted the enclosing Option<Int> signature"

printf '8\n0\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
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
        grep -Fq 'if (!pgy_scalar_option_int_is_some(' "$artifact" ||
            fail "C artifact omitted the None propagation edge"
        grep -Fq '= pgy_scalar_option_int_unwrap(' "$artifact" ||
            fail "C artifact omitted the Some payload write"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C artifact did not compile"
    else
        grep -Fq 'label %pgy.try.some.' "$artifact" ||
            fail "LLVM artifact omitted the Some edge"
        grep -Fq 'label %pgy.try.none.' "$artifact" ||
            fail "LLVM artifact omitted the None edge"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in payload-type enclosing-return try-edge option-abi value-result; do
    mutated_rel="$WORK_REL/$mutation.json"
    mutated="$ROOT_DIR/$mutated_rel"
    python "$MUTATIONS" "$MIR" "$mutation" "$mutated"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted the $mutation mutation"
        fi
        [[ ! -e "$output" ]] ||
            fail "$backend published an artifact for $mutation"
    done
done

echo "[$LABEL] Option<Int> try-let Some/None C/LLVM parity + negatives: PASS"
