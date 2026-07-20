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
   dynamic nominal types. That is a materialization seam, not a closed
   `RuntimeCallAbiId` owner.
4. The protocol registry still marks the runtime-call row authority and the
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
`llvm_slot_runtime_row_for_operation` owner. That owner reads the concrete
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

This is only a consumer migration. `pergyra.runtime-call-abi.v2` remains
`BRIDGE` until the runtime-call row owner and compatibility policy are
promoted in the protocol registry and the constructed nominal materialization
edge is either carried as a typed MIR fact or rejected. The focused evidence
is `abi-ownership-shape-test-smoke`, `backend-fail-closed-test-smoke`,
`perf-contract-test-smoke`, the LLVM compiler link, and `tests/llvm_smoke.sh`.

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

Machine-layer `DeviceSlot` and `RemoteFuture` programs are DRV-2 fixtures 78
and 79. They require the target-owned declaration fixture
`tests/self_hosted/fixtures/machine_layer_declaration.json` as an external ABI
input, carry its physical grant together with self-produced contact rows into
canonical MIR, and run through both self-driver backend builds. The C oracle
does not generate this comparison input. Omitting the declaration is a
negative fixture, and neither the C oracle nor native MIR may fill the missing
row.

The MIR-to-tree comparison bridge treats `resource-op` as evidence, not as a
second executable statement. Executable `def`/`stmt` rows retain the machine
contact facts. Reinterpreting a `resource-op.expr0` as source text would create
an AST fallback and is blocked by the hard-self-host contract.

### Comparison cadence

Oracle comparison is not postponed until the whole compiler exists. Every
executable replacement rung must compare its own bounded canonical MIR,
ABI/projection rows, emitted artifact, rejection diagnostic, and runtime result
against C/LLVM before the next owner surface opens. What is deferred is the
full CI matrix, not the local semantic comparison.

The expansion order after the two machine fixtures is:

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
