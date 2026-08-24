#!/usr/bin/env bash
# The self-host DIR debug artifact consumes exact admitted node/edge rows.
# Numeric source syntax IDs are producer-local, but node indexes, resolved
# indexes, row ordering, kinds, names, labels, and topology must match the
# independent native DIR oracle.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[self-host-dir-inventory] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "dir-inventory" "$PGY" \
    || fail "PGY_BIN is not runnable"

PYTHON_BIN="${PYTHON:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        fail "python3/python is required"
    fi
fi

INVENTORY="$ROOT_DIR/src/self_hosted/dir/domain_graph_inventory_owner.pgy"
ROW_OWNER="$ROOT_DIR/src/self_hosted/dir/domain_graph_row_owner.pgy"
RENDERER="$ROOT_DIR/src/self_hosted/compiler/dir_text_artifact_owner.pgy"
INTENT_RENDERER="$ROOT_DIR/src/self_hosted/compiler/dir_intent_text_artifact_owner.pgy"
INTENT_PROVENANCE="$ROOT_DIR/src/self_hosted/dir/intent_step_provenance_fact_owner.pgy"
INTENT_HEADER="$ROOT_DIR/src/self_hosted/semantic/ast_intent_transition_row_owner.pgy"
INTENT_DEFAULTS="$ROOT_DIR/src/self_hosted/parser/intent_default_clause_owner.pgy"
INTENT_ROW="$ROOT_DIR/src/self_hosted/dir/intent_row_owner.pgy"
INTENT_CARRIAGE="$ROOT_DIR/src/self_hosted/dir/intent_step_carriage_contract_owner.pgy"
AST_KINDS="$ROOT_DIR/src/self_hosted/hir/ast_node_kind_owner.pgy"
DOMAIN_CENSUS="$ROOT_DIR/src/self_hosted/dir/domain_graph_fact_owner.pgy"
ZONE_STATE="$ROOT_DIR/src/self_hosted/dir/zone_state_row_fact_owner.pgy"
ZONE_PARSER="$ROOT_DIR/src/self_hosted/parser/decl_zone_owner.pgy"
PROBE="$ROOT_DIR/tests/self_hosted/fixtures/dir_graph_inventory_probe.pgy"
BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/dir_graph_inventory"
PROBE_BIN="$BUILD_DIR/dir_graph_inventory_probe.exe"

mkdir -p "$BUILD_DIR"

for term in 'struct SelfDirGraphInventoryFacts' \
    'SelfDirGraphInventoryAppendNode' \
    'SelfDirGraphInventoryAppendEdge' \
    'SelfDirGraphInventoryNodeIndex'; do
    grep -Fq -- "$term" "$ROW_OWNER" \
        || fail "missing exact graph row owner term: $term"
done
for term in 'SelfDirGraphInventoryFactsFromAdmittedFacts' \
    'SelfDirGraphInventoryAppendPrimaryNodes' \
    'SelfDirGraphInventoryAppendProjectionTopologyEdges' \
    'zone_states: SelfDirZoneStateRows' \
    'facts, "zone-state", node_id, node_id' \
    'SelfDirGraphInventoryFactsReady'; do
    grep -Fq -- "$term" "$INVENTORY" \
        || fail "missing exact inventory owner term: $term"
done
for term in 'struct SelfDirZoneStateRows' \
    'SelfDirZoneStateRowsFromArtifact' \
    'SelfDirFieldSourceSyntaxId(' \
    'SelfDirZoneStateRowsReady'; do
    grep -Fq -- "$term" "$ZONE_STATE" \
        || fail "missing exact zone-state row owner term: $term"
done
grep -Fq 'ParserCharAt(content, i) != "\n"' "$ZONE_PARSER" \
    || fail "zone state parser lost optional-semicolon line termination"
state_parser_body="$(sed -n \
    '/LanguageWordId.WordState)/,/LanguageWordId.WordBind)/p' \
    "$ZONE_PARSER")"
