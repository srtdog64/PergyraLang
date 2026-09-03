# Async direction cores

Companion to `AsyncScopeCore.v`, `CapabilityFlowCore.v`,
`SuspensionRevalidationCore.v` and `DeterministicSubsetCore.v`: the four
theorem targets `docs/204_concurrency_direction_pscc_review.md` §5 set for the
async direction, minus data-race freedom, which `WitnessDataRace.v` already
carried.

They sit next to `AsyncModelCores.md`, which describes `AsyncLifecycleCore.v`
and `AsyncContextCore.v`. The division is deliberate: those two model
contracts that have landed (the checker's affine Future flow, the runtime's
parent-context capture) one handle and one context at a time. These four
model the disciplines docs/204 adopts above them: a scope tree with
cancellation and detach, capability carriage across share/lend/move,
revalidation of a reference that crossed a suspension, and schedule
independence of an admitted task family. Where the two groups meet, the
smaller model is the one this doc points at.

Each core follows the shape `ParallelModelCores.md` established. The adopted
discipline becomes a step relation; the property it was adopted for becomes a
theorem; and the unstructured alternative becomes a machine-checked
counterexample against the same model, so every core answers what the rung
buys and what its absence permits.

These are model theorems, not implementation verification. Each transcribes
a shape from `docs/113`, from `src/runtime`, or from `docs/178`, and
`tests/async_direction_adequacy_smoke.sh` binds every transcribed shape back
to its source line so a model cannot silently stop describing the code.

## 1. `AsyncScopeCore.v` — a running task always has a live scope

A configuration holds task rows `(task, scope, state)`, the open scopes, the
scope tree, and whether the detach capability is held. The root scope is
always open; it is the only place a detached task may live.

| step | guard | what it models |
|---|---|---|
| `open s` | `s` fresh, non-root, parent open | entering a `parallel` block or an intent step |
| `spawn t s` | `s` open, non-root | named `spawn` inside a scope |
| `complete t` | — | the task finishes |
| `cancel s` | `cs` is exactly the descendants of `s` | cooperative cancellation of a subtree |
| `close s` | nothing in `s` still runs, no child scope open | join-before-continuation |
| `detach t` | the detach capability is held | moving a task to the background |

The invariant `contained` says every running task's scope is open.

### What is proved

- **`run_no_orphan`** — a structured run never reaches an orphan. Every step
  preserves `contained`, and the root stays open under every step.
- **`no_running_task_in_closed_scope`** — once a scope is closed nothing in it
  runs: `parallel`'s join-before-continuation generalised to every scope.
  `docs/113` now requires this of every named handle; `AsyncLifecycleCore.v`
  proves it per handle; this theorem states it for the tree.
- **`cancel_reaches_descendants`** — cancelling a scope leaves no running task
  in it or in any descendant scope.
- **`background_only_via_detach`** — with the detach capability absent, no
  reachable configuration has a task in the root scope. Detach is a
  permission, which is docs/204 §2.5's decision.
- **`orphan_reachable_unstructured`** — the rule the language had before the
  structured spawn lifecycle landed (commit `cf66092b`): a scope could be left
  with a task still running. Three steps from the empty world reach the
  orphan. The guard's necessity stays a theorem rather than a memory, and the
  affine-flow rule is bounded to named handles, so anonymous capture-bearing
  blocks and detach still have to land here.
- **`structured_run_exists`** — open, spawn, complete, close is derivable, so
  the discipline is not vacuous.

### What is not established

Cancellation is a state change, not preemption (`docs/114` §5). Nothing
shows the compiler enforces the scope guards beyond the named-Future flow;
that enforcement is docs/204 §4 item 2. The runtime skeleton this rung will
consume exists with no caller (`src/runtime/async/async_scope.h`), and the
gate pins its `AsyncScopeWaitAll` so the model's close rule stays tied to a
real join.

## 2. `CapabilityFlowCore.v` — nothing held was not granted

Capabilities here are the authority mask (`PGY_CAP_IO_READ`, `NETWORK`, …),
not slot access modes. A configuration holds the manifest, each task's mask,
each task's parent, and the caps a task holds on loan. Task creation carries
the mask by `share` (copy), `lend` (the parent gives `L` up until the child
returns it) or `move` (for good); a child `return`s its loan; a task may
`narrow` its own mask but never a borrowed cap.

### What is proved

- **`run_bounded`** — Non-Forgery: every reachable mask is inside the
  manifest.
- **`child_within_parent`** — at creation the child's mask is inside the
  parent's, for all three carriage forms. `AsyncContextCore.v` proves the
  runtime's capture copies the parent's masks exactly; this is the same fact
  read as an inclusion, with lend and move as the two ways to carve it down.
- **`loan_uniquely_held`** — while a cap is on loan the borrower holds it and
  the lender does not. The loan is a unique key, revoked from the lender for
  its duration. This is the analogue, at the mask level, of the unique ghost
  key that guards a capsule in the DRFcaml model (POPL 2025) and of the
  flow-sensitive receive/revoke/return of *Typestate via Revocable
  Capabilities* (PLDI 2026); both are cited in docs/204 appendix A and
  neither is claimed to be reproduced here.
- **`return_restores`** — returning the loan gives the lender its cap back.
- **`lend_then_return_round_trip`** — the discipline runs end to end.
- **`tls_default_forges`** — an executor that reads the per-thread default
  context (`_Thread_local`, mask `PGY_CAP_ALL`) instead of capturing the
  parent puts `network` into a worker's mask from a sandbox that granted only
  `io_read`. The runtime captures the parent since
  `pgy_runtime_context_capture_task` landed; the gate pins that capture on the
  spawn path, so this stays a counterexample about the design and not a
  description of the tree.
