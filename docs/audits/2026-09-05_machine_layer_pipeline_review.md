# Machine-layer pipeline assessment reconciliation

Date: 2026-09-05 (Asia/Seoul)

Observed base: `f34355b37dbd9e86ef574399e895a78fd41dd0a3`, with the shared
working tree dirty. This report inspects the current source, not merely that
commit. It is a bounded documentation review under
`docs/agent_work_directives/machine_layer_documentation_review_2026-09-05.md`.
No compiler, runtime, proof, test, registry or shared navigation file was edited
by this lane. No build, executable gate or proof was run.

The supplied assessment is directionally sound: address evidence and permission
to perform a machine operation must remain distinct. Its implementation diagram
needs qualification. Native and self-host paths carry real machine facts, but
this is neither an AIR-to-backend lowering pipeline nor a completed physical
hardware implementation.

## Claim-by-claim findings

The anchors below are repository-relative `file:line` source locations at the
observed dirty tree. CONFIRMED means source inspection, not newly executed
evidence. CORRECTED preserves the supported claim while narrowing its scope.

### RIR contacts — CONFIRMED for the native producer

`RIRMachineContactKind` explicitly declares CLAIM, READ, WRITE, RELEASE and
SUBMIT_READ (`src/compiler/rir.h:116`). The native walker maps `DeviceRead`,
`DeviceWrite`, `ReleaseDeviceSlot` and `SubmitDeviceRead` to those contact facts
at `src/compiler/rir_builder_walk.c:82`; `ClaimDeviceSlot` is recognized at a
let initializer at line 217. SubmitRead is carried by `RIR_OP_AWAIT_REMOTE`;
the contact identity is distinct from that general resource-operation kind.

Recommended wording: **The native RIR producer classifies the five supported
DeviceSlot operations and carries the classification into MIR.** Do not imply
that every current self-host source route first builds this native RIR object.

### Explicit MIR facts — CONFIRMED

`src/compiler/mir_types.h:206` owns the instruction fields. The native attachment
at `src/compiler/mir_machine_layer.c:8` copies the RIR contact and resolves its
abstract operation and selected physical grant from the manifest owner. The
validator at line 92 compares identity, base, size, mode, runtime operation and
the authority/live-lease requirements against that owner.

Recommended wording: **A machine-contact MIR instruction must carry a valid
manifest-bound machine fact; backend admission cannot replace a missing fact
with a guess from the call spelling.** This is a requirement on machine
contacts, not a requirement that every ordinary MIR instruction have a
non-null machine object.

### AIR machine sites — CORRECTED: validation beside execution lowering

`src/compiler/air_evidence_mir_facts.c:318` collects machine sites from MIR
resource instructions and rejects invalid MIR machine facts before copying
their fields. `src/compiler/air_validate_machine_layer.c:9` then verifies site
completeness and calls the manifest's site validator.

The site's `authority_required` and `live_lease_required` fields are requirement
facts. The site validator checks their presence and agreement with the manifest;
that function alone is not a proof that every concrete authority or resource
lifetime obligation is satisfied. Resource-state analysis has its own owner,
for example `src/compiler/rir.c:265`. Do not equate a true requirement bit with
an independently established capability or live lease.

The native driver synthesizes AIR from HIR/DIR/RIR at
`src/compiler/driver_app.c:419`, lowers MIR from its explicit lower request at
line 441, and adds MIR evidence to AIR at line 471. MIR is not lowered from AIR.
`CompilerIRBundle` carries no AIR graph (`src/compiler/compiler.h:54`). Compiler
orchestration obtains verified plan rows; backend code consumes those rows and
MIR, not `AIRProgram` (`src/compiler/compiler.c:50`,
`src/compiler/compiler_llvm.c:27`).

Recommended wording: **AIR validates evidence alongside the execution IR path.
Its certificate admits specific derived projection artifacts; AIR is not a
backend execution IR or an input that is lowered into CPU MIR.**

### VerifiedProjectionPlan — CONFIRMED artifact, CORRECTED universality

