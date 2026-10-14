# Self-host front end performance (2026-10-14)

Status: `AUDIT COMPLETE`. These are measurements for one lane's performance
commits. They own no semantics and are not a performance gate or threshold.
Lane: Claude, on the V1 self-host path
(`agent_work_directives/v1_scope_ownership_and_selfhost_generality_2026-10-14.md`).

## Question

The bootstrap's first stage, gen0, is the self-host compiler built by the
native compiler. Running it over the compiler's own source took minutes:
190 s on `codegen/main.pgy` and 716 s on `compiler/driver_bootstrap_main.pgy`.
Where did that time go, and how much of it was repeated work that one owner
could do once?

## Method

- **Machine:** WSL Ubuntu, 16 cores, one run per input. Every number is
  single-threaded wall time from `/usr/bin/time`.
- **Inputs:** gen0 built with
  `bin/pgy src/self_hosted/codegen/main.pgy --native-pipeline --backend=c`,
  then run as `gen0 --source <input>` on the two sources above.
- **Profiles:**
  - First, gprof (`-pg`). gprof misses libc time and inflates loops that
    make many calls, so its percentages overstate some functions.
  - From stage 5 on, a SIGPROF sampler loaded with `LD_PRELOAD`. Each
    sample records the PC and the return addresses. Time spent in
    malloc, free, strcmp and strdup is charged to the nearest compiled
    Pergyra caller.
  - Neither tool is committed. Both live in the lane's scratch space.
- **Behavior gates, checked at every stage before its commit:**
  - Both inputs stop at the same first refusal,
    `borrow_boundary_escape / unproved_exclusive_member_mutation`. The node
    id moves only with added code.
  - The ownership scan census
    (`p0_ownership_refusal_census_2026-10-14.md`) still finds 9 and 97
    refusals of the same kinds and counts. The late
    `aggregate_release_incomplete` refusal stays at its original node.
  - `generic_return_probe --callable-resolution` matches
    `callable_resolution_expected.txt`.
  - The eight self-host entry points type-check natively with no errors.
  - From stage 7 on, gen0 output is byte-identical to the previous stage's
    on 200 example and test programs that declare enums. 120 of them
    compile to C, and 80 stop at the same diagnostics.

## Results

| Stage | Commit | Repeated work removed | codegen | driver |
|---|---|---|---:|---:|
| base | `af8ede6f0` | | 190 s | 716 s |
| 1 | `1d9e00053` | signature lookup by canonical name scanned every row | 110 s | 368 s |
| 2 | `3e9a121f1` | callable lookup scanned every function name twice | 73 s | 208 s |
| 3 | `13c1c7af1` | enum seed rescanned the environment per value; alias scan looked up every program local | 29 s | 89 s |
| 4 | `f362d022a` | consumer scan recomputed node kinds; enum rows revalidated per seed | 28 s | 71 s |
| 5 | `2ddda171a` | arity per scanned row; quadratic field id uniqueness check | 27 s | 65 s |
| 6 | `f5304f511` | nominal declarations looked up by scanning names | 27 s | 57 s |
| 7 | `7e6f44948` | about 900 enum values copied into every expression environment | 14 s | 35 s |
| 8 | `df2e7b21a` | second walk over locals just to push their modes | 14.0 s | 34.7 s |
| 9 | `85d16ffa3` | 0.4–1.7 KB facts structs copied per seed call | 13.5 s | 33.8 s |
| 10 | `461f6e475` | whole-program graph rows rechecked on every artifact admission | 12.2 s | 31.8 s |
| 12 | `7e2077927` | semantic graph shape rechecked by about 28 admissions | 10.9 s | 30.6 s |
| 13–14 | `e01f7e4ce`, `6e0aaca0f`, `34d2fb3bf` | export scan measured the file per offset; forward parent search per leaf; intent seeds copied facts | 10.6 s | 24.4 s |

Overall: codegen input 190 s → 10.6 s (18×), driver input 716 s → 24.4 s
(29×). Both stage 13–14 numbers are the fastest of three rotation runs
against stage 12 (11.0 s / 27.2 s). Peak memory stayed between 1.9 and 4.4 GB throughout.

The table gives one run per stage. Repeat runs of the same build differ by
10–20%, which is more than stages 8–10 changed. Those four builds were
timed again in rotation, three runs each, all on the stage 10 source. The
table below gives the fastest run:

| Build | codegen | driver |
|---|---:|---:|
| stage 7 | 13.4 s | 34.0 s |
| stage 8 | 13.0 s | 34.0 s |
| stage 9 | 12.7 s | 33.8 s |
| stage 10 | 11.6 s | 31.8 s |
| stage 12 (separate rotation with stage 10: 11.7 s / 30.4 s) | 11.0 s | 30.4 s |

Stages 8 and 9 are small steps in the right direction. Stage 10 gives a
steady 6–8%. Stage 12 removes most of the graph readiness samples (377 of
3124 → 46 of 2777). It shows on the codegen input but not beyond noise on
the driver input.

Two attempts were measured and dropped:
- **Check only the new root when the parser combines expression arenas.**
  It saved nothing measurable. The binary-node arenas it skipped were
  small. Most of the time is in whole-program row checks.
- **Pass the typed AST arena by ref to its accessors** (stage 11, not
  committed). There was no
  difference beyond noise; the C compiler likely already inlines these
  small accessors. The first version also passed a borrowed `Array<Int>`
  on to a by-value parameter (`ChildAt` → `ChildAtFromEdges`). The native
  type check accepted that, but gen0 refused it with `borrow_boundary_escape`
  at node 15463. Only the verdict gate caught it. The verdict gate needs
  gen0 itself; type-checking the self-host source is not enough.

## What remains (sampled on stage 10, codegen input; stage 12 removed the first item)

- **Done in stage 12: whole-graph validation, about 12%.** The shape rows
  (topology, texts, payloads, children) are now checked once when a
  surface graph is finished. Overlays (call targets, identities, place
  kinds) are still checked at every admission, because body passes write
  them. A generation counter was rejected: about ten files write overlays
  through unencapsulated fields, so one missed bump would skip a check on a
  changed graph.
- **About 11%: per-root environment construction.**
  `SemanticAstExpressionSeedVisibleLocals` and
  `SemanticAstScopedLocalBindingIdentityForName` both walk scopes through
  `TypedAstArenaParentId`.
- **Done in stage 13: `MatchKeyword` on the export scan.** The scan now
  passes the extent it holds (`MatchKeywordWithin`), so it no longer
  measures the file at every offset. It accounted for 75 of the 140
  `MatchKeyword` samples. The other parser callers still measure their
  content per call. Switching the callers that hold a `content_extent`
  parameter made the native extent prover refuse unrelated calls in
  `ParseMatchStmt`. That step waits on the prover.
- **Done in stage 13: the parent search in
  `SemanticAstBorrowedTextConsumer`.** A sealed surface graph now carries
  each node's first consumer, so the lookup is constant time. Callers in
  the exclusive-value family are unchanged.
- **Done in stage 14: intent environment seeds** take their facts by ref.
- **Still open: `SemanticAstCollectionExclusiveFieldReads`, about 5%.** Its
  own loop sits in the exclusive-value owner family, which another lane's
  uncommitted work is restructuring.
