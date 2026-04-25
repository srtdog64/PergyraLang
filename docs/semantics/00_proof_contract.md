# 00. Global Proof Contract

Last updated: 2026-04-25

Status: `beta-proof-obligation`

This document defines the shared notation for all keyword-level proof documents in `docs/증명/`.

## Semantic Domains

- `tau`: types. Includes primitives, nominal types, `Slot<T>`, `SecureSlot<T>`, stable collections, `Option<T>`, `Result<T,E>`, and function types.
- `Gamma`: local typing environment for values, functions, parameters, captures, and subject slots.
- `ModuleEnv`: module environment for visibility, exports, imports, hidden declarations, and default-export contracts.
- `Delta`: type-resolution dependency graph. Nodes are type providers and consumers; edges encode provider-before-consumer obligations.
- `ResourceState`: ownership, authority, relation/effect/projection metadata, dirty/ready flags, epoch, and cause.
- `ZoneState`: world/zone lifecycle state, embedded zones, handoff state, and frontier scheduling state.
- `History`: runtime observability history for `last`, `history`, `active`, and `recent`.
- `PanicState`: hard-fail runtime state for OOM, divide-by-zero, out-of-bounds, slot invariant breaks, token mismatch, authority mismatch, and internal compiler/runtime invariant breaks.
- `MIRState`: lowered declaration/body/inventory state used by both C and LLVM backends.
- `BackendPair`: C and LLVM artifacts emitted from the same MIR-level semantics.

## Core Judgments

Typing and declaration judgments:

```text
ModuleEnv; Gamma; Delta |- e : tau
ModuleEnv; Gamma; Delta |- stmt ok => Gamma'
ModuleEnv; Gamma; Delta |- decl ok => Delta'
ModuleEnv; Delta |- module ok
```

Resource and runtime transition judgments:

```text
ResourceState |- op => ResourceState'
ZoneState; ResourceState; History |- step => ZoneState'; ResourceState'; History'; outcome
ZoneState; ResourceState |- frontier => ZoneState'; ResourceState'
ResourceState; PanicState |- runtime_op => outcome
```

Lowering and backend judgments:

```text
P => HIR => DIR => RIR => MIR
MIRState |- emit C
MIRState |- emit LLVM
C ~= LLVM under observable beta behavior
```

Diagnostic judgments:

```text
error has code, contract_source, reason, fix
recoverable failure has queryable runtime state
contract violation has stable diagnostic provenance
internal invariant break hard-fails
```

## Global Theorems

### Type Preservation

If `ModuleEnv; Gamma; Delta |- e : tau` and runtime evaluation of `e` produces `v`, then `v` has type `tau` or evaluation produces a typed recoverable failure.

The proof depends on:

- DAG soundness for provider-before-consumer ordering.
- Generic contract soundness for ability/default/multi-bound constraints.
- Module visibility non-interference.
- Runtime failure separation.

### Progress

A well-typed beta program either takes a runtime step, returns a value, produces a recoverable failure, reports a contract violation, or hard-fails only on an internal invariant break.

The proof depends on:

- No accepted stable syntax lowers to a silent backend fallback.
- Runtime authority/projection/frontier failures have defined outcomes.
- Out-of-beta syntax is either rejected or marked experimental.

### Failure Separation

Recoverable failure, contract violation, and internal compiler/runtime bug are disjoint classes.

Required behavior:

- Recoverable failure: queryable state, no process abort by default.
- Contract violation: stable diagnostic provenance with code/reason/fix.
- Internal invariant break: hard-fail path, not presented as normal user failure.
- Runtime panic: hard-fail path with backend-equivalent class and no silent fallback to a different result.

### Backend Observational Equivalence

For accepted beta programs, C and LLVM must agree on:

- stdout/stderr contract.
- exit state.
- runtime observability state.
- recoverable failure state.
- hard-fail class.
- panic class for OOM/divide-by-zero/out-of-bounds/slot violation/token mismatch/authority mismatch.

## Evidence Boundary & Formal Calculus

While regression tests and backend compare runs provide implementation evidence, the core ownership and security semantics of Pergyra (Slot System, Tokens, Pinning) are transitioning towards **Strict Operational Semantics**.

See [08. Slot Capability Calculus](08_slot_capability_calculus.md) for the rigorous mathematical rules and inference proofs governing dynamic capability leases. Any PR modifying the ABI, slot lifecycle, or token capabilities MUST ensure the calculus transition rules hold without logical contradiction.

Proof status labels:

- `CLOSED`: theorem statement, implementation path, diagnostics, tests, and docs agree.
- `IN PROGRESS`: theorem statement exists, but implementation or evidence still has known gaps.
- `BLOCKER`: beta cannot close while this proof obligation is open.
- `OUT OF BETA`: not part of beta soundness.
