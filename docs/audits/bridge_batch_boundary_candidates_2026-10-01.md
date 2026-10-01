# Boundary BRIDGE closure candidates

Status: READ-ONLY phase-one findings, not implementation or acceptance evidence.
Observed HEAD: `62a83a8256bf4fa34878cca8f1f14641736fdeba`.
Date: 2026-10-01, Asia/Seoul.
Scope: the ten machine/HIR/DIR/RIR/MIR/generic/AIR/ABI/target/diagnostic rows
assigned in `docs/agent_work_directives/bridge_batch_closure_2026-10-01.md`.
No source, registry, binary, test artifact, or Git index was changed by this
review. Only this report is written. No gate or compiler build was executed.

## Result

No full-row CLOSED candidate is justified by a small final legacy-read deletion
in this scope. Three useful implementation rungs exist below, but they are not
three registry closures. Each broad row retains a real missing contract or
reachable old carrier. They must not be relabelled by treating unimplemented
behavior as outside its existing identity.

The projection review is a better potential shared installed-executable batch
than reopening HIR/RIR/AIR, generic specialization, target profiles, and machine
provider provenance together. The following findings identify bounded deletion
work and explain why it cannot honestly subtract ten BRIDGE rows.

## Existing production reachability

The installed composition is real, not just a declared stage world:

- `driver_bootstrap_main.pgy` constructs `PgyCompilerWorld`.
- `world.pgy#CompileSourceToC` invokes `CompilePergyraCArtifact`, then
  `driver_source_c_execution_owner.pgy#DriverSourceCProducePayloadAdmitted`
  calls `driver_rung2_owner.pgy#CompileSourceToCVerified`. That routine obtains
  verified source MIR before the admitted C consumer.
- `world.pgy#CompileSourceToLlvm` invokes `CompilePergyraProgram`, then
  `driver_source_llvm_intent_execution_owner.pgy#Compile` produces a typed
  source-MIR receipt and calls
  `DriverRung2PublishMirPayloadLlvmArtifactForIdentity`.
- The direct backend consumer constructs `CompilerEmissionArtifact` in
  `direct_mir_backend_projection_owner.pgy`; the source-C and source-LLVM
  publication boundaries consume its carried target identity.

These are source-call-graph observations, not a newly executed gate. The live
ArrayDrop audit records an installed C/LLVM pair at this HEAD's feature boundary;
it does not prove machine-contact, generic, or AIR lifetime completeness.
`tests/machine_layer_pipeline_smoke.sh` explicitly forces the native pipeline;
it must not be reported as default installed source-route substitution.

## 1. Machine-contact AST lookup deletion: smallest residue, not full closure

Owner: `semantic.machine_layer_transition`, currently
`mir_machine_layer.c#mir_attach_machine_layer_fact` plus the checked immutable
machine declaration/projection owners. Last native consumer:
`mir.c#mir_lower` calls `mir_enrich_machine_layer_facts` after statement
instruction population. The self-host producer already uses carried semantic
call-target facts in
`mir/machine_layer_projection_owner.pgy#SelfMirMachineLayerProjectionForGraph`.

Exact old reads:

- `src/compiler/mir_machine_layer.c#mir_attach_machine_layer_fact_for_ast`
  calls `rir_scope_find_op_by_ast`, special-cases `AST_LET_DECL`, and reopens
  `ast_let_initializer` before trying a second pointer lookup.
- `src/compiler/mir_machine_layer.c#mir_enrich_machine_layer_facts` passes
  `inst->ast` into that lookup for every instruction lacking a machine row.
- `src/compiler/rir_public_surface.c#rir_scope_find_op_by_ast` joins an RIR op
  by borrowed AST pointer rather than the owned contact identity.

Exact proposed edit boundary, not an applied/compilable patch:

1. In `rir.h#RIROp`, add a captured expression/contact SyntaxNodeId separate
   from `source_statement_syntax_id`. Capture it once in
   `rir_facts.c#add_op_with_machine_contact_source_statement`, the existing
   initial producer, rather than recovering it at a consumer.
