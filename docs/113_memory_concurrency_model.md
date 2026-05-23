# Memory And Concurrency Model Beta Contract

Status: beta-freeze-source-of-truth.

This document freezes the beta memory/concurrency contract. The goal is not a
full formal memory model; the beta promise is a narrow, executable contract for
`parallel`, named `spawn`, `async`/`await`, channels, cancellation, and
ownership-bearing payload boundaries.

For *why* this contract is shaped the way it is (positioning vs callback /
promise / async-await waves, function coloring, coloring decomposition,
sequential trap, futurelock-class deadlocks), see
`docs/114_async_model_positioning.md`. This file is the contract; that file
is the rationale.

Executable gate: `make memory-concurrency-model-test-smoke`.

## Stable Execution Surface

- `parallel` is the core execution primitive.
- Named `spawn Worker(args...)` is beta-stable when the callee declaration
  exposes parameter, effect, and ownership facts.
- `async func`/`await` is beta-stable for copy-only values and checked futures.
  `await` is a completion join only; it does not own lifetime, cancellation,
  failure classification, or parallel structure.
- `Future<T>` and `RemoteFuture<T>` are typed completion handles, not a general
  user-level effect system.
- `select` and `channel` are beta-stable for the currently implemented typed
  channel families and copy-only non-blocking receive surface.
- Anonymous async spawn bodies are explicitly rejected for beta. Detached
  anonymous async blocks with local captures are not the stable task-creation
  model; use named `spawn Worker(args...)` so ownership and cleanup facts cross
  a declaration boundary.

## Happens-Before Contract

- A `parallel { ... }` block joins before control continues after the block.
- Writes that are accepted by semantic analysis inside a `parallel` task become
  visible after the join.
- Shared `ref`/`ref` reads of the same ownership-bearing value across parallel
  tasks are accepted.
- `ref`/`own` and `own`/`own` task-boundary conflicts are rejected.
- `WriteView<T>` and pinned view conflicts are rejected across parallel tasks.
- No data-race freedom is promised for `unsafe` or out-of-beta surfaces.

## Undefined-Behavior Hygiene Contract

Pergyra must not treat "it works in the current run" as evidence that shared
state is safe. The runtime and generated code follow these beta rules:

- Non-atomic shared counters are forbidden across worker threads. Allocation
  cursors, publish cursors, generation counters, and cache hit/miss counters
  must be worker-owned, protected by a lock/phase barrier, or implemented with
  C11 atomics.
- Non-thread-safe containers may not be mutated while another thread reads
  them. This is especially strict for open-addressed maps and hash tables:
  insert/rehash invalidates concurrent readers and is undefined behavior unless
  the map is locked, phase-separated, or published as an immutable snapshot.
- Runtime caches are worker-local by default. A shared cache requires an
  explicit publication protocol: build privately, publish once, then read only;
  mutation after publication requires a new snapshot or a lock.
- Static local buffers/state are not thread-safe by default. They must be
  `_Thread_local`, immutable `const`, or guarded by the owning runtime lock.
- Generated code may reuse thread-pool workers, but it may not infer ownership
  from the worker id alone. Worker-id-indexed caches are valid only when the
  cache is exclusively owned by that worker or when all shared entries use the
  same publish/lock protocol above.
- AI-generated parallel code is held to the same rule: no non-atomic
  `current++`-style cursor sharing, no map read during rehash, and no mutable
  static scratch storage crossing task boundaries.

This contract is intentionally narrower than a full C memory model. It is a
source-of-truth rule for Pergyra lowering: if the compiler cannot prove
worker-local ownership, lock/phase separation, atomic access, or immutable
snapshot publication, the boundary must stay rejected or be marked `unsafe`.

Implementation checkpoint: `src/runtime/party_runtime_stats.c` treats the
process-global fiber stats table as a shared registry. `UpdateFiberStats`,
`GetFiberStats`, and `party_runtime_dump_fiber_stats` all acquire the same
registry mutex before touching the open-addressed index, and dump output uses
a deep-copied snapshot before printing outside the lock. A shallow pointer
snapshot would re-open the same rehash/lifetime UB class this section forbids.

### Zone Generation Counter — Atomic Contract (2026-05-17)

The zone `__sync_generation` counter is the canonical example of a counter
that *crosses* parallel/spawn boundaries: producer steps bump it under the
zone write-lock to invalidate dependent world frontier caches, consumer
steps read it to decide whether to re-sync.

The beta contract is:

- The counter is stored as `_Atomic uint32_t` regardless of build mode.
- The C backend uses `PGY_ZONE_GENERATION_INC` (release-order RMW) and
  `PGY_ZONE_GENERATION_LOAD` (acquire-order load) — never direct field
  access.
- The LLVM backend uses `LLVMBuildAtomicRMW(LLVMAtomicRMWBinOpAdd, ...,
  LLVMAtomicOrderingRelease)` for increment and acquire-ordered loads for
  reads — never plain load + add + store.
- `PGY_ZONE_THREADSAFE` is auto-defined for hosted builds (Linux / macOS /
  MinGW) so the rwlock that guards the rest of the zone struct is also
  active. Embedded / explicit single-threaded targets can opt out with
  `make PGY_ZONE_THREADSAFE=0`.

The atomic counter is the *minimum* fix: it removes data-race UB on the
counter and its read path, even when the rest of the zone struct relies
on the rwlock. Direct `struct->__sync_generation` access from generated
code is a regression bug; the compiler must emit the macro or the
atomic-RMW path instead. See
`src/runtime/pgy_runtime_zone_result_option_inline.h`,
`src/codegen/llvm_domain_sync_frontier.c`, and
`src/codegen/llvm_domain_world_frontier_zones.c` for the current
contract owners.

## Channel Contract

- Blocking send/receive is the stable ownership-transfer path for named
  ownership-bearing payloads.
- Non-blocking/timeout receive is copy-only for beta.
- `TrySend`, send-timeout, and status send helpers reject movable resources and
  authority-bearing tokens.
- `ChannelClose(Channel<T>)` is copy-only for beta; ownership-bearing queued
  payload channels must be drained explicitly before close.
- Channel buffering/fairness beyond current FIFO/runtime behavior is
  out-of-beta unless covered by a named backend-compare fixture.

## Cancellation Contract

- `Cancel(Future<T>)` and `Cancel(RemoteFuture<T>)` are copy-only for beta.
- Ownership-bearing future payload cancellation is explicitly rejected until
  task-boundary cleanup summaries prove where movable/anchored/subject/token
  payloads are released or observed.

## Backend Parity Evidence

`make memory-concurrency-model-test-smoke` runs:

- `make parallel-core-contract-test-smoke`
- backend compare for `parallel_channel_sum`
- backend compare for `parallel_channel_dual`
- backend compare for `triple_paradigm`

## Explicitly Out Of Beta

- Full weak-memory ordering vocabulary.
- User-selectable memory orders.
- Lock-free data structure correctness claims.
- Scheduler fairness guarantees beyond current tested fixtures.
- Anonymous async closure capture/lifetime analysis.
- Capture-bearing detached async block stability.
- Cross-thread `Arc<T>` / `Send` / `Sync` style trait system.
- Ownership-bearing non-blocking receive and cancellation payload cleanup.
