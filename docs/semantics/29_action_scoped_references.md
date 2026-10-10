# Action-Scoped References

Status: **DEFINITION RECORDED at the user's request (2026-10-09); bounded
model checked; surface syntax, acquisition inference and production OPEN**.

**No value has a declared lifetime.** Nobody can say in advance how long an
object will exist, in a program or in the world. Pergyra therefore does not
ask the author to promise it. Whether a node still exists is decided only by
ownership events. A reference is a name for it, not a promise that it is
there. What a program may rely on is the extent of an action that holds what
it uses.

This document records that contract and its checked model. It does not
redefine graph storage, cleanup or Slot. Their owners remain
[Memory Boundary Composition](28_memory_boundary_composition.md),
[Ownership Clean](27_ownership_clean.md) and the
[graph design model](proofs/OwnershipGraphLinks.v). Lifetime annotation
syntax stays forbidden ([docs/118 §2.1](../118_slot_model_rigor_audit.md)).

## The contract

1. **Existence.** Only ownership events end a node: a delete of its slot or
   a drop of its store. Writes, inserts, table growth, new stores and
   borrows never make a reference stale.
2. **References are names.** Using a stale reference is an explicit
   refusal, never undefined behaviour.
3. **Holding is the extent of a step.** A step acquires the references it
   uses when it starts and holds them until it ends. Holding is not a
   prediction about the object; it ends with the step. While a node is
   held, its destroyers are refused. The refusal goes to the destroyer,
   where it is observable, not to the reader.
4. **Acquisition failure is decided at the step's start.** An admitted step acquires
   every reference it reads and changes no hold inside its body. It never
   fails a dereference. Its only failures are an acquisition refusal at its
   start or a refused body operation.
5. **Between steps none of this step's holds remains.** A step releases exactly what it
   acquired, including after a partial acquisition. A long action is a saga
   of steps. A node deleted between steps is discovered when the next step
   starts; the saga then compensates the steps it completed. Pre-existing
   outer holds are preserved, not cleared. Body-operation failure can happen
   after successful effects; releasing holds does not roll those effects back.
6. **Compensation targets must be spared.** If a node a compensation needs
   is deleted between steps, the compensation cannot start and the saga is
   observably stuck. Preventing that is the job of deletion authority
   (only the owner may delete). Binding it is OPEN.
7. **Compensation execution is not recovery proof.** Results retain the failed
   forward step's receipt. A failed compensation retains its own receipt too.
   Finishing all completed-step compensations is an observable failure outcome,
   not `SagaDone`, restored effects, or permission to retry.

A lifetime promises that a reference never goes stale. This contract
promises less and asks for less. A held reference cannot go stale while the
step holding it runs. The compiler needs the step's extent and a complete
acquisition set. Block structure suggests an extent; proving that inference
covers aliases, dynamic traversal and calls is still an obligation, not
supplied by this model.

## Checked model

[`OwnershipGraphActionScope.v`](proofs/OwnershipGraphActionScope.v) adds
steps and sagas over the unchanged graph machine. It has no assumptions.

| Claim | Theorem |
|---|---|
| A reference resolves to the same blocks through every operation that is not one of its two destroyers | `spared_step`, `spared_run_keeps_link` |
| A reference goes stale only if a run deleted its slot or dropped its store | `stale_needs_a_destroyer` |
| While held, both destroyers are refused, and the reference resolves until the hold ends | `held_refuses_destroyers`, `held_step`, `held_run_keeps_link` |
| An admitted step never fails a dereference | `admitted_step_never_fails_deref` |
| A step releases exactly what it acquired | `run_step_restores_borrows` |
| In a saga of admitted steps, under any operations other code runs between steps, no outcome is a failed dereference | `saga_never_fails_deref` |
| A body receipt records a successful prefix that accounts for its final state and the exact next refusal, without ignoring failed operations | `run_body_receipt_sound`, `run_body_receipt_projects` |
| A receipt never exceeds the body; successful completion covers it all; acquisition failure covers none | `run_step_receipt_bound`, `completed_step_accounts_for_whole_body`, `acquisition_failure_has_no_body_prefix` |

Witnesses and falsifiers, starting from the model's three-node graph:

- `destroyer_gets_the_refusal`: deleting a held node is refused and the
  node stays readable; deleting an unheld node succeeds.
- `partial_acquisition_is_released`: an acquisition that fails part way
  leaves the state exactly as before the step.
- `neighbor_needs_its_own_acquisition`: holding A does not protect C, a
  node A's edge names. Reading C without acquiring it is not admitted, and
  it fails once C is deleted.
- `unguarded_delete_breaks_hold`: a delete that ignores holds makes a held
  reference stale. The borrow check is load-bearing.
- `stale_at_step_start_compensates`: a node deleted by other code between
  two steps is found at the second step's start, and the first step's
  compensation restores its write.
- `deleted_compensation_target_gets_stuck`: deleting the compensation's
  target too leaves the saga stuck with an acquisition refusal.
- `partial_forward_effect_is_reported`: a successful insert followed by a
  refused delete leaves the new node; the receipt reports prefix length one.
- `noop_compensation_is_not_restore`: an admitted empty compensation finishes
  but leaves the earlier write in place.
