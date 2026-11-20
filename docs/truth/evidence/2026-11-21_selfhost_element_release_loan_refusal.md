# Self-host element release ends String loans (2026-11-21)

Source: main `1559b5d7f` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts3` (the change) and `/root/ts2` (main). Drivers: main
`25f4850b66f3f1d4da0fbee00c8e86704ac4c44bc79306c2d297485cae6aacaa`
(`make self-host-compiler` at `5b266edd5`; no self-host source changed since);
the change's release-profile test build
`5dffb5146910b925017e0a40f92adc76eae006c7f9672628ba01bc704042a4fc`. Native
`9ca3196b586e1345b79c11fbedb384ee8ff7c7b08829612cf94d3e6da353e936`.
Decision: [element release ends String loans](../decisions/2026-11-20_element_release_ends_string_loans.md).

## Before

On main the general direct-MIR route ran
`let name = names[i]; ArrayPop(names); Log(name)` with heap-use-after-free
on C and LLVM. The new gate fails on the main driver at its first case:
`the MIR producer admitted pop_index_alias`.

The first version of the change (a session of 2026-11-20, not landed) also
refused `tests/cases/function_param_flow_summary/inout_defers_kept_by_callee_before_nested_call_landed_valid.pgy`:
the recursive `Emit` call's String result counted as a loan of the `inout`
`defers` formal, and the owned `ArraySet` on that formal ended it.

## Change

- The self-host front end ends String loans at an owned pop or set and at a
  call whose callee releases an element of the argument (its element
  requirement), and refuses a later read (`indexed_string_use_after_element_release`
  and the loop form).
- A fresh String result plan (`ast_collection_fresh_string_result_plan_owner.pgy`)
  proves which functions return only fresh text. A `let` or assignment that
  takes such a call's result records no loan. `Emit` is in the plan: its
  returned local is `""` and `Concat(result, nested)`.

## Results

- `element_release_loan_refusal_owner.sh`: 10 refusals on the MIR producer
  and both default routes (2 new: a returned local joined with the argument,
  and one an `inout` call rebinds), 6 admitted programs equal on both default
  routes and clean under ASan (1 new: a callee returning `Concat`-built
  text), 1 argument copied where it is taken. PASS. On the main driver it
  fails at the first case.
- The `_valid` program above is admitted again.
- Canonical DRV-2 producer (`--emit-mir-json-verified` on
  `driver_rung2_main.pgy`): exit 0 on both drivers, 140 s / 149 s, peak
  4,782,056 / 4,786,760 KiB, MIR byte-equal (SHA-256 prefix `25460f1a5da69bb6`).
- Default-LLVM sweep, 1,398 programs against the `3ca35ae4a` sweep: equal
  526 → 526. Three `function_param_flow_summary` `*_landed_valid` programs
  that the main front end refused (`borrow_boundary_escape`) pass the front
  end and are refused by the route that cannot carry their landed Clone.
- `owned_element_retaining_text_negative.pgy` is renamed
  `owned_element_retaining_text_landed_positive.pgy`. The landing plan
  copies `a[0]` where `NamedFrom` takes it; the default C route emits
  `NamedFrom(pgy_concat(pgy_as_get(a, 0), ""))` and runs clean under ASan
  (exit 2, `StringLength("xy")`), the default LLVM route refuses with the
  typed landing refusal, and the native pipeline admits it. The 2026-11-12
  [mismatch record](2026-11-12_inout_effect_mismatch_classes.md) found native
  copying and the self-host driver reading freed memory.

## Ownership gates

The 45 ownership gates that make targets or push CI steps reach ran in
`/root/ts3` with the change's driver installed as `bin/pgy-self-driver` and a
semantic owner probe built from the changed tree. 38 PASS, including
`shared_storage_copy_landing_owner.sh` (its refused own hand-off now reports
`indexed_string_use_after_element_release`), `collection_inout_effect_owner.sh`
(with the renamed fixture) and the component contract. The other 7 fail the
same way on main in `/root/ts2` with the main driver and a main probe:
`aggregate_field_policy_probe_parity` and `collection_policy_probe_parity`
(`LLVM ERROR: Instruction Combining did not reach a fixpoint`),
`array_string_layout_consumer_closure_owner` (owned-return C projection
refused), `assignment_projection_probe_parity` (CODEGEN ERROR),
`collection_descriptor_effect_owner` (case 49), `collection_replacement_cleanup_owner`
and `initializer_projection_probe_parity`. `tests/repo/gate_script_reachability_smoke.py`
PASS.

## Not covered

- Reassigning the array under a live loan, a `for` binding and a call nested
  in another expression, as the decision lists.
- The bootstrap (gen2 == gen3) and installed-driver jobs run in CI.
