# Parallel model cores

Companion to `ParallelSchedulingCore.v` and `ParallelReductionCore.v`.

Pergyra's parallel runtime was built by measurement and refutation: a hang was
witnessed, a mechanism was written, the hang went away. That is good evidence
that the mechanism *works*, and no evidence at all about *why* it works, which
inputs it covers, or whether the next change breaks it. These two proof cores
supply the missing half. Each mechanism in the runtime becomes a rule in a
model, the property it was written to establish becomes a theorem, and the
alternative that was tried and abandoned becomes a machine-checked
counterexample against that same model.

Neither file claims the implementation is verified. They state what is true of
the *design*, and `tests/parallel_model_adequacy_smoke.sh` binds each modelled
decision back to the source line it was transcribed from, so the model cannot
silently stop describing the code.

## 1. `ParallelSchedulingCore.v` — the pool cannot deadlock

A worker is a **stack** of task frames, so help-nesting (running a queued task
on top of the frame that is awaiting) is representable rather than abstracted
away. `WRun []` is an idle worker. Three await policies share one step
relation:

| policy | await behaviour | runtime site |
|---|---|---|
| `PolParkOnly` | always parks the worker | the classic bounded pool; what Pergyra used to do |
| `PolHelpFirst` | drains the queue first, parks only when it is empty | `pgy_await` → `pgy_pool_help_run_one` |
| `PolCompensate` | parks, but queued work with no runner adds a spare | `pgy_pool_spawn_spare_locked` |

Everything turns on `push`, the order in which frames were pushed, and one
hypothesis:

```
spawn_tree :  awaits h t  ->  push h < push t
```

*A task only awaits something pushed after it.* That holds exactly when a task
awaits only tasks it spawned — the join lane — and fails for a channel receive,
where the producer being waited on may have been pushed long before the waiter.

### What is proved

- **`help_first_progress`** — under `PolHelpFirst` with spawn-tree awaits, no
  non-final configuration is stuck. Bounded workers, unbounded nesting, no
  deadlock. The argument: in an all-parked, empty-queue configuration every
  parked worker's target sits on another parked worker's stack, and spawn_tree
  makes that worker's head strictly newer — so "waits-for" strictly increases
  `push_head`, which a finite worker list cannot sustain.
- **`help_first_progress_is_not_vacuous`** — the hypotheses are jointly
  satisfiable (instantiated at `push := id`, `awaits := (<)`), so the theorem
  above is not true by contradiction.
- **`park_only_deadlocks`** — `PolParkOnly` reaches a stuck configuration in
  three steps from a one-worker pool: take a task, spawn a child, await it. The
  WO-RT-3 hang, machine-checked.
- **`help_first_never_parks_with_work`** — from that same configuration
  help-first cannot produce a parked worker *at all*. The fatal state is not
  avoided by luck; it is unreachable.
- **`cyclic_await_deadlocks`** — help-first is *not* enough once awaits may
  cycle: two workers park legally (the queue really is empty each time) and
  deadlock. **`cyclic_await_breaks_spawn_tree`** shows a cycle is precisely a
  spawn_tree violation, so the hypothesis is load-bearing rather than
  decorative.
- **`help_in_cyclic_wait_self_deadlocks`** — the refutation the runtime records
  in a comment, as a theorem. Helping inside a cyclic wait pushes the helper
  *above* the task being awaited on the same worker, so the worker is parked on
  something buried under its own frame. Stuck — and impossible under spawn_tree,
  because a stack head is the newest frame while an awaited task must be newer
  than its awaiter.
- **`compensation_moves_where_the_others_stick`** — one configuration, three
  verdicts: stuck under `PolParkOnly`, stuck under `PolHelpFirst`, steps under
  `PolCompensate`.
- **`help_first_preserves_queue_runner`**, **`help_first_preserves_desc_stacks`**
  — the two invariants that carry the progress proof are preserved by every
  rule, so the theorem applies to reachable configurations and not only to
  hand-written ones.

### Scorecard

|  | nested fan-out (spawn tree) | cyclic wait (channel) | threads |
|---|---|---|---|
| `PolParkOnly` | **deadlock** | **deadlock** | bounded |
| `PolHelpFirst` | progress | **deadlock** | bounded |
| `PolCompensate` | progress | progress | bounded + spares |

