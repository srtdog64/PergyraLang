# PergyraLang Documentation Index

Last updated: 2026-06-25

Anti-hype update: 2026-04-29

This index is the human entry point for the current beta-closure documentation.
It intentionally prioritizes the frozen beta contract, implementation evidence,
and follow-up debt over older design notes.

## Beta Closure Source Of Truth

| Document | Purpose |
|---|---|
| [`100_beta_readiness_checklist.md`](100_beta_readiness_checklist.md) | Lightweight index for the split beta readiness checklist |
| [`100a_beta_active_status.md`](100a_beta_active_status.md) | Active status, current blockers, and recent closure context |
| [`100b_beta_p0_semantics_systems_air.md`](100b_beta_p0_semantics_systems_air.md) | P0 formal semantics, systems baseline, CFG, AIR, and compiler quality gates |
| [`100c_beta_dag_mir_abi_runtime.md`](100c_beta_dag_mir_abi_runtime.md) | DAG, MIR, ABI/runtime, parallel, and pain-point closure gates |
| [`100d_beta_execution_log.md`](100d_beta_execution_log.md) | Immediate execution order and historical progress log |
| [`125_source_of_truth_spine.md`](125_source_of_truth_spine.md) | Compiler source-of-truth ownership spine to stop A -> B -> A refactoring loops |
| [`133_beta_completed_closure_archive.md`](133_beta_completed_closure_archive.md) | Completed beta-closure evidence moved out of the active checklist |
| [`19_design_philosophy.md`](19_design_philosophy.md) | Systems-language identity and non-negotiable substrate baseline |
| [`107_beta_stable_subset.md`](107_beta_stable_subset.md) | Single freeze point for the beta-stable language subset |
| [`111_beta_test_suite_freeze.md`](111_beta_test_suite_freeze.md) | Mandatory pre-beta gate inventory |
| [`70_beta_closure_master_board.md`](70_beta_closure_master_board.md) | B0/B1/B2 closure board and prioritization |
| [`71_beta_execution_tickets.md`](71_beta_execution_tickets.md) | Execution-ticket breakdown for beta closure |
| [`120_vision_and_capability_audit.md`](120_vision_and_capability_audit.md) | Anti-hype current-vs-vision audit for external claims |

## Post-Beta Self-Hosting Track

| Document | Purpose |
|---|---|
| [`self_hosted/README.md`](self_hosted/README.md) | Entry point for post-beta self-hosting preparation |
| [`self_hosted/00_agent_entry.md`](self_hosted/00_agent_entry.md) | Agent handoff rules for self-hosted work |
| [`self_hosted/01_staged_roadmap.md`](self_hosted/01_staged_roadmap.md) | Staged roadmap from soft self-host tools to compiler slices |
| [`self_hosted/02_required_language_surface.md`](self_hosted/02_required_language_surface.md) | Language surface required before self-hosting can become credible |
| [`self_hosted/03_tool_candidates.md`](self_hosted/03_tool_candidates.md) | First self-hostable tool candidates and non-goals |
| [`self_hosted/04_beta_exit_handoff.md`](self_hosted/04_beta_exit_handoff.md) | Exact beta-exit artifacts required before self-host migration starts |
| [`self_hosted/05_compiler_core_gap_analysis.md`](self_hosted/05_compiler_core_gap_analysis.md) | Hard self-host gap analysis and substrate entry criteria |
| [`self_hosted/06_self_host_groundwork_readiness.md`](self_hosted/06_self_host_groundwork_readiness.md) | Self-host substrate readiness and first AIR graph consumer slice |
| [`self_hosted/10_hard_self_host_contract.md`](self_hosted/10_hard_self_host_contract.md) | Active hard self-host substitution contract and SoT pass condition |
| [`self_hosted/11_compiler_world_architecture.md`](self_hosted/11_compiler_world_architecture.md) | PgyCompilerWorld self-host compiler shape: stage facts remain owned, compiler flow is one Pergyra world |
| [`self_hosted/12_intent_zone_self_host_architecture.md`](self_hosted/12_intent_zone_self_host_architecture.md) | Intent/zone architecture for self-hosted compiler growth, codegen resources, and path/source intake facts |
| [`self_hosted/13_compiler_substrate_architecture.md`](self_hosted/13_compiler_substrate_architecture.md) | Self-hosted compiler architecture stack: codegen resources, compiler-world fact owners, import graph, deterministic facts, runtime materialization, caching, and parity promotion |
| [`self_hosted/14_target_compiler_world.md`](self_hosted/14_target_compiler_world.md) | Target compiler world: fact zones -> single Codegen Projection intent -> C/LLVM/SelfHosted emission peers -> Artifact Zone parity sink |

## Historical Snapshots