- **`structured_spawn_cannot_forge`** — from the same sandbox, no structured
  run does that.

### What is not established

Sub-lending a borrowed cap is outside the model. The lend/return carriage is
docs/204 §2.4's fact vocabulary, not a landed runtime edge; the runtime today
carries `share` (exact capture).

## 3. `SuspensionRevalidationCore.v` — a stale reference never resolves

`SlotCalculus.v` proves stale-handle access impossible for one context. This
core is the concurrent half: while a task is suspended, other tasks may
despawn the entity and reuse its slot. What crosses the suspension is a
`SlotRef` — slot id plus the generation the task observed, the shape of
`SlotHandle{slotId, generation}` in `src/runtime/slot_manager.h` — and the
task resolves it on resume.

### What is proved

- **`gen_monotone`**, **`despawn_advances`** — generations never move
  backwards, and a despawn strictly advances its slot.
- **`resolve_sound`** — a resolved reference names the current incarnation.
- **`stale_never_resolves`** — after any run containing a despawn of the slot,
  the old reference resolves to nothing, even when the slot is live again with
  a new occupant.
- **`resolved_means_same_incarnation`** — a reference that still resolves saw
  no despawn of its slot: the entity it names is the one it was taken from.
- **`unchecked_deref_hits_new_occupant`** — dereferencing by slot id alone,
  which is what carrying a live view across `await` amounts to, touches a live
  slot whose occupant is not the one the reference was taken from. docs/204
  appendix A.14's "stale async problem", machine-checked.
- **`revalidation_not_vacuous`** — a run that despawns a different slot keeps
  the reference alive.

### What is not established

The generation is an unbounded `nat`; the runtime's is `uint32_t`, so
wrap-around after 2^32 reuses of one slot is outside the model. Who bumps the
counter, and that the compiler inserts the resume-time resolve, is docs/204
§4 item 5 — the checked suspension contract `docs/107` left a place for.
`AsyncModelCores.md` lists "checked Slot revalidation after resume" as outside
its two cores; this core is where that claim now lives, at model level.

## 4. `DeterministicSubsetCore.v` — the admitted subset is schedule independent

A state maps locations to values. A task has a read footprint, a write
footprint and a body; the body changes nothing outside its write footprint
and what it writes depends only on what it reads or writes. Two tasks are
`independent` when neither writes a location the other touches: `docs/178`'s
Disjointness and Exclusivity evidence, and `WitnessDataRace.v`'s `xor_mut`
read at footprint granularity.

### What is proved

- **`commute`** — independent bodies commute, pointwise.
- **`run_permutation`** — for a pairwise-independent family, any two task
  orders that are permutations of each other end in the same state.
- **`deterministic_subset`** — every schedule of tasks `0..n-1` ends in the
  canonical index-order state. This is docs/204 §2.6's definition of the
  admitted subset, "observable result equals canonical sequential execution",
  as a theorem about the model, and it is what makes "sequential execution is
  a legal lowering" and "an executor that changes the result is an executor
  bug" (`docs/186` §3) sound.
- **`footprints_not_vacuous`** — the hypotheses hold for real bodies, and the
  theorem yields a concrete equality.
- **`write_conflict_is_schedule_dependent`** — two tasks writing one location
  end differently in the two orders, and
  **`conflicting_pair_not_independent`** — that pair fails `independent`, so
  the theorem never spoke for it.

### What is not established

Tasks interleave as units; independence by footprint makes finer
interleavings agree as well, but that refinement is not mechanised. The fold
is `ParallelReductionCore.v`'s. Who computes the footprints, and that the
compiler admits exactly the pairs this file calls independent, is the
boundary-witness refinement `docs/semantics/10` §7 records. No function
extensionality is assumed: every equality between states is pointwise.

## 5. Scorecard against docs/204 §5

| docs/204 theorem | landed-contract core | direction core | what the direction core adds |
|---|---|---|---|
| 1. Capability Non-Forgery | `AsyncContextCore.v` (exact capture) | `CapabilityFlowCore.v` | lend/move carriage, loan uniqueness, the executor-default counterexample against a narrowed manifest |
| 2. Data-Race Freedom | `WitnessDataRace.v` | — | consumed as the admission evidence of core 4 |
| 3. Slot Temporal Safety | `SlotCalculus.v` (one context) | `SuspensionRevalidationCore.v` | the suspension: stale never resolves, resolved means same incarnation |
| 4. Structured Task Containment | `AsyncLifecycleCore.v` (one handle) | `AsyncScopeCore.v` | the scope tree: subtree cancellation, detach as capability, the pre-lifecycle orphan |
| 5. Deterministic Parallel Subset | `ParallelReductionCore.v` (the fold) | `DeterministicSubsetCore.v` | the bodies: any schedule equals canonical order |

The models are deliberately small; the theorems are about the design, and
the refutations are about the alternative the design rules out. Landing the
rungs docs/204 §4 orders is what turns each refutation into a re-pointing of
`tests/async_direction_adequacy_smoke.sh`.

## 6. Re-checking

```
make async-direction-adequacy-test-smoke  # models still describe the code
bash tests/coq_kernel_check.sh            # theorems hold, axiom budget pinned
```

The kernel check is the one that matters for the proofs. All four cores
add zero axioms: the corpus budget stays at the two declared abstractions
in `SlotCalculus.v`.
