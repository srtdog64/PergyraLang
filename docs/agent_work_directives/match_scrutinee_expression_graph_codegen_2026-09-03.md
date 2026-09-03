# Match scrutinee expression-graph codegen lease — 2026-09-03

Status: `IMPLEMENTED — LOCAL FOCUSED GREEN; EXACT CI PENDING`

Exact base: `8f1a35d334c8506fc0e8ecf71c6df6fb4b6ca284` on
`origin/main`.

This is a bounded implementation lease, not a semantic authority or progress
owner.

## Objective card

- Objective: make installed self-host C match emission consume the already
  required semantic expression graph for its scrutinee instead of re-entering
  text expression classification.
- Priority: preserve match behavior; consume the existing graph identity;
  remove the text decision; fail closed on missing or mismatched graph facts;
  then keep the patch small.
- Fact owner: `SemanticAstExpressionSurfaceFacts`, specifically the atom-lane
  `SemanticExpressionGraphView` required for `TypedAstKindMatchStmtTag()`.
- Last legitimate consumer: `stmt_emit.pgy` renders the graph once and passes
  the rendered C expression to the scalar, Option, and tagged-enum match
  emitters.
- Forbidden fallback: `IntEval(match_subject, env)` or
  `RewriteExpr(match_value, env)` at the match emission boundary.
- Verification gate: `driver_rung2_match_graph_use_owner.sh`, followed by
  focused DRV-2 parity for `match_case_int`, `option_match`, `enum_match`,
  `class_bump_option_match`, and `class_holds_enum_field`. A missing or
  inconsistent match expression graph must remain a rejection.

## Independent edit scope

- `src/self_hosted/codegen/emission/stmt_emit.pgy`
- `src/self_hosted/codegen/emission/option_match_owner.pgy`
- `src/self_hosted/codegen/emission/tagged_enum_match_owner.pgy`
- `tests/self_hosted/parity/driver_rung2_match_graph_use_owner.sh`
- this directive and the active handoff after verification

Do not widen this lease into general expression-text cleanup, match subject
materialization/lifetime changes, new syntax, or registry closure.

## Local evidence

- `stmt_emit.pgy` now requires the match atom-lane graph and renders the
  scrutinee through `RewriteExprWithSemanticGraph` before dispatching match
  shape. Scalar match no longer calls `IntEval` on the text payload; Option and
  tagged-enum owners receive only the rendered C expression and no longer
  import or call `expr_rewrite.pgy`.
- `driver_rung2_match_graph_use_owner.sh` passes and rejects reopening any
  `RewriteExpr` call in the Option or tagged-enum match owners.
- A changed-source C DRV-2 build produced
  `.tmp/self_hosted/driver_rung2/driver_c.exe`, 5,810,176 bytes, SHA-256
  `9BAA7208AAF9B6046E14391AD4CD3597D17FD616D8304DF994387790DB8519D0`.
  Reusing that exact binary with `PGY_SELFHOST_DRIVER_BACKENDS=c`, focused
  producer/source/MIR parity passed all five selected fixtures.
- `self_host_hard_contract_smoke.sh` passed. The broad component inventory was
  stopped after five minutes because it exceeded the repository's static-gate
  budget; its reached `driver-source-mir-execution-action` slice passed. This
  interruption is not recorded as a green component gate and exact CI remains
  the integration authority.
