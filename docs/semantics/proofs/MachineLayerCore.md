# Machine Layer Core: the layer that reaches the machine

This document separates two things that a raw pointer incorrectly conflates:

1. **Address evidence**: where a region is, how large it is, which declaration
   owns it, and which access class applies (`Grant` and `Region`).
2. **Machine layer state**: the operation that reaches that region, updates the
   abstract machine state, and produces an observable machine event
   (`contact_step`).

The proof is in [`MachineLayerCore.v`](MachineLayerCore.v). It is a small
constructive core. The formal model is intentionally conditional on an explicit
hardware-declaration adequacy witness; the live compiler bridge below currently
targets the abstract `DeviceSlot` runtime contract, not physical MMIO or a
silicon-specific memory model.

The current bridge does claim the abstract compiler/codegen path from Pergyra
source through RIR/MIR/AIR to C, LLVM, and self-hosted contact projections. It
does not yet claim refinement to live board/MMU behavior: the default host-sim
declaration is not live board/MMU evidence. Non-host target declarations carry a
required runtime-provider bit through the verified plan; generated
C/LLVM/self-host startup fails closed
until an embedder binds the board/MMU provider.

## The design boundary

`Region` is not a pointer and it is not itself a machine instruction. It is an
address-evidence record:

```text
Grant       { id, base, size, mode }       declared machine range
    |
    +-- region_of_grant
    v
Region      { base, extent, mode, prov }   address evidence
    |
    +-- carve
    v
Region      sub-range with the same grant root
```

The plain-data bridge is deliberately one-way:

```text
place : Region -> TypeLayout -> option Slot
```

`TypeLayout` carries nominal type identity, ABI size, and ABI alignment.
`place` proves that a plain typed cell fits a valid region and preserves that
identity in the resulting `Slot`. It is an upper-layer bridge to `Slot`, not
the operation that touches a device or memory bus.

Actual contact is explicit:

```text
ContactOp = Read | Write | VolatileRead | VolatileWrite | AtomicRmw | Fence

contact_step declaration op config region config'
```

A contact step is derivable only when all of these facts are supplied:

- the region is inside a declared grant;
- the declaration has an explicit hardware-adequacy predicate;
- the current configuration holds authority for that grant;
- the grant lease is live, not revoked;
- the operation matches the region's access mode.

The transition applies the operation to the abstract cell-valued memory state:
reads record the pre-step value, writes update the addressed cell, and atomic
read-modify-write records the old value before applying the new one. It also
appends a `ContactEvent` containing the operation, target base, extent, mode,
grant provenance, and operation value/observation to the trace. Concrete loads,
stores, device side effects, cache/TLB behavior, DMA, and ordering are backend
refinement obligations; they are not silently implied by a range proof.

## What is proved

All theorems are constructive. The file adds no Coq axiom and no `Admitted`.

