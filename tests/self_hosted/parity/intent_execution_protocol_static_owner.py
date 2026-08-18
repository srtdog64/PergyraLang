#!/usr/bin/env python3
"""Static native-wire and self-host once-validation ratchet."""

import pathlib
import sys
assert len(sys.argv) == 2
root = pathlib.Path(sys.argv[1])
native_schema = root / "src/compiler/mir_intent_execution.h"
native_writer = root / "src/compiler/mir_json_dump_intent_execution.c"
native_digest = root / "src/compiler/mir_intent_execution.c"
native_graph = root / "src/compiler/mir_intent_execution_graph.c"
native_validator = root / "src/compiler/mir_intent_execution_validate.c"
native_c_consumer = root / "src/codegen/transpiler_intent_typed_execution.c"
native_llvm_consumer = root / "src/codegen/llvm_intent_typed_execution.c"
self_root = root / "src/self_hosted"
self_schema = self_root / "mir/intent_execution_schema_owner.pgy"
self_fact = self_root / "mir/intent_execution_fact_owner.pgy"
self_digest = self_root / "mir/intent_execution_digest_owner.pgy"
self_identity = self_root / "mir_lower/intent_execution_identity_index_owner.pgy"
makefile = root / "Makefile"


def require_terms(path, terms):
    text = path.read_text(encoding="utf-8")
    missing = [term for term in terms if term not in text]
    assert not missing, (path, missing)
    return text

require_terms(native_schema, ["pgy.selfhost.mir-intent-execution-plan.v3"])
require_terms(native_writer, [
    r'\"has_predecessor\":%s',
    r'\"compensations\":[',
    r'\"terminals\":[',
    "success_payload_decl_syntax_id",
    "failure_payload_decl_syntax_id",
    "source_payload_decl_syntax_id",
    "result_payload_decl_syntax_id",
    "where_zone_name",
    "where_zone_syntax_id",
    "mir_intent_execution_program_digest(mir)",
])
native_execution_text = require_terms(native_digest, [
    "intent_execution_hash_string",
    "intent_execution_hash_int",
    "intent_execution_digest_rows",
    "mir_intent_execution_program_digest",
    "payload_decl_syntax_id",
    "intent_execution_append_block(",
    "intent_execution_materialize_step(",
    "intent_execution_set_branch(",
    "intent_execution_set_goto(",
    "pgy_arena_strdup(\n            &routine->scratch, step->where_type_name)",
    "dir->nodes[step->where_type_node_id].source_syntax_id",
])
assert "row->where_zone_name = step->where_type_name" not in native_execution_text, (
    "MIR intent execution must not borrow DIR-owned zone spelling",
    native_digest,
)
require_terms(makefile, [
    "ASAN_UNIT_BATTERIES ?= test_air test_semantic test_parser test_mir",
])
require_terms(native_graph, [
    "intent_execution_materialize_step",
    "intent_execution_capture_compensations",
    "intent_execution_set_branch",
    "IntentOutcomeBranch",
    "IntentStepCompleted",
    "IntentSuccessPayload",
    "IntentFailurePayload",
    "mir_expression_graph_identity",
])
for forbidden in (
    "intent_execution_materialize_step(MIRRoutine *routine",
    "intent_execution_capture_compensations(MIRRoutine *routine",
):
    assert forbidden not in native_execution_text, (
        "top-level intent execution orchestration re-owned graph materialization",
        forbidden,
    )
