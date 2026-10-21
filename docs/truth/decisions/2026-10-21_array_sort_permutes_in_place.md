# 2026-10-21: ArraySort and ArrayReverse permute their receiver and return it

Decided by: the Claude lane. It models two stdlib calls from what C and
LLVM emit for them. The user can overturn it.

## Rule

`ArraySort(xs)` and `ArrayReverse(xs)` write `xs`'s elements in place and
return `xs` itself:

- C emits `({ pgy_array_sort_T(xs.data, xs.length); xs; })`, LLVM calls the
  runtime on the receiver's data and reloads the receiver;
- so the result's storage is the receiver's (a checked result relation
  that depends on the receiver and keeps nothing), and the call writes the
  receiver, as `ArraySet` does: either side of a local copy changes. A
  formal's path or alias handed to it already counts as a write of the
  formal (its slot summary and the alias rule), so its callers still see
  a move.

Before, a sort of anything but a private local Array literal was a call
the analysis did not model, so the frame of every formal in that routine
was unsupported, and that spread to every caller's frame.

## Why

- On the canonical driver input one such sort
  (`program_instruction_identity_owner.pgy:63`, on an Array an `inout`
  callee filled) was the only unmodelled call. It made 139 frames
  unsupported or incomplete, and the formals of routines with an `inout`
  output took a conservative call escape.
- The census drops from 2055 to 2027 refusals, 26 of them borrowed `ref`
  escapes ([census record](../evidence/2026-10-21_native_census_array_sort_modelled.md)).
- Main admits a sort through a local copy of a by-value formal's Array,
  and the caller sees its own Array sorted; a sort of an Array a local
  copied shows through the copy. The `*sort*` fixtures hold both.

Replaces: nothing.