| Document | Purpose |
|---|---|
| [`98_beta_closure_readiness_report.md`](98_beta_closure_readiness_report.md) | Historical readiness snapshot; do not cite as the current beta verdict |

## Core Semantics

| Document | Purpose |
|---|---|
| [`102_formal_semantics_and_proof_obligations.md`](102_formal_semantics_and_proof_obligations.md) | Formal proof entry point |
| [`semantics/`](semantics/) | Split formal semantics and proof notes |
| [`semantics/09_abstraction_loss_contracts.md`](semantics/09_abstraction_loss_contracts.md) | Abstraction loss contract rules for compiler and tooling boundaries |
| [`semantics/pass_contract_manifest.md`](semantics/pass_contract_manifest.md) | Pass-level fact contract manifest for CFG/MIR, AIR, DAG/type-resolution, MIR/LLVM declaration parity, and ABI/Slot/Pin closure |
| [`semantics/10_behavior_contract_closure_gaps.md`](semantics/10_behavior_contract_closure_gaps.md) | Remaining proof gaps before calling behavior contracts a closed calculus |
| [`semantics/18_machine_neutral_compute.md`](semantics/18_machine_neutral_compute.md) | Machine-neutral compute contract: C/LLVM as first projections, AIR/MIR/ABI facts as the long-term target-independent source of truth |
| [`semantics/19_theoretical_foundations.md`](semantics/19_theoretical_foundations.md) | Theory-lineage bibliography and synthesis boundary: citations anchor each axis, but the open proof target is Pergyra's own abstract machine/core calculus |
| [`103_cfg_body_dataflow_need.md`](103_cfg_body_dataflow_need.md) | Why CFG/body dataflow is required for beta-grade ownership and cleanup |
| [`104_air_compiler_architecture.md`](104_air_compiler_architecture.md) | AIR architecture and abstraction drift model |
| [`105_runtime_panic_contract.md`](105_runtime_panic_contract.md) | Runtime panic and hard-fail contract |
| [`106_ownership_model_comparison.md`](106_ownership_model_comparison.md) | Ownership comparison and Option C lift |

## Async, Parallel, And Memory

| Document | Purpose |
|---|---|
| [`05_async_concurrency.md`](05_async_concurrency.md) | User-facing async/concurrency guide |
| [`113_memory_concurrency_model.md`](113_memory_concurrency_model.md) | Frozen beta memory/concurrency contract |
| [`114_async_model_positioning.md`](114_async_model_positioning.md) | Async positioning: coloring decomposition, not coloring avoidance |
| [`53_parallel_core_policy.md`](53_parallel_core_policy.md) | Parallel core policy |
| [`54_parallel_execution_relayout_board.md`](54_parallel_execution_relayout_board.md) | Parallel execution implementation board |
| [`74_slot_pinning_caching.md`](74_slot_pinning_caching.md) | Slot pinning / lease / view rules |

## Runtime, ABI, And Backend

| Document | Purpose |
|---|---|
| [`36_ir_pipeline_architecture.md`](36_ir_pipeline_architecture.md) | Compiler IR pipeline overview |
| [`37_compiler_contracts.md`](37_compiler_contracts.md) | Compiler contracts |
| [`38_mir_only_backend_migration.md`](38_mir_only_backend_migration.md) | MIR-only backend migration |
| [`38_c_macro_deception_and_abi.md`](38_c_macro_deception_and_abi.md) | C macro and ABI lessons |
| [`39_test_driven_abi_and_explicit_lowering.md`](39_test_driven_abi_and_explicit_lowering.md) | Test-driven ABI and explicit lowering |
| [`40_lowering_rules.md`](40_lowering_rules.md) | Lowering rule catalog |
| [`44_llvm_backend_coverage.md`](44_llvm_backend_coverage.md) | LLVM backend coverage map |
| [`48_abi_performance_contract.md`](48_abi_performance_contract.md) | ABI performance contract |
| [`51_c_backend_reference_policy.md`](51_c_backend_reference_policy.md) | C backend reference policy |
| [`52_llvm_native_first_roadmap.md`](52_llvm_native_first_roadmap.md) | LLVM/native-first roadmap |
| [`62_llvm_backend_debt_ledger.md`](62_llvm_backend_debt_ledger.md) | LLVM backend remaining debt |
| [`92_inc_split_roadmap.md`](92_inc_split_roadmap.md) | Include-split cleanup roadmap |
| [`115_inc_cleanup_status.md`](115_inc_cleanup_status.md) | Current `.inc` cleanup ledger |
| [`known_bug_if_call_assign_then_let_then_if.md`](known_bug_if_call_assign_then_let_then_if.md) | Open lowering bug: outer call assignment inside `if` can be dropped in a narrow CFG shape |
| [`128_pointer_risk_register.md`](128_pointer_risk_register.md) | Pointer/lifetime risk register for ABI, containers, scratch buffers, and raw escape |
| [`132_unsafe_capability_scope.md`](132_unsafe_capability_scope.md) | Unsafe capability scope contract |
| [`135_backend_wasm_pointer_closure.md`](135_backend_wasm_pointer_closure.md) | Backend/WASM/pointer wording guard: verified subset, named debt, and non-overclaiming lifetime status |
| [`136_abi_niche_and_explicit_layout.md`](136_abi_niche_and_explicit_layout.md) | ABI niche optimization and explicit layout policy: current tagged Option ABI, future proof gates, and raw/extern-only layout scope |
| [`139_golden_adt_verification_methodology.md`](139_golden_adt_verification_methodology.md) | Golden, ADT, and verification methodology: evidence ladder, ADT owner rules, differential/property/model-check/proof roles, and hard self-host review checklist |
| [`141_closure_capture_design.md`](141_closure_capture_design.md) | Closure capture design: local capture gap, explicit environment ownership, and backend lowering obligations |
| [`142_evidence_driven_guard_amortization.md`](142_evidence_driven_guard_amortization.md) | Evidence-driven guard amortization: measured preflight-view gate for slot-style hot paths |
| [`143_evidence_parameter_attributes.md`](143_evidence_parameter_attributes.md) | Evidence-projected LLVM parameter attributes: ownership evidence lowered to backend optimization facts |
| [`145_bit_layout_boundary_matrix.md`](145_bit_layout_boundary_matrix.md) | Bit/layout boundary matrix: explicit bit-order value conversion, world-bound reinterpretation, and language-by-language gap tracking |

