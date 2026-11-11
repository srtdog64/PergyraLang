# 2026-11-12: the native compiler admits 40 memory-unsafe negative fixtures

Run by the Claude root lane while classifying the step-4 contract gate
failures that the merged semantic-owner probe exposed.

## Method

- Compiler: native `pgy` built from `3cd211b94` sources (the MIR validator
  and LLVM registry change included), SHA-256
  `69eca50e7500ad1b0b20b34d9215133cec0599b104d2146dc92b6eda57e85a4b`,
  WSL `Ubuntu-C-WSL`, gcc with `-fsanitize=address,undefined`.
- Inputs: every `*_negative*.pgy` in
  `tests/self_hosted/parity/fixture/collection_field_lifetime/` and
  `tests/self_hosted/parity/fixture/aggregate_storage_transfer/` (483 files).
  Most are analyzer-only inputs for self-host gates; no gate had compiled
  them with the native compiler.
- Each input the native compiler admits (`--native-pipeline --emit-c`) is
  built with ASan/UBSan and run with `ASAN_OPTIONS=detect_leaks=0`, 10 s.

## Result

- 483 negatives, 275 admitted natively, 40 report a memory error at run
  time: 33 heap-use-after-free, 6 double free, 1 stack overflow
  (`inout_cycle_drop_negative`, an unbounded recursion).
- Map payload views (12): `map_payload_{array_escape, assigned_formal_after,
  assigned_formal_before, field_alias, forwarded, loop_backedge, map_alias,
  nested_argument, remove, replace, string_alias, unproved_formal}_negative`.
  Minimal shape: `let view: String = MapGet(table, "key");` then
  `MapSet(table, "key", "new")` then a read of `view`. The same two map
  inputs are admitted with the use-after-free at `3dac8b4ac`, `f077729f2`,
  `8a56b31e4`, `1acc31c5b` and `673efaede`; this is not a recent regression.
- Owned strings (7): `owned_string_join_alias`,
  `owned_string_literal_transfer_aliased_result`,
  `owned_string_local_literal_transfer_{alias, allocator_formal, reuse}`,
  `owned_string_named_actual_{after, duplicate}` (all `_negative`).
- Exclusive aggregate aliasing and growth (13): `exclusive_{alias_read_shared_root,
  array_copy, branch_rebind_continue, copy_alias, dead_alias_partial,
  focus_reads_grown_field, grown_field_holder, holder_rebind_branch,
  inout_repeat_shared_alias, later_holder, mutating_sibling_call,
  plain_alias_grown_alias, plain_alias_grown_source}_negative`.
- Others (8): `empty_capture_field_assignment_negative`,
  `empty_capture_nested_sibling_alias_negative`, `inout_cycle_drop_negative`,
  `inout_deferred_copy_own_negative`, `inout_index_deferred_own_negative`,
  `inout_index_nested_own_negative`, `inout_nested_own_argument_copy_negative`,
  `read_temporary_releasing_sibling_negative`.

## Self-host comparison (collection_inout_effect, C backend)

The gate's verdict loop, run on a scratch copy that reports instead of
stopping, differs from its expectations on 59 inputs: 37 negatives the
self-host analyzer now admits, 15 positives it refuses, 7 negatives refused
with another code. The admitted negatives include 11 of the 12 unsafe map
payload views above (`map_payload_assigned_formal_after_negative` is still
refused by the self-host analyzer). `owned_element_retaining_text_negative`
is admitted by both; the native C copies and runs clean, while the installed
self-host driver's C (driver of 2026-11-11 07:12) reads freed memory.

## Consequence

The V1 criterion "the self-host compiles every program the native compiler
admits" cannot take these 40 admissions as its oracle; the native checker
has to refuse them (or land a copy) first.