`PgyVerifiedProjectionPlanRow` really contains the advertised target, disposition,
runtime profile, certificate/fingerprint, selected physical window and provider
requirement fields (`src/compiler/verified_projection_plan.h:34`). Production
construction validates the certificate and target/manifest readiness before
copying those facts (`src/compiler/verified_projection_plan.c:184`).

The exact `PgyProjectionAxis` enum currently has only Intent observability
(`src/compiler/verified_projection_plan.h:17`). Parallel capture and spawn lane
are separate plan families in the same header at lines 67 and 129. This is not
one fully generalized per-operation machine-contact plan, and a native plan
structure does not establish the same completed artifact contract for every
self-host route. The host compile/link boundary explicitly does not fabricate
a native plan identity for an admitted self-host C artifact
(`src/compiler/compiler.h:74`).

Recommended wording: **The native compiler has a certificate-bound Intent
observability plan that also carries machine-manifest/window admission facts,
plus separate spawn-lane and parallel-capture artifacts. This is concrete
partial implementation of the projection boundary, not whole-backend closure.**

### C/LLVM consumption — CONFIRMED fail-closed gate, CORRECTED no-name claim

The cited C guard is real: `slot_builtin_require_machine_fact` finds the
source-stable-ID instruction, requires a bound projection, and validates its
contact kind and machine fact (`src/codegen/transpiler_slot_builtin_emit.c:111`).
LLVM has the equivalent boundary
(`src/codegen/llvm_expr_slot_device_calls.c:105`), with a separate claim-let
check (`src/codegen/llvm_stmt_let_slots.c:143`).

However, LLVM still has a call-name dispatch table at
`src/codegen/llvm_expr_slot_device_calls.c:46` and selects it at line 184 before
requiring the corresponding MIR fact. The self-host semantic C emitter also
dispatches from a carried call-target name
(`src/self_hosted/codegen/emission/expr_semantic_machine_call_emit_owner.pgy:11`)
through an owned runtime ABI mapping. That is not a raw-source re-scan, but it
does disprove the stronger claim that all name-based operation selection has
already vanished from every backend path.

Recommended wording: **Supported native machine emitters require the lowered
MIR fact and admitted projection; call-name dispatch still exists and must not
be mistaken for complete elimination of backend reconstruction.**

### Abstract/physical manifests and self-host carriage — CONFIRMED, bounded

The five abstract operation rows and three projection descriptions are present
at `src/compiler/machine_layer_manifest.c:17` and line 26. The default physical
declaration is explicitly host simulation: volatile grant `device-slot0`, base
`0x10000000`, size `0x1000` (`src/compiler/machine_layer_manifest.c:41`). Its
identity is `pergyra.machine-declaration.host-sim.v1`
(`src/compiler/machine_layer_manifest.h:15`). Projection names describe distinct
contracts, not proof of a deployed MMIO/address-space implementation.

The native serializer remains the physical declaration source. Public
`--machine-manifest-json` dispatches to verified replay of the installed
self-host companion (`src/pgy_driver.c:263`,
`src/compiler/self_host_machine_manifest_artifact_owner.c:12`). Missing binary
or companion is rejected. The Pergyra consumer parses and validates that
artifact (`src/self_hosted/compiler/machine_layer_declaration_consumer.pgy:189`,
line 220); it does not discover hardware.

Self-source MIR classification uses carried semantic expression-call facts,
not the native RIR object
(`src/self_hosted/mir/machine_layer_projection_owner.pgy:67`). Its contract check
compares the declaration's operation rows with the Pergyra ABI projection
(line 44). That projection contains explicit five-operation mappings
(`src/self_hosted/compiler/machine_layer_runtime_projection_owner.pgy:3`), so
describe it as a checked projection, not as evidence that all duplicated
representation has physically disappeared.

The self-host MIR consumer validates contact/object consistency and declaration
agreement (`src/self_hosted/mir_lower/machine_layer_fact_owner.pgy:143`, line 185).
An absent/null machine object is allowed for a non-contact instruction; a
machine contact with a missing or inconsistent object is rejected. This is the
precise supported version of the attachment's missing-row assertion.

