# Build Troubleshooting

마지막 업데이트: 2026-08-03

빌드/회귀 도중 자주 마주치는 문제와 대응. **항상 `mingw32-make rebuild`를 먼저 시도**하면 절반은 풀린다.

---

## General scalar CFG rejects a Log or silently pressures a topology mini-compiler

Symptom: source-to-MIR succeeds, but direct C/LLVM projection reports
`direct MIR scalar CFG Log fact is invalid` or a new fixture appears to require
another block-count-specific emitter.

First check ValueId dominance rather than adding a fixture route. Two distinct
producer/consumer cases were reached on 2026-08-03:

1. `nested_if_in_loop.pgy` retained a false edge from a constant-true loop
   header to the exit even though every body path breaks. The exit used the
   inner merge value `largest.8`, which did not dominate that infeasible edge.
   The producer repair uses the owned Bool expression graph plus loop
   reachability fact to omit only that impossible edge.
2. `break_after_stmt.pgy` had a genuinely feasible condition exit and a break
   exit. Its exit used break-path `i.4` without a phi covering the header-false
   predecessor. The closed repair captures each break predecessor/local-version
   snapshot and emits producer-owned `i.8 = phi(i.2, i.4)` at the exit.
   The bridge and topology-specific break plan/emitter are deleted; a backend
   no longer invents or guesses an exit value.
3. `multiple_break_exit.pgy` reaches an exit with header `i.2` and two break
   predecessors both carrying `i.4`, so the producer preserves three phi slots
   `[i.2, i.4, i.4]`. Equal ValueIds are not ambiguity by themselves: each
   predecessor consumes one physical incoming slot. Slot consumption alone is
   insufficient, however. The first repair falsely accepted forged stale
   `[i.2, i.2]`. Binding must also scan every same-local routine definition and
   require the latest definition dominating that predecessor. Incoming row
   permutation may change storage order but not C/LLVM artifact bytes.
4. A range `for` can capture break blocks and version snapshots yet still use
   only the block list to connect exit edges. If an outer local is observed
   after the loop, the producer must merge the header/range completion lane and
   every reachable break snapshot before projection. Do not move that missing
   phi to a for-specific backend or treat the block list as value authority.

The general owner is selected by supported typed operations and source-local
types, not fixture name or exact block count. It validates exact
predecessor-to-incoming phi binding, latest dominating local ValueId,
assignment-target graph, loop-flow receipt, and non-backedge break targets.
Changing a valid edge or comparison into another valid CFG program is not a
negative test. Missing edges, non-dominating/stale uses, unbound phi incoming,
wrong assignment targets, and break backedges are negatives.

Use these gates in order:

```text
tests/self_hosted/parity/one_mir_scalar_cfg_graph_projection.sh
tests/self_hosted/parity/one_mir_scalar_cfg_break_exit_projection.sh
tests/self_hosted/parity/one_mir_cfg_air_plan_projection.sh
tests/self_hosted/parity/public_nested_scalar_cfg_llvm_owner.sh
tests/self_hosted_component_contract_smoke.sh
```

Do not repair the symptom with `block_count == 8`, a nested-loop emitter,
uses-array position as predecessor identity, source/`expr0` reconstruction, or
fallback to the older planner after the general route has claimed a graph.
The component gate rejects restoration of the retired bridge and compiler-side
break shape/plan/emitter. AIR may retain its bounded historical certificate,
but no compiler planner or emitter may consume it.

## A direct-mode gate says the bootstrap root lacks CLI option strings

Symptom: the installed driver accepts `--mir-json-backend=c|llvm`, but a static
gate reports `bootstrap CLI is missing direct mode`.

The composition root no longer owns argv spelling. Check
`driver_rung2_cli_request_owner.pgy`, which admits the complete request before
I/O; `driver_bootstrap_main.pgy` only composes and executes that typed request.
Pinning option strings to the root is a stale ownership assertion and can stay
red after the executable path is correct. The dual-backend gate must inspect
the CLI request owner while keeping the production execution action gate.

## Installed and standalone self-host drivers interpret the same argv differently

Symptom: `--emit-mir-json-verified SOURCE THIRD` or `--mir-json INPUT THIRD`
writes a file under the installed driver but treats `THIRD` as a machine
manifest under the standalone driver. Downstream compilation may still pass,
so this is an argv ownership defect rather than a backend failure.

The closed contract is `pergyra.selfhost-driver-cli.v1`:

```text
stdout + machine facts:
  MODE INPUT --machine-manifest-json MANIFEST

artifact publication:
  --emit-mir-json-verified SOURCE -o OUTPUT
  --mir-json INPUT -o OUTPUT
  --mir-json-backend=c|llvm INPUT -o OUTPUT
```

`driver_rung2_cli_request_owner.pgy` must admit the complete argv once before
I/O. `driver_rung2_cli_read_execution_owner.pgy` may execute only read/stdout
variants; `driver_rung2_installed_cli_owner.pgy` owns artifact variants. Do not
repair this by restoring positional-third guessing in either executor.

The falsifier is
`tests/self_hosted/parity/installed_driver_cli_mode_owner.sh`: legacy positional
forms, missing `-o` values, option-shaped paths, extras, and empty argv must
fail without output or transaction temporaries. Fixture manifests are test-only
and must remain outside `driver_bootstrap_main.pgy`.

When validating only this installed-driver change, do not use the full
`make self-host-compiler` chain as a timing sample: that target may rebuild the
seed parser and gen2 codegen first. Reuse the already validated seed and run
`tests/self_hosted/parity/self_host_compiler_build.sh`, then state that the
measurement covers the installed driver only.

## Canonical MIR reports stale call-return rows after source MIR succeeds

Symptom: `--emit-mir-json-verified` succeeds, but passing that artifact to
`--canonicalize-mir-json` fails with `semantic call return type rows are
incomplete` or rejects an already populated call-return row as stale. On a
compiler-scale graph, the same defect can also inflate elapsed time and memory
toward the 3 GiB pressure boundary even though the required fact was already
computed.

Check execution multiplicity before adding a cache or weakening validation. The
reached failure was:

```text
analyze + verify -> DriverRung2VerifiedFacts
canonical projection -> admitted-analysis projection -> verify again
```

The first verification correctly published `ToString -> String`. The second
verification revisited the same mutable analysis epoch, and the intentional
stale-row negative rejected the prefilled value. This was repeated owned work,
not a missing return-type rule.

The owner-correct repair is to carry the existing `DriverRung2VerifiedFacts`
receipt into `DriverRung2MirProjectionFromVerifiedFactsObserved`. That consumer
validates `SemanticAstBodyTypeBundleAdmissionReceiptReadyFor`, then lowers the
already ready body facts exactly once. Keep the stale-row rejection strict. Do
not make the resolver idempotent, reconstruct a return-type table, rescan the
AST/program root, or hide the second verification behind a cache.

Use
`tests/self_hosted/parity/canonical_mir_verified_projection_owner.sh` as the
focused falsifier. It emits `let_log.pgy`, canonicalizes through the installed
launcher, requires byte equality, canonicalizes the result again, and requires
the second result to be the same fixpoint. Broaden to
`tests/self_host_live_replacement_smoke.sh` only after this gate is green. For
memory evidence, record one final integration maximum; do not rerun a pressure
probe after every local edit.

## Canonical MIR swaps `Main`/intent identity or mixed generics become unknown

Symptom: native and self MIR contain the same routines and bodies, but
canonical output differs only in top-level routine `source_syntax_id`. A mixed
program may then fail raw MIR-to-C with:

```text
MIR generic specialization facts are incomplete:
MIR generic specialization identity is unknown
```

Check whether one producer preserves source interleaving while another emits
all functions and then all intents. JSON row position is not source order. Do
not repair this with a global function-first sort, a post-hoc ID patch, a
numeric offset, or a `new ? old` generic-identity fallback.

The closed owner split is:

```text
function phase --\
                  MirTopLevelRoutineOrderCursor -> canonical AST preorder
intent phase   --/

producer generic owner preorder --\
                                  MirGenericSpecializationIdentityEpoch
canonical generic owner preorder-/
```

Each producer phase must already be monotonic by its admitted
`source_syntax_id`; the cursor only merges the two heads. Methods remain with
declaration lowering. The generic epoch owner collects each distinct producer
and canonical owner once, sorts each within its own epoch, and maps equal
preorder ranks. The decoder then still requires exact lane, call ordinal,
callable, generic actuals, specialized symbol, and semantic graph call node.
Raw producer and canonical ID numbers are never compared, and no offset is
inferred.

Use `canonical_mir_routine_phase_identity_owner.sh` for cross-kind row
permutation, source-preorder preservation, repeated fixpoint, and same-phase
nonmonotonic rejection. Use
`generic_specialization_identity_epoch_owner.sh` for raw mixed
generic+intent MIR execution and invalid ordinal rejection without partial C.
Only then broaden to `self_host_live_replacement_smoke.sh`. Do not turn this
bounded identity mapping into a general query/cache engine or recompute it per
generic row.

## Role-operator targets disappear between semantic analysis and MIR

A role operator is not primitive arithmetic merely because its source token is
`+`. Semantic analysis must join the admitted ability, role, impl, method
signature, receiver type, and expression graph, then persist one exact target:

```text
call_target_kind = role_operator
call_target_name = <ability>.<role>.<method>
```

The backend must reject an empty, malformed, or cross-wired target. It must not
select the only visible role, decode the operator spelling again, inspect
display-only `expr0`, or retry primitive arithmetic after the role route has
claimed the graph. The method signature also requires exactly one `self`; an
empty raw source type for `self` is canonicalized by the signature owner and
must not be treated as a missing receiver fact.

Keep the operator vocabulary in one self-host semantic owner. The graph builder
and legacy self-host role dispatcher consume the same kind/alias/method mapping.
A native C table that has not migrated is a `BRIDGE`, not permission to add
another self-host table.

## A compiler-scale graph becomes slow or memory-heavy after a local semantic check

First look for a cumulative graph operation that is being repeated per body or
per optional feature. In the role-operator slice, the no-role program path
initially called the full expression-graph readiness check again after the
semantic bundle had already admitted that cumulative graph. This turned one
owner-boundary validation into repeated whole-program work.

The correction is an explicit declaration-presence branch:

- if role-operator declarations exist, resolve only the reached role targets;
- if none exist, scan only for a forbidden already-carried role target;
- do not rebuild, concatenate, copy, or revalidate the cumulative expression
  graph at this local owner.

The current sibling driver then processed the compiler-scale
`src/self_hosted/codegen/main.pgy` source to a 3,884,672-byte C artifact in
73.1 seconds. This is bounded performance evidence for the fixed operation, not
a general cache/query-engine justification and not a memory measurement.

## Self-host codegen results disagree with current source

Identify which compiler binary produced the result before debugging semantics.
`bin/pgy.exe` can be an older prebuilt seed while `bin/pgy-self-driver.exe` is
the current-source sibling. A diagnostic that remains unchanged after source
instrumentation is evidence of a stale executable, not evidence that the new
branch executed.

- Build/install the sibling through the official
  `tests/self_hosted/parity/self_host_compiler_build.sh` owner.
- On Windows invoke `C:\Program Files\Git\bin\bash.exe` explicitly.
- Pass repository-relative source and artifact paths to self-host tools; do not
  turn an absolute Windows path into a second path authority inside Git Bash.
- Record the exact binary size/hash with the test result.
- Do not claim the current sibling supports an option it rejects. At this
  checkpoint it does not expose the legacy `--backend=c` codegen-tool mode.

The current-source codegen tool can be emitted and host-compiled separately.
Its role fixture currently fails closed with
`role receiver target nominal-kind fact is missing`. That is the next codegen
receiver-admission blocker. It is not a failure of the closed direct-MIR role
operator projection and must not be hidden by native codegen fallback.

## 2026-07-30 exhaustive self-check and platform-contract failures

When `selfcheck_sources.sh` is strengthened, a new failure does not
automatically mean that the latest compiler rung introduced the defect. The
full 679-source C run exposed several older owner-boundary assumptions at once;
the repaired current manifest has since grown to 684 sources.
Repair the owning seam; do not edit the failing source merely to make the
checker accept it.

Observed failure classes and their owner-correct repairs:

- Imported enum declarations survive source-bundle flattening, but the
  lightweight semantic checker previously seeded only function and nominal
  constructor callables. Project every enum variant into the canonical callable
  table, including payload signature and enum return type. Keep qualified
  zero-payload values such as `ImportedDecision.ImportedReady` visible, and
  reject a missing qualified variant with `undefined_symbol`.
- A shared comma-range scanner treated every `<` as a generic opener. Thus a
  comparison such as `root < Limit(3), tail` swallowed the later comma. Track
  angle, parenthesis, and bracket depth separately, and recognize a type-angle
  opener only from the type-shaped lexical context owned by that scanner.
- Nominal field scans must consume optional `mut` after `let`; otherwise the
  constructor table records `mut` as the field name.
- Every standalone self-host source must import the owner it directly uses.
  Do not rely on transitive imports from a document store, projection, parser,
  or expression consumer. If two owners are an intentional recursive cluster,
  map the checker target to the declared cluster root instead of creating a
  circular import.

The focused C self-application after these repairs reported:

```text
[self-host-selfcheck] backend=c ok: 679 real sources accepted
[self-host-selfcheck] real-source self-application ok: 679 sources; backends=c
```

Platform contract drift found in the same CI run has three separate owners:

- Language-word implementation counts are generated output. Run
  `scripts/render_language_keyword_registry.py ... --write` after all `.pgy`
  edits, then run `tests/language_keyword_registry_smoke.sh`; never hand-edit
  the generated inventory.
- Production header count changes belong in the self-host checker golden only
  after `tests/production_header_size_smoke.sh` proves the actual census. The
  zone sync ABI header changed the clean count from 716 to 717 while leaving
  the 600-line cap intact.
- A runtime failure-string gate must read the canonical ABI owner. Zone lock
  initialization/read/write/unlock diagnostics moved to
  `pgy_runtime_zone_sync_abi.h`; duplicating those strings back into the old
  result/option header would restore dual ownership.

The semantic self-check proves source acceptance, not full bootstrap execution.
A later `mir-facts:start` failure must be diagnosed at the DIR/MIR contract or
consumer cost that reached it, and remains a separate gate.

### Full-bootstrap intent bindings and optional authority shape

The first fresh full-bootstrap rerun after the exhaustive source repairs failed
with `using binding unresolved`. That stable diagnostic prefix did **not** mean
that the production `using`, `where`, or participant spelling was wrong. All 14
production intent steps named a declared parameter and a matching zone. The
integrated AST instead revealed source-order classification: an imported intent
could be parsed before the subject/zone declaration that determines whether a
header parameter is an involved participant or a value.

The repair is declaration-order finalization, not moving imports. Both parsers
first emit or retain neutral header bindings, complete the declaration/import
graph, and then resolve each header parameter exactly once from the final
nominal inventory. A generic container remains a value; an exact subject/zone
identity becomes an involved participant; unresolved required native facts fail
closed. Do not reintroduce type-name suffix guesses or a `Zone`-typed
`IntentValue` blanket rejection: a value type such as `AuditZone` is legal when
no zone declaration owns that exact identity.

The self-host resolver must match only an indentation-anchored
`IntentBinding:` AST label. Searching the whole line or the final rendered
tree mistakes contract strings inside this owner's own source for unresolved
parameters and makes self-application fail. The parser gate therefore rejects
only anchored leaked binding rows after resolution.

The native import resolver has a distinct lifetime boundary: standalone
`parser_parse_program()` stays strict, while import loading defers only the
intent-role finalizer until recursive splice/normalization has produced the
whole AST. It then builds one exact declaration-name registry and finalizes all
header roles once. Cross-module zone/value and unresolved negatives belong in
`parser_imported_intent_composition_smoke.sh`; a parser-local success alone does
not prove this import boundary.

The corrected self resolver accepted its own focused source, but the integrated
whole-driver self-parser run produced no artifact before the 1,532.042-second
policy stop. Last observed working set/private values were 648,421,376 and
717,144,064 bytes; peak measurement was not enabled, so they are observation
lower bounds rather than exact peaks. Classify this result as CPU timeout /
incomplete, not parser failure, full-parser success, or memory regression.

The next reached seam was `self-host DIR authority shape is unsupported`.
Pergyra permits both a bare authority slot and one optional `requires` child:

```text
authority execution
authority semantic requires CompilerSemanticAnalysis
```

The native graph meaning is one authority node with two ownership/subject
edges, plus two edges for every required ability. The self-host DIR owner now
admits child count 0 or exactly one well-formed `requires` child and still
rejects unknown slots, unknown abilities, other child kinds, and multiple
children. The focused native/self anchor for the existing bare-authority driver
fixture is 10 nodes, 9 edges, and domain graph id
`14937234930791904899`.

Fresh process-tree measurements distinguish these semantic failures from the
old memory defect. The post-import-order codegen seed completed in 491,300 ms
at 1,504.5 MB peak private. Its full fixpoint reached the later authority seam
in 1,070,135 ms at 2,270.2 MB peak private, below the unchanged 3,072 MB cap.
After the authority repair, the codegen seed completed in 573,290 ms at
1,504.7 MB peak private. The following full integration attempt reached
`mir-facts:start` and was manually stopped at the fixed policy boundary after
2,534,272 ms; peak private memory was 2,284.8 MB and peak working set was
2,134.1 MB. It is an incomplete CPU timeout, not a passing build. These are
project-tree peaks, not whole-desktop memory totals; none reproduces a 20 GB
compiler process.

The historical high-pressure cause remains the repeated validation of an
already completed whole-program graph from local consumers. The owning boundary
must validate that graph once and carry the typed readiness/identity evidence.
A consumer-loop revalidation is a regression even if a small fixture happens to
stay below the cap.

The current bottleneck is therefore split explicitly:

The pressure owner samples its own process tree and writes the maximum to the
final summary. Do not repeatedly query live processes or tail the raw sample
file during a normal run. Read `peak_private_gib` and
`attention_required` once after termination. The hard limit is 3 GiB; the
memory attention threshold is 80% (2.4 GiB). A result below that threshold is
recorded but does not become an optimization task.

- memory: no 20 GB compiler process reproduced; the latest full peak is 2.228
  GiB, below both the 2.4 GiB attention threshold and 3 GiB hard limit;
- CPU: full integration still does not complete inside the policy window;
- observed CPU improvement: one-time MIR input-row validation advanced the
  30-minute cutoff from routine 588 to 696. Function-scoped statement and local
  ranges advanced it again to routine 776, a 32.0% increase from the original
  cutoff without increasing memory pressure;
- focused shard: the existing seed runs MIR only for five minutes and reached
  routine 303 at 1.561 GiB peak private. Semantic verification consumed about
  151.6 seconds and routines 0..302 about 125.1 seconds. The slowest routines
  were large AST-text projections, so the next falsifier is routine-lowering
  fact access, not memory tuning;
- rejected experiment: seeding each routine with a value snapshot of the
  program expression graph avoided repeated whole-graph equality but regressed
  the five-minute cutoff from routine 303 to 267. Peak private stayed at 1.562
  GiB with `attention_required=false`. The change was reverted. The graph must
  remain program-owned while MIR instructions carry only root/range handles;
- next evidence: the pressure-observed `mir-facts` owner now emits bounded
  iteration-validation, generic-specializations, domain-projection,
  routine/intent row, and canonical-ID start/done events. The pressure summary
  records `observed_stage_count` and `last_observed_stage`; use the final open
  event to select one owner before measuring its operation count.
- no cache/query identity has been approved yet. Define one only after that
  measurement proves a repeated query at this exact production boundary. A
  generic query engine, longer timeout, or larger memory cap is not a diagnosis.
- The summary-field wiring was executed with one synthetic
  `[driver-pressure-stage] mir-facts:test:start` line: the JSON reported
  `observed_stage_count: 1` and preserved that exact line as
  `last_observed_stage`. This verifies instrumentation transport only; it is not
  a full-bootstrap performance result.
- A clean dependency rebuild caused by adding `parser_program.c` was sampled
  while native `gcc` was compiling the full compiler source set. The visible
  `gcc`, `cc1`, `make`, `bash`, and `sh` process set used about 48.9 MiB working
  set and 43.2 MiB private memory at that sample. This is not a peak profile,
  but it confirms that ordinary native compilation is not the multi-gigabyte
  owner; the historical pressure belongs to repeated self-host whole-program
  graph work.

### `self-host-compiler` 시간과 실제 driver install 시간 구분

`make self-host-compiler`는 순수 driver C compile target이 아니다. 현재 Makefile은
먼저 `self-host-codegen-bootstrap-seed-test-smoke`를 실행해 native gen0, parser
producer, Pergyra-built gen1/gen2를 만든 다음 `self_host_compiler_build.sh` install
leg를 실행한다. 따라서 이 target의 wall time을 일반 C/LLVM 빌드나 driver install
시간으로 기록하면 안 된다.

