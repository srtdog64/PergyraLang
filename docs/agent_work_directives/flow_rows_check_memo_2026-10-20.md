# Flow rows check memo: closure-blocking cost

Status: IMPLEMENTATION CANDIDATE, Claude lane, single implementation lane.
Base revision `db8350709` on `claude/p0-native-checker`.

## Objective card

- **Objective.** The native checker's census of the canonical driver input
  takes about 41 minutes per semantic analysis, and a census with landing
  rounds runs up to four analyses (95 to 109 minutes). That is beyond the
  30-minute integration budget, so every change on the refusal fronts
  waits hours for its census. Once the refusals reach zero, a normal
  compile does the same work. Cut the repeated work without changing any
  answer.
- **Priority.**
  1. No check answers differently: the census refusal set is identical.
  2. A reuse that no longer holds fails closed, and the verify mode checks
     every reuse in full.
  3. Time.
- **Fact owner.** The flow summary store's rows check,
  `function_param_flow_backing_rows_capture`.
- **Last consumers.** `function_param_flow_output_current`,
  `function_param_flow_returned_backing_effects`,
  `function_param_flow_declared_result_frame_current`,
  `sparse_loan_admitted` and `function_param_flow_backing_unsupported`.
- **Forbidden:**
  - a reuse keyed by an epoch or counter alone;
  - skipping a check whose input can still change;
  - a larger budget, a smaller input or a weaker check.
- **Gates.**
  - The flow gate in normal and `PGY_VERIFY_FLOW_CURRENCY_MEMO` modes.
  - One mutation per memo condition.
  - A census-mode analysis of the same input, compared with the
    unchanged commit: same refusal set, and its time.

## Measurement

The fixed input is `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
Each run is one census-mode analysis (every error downgraded, every final
owner run) of the census tree.

- The `c425c09d2` census took 2489 s and 3.47 GB peak.
- gprof of one analysis at `db8350709`: the rows check is 81.9% of the
  time, inclusive. Its 163,914 calls walked 11.37 million routine visits
  (69 per call) and made 945 million callee lookups. Each lookup rechecks
  typed call facts (11.2 billion publication row lookups in all).
- Counters on the same analysis:
  - 7,809 demands;
  - 825,426 output currency queries, of which 135,381 replayed (85,579 of
    them in a solve);
  - every one of the 163,914 rows checks passed;
  - the rows checks took 2638 s of the 2993 s.
- A simulation stamped what each routine's own check reads and compared it
  with that routine's previous visit in the same mode. The stamp matched on
  3,682,046 of 3,690,607 query visits and 7,643,133 of 7,683,507 solve
  visits, and the outcome never differed.

## Repeated operation

A rows check walks the routine's callee closure. For every routine in it,
the check revalidates each frame's rows against the typed facts and the
store. The output currency memo keeps an answer for one demand only. Each
of the roughly 8,000 demands starts a new epoch, so the next queries walk
the same unchanged closures again.

## Design

Typed facts are sealed while the flow store lives: a demand requires
sealed typed inputs, and the store exists only after the final
publication. What can change is the store. But a complete entry and a
built frame that is neither building nor pending never change again.

- **Shape.** A routine's check reads its entries, its frames (one per
  formal, plus the declared result) and its claims. A shape snapshot
  records the entry index and frame of each formal, the claim count and
  whether claims failed. Its version changes whenever a refresh finds the
  snapshot different, settledness included. It is settled when every entry
  is complete and every frame is settled.
- **What else the check reads.** For the calls its rows name, it reads the
  callee entries and frames those rows were built from. Outside a solve
  every entry is complete, and the check passes only on settled callee
  frames, so that state no longer changes. Every such callee is also
  reached by the same capture and checked there. So a callee's later
  change needs no tracking in the caller's memo.
- **Memo.** A routine's own check that passed outside a solve, on a
  settled shape, is kept with:
  - its shape version;
  - the callees it reached, which a reuse enqueues;
  - the checks of state outside any shape, which a reuse runs again:
    no-store graphs and read-only receivers.
- **Reuse.** A memo answers again, in a solve or a query, when the shape
  still has that version and the repeated checks pass. A solve admits
  everything a query admits, so a query's pass holds there too.
- **Verify mode.** `PGY_VERIFY_FLOW_CURRENCY_MEMO` also runs the full
  check on every reuse. The store fails if the full check refuses, or if
  it reaches other callees, no-store routines or receivers than the memo
  holds.
- **Rejected.** A memo of the whole closure, keyed by a fingerprint of
  every routine in it. One changed routine would invalidate every closure
  through it, and the fingerprint would have to cover state that a
  routine-level memo never reads.

## Also in this slice

The pending and unsupported relation checks look for the node row that
owns the relation: the row that holds its call, or the row that holds an
intent call whose step makes it. Each node row tried the intent step
test, which is a typed call lookup, even when another row held the call
(4.0 billion tests in one analysis). The check now looks for the row that
holds the call first, and tries the intent step test only when no row
does. The answer is the same: both tests read only sealed facts.

One rows capture also asked the same typed questions of an argument once
per frame of its routine (about four times on average): is it checked
with a current value, and which routine does it bind to. A capture writes
no typed fact, so the capture now keeps each argument's answers until it
returns. The verify mode reads every reuse again and fails the store on a
different answer.

Every lookup of an existing program-point index checked its claims again in
full: each claim row against the typed facts, each row's empty-consumer
answer, and the control projection (15.96 million checks in one analysis,
about 100 claim rows each). The claims change only when a row is added or
promoted after the index's snapshot, which writes rows and the projection
once while the index is built and before any check of it. So the claims
keep a generation that those two writes change, and a passed check holds
again while the generation and the index's local count are the same and
the claims have not failed. The verify mode checks every reuse in full
and fails the store on a divergence. The unit fails when the generation,
the local count or the failed flag is not compared, or when either write
does not change the generation.

## Results

| Run (one census-mode analysis, same input) | Wall | Peak |
|---|---|---|
| `db8350709` | 2605 s / 2613 s | 3.47 GB |
| memo, checks kept outside a solve only | 1542 s | 3.47 GB |
| memo, checks kept in a solve without pending relations | 1547 s | 3.47 GB |
| and the owner-row test order and the argument answers below | 1206 s (unchanged run: 2607 s) | 3.47 GB |
| and the index claims check reuse below | 235 s (unchanged run: 2582 s) | 3.47 GB |

- Each memo run was started together with an unchanged run, with up to
  two other analyses running on the 16-core machine.
- Every memo run had the same 3890 downgraded refusals as the unchanged
  run, line for line. The flow store statistics were also identical:
  14,544 entries, 18,989 evaluations, 7,970 fixed-point passes.
- The second run reused 6.78 million of the 11.37 million routine visits
  without being faster, so the rest of the time is in checks a memo cannot
  answer and in each visit's index lookup.

## Gates

- The flow summary gate passes in both modes, with no divergence. The
  verify mode compares the callees, no-store routines and receivers a full
  check reaches with the memo's as well. In the fixtures, 138 of 598
  programs reuse a check (672 reuses).
- `tests/semantic/flow_rows_check_memo_unit.c` forges store rows around a
  parsed routine. Each memo condition removed makes it fail:
  - the version comparison;
  - the settled shape required to keep a check;
  - the pending relation condition in a solve;
  - the repeated no-store graph and read-only receiver checks.
- The flow summary gate cannot falsify those conditions. In a valid store
  every rows check of a completed routine passes, so nothing a memo skips
  changes an answer there.
