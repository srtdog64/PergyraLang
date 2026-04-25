# Async Model Positioning

Last updated: 2026-04-26

Related documents:

- `docs/113_memory_concurrency_model.md` — frozen beta contract for parallel/spawn/async/channel
- `docs/106_ownership_model_comparison.md` — sister positioning doc for ownership
- `docs/104_air_compiler_architecture.md` — AIR drift detection (sync/async constraint)
- `docs/74_slot_pinning_caching.md` — pin block boundary rules vs await/spawn/parallel

This document positions Pergyra's concurrency model relative to the
callback → promise → async/await wave history. It is a **comparison /
positioning doc**, not a contract. The frozen contract lives in `docs/113`.

## 0. Framing — Visibility Is Information, Not Just Cost

Bob Nystrom's 2015 "What Color is Your Function?" framed function coloring
as a tax on API designers and library authors. That framing is widely
quoted but only describes one side of the trade-off.

In safety-critical, deterministic-timing, or industrial-control software,
the fact that `async fn` propagates through the call graph is not a tax —
it is a **safety contract**. It tells the reader and the compiler:

- this function may yield control to another task
- a watchdog timer or interrupt may interpose between any two of its
  statements
- a mutex or spinlock held across this call may be held during a
  suspension, which is usually a bug

Hiding suspension visibility (Go goroutines without explicit yield, Loom
virtual threads, Zig's `Io` parameter to a degree) buys ergonomic
uniformity at the cost of *traceability*. In a factory equipment context
or any environment where operational debugging is more expensive than
authoring effort, that trade-off goes the wrong way.

So the real critique of async/await is not that it surfaces information
in the type. The real critique is the one in §3 below: **a single
keyword tries to express suspension, lifetime, cancellation, failure
propagation, and parallelism — and it can carry only one of them
adequately.** The rest leak out as separate apparatus
(`AbortController`, `context.Context`, `Promise.all`, `try/catch`,
structured concurrency libraries) that do not compose with each other.

Pergyra's bet is therefore not "avoid coloring." It is **decompose the
overloaded async keyword into a set of vocabularies that each express
one concern visibly and compose with the rest of the language**. The
sections below restate every wave-3 cost in those terms.

## 1. The Wave History (Summary)

The async problem space accumulated three solution waves, each fixing the
previous wave's worst symptom while introducing a new structural cost:

| Wave | Solved | Introduced |
|---|---|---|
| Callbacks (epoll/Node.js, ~2009) | Thread-per-connection resource exhaustion | Inverted control flow, fragmented error handling, no cancellation |
| Promises / Futures (ES2015, Java 8) | Nesting, error consolidation | One-shot semantics, silent error swallowing, mild type split |
| async/await (C# 2012, JS/Python/Rust) | Linear-sequence ergonomics, *plus surfaced suspension visibility in the type* | Single keyword overloaded for suspension/lifetime/cancel/failure/parallelism, ecosystem fragmentation, futurelock-class deadlocks, sequential trap |

Wave-3 deserves more credit than the Nystrom frame gives it: forcing
`async` into the signature is the part most safety-critical languages
should keep. The mistake was not adding a separate apparatus for
lifetime, cancellation, and parallelism, which forced async/await to
absorb those concerns informally.

Wave-4 candidates split along two axes: *do we keep suspension
visibility?* and *do we decompose the overloaded keyword?*

| Language | Suspension visibility | Decomposed responsibilities? |
|---|---|---|
| Go (goroutines) | Hidden (any function can yield); `context.Context` added later for cancellation | Partial — `select` for parallelism, channels for streaming, no lifetime/scope primitive until structured concurrency proposals |
| Java Project Loom | Hidden (virtual threads behave like OS threads); `StructuredTaskScope` (JEP 453) adds scoping | Yes — Loom explicitly pairs virtual threads with structured concurrency |
| Zig | Surfaced via `Io` parameter | Partial — `Io` carries the runtime, separate primitives for cancellation/error |
| Rust async | Surfaced via `async fn` | No — single keyword absorbs suspension, lifetime via `Pin`, cancellation via drop, failure via `?`, parallelism via combinators |
| **Pergyra** | **Surfaced via `effect` mask, `pin` boundary rules, and explicit `await` where present** | **Yes — `effect` / `intent` / `pin` / `parallel` / `Channel` / `Result` each express one concern** |

Pergyra is closer to Rust on the visibility axis (information stays in
the type) and closer to Loom on the decomposition axis (each concern
has its own vocabulary). It is not "wave-4 in the Go/Loom family" — it
is its own coordinate.

## 2. Restating the Wave-3 Costs Under Decomposition

### 2.1 Coloring: Real Critique vs Apparent Critique

Apparent critique (Nystrom 2015): `async fn` propagates through the
call graph; ecosystems fragment around incompatible runtimes (Tokio vs
async-std).

Real critique (when separated from the apparent one): the `async`
keyword tells the reader *only* that the function may suspend. It does
not tell the reader:

- which task scope owns the resulting work
- whether the work is cancellable, and how
- how partial failure propagates
- whether the calls inside it can run in parallel

Because `async` carries only suspension info, the other concerns get
expressed informally — through library conventions, runtime-specific
types, and combinator patterns. The ecosystem fragmentation that
Nystrom blamed on coloring is more accurately blamed on every runtime
needing to invent its own apparatus for the four missing concerns.

**Pergyra position — visibility maintained, responsibilities split:**

| Concern | Pergyra vocabulary | Visible where |
|---|---|---|
| Suspension may occur | `effect` mask (IO, network, storage, etc.) | Function signature |
| Resource cannot survive suspension | `pin slot as view: ReadView<T> { ... }` | Block boundary; rejected across `await` / `spawn` / `parallel` / `channel` |
| Work scope (lifetime) | `intent` step, `parallel { task A() }` block | Block scope; auto-join at block exit |
| Cancellation | `Cancel(Future<T>)`, `intent` `compensation_hook`, `failure_class` | Step / future definition |
| Failure classification | `Result<T>`, `failure_class: Recoverable \| Fatal \| Compensable` | Return type, intent step |
| Parallel structure | `parallel { ... }` block, intent step DAG | Block site, intent declaration |

A function does not become "async-colored" because of any one of these.
It carries an `effect` mask that lists *what kind* of suspension or
side-effect it may perform. That is more information than `async`
carries, decomposed into reviewable pieces. Adding a sensor read to a
previously pure function changes its `effect` mask, which is the same
kind of refactoring cost as Rust's `async` propagation, but the cost
buys per-effect granularity rather than a single boolean.

**Risk:** if `Future<T>` / `RemoteFuture<T>` stabilize as a primary
user surface (rather than internal vocabulary), the single-keyword
overload can still re-enter through that type. Beta closure
intentionally keeps the surface narrow and prefers `intent step` /
`parallel` / `Channel` as the user-facing vocabulary.

### 2.2 Sequential Trap

```
const user = await getUser(userId);
const orders = await getOrders(user.id);
const recommendations = await getRecommendations(user.id);
```

This sequence runs serially even though `orders` and `recommendations`
are independent. async/await's syntactic strength (looks sequential)
becomes the cognitive trap (obscures the dependency DAG).

**Pergyra position:**

- `parallel { task A(); task B(); }` makes parallelism explicit at the
  block site. The dependency structure is visible in the source.
- `intent` step DAG is the long-term answer: each step's input/output
  is declared, so AIR (Phase 2) can detect *which steps are
  independent* and report missed parallelism as a drift fact.
- Phase 1 AIR only checks sync/async drift. Independence detection is
  a Phase 2 deliverable.

**Status:** the DAG-derived independence detection is post-beta. For
beta, Pergyra users still must write `parallel { ... }` explicitly, but
the syntax does not lie about what is sequential vs concurrent.

### 2.3 Futurelock-Class Deadlocks

David Crawshaw / Adam O'Connor (Oxide) documented a new deadlock class
in async Rust: a future acquires a lock, then stops being polled while
another future tries to acquire the same lock. With OS threads the
holder always makes progress toward release. With async/await,
`select!`, `FuturesUnordered`, and buffered streams routinely stop
polling resource-holding futures.

This is the canonical case where suspension visibility is a *safety
feature*: if the language refuses programs that hold a resource across
a yield, futurelock cannot occur. Pergyra applies that rule
structurally:

- `pin slot as view: ReadView<T> { ... }` blocks reject crossing
  `await`, `spawn`, `async`, `parallel`, callback, and channel
  boundaries. (`PGY_SEM_PIN_AWAIT_BOUNDARY`,
  `PGY_SEM_PIN_PARALLEL_CONFLICT`.)
- `Token<T>` transport is rejected on channel send, spawn boundary,
  and cancellation payload.
- Cleanup edges are inserted on early return, panic, and branch joins.
  A pinned view cannot survive a stopped polling site because it cannot
  cross the boundary in the first place.

This is a structural rejection at compile time, not a runtime
detection. Rust async catches futurelock with core dumps and a
disassembler; Pergyra refuses the program because the suspension
boundary is visible at the type level (not because it hides
suspension).

### 2.4 Cancellation

- Callbacks: no cancellation primitive.
- Promises: no cancellation in the original ES2015 spec; AbortController
  added later as a separate concept.
- Go: `context.Context` introduced after the language shipped, propagating
  through every function as a parameter (a soft form of coloring layered
  onto a language that started by avoiding it).

**Pergyra position:**

- `Cancel(Future<T>)` and `Cancel(RemoteFuture<T>)` are language builtins
  (currently copy-only payload for beta).
- `intent` declarations carry `compensation_hook` and a
  `failure_class: Recoverable | Fatal | Compensable`. Rollback is part
  of the step definition.
- Cancellation is a property of an intent step or a future, not a
  parameter threaded through every function. There is no
  `context.Context` viral propagation.

**Status:** ownership-bearing cancellation payloads are explicitly
out-of-beta. Beta covers copy-only payload cancellation.

## 3. The Real Critique — Single Keyword, Four Responsibilities

The four costs in §2 share a root cause: `async` was used as the only
visible vocabulary for several different concerns. The table below
makes the overload explicit.

| Concern | What async/await carries | What it cannot carry |
|---|---|---|
| Suspension may occur | `async fn` signature | (handled — this is the part wave-3 got right) |
| Work scope / lifetime | Future ownership held by caller | Structured task scope; bolted on later (Trio, JEP 453, Swift TaskGroup) |
| Cancellation | None native | `AbortController`, `context.Context`, drop-on-await — all separate |
| Failure classification | `Result<T>` + `?` | Recoverable / Fatal / Compensable distinction; partial-failure semantics |
| Parallel structure | None native | `Promise.all`, `join!`, `select!` — combinators, not first-class structure |

Languages that bolted on the missing pieces (Python's `asyncio.gather`,
Rust's `tokio::join!`, JavaScript's `Promise.all`) ended up with
incompatible apparatus per runtime. The ecosystem split is the
*symptom*; the cause is that one keyword cannot express five concerns,
so each runtime invented four of them.

Decomposition is the structural fix. Pergyra's bet is that giving each
concern its own visible primitive — `effect`, `intent`, `pin`,
`parallel`, `Channel`, `Result` — produces a system where:

- adding a new sensor read changes the `effect` mask, not the
  function color
- a held resource is rejected across yield by `pin`, not detected
  with a disassembler
- a cancellable operation declares `Cancel` and `compensation_hook`,
  not threads `context.Context`
- partial failure is `failure_class`, not informal convention
- parallelism is a `parallel` block site, not a combinator pattern

If decomposition succeeds, Pergyra does not need a "wave-5" — wave-3
already gave the right answer for visibility, and decomposition gives
the right answer for the rest.

## 4. One-Shot vs Streaming Split

JavaScript ended up with three concurrency vocabularies:

- `Promise<T>` for one-shot async values
- `EventEmitter` / `AsyncIterator` for streams
- `Observable` (RxJS) for reactive streams

These are different APIs by different authors and do not compose.

**Pergyra position:**

- One-shot: `Future<T>`, `intent` step result.
- Streaming: `Channel<T>` with frozen FIFO/runtime behavior, blocking
  send/receive for ownership-bearing payloads, copy-only non-blocking
  receive, explicit close contract.

Both surfaces live in the same language and share the ownership
classifier (no separate "stream library"). Stream backpressure beyond
current FIFO/runtime behavior is out-of-beta.

## 5. Where Pergyra Sits — Two-Axis Map

Instead of a single wave number, Pergyra is more accurately placed on a
two-axis map: *visibility* (does suspension show up in the type?) and
*decomposition* (is the overloaded async keyword split into per-concern
vocabularies?).

| Language | Visibility | Decomposition | Coordinate |
|---|---|---|---|
| Callbacks (Node.js) | Hidden (callback registration is the only signal) | Single concern (just I/O) | Visibility-low / decomp-NA |
| Promises | Partial (`Promise<T>` return type) | None | Visibility-mid / decomp-low |
| async/await (Rust, JS, Py, C#) | Yes | None — single keyword | Visibility-high / decomp-low |
| Go goroutines | Hidden | Partial — channel + select + later context | Visibility-low / decomp-mid |
| Java Loom | Hidden | Yes — virtual threads + StructuredTaskScope | Visibility-low / decomp-high |
| Zig | Yes (via `Io` parameter) | Partial | Visibility-mid / decomp-mid |
| **Pergyra** | **Yes — via `effect` mask, `pin` boundary rules, explicit `await` surfaces** | **Yes — `effect` / `intent` / `pin` / `parallel` / `Channel` / `Result` each carry one concern** | **Visibility-high / decomp-high** |

The visibility-high / decomp-high quadrant is largely unoccupied in
current production languages. Loom is closest on decomposition but
hides visibility; Rust is closest on visibility but does not decompose.
Pergyra's bet is that this quadrant is the right one for the workloads
it targets — distributed business systems, transactional domain
modeling, AI orchestration, games, and (per beta-user feedback)
industrial-control / safety-critical software where suspension
visibility is a safety contract.

## 6. Honest Weaknesses

These are recorded for beta closure decisions and for users evaluating
whether Pergyra fits their workload.

1. **Fiber runtime maturity.** Pergyra is committed to fiber-based
   concurrency, but the scheduler / work-stealing / IO integration is
   not at Tokio or Loom maturity. C10K-class load regression does not
   exist yet.

2. **`Future<T>` / `RemoteFuture<T>` user surface is narrow but not
   pinned.** These are reserved as language vocabulary. If they
   stabilize as a primary user-visible type rather than internal
   vocabulary, the single-keyword overload (§3) can re-enter through
   the back door. Beta closure must decide on the user surface
   explicitly.

3. **`await` is referenced in pin rules but the user surface is
   under-specified.** The intent is that `intent` step boundaries
   replace user-visible `await`, but the boundary between "implicit
   await at step boundary" and "explicit `await Future<T>`" is not
   closed in beta documentation.

4. **`spawn async () { ... }` is parser-reject, not designed.**
   Anonymous async spawn capture/lifetime analysis is explicit
   out-of-beta. The stable surface is named `spawn Worker(args...)`.
   The anonymous-closure surface needs design before any post-beta lift.

5. **AIR Phase 2 (independence detection) is post-beta.** Phase 1 only
   detects sync/async drift between intent constraint and boundary.
   Detecting "these two intent steps are independent and could run in
   parallel" requires constraint/effect node implementation.

6. **No `Send` / `Sync` analogue — visibility moved into channel
   contract.** Cross-thread / cross-World ownership safety is expressed
   through Channel-only transfer, ownership classifier, and `Token<T>`
   transport rejection. There is no per-type `Send` / `Sync` marker
   trait. This is a deliberate decomposition choice, not an absence:
   the visibility moved from a per-type marker to a per-boundary
   contract. Cross-boundary safety is checked when a value crosses a
   channel send / spawn / parallel task / world handoff, not when the
   type is declared.

7. **Industrial / real-time exposure of preemption points is
   undocumented.** Suspension visibility at the source level (effect
   mask, pin boundary) does not yet imply visibility of the runtime's
   actual preemption points (where the fiber scheduler may yield, how
   watchdog timers interact, whether interrupts are masked across an
   `effect` boundary). For factory-equipment, real-time, or
   safety-critical workloads, the runtime side of the contract needs
   its own document. This is a beta-closure documentation gap, not a
   design gap.

## 7. Beta Promise

The frozen beta promise (cross-reference `docs/113`) is narrow:

- `parallel` and named `spawn` are stable execution primitives.
- `async` / `await` is stable for copy-only values and checked futures.
- Anonymous async spawn bodies are parser-rejected.
- Ownership-bearing channel transfer uses blocking send / receive only.
- Cancellation is copy-only payload.
- Cross-thread `Arc<T>` / `Send` / `Sync` is explicit out-of-beta.
- AIR Phase 1 detects sync/async drift; independence detection is
  post-beta.

This document records *why* those choices were made: each one closes
one of the four wave-3 costs, and shipping a wider surface in beta
would risk re-introducing the single-keyword overload (§3) those
choices were meant to prevent.

## 8. Summary

Pergyra's concurrency model is not "avoid coloring." Suspension
visibility is information that safety-critical and industrial users
need, and Pergyra preserves it via `effect`, `pin`, and explicit
boundary rules.

What Pergyra avoids is the **single-keyword overload**: async/await
expressing suspension, lifetime, cancellation, failure propagation,
and parallelism through one mark, then leaking the missing four into
incompatible per-runtime apparatus.

The decomposition is:

- `effect` mask — what kinds of suspension / side-effect occur
- `pin` block — what cannot survive a yield
- `intent` step + `parallel` block — work scope and lifetime
- `Cancel` / `compensation_hook` / `failure_class` — cancellation and
  partial failure
- `Channel<T>` — streaming transport with explicit ownership rules
- `Result<T>` — failure as data

The risk is self-inflicted: `Future<T>` / `RemoteFuture<T>` /
user-visible `await` could re-introduce the single-keyword overload if
their surface is pushed wider than beta needs. Beta closure
intentionally keeps that surface narrow.

The reward is that for the workloads Pergyra targets — distributed
business systems, transactional domain modeling, AI orchestration,
games, and industrial-control software — the user keeps wave-3's
visibility win and does not pay the bolt-on tax that callbacks,
promises, and async/await accumulated over fifteen years.