if grep -Fq 'i = Expect(content, i, ";");' <<<"$state_parser_body"; then
    fail "zone state parser restored mandatory semicolon admission"
fi
for term in 'CompilerDirTextArtifactFromProjection' \
    'SelfDirGraphInventoryFactsFromAdmittedFacts(' \
    'self-host DIR node/edge census diverged from its inventory' \
    'CompilerDirIntentTextFromFacts('; do
    grep -Fq -- "$term" "$RENDERER" \
        || fail "missing admitted DIR renderer term: $term"
done
for term in 'CompilerDirIntentTextFromFacts' \
    'facts.steps.provenance' \
    'authorized_by_provenance action-inherited'; do
    grep -Fq -- "$term" "$INTENT_RENDERER" \
        || fail "missing exact intent text owner term: $term"
done
for term in 'struct SelfDirIntentStepProvenanceFacts' \
    'SelfDirIntentStepResolvedProvenance' \
    'SelfDirIntentStepContractProvenanceFromText' \
    'SelfDirIntentStepProvenanceFactsReady'; do
    grep -Fq -- "$term" "$INTENT_PROVENANCE" \
        || fail "missing typed intent provenance term: $term"
done
for term in 'ParserIntentDefaultClauseFromSource' \
    'ParserIntentStepDefaultsResolve' \
    'reused who from intent-level default' \
    'reused zone from intent-level default'; do
    grep -Fq -- "$term" "$INTENT_DEFAULTS" \
        || fail "missing intent-level default owner term: $term"
done
grep -Fq 'TypedAstKindIntentStepContractProvenanceTag() -> Int { return 88; }' \
    "$AST_KINDS" || fail "intent contract provenance lost its appended AST identity"
grep -Fq 'TypedAstKindIntentStepIntentTag() -> Int { return 89; }' \
    "$AST_KINDS" || fail "inline intent target lost its appended AST identity"
grep -Fq 'TypedAstKindIntentPriorityTag() -> Int { return 90; }' \
    "$AST_KINDS" || fail "intent priority lost its appended AST identity"
for term in 'struct SemanticAstIntentStepHeaderRow' \
    'SemanticAstIntentStepHeaderFromText' \
    'intent_text: String' \
    'transfer_from_alias' \
    'transfer_to_alias'; do
    grep -Fq -- "$term" "$INTENT_HEADER" \
        || fail "missing typed intent-step header term: $term"
done
grep -Fq 'target_expression_node_ids: Array<Int>;' "$INTENT_ROW" \
    || fail "intent step lost its exact target-expression carrier"
for term in 'steps.target_kinds[i] == "action"' \
    'TypedAstKindIntentStepOnTag()' \
    'TypedAstKindIntentStepIntentTag()' \
    'steps.target_expression_node_ids[i]'; do
    grep -Fq -- "$term" "$INTENT_CARRIAGE" \
        || fail "intent carriage lost target-kind AST validation: $term"
done
grep -Fq 'target_ready = target_ready || TypedAstArenaNodeKindIs(' \
    "$INTENT_CARRIAGE" ||
    fail "nested intent on:/intent: syntax carriage is not admitted"
if grep -Eq '(^|[^A-Za-z0-9_])on_node_ids([^A-Za-z0-9_]|$)' \
    "$INTENT_ROW" "$INTENT_CARRIAGE"; then
    fail "action-only on-node carriage reappeared"
fi
if grep -Eq 'TypedAstArenaProvenanceText|native_oracle|SelfDirDomainGraphAnchor' \
    "$INVENTORY" "$ROW_OWNER" "$RENDERER" "$INTENT_RENDERER"; then
    fail "DIR inventory reopened provenance, native-oracle, or count-anchor reconstruction"