2026-08-02 관측에서는 clean native dependency rebuild와 seed 생성이 이어졌고,
bounded runner의 15분 output-cell 제한이 gen2 생성 직후 끝났다. 이는 driver source
compile 실패도, seed gate green도 아니다. 같은 최신 parser/gen2를 사용해 실제
install leg만 실행했을 때는 128.8초에 완료됐고 3,864,005-byte driver를 설치했다.

Self-host owner를 반복 수정하면서 parser/codegen seed source가 바뀌지 않았다면 다음
owned install leg로 시간을 분리해 측정할 수 있다.

```sh
PGY_BIN="$PWD/bin/pgy.exe" \
PGY_SELF_DRIVER_BIN="$PWD/bin/pgy-self-driver.exe" \
PGY_SELFHOST_CC=gcc \
bash tests/self_hosted/parity/self_host_compiler_build.sh
```

이 경로는 seed가 현재 source와 맞다는 증거를 새로 만들지 않는다. Parser/codegen
seed owner가 바뀌었거나 clean checkout이라면 먼저 seed target을 실행해야 한다.
반대로 focused projection gate에서 매번 seed/bootstrap breadth를 재실행하는 것도
잘못이다. 현재 semantic target마다 driver를 한 번 설치하고, 같은 설치 산출물로
그 target의 C/LLVM positive/negative gate를 실행한다. Full bootstrap/fixpoint는
scheduled 또는 merge boundary의 별도 증거다.

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

The 2026-07-29 typed-intent execution slice applies the same rule explicitly.
`MirIntentExecutionPlanReady` performs full schema/topology/digest validation
exactly once in `machine_layer_fact_owner.pgy`. Codegen and compiler consumers
receive the admitted carrier and must not call plan readiness/digest, step or
terminal readiness, or recursively rebuild an expression graph. The static
protocol gate scans every self-host consumer and rejects a second validation
call or a restored `SemanticAstExpressionGraphForNode` projection read. The
canonical v2 fixture admitted 2 steps/3 terminals at digest `1268084794`; the
multi-routine fixture admitted at `1173492658`; 41 wire/identity mutations
failed before partial C. During the fresh production-like self-driver rebuild,
the observed concurrent high point was about 791 MiB private for `pgy`, 739 MiB
for its `cc1` child, and 1 MiB for `gcc` (about 1,531 MiB process-tree private),
not 20 GiB. A second fresh rebuild observed about 1,531 MiB again. This is a
sampled concurrent high point, not a new general memory allowance.

The final production-consumer gate also proved that a single admission cannot
be weakened after the memory fix. Plan-owned `on`, compensation, and terminal
graphs require the sealed persisted shape `{root,digest,nodes}` with a positive
digest. Missing/zero/extra digest fields, foreign or missing exact action
targets, missing named graph targets, and reachable/non-empty zero-compensation
scaffolds all fail before partial C. A consumer-side revalidation or graph
rebuild is therefore both a correctness regression and the first suspect for
another multi-GiB observation.

If this slice regresses, do not raise the cap or add a consumer cache. First
run `intent_execution_protocol_static_owner.py` and inspect whether admission
lost its sole `MirIntentExecutionPlanReady` call, whether a consumer restored
`MirIntentExecutionPlanDigest`/step/terminal readiness, or whether projection
started reparsing graph subtrees. Validate once at the owner boundary, carry
the receipt, and join later consumers by exact routine/block/instruction and
declaration identities.

#### 2026-07-31 completeness program graph and repeated source scans

GitHub Actions run `30562668988` exposed a graph-identity defect rather than
an unsupported language feature. Semantic completeness checked the
import-composed program rooted at `src/self_hosted/compiler/world.pgy`, while
codegen consumed an externally exported source-unit AST with imports disabled.
The same ledger row therefore meant two different programs. The codegen
`--check-source` path now performs root parsing, typed semantic analysis,
semantic admission, and codegen shape checking over one `AstTreeArtifact`.
The external source-unit AST and text-node inventory bridge are no longer part
of this completeness path.

Three repeated-work defects were measured on that exact program:

- parser declaration/import composition repeatedly finished and concatenated
  recursive text blocks, expression rows were fully validated after each
  append, and intent binding resolution repeatedly scanned the multi-megabyte
  tree by character;
- codegen rebuilt a text-node inventory after typed semantic analysis and
  repeatedly searched sparse fact arrays for each syntax node;
- `RejectUnsupportedCodegenBuiltins` used a newline scan whose inner
  `StringLength(tree_text)` revisited the full roughly 5 MB artifact for
  every line.

The fixes keep one shared import-composition accumulator, use an import
`Set<String>`, perform only O(1) append-shape checks until the final
expression-graph owner admission, split the intent and codegen surface text
once by newline, consume the admitted typed arena directly, and use dense or
ordered NodeId lookup where the producer already guarantees that invariant.
No general cache or query engine was added.

Executable measurements on the same workstation:

| Check | Before | After |
|---|---:|---:|
| observed root parse | policy-stopped after more than 304 s | 11.7 s |
| typed world codegen check | 101.1 s initially; 74-77 s after partial indexing | 11.9 s |
| monitored final codegen process | not applicable | 10.614 s, 552.6 MB peak private, 504.8 MB peak working set |

Two stale diagnostic parser processes were also still alive at about 627.4 MB
and 414.7 MB private memory. They were stopped after their exact command/path
ownership was verified. A concurrent focused codegen process was about 545 MB.
This explains a large desktop total without establishing a 20 GB compiler
process. A future high-memory report must first separate one process peak,
descendant process-tree peak, and overlapping stale runs.

Completeness attribution now follows program identity. If several inventory
sources map to the same composed program target, semantic/codegen execute that
target once per stage and run, then reuse the result for each row even when
`PGY_SELFHOST_COMPLETENESS_CACHE=0`. The focused falsifier mapped
`expr_postfix_owner.pgy` and `expr_precedence_owner.pgy` to
`expr_owner.pgy`: the ledger reported 2/2 row passes, one unique check, and
one reuse. Treat a return to two target executions, per-append whole-graph
validation, or an exported AST/text reconstruction as a performance and SoT
regression.

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

## Zone frontier output은 같은데 topology가 사라진 경우

`zone_layer_projection_runtime`처럼 slot/layer 수가 graph depth보다 큰 fixture는
최종 loop limit과 stdout parity만으로 topology 손실을 발견할 수 없다. 관측된
정상값은 graph `nodes=3, edges=2, depth=2, pass_limit=2`지만 count-based floor가
3이라 생성 C의 `_pgy_zone_frontier_pass_limit`은 3이다. Graph가 비어도 이 floor만
남으면 실행 결과가 우연히 같을 수 있다.

이 경우 memory cap이나 pass count를 먼저 올리지 말고 다음을 확인한다.

1. `PGY_DUMP_PROPAGATION=1`로 C/LLVM compile trace를 각각 얻는다.
2. `trust <- player`, `trust <- enemy` 같은 exact dependency가 양쪽에 있는지 본다.
3. `propagation_graph_build_from_zone(ASTNode *)` 같은 backend AST builder가 다시
   생기지 않았는지 확인한다.
4. DIR row의 owner/directive/slot stable identity와 MIR의 same-source DIR binding
   negative를 실행한다.

현재 owner는 `dir.domain_graph`, MIR은 carrier, C/LLVM frontier는 마지막 consumer다.
`tests/domain_runtime_topology_smoke.sh`가 exact trace, count floor, 양 backend parity,
retired AST entrypoint 부재를 함께 고정한다. 전체 graph를 backend마다 재검증하거나
`MIR row가 없으면 AST` fallback을 추가하면 과거의 중복 graph 비용과 dual SoT를
동시에 되살린다.

## Native와 self-host의 `source_syntax_id` 숫자가 다른 경우

같은 source를 native와 self-host가 MIR로 만들었는데 declaration field ID가 서로
다른 현상은 그 자체로 메모리 손상이나 identity collision이 아니다. Native는
lossless AST의 canonical preorder에서 ID를 배정하고, 현재 self-host는 compact
typed-AST arena의 producer-local identity를 사용한다. Compact text는 type,
expression, generic bound 같은 hidden identity node를 평탄화하므로 row ordinal에
상수를 더해 native 번호를 일반적으로 복원할 수 없다.

판정할 때는 raw 숫자 equality 대신 다음을 확인한다.

1. 각 MIR 문서 안에서 field ID가 nonzero이며 중복되지 않는가.
2. topology reference가 같은 문서의 `(owner, field name, ID, field_kind)`와 exact
   join되는가.
3. canonicalization이 새 declaration ID와 모든 dependent topology ID를 같은
   identity epoch에서 함께 재발급하는가.
4. `player` 이름에 유효한 `enemy` ID를 붙이거나 canonical row 하나만 이전 raw
   ID로 되돌린 mutation이 backend 전에 실패하는가.

숫자 차이를 맞추려고 offset, name hash, AST-text 재파싱을 넣지 않는다. 그것들은
provenance를 복원하지 못하고 fixture별 우연을 두 번째 SoT로 만든다. 현재
non-empty topology의 MIR-to-AST canonicalization은 atomic remap owner가 없으므로
명시적으로 거부한다. 이 실패는 compatibility fallback을 추가할 신호가 아니라
다음 executable rung의 정확한 missing fact다.

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

### `ast_artifact_invalid` at a nominal constructor argument

The 2026-07-29 codegen seed failed closed at AST node `32501` with owner
`nominal_constructor_argument_type`. The failing expression was the second
argument of `AstExpressionGraphRows(...)`, an array literal containing a typed
kind call. The graph field-value type path tried scalar and intrinsic typing but
did not consume the existing array-literal type owner, so the constructor saw no
`Array<Int>` fact.

Do not add a constructor-name exception, recover the type from source text, or
relax nominal assignability. `SemanticExpressionGraphFieldValueTypeName` now
delegates array literals to `SemanticExpressionGraphArrayLiteralTypeName` before
the intrinsic path, and the array-literal owner has a focused typed-call fixture.
The diagnostic retains the owner and now includes constructor name and argument
index so another missing fact identifies its exact boundary.

Observed evidence:

- `aggregate_field_policy_probe_parity.sh`: PASS for the C oracle and graph-owned
  aggregate policy;
- the corrected gen0 consumed the full current `main_ast.txt`, emitted a
  55,720-line gen1 C artifact, and GCC compiled it;
- the formal seed script independently regenerated and compiled a 2.7 MiB
  `gen1.c`/2.0 MiB `gen1.exe`, passing the old node-32501 boundary. The run was
  stopped during gen2 emission after the 20-minute local edit-loop budget, so
  this observation is not a complete seed PASS.

If `air_graph_json_validator_parity.sh` differs only in
`mir_evidence_binding_fingerprint`, run the current producer twice before
changing the fixture. On 2026-07-29 both runs produced the same new fingerprint;
the owner-generated fixture was refreshed and live-drift parity passed. A stable
owner result is fixture drift, while changing results are a determinism defect.

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
  Returns: DriverRung2ExecutionOutcome
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

#### Action이 `tobject` payload enum을 반환하면 C에서 unknown type 또는 cycle

`subject.action -> enum -> tobject payload` 조합은 LLVM과 self C에서는 실행됐지만
native C가 action prototype을 outcome enum보다 먼저 출력해 `unknown type name`과
`conflicting types`를 냈다. 원인은 `transpiler_type_decl_schedule.c`가 nominal
field와 enum payload 의존성만 보고 hosted method/action의 by-value return type을
보지 않은 것이다.

수정된 scheduler는 `MIRDeclMethod.return_type_name`과 value-carried explicit
parameter를 같은 declaration schedule에서 소비한다. 다만 다음은 complete-layout
선행 의존성이 아니므로 기다리지 않는다.

- 이름이 `self`인 implicit receiver;
- nominal forward typedef 뒤 포인터로 전달되는 subject-like parameter;
- host struct 본문 뒤 prototype이 출력되는 direct host-self return/parameter.

이 예외가 없으면 `Alpha.action(... Beta)` / `Beta.action(... Alpha)` 또는
`ValueTool.Clone(self, other: ValueTool) -> ValueTool`이 가짜
`cyclic by-value type declaration dependency`로 실패한다. Focused fixture는 이 두
shape와 success/failure tobject payload를 함께 고정한다.

같은 rung에서 artifact failure 진단의 Bool을 local String에 조건부 대입했을 때
self C join이 빈 문자열을 만든 사례도 있었다. 진단 builder가 control-flow local
문자열을 합치게 하지 말고 Bool owner가 곧바로 `"true"`/`"false"`를 반환하게
한다. Production open-failure gate는 exact schema/path/stage/status/preservation/
cleanup 행을 Main에서 비교한다. 이 build에서 관측한 동시 peak는 `pgy` 약 808MB,
자식 `cc1` 약 1.03GB로 합계 약 1.8GB였으며 20GB 재발은 없었다.

2026-07-29의 현재 source로 `driver_rung2_main.pgy`를 다시 C 빌드한 Windows
프로세스 표본도 같은 범위였다. 200ms 간격으로 `pgy`/`gcc`/`cc1`의
`PrivateMemorySize64`와 `WorkingSet64`를 합산했을 때 peak private은 1,575.1MiB,
peak working set은 1,485.3MiB였고, peak 표본은 `pgy` 708.0MiB + `cc1`
867.0MiB였다. 빌드는 exit 0으로 완료됐다. 같은 환경에서 MSYS GNU
`time -v`의 `Maximum resident set size`가 30,667,532KB처럼 출력된 값은 Windows
process private/working-set 표본과 모순되므로 실제 30GB RSS 증거로 사용하지
않는다. 회귀 판단은 project pressure runner나 Windows process-tree 합산처럼
측정 owner가 명확한 지표로 한다.

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

#### MIR canonicalization repeats admission or recomputes a lossy domain graph

The first self-host empty-topology producer exposed another place where a
seemingly harmless bridge could repeat graph work. `MirJsonReadInput` already
returns data from a fully admitted `MirMachineLayerAdmittedJsonInput`; calling
`MirMachineLayerAdmitJsonInput` again in the canonicalizer would index and
validate the same document twice. Canonicalization now calls
`MirJsonReadMachineAdmittedInput` once and carries that typed result.

It must also preserve the admitted empty topology instead of recomputing it
from `EmitMirProgramTree`. The declaration tree projection does not carry zone
authority yet, while authority contributes one DIR node and six edges in
`function_clause_order_minimal`. Recomputing after that lossy round-trip would
turn the exact native `9 nodes / 16 edges` anchor into a different graph. The
source producer independently derives the 9/16 census; only MIR-input
canonicalization carries the already-admitted identity.

The non-empty projection now has the same rule. The self-host source producer
emits typed `refresh`, `publish`, `apply-effect`, and `link-relation` rows;
`apply-effect` is distinct from `maintain-effect` and contributes no dependency
edge. Canonicalization
reissues owner, directive, and field identities in one reconstructed epoch.
Restoring an old raw field ID or pairing `player` with the canonical `enemy`
ID fails before output. It does not repair IDs by offset and does not parse the
original source or provenance text.

`apply stateAlias` must not disappear at this boundary. Native semantic now
binds the alias to the exact effect/target slot names before DIR collection,
and the topology smoke verifies that the alias form emits the same typed
`apply-effect` identity. DIR treats an unresolved apply as a hard failure rather
than reducing the expected row count. The production self source parser still
rejects the shorthand; until it carries state declaration identity, do not add
a name-only self lookup or claim native/self parity for this syntax.

The target-neutral topology plan is built and fully validated exactly once in
`MirMachineLayerAdmitDocumentWithTopologyObserved`. Production C/LLVM
consumers receive a bounded receipt containing the graph identity, digest, and
cardinalities; they do not call the full plan readiness walk again. Digest
mutation is exercised only by the dedicated gate probe, never as a production
self-test. This distinction is important: putting a negative witness or a
whole-plan verifier in every emission call would recreate the repeated-graph
work pattern behind the earlier 3 GiB/20+ GiB incident.

Do not confuse plan consumption with runtime materialization. The current
self-host plan can project the exact `BattleZone` 3-node/2-edge schedule to C
and LLVM, but it does not yet create `.poison`/`.trust` storage or execute the
`apply` plus refresh/publish synchronization needed for runtime output
`7`/`dst`. The source producer carries the apply row, but the wire still lacks
effect/relation destination roles, projection member maps and receiver
carriage. Same-name/ordinal inference, generic zero-fill, a native topology
graft, or claiming the correct plan trace as the runtime result would hide the
missing fact rather than fix it.

2026-07-28 fresh pressure evidence after the non-empty topology producer and
one-plan change is green. `build-pressure-self-host-compiler` completed the
seed generations, installed `bin/pgy-self-driver.exe`, and passed its smoke in
2,138,300 ms. Peak sampled working set was 1,038.0 MiB, peak private memory was
1,132.4 MiB, and the largest single process was `gen2.exe` at 1,119.4 MiB.
The unchanged 3,072 MiB stop-on-limit boundary was not crossed. This rejects a
20+ GiB recurrence for the current bounded full build, but the 35-minute fresh
bootstrap remains explicit CPU/time optimization debt.

The first run exposed two non-memory dogfood defects before that green result.
The graph builder passed growable struct-member arrays directly to `ArrayPush`,
which the self-host semantic/codegen path correctly could not prove as a local
binding. The fixed owner builds local arrays and publishes one immutable graph
or plan. The emitted driver then collided with the canonical runtime's
`pgy_args` symbol and compiled atomics under an unspecified C language mode.
The self-host-only argv helper now has a distinct internal identity, and both
driver installer compile paths pin `-std=c11`. Do not raise the memory cap or
patch generated C for either failure.

A second fresh build after the world/tobject DIR and semantic changes completed
and installed `bin/pgy-self-driver.exe` in about 28 minutes. The one observed
`gen2.exe` peaked at 1,173.0 MiB private memory and 1,071.1 MiB working set.
This is consistent with the prior bounded sample and again rejects a 20+ GiB
recurrence; it remains CPU/time debt, not justification for a larger memory
allowance. The build also exposed another instance of the same Pergyra-native
builder rule: production domain-runtime assignment/codegen owners must collect
growable rows in local arrays and construct the immutable fact record once.
Direct `ArrayPush(result.member, ...)` relies on mutable record-member builder
semantics that the Pergyra-built driver does not admit. The component gate now
ratchets that form out of the affected production owners.

### A split owner is correct but a broad contract still names the old file

After a responsibility is extracted, a broad smoke can fail even though the
new owner compiles and focused gates pass. The July 29 CI run exposed four such
stale assertions: intent on-receiver inference still pointed at
`type_checker_intent_decl.c`; MIR pin entry still expected raw `PinRead` /
`PinWrite`; LLVM enum diagnostics still pointed at the generic constructor
owner; and C constructor-argument diagnostics still pointed at the domain
constructor orchestrator.

Do not copy the implementation or diagnostic back into the old file to satisfy
the grep. Resolve the current fact owner and last consumer, move the positive
assertion to that owner, and add a negative assertion where the old ABI or
owner path must not return. For secure MIR pin entry, the executable witness is
generated C containing `pgy_secure_pin_read_init_Int` plus
`pgy_secure_unpin_Int`, with no raw `pgy_secure_pin_read_Int(...)` call. The
default 50,000,000-iteration amortization gate and the full perf contract both
passed after the gate followed the typed owner.

### A Windows self-host gate launches a stale compiler and exits 127

The Windows preparation target built `$(PGY)` in its configured `BIN_DIR`, but
the callable-vocabulary script defaulted to `repo/bin/pgy.exe`. In a CI job
that reuses several build directories, that path may be a stale binary with a
different runtime dependency set. Its stderr was redirected into the fixture
directory, so Make reported only `Error 127` immediately after the native
vocabulary probe.

Executable gates must receive `PGY_BIN="$(abspath $(PGY))"` from the Make
target, classify that exact binary with `pgy_binary_path_helpers.sh`, prepend
the owned Windows runtime paths, and convert fixture/output paths through the
same helper. `build_source_inventory_smoke.sh` now rejects omission of either
the exact compiler identity or the shared path helper. The focused Make target
passes locally; a bounded full preparation run progressed past the old failure
and reached the longer component contract before its 180-second ceiling.

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


## 여러 top-level test target이 compiler 전체를 다시 빌드하는 경우

