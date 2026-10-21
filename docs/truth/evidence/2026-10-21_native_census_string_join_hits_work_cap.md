# 2026-10-21: modelling StringJoin hits the flow work cap (not landed)

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`9b3ed0fb0` plus an experiment that was not committed.

## The experiment

`StringJoin` copies every part into a new String and keeps neither
argument (runtime `StringJoin`). Its Array argument had no retention row
and no result relation, so a backing frame that passed a formal's storage
to it recorded a call escape; 28 such calls start call escapes in the
first analysis. The experiment gave it the result relation `Split` has
(depends on nothing, keeps nothing) and recorded its result type name.

## Canonical census

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).

| Run | Refusals | Rounds |
|---|---|---|
| `a21fe4a5e` | 2027 | 3977, 2207, 2034, 2027 |
| StringJoin related | 1831 | 45768, 3160, 1999, 1831 |

The first analysis failed the flow store: "recursive summary work budget
exceeded". The later ones did not, after landings changed the program, so
the final count is not a valid measure: a plain compile stops at the
first analysis.

## Why the cap is hit

One recursive closure of codegen rewrite routines
(`WrapExprWithSemanticGraph`, `RewriteSemanticStructCall`,
`TypeEnvSemanticValueType` and their callers) already spends 3493 of the
4096 units in the first analysis without the experiment: every member
formal is evaluated four times. With StringJoin related, the summaries
take a fifth pass, and 7768 evaluations of the first analysis exceed the
cap. The [cap](../decisions/2026-10-18_flow_budget_counts_closure_entries.md)
is fixed and a gate pins it at 4096.

## What it needs

The repeated operation is the fixed point re-evaluating every member of a
recursive closure on each pass. A pass that re-evaluates only the formals
whose callee summaries changed would let this closure, and any precision
gain inside it, stay under the cap. Until then, a relation for StringJoin
cannot land.