fi
if grep -Fq 'domain_graph_inventory_owner.pgy' "$DOMAIN_CENSUS"; then
    fail "normal MIR domain census rebuilt the debug program inventory"
fi
[[ "$(wc -l <"$INVENTORY" | tr -d ' ')" -le 650 ]] \
    || fail "DIR graph inventory owner exceeds 650 lines"
[[ "$(wc -l <"$ROW_OWNER" | tr -d ' ')" -le 190 ]] \
    || fail "DIR graph row owner exceeds 190 lines"
[[ "$(wc -l <"$ZONE_STATE" | tr -d ' ')" -le 300 ]] \
    || fail "DIR zone-state row owner exceeds 300 lines"
[[ "$(wc -l <"$RENDERER" | tr -d ' ')" -le 220 ]] \
    || fail "DIR text artifact owner exceeds 220 lines"
[[ "$(wc -l <"$INTENT_RENDERER" | tr -d ' ')" -le 220 ]] \
    || fail "DIR intent text artifact owner exceeds 220 lines"
[[ "$(wc -l <"$INTENT_PROVENANCE" | tr -d ' ')" -le 190 ]] \
    || fail "DIR intent provenance owner exceeds 190 lines"
[[ "$(wc -l <"$INTENT_HEADER" | tr -d ' ')" -le 240 ]] \
    || fail "semantic intent-step header owner exceeds 240 lines"
[[ "$(wc -l <"$INTENT_DEFAULTS" | tr -d ' ')" -le 100 ]] \
    || fail "intent-level default owner exceeds 100 lines"

(cd "$ROOT_DIR" && "$PGY" --native-pipeline \
    "$(pgy_path_for_compiler "$PGY" "$PROBE")" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$PROBE_BIN")" >/dev/null)
[[ -s "$PROBE_BIN" ]] || fail "DIR inventory probe was not built"

assert_compact_ast_equal() {
    local case_name="$1"
    local source_rel="$2"
    (cd "$ROOT_DIR" && "$PROBE_BIN" --ast "$source_rel") \
        >"$BUILD_DIR/$case_name.self.ast" \
        2>"$BUILD_DIR/$case_name.self.ast.err" \
        || fail "$case_name self AST projection failed"
    (cd "$ROOT_DIR" && "$PGY" --native-pipeline --ast "$source_rel") \
        >"$BUILD_DIR/$case_name.native.ast" \
        2>"$BUILD_DIR/$case_name.native.ast.err" \
        || fail "$case_name native AST oracle failed"
    pgy_selfhost_normalize_text_artifact \
        <"$BUILD_DIR/$case_name.self.ast" \
        >"$BUILD_DIR/$case_name.self.ast.norm"
    pgy_selfhost_normalize_text_artifact \
        <"$BUILD_DIR/$case_name.native.ast" \
        >"$BUILD_DIR/$case_name.native.ast.norm"
    cmp -s "$BUILD_DIR/$case_name.self.ast.norm" \
        "$BUILD_DIR/$case_name.native.ast.norm" \
        || fail "$case_name compact AST differs from native"
}

assert_compact_ast_equal intent-defaults \
    tests/self_hosted/parity/fixture/dir_intent_defaults.pgy
assert_compact_ast_equal intent-transfer-move \
    examples/transfer_move_minimal.pgy

normalize_dir() {
    "$PYTHON_BIN" - "$1" "$2" <<'PY'
import re
import sys

source_path, output_path = sys.argv[1:]
with open(source_path, "r", encoding="utf-8") as stream:
    text = stream.read().replace("\r\n", "\n").replace("\r", "\n")

# AST stable IDs belong to each producer. DIR-local row IDs and resolved
# references do not: those remain byte-significant below.
text = re.sub(
    r" source=\d+ owner_source=\d+",
    " source=<producer-id> owner_source=<producer-id>",
    text,
)
text = re.sub(
    r"(topology\[\d+\].*? source=)\d+",
    r"\1<producer-id>",
    text,
)
with open(output_path, "w", encoding="utf-8", newline="\n") as stream:
    stream.write(text)
PY
}

