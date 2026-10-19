# 2026-10-19: a callee that returns a String element lends it

Decided by: the Claude lane, extending the
[results lend `ref` inputs](2026-10-19_results_lend_ref_inputs.md) rule to
String elements. The user can overturn it.

## Rule

A String element of an Array goes to a declared callee whose summary for
that formal only reads it or returns it, alone or inside its result. No
other argument can change an Array during the call. The call then lends
the element to its result, and is admitted when the loan proof holds:

- the result's loan stays in the caller, or leaves it only as the caller's
  own formal returns;
- nothing in the caller changes the Array while the loan lives: no
  assignment to it, no writing builtin (`ArraySet`, `ArrayPush`), no call
  that moves, keeps or releases it.

A callee that keeps the String or stores it anywhere else is refused as
before.

## Why

Until now only a callee that read the element without returning it was
admitted. `string_element_returned_by_named_call_invalid` pinned the
returned case as refused when no proof route existed (2026-10-18). Its
program (`let kept = Kept(names[1]); Print(kept)`) keeps the loan inside
the routine while `names` is unchanged, which is what the aggregate loan
route already admits. The fixture is now `_valid`, and two negatives pin
the cases that stay refused: the loan returned past the Array, and the
Array changed while the loan lives.

The CI test harness manifest needed this rule: it hands each inventory
path to `CompilerCompletenessProgramCheckTarget`, which returns it inside a
`CompilerCompletenessCheckTarget` read in the same pass.

Replaces: nothing. The read-only rule is unchanged.
