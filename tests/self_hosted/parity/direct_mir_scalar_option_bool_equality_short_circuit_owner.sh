#!/usr/bin/env bash
# Bool equality over Option<Bool> unwrap must remain on the logical RHS edge.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-option-bool-equality-short-circuit"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_option_bool_equality_short_circuit"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_scalar_option_bool_equality_short_circuit.pgy"
MIR_REL="$WORK_REL/producer.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_option_bool_equality_short_circuit_mutations.py"
KIND_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_id_owner.pgy"
BOOL_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_bool_readiness_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for path in "$MUTATIONS" "$KIND_OWNER" "$BOOL_OWNER"; do
    [[ -f "$path" ]] || fail "missing owner: ${path#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'func DirectMirScalarProgramExprEqualBool() -> Int { return 72; }' \
    "$KIND_OWNER" || fail "stable Bool equality identity is missing"
grep -Fq 'DirectMirScalarProgramExprEqualBool()' "$BOOL_OWNER" ||
    fail "Bool equality lost its recursive non-trapping proof"
! grep -Fq 'Matches' "$KIND_OWNER" ||
    fail "expression identity branched on the fixture routine"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
mir_sha="$(sha256sum "$MIR" | awk '{print toupper($1)}')"
[[ "$mir_sha" == "0F277B559FDE05498591D64B38B6D3046512B85D308023EB7AA479F17251AFDA" ]] ||
    fail "source MIR identity changed: $mir_sha"
grep -Fq '"kind":"equality"' "$MIR" || fail "producer omitted Bool equality"
grep -Fq '"abi_type_name":"Option<Bool>"' "$MIR" ||
    fail "producer omitted the Option<Bool> ABI receipt"

printf '1\n0\n0\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq '&&' "$artifact" || fail "C artifact lost short-circuit syntax"
        grep -Fq 'pgy_scalar_option_bool_unwrap' "$artifact" ||
            fail "C artifact omitted Option<Bool> unwrap"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >/dev/null 2>"$WORK_DIR/$backend.compile.err" ||
            fail "C artifact did not compile"
    else
        python - "$artifact" <<'PY'
import pathlib, sys
text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
unwrap = text.find("call i1 @pgy.scalar.option.bool.unwrap(")
right = text.rfind(".right:\n", 0, unwrap)
equality = text.find(" = icmp eq i1 ", unwrap)
merge = text.find(".merge:\n", equality)
if min(unwrap, right, equality, merge) < 0 or not right < unwrap < equality < merge:
    raise SystemExit("LLVM unwrap/equality escaped the conditional right edge")
PY
        "$CLANG" -x ir "$artifact" -o "$bin" >/dev/null \
            2>"$WORK_DIR/$backend.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in equality-kind equality-right-type unwrap-call-identity \
        missing-local-use option-abi-layout short-circuit-edge; do
    mutated_rel="$WORK_REL/$mutation.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >/dev/null 2>&1; then
            fail "$backend accepted the $mutation mutation"
        fi
        [[ ! -e "$output" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] Option<Bool> equality short-circuit C/LLVM parity + negatives: PASS"
