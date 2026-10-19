# 2026-10-20: an unprovable getter loan is copied where the proof fails

Decided by: the Claude lane, after the user answered "whichever is better
long term" on the [open question](2026-10-20_open_escaping_loan_copy_or_move.md).
The user can overturn it.

## Rule

When a getter's loan of a by-value aggregate fails its proof, the compiler
selects a `Clone` at the place the proof fails. It does not move the
argument.

- **A loan that leaves the routine** (returned, kept by a callee, stored)
  is copied where it leaves. Only the value that leaves is copied.
- **A loan whose owner changes while it lives** is copied where it is
  made: the getter call's result becomes an independent `Clone`.

The selection happens after the flow proof and is visible in MIR as a
`Clone` call. A loan the proof admits is not copied. A value with no
`Clone` keeps the move.

## Why

- Copying the whole result at every failing call copies values that never
  leave: a fact struct is deep-copied when one String of it escapes. A copy
  at the escape costs only what leaves, so it uses less memory. That
  matches the [String landing](2026-10-19_string_landing_copy_then_move.md)
  rule: copy where no proof admits the store.
- A loan that must outlive a change of its owner cannot be fixed at the
  escape, because nothing escapes. Its only correct copy is at creation.
- Keeping the move leaves the unchanged canonical program refused, so V1
  would stay blocked.

## Cost on the record

The emitted code frees no String yet, so every copy stays allocated until
the process ends. A move landing or the cleanup model can take a copy back
where a measurement shows it matters.

Replaces: the OPEN question of 2026-10-20.
