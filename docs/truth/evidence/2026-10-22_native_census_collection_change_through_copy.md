# 2026-10-22: canonical census after collection changes through a copy

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`f0bf3e6a1` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-22_collection_change_through_a_copy.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `64e69eb92` (same source as `f0bf3e6a1`) | 3404, 476, 385, 383 | 341 |
| rules 1 to 3 | 3405, 477, 386, 384 | 342 |
| rules 1 to 4 | 3400, 472, 381, 379 | 337 |

Rules 1 to 3 alone added one refusal, in
`SemanticAstCollectionExclusiveRecordReturn`, and nothing else changed.
Rule 4 removed it and four more in
`SemanticAstCollectionFocusRecoveryFromResolvedFacts`, all copies whose
change reached a value through the whole holder. Local copies whose other
side changed go 55 -> 51; no refusal was added.

## Programs the branch admitted

Each read freed storage (C with ASan: heap-use-after-free) or the other
side's change, on C and LLVM:

- a local copy of a `HashMap`, `Set`, `List` or `Queue` changed by any of
  the eight calls, then the source used (`MapSet` growth: C and LLVM
  panic `map key not found`; `QueuePush` growth: LLVM printed `18481`);
- an `Array<Int>` stored by `MapSet`, `ListPush`, `ListSet` or
  `QueuePush`, then grown, then read back through the collection;
- `let b = Clone(a)` of a `HashMap<String, Int>` grown past its capacity,
  then `a` read.

## Fixtures (727 compared against `f0bf3e6a1`)

Changed: `local_copy_inout_callee_changes_invalid` gains 13:19 (Probe's
own copy read after its `MapSet`). New: `local_copy_collection_changed_invalid`
(eight sites, one per call; the base admits it and prints
`1 false true false 1 2 2 1`), `local_copy_collection_changed_side_used_valid`,
`collection_value_store_grown_landed_valid` (one Clone in each of five
routines), `collection_value_store_shallow_grown_invalid`,
`local_copy_held_at_other_field_valid` and
`local_copy_held_at_field_changed_invalid` (four sites).

`tests/cases/backend_compare/empty_literal_collection_result` now reads
its List parameter's size before pushing onto a local copy; its C output
is unchanged (`8 1 3 11`). The C output of 1095 `backend_compare` cases
and examples is otherwise equal to `f0bf3e6a1`'s (40 fail to build on
both).

## Mutations

| Mutation | Caught by |
|---|---|
| each of the eight names dropped | its site in `local_copy_collection_changed_invalid` |
| each of the five stored values dropped | a Clone in `collection_value_store_grown_landed_valid` |
| a removal records its receiver as its own copy | `local_copy_collection_changed_side_used_valid` refused at 18:19 |
| the carrier side changes whatever path of the holder changed | `local_copy_held_at_other_field_valid` refused |
| a change through the value refuses the whole holder | the same, and the invalid fixture's message names `facts` |
| either half of the path overlap dropped | 43:20 or 51:24 no longer refused |
| an assignment or a collection store records the whole holder | the valid fixture refused, or one Clone landed |

The checks on the published row (stdlib, builtin, declaration, value
binding) survived every fixture and were removed: the eight names are
reserved, so no program declaration can take one.

## Gates

`function-param-flow-summary` gate passed (170 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the `test-mir`
target still exits 2 at its self-host driver step, as on main). C with
ASan is clean on both landed or admitted collection fixtures.

## A deep HashMap Clone on top (checker only, reverted)

Calling a `Clone` of a `HashMap` with String or scalar keys and values
independent, as [the backend request](../../agent_work_directives/hashmap_deep_clone_backend_2026-10-22.md)
would allow: 3400, 472, 329, 327. Outside `direct_mir_*`: moves 102 -> 85,
local copies whose other side changed 51 -> 17, borrowed storage handed on
1 -> 0. No backend emits it yet.
