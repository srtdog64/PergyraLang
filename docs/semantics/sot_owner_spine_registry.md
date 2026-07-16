# SoT Owner Spine Registry

Status: `architecture-owner-registry`  
Date: 2026-07-12

This registry fixes the first whole-compiler owner outline. It is the
machine-gated companion to `docs/125_source_of_truth_spine.md` and
`docs/180_compiler_logical_spine_handles_gates.md`.

An owner row declares authority identity. It does not by itself prove that all
implementation consumers have migrated. `CLOSED` is permitted only when the
named consumer inventory is fail-closed, the old semantic read is absent, and
the enforcement gate names every forbidden fallback. `BRIDGE` means an old
semantic carrier remains reachable. `ACTIVE` means the owner identity is fixed
but consumer or stable-handle coverage is incomplete.

## Fields

```text
owner_id | fact_class | stable_handle | coq_fact | coq_owner |
authority_path | producer_term | last_consumers | forbidden_fallbacks |
enforcement_gate | status | open_reason
```

- `owner_id` is the stable registry identity.
- `fact_class` is `syntax`, `semantic`, `resource`, `execution`,
  `verification`, `abi`, `target`, `projection`, `diagnostic`, `artifact`, or
  `compatibility`.
- `stable_handle` names the target identity even when its implementation is
  still incomplete.
- `coq_fact` and `coq_owner` bind the row to `SoTAuthority.v`.
- `authority_path#producer_term` identifies the current or target authority
  implementation.
- `last_consumers` is the complete architectural last-consumer class for this
  outline, expressed as live paths.
- `forbidden_fallbacks` names semantic recovery that must disappear before
  promotion to `CLOSED`.
- List fields use CSV quoting. A token that contains a comma must be quoted;
  an unquoted comma is a relationship separator, not part of a token.
- `enforcement_gate` uses `path#required-text`.
- `open_reason` is `none` only for `CLOSED` rows.

