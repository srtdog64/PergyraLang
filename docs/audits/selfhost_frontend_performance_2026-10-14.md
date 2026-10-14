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
  15–20 are compared by user CPU time; see their subsection.
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

### Stages 15–20

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
| 20 | `67d1b1c15` | the core verdict and the scalar type resolved one call's target once per fact (1,471,533 resolutions for 42,586 call nodes) | 7.9 s | 17.6 s |

The two earlier rotations, on the loaded machine, give the steps between:
codegen 12.0 → 10.4 (stage 15) → 9.9–10.2 (16–17) → 9.9 s (18), and driver
28.5 → 25.3 → 23.8–24.0 → 23.2 s. Stages 15–18 cut user CPU by about 18% on
the codegen input and 17% on the driver input.

Stage 19's row comes from a later rotation against main before 15 (10.3 s /
24.1 s) and stage 18 (8.5 s / 19.6 s). Stage 20's comes from a rotation
against stage 19 (8.1 s / 18.0 s); its gain is small, 3% and 2%, but the run
ranges did not overlap. Stage 19 changed a file that also
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

### Stages 21–29

| Stage | Commit | Repeated work removed |
|---|---|---|
| 21 | `23d81dd9f` | a local looked up by walking every local of its function (3,131,924 lookups on the codegen input) |
| 22 | `4a1124c84` | each new function table row looked up by scanning every earlier row; codegen user CPU about 1.4% lower |
| 23 | `a71f95d26`, `0b448a681` | every builtin name reservation split once per signature, the pieces never freed; the export word looked up in the word registry at every offset of an imported source |
| 24 | `ef2b8d4ed`, `5436ce84e` | a one-character string per identifier character, never freed; the environment scanned twice per value or mode lookup |
| 25 | `9271f7520` | literal checks built a one-character string per character; a call target fact built a spine view for a root that is not a call |
| 26 | `55842c700`, `f54eca9ae` | none; it restores the negative gates that stages 4, 6 and 16 broke (next subsection) |
| 27 | `9fdb707d9` | parser functions holding the content's extent measured the content again in each whitespace skip (171 calls in 18 functions); parenthesis stripping measured its text four times; AST text kind checks measured the text twice per prefix (up to 99 prefixes) |
| 28 | `0894932e3` | each callee name compared with all 23 collection protocol names; the sink alias fixed point walked every local of the program in each round |
| 29 | `4afe1e0b6` | the same parser functions measured the content again for character reads, language word matches, identifier reads and loop bounds |

Stages 23–24 lowered peak memory from 1.46 to 1.31 GB on the codegen input
and from 3.17 to 2.87 GB on the driver input. One rotation, five runs each,
user CPU, median:

| Build | codegen | driver |
|---|---:|---:|
| stage 24 | 7.55 s | 16.86 s |
| stage 25 | 7.49 s | 16.88 s |
| stage 26 | 7.59 s | 16.94 s |

Stage 25 is within noise on the driver input. Stage 26 costs about 1% on the
codegen input; that is the price of the restored checks.

Stages 27–29 were each timed against the stage before them, five runs per
build, user CPU, median:

| Step | codegen | driver |
|---|---:|---:|
| stage 26 → 27 | 7.64 → 7.30 s | 17.02 → 16.37 s |
| stage 27 → 28 | 7.25 → 7.16 s | 16.51 → 16.09 s |
| stage 28 → 29 | 7.19 → 7.08 s | 16.02 → 16.10 s |

Stage 29's gain shows on the codegen input only. The driver input stays
within the noise.

Since stage 20, `SemanticStripOuterParens` has stayed at about 3% of the
driver input. The parser wraps every binary node's text in parentheses, so
each such node needs a full depth scan. A shortcut would have to rely on how
the parser builds those texts. `ParserExpressionBinary` copies both child
arenas, texts included, for every binary node. Building into the left
arena in place would mean mutating a by-value parameter's member arrays,
and that pattern is what the P0 refusal is about.

### Fact frames edited in place