## Language Surface

| Document | Purpose |
|---|---|
| [`00_vision.md`](00_vision.md) | Language vision |
| [`00_engine_core_spec.md`](00_engine_core_spec.md) | Engine/core spec |
| [`01_intent_first_design.md`](01_intent_first_design.md) | Intent-first design |
| [`04_generic_design.md`](04_generic_design.md) | Generic design |
| [`07_error_handling.md`](07_error_handling.md) | Error handling model |
| [`08_module_system.md`](08_module_system.md) | Module system design |
| [`21_slot_relation_model.md`](21_slot_relation_model.md) | Slot relation model |
| [`22_ownership_model.md`](22_ownership_model.md) | Ownership model |
| [`22_class_object_model.md`](22_class_object_model.md) | Class/object model |
| [`24_visibility_model.md`](24_visibility_model.md) | Visibility model |
| [`25_declaration_vs_instantiation.md`](25_declaration_vs_instantiation.md) | Declaration vs instantiation |
| [`26_vessel_action_model.md`](26_vessel_action_model.md) | Vessel/action model |
| [`42_keyword_orthogonality.md`](42_keyword_orthogonality.md) | Keyword orthogonality |
| [`45_math_layer_design.md`](45_math_layer_design.md) | Math layer design |
| [`46_texmath_spec.md`](46_texmath_spec.md) | TeX math spec |

## Modules And Standard Library

| Document | Purpose |
|---|---|
| [`99_language_module_taxonomy.md`](99_language_module_taxonomy.md) | Core/foundation/execution/compat/domain-kit taxonomy |
| [`language_module_manifest.json`](language_module_manifest.json) | Machine-readable module taxonomy |
| [`language_module_cases.json`](language_module_cases.json) | Module-layer case manifest |
| [`108_stdlib_beta_freeze.md`](108_stdlib_beta_freeze.md) | Stdlib beta freeze list |
| [`109_package_module_resolver_contract.md`](109_package_module_resolver_contract.md) | Seashell package/module resolver contract: `pgy.toml` remains a fail-closed TOML subset while `pgy.seashell.v1` owns local package/build declaration |
| [`29_stdlib_design.md`](29_stdlib_design.md) | Stdlib design |
| [`67_layered_stdlib_and_domain_kits.md`](67_layered_stdlib_and_domain_kits.md) | Layered stdlib and domain kit policy |

## Diagnostics, Observability, And Docs Quality

