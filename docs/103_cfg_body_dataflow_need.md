# CFG Body Dataflow Need

Last updated: 2026-04-27

This document fixes why CFG-backed body dataflow is a beta blocker for PergyraLang. It is not a new language axis and it does not widen the beta surface. It is the missing execution-proof layer for the surface that already exists.

## Summary

Pergyra already has CFG infrastructure:

- HIR has function CFG v0, block predecessors/successors, reachability, dominator/frontier, loop depth, local defs, and phi candidate skeletons.
- RIR carries flow-block summaries for resource, projection, world-handoff, invalidation, authority-loss, and branch/join facts.
- MIR has routine/block/instruction/cleanup blocks, SSA version maps, def/use summaries, cleanup/rollback/invalidation exceptional CFG, liveness/DCE slices, and backend vertical slices.
- MIR cleanup consumes RIR flow/fact/semantic summaries for rollback and
  invalidation block decisions. Intent AST-carried invalidation scanning is not
  a valid beta path; `cfg-body-dataflow-test-smoke` rejects reintroduction of
  `using` / `transfer` AST fallback checks inside `mir_cleanup.c`.

The missing beta blocker is different:

- Semantic body safety is not yet fully sourced from those CFG/dataflow facts.
- Some body checks still depend on AST-shaped traversal, local helper summaries, or backend-side fallback behavior.
- Strict beta requires the compiler to prove routine body behavior across all paths, not just along local syntax order.

## Why CFG Is Necessary

AST traversal is enough to parse and type-check many local expressions. It is not enough to close the safety model that Pergyra advertises.

The following checks are inherently path-sensitive:

- All-path return: every reachable normal path in a non-void routine must return a value.
- Definite assignment: a variable initialized in one branch but not another must be rejected before use.
- Move/use-after-move: moved resources must stay unavailable across branch/join and loop edges.
- Borrow lifetime: borrowed references must not outlive their source or overlap with incompatible mutable borrows.
- Drop/cleanup: owned resources must be released on normal return, early return, break/continue, rollback, cancellation, and invalidation paths.
- Zone/effect transition: zone state, effect publication, projection freshness, and handoff state must remain coherent at joins.
- Parallel/channel boundary: moved values, borrowed references, authority-bearing tokens, and cancellation cleanup must be summarized at task boundaries.

These are graph properties. They require blocks, edges, terminators, join states, widening rules, and diagnostics that can explain which path produced the failure.

## Beta Scope

CFG body dataflow is beta scope because it protects already-stable surfaces:

- `func`, `action`, `intent step`, `parallel`, and channel execution.
- anchored `own`/`ref` and slot-handle boundary behavior.
- `zone`, `effect`, `relation`, `projection`, `handoff`, and runtime observability.
- C/LLVM parity for the frozen subset.

It is not beta scope to add:

- full Rust-style region inference;
- arbitrary ownership lattice generalization;
- advanced optimizer passes;
- full exception/unwind semantics beyond the current intent cleanup/rollback/invalidation model.

## Required Dataflow Facts

Each body-level routine needs a stable analysis summary:

- `reachable`: block is reachable from entry.
- `must_return`: all normal exits from this block return.
- `maybe_uninit`: local may be used before assignment.
- `moved`: resource was moved and cannot be used.
- `borrowed_shared` / `borrowed_mut`: reference lifetime and exclusivity facts.
- `must_drop`: owned resource needs cleanup at this edge.
- `zone_state`: zone lifecycle state at entry/exit.
- `effect_state`: effect publish/refresh/bind state at entry/exit.
- `projection_state`: projection freshness at entry/exit.
- `task_boundary`: values crossing `parallel` or channel boundaries.
- `cleanup_edge`: normal-to-cleanup, cleanup-to-rollback, cleanup-to-invalidation edges.

Interprocedural summaries must be explicit:

- `may_return`
- `may_escape_ref`
- `moves_param`
- `borrows_param`
- `drops_resource`
- `effects`
- `requires_zone`
- `spawns_task`
- `sends_channel`

## Slot Borrow-Safety Bridge Facts

`docs/semantics/08_slot_capability_calculus.md` deliberately states that Slot
is not a borrow checker by itself. CFG body dataflow is the static bridge that
can make a narrow borrow-checker-equivalent claim honest for the stable subset.

The bridge is expressed through these facts:

- `NoEscape(view, region)`: a `ReadView<T>`, `WriteView<T>`, or future
  `PinnedView<T>` cannot be returned, stored into longer-lived state, captured
  by a spawned task, or sent through a channel unless a stable ownership
  transfer rule explicitly admits it.
- `NoSuspend(view, region)`: a live view cannot cross `await`, `spawn`,
  `async`, `parallel`, callback, channel handoff, or cancellation cleanup
  boundaries.
