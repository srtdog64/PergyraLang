# 2026-10-23 (OPEN): is an element write through a by-value Array formal a move?

Raised by: the Claude lane. Needs the user's decision.

## Question

A routine that takes an Array by value and hands it to an `inout` callee
that sets an element (`ArraySet`) changes the caller's Array: the formal
shares its buffer with the caller's binding, and the set writes the slot
in place.

```
func Mark(inout rows: Array<Int>) -> Int { ArraySet(rows, 0, 7); return 0; }
func Pass(rows: Array<Int>) -> Int { return Mark(rows); }
// Main: rows = [1]; Pass(rows); Print(rows[0])  -> prints 7
```

The [default aggregate move](2026-10-18_default_aggregate_move.md) says a
callee that changes a default-mode aggregate's storage in place moves it,
and a caller that uses it again takes a copy: under that reading Main
prints 1. The checker admits this today with no copy, and the type checker
refuses the direct form (`ArraySet` on a by-value formal).

## Why it is open

The canonical driver input relies on the sharing. The self-hosted
expression parser takes its cursor by value in `ParseExprFact`,
`ParseExprFactIn` and `ParseExpr` and reads the advanced cursor after the
call (`ParserExpressionPrecedenceGraphContractReady` checks `cursor[0]`).
A copy at those calls would leave the cursor where it was and change what
the parser does, and the canonical program is to stay unchanged.

It is not a memory-safety question: a set leaves the buffer where it was.
Pushes, which may free it, already move the formal
([rule](2026-10-23_by_value_array_handed_to_inout_formal_moves.md)).

## Options

1. Keep the sharing: an element write through a by-value Array formal is
   visible to the caller (today's behaviour). The move rule's wording
   would name buffer reallocation, not every in-place change.
2. Treat it as a move with a copy, and change the parser's cursor
   parameters to `inout` (the canonical program changes).
3. Refuse it, as the direct form is, and change the parser's cursor
   parameters to `inout`.
