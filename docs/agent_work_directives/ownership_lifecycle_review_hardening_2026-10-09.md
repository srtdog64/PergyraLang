# Ownership lifecycle review hardening

Status: `IMPLEMENTATION COMPLETE` for this bounded model/document task;
production graph integration remains OPEN.
Base: `main @ 85fff5aa339ae136ac0797c4a6a95a5285ada70e`,
shared dirty checkout, 2026-10-09. The user requested completion of the
review corrections, stronger documentation, and implementation boundaries.
This is a bounded model/contract task, not a successor to the active P1/DRV-2
production rung, nor a new SoT row. No commit, push or installation is included.

## Objective card and chain

- Objective: make partial step execution and compensation failure inspectable
  without claiming rollback; record every remaining physical implementation
  obligation before graph lifecycle integration begins.
- Priority: preserve semantic identity and canonical owners; explicit failure;
  migrate the last consumer and remove ambiguous outcomes; negative evidence;
  then patch size and cost.
- Owners: `OwnershipGraphLinks.v` owns graph operations, identities and borrows;
  `OwnershipGraphActionScope.v` / semantics 29 own step/saga execution;
  `CompensationCore.v` owns the separate ideal snapshot interface. Semantics 28,
  tree authority/atomic batch, counted reclaim and checked roots keep their
  existing ownership. Doc 209 and the boundary matrix are projections only.
- Chain: graph primitive -> ordered acquire -> body prefix -> release of this
  step's holds -> forward saga -> reverse completed-step compensations ->
  independent `GraphActionScopeAudit.v` -> focused kernel/selftest receipt ->
  semantics 29/doc 209 and boundary documentation. There is no native caller
  of this model API to migrate; production P1 sources are outside this scope.
- Last consumer: the independent proof consumer and the explanatory algorithm;
  future compiler recovery dispatch must not read a receipt as recovery proof.
- Forbidden: snapshot rollback after physical free; automatically running a
  full-step compensation on a partial forward step; classifying compensation
  completion as restored effects; dropping the original failure on a failed
  compensation; manual lifetime/own/ref requirements; tracing-GC fallback;
  changing other production lanes or claiming native/CI closure.

## Complete change set and dependency order

1. Preserve graph/tree/counter machines. Extend the canonical action runner's
   result with a successful body-prefix length; count successful reads too.
   The receipt is not a persistent-effect list or a replay instruction. Its
   interpretation is bound to that exact body and execution generation.
2. Prove the recorded prefix is within the body, corresponds to successful
   execution and accounts for the body state. Primitive refusal, not composite
   step failure, has the unchanged-state contract. Preserve borrow restoration
   and admitted no-dereference-failure results.
3. Replace `SagaCompensated` with `SagaCompensationFinished`. Keep the failed
   forward receipt and, on `SagaStuck`, the failed compensation receipt too.
   A completed-step compensation is arbitrary admitted code, not an inverse.
   Do not execute the failed forward step's compensation automatically.
4. Migrate all model witnesses and the independent consumer. Promote the
   partial-insert and no-op-compensation counterexamples from the review;
   add partial compensation and prefix-bound witnesses. Tighten planted
   regressions and include this consumer in the full kernel snapshot.
5. Correct doc 209's primitive/composite failure distinction, acquisition,
   optional collector terminology, store identity, model-only atomicity and
   cost claims. Strengthen semantics 29 and cross-links, without duplicating
   semantics in prose.
6. Add a boundary matrix: owner/producer/last consumer, admitted evidence,
   refusal, retirement, missing refinement and falsifying test. Include
   finite identities/counters, authority/lease binding, root completeness,
   whole-heap frame, native preflight/commit, effects, traversal, concurrency,
   DX and total work accounting. These remain OPEN where not proved.
7. Run focused kernel and planted regressions, then existing graph gate,
   static formal inventory and full kernel at the integration boundary.
   Record actual hashes/results and a subordinate handoff note; do not replace
   the concurrently maintained active P1 card.

## Scope, integration and evidence

One editor (this task), no agents or worktrees. Edit only the named action
model/consumer/gates, their documentation/index links, a narrow full-consumer
registration, this directive/audit and a subordinate handoff entry. The direct
user request invokes exception 2 of the collaboration agreement for the action
model; record that before changing it. Do not edit canonical value/graph/tree
machines, native/self-host sources, installed executables or SoT registry.

Integration owner: this task. Primary integration gate:
`tests/graph_action_scope_smoke.sh` with its independent consumer; planted
regressions in `tests/graph_action_scope_selftest.sh`. Static budget 60 seconds,
focused invocation 300 seconds, full kernel integration 1800 seconds. Use the
repository's pinned Rocq 9.3.0 wrapper. No timeout/axiom-policy relaxation.

Outputs are checked bounded model candidates, updated contract documentation
and an implementation-readiness matrix. Native effect recovery, no-fail physical
commit, source inference, cost bounds and whole-language safety are not outputs
of this task. A green model gate does not close those obligations.

Observed completion: canonical receipt/outcome migration, independent
consumer, 11 planted regressions, action/graph focused gates, full wrapped
formal/kernel gates and boundary documentation. Exact scope, hashes, initial
failed runs, remaining warnings and evidence are in the
[hardening audit](../audits/ownership_lifecycle_review_hardening_2026-10-09.md).
No native implementation or SoT status was changed by this task.
