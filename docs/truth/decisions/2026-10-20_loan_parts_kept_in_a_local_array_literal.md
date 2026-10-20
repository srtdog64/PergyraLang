# 2026-10-20: a local Array literal may keep parts of a loan and grow

Decided by: the Claude lane. It refines the loan proof of the
[call-result rule](2026-10-20_call_result_holding_formal_storage_is_its_path.md).
The user can overturn it.

## Rule

A local the loan's backing reached must stay as unchanged as the owner.
A local declared with an Array literal (`let names: Array<String> = []`)
is an exception for one kind of change: a push or set whose receiver is
that local itself. Its descriptor was made by the literal, not taken from
the loan, so growing or setting it changes none of the owner's storage.
Assigning to it, passing it to an `inout` or `own` formal and every other
use are checked as before.

## Why

- The canonical driver input reads a getter's `Option<String>`, keeps the
  String in a local and pushes it onto Array literals in the same routine
  that uses the arena again (`SemanticAstStatementFactsFromArtifact`,
  `SemanticAstRoleFactsFromArtifact` and others). Each push counted as a
  change of the arena.
- A local that takes a path of the loan instead (`let items =
  part.items`) is not an Array literal, so a push onto it still changes
  the owner and the loan is copied where it is made.

Replaces: nothing.