2. In `rir_public_surface.c`/`rir.h`, expose an explicit unique contact lookup
   by that captured ID. Missing, zero, duplicate, or wrong-kind identity is a
   distinguishable refusal, not the first matching op.
3. In the MIR statement/HIR fact producer, carry the exact contact/call ID and
   whether a machine row is required from the admitted call fact. For a let
   initializer this is the call's identity, not the declaration's identity.
4. Replace the AST-taking function in `mir_machine_layer.c` with consumption
   of that exact carrier. Remove its AST_LET branch, both pointer lookups, and
   the `inst->ast` argument in enrichment. Preserve
   `mir_attach_machine_layer_fact(inst, op)` for direct resource instructions;
   `mir_lower_population.c#mir_add_resource_instruction` already consumes an
   RIR op directly.
5. Delete `rir_scope_find_op_by_ast` only after checking its other consumers.
   Add a structural negative ratchet against the retired machine lookup path;
   do not globally delete legitimate AST provenance storage.

Do not simply join on existing `source_statement_syntax_id`: multiple nested
contacts may share one statement. Do not mark a missing contact row as an
ordinary no-contact success. Contact-required admission must be owned before
the lookup; otherwise deleting an RIR row still silently hides a machine call.

Positive gates to reuse: native machine pipeline, MIR machine projection
probe, and installed immutable-manifest delivery. Add missing exact-contact
ID, duplicate contact ID, crossed statement/expression ID, and two contacts in
one statement negatives before permitting the replacement. The installed
manifest delivery gate owns companion bytes and no-native-retry behavior, not
machine-operation execution by itself.

Effort: medium, cross-stage provenance handoff. Remaining full-row blocker:
native physical companion/provider provenance and the existing broader machine
contract. This is a genuine old-read deletion but not a full-row CLOSED card.

## 2. Target profile: existing physical owner does not contain missing facts

Owner: `target.capability_profile` /
`target_capability_owner.pgy#CompilerTargetCapabilityEnvelopeReady`.
Last consumers are native `verified_projection_plan.c`,
`target_projection_fact_owner.pgy`, `direct_mir_cfg_plan_owner.pgy`, and
`codegen/emission/program_entry_owner.pgy`.

Observed existing facts and their limits:

- Native and Pergyra target envelopes own projection/fact/fallback vocabulary
  and different-width fingerprints. They do not contain pointer size/alignment,
  endian, object format, or a complete physical data-layout profile.
- `SelfHostMachineLayerDeclaration` and `PgyMachineLayerPhysicalManifest`
  carry target-kind, board, boot/linker contract, address limit and device grant
  base/size/mode. Address/grant authority is not object/data-layout authority.
- `CompilerAbiLayoutTargetPolicyRow` carries vocabulary policy, not concrete
  physical target facts.
- `DirectMirArrayStorageLayoutContract` owns fixed 32-byte/8-aligned,
  pointer64/size_t64 Array layout. It is not derived from a carried physical
  target profile. The ABI manifest still exposes `target-c-default` in
  `abi_layout_row_owner.pgy#CompilerAbiLayoutTargetCDefaultSizeAlign`.
- `lib/SnapshotTicket` and `BinaryProjectionPreflight` validate caller-supplied
  endian at a snapshot boundary. They are not the compiler target profile and
  are not imported by the production target capability owners.
- `CompilerEmissionArtifact` in `driver_pipeline_owner.pgy` has only kind,
  payload, target schema/projection/fingerprint. No AIR anchor, physical target
  profile identity, plan revision, or plan digest is carried to publication.

A full-row card therefore requires a real typed physical-profile producer,
its native/Pergyra derived consumers, exact physical-profile binding in MIR/ABI
and AIR/plan admission, and emission-artifact identity propagation through all
constructors/publication consumers. It cannot be obtained by deleting one
unused field or accepting only a vocabulary envelope. The fixed Array receipt
is useful input but does not prove endian/object format or all profile rows.

