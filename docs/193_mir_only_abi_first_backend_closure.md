# MIR-only·ABI-first backend closure

## What “MIR-only” means here

Pergyra does not have a second backend pipeline that jumps from syntax to C or
LLVM. The production path is:

```text
AST → Semantic → HIR/DIR/RIR → AIR synthesis + verification
    → MIR lowering + validation → AIR MIR evidence + re-verification
    → AIR-bound Verified Projection Plan → C/LLVM projection
```

`Only` applies to the last projection boundary. It means that a backend which
has crossed AIR and the verified projection plan may consume MIR-owned facts,
ABI rows, and target facts only; it must not reconstruct semantic decisions by
re-reading AST type or declaration payloads.

## Why closure had not happened

The entrypoints were already named `transpile_mir_only` and
`llvm_codegen_from_mir`, but those names described admission, not complete
consumer migration:

1. `MIRProgram` still carried AST declaration arrays for the top-level C
   orchestration pass. `emit_program` selected those arrays and passed AST
   declarations to nominal/domain emitters.
2. Several routine consumers had MIR signature fields but still used the AST
   type node when a type-name fact was absent. A MIR body could therefore be
   admitted while its prototype or parameter declarator reopened AST recovery.
3. Runtime-call ABI rows still had a constructed-resource spelling path for
   dynamic nominal types. Before the current rung, MIR carried the spelling
   and call shape but not a stable `RuntimeCallAbiId`, so row identity was
   still a materialization seam.
4. The protocol registry still marks runtime-call compatibility and the
   lowering API as bridge/open. The last consumers are wider than the bounded
   parity fixtures, so the SoT cannot be promoted by documentation alone.

AIR is not the missing step. Production `compiler_emit_c` and LLVM entrypoints
obtain the projection plan through
`pgy_verified_projection_plan_intent_observability_with_air` and obtain the
spawn execution-lane rows through `pgy_verified_spawn_lane_plan_from_air`.
The backend entrypoints reject a missing or unverified plan, so a direct
MIR-only probe remains intentionally available only as a failing/unit-test
surface, not as a production admission proof.
They also validate the plan identity digest and require the AIR certificate
fingerprint before emitting either C or LLVM.

## First executable closure rung

The first rung closes routine signature admission after AIR/MIR:

- owner: `MIRRoutine.has_signature`, `param_type_names`,
  `MIRCallableSig`, and `return_type_name`;
- last consumers: C function body emission, forward declarations, and MIR
  emission contract validation;
- forbidden fallback: `transpiler_mir_ast_type_supported` on an active MIR
  routine;
- missing facts fail closed with a MIR inventory diagnostic;
- callable parameters and callable returns are rendered from MIR type names,
  not from the retained `AST_EVENT_HANDLER_TYPE` node;
- gate: `make mir-only-signature-test-smoke`, included by
  `make backend-fail-closed-test-smoke`.

This rung does not claim that all backend semantic reads are gone. The next
consumer migrations are declaration inventory/projection facts, MIR expression
facts, and the single runtime-call ABI row authority. Each must retain the same
AIR-bound admission and negative gate before the status can move from `PARTIAL`
to `CLOSED`.

### Declaration inventory sub-rungs

The C backend's nominal forward-typedef pass is now MIR-owned. It reads the
declaration kind and stable name from `MIRDeclHeaderInventory`; it no longer
walks the AST declaration arrays or calls the AST declaration-name owner for
classes, parties, rosters, relations, effects, zones, and worlds. A missing
header name produces a MIR inventory diagnostic and stops emission. The
`mir-declaration-inventory-test-smoke` gate and the MIR-only backend gate keep
this AST path from being reopened.

This is intentionally a sub-rung, not a claim that every domain emitter is
AST-free. Class, relation, effect, party, roster, zone, and world dispatch now
select their declaration from MIR headers. Constructor consumers and the LLVM
bootstrap remain declaration-side replacement seams.

### Declaration type materialization sub-rung

The declaration views now mark a selected MIR header as metadata-required even
for header-only callers. C field/slot emitters for relation, effect, party,
roster, and world declarations consume `MIRDeclField.type_name` through the
bounded C type-name owner. If that row is absent, emission reports a MIR
inventory diagnostic; it does not recover the AST type node. The non-MIR
compatibility branch remains available only for the legacy AST-owned path.

This closes a narrower but important ABI seam: the nominal layout's C type is
now derived from the same MIR declaration row that owns the field identity,
rather than from a second AST payload. Relation/effect and party/roster
dispatch now select `emit_*_decl_from_mir_header` directly from the inventory.
The remaining declaration work is the AST-only role implementation surface,
constructor consumers, and LLVM declaration/bootstrap paths.

