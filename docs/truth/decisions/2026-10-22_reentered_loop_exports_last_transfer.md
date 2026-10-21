# 2026-10-22: a loop checked again from another entry exports the transfer of its last check

Decided by: the Claude lane. It fixes which loop transfer the semantic
checker exports; the user can overturn it.

## Rule

The checker keeps one private loop summary per entry state, as a memo: a
loop checked again from an entry it has seen applies that summary instead
of walking the body. An enclosing loop's fixed point checks an inner loop
once per outer walk, so an inner loop can have several. Downstream stages
take one transfer row per loop (`PgyLoopFlowSummaryFact`, then HIR, MIR,
MIR JSON and the self-host AIR certificate readers).

- The exported row is the summary the loop's **last** check used, recorded
  or applied. The last outer walk is the one at the outer fixed point, so
  that transfer starts from the outer loop's final entry.
- Rows are exported when the function's check ends, in the order the
  summaries were recorded. A loop checked once exports what it exported
  before, in the same place.
- A failure to keep a summary or its trace marks the loop capture failed,
  and the semantic result refuses, instead of leaving an older row.

## Why

Before, every recorded summary was exported at once, so a loop checked
from two entries had two rows, and HIR validation refused the routine
("loop-flow summaries share loop SyntaxNodeId"). Any `while` holding an
inner loop and an `inout` call after it reaches this: the call changes the
state the inner loop is entered with on the next outer walk. The canonical
driver input has this shape (the loop around
`SemanticAstGenericSpecializationAppend` in
`src/self_hosted/semantic/ast_generic_specialization_fact_owner.pgy`), and
a probe copied from it failed the same way, so a semantically admitted
canonical program would still stop at HIR. Old main (`4f7692f38`) refuses
it the same way.

## Gates

- `test_semantic`'s `test_loop_flow_reentry`: one row per loop, and the
  inner loop's exported entry has `ids` used, which only the later outer
  walks see.
- `tests/cases/function_param_flow_summary/inout_call_after_inner_loop_valid.pgy`
  emits MIR and prints `3` on C and LLVM.
