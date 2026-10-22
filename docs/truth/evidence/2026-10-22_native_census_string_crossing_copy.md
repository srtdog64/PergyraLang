# 2026-10-22: canonical census after a String leaving its Array owner is copied where it leaves

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`93d076173` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-22_string_crossing_copied_where_it_crosses.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `93d076173` | 3404, 741, 636, 634 | 484 |
| copy where the String leaves | 3404, 566, 437, 435 | 393 |
| and the window pair reads through Clone | 3404, 565, 436, 434 | 392 |

Outside `direct_mir_*`: opaque String crossings 86 -> 3, Strings
retained beyond their Array owner 6 -> 0, preserved-storage Array calls
43 -> 41, `ArrayDropOwnedStrings` storage 2 -> 1. Inside
`direct_mir_*`: 150 -> 42. No kind was added in the final run.

## How the cause was found

The opaque refusals were mostly String fields passed to fact
constructors and Strings returned or placed in literals; only builtin
stores had an owned-copy landing. The refusal is made during type check,
before values are published, so the selection is recorded from the
owner's own String fact rather than a published value. The first run
added one refusal: `MirProgramRoutineIndex` in
`BuildMirProgramRoutineIndexFromTable` could no longer prove that
`source_json_extent` is `StringLength` of `source_json` once the source
was `Clone(routines.json)`. The extent owner now reads a builtin Clone's
argument, whose length the copy keeps.

## Fixtures (717 compared against `93d076173`)

- 24 negatives that checked such a crossing's refusal now land one copy
  each and were renamed `*_landed_valid`: constructor, field, tuple,
  Result, slot and secure-slot, device write, write view, spawn,
  returned-element and shadowed-`Join` cases. 20 run with C and LLVM
  output equal and C with ASan and UBSan clean. Three call an unlinked
  `extern` and are checked to MIR. `array_result_owned_element_tuple_direct_landed_valid`
  runs on LLVM; C cannot build its `let (a, b) = (s, 0)` (no
  `PgyTuple_String_Int_t`), already on the base without any copy.
- `array_result_owned_element_direct_return_invalid` stays refused, now
  at 18:9 only: the callee's return lands a copy, but the String owner
  still reads the call's result as a loan of the dropped Array.
- `formal_string_store_local_alias_set_invalid` is unchanged: an
  `ArraySet` keeps its caller's store landing, and a second copy there
  would wrap the value twice (`formal_string_store_landing_value_set_valid`
  counts one `Clone`).
- New: `string_crossing_window_pair_landed_valid` (one landing, prints
  `row`, ASan clean) and `string_window_pair_other_call_invalid` (a
  routine between the text and the window: refused at 14:12).

## Gates and mutations

`function-param-flow-summary` gate passed (214 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the `test-mir`
target still exits 2 at its self-host driver step, as on main).

| Mutation | Caught by |
|---|---|
| no copy where a String crosses an opaque boundary | `array_result_local_owned_element_constructor_landed_valid` refused, 0 landings |
| no copy where a String is retained past its Array | `array_result_owned_element_local_return_landed_valid` refused, 0 landings |
| builtin stores take the copy too | flow gate: `formal_string_store_landing_value_set_valid` reaches MIR with two Clones |
| window pair does not read through Clone | `string_crossing_window_pair_landed_valid` refused after its landing |
| window pair reads through any one-argument call | `string_window_pair_other_call_invalid` admitted |
