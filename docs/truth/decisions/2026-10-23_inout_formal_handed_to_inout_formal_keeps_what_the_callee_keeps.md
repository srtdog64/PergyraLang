# 2026-10-23: an `inout` formal handed to an `inout` formal escapes what the callee keeps

Decided by: the Claude lane. The user can overturn it.

## Rule

A routine that passed one of its formals to an `inout` formal of a
declared callee was summarised as letting the formal escape into a call,
whatever the callee did with it. When the argument is the routine's own
`inout` formal, named whole, the summary now takes from the callee's
formal summary what the callee keeps, as it already did when the
callee's formal is `ref` or by-value (`slot_analyzer_escape.c`). The returned-backing walk
likewise treats an alias written back through such an `inout` argument as
leaving the routine's `inout` input, as it did for a `ref` input
(`backing_inout_argument_leaves_input`).

A by-value or `ref` formal handed to an `inout` formal keeps the call
escape, and so does a member path or an element of the formal.

## Why it holds

The caller lent its `inout` place for change: the routine may set it,
grow it or rebind it, and so may a callee it hands the place to. What
reaches the caller through that place is what the caller lent; it is not
storage of the routine's that outlives the call. What the callee keeps of
the formal elsewhere, in another `inout` place, a channel or its result,
is its summary's call, channel or return escape, which the hand-off now
adds as before (`inout_defers_kept_by_callee_before_nested_call_invalid`).
An alias of the place that the callee changes is refused at its next use
or at the routine's exit (`inout_defers_alias_written_by_inout_callee_invalid`).

## Why

On the canonical driver input `EmitStmtList` and `ParseDeclsInto` recurse
with an `inout` Array of routine defers or declarations and hand it to the
nested call. The blanket call escape kept the recursive callee's formal
from being read as preserving its storage, so each Array argument to a
deferred consumer was refused as needing preserved storage
(`inout_defers_handed_to_recursive_emitter_valid`).

Replaces: nothing.