| Theorem | Contract |
| --- | --- |
| `grant_yields_valid_region` | A declared grant yields a region inside that grant. |
| `carve_preserves_validity` | A successful sub-range carve preserves grant grounding. |
| `carve_disjoint` | Non-overlapping carve offsets produce disjoint ranges. |
| `place_grounds_slot` | A plain `TypeLayout` placement preserves provenance and bounds. |
| `place_preserves_layout_identity` | A successful placement preserves nominal type identity from the layout record. |
| `chain_grant_carve_place_grounded` | The grant -> carve -> plain-slot bridge stays grounded. |
| `place_rejects_volatile` / `place_rejects_atomic` | Plain `Slot` placement cannot cross MMIO/atomic modes. |
| `declared_grant_id_unique` | A `MachineDeclaration` cannot contain two distinct grants with the same id. |
| `declared_grants_nonoverlap` | A declaration carries non-overlap evidence for distinct grants. |
| `declared_grant_address_bounded` / `valid_region_address_bounded` | Declared and valid ranges stay below the target address-space bound. |
| `declared_grant_hardware_adequate` | Hardware adequacy is an explicit declaration field, not an implicit axiom. |
| `valid_region_has_declared_hardware_adequacy` | Valid regions inherit adequacy from the declaration boundary. |
| `contact_step_requires_valid_region` | Every contact is rooted in a declared region. |
| `contact_step_requires_hardware_adequacy` | Every contact carries a hardware-adequacy witness. |
| `contact_step_requires_capability` | Contact requires grant-specific authority evidence. |
| `contact_step_requires_live_lease` | Contact requires a live lifetime lease. |
| `contact_step_requires_mode` | Contact operation and access mode must agree. |
| `contact_step_constructible` | The contact constructor has a positive path when every required witness is present. |
| `sample_plain_read_contact` | A concrete one-grant declaration exercises the positive contact path. |
| `volatile_contact_requires_volatile` / `atomic_contact_requires_atomic` | MMIO and atomic operations cannot silently run against another mode. |
| `memory_write` / `contact_apply` | The machine layer owns the memory-state transition for each operation family. |
| `contact_event_for` | Event payload is derived from the pre-step state and operation, not a backend-local guess. |
| `contact_step_emits_event` | Every contact appends an observable operation event. |
| `contact_step_reads_current_value` | A plain read records the addressed pre-step value. |
| `contact_step_volatile_reads_current_value` | A volatile read records the addressed pre-step value. |
| `contact_step_writes_value` | A plain write changes the addressed cell to the operation value. |
| `contact_step_volatile_writes_value` | A volatile write changes the addressed cell to the operation value. |
| `contact_step_atomic_rmw_reads_before_write` | Atomic read-modify-write exposes the old value while installing the new value. |
| `contact_step_atomic_rmw_writes_value` | Atomic read-modify-write installs its new value at the addressed cell. |
| `contact_step_fence_preserves_memory` | A fence records the ordering event without changing the abstract memory map. |
| `contact_step_preserves_authority` / `contact_step_preserves_lease` | Contact does not silently create authority or extend lifetime. |
| `cap_gate_fail_closed` | No authority means no contact transition. |
| `revoked_lease_fail_closed` | A revoked lease cannot be used for contact. |
| `contact_mode_fail_closed` | A mode-mismatched operation is not derivable. |
| `sample_plain_region_rejects_volatile_read` / `sample_revoked_region_rejects_read` | Concrete negative witnesses exercise mode and revocation rejection. |

The old `no_wild_slot` theorem is retained only as a precise projection from
the `slot_grounded` predicate. It is not advertised as an API-level
unforgeability theorem; the actual no-ambient-contact property is
`no_ambient_machine_contact`.

## Declaration versus hardware

`MachineDeclaration` contains:

- the declared grant list;
- a predicate supplied by the target/boot boundary describing hardware
  adequacy;
- unique grant-id evidence;
- non-overlap evidence;
- an explicit address-space upper bound for every declared range;
- adequacy for every declared grant.

This is the honest boundary. A Rocq proof over a parameterized declaration does
not establish that a bootloader, MMU, linker script, or device actually agrees
with that declaration. That requires a target-specific manifest/refinement
proof and a compiler/runtime gate.

## Implementation status

The proof-layer machine state and transition are implemented, and the abstract
machine-facing compiler bridge is now live for the existing `DeviceSlot<T>`
surface:

- semantic `ClaimDeviceSlot`, `DeviceRead`, `DeviceWrite`,
  `ReleaseDeviceSlot`, and `SubmitDeviceRead` calls are classified as
  `RIRMachineContactKind` facts;
- `--rir-json` exposes those owner-declared rows as the stable
  `pgy.rir.v1` artifact through `rir_dump_json`; its stdout is a clean JSON
  boundary, so downstream consumers do not need to rescan source spellings or
  parse the human semantic summary;