`make test-parser test-semantic`처럼 서로 다른 top-level target을 한 호출에
묶어 실행했는데 compiler source 전체가 두 번 컴파일되는 현상은 메모리 결함과
별개의 build-graph 중복이다. 2026-07-29 격리 WSL 실행에서 parser target 뒤의
configuration stamp 갱신이 기존 object를 정리했고, semantic target이 같은 source
tree를 다시 컴파일하는 것을 관찰했다. `-j2`에서는 peak memory가 폭증하지 않았지만
CPU, I/O, wall time을 중복 지불하므로 full graph 검증 비용을 왜곡한다.
같은 조사에서 동일 whole-graph gate를 이전 process tree의 종료 확인 없이 다시
시작해, reparented/orphan native worker 6개가 동시에 겹친 것도 관측했다. 이
합산 system pressure는 단일 compiler peak가 아니다.

진단과 운영 원칙은 다음과 같다.

- 같은 `CC`, `LLVM_ENABLED`, `BUILD_DIR`, `BIN_DIR` 조합은 하나의 configured build
  graph로 만들고 그 산출물을 여러 test binary가 재사용한다. 한 configured
  graph에서는 bounded whole-graph gate도 동시에 하나만 실행한다.
- 서로 다른 configuration은 동일 object directory를 공유하지 않는다. stamp 변경이
  다른 target의 정상 산출물을 지우는 구조를 병렬 실행하지 않는다.
- gate 재실행 전 wrapper와 descendant의 PID 및 전체 command line을 기록하고,
  timeout/종료 뒤 같은 PID가 남지 않았는지 확인한다. process name만으로는 이전
  orphan과 새 worker를 구분할 수 없다.
- memory verdict는 compiler process tree의 peak memory로 판정하고, 재빌드 횟수와
  누적 allocation/CPU는 별도 build-work 지표로 기록한다.
- 20+ GiB 회귀를 의심할 때 cap을 올리기 전에 whole-graph readiness 호출 횟수와
  configuration-stamp invalidation 횟수를 함께 확인한다.

이 관찰은 기존 repeated whole-graph readiness 결함의 원인을 바꾸지 않는다. 현재
정상 self-host build의 약 1.1--1.5 GiB 관측과 달리, 20+ GiB 증상은 이미 소유된
graph proof를 consumer/local row마다 다시 수행한 결함이었다. build target 재구성은
그와 별도로 중복 전체 컴파일을 없애야 하는 DX/throughput 작업이다.

### Source-to-MIR action을 검증하려는 production driver build가 산출물 없이 끝나는 경우

2026-07-29의 새 source-to-MIR world/action rung에서 C production driver build와
분리 C emission을 각각 120초로 제한해 실행했다. 두 시도 모두 frontend 진단은
`0 error(s), 0 warning(s)`였지만 timeout `rc=124` 전에 요청한 `.exe` 또는 `.c`
산출물을 publish하지 못했다. 두 로그의 SHA-256은
`1a9ded083816fe692fbfc6a0dafe1f90a7e40e4655706a8a0518e20eab74e3a8`로 같았다.
그러므로 이 결과는 build PASS도 action runtime FAIL도 아니다. Driver가 없으므로
`function_clause_order_minimal` 실행, MIR/C 비교, LLVM leg는 시작하지 않았다.

Timeout 뒤 `pgy`/`gcc`/`clang` descendant가 남지 않았음을 확인했다. 이 경우의
운영 규칙은 다음과 같다.

- frontend 성공 문구만으로 최종 artifact 성공을 주장하지 않는다. 요청 경로의
  존재, process rc, 다음 실행 leg를 각각 확인한다.
- C prerequisite artifact가 없으면 LLVM/parity leg를 병렬로 시작하지 않는다.
- 이 120초 결과를 historical 3 GiB/20+ GiB memory regression으로 분류하지 않는다.
  pressure-owned process-tree 표본이 없으므로 memory verdict는 `Unknown`이다.
- 다음 측정은 동일 source와 cache 상태를 기록하고 pressure owner 아래에서 phase,
  peak working/private bytes, artifact publish 시점을 함께 관측한다. Timeout 상향만으로
  correctness gate를 green 처리하지 않는다.

### 설치 self-driver가 문서의 bootstrap root와 다른 그래프를 실행하는 경우

2026-07-30 실제 launcher를 끝까지 추적하자 `bin/pgy --self-driver`는
`driver_bootstrap_main.pgy`를 실행하지 않았다. Native
`src/compiler/self_host_driver.c`가 sibling `bin/pgy-self-driver`를 실행하고, 그
binary는 `driver_rung2_main.pgy -> driver_rung2_cli_owner.pgy`에서 빌드된다. 설치
CLI에는 source-to-MIR direct compile이 남아 있어 문서상 world/action graph와 실제
사용자 graph가 갈라져 있었다.

해결 원칙은 물리적 stage 폴더를 합치는 것이 아니다. Lexer/parser/semantic/MIR
폴더는 fact lifetime owner로 유지하고, 설치 CLI와 bootstrap artifact root가 같은
`PgyCompilerWorld.source_mir -> DriverSourceMirExecution`을 소비하게 한다. Payload
admission은 하나지만 publication action은 capability 경계에서 나눈다.

- stdout `ProduceSourceMir`: `io_read`, typed payload receipt;
- artifact `PublishSourceMirArtifact`: `io_read, io_write`, path를 먼저 검증하고
  atomic commit 한 번;
- 금지: empty-path stdout sentinel, temp-file round trip, caller-side direct compile,
  helper로 이동한 우회 call site.

첫 실제 `make -j2 self-host-compiler` pressure run은 1,800초 상한에서 exit 124로
끝났고 final install은 완료되지 않았다. 그 전까지 관측한 peak는 working set
1,144.1MB, private 1,198.0MB, top `cc1.exe` 724.2MB였다. 따라서 build PASS는
아니지만 3GB/20GB memory regression도 아니다. 세대별 worker가 종료되면서
메모리가 내려갔고 누적 whole-graph validation 패턴은 관측되지 않았다.

Windows/MSYS에서는 timeout owner의 Windows parent tree가 끝난 뒤에도 Cygwin
reparenting된 `bash -> gen1.exe`가 남을 수 있다. 이 경우 다른 Codex 작업이나
이름만 같은 프로세스를 종료하지 않는다. Probe 시작시각 이후의 exact executable
path, command line, parent chain이 해당 run과 일치하는지 확인하고 그 chain만
bottom-up으로 정리한다. 이 orphan을 둔 채 다음 build를 시작하면 두 bootstrap이
겹쳐 메모리 결함처럼 보일 수 있다. 최종 판정은 더 긴 상한에서 fresh installed
artifact와 launcher runtime을 함께 관측한 뒤 기록한다.

후속 capability-split run은 worker를 중단하지 않고 82.7분까지 진행됐다. 원래
pressure writer는 약 49분에 live `Import-Csv` reader의 Windows file lock과
충돌해 종료됐지만, exact build chain은 계속 추적했다. 중단 전 CSV와 attach
monitor를 합친 peak는 working set 1,300.8MB, private 1,465.9MB였고 top owner는
`gen2.exe`였다. 즉 3GiB/20GiB 폭주는 재현되지 않았다. 다만 instrumentation
종료 때문에 이 run에는 완전한 v2 summary나 build rc가 없다. Live sample을
읽을 수 있다는 이유로 build PASS를 추정하지 않는다.

이 충돌 뒤 `measure_build_pressure.ps1`의 sample append는 `IOException`만 25ms씩
최대 20회 재시도한다. 재시도 한계를 넘으면 여전히 명시적으로 실패한다.
`build_pressure_contract_smoke.sh`가 bounded retry와 terminal diagnostic을
ratchet한다. Pressure CSV를 관찰할 때는 가능하면 tail/read를 짧게 유지하되,
짧은 reader lock이 측정 owner 전체를 죽이는 현상은 다시 허용하지 않는다.

해당 build는 프로세스 종료까지 갔지만 설치에는 실패했다. 생성된 `driver.c`는
389-byte `CODEGEN ERROR`였고 Pergyra-built semantic owner가
`ArrayPush(projection.on_texts, ...)`를 `projection.on_texts`라는 미정의 local로
분류했다. 기존 installed binary와 build stamp의 timestamp는 바뀌지 않았다.
해법은 member-array inout을 특별 취급하는 fallback이 아니다.
`MirIntentPhaseProjectionFromCarriers`가 distinct local arrays에 phase fact를
transactionally 조립하고 성공할 때 한 번 projection을 materialize한다. Error
outcome은 partial arrays를 publish하지 않으며 component gate는
전체 self-host source에서 `ArrayPush/ArraySet(local.member, ...)` 재도입을
거부한다. 작은 Pergyra-built
`gen2 --check` fixture는 이 staging shape를 통과했다. Fresh full install과
launcher parity는 별도 실행 증거가 나올 때까지 OPEN이다.

그 다음 staged-array full run은 5,101,206ms 뒤 exit 2로 끝났고 설치 artifact는
갱신되지 않았다. 측정 자체는 완전했다: peak working set 1,301.8MB, peak private
1,469.2MB, top `gen2.exe` private 1,455.7MB, `limit_exceeded=false`였다. 따라서 이
실패 역시 3GiB/20GiB memory regression이 아니라 self-host semantic coverage
결함이다. 생성된 483-byte `driver.c`는
`Clone(admitted.intent_execution_plan)` initializer의 type을 resolve하지 못했다.

`MirIntentExecutionPlan`은 admission이 이미 readiness/digest를 검증한 순수 struct
value carrier이고 projection에서는 read-only다. Native 일반-value `Clone`도 deep
copy가 아니라 value pass-through이므로 이 경계에 새 copy owner나 재검증을
추가하면 안 된다. Intermediate typed local은 old gen2 inference 오류를 제거했지만
current compiler는 `ref admitted`에서 나온 member를 새 local과 반환 struct field로
escape시키는 것을 정확히 거부했다. 최종 shape는 projection 함수의 typed value
parameter `plan: MirIntentExecutionPlan`이다. Caller가
`admitted.intent_execution_plan`을 이 value boundary로 투영하고 반환 view가 그
값을 소유한다. Machine receipt 전체를 `own`으로 넓히지 않으며 detached local이나
polymorphic `Clone`도 없다. Static gate는 이 call edge와 value parameter를 고정하고
Clone, local detachment, plan 재구성/재검증을 함께 거부한다. Nested nominal member
`Clone` inference 자체는 별도 언어 capability gap이며 현재 rung의 fallback으로
고치지 않는다.

2026-07-30 install-only intermediate direct-binding rerun은 이미 빌드된 gen2/parser
seed를 사용했고 former initializer diagnostic을 지나 계속 실행됐다. 그러나
4,605,377ms 뒤 unchanged 3,072MB pressure limit에서 wrapper가 전체 측정 tree를
중단했다. 완전한 요약은 peak working set 2,820.5MB, peak private 3,072.0MB,
top `gen2.exe` private 3,052.8MB, `limit_exceeded=true`, output capture complete다.
`driver.c`는 0바이트였고 `pgy-self-driver.exe` timestamp도 갱신되지 않았다.
따라서 Clone inference blocker는 제거됐지만 fresh install은 실패했고 launcher
parity도 실행할 수 없다. Final typed-value source는 current native compiler로
focused `driver_rung2.exe`를 생성하는 데 성공했다. 이어진 broader machine-layer
gate는 fresh projection probe JSON을 self-host MIR consumer가
`MIR machine-layer facts are missing or invalid`로 거부해 RED였다. 이는 source
compile success와 full install success를 구분하는 증거다. 이 결과를 “self-host라
무거운 정상 빌드”로 분류하거나 limit/timeout을 올려 닫지 않는다.

시간 측면에서는 timeout 상향으로 숨기면 안 되는 scaling RED도 확인됐다.
현재 5.1MB driver AST의 단일 `gen2` leg만 약 45분을 소비했고, 30분 codegen CI와
seed 이후 더 많은 작업이 남는 60분 full CI는 구조상 green이 될 수 없다.
`GenerateCUnitFromSemanticArtifact`가
`SemanticAstArtifactAnalyzeCompactBridge`로 analysis를 만든 뒤에도
`GenerateCUnitFromSemanticFacts`가 `SemanticAstArtifactAnalysisMatches`를 다시
호출한다. 이 match는 constructor/local/assignment/statement/enum/role/
expression/type/kind `*FactsMatchArtifact`를 통해 fact families와 전체 expression
surface graph를 `*FromArtifact`로 재구성한다. 다음 최적화 rung의 owner는 admitted
semantic analysis receipt/identity다. Last consumer인 codegen emission은 그
receipt를 소비하고 fixed-size identity/shape만 확인해야 한다. Forbidden fallback은
emission 성공 경로의 whole-artifact fact 재구성, timeout만 늘리기, 또는 검사 생략이다.
Falsifier는 같은 committed AST에 대해 analysis construction count 1, emission
boundary reconstruction count 0을 stage counter로 증명하고, 2.9MB와 5.1MB input의
wall time/peak를 같은 pressure owner 아래 기록하는 것이다.

### semantic analysis를 만든 뒤 emission에서 전체 fact family를 다시 검증하는 경우

2026-07-30 위 3,072MB scaling RED의 실제 active path를 다시 추적했다.
`driver_rung2`의 MIR consumer만 본 것이 아니라 Pergyra-built codegen seed 자체의
경로가 핵심이었다.

```text
gen2.exe <large-ast>
  -> GenerateCUnitFromAstArtifact
  -> SemanticAstArtifactAnalyzeCompactBridge
  -> GenerateCUnitFromSemanticArtifact
  -> GenerateCUnitFromSemanticFacts
  -> SemanticAstArtifactAnalysisMatches
  -> eleven *FactsMatchArtifact / *FromArtifact families
```

즉 analysis constructor가 이미 만든 사실을 emission admission 명목으로 다시
구성했다. 단순히 검사를 삭제하면 같은 node count의 다른 artifact와 analysis를
교차 결합할 수 있으므로 해결책이 아니다. `node_count`만 receipt로 쓰는 것도 같은
이유로 불충분하다.

현재 owner 계약은 다음과 같다.

- `AstTreeArtifact` schema v4가 tree text, node count, parser expression-graph
  roots/arena를 producer에서 한 번 해시한 `identity_digest`를 소유한다.
- parser가 expression graph를 붙일 때 identity를 한 번 갱신하고, 그 graph에서
  결정적으로 파생되는 owner-kind/destructure arena binding은 같은 epoch를 보존한다.
- `SemanticAstArtifactVerdict`가 artifact identity와 entrypoint policy를 함께
  봉인한다.
- `SemanticAstArtifactAdmissionReady`는 봉인된 identity/count/policy만 O(1)로
  비교한다. 여기서 tree hash, arena readiness, graph readiness 또는 semantic fact
  reconstruction을 호출하면 안 된다.
- 이 digest는 외부 입력을 인증하는 보안 seal이 아니다. Admitted fast path는
  semantic owner가 같은 호출 epoch에서 만든 artifact/analysis와, admission 뒤
  변경하지 않는 fact-family array만 받는다. 임의 pair 또는 mutable pair를 받는
  public compatibility entry는 계속 deep match를 거친다. Exact caller allowlist가
  Ready/admitted/verified 경로에 새 우회 호출이 생기는 것을 막는다.
- direct codegen seed, source-to-C, admitted MIR-to-C는
  `GenerateCUnitFromReadySemanticFacts`로 간다. Raw semantic-analysis compatibility
  entrypoint만 한 번의 deep match를 유지한다. 사용되지 않던 checked C-facts
  wrapper는 삭제했다.
- body/assignment/statement rows는 Ready core 진입 전에
  `CodegenSemanticBodyTypeFactsFromBundleOrDie`가 한 번 검증한다. Emission core가
  그 행들을 다시 전체 순회하지 않는다.

Negative gate는 같은 node count의 다른 text뿐 아니라 같은 text/count에 다른
expression graph를 붙인 artifact도 거부한다. Static gate는 direct codegen entry의
analysis constructor가 정확히 한 번이고 admitted/verified/source pipeline이 Ready
core로만 가는지 확인하고 fast-path caller set을 exact allowlist로 고정한다.
Ready core와 fixed-size admission body에는
`SemanticAstArtifactAnalysisMatches`, `AstTreeArtifactReady`, hashing,
`*FactsMatchArtifact`, `*FactsFromArtifact`, `*RowsFromArtifact`가 모두 금지된다.

Focused `driver_rung2_main.pgy --emit-c`, executable
`CompilerDriverPipelineReady` probe, component contract와 semantic lifetime/Ready
ratchet은 통과했다. 이것은 구조 및 작은 실행 증거이지 3GiB scaling closure 자체는
아니다. 반드시 이 변경을 포함한 fresh Pergyra-built codegen을 만든 뒤 동일한
2.9MB/5.1MB AST, 3,072MB cap, pressure owner로 다시 측정한다. 예전 gen2 binary를
재사용한 결과는 수정 효과의 증거가 아니다. 메모리 상한이나 timeout만 올리거나,
output byte equality만 보고 반복 검증이 사라졌다고 주장하지 않는다.

2026-07-30 fresh native-seed build와 고정 입력 pressure 관찰은 다음과 같다.
모든 수치는 `scripts/measure_build_pressure.ps1`, `LimitMB=3072`,
`StopOnLimit`, root process tree 기준이다.

| 관찰 | 결과 | elapsed | peak working set | peak private |
| --- | --- | ---: | ---: | ---: |
| current `codegen/main.pgy` C build | PASS | 47,749ms | 1,138.2MB | 1,190.8MB |
| 2,864,634-byte codegen AST emit | PASS | 1,098,757ms | 890.5MB | 968.4MB |
| 5,106,665-byte driver AST emit | TIMEOUT 124 | 2,400,686ms | 1,436.1MB | 1,551.4MB |
| 2.9MB emitted C -> gen2 compile | PASS | 4,710ms | 244.5MB | 229.7MB |
| gen2 consumes the same 2.9MB AST | PASS | 1,059,367ms | 1,177.7MB | 1,357.3MB |

2.9MB 입력 SHA-256은
`33096A1F0223E955A8CBF6B412188E1E6795B6559A6B82645ACD33B9C80F6AC3`,
5.1MB 입력은
`97EEFA34159BE8AFEA8D15F44BF5F74FB57D5DD1D8C03ABF565AF4A14B8D5190`,
측정 codegen binary는
`B09D3C84B49B695D30355386B3332C044F5AD3A74EDCF062411DF3B79029E978`다.
컴파일된 gen2는
`FA5A380AE771714E6AC25169FA9FE76CF6C008350344D19215778FA0A9220FE4`다.
Seed와 gen2의 raw C 출력은 EOF 빈 줄 하나만 달랐고, 저장소의
`pgy_selfhost_compare_expected_text_artifact_file_with_owner` emitted-C
normalization/comparator는 PASS했다.
5.1MB는 3GiB를 넘지 않았으므로 memory RED는 재현되지 않았지만, timeout 전에
output을 완성하지 못했으므로 end-to-end PASS가 아니다. 제한을 올려 성공으로
재분류하지 않는다. 처음 두 absolute-path 시도는 `PGY_IO_ALLOW_ABSOLUTE`가 없는
self-host I/O 정책에 의해 즉시 거부됐으며 pressure 증거에서 제외했다.

### 5.1MB 입력이 메모리 상한보다 CPU 시간에서 먼저 멈추는 경우

2026-07-30의 5,106,665-byte driver AST 측정은 3,072MB 상한을 넘지
않았지만 2,400,686ms에 timeout 124로 끝났다. 따라서 이 관측을 메모리
회귀나 end-to-end 성공으로 분류하면 안 된다. pressure 곡선과 실제 호출
그래프를 함께 감사하면 CPU 비용은 두 구간으로 나뉜다.

첫 구간은
`SemanticAstBodyTypeBundleFromAnalysis` 안의 body semantic proof다. 이미
생산된 signature/local/iteration/assignment/statement facts를 각
`*FactsMatchArtifact`가 다시 만들거나 deep-match하고, 일부 match는 그
안에서 signature 또는 iteration 전체 검증을 다시 호출한다. emission core의
whole-artifact 재구성은 제거됐지만, 그 직전 body admission에는 같은 종류의
반복 proof가 남아 있다.

둘째 구간은 `GenerateCUnitFromReadySemanticFacts`의 definitions loop에서
`EmitFunctionSet -> EmitStmtList`로 이어지는 C emission이다. 현재 각 문장은
local, assignment, statement kind와 expression surface를 각각 선형 검색한다.
고정 fixture에는 nonempty AST row 110,971개, callable 4,094개, local
12,224개, assignment 6,958개와 최소 27,675개의 명백한 tracked statement가
있다. tracked statement의 local/assignment miss만 계산해도 node-id 비교
하한은 530,861,850회다. 현재 predicate 순서와 평균 statement 위치를
적용한 statement-index 비교는 약 35억 회로 추정된다. 이 수치는 runtime
counter가 아니라 현재 loop와 fixture census에서 계산한 작업량이므로,
최적화 성공 판정에는 아래 coarse stage 계측과 동일 fixture 재측정을 쓴다.

