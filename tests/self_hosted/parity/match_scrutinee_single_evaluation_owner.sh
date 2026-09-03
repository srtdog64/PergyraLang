#!/usr/bin/env bash
# A match scrutinee is one expression evaluation, not one evaluation per case
# condition or payload extraction. Native/self agreement alone is insufficient
# because those paths historically shared the duplication. This bounded rung
# targets one canonical Pergyra-owned MIR artifact and both C/LLVM projections.
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
MATCH_LOCAL_FACT_OWNER="src/self_hosted/mir_lower/match_binding_local_fact_owner.pgy"
WIRE_OWNER="src/self_hosted/compiler/direct_mir_scalar_cfg_wire_local_ref_owner.pgy"
SCOPED_LOCAL_OWNER="src/self_hosted/compiler/direct_mir_scalar_cfg_scoped_match_binding_operand_owner.pgy"
OPTION_MATCH_OWNER="src/self_hosted/compiler/direct_mir_scalar_program_option_int_match_condition_owner.pgy"
OPTION_MATCH_TARGET="src/self_hosted/compiler/direct_mir_scalar_program_option_int_match_target_expression_owner.pgy"
OPTION_C_OWNER="src/self_hosted/compiler/direct_mir_scalar_program_c_option_int_owner.pgy"
OPTION_C_ACCESSOR_OWNER="src/self_hosted/compiler/direct_mir_scalar_program_c_option_int_accessor_preamble_owner.pgy"
OPTION_LLVM_OWNER="src/self_hosted/compiler/direct_mir_scalar_program_llvm_option_int_owner.pgy"
MUTATIONS="tests/self_hosted/parity/match_scrutinee_single_evaluation_mutations.py"
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
for owner in "$MATCH_LOCAL_FACT_OWNER" "$WIRE_OWNER" "$SCOPED_LOCAL_OWNER" \
        "$OPTION_MATCH_OWNER" "$OPTION_MATCH_TARGET" "$OPTION_C_OWNER" \
        "$OPTION_C_ACCESSOR_OWNER" "$OPTION_LLVM_OWNER" "$MUTATIONS"; do
    [[ -f "$ROOT_DIR/$owner" ]] || fail "missing owner: $owner"
done
CC="${CC:-cc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

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
grep -Fq 'ArrayPush(names, name);' "$ROOT_DIR/$MATCH_LOCAL_FACT_OWNER" ||
    fail "match-binding fact owner merged instruction-scoped identities"
! grep -Fq 'let existing: String' "$ROOT_DIR/$MATCH_LOCAL_FACT_OWNER" ||
    fail "match-binding fact owner reintroduced name/type deduplication"
grep -Fq 'index.declared_source_local_count' "$ROOT_DIR/$WIRE_OWNER" ||
    fail "wire collision owner includes instruction-scoped match facts"
grep -Fq 'MirRoutineBlockDominates(index, scope, use_block)' \
    "$ROOT_DIR/$SCOPED_LOCAL_OWNER" ||
    fail "match binding use no longer consumes CFG dominance"
grep -Fq 'DirectMirOptionMatchAbiFactReady(option_int_abi)' \
    "$ROOT_DIR/$OPTION_MATCH_OWNER" ||
    fail "Option match condition bypasses its ABI receipt"
grep -Fq 'pgy_scalar_option_int_payload' "$ROOT_DIR/$OPTION_MATCH_TARGET" ||
    fail "Option match target omitted payload binding"
grep -Fq 'fact.value_name' "$ROOT_DIR/$OPTION_C_ACCESSOR_OWNER" ||
    fail "C Option payload helper guessed the ABI field"
grep -Fq 'ToString(abi.value_index)' "$ROOT_DIR/$OPTION_LLVM_OWNER" ||
    fail "LLVM Option payload helper guessed the ABI field index"

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

mir_rel="$WORK_REL/program.mir.json"
mir="$ROOT_DIR/$mir_rel"
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-verified "$SOURCE" \
    -o "$mir_rel") >"$WORK_DIR/mir.out" 2>"$WORK_DIR/mir.err" || {
        cat "$WORK_DIR/mir.out" "$WORK_DIR/mir.err" >&2
        fail "self-host driver could not produce canonical MIR"
    }
[[ -s "$mir" ]] || fail "self-host driver published no canonical MIR"

for backend in c llvm; do
    extension="$backend"
    [[ "$backend" == llvm ]] && extension="ll"
    artifact_rel="$WORK_REL/program.$extension"
    artifact="$ROOT_DIR/$artifact_rel"
    binary="$WORK_DIR/mir-$backend-program$suffix"
    (cd "$ROOT_DIR" && "$SELF_DRIVER" "--mir-json-backend=$backend" \
        "$mir_rel" -o "$artifact_rel") \
        >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" \
                "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection rejected canonical match MIR"
        }
    [[ -s "$artifact" ]] || fail "$backend projection published no artifact"
    if [[ "$backend" == c ]]; then
        pgy_selfhost_driver_rung2_compile_emitted 0 "$artifact" "$binary" \
            "$WORK_DIR/$backend.compile.log" || {
                cat "$WORK_DIR/$backend.compile.log" >&2
                fail "projected C artifact did not compile"
            }
    else
        "$CLANG" -x ir "$artifact" -o "$binary" \
            >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || {
                cat "$WORK_DIR/$backend.compile.err" >&2
                fail "projected LLVM artifact did not compile"
            }
    fi
    "$binary" | tr -d '\r' >"$WORK_DIR/$backend.mir.run.out"
    cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/$backend.mir.run.out" ||
        fail "$backend canonical-MIR runtime output drifted"
done

for mutation in binding-type option-tag; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    mutated="$ROOT_DIR/$mutated_rel"
    python "$ROOT_DIR/$MUTATIONS" "$mir" "$mutation" "$mutated"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$SELF_DRIVER" \
            "--mir-json-backend=$backend" "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation mutation"
        fi
        [[ ! -e "$output" ]] ||
            fail "$backend published an artifact for $mutation mutation"
    done
done

echo "[self-host-match-single-evaluation] installed C plus canonical-MIR" \
     "C/LLVM evaluate each side-effecting match scrutinee exactly once;" \
     "owned negatives fail closed"
