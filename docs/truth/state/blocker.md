# Current blocker

Owner: GPT root lane for integration and the reached indexed-assignment seam;
Claude's other canonical fronts remain separate. Work follows the
[decision](../decisions/2026-10-18_native_checker_in_v1.md). Checked 2026-10-22
KST on local `main`; nothing is pushed while this state is red.

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`: 361 downgraded
refusals with every final owner run ([census record](../evidence/2026-10-22_native_census_string_element_copy.md)).
A census runs four analyses in 840 to 1370 s. The CI harness manifest compiles natively since `e23761a3a`.

## Open fronts, in order

1. **`direct_mir_*` routes:** 42 refusals, removed with the routes as
   the general emitter covers their programs (self-host generality).
2. **Moves (102 outside `direct_mir_*`)**, mostly getter loans whose proof
   fails, of values `Clone` copies only by value ([others are copied](../decisions/2026-10-22_moved_argument_used_again_is_copied.md)),
   and 51 local copies whose other side changed, 6 more at [`inout` exits](../decisions/2026-10-22_inout_place_changed_through_a_copy.md).
   Copies would duplicate whole fact graphs; they need proofs ([measured](../evidence/2026-10-22_deep_clone_counterfactual.md)).
   GPT has the deep `Clone` of String and scalar maps ([request](../../agent_work_directives/hashmap_deep_clone_backend_2026-10-22.md)).
3. **Borrowed `ref` escapes (63 outside `direct_mir_*`):** 37 loans still
   refused after landing, cause not split ([most are copied now](../decisions/2026-10-22_ref_call_loan_copied_where_made.md)),
   27 `ref` formals escaping through callees (the `SelfMirLower*` chain
   pops its by-value `build`), 4 callees that keep the value.
4. **Strings that leave their owner.** Owned copies land where a String
   leaves its Array owner ([decision](../decisions/2026-10-22_string_crossing_copied_where_it_crosses.md)).
   Left: 3 opaque crossings, 8 released elements, 23 owned pushes.
5. **Hidden owners:** `collection_owned_element_requirements_finalize`
   has three refusals on the canonical, from a field taken out before an
   `own` retire ([OPEN](../decisions/2026-10-19_open_field_take_before_own_handoff.md)).
   So a normal compile never runs the move, lane and named-call owners.
6. **Codegen gaps the checker admits:** a by-value `self` method on a `ref`
   receiver, `graph.blocks[0].name = x` (C emits a non-lvalue),
   `let (a, b) = (s, 0)` (C lacks `PgyTuple_String_Int_t`), and an
   `Array<String>` born `[]` in a `while` body: MIR retires one after builtin
   uses (GPT `c9513d5b2`); declared calls wait on GPT carrying the published
   [Array verdict](../decisions/2026-10-21_array_preservation_published_per_formal.md).
   A re-entered inner loop no longer stops HIR ([rule](../decisions/2026-10-22_reentered_loop_exports_last_transfer.md)).
7. **Preserved Array storage (20 outside `direct_mir_*`):** mostly calls
   after an `inout` callee wrote the Array's elements (`EmitStmtList` 8).

## Lessons on the record

- **An incomplete frame is not a safety rule.** Main admitted a pool's text
  read after `AllocatorDestroy` (ASan use-after-free); `bc2aefb30` refuses it.
- **Proofs must walk every place a formal's storage can go:** a `for`
  iterable (`99a31fe0f`) and a getter's result (ASan use-after-free;
  [rule](../decisions/2026-10-20_call_result_holding_formal_storage_is_its_path.md)).
- **Local copies share storage** ([rule](../decisions/2026-10-21_local_copies_refuse_the_other_side_after_a_change.md)).
- **A defer runs at every exit, not where it is written.** The branch admitted
  `defer { ArrayDrop(rows); }` then `return Keep(rows)` (ASan heap-use-after-free;
  [record](../evidence/2026-10-21_defer_release_admitted_at_each_exit.md)).
- **Emitted code frees nothing** on `ArraySet`, reassignment or a move: most
  owner violations leak instead of failing at run time.

## CI comparison

`make ci-push-linux` at `2333da64c` failed the same 49 of 65 steps as main
`7cd004fab` (not rerun). Bootstrap, installed driver, fixed point and CI remain OPEN.