다음 active seam은 body semantic proof admission이다.

- owner: artifact identity를 봉인한 semantic analysis/admission owner;
- last consumer: `SemanticAstBodyTypeBundleFromAnalysisObserved` 입구;
- forbidden fallback: admitted body 경로의 `*FactsMatchArtifact`,
  `*FactsFromArtifact`, iteration/local/statement fact 재생성;
- negative gate: stale digest와 malformed parallel-row 길이를 body loop 전에
  거부하고, admitted body transitive call graph에서 deep-match/rebuild를
  금지하며 fast caller exact allowlist와 immutable-after-admission 계약을
  유지한다;
- falsifier: 같은 5,106,665-byte fixture, 3GiB 상한, 2,400초 상한과
  normalized C parity.

계측은 row별 로그가 아니라 `artifact`, `semantic-analysis`, body의
`base/iteration/refine/assignment/statement`, emission의
`definitions/assembly` 경계만 기록한다. 그 seam이 닫힌 뒤 별도 rung에서
이미 정렬되어 생산되는 owner-local `node_ids`에 lower-bound 조회를 적용한다.
local destructure duplicate의 첫 행 우선 규칙은 보존하고, generic cache나 AST
재스캔을 새 authority로 만들지 않는다.

### self-host CI가 통합 root에서는 통과하고 source selfcheck에서만 실패하는 경우

2026-07-30 GitHub run `30498129265`의 `self-host-parity-linux`는
`expr_semantic_call_type_owner.pgy`에서 `CodegenExpressionTypeFromGraph`를 찾지
못해 실패했다. 이 함수의 owner를 직접 import하면
`expr_semantic_type_owner -> expr_semantic_call_type_owner ->
expr_semantic_type_owner` 순환이 생기며, Pergyra module resolver는 이를
의도적으로 거부한다. 실제 결함은 순환 expression emission cluster를
`expr_semantic_graph_emit_owner.pgy` closure로 검사하는 completeness manifest에
`expr_semantic_call_type_owner.pgy` 행만 빠진 것이었다. 기존 closure mapping에
그 행을 추가하고 component contract가 mapping의 재누락을 거부하도록 했다.
역-import나 duplicate declaration은 해법이 아니다.

같은 run의 Linux/Windows/macOS build에는 두 개의 독립 계약 드리프트가 더
있었다.

- `selfhost.semantic_artifact_admission` registry 행은
  `SFSemanticAstArtifactAdmission / SOSemanticArtifact`를 선언했지만 Coq
  `SpineFact`, `SpineOwner`, `spine_authority` projection이 갱신되지 않았다.
  registry 상태 집계도 `ACTIVE=0`으로 낡아 있었다. projection과 집계를
  `ACTIVE=1`로 고치고 enforcement evidence는 주석이 아니라 실제
  unbounded-proof 진단과 `stale_same_count`/`stale_same_tree` falsifier에
  연결한다.
- machine-layer MIR projection probe는 강화된 소비자 계약 뒤에도 routine
  source identity `0`과 `abi_type_name:null`인 let rows를 생산했다. `0`은
  stable identity의 sentinel이므로 양수 `1`로 바꾸고, 두 let row에
  `DeviceSlot<Int>`와 `Int` ABI type facts를 producer에서 명시한다. 소비자에서
  추측하거나 machine-layer validation을 느슨하게 하면 안 된다.

검증은 새 repo-relative `PGY_SELFHOST_BUILD_DIR` 하나에서만 실행한다. 절대
build-dir은 `${path#$ROOT_DIR/}`의 MSYS 표기와 맞지 않아 self-host absolute-I/O
정책에 의해 거부될 수 있으며, 이 harness 실패를 제품 실패나 성공으로 세지
않는다. 유효한 unique run은 SoT authority edge, component contract와 전체
machine-layer MIR/AIR/C 경계를 모두 통과해야 한다.

### self-host closure의 간접 구성원이 source selfcheck에서만 빠지는 경우

2026-07-30 GitHub run `30501338487`의 `self-host-parity-linux`는 앞선
`expr_semantic_call_type_owner.pgy` 보정 뒤 25개 source target을 통과하고,
26번째 `expr_semantic_dynamic_ability_call_emit_owner.pgy`에서
`RewriteExprFromSemanticGraph`를 찾지 못해 실패했다. 마지막 명시적 green은
C/LLVM semantic parity 113 fixtures였고, 진단은 `undefined_function`이었다.

이 파일도 독립 모듈이 아니라 다음 재귀 emission closure의 구성원이다.

```text
expr_semantic_graph_emit_owner
  -> expr_semantic_call_emit_owner
  -> expr_semantic_dynamic_ability_call_emit_owner
  -> RewriteExprFromSemanticGraph (graph root 소유)
```

따라서 dynamic owner에서 graph root를 역-import하는 수정은 순환 import와
이중 선언을 만들며 해법이 아니다. `CompilerCompletenessSemanticCheckTarget`가
dynamic source의 검사 target을 `expr_semantic_graph_emit_owner.pgy`로 투영해야
한다. Component contract는 그 source-to-target 행의 존재와 dynamic-to-graph
역-import 금지를 함께 고정한다.

좁은 재검증에서는 현재 tree로 manifest를 다시 빌드해 정확한 행이 생성됐고,
graph target semantic checker가 exit 0, `Status: ok`, `Diagnostics: none`을
반환했다. 한 cluster 누락을 고칠 때는 실패한 함수의 파일을 무조건 import하지
말고, 실제 통합 root와 completeness owner의 target projection을 먼저 확인한다.

같은 날 다음 run `30502023063`은 이 target을 통과해 29번째
`expr_semantic_option_value_owner.pgy`까지 진행한 뒤
`SemanticExpressionGraphCallTargetKind`에서 다시 실패했다. 이 경우는 순환
closure가 아니었다. Option value owner가 call-target kind/name을 직접
소비하면서 실제 정의 owner인
`ast_expression_call_target_fact_owner.pgy`를 import하지 않았고, 상위 composite
graph의 통합 import가 누락을 가리고 있었다. 따라서 이 경계에는 completeness
redirect가 아니라 직접 import를 추가하고 component contract가 그 owner edge를
고정한다. `undefined_function`이라는 표면 진단이 같아도, 먼저 import graph의
순환 여부와 실제 정의 owner를 확인해 두 수정을 구분해야 한다.

같은 run의 macOS C-only job은 제품/계약 63단계 중 62개를 통과하고
`self_host_source_scan_owner_smoke.sh`의 owner-set evidence에서만 실패했다.
현재 `source_scan_owner.pgy`, `cursor_owner.pgy`, `text_scan_owner.pgy`의 내용
해시는 `2ECB092EA4E5C16B786CE8A6D732A5B958434C8AB748E9E7DB060C9745548DC5`인데
benchmark evidence가 이전 해시를 보유한 상태였다. 이 gate는 성능 수치를
새로 측정했다고 주장하는 곳이 아니라 측정 당시의 hot-scan owner 내용이
무언가 바뀌었음을 강제 노출하는 ratchet이다. 따라서 현재 세 파일을 동일한
CRLF 정규화/대문자 SHA-256 규칙으로 다시 계산한 owner-set 값만 갱신하고,
기존 elapsed/peak/fixture/parity 수치는 재측정 없이 바꾸지 않는다. 로컬 gate가
나머지 개별 owner hash와 evidence 관계까지 모두 통과해야 유효한 갱신이다.
이 검토에서 함께 바뀐 type-canonical owner의 현재 해시는
`E6BD4E6D10612CB019265AD7763DF7FC37BBF748A0F10C919D4EFF5D5D74D859`였다.
변경 내용은 기존 hot canonicalization의 할당형 `Trim`을 reuse owner로 옮기고
constructed-type/Unknown 판별을 추가한 것이므로 hash ratchet은 갱신하되,
기존 benchmark 숫자를 새 측정치라고 재라벨하지 않는다.

### linked runtime ABI consumer가 실제 owner보다 먼저 검색되는 경우

Resource lowering은 실제 `MIR_INST_RESOURCE_OP` owner의 primary runtime-call ABI
row를 unambiguous DEF/STMT consumer에도 링크한다. 따라서 “row가 있다” 또는
inventory에서 먼저 발견됐다는 사실만으로 owner를 선택하면 linked Release
consumer가 실제 Release instruction보다 먼저 반환될 수 있다. 반대로
`MIR_INST_RESOURCE_OP`만 owner로 허용하면 slot-sugar
`MIR_INST_DEF`/`MIR_INST_DESTRUCTURE`가 직접 소유하는 Claim + Read/Write auxiliary
rows를 잃는다.

현재 단일 predicate `mir_abi_resource_runtime_instruction_owns_rows`가 owner
provenance를 고정한다. Resource operation은 primary row를 직접 소유하고,
non-resource instruction은 DEF/DESTRUCTURE이면서 primary operation이 Claim이고
auxiliary owner set이 있을 때만 owner다. Link된 consumer는 primary row만
복사받으므로 owner가 아니다. Validator도 같은 predicate를 사용해 linked
consumer에 forged auxiliary row를 붙이는 입력을 fail closed한다.

Falsifier는 두 방향을 함께 유지해야 한다.

- linked Release consumer가 inventory에서 먼저 와도 실제 resource owner 선택;
- slot-sugar DEF의 concrete Write row는 계속 검색 가능;
- linked consumer에 valid-looking auxiliary row를 붙여도 lookup owner가 되지
  못하고 MIR validation이 owner-provenance diagnostic으로 실패.

이를 `test_mir_runtime_call_abi.cases.h`가 소유한다. Backend에서 layout 검사를
완화하거나 global ABI table로 fallback하는 것은 해결책이 아니다.

### 분리한 C owner는 컴파일되지만 축약 테스트 링크에서만 undefined reference가 나는 경우

2026-07-29 GitHub run `30454762165`의 Linux/Windows/macOS C-only job은
`mir_lower_request_init`, `mir_lower_request_bind_dir`,
`mir_validate_decl_method_metadata`를 찾지 못해 `test_mir` 링크에서 실패했다. 함수
구현과 header 선언은 존재했고 production object inventory에도 source가 있었으므로
구현 결함이 아니라 `MIR_CORE_OBJECTS`의 축약 링크 인벤토리 누락이었다. 같은 run의
Linux region unit은 `ast_block_match_event_accessors.c`를 직접 링크하면서 그 파일이
소비하는 `ast_with_body`와 `ast_select_case*`의 실제 owner
`ast_async_lambda_accessors.c`를 생략했다.

수정은 consumer에 stub을 넣거나 함수를 다시 정의하지 않는다. 실제 owner 객체
`mir_lower_request.o`, `mir_decl_header_method_validate.o`를 `MIR_CORE_OBJECTS`에 넣고,
region unit의 prerequisite와 link input 양쪽에
`ast_async_lambda_accessors.c`를 넣는다. `build_source_inventory_smoke.sh`는 이 owner를
다시 빼는 축약 링크가 재도입되지 않도록 고정한다. 로컬 `make -j2 test-mir`에서
MIR `157 passed, 0 failed`와 후속 topology/type/speculation gate가 통과했다.

같은 CI의 `doc_link_checker` mismatch는 checker 구현 차이가 아니라
`docs/INDEX.md`가 늘어난 뒤 expected census가 `164/159`에 멈춘 문제였다. 실제 clean
census `173 total / 168 Markdown / 0 missing`과 같은 입력의 synthetic dead-link
`168 missing`을 expected artifact에 반영한 뒤 C/LLVM artifact parity와 negative
fixture가 통과했다. Golden 갱신은 현재 입력 owner를 다시 실행한 관측값과 음성
fixture가 함께 맞을 때만 허용하며, 단순 byte-equal 맞추기로 실패를 숨기지 않는다.

### body 의미분석이 이미 sealed된 사실을 단계마다 다시 검증하는 경우

5.1MB bootstrap AST에서 3GB 이상으로 커졌던 핵심 결함은 “의미분석이 원래
무겁다”가 아니라, 하나의 `SemanticAstArtifactAnalysis`가 이미 소유한 enum,
function-scope, signature/local/assignment/statement 및 expression-graph 사실을 body
8단계가 다시 match하거나 재구축한 것이었다. 특히 match-visible 환경 seeder가
사용 지점마다 enum과 function-scope inventory를 다시 만들었다. 한 번의 producer
proof면 되는 일을 stage와 use-site마다 반복한 것이다.

현재 경계는 다음처럼 닫는다.

- `SemanticAstArtifactAnalysis`가 `function_scopes`를 한 번 생산해 소유한다.
- `ast_body_analysis_admission_owner.pgy`가 artifact identity와 producer row의
  parallel shape를 body materialization 전에 한 번만 검사한다.
- Reconstruction-free `ast_body_analysis_shape_owner.pgy`가 signature
  generic/parameter span, role method/impl/ability span, intent step/terminal
  span, constructor/enum artifact epoch과 nested type-expression row 범위를
  같은 receipt에서 검증한다. `ok` flag만 믿고 growable parallel row를 직접
  인덱싱하지 않는다.
- initializer, iteration, call-target, refinement, expression-place, assignment,
  statement, generic stage는 각각 admitted core를 호출한다.
- production caller는 codegen admitted entry, source-to-C pipeline, rung2 세 곳만
  admitted body API를 사용한다. 임의/mutable pair용 checked API는 deep proof를 한
  번 수행한 뒤 같은 admitted core로 내려간다.
- stale same-shape identity와 깨진 local parallel row는
  `base-initializer:start` 전에 `body_analysis_admission`으로 실패하고 모든 파생 row가
  비어 있어야 한다.
- 외부 raw `SemanticAstArtifactAnalysis`를 받는 rung2 API는 먼저
  `SemanticAstArtifactAnalysisMatches` deep proof를 수행한다. Fresh analyzer를 바로
  소비하는 production call edge만 이름이 명시된 admitted verifier/projection을
  사용한다. 같은 길이의 local name을 변조한 fixture는 shape admission은 통과하지만
  raw driver boundary에서 `body-types:start` 전에 거부된다.
- 정적 ratchet은 admitted core 안의 `SemanticAstArtifactAnalysisMatches`,
  `*FactsMatchArtifact`, 이미 운반된 plural `*FactsFromArtifact`, surface borrow 및
  full graph readiness 재도입을 거부한다.

이 변경 뒤 fresh semantic checker C build는 `0 errors, 0 warnings`, 수정된 semantic
owner와 negative contract, generic/initializer probe, production admitted entry 및
source-to-C pipeline self-check가 통과했다. Rung2 전체 owner check는 180초 제한 안에
끝나지 않았으므로 PASS로 기록하지 않는다. 5.1MB fixture의 새 시간/peak/parity도
아직 재측정 전이며, 기존 timeout 수치를 개선 결과로 재라벨하지 않는다.

후속 raw-boundary 보강 뒤
`tests/self_hosted/parity/driver_rung2_analysis_admission_owner.sh`는 fresh/checked
호출 방향의 정적 ratchet, mutable-analysis C probe build, materialization 전 exact
diagnostic을 실행해 PASS했다. 이 gate는 exhaustive self-host parity target에도
결속했다. Signature/role/intent/constructor/enum malformed shape negative는 별도
body admission contract가 소유한다.

### admitted body가 생성자 행을 표현식마다 다시 검증하는 경우

2026-07-30 exact 5,106,665-byte fixture를 동일한 3,072MiB/2,400초 상한으로
sampling하자 emission 진입 전 첫 체류가 다음 call stack으로 두 번 재현됐다.

```text
SemanticAstBodyTypeBundleFromAdmittedAnalysisObserved
-> SemanticAstAnalysisResolveCallTargetsFromAdmittedBody
-> SemanticAstBodyExpressionEnvironmentSeed
-> SemanticAstExpressionSeedOwnerFields
-> SemanticAstNominalConstructorRowsReady
```

이 fixture에는 nominal 311개와 field row 2,280개가 있다. 현재
`SemanticAstNominalConstructorRowsReady`의 중첩 uniqueness proof는 호출 한 번에
field identity 비교 2,598,060회를 수행한다. admission receipt가 이미 같은 producer
epoch과 parallel shape를 보증하는데 이 checked seeder를 expression surface마다
호출한 것이 CPU RED의 첫 동적 원인이었다. 메모리는 약 374MiB private에 머물렀으므로
3GiB 재발로 분류하지 않는다.

call-target 한 곳만 admitted seeder로 내린 재실행은 call-target을 통과했지만
77초와 145초 표본 모두 expression-place의 같은 `RowsReady`에서 체류했다. 이 run은
원인이 반복 재현된 뒤 exact PID와 child만 157,212ms에 종료했으며, peak private
374.0MiB인 **sampled RED**다. 완료나 timeout PASS가 아니다.

수정 원칙은 다음과 같다.

- body admission receipt를 받는 production core만
  `SemanticAstExpressionSeedOwnerFieldsFromAdmittedConstructors`를 사용한다.
- arbitrary pair를 받는 public wrapper와 standalone contract는 기존 checked seeder와
  `SemanticAstNominalConstructorRowsReady`를 유지한다.
- admitted body transitive family에 checked seeder 호출이 하나라도 재도입되면
  lifetime/component negative gate가 실패한다.
- cache나 두 번째 constructor registry를 만들지 않는다. 기존 owner가 한 번 증명한
  사실을 같은 epoch의 마지막 consumer까지 운반한다.

여섯 body consumer를 모두 admitted seeder로 내린 r2 exact run은 87,896ms 표본에서
body 단계를 전부 통과하고 emission의 unsupported-builtin text scan에 도달했다. 그
표본은 일시적이었다. 199.7초와 310.1초 표본은 모두 다음 경로에 머물렀다.

```text
GenerateCUnitFromReadySemanticFacts
-> EmitFunctionSet
-> EmitFunctionWithSpecialization
-> CodegenCallableReceiverCarriageForSignatureOrDie
-> CodegenCallableReceiverFactsReady
```

receiver fact는 약 4,094개 signature와 parallel row identity를 검사하고 prior-row
uniqueness까지 확인한다. 이를 함수/prototype/generic loop의 row accessor마다 다시
호출하면 producer proof 하나가 emission에서 반복되는 같은 결함이 된다. r2는 세
표본 뒤 exact PID와 child만 335,387ms에 종료했고 peak private 1,253.3MiB인
**sampled RED**다. 메모리 상한 실패나 완료 PASS가 아니다.

이 경계는 admitted receiver producer가 full identity/uniqueness `Ready`를 정확히
한 번 수행한 뒤 receipt bit를 seal하게 한다. Ready emission은 `ok`, `admitted`, row
bounds만 검사하는 O(1) accessor를 사용한다. 임의 facts용 accessor,
`FromAdmittedRowsOrDie`, environment-row 외부 경계는 full `Ready`를 유지한다.
Negative gate는 producer full proof 1회, `function_emit.pgy` 안의 old accessor/full
proof 0회를 고정한다.

최종 판정은 이 교정이 들어간 새 codegen binary로 동일 fixture를 끝까지 실행하고
normalized emitted-C parity를 관측한 뒤에만 갱신한다.

### focused gate는 통과하지만 cross-platform CI의 생성물·owner 상한이 깨지는 경우

GitHub run `30510118949`의 Windows와 macOS C-only job은 같은 두 SoT drift를
보고했다.

- `language_word_implementation_inventory.generated.md`가 canonical 146-row keyword
  registry에서 다시 계산한 source-use count와 달랐다.
- semantic owner hard cap 599줄을 artifact verdict 600, generic specialization 641,
  initializer type 637, statement type 622줄이 넘었다.

키워드 실패는 문서 숫자를 수동으로 고치지 않는다.
`render_language_keyword_registry.py --write`로 registry, self-host projection,
TextMate grammar와 implementation inventory의 한 projection chain을 재생성한다.
그 뒤 `tests/language_keyword_registry_smoke.sh`가 146 rows, 70 reserved rows,
76 parser selectors와 fixture를 함께 검증해야 한다.

줄 상한도 CI 숫자를 올려 숨기지 않는다. Production/admitted fact 생성은 기존
owner에 남기고 read-only query와 readiness/match projection만 책임별 query owner로
옮겼다.

```text
initializer: fact 548 + query 80
generic specialization: fact 551 + query 92
statement type: fact 525 + query 150
artifact verdict: fact 534 + executable contract 70
```

모든 direct consumer는 새 query owner를 명시적으로 import한다. Component gate는
old fact owner에 query 함수가 돌아오는 것을 거부하고 각 새 owner의 exact cap을
고정한다. `tests/test_inc_size_smoke.sh`, 관련 self-host semantic checks와 component
contract를 함께 관측해야 이 CI 결함을 닫았다고 기록한다.

### self-checker가 중첩 typed fact 필드를 undefined symbol로 보는 경우

