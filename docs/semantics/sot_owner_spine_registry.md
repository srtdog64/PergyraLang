# SoT Owner Spine Registry

Status: `architecture-owner-registry`  
Date: 2026-08-29

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
selfhost.source_format_layout | syntax | FormattedSourceArtifactPath | SFSourceFormatLayout | SOSourceFormatter | src/self_hosted/fmt/layout_owner.pgy | FormatSourceFromTokenFacts | src/self_hosted/fmt/session_owner.pgy,src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy,src/compiler/self_host_fmt_driver.c,src/compiler/fmt.c | lexer_create,lexer_next_token,parser_parse_program,format_source_to_stream,fmt_token_needs_space,fmt_token_starts_toplevel_decl,missing installed formatter retried natively,failed formatting emitted partial stdout,formatter reparses the token debug serialization,remove-then-rename publication,invalid source character deletion,lossy interpolated-string prefix,forged token kind accepted,unadmitted formatter artifact publication,valid use or lifecycle surface rejected,fixed source-suffix temporary artifact deletion,stale source overwrite,formatter replaced the source symlink inode,final source identity fell back after proof failure,atomic replacement changed source permission bits,rollback failure deleted a displaced concurrent edit,formatter argv admission accepted an ambiguous request | tests/self_hosted/parity/public_fmt_installed_self_host_owner.sh#Pergyra token/layout owner substitutes public formatting | CLOSED | none
selfhost.debug_session | execution | DebugSessionId | SFDebugSession | SODebugSession | src/self_hosted/debug/session_owner.pgy | RunDebugSession | src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy,src/compiler/self_host_debug_driver.c,src/compiler/debugger.c | parser_parse_program,semantic_analyze,debug_walk_statements,lexer_create,ParseRootProgramArtifact(,LoadSemanticSource(,missing installed debugger retried natively,debug failure re-entered the native timed pipeline | tests/self_hosted/parity/public_debug_installed_self_host_session_owner.sh#installed Pergyra owner owns the complete public session | CLOSED | none
selfhost.driver_cli_request | execution | DriverCliRequestId | SFDriverCliRequest | SODriverCliRequest | src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy | DriverRung2CliRequestFromArgsOrDie | src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy,src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy | raw_argv_reparse,optional_third_position_guess,same_argv_different_effect,implicit_default_source,artifact_without_explicit_output_token,unknown_option_as_path,test_fixture_manifest_in_production_root,public_token_native_fallback,public_token_oracle_self_compare,public_ast_native_fallback,public_ast_oracle_self_compare,public_machine_manifest_native_fallback,public_machine_manifest_reconstruction,public_capability_manifest_native_fallback,public_capability_manifest_oracle_self_compare,missing_capability_driver_native_retry,public_dir_native_fallback,public_dir_oracle_self_compare,missing_dir_driver_native_retry,source_c_artifact_machine_manifest_omission,source_c_machine_manifest_missing_or_corrupt_native_retry | tests/self_hosted/parity/installed_driver_cli_mode_owner.sh#one typed argv owner keeps public MIR diagnostic, source-C, source-MIR, and MIR-C effects disjoint,tests/self_hosted/parity/public_tokens_installed_self_host_owner.sh#installed Pergyra lexer owns public --tokens and fails closed,tests/self_hosted/parity/public_ast_installed_self_host_owner.sh#installed Pergyra parser owns public --ast and fails closed,tests/self_hosted/parity/public_machine_manifest_installed_self_host_owner.sh#installed verified companion owns public output and fails closed,tests/self_hosted/parity/public_capability_manifest_installed_self_host_owner.sh#installed semantic facts own public output and fail closed,tests/self_hosted/parity/public_dir_installed_self_host_owner.sh#installed Pergyra DIR facts own public --dir and fail closed,tests/self_hosted/parity/public_device_slot_machine_manifest_installed_self_host_owner.sh#installed companion owns DeviceSlot source-C projection and missing or corrupt evidence fails closed,tests/self_hosted/parity/driver_source_mir_execution_action_gate.sh#pure CLI request admission regained compiler or I/O authority | CLOSED | none
semantic.domain_runtime_assignment | semantic | DomainRuntimeAssignmentId | SFDomainRuntimeAssignment | SODomainRuntimeAssignment | src/semantic/domain_runtime_fact.c | semantic_collect_domain_participant_role_facts | src/semantic/type_checker_domain_projection_fields.c,src/semantic/type_checker_projection_path.c,src/compiler/hir_semantic_fact_projection.c,src/compiler/dir.c,src/compiler/dir_validate.c,src/compiler/mir_domain_runtime.c,src/compiler/mir_json_dump_domain_runtime.c,src/codegen/transpiler_domain_provenance_emit.c,src/codegen/transpiler_overlay_zone_bind.c,src/codegen/transpiler_overlay_zone_relation_bind.c,src/codegen/llvm_domain_runtime_facts.c,src/codegen/llvm_domain_projection_sync_body_helpers.c,src/codegen/llvm_domain_zone_bind_lowering.c,src/self_hosted/parser/domain_projection_map_owner.pgy,src/self_hosted/dir/domain_projection_map_row_owner.pgy,src/self_hosted/semantic/domain_projection_assignability_owner.pgy,src/self_hosted/mir/domain_runtime_assignment_fact_owner.pgy,src/self_hosted/mir/domain_runtime_assignment_json_owner.pgy,src/self_hosted/compiler/canonical_mir_identity_epoch_owner.pgy,src/self_hosted/mir_lower/domain_runtime_participant_role_fact_owner.pgy,src/self_hosted/mir_lower/domain_runtime_assignment_fact_owner.pgy,src/self_hosted/mir_lower/domain_runtime_plan_owner.pgy,src/self_hosted/compiler/domain_runtime_c_codegen_bridge_owner.pgy,src/self_hosted/codegen/input/domain_runtime_codegen_view_owner.pgy,src/self_hosted/codegen/emission/function_emit.pgy | backend_same_name_projection_join,missing_source_zero_fill,ordinal_effect_bearer,ordinal_relation_endpoint,repeated_domain_runtime_plan_validation,missing_runtime_assignment_success,foreign_valid_runtime_identity,source_to_c_runtime_plan_bypass,declaration_id_name_only_join,explicit_map_to_implicit_fold,projection_assignability_string_equality,canonical_map_child_drop | tests/self_hosted/parity/domain_runtime_assignment_execution_owner.sh#[self-host-parity:domain-runtime-assignment] PASS,tests/self_hosted/parity/domain_runtime_explicit_map_execution_owner.sh#[self-host-parity:domain-runtime-explicit-map] PASS,tests/self_hosted/parity/tobject_boundary_execution_owner.sh#production binding self-C and valid-ID wrong-kind negatives,tests/build_source_inventory_smoke.sh#emit_domain_projection_sync_loop_from_mir_runtime_facts,tests/mir_declaration_inventory_smoke.sh#llvm_build_domain_projection_value_from_runtime_facts( | BRIDGE | native and self source producers carry exact participant roles and implicit plus explicit member/path assignments through MIR; parser and DIR preserve explicit parent/entry spelling while the self semantic assignability owner decides source-to-target compatibility; native C and LLVM plus the production self source to MIR to general C entrypoint execute 7 and dst from one admission-time plan validation, including Long target from Int source; the exact BindingSlot source executes view.hp from door.hp with byte-equal direct/admitted self C and valid-ID wrong-kind rejection; declaration-level source IDs are absent from pgy.mir.v1 inventory; the self resolved semantic family is still produced at the MIR boundary, and dirty epoch, full lifecycle operations, the general explicit projection builtin, world-effect ordinal bridge, and one shared native/self runtime plan remain open
source.module_graph | syntax | SourceArtifactId | SFSourceModuleGraph | SOModuleLoader | src/compiler/module_loader.c | module_loader_load_program_with_graph | src/compiler/driver_app.c | unowned_source_reload,module_path_string_identity | tests/source_module_graph_smoke.sh#driver_validates_and_destroys_graph | CLOSED | none
lexer.token_stream | syntax | TokenStreamId | SFTokenStream | SOLexer | src/lexer/lexer.c | lexer_token_stream_handle | src/parser/parser.c | parser_relex,source_bytes_as_token_identity | tests/lexer_token_stream_anchor_smoke.sh#lexer_parser_anchor_and_byte_output | CLOSED | none
lexer.language_word_registry | syntax | LanguageWordId | SFLanguageWordRegistry | SOLanguageLexicon | src/lexer/language_keyword_registry.def | PGY_LANGUAGE_WORD_REGISTRY_ROWS | src/lexer/lexer_keywords.c,src/lexer/lexer_token_debug.c,src/parser/parser.c,src/self_hosted/lexer/language_keyword_registry_projection_owner.pgy,src/self_hosted/parser/cursor_owner.pgy,src/self_hosted/parser/decl_ability_owner.pgy,src/self_hosted/parser/decl_dispatch_owner.pgy,src/self_hosted/parser/decl_effect_relation_owner.pgy,src/self_hosted/parser/decl_enum_owner.pgy,src/self_hosted/parser/decl_event_owner.pgy,src/self_hosted/parser/decl_intent_owner.pgy,src/self_hosted/parser/decl_nominal_owner.pgy,src/self_hosted/parser/decl_role_owner.pgy,src/self_hosted/parser/decl_type_owner.pgy,src/self_hosted/parser/decl_zone_owner.pgy,src/self_hosted/parser/expr_precedence_owner.pgy,src/self_hosted/parser/expr_primary_owner.pgy,src/self_hosted/parser/function_decl_owner.pgy,src/self_hosted/parser/stmt_if_owner.pgy,src/self_hosted/parser/stmt_loop_owner.pgy,src/self_hosted/parser/stmt_match_owner.pgy,src/self_hosted/parser/stmt_owner.pgy,src/self_hosted/parser/stmt_parallel_owner.pgy,src/self_hosted/parser/type_name_owner.pgy,src/lsp/pgy_lsp_protocol.c,src/lsp/pgy_lsp_hover.c,src/self_hosted/lsp/completion_owner.pgy,src/self_hosted/lsp/feature_owner.pgy,src/self_hosted/lsp/hover_content_projection_owner.pgy,editor/vscode-pergyra/syntaxes/pergyra.tmLanguage.json,docs/grammar/01_syntax.md,docs/grammar/02_grammar.md | native_keyword_table,token_debug_keyword_switch,selfhost_handwritten_keyword_map,parser_unregistered_contextual_selector,selfhost_raw_keyword_selector,lsp_hardcoded_completion_words,selfhost_empty_completion_provider,lsp_unregistered_hover_word,selfhost_handwritten_hover_table,textmate_only_language_word,textmate_scope_as_authority,docs_keyword_list_as_authority,second_tmLanguage | tests/language_keyword_registry_smoke.sh#146 rows,tests/self_hosted/parity/parser_language_word_registry_parity.sh#typed selectors and parser boundaries ok,tests/lsp_completion_registry_smoke.sh#lsp_build_completion_items_json,tests/lsp_hover_registry_smoke.sh#LspHoverPresentationTextForWord,tests/vscode_language_graph_smoke.sh#one canonical grammar | BRIDGE | all 146 stable word identities, metadata, selfhost projection, LSP completion, and exact TextMate spelling/scope are registry-directed; implementation census is still BRIDGE: native+selfhost-typed 85, native+selfhost-direct-only 18, native-only 43, with 51 direct selfhost selectors across 36 words that cannot count as typed closure
semantic.callable_contract_vocabulary | semantic | PgyCallableContractWordId | SFCallableContractVocabulary | SOCallableContractVocabulary | src/semantic/callable_contract_vocabulary.def | PGY_CALLABLE_CONTRACT_VOCABULARY_ROWS | src/parser/parser_decl_clause.c,src/parser/ast_print.c,src/semantic/type_checker_helpers_effects.c,src/semantic/type_checker_flow_effects.c,src/semantic/capability_analyze.c,src/runtime/pgy_callable_contract_capability_projection.h,src/runtime/pgy_runtime_capability.h,src/compiler/mir_json_dump_decl.c,src/compiler/mir_decl_header_validate.c,src/self_hosted/lib/callable_contract_vocabulary_projection_owner.pgy,src/self_hosted/parser/function_decl_owner.pgy,src/self_hosted/semantic/ast_action_contract_fact_owner.pgy,src/self_hosted/mir/declaration_verify_owner.pgy,src/self_hosted/mir_lower/declaration_method_contract_fact_owner.pgy | parser_local_capability_table,parser_local_effect_table,ast_print_mask_name_table,semantic_effect_word_table,runtime_capability_word_table,mir_json_mask_name_table,selfhost_contract_vocabulary_array,mir_lower_chained_vocabulary_predicate,literal_known_mask_union,duplicate_clause_word_success,local_nonlocal_mix_success,projection_drift | tests/callable_contract_vocabulary_smoke.sh#18 semantic words and projections: ok,tests/self_hosted/parity/driver_rung2_action_contract_parity_owner.sh#ActionContract carriage and fail-closed wire mutations | CLOSED | none
semantic.builtin_capability_policy | semantic | BuiltinCapabilityPolicyId | SFBuiltinCapabilityPolicy | SOBuiltinCapabilityPolicy | src/semantic/builtin_capability_registry.def | PGY_BUILTIN_CAPABILITY_POLICY_ROWS | src/semantic/capability_analyze.c,src/semantic/type_checker_program.c,src/semantic/type_checker_builtins_stdlib_body.c,src/runtime/pgy_runtime_io_qubit_inline.h,src/runtime/pgy_runtime_lib_io_string_exports.h,src/self_hosted/semantic/builtin_capability_projection_owner.pgy,src/self_hosted/semantic/ast_capability_fact_owner.pgy,src/self_hosted/lsp/live_session_owner.pgy | native_builtin_capability_switch,selfhost_builtin_capability_switch,manifest_builtin_name_rescan,missing_builtin_capability_registry_success,print_semantic_capability_bypass,print_runtime_capability_bypass,print_bytes_before_capability_admission | tests/builtin_capability_registry_smoke.sh#builtin capability registry smoke: ok,tests/capability/run_runtime_enforce.sh#print denied clean,tests/self_hosted/parity/lsp_live_session_owner.sh#io_write denial emitted a partial protocol response | CLOSED | none
semantic.file_mode_capability_policy | semantic | FileModeCapabilityPolicyId | SFFileModeCapabilityPolicy | SOFileModeCapabilityPolicy | src/runtime/pgy_file_mode_capability.def | PGY_FILE_MODE_CAPABILITY_POLICY_ROWS | src/runtime/pgy_runtime_file_mode_capability.h,scripts/render_builtin_capability_registry.py,src/self_hosted/semantic/builtin_capability_projection_owner.pgy,src/self_hosted/semantic/ast_capability_fact_owner.pgy | native_file_mode_character_switch,selfhost_file_mode_character_switch,dynamic_file_mode_read_only_default,unknown_file_mode_empty_capability | tests/builtin_capability_registry_smoke.sh#builtin capability registry smoke: ok | CLOSED | none
selfhost.source_capability_facts | semantic | SelfHostSourceCapabilityFactId | SFSelfHostSourceCapabilityFacts | SOSelfHostSemanticCapability | src/self_hosted/semantic/ast_capability_fact_owner.pgy | SemanticAstCapabilityFactsFromAdmittedBody | src/self_hosted/compiler/capability_manifest_owner.pgy,src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy | capability_declared_as_used,capability_renderer_builtin_rescan,capability_missing_call_target_success,capability_program_mask_without_callable_fixed_point,immutable_source_capability_manifest_companion,public_capability_manifest_native_fallback,missing_capability_driver_native_retry | tests/self_hosted/parity/public_capability_manifest_installed_self_host_owner.sh#installed semantic facts own public output and fail closed,tests/builtin_capability_registry_smoke.sh#builtin capability registry smoke: ok | CLOSED | none
semantic.callable_receiver_carriage | abi | CallableSyntaxId | SFCallableReceiverCarriage | SOSemanticCallableReceiver | src/self_hosted/semantic/callable_receiver_carriage_policy_owner.pgy | CallableReceiverCarriageKnown | src/compiler/mir.c,src/compiler/mir_program_fact_validate.c,src/compiler/mir_json_dump.c,src/self_hosted/mir/routine_receiver_carriage_owner.pgy,src/self_hosted/mir/json_projection_owner.pgy,src/self_hosted/mir/program_json_artifact_writer_owner.pgy,src/self_hosted/mir_lower/program_routine_receiver_identity_owner.pgy,src/self_hosted/mir_lower/program_routine_index_owner.pgy,src/self_hosted/semantic/role_operator_resolution_owner.pgy,src/self_hosted/semantic/ast_body_role_operator_resolution_owner.pgy,src/self_hosted/codegen/input/callable_receiver_codegen_view_owner.pgy,src/self_hosted/compiler/codegen_callable_receiver_bridge_owner.pgy,src/self_hosted/codegen/emission/function_emit.pgy,src/self_hosted/codegen/emission/member_call_receiver_carriage_owner.pgy,src/self_hosted/codegen/emission/role_receiver_binding_owner.pgy,src/self_hosted/compiler/direct_mir_mutable_nominal_identity_plan_owner.pgy,src/self_hosted/compiler/direct_mir_role_operator_declaration_fact_owner.pgy,src/self_hosted/compiler/direct_mir_role_operator_plan_owner.pgy,src/self_hosted/compiler/direct_mir_role_operator_abi_projection_owner.pgy,src/self_hosted/compiler/direct_mir_role_operator_emission_owner.pgy | missing_receiver_carriage_success,unknown_receiver_carriage_success,owner_name_only_join,duplicate_nominal_receiver_policy,mutable_identity_by_value,temporary_mutable_receiver_address,role_erased_mutable_receiver_copy,missing_concrete_role_target_carriage_success,late_role_target_rescan,general_parameter_abi_from_receiver_carriage,role_operator_target_from_spelling,sole_visible_role_dispatch,primitive_operator_retry_after_role_claim | tests/self_hosted/parity/mir_receiver_carriage_admission_owner.sh#[self-host-parity:mir-receiver-carriage-admission] PASS,tests/self_hosted/parity/driver_rung2_callable_receiver_carriage_owner.sh#accepted a temporary mutable receiver,tests/self_hosted/parity/driver_rung2_callable_receiver_carriage_owner.sh#mutable role target was copied,tests/self_hosted/parity/one_mir_mutable_nominal_identity_projection.sh#subject exact 7 + vessel exact 13, one mutable owner,tests/self_hosted/parity/one_mir_role_operator_projection.sh#exact C/LLVM 123, 6 metamorphic cases,tests/self_hosted/parity/codegen_role_receiver_admission_owner.sh#PASS exact base/metamorphic execution and negative admission | BRIDGE | native and self MIR carry mandatory none/value/mutable-identity rows through exact callable/declaration identity and the general self C consumer emits pointer self plus stable-address calls; role-erased local ABI preserves mutable concrete targets behind the erased pointer. The bounded role-operator path now resolves one semantic target, carries it through MIR, and executes exact 123 through one target-neutral plan plus selected C/LLVM ABI projection without source, display, sole-role, or primitive-arithmetic fallback. Bounded subject/vessel literals exact-7/13 also consume this carriage without aggregate copy. The general self codegen receiver now binds exact role target type plus distinct value-or-identity carriage once at callable admission, executes builtin Int by value, preserves mutable nominal pointer identity, and rejects late nominal-kind reconstruction or non-copyable builtin targets. The row remains BRIDGE because MIR does not yet carry this concrete target carriage as its own callable row and native C/LLVM plus general parameter ABI still reuse the broader uses_pointer_self compatibility policy
semantic.nominal_field_kind | semantic | SyntaxNodeIdFieldOrdinal | SFNominalFieldKind | SONominalFieldKind | src/compiler/mir_decl_field_kind_vocabulary.def | PGY_MIR_DECL_FIELD_KIND_ROWS | src/compiler/mir_json_dump_decl.c,src/self_hosted/lib/mir_decl_field_kind_vocabulary_projection_owner.pgy,src/self_hosted/lib/nominal_field_kind_owner.pgy,src/self_hosted/hir/ast_text_inventory_owner.pgy,src/self_hosted/semantic/ast_nominal_constructor_fact_owner.pgy,src/self_hosted/mir/declaration_rows_owner.pgy,src/self_hosted/mir/declaration_verify_owner.pgy,src/self_hosted/mir/declaration_json_projection_owner.pgy,src/self_hosted/mir_lower/program_declaration_field_identity_index_owner.pgy,src/self_hosted/mir_lower/decl_lower.pgy | native_field_kind_string_table,selfhost_field_kind_string_table,field_text_as_semantic_kind,field_type_as_semantic_kind,nominal_name_as_field_kind,unknown_field_kind_success,invalid_host_field_kind_success,subject_slot_as_field,effect_slot_as_field,relation_slot_as_effect_slot,pool_without_capacity,field_name_with_foreign_valid_source_syntax_id,field_identity_kind_drift,projection_drift | tests/mir_decl_field_kind_vocabulary_smoke.sh#14 wire identities and projections: ok,tests/self_hosted/parity/driver_rung2_effect_declaration_parity_owner.sh#effect declaration and subject/effect slot mutations,tests/self_hosted/parity/domain_topology_admission_owner.sh#exact declaration field identity joins and topology mutations are fail-closed,tests/self_hosted/parity/tobject_boundary_execution_owner.sh#production binding self-C and valid-ID wrong-kind negatives | BRIDGE | vocabulary and current effect/zone field-kind carrier are registry-directed; declaration field identity is carried and exact-joined within one producer revision, relation declarations and non-empty canonical remap are present, and exact canonical C execution is green; raw native/self ID equality is not a contract, the binding distinction executes through production self C with exact native C/LLVM parity and valid-ID wrong-kind rejection, while owner declaration identity,pool capacity,vessel distinction,VesselSlot carriage,projection member maps,layer destination roles and C/LLVM runtime consumers remain open
parser.syntax_provenance | syntax | SyntaxNodeId | SFSyntaxProvenanceTree | SOParserAst | src/parser/ast.c | ast_program_append_statement | src/semantic/semantic.c,src/compiler/hir.c,src/compiler/dir.c,src/compiler/rir_builder.c | raw_AST_pointer_identity,name_or_span_identity | tests/stable_identity_contract_smoke.sh#overflow fail-close | BRIDGE | annotated_AST_fans_out_to_semantic_and_three_IR_lowerers
selfhost.match_case_pattern | syntax | SyntaxNodeId | SFMatchCasePattern | SOAstMatchCasePattern | src/self_hosted/hir/ast_match_pattern_fact_owner.pgy | AstMatchCasePatternFactFromArtifact | src/self_hosted/semantic/ast_statement_fact_owner.pgy,src/self_hosted/semantic/ast_match_binding_environment_owner.pgy,src/self_hosted/mir/routine_match_owner.pgy,src/self_hosted/codegen/input/semantic_statement_codegen_view_owner.pgy,src/self_hosted/codegen/emission/option_match_owner.pgy,src/self_hosted/codegen/emission/tagged_enum_match_owner.pgy | match_pattern_graphs,match_case_ordinal_join,semantic_consumer_atom_parse,mir_match_pattern_text_parse,codegen_match_pattern_text_parse,consumer_local_pattern_parse | tests/self_hosted_component_contract_smoke.sh#SemanticAstMatchCasePatternFactForNode(,tests/self_hosted/parity/semantic_expression_environment_owned_lifetime_smoke.sh#SemanticAstMatchCasePatternFactForNode(,tests/self_hosted/parity/routine_build_storage_lifetime_owner.sh#SemanticAstMatchCasePatternFactForNode(,tests/self_hosted/parity/driver_rung2_match_parity_owner.sh#pgy_selfhost_verify_driver_rung2_match(),tests/self_hosted/parity/codegen_parity.sh#run_tool_fixture() | CLOSED | none
semantic.symbol_type_graph | semantic | SymbolId | SFSemanticSymbolTypeGraph | SOSemanticAnalyzer | src/semantic/semantic.c | semantic_analyze_ex | src/semantic/slot_analyzer_lookup.c,src/compiler/hir.c,src/compiler/mir.c,src/compiler/air_evidence_dag.c | AST_annotation_as_final_type,slot_AST_program_root_rescan_with_host_index,backend_type_recovery | tests/slot_analyzer_host_index_smoke.sh#hash owner and no-fallback gates passed | BRIDGE | MIR_and_AIR_still_receive_semantic_context_and_type_spellings
semantic.destructure_binding_type | semantic | DestructureSyntaxIdBindingIndex | SFSemanticDestructureBindingType | SOSemanticDestructureType | src/semantic/destructure_type_fact.c | semantic_destructure_type_fact_record | src/compiler/hir_destructure_type_facts.c,src/compiler/mir_destructure_type_facts.c,src/compiler/mir_source_local_types.c,src/compiler/mir_json_dump_flow.c,src/self_hosted/mir/destructure_type_fact_owner.pgy,src/self_hosted/mir/destructure_type_json_projection_owner.pgy | mir_source_local_expr_type_name,mir_source_local_unwrap_array_or_slice_type,mir_source_local_tuple_element_type | tests/destructure_type_fact_smoke.sh#missing semantic destructure fact must fail MIR lowering | CLOSED | none
semantic.match_binding_type | semantic | MatchCaseSyntaxIdBindingIndex | SFSemanticMatchBindingType | SOSemanticMatchBindingType | src/semantic/type_checker_flow_match.c | semantic_match_binding_type_fact_record | src/compiler/hir_semantic_fact_projection.c,src/compiler/mir_branch_source_facts.c,src/compiler/mir_json_dump.c,src/self_hosted/mir_lower/match_binding_local_fact_owner.pgy,src/self_hosted/mir_lower/match_binding_render_owner.pgy | variant_name_type_guess,untyped_match_binding_render,source_match_binding_type_reparse | tests/match_binding_type_fact_smoke.sh#semantic -> HIR -> MIR -> Pergyra carrier ok | CLOSED | none
semantic.resource_flow_universe | resource | SymbolId | SFResourceFlowUniverse | SOResourceFlowUniverse | src/semantic/type_checker_flow_universe.c | resource_flow_universe_bind | src/semantic/type_checker_flow_resources.c,src/semantic/type_checker_flow_loop_summary.c,src/compiler/hir.c,src/compiler/mir.c,src/compiler/mir_program_validate.c,src/compiler/mir_program_fact_validate.c,src/compiler/mir_json_dump.c,src/compiler/rir_flow.c,src/compiler/rir_validation.c,src/compiler/rir_public_surface.c,src/self_hosted/mir_lower/routine_fact_index_owner.pgy,src/self_hosted/mir_lower/routine_lower.pgy | Symbol_pointer_as_function_flow_identity,missing_universe_pointer_fallback,semantic_rows_direct_to_dir_or_rir,selfhost_resource_flow_symbol_recovery,mir_json_hir_only_resource_fact_owner,DIR_resource_flow_reserialization | tests/dir_resource_flow_identity_smoke.sh#HIR owns validated routine-local rows; DIR carries no duplicate snapshot,tests/self_hosted/mir_resource_flow_identity_smoke.sh#MIR-owned ResourceFlowUniverse rows reach self-host identity and malformed counts fail closed | BRIDGE | native semantic owner records rows once; HIR is the validated routine-local adapter and DIR has no consumer or duplicate serialization of this family; MIR and RIR routines own validated local projections and their JSON surfaces emit those rows; selfhosted mir_lower validates count, required identity fields, parameter boundaries, duplicate stable indexes, and LoopFlow state references; typed DIR/RIR transition payload migration beyond these projections remains open
semantic.loop_flow_summary | semantic | SyntaxNodeId | SFLoopFlowSummary | SOLoopFlowSummary | src/semantic/type_checker_flow_loop_summary.c | loop_flow_summary_record | src/semantic/type_checker_flow_loops.c,src/semantic/semantic.c,src/compiler/hir.c,src/compiler/mir.c,src/compiler/mir_program_validate.c,src/compiler/mir_program_fact_validate.c,src/compiler/mir_json_dump.c,src/self_hosted/mir/loop_flow_rows_owner.pgy,src/self_hosted/mir_lower/routine_fact_index_owner.pgy,src/self_hosted/mir_lower/routine_lower.pgy | repeated_loop_body_reinspection,missing_summary_timeout_success,selfhost_loop_flow_summary_recovery,mir_json_hir_only_loop_fact_owner,selfhost_loop_effect_or_state_default_claim | tests/self_hosted/parity/driver_rung2_mir_producer_parity_owner.sh#loop summary owner row was lost | BRIDGE | native semantic owner records complete rows; HIR is the adapter and MIR routine owns the validated JSON handoff; the bounded self-host producer now carries loop identity and kind from typed semantic statement facts and emits explicit empty effect/resource-state rows for its current no-state frontier; this is not complete semantic loop-flow ownership, and non-empty effect/state plus must-return flags remain a self-host semantic-owner obligation; mir_lower validates carried rows and never recovers them from CFG text
semantic.function_param_flow_summary | resource | FunctionIdParamIndex | SFFunctionParamFlowSummary | SOFunctionParamFlowSummary | src/semantic/function_param_flow_summary.c | function_param_flow_summary_demand | src/semantic/slot_analyzer.c,src/semantic/slot_analyzer_access.c,src/semantic/slot_analyzer_escape.c,src/compiler/hir.c,src/compiler/mir.c,src/compiler/mir_program_fact_validate.c,src/compiler/air_evidence_mir.c,src/self_hosted/mir_lower/routine_fact_index_owner.pgy | recursive_callee_body_reopen,depth_limited_summary_truncation,unknown_HIR_routine_attachment,unknown_MIR_routine_attachment,unknown_AIR_routine_attachment,unknown_selfhost_summary_consumer | tests/self_hosted/mir_function_param_flow_summary_smoke.sh#live MIR JSON summary rows reach routine identity and malformed rows fail closed | BRIDGE | native semantic owner and selfhosted reader agree on routine-owned rows; 100-fixture parity is additionally tracked by tests/self_hosted/parity/mir_json_parity.sh; full owner migration and deletion of native-only consumers remain open
semantic.machine_layer_transition | execution | MachineLayerId | SFMachineLayerTransition | SOMachineLayer | src/compiler/mir_machine_layer.c | mir_attach_machine_layer_fact | src/compiler/rir.h,src/compiler/rir_builder_walk.c,src/compiler/rir_public_surface.c,src/compiler/air_evidence_mir_facts.c,src/compiler/air_validate.c,src/compiler/machine_layer_manifest.c,src/compiler/verified_projection_plan.c,src/compiler/driver_app.c,src/compiler/driver_app.h,src/compiler/self_host_machine_manifest_artifact_owner.c,src/runtime/pgy_runtime_machine_layer_inline.h,src/runtime/pgy_runtime_lib_machine_layer_exports.h,src/self_hosted/compiler/machine_layer_declaration_consumer.pgy,src/self_hosted/compiler/machine_layer_runtime_projection_owner.pgy,src/self_hosted/compiler/machine_layer_runtime_binding_owner.pgy,src/self_hosted/mir/machine_layer_projection_owner.pgy,src/self_hosted/mir/json_projection_owner.pgy,src/self_hosted/tools/machine_layer_rir_validator/main.pgy,docs/semantics/proofs/MachineLayerCore.v,docs/semantics/proofs/ResourceMachineBridge.v,tests/machine_layer_core_smoke.sh,tests/self_hosted/mir_machine_layer_smoke.sh | bool_capability_gate,region_place_as_contact,machine_map_assumed_without_adequacy,missing_contact_event,backend_machine_fact_recovery,backend_projection_default,air_manifest_rescan,llvm_device_projection_reuses_slot_type,physical_declaration_default,physical_shape_default,selfhost_machine_row_recovery,selfhost_rir_contact_recovery,mir_machine_layer_ast_requires_fact,rir_json_source_summary_prefix,runtime_bind_fingerprint_drop,provider_binding_drop,selfhost_startup_mapping_drop,resource_identity_as_machine_address,machine_address_as_resource_authority,missing_companion_native_retry,selfhost_physical_manifest_reconstruction | tests/machine_layer_pipeline_smoke.sh#RIR JSON/MIR/AIR facts and C/LLVM contact lowering are connected; distinct C/LLVM physical projection rows, checked host-sim MachineDeclaration provenance envelope, self-host startup mapping, and fail-closed manifest validation,tests/self_hosted/parity/public_machine_manifest_installed_self_host_owner.sh#installed verified companion owns public output and fails closed | BRIDGE | abstract DeviceSlot manifest, clean RIR pgy.rir.v1 JSON handoff, checked declaration plus one-shot native provider binding seam, board/boot/linker provenance carriage, contact-to-runtime-operation identity, exact physical base/size/mode carriage, owner-directed C/LLVM/self-hosted projection rows, verified-plan fingerprint bind and provider-required startup guard at C/LLVM/self-host are wired; public manifest delivery now consumes one native-generated immutable companion through the installed declaration validator without repeating physical literals or retrying native semantics; `ResourceMachineBridge.v` proves the model-level non-inference and explicit binding contract without becoming a second implementation owner; the selfhost producer derives rows from semantic call-target graph facts and the selfhost reader/AIR/RIR validators reject shape drift; LLVM preserves a distinct named DeviceSlot aggregate/runtime ABI; AIR delegates site validation to the manifest owner; concrete board/linker/boot provider implementation and beta-stable Region/Grant syntax remain embedder/product work
hir.typed_control_flow | semantic | RoutineId | SFHirTypedControlFlow | SOHir | src/compiler/hir.c | hir_lower | src/compiler/dir.c,src/compiler/rir_flow.c,src/compiler/mir.c,src/compiler/air.c | AST_body_rescan,name_join_call_identity | tests/hir_routine_identity_smoke.sh#without a name fallback | BRIDGE | DIR top-level inventory/edge iteration and RIR resource-flow enrichment consume HIR; typed domain payload rows and complete HIR-owned control-flow input remain open
dir.domain_graph | semantic | DomainNodeId | SFDirDomainGraph | SODir | src/compiler/dir.c | dir_lower | src/compiler/dir_validate.c,src/compiler/rir_validation_dir.c,src/compiler/air.c,src/compiler/mir_domain_topology.c,src/compiler/mir_json_dump_domain_topology.c,src/compiler/mir_json_dump_decl.c,src/compiler/propagation_graph_build.c,src/codegen/transpiler_zone_decl_emit.c,src/codegen/llvm_domain_zone_sync.c,src/self_hosted/dir/domain_graph_fact_owner.pgy,src/self_hosted/dir/zone_state_row_fact_owner.pgy,src/self_hosted/dir/domain_graph_inventory_owner.pgy,src/self_hosted/compiler/dir_text_artifact_owner.pgy,src/self_hosted/compiler/dir_intent_text_artifact_owner.pgy,src/self_hosted/dir/domain_topology_row_owner.pgy,src/self_hosted/dir/domain_projection_map_row_owner.pgy,src/self_hosted/mir/domain_topology_fact_owner.pgy,src/self_hosted/mir/json_projection_owner.pgy,src/self_hosted/mir/program_json_artifact_writer_owner.pgy,src/self_hosted/compiler/canonical_mir_identity_epoch_owner.pgy,src/self_hosted/compiler/canonical_mir_field_identity_epoch_owner.pgy,src/self_hosted/mir_lower/json_fact_read.pgy,src/self_hosted/mir_lower/program_declaration_index_owner.pgy,src/self_hosted/mir_lower/program_declaration_field_identity_index_owner.pgy,src/self_hosted/mir_lower/domain_topology_fact_owner.pgy,src/self_hosted/mir_lower/domain_topology_graph_plan_owner.pgy,src/self_hosted/mir_lower/domain_topology_graph_build_owner.pgy,src/self_hosted/mir_lower/domain_topology_graph_schedule_owner.pgy,src/self_hosted/mir_lower/machine_layer_fact_owner.pgy,src/self_hosted/mir_lower/mir_json_input_owner.pgy,src/self_hosted/compiler/domain_topology_graph_plan_consumer_owner.pgy,src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy | AST_domain_rescan,name_only_domain_join,missing_source_owner_identity,wrong_source_DIR_binding,backend_AST_zone_frontier_graph,missing_topology_row_success,selfhost_domain_topology_source_recovery,selfhost_domain_topology_missing_success,field_name_with_foreign_valid_source_syntax_id,field_identity_kind_drift,declaration_field_missing_or_duplicate_source_syntax_id,declaration_count_graph_id,count_only_dir_inventory,source_id_numeric_equality_as_cross_producer_identity,constant_graph_id,native_oracle_graph_graft,nonempty_topology_to_empty_rows,canonicalizer_second_graph_admission,cross_epoch_identity_offset_repair,canonical_stale_field_identity,repeated_domain_topology_plan_validation,backend_domain_plan_rebuild,same_name_projection_member_join,ordinal_layer_destination_join,layer_storage_zero_fill_as_runtime,by_value_zone_receiver_as_identity,public_dir_native_fallback,public_dir_oracle_self_compare,missing_dir_driver_native_retry | tests/dir_domain_identity_smoke.sh#DIR source/owner identity is required for RIR validation,tests/domain_runtime_topology_smoke.sh#DIR -> MIR -> C/LLVM zone frontier topology and declaration field identity are exact,tests/self_hosted/parity/domain_topology_admission_owner.sh#exact declaration field identity joins and topology mutations are fail-closed,tests/self_hosted/parity/driver_rung2_domain_graph_producer_owner.sh#self DIR census did not reproduce the native anchor,tests/self_hosted/parity/dir_graph_inventory_owner.sh#exact rows including zone state, admitted intent defaults, transfer detail, and inline sub-intents match native; count-only, provenance/transfer mutations, and wrong-endpoint paths fail closed,tests/self_hosted/parity/public_dir_installed_self_host_owner.sh#installed Pergyra DIR facts own public --dir and fail closed,tests/self_hosted/parity/driver_rung2_domain_topology_producer_owner.sh#topology admitted exact-field mutation,tests/self_hosted/parity/driver_rung2_canonical_identity_epoch_owner.sh#exact hosted-method tree ID, apply/link epoch remap, and stale/wrong-kind field-ID negatives ok,tests/self_hosted/parity/domain_topology_graph_plan_consumer_owner.sh#self-produced topology reached one target-neutral C/LLVM plan,tests/self_hosted/parity/tobject_boundary_execution_owner.sh#production binding self-C and valid-ID wrong-kind negatives | BRIDGE | projection refresh/publish/bind, distinct apply lifecycle, maintained-effect and relation-link rows carry stable directive/slot identity; apply is admitted as a no-edge lifecycle row rather than folded into maintain; MIR is a carrier and the native C/LLVM zone frontier graph is SUBSTITUTING with the old AST graph entrypoint deleted; the bounded self-host empty-topology producer and the zone_layer_projection_runtime non-empty refresh/publish/apply/link producer are SUBSTITUTING C-owner replacements with exact declaration-field joins and atomic canonical identity remap; exact participant roles and implicit plus explicit projection paths are now owned by semantic.domain_runtime_assignment and production native C, LLVM, and self C execute 7 and dst; LockZone refresh carries the exact BindingSlot source into byte-equal direct/admitted self C, while valid-ID object/tobject role drift is rejected before C; the admitted self-host debug inventory now matches native node/edge/topology rows for authority, party, relation/effect projection, and semicolon-optional zone-state inputs after normalizing only producer-local source syntax IDs, while participant, explicit-step, and action-default intent detail now render from typed provenance rows; this graph family remains BRIDGE for layer pool/materialization, dirty/epoch, detach/unlink/state scheduling, authority/action transition, world/intent production reachability, and one shared native/self runtime plan; callable receiver carriage substitutes the self general C path but native C/LLVM parameter policy remains a bridge
rir.resource_transition_graph | resource | ResourceTransitionId | SFRirResourceTransitionGraph | SORir | src/compiler/rir_builder.c | rir_lower | src/compiler/rir_flow.c,src/compiler/rir_validation.c,src/compiler/rir_public_surface.c,src/compiler/mir.c,src/compiler/air.c | AST_resource_rescan,runtime_manager_as_static_proof,unvalidated_param_summary_copy | tests/rir_resource_flow_identity_smoke.sh#RIR-owned ResourceFlow rows reach JSON and malformed identity fails closed | BRIDGE | RIR captures source IDs at initial fact collection, HIR is the lowering adapter, and RIR scope owns the copied ResourceFlow identity table consumed by attachment, validation, and JSON; initial AST-owned shape/resource collection and selfhosted RIR consumer remain open
mir.execution_graph | execution | ValueId | SFMirExecutionGraph | SOMir | src/compiler/mir.c | mir_lower | src/codegen/transpiler_entry.c,src/codegen/llvm_api.c,src/compiler/air_evidence_mir.c,src/compiler/mir_json_expression_graph.c,src/self_hosted/mir/routine_local_inventory_owner.pgy,src/self_hosted/mir/routine_expression_use_owner.pgy,src/self_hosted/mir/routine_assignment_owner.pgy,src/self_hosted/mir/routine_for_owner.pgy,src/self_hosted/mir/routine_iteration_owner.pgy,src/self_hosted/mir/routine_tracked_statement_owner.pgy,src/self_hosted/mir/routine_statement_owner.pgy,src/self_hosted/mir/routine_if_owner.pgy,src/self_hosted/mir/routine_while_owner.pgy,src/self_hosted/mir/routine_local_predecessor_snapshot_owner.pgy,src/self_hosted/mir/routine_loop_header_phi_owner.pgy,src/self_hosted/mir/routine_loop_header_backedge_binding_owner.pgy,src/self_hosted/mir/routine_loop_exit_phi_owner.pgy,src/self_hosted/mir/routine_match_owner.pgy,src/self_hosted/mir/routine_destructure_owner.pgy,src/self_hosted/mir_lower/expression_graph_fact_owner.pgy,src/self_hosted/mir_lower/expression_graph_parser_bridge_owner.pgy,src/self_hosted/mir_lower/expression_graph_sequence_owner.pgy,src/self_hosted/mir_lower/routine_instruction_use_fact_owner.pgy,src/self_hosted/mir_lower/routine_result_definition_fact_owner.pgy,src/self_hosted/mir_lower/phi_fact_owner.pgy,src/self_hosted/mir_lower/ssa_identity_owner.pgy,src/self_hosted/mir_lower/machine_layer_fact_owner.pgy,src/self_hosted/air/mir_cfg_identity_owner.pgy,src/self_hosted/air/mir_cfg_certificate_fact_owner.pgy,src/self_hosted/air/mir_nested_cfg_certificate_fact_owner.pgy,src/self_hosted/air/mir_loop_cfg_certificate_fact_owner.pgy,src/self_hosted/air/mir_range_cfg_certificate_fact_owner.pgy,src/self_hosted/air/mir_break_cfg_certificate_fact_owner.pgy,src/self_hosted/air/mir_cfg_certificate_value_owner.pgy,src/self_hosted/air/mir_cfg_certificate_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_graph_admission_owner.pgy,src/self_hosted/compiler/direct_mir_nominal_literal_graph_fact_owner.pgy,src/self_hosted/compiler/direct_mir_nominal_literal_program_identity_owner.pgy,src/self_hosted/compiler/direct_mir_nominal_literal_program_admission_owner.pgy,src/self_hosted/compiler/direct_mir_mutable_nominal_identity_plan_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_member_graph_fact_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_member_instruction_admission_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_member_plan_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_generic_member_method_graph_fact_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_generic_member_main_graph_fact_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_generic_member_method_instruction_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_generic_member_main_instruction_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_generic_member_plan_owner.pgy,src/self_hosted/compiler/direct_mir_cfg_entry_fact_owner.pgy,src/self_hosted/compiler/direct_mir_cfg_log_shape_owner.pgy,src/self_hosted/compiler/direct_mir_cfg_shape_fact_owner.pgy,src/self_hosted/compiler/direct_mir_nested_cfg_shape_owner.pgy,src/self_hosted/compiler/direct_mir_loop_cfg_shape_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_graph_identity_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_graph_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_range_iteration_owner.pgy,src/self_hosted/compiler/direct_mir_cfg_plan_fact_owner.pgy,src/self_hosted/compiler/direct_mir_loop_cfg_plan_fact_owner.pgy,src/self_hosted/compiler/direct_mir_cfg_plan_value_owner.pgy,src/self_hosted/compiler/direct_mir_cfg_plan_mutation_owner.pgy,src/self_hosted/compiler/direct_mir_cfg_plan_owner.pgy,src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy,src/self_hosted/compiler/direct_mir_backend_emission_owner.pgy,src/self_hosted/compiler/direct_mir_nested_cfg_emission_owner.pgy,src/self_hosted/compiler/direct_mir_loop_cfg_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_range_emission_owner.pgy,src/self_hosted/mir_lower/intent_execution_plan_fact_owner.pgy,src/self_hosted/compiler/driver_rung2_intent_consumer_owner.pgy,src/self_hosted/codegen/input/intent_execution_codegen_view_owner.pgy,src/self_hosted/codegen/emission/intent_execution_plan_emit_owner.pgy,src/self_hosted/codegen/emission/intent_execution_plan_control_emit_owner.pgy,src/self_hosted/codegen/emission/intent_execution_plan_local_emit_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_identity_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_call_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_direct_call_expression_owner.pgy | backend_AST_semantic_read,silent_i32_fallback,residual_source_payload_dispatch,mir_json_expression_text_reparse,routine_local_stack_as_program_fact,assignment_text_use_recovery,condition_text_use_recovery,return_text_use_recovery,match_text_use_recovery,destructure_text_use_recovery,iteration_text_use_recovery,simple_statement_text_use_recovery,collection_statement_text_use_recovery,backend_specific_mir_json_reader,raw_instruction_use_array_read,raw_phi_use_array_read,raw_expr0_backend_semantics,direct_mir_document_reindex,direct_backend_runtime_abi_hardcode,direct_cfg_unbound_air_certificate,direct_cfg_unverified_plan,direct_cfg_full_certificate_plan_retention,direct_cfg_backend_reparse,consumer_plan_revalidation,expression_graph_reconstruction,name_only_payload_declaration_join,reachable_zero_compensation_scaffold,typed_direct_rollback_bypass,missing_callable_binding_syntax_identity,forged_callable_binding_syntax_identity | tests/self_host_live_replacement_smoke.sh#hard_self_MIR_is_graph_owned_and_matches_the_explicit_C_oracle_bridge,tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh#indexed assignment reopened text use recovery,tests/self_hosted/parity/driver_rung2_return_graph_use_owner.sh#value returns are graph-owned,tests/self_hosted/parity/driver_rung2_match_graph_use_owner.sh#match subject uses are graph-owned,tests/self_hosted/parity/driver_rung2_destructure_graph_use_owner.sh#destructure initializer uses are graph-owned,tests/self_hosted/parity/driver_rung2_iteration_graph_use_owner.sh#iteration uses are graph-owned,tests/self_hosted/parity/driver_rung2_simple_statement_graph_use_owner.sh#Log/call/Exit uses are graph-owned,tests/self_hosted/parity/driver_rung2_collection_mutation_graph_use_owner.sh#collection receiver/value/index lanes are graph-owned,tests/self_hosted/parity/one_mir_dual_backend_projection.sh#hello + let_log + multilet one-MIR dual-backend gate ok,tests/self_hosted/parity/one_mir_inferred_generic_member_projection.sh#One self MIR drives nested inferred generic value-receiver calls through C/LLVM.,tests/self_hosted/parity/one_mir_mutable_nominal_identity_projection.sh#Subject and vessel literals share one stable mutable-identity owner family.,tests/self_hosted/parity/one_mir_constructed_generic_member_projection.sh#Produce source MIR once. Every target, permutation, and falsifier derives from it.,tests/self_hosted/parity/one_mir_cfg_air_plan_projection.sh#post_verification_plan_mutation, topology_specific_break_plan.,tests/self_hosted/parity/one_mir_scalar_cfg_break_exit_projection.sh#Producer-owned loop-exit phi is consumed once by the general scalar CFG plan.,tests/self_hosted/parity/one_mir_scalar_cfg_for_break_exit_projection.sh#producer-owned for header/exit phis project through one C/LLVM graph plan,tests/self_hosted/parity/one_mir_scalar_cfg_continue_backedge_projection.sh#continue and fallthrough snapshots drive exact 42 in C/LLVM,tests/self_hosted/parity/intent_typed_outcome_compensation_owner.sh#v3 zone identity + predecessor compensation + history parity: PASS,tests/self_hosted/parity/intent_execution_protocol_static_owner.py#intent plan/CFG/digest revalidation escaped admission,tests/self_hosted/parity/driver_rung2_callable_parameter_identity_owner.sh#callable identity C/LLVM runtime parity + exact negative ratchet: PASS | BRIDGE | selfhost_DRV2 branch/definition/return/log/index/indexed-assignment target/logical-not/numeric-negate/direct-identifier-call/simple-and-nested-member/field/instance-method/namespace-qualified-call/pipe/postfix-try/named-struct-literal/ordered-explicit-generic-actual/callee-and-numeric-cast/type-name graph carriage remains bounded; the bounded inferred and constructed-Option generic member slices seal nested targets, value receivers, inner-result-to-outer-argument edges, checked unwrap, and exact SSA uses into target-neutral plans; the nominal-literal slice seals one constructor, one member read and one SSA use before value/identity plan selection; routine local inventory projects semantic binding, initializer, and iteration facts, while statement and collection consumers use owner-directed graphs without text recovery; direct scalar admission reuses the machine-admitted document index, while bounded four-block `ifelse`/`if_else_assign`, three-block `reassign_block`, five-block `nestedif`, four-block single-header `whileloop`, phi-free integer `forloop`, and six-block `break_after_stmt` and eight-block `break_continue` paths derive CFG roles, assignment SSA definitions, predecessor-resolved phi incoming identities, nested branch/merge roles, loop header/backedge/exit roles, producer-owned break/normal exit phis, per-predecessor continue/fallthrough header snapshots, and Log/increment uses from typed owners; legacy non-range shapes bind once to MIR/CFG/phi/nested/while digests while range and break graphs use the general scalar CFG plan; direct-false shapes bind entry-carried or empty-forwarder lanes to merge edges without synthetic blocks; raw phi/use or expr0 semantics, AST/semantic reconstruction, a second document index, backend-specific MIR readers, repeated graph/certificate validation, a second plan, and local runtime symbol/format/type policy are negative-gated; the family remains BRIDGE because general range expressions, foreach, nested or multiple loops, multi-phi, and general CFG projection, declarations, cast target shape, checked float-to-integer materialization, unsupported native AST shapes, and backend compatibility inventory remain open
mir.generic_specialization | semantic | SourceCallIdentity | SFMirGenericSpecialization | SOMir | src/compiler/mir_generic_method_specialization.c | mir_generic_method_specializations_capture | src/codegen/transpiler_generic_method_specialization_emit.c,src/codegen/llvm_generic_method_specialization.c,src/codegen/transpiler_expr_call_member_emit.c,src/codegen/llvm_member_call_emit.c,src/self_hosted/mir_lower/generic_specialization_fact_owner.pgy,src/self_hosted/compiler/driver_rung2_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy,src/self_hosted/compiler/direct_mir_generic_specialization_fact_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_specialization_fact_owner.pgy,src/self_hosted/compiler/direct_mir_mixed_lane_generic_specialization_fact_owner.pgy,src/self_hosted/compiler/direct_mir_generic_member_signature_fact_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_member_specialization_fact_owner.pgy,src/self_hosted/compiler/direct_mir_generic_struct_value_flow_plan_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_nominal_plan_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_scalar_plan_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_member_plan_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_generic_member_signature_fact_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_generic_member_specialization_fact_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_generic_member_substitution_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_generic_member_program_identity_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_generic_member_plan_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_member_specialization_pair_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_member_variant_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_array_member_signature_fact_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_array_member_specialization_fact_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_array_member_substitution_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_array_member_program_identity_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_array_member_plan_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_record_array_member_specialization_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_record_array_member_plan_owner.pgy,src/self_hosted/compiler/direct_mir_aggregate_value_flow_fact_owner.pgy,src/self_hosted/compiler/direct_mir_aggregate_value_flow_target_projection_owner.pgy | backend_generic_argument_rescan,raw_generic_method_emit,missing_specialization_fallback,selfhost_mir_specialization_row_recovery,semantic_generic_specialization_overwrite,expression_graph_index_as_stable_identity,source_owner_syntax_id_numeric_literal | tests/generic_method_specialization_smoke.sh#MIR owner, C/LLVM consumers, runtime parity, and C missing-row mutation gate ok,tests/self_hosted/parity/one_mir_generic_struct_value_flow_projection.sh#One self MIR drives real C/LLVM generic specialization and nominal value flow.,tests/self_hosted/parity/one_mir_inferred_generic_nominal_projection.sh#One self MIR drives inferred generic Pair flow through real C and LLVM calls.,tests/self_hosted/parity/one_mir_inferred_generic_scalar_projection.sh#One self MIR drives inferred generic return/assignment through C and LLVM.,tests/self_hosted/parity/one_mir_inferred_generic_member_projection.sh#PASS: one owner path, class exact 41 plus vessel exact 42, C/LLVM receiver ABI parity, six order invariants, five variants, 81 C negatives, 9 LLVM sentinels,tests/self_hosted/parity/one_mir_passive_nominal_literal_projection.sh#PASS: tobject exact 12, real C/LLVM construction+read, subject/vessel identity split, 33 C negatives, 14 LLVM sentinels (sha256=,tests/self_hosted/parity/one_mir_mutable_nominal_identity_projection.sh#PASS: subject exact 7 + vessel exact 13, one mutable owner, 6 positive pairs, 33 C negatives, 14 LLVM sentinels,tests/self_hosted/parity/one_mir_constructed_generic_member_projection.sh#One self MIR carries Option<Int> through two heterogeneous member specializations.,tests/self_hosted/parity/one_mir_constructed_array_member_projection.sh#PASS: one MIR, caller-owned Array through two member specializations, C/LLVM exact 44, six invariants, three variants, 27 C negatives, 7 LLVM sentinels,tests/self_hosted/parity/one_mir_constructed_record_array_member_projection.sh#One self MIR carries a Point through Wrap<Point>, Array<Point>, index, and x. | BRIDGE | native MIR capture is the current oracle authority; bounded selfhost direct/member rows are a replacement projection and are not a second registry authority; the MIR JSON wire key is unified on generic_method_specializations and the retired generic_specializations key is negative-gated; native uses call SyntaxNodeId while selfhost uses owner SyntaxNodeId plus expression lane plus local call ordinal, and neither promotes a graph array index; exact generic-return nested inference and bounded explicit-four, inferred-two nominal direct, mixed-lane return/assignment direct, and exact-two inferred member projections are landed and target-specific SUBSTITUTING. The inferred source owner remains opaque because the graph carries no joinable stable call owner, so coherent owner renumber is metamorphic rather than a false negative; constructed Option, Array<Int>, and mixed-declaration Array<Point> member substitutions are target-specific SUBSTITUTING through one neutral pair and exclusive family projection; the topology-specific aggregate decisions are promoted into one representation-parameterized value-flow fact; passive vessel generic-member exact-42 is target-specific SUBSTITUTING through the shared member plan; passive class/object/tobject literal construction and field-read exact-12 and subject/vessel stable-identity exact-7/13 are target-specific SUBSTITUTING through one sealed route and shared admission with exclusive value/identity plans; identity convergence, native direct-row carriage, and broader host execution remain open, with ability_decl exact-7 compile-time declaration erasure as the active falsifier
air.evidence_graph | verification | EvidenceId | SFAirEvidenceGraph | SOAir | src/compiler/air.c | air_synthesize | src/compiler/air_verify.c,src/compiler/air_evidence_mir.c,src/compiler/air_evidence_certificate.c,src/compiler/air_verification_handle.h,src/compiler/air_validate.c,src/compiler/air_dump_json.c,src/compiler/compiler.c,src/compiler/compiler_llvm.c,src/compiler/c_runner.c,src/compiler/llvm_runner.c,src/compiler/driver_app.c | summary_counter_as_proof,backend_reads_AIR,missing_evidence_success,mir_inventory_rescan,mir_evidence_rebind | tests/air_mir_binding_smoke.sh#AIR owns a one-shot MIR evidence anchor and serialized fingerprint | BRIDGE | AIR consumes MIR routine inventory in one adapter pass for parameter, retain, machine, effect, cleanup, CFG, and pin evidence; a one-shot AIR-owned MIR binding anchor carries a deterministic input fingerprint, copied evidence lifetime, certificate binding, JSON visibility, and fail-closed rebind rejection. HIR/RIR/DAG evidence still share AIR as the final verification owner while their complete cross-stage lifetime migration remains open
air.mir_cfg_certificate | verification | MirCfgCertificateId | SFDirectMirCfgCertificate | SOAir | src/self_hosted/air/mir_cfg_certificate_owner.pgy | DirectMirCfgCertificateFromIndex | src/self_hosted/compiler/direct_mir_cfg_plan_owner.pgy | serialized_air_reparse,unbound_mir_certificate,certificate_fallback_or_drift,raw_phi_use_reparse,phi_predecessor_order_assumption,post_issue_identity_mutation | tests/self_hosted/parity/one_mir_cfg_air_plan_projection.sh#MIR-bound strict certificate and evidence mutations reject before output | CLOSED | none
projection.direct_mir_cfg_plan | projection | VerifiedProjectionPlanId | SFDirectMirCfgProjectionPlan | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_cfg_plan_owner.pgy | DirectMirCfgPlanFromAdmitted | src/self_hosted/compiler/direct_mir_backend_emission_owner.pgy | backend_mir_or_air_read,backend_specific_cfg_plan,full_certificate_plan_retention,second_shape_plan,unbound_target_fingerprint,post_verification_plan_mutation,topology_specific_break_plan | tests/self_hosted/parity/one_mir_cfg_air_plan_projection.sh#One admitted MIR identity -> one MIR-bound AIR/CFG plan -> both backends. | CLOSED | none
projection.direct_mir_string_array_push | projection | DirectMirStringArrayMutationPlanId | SFDirectMirStringArrayPushPlan | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_admission_owner.pgy | DirectMirScalarCfgStringArrayPlanFromOwners | src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_capacity_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_push_dominance_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_entry_execution_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_push_graph_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_c_storage_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_c_mutation_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_llvm_storage_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_llvm_mutation_emission_owner.pgy | empty_array_as_missing,push_order_from_instruction_id,future_push_count_as_current_length,capacity_as_length,backend_push_discovery,immutable_length_snapshot,entry_block_reentry,runtime_growth_claim_without_evidence,late_or_loop_push,native_codegen_fallback | tests/self_hosted/parity/one_mir_string_array_push_projection.sh#Empty Array<String> plus straight-line literal pushes owns C/LLVM mutation. | BRIDGE | bounded nonescaping local literal storage and once-only entry-prefix pushes only; dynamic values, aliases, return/parameter carriage, loop/branch mutation, reallocation, reserve/drop, and capacity observation remain open
projection.direct_mir_collection_pop_effect | projection | DirectMirCollectionPopProgramFactId | SFDirectMirCollectionPopEffect | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_scalar_cfg_collection_pop_program_fact_owner.pgy | DirectMirScalarCfgCollectionPopProgramFromOwners | src/self_hosted/compiler/direct_mir_scalar_cfg_collection_pop_program_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_collection_pop_typed_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_foreach_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_binding_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_mutation_length_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_c_operation_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_llvm_operation_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_pop_c_operation_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_pop_llvm_operation_owner.pgy | fixture_or_text_route,partial_int_only_success,partial_string_only_success,backend_mir_read,capacity_as_current_length,pretrimmed_source_storage,final_literal_length_store,incompatible_three_field_value_returning_pop_helper,cross_collection_receiver,stale_receiver,pop_result_invention,claimed_invalid_legacy_retry | tests/self_hosted/parity/one_mir_array_pop_projection.sh#bounded ArrayPop executes exact C/LLVM parity | BRIDGE | bounded nonescaping local literal Array<Int> with two entry pops before foreach and local literal Array<String> with one straight-line pop before length/index observations only; empty pop, aliases, parameters, returns, branch/loop mutation, mixed push-pop, arbitrary element types, reserve/drop, and ownership-sensitive String destruction remain open
projection.direct_mir_array_int_program | projection | DirectMirArrayIntProgramPlanId | SFDirectMirArrayIntProgram | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_admission_owner.pgy | DirectMirScalarCfgArrayIntProgramFromOwners | src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_transform_route_fact_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_static_mutation_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_foreach_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_binding_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_operation_binding_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_operation_absence_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_static_mutation_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_c_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_c_operation_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_c_operation_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_llvm_storage_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_llvm_operation_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_llvm_operation_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_llvm_emission_owner.pgy | array_int_as_scalar_local,string_push_plan_reuse,loop_effect_delta_as_mutation_authority,fixture_or_block_count_route,exact_admission_as_route_claim,backend_graph_reparse,prebaked_push_set_max_reverse_or_pop_values,capacity_as_current_length,runtime_collection_retry,incompatible_three_field_collection_helper,source_result_alias,in_place_reverse,duplicate_foreach_storage,store_after_length_advance,index_without_owned_length_guard,ownerless_array_operation,dynamic_then_static_mode_retry,static_then_read_only_mode_retry,read_only_then_reverse_mode_retry,reverse_then_pop_mode_retry,pop_then_legacy_mode_retry,duplicated_range_topology,unbound_phi_predecessor,stale_final_log,claimed_invalid_legacy_retry | tests/self_hosted/parity/one_mir_array_int_loop_push_projection.sh#Bounded dynamic Array<Int> push owns C/LLVM mutation, length, and read.,tests/self_hosted/parity/one_mir_array_int_sum_projection.sh#initialized sum and static set execute exact C/LLVM parity,tests/self_hosted/parity/one_mir_array_int_max_projection.sh#Read-only initialized Array<Int> range maximum shares the sealed GraphPlan.,tests/self_hosted/parity/one_mir_array_int_reverse_projection.sh#fresh ArrayReverse executes exact C/LLVM parity,tests/self_hosted/parity/one_mir_array_pop_projection.sh#bounded ArrayPop executes exact C/LLVM parity | BRIDGE | one receipt has mutually exclusive bounded dynamic-push, initialized-static-set, initialized-read-only-range, bounded fresh-reverse, and foreach-backed-pop modes over nonescaping local Array<Int> values; aliases, parameters, returns, unsealed lengths, multiple transforms, arbitrary predicates, branches around mutation, general growth/reallocation, reserve/drop, and arbitrary element expressions remain open
projection.direct_mir_scalar_cfg_graph_plan | projection | DirectMirScalarCfgGraphPlanId | SFDirectMirScalarCfgGraphPlan | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_scalar_cfg_graph_admission_owner.pgy | DirectMirScalarCfgGraphPlanFromAdmitted | src/self_hosted/compiler/direct_mir_scalar_cfg_graph_input_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_range_fact_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_range_set_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_range_bound_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_range_local_ref_identity_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_range_iteration_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_range_transfer_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_wire_local_ref_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_wire_range_scope_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_local_ref_identity_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_iteration_local_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_local_ref_plan_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_direct_local_operand_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_string_collection_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_collection_definition_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_array_length_fact_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_hash_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_local_inventory_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_fact_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_lookup_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_append_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_identity_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_graph_shape_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_dominance_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_index_safety_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_binding_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_graph_identity_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_range_graph_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_graph_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_graph_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_projection_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_range_block_effect_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_c_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_c_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_llvm_emission_owner.pgy | fixture_name_route,block_count_route,raw_expr0_semantics,phi_use_order_as_predecessor,merely_dominating_stale_value,unvalidated_assignment_target,break_backedge,backend_invented_exit_phi,reused_phi_incoming_slot,missing_latest_local_incoming,source_local_row_identity,name_first_local_binding,last_name_local_binding,backend_lexical_scope_recovery,missing_local_ref_success,orphan_local_ref_success,local_ref_grammar_reimplementation,range_owner_raw_local_ref_second_read,singular_range_receipt,receipt_array_position_identity,nested_continue_outer_header,nested_fallthrough_outer_header,outer_valueid_shadow_fallback,range_stop_expr0_fallback,branch_range_use_from_start_graph,array_capacity_as_length,backend_index_reconstruction,native_codegen_fallback,claimed_invalid_legacy_retry,indexed_string_legacy_retry,unsigned_index_without_nonnegative_proof,guard_true_edge_bypass,stale_index_row_read,generic_array_set_claim | tests/self_hosted/parity/one_mir_scalar_cfg_graph_projection.sh#one general scalar CFG plan owns exact C/LLVM execution and negatives,tests/self_hosted/parity/one_mir_scalar_cfg_break_exit_projection.sh#producer exit phi and repeated-slot general C/LLVM CFG route ok,tests/self_hosted/parity/one_mir_scalar_cfg_continue_backedge_projection.sh#Producer-captured continue and fallthrough snapshots bind one range header phi.,tests/self_hosted/parity/one_mir_iteration_binding_scope_owner.sh#one LocalRef plan preserves lexical scope in exact C/LLVM execution,tests/self_hosted/parity/one_mir_nested_iteration_binding_scope_owner.sh#Canonical receipt-set identity preserves nested same-spelling range binders.,tests/self_hosted/parity/one_mir_nested_iteration_continue_scope_owner.sh#innermost range owns continue and fallthrough transfers,tests/self_hosted/parity/one_mir_indexed_string_array_projection.sh#graph-owned ArrayLength/parts[i] executes exact C/LLVM parity and all stale/text/capacity paths fail closed,tests/self_hosted/parity/one_mir_string_array_mutation_projection.sh#one receipt executes exact C/LLVM while-read-static-set parity and all stale paths fail closed,tests/self_hosted/parity/public_nested_scalar_cfg_llvm_owner.sh#public file/stdout use installed scalar CFG LLVM through lexical LocalRefs | CLOSED | none
projection.direct_mir_scalar_cfg_foreach_receipt | projection | LoopSyntaxId | SFDirectMirScalarCfgForEachFact | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_admission_owner.pgy | DirectMirScalarCfgForEachFactsFromOwners | src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_fact_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_set_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_append_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_element_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_collection_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_collection_owner.pgy,src/self_hosted/compiler/direct_mir_returned_array_foreach_call_abi_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_string_collection_owner.pgy,src/self_hosted/compiler/direct_mir_array_int_producer_fact_owner.pgy,src/self_hosted/compiler/direct_mir_returned_array_foreach_program_owner.pgy,src/self_hosted/compiler/direct_mir_returned_array_foreach_projection_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_local_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_graph_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_graph_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_c_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_typed_c_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_llvm_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_typed_llvm_emission_owner.pgy | typed_iterable_mismatch_as_range,backend_collection_protocol_reconstruction,backend_element_type_reconstruction,raw_expr0_array_read,call_text_reparse,repeated_return_collection_materialization,range_owner_foreign_local_ref_claim,capacity_as_length,hardcoded_element_count,collection_in_scalar_local_inventory,block_count_route,source_local_type_route,claimed_invalid_legacy_retry,int_literal_post_parse_wrap_admission,hoisted_call_abi_ignored,hoisted_call_as_local_literal | tests/self_hosted/parity/one_mir_scalar_cfg_foreach_array_int_projection.sh#Array<Int> foreach receipt owns C/LLVM execution and rejects fallback,tests/self_hosted/parity/one_mir_returned_array_foreach_projection.sh#One returned Array<Int> producer receipt feeds nested/sequential foreach CFG.,tests/self_hosted/parity/one_mir_mixed_collection_foreach_projection.sh#One typed scalar-CFG plan executes sequential Int and String foreach loops. | CLOSED | none
air.mir_option_match_cfg_certificate | verification | MirCfgCertificateId | SFDirectMirOptionMatchCfgCertificate | SOAir | src/self_hosted/air/mir_option_match_cfg_certificate_fact_owner.pgy | DirectMirOptionMatchCfgCertificateFactFromOwners | src/self_hosted/compiler/direct_mir_option_match_cfg_plan_owner.pgy | raw_match_text_recovery,certificate_raw_json_reopen,raw_use_array_backend_read,backend_specific_option_certificate,unbound_abi_layout,successor_default,pattern_or_binding_fallback,post_issue_certificate_mutation | tests/self_hosted/parity/one_mir_option_match_projection.sh#Option<Int> match direct C/LLVM parity and seven mutations are fail-closed (sha256= | CLOSED | none
projection.direct_mir_option_match_cfg_plan | projection | VerifiedProjectionPlanId | SFDirectMirOptionMatchCfgPlan | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_option_match_cfg_plan_owner.pgy | DirectMirOptionMatchCfgPlanFromAdmitted | src/self_hosted/compiler/direct_mir_option_match_cfg_emission_owner.pgy | backend_mir_or_air_read,backend_specific_option_plan,unbound_target_fingerprint,unverified_abi_projection,both_backend_mappings_in_one_receipt,runtime_symbol_guess,native_codegen_fallback,post_issue_plan_mutation | tests/self_hosted/parity/one_mir_option_match_projection.sh#One admitted Option<Int> match CFG drives direct C and textual LLVM. | CLOSED | none
abi.mir_option_match_layout_admission | abi | LayoutId | SFAbiOptionMatchLayoutAdmission | SOMirAbi | src/self_hosted/mir_lower/abi_layout_admission_fact_owner.pgy | MirCapturedRequiredAbiLayoutRowAdmission | src/self_hosted/compiler/direct_mir_option_match_cfg_plan_owner.pgy,src/self_hosted/compiler/direct_mir_option_match_abi_capture_owner.pgy,src/self_hosted/compiler/direct_mir_option_match_abi_fact_owner.pgy,src/self_hosted/compiler/direct_mir_option_match_abi_projection_owner.pgy | backend_local_option_layout,layout_id_without_reconstructible_row,tag_or_offset_guess,duplicate_layout_parse,post_issue_layout_identity_mutation | tests/self_hosted/parity/one_mir_option_match_projection.sh#Option<Int> match direct C/LLVM parity and seven mutations are fail-closed (sha256= | CLOSED | none
abi.layout_rows | abi | LayoutId | SFAbiLayoutRows | SOMirAbi | src/compiler/mir_abi_layout.c | mir_abi_lookup | src/compiler/mir_json_dump.c,src/codegen/transpiler_entry.c,src/codegen/llvm_api.c,src/compiler/mir_fact_surface_validate.c,src/codegen/transpiler_mir_resource_op_core.c,src/compiler/mir_nominal_abi_layout.c,src/compiler/mir_signature_metadata.c,src/compiler/mir_call_fact.c,src/compiler/mir_hir_block_projection.c,src/compiler/mir_stmt_population_source.c,src/self_hosted/mir/nominal_abi_layout_fact_owner.pgy,src/self_hosted/mir/nominal_abi_layout_json_projection_owner.pgy,src/self_hosted/mir/option_nominal_abi_layout_fact_owner.pgy,src/self_hosted/mir/option_nominal_abi_layout_verify_owner.pgy,src/self_hosted/mir/option_nominal_abi_layout_json_projection_owner.pgy,src/self_hosted/mir/routine_param_json_projection_owner.pgy,src/self_hosted/mir/instruction_abi_receipt_fact_owner.pgy,src/self_hosted/mir/instruction_abi_receipt_verify_owner.pgy,src/self_hosted/mir/instruction_abi_receipt_json_projection_owner.pgy,src/self_hosted/mir_lower/abi_layout_fact_owner.pgy,src/self_hosted/compiler/direct_mir_nominal_declaration_abi_fact_owner.pgy,src/self_hosted/compiler/direct_mir_two_int_nominal_abi_shape_owner.pgy,src/self_hosted/compiler/direct_mir_instruction_abi_absence_owner.pgy,src/self_hosted/compiler/direct_mir_struct_argument_program_identity_owner.pgy,src/self_hosted/compiler/direct_mir_struct_argument_plan_owner.pgy,src/self_hosted/compiler/direct_mir_struct_value_flow_abi_fact_owner.pgy,src/self_hosted/compiler/direct_mir_struct_value_flow_plan_owner.pgy,src/self_hosted/compiler/direct_mir_option_struct_value_flow_abi_fact_owner.pgy,src/self_hosted/compiler/direct_mir_option_struct_value_flow_plan_owner.pgy,src/self_hosted/compiler/direct_mir_generic_struct_value_flow_abi_fact_owner.pgy,src/self_hosted/compiler/direct_mir_generic_struct_value_flow_plan_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_nominal_abi_fact_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_nominal_plan_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_scalar_abi_fact_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_scalar_plan_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_member_declaration_fact_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_member_representation_fact_owner.pgy,src/self_hosted/compiler/direct_mir_inferred_generic_member_plan_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_generic_member_option_abi_admission_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_generic_member_representation_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_generic_member_plan_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_array_member_abi_admission_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_array_member_representation_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_array_member_plan_owner.pgy,src/self_hosted/compiler/direct_mir_array_storage_layout_contract_owner.pgy,src/self_hosted/compiler/direct_mir_array_storage_abi_projection_owner.pgy,src/self_hosted/compiler/direct_mir_array_storage_c_assertion_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_record_array_member_point_abi_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_record_array_member_abi_projection_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_record_array_member_plan_owner.pgy,src/self_hosted/compiler/direct_mir_constructed_record_array_member_array_abi_absence_owner.pgy,src/self_hosted/compiler/direct_mir_nominal_literal_abi_absence_owner.pgy,src/self_hosted/compiler/direct_mir_passive_nominal_literal_plan_owner.pgy,src/self_hosted/compiler/direct_mir_mutable_nominal_identity_plan_owner.pgy,src/self_hosted/compiler/direct_mir_mutable_nominal_identity_target_projection_owner.pgy,src/self_hosted/compiler/direct_mir_aggregate_value_flow_fact_owner.pgy,src/self_hosted/compiler/direct_mir_aggregate_value_flow_target_projection_owner.pgy | backend_local_layout_guess,implicit_option_niche,target_default_guess,mir_active_abi_type_lookup,transpiler_mir_layout_from_type_annotation,active_view_owner_inventory_scan,selfhost_static_layout_row_recovery,backend_nominal_offset_guess,type_spelling_layout_recovery,missing_value_parameter_receipt_success,layout_id_without_exact_declaration_row,producer_local_syntax_id_numeric_equality,struct_candidate_array_retry,aggregate_return_or_local_definition_layout_recovery,nested_option_nominal_payload_layout_recovery,option_struct_candidate_plain_struct_retry,nonrequired_nominal_promoted_to_physical_receipt,typed_scalar_no_layout_erases_type_identity,generic_nominal_layout_from_specialization_text,family_direct_array_storage_layout_import,C_reserved_double_underscore_hidden_storage,source_storage_symbol_collision | tests/abi_ownership_shape_smoke.sh#mir_abi_layout_id,tests/backend_fail_closed_smoke.sh#active C MIR view hooks must consume carried owner facts,Makefile#self-host-mir-abi-first-test-smoke,tests/self_hosted/parity/one_mir_struct_argument_projection.sh#PASS: one MIR, nominal ABI cross-seal, C/LLVM exact 6, permutations, 15 negatives,tests/self_hosted/parity/one_mir_struct_value_flow_projection.sh#PASS: one MIR, nominal return/local ABI cross-seal, C/LLVM exact 11, routine permutation, 13 negatives,tests/self_hosted/parity/one_mir_option_struct_value_flow_projection.sh#PASS: one MIR, Option<Pair> ABI/provenance cross-seal, C/LLVM exact 7/11/5, routine permutation, 22 negatives,tests/self_hosted/parity/one_mir_generic_struct_value_flow_projection.sh#PASS: one MIR, real Identity<Int> C/LLVM calls, exact 7, two permutations, 29 negatives,tests/self_hosted/parity/one_mir_inferred_generic_nominal_projection.sh#PASS: one MIR, inferred Identity<Int> C/LLVM calls, exact 42, four metamorphics, 32 negatives,tests/self_hosted/parity/one_mir_inferred_generic_scalar_projection.sh#PASS: one MIR, mixed-lane inference, C/LLVM exact 41, five metamorphics, two value variants, 55 C negatives, 3 LLVM sentinels,tests/self_hosted/parity/one_mir_inferred_generic_member_projection.sh#C/LLVM receiver ABI parity,tests/self_hosted/parity/one_mir_passive_nominal_literal_projection.sh#PASS: tobject exact 12,tests/self_hosted/parity/one_mir_mutable_nominal_identity_projection.sh#Subject and vessel literals share one stable mutable-identity owner family,tests/self_hosted/parity/one_mir_constructed_generic_member_projection.sh#PASS: one MIR, two heterogeneous member specializations, C/LLVM exact 43,tests/self_hosted/parity/one_mir_constructed_array_member_projection.sh#PASS: one MIR, c,tests/self_hosted/parity/one_mir_constructed_record_array_member_projection.sh#PASS: one MIR, Point,Makefile#self-host-codegen-bootstrap-seed-test-smoke | BRIDGE | mir_abi_lookup owns static rows and mir_abi_layout_id owns stable content identity; native MIR JSON carries complete static rows and selfhost mir_lower rejects missing or mismatched IDs; selfhost producer materializes fixed scalar Slot/DeviceSlot/SecureSlot rows, bounded Array rows, program-owned fixed scalar/nested-value nominal struct receipts, and derived Option-of-nominal wrappers from one static tag contract plus one inner declaration receipt. The bounded Line parameter, Pair return/local, Option<Pair> return/local, explicit-generic Pair, inferred-generic Pair, mixed-lane inferred scalar return/assignment, and inferred generic member flows are target-specific SUBSTITUTING through installed C and LLVM; the member class and vessel carry distinct internal value and mutable-identity representation cross-seals and invent no physical ABI receipt; instruction-owned receipts keep JSON projection and backends from reopening expression text or declaration scans, while native residual assignment and self SSA remain representation-local. A nominal declaration with required=0 retains the neutral kind-0,row-minus-one,id-0 receipt; a typed scalar without physical layout retains its exact type with id zero, required false, and null layout; the shared nominal-literal admission now seals that typed absence to the actual definition-capture digest before exclusive passive-value or stable-identity projection; the mutable nominal identity plan binds one compiler-private storage identity to the same capture without inventing a physical ABI receipt; a statement receipt retains null type/layout with id zero and required false. Other pointer-bearing, unknown, target-dependent, broader nested wrapper, and general generic rows remain open; the bounded passive vessel pointer receiver is target-specific SUBSTITUTING; constructed Option, Array<Int>, and mixed-declaration Array<Point> member returns are target-specific SUBSTITUTING. Public four-field Array storage is separate from private three-field self-host compiler containers and from the closed-module call-classification receipt. DirectMirArrayStorageAbiProjection now owns the shared target-bound four-field mapping consumed by both substituting families, both C emitters publish the same six layout assertions, and collision-owned _pgy_array_storage_N avoids the reserved C __* namespace; the target-neutral aggregate value-flow promotion is closed while preserving captured physical Array versus typed-absence provenance; external target-profile interoperability remains open
abi.runtime_call_rows | abi | RuntimeCallAbiId | SFAbiRuntimeCallRows | SOMirAbi | src/compiler/mir_abi_resource_runtime.c | mir_abi_resource_runtime_row_for_type_name | src/compiler/mir_lower_population.c,src/compiler/mir_fact_surface_validate.c,src/compiler/mir_decl_field_claim_abi.c,src/codegen/transpiler_slot_runtime_row.c,src/codegen/transpiler_class_decl_emit.c,src/codegen/llvm_runtime.c,src/codegen/llvm_runtime_row.c,src/compiler/mir_abi_resource_runtime_constructed.c,src/compiler/mir_abi_resource_runtime_mir.c,src/self_hosted/mir/routine_build_owner.pgy,src/self_hosted/mir_lower/resource_runtime_abi_fact_owner.pgy,src/self_hosted/compiler/runtime_call_abi_row_manifest.pgy,src/self_hosted/compiler/runtime_call_abi_structured_fact_owner.pgy,src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy,src/self_hosted/compiler/direct_mir_backend_emission_owner.pgy | mir_abi_resource_runtime_row_by_kind(,mir_abi_resource_runtime_fn_by_kind(,class_field_claim_type_recovery,claim_expr1_type_recovery,runtime_resource_expr_graph_receiver_type_recovery,serialized_runtime_row_reparse,direct_backend_runtime_symbol_guess | tests/abi_ownership_shape_smoke.sh#MIRResourceRuntimeRow,tests/self_hosted/parity/runtime_call_abi_row_manifest_parity.sh#parity ok,tests/self_hosted/parity/driver_rung2_machine_mir_parity_owner.sh#Claim result-local and resource receiver graph owner,tests/self_hosted/parity/one_mir_dual_backend_projection.sh#One admitted graph directly drives C and LLVM; expected output is pinned,tests/self_hosted/parity/one_mir_cfg_air_plan_projection.sh#typed string line format drives both CFG emitters,tests/backend_fail_closed_smoke.sh#transpiler_slot_runtime_fn_for_decl_claim( | BRIDGE | routine instructions and declaration field claims carry owner-derived runtime rows; Claim projects the result SSA binding's routine-local type and non-Claim resource calls project the receiver from the attached expression graph; the named string formatted-print row input now projects one typed symbol/materialization/call-shape/stable-ID fact consumed by both direct backends without parsing the serialized row; missing and unknown facts fail closed; constructed nominal materialization and full C/LLVM/self-host compatibility promotion remain open
abi.intent_observability_rows | abi | RuntimeCallAbiId | SFIntentObservabilityAbiRows | SOIntentObservabilityAbi | src/common/intent_observability_abi.def | PGY_INTENT_OBSERVABILITY_ABI_ROWS_OWNER | src/common/intent_observability_abi.c,src/semantic/type_checker_builtins_intent_observability.c,src/codegen/transpiler_intent_observability_builtin_emit.c,src/codegen/llvm_expr_intent_observability_calls.c,src/self_hosted/lib/intent_observability_abi_projection_owner.pgy,src/self_hosted/semantic/builtin_signature_owner.pgy,src/self_hosted/semantic/ast_expression_identity_resolution_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_builtin_signature_projection_owner.pgy,src/self_hosted/compiler/direct_mir_nested_intent_program_graph_fact_owner.pgy,src/self_hosted/compiler/direct_mir_nested_intent_program_c_emission_owner.pgy,src/self_hosted/compiler/direct_mir_nested_intent_program_llvm_emission_owner.pgy,src/self_hosted/compiler/direct_mir_composite_intent_program_plan_owner.pgy,src/self_hosted/compiler/direct_mir_composite_intent_program_llvm_emission_owner.pgy | native_literal_observability_table,selfhost_literal_observability_signature_table,stable_id_from_sorted_index,argument_kind_from_count,parallel_scalar_observability_projection,stale_selfhost_projection,unknown_observability_success,mir_runtime_call_abi_name_reconstruction,native_backend_source_name_abi_lookup,emitter_source_name_abi_lookup,missing_carried_runtime_call_abi_success,valid_foreign_runtime_call_abi_success,forged_runtime_call_abi_success | tests/intent_observability_abi_registry_smoke.sh#51 native/self-host registry rows plus carried-ID lookup/cross-seal and backend old-lookup negatives,tests/self_hosted/parity/intent_observability_installed_self_host_owner.sh#public installed/native C/LLVM consume zero-, one-, and two-argument registry rows without native re-entry,tests/self_hosted/parity/intent_observability_mir_identity_owner.sh#native/self MIR -> direct C/LLVM stable ABI identity PASS,tests/self_hosted/parity/direct_mir_nested_intent_program_c_owner.sh#LLVM parity plus byte-identical source/direct C, exact runtime, and twelve no-artifact negatives: PASS,tests/self_hosted/parity/direct_mir_composite_intent_program_llvm_owner.sh#success/failure parity and five no-artifact negatives: PASS,tests/self_hosted/parity/intent_guard_post_compensation_execution_owner.sh#guard/expect/post failure + ordered compensation + history parity,tests/self_hosted/parity/intent_typed_outcome_compensation_owner.sh#v3 zone identity + predecessor compensation + history parity: PASS,tests/verified_projection_plan_smoke.sh#ABI rows must remain source-name sorted | CLOSED | none
resource.region_allocation_plan | resource | AllocationSiteId | SFRegionAllocationPlan | SORegionPlan | src/semantic/region_escape_fact.c | semantic_region_escape_collect | src/compiler/hir_region_escape_facts.c,src/compiler/hir_semantic_fact_projection.c,src/compiler/mir_region_escape_facts.c,src/compiler/verified_region_plan.c,src/compiler/driver_app.c,src/codegen/transpiler_context.c,src/codegen/llvm_expr_scalar_core.c,src/self_hosted/compiler/region_plan_owner.pgy | backend_ast_region_safety_guess,allocation_site_pointer_identity,missing_region_plan_success,compiler_region_escape_producer_recovery,driver_semantic_region_escape_read,driver_hir_region_escape_read | tests/region_escape_unit.c#semantic_region_escape_collect,src/test_hir.c#HIR carries semantic region rows,src/test_mir.c#MIR carries HIR region escape facts,tests/region_plan_unit.c#pgy_verified_region_plan_lookup(,tests/region_backend_wiring_smoke.sh#builtin and direct user-callee sinks are region-backed | BRIDGE | semantic analysis remains the fact owner; HIR and MIR own successive projected stable rows; driver consumes the MIR carrier before AIR-gated plan materialization; C/LLVM/self-host consumers validate stable AllocationSiteId rows; builtin and direct user-callee retention summaries are owner-directed and fail closed; complete region allocation ownership remains open
target.capability_profile | target | TargetProfileId | SFTargetCapabilityProfile | SOTargetCapability | src/self_hosted/compiler/target_capability_owner.pgy | CompilerTargetCapabilityEnvelopeReady | src/compiler/verified_projection_plan.c,src/self_hosted/compiler/target_projection_fact_owner.pgy,src/self_hosted/compiler/direct_mir_cfg_plan_owner.pgy,src/self_hosted/codegen/emission/program_entry_owner.pgy | backend_target_default,compiler_command_guess,projection_fingerprint_recovery | tests/target_capability_projection_smoke.sh#planner is the sole native target-envelope consumer,tests/self_hosted/parity/target_capability_manifest_parity.sh#target_fingerprint_mutation_rejected,tests/self_hosted/parity/driver_rung2_target_projection_negative_owner.sh#self-host C emission target projection fact is missing or invalid,tests/self_hosted/parity/one_mir_cfg_air_plan_projection.sh#direct CFG plan target mutation rejects before output | BRIDGE | CompilerTargetCapabilityFingerprint is a bounded carriage digest derived under the canonical envelope producer; native planner remains the sole full-width target-envelope consumer; the direct CFG plan now binds the same global fingerprint before C/LLVM emission; concrete size/alignment/endian values, object format, full AIR references, and CompilerEmissionArtifact plan-digest carriage remain open
projection.verified_plan | projection | ProjectionPlanId | SFProjectionPlan | SOProjectionPlanner | src/compiler/verified_projection_plan.c | pgy_verified_projection_plan_intent_observability_with_air | src/codegen/transpiler_entry.c,src/codegen/llvm_api.c | backend_materialization_guess,AST_HIR_usage_inference,MIR_only_plan_without_AIR_certificate,unbound_machine_manifest | tests/verified_projection_plan_smoke.sh#owner facts changed after verification | BRIDGE | first_intent_observability_row_is_AIR_certificate_and_target-envelope plus machine-manifest fingerprint bound; typed_layout_cleanup_materialization_artifact_and_selfhost rows remain open
diagnostic.catalog | diagnostic | DiagnosticId | SFDiagnosticCatalog | SODiagnosticCatalog | src/semantic/diag_codes.h | PGY_CODE_SEM_TYPE_MISMATCH | src/semantic/type_checker_diag.c,src/compiler/driver_diag.c | free_text_code_recovery,stage_guess_from_message | tests/diagnostic_registry_smoke.sh#macros and call sites ok | BRIDGE | driver_still_maps_some_free_text_messages_to_codes
artifact.zone | artifact | ArtifactId | SFBackendArtifact | SOArtifactZone | src/self_hosted/compiler/artifact_zone_owner.pgy | CompilerArtifactZoneReady | src/compiler/compiler.c,src/compiler/compiler_llvm.c,tests/self_hosted/parity/backend_output_comparator_parity.sh | path_as_artifact_identity,backend_output_without_plan_digest | tests/artifact_zone_plan_identity_smoke.sh#native_C_LLVM_artifact_plan_identity | CLOSED | none
selfhost.compiler_artifact_commit | artifact | ArtifactTransactionHandle | SFCompilerArtifactCommit | SOCompilerArtifactCommit | src/self_hosted/mir/artifact_transaction_owner.pgy | SelfMirArtifactCommit | src/self_hosted/mir/program_json_artifact_writer_owner.pgy,src/self_hosted/compiler/driver_rung2_execution_owner.pgy,src/self_hosted/compiler/driver_source_mir_execution_owner.pgy,src/self_hosted/compiler/driver_source_c_execution_owner.pgy,src/self_hosted/compiler/driver_bootstrap_main.pgy,src/self_hosted/compiler/driver_cli_owner.pgy,src/runtime/pgy_runtime_artifact_transaction_core.h | raw_final_writer,duplicate_backend_transaction_algorithm,commit_without_typed_receipt,second_whole_graph_validation,crash_durability_without_sync,failure_tag_only,outcome_bool_collapse_before_last_consumer,receipt_as_authority_or_projection_source,unknown_failure_status,known_wrong_target_projection,source_mir_main_direct_commit,source_mir_file_helper_fallback,source_c_installed_direct_compile_commit,mir_c_installed_direct_compile_commit | tests/artifact_atomic_transaction_contract_smoke.sh#one runtime owner; typed receipt; no raw final writer; single production validation,tests/runtime_artifact_atomic_transaction_smoke.sh#success and open/write/flush/close/publish/cleanup failures preserve final and clean temp,tests/self_hosted/parity/driver_rung2_fallible_tobject_outcome_owner.sh#success receipt + exact begin failure payload: PASS,tests/self_hosted/parity/driver_source_mir_execution_action_gate.sh#artifact+stdout source-MIR world/action/pressure/commit ratchet PASS,tests/self_hosted/parity/driver_source_c_execution_action_gate.sh#world/action artifact parity, execution, and transaction rejection: PASS,tests/self_hosted/parity/driver_source_c_stdout_execution_action_gate.sh#default/explicit raw byte parity, normalized artifact payload parity, admitted manifest, and typed invalid-manifest rejection: PASS,tests/self_hosted/parity/installed_driver_cli_mode_owner.sh#general MIR-C world/action artifact parity, pressure observation, and transaction rejection: PASS | CLOSED | none
compatibility.evolution | compatibility | CompatibilitySurfaceId | SFCompatibilityEvolution | SOCompatibilityEvolution | src/self_hosted/compiler/compatibility_evolution_owner.pgy | CompilerCompatibilityEvolutionReady | src/self_hosted/tools/compatibility_evolution_checker/report_owner.pgy,src/compiler/driver_diag.c,src/compiler/driver_app.c | local_compatibility_list,warning_without_migration_metadata | tests/self_hosted/parity/compatibility_evolution_manifest_parity.sh#parity ok | BRIDGE | native driver now validates the nine-row self-host manifest; diagnostic ABI trace and package-gate consumers still need direct row carriage
selfhost.expression_graph | syntax | SyntaxNodeId | SFExpressionGraph | SOParserExpressionGraph | src/self_hosted/parser/expression_graph_owner.pgy | ParserExpressionGraphsAppend | src/self_hosted/codegen/input/semantic_expression_codegen_view_owner.pgy,src/self_hosted/codegen/emission/try_let_emit_owner.pgy,src/self_hosted/codegen/emission/stmt_emit.pgy,src/self_hosted/mir/expression_graph_fact_owner.pgy,src/self_hosted/mir/program_fact_owner.pgy,src/self_hosted/mir/json_projection_owner.pgy,src/self_hosted/mir/program_verify_owner.pgy,src/self_hosted/semantic/ast_expression_carried_callable_identity_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_identity_bound_call_emit_owner.pgy | initializer_array_bodies,SemanticAstLocalBindingArrayLiteralBodyAt(,semantic_array_literal_codegen_view_owner.pgy,initializer_try_operands,SemanticAstLocalBindingTryOperandAt(,SemanticTryOperand(,SelfMirExpressionGraphRows.graph,SemanticExpressionGraphFactsEqual(,SelfMirExpressionGraphRows.node_kinds,SelfMirExpressionGraphRows.node_texts,SelfMirExpressionGraphRows.left_children,SelfMirExpressionGraphRows.right_children,SelfMirExpressionGraphRows.call_target_kinds,SelfMirExpressionGraphRows.call_target_names | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok,tests/self_hosted/parity/mir_expression_graph_projection_owner_smoke.sh#MIR projection carries semantic graph handles,tests/self_hosted/parity/driver_rung2_callable_parameter_identity_owner.sh#callable identity C/LLVM runtime parity + exact negative ratchet: PASS | CLOSED | none
selfhost.set_literal_runtime_surface | semantic | SyntaxNodeId | SFSetLiteralRuntimeSurface | SOSetLiteralRuntime | src/self_hosted/semantic/ast_expression_graph_set_literal_owner.pgy | SemanticExpressionGraphSetLiteralMatchesDeclaredType | src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy,src/self_hosted/semantic/ast_expression_verdict_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_composite_literal_emit_owner.pgy,src/self_hosted/mir/expression_graph_kind_name_owner.pgy,src/self_hosted/mir_lower/expression_graph_sequence_owner.pgy | set_literal_as_array_or_struct_fallback,set_element_type_guess,source_set_spelling_as_abi,untyped_empty_set_success,missing_set_runtime_fact_success | tests/self_hosted/parity/driver_rung2_set_literal_parity_owner.sh#Set literal graph/runtime ABI parity and fail-closed empty/type/missing-row negatives | CLOSED | none
selfhost.collection_mutation_statement | semantic | SyntaxNodeId | SFCollectionMutationStatement | SOSemanticStatement | src/self_hosted/semantic/ast_statement_fact_owner.pgy | SemanticAstStatementFacts | src/self_hosted/codegen/input/semantic_statement_codegen_view_owner.pgy | TypedAstArenaAtomText,TypedAstArenaValueText,TypedAstArenaAuxValueText,ast_text_collection_stmt_owner.pgy,StringTrim( | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.enum_declaration_rows | semantic | SyntaxNodeId | SFEnumDeclarationRows | SOSemanticEnum | src/self_hosted/semantic/ast_enum_fact_owner.pgy | SemanticAstEnumFacts | src/self_hosted/semantic/ast_expression_graph_enum_payload_owner.pgy,src/self_hosted/semantic/ast_expression_environment_owner.pgy,src/self_hosted/semantic/enum_callable_signature_owner.pgy,src/self_hosted/hir/ast_match_pattern_fact_owner.pgy,src/self_hosted/mir/declaration_rows_owner.pgy,src/self_hosted/mir/enum_declaration_verify_owner.pgy,src/self_hosted/mir/routine_match_owner.pgy,src/self_hosted/mir/json_projection_owner.pgy,src/self_hosted/mir_lower/decl_lower.pgy,src/self_hosted/mir_lower/expression_graph_tagged_enum_match_owner.pgy,src/self_hosted/mir_lower/program_enum_variant_index_owner.pgy,src/self_hosted/compiler/direct_mir_enum_value_match_plan_owner.pgy,src/self_hosted/compiler/direct_mir_enum_value_match_plan_fact_owner.pgy,src/self_hosted/codegen/input/semantic_enum_codegen_view_owner.pgy,src/self_hosted/codegen/emission/enum_emit_owner.pgy,src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy,src/self_hosted/codegen/emission/type_declaration_emit_owner.pgy,src/self_hosted/codegen/emission/program_emit.pgy,src/codegen/transpiler_type_decl_schedule.c,src/self_hosted/codegen/emission/tagged_enum_match_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_graph_emit_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy,src/self_hosted/compiler/driver_rung2_owner.pgy | TypedAstArenaAuxValueText,ExprSequenceItemCount,ExprSequenceItemAt,ast_text_enum_variant_owner.pgy,CodegenAstArenaEnumNameOrDie,CodegenAstArenaEnumVariantCount,CodegenAstArenaEnumVariantNameAt,payload enum variants are not supported,binding: String;,rows.binding_counts[i] > 1,tagged enum match binding requires exactly one payload,"CollectEnums(semantic_analysis.enums, env_acc)",OptionResultRuntimeCStructOptionDefinitionBlock,option_struct_block,lightweight_checker_source_enum_projection,direct_mir_enum_value_match_retry,direct_mir_enum_runtime_aggregate | tests/self_hosted_component_contract_smoke.sh#CodegenSemanticEnumVariantPayloadTypeAtOrDie,tests/self_hosted/parity/mir_json_parity.sh#enum_multi_payload: missing ordered payload owner fact,tests/self_hosted/parity/driver_rung2_match_parity_owner.sh#incomplete ordered enum binding types were accepted,tests/self_hosted/parity/codegen_parity.sh#tagged enum equality,tests/self_hosted/parity/one_mir_enum_value_match_projection.sh#One payload-free enum value and real match CFG feed both selected backends.,tests/self_hosted_component_contract_smoke.sh#SemanticEnumCallableProjection | BRIDGE | typed AST enum consumers are closed; the lightweight semantic checker still receives a concatenated source bundle and projects bounded callable rows until that entrypoint consumes parser-owned enum facts; the installed direct-MIR slice now SUBSTITUTES one payload-free ordinal value and identity-match CFG through the shared plan, while payload enums and general enum execution remain open
selfhost.nominal_declaration_rows | semantic | SyntaxNodeId | SFNominalDeclarationRows | SOSemanticNominalConstructor | src/self_hosted/semantic/ast_nominal_constructor_fact_owner.pgy | SemanticAstNominalConstructorFacts | src/self_hosted/codegen/input/semantic_nominal_codegen_view_owner.pgy,src/self_hosted/codegen/emission/nominal_struct_emit_owner.pgy,src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy,src/self_hosted/codegen/emission/type_declaration_emit_owner.pgy,src/self_hosted/codegen/emission/program_emit.pgy,src/codegen/transpiler_type_decl_schedule.c | ast_text_declaration_owner.pgy,CodegenAstArenaNominalNameOrDie,CodegenAstArenaFieldNameOrDie,CodegenAstArenaFieldTypeNameOrDie,CodegenAstArenaIsNominalDecl,"CodegenAstArenaIsFieldsHeader(arena, j)","CollectStructs(semantic_analysis.constructors, env_acc)",OptionResultRuntimeCStructOptionDefinitionBlock,option_struct_block | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok,tests/self_hosted_component_contract_smoke.sh#CollectTypeDeclarations( | CLOSED | none
selfhost.role_declaration_rows | semantic | SyntaxNodeId | SFRoleDeclarationRows | SOSemanticRole | src/self_hosted/semantic/ast_role_fact_owner.pgy | SemanticAstRoleFacts | src/self_hosted/codegen/input/semantic_role_codegen_view_owner.pgy,src/self_hosted/codegen/emission/function_emit.pgy,src/self_hosted/codegen/emission/program_emit.pgy | ast_text_role_declaration_owner.pgy,CodegenAstArenaRoleNameOrDie,CodegenAstArenaRoleTargetTypeNameOrDie,CodegenAstArenaIsRoleDecl,"CodegenAstArenaIsDescendantOf(arena, j, i)" | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.ability_generic_bounds | semantic | SyntaxNodeIdGenericParameterIndex | SFAbilityGenericBounds | SOSemanticRole | src/self_hosted/semantic/ast_role_fact_owner.pgy | SemanticAstAbilityGenericConstraintRowsFromNode | src/self_hosted/semantic/ast_ability_generic_bound_verdict_owner.pgy,src/self_hosted/semantic/ast_artifact_verdict_owner.pgy,src/self_hosted/mir/declaration_rows_owner.pgy,src/self_hosted/mir_lower/decl_lower.pgy,src/compiler/mir_decl_header_generic_metadata.c,src/self_hosted/codegen/emission/role_dispatch_emit_owner.pgy,src/self_hosted/codegen/emission/program_emit.pgy,src/self_hosted/compiler/driver_rung2_owner.pgy | generic_params,missing generic bound was accepted,SemanticAstAbilityGenericBoundVerdict,generic multi-bound fact drifted,where T: Comparable + Cloneable | tests/self_hosted/parity/driver_rung2_generic_multi_bound_defaults_parity_owner.sh#Owns ordered ability-bound/default carriage and its fail-closed falsifier. | CLOSED | none
selfhost.ability_bind_statement | semantic | SyntaxNodeId | SFAbilityBindStatement | SOAbilityBindStatement | src/self_hosted/semantic/ast_bind_statement_type_fact_owner.pgy | SemanticAstBindStatementTypeVerdictFor | src/self_hosted/semantic/ast_statement_type_fact_owner.pgy,src/self_hosted/mir/declaration_rows_owner.pgy,src/self_hosted/mir/declaration_json_projection_owner.pgy,src/self_hosted/mir_lower/routine_lower.pgy,src/self_hosted/codegen/emission/ability_bind_emit_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_dynamic_ability_call_emit_owner.pgy,src/self_hosted/codegen/emission/role_dispatch_emit_owner.pgy | bind_party_slot_text_reparse,role_implementation_text_reparse,direct_ability_call_fallback,missing_role_slot_abi_fact_success | tests/self_hosted/parity/driver_rung2_ability_bind_dispatch_parity_owner.sh#Owns dynamic role-slot ABI dispatch and fail-closed bind validation. | CLOSED | none
selfhost.expression_surface | semantic | SyntaxNodeId | SFExpressionSurface | SOSemanticExpressionSurface | src/self_hosted/semantic/ast_expression_surface_fact_owner.pgy | SemanticAstExpressionSurfaceFacts | src/self_hosted/semantic/ast_expression_graph_struct_type_verdict_owner.pgy,src/self_hosted/semantic/ast_expression_graph_field_type_owner.pgy,src/self_hosted/semantic/call_check_owner.pgy,src/self_hosted/semantic/ast_expression_graph_generic_call_owner.pgy,src/self_hosted/semantic/ast_generic_specialization_fact_owner.pgy,src/self_hosted/semantic/ast_expression_graph_call_view_owner.pgy,src/self_hosted/codegen/input/ast_expression_usage_owner.pgy,src/self_hosted/codegen/input/ast_usage_owner.pgy,src/self_hosted/codegen/input/semantic_expression_codegen_view_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_graph_emit_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_identity_bound_call_emit_owner.pgy,src/self_hosted/codegen/emission/option_value_emit_owner.pgy,src/self_hosted/codegen/emission/collection_element_emit_owner.pgy | TypedAstArenaAtomText,TypedAstArenaValueText,TypedAstArenaAuxValueText,ContainsCallOutsideStrings,CodegenExpressionUsageFactsFromArena,CodegenAstArenaExpressionPartsAt,FindTopLevelPlus,RewriteBool,RewriteIndexing,RewriteInoutCallArgs,ExprSequenceItemAt,generic_return_text_inference,codegen_generic_graph_rescan,expr_semantic_shape_emit_owner,callable_type_from_text,callable_target_name_dispatch | tests/self_hosted/parity/driver_rung2_body_parity.sh#producer-first source/MIR parity ok,tests/self_hosted/parity/driver_rung2_nested_generic_containers_parity_owner.sh#List<T> contextual construction and fail-closed ABI negatives,tests/self_hosted/parity/driver_rung2_callable_parameter_identity_owner.sh#callable identity C/LLVM runtime parity + exact negative ratchet: PASS,tests/self_hosted/parity/callable_parameter_installed_self_host_owner.sh#public C/LLVM callable substitution + native runtime oracle: PASS | BRIDGE | logical_equality_and_string_compare_projection_without_text_reparse_relational_arithmetic_index_logical_not_numeric_negate_direct_identifier_call_simple_and_nested_member_field_method_namespace_qualified_call_log_argument_pipe_call_postfix_try_for_range_identifier_and_nonidentifier_foreach_payload_free_enum_argument_array_literal_call_argument_named_struct_literal_call_argument_general_named_struct_literal_value_struct_literal_nominal_and_field_types_option_struct_some_constructor_payload_contextual_option_struct_none_codegen_ast_text_node_array_push_array_set_array_literal_and_indexed_assignment_value_and_target_graphs_are_parser_owned_and_assignment_target/index_scalar_type_facts_are_semantic_owned;graph_owned_concrete_scalar_tree_results_operand_diagnostics_resolved_direct_namespace_receiver_generic_local_and_chained_field_receiver_call_target_identity_arity_argument_verdicts_exact_and_nested_formal_parameter_binding_nested_generic_direct_return_substitution_ordered_explicit_generic_actual_carriage_and_conflict_rejection_bounded_MIR_codegen_target_carriage_scalar_option_result_builtin_policy_with_target_consistency_collection_mutation_admission_across_specialized_statement_and_graph_call_consumers_and_aggregate_field_types_and_assignability_for_scalar_direct_or_wrapper_nominal_values_contextual_Result_value_to_explicit_Result_field_assignment_plus_explicit_top_level_and_local_initializer_inferred_generic_actual_aggregate_values_and_ref_inout_call_argument_addressability_from_node_binding_and_nominal_field_facts_are_C_LLVM_negative_gated;the_top_level_shape_emitter_and_its_dead_accessors_are_deleted_while_shape_rows_remain_graph_root_consistency_evidence;collection_result_or_element_typing_unknown_or_aggregate_wrapper_payloads_member_aggregate_field_values_return_or_assignment_rooted_inferred_generic_calls_member_generic_calls_nested_generic_locals_object_init_special_unary_and_initial_compact_tree_arena_construction_remain_bridged;canonical_recursive_callable_type_shapes_and_callable_valued_formal_or_declared_leaves_now_keep_target_and_binding_SyntaxNodeId_through_installed_C_semantic_reentry_and_direct_MIR_LLVM_but_general_function_values_closures_and_remaining_expression_shapes_keep_the_row_BRIDGE
selfhost.semantic_artifact_admission | semantic | AstTreeArtifactIdentityDigest | SFSemanticAstArtifactAdmission | SOSemanticArtifact | src/self_hosted/semantic/ast_artifact_verdict_owner.pgy | SemanticAstArtifactAnalyzeFromExpressionSurfaces | src/self_hosted/semantic/ast_body_analysis_admission_owner.pgy,src/self_hosted/semantic/ast_body_type_bundle_owner.pgy,src/self_hosted/semantic/ast_named_value_boundary_verdict_owner.pgy,src/self_hosted/codegen/emission/program_admitted_semantic_owner.pgy,src/self_hosted/codegen/emission/program_entry_owner.pgy,src/self_hosted/codegen/emission/program_emit.pgy,src/self_hosted/compiler/driver_pipeline_owner.pgy,src/self_hosted/compiler/driver_rung2_owner.pgy,src/self_hosted/semantic/ast_expression_carried_callable_identity_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_identity_bound_call_emit_owner.pgy | emission_whole_artifact_semantic_reconstruction,identity_from_node_count_only,emission_digest_recalculation,admitted_path_checked_fallback,admitted_body_constructor_rows_revalidation,malformed_body_parallel_row,raw_analysis_without_deep_proof,timeout_as_correctness,callable_identity_reconstructed_after_admission,callable_builtin_name_override,backend_named_value_boundary_rejection_as_source_semantics,owned_sequence_type_named_boundary_allowlist | tests/self_hosted/parity/direct_mir_array_named_value_boundary_owner.sh#native/self Array family rejection + named/default/copy-only controls: PASS,tests/self_hosted/parity/direct_mir_owned_sequence_named_value_boundary_owner.sh#native/self List+Queue rejection + named/default controls + native fresh constructors: PASS,tests/self_hosted/parity/semantic_expression_environment_owned_lifetime_smoke.sh#semantic admission repeats an unbounded artifact proof,tests/self_hosted/parity/semantic_expression_environment_owned_lifetime_smoke.sh#admitted body reopens checked constructor rows,tests/self_hosted/parity/semantic_expression_environment_owned_lifetime_smoke.sh#direct codegen seed lost its one-analysis admitted path,src/self_hosted/semantic/ast_artifact_verdict_contract_owner.pgy#stale_same_count,src/self_hosted/semantic/ast_artifact_verdict_contract_owner.pgy#stale_same_tree,src/self_hosted/semantic/ast_body_analysis_admission_contract_owner.pgy#malformed_parallel_shape,tests/self_hosted/parity/compiler_internal_builtin_caller_provenance_owner.sh#common caller registry and parser-owned declaration module provenance reject exact wrong-path impersonation in C/LLVM legacy and artifact routes while the production driver bootstrap requires typed --source and rejects its parser/AST-text detour,tests/self_hosted/parity/domain_runtime_zone_sync_execution_owner.sh#zero-topology typed receipt and exact zone-definition bijection,tests/self_hosted/parity/default_c_compile_installed_self_host_owner.sh#installed self-host artifact owns plain C compile; host-only link is closed,tests/self_hosted/parity/default_llvm_installed_self_host_owner.sh#installed compiler intent artifact and canonical runtime own public LLVM compile/run,tests/self_hosted/parity/public_ast_installed_self_host_owner.sh#canonical composite priority AST identity,tests/self_hosted/parity/public_dir_installed_self_host_owner.sh#canonical composite priority DIR admission and non-Int rejection,tests/self_hosted/parity/driver_rung2_callable_parameter_identity_owner.sh#callable identity C/LLVM runtime parity + exact negative ratchet: PASS,tests/self_hosted/parity/callable_parameter_installed_self_host_owner.sh#public C/LLVM callable substitution + native runtime oracle: PASS | ACTIVE | body materialization consumes one identity-and-row-shape receipt; admitted production consumers reuse constructor proof while arbitrary-pair wrappers retain the checked path; internal storage-retire caller identity is owned once by the common registry and parser-owned declaration module provenance reaches aligned semantic signatures, with consumer-local tuple and root-path guessing rejected; the production driver bootstrap consumes typed --source directly, while AST-text provenance remains unknown and compiler-internal calls fail closed; the installed C artifact, compile/link, and run targets consume the admitted compiler-root path with native semantic/codegen fallback deleted; released default-runtime LLVM now consumes the canonical runtime owner for the admitted host-I/O surface; canonical composite priority now reaches installed AST and full DIR with non-Int rejection; intent-observability and generated thread-safe zone construction/destruction remain open; canonical callable types and formal/declared callable identities now cross-seal syntax and binding IDs through installed C/LLVM while broader artifact admission stays ACTIVE
selfhost.initializer_type_verdict | semantic | SyntaxNodeId | SFInitializerTypeVerdict | SOSemanticInitializerType | src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy | SemanticAstInitializerTypeFacts | src/self_hosted/semantic/ast_body_type_bundle_owner.pgy,src/self_hosted/mir/artifact_lower_owner.pgy,src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy,src/self_hosted/semantic/ast_iteration_type_fact_owner.pgy,src/self_hosted/semantic/ast_statement_type_fact_owner.pgy,src/self_hosted/semantic/ast_contextual_builtin_type_owner.pgy | source_initializer_type_rescan,backend_initializer_type_guess,semantic_normalization_char_at,semantic_normalization_trim_copy,semantic_validation_trim_copy | tests/self_hosted/parity/initializer_projection_probe_parity.sh#missing-initializer-row,tests/self_hosted/parity/driver_rung2_nested_generic_containers_parity_owner.sh#missing contextual List type fails closed,tests/self_hosted/parity/semantic_expression_normalization_owner_smoke.sh#byte-view normalization and source reuse are wired,tests/self_hosted/parity/semantic_expression_validation_lifetime_owner_smoke.sh#graph validation reuses normalized source | CLOSED | none
selfhost.iteration_type_verdict | semantic | SyntaxNodeId | SFIterationTypeVerdict | SOSemanticIterationType | src/self_hosted/semantic/ast_iteration_type_fact_owner.pgy | SemanticAstIterationTypeFacts | src/self_hosted/semantic/ast_body_type_bundle_owner.pgy,src/self_hosted/mir/artifact_lower_owner.pgy,src/self_hosted/mir/json_projection_owner.pgy,src/self_hosted/mir_lower/iteration_type_fact_owner.pgy,src/self_hosted/mir_lower/routine_fact_index_owner.pgy,src/self_hosted/mir_lower/routine_lower.pgy,src/self_hosted/codegen/emission/foreach_collection_runtime_owner.pgy,src/self_hosted/codegen/emission/stmt_emit.pgy,src/compiler/mir_source_local_types.c,src/compiler/mir_json_dump.c | source_iteration_type_rescan,backend_iteration_type_guess,source_local_type_as_iteration_authority,mir_foreach_collection_type_guess | tests/self_hosted/parity/driver_rung2_body_parity.sh#native_MIR_JSON_rows_and_C_LLVM_forloop_foreach_rows,tests/self_hosted/parity/driver_rung2_for_in_list_parity_owner.sh#Owns List<T> foreach iteration facts, graph shape, and List ABI use. | CLOSED | none
selfhost.assignment_type_verdict | semantic | SyntaxNodeId | SFAssignmentTypeVerdict | SOSemanticAssignmentType | src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy | SemanticAstAssignmentTypeFacts | src/self_hosted/semantic/ast_body_type_bundle_owner.pgy,src/self_hosted/codegen/input/semantic_assignment_codegen_view_owner.pgy,src/self_hosted/codegen/input/semantic_body_type_codegen_view_owner.pgy,src/self_hosted/codegen/emission/stmt_emit.pgy,src/self_hosted/codegen/emission/assign_emit_owner.pgy | source_assignment_type_rescan,backend_assignment_type_guess,missing_expected_type_success,assignment_target_name_as_c_binding,assignment_target_local_sanitize,collection_target_name_as_c_binding,collection_target_cref_fallback,let_binding_name_as_c_binding | tests/self_hosted/parity/assignment_projection_probe_parity.sh#missing-expected-type,tests/self_hosted/parity/assignment_projection_probe_parity.sh#missing-c-binding,tests/self_hosted/parity/assignment_projection_probe_parity.sh#collection-cref-only | CLOSED | none
selfhost.call_target_identity | semantic | SyntaxNodeId | SFCallTargetIdentity | SOSemanticCallTarget | src/self_hosted/semantic/ast_expression_call_target_fact_owner.pgy | SemanticExpressionCallTargetFact | src/self_hosted/semantic/ast_expression_call_target_capture_owner.pgy,src/self_hosted/semantic/ast_expression_graph_resolved_call_type_owner.pgy,src/self_hosted/semantic/ast_expression_graph_concrete_scalar_verdict_owner.pgy,src/self_hosted/mir/expression_graph_fact_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy,src/self_hosted/compiler/driver_rung2_owner.pgy,src/self_hosted/semantic/ast_expression_carried_callable_identity_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_identity_bound_call_emit_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_identity_owner.pgy | callee_text_as_final_identity,namespace_name_join_fallback,codegen_receiver_type_method_join | tests/self_hosted/parity/initializer_projection_probe_parity.sh#missing-carried-direct-target,tests/self_hosted/parity/driver_rung2_list_ops_parity_owner.sh#missing List call target fails closed,tests/self_hosted/parity/driver_rung2_callable_parameter_identity_owner.sh#callable identity C/LLVM runtime parity + exact negative ratchet: PASS | CLOSED | none
selfhost.collection_call_protocol | semantic | CollectionCallId | SFCollectionCallProtocol | SOCollectionCallProtocol | src/self_hosted/semantic/ast_expression_graph_collection_call_protocol_owner.pgy | SemanticExpressionGraphCollectionCallProtocolFromName | src/self_hosted/semantic/ast_expression_graph_resolved_call_type_owner.pgy,src/self_hosted/semantic/ast_expression_graph_queue_call_owner.pgy,src/self_hosted/semantic/ast_expression_graph_set_call_owner.pgy,src/self_hosted/semantic/ast_expression_verdict_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_composite_literal_emit_owner.pgy,src/self_hosted/codegen/emission/list_call_type_owner.pgy,src/self_hosted/codegen/emission/queue_call_type_owner.pgy,src/self_hosted/codegen/emission/set_call_type_owner.pgy,src/self_hosted/codegen/emission/list_call_emit_owner.pgy,src/self_hosted/codegen/emission/queue_call_emit_owner.pgy,src/self_hosted/codegen/emission/set_call_emit_owner.pgy | collection_call_name_redeclaration,collection_call_arity_redeclaration,collection_call_argument_position_redeclaration,collection_call_return_shape_redeclaration,missing_collection_call_protocol_success | tests/self_hosted/parity/collection_call_protocol_owner_smoke.sh#shared collection call protocol owner and negative ratchet are wired | CLOSED | none
selfhost.queue_call_runtime_surface | semantic | SyntaxNodeId | SFQueueCallRuntimeSurface | SOQueueCallRuntime | src/self_hosted/semantic/ast_expression_graph_queue_call_owner.pgy | SemanticExpressionGraphQueueCallFact | src/self_hosted/semantic/ast_expression_graph_resolved_call_type_owner.pgy,src/self_hosted/semantic/ast_expression_verdict_owner.pgy,src/self_hosted/codegen/runtime_abi/queue_runtime_owner.pgy,src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_call_type_owner.pgy,src/self_hosted/codegen/emission/queue_call_type_owner.pgy,src/self_hosted/codegen/emission/queue_call_emit_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_composite_literal_emit_owner.pgy,src/self_hosted/codegen/input/ast_type_usage_owner.pgy,src/self_hosted/codegen/input/ast_usage_owner.pgy,src/self_hosted/codegen/emission/program_emit.pgy | queue_receiver_type_guess,queue_element_type_guess,source Queue spelling as ABI,missing_queue_runtime_fact_success,List_runtime_fallback | tests/self_hosted/parity/driver_rung2_queue_ops_parity_owner.sh#Queue MIR/runtime ABI parity and fail-closed negatives | CLOSED | none
selfhost.set_call_runtime_surface | semantic | SyntaxNodeId | SFSetCallRuntimeSurface | SOSetCallRuntime | src/self_hosted/semantic/ast_expression_graph_set_call_owner.pgy | SemanticExpressionGraphSetCallFact | src/self_hosted/semantic/ast_expression_graph_resolved_call_type_owner.pgy,src/self_hosted/semantic/ast_expression_verdict_owner.pgy,src/self_hosted/codegen/runtime_abi/set_runtime_owner.pgy,src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_call_type_owner.pgy,src/self_hosted/codegen/emission/set_call_type_owner.pgy,src/self_hosted/codegen/emission/set_call_emit_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_composite_literal_emit_owner.pgy,src/self_hosted/codegen/input/ast_type_usage_owner.pgy,src/self_hosted/codegen/input/ast_usage_owner.pgy,src/self_hosted/codegen/emission/program_emit.pgy | set_receiver_type_guess,set_element_type_guess,source Set spelling as ABI,missing_set_runtime_fact_success,Queue_runtime_fallback | tests/self_hosted/parity/driver_rung2_set_ops_parity_owner.sh#Set MIR/runtime ABI parity and fail-closed negatives | CLOSED | none
selfhost.expression_place_kind | semantic | SyntaxNodeId | SFExpressionPlaceKind | SOSemanticExpressionPlace | src/self_hosted/semantic/ast_expression_place_fact_owner.pgy | SemanticAstAnalysisResolveExpressionPlacesFromBody | src/self_hosted/semantic/ast_body_type_bundle_owner.pgy,src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy | expr_semantic_addressability_owner.pgy,CodegenExpressionAddressabilityFromGraph,codegen_cbind_addressability_recovery | tests/self_hosted_component_contract_smoke.sh#func SemanticAstBodyTypeBundleMissingPlaceContractReady( | CLOSED | none
selfhost.type_runtime_usage_surface | semantic | SyntaxNodeId | SFTypeRuntimeUsageSurface | SOSemanticTypeSurface | src/self_hosted/semantic/ast_type_surface_fact_owner.pgy | SemanticAstTypeSurfaceFacts | src/self_hosted/codegen/input/ast_type_usage_owner.pgy,src/self_hosted/codegen/input/value_wrapper_usage_owner.pgy,src/self_hosted/codegen/input/ast_usage_owner.pgy,src/self_hosted/codegen/emission/type_declaration_emit_owner.pgy,src/self_hosted/codegen/emission/result_runtime_emit_owner.pgy,src/self_hosted/codegen/emission/foreach_collection_runtime_owner.pgy,src/self_hosted/codegen/abi_layout/enum_abi_value_fact_owner.pgy,src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy,src/self_hosted/codegen/runtime_abi/option_result_runtime_owner.pgy,src/self_hosted/codegen/runtime_abi/list_runtime_owner.pgy | TypedAstArenaTypeName,CodegenAstArenaTypeFactPresent,CodegenTypeUsageFactsFromArena,CodegenResultUsageFactsFromSemantic,EmitResultRuntimeDefinitions,OptionResultRuntimeCNamedOptionDefinition | tests/self_hosted_component_contract_smoke.sh#option_payload_free_enum_field_declaration,tests/self_hosted/parity/driver_rung2_body_parity.sh#producer-first source/MIR parity ok,tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok,tests/self_hosted/parity/driver_rung2_nested_generic_containers_parity_owner.sh#nested List ABI owner and negative mutations,tests/self_hosted/parity/driver_rung2_list_ops_parity_owner.sh#List operation ABI calls,tests/self_hosted/parity/driver_rung2_for_in_list_parity_owner.sh#List foreach runtime symbol missing | CLOSED | none
selfhost.node_kind_surface | semantic | SyntaxNodeId | SFNodeKindSurface | SOSemanticKindSurface | src/self_hosted/semantic/ast_kind_surface_fact_owner.pgy | SemanticAstKindSurfaceFacts | src/self_hosted/codegen/input/ast_kind_usage_owner.pgy,src/self_hosted/codegen/input/ast_usage_owner.pgy,src/self_hosted/codegen/input/semantic_kind_codegen_view_owner.pgy,src/self_hosted/codegen/emission/program_emit.pgy | TypedAstArenaNodeKindIs,CodegenAstArenaKindPresent,CodegenKindUsageFactsFromArena,CodegenAstKindArrayLiteral,CodegenAstArenaIsAbilityDecl,CodegenAstArenaIsEventDecl | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.entrypoint_selection | semantic | SyntaxNodeId | SFEntrypointSelection | SOSemanticSignature | src/self_hosted/semantic/ast_signature_fact_owner.pgy | SemanticAstFunctionSignatureFacts | src/self_hosted/semantic/ast_artifact_verdict_owner.pgy,src/self_hosted/codegen/input/semantic_signature_codegen_view_owner.pgy,src/self_hosted/codegen/emission/program_emit.pgy | SemanticAstArtifactIsMainFunction,CodegenAstArenaIsMainFunction | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.function_declaration_rows | semantic | SyntaxNodeId | SFFunctionDeclarationRows | SOSemanticSignature | src/self_hosted/semantic/ast_signature_fact_owner.pgy | SemanticAstFunctionSignatureFacts | src/self_hosted/codegen/input/semantic_signature_codegen_view_owner.pgy,src/self_hosted/codegen/emission/program_emit.pgy,src/self_hosted/semantic/ast_signature_param_node_query_owner.pgy,src/self_hosted/semantic/ast_callable_type_shape_owner.pgy,src/self_hosted/codegen/emission/callable_parameter_prototype_owner.pgy | CodegenAstArenaIsFunction | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok,tests/self_hosted/parity/driver_rung2_callable_parameter_identity_owner.sh#callable identity C/LLVM runtime parity + exact negative ratchet: PASS | CLOSED | none
selfhost.intent_declaration_rows | semantic | SyntaxNodeId | SFIntentDeclarationRows | SOSemanticIntentSignature | src/self_hosted/semantic/ast_intent_signature_fact_owner.pgy | SemanticAstIntentSignatureFacts | src/self_hosted/semantic/ast_expression_environment_owner.pgy,src/self_hosted/semantic/ast_intent_expression_environment_owner.pgy,src/self_hosted/dir/intent_fact_owner.pgy,src/self_hosted/dir/intent_exact_identity_contract_owner.pgy,src/self_hosted/dir/intent_row_owner.pgy,src/self_hosted/dir/intent_step_fact_owner.pgy,src/self_hosted/dir/intent_step_clause_fact_owner.pgy,src/self_hosted/dir/intent_step_carriage_contract_owner.pgy,src/self_hosted/mir/intent_routine_owner.pgy,src/self_hosted/mir/intent_resource_lifetime_owner.pgy,src/self_hosted/mir/intent_phase_contract_owner.pgy,src/self_hosted/mir/intent_phase_emission_owner.pgy,src/self_hosted/mir/program_fact_owner.pgy,src/self_hosted/mir_lower/intent_lower_owner.pgy,src/self_hosted/mir_lower/intent_phase_projection_owner.pgy,src/self_hosted/mir_lower/intent_phase_tree_owner.pgy,src/self_hosted/mir_lower/intent_step_placement_contract_owner.pgy,src/self_hosted/mir_lower/expression_graph_instruction_policy_owner.pgy,src/self_hosted/mir_lower/intent_execution_plan_fact_owner.pgy,src/self_hosted/compiler/driver_rung2_intent_consumer_owner.pgy,src/self_hosted/compiler/intent_policy_c_codegen_bridge_owner.pgy,src/self_hosted/compiler/intent_execution_c_codegen_bridge_owner.pgy,src/self_hosted/codegen/input/intent_execution_codegen_view_owner.pgy,src/self_hosted/codegen/input/intent_policy_codegen_view_owner.pgy,src/self_hosted/codegen/emission/intent_execution_plan_emit_owner.pgy,src/self_hosted/codegen/emission/intent_execution_plan_control_emit_owner.pgy,src/self_hosted/codegen/emission/intent_execution_plan_local_emit_owner.pgy,src/self_hosted/codegen/emission/intent_emit_owner.pgy,src/self_hosted/codegen/emission/intent_action_step_emit_owner.pgy,src/self_hosted/codegen/emission/intent_nested_call_emit_owner.pgy,src/self_hosted/codegen/emission/intent_signature_emit_owner.pgy,src/self_hosted/codegen/emission/intent_control_flow_emit_owner.pgy | intent_signature_as_function_row,raw_source_intent_signature,unknown_intent_parameter_success,duplicate_intent_parameter_success,value_participant_as_authority,intent_kind_fallback,commit_identity_drift,binding_type_drift,zone_alias_drift,authorization_cross_carrier,rollback_identity_drift,outcome_bool_collapse,variant_spelling_classification,payload_type_reinfer,predecessor_from_source_order,completion_after_any_call,compensate_ast_rescan,final_intent_ast_child_rescan,missing_source_dir_receipt,crossed_source_dir_identity,unknown_intent_phase,orphan_intent_phase,wrong_intent_phase_step_or_slot,duplicate_intent_phase,check_or_compensate_result_type,on_result_type_asymmetry,missing_intent_phase_graph,old_direct_orchestration,consumer_plan_revalidation,expression_graph_reconstruction,name_only_payload_declaration_join,reachable_zero_compensation_scaffold,typed_direct_rollback_bypass,nested_intent_as_action_step,nested_intent_fake_placement_materialization,unconditional_action_authority_requirement | tests/self_hosted/parity/intent_guard_post_compensation_execution_owner.sh#guard/expect/post failure + ordered compensation + history parity: PASS,tests/self_hosted/parity/intent_callable_reachability_owner.sh#semantic + exact intent DIR reachability: PASS,tests/self_hosted/parity/intent_callable_execution_owner.sh#exact action + nested-intent execution and MIR negatives: PASS,tests/self_hosted/parity/intent_nested_callable_execution_owner.sh#public C direct-call success/failure parity: PASS,tests/self_hosted/parity/intent_typed_outcome_execution_owner.sh#enum<tobject> binding + exact-once parity + MIR negatives: PASS,tests/self_hosted/parity/intent_phase_carrier_negative_owner.sh#phase order + admitted MIR negatives: PASS,tests/self_hosted/parity/intent_typed_outcome_compensation_owner.sh#v3 zone identity + predecessor compensation + history parity: PASS,tests/self_hosted/parity/intent_execution_protocol_static_owner.py#intent plan/CFG/digest revalidation escaped admission,tests/self_hosted/parity/compiler_root_intent_takeover_gate.sh#one Pergyra purpose intent/action owns source-to-LLVM coordination,tests/self_hosted_component_contract_smoke.sh#retired_intent_ast_reconstruction | CLOSED | none
selfhost.zone_authority_rows | semantic | SyntaxNodeId | SFZoneAuthorityRows | SOSemanticZoneAuthority | src/self_hosted/semantic/ast_zone_authority_fact_owner.pgy | SemanticAstZoneAuthorityFactsFromArtifact | src/self_hosted/semantic/ast_zone_authority_validation_owner.pgy,src/self_hosted/semantic/ast_artifact_verdict_owner.pgy,src/self_hosted/semantic/ast_body_analysis_admission_owner.pgy,src/self_hosted/dir/domain_graph_fact_owner.pgy,src/self_hosted/codegen/emission/intent_step_binding_owner.pgy,src/self_hosted/codegen/emission/intent_emit_owner.pgy,src/self_hosted/codegen/emission/program_emit.pgy | authority_from_participant_type,undeclared_subject_as_authority,where_using_zone_drift,ambiguous_subject_slot_success,missing_subject_slot_success,AST_domain_rescan,authority_child_rescan,ability_name_only_join | tests/self_hosted/parity/intent_step_binding_contract_owner.sh#distinct actor/authority plus where/value/inout/ambiguous/missing negatives: PASS,tests/self_hosted/parity/zone_authority_fact_owner.sh#semantic identity carriage + admitted mutation negatives + DIR no-rescan: PASS,tests/self_hosted_component_contract_smoke.sh#zone authority owner and consumer ratchets | BRIDGE | exact authority/zone/subject-slot/required-ability identities now reach DIR without AST rescans; legacy self-C intent binding remains reachable while the production MIR authority transition and shared zone-sync runtime plan remain open
selfhost.action_contract | semantic | SyntaxNodeId | SFActionContract | SOSemanticActionContract | src/self_hosted/semantic/ast_action_contract_fact_owner.pgy | SemanticAstActionContractFactsFromArtifact | src/self_hosted/semantic/ast_signature_fact_owner.pgy,src/self_hosted/semantic/ast_signature_artifact_match_owner.pgy,src/self_hosted/codegen/input/semantic_signature_codegen_view_owner.pgy,src/self_hosted/codegen/emission/function_emit.pgy,src/self_hosted/codegen/emission/program_emit.pgy,src/self_hosted/mir/declaration_rows_owner.pgy,src/self_hosted/mir/declaration_verify_owner.pgy,src/self_hosted/mir/declaration_json_projection_owner.pgy,src/self_hosted/mir_lower/declaration_method_contract_fact_owner.pgy,src/self_hosted/mir_lower/declaration_callable_lower_owner.pgy,src/compiler/mir_decl_header_validate.c,src/compiler/mir_json_dump_decl.c | action_as_function_kind,action_clause_skip_to_body,action_clause_text_rescan,parser_caps_effects_discard,missing_contract_wire_success,callable_kind_default_function,backend_contract_recovery,independent_contract_vocabulary,multi_impl_role_declaration_drop | tests/self_hosted/parity/driver_rung2_action_contract_parity_owner.sh#ActionContract carriage and fail-closed wire mutations,tests/callable_contract_vocabulary_smoke.sh#18 semantic words and projections: ok | CLOSED | none
selfhost.local_binding_statement_routing | semantic | SyntaxNodeId | SFLocalBindingStatementRouting | SOSemanticLocalBinding | src/self_hosted/semantic/ast_local_binding_fact_owner.pgy | SemanticAstLocalBindingFacts | src/self_hosted/codegen/input/semantic_local_binding_codegen_view_owner.pgy,src/self_hosted/codegen/emission/stmt_emit.pgy,src/self_hosted/codegen/emission/program_emit.pgy | CodegenAstArenaIsLetStmt | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.assignment_statement_routing | semantic | SyntaxNodeId | SFAssignmentStatementRouting | SOSemanticAssignment | src/self_hosted/semantic/ast_assignment_fact_owner.pgy | SemanticAstAssignmentFacts | src/self_hosted/codegen/input/semantic_assignment_codegen_view_owner.pgy,src/self_hosted/codegen/emission/stmt_emit.pgy,src/self_hosted/codegen/emission/program_emit.pgy | CodegenAstArenaIsAssignStmt | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.statement_kind_routing | semantic | SyntaxNodeId | SFStatementKindRouting | SOSemanticStatement | src/self_hosted/semantic/ast_statement_fact_owner.pgy | SemanticAstStatementFacts | src/self_hosted/codegen/input/semantic_statement_codegen_view_owner.pgy,src/self_hosted/codegen/emission/stmt_emit.pgy,src/self_hosted/codegen/emission/program_emit.pgy | CodegenAstArenaIsLogStmt,CodegenAstArenaIsBareReturnStmt,CodegenAstArenaIsValueReturnStmt,CodegenAstArenaIsDeferStmt,CodegenAstArenaIsArrayPopStmt,CodegenAstArenaIsArraySetStmt,CodegenAstArenaIsArrayPushStmt,CodegenAstArenaIsExitStmt,CodegenAstArenaIsBreakStmt,CodegenAstArenaIsContinueStmt,CodegenAstArenaIsForStmt,CodegenAstArenaIsWhileStmt,CodegenAstArenaIsIfStmt,CodegenAstArenaIsElseIfAt,CodegenAstArenaIsMatchStmt,CodegenAstArenaIsMatchCaseStmt,CodegenAstArenaIsMatchDefaultStmt,CodegenAstArenaIsBareCallStmt | tests/sot_authority_adequacy_smoke.sh#live owner/consumer binding and negative mutations ok | CLOSED | none
selfhost.statement_result_type | semantic | SyntaxNodeId | SFStatementResultType | SOSemanticStatementType | src/self_hosted/semantic/ast_statement_type_fact_owner.pgy | SemanticAstStatementTypeFacts | src/self_hosted/semantic/ast_body_type_bundle_owner.pgy,src/self_hosted/semantic/ast_statement_type_query_owner.pgy,src/self_hosted/codegen/input/semantic_statement_codegen_view_owner.pgy,src/self_hosted/codegen/emission/stmt_emit.pgy,src/self_hosted/codegen/emission/log_emit_owner.pgy | ExprKind(,ExprKind(match_subject,CodegenSemanticLogArgumentOrDie | tests/self_hosted_component_contract_smoke.sh#SemanticAstStatementTypeQueryContractReady() | CLOSED | none
projection.direct_mir_compile_time_declaration_erasure | projection | DirectMirCompileTimeDeclarationErasureId | SFDirectMirCompileTimeDeclarationErasure | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_compile_time_declaration_erasure_owner.pgy | DirectMirCompileTimeDeclarationErasureFactFromAdmitted | src/self_hosted/compiler/direct_mir_literal_log_plan_owner.pgy | DirectMirHelloProjectionFromAdmitted,DirectMirHelloEmit,Arithmetic,expr0 | tests/self_hosted/parity/one_mir_compile_time_declaration_literal_projection.sh#ability exact 7, zero-decl/rename/display equality, literal 73, and | CLOSED | none
projection.direct_mir_literal_log_plan | projection | VerifiedLiteralLogProjectionPlanId | SFDirectMirLiteralLogPlan | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_literal_log_plan_owner.pgy | DirectMirLiteralLogPlanFromAdmitted | src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy,src/self_hosted/compiler/direct_mir_literal_log_emission_owner.pgy | DirectMirHelloProjectionFromAdmitted,DirectMirHelloEmit,Arithmetic,expr0 | tests/self_hosted/parity/one_mir_compile_time_declaration_literal_projection.sh#ability exact 7, zero-decl/rename/display equality, literal 73, and | CLOSED | none
projection.direct_mir_array_int_plan | projection | VerifiedArrayIntProjectionPlanId | SFDirectMirArrayIntCfgPlan | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_array_int_plan_owner.pgy | DirectMirArrayIntPlanFromAdmitted | src/self_hosted/compiler/direct_mir_array_int_abi_projection_owner.pgy,src/self_hosted/compiler/direct_mir_array_int_emission_owner.pgy | scalar_retry_after_array_classification,array_source_text_recovery,backend_specific_array_plan,repeated_expression_graph_index,stale_ssa_use,unbound_target_fingerprint,unchecked_static_index,native_codegen_fallback,post_issue_plan_mutation | tests/self_hosted/parity/one_mir_array_int_projection.sh#runtime-free Array<Int> C/LLVM parity and seven negatives are fail-closed (sha256= | CLOSED | none
projection.direct_mir_array_return_program_identity | projection | DirectMirArrayReturnProgramId | SFDirectMirArrayReturnProgramIdentity | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_array_return_program_identity_owner.pgy | DirectMirArrayReturnProgramIdentityFromAdmitted | src/self_hosted/compiler/direct_mir_array_return_plan_owner.pgy | first_routine_entrypoint,row_order_authority,name_only_callee_without_signature,missing_return_void_default,call_target_text_fallback,native_codegen_fallback | tests/self_hosted/parity/one_mir_array_return_projection.sh#One source-produced two-routine MIR graph drives runtime-free C and LLVM. | CLOSED | none
projection.direct_mir_array_return_plan | projection | VerifiedArrayReturnProjectionPlanId | SFDirectMirCallReturnGraph | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_array_return_plan_owner.pgy | DirectMirArrayReturnPlanFromAdmitted | src/self_hosted/compiler/direct_mir_array_int_abi_projection_owner.pgy,src/self_hosted/compiler/direct_mir_array_return_emission_owner.pgy | backend_mir_read,callee_stack_pointer_return,flattened_call_graph,stale_caller_ssa_use,nonterminal_or_unreachable_straight_line,forged_log_scalar_fact,backend_specific_return_plan,unbound_target_fingerprint,single_routine_retry,native_codegen_fallback,post_issue_plan_mutation | tests/self_hosted/parity/one_mir_array_return_projection.sh#The caller owns returned Array<Int> backing storage, and routine order is not | CLOSED | none
projection.direct_mir_array_argument_program_identity | projection | DirectMirArrayArgumentProgramId | SFDirectMirArrayArgumentProgramIdentity | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_array_argument_program_identity_owner.pgy | DirectMirArrayArgumentProgramIdentityFromAdmitted | src/self_hosted/compiler/direct_mir_array_argument_plan_owner.pgy | first_routine_entrypoint,row_order_authority,name_only_callee_without_signature,missing_param_abi,parameter_ssa_invention,call_target_text_fallback,two_routine_retry,native_codegen_fallback | tests/self_hosted/parity/one_mir_array_argument_projection.sh#One source-produced three-routine MIR graph drives runtime-free C and LLVM. | CLOSED | none
projection.direct_mir_array_argument_plan | projection | VerifiedArrayArgumentProjectionPlanId | SFDirectMirCallParameterGraph | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_array_argument_plan_owner.pgy | DirectMirArrayArgumentPlanFromAdmitted | src/self_hosted/compiler/direct_mir_array_int_abi_projection_owner.pgy,src/self_hosted/compiler/direct_mir_array_argument_emission_owner.pgy | backend_mir_read,flattened_call_graph,raw_pointer_array_carriage,callee_owned_argument_storage,instruction_abi_fallback,stale_or_invented_ssa_use,backend_specific_parameter_plan,unbound_target_fingerprint,two_routine_retry,native_codegen_fallback,post_issue_plan_mutation | tests/self_hosted/parity/one_mir_array_argument_projection.sh#native/self parameter ABI, caller-owned by-value Array<Int> C/LLVM parity, cyclic row order, and sixteen negatives are fail-closed (sha256= | CLOSED | none
projection.direct_mir_collection_program_plan | projection | DirectMirCollectionProgramPlanId | SFDirectMirCollectionProgramPlan | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_collection_program_plan_owner.pgy | DirectMirCollectionProgramPlanFromAdmitted | src/self_hosted/compiler/direct_mir_collection_program_projection_owner.pgy,src/self_hosted/compiler/direct_mir_collection_program_c_emission_owner.pgy,src/self_hosted/compiler/direct_mir_collection_program_llvm_emission_owner.pgy | fixture_or_constant_route,three_routine_count_classifier,raw_valueid_program_lookup,fixed_no_reallocation,backend_mir_read,array_argument_retry,stale_return_edge,unbound_parameter_abi,post_issue_plan_mutation | tests/self_hosted/parity/one_mir_array_param_projection.sh#collection return/parameter projection ok | BRIDGE | bounded one-producer one-entrypoint one-reduction Array<Int> program only; aliases, multiple collections, arbitrary reducers/effects, reserve policies, ownership-sensitive elements, and escaping cleanup remain separate rungs
projection.direct_mir_scalar_cfg_program_extension | projection | DirectMirScalarCfgProgramExtensionId | SFDirectMirScalarCfgProgramExtension | SOProjectionPlanner | src/self_hosted/compiler/direct_mir_scalar_cfg_graph_admission_owner.pgy | DirectMirScalarCfgGraphPlanFromAdmitted | src/self_hosted/compiler/direct_mir_scalar_cfg_program_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_builtin_route_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_builtin_argument_chain_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_builtin_signature_projection_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_builtin_call_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_nary_operand_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_nary_expression_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_runtime_abi_fact_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_runtime_abi_identity_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_runtime_abi_requirement_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_string_runtime_requirement_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_runtime_abi_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_runtime_abi_projection_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_runtime_abi_projection_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_graph_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_minimum_plan_shape_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_string_window_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_string_window_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_window_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_window_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_collection_builtin_signature_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_collection_expression_kind_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_collection_expression_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_query_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_runtime_materialization_requirement_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_string_collection_runtime_requirement_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_generated_runtime_projection_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_string_collection_runtime_projection_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_array_string_abi_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_array_string_abi_projection_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_graph_input_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_string_collection_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_string_collection_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_process_args_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_array_string_storage_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_dir_walk_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_collection_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_collection_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_process_args_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_storage_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_join_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_readonly_ref_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_program_llvm_emission_owner.pgy,src/self_hosted/compiler/direct_mir_routine_parameter_set_fact_owner.pgy,src/self_hosted/compiler/direct_mir_routine_parameter_set_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_program_parameter_identity_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_call_consumption_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_direct_call_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_program_expression_identity_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_case_math_builtin_signature_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_case_math_expression_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_case_math_runtime_requirement_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_case_math_runtime_projection_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_program_expression_diagnostic_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_graph_plan_verification_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_signature_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_program_llvm_signature_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_direct_call_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_direct_call_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_case_math_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_case_math_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_string_window_builtin_signature_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_string_index_expression_kind_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_string_search_expression_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_string_index_runtime_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_string_search_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_string_index_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_search_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_index_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_substring_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_string_trim_expression_kind_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_string_transform_builtin_signature_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_string_transform_expression_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_string_trim_runtime_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_string_transform_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_string_special_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_string_trim_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_string_scalar_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_transform_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_special_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_trim_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_foreign_declaration_owner.pgy,src/self_hosted/compiler/direct_mir_array_string_literal_fact_owner.pgy,src/self_hosted/compiler/direct_mir_bounded_literal_index_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_array_string_boundary_fact_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_array_string_boundary_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_array_string_boundary_plan_readiness_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_array_string_literal_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_literal_expression_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_array_string_cleanup_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_cleanup_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_fact_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy | second_cfg_plan,duplicate_ssa_or_phi_arrays,routine_or_block_count_route,string_concat_fixture_route,string_builtin_fixture_route,returned_array_retry,effectful_eager_logical_rhs,display_expression_fallback,fixture_output_constants,backend_mir_read,native_codegen_fallback,unbound_ssa_use,stale_phi_incoming,name_only_call_target,untyped_call_argument,partial_argument_chain,nary_operand_reparse,inactive_extension_payload,unchecked_modulo_divisor,llvm_signed_add_nsw,partial_phi_append,backend_local_string_abi,builtin_registry_bypass,loose_runtime_abi_fields,exact_to_string_log_fusion,string_collection_fixture_route,compile_time_split_result,external_collection_plan_double_claim,consumer_local_expression_kind_range,copied_array_string_offsets,legacy_string_array_retry,struct_parameter_array_abi,parameter_json_cursor_replay,compile_time_string_replace_or_math,backend_divergent_integer_wrap,opaque_program_expression_failure,compile_time_string_indexof_result,integer_wrap_proof_narrowing,selfhost_substring_invalid_window_drift,string_trim_fixture_route,compile_time_string_trim_result,duplicate_llvm_foreign_declaration,array_string_literal_second_decoder,array_string_type_only_cleanup,unbounded_array_string_index,c_dir_walk_positional_array_string_layout,duplicate_scalar_program_array_string_projection_readiness,process_args_array_string_storage_alignment_literal,process_args_without_target_projection,llvm_readonly_ref_array_string_storage_alignment_literal,readonly_ref_array_string_without_target_projection,owner_handle_array_string_without_caller_move_fact,caller_cleanup_after_owner_handle_array_string_move,owner_handle_array_string_use_after_move | tests/self_hosted/parity/one_mir_bool_logic_projection.sh#ok: GraphPlan solely owns CFG/SSA/phi; typed Bool/call rows extend both backends,tests/self_hosted/parity/one_mir_string_equality_projection.sh#ok: one layered GraphPlan executes routine-partitioned String equality in C and LLVM,tests/self_hosted/parity/one_mir_string_equality_concat_projection.sh#ok: one GraphPlan executes typed String concat/equality in C and LLVM,tests/self_hosted/parity/one_mir_string_builtin_program_projection.sh#ok: nested String builtins execute from one typed GraphPlan in C and LLVM,tests/self_hosted/parity/one_mir_string_window_builtin_projection.sh#Registry-owned String length/window calls through one scalar GraphPlan.,tests/self_hosted/parity/one_mir_string_collection_builtin_projection.sh#ok: runtime String/Array<String> expressions execute from one typed GraphPlan in C and LLVM,tests/self_hosted/parity/one_mir_string_case_math_projection.sh#Ordered three-argument call plus StringReplace/Int math in one GraphPlan.,tests/self_hosted/parity/one_mir_string_indexof_projection.sh#ok: StringIndexOf range and checked windows execute from one typed GraphPlan in C and LLVM,tests/self_hosted/parity/one_mir_string_trim_projection.sh#ok: StringTrim executes from one typed GraphPlan with one LLVM declaration owner,tests/self_hosted/parity/one_mir_string_array_index_return_projection.sh#One GraphPlan carriage of a borrowed-static Array<String> through a call,tests/self_hosted/parity/direct_mir_scalar_dir_walk_direct_call_owner.sh#C DirWalk consumes projected ArrayString fields and rejects layout drift,tests/self_hosted/parity/direct_mir_scalar_process_args_direct_call_owner.sh#Process Args consumes projected ArrayString storage alignment and rejects layout drift,tests/self_hosted/parity/direct_mir_scalar_array_string_readonly_ref_owner.sh#Read-only ArrayString parameter loads consume the carried LLVM projection and reject layout drift,tests/self_hosted/parity/direct_mir_scalar_owned_array_string_parameter_owner.sh#One last-use caller local moves into an owner-handle ArrayString parameter and C/LLVM reject use-after-move | BRIDGE | typed wrap-defined Int plus bounded nontrapping Bool and String expression DAGs over one or two routines, including ordered scalar parameters/direct calls and registry-owned ToString/ToUpper/ToLower/StringLength/Substring/SubstringWithLen/Concat/StringContains/Split/ToInt/StringReplace/StringIndexOf/StringTrim/Abs/Min/Max calls plus Array<String> length/index and the exact captured Array<String> layout; one canonical caller-frame Array<String> literal/parameter/index/borrowed-result boundary is closed; one single-block owner-handle ArrayString last-use move retires caller cleanup; arbitrary collection ownership, dynamic indices, owned aggregate returns, effectful short-circuit ordering, temporary String allocations, recursive or multiple callables, multiple or conditional owner moves, named aggregate-result moves, and general value returns remain separate rungs; unnamed borrow-tracked fresh results or literals are rejected by source semantic admission before this backend row
abi.mir_array_int_layout_projection | abi | ArrayIntLayoutProjectionId | SFAbiArrayIntLayoutProjection | SOMirAbi | src/self_hosted/compiler/direct_mir_array_int_abi_fact_owner.pgy | DirectMirArrayIntCapturedAbiReady | src/self_hosted/compiler/direct_mir_array_int_plan_owner.pgy,src/self_hosted/compiler/direct_mir_array_return_plan_owner.pgy,src/self_hosted/compiler/direct_mir_array_int_producer_fact_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_collection_owner.pgy,src/self_hosted/compiler/direct_mir_returned_array_foreach_call_abi_owner.pgy,src/self_hosted/compiler/direct_mir_array_argument_program_identity_owner.pgy,src/self_hosted/compiler/direct_mir_array_argument_plan_owner.pgy,src/self_hosted/compiler/direct_mir_collection_program_instruction_abi_owner.pgy,src/self_hosted/compiler/direct_mir_collection_program_local_plan_owner.pgy,src/self_hosted/compiler/direct_mir_collection_program_plan_owner.pgy,src/self_hosted/compiler/direct_mir_array_int_abi_projection_owner.pgy,src/self_hosted/compiler/direct_mir_array_int_emission_owner.pgy,src/self_hosted/compiler/direct_mir_array_return_emission_owner.pgy,src/self_hosted/compiler/direct_mir_array_argument_emission_owner.pgy | backend_local_array_layout,layout_id_without_row_admission,array_runtime_symbol_guess,both_backend_mappings_in_one_receipt,llvm_text_runtime_scan,implicit_runtime_link,callee_stack_pointer_return,missing_formal_parameter_receipt,post_issue_layout_mutation | tests/self_hosted/parity/one_mir_array_int_projection.sh#One admitted local Array<Int> graph drives runtime-free C and LLVM exactly,tests/self_hosted/parity/one_mir_array_return_projection.sh#The caller owns returned Array<Int> backing storage, and routine order is not,tests/self_hosted/parity/one_mir_array_argument_projection.sh#One source-produced three-routine MIR graph drives runtime-free C and LLVM.,tests/self_hosted/parity/one_mir_returned_array_foreach_projection.sh#returned-array producer receipt owns nested/sequential C/LLVM foreach,tests/self_hosted/parity/one_mir_array_param_projection.sh#collection return/parameter projection ok | CLOSED | none
abi.mir_array_string_layout_projection | abi | ArrayStringLayoutProjectionId | SFAbiArrayStringLayoutProjection | SOMirAbi | src/self_hosted/compiler/direct_mir_array_string_abi_fact_owner.pgy | DirectMirArrayStringCapturedAbiReady | src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_string_collection_owner.pgy,src/self_hosted/compiler/direct_mir_array_string_abi_projection_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_typed_c_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_typed_llvm_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_array_string_abi_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_array_string_abi_projection_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_array_string_mutation_projection_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_string_collection_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_process_args_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_array_string_storage_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_dir_walk_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_collection_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_process_args_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_storage_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_join_materialization_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_readonly_ref_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_program_llvm_emission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_signature_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_payload_enum_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_c_array_string_value_result_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_value_result_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_fact_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_admission_owner.pgy,src/self_hosted/compiler/direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy | backend_local_array_layout,layout_id_without_row_admission,array_runtime_symbol_guess,backend_string_element_reconstruction,capacity_as_length,post_issue_layout_mutation,scalar_program_preamble_layout_literal,complete_cross_family_row_acceptance,c_dir_walk_positional_array_string_layout,duplicate_scalar_program_array_string_projection_readiness,process_args_array_string_storage_alignment_literal,process_args_without_target_projection,llvm_readonly_ref_array_string_storage_alignment_literal,readonly_ref_array_string_without_target_projection,owner_handle_array_string_without_caller_move_fact,caller_cleanup_after_owner_handle_array_string_move,owner_handle_array_string_use_after_move | tests/self_hosted/parity/one_mir_mixed_collection_foreach_projection.sh#one typed receipt owns mixed Int/String foreach C/LLVM execution,tests/self_hosted/parity/direct_mir_scalar_bool_two_array_string_two_array_int_value_result_owner.sh#one carried target projection owns scalar preamble and value-result C/LLVM storage plus complete cross-family row rejection,tests/self_hosted/parity/one_mir_string_collection_builtin_projection.sh#target-projected scalar storage executes String/Array<String> builtins in C and LLVM,tests/self_hosted/parity/direct_mir_scalar_dir_walk_direct_call_owner.sh#C DirWalk consumes projected ArrayString fields and rejects layout drift,tests/self_hosted/parity/direct_mir_scalar_process_args_direct_call_owner.sh#Process Args consumes projected ArrayString storage alignment and rejects layout drift,tests/self_hosted/parity/direct_mir_scalar_array_string_readonly_ref_owner.sh#Read-only ArrayString parameter loads consume the carried LLVM projection and reject layout drift,tests/self_hosted/parity/direct_mir_scalar_owned_array_string_parameter_owner.sh#One last-use caller local moves into an owner-handle ArrayString parameter and C/LLVM reject use-after-move | BRIDGE | local-literal foreach, scalar-program collection preamble and value-result storage, LLVM StringJoin, C DirWalk, process Args, the LLVM read-only parameter load, and one single-block last-use owner-handle caller move consume admitted target projections; multiple or conditional owned-parameter moves, fresh-result or literal moves, owned return, cleanup, and remaining expression materializers still require consumer migration
```
<!-- END sot-owner-spine-registry -->

## Active callable-carriage evidence — 2026-08-27

`selfhost.semantic_artifact_admission` remains `ACTIVE`; no census or progress
promotion is implied. Native formal parameters now own a parser declaration
SyntaxNodeId distinct from `TypeId`; semantic symbols, MIR routine rows, and
persisted expression leaves carry that exact ID and ordinal. Declared callable
identity joins admitted function and intent signature facts once, and the final
self-C environment publishes the corresponding declaration-ID key without a
name fallback. Missing/crossed formal, function, or intent identity fails before
artifact publication. The focused executable ratchet is
`tests/self_hosted/parity/intent_typed_outcome_compensation_owner.sh`; remote
replacement CI is still required before any status change.

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
src/self_hosted/debug/source_location_fact_owner.pgy | DebugSourceLocationFactsFromParserBuild | selfhost.debug_session | local_view
src/self_hosted/mir/domain_runtime_assignment_fact_owner.pgy | SelfMirDomainRuntimeAssignmentsFromFacts | semantic.domain_runtime_assignment | bridge
src/self_hosted/mir_lower/domain_runtime_participant_role_fact_owner.pgy | MirDomainParticipantRoleFactsFromTable | semantic.domain_runtime_assignment | local_view
src/self_hosted/mir_lower/domain_runtime_assignment_fact_owner.pgy | MirDomainRuntimeAssignmentFactsFromDocument | semantic.domain_runtime_assignment | projection
src/self_hosted/parser/expression_fact_owner.pgy | ParserExpressionFact | selfhost.expression_graph | local_view
src/self_hosted/parser/expression_scalar_fact_owner.pgy | ParserExpressionScalarFactContractReady | selfhost.expression_graph | local_view
src/self_hosted/mir/program_fact_owner.pgy | SelfMirProgramFacts | mir.execution_graph | projection
src/self_hosted/mir/local_ref_fact_owner.pgy | SelfMirInstructionLocalRefRows | mir.execution_graph | projection
src/self_hosted/mir/declaration_fact_owner.pgy | SelfMirDeclarationRows | mir.execution_graph | projection
src/self_hosted/mir/destructure_fact_owner.pgy | SelfMirDestructureFactRowsAttach | mir.execution_graph | projection
src/self_hosted/mir/destructure_type_fact_owner.pgy | SelfMirDestructureTypeFactRows | semantic.destructure_binding_type | projection
src/self_hosted/mir/loop_reachability_fact_owner.pgy | SelfMirLoopReachabilityForBlock | hir.typed_control_flow | projection
src/self_hosted/mir/parallel_capture_fact_owner.pgy | SelfMirParallelCaptureRows | mir.execution_graph | projection
src/self_hosted/mir/expression_graph_fact_owner.pgy | SelfMirExpressionGraphRows | mir.execution_graph | projection
src/self_hosted/mir/runtime_value_call_abi_fact_owner.pgy | SelfMirRuntimeValueCallAbiRows | abi.runtime_call_rows | projection
src/self_hosted/mir/intent_execution_fact_owner.pgy | MirIntentExecutionPlanFromFacts | mir.execution_graph | projection
src/self_hosted/mir/expression_fact_owner.pgy | SelfMirExpressionKind | mir.execution_graph | bridge
src/self_hosted/mir/match_fact_owner.pgy | SelfMirMatchFactRows | mir.execution_graph | projection
src/self_hosted/mir_lower/parallel_capture_fact_owner.pgy | MirParallelCaptureFactsReady | mir.execution_graph | projection
src/self_hosted/mir_lower/machine_layer_fact_owner.pgy | MirMachineLayerFactsReady | semantic.machine_layer_transition | projection
src/self_hosted/dir/domain_graph_fact_owner.pgy | SelfDirDomainGraphFactsFromArtifact | dir.domain_graph | bridge
src/self_hosted/dir/zone_state_row_fact_owner.pgy | SelfDirZoneStateRowsFromArtifact | dir.domain_graph | bridge
src/self_hosted/dir/intent_fact_owner.pgy | SelfDirIntentFactsFromArtifact | dir.domain_graph | bridge
src/self_hosted/dir/intent_mode_fact_owner.pgy | SelfDirIntentModeFactsFromArtifact | dir.domain_graph | bridge
src/self_hosted/dir/intent_priority_fact_owner.pgy | SelfDirIntentPriorityFactsFromArtifact | dir.domain_graph | bridge
src/self_hosted/dir/intent_step_fact_owner.pgy | SelfDirIntentStepFromArtifact | dir.domain_graph | bridge
src/self_hosted/dir/intent_step_provenance_fact_owner.pgy | SelfDirIntentStepResolvedProvenance | dir.domain_graph | bridge
src/self_hosted/dir/intent_step_clause_fact_owner.pgy | SelfDirIntentStepClauseFactsFromArtifact | selfhost.intent_declaration_rows | bridge
src/self_hosted/semantic/ast_intent_call_fact_owner.pgy | SemanticAstIntentCallFromGraph | selfhost.intent_declaration_rows | bridge
src/self_hosted/semantic/ast_intent_transition_fact_owner.pgy | SemanticAstIntentTransitionFactsFromArtifact | selfhost.intent_declaration_rows | bridge
src/self_hosted/mir/domain_topology_fact_owner.pgy | SelfMirDomainTopologyFactsFromSelfDir | dir.domain_graph | projection
src/self_hosted/mir_lower/domain_topology_fact_owner.pgy | MirDomainTopologyFactsFromDocument | dir.domain_graph | projection
src/self_hosted/mir_lower/expression_graph_fact_owner.pgy | MirExpressionGraphFactsForArtifact | mir.execution_graph | projection
src/self_hosted/mir_lower/intent_execution_plan_fact_owner.pgy | MirIntentExecutionPlanFromDocument | mir.execution_graph | projection
src/self_hosted/mir_lower/program_routine_block_fact_owner.pgy | MirProgramRoutineBlockCaptureWithin | mir.execution_graph | local_view
src/self_hosted/mir_lower/routine_instruction_use_fact_owner.pgy | BuildMirRoutineInstructionUseFacts | mir.execution_graph | local_view
src/self_hosted/mir_lower/routine_result_definition_fact_owner.pgy | MirRoutineFactIndexUniqueResultDefinition | mir.execution_graph | local_view
src/self_hosted/mir_lower/match_json_fact_owner.pgy | MirMatchPatternCount | mir.execution_graph | local_view
src/self_hosted/mir_lower/match_binding_local_fact_owner.pgy | MirMatchBindingLocalFacts | mir.execution_graph | local_view
src/self_hosted/mir_lower/iteration_type_fact_owner.pgy | MirIterationTypeFacts | selfhost.iteration_type_verdict | local_view
src/self_hosted/mir_lower/phi_fact_owner.pgy | MirRoutinePhiFactsReady | mir.execution_graph | local_view
src/self_hosted/mir_lower/routine_definition_dominance_fact_owner.pgy | MirRoutineDefinitionStrictlyPrecedes | mir.execution_graph | local_view
src/self_hosted/mir_lower/phi_predecessor_binding_fact_owner.pgy | MirPhiPredecessorBindingFactFromOwners | mir.execution_graph | local_view
src/self_hosted/mir_lower/latest_local_value_fact_owner.pgy | MirRoutineLatestDominatingLocalValueMatches | mir.execution_graph | local_view
src/self_hosted/air/mir_nested_cfg_certificate_fact_owner.pgy | DirectMirNestedCfgCertificateFactFromIndex | air.mir_cfg_certificate | projection
src/self_hosted/air/mir_cfg_certificate_fact_owner.pgy | DirectMirCfgCertificateIdentityDigest | air.mir_cfg_certificate | local_view
src/self_hosted/air/mir_loop_cfg_certificate_fact_owner.pgy | DirectMirLoopCfgCertificateFactFromIndex | air.mir_cfg_certificate | projection
src/self_hosted/air/mir_range_cfg_certificate_fact_owner.pgy | DirectMirRangeCfgCertificateFactFromIndex | air.mir_cfg_certificate | projection
src/self_hosted/air/mir_break_cfg_certificate_fact_owner.pgy | DirectMirBreakCfgCertificateFactFromIndex | air.mir_cfg_certificate | projection
src/self_hosted/compiler/direct_mir_cfg_entry_fact_owner.pgy | DirectMirCfgEntryFactFromOwners | mir.execution_graph | local_view
src/self_hosted/compiler/direct_mir_cfg_shape_fact_owner.pgy | DirectMirCfgShapeFactsFromOwners | projection.direct_mir_cfg_plan | projection
src/self_hosted/compiler/direct_mir_cfg_plan_fact_owner.pgy | DirectMirCfgPlanReady | projection.direct_mir_cfg_plan | local_view
src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_fact_owner.pgy | DirectMirScalarCfgCollectionPlan | projection.direct_mir_scalar_cfg_graph_plan | projection
src/self_hosted/compiler/direct_mir_collection_local_context_fact_owner.pgy | DirectMirCollectionValueOriginSet | projection.direct_mir_scalar_cfg_graph_plan | projection
src/self_hosted/compiler/direct_mir_collection_program_route_fact_owner.pgy | DirectMirCollectionProgramRouteFactFromAdmitted | projection.direct_mir_collection_program_plan | local_view
src/self_hosted/compiler/direct_mir_collection_program_routine_fact_owner.pgy | DirectMirCollectionProducerRoutineFact | projection.direct_mir_collection_program_plan | local_view
src/self_hosted/compiler/direct_mir_collection_program_edge_fact_owner.pgy | DirectMirCollectionProgramEdgeSet | projection.direct_mir_collection_program_plan | local_view
src/self_hosted/compiler/direct_mir_loop_cfg_plan_fact_owner.pgy | DirectMirLoopCfgPlanFactReady | projection.direct_mir_cfg_plan | local_view
src/self_hosted/compiler/direct_mir_scalar_cfg_range_fact_owner.pgy | DirectMirScalarCfgRangeIterationFactReady | projection.direct_mir_scalar_cfg_graph_plan | local_view
src/self_hosted/compiler/direct_mir_scalar_program_route_fact_owner.pgy | DirectMirScalarProgramRouteFact | projection.direct_mir_scalar_cfg_program_extension | projection
src/self_hosted/compiler/direct_mir_scalar_program_expression_fact_owner.pgy | DirectMirScalarProgramExpressionSetReady | projection.direct_mir_scalar_cfg_program_extension | projection
src/self_hosted/compiler/direct_mir_scalar_program_direct_call_fact_owner.pgy | DirectMirScalarProgramDirectCallFact | projection.direct_mir_scalar_cfg_program_extension | local_view
src/self_hosted/compiler/direct_mir_scalar_program_leaf_identity_fact_owner.pgy | DirectMirScalarProgramLeafIdentityFactFromOwners | projection.direct_mir_scalar_cfg_program_extension | local_view
src/self_hosted/compiler/direct_mir_scalar_program_runtime_abi_fact_owner.pgy | DirectMirScalarProgramRuntimeAbiFact | projection.direct_mir_scalar_cfg_program_extension | projection
src/self_hosted/compiler/direct_mir_scalar_program_indexed_assignment_fact_owner.pgy | DirectMirScalarProgramIndexedAssignmentFactSetFromOwners | projection.direct_mir_scalar_cfg_program_extension | projection
src/self_hosted/compiler/direct_mir_scalar_program_string_literal_fact_owner.pgy | DirectMirScalarProgramStringLiteralContent | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_expression_fact_owner.pgy | DirectMirScalarProgramLlvmStringExpression | projection.direct_mir_scalar_cfg_program_extension | projection
src/self_hosted/mir_lower/generic_specialization_fact_owner.pgy | MirCodegenGenericSpecializationFacts | mir.generic_specialization | projection
src/self_hosted/mir_lower/resource_flow_fact_owner.pgy | MirResourceFlowFacts | semantic.resource_flow_universe | projection
src/self_hosted/mir_lower/loop_flow_fact_owner.pgy | MirLoopFlowFacts | semantic.loop_flow_summary | projection
src/self_hosted/mir_lower/resource_runtime_abi_fact_owner.pgy | MirResourceRuntimeRowFactReady | abi.runtime_call_rows | local_view
src/self_hosted/mir_lower/abi_layout_fact_owner.pgy | MirCapturedAbiLayoutFactReady | abi.layout_rows | local_view
src/self_hosted/mir_lower/declaration_method_contract_fact_owner.pgy | MirDeclarationMethodContractFactFromBounds | selfhost.action_contract | bridge
src/self_hosted/codegen/abi_layout/enum_abi_value_fact_owner.pgy | EnumAbiValueFactContractReady | abi.layout_rows | projection
src/self_hosted/mir_lower/assignment_binding_mode_fact_owner.pgy | MirAssignmentBindingModesMatchSemanticFacts | selfhost.assignment_statement_routing | local_view
src/self_hosted/mir/runtime_call_abi_fact_owner.pgy | SelfMirRuntimeCallAbiRows | abi.runtime_call_rows | projection
src/self_hosted/compiler/runtime_call_abi_structured_fact_owner.pgy | CompilerRuntimeCallAbiFactFromRowInput | abi.runtime_call_rows | projection
src/self_hosted/compiler/target_projection_fact_owner.pgy | CompilerTargetProjectionFactFromOwner | target.capability_profile | projection
src/self_hosted/hir/ast_text_row_fact_owner.pgy | CodegenAstTextRowFactInput | hir.typed_control_flow | bridge
src/self_hosted/hir/ast_source_module_fact_owner.pgy | AstSourceModuleFactsFromTopLevelPaths | parser.syntax_provenance | projection
src/self_hosted/semantic/ast_generic_parameter_fact_owner.pgy | SemanticAstGenericParameterRowsFromNode | selfhost.function_declaration_rows | local_view
src/self_hosted/semantic/ast_signature_type_expression_fact_owner.pgy | SemanticAstSignatureTypeExpressionFacts | selfhost.function_declaration_rows | local_view
src/self_hosted/semantic/try_expression_fact_owner.pgy | SemanticTryOperand | selfhost.expression_graph | bridge
src/self_hosted/semantic/ast_expression_graph_fact_owner.pgy | SemanticExpressionGraphFacts | selfhost.expression_graph | bridge
src/self_hosted/semantic/ast_expression_function_table_fact_owner.pgy | SemanticAstExpressionFunctionTableFacts | selfhost.function_declaration_rows | projection
src/self_hosted/semantic/ast_intent_action_call_fact_owner.pgy | SemanticAstIntentActionCallFact | selfhost.intent_declaration_rows | local_view
src/self_hosted/semantic/delimited_range_fact_owner.pgy | SemanticDelimitedRangeFacts | semantic.symbol_type_graph | local_view
src/self_hosted/semantic/expression_operator_fact_owner.pgy | SemanticTopLevelOperatorFacts | selfhost.expression_surface | local_view
src/self_hosted/semantic/expression_cast_fact_owner.pgy | SemanticOuterCastTargetType | selfhost.expression_surface | bridge
src/self_hosted/semantic/ast_generic_specialization_fact_owner.pgy | SemanticAstGenericSpecializationFactsFromBody | selfhost.expression_surface | projection
src/self_hosted/semantic/ast_destructure_binding_fact_owner.pgy | SemanticAstDestructureBindingFactsFromArtifact | selfhost.local_binding_statement_routing | projection
src/self_hosted/semantic/ast_function_scope_fact_owner.pgy | SemanticAstFunctionScopeFacts | selfhost.node_kind_surface | local_view
```
src/self_hosted/air/mir_identity_match_cfg_certificate_fact_owner.pgy | DirectMirIdentityMatchCfgCertificateFact | air.mir_cfg_certificate | projection
src/self_hosted/air/mir_option_match_graph_fact_owner.pgy | DirectMirOptionMatchGraphAt | air.mir_cfg_certificate | projection
src/self_hosted/compiler/direct_mir_aggregate_value_flow_fact_owner.pgy | DirectMirAggregateValueFlowFact | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_array_argument_graph_fact_owner.pgy | DirectMirArrayArgumentMainGraphFact | projection.direct_mir_array_argument_plan | projection
src/self_hosted/compiler/direct_mir_array_bool_abi_fact_owner.pgy | DirectMirArrayBoolCapturedAbiReady | abi.layout_rows | projection
src/self_hosted/compiler/direct_mir_array_captured_abi_fact_owner.pgy | DirectMirArrayCapturedAbiReady | abi.layout_rows | projection
src/self_hosted/compiler/direct_mir_array_int_graph_fact_owner.pgy | DirectMirArrayIntGraphFromExpressionFact | projection.direct_mir_array_int_program | projection
src/self_hosted/compiler/direct_mir_array_int_producer_fact_owner.pgy | DirectMirArrayIntProducerFact | projection.direct_mir_array_int_program | projection
src/self_hosted/compiler/direct_mir_array_return_graph_fact_owner.pgy | DirectMirArrayReturnProducerName | projection.direct_mir_array_return_plan | projection
src/self_hosted/compiler/direct_mir_array_string_literal_fact_owner.pgy | DirectMirArrayStringLiteralFact | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_collection_program_graph_fact_owner.pgy | DirectMirCollectionNamePairGraphFact | projection.direct_mir_collection_program_plan | projection
src/self_hosted/compiler/direct_mir_composite_intent_program_graph_fact_owner.pgy | DirectMirCompositeIntentProgramGraphFactFromAdmitted | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_composite_intent_program_route_fact_owner.pgy | DirectMirCompositeIntentProgramRouteFactFromAdmitted | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_constructed_array_member_main_graph_fact_owner.pgy | DirectMirConstructedArrayMemberMainGraphFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_constructed_array_member_method_graph_fact_owner.pgy | DirectMirConstructedArrayMemberMethodGraphFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_constructed_array_member_signature_fact_owner.pgy | DirectMirConstructedArrayMemberSignatureFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_constructed_array_member_specialization_fact_owner.pgy | DirectMirConstructedArrayMemberSpecializationFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_constructed_generic_member_declaration_fact_owner.pgy | DirectMirConstructedGenericMemberDeclarationFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_constructed_generic_member_main_graph_fact_owner.pgy | DirectMirConstructedGenericMemberMainGraphFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_constructed_generic_member_method_graph_fact_owner.pgy | DirectMirConstructedGenericMemberMethodGraphFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_constructed_generic_member_signature_fact_owner.pgy | DirectMirConstructedGenericMemberSignatureFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_constructed_generic_member_specialization_fact_owner.pgy | DirectMirConstructedGenericMemberSpecializationFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_enum_value_match_plan_fact_owner.pgy | DirectMirEnumValueMatchPlanFact | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_generic_member_signature_fact_owner.pgy | DirectMirGenericMemberSignatureFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_generic_routine_signature_fact_owner.pgy | DirectMirGenericRoutineSignatureFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_generic_specialization_fact_owner.pgy | DirectMirGenericSpecializationFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_generic_struct_value_flow_abi_fact_owner.pgy | DirectMirGenericStructValueFlowAbiFact | abi.layout_rows | projection
src/self_hosted/compiler/direct_mir_generic_struct_value_flow_graph_fact_owner.pgy | DirectMirGenericStructValueFlowGraphFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_inferred_generic_member_declaration_fact_owner.pgy | DirectMirInferredGenericMemberDeclarationFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_inferred_generic_member_graph_fact_owner.pgy | DirectMirInferredGenericMemberGraphFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_inferred_generic_member_host_kind_fact_owner.pgy | DirectMirInferredGenericMemberHostKindFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_inferred_generic_member_representation_fact_owner.pgy | DirectMirInferredGenericMemberRepresentationFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_inferred_generic_member_specialization_fact_owner.pgy | DirectMirInferredGenericMemberSpecializationFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_inferred_generic_nominal_abi_fact_owner.pgy | DirectMirInferredGenericNominalAbiFact | abi.layout_rows | projection
src/self_hosted/compiler/direct_mir_inferred_generic_nominal_graph_fact_owner.pgy | DirectMirInferredGenericNominalGraphFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_inferred_generic_scalar_abi_fact_owner.pgy | DirectMirInferredGenericScalarAbiFact | abi.layout_rows | projection
src/self_hosted/compiler/direct_mir_inferred_generic_scalar_graph_fact_owner.pgy | DirectMirInferredGenericScalarGraphFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_inferred_generic_specialization_fact_owner.pgy | DirectMirInferredGenericSpecializationFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_legacy_intent_program_graph_fact_owner.pgy | DirectMirLegacyIntentProgramGraphFactFromAdmitted | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_legacy_intent_program_route_fact_owner.pgy | DirectMirLegacyIntentProgramRouteFactFromAdmitted | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_mixed_lane_generic_specialization_fact_owner.pgy | DirectMirMixedLaneGenericSpecializationFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_nested_intent_program_graph_fact_owner.pgy | DirectMirNestedIntentProgramGraphFactFromAdmitted | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_nested_intent_program_route_fact_owner.pgy | DirectMirNestedIntentProgramRouteFactFromAdmitted | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_nominal_declaration_abi_fact_owner.pgy | DirectMirNominalDeclarationAbiFact | abi.layout_rows | projection
src/self_hosted/compiler/direct_mir_nominal_literal_declaration_fact_owner.pgy | DirectMirNominalLiteralDeclarationFact | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_nominal_literal_graph_fact_owner.pgy | DirectMirNominalLiteralGraphFact | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_nominal_literal_route_fact_owner.pgy | DirectMirNominalLiteralRouteFact | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_option_match_abi_fact_owner.pgy | DirectMirOptionMatchAbiFact | abi.layout_rows | projection
src/self_hosted/compiler/direct_mir_option_match_cfg_plan_fact_owner.pgy | DirectMirOptionMatchCfgPlan | projection.direct_mir_option_match_cfg_plan | projection
src/self_hosted/compiler/direct_mir_option_struct_value_flow_abi_fact_owner.pgy | DirectMirOptionStructValueFlowAbiFact | abi.layout_rows | projection
src/self_hosted/compiler/direct_mir_option_struct_value_flow_graph_fact_owner.pgy | DirectMirOptionStructValueFlowGraphFact | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_result_int_abi_fact_owner.pgy | DirectMirResultIntAbiFact | abi.layout_rows | projection
src/self_hosted/compiler/direct_mir_role_operator_declaration_fact_owner.pgy | DirectMirRoleOperatorMethodDeclarationFact | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_routine_param_fact_owner.pgy | DirectMirRoutineParamFact | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_routine_parameter_set_fact_owner.pgy | DirectMirRoutineParameterSetFact | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_routine_signature_fact_owner.pgy | DirectMirRoutineSignatureFact | mir.generic_specialization | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_mutating_fact_owner.pgy | DirectMirScalarCfgArrayIntProducerFact | projection.direct_mir_array_int_program | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_fact_owner.pgy | DirectMirScalarCfgArrayIntPopFact | projection.direct_mir_array_int_program | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_fact_owner.pgy | DirectMirScalarCfgArrayIntProgramFact | projection.direct_mir_array_int_program | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_fact_owner.pgy | DirectMirScalarCfgArrayIntReadAccessFact | projection.direct_mir_array_int_program | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_fact_owner.pgy | DirectMirScalarCfgArrayIntDerivedCollectionFact | projection.direct_mir_array_int_program | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_static_mutation_fact_owner.pgy | DirectMirScalarCfgArrayIntStaticMutationFact | projection.direct_mir_array_int_program | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_transform_route_fact_owner.pgy | DirectMirScalarCfgArrayIntTransformRouteFact | projection.direct_mir_array_int_program | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_array_length_fact_owner.pgy | DirectMirScalarCfgArrayLengthGraphFact | projection.direct_mir_scalar_cfg_graph_plan | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_c_collection_operation_fact_owner.pgy | DirectMirScalarCfgCCollectionOperationEmission | projection.direct_mir_scalar_cfg_graph_plan | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_fact_owner.pgy | DirectMirScalarCfgForEachFact | projection.direct_mir_scalar_cfg_graph_plan | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy | DirectMirScalarCfgGraphPlan | projection.direct_mir_scalar_cfg_graph_plan | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_collection_operation_fact_owner.pgy | DirectMirScalarCfgLlvmCollectionOperationEmission | projection.direct_mir_scalar_cfg_graph_plan | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_program_extension_fact_owner.pgy | DirectMirScalarCfgProgramExtensionFact | projection.direct_mir_scalar_cfg_graph_plan | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_routine_partition_fact_owner.pgy | DirectMirScalarCfgRoutinePartitionFact | projection.direct_mir_scalar_cfg_graph_plan | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_fact_owner.pgy | DirectMirScalarCfgStringArrayIndexFact | projection.direct_mir_string_array_push | projection
src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_source_fact_owner.pgy | DirectMirScalarCfgStringArrayGraphAt | projection.direct_mir_string_array_push | projection
src/self_hosted/compiler/direct_mir_scalar_program_array_string_boundary_fact_owner.pgy | DirectMirScalarProgramArrayStringBoundaryFact | projection.direct_mir_scalar_cfg_program_extension | projection
src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_fact_owner.pgy | DirectMirScalarProgramOwnedArrayStringMoveFact | projection.direct_mir_scalar_cfg_program_extension | projection
src/self_hosted/compiler/direct_mir_scalar_program_builtin_signature_fact_owner.pgy | DirectMirScalarProgramBuiltinSignatureFact | projection.direct_mir_scalar_cfg_program_extension | projection
src/self_hosted/compiler/direct_mir_scalar_program_callable_fact_owner.pgy | DirectMirScalarCfgProgramCallableFact | projection.direct_mir_scalar_cfg_program_extension | projection
src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_fact_owner.pgy | DirectMirScalarProgramExpressionKindFact | projection.direct_mir_scalar_cfg_program_extension | projection
src/self_hosted/compiler/direct_mir_scalar_program_two_int_nominal_abi_fact_owner.pgy | DirectMirScalarProgramTwoIntNominalAbiFact | abi.layout_rows | projection
src/self_hosted/compiler/direct_mir_scalar_program_array_int_value_result_fact_owner.pgy | DirectMirScalarProgramArrayIntValueResultFact | abi.layout_rows | projection
src/self_hosted/compiler/direct_mir_scalar_program_logical_record_fact_owner.pgy | DirectMirScalarProgramLogicalRecordFact | semantic.nominal_field_kind | projection
src/self_hosted/compiler/direct_mir_scalar_program_referenced_enum_fact_owner.pgy | DirectMirScalarProgramReferencedEnumFactFromAdmitted | selfhost.enum_declaration_rows | projection
src/self_hosted/compiler/direct_mir_scalar_program_payload_free_enum_fact_owner.pgy | DirectMirScalarProgramPayloadFreeEnumFactFromAdmitted | selfhost.enum_declaration_rows | projection
src/self_hosted/compiler/direct_mir_struct_argument_graph_fact_owner.pgy | DirectMirStructArgumentGraphFact | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_struct_value_flow_abi_fact_owner.pgy | DirectMirStructValueFlowAbiFact | abi.layout_rows | projection
src/self_hosted/compiler/direct_mir_struct_value_flow_graph_fact_owner.pgy | DirectMirStructValueFlowGraphFact | mir.execution_graph | projection
src/self_hosted/compiler/direct_mir_three_routine_classification_fact_owner.pgy | DirectMirThreeRoutineClassificationFact | mir.execution_graph | projection
src/self_hosted/mir/instruction_abi_receipt_fact_owner.pgy | SelfMirInstructionAbiReceiptRows | abi.layout_rows | projection
src/self_hosted/mir/nominal_abi_layout_fact_owner.pgy | SelfMirNominalAbiLayoutRows | abi.layout_rows | projection
src/self_hosted/mir/option_nominal_abi_layout_fact_owner.pgy | SelfMirOptionNominalAbiLayoutRows | abi.layout_rows | projection
src/self_hosted/mir_lower/routine_instruction_match_fact_owner.pgy | MirRoutineInstructionMatchFacts | mir.execution_graph | projection
src/self_hosted/semantic/ast_expression_identity_fact_owner.pgy | SemanticExpressionIdentityRows | selfhost.call_target_identity | projection
<!-- END sot-derived-fact-registry -->

The 2026-08-10 scalar-route diagnostic closure makes
`DirectMirScalarProgramRouteAdmission` the single consumer-facing result of
route selection. It contains either the ready immutable route or one stable
owner/stage/routine/parameter rejection receipt. The callable-envelope owner
produces its assessment once, the route admission preserves it, and the
terminal dispatcher consumes it only after earlier route families decline.
Reopening the MIR at the terminal boundary or restoring the generic
`terminal multi-routine graph is unsupported` message is forbidden.

The 2026-08-11 zero-parameter callable closure reuses the existing
`DirectMirRoutineSignatureFact` and program callable inventory; it introduces
no parallel signature authority. An empty parameter range is preserved and
emitted as `(void)` in C and `()` in LLVM, while zero-argument call-expression
support remains a separate admission decision. GraphPlan v30 corrected the
first scalar-only version: `param_count == 0` now consumes the same admitted
logical-record return inventory as every other signature instead of
short-circuiting it. The focused positive carries both scalar and logical-record
zero-parameter returns; the paired called fixture still rejects an unsupported
zero-argument expression without artifact publication.

The 2026-08-11 GraphPlan v30/v31 logical ordered-record closure makes
`DirectMirScalarProgramLogicalRecordFact` one declaration-keyed inventory of
all callable-referenced ordered logical-record identities. Its semantic
authority is `semantic.nominal_field_kind` through the admitted declaration
field index, cross-sealed by explicit absence of a persisted instruction ABI
layout. Equal field shapes never merge declaration identities, and an
unreferenced same-shape declaration is not selected. The fact carries ordered
field identity, not size, alignment, or offsets. C and LLVM materialize only
bounded target-local value carriers from those ordinals; neither target may
infer a physical interoperability layout.

GraphPlan v31 additionally persists routine `parameter_carriages` through the
partition. A logical-record readonly parameter is admitted only when the exact
signature row says `readonly-ref`, `resource=none`, `pass=indirect`, and ABI
absent. C consumes it as a const pointer; LLVM consumes it as `ptr` plus a
bounded aggregate load. Direct-call admission requires an addressable local or
readonly formal and rejects temporary-by-address and by-value coercion. The
focused executable gate proves three distinct logical-record identities,
zero-parameter record construction, readonly parameter calls, member parity,
and C/LLVM execution, while field-order, forged-layout, cross-identity, and
carriage mutations fail before artifact publication. Before GraphPlan v32, the
fixed production canary passed the former routine 57 readonly-carriage boundary
and failed closed at routine 70, whose coherent unsupported shape was a `Void`
return plus an `Array<String>` value-result parameter.

GraphPlan v34 generalizes the same fact owner without adding a physical-layout
authority. Return and parameter types are roots; scalar fields are terminal,
and logical-record fields join only to declaration identities already selected
in dependency-first order. Field counts are variable. A cycle, missing
dependency, cross-identity field, or ABI-present declaration remains outside
the projection and fails at its referenced callable envelope, while an
unrelated unsupported declaration no longer poisons the complete optional
subset. `DirectMirScalarCfgPhiOperationKind` owns the target-neutral
`PhiValue` row used by Bool and admitted logical-record joins; C and LLVM keep
that join memory-local and do not invent an integer, String, or target layout.
`tests/self_hosted/parity/direct_mir_scalar_recursive_logical_record_phi_owner.sh`
pins the five-field nested positive and the cycle, cross-identity, and phi-type
negative cases. Its last-consumer ratchet also forbids the fact-free callable
readiness wrapper after the fact-bearing admission has succeeded.

GraphPlan v32 closes that routine-70 shape without widening general collection
ownership. `DirectMirScalarProgramArrayStringAbiFact` now carries exact
value-result routine/parameter identity and the captured four-field storage
layout. The routine signature still owns carriage, and the C/LLVM signature,
addressable call, copy-in, push, and every return-edge copy-out owners consume
the joined fact. The focused recursive fixture observes the mutation in the
caller after early and fallthrough Void returns; layout and carriage mutations
fail before publication. The fixed production canary now passes routine 70 and
fails closed at routine 71 `JsonScalarFieldValues() -> Array<String>`, leaving
owned Array<String> return semantics as the next separate rung.

GraphPlan v33 closes that routine-71 boundary without treating it as the
borrowed-static array result or the value-result parameter contract. The exact
two-String signature owns return identity, while the existing ArrayString ABI
owner records owned-return presence and validates every Array<String> def and
return capture in the same instruction scan. C/LLVM return values transfer the
carrier into caller locals; only the entrypoint cleanup owner releases those
owned carriers. The focused executable gate proves empty and populated returns
and rejects both a forged layout and a return row relabeled as a definition.
The fixed canary now fails closed at routine 80 `BuildMirDocumentFactIndex`, so
nominal document-index representation remains a separate rung.

GraphPlan v35 removes the exact two-String parameter fallback from that same
owned-return row. The parameter list is nonempty and every parameter must be a
direct value-carried Bool, Int, or String with no resource or ABI receipt. The
return still joins the exact `DirectMirScalarProgramArrayStringAbiFact`; C/LLVM
transfer and caller cleanup are unchanged. The focused gate now executes both
two- and four-parameter callables and rejects the old fixed-arity source path.
This does not authorize local `ArrayPush`, collection parameters, or a nominal
record containing Array fields.

GraphPlan v36 keeps the declaration-keyed logical-record fact as a projection
of `semantic.nominal_field_kind` while allowing ArrayInt and ArrayString as
terminal field identities. Physical authority stays in `abi.layout_rows`:
`DirectMirScalarProgramLogicalRecordCollectionAbiReady` joins those existing
facts, and the target emitters consume the join rather than copying offsets into
the logical-record inventory. Final typed readiness additionally requires the
ArrayInt fact to be present, so a canonical-empty receipt cannot authorize an
ArrayInt local. The focused C/LLVM gate covers exact construction/member order
and rejects same-shape identity and both collection-layout mutations.

GraphPlan v37/v38 preserve the same ABI owners while admitting direct
value-carried ArrayInt and ArrayString parameters. `DirectMirRoutineSignatureFact`
owns `value` versus `value-result`; the program collection facts own only the
physical layout, and their value-result identity arrays still contain only
copy-in/copy-out rows. The ArrayString caller-frame boundary is a separate
projection: `DirectMirScalarProgramArrayStringBoundarySignatureReady` restricts
it to the exact one-parameter `Array<String> value -> String` signature, and
both extension sealing and readiness consume that one predicate. The focused
C/LLVM gates reject layout, carriage, and pass-shape drift without publishing
an artifact.

GraphPlan v39 extends the same declaration-keyed logical-record projection to a
nested `ProgramIndex -> ReachabilityRows -> Array<Bool>` value. The physical
ArrayBool layout remains owned by `abi.layout_rows`; the logical-record fact
only records field identity and dependency order. Constructor readiness and
both target emitters consume one normalized argument-row owner so the
`left/right` encoding for one/two arguments and the nary encoding for larger
calls cannot diverge. Mixed encodings fail closed. The focused gate includes an
unreferenced same-shape declaration and rejects identity, field-order, layout,
and pass-shape drift.

GraphPlan v40 admits direct ArrayInt returns without adding a fact family or
changing the v36 carrier schema. `DirectMirRoutineSignatureFact` owns return
identity; `DirectMirScalarProgramArrayIntValueResultFact` joins matching
definition, parameter, and return rows to the one physical ArrayInt receipt.
Only formal `value-result` carriage contributes copy-in/copy-out identities.
The three-routine C/LLVM gate stores the returned carrier in a caller local,
passes it by value, and rejects return-layout, return-kind, and parameter-layout
mutations before publication. The fixed canary now passes routine 229 and fails
closed at routine 256 on the still-unsupported direct `Long` return.

GraphPlan v41 changes the carrier schema to v37 because it adds stable normalized
expression kind 53 for a canonical Long literal. The type and target width are
not new authorities: `abi_layout_row_owner.pgy` already maps Long to C
`long long` and LLVM `i64`. The expression owner validates the source `L`
payload, stores suffix-free digits, and each backend adds only its target
spelling. ArrayInt kind 51 and ArrayBool kind 52 remain distinct and are pinned
beside Long to prevent another cross-owner collision. The focused gate compiles
and executes both targets and rejects return-type and literal-kind mutations.
The fixed canary passes routines 256/257 and next fails closed at routine 268,
whose Bool return is combined with four ArrayString value-result formals.

GraphPlan v42 keeps schema v37. `DirectMirRoutineSignatureFact` owns the exact
Bool return and parameter order; `DirectMirScalarProgramArrayStringAbiFact`
owns four distinct `(routine, parameter, digest)` copyout identities over one
physical layout. The shared callable policy admits only the historical
Void/one-copyout shape and the reached `String, Int, Int` plus four trailing
ArrayString value-result shape. C and LLVM consume those rows for pointer
signatures, copy-in, and copy-out before both early and final Bool returns. The
focused execution gate rejects return-type and fourth-carriage mutations
without artifact publication. The fixed canary now passes routine 268 and next
fails closed at routine 275 on a declaration-keyed
`MirAbiLayoutValidationSession` value-result parameter.

GraphPlan v43 keeps schema v37 and joins that reached value-result parameter to
the existing declaration-keyed logical-record fact. The signature owner admits
exactly one logical-record copyout with direct scalar value parameters and a
scalar return. The target-neutral copyout identity is consumed by direct-call
readiness as well as the C/LLVM signature, entry copy-in, parameter read, and
every-return copy-out owners. The complete ordered declaration carrier is
copied; no field list or physical layout is recreated in a backend. The focused
C/LLVM gate rejects carriage and pass-shape mutations before publication. The
fixed canary now passes routines 275 and 276 and fails closed at routine 277
`MirLowerFailClosed(String) -> Void`, leaving its Void callable and terminal
statement effects as the next separate rung.

GraphPlan v44 advances schema v37 to v38 for the stable `ProcessExit` operation.
`DirectMirScalarProgramVoidScalarCallableSignatureReady` owns the exact Void
callable shape and is consumed by both claimant-envelope and final-signature
readiness. The operation row preserves admitted statement order, while
`CompilerRuntimeCallAbiProcessExitFact` consumes the canonical runtime ABI row
`host-io|exit|int_to_noreturn`; C and LLVM project that same symbol and integer
argument contract. The focused gate independently pins stdout and exit status
7 and rejects return, carriage, and Exit-argument mutations without artifact
publication. The fixed canary passes routine 277 and next fails closed at
routine 405 `TypedAstKindTags() -> Array<Int>`, leaving its zero-parameter
populated ArrayInt return as a separate rung.

GraphPlan v45 advances schema v38 to v39 for stable expression identity 54,
the populated `Array<Int>` literal whose elements are zero-parameter direct
calls returning `Int`. The persisted graph owns element order, call-target
SyntaxNodeIds, and the canonical array spine; the callable inventory owns the
joined target and return type; the existing ArrayInt ABI receipt owns physical
storage. C and LLVM consume those facts without a tag-name table or backend MIR
reread. The focused gate pins independent ordered output and rejects missing
target, wrong target return, and storage-layout mutations before publication.
The fixed canary now passes routine 405 and next fails closed at routine 495
`CodegenAstTextNodeInventory`, leaving its mixed logical-record-array and
ArrayInt value-result Void signature as a separate rung.

GraphPlan v46 keeps schema v39. The existing logical-record inventory derives
the element declaration of canonical `Array<T>` parameters, and
`CompilerAbiNominalArrayLayoutFact` owns the compiler-internal three-field
`data,len,cap` representation separately from the persisted public four-field
ArrayInt ABI. One mixed Void signature carries exactly one record-array
value-result plus one-or-more ArrayInt value-results and direct scalar values.
C and LLVM consume those owners for type projection and every-return copyout;
the focused gate rejects carriage, missing-element declaration, and false
physical-ABI mutations before publication. The fixed canary passes routine 495
and next rejects the by-value record-array parameter at routine 563, leaving
that distinct value/read policy as the next rung.

GraphPlan v47 advances schema v39 to v40 for stable expression identity 55,
the declaration-keyed indexed read from a compiler-owned three-field
`Array<Record>`. `DirectMirRoutineSignatureFact` owns value carriage,
`DirectMirScalarProgramLogicalRecordFact` owns the element declaration and
member order, and `CompilerAbiNominalArrayLayoutFact` owns `data,len,cap`.
The C/LLVM consumers receive the admitted target projection and do not infer a
public four-field Array layout or reopen MIR. The focused gate rejects carriage,
missing-element, false physical-ABI, and missing-member mutations before
publication. The fixed canary passes routine 563 and next rejects routine 568
`CodegenAstTextTypedArenaFromNodes`, whose same by-value record Array feeds a
logical-record return and local collection construction; that is a distinct
next signature/operation rung.

GraphPlan v48 advances schema v40 to v41 without a new expression identity.
`MirRoutineInstructionUseFacts` owns the first `ArrayPush` use as the receiver;
the existing routine-local value/type plans join it to `Array<Int>` or
`Array<String>`, and the expression owner consumes uses from offset one. The
sealed operation stores that target in `operation_left_locals`, with C/LLVM as
the last consumers. Local identity and definition blocks are validated within
their routine partition, and condition evidence indexes flat program rows only
through the routine's explicit block offset. The focused gate executes the
record-returning loop and rejects a receiver-use mutation. The fixed canary
advances only the callable envelope from routine 568 to routine 569; it is not
production-body execution evidence.

GraphPlan v49 keeps schema v41. The declaration-keyed logical-record Array
value-parameter policy now owns exact `Bool` return admission in addition to
its existing public `Array<Int>` and logical-record returns. Claimant/final
signature and both backend signature emitters consume that same policy. The
focused fixture executes indexed record-member flow through a Bool-returning
callable and rejects a Bool-to-String return mutation before publication. The
fixed canary advances the callable-envelope scan from routine 569 to routine
625 `LanguageWordSpelling(LanguageWordId) -> String`; it is not body-execution
evidence for the intervening routines.

GraphPlan v50 advances schema v41 to v42 for the optional declaration-keyed
payload-free enum carrier. `MirProgramDeclarationIndex.enum_variants` remains
the persisted identity owner; the derived
`DirectMirScalarProgramPayloadFreeEnumFact` admits only callable-referenced enum
declarations whose variants are all payload-free and have contiguous ordinals.
Claimant/final signature and both backend type emitters consume the same exact
`value`-carriage fact. There is no `LanguageWordId` spelling branch, general
enum-to-Int widening, payload-bearing fallback, or backend MIR reconstruction.
The focused C/LLVM fixture owns signature execution evidence. The fixed canary
advances only the pre-body callable envelope from routine 625 to routine 670
`ParserExpressionGraphsAppendInto(...)->Void`.

GraphPlan v51 keeps schema v42. The existing declaration-keyed logical-record
Array value-result owner now admits the exact four-parameter Void shape with
one record-Array copyout, one distinct direct logical-record value, and two
direct Int values. GraphPlan v52 keeps the same carrier and adds the separate
two-parameter Void shape whose direct record value must be the array element's
same declaration. The original record-Array plus public `Array<Int>` copyout
shape remains unchanged. C and LLVM reuse the existing target-neutral record-
Array copy lifecycle and logical-record value types; no routine-name branch,
general aggregate widening, or backend MIR reread was added. Focused execution
owns both new shapes. The fixed canary advances only the pre-body callable
envelope through routines 670 and 672 to routine 710
`SemanticExpressionGraphAppendNode`, where the next unsupported fact is a mixed
`Array<Int>`/`Array<String>` value-result signature with a logical-record return.

GraphPlan v53 keeps schema v42. The new
`DirectMirScalarProgramLogicalRecordMixedCollectionValueResult` policy owns the
exact ten-parameter signature reached at routine 710: a declaration-keyed
logical-record return, four ordered `Array<Int>` value-results, two ordered
`Array<String>` value-results, and an Int/String/Int/Int value tail. The
existing persisted Array ABI facts and C/LLVM copy owners remain the only
physical authorities. The focused gate executes both backends and rejects
carriage, ABI, collection-count/order, scalar, and return mutations before
publication. The fixed canary advances only the pre-body callable envelope to
routine 711, which shares the 4+2 collection prefix but has a distinct single-
String tail and therefore remains fail-closed.

GraphPlan v54 keeps schema v42 and adds that exact seven-parameter String-tail
family to the same mixed-collection signature owner. The focused gate now owns
both complete signatures, two copies of every backend Array copy lifecycle,
and compact-family carriage/type/cardinality negatives. The fixed canary passes
the matching envelopes at routines 711–713 and next fails closed at routine
714's separately persisted `Array<Bool>` value-result boundary; no ArrayBool
layout or copyout is inferred from ArrayInt/ArrayString facts.

GraphPlan v55 advances schema v42 to v43 because the existing
`DirectMirScalarProgramArrayBoolAbiFact` now owns exact value-result
routine/parameter/digest rows beside its sole physical layout receipt. A
separate capture owner consumes only the selected admitted routine inventory;
it does not become a second fact authority and the retired whole-program
instruction scan cannot reappear. The exact Bool-returning signature has five
ordered `Array<Int>`, one `Array<Bool>`, and two `Array<String>` copyouts plus a
Bool/Bool/String value tail. C and LLVM consume the same rows for signature and
every-return copyout. The focused gate executes both backends and rejects ABI,
carriage, family order/count, scalar, and return mutations before publication.
The fixed canary advances only the pre-body envelope through routine 730 and
next fails closed at routine 731's exact Void return boundary.

GraphPlan v56 keeps schema v43. The exact
`DirectMirScalarProgramVoidLogicalRecordArrayIntValueResultSignatureReady`
policy joins one persisted `Array<Int>` value-result, one by-value
declaration-keyed logical record, and one direct String value in a complete
three-parameter Void signature. It creates no physical carrier or target
authority: existing ArrayInt and logical-record facts remain the SoTs and
existing C/LLVM owners copy every explicit return. The focused gate executes
both targets and rejects nine signature/ABI mutations before publication. The
fixed canary advances only the callable envelope through routine 770 and next
fails closed at routine 771's distinct two-record-value plus one-record-
value-result Bool signature.

GraphPlan v57 keeps schema v43. The exact
`DirectMirScalarProgramLogicalRecordInputsValueResultSignatureReady` policy
joins three distinct persisted logical-record declaration identities to one
complete Bool signature: direct values at ordinals 0/1 and a direct
value-result at ordinal 2, with no resource or physical ABI rows. It creates no
new record fact or layout authority. The existing callable-signature owner is
the final admission consumer and the existing C/LLVM logical-record owners
copy ordinal 2 on every explicit return. The focused gate compiles both emitted
callees and rejects nine signature mutations; its Main intentionally does not
call the three-record routine because caller-side `AST_CALL` admission remains
separately fail closed. The fixed canary advances only the callable envelope
through routine 782 and next fails at routine 783's distinct readonly-record,
two-String, two-ArrayString-copyout signature.

GraphPlan v58 keeps schema v43. The exact
`DirectMirScalarProgramReadonlyLogicalRecordArrayStringValueResultSignatureReady`
policy joins one persisted declaration-keyed logical record and two persisted
ArrayString value-result rows to a complete Bool signature. The record is
indirect readonly-ref at ordinal 0, ordinals 1/2 are ABI-free String values,
and ordinals 3/4 are direct copyouts whose positive ABI layout identities must
match. The policy is not a new carrier owner: the logical-record and
`DirectMirScalarProgramArrayStringAbiFact` rows remain authoritative, and the
existing C/LLVM emitters are their final consumers. The focused gate executes
the combined call in both backends and rejects ten signature/ABI mutations.
The fixed canary advances only the callable envelope through routine 792 and
next fails at routine 793's distinct Int-returning two-ArrayString-copyout plus
two-String signature.

GraphPlan v59 keeps schema v43. It originally introduced the exact
Int/two-ArrayString policy for the then-reached four-parameter signature. The
current shared `DirectMirScalarProgramArrayStringValueResultSignatureReady`
owner subsumes that temporary shape: Bool and Int returns admit one-or-more
same-layout direct ArrayString value-results with scalar values in any ordinal,
while Void retains its single-copyout boundary. The old positional owner is
deleted and negatively gated. The existing ArrayString fact remains the sole
carrier authority, and the C/LLVM owners copy every admitted value on each
return. The focused gate executes both the original two-copyout case and the
production-shaped three-copyout `EmitDeclFields` case.

GraphPlan v60 keeps schema v43. The exact
`DirectMirScalarProgramBoolTwoArrayStringTwoArrayIntValueResultSignatureReady`
policy joins two persisted ArrayString value-result rows, two persisted
ArrayInt value-result rows, and four direct String values to one complete Bool
signature. The copyouts occupy ordinals 0..3; each same-family pair must carry
one positive equal layout identity and the two family identities must differ.
The existing ArrayString and ArrayInt facts remain the sole carrier authorities,
and their existing C/LLVM owners copy all four values on every explicit return.
The focused gate compiles both emitted callees and rejects thirteen mutations;
its Main does not call the eight-parameter routine because caller-side mixed
`AST_CALL` admission remains separately fail closed. The fixed canary advances
through routine 794 and next fails at routine 795's distinct readonly logical-
record plus Int/Bool parameters and owned ArrayBool return envelope.

GraphPlan v61 keeps schema v43. The exact
`DirectMirScalarProgramReadonlyLogicalRecordArrayBoolReturnSignatureReady`
policy joins one declaration-keyed indirect readonly-ref logical record and
direct ABI-free Int/Bool values to an owned ArrayBool return. The logical-record
inventory remains the parameter identity owner. The existing
`DirectMirScalarProgramArrayBoolAbiFact` remains the return carrier authority
and now records `owned_return_present` only when every return instruction
carries the same admitted four-field layout. Existing C/LLVM signature,
expression, and return emitters are the last consumers; no general ArrayBool-
return signature or copied record layout was introduced. The focused gate
compiles both emitted callees and rejects twelve mutations. Its Main leaves the
callee uncalled, so the evidence is limited to the callee ABI and return-
expression boundary. The fixed canary advances through routine 795 and next
fails at routine 796's distinct one-parameter `owner-handle` logical-record
transfer and same-record return envelope.

GraphPlan v62 keeps schema v43. The exact
`DirectMirScalarProgramOwnedLogicalRecordReturnSignatureReady` policy admits
one declaration-keyed logical record carried by `owner-handle`, with direct
pass shape, no physical ABI row, and the same declaration identity as its
return. `DirectMirRoutineSignatureFact`, the logical-record inventory, and the
existing MIR ownership spelling remain the authorities; existing C/LLVM
logical-record value emitters are the last consumers. No destructor, copied
layout, name branch, or general owner-handle route was added. The focused gate
compiles both emitted callees and rejects nine genuinely invalid mutations.
Explicit value and readonly-ref variants are not negatives because existing
owners admit them as different valid signatures; an unknown carriage and a
same-record mismatch own the recurrence falsifier. The fixed canary advances
through routine 796 and routines 797..806, then fails at routine 807's distinct
readonly record, String value, and ArrayString value-result Bool envelope.

GraphPlan v63 keeps schema v43. The exact
`DirectMirScalarProgramReadonlyLogicalRecordStringArrayStringValueResultSignatureReady`
policy admits one declaration-keyed indirect readonly-ref record, one direct
ABI-free String value, and one direct ArrayString value-result under a Bool
return. `DirectMirRoutineSignatureFact`, the logical-record inventory, and the
persisted ArrayString ABI fact remain the only authorities; existing C/LLVM
readonly-record and ArrayString copy-in/out emitters are the last consumers.
No name branch, general one-copyout route, record-as-value coercion, copied
layout, or backend MIR reread was added. The focused gate executes both target
calls and rejects eleven invalid mutations. Its initial C regex failure was a
formatting-only `const char*` versus `const char *` mismatch, so the recurrence
ratchet records semantic parameter order, pointer/mutref identity, and copy
lifecycle rather than whitespace. The fixed canary advances through routine
807 and routines 808..871, then fails at routine 872's distinct String,
readonly record, and two logical-record value-result Bool envelope.

GraphPlan v64 keeps schema v43. The exact
`DirectMirScalarProgramReadonlyLogicalRecordTwoLogicalRecordValueResultSignatureReady`
policy admits one direct ABI-free String, one declaration-keyed indirect
readonly-ref record, and two distinct declaration-keyed direct record value-
results under a Bool return. `DirectMirRoutineSignatureFact` and the logical-
record inventory remain the only authorities; existing C/LLVM readonly and
record copy-in/out emitters enumerate the admitted ordinals as last consumers.
No name branch, broad two-copyout rule, record identity collapse, physical
record layout, partial-field copy, or backend MIR reread was added. The focused
gate executes both targets and rejects fourteen invalid mutations. Its first
two failures were measurement defects: line-counting a one-line JSON document
did not count repeated facts, and one monolithic C signature regex mixed
formatting with ABI identity. The recurrence ratchet now counts occurrences
and checks each parameter/copy lifecycle independently. The fixed canary
advances through routine 872 and routines 873..915, then fails at routine 916's
distinct ArrayString-copyout plus same-record return envelope.

GraphPlan v65 keeps schema v43. The exact
`DirectMirScalarProgramLogicalRecordReturnArrayStringValueResultSignatureReady`
policy admits one direct ArrayString value-result, one direct declaration-keyed
logical-record value, and one direct String value, with the return fixed to the
same record identity as parameter 1. `DirectMirRoutineSignatureFact`, the
logical-record inventory, and the persisted ArrayString ABI fact remain the
authorities; existing C/LLVM collection copyout, record-value, and record-
return emitters are the last consumers. The final signature guard recognizes
this exact proof beside the prior mixed-collection proof; it does not create a
general record-return/copyout route. The focused gate executes both targets and
rejects nine genuinely invalid mutations. A proposed carriage mutation was
removed because another owner already admits by-value ArrayString; a valid
alternative signature is not a falsifier. The fixed canary advances through
routine 916 and routines 917..920, then fails at routine 921's distinct Void,
ArrayString-copyout, and record-value envelope.

GraphPlan v66 keeps schema v43. The exact
`DirectMirScalarProgramVoidLogicalRecordArrayStringValueResultSignatureReady`
policy admits one direct ArrayString value-result and one direct declaration-
keyed logical-record value under a Void return. `DirectMirRoutineSignatureFact`,
the logical-record inventory, and the persisted ArrayString ABI fact remain the
only authorities; existing C/LLVM collection copyout and record-value emitters
are the last consumers. No routine/record-name branch, broad Void widening,
record-carriage coercion, copied layout, or backend MIR reread was added. The
focused gate executes both backends across early/final exits and rejects ten
invalid mutations. The thirty-nine-gate aggregate rebuilds the driver once and
passes without errors or warnings. The fixed canary advances through routine
921, then fails at routine 922's distinct five-parameter Void envelope with the
same ArrayString copyout and record value plus three String values.

GraphPlan v67 keeps schema v43. The exact
`DirectMirScalarProgramVoidLogicalRecordThreeStringArrayStringValueResultSignatureReady`
policy admits one direct ArrayString value-result, one direct declaration-keyed
logical-record value, and three ordered direct String values under a Void
return. It consumes the same `DirectMirRoutineSignatureFact`, logical-record
inventory, and persisted ArrayString ABI fact without widening v66. Existing
C/LLVM collection copyout, record-value, and String-value emitters are the last
consumers. No name branch, arbitrary scalar tail, carriage coercion, copied
layout, or backend MIR reread was added. The focused gate executes both targets
and rejects twelve invalid mutations; the forty-gate aggregate passes without
errors or warnings. The fixed canary advances through routine 922 and routines
923..925, then fails at routine 926's distinct six-parameter Void envelope with
four String values.

GraphPlan v68 keeps schema v43 and replaces the v66/v67 cardinality-specific
split with one exact reached-family owner:
`DirectMirScalarProgramVoidLogicalRecordArrayStringValueResultSignatureReady`.
It admits one direct ArrayString value-result, one direct declaration-keyed
logical-record value, and exactly 0, 3, or 4 ordered direct String values under
a Void return. `DirectMirRoutineSignatureFact`, the logical-record inventory,
and the persisted ArrayString ABI fact remain the only authorities. Existing
C/LLVM copy-in/out and value emitters remain the last consumers; there is no
new physical fact or backend route. The old three-String policy file and every
consumer read are deleted, and the component gate rejects its reappearance.
Counts other than 0, 3, and 4 fail closed, so this is not a general scalar-tail
policy. The focused gates execute all three admitted family members and reject
invalid carriage, type, pass, layout, record, String-tail, return, and arity
facts. The forty-one-gate aggregate passes without errors or warnings. The
fixed canary advances through the former routine 926 and routines 927..1105,
then fails at routine 1106's distinct zero-parameter Void envelope.

The first v68 canonical attempt was rejected by the parser because the merge
had dropped one guard terminator and the next `if`. Source-inventory checks did
not prove syntax. The recurrence contract is therefore to parse the changed
owner root and its import graph with the current parser seed before a full
compiler rebuild, then still require the canonical composed-graph build. This
preflight is diagnostic evidence only; it does not replace the canonical
parser/build authority.

GraphPlan v69 keeps schema v43. The existing
`DirectMirScalarProgramVoidScalarCallableSignatureReady` owner now admits zero
or more direct ABI-free scalar value parameters under a Void return. The empty
parameter list is not a new semantic family, so no zero-Void owner, routine
allowlist, or dummy carrier was added. The claimant envelope and final
signature owner's zero-parameter branches consume this same fact beside the
existing non-Void zero-return policy; they no longer reconstruct Void support
from a separate return-type list.

For the reached body, `DirectMirScalarProgramDirectCallFact` remains the direct
call identity owner. General expression admission now consumes its existing
zero-argument projection when a persisted call marker resolves by source
SyntaxNodeId to an exact zero-parameter callable. Nonzero calls continue through
the ordered CallArgument owner, and complete call-marker coverage remains
fail-closed. The C/LLVM direct-call emitters only consume the normalized
callable ordinal and return type; no target spelling recovery or backend MIR
reread exists. The focused and adjacent executable gates, component contract,
and forty-one-gate aggregate pass. The fixed canary advances through routine
1106 and routines 1107..1260, then fails at routine 1261's distinct nested
`Option<OptionStructRuntimeFact>` return envelope.

GraphPlan v70 keeps schema v43 and corrects that last description: the return
is not a nested generic Option. `OptionStructRuntimeFact` is a declaration
name, so the type is one ordinary `Option<T>` whose payload `T` is a logical
record. `OptionPayloadTypeOpt` remains the wrapper-shape SoT, while
`DirectMirScalarProgramLogicalRecordFact` remains the declaration/field-order
SoT. The new join admits contextual Some/None/IsSome/UnwrapOption facts only
when the parsed payload resolves to that inventory. `Option<Unknown>` remains
limited to the persisted None leaf after an expected type is known.

The C/LLVM tag-plus-record carrier is a target-local projection of those two
facts, not a physical ABI fact: the declaration and reached instructions keep
layout id zero, `required=false`, and null layout. LLVM foreign declarations
remain owned once by `DirectMirScalarCfgLlvmForeignDeclarations`; dependent
record types precede Option helpers. The focused gate rejects carrier and
physical-layout mutations, the forty-two-gate aggregate passes, and the fixed
canary advances through routine 1468 before reaching a separate logical-record
value-result parameter at routine 1469. Record/routine-name branching,
physical-layout inference, copied field inventories, and backend MIR rereads
remain forbidden.

The 2026-08-10 GraphPlan v28 `Option<Bool>` closure adds a third exact
instance of the target-neutral Option physical carrier to
`projection.direct_mir_scalar_cfg_program_extension`. The persisted MIR row is
the sole layout authority: size eight, alignment four, a four-byte tag at
offset zero, a one-byte Bool payload at offset four, and the canonical
runtime/discriminant identity. C emits the matching asserted aggregate and
LLVM emits `{ i32, i1 }`; callable returns, direct calls, locals,
Some/IsSome/UnwrapOption, and both backends consume that same receipt. The
generic source `None` is persisted as a `binding=none` leaf with
`Option<Unknown>` instruction spelling, so
`DirectMirScalarProgramOptionAbsenceExpressionKind` joins it to the expected
Option<Int/String/Bool> type without treating that spelling as a physical ABI
fact. The focused gate executes exact C/LLVM value flow and rejects a mutated
Bool payload offset before artifact publication. The fixed 1,484-routine
canary passes the former Option<Bool> callable-envelope boundary and next
rejects the unsupported `JsonArrayStringFact` nominal return at routine 47.

The 2026-08-10 GraphPlan v27 `Option<String>` closure extends
`projection.direct_mir_scalar_cfg_program_extension` with another instance of
the existing target-neutral Option physical carrier. Its sole input is the
persisted required MIR ABI row: 16-byte size, 8-byte alignment, tag at offset
zero, pointer payload at offset eight, and the canonical runtime/discriminant
identity. Contextual Some/IsSome/UnwrapOption selection consumes actual and
expected types once; expression admission may not reopen the builtin registry
or fall back to the Option<Int> specialization. C and LLVM signatures,
expressions, and local storage consume the same fact, and the focused gate
executes value flow while rejecting a mutated payload offset before artifact
publication. The later v28 absence owner now admits its persisted `None` leaf;
that does not authorize any target to derive layout from `Option<Unknown>`.

The 2026-08-10 GraphPlan v26 value-result `Array<Int>` closure extends
`projection.direct_mir_scalar_cfg_program_extension` without adding a second
top-level fact family. The derived
`DirectMirScalarProgramArrayIntValueResultFact` joins the formal parameter's
persisted `value-result` carriage to its complete `abi.layout_rows` receipt,
and C/LLVM consume one target projection for representation plus copy-in and
copy-out. Caller-side value-result invocation remains fail-closed instead of
falling through the by-value direct-call path. The preceding v25 derived
`DirectMirScalarProgramTwoIntNominalAbiFact` selects at most one eligible row
from the admitted declaration index, joins its field identities to its required
MIR ABI row, seals matching formal-parameter
and instruction receipts, and is the only input to the C/LLVM target
projection. The projection carries the target capability fingerprint; C
asserts size, alignment, and field offset from the receipt, while LLVM emits
the proven two-field aggregate. The focused four-routine gate compiles an
otherwise-unused nominal passthrough routine beside an unrelated declaration
on both targets, rejects an actually referenced unsupported nominal, and
rejects a mutated field offset before artifact publication. This is a bounded
one-candidate closure, not permission to accept a general declaration table or
infer layouts from nominal spelling.

The preceding GraphPlan v24 Option<Int> closure extends
`projection.direct_mir_scalar_cfg_program_extension` without adding a second
top-level fact family. The required MIR ABI row remains the physical layout
authority; the program extension carries one target-neutral
`DirectMirOptionMatchAbiFact`, and C/LLVM consume its target projections.
`direct_mir_scalar_option_int_owner.sh` proves exact four-routine execution and
rejects a mutated value-field offset before either target publishes an
artifact. This does not admit nominal declarations, `Option<String>`, Option
locals, Array parameters, or arbitrary aggregate ownership on the v24 path;
the later v27 row owns the separate Option<String> physical/local boundary.

The 2026-08-05 GraphPlan v23 Array<String> call closure extends
`projection.direct_mir_scalar_cfg_program_extension` without adding a second
top-level fact family. The shared literal fact supplies typed String elements
to both the legacy local collection route and the program ExpressionSet. One
sealed boundary subfact joins caller-frame storage, borrowed-static element
ownership, the canonical Array<String> ABI row, a by-value formal parameter,
a literal-bounded callee index, the borrowed String return, and its last caller
use. C and LLVM are target projections of that same boundary; cleanup may not
infer ownership from `Array<String>` spelling. The executable evidence is
`one_mir_string_array_index_return_projection.sh`, including repaired-identity
ABI, call, bound, topology, and literal-spine negatives with no artifact.

The 2026-08-13 registry normalization removes the final duplicate Coq fact
group. The exact Array<String> value-result and owned-return shapes remain
executable GraphPlan evidence, but they are not peer top-level authorities.
Their shared `DirectMirScalarProgramArrayStringAbiFactFromAdmitted` remains an
explicit consumer-owned fact under
`projection.direct_mir_scalar_cfg_program_extension`; the existing
`DirectMirScalarProgramArrayStringBoundaryFact` is the classified derived
projection. The earlier six-group audit remains historical evidence in
`docs/self_hosted/18_self_host_layering_duplication_audit.md`; it is no longer
the current registry state.

The 2026-08-05 GraphPlan v20 closure extends
`projection.direct_mir_scalar_cfg_program_extension` through the derived rows
above. Producer-carried call topology and the canonical semantic builtin
signature table now admit the prior String DAGs plus runtime `StringContains`,
`Split`, `ToInt`, and `Array<String>` length/index expressions. One sealed
runtime-ABI fact and one captured four-offset `Array<String>` layout fact feed
both target projections; C static assertions derive from that captured fact.
The program GraphInput owner prevents the legacy String-array planner from
claiming a `Split`-defined graph beside GraphPlan, while the shared expression-
kind query and materialization requirement remove consumer-local range scans.
`one_mir_string_collection_builtin_projection.sh` is the executable positive,
semantic-mutation, and negative evidence. Ordered multi-parameter callables,
`StringReplace`, math builtin graphs, arbitrary collection ownership, and
general temporary String lifetime remain open; `str_case_math.pgy` is the next
falsifying fixture, so this does not promote the family beyond bounded closure.

The expression rows above are deliberately bounded. The initializer row
closes array-literal initializer body ownership; the expression-surface row
also carries array-literal call arguments as ordered element graphs, named
struct-literal values as ordered field-binding graphs, and `Option<struct>`
constructors as ordered call spines whose payload remains that struct graph.
Contextual `Option<T>` `None`/`Some` initialization, reassignment, return, and
typed call arguments select the MIR-owned ABI constructor through the shared
expected-type value dispatcher and
`expr_semantic_option_value_owner.pgy`; statement adapters may not duplicate
that policy, and native C/LLVM consumers may not recover the type from AST text
or fall back to `Option<Int>`.
`Result<T,E>` match binding types follow the same single-owner direction. The
semantic statement-result fact owns the match subject type; the canonical
wrapper owner projects the `Ok` payload or `Err` error type; and
`SelfMirMatchFactRows` carries that exact type beside the binding. The
mir-lower local view and binding renderer consume the carried row. A missing
row fails ordinary graph admission, while only the explicitly named native
oracle canonicalization bridge may reconstruct an inferred legacy `Let` and
then reproduce canonical Pergyra MIR. Version zero denotes an inactive lexical
name, so branch or match arms cannot promote a block-local binding into an
outer phi.
Other expression shapes remain under `selfhost.expression_surface`. The try row closes postfix-try
structure and its operand edge only. Payload type classification remains in the
expression-surface `BRIDGE`, and the compact legacy/native canonicalization
bridge must reproduce the same graph but is not hard-codegen authority.
Direct zero-argument `ListNew`, `QueueNew`, and `SetNew` call arguments consume
the enclosing signature's carried expected type through the contextual builtin
owner before source-text fallback. Matching families and wrong-family/arity
falsifiers are owned by
`direct_mir_contextual_collection_constructor_argument_owner.sh`; this closes
that reached consumer slice but does not close the broader expression-surface
`BRIDGE` row.

Concrete Option/Result C materialization is a derived projection of
`selfhost.type_runtime_usage_surface` and `abi.layout_rows`, not a new ABI
authority. The canonical recursive value-wrapper inventory captures nested
Option/Result nodes once from semantic type usage;
`ResultRuntimeFactForType` owns its tagged C type and constructor/query/unwrap
symbols; `EnumAbiValueFactFor` is the single payload-free/tagged enum C value
and default-return projection; Option runtime facts consume that enum fact or
another canonical inner C value fact. `type_declaration_emit_owner.pgy`
consumes the same inventory as declaration nodes, including wrapper-to-wrapper
edges. The bootstrap C scheduler mirrors those dependencies from MIR
field/payload type names. Consumer-local enum layout switches, a Result-only
inventory, nominal-triggered Option generation, and a post-hoc
`EmitResultRuntimeDefinitions(...)` pass are forbidden.
`nested_option_result_field_declaration` and the nested cyclic declaration
reject are the current placement witnesses. `dish_result_collect` remains the
executable value-flow witness for `Result<Dish,CookErr>` and its missing-
binding-type mutation. Native C does not gain a parallel wrapper inference
path.

For the self-host `pgy.mir.v1` consumer, `MirProgramRoutineIndex` is one
admitted local view of `mir.execution_graph`: it captures routine, block, and
instruction partitions plus kind/source-type and machine spans once.
`MirRoutineFactIndex` layers routine-local result/CFG facts on that identity.
Neither row is a new authority. Machine admission proves full structure once
and carries the same `MirDocumentFactIndex` to the direct consumer; rebuilding
that document index is negative-gated. The graph sequence owner validates exact
graph/node schemas and derives count while collecting the same node pass.
Canonical local/version identity is one shared SSA predicate used by phi and
direct scalar admission. The family remains `BRIDGE`: hello, `let_log`,
`multilet`, four-block `ifelse`/`if_else_assign`, three-block `reassign_block`,
five-block `nestedif`, and the four-block single-header `whileloop` directly
project one unchanged MIR to C and LLVM. The CFG path derives typed roles,
binds one MIR/AIR certificate and one verified plan, and keeps emitters away
from MIR/AIR reads. General `for` lowering, nested or multiple loops,
multi-phi, and general CFG carriage remain open.

For `abi.runtime_call_rows`, the declared owner remains `SFAbiRuntimeCallRows`
under `SOMirAbi`. The current stable-identity sub-rung is executable: MIR
materializes `runtime_call_abi_id` from the canonical domain/type/operation key,
and C, LLVM, and self-host consumers validate the carried ID. The helper
`mir_abi_resource_runtime_row_id` and the self-host `Long` mirror are projections
of that owner, not additional SoTs. The registry row remains `BRIDGE` because
the aggregate/runtime compatibility corpus and all legacy compatibility paths
still need migration; the identity gate alone does not promote the family.
The active MIR consumer seam is now explicit: slot DEF/destructure owners carry
Claim plus concrete Read/Write auxiliary rows, pin owners carry
PinRead/PinWrite Init and cleanup auxiliary rows, and the C/LLVM selectors only
select those instruction-owned rows. Missing auxiliary capacity or a removed
owner fact fails closed; no backend-side ABI-table repair is permitted.
The self-host MIR consumer validates the carried `runtime_call_abi_aux` array
with the same owner/type/payload/stable-ID policy, and the rung-2 auxiliary-ID
mutation is negative-gated; this does not close the wider aggregate/runtime
compatibility bridge.
The named string formatted-print row input is also projected directly into a
typed `CompilerRuntimeCallAbiFact`. Both direct scalar backends consume its
symbol, target-library materialization, call shape, and stable ID together with
the separate line-format and `Int` layout owners. The structured owner never
parses its serialized manifest row back into authority; unknown row input is a
missing fact. This widens the executable projection while the registry status
remains `BRIDGE` for unported runtime-call consumers.

For `target.capability_profile`, `target_capability_owner.pgy` owns the
accepted projection and required-fact vocabulary.
`target_projection_fact_owner.pgy` is a derived carriage owner only: DRV-2
passes its `cpu-c` row to the final emitter, carries a bounded target
fingerprint, and rejects a missing or mutated row. The family remains `BRIDGE`
until the native full-width target fingerprint, concrete size/alignment/endian
values, object format, and AIR evidence references are carried instead of
vocabulary plus a self-host carriage digest alone.

## 2026-08-13 GraphPlan receiver-carriage evidence

This evidence extends the existing
`projection.direct_mir_scalar_cfg_graph_plan` substitution; it does not add a
registry row or change its `CLOSED` status.

- The source-to-MIR producer attaches one primary LocalRef to `ArrayPush`,
  `ArraySet`, and `ArrayPop`. A value-result receiver is carried as
  `parameter:<routine-source-syntax-id>:<ordinal>` and joins the existing
  routine signature and collection ABI facts.
- Program operation storage/readiness and both backends consume that exact row.
  ArrayInt value-result set is operation 37; its index and value keep their
  admitted expression identities. The old use-name receiver selection,
  unique-parameter inference, local-only owner paths, and backend MIR rereads
  are negative-gated.
- The focused executable gate mutates only ordinal 1 of two same-typed
  `Array<Int>` value-result formals and observes the untouched first formal in
  C and LLVM. Canonical self-driver build, CI-profile inventory, and component
  structural inventory also pass.
- A shared control-transfer fact now owns exact break/continue edges for both
  scalar CFG consumers. Its focused C/LLVM target-swap negatives and the old
  loop snapshot/exit-phi gates pass.
- `StringJoin`/`Join` now consume the existing semantic signature and runtime
  ABI row in both targets; wrong type/order negatives fail closed.
  `ToString(String)` consumes a target-neutral identity specialization while
  `ToString(Int)` retains the existing formatting ABI. These are derived
  GraphPlan projections and add no authority row. Their stable expression IDs
  69 and 70 advance the current wire schema from `graph-plan.v44` to
  `graph-plan.v46`; historical `GraphPlan vN` labels are migration-rung
  identifiers, not wire-schema suffix ownership.
- Stable operation 38 closes the reached OptionInt try-let without adding an
  authority row. The persisted unary try graph, callable return identity, and
  existing OptionInt ABI receipt are consumed once; C and LLVM execute the
  same Some/None behavior and five malformed fact mutations fail closed. This
  advances the current wire schema from `graph-plan.v46` to
  `graph-plan.v47`.
- The fixed canary now reaches global row 592, the Bool branch
  `((start + 4) == end) && SubEqualsWithLen(...)`. Its conditional right-call
  evaluation is the open executable frontier. No registry status or census
  count changes.

## Current Judgment

The owner outline is complete for the listed compiler spine, but implementation
closure is not. The exact status counts are gate-owned
and must change only when a row gains or loses the evidence required by its
status.

Current status counts: `CLOSED=55 BRIDGE=32 ACTIVE=1`.

`selfhost.match_case_pattern` is closed on one HIR parse at
`SemanticAstStatementFacts` admission. Canonical pattern, variant, binding
range/pool, and digest remain keyed by `SyntaxNodeId`; semantic, MIR, and
self-C consumers borrow that structure. Missing, wrong-kind, cross-wired
range, variant, and binding mutations fail closed without a text fallback.

The current world/tobject semantic slice does not introduce another top-level
authority row. `ast_expression_graph_nominal_constructor_call_owner.pgy` and
`ast_expression_graph_domain_query_owner.pgy` are bounded verdict consumers of
`selfhost.nominal_declaration_rows`: the former derives the caller-supplied
constructor prefix from exact field kinds, and the latter joins a world zone
slot to an exact `ObjectSlot`/`TObjectSlot` declaration identity. Neither owner
may add a constructor/function compatibility fallback or put symbolic query
arguments into the ordinary value environment. Promotion to a carried fact
family is required only when a downstream MIR/backend consumer needs the query
identity beyond semantic admission.

The exact intent DIR projection stays inside `dir.domain_graph`; it does not
create a second intent authority. `selfhost.intent_declaration_rows` is now
`CLOSED`: `intent_fact_owner.pgy` admits exact declaration, participant, and
ordered-step ranges, `intent_exact_identity_contract_owner.pgy` seals them to
the same typed artifact epoch, and `CodegenIntentExecutionView` carries the
receipt to the final source-C emitter. That emitter no longer opens AST
children, reparses step headers, or reconstructs compensation rows. Missing
and crossed source receipts fail at codegen admission before an emitter is
called. The direct-MIR
plan remains a distinct admitted lane and may omit source facts only when it
owns every semantic intent routine; it is not a fallback reconstruction path.
The broader zone-authority and MIR transition families retain their own
registry status and are not promoted by this declaration-row closure.

The registry does not replace the detailed pass contract or migration ledger.
It answers a narrower question: who is allowed to decide each top-level fact
family, and which last consumers must eventually lose every alternate read.

### Zone runtime projection checkpoint

The self-host zone C projection does not create a new semantic owner.
`selfhost.nominal_declaration_rows` owns zone/field identity,
`dir.domain_graph` owns frontier topology, and
`semantic.domain_runtime_assignment` owns participant and projection-member
assignments. The additional last consumers are
`program_admitted_semantic_owner.pgy`, `driver_pipeline_owner.pgy`,
`nominal_struct_emit_owner.pgy`, `program_emit.pgy`, and
`domain_runtime_c_codegen_bridge_owner.pgy`. The shared C layout boundary is
`pgy_runtime_zone_sync_abi.h`; bounded overflow policy remains in
`pgy_frontier_policy.h`.

The exact compiler artifact now has a 20/20 zone declaration-to-definition
bijection and host-compiles. This domain-runtime slice remains `REACHABLE`;
promotion of the separate installed pure-C artifact target does not promote
zone lifecycle ownership. The zero-topology semantic-artifact receipt must not replace the admitted
nonzero direct-MIR plan. Generated thread-safe zone constructor/destructor
lifecycle and composite-intent full DIR admission also remain open. The focused
negative/execution owner is
`tests/self_hosted/parity/domain_runtime_zone_sync_execution_owner.sh`.

### Intent execution transition projection (bounded substitution)

The MIR-to-recursive-AST step projection consumes semantic intent and action
authority identity; it does not own or synthesize another authority policy. A
step may carry one exact direct authority row, or it may delegate through one
exact declared intent/action target whose action contract owns `requires`,
`within`, and `authorized by self`. The two forms are mutually exclusive.
Missing, duplicate, mismatched, or ambiguous facts fail closed. In particular,
outer orchestration must not manufacture an empty `AuthorizedBy` or duplicate
the terminal action contract. `tests/self_hosted/parity/mir_json_parity.sh`
pins terminal direct authority plus absence of duplicated outer authority;
`tests/self_host_compiler_world_contract_smoke.sh` rejects undeclared or
incomplete delegation without using a step-name allowlist.

The typed branch/compensation rung uses MIR execution subfacts without turning
`selfhost.intent_declaration_rows` into a second MIR or universal intent
authority. `mir.intent_step_transition` uses stable handle
`IntentStepTransitionId`; `mir.intent_terminal_transition` uses
`IntentTerminalTransitionId`. The target-neutral in-memory projection is
`src/self_hosted/mir/intent_execution_fact_owner.pgy` and remains classified
under `mir.execution_graph`.

The step projection seals action result identity, exact enum variants and
tobject payload declarations, explicit successors, success-only completion,
the DIR-owned predecessor, and ordered compensation expression/action identity.
The terminal projection binds one exact source step/role/payload to an intent
result constructor. `MIRRoutine.return_type` remains the return-type authority;
a terminal row proves only construction and last consumption. Distinct
`failure A` and `failure B` therefore cannot collapse into one generic exit.

Native DIR-to-MIR production, exact routine result signatures, stable
declaration identities, the v2 JSON protocol, validated transition blocks,
and target-specific C/LLVM consumers exist. The self machine layer now admits
and cross-seals that plan exactly once. The production self driver consumes the
admitted carrier without the old typed direct/rollback path. Its execution gate
covers success, distinct failure A/B payloads, success-only completion,
predecessor-only rollback, reverse multiple compensation, duplicate expression
spelling, and zero compensation.

This bounded input-language MIR-to-self-C consumer is `SUBSTITUTING`; native
C/LLVM execution is `REACHABLE` evidence. The broader intent registry row stays
`BRIDGE`, because `PgyCompilerWorld` reaches no canonical real-purpose intent
and no full participant/coordination/authority/effect/boundary/compensation/
trace bundle replaces the compiler root's direct orchestration. The bounded
plan must not be used to claim universal intent ownership or compiler-root
dogfood.

Forbidden fallbacks include outcome-to-Bool collapse, variant spelling or
payload-type inference, source/row-order predecessor recovery, call-implies-
completion, all-earlier-step rollback, compensation AST/source rescan,
consumer plan revalidation, expression-graph reconstruction, name-only payload
declaration joins, reachable zero-compensation scaffolds, native MIR grafting,
and coexistence with the old typed direct/rollback path. The gates cross-wire
otherwise-valid result, variant/successor, predecessor, completion, action,
payload declaration, graph digest, and scaffold identities and reject them
before a partial C artifact.

The focused gates are
`tests/self_hosted/parity/intent_execution_fact_contract_owner.sh`,
`tests/intent_typed_transition_native_execution_smoke.sh`, and
`tests/self_hosted/parity/intent_typed_outcome_compensation_owner.sh`.
Protocol admission and no-revalidation enforcement live in
`tests/self_hosted/parity/intent_execution_plan_json_admission_owner.sh` and
`tests/self_hosted/parity/intent_execution_protocol_static_owner.py`. The
detailed contract is
`docs/self_hosted/19_intent_execution_transition_contract.md`.

For `semantic.machine_layer_transition`, the self-host physical declaration
consumer is `src/self_hosted/compiler/machine_layer_declaration_consumer.pgy`.
It consumes the native `pgy.machine-layer.declaration.v1` artifact emitted by
`--machine-manifest-json`; `machine_layer_runtime_projection_owner.pgy` owns
only abstract contact/runtime names. The native provider may supply a different
immutable declaration in the `pergyra.machine-declaration.*` namespace, and the
self-host consumer checks that namespace plus target provenance. Repeating
host-sim grant literals in that abstract owner is a forbidden alternate read
path.

The installed public manifest path packages that native artifact beside the
selected Pergyra-built driver. The companion hash participates in the driver
build key, and `SelfHostMachineLayerDeclarationArtifactPayloadFromPathVerified`
replays its bytes only after the same declaration consumer admits them. Missing
or invalid companions fail closed; the launcher cannot retry native semantics,
and the self-host path cannot serialize a second physical declaration.

The selected grant's physical `base`/`size`/`mode` now remains owned by the
verified projection plan row (`src/compiler/verified_projection_plan.h`) until
the C/LLVM startup consumer `pgy_machine_layer_runtime_bind_mapping_export`.
Backends may not reconstruct that window from a grant name or backend default;
live board/MMU mapping evidence remains a separate refinement gate.
The runtime also exposes `pgy_machine_layer_runtime_provider_bind_export` for
an embedder-owned board/MMU provider; the default host-sim process leaves that
provider unbound, so declaration acceptance is not misreported as live mapping
evidence.

For the self-host C synchronous `DeviceSlot<T>` path, the last codegen
consumers are `src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy`,
`src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy`, and
`src/self_hosted/codegen/emission/program_emit.pgy`. They consume the typed
machine runtime projection owner; they may not duplicate C symbol spelling,
physical grant literals, or recover contact identity from source text.
The same owner now supplies the `RemoteFuture<Int>` await-result bridge used by
`src/self_hosted/codegen/emission/expr_semantic_graph_emit_owner.pgy` and
`try_let_emit_owner.pgy`; unsupported payload rows remain fail-closed.
The self-host declaration consumer then hands the verified fingerprints and
grant window to `machine_layer_runtime_binding_owner.pgy`, which emits the
same mapping bind at `Main` startup; it does not create a second physical fact
owner or a backend-local target default.

For the `List<T>` operation seam, `ast_expression_graph_resolved_call_type_owner.pgy`
owns direct target, receiver, arity, index/value, and return-type facts. Scalar
index/value expressions are projected through the operator policy in
`ast_expression_graph_scalar_shape_owner.pgy`; the List owner must not grow a
second scalar-type taxonomy. `list_runtime_owner.pgy` owns the element-specific
C operation symbols, and `list_call_emit_owner.pgy` is the last codegen
consumer. The DRV-2 `list_ops` and `list_push_get_loop` rows check native/self
MIR parity, emitted `pgy_list_*` symbols, runtime output, rejection of a missing
`collection_call_target` fact, rejection of a missing scalar operand edge, and
rejection of a mixed scalar value type. Source-level List operation names and
List-local operator typing are not compatibility fallbacks.

For compound expressions, `list_call_type_owner.pgy` is the last codegen type
consumer for the carried `ListGet` return element type. The `list_int_loop`
DRV-2 row proves that this fact reaches arithmetic lowering; a mutated ListGet
target is rejected before an operator-side type default can hide the loss.

The lexical binding sub-rung now consumes the same artifact-bound identity
through `SemanticAstLocalBindingOrdinalAt`, `SelfMirRoutineDeclareLocal`, and
the routine block-exit inventory restore. Codegen receives the active typed
binding stack rather than a function-wide name table, so an inner
`items: List<String>` cannot overwrite the outer `items: List<Int>` ABI. The
`list_shadow_scope_metadata` DRV-2 row proves `items.1`/`items.2` identity,
restores the outer List ABI after the branch, and rejects a missing declaration
ABI type fact. Source-name-only SSA lookup and backend type recovery remain
forbidden.

The contextual sequence-literal sub-rung keeps its fact owner at
`ast_expression_graph_array_literal_owner.pgy` plus the dependency-light
`SemanticSequenceElementType` shape owner. `ast_initializer_type_fact_owner.pgy`
promotes a graph-verified `[ ... ]` initializer to the declared `List<T>` type;
the composite emission owner consumes the declared List runtime ABI and emits
the element-specific constructor/push symbols. The `list_literal_context`
DRV-2 row proves `List<Int>`, `List<String>`, and empty `List<String>` values,
rejects a mismatched element and a missing declaration ABI type, and does not
promote the broader Queue surface, which remains the next open seam.

The MIR-lower expression-graph projection keeps identity authority in the
admitted `MirExpressionGraphSequence`. `MirExpressionGraphSequenceAppendParserBridge`
must preserve all prefix `call_target_syntax_ids`, `binding_syntax_ids`,
`binding_kinds`, and `binding_ordinals`, append Unknown identities only for
newly parsed nodes, and
reassemble the arena through the identity-carrying constructor.
`MirIntentExecutionGraphTargetProject` adds no nodes, so it must project the
exact existing identity arrays. Reconstructing either consumer through
`SemanticExpressionGraphArenaFromTopology` is a forbidden dual owner: it both
discards admitted identity and reallocates the cumulative identity prefix.
`expression_graph_identity_prefix_owner_smoke.sh` falsifies that fallback with
nonzero call-target and formal-binding prefix identities.

At checkpoint `be376971`, the `mir.execution_graph` row's self-hosted direct
consumer evidence also includes
`tests/self_hosted/parity/one_mir_string_equality_projection.sh`. One
routine-partitioned `DirectMirScalarCfgGraphPlan` v16 owns the flat CFG, SSA,
phi, local, operation, range, and digest facts for Main plus
`Kind(String) -> String`; typed expression/return links and the registry-owned
String-compare ABI are extensions, not a second graph authority. The gate pins
one seal, one shared routine admission, persisted call/parameter identities,
no backend MIR read, and no artifact for six repaired identity/type/CFG/return
mutations. This is bounded consumer substitution evidence; the registry row
remains `BRIDGE` because general execution-graph consumers and native owners
remain open.

Implementation `30b84f80`, lifetime repair `f6d6fb4b`, and native-MIR identity
repair `024d1ba7` extend that same admitted graph rather than creating a
callable side table. Pergyra-produced routine parameters carry their declaration
`source_syntax_id`; native MIR without a parameter-identity owner omits that
field instead of substituting the parameter type ID, and MIR-to-AST breadth
accepts only wholly absent or complete unique identity rows. Call nodes carry
both `call_target_syntax_id` and
`binding_syntax_id`. Declared and formal callable targets must cross-seal those
IDs with the admitted semantic signature, while builtin and constructor rows
must retain canonical zero/none identity. Installed C consumes the admitted
identity after semantic re-entry and installed LLVM consumes it through the
direct-MIR GraphPlan. The focused gate rejects 20 missing, forged, and
cross-wired type/target/binding/carriage mutations before artifact publication;
the separate installed gate executes exact C/LLVM outputs. This closes the
bounded callable-parameter consumer path but does not promote
`mir.execution_graph`, `selfhost.expression_surface`, or
`selfhost.semantic_artifact_admission` beyond their existing states.
The full-bootstrap partial-identity falsifier must mutate routine parameter rows,
not merely an earlier declaration-shaped JSON row; checkpoint `5f739701` makes
that mutation global and both self-built and oracle mir_lower reject it.

Repair `1d459036` keeps the same registry row and closes the producer/canonical
identity-epoch seam reached by that bounded consumer. `MirExpressionIdentityEpoch`
is the single exact routine/parameter join; MIR expression call targets and
declared/formal callee bindings are rebound to reconstructed canonical IDs
before semantic admission. Numeric offsets, source/canonical dual reads, and
name-only semantic acceptance are forbidden. Collection receiver atom rows
that MIR v1 deliberately does not persist remain an explicit producer-only
lane: they must carry neutral identity and the canonical semantic owner fills
them once. This adds executable negative evidence but does not promote a
top-level row or change the `CLOSED/BRIDGE/ACTIVE` census. Gate-identity ratchet
`e070fcec` preserves the stable canonical-epoch pass marker; the SoT edge gate
observes `CLOSED=50 BRIDGE=35 ACTIVE=1`.

Run `33016014561` reached beyond the first callable epoch mismatch and exposed
two assumptions inside the same owner: MIR routine inventory order is not
source SyntaxNodeId order, and intent participants belong to the admitted intent
execution plan rather than MIR routine formal rows. Repair `dfbe9b0a` keeps the
exact pair table sorted by source ID, rejects duplicate source or canonical IDs,
and requires intent routine formal rows to be empty without creating a second
participant owner. The non-monotonic grammar falsifier is executable evidence;
this repair does not add or promote a top-level registry row.

GraphPlan v71 does not add a registry row or change a top-level fact identity.
It tightens the existing declaration-keyed logical-record signature consumer:
`DirectMirRoutineSignatureFact` owns carriage/pass/resource/ABI facts, and
`DirectMirScalarProgramLogicalRecordFact` owns record identity and ordered
fields. Their consumer may compose zero-or-more readonly record inputs with
exactly one record value-result and direct scalar values. Routine spelling,
parameter count, CFG size, and backend reconstruction are not alternate
authorities. The fixed canary's move from routine 1469 to routine 1474 is
executable evidence for this consumer substitution; the next two-copyout
cardinality generalization remains OPEN until the exact old classifier is
removed and its negative gate migrates.

GraphPlan v72 closes that OPEN consumer seam without creating a registry row.
`DirectMirScalarProgramLogicalRecordValueResultSignatureReady` is now the sole
positive-cardinality logical-record copyout classifier. It consumes the same
routine-signature and declaration facts for one or more copyouts, readonly
record inputs, and direct scalar values. The former exact readonly-plus-two-
copyout owner and every production reference to it are deleted; component
inventory rejects their return. The old two-copyout executable gate now proves
the generic consumer and rejects zero copyouts. This is `CLOSED` for signature
copyout cardinality, while readonly-only record signatures and the coarse
callable-inventory diagnostic remain OPEN separate consumers.

GraphPlan v73/v74 also add no registry row and keep GraphPlan schema v43. V73
moves final callable-signature rejection behind the existing route-admission
receipt: `callable-signature/signature-family` owns the exact routine/name/type
failure until the terminal diagnostic consumes it. The old graph-inventory
message is no longer an alternate authority for this decision.

V74 closes the direct scalar callable consumer as one owner.
`DirectMirRoutineSignatureFact` remains the semantic fact owner;
`DirectMirScalarProgramDirectScalarCallableSignatureReady` consumes it for a
Void or scalar return plus zero-or-more direct scalar value parameters with no
resource or layout receipt. The former exact Void owner and its production
references are deleted and negative-gated. This consumer seam is `CLOSED` for
direct scalar signatures. `Option<Int>` returns with general direct scalar
parameters remain `OPEN` under their existing Option ABI owner; they are not
silently reclassified as scalar.

GraphPlan v75 closes that `Option<Int>` consumer without adding a registry row.
`DirectMirScalarProgramDirectScalarParametersReady` is the reusable consumer of
the routine-signature parameter roles; the ordinary direct-scalar callable and
the `Option<Int>` final-signature path compose it with different return-family
owners. `DirectMirOptionMatchAbiFact` remains the only physical Option layout
authority, and Option is not added to the scalar type family. The fixed canary
passes routine 3 and reaches routine 13 `ReadJsonString`; composing a String
return with one ArrayInt value-result remains the next `OPEN` signature
consumer.

GraphPlan v76 also adds no registry row. The new
`DirectMirScalarProgramArrayIntValueResultCallableSignatureReady` is a consumer
of the existing routine-signature and ArrayInt ABI facts, not a second physical
layout owner. It replaces and negative-gates the exact
`json_string_value_result` arity branch with positive copyout cardinality plus
per-parameter roles. This signature consumer is `CLOSED`; the fixed canary's
next `Option<Int>` plus readonly logical-record input is `OPEN`.

## Dynamic Long remainder consumer

Dynamic Long remainder does not add a top-level registry family. Stable
expression identity 75 consumes append-only `abi.runtime_call_rows` row 246,
whose canonical symbol/call shape are
`pgy_checked_mod_i64_export/long_long_to_long`. Both backends consume that row
through the existing case-math projection, while the runtime owner defines
zero-divisor panic and `INT64_MIN % -1 == 0`. GraphPlan schema advances to v52
for the new normalized expression identity; no carrier column or parallel
arithmetic graph is added. The ABI family remains `BRIDGE`, and the registry
census remains `49 CLOSED / 36 BRIDGE / 1 ACTIVE`.

The fixed canary now stops at row 4363, the first Long loop-header phi in raw
routine row 258. That next consumer must extend the existing common PhiValue
type admission and predecessor receipt. It does not authorize a new SoT row,
Long-only opcode, or routine/backend exception.

## Common Long PhiValue consumer

Long PhiValue admission adds no top-level registry family and no operation
identity. Existing operation 29 consumes the existing value-type plan and
`MirPhiPredecessorBindingFact`; the C/LLVM memory-local emitters remain the last
consumers. The focused executable gate rejects wrong type, non-dominating, and
missing incoming facts. This closes the reached consumer seam without changing
the registry census: `49 CLOSED / 36 BRIDGE / 1 ACTIVE`.

The fixed canary now stops at row 4366, the exact `right > 0L` expression in
raw routine row 258. The next comparison consumer may extend an existing stable
expression family or add one append-only expression identity after its type
contract is explicit, but it does not authorize a new top-level SoT row,
routine exception, or backend-local comparison authority.

## Typed Long comparison consumers

Exact Long greater/equality adds no top-level registry family. Append-only
expression identities 76 and 77 are owned by the shared typed comparison
kind/readiness family; existing Int comparison identities remain unchanged.
The C/LLVM expression and branch emitters are the last consumers, and the
retired Int-only owner path is deleted and negative-gated. GraphPlan schema
advances to v54 without a carrier column or parallel graph. The registry census therefore remains
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`.

The fixed canary now stops at row4368, node2 of
`((result + left) % modulus)`. The next exact Long addition consumer must first
identify the language-owned overflow contract. It does not authorize Int-add
identity aliasing, raw C signed UB, routine/backend exceptions, or a new
top-level SoT row.

## Long wrap arithmetic consumers

Exact Long addition, multiplication, and subtraction add no top-level registry
family. Append-only expression identities 78, 81, and 82 are owned by the
existing typed expression kind/readiness family. The language arithmetic UB
model remains authoritative for wrap semantics; common C/LLVM emitters are the
last consumers and checked-runtime rows are not synthesized for these
operations. Checked Long division identity 79 and Long inequality identity 80
remain separate exact consumers. The registry census therefore remains
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`.

The fixed canary now stops at row4397 node12 in raw routine row261
`MirAbiLayoutHashString`, the `type_name Long` child of
`CharCode(value, n, i) as Long`. The next consumer may add an append-only exact
`Int -> Long` cast identity only after source, target, and graph shape are
explicit. It does not authorize arbitrary casts, backend-local type-name
parsing, routine exceptions, or a new top-level SoT row.

## Exact Int-to-Long cast consumer

Exact `TypeName(Long)` and `Cast(Int, Long)` add no top-level registry family.
Append-only identities 83/84 are owned by one numeric-cast kind/readiness
family, and the common C/LLVM expression projections are the last consumers.
The physical GraphPlan representation is `long long`/`i64` for both Int and
Long, but the semantic source/target distinction remains sealed in the typed
expression facts. Arbitrary casts, arbitrary type-name leaves, and backend
type inference remain negative-gated. The registry census stays
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`.

The fixed canary now stops at row4402 node2 in raw routine row262
`MirAbiLayoutHashU32`, the exact Long-less root of `unsigned_value < 0L`. The
next consumer may append one typed Long-less comparison identity; it does not
authorize Int identity reuse, mixed operands, routine exceptions, or a new
top-level SoT row.

## Exact Long-less comparison consumer

Exact `Long < Long -> Bool` adds no top-level registry family. Append-only
identity 85 is owned by the existing typed comparison kind/readiness family;
all earlier Int and Long identities stay unchanged. Common C/LLVM expression
and branch emitters remain the last consumers. GraphPlan advances to v61 with
no carrier column or parallel graph, so the registry census stays
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`.

The fixed canary now stops at row4513 node16 in raw routine row268
`MirAbiLayoutFieldsCaptureWithin`, the outer spine of
`[(0 - 1), (0 - 1), (0 - 1), (0 - 1)]`. The next consumer may extend the
existing populated `Array<Int>` literal operand owner to ordered admitted Int
expression roots; it does not authorize source-text evaluation, arbitrary
element types, routine/backend exceptions, a second graph, or a new top-level
SoT row.

## Local logical-record Array consumers

Exact local `Array<LogicalRecord>` storage and its empty literal add no
top-level registry family. Append-only expression identity 88 consumes the
existing declaration-keyed logical-record inventory and nominal-array target;
CFG typed/value plans, direct-call value-result identity, and common C/LLVM
emitters are the last consumers. The public Array ABI is not substituted and no
parallel layout or type authority was created. The registry census therefore
stays `49 CLOSED / 36 BRIDGE / 1 ACTIVE`.

The fixed canary now stops at global row7044 in routine index613
`SemanticExpressionGraphFactsFromAstRows`, at the value-formal leaf of
`rows.roots[i]`. The next consumer may join that formal with the existing
logical-record member and ArrayInt facts, but it does not authorize a new SoT
row, member-name guessing, source-text evaluation, backend MIR reads, or a
second expression graph.

## Expr0 lane and Bool inequality consumers

The row7044 repair adds no top-level owner or expression identity. It makes the
existing LocalRef wire's `expr0` ownership explicit at its leaf-admission
consumer, preventing later expression lanes from reinterpreting a coincident
node ordinal. GraphPlan v69 then appends exact Bool-inequality identity 89 to
the existing typed comparison family; Bool equality and inequality have one
kind/readiness owner and common C/LLVM consumers. The registry census remains
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`.

The fixed canary now stops at row7201 node1 in routine index625
`LanguageWordSpelling`, the declaration/variant side of
`id == LanguageWordId.WordAbility`. The next consumer may join an exact
value-formal payload-free enum to an existing declaration-owned variant fact.
It does not authorize a new top-level row, enum spelling or ordinal inference,
source evaluation, backend MIR reads, or a parallel expression graph.

## Payload-free enum expression consumers

Exact payload-free enum variant/equality adds no top-level registry family.
GraphPlan v70 identities90/91 consume the existing declaration/variant
inventory and scalar-ordinal representation. Expression readiness rejoins the
stored variant ordinal to that fact; C/LLVM direct-call and comparison emitters
are the last consumers. Enum/routine spelling, ordinal inference, generic
member fallback, and backend MIR rereads remain negative-gated. The registry
census stays `49 CLOSED / 36 BRIDGE / 1 ACTIVE`.

The fixed canary now stops at row8457 node2 in routine index649
`ParserExpressionLeaf`, exact source `[text]`. The next consumer may extend the
existing populated Array<String> literal owner to one exact value-formal String
element. It does not authorize a new top-level row, arbitrary expression
elements, source evaluation, or a second array graph.

## Owner-handle call and ArrayString Set consumer closure

The exact logical-record `owner-handle` call and target-neutral ArrayString Set
identity34 add no top-level registry family. The former consumes the existing
callable signature and declaration-keyed logical-record inventory; the latter
consumes the existing ArrayString ABI fact plus producer-owned LocalRef and
formal ordinal. Common C/LLVM call, checked-set, and copyout emitters remain the
last consumers. No move/lifetime authority, opcode, layout owner, or backend MIR
read was added, so the registry census stays
`49 CLOSED / 36 BRIDGE / 1 ACTIVE`.

The fixed canary now stops at raw routine index1111
`CompilerSymbolCIdentifier`, source syntax ID28197, at `stage=local_inventory`.
The exact reached unsupported locals are `Allocator` and `TextBuilder`.
`abi_layout_row_owner.pgy` and `runtime_call_abi_row_owner.pgy` already own their
physical and call identities, so the next consumer seam must derive one general
runtime-value representation from those rows. A scalar-type allowlist edit,
separate type-specific GraphPlans, copied layouts, or a new top-level SoT row is
not authorized.

## Typed logical-record indexed-array assignment consumer

The reached nested indexed assignment adds no top-level registry row and does
not create a second record, collection, or opcode authority. Stable operation
44 consumes the existing declaration-keyed logical-record field inventory,
routine LocalRef/value-type plan, formal carriage/ordinal, persisted
`expr1_graph`, and resolved `Array<Int>`/`Array<String>` terminal type. It
accepts both a routine-local root and the exact logical-record value-result
parameter root. The full target graph remains the secondary operation
expression, the RHS is primary, and the typed fact carries the element type.

The canonical latest-dominating LocalRef row owns predecessor selection. A
first value-result mutation may start at the parameter entry, but an existing
explicit predecessor must occur as the first persisted use; missing or stale
predecessors fail closed. C and LLVM root the member chain at that admitted
operation-result LocalRef and finish through the existing bounds-checked
`pgy_ai_set` or `pgy_as_set`. Source-path splitting, member-name lookup in a
backend, fixed nesting depth, parameter-entry fallback after an SSA
definition, and a parallel value-result opcode are forbidden.

The original focused gate still covers a three-member local `Array<String>`
path and eight damaged identity/use/edge/source facts. The value-result
`Array<Int>` gate covers two ordered writes, exact C/LLVM stdout `8`, copyout,
and eight formal-binding/carriage/predecessor/member/RHS/result mutations. On
the fixed 48,531,749-byte MIR these consumers advance the first failure from
global row 17147 through row 17618 to row 17851.

## Populated ArrayInt local-SSA literal consumer

The existing populated `Array<Int>` literal owner now consumes an element leaf
from the canonical instruction-use, LocalRef, and value-type plans in addition
to its existing literal, zero-call, formal-parameter, and computed Int forms.
It advances the caller's ordered use cursor and persists the admitted local row
in the same expression arena. Readiness and C/LLVM materializers already own
the local operand kind, so no new expression identity, operation, ABI row, or
backend route is introduced. Missing or foreign use identity, an untyped local,
spelling inference, and backend MIR rereads remain forbidden.

The strengthened existing fixture reproduces the old
`stage=literal-uses node=2` failure with `let pending: Array<Int> = [root_id]`.
The current driver executes that form and all existing operand families in C
and LLVM and rejects `local-missing-use`. On the same fixed MIR this advances
the first failure from row 17851 to row 18392. The next RED is routine 1197
`SemanticAstAnalysisResolveExpressionPlacesFromAdmittedBody`: local SSA
`graph.21` is assigned into value-result member target
`analysis.expression_surfaces.expression_graph` and fails at
`stage=admitted-type`. That existing member-rebind type join is the next
consumer seam; it is not permission for a new operation, V, cache, shard,
timeout, cap, or top-level SoT row.