### Runtime provider — CONFIRMED seam, CORRECTED hardware conclusion

The C provider callback accepts base, size, mode and context and returns an
integer acceptance result (`src/runtime/pgy_runtime_machine_layer_inline.h:14`).
The mapping bind refuses a required but missing provider, a rejected mapping,
or a conflicting already-bound window (line 81). The LLVM runtime twin has the
same boundary (`src/runtime/pgy_runtime_lib_machine_layer_exports.h:59`).
Current device mapping accepts volatile mode only; the broader formal
plain/atomic operation families are not all implemented by this runtime bind.

Native provider-required selection is presently based on the physical manifest
identity differing from the built-in host-sim identity
(`src/compiler/verified_projection_plan.c:267`), not a measurement of the target.
The self-host C startup serializer applies the same distinction
(`src/self_hosted/compiler/machine_layer_runtime_binding_owner.pgy:13`). C and
LLVM startup emit the carried window and fail on refusal
(`src/codegen/transpiler.c:162`, `src/codegen/llvm_main_wrapper.c:71`).

Crucially, provider acceptance does not install a device-operation implementation
or turn the bound integer base into MMIO storage. The shipped DeviceSlot runtime
contains `value` and `claimed`; its read/write routines access that value
(`src/runtime/pgy_runtime_slot_macros.h:24`,
`src/runtime/pgy_runtime_lib_device_slot_exports.h:8`). Therefore **actual board
mapping and actual hardware contact remain UNVERIFIED by this review**. A board
provider, target operation implementation and refinement evidence are separate
obligations, not consequences of returning 1 from the callback.

Recommended wording: **The runtime seam requires an embedder-owned mapping
acceptance for non-host declarations. The shipped operations are host simulation;
the callback is an admission boundary, not hardware measurement or MMIO support.**

## Corrected current-path sketch

```text
Native HIR / DIR / RIR owner facts -----> MIR / ABI execution facts ------+
             |                                |                         |
             +-------------> AIR <------------+                         |
                              | certificate                             |
Native manifest / target -----+----> specific verified plan artifacts ---+
                                                                        |
                                                   C / LLVM emitters <--+
                                                         |
                                             runtime identity/window bind
                                                         |
                                                host-sim DeviceSlot ops

Self-source semantic call facts + installed manifest companion
        -> Pergyra MIR machine fact / checked admission
        -> reached self-host projection / runtime binding consumers
```

This sketch is a relationship map, not a promise that every fact travels through
every displayed path or that all self-host projections share the native plan
type. Future non-CPU projection and board/MMU refinement are contracts outside
these observed implementation arrows. The machine-neutral contract distinguishes
that intent from current C/LLVM production scope
(`docs/semantics/18_machine_neutral_compute.md:78`, line 112).

## Suggested verification, not executed in this lane

- `tests/border_registry_smoke.sh`: narrow AIR/backend include boundary.
- `tests/machine_layer_manifest_smoke.sh`: existing manifest/runtime probe and
  structural owner checks; requires its probe binary and is not just a lint.
- `tests/machine_layer_pipeline_smoke.sh`: native RIR/MIR/AIR and C/LLVM machine
  pipeline. Inspect any reported skip; its old introductory self-host limitation
  is not evidence against the newer installed-source gate below.
- `tests/self_hosted/parity/public_machine_manifest_installed_self_host_owner.sh`:
  installed companion replay, native oracle equality, missing/corrupt refusals.
- `tests/self_hosted/parity/public_device_slot_machine_manifest_installed_self_host_owner.sh`:
  reached public source-C, exact host-sim output and missing/corrupt companion
  no-native-retry cases. It does not prove all source-LLVM or board targets.
- `tests/self_hosted/mir_machine_layer_smoke.sh`: broader existing producer and
  checked-consumer parity/negative integration if the primary needs that scope.

The RIR/AIR parity wrappers delegate to the same last gate, rather than providing
two independent producer implementations. Do not multiply these wrapper names
into progress evidence. None of these gates was run by this reviewer; this
documentation review changes neither SoT status nor self-host replacement count.
