# 2026-11-14: native unsafe admissions of negative fixtures, remeasured

Run by the Claude root lane after required CI went green on `bcdf042c4`.
Same inputs as the
[2026-11-12 record](2026-11-12_native_admits_unsafe_negative_fixtures.md);
this run also counts a SEGV, which that record's pattern did not match.

## Method

- Compiler: native `pgy` built in WSL `Ubuntu-C-WSL` from the `751e5dd05`
  tree (native sources unchanged by `bcdf042c4`), SHA-256
  `838364fb9ec9fd3c8cd53c2b96518dc777940e762ff1146e06b7241ef5a7a016`.
- Inputs: every `*_negative*.pgy` in
  `tests/self_hosted/parity/fixture/collection_field_lifetime/` and
  `tests/self_hosted/parity/fixture/aggregate_storage_transfer/` (485
  files).
- Each input the native compiler admits (`--native-pipeline --emit-c`) is
  built with gcc `-fsanitize=address,undefined` and run with
  `ASAN_OPTIONS=detect_leaks=0`, 10 s. A run that exits non-zero is
  classified by the first sanitizer line of its stderr.

## Result

- 485 negatives, 263 admitted natively. 36 report a memory error at run
  time: 21 heap-use-after-free, 8 SEGV, 6 double free, 1 stack overflow.
  Three more exit non-zero without one: `copy_field_loop_holder` runs past
  10 s, `exclusive_nested_copy_{grown,prefix}_read` stop on an array index
  panic. Two emit C that does not compile.
- Owned Strings (13): heap-use-after-free in `owned_string_{join_alias,
  literal_transfer_aliased_result, local_literal_transfer_alias,
  local_literal_transfer_reuse, member_actual_borrowed,
  member_actual_reused, named_actual_after}`; double free in
  `owned_string_{local_literal_transfer_allocator_formal,
  named_actual_duplicate}`; SEGV in `owned_string_{literal_transfer_borrowed,
  local_literal_transfer_borrowed, local_literal_transfer_shadow,
  named_actual_borrowed}`. 24 of the 34 owned-String negatives are
  admitted; the native checker has no transfer rule for a local String
  passed to an `own` formal or retired through `ArrayDropOwnedStrings`.
- A callee grows a local copy of a by-value formal's field (11):
  `exclusive_{array_copy, branch_rebind_continue, copy_alias,
  dead_alias_partial, focus_reads_grown_field, grown_field_holder,
  holder_rebind_branch, later_holder, mutating_sibling_call,
  plain_alias_grown_alias, plain_alias_grown_source}`; the
  `copy_field_loop_holder` loop has the same shape.
- Others (12): `bundle_field_inout_borrowed`, `callable_table_borrowed`,
  `empty_capture_nonempty`, `numeric_string_shadow` (SEGV);
  `empty_capture_field_assignment`, `empty_capture_nested_sibling_alias`,
  `inout_index_deferred_own`, `inout_index_nested_own`,
  `read_temporary_releasing_sibling` (use after free);
  `inout_deferred_copy_own`, `inout_nested_own_argument_copy` (double
  free); `inout_cycle_drop` (stack overflow).
- C that does not compile: `member_formal_index_target` and
  `member_local_index_target` (`pgy_array_set_Array<String>`: the element
  store did not sanitize a nested Array element's suffix).
- All names above end in `_negative`. Fixed since 2026-11-12: the 12 map
  payload views (`0268e6330`), `exclusive_alias_read_shared_root` and
  `exclusive_inout_repeat_shared_alias`.