GitHub run `30502868266`의 exhaustive parity는 129/661
`canonical_mir_identity_epoch_owner.pgy`에서 `topology_row_indices`를 undefined local로
진단했다. 필드는 실제 `MirDomainProjectionAssignmentFacts`에 존재했으므로 단순 필드
누락이 아니었다. completeness 경계에서
`runtime_assignments.projection_members.topology_row_indices`처럼 두 단계 이상 내려간
field chain의 중간 타입이 정확히 보존되지 않은 것이 원인이었다.

정의 owner를 direct import하는 것만으로는 충분하지 않았다. 반대로 nested aggregate를
새 typed local에 복사하면 native compiler가 borrowed `ref` provenance의 탈출로 정확히
거부한다. 최종 경계는 canonical consumer의 기존
`ref MirDomainRuntimeAssignmentFacts` 수명을 보존하고, nested projection row 접근만
정의 owner의 `MirDomainRuntimeProjection*` accessor로 내린다. Accessor는 exact topology
row, explicit-map flag, segment bounds/name과 target field를 typed receipt에서 읽으며 이름
join이나 두 번째 topology SoT를 만들지 않는다. 두 owner의 self-host semantic checker는
이 형태에서 `Status: ok`가 되었다. Native focused execution gate도 최종적으로
`[self-host-parity:canonical-identity-epoch] ... ok`와 exit 0을 관측했다. 이 gate는
hosted-method tree ID, apply/link epoch remap, stale raw ID와 wrong-kind canonical ID
negative를 모두 실행한다. Checker PASS와 executable PASS를 별도 증거로 기록한다.

이 과정에서 focused gate의 positive fixture도 별도 stale epoch 결함을 드러냈다. Fixture는
declaration field와 topology ID에만 offset epoch를 적용하고, 같은 identity epoch에 묶인
`domain_runtime_assignments`의 participant owner/field 및 projection
owner/directive/slot/target/path-segment ID는 옛 값으로 남겼다. 따라서 canonicalizer에
도달하기 전에 machine admission이 `MIR machine-layer facts are missing or invalid`로
정확히 거부했다. Positive fixture는 이 runtime assignment ID들도 같은 mapping으로
원자적으로 옮겨야 한다. Validator를 느슨하게 하거나 stale runtime assignment를
허용해서 gate를 통과시키면 안 된다. 첫 fixture 보정 뒤에는 두 번째 stale seam도
드러났다. Canonical carrier가 declaration/topology만 canonical output으로 바꾸고
`domain_runtime_assignments`를 raw epoch에 남겨 machine admission에서 거부된 것이다.
최종 carrier는 canonicalizer가 방금 생산한 runtime-assignment receipt까지 같은 epoch로
복사한다. 즉 test도 declaration, topology, runtime assignment를 하나의 identity epoch로
취급한다.

같은 run에서 macOS/Linux/Windows가 공통으로 실패한 원인은 별개인 stale source-shape
gate였다. Driver pipeline은 body bundle을 이미 만들고
`GenerateCUnitFromReadySemanticFacts`를 호출하는데 compiler-world contract가 삭제된
중간 reanalysis 호출 `GenerateCFromVerifiedSemanticArtifact`를 계속 요구했다. Gate는
현재 Ready-fact call을 요구하고 옛 checked call의 재도입을 금지해야 한다. 구현을
구식 gate에 맞춰 다시 reanalysis 경로로 되돌리는 것은 해결책이 아니다. 갱신된
`tests/self_host_compiler_world_contract_smoke.sh`는 topology와 compiler-world source
shape를 모두 실행해 exit 0을 관측했다.

### 20GB 증상과 최신 5.1/5.3MB 부트스트랩 메모리 결과

과거 3GB 이상 및 사용자 환경의 20GB 증상은 큰 AST 자체가 정상적으로 요구하는
메모리가 아니었다. 이미 admission된 그래프/constructor/body owner proof를 여러 소비자가
다시 열고, 그 안에서 전역 직렬화의 길이와 행을 매 lookup마다 재계산한 것이 핵심
결함이었다. 특히 311개 nominal, 2,280개 field의 constructor readiness 한 번이
2,598,060번의 field identity 비교를 수행했으며, 이것이 식별자/표현식마다 반복됐다.

수정 뒤 동일한 3,072MiB/2,400초 pressure policy에서 완전한 codegen을 관측했다.

- 고정 5,106,665-byte AST(r8): 158.020초, peak private 1,659.1MB,
  working set 1,570.9MB, exit 0.
- fresh parser의 5,326,689-byte AST(r9): 164.252초, peak private 1,742.1MB,
  working set 1,648.5MB, exit 0.
- exact zone authority/where/slot 검증을 포함한 동일 AST(r10d): 145.719초,
  peak private 1,759.6MB, working set 1,666.9MB, exit 0. 생성 C는 r9와
  byte-identical이다.
- 현재 소스를 다시 parse하고 현재 codegen을 다시 build한 r11: 5,324,488-byte
  AST, 164.133초, peak private 1,726.6MB, working set 1,635.7MB, exit 0.
  생성 C는 5,351,899 bytes이며 CR 제거 후 5,256,386 bytes다. Codegen 자체의
  fresh build도 52.295초, peak private 1,222.8MB에서 끝났다.

따라서 현재 소스로 20GB는 재현되지 않으며 “오라클 프로젝트라서 원래 그 정도가
필요하다”는 설명은 틀리다. 다만 이 수치는 셀프호스트 codegen 프로세스의 실제 peak이고
여전히 최적화 대상이다. build leg도 약 1,213MB peak private에서 끝났으므로 build와 exact
codegen을 합쳐 20GB 정상치로 해석하면 안 된다.

같은-epoch `CodegenTypeGlobalIndex`를 처음 넣었을 때도 CPU가 비정상적으로 커졌다. 원인은
index construction의 `HashRow`/`RowRangeEquals`가 각 문자 접근마다
`StringLength(rows)`를 다시 수행한 것이다. producer가 한 번 계산한 `rows_length`를 두
함수에 넘기고, `CodegenCharAt` 대신 길이가 봉인된 byte-range 비교를 사용해야 한다.
Component ratchet은 lookup/contains/hash/range에서 `StringLength(rows)` 재도입과 builder의
`CodegenCharAt(rows, ...)` 재도입을 거부한다.

### exact codegen의 `*Zone_sync` 누락과 typed runtime receipt

r10d/r11의 exact C emission은 성공했지만 host compiler가 15개의 `*Zone_sync`
declaration/body 누락으로 실패했다. 원인은 호출된 15개 이름이 아니라, 모든 zone
declaration을 typed runtime fact에서 저장·sync 정의로 투영하는 self-host owner가 없었던
것이다. compiler world 18개와 support zone 2개를 합친 실제 program graph zone은
20개다.

수정 후 zone-sync 9c 측정은 다음과 같다.

- fresh codegen build: 71.756초, peak private 911.2MB, working set 865.2MB;
- 5,324,488-byte exact AST emission: 123.632초, peak private 1,838.6MB,
  working set 1,733.1MB, output 5,368,419 bytes;
- AST zone identity 20개와 C `static void *Zone_sync` definition 20개의 exact
  sorted bijection;
- host GCC compile 성공; 생성 executable이 예상된 driver argument 오류까지 진입;
- zero-topology fixture에서 sync 2회 후 atomic generation `0 -> 2`, object와
  `tobject`의 ready/dirty/epoch/cause tuple 불변;
- `PGY_ZONE_THREADSAFE`도 fixture가 명시적으로 init/destroy를 소유하는 harness에서
  실행 PASS. 언어가 생성한 zone constructor/destructor의 lifecycle owner는 아직
  OPEN이다.

최종 owner/policy 통합 뒤 current-source codegen도 동일 AST를 136.249초에 다시
내렸고 raw UTF-8 C 5,368,053 bytes, 20/20 bijection, host GCC compile을 재확인했다.
이 repeat는 pressure sampling 없이 실행했으므로 peak memory에는 9c 측정값을 사용한다.

해법은 빈 stub이나 이름 열거가 아니다. Zone 구조체는 공용
`pgy_runtime_zone_sync_abi.h`에서 lock/generation ABI를 소비하고, self-host emitter는
declaration inventory와 admitted DIR/MIR topology 및
`semantic.domain_runtime_assignment`를 소비한다. Semantic-artifact fast path는 현재
증명 가능한 zero topology만 직접 받으며, nonzero topology 또는 projection map을 만나면
partial C 전에 fail closed한다. 반대로 기존 direct-MIR 경로의 admitted nonzero topology는
그 plan을 계속 실행해야 한다. Zero-only receipt를 direct-MIR consumer까지 퍼뜨리면
`domain_runtime_assignment_execution_owner.sh`가 회귀하므로 두 경계의 evidence lifetime을
섞지 않는다.

Exact host compile 성공은 `REACHABLE` 증거다. Fresh installed driver가 실제 production
entrypoint를 대체하고 이전 C-owned 경로를 삭제하기 전에는 hard `SUBSTITUTING`으로
기록하지 않는다. 다음 검증은 fresh installed-driver build, launcher parity, live
typed-intent execution/compensation 순서다. 기존 `bin/pgy-self-driver.exe`는 stale일 수
있으므로 현재 source/typed contract의 증거로 재사용하지 않는다.

측정 실행 파일에 AST를 전달할 때 repository I/O policy도 보존해야 한다. `FileExists`는
기본적으로 repository root 안의 상대 경로를 받으며, 별도 허용 없이 Windows 절대 경로를
전달하면 실제 파일이 있어도 `AST file not found`로 fail closed한다. 이 즉시 실패를
codegen 또는 메모리 결함으로 분류하지 말고, 측정 프로세스의 working directory를 repo
root로 고정한 뒤 `.tmp/...` 상대 경로를 사용한다. 절대 경로 허용 정책을 측정 편의를 위해
넓히지 않는다.

별도의 leaf-place contract checker는 10분 이상 CPU를 사용하고 출력 없이 끝나지 않아
중단했다. 이는 PASS가 아니다. 같은 contract가 포함된 graph semantic checker와 component
gate의 PASS만 관측 증거로 사용하고, standalone 장시간 실행은 별도 성능 결함으로 남긴다.

Intent step의 공개 gate는
`tests/self_hosted/parity/intent_step_binding_contract_parity.sh`로 별도 고정하고,
내부 owner runner를 source한다. Tool scaffold는 `intent.md`를 소유해야 하며 둘 중 하나가
없으면 build-source inventory에서 실패한다. 이 실행 gate는 actor와 authority가 다른 정상 사례, by-value/inout zone
주소 모드, `where`/`using` 불일치, 선언되지 않은 authority, 동일 타입 subject slot의
모호성, slot 누락을 포함한다. 단순 문자열 ratchet만으로 이 의미를 검증했다고 기록하면
안 된다.

### real-source semantic selfcheck가 작은 root에서 60초 timeout인 경우

`compiler_world_direct_mir_owner.pgy`는 61줄이지만 import signature bundle은 약
760KB다. 2026-07-30 CI에서는 이 target이 60초를 넘었고, 로컬 동일 checker도
86.383초 뒤까지 끝나지 않았다. 원인은 root 크기가 아니라 source-bundle/parser scan이
이미 알고 있는 전체 문자열 길이를 `SkipWhitespaceAndComments`, keyword match, identifier
read마다 다시 `StringLength`로 계산하고, import stub을 누적 `Concat`으로 조립한 것이었다.

길이를 owner boundary에서 한 번 봉인하고 `*Within` scanner에 전달하며, bundle은
`TextBuilder`로 한 번만 완성해야 한다. 또한 빨라진 checker가 처음 드러낸 `zone/world`
constructor 누락을 시간 제한 완화로 가리면 안 된다. Nominal constructor owner는
`zone`/`world`의 `zone`, `subject slot`, `object slot`, `tobject slot` 필드를 정확히 읽고
중첩 action/function body를 field로 오인하지 않아야 한다. 수정 후 동일 Windows target은
source bundle 2.978초, semantic check 2.681초, `Status: ok`를 관측했다.

CI의 exhaustive real-source selfcheck는 큰 root timeout을 닫은 뒤 더 뒤의 독립된
standalone-owner 결함을 드러낼 수 있다. 2026-07-30 run `30524796373`은 155/677의
`direct_mir_llvm_text_format_owner.pgy`에서 `undefined_function: Die`로 멈췄다. 이
owner는 큰 emitter import graph에서는 우연히 `Die`를 볼 수 있었지만 standalone
source root로는 그 소유자를 import하지 않았다. 해결은 builtin 목록에 `Die`를 넣거나
checker를 느슨하게 하는 것이 아니라 `../codegen/text/text_owner.pgy`를 direct import하고
component ratchet으로 그 edge를 고정하는 것이다.

같은 run에서 함께 드러난 독립 결함도 한 원인으로 합치지 않는다. Bash 3.2 hosted
runner는 `case` pattern의 줄 연속을 안전하게 해석하지 못했으므로 allowlist pattern을 한
줄로 고정했다. 새 self-host tool은 executable owner만 추가해서 끝나지 않고 tool-local
`intent.md`와 public parity wrapper를 함께 inventory에 넣어야 한다. Expression surface
contract의 canonical root는 `Call`이 아니라 `CallArgument`이며, 이전 이름을 expected
fixture로 유지하면 의미 회귀가 아니라 stale oracle failure가 된다. 이 네 문제는 각각
component contract, build-source inventory, intent-step executable gate, MIR machine-layer
gate로 분리해 재발을 막는다.

## Full source-to-MIR completes computation but dies while publishing JSON

2026-07-31 fixed-input evidence separates three costs that were previously
reported as one heavy self-host build.

- Native pgy translated driver_bootstrap_main.pgy to release C in 399.2 s.
- GCC compiled that generated unit with -O3 -fwrapv -fno-strict-aliasing in
  75.9 s.
- The pre-fix release executable completed every routine, all intents,
  canonical IDs, and JSON construction, but the pressure owner stopped it at
  3.098 GiB private while publishing an approximately 86 MB payload.
- The updated artifact action streamed verified SelfMirProgramFacts through
  SelfMirProgramJsonWriteArtifactVerified. The same target exited 0 in
  83.364 s at 1.525 GiB peak private and 1.404 GiB working set.

The cause was dual materialization at the final boundary, not LLVM and not the
number of tests. Artifact mode first built one whole SelfMirProgramJson string
and then passed it to SelfMirArtifactCommitPayload. The transaction write
temporarily retained the complete MIR graph plus the complete payload and its
publication state. Stdout mode still has a real payload boundary; file mode
does not.

The earlier scaling defect was separate but cumulative: every instruction row
stored or compared the full semantic expression graph. SelfMirProgramFacts now
owns that graph once and instruction rows carry only root and bounded-range
handles. Reintroducing a graph field, whole-graph equality, or routine-local
program-graph snapshots is a regression even when byte output matches.

Measurement policy:

- Execute one semantic program target once per stage or run.
- Read peak_private_gib and attention_required only from the final pressure
  summary. Do not tail or optimize against every live sample.
- Keep the 3 GiB hard stop and 2.4 GiB attention threshold fixed.
- A run below attention is not a memory optimization owner.
- Do not classify test matrices or gen2/gen3 fixed-point duration as ordinary
  compile latency.

Build profile policy:

- release is the default self-host emitted-C profile and uses
  -O3 -fwrapv -fno-strict-aliasing.
- PGY_SELFHOST_CC_PROFILE=test is an explicit debugging profile and uses -O0
  with the same semantic flags.
- The O0 Windows build currently exposes a distinct stack defect: nested
  ApplyPostfixFact lowering reaches routine 397 and overflows through generated
  lowering frames measured in tens of KiB. Raising the process stack is not a
  fix, and the O0 failure must not be used as the release performance number.

Current evidence artifacts are under .tmp/source_mir_stream. The authoritative
summary is pressure/source-mir-stream-release.summary.json. Temporary artifacts
are evidence only and are not semantic owners.

## Prototype streaming 뒤에도 3 GiB를 넘으면 raw MIR lifetime을 확인한다

2026-08-01의 current-driver fixed-point에서 처음 관찰된 마지막 marker는
`[codegen-pressure-stage] definitions:done:4267`이었고 old installed driver는
private 3.071 GiB에서 중단됐다. 이 단계의 source 변경은 old driver가 자기
codegen을 끝낸 뒤에야 실행될 수 있으므로, current source 자체를 검증하려면
호환되는 Pergyra parser/codegen seed로 bridge driver를 먼저 만들어야 한다.
Windows에서는 WSL alias `bash.exe`가 아니라 실제 MSYS2 UCRT64 bash와 PATH를
사용한다.

`CollectProtos`의 `Array<String>` 전수 보존을 책임명
`function_prototype_block_owner.pgy`의 한 `TextBuilder` streaming으로 바꾸자
동일 compiler-scale MIR가 모든 emission stage를 끝냈다. 그러나 sampled peak는
여전히 3.077 GiB였고 prototype 단계 자체는 약 216 ms뿐이었다. 따라서
"prototype 배열 하나만 없애면 끝"이라는 최초 가설은 불완전했다.

다음 수명 겹침은 약 91 MiB의 file-backed raw MIR JSON이었다. Routine/use/match/
ABI/codegen view owner가 필요한 typed facts를 모두 만든 뒤에도
`MirMachineLayerAdmittedJsonInput`이 raw buffer를 C emission 끝까지 보존했다.
파일 입력 경로는 machine declaration과 topology receipt/plan을 snapshot한 뒤
`MirJsonOwnedInputRelease`로 그 owned buffer를 retire한다. Borrowed text API는
caller가 소유한 입력이므로 같은 release를 실행하지 않는다.

관찰 결과:

| 단계 | 시간 | peak private | 판정 |
| --- | ---: | ---: | --- |
| compatible Pergyra bridge build | 112.658초 | 2.236 GiB | PASS |
| current source -> MIR | 49.516초 | 2.059 GiB | PASS |
| sealed MIR -> gen2 C | 103.733초 | 2.844 GiB | PASS, attention |
| gen2 host release C compile | 54.138초 | 0.758 GiB | PASS |
| same MIR -> gen3 C | 106.105초 | 2.852 GiB | PASS, attention |

Gen2와 gen3는 모두 5,661,265 bytes이고 SHA-256
`B30B28CE978582764B168B1238C5EB5D2CF2AA6CDB8EB25FB0AF346C01ADB4FF`로
byte-equal이다. 이 결과는 hard cap을 높이거나 cache/shard/worker를 추가한 것이
아니다. 같은 semantic target을 한 번씩 실행하고 final summary만 읽었으며,
3 GiB hard stop과 2.4 GiB attention threshold는 그대로다.

재발 방지 규칙:

- prototype 전체를 parallel array로 다시 보존하지 않는다.
- JSON-indexed consumer가 끝난 뒤 file-backed raw MIR을 emission까지 연장하지
  않는다.
- borrowed input과 owned file buffer의 release 권한을 합치지 않는다.
- host C compiler의 0.758 GiB와 self-host MIR consumer의 2.8 GiB를 같은 build
  비용으로 보고하지 않는다.
- attention 초과는 기록하되 hard cap 미만이라는 이유만으로 새 active rung을
  중단하거나 매 sample에 맞춰 구조를 바꾸지 않는다.

## 누적 expression graph를 매 row마다 다시 검증해 3 GiB에 도달하는 경우

2026-08-01의 90,304,012-byte 고정 MIR consumer에서 메모리 hard stop의 직접 원인은
graph 크기 자체가 아니었다. `SemanticExpressionGraphArenaFromRows`가 새 row를
추가할 때마다 지금까지의 모든 node에서 `call_return_type_names`를 다시 만들었고,
target projection도 각 occurrence마다 전체 누적 arena의
`SemanticExpressionGraphArenaReady`를 다시 실행했다.

수정 전 r54는 graph row 4,096/8,192/12,288을 각각 88.353/169.139/277.636초에
통과한 뒤 311.431초에 private 3.009 GiB hard stop에 도달했다. 수정 후 r55는
900초 timeout 동안 row 28,672까지 진행했으며 peak private 0.965 GiB,
working set 0.904 GiB였다. 약 68%의 private 감소는 반복 누적 재구성 결함이
닫혔다는 증거지만, timeout run은 full consumer PASS가 아니다.

해법은 cache나 메모리 한도 상향이 아니다.

- sequence/parser bridge는 이전 `call_return_type_names` vector를 이어받고
  새 node의 빈 fact 한 개만 append한다.
- target projection은 이미 입증된 local identity만 검사한다.
- 최종 `expression_graph_fact_owner.pgy`가 완성된 arena를 정확히 한 번 Ready한다.
- append에서 `ArenaFromRows`, target projection에서 `ArenaReady`를 다시 쓰면
  component negative ratchet이 실패한다.