World derived-state and command-directive rows are now captured and verified in
`MIRDeclHeader`. When MIR is active, both the C and LLVM world emitters consume
those rows for world layout, derived-state, and activate/maintain/deactivate
passes. A missing header, count, name, source kind, slot, detail, aggregate
input, aggregate input target, or directive target fails closed. Aggregate
inputs and directive state references are resolved through MIR-backed world
zone/state views; world layout, frontier, derived-state, and `HasZone` query
consumers use the same rows, and neither backend rescans the AST to recover
the decision.
This remains a bounded sub-rung: constructor consumers and LLVM bootstrap are
still open seams and are not silently treated as MIR-only closure.

### Callable declaration sub-rung (AIR-bound)

The LLVM routine forward-declaration path now consumes `MIRCallableSig` rows
for `EventHandler` parameters and returns. The row is produced during MIR
signature capture and reaches LLVM only after the AIR-bound verified projection
plan has admitted the backend; LLVM does not use the retained
`AST_EVENT_HANDLER_TYPE` node to reconstruct the function-pointer ABI on an
active MIR routine. A missing callable row, parameter type name, or callable
arity overflow is reported through the MIR inventory diagnostic and stops
declaration emission. The C path keeps the same MIR callable owner, so the two
backends project the ABI differently while consuming one row.

The executable evidence is the LLVM object compile, the full LLVM compiler
link, `mir-declaration-inventory-test-smoke`, and `tests/llvm_smoke.sh` (which
includes `event_system`). This is a bounded routine-declaration sub-rung, not
full LLVM declaration/bootstrap closure: method/domain callable declarations,
constructors, and runtime-call ABI rows remain separate open seams.

### Runtime-call row consumer sub-rung (AIR-bound)

The active LLVM Slot/SecureSlot consumers now enter through the single
`llvm_slot_runtime_row_for_operation` owner in
`src/codegen/llvm_runtime_row.c`. Runtime declaration inventory remains in
`llvm_runtime.c`; it is no longer physically mixed with row selection. The
row owner reads the concrete
`MIRResourceRuntimeRow`, checks the operation's call shape, and rejects a
machine-layer runtime-operation mismatch before LLVM function lookup. The
LLVM MIR pin-region path uses that same owner for secure pin initialization
and unpin cleanup, and LLVM identifier auto-read uses it for Slot/SecureSlot
Read, so pin/read-specific call-shape checks are not duplicated in separate
LLVM consumers. SecureSlot runtime declaration registration also resolves
Claim/Read/Write/Release/pin rows through that owner. The consumer files no
longer call the symbol-only
`mir_abi_resource_runtime_fn_by_kind` API. A missing row is still observable
as `NULL` so the explicitly bounded nominal/structural lowering path can run;
external-helper paths fail closed with the existing MIR ABI diagnostic.
The backend fail-closed gate also rejects any direct resource-row lookup in
LLVM consumers outside the shared owner implementation.

The native MIR registry is split on the same boundary. Static canonical rows
and `RuntimeCallAbiId` remain in `mir_abi_resource_runtime.c`; constructed
nominal spelling and call-shape materialization live in
`mir_abi_resource_runtime_constructed.c`; exact source/ABI lookup and derived
pin rows live in `mir_abi_resource_runtime_mir.c`. All three are below the
production owner cap, and the Makefile inventory plus ABI ownership gate names
the responsibility split.

Pin enter/exit has no source call node to own a runtime row. MIR therefore
authorizes `PinReadInit`, `PinWriteInit`, and `Unpin` only from a same-type
`Read`/`Write` resource instruction, and exposes both the owner instruction and
the derived row through `mir_abi_resource_runtime_*_pin_*_for_mir`. LLVM uses
that MIR owner for layout identity validation; it does not inspect the stale
backend instruction cursor or synthesize a row from `SecureSlot<T>` spelling.
If the authorizing MIR row is absent, the pin path fails closed.

The active C MIR resource-op path now has the corresponding producer seam:
MIR lowering resolves the static or constructed `Slot<T>` family row once and
copies it into `MIRInstruction.resource_runtime_fact`, including the runtime
symbol, materialization, and call shape. C MIR resource emission consumes that
fact and rejects a missing row instead of reconstructing a symbol from a type
suffix. C pin/block/builtin/let-slot emitters now route through the same
`transpiler_slot_runtime_row_for_source_operation` owner; its only direct kind lookup
is the explicit non-MIR compatibility branch. The self-host MIR producer now
attaches the same row to each machine-layer instruction from its carried
operation plus the routine builder's semantic local/parameter type fact. It
uses `runtime_call_abi_structured_fact_owner.pgy`; it does not parse a row
string, reopen source text, or call the C oracle. The self-hosted `mir_lower`
consumer validates the nested `runtime_call_abi` fact before it suppresses
evidence-only resource rows.

