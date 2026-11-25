# Bare field reads in class methods on the general route (2026-11-26)

Source: main `52396e0a7` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` (drivers, sweep) and `/root/ts3` (gates). Drivers (test
profile, `-O0`): base `driver_cl1c` (the `fd2e097a7` source), change
`driver_vr1`. Follows the [receiver-field change](2026-11-25_receiver_field_binding.md).

## Before

A class or struct method has a value receiver (`receiver_carriage:
value`): it gets its own copy of the receiver record, so a write inside it
is not seen by the caller (native prints `75 50 151` for a `Deposit` that
adds 25 to a balance of 50 and returns it). `return n + 1` in such a method
reads the receiver's field `n` by its bare name; the general route found no
local or parameter `n` and refused at `leaf-operand`. `return self.n + 1`
already ran. After the receiver-field change, `leaf-operand` with an
`AST_BINARY` source was the third largest default LLVM refusal (62
programs), most of them class methods.

## Change

- `direct_mir_receiver_field_binding_owner.pgy`: in a method of a value
  receiver whose type is an admitted logical record, an unbound leaf naming
  one of its fields is admitted as the receiver parameter and its record
  member (the nodes `self.n` gets). The leaf fact names the member kind, so
  the expression admission builds a cell member or a record member.
- A bare field write in such a method stays refused
  (`identity_cell_store_target`): the receiver copy has no store owner here.
- `direct_mir_receiver_field_owner.sh` also runs
  `tests/self_hosted/fixtures/direct_mir_value_receiver_field.pgy` (bare
  `Int`, `String` and `Bool` field reads, in a loop and through a method
  calling another with `self.`).

## Results

- `direct_mir_receiver_field_owner.sh` PASS on `driver_vr1`; on
  `driver_cl1c` it fails at the new fixture (`leaf-operand`).
- Sweep of 2,746 programs, `driver_cl1c` -> `driver_vr1` (equal / self
  refuses / other output / runs a native refusal): default LLVM
  891 / 1,137 / 0 / 25 -> 937 / 1,091 / 0 / 25; default C
  1,418 / 602 / 8 / 85, unchanged. The 46 newly equal programs (all class
  or struct methods reading bare fields, mostly
  `tests/cases/backend_compare/class_*` and `option_class_*`) run ASan clean
  and equal to native on direct C.
- Twenty other refused programs now stop later: 13 at `enum-match`, 3 at a
  terminal return, and one each at `return_abi`, `admitted-type`,
  `builtin-call` and a ref-formal release check.
- Component contract, language word registry (inventory regenerated), SoT
  authority edge and adequacy and gate reachability PASS.
- The 30 CI direct-MIR and self-host-gate scripts of the
  [subject Clone record](2026-11-25_subject_clone.md) and the two
  installed-pair gates PASS on `driver_vr1` with `wslpath` hidden.
- Codegen bootstrap: gen2 == gen3 (125,935 lines).

## Not covered

- A bare field write in a class method.
- A method call without `self.` inside a class method (`Next()`): the
  self-host front end refuses it (`call_arity_mismatch`, expected 1, actual
  0); native admits it.
