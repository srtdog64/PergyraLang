# Async Model Positioning

Last updated: 2026-04-26

Related documents:

- `docs/19_design_philosophy.md` §0 - **core identity** (systems language baseline; concurrency model is one of the layers above it).
- `docs/113_memory_concurrency_model.md` - frozen beta contract for
  `parallel`, named `spawn`, `async`/`await`, channels, cancellation, and
  ownership-bearing boundaries.
- `docs/05_async_concurrency.md` - user-facing beta surface guide.
- `docs/106_ownership_model_comparison.md` - sister positioning doc for
  ownership.
- `docs/104_air_compiler_architecture.md` - AIR drift detection and future
  dependency analysis.
- `docs/74_slot_pinning_caching.md` - pin block boundary rules vs
  `await` / `spawn` / `parallel`.
- `docs/117_backend_strategy_positioning.md` - sister positioning doc for
  backend strategy (LLVM + C dual-emit, abstraction portability).
- `docs/118_slot_model_rigor_audit.md` - sister audit doc; Slot vs
  borrow-check rigor and marketing-language guide.
- `docs/119_pergyra_lineage_positioning.md` - sister positioning doc for
  language lineage (C# father, Tier 1-5 substrate borrow, DDD unique
  synthesis).
- `docs/120_vision_and_capability_audit.md` - sister audit; capability
  negative-space + current-vs-vision separation.

This document positions Pergyra's concurrency model relative to the
callback -> promise/future -> async/await history. It is a positioning and
rationale document, not the frozen contract. The frozen contract is
`docs/113_memory_concurrency_model.md`.

## 0. Thesis: Coloring Is Decomposed, Not Hidden

The failure mode of mainstream async/await is not that it makes suspension
visible. For many workloads, visibility is the part worth keeping.

In safety-critical, deterministic-timing, industrial-control, game-loop, and
operationally observable distributed systems, a suspension point is not just
an implementation detail. It tells the reader and the compiler:

- another task may run between two source statements,
- a watchdog, interrupt, or cancellation check may interpose,
- a lock, slot view, token, or authority capability may be held across a
  yield, which is often a bug,
- a failure can arrive from a boundary that is not local to the current stack.

The actual async/await problem is that a single marker grew to carry too many
unrelated meanings:

- suspension,
- lifetime and task ownership,
- cancellation,
- fallible remote completion,
- parallel structure,
- resource crossing rules.

Pergyra's design is therefore not "avoid coloring". It is
**coloring decomposition**: keep the useful visibility, then split the overloaded
responsibilities into separate language vocabularies.

## 1. What Each Vocabulary Owns

Pergyra should not let `async` become the umbrella word for all concurrent
behavior. Each concern has an owner.

| Concern | Beta vocabulary | Visible where |
|---|---|---|
| Suspension or side effect may occur | `effect` mask | Function/action signature |
| Work scope and lifetime | `parallel { ... }`, named `spawn Worker(...)`, intent step | Block or intent declaration |
| Completion join | `await Future<T>` / `await RemoteFuture<T>` | Join expression |
| Fallible remote completion | `RemoteFuture<T> -> await -> Result<T>` | Type of the joined value |
| Cancellation | `Cancel(Future<T>)`, `Cancel(RemoteFuture<T>)`, `IsCancelled()`, intent compensation | Future or intent step |
| Resource cannot cross suspension | `pin slot as view { ... }`, `ReadView<T>`, `WriteView<T>` | Block boundary and diagnostics |
| Streaming transport | `Channel<T>`, `select` | Channel boundary |
| Partial failure as data | `Result<T>` | Return type or step result |
| Runtime drift evidence | AIR boundary node | Compiler IR and diagnostics |

This is the core design rule: Widening one cell must not silently widen the others.
For example, adding an I/O operation changes an `effect` mask; it does not imply
ownership transfer, cancellation semantics, or parallel execution.

## 2. Why Function Coloring Was Misdiagnosed

Bob Nystrom's "What Color is Your Function?" made the API propagation cost of
async functions visible. That cost is real, but it is not the whole story.
Coloring also carries valuable information: the call may suspend.

The costly part is not "the type tells me something." The costly part is
"the type tells me only one bit, while the runtime still needs five separate
contracts." Mainstream ecosystems then add the missing contracts as separate
libraries:

- JavaScript: `AbortController`, `Promise.all`, event streams, Rx libraries.
- Go: `context.Context`, channels, `select`, ad-hoc structured concurrency.
- Rust: `Pin`, executor-specific spawn handles, `join!`, `select!`,
  cancellation-by-drop conventions.
- Java Loom: virtual threads plus `StructuredTaskScope`.

Pergyra's bet is that the language should own these contracts directly rather
than letting each runtime or library reinvent them.

## 3. Sequential Trap

The classic trap is a source sequence that looks natural but runs
unnecessarily serially:

```pergyra
let user = await GetUser(id);
let orders = await GetOrders(user.id);
let recommendations = await GetRecommendations(user.id);
```

If `orders` and `recommendations` are independent, the source shape hides a
dependency DAG. Pergyra's beta answer is explicit named task creation plus
explicit joins:

```pergyra
let ordersTask: Future<OrderList> = spawn GetOrders(user.id);
let recommendationsTask: Future<Recommendations> =
    spawn GetRecommendations(user.id);

let orders: OrderList = await ordersTask;
let recommendations: Recommendations = await recommendationsTask;
```

Long term, AIR should detect independent intent steps and report missed
parallelism as a drift fact. That is AIR Phase 2. Beta does not promise
automatic independence detection; it promises that explicit `parallel`, named
`spawn`, `await`, and intent step boundaries are visible and checked.

