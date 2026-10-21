# 2026-10-22: borrowed storage handed to a callee that changes or keeps it is copied there

Decided by: the user, on 2026-10-22, choosing the recommended answer.

## Rule

A routine's `ref` or `inout` parameter only borrows its value from the
caller, so it cannot move. When such a parameter is passed to a by-value
formal whose callee changes, keeps or returns the storage it receives
(for example `ErrorFact(code, message, symbolic_identifier_nodes)` storing
an `inout` Array in the returned struct), the next analysis passes
`Clone(argument)` at that call when `Clone` copies the value whole. The
callee then owns a copy. Otherwise the refusal ("Borrowed parameter
storage cannot be moved") stays.

A `ref` parameter that a read-and-return callee only lends on is not
copied: its loan is proved as before.

## Why

The same reason and place as
[a moved argument used again](2026-10-22_moved_argument_used_again_is_copied.md):
copy where the proof fails. On the canonical driver input 24 refusals are
of this kind; 14 are in `SemanticExpressionGraphDomainQueryValidateCall`.

Replaces: nothing.
