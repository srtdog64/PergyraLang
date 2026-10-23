# 2026-10-23: a complete frame decides RETURN for a routine whose result holds no storage

Decided by: the Claude lane. It takes up what
[a Clone reads a lent owner](2026-10-22_clone_reads_a_lent_owner.md)
measured and did not take, on the evidence below. The user can overturn it.

## Rule

The syntactic summary marks an input RETURN when its storage reaches any
call result, wherever that result goes. For a routine whose result is
`Void` or a physical scalar, that RETURN cannot be its own result, and
the routine's backing frame follows the call result to each output, call
or channel it reaches. When the frame is complete, its effects now
replace the syntactic RETURN for such a routine whatever its parameter
modes. Before, this held only for routines whose formals are all
by-value or `ref`; a routine with an `inout` or `own` formal kept the
syntactic RETURN, so `SemanticFutureJoin(inout target, ref branch)`
lent `branch` even when it stored only a copy of what it built from it.

## Why it holds now

On 2026-10-22 the same change admitted
`named_string_field_forward_project_drop_invalid`, a use-after-free: a
String of an Array kept in a node pushed into an `inout` output, then
the Array's Strings dropped. Now the frame sees that String reach the
output as a call escape: with the copy landing on that push turned off,
the drop is still refused. With the landing on, the node is a Clone and
the drop frees nothing it holds (ASan runs it clean, and
`ArrayDropOwnedStrings` does free). A node `Clone` copies only by value
gets no copy and stays refused.

## Why

On the canonical driver input four callers were refused for borrowed
`ref` values passed to such routines.

Replaces: the "measured and not taken" paragraph of
[a Clone reads a lent owner](2026-10-22_clone_reads_a_lent_owner.md).