- MIR carries the fact with `pergyra.abstract-device-slot.v1`, an explicit
  contact-to-runtime-operation mapping (`Claim`, `Read`, `Write`, `Release`,
  `SubmitRead`), the declared physical grant identity (`device-slot0`), and
  hardware-adequacy, authority, and live-lease requirements;
- MIR validation receives a typed `machine_layer_fact_required` bit from the
  RIR contact owner. It never classifies a machine boundary by scanning AST or
  source call names; once RIR declares a contact, a missing owner row therefore
  fails closed at the owner seam rather than being recovered from syntax;
- the same MIR row carries the owner-declared physical shape (`base`, `size`,
  and access mode), and AIR copies and revalidates those values instead of
  reconstructing them from the grant name or a backend default;
- AIR publishes validated `machine_layer_sites` for the lowering operations;
- each MIR/AIR machine site carries the physical grant identity, and the native
  manifest owner rejects a site whose grant is absent, unknown, non-adequate,
  or not the declared volatile device window;
- AIR site validation delegates contact/runtime identity and authority/lease
  requirements back to the machine-manifest owner; it does not rescan the
  operation table or invent a second site contract;
- MIR JSON carries an explicit `machine_contact_kind` beside each instruction's
  optional `machine_layer` object, so a self-host consumer can reject a missing
  owner row without recovering contact identity from expression text;
- C and LLVM admission checks consume that MIR owner fact and fail closed when
  it is missing or invalid;
- the verified projection planner binds a stable fingerprint of the abstract
  machine manifest into the plan row, so backend projections cannot silently
  switch operation or hardware-adequacy policy;
- generated C and LLVM executable wrappers pass both verified-plan fingerprints
  plus the selected grant's declared `base`/`size`/`mode` to
  `pgy_machine_layer_runtime_bind_mapping_export` at process start, including
  the provider-required bit, and fail closed before user code if the runtime
  rejects the identity, window shape, or missing external provider;
  the C inline twin and LLVM runtime export keep their implementation lanes
  separate, so the runtime receives owner values without re-reading or
  reconstructing the declaration;
- `tests/machine_layer_pipeline_smoke.sh` exercises the RIR JSON/MIR/AIR path
  plus C/LLVM lowering for the direct and submitted device-read paths.
- the self-hosted machine-layer runtime projection owner exports only the five
  abstract contact rows; `machine_layer_declaration_consumer.pgy` is the sole
  self-hosted consumer of native physical declaration JSON, so grant literals
  are not duplicated in `.pgy` source. Its current beta envelope admits the
  checked board/boot/linker provenance namespace and fails closed for a
  declaration outside `pergyra.machine-declaration.*`.
- self-hosted `mir_lower` consumes the `machine_layer` rows in MIR JSON through
  `machine_layer_fact_owner.pgy`, checks manifest/runtime-operation identity
  and physical `base`/`size`/`mode` against the passed declaration artifact, and
  rejects a mutated row before AST reconstruction
  (`tests/self_hosted/mir_machine_layer_smoke.sh`).
- the self-hosted AIR validator consumes `machine_layer_sites` through the
  declaration consumer, checks the physical shape fields, and rejects mutated
  AIR site rows in that gate; it is an AIR consumer, not a second machine
  manifest.
- the self-hosted MIR producer now projects the semantic expression-graph
  call-target row into the same explicit machine fields and JSON shape; an
  executable probe covers all five contacts and the self-host `mir_lower`
  consumer reads that produced artifact back without source recovery.
- the self-hosted source-to-MIR driver is also wired through that declaration
  consumer for both direct `DeviceRead` and remote `SubmitDeviceRead` fixtures;
  the smoke requires their contact rows and rejects a mismatched physical
  device-grant identity before lowering.
