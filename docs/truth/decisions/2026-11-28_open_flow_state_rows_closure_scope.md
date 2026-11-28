# OPEN 2026-11-28: what closes `semantic.loop_flow_summary` and `semantic.resource_flow_universe`?

Asked by: the Claude lane, after the self-host slot release flow
([record](../evidence/2026-11-28_selfhost_slot_release_flow.md)) and loop
flag carriage ([record](../evidence/2026-11-28_selfhost_loop_flow_carriage.md)).
Not decided; both rows stay BRIDGE.

## What each row still lists as open

- `semantic.loop_flow_summary`: the self-host MIR carries loop identity,
  kind and native's result flags (equal to native on the 2,746-program
  census except one dead-code loop native does not record). It does not
  compute loop effect masks or entry/exit resource states.
- `semantic.resource_flow_universe`: the self-host MIR writes no universe
  rows. Separately, native's typed DIR/RIR transition payload migration is
  listed as open.

## Who reads the missing rows

- Native: the semantic checker builds and reads them during its own flow
  analysis; HIR, MIR and RIR copy, validate and serialize them; AIR mixes a
  has-facts bit into its fingerprint. Native C and LLVM code generation read
  none of them.
- Self-host: the general route reads only loop flags. `mir_lower`
  validates the rows and, since 2026-11-28, refuses state rows, effects or
  universe rows in a routine marked `flow_state_carried:false`. No
  self-host consumer decides anything from them.
- The refusals native derives from that state have self-host owners:
  anchored-handle release and move (`ast_slot_release_admission_owner`,
  new), Future and RemoteFuture completion (`ast_future_lifecycle_admission_owner`),
  collection moves and releases (the collection ownership checker; its
  divergences from native are decided ownership rules, blocker item 5).
  QubitSlot programs are refused by the self-host front end altogether.

## Options

1. Close both rows with this scope: native owns the rows for its own
   checker and IR copies; the self-host MIR carries loop identity, kind and
   flags and marks flow state not carried, and the self-host semantic
   owners above decide the refusals that state decides, each with a native
   parity gate. The typed DIR/RIR transition payload item moves to
   `rir.resource_transition_graph`, which owns RIR. If a self-host consumer
   that needs the rows appears, the rows reopen.
2. Keep both rows BRIDGE until the self-host front end produces universe
   rows and loop states equal to native's and carries them in MIR. The rows
   follow native's internal binding order (a symbol joins the universe at
   its first resource operation), so equality means porting native's
   resource-state analysis (about 5,300 lines of `type_checker_flow_*.c`)
   for rows nothing reads.

The Claude lane recommends option 1. Option 1 counts as SoT closure only,
not V1 progress.
