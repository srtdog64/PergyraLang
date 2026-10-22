# 2026-10-23: handing what a ref input lends to an inout formal is no escape by itself

Decided by: the Claude lane. The user can overturn it.

## Rule

In a backing frame, a value carrying the frame's input handed to an
`inout` formal counted as a call escape of the input, whatever the callee
does with that formal. For a `ref` input it no longer does by itself:
what the callee keeps of the formal is still its summary's call or
channel escape, which the call adds as before. A `by-value`, `own` or
`inout` input keeps the call escape, and so does a loan proof.

## Why it holds

A local holds of a `ref` input only what the ref lends without a refusal
at the store, its Strings and scalars: `let names = index.labels` is
refused at the binding, a mutable aggregate pushed from a `ref` path is
refused at the push, and a `ref` path is no `inout` argument. So the
callee rewriting the argument changes nothing of the input, and the
frame's origins already follow what the argument holds. A callee that
keeps the formal through an `own` formal keeps the escape
(`ref_string_through_inout_keeper_invalid`). A by-value input shares its
caller's storage: an alias of its field written by an `inout` callee
changes the caller, so that call stays an escape
(`by_value_field_alias_written_by_inout_callee_landed_valid` prints the
caller's own value only because of it).

## Why

On the canonical driver input `MirExpressionIdentityEpochRebindGraphForArtifact`
puts Strings of its `ref routines` into two local Arrays and hands them
to `MirExpressionIdentityEpochDeclarationMemberNames(inout ...)`, which
only sets their elements. Seven callers up the MIR expression graph and
canonicalization chain were refused for `routines` and `admitted`.

## Left on the record

The store of `SemanticCallableCanonicalDeclaredName(...)` into those
Arrays has no independent-result proof. That check runs only in an
analysis with no earlier error, so the canonical census does not report
it yet; `ref_string_through_inout_reader_shallow_store_invalid` shows it.

Replaces: nothing.
