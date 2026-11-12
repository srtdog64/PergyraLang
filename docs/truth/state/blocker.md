# Current blocker

Owner: Claude root lane. Checked 2026-11-12 on `ci/all-green` after
`c7e599f58`.

## Objective card

- Objective: required CI green on one published commit, with the bootstrap
  and installed-driver jobs executed.
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. The native compiler admits 28 negative fixtures that fail under ASan
   ([record](../evidence/2026-11-12_native_admits_unsafe_negative_fixtures.md),
   minus the 12 MapGet views fixed in `0268e6330`), and
   `focus_recovery_scope_break_continue` frees a bad pointer natively.
2. Self-host generality gaps (native admits and runs clean, the self-host
   refuses): eight inout gate positives
   ([class 6](../evidence/2026-11-12_inout_effect_mismatch_classes.md)) and
   five former ownership negatives native now lands as owned copies
   (`empty_mixed_string_push_without_drop`, `map_keys_shallow_{push,set}`,
   `ast_graph_atom_late_owned_growth_invalid`, `collection_field_owned_push`).
3. The self-host MIR route carries no compiler `Clone` (focus part copies,
   constructor and mutation copy landings); only its semantic C route does.
4. Contracts 3: identity cell probe build (310 s) over its 120 s limit.
5. Core 5 (direct-MIR C with an unused routine), 6, 24; backend compare 4, 7;
   bootstrap `self-host-array-index-type` (a `ref Slice` element write that
   both front ends refuse). All red at `1acc31c5b` too.

## Done since `c7e599f58` (local, not yet on CI)

- A field copy an exit leaves unassigned is a copy
  ([decision](../decisions/2026-11-12_unwritten_field_copy_is_a_copy.md)):
  the self-host takes a Clone where the part is defined and no longer writes
  it back; its emitted C now prints what native prints on every focus input
  both admit. The canonical driver stays admitted (1,310 s).
- Self-host refusals added: a grown Map/Set/List/Queue receiver whose copy is
  read later; a String element loan a call hands to a struct; the defer
  regression of `13dea684c`.
- collection_inout_effect, formal key, focus recovery and the other
  contracts step-4 gates pass locally on the side tree (the inout gate had
  59 verdict mismatches at `1acc31c5b` and had never run past them); the
  ownership semantic gate is checked through the installed-driver steps on
  CI only.

## Falsifiers kept

- A drop on one branch still refuses the read after the join.
- Own transfers of storage a constructor kept stay refused natively.
- The zone/world value-carriage mutant is refused on both MIR backends.