Slot-sugar initialization is also explicit carriage rather than a backend
exception. Its `MIR_INST_DEF` carries the canonical Claim row and layout ID.
The shared MIR verifier validates every present runtime-call row regardless of
instruction kind, and C/LLVM may consume the active DEF row only when its
operation matches the requested operation. An implicit Write sharing the same
source node as Claim must reject the mismatched exact row and continue through
the MIR-owned same-type operation projection; it cannot treat Claim as Write.

One executable statement may contain more than one resource operation, for
example `Write(a, Read(b) + Read(c))`. Such a statement cannot own one
meaningful runtime-call row. Lowering therefore keeps the authoritative row on
each `MIR_INST_RESOURCE_OP` and copies it to a DEF/STMT consumer only when the
consumer has exactly one resource source. RIR assigns every resource operation
the stable identity of its enclosing source statement. MIR carries that fact
onto both resource operations and executable statement consumers; the linker
uses source-statement order, the carried stable statement identity, or the
exact source-operation identity. It does not compare source lines or re-read a
call's callee/arguments. Multiple matches remain uncollapsed. C and LLVM
nested-call emitters use the source-stable operation identity to select the
exact MIR row. When a MIR routine and source call are present, failure to find
that exact row is an error; the backend must not recover it from the ABI table.
The focused
`while_loop_slot_read` and `three_slots_cross_update` backend comparison is the
multi-operation regression gate.

Synthetic consumers are owner-derived, never table-fallback consumers. A
typed `Slot<T>`, `SecureSlot<T>`, or `DeviceSlot<T>` DEF and a destructure
Claim carry the canonical Claim row on that MIR owner instruction. When an
auto-read, pin helper, or other source expression has no unique resource-op
identity, the C and LLVM row owners may derive the requested operation only
from an existing same-type MIR runtime owner. If every same-type owner fact is
removed, lookup returns no row and the backend fails closed; the static ABI
table cannot authorize the operation by itself. The permanent MIR gate is
`MIR synthetic slot consumers derive rows from an existing owner`, including
the negative owner-removal assertion.

`with slot` cleanup is also an MIR ordering fact. The lowering inventory moves
the with-owned Release after all body-tail consumers before the block becomes
backend input. Release is excluded from the residual-statement suppression
predicate, so a body `Read` nested in `Print` remains concrete and is not
silently replaced by an observability export. The C source-order fixture
ratchets claim/write/read/print/release order for one and two with slots; LLVM
consumes the same MIR order and row identity.

The runtime-call row now has one declared SoT owner: `SFAbiRuntimeCallRows`
under `SOMirAbi`. The self-host `runtime_call_abi` artifact is a projection of
that owner, not a second authority. MIR lowering now materializes a stable
`runtime_call_abi_id` from the canonical `domain|abi_type|operation` key after
the single row lookup. C, LLVM, and self-host MIR consumers validate the ID
against the same key. Table position is deliberately irrelevant. Symbol,
target kind, materialization, and call shape are separately compared with the
canonical row owner, so a valid ID cannot authorize a mutated payload. A
changed payload or carried ID fails closed in native MIR, C, LLVM, and the
self-host MIR consumer. The self-host projection uses `Long` for the hash
intermediate so its arithmetic is byte-equal to the native `uint64_t` owner,
while the carried wire value remains a bounded positive `Int`/`uint32_t`.
`LayoutId` and `RuntimeCallAbiId` occupy disjoint fixed namespaces: layout IDs
use `0x2...` and runtime-call IDs use `0x4...`. Tests reject a zero, duplicate,
or wrong-namespace runtime-call ID and a changed layout-row identity.

