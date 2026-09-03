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

Executable gates: `make memory-concurrency-model-test-smoke` and
`make structured-spawn-lifecycle-test-smoke`.

## Stable Execution Surface

- `parallel` is the core execution primitive.
- Named `spawn Worker(args...)` is beta-stable when the callee declaration
  exposes parameter, effect, and ownership facts. Its completion handle is an
  affine lexical obligation: bind it directly or await it immediately, then
  retire it on every normal path before leaving the owning scope.
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

## Spawn Runtime Authority Contract

- Task creation snapshots the currently bound `PgyRuntimeContext` capability
  masks. Inline, pinned, blocking-pool, local-async, worker-pool, and movable
  scheduler execution must bind that snapshot before calling the task body;
  executor-thread default TLS is not an authority fallback.
- A task does not receive an independent quantitative allowance. Its context
  points to the exact parent-owned `PgyBudgetState`, so charges from children,
  nested help-first execution, and movable tasks contribute to one ceiling.
- A runtime task path must not reread environment grants, initialize a fresh
  budget, or widen the captured manifest merely because execution moved to a
  different thread or fiber.
- Every execution boundary restores the surrounding context after the task
  returns. Local coroutines additionally restore scheduler context before
  `yield`/`await` switches and rebind their own captured context when resumed.
- Authority carriage and structured containment have separate owners. Runtime
  task records carry the captured context; semantic Future lifecycle flow now
  proves that a named spawn is joined or explicitly transferred before its
  lexical owner exits. The runtime does not invent an implicit drain fallback.

Implementation checkpoint: `src/runtime/pgy_runtime_context.h` owns task
capture and the shared budget-state reference. `PgyTask` and `PgyCoroTask` are
carriers only. `make runtime-spawn-context-propagation-test-smoke` executes the
inline, C-extern, and LLVM runtime materializations across every lane and pins
nested help-run plus coroutine suspension restoration.

## Happens-Before Contract

- A `parallel { ... }` block joins before control continues after the block.
- Writes that are accepted by semantic analysis inside a `parallel` task become
  visible after the join.
- Shared `ref`/`ref` reads of the same ownership-bearing value across parallel
  tasks are accepted.
- `ref`/`own` and `own`/`own` task-boundary conflicts are rejected.
- Slot read/write and write/write overlaps across sibling `parallel` tasks are
  rejected by the boundary-witness `op_guard` refinement. Shared read/read
  overlap remains accepted. In short: read/write overlap is a semantic error.
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

Implementation checkpoint: `src/common/worker_boundary_storage_policy.{h,c}`
owns the list of growable or synchronization-backed storage that cannot cross
worker boundaries by raw pointer. Semantic analysis, C lowering, and LLVM
lowering consume that shared policy instead of rebuilding local lists.
`make worker-boundary-ub-test-smoke` pins the owner and requires semantic
regressions for Array, Slice, HashMap, and Channel worker-boundary rejection.

Refinement (2026-07-09, docs/178): evidence-carrying crossings are admitted
through that same reject — a construction-guaranteed disjoint Slice split
pair (Disjointness evidence, `parallel-disjoint-test-smoke`) and pre-parallel
snapshot copies of single-writer primitive scalars (Copy evidence,
`parallel-snapshot-test-smoke`). Slice views are fixed `{data,len}` spans, so
the growable/rehash reason above does not apply to them; the parallel capture
emitters therefore trust the semantic admission for slices while every
evidence-free crossing keeps failing closed exactly as this section requires.
Async blocks have no admission path and keep the full reject.

Implementation checkpoint: `src/semantic/boundary_witness.{h,c}` records the
C semantic checker's `OpAcqR`/`OpAcqW`/`OpRel` decisions in
`PgyBoundaryWitnessSummary`, using the same shape as
`docs/semantics/proofs/WitnessDataRace.v`. `type_checker_flow_parallel.c`
feeds it from `ResourceConsumeSnapshot` deltas, and read/write overlap is a
semantic error, not a warning. `test_semantic_parallel_context.cases.h` pins
the op_guard oracle and the generated C-checker witness counters.

Implementation checkpoint: `src/runtime/party_runtime_stats.c` treats the
process-global fiber stats table as a shared registry. `UpdateFiberStats`,
`GetFiberStats`, and `party_runtime_dump_fiber_stats` all acquire the same
registry mutex before touching the open-addressed index, and dump output uses
a deep-copied snapshot before printing outside the lock. A shallow pointer
snapshot would re-open the same rehash/lifetime UB class this section forbids.
`GetFiberStats` returns the numeric counters by value, but its `roleId` field
is a registry-owned borrowed string; callers must not free or cache it as an
independent allocation. Use `party_runtime_dump_fiber_stats` or an explicit
copy when a durable cross-thread stats snapshot is needed.

Implementation checkpoint: `src/runtime/party_runtime_scheduler.c` treats the
process-global scheduler registry as mutex-owned state. `RegisterScheduler`,
`GetSchedulerForTag`, and `DumpFiberMaps` acquire `g_schedulerRegistryMutex`
before reading or mutating `g_schedulerRegistry`, `g_schedulerByTag`, or
`g_schedulerCount`. A direct worker-id or tag lookup without that lock would
re-open the shared-cache race this section forbids.