assert_inventory_shape() {
    "$PYTHON_BIN" - "$1" <<'PY'
import re
import sys

text = open(sys.argv[1], encoding="utf-8").read()
node_count = int(re.search(r"^  nodes: (\d+)$", text, re.M).group(1))
edge_count = int(re.search(r"^  edges: (\d+)$", text, re.M).group(1))
nodes = re.findall(r"^  node\[\d+\] ", text, re.M)
edges = re.findall(r"^  edge\[\d+\] ", text, re.M)
if len(nodes) != node_count or len(edges) != edge_count:
    raise SystemExit("header census does not equal materialized inventory")
PY
}

while IFS='|' read -r case_name source_rel; do
    self_out="$BUILD_DIR/$case_name.self"
    native_out="$BUILD_DIR/$case_name.native"
    (cd "$ROOT_DIR" && "$PROBE_BIN" "$source_rel") \
        >"$self_out" 2>"$self_out.err" \
        || fail "$case_name self-host DIR projection failed"
    (cd "$ROOT_DIR" && "$PGY" --native-pipeline --dir "$source_rel") \
        >"$native_out" 2>"$native_out.err" \
        || fail "$case_name native DIR oracle failed"
    normalize_dir "$self_out" "$self_out.norm"
    normalize_dir "$native_out" "$native_out.norm"
    assert_inventory_shape "$self_out.norm"
    assert_inventory_shape "$native_out.norm"
    cmp -s "$self_out.norm" "$native_out.norm" \
        || {
            diff -u "$native_out.norm" "$self_out.norm" >&2 || true
            fail "$case_name node/edge/topology inventory differs from native"
        }
done <<'CASES'
domain_authority|examples/function_clause_order_minimal.pgy
party_role|tests/cases/backend_compare/party_role_bind/main.pgy
relation_effect_topology|tests/self_hosted/parity/fixture/domain_topology_semicolon_legacy.pgy
zone_state_optional_semicolon|tests/self_hosted/parity/fixture/dir_zone_state_semicolon_optional.pgy
intent_participant|tests/cases/backend_compare/intent_minimal/main.pgy
intent_explicit_step|tests/cases/abi_pipeline/intent_active_abi/main.pgy
intent_action_defaults|tests/self_hosted/parity/fixture/intent_callable_reachability.pgy
intent_level_defaults|tests/self_hosted/parity/fixture/dir_intent_defaults.pgy
intent_transfer_derived|examples/intent_contract_derivation_minimal.pgy
intent_transfer_explicit|tests/cases/abi_pipeline/intent_authority_snapshot_abi/main.pgy
intent_transfer_move|examples/transfer_move_minimal.pgy
intent_inline_subintent|examples/composite_intent_orchestration_explicit.pgy
intent_nested_on_subintent|tests/self_hosted/parity/fixture/intent_nested_call_reachability.pgy
CASES

cp "$BUILD_DIR/domain_authority.self.norm" "$BUILD_DIR/mutated.norm"
"$PYTHON_BIN" - "$BUILD_DIR/mutated.norm" <<'PY'
import sys

path = sys.argv[1]
text = open(path, encoding="utf-8").read()
mutated = text.replace("edge[00] role-for", "edge[00] wrong-edge", 1)
if mutated == text:
    raise SystemExit("row mutation target was not found")
open(path, "w", encoding="utf-8", newline="\n").write(mutated)
PY
if cmp -s "$BUILD_DIR/mutated.norm" \
    "$BUILD_DIR/domain_authority.native.norm"; then
    fail "count-preserving edge mutation passed the inventory comparator"
fi

cp "$BUILD_DIR/intent_action_defaults.self.norm" \
    "$BUILD_DIR/intent-provenance-mutated.norm"
