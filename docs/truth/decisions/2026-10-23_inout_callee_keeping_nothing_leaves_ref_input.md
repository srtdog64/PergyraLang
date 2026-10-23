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

## Store landing remains separate

The inout escape rule does not itself admit a borrowed String store.  Where
the stored result has no independent-result proof, the existing String-store
landing owner must synthesize and record an exact `Clone` at the store.  The
integrated positive
`ref_string_through_inout_reader_shallow_store_landed_valid` checks that one
Clone reaches MIR and that C and LLVM execute the same result.  Removing that
landing must make the store fail closed again; the read-only inout summary is
not a substitute for it.

Replaces: nothing.
