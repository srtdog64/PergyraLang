# 2026-10-23: safe aggregate copy landings are positive cases

Decided by: the GPT root lane while integrating the native checker work.

## Decision

Keep the adopted compiler-inserted copy rules. The following aggregate
storage cases are positive programs because the emitted `Clone` gives the
two resulting owners independent storage before either can grow it:

- `duplicate_array_landed_valid`;
- `array_alias_landed_valid`;
- `push_pop_duplicate_landed_valid`;
- `post_field_push_landed_valid`.

The C and LLVM paths must compile and execute these fixtures with their
declared output. This is the behavioral gate; copy admission by itself is
not evidence that the copy is independent or eventually released.

`map_alias_negative` remains a negative because `Clone` does not yet copy a
map deeply. `post_field_return_negative`, `nested_overlap_move_negative`,
and `owned_string_set_stale_borrow_negative` remain ownership negatives at
their named hazards.

## Reason

The repository already adopted copy-on-later-use for a moved aggregate and
the current aggregate storage gate had migrated these four fixtures to
runtime positives before this decision was integrated. Reverting them to
source-level refusals would create a second policy for the same ownership
boundary. The positive runtime checks preserve the actual invariant: no two
live owners may observe one growable backing store.

This resolves the former OPEN question recorded by the native-checker lane.
