# Architecture Decision: Slot Pinning / Lease

Last updated: 2026-06-24

Related documents:

- `docs/19_design_philosophy.md` §0 — core identity: Pergyra is a systems
  language first, and Slot/Pin/Lease is layered on that baseline.
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

System-language boundary:

- Pin/Lease is a typed lexical lease, not the system-tier raw escape described
  in `docs/19_design_philosophy.md` §0.
- Pin/Lease amortizes repeated slot validation for hot paths while preserving
  capability, generation, token, and cleanup invariants.
- It does not by itself satisfy driver/kernel/embedded/ISR needs such as raw
  pointer exposure, MMIO pointer arithmetic, or inline assembly operands.
- If Pergyra adds a system-tier raw pointer escape, that surface must be a
  separate scoped unsafe capability contract, such as `unsafe(raw) { ... }`,
  with explicit syntax, semantic gates, AIR evidence, ABI lowering,
  diagnostics, and determinism tests. It must not weaken the typed Pin/Lease
  contract.
- `SlotRawPointer(...)` is reserved for that future direction but currently
  rejects with `PGY_SEM_RAW_ESCAPE_UNSTABLE`; `unsafe { ... }` does not bypass
  the rejection.

Current beta status:

- Runtime ABI baseline exists: `PgyPinnedView`, `PergyraSlotPin(...)`, and
  `PergyraSlotUnpin(...)`.
- Generated inline slot ABI now has a matching typed wrapper layer:
  `PgyPinnedSlotView_*`, `PgyPinnedSecureSlotView_*`,
  `pgy_pin_read_*`, `pgy_pin_write_*`, `pgy_unpin_*`, and secure pin/unpin
  helpers. This keeps the existing `PgySlot_*` / `PgySecureSlot_*` layout stable
  while giving C and LLVM a shared call surface for source-level pin cleanup.
- C source-block emission now lowers a recognized pin block to a typed wrapper
  local with a GCC cleanup hook (`pgy_unpin_cleanup_*` /
  `pgy_secure_unpin_cleanup_*`). C MIR emission consumes pin-region metadata
  through `src/codegen/transpiler_mir_pin_emit.h`. For plain `Slot<T>` MIR pin
  regions with cleanup-edge evidence, it emits a preflight local plus
  `PgyPinnedSlotView_*` initializer directly; secure pin still emits typed
  secure pin/unpin calls before successor and return exits.
- LLVM MIR emission now lowers plain `Slot<T>` pin entry/exit as inline IR:
  a null-slot guard, an `occupied` field load through canonical slot layout,
  and direct `PgyPinnedSlotView_*` field stores. Secure pin still goes through
  the secure runtime pin/unpin ABI because token validation is a capability
  retain point, not a plain-slot layout operation.
- `WriteView<T>` exclusive access is now enforced for the existing
  `ViewRead(...)` / `ViewWrite(...)` semantic surface: a new `WriteView<T>`
  conflicts with any active view of the same slot, and a new `ReadView<T>`
  conflicts with an active `WriteView<T>`.
- Releasing or moving the source slot while a source-level `pin` block or
  existing typed `ReadView<T>` / `WriteView<T>` over that slot is live is now a
  semantic error (`PGY_SEM_PIN_PARALLEL_CONFLICT`) and has JSON diagnostic
  coverage.
- Direct owner writes while any typed view is live, and direct owner reads while
  a `WriteView<T>` is live, are also rejected so the view contract cannot be
  bypassed by spelling the original slot name.
- Slot sugar follows the same rule: `slot = value` is treated as an owner write,
  and value-position `slot` use is treated as an owner read.
- Passing the owning source slot to an `own`/`ref Slot<T>` helper while a typed
  view is live is rejected. Helpers must accept the typed view directly or be
  called outside the pin/view scope.
- Storing or forwarding the owning source slot through a stable container
  boundary while a typed view is live is rejected. This covers array literals
  and `ArrayPush`, `ArraySet`, `ListPush`, `ListSet`, `SetAdd`, `MapSet`, and
  `QueuePush`.
- Returning the owning source slot while a typed view is live is rejected at
  semantic time instead of falling through to backend auto-read/type drift.
- `Box<T>` and `BoxSet` reject resource-handle payloads in the beta-stable
  surface. Boxing a slot handle would create a second storage owner that the
  current CFG/ABI proof layer does not claim.