r56은 같은 target을 더 오래 기다리며 row 40,960까지 도달했지만 약 1,131초에
의도적으로 중단했다. 이것을 green이나 bootstrap 진척으로 기록하지 않는다.
동일 미완주 실행의 timeout만 늘리는 것은 코드 진행이 아니다. reached owner를
수정한 뒤에만 semantic target을 다시 한 번 실행한다.

남은 bounded JSON 비용도 같은 원칙을 따른다. 기존 `Substring`은 구간이 작아도
전체 source에 `strlen`을 실행하고, 문자별 `CharAtN` 조립은 byte마다 heap string을
만든다. `SubstringWithLen(source, source_len, start, len)`은 이미 봉인된 길이를
소비해 구간을 한 번만 복사한다. Unescaped JSON string과 검증된 number token은 이
primitive를 사용하고, escape가 실제 존재할 때만 decode chunk 경로를 쓴다.
C/LLVM string-window parity와 bounded JSON exact-bound parity가 이 경계를 검증한다.

## builtin row 추가 후 bootstrap readiness가 실패하는 경우

2026-08-01의 fresh bounded bootstrap은 새 Pergyra-built gen2와 self parser를
정상 생성한 뒤 production sample 실행에서 다음처럼 fail-closed했다.

```text
CODEGEN ERROR: driver rung-2 semantic facts are not ready: builtin_signature
```

원인은 builtin 구현이나 backend symbol이 아니었다. `SemanticBuiltinSignatureRows`
에는 `SubstringWithLen` row가 추가됐지만 같은 owner의 readiness가 base row 수를
숫자 `124`로 다시 소유하고 있었다. Seed와 registry-prefix parity가 이미 정확한
row projection을 검증하므로 이 숫자는 안전 ratchet이 아니라 stale 이중 SoT였다.

해결 규칙은 다음과 같다.

- builtin population과 순서는 `SemanticBuiltinSignatureRows`만 소유한다.
- readiness는 owner에서 seed한 names/returns/params와 registry projection을
  비교하며, 별도 숫자 row count를 두지 않는다.
- `builtin_signature_registry_owner_parity.sh`는 numeric count mirror를 거부하고,
  `SubstringWithLen` row가 정확히 하나인지 확인한 뒤 readiness probe를 C/LLVM으로
  컴파일·실행해 artifact equality를 확인한다.
- 첫 diagnostic 뒤 codegen seed를 무조건 다시 만들지 않는다. imported source
  identity가 그대로면 기존 gen2/parser를 재사용해 reached production owner만
  다시 검증한다. Make의 driver target은 seed target을 dependency로 가지므로,
  재사용 검증에서는 build-dir을 명시하고 `driver_bootstrap.sh`를 직접 실행한다.

수정 후 focused C/LLVM readiness gate는 17.2초에 통과했고, bounded production
driver는 534.4초에 exit 0으로 sample C, MIR producer JSON, MIR consumer C가 native
oracle과 byte-identical했다. 이 run의 긴 구간은 native oracle driver 컴파일이며
약 0.967 GiB RSS로 관찰됐다. 이를 self-host 실행이나 3 GiB graph 회귀로 분류하지
않는다.

## 현재 소스 fixed point와 3 GiB 근접 MIR consumer를 구분하는 법

2026-08-01의 `46eef938`은 이전의 30분 미완주 상태를 폐쇄했다. 현재 compiler
source를 Pergyra-built driver로 처리한 결과는 다음과 같다.

| 단계 | 결과 | 시간 | peak private |
|---|---:|---:|---:|
| current source -> MIR | 90,347,259 bytes, `A5062BEE...BD54` | 57.715초 | 1.990 GiB |
| current MIR -> gen2 C | 5,589,506 bytes, `BBB42686...6D3` | 95.336초 | 2.986 GiB |
| same MIR -> gen3 C | gen2와 byte-equal | 98.520초 | 2.943 GiB |

source-to-MIR는 attention threshold 아래에서 끝난다. MIR-to-C는 3 GiB hard cap
아래에서 완주하지만 2.4 GiB attention threshold는 넘는다. 따라서 현재 판정은
`CPU 미완주`가 아니라 `고정점 완주, consumer memory 후속 관찰 필요`다.

이번에 닫힌 반복 작업은 다음과 같다.

- body-type bundle 전체 readiness를 verify와 codegen에서 두 번 실행하던 경로를
  한 번의 admission receipt로 바꿨다.
- builtin group마다 전체 expression surface를 다시 훑고, cast와 spawn마다
  expression graph를 별도로 훑던 경로를 surface 1회 + graph 1회로 바꿨다.
- 각 callee 비교마다 `Concat(callee, "(")` needle을 만들지 않고 byte 경계를
  직접 비교한다.
- nominal마다 전체 global environment prefix를 다시 붙이지 않고 선택 batch의
  row만 모아 한 번 붙인다.
- 모든 function C definition 문자열을 배열에 끝까지 보존하지 않고
  `program_function_definition_block_owner`가 builder에 복사한 뒤 해당 definition
  epoch을 즉시 해제한다.
- 함수 종료 뒤에도 살아 있던 body, local environment, defer/copyout/signature
  조립 문자열은 마지막 consumer 뒤 한 owned epoch으로 해제한다.

pressure marker의 마지막 줄만 병목으로 단정하지 않는다. marker는 마지막으로
도달한 소유자를 말하고, peak sample은 그 이후의 type declaration, runtime usage,
definition assembly 같은 다른 단계에서 발생할 수 있다. 최종 summary의 peak와
stage CSV를 시간으로 대조한 뒤 소유자를 정한다.

다음 두 가지 잘못된 진단을 피한다.

1. 오래된 MIR을 새 generator가 처리하다 timeout 난 결과는 현재 gen2 성능이
   아니다. generator source가 바뀌면 먼저 현재 source MIR을 한 번 만들고, 같은
   MIR을 gen2와 gen3가 소비하게 해야 한다.
2. gen2==gen3 전체 테스트 시간은 일반 작은 프로그램 빌드 시간이 아니다.
   hello oracle은 별도로 artifact equality를 확인한다. 기본 배포 driver가
   self-host 경로로 치환되기 전에는 native `pgy` 시간과 self-host 고정점 시간을
   섞어 제품 컴파일 성능이라고 보고하지 않는다.

실패한 최적화도 보존한다. statement traversal 안에서 교체된 local environment
문자열을 즉시 해제하면 peak가 약 2.669 GiB까지 낮아졌지만
`semantic leaf binding fact is missing: c`로 실패했다. 현재 statement view가 이전
row를 빌려 쓰므로 이는 use-after-owner-release에 해당한다. 이 실험은 완전히
되돌렸고, borrow owner를 먼저 바꾸지 않는 한 재도입하지 않는다.

## Windows 기본 `BIN_DIR=bin`에서 실행 파일이 객체 파일로 바뀌는 경우

2026-08-01에 `PROJECT_ROOT=D:/PergyraLang BUILD_DIR=... BIN_DIR=bin compiler`를
직접 실행했을 때 make는 성공 종료했지만 다음 경고를 냈고, `bin/pgy.exe`는
`MZ` 실행 파일이 아니라 3,291-byte COFF 객체가 됐다.

```text
overriding recipe for target '.../bin/pgy.exe'
Circular .../bin/pgy.exe <- .../bin/pgy.exe dependency dropped
```

원인은 Windows의 `EXEEXT=.exe`에서 canonical `$(PGY)`와 repo-copy target
`$(REPO_BIN_DIR)/pgy$(EXEEXT)`가 같은 경로인데도 두 번째 copy rule을 선언한
것이다. recipe 내부의 경로 비교는 너무 늦다. make가 이미 같은 target의 recipe와
자기 의존성을 병합했기 때문이다.

수정 규칙은 recipe가 아니라 선언 자체를 `abspath` 불일치 조건으로 감싸는 것이다.
같은 조건을 `pgy-lsp`와 중복 `REPO_BIN_DIR` directory rule에도 적용한다. 복구할 때는
별도 `BIN_DIR`에 먼저 링크하고 `MZ` header와 크기를 확인한 뒤 `bin`에 설치한다.
make exit 0만으로 실행 파일 건전성을 주장하지 않는다. 기본 경로 최종 확인은
`default_c_emit_installed_self_host_owner.sh`가 sibling `pgy-self-driver`를 환경변수
없이 선택하고 hello artifact/실행 parity를 통과해야 한다.

같은 세션의 self-host 설치 후보는 114.600초에 정상 생성됐지만 pressure wrapper를
`RootProcessTreeOnly`로 실행하자 MSYS가 분리한 codegen 자식을 추적하지 못했다.
summary의 `detached_compiler_worker_tracking=false`, compile sample 0, peak private
0.008 GiB는 유효한 compiler peak가 아니다. 수치가 지나치게 작을 때도 그대로
성능 개선으로 기록하지 말고, process coverage를 먼저 확인한다. 이미 완주한 semantic
target을 이 계측 실수만으로 반복하지 않는다.

### Default-driver promotion 뒤 current fixed point가 다시 깨지는 경우

설치 composition root가 production CLI owner를 새로 import한 직후 source-to-MIR는
완료했지만 MIR consumer가 다음처럼 fail-closed했다.

```text
MIR-LOWER ERROR: MIR phi facts are missing or inconsistent: RunDriverRung2FromArgs
```

CLI owner는 optional machine declaration을 다섯 branch에서 `Empty()`로 만든 뒤
조건부 재할당했다. 설치 graph에 처음 도달한 이 함수의 merge identity가 MIR phi
계약을 만족하지 못했다. 해법은 phi를 추측하거나 consumer fallback을 넣는 것이
아니라, `DriverRung2OptionalMachineDeclaration`이 branch별 값을 조기 return하게 하고
source mode도 각 branch에서 완결된 declaration으로 즉시 실행하는 것이다.

그 다음 gen2는 2.991 GiB로 완료했지만 새 gen2가 같은 MIR을 처리하는 gen3는
3.035 GiB에서 hard stop됐다. 마지막 marker는
`[codegen-pressure-stage] definitions:done:4244`였다. Function definitions는 이미
builder streaming이었지만 `CollectProtos`가 모든 prototype 문자열을 `Array<String>`에
보존한 뒤 join하는 두 번째 program-scale retention 경로를 갖고 있었다. Prototype도
같은 방식으로 한 builder에 append하고 각 completed row를 즉시 해제한다. 이 경로에
cache, shard, worker, timeout, 또는 더 높은 memory limit을 추가하지 않는다.

수정 후 current-source fixed-point 증거는 다음과 같다.

| 단계 | 결과 | 시간 | peak private |
|---|---:|---:|---:|
| current source -> MIR | 90,429,326 bytes, `BA91F9CF...C2650` | 44.906초 | 1.993 GiB |
| current MIR -> gen2 C | 5,595,167 bytes, `D65657BF...096A` | 98.095초 | 2.949 GiB |
| gen2 -> gen3 C | gen2와 byte-equal | 98.614초 | 2.988 GiB |

gen2 host C compile은 별도 측정에서 55.063초/0.753 GiB였다. 따라서 attention
대상은 일반 C compiler가 아니라 compiler-scale self-host MIR consumer다. 정상
설치 빌드 경로도 최신 source에서 96.9초에 완료했다. 2.4 GiB attention은 계속
유지하되, hard cap 아래의 완주를 이유로 다시 같은 세대를 반복하지 않는다.

### Fixed-point 단계는 통과하지만 direct source-to-C만 3 GiB를 넘는 경우

2026-08-01 `d12f8240` 소스에서 설치 `pgy-self-driver`에 compiler source와 C
output을 한 번에 주는 경로는 52.095초에 peak private 3.187 GiB로 hard stop됐다.
Host compiler나 출력 크기 문제가 아니다. 같은 90,429,326-byte 입력을 기존
artifact owner 경계로 분리하면 다음과 같이 완주한다.

| 단계 | 결과 | 시간 | peak private |
|---|---:|---:|---:|
| source -> MIR artifact | SHA-256 `A151D69C...F9CA9B` | 53.579초 | 2.038 GiB |
| MIR artifact -> gen2 C | 5,595,167 bytes, `275A66AC...A33440F` | 106.435초 | 2.912 GiB |
| gen2 C -> host executable | 3,486,183 bytes | 59.450초 | 0.752 GiB |
| gen2 -> gen3 C | gen2와 byte-equal | 105.837초 | 2.985 GiB |

관찰된 소스 경로는 `CompileSourceToCVerified`가
`CompileSourceToMirJsonVerified`의 완전한 JSON 문자열을 만든 직후 같은
프로세스에서 `CompileMirJsonTextToCVerified`로 다시 admission한다. 두 개별 단계가
각각 cap 아래인데 합성 경로만 cap을 넘으므로, producer epoch와 MIR consumer
epoch의 whole-program lifetime이 겹친다는 판단은 실측과 소스에서 나온 inference다.
해법을 cache, shard, timeout 증가로 바꾸지 않는다. 현재 compiler-scale fixed point는
source-to-MIR와 MIR-to-C를 별도 프로세스/transaction owner로 실행해 앞 단계의
증거 lifetime을 종료한 뒤 다음 단계를 시작한다. Direct 경로를 다시 허용하려면
structured fact handoff 또는 명시적 release가 같은 hard-cap gate를 통과해야 한다.

같은 세션에 normal install script도 별도 문제를 드러냈다. 기본
`.tmp/self_hosted/codegen/bootstrap/gen2.exe`는 현재 `SubstringWithLen` builtin을
모르는 stale AST codegen seed여서 126.229초/1.113 GiB 뒤
`undefined_function: SubstringWithLen`로 실패했다. 설치 `pgy-self-driver`는
source/MIR driver이지 AST-to-stdout codegen seed가 아니므로 그 자리에 넣으면
11.888초 만에 AST 텍스트를 source로 오해한 parse error로 실패한다.

따라서 seed 존재나 binary hash만 보고 호환된다고 판단하지 않는다. DRV-2 전체
graph를 처리하기 전에 실제 `SubstringWithLen(...)` 호출 fixture를 선택된
parser -> AST -> codegen 조합으로 실행하는 capability preflight를 두고 fail-fast해야
한다. Previous-generation seed 자체는 정상 bootstrap 입력이므로 current source-set
전체 hash equality를 강제해 모든 편집마다 reseed하는 것도 금지한다.

## Installed C compile/run 검증에서 경로·메모리·일반 빌드를 혼동하는 경우

2026-08-01 public C compile/link와 `--run`을 installed self-host artifact로
치환하면서 세 가지 독립 문제를 확인했다.

첫째, MSYS make가 native linker response file에 `/d/PergyraLang/...` 형태의
경로를 기록하면 Windows `ld`는 response file 내부의 MSYS path를 해석하지 못한다.
이는 권한 문제나 compiler 결함이 아니다. Staged build는 repo-relative
`BUILD_DIR=build`를 쓰고, 별도 repo-relative `BIN_DIR`에 먼저 링크한다. 절대 경로가
필요한 변수는 `D:/PergyraLang` 같은 mixed/native spelling으로 넘기거나 response
file 생성 전에 `cygpath -m`으로 변환한다. 성공 뒤에는 exit code뿐 아니라 `MZ`
header와 실행 결과를 확인한 다음 설치한다.

둘째, self-host runtime은 명시적 authority 없이 absolute output path를 거부한다.
이를 피하려고 `PGY_IO_ALLOW_ABSOLUTE=1`을 launcher 전역에 설정하면 sandbox 계약을
약화한다. Native C launcher는 repository-relative private workspace를 만들고 그
안의 C artifact를 정확히 한 번 materialize한 뒤 host compile/link와 optional run을
수행하고 workspace를 정리한다. Missing driver, missing artifact, unsupported option은
source를 native pipeline으로 다시 처리하지 않고 실패해야 한다.

셋째, 일반 C 빌드와 compiler fixed-point/test 비용을 분리한다. Installed C binary
target은 작은 입력에서 self-host artifact 한 번과 host compile/link 한 번만 수행한다.
Gen2/gen3 byte equality, 전체 compiler source MIR, sanitizers와 수백 gate는 test 또는
merge-boundary 증거이지 사용자 normal build의 반복 단계가 아니다. 메모리는 매
샘플마다 최적화 신호로 삼지 않고 semantic target당 한 번 실행한 final max만
기록한다. Attention은 2.4 GiB, hard stop은 3 GiB이며 threshold를 넘기지 않은 같은
target을 계측 확인만으로 반복하지 않는다.

### MSYS bootstrap에서 import-composed source만 절대 경로로 거부되는 경우

2026-08-01 current codegen seed를 갱신할 때 bootstrap script가
`D:/PergyraLang/src/self_hosted/...` 절대 source path를 native `pgy`에 넘겨
import composition 전에 실패했다. Output artifact의 절대 경로 허용과 source 입력의
authority는 서로 다른 계약이다. `PGY_IO_ALLOW_ABSOLUTE` 또는 launcher 권한을 넓혀
source check를 우회하지 않는다.

수정은 codegen, LLVM leg, parser-tool build의 **source input만** repository-relative로
전달하는 것이다. Output path와 cache identity는 기존 explicit owner를 유지한다.
Component contract는 bootstrap owner에 절대 source invocation이 다시 들어오면
실패해야 한다.

수정 후 current-source capable gen2 seed는 410.451초에 exit 0이었고 process-tree
peak working/private는 2.705/2.841 GiB였다. 이는 2.4 GiB attention 대상이지만 3 GiB
hard stop은 넘지 않는다. 이 seed-only 성공은 current gen2==gen3 fixed point를
의미하지 않는다. 고정점 gate를 실제로 다시 실행하지 않았다면 이전 세대의
byte-equality를 최신 증거로 기록하지 않는다.

같은 rung의 intermediate driver build는 98.359초, peak working/private
1.579/1.684 GiB였다. 이후 작은 rebuild를 계측하지 않았다면 이 값을 최종 installed
binary의 정확한 peak라고 보고하지 않는다. 최종 artifact hash/size와 resource
measurement의 증거 범위를 따로 기록한다.

## Self-host driver 수정마다 codegen seed가 다시 실행되는 경우

`make self-host-compiler`는 단순한 증분 driver target이 아니다. 이 target은
`self-host-codegen-bootstrap-seed-test-smoke`에 의존하고, 그 선행 target은 phony다.
따라서 source import 하나를 고친 뒤 `make self-host-compiler`를 다시 호출하면 이미
capability가 확인된 seed 단계까지 재실행할 수 있다. 이것은 권한 문제나 compiler가
멈춘 증상이 아니라 Make dependency가 정의한 **bootstrap/test 경계**다.

Clean tree와 CI에서 실행하는 focused gate는 재현 가능해야 하므로
`self-host-one-mir-array-return-projection-test-smoke`처럼 `self-host-compiler`에
의존한다. 반면 현재 parser/gen2 seed의 capability가 이미 이 checkpoint에서
확인됐고 import-composed driver source만 바뀐 로컬 edit loop라면 다음 owner script를
직접 한 번 실행해 DRV-2만 갱신할 수 있다.

```sh
PGY_SELF_DRIVER_BIN=/d/PergyraLang/bin/pgy-self-driver.exe \
  tests/self_hosted/parity/self_host_compiler_build.sh
```

Windows에서는 Make가 선택한 MSYS bash 또는 로그인 MSYS shell을 사용한다. 빈
Windows `PATH`를 상속한 `bash.exe`를 직접 띄우면 build 시작 전 `dirname: command not
found`로 끝날 수 있다. 이 실패는 source parse/codegen 실행이나 artifact 변경 증거가
아니다.

직접 script 경로는 current seed를 무조건 신뢰하는 우회가 아니다. Script가 parser와
codegen binary, composed AST, output identity, compiler profile을 fingerprint하고 smoke를
통과한 뒤에만 설치한다. Seed capability가 바뀌었거나 clean/CI 증거가 필요하면 Make의
전체 의존 경계를 사용한다. 어느 경로든 반복 seed 실행 시간이나 gate 수를 self-host
치환 진척으로 세지 않는다.

2026-08-01의 최종 multi-routine Array-return DRV-2 갱신은 direct script에서 93.9초에
완료됐다. 이 실행은 pressure owner로 계측하지 않았으므로 peak memory는 `Unknown`이며,
이전 1.684 GiB나 2.841 GiB 수치를 상속하지 않는다.

## Aggregate parameter가 C/LLVM에서 서로 다른 폭이나 layout으로 보이는 경우

2026-08-02 첫 three-routine Array parameter 작업에서 일반 self-host C emitter의
`long long` 표면과 direct-MIR lane의 `int32_t`가 함께 보여 32/64-bit ABI 충돌처럼
보였다. 이때 일반 emitter 문자열이나 backend 관례를 ABI 권위로 사용하면 안 된다.
Pergyra `Int`의 언어 ABI는 signed 32-bit이고, `Array<Int>`의 물리 경계는 MIR formal
parameter가 운반하는 layout receipt가 소유한다. `i64`/`long long`은 이 direct lane에서
printf 확장을 위한 최종 표현일 뿐 파라미터 ABI가 아니다.

