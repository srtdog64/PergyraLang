# Current blocker

Owner: Claude root lane. Checked 2026-11-15 on `fix/ref-formal-value-argument`
(base main `a7d7655e1`, CI green there).

## Objective card

- Objective: neither front end admits a lifetime program that reads or
  frees freed memory, with required CI green on one published commit (user
  goal 2026-11-14: all memory errors, SoT rows they pass through).
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Required CI on the commit carrying the self-host fixes
   ([measurement](../evidence/2026-11-15_selfhost_lifetime_fixture_admissions.md)):
   self-host lifetime fixtures admit no memory error except endless
   recursion (`inout_cycle_read_positive`, shared with native).
2. A user-call String producer that the owned-result examination neither
   proves nor disproves stays at the native boundary. The self-host
   analyzer proves the canonical ones, e.g. `EmitFunctionSet`.
3. Locally, core step 5 fails only on WSL (`wslpath -w` gives a UNC repo
   root). A pass-through `wslpath` shim makes it pass.
4. The self-host MIR route carries no compiler `Clone` and lands no
   element copy for a callee's escaping elements. Native lands a `Clone`
   for `collection_field_use_after_move`; the self-host refuses it, and
   refuses an `Array<String>` user call result such as
   `storage_opaque_return_read_positive`.
5. Front-end divergences the self-host still admits:
   - `let first = values` for a `ref` formal (refused at codegen, not
     semantically);
   - `fail` as a parameter name;
   - `Main() -> Int` returns its value as the exit code (native C exits 0).

## Done since `a7d7655e1`

- An actual of a `ref` formal passed on by value is dereferenced in C and
  LLVM. Backend compare case `ref_formal_value_argument`.
- Direct MIR `Array<String>` cleanup refuses a local that has no row and
  no runtime producer, instead of guessing a deep drop. Gate
  `direct_mir_array_string_cleanup_fact_owner.sh` (contracts shard).
  Five self-host positives that crashed with SEGV are now refused.

## Falsifiers kept

- A drop on one branch still refuses the read after the join. An own
  transfer of storage a constructor kept stays refused natively.
- A field restore whose MIR row names another field, and a binding move
  whose destination row names itself, are refused with no artifact.
- A user call result and a `ref` formal alias are refused direct-MIR
  `Array<String>` cleanup on C and LLVM. A borrowed literal, pushed owned
  copies and a `Split` result still compile and run.
- `owned_string_member_actual_positive`, `exclusive_focus_update_positive`
  and `empty_capture_two_fields_positive` stay admitted beside the refused
  negatives they mirror.
