# OPEN 2026-10-21: copy a moved argument the caller uses again?

Asked by: the Claude lane. Not decided; the rule below is not applied.

## The case

The [default aggregate move rule](2026-10-18_default_aggregate_move.md)
moves a by-value aggregate argument when the callee changes, keeps or
releases its storage, and refuses any later use of it. On the canonical
driver input 270 refused uses, over all modules, are such moves
([census record](../evidence/2026-10-21_native_census_flow_sensitive_copies.md)),
the largest move front left. Most callees are codegen rewrite functions
that take a context by value and grow it.

## The choice

| | Keep refusing | Copy where the call takes it |
|---|---|---|
| What happens | the later use stays refused | when the caller uses the value again and `Clone` copies it whole, the next analysis passes `Clone(x)`; otherwise the use stays refused |
| Rule changed | none | the move rule's "any later use is refused" becomes "copied, else refused" |
| Cost | the canonical stays refused at these uses | one whole copy per such call, each time it runs |
| Consistency | as decided on 2026-10-18 | the same as the getter loans and local copies, which already copy where the proof fails |

The Claude lane recommends copying: it matches the value semantics of the
source model and the copy-where-the-proof-fails rule the user chose for
loans. The 2026-10-18 rule chose the move to avoid whole-array copies on
every functional update; `x = F(x)` still moves without a copy.
