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

- **Machine:** WSL Ubuntu, 16 cores, one run per input. Through stage 14
  every number is single-threaded wall time from `/usr/bin/time`. Stages
  15–19 are compared by user CPU time; see their subsection.
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

Overall to stage 14: codegen input 190 s → 10.6 s (18×), driver input
716 s → 24.4 s (29×). Both stage 13–14 numbers are the fastest of three
rotation runs against stage 12 (11.0 s / 27.2 s). Peak memory stayed
between 1.9 and 4.4 GB throughout.

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

### Stages 15–19

Another program kept the machine busy during these stages, and wall time
varied by up to 2× between runs of one build. They were compared by user
CPU seconds, fastest of three rotation runs, with main before stage 15 in
every rotation. All three rotations gave the same order. The last one ran on
a quieter machine:

| Stage | Commit | Repeated work removed | codegen | driver |
|---|---|---|---:|---:|
| main before 15 | `259c13fe0` | | 10.2 s | 23.3 s |
| 15 | `63eaeeb35`, `03e939a56`, `7b0b6899d`, `9903bea9c` | an error verdict built and thrown away for each checked statement; a parent walk for each local of each environment seed; receiver rows compared pairwise; every callable name rewritten before lookup; the graph shape checked again after call-target capture and both attaches | | |
| 16–17 | `761f820a6`, `a37a89118` | the name of each earlier local compared as a string; keyword checks measured the whole file | | |
| 18 | `be8d84b05` | character reads measured the whole file | 8.4 s | 19.4 s |
| 19 | `dd38f6511` | each round of the collection field-read closure listed and indexed every key again (11 rounds over up to 63,988 keys) | 8.0 s | 18.4 s |

The two earlier rotations, on the loaded machine, give the steps between:
codegen 12.0 → 10.4 (stage 15) → 9.9–10.2 (16–17) → 9.9 s (18), and driver
28.5 → 25.3 → 23.8–24.0 → 23.2 s. Stages 15–18 cut user CPU by about 18% on
the codegen input and 17% on the driver input.

Stage 19's row comes from a later rotation against main before 15 (10.3 s /
24.1 s) and stage 18 (8.5 s / 19.6 s). Stage 19 changed a file that also
has another lane's uncommitted edits; a three-way merge of those edits onto
it applies without conflict. Its closure gives the same key sets as the old
rounds on both inputs (63,988 and 162,729 keys). The first versions moved
key snapshots through an owned array, and the self-host checker refused the
function itself (`owned_element_escape`, then
`unproved_formal_element_use_entry`); only the gen0 verdict showed it.

Wall time, fastest of three rotation runs. Stage 18 against main before 15:
8.5 s / 21.3 s against 10.6 s / 24.7 s, the latter matching the stage 13–14
row. Stage 19 in a later rotation: 9.0 s / 20.5 s against 11.8 s / 26.8 s,
so the machine ran about 10% slower that time. Both rotations put stages
15–19 at about 23% less wall time than main before stage 15. Scaled to the
stage 13–14 row, that is about 8.2 s and 19 s: codegen input 190 s → about
8.2 s (23×), driver input 716 s → about 19 s (37×).

Three attempts were measured and dropped:
- **Build generic specialization spine views only at call nodes** (after
  stage 18). `SemanticAstGenericSpecializationFactsFromAdmittedBody` walks
  the whole graph twice and builds a call spine view at every node,
  1,343,205 of the 5,589,068 views on the codegen input. Skipping non-call
  nodes gives the same facts, because an argument row's view equals its
  call node's view and the call node has the lower id. User CPU did not
  move (8.35 s / 19.41 s against 8.36 s / 19.35 s), so it was not committed.
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

## What remains (stage 19, both inputs)

The stage 19 profile is flat. Outside the ownership verdict family, no
function holds more than about 5% of the samples. Call counts below come
from a gprof build on the codegen input, which has 63,308 verdict roots and
42,586 call nodes.

- **Another lane's files: the collection ownership verdict, 28% of the
  driver input including callees** (33% before stage 19, which removed
  `SemanticAstCollectionExclusiveFieldReads`, about 10%). Most of what is
  left sits in owners with large uncommitted edits from that lane, such as
  the consumption owner.
- **Leaf identities resolved again by name, 4–5%.**
  `SemanticAstScopedLocalBindingIdentityForName` ran 2,044,720 times, almost
  all through `SemanticAstScopedLocalBindingIdentityForGraphLeaf` from the
  same ownership family, for the same leaves again and again. The graph
  also carries identity overlay rows (`binding_syntax_ids`,
  `binding_ordinals`). This audit did not check whether they answer the
  same question. Either way the fix is a consumer change inside that
  family, and the identity resolution owner is also in that lane's work.
  Stage 16 made each lookup cheaper; the repeated calls remain.
- **Call targets resolved again inside one verdict, about 4%.**
  `SemanticExpressionGraphResolvedCallTarget` ran 1,471,533 times, about 35
  times per call node. Scalar types, return types, generic captures,
  argument facts and constructor facts each resolve the target again. Its
  inputs are fixed for one verdict, so a memo per verdict would be exact,
  but it has to pass through about fifteen verdict functions. The whole
  expression verdict family is about 12% including callees.
- **Parsing, about 9% including callees.** It is spread across the
  recursive-descent functions. Stages 17 and 18 removed the content
  measurements in the hot ones. The other parser functions that hold an
  extent still call `MatchLanguageWord` and `ParserCharAt`.
- **The rest is under 3% each.** `SemanticStripOuterParens` (1,568,274 calls)
  normalizes each node's text once. `SemanticExpressionGraphArenaOverlayReady`
  runs at each admission because body passes write overlays.
  `AstExpressionGraphRowsReady` checks the parser rows five times between
  the parser and the semantic graph producer.
