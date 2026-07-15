# Compiler, IR, ABI, And Backends

This facade follows facts from source ownership to machine projection.

## Spine

1. [`../20_compiler_pipeline_guide.md`](../20_compiler_pipeline_guide.md) - contributor pipeline overview.
2. [`../36_ir_pipeline_architecture.md`](../36_ir_pipeline_architecture.md) - IR responsibilities.
3. [`../125_source_of_truth_spine.md`](../125_source_of_truth_spine.md) - fact ownership rules.
4. [`../180_compiler_logical_spine_handles_gates.md`](../180_compiler_logical_spine_handles_gates.md) - current logical spine and ownership migration protocol.
5. [`../semantics/sot_owner_spine_registry.md`](../semantics/sot_owner_spine_registry.md) - canonical top-level fact-owner registry.

## Work Areas

| Concern | Canonical Document |
|---|---|
| Compiler contracts | [`../37_compiler_contracts.md`](../37_compiler_contracts.md) |
| AIR | [`../104_air_compiler_architecture.md`](../104_air_compiler_architecture.md) |
| CFG/body dataflow | [`../103_cfg_body_dataflow_need.md`](../103_cfg_body_dataflow_need.md) |
| MIR-only backend migration | [`../38_mir_only_backend_migration.md`](../38_mir_only_backend_migration.md) |
| ABI ownership and explicit layout | [`../38_c_macro_deception_and_abi.md`](../38_c_macro_deception_and_abi.md), [`../136_abi_niche_and_explicit_layout.md`](../136_abi_niche_and_explicit_layout.md) |
| C backend policy | [`../51_c_backend_reference_policy.md`](../51_c_backend_reference_policy.md) |
| LLVM coverage and debt | [`../44_llvm_backend_coverage.md`](../44_llvm_backend_coverage.md), [`../62_llvm_backend_debt_ledger.md`](../62_llvm_backend_debt_ledger.md) |
| Target-neutral projection | [`../162_target4_unified_mir_consumption_blueprint.md`](../162_target4_unified_mir_consumption_blueprint.md), [`../semantics/18_machine_neutral_compute.md`](../semantics/18_machine_neutral_compute.md) |
| Physical owner clusters | [`../compiler_owner_clusters.tsv`](../compiler_owner_clusters.tsv), [`../92_inc_split_roadmap.md`](../92_inc_split_roadmap.md) |
| SoT gates | [`../185_sot_gate_catalog.md`](../185_sot_gate_catalog.md) |

Backends are consumers of owned facts. A facade link must never be used as
authority for ABI layout, semantic recovery, or runtime-call spelling.