Positive gates exist for native envelope/planner ownership, self-host envelope
fingerprint, and direct CFG plan target binding. Full closure needs explicit
missing physical field/profile, profile-layout mismatch, crossed AIR/plan,
erased emission plan digest, and wrong object-format negatives at installed
C/LLVM publication. Existing tests do not establish those claims.

Effort: large, new required facts plus all emission artifact carriers. This
is not a small full-row candidate for the current parallel projection batch.

## 3. AIR boundary identity/lifetime: coherent with HIR/RIR, not current rung

Owner: `air.evidence_graph` / `air.c#air_synthesize`. Last consumer is AIR
verification and its certificate consumed by the verified projection planner.

Exact remaining old reads:

- `air_evidence_hir.c#air_hir_routine_matches_boundary` joins routine/owner/step
  and boundary identity by names.
- `air_evidence_hir.c#air_hir_cfg_contains_boundary_ast` walks statement and
  terminator AST descendants via `air_ast_contains_node`.
- `air_evidence_rir_match.c#air_rir_scope_matches_boundary` joins by names;
  `air_rir_op_matches_boundary_ast` uses pointer equality/AST containment and
  treats absent boundary AST as matching.
- `air_evidence_rir_boundary.c` repeats AST containment for authority and
  capture evidence. `air_evidence_ast.c` implements the broad descendant scan.
- `rir_builder.c#rir_collect_func_scope` still scans `ast_func_body` using
  `rir_walk_node`; zone/world/resource shape collection still reads AST.

An owned boundary identity/containment anchor produced once from admitted
HIR/DIR/RIR facts can retire the AIR matching scans. The existing RIR statement
ID and HIR routine ID are prerequisites, not a complete nested-boundary map.
Capture the exact enclosing routine, boundary and occurrence identities;
missing/crossed anchors refuse. Copied evidence lifetime and MIR rebind guards
must remain under the AIR owner rather than a second evidence authority.

Reuse `air_mir_binding_smoke.sh`, `hir_routine_identity_smoke.sh`, and
`rir_resource_flow_identity_smoke.sh`; add same-name different-routine and
crossed nested-boundary identity negatives. They are separate native-stage
proofs until an installed Pergyra producer actually replaces the reached path.
This batch does not presently provide that replacement.

Effort: large. It can form one coherent HIR/RIR/AIR anchor migration later, but
initial typed domain/control-flow and resource collection remain open even
after AIR matching changes. Three CLOSED promotions are not justified.

## Remaining row disposition

| Row | Current exact remaining boundary | Full-row batch disposition |
|---|---|---|
| `hir.typed_control_flow` | typed domain payload and AST-backed CFG input; AIR matching still rescans descendants | retain BRIDGE |
| `dir.domain_graph` | materialization/dirty/epoch/detach/unlink/state scheduling, authority/action and world/intent production; native receiver policy | retain BRIDGE |
| `rir.resource_transition_graph` | `rir_builder.c`/`rir_builder_walk.c` initial AST resource/shape collection; no complete self-host RIR consumer | retain BRIDGE |
| `mir.execution_graph` | general multi-loop/multi-phi/foreach and broader native/backend compatibility carriers | retain BRIDGE |
| `mir.generic_specialization` | native `mir_generic_method_specialization.c#mir_generic_method_capture_node` remains source capture oracle; broader generic/member witness/copy-return/identity convergence | retain BRIDGE |
| `abi.layout_rows` | fixed/nominal admitted receipts exist, but target-dependent/pointer-bearing/general wrappers and profile interoperability do not | retain BRIDGE |
| `diagnostic.catalog` | `driver_diag.c#driver_diag_code_from_message` uses substring recovery; `driver_emit_stage_fail` and `driver_app.c` still call it across module/HIR/DIR/RIR/AIR/native backend failures | retain BRIDGE |

The generic source capture and ABI table are declared current owners, so an
AST read inside those producers is not automatically a forbidden backend
fallback. This review does not claim an exhaustive function-level old-read
inventory for their 44 and 62 registered consumer paths. That larger closure
needs owner/family admission coverage, not only a grep matching `ast_`.
