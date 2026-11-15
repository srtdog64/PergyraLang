# Current blocker

Owner: Claude root lane. Checked 2026-11-15 on `fix/llvm-leg-memory`
(base main `20325cad5`, CI green there).

## Objective card

- Objective: neither front end admits a lifetime program that reads or
  frees freed memory on either backend, with required CI green on one
  published commit (user goal 2026-11-14: all memory errors, SoT rows they
  pass through).
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Required CI on the commit carrying the LLVM-leg fixes. Measured on the
   C and LLVM legs of both front ends (868 lifetime fixtures, ASan): the
   only memory error left is endless recursion (`inout_cycle_*`).
2. The semantic `disposition` stays `live` after a constructor or a callee
   that grows the parameter takes a local's storage. Direct MIR refuses that
   release instead (C leg: 0 fixtures lost; LLVM leg: 174 to 136 admitted,
   6 of the 38 crashed, the rest crash once growth passes the literal's
   capacity). Exporting the hand-off as a retirement would admit them again.
3. The self-host MIR route carries no compiler `Clone` and lands no element
   copy for a callee's escaping elements (`collection_field_use_after_move`,
   `storage_opaque_return_read_positive` refused).
4. A user-call String producer that the owned-result examination neither
   proves nor disproves stays at the native boundary.
5. Front-end divergences the self-host still admits: `let first = values`
   for a `ref` formal (refused at codegen), `fail` as a parameter name.
6. Locally, core step 5 fails only on WSL (`wslpath -w` gives a UNC repo
   root). A pass-through `wslpath` shim makes it pass.

## Done since `a7d7655e1`

- A `ref` formal passed on by value is dereferenced in C and LLVM
  (`ref_formal_value_argument`); direct MIR refuses an `Array<String>`
  cleanup with neither a row nor a runtime producer (landed `20325cad5`).
- Native LLVM `ToString(Bool)` returns the runtime's owned copy, as C does
  (`bool_to_string_owned_drop`; the old LLVM leg aborted freeing a literal).
- Direct MIR refuses to release an `Array<String>` whose storage a growing
  callee, a constructor whose record is not only read, or another binding
  still holds, and a grown alias of an inout or ref member; the descriptor
  preservation proof gained a String-element mode for it.

## Falsifiers kept

- A drop on one branch still refuses the read after the join. An own
  transfer of storage a constructor kept stays refused natively.
- `direct_mir_array_string_cleanup_fact_owner.sh` refuses 9 inputs (no row,
  growing callee, constructor alias, a record grown past capacity, an inout
  member alias) and runs reading callees and a read-only record on both
  legs.
- `owned_string_member_actual_positive` and
  `empty_capture_two_fields_positive` stay admitted beside the refused
  negatives they mirror.
