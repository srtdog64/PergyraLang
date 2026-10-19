# 2026-10-19: a by-value getter call lends a formal that is used again

Decided by: the Claude lane. The user was asked the
[open question](2026-10-19_open_by_value_getter_lend_or_move.md) and
answered "whichever is better long term". The user can overturn it.

## Rule

A callee may only read a default-mode (by-value) aggregate argument and
return it or part of it: its summary has nothing but read and return.

- If the argument is a formal of the calling routine and the routine uses
  it again after the call, the call lends it, as a `ref` input lends. The
  later use is admitted only if the loan stays within the formal's
  lifetime (it stays in the routine, or the routine's own summary returns
  the formal too) and nothing changes the formal while the loan lives,
  the lending call included. Otherwise the later use is refused as a use
  of a moved value.
- If the argument is a local, or is not used again, the call moves it, as
  the [default-aggregate move rule](2026-10-18_default_aggregate_move.md)
  says. The result owns what it took: `return RowsError(code, ids)` hands
  a local into the returned facts.

A callee that changes, keeps or releases the argument still moves it,
whatever the caller does next. "Changes" includes a store through an
access path of the argument (`ArrayPush(arena.atoms, x)`,
`arena.blocks[i].ids`, `arena.atoms[0] = x`), which the Array storage
proof now checks for every by-value aggregate, not only an Array formal.

## Why a formal and not a local

A routine never hands its own formal on while it runs, so a loan of it
cannot meet a transfer in that routine. A local can: with
`let first = AtomText(arena, 0); let second = AtomText(arena, 1)`, the
last call hands `arena` to `second`, which owns it from then on, while
`first` still borrows it. Lending a local as well needs a rule that a
transfer is refused while any loan of the same owner lives; that is the
next step if the canonical census shows local getter refusals.

## Why per call

- Lend only would refuse every constructor-like helper that hands the
  caller's locals into a returned value. Only a move makes that sound.
- Move only refuses the most common use in the canonical program: reading
  a field of a by-value fact or arena through a getter, then using the
  value again. That was 1986 of the 3654 move refusals the canonical
  census showed with every final owner run.
- Rust makes the same split with `T` and `&T` in the signature. Here it is
  inferred from the callee's summary and the caller's later uses, so no
  annotation is written.

Replaces: the OPEN question of 2026-10-19.
