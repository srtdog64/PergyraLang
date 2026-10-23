# 2026-10-23: a String push the owned push refuses lands as a Clone

Decided by: the Claude lane. It extends
[a String that leaves its Array owner is copied where it crosses](2026-10-22_string_crossing_copied_where_it_crosses.md),
which keeps `ArrayPushOwnedString` where that operation exists, to the
pushes it refuses. The user can overturn it.

## Rule

A push of a String that no owner proves alive for the receiver becomes
`ArrayPushOwnedString`. That operation refuses a receiver without
release authority (an Array bound from a field, `let names = rows.names`)
and a receiver whose elements are borrowed or of unknown ownership. The
next analysis then pushes `Clone(value)` with a plain `ArrayPush`, for
any String value. Before, only a String path of a by-value or `ref`
formal landed this way; a local String
(`let owner = analysis.names[i]; ArrayPush(names, owner)`) and any other
value stayed refused.

## Why it holds

The Clone is a fresh String that only the receiver holds. A plain push
adds it as the receiver's other elements are held, so the receiver's
cleanup policy is unchanged and no release covers the copy: it is never
freed while held. At worst it stays allocated, as every landed copy does
while emitted code frees no String.

## Why

On the canonical driver input 23 owned pushes were refused this way:
the self-host builders take an Array out of a fact record, push Strings
that other facts lend, and assign the Array back. Each refusal also
made its routine's flow summary fail closed, which refused the routine's
callers for borrowed `ref` escapes.

Replaces: nothing.
