# 2026-10-21: each formal's Array preservation verdict is published in the flow snapshot

Decided by: the Claude lane, as the producer half of
[the loop-local cleanup handoff](../../agent_work_directives/mir_loop_cleanup_array_preservation_projection_2026-10-21.md)
(the user chose: Claude publishes the evidence, GPT connects the MIR
consumers). It names the field the consumer carries.

## Field

`PgyFunctionParamFlowFact` (`src/semantic/function_param_flow_fact.h`)
gains `uint32_t array_preserved`, beside `mask`, in the same row and
generation:

| Bit | Meaning |
|---|---|
| `PGY_FUNCTION_PARAM_ARRAY_DESCRIPTOR_PRESERVED` (1) | the callee neither keeps, hands off nor releases the Array storage the caller passed in this formal |
| `PGY_FUNCTION_PARAM_ARRAY_ELEMENTS_PRESERVED` (2) | set only with bit 1: the call also leaves the elements as owned as they were, and the callee keeps none of them |

Zero is unproved. The snapshot sets the bits from the same owner verdict
the semantic consumers read (`function_param_flow_array_preservation_failures`),
through a read that demands nothing (`..._settled`): a row that is not
complete, is unsupported, has no ready output dependencies or is not
current publishes 0. A failed store publishes no rows at all.

## Why not `has_array_preservation` plus failure bits

The handoff proposed a presence flag and the existing `*_UNPROVED` bits.
With positive bits a zeroed, forgotten or calloc'd row reads as unproved
without a second field to check, which is the handoff's own condition
("an absent/unavailable/stale proof must never look like zero failures").
`FUNCTION_PARAM_ARRAY_*_UNPROVED` stays private to the semantic owner.

## What the consumer must still check

- Only formals some call demanded have a row. An `own` formal is never
  demanded by a hand-off, so it has no row; a missing row is unproved.
- Preservation is not non-retention or non-mutation: the `mask` and the
  result relations stay authoritative for escape, and an `inout` formal
  may be descriptor-preserved while it changes the Array.
- An element bit without the descriptor bit is never published; carriage
  validation should refuse it. No admitted source probe produced a
  descriptor-only failure: the probes that would (a writer through a
  formal's element path, a push through a by-value formal) are refused by
  the mutator rules, so that encoding rule has no source falsifier here.
- No `src/compiler` or `src/codegen` file is changed by this producer:
  HIR/MIR carriage, validation, AIR binding, MIR JSON and the self-host
  reader belong to the consumer change set.

## Producer commit and gate

Producer commit: `a35ecdd60`, on main `7e7b63673`
([gates and mutations](../evidence/2026-10-22_array_preservation_snapshot_gates.md)).
Behavioral gate: `test_semantic`'s `test_array_preservation_facet`, which
reads the published rows of an admitted program: a by-value and a `ref`
read-only formal (the latter in the handoff's loop falsifier) publish 3,
an element write publishes 1, a formal used inside a `defer` publishes 0,
and an `own` formal has no row or 0.
