# Native checker distance

Owner: Claude lane, semantic facts of the native checker
([decision](../decisions/2026-10-18_native_checker_in_v1.md); the main
blocker is [blocker.md](blocker.md)). Checked 2026-10-23 KST on branch
`claude/p0-native-checker` (local, not pushed), with main merged.

The native checker still refuses the unchanged canonical driver input
`src/self_hosted/compiler/driver_bootstrap_main.pgy`: 189 downgraded
refusals with every final owner run ([census record](../evidence/2026-10-23_text_builder_append_refused_text_once.md)),
and 905 more String receipt refusals the final owners hide ([measured](../evidence/2026-10-23_native_census_hidden_string_receipts.md)).
A census runs four analyses in 840 to 1370 s. The CI harness manifest compiles natively since `e23761a3a`.

## Open fronts, in order

1. **`direct_mir_*` routes:** 42 refusals, removed with the routes as
   the general emitter covers their programs (self-host generality).
2. **Moves (59 outside `direct_mir_*`)**: 12 where `SemanticExpressionGraphCallTargetCaptureFromSignatures` sets its graph's call-target rows in place while the loan lives in other rows (a proof needs field-level loan origins), 14 `function_tables` maps and 8 `cfg` rows grown in place (need a deep `Clone`), 9 a real alias of `valid`, 2 a borrowed String stored into `self.outcome` and released on overwrite
   ([others are copied](../decisions/2026-10-22_moved_argument_used_again_is_copied.md); not changes [before the loan](../decisions/2026-10-23_loan_owner_changes_before_the_loan_do_not_count.md) or [after its locals' statement](../decisions/2026-10-23_loan_ends_with_the_statement_that_holds_its_locals.md), [nor a constructor](../decisions/2026-10-23_constructor_of_loan_owner_fields_reads_the_owner.md) ([of a carrier](../decisions/2026-10-23_carrier_fields_read_into_any_constructor.md)), [nor after the last read](../decisions/2026-10-23_loan_owner_changes_after_the_last_read_do_not_count.md), [nor a match pattern](../decisions/2026-10-23_match_pattern_is_no_change_of_a_loan_owner.md), [whose bindings root at the subject](../decisions/2026-10-23_match_payload_binding_rooted_at_its_subject.md)),
   and 16 local copies whose other side changed ([`inout` callees by field](../decisions/2026-10-22_inout_callee_changes_only_the_fields_it_writes.md), [callee read paths](../decisions/2026-10-23_local_copy_other_side_read_by_callee_paths.md)), 1 more at an `inout` exit.
   Copies would duplicate whole fact graphs; they need proofs ([measured](../evidence/2026-10-22_deep_clone_counterfactual.md)). GPT has the deep `Clone` of String and scalar maps ([request](../../agent_work_directives/hashmap_deep_clone_backend_2026-10-22.md)).
3. **Borrowed `ref` escapes (18 outside `direct_mir_*`):** `compatibility_view` (4) into `direct_mir_*` and `CompilerArtifactBegin`
   (copies its path; no retention row, which would change the generated canonical projection), MIR lowering (`input`, 3) ([not ArrayPop](../decisions/2026-10-22_array_pop_keeps_nothing_of_its_receiver.md), [not a write-back](../decisions/2026-10-22_store_of_held_origins_adds_no_call_escape.md),
   [not a copy into an output](../decisions/2026-10-23_complete_frame_decides_return_of_storage_free_routine.md), [not an `inout` argument](../decisions/2026-10-23_inout_callee_keeping_nothing_leaves_ref_input.md)). For GPT: `SelfMirRoutineBuildStorageRetireAfterLastConsumer(own build)` retires a build that lends `input`.
4. **Strings that leave their owner.** Owned copies land where a String
   leaves its Array owner ([decision](../decisions/2026-10-22_string_crossing_copied_where_it_crosses.md)),
   and as a Clone where the owned push is refused ([rule](../decisions/2026-10-23_refused_owned_string_push_lands_as_clone.md)).
   Left: 3 opaque crossings, 8 released elements.
5. **Hidden owners:** `collection_owned_element_requirements_finalize` has three refusals on the canonical, from a
   field taken out before an `own` retire ([OPEN](../decisions/2026-10-19_open_field_take_before_own_handoff.md)).
   So a normal compile never runs the move, lane and named-call owners, nor the String receipts pass: 900 shallow
   String stores have no independent-result proof, and a refused one selects no copy (String proof lane: GPT).
6. **Codegen gaps the checker admits:** a by-value `self` method on a `ref`
   receiver, `graph.blocks[0].name = x` (C emits a non-lvalue),
   `let (a, b) = (s, 0)` (C lacks `PgyTuple_String_Int_t`), and an
   `Array<String>` born `[]` in a `while` body: MIR retires one after builtin
   uses (GPT `c9513d5b2`); declared calls wait on GPT carrying the published
   [Array verdict](../decisions/2026-10-21_array_preservation_published_per_formal.md).
   A re-entered inner loop no longer stops HIR ([rule](../decisions/2026-10-22_reentered_loop_exports_last_transfer.md)).
7. **Preserved Array storage (3 outside `direct_mir_*`):** `SemanticExpressionGraphDomainQuery*` (a result's loan leaves by `return` on one path; the lent proof is not path-wise).
   [Same-call duplicates are copied](../decisions/2026-10-23_array_handed_to_two_keeping_formals_of_one_call_is_copied.md). An [`inout` hand-off keeps what the callee keeps](../decisions/2026-10-23_inout_formal_handed_to_inout_formal_keeps_what_the_callee_keeps.md); [ArrayPop keeps nothing](../decisions/2026-10-23_array_pop_adds_no_call_escape_to_a_formal_summary.md).

## Lessons on the record

- **An incomplete frame is not a safety rule.** Main admitted a pool's text
  read after `AllocatorDestroy` (ASan use-after-free); `bc2aefb30` refuses it.
- **Proofs must walk every place a formal's storage can go:** a `for`
  iterable (`99a31fe0f`) and a getter's result (ASan use-after-free;
  [rule](../decisions/2026-10-20_call_result_holding_formal_storage_is_its_path.md)).
- **Local copies share storage** ([rule](../decisions/2026-10-21_local_copies_refuse_the_other_side_after_a_change.md)), and so does a by-value Array an
  `inout` callee pushes into or that goes beside its own `inout` argument ([ASan](../evidence/2026-10-23_by_value_argument_beside_its_inout.md); [record](../evidence/2026-10-23_by_value_array_buffer_through_inout.md); element writes [OPEN](../decisions/2026-10-23_open_by_value_array_element_write_through_inout.md)).
- **A defer runs at every exit, not where it is written.** The branch admitted
  `defer { ArrayDrop(rows); }` then `return Keep(rows)` (ASan heap-use-after-free;
  [record](../evidence/2026-10-21_defer_release_admitted_at_each_exit.md)).
- **Emitted code frees nothing** on `ArraySet`, reassignment or a move: most owner violations leak instead of failing at run time.

Bootstrap, installed driver, gen2 == gen3 and required CI are not
established on this branch; main's are in [main.md](main.md).
