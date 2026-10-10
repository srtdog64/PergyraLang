# Store-local graph reclaim: scoped integration review

Status: SCOPED REVIEW VERIFIED; production and policy remain OPEN.
Base: `main @ 85fff5aa339ae136ac0797c4a6a95a5285ada70e`.
This side-conversation scope does not adopt a collection policy, change the
production compiler/runtime, or resume the main ownership-cutover rung.
The existing uncommitted model, gate, workflow and document changes are preserved.

## Objective card and closure boundary

- Objective: check the reported root-completeness and local-reclaim claims,
  connect maintained counts and canonical cleanup-authority admission at the
  model boundary, and independently kernel-check the resulting contracts.
- Priority: semantic preservation, authoritative facts, negative evidence,
  accurate scope, then patch size. No performance superiority claim.
- Fact owners: `OwnershipGraphLinks.v` owns graph transitions;
  `OwnershipGraphCycleReclaim.v` owns counter maintenance/local selection;
  `OwnershipGraphRootCompleteness.v` owns checked-language roots/simulation;
  `OwnershipTeardownAuthority.v` owns cleanup-right issuance/transfer/consumption.
- Last consumer: the checked reclaim boundary and its independent proof consumer.
- Forbidden fallback: trusting a hand-written root list or a stale right;
  calling a recount a measured local implementation; approving an axiom or
  claiming production closure from a model kernel pass.
- Verification: the focused graph gate with an independent typed consumer,
  zero assumptions, deletion/mutation negatives, then relevant document gates.
- Falsifiers: nested-value/caller/view/return root omission; identity reuse;
  wrong/missing/transferred/consumed cleanup right; incomplete counter evidence;
  theorem-name-only and source-generation drift in the gate.

## Survey and dependency order

1. Existing `rexec` -> `reclaim_step` -> `trial_garbage` uses an exact
   whole-store recount. A local check with maintained counts exists separately.
   Keep that distinction explicit and connect the operational counted boundary
   to the existing simulation instead of replacing the graph machine.
2. Existing `count_update` -> `gexec` preserves counters for each admitted
   operation. Package the graph and count transition together and establish
   the run/reclaim connection; do not invent another heap operation.
3. Existing language authority is a fixed `rights` function. Derive admission
   from the canonical ledger and prove its issuance/revocation behavior.
   Graph-to-forest identity/footprint binding remains a separate refinement;
   a permission projection is not synchronized physical retirement.
4. The original focused gate protected declaration names and checked four modules.
   Add a separately typed consumer, isolated receipts and source bindings.
5. Correct only the new graph sections of documents; preserve the main
   handoff's active card. The parent chat receives the tested patch boundary
   and all remaining obligations, not an instruction to claim CLOSED.

## Out of scope / not inferred complete

Policy adoption, arbitrary MIR/CFG certification, real layout/frame producers,
physical C/LLVM counters and overflow, scheduling/cost measurements, concurrent
mutation, cross-store edges, weak links, finalizers, and complete graph/forest
retirement refinement are not closed by this scoped repair.

## Findings and repairs

1. **Disconnected counters:** `rexec` uses `reclaim_step`/`trial_garbage`,
   an exact recount. Added `counted_gstep`/`counted_grun` to consume one
   `gexec` result with its count delta, and `counted_reclaim` to use maintained
   counts. Run/reclaim projection and count preservation are kernel-proved
   under the existing admission/issued-identity premises. The recount remains
   the reference specification, not a native compatibility fallback.
2. **Disconnected permission:** the original language uses a fixed rights
   map. `ledger_rights` now derives admission from the canonical ledger's
   root epoch and holder. Grant, transfer and consumption are connected;
   the checked counted boundary refines the existing simulation. This does
   not establish the store/root binding's provenance, synchronize the two
   heaps, or apply the forest's whole-unit lease/pin checks. These remain OPEN.
3. **Physical-locality overclaim:** `internal` filters contributions from C
   but traverses the entire slot-list spine; `nth_error` is not constant time.
   Corrected model headers, doc 28, README and docs/102. The arithmetic
   decomposition is exact, but physical candidate-only traversal and cost
   are not proved. `fuel` bounds closure rounds, not total work or latency.
4. **Scope overclaims:** counter maintenance covers the seven current `GOp`
   constructors under `CountsExact`, `AllEdgesIssued` and `op_admitted`.
   Past-generation stale edges are permitted; future/unissued vacant-current
   identities are not. Snapshot is a slot-list lemma, not a language operation.
   Root simulation covers terminating checked reference runs under the issuing
   invariants. View identities are roots, not a physical lease/pin model.
5. **Names-only ratchet:** added `tests/coq/GraphCycleReclaimAudit.v` with
   20 independently typed propositions/examples. It pins simulation, exact
   counts, ledger revocation and boundary contracts; concrete tests cover
   grant, wrong context, transfer, consumption, root reissue, incomplete roots,
   nested/caller/view/returned links, stale-generation counters, closure,
   round exhaustion and visible raw-index allocation reuse. The canonical
   authority witness operations are themselves checked to be accepted.
