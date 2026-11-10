# 2026-10-23: a match case pattern is no change of a loan's owner

Decided by: the Claude lane. The user can overturn it.

## Rule

The proof that a loan's owner, or a local that carries the loan, stays
unchanged walks every call of the routine and refuses one with an
argument the type checker did not check. A match case pattern such as
`case DriverSourceCPayloadDenied(rejection):` is recorded as a call whose
argument is a new binding, so every loan of a routine with a payload match
was refused there. A call that is a match case pattern, or a call inside
one, is now no claim of the proof: the nearest case above it decides.

## Why it holds

A pattern evaluates nothing: it tests the subject's variant and binds
parts of its payload. The loan frame gives each binding the subject's
origins (`backing_match_bindings`), so a binding that takes part of a
loan carries it and has a proof of its own: a write through it is refused
(`loan_payload_bound_by_match_written_invalid`). The case's guard and body
are not patterns and stay claims.

## Why

On the canonical driver input `DriverRung2CliLogSourceCPayloadOrDie` and
`DriverRung2CliLogMirCPayloadOrDie` match on a payload admission that lends
their request; each had two refusals at the patterns. Three more routines
of the same shape (`DriverSourceCExecution.Compile`, `PublishMirCArtifact`,
`EmitDirectMir`) now pass the patterns and fail at the first call that
takes a pattern binding, whose storage root the proof does not resolve.

`enum_payload_constructor_ref_field_return_invalid` was refused only at its
patterns. Its loan, a `ref` input's fields returned in a payload while
`Main` keeps the input unchanged, is admitted as a struct result and as a
payload read without a match on the tree before the rule; it is now
`enum_payload_constructor_ref_field_return_valid`, and
`enum_payload_constructor_ref_field_owner_changed_landed_valid`, whose two
String fields are copied where `RefusalOf` constructs its result and therefore
remain valid when the case body replaces the input. The paired
`enum_payload_constructor_ref_field_uncopyable_owner_changed_invalid` keeps
the refusal when the payload also borrows nested `Array<Group>` storage.

Replaces: nothing.
