#!/usr/bin/env bash
# Bool short-circuit over the canonical five-argument SubEqualsWithLen ABI.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-bool-sub-equals-short-circuit"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_bool_sub_equals_short_circuit"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_scalar_bool_sub_equals_short_circuit.pgy"
MIR_REL="$WORK_REL/producer.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_bool_sub_equals_short_circuit_mutations.py"
SIGNATURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_string_window_builtin_signature_owner.pgy"
RUNTIME="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_string_runtime_requirement_owner.pgy"
SHORT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_short_circuit_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for path in "$MUTATIONS" "$SIGNATURE" "$RUNTIME" "$SHORT"; do
    [[ -f "$path" ]] || fail "missing owner: ${path#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'DirectMirScalarProgramExprSubEqualsWithLen())' "$SIGNATURE" ||
    fail "builtin signature omitted stable SubEqualsWithLen identity"
grep -Fq '"string_int_string_int_to_bool"' "$RUNTIME" ||
    fail "runtime requirement omitted the canonical call shape"
grep -Fq '!DirectMirScalarProgramNonTrappingBoolNode(expressions, right)' \
    "$SHORT" || fail "LLVM short-circuit owner lost conditional evaluation"
! grep -Fq 'JsonTrueAt' "$SIGNATURE" ||
    fail "builtin signature branched on the fixture routine"
! grep -Fq 'JsonValueEnd' "$SIGNATURE" ||
    fail "builtin signature reintroduced raw MIR parsing"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
mir_sha="$(sha256sum "$MIR" | awk '{print toupper($1)}')"
[[ "$mir_sha" == "C4B2EBDA98EE77AF04BABECB6A78CF3467EA7EE2332A0A5E6EBB43AEE08F48F7" ]] ||
    fail "source MIR identity changed: $mir_sha"
grep -Fq '"kind":"logical_and"' "$MIR" ||
    fail "producer omitted persisted logical_and topology"
grep -Fq '"call_target_name":"SubEqualsWithLen"' "$MIR" ||
    fail "producer omitted the semantic builtin identity"

printf '1\n0\n' >"$WORK_DIR/expected.run"
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
        grep -Fq '&& pgy_subequals_with_len(' "$artifact" ||
            fail "C artifact lost language-level short-circuit evaluation"
        grep -Fq 'static int pgy_subequals_with_len(' "$artifact" ||
            fail "C artifact omitted the admitted runtime body"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C artifact did not compile"
    else
        python - "$artifact" <<'PY'
import pathlib, sys
text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
call = text.find("call i1 @pgy_subequals_with_len(")
right = text.rfind(".right:\n", 0, call)
merge = text.find(".merge:\n", call)
if min(call, right, merge) < 0 or not right < call < merge:
    raise SystemExit("LLVM runtime call is not confined to the right edge")
PY
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in call-name call-kind call-syntax-id argument-chain short-circuit-edge; do
    mutated_rel="$WORK_REL/$mutation.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
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

echo "[$LABEL] Bool SubEqualsWithLen short-circuit C/LLVM parity + negatives: PASS"
