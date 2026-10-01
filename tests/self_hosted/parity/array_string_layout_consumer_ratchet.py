#!/usr/bin/env python3
"""Static old-read inventory; execution is owned by the companion shell gate."""
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[3]
COMPILER = ROOT / "src/self_hosted/compiler"


def body(file: str, function: str) -> str:
    text = (COMPILER / file).read_text(encoding="utf-8")
    marker = f"func {function}("
    if text.count(marker) != 1:
        raise AssertionError(f"missing or repeated owner: {function}")
    section = text.split(marker, 1)[1]
    return section.split("\nfunc ", 1)[0].split("\nstruct ", 1)[0]


def check(file: str, function: str, required=(), forbidden=()) -> None:
    section = "".join(body(file, function).split())
    for value in required:
        if "".join(value.split()) not in section:
            raise AssertionError(f"{function}: missing {value}")
    for value in forbidden:
        if "".join(value.split()) in section:
            raise AssertionError(f"{function}: retired read {value}")


def main() -> None:
    for file, retired in (
        ("direct_mir_scalar_cfg_collection_plan_fact_owner.pgy",
         "DirectMirScalarCfgCollectionValueRow"),
        ("direct_mir_scalar_program_array_string_abi_owner.pgy",
         "DirectMirScalarProgramArrayStringValueResultParameter"),
    ):
        if f"func {retired}(" in (COMPILER / file).read_text(encoding="utf-8"):
            raise AssertionError(f"retired orphan reappeared: {retired}")
    check("direct_mir_scalar_cfg_array_c_materialization_owner.pgy",
          "DirectMirScalarCfgArrayCStruct",
          ("storage.c_field_order", "storage.c_length_type",
           "DirectMirArrayStorageCAssertionBlock("),
          ("size_t length;", "size_t capacity;", "void *allocator;"))
    check("direct_mir_scalar_cfg_array_c_materialization_owner.pgy",
          "DirectMirScalarCfgArrayCInitializer",
          tuple(f"storage.{field}_field" for field in
                ("data", "length", "capacity", "allocator")),
          ("{NULL, 0, 0, NULL}",))
    check("direct_mir_scalar_cfg_array_int_c_emission_owner.pgy",
          "DirectMirScalarCfgArrayIntCPreamble",
          ("plan.array_int_program.mode == DirectMirScalarCfgArrayIntForEachPrefixPopMode()",
           "DirectMirScalarCfgArrayCStruct("))
    for file, function in (
        ("direct_mir_scalar_cfg_string_array_c_storage_emission_owner.pgy",
         "DirectMirScalarCfgStringArrayCPreamble"),
        ("direct_mir_scalar_cfg_collection_plan_c_storage_owner.pgy",
         "DirectMirScalarCfgCollectionPlanCPreamble"),
        ("direct_mir_scalar_cfg_foreach_typed_c_emission_owner.pgy",
         "DirectMirScalarCfgForEachTypedCPreamble"),
    ):
        check(file, function, ("abi.storage", "DirectMirScalarCfgArrayCStruct("),
              ("typedef struct",))
    for file, function in (
        ("direct_mir_scalar_cfg_string_array_c_storage_emission_owner.pgy",
         "DirectMirScalarCfgStringArrayCDeclarations"),
        ("direct_mir_scalar_cfg_collection_plan_c_storage_owner.pgy",
         "DirectMirScalarCfgCollectionPlanCDeclarations"),
        ("direct_mir_scalar_cfg_foreach_typed_c_emission_owner.pgy",
         "DirectMirScalarCfgForEachTypedCDeclarations"),
    ):
        check(file, function, ("DirectMirScalarCfgArrayCInitializer(", "abi.storage"))
    check("direct_mir_scalar_cfg_array_string_llvm_value_owner.pgy",
          "DirectMirScalarCfgArrayStringLlvmValue",
          tuple(f"abi.{field}" for field in
                ("element_align", "llvm_aggregate_type", "data_index", "length_index",
                 "capacity_index", "allocator_index")), ("align 8",))
    for file, function in (
        ("direct_mir_scalar_cfg_string_array_llvm_storage_emission_owner.pgy",
         "DirectMirScalarCfgStringArrayLlvmDeclarations"),
        ("direct_mir_scalar_cfg_collection_plan_llvm_storage_owner.pgy",
         "DirectMirScalarCfgCollectionPlanLlvmDeclarations"),
    ):
        check(file, function, ("abi.storage.align", "abi.element_align",
                              "abi.data_index", "abi.length_index",
                              "abi.capacity_index", "abi.allocator_index"))
    for file, function in (
        ("direct_mir_scalar_cfg_string_array_llvm_emission_owner.pgy",
         "DirectMirScalarCfgStringArrayLlvmAddress"),
        ("direct_mir_scalar_cfg_string_array_llvm_emission_owner.pgy",
         "DirectMirScalarCfgStringArrayLlvmCondition"),
        ("direct_mir_scalar_cfg_string_array_llvm_mutation_emission_owner.pgy",
         "DirectMirScalarCfgStringArrayLlvmPush"),
        ("direct_mir_scalar_cfg_string_array_llvm_mutation_emission_owner.pgy",
         "DirectMirScalarCfgStringArrayLlvmLengthLog"),
        ("direct_mir_scalar_cfg_string_array_pop_llvm_operation_owner.pgy",
         "DirectMirScalarCfgStringArrayLlvmPop"),
    ):
        check(file, function, ("DirectMirScalarCfgStringArrayLlvmProjectionAt(",
                              "abi.storage.align"), ("align 8",))
    check("direct_mir_scalar_cfg_foreach_typed_llvm_emission_owner.pgy",
          "DirectMirScalarCfgForEachTypedLlvmBlockEntry",
          ("abi.llvm_aggregate_type", "abi.llvm_element_type", "abi.data_index",
           "align = abi.element_align"))
    check("direct_mir_scalar_program_c_array_string_storage_materialization_owner.pgy",
          "DirectMirScalarProgramCStringArrayStorageBlock",
          ("DirectMirScalarCfgArrayCStruct(", "DirectMirScalarCfgArrayCInitializer(",
           "DirectMirScalarProgramArrayStringAbiProjectionReadyForFact("),
          ("typedef struct", "{NULL, 0, 0, NULL}"))
    check("direct_mir_scalar_program_c_slice_expression_owner.pgy",
          "DirectMirScalarProgramCSlicePreamble",
          ("DirectMirScalarProgramCArrayStringCarrierType(",
           "DirectMirScalarCfgArrayCInitializer(", "projection.storage.length_field"),
          ("(pgy_as){NULL, 0, 0, NULL}",))
    for file, function in (
        ("direct_mir_scalar_program_llvm_owned_array_string_parameter_binding_owner.pgy",
         "DirectMirScalarProgramLlvmOwnedArrayStringParameterCopyIn"),
        ("direct_mir_scalar_program_llvm_dir_walk_materialization_owner.pgy",
         "DirectMirScalarProgramLlvmDirWalkBlock"),
    ):
        check(file, function,
              ("DirectMirScalarProgramArrayStringAbiProjectionReadyForFact(",
               "projection.llvm_value_type", "IsSome(projection_opt)"),
              ("%pgy.array.string", "align 8"))
    check("direct_mir_scalar_program_llvm_string_join_materialization_owner.pgy",
          "DirectMirScalarProgramLlvmStringJoinBlock",
          ("DirectMirArrayStringAbiProjectionReadyFor(", "abi.llvm_value_type",
           "abi.llvm_element_type", "ToString(abi.element_align)"),
          ("%pgy.array.string", "align 8"))
    check("direct_mir_scalar_program_llvm_slice_expression_owner.pgy",
          "DirectMirScalarProgramLlvmStringSliceCopyHelper",
          tuple(f"abi.{field}" for field in
                ("element_size", "element_align", "data_index", "length_index",
                 "capacity_index", "allocator_index", "llvm_value_type")),
          ("%pgy.array.string", "align 8", "%bytes = mul i64 %length, 8"))
    check("direct_mir_scalar_program_llvm_slice_expression_owner.pgy",
          "DirectMirScalarProgramLlvmArraySliceHelper",
          ("storage.data_index", "storage.length_index",
           "DirectMirArrayStorageAbiProjectionReadyFor("),
          ("extractvalue ${llvm_array} %array, 0",
           "extractvalue ${llvm_array} %array, 1"))
    check("direct_mir_scalar_program_llvm_slice_expression_owner.pgy",
          "DirectMirScalarProgramLlvmSliceExpressionAt",
          ("storage.data_index", "storage.length_index", "abi.llvm_value_type"),
          ("extractvalue ${llvm_array} ${left}, 0",
           "extractvalue ${llvm_array} ${left}, 1"))
    for file, function in (
        ("direct_mir_scalar_cfg_llvm_local_emission_owner.pgy",
         "DirectMirScalarCfgLlvmLocalDeclarationsInRange"),
        ("direct_mir_scalar_cfg_program_llvm_operation_owner.pgy",
         "DirectMirScalarCfgProgramLlvmOperation"),
        ("direct_mir_scalar_program_llvm_expression_owner.pgy",
         "DirectMirScalarProgramLlvmExpressionAt"),
    ):
        check(file, function,
              ("DirectMirScalarProgramLlvmArrayStringStorageProjection(",
               "projection.storage.align"),
              ("alloca %pgy.array.string, align 8",))
    check("direct_mir_scalar_program_array_string_abi_projection_owner.pgy",
          "DirectMirScalarProgramLlvmArrayStringStorageProjection",
          ("IsSome(projection_opt)", "DirectMirScalarProgramArrayStringAbiProjectionReadyForFact(",
           "CompilerTargetCpuLlvmProjection()"))
    check("direct_mir_scalar_cfg_borrowed_collection_admission_owner.pgy",
          "DirectMirScalarCfgBorrowedCollectionFactsReady",
          ("index.valid", "MirCollectionOwnershipLocalRow(",
           'index.collection_ownership_facts.origins[fact_row] != "borrowed-literal"',
           'index.collection_ownership_facts.dispositions[fact_row] != "live"',
           "index.collection_ownership_facts.origin_syntax_ids[fact_row] != binding_id",
           "collection_count == ArrayLength(index.collection_ownership_facts.binding_syntax_ids)"))
    for file, function in (
        ("direct_mir_scalar_cfg_program_routine_admission_owner.pgy",
         "DirectMirScalarCfgProgramAppendRoutine"),
        ("direct_mir_scalar_cfg_graph_admission_owner.pgy",
         "DirectMirScalarCfgGraphPlanFromAdmitted"),
    ):
        check(file, function, ("DirectMirScalarCfgVoidReturnReady(",),
              ("let public_void: Bool",))
    check("direct_mir_scalar_graph_admission_owner.pgy",
          "DirectMirRoutineHasNoUnsupportedFactsExceptLoop",
          ('"collection_ownership_facts"', '"collection_ownership_fact_count", "0"'))
    check("direct_mir_option_match_cfg_plan_owner.pgy",
          "DirectMirOptionMatchCfgRouteClaimed",
          ("BuildMirRoutineInstructionMatchFacts(", "matches.pattern_counts[row]"))
    check("direct_mir_backend_projection_owner.pgy",
          "CompileAdmittedDirectMirForTargetObserved",
          ("DirectMirOptionMatchCfgRouteClaimed(admitted)",),
          ("else if block_count == 7",))
    signature = body("direct_mir_scalar_program_collection_builtin_signature_owner.pgy",
                     "DirectMirScalarProgramCollectionBuiltinSignatureExpected")
    to_int = signature.split('if name == "ToInt" {', 1)[1].split("}", 1)[0]
    if '["Int", "String"' not in to_int:
        raise AssertionError("ToInt lost its canonical String parameter")
    print("[array-string-layout-ratchet] retired layout reads and two orphan functions: PASS")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError) as error:
        print(f"[array-string-layout-ratchet] {error}", file=sys.stderr)
        raise SystemExit(1)