## 4. Futurelock-Class Deadlocks

Async Rust exposed a deadlock class where a future holds a resource, stops
being polled, and another future waits on that same resource. The compiler can
see the suspension boundary but ordinary async/await does not know which
resources must not cross it.

Pergyra keeps the suspension visible and gives the resource rule a separate
owner:

- `pin slot as view: ReadView<T> { ... }` and `WriteView<T>` cannot cross
  `await`, `spawn`, `parallel`, channel, callback, or async boundaries.
- `Token<T>` transport is rejected across channel send, spawn, and
  cancellation payload boundaries.
- Early-return and cleanup paths are CFG responsibilities, not informal
  runtime conventions.

This is compile-time structural rejection, not post-mortem runtime debugging.

## 5. Cancellation

Cancellation is not hidden inside `async`. It is a distinct contract:

- `Cancel(Future<T>)` and `Cancel(RemoteFuture<T>)` request cooperative
  cancellation.
- `IsCancelled()` observes cancellation inside the current task.
- Spawned async descendants inherit the cancellation chain in the current
  runtime model.
- Intent steps can carry compensation and failure classification.

Beta intentionally keeps cancellation payloads copy-only. Ownership-bearing
payload cancellation is rejected until task-boundary cleanup summaries prove
where movable, anchored, subject, and token payloads are released or observed.

## 6. One-Shot vs Streaming

Mainstream ecosystems often split one-shot async values and streams into
unrelated APIs. Pergyra keeps the split explicit but in one language model:

- One-shot completion: `Future<T>`, `RemoteFuture<T>`, intent step result.
- Streaming transport: `Channel<T>` and `select`.
- Fallible remote completion: `RemoteFuture<T> -> await -> Result<T>`.
- Copy-only non-blocking channel helpers: `TryRecv`, `RecvTimeout`,
  `TrySend`, `SendTimeout`, status helpers.
- Ownership-bearing channel transfer: blocking send/receive only for beta.

The ownership classifier is shared by both one-shot and streaming surfaces.
There is no separate stream library with separate safety rules.

## 7. Two-Axis Position

Pergyra is best described with two axes:

- Visibility: does suspension show up in the program contract?
- Decomposition: are lifetime, cancellation, failure, and parallelism split
  into separate vocabularies?

| Language family | Visibility | Decomposition |
|---|---|---|
| Callbacks | Low | Low |
| Promises / futures | Medium | Low |
| mainstream async/await | High | Low |
| Go goroutines | Low | Medium |
| Java Loom | Low | High |
| Rust async | High | Medium-low |
| Pergyra | High | High |

This puts Pergyra in the visibility-high / decomposition-high quadrant. That
quadrant is the reason the beta surface must stay narrow: if `Future<T>` and
`await` become the primary user model for everything, Pergyra falls back into
the mainstream async/await failure mode.

## 8. Beta Surface Rule

The beta rule is:

`parallel` is the core execution primitive. Named `spawn` is the task-producing
surface. `await` is a completion join for checked futures. Future<T> and
RemoteFuture<T> are typed completion handles, not a general user-level effect
system.

Future<T> and RemoteFuture<T> are typed completion handles.
They are not a general user-level effect system.

Stable beta:

For beta, await is a completion join for checked futures.

- `parallel { ... }` with join-before-continuation semantics.
- Named `spawn Worker(args...)`.
- `await Future<T> -> T`.
- `await RemoteFuture<T> -> Result<T>`.
- `Cancel(Future<T>)` / `Cancel(RemoteFuture<T>)` for copy-only payloads.
- `Channel<T>` and `select` for the currently implemented typed channel
  families.
- Pin/view diagnostics across `await`, `spawn`, `parallel`, and channel
  boundaries.
- AIR Phase 1 sync/async drift detection.

Rejected or out-of-beta:

- Anonymous async spawn bodies as stable syntax.
- Capture-bearing detached async blocks as the stable task creation model.
- Ownership-bearing cancellation payloads.
- Ownership-bearing non-blocking channel receive.
- User-selectable scheduler or memory-order vocabulary.
- `Send` / `Sync` style marker trait system.
- AIR Phase 2 automatic independence detection.

## 9. Honest Weaknesses

These are not reasons to widen `async`; they are closure work items.

1. Fiber runtime maturity is below Tokio/Loom-class production runtimes.
   Scheduler fairness, work stealing, and I/O integration need stronger
   regression baselines.
2. Actual runtime preemption points are not yet documented with enough
   precision for hard real-time claims.
3. AIR Phase 2 independence detection is still future work.
4. Cancellation is cooperative, not preemptive.
5. `Future<T>` / `RemoteFuture<T>` must remain constrained handles, or the
   single-keyword overload can re-enter through the back door.

## 10. Summary

Pergyra's concurrency model is not hiding suspension. It is
**coloring decomposition**.

The model keeps the important visibility of suspension and side effects, then
assigns the missing contracts to specific language surfaces:

- `effect` for side-effect and suspension class,
- `pin` / `ReadView<T>` / `WriteView<T>` for resource lifetime across yields,
- `parallel` and named `spawn` for work scope,
- `await` for completion join only,
- `Cancel` and intent compensation for cancellation,
- `Result<T>` for failure as data,
- `Channel<T>` for streaming transport,
- AIR for compiler-visible boundary drift.

That decomposition is why the beta surface is intentionally narrow. A wider
`async` surface would not make the language more complete; it would collapse
separate contracts back into one overloaded keyword.