수정 원칙은 다음과 같다.

- Native MIR, self-host 문자열 JSON, self-host streaming JSON이 동일한 parameter
  row를 내보내야 한다: `name`, `type`, `carriage`, `resource`, `pass`,
  `abi_type_name`, `abi_layout_id`, `abi_layout_required`, `abi_layout`.
- Required aggregate는 complete row와 재계산 가능한 ID를 운반한다. Backend가
  type spelling에서 offset/size/align을 복원하지 않는다.
- Scalar처럼 row가 필요하지 않은 타입은 ID 0, required false, null layout을
  명시한다. 반대로 by-value nominal aggregate가 이 상태라면 허용 신호가 아니라
  아직 ABI owner가 닫히지 않은 증거다.
- MIR에 parameter SSA/use가 없다면 만들어내지 않는다. 이 fixture의 call/result
  identity는 typed expression graph edge와 exact callee signature가 소유한다.
- String과 streaming JSON writer는
  `routine_param_json_projection_owner.pgy` 하나를 공유한다. 파일 줄 수 cap을
  올려 중복 직렬화를 유지하지 않는다.

Focused 확인은 `one_mir_array_argument_projection.sh` 하나로 native/self parameter
receipt parity, 같은 self-host MIR의 C/LLVM execution, routine permutation, repaired
ABI ID와 missing ABI를 포함한 16 negatives를 실행한다. 일반 build 성능이나 메모리
문제로 확대하지 말고, 다음 nominal aggregate가 `abi_layout_required=false`로 나오면
그 nominal declaration/layout owner를 다음 falsifier로 기록한다.

## Named struct parameter가 declaration과 다른 ABI로 보이는 경우

`struct_literal_call_argument.pgy`의 첫 MIR은 `Vec2`/`Line` field 의미는 갖고
있었지만 `Line` formal parameter가 ID 0, required false, null layout이었다. 이를
backend가 C struct나 LLVM aggregate 규칙으로 보충하면 두 backend가 각각 물리
권위가 된다. 해법은 program declaration graph를 topological order로 한 번 계산해
complete nominal receipt만 발행하고, parameter가 그 exact declaration receipt를
운반하게 하는 것이다.

- 현재 admitted leaf는 Pergyra `Int`의 4-byte/4-align 언어 ABI와 이미 해결된
  nested value struct다. Unsupported/cyclic declaration은 row를 추측하지 않는다.
- `Vec2`는 size 8/align 4/ID `669680999`, `Line`은 size 16/align 4/ID
  `643231747`이다. Field order, type, offset, size, align과 content ID를 함께 검증한다.
- Native/self syntax ID는 서로 다른 arena의 local representation이므로 숫자
  equality를 요구하지 않는다. 대신 각 producer에서 positive/local unique인지와
  semantic field/layout 교차봉인을 확인한다.
- 같은 MIR을 backend마다 다시 생성하지 않는다. Source-to-MIR 한 번 뒤 C/LLVM을
  각각 한 번 투영하고, routine/declaration permutation artifact equality를 본다.
- Named-struct candidate가 분류된 뒤 Array plan을 retry하거나 backend가 type
  spelling으로 layout을 복원하면 negative ratchet 위반이다.

최종 focused gate는 exact `6`, 세 permutation, 15개 pre-artifact negative를
통과했다. 설치 C/LLVM 경로도 같은 frontier를 사용한다. 이 결과는 aggregate
return/local ABI까지 닫혔다는 뜻이 아니다. 바로 다음
`struct_literal_value_flow.pgy`는 declaration receipt만 있고 routine return과 local
SSA definition receipt가 없어 fail-closed한다.

## `ArrayPush` 뒤 self-host driver가 Windows access violation으로 끝나는 경우

Nominal layout owner를 처음 추가했을 때 source parse가 아니라 generated driver
실행 중 access violation이 났다. 원인은 메모리 총량이나 누적 graph 검증이 아니라,
growable `Array<T>` descriptor를 포함한 struct snapshot을 만든 뒤 그 snapshot의
field에 `ArrayPush`를 수행하고 오래된 descriptor를 계속 읽은 것이었다. Push가
storage를 재할당하면 이전 descriptor copy는 최신 pointer/capacity를 소유하지 않는다.

수정 규칙은 다음과 같다.

- Grow 가능한 각 array를 owner 시작 시 local handle로 꺼내고, 이후 push/set/read와
  nested resolver 전달은 그 최신 local handle만 사용한다.
- 모든 mutation이 끝난 뒤 최신 handle들로 result struct를 한 번 재조립한다.
- Push 이전의 outer struct snapshot이나 그 field expression을 mutation 이후
  lookup/read authority로 재사용하지 않는다.
- Access violation을 메모리 hard-cap 문제로 오분류해 cap, cache, shard, worker를
  추가하지 않는다. 누적 graph 반복검증 결함과 stale growable-handle 결함은 서로
  다른 원인과 gate를 가진다.

이 rung에서는 위 수정 뒤 DRV-2가 96.2초에 설치됐고 focused/installed/hard/component
gate가 통과했다. Memory pressure는 측정하지 않았으므로 과거 peak를 이 binary에
붙이지 않는다.

## Named struct return/local이 declaration receipt 없이 backend에 도달하는 경우

`struct_literal_value_flow.pgy`의 첫 MIR은 `Pair` declaration에 size 8, align 4,
ID `674136663`을 갖고도 `BuildPair` return과 aggregate local definitions에는 그
receipt를 싣지 않았다. Backend가 routine return spelling이나 let expression을 다시
읽어 layout을 찾으면 producer와 backend가 이중 권위가 된다.

현재 규칙은 다음과 같다.

- Native producer는 static ABI lookup 뒤 unique program nominal declaration만
  허용한다. 알려진 program ABI가 있는 return/def/assign은 exact receipt 누락 시
  MIR validation에서 실패한다.
- Self producer는 instruction마다 type name뿐 아니라 exact declaration row와
  layout ID를 소유한다. JSON writer는 이 receipt만 소비하며 declaration inventory나
  `expr1`을 fallback으로 다시 읽지 않는다.
- Native residual assignment와 self SSA `pair.2`는 서로 다른 representation이다.
  Gate는 둘을 같은 instruction shape로 위장하지 않고 latest semantic use와 exact
  receipt를 교차 검증한다.
- Source-to-MIR은 한 번만 수행한다. 같은 11,441-byte MIR을 C와 LLVM에 각각 한 번
  투영하고, permutation과 negatives는 source graph를 재생성하지 않는다.
- Plain-struct candidate가 분류된 뒤 declaration-free Array-return owner로 retry하거나
  backend가 offset을 추측하면 실패한다.

최종 focused gate는 exact `11`, routine permutation, 13개 pre-artifact negative를
통과했고 installed C/LLVM 경로도 같은 frontier를 사용한다. 다음
`option_struct_value_flow.pgy`는 unwrapped `Pair` receipt는 있지만 `Option<Pair>`
return/local의 ID가 0이고 required false라 fail-closed한다. 이 경우 plain `Pair`
owner를 넓히지 말고 Option ABI와 inner nominal declaration을 결합한 nested receipt를
별도 소유해야 한다.

## `make self-host-compiler` 8분을 일반 컴파일 속도로 해석하는 경우

이번 최종 명령은 seed artifact와 Pergyra-built driver를 함께 갱신해 506.2초가
걸렸다. 이는 사용자 프로그램 한 개의 일반 C/LLVM 컴파일 시간이 아니며, focused
fixture projection이나 이미 설치된 driver의 보통 compile/run 수치로 재사용하면 안
된다. 성능 보고는 다음 세 경계를 분리한다.

- 일반 build: 설치된 self-host driver가 입력 하나를 MIR과 선택 backend artifact로
  만드는 시간.
- test build: parity, mutation, permutation, installed-path ratchet을 포함한 시간.
- bootstrap/fixed point: seed, driver, gen2/gen3 또는 전체 통합을 갱신하는 시간.

메모리도 같은 원칙을 따른다. 매 단계 반복 측정하지 않고 semantic target의 final
maximum만 기록하며, 2.4 GiB attention과 3 GiB hard stop을 넘을 때 반복 owned
operation부터 조사한다. 이번 506.2초 실행은 메모리를 측정하지 않았으므로 과거
3 GiB peak나 다른 build의 RSS를 붙이지 않는다.

## `Option<Pair>`가 inner `Pair`만 알고 wrapper ABI는 모르는 경우

`option_struct_value_flow.pgy`의 첫 self MIR은 unwrap 결과인 `Pair`에는 size 8,
align 4, ID `674136663`을 운반했지만 return/local의 `Option<Pair>`에는 ID 0과
`required=false`를 기록했다. Backend가 `Option<Int>`를 복사하거나 type spelling을
분해해 tag/payload offset을 만들면 wrapper ABI가 backend-local 권위가 된다.

수정 규칙은 다음과 같다.

- Static Option ABI owner는 tag 이름, offset/size/align, representation,
  discriminant와 Some/None tag만 소유한다.
- Program nominal owner는 inner `Pair`의 size/align/field layout을 소유한다.
- Producer가 두 사실을 declaration row에서 한 번 결합해 size 12, align 4,
  tag@0, value@4, ID `798450640`인 `Option<Pair>` receipt를 발행한다.
- Instruction은 wrapper kind, exact declaration row와 layout ID를 운반한다. JSON
  writer와 C/LLVM emitter는 type/expression text나 declaration inventory를 다시
  읽지 않는다.
- Nominal classifier는 Option과 plain을 한 번만 구분한다. Option plan이 실패한
  뒤 plain-struct plan으로 재시도하지 않는다.

Focused gate는 한 source MIR을 C/LLVM이 공유하고 exact `7\n11\n5`, routine
permutation, outer 5개/inner 2개 receipt와 tag/payload/call/use/member 변조 20회가
artifact 발행 전에 닫히는지 확인한다. Installed C/LLVM도 같은 fixture를 사용한다.

## Bootstrap manifest가 nominal ABI receipt 오류로 멈추는 경우

증상은 `make self-host-codegen-bootstrap-seed-test-smoke`가 세대 생성 전에 다음
진단으로 멈추는 것이다.

```text
MIR instruction ABI receipt rows are invalid
```

2026-08-02 회귀에서 `CompilerCompletenessCheckTarget { path: String }`은 nominal
declaration이지만 고정 물리 ABI를 갖지 않아 native MIR의
`abi_layout_required=false`, ID 0이 정상이었다. 새 producer가 exact name match만
보고 18개 instruction을 nominal kind 1로 승격한 뒤 ID 0을 오류로 처리했다.

판정 규칙은 declaration 존재와 physical receipt 존재를 분리한다.

- `required==1`: kind 1(nominal) 또는 2(Option nominal), exact declaration row,
  nonzero layout ID를 요구한다.
- `required==0`: 중립 tuple `(kind=0,row=-1,id=0)`를 유지한다.
- String nominal ABI를 억지로 추가하거나 unknown layout을 추정하지 않는다.
- Producer와 verifier가 같은 required 조건을 교차 봉인하고, TestHarness manifest
  compile과 bootstrap seed를 positive regression으로 유지한다.

수정 뒤 manifest의 receipt/ABI 배열은 모두 2,669행으로 일치하고 `ready=1`이며,
current-source bootstrap seed가 416.6초에 완주했다. 이 시간은 일반 프로그램
컴파일 성능이 아니라 bootstrap integration 비용이다.

## Explicit generic MIR이 C에서는 보이지만 direct C/LLVM plan에서 거부되는 경우

`generic_struct_field_value_flow.pgy`의 self MIR은 `Identity<T>`와 네 개의
`Identity<Int>` 호출, 그리고 네 specialization receipt를 운반한다. 기존
multi-routine root는 declaration 수 1인 세-routine 프로그램을 소유하지 않아 두
target 모두 artifact 전에 거부했다. Native MIR에는 동일 graph와 `Pair` ABI가 있지만
specialization table은 비어 있다.

이 경우 native의 빈 table을 정답으로 삼거나 `Identity<Int>` source text에서 symbol을
복원하면 안 된다. 현재 판정 규칙은 다음과 같다.

- Self MIR specialization table이 direct projection의 carried authority다. Native는 실제로
  공통인 routine graph와 ABI parity에만 사용한다.
- 네 행은 `(target kind, owner, callable, formal, actual, specialized symbol)`이 같은
  uniform semantic class여야 하고, Atom/Value lane의 ordinal 0/1 좌표가 중복 없이
  정확히 존재해야 한다.
- MIR graph에도 동일한 direct callable과 actual을 가진 generic call이 정확히 네 개
  있어야 한다. Emitter는 verified symbol만 소비하고 이름을 다시 조합하지 않는다.
- `source_owner_syntax_id`는 현재 specialization table 밖의 graph receipt와 직접 join되지
  않는다. 따라서 임의의 새 양수로 일관되게 renumber한 mutation까지 거부한다고
  주장하지 않는다. Exact per-call provenance가 필요하면 producer protocol에 stable call
  identity를 추가하는 별도 seam이 필요하다.
- 세-routine root는 declaration 수로 한 번만 분류한다. Generic plan 실패 뒤 Array나
  nested-struct plan을 retry하지 않는다.

Focused gate는 한 14,014-byte self MIR로 C/LLVM exact `7`, 실제
`Identity_Int` 정의 1개와 호출 4개, routine/specialization permutation과 29개
pre-artifact negative를 검증한다.

## Inferred generic Pair MIR이 plain nominal 분류에서 먼저 거부되는 경우

`generic_struct_field_inferred_value_flow.pgy`는 routine 2개와 declaration 1개라
기존 plain/Option nominal 후보에도 들어간다. 하지만 두 번째 routine은 일반 함수가
아니라 `Identity<T>(value:T)->T`이므로 generic을 금지하는 일반 signature owner에서
다음과 같이 artifact 전에 멈췄다.

```text
CODEGEN ERROR: direct MIR two-routine nominal classification is invalid
```

해결은 semantic classifier를 느슨하게 만드는 것이 아니라 specialization cardinality를
상위에서 한 번만 분류하는 것이다.

- specialization 0행은 기존 plain/Option nominal refinement만 소비한다.
- specialization 2행은 별도 inferred-generic nominal owner만 소비한다.
- 다른 수이거나 선택된 plan이 거부되면 다른 해석으로 retry하지 않는다.
- inferred 두 행은 같은 양수 source owner, Value lane, ordinal `{0,1}`과 동일한
  `(direct, callable, formal, actual, symbol)` tuple을 가져야 한다.
- 기존 explicit 4행 Atom/Value owner는 그대로 유지하며 공용 permissive owner로
  합치지 않는다.
- Native MIR의 빈 specialization table은 공통 routine graph와 `Pair` ABI parity만
  제공하며 specialization 복원에 쓰지 않는다.

현재 `source_owner_syntax_id`는 graph call node와 join할 stable ID가 direct MIR에 없다.
따라서 한 행만 다른 owner로 바꾼 경우는 거부하지만, 두 행을 같은 다른 양수로
일관되게 바꾼 경우는 artifact-equal metamorphic이다. 숫자 `18`을 하드코딩해 이를
negative로 만들면 provenance를 증명하는 것이 아니라 fixture 번호를 외운다. 더 강한
증거가 필요하면 producer protocol이 initializer owner identity를 graph/instruction에
추가로 운반해야 한다.

Focused gate는 하나의 7,200-byte self MIR로 C/LLVM exact `42`, 실제
`Identity_Int` 정의 1개와 호출 2개, 네 metamorphic과 32개 negative를 검증한다.

## `abi_layout_required=false`를 타입 없음으로 잘못 해석하는 경우

Current-source driver를 다시 만든 뒤 기존 Array argument와 nested-struct argument gate가
다음 진단으로 멈출 수 있다.

```text
direct MIR Array argument instruction ABI is invalid
direct MIR struct argument instruction ABI is invalid
```

문제의 scalar return은 `abi_type_name=Int`, `abi_layout_id=0`,
`abi_layout_required=false`, `abi_layout=null`이다. 이것은 typed scalar가 물리 aggregate
layout을 필요로 하지 않는다는 뜻이지 타입 identity가 비어 있다는 뜻이 아니다. 기존
plan은 “no physical ABI”를 `abi_type_name == ""`와 동일시해 최신 producer의 정상
receipt를 거부했다.

수정 규칙은 expected semantic type과 physical layout 유무를 분리하는 것이다.

- `Main`의 Log statement처럼 값 ABI가 없는 instruction은 expected type `""`를
  요구한다.
- `Int`를 반환하는 user routine은 expected type `Int`를 유지한다.
- 두 경우 모두 physical layout이 없다면 ID 0, required false, null layout을 정확히
  요구한다.
- Scalar type을 지우거나 임의의 aggregate layout을 추가해 gate를 통과시키지 않는다.

수정 뒤 Array argument, nested-struct argument, explicit-generic, Option<Pair> C/LLVM
focused gate가 함께 green인지 확인한다.

## PowerShell에서 MSYS Make가 `tr`, `rm`, `gcc`를 찾지 못하는 경우

전체 액세스나 승인 문제로 오해하지 않는다. Windows PowerShell에서
`C:\msys64\usr\bin\make.exe`를 직접 실행하면 이 설치의 UCRT64 경로가
상속되지 않아 Make 내부 `/usr/bin/bash`가 도구를 찾지 못할 수 있다. 설정 stamp가
먼저 갱신되면 이후 정상 호출에서도 전체 native object 재빌드가 한 번 발생한다.

이 환경의 재현 가능한 호출은 MSYS 로그인 셸 안에서 UCRT64와 `/usr/bin`을 먼저
소유시키는 것이다.

```powershell
& 'C:\msys64\usr\bin\bash.exe' -lc `
  'export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/PergyraLang && make <target>'
