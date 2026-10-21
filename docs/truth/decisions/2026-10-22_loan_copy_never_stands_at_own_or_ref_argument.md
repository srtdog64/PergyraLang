# 2026-10-22: a loan copy never stands at an `own` or `ref` argument

Decided by: the Claude lane. It narrows where
[an escaping loan is copied](2026-10-20_escaping_loan_copies_where_it_fails.md).
The user can overturn it.

## Rule

A loan that leaves through a call is copied where it leaves only when the
argument goes to a by-value formal. An `own` or `ref` boundary argument
must be a named variable, so a `Clone(argument)` there can never be
admitted; such a loan is copied where it is made, as one whose exit
cannot take a copy already is
([rule](2026-10-20_loan_copied_where_made_when_its_exit_cannot_be.md)).
An `inout` argument stays excluded: it is the caller's own place.

## Why

Before, the copy stood at any non-`inout` argument. At an `own` or `ref`
one the next analysis refused it with "Value boundary arguments ... must
use a named variable", a refusal no later analysis can clear. On the
canonical driver input that was 7 refusals
(`CanonicalizeMirJsonVerified`, `CanonicalizeOracleMirJsonBridge`,
`DirectMirCfgPlanFromAdmitted`); with the copy where the loan is made,
5 of them clear and 2 show the loan's actual escape
([census](../evidence/2026-10-22_native_census_loan_copy_carrier.md)).

Replaces: nothing.
