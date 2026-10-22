# Decisions

One decision per file. A file is never rewritten; a later decision names
the one it replaces. OPEN files wait for the user.

| Date | Decision | Status |
|---|---|---|
| 2026-10-14 | [V1 is two goals](2026-10-14_v1_scope.md) | in force |
| 2026-10-16 | [One main, integrated by the GPT root lane](2026-10-16_main_only_integration.md) | in force |
| 2026-10-17 | [State and decisions live in docs/truth/](2026-10-17_docs_truth_facade.md) | in force |
| 2026-10-17 | [Is the new native ownership checker part of V1?](2026-10-17_open_native_checker_scope.md) | answered 2026-10-18 |
| 2026-10-18 | [The native ownership checker is part of V1](2026-10-18_native_checker_in_v1.md) | in force |
| 2026-10-18 | [A call moves a default-mode aggregate the callee changes](2026-10-18_default_aggregate_move.md) | in force |
| 2026-10-18 | [The flow work cap counts closure entries, not frame nodes](2026-10-18_flow_budget_counts_closure_entries.md) | in force |
| 2026-10-18 | [Copy or move a String the proof route cannot alias?](2026-10-18_open_string_push_landing.md) | answered 2026-10-19 |
| 2026-10-19 | [Is Clone of a struct a deep copy?](2026-10-19_open_struct_clone_depth.md) | answered 2026-10-19 |
| 2026-10-19 | [An unprovable String store lands as an owned copy](2026-10-19_string_landing_copy_then_move.md) | in force |
| 2026-10-19 | [Clone of a struct is a deep copy](2026-10-19_struct_clone_is_deep.md) | in force |
| 2026-10-19 | [A result may lend storage of a `ref` input](2026-10-19_results_lend_ref_inputs.md) | in force |
| 2026-10-19 | [Lend or move a by-value aggregate to a getter?](2026-10-19_open_by_value_getter_lend_or_move.md) | answered 2026-10-19 |
| 2026-10-19 | [A by-value getter call lends a formal used again](2026-10-19_by_value_getter_lends_when_used_again.md) | in force, partly replaced |
| 2026-10-19 | [A callee that returns a String element lends it](2026-10-19_returned_string_element_lends.md) | in force |
| 2026-10-19 | [Text finished with a non-result allocator borrows it](2026-10-19_finished_text_borrows_its_allocator.md) | in force |
| 2026-10-19 | [A getter lends a local too, until a call hands it on](2026-10-19_local_getter_lends_until_handed_on.md) | in force |
| 2026-10-19 | [A field taken out before an `own` hand-off?](2026-10-19_open_field_take_before_own_handoff.md) | OPEN |
| 2026-10-19 | [A generated projection may change with its registry](2026-10-19_generated_projection_in_canonical.md) | in force |
| 2026-10-19 | [A local or loop variable that takes a path is the formal's alias](2026-10-19_path_aliases_follow_the_formal.md) | in force |
| 2026-10-20 | [A getter's loan that cannot be proved: copy or move?](2026-10-20_open_escaping_loan_copy_or_move.md) | answered 2026-10-20 |
| 2026-10-20 | [An unprovable getter loan is copied where the proof fails](2026-10-20_escaping_loan_copies_where_it_fails.md) | in force |
| 2026-10-20 | [A formal's storage an output keeps is copied where it is stored](2026-10-20_formal_storage_kept_by_output_is_copied.md) | in force |
| 2026-10-20 | [A loan is copied where it is made when its exit cannot be](2026-10-20_loan_copied_where_made_when_its_exit_cannot_be.md) | in force |
| 2026-10-21 | [Loop-local Array retirement preserves the birth check](2026-10-21_loop_local_array_cleanup.md) | bounded implementation contract |
| 2026-10-21 | [ArraySort and ArrayReverse permute their receiver in place](2026-10-21_array_sort_permutes_in_place.md) | in force |
| 2026-10-21 | [Every loan of a path is checked](2026-10-21_every_loan_of_a_path_is_checked.md) | in force |
| 2026-10-21 | [A flow pass evaluates only entries whose reads changed](2026-10-21_flow_passes_evaluate_only_changed_reads.md) | in force |
| 2026-10-21 | [A change through one side of a local copy refuses the other](2026-10-21_local_copies_refuse_the_other_side_after_a_change.md) | in force |
| 2026-10-21 | [A String argument no proof admits is copied at the call](2026-10-21_string_argument_no_proof_admits_is_copied.md) | in force |
| 2026-10-21 | [Copy a moved argument the caller uses again?](2026-10-21_open_copy_a_moved_argument_used_again.md) | answered 2026-10-22 |
| 2026-10-21 | [The rows check's intent-step owner clauses lost their falsifier](2026-10-21_open_intent_step_owner_clauses_unfalsified.md) | OPEN |
| 2026-10-21 | [Each formal's Array preservation verdict is published](2026-10-21_array_preservation_published_per_formal.md) | in force |
| 2026-10-22 | [A re-entered loop exports the transfer of its last check](2026-10-22_reentered_loop_exports_last_transfer.md) | in force |
| 2026-10-22 | [A formal's storage assigned into an output is copied there](2026-10-22_formal_storage_assigned_into_output_is_copied.md) | in force |
| 2026-10-22 | [A moved argument used again is copied where the call takes it](2026-10-22_moved_argument_used_again_is_copied.md) | in force |
| 2026-10-22 | [Borrowed storage handed to a changing or keeping callee is copied there](2026-10-22_borrowed_storage_handed_on_is_copied.md) | in force |
| 2026-10-22 | [A loan copy never stands at an own or ref argument](2026-10-22_loan_copy_never_stands_at_own_or_ref_argument.md) | in force |
| 2026-10-22 | [An alias returned inside a result is the formal's own return](2026-10-22_returned_alias_is_the_formals_return.md) | in force |
| 2026-10-22 | [A copy changed under a loan of its source copies the loan](2026-10-22_copy_changed_under_a_loan_is_copied.md) | in force |
| 2026-10-22 | [A `ref` call's loan no proof admits is copied where it is made](2026-10-22_ref_call_loan_copied_where_made.md) | in force |
| 2026-10-22 | [An `inout` formal written through nowhere preserves the caller's storage](2026-10-22_unwritten_inout_formal_preserves_storage.md) | in force |
