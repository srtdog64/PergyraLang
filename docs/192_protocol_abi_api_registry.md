# Protocol, ABI, and API Registry

Status: `derived-crosswalk, gate-backed`
Date: 2026-07-20

This document is the single crosswalk for the compiler's externally visible
and cross-stage protocols. It is **not** a new source of truth. The existing
owner in `docs/semantics/sot_owner_spine_registry.md` remains authoritative
for each fact family; this registry only joins the owner to its wire/layout,
producer, final consumer, projections, failure boundary, compatibility rule,
and executable gate.

The protocol row is not `CLOSED` merely because a producer or a serializer
exists. A row is `IMPLEMENTED` only when the owner, all named consumers, the
missing-fact failure, and the gate are present. `BRIDGE` means the current
slice is executable but an owner, consumer migration, or compatibility edge
is still open. `OPEN` is an explicit documentation/implementation gap.

## Row contract

```text
protocol_id | owner_ref | wire_layout | producer | last_consumer | projection | missing_fact_failure | compatibility_policy | gate | status
```

- `protocol_id` is the stable protocol identity and version. `unversioned` is
  allowed only as an explicit gap marker.
- `owner_ref` is `registry:<owner_id>` for an existing SoT owner, or an
  explicit `UNREGISTERED:<reason>` when the protocol has no owner row yet.
- Other cells are `path#term` references separated by `;`. A path without a
  term is an inventory reference only and is still required to exist.
- `UNSPECIFIED:<reason>` is an explicit compatibility/gate gap. It must not be
  used on an `IMPLEMENTED` row.
- `status` describes this protocol crosswalk, not the status of a single
  source file or a bounded parity fixture.

<!-- BEGIN protocol-abi-api-registry -->
```text
pergyra.abi.core.v1 | registry:abi.layout_rows | src/runtime/pgy_abi_spec.h#Pergyra ABI Specification (Single Source of Truth) | src/compiler/mir_abi_layout.c#mir_abi_lookup | src/codegen/transpiler_entry.c;src/codegen/llvm_api.c | src/codegen/transpiler_slot_runtime_row.c#MIR ABI runtime function row;src/codegen/llvm_expr_slot_device_calls.c#MIR ABI runtime function row;src/self_hosted/compiler/abi_layout_row_manifest.pgy#CompilerAbiLayoutManifestRowAt | src/codegen/transpiler_mir_resource_op_core.c#missing runtime ABI layout metadata | docs/136_abi_niche_and_explicit_layout.md#MIR ABI fact must be the only backend input | Makefile#test-abi;tests/abi_ownership_shape_smoke.sh#MIR_ABI_REPR_EXPLICIT_TAG | BRIDGE
pergyra.mir-json.v1 | registry:mir.execution_graph | src/compiler/mir.h#schema pgy.mir.v1 | src/compiler/mir_json_dump.c#schema pgy.mir.v1;src/self_hosted/mir/json_projection_owner.pgy#pgy.mir.v1 | src/compiler/mir_json_dump.c;src/self_hosted/mir_lower/mir_json_input_owner.pgy;src/codegen/transpiler_entry.c;src/codegen/llvm_api.c | src/self_hosted/mir/json_projection_owner.pgy#JsonEmitFieldString("schema", "pgy.mir.v1");src/self_hosted/mir_lower/mir_json_input_owner.pgy#MirDocumentSchemaEquals(json, "pgy.mir.v1") | src/self_hosted/mir_lower/routine_lower.pgy#routine MIR fact index is incomplete | tests/self_hosted/parity/mir_json_parity.sh#schema acceptance from becoming an ignore-unknown compatibility fallback | Makefile#self-host-mir-json-parity-test-smoke;tests/self_hosted/parity/mir_json_parity.sh#schema acceptance | BRIDGE
pergyra.runtime-call-abi.v2 | UNREGISTERED:runtime-call-abi-row-authority | src/self_hosted/compiler/expected/runtime_call_abi_rows.txt#schema=pgy.selfhost.runtime-call-abi-row.v2 | src/compiler/mir_abi_resource_runtime.c#mir_abi_resource_runtime_fn;src/self_hosted/compiler/runtime_call_abi_row_manifest.pgy#CompilerRuntimeCallAbiManifestRowAt | src/test_mir.c;src/codegen/transpiler_slot_runtime_row.c;src/codegen/llvm_slot_runtime_row_for_operation;src/codegen/llvm_expr_slot_device_calls.c;src/codegen/llvm_expr_identifier_slot_helpers.c;src/codegen/llvm_expr_call_methods_domain_slice.c;src/codegen/llvm_expr_assignment_member_projection.c;src/codegen/llvm_stmt_let_resources.c;src/codegen/llvm_stmt_with.c;src/codegen/llvm_stmt_block.c;src/codegen/llvm_mir_pin_region.c;src/codegen/llvm_runtime_secure_slot_decl.c | src/self_hosted/compiler/runtime_call_abi_row_manifest.pgy#CompilerRuntimeCallAbiRowsReady;src/self_hosted/compiler/expected/runtime_call_abi_rows.txt#count=245 | src/codegen/transpiler_mir_resource_op_core.c#missing runtime ABI layout metadata;src/codegen/llvm_runtime.c#LLVM slot operation call-shape/machine-layer mismatch | UNSPECIFIED:runtime-call-row-migration-policy | Makefile#abi-ownership-shape-test-smoke;Makefile#backend-fail-closed-test-smoke;Makefile#perf-contract-test-smoke;Makefile#self-host-runtime-call-abi-row-parity-test-smoke;tests/self_hosted/parity/runtime_call_abi_row_manifest_parity.sh | BRIDGE
pergyra.machine-layer.declaration.v1 | registry:semantic.machine_layer_transition | tests/machine_layer_pipeline_smoke.sh#pgy.machine-layer.declaration.v1 | src/compiler/machine_layer_manifest.c#pgy_machine_layer_manifest_dump_json | src/compiler/verified_projection_plan.c;src/codegen/transpiler_entry.c;src/codegen/llvm_api.c;src/self_hosted/compiler/machine_layer_declaration_consumer.pgy | src/compiler/verified_projection_plan.c#pgy_machine_layer_manifest_fingerprint;src/codegen/transpiler_context.c#transpiler_machine_layer_projection_is_bound;src/codegen/llvm_api.c#llvm_machine_layer_projection_is_bound;src/self_hosted/compiler/machine_layer_declaration_consumer.pgy#pgy.machine-layer.declaration.v1 | src/compiler/machine_layer_manifest.c#machine layer: missing target manifest | src/self_hosted/compiler/machine_layer_declaration_consumer.pgy#pgy.machine-layer.declaration.v1 | Makefile#machine-layer-pipeline-test-smoke;tests/machine_layer_pipeline_smoke.sh#pgy_machine_layer_manifest_validate_site | BRIDGE
pergyra.lsp.json-rpc.v1 | UNREGISTERED:lsp-protocol-authority | src/lsp/pgy_lsp_protocol.c#LSP wire helpers | src/lsp/pgy_lsp.c#Content-Length:;src/self_hosted/lsp/transport_owner.pgy#LspTransportSchema | src/lsp/pgy_lsp.c;src/self_hosted/lsp/transport_owner.pgy;src/self_hosted/lsp/diagnostics_owner.pgy | src/lsp/pgy_lsp.c#textDocument/publishDiagnostics;src/self_hosted/lsp/transport_owner.pgy#LspTransportFrameContractReady | src/self_hosted/lsp/transport_owner.pgy#missing_content_length | UNSPECIFIED:lsp-session-compatibility-policy | Makefile#self-host-lsp-diagnostics-parity-test-smoke;Makefile#self-host-lsp-transport-frame-parity-test-smoke | BRIDGE
pergyra.compiler-lowering-api.unversioned | UNREGISTERED:mir-lower-public-api-authority | src/compiler/mir_lower_public_api.h#MIRProgram *mir_lower | src/compiler/mir.c#mir_lower | src/compiler/driver_app.c;src/codegen/transpiler_entry.c;src/codegen/llvm_api.c | src/compiler/driver_app.c#mir = mir_lower;src/self_hosted/mir_lower/routine_lower.pgy#MIR routine CFG fact lowering owner | src/compiler/mir.c#MIR lowering requires HIR and semantic facts | UNSPECIFIED:public-lowering-api-version-policy | UNSPECIFIED:public-lowering-api-gate | OPEN
```
<!-- END protocol-abi-api-registry -->

