# 2026-10-24: a refused loan copies its argument when its result cannot be copied

Decided by: the Claude lane. It adds a third place to
[a loan copied where it is made](2026-10-20_loan_copied_where_made_when_its_exit_cannot_be.md),
the place the user chose for a moved argument
([decision](2026-10-22_moved_argument_used_again_is_copied.md)).

## Rule

A by-value aggregate argument lent by a read-and-return call, whose loan
proof fails, is copied at the first of these places a copy can stand in:

1. where the loan leaves (unchanged);
2. where it is made, as `Clone(call)` (unchanged);
3. otherwise where the call takes the argument, as `call(Clone(argument))`,
   when `Clone` copies the argument whole.

The lent argument is recorded with the loan, as a moved one is. When none
of the three can take a copy and the loan's owner changed, the places the
loan leaves are copied
([chain decision](2026-10-24_refused_loan_copies_its_chain_in_one_analysis.md));
otherwise the use stays refused.

## Why

The third place has the meaning the user chose for a moved argument used
again: the callee receives a copy of its own, and the caller's value is
untouched. It covers a result that holds a value `Clone` copies only by
value, such as an Array of structs. On the canonical driver input,
`SemanticAstAssignmentTypeFactsContractReady` writes a node text through
`SemanticAstExpressionSurfaceFactsFromTypedArtifact(valid)`'s result and
then reads `valid` again (nine refused uses); that result holds an
`Array<SemanticAstBuiltinArgumentRetentionCallFact>`, so the old rule
copied nothing.

It also reaches calls that both return their argument's storage and grow
it in place (`AddBlock`, `TakeFirst` in the flow fixtures). Such a call
was recorded as a loan, so the moved-argument copy never applied to it,
and no copy can stand at its result (a Clone of the result keeps the
growth in the caller's storage). It now takes a copy of its own, which is
the value semantics the user's decision describes.

## Falsifiers

- `by_value_lend_result_written_argument_copied_landed_valid`: `Wrap`
  lends its box into a result that also holds an `Array<Pair>`; `Main`
  writes the result's box and reads its own. It lands `Wrap(Clone(box))`
  and prints `19` on C and LLVM. Without the rule the use is refused
  (`'box' was moved into 'Wrap'`).
- Five negatives of the default aggregate move become landed positives
  with the same programs (`default_aggregate_move_reuse_landed_valid` 14,
  `..._cloned_growth_landed_valid` 14, `by_value_getter_lend_growing_landed_valid`
  `ab1`, `..._branch_use_landed_valid` 3, `..._loop_reuse_landed_valid` 4,
  each with its `Clone` at the argument). The `_invalid` names keep the
  refusal for the same programs with an `Array<Pair>` field added, which a
  Clone copies only by value.
