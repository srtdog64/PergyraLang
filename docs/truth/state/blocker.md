# Current blocker

Owner: Claude lane (GPT root lane's work since 2026-11-19); published
revision/CI: [main](main.md).
The v11 full MIR stops at transition ordinal 1438, the same first refusal as
published v8; no C output ([receipt](../evidence/2026-11-18_inout_member_restore_transition.md)).
A scratch diagnostic traversed all 9,623 route instances: 109 distinct first refusals ([audit](../../audits/selfhost_full_mir_independent_refusal_census_2026-11-18.md)).
Next: publish and validate the sealed owned-result callee proof in MIR before
admitting a returned call-result row across branches. The semantic plan is
retired before MIR publication; changing its `retired` class alone would trust
forgeable input. The reached owner/consumer map and falsifiers are in the
[active plan](../../agent_work_directives/selfhost_owned_value_lifetime_execution_2026-11-18.md).
The general emitter leaks the deleted collection route's `ToString` results and returned `Array<Int>` ([run record](../evidence/2026-11-19_collection_program_route_deleted.md));
nonempty record-field retirement leaks 32 bytes per backend (S2). The
one-process full DRV-2 source-to-C now ends within its 14 GiB bound at
12.1 GiB ([measurements](measurements.md)). Of 2,746 corpus programs default LLVM
runs none unlike native and default C eight: five `Float` prints, a zone refresh,
a party role call, a skipped `defer` ([record](../evidence/2026-11-24_selfhost_foreach_string_loans.md)). Of the 718 native refuses, default C runs 85, none reading freed memory under ASan
(the six `inout_call_string_loan/` ones are refused since 2026-11-24, [record](../evidence/2026-11-24_review_batch_inout_set_copy_report_word_tiers.md)).
Default C, through the MIR-to-AST round trip, also prints garbage for a
foreach whose body pushes to the walked local ([record](../evidence/2026-11-19_general_route_collection_iteration.md)). Generality: the one-routine dispatch is the general route alone; the intent and three-routine routes remain; the three-routine route runs one corpus program, a generic method returning `Array<Point>` ([census](../evidence/2026-11-25_record_field_array_abi.md)) ([intent plan](../../agent_work_directives/general_route_intent_execution_2026-11-21.md), first slice: receiver field binding identity in MIR).
Native refuses a String foreach or index loan read after a set, pop or drop
(2026-11-20 [record](../evidence/2026-11-20_native_foreach_string_borrow_refusal.md)), and after `ListSet`/`ListRemove` refuses the List foreach loan and copies a `let` one ([record](../evidence/2026-11-20_native_list_string_loan_retirement.md)); a List or Slice element stored past its release is copied ([record](../evidence/2026-11-20_native_unknown_string_store_copy.md)). A loan held across an `inout` call that frees it is refused or copied ([record](../evidence/2026-11-20_native_inout_call_string_loan_retirement.md)); a release before a `break` read after the loop still reads freed memory. The self-host front end refuses a String loan read after an owned pop or set, or after a call that releases the element ([record](../evidence/2026-11-21_selfhost_element_release_loan_refusal.md)), and since 2026-11-24 a `for` binding over an `Array<String>` local lends its element and a whole reassignment of proved storage ends its loans; a loan of unproved storage still survives a reassignment.

## Objective card

- Objective: neither front end admits freed-memory access on C or LLVM;
  required CI green on one published commit (user goal 2026-11-14).
- Priority: memory safety, one fact owner, fail-closed admission, backend
  parity, fixed point, exact-SHA CI.
- Forbidden: larger budgets, reduced input, skipped backend, weakened
  negative or per-program-shape route admission.

## Retained obligations (not parallel implementation tracks)

1. Required CI on the commit carrying this file ([gates](gates.md)).
2. Memory: four legs, LLVM IR at -O0, 3143 inputs: only endless recursion
   ([evidence](../evidence/2026-11-16_llvm_legs_at_o0.md)). The `--mir-json`
   route admits MIR that lacks a match temporary or loop binding row (the
   direct backend refuses it); a fix is on the DM3 branch, unverified.
3. One hand-off rule ([decision](../decisions/2026-11-16_one_handoff_rule_for_both_front_ends.md)):
   S0-S4 landed ([S3](../evidence/2026-11-17_s3_formal_alias_closure.md),
   [S4](../evidence/2026-11-17_s4_native_array_formal_class.md)). S5a (one
   later-use definition, Unproved moves) is an unlanded branch. Before S6
   deletes the backend sharing guard, `alias_read_after` must move or be
   refused: with the guard off it double-frees. S5c: direct MIR copies String,
   Array and record landings; Map copies and a landed initializer's `clone`
   row are open. `semantic.hashmap_collection_ownership` stays ACTIVE.
4. Compiler temporaries released at last use
   ([decision](../decisions/2026-11-16_compiler_temporaries_are_released_at_last_use.md)):
   DRV-2 fits its bound by less waste; the driver still releases almost nothing.
5. Front ends differ: native admits a `let` field write through a world,
   zone slot and subject path; `x = values` from a ref formal; generic
   `ref value: T`; reserved zone/role/effect names. F2 branch, unlanded.
   A `List` passed to a `ref` formal is refused even for `ListSize`. Native
   admits a `ref` formal stored in a local constructor; self-host refuses. A `ref` formal moved into a growing callee: native refuses, self-host copies it ([record](../evidence/2026-11-23_selfhost_record_clone.md)). An element write through a copy it cannot follow (one handed to a call inside a `Print`): native copies, self-host refuses ([record](../evidence/2026-11-24_selfhost_element_write_copies.md)).

## Falsifiers kept

- `direct_mir_array_string_cleanup_fact_owner.sh`: refused 17+4+6+2, admitted 9x2 plus retained/nested cases; call-handoff row reader: 44.