"$PYTHON_BIN" - "$BUILD_DIR/intent-provenance-mutated.norm" <<'PY'
import sys

path = sys.argv[1]
text = open(path, encoding="utf-8").read()
mutated = text.replace(
    "who-derived=on-receiver", "who-default=action", 1
)
if mutated == text:
    raise SystemExit("intent provenance mutation target was not found")
open(path, "w", encoding="utf-8", newline="\n").write(mutated)
PY
if cmp -s "$BUILD_DIR/intent-provenance-mutated.norm" \
    "$BUILD_DIR/intent_action_defaults.native.norm"; then
    fail "count-preserving intent provenance mutation passed"
fi

cp "$BUILD_DIR/intent_level_defaults.self.norm" \
    "$BUILD_DIR/intent-default-mutated.norm"
"$PYTHON_BIN" - "$BUILD_DIR/intent-default-mutated.norm" <<'PY'
import sys

path = sys.argv[1]
text = open(path, encoding="utf-8").read()
mutated = text.replace("who-default=intent", "who-derived=on-receiver", 1)
if mutated == text:
    raise SystemExit("intent-level default mutation target was not found")
open(path, "w", encoding="utf-8", newline="\n").write(mutated)
PY
if cmp -s "$BUILD_DIR/intent-default-mutated.norm" \
    "$BUILD_DIR/intent_level_defaults.native.norm"; then
    fail "count-preserving intent-level default mutation passed"
fi

cp "$BUILD_DIR/intent_transfer_derived.self.norm" \
    "$BUILD_DIR/intent-transfer-mutated.norm"
"$PYTHON_BIN" - "$BUILD_DIR/intent-transfer-mutated.norm" <<'PY'
import sys

path = sys.argv[1]
text = open(path, encoding="utf-8").read()
mutated = text.replace("transfer=cart->payment", "transfer=payment->cart", 1)
if mutated == text:
    raise SystemExit("intent transfer mutation target was not found")
open(path, "w", encoding="utf-8", newline="\n").write(mutated)
PY
if cmp -s "$BUILD_DIR/intent-transfer-mutated.norm" \
    "$BUILD_DIR/intent_transfer_derived.native.norm"; then
    fail "count-preserving intent transfer mutation passed"
fi

set +e
(cd "$ROOT_DIR" && "$PROBE_BIN" \
    tests/self_hosted/parity/fixture/dir_zone_state_wrong_endpoint.pgy) \
    >"$BUILD_DIR/state-invalid.out" 2>"$BUILD_DIR/state-invalid.err"
state_invalid_rc=$?
set -e
[[ "$state_invalid_rc" -ne 0 ]] \
    || fail "wrong zone-state endpoint silently produced a successful DIR"
grep -Fq 'zone-state fields do not join the zone' \
    "$BUILD_DIR/state-invalid.out" "$BUILD_DIR/state-invalid.err" \
    || fail "wrong zone-state endpoint lost its typed-row diagnostic"
set +e
(cd "$ROOT_DIR" && "$PGY" --native-pipeline --dir \
    tests/self_hosted/parity/fixture/dir_zone_state_wrong_endpoint.pgy) \
    >"$BUILD_DIR/state-invalid.native.out" \
    2>"$BUILD_DIR/state-invalid.native.err"
state_invalid_native_rc=$?
set -e
[[ "$state_invalid_native_rc" -ne 0 ]] \
    || fail "native oracle accepted the wrong zone-state endpoint"
grep -Fq "references unknown target slot 'missing'" \
    "$BUILD_DIR/state-invalid.native.out" \
    "$BUILD_DIR/state-invalid.native.err" \
    || fail "native wrong-endpoint oracle lost its semantic diagnostic"

echo "[self-host-dir-inventory] exact rows including zone state, admitted intent defaults, transfer detail, and inline sub-intents match native; count-only, provenance/transfer mutations, and wrong-endpoint paths fail closed"