- `partial_compensation_preserves_both_failures`: compensation writes and then
  fails; both the original acquisition failure and the compensation's partial
  body failure survive, with the changed data.
- `receipt_counts_successful_reads_and_writes`: the prefix includes reads,
  excludes the refused operation and never includes the unexecuted suffix.

The independent consumer
[`GraphActionScopeAudit.v`](../../tests/coq/GraphActionScopeAudit.v)
restates the theorems without the owner's summary predicates. It also pins
the admission and these observations, so weakening a statement or emptying
the admission breaks it.

## Execution receipts and effect recovery

The canonical result of `run_step` is `(state, StepReceipt)`, where the
receipt carries `step_out` and `completed`, the number of successfully run
body operations. It is interpreted against that invocation's exact body.
Reads count too: a nonzero prefix is not proof of a persistent mutation.
The receipt is an observation, not an undo log, a recovery program or an
authorization to replay. A production representation must bind it to the
body identity and execution generation; no full runtime trace is mandated.

`graph_refusal_unchanged` applies to a single graph primitive. It does not
apply to the successful prefix of a refused body. `run_body_receipt_projects`
accounts for that prefix with the imported graph machine and non-refused
results. Hold release leaves the prefix effects in place.

The saga compensates **completed** forward steps, in reverse order. The
failed current step is not pushed: its full-step compensation is not
necessarily safe on a prefix. The result vocabulary makes the limit explicit:

| Outcome | Meaning |
|---|---|
| `SagaDone` | All forward steps completed |
| `SagaCompensationFinished(k, failed_forward)` | Completed-step compensation execution ended; forward failure and possibly residual effects remain |
| `SagaStuck(k, j, failed_forward, failed_compensation)` | Compensation j failed, possibly after its own effects; both failure receipts remain |

The previous ambiguous `SagaCompensated` constructor has been removed, without
a compatibility alias. Neither failure outcome may be converted to success
or automatically retried. No recovery certificate is emitted. An application
requiring recovery must establish local transaction atomicity, prefix-aware
compensation, or an explicit domain postcondition before claiming it.

Sequencing resembles a failure-short-circuiting bind: later body operations
do not run after refusal. That limits **future** effects, not effects already
performed. Adding a monadic branch alone cannot supply rollback.

The concrete compensated-write witness establishes restoration only for that
write, not for all admitted compensation programs. `CompensationCore` models
ideal snapshot restoration separately; arbitrary user code is not refined to
that interface here. The boundary and falsifiers are collected in
[implementation boundaries](ownership_lifecycle_implementation_boundaries.md).

## Costs and tradeoffs

- A hold keeps its node for the whole step and refuses table growth of its
  whole store; filling a free slot is still admitted. A long step keeps
  memory long; a long action should be a saga of short steps.
- The acquisition set must be known when the step starts. Traversal that
  discovers nodes as it goes needs acquire-on-first-touch. A failure there
  happens after some body effects, so it needs step atomicity or
  compensation. This is not modelled.
- The model checks generation and records a hold in `gbor`. That table is
  ghost state, not a mandate for a runtime counter or allocation on every
  acquire. A concrete representation and its cost need a refinement proof.
  A dereference inside an admitted step cannot fail, so its check could be
  elided under the same premises; no elision is implemented or measured.
- A destroyer may be refused, so it must handle that refusal. This moves the
  `Result` from every reader to the one destroyer. It does not remove it.
- Compensation needs its targets alive. Holding them for the whole saga
  would bring long holds back, so the model leaves sparing to deletion
  authority.

## Relation to existing owners

- [`OwnershipGraphLinks.v`](proofs/OwnershipGraphLinks.v) owns the machine,
  generations and borrow rules. This model imports it and does not change
  it.
- [`CompensationCore.v`](proofs/CompensationCore.v) owns the
  snapshot-restoration interface. Here a compensation is any admitted step;
  that it undoes its forward step is not shown.
- [docs/173](../173_intent_axis_strengthening.md) owns the intent fact
  families, including compensation and steps. The mapping of a model step
  to a source `step`, `action` or `intent` is OPEN.
- [`OwnershipTeardownAuthority.v`](proofs/OwnershipTeardownAuthority.v) owns
  cleanup rights. Using it to spare compensation targets is OPEN.

## Open obligations

- Surface: how a step names or infers its acquisitions. No syntax is
  selected, and no new keyword is implied.
- Compiler: infer each step's acquisition set from its body; fail closed
  when a read is not covered.
- Acquire-on-first-touch for traversal, with its atomicity contract.
- Bind compensation-target sparing to deletion authority.
- Bind failure receipts to source effects and execution identity; establish
  a recovery contract before reporting restored effects. Partial body and
  partial compensation effects are explicit, not undone by this model.
- Workers, async suspension inside a step, and FFI: the model is
  sequential.
- Runtime representation, C/LLVM refinement and cost.

## Non-claims

This does not establish the language's memory safety, that lifetimes are
unnecessary for every program, that compensation undoes its effects, or any
performance result.

Gate: `tests/graph_action_scope_smoke.sh`; planted regressions:
`tests/graph_action_scope_selftest.sh`.
