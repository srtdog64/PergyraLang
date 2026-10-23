# 2026-10-23: an Array handed to two keeping formals of one call is copied at the earlier ones

Decided by: the Claude lane. It applies
[a moved argument used again is copied](2026-10-22_moved_argument_used_again_is_copied.md)
inside one call. The user can overturn it.

## Rule

The deferred preservation owner refuses a call that takes an Array an
earlier handoff may keep or rebind. When every such earlier handoff is an
argument of the same call to a by-value formal, each of those arguments is
now selected for a copy where the call takes it
(`array_storage_deferred_preservation_owner.c`); the next analysis passes
`Clone` of it there when `Clone` copies the value whole, and the refusal
stands in this analysis as before. When another call keeps the Array too,
or an earlier result holding it has no ownership proof, nothing is
selected and the refusal stays
(`array_kept_by_earlier_call_and_two_formals_invalid`).

## Why it holds

Two formals that keep one Array would leave two holders of one buffer: a
push through one may free the storage the other names. A copy at each
earlier argument gives every holder its own storage, and the last argument
keeps the original, as a by-value parameter means.

## Why

On the canonical driver input `SemanticAstGenericSpecializationError` is
called with one empty Array for four of its formals, which its result
keeps, at two sites; each was refused as needing preserved storage
(`array_handed_to_two_keeping_formals_landed_valid`,
`array_handed_to_three_keeping_formals_landed_valid`).

Replaces: nothing.