```

환경 경계 때문에 일어난 전체 재빌드 시간은 focused gate나 일반 self-host program
compile 시간에 합산하지 않는다.

## Generic member MIR이 count-only 배열 검사나 C 임시 이름 때문에 잘못 통과하는 경우

`generic_member_inferred_flow.pgy`를 direct C/LLVM 경로에 연결할 때 배열의 기대 행
수만 세고 다음 토큰이 무엇인지 확인하지 않으면, 유효한 행 뒤에 scalar를 붙인 변조가
같은 plan으로 통과할 수 있다. JSON 배열 reader가 첫 non-row에서 멈추는 것과 배열이
정확히 끝났다는 것은 다른 사실이다.

- 이 rung이 읽는 specialization, generic, parameter, field, method, source-local 및
  parallel-capture 배열은 기대 행 뒤의 다음 유효 토큰이 반드시 `]`인지 확인한다.
- 행 수만 맞는 scalar tail은 malformed document이며 artifact를 만들기 전에 거부한다.
- 범용 JSON parser를 새 권위로 만들지 않고, 실제 plan이 소비하는 bounded array
  owner에서 exact-tail closure를 검증한다.

같은 작업에서 generated C 내부 변수 `pgy_inner`가 합법적인 source local과 충돌해
컴파일 실패하는 문제도 드러났다. Backend 임시는 source spelling과 별도 namespace를
사용하며 `_pgy_receiver_0`, `_pgy_inner_0`, `_pgy_result_0`처럼 emitter가 소유한다.
Source local 이름을 금지 목록으로 막지 않으며 field와 local이 같은 spelling을 쓰는
합법적 프로그램도 계속 허용한다.

마지막으로 class value를 운반한다고 해서 physical ABI receipt를 발명하지 않는다.
현재 `Box` slice는 `internal_single_int_value_class`라는 bounded internal
representation을 declaration/signature/receiver와 교차 봉인한다. 외부 ABI가 실제로
필요한 seam에 도달하기 전까지 layout row나 interoperability 보장을 주장하지 않는다.

두-routine routing은 exact shape로 분류한다. Member
`(class,class,2,1,1)`, plain/Option `(struct,struct,0,0,0)`, inferred direct nominal
`(struct,struct,2,1,0)` 중 하나를 선택한 뒤 실패하면 다른 해석으로 retry하지 않는다.
이 규칙은 malformed member artifact가 더 느슨한 기존 planner로 새는 것을 막는다.

## Windows에서 self-host build가 존재하는 `driver.c`를 찾지 못하는 경우

다음 오류는 parser/codegen 실패나 권한 문제가 아닐 수 있다.

```text
cc1.exe: fatal error: /d/PergyraLang/.tmp/self_hosted/compiler/bootstrap/driver.c:
No such file or directory
```

`driver.c`가 실제로 생성돼 있는데 GCC만 못 찾는다면 caller 전체에 설정한
`MSYS2_ARG_CONV_EXCL=*`를 먼저 확인한다. 이 값은 self-host driver에 repository-
relative 인자를 보존할 때는 필요하지만, Git Bash가 GCC에 넘기는 `/d/...` 경로의
Windows 변환까지 막으면 안 된다.

- PowerShell에서 `MSYS2_ARG_CONV_EXCL=*`를 전역 설정한 뒤 build script를 호출하지
  않는다.
- `self_host_compiler_build.sh`가 필요한 self-driver invocation에만 해당 값을
  국소 적용하도록 둔다.
- PowerShell의 `bash`가 `C:\Windows\System32\bash.exe` 또는 WindowsApps의 WSL
  launcher로 해석되면 `/mnt/d/...` chdir 단계에서 먼저 실패할 수 있다. 이 환경의
  repository gate는 `C:\Program Files\Git\bin\bash.exe`를 명시해 실행한다.
- 실패 뒤 `driver.c`, `driver.ast.txt`, `driver.emit.key`가 존재하는지 확인한다.
  이미 fingerprint가 맞으면 다음 정상 호출은 emitted C를 재사용하고 compile/install
  단계만 다시 수행한다.

이 실패 시간을 compiler build 성능이나 메모리 표본에 합산하지 않는다. 환경 경계가
정상화된 호출만 측정 증거로 기록한다.

## Self MIR의 Array literal 반환이 `AST_IDENTIFIER`로 나오는 경우

`return [value];`의 native MIR은 `source_type=AST_ARRAY_LITERAL`인데 self MIR만
`AST_IDENTIFIER`라면 backend admission을 느슨하게 만들지 않는다. 이는 배열 문법
지원 부족이 아니라 value-return source kind를 expression text의 괄호/연산자 유무로만
추측한 producer 결함이다. `[value]`에는 그 표식이 없어 identifier로 떨어질 수 있다.

- 이미 admitted된 semantic expression graph의 root kind를 읽는다.
- root가 array-literal spine이면 `AST_ARRAY_LITERAL`을 기록한다.
- 기존 Array-return/constructed-Array consumer도 이 정확한 source fact를 요구한다.
- native/self MIR 행 parity가 source type까지 일치하는지 focused gate로 확인한다.
- consumer가 `AST_IDENTIFIER || AST_ARRAY_LITERAL`을 허용하거나 expression text의
  `[` prefix를 다시 검사하도록 만들지 않는다.

이 결함은 메모리나 host compiler 문제가 아니다. Producer source identity와 consumer
gate를 함께 고친 뒤 current-source self-host driver를 다시 설치해야 한다.

## `Array<T>` layout을 self-host 내부 container나 호출 ABI와 혼동한 경우

`Array<Point>` direct-MIR projection을 추가할 때 세 가지 서로 다른 사실을 하나의
"Array ABI"로 부르면 잘못된 SoT 충돌이 생긴다.

- Public/runtime Array value storage는 `src/runtime/pgy_abi_spec.h`와 direct-MIR
  storage contract가 소유하는 `{data,length,capacity,allocator}` 네 필드다.
- Self-host compiler가 AST/MIR row를 담기 위해 쓰는 growable container의
  `{data,len,cap}`은 compiler-private 구현 자료구조다. Public value ABI가 아니다.
- Value의 storage layout은 함수 인자/반환 전달 규칙이 아니다. Calling convention은
  target profile과 interop boundary를 포함하는 별도 사실이어야 한다.

따라서 direct-MIR plan은 public storage layout ID
`pgy.runtime.pointer64-size_t64.v1`과 별도 closed-module call-ABI receipt를 각각
소비한다. 현재 C는 host compiler가 한 closed module 안의 호출을 분류하고 LLVM은
internal default convention을 사용한다. 이 사실로 Win64, SysV AMD64, AArch64 또는
외부 C ABI 호환을 주장하지 않는다. 외부 symbol 경계가 열릴 때 target profile,
data layout과 call classification을 별도 SoT로 승격하고 cross-target gate를 추가한다.

Consumer가 private `field_order`를 public layout으로 사용하거나, 네 필드가 같다는
이유만으로 external ABI가 닫혔다고 판단하거나, emitter가 target default를 추측하면
artifact publication 전에 실패시킨다.

## PowerShell pressure runner의 self-host 격리 경로가 codegen에서 사라지는 경우

`measure_build_pressure.ps1`로 fresh self-host build를 감쌀 때 Windows 기본 `bash`는
WSL launcher일 수 있다. 이 경우 `/mnt/d/...` chdir 전에 실패하며 peak 0인 표본은
compiler evidence가 아니다. Git Bash를 명시해도 격리 변수를 `D:/...` 절대경로로
넘기면 `self_host_compiler_build.sh`의 repo-relative 정규화와 맞지 않아 다음처럼
존재하는 AST를 codegen seed가 찾지 못할 수 있다.

```text
CODEGEN ERROR: AST file not found:
D:/PergyraLang/.tmp/self_hosted/compiler/<run>/driver.ast.txt
```

Pressure invocation은 다음 경계를 지킨다.

- `-Command 'C:\Program Files\Git\bin\bash.exe'`를 사용한다.
- `PGY_SELFHOST_COMPILER_BUILD_DIR`와 `PGY_SELF_DRIVER_BIN`은 repository-relative
  `.tmp/...`로 넘긴다.
- parser, gen2 codegen, host compile, generated-driver smoke가 모두 끝난 exit 0
  summary만 최종 peak로 기록한다.
- WSL/경로 정규화 조기 실패의 elapsed/peak는 최종 compiler build 표본에 합산하지
  않는다.

2026-08-02의 정상 표본은 104.381초, peak private 1.937 GiB, peak working set
1.836 GiB, `attention_required=false`였다. Summary owner는
`.tmp/build-pressure/record-array-final/record-array-final-self-host-build-relative.summary.json`이다.

## 생성 C의 hidden storage가 `__*`이거나 source parameter와 충돌하는 경우

Direct-MIR Array emitter가 내부 backing storage 이름을 직접 고정하면 두 종류의 결함이
생긴다. `__pgy_array_storage`처럼 double underscore로 시작하는 이름은 C 구현에
예약되어 있고, `_pgy_array_storage_0`처럼 합법적인 이름도 source parameter가 이미
소유할 수 있다. Emitter가 이름을 바꾸는 ad-hoc fallback을 가지면 두 target family가
서로 다른 symbol 정책을 다시 소유하게 된다.

- 책임 이름을 `DirectMirArrayStorageSymbol` owner 한 곳에서 발급한다.
- 후보는 `_pgy_array_storage_N`이며 block/parameter scope의 source 이름과 충돌하면
  ordinal을 증가시킨다. 현재 bounded lane은 ordinal 0..15만 허용하고 소진 시
  fail-closed한다.
- Array<Int>와 Array<Point> emitter는 발급된 symbol만 소비하며 `__*`를 직접
  방출하지 않는다.
- Focused mutation은 정확히 `_pgy_array_storage_0` parameter를 주입하고 생성
  artifact가 `_pgy_array_storage_1`을 사용하는지 실행으로 확인한다.
- Public Array layout은 별도 assertion owner가 크기, 정렬, data/length/capacity/
  allocator offset의 여섯 `_Static_assert`를 생성한다. Symbol 충돌 회피가 layout
  또는 call-ABI 판단을 소유해서는 안 된다.

이 문제는 단순 명명 스타일이 아니다. 예약 namespace 사용은 host compiler와의
충돌 가능성을 만들고, source symbol collision은 잘못된 C binding을 만든다. 따라서
두 조건 모두 artifact publication 전 negative gate로 유지한다.

## 선언이 있는 단일 `Log` MIR을 hello 경로가 거부하거나 임의로 무시하는 경우

단일 `Log` instruction이라고 해서 declaration을 버리고 기존 hello emitter를
재사용하면 안 된다. 2026-08-03의 `ability_decl.pgy`는 runtime 값이 필요 없는 ability
method declaration과 integer literal `Log`를 함께 가진다. 기존 경로는 선언 수 0과
string hello를 한 덩어리로 가정해 거부했고, 선언을 그냥 무시하는 완화는 임의의
runtime declaration까지 통과시키는 결함이 된다.

해결 경계는 두 owner로 나눈다.

- compile-time declaration-erasure receipt가 정확한 declaration/method/parameter/
  contract schema와 runtime materialization/layout/symbol/storage의 부재를 봉인한다;
- 일반 literal-Log plan은 그 receipt와 persisted expression graph, instruction/use,
  structured formatted-print ABI만 소비한다;
- C/LLVM emitter는 plan의 target-neutral value와 target ABI projection만 읽고 MIR,
  source, `expr0`를 다시 열지 않는다;
- declaration을 제거한 프로그램, 일관된 이름 변경, display-only `expr0` 변경은
  backend별 artifact byte-equal이어야 하고, graph literal 변경은 실제 출력으로
  관찰돼야 한다;
- declaration/method/contract/graph 변조는 artifact 전에 거부한다.

`DirectMirHelloProjectionFromAdmitted`와 target별 hello emitter를 병존시키거나,
ability/fixture/예상 출력 이름으로 분기하거나, erasure 실패 뒤 scalar/hello/native
경로를 재시도하지 않는다. 다음 enum fixture처럼 runtime identity가 필요한 declaration은
compile-time erasure가 아니므로 별도 값/CFG owner가 받아야 한다.

## self-host 회귀에서 public `pgy --backend=c`를 native oracle로 부른 경우

Public C 경로가 self-host sibling driver로 치환된 뒤에는 같은 regression test 안에서
`pgy --backend=c`를 "native expected output"으로 호출하면 순환 검증이다. Self-host
artifact와 비교할 독립 oracle이 아니라 같은 production path를 다시 호출하기 때문이다.

- 이미 닫힌 작은 fixture는 문서화된 exact output을 gate 입력으로 고정한다;
- 독립 native oracle이 필요하면 self-host substitution을 거치지 않는 명시적 owner
  경계를 사용하고, 그 경계를 테스트가 이름으로 드러낸다;
- public path의 성공을 native와 self-host 두 증거로 중복 계산하지 않는다.

2026-08-03에는 hello, `let_log`, `multilet` 회귀의 expected output을 각각 exact
출력으로 고정해 순환을 제거했다.

## self-host build 579초와 114초를 같은 성능 표본으로 해석한 경우

`make self-host-compiler`는 seed capability에 따라 native configuration, parser,
gen2 codegen과 DRV-2 설치까지 재생성할 수 있다. 2026-08-03의 579.2초 실행은 이 전체
체인을 수행했다. 같은 seed를 재사용한 direct `self_host_compiler_build.sh` 실행은
114.2초였다. 두 수치를 일반 프로그램 컴파일 시간이나 같은 증분 build로 비교하면
안 된다.

성능 기록에는 반드시 실행 범위를 함께 적는다.

- seed/native/parser/gen2 재생성 포함 여부;
- installed driver만 다시 만드는지;
- tests/fixed point/pressure 포함 여부;
- peak memory를 실제로 계측했는지.

계측하지 않은 build에 이전 peak를 붙이지 않는다. 메모리는 매 편집마다 최적화
목표로 쓰지 않고, semantic target을 닫은 integration boundary에서 최종 maximum을
기록한다. 2.4 GiB attention과 3 GiB hard stop 정책은 그대로 유지한다.

## MIR 생산자가 ABI fact를 강화한 뒤 CFG gate가 오래된 hash에서 멈추는 경우

2026-08-03에 `if_else_assign`, `reassign_block`, `nestedif`, `whileloop`,
`forloop`, `break_after_stmt`의 고정 hash가 한꺼번에 어긋났다. 단순한 비결정성이
아니었다. `cd886a6d`에서 assignment MIR 생산자는 target local의 실제 타입을
`abi_type_name: "Int"`로 운반하기 시작했지만 direct branch/loop/break 소비자는
여전히 빈 ABI를 요구했다. 따라서 새 생산자는 결정적으로 새 MIR을 만들었고,
옛 소비자는 그 MIR을 artifact 전에 거부했다.

이 경우 hash만 갱신하면 안 된다.

1. 설치 드라이버와 격리 빌드 드라이버 또는 같은 current-source 드라이버의 독립
   두 실행으로 MIR byte equality를 확인한다.
2. 생산자 변경 commit과 새 fact owner를 확인한다. 이번 경우 owner는 assignment
   target type이고 소비자는 정확히 `Int`를 요구해야 했다.
3. C와 LLVM의 실제 투영·compile·run을 모두 실행한다.
4. 다른 타입(`String`)으로 ABI를 위조해 artifact publication 전에 거부되는지
   branch, loop, break 각 소비자에서 확인한다.
5. 그 증거가 모두 green인 뒤에만 size/hash identity pin을 갱신한다.

현재 pin은 단순 snapshot이 아니라 “현재 생산자 identity + 현재 소비자 계약”을
묶는 회귀 증거다. 생산자가 강화됐는데 소비자와 gate가 함께 이동하지 않으면 CI가
조용히 죽어 있는 상태로 취급한다.

## self-host role receiver가 builtin target에서 nominal-kind 누락으로 실패하는 경우

증상이 `role receiver target nominal-kind fact is missing`이고 선언이
`role IntMath for Int` 같은 형태라면, `Int=nk:struct` 같은 행을 추가하지 않는다.
`Int`는 nominal 선언이 아니라 compiler ABI row가 소유하는 plain value다. 이
실패는 두 개의 서로 다른 carriage를 하나로 취급했을 때 발생한다.

- erased role method ABI는 stable address를 받으므로 `mutable-identity`다.
- 그 주소 뒤의 concrete target은 `Int`라면 `value`, `subject`라면
  `mutable-identity`다.
- callable identity admission이 target type과 concrete target carriage를 함께
  봉인해야 한다. Function emitter가 role table을 다시 훑거나 binding owner가
  generic type environment의 `nk` row를 조회하면 같은 결함이 재발한다.
- builtin, enum, source nominal authority가 같은 이름을 동시에 주장하면
  ambiguous로 실패하고, 누락되거나 non-copyable builtin이면 C를 내기 전에
  실패한다.

Focused gate는
`make self-host-codegen-role-receiver-admission-test-smoke`다. 설치된 Pergyra
sibling이 현재 codegen tool을 만들고, `operator_add.pgy`를 C로 내린 뒤 exact
`123/123/3`, body mutation `321/321/3`, non-copyable target rejection을 확인한다.

Windows에서 설치 driver에 `source --emit-c-verified` 두 인자를 주면 옛
`driver_bootstrap_main.pgy`는 artifact transaction을 먼저 선택해
`--emit-c-verified`라는 파일을 만들었다. `source output.c` 역시 효과를 arity에
숨기고 다음 옵션을 파일명으로 오인할 수 있으므로 폐기했다. 현재 계약은 다음처럼
서로 겹치지 않는다.

```text
source.pgy --emit-c-verified
    -> stdout
--emit-c-artifact-verified source.pgy output.c
    -> atomic file publication
```

missing output, old positional form, unknown second option, option-shaped output
path는 source/MIR read와 artifact begin 전에 실패한다. Native public `--emit-c`와
compile/run은 사용자 문법을 바꾸지 않고 새 artifact mode를 child argv로 전달한다.
2026-08-03 공식 installed-driver rebuild는 109.2초였고, focused CLI gate, public
C emit, default C compile/run, 17 grammar fixture, component ratchet이 모두 green이다.
이번 빌드에는 pressure 측정을 붙이지 않았으므로 이전 메모리 peak를 재사용하지
않는다.

아직 전체 argv SoT가 닫힌 것은 아니다. `--emit-mir-json-verified source third`와
`--mir-json input third`는 installed root에서 output, standalone root에서 machine
declaration으로 해석된다. 다음 rung은 argv 전체를 I/O 전에 한 번 typed request로
admit해 이 이중 의미를 제거한다. 또한 PowerShell의 `>`는 native stdout을 UTF-16LE로
저장할 수 있으므로 생성 C 캡처는 Git Bash redirection 또는 명시적 byte/UTF-8
writer를 사용한다.

## public `--mir-json` 치환 뒤 parity가 자기 자신을 oracle로 비교하는 경우

2026-08-03의 `ced304fb`부터 정확한 public
`pgy --mir-json <source>` 요청은 native `driver_run_pipeline -> mir_dump_json`으로
들어가지 않는다. `src/pgy_driver.c`의 selector가 설치된 sibling
`pgy-self-driver --emit-mir-json-verified <source>`를 한 번 실행한다. Sibling이
없거나 요청에 지원 밖 runtime/output/dump 옵션이 섞이면 native pipeline으로
retry하지 않고 명시적으로 실패한다.

이 변경 직후 가장 위험한 가짜 green은 기존 parity가 public `--mir-json`을 C
oracle이라고 계속 부르는 것이다. 그러면 self producer와 같은 self producer를
비교하게 된다. Native producer가 필요한 테스트는 반드시 다음 test-only 경계를
사용한다.

```text
pgy --test-native-mir-json-oracle <source>
```

이 옵션은 일반 fallback이 아니다. Public mode와 함께 지정하거나 source-MIR exact
contract 밖 옵션을 섞으면 거부된다. 기존 native MIR carrier 테스트를 이 경계로
옮긴 이유는 oracle 독립성뿐 아니라 clean bootstrap의 순환 의존을 막기 위해서다.
`self-host-codegen-bootstrap-seed-test-smoke`가 아직 만들지 않은 installed sibling을
public producer로 요구해서는 self driver를 처음부터 만들 수 없다.

Focused gate는
`tests/self_hosted/parity/public_mir_json_installed_self_host_owner.sh`다. Mixed
generic+intent fixture에서 public/self 46,727-byte equality, frozen-native/self
canonical equality, rejected source diagnostic equality, missing sibling, unsupported
option, mixed public/test mode를 확인한다. 이 gate는 다른 shell test가 public
`--mir-json`을 다시 native oracle로 사용하는 경우도 거부한다. 이어 실행한 13-row
live matrix, hard contract, component/removed-path ratchet도 green이었다.

같은 작업 중 PowerShell에서 Git Bash를 통해 MSYS Make를 실행하자 GCC temporary
path가 `C:\Windows`로 떨어져 permission error가 났다. MSYS2 bash를 직접 사용하되
`PATH=/ucrt64/bin:/usr/bin`과 repository-local `TEMP`/`TMP`/`TMPDIR`을 함께 주어야
한다. Toolchain이 잠시 `CC_MACHINE=unknown`으로 관측되면 configuration stamp가
기존 object cache를 무효화하므로, 같은 명령을 무작정 반복하지 말고 남은 object
수를 확인한 뒤 올바른 MSYS2 환경에서 증분을 재개한다. 이번 재개는 `-j4`로 남은
오브젝트와 link를 19.8초에 끝냈지만 pressure 표본은 아니므로 메모리 최대값으로
기록하지 않는다.

## public `--emit-llvm -o`가 native libLLVM으로 되돌아가는 경우

`279d0646`부터 exact file form은 installed source-MIR producer와
`--mir-json-backend=llvm` projector를 각각 한 번 실행한다. 일반
`driver_run_pipeline`이나 `compiler_emit_llvm_ir_to_file`은 이 selector의 fallback이
아니다. Runtime/debug/dev/json-diagnostic처럼 아직 admit하지 않은 옵션은 native로
넘기지 않고 selector에서 실패한다.

Final path에 직접 생성하면 projector 실패가 옛 `.ll`을 남기거나 partial file을
노출할 수 있다. 현재 publication owner는 output directory 안의 private workspace에
MIR과 LLVM artifact를 완성한 뒤 성공한 `.ll`만 rename한다. 실행 전에 stale final을
지우며 producer/projector failure 뒤에는 final artifact가 없다. Source와 output의
spelling이 같으면 source를 지우기 전에 fail closed한다.

Focused gate는
`tests/self_hosted/parity/public_llvm_ir_installed_self_host_owner.sh`다. Public/direct
LLVM byte equality, host clang exact `7/11/5`, producer/backend exact-once,
missing/unsupported/rejected action, stale output, source/output identity, native timing
부재를 확인한다. 이 file-form green 하나만으로 stdout까지 치환됐다고 과장하지
않는다.

후속 `8bc7f525`에서 stdout도 같은 installed producer/projector로 넘어갔다.
Projector 성공 전에는 stdout을 쓰지 않고, 완료된 private `.ll`을 Windows binary
mode의 16 KiB 고정 버퍼로 전달한다. `path_read_file`이나 whole-artifact allocation을
추가하면 이전 메모리/복사 결함을 되살리므로 금지된다. Focused stdout gate는
file/stdout byte equality, clang exact `7/11/5`, exact-once와 failure zero-payload를
검사한다. 이제 둘 다 bounded substitution이지만 general CFG coverage는 별도다.
