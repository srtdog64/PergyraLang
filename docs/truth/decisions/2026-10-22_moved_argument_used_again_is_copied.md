# 2026-10-22: a moved argument the caller uses again is copied where the call takes it

Decided by: the user, on 2026-10-22, choosing the recommended answer to
[the open question](2026-10-21_open_copy_a_moved_argument_used_again.md).
The user also asked whether to change the canonical source or the V1
criterion instead; the Claude lane recommended keeping both and measuring
the copies first.

## Rule

The [default aggregate move](2026-10-18_default_aggregate_move.md) still
moves a by-value aggregate argument when the callee changes, keeps or
releases its storage. What changes is the caller's later use:

- When the caller uses the moved value again and `Clone` copies that
  value whole, the next analysis passes `Clone(argument)` at the moving
  call. The callee then changes or keeps its own copy, and the caller's
  value is untouched. The copy is visible in MIR as `Clone`.
- When `Clone` copies the value only by value (an Array of Arrays or of
  structs, a generic instance), nothing lands and the use stays refused.
- A last use moves without a copy, as before: `x = F(x)` reassigns the
  value from the result, so nothing uses the moved value again.
- Moves of the same path by different calls, as in two branches, each
  get their copy.

## Why

- It is the value semantics of the source model: a callee that changes a
  by-value argument does not change the caller's value, and a caller that
  reads the value again wanted the value it passed.
- It is the same contract as the getter loans, the local copies and the
  output stores, which already copy where the proof fails.
- Refusing would leave the unchanged canonical program refused at these
  uses, against the V1 criterion.

## Cost and what to measure

One whole copy per such call, each time it runs. Most canonical sites are
codegen functions that take a context by value and grow it; a copy there
may cost compile time. After landing, the self-host compile time and the
copies are measured against the current baseline; a site that costs too
much is brought back to the user (an `inout` signature is a change to the
canonical source, so it is an explicit exception to the V1 criterion).

Replaces: [the open question](2026-10-21_open_copy_a_moved_argument_used_again.md).
