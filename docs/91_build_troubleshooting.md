# Build Troubleshooting

마지막 업데이트: 2026-05-24

빌드/회귀 도중 자주 마주치는 문제와 대응. **항상 `mingw32-make rebuild`를 먼저 시도**하면 절반은 풀린다.

---

## 0. Resource pressure first

If the desktop hangs during local builds, check disk and scratch pressure before
debugging compiler logic.

Observed local pressure pattern:

- `make all` builds only `pgy` and `pgy-lsp`; test binaries are behind
  `all-with-tests` and test targets.
- The default build keeps debug symbols off (`PGY_DEBUG_SYMBOLS=0`). Use
  `PGY_DEBUG_SYMBOLS=1 mingw32-make all` or `mingw32-make debug` only when
  symbolized debugging is intentional.
- `make clean` removes only the active `BUILD_DIR` and `BIN_DIR`.
- Ad-hoc roots such as `build-codex*`, `bin-codex*`, `build-llvm*`, and
  `.tmp/self_hosted/*` are intentionally ignored, but they can accumulate.
- LLVM-enabled links are the heaviest local step. With low free disk, linker and
  test scratch writes can make the machine look frozen.

Useful commands:

```sh
mingw32-make build-resource-report
PGY_BUILD_RESOURCE_DEEP=1 mingw32-make build-resource-report  # slower exact sizes
mingw32-make build-pressure-dev-compiler # samples pgy-only build RSS/private bytes
mingw32-make build-pressure-compiler     # samples default LLVM-enabled compiler build
mingw32-make build-pressure-self-host-compiler # samples the Pergyra-built DRV-2 build
mingw32-make self-host-driver-bootstrap-full-test-smoke # full fixpoint; pressure-wrapped on Windows
mingw32-make clean-scratch              # removes .tmp only
mingw32-make clean-local-artifacts      # removes build/bin, .tmp, build-*, bin-*
```

The default resource report is intentionally shallow. It lists artifact roots
and free space without recursively counting files. Use
`PGY_BUILD_RESOURCE_DEEP=1` only when you need exact size/file-count evidence;
on Windows/Git Bash, scanning tens of thousands of scratch files can itself
make the desktop feel stalled.

`build-pressure-dev-compiler`, `build-pressure-compiler`, and
`build-pressure-self-host-compiler` are the memory bug lines. They run the
low-pressure C-only compiler build, the default LLVM-enabled compiler build,
and the Pergyra-built bounded DRV-2 build through
`scripts/measure_build_pressure.ps1`, then sample the process tree. The default
limit is 3 GiB (`PGY_BUILD_PRESSURE_LIMIT_MB`), and all three targets stop the
measured tree when it crosses that line. If one compiler build crosses the
line, treat it as a build/compiler memory defect until the sample log proves
otherwise. Do not use a broad parity matrix's system-wide memory total as the
compiler-build measurement. This is separate from disk/file-count pressure: a
full artifact scan can stall the desktop with small RSS when the repo drive is
nearly full.

The same rule applies to self-host stage tools. A `--check` mode must validate
the stage contract without materializing a full generated artifact unless that
artifact is the thing being tested. 2026-07-09 evidence: self-host codegen
`--check` over an 840 KiB compiler AST peaked at about 3.4 GiB when it called
the full C-emission path; after splitting the check path into structural
subset verification, the same input peaked at about 76 MiB. Treat a future
`--check` path that builds full generated C text as a memory regression.

Do not run broad CI targets when the repo drive has less than about 10 GiB free.
Use the narrow gate named by the source-of-truth seam first. For low-pressure
local builds, prefer:

```sh
mingw32-make dev-compiler              # C-only, no debug symbols, pgy only, serialized
PGY_DEV_COMPILER_JOBS=4 mingw32-make dev-compiler  # explicit opt-in parallelism
mingw32-make LLVM_ENABLED=0 all        # C-only, pgy + pgy-lsp
mingw32-make abi-ownership-shape-test-smoke
```

2026-07-09 local measurement on Windows/MinGW, with tests excluded and debug
symbols off:

- clean `dev-compiler` rebuild after removing `build-dev` / `bin-dev`: peak
  sampled working set 290.5 MB, peak sampled private memory 266.4 MB, top
  process `cc1.exe` at 243.2 MB;
- clean default `compiler` rebuild after removing `build` / `bin`: peak sampled
  working set 385.4 MB, peak sampled private memory 364.3 MB, top process
  `cc1.exe` at 357.2 MB;
- local artifact pressure can still dominate perceived hangs. The resource
  report on the same checkout showed the E: drive at 99% used with about
  15.5 GiB free, many local `build-*` / `bin-*` variants, and more than 28k
  files under the active `.tmp` / `build` / `bin` sample. In that state, broad
  local CI may stall from file churn even when compiler RSS stays under 400 MB.

2026-07-24 Windows/UCRT64 incident evidence separated the build units again:

- a clean `release` rebuild completed in 1,576,373 ms; a second isolated LTO
  relink with detached MSYS compiler-worker tracking peaked at 490.3 MB working
  set and 444.1 MB private memory, with `cc1.exe` the largest process;
- a fresh Pergyra-built bounded DRV-2 build completed in 351,507 ms, producing
  a 2,927,734-byte AST, 2,959,613-byte C unit, and 2,397,166-byte driver. It
  peaked at 1,343.8 MB working set and 1,412.2 MB private memory; `gen2.exe`
  owned 1,134.1 MB of private memory;
- the DRV-2 result is below the 3 GiB hard ceiling, but a 1.1 GiB codegen seed
  for a roughly 3 MiB artifact is explicit optimization debt. Do not describe
  it as normal just because the compiler is being self-hosted;
- the observed desktop pressure also had an unfiltered Git Bash DRV-2 wrapper
  whose worker survived as a reparented native process, a replacement full
  matrix started on top of it, and the D: volume at 97% use. The shallow
  resource report correctly warned that broad CI could stall;
- `measure_build_pressure.ps1` now attributes detached `cc1`, LTO, linker, and
  Pergyra seed workers by probe start time. All compiler pressure targets stop
  the measured workers at 3 GiB instead of reporting only after completion.

These measurements do not claim that the released compiler is self-hosted.
DRV-2 remains a bounded Pergyra-built source/MIR-to-C replacement. C and LLVM
are the native compiler's peer production backends; compiling a self-host tool
through both backends is parity evidence, not evidence that the Pergyra-built
driver owns a self-hosted LLVM emitter.

There is also a distinct, confirmed full-input defect. An earlier isolated
`driver_mir_oracle --emit-mir-json-verified` run over the driver source reached
approximately 17 GiB RSS / 28 GiB private memory and produced no artifact
before it was stopped. That is not a normal compiler build and it is not
excused by self-hosting: it is unresolved full-driver semantic-to-MIR pipeline
amplification. On Windows, the official
`self-host-driver-bootstrap-full-test-smoke` entry now runs inside the same
3 GiB hard pressure boundary and attributes reparented `driver_oracle`,
`driver_seed`, and `driver_genN` workers. Do not invoke the script directly
with `PGY_SELFHOST_DRIVER_FULL_FIXPOINT=1` when investigating this defect.

Two bounded builds isolate this defect from ordinary native linking while also
showing real compiler-scale optimization debt. Compiling the approximately
3 MiB driver source to a guarded oracle through the released compiler's C
backend completed in 74,025 ms at 2,138.8 MB working set / 2,145.6 MB private.
The LLVM backend build completed in 147,566 ms at 2,228.2 MB working set /
2,239.5 MB private. These are compile-to-executable measurements, not the
full-input oracle execution. They show that large-source compilation already
needs optimization, but they do not explain away a later 28 GiB oracle process.

The C- and LLVM-built guarded oracles both reject a direct full-driver MIR
request before materialization, reject use of the full-fixpoint token on a
bounded fixture, and emit the same 2,341-byte `let_log` MIR artifact. This is
runnable guard parity as well as lowering/linking evidence. The CLI order is
`mode, source, output[, token]`; reversing mode and source reaches the usage
diagnostic and is not evidence of an argv backend defect.

An actual pressure-owned C-oracle execution over the full driver source then
ran for 170,534 ms. The wrapper stopped the process tree at 3,079.2 MB private
memory / 2,549.3 MB working set; `driver_oracle_guard.exe` itself owned
3,030.0 MB private. It produced no MIR artifact and left no oracle process.
This is the expected current falsifier and proves the 3 GiB boundary is active;
it does not close the underlying semantic projection defect.

Pressure-only stage markers now locate the current 3 GiB crossing before MIR
or JSON construction:

- AST construction and the initial typed semantic analysis both complete;
- driver readiness completes and body-type projection starts;
- the first base-initializer projection starts, while its completion marker,
  iteration projection, MIR-fact construction, and JSON projection are never
  reached;
- a finer isolated C-oracle run completed initializer rows 0 through 5,003,
  then crossed the cap at row 5,004 before that row's environment marker. It
  was stopped after 165,336 ms at 3,074.4 MB private / 2,527.8 MB working set
  and produced no artifact.

This falsifies the earlier JSON-leading hypothesis for the current 3 GiB
boundary. JSON emission still has nested `Array<String>` / `Concat` lifetime
debt, but it cannot cause a run that has not entered MIR or JSON. Do not tune
JSON first or describe the historical 28 GiB peak as a measured JSON share.

The exact cause is repeated whole-graph readiness, not one exceptional
initializer and not the backend. A synchronized marker run crossed the same
cap at local row 3,343 (`peak_private_mb=3072.6`). Private memory rose from
532.8 MB at row 0 to 3,001.2 MB at row 3,250, approximately 0.76 MB per local
row; graph-root, verdict, and row-completion markers were flat inside each
sampled row.

An allocator-interposed generated seed attributed each environment interval
to about 400 bytes of ordinary mallocs but 1,572,828 requested realloc bytes
across 32 realloc calls, with no frees. The large realloc stack is:

`SemanticAstInitializerTypeFactsFromArtifactWithIterationRowsObservedWithFunctionTables`
-> `SemanticAstExpressionSeedVisibleMatchBindings`
-> `AstTreeArtifactReady`
-> `AstExpressionGraphRowsReady`.

`AstExpressionGraphRowsReady` constructs whole-graph `seen` and `stack`
arrays. The initializer outer pass had already proved
`AstTreeArtifactReady(artifact)`, but the match-binding seed repeated that
proof for every local row. The full driver contains 8,149 local rows and the
diagnostic AST contained no `Match` or `Case` node, so even the empty-case
path paid for whole expression and match graph validation before it could
return. In short: a once-per-artifact proof was accidentally executed
once-per-local.

The repair keeps readiness with one Pergyra owner. The initializer outer pass
validates artifact and expression-surface readiness once, then calls
`SemanticAstExpressionSeedVisibleMatchBindingsFromReadyArtifact` inside the
row loop. That borrowed entrypoint checks only its local ready-artifact
contract and must not invoke `AstTreeArtifactReady` or
`AstExpressionGraphRowsReady`. The original checked entrypoint remains for
standalone callers and delegates to the borrowed core after performing the
once-owned proof.

`semantic_expression_environment_owned_lifetime_smoke.sh` and the component
contract reject a checked match-binding call in the initializer hot loop or a
whole-graph readiness call in the ready-artifact core. The executable
verification remains initializer C/LLVM parity followed by the official
full-driver request under the unchanged 3 GiB cap. Raising the cap, splitting
the compiler into per-chunk processes, skipping validation, or mirroring the
fix in separate C/LLVM fragments is not a repair. C and LLVM remain peer
consumers of the one Pergyra-owned semantic/MIR/ABI fact spine.


The first post-fix official full-driver run confirms that this specific seam is
closed but does not close the full 3 GiB gate. All 8,149 initializer rows
completed through `row:done:8148`; the run then entered
`semantic-body-type-stage call-targets:start`. The pressure owner stopped that
later stage after `7,992,190 ms` at `peak_private_mb=3074.3` and
`peak_working_set_mb=2521.4`; `driver_oracle.exe` owned 3,063.3 MB private and
the outer target returned `Error 88`. No full MIR artifact was produced. Thus
the old per-local readiness reconstruction is no longer the 3 GiB crossing,
while call-target resolution is the next falsifying owner boundary. Record
these as two separate results: initializer readiness amortization is green,
the end-to-end pressure gate remains red.

A focused current-source pressure shard then applied the same ready-artifact
contract to `SemanticAstAnalysisResolveCallTargetsFromBody`. It completed
`call-targets:done` and `initializer-refine:done`, then entered
`expression-places:start`. The unchanged pressure owner stopped that next
consumer after `328,425 ms` at `peak_private_mb=3072.8` and
`peak_working_set_mb=2514.6`; the oracle owned 3,071.5 MB private. This is
positive evidence for the call-target consumer migration, not a full green
result. The active falsifier has moved to expression-place resolution, which
still used the checked match-environment entrypoint at this checkpoint.

Three subsequent current-source shards moved that same owner contract through
the remaining hot semantic-body consumers. The expression-place shard
completed `expression-places:done` and `assignment:done`, then stopped at
`statement:start` after `266,437 ms` (`peak_private_mb=3076.9`,
`peak_working_set_mb=2519.2`, oracle private 3,075.7 MB). The statement shard
completed `statement:done`, then stopped at `generic:start` after `274,579 ms`
(`peak_private_mb=3074.7`, `peak_working_set_mb=2529.0`, oracle private
3,073.5 MB). The generic shard completed `generic:done`, `verdict:done`,
`body-types:ready`, and `verify:done`, then stopped at `mir-facts:start` after
`264,914 ms` (`peak_private_mb=3073.5`, `peak_working_set_mb=2531.1`, oracle
private 3,072.3 MB).

Each migrated consumer now proves expression-surface readiness once at its
owner boundary and uses
`SemanticAstExpressionSeedVisibleMatchBindingsFromReadyArtifact` inside the
row/surface loop. The lifetime smoke gate rejects restoration of the checked
entrypoint. These observations prove executable movement through the complete
semantic-body bundle; they do not make the full pressure gate green. No full
MIR artifact was produced, and the active falsifier is now MIR-fact
materialization. Keep the 3 GiB cap unchanged and diagnose that Pergyra owner
instead of adding backend-local C/LLVM state.

### MIR readiness and nested JSON materialization

The next current-source runs separated two more defects. First, the verified
driver had already completed `SemanticAstBodyTypeBundleReady` but called the
checked `SelfMirProgramFactsFromArtifact`, repeating the whole-semantic proof
at the MIR boundary. The driver now calls
`SelfMirProgramFactsFromReadyArtifact`; the checked entrypoint remains for
standalone callers and delegates after its own proof. The focused run then
completed `mir-facts:start` instead of crossing the cap, peaking at 2,865.8 MB,
and failed closed on a concrete invariant:

```text
MIR assignment target binding type drifted: target=base node=5290
local_type=ParserExpressionFact semantic_type=String
```

Node 5290 belongs to `ApplyPostfixFact` and represents `base.text = ...`.
Comparing the root-local type to the final selected member type was valid only
for a direct `x = ...` target. The MIR owner now performs that equality check
only when `target_text == target`; composite targets retain root-local
existence plus their semantic target graph and final type facts.

That correction exposed a distinct JSON high-water mark. All later runs
completed `mir-facts:done` and entered JSON projection:

| label | elapsed ms | peak private MB | last evidence |
|---|---:|---:|---|
| `assignment-composite-ready` | 1,095,642 | 3,233.9 | `json:start`; no artifact |
| `json-builder-ready` | 936,636 | 3,195.6 | shared `TextBuilder`; still `json:start` |
| `json-file-ready` | 960,383 | 3,290.1 | 20,013,056-byte partial file; whole routine string |
| `json-block-file-ready` | 999,598 | 3,197.3 | 20,901,888-byte partial file; instruction strings |