6. **Receipt collisions/drift:** focused runs get unique receipts, bind
   copied inputs to SHA-256, reject copy/run source drift and reject receipt
   overwrites. Before/after equality is not continuous freeze evidence.
   Added an isolated five-case negative self-test and registered it in the
   existing Makefile/CI graph gate. The full kernel gate includes the new
   independent consumer; no assumption approval was expanded.

The Pergyra authoring skill's ownership/lifetime guidance kept these repairs
behind existing graph and authority owners. No manual-drop ceremony,
annotation, new user syntax or hidden collector policy was introduced.

## Verification receipt

Observed locally on the admitted Rocq 9.3.0 / Stdlib 9.2.0 prefix:

| Gate | Observed result |
| --- | --- |
| Focused graph gate, including independent typed consumer | PASS: six model modules + consumer; zero assumptions; `.tmp/graph-cycle-reclaim/review-final-2026-10-09/kernel.log` and matching `source-after.log` |
| `graph_cycle_reclaim_selftest.sh` | PASS: deleted falsifier, theorem weakened to `True`, planted axiom, source drift, receipt overwrite all refused for their expected reasons; `.tmp/graph-cycle-reclaim/selftest.B0qoJt/` |
| `formal_semantics_smoke.sh` | PASS: 72 proof files; kernel verdict 81 modules plus approval export/binding consumer, existing two approved Slot abstractions only; `.tmp/graph-cycle-reclaim/formal-review-2026-10-09.log` |
| `documentation_quality_smoke.sh` | PASS after handoff/document edits; `.tmp/graph-cycle-reclaim/doc-quality-final-2026-10-09.log` |
| `evidence_lifetime_smoke.sh` | PASS: 15 kinds, exact correspondence, negative self-test fired; `.tmp/graph-cycle-reclaim/evidence-lifetime-review-2026-10-09.log` |
| `evidence_lifecycle_adequacy_smoke.sh` | PASS; `.tmp/graph-cycle-reclaim/evidence-lifecycle-review-2026-10-09.log` |
| `beta_readiness_checklist_smoke.sh` | PASS; `.tmp/graph-cycle-reclaim/beta-readiness-review-2026-10-09.log` |

Warnings are visible in the logs: nested-list scheme registration (`GVal`/
`SVal`), deprecated names/notations, and approval load-path rebinding.
These are not a warning-free run. The kernel's indices-theory dependency
profile remains reported; "two abstractions" is the named Axiom budget, not
a claim that every foundational theory dependency is absent.
After the full run, all 81 source-manifest hashes were compared with current
model/consumer files and matched. `git diff --check` passed. This is snapshot
binding, not continuous freeze or main P0 baseline evidence.

Reviewed sources (SHA-256):

- Cycle model: `60410a323110a1d57cc2a94d0bd480e0bcfe0eb532c56d800c450b5aa0b51f9f`.
- Root model: `bdb80df5da1329a2d81b373d445ac4f7c481f1c3bbbf58520778e1d7dfc3d61b`.
- Independent consumer: `40da253b4a7b82eeb32d855feafc463a3fe35aa90ab750954caafe8556082498`.

No compiler/runtime source, public bin, active self-host card or SoT registry
was changed. Original Claude/user changes and `gmon.out` are preserved.
The new models, focused gate, negative self-test, consumer and this audit are
untracked; document/build/kernel-registration changes remain unstaged.
No commit, push, native runtime/performance test or remote CI check was done.
These results are not the main thread's P0 baseline or whole-project CI green.

## Parent handoff boundary

Destination authorized by the user: the original `Install TypeSafe skill`
chat (`01a0b96c-2cf8-7583-a99e-761db7eec206`, local).
Automatic message handoff FAILED: the app returned
`'sendRequest.bind' is not a function.` on both the explicit-host attempt
and the default-host retry. A read-back after the first failure showed the
parent idle with its earlier completed turn, not a new handoff. No delivery
is claimed. This audit and the scoped handoff section are the file-backed
handoff; the main chat can read them directly on resume.
Deliver the tested scope
and the following obligations, not a CLOSED claim or a new independent rung:

1. Decide whether explicit store-local reachability tracing is allowed;
   it is not the adopted default ownership cleanup mechanism.
2. Name and implement MIR/CFG live-set, layout and frame-map producers plus
   their checkers/refinement. The structured model's invariant is not proof
   that a real producer emits complete roots.
3. Issue store/root bindings from one authority; connect exact graph/forest
   retirement footprints and active leases/pins. The adapter checks permission
   only and leaves the canonical ledger in control.
4. Only if adopted on the active implementation chain, refine native
   candidate-only traversal, finite counters/overflow and scheduling/budgets;
   measure fixed-input C/LLVM behavior. Do not infer cost from closure fuel.
5. Keep concurrency, cross-store edges, weak links and finalizers explicitly
   outside this model until their contracts and falsifiers exist.
