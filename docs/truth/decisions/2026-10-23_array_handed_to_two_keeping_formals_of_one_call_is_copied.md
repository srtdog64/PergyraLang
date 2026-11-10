# 2026-10-23: an Array handed to two keeping formals of one call is copied at the earlier ones

Decided by: the Claude lane. It applies
[a moved argument used again is copied](2026-10-22_moved_argument_used_again_is_copied.md)
inside one call. The user can overturn it.

## Rule

The deferred preservation owner selects every non-final keeping handoff for
a copy, including a handoff in an earlier call and earlier by-value arguments
of the final call (`array_storage_deferred_preservation_owner.c`). The next
analysis passes `Clone` at each selected handoff only when `Clone` copies the
whole value independently; the final handoff moves the original storage.
If any required whole-value copy is unsupported, the existing preservation
or released-storage refusal remains. An earlier result holding the Array with
no ownership proof also remains refused.

## Why it holds

Several keeping handoffs of one Array would leave several holders of one
buffer: a push through one may free the storage the others name. A copy at
every non-final handoff gives each retained value its own storage, and the
last handoff keeps the original, as a by-value parameter means. The positive
gate `array_kept_by_earlier_call_and_two_formals_landed_valid` requires one
Clone in the earlier callee and one in the final caller; the uncopyable
aggregate gates keep the fail-closed boundary.

## Why

On the canonical driver input `SemanticAstGenericSpecializationError` is
called with one empty Array for four of its formals, which its result
keeps, at two sites; each was refused as needing preserved storage
(`array_handed_to_two_keeping_formals_landed_valid`,
`array_handed_to_three_keeping_formals_landed_valid`).

Replaces: nothing.
