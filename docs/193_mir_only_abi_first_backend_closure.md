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
AST-free. Class, relation, and effect dispatch now select their declaration
from MIR headers. Party, roster, zone, and world dispatch, constructor
consumers, and LLVM bootstrap remain declaration-side replacement seams.

### Declaration type materialization sub-rung

The declaration views now mark a selected MIR header as metadata-required even
for header-only callers. C field/slot emitters for relation, effect, party,
roster, and world declarations consume `MIRDeclField.type_name` through the
bounded C type-name owner. If that row is absent, emission reports a MIR
inventory diagnostic; it does not recover the AST type node. The non-MIR
compatibility branch remains available only for the legacy AST-owned path.

This closes a narrower but important ABI seam: the nominal layout's C type is
now derived from the same MIR declaration row that owns the field identity,
rather than from a second AST payload. The remaining declaration work is
header-driven dispatch for the other domain emitters and their AST-only role
implementation surfaces.

See also:

- `docs/180_compiler_logical_spine_handles_gates.md` for the owner/gate matrix;
- `docs/192_protocol_abi_api_registry.md` for protocol and ABI status;
- `tests/mir_only_signature_smoke.sh` for the executable negative ratchet.