- The existing view surface now emits the pin-specific diagnostics for the
  first stable escape/boundary cases: returning a view reports
  `PGY_SEM_PIN_ESCAPE`, crossing `await` reports
  `PGY_SEM_PIN_AWAIT_BOUNDARY`, crossing/acquiring inside `parallel` reports
  `PGY_SEM_PIN_PARALLEL_CONFLICT`, and `ViewRead/ViewWrite(QubitSlot)` reports
  `PGY_SEM_PIN_QUBIT_REJECT`.
- Source-level `pin slot as view: ReadView<T>|WriteView<T> { ... }` is now
  accepted and desugars to the existing typed `ViewRead(...)` / `ViewWrite(...)`
  semantic surface.
- The stable hot-path runtime lowering is still narrower than the design target:
  raw `PgyPinnedView` / `PergyraSlotPin` / `PergyraSlotUnpin` remains the
  table-backed hard non-eviction ABI, while generated inline slots use typed
  wrapper views. Plain C/LLVM MIR pin blocks with
  `mir_block_has_pin_guard_amortization_region(...)` evidence no longer require
  the plain `pgy_pin_*_init_*`, `pgy_pin_*`, or `pgy_unpin_*` runtime call path.
  Source-block C cleanup still uses the typed cleanup-hook wrapper path, and
  secure pins still retain the pointer/token runtime ABI in C and LLVM.
  Explicit cleanup-edge lowering is implemented for C source-blocks and for
  C/LLVM MIR successor/return slices over the frozen pin backend-compare
  fixtures, including normal successor cleanup, direct return from inside a pin
  block, conditional branch-to-return cleanup, and loop `break`/`continue`
  cleanup.

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
- `defer` registration while a view is live is rejected because the cleanup may
  run after the pin scope has ended.
- `spawn`/`async` capture of a view is rejected.
- `parallel` access to the same slot with a `WriteView<T>` conflict is rejected.
- `Release(slot)` / `Move(slot)` while any active typed view is live over
  `slot` is rejected.
- `Write(slot, ...)` while any active typed view is live over `slot` is
  rejected; `Read(slot)` is rejected while an active `WriteView<T>` is live.
- Slot sugar (`slot = value`, `Log(slot)`, or another value-position owner
  identifier use) cannot bypass those same owner read/write restrictions.
- `Helper(slot)` cannot bypass the lease through an `own`/`ref Slot<T>` helper
  parameter while a typed view over `slot` is live.
- `ListPush(items, slot)`, `MapSet(map, key, slot)`, `ArrayPush(items, slot)`,
  `[slot]`, and equivalent stable container-store paths cannot forward the
  owning source slot while a typed view over that source is live.
- `return slot` cannot forward the owning source slot while a typed view over
  that source is live.
- `Box(slot)` / `BoxSet(box, slot)` are not stable resource-handle storage
  paths; use a copied payload or keep the slot in its owning binding.
- Read sharing across parallel tasks is post-beta unless the runtime and CFG
  agree on a task-local read lease model.
- Device pinning must define host/device synchronization, stale mapping, and
  mapping failure behavior before it becomes stable.

## 6. Diagnostics

Planned semantic diagnostic codes:

- `PGY_SEM_PIN_ESCAPE`: view escapes its lexical scope.
- `PGY_SEM_PIN_PARALLEL_CONFLICT`: parallel tasks conflict on a pinned slot.
- `PGY_SEM_PIN_AWAIT_BOUNDARY`: pin crosses an `await` suspension boundary or
  cleanup boundary such as `defer`.
- `PGY_SEM_PIN_QUBIT_REJECT`: attempted pin of `QubitSlot`.
- `PGY_SEM_PIN_TOKEN_INVALID`: token/capability check fails for a secure pin.

Current implementation note:

- Block syntax is active at the parser/semantic layer and lowers to a lexical
  block containing `let view: ReadView<T>|WriteView<T> = ViewRead/ViewWrite(slot)`.
- All five semantic codes above are active on both the source-level
  `pin ... as ... { ... }` block and the existing `ViewRead(...)` /
  `ViewWrite(...)` surface; the first four are covered by
  `make diagnostics-json-test-smoke`.
