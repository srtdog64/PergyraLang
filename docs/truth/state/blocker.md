# Current blocker

Owner: GPT root lane for main integration and the one reached ownership
producer at a time. Work follows the
[decision](../decisions/2026-10-18_native_checker_in_v1.md). Checked 2026-10-22
KST at executable commit `701a1fe51`; nothing is pushed while this state is
red.

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`: the 350-refusal baseline
was reduced to 342 with every final owner run
([scalar-root record](../evidence/2026-10-22_expression_graph_scalar_root_projection_census.md)).
Exactly eight graph-view moves disappeared and no refusal was added. This run
took 951.49 s and stayed within the 30-minute integration budget.

## Open fronts, in order

1. **Moves (94 outside `direct_mir_*`)**, mostly getter loans whose proof
   fails, of values `Clone` copies only by value ([others are copied](../decisions/2026-10-22_moved_argument_used_again_is_copied.md)),
   and 51 local copies whose other side changed, 6 more at [`inout` exits](../decisions/2026-10-22_inout_place_changed_through_a_copy.md).
   Copies would duplicate whole fact graphs; they need proofs ([measured](../evidence/2026-10-22_deep_clone_counterfactual.md)).
   GPT has the deep `Clone` of String and scalar maps ([request](../../agent_work_directives/hashmap_deep_clone_backend_2026-10-22.md)).
2. **Borrowed `ref` escapes (57 outside `direct_mir_*`):** callees whose formal
   reaches a call escape (driver and MIR emission chains; [not ArrayPop](../decisions/2026-10-22_array_pop_keeps_nothing_of_its_receiver.md)),
   and returned loans a copy of the call cannot end because the callee stores
   the String into an `inout` output (`...NumericStringAllocationRecordResolvedCall`).
3. **Strings that leave their owner.** Owned copies land where a String
   leaves its Array owner ([decision](../decisions/2026-10-22_string_crossing_copied_where_it_crosses.md)).
   Left: 3 opaque crossings, 8 released elements, 23 owned pushes.
4. **Hidden owners:** `collection_owned_element_requirements_finalize`
   has three refusals on the canonical, from a field taken out before an
   `own` retire ([OPEN](../decisions/2026-10-19_open_field_take_before_own_handoff.md)).
   So a normal compile never runs the move, lane and named-call owners.
5. **Preserved Array storage (20 outside `direct_mir_*`):** mostly calls
   after an `inout` callee wrote the Array's elements (`EmitStmtList` 8).
6. **Codegen gaps the checker admits:** a by-value `self` method on a `ref`
   receiver, `graph.blocks[0].name = x` (C emits a non-lvalue),
   `let (a, b) = (s, 0)` (C lacks `PgyTuple_String_Int_t`), and an
   `Array<String>` born `[]` in a `while` body: MIR retires one after builtin
   uses (GPT `c9513d5b2`); declared calls wait on GPT carrying the published
   [Array verdict](../decisions/2026-10-21_array_preservation_published_per_formal.md).
   A re-entered inner loop no longer stops HIR ([rule](../decisions/2026-10-22_reentered_loop_exports_last_transfer.md)).
7. **`direct_mir_*` routes (42):** retire only as the general MIR emitters
   cover their programs. Do not fix those per-program shape routes or count
   their deletion before the ownership fronts outside them are closed.

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
