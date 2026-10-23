# 2026-10-23: a match payload binding's storage is rooted at the match subject

Decided by: the Claude lane. The user can overturn it.

## Rule

The loan proof resolves each argument and assignment target to the local
or formal whose storage it is: a `let` local is its own root, an alias
`let` takes its initializer's. A binding a match case pattern makes had no
root, so the proof refused at the first call that took one
(`DriverSourceCPayloadReceiptReadyFor(receipt, ...)`). Such a binding is
now rooted where the match subject is rooted, when a local or formal roots
the subject. A subject that is a call's result roots nothing the proof can
follow, and a binding of it still refuses.

## Why it holds

The loan frame gives a payload binding the subject's origins
([match patterns](2026-10-23_match_pattern_is_no_change_of_a_loan_owner.md)),
so it holds part of the subject's storage. A write through the binding is
then a write of the subject: when the subject carries a loan, the subject's
own proof counts it (`loan_payload_binding_written_by_callee_invalid`).
With no local behind the subject, neither the owner's nor a carrier's
proof could attribute such a write, so the binding keeps refusing; in
`loan_payload_binding_of_call_subject_written_invalid` the loan frame also
refuses the write, as a call escape of the loan.

## Why

On the canonical driver input `PublishMirCArtifact` matches on a payload
admission that lends its machine request and passes the receipt it binds
to `DriverRung2MirCPayloadReceiptReadyFor`; its two refusals failed there.
`DriverSourceCExecution.Compile` now passes its receipt and fails at an
assignment to `self.outcome`, whose root, the action's own subject, the
proof does not resolve; `EmitDirectMir` fails at a read-and-return call on
a local whose result leaves.

Replaces: nothing.
