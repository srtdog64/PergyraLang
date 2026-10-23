# 2026-10-22: canonical census after an inout callee changes only the fields it writes

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`8c1611760` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-22_inout_callee_changes_only_the_fields_it_writes.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `8c1611760` | 3420, 443, 352, 350 | 308 |
| no `inout` call changes a copy (unsound bound) | | 275 (317 total) |
| changed fields only | 3388, 415, 324, 322 | 280 |

Outside `direct_mir_*`: local copies whose other side changed 51 -> 28,
`inout` places refused at an exit 6 -> 1. No refusal was added. Of the
33 the bound allows, 28 were admitted.

The same census run back to back on an otherwise idle machine took
1011 s and 3977368 KiB at peak on `8c1611760`, and 953 s and 3977456 KiB
with the field verdicts: no cost added by the field walks.

## Gates

`function-param-flow-summary` gate passed. `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main). C with ASan is clean on the
new valid fixture (`0 3`).

## Fixtures (735 compared against `d531726e2`)

New: `inout_callee_other_field_changed_valid` (refused on `8c1611760` at
28:20, admitted now; `0 3` on C and LLVM) and
`inout_callee_same_field_changed_invalid` (five sites: a direct
`MapSet`, a local copy assigned back, a nested `inout` call, a field
assignment, a whole assignment; refused on both trees).
`local_copy_held_at_field_changed_invalid`'s whole-holder routine passed
its `inout` facts to a callee that writes only another field; it now
moves a local holder into a by-value callee that writes through it, so
the change still reaches the whole holder (48:20).

## Mutations

| Mutation | Caught by |
|---|---|
| another field's path counts as the formal's root | `inout_callee_other_field_changed_valid` refused at 28:20 |
| another field's path is walked as an ordinary use | the same |
| a nested call taking the whole formal uses the callee's whole verdict | the same |
| every field of the struct counts as changed | the same |
| no field counts as changed | `local_copy_inout_callee_changes_invalid` loses 19:12 and 20:19 |
| the other-field test compares nothing | `inout_callee_other_field_changed_valid` refused |

## The mutation runner, corrected

The runner restored the tree with `rsync`, which sets a file's time back
to the source's, then touched only the file the next mutation edits. An
object built from an earlier mutation of another file was newer than its
restored source and was not rebuilt, so a mutation could run with the
previous one still in place. The runner now touches every file any
mutation edits after each restore. With it, all 34 mutations recorded for
`eec39bb63`, `5c4dd0fd4`, `d531726e2`, `8c1611760` and this change were
run again: every one is still caught.
