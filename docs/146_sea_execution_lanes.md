# 146. SEA — Structured Effect Async, BoundaryCaptureFact, and ExecutionLane

Status: design + BoundaryCaptureFact/ExecutionLane first slices landed
(2026-06-29). Sister doc to
`docs/114_async_model_positioning.md` (the async positioning) — this one names
the execution layer below it.

## 0. The question Pergyra actually asks

Not "how many tasks on how many OS threads" (M:N vs 1:1). That is a runtime
strategy, below the meaning line, and `docs/114 §8` already excludes a
user-selectable scheduler from the language. The Pergyra question is:

> **which executions can move, carried by what evidence, and which must stay
> pinned?**

M:N is therefore not the centre of the language. It is one runtime lane, used
**only when the evidence permits it**. Putting M:N at the centre would be the
same category error as letting `async` mean five things at once — it conflates
*scheduling cost* with *isolation strength* and with *movability safety*.

## 1. Three layers — do not collapse them

| Layer | Name | What it is | Where |
|---|---|---|---|
| Semantic model / contract | **SEA** (Structured Effect Async) | execution boundaries are decided by intent/effect/authority/coordination evidence | docs/114, this doc |
| Compiler input (IR fact) | **BoundaryCaptureFact** | the per-boundary capture/movability facts that lane classification consumes | `src/compiler/execution_lane.{h,c}`, stored on `AIRBoundaryNode` |
| Compiler decision (IR fact) | **ExecutionLaneFact** | a per-task lane chosen from `BoundaryCaptureFact` | `src/compiler/execution_lane.{h,c}`, attached to AIR |
| Runtime implementation | **PgyLaneScheduler** | facade that dispatches a lane to a concrete executor | `src/runtime/` (skeleton) |

SEA is the philosophy/contract; it must never be used as the *name of a
scheduler algorithm*. Work-stealing, worker pool, local coroutine are SEA's
sub-strategies, not SEA itself.

## 2. The lanes

```
ExecutionLane =
  Inline            // no concurrency need — run in place
  PinnedZone        // pin/slot/live-view present: bound to its owner/zone
  BlockingPool      // IO / FFI / OS blocking: its own lane
  LocalAsync        // await-heavy + local state: cooperative, same owner
  WorkerPool        // deterministic fork-join / pure value: bounded pool
  MovableScheduler  // the ONLY M:N lane: pure value, no pin/raw, authority clear
  Reject            // fail-closed: a pinned/raw resource asked to move
```

The runtime executors that consume them: `LocalCoroutineExecutor`,
`WorkerPoolExecutor`, `BlockingExecutor`, `MovableExecutor`, and a pinned
executor for `PinnedZone`.

## 3. The decision table (the contract)

`pgy_classify_execution_lane(boundary_capture)` is pure, total, ordered,
fail-closed.
The ORDER is the contract: a pinned resource is decided before an effect, which
is decided before a movability optimisation, so the same evidence always yields
the same lane regardless of runtime.

| Evidence (first match wins) | Lane |
|---|---|
| not a concurrency site | `Inline` |
| pin/live-view **or** raw Slot/Channel capture, **and** requires movability | **`Reject`** |
| pin/live-view **or** raw Slot/Channel capture | `PinnedZone` |
| IO / FFI / OS-blocking effect | `BlockingPool` |
| await-heavy, only local state | `LocalAsync` |
| deterministic fork-join | `WorkerPool` |
| pure-value capture **and** authority boundary clear **and** no raw capture | **`MovableScheduler`** |
| pure-value capture | `WorkerPool` |
| concurrent, no movability evidence | `LocalAsync` (conservative) |

Two load-bearing edges:
- **Reject** — M:N is not silently downgraded when a pinned resource is asked to
  move; the contradiction is a compile-time error (§1.1 no hidden flow).
- **MovableScheduler** is the strictest gate. M:N is an *evidence-gated
  optimisation*, never a default.

## 4. Why this is the Pergyra-shaped answer

The classifier reads only facts the compiler already computes — pin/slot
presence, raw-vs-value capture (Stage A copy-capture), effect mask (IO/FFI),
authority boundary, async shape. So the lane is *derived from the same evidence*
that already governs safety; it is not a new tuning knob. M:N stops being "the
right answer" and becomes "the lane the evidence unlocked". That is the
intent/effect/authority centre of Pergyra deciding execution, with the scheduler
strictly underneath.

