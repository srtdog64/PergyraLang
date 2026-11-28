# `semantic.loop_flow_summary` and `semantic.resource_flow_universe` closed

Observed 2026-11-29 KST on main `1fe45b8b7` plus this change, in WSL
`Ubuntu-C-WSL` tree `/root/ts4`. The rows close by the user's scope
decision of the same day
([decision](../decisions/2026-11-29_flow_state_rows_close_by_scope.md)).

## What the decision rests on

- Self-host loop rows equal native's loop kind and flags on the 2,746-program
  census except one dead-code loop
  ([record](2026-11-28_selfhost_loop_flow_carriage.md)); every self-host
  routine is marked `flow_state_carried:false` and `mir_lower` refuses
  effects, states or universe rows under that mark.
- The refusals native derives from flow state have self-host owners. The
  anchored-handle one landed on 2026-11-28
  ([record](2026-11-28_selfhost_slot_release_flow.md)); Future completion
  and collection ownership existed; QubitSlot builtins are refused by the
  self-host front end.
- The [lost-refusal census](../../audits/lost_refusal_census_2026-11-28.md)
  found no refusal native lost since 2026-11-15. Of the 82 refusals default
  C lacks, none is a flow-state refusal that a self-host owner should make
  under these rows. Eight `slot_wrapper_*_own_invalid` programs are borrow
  escapes of a `ref` anchored handle, which native refuses through its
  function parameter flow summary. The decision records them as not
  covered.

## Gates

Every enforcement gate of the two rows now runs in push CI:

- `mir_loop_flow_carriage_owner.sh` and `slot_release_flow_owner.sh`
  already did (self-host gates shard).
- `self-host-mir-resource-flow-identity-test-smoke` and
  `self-host-mir-loop-flow-summary-test-smoke` were manual. They now run
  in the self-host shard after `self-host-mir-function-param-flow-summary`,
  reusing its MIR lowering tool build. Their recipes pass `PGY_BIN` the way
  that step does, so the tool receipts match.
- `dir-resource-flow-identity-test-smoke` and
  `rir-resource-flow-identity-test-smoke` were manual. They now run in the
  core shard after `test-all`, with the CI build directory.

Measured locally: DIR 2 s, RIR 1 s, the two self-host gates 1 s each once
the tool is built (640 s for the first build, which CI pays in the step
before).

## Registry

Both rows are CLOSED with `open_reason` `none`. Native's typed DIR/RIR
transition payload migration moves to `rir.resource_transition_graph`.
Census: `CLOSED=63 BRIDGE=14 ACTIVE=2`.

## Results

- SoT edge (79 authorities, 145 derived carriers, `CLOSED=63 BRIDGE=14
  ACTIVE=2`) and adequacy, gate reachability (0 stale, 0 undeclared) and the
  component contract PASS.
- Run as CI runs them: `self-host-mir-function-param-flow-summary` (tool
  built), then the two identity targets in one make call ("receipts match;
  kept", 1 s), then the DIR and RIR targets (1 s): all PASS.
- The integrated tree these rows rest on (`95c002c21`) passed gen2 == gen3,
  the native oracle emission and the 2,746-program sweep on 2026-11-28
  (main `189674069`, CI run 46158595554 green, 34 jobs).
