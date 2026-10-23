# 2026-10-22: a store of what a place already holds adds no call escape

Decided by: the Claude lane. The user can overturn it.

## Rule

A store into a place that shares an output's storage (a `ref` or `inout`
formal, or a local taken from one) adds the call escape only for origins
the place does not hold yet. The output map still takes every origin the
store carries. A value written back where it came from, as
`carriers.names = names` after `let names = carriers.names`, carries
nothing more of the frame's input or loan into an output, so a loan that
only travels with its holder stays a return of the routine's `ref` input.
A store that brings a new origin into an output's place still escapes:
`out.names = lent`, where `lent` is a local the `ref` index lent and `out`
is an `inout` formal that held nothing of index, is refused as before.

## Why it holds

A place's origins only grow, so a held origin can be one the place got
before it shared an output's storage. Such a place is a local: a `ref` or
`inout` formal takes no origin from a store (only locals and by-value or
`own` parameters are replaced), so a formal holds only its own backing.
A local holds its own value of the output it was taken from. A field
assigned on it stays in the local: after `tmp = out; tmp.names = lent`,
`out` is unchanged at run time. A change in place through it lands a copy
where it was taken. So a held origin hides no store into an output.

## Why

The canonical driver input refuses loans whose holder is written back
into itself, as `MirIntentRoutinePlanFromOwners` does with its
`routine_carriers.carrier_*` fields: each write-back counted as a new
escape of the owners the carriers lent.

Replaces: nothing.
