# CFG Body Dataflow Need

Last updated: 2026-04-25

This document fixes why CFG-backed body dataflow is a beta blocker for PergyraLang. It is not a new language axis and it does not widen the beta surface. It is the missing execution-proof layer for the surface that already exists.

## Summary

Pergyra already has CFG infrastructure:

- HIR has function CFG v0, block predecessors/successors, reachability, dominator/frontier, loop depth, local defs, and phi candidate skeletons.
- RIR carries flow-block summaries for resource, projection, world-handoff, invalidation, authority-loss, and branch/join facts.
- MIR has routine/block/instruction/cleanup blocks, SSA version maps, def/use summaries, cleanup/rollback/invalidation exceptional CFG, liveness/DCE slices, and backend vertical slices.

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
  summary bits. This is not yet the final CFG evaluator, but it gives later
  zone/effect/runtime propagation and backend parity gates a stable summary
  seam instead of rediscovering facts by ad-hoc AST traversal.
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
  rejection, first-stage interprocedural `body_summary_mask`, anonymous async
  spawn explicit reject, timeout/status channel-send transport rejection,
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