## 5. Status / remaining

**Landed — the facts flow end to end, gated and golden-tested:**
- `BoundaryCaptureFact` is the input SoT for lane selection. It records
  `captures_pin`, `captures_live_view`, `captures_raw_slot`,
  `captures_raw_channel`, `captures_value_only`,
  `crosses_authority_boundary`, and `requires_movability`, plus effect/shape
  facts needed by the decision table.
- `ExecutionLaneFact` is the output SoT. The pure classification policy has a
  decision-table proof covering every lane and both load-bearing edges
  (`execution-lane-policy-test-smoke`, 10/10).
- `boundary_capture` and `execution_lane` live on `AIRBoundaryNode`, are
  finalized in ONE pass in `air_synthesize`, and are emitted in `--air-json`.
- A golden test (`sea-execution-lane-golden-test-smoke`) that compiles a real
  program, synthesises AIR, and pins the per-boundary lanes
  (`zone -> PinnedZone`, `world -> LocalAsync`), with a regression guard that a
  classified boundary is never left at the fail-closed zero (`Reject`). The AIR
  JSON schema smoke also requires the `boundary_capture` object.

**Landed — runtime facade + self-host mirror (2026-06-27):**
- `PgyLaneScheduler` (`src/runtime/pgy_lane_scheduler.c`) consumes the fact:
  `pgy_lane_dispatch` maps a lane to an executor (Inline/Pinned run in place;
  Worker/Blocking/LocalAsync/Movable on a worker thread joined for the result)
  and fails closed on `Reject`. Its contract is executor-invariance — the same
  task yields the same result on every non-Reject lane — proved by
  `lane-scheduler-test-smoke`. This is the keystone wiring: the compiler's
  decision now reaches actual execution.
- The self-host compiler makes the SAME decision in idiomatic Pergyra
  (`src/self_hosted/sea/execution_lane.pgy` — a typed `BoundaryCaptureFact`
  struct consumed by a typed `ExecutionLane` return, `Reject` as a first-class
  variant, zero `-1` sentinels). A cross-language / cross-backend parity smoke
  (`self-host-execution-lane-parity-test-smoke`) diffs it against the C policy's
  decision-table output on both C and LLVM (10/10 each).

**Landed — spawn expression consumes the lane facade (2026-06-29):**
- Both backends used to choose the spawn executor with an independent
  `ast_spawn_is_blocking ? blocking : async` branch. The first slice routed that
  choice through `pgy_spawn_lane_from_blocking`; the second slice now emits the
  lane itself and calls the lane-owned spawn facade:
  `pgy_lane_spawn_dispatch(...)` in generated C and
  `pgy_lane_spawn_dispatch_export(...)` in LLVM. The concrete executor mapping
  lives under `PgyLaneScheduler`, so spawn expression lowering no longer selects
  `pgy_async_spawn` or `pgy_spawn_blocking` directly.

**Remaining (deep fill, not a quick slice):**
- **Precise capture plumbing.** `BoundaryCaptureFact` now exists and is stored
  on AIR, but some fields are still filled conservatively from boundary kind.
  Enriching them precisely needs per-boundary capture facts (pin/live-view,
  raw-vs-value capture) from MIR/closure-capture analysis. A coarse routine-level correlation
  (does the boundary's routine hold any slot/effect anywhere) was rejected: it
  over-pins — a `parallel` would become `PinnedZone` merely because an unrelated
  slot exists in the same routine. Precise evidence is the F-series closure-
  capture plumbing, not a kind lookup.
- **Remaining concurrent-site facade coverage.** Spawn expressions now consume
  the lane-owned spawn facade. Async blocks, `parallel { ... }`, channel
  send/receive, and cancellation still call their older runtime entry points
  directly. Unifying those sites is the next codegen step. On the self-host side
  it is gated by async *lowering*: the self-host parses async but does not lower
  it (its `mir_lower` carries zero async facts), so the self-host async codegen
  is the larger frontier -- the MIR JSON async fact
  surface, tracked with the self-host expansion.
- **Executor depth.** The Worker/Blocking/LocalAsync/Movable lanes currently
  share one worker-thread executor; backing them with the fiber scheduler /
  work-stealing pool / dedicated blocking pool is refinement under the same
  executor-invariant contract.