- `WriteExclusive(slot, region)`: while a `WriteView<T>` is active for a
  source slot, no other read or write view of the same slot may be active.
  Shared `ReadView<T>` / `ReadView<T>` remains accepted.
- `DropOnce(owner, all_cfg_exits)`: owner cleanup must be inserted exactly once
  on every normal and early-exit path that owns the resource.
- `ReleaseAfterUnpin(slot, all_cfg_exits)`: a pinned slot can only be released
  after all active views have been unpinned on every reachable exit.
- `NoUnsupportedTokenTransport(token, boundary)`: authority-bearing tokens
  cannot cross unsupported task/channel/cancellation boundaries.

Current beta evidence covers the first stable slice:

- `NoEscape`: `ReadView<T>` return escape reports `PGY_SEM_PIN_ESCAPE`;
  borrowed subject spawn boundary reports `PGY_SEM_BORROW_ESCAPE`; token and
  ownership-bearing channel/cancel transports are rejected.
- `NoSuspend`: active `ReadView<T>` across `await`, `spawn`, `async` block,
  event callback registration, channel handoff/close, cancellation cleanup,
  `parallel`, and view acquisition inside `parallel` report `PGY_SEM_PIN_AWAIT_BOUNDARY` and
  `PGY_SEM_PIN_PARALLEL_CONFLICT`.
- `WriteExclusive`: `ViewWrite(...)` conflicts with any active view of the
  same slot; `ViewRead(...)` conflicts with an active `WriteView<T>`; shared
  `ReadView<T>` bindings remain accepted.

Remaining bridge work:

- HIR/MIR already preserve pin-region metadata for source slot, view binding,
  and read/write mode, and MIR now materializes `pin-unpin-cleanup-edge`
  metadata. The MIR validator now rejects reachable pin-region blocks that
  lack the matching unpin cleanup fact, so backend/runtime consumers no longer
  have to rediscover pin regions from desugared statements;
- `DropOnce` and `ReleaseAfterUnpin` over the final block-scoped pin surface;
- wider no-escape/no-suspend proof for closure/lambda captures and general
  async task lifetimes.

## Diagnostics Contract

Every CFG-backed body diagnostic must include:

- source construct and block/path provenance;
- previous state and conflicting new state;
- branch/join or cleanup edge that makes the issue reachable;
- `Reason:`;
- `Fix:`.

Example diagnostic shape:

```text
error[PGY-CFG-MOVE-JOIN]: resource may be used after move
  path: function ShipCargo block[03] -> block[05]
  previous state: moved in branch true
  conflicting state: used after join
  Reason: the resource is unavailable on at least one reachable path.
  Fix: move the use into the owning branch, clone a copyable value, or return ownership before the join.
```

## Implementation Skeleton

The migration should be incremental and gated:

1. Inventory current HIR/RIR/MIR facts and keep them visible through `cfg-body-dataflow-test-smoke`.
2. Promote reachability and all-path return to semantic CFG consumers.
3. Promote definite assignment/use-before-init. For the current stable subset,
   local delayed initialization is sealed by syntax (`let` requires `=`) and by
   `PGY_SEM_UNINIT_LOCAL` as a semantic backstop. The remaining CFG work is the
   general branch/join assignment lattice needed if delayed assignment or wider
   mutable local initialization becomes beta-stable later.
4. Promote move/use-after-move and borrow lifetime facts.
5. Promote drop/cleanup insertion point calculation.
6. Promote zone/effect/relation/projection transition facts.
7. Promote `parallel`/channel/task boundary facts.
8. Make MIR/C/LLVM consume the same frozen facts and add backend compare cases.

## Current Progress

- Done: `cfg-body-dataflow-test-smoke` gates HIR CFG, HIR dominator, RIR flow-block, and MIR cleanup/SSA visibility.
- Done: HIR CFG lowering now gives `break` and `continue` explicit loop edges
  instead of leaving them as opaque statement payloads. `while` and `for`
  bodies carry a loop context, so `break` targets the loop exit block and
  `continue` targets the loop header. `src/test_hir.c` locks this with the
  `HIR CFG lowers loop break and continue edges explicitly` regression.
- Done: HIR CFG loop control is label-aware. Nested `break outer` and
  `continue outer` now resolve to the named loop's exit/header instead of the
  nearest loop, matching the semantic loop-label validation surface.
- Done: HIR CFG lowering now expands `match` into an explicit dispatch chain.
  Each `case` gets a branch edge, case bodies and `default` bodies join through
  CFG successors, and terminating case bodies remain closed. `src/test_hir.c`
  locks this with `HIR CFG lowers match cases and default as explicit edges`.
