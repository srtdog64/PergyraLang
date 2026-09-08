#!/usr/bin/env python3
"""Structural residue gate only; execution and escape gates own correctness."""
from pathlib import Path
import re
import sys

# Reached full-driver read queries. These pins are test expectations, not a
# semantic owner or a complete call/alias graph. Mutable producers are excluded.
READ_QUERIES = {
    "src/self_hosted/codegen/input/callable_receiver_codegen_view_owner.pgy": {
        "CodegenCallableReceiverFactsFromAdmittedRowsOrDie": ["routine_source_syntax_ids:Array<String>","routine_kinds:Array<String>","routine_owners:Array<String>","routine_names:Array<String>","routine_receiver_carriages:Array<String>"],
    },
    "src/self_hosted/compiler/runtime_value_representation_owner.pgy": {
        "CompilerRuntimeValueCLocalPreamble": ["local_types:Array<String>","parameter_types:Array<String>"],
        "CompilerRuntimeValueLlvmLocalPreamble": ["local_types:Array<String>","parameter_types:Array<String>"],
        "CompilerRuntimeValueTypesPresent": ["local_types:Array<String>","parameter_types:Array<String>"],
    },
    "src/self_hosted/compiler/direct_mir_array_int_emission_owner.pgy": {
        "DirectMirArrayIntCElementList": ["values:Array<Int>"],
        "DirectMirArrayIntEmitLlvmValue": ["values:Array<Int>"],
    },
    "src/self_hosted/compiler/direct_mir_composite_intent_program_plan_owner.pgy": {
        "DirectMirCompositeIntentNameIndex": ["names:Array<String>"],
        "DirectMirCompositeIntentObservabilitySequenceReady": ["runtime_call_abi_ids:Array<Int>"],
    },
    "src/self_hosted/compiler/direct_mir_composite_intent_program_graph_fact_owner.pgy": {
        "DirectMirCompositeIntentStringsDistinct": ["values:Array<String>"],
    },
    "src/self_hosted/compiler/direct_mir_legacy_intent_program_plan_owner.pgy": {
        "DirectMirLegacyIntentCleanupRowsReady": ["names:Array<String>","slots:Array<String>","args:Array<String>"],
    },
    "src/self_hosted/compiler/direct_mir_legacy_intent_program_llvm_emission_owner.pgy": {
        "DirectMirLegacyIntentLlvmCleanupCalls": ["names:Array<String>","slots:Array<String>","args:Array<String>"],
        "DirectMirLegacyIntentLlvmCleanupGlobals": ["names:Array<String>","slots:Array<String>","args:Array<String>"],
    },
    "src/self_hosted/compiler/direct_mir_scalar_cfg_local_ref_identity_owner.pgy": {
        "DirectMirScalarCfgLocalIdentityRow": ["identities:Array<String>"],
    },
    "src/self_hosted/compiler/direct_mir_scalar_cfg_local_ref_plan_owner.pgy": {
        "DirectMirScalarCfgLocalRefsReady": ["local_names:Array<String>","local_ref_kinds:Array<Int>","local_ref_value_ids:Array<String>","local_ref_syntax_ids:Array<Int>","value_names:Array<String>","value_local_rows:Array<Int>","value_definition_blocks:Array<Int>"],
    },
    "src/self_hosted/compiler/direct_mir_scalar_cfg_routine_partition_fact_owner.pgy": {
        "DirectMirScalarCfgRoutinePartitionRangeOwner": ["starts:Array<Int>","counts:Array<Int>"],
        "DirectMirScalarCfgRoutinePartitionRangesReady": ["starts:Array<Int>","counts:Array<Int>"],
    },
    "src/self_hosted/compiler/direct_mir_scalar_program_array_string_boundary_admission_owner.pgy": {
        "DirectMirScalarProgramArrayStringBoundaryFactFromOwners": ["operation_kinds:Array<Int>","operation_results:Array<Int>","operation_expression_rows:Array<Int>","value_local_rows:Array<Int>","block_return_expression_rows:Array<Int>"],
    },
    "src/self_hosted/compiler/direct_mir_scalar_program_callable_type_policy_owner.pgy": {
        "DirectMirScalarProgramCallableTypeMatchesRows": ["parameter_types:Array<String>","parameter_carriages:Array<String>"],
    },
    "src/self_hosted/compiler/direct_mir_scalar_program_leaf_identity_fact_owner.pgy": {
        "DirectMirScalarProgramLeafIdentityFactFromOwners": ["bound_texts:Array<String>","bound_locals:Array<Int>","parameter_names:Array<String>","parameter_syntax_ids:Array<Int>"],
    },
    "src/self_hosted/compiler/direct_mir_scalar_program_runtime_materialization_requirement_owner.pgy": {
        "DirectMirScalarProgramSetStringRuntimeHeaderRequired": ["local_types:Array<String>","parameter_types:Array<String>","return_types:Array<String>"],
    },
    "src/self_hosted/mir_lower/intent_carrier_projection_owner.pgy": {
        "MirIntentCarrierCount": ["names:Array<String>"],
    },
    "src/self_hosted/mir_lower/mir_cfg_graph_owner.pgy": {
        "MirRoutineGraphCanReachAvoiding": ["block_succ_true:Array<Int>","block_succ_false:Array<Int>"],
        "MirRoutineGraphDistancesAvoiding": ["block_succ_true:Array<Int>","block_succ_false:Array<Int>"],
    },
    "src/self_hosted/dir/intent_mode_fact_owner.pgy": {
        "SelfDirIntentModeFactsReady": ["intent_node_ids:Array<Int>"],
    },
    "src/self_hosted/dir/intent_priority_fact_owner.pgy": {
        "SelfDirIntentPriorityFactsReady": ["intent_node_ids:Array<Int>"],
    },
    "src/self_hosted/mir/declaration_verify_owner.pgy": {
        "SelfMirDeclarationContractVocabularyRangeReady": ["rows:Array<String>"],
        "SelfMirDeclarationParallelCount": ["rows:Array<Int>"],
        "SelfMirDeclarationParallelStringCount": ["rows:Array<String>"],
        "SelfMirDeclarationStringRangeReady": ["rows:Array<String>"],
    },
    "src/self_hosted/mir/declaration_zone_authority_rows_owner.pgy": {
        "SelfMirDeclarationZoneAuthorityRowsReady": ["declaration_names:Array<String>","declaration_nominal_kinds:Array<String>"],
    },
    "src/self_hosted/mir/nominal_abi_layout_fact_owner.pgy": {
        "SelfMirNominalUniqueStructIndex": ["names:Array<String>","nominal_kinds:Array<String>"],
    },
    "src/self_hosted/mir/option_nominal_abi_layout_verify_owner.pgy": {
        "SelfMirOptionNominalAbiLayoutRowsReady": ["names:Array<String>","nominal_kinds:Array<String>"],
    },
    "src/self_hosted/semantic/ast_enum_payload_variant_provenance_verdict_owner.pgy": {
        "SemanticAstEnumPayloadBindingForGraphLeaf": ["inferred_types:Array<String>"],
        "SemanticAstEnumPayloadNodeMayMutateBindingThroughCall": ["inferred_types:Array<String>"],
        "SemanticAstEnumPayloadReceiverTypeName": ["inferred_types:Array<String>"],
        "SemanticAstEnumPayloadVariantAtUse": ["inferred_types:Array<String>"],
        "SemanticAstEnumPayloadIfThenEntryState": ["inferred_types:Array<String>"],
        "SemanticAstEnumPayloadAssignmentEffect": ["inferred_types:Array<String>"],
        "SemanticAstEnumPayloadSubtreeMayAssignBinding": ["inferred_types:Array<String>"],
        "SemanticAstEnumPayloadApplyBlockToEnd": ["inferred_types:Array<String>"],
        "SemanticAstEnumPayloadApplyNodeToEnd": ["inferred_types:Array<String>"],
        "SemanticAstEnumPayloadMatchArmEntryState": ["inferred_types:Array<String>"],
        "SemanticAstEnumPayloadStateBeforeUseInBlock": ["inferred_types:Array<String>"],
        "SemanticAstEnumPayloadStateBeforeUseInNode": ["inferred_types:Array<String>"],
    },
    "src/self_hosted/semantic/ast_generic_specialization_fact_owner.pgy": {
        "SemanticAstGenericSpecializationIndexForCall": ["call_node_ids:Array<Int>"],
    },
    "src/self_hosted/semantic/ast_local_binding_identity_owner.pgy": {
        "SemanticAstLocalBindingIdentityRowsReady": ["inferred_types:Array<String>"],
        "SemanticAstScopedLocalBindingIdentityForName": ["inferred_types:Array<String>"],
        "SemanticAstScopedLocalBindingIdentityForGraphLeaf": ["inferred_types:Array<String>"],
    },
    "src/self_hosted/compiler/direct_mir_scalar_cfg_iteration_local_owner.pgy": {
        "DirectMirScalarCfgIterationLocalsReady": ["local_names:Array<String>","local_ref_kinds:Array<Int>","local_ref_value_ids:Array<String>","local_ref_syntax_ids:Array<Int>"],
        "DirectMirScalarCfgIterationLocalRow": ["local_ref_kinds:Array<Int>","local_ref_syntax_ids:Array<Int>"],
    },
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_local_owner.pgy": {
        "DirectMirScalarCfgForEachLocalsReady": ["local_names:Array<String>","local_ref_kinds:Array<Int>","local_ref_syntax_ids:Array<Int>"],
    },
}

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[3]
failures = []
checked = 0
for relative, queries in READ_QUERIES.items():
    source = (root / relative).read_text(encoding="utf-8")
    for name, expected in queries.items():
        declarations = re.findall(
            r"^func\s+" + re.escape(name) + r"\((.*?)\)\s*->",
            source, re.MULTILINE | re.DOTALL,
        )
        if len(declarations) != 1:
            failures.append(f"{relative}: {name}: missing/duplicate declaration")
            continue
        parameters = {
            re.sub(r"\s+", "", parameter)
            for parameter in declarations[0].split(",")
        }
        for parameter in expected:
            if "ref" + parameter not in parameters:
                failures.append(f"{relative}: {name}: needs ref {parameter}")
            checked += 1
if failures:
    print("\n".join(failures), file=sys.stderr)
    raise SystemExit(1)
print(f"[compiler-readonly-query-signatures] {checked} ref parameters: PASS (source inventory only)")
