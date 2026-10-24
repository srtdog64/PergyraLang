# 2026-10-25: the ownership direction is re-evaluated on 2026-11-08

Decided by: the user, on 2026-10-25. The current direction stays:
compiler-inferred ownership, with no lifetime annotations and no GC or
reference counting. The user rejected switching to a restricted memory
model like Goose's. The user also noted on 2026-10-24 that the ownership
finish line had not closed in two weeks. This decision fixes when and how
that is judged again.

## Rule

- Until 2026-11-08 the direction is unchanged. The GPT root lane goes on
  with the native checker and release synthesis, and may use
  [explicit Clones at real sharing sites](2026-10-25_real_sharing_site_may_take_explicit_clone.md).
- First, one census. Today the two measurements differ in tool, mode and
  tree:
  - the GPT fixed final-owner census: 309 on `4716f9539`,
    [blocker](../state/blocker.md);
  - the Claude census: 91 on `707cbfc24`,
    [record](../evidence/2026-10-24_native_census_lent_argument_copy.md).

  One of them, or a replacement, is named in
  [native_checker.md](../state/native_checker.md) with its mode and
  revision before progress is counted.
- On 2026-11-08 the GPT root lane adds an evidence file. It gives, on one
  named revision:
  - the refusals on the fixed DRV-2 input, measured with that census;
  - the explicit Clones written under the Clone decision, with their sites;
  - the `ArrayDrop(` calls left in `src/self_hosted` (144 on
    `4ddb879e7`);
  - whether emitted code frees an owned value after its last use. On
    2026-10-25 it frees nothing on `ArraySet`, reassignment or a move
    ([native_checker.md](../state/native_checker.md)).
- If the refusals are zero, V1 goes on to its remaining finish-line items.
- If they are not, the direction goes back to the user as an OPEN
  decision, with those numbers. The alternatives named on 2026-10-25 are
  inputs to that file and are not decided here:
  - region lifetime for compiler pass or routine data
    ([docs/197](../../197_region_arena_strategy.md));
  - generational Slot handles where lifetimes do not nest.
- Nothing switches automatically, and the date moves only when the user
  moves it.

## Why

With a fixed date and fixed measurements, the work cannot drift on as
"a little more" without anyone deciding. The census comes first because
the two tools disagree by a factor of three, and a deadline judged on an
unnamed number decides nothing.