- Done: HIR CFG lowering now expands `select` into the same explicit
  dispatch/join helper as `match`. Channel readiness cases and default bodies
  are visible to HIR/MIR CFG consumers instead of remaining a single opaque AST
  payload. `src/test_hir.c` locks this with
  `HIR CFG lowers select cases and default as explicit edges`.
- Done: HIR CFG lowering now traverses `unsafe` block bodies instead of keeping
  them opaque. Control-flow constructs inside `unsafe` blocks, including
  nested returns, now produce the same CFG terminators as ordinary blocks.
- Done: MIR cleanup block creation now consumes RIR policy ops, RIR
  conservative semantics, RIR flow-block summaries, and RIR resource facts for
  rollback/invalidation decisions. The former intent-step AST invalidation
  fallback is removed.
- Done: MIR validation now treats `pin-unpin-cleanup-edge` as a required
  cleanup fact for every reachable pin-region block, including the source slot,
  view binding, and read/write mode. `src/test_mir.c` includes a negative
  regression that corrupts this fact and expects `mir_validate()` to reject it.
- Done: non-`Void` function all-path return now consumes the semantic CFG body flow summary and emits `PGY_SEM_MISSING_RETURN` when a reachable normal path can fall through.
- Done: unreachable statements after direct terminators and after `if`/`match`
  bodies whose reachable paths all terminate now emit
  `PGY_SEM_UNREACHABLE_CODE` with `Reason:` and `Fix:` instead of being
  silently ignored by the body-flow walk.
- Done: semantic regression covers missing return on one branch, complete if/else
  returns, exhaustive `Option<T>` match returns, unreachable-after-return,
  unreachable-after-terminating-if, unreachable-after-exhaustive-match,
  unreachable-after-loop-break, and unreachable-after-loop-continue.
- Done: source-level loop move/join regression covers `QubitSlot` consumption on
  a `break` exit path and consumed-resource detection on a `continue` backedge.
- Done: `defer` cleanup bodies no longer terminate the surrounding CFG path;
  a `return` inside `defer` does not make the following statement unreachable
  and does not satisfy a non-`Void` function's all-path return requirement.
- Done: `defer` cleanup bodies use a resource-state snapshot. Moves, releases,
  and cleanup-only `break`/`continue` facts inside `defer` are checked, but they
  do not consume the surrounding path's current resource state or outer loop
  flow.
- Done: the legacy `type_check_statement()` fallback path now reuses the same
  `defer` cleanup snapshot helper, so direct AST semantic tests and full body
  flow share one cleanup-resource contract.
- Done: resource snapshots now include anchored slot state (`Slot<T>`,
  `SecureSlot<T>`, and `DeviceSlot<T>`) in addition to `QubitSlot` consumption
  state. A release on a terminating branch no longer poisons the reachable
  fallthrough path, while a release on a fallthrough branch remains joined as
  released.
- Done: CFG ownership snapshots now include classifier-backed ownership
  boundary values (`subject` identity, borrow-tracked aggregates, movable
  resources, and anchored handles). `own subject` transfers in terminating
  branches are isolated from fallthrough paths, fallthrough transfers join as
  consumed, and parallel subject transfers use the same duplicate-consume
  conflict gate as slot resources.
- Done: parallel CFG ownership snapshots now include task-local use facts. A
  `ref` borrow of an ownership-bearing value in one parallel task conflicts
  with an `own` transfer of the same value in another task, closing the stable
  `borrow + move` task-boundary hole without opening a full arbitrary borrow
  lattice.
- Done: shared `ref` reads of the same ownership-bearing value across parallel
  tasks are explicitly accepted. The stable beta line is `ref`/`ref` allowed,
  `ref`/`own` rejected, and `own`/`own` rejected.
- Done: direct named-call `spawn` now rejects `ref` parameters whose parameter
  type is ownership-bearing, because the spawned task may outlive the current
  synchronous frame. Copy-only `ref` spawn arguments remain accepted. This
  closes the stable borrowed-reference task lifetime baseline without claiming
  full closure/lambda lifetime solving.
- Done: function types now carry a first-stage interprocedural
  `body_summary_mask`. Semantic recording covers `may_return`,
  `may_escape_ref`, `moves_param`, `borrows_param`, `drops_resource`,
  `effects`, `requires_zone`, `spawns_task`, and `sends_channel` as explicit
  summary bits. Direct function calls now consume callee summaries and propagate
  the transitive facts that are meaningful to the caller (`may_escape_ref`,
  `drops_resource`, `effects`, `requires_zone`, `spawns_task`, and
  `sends_channel`) while intentionally not propagating callee-local
  `may_return`. Direct function calls, method calls, and host calls now also
  record declaration-known boundary facts (`effects`, `requires_zone`, and
  `own/ref` parameter modes). This is not
  yet the final CFG evaluator, but it gives later zone/effect/runtime
  propagation and backend parity gates a stable summary seam instead of
  rediscovering facts by ad-hoc AST traversal.
