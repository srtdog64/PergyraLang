# 2026-10-20: a formal's storage an output keeps is copied where it is stored

Decided by: the Claude lane. It extends the rule the user chose for the
[String landing](2026-10-19_string_landing_copy_then_move.md) and the
[escaping loan](2026-10-20_escaping_loan_copies_where_it_fails.md) (copy
where the proof fails; the user asked for the long-term choice). The user
can overturn it.

## Rule

An Array store (`ArrayPush`, `ArraySet`) may put a value that carries a
by-value or `ref` formal's backing into an `inout` output. When `Clone`
copies that value whole, the compiler stores `Clone(value)` instead.

- The formal's backing frame selects the store after the flow proof. The
  next analysis lands it, visible in MIR as `Clone`, or as
  `ArrayPushOwnedString` for a String pushed into an Array that admits an
  owned push.
- The output then keeps nothing of the formal, so the call does not move
  the caller's argument.
- A value `Clone` copies only by value (an Array of elements that are not
  physical scalars or Strings, a generic instance) lands nothing, and the
  move stays.
- Only the stored value is copied, never the formal.

## Why

- It is the same place and the same reason as the String landing and the
  loan copy: the formal's storage leaves the routine through the store,
  and no proof admits it.
- On the canonical driver input the landing cleared 237 refusals where
  the callee only kept a copyable value in an output, such as parser
  source locations and collected names
  ([census](../evidence/2026-10-20_native_census_formal_store_landing.md)).
  Copying at the store costs the stored value once. Keeping the move would
  leave the unchanged canonical program refused.
- The move stays for a callee that changes the caller's storage in place
  or returns it: a copy at a store cannot undo that.

## Cost on the record

Like the other landings, each copy stays allocated until the process ends
while emitted code frees no String.

Replaces: nothing. It narrows the
[default aggregate move](2026-10-18_default_aggregate_move.md) for the case
of a callee that keeps a copyable value in an output.