This closes the stable-identity sub-rung for static and constructed nominal
rows. `pergyra.runtime-call-abi.v2` remains `BRIDGE` because the complete
aggregate/runtime compatibility corpus and every legacy compatibility path
have not yet migrated behind the same policy. The compatibility policy is no longer
documentation-only: `driver_diag_compatibility_manifest_validate_file` now
requires the seven `runtime_call_abi_*` rows, rejects duplicates/unknown keys,
and fails closed on a changed policy value before native compilation starts.
Self-host projection and C/LLVM row consumers still use the same declared
policy; the remaining bridge is the wider artifact/consumer migration, not
the existence of a policy string. The focused evidence
is `abi-ownership-shape-test-smoke`, `backend-fail-closed-test-smoke`,
`machine-layer-pipeline-test-smoke`, and
`tests/self_hosted/parity/mir_abi_first_lane.sh`. The stable-ID negative
ratchet is also covered by `src/tests/mir/test_mir_runtime_call_abi.cases.h`
and the driver rung-2 wrong-ID mutation in
`tests/self_hosted/parity/driver_rung2_resource_runtime_abi_negative_owner.sh`.

### Static ABI layout identity sub-rung (AIR-bound)

Static `MIRTypeLayout` rows now have a content-derived `LayoutId` materialized
on every MIR instruction that carries a layout. The owner is
`mir_abi_layout_id()` in `src/compiler/mir_abi_layout.c`; its hash covers the
canonical type name, size/alignment, ordered field rows, representation/tag
facts, runtime type, and niche policy. It deliberately excludes table index and
pointer address, so C and LLVM can consume the same logical identity even when
their in-memory tables differ.

`mir_fact_surface_validate.c` rejects a missing or changed `abi_layout_id`
before AIR-bound projection reaches either backend. The negative MIR fixture is
`MIR validator rejects missing ABI layout identity` in
`src/tests/mir/test_mir_lowering_part_a_1.cases.h`, and the ABI ownership shape
gate requires both the owner and the mutation. This is a bounded identity rung:
dynamic nominal layouts still use their runtime-call row, and the self-host JSON
projection has not yet promoted static layout IDs to a separate compatibility
row. Therefore `pergyra.abi.core.v1` remains `BRIDGE` until aggregate/layout
consumers and the self-host wire row migrate together.

The constructed-nominal exception is now owner-directed rather than a backend
guess: `mir_abi_resource_runtime_row_is_constructed_nominal()` recognizes only
the ABI owner's `constructed-resource` / `constructed_resource_runtime_spelling`
materialization pair. LLVM may accept the absent static layout row only for
that explicit compatibility edge; a missing layout on a native/static row
still fails closed. No backend may derive a symbol or layout from the nominal
type spelling.

The C last consumer is now fail-closed as well. When MIR is active,
`transpiler_mir_resource_op_core.c` must receive the instruction's complete
layout and matching `LayoutId`; it cannot reconstruct a layout with
`mir_abi_lookup(abi_type_name)`. That lookup remains only on the legacy
non-MIR path. The backend guard is covered by
`tests/backend_fail_closed_smoke.sh`, while AIR-bound validation remains the
earlier shared admission gate for both C and LLVM.

LLVM versioned-DEF copying follows the same rule. Once a MIR routine is active,
`llvm_mir_source_def_copy.c` takes Future/Channel/nominal registration from the
routine's `MIRSourceLocalType` fact (or the instruction's carried ABI type) and
does not inspect `inst->expr1` as an AST type annotation. The AST registration
branch remains explicitly legacy-only; an active MIR instruction with neither
owner fact reaches the existing typed-metadata failure later in the LLVM MIR
consumer instead of recovering a type from source syntax.

The view-backed C hook follows the same boundary. Its prior-instruction scan
may reuse an already carried `MIRTypeLayout`, but its AST type-annotation
lookup (`transpiler_mir_layout_from_type_annotation`) is now explicitly
legacy-only. An active MIR routine with no owner-carried layout reaches the
existing missing-owner/layout diagnostic instead of reopening the AST node.
Statement identity is carried by the shared `mir_set_inst_source_statement_fact`
owner for DEF and non-CFG instructions, so multiple resource rows emitted from
one source statement retain the same statement identity without rescanning the
AST. The fixture `MIR keeps multiple resource runtime rows distinct within one
statement` ratchets both distinct row ownership and that shared identity path.
The ABI ownership shape gate carries a negative ratchet for this branch.

See also:

- `docs/180_compiler_logical_spine_handles_gates.md` for the owner/gate matrix;
- `docs/192_protocol_abi_api_registry.md` for protocol and ABI status;
- `tests/mir_only_signature_smoke.sh` for the executable negative ratchet.

## Hard self-host operating lane

### Objective card

- objective: replace the bounded DRV-2 machine path with a Pergyra producer and
  consumer before expanding the default driver;
- priority: canonical MIR identity, ABI/projection rows, emitted artifact,
  diagnostics, then runtime output;