The runtime runs help-first on the join lane, where spawn_tree holds and no
spare thread is ever needed, and compensation on the channel lane, where it does
not. Neither mechanism is redundant and neither generalises to the other's lane.

## 2. `ParallelReductionCore.v` — the join is schedule- and worker-invariant

`parallel (i in 0..n) join with op` cuts the range into `k` contiguous chunks
(`k = pgy_parallel_chunk_count n`), runs the chunk tasks concurrently with each
writing its per-index result into a dedicated cell, and then folds **the cells
in index order**. So neither the schedule nor `k` — a function of the worker
count, i.e. of the machine — should be visible in the result.

### What is proved

- **`join_schedule_invariant`** — any two completion orders covering the range
  fold to the same value, for an **arbitrary** `op`: no associativity, no
  commutativity, no identity element is assumed anywhere.
- **`chunk_tiles`** — the runtime's `lo`/`hi` arithmetic tiles `[0, n)` exactly
  and in order for every `k ≥ 1`; the concatenation of the chunk index lists is
  literally `seq 0 n`. Chunking re-associates the *spawn*, never the index
  sequence.
- **`join_chunk_count_invariant`** — hence two worker counts fold to the same
  value. This is the "semantics byte-preserving" claim of the auto-chunk landing
  (WO-RT-4 B3) as a theorem rather than a measurement, and it is what the
  `PGY_WORKERS=1 2 3 16` byte-identical witness observes empirically.

### What the alternatives cost

Both competing shapes are defined in the same file and refuted against their own
definitions:

- **`completion_order_matters`** — reducing in completion order (folding results
  as they arrive) needs `op` **commutative**, or the answer depends on thread
  timing. Counterexample uses list append, the collection join.
- **`chunk_count_matters`** — folding each chunk independently and combining the
  chunk results (the OpenMP `reduction` / tree-reduce shape) needs `op`
  **associative**, or the worker count leaks into the answer. Same three values,
  two chunk counts, two answers.
- **`index_fold_survives_nonassociative`** — `join_chunk_count_invariant`
  instantiated at exactly the non-associative operator that just broke the
  tree-reduce.

Non-associativity is not a corner case: IEEE floating-point addition is
non-associative, which is why reproducibility across worker counts is normally
lost. `Nat.sub` stands in for it so the counterexample is exact rather than
epsilon-sized.

|  | schedule-invariant | worker-count-invariant |
|---|---|---|
| completion-order reduce | only if commutative | only if commutative |
| chunk / tree reduce | yes | only if associative |
| index-order cell fold | **yes, unconditional** | **yes, unconditional** |

The price of the bottom row is that the reduction itself is serial: the
parallelism buys the per-index bodies, not the fold. That is a real cost, and it
is the same fact as the missing algebraic side condition.

## 3. Adequacy — what is and is not established

**Bound to the code** by `tests/parallel_model_adequacy_smoke.sh`: the chunk
partition expressions verbatim, the help-first consult-then-park shape of
`pgy_await`, the compensation spare, and the reduce loop running to the index
count rather than the chunk count. If any of those change, the gate fails
instead of the proofs quietly going stale.

**Not established.** These are scheduling and reduction models, not a memory
model — no atomics, no happens-before, no C11 ordering; `push` abstracts a real
clock. Progress means "some rule applies", i.e. absence of deadlock, not
termination and not fairness. Two of the four invariants consumed by
`help_first_progress` (park well-formedness and target location) are structural
bookkeeping, asserted rather than derived. The cell model assumes each index is
written once with the same value; enforcing that is the outer-write rejection in
the front end (`tests/cases/parallel_join/reject_outer_write.pgy`). Data-race
freedom of the chunk bodies is `WitnessDataRace.v`, not here. And the binding
from these models to what the C and LLVM emitters actually produce remains what
the parallel gates check empirically.

## 4. Re-checking

```
make parallel-model-adequacy-test-smoke   # model still describes the code
bash tests/coq_kernel_check.sh            # theorems still hold, axiom budget pinned
```

The kernel check is the one that matters for the proofs: `coqc` only says the
elaborator accepted a file, while `rocqchk` re-runs the trusted kernel over the
compiled `.vo` and pins the assumption base. Both cores add **zero** axioms —
the corpus budget stays at the two declared abstractions in `SlotCalculus.v`.
