# 2026-10-22: canonical census after an inout place is checked at each exit

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`eec39bb63` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-22_inout_place_changed_through_a_copy.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `eec39bb63` | 3400, 472, 381, 379 | 337 |
| exits checked | 3420, 478, 387, 385 | 343 |

Six exit refusals were added, none removed:

- `SemanticAstCollectionFocusRecoveryFromResolvedFacts` (five exits):
  `facts.exclusive_copies = values.copies` shares the `inout` formal's map
  with the result, and `facts` then goes to the `inout` callee
  `SemanticAstCollectionFocusRecoveryAppendExit`. No fact says which
  fields an `inout` callee changes, so the whole `facts` counts as
  changed. A deep `Clone` of `HashMap<String, Int>` would copy the map
  where it is assigned.
- `SemanticAstAnalysisResolveCallReturnTypes` (the last `return false`):
  `graph`, a copy of `analysis.expression_surfaces.expression_graph`, hands
  its arena's parts to `SemanticExpressionGraphArenaFromTopologyWithIdentities`,
  which returns them in a new arena. On the path that refuses the new
  surface, the caller's analysis keeps storage that local arena holds.

## Programs the branch admitted

`formal_field_copy_changed_at_exit_landed_valid` before the rule: `Grow`
binds a local to its `inout` formal's `Array<Int>` field, pushes forty
values and returns; `Main` then reads its own field (C with ASan:
heap-use-after-free). The same with a `HashMap<String, Int>` field.

Three routines of `local_copy_held_at_other_field_valid` as `eec39bb63`
committed it were of this kind: each changed a copy of an `inout`
formal's field, or a value assigned into one, and returned. The fixture
now builds each struct as a local, so it checks only the holder's path;
the invalid fixture's `inout` routines gain the two exit refusals 28:5 and
36:5.

## Fixtures (730 compared against `eec39bb63`)

Changed: `local_copy_held_at_field_changed_invalid` (28:5, 36:5 added).
New: `formal_field_copy_changed_at_exit_invalid` (14:29 and 15:5 at two
returns, 18:6 at the end of a body), `formal_field_copy_changed_at_exit_landed_valid`
(one Clone in `Grow`; `41 8` on C and LLVM, ASan clean) and
`formal_field_copy_stored_back_valid` (assigned back, only read, by value,
`own`, only lent).

## Mutations

| Mutation | Caught by |
|---|---|
| no check at a return | 28:5 and 36:5 lost |
| no check at the end of a body | 18:6 lost |
| every formal but `own` checked | `ByValue` refused at 34:5 |
| every formal checked | the same |
| copy rows counted as changes | `local_copy_inout_callee_changes_invalid` refused at 13:5 and 18:28 |

A check that the row has no callee survived: an `inout` formal is never
moved or lent, both are refused where they are made, so it was removed.
The six holder-path mutations of `eec39bb63` were run again on the
rewritten fixture and are all caught.

## Gates

`function-param-flow-summary` gate passed (165 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main). C with ASan is clean on the
landed, stored-back and holder-path fixtures. The C output of 1095
`backend_compare` cases and examples equals `eec39bb63`'s.