<!-- BEGIN sot-owner-spine-registry -->
```text
source.module_graph | syntax | SourceArtifactId | SFSourceModuleGraph | SOModuleLoader | src/compiler/module_loader.c | module_loader_load_program | src/compiler/driver_app.c | unowned_source_reload,module_path_string_identity | tests/module_smoke.sh#parallel_ref_slot_conflict | ACTIVE | stable_source_and_module_handles_not_landed
lexer.token_stream | syntax | TokenStreamId | SFTokenStream | SOLexer | src/lexer/lexer.c | lexer_next_token | src/parser/parser.c | parser_relex,source_bytes_as_token_identity | tests/parser_lexer_diagnostic_smoke.sh#parser/lexer diagnostic smoke violations | ACTIVE | token_stream_handle_not_landed
parser.syntax_provenance | syntax | SyntaxNodeId | SFSyntaxProvenanceTree | SOParserAst | src/parser/ast.c | ast_program_append_statement | src/semantic/semantic.c,src/compiler/hir.c,src/compiler/dir.c,src/compiler/rir_builder.c | raw_AST_pointer_identity,name_or_span_identity | tests/stable_identity_contract_smoke.sh#overflow fail-close | BRIDGE | annotated_AST_fans_out_to_semantic_and_three_IR_lowerers
semantic.symbol_type_graph | semantic | SymbolId | SFSemanticSymbolTypeGraph | SOSemanticAnalyzer | src/semantic/semantic.c | semantic_analyze_ex | src/semantic/slot_analyzer_lookup.c,src/compiler/hir.c,src/compiler/mir.c,src/compiler/air_evidence_dag.c | AST_annotation_as_final_type,slot_AST_program_root_rescan_with_host_index,backend_type_recovery | tests/slot_analyzer_host_index_smoke.sh#hash owner and no-fallback gates passed | BRIDGE | MIR_and_AIR_still_receive_semantic_context_and_type_spellings
semantic.resource_flow_universe | resource | SymbolId | SFResourceFlowUniverse | SOResourceFlowUniverse | src/semantic/type_checker_flow_universe.c | resource_flow_universe_bind | src/semantic/type_checker_flow_resources.c,src/semantic/type_checker_flow_loop_summary.c | Symbol_pointer_as_function_flow_identity,missing_universe_pointer_fallback | tests/loop_flow_summary_smoke.sh#permanent 1..4 loop re-entry and 5s gates passed | ACTIVE | stable_indices_are_function_local_and_not_yet_carried_into_HIR
semantic.loop_flow_summary | semantic | SyntaxNodeId | SFLoopFlowSummary | SOLoopFlowSummary | src/semantic/type_checker_flow_loop_summary.c | loop_flow_summary_record | src/semantic/type_checker_flow_loops.c | repeated_loop_body_reinspection,missing_summary_timeout_success | tests/loop_flow_summary_smoke.sh#permanent 1..4 loop re-entry and 5s gates passed | ACTIVE | summary_is_native_semantic_owner_and_not_yet_selfhosted
semantic.function_param_flow_summary | resource | FunctionIdParamIndex | SFFunctionParamFlowSummary | SOFunctionParamFlowSummary | src/semantic/function_param_flow_summary.c | function_param_flow_summary_demand | src/semantic/slot_analyzer.c,src/semantic/slot_analyzer_access.c,src/semantic/slot_analyzer_escape.c | recursive_callee_body_reopen,depth_limited_summary_truncation | tests/function_param_flow_summary_smoke.sh#demanded recursive fixed point and no-reopen gates passed | ACTIVE | summary_is_native_semantic_owner_and_not_yet_CFG_or_HIR_carried
hir.typed_control_flow | semantic | RoutineId | SFHirTypedControlFlow | SOHir | src/compiler/hir.c | hir_lower | src/compiler/rir_flow.c,src/compiler/mir.c,src/compiler/air.c | AST_body_rescan,name_join_call_identity | tests/hir_routine_identity_smoke.sh#without a name fallback | BRIDGE | DIR_and_initial_RIR_lowering_still_bypass_HIR
dir.domain_graph | semantic | DomainNodeId | SFDirDomainGraph | SODir | src/compiler/dir.c | dir_lower | src/compiler/rir_validation_dir.c,src/compiler/air.c | AST_domain_rescan,name_only_domain_join | tests/ir_minimality_adequacy_smoke.sh#const DIRProgram *dir | ACTIVE | domain_ids_and_cross_layer_handles_are_incomplete
rir.resource_transition_graph | resource | ResourceTransitionId | SFRirResourceTransitionGraph | SORir | src/compiler/rir_builder.c | rir_lower | src/compiler/mir.c,src/compiler/air.c | AST_resource_rescan,runtime_manager_as_static_proof | tests/ir_minimality_adequacy_smoke.sh#rir_enrich_scope_with_hir_flow | ACTIVE | initial_RIR_shape_still_lowers_from_AST_before_HIR_enrichment
mir.execution_graph | execution | ValueId | SFMirExecutionGraph | SOMir | src/compiler/mir.c | mir_lower | src/codegen/transpiler_entry.c,src/codegen/llvm_api.c,src/compiler/air_evidence_mir.c | backend_AST_semantic_read,silent_i32_fallback,residual_source_payload_dispatch | tests/self_host_hard_contract_smoke.sh#MIR instruction expression graph is missing or invalid | BRIDGE | selfhost_DRV2_branch_definition_return_log_index_indexed_assignment_target_logical_not_numeric_negate_direct_identifier_call_simple_and_nested_member_field_instance_method_namespace_qualified_call_pipe_and_postfix_try_graph_carriage_closed;object_init_special_unary_literal_argument_payload_type_classification_and_backend_compatibility_inventory_remain
air.evidence_graph | verification | EvidenceId | SFAirEvidenceGraph | SOAir | src/compiler/air.c | air_synthesize | src/compiler/air_verify.c,src/compiler/driver_app.c | summary_counter_as_proof,backend_reads_AIR,missing_evidence_success | tests/air_drift_smoke.sh#air_synthesize(hir, dir, rir | ACTIVE | live_AIR_MIR_owner_fact_binding_is_not_complete
abi.layout_rows | abi | LayoutId | SFAbiLayoutRows | SOMirAbi | src/compiler/mir_abi_layout.c | mir_abi_lookup | src/codegen/transpiler_entry.c,src/codegen/llvm_api.c | backend_local_layout_guess,implicit_option_niche,target_default_guess | tests/abi_ownership_shape_smoke.sh#MIR_ABI_REPR_EXPLICIT_TAG | BRIDGE | aggregate_generic_and_target_specific_layout_consumers_are_partial
target.capability_profile | target | TargetProfileId | SFTargetCapabilityProfile | SOTargetCapability | src/self_hosted/compiler/target_capability_owner.pgy | CompilerTargetCapabilityEnvelopeReady | src/compiler/compiler_toolchain.c,src/codegen/transpiler_entry.c,src/codegen/llvm_api.c | backend_target_default,compiler_command_guess | tests/self_hosted_component_contract_smoke.sh#target-capability-envelope-paths | ACTIVE | native_C_and_LLVM_consumers_do_not_yet_read_the_selfhost_envelope
projection.verified_plan | projection | ProjectionPlanId | SFProjectionPlan | SOProjectionPlanner | src/compiler/verified_projection_plan.c | pgy_verified_projection_plan_intent_observability | src/codegen/transpiler_entry.c,src/codegen/llvm_api.c | backend_materialization_guess,AST_HIR_usage_inference | tests/verified_projection_plan_smoke.sh#canonical ABI and fail-closed projection owner are closed | ACTIVE | only_intent_observability_has_a_verified_plan_row
diagnostic.catalog | diagnostic | DiagnosticId | SFDiagnosticCatalog | SODiagnosticCatalog | src/semantic/diag_codes.h | PGY_CODE_SEM_TYPE_MISMATCH | src/semantic/type_checker_diag.c,src/compiler/driver_diag.c | free_text_code_recovery,stage_guess_from_message | tests/diagnostic_registry_smoke.sh#macros and call sites ok | BRIDGE | driver_still_maps_some_free_text_messages_to_codes
artifact.zone | artifact | ArtifactId | SFBackendArtifact | SOArtifactZone | src/self_hosted/compiler/artifact_zone_owner.pgy | CompilerArtifactZoneReady | src/compiler/compiler.c,src/compiler/compiler_llvm.c,tests/self_hosted/parity/backend_output_comparator_parity.sh | path_as_artifact_identity,backend_output_without_plan_digest | tests/self_hosted_component_contract_smoke.sh#CompilerArtifactKindKnown | ACTIVE | native_artifacts_do_not_yet_carry_plan_revision_and_digest_identity
compatibility.evolution | compatibility | CompatibilitySurfaceId | SFCompatibilityEvolution | SOCompatibilityEvolution | src/self_hosted/compiler/compatibility_evolution_owner.pgy | CompilerCompatibilityEvolutionReady | src/self_hosted/tools/compatibility_evolution_checker/report_owner.pgy,src/compiler/driver_diag.c | local_compatibility_list,warning_without_migration_metadata | tests/self_hosted/parity/compatibility_evolution_manifest_parity.sh#parity ok | ACTIVE | native_diagnostic_ABI_trace_and_package_gates_do_not_all_consume_the_rows
selfhost.expression_graph | syntax | SyntaxNodeId | SFExpressionGraph | SOParserExpressionGraph | src/self_hosted/parser/expression_graph_owner.pgy | ParserExpressionGraphContractReady | src/self_hosted/codegen/input/semantic_expression_codegen_view_owner.pgy,src/self_hosted/codegen/emission/try_let_emit_owner.pgy,src/self_hosted/codegen/emission/stmt_emit.pgy | initializer_array_bodies,SemanticAstLocalBindingArrayLiteralBodyAt(,semantic_array_literal_codegen_view_owner.pgy,initializer_try_operands,SemanticAstLocalBindingTryOperandAt(,SemanticTryOperand( | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.collection_mutation_statement | semantic | SyntaxNodeId | SFCollectionMutationStatement | SOSemanticStatement | src/self_hosted/semantic/ast_statement_fact_owner.pgy | SemanticAstStatementFacts | src/self_hosted/codegen/input/semantic_statement_codegen_view_owner.pgy | TypedAstArenaAtomText,TypedAstArenaValueText,TypedAstArenaAuxValueText,ast_text_collection_stmt_owner.pgy,StringTrim( | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.enum_declaration_rows | semantic | SyntaxNodeId | SFEnumDeclarationRows | SOSemanticEnum | src/self_hosted/semantic/ast_enum_fact_owner.pgy | SemanticAstEnumFacts | src/self_hosted/codegen/input/semantic_enum_codegen_view_owner.pgy,src/self_hosted/codegen/emission/function_emit.pgy,src/self_hosted/codegen/emission/program_emit.pgy | TypedAstArenaAuxValueText,ExprSequenceItemCount,ExprSequenceItemAt,ast_text_enum_variant_owner.pgy,CodegenAstArenaEnumNameOrDie,CodegenAstArenaEnumVariantCount,CodegenAstArenaEnumVariantNameAt,CodegenAstArenaIsEnumDecl | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.nominal_declaration_rows | semantic | SyntaxNodeId | SFNominalDeclarationRows | SOSemanticNominalConstructor | src/self_hosted/semantic/ast_nominal_constructor_fact_owner.pgy | SemanticAstNominalConstructorFacts | src/self_hosted/codegen/input/semantic_nominal_codegen_view_owner.pgy,src/self_hosted/codegen/emission/function_emit.pgy,src/self_hosted/codegen/emission/program_emit.pgy | ast_text_declaration_owner.pgy,CodegenAstArenaNominalNameOrDie,CodegenAstArenaFieldNameOrDie,CodegenAstArenaFieldTypeNameOrDie,CodegenAstArenaIsNominalDecl,"CodegenAstArenaIsFieldsHeader(arena, j)" | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.role_declaration_rows | semantic | SyntaxNodeId | SFRoleDeclarationRows | SOSemanticRole | src/self_hosted/semantic/ast_role_fact_owner.pgy | SemanticAstRoleFacts | src/self_hosted/codegen/input/semantic_role_codegen_view_owner.pgy,src/self_hosted/codegen/emission/function_emit.pgy,src/self_hosted/codegen/emission/program_emit.pgy | ast_text_role_declaration_owner.pgy,CodegenAstArenaRoleNameOrDie,CodegenAstArenaRoleTargetTypeNameOrDie,CodegenAstArenaIsRoleDecl,"CodegenAstArenaIsDescendantOf(arena, j, i)" | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.expression_surface | semantic | SyntaxNodeId | SFExpressionSurface | SOSemanticExpressionSurface | src/self_hosted/semantic/ast_expression_surface_fact_owner.pgy | SemanticAstExpressionSurfaceFacts | src/self_hosted/semantic/ast_expression_graph_struct_type_verdict_owner.pgy,src/self_hosted/semantic/ast_expression_graph_generic_call_owner.pgy,src/self_hosted/semantic/ast_generic_specialization_fact_owner.pgy,src/self_hosted/codegen/input/ast_expression_usage_owner.pgy,src/self_hosted/codegen/input/ast_usage_owner.pgy,src/self_hosted/codegen/input/semantic_expression_codegen_view_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_shape_emit_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_graph_emit_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy,src/self_hosted/codegen/emission/option_value_emit_owner.pgy,src/self_hosted/codegen/emission/collection_element_emit_owner.pgy | TypedAstArenaAtomText,TypedAstArenaValueText,TypedAstArenaAuxValueText,ContainsCallOutsideStrings,CodegenExpressionUsageFactsFromArena,CodegenAstArenaExpressionPartsAt,FindTopLevelPlus,RewriteBool,RewriteIndexing,RewriteInoutCallArgs,ExprSequenceItemAt,generic_return_text_inference,codegen_generic_graph_rescan | tests/self_hosted/parity/driver_rung2_body_parity.sh#producer-first source/MIR parity ok | BRIDGE | logical_equality_relational_arithmetic_index_logical_not_numeric_negate_direct_identifier_call_simple_and_nested_member_field_method_namespace_qualified_call_log_argument_pipe_call_postfix_try_for_range_identifier_and_nonidentifier_foreach_payload_free_enum_argument_array_literal_call_argument_named_struct_literal_call_argument_general_named_struct_literal_value_struct_literal_nominal_and_field_types_option_struct_some_constructor_payload_contextual_option_struct_none_codegen_ast_text_node_array_push_array_set_array_literal_and_indexed_assignment_value_and_target_graphs_are_parser_owned_and_assignment_target/index_scalar_type_facts_are_semantic_owned;graph_owned_concrete_scalar_tree_results_operand_diagnostics_resolved_direct_namespace_receiver_generic_local_and_chained_field_receiver_call_target_identity_arity_argument_verdicts_exact_and_nested_formal_parameter_binding_nested_generic_direct_return_substitution_ordered_explicit_generic_actual_carriage_and_conflict_rejection_bounded_MIR_codegen_target_carriage_scalar_option_result_builtin_policy_with_target_consistency_collection_mutation_admission_across_specialized_statement_and_graph_call_consumers_and_aggregate_field_types_and_assignability_for_scalar_direct_or_wrapper_nominal_values_plus_explicit_top_level_and_local_initializer_inferred_generic_actual_aggregate_values_are_C_LLVM_negative_gated;collection_result_or_element_typing_unknown_or_aggregate_wrapper_payloads_member_aggregate_field_values_return_or_assignment_rooted_inferred_generic_calls_member_generic_calls_nested_generic_locals_object_init_special_unary_and_initial_compact_tree_arena_construction_remain_bridged
selfhost.initializer_type_verdict | semantic | SyntaxNodeId | SFInitializerTypeVerdict | SOSemanticInitializerType | src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy | SemanticAstInitializerTypeFacts | src/self_hosted/semantic/ast_body_type_bundle_owner.pgy,src/self_hosted/mir/artifact_lower_owner.pgy,src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy,src/self_hosted/semantic/ast_iteration_type_fact_owner.pgy,src/self_hosted/semantic/ast_statement_type_fact_owner.pgy | source_initializer_type_rescan,backend_initializer_type_guess | tests/self_hosted/parity/initializer_projection_probe_parity.sh#missing-initializer-row | CLOSED | none
selfhost.iteration_type_verdict | semantic | SyntaxNodeId | SFIterationTypeVerdict | SOSemanticIterationType | src/self_hosted/semantic/ast_iteration_type_fact_owner.pgy | SemanticAstIterationTypeFacts | src/self_hosted/semantic/ast_body_type_bundle_owner.pgy,src/self_hosted/mir/artifact_lower_owner.pgy,src/self_hosted/mir/routine_input_owner.pgy,src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy,src/self_hosted/semantic/ast_statement_type_fact_owner.pgy | source_iteration_type_rescan,backend_iteration_type_guess | tests/self_hosted_component_contract_smoke.sh#struct SemanticAstIterationTypeFacts | ACTIVE | executable_C_LLVM_last_consumer_missing_fact_matrix_not_closed
selfhost.assignment_type_verdict | semantic | SyntaxNodeId | SFAssignmentTypeVerdict | SOSemanticAssignmentType | src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy | SemanticAstAssignmentTypeFacts | src/self_hosted/semantic/ast_body_type_bundle_owner.pgy,src/self_hosted/codegen/input/semantic_assignment_codegen_view_owner.pgy,src/self_hosted/codegen/input/semantic_body_type_codegen_view_owner.pgy | source_assignment_type_rescan,backend_assignment_type_guess,missing_expected_type_success | tests/self_hosted/parity/assignment_projection_probe_parity.sh#missing-expected-type | CLOSED | none
selfhost.call_target_identity | semantic | SyntaxNodeId | SFCallTargetIdentity | SOSemanticCallTarget | src/self_hosted/semantic/ast_expression_call_target_fact_owner.pgy | SemanticExpressionCallTargetFact | src/self_hosted/semantic/ast_expression_call_target_capture_owner.pgy,src/self_hosted/semantic/ast_expression_graph_resolved_call_type_owner.pgy,src/self_hosted/semantic/ast_expression_graph_concrete_scalar_verdict_owner.pgy,src/self_hosted/mir/expression_graph_fact_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy,src/self_hosted/compiler/driver_rung2_owner.pgy | callee_text_as_final_identity,namespace_name_join_fallback,codegen_receiver_type_method_join | tests/self_hosted/parity/initializer_projection_probe_parity.sh#missing-carried-direct-target | CLOSED | none
selfhost.type_runtime_usage_surface | semantic | SyntaxNodeId | SFTypeRuntimeUsageSurface | SOSemanticTypeSurface | src/self_hosted/semantic/ast_type_surface_fact_owner.pgy | SemanticAstTypeSurfaceFacts | src/self_hosted/codegen/input/ast_type_usage_owner.pgy,src/self_hosted/codegen/input/ast_usage_owner.pgy | TypedAstArenaTypeName,CodegenAstArenaTypeFactPresent,CodegenTypeUsageFactsFromArena | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.node_kind_surface | semantic | SyntaxNodeId | SFNodeKindSurface | SOSemanticKindSurface | src/self_hosted/semantic/ast_kind_surface_fact_owner.pgy | SemanticAstKindSurfaceFacts | src/self_hosted/codegen/input/ast_kind_usage_owner.pgy,src/self_hosted/codegen/input/ast_usage_owner.pgy,src/self_hosted/codegen/input/semantic_kind_codegen_view_owner.pgy,src/self_hosted/codegen/emission/program_emit.pgy | TypedAstArenaNodeKindIs,CodegenAstArenaKindPresent,CodegenKindUsageFactsFromArena,CodegenAstKindArrayLiteral,CodegenAstArenaIsAbilityDecl,CodegenAstArenaIsEventDecl | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.entrypoint_selection | semantic | SyntaxNodeId | SFEntrypointSelection | SOSemanticSignature | src/self_hosted/semantic/ast_signature_fact_owner.pgy | SemanticAstFunctionSignatureFacts | src/self_hosted/semantic/ast_artifact_verdict_owner.pgy,src/self_hosted/codegen/input/semantic_signature_codegen_view_owner.pgy,src/self_hosted/codegen/emission/program_emit.pgy | SemanticAstArtifactIsMainFunction,CodegenAstArenaIsMainFunction | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.function_declaration_rows | semantic | SyntaxNodeId | SFFunctionDeclarationRows | SOSemanticSignature | src/self_hosted/semantic/ast_signature_fact_owner.pgy | SemanticAstFunctionSignatureFacts | src/self_hosted/codegen/input/semantic_signature_codegen_view_owner.pgy,src/self_hosted/codegen/emission/program_emit.pgy | CodegenAstArenaIsFunction | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.local_binding_statement_routing | semantic | SyntaxNodeId | SFLocalBindingStatementRouting | SOSemanticLocalBinding | src/self_hosted/semantic/ast_local_binding_fact_owner.pgy | SemanticAstLocalBindingFacts | src/self_hosted/codegen/input/semantic_local_binding_codegen_view_owner.pgy,src/self_hosted/codegen/emission/stmt_emit.pgy,src/self_hosted/codegen/emission/program_emit.pgy | CodegenAstArenaIsLetStmt | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.assignment_statement_routing | semantic | SyntaxNodeId | SFAssignmentStatementRouting | SOSemanticAssignment | src/self_hosted/semantic/ast_assignment_fact_owner.pgy | SemanticAstAssignmentFacts | src/self_hosted/codegen/input/semantic_assignment_codegen_view_owner.pgy,src/self_hosted/codegen/emission/stmt_emit.pgy,src/self_hosted/codegen/emission/program_emit.pgy | CodegenAstArenaIsAssignStmt | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.statement_kind_routing | semantic | SyntaxNodeId | SFStatementKindRouting | SOSemanticStatement | src/self_hosted/semantic/ast_statement_fact_owner.pgy | SemanticAstStatementFacts | src/self_hosted/codegen/input/semantic_statement_codegen_view_owner.pgy,src/self_hosted/codegen/emission/stmt_emit.pgy,src/self_hosted/codegen/emission/program_emit.pgy | CodegenAstArenaIsLogStmt,CodegenAstArenaIsBareReturnStmt,CodegenAstArenaIsValueReturnStmt,CodegenAstArenaIsDeferStmt,CodegenAstArenaIsArrayPopStmt,CodegenAstArenaIsArraySetStmt,CodegenAstArenaIsArrayPushStmt,CodegenAstArenaIsExitStmt,CodegenAstArenaIsBreakStmt,CodegenAstArenaIsContinueStmt,CodegenAstArenaIsForStmt,CodegenAstArenaIsWhileStmt,CodegenAstArenaIsIfStmt,CodegenAstArenaIsElseIfAt,CodegenAstArenaIsMatchStmt,CodegenAstArenaIsMatchCaseStmt,CodegenAstArenaIsMatchDefaultStmt,CodegenAstArenaIsBareCallStmt | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.statement_result_type | semantic | SyntaxNodeId | SFStatementResultType | SOSemanticStatementType | src/self_hosted/semantic/ast_statement_type_fact_owner.pgy | SemanticAstStatementTypeFacts | src/self_hosted/semantic/ast_body_type_bundle_owner.pgy,src/self_hosted/semantic/ast_statement_type_query_owner.pgy,src/self_hosted/codegen/input/semantic_statement_codegen_view_owner.pgy,src/self_hosted/codegen/emission/stmt_emit.pgy,src/self_hosted/codegen/emission/log_emit_owner.pgy | ExprKind(,ExprKind(match_subject,CodegenSemanticLogArgumentOrDie | tests/self_hosted_component_contract_smoke.sh#SemanticAstStatementTypeQueryContractReady() | CLOSED | none
```
<!-- END sot-owner-spine-registry -->

