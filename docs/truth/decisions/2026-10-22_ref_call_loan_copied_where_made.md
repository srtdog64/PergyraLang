# 2026-10-22: a `ref` call's loan that no proof admits is copied where it is made

Decided by: the Claude lane. It extends
[a loan is copied where it is made when its exit cannot be](2026-10-20_loan_copied_where_made_when_its_exit_cannot_be.md),
which covered getters of by-value aggregates, to calls that lend a `ref`
input, in the direction the user chose for
[borrowed storage handed on](2026-10-22_borrowed_storage_handed_on_is_copied.md).
The user can overturn it.

## Rule

A call that hands a value to a `ref` formal whose callee returns its
storage lends that storage through the result
([results lend `ref` inputs](2026-10-19_results_lend_ref_inputs.md)). The
value may be a local or the routine's own `ref` or `inout` parameter. When
the caller's proof of the loan fails, the refusal stands in this analysis
and the next analysis takes the call as `Clone(call)`, when `Clone`
copies the call's result whole and the call changes no storage it shares
with its caller. A loan whose call is the argument of such a `Clone` ends
there: nothing of the owner is shared after the copy, so the proof admits
it without walking the owner. Otherwise the refusal stays.

The copy stands where the loan is made, not where it leaves. The owner
walk admits only a `ref` formal or a local no write reaches, so for an
`inout` owner, or a local changed while the loan lives, a copy at the exit
would leave the same refusal. For a String loan that leaves at once
(`return EchoText(record.text)`) both places copy the same String.

## Why

On the canonical driver input most refused `ref` escapes are such loans:
`let block_env = CodegenTypeEnvStateBorrow(env_state)` handed to the
nested `EmitStmtList` as its `inout env_state` (9), and the Strings graph
getters return (`SemanticExpressionGraphNodeText`,
`SemanticExpressionGraphCallTargetName`) that the routine returns or
stores.

27 flow fixtures that checked the refusal of such a loan (a local owner
returned, replaced, moved or kept; a loan written to a slot, returned
through an `inout` output or an alias) now land one copy and run; their
first analysis still refuses, which the gate's landing count checks.

## Cost on the record

The copy costs the whole result once per call (for `EmitStmtList` the
local rows text of each nested block). As with the other landings it
stays allocated until the process ends while emitted code frees no
String. The self-host compile time is measured once the canonical program
is admitted.

Fixtures: `borrowed_owner_loan_grown_landed_valid` (one landing, prints
`31` on C and LLVM), `borrowed_owner_loan_grown_by_value_clone_invalid`
(the result also borrows `env.groups`, whose nested Array storage is not made
independent by the String-field copy: refused before MIR at the constructor
store and retained through the routine/caller summaries).

Replaces: nothing.
