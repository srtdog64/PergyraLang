# 2026-10-21: copies of a path lent through the loan proof (not landed)

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`ce13eda6d` plus an uncommitted change that was then reverted.

## The hole

`main` (`7cd004fab`) and this branch admit each of these, and C with ASan
reports heap-use-after-free:

- `let other: Inner = outer.inner;`, pushes onto `other.items`, then a
  read of `outer.inner.items[1]` (and the same for `other = outer.inner`,
  `Outer(outer.inner, 1)`, `[outer.inner]` and
  `ArrayPush(held, outer.inner)` before pushes onto the copy);
- pushes onto `outer.inner.items` after the copy, then a read of the copy;
- the same inside a routine whose by-value formal is copied.

The source model says each read is an independent value
(`docs/semantics/27_ownership_clean.md` section 2.1), so every one of
these must either copy or be refused.

## What was tried

The move owner treated each such copy of a path as a loan of the path,
checked by the getter loan proof, and copied the path where it is made
when the proof failed. All ten new fixtures behaved: copies landed where
`Clone` copies whole, refusals where it does not, and the flow gate
passed.

## Canonical census

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).

| Run | Refusals | Rounds |
|---|---|---|
| `ce13eda6d` | 2066 | 3944, 2258, 2096, 2066 |
| copies lent | 46176 | 4780, 46339, 46176 |

- The first analysis refused 836 more uses. The loan proof is not
  flow-sensitive: any change of the source anywhere in the routine, even
  before the copy, fails every copy of it.
- After the first landing analysis one routine's parameter program-point
  index failed ("unavailable or stale"), the flow store failed, and every
  later verdict became fail-closed noise. Which landed copy broke the
  index is not known.
- The first run also crashed (SIGSEGV): a copy row that a later call moved
  kept no callee name. Fixed in the attempt before this measurement.

## What a rule needs

A copy invalidates the other side only when that side changes while the
copy is still used. The move owner already walks in execution order, so a
change through the source or the copy, followed by a use of the other,
is where the refusal or the copy belongs.