The original shared emitter allocated one `Substring` per character, then
`StringJoin` plus nested `Concat` copies for each object and array. It now
uses span-based escaping and exact-capacity `TextBuilder` assembly. The
production MIR artifact path also writes schema tokens, declarations,
specializations, captures, routines, and blocks directly to one file owner;
the bootstrap CLI no longer stores a whole-program `mir_json: String` before
`WriteFile`. A small fixture is byte-identical to the prior self-host path at
11,262 bytes with SHA-256
`007d5dacdd8157a0d5dd0f87975f82c7abe2fa4987983afb3945bd61b29efc09`, and a
shared JSON escape/object probe emits identical bytes through C and LLVM.

These changes are real executable replacements, but they do not make the full
pressure gate green. The last run was stable near 2,933 MB immediately before
JSON writing, then retained per-instruction/field strings until the cap. A
complete full-driver MIR artifact still does not exist. Do not respond by
raising the cap, adding a C- or LLVM-specific serializer, or splitting the
compiler into process chunks. The next active owner is the initializer
expression environment: static inventory shows the current visible-local
reconstruction is O(sum of squared local counts), including about 8,000 locals
in the largest function. Replace that with one scope-aware sequential cursor,
then rerun this same pressure gate. `FileWrite` currently has no observable
write-status result after a valid `FileOpen`; the new writer therefore proves
open failure and byte parity but does not claim atomic or error-reporting file
semantics that the runtime does not yet expose.

#### Sequential initializer environment cursor result

Checkpoint `ffe31ce8` replaces the two per-row full-function local scans in the
initializer production loop. The cursor seeds enum/owner/parameter rows once
per function, keeps visible locals as a lexical suffix, pops that suffix on
scope exit, and appends transient iteration/match rows only for the current
verdict. It delays local publication until the current syntax node completes;
all rows from one destructure node become visible together. Local identity,
source order, and scope still belong to `SemanticAstLocalBindingFacts` and the
typed AST arena, so the cursor is not a second semantic authority.

The executable C/LLVM parity covers outer-shadow reads, outer binding
restoration after a nested scope, and atomic destructure publication.
Self-reference and sibling-scope leakage both fail with `undefined_symbol`.
The static cursor gate rejects the retired full-range calls inside the
initializer loop and forbids owned-string insertion into the borrowed
environment arrays.

The exact committed pressure result is:

| label | elapsed ms | peak private MB | peak working set MB | partial artifact |
|---|---:|---:|---:|---:|
| `initializer-cursor-ready` | 869,913 | 3,117.9 | 2,601.7 | 13,709,312 bytes |

The run completed base initializer rows 0 through 8,228, the remaining
semantic-body passes, verification, and MIR facts. It crossed the unchanged
3072 MB ceiling only after `json-write:start`, while writing routine
`SemanticExpressionGraphNodeKind`. `driver_oracle.exe` owned 3,116.7 MB
private and no measured process remained afterward. Relative to
`json-block-file-ready`, time to the cap improved by 129,685 ms and sampled
peak overshoot was 79.4 MB smaller. Do not interpret the latter as 79.4 MB of
reclaimed live state: both runs are kill-on-limit samples, and the new pre-JSON
baseline was still about 2,937 MB.

This falsifies the cursor as the remaining JSON crossing. The next production
owner is the instruction serializer called by
`SelfMirJsonBlockWriteFile`: it still materializes one complete
`SelfMirJsonInstruction` string, including nested expression graphs, before
each `FileWrite`. Stream those fields from the same MIR facts, retain the
String serializer only as a fixture bridge, and require byte-exact C/LLVM/file
parity. Do not split policy between backends or add a second MIR fact read.

#### Sequential instruction writer and call-local JSON leaf result

Checkpoint `e5587bee` removes that aggregate production call. The
responsibility-named instruction artifact writer preserves the canonical
instruction field order while directly framing expression-graph nodes,
match/destructure arrays, uses, and runtime-call ABI auxiliary rows. It does
not introduce another MIR store or backend-specific serializer;
`SelfMirProgramFacts` remains the sole semantic owner. The old String
projection remains a fixture oracle only. The focused gate feeds the exact
same facts to String and file projections, compares raw bytes without newline
or canonicalization normalization, compiles the probe through C and LLVM, and
covers small, graph-heavy, match, destructure, and ABI/optional fixtures.
Invalid instruction row counts are rejected before `FileOpen`, so a sentinel
artifact is not truncated.

The first fixed-cap result isolated one more lifetime seam:

| label | elapsed ms | peak private MB | peak working set MB | artifact |
|---|---:|---:|---:|---:|
| `instruction-stream-ready` | 810,472 | 3,092.7 | 2,574.5 | 40,263,680-byte partial |
| `instruction-string-pool-ready` | 675,355 | 3,064.3 | 2,544.9 | 51,807,108-byte complete |

`instruction-stream-ready` completed all 8,266 current initializer rows,
semantic verification, and MIR facts, then started JSON at about 2,956 MB.
It advanced almost three times as many artifact bytes as
`initializer-cursor-ready`, but private memory rose from 2,956.1 MB to
3,092.7 MB over the final two samples. Direct graph framing had removed the
large nested object copies, while every `JsonStringLiteral` still promoted
its escaped and quoted results into `AllocatorResult()`. Because
`AllocatorDestroy` has no pool to release in that mode, synchronous
`FileWrite` did not end those lifetimes.

Checkpoint `6329356f` adds an allocator-parameterized JSON string emitter and
uses it only at the file boundary. `JsonStringLiteralWriteFile` sizes a
call-local pool for the worst supported escaping expansion, writes the
literal synchronously, and destroys the pool after `FileWrite` returns. The
production instruction writer now uses that boundary for all unbounded graph
and list string leaves. Numeric `ToString`, fixed ABI layout objects, and
program/routine framing were not changed without evidence.

The successor run exited 0 below the unchanged 3072 MB ceiling. Its top
`driver_oracle.exe` owned 3,063.1 MB; there were two measured processes and no
compiler/link subprocess. The output is a complete `pgy.mir.v1` document with
SHA-256
`1621adf4070bc778dd90493e29db857c22f13722d951bea8a94d1241e9ee884e`,
2,345 routines, 142 declarations, a closing `]}`, and a successful full JSON
parse. The pressure log reached `json-write:done`. This is the first complete
current full-driver MIR artifact under the 3 GiB gate.

Do not overstate the margin: the sampled peak is only 7.7 MB below the cap,
so this closes artifact production but not the broader compiler memory debt.
Do not raise the limit or recreate facts in a second process. The next rung is
the existing Pergyra MIR consumer: consume this exact completed artifact to
emit gen2 C, compile it, and run the bounded generated-driver parity preflight.
`FileWrite` still has no status return, so success here means valid open,
completed process, byte parity, complete JSON, and observed stage completion;
it is not a new claim of atomic file-write error reporting.

### Full MIR consumer: low memory, high CPU, repeated `strlen`

The completed 51,807,108-byte artifact exposed a different defect in the
`--mir-json` consumer. The process stayed between roughly 50 and 64 MB private
while holding one CPU core, produced no partial C, and timed out before machine
admission completed. This is not the earlier 3 GiB production-memory defect.

The first consumer implementation used row-index lookups that rebuilt root and
array tables. Replacing those with a carried `MirProgramRoutineIndex` and
sequential block/instruction cursors removed the logical O(N-squared) lookup,
but the generated C still called `strlen(json)` inside every cursor and field
read. For 2,345 routines, 20,022 blocks, and 34,091 instructions, the three
cursor calls alone implied about 8.8 TB of avoidable byte walking. Routine
`kind`/`owner`/`name` reads added further whole-document length discovery.

Checkpoint `e9592a6a` establishes these rules:

- a path input creates one typed machine admission and carries the declaration
  and routine index used by that proof;
- exact-bound JSON access is available only for spans produced by a validated
  structure owner; general JSON callers retain their ordinary length boundary;
- machine validation uses sequential exact routine/block/instruction bounds,
  and the old row-index restart calls are statically rejected;
- declaration phases reuse one document-order declaration inventory;
- expression-graph node arrays use sequential cursors rather than count/index
  restarts.

The fixed 300-second observations show the progression:

| label | peak private MB | last stage |
|---|---:|---|
| `full-mir-consumer-admitted` | 53.0 | `machine-layer:start` |
| `full-mir-consumer-bounded-cursor` | 54.8 | `routine-index:start` |
| `full-mir-consumer-exact-bound` | 59.3 | `routine-index:done`, then `instruction-scan:start` |
| `full-mir-consumer-machine-twofield` | 63.6 | `instruction-scan:start` |
| `full-mir-consumer-key-compare` | 57.1 | machine/input admission done, then `mir-to-ast:start` |

`0857899e` removes normal-key allocation from the exact-bound field reader and
keeps bounded decode only as an escaped-key fallback. The final row proves
`instruction-scan:done`, `machine-layer:done`, and `input:done`; machine
admission is no longer the first CPU blocker. The run is still RED because it
timed out after `consumer:mir-to-ast:start`. Do not report gen2 or bootstrap
completion, raise the timeout as a substitute for ownership work, skip
validation, or add a C/LLVM-specific parser. The next falsifier is
`consumer:mir-to-ast:done` using the same admitted routine and declaration
inventories.

#### Exact MIR-to-AST span consumption result

Checkpoint `157c340b` carries the admitted structure spans into the first real
MIR-to-AST consumers instead of reopening document-wide discovery. The owner
changes are deliberately one-way:

- declaration fields take their array bounds once and advance with
  `JsonArrayNextObjectBounds`;
- method lookup returns the routine-index row, so class, role, and top-level
  callers all pass the indexed routine start and end;
- the old `RoutineObjectEnd`, `RoutineNameEnd`, and document-end fallback are
  deleted and statically rejected;
- routine iteration/resource/loop/CFG/local facts use only exact `AtBounds` or
  `Within` reads over structure-owner spans.

The 142 declarations contain 1,110 fields. The retired indexed field loop made
9,902 object visits including terminal checks and, through its generic JSON
path, at least 47,290 whole-document `strlen` calls: 2,449,958,137,320 bytes of
logical walking. `full-mir-consumer-exact-span` then reached the new
`consumer:mir-to-ast:declarations:done` marker before its 300-second timeout;
peak private was 58.0 MB and peak working set was 70.7 MB.

