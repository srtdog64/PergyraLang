#!/usr/bin/env bash
# A match scrutinee is one expression evaluation, not one evaluation per case
# condition or payload extraction. Native/self agreement alone is insufficient
# because those paths historically shared the duplication. This bounded rung
# targets the Pergyra-owned source/MIR/C path; the public Pergyra-owned LLVM path
# does not yet admit this Option-match structural shape.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_pipeline_step_owner.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
EXPLICIT_SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-}"
SELF_DRIVER="${EXPLICIT_SELF_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}"
SOURCE="tests/self_hosted/fixtures/match_scrutinee_single_evaluation.pgy"
CODEGEN_OWNER="src/self_hosted/codegen/emission/stmt_emit.pgy"
SEMANTIC_OWNER="src/self_hosted/semantic/ast_match_materialization_fact_owner.pgy"
MIR_OWNER="src/self_hosted/mir/routine_match_owner.pgy"
WORK_REL=".tmp/self_hosted/match_scrutinee_single_evaluation"
WORK_DIR="$ROOT_DIR/$WORK_REL"

fail() {
    echo "[self-host-match-single-evaluation] $*" >&2
    exit 1
}

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$SELF_DRIVER" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${SELF_DRIVER}.exe"; then
    SELF_DRIVER="${SELF_DRIVER}.exe"
fi
[[ -x "$PGY" ]] || fail "missing public launcher: $PGY"
[[ -x "$SELF_DRIVER" ]] || fail "missing installed Pergyra driver: $SELF_DRIVER"
[[ -f "$ROOT_DIR/$SOURCE" ]] || fail "missing fixture: $SOURCE"
[[ -f "$ROOT_DIR/$CODEGEN_OWNER" ]] || fail "missing owner: $CODEGEN_OWNER"
[[ -f "$ROOT_DIR/$SEMANTIC_OWNER" ]] || fail "missing owner: $SEMANTIC_OWNER"
[[ -f "$ROOT_DIR/$MIR_OWNER" ]] || fail "missing owner: $MIR_OWNER"
CC="${CC:-cc}"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"

grep -Fq 'let match_tmp_type: String = "";' \
    "$ROOT_DIR/$CODEGEN_OWNER" ||
    fail "match owner no longer materializes a typed temporary"
grep -Fq 'match_tmp_type = AbiLayoutCLocalType(' \
    "$ROOT_DIR/$CODEGEN_OWNER" ||
    fail "match temporary no longer derives its ABI type from the owner"
grep -Fq 'match_tmp_decl = Concat(match_tmp_decl, ";\n");' \
    "$ROOT_DIR/$CODEGEN_OWNER" ||
    fail "match temporary declaration is incomplete"
if grep -Fq 'case_fact, rendered_match_subject,' "$ROOT_DIR/$CODEGEN_OWNER" ||
    grep -Fq 'rendered_match_subject, env' "$ROOT_DIR/$CODEGEN_OWNER"; then
    fail "case or payload consumer re-opened the scrutinee expression"
fi
grep -Fq 'ProgramExpressionGraphAppendIsolatedNode(' \
    "$ROOT_DIR/$SEMANTIC_OWNER" ||
    fail "semantic owner no longer seals a synthetic binding graph"
grep -Fq 'SelfMirMatchMaterializationRowForNode(' "$ROOT_DIR/$MIR_OWNER" ||
    fail "MIR match owner no longer consumes the materialization fact"
grep -Fq '"AST_LET_DECL", source_uses' "$ROOT_DIR/$MIR_OWNER" ||
    fail "MIR match owner no longer defines the scrutinee value once"
grep -Fq 'match_value = synthetic_name;' "$ROOT_DIR/$MIR_OWNER" ||
    fail "MIR cases no longer consume the synthetic match value"
grep -Fq 'SelfMirMatchSyntheticGraphView(' "$ROOT_DIR/$MIR_OWNER" ||
    fail "MIR match owner no longer projects the synthetic graph by value"
if grep -Fq 'input.analysis.expression_surfaces.expression_graph' \
    "$ROOT_DIR/$MIR_OWNER"; then
    fail "MIR match owner stored a graph directly from its borrowed input"
fi
[[ "$(grep -Fc 'SemanticAstExpressionGraphForNode(' "$ROOT_DIR/$MIR_OWNER")" -eq 1 ]] ||
    fail "MIR match owner re-opened the source graph per case"

suffix=""
[[ "$PGY" == *.exe ]] && suffix=".exe"
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
printf 'probe-some\n7\nprobe-none\n0\n' >"$WORK_DIR/expected.out"

for backend in c; do
    program_rel="$WORK_REL/$backend-program$suffix"
    program="$ROOT_DIR/$program_rel"
    if [[ -n "$EXPLICIT_SELF_DRIVER" ]]; then
        compile_status=0
        (cd "$ROOT_DIR" && \
            "$SELF_DRIVER" "$SOURCE" --emit-c-verified) \
            >"$WORK_DIR/$backend.emitted.c" \
            2>"$WORK_DIR/$backend.compile.err" || compile_status=$?
        if [[ "$compile_status" -eq 0 ]] && \
            ! pgy_selfhost_driver_rung2_compile_emitted 0 \
                "$WORK_DIR/$backend.emitted.c" "$program" \
                "$WORK_DIR/$backend.cc.log"; then
            compile_status=1
            cat "$WORK_DIR/$backend.cc.log" >&2
        fi
    else
        compile_status=0
        (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && \
            "$PGY" "$SOURCE" "--backend=$backend" -o "$program_rel") \
            >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || compile_status=$?
    fi
    if [[ "$compile_status" -ne 0 ]]; then
        cat "$WORK_DIR/$backend.compile.err" >&2
        fail "installed $backend path could not compile the fixture"
    fi
    [[ -x "$program" ]] || fail "installed $backend path published no binary"
    "$program" | tr -d '\r' >"$WORK_DIR/$backend.run.out"
    if ! cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/$backend.run.out"; then
        echo "[self-host-match-single-evaluation] expected:" >&2
        cat "$WORK_DIR/expected.out" >&2
        echo "[self-host-match-single-evaluation] actual ($backend):" >&2
        cat "$WORK_DIR/$backend.run.out" >&2
        fail "$backend re-evaluated the match scrutinee"
    fi
done

echo "[self-host-match-single-evaluation] installed C evaluates each" \
     "side-effecting match scrutinee exactly once"
