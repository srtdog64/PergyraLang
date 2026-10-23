# 2026-10-23: canonical census after one storage in two parts of a value is copied

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`d30aaef8c` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_one_storage_in_two_parts_of_a_value_is_copied.md).

## The hole

On `d30aaef8c`, and on main's own build, `Make()` returning
`Pair(rows, rows)` was admitted with no copy; a caller that pushed into
`pair.first` and read `pair.second` read freed storage (ASan
heap-use-after-free on C).

## Canonical census

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `d30aaef8c` | 167 | 125 |
| first version | 1086 | 824 |
| parts compared by the path each names | 168 | 126 |

The first version followed a part through a local copy to the whole path
copied, so `instructions.ids` and `instructions.kinds` of a local copy of
`cfg.instructions` were one storage, and it selected only one earlier part
per overlap and round, so a value that repeats one Array more than three
times outran the three landing analyses: 699 refusals. The version kept
extends the copied path by the part's own path below the copy and selects
every earlier part that shares storage with a later one in one analysis.

Added: one, in `SemanticAstCollectionFormalEffectsFromResolvedFacts`, whose
error path passes one empty `HashMap` (`no_effects`) for two fields. `Clone`
does not copy maps whole yet, so no copy can stand in
([deep Clone request](../../agent_work_directives/hashmap_deep_clone_backend_2026-10-22.md)).
Every other repeated part on the input lands a copy. The census took 1027 s
and 3979376 KiB at peak.

## Gates

`function-param-flow-summary` gate passed (229 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 163, `test-rir` 44,
`test-hir` 58, `test-mir` units passed, 0 failed (the target exits 2 at its
self-host driver step, as on main). `aggregate_storage_transfer_smoke.sh`
fails 10 checks, 12 before
([open question](../decisions/2026-10-23_open_aggregate_smoke_negatives_copied.md)).

## Fixtures (811 compared against `d30aaef8c`)

Errors, landing analyses and `Clone` counts per routine changed only in new
fixtures: a copy lands where a returned constructor, a constructor through
a local alias and an Array literal repeat one Array (outputs 211, 211 and
22 on C and LLVM, ASan clean), and one map in two fields is refused.
Unchanged: two parts of distinct storage, and distinct fields of one local
copy.

## Mutations (237 fixtures)

| Mutation | Caught by |
|---|---|
| constructors are not checked | `one_storage_in_two_constructor_parts_returned_landed_valid` |
| literals are not checked | `one_storage_in_two_literal_elements_landed_valid` |
| local copies are not followed | `one_storage_through_alias_in_two_parts_landed_valid` |
| no copy is selected | the returned constructor, refused |
| a copied path is not extended by the part's path | `distinct_fields_of_a_local_copy_in_two_parts_valid` |
| sources beyond 32 are not shared | not caught |

The last stays as a guard: a part whose sources the walk cannot all
follow is taken to share storage.
