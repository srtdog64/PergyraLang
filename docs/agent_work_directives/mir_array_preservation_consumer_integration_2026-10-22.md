# Array preservation: native MIR consumer integration (2026-10-22)

Status: NATIVE CONSUMER IMPLEMENTED; integration/V1 remains OPEN.
Results belong to [current gates](../truth/state/gates.md). Base main:
`3cdb56256564962ae7eca4420823cf7c50740443`, fast-forwarded from
`7e7b63673`. The Claude worktree's uncommitted changes are excluded.
This implements the consumer half of
[the existing handoff](mir_loop_cleanup_array_preservation_projection_2026-10-21.md).

## Objective card

- Objective: an iteration-local Array may be read by a declared synchronous
  function and then retired by the compiler, without a source drop.
- Priority: preserve the published identity/verdict, fail closed, check
  actual cleanup/alias behavior, then retain both lanes' regression gates.
- Fact owner: the demanded semantic parameter summary, published once as
  `PgyFunctionParamFlowFact.array_preserved` by `a35ecdd60`.
- Last consumers: checked MIR cleanup exits, native C/LLVM terminators;
  MIR/AIR serialization and binding carry the verdict unchanged.
- Forbidden: Slot-mask-only grants, callee-name routes, body reanalysis,
  unknown/missing verdicts treated as preserved, or per-program exemptions.
- Shared gate: declared-read loops on C/LLVM, including a later-declared
  callee, with stale/missing/cross-formal/element/glue faults and the existing
  escape/defer negatives; MIR/HIR/AIR units and the full flow target.

## Whole chain and ordered changes

1. Consume the producer's positive bits (zero = unproved), not the obsolete
   two-field proposal in the original request. ELEMENTS without DESCRIPTOR
   and unknown bits refuse. Keep the published function/formal identity.
2. Carry the field through HIR and MIR summaries. At MIR entry compare HIR
   with the supplied immutable semantic snapshot; later compare MIR with
   its admitted HIR rows. The join is one-to-one: duplicated HIR rows cannot
   replace another semantic formal. Do not demand analysis from those reads.
3. Existing per-routine lowering currently issues cleanup before later
   callees exist in the MIR inventory. Move cleanup issuance to the complete
   routine-inventory boundary, before final DCE. Recompute changed routines'
   analysis there; do not make declaration order a permission condition.
4. Resolve a direct declared callee by its syntax identity and exact formal
   ordinal/arity. Require a current summary, a scalar/Void result, a non-own,
   non-inout Array formal and both preservation bits. Keep flow escape/write
   refusal separate; preservation is not a substitute for the flow mask.
5. Extend existing MIR/AIR binding hashes, row validation and emitted facts.
   Native backends continue consuming the same cleanup edge, not a second
   inferred cleanup policy. The legacy self-host cleanup refusal stays until
   its general emitter owns this instruction; no Task B deletion is claimed.
6. Integrate and independently rerun the committed copy-landing fixtures,
   requiring their measured landing counts and C/LLVM outputs. Preserve
   by-value-only Clone negatives and missing-fact/alias/defer refusals.

## Scope and finish

GPT edits compiler fact carriage/validation/cleanup, related behavioral
gates and its truth-state records. Claude retains semantic implementation,
flow corpus/gate and `state/blocker.md`; no uncommitted Claude source is
copied. Integration owner: GPT root. Budgets remain 60 seconds static,
5 minutes focused, 30 minutes integration; no larger allowances or reduced
canonical input.

Run native loop gate first, units and full flow gate next, then required
String-result and MIR integration gates. Record installed-driver failures
and canonical refusals explicitly. A source-level route survey, local unit
pass or reduced diagnostic count does not close V1 or any SoT row.