## Derived Fact Carriers

Every self-hosted `*_fact_owner.pgy` file is either an authority path in the
table above or a classified derivative below. A derivative may project, cache,
bridge, or expose a local view of an authority, but it may not make an
independent semantic decision. This table prevents a new fact-shaped file from
silently becoming a second authority.

```text
path | primary_term | owner_id | relation
```

<!-- BEGIN sot-derived-fact-registry -->
```text
src/self_hosted/parser/expression_fact_owner.pgy | ParserExpressionFact | selfhost.expression_graph | local_view
src/self_hosted/mir/program_fact_owner.pgy | SelfMirProgramFacts | mir.execution_graph | projection
src/self_hosted/mir/parallel_capture_fact_owner.pgy | SelfMirParallelCaptureRows | mir.execution_graph | projection
src/self_hosted/mir/expression_graph_fact_owner.pgy | SelfMirExpressionGraphRows | mir.execution_graph | projection
src/self_hosted/mir/expression_fact_owner.pgy | SelfMirExpressionKind | mir.execution_graph | bridge
src/self_hosted/mir_lower/parallel_capture_fact_owner.pgy | MirParallelCaptureFactsReady | mir.execution_graph | projection
src/self_hosted/mir_lower/expression_graph_fact_owner.pgy | MirExpressionGraphSequence | mir.execution_graph | projection
src/self_hosted/hir/ast_text_row_fact_owner.pgy | CodegenAstTextRowFactInput | hir.typed_control_flow | bridge
src/self_hosted/semantic/ast_generic_parameter_fact_owner.pgy | SemanticAstGenericParameterRowsFromNode | selfhost.function_declaration_rows | local_view
src/self_hosted/semantic/ast_signature_type_expression_fact_owner.pgy | SemanticAstSignatureTypeExpressionFacts | selfhost.function_declaration_rows | local_view
src/self_hosted/semantic/try_expression_fact_owner.pgy | SemanticTryOperand | selfhost.expression_graph | bridge
src/self_hosted/semantic/ast_expression_graph_fact_owner.pgy | SemanticExpressionGraphFacts | selfhost.expression_graph | bridge
src/self_hosted/semantic/delimited_range_fact_owner.pgy | SemanticDelimitedRangeFacts | semantic.symbol_type_graph | local_view
src/self_hosted/semantic/expression_operator_fact_owner.pgy | SemanticTopLevelOperatorFacts | selfhost.expression_surface | local_view
src/self_hosted/semantic/ast_generic_specialization_fact_owner.pgy | SemanticAstGenericSpecializationFactsFromBody | selfhost.expression_surface | projection
```
<!-- END sot-derived-fact-registry -->

