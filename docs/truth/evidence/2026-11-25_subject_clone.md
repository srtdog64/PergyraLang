# `Clone` of a subject on the general route (2026-11-25)

Source: main `d2df5562a` plus the [receiver-field change](2026-11-25_receiver_field_binding.md)
plus this change. Trees: WSL `Ubuntu-C-WSL` `/root/ts2` (drivers, sweep) and
`/root/ts3` (gates). Drivers (test profile, `-O0`): base `driver_rf2c` (the
receiver-field change), change `driver_cl1c`; `driver_cl1` is the same
change with the intent routes switched off and the route log on. Slice 2
of the [intent plan](../../agent_work_directives/general_route_intent_execution_2026-11-21.md).

## Before

`Clone` admitted a `String`, an `Array`, or a logical record (copied field
by field), not an identity cell. `CounterZone(Clone(counter))` and
`let copy = Clone(counter)` with `counter` a subject were refused at
`builtin-call`; this was the first refusal of all three intent programs
once the intent routes are off.

## Change

- `direct_mir_scalar_program_logical_record_clone_owner.pgy`: the copied
  type may be a subject or vessel cell as well as a record. Its copy reads
  each field as a cell member (a `String`, `Array` field through its Clone,
  a record field by the record copy) and a fresh cell constructor takes the
  results. The copy plan carries each step's type name instead of a record
  field index. A zone or world cell, which holds cells, has no copy.
- The expression admission is unchanged: the cell copy arrives through the
  existing record Clone lowering, and a zone slot built from it is an
  inline constructor, which the identity-cell lifetime owner admits.
- Gate `direct_mir_subject_clone_owner.sh` (self-host gates shard).

## Results

- Probes (`driver_cl1`, C under ASan and LLVM, against native): a subject
  with `Int`, `String` and `Bool` fields cloned to a local, the clone
  cloned into a zone slot, each changed afterwards: `4 6 5 a true` on both
  backends, as native; `CounterZone(Clone(counter))` prints 3.
- `direct_mir_subject_clone_owner.sh` PASS on `driver_cl1`; on
  `driver_rf2c` it fails at the first projection (`builtin-call`).
- Sweep of 2,746 programs, `driver_rf2c` -> `driver_cl1c`: no program
  changes class (default LLVM 891 / 1,137 / 0 / 25, default C
  1,418 / 602 / 8 / 85). Three intent programs move from `builtin-call` to
  their intent routine (`instruction_kind`):
  `tests/cases/axis_composition/comp_actor_intent/who_a.pgy`, `who_b.pgy`
  and `comp_world_intent/control.pgy`.
- With the intent routes off (`driver_cl1`), the three intent gate programs
  (legacy, composite, nested) first stop at their intent routine
  (`instruction_kind`, `AST_INTENT_DECL`); before slice 1 they stopped at
  this `Clone`.
- Component contract, language word registry (inventory regenerated), SoT
  authority edge and adequacy (79 authorities, 144 carriers) and gate
  reachability (888 scripts, 0 undeclared, 0 stale) PASS.
- The receiver-field record's 29 gate scripts, `direct_mir_subject_clone_owner.sh`
  and the two installed-pair gates PASS on `driver_cl1c`.
- Codegen bootstrap: gen2 == gen3 (125,935 lines).

## Not covered

- `Clone` of a zone (`tests/self_hosted/fixtures/direct_mir_zone_clone_refused.pgy`;
  native prints 3) is refused at `builtin-call`.
- The copied `String` fields are not released with their cells: the gate's
  program leaks the two 2-byte copies under LeakSanitizer.
