# 2026-10-22: canonical census after a returned alias lends and a copy under a loan copies it

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`3cdb56256` plus the change measured (local, not pushed). Rules:
[returned alias](../decisions/2026-10-22_returned_alias_is_the_formals_return.md),
[copy under a loan](../decisions/2026-10-22_copy_changed_under_a_loan_is_copied.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `3cdb56256` | 3463, 1064, 958, 956 | 667 |
| both changes | 3421, 1015, 906, 904 | 615 |

Outside `direct_mir_*`, moves fell 217 -> 166 and borrowed-storage moves
2 -> 1; no kind was added. The copy-under-loan check refuses nothing on
the canonical input.

## How the cause was found

A census with move kinds (MK) split the 217 moves: 137 getter loans whose
proof failed, 44 read-only summaries with unproved storage, 36 real
changes. Storage leaves (SLEAF, STOR) put the 44 at one routine,
`SemanticAstExpressionSurfaceFactsWithGraph`, whose `graph` goes through
`let sealed = graph` into the returned facts: the storage walk ran before
the backing frame proved that return, saw a returned alias with no RETURN
in the summary, and marked the storage unproved for good.

## The use-after-free

Checking the lend on a grown copy found a hole older than this branch's
recent work: a getter's loan (`facts = WithGraph(graph)`), then a local
copy of the source path (`var grown = graph.rows`) grown four times, then
a read of the loan. `4f7692f38` and `3cdb56256` admitted it; C with ASan
reports heap-use-after-free, and the plain C and LLVM builds dump core.

## Fixtures (713 compared against `3cdb56256`)

- `returned_alias_lends_formal_valid`: refused as a move before, admitted
  with no landing, prints `22`.
- `returned_alias_lends_formal_grown_invalid`: the grown copy under the
  loan, Array of structs, refused with the new diagnostic.
- `lent_path_copy_grown_landed_valid`: the same with clonable rows; one
  landing copies the loan where it is made, prints `75`, ASan clean.
- `default_aggregate_move_field_alias_returned_invalid` and
  `match_payload_return_moves_invalid` returned a local alias or a match
  payload; they now lend, the loan is copied where it is made, and each
  became `*_landed_valid` (one landing, prints `4` and `a`, ASan clean).
- `output_field_store_of_ref_formal_by_value_clone_invalid` stays refused
  with borrow-escape diagnostics only (one moved to the callee).
- No other fixture changed.

## Gates and mutations

`test-semantic` 3237, `test-transpile` 1014, `test-dir` 15, `test-air`
161, `test-rir` 44, `test-hir` 58, `test-mir` units 691 passed, 0
failed; the flow summary target passed (the `test-mir` target still
exits 2 at its self-host driver step, as on main).

| Mutation | Caught by |
|---|---|
| storage walk without the settled summary | `by_value_getter_result_local_returned_valid` refused |
| no check of a copy changed under a loan | `lent_path_copy_grown_landed_valid` ran 0 landings |