- `PGY_SEM_PIN_TOKEN_INVALID` now fires from the source-level surface when
  `ViewRead(...)` / `ViewWrite(...)` is applied to a `SecureSlot<T>` and the
  paired capability token symbol is not reachable in the current scope. The
  runtime ABI capability hard-fail remains the deeper backstop, but the
  source-level diagnostic catches token-missing pins before runtime.

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
- The current table-backed runtime ABI uses 32-bit `slotId` / `generation`.
  It therefore must reject the zero-id sentinel and tombstone the manager before
  `slotId` wrap instead of silently reusing an old id. `SlotClaim` now hard-fails
  with `SLOT_ERROR_ID_EXHAUSTED` when the id space is exhausted.
- Unpin must match the issued view generation, mode, thread affinity, and
  pointer identity. A tampered view cannot clear the runtime pin state.
- Secure write pin opens sealed payload into a lease buffer and reseals on
  unpin.
- Secure lease buffers are wiped after unpin.
- Pinned entries cannot be released or cleaned by TTL while the pin is active.
- Secure scope destruction must not wipe scope-owned handles/tokens while any
  scoped secure slot is pinned. Checked destruction returns `SLOT_ERROR_PINNED`;
  the legacy void destructor panics because losing the token would make the
  pinned secure slot unreleasable.

## 7a. Evidence View Cache Policy

Pin/Lease is the stable evidence-amortization path for repeated Slot access. It
is not a promise that every Slot operation is zero cost. The intended shape is:

1. validate the handle, generation, mode, authority, token, and layout once at
   the lexical entry point;
2. materialize a typed evidence view whose lifetime is bounded by MIR pin-region
   and cleanup-edge facts;
3. use that view on the hot path without repeating the full owner/generation/
   capability/state guard on every indexed access;
4. invalidate the view on every exit, mutation boundary, release/move, async or
   parallel boundary, token revocation, generation change, or layout change.

This path is cacheable only as an acceleration cache over MIR facts, never as a
second source of truth. The cache key must include at least:

- source Slot identity;
- generation or equivalent freshness epoch;
- access mode (`ReadView<T>` or `WriteView<T>`);
- capability/token identity for secure slots;
- canonical payload type/layout fact;
- MIR pin-region id and cleanup-edge owner.

There are three allowed cache scopes:

| Scope | Allowed? | Rule |
|---|---|---|
| local SSA/view variable inside one pin block | yes | Preferred hot path. The view dies at the lexical cleanup edge. |
| loop/region-local preflight reused across repeated reads or writes | yes | Requires `mir_block_has_pin_guard_amortization_region(...)` and no invalidating edge in the region. |
| cross-call, cross-intent, async, parallel, or persistent runtime cache | no for beta | Requires a future retained materialization contract with explicit epochs, revocation, and task ownership. |

The cache must be fail-closed. A missing fact, mismatched generation, unknown
layout, or escaped view rejects or retains through the documented runtime path;
it must not silently fall back to a stale cached pointer. Plain Slot MIR pin
regions may inline the preflight view because the layout fact is static. Secure
Slot pinning remains a policy retain point unless the token/capability evidence
is also proven local and non-escaping.

Current measurement:

- `benchmarks/perf_guard_amortization.c` compares per-access guards with a
  repeated-preflight no-cache path and a one-time cached preflight evidence
  view. The fixture reports internal benchmark-process timings to avoid shell
  launch and scheduler noise dominating the signal.
- `make evidence-guard-amortization-test-smoke` gates the source shape and, on
  supported shell/toolchain paths, the guard and cache-effect best paired
  ratios.
- The cache target is not "zero cost"; it is evidence cost paid once per
  proven region and amortized across the hot loop.

## 8. CFG / AIR / Backend Interaction

CFG requirements:

- Model pin as acquire/release resource edges.
- Insert cleanup on early return, loop exit, panic/unwind, and branch joins.
- Track view escape as the same family as borrow/resource escape.
- Track `WriteView<T>` as aliasing-XOR-mutability evidence.
- Reject or retain any attempted evidence-view cache whose invalidation point is
  not visible in CFG/MIR.

AIR requirements:

- AIR may validate capability evidence and boundary drift.
- AIR must not silently change codegen behavior.
- Strict evidence mode must fail when authority evidence for a secure pin is
  missing or inconsistent.
- AIR records why a retained/materialized view survives; it does not decide a
  cache hit from backend-local runtime symbols.

