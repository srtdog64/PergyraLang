# 2026-10-18: native refusal census on the canonical driver input

Recorded by the Claude lane. Input: unchanged
`src/self_hosted/compiler/driver_bootstrap_main.pgy`, compiled with
`pgy <input> --native-pipeline --backend=c --error-format=json`.

## Production result at local main `7cd004fab`

40,463 errors and 21 warnings; first error at
`hir/ast_expression_owner_kind_binding.pgy:61:55` (`Some(provenance_texts[i])`).
About 40,000 of the errors are one cascade: after the first error the
parameter-flow summary store fails closed (`function_param_flow_fail`
records no reason once an error exists), and every later summary is
`SLOT_PARAM_SUMMARY_ALL`, so each `ref` Array parameter reports return,
channel and call escapes.

## Method (scratch build only, never committed)

A copy of the tree with three diagnostic patches:
- print module, function, node kind and callee for every error;
- downgrade errors to warnings so later analysis still runs
  (`PGY_CENSUS_DOWNGRADE`, `PGY_CENSUS_ALL`);
- make the flow work budget an environment value (`PGY_CENSUS_BUDGET`).

## Findings

| Item | Value |
|---|---|
| Flow summary work budget (fixed, per demand closure) | 4,096 |
| Largest closure work at full analysis | 28,258 (`SemanticAstCollectionExclusiveClassify`, 65 entries, 3 passes) |
| Full analysis with budget 200,000 | 1,063 s; then fails closed: "recursive backing relations are incomplete or stale" at `mir/routine_if_owner.pgy:144` |
| Opaque-boundary String sites at `7cd004fab` | 1,083 sites in 218 functions |
| After `c1df248d6` (scalar ToString) | 992 sites in 211 functions |
| After `08f0cb894` (mutator receivers, consumer effects, parameter paths) | 767 sites in 182 functions |
| Released-element String borrows | 131 sites in 5 functions |
| Deferred "Array argument call requires preserved array storage" | 1,475; depends on flow summaries |

A falsifier for the receiver fix: calling an `inout Array<String>` helper
twice on one array was refused at `7cd004fab` and at `39a8fc1db`, and
admitted for `Array<Int>`.
