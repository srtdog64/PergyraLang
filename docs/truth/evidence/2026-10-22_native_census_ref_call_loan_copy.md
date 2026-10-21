# 2026-10-22: canonical census after a `ref` call's refused loan is copied where it is made

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`9f30e9c4b` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-22_ref_call_loan_copied_where_made.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `9f30e9c4b` | 3421, 1015, 906, 904 | 615 |
| copy where the loan is made | 3421, 757, 652, 650 | 500 |

Outside `direct_mir_*`: `ref` escapes through a call 149 -> 63 (other
`ref` escape labels 10 -> 1), moves 166 -> 155; no kind was added.
Inside `direct_mir_*`: 289 -> 150.

## How the cause was found

The escapes split three ways by where they are reported: 94 where a
loan a `ref` call returned leaves or its owner changes, 32 at a routine
whose own `ref` formal escapes through its callees, and 46 at a call
whose callee keeps the value. An instrumented run of the loan proof
showed nearly every failing loan has a result `Clone` copies whole; many
fail with no escape at all, because the owner walk admits only a `ref`
formal or a local no write reaches (an `inout` owner never passes it).
After the change: 37, 27 and 4.

## Fixtures (715 compared against `9f30e9c4b`)

- `borrowed_owner_loan_grown_landed_valid` (new): a loan of an `inout`
  owner grown through an `inout` call; refused before, one landing now,
  C and LLVM print `31`.
- `borrowed_owner_loan_grown_by_value_clone_invalid` (new): the same with
  an Env that `Clone` copies only by value; refused at 24:5.
- 27 negatives that checked such a loan's refusal now land one copy each
  and were renamed `*_landed_valid`: the `aggregate_string_loan_*` owner
  cases, `callee_alias_*`, `declared_array_borrow`, `default_string_*`,
  `result_frame_formal_*`, `return_loan_*` and
  `stdlib_tostring_string_forward`. 21 run with C and LLVM output equal
  and C with ASan and UBSan clean; the other 6 are checked to MIR (5 call
  an unlinked `extern "C"` Fill).
- No other fixture changed.

`return_loan_two_alias_assignment_landed_valid` prints `literal`: an
assignment through a `using` alias does not reach the aliased local on
either backend, already at `9f30e9c4b` (`using x as y` lowers to a `let`,
and a local `let` does not refuse assignment yet). Its output is not
pinned.

## Gates and mutations

`function-param-flow-summary` gate passed (189 s). `test-semantic` 3237,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the `test-mir`
target still exits 2 at its self-host driver step, as on main).

| Mutation | Caught by |
|---|---|
| no admission of a loan a Clone copies where it is made | `borrowed_owner_loan_grown_landed_valid` refused after its landing (20:22) |
| no landing selection for the refused loan | the same fixture ran 0 landings, refused at 21:5 |

## Measured and not taken

Treating the builtin positions that most often receive a formal's
storage with no relation as keeping nothing (`MapSet` receiver,
`ArrayPush` value, `ArrayPop`, `ReadFile`, `FileExists`, `WriteFile`,
`ArraySet` value) moved the census 904 -> 901: `ref` escapes -15, moves
+18. The largest chain, `SelfMirLower*FromArtifact` through
`SelfMirRoutineAtLocalCount`, pops Arrays of its by-value `build`, which
changes the caller's storage, so its move is the correct verdict, and
`Clone` copies a `SelfMirRoutineBuild` only by value.
