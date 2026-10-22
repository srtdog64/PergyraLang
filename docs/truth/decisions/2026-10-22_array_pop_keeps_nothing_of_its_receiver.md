# 2026-10-22: ArrayPop keeps nothing of its receiver

Decided by: the Claude lane. The user can overturn it.

## Rule

When a routine's backing frame follows a formal's storage into a builtin
call, `ArrayPop`'s receiver counts as used for the call only, as
`ArrayPush`'s receiver does through the builtin argument retention
registry. `ArrayPop` shortens its receiver in place and returns nothing,
so it adds no call escape to the formal it reaches. It still changes the
receiver's descriptor: the Array storage proofs do not read it as a
preserving use, and the collection ownership owner still decides whether
an owned String element may be popped.

## Why

The canonical driver input refused 63 `ref` formals as escaping through
a call (outside `direct_mir_*`). A census that named each frame gaining a
call escape (scratch instrumentation) traced the `SelfMirLower*` chain
to `SelfMirRoutineAtLocalCount`, which pops aliases of its by-value
`build`'s Arrays. With no registry row for `ArrayPop`, the frame failed
closed with a call escape on `build`, and every routine that handed it a
`build` lending its `ref input` refused that input.

The registry is projected into the self-hosted compiler
(`builtin_argument_retention_projection_owner.pgy`) and read by MIR loop
cleanup, so a row there would change the canonical program; the rule
lives in the backing frame instead. A first try that gave `ArrayPop` a
result relation sent its Void result through the result claim, which
failed closed and refused 37321 sites; it was reverted.

Replaces: nothing.
