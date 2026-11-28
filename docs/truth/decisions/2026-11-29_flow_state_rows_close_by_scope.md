# 2026-11-29: `semantic.loop_flow_summary` and `semantic.resource_flow_universe` close by scope

Decided by: the user, on 2026-11-29 ("Sot 2개 닫는걸 목표로 시작해 결정할것은
범위 결정으로 닫고"), answering the
[2026-11-28 OPEN question](2026-11-28_open_flow_state_rows_closure_scope.md)
with its option 1; recorded by the Claude lane. The user can overturn it.

## Rule

1. The native semantic checker owns the loop flow rows and the resource-flow
   universe for its own flow analysis. HIR, MIR and RIR copy, validate and
   serialize them; native C and LLVM code generation reads none of them.
2. The self-host MIR carries each loop's identity, kind and native result
   flags, read by the general route's loop admission and range iteration.
   It writes every routine with `"flow_state_carried":false`: no effect
   masks, loop entry/exit states or universe rows. `mir_lower` refuses
   any of those in a routine without the `true` mark.
3. The refusals native derives from that state have self-host owners,
   each with a native parity gate:
   - anchored-handle release and move: `ast_slot_release_admission_owner.pgy`,
     `slot_release_flow_owner.sh`;
   - Future and RemoteFuture completion:
     `ast_future_lifecycle_admission_owner.pgy`;
   - collection moves and releases: the collection ownership checker. Its
     divergences from native are decided ownership rules.
   - QubitSlot builtins are refused by the self-host front end altogether.
4. The self-host loop rows follow the self-host CFG. A loop native skips
   as dead code but the self-host CFG keeps has a self-host row (one
   census program, `control_navigation_dead_ancestor_valid`).
5. Native's typed DIR/RIR transition payload migration is not part of
   these rows. It moves to `rir.resource_transition_graph`, which owns RIR.

## Reopens when

- a self-host consumer needs loop states, effect masks or universe rows;
  the rows are then produced and carried, and these rows reopen;
- or a refusal native derives from that state is found admitted on a
  self-host route without a decided divergence.

## Not covered

- A borrowed (`ref`) anchored handle that escapes into an `own` argument, a
  return, an array, a binding or an assignment (eight
  `function_param_flow_summary/slot_wrapper_*_own_invalid` programs that
  default C admits, [audit](../../audits/lost_refusal_census_2026-11-28.md)).
  Native refuses them through its function parameter flow summary, not
  through the flow state these rows carry. They belong to the anchored-handle
  borrow boundary.

Replaces: the [OPEN question](2026-11-28_open_flow_state_rows_closure_scope.md).
