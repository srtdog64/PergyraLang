#!/usr/bin/env bash
# Namespace-internal short call spelling consumes carried callable SyntaxNodeId.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-namespace-internal-call"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_namespace_internal_call"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_scalar_namespace_internal_call.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"
MARKER_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_call_callee_identity_owner.pgy"
DECLARED_IDENTITY_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_declared_call_callee_identity_owner.pgy"
IDENTITY_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_identity_resolution_owner.pgy"
CALL_TARGET_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_call_target_capture_owner.pgy"
CALL_ARGUMENT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_call_with_arguments_admission_owner.pgy"
EXPECTED_TYPE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_call_argument_expected_type_owner.pgy"
ZERO_ARGUMENT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_zero_argument_call_admission_owner.pgy"
IDENTITY_BOUND_C_OWNER="$ROOT_DIR/src/self_hosted/codegen/emission/expr_semantic_identity_bound_call_emit_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$MUTATIONS" "$MARKER_OWNER" "$DECLARED_IDENTITY_OWNER" \
        "$IDENTITY_OWNER" "$CALL_TARGET_OWNER" "$CALL_ARGUMENT_OWNER" "$EXPECTED_TYPE_OWNER" \
        "$ZERO_ARGUMENT_OWNER" "$IDENTITY_BOUND_C_OWNER"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'sequence.arena.identities.call_target_syntax_ids[node] > 0' \
    "$MARKER_OWNER" || fail "call marker ignores carried SyntaxNodeId"
grep -Fq 'SemanticCallTargetNamespace()' "$MARKER_OWNER" ||
    fail "callee identity owner omits namespace-call topology"
grep -Fq 'func DirectMirScalarProgramDeclaredCallCalleeIdentityReady(' \
    "$DECLARED_IDENTITY_OWNER" || fail "declared-call callee identity owner is missing"
grep -Fq 'SemanticCallTargetNamespace()' "$IDENTITY_OWNER" ||
    fail "semantic identity owner omits namespace-call SyntaxNodeId"
grep -Fq 'graph.arena.topology.left_children[node + 1] ==' \
    "$IDENTITY_OWNER" || fail "semantic identity owner lost exact call-callee edge"
grep -Fq 'signatures, canonical_target_name' "$CALL_TARGET_OWNER" ||
    fail "call-target admission lost canonical declared-callable identity"
if grep -Fq 'signatures, UnwrapOption(source_name)' "$CALL_TARGET_OWNER"; then fail "call-target admission reopened namespace-local callee spelling"; fi
for owner in "$CALL_ARGUMENT_OWNER" "$EXPECTED_TYPE_OWNER" \
        "$ZERO_ARGUMENT_OWNER"; do
    if grep -Fq 'node_texts[callee] != callables.names' "$owner"; then fail "declared-call admission reopened callee display-text identity"; fi
done
grep -Fq '(formal && UnwrapOption(callee_name) != source_name)' \
    "$IDENTITY_BOUND_C_OWNER" ||
    fail "identity-bound C emitter lost formal-only spelling validation"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"text":"Fact1"' "$MIR" ||
    fail "producer omitted the namespace-local source spelling"
grep -Fq '"call_target_name":"InternalNames_Fact1"' "$MIR" ||
    fail "producer omitted the canonical namespace callable identity"
grep -Fq '"call_target_kind":"namespace","call_target_name":"InternalNames_Fact2"' "$MIR" ||
    fail "producer omitted the qualified namespace callable identity"
python - "$MIR" <<'PY'
import json, pathlib, sys

document = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
edges = []
for routine in document.get("routines", []):
    for block in routine.get("blocks", []):
        for instruction in block.get("instructions", []):
            for lane in ("expr0_graph", "expr1_graph"):
                graph = instruction.get(lane)
                if not isinstance(graph, dict):
                    continue
                nodes = graph.get("nodes", [])
                for call in nodes:
                    if (call.get("call_target_kind") != "direct" or
                            call.get("call_target_name") !=
                            "InternalNames_Fact1"):
                        continue
                    callee_index = call.get("left")
                    if (not isinstance(callee_index, int) or
                            callee_index < 0 or callee_index >= len(nodes)):
                        raise SystemExit("namespace-internal call has no exact callee edge")
                    edges.append((call, nodes[callee_index]))
if len(edges) != 1:
    raise SystemExit(f"expected one namespace-internal call edge, got {len(edges)}")
call, callee = edges[0]
target_id = call.get("call_target_syntax_id")
if (not isinstance(target_id, int) or target_id <= 0 or
        callee.get("kind") != "leaf" or callee.get("text") != "Fact1" or
        callee.get("binding_syntax_id") != target_id or
        callee.get("binding_kind") != "declared_callable" or
        callee.get("binding_ordinal") is not None):
    raise SystemExit("namespace-internal callee did not carry its call target identity")
PY
printf 'namespace:internal-ready\n' >"$WORK_DIR/expected.run"

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
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
    else
        command=("$CLANG" -x ir "$artifact" -o "$bin")
    fi
    "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
        2>"$WORK_DIR/$backend.compile.err" || {
            cat "$WORK_DIR/$backend.compile.err" >&2
            fail "$backend artifact did not compile"
        }
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in namespace-internal-call-syntax-id \
        namespace-qualified-call-syntax-id \
        namespace-internal-callee-binding-missing \
        namespace-internal-callee-binding-crossed; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$mutated_rel" -o "$output_rel") \
                >"$WORK_DIR/$mutation.$backend.out" \
                2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation without SyntaxNodeId"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published an artifact for $mutation"
    done
done
echo "[$LABEL] namespace-internal direct call C/LLVM parity + negative: PASS"
