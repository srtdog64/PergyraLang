# Receiver fields without `self.` on the general route (2026-11-25)

Source: main `d2df5562a` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` (drivers, sweep) and `/root/ts3` (gates). Drivers (test
profile, `-O0`): base `driver_dl1` (the `d2df5562a` source), change
`driver_rf2c`; `driver_rf2` is the same change with the intent routes
switched off and the route log on, used to see what the general route does
with the intent programs. Slice 1 of the
[intent plan](../../agent_work_directives/general_route_intent_execution_2026-11-21.md).

## Before

Inside a method of a mutable-identity receiver (a subject action), MIR
carries `value = value + 1` as an owner-field SSA variable: an `owner_field`
definition (`arg0` `value`, result `value.1`) whose uses name the entry
value `value.0`, later definitions using `value.N`, and phis at joins. A
read before any definition in the routine has no use at all (`return
value + 1` in a getter has `uses: []`). The general route refused all of
them: the local ref plan counted the definition as a local `value` that no
source local or parameter matches (`local_inventory`), and a bare read
found no local (`leaf-operand`). The explicit `self.value = self.value + 1`
already ran, as an identity-cell store and member reads.

## Change

- New owner `direct_mir_receiver_field_binding_owner.pgy`: the receiver
  field's ordinal (a method whose receiver is an admitted mutable-identity
  cell, declared by the routine's receiver owner row), the owner-field
  variable names of a routine (`arg0` of its `owner_field` definitions),
  an unbound leaf that names a receiver field, and a phi that joins only
  versions of one receiver field.
- Local ref plan: an owner-field variable's definitions and phis get no
  local; an ordinary definition with an owner-field variable's name makes
  the plan invalid.
- An `owner_field` definition is the identity-cell store
  (`DirectMirIdentityCellReceiverFieldStore`): the target is one unbound leaf
  naming the definition's field, the ABI type is the field's type, and the
  store publishes no SSA value.
- Expression admission: a leaf that is no local, parameter or match binding
  and names a receiver field is admitted as the receiver parameter and its
  identity-cell member, the two nodes `self.value` gets. In an
  instruction's value lane it consumes the leaf's `value.N` uses; the target
  lane of an owner-field definition carries none.
- A receiver-field phi has no operation; routine admission skips it after
  checking it, and the collection-ownership receipt plan, which pairs each
  row-producing instruction with an operation, skips it too.
- Gate `direct_mir_receiver_field_owner.sh` (self-host gates shard).

Reads go to the cell, so a call that changes the field between two reads
is seen, as in native code.

## Results

- Probes (`driver_rf2`, C under ASan and LLVM, against native): a bare
  write, a getter read with no use, reads and writes across a branch and
  its phi, a `String` field, and a method mixing `self.steps = steps + 1`
  with `self.value` all print native's output. The leaks are those of the
  explicit spelling (the `ToString` results the logs print).
- `direct_mir_receiver_field_owner.sh` PASS on `driver_rf2c`; on
  `driver_dl1` it fails at the first projection (`local_inventory`). Its four
  forged MIR are refused by the general route: a definition of a field the
  cell lacks and a target naming another field at
  `identity_cell_store_target`, a local definition with a field variable's
  name through the invalid local plan (reported first by the value type
  plan, `stage=inputs`), and a bare read of an unknown name at
  `leaf-operand`.
- Sweep of 2,746 programs, `driver_dl1` -> `driver_rf2c` (equal / self
  refuses / other output / runs a native refusal): default LLVM
  887 / 1,141 / 0 / 25 -> 891 / 1,137 / 0 / 25; default C 1,418 / 602 / 8 / 85,
  unchanged. Newly equal: `examples/battle_test.pgy`,
  `examples/battle_test2.pgy`,
  `tests/cases/backend_compare/subject_action_global_helper/main.pgy` and
  `tests/cases/backend_compare/subject_action_state/main.pgy`; all four run
  ASan clean and equal to native on direct C.
- Eight refused programs now stop later: three with a class method on a
  value receiver at `identity_cell_store_target` (below), two intent
  programs at their intent instructions (`instruction_kind`, slice 3),
  `examples/battle_sim.pgy` at a store through a non-receiver cell
  parameter (`target.hp = ...`; the store admits only `self`),
  `examples/battle_minimal.pgy` at `program_readiness=25` and
  `examples/vessel_method_test.pgy` at a `Bool` direct call.
- Component contract, language word registry (inventory regenerated: the
  new fixture is counted), SoT authority edge and adequacy (79 authorities,
  144 carriers, `CLOSED=61 BRIDGE=16 ACTIVE=2`) and gate reachability
  (887 scripts, 0 undeclared, 0 stale) PASS.
- 29 CI direct-MIR and self-host-gate scripts this change reaches (the
  push-step direct-MIR gates, the legacy, composite and nested intent
  route gates, the record and generic gates, the new gate) and the two
  installed-pair gates PASS on `driver_rf2c` with `wslpath` hidden, and
  again on `driver_cl1c` with the subject Clone change.
  `direct_mir_nested_intent_program_c_owner.sh` is a part the nested LLVM
  gate sources, not a standalone script.
- Codegen bootstrap on the tree with both changes: gen2 == gen3
  (125,935 lines).

## Not covered

- A method with a value receiver (`class Account` in
  `src/self_hosted/mir_lower/fixture/owner_field_assignment.pgy`,
  `receiver_carriage: value`) is refused at `identity_cell_store_target`:
  its field writes change the callee's copy, which no general-route owner
  stores yet.
- A compound assignment to a bare field (`value += 2`, MIR `assign-add`
  with the field's entry use on the target lane) goes through the compound
  path and is refused at `graph-use-completeness`; native prints 3 for
  `Counter(1)`. The explicit `self.value += 2` is refused as well
  (`program_readiness=25`): a compound store into a cell field is a gap of
  both spellings.
