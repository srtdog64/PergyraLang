# Self-host foreach String loans and Array reassignment (2026-11-24)

Source: main `72c2d2747` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` and `/root/ts3`. Native reference: `bin/pgy` built from
`cc27d42c6` (no native C source changed since). Drivers: base `driver_a3c`
(the `72c2d2747` source, sha256 `6350360a6bec6e70`, release), change
`driver_f4` (release). Decision:
[a foreach String binding lends its element](../decisions/2026-11-20_foreach_string_binding_lends_its_element.md).

## Before

- `foreach_string_borrow/push_then_set_valid`: a walked String pushed into
  another Array, then an owned `ArraySet` on the walked Array and a read of
  the other Array. Native copies at the push and prints `5 99 2`; both
  default self-host routes printed freed memory.
- `foreach_string_borrow/kept_after_set_invalid`: the binding kept in an
  outer String, then the owned set and a read. Native refuses; both default
  routes compiled it and printed different bytes on C and LLVM. The sweep
  counted it as "native refuses" and so never showed it.
- The same push followed by `names = []`, and `let first = names[0];
  names = []; Log(first)`: heap-use-after-free on default C and direct MIR C
  (ASan). Native refuses the first (MIR validation) and leaks in the second.

## Cause

1. The local binding facts hold `let` and destructure bindings only, so a
   leaf naming a `for` binding is unresolved, and the loan source policy
   took an unresolved String leaf as proved independent (0). The binding
   carried no loan, so neither a carrier it was stored in nor an outer String
   it was assigned to did.
2. A whole reassignment of an `Array<String>` local ended no loan, though
   the emitted code releases the storage the previous definition held.

## Change

- `ast_collection_iteration_string_loan_owner.pgy` (new): a leaf no local
  declares is looked up in the nearest enclosing `for` that declares its
  name. Over an `Array<String>` local the binding borrows that local's
  storage source, over a `Slice<String>` local an unknown source; over
  anything else (a formal, a field, a call result) it still borrows nothing.
- `ast_collection_indexed_string_borrow_source_policy_owner.pgy` asks it for
  an unresolved leaf.
- `ast_collection_indexed_string_borrow_retirement_owner.pgy`: at the value
  lane of a whole reassignment of an `Array<String>` local, before the new
  definition completes, the loans proved to borrow the old storage end
  (`ast_collection_ownership_scan_owner.pgy` calls it).
- Gate `element_release_loan_refusal_owner.sh`: 4 more refusals (the pushed
  binding then set, the kept binding then set, an index loan and a pushed
  binding read after a reassignment) and 3 more neighbours (the pushed
  binding with no release, a reassignment after the last read, a
  reassignment of another array). PASS; on `driver_a3c` it fails at the
  first new case.

A first version ended loans at a reassignment as a direct deep drop does,
including every loan of unknown source. The canonical DRV-2 producer then
refused the compiler at `program_enum_variant_index_owner.pgy:97` and,
after ending unknown loans only for unproved storage, at
`direct_mir_scalar_program_builtin_signature_projection_owner.pgy:134`:
both loans joined a formal's text, which no local reassignment frees. The
reassignment now ends only loans proved to borrow the released storage.

## Results

- 18 probes on native and both default routes: the 4 programs above are
  refused (each read freed memory on `driver_a3c` under ASan); 8 neighbours
  equal to native and clean under ASan on default C and direct MIR C,
  among them a walked `inout` formal, a walked `Split` result and a
  reassignment of another array. Native copies where the change refuses in
  4 (a pushed loan then released, also for a `let` index loan).
- Sweep, 2,746 programs: only the two `foreach_string_borrow` programs
  moved. Default C other output 9 -> 8, default LLVM 1 -> 0;
  `kept_after_set_invalid` is refused on both.
- Canonical DRV-2 producer: exit 0, 189 s, peak 4,686,936 KiB; MIR
  byte-equal to `driver_a3c`'s on the same tree (sha256 `41c50ddfb0be321d`).
- Tool programs (38): every verdict and MIR equal to `driver_a3c`'s.

## A class the sweep did not count

Native refuses 718 programs. The base driver's default C route compiled and
ran 92 of them (LLVM 25); after the change 91 (24). Under ASan on default C:

- 79 run clean (75 are `function_param_flow_summary` fixtures, where the
  self-host lands copies or hands off where native refuses);
- 5 `slot_contract/reject` programs stop at the runtime slot guard (exit
  134), as intended;
- 1 recurses without end (stack overflow);
- 6 read freed memory: `inout_call_string_loan/` `array_set_in_callee`,
  `call_in_branch`, `callee_declared_later`, `loop_continue_path`,
  `loop_read_before_call`, `reassigned_local`. Native refuses a String loan
  held across an `inout` call that frees it (`c5bbf595f`); the self-host
  front end has no such rule.

## Gates

Run in `/root/ts3` on main + this change with `driver_f4` installed:

- The 45 ownership gates the Makefile or push steps reach, the semantic
  owner probe build and the default-route parity gate: 38 + 2 PASS. The 7
  that fail fail as on main
  ([list](2026-11-21_selfhost_element_release_loan_refusal.md#ownership-gates)).
- Component contract: FAIL at one line cap,
  `ast_collection_indexed_string_borrow_source_policy_owner.pgy` 129/125;
  repaired by folding one call in that file (no cap raised).
- Codegen bootstrap: gen2 == gen3 (125,740 lines).

Final tree (cap repaired), release driver `driver_f5` (sha256
`591bbaab1a320ec9`): component contract, `element_release_loan_refusal_owner.sh`
and `default_route_native_runtime_parity_owner.sh` PASS; canonical DRV-2
exit 0, 166 s, peak 4,689,332 KiB, MIR equal to `driver_f4`'s on that tree.
The installed-driver and driver fixed-point jobs run in CI.

## CI step order

`build-linux` of `72c2d2747` was cancelled at its 30-minute limit twice
(CI run 45349187007): the 1,132 unchanged native objects compiled in 128 s
and 133 s, against 101 s in the last green run, so the runner was slower;
the job took 20-25 minutes on the runs before. In that job the release debug
section step built `/tmp/pgy-ci-linux-bin/pgy` (1,133 objects, 119 s), and
the `clean` step before `test-all` deleted that build, which `test-all` then
made again with the same flags. `scripts/ci_push_linux_steps.sh` now runs
the release step after `test-all`, which leaves the binary it checks. No
limit is raised and no step is removed.

## Not covered

- A `for` binding over a formal, a field or a call result borrows nothing;
  the probes of a walked `inout` formal and a walked `Split` result run
  clean.
- A reassignment of an `Array<String>` local whose storage is unproved ends
  no loan.
- A read of the binding itself after a release in the body is refused by
  direct MIR (`CODEGEN ERROR`), not by the front end.
- The six `inout_call_string_loan` programs above.
