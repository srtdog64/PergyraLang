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
- `enforcement_gate` uses `path#required-text`.
- `open_reason` is `none` only for `CLOSED` rows.

<!-- BEGIN sot-owner-spine-registry -->
```text
source.module_graph | syntax | SourceArtifactId | SFSourceModuleGraph | SOModuleLoader | src/compiler/module_loader.c | module_loader_load_program | src/compiler/driver_app.c | unowned_source_reload,module_path_string_identity | tests/module_smoke.sh#parallel_ref_slot_conflict | ACTIVE | stable_source_and_module_handles_not_landed
lexer.token_stream | syntax | TokenStreamId | SFTokenStream | SOLexer | src/lexer/lexer.c | lexer_next_token | src/parser/parser.c | parser_relex,source_bytes_as_token_identity | tests/parser_lexer_diagnostic_smoke.sh#parser/lexer diagnostic smoke violations | ACTIVE | token_stream_handle_not_landed
parser.syntax_provenance | syntax | SyntaxNodeId | SFSyntaxProvenanceTree | SOParserAst | src/parser/ast.c | ast_program_append_statement | src/semantic/semantic.c,src/compiler/hir.c,src/compiler/dir.c,src/compiler/rir_builder.c | raw_AST_pointer_identity,name_or_span_identity | tests/stable_identity_contract_smoke.sh#overflow fail-close | BRIDGE | annotated_AST_fans_out_to_semantic_and_three_IR_lowerers
semantic.symbol_type_graph | semantic | SymbolId | SFSemanticSymbolTypeGraph | SOSemanticAnalyzer | src/semantic/semantic.c | semantic_analyze_ex | src/compiler/hir.c,src/compiler/mir.c,src/compiler/air_evidence_dag.c | AST_annotation_as_final_type,backend_type_recovery | tests/type_resolution_resolver_inventory_smoke.sh#fallback seams= | BRIDGE | MIR_and_AIR_still_receive_semantic_context_and_type_spellings
hir.typed_control_flow | semantic | RoutineId | SFHirTypedControlFlow | SOHir | src/compiler/hir.c | hir_lower | src/compiler/rir_flow.c,src/compiler/mir.c,src/compiler/air.c | AST_body_rescan,name_join_call_identity | tests/hir_routine_identity_smoke.sh#without a name fallback | BRIDGE | DIR_and_initial_RIR_lowering_still_bypass_HIR
dir.domain_graph | semantic | DomainNodeId | SFDirDomainGraph | SODir | src/compiler/dir.c | dir_lower | src/compiler/rir_validation_dir.c,src/compiler/air.c | AST_domain_rescan,name_only_domain_join | tests/ir_minimality_adequacy_smoke.sh#const DIRProgram *dir | ACTIVE | domain_ids_and_cross_layer_handles_are_incomplete
rir.resource_transition_graph | resource | ResourceTransitionId | SFRirResourceTransitionGraph | SORir | src/compiler/rir_builder.c | rir_lower | src/compiler/mir.c,src/compiler/air.c | AST_resource_rescan,runtime_manager_as_static_proof | tests/ir_minimality_adequacy_smoke.sh#rir_enrich_scope_with_hir_flow | ACTIVE | initial_RIR_shape_still_lowers_from_AST_before_HIR_enrichment
mir.execution_graph | execution | ValueId | SFMirExecutionGraph | SOMir | src/compiler/mir.c | mir_lower | src/codegen/transpiler_entry.c,src/codegen/llvm_api.c,src/compiler/air_evidence_mir.c | backend_AST_semantic_read,silent_i32_fallback,residual_source_payload_dispatch | tests/cfg_body_dataflow_smoke.sh#MIR validator rejects residual STMT | BRIDGE | residual_source_payload_and_backend_compatibility_inventory_remain
air.evidence_graph | verification | EvidenceId | SFAirEvidenceGraph | SOAir | src/compiler/air.c | air_synthesize | src/compiler/air_verify.c,src/compiler/driver_app.c | summary_counter_as_proof,backend_reads_AIR,missing_evidence_success | tests/air_drift_smoke.sh#air_synthesize(hir, dir, rir | ACTIVE | live_AIR_MIR_owner_fact_binding_is_not_complete
abi.layout_rows | abi | LayoutId | SFAbiLayoutRows | SOMirAbi | src/compiler/mir_abi_layout.c | mir_abi_lookup | src/codegen/transpiler_entry.c,src/codegen/llvm_api.c | backend_local_layout_guess,implicit_option_niche,target_default_guess | tests/abi_ownership_shape_smoke.sh#MIR_ABI_REPR_EXPLICIT_TAG | BRIDGE | aggregate_generic_and_target_specific_layout_consumers_are_partial
target.capability_profile | target | TargetProfileId | SFTargetCapabilityProfile | SOTargetCapability | src/self_hosted/compiler/target_capability_owner.pgy | CompilerTargetCapabilityEnvelopeReady | src/compiler/compiler_toolchain.c,src/codegen/transpiler_entry.c,src/codegen/llvm_api.c | backend_target_default,compiler_command_guess | tests/self_hosted_component_contract_smoke.sh#target-capability-envelope-paths | ACTIVE | native_C_and_LLVM_consumers_do_not_yet_read_the_selfhost_envelope
projection.verified_plan | projection | ProjectionPlanId | SFProjectionPlan | SOProjectionPlanner | src/compiler/verified_projection_plan.c | pgy_verified_projection_plan_intent_observability | src/codegen/transpiler_entry.c,src/codegen/llvm_api.c | backend_materialization_guess,AST_HIR_usage_inference | tests/verified_projection_plan_smoke.sh#canonical ABI and fail-closed projection owner are closed | ACTIVE | only_intent_observability_has_a_verified_plan_row
diagnostic.catalog | diagnostic | DiagnosticId | SFDiagnosticCatalog | SODiagnosticCatalog | src/semantic/diag_codes.h | PGY_CODE_SEM_TYPE_MISMATCH | src/semantic/type_checker_diag.c,src/compiler/driver_diag.c | free_text_code_recovery,stage_guess_from_message | tests/diagnostic_registry_smoke.sh#macros and call sites ok | BRIDGE | driver_still_maps_some_free_text_messages_to_codes
artifact.zone | artifact | ArtifactId | SFBackendArtifact | SOArtifactZone | src/self_hosted/compiler/artifact_zone_owner.pgy | CompilerArtifactZoneReady | src/compiler/compiler.c,src/compiler/compiler_llvm.c,tests/self_hosted/parity/backend_output_comparator_parity.sh | path_as_artifact_identity,backend_output_without_plan_digest | tests/self_hosted_component_contract_smoke.sh#CompilerArtifactKindKnown | ACTIVE | native_artifacts_do_not_yet_carry_plan_revision_and_digest_identity
compatibility.evolution | compatibility | CompatibilitySurfaceId | SFCompatibilityEvolution | SOCompatibilityEvolution | src/self_hosted/compiler/compatibility_evolution_owner.pgy | CompilerCompatibilityEvolutionReady | src/self_hosted/tools/compatibility_evolution_checker/report_owner.pgy,src/compiler/driver_diag.c | local_compatibility_list,warning_without_migration_metadata | tests/self_hosted/parity/compatibility_evolution_manifest_parity.sh#parity ok | ACTIVE | native_diagnostic_ABI_trace_and_package_gates_do_not_all_consume_the_rows
selfhost.initializer_expression_shape | semantic | SyntaxNodeId | SFInitializerExpressionShape | SOSemanticLocalBinding | src/self_hosted/semantic/ast_local_binding_fact_owner.pgy | SemanticAstLocalBindingFacts | src/self_hosted/codegen/input/semantic_array_literal_codegen_view_owner.pgy,src/self_hosted/codegen/input/semantic_try_let_codegen_view_owner.pgy | StringTrim(,CharAt(,TypedAstArenaValueText,CodegenAstArenaValueOrDie,ContainsOutsideStrings(,FindMatchingParen( | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.collection_mutation_statement | semantic | SyntaxNodeId | SFCollectionMutationStatement | SOSemanticStatement | src/self_hosted/semantic/ast_statement_fact_owner.pgy | SemanticAstStatementFacts | src/self_hosted/codegen/input/semantic_statement_codegen_view_owner.pgy | TypedAstArenaAtomText,TypedAstArenaValueText,TypedAstArenaAuxValueText,ast_text_collection_stmt_owner.pgy,StringTrim( | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.enum_declaration_rows | semantic | SyntaxNodeId | SFEnumDeclarationRows | SOSemanticEnum | src/self_hosted/semantic/ast_enum_fact_owner.pgy | SemanticAstEnumFacts | src/self_hosted/codegen/input/semantic_enum_codegen_view_owner.pgy,src/self_hosted/codegen/emission/function_emit.pgy | TypedAstArenaAuxValueText,ExprSequenceItemCount,ExprSequenceItemAt,ast_text_enum_variant_owner.pgy,CodegenAstArenaEnumNameOrDie,CodegenAstArenaEnumVariantCount,CodegenAstArenaEnumVariantNameAt | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.nominal_declaration_rows | semantic | SyntaxNodeId | SFNominalDeclarationRows | SOSemanticNominalConstructor | src/self_hosted/semantic/ast_nominal_constructor_fact_owner.pgy | SemanticAstNominalConstructorFacts | src/self_hosted/codegen/input/semantic_nominal_codegen_view_owner.pgy,src/self_hosted/codegen/emission/function_emit.pgy | ast_text_declaration_owner.pgy,CodegenAstArenaNominalNameOrDie,CodegenAstArenaFieldNameOrDie,CodegenAstArenaFieldTypeNameOrDie,CodegenAstArenaIsNominalDecl(arena, i),CodegenAstArenaIsFieldsHeader(arena, j) | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.role_declaration_rows | semantic | SyntaxNodeId | SFRoleDeclarationRows | SOSemanticRole | src/self_hosted/semantic/ast_role_fact_owner.pgy | SemanticAstRoleFacts | src/self_hosted/codegen/input/semantic_role_codegen_view_owner.pgy,src/self_hosted/codegen/emission/function_emit.pgy | ast_text_role_declaration_owner.pgy,CodegenAstArenaRoleNameOrDie,CodegenAstArenaRoleTargetTypeNameOrDie,CodegenAstArenaIsRoleDecl(arena, i),CodegenAstArenaIsDescendantOf(arena, j, i) | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.expression_runtime_usage_surface | semantic | SyntaxNodeId | SFExpressionRuntimeUsageSurface | SOSemanticExpressionSurface | src/self_hosted/semantic/ast_expression_surface_fact_owner.pgy | SemanticAstExpressionSurfaceFacts | src/self_hosted/codegen/input/ast_expression_usage_owner.pgy,src/self_hosted/codegen/input/ast_usage_owner.pgy | TypedAstArenaAtomText,TypedAstArenaValueText,TypedAstArenaAuxValueText,ContainsCallOutsideStrings,CodegenExpressionUsageFactsFromArena,CodegenAstArenaExpressionPartsAt | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.type_runtime_usage_surface | semantic | SyntaxNodeId | SFTypeRuntimeUsageSurface | SOSemanticTypeSurface | src/self_hosted/semantic/ast_type_surface_fact_owner.pgy | SemanticAstTypeSurfaceFacts | src/self_hosted/codegen/input/ast_type_usage_owner.pgy,src/self_hosted/codegen/input/ast_usage_owner.pgy | TypedAstArenaTypeName,CodegenAstArenaTypeFactPresent,CodegenTypeUsageFactsFromArena | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.kind_runtime_usage_surface | semantic | SyntaxNodeId | SFKindRuntimeUsageSurface | SOSemanticKindSurface | src/self_hosted/semantic/ast_kind_surface_fact_owner.pgy | SemanticAstKindSurfaceFacts | src/self_hosted/codegen/input/ast_kind_usage_owner.pgy,src/self_hosted/codegen/input/ast_usage_owner.pgy | TypedAstArenaNodeKindIs,CodegenAstArenaKindPresent,CodegenKindUsageFactsFromArena,CodegenAstKindArrayLiteral | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
```
<!-- END sot-owner-spine-registry -->

## Current Judgment

The owner outline is complete for the listed compiler spine, but implementation
closure is not. There are eight `CLOSED` rows, six `BRIDGE` rows, and nine `ACTIVE`
rows. The exact counts are gate-owned and must change only when a row gains or
loses the evidence required by its status.

Current status counts: `CLOSED=8 BRIDGE=6 ACTIVE=9`.

The registry does not replace the detailed pass contract or migration ledger.
It answers a narrower question: who is allowed to decide each top-level fact
family, and which last consumers must eventually lose every alternate read.