## What this exposes

The core ABI and MIR JSON rows have real executable producers, consumers, and
negative gates. They are still `BRIDGE` because the whole aggregate/runtime
surface is not migrated behind one owner. The active LLVM Slot/resource
consumers now read concrete rows and call shapes through one helper, but
runtime-call ABI rows still have no dedicated top-level SoT owner or
compatibility policy. LSP has staged self-host transport/diagnostic owners, but no single
protocol authority row and no session compatibility policy. The public MIR
lowering header is an API boundary, not yet a versioned protocol with a
declared owner or gate.

This is why SoT closure stalls even when individual parity tests pass:

1. **Owner identity is incomplete.** Some protocol surfaces (runtime-call
   rows, LSP, public lowering API) have manifests or headers but no matching
   top-level owner row. A manifest is a projection, not authority.
2. **The last consumer is wider than the current fixture.** C/LLVM, runtime,
   self-host, driver, and artifact paths must all consume the same row. A
   bounded self-host parity result cannot promote a row while another native
   consumer still reconstructs or guesses the fact.
3. **Compatibility is not a data field yet.** `pgy.mir.v1` and the runtime-call
   row schema are named, but migration/unknown-field/old-artifact policy is not
   uniformly owned. Without that policy, a byte-equal current output does not
   prove old artifacts are safe.
4. **Missing-fact negatives are local, not global.** Several gates reject a
   missing fact at one last consumer, but there is not yet a single protocol
   gate proving every producer/consumer pair fails closed. The registry keeps
   those gaps visible instead of marking them `CLOSED`.
5. **Self-host is a projection frontier, not a whole replacement.** The
   self-host compiler consumes many of the same artifacts, but the released
   driver/LSP and full semantic/MIR replacement remain partial. Therefore the
   protocol can be wired for a rung without being globally self-hosted.

The registry validator checks only this crosswalk's integrity: unique IDs,
explicit version/gap markers, valid owner references, existing source paths,
and non-empty failure/gate references. It is a subcheck of the canonical
`sot-authority-edge-test-smoke`; it is not a second Gate SoT or dashboard row.
It deliberately does not replace the ABI, MIR, machine-layer, LSP, or SoT
authority gates named in each row.

Related source-of-truth and status documents:

- [`docs/semantics/sot_owner_spine_registry.md`](semantics/sot_owner_spine_registry.md)
- [`docs/47_abi_alignment_audit.md`](47_abi_alignment_audit.md)
- [`docs/180_compiler_logical_spine_handles_gates.md`](180_compiler_logical_spine_handles_gates.md)
- [`docs/185_sot_gate_catalog.md`](185_sot_gate_catalog.md)
- [`docs/150_selfhost_driver_lsp_wiring.md`](150_selfhost_driver_lsp_wiring.md)
