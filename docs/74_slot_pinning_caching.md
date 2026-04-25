# Architecture Decision: Slot Pinning / Lease

Last updated: 2026-04-25

Related documents:

- `docs/100_beta_readiness_checklist.md`
- `docs/106_ownership_model_comparison.md`
- `docs/104_air_compiler_architecture.md`
- `docs/semantics/04_ownership_abi.md`

## 1. Decision

Slot Pinning / Lease is the hot-path answer for Pergyra slots. It is not a
security bypass. It is a scope-entry capability lease:

- Validate slot handle, generation, token, authority, and type at block entry.
- Expose only a typed `ReadView<T>` or `WriteView<T>` inside the lexical scope.
- Automatically unpin on normal exit, `return`, `break`, and panic/unwind paths.
- Block release, reallocation, TTL cleanup, and conflicting writes while pinned.
- Keep raw `void *` inside the runtime/backend ABI. User code never receives it.

Current beta status:

- Runtime ABI baseline exists: `PgyPinnedView`, `PergyraSlotPin(...)`, and
  `PergyraSlotUnpin(...)`.
- `WriteView<T>` exclusive access is now enforced for the existing
  `ViewRead(...)` / `ViewWrite(...)` semantic surface: a new `WriteView<T>`
  conflicts with any active view of the same slot, and a new `ReadView<T>`
  conflicts with an active `WriteView<T>`.
- The existing view surface now emits the pin-specific diagnostics for the
  first stable escape/boundary cases: returning a view reports
  `PGY_SEM_PIN_ESCAPE`, crossing `await` reports
  `PGY_SEM_PIN_AWAIT_BOUNDARY`, crossing/acquiring inside `parallel` reports
  `PGY_SEM_PIN_PARALLEL_CONFLICT`, and `ViewRead/ViewWrite(QubitSlot)` reports
  `PGY_SEM_PIN_QUBIT_REJECT`.
- Parser explicitly rejects candidate source syntax with
  `Pin/Lease syntax is not beta-stable yet`.
- The stable source surface is not shipped until CFG cleanup edges, escape
  diagnostics, and C/LLVM lowering parity are complete.

## 2. Proposed Source Surface

The only candidate beta-stable syntax is block-scoped:

```pergyra
pin scores as view: ReadView<Int> {
    let total = Sum(view)
    return total
}
```

```pergyra
pin pixels as view: WriteView<Int> {
    for i in 0..len {
        view[i] = Normalize(view[i])
    }
}
```

The block form is intentionally close to C# `fixed`: entry validates and pins,
the compiler owns cleanup, and the pointer-like capability cannot escape the
block. Pergyra adds slot generation, token, authority, and lifecycle checks.

`PinnedView<T>` is the future RAII handle form for non-block cases:

```pergyra
let view: PinnedView<Int> = ClaimPin(buffer, mode: .read)
process(ref view)
```

`PinnedView<T>` is classified as `ANCHORED_HANDLE`. It is not stable until CFG
escape rules, function-boundary rules, and backend cleanup lowering are closed.

## 3. Type Applicability

| Type | Beta position | Reason |
|---|---|---|
| `SecureSlot<T>` | Candidate stable | Primary use case. Token/capability validation is required before a typed view is exposed. Runtime baseline exists. |
| `DeviceSlot<T>` | Candidate, not implemented as stable | Important for GPU/Spray and AI workloads, but needs device mapping failure classes and backend parity. |
| `Slot<T>` | Supported runtime primitive, cautious language surface | Useful for hot loops over anchored payloads, but direct arrays/values are often better for trivial data. |
| `QubitSlot` | Explicit reject | Pinning a qubit view would violate the quantum ownership/resource model. |
| `Channel<T>`, remote/world-crossing handles | Explicit reject | A pin must not cross transport, world, or async suspension boundaries. |

## 4. Semantic Contract

`ReadView<T>`:

- Allows read-only indexed or view-style access.
- May later support shared read pins.
- Beta runtime baseline is conservative: one active pin per slot.

`WriteView<T>`:

- Allows mutation through the typed view.
- Requires exclusive access: no other read/write pin, write, release, move, or
  cleanup can conflict with it.
- Must update checksum/access metadata at unpin.

Escape is forbidden:

- Returning a view.
- Storing a view into an outer variable or collection.
- Sending a view through a channel.
- Capturing a view in `spawn`, `async`, `parallel`, callback, or closure state.
- Crossing `await` while a view is live.

The CFG/body dataflow layer must prove these facts. AST-only checks are not
enough because early returns, branch joins, panic paths, and async suspension
need explicit cleanup and escape edges.

## 5. Concurrency Rules

Pinning is task-local unless a later design explicitly proves otherwise.

- `await` inside a pin block is rejected.
- `spawn`/`async` capture of a view is rejected.
- `parallel` access to the same slot with a `WriteView<T>` conflict is rejected.
- Read sharing across parallel tasks is post-beta unless the runtime and CFG
  agree on a task-local read lease model.
- Device pinning must define host/device synchronization, stale mapping, and
  mapping failure behavior before it becomes stable.

## 6. Diagnostics

Planned semantic diagnostic codes:

- `PGY_SEM_PIN_ESCAPE`: view escapes its lexical scope.
- `PGY_SEM_PIN_PARALLEL_CONFLICT`: parallel tasks conflict on a pinned slot.
- `PGY_SEM_PIN_AWAIT_BOUNDARY`: pin crosses an `await` suspension boundary.
- `PGY_SEM_PIN_QUBIT_REJECT`: attempted pin of `QubitSlot`.
- `PGY_SEM_PIN_TOKEN_INVALID`: token/capability check fails for a secure pin.

Current implementation note:

- Candidate syntax is still rejected in the parser as `PGY_PARSE_SYNTAX` with
  the message `Pin/Lease syntax is not beta-stable yet`.
- Four of the five semantic codes above are active on the existing
  `ViewRead(...)` / `ViewWrite(...)` surface and covered by `make test-semantic`;
  their user-facing JSON routing is covered by
  `make diagnostics-json-test-smoke`;
  `PGY_SEM_PIN_TOKEN_INVALID` remains a runtime/API capability-path diagnostic
  until source-level secure pin syntax is promoted.
  `docs/72_diagnostic_codes.md`, but stable source syntax does not emit them
  yet. They become active only when semantic Pin/Lease syntax and CFG checks are
  implemented.

Every diagnostic must include `Reason:` and `Fix:`. Projection/source/target
style provenance is not enough here; the message also needs the slot type, view
mode, and boundary that made the pin invalid.

## 7. Runtime ABI Contract

Internal ABI shape:

```c
typedef enum {
    PGY_SLOT_PIN_READ,
    PGY_SLOT_PIN_WRITE
} PgySlotPinMode;

typedef struct {
    void *ptr;
    size_t size;
    uint32_t slot_id;
    uint32_t generation;
    PgySlotPinMode mode;
    bool valid;
} PgyPinnedView;
```

Required calls:

```c
SlotError PergyraSlotPin(
    SlotManager *manager,
    const SlotHandle *handle,
    PgySlotPinMode mode,
    const TokenCapability *token,
    PgyPinnedView *out_view);

SlotError PergyraSlotUnpin(
    SlotManager *manager,
    PgyPinnedView *view);
```

Runtime invariants:

- `view.ptr` is stable only until `PergyraSlotUnpin`.
- `PergyraSlotUnpin` is single-use. Double unpin is an invalid pin error.
- If `view.generation` differs from the slot generation, unpin is stale.
- Secure write pin opens sealed payload into a lease buffer and reseals on
  unpin.
- Secure lease buffers are wiped after unpin.
- Pinned entries cannot be released or cleaned by TTL while the pin is active.

## 8. CFG / AIR / Backend Interaction

CFG requirements:

- Model pin as acquire/release resource edges.
- Insert cleanup on early return, loop exit, panic/unwind, and branch joins.
- Track view escape as the same family as borrow/resource escape.
- Track `WriteView<T>` as aliasing-XOR-mutability evidence.

AIR requirements:

- AIR may validate capability evidence and boundary drift.
- AIR must not silently change codegen behavior.
- Strict evidence mode must fail when authority evidence for a secure pin is
  missing or inconsistent.

Backend requirements:

- C and LLVM must lower through the same runtime ABI.
- Generated code may use raw pointers internally, but the source surface stays
  typed.
- Backend compare must cover read pin, write pin, early return cleanup, invalid
  token, qubit reject, and conflict cases before the syntax is stable.

## 9. Implementation Order

1. Runtime ABI baseline: `PgyPinnedView`, pin, unpin, release/cleanup blocking.
2. Parser gate: reject candidate `pin slot as view { ... }` syntax until stable.
3. AST and semantic model: typed block syntax, view mode, and target slot type.
4. CFG integration: cleanup edges, escape facts, await/spawn/parallel rejects.
5. Backend parity: C/LLVM lowering through the same ABI and cleanup paths.
6. Diagnostics: register the five semantic codes and add JSON regression.
7. Stable surface decision: either promote the block syntax or keep it explicitly
   experimental.

Steps 1 and 2 are implemented. The existing view constructor surface now has
exclusive-write, return escape, await-boundary, parallel-boundary, and QubitSlot
reject semantic gates covered by `make test-semantic`. Steps 3 to 7 remain beta
blockers if the language chooses to ship block-scoped Pin/Lease syntax in beta.

## 10. Beta Position

Pin/Lease belongs under `docs/100_beta_readiness_checklist.md` §4
ABI Ownership / Arena Lifetime Closure.

The correct beta promise is narrow:

- Runtime ABI may exist internally.
- Candidate syntax must stay rejected until CFG cleanup and backend parity close.
- A manual raw pointer API is not user-facing.
- C / LLVM lowering must share the same runtime calls and cleanup behavior.
- `QubitSlot`, `await` crossing, channel escape, and task escape are explicit
  rejects.
- `DeviceSlot<T>` is a candidate because it matters for GPU/Spray, but it is not
  stable until device mapping semantics are implemented.

This is better than exposing a fast API early. If the syntax ships before CFG
cleanup and C/LLVM parity, the slot model becomes a performance escape hatch
instead of a verified capability lease.