- fact owners: `SelfMirProgramFacts`, the machine-layer declaration consumer,
  ABI layout rows, and runtime-call ABI rows;
- last legitimate consumers: the self-host MIR verifier and C emitter;
- forbidden fallback: native MIR, AST compatibility text, or C-derived facts
  entering `CompileSourceToMirJsonVerified` or `CompileSourceToCVerified`;
- focused gate: `make self-host-mir-abi-first-test-smoke`.

`DRV-2` is the active replacement lane. The C compiler is a read-only oracle,
not a provider for the self-host producer. The blocking comparison order is:

1. self-produced canonical MIR JSON;
2. MIR-carried projection and ABI rows;
3. MIR-consumed C artifact under C-built and LLVM-built self drivers;
4. byte-stable diagnostics for rejected fixtures;
5. runtime output against the native C oracle.

The explicitly named `--canonicalize-oracle-mir-json` mode may normalize only
the native oracle artifact for comparison. It is not callable from
`CompileSourceToMirJsonVerified` or `CompileSourceToCVerified`. If the self
producer lacks a fact, the producer fails before canonicalization or emission.

Machine-layer `DeviceSlot`, `RemoteFuture`, and routine-parameter programs are
DRV-2 fixtures 78 through 80. They require the target-owned declaration fixture
`tests/self_hosted/fixtures/machine_layer_declaration.json` as an external ABI
input, carry its physical grant together with self-produced contact rows into
canonical MIR, and run through both self-driver backend builds. Fixture 80
also proves that `DeviceSlot<Int>` parameter carriage and concrete Read and
Release instructions consume the self-produced ABI row. The C oracle does not
generate this comparison input. Omitting the declaration or removing the
concrete Read row is a negative fixture, and neither the C oracle nor native
MIR may fill the missing row.

The MIR-to-tree comparison bridge treats `resource-op` as evidence, not as a
second executable statement. Executable `def`/`stmt` rows retain the machine
contact facts. Reinterpreting a `resource-op.expr0` as source text would create
an AST fallback and is blocked by the hard-self-host contract.

### Target projection carriage

The DRV-2 C emitter no longer accepts target readiness from the global envelope
alone. `target_capability_owner.pgy` remains the vocabulary owner,
`target_projection_fact_owner.pgy` derives one admitted projection carriage
row, and `CompilerEmissionArtifact` records its schema and projection.
`GenerateCFromVerifiedSemanticArtifact` consumes that row; its verifier checks
the carried values against the canonical owner and rejects an empty or changed
projection before C text emission. The negative
owner is
`tests/self_hosted/parity/driver_rung2_target_projection_negative_owner.sh`;
the focused `device_slot_routine` lane passes under both C-built and
LLVM-built Pergyra drivers.

This row is deliberately still `BRIDGE`. It proves that the selected `cpu-c`
projection and the ordered required-fact/fallback vocabulary survive to the
last emitter. It does not yet carry a native target fingerprint, concrete
size/alignment/endian facts, object format, AIR evidence references, or an
LLVM/self-hosted physical projection. Those values must become target-owned
facts before `target.capability_profile` can be promoted.

### Comparison cadence

Oracle comparison is not postponed until the whole compiler exists. Every
executable replacement rung must compare its own bounded canonical MIR,
ABI/projection rows, emitted artifact, rejection diagnostic, and runtime result
against C/LLVM before the next owner surface opens. What is deferred is the
full CI matrix, not the local semantic comparison.

The expansion order after the bounded machine fixtures is:

1. declaration and routine fact production;
2. CFG, SSA, cleanup, and cancellation consumption;
3. aggregate layout and runtime-call ABI projection;
4. additional machine-contact and target-capability rows;
5. whole-corpus diagnostics, artifacts, and runtime parity;
6. fixed-point bootstrap and default-driver replacement.

Each item advances only when the Pergyra producer supplies every required fact,
the consumer rejects a removed fact, and C remains read-only. Broad
`self-host-preparation-test-smoke` and platform CI run at coherent integration
boundaries; they are not a prerequisite for every owner-sized implementation
commit.

### Bootstrap boundary

The focused MIR/ABI-first lane is green, but compiler-scale codegen bootstrap
is not. A seed-only regeneration attempt did not emit `gen1.c`; it was stopped
after roughly 15 minutes at 8.98 GB working set and 12.5 GB private bytes to
avoid exhausting the workstation. This is the already isolated codegen string
amplification blocker, not evidence that DRV-2 lacks a semantic fact. No fixed
point or whole-compiler bootstrap claim follows from the focused lane.