Three stages moved a check or a lookup out of each use and into the facts
producer, and each commit said nothing changes the facts afterwards:

- stage 4 ran `SemanticAstEnumRowsReady` once in the enum facts producer and
  folded the result into `ok`;
- stage 6 indexed nominal declarations by name;
- stage 16 kept a length-and-first-byte key beside every local name.

The negative probes do change them. They edit one row of admitted facts in
place, or build a facts struct by hand with `ok` copied from good facts,
and expect the consumer to refuse. Five such checks in four probes failed
on stage 24 and passed on `af8ede6f0`:

- `collection_constructor_escape_identity_probe` mutation 1 renames a
  second declaration to the constructor's name. The index still counted one
  row, so the duplicate was not refused.
- The same probe's mutation 12 renames a local to the constructor's name.
  Its stale key hid the local from the lookup.
- `collection_member_identity_probe` mutation 29 and the last check of
  `nominal_constructor_field_identity` make the same kind of duplicate.
- `enum_qualified_environment_fact_probe` builds enum facts with wrong
  qualified names and the good facts' `ok`. The seed accepted them.

No CI job ran these probes. Every Linux job waits for
`self-host-codegen-bootstrap-linux`, and that job stops at the P0 ownership
refusal. The lane's struct changes had also broken 34 test programs that
import self-host owners. A native type check of all 162 such programs on
`af8ede6f0` and on the lane found them. After the fixes, none differ. The
negative probes were found by running each touched parity script on both
trees.

Stage 26 changes:
- **Local name keys removed.** Stage 21 already made the walk short.
- **The nominal name index stays, and is checked.** A lookup refuses when
  the indexed row no longer carries the name. The artifact match check
  compares each name's index entry with the rebuilt facts' entry. An index
  cannot see another row renamed to the same name unless it reads every
  row, which is the scan stage 6 removed. The two rename probes therefore
  and the field identity check rebuild the index with the producer's
  `SemanticAstNominalConstructorNameIndexFill` after renaming, so the
  duplicate is a real one, and the lookup still refuses it.
  `nominal_constructor_field_identity` gains a check: a forged index entry
  fails both the artifact match and the lookup. The stage 24 code passes
  both of those, so the new check falsifies it.
- **The enum seed runs `SemanticAstEnumRowsReady` again.** Since stage 7
  the walkers no longer call the seed for every expression root.

The first version compared the index sizes with `MapSize` on a facts
field. The native compiler accepted it. gen0 refused its own source with
`ast_artifact_invalid` (`owner: expression_graph_type`) at the first
expression root, before any ownership check. So did
`collection_borrowed_descriptor_read`, whose observer analyzes a source
that imports the owner. The per-name comparison alone is enough, because
a key that no row carries fails the lookup's row check.

## What remains (stage 20, both inputs)

The stage 20 profile is flat. Outside the ownership verdict family, no
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
  Stage 21 made each lookup walk back only from the use (stage 26 removed
  stage 16's name key); the repeated calls remain.
- **Call targets resolved again inside one verdict, about 3% after stage 20
  (4.5% before).**
  `SemanticExpressionGraphResolvedCallTarget` ran 1,471,533 times, about 35
  times per call node. Stage 20 shares the root target inside the core
  verdict and the scalar type. Argument facts, domain queries and the call
  error path still resolve their own, and each verdict resolves the same
  call again for every subtree it checks. A memo for one verdict would be
  exact, but it has to pass through about fifteen verdict functions. The
  whole expression verdict family is about 13% including callees.
- **Parsing, about 9% including callees.** It is spread across the
  recursive-descent functions. Stages 17 and 18 removed the content
  measurements in the hot ones. The other parser functions that hold an
  extent still call `MatchLanguageWord` and `ParserCharAt`.
- **The rest is under 3% each.** `SemanticStripOuterParens` (1,568,274 calls)
  normalizes each node's text once. `SemanticExpressionGraphArenaOverlayReady`
  runs at each admission because body passes write overlays.
  `AstExpressionGraphRowsReady` checks the parser rows five times between
  the parser and the semantic graph producer.
