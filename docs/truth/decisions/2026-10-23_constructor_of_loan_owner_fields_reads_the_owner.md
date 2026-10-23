# 2026-10-23: a constructor of a loan owner's fields reads the owner

Decided by: the Claude lane. The user can overturn it.

## Rule

The proof that a loan's owner stays unchanged
([lent arguments](2026-10-23_loan_ends_with_the_statement_that_holds_its_locals.md),
returned loans) refused any call that takes a path of the owner and has
no callee formal, a struct constructor among them. A struct constructor
whose value stays in the routine is now a read of the owner: the loan
frame of that value adds no escape, so it is not returned, kept, sent,
passed to an `own` or `inout` formal, or passed to a formal whose storage
proof fails, as a formal the callee writes. Any other call without a
formal, an in-place `ArrayReverse` among them
(`loan_owner_permuted_in_place_invalid`), still refuses.

## Why it holds

A constructor stores its arguments in the value it returns and writes
none of them. That value shares the owner's storage, so a change through
it changes the owner: the local copy rule records the value as a copy of
each path it was built from
([rule](2026-10-21_local_copies_refuse_the_other_side_after_a_change.md)),
and a change of the copy while a loan of the path lives is refused where
it is made ([rule](2026-10-22_copy_changed_under_a_loan_is_copied.md);
`loan_owner_constructor_copy_changed_invalid`). A callee that writes the
value, or a result that takes it out, is outside that rule's sight, which
is why the value must stay
(`loan_owner_constructor_copy_written_by_callee_invalid`,
`loan_owner_constructor_copy_returned_invalid`).

## Why

On the canonical driver input `SemanticAstExpressionTypedBindingContractReady`
lends a base artifact to its surface facts, then builds two artifacts from
the base's fields and lends each in turn. Its 14 refusals (`'base' was
moved into 'SemanticAstExpressionSurfaceFactsFromTypedArtifact'`) failed at
those constructors. The same rule limited to lent arguments measured the
same; a rule taking every constructor argument as a read, with no check
on where the value goes, removed these 14 and added 18 elsewhere on
2026-10-23 (census at `d769a84db`).

Replaces: nothing.