- the self-hosted C emitter now consumes the same typed machine runtime rows
  for synchronous `DeviceSlot<T>` and the `RemoteFuture<Int>` read path: ABI
  layout selects `PgyDeviceSlot_<T>`/`PgyTaskHandle`, semantic graph calls
  select owner runtime symbols, and the await leaf plus Try graph lower through
  the owner-provided `Result<Int>` bridge. The machine-layer smoke emits,
  syntax-checks, and executes both fixtures; other `RemoteFuture<T>` payloads
  remain fail-closed until their result ABI rows are frozen.
- `machine_layer_rir_validator.pgy` consumes the native `pgy.rir.v1` JSON
  artifact before MIR lowering and checks every non-`none` contact against the
  same self-host projection owner; the smoke mutates one contact and requires
  rejection.
- the native machine manifest now carries a checked physical target declaration
  (`pergyra.machine-declaration.host-sim.v1`) with an address limit, unique
  non-overlapping grant table, explicit `device-slot0` volatile window, and a
  board/boot/linker provenance envelope plus a stable physical-declaration
  fingerprint; the selected device grant must also
  be adequate and volatile at backend-plan admission, so a plain/unknown
  declaration cannot survive until code generation;
- native embedders can bind one immutable `PgyMachineLayerPhysicalManifest`
  through `pgy_machine_layer_physical_manifest_bind` before the driver starts.
  The driver consumes that provider through `DriverFlags` and the planner then
  reads the same active declaration; no JSON reparse or host-sim fallback is
  introduced after a provider has been accepted. Physical declaration IDs are
  restricted to the `pergyra.machine-declaration.*` namespace. The self-hosted
  consumer admits the native declaration namespace and requires nonempty
  target-specific provenance; the host-sim tuple remains the default fixture,
  not a self-hosted hardcoded target.
- the verified projection plan binds both the abstract contact-manifest
  fingerprint and the physical-declaration fingerprint. C and LLVM admission
  therefore cannot lower a `DeviceSlot<T>` fact unless the same declared
  target window is present; self-host consumes the matching declaration JSON
  through the declaration consumer. The fingerprints certify the declaration as a
  whole, while MIR/AIR JSON exposes its exact base, size, and mode for the
  final owner-directed checks.
- the runtime startup mapping guard now consumes the planner's selected grant
  shape as a single declaration-level mapping record; it does not change the
  `DeviceSlot<T>` ABI layout. Host-sim keeps the provider optional, while every
  non-host declaration sets the provider-required bit and rejects startup until
  the embedder-owned board/MMU callback is bound.
- the self-hosted C emitter now reaches that same startup consumer: its sole
  native declaration view serializes the verified manifest/physical
  fingerprints and grant window into `Main` before the first machine contact;
  missing or malformed declaration facts fail closed instead of falling back
  to a self-host literal.
- the hosted runtime exposes an explicit mapping-provider callback seam. When
  an embedder binds a board/MMU provider, every mapping bind is checked against
  that provider and mismatched windows fail closed; non-host declarations also
  fail closed when the provider is absent, while the default host-sim path
  intentionally remains declaration-only.

The checked host-sim declaration is a compiler target record, not evidence that
a particular board, MMU, linker script, or device agrees with it. User-facing
`Region`/`Grant` syntax is not beta-stable, and the compiler/runtime still does
not claim volatile ordering, atomic ordering, or device side effects from this
proof alone. The remaining refinement is the target integration that supplies
the declaration from a real boot/link/device boundary without bypassing the
owner fact; the current host-sim window is deliberately explicit and
fail-closed rather than silently treating an abstract handle as a physical
address.

### Backend representation and self-hosting

The machine layer has one semantic contract and multiple physical projections.
The abstract manifest records these projection rows explicitly:

| Projection | Physical representation | Lowering contract |
| --- | --- | --- |
| `cpu-c` | `c-abi-runtime-handle` | C ABI/runtime call over an address-like `DeviceSlot` handle |
| `cpu-llvm` | `llvm-ssa-address-space` | LLVM SSA value with target address-space and calling-convention metadata; `DeviceSlot<T>` is emitted as a distinct named `%PgyDeviceSlot_<T>` aggregate, never the ordinary `%PgySlot_<T>` type |
| `self-hosted` | `owner-fact-artifact` | self-hosted owner-fact artifact consumed by a backend projection |