The expression rows above are deliberately bounded. The initializer row
closes array-literal initializer body ownership; the expression-surface row
also carries array-literal call arguments as ordered element graphs, named
struct-literal values as ordered field-binding graphs, and `Option<struct>`
constructors as ordered call spines whose payload remains that struct graph.
Contextual `Option<struct>` `None` initialization, reassignment, and return
select the MIR-owned ABI constructor through expected-type facts; the native C
and LLVM assignment consumers may not recover that type from AST text or fall
back to `Option<Int>`.
Other expression shapes remain under `selfhost.expression_surface`. The try row closes postfix-try
structure and its operand edge only. Payload type classification remains in the
expression-surface `BRIDGE`, and the compact legacy/native canonicalization
bridge must reproduce the same graph but is not hard-codegen authority.

## Current Judgment

The owner outline is complete for the listed compiler spine, but implementation
closure is not. There are sixteen `CLOSED` rows, seven `BRIDGE` rows, and thirteen `ACTIVE`
rows. The exact counts are gate-owned and must change only when a row gains or
loses the evidence required by its status.

Current status counts: `CLOSED=16 BRIDGE=7 ACTIVE=13`.

The registry does not replace the detailed pass contract or migration ledger.
It answers a narrower question: who is allowed to decide each top-level fact
family, and which last consumers must eventually lose every alternate read.