validator_text = require_terms(native_validator, [
    "intent_execution_tobject_by_identity", "branch->payload_decl_syntax_id",
    "row->result_payload_decl_syntax_id",
    "step->where_zone_syntax_id", "AST_ZONE_DECL",
])
assert "intent_execution_tobject_by_name" not in validator_text
require_terms(native_c_consumer, [
    "pgy_intent_trace_step_export",
    "row->step_name, row->where_zone_name",
])
require_terms(native_llvm_consumer, [
    'llvm_lookup_function(ctx, "pgy_intent_trace_step_export")',
    "row->step_name, row->where_zone_name",
])
require_terms(self_schema, [
    "pgy.selfhost.mir-intent-execution-plan.v3",
    "success_payload_decl_syntax_ids",
    "failure_payload_decl_syntax_ids",
    "source_payload_decl_syntax_ids",
    "result_payload_decl_syntax_ids",
    "where_zone_names",
    "where_zone_syntax_ids",
])
identity_text = require_terms(self_identity, [
    "name: String, source_syntax_id: Int",
    "declarations.source_syntax_ids[row] == source_syntax_id",
    "payload_decl_syntax_id: Int",
    "MirIntentExecutionZoneDeclarationReady",
    'declarations.nominal_kinds[row] == "zone"',
])
assert "name: String\n) -> Bool" not in identity_text
for term in (
    "struct MirIntentExecutionPlan",
    "MirIntentExecutionPlanReady",
    "MirIntentExecutionPlanDigest",
):
    assert any(
        term in path.read_text(encoding="utf-8")
        for path in (self_schema, self_fact, self_digest)
    ), term

fallback_text = "\n".join(
    path.read_text(encoding="utf-8")
    for path in (self_schema, self_fact, self_digest)
)
for forbidden in ("source_order", "source order", "step_index - 1"):
    assert forbidden not in fallback_text, forbidden
require_terms(self_digest, [
    "let routine_anchor: Int = 0",
    "let first_for_routine: Bool = true",
    "plan.steps.routine_syntax_ids[step] == routine_syntax_id",
    "plan.terminals.routine_syntax_ids[terminal] ==",
])

calls = []
for path in self_root.rglob("*.pgy"):
    for line_number, line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), 1
    ):
        if "MirIntentExecutionPlanReady(" not in line:
            continue
        if line.lstrip().startswith("func MirIntentExecutionPlanReady("):
            continue
        calls.append((path, line_number, line.strip()))
assert len(calls) == 1, calls
assert calls[0][0].name == "machine_layer_fact_owner.pgy", calls

admission_owner = self_root / "mir_lower/intent_execution_plan_fact_owner.pgy"
admission_text = admission_owner.read_text(encoding="utf-8")
for required in (
    "MirIntentExecutionZoneDeclarationReady(",
    "plan.steps.where_zone_names[step]",
    "plan.steps.where_zone_syntax_ids[step]",
):
    assert required in admission_text, required
for forbidden in (
    "MirIntentExecutionPlanFromFacts(",
    "MirIntentExecutionPlanDigest(",
    "MirIntentStepTransitionFactsReady(",
    "MirIntentTerminalTransitionFactsReady(",
):
    assert forbidden not in admission_text, (
        "JSON admission must load an unchecked exact-wire carrier", forbidden
    )

full_revalidation_terms = (
    "MirIntentExecutionPlanReady(",
    "MirIntentExecutionPlanDigest(",
    "MirIntentStepTransitionFactsReady(",
    "MirIntentTerminalTransitionFactsReady(",
)
for forbidden_root in (self_root / "codegen", self_root / "compiler"):
    for path in forbidden_root.rglob("*.pgy"):
        text = path.read_text(encoding="utf-8")
        assert not any(term in text for term in full_revalidation_terms), (
            "intent plan/CFG/digest revalidation escaped admission", path
        )

projection_owner = self_root / "compiler/direct_mir_intent_plan_projection_owner.pgy"
projection_text = projection_owner.read_text(encoding="utf-8")
assert "plan: MirIntentExecutionPlan" in projection_text, (
    "projection must receive the admitted plan through a typed value boundary"
)
assert "let plan:" not in projection_text, (
    "projection must not detach a borrowed admitted member into a new binding"
)
bridge_owner = self_root / "compiler/codegen_callable_receiver_bridge_owner.pgy"
require_terms(bridge_owner, [
    "DirectMirIntentExecutionViewFromAdmitted(",
    "admitted, admitted.intent_execution_plan,",
])
assert "Clone(" not in projection_text, (
    "projection reintroduced polymorphic Clone at the admitted plan boundary",
    projection_owner,
)
for forbidden in (
    "SemanticAstExpressionGraphForNode(",
    "DirectMirIntentGraphSubtreeEquals(",
):
    assert forbidden not in projection_text, (
        "projection reopened recursive expression-graph validation",
        projection_owner, forbidden,
    )