C may therefore represent a contact as an ABI/runtime call over an
address-like handle; LLVM may represent the same contact with target address
spaces, SSA values, and calling-convention metadata. LLVM keeps the physical
boundary inspectable by giving `DeviceSlot<T>` its own named aggregate and
runtime declaration types (`%PgyDeviceSlot_<T>`), even when the current field
layout matches `Slot<T>`. Those representations are expected to differ, but
the `claim/read/write/release/submit-read` operation,
authority/lease requirements, machine manifest, and fail-closed validation
must be identical. The C and LLVM lowering paths call the manifest projection
gate before they consume a machine contact fact;
`machine-layer-manifest-test-smoke` checks that the rows remain distinct and
that unknown or inadequate projections fail closed. A self-hosted compiler is
another consumer that must emit the same owner facts and diagnostics; it is not
the only layer allowed to observe or lower machine contact. Self-hosting is
complete only when its projection reaches the same verified MIR/AIR/ABI
contract, not when it invents a separate machine meaning.

## Non-goals and remaining obligations

This core does not yet prove:

- full pointer provenance under arbitrary aliasing or type-punning;
- C11/Rust atomic ordering, fences, or compiler reordering constraints;
- MMIO device side effects, interrupts, DMA coherence, or cache/TLB behavior;
- physical versus virtual address translation or machine-width overflow;
- runtime revocation/unmapping implementation;
- a concrete board/MMIO provider implementation supplied by a target embedder;
  the compiler/runtime contract now rejects a non-host declaration without
  that provider, but this repository cannot invent a board's MMU truth;
- physical compiler/codegen refinement from the abstract contact transition to
  a board/MMIO/device declaration agreed with a live target; the current
  source -> RIR -> MIR -> AIR -> C/LLVM/self-host bridge is live for the
  abstract `DeviceSlot<T>` contract plus a checked declaration, and the
  provider-required bit is carried to startup; an embedder still supplies the
  board/linker/bootloader truth source.

These are named refinement obligations, not hidden safety claims. In
particular, this file must not be summarized as Rust-equivalent memory safety
or as a proof that real hardware follows a declaration.

## Research lineage

These references motivate individual obligations; none is treated as a proof
of Pergyra's implementation:

- Tofte and Talpin, *Region-Based Memory Management*, Information and
  Computation 132(2), 1997 — scoped region ownership and carving.
- Memarian et al., *Exploring C Semantics and Pointer Provenance*, POPL 2019 —
  why address values alone are insufficient for provenance claims.
- Jung et al., *RustBelt: Securing the Foundations of the Rust Programming
  Language*, POPL 2017 — the gap between a safe core model and compiler/runtime
  adequacy.
- Jung et al., *Tree Borrows: An Aliasing Model for Rust*, POPL 2021 — the
  remaining aliasing/provenance refinement obligation.
- Watson et al., *CHERI: A Hybrid Capability-System Architecture for Scalable
  Software Compartmentalization*, IEEE S&P 2015 — capability authority as a
  machine-facing boundary rather than an integer flag.
- ISO/IEC 9899:2011, C11 memory model — the external reference point for
  volatile and atomic ordering obligations that this core intentionally does
  not yet encode.

## See also

- [`08_slot_capability_calculus.md`](../08_slot_capability_calculus.md) — the
  typed slot layer above this core.
- [`04_ownership_abi.md`](../04_ownership_abi.md) — ownership and layout facts.
- [`15_capability_sandbox.md`](../15_capability_sandbox.md) — capability-as-effect
  direction for the compiler.
- [`19_theoretical_foundations.md`](../19_theoretical_foundations.md) — lineage
  and proof-scope discipline.
