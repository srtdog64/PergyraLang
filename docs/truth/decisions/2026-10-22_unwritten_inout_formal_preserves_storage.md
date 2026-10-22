# 2026-10-22: an `inout` formal the callee writes nothing through preserves the caller's storage

Decided by: the Claude lane. It narrows
[local copies refuse the other side after a change](2026-10-21_local_copies_refuse_the_other_side_after_a_change.md),
which counted every `inout` argument as a change. The user can overturn
it.

## Rule

The storage walk that publishes each formal's preservation verdict
([decision](2026-10-21_array_preservation_published_per_formal.md)) now
also walks an `inout` formal that is not an Array, through its access
paths as for a by-value or `ref` one, with one more rule: any assignment
through the formal or an alias of it fails the proof, because the formal
is the caller's place. A preserved `inout` formal is one the callee only
reads, as a `ref` formal is. An `inout` Array formal keeps its descriptor
walk, and an `own` formal is still not proved.

A call that hands a path to such a formal does not change the storage a
local copy of that path shares, so the copy's later uses are admitted.
Any other `inout` argument still counts as a change.

The same walk now reads a typed Map query's result (an Int `MapGet`, a
`MapKeys`) as new storage, as the backing frame already did: both
backends copy every key into the Array `MapKeys` returns (`strdup` in
`pgy_map_keys_*`), so `ArrayDropOwnedStrings(keys)` releases nothing of
the map.

Walking an `inout` formal's frame for the first time showed that an
assignment of the formal's own storage back to itself
(`rows = Rows(rows.values, ...)`) selected a copy, against the rule that
only another `inout` output takes one
([decision](2026-10-22_formal_storage_assigned_into_output_is_copied.md)).
The frame's own input is now excluded.

## Why

On the canonical driver input many routines copy fields of their `inout`
state into locals (`let sources = loans.map_payload_sources`), pass the
state to a helper declared `inout` that only reads it
(`SemanticAstMapStringPayloadInvalidated`), then change the locals and
write them back. Each such call refused every later use of the copies
(15 refusals in `SemanticAstMapStringPayloadAtCallCompletion` alone), and
no copy can land: `Clone` copies a HashMap only by value.

Fixtures: `local_copy_inout_read_only_callee_valid` (the helper walks
`MapKeys` of the state's map and drops them; admitted, prints `10`),
`local_copy_inout_callee_changes_invalid` (the helper sets an entry of the
shared map: refused at 18:12 and 19:19), and
`inout_self_assignment_lands_nothing_valid` (unchanged, no landing). Unit
test: the published verdict of a read-only `inout` formal is fully
preserved, of one whose map the callee sets or whose field it assigns is
unpreserved.

Replaces: nothing.