- Done: lambda bodies now get an isolated function-summary lane. Effects and
  body facts recorded while checking the lambda are stored on the lambda
  function type, then the enclosing function's effect/body summary is restored.
  This prevents lambda-local `return`, `spawn`, and channel facts from
  polluting the outer routine before the lambda is actually called. Calling a
  lambda through a function-typed binding consumes that stored summary through
  the same callee-summary path as named functions.
- Done: anonymous async spawn bodies are explicitly rejected for beta. The
  parser may accept `spawn async () { ... }`, but semantic now reports it as
  beta-out-of-scope until closure capture and lifetime facts are modeled. Named
  async/function spawn remains the stable surface.
- Done: `parallel` task bodies are checked from a shared entry resource
  snapshot, task-local terminators do not terminate the outer CFG path, moved or
  released resources are joined after the parallel block, and duplicate
  resource consumption across tasks emits `PGY_SEM_PARALLEL_SLOT_CONFLICT` with
  `semantic:parallel:resource_conflict`. Blocking channel send of a movable
  resource inside a parallel task is now included in the same resource-consume
  boundary and is covered by semantic regression.
- Done: timeout/status builtin channel sends (`SendTimeout`, `TrySendStatus`,
  `SendTimeoutStatus`) are regression-locked to the same transport policy as
  `TrySend`: builtin send helpers reject movable resource payloads and
  authority-bearing `Token<T>` payloads, while the explicit blocking send
  operator remains the stable ownership-transfer surface.
- Done: `TryRecv` and `RecvTimeout` explicitly reject ownership-bearing
  channels (movable slot handles, subjects, boundary values, anchored handles,
  and tokens). The stable non-blocking receive path is copy-only; blocking `<-`
  receive into a named binding remains the supported path for ownership-bearing
  payloads because it creates a visible CFG/resource fact.
- Done: `Cancel(Future<T>)` / `Cancel(RemoteFuture<T>)` explicitly reject
  ownership-bearing payloads. Copy-only cancellation remains stable, while
  movable/anchored/subject/token payload cancellation stays blocked until an
  interprocedural task cleanup summary proves observation/release.
- Done: `ChannelClose(Channel<T>)` explicitly rejects ownership-bearing
  payload channels. Copy-only close remains stable; movable/anchored/subject/
  token channels must be drained through explicit receives until a
  cleanup/backpressure summary exists.
- Done: Slot borrow-safety bridge facts are now named in this document and in
  `docs/semantics/08_slot_capability_calculus.md`. Existing `ReadView(...)` /
  `ViewWrite(...)` regressions cover the first `NoEscape`, `NoSuspend`
  (`await`, direct `spawn`, `async` block, event callback registration, channel
  send/receive/close, cancellation cleanup, and `parallel`), and
  `WriteExclusive` slice without claiming full Rust-style borrow checking.
- Done for the stable local-binding subset: `let` declarations require an
  initializer at parse time, and `PGY_SEM_UNINIT_LOCAL` remains as the semantic
  guard if an initializer-free local AST is constructed by another path.
- Remaining: richer reachability provenance across nested/exceptional CFG edges,
  general branch/join assignment lattice beyond the current sealed `let`
  surface and longer-lived borrow lifetime beyond the current task-local
  borrow/use snapshot baseline, full drop/cleanup insertion and
  validation beyond the current `defer` terminator/resource-state isolation,
  zone/effect transitions,
  broader channel receive/backpressure, and richer cancellation summaries. Anchored slot
  branch-join state, `own subject` branch-join state, the current parallel
  task/channel-send resource-or-boundary consume boundary, and parallel
  `ref`+`own` boundary conflicts, plus direct named-call `spawn ref` boundary
  rejection, first-stage interprocedural `body_summary_mask` with direct
  function-call and method-declaration propagation, anonymous async spawn
  explicit reject, timeout/status channel-send
  transport rejection,
  non-blocking ownership-bearing receive explicit reject, copy-only
  cancellation payload reject, and copy-only channel close are closed baseline
  evidence, not remaining blocker text.
- Remaining mutable-borrow overlap is beta-out-of-scope until there is a
  first-class `mut ref` / `ref mut` surface. Current `ref` parameters are
  non-owning shared borrows.

## Completion Criteria

CFG body dataflow is beta-closed when:

- `make cfg-body-dataflow-test-smoke` is a required CI gate.
- `make test-semantic` includes positive and negative tests for all-path return, use-before-init, move/borrow join, drop cleanup, zone/effect transition, and parallel/channel boundary.
- `make ir-pipeline-test-smoke` proves the HIR/RIR/MIR fact bridge does not regress.
- `make llvm-test-backend-compare` contains representative frozen body dataflow cases.
- docs, diagnostics, and implementation all use the same terms.