| Document | Purpose |
|---|---|
| [`72_diagnostic_codes.md`](72_diagnostic_codes.md) | Diagnostic code registry |
| [`73_diagnostic_vocabulary.md`](73_diagnostic_vocabulary.md) | Diagnostic vocabulary |
| [`110_string_unicode_policy.md`](110_string_unicode_policy.md) | String/unicode beta policy |
| [`112_observability_trace_schema.md`](112_observability_trace_schema.md) | Observability and trace schema |
| [`116_documentation_quality_audit.md`](116_documentation_quality_audit.md) | Documentation quality audit and next cleanup priorities |
| [`118_slot_model_rigor_audit.md`](118_slot_model_rigor_audit.md) | Slot/ownership rigor audit and forbidden marketing vocabulary |
| [`119_pergyra_lineage_positioning.md`](119_pergyra_lineage_positioning.md) | Lineage positioning without feature-parity claims |
| [`120_vision_and_capability_audit.md`](120_vision_and_capability_audit.md) | Anti-hype capability audit: current state vs aspiration |
| [`121_types_as_domain_medium.md`](121_types_as_domain_medium.md) | Type-system mandate and subject/authority/projection modeling guard |
| [`122_managing_intent_drift.md`](122_managing_intent_drift.md) | Drift management discipline: 5×5 matrix and recognition signals |
| [`123_terminal_output_standard.md`](123_terminal_output_standard.md) | Terminal-output 3-tier architecture: Core / Stream CLI / Grid TUI |
| [`124_syntax_pattern_matrix.md`](124_syntax_pattern_matrix.md) | Syntax pattern matrix: Pergyra vs C# / Rust / TS / Python / Go / Swift; gap + unique surface tracking |
| [`127_compiler_speed_engineering.md`](127_compiler_speed_engineering.md) | Compile-speed / engineering discipline borrowed from D (DMD/LDC/GDC); measurement plan and anti-hype constraints |
| [`129_tex_semantics_lessons.md`](129_tex_semantics_lessons.md) | TeX-derived contract lessons for scanner boundaries, delayed effects, planner-only parameters, probe-order semantics, token identity, recovery artifacts, and reviewable semantic fixtures |
| [`130_c_backend_owner_migration_map.md`](130_c_backend_owner_migration_map.md) | C backend owner migration map and guardrails for avoiding mechanical helper/header churn |
| [`131_ai_coding_atomic_units.md`](131_ai_coding_atomic_units.md) | AI-coding thesis: verifiable intent atoms, pattern-context units, and specification gradients |
| [`134_language_surface_hygiene.md`](134_language_surface_hygiene.md) | Language surface hygiene: keep orthogonal terms, close alias/fallback source-of-truth seams |

## Implementation Guides

| Document | Purpose |
|---|---|
| [`20_compiler_pipeline_guide.md`](20_compiler_pipeline_guide.md) | Contributor guide for the compiler pipeline |
| [`66_semantic_implementation_map.md`](66_semantic_implementation_map.md) | Semantic implementation map |
| [`94_arena_index_lifetime_plan.md`](94_arena_index_lifetime_plan.md) | Arena/index lifetime plan |
| [`95_ast_dispatch_partition.md`](95_ast_dispatch_partition.md) | AST dispatch partition |
| [`101_semantic_split_template.md`](101_semantic_split_template.md) | Semantic split template |

## Practical Reading Order

1. Read [`107_beta_stable_subset.md`](107_beta_stable_subset.md).
2. Read [`100_beta_readiness_checklist.md`](100_beta_readiness_checklist.md).
3. Read [`125_source_of_truth_spine.md`](125_source_of_truth_spine.md) before changing compiler architecture.
4. For async/parallel, read [`113_memory_concurrency_model.md`](113_memory_concurrency_model.md), then [`114_async_model_positioning.md`](114_async_model_positioning.md), then [`05_async_concurrency.md`](05_async_concurrency.md).
5. For backend work, read [`135_backend_wasm_pointer_closure.md`](135_backend_wasm_pointer_closure.md), [`115_inc_cleanup_status.md`](115_inc_cleanup_status.md), [`62_llvm_backend_debt_ledger.md`](62_llvm_backend_debt_ledger.md), and [`44_llvm_backend_coverage.md`](44_llvm_backend_coverage.md).
6. For formal closure, read [`102_formal_semantics_and_proof_obligations.md`](102_formal_semantics_and_proof_obligations.md), [`103_cfg_body_dataflow_need.md`](103_cfg_body_dataflow_need.md), and [`104_air_compiler_architecture.md`](104_air_compiler_architecture.md).
7. For post-beta self-hosting preparation, read [`self_hosted/README.md`](self_hosted/README.md) after the beta source-of-truth documents, not before them.

## Current Documentation Policy

- A doc may describe future design only if it explicitly labels the surface as
  out-of-beta or post-beta.
- Beta-stable claims must point to a smoke/regression gate.
- User-facing examples must not rely on parser-accepted surfaces whose semantic,
  runtime, C, LLVM, diagnostic, and test contracts are not closed.
- The docs should prefer UTF-8 text and avoid stale mojibake; if a file cannot
  be fixed immediately, a newer source-of-truth document must supersede it.
- External-facing wording must pass the anti-hype triad:
  `118_slot_model_rigor_audit.md` for vocabulary,
  `119_pergyra_lineage_positioning.md` for lineage, and
  `120_vision_and_capability_audit.md` for current capability vs aspiration.
- Type-system and modeling claims must also pass
  `121_types_as_domain_medium.md`: `subject` is not "important information,"
  `authority` is not an importance ranking, and selective information exposure
  belongs to projection/visibility.
