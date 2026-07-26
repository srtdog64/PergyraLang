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