Implementation checkpoint: `src/runtime/async/concurrent_queue.c` treats queue
node links as mutex-owned FIFO state and exposes the queue size as an atomic
observation only. Size updates use compare-exchange RMW loops with saturation
instead of load-plus-store arithmetic, so future worker paths cannot wrap or
lose count updates if the queue implementation becomes less strictly
mutex-backed. Queue payload pointers are non-null by contract because
`ConcurrentQueuePop` uses `NULL` as the empty/failure sentinel; accepting a
`NULL` item would make a queued task indistinguishable from an empty queue.

World/roster async execution is a borrowed-handle surface for beta.
`ExecuteRosterAsync` does not deep-copy `RosterContext`, `DispatcherConfig`,
`PartyContext`, or `FiberMap` graphs. Those objects must remain immutable and
live until `WaitForRoster` returns a completed result. A timeout from
`WaitForRoster` does not consume or free the handle; the caller must call
`WaitForRoster` again later to join and release it. This keeps the runtime from
pretending that a shallow graph snapshot is safe across worker threads.

Party dispatch follows the same rule. `DispatchParallel` borrows the
`FiberMap` and `PartyContext` graph for the duration of the dispatch, so callers
must not mutate or free that graph until all worker threads have joined. The
returned `DispatchResult` owns its role-id strings and must be released with
`FreeDispatchResult`; this keeps generated `FiberMap` convenience paths from
returning pointers into a freed dispatch graph.

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
- `ChannelDestroy(Channel<T>)` is quiescent-only. It may free the backing
  buffer and destroy mutex/condvar state, so all producers, consumers, and
  waiters must have stopped or joined before destroy. Use `ChannelClose` to
  publish shutdown; use destroy only after the join/drain boundary.
- A condition-variable wait failure is not a normal wakeup. Channel send/recv
  owners must warn, unlock, and return failure instead of continuing with a
  possibly-invalid wait state.
- Channel buffering/fairness beyond current FIFO/runtime behavior is
  out-of-beta unless covered by a named backend-compare fixture.

## Future Await Contract

- `await` is a completion join, but the named `Future<T>` or
  `RemoteFuture<T>` handle is consumed by the join. The runtime frees the task
  handle on await, so the semantic checker marks a named awaited handle
  consumed and rejects double-await and use after an affine transfer.
- A named `Future<T>` or `RemoteFuture<T>` must be retired on every normal path
  before its lexical scope or function exits. Retirement is either `await` or
  transfer to an explicit `own Future<T>`/`own RemoteFuture<T>` parameter; the
  receiving function inherits the same obligation.
- `spawn` is admitted only as the direct initializer of an immutable binding
  or as the direct operand of `await`. Bare spawn expressions, mutable Future
  bindings, Future-to-Future `let` aliases, borrowed/default Future parameters,
  Future return boundaries, and branch-dependent live/retired state are
  semantic errors.
- This rule does not make `async` a lifetime structure and does not add a
  hidden finalizer. The affine Future binding and lexical flow fact own the
  obligation; `parallel` retains its own block join contract.
- Inline `await spawn Worker(args...)` is valid because the temporary handle is
  consumed immediately and never creates a reusable binding.
- A task condition-variable wait failure is an internal invariant violation;
  `await` must not continue as if the task completed normally.

Implementation checkpoint: `src/semantic/type_checker_future_lifecycle.c`
owns admission, retirement, transfer, and scope-exit rejection.
`ResourceConsumeSnapshot` carries the state across if/match/loop/parallel flow;
alternative paths must agree, while a structured parallel join combines the
executed task states. Static boolean branches, provably non-empty literal
ranges including their final `continue`, `while true`, and literal match
selection exclude impossible paths, unreachable returns and loop exits, and
exact zero-iteration loop bodies;
unknown conditions remain conservative. `make
structured-spawn-lifecycle-test-smoke` pins nineteen positive and eighteen
fail-closed production fixtures across both C and LLVM semantic entrypoints,
exact transfer execution output, the stable `PGY_SEM_TASK_LIFECYCLE`,
single-owner diagnostics without type-mismatch cascades, move-after-transfer,
and parallel double-consume diagnostics.

## Cancellation Contract

- `Cancel(Future<T>)` and `Cancel(RemoteFuture<T>)` are copy-only for beta.
- `Cancel(...)` requests cooperative cancellation; it neither joins nor frees
  the completion handle. The caller must still `await` it or transfer it to an
  explicit `own Future` parameter before leaving the owning scope.
- Ownership-bearing future payload cancellation is explicitly rejected until
  task-boundary cleanup summaries prove where movable/anchored/subject/token
  payloads are released or observed.

## Backend Parity Evidence

`make memory-concurrency-model-test-smoke` runs:

- `make parallel-core-contract-test-smoke`
- backend compare for `parallel_channel_sum`
- backend compare for `parallel_channel_dual`
- backend compare for `triple_paradigm`
- the structured spawn lifecycle gate for immutable ownership, join/transfer,
  and negative exit-path cases

## Explicitly Out Of Beta

- Full weak-memory ordering vocabulary.
- User-selectable memory orders.
- Lock-free data structure correctness claims.
- Scheduler fairness guarantees beyond current tested fixtures.
- Anonymous async closure capture/lifetime analysis.
- Capture-bearing detached async block stability.
- Cross-thread `Arc<T>` / `Send` / `Sync` style trait system.
- Ownership-bearing non-blocking receive and cancellation payload cleanup.