The next exact routine-fact bundle removes the generic block successor,
instruction-array, and instruction-kind reads. On 20,022 blocks and 34,091
instructions, those retired paths represented a measured lower bound of about
118.9 TB more whole-document walking. The successor run
`full-mir-consumer-routine-fact-exact` reached
`consumer:mir-to-ast:first-top-level-routine-fact-index:done`, timed out after
300,425 ms, and remained at 58.0 MB peak private / 70.8 MB peak working set.
No gen2 C file was opened. A bounded MIR fixture still emits the prior exact
414 bytes (SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`).

#### Routine-consumer CPU closure after exact spans

Checkpoint `d62553ee` keeps routine header, instruction result, phi-use,
match/destructure, render, and ABI reads behind the admitted routine/index
owners. In particular, every instruction `result` is decoded once into
`MirRoutineFactIndex`; phi validation does not reopen every instruction JSON
for every use. Match binding arrays are captured sequentially instead of using
count-plus-index restart reads. These are CPU-complexity changes, not a larger
memory allowance.

The same checkpoint moves structural-merge selection into the pure CFG graph
owner. The old path ran two blocked BFS queries for every candidate block,
giving worst-case O(B^3) work and repeated `visited`/`queue` allocation. The
new path computes two blocked reachability arrays per conditional branch and
uses them only for eligibility. It deliberately retains unrestricted distance
for ranking, candidate order, strict `<` ties, terminal fallback, and detached
component behavior. `mir_cfg_graph_query_owner_smoke.sh` proves those
conditions through both C and LLVM; the component contract rejects return of
the retired candidate-local query.

Measured evidence:

| Slice | Result | Peak private | Peak working set | Last evidence |
| --- | --- | ---: | ---: | --- |
| `mir-routine-indexed-consumer-driver-build` | exit 0, 52,074 ms | 2,427.8 MB | 2,416.3 MB | integrated driver compiled |
| `full-mir-consumer-routine-indexed` | timeout, 300,471 ms | 58.0 MB | 70.7 MB | first top-level routine done; no gen2 output |
| `mir-cfg-owner-driver-build` | exit 0, 58,512 ms | 2,422.7 MB | 2,411.3 MB | integrated driver compiled |
| `full-mir-consumer-cfg-owner` | timeout, 300,687 ms | 57.8 MB | 68.7 MB | first top-level routine done; no 16 marker or gen2 output |
| `mir-document-index-driver-build-v2` | exit 0, 57,528 ms | 2,319.9 MB | 2,322.4 MB | one document index and bounded string reads compiled |
| `full-mir-consumer-document-index` | timeout, 300,554 ms | 63.4 MB | 74.0 MB | reached 16 top-level routines; no gen2 output |
| `mir-program-instruction-index-driver-build-v3` | exit 0, 50,974 ms | 2,405.9 MB | 2,409.3 MB | admitted structural view and O(1) routine row guard compiled |
| `full-mir-consumer-program-instruction-index-v3` | timeout, 300,606 ms | 85.2 MB | 93.6 MB | still reached 16 top-level routines; no gen2 output; cap not crossed |

The full artifact has 2,345 routines, 20,022 blocks, 34,091 instructions,
3,532 phi rows, and 214,151 expression-graph nodes. Its first top-level
routine is only 2,063 bytes with one block/instruction, so that routine is not
the 300-second cause. The diagnostic window still spends most of its time in
the admitted input/machine path and accumulated routine work. Do not raise the
3072 MB cap or the 300-second focused window to hide that cost; close the next
owner-directed scan and require `consumer:mir-to-ast:done` before claiming
gen2.

A streaming routine-object audit makes the coarse markers explicit. Routines
1-64 total 274,581 of 51,741,503 bytes (0.531%); the tail from routine 65 owns
99.469% of bytes. Reaching 16 or 64 is therefore only a CPU progress sentinel,
never a bootstrap or completion verdict.

The next audit found that an API being named `Bounded` was not enough to make
its returned String bounded in cost. The full artifact stores
`machine_layer:null` in all 34,091 instruction rows. Each row verified the
four-byte token with `Substring(json, start, 4)`, and native `Substring` first
calls `strlen(json)`. That is about 1,766,156,118,828 bytes (1.766 TB) of
logical document walking plus 34,091 unnecessary token allocations. Routine
kind/name decoding reached the same materialization path at least 4,690 times,
another 242,975,336,520 bytes (243 GB), before counting nonempty owners.

The closure is shared by C and LLVM rather than attached to a backend. The
machine null check uses `SubEqualsWithLen`; `ReadJsonStringBounded` builds the
result from `CharAtN(json, limit, ...)` and never calls `Substring(json, ...)`.
`json_bounded_string_owner_smoke.sh` proves normal, escaped, empty, and
truncated inputs on both backends, and the component gate rejects restoration
of the unbounded materialization. The production hard path also carries one
`MirDocumentFactIndex` instead of independently rebuilding the root for schema,
parallel capture, and routine admission.

This change does not complete self-hosting. It improved the fixed window from
one completed top-level routine to 16. Checkpoint `190d0dbf` then captured one
program/routine/block/instruction view for machine admission and
`MirRoutineFactIndex`, eliminating that second structural walk. Review also
removed a whole-program structure validator accidentally repeated per routine.
The v3 run nevertheless remained at 16, proving these were real but not
dominant costs.

#### Repeated instruction-object reads and phi wire semantics

The next detailed run separated a low-memory CPU defect from the earlier 3 GiB
readiness defect. Converting fact accessors to `ref` did not improve v9:
routine 16 still completed at 133,593 ms, with 82.6 MB peak private. The
initial interpretation that `JsonObjectFactTableFromBounds` copied the complete
51.8 MB source `String` into each local table was wrong. Generated C passes a
`String` as `char *`; the table copies only that pointer and its bounds. The
measured CPU cost came from reconstructing/revalidating the instruction table
and rescanning the same object to rediscover fields and value bounds.

Checkpoint `06f6994d` makes the common ABI/resource decisions directly from
the admitted instruction bounds. A local instruction table is constructed only
when a nested fact is actually present. In the same marker slice, ABI
validation fell from 492 ms to 9 ms, resource validation from 646 ms to 0 ms,
and routine 16 from 133,593 ms to 69,919 ms. Use only pressure records with
`output_capture_complete=true` for these comparisons.

That speedup exposed a separate producer-wire mismatch. MIR `phi.uses` is an
incoming value inventory, not a predecessor-indexed native machine phi table.
`FindTopLevelComma` has seven CFG predecessors at its loop header but only two
inventory values. The fail-closed condition is therefore
`2 <= use_count <= predecessor_count`; a self-result input is valid only when
the CFG owner proves an incoming backedge. v11 passed that counterexample and
continued to routine 64.

CFG successors are now decoded once into integer block identities. Missing
fields alone become the internal negative sentinel; an explicit negative wire
successor is rejected by a C/LLVM executable ratchet. Do not use `own` to force
borrowed string arrays through the BFS helpers: the correct boundary is typed
integer identity, and generated value-array calls copy only their descriptor.

The v13 full run timed out at 180,056 ms with 88.6 MB peak private / 96.6 MB
working set, routine 64 at 99,447 ms, and routine 128 at 164,457 ms.
`limit_exceeded=false` and `output_capture_complete=true`. The final v14
integrated driver built in 48,451 ms at 2,442.7 MB peak private and preserved
the 414-byte bounded output SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
No complete gen2 file exists.

Keep the two memory stories distinct. The historical 3 GiB defect ran
once-per-artifact readiness once per local and repeatedly revalidated a whole
graph. The current consumer defect stays around 83-94 MB peak private /
91-102 MB working set and accumulates CPU work after routine 128. Preserve the
180/300-second diagnostic window and 3072 MB cap, then close only the next
measured routine-owner seam.

Checkpoint `dd68d6f3` moves the next local reads behind one
`MirRoutineInstructionFactBundle`. Each routine fact-index construction captures
`result`, render scalars, ABI type, slot anchor, and match variant in one pass
over the program-owned instruction spans. The bundle is deliberately
routine-local: adding these rows to `MirProgramRoutineIndex` would mix program
structure with local fact
lifetime. Duplicate/non-string scalars and a count that would cross into the
next routine fail closed. Phi predecessor count is now computed only for a
block that actually contains a phi, while the incoming-backedge answer comes
from the canonical routine fact index rather than another dominator walk.

The current v23 integrated build exited 0 in 47,746 ms at 2,509.8 MB peak
private / 2,498.5 MB working set. The bounded output remains exactly 414 bytes
with SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
The full 180-second run timed out at 180,343 ms with only 87.0 MB peak private /
95.3 MB working set, reached routine 64 at 96,607 ms and routine 128 at 160,331
ms, and opened no gen2 output. Against the v14 300-second run's routine-128
marker, the same marker moved from 165,019 ms by 4,688 ms. This proves a real
but non-dominant CPU seam; it is not a memory regression or self-host
completion. The active MIR-to-AST
reconstruction reuses the bundle, but the later expression-graph and assignment
post-passes still reconstruct routine indexes; that re-entry remains open.

The filtered `dir_walk,break_after_stmt` broad parity attempt currently fails
earlier when the reconstructed C lacks `PGY_RUNTIME_PANIC` declarations. Keep
that compile RED separate from CFG analysis. It is neither proof against this
optimization nor a successful runtime parity result.

The later v74 direct-CFG gate isolates `break_after_stmt` from that unrelated
runtime-header RED. Its first implementation failed because the common
certificate assumed that every phi-bearing merge block used the earlier
`AST_IDENTIFIER` branch anchor. A loop-break header owns an `AST_BINARY`
condition instead. The repair does not weaken all phi checks: it admits that
anchor only when the separately issued six-block break fact is ready. The
break fact still fixes the exact header/decision/break/continuation/exit roles,
one phi, one while summary, and eight instructions.

Two negative mutations can fail at the earlier machine-layer admission before
the break-specific diagnostic: changing the break block identity invalidates
the admitted CFG, and injecting a partial statement row invalidates the typed
instruction envelope. That is a valid pre-artifact rejection, not evidence
that the break owner consumed the mutation. Keep owner-specific mutations for
the remaining break row, edge, forwarded predecessor, SSA use, graph, and
repaired-digest cases; do not force an earlier invalid artifact past its real
owner merely to obtain a later diagnostic.
The current focused body-gate attempt similarly stops at
`valid_array_builtins`: emitted C lacks `<string.h>` plus the runtime panic
declarations. A separately isolated current-driver `nested_if_in_loop` MIR
round trip passes, and injecting a one-predecessor header phi is rejected with
the owned phi diagnostic. Record the broad gate as RED until its runtime-header
owner is fixed.

This is still RED bootstrap evidence: the active seam is after the first valid
top-level routine index and before `top-level-routines:done`. Keep the 300
second diagnostic window and 3072 MB cap fixed. Do not replace the remaining
reads with another parsed document, backend-specific path, or source recovery.

Two broad gates currently stop later for unrelated existing reasons. The
machine smoke reaches MIR lowering and then reports `local declaration is
missing its MIR ABI type fact`. The full MIR JSON parity expects enum variants
without the current `param_types:[]` field. Preserve both as explicit red
evidence rather than calling this consumer slice fully green.

#### Required ABI rows: outer bounds were not the bottleneck

The v29-v37 observation ladder separated routine-index construction from
statement rendering and then split each focused instruction into ABI,
resource, and render markers. The result falsified the first outer-scan
hypothesis. In v37, required Array rows cost about 1.35 seconds each and
required Option rows about 1.09 seconds each; optional rows cost about 9 ms.
The repeated work was inside the nested layout owner:
`JsonObjectFactTableFromBounds`, repeated `HasField`/value scans,
`JsonArrayObjectFactAt` restarting from the first field, and a second complete
walk in `MirAbiLayoutIdFromRow`.

The v38 experiment captured the four outer ABI value spans in the existing
instruction scalar pass but left nested validation unchanged. That was useful
negative evidence, not a speedup. Its 300-second run used 92.1 MB peak private /
100.0 MB working set and reached routine 248 at 293,877 ms, compared with
v37's 290,268 ms. The focused required-row ABI total was effectively unchanged
at 50.72 seconds. Do not report a renderer marker moving earlier when the same
work merely moved into fact-index construction.

Checkpoint `a5d56f42` keeps wire interpretation in
`abi_layout_fact_owner.pgy` and replaces the nested repeated reads with one
order-independent row capture plus one field-array walk. Canonical hash order
is applied after capture, so JSON field order does not become authority. A
maximum of eight layout fields matches the native contract. Missing,
duplicate, wrong-kind, invalid identity, and truncated carried bounds fail
closed. The old instruction-span validator and old repeated-scan hash owner are
deleted; `MirAbiLayoutIdFromRow` now delegates to the same captured identity
implementation used by the final consumer.

The v39 full run timed out at 300,560 ms with 134.7 MB peak private / 140.8 MB
working set, but progressed far further: routine 128 at 90,643 ms, routine 192
at 102,775 ms, routine 248 at 115,450 ms, routine 448 at 231,271 ms, and routine
640 at 298,374 ms. Against v38, routine 192 improved by 130,742 ms (56.0%) and
routine 248 by 178,427 ms (60.7%). It still did not reach
`consumer:mir-to-ast:done` and did not open gen2 output. The exact final-source
v40 driver then built in 55,007 ms at 2,565.3 MB peak private / 2,554.5 MB
working set. Its bounded output is still exactly 414 bytes with SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`,
and a bounded ABI-ID mutation exits 1 with the owned ABI diagnostic.

Keep the memory and CPU verdicts separate. The historical whole-graph
revalidation defect crossed 3 GiB; the current v39 consumer stayed around
135/141 MB while doing too much repeated ABI work. The next fixed-window
falsifier is routine 704 and then `consumer:mir-to-ast:done`. Any reuse added
next must compare the exact raw/canonical ABI tuple; the 28-bit layout ID alone
cannot authorize a cache hit because collisions or a mutated second payload
must not bypass validation.

#### Exact ABI reuse must retain the complete validated tuple

The v39 census explained why the one-pass row capture still left material CPU
work. Before routine 640, 10,635 instructions carried only 40 complete ABI
tuples. The 580 required rows represented five tuples, so 575 successful nested
capture/hash operations repeated facts already validated during the same
MIR-to-AST execution. Across the complete input, 2,504 required rows represent
seven tuples. This is validation witness reuse, not a new ABI layout authority.

Checkpoint `0da9c5c2` gives `abi_layout_fact_owner.pgy` one program-lifetime
validation session. Required hits compare the raw type value, canonical decimal
ID, required state, and complete raw layout payload. The ID is only one part of
the key. Optional rows still prove their exact `id=0`/`layout=null` contract.
Only rows which passed the full order-independent capture and canonical hash are
remembered. Different JSON property order is a safe miss and full revalidation;
the same ID with a changed nested payload is also a miss and fails closed.
Store both the raw type key and decoded type name: returning the quoted raw key
as a decoded name makes a later safe miss fail incorrectly.

The v41 integrated driver built in 52,722 ms at 2,346.8 MB peak private /
2,336.6 MB working set, below the unchanged 3,072 MB cap. Its bounded output
remains byte-equal at 414 bytes with SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
A wrong-ID bounded input exits 1 with the existing ABI diagnostic and creates
no output.

The v41 full run reached routine 192 at 93,030 ms, routine 320 at 139,456 ms,
routine 512 at 200,634 ms, routine 640 at 228,455 ms, routine 704 at 238,884 ms,
and routine 896 at 288,574 ms. Compared with v39, routine 640 moved earlier by
69,919 ms (23.4%). The run timed out at 300,227 ms with 157.2 MB peak private /
162.3 MB working set, `limit_exceeded=false`, no `mir-to-ast:done`, and no gen2
file. Keep the next test window fixed; profile the first interval after routine
896 and use routine 960 as the next falsifier rather than raising time or memory
limits.

#### Carry common ABI wire facts instead of validating the same row twice

The v41 interval census showed that elapsed time tracked instruction count much
more closely than the remaining required-ABI count. The next duplicate work was
the common optional row: the routine scalar pass had already decoded
`abi_type_name`, but the ABI owner reopened the same instruction value and
decoded the optional type and ID again. This was a repeated read/validation
cost, not evidence that the consumer needed more memory.

Checkpoint `bf8a56b8` carries `abi_type_value_ready` alongside the already
captured type name. Readiness means the scalar scan observed either one valid
JSON string or the exact optional `null`; it does not authorize an ABI semantic
decision. `abi_layout_fact_owner.pgy` remains the sole owner of the type, ID,
required-state, and layout relationship. It accepts the common optional case
only when the carried type is ready and the raw tokens are exactly
`abi_layout_id:0` and `abi_layout:null`. Required rows still use the complete
raw tuple witness and full canonical validation. Missing, duplicate,
wrong-kind, noncanonical-ID, and changed-layout cases still fail with the owned
ABI diagnostic. No C/LLVM split and no second cache were added.

The exact-source v42 driver built in 53,265 ms at 2,515.0 MB peak private /
2,503.6 MB working set. Its bounded result remained 414 bytes with SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
the wrong-ABI run exited 1 in 551 ms and opened no output. The fixed-window full
run reached routine 192 at 83,846 ms, routine 704 at 162,849 ms, routine 896 at
192,157 ms, routine 1,600 at 241,729 ms, and routine 1,920 at 293,147 ms. It
timed out at 300,115 ms with only 214.4 MB peak private / 216.6 MB working set,
`limit_exceeded=false`, no `mir-to-ast:done`, and no gen2 file. The v42 run was
76,035 ms earlier at routine 704 and 96,417 ms earlier at routine 896 than v41,
and reached 1,024 additional routines in the same window.

This also explains the earlier multi-gigabyte symptom precisely. The historic
3 GiB-class failure repeatedly validated whole-program graph/readiness state
that admission needed to prove once. The current v42 runtime stays near
214/217 MB; its remaining failure is CPU completion within the diagnostic
window. Do not raise the memory cap, copy the graph, shard it by process, or
turn a carried readiness bit into semantic authority. Continue from routine
1,920, with routine 1,984 as the next fixed-window falsifier.

#### Key-count reduction is not automatically a dominant wall-time reduction

The v42 source contained eleven unconditional semantic key comparisons for
every key visited by `MirRoutineInstructionScalarCaptureWithin`. Static census
found 852,275 keys across 34,091 instructions: 9,375,025 comparison call sites
at runtime. Checkpoint `dfc8e406` scans each raw key for an escape, dispatches a
plain key to only its matching length group, and preserves the full semantic
fallback for escaped spelling. The focused C/LLVM fixture proves all eleven
target keys, a same-length non-target, an escaped target, and rejection of a
plain-plus-escaped duplicate. No fact owner or ABI contract moved.

The v43 integrated driver built in 52,451 ms at 2,523.0 MB peak private /
2,511.6 MB working set. Its bounded output remained 414 bytes with SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`,
and the wrong-ABI input exited 1 with the owned diagnostic and no output. The
full fixed-window run reached routine 704 at 162,255 ms, routine 896 at 190,875
ms, routine 1,600 at 239,277 ms, and routine 1,920 at 290,054 ms. It timed out
at 300,268 ms with 215.1 MB peak private / 217.1 MB working set and no cap
crossing. It did not reach routine 1,984, `mir-to-ast:done`, or gen2 output.

Against v42, the shared markers improved by 594 ms at routine 704, 1,282 ms at
routine 896, 2,452 ms at routine 1,600, and 3,093 ms (1.06%) at routine 1,920.
Record that as a real but minor result. A large static comparison-count
reduction did not make scalar key dispatch the dominant wall-time owner. Do not
stack more name-length micro-optimizations or raise the diagnostic window.
The next active seam is the existing CFG graph owner's repeated backedge query:
the remaining tail performs an estimated 9,144 entry/avoid-target BFS calls
because `BuildMirRoutineFactIndex` asks once per edge. Replace that with one
routine-level owner result, preserve structural-merge/phi behavior, and use the
same routine-1,984 fixed-window falsifier.

#### Fewer graph traversals still require fixed-window wall-time evidence

Checkpoint `73133678` replaces the edge-local backedge loop with
`MirRoutineBackedgeHeaders` in the existing CFG owner. Entry reachability is
computed once, each reachable distinct incoming target gets at most one
avoiding traversal, and target-major source checks classify the incoming edges.
The old `MirRoutineEdgeTargetsDominator` definition is deleted, not retained as
a second read path. `ec4b9eef` adds the consumer-level malformed successor
negative. Invalid lengths/targets produce an empty typed owner result and the
fact-index consumer reports `cfg_backedge`; it does not silently accept an
all-zero loop view. Structural merge and phi are unchanged.

The static tail model reduces backedge BFS calls from 9,144 to 4,128 (54.9%).
That is a work-count proof, not a wall-time result. The exact-source v44 driver
built in 52,316 ms at 2,433.5 MB peak private / 2,427.0 MB working set. Its
bounded output remained 414 bytes with the established SHA; the wrong-ABI
input exited 1 with no output. The full run reached routine 704 at 162,403 ms,
routine 896 at 191,236 ms, routine 1,600 at 240,535 ms, and routine 1,920 at
291,308 ms. It timed out at 300,682 ms with 202.7 MB peak private / 205.0 MB
working set, `limit_exceeded=false`, no routine 1,984, no `mir-to-ast:done`, and
no gen2 file.

At the shared routine-1,920 marker, v44 is 1,254 ms (0.43%) later than v43.
Treat the difference as a negative/noise result: the batch is an owner and
fallback closure, but this run does not prove it as the dominant CPU fix. Do
not continue by caching structural-merge or phi traversals merely because their
static counts are large, and do not rerun the same revision until a favorable
sample appears. Select the next exact duplicate owner/consumer, add a negative
gate, and keep the 300-second/3,072 MB falsifier unchanged.

#### Carry the unique branch row instead of searching each block again

The next full-input census found 20,022 blocks, 34,091 instructions, and 8,387
blocks with a branch terminator. Three mandatory routine-lowering consumers
were each reconstructing typed instruction views while searching every block
for the same unique branch. That repeated at least 77,112 view
reconstructions beyond the rows the consumers actually needed.

Checkpoint `4ee29ce2` extends the existing routine-local
`MirRoutineInstructionFactBundle` scalar pass with one branch global row per
block. `BlockCond`, `BlockHasLoopTransfer`, and `BlockMatchBindingLine` consume
that fact. A genuinely missing branch remains an explicit valid/not-found
result; duplicate branches, a row outside its block, scalar-span mismatch, or
a carried row whose program-owned kind is not `branch` fail closed. The old
routine-lowering branch searches are statically rejected. This does not add a
program-global scalar aggregate, second cache, JSON fallback, or backend split.

The exact-source v45 driver built in 52,025 ms at 2,534.1 MB peak private /
2,522.6 MB working set. Its bounded output remained 414 bytes with the
established SHA, and the wrong-ABI input exited 1 with the owned diagnostic and
no output. The fixed 300-second run reached routine 704 at 161,510 ms, routine
896 at 189,756 ms, routine 1,600 at 238,576 ms, routine 1,920 at 288,324 ms,
and routine 1,984 at 298,381 ms. It timed out at 300,345 ms with 204.8 MB peak
private / 206.9 MB working set and no cap crossing.

This is the first observation of routine 1,984 under the fixed window. At the
shared routine-1,920 marker, v45 is 2,984 ms (1.02%) earlier than v44 and 1,730
ms earlier than v43. The result is modest but positive; it is not bootstrap
completion. Routine 2,048, `consumer:mir-to-ast:done`, and gen2 output remain
absent. Keep the 300-second/3,072 MB gate unchanged; routine 2,048 is the next
fixed falsifier.

#### Fewer instruction views do not prove the phi prefix is wall-time dominant

The v45 tail census found that `MirRoutinePhiFactsReady` reconstructed every
instruction view in each block even though only leading phi rows participate in
phi semantics. The full artifact contains 34,091 instructions but 3,532 phi
rows; routines 1,984 through 2,048 contain 1,161 instructions and 104 phi rows.
That made a block-owned prefix a precise duplicate-read seam rather than a
reason to add another cache or program-global aggregate.

Checkpoint `99e76e76` makes the existing routine-local fact bundle record each
block's leading phi count. A phi after the first non-phi records an invalid
sentinel. The phi semantic owner iterates only the carried prefix and still
checks program-owned `kind=phi`, predecessor count, arity, result identity,
incoming values, and CFG-owned backedge evidence. Missing or invalid prefix
facts fail closed; the old whole-block instruction-count loop, JSON kind
recovery, and `new ? old` fallback are absent and statically rejected.

The exact-source v46 driver built in 52,507 ms at 2,556.9 MB peak private /
2,546.0 MB working set. Its bounded output remained 414 bytes with the
established SHA, and the wrong-ABI input exited 1 with the owned diagnostic and
no output. The fixed run reached routine 704 at 163,937 ms, routine 896 at
193,024 ms, routine 1,600 at 242,500 ms, and routine 1,920 at 293,716 ms. It
timed out at 300,163 ms with 202.1 MB peak private / 204.3 MB working set,
`limit_exceeded=false`, no routine 1,984 or 2,048, no
`consumer:mir-to-ast:done`, and no gen2 file.

At the shared routine-1,920 marker, v46 is 5,392 ms (1.87%) later than v45.
Treat this as CPU negative/noise: the phi owner/fallback closure is valid, but
the static 30,559-view reduction is not the dominant wall-time fix. Do not
rerun the same revision until a favorable sample appears, raise the window, or
expand the prefix into another global/local authority. Keep routine 2,048 as
the fixed falsifier and choose the next exact measured owner/consumer seam.

#### Admit a carried routine fact once, not once per block

The v46 regression came from the shape of the new read path, not from phi
semantics. `BuildMirRoutineFactIndex` had already built and admitted one
routine-local bundle, but every block called the prefix accessor. That accessor
repeated `MirProgramRoutineIndexRowReady` and
`MirRoutineInstructionFactBundleReady`, including at least 23 array-length
checks. Across 20,022 blocks and 2,345 routines this introduced 17,677
redundant admissions and at least 406,571 redundant shape checks. In routines
1,984 through 2,048 it repeated 618 admissions where 64 were sufficient.

Checkpoint `a05aaf06` moves row identity, exact block-count, and bundle-shape
admission to the entry of `MirRoutinePhiFactsReady`. The block loop reads the
carried prefix array directly and rejects negative or oversized values. The
one-use `MirRoutineInstructionFactBundlePhiPrefixCountAtBlock` definition and
all calls are deleted. A truncated prefix carrier fails the focused C/LLVM
gate. There is no per-block fallback, count-to-zero repair, extra cache,
program-global aggregate, or backend-specific path.

The exact-source v47 driver built in 51,436 ms at 2,535.7 MB peak private /
2,524.3 MB working set. Its bounded output remained 414 bytes with the
established SHA, and the wrong-ABI input exited 1 with the owned diagnostic and
no output. The fixed run reached routine 704 at 158,438 ms, routine 896 at
186,805 ms, routine 1,600 at 234,127 ms, routine 1,920 at 283,594 ms, and
routine 1,984 at 293,201 ms. It timed out at 300,384 ms with 207.7 MB peak
private / 209.7 MB working set and no cap crossing.

At routine 1,920, v47 is 10,122 ms (3.45%) earlier than v46 and 4,730 ms
(1.64%) earlier than v45. Routine 1,984 is 5,180 ms (1.74%) earlier than v45.
This both recovers the v46 regression and establishes measured executable CPU
progress. Routine 2,048, `consumer:mir-to-ast:done`, and gen2 output remain
absent, so keep routine 2,048 as the unchanged fixed falsifier.

#### Moving admission to the right owner can still be wall-time neutral

After v47, three branch consumers still called a bundle-owned accessor that
repeated program-row and full bundle admission despite receiving an admitted
`MirRoutineFactIndex`. The validation loop alone made 21,910 such calls across
the full artifact, a lower bound of 503,930 repeated shape checks. Routines
1,984 through 2,048 make at least 662 calls before region rendering adds more.

Checkpoint `8074d6c8` leaves the branch row in the routine-local bundle but
moves selection to `MirRoutineFactIndexBranchAtBlock`. The new boundary checks
index admission, routine/block identity, local and global instruction range,
carried span equality, and final program-owned `kind=branch`. The old bundle
accessor is deleted and the three consumers use only the index owner. Missing
is represented solely by the exact negative sentinel; other negative,
out-of-block, forged-kind, and inconsistent rows fail closed. There is no old
helper, block scan, JSON kind fallback, extra cache, or backend split.

The exact-source v48 driver built in 51,479 ms at 2,567.8 MB peak private /
2,557.0 MB working set. Its bounded output remained 414 bytes with the
established SHA, and the wrong-ABI input exited 1 with the owned diagnostic and
no output. The fixed run reached routine 704 at 158,817 ms, routine 896 at
187,672 ms, routine 1,600 at 235,166 ms, routine 1,920 at 285,333 ms, and
routine 1,984 at 295,075 ms. It timed out at 300,615 ms with 206.3 MB peak
private / 208.3 MB working set and no cap crossing.

At routines 1,920 and 1,984, v48 is 1,739 ms (0.61%) and 1,874 ms (0.64%) later
than v47. Treat this as an owner/fallback closure and CPU negative/noise result,
not a speedup. Do not rerun v48 for a favorable sample or keep shaving local
guards without a measured owner seam. Routine 2,048 remains the unchanged
falsifier; MIR-to-AST completion and gen2 output remain absent.

#### Revert a structurally valid optimization when generated-code cost wins

The next experiment replaced `EmitBlockStmts`' three checked accessors with a
single block-boundary admission and direct instruction/scalar construction.
Its static model removed about 1,202,928 repeated shape checks across the full
artifact, and a focused C/LLVM cross-block negative proved the new slice
boundary failed closed. Those facts established correctness of the proposed
owner path; they did not establish a cheaper generated program.

Checkpoint `80a54268` built in 60,860 ms at 2,587.7 MB peak private / 2,578.1
MB working set, compared with v48's 51,479 ms. Its bounded output remained 414
bytes with the established SHA, and wrong ABI exited 1 with no output. The
fixed full run reached routine 704 at 166,252 ms, routine 896 at 194,769 ms,
routine 1,600 at 243,264 ms, and routine 1,920 at 293,502 ms. It timed out at
300,269 ms with 202.3 MB peak private / 205.0 MB working set, no routine 1,984
or 2,048, no `consumer:mir-to-ast:done`, and no gen2 file.

The shared routine-1,920 marker was 8,169 ms (2.86%) later than v48, and v49
lost the routine-1,984 marker. This is a material regression, not noise. The
likely cost is the large generated block guard and direct aggregate
construction replacing smaller called accessors. `85cee4ff` therefore reverts
the experiment and restores the exact v48 source tree. Do not preserve a
performance regression merely because its static check count is lower, and do
not repeat the direct-construction shape under another name.

#### A one-pass capture can regress when it expands every instruction carrier

The next experiment estimated about 145.6 MB of repeated resource-runtime
top-field scanning across the complete MIR. `530682af` captured `name` and the
three runtime ABI value bounds during the existing scalar scan, carried them in
the routine bundle, and removed the later top-span reads. Focused C/LLVM,
component, bounded, and wrong-ABI gates were green. The bounded result even
completed in 609 ms and preserved the established 414-byte SHA.

That evidence did not predict full-artifact cost. The integrated driver build
took 62,385 ms at 2,445.2 MB peak private / 2,438.9 MB working set, versus
v48's 51,479 ms. In the fixed run the machine routine-index marker moved from
67,567 to 80,353 ms, routine 704 moved from 158,817 to 189,951 ms, routine 896
from 187,672 to 222,884 ms, and routine 1,600 from 235,166 to 279,085 ms. The
last marker was routine 1,728 at 296,959 ms; timeout was 300,680 ms with only
178.2 MB peak private / 182.3 MB working set and no gen2 output.

This is not the old 3 GiB graph/readiness defect. The extra scalar fields,
routine arrays, constructor traffic, and generated code changed costs outside
the removed resource reader; the pre-MIR marker regression proves the effect
is not attributable only to the late top-span scans. `c5ee6e62` reverts the
whole carrier shape. Keep its measurements as negative evidence and do not
reintroduce the same expanded per-instruction aggregate from a byte-scan model
alone.

Independent review found one separable correctness issue: an explicit
wrong-kind `runtime_call_abi` on a non-resource instruction was treated like an
absent row. `5e12cf43` changes only that early-return condition, adds a C/LLVM
stray-row negative, and leaves documented markerless native resource rows
compatible. Separate such fail-closed corrections from rejected performance
carriers so reverting an optimization does not reopen a real semantic hole.

#### A local one-pass scan can still lose to the established generated shape

The second and final resource experiment (`e6abdeaa`) kept meaning in
`MirResourceRuntimeRowFactReady` and replaced its four independent top-level
field lookups with one ephemeral object scan. It added no carrier, array,
cache, helper file, global aggregate, or backend branch. Expanded C/LLVM gates
covered markerless and explicit-`true` rows, escaped and duplicate semantic
keys, wrong-kind/`false` required markers, name edge cases, stray rows, and
auxiliary-table failures. Focused gates, the component ratchet, bounded output,
and the wrong-ABI negative all passed.

The v51 driver built in 56,417 ms at 2,576.8 MB peak private / 2,565.8 MB
working set. Its 1,408 ms bounded result remained 414 bytes with the established
SHA. The fixed full run reached routine 704 at 173,196 ms, routine 896 at
204,052 ms, routine 1,600 at 255,976 ms, routine 1,728 at 272,517 ms, and
routine 1,792 at 287,519 ms. It timed out at 300,614 ms with only 192.6 MB
peak private / 195.6 MB working set and lost v48's routine-1,984 marker.

This is generated-code/CPU regression, not memory exhaustion. `6879f0c0`
reverts v51. After both the v50 carrier and v51 local scan regressed, the
resource read seam is abandoned: do not try a third carrier, guard, or scan
shape. A lower static lookup count remains only a hypothesis until the fixed
wall-time markers improve. Keep the 300-second/3,072 MB gate fixed and move to
the separately quantified block-successor pair.

#### One block pass can cost more than two narrow generated readers

The v52 experiment (`8c49f74f`) targeted 20,022 blocks whose successor fields
are serialized after their instruction arrays. A single order-independent
capture theoretically removed 20,022 object scans and about 49.5 million
character visits. It preserved the `MirRoutineFactIndex` owner and rejected
duplicate, malformed, negative, and out-of-range rows at `cfg_successor` in
current-source C and LLVM. The static reduction and focused correctness gates
were real, but they did not predict generated-program cost.

The exact-source driver build took 67,265 ms at 2,591.5 MB peak private /
2,580.9 MB working set, versus v48's 51,479 ms. In the correctly observed
fixed run, machine routine-index completion moved from 67,567 to 83,531 ms,
routine 704 moved from 158,817 to 198,093 ms, routine 896 moved from 187,672
to 233,293 ms, and routine 1,600 moved from 235,166 to 291,565 ms. The last
marker was routine 1,664 at 298,472 ms; timeout was 300,560 ms at only 172.9 MB
peak private / 176.6 MB working set. No gen2 file was opened.

The new 97-line stateful capture and aggregate-return shape therefore cost
more in the generated compiler than the removed byte visits saved. The
pre-MIR marker regression directly attributes material cost to routine-index
admission rather than later lowering. `40037e52` reverts the experiment and
abandons this successor-pair seam. Do not repeat it with another pair struct,
array carrier, or generic two-field wrapper.

One earlier v52 pressure invocation omitted
`--observe-mir-consumer-stages`. Its 300,304 ms timeout and 172.3/176.1 MB
memory observation remain valid, but its empty stage stream is not valid
marker evidence. The separately labeled `v52-300s-observed` run above is the
only v52 marker comparison. Always pass the observation token when a fixed
MIR-consumer run is intended to compare routine progress.

#### LLVM strategy does not substitute for measuring the generated driver

The accepted source was built through `--backend=llvm` as v53 without changing
any semantic fact, MIR artifact, or bootstrap owner. The integrated LLVM driver
built successfully below the cap in 139,295 ms at 2,399.0 MB peak private /
2,389.0 MB working set. Its bounded result was byte-equal at 414 bytes with the
established SHA, and wrong ABI failed with the same diagnostic and no output.
This is positive connectivity and parity evidence for the LLVM projection.

It is not current self-host performance evidence. The observed full run reached
machine routine-index completion at 73,014 ms, routine 704 at 172,586 ms,
routine 896 at 202,127 ms, routine 1,600 at 250,313 ms, and routine 1,856 at
295,125 ms. It timed out at 300,518 ms with 214.0 MB peak private / 210.8 MB
working set, no routine 1,920/1,984/2,048, and no gen2 file. C v48 reached
routine 1,984 at 295,075 ms on the same artifact.

Troubleshooting rule: keep the project-level LLVM performance-primary strategy
separate from the performance of a particular generated compiler. Backend
connectivity, `-O3`, smaller build RSS, or bounded byte parity does not prove
the LLVM-built DRV-2 is faster. Compare the same complete artifact and fixed
markers. Do not change Pergyra semantics, owner facts, or the input artifact to
make a backend positioning claim pass.

#### A faster host compile does not imply a faster generated compiler

The unchanged C projection was compiled once with the available Windows clang
driver as v54. Build time improved from v48's 51,479 ms to 42,649 ms, with
2,557.6 MB peak private / 2,546.5 MB working set. Bounded output and wrong-ABI
failure remained identical. The full driver, however, reached routine 1,984 at
296,279 ms, 1,204 ms later than GCC v48, and timed out at 300,665 ms without
gen2. Peak private/working set was 206.0/208.0 MB.

Treat compiler build time and generated-program run time as separate metrics.
Do not change the Windows default compiler solely from v54: the repository's
GCC-first selection also owns a known MinGW thread/runtime compatibility
boundary, and clang did not improve the active full-run marker. An explicit
toolchain experiment must preserve runtime linking, byte parity, failure
behavior, and the same fixed artifact before it can influence default policy.

Disassembly of the accepted GCC-built driver then provided a narrower v55
hypothesis. `JsonSkipWhitespaceWithin` called `CharCode` once for the input byte
and again for each immutable whitespace literal comparison, while
`JsonIsDigitCode` converted both digit endpoints on every check. Commit
`2eeeec13` replaced only those literal conversions inside the shared JSON
scanner and added C/LLVM edge fixtures plus a negative source ratchet.

The generated machine code did improve locally: whitespace scanning retained
one checked input `CharCode` and compiled its four constants into a membership
test, while digit classification became a direct `48..57` range check with no
`CharCode` call. That local instruction-count win was not a whole-program win.
The v55 driver built in 51,536 ms at 2,516.9 MB peak private / 2,505.4 MB
working set, preserved the 414-byte bounded SHA and wrong-ABI failure, but its
observed full run reached routines 704/896/1,600/1,920 at
162,958/191,199/240,394/291,112 ms. Routine 1,920 was 5,779 ms later than v48;
the run timed out at 300,480 ms with 202.9/205.3 MB private/working set and no
routine 1,984 or gen2. `1f77b0bc` reverts the experiment.

Troubleshooting rule: fewer instructions in a plausible hot helper are not
evidence that the helper dominates the integrated compiler. Preserve the
disassembly and fixed-marker measurements, reject the change when the complete
artifact does not improve materially, and profile or instrument the admitted
MIR-to-AST loop before choosing another source rewrite from static call counts.

### Compare generated-driver changes against an adjacent control

The v56 match-local experiment (`6f5c373d`) demonstrated why historical
absolute markers are insufficient when host load changes. It filtered local
facts by instruction identity but also performed a separate per-instruction
span-alignment pass. The exact-source driver built in 69,158 ms at 2,587.0 MB
peak private / 2,576.3 MB working set. Bounded and wrong-ABI gates remained
exact, while the full run timed out at 300,772 ms with only 166.2/170.5 MB
private/working set and reached routine 1,408 at 296,916 ms.

That run initially appeared much slower than historical v48, but an adjacent
unchanged v48 control also entered MIR-to-AST late: 83,190 ms instead of the
historical 67,580 ms. That control reached routines 256/704/896/1,600/1,664 at
108,489/198,926/233,149/290,131/296,995 ms and timed out at 300,625 ms with
174.2/177.9 MB private/working set. It was an adjacent current-session control,
not a simultaneous run. Compare the code under test and control relative to
their own `consumer:mir-to-ast:start` marker. On that basis v56 was still
slower by 2,420 ms at routine 256, 2,929 ms at routine 704, and 5,767 ms at
routine 896. `c9e8011a` therefore reverts v56. The conclusion is specifically
a generated-code CPU regression after load normalization, not a 3 GiB/20 GiB
memory failure and not the whole raw wall-time gap attributed to the patch.

The final v57 shape (`ab3f9066`) removed that redundant pass and consumed the
existing `MirProgramRoutineIndex` row directly. Focused C/LLVM, component,
bounded, and wrong-ABI gates passed. The v57 driver built in 56,640 ms at
2,588.3/2,577.6 MB peak private/working set. Its observed run entered MIR-to-
AST at 74,173 ms, reached routines 256/704/896/1,600/1,664/1,728/1,792/1,856
at 97,495/172,807/202,276/251,736/258,128/267,628/281,858/296,651 ms, and
timed out at 300,609 ms with 197.5/200.4 MB private/working set.

Against the adjacent v48 control, relative MIR-to-AST elapsed time improved by
1,977 ms at routine 256, 17,102 ms at routine 704, 21,856 ms at routine 896,
29,378 ms at routine 1,600, and 29,850 ms at routine 1,664. Accept v57 as a CPU
improvement. It still emitted no gen2 file, so faster progress is not bootstrap
completion. Keep the 300-second and 3,072 MB bounds fixed, retain both raw and
start-normalized marker evidence, and do not add a third match-local shape.

### Repeated graph/fact validation can look like a memory defect

The original 20+ GiB observation was not a normal compiler working-set
requirement. The active oracle path repeatedly rebuilt or revalidated graph and
readiness facts that were already owned. Those repeated passes multiplied
temporary allocation and CPU work; a single validation at the owner boundary
was sufficient. Keep the pressure runner's process-tree accounting,
`-StopOnLimit`, 3,072 MB cap, and stage markers enabled so a detached worker or
overlapping run cannot be mistaken for one compiler process.

The current measurements put the distinction on executable evidence. The v58
integrated C driver build completed at 2,587.9 MB peak private / 2,577.0 MB
working set, and the focused LLVM `mir_lower` build completed at 315.5/318.3
MB. The 300-second generated-driver run used only 197.3/200.0 MB. Therefore a
fresh 20 GiB observation is a regression, overlapping process, or measurement
scope problem until proven otherwise; it is not an accepted cost of the
oracle or self-host lane.

v58 (`195d9b64`) closes one concrete repetition. Loop-summary projection used
to call branch selection twice for every block and scalar capture twice for
every branch-bearing block. On the fixed artifact that meant 40,044 branch
selections and 16,774 scalar reads. It now consumes the owned branch row once,
performs one branch/scalar validation only on 8,387 branch blocks, and removes
31,657 selections plus 8,387 scalar reads. Do not replace this with another
cache or graph: the existing routine bundle is the owner.

Compare against an adjacent accepted executable because host load still moves
the absolute markers. The v57 control entered MIR-to-AST at 80,208 ms; v58
entered at 75,535 ms. After normalizing each run to that start, v58 improved
routines 256/704/896/1,600/1,664/1,728 by
2,442/13,115/17,413/23,866/24,729/25,971 ms and reached routine 1,856 at
297,340 ms, while the control ended at routine 1,728. This is material CPU
progress with stable memory, but no `driver_gen2.c` was emitted. Keep the
fixed limits and continue the same artifact; a higher timeout or memory cap is
not a substitute for closing the next owned repeated validation.

#### Rebuilding cumulative expression arenas explains the 20 GiB observation

The first full v58 completion run reached `consumer:mir-to-ast:done` at
387,029 ms, entered `consumer:expression-graph:start`, and then crossed the
3,072 MB cap at 1,059,616 ms without opening a gen2 output. Static accounting
identified the exact amplifier. The frozen artifact contains 34,962 persisted
graphs and 214,151 graph nodes. Every append rebuilt a fresh cumulative
`place_kinds` array and revalidated the complete cumulative arena. The first
persisted pass alone retained an estimated 18.895 GiB of array backing; the
second `AppendView`/parser-bridge assembly raised the two-pass lower bound to
38.99 GiB and implied at least 14.47 billion cumulative readiness node visits.

v59 (`19ecce41`, prefix-proof follow-up `7eef684b`) reuses the existing place
array, appends one `Unknown` row per new node, validates raw graph-local shape
and reachability, and performs full arena readiness only at sequence/final
owner boundaries. It also consumes the admitted `MirProgramRoutineIndex`
instruction bounds instead of rebuilding the program and per-routine indexes.
The sequence now carries an O(1) `ready_node_count` proof, and the index-taking
boundary rechecks the complete routine-index structure. Static gates reject
restoring per-append `ArenaUnclassified`, whole-arena readiness, or index
reconstruction.

The exact-source driver built in 66,274 ms at 2,590.1/2,579.1 MB peak
private/working set. Its bounded result remained 414 bytes with SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
wrong ABI failed with the owned diagnostic and no output. On the same complete
MIR, v59 passed v58's 1,059-second/3,072 MB failure point at 547 MB private and
finally failed closed at 1,645,538 ms with only 801.8/749.4 MB peak
private/working set. This is executable proof that repeated cumulative graph
materialization—not normal oracle size—caused the 3 GiB/20 GiB symptom.

Do not mistake the later v59 failure for a memory regression. All 34,962 raw
graphs and 214,151 nodes validate. The reconstructed structured AST has 35,638
persisted-required lanes, 676 more than the raw document-order roots because
20 routines revisit CFG blocks. The first mismatch is routine 289
`ParsePrimaryFact`, root ordinal 2,875: the structured surface expects
`tuple_probe`, while positional raw order supplies `tuple_ch == "\\\""`.
The fix is stable `(routine row, global instruction row, lane, derived ordinal)`
identity carried by structured emission into one final graph. Do not reorder by
text, relax the count check, deduplicate repeated CFG visits, rebuild a second
graph, or raise the memory/time limits.

#### v60 closes positional graph identity without reopening the memory defect

v60 makes structured emission carry the exact occurrence key
`(global instruction row, AST lane, derived ordinal)`. The occurrence array is
the order authority: revisiting a CFG block repeats the same producer key and
creates another range in the one final graph arena. It is not deduplicated.
`MirExpressionGraphFactsForArtifact` consumes that order, checks source text
only as an assertion, and records producer coverage so a missing required MIR
graph cannot hide behind an omitted structured occurrence. The intermediate
persisted sequence/view owner is deleted and statically forbidden.

The exact-source C driver built in 69,368 ms at 2,480.3 MB peak private /
2,473.7 MB working set. The observed bootstrap driver built in 65,293 ms at
2,575.8/2,564.5 MB. The bounded MIR still emits 414 LF-normalized bytes with
SHA-256 `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
wrong ABI still exits 1 with the owned diagnostic and no output. Option match,
array destructure, and collection mutation graph fixtures pass direct hard
consumption, and missing or invalid graphs fail closed.
The same gate exposed and closed a native range-loop producer drift: the
range branch now serializes its MIR-owned stop expression (`expr1`) as the
branch graph, while loop-init retains the start expression. A focused
`forloop` native/self MIR-to-C parity run proves stop `3` and rejects a
regression to start `0`; the consumer does not repair this distinction.

The complete fixed-artifact run no longer fails on the v59 `ParsePrimaryFact`
positional mismatch. It completed expression-graph construction at
1,673,958 ms and semantic analysis at 1,674,754 ms, then reached
`semantic-body-type-stage assignment:start`. The 30-minute integration budget
expired at 1,800,768 ms during that assignment stage, with 1,130.3 MB peak
private / 1,041.1 MB working set and no output file. This is a time-budget
failure at the next named consumer, not a graph error and not a memory-limit
failure.

Treat this sequence as the durable diagnosis:

1. v58 proved repeated cumulative arena copies and whole-graph readiness could
   cross 3,072 MB and imply tens of GiB of logical allocation;
2. v59 made construction linear and exposed that raw document position is not
   structured execution identity;
3. v60 uses stable structured occurrence identity and one final arena, so the
   full run stays near 1.1 GiB and advances into assignment body typing.

Never validate the whole prefix after each append, never use the next raw graph
position as identity, and never repair a mismatch by text lookup. Validate a
new graph locally, validate the complete arena once at the owner boundary, and
keep the negative producer-coverage gate. The next pressure investigation must
start at assignment body-type admission under the same 1,800-second / 3,072 MB
budget rather than raising either limit.

#### v73 bounded bootstrap stays near 1 GiB; select the current seed explicitly

The v73 phi-free range change rebuilt the integrated driver with the verified
`bootstrap_v64_formal` parser/codegen seed. During the long gen2 C emission,
two live samples were 886.2/791.0 MB and 988.4/887.8 MB private/working set.
They are in-flight observations rather than a peak measurement, but they show
no 20 GiB-class recurrence. The generated seed then matched the native oracle
for sample C, MIR production, and bounded MIR consumption and passed the full
direct CFG chain through `forloop`.

An earlier invocation used the old default `.tmp/self_hosted/codegen/bootstrap`
cache from 2026-07-24. It failed closed before C compilation because that seed
did not recognize the already-owned `ArrayPushOwnedString` builtin. This is a
seed-version mismatch, not range-CFG or memory evidence. For resumed bootstrap
work, resolve and record the exact codegen seed directory; existence of
`gen2.exe` is insufficient when the compiler-world builtin surface has moved.

#### v61 admits expression-graph readiness once for assignment typing

The v60 timeout was a CPU defect in assignment admission, not residual graph
construction cost. The frozen artifact contains 4,382 raw assignment rows and
214,151 expression-graph nodes. `SemanticAstAssignmentTypeFactsFromArtifact`
called the checked match-binding seed wrapper once per assignment; that wrapper
reproved complete expression-surface and graph readiness. Even the two minimum
whole-arena passes therefore implied at least
`4,382 * 214,151 * 2 = 1,876,819,364` node-validation iterations.

v61 proves `SemanticAstExpressionSurfaceBorrowReady` once at the assignment
owner boundary and calls only
`SemanticAstExpressionSeedVisibleMatchBindingsFromReadyArtifact` inside the
row loop. It adds no graph, cache, text recovery, or backend-specific path.
Static lifetime and component gates require exactly one readiness admission and
reject restoration of the checked wrapper in the hot function. Focused C/LLVM
assignment projection, negative cases, component contracts, and the full DRV-2
parser pass.

The observed v61 driver built in 66,670 ms at 2,630.3/2,620.0 MB peak
private/working set, below the unchanged 3,072 MB cap. On the same
51,807,108-byte MIR, expression graph construction completed at 1,513,956 ms.
Assignment typing then completed in 796 ms, followed by statement typing in
2,330 ms, generic typing in 6,591 ms, verdict construction in 124 ms, body-type
readiness, and verification. The run failed closed at 1,671,316 ms after
`consumer:assignment-binding:start` with
`MIR assignment binding-mode fact is missing or invalid`. Peak private/working
set was 1,319.9/1,216.3 MB; no output file was opened.

The first v61 integration attempt is invalid memory evidence. The pressure
probe attributed every compiler-named process created after probe start, so an
unrelated concurrent `pgy-self-driver.exe` was counted and stopped when the
aggregate crossed 3,072 MB. `-RootProcessTreeOnly` now limits direct-executable
integration probes to the launched PID tree and records
`detached_compiler_worker_tracking=false`; default MSYS detached-worker
tracking remains available for isolated native build probes. Never report or
terminate another Codex task's compiler as if it were owned by the current
probe.

The next owner seam is structured assignment occurrence identity. The old
binding-mode checker rebuilds the program/routine indexes and walks unique raw
MIR instructions, while semantic assignment facts follow the structured AST
and preserve CFG revisits. Consume the already admitted
`MirStructuredExpressionEmissionOrder` global-row/lane identity and fail closed
on a missing pair or mode. Do not add a second assignment sequence, fall back
to raw order, or recover from AST text.

#### v62 consumes assignment modes in structured occurrence order

The v61 binding checker spent 144,314 ms after
`consumer:assignment-binding:start`, rebuilt the already admitted 51.8 MB
program index, rebuilt all 2,345 routine fact indexes, and reparsed all 34,091
instruction objects. It also compared 4,382 unique raw assignment rows against
semantic facts whose identity follows structured CFG emission, so it could not
represent repeated occurrences.

v62 keeps the existing semantic and MIR owners but changes the receiving seam.
`MirAssignmentBindingModesMatchSemanticFacts` receives the prebuilt
`MirProgramRoutineIndex` and `MirStructuredExpressionEmissionOrder`. A single
cursor consumes each `AST_ASSIGNMENT` atom/value ordinal-zero pair with the
same global instruction row, reads `arg1` from that exact instruction span,
and compares it with the next semantic binding mode. Repeated global rows
increment the semantic cursor again; no row is deduplicated. Bad rows, pair
shape, ordinals, instruction kinds, missing modes, and final count mismatch all
fail closed. Static gates reject index rebuilding, raw instruction loops,
text recovery, and seen-row caches. A focused B,A,A synthetic order accepts all
three occurrences and rejects a mode drift on the repeated A row, missing
pairs, invalid rows, and invalid kinds.

The v62 observed driver built in 57,282 ms at 2,515.1/2,503.6 MB peak
private/working set. The same root-owned full run completed graph construction
at 1,392,910 ms, assignment typing at 1,396,994 ms, body readiness and
verification at 1,405,138 ms, then completed assignment-binding validation at
1,420,016 ms. The binding slice took 14,878 ms, about 9.7 times less than v61.
Generic-specialization and codegen-view admission completed in 214/107 ms.
C emission then failed closed with `ToString argument type fact is missing`;
the process exited 1 at 1,478,323 ms with 1,432.9/1,322.5 MB peak
private/working set and no output. The next investigation starts at the
ToString argument-type producer/consumer boundary, not at assignment or graph
construction.

#### v63 preserves nested interpolation calls and reaches the full fixed point

The first v62 C-emission failure was not a codegen type-inference gap. The
full-source MIR contained 260 actual `ToString` calls. Their source types were
244 `Int` plus 16 `String`; codegen resolved all 244 `Int` and only 12
`String`, leaving exactly four calls in `SelfHostDiagnostic_Fact2`/`Fact3`.
Those four normal `${...}` interpolation bodies had been flattened by the
parser owner into text leaves such as `Fact1(k1, v1)`, so the consumer had no
call graph from which to obtain the result type. Adding a codegen type guess,
reparsing the leaf text, or accepting an unknown type would create a second
authority and is forbidden.

v63 fixes the producer. Normal interpolation now parses its body with
`ParseExprFact`, requires complete cursor consumption, and connects the
resulting expression graph as the `ToString` argument. Malformed or unmatched
interpolation retains the established native string-literal fallback. The
readiness contract proves that `${Fact1(k1, v1)}` reaches `ToString` as a
call-argument graph whose direct callee is `Fact1`; static gates forbid the old
`ParserExpressionLeaf` construction.

The current C-oracle-built full-source producer emitted a 54,205,046-byte MIR artifact with
SHA-256
`3d6aa33595592f8af2c78a68c6d5fc9e5a242c15e55b9e5a8deb4fe60209083b`.
Producing it took 767,407 ms at 844.3/762.8 MB peak private/working set. The
seed consumed it in 1,774,216 ms at 1,714.8/1,590.9 MB and emitted complete
3,378,704-byte gen2 C. Host GCC compiled gen2 in 4,721 ms. Gen2 consumed the
same MIR in 800,248 ms at 2,033.2/1,867.9 MB and emitted byte-identical gen3 C;
both have SHA-256
`6aaf915d67fb129fce6a85bece93d9c814c66dadf94578c8ee160e7b9e1f7087`.
Gen3 compiled in 4,942 ms, and both generated drivers reproduced the
established 414-byte bounded artifact.

This result also confirms the earlier memory diagnosis. The complete producer,
consumer, and gen2 fixed-point legs all remained below 3,072 MB; the old
20+ GiB/3 GiB symptom came from cumulative graph copying and repeated
whole-arena readiness, not from the compiler's necessary live state. Keep the
cap and root-process-tree measurement. The next rung is full-source Pergyra MIR
production, followed by released/default selection, not another graph,
assignment, or interpolation optimization.

#### v64 moves complete-source MIR production to Pergyra gen2

The direct falsifier used the v63 Pergyra-built gen2 executable, not the
C-oracle-built driver. It ran `--emit-mir-json-verified` on the current complete
compiler source in 1,210,574 ms at 1,091.0/963.4 MB peak private/working set.
The resulting 54,205,046-byte artifact has SHA-256
`3d6aa33595592f8af2c78a68c6d5fc9e5a242c15e55b9e5a8deb4fe60209083b`
and is byte-identical to the C-oracle artifact. The output remained unopened
until the verified JSON-write boundary.

The formal full gate must therefore start from the Pergyra producer. Generate a
separate native artifact once as oracle evidence, compare it through the owned
artifact comparator, and let gen2/gen3 consume only the Pergyra-produced MIR.
Do not regenerate MIR between generations or relabel the native artifact as the
fixed-point input. The next memory investigation, if needed, starts from this
1,091 MB baseline; a return to multi-GiB growth is a regression.

The fresh composed runner confirms that wiring. Refreshing isolated
codegen/parser seeds took 412,649 ms at 1,107.9/1,123.6 MB peak
private/working set. The full driver sequence took 3,770,822 ms at
2,658.0/2,667.1 MB and exited 0: Pergyra/C-oracle MIR parity, gen2 C emission
and compile, bounded gen2 preflight, and 3,378,704-byte / 59,482-line
gen2/gen3 C equality all passed. On a host without `mingw32-make`, invoke the
same runner body directly under `measure_build_pressure.ps1`; record that the
wrapper was unavailable instead of claiming its target ran.

#### v66 direct dual-backend widening remains below the fixed cap

The bounded integrated driver was rebuilt after adding typed instruction-use
admission and the direct `let_log` C/LLVM projection. During full-driver C
generation, an observed process sample was 2,108.9 MB private / 2,096.3 MB
working set. This was an in-flight sample, not a claimed peak, but it remained
below the unchanged 3,072 MB fail-closed cap and the bounded bootstrap exited
successfully.

The follow-up gate produced the 2,341-byte `let_log` MIR once, kept SHA-256
`0ad63b8802e964f238807aabf3f2c73e59a1f795dc7fa73e078a59aff998ecee`
unchanged across C and LLVM projection, and completed in about 17.4 seconds.
The old 20+ GiB diagnosis therefore remains unchanged: do not answer later
growth by raising the cap. First check for cumulative graph copying, repeated
whole-arena readiness, or a consumer reopening raw instruction arrays instead
of extending the typed routine-local view.

#### v67 scalar widening removes two more repeat-work seams

The current-source focused gate produced `multilet.pgy` once as 4,135 bytes,
SHA-256
`31fb7b7300674c1483a5c54370d90a66c1ab1d4cddc3998d2eafbc03931f4efd`,
then compiled and ran the unchanged artifact through direct C and LLVM with
native-equal output `35` / `12`. Hello and `let_log` stayed green. The final r3
Pergyra-built bounded bootstrap and the same direct positive/negative gate
passed. An in-flight r3 seed-emission sample was 764.8 MB private / 673.3 MB
working set; it is useful evidence below the 3,072 MB cap, not a claimed peak.

Two repeat-work ratchets matter for later memory diagnosis. Machine admission
now carries its `MirDocumentFactIndex` into the admitted input, and direct
admission consumes that carrier instead of calling `BuildMirDocumentFactIndex`
again. `MirExpressionGraphSequenceAppend` validates exact graph/node schemas
and derives the node count during the same node walk instead of first scanning
the array only to count it. If direct-backend memory grows again, first verify
these carrier and one-pass contracts remain intact; do not add a cache, raise
3,072 MB, or create one document/graph reader per backend.

The next active falsifier is `ifelse.pgy`, not another scalar fixture. Its
3,413-byte MIR has SHA-256
`09586fd65f95c178c17e2d77d355015eb93364f8b151881d222a4cc6e960e858`,
is a four-block diamond without phi, and prints `pos`. Current direct C and LLVM
both reject it without opening output. Diagnose that boundary by carrying MIR
CFG facts into a MIR-bound AIR certificate and one verified plan; a backend-
local CFG read would recreate the duplicated graph problem.

#### v68 certificates must not revalidate the admitted graph

The first v68 certificate draft recomputed the full MIR digest and normalized
CFG digest in both issuance and `CertificateReady`. That is logically safe but
reintroduces the same class of repeat work behind the historical 3 GiB/20+ GiB
symptom: a proof boundary should not repeatedly traverse its input merely to
prove that its already-issued proof object is unchanged.

The accepted shape validates typed MIR/CFG facts once while issuing the AIR
certificate. It stores MIR and CFG digests plus a separate certificate
self-digest. The verified plan copies those identities and adds its own
self-digest. After issuance, plan construction and both emitters inspect only
the fixed-size certificate/plan; they do not hash MIR again, recompute
structural merges, revisit expression graphs, or read a serialized AIR
artifact. Evidence/fallback/drift and digest/target mutations are recomputed
over the small proof objects and must reject before output.

The fresh bounded Pergyra-built bootstrap passed. An in-flight `gen2` sample
was 882.5 MB private / 782 MB working set; this is not a peak measurement, but
it is evidence against a return to the cumulative graph-revalidation defect.
If memory rises again, count MIR/CFG/graph owner traversals before adding a
cache or raising the unchanged 3,072 MB cap. The valid count for this direct
path is one admission traversal followed only by fixed-size identity checks.

#### v69 phi admission preserves the same one-pass boundary

The four-block `if_else_assign.pgy` rung produces a 4,916-byte MIR artifact,
SHA-256
`da44b115d51ee8b83b6b2cc2d7443dfd22f6877368e86e7b3487646c0a4af393`,
with one merge phi and native output `2`. Phi admission now consumes the typed
instruction-use facts already built for the routine. It does not reopen the
raw `uses` arrays or validate the complete routine graph for each incoming
edge. Each incoming SSA result is mapped to its unique definition block, so
predecessor coverage is independent of the serialized `uses` order.

The normalized CFG shape is issued from the one AIR certificate, and the
target-neutral plan copies only the certificate/MIR/CFG/phi identities plus
the closed shape fact. It does not retain the full certificate or call
certificate readiness again. Plan readiness recomputes the small phi binding
digest from the normalized local/result/true/false SSA fields; changing either
the digest or those fields and then repairing the plan self-digest still
rejects. Each invocation selects exactly one C or LLVM emitter, so an LLVM
request no longer constructs and discards a C payload first. After plan
issuance neither path performs a second MIR, CFG, expression-graph, phi, or
certificate traversal.

The final bounded Pergyra-built r2 bootstrap exited 0 with seed/oracle MIR and
consumer parity. Its heavy `gen2` seed-emission step was then repeated under
detached-worker-aware pressure measurement: 355,226 ms, 1,022.1 MB peak
private / 937.2 MB peak working set, with `gen2.exe` owning 1,005.8 MB private.
The 3,366,105-byte output was byte-identical to the bounded bootstrap seed
(SHA-256
`ef8f0be361e9df7e0835c32c30fb4a38d8c33aeaccbe0776912fe309ec06637`),
and the unchanged 3,072 MB fail-closed cap was not exceeded.

Do not use the same runner's `RootProcessTreeOnly` summary as gen2 memory
evidence under Git Bash. The native worker can be reparented after an MSYS
pipeline parent exits; the root-only run then reported only 27.7/9.8 MB for
bash while the real gen2 process was near 1 GB. Use the default detached
compiler-worker attribution for an isolated probe, or label a manual sample as
non-peak. The old 3 GiB/20+ GiB symptom was caused by cumulative graph copying
and repeated whole-graph validation; do not classify a later high-water mark
as the same defect until owner traversal counts show that those ratchets
regressed.

One v69 LLVM failure was a separate owned-`String` correctness defect. The
emitter reused a local `format_name` after the first `Concat` had consumed its
storage, so a later interpolation emitted an empty global reference (`ptr @`).
`DirectMirCfgLlvmFormatName` now returns a fresh owned string for every use.
If an LLVM projection contains an empty symbol while C projection is valid,
audit consumed string reuse first; raising the memory cap or revalidating the
graph cannot repair it.

### Owned semantic scratch: heap corruption versus retained memory

The first owned-String cleanup attempt exposed a separate correctness failure,
not merely a high-water mark. The initializer projection probe's C-built
`--member-call-positive` mode exited on Windows with `0xC0000374` (heap
corruption). Instrumenting only the generated scratch C showed
`SemanticAstExpressionEnvironmentClear` trying to free the owner-field name
`value`. That row had entered the owned scratch array through ordinary
`ArrayPush`, so the cleanup contract was freeing a borrowed string.

Checkpoint `ca35a157` closes the reachable mixed-ownership path as one Pergyra
contract:

- every expression-environment producer, including owner fields, match
  bindings, and visible iteration rows, uses `ArrayPushOwnedString`;
- assignment, call-target, place, generic-specialization, initializer,
  iteration, and statement consumers release the scratch rows at their last
  success-path use;
- facts that survive cleanup copy the selected string before release, rather
  than retaining an address into the scratch owner;
- `semantic_expression_environment_owned_lifetime_smoke.sh` rejects an
  ordinary environment push, a missing named-consumer cleanup, or a missing
  result copy. The initializer/member-call C/LLVM probe remains the executable
  corruption and parity gate.

When this failure reappears, distinguish it from pressure before changing the
memory ceiling: capture the exact process exit code, run the smallest affected
probe, identify the first freed value and its producer, then verify both the
producer's push operation and the last consumer. Do not fix it by disabling
the drop, freeing in C/LLVM emitters separately, or copying the whole program
graph. The full-driver pressure gate must still measure the committed revision;
a crash-free focused probe alone does not prove the 3 GiB defect is closed.

### `for_each_call`: MIR expression graph attachment failed

This focused error had a different cause from the full-driver memory ceiling.
For a non-identifier foreach such as `for value in MakeValues()`, the
self-hosted producer correctly attached the program-owned call graph to the
collection-hoist definition, but then built a separate one-node graph for the
compiler-generated `__pgy_forin_N` local. The MIR instruction graph owner
rejects mixing graph identities, so the producer stopped at `MIR expression
graph attachment failed` before backend emission. Raising memory limits or
loosening graph equality cannot repair this failure.

The closed path is now:

1. HIR's `program_graph_owner.pgy` appends the compiler-generated leaf to the
   existing revision-local topology through its owned extension API.
2. The semantic iteration graph-root owner attaches `none` call-target and
   binding-place overlays and records the synthetic name/root handle.
3. MIR consumes that carried handle for loop-init, branch, local inventory,
   and instruction graph attachment. It does not traverse the AST to recreate
   an ordinal and does not construct a sibling graph.

Run `tests/self_hosted/parity/driver_rung2_iteration_graph_use_owner.sh` for
the negative ownership ratchet, then run the DRV-2 body parity gate with
`PGY_SELFHOST_DRIVER_MIR_FIXTURE_FILTER=for_each_call` for C and LLVM. The
structural check must continue to report `phase=unified structural_owners=1`.
This fix removes a correctness blocker; it does not change the separate
full-driver 3 GiB pressure verdict above.

### Source Control shows more than one `PergyraLang` root

Two different graphs can look similar in the editor. The compiler program
graph is a revision-local semantic fact and is permanently ratcheted by
`tests/self_host_program_graph_unification_smoke.sh`. A Git linked worktree is
only another physical checkout with its own branch and running processes; it
is not a compiler fact owner, and the program-graph gate deliberately does not
inspect local worktrees.

Consolidate a completed linked worktree without losing its history:

1. inspect `git worktree list --porcelain`, both worktree statuses, and active
   compiler/build processes;
2. merge the branch into `main` and run the combined owner, component, C/LLVM,
   and program-graph gates;
3. require `git merge-base --is-ancestor <branch> main` to succeed;
4. remove the linked worktree only when it is clean and no process owns a path
   below it, then delete the fully merged local/remote branch if it is no longer
   a collaboration boundary;
5. confirm `git worktree list` names one intended checkout and
   `git status --short --branch` still lists every unrelated user edit.

Do not delete a directory to make the editor look unified, squash away
unmerged evidence, or add a repository test that forbids all developer
worktrees. The durable source gate owns the compiler graph; ancestry, clean
status, and process checks own the local Git consolidation.

The follow-up check found that exact bypass active beside a 95-fixture DRV-2
shard. Together with a short-lived third recursive make probe, the three runs
owned 21 project processes and 2,114 MB private memory at an early snapshot;
they were stopped before the full-input oracle could grow further. This also
exposed a GNU make diagnostic trap: `make -n` still executes a recipe line that
contains `$(MAKE)`, because make treats it as a recursive invocation. Do not
dry-run the full-fixpoint pressure target; use
`tests/build_pressure_contract_smoke.sh` to verify its wiring.

So a stalled desktop is not automatically evidence of a compiler heap leak. If
single `compiler` builds stay below a few hundred MB but broad smokes stall the
machine, treat the first suspect as scratch/file-count pressure from self-host
and backend parity artifacts. Run `PGY_BUILD_RESOURCE_DEEP=1 mingw32-make
build-resource-report`; if `.tmp/self_hosted` or backend campaign scratch owns
the file count, use `mingw32-make clean-scratch` before broad local CI.

For a self-host owner edit, do not rerun the 204-source completeness ledger
until the focused slice is stable. Use the source filter with the relevant
stage first:

```sh
PGY_SELFHOST_COMPLETENESS_STAGES=codegen \
PGY_SELFHOST_COMPLETENESS_SOURCES=src/self_hosted/codegen/input/ast_type_usage_owner.pgy \
mingw32-make self-host-completeness-smoke
```

The source filter is local validation only. It requires every selected source
to pass every selected stage, but it does not prove source-count minima,
pipeline identity, or the full 204-source replacement ledger.

The codegen parity matrix is also an integration gate, not a narrow edit loop.
Its 69 fixtures each run through oracle, C-built self-host, and LLVM-built
self-host legs. The runner uses bounded fixture parallelism with two workers by
default; set `PGY_SELFHOST_CODEGEN_JOBS=1` for pressure diagnosis or at most 4
on a measured CI worker. Unbounded parallelism is forbidden because it trades
wall time for desktop stalls and hides per-process memory regressions. During a
local slice, select only the fixtures that exercise the owner:

```sh
PGY_SELFHOST_CODEGEN_FIXTURES=hello,func_recursive \
PGY_SELFHOST_CODEGEN_JOBS=2 \
mingw32-make self-host-codegen-parity-test-smoke
```

The complete 69-fixture matrix remains the integration proof. It should not be
silently substituted for a compiler build, and it should not be repeated by
multiple aggregate targets in one CI job.

The 280-row DRV-2 body matrix has the same isolation rule. Run its unfiltered
full matrix from MSYS2 bash on Windows. A Git Bash wrapper can exit while its
long-running worker remains reparented as a native Windows process; starting a
replacement then overlaps two full artifact-producing runs. The DRV-2 runner
therefore rejects an unfiltered Git Bash invocation. Git Bash remains available
for a focused development gate when
`PGY_SELFHOST_DRIVER_MIR_FIXTURE_FILTER` names the exact fixtures.

Windows evidence on 2026-07-12: the serial full matrix took about 31 minutes;
the same 69-fixture C/LLVM matrix with the default two workers completed in
1,342,043 ms (22 minutes 22 seconds), with both backends at 69/69. This is a
bounded wall-time improvement, not a fast edit loop. Parser/tool compilation
and native process orchestration remain serial. Use
`self-host-preparation-contract-test-smoke` for owner-shape edits and reserve
`self-host-preparation-parity-test-smoke` for integration or scheduled CI.

---

## 1. "Nothing to be done for 'bin/pgy.exe'"

### 증상
소스를 수정했는데 `mingw32-make bin/pgy.exe`가 즉시 끝나면서 위 메시지를 출력하고, 실제로는 변경이 반영되지 않은 바이너리를 그대로 사용한다.

### 원인
- `.d` (dependency) 파일이 stale해서 make가 재빌드 필요성을 인식하지 못함
- `.inc` 파일을 수정했는데 의존하는 `.c`가 그 사실을 모름 (MMD가 .inc 까지 추적하지 못하는 경우)
- 파일 시스템 mtime이 빌드 시점과 어긋남 (네트워크 드라이브/MSYS2 vs Windows native 혼용)

### 대응
```sh
mingw32-make rebuild           # clean + all 한 번에
```
또는 명시적으로:
```sh
mingw32-make clean
mingw32-make all
```

특정 파일만 강제 재빌드하고 싶으면:
```sh
rm build/semantic/type_checker.o
mingw32-make bin/pgy.exe
```

---

## 2. PowerShell vs bash vs cmd.exe 차이

### 증상
- PowerShell에서 `mingw32-make ... 2>&1 | Select-Object -Last 30` 호출 시 stderr가 ErrorRecord로 mangled되어 실제 빌드 출력이 깨져 보임
- bash에서 gcc subprocess가 exit 1로 침묵 종료 (sandbox 환경)
- cmd.exe에서 Makefile recipe의 sh 의존 명령(`find`, `sed`)이 실패

### 권장 (Windows)
**MSYS2 MinGW64 shell** 또는 **Git Bash**를 사용한다. PowerShell/cmd.exe는 보조 용도.

CI는 `windows-latest` + `msys2/setup-msys2` native MinGW/MSYS2 runtime이 공식 라인. plain Linux-hosted gcc는 acceptance line이 아님.

### PowerShell에서 어쩔 수 없이 빌드해야 하면
```powershell
Set-Location E:\PergyraLang
& mingw32-make rebuild *>$env:TEMP\build.log
$LASTEXITCODE
Get-Content $env:TEMP\build.log -Tail 30
```
stderr를 파일로 떨어뜨리고 별도로 읽어야 mangled되지 않는다.

---

## 3. CONFIG_STAMP 작동

### 정의
`Makefile:96` — `$(BUILD_DIR)/.config_llvm_$(LLVM_ENABLED)_$(CC_TAG).stamp`

### 트리거
다음이 변경되면 모든 `.o`/`.d`가 강제 삭제 + 재빌드:
- `LLVM_ENABLED` 플래그
- 컴파일러 변경 (`CC_TAG`)

### 트리거 안 되는 것
- 소스 파일 자체의 mtime
- `.inc` include 추가/제거
- 헤더의 macro 정의 변경 (이건 `.d` dependency가 cover해야 하는데 stale 시 누락)

→ 헤더/매크로 의심되면 `make rebuild`로 강제.

---

## 4. Stale .o 진단

### 증상 발견 절차
1. 소스 수정한 사이트에 `#error "marker"` 추가
2. `mingw32-make bin/pgy.exe` 실행
3. 컴파일 에러가 안 나면 → 그 .o가 stale

### 즉시 대응
```sh
find build -name "*.d" -delete   # dependency 캐시 비우기
mingw32-make bin/pgy.exe
```
이래도 안 되면:
```sh
mingw32-make rebuild
```

---

## 5. CI/로컬 차이

### 자주 나오는 패턴
- 로컬에서 통과한 테스트가 CI에서 실패
- 원인: 로컬은 incremental build, CI는 fresh build

### 로컬에서 CI 환경 재현
```sh
mingw32-make rebuild
mingw32-make ci-windows         # 또는 ci-linux
```

`ci-windows`는 Windows C regression(`test-all`, `fmt-test-smoke`, `stdlib-test-smoke`, `example-test-smoke`)을 기본 실행한다. Windows LLVM smoke/backend-compare는 executable `llvm-config --libs core` evidence가 있을 때만 추가 실행하며, 단순 `C:/Program Files/LLVM/lib` 폴더 존재는 beta support evidence가 아니다.

---

## 6. 빌드 시간 단축 vs 신뢰성

| 상황 | 권장 |
|---|---|
| 한 파일만 수정, 빠르게 확인 | `mingw32-make bin/test_semantic.exe` |
| 헤더/매크로 수정 | `mingw32-make rebuild` |
| `.inc` 파일 수정 | `mingw32-make rebuild` (안전) 또는 의존 .o 삭제 후 빌드 |
| PR 직전 / merge 전 | `mingw32-make rebuild && mingw32-make ci-windows` |
| stale 의심 | 무조건 `mingw32-make rebuild` |

원칙: **"빠른 증분 빌드"보다 "신뢰 가능한 재빌드"를 우선**.

---

## 7. Shared `build/` 병렬 실행 금지

### 증상

두 개 이상의 `mingw32-make` gate를 같은 checkout에서 동시에 실행한 뒤,
링커가 다음과 비슷한 오류를 낸다.

```text
file in wrong format
unrecognized storage class
local symbol has no section
```

### 원인

여러 gate가 같은 `build/`와 `bin/`을 공유하면서 같은 `.o`를 동시에
컴파일/링크한다. MinGW object가 부분적으로 쓰인 상태에서 다른 링크가
읽으면 이후 증분 빌드까지 오염된다.

### 대응

순차 실행한다.

```sh
mingw32-make test-transpile
mingw32-make raw-escape-contract-test-smoke
```

`raw-escape-contract-test-smoke`와 `runtime-none-contract-test-smoke`는 source
contract를 항상 검사하고, 이미 있는 `pgy`만 실행 probe에 사용한다. 이 둘은
다른 build gate를 검증하기 위해 전체 compiler rebuild를 강제하지 않는다.

병렬 검증이 필요하면 gate마다 별도 디렉터리를 지정한다.

```sh
mingw32-make BUILD_DIR=/tmp/pgy-a-build BIN_DIR=/tmp/pgy-a-bin test-transpile
mingw32-make BUILD_DIR=/tmp/pgy-b-build BIN_DIR=/tmp/pgy-b-bin raw-escape-contract-test-smoke
```

이미 오염됐다면 해당 `.o`/`.d`를 지우거나 `rebuild`를 사용한다.

---

## 8. 참고

- `Makefile:704` — `clean` target
- `Makefile:707` — `clean-objects` (object만 삭제, 디렉터리 유지)
- `Makefile` — `rebuild` target (clean + all)
- `tests/diagnostics_json_smoke.sh` — JSON 진단 회귀
- `tests/compare_backends.sh` — C/LLVM parity 회귀
## Self-host world에서 semantic 0/0 뒤 AIR authority evidence가 끊기는 경우

증상은 semantic 진단이 `0 error(s), 0 warning(s)`인데 곧바로 다음 형태로
중단되는 것이다.

```text
PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
expected authority participant(s): <participant>
rir_boundary=<Zone> rir_authority=<none>
```

`authorized by`를 intent에 반복해서 붙이거나 임의 actor 이름을 추가하지
않는다. matching subject action이 `requires`/`within`/`authorized by self`를
소유하면 intent는 그 계약을 상속하는 것이 canonical이다.

이번 원인은 action-inherited zone boundary의 첫 구조 증거를 zone RIR scope가
제공한 뒤, 실제 `Authorize` op를 소유한 intent RIR scope가 같은 boundary의
두 번째 provider로 보존되지 않은 것이었다. AIR는 exact intent provider와
같은 step AST의 `Authorize` op를 함께 요구해야 한다. zone의 첫 authority
row나 다른 step의 authorization을 호환 증거로 쓰면 안 된다.

회귀 확인은 다음 세 층을 함께 본다.

- AIR unit: participant alias가 zone slot 이름과 달라도 exact intent scope의
  authority evidence가 남는다.
- world compile: `world.pgy --emit-c`가 0 errors/0 warnings로 끝난다.
- C/LLVM ABI: 실제 world→zone→subject action 호출 뒤 authority snapshot의
  zone과 participant가 정확하며, authority 삭제/교체는 artifact 전에
  거부된다.

## Hosted method가 뒤에 선언된 object/zone/world 값을 인자로 받는 경우

C의 `parameter has incomplete type`와 LLVM의
`type is not registered in LLVM type map`이 같은 source에서 함께 나타나면
source 선언 순서를 바꾸지 않는다. 이 문제는 사용자가 dependency 순서를
맞춰야 하는 문법 문제가 아니라 declaration inventory scheduler 결함이다.

C는 nominal layout/forward declaration과 hosted method body emission을
분리하고, 모든 domain value type이 완성된 뒤 body를 방출한다. LLVM도 모든
nominal layout과 zone/world layout을 등록한 뒤 hosted method signature를
등록한다. 두 backend 모두 MIR declaration inventory를 소비하며 AST를 다시
탐색하거나 unknown type을 scalar로 추측하지 않는다.

최소 회귀는 “앞의 subject hosted method가 뒤의 object를 by-value parameter로
받고 그 field를 읽어 `42`를 반환”하는 한 source를 C/LLVM으로 각각 compile,
run하는 것이다. world source 재배치나 duplicate forward typedef는 허용되는
수정이 아니다.

## Native type-resolution dependency scratch can exceed the 3 GiB build cap

The production-reachable `PgyCompilerWorld` build exposed a native compiler
memory defect before either backend became the dominant process. The completed
semantic graph had 27,807 nodes and 28,233 edges. Under the 4 GiB diagnostic
ceiling, the C build completed with `pgy.exe` as the top process at 3,522.4 MiB
peak private memory. A 3,072 MiB kill-on-limit run stopped the same compiler
before any C compiler worker started. This is not normal oracle, C, LLVM, or
self-host working-set cost.

The exact owner was
`semantic_type_resolution_record_named_dependency` in
`src/semantic/type_checker_resolution_graph_core.c`. For every dependency it
allocated both `bool visited[N]` and `size_t path[N]`, where `N` was the graph's
current node count, from `SemanticContext.scratch_arena`. The arrays were local
to one `type_resolution_find_path` probe, but arena ownership retained every
pair until the entire semantic context was destroyed. On a 64-bit host this is
at least `9 * N` bytes per dependency, accumulated while `N` grows. The
triangular model for the observed graph is about 3,369.2 MiB before arena
bookkeeping and the real graph/semantic state, which explains the measured
3,522.4 MiB peak. The graph itself was not a 3.5 GiB object; repeated
graph-sized scratch lifetime was the amplifier.

The fix collects each dependency edge without an immediate whole-graph path
probe and validates the completed graph once before building the topological
worklist. The checker snapshots the validated node/edge generation; if pass 2
adds a genuinely new node or edge, it validates that new generation once at
the boundary. Duplicate edges do not change the generation. The existing
cycle validator remains the diagnostic owner and preserves
`PGY_SEM_TYPE_DEPENDENCY_CYCLE`, edge provenance, cycle path, and fail-closed
behavior. The negative DAG gate rejects restoration of either the per-edge
`type_resolution_find_path` call or graph-sized context-arena scratch.

After this change, the same compiler-world source completed below the unchanged
ceiling: the C build peaked at 1,566.4 MiB private and the LLVM build at
1,226.0 MiB private. Treat these as fixed-input regression witnesses, not as a
new general memory allowance.

Use the pressure owner and graph stats together when diagnosing a recurrence:

```powershell
mingw32-make bin/pgy.exe
New-Item -ItemType Directory -Force `
  -Path '.tmp/self_hosted/world_driver' | Out-Null
$env:PGY_TYPE_RES_STATS = '1'
$env:PGY_DEBUG_SEMANTIC_TIMING = '1'

.\scripts\measure_build_pressure.ps1 `
  -Label 'driver-world-c-memory' `
  -Command '.\bin\pgy.exe' `
  -Arguments @('src\self_hosted\compiler\driver_bootstrap_main.pgy',
               '--backend=c', '-o',
               '.tmp\self_hosted\world_driver\driver_world_c.exe') `
  -LimitMB 3072 -StopOnLimit -TimeoutSec 300

.\scripts\measure_build_pressure.ps1 `
  -Label 'driver-world-llvm-memory' `
  -Command '.\bin\pgy.exe' `
  -Arguments @('src\self_hosted\compiler\driver_bootstrap_main.pgy',
               '--backend=llvm', '-o',
               '.tmp\self_hosted\world_driver\driver_world_llvm.exe') `
  -LimitMB 3072 -StopOnLimit -TimeoutSec 300
```

Inspect each summary's `top_private_process`, `peak_private_mb`, and phase
breakdown, then correlate stderr's `[type-res-stats] nodes=... edges=...` with
the semantic timing slots. Sample CSV numbers are emitted with invariant
decimal formatting; locale-aware thousands separators must not be used because
they change the CSV column count above 999.9 MiB. Keep the 3,072 MiB cap fixed.
Raising the cap,
removing compiler-world owners to shrink the input graph, splitting the world
into backend-specific graphs, or hiding the graph in another process does not
repair the lifetime defect and must not be accepted as the fix.

## `with caps io_write`인데 streaming `FileWrite`가 grant 없이 성공하는 경우

증상은 `WriteFile`은 `PGY_CAP_GRANT=io_read`에서 거부되는데 같은 내용을
`FileOpen(path, "w") -> FileWrite -> FileClose`로 쓰면 성공하는 것이다. 이는
action/zone 문제가 아니라 raw file-handle builtin이 static capability inference와
runtime `pgy_cap_require_export`를 모두 우회한 결함이었다.

현재 native owner는 다음을 고정한다.

- semantic은 literal `r`을 `io_read`, `w`/`a`를 `io_write`, `+`를 양쪽으로
  추론하고 dynamic/unknown mode는 양쪽을 보수적으로 요구한다;
- runtime은 실제 `FileOpen` mode를 다시 분류하고 `FileRead`/`FileWrite`에서도
  각각 capability를 재검사한다;
- `FileExists`도 ambient read로 분류한다;
- C/LLVM runtime gate는 denied write가 final artifact를 만들기 전에
  `class=capability-denied`로 중단되는지 검사한다.

회귀는 다음 두 gate로 확인한다.

```sh
PGY_BIN=bin/pgy.exe bash tests/capability/run_manifest.sh
PGY_BIN=bin/pgy.exe bash tests/capability/run_runtime_enforce.sh
```

이 수정만으로 artifact commit이 완성된 것은 아니었고, 일반
`FileWrite`/`FileClose`는 지금도 `Void`인 호환성 표면이다. compiler artifact는
이 raw handle을 사용하지 않는다. 아래 전용 transaction owner가 same-directory
temp, checked write/flush/close, atomic replace, typed receipt를 소유한다.

## compiler artifact writer가 MIR graph를 다시 검증해 수 GiB를 쓰는 경우

증상은 source-to-MIR projection 자체가 성공한 뒤 JSON 파일을 쓰는 단계에서
메모리가 다시 급증하거나, 같은 graph를 한 번 출력하는 데 검증 시간이 거의 두
배로 보이는 것이다. 원인은 production caller가 이미
`SelfMirProgramFactsReady(facts)`를 통과했는데 compatibility writer가 같은
whole-program facts를 다시 검증한 것이었다. graph 전체 검증은 산출물 한 번마다
다시 지불할 local guard가 아니라 generation이 바뀔 때 한 번만 지불하는 owner
경계다.

현재 production 경로는 다음을 고정한다.

```text
DriverRung2MirProjectionFromAnalysisObserved
  -> SelfMirProgramFactsReady(facts)          # exactly once
  -> SelfMirProgramJsonWriteArtifactVerified # no second graph validation
  -> CompilerArtifactBegin/Write/Commit
```

raw/외부 facts를 받는 compatibility entrypoint
`SelfMirProgramJsonWriteArtifact`는 계속 정확히 한 번 검증한다. 반대로 이미
검증된 production caller만 `...Verified`를 호출한다. 검증을 없앤 것이 아니라
증거 lifetime을 소비자에게 운반해 같은 graph를 매번 재검증하던 중복을 없앤
것이다. `tests/artifact_atomic_transaction_contract_smoke.sh`는 production caller가
다시 validating wrapper를 호출하거나 writer 내부에 두 번째 readiness 검사가
생기면 실패한다.

파일 공개도 같은 단일 경계 원칙을 따른다. C-inline과 LLVM-linked runtime은
`pgy_runtime_artifact_transaction_core.h` 하나를 소비하고, 최종 경로는
write/flush/close가 모두 성공한 뒤 한 번만 atomic replace한다. test-only fault
injection은 open/write/flush/close/publish 각각에서 기존 sentinel final이 그대로고
temp가 0개이며 success receipt가 없음을 확인한다. 이 계약은 **atomic
visibility**이지 **crash durability**가 아니다. file/directory sync가 없으므로
전원 손실 이후의 영속성을 주장하지 않는다.

회귀는 다음 gate로 확인한다.

```sh
make artifact-atomic-transaction-test-smoke
make self-host-mir-json-instruction-writer-parity-test-smoke
```

## Self-host codegen exits with `0xC00000FD` while reading `main_ast.txt`

The Bash wrapper may report exit 127, while the Windows process exit is
`-1073741571` (`0xC00000FD`, stack overflow). In the observed failure the
current 2.46 MiB `main_ast.txt` was valid and `gen0.exe` retained the normal
2 MiB PE stack reserve. GDB showed 123 nested `ParsePrimaryFact` / expression
precedence frames. The nesting came from a manually duplicated
`SemanticBuiltinSignatureContractReady` expression: every builtin row added
another parenthesized `&&` term, so growing the language registry also grew the
parser call stack.

Do not raise the executable stack reserve or delete the contract checks. The
signature registry is the fact owner, so it must validate its projection with
one bounded loop. `SemanticBuiltinSignatureProjectionPrefixReady` walks
`SemanticBuiltinSignatureRows()` once and compares the consumer arrays. Exact
registry length and a consumer-owned tail row are checked separately. The
expression-environment contract consumes that verifier instead of reproducing
all builtin indexes. This keeps the contract proportional in work but constant
in source-expression nesting and prevents a second keyword/builtin authority.

The component gate rejects restoration of the observed high-index manual
projection chain. Verify the executable boundary with:

```sh
make self-host-component-contract-test-smoke
make self-host-codegen-bootstrap-seed-test-smoke
```

A normal run reaches `seed artifacts ready: gen2 codegen and parser AST
producer`. During the fixed-input witness, `gen0` stayed near 490 MiB private
and `gen1` near 560 MiB; those values are diagnostic observations, not new
memory allowances. Diagnose them separately from repeated whole-MIR graph
validation and native type-resolution scratch retention.

If seed readiness succeeds but the integrated driver then reports `expected
statement terminator` at `public zone`, inspect top-level visibility dispatch.
The self-host parser must consume `public`/`private` through `LanguageWordId`,
and must carry `public`/`export` into the nominal AST as `[export]`. Skipping the
word without carrying the fact merely moves the failure: parsing may continue,
but native/self-host AST parity is still false. The committed
`top_level_visibility_decl` parser fixture covers both private class and public
subject; the integrated `public zone DriverRung2DirectMirZone` is the production
falsifier.

## Integrated self-host semantic fails at match binding after parser parity passes

If the diagnostic points to `match_binding_environment` or
`SeedMatchBindings@...`, compare how the AST artifact was constructed. The old
parser artifact carried a typed `Case:` atom **and** a separate
`match_pattern_graphs` row joined by ordinal. `AstTreeArtifactFromText`, used by
the native/compact bootstrap bridge, had the atom but an empty pattern graph.
The same case therefore succeeded through the self-host parser artifact and
failed through the compact bridge.

Do not add `graph if present, otherwise parse atom` fallback. Match pattern
identity now belongs to the typed `MatchCase` atom and
`AstMatchCasePatternFactFromArtifact` is the only semantic/MIR interpretation
boundary. `AstTreeArtifact` payload schema v3 has no `match_pattern_graphs`;
the parser partition owner and ordinal join are removed. Unsupported
`or`/guard/string patterns plus malformed, duplicate, or non-identifier
bindings fail closed at the bounded fact owner.

Verify with:

```sh
make self-host-component-contract-test-smoke
make self-host-parser-parity-test-smoke
make self-host-one-mir-dual-backend-projection-test-smoke
```

The first gate rejects return of the retired graph and consumer-local semantic
atom parsing. The parser parity gate proves 189 native/self-host AST projections
remain byte-equal. The integrated gate is required because byte-equal text alone
does not prove semantic reachability through the compact artifact path.

## Integrated driver reaches `Action:` but codegen expects `Body:`

The 2026-07-27 one-MIR integration first exposed two invalid shortcuts before
reaching the action clause itself. `compiler_world_direct_mir_owner.pgy` had an
untyped `compiler_world` local, then called the 19-member world constructor
with one argument and depended on native aggregate zero-fill. The self-host
semantic correctly rejected these as an unresolved local and `expected 19 /
actual 1`. The fix is an explicitly typed local and an exact-arity world:
`PgyCompilerWorld` contains only the production-reachable `direct_mir` member;
the other 18 declared zone types remain target topology until their direct
production bypass is deleted. Do not fill them with fake subjects/schema facts
or weaken constructor arity.

After those fixes, the same integration reaches typed AST node 88972:

```text
Action: EmitDirectMir
  Returns: DriverRung2ExecutionResult
  Within: DriverRung2DirectMirZone
  Authorized by: self
  Body:
```

and failed closed with `expected Body:` at `Within:`. This was the executable
falsifier for the `ActionContract` gap: the self-host declaration/codegen path
treated action like an ordinary function and did not carry
`requires`/`within`/`causes`/`authorized by`/caps/effects as one typed fact.

The fix does not skip rows until `Body:`. `Action:` and every clause now have
distinct typed AST kinds; `SemanticAstActionContractFacts` binds their exact
node IDs to the callable `SyntaxNodeId`; codegen advances only over those owned
nodes. Native and self-host MIR declarations emit the same
`callable_kind + contract` object, and `mir_lower` validates it once before
reconstructing the action.

The focused `function_clause_order_minimal` gate rebuilt both C- and LLVM-built
drivers and observed native/self MIR parity. It rejects the original six field
faults plus unknown, duplicate, noncanonical, and `local + nonlocal` vocabulary
mutations before backend output. `semantic.callable_contract_vocabulary` now
owns the 9 capability and 9 effect rows; native/self/runtime consumers use its
direct or generated projections. If this failure returns, check the first layer
that lost `ActionContract`; do not add a cursor scan, default function contract,
or consumer-local string table. This declaration seam is `CLOSED`, but it does
not make the production action `SUBSTITUTING`.

#### Multi-ability role or zone slot makes self MIR declarations disappear

The focused C shard later exposed two adjacent declaration-carriage defects.
First, `SelfMirDeclarationsFromAnalysis` returned an empty declaration table
whenever one role implemented more than one ability. The old code also assigned
the entire role method span to every impl. The projection now partitions each
impl from the referenced ability semantic row and rejects a partition that does
not cover the role method span exactly.

Second, the compact AST inventory classified subject/object/tobject slots as
nominal fields but omitted `EffectSlot:` and `RelationSlot:`. That erased
`damage: Damage` and relation state before MIR. Both labels now enter the same
typed nominal field stream. The focused action subgate and canonical native/self
MIR parity are green after these fixes.

The shard then reached a later, distinct historical RED:

```text
CODEGEN ERROR: unsupported C ABI value type ...: Damage
```

The fix did not accept an unknown capitalized type as a struct. Native MIR now
projects `AST_EFFECT_DECL` explicitly as `kind=effect, nominal_kind=effect`, the
self-host typed arena and semantic constructor facts preserve that identity,
and `causes Damage` must resolve to an actual effect declaration. The same
focused C shard now compiles and runs through the explicit `Damage` value ABI.
It also exposed a role ABI defect: an impl method with zero explicit parameters
was emitted once as `HeroCombat_Ping(void *self)` and once as
`HeroCombat_Ping(void)`. The emitter now preserves the implicit role receiver;
the emitted-C gate rejects the receiver-free signature.

The adjacent slot loss is closed only as a carriage bridge. A compiler-owned
`mir_decl_field_kind_vocabulary.def` registry defines 14 stable wire spellings
and AST projection labels; the self-host projection is generated and checked
for drift. Native/self declarations now carry `field_kind` and reconstruct
`SubjectSlot`, `ObjectSlot`, `TObjectSlot`, `EffectSlot`, `RelationSlot`, shared
fields and current world/roster labels without guessing from field name/type.
The focused gate rejects effect/class identity drift, a missing effect,
unknown `causes`, missing/flattened field kind, a flattened zone effect slot,
and loss of the effect's exactly-one subject participant before backend output.

Do not interpret that green shard as zone runtime closure. Pool capacity,
vessel/binding distinction, relation declaration admission, stable field
identity, refresh/authority/state/lifecycle topology and the C/LLVM runtime
operations remain open. The next executable falsifier is
`zone_layer_projection_runtime`; its topology must be derived once from typed
facts rather than rebuilt from AST text in each backend.

During the observed runs, large seed generation remained in the hundreds of
MiB and integrated gen2 emission peaked around 1.33 GiB private, below the
unchanged 3 GiB cap. This is separate from the fixed 20 GiB/3.5 GiB repeated
graph-validation defect; the runtime is still expensive but the old memory
growth pattern did not recur.

### Array-only emitted C loses runtime headers

An Array program can use the collection runtime without otherwise using
`String`. `CollectionRuntimeCArrayBlock` still emits the owned-String helpers,
which reference `strlen`, `memcpy`, and `PGY_RUNTIME_PANIC`. The observed
`valid_array_builtins` failure happened because header selection did not receive
the already-owned `uses_array` fact, so generated C lacked both `<string.h>` and
`pgy_runtime_panic_contract.h`.

The runtime-header owner now receives `uses_array` explicitly. Array emission
selects `<string.h>` plus the narrow panic-contract header; it does not claim
that the array uses the String language surface, and it does not include the
entire `pgy_runtime.h`. `RuntimeCHeaderOwnsCheckedArithmetic` deliberately passes
`uses_array=false`, because the panic contract alone does not own checked
arithmetic helpers. The focused lifetime/component gates reject old call arity
and missing header relationships; `valid_array_builtins` emitted C compile/run
is the executable witness. The MIR JSON parity harness must compile temporary C
with `src/runtime` on its include path; otherwise the correct narrow header is
misreported as a generated-code failure. Removing either header must make the
C11 negative compile fail.
