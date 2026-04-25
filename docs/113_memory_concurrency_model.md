# Memory And Concurrency Model Beta Contract

Status: beta-freeze-source-of-truth.

This document freezes the beta memory/concurrency contract. The goal is not a
full formal memory model; the beta promise is a narrow, executable contract for
`parallel`, named `spawn`, `async`/`await`, channels, cancellation, and
ownership-bearing payload boundaries.

For *why* this contract is shaped the way it is (positioning vs callback /
promise / async-await waves, function coloring, sequential trap,
futurelock-class deadlocks), see
`docs/114_async_model_positioning.md`. This file is the contract; that file
is the rationale.

Executable gate: `make memory-concurrency-model-test-smoke`.

## Stable Execution Surface

- `parallel` is the core execution primitive.
- Named `spawn Worker(args...)` is beta-stable when the callee declaration
  exposes parameter, effect, and ownership facts.
- `async`/`await` is beta-stable for copy-only values and checked futures.
- `select` and `channel` are beta-stable for the currently implemented typed
  channel families and copy-only non-blocking receive surface.
- Anonymous async spawn bodies are explicitly rejected for beta.

## Happens-Before Contract

- A `parallel { ... }` block joins before control continues after the block.
- Writes that are accepted by semantic analysis inside a `parallel` task become
  visible after the join.
- Shared `ref`/`ref` reads of the same ownership-bearing value across parallel
  tasks are accepted.
- `ref`/`own` and `own`/`own` task-boundary conflicts are rejected.
- `WriteView<T>` and pinned view conflicts are rejected across parallel tasks.
- No data-race freedom is promised for `unsafe` or out-of-beta surfaces.

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
- Cross-thread `Arc<T>` / `Send` / `Sync` style trait system.
- Ownership-bearing non-blocking receive and cancellation payload cleanup.
