# 2026-10-22: a formal's storage assigned into an output is copied where it is assigned

Decided by: the Claude lane. It extends
[the Array store rule](2026-10-20_formal_storage_kept_by_output_is_copied.md)
from `ArrayPush`/`ArraySet` to assignments. The user can overturn it.

## Rule

An assignment may put a value that carries a by-value or `ref` formal's
backing into another formal that is `inout`: the whole formal
(`target = value`), a field (`target.verdict = value`) or an element.
When `Clone` copies that value whole, the next analysis assigns
`Clone(value)` instead, and the output keeps nothing of the formal.

- The formal's backing frame selects the assigned value after the flow
  proof, and the next analysis wraps it in `Clone` by its syntax identity,
  as a loan copy is wrapped. It is visible in MIR as `Clone`.
- A value `Clone` copies only by value (an Array of structs, a generic
  instance) lands nothing, and the refusal stays.
- Only a by-value or `ref` formal has a backing frame, so an `inout`
  formal assigned from its own storage (`rows = Rows(rows.values, ...)`)
  lands nothing. Loan walks keep their own carrier landing.
- A local assigned back (`out.names = names` after pushes into `names`)
  lands nothing: it got the formal's backing from a store into it, which
  is where a copy stands, and a `Clone` there would end the write-back.

## Why

`SemanticFutureJoin(inout target, ref branch)` in the canonical driver
input keeps `branch`'s diagnostic Strings in `target` with
`target.verdict = SemanticAstBodyVerdictError(branch.verdict.code, ...)`.
That is a real escape: `target` would hold Strings of `branch`. The Array
store rule copied such values for pushes and sets only, so the assignment
stayed an escape of `branch`, and every caller that handed its own `ref`
formal on was refused. Copying at the assignment is the same place and
reason as the Array store copy. On the canonical input it cleared 15
refusals and added none
([census](../evidence/2026-10-22_native_census_assigned_output_copy.md)).

## Cost on the record

Each copy stays allocated until the process ends while emitted code frees
no String, as with the other landings.

Replaces: nothing.