Backend requirements:

- C and LLVM must lower through the same runtime ABI.
- For current generated inline slots, that ABI is the typed wrapper surface
  (`pgy_pin_read_*`, `pgy_pin_write_*`, `pgy_unpin_*`) rather than the
  table-backed `SlotManager` functions. A later ABI migration may make
  source-level `Slot<T>` lower directly to `SlotHandle`; until then the two
  runtime layers are intentionally separate.
- Generated code may use raw pointers internally, but the source surface stays
  typed.
- Generated code may cache a typed view only when MIR pin-region facts and
  cleanup-edge facts prove the view lifetime and invalidation boundary.
- Backend compare must cover read pin, write pin, early return cleanup, invalid
  token, qubit reject, and conflict cases before the syntax is stable.

## 9. Implementation Order

1. Runtime ABI baseline: `PgyPinnedView`, pin, unpin, release/cleanup blocking.
2. Parser syntax: accept `pin slot as view: ReadView<T>|WriteView<T> { ... }`
   without reserving `pin` as a keyword.
3. AST and semantic model: desugar to a lexical block containing the typed
   view constructor, preserving existing escape/boundary diagnostics.
4. CFG integration: cleanup edges, escape facts, await/spawn/parallel rejects.
5. Backend parity: C/LLVM lowering through the same ABI and cleanup paths.
6. Diagnostics: register the five semantic codes and add JSON regression.
7. Stable surface decision: either promote the block syntax or keep it explicitly
   experimental.

Steps 1 to 3 are implemented for the typed-view front-end slice. The source
block and existing view constructor surface now share exclusive-write, return
escape, await/spawn/async/callback/channel/cancel/defer boundary,
parallel-boundary, and QubitSlot reject semantic gates covered by
`make test-semantic` and
`make diagnostics-json-test-smoke`; the source-level pin block has explicit
cancel/defer boundary JSON regressions so cleanup-boundary failures cannot leak
to C/LLVM codegen. The typed-view read/write path now has C/LLVM
backend parity for plain, secure, and sequential mixed slot blocks through
`pin_read_view_block`, `pin_secure_read_view_block`, and
`pin_mixed_read_view_sequence`, plus write fixtures `pin_write_view_block` and
`pin_secure_write_view_block`. The generated inline runtime wrapper ABI is
covered by `make test-memory`, and the C source-block wrapper emission is
covered by `make test-transpile`. C MIR successor/return cleanup emission is
also covered by `make test-transpile` with explicit `pgy_pin_*` / `pgy_unpin_*`
ordering around the successor path. C/LLVM backend compare now covers
source-level plain read, secure read, plain write, secure write, and mixed
plain+secure pin blocks. Remaining beta work is narrower: expand the all-exit
proof/regression matrix and add richer secure-token source diagnostics.

## 10. Beta Position

Pin/Lease belongs under `docs/100_beta_readiness_checklist.md` §4
ABI Ownership / Arena Lifetime Closure.

The correct beta promise is narrow:

- Runtime ABI may exist internally.
- Source syntax is accepted only as a typed-view lexical block until CFG cleanup
  and backend parity close for the internal pin runtime ABI. The current
  typed-view slice has read/write parity, runtime wrapper coverage, C
  source-block cleanup emission, and C/LLVM MIR successor/return cleanup
  emission over the frozen fixtures. Active view + `defer` registration is
  rejected semantically. Broader exceptional/cancellation proof coverage and
  richer secure-token source diagnostics remain blockers.
- A manual raw pointer API is not user-facing.
- Pin/Lease must not be advertised as the full systems raw-pointer escape. It
  is the stable typed hot-path lease layer under Slot; system-tier raw escape
  remains a separate beta blocker tracked by `docs/19_design_philosophy.md` and
  `docs/100_beta_readiness_checklist.md`.
- C / LLVM lowering must share the same runtime calls and cleanup behavior.
- `QubitSlot`, `await` crossing, channel escape, and task escape are explicit
  rejects.
- `DeviceSlot<T>` is a candidate because it matters for GPU/Spray, but it is not
  stable until device mapping semantics are implemented.

This is better than exposing a fast API early. If the syntax ships before CFG
cleanup and C/LLVM parity, the slot model becomes a performance escape hatch
instead of a verified capability lease.
