# Current Work Handoff

Updated: 2026-08-27 (Asia/Seoul)

This file is a resume snapshot, not semantic authority. Verify it against the
current source, `git status --short --branch`, the SoT registries, the named
owner, and the named executable gate.

Concurrent work must first read
`docs/current_work_collaboration.md`. Its top non-`DONE` edit lease prevents
two Codex tasks from editing or publishing the same executable rung; it is a
coordination aid, not completion evidence.

Project-wide progress is tracked separately in `docs/00_progress.md`. The
2026-08-27 evidence-reconciled working forecast is 83% (81-85% range) for
language beta plus SoT, self-host, bootstrap, and CI/release together; strict
language beta remains at the separately owned official 83% line. V numbers,
`.tmp` artifacts, owner count, and gate count do not increment either
percentage by themselves.

## Active self-host context - source-C closure reconciled; successor not inferred

- Closed predecessor: formal/intent callable identity implementation
  `9454f9fe`, closure checkpoint `5d7740ce`, and exact-head run
  `33064629767` are published and GREEN 29/29. The run includes `build-linux`,
  full self-host fixed point, codegen bootstrap, 20 backend shards,
  Windows/macOS, sanitizers, TSan, and Rocq. The prior gen0, hard-contract, and
  likeness failures are therefore closed at the exact published revision.
- Objective card: make public installed source-to-C compilation execute one
  real-purpose Pergyra intent instead of directly publishing through a world
  method. Priority is one semantic owner, typed outcome carriage, direct-bypass
  deletion, intent trace, negative ratchet, then patch size.
  `DriverSourceCExecution` and `DriverSourceCExecutionOutcome` own compile,
  transaction, and typed result facts; `DriverRung2InstalledPublishSourceC` is
  the last orchestration consumer. Duplicating compile/commit logic in an
  intent wrapper, keeping direct publish beside the intent, native retry, and
  target-name inference are forbidden.
- Production entrypoint and direct bypass: public
  `pgy SOURCE --emit-c -o OUTPUT` dispatches the installed
  `DriverCliSourceCArtifact` request into
  `DriverRung2InstalledPublishSourceC`, which at baseline called
  `PublishSourceCArtifactThroughPgyCompilerWorld` ->
  `PgyCompilerWorld.PublishSourceCArtifact` ->
  `DriverSourceCExecution.PublishSourceCArtifact`. Source-to-LLVM already uses
  a real-purpose intent; this source-C chain is the reached direct bypass.
- Verification/falsifier: `driver_source_c_execution_action_gate.sh` owns
  installed/public byte parity, compiled runtime intent/trace output, and
  no-artifact transaction rejection. Tighten it so the old world/composition
  publish names cannot return and one successful `CompilePergyraCArtifact`
  trace is mandatory. Installed-driver evidence must then pass the existing
  source-C action gate and bootstrap/source-scan integration gates.
- Scope guard: the attached identity-algebra review is an audit lens, not
  authority to open a general query engine, cache, O(n^2) epoch rewrite, or
  performance track. Those remain unopened unless the selected executable rung
  reaches them as its exact blocker. Protected untracked
  `docs/compiler_architectures/` and `pgy-80135c2c/` remain untouched.
- Local implementation `fb4acef4`: `DriverSourceCExecution` now stores one typed outcome
  and its `Compile` action is the single step of `CompilePergyraCArtifact`.
  `PgyCompilerWorld.CompileSourceToC` cross-checks Bool completion against that
  outcome; the old world/composition publish methods are deleted. The installed
  consumer requires one successful canonical intent history row. The first
  isolated self-build exposed a misplaced trace block in the next source-MIR
  function; the repaired focused gate now verifies trace locality inside the
  source-C consumer itself.
- Publication checkpoint `20e7da6e` reached exact-head run `33068411554`, which
  completed 28/29: every platform, backend shard, sanitizer, proof, Linux fast
  build, and codegen-bootstrap job passed. The sole failure was full self-host
  native oracle emission: the imported intent was private, then exposing it
  revealed MIR-only inference treating two enum variant constructors as
  implicit `DriverSourceCExecution` methods.
- Repair `cb53b879` makes the cross-module intent explicitly `public`, routes
  success and artifact-failure construction through the protocol owner, and
  teaches the world/likeness gates to recognize exported/public intents. The
  exact failing native command now emits and compiles the integrated oracle;
  that oracle publishes `hello.pgy`, whose C compiles and runs exact
  `Hello, Pergyra!`. Focused source-C parity/negatives, topology, native world,
  source scan, likeness (`4393/4393`, intent `15/15`, zone-bound `37/37`), and
  hard contract are green. Compiler-world and hard-contract exceeded the
  60-second static budget but completed green; full component inventory did
  not run locally and is not claimed from that local run.
- Exact-head closure: documentation checkpoint `c0632e4f` run `33071044311`
  completed GREEN 29/29 in 34m32. `build-linux`, the full self-host fixed point
  and policy corpus, codegen bootstrap, all 20 backend shards, Windows/macOS,
  sanitizers, TSan, and Rocq passed. This closes the source-C intent takeover
  and its bootstrap repair at the published revision.
- Successor admission: the public launcher, REPL compiler call, and package
  compiler path have no remaining implicit `driver_run_pipeline` call. The
  remaining native calls are explicit oracle/opt-out paths, while package
  manifest parsing and unsupported RIR/AIR/HIR surfaces do not yet have a
  complete Pergyra owner. Do not manufacture a source-MIR intent conversion,
  query/cache layer, epoch rewrite, or unrelated SoT cleanup as the successor.
  A new rung may open only after a fresh production compiler bypass, its
  existing complete Pergyra owner, last consumer, and executable falsifier are
  all named. Registry and progress remain `50 CLOSED / 35 BRIDGE / 1 ACTIVE`,
  hard closure 58.1%, migration 78.8%, integrated 83% (81-85%), strict beta
  83%, and hard replacement 75%.
- Local artifact structure: the 23 root `bin-codex*`/`build-codex*`/
  `bin-dev*`/`build-dev*` directories remain Git-ignored, untracked,
  rebuildable output (about 126 MiB total), and ignore checkpoint `1e8b5531`
  already closes their Git status. Exact-path, workspace-parent, reparse-point,
  and ignore validation passed, but the current execution policy rejected the
  recursive deletion before the process started; zero directories were
  removed. A user-run `mingw32-make clean-local-variant-artifacts` removes only
  those top-level variants. The heavier `clean-local-artifacts` also removes
  active `build/`, `bin/`, and `.tmp/` and is not implied by this handoff.

## Completed self-host context - native formal and intent callable identity carriage

- Published base: implementation `d437e9e8`, documentation checkpoint
  `b2f9a5ca`, and exact-head run `33045433992` are GREEN 29/29. Formal/intent
  identity implementation `e1ad082f` and checkpoint `deac496a` are published,
  but exact-head run `33053920579` failed in four common self-host jobs. Native
  gen0 rejected reserved local name `intent` in the carried-callable identity
  owner. Local repair `2c052d42` uses `intent_index` and updates the bootstrap
  partial-parameter mutation for the complete routine-ID schema. Protected untracked
  `docs/compiler_architectures/` and `pgy-80135c2c/` remain untouched.
- Objective card: make formal-parameter identity and declared-callable identity
  distinct, owner-directed facts all the way from native parser production to
  installed self-C consumption. Priority is exact identity domains, complete
  carriage, intent/function join, missing/crossed failure, negative ratchet,
  then patch size. Parser `FuncParam.stable_id` owns the formal declaration;
  admitted function/intent signature facts own declared callable identity;
  `RewriteSemanticIdentityBoundCall` is the last consumer.
- Production entrypoint and observed RED chain: native
  `pgy --test-native-mir-json-oracle
  tests/self_hosted/parity/fixture/intent_typed_outcome_compensation.pgy`, then
  installed `pgy-self-driver --mir-json`. Exact declared call carriage first
  reached a missing formal `self` identity, positive formal rows then exposed
  the final resolver's function-only `RunWorkflow` lookup, and the corrected
  resolver finally exposed a missing intent declaration-ID C environment row.
  These are consecutive falsifiers on one executable rung.
- Owner/carrier/consumer: `ast_assign_stable_ids` appends parameter declaration
  IDs without renumbering existing AST nodes; semantic symbols retain that ID;
  MIR routine params and 11-field expression nodes carry it; canonical semantic
  re-entry joins function and intent declarations exactly; `BuildFunctionEnv`
  publishes the admitted intent ID under
  `@declared_callable_syntax:<SyntaxNodeId>`. `TypeId` substitution, source-name
  MIR repair, target-only acceptance, copied IDs, dual reads, and native retry
  are forbidden.
- Fresh v23 evidence: clean LLVM-disabled native build is green and a same-path
  `make -q` is up to date. The 207,321-byte oracle has 12 positive unique
  routine parameters, 451 expression nodes, and 39 exact formal leaf
  ID/kind/ordinal matches. `Observe` and `RunWorkflow` call/leaf declaration
  IDs match. Installed driver SHA-256
  `B7BAE50CDD5992CB290D84EE36D4A2CF2940920D0DA998FD3ED264C64A58B305`
  emits 23,779-byte C; compiled execution matches the exact 32-line v3 oracle.
  Twelve digest/function/intent/formal mutations fail before partial C output.
  Stable-identity and source-scan gates are green.
- Local omissions are explicit: the complete v3 script reaches its native LLVM
  leg and stops because v23 was built with `LLVM_ENABLED=0`; all preceding self
  admission/runtime and native C compilation pass. The parameter-carriage gate
  passes the new positive/unique identity assertion but its unrelated diagnostic
  JSON tail is not green under that isolated binary. Complete component
  inventory exceeded the 60-second budget and is not claimed green. After
  `2c052d42`, native gen0 parse reports `0 error(s), 0 warning(s)` and
  gen2==gen3 at 73,161 lines. The first repaired bootstrap run then exposed its
  stale partial-ID mutation; the corrected mutation removes one real routine
  parameter ID and both self/oracle MIR lower reject it with exit 1 and the
  owned partial-carriage diagnostic. Full codegen bootstrap then completes
  lexer/parser/semantic/MIR lower/tool/fuzz oracle parity with
  `SELF-HOSTING OK`; source-scan and documentation gates are green. Publication
  run `33055970238` made 27 jobs green. `build-linux` passed all earlier
  execution gates and failed only because hard-contract still required three
  pre-routine-identity expression graph call strings. Full self-host then
  completed green, making the run 28/29 with only `build-linux` red. Local
  repair `5c722a6f` updates those ratchets, and the complete focused
  hard-contract exits 0. Publication and replacement exact-head CI are the next
  falsifiers. Replacement run `33058636093` then completed 28/29 with full
  self-host and every non-`build-linux` job green. `build-linux` reached only
  likeness, where the consolidated lookup had fallen to result-use 4372/4374
  by encoding invalid/missing/found as raw Int sentinels. Local repair
  `9454f9fe` returns `Result<Int>` with `Err`, `Ok(0)`, and exact
  `Ok(SyntaxNodeId)` outcomes; runtime ABI fallback is reachable only from the
  valid miss. Likeness is 4385/4385 with sentinel 23, native gen0 parse is 0/0,
  gen2==gen3 is 73,172 lines, and full codegen bootstrap reaches
  `SELF-HOSTING OK`. Complete component inventory again exceeded 60 seconds and
  is not claimed green. Implementation/checkpoint `9454f9fe`/`4a1261ec` are
  published, and exact-head run `33061911002` completed GREEN 29/29. Its
  `build-linux` and full self-host jobs both passed, so the typed lookup,
  routine-aware hard-contract, and native gen0 compatibility repairs are
  remotely closed at the exact published revision.
- Build reliability note: the earlier segfault came from reusing one build
  directory with relative and `/d/...` absolute spellings, so included `.d`
  targets did not name the same object target. It is not recorded as a general
  CI dependency failure. v23 uses one absolute spelling throughout.
- No query/cache, O(n^2) epoch replacement, broad identity algebra rewrite, or
  performance track was opened by this rung. Registry and progress remain
  `50 CLOSED / 35 BRIDGE / 1 ACTIVE`, hard closure 58.1%, migration 78.8%,
  integrated 83% (81-85%), strict beta 83%, and hard replacement 75%.
- Closure checkpoint: tracked state was clean at `4a1261ec` and only the two
  protected untracked paths remained. Exact-head run `33061911002` verified all
  29 jobs. Resume from the active selection card above rather than reopening
  this completed identity-carriage rung.

## Completed self-host context - intent-phase declared callee binding identity

- Published base: declared-callable C alias implementation `c72ba209`,
  documentation checkpoint `5be3a3ee`, and closure checkpoint `d9849204` are
  published. Exact-head run `33041466890` at
  `5be3a3eee67e2d7f1f85579bc0f30f3034e7aa95` is GREEN 29/29. Intent callee
  implementation `d437e9e8` is local and exact-head CI is not yet claimed.
- Objective card: make intent-owned phase expression graphs carry the same
  resolved declared-callee SyntaxNodeId on the leaf and call target before MIR
  publication. Priority is exact semantic identity, producer-side carriage,
  missing/crossed failure, negative ratchet, then patch size.
- Production entrypoint and baseline RED: installed
  `pgy-self-driver --emit-mir-json-verified
  tests/self_hosted/parity/fixture/intent_typed_outcome_execution.pgy`, followed
  by installed `--mir-json`, currently exits 1. The persisted
  `IntentRunAccepted` call carries target ID `56`, while its callee leaf carries
  `binding_syntax_id:0` and `binding_kind:none`; semantic re-entry reports the
  exact `0/56` binding mismatch before C publication.
- Missing fact, owner, and last consumer: function signatures own declaration
  SyntaxNodeId. `SemanticAstAnalysisResolveExpressionIdentities` resolves a
  declared leaf only inside its `IsSome(function_node)` branch, so an
  intent-owned surface with an `intent_node` but no function owner never gets
  `SemanticExpressionBindingDeclaredCallable` carriage. The semantic
  expression graph is the producer; MIR JSON projection is only a carrier, and
  semantic re-entry is the last verifier. Name-based MIR repair, raw target-ID
  copying without an exact callee edge, native graft, and a dual read are
  forbidden.
- Falsifier: `generic_specialization_identity_epoch_owner.sh` must execute exact
  `accepted=true`, `calls=1`, `rejected=false`, `calls=2`, retain its invalid
  generic ordinal no-artifact rejection, and add a declared-callee binding
  missing/crossed rejection before C publication. The same RED is observed on
  pre-change v19, v20, and the currently installed driver, so it is a real
  baseline producer gap rather than a regression from `c72ba209`. No query,
  cache, O(n^2), profiling, or unrelated SoT track is open.
- Local implementation `d437e9e8` lets declared-leaf identity resolution run
  on either a function- or intent-owned semantic surface, while keeping formal
  parameter ordinal resolution function-only. The exact declared callee
  SyntaxNodeId therefore reaches the intent phase graph before MIR publication;
  no MIR/name repair or target-only acceptance was added. `OWNERS.md` records
  the function/intent surface boundary.
- Fresh v21 evidence is GREEN: codegen gen2==gen3 at 73,145 lines; integrated
  driver seed/oracle, bounded MIR/C, and consumer parity; mixed intent/generic
  runtime exact `accepted=true`, `calls=1`, `rejected=false`, `calls=2`; invalid
  generic ordinal plus missing/crossed callee binding no-artifact negatives;
  callable, namespace, canonical-epoch, typed-intent, and phase-carrier
  regressions. Source scan, likeness (`23/23`, Result/Option `4374/4374`), Bash
  syntax, gate cap `100/100`, and diff checks are green. The complete component
  inventory exceeded its 60-second local budget and is not claimed green.
  Registry census and progress remain `50/35/1`, 58.1%, 78.8%, integrated 83%
  (81-85%), strict beta 83%, and hard replacement 75%. The next falsifier is
  exact-head CI after the documentation checkpoint is published.

## Completed self-host context - declared callable C alias identity

- Published base: formal callable SyntaxNodeId implementation `e4a14e7b`,
  failure-owner import repair `0fb875bc`, namespace gate cap repair `8e8cd8cb`,
  and checkpoint `e7c27b68` are published. Exact-head run `33038171342` at
  `e7c27b6835d0313d5625c712ce8c1fe3a6107333` is GREEN 29/29. The formal
  callable display-text lease below is closed.
- Objective card: make an already resolved declared callable's canonical
  declaration SyntaxNodeId own its final C alias selection. Priority is exact
  identity, one internal declaration-ID key, fail closed on missing/crossed
  identity, old name-read deletion, negative ratchet, then patch size.
- Production entrypoint: installed `bin/pgy.exe SOURCE --backend=c -o EXE` for
  the bounded direct-call/namespace fixtures. Inspection corrected the direct
  bypass: declared calls branch from `RewriteSemanticDirectCall` into
  `RewriteSemanticIdentityBoundCall`, where `call_symbol` and `binding_key`
  were initialized from `source_name` before the final `LookupKindType` read.
  The general `RewriteSemanticCall` alias path is not this rung's consumer.
- Fact owner and last consumer: the semantic signature fact owns declaration
  SyntaxNodeId and `SemanticExpressionGraphCalleeBindingFact` carries it on the
  resolved callee. `BuildFunctionEnv` may publish one internal declaration-ID
  key; `RewriteSemanticIdentityBoundCall` is the last legitimate consumer.
  Display spelling, canonical-name lookup, name/ID dual reads, and native retry
  are forbidden fallbacks for this slice.
- Falsifier: installed/direct-driver C and LLVM direct-call plus namespace
  runtime parity must stay exact; missing, forged, and cross-wired declaration
  IDs must fail before artifact publication; a static negative must reject the
  old `source_name` carrier in the identity-bound emitter. No query/cache,
  O(n^2) epoch, or performance track is open.
- Local implementation `c72ba209` publishes non-generic callable C aliases
  under `@declared_callable_syntax:<declaration SyntaxNodeId>`. The final
  identity-bound emitter now reads the formal parameter key, then for declared
  calls the admitted generic call-node specialization key or the declaration
  key; missing rows fail closed. It has no `source_name` parameter or name
  fallback. The key encodings are co-owned by the function-binding environment
  instead of the callable-parameter row owner.
- Fresh v20 evidence is GREEN: codegen gen2==gen3 at 73,145 lines, integrated
  driver seed/oracle and bounded MIR/C parity, callable identity C/LLVM with all
  20 mutations, namespace-internal C/LLVM with four identity mutations, and
  canonical identity epoch positives/negatives. The native manifest owner
  generated the required sibling for the isolated v20 driver; public launcher
  C/LLVM then executed exact `16\n13\n6` and `namespace:internal-ready`.
  Generic-default source-to-C executed exact `save=9\nbox=7`. Source scan,
  likeness (`23/23`, Result/Option `4374/4374`), shell syntax, and diff checks
  are green. The complete component inventory exceeded its 60-second local
  budget and is not claimed green.
- `generic_specialization_identity_epoch_owner.sh` still rejects
  `IntentRunAccepted` at semantic admission before reaching this C alias
  consumer; the same diagnostic occurs with the pre-change v19 driver, so it
  is recorded as baseline evidence rather than a passing or regressing gate.
  Exact-head run `33041466890` at `5be3a3ee` completed GREEN 29/29:
  `build-linux`, full self-host fixed point, codegen bootstrap, all 20 backend
  shards, Windows/macOS, sanitizers, TSan, and Rocq succeeded. This lease is
  closed. Census and progress stay `50/35/1`, 58.1%, 78.8%, integrated 83%
  (81-85%), strict beta 83%, and hard replacement 75%; the active card above
  owns the next executable identity gap.

## Completed self-host context - formal callable codegen binding identity

- Closed predecessor: implementation `b80bc803` and checkpoint `ae8b1341` are
  published. Exact-head run `33032356735` at
  `ae8b13413647be1693ea442523d07f2698ae104f` is GREEN 29/29, including
  `build-linux`, self-host codegen bootstrap, full fixed point, all 20 backend
  shards, Windows/macOS, sanitizers, TSan, and Rocq. The callable-parameter
  installed-path substitution lease is closed.
- Objective card: remove display spelling as the final C binding authority for
  an already resolved formal callable. Canonical parameter SyntaxNodeId owns
  the binding; `SemanticExpressionBindingIdentityFact` carries it, and the
  function-local codegen environment may serialize an internal SyntaxNodeId key
  only as a lookup carrier. Priority is exact identity, delete the name read,
  fail closed on missing/crossed identity, negative ratchet, then patch size.
- Production entrypoint: installed `bin/pgy.exe SOURCE --backend=c -o EXE` for
  `compose_two_functions` and `callable_parameter_builtin_shadow`. The direct
  bypass to delete is formal-call lookup through
  `LookupKindType(env, source_name, "call")` plus the callee display-text
  equality in `RewriteSemanticIdentityBoundCall`.
- Fact owner and last consumer: `SemanticAstFunctionParamNodeAt` owns the
  canonical formal parameter SyntaxNodeId; `CodegenCallableParameterBindingRows`
  may carry its C binding into the function-local environment; the last
  legitimate consumer is `RewriteSemanticIdentityBoundCall`. Source spelling,
  binding ordinal alone, global-name precedence, and a dual name/ID lookup are
  forbidden fallbacks.
- Falsifier: the v18 callable C/LLVM gate must retain exact `16\n13\n6`, the
  builtin-shadow program must retain exact `6`, and all 20 missing/forged/
  cross-wired identity mutations must fail before publication. A static
  negative must reject formal lookup by `source_name` or callee node text.
  LLVM already consumes the sealed direct-MIR identity and is a parity guard,
  not a second implementation scope. No parallel edit lease is open.
- Local implementation `e4a14e7b` emits the callable `call` row under internal
  key `@binding_syntax:<canonical parameter SyntaxNodeId>`, deletes the
  name-keyed formal row, and makes the final C emitter consume that key. The
  emitter no longer reads callee node text; a missing identity-keyed row fails
  before C call publication. Fresh v19 compiled a 6,459,372-byte Pergyra seed
  and a 7,111,994-byte oracle. Callable C/LLVM plus all 20 mutations,
  namespace-internal C/LLVM plus negatives, canonical epoch, likeness
  sentinel `23/23`, and Result/Option `4374/4374` are green. The complete
  component inventory and remote CI are not yet claimed green. Do not pass a
  drive-letter or repo-relative build directory to the focused bootstrap
  wrapper; use its default or a Git-Bash absolute `/d/...` path.
- Exact-head run `33035298360` reached 27 successful jobs and 20/20 backend
  shards, but `build-linux` failed in the complete component inventory because
  `callable_parameter_binding_rows_owner.pgy` directly called `Die` without
  importing the owned `text_owner.pgy`. The local repair adds that explicit
  import; the direct-consumer scan, source scan, Bash syntax, and likeness
  `23/23` plus Result/Option `4374/4374` are green. The complete component
  inventory exhausted the 60-second local budget and is not claimed green;
  replacement exact-head CI remains the falsifier.
- CI status handling rule: a job disappearing from a `status != completed`
  view proves only completion, never success. Read its explicit `conclusion`
  before reporting green. This session briefly misreported `build-linux` as
  successful from omission, then corrected it from the explicit conclusion and
  job log; do not repeat that inference.
- Replacement run `33037083062` proved the missing import repair by advancing
  past that check, then found the next static ratchet: the updated namespace
  parity gate was 165 lines against its existing 160-line cap. Repair
  `8e8cd8cb` keeps the SyntaxNodeId lookup and display-text negatives intact
  while removing five layout-only lines; the file is exactly `160/160`, Bash
  syntax is green, and its namespace C/LLVM parity plus negative execution is
  green. Do not raise the cap. At that checkpoint another exact-head run
  remained required.
- Exact-head run `33038171342` at
  `e7c27b6835d0313d5625c712ce8c1fe3a6107333` completed GREEN 29/29:
  `build-linux`, full self-host fixed point, codegen bootstrap, all 20 backend
  shards, Windows/macOS, sanitizers, TSan, and Rocq succeeded. This lease is
  closed; the active card above owns the next executable identity seam.

## Completed self-host context - callable-parameter public substitution

- Callable implementation `30b84f80aaf13a8479b533a931ef115dfcea5905`, lifetime
  repair `f6d6fb4b90445d788c90e546482742e18cf5c2fa`, native-MIR repair
  `024d1ba7f858b09802d97bc0372c29deaa440745`, fixture-scope checkpoint
  `dc7be82f6cc8da0e6d2427c405101cbf262591bd`, and Linux mutation repair
  `5f73970168b45252b8c6637691e7ef363e8304b3`, canonical expression-identity
  repair `1d4590364e32bb4708659e609cc6a96e6b23b318`, and stable gate-identity
  ratchet `e070fcec17da1f4dd3f6ea33014e5e5cca5955f2`, non-monotonic epoch repair
  `dfbe9b0a1dc224db0ba95193520c264a9c80933f`, and CI preparation checkpoint
  `5d23fdda1be2cf7cf87720eed31f73416bd5dbcc` are published. Namespace-internal
  canonical callable carriage checkpoint `9ab03311` and documentation
  checkpoint `c31da1d2`, Linux cap repair `6fa362c5`, nested receiver-identity
  repair `af91687d`, and checkpoint `3e8a3567` are published. Identity-policy
  caller ratchet `a5ecff34`, checkpoint `9bf511d5`, typed binding-ordinal repair
  `b80bc803`, and closure checkpoint `ae8b1341` are published. Exact-head run
  `33032356735` is GREEN 29/29. The unrelated user-owned
  `pgy-80135c2c/` and
  concurrent `docs/compiler_architectures/` paths remain untracked and must not
  be inspected, staged, deleted, or rewritten.
- Objective card: let canonical `func(T...) -> R` declarations and values retain
  one syntax/semantic identity through source-to-MIR, then make installed public
  C and LLVM execute the same callable program without native retry. `ReadType`,
  semantic callable signature/expression facts, and MIR target/binding
  SyntaxNodeIds own the chain; spelling-based backend recovery is forbidden.
- Installed `bin/pgy.exe SOURCE --backend=c -o EXE` now reaches Pergyra
  source-to-MIR, semantic re-entry, and the self-C identity-bound call emitter.
  Installed `--backend=llvm -o EXE` reaches source-to-MIR and the direct-MIR
  GraphPlan LLVM consumer. Explicit native C is used only as a runtime-output
  oracle; native/self MIR byte identity is not claimed because their parameter
  source-identity projections intentionally differ today.
- `compose_two_functions` executes exact `16\n13\n6` through both public
  backends. `callable_parameter_builtin_shadow` executes exact `6`, proving a
  formal named `StringLength` does not fall into builtin name dispatch. The
  focused gate's 20 missing/forged/cross-wired mutations fail before artifact
  publication.
- A fresh release self-host compiler was built and installed. The focused
  callable gate, separate installed-public gate, full component/source-MIR
  inventory, and the post-build installed gate are local GREEN. Remote run
  `33000341546` rejected five fresh-build jobs: a new prototype helper
  illegally accepted `TextBuilder`, which local fingerprint reuse had hidden.
  Repair `f6d6fb4b` returns `Option<String>` and adds a negative structural
  ratchet. A fresh isolated gen2 seed, fresh DRV-2 install, focused and public
  callable gates, and the complete component/source-MIR gate are green after
  repair.
- Replacement run `33002949085` then exposed two later seams. Native
  `mir_json_dump.c` had serialized `fp->type->stable_id` as parameter identity,
  producing partial identity for role `Add(self, rhs)`; three bootstrap/aggregate
  jobs failed there. All 20 backend shards independently stopped at inventory
  because the self-host-only builtin-shadow fixture was under
  `tests/cases/backend_compare` without native-default registration.
- Repair `024d1ba7` deletes the false native field, permits only complete unique
  or wholly absent identity at the MIR-to-AST breadth owner, and adds a partial-
  identity negative to full codegen bootstrap. That full bootstrap is local
  GREEN through `role_operator_dispatch`. Checkpoint `dc7be82f` moves the
  shadow fixture under `tests/self_hosted/fixtures`; backend inventory and both
  callable gates are GREEN.
- Run `33005863688` passed Windows, macOS, toolchain, TSAN, Rocq, and all 20
  backend shards before the push superseded its three still-running long jobs.
  Its sole failure was the new codegen negative: first-occurrence `sed` changed
  a declaration row on Linux but left routine parameter identity wholly absent,
  so the compiler correctly admitted it. Repair `5f739701` changes all matching
  rows. The exact three-row global mutation makes both self-built and oracle
  mir_lower fail with `identity carriage is partial`; script syntax is GREEN.
  Run `33006827756` was superseded. Final-head run `33007078796` passed 27/29,
  including all 20 backend shards and the full codegen bootstrap, then exposed
  producer-vs-canonical callable identity epoch drift in `build-linux` and the
  full driver bootstrap. The first exact row was `JsonCharCodeAt()` with source
  call/callee ID `17` compared against a different reconstructed signature ID.
- Repair `1d459036` moves the exact routine/parameter identity join behind
  `MirExpressionIdentityEpoch` and makes the MIR expression graph consume only
  canonical IDs before semantic admission. It also distinguishes persisted
  lanes from the existing producer-only collection receiver bridge: persisted
  facts remain exact, while producer-only rows must carry neutral identity and
  are filled once by the canonical semantic owner. Numeric offsets, dual-epoch
  reads, name-only admission, and native fallback remain forbidden.
- Fresh v7 local evidence is GREEN: the callable C/LLVM gate executes exact
  `16\n13\n6` and rejects 20 mutations; the canonical method/topology epoch
  gate passes; full `src/self_hosted/mir_lower/main.pgy` semantic re-entry emits
  a 1,975,383-byte C artifact; and the complete component contract gate passes.
  The canonical-epoch stable pass marker and SoT edge census are GREEN at
  `e070fcec`.
- Replacement run `33016014561` passed 27/29 and proved the prior callable row
  no longer fails. `build-linux` then found grammar 04's non-monotonic routine
  source IDs (`9,22,44,35`) and stale generated language-word inventory. Full
  bootstrap found the same epoch owner requiring reconstructed intent
  participants to equal the MIR routine's intentionally empty formal rows.
- Repair `dfbe9b0a` performs sorted exact-pair insertion, rejects source and
  canonical ID duplication, and keeps intent participants under the admitted
  intent execution plan while requiring zero intent routine formal rows. Fresh
  v9 local evidence is GREEN for callable C/LLVM plus 20 negatives, the
  canonical epoch gate, all 17 manifest-verified grammar examples, full
  `mir_lower` C emission (1,975,383 bytes), the language-word registry, and SoT
  edge. The post-repair full component rerun exceeded the five-minute focused
  budget and was stopped, so it is not claimed green. A new remote replacement
  run is the next falsifier.
- Exact-head run `33019529720` at `5d23fdda` passed 27/29. All 20 backend
  shards, Windows, macOS, sanitizer, TSan, Rocq, toolchain, and codegen
  bootstrap were green. `build-linux` found one stale static expectation for
  the identity-preserving expression graph constructor. Full bootstrap reached
  `PathCharAt()` and rejected a namespace-internal short spelling whose call
  target carried canonical `__imp0_SelfHostPath_PathCharAt` identity while its
  leaf still used the local display spelling.
- The repair makes callable-index canonical target name plus exact
  call/callee SyntaxNodeIds the declared-call identity. Direct declared calls
  require exact leaf carriage; namespace calls require the persisted
  member-access topology and canonical target ID. Formal callable spelling
  remains checked, but declared call admission and C/LLVM consumption cannot
  recover identity from leaf display text. Missing and crossed callee binding
  mutations now fail before artifact publication.
- Fresh v16 evidence is green for namespace-internal C/LLVM execution and four
  target/binding negatives, callable C/LLVM execution and all 20 negatives,
  canonical identity epoch positives/negatives, and program-graph unification.
  The same source produced a 6,456,445-byte driver after a 10,721,396-byte C
  seed. Before the final owner split, the exact full 270,050,952-byte driver MIR
  reached the old failing consumer and the repaired driver emitted an
  11,180,254-byte C artifact with exit 0. The split preserves that function and
  is newly compiled in v16. Full bootstrap/component are not claimed green;
  the replacement 29/29 run remains the next falsifier.
- Exact-head run `33025012263` at `c31da1d2` completed 27/29. All 20 backend
  shards, Windows, macOS, sanitizers, TSan, Rocq, toolchain, and codegen
  bootstrap were green. `build-linux` failed only because
  `direct_mir_multi_routine_mutations.py` had grown to 778 lines against its
  fixed 750-line cap. Responsibility-named split `6fa362c5` restores 741/750
  without raising the cap and keeps the namespace mutation behavior.
- The full bootstrap reached seed/oracle/bounded construction, gen2==gen3 at
  169,347 lines, installed DRV-2, and legacy/composite intent LLVM execution.
  It then rejected the nested-intent program because the dedicated implicit
  receiver owner still required the old nine-field parameter object after
  callable identity added `source_syntax_id` as the tenth field. Repair
  `af91687d` admits exactly ten fields, requires a positive canonical decimal
  receiver SyntaxNodeId, and cross-seals the routine owner. A zero receiver ID
  now fails at that owner before publication.
- Fresh v17 is local green for nested-intent public/native C/LLVM runtime and
  its no-artifact negatives, namespace-internal C/LLVM and four identity
  negatives, callable C/LLVM and all 20 negatives, canonical identity epoch,
  and unified program-graph ownership. The nested LLVM shell remains 160/160;
  the plan owner remains 639/700. The full component gate was stopped after
  exceeding the 60-second static-owner budget and is not claimed green.
- Architecture review `2026-08-27` correctly identifies identity-domain
  separation and post-resolution display-text erasure as the next design
  frontier. Its O(n^2) epoch replacement, keyed query spine, Scope Graph,
  270-MB profiling, compact MIR, and runtime research items remain proposals,
  not active work. The hard self-host guard keeps this callable executable/CI
  rung singular until a replacement exact-head run is 29/29.
- Replacement run `33027933374` at `3e8a3567` completed 28/29. Full
  `self-host-bootstrap-linux` is green: the fixed point and installed driver
  crossed the previously failing nested-intent LLVM program. `build-linux`
  also crossed the 750-line cap and complete component inventory, then failed
  only the semantic lifetime gate's stale exact caller list. Callable work had
  moved the driver from the legacy observed body-bundle entrypoint to the
  carried-identity-policy entrypoint in `30b84f80`, but that list still named
  the old call.
- Ratchet repair `a5ecff34` removes the driver only from the legacy caller set,
  adds an exact two-file set for the identity-policy boundary, and requires the
  production driver body to call that policy boundary. The focused lifetime
  gate is local green.
- Exact-head run `33029460672` at `9bf511d5` completed 28/29. Full
  `self-host-bootstrap-linux` is green again, and `build-linux` crossed the
  lifetime caller ratchet before stopping only at
  `tests/self_host_pergyra_likeness_smoke.sh`: six callable-identity additions
  had raised real `return/compare -1` sites from 24 to 30. Five were introduced
  by `30b84f80` and one by `1d459036`; no unrelated historical sentinel row is
  part of this repair.
- Repair `b80bc803` makes the semantic expression-binding owner publish one
  scalar `has_ordinal` fact and an `Option<Int>` read boundary. Call-target
  capture, identity resolution, and C emission no longer reopen the raw absent
  ordinal. A direct `Option<Int>` struct field was rejected by the current
  self-host codegen subset, so the carrier remains flat while absence becomes
  typed at its last consumers. Raising the likeness cap or adding an exclusion
  is forbidden. The ratchets are exact green at sentinel `23/23` and typed
  Result/Option surface `4375/4375`.
- Fresh v18 generated and compiled a 6,457,768-byte Pergyra-built driver seed
  and a 7,112,438-byte native oracle. The v18 callable C/LLVM gate plus all 20
  identity mutations, namespace-internal C/LLVM parity plus its negatives, and
  the canonical identity epoch gate are green. The complete component scan
  exceeded its 60-second static-owner budget and is not claimed green.
- The focused bootstrap wrapper itself is not claimed green: after both v18
  executables compiled, its artifact comparator rejected the drive-letter
  build-dir spelling `D:/PergyraLang/...` as escaping the repository. Future
  invocations must use the script default or Git-Bash absolute
  `/d/PergyraLang/...`. Both drive-letter `D:/...` and repo-relative build-dir
  arguments are rejected; do not repeat either form. This was
  a harness-path failure after compilation, not compiler semantic evidence.
  Replacement run `33032356735` is the observed exact-head 29/29 GREEN
  falsifier. This predecessor lease is closed; the active card above owns the
  next formal-callable display-text seam.
- Root variant output remains Git-closed by published checkpoint `1e8b5531`:
  `/bin/`, `/bin-*`, and `/build*/` are ignored and no such folder is tracked.
  The current root census is 11 `bin*` and 14 `build*` directories, all ignored,
  with zero exact root tracked paths. They remain physically present because
  other Codex sessions may be consuming them; do not delete them on this rung.
- The SoT census remains `CLOSED=50 BRIDGE=35 ACTIVE=1`. Reconciled hard SoT is
  `50/86 = 58.1%` and the migration index is 78.8%; integrated progress remains
  83% (81-85%), strict beta 83%, and hard replacement 75%. These are evidence
  corrections, not a callable-rung percentage increase.

## Previous self-host context - structured MatchCase carrier closure

- Implementation checkpoint `aafcadbd` and CI-ratchet repair `5ce4b384` are
  published. Replacement run `32969362909` passed 29/29 at exact repair HEAD
  `5ce4b384e45f63f68dff4605b8065eb68143b861` in about 30m26. The
  unrelated user-owned `pgy-80135c2c/` and concurrent
  `docs/compiler_architectures/` paths remain untracked and were not inspected,
  staged, deleted, or rewritten.
- `AstMatchCasePatternFact` remains the sole syntax owner. Its typed MatchCase
  atom is parsed once during `SemanticAstStatementFacts` admission; the existing
  SyntaxNodeId row carries canonical pattern, variant, flat binding range/pool,
  and a digest. The statement bundle is a carrier, not a second authority.
- Semantic match environments, MIR lowering, and self-C Option/tagged condition
  and binding emission consume the same structured fact. The raw MIR
  `SelfMirMatchCaseFactFromText` and codegen String accessor are deleted. Source
  inventory finds text parsing only inside the HIR owner and ready-artifact
  reading only there plus statement admission.
- Missing/wrong-kind, changed variant, changed binding, and crossed binding
  range negatives compile and execute fail closed. Both storage-lifetime gates
  pass. A fresh self-C tool compiles and executes `enum_match`,
  `enum_multi_payload`, and `option_enum_with_payload` with exact expected
  output. A fresh isolated installed DRV-2 passes canonical source-MIR parity
  for those fixtures plus `option_match`.
- `sot-authority-edge-test-smoke`, documentation quality, source syntax, and
  all 19 modified Pergyra source checks pass. The complete component inventory
  exceeded the focused local budget after its task-local assertions, so local
  completion is not claimed; replacement `build-linux` ran that component and
  the complete fast push target green in about 16m03. No timeout or cap was
  raised.
- Local variant folders are Git-closed by published ignore checkpoint
  `1e8b5531`: 23 root `bin-codex*`/`build-codex*`/`bin-dev*`/`build-dev*`
  directories contain only ignored rebuildable output. Physical deletion was
  attempted only after exact-path and reparse-point validation, but the current
  execution policy rejected recursive deletion before any file was removed.
- `selfhost.match_case_pattern` is `CLOSED`, so the SoT census is now
  `CLOSED=50 BRIDGE=35 ACTIVE=1`. Integrated progress remains 83% (81-85%),
  strict beta 83%, and hard replacement 75%. Publication and the 29/29
  replacement matrix are closed; no successor implementation lease is
  inferred by this handoff.

## Previous self-host context - readiness evidence reconciled; no successor inferred

- Reconciliation starts from published checkpoint
  `ab816bc923df2a7d0121a8d74134b2af2fa05a3e`. The unrelated user-owned
  untracked `pgy-80135c2c/` directory must not be inspected, staged, deleted,
  or rewritten.
- Objective card: make the published readiness/progress consumers agree with
  the executable scorecard, completed installed fixed point, and latest remote
  evidence without promoting native product tools or unsupported IR producers.
  The scorecard/fixed-point/remote gates own those facts; the readiness table,
  progress baseline, and this handoff are their last documentation consumers.
- The executable scorecard already classified arena/ownership Phase 1 as
  `READY`, and its owner document's measured closure said the same while the
  summary table still said `SUBSET`. Capability 4 now names all four gates and
  the table is `10/10 READY`; a negative ratchet rejects the old row and old
  `9/10` progress line.
- The same implementation source has installed fixed-point reproduction and
  remote run `32938125698` green 29/29. Bootstrap and CI/release evidence are
  therefore `4/4`; the fixed weighting recalculates the integrated forecast to
  83.20%, displayed as 83% with an 81-85% range. Strict beta stays 83%, hard
  replacement stays 75%, and SoT stays `CLOSED=49 BRIDGE=36 ACTIVE=1`.
- Local static scorecard, documentation quality, and source UTF-8 gates are
  green. This correction is not a new compiler substitution, closes no SoT
  registry row, and does not justify a formatter/debugger/scaffold/package-
  metadata/whole-REPL implementation track. A later rung still needs a fresh
  production compiler bypass, an existing complete Pergyra owner, and one
  executable falsifier.

## Previous self-host context - REPL compile bypass substitution closed

- Directive/audit checkpoint `36af9496` and implementation checkpoint
  `48aeccca` are on local and remote `main`. The worktree is clean except for
  the unrelated user-owned untracked `pgy-80135c2c/` directory, which must not
  be inspected, staged, deleted, or rewritten.
- Objective card: keep the C-owned REPL prompt, declaration accumulation,
  multiline handling, and cleanup, but replace its per-evaluation direct
  `driver_run_pipeline` call with the existing installed Pergyra C compile/run
  owner. This does not claim a Pergyra REPL session owner.
- Observed RED: with `PGY_SELF_DRIVER_BIN` set to a nonexistent executable,
  public `pgy --repl` still compiled and ran `repl-native-bypass` through the
  native pipeline, exited 0, and emitted no missing-driver diagnostic.
- `repl_run` now receives the launcher identity and calls exactly one
  `c_runner_execute_installed_self_host_c` boundary with the current REPL dev
  profile. Missing or failed installed drivers do not retry native. The REPL
  continues after a rejected evaluation as before and retires its synthesized
  source and binary.
- The new focused gate passed in 8 seconds. It observes a real installed-driver
  evaluation, an exactly-once counting sibling, missing-driver failure without
  a program/compile receipt, invalid-source failure without a binary, no native
  timing, cleanup, and a static ban on `driver_run_pipeline` in `repl.c`.
  Incremental `make pgy` is green. The existing installed-driver integration
  target, which reuses the same build and sources the new gate, is green; its
  seed/bootstrap preparation took about five minutes while the focused REPL
  slice itself remained eight seconds.
- Push run `32938125698` passed 29/29 in 30m31. Linux aggregate took 15m39,
  full self-host 30m27, Windows and sanitizers 8m50, codegen bootstrap 7m46,
  backend toolchain 9m27, macOS 2m25, Rocq 1m47, TSan 15 seconds, and all 20
  backend shards 41-58 seconds.
- Three reports under the completed nonnumbered agent directive independently
  census launcher paths, package metadata, and native product tools. Package
  manifest/lock is not a compiler substitution target and lacks a typed
  Seashell admission graph; formatter, debugger, scaffold/new, package init,
  and the complete REPL session remain `NOT READY` product boundaries.
- Classification is bounded `SUBSTITUTING` only for the REPL's compiler-bearing
  interior. Product-level REPL ownership, integrated 78%, strict beta 83%, and
  hard SoT `CLOSED=49 BRIDGE=36 ACTIVE=1` remain unchanged. No second successor
  rung is inferred from these audits.

## Previous self-host context - explicit-native isolation closed

- Published implementation checkpoint `4eef51ad` and repair checkpoint
  `45a2cfae` are on local and remote `main`. First run `32932076025` found the
  359/340 launcher-cap violation and was superseded after recording 27 green
  jobs and that exact Linux failure. The repair moves mode identity to
  `driver_self_host_selection_owner` and diagnostic emission to `driver_diag`;
  launcher/selection owner are exactly 340/140 lines. Replacement run
  `32933640461` passed 29/29 in 30m09: `build-linux` 14m41, full self-host
  29m49, sanitizers 12m35, Windows 8m37, codegen bootstrap 7m48, backend
  toolchain 9m13, and 20 shards in 40-59 seconds. The unrelated user-owned
  `pgy-80135c2c/` path remains untracked and must not be inspected, staged,
  deleted, or rewritten.
- Objective card: delete the launcher's final implicit native fallback for
  bare `--rir`, `--rir-json`, `--air`, `--air-json`, `--hir`, `--hir-cfg`,
  `--hir-dom`, and `--hir-ssa`. No complete installed Pergyra producer owns
  these payloads, so the launcher request boundary is the last legitimate
  consumer and must fail closed. Native RIR/AIR/HIR producers remain reachable
  only through the explicit `--native-pipeline` oracle.
- The final default `return driver_run_pipeline(&flags)` is deleted. Every bare
  mode exits nonzero with empty stdout, a mode-specific missing-owner
  diagnostic, and no pipeline timing even with a missing self-driver. Every
  explicit-native counterpart remains executable. A negative source ratchet
  permits exactly the bounded test-native MIR oracle and the declared native
  opt-out before installed delegation; no later native dispatch is allowed.
- Read-only RIR/AIR/HIR audits are complete under the nonnumbered directive in
  `docs/agent_work_directives/`. The first missing owners are an ordered RIR
  program carrying scope/fact/op/state/flow rows, a general AIR graph issuance
  fact, and an identity-bearing post-semantic HIR routine/CFG carrier. Their
  reports under `docs/audits/` are navigation evidence, not semantic authority
  or an implementation queue.
- Local green evidence: current-source forced build; installed-driver parent
  CLI; public MIR; eight-mode explicit-native isolation; AIR graph JSON parity;
  AIR schema and MIR binding; machine-neutral capability projection; IR probe;
  machine-layer pipeline; RIR resource flow; SEA lane; proof envelope; and
  C/LLVM observability. The AIR fixture's sole old binding fingerprint drift
  was deterministic over three current-owner runs and is refreshed without a
  schema or field change.
- `tests/self_hosted/mir_machine_layer_smoke.sh` exceeded its five-minute
  focused budget while `pgy.exe` recompiled `driver_rung2_main.pgy` and was
  stopped; do not report it green or increase its allowance. The installed
  parent gate reused the current driver and completed in about 23 seconds.
- Classification is fallback/SoT closure, not `SUBSTITUTING`: no Pergyra
  implementation replaced the native RIR/AIR/HIR producer. Integrated progress
  remains 78%, strict beta 83%, and hard SoT remains
  `CLOSED=49 BRIDGE=36 ACTIVE=1` (86 authorities / 180 derived carriers).
  No successor implementation rung may be inferred until one production
  bypass reaches an existing complete Pergyra owner and an executable
  falsifier.

## Previous self-host context - public MIR diagnostic substitution

- Published closure checkpoint `b3da55a3` is on local and remote `main`; the
  substitution implementation entered at `c2ff6548`. Remote CI run
  `32926584459` completed 29/29 green. The unrelated user-owned path is
  untracked `pgy-80135c2c/`; do not inspect, stage, delete, or rewrite it.
  Recheck status for concurrent-session scratch before staging any result-only
  documents.
- Objective card: make public installed `pgy --mir SOURCE` use the existing
  Pergyra source-to-MIR world action, full borrowed-text MIR admission, and one
  stable human projection. Fact owners are the existing
  `DriverSourceMirPayloadReceipt`, admitted MIR indexes, and their schema/
  topology/machine/intent validators. Last consumer is installed child stdout;
  the native launcher may relay only a bounded status/payload. Forbidden paths
  are default `driver_run_pipeline -> mir_dump`, native retry, source/AST/JSON
  reconstruction in the projection, guessed lifecycle facts, a temporary
  artifact, or a new mode-specific world/protocol species.
- Observed RED was exact: with `PGY_SELF_DRIVER_BIN` missing, default public
  `--mir` exited 0 and produced the same native `MIRProgram` lifecycle dump as
  explicit `--native-pipeline --mir`. The canonical `pgy.mir.v1` owner does not
  carry several native lifecycle/liveness/source fields, so copying the legacy
  shape would have invented a second fact authority.
- The launcher now delegates default `--mir` to
  `--emit-mir-diagnostic-verified` after the explicit native opt-out and before
  final native dispatch. The Pergyra child reuses
  `ProduceSourceMirThroughPgyCompilerWorld`, admits its canonical payload once,
  and renders only typed routine/block/instruction facts. Internal and public
  hello diagnostics are byte-identical; the four-block/phi CFG fixture retains
  exact successor and instruction inventory. Explicit native lifecycle output
  remains independently observable and byte-distinct.
- The C relay buffers at most 128 MiB for at most 300 seconds and never relays
  a child-failure prefix. Windows uses a kill-on-close Job Object plus process
  polling; POSIX uses a process group and nonblocking poll. Invalid source,
  missing driver, malformed admitted schema, unsupported options, silent
  success, descendant-held stdout, and stdout-close-before-exit all fail with
  no diagnostic payload. Timeout, overflow, crash, execution/capture failure,
  and ordinary child exit remain distinguishable at the relay boundary.
- A final current-source Pergyra-built DRV-2 is installed. The full installed
  CLI gate is green; the focused diagnostic gate, public MIR-JSON, source-MIR
  world/action, explicit native IR probe, SoT edge, likeness, shell syntax, and
  diff checks are green. Make dry-run for the weekly public-MIR/default-C pair
  reports exactly one self-host build and one installed-driver gate, with no
  undefined or standalone diagnostic target. The complete component inventory
  is not claimed locally because its primary scan was stopped at the static-loop
  budget; exact local predicates are 198 <= 200 lines, likeness 76/76, and
  production-root native emission with 0 errors.
- Remote run `32926584459` at exact HEAD `b3da55a3` completed 29/29 in 18m26.
  Full self-host fixed point/policy corpus passed in 18m04, `build-linux` in
  15m06, sanitizers in 10m34, Windows in 9m08, codegen bootstrap in 7m42, and
  the shared backend toolchain in 7m31; all 20 backend shards passed in 39-76
  seconds. This remotely closes the POSIX capture branch, complete structural
  inventory, likeness, and bootstrap-subset falsifiers. Node 20 deprecation
  annotations from artifact actions were warnings, not failed gates.
- This is bounded `SUBSTITUTING`: a real native C-owned public `mir_dump` path
  was removed. No top-level SoT row or native-only lifecycle fact is promoted;
  integrated progress remains 78%, strict beta 83%, and hard SoT remains
  `CLOSED=49 BRIDGE=36 ACTIVE=1` (86 authorities / 180 derived carriers).
- Do not infer a successor implementation rung from this result-only handoff.
  The next session must observe a production entrypoint and its direct bypass,
  name the existing Pergyra fact owner and last orchestration consumer, and fix
  one executable falsifier before opening edits. Parallel architecture audits
  are navigation evidence, not a new active rung.
  They found no duplicated semantic decision in Lease F or the nested one-plan
  route, recommended deferring folder movement, and named only a lower-priority
  scalar GraphPlan parameter-indirection candidate. The remote falsifier is
  closed; do not implement that candidate unless a fresh production bypass and
  objective card select it.

### Historical archive boundary

Everything below this line is inactive lookup evidence, not an active queue.

## Previous self-host context - exact nested priority/observability C target-pair substitution (inactive)

- Code checkpoint `60e9fb8a2d3ed32535c6ceee7d67246f6c32ddba` is on local
  and remote `main`. Before the result-only handoff commit, the handoff,
  progress, dogfood, and coordination documents are project-owned dirty state;
  afterward the worktree should contain only the unrelated user-owned untracked
  `pgy-80135c2c/` directory. Do not inspect it as evidence, stage it, delete it,
  or rewrite it.
- Objective card: make the exact one-subject/one-zone/two-intent nested
  priority/observability family emit C from the same sealed
  `DirectMirNestedIntentProgramPlan` as direct LLVM. Priority = exact behavior,
  one plan, MIR-blind C materialization, byte-identical source/direct C,
  claimed-family fail-closed mutations, old-path rejection, then warning
  cleanliness. Fact owner =
  `DirectMirNestedIntentProgramPlanFromAdmitted`; last consumers = installed
  source/MIR-to-C and direct-MIR C artifact entrypoints. Forbidden = source-C
  MIR-to-AST reconstruction or direct-C scalar retry after this route claims.
- Two read-only peer audits found the decisive seams before implementation.
  The old target branch returned `None` for every C request before asking the
  route owner, and the source-C consumer entered
  `DriverRung2IntentTreeEmissionOrDie` before any exact projection. The audits
  also corrected a false eight-line claim to the actual nine-line oracle and
  showed that the 160-line parent gate could source one sibling without a new
  Make target, workflow job, or second self-host build.
- The projection now claims once, seals one plan, and dispatches it to C or
  LLVM. The C emitter consumes only the plan plus canonical runtime/ABI symbol
  owners. `DriverRung2NestedIntentCSubstitutionIfClaimed` handles only the exact
  four-routine/two-declaration source-C family before MIR-to-AST reconstruction;
  unclaimed programs alone continue to the general path. Direct C consumes the
  same projection before scalar admission. Claimed-invalid programs die at the
  nested owner and are never retried.
- A fresh current-source Pergyra-built DRV-2 is installed. The final focused
  target script passed in 11.6 seconds. Source-C and direct-MIR C are exactly
  2,488 bytes with SHA-256 `4F2B9434AF2E8ABCD9F782E2909EA4261CD2E03638A52147E61940D99D23E644`.
  Their shared artifact compiles with thread-safe zone ABI and
  `-Wall -Wextra -Werror`, then executes the exact nine-line output. Existing
  LLVM parity/five negatives plus both C entrypoints' ten no-artifact negatives
  pass. Focused caps/order ratchets, shell syntax, and `git diff --check` pass.
- A complete component inventory scan was stopped after 90 seconds with no
  output to respect the 60-second static-loop budget; it is not claimed green.
  The real DRV-2 build and executable/negative gate are green and own this
  slice. First push run `32911287910` found that the new target projection's
  final `Die` needed an explicit unreachable `return None` for the self-host
  CFG body-safety proof. Repair `2f4dfe28` adds only that fail-closed return;
  the exact native-oracle driver emission then completed locally with zero
  errors and the focused target remained green. Replacement run `32912230440`
  passed fixed-point equality, installed DRV-2, and the preceding intent gates,
  then found the new thread-safe C harness omitted Linux POSIX feature macros.
  Checkpoint `60e9fb8a` mirrors the existing bootstrap emitted-C compiler
  profile; it does not change the artifact. Shell syntax and the exact focused
  execution are local green. Final run `32913743277` completed 29/29 green in
  29m25; `build-linux` took 15m24 and full self-host took 29m20. All 20 backend
  shards, sanitizers, platforms, codegen bootstrap, TSan, and Rocq passed.
- Same-mistake rule for this Windows host: run shell gates through
  `C:\msys64\usr\bin\bash.exe` with `/ucrt64/bin` first on `PATH`. Bare
  `bash` reaches an unavailable WSL `/bin/bash`, while `/mingw64/bin` selects
  the wrong runtime family. After directly installing the current DRV-2, run
  the focused script itself instead of its phony self-host Make prerequisite,
  which starts a duplicate bootstrap before the actual gate.
- This exact C family is bounded `SUBSTITUTING`: it replaces both a real
  source-C reconstruction path and the direct-C scalar dead end with one
  Pergyra plan. It does not close arbitrary intent C, the broader intent
  declaration family, or a top-level SoT row. Overall remains 78%, strict beta
  83%, and hard SoT remains `CLOSED=49 BRIDGE=36 ACTIVE=1`.
- Lease E is DONE. Do not infer another implementation from this handoff;
  observe the next production bypass, write a new objective card, and name its
  falsifying fixture before opening the next executable rung.

## Previous self-host context - DRV-1 scalar routine emission memory closure (inactive)

- Code checkpoint `2f3ad014` on local `main` contains the C/LLVM per-routine
  emitter pair plus its structural negative ratchet; `9a7ef022` is its clean
  predecessor on `origin/main`. Before this handoff commit, only this file and
  `src/self_hosted/PROGRESS.md` are dirty. Preserve the unrelated untracked
  `pgy-80135c2c/` directory. The preceding typed artifact transaction LLVM
  rung remains closed and is archived below.
- Objective card: objective = bound the compiler-scale direct-MIR C/LLVM
  emission lifetime without changing output identity; priority = byte-stable
  semantics, one per-routine builder owner, allocator cleanup, old cumulative
  copy rejection, compiler-scale memory/time evidence, then CI wall time; fact
  owner = `DirectMirScalarCfgProgramCRoutine` and
  `DirectMirScalarCfgProgramLlvmRoutine`; last legitimate consumer = the outer
  program emitter's sequential routine loop; forbidden = repeated
  `output = Concat(output, fragment)`, hiding the growth with a cache/shard/
  timeout/memory allowance, or sharing growable output storage across workers;
  falsifier = the exact 95,523,078-byte current DRV-1 MIR projected to the
  byte-identical LLVM artifact under a bounded memory observation, plus the
  focused C/LLVM execution/negative gate and remote full bootstrap.
- Measurement identified one concrete repeated owned operation. The outer
  emitters already used `TextBuilder`, but each inner routine accumulated 59 C
  or 68 LLVM fragments with `Concat`, retaining intermediate strings. On the
  exact DRV-1 MIR (`D2B4E47E...81EE`), the old LLVM projection spent about 95
  seconds in scalar admission, about 590 seconds in graph planning, and about
  219 seconds in emission; final private memory was about 11.12 GiB for a
  22,492,152-byte artifact.
- Both inner routine owners now allocate one 4,096-byte-initialized
  `TextBuilder`, append in the unchanged order, finish once, destroy their
  allocator, and return the finished string. The component contract requires
  builder creation, finish, and allocator destruction and rejects
  `Concat(output` inside either function. The LLVM owner remains at its
  360-line cap; the C owner is 309 lines.
- Full source reachability is observed rather than inferred. The exact
  `driver_bootstrap_main.pgy --pressure-owned-full-fixpoint` root produced a
  239,447,870-byte `pgy.mir.v1` artifact, SHA-256
  `9A3B13489B66941A21E1CE4B9F7C1FFC1B3FD9D7A356E40492E1224CFB4DB40A`,
  with 6,988 routines. It contains exactly one C and one LLVM routine emitter,
  both with the builder/finish/destroy path and neither with the cumulative
  `Concat` path.
- A canonical Pergyra-codegen-built isolated driver passes the focused
  explicit-entrypoint-return C/LLVM compile/run and malformed-return negative.
  Its same-input LLVM projection completed in 837.830 seconds at 5.399 GiB
  peak private / 5.187 GiB peak working set under an 8 GiB stop boundary.
  Scalar admission ended at 99.826 seconds, graph planning at 634.182 seconds,
  and emission at 837.331 seconds. Output is byte-identical at 22,492,152
  bytes, SHA-256
  `B55BDC95D128D17B97A53747631FA62F4C75693719866293E3C7C02E2EA10E74`.
  Against the observed old run this is about 7.4% lower total time and 51.4%
  lower peak private memory; graph planning remains the dominant CPU seam.
- Two diagnostic cautions are now explicit. Direct-MIR large-input paths must
  use repository-relative POSIX spelling on Windows; the same file is rejected
  as unreadable through an absolute Windows spelling. Also do not wrap the
  installer script itself in `measure_build_pressure.ps1`: its exported
  `PGY_BUILD_PRESSURE_ACTIVE=1` changes the nested bounded-smoke lane. The
  generated canonical C compiled successfully, and the same executable passed
  that smoke outside the pressure wrapper.
- Supporting native-oracle work exposed a separate launcher lifetime defect:
  plain native compile reached 3.060 GiB because `pgy.exe` retained about
  2.397 GiB while starting the host compiler. Split owner stages passed at
  80.483 seconds / 2.378 GiB for source-to-C and 136.884 seconds / 2.125 GiB
  for host compile. This RED is recorded, not hidden by raising the 3,072 MiB
  ceiling, but it is not the active self-host substitution owner.
- Local omissions remain explicit. The broad Bool projection gate has a stale
  pinned MIR hash: both the installed driver and the new isolated driver emit
  29,788-byte SHA `E25B95D7...A0F6F4E`, while the script still expects the
  older `B4DE3B8...D03B4B`. The broad dual-backend gate passed hello C/LLVM
  positive parity, then its diagnostic-specific negative expected a narrower
  message than the current generic admission failure. Neither RED is reported
  as a pass or silently repaired in this performance slice.
- Push CI is already runner-parallel, but hosted-runner availability is
  variable. Run `32706421231` completed 29/29 in 23m48 with all 20 backend
  shards starting together; the succeeding all-green run `32709251632` took
  58m53 because the same short shards were allocated mostly serially. Its job
  execution sum was 93.6 runner-minutes; the full bootstrap itself was 24.2
  minutes and each artifact-fed shard only 0.6-1.1 minutes. Do not claim a
  shard-count reduction before measuring the 926-case distribution under the
  same toolchain; the active compiler memory fix must reach remote bootstrap
  first.
- Local final gates are green: the complete component contract after the
  allocator ratchet, full UTF-8 documentation quality, progress metric, and
  `git diff --check`. Next falsifier: publish the code and handoff commits,
  require `self-host-bootstrap-linux` and all 29 push jobs to stay green, then
  compare the new remote critical path before changing matrix width. This
  performance closure does not increment substitution percentages: overall
  remains 78%, strict beta 83%, and hard SoT
  `CLOSED=49 BRIDGE=36 ACTIVE=1`.

## Previous self-host context - DRV-1 typed artifact transaction LLVM closure (inactive)

- The semantic checkpoint is `d6b82a29`; CI toolchain reuse is checkpoint
  `64eeeda0`, first-remote-run repair is `1df380a1`, and its derived-fact
  classification successor is `cff2a2fa` on local `main`. This handoff records
  the final current-source executable proof and the third remote result.
  Before its commit the only project dirt is this file and
  `src/self_hosted/PROGRESS.md`; after publication the worktree should contain
  only the unrelated user-owned `pgy-80135c2c/`. Do not stage, discard,
  rewrite, or scan that directory as project evidence.
- Objective card: objective = make the production DRV-1 LLVM executable
  consume the typed artifact transaction without a native/C-only bypass;
  priority = typed outcome identity, referenced-enum ownership, payload
  construction/match binding, Begin/Commit/Abort ABI, old-path rejection,
  executable publication, then performance; fact owner =
  `DirectMirScalarProgramReferencedEnumFact` plus
  `SelfMirArtifactCommitOutcome` and the runtime-call ABI registry; last
  legitimate consumer = `driver_cli_owner.pgy` `-o` publication; forbidden =
  filtering the payload enum, collapsing the outcome to `Bool`, reconstructing
  declarations in the payload-free view, retrying native/C after installed
  failure, or masking compiler-scale copying with a cache/shard/timeout; final
  falsifier = current `driver_rung1_main.pgy` source -> verified MIR -> LLVM ->
  linked executable -> committed output artifact, with malformed enum/ABI and
  missing-transaction facts still failing closed.
- The reached semantic slice is implemented locally. One referenced-enum fact
  owns declaration identity for both payload-free and payload-bearing enums;
  payload-free facts project from it instead of rescanning declarations.
  Direct C/LLVM support now carries payload enum construction, exhaustive match
  selection, match-binding locals, callable/route/signature facts, and exact
  local types. Artifact Begin/Commit/Abort own runtime-call ABI rows 259-261;
  the manifest count is 262. Expression kinds 114-119 own payload construction,
  payload match, the three artifact calls, and payload-free enum inequality.
- The current installed `bin/pgy-self-driver.exe` was rebuilt after the last
  compiler-source change. Focused payload-bearing enum construction/match
  binding C/LLVM parity plus three negatives passes in 7.0 seconds; adjacent
  payload-free enum parity passes in 11.3 seconds. The complete component
  contract now passes, including the source-MIR world/action/pressure/commit
  executable ratchet and the negative structural inventory. Runtime-call ABI
  row parity passes; its local LLVM leg explicitly skipped because the default
  `bin/pgy.exe` was built without LLVM support. `git diff --check` is green.
- Current source-to-MIR evidence is now exact rather than inferred. The public
  installed producer emitted
  `.tmp/self_hosted/driver_rung1/driver_rung1_current_source_borrow_fix.mir.json`
  in 27.97 seconds: 95,523,078 bytes, SHA-256
  `D2B4E47E051590ED758E227F9CF2B1CFEA2334CA2652633D9CA0F2A7E56781EE`.
  This is byte-identical to the pre-repair current-source MIR: removing the
  illegal borrowed-index local changes source ownership admission, not
  canonical program meaning. It includes the final referenced-enum/direct-call
  compiler edits and replaces the older stale source snapshot for the next
  falsifier.
- The exact current MIR now completes the final local executable falsifier. The
  installed self-driver projected it to 22,492,152-byte LLVM, SHA-256
  `B55BDC95D128D17B97A53747631FA62F4C75693719866293E3C7C02E2EA10E74`.
  That IR is byte-identical to the preceding fixed-MIR projection, consistent
  with the final borrow-safe and inventory/CI/documentation edits preserving
  executable meaning. Clang linked it with the observation-disabled runtime
  object into a 25,148,709-byte DRV-1 executable, SHA-256
  `490D92255CEF75860B1005ECBE08B8BC625932DE6409D2FF4DF779226A89DD6D`.
- That current-source executable consumed
  `direct_mir_payload_free_enum_value_parameter.pgy` through the typed `-o`
  transaction and committed an 11,294-byte C artifact, SHA-256
  `56BEECAF1E7974DFF66E966D24506E80FECFA1FC1238BDC74E9879CBE29AE001`.
  The artifact is byte-identical to the preceding success, compiles with GCC,
  and executes the exact ten-line enum result ending in
  `payload-free-enum-parameter-ready`. Observed projector private memory peaked
  at 10.75GiB with 3.23GiB still free; no user process was terminated. Repeated
  whole-program aggregation remains the next bounded performance seam before
  any intra-process parallelism, cache, or larger allowance.
- Push CI retains its existing runner-level parallelism. The local CI delta
  removes a different repeated operation: the 20-way backend-compare matrix
  previously rebuilt the same LLVM-enabled installed compiler in every shard.
  `backend-compare-toolchain-linux` now builds and uploads the exact launcher/
  self-driver pair once; all 20 independent shards download it and enter an
  explicit fail-closed `prebuilt` compiler mode. The CI profile gate passes,
  an invalid mode fails with rc=2, and an LLVM-enabled launcher plus the current
  self-driver passes one real artifact-fed C/LLVM backend case without a
  rebuild.
- First remote run `32701478910` at `fe7e9dc6` completed 27/29. The new
  producer uploaded its artifact and all 20 artifact-fed backend shards passed;
  the CI topology change is therefore remotely green. The two reds were exact
  semantic/inventory omissions from the compiler change: `build-linux` reached
  the final contract after all core tests passed, then rejected a stale
  generated language-word implementation inventory; full bootstrap rejected a
  borrowed `MirProgramEnumVariantIndex` local copied from `admitted`. Checkpoint
  `1df380a1` regenerates the inventory through its owner, reads enum-index facts
  directly from the borrowed admission owner, and negative-gates the forbidden
  local copy. The exact native `--emit-c` reproduction now passes in 12.4
  seconds with 0 errors/0 warnings and emits 13,195,128-byte C; the language
  registry and complete component contract are green. Second remote run
  `32703532890` at `f44181d4` completed 28/29: full self-host bootstrap, the
  shared toolchain producer, and all 20 backend shards passed. Its only red was
  the final `sot-authority-edge` check: the new referenced-enum fact carrier
  had not been classified in the derived-fact registry. It is now registered
  as a `projection` of `selfhost.enum_declaration_rows`, not as a second
  authority; focused edge and adequacy gates pass locally. Third run
  `32706421231` at `cff2a2fa` completed all 29/29 jobs green in 23m48; the full
  bootstrap itself passed in 23m43. This remotely closes the derived-fact
  repair, full bootstrap, shared toolchain producer, and all 20 artifact-fed
  backend shards at the executable checkpoint.
- Last remote baselines remain green: fast/full-platform split run
  `32680354623` completed 13/13 in 40m36 at `88fbe332`; the successor fast/docs
  run `32682690750` completed 28/28 at `e179537d`. This integration does not
  increment the published forecasts by itself: overall remains 78%, strict
  beta 83%, and hard SoT `CLOSED=49 BRIDGE=36 ACTIVE=1`.
- This bounded DRV-1 typed-transaction LLVM rung is closed: a Pergyra-produced
  current MIR reaches a linked compiler executable and commits the output
  artifact without the forbidden C/native fallback. Next: publish this
  checkpoint, then select one next production-bypass rung from current source
  evidence. Measure the reached repeated aggregation before proposing
  projector parallelism. Do not count the CI topology edit, MIR bytes, owner
  files, or tests as substitution progress by themselves.

## Previous self-host context - artifact-fed full-platform parity shards (inactive)

- The executable CI checkpoint is
  `88fbe332ee216d163cb1f1949ecd9dbb53277bce` on local `main`, synchronized
  with `origin/main`; this handoff is its measured documentation successor.
  After publication the worktree should contain only the unrelated untracked
  `pgy-80135c2c/`; do not stage, discard, rewrite, or scan that directory as
  project evidence.
- Objective card: objective = reduce the scheduled/manual/release full-platform
  critical path without weakening one proof; priority = retain exact parser,
  semantic, codegen, driver, contract, and core evidence, consume one same-run
  installed toolchain, preserve independent scratch ownership, then wall time;
  fact owners = the existing four parity scripts and their Make targets for
  behavioral evidence, `.github/workflows/platform_full.yml` for runner/shard
  placement, `scripts/ci_linux_steps.sh` and `scripts/ci_windows_steps.sh` for
  standalone-versus-artifact-fed core mode, and
  `tests/self_host_ci_profile_smoke.sh` for the negative ratchet; last consumers
  = Linux/Windows full core plus their four parity matrix legs; forbidden =
  removing or skipping a gate, advisory failure, a shared growable scratch,
  fixed-runner `make -jN`, accepting an incomplete artifact, or silently
  rebuilding inside an artifact-fed parity shard; falsifiers = CI profile,
  source inventory, beta readiness, one real installed parser shard, remote
  fast main, and a manually dispatched full workflow.
- Serial baseline run `32649263604` remains the last green full proof before
  this split: Windows 73m12, Linux 59m20, and macOS 29m09. Recomputed log
  boundaries show the actual bottlenecks. Windows spent 38m49 in platform
  preparation, 13m34 in `self-host-compiler`, and 6m31 in `test-all`; Linux
  spent 25m36 in preparation, 8m33 in `self-host-compiler`, and 7m45 in formal
  semantics; macOS spent 11m53, 5m42, and 4m48 respectively. The Windows
  preparation tail was dominated by the approximately 24m33 driver shard;
  Linux's driver shard was approximately 19m26. Compiler construction itself
  was not the whole delay.
- The current topology builds one installed Linux and one installed Windows
  toolchain, uploads only `pgy`, `pgy-self-driver`, and the owned machine-layer
  manifest, then fans out core plus parser/semantic/codegen/driver parity.
  `fail-fast: false` keeps all independent failure evidence visible. The core
  jobs run the unchanged `ci-linux`/`ci-windows` lists in explicit `prebuilt +
  contract-only` mode; standalone/local invocation defaults to `build + full`
  and therefore retains the prior one-command full proof. macOS stays serial
  because its measured leg remains below the split critical path.
- `scripts/ci_self_host_platform_parity_shard_owner.sh` accepts exactly one of
  the four named shards, validates executable compiler/driver plus the schema
  of the installed manifest, and then invokes the existing behavioral owner.
  It contains no compiler build command or fallback. Linux consumes `c llvm`;
  Windows retains its existing C-only proof. Each matrix leg gets a fresh
  checkout and its existing distinct `.tmp/self_hosted/*` root.
- Focused local evidence is green: `bash -n` for both modified platform lists,
  the new shard owner, and the CI profile; the CI profile gate; build-source
  inventory; beta readiness; `git diff --check`; and a synthetic core routing
  probe that observed Linux 116 and Windows 59 commands with the contract
  present and both the aggregate parity target and `self-host-compiler` absent.
  A real installed Windows parser shard reused its fingerprinted C tool and
  passed byte equality for all 189 sources. The first attempt correctly failed
  at the existing mixed Windows/POSIX scratch-path guard; the same command with
  the CI-owned MSYS path passed. The invalid shard negative also failed closed.
- Remote fast evidence is green: CI run `32679346787` completed 28/28 jobs at
  `88fbe332` in 17m42. Remote full evidence is also green: manual run
  `32680354623` completed all 13 jobs at the same revision in 40m36. The prior
  serial full workflow consumed 75m08, so the measured workflow wall time fell
  by 34m32, approximately 46%, without deleting, skipping, retrying, or making
  a test advisory.
- The split full timings are load-bearing evidence rather than projections.
  Linux toolchain was 8m37; parser 0m54, semantic 1m02, codegen 3m46, driver
  21m53, and core 26m47. Its critical dependency path is therefore about
  35m24 versus the prior 59m20. Windows toolchain was 13m34; parser 3m03,
  semantic 3m43, codegen 6m56, driver 26m54, and core 25m01. Its critical
  dependency path is about 40m28 versus the prior 73m12. macOS stayed serial
  and completed in 26m57 versus 29m09. All artifact downloads, executable-bit
  restoration, manifest validation, C-only Windows selection, and C/LLVM Linux
  selection executed on their real hosted runners.
- This is CI blocker removal only. It changes no compiler semantic owner and
  does not increment substitution progress or the published forecasts: overall
  78%, strict beta 83%, hard SoT `CLOSED=49 BRIDGE=36 ACTIVE=1`. Next: publish
  this measured handoff, require its docs-only fast CI successor to stay green,
  then return directly to the active DRV-1 payload-bearing artifact-outcome
  LLVM support and Begin/Commit/Abort runtime-call falsifier. Do not open
  another CI topology track from this now-closed blocker.

## Previous self-host context - bounded push feedback and safe full-platform proof (inactive)

- The exact CI-topology checkpoint is
  `65e98896db3f1b76690beacc8be666404ddaa390` on local `main`; this handoff is
  its documentation-only successor. Predecessor `103f2e0b` splits branch
  feedback from the full native-platform ladder, and predecessor `1ce15a58`
  removes fixed-runner `make -j5` oversubscription after remote measurement
  falsified it as a sound default. The current checkpoint gives the measured
  Linux full proof a bounded timeout margin. After publication the worktree
  should be clean except for the unrelated untracked `pgy-80135c2c/`; do not
  stage, discard, rewrite, or scan it as project evidence.
- Last green remote baseline: CI run `32633975124` completed all 28 jobs at
  `dc98d919`. Its platform jobs were green but expensive: Linux 40.4 minutes,
  macOS 21.6 minutes, and Windows 77.7 minutes. Log timestamps attribute the
  delay to test/bootstrap breadth, not an ordinary compiler build. Windows
  spent about 42 minutes in `self-host-preparation-platform-test-smoke`
  (contract 6m53, parser 1m17, semantic 2m13, codegen 5m16, driver 26m08),
  14 minutes in `self-host-compiler`, and 7 minutes in `test-all`.
- Objective card: objective = keep ordinary main/PR feedback below one bounded
  integration window while retaining every full platform proof; priority =
  semantic evidence, one named owner per fact, platform-specific negatives,
  reproducible runner use, then wall time; fact owners =
  `scripts/ci_push_*_steps.sh` for branch
  feedback, `scripts/ci_*_steps.sh` plus
  `.github/workflows/platform_full.yml` for full platform evidence, and
  `tests/self_host_ci_profile_smoke.sh` for the negative ratchet; last
  consumers = the three main workflow platform jobs and the weekly/manual/`v*`
  full workflow; forbidden = deleting a test, `continue-on-error`, a silent
  skip, duplicating platform-independent self-host evidence on all three push
  runners, restoring the full ladder to branch push, oversubscribing one
  fixed-size runner with `make -jN`, or sharing one growable scratch owner
  across parallel parity processes; verification = fast Windows native
  `test-all`, the profile/inventory/readiness gates, remote main, and a manually
  dispatched full workflow.
- The main workflow keeps the stable `build-linux`, `build-windows`, and
  `build-macos-c-only` job identities. Linux owns the self-host compiler,
  platform-independent contract, core executable suite, and high-value static
  gates. Windows and macOS own native core/platform behavior with the explicit
  `PGY_NATIVE_PIPELINE=1` boundary; their full self-host runs remain in the
  full workflow. Fast timeouts are 25/35/20 minutes for Linux/Windows/macOS.
- `Platform full` owns the unchanged `ci-linux`, `ci-windows`, and `ci-macos`
  commands, the existing declared Coq skips on Windows/macOS, and the existing
  75/90/45-minute hang budgets. Linux uses 75 minutes because the first clean
  serial full run consumed 59m20; Windows/macOS retain measured 90/45-minute
  margins. It runs manually, Sunday 15:00 UTC, and for
  `v*` tags, before the separate Sunday 18:00 UTC exhaustive self-host parity
  workflow. No full proof was made advisory or deleted.
- The preparation aggregate still exposes contract, parser, semantic, codegen,
  and driver as five named prerequisites with distinct
  `.tmp/self_hosted/*` roots. A local Windows `make -j5` run proved scratch
  isolation for contract, parser (189 sources), semantic (114 fixtures), and
  codegen (85 fixtures), but the driver exceeded the 30-minute integration
  budget and peaked at an observed 7.9 GiB private memory. Remote full run
  `32644271941` then falsified `-j5` as a sound fixed-runner default: macOS was
  green in 24m51 (only about 3% faster than 25m42), Linux was green in 53m04
  (about 31% slower than 40m24), and Windows had not completed when the run was
  cancelled after 66m14. The log records `Ctrl+C` and no test failure. Run
  `32647681649` started from the weekly schedule nine seconds before the
  cancellation, and `.github/workflows/platform_full.yml` owns one
  `platform-full-${{ github.ref }}` concurrency group with
  `cancel-in-progress: true`; the scheduled run therefore superseded the
  manual run. This was not a user cancellation or test failure. The current
  checkpoint restores the aggregate's serial invocation on all three platforms
  and the CI profile gate rejects a reintroduced in-runner `make -jN` call. No
  all-green full-workflow PASS is claimed for the superseded run.
- Current serial full evidence is green: manually dispatched run `32649263604`
  completed all three jobs at `bf1d190b`. macOS took 29m09, Linux 59m20, and
  Windows 73m12. The Linux result left only 40 seconds under the original
  60-minute timeout, so checkpoint `65e98896` raises that hang budget to 75
  minutes without changing, skipping, retrying, or weakening any proof.
- Focused local evidence is green: the CI profile, build source inventory,
  beta readiness checklist, documentation quality gate, Windows filesystem
  walk, AIR erasure dashboard, semantic fixture isolation, and the explicit
  native Windows `test-all`. The first native test run correctly failed at the
  default self-host delegation boundary; with the profile-owned
  `PGY_NATIVE_PIPELINE=1`, the former failure passed and the suite completed,
  including semantic 2,823/2,823, MIR 162/162, and HIR 25/25. A local Coq proof
  PASS is not claimed; the aggregate used the declared Windows missing-prover
  mode, while remote Rocq/Linux jobs remain authoritative. For the current
  serial-restoration checkpoint, shell syntax, the CI profile negative gate,
  absence of `make -jN` in the three full scripts, and `git diff --check` are
  green. A combined broad inventory dry-run was stopped rather than allowing it
  to scan the unrelated user-owned `pgy-80135c2c/` tree.
- Remote main run `32649259745` at `bf1d190b` is green 28/28 with a 23m40
  critical path while the serial full workflow was also running. Its fast
  platform jobs are Linux 14m59, Windows 9m18, and macOS 1m53, versus 40m24,
  77m42, and 21m36 in the prior full-on-push baseline. The preceding isolated
  fast run `32648096195` completed in 19m26. `Platform full` did not
  branch-trigger; run `32649263604` was the explicit manual dispatch described
  above.
- The active semantic rung remains DRV-1 payload-bearing artifact-outcome LLVM
  support and its Begin/Commit/Abort calls. This CI topology work changes no
  compiler semantic owner and counts as blocker removal, not self-host
  substitution progress. Project progress therefore remains 78% overall,
  strict beta 83%, and hard SoT `CLOSED=49 BRIDGE=36 ACTIVE=1`.
- Next executable rung: commit this handoff, push `main`, require the final
  28-job fast workflow to remain green, then return directly to the DRV-1
  focused falsifier. Both CI boundaries are otherwise verified. Further
  full-performance work must first measure repeated compiler construction
  inside the reached driver owner; do not add more workers, shards, or caches
  from this run alone.

## Previous self-host context - push CI restoration and DRV-1 frontier (inactive)

- The exact CI-contract checkpoint is
  `0218d045ef01598b224bb25ad91f321bea0cac78` on local `main`; component-contract
  predecessor `438fea41a2a35590c0854957ff3855c7d4c1d594` moves the first structural
  ratchet family, CI-profile
  predecessor `9197b957fc6bf1ec807ec7cdc23febf42a09f2ab` owns the workflow split,
  timeout
  predecessor `7e68b47ff5792598ceb04aca77161d8ec2a6ddd1` established the measured
  180-minute exhaustive bound, likeness predecessor
  `052ae5794693e7102e2f48bf05fb66464aa73632` classifies the reached terminal
  LLVM projection, executable predecessor `3e03d4887c76252c1b062fca7c73e1e60b8dabf4`
  closes the reached source-subset dependencies, and registry predecessor
  `0d69e2cdc254ca2b676659f4a34958ea7fb67f64` classifies their derived facts.
  This handoff is the documentation-only successor. After its commit the
  worktree is clean except for the unrelated untracked `pgy-80135c2c/`; do not
  stage, discard, rewrite, or scan it as project evidence. Ignored `.tmp`
  compiler, MIR, C, executable, comparison, and build-pressure artifacts are
  evidence only.
- Objective card: objective = make ordinary `push`/`pull_request` feedback
  clean and bounded without hiding, weakening, or deleting exhaustive
  self-host parity; priority = semantic evidence, fast push feedback,
  scheduled/manual/release exhaustive proof, fallback prevention, then patch
  size; fact owners = `.github/workflows/ci.yml`,
  `.github/workflows/self_host_parity.yml`, and
  `tests/self_host_ci_profile_smoke.sh`; last consumers = GitHub push/PR runs
  and the weekly, manual, or `v*` release-tag exhaustive run; forbidden =
  `continue-on-error`, deleting or shortening the parity command, reintroducing
  branch-push parity, unbounded/default timeout, treating a skipped or failed
  exhaustive run as green, collapsing `SelfMirArtifactCommitOutcome` to
  `Bool`, or skipping its payload-bearing enum; verification gate = the 28-job
  main workflow is green while the dedicated 180-minute exhaustive job remains
  structurally required; next semantic falsifier = focused DRV-1 C/LLVM parity.
- Run `32623093485` at `9d532ba4` completed with 28 successful jobs and one
  cancelled job. Every platform build, both bootstrap jobs, sanitizers, TSan,
  Rocq, and all 20 backend shards passed. The exhaustive job completed all
  1,623 sources and 3,214 unique program-target checks with zero failures plus
  the incremental, 85-fixture C/LLVM, initializer, generic, wrapper,
  collection, aggregate, and dashboard suites before its old 90-minute timeout
  cancelled it during DRV-0.
- Run `32627393371` at `c65796c4` completed with those same 28 ordinary jobs
  green and only `self-host-parity-linux` red. Under the measured 180-minute
  bound it completed the exhaustive ledger and DRV-0 C/LLVM artifact parity
  for all 85 fixtures. Its first actual semantic failure was then DRV-1 LLVM
  compilation: the direct-MIR scalar program route rejected
  `owner=scalar-program-route stage=payload-free-enum`. The exact local
  reproduction is
  `bin/pgy.exe src/self_hosted/compiler/driver_rung1_main.pgy --backend=llvm -o .tmp/self_hosted/driver_rung1/direct_llvm_repro.exe`.
  This is not a Linux-only failure and the dedicated exhaustive workflow is not
  currently green.
- The DRV-1 cause is bounded: the admitted program includes payload-free
  `LanguageWordId` and `SelfMirArtifactCommitStage`, but also payload-bearing
  `SelfMirArtifactCommitOutcome` variants. Direct-MIR scalar LLVM currently has
  neither general payload-bearing user-enum projection nor the artifact
  Begin/Commit/Abort runtime calls needed by that outcome's real consumer.
  Merely filtering the enum from the payload-free fact would expose the next
  missing feature and is not a fix. The typed outcome was deliberately added
  to preserve fallible artifact-transaction meaning; it must not be replaced
  by a boolean or bypassed for CI.
- Commit `9197b957` moves the unchanged exhaustive parity job out of the branch
  push workflow into `Self-host parity`, triggered weekly at Sunday 18:00 UTC
  (Monday 03:00 KST), manually, and by `v*` release tags. It preserves the
  exact full target list, the 180-minute timeout, cancellation behavior, and
  hard failure semantics. The main workflow retains the same 28 jobs already
  observed green at `c65796c4`; no test or platform job was removed from that
  profile.
- Push run `32631419965` at `c8abd8fe` registered both workflows correctly,
  launched only the 28-job main workflow, and launched no dedicated parity
  run. It reached 19 green jobs before the first failure in
  `build-macos-c-only`. The compiler and behavioral gates were not the cause:
  `self_hosted_component_contract_smoke.sh` still required 23 exhaustive target
  names specifically in `.github/workflows/ci.yml`, even though every target
  had moved intact to `.github/workflows/self_host_parity.yml`. Commit
  `438fea41` moves all 23 explicit structural owner assertions to that exact
  workflow; it does not add an either-file fallback or change any executable
  target.
- Successor push run `32632566150` at `c26cc2db` reached 19 green jobs and
  proved the repaired component inventory passes remotely. Its first failure
  was the next exact structural owner: `self_host_hard_contract_smoke.sh` still
  required 16 dedicated parity projection targets in `.github/workflows/ci.yml`.
  Repository-wide workflow-reference inspection found no other displaced
  parity assertions; the remaining main-workflow references own platform,
  sanitizer, formal-proof, and 20-shard backend checks that still execute on
  push. Commit `0218d045` moves only those 16 hard-contract assertions to the
  exact dedicated workflow.
- Local verification: `self_host_ci_profile_smoke.sh` passes and requires both
  workflow boundaries, their trigger exclusions, the exact exhaustive command,
  and all timeout budgets. The full
  `self_hosted_component_contract_smoke.sh` structural inventory passes after
  the 23 owner moves, with zero remaining `ci.yml` assertions and exactly 23
  dedicated parity-workflow assertions. `self_host_hard_contract_smoke.sh`
  passes with zero remaining `ci.yml` assertions and exactly 16 dedicated
  parity-workflow assertions. The aggregate
  `self-host-preparation-contract-test-smoke` then passed both changed gates,
  completeness/substrate, the DRV-2 graph-owner sequence, and stopped
  fail-closed at `sot_authority_adequacy_smoke.sh` only because this Windows
  environment has neither `rocq` nor `coqc`; no missing-proof skip was set and
  no full aggregate pass is claimed. `build_source_inventory_smoke.sh` passes.
  Earlier
  reached evidence remains green: exact native production source emission,
  the pressure-owned full Windows bootstrap below its 3 GiB limit, installed
  C/LLVM intent observability, both 1,623-source semantic-checker passes,
  authority census `CLOSED=49 BRIDGE=36 ACTIVE=1`, and likeness metrics
  `core_string_munge=76/76` and `result_use=4214/4214`. No warning-clean claim
  is made for the three existing intent-`who` redundancy warnings.
- Next executable rung: commit this handoff, push `main`, and inspect the newly
  triggered 28-job run. If red, resume only from its first deterministic
  failure. Confirm that the dedicated workflow is registered but not launched
  by the branch push. Do not manually spend another exhaustive run while its
  DRV-1 failure is already reproduced. After main CI is green, implement real
  direct-LLVM support for the payload-bearing artifact outcome and its
  Begin/Commit/Abort calls, with `driver_rung1_parity.sh` as the focused
  falsifier.

## Previous self-host context - production LLVM DRV-0 parity publication (inactive)

- Checkpoint `108e4d4b5ea0dd837df8302daeaf15feaec498a7` closed the
  production DRV-0 `Result<Int>`, logical-record value-result storage, nested
  short-circuit, runtime-value lifecycle, and C/LLVM parity chain. Its focused
  value-result negatives, exact 3,000-routine LLVM validation, and all 85 DRV-0
  C/LLVM fixtures passed. The following publication exposed only the active CI
  restoration causes above; do not reopen this semantic chain as an independent
  SoT queue.

## Previous self-host context - runtime-value ABI and Int32 parity publication (inactive)

- Checkpoints `4d892744` and `3cdbb203` closed typed runtime-value and
  `CompilerArtifactWrite` ABI identities, restored signed-32-bit `Int`
  arithmetic/formatting, advanced GraphPlan to v80, and published the prior
  handoff. Runtime-call ABI rows, runtime-value lifecycle, native MIR 162/162,
  and the then-current component and dashboard gates were green. The subsequent
  remote run reached the production LLVM DRV-0 failure now closed by the active
  context; do not resume this earlier ABI card as an independent SoT queue.

## Previous self-host context - wrapper-policy parity tail publication (inactive)

- The exact compiler-semantic checkpoint is
  `b96276b0f122bbe53ab419087b38d9bbfa0377dc` on `main`. This handoff is its
  documentation-only successor. After publication, the worktree should be
  clean except for the unrelated untracked `pgy-80135c2c/`; do not stage,
  discard, or rewrite it. Ignored `.tmp` compiler, C, LLVM, executable, and
  comparison artifacts are evidence only.
- Remote run `32553158962` at `1ac3d4d0` completed with 28 of 29 jobs green.
  Linux, Windows, macOS, sanitizers, TSan, both bootstrap jobs, Rocq, and all
  20 backend shards passed. The sole red `self-host-parity-linux` completed the
  1,567-source ledger, 85-fixture C/LLVM codegen, initializer projection, and
  the formerly red generic-return parity. Its first deterministic failure was
  `wrapper_policy_probe_parity.sh`: the alleged native C oracle used plain
  `--backend=c`, delegated to the installed self-host driver, and was correctly
  rejected because `--error-format=json` is outside that installed contract.
- First publication run `32559124836` at `32cff507` reached 25 green jobs with
  Linux, Windows, and self-host parity still active. Its macOS C-only job passed
  64 of 65 steps and failed only in `sot_authority_edge_smoke.sh`: the newly
  added leaf identity fact owner had no authority/derived registry row. It is
  now classified as a `local_view` of
  `projection.direct_mir_scalar_cfg_program_extension`; the focused local gate
  passes with 86 authorities, 169 derived carriers, and the unchanged status
  census `CLOSED=49 BRIDGE=36 ACTIVE=1`. No result is claimed for the three
  jobs that had not completed at observation.
- Objective card: objective = keep native policy oracles native and make every
  duplicate formal-parameter leaf in one admitted expression consume the same
  latest MIR value without changing parameter identity across a later target
  lane; priority = MIR value identity, owner-directed leaf facts, old direct
  read deletion, negative ratchet, then patch size; fact owner =
  `DirectMirScalarCfgLeafOperandFromOwners` plus
  `DirectMirScalarProgramLeafIdentityFactFromOwners`; last consumer = scalar
  program expression admission before C/LLVM emission; forbidden = plain-C
  oracle delegation, direct `DirectMirScalarCfgUseValueRow` reads in admission,
  or an AST formal-parameter fallback that overrides an already bound leaf in
  the same expression; falsifier = wrapper/collection/aggregate C-oracle/C/LLVM
  parity, component removed-path ratchets, and the full 29-job clean matrix.
- The wrapper, collection, and aggregate native oracle legs now pass
  `--native-pipeline`; their self-host C/LLVM probe legs remain unchanged.
  `tests/self_hosted_component_contract_smoke.sh` rejects removal of the three
  explicit native boundaries. This follows the already canonical semantic
  parity oracle pattern instead of weakening the installed-driver contract.
- After that harness correction, the reached LLVM execution defect was
  concrete: `SemanticCanonicalTypeName("Option<Int: Int>")` produced
  `Option<>`, while C produced `Option<Int>`. Generated LLVM for
  `Substring(expr, start, end - start)` loaded the latest local for the first
  `start` but the original `%pgy.param` for the duplicate second leaf. The
  admission owner had a direct current-use probe beside the canonical leaf
  operand owner; the second occurrence therefore fell through to AST formal
  identity. Admission now consumes only the leaf operand owner. A compact leaf
  identity fact gives same-expression bound locals priority while preserving a
  formal parameter for a genuinely unresolved or prior-lane target use. The
  old direct use read is deleted and rejected structurally.
- A final current-source Pergyra-built DRV-2 was installed with SHA-256
  `D20E1EFDBDB3917156BB0FF7DB09C823389B099BECA5929703AE6AB25DD20F28`.
  With that exact driver, `wrapper_policy_probe_parity.sh` passes its native
  oracle, self-host C, self-host LLVM, target-drift negative, and both wrapper
  diagnostic negatives. `collection_policy_probe_parity.sh` and
  `aggregate_field_policy_probe_parity.sh` passed after the semantic fix and
  before the responsibility-only owner split; the final wrapper run proves the
  split owner is present in the installed production graph.
- `tests/self_hosted_component_contract_smoke.sh` passes after enforcing the
  admission owner at 445/445 lines and the new leaf identity fact owner at
  44/55 lines. Shell syntax checks for all edited parity/component scripts and
  `git diff --check` pass. `tests/documentation_quality_smoke.sh` and
  `tests/self_host_ci_profile_smoke.sh` also pass. Project progress remains 78%
  overall, strict beta 83%, and hard SoT 49/86; this is a reached executable/CI
  tail closure, not a new top-level registry row.
- Next executable rung: publish the derived-fact classification and this
  refreshed handoff, then inspect the newly triggered clean GitHub Actions run.
  A 29/29 green matrix closes this CI tail. If red, resume only from its first
  deterministic failure; do not reopen general SoT, cache, shard, timeout, or
  memory-allowance work.

## Previous self-host context - exhaustive parity provenance-tail verification (inactive)

- The exact compiler-semantic checkpoint is commit
  `7230cd07a416b5e2a4215f91f7393a809f3cb409` on `main`. Following publication
  successors only restore ratchet-compatible signature formatting, repair CI
  provisioning/static pins, bound exhaustive-gate execution cost, or repair
  LLVM artifact materialization cost without changing emitted semantics. The
  current executable resource checkpoint is commit
  `eca0a68549b4a0f60174b4606d2a3ae3949ea07c`. Commit `2eb3bc2d` closes the
  deterministic initializer-projection provenance failure reached only after
  that resource checkpoint completed; this handoff refresh is its
  documentation-only successor. After publication, the only remaining
  worktree path is the unrelated untracked `pgy-80135c2c/`; do not stage,
  discard, or rewrite it.
- Remote run `32461860319` at predecessor `287868a609fc9c7589163858500942cca929d244`
  had 26 green jobs and one deterministic red job,
  `self-host-parity-linux`. The reached production failure was callable
  admission for an exact `Array<String>` `owner-handle`, not another CI timeout
  and not an independent general SoT-cleanup queue.
- Objective card: objective = make the production self-host C/LLVM projection
  consume the admitted routine signature, carriage, expression, collection,
  and logical-record facts without a backend-local fallback; priority = exact
  semantic identity, owner-directed facts, fallback removal, negative ratchet,
  then patch size; fact owners = routine signature/partition, collection ABI,
  logical-record ABI, and expression arena; last consumers = callable routing,
  mutation/drop readiness, local binding, and C/LLVM call/expression emission;
  forbidden = function-name/source inference, generic owner-handle admission,
  eager boolean evaluation presented as short-circuiting, direct backend reads
  that bypass the shared mutation policy, or value-result forwarding without a
  local copy-in/copy-out binding; falsifier = focused assignment projection
  parity plus LLVM/C validation of the admitted 53.6 MB, 1,988-routine
  production MIR.
- Commit `7230cd07` admits the exact owned `Array<String>` parameter shape,
  carries its stable parameter identity through mutation/drop and local
  binding, and centralizes the Array<String> parameter mutation decision before
  the C/LLVM consumers. Value-result `Array<Int>`, `Array<Bool>`,
  `Array<String>`, and logical-record arrays now use explicit local
  copy-in/copy-out facts; readonly logical-record arguments have separate
  member and parameter binding owners. String/numeric comparison operands and
  logical-record member receivers must be proven non-trapping before LLVM may
  avoid a short-circuit CFG.
- The final source generated and linked `fixed-driver18`. With that exact
  driver, `assignment_projection_probe_parity.sh` passed its C/LLVM positive
  paths and missing-fact negatives. The production MIR projected to LLVM and
  `llvm-as` accepted it; the same MIR projected to C and GCC compiled it to an
  object. `tests/self_hosted_component_contract_smoke.sh`,
  `tests/documentation_quality_smoke.sh`,
  `tests/self_host_ci_profile_smoke.sh`, and `git diff --check` are green.
  Temporary driver/MIR/backend artifacts remain ignored and do not count as
  progress.
- First publication run `32499479384` reached 24 green jobs before
  `build-macos-c-only` failed the shared self-host-likeness ratchet:
  `core_string_munge_sig` was 77 with a maximum of 76. The increase came from
  folding the pre-existing C array-push materialization signature onto one
  line while satisfying an owner line cap, not from a new text transform.
  Restoring the multi-line signature and recovering the line in the shared
  mutation-policy call returns the measured surface to 76/76 without relaxing
  either cap or changing emitted behavior.
- Second publication run `32501978243` made macOS, Windows, both bootstrap
  jobs, sanitizers, TSan, Rocq, and all 20 backend shards green. Its
  `build-linux` job reached `self-host-execution-lane-parity-test-smoke` and
  failed only while linking the LLVM projection because Ubuntu could not find
  `-lomp`. The exhaustive parity job already installed `libomp-dev`; the
  integrated Linux job did not. The workflow now installs that exact link
  dependency in `build-linux`, and `self_host_ci_profile_smoke.sh` rejects its
  removal. This is runner provisioning evidence, not a new compiler-semantic
  failure or substitution delta.
- Third publication run `32506892987` installed `libomp-dev` successfully in
  `build-linux` and kept that job alive beyond the predecessor's 33m18s
  `-lomp` failure point. It reached 25 green jobs before macOS reported two
  static contract failures: the beta-readiness and formal-semantics gates still
  required the predecessor apt command byte-for-byte. Both pins now require
  the same `libomp-dev`-bearing command as the workflow. This is CI contract
  migration fallout; the macOS compiler/runtime surface itself passed through
  the exhaustive C-only self-host parity before the static pins failed.
- Fourth publication run `32510788380` at `644d7dd3` made 28 jobs green,
  including `build-linux`, macOS, Windows, both bootstrap jobs, and every
  backend shard. `build-linux` passed the formerly failing OpenMP execution
  lane and its complete `test-all` tail. The remaining exhaustive parity job
  completed the 1,566-source ledger, incremental/impact checks, and C/LLVM
  85-fixture codegen parity twice; both attempts then received an external
  GitHub runner shutdown while compiling the assignment projection probe. No
  assertion failed and neither attempt reached its 90-minute timeout.
- Fifth publication run `32522281489` executed that exact `clean-scratch`
  boundary, then received the same runner shutdown 3m22s after entering the
  first assignment C compile. The deletion therefore falsifies the scratch-
  directory lifetime hypothesis; it must not remain as an alleged fix. The run
  ended with 27 green jobs, the parity failure, and Windows cancelled by the
  next publication. No assignment assertion or missing-fact negative failed.
- Sixth publication run `32528582687` at `f1ff39b9` moved the unchanged
  assignment gate before the cumulative ledger/codegen corpus. It still
  received the runner shutdown 3m19s after entering the first C compile, which
  falsifies cumulative-work ordering as the cause. The first-falsifier order
  remains only as a fail-fast budget rule. At observation, 26 jobs are green,
  parity is the sole failure, and two long platform jobs remain active.
- An apples-to-apples local C-only measurement with the same installed stale
  driver took 64.45s and 1,860,136 KiB maximum resident set at the default
  release profile, versus 32.12s and 359,480 KiB with `--opt=dev`. The root
  source is only 7.9 KiB, but its imported owner closure is the fixed compiler-
  scale projection. Both profiles use the installed self-host artifact owners;
  the gate owns projection semantics and fail-closed runtime behavior, not
  optimizer throughput. Commit `666db88d` therefore keeps both C/LLVM positive
  paths and every missing-fact negative while compiling their artifacts at
  `-O0`. A fresh current-source release pair then passed the complete focused
  gate locally in 4m30.19s. The component contract, CI profile, documentation
  quality, shell syntax, and diff checks are green.
- Seventh publication run `32532254522` at `289c019c` used that dev profile,
  but the runner still shut down 2m27s after entering the first C compile. No
  assignment assertion ran or failed. This falsifies final-artifact `-O3` cost
  as the sole cause; the remaining ambiguity is self-host projection resource
  pressure versus an external runner-service termination. At observation, 24
  jobs are green, parity is the sole failure, and four platform jobs remain
  active.
- GitHub's runner message is a generic service/VM shutdown report, not an OOM
  or compiler diagnostic; the official runner tracker contains the same signal
  even in cases with live log streaming and healthy memory
  (`actions/runner#4492`, `actions/runner-images#6709`). Commit `5d214902`
  therefore adds a bounded 15-second observer only around the reached
  assignment compiler child. It reports recursive process RSS, Linux
  `MemAvailable`, and swap without changing source, backend coverage, runtime
  negatives, timeout, or artifact authority. The local C gate emitted the
  heartbeat and passed; component/profile/docs/syntax/diff ratchets are green.
- Eighth publication run `32533689600` at `67f24eb6` removed that ambiguity.
  The assignment C artifact completed. During the LLVM artifact, recursive RSS
  rose from 1,860,012 KiB at 120 seconds to 8,157,532 KiB at 135 seconds;
  system `MemAvailable` then fell to 352,952 KiB and swap was consumed before
  the runner terminated the compiler. This is direct compiler-scale
  materialization pressure, not an assignment semantic assertion, optimizer
  tail, scratch lifetime, cumulative ordering, or unexplained external
  shutdown.
- The reached owner was `DirectMirScalarCfgEmitProgramLlvm`: for each of about
  1,983 routines it appended a completed local string with
  `output = Concat(output, routine)`, copying the already-built program again.
  Commit `b1a57150` preserves the exact preamble and routine order but gives
  the program-global serialization to one `TextBuilder`, retires each owned
  routine string after its last append, and rejects restoration of the
  program-global `Concat` path in the component ratchet. This is a bounded
  execution-resource repair at the reached LLVM artifact owner, not a cache,
  shard, timeout, swap allowance, or new semantic authority.
- A fresh Pergyra-built `pgy-self-driver.exe` was generated from the modified
  source graph. The focused assignment C/LLVM parity gate then passed the
  positive output and all missing-fact negatives; C completed after its first
  heartbeat and LLVM completed shortly after its 150-second heartbeat instead
  of terminating at the former resource boundary. The component structural inventory,
  line cap, removed-path ratchets, and `git diff --check` are green. Windows
  MSYS cannot report useful recursive RSS for the Windows compiler child, so
  the clean Ubuntu observer remains the exact memory falsifier.
- Ninth publication run `32536152587` at `7684fa0d` proved that the outer
  program builder was necessary but not sufficient. At 135 seconds the LLVM
  subtree was only 2,029,056 KiB, down from the predecessor's 8,157,532 KiB,
  but at 150 seconds it jumped to 15,390,080 KiB; `MemAvailable` fell to
  182,972 KiB and swap to 917,196 KiB before shutdown. At observation the run
  had 26 green jobs, parity as its one failure, Windows still active, and
  `build-linux` cancelled by the runner loss.
- Direct process observation proved the late spike still belonged to
  `pgy-self-driver`, not clang. Bounded stage instrumentation then isolated
  `DirectMirScalarProgramLlvmStringGlobals`: the process entered that owner at
  2,005,300 KiB and returned at 30,139,972 KiB, while each following support
  block changed RSS by only a few MiB. The owner serialized 9,896 admitted
  string literal rows totaling 848,978 bytes by repeatedly concatenating the
  entire accumulated prefix, retaining about 28 GiB of intermediate copies.
- Commit `eca0a685` moves only that admitted string-global serialization to a
  `TextBuilder`, retires non-empty literal payload fragments and the completed
  globals fragment after their final append, and fails closed against treating
  static empty strings as owned allocations. The component ratchet requires
  the builder/finish/lifetime guards and rejects restoration of the old
  program-global `Concat` path. No cache, shard, timeout, swap increase, or
  alternate artifact owner was introduced.
- A final source-built driver projected the fresh 53,566,745-byte MIR directly
  to an 11,642,817-byte LLVM artifact with exit 0. Peak working set was
  5,117.7 MiB, versus 33,033.7 MiB before this owner fix. The complete focused
  C/LLVM assignment parity then passed its positive output and all missing-fact
  negatives, including empty string literal coverage. The component structural
  inventory, 350-line owner cap, removed-path ratchets, and diff checks are
  green. Temporary MIR/LLVM/diagnostic drivers are ignored measurement
  artifacts and do not count as substitution progress.
- Tenth publication run `32540324732` at `1dcec434` proved the assignment LLVM
  resource rung closed: recursive RSS rose through 4,742,280 KiB at 150
  seconds, fell to 243,276 KiB at 165 seconds, and the semantic assignment
  projection completed. The unchanged cumulative tail then passed all 1,566
  lexer/parser/semantic/codegen rows, the incremental and impact checks, and
  C/LLVM parity for all 85 codegen fixtures. `build-linux` and 26 other jobs
  are green; Windows was still active at observation. The sole deterministic
  red stopped in `initializer_projection_probe_parity.sh` before any message:
  its first synthetic AST reached `artifact_lower_owner.pgy` without the now-
  required parser-owned source-module fact and correctly failed closed with
  `mir_source_module_path_missing`.
- Commit `2eb3bc2d` binds every synthetic initializer-probe AST through
  `CanonicalMirIdentityArtifactBindSourceModules`, with the probe source path
  explicitly owned by that fixture boundary. The parity ratchet requires the
  binder and rejects additional bare `AstTreeArtifactFromText` construction,
  so the missing provenance cannot be hidden by an empty/default path. The
  complete local C/LLVM initializer projection gate passes its positive and
  failure cases; the initializer-environment cursor, MIR expression-graph,
  and full component-contract gates are also green.
- Next executable rung: publish `2eb3bc2d` plus this handoff and inspect the
  newest clean GitHub Actions run. The falsifier is the full 29-job set, with
  exhaustive parity completing both the bounded assignment projection and the
  formerly failing initializer-projection tail. A green full matrix closes
  this CI rung. If it is red, resume from its first deterministic failure and
  do not open another general SoT cleanup.

### Previous CI parity closure and budget context (inactive)

- The exact compiler-source checkpoint is commit
  `167d81ee1b385990ad83224df3d61e33ed46bddc` on `main`. Published handoff
  successor `0a600447b64925eb109f593d9e29b56117d40cfc` matches `origin/main`; the
  commit containing this card is a CI-budget successor with no compiler
  semantic change. The only remaining worktree path after publication is the
  unrelated untracked `pgy-80135c2c/`; do not stage, discard, or rewrite it.
- Parent remote evidence was CI run `32408205595` at commit
  `afaee1b198ef9c31f27677bf58ae751e41c4d6fe`. Bootstrap, codegen bootstrap,
  Windows, macOS C-only, sanitizers, TSan, Rocq, and all backend-compare shards
  were green. The two deterministic red jobs were `self-host-parity-linux`
  (first stop: compensation execution parity) and `build-linux` (self-hosted
  driver was incorrectly selected by native compiler-subject gates, plus the
  stale production-header golden).
- The first publication run, `32456845688` at `a8275469`, advanced the parent
  failures and exposed one clean-checkout bootstrap defect: the new exhaustive
  enum-match owner copied `plan.program.expressions` out of borrowed `ref plan`.
  Commit `167d81ee` removes that local binding and reads the nested owner path
  directly. The exact production `driver_bootstrap_main.pgy` entrypoint with
  `--native-pipeline --emit-c` completes locally with 0 errors; the four
  TextBuilder diagnostics printed beside the borrow error were cascading and
  disappear on the corrected source. Successor run `32458203430` then proved
  clean Ubuntu bootstrap in 23m06s; codegen bootstrap, sanitizers, TSan, Rocq,
  macOS C-only, and all 20 backend shards were also green.
- Run `32458203430` exposed one CI-budget defect rather than another compiler
  failure. `self-host-parity-linux` made continuous linear progress through the
  current 1,556-row completeness ledger but the stale 40-minute job ceiling
  cancelled it at codegen row 899, after the job spent about 22 minutes reaching
  the ledger. Recent history contains the same 40-minute cancellation shape,
  while the last successful July runs were 26.6-33.7 minutes on a smaller source
  set. The workflow and its profile ratchet now own a 90-minute hang budget:
  enough for the measured cold-checkout ledger and parity tail, still bounded
  far below GitHub's 360-minute default. Do not report the cancelled job as a
  semantic red or bypass its exhaustive surface.
- Objective card: objective = make those two CI jobs green without widening
  self-host authority; priority = executable C/LLVM parity, explicit owner
  identity, old-path rejection, then structural ratchets; fact owners = the
  direct-MIR payload-free enum declaration/match graph and the intent forward
  trace transition; last consumers = direct C/LLVM emission and installed
  compensation execution; forbidden = node-spelling enum inference, duplicate
  trace materialization during cleanup, or letting native compiler gates
  silently exercise the installed self-host sibling; falsifier = a current
  `main` CI run in which both jobs pass from clean Ubuntu checkout.
- The direct-MIR scalar program now admits payload-free enum values across
  parameters, return values, logical-record fields, direct calls, equality,
  and exhaustive match CFGs through declaration-keyed enum facts. Match
  admission proves one stable scrutinee, unique variant ordinals, exact CFG
  edges, and the otherwise-empty terminal fallthrough; malformed declaration,
  payload, physical ABI, graph owner, duplicate variant, or non-exhaustive
  shapes fail closed. There is no source-text or node-shape fallback.
- Intent compensation still restores bound values, but cleanup no longer emits
  the forward-path materialize/transfer observability events a second time.
  C and LLVM consume the same explicit `emit_observability=false` cleanup fact;
  normal execution remains `true`. The installed parity fixture compares
  emitted C and all runtime outputs through the repository comparator and uses
  the exact native C/LLVM route for its oracle.
- Native compiler-subject gates now declare `PGY_NATIVE_PIPELINE=1`; the
  gate-subject ratchet reports 54 declared subjects. This removes the false
  authority handoff that made channel, region, runtime-frontier, campaign, and
  emitted-C gates fail on syntax outside the current direct self-host slice.
  Imported self-host parsing also recognizes explicit `export` as a token fact
  while ignoring comments and strings, so imported declarations keep stable
  identity without substring inference.
- Observed local green evidence includes: fresh installed-driver C and LLVM
  execution-lane parity (35/35 each), focused payload-free enum C/LLVM parity
  and mutation negatives, compensation self/native C/native LLVM execution,
  ABI specification 83/83 plus C/LLVM ABI pipeline, self-host component and MIR
  declaration ratchets, language-word registry generation, formal Coq tail,
  AIR backend non-impact over all 926 fixtures in both backends, and the
  production-header owner-size check with the canonical 728-header golden.
  The Linux parser/semantic/codegen platform suites passed 189/114/85.
- Local WSL is a mixed Windows-driver environment. Absolute `/mnt/d/...`
  source identities and `/tmp` sibling discovery can differ from a clean Linux
  runner; those cases were rerun through the owning native/Windows route and
  are not reported as product failures. The separate Windows/MSYS filtered
  `driver_rung2_body_parity.sh` rerun ended at local oracle MIR canonicalization
  in that mixed environment and is not counted as green or as a product failure.
- Next executable rung: publish the bounded CI-budget repair and inspect the
  successor GitHub Actions run. The first falsifier is the exact
  `self-host-parity-linux` continuation beyond completeness codegen row 899;
  `build-linux` and `build-windows` remain independent clean-run witnesses. If
  any job fails, resume from that first clean-run failure; do not open another
  general SoT cleanup.

### Previous DIR-to-MIR lifetime and CI context (inactive)

- HEAD is `c363a94a14091f8e479607c4550c054272fd7e1f` on `main` and matches
  `origin/main`. The previous accumulated frontier was committed externally
  while this session was running. The remaining local delta is intentionally
  uncommitted at 27 paths (26 tracked and the unrelated untracked
  `pgy-80135c2c/` directory); do not stage, discard, or rewrite it.
- The next reached failure after the match-pattern lifetime closure was an
  executable control-flow defect, not an SoT inventory gap. The previous
  oracle lowered every direct `else if` tail through recursive
  `SelfMirLowerIfFromArtifact -> SelfMirLowerBlockFromArtifact` calls. A
  41-condition production-shaped fixture exited with Windows status
  `0xC00000FD` (stack overflow), and the compiler-scale source stopped at
  `mir-facts:routine:2743:start` in
  `SelfDirIntentStepClauseFactsFromArtifact`.
- `SelfMirIfElseNestedIfRoot` now identifies only a direct syntactic nested-If
  tail. `SelfMirLowerIfFromArtifact` descends that tail with explicit admitted
  entry/then frames and unwinds merges in reverse order through the existing
  `SelfMirMergeIfBranches` owner. Ordinary nested If statements inside blocks
  retain their normal recursive block semantics. Missing statement facts,
  kind drift, frame cardinality drift, and invalid version slices all fail
  closed; there is no source splitting, stack-size increase, cache, shard, or
  fallback parser.
- The 41-condition fixture now completes with the Pergyra-built seed and the
  native oracle. Their verified MIR is byte-equal at 90,132 bytes, the emitted
  C is byte-equal at 11,788 bytes, and the compiled program prints the exact
  `neg`, `zero`, `small`, `big` oracle. Adjacent `if_else_assign`, `nestedif`,
  and `nested_if_in_loop` MIR artifacts are also byte-equal, preserving SSA
  merge and lexical-local restoration behavior.
- The canonical full gate
  `tests/self_hosted/parity/driver_bootstrap.sh` ran with
  `PGY_SELFHOST_DRIVER_FULL_FIXPOINT=1` and the owned release flags
  `-O3 -fwrapv -fno-strict-aliasing`. It exited 0. The 5,903,397-byte
  Pergyra-built seed and 6,554,592-byte native oracle independently emitted
  byte-identical 232,242,252-byte MIR, SHA-256
  `47679723ED88B38972ACCA78488268277EF7BDFCD3980D33F60DCDC7CDA10F48`.
  The O3-compiled 5,981,622-byte gen2 then consumed the same MIR and reproduced
  byte-identical 10,265,701-byte gen2/gen3 C (157,247 lines), SHA-256
  `9187E188FBA6C0EC405643E14D6A33197B34E025AEA7677962C5214BBE88D0C1`.
  This upgrades the earlier O2 functional evidence to the canonical local
  release profile; it is still not committed, installed, or remote-CI proof.
- The existing install transaction was then exercised without touching
  `bin/`: `self_host_compiler_build.sh` installed an isolated 5,903,397-byte
  candidate, SHA-256
  `1D06C4707D592BF386AEA719B4793C325E33036D2627697767BB5CC4B0C29EB4`.
  Typed-source emission, O3 host compile, bounded source smoke, and native
  machine-manifest replay all passed; source and replay manifests are
  byte-identical at 1,144 bytes/SHA-256
  `0A83B0DB5EFE3C00C6D9413C63045C4B17AFF079781213B280442C588E5A9C19`.
  `installed_driver_cli_mode_owner.sh` also passes against this candidate,
  keeping source-C, source-MIR, and MIR-C stdout/artifact effects disjoint.
  This is isolated installer evidence, not proof that the repository's public
  sibling binary or a remote release has been promoted.
- The isolated public sibling boundary now also executes on the same revision.
  A fresh native release/LTO launcher was built from the current tree beside
  that candidate without replacing `bin/`: `pgy.exe` is 3,384,801 bytes/SHA-256
  `C0D3605AD41BAD30DB36ECC3C6DFFF936B85470559EA486390A2412C94D9C07D`.
  The first exact public-MIR run proved that sibling selection already worked,
  then exposed one real identity split: native import resolution serialized an
  absolute Windows path while the delegated producer retained the relative
  argv spelling. The public source handoff now consumes the existing
  `import_resolver_canonicalize_path_dup` owner, whose Windows spelling is
  normalized once to `/`; no MIR-field deletion or comparison fallback was
  added. That identity spelling is limited to MIR/C source handoffs: public
  `--tokens`, `--ast`, capability-manifest, and DIR stdout retain the user's
  relative argv spelling, and all four executable fail-closed gates pass.
  Public and direct self-host MIR are byte-identical at 59,402 bytes/
  SHA-256 `447440EC0547886CBC0216C70F6466FDF4B4E10A84D3F7C2149CC6072038F491`,
  and native/self canonical MIR is byte-identical at 64,494 bytes/SHA-256
  `CADA3C569501FD2CB18E071D5F0B0A89DF195B57690E8923B2F9D8A344B16DE9`.
  Public `--emit-c` and plain C compile/run gates also pass, including missing-
  sibling and unsupported-option fail-closed cases. The existing Linux parity
  job now invokes the public-MIR replacement gate directly, and the CI-profile
  plus hard-substitution contract gates are green. Repository `bin/`, commit,
  and remote CI state remain unchanged.
- The release installation boundary is now coherent. Before this change,
  `all` and `release` built the public launcher without the sibling that owns
  every ordinary source compile, so a clean user-facing build could succeed
  and still be unusable. Both targets now depend on the existing
  `self-host-compiler` installer; the serial Linux parity job starts with
  `make release` and later public gates consume that same pair. One isolated
  staging invocation completed without touching `bin/` and installed
  `pgy.exe` (3,384,801 bytes/SHA-256
  `9683E04AF8FC29378EA85C7306609FA77A6DCA59C895B6907DF6A595F951C2FA`),
  `pgy-self-driver.exe` (5,903,397 bytes/SHA-256
  `D6064B1050A1EE0E449DBBF268B269C3D1AD3E878B178A013B875A49C59AA1D3`),
  and the 1,144-byte manifest/SHA-256
  `0A83B0DB5EFE3C00C6D9413C63045C4B17AFF079781213B280442C588E5A9C19`
  in one `BIN_DIR`. With no sibling override, installed CLI-mode, public MIR,
  public C emission, and plain C compile/run gates all pass. The first staging
  attempt was externally terminated with Windows status `0x40010004` during a
  deliberately interrupted turn and produced no sibling; the owner-clean
  rerun completed, so it is not recorded as a compiler failure. An intentional
  repository `bin/` promotion remains unobserved.
- The reached repeat-build seam is also locally closed without skipping typed-
  source emission. Installer schema
  `pgy.selfhost.compiler-build.v4-source-artifact` no longer hashes the
  relink-volatile codegen PE: the normalized generated C, native machine
  manifest, runtime-header inventory, output identity, compiler profile/flags,
  and compiler version own the host-compile key. A one-byte PE-overlay seed
  changed the seed SHA-256 from
  `4B655A49B5DFD6BAD4159C0A8916FCF3BB1FD206EB2E6AEC3B05B406059E5A5A`
  to `15AC11E920A1646815EE649A88809C76D196B8663D7BB3DF2600B5A73D4BF2E1`
  while emitting the same 9,850,372-byte C; the installer printed
  `reusing fingerprinted Pergyra-built driver`, and the 5,903,397-byte driver
  retained SHA-256
  `E32850D01A68074CF7E713AE3FC3299671FB6B5724C885B1F38D4B0B958C08D0`
  and the exact same mtime. A full staging `make all` then regenerated gen2
  with SHA-256
  `39D8B961E187A6A9D9E0F30F0D241A49188C39F048A5B5C92C46731F9D2763FD`
  but emitted C remained SHA-256
  `512B512339A70444DAF599361FED30A1CB8F716126E35C3F63FEC94C5C10B0E2`
  and the installed driver was again reused without relink. An invalid compile
  profile now fails before source emission and cannot pass through a cached
  output. Hard-contract, CI-profile, public default-C emit, and shell syntax
  gates are green. Codegen seed generation itself remains phony/repeated and
  is the next measured build-cost seam; no cache architecture has been added.
- Commit `c363a94a` remote run `32071813850` completed with failure. TSan,
  sanitizers, Rocq, codegen bootstrap, and most backend shards were green. The
  deterministic failures were stale default-build inventory, stale generated
  language-word inventory, the 2,600-line beta-status cap, backend shard 18
  (`allocator_defer_cleanup`), backend shard 19 (`intent_trace_compensate`),
  one silent installed intent-observability compile phase, and a macOS C-only
  composite MIR assertion. The self-host full-bootstrap job was cancelled
  while installing dependencies and is not compiler evidence.
- Current local source closes the two backend execution failures. Defer
  registration now carries its originating MIR instruction to delayed C/LLVM
  emission, so `AllocatorDestroy` consumes the attached runtime-call ABI row;
  compensation uses the normal path's final trace materialization. The MIR
  suite is 162/162 and the exact backend pair is 2/2. The C allocator emitter
  also fails closed instead of spelling a runtime fallback locally.
- Build inventory, language registry generation, beta status,
  ABI shape, hard self-host contract, CI profile, and installed
  intent-observability C/LLVM execution are locally green. The installed gate
  now prints the failing compile phase and its captured stdout/stderr instead
  of exiting silently. The macOS composite retains all nine acceptance clauses
  and now reports the exact false clause/diagnostic. Ubuntu GCC reproduced that
  failure as `step zone identity cross-seal`: MIR had shallow-borrowed DIR-owned
  `step->where_type_name` after `dir_destroy`. Materialization now copies the
  spelling into routine scratch; the static owner gate requires the copy and
  rejects the old borrow. Ubuntu GCC normal/ASan+UBSan and Windows GCC/Clang
  C-only core MIR all pass 162/162. Linux `test-asan` now owns `test_mir` beside
  AIR/semantic/parser; its intentional UAF witness, 40-source compiler corpus,
  and all four sanitizer batteries pass in one isolated run. The native typed-intent gate now explicitly
  uses `--native-pipeline` for source emission/execution, checks all 32 fixture
  lines, and passes C/LLVM with an intentionally missing sibling. The separate
  prebuilt Pergyra driver also passes self C/native C/native LLVM v3 zone,
  compensation, and history parity. Current remote Apple/Linux runners are still required.
- `driver_rung2_if_graph_use_owner.sh` and shell syntax checks are green. The
  broader filtered body-parity command remains blocked before the selected
  fixture by the unrelated existing domain-topology structural guard
  (`constructor_fields` inventory). The broad component inventory reaches its
  independent existing cap debt:
  `direct_mir_scalar_cfg_program_expression_identity_readiness_owner.pgy` is
  163 lines against a 125-line cap. Do not misreport either broad gate as green,
  and do not reopen this control-flow owner merely to absorb those independent
  failures.
- Next objective card: objective = verify the DIR-to-MIR lifetime correction on
  current platform jobs, then continue the installed self-host/bootstrap rung;
  priority = exact zone identity, owned lifetime, runtime behavior, current CI,
  then installer reuse; fact owner = DIR intent-step zone identity, copied at
  MIR materialization into routine scratch; last consumers = MIR validation,
  serialization, and typed-intent C/LLVM execution; forbidden = borrowing DIR
  storage past `dir_destroy`, weakening the macOS assertion, stale driver reuse,
  another broad SoT cleanup, cache/shard/retry, or cap/timeout increase;
  falsifier = MIR 162/162 under normal and sanitizer C-only builds, self/native
  typed-intent execution, and a current macOS/Linux remote run. Progress is
  executable substitution and current CI, not registry or gate count.

### Previous match-pattern ready-artifact context (inactive)

- HEAD is `330c82ca5f679d2b41d42c16c72ddb2049113101` on `main`.
  The intentionally uncommitted worktree currently has 874 dirty paths (438
  tracked and 436 untracked). Preserve all unrelated accumulated work; do not
  stage, discard, or rewrite it.
- The refreshed Pergyra-built codegen seed is
  `.tmp/self_hosted/codegen/collection-route-exact-v1-20260817/gen2.exe`,
  2,490,793 bytes, SHA-256
  `EF39A1AC8BFD79BBAF375115232D24ADCFD72B9EB8F45CF68B45AC3A8E8D33B9`.
  The current measured DRV-2 candidate is
  `.tmp/self_hosted/compiler/match-pattern-ready-v1-20260818/driver_seed.exe`,
  5,892,010 bytes, SHA-256
  `45996B0F624711B7D0F5F2E23CBC337F3F7972BE0CD40664688B039A0666BBDC`.
  It was emitted by that Pergyra-built seed and host-compiled with the canonical
  release flags. It is fixed-point evidence for this slice, but has not yet
  replaced the installed/release compiler.
- `DirectMirCollectionProgramRouteFactFromAdmitted` now claims only the exact
  three-routine envelope: one `Main`, one Array producer, and one Array
  consumer. The exclusive multi-routine projection reads that fact before the
  broad scalar route, and the terminal owner no longer reconstructs or retries
  the collection decision. This prevents a compiler-scale program containing
  unrelated matching routines from being misclassified while keeping malformed
  members of the exact family fail-closed.
- The shared Array<Int> ABI projection now derives the C `%lld` argument cast
  from `StringRuntimeCLongPrintArgumentType`; it no longer pairs `%lld` with
  the canonical 32-bit Pergyra `Int` storage type. The focused collection gate
  passes exact base `12/4`, alternate `20/5`, C/LLVM parity, routine-row-order
  equality, and ABI/call/return/collision negatives. The updated MIR receipt
  SHA-256 is
  `C473FF46F33690E8C47231F959C9DE45A2856E1B7B9FA5D60F58D561926A968C`,
  independently reproduced by the previous and current drivers.
- A diagnostic-only generated-C allocation census localized the producer peak
  to one repeated proof. During body analysis,
  `AstMatchCasePatternFactFromArtifact` reopened `AstTreeArtifactReady` once per
  visible match-case ancestor. The fixed input executed that call exactly 214
  times; the nested `AstExpressionGraphRowsReady` requested 448,789,672 bytes
  for `seen` growth and 224,391,840 bytes for traversal-stack growth. The
  checked match-pattern API still proves the whole artifact. Its new
  ready-artifact projection keeps the exact node/kind/atom checks, and only the
  already-admitted semantic match-binding path consumes it. A focused negative
  gate rejects restoration of whole-artifact or whole-graph readiness there.
- The updated driver published a current-source 232,064,536-byte MIR artifact,
  SHA-256
  `56EF4D76E96E8B8E3F8C63B786803506CC841C18CDC7DF5B353DB91582F820EC`,
  in 102.981 seconds under the unchanged 3,072MiB process-tree cap. Peak
  private memory fell from the adjacent 2.781GiB baseline to 1.753GiB and no
  longer crosses the 2.4GiB attention threshold. Nearest samples show the
  assignment-to-statement increase falling from 612.1MiB to 275.7MiB;
  statement completion fell from 2,084.0MiB to 1,389.3MiB, and body-type
  completion from 2,517.6MiB to 1,474.2MiB. This is a measured lifetime
  reduction, not an inferred SoT/file-count gain.
- DRV-2 consumed that MIR in 85.530 seconds at 1.670GiB peak private and
  emitted 10,257,419-byte gen2 C. The 5,971,259-byte host executable, SHA-256
  `F757BC1DD4B8050FE3D4968EFCFDB817DAD3C9084201BC7D5C3CA618EE3C4FBF`,
  consumed the same MIR in 89.758 seconds at 1.712GiB and emitted byte-identical
  gen3 C. Both C payloads have SHA-256
  `484D5246C782FD7BC70E24B3EE7EE341F9B3D38F962D6786AD4DC0B6B5500608`.
- `one_mir_option_match_projection.sh`, the semantic environment lifetime
  gate, `one_mir_array_param_projection.sh`, the routine-build storage lifetime
  gate, compiler-internal caller provenance C/LLVM gate, caller-registry
  generator check, shell syntax checks, and `self_host_ci_profile_smoke.sh`
  are green. The broad component inventory reaches its known unrelated RED:
  `direct_mir_scalar_cfg_program_expression_identity_readiness_owner.pgy` is
  163 lines against its 125-line cap. No new job, V label, retry, cache, shard,
  timeout, or memory cap was added.
- Next objective card: objective = integrate the measured ready-artifact
  lifetime closure through the existing canonical CI/release boundary before
  opening another SoT family; priority = preserve match-pattern identity,
  checked fail-closed behavior, C/LLVM Option-match parity, current-source
  fixed point, then installed/remote evidence; fact owner =
  `ast_match_pattern_fact_owner.pgy`; last consumer =
  `SemanticAstExpressionSeedMatchCaseBindings` after body admission; forbidden
  = whole-artifact/whole-graph readiness per nested use site, parallel pattern
  parser, cache, shard, retry, timeout, cap increase, or another SoT-only
  commit; falsifier = the focused lifetime and Option-match gates plus canonical
  publication/consumption preserving gen2==gen3 C. Progress is the measured
  producer-lifetime reduction and fixed point above, not another registry row.

## Historical checkpoint archive - previous active card is inactive below

### Previous callable ABI production frontier

- Current HEAD is `330c82ca5f679d2b41d42c16c72ddb2049113101` on
  `main`. The worktree is intentionally uncommitted: 841 paths are dirty
  (`420 tracked`, including tracked deletions, and `421 untracked`) and include the user's accumulated GraphPlan work
  plus the current semantic-provenance closure.
  Do not stage, discard, or rewrite unrelated paths.
- The active executable frontier is no longer an SoT-only cleanup loop. A
  Pergyra-built DRV-2 at
  `.tmp/self_hosted/compiler/dynamic-indexed-phi-final-20260817/driver_seed.exe`
  is 5,863,489 bytes with SHA-256
  `798E9D82CB33E987CBB7F263AD99C6519A0AE6A6BBB189D204B233A5B99A44E9`.
  It closes fixed generic-probe row 5686 without a new opcode or backend route:
  the existing indexed-assignment fact now admits exact value-result
  `Array<Int>` and `Array<String>` targets, consumes a dynamic target index from
  the persisted target graph's right root, then consumes the RHS from the same
  ordered use cursor. Existing C/LLVM array mutation operation identities 37
  and 34 remain the last consumers. The new dynamic `Array<String>` focused
  gate passes exact `left/middle/right` C/LLVM output and ten missing/drifted-
  fact mutations; the existing Array<Int>, Option<Int> try-let, general array
  mutation, and readonly-ref Array<String> gates also pass. The Option<Int>
  source MIR hash receipt was stale: both the prior integrated driver and this
  driver independently emit the same
  `CA0108FA272F2B4B293E0F6D89504B69828F273E5C5A0AC970D7444EE326D61B`
  artifact, so only the pinned receipt changed.
- The same fixed 48,531,749-byte MIR
  (`B2F363EC09097137C032B3A55BE9FD53F7BC57A490056500DE4F9F6CDEC0D729`)
  now advances through row 5688. The exact defect was not the common PHI or
  either backend: indexed-assignment results were omitted from LocalRef/value
  inventory, so `generic_constraints.43` could not join the parameter local.
  The exclusion is deleted; the result remains an SSA value of the exact
  parameter LocalRef and the existing operation 29 consumes the join. A focused
  loop fixture reproduced the old C/LLVM `stage=phi` failure before the change
  and now passes local plus value-result Array collection PHIs and a foreign-
  incoming negative. The fixed MIR's next exact RED is global row 6450 in
  routine 596 `SemanticAstLocalBindingFactsContractReady`: a 117-node logical-
  record constructor fails expression admission at the first `Array<Int>`
  literal element, `stage=left-edge node=15`. That aggregate literal admission,
  not a new SoT inventory, V, cache, shard, timeout, or memory-cap, is the next
  executable seam.
- The CI profile inventory is green with the dynamic indexed-assignment,
  readonly-ref, explicit-return, and namespace-internal focused gates wired into
  the existing GraphPlan job. The new expression and routine owners remain at
  their existing caps, 445/445 and 430/430; no cap was raised and no pass-through
  owner was added. The complete structural component inventory passes those new
  contracts and remains RED only at its next pre-existing failure:
  `direct_mir_scalar_cfg_program_expression_identity_readiness_owner.pgy` at
  157 lines against the existing 125-line cap. Several preceding integration
  cap failures were removed by moving direct-call argument carriage into the
  existing C/LLVM direct-call owners; no cap was raised. Do not describe the
  component inventory as green or split the remaining expression-identity
  owner merely to make this unrelated active-rung check disappear.
- A local attempt to invoke the new focused Make target through the standalone
  MSYS `make.exe` crossed the Makefile's compiler prerequisite first. Its
  configuration stamp removed the generated `build/**/*.o` and `build/**/*.d`
  cache, then that shell failed because `gcc` was absent from its PATH. Source
  files and `bin/pgy.exe` were not changed; the build cache now contains zero
  object/dependency files and is recoverable by the next canonical compiler
  build. The target's exact script recipe was executed directly with the
  installed compiler and passed. Do not report the unobserved Make wrapper or
  a rebuilt native compiler as green.
- This worktree is not one coherent SoT closure. The 418 untracked paths span
  compiler owners, parity paths, fixtures, and supporting artifacts, while the
  top-level registry remains exactly `49 CLOSED / 36 BRIDGE / 1 ACTIVE`, the
  same census as HEAD. The fixed 1,660-routine publication and executable MIR
  verification are real substitution evidence, but the owner/V/fixture breadth
  beyond that merge boundary is not a progress numerator. Freeze new GraphPlan
  owner and V expansion until the current provenance slice and production
  bootstrap are integrated and the accumulated diff is deliberately reduced.
- The latest audit therefore gives a mixed verdict, not a flattering global
  one. The 838-path aggregate is still unintegrated churn and contributes zero
  to the SoT numerator. The active codegen lifetime slice is genuine hard
  substitution: the semantic expression graph remains the one fact owner,
  recursive owned C-expression fragments are retired after the selected root,
  and owned let/assignment/bind/log statement fragments are retired only after
  the final prefixed line is materialized. The old nested statement-line copy
  is negative-gated. No new V, GraphPlan family, cache, shard, timeout, or
  memory cap was added.
- The 2026-08-17 audit separates that broad aggregate from the active slice.
  The aggregate is still sprawling: 418 untracked paths and an unchanged
  `49 CLOSED / 36 BRIDGE / 1 ACTIVE` registry mean that file, owner, fixture,
  and V growth cannot be counted as SoT closure. The active String/scalar-ABI
  slice is a real closure: three independent MIR producers agree on the same
  normalized String fixture identity; duplicate checked-division name
  projections were replaced by one typed runtime-symbol fact; the C type
  consumer now preserves `Int -> int32_t` and `Long -> int64_t`; and old shared
  type/name paths are negative-gated. This is bounded hard substitution inside
  an otherwise unintegrated worktree, not a claim that the whole aggregate is
  coherent.
- The current active closure preserves parser-owned top-level source-module
  provenance across the MIR wire. Native and self-host declaration/routine
  producers emit `source_module_path`; MIR declaration/routine admission now
  rejects missing or empty paths; canonical MIR-to-AST reconstruction binds
  the aligned paths through the existing `AstSourceModuleFacts` owner and
  reseals the artifact identity digest. The old provenance-free AST fallback,
  owner-name/signature-only internal-builtin admission, invented unknown path,
  and early provenance backing retirement remain forbidden. The focused
  legacy/artifact C+LLVM gate passes both exact-owner admission and wrong-path,
  external, and missing-path rejection. Component inventory, CI profile,
  CI step runner, and the 13-early/7-final AST storage lifetime gate are green.
  CI executes both focused provenance and lifetime gates in the existing
  serial self-host parity job.
- The reached `CompilerRetireArrayStorage` provenance seam is now closed through
  the production fixed point, not just through source inventory. A current
  isolated codegen seed completed in 235,185 ms at 1.516 GiB peak private; its
  `gen2.exe` SHA-256 is
  `DC812B83506996CFE58541B24A7AFA68398B7B2764AB76CE18B1DD8B94003FB2`.
  That seed built the current driver in 233,198 ms at 2.995 GiB peak private.
  The driver then published a 229,290,183-byte verified MIR artifact in
  113,672 ms at 2.816 GiB peak private, SHA-256
  `51470055D3265BB1A1B8B345621DC680D3882F99700FCE0DB4A29D74A6A03122`.
  The artifact contains 7,430 nonempty `source_module_path` fields and zero
  null or empty rows. Production MIR consumption completed in 110,902 ms at
  1.926 GiB peak private and emitted 10,126,081-byte C. Canonical host
  compilation completed in 121,830 ms, and the resulting executable emitted
  gen3 C in 138,638 ms at 1.933 GiB peak private. Gen2 and gen3 C are byte
  equal with SHA-256
  `3401A5DD1269E3489DF78046F67016C721A387765A995A12F72A532D71014F35`.
- Installed-route auditing found one remaining provenance migration gap rather
  than a new owner. Six specialized direct-MIR declaration consumers still
  required the old six/seven-field object shape. They now accept the exact
  seven/eight-field shape, read `source_module_path`, and cross-seal it against
  `MirProgramDeclarationIndex.source_module_paths[row]`; the component gate
  rejects all six retired counts. The focused enum gate then exposed a missing
  `<stdint.h>` after the existing `Int -> int32_t` migration, while the
  payload-free enum CI gate still expected the retired `long long` C spelling.
  Both the emitter and CI expectation are corrected.
- One authoritative rebuild initially exceeded the 3072 MiB process-tree cap:
  `gen2.exe` held 3,063.9 MiB while the concurrent `gen2 | tr` pipeline raised
  aggregate private memory to 3.012 GiB. The existing opt-in source-pressure
  path proved the latest source itself completes through `output:finished` in
  126,598 ms at 2.984 GiB. The canonical build owner now writes the raw codegen
  payload first and runs CRLF normalization only after the compiler exits; the
  source route, normalized payload hash, host compilation, smoke, manifest
  transaction, and fail-closed errors are unchanged. The resulting latest
  driver built in 217,994 ms at 2.938 GiB peak aggregate private, SHA-256
  `C111DAAD3B19F27CC2B087D788775D8F437BF8B3D9207E2267D01B490F5D2A9E`.
  Component inventory rejects restoration of the concurrent normalization
  pipe. The source-pressure receipt still shows definitions growing private
  memory from 2,622.4 MiB to 3,055.3 MiB across 6,727 definitions; that
  attention-level codegen lifetime remains open even though the build is green.
- The first bounded follow-up inside that definitions interval closes one exact
  repeated fact operation without opening another owner track.
  `CodegenFunctionValueBindingFactFor` already materializes the function-local
  type-environment row in `binding.env_rows`; seven `EmitLet` branches were
  independently rebuilding the same source name, type, value kind, and C name.
  They now pass the existing row to
  `CodegenTypeEnvStateAppendOwnedLocalRows`, which copies it into the local
  environment and retires the temporary backing after the copy. The old
  `EmitLet -> CodegenTypeEnvStateAppendTypedValueBinding` path is rejected by
  the existing type-environment preseal focused gate. That gate pins exactly
  seven admitted row consumptions, copy/install/retire ordering, and the
  absence of the rebuild path, then executes its ordered-delta fixture through
  installed C and native-pipeline LLVM. It passes in 7.3 seconds and is now in
  the existing serial Linux self-host CI invocation; the CI profile gate is
  green. The full structural component gate is green, and the focused
  `string_concat_op` fixture is green through both C and LLVM self-host codegen
  backends. The broader `bool_logic` fixture currently has
  an unrelated dirty-tree oracle drift (actual 10 lines versus expected 9), so
  it is recorded as RED rather than being hidden or repaired in this slice.
  A fresh Pergyra-built codegen seed completed with SHA-256
  `0A1068EB4C76F6CBCE24AEF4C631BDF3FC840CAB31356AF57F12EA1D150AC202`.
  Under the unchanged 3072 MiB/300s process-tree boundary, that seed completed
  the current-source pressure route in 113,582 ms, reached
  `definitions:done:6728` and `output:finished`, and peaked at 3,038.8 MiB
  private. The definition interval grew from 2,622.5 MiB to 3,038.8 MiB, or
  416.3 MiB, versus the prior adjacent 432.9 MiB receipt: an observed 16.6 MiB
  reduction. The interval itself took 3,878 ms versus the prior 2,999 ms, so
  this receipt supports the lifetime reduction but not a speedup claim. Peak
  private remains above the 2.4 GiB attention line and only 33.2 MiB below the
  hard cap. This is therefore a real local SoT/lifetime substitution inside an
  existing BRIDGE, not a registry promotion or closure of the broader
  definition-stage lifetime debt.
- The next bounded repeat inside that same owner is also removed, but its
  result must not be overstated. `TypeEnvAppendLocalRows` used to copy the
  retained local-row prefix three times (`Substring` suffix, `combined`, then
  a re-prefixed result) for every admitted binding. It now performs one
  newest-first `Concat(rows, local_rows)`. The local-row scan owner accepts a
  row at offset zero and owns the shared row-start fact used by both value and
  presence lookups, so the representation change does not create a second
  parser. The focused gate rejects the three retired reconstruction steps and
  proves first-row lookup, newest-first shadowing, malformed preseal rejection,
  and C/native-pipeline LLVM parity. The full component gate and the
  `string_concat_op` self-host C/LLVM parity fixture are green. A fresh isolated
  Pergyra-built gen2 has SHA-256
  `9174B6583E01191C7440C0065A375E3A71655D480AE3306DF07D9E66CF99332E`.
  Its unchanged 3072 MiB/300 s source-pressure run exited 0 in 106,705 ms,
  reached `definitions:done:6729` and `output:finished`, and peaked at
  3,063.1 MiB private. The definition marker interval was 2,738 ms, shorter
  than the adjacent receipts, but one run is not a speed claim; the 50 ms
  sampler brackets `definitions:done` between 3,002.4 and 3,040.1 MiB and does
  not prove a memory reduction. This is an exact old-path deletion and local
  hard substitution, not closure of the 3 GiB definition-lifetime blocker.
- The next step used allocation ownership evidence instead of opening another
  SoT track. A diagnostic-only allocator wrapper was linked around the prior
  Pergyra-built codegen C in `.tmp`; it emitted byte-identical 3,972,166-byte C
  with SHA-256
  `32FD6565FCBFC2E202C6AA6FB2303B0FAF93AB3A928BF64B5EBBA941FC4356EF`
  and reported zero untracked allocations or releases. On the codegen's own
  2,900-definition AST, the definition block increased direct live payload
  from 353,146,582 to 404,218,328 bytes while issuing 8,285,484 allocations.
  Function-stack attribution showed that nearly all of the 4.39 million
  one-character `CodegenCharAt` allocations in that interval came from only
  `CsvAt` and `ParamModeCsvCount`: their comma scans retained 6.90 MiB and
  1.39 MiB of one/two-byte String payload respectively, with allocator metadata
  accounting for much more process-private memory. Both functions now consume
  the existing allocation-free `CodegenCharCodeAt(..., length, index) == 44`
  delimiter fact. No new owner, cache, representation, builtin, or user syntax
  was added, and the old loop-local String delimiter path is negative-gated.
- The focused type-environment gate now executes first/middle/last/out-of-range
  CSV selection plus empty/three-row mode counting in installed C and native-
  pipeline LLVM; it is green. The complete component inventory and focused
  `string_concat_op` self-host C/LLVM parity are also green. A fresh isolated
  Pergyra-built gen2 has SHA-256
  `F4E1452EB634725C040961C832B667C89E5C26A19A0BAAD6D20F89458B4FF73A`.
  That executable regenerated its own raw C byte-identically: gen2/gen3 are
  both 3,972,162 bytes with SHA-256
  `AED3A59592F6D20662D9BD2C0805E3F3EE5BB2B2206E2A28A48BC363B1C85847`.
  On the exact 6,729-definition driver source route and the unchanged
  3072 MiB/300 s/50 ms boundary, it reached all 172 stages and
  `output:finished` in 116,577 ms at 2,890.8 MiB peak private. The prior
  adjacent receipt was 106,705 ms/3,063.1 MiB. Definition-start private is
  effectively unchanged (2,622.4 versus 2,622.9 MiB), while the sample after
  `definitions:done:6729` moved from 3,040.1 to 2,867.6 MiB and final peak fell
  by 172.3 MiB. This proves the lifetime reduction in the reached definition
  interval but not a speedup; elapsed time regressed by 9,872 ms and may include
  host variance before definitions. Peak memory remains above the 2.4 GiB
  attention line, so broader compiler lifetime remains open and the registry
  census and 78%/83% baselines do not move.
- The next allocation-owned repeat was closed without opening another SoT row.
  `SemanticCallSpineViewFromGraph` already owns the source-order projection of
  parser call-spine argument and generic-actual rows, but it used to build each
  row in reverse and then copy it into a second ordered Array. It now reverses
  those same two backings in place and returns them directly. The focused gate
  rejects restoration of the second `arguments`/`actuals` reconstruction.
  Clean, callable-resolution, target-mismatch, nested-mismatch, and explicit-
  mismatch modes all match their exact expected text and exit status through
  current native C and LLVM; the installed self-host C route is also green.
  The complete component inventory is green. The installed self-host LLVM leg
  remains RED at the independent direct-MIR projector boundary
  `SemanticAstGenericParameterDefaultRowsFromNode` parameter 2,
  `Array<String>` with `value-result`; that failure is not hidden or counted as
  call-view parity.
- The current isolated Pergyra-built gen2 is 2,490,207 bytes with SHA-256
  `4C9D3E31C22AAFF9ED48CF0E548255BEE9FE5FEDF3BF59D98DB748DE377DEF3D`.
  It regenerated its own normalized C with the same gen2/gen3 SHA-256
  `6C92CC343AE3E80BD77444C1F82CF35C1CA2CDFB71C38829B08801D396E9B2A5`.
  On the exact 6,729-definition driver source and unchanged
  3072 MiB/300 s/50 ms boundary, all 172 stages reached `output:finished` in
  118,113 ms at 2,823.5 MiB peak private. Against the immediately preceding
  CSV-charcode receipt, peak fell 67.3 MiB. The first sample after
  `definitions:start`/`definitions:done` moved from 2,640.2 -> 2,867.6 MiB to
  2,602.2 -> 2,798.0 MiB, so definition growth fell from 227.4 to 195.8 MiB,
  or 31.6 MiB. The marker interval was 2,614 ms versus 2,637 ms, while total
  elapsed time was 1,536 ms longer; neither difference is a speedup claim.
  Peak remains above the 2.4 GiB attention line. This is a measured lifetime
  substitution inside the existing BRIDGE, not a registry promotion, full
  bootstrap installation, or remote-CI result; 49/86, 78%, and 83% stay fixed.
- The next two changes followed the first failing production route instead of
  opening another SoT inventory. The callable parameter policy and unique role
  plan now admit by-value `Array<Bool>` through the existing ArrayBool storage
  receipt, then admit by-value `Option<String>` through the existing
  OptionString layout receipt. Routine admission passes complete parameter ABI
  facts to that Option owner; it does not reconstruct JSON, infer a layout, or
  add a second signature family. The focused ArrayInt/ArrayBool,
  ArrayString, and OptionString gates all pass current C and LLVM execution plus
  ABI-layout, carriage, pass-shape, and no-mutref negatives. The component
  structural gate and CI-profile gate are green, and the existing serial
  `self-host-direct-mir-scalar-graph-plan-test-smoke` dependency already owns
  all three gates. No new workflow job, timeout, memory cap, cache, GraphPlan,
  or registry row was added.
- The same generic-return production route now advances through the former
  routine 985 composable logical-record return, routine 1159 by-value
  `Option<Int>`, and routine 1173 composable owned `Array<String>` return.
  These are not three new SoT rows. The unique callable parameter-role plan now
  composes existing logical-record, OptionInt, and ArrayString ABI receipts,
  and the ArrayString ABI last consumer cross-seals the same composable
  signature fact instead of retaining its older scalar-only return test. The
  focused composable logical-record/ArrayString, owned ArrayString, and
  OptionInt gates pass C and LLVM execution plus layout, carriage, copyout, and
  return-identity negatives. The complete component inventory and CI-profile
  gate are green.
- The preceding isolated Pergyra-built driver was 5,840,602 bytes with SHA-256
  `30A8EECB2D45B783BE8F356623CB8125942A2266A284E8B51A2A445CA68689E0`.
  With that driver, the whole generic-return route first failed at routine 1228
  `ParserImportGraphSeen` on a by-value `Set<String>` parameter. That executable
  boundary is now replaced. The central ABI row owns the `Set<String>` carrier,
  the callable policy and role plan consume it, and the C/LLVM expression
  owners consume the existing Set runtime symbols for `SetNew`, `SetAdd`, and
  `SetHas`. The focused current-driver gate passes both backends and rejects
  carriage, pass-shape, type, ABI-required, and call-target mutations without
  publishing artifacts. The current isolated Pergyra-built driver is 5,850,285
  bytes with SHA-256
  `607034BDA8E5BB9FF5CE7A4DFDBF523FAA11B77D5AB718335FF05C7524AF745E`.
  `bin/pgy.exe` remained the fixed source compiler and nothing was installed
  into `bin/`. Re-running the whole route advances the first failure to routine
  1250: `owner=callable-route-envelope stage=parameter-type-or-carriage
  name=ParseDestructureLetStmt parameter=4
  type=Array<AstExpressionGraphRows> carriage=value-result`. This is executable
  substitution evidence, not a new SoT row or lifecycle closure for general
  Set aliases. The next active objective is the admitted
  `Array<AstExpressionGraphRows>` value-result ABI boundary; unrelated
  SoT/GraphPlan/V work remains frozen. Registry and project percentages remain
  exactly `49/86 CLOSED`, 78%, and 83%; canonical installation and remote CI
  were not performed.
- A broader direct-definition sink was rejected before implementation. The
  apparent seam would pass `CodegenProgramFunctionDefinitionBlock.output` into
  lower emitters, but the language deliberately forbids `TextBuilder`
  parameters until a copy/borrow/transfer boundary is proved. No ownership
  exception, new builtin, or monolithic emitter was added merely to make this
  performance hypothesis compile.
- A narrower seven-branch local-declaration materializer was also executed and
  reverted. It moved the repeated `type name = value;` framing behind the
  existing binding owner and passed focused C/LLVM plus the complete component
  gate. Its fresh gen2 SHA-256 was
  `9E6732E8E453A1C49E6AC73CD0FC2EFB3496B049611834F6AA5373B152FAA1AD`,
  but the same pressure run peaked at 3,061.0 MiB versus the adjacent
  3,063.1 MiB and did not improve elapsed time. The 2.1 MiB peak difference is
  inside run/sampling variation, so the new function, calls, gate terms, and
  owner prose were removed. This experiment is evidence against another small
  framing abstraction as the active 3 GiB fix; it is not progress credit.
- One adjacent statement-line allocation hypothesis was falsified and reverted
  rather than retained as apparent progress. A 500 ms sample initially made a
  single-`TextBuilder` replacement for the three-fragment
  `CodegenPrefixOwnedStatementLine` shape look 63.8 MiB better. Repeating the
  same seed at a 50 ms requested interval placed the definition boundary at
  2,622.7 MiB -> 3,035.5 MiB, or about 412.8 MiB: only 3.5 MiB below the
  preceding 416.3 MiB receipt and within run/sampling variation. The support
  tail then added about 22 MiB. The experiment preserved C/LLVM behavior but
  did not close the reached cost, so its source and structural-gate edits were
  removed. Definition-internal lifetime remains the active measured blocker;
  final payload assembly has not been promoted to a new owner track.
- In an isolated adjacent-launcher sandbox, that exact latest driver passes the
  complete installed C and LLVM public routes through Option,
  inferred/constructed generic members, passive nominal,
  subject/vessel/ability, and runtime fixtures. Enum value-match C/LLVM, five
  metamorphic cases, 34 negatives, and payload-free enum C/LLVM negatives also
  pass. Nothing was copied into `bin/`.
- This production result does not change the top-level
  `49 CLOSED / 36 BRIDGE / 1 ACTIVE` census or the 78%/83% progress baselines:
  the wider owner family is still BRIDGE and the 818-path aggregate remains
  unintegrated. Installed-driver verification is now locally green for the
  authoritative provenance candidate, but deliberate diff integration and
  remote CI on one reviewed revision remain open. The serialized build restored
  roughly 64 MiB of aggregate headroom versus the hard cap, but the compiler
  itself still peaks above 3.05 GiB during definition emission. Memory
  headroom is therefore an explicit attention debt. Do not start another SoT,
  GraphPlan, V, cache, shard, timeout, or cap track in place of that integration
  boundary.

## Historical checkpoint archive - inactive below this boundary

Everything below this heading is retained for evidence lookup only. Do not
resume an older runtime-value, MIR-consumer, GraphPlan, codegen-lifetime, or V
frontier from this archive unless the active card above names it as the exact
blocker again.

- The latest active slice is now a production-scale hard substitution, not an
  owner-count claim. Final MIR instruction validation previously called
  `CompilerRuntimeValueCallAbiFactForId` for each runtime-value instruction.
  That materializer rebuilt six candidate facts and repeatedly serialized and
  scanned the 256-row runtime-call ABI registry. The validator now consumes the
  allocation-free `CompilerRuntimeValueCallAbiIdentityForId` receipt; the
  materializer remains only in codegen consumers that need a full fact. The old
  validator read is rejected by the component gate, and stable row-ID hashing
  no longer constructs `domain|type|operation` with `Concat`.
- The current-source evidence is complete for this seam. Runtime-value
  lifecycle C/LLVM negatives and runtime-call ABI manifest parity are green. A
  fresh Pergyra-built source emission completed in 121,919 ms under the
  unchanged 3072 MiB cap and produced 9,857,088-byte C, SHA-256
  `FB67F2C543C5704BC9B9D9341D5DD798B077B1204430617B427F3931D58099C9`.
  Canonical `-O3 -fwrapv -fno-strict-aliasing` host compilation completed in
  105,823 ms and produced executable SHA-256
  `CEAD268EAE63E00A85EE6E4D516A77A594F8C53C753E2B29E99FD8E268177C81`.
  That executable completed full source-to-verified-MIR publication in 116,270
  ms at 2.812 GiB peak private, below the same hard cap. Validation passed row
  90,112, local refs, instruction ABI, blocks, routines, and `mir-facts:done`,
  then reached `json-write:done`. The 228,492,268-byte MIR artifact has SHA-256
  `F338B0E4F8EDFBAF490E4994726725A32A8E34F6F80160041A98001B79BA773E`.
  The previous current-source run stopped near instruction row 73,728 at 3.203
  GiB, so this is executable removal of repeated SoT reconstruction. It does
  not promote a top-level registry row or make the 786-path aggregate coherent;
  `49 CLOSED / 36 BRIDGE / 1 ACTIVE`, 78%, and 83% remain unchanged.
- The active executable rung now moves to the current MIR consumer. The same
  executable consumed the new 228,492,268-byte MIR through the production
  `--mir-json --observe-mir-consumer-stages` path under 3072 MiB/300 s. It
  remained memory-bounded at 0.616 GiB peak private but timed out after 317,202
  ms at top-level routine 6,464 of 6,704, before output publication. The largest
  completed 64-routine interval was 3,008->3,072 at 12,605 ms; its current MIR
  payload is 4,646,507 bytes, 840 blocks, and 2,011 instructions. Its largest
  rows include `SelfMirIntentRoutineBuild`,
  `SelfMirRoutineBuildStorageRetireAfterLastConsumer`, `SelfMirAppendRoutine`,
  and `SelfMirAppendCfg`. Across 101 completed batches, elapsed time correlates
  more with instruction count (`r=0.5833`) and raw routine bytes (`r=0.5342`)
  than with the CFG-quadratic proxies sum(blocks*conditionals) (`r=0.1605`) or
  sum(blocks^2) (`r=0.1818`). Therefore the next change must first distinguish
  routine fact-index time from validation/header/region rendering at the
  reached 3,056 routine; broad CFG optimization, cache/FactStore work, timeout
  increase, or another registry-owner expansion is not yet justified. One
  bounded 240-second focus attempt reached only routine 2,880 because the host
  was slower than the baseline and therefore did not produce the required
  3,056 substage receipt. The temporary ordinal focus was removed from source
  and its gate; this failed observation is not progress evidence.
- The latest typed-source candidate was emitted by the Pergyra-built codegen
  through the canonical compiler-build owner and host-compiled with canonical
  `-O3 -fwrapv -fno-strict-aliasing` flags. Its 9,695,682-byte C artifact has
  SHA-256
  `2AC69466D8F9C80570A7B412964E4F930CFCBF6BA8AF8A5EBE41D5BA1109876B`;
  the 5,804,704-byte executable has SHA-256
  `386DDC7FE6F05E57915DA87A7D0E620D0C3153AD811C7C10C38622F0CA50F2C5`.
  It remains isolated under `.tmp` and is not installed into `bin`, but this is
  no longer a source-emission-only receipt: the canonical build completed its
  host compile, bounded source smoke, and machine-manifest replay. Re-running
  the build against the final source graph printed `reusing fingerprinted
  Pergyra-built driver`, proving the candidate key is current.
- The matching codegen fixed point is byte-equal gen6==gen7 C, 3,965,061 bytes,
  SHA-256
  `86FC064C8B9E6E9AB78104154D671BB3F7F3A965134924AFADA9F97F0F95CF28`.
  Its canonical host executable is 2,482,802 bytes, SHA-256
  `5C9E243A835DAC167B59949CB4C4B1C0AFFB264830189F9EB5FA624D39DCE366`.
  Adding the final direct `text_owner` dependency does not change that C hash.
- Under the unchanged 3072MiB/300s observation boundary, the final codegen run
  exits 0 at 127,589ms, reaches `definitions:done:6719`,
  `support-blocks:done`, and `output:finished`, and peaks at 3051.6MiB aggregate
  private / 3050.4MiB in the codegen process. The canonical compiler build then
  exits 0 at 258,283ms under the unchanged 3072MiB/600s boundary, with
  3041.1MiB aggregate peak private and 3020.5MiB in the codegen process. The
  previous same-owner canonical attempt was killed at 3091.9MiB before C
  publication. The measured delta is real, but the 2.4GiB attention threshold
  still fires and performance headroom remains open.
- The active bootstrap blocker moved through a real executable boundary on
  2026-08-16. `expression_c_text_materialization_owner.pgy` now owns one
  recursive C-expression lifetime epoch; binary/unary, member access, direct
  and member call results, receiver fragments, and TextBuilder-produced call
  argument text are retired only after their parent/root last consumer. The
  admitted semantic graph remains borrowed and is never freed by this owner.
  Three representative fixtures (`array_scalar_aggregate_core`, `bool_logic`,
  `string_concat_op`) produce byte-identical C before/after the call-lifetime
  extension. A current Pergyra-built codegen carrier then completed
  `--observe-source-pressure src/self_hosted/compiler/driver_bootstrap_main.pgy`
  with exit 0 in 128,609ms under the unchanged 3072MiB/300s boundary. It reached
  `definitions:done:6718`, `support-blocks:done`, and `output:finished`; peak
  private was 3066.5MiB, only 5.5MiB below the hard cap, so attention/performance
  debt remains open.
- The completed output is a 9,840,366-byte C artifact, SHA-256
  `257CAFE0C9A87D60F03A33C932B04C3F6E35AE9ABC719CCDD10C604CA9E9B3D6`.
  Canonical host flags (`-O3 -fwrapv -fno-strict-aliasing`) compiled it to a
  5,802,628-byte executable, SHA-256
  `6181B9F28F9EBFF6B408D3A3DD3B4B00D5AC757BA8B3525AD9D50BD4E38FB598`.
  The candidate and the installed DRV-2 both execute `--tokens` over
  `src/self_hosted/codegen/fixture/hello.pgy` with exit 0 and byte-identical
  SHA-256
  `A59B414C2FC153AEA8F008913E3BBE7736FF29C27AB3C744289945DC7B1A29DD`.
  The new Pergyra-built codegen then regenerated its own current AST with exact
  gen3==gen4 C equality, SHA-256
  `71C63F0415648B599FB5D35AAAF2D95E24787E9BD2AD54DC01BD23E3B4A7FEF3`.
  This is a current codegen fixed point plus current-driver source emission and
  host-compilation receipt, not yet an installed/full-driver fixed point,
  remote CI green, or a top-level SoT registry promotion. The registry remains
  `49 CLOSED / 36 BRIDGE / 1 ACTIVE`.
- Do not reuse `.tmp/int-long-scalar-abi/driver-expression-epoch.ast.raw` as a
  current semantic carrier. It was produced before the final
  `CodegenCExpressionTextCommitRoot` correction and still contains
  `let root = ArrayPop(fragments)`. A second AST produced by the older external
  parser also loses the current compiler-internal module-provenance contract.
  Current-source bootstrap evidence must use the typed `--source`/
  `--observe-source-pressure` boundary, or an AST producer whose revision and
  provenance schema are pinned and revalidated.
- The fixed producer is unchanged at
  `.tmp/multi-routine-generalization/runtime-value-current-producer/mir-lower-current.mir.json`:
  41,051,560 bytes, 1,660 routines, SHA-256
  `1F83A18848BC0A31E97F7825E430FC945933A1B40FB07C092B7CCF7A80DFF937`.
  `.tmp` remains reproducible measurement evidence, not semantic authority or
  a progress unit.
- The nested `Array<String>` expression and populated `Array<String>` C/LLVM
  backing paths now consume admitted GraphPlan identity and allocate growable
  storage instead of publishing stack-backed array storage. The fixed
  Pergyra-built consumer canary
  `runtime-value-current-consumer-growable-array-string-20260816-v36` exited 0
  in 118,815ms under the unchanged 3072MiB/300s boundary. Peak private was
  2.511GiB and peak working set was 2.438GiB; the limit was not exceeded, but
  the 2.4GiB attention threshold fired, so performance headroom is still open.
  The last receipt was `[driver-pressure-stage] direct-mir:projection:done`.
- That canary produced a 2,827,611-byte C artifact, SHA-256
  `619EF5741F021269826C8729BA8E525511D8E63CA3CBD0398DC318ED7598ED80`.
  Host compilation with `-Werror=free-nonheap-object` succeeded and produced a
  523,305-byte executable, SHA-256
  `42A1276EA4C022F9EBA31E1EC80AE2965E80608C47156D5418BEF7412309C30C`.
  That executable's `--verify-input` run over the fixed MIR exited 0 in
  1,746ms at 0.049GiB peak private and printed
  `pgy.mir.v1 input verified`. This is the first complete current carrier
  chain in this rung: Pergyra-built consumer -> C -> host executable -> fixed
  MIR runtime verification.
- The current installed DRV-2 is `bin/pgy-self-driver.exe`, 5,766,328 bytes,
  mtime 2026-08-16 18:25:42 +09:00, SHA-256
  `76B05F94576EC9EA2F4F61E5FE6CFC380F89559AA34B73FA180C819A14FFDC37`.
  Its 9,679,763-byte typed-source C input has SHA-256
  `70DD47906ABECE7A31196E42B09745EB465301BC89FBB0850B4E3AFB4D96B421`.
- The active typed-intent delta advances the wire to
  `pgy.selfhost.mir-intent-execution-plan.v3`. Each step now carries the exact
  `where_zone_name` plus declaration `where_zone_syntax_id`; native production,
  native/self admission, the mutation digest, and C/LLVM last consumers all
  cross-seal that identity. The focused stage-0 self C plus native C/LLVM gate
  `intent_typed_outcome_compensation_owner.sh` exits 0 and observes both steps
  in `WorkflowZone`. This is real `REACHABLE` semantic progress, not a new SoT
  row or a `CLOSED` promotion.
- The version-3 replacement is not installed. A current native-stage codegen
  completed typed-source emission in 235,035ms at 2.745GiB peak private. The
  canonical Pergyra-built gen2 seed is
  `.tmp/self_hosted/codegen/bootstrap/gen2.exe`, 2,455,198 bytes, SHA-256
  `2D319922250001D90BFD57FA2FAA11E5C0AECB353A895BBCCDCED99D57BC8778`.
  Under the same 3072MiB boundary it reached
  `[codegen-pressure-stage] definition:done:2432` and was stopped at 3.035GiB
  peak private before producing a replacement driver. Therefore the existing
  installed version-2 typed transition remains the bounded `SUBSTITUTING`
  receipt while the version-3 zone/observability extension remains
  `REACHABLE`; the registry stays `49 CLOSED / 36 BRIDGE / 1 ACTIVE`.
- CI fingerprinting had a separate Windows process-amplification defect:
  parser-tool source-set hashing launched `sha256sum` once per 2,011 source
  files. `parser_tool_build_leg.sh` now owns a sorted NUL-delimited batch and
  schema `pgy.selfhost.parser-tool-build.v3-batched-native`. The fixed source
  set produces 2,011 rows deterministically in about 6.6 seconds. This closes
  the CI measurement seam only; it is not compiler-semantic substitution.
- The v3 static protocol gate, Bash syntax, touched-slice diff check, and the
  prebuilt-driver typed compensation execution gate are green. The parser
  fingerprint was remeasured at 2,011/2,011 rows with SHA-256
  `a19ac0bcd7fb66007a99094b80cc0da5c3c68bee0bdf983a42b5e9cf899e98d2`
  in 4,374ms. The full component inventory passes the new v3/Make/CI checks and
  then remains RED at the unrelated accumulated GraphPlan boundary:
  `src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy` calls but
  does not declare `RewriteSemanticStructCall`. Do not patch that owner as part
  of the intent/CI slice or report the full component gate green.
- The installed compiler build had retained the provenance-free
  parser-to-AST-text route even after the bounded driver bootstrap moved to
  typed `--source`. It now emits from the typed source artifact directly and
  keys installation by the resulting C artifact hash. Parser/AST-text variables
  are negative-gated. This was a real remaining bypass, not a new semantic
  owner.
- The canonical 51-row intent-observability ABI now reaches installed self-host
  C and LLVM execution. Codegen derives one usage receipt and runtime symbol from
  the generated complete row and emits the enabled runtime header only when
  needed. The public installed routes and native oracles execute
  `IntentHistoryCount()` (zero arguments/Int), `IntentActiveConcurrent(0)` (one
  Int argument/Bool), and `IntentActiveStepName(0, 0)` (two Int arguments/String)
  as exact stdout `0`, `false`, and an empty line without native re-entry. The
  focused execution, public default-C regression, hard contract, component
  inventory, CI step runner/profile, generated registry, and 51-row negative
  gate are green. Native 7-field and self-host complete 10-field MIR nodes now
  carry canonical `RuntimeCallAbiId`; direct admission cross-seals it against
  the registry row and both backends consume `RowForId`. Native/self MIR x
  direct C/LLVM all execute the 0/1/2-argument sample exactly. Missing,
  mismatched, forged non-observability, and mixed syntax/runtime identities fail
  before publication. The installed default-priority legacy intent emitter now
  projects enter/step/bind/materialize/fail/ok/exit events from admitted facts.
  Its success, guard/expect/post failures, reverse compensation, two history
  rows, exact `post:ForwardB`, and zero active intents after exit match native
  C/LLVM. Typed-plan observability, non-default priority, and the compiler-purpose
  root intent remain open, so the registry row stays BRIDGE and project
  percentages do not move.
- The focused installed intent-observability gate owns four independent
  execution legs: installed C, native C, installed LLVM, and native LLVM, each
  pinned to exact stdout `0`, `false`, and an empty line. All four pass with the current installed DRV-2,
  and both public installed routes reject native pipeline re-entry. CI/Make use the backend-neutral
  `self-host-intent-observability-runtime-test-smoke` target and reject the
  retired C-only target spelling.
- The carried-ID execution owner is
  `self-host-intent-observability-mir-identity-test-smoke`; it reuses the same
  serial installed-driver build in CI and does not add a second bootstrap.
  Current installed DRV-2 is 5,766,328 bytes, SHA-256
  `76B05F94576EC9EA2F4F61E5FE6CFC380F89559AA34B73FA180C819A14FFDC37`.
- The replacement DRV-2 typed-source emission now completes inside the unchanged
  3072MiB/300s observation boundary. The measured wrapper exited 124 at 300,273ms
  after C emission had completed, with 2.926GiB peak private and 2.564GiB peak
  working set; the subsequent host compile was a separate roughly 130-second
  phase. Do not relabel that monolithic wrapper run exit 0: the emitted C,
  compiled candidate, source smoke, manifest hash, and four execution legs were
  verified separately before local installation. A
  typed-source `--check-source` control completed at 1.12GiB, excluding source
  parsing, semantic admission, and shape checking as that rebuild's immediate
  blocker. The opt-in `--observe-source-pressure` route preserves typed module
  provenance and never uses the rejected AST-text detour.
- Definition-stage observation found a real function-local type-row lifetime
  defect: every local binding replaced the serialized local environment while
  retaining the old owned row string. The first retirement attempt freed that
  row before the initializer expression's borrowed environment finished and
  failed after definition 256 with `semantic leaf binding fact is missing: c`.
  `type_env_state_lifetime_owner.pgy` now gives each statement a borrowed child
  state and adopts it only after expression emission returns. Native source
  compilation is green at 1.491GiB, and ten focused local/scope/expression fixtures
  (`int_arith`, `nested_ctrl`, `else_if_chain`, `for_sum`, `enum_match`,
  `option_try`, `defer_scope`, `struct_point`, `struct_mixed_fields`, and
  `struct_nested_fields`) compile and run with exact expected stdout.
- The recursive expression/call materialization blocker is now closed behind
  `expression_c_text_materialization_owner.pgy` and
  `expr_semantic_struct_call_emit_owner.pgy`. Binary/unary roots use one
  explicit owned-fragment epoch; member/direct call paths join that same epoch,
  and call argument assembly uses one TextBuilder result instead of repeatedly
  replacing a Concat accumulator. Struct construction keeps its own bounded
  TextBuilder last consumer. The ABI target-policy readiness hot path also
  consumes one structured row instead of rebuilding its CSV row. This is the
  change that let current-source C emission reach `output:finished` below the
  unchanged 3GiB cap; no cache, shard, timeout increase, or AST-text fallback
  was added.
- The scalar builtin runtime-call identity duplication is now closed at its
  existing signature owner. `DirectMirScalarProgramBuiltinSignatureFact` owns
  the canonical runtime-call ABI ID together with arity/type/expression kind;
  call admission consumes the MIR-carried ID and only cross-seals it against
  that fact. The call owner no longer re-queries either intent-observability or
  runtime-value registries by source name. It is 115/115 lines and the signature
  owner is 205/205 without raising either cap. Current-source `--check-source`
  is green, and a new Pergyra-built driver C artifact completed under the same
  3072MiB/300s boundary in 124,961ms at 3035.2MiB peak private. The C artifact
  is 9,841,206 bytes, SHA-256
  `EFE924855388CDB5554FC24358A0CA6B1A7A06004A02EB32E3C73D2EC13AB629`;
  canonical host flags produced a 5,803,148-byte candidate, SHA-256
  `6E0FD990A67D6958B787AE5A0829DE5D268FBB779962178E301FA0D685FB04E6`.
  That candidate passes native/self MIR identity, direct C/LLVM execution, and
  missing/mismatched/forged ABI-ID negatives. The installed four-leg runtime
  gate also remains green.
- The next component RED was a real incomplete literal-Log migration, not a
  reason to raise its 560-line family cap. The old declaration route contained
  a 27-line semantic kind scan but had no production caller, while malformed
  declaration-bearing literal programs could fall through to the scalar route.
  It is replaced by one shape-only claim; semantic kind/name validation remains
  solely in the erasure fact and the plan owns final admission. The family is
  exactly 560/560. The same pass closed stale Int=64 assumptions: C emits the
  exact-width header and `%d` for `int32_t`, while LLVM supplies `i32` rather
  than `i64`. The focused C/LLVM gate passes ability exact 7, zero-declaration,
  coherent rename and display-text equality, literal 73, plus 25 malformed MIR
  negatives. The full component inventory now exits 0.
- The latest current-source build containing both closures completed under the
  unchanged 3072MiB/300s boundary in 142,920ms at 3069.8MiB peak private
  (2.998GiB), so functional closure is green but bootstrap headroom is critical.
  Its 9,840,751-byte C artifact has SHA-256
  `E507A52B70F0327EECA5BFC684A97D51F9B576E3146509D37A1E82B3161577FA`;
  canonical host compilation produced a 5,802,118-byte candidate with SHA-256
  `E833ACE13B5B3E419B02EA22EFA897193F861E10E9AD5B3C8847AF9947521F3D`.
  The candidate repeats the native/self MIR plus direct C/LLVM ABI identity gate
  and its token output is byte-identical to installed DRV-2, SHA-256
  `A59B414C2FC153AEA8F008913E3BBE7736FF29C27AB3C744289945DC7B1A29DD`.
  The former String-builtin GraphPlan blocker is closed rather than bypassed.
  Installed, current Pergyra-built, and native MIR producers agree after the
  established CRLF normalization on 16,434 bytes and SHA-256
  `C39CF0215F9ACA7CA5841D027966786C418967831A66ADE527FD05B9A04E03CA`.
  The fixture now observes the required `ToString` result (`foo`), the scalar C
  preamble owns `<stdint.h>`, and the current candidate passes the positive plus
  eight malformed-MIR C/LLVM cases. The rejected AST-text attempt is useful
  fail-closed evidence: it lacks current module provenance and correctly emits
  `compiler_internal_builtin`; current bootstrap must continue through typed
  `--source`, not restore a provenance fallback.
- DIR still has one graph identity authority, but its row carriers, lookup,
  append, and declaration naming were moved into
  `src/self_hosted/dir/domain_graph_row_owner.pgy`; orchestration remains in
  `domain_graph_inventory_owner.pgy`. The focused exact-row C/LLVM parity and
  malformed endpoint/provenance negatives pass. This removes the active
  implementation-size pressure without creating independent DIR graphs; it
  does not yet replace name joins with stable typed IDs.
- The current GraphPlan aggregate completed from scratch with exit 0 in 816
  seconds. Its reached exact gates include owned logical-record returns,
  readonly/value-result record combinations, collection copyouts, ArrayBool
  return materialization, control transfer, StringIndexOf, host I/O, Long
  arithmetic, and populated array literals in both C and LLVM. The overlap
  between owner-handle record return and a same-record value-result formal now
  fails closed at the callable-signature owner; legal value-plus-copyout shapes
  remain positive fixtures rather than stale negative samples. Control-transfer
  and StringIndexOf host compilation now consume the common emitted-C runtime
  header contract, so clean CI workspaces receive the required runtime include
  paths without weakening emitted-code checks.
- Local verification after the CI repairs is green for the current affected
  slice: build-source inventory, full component structural inventory, size
  inventory, likeness (`core_string_munge=76/76`, `sentinel=24/24`,
  `result_use=4101/4101`), and the current-candidate String plus Long
  division/remainder C/LLVM gates. The earlier MIR `161/161`,
  DIR/destructure/speculation follow-ons, full GraphPlan aggregate, JSON writer
  lifetime, CI step runner/profile, machine-layer manifest, and affected
  Array/record/DIR receipts remain prior evidence; they were not all rerun after
  this final candidate. `git diff --check` reports only the pre-existing CRLF next-touch
  warning for
  `program_routine_index_owner.pgy`. The MIR suite emits two existing generated
  C unused-variable warnings but completes with 0 errors.
- The latest GitHub Actions run is still RED because it is run
  `31778017449` on this unchanged committed HEAD, before the local worktree
  repairs. Its failed bootstrap/parity/platform jobs must not be reported as
  proof that the current worktree is green. Local focused fixes cover named
  observed causes: unsafe Bash
  alternation in the JSON lifetime gate, wrong Makefile object-owner
  assertions, a self-host `Die` dependency, stale LoC/likeness/include
  inventories, and duplicate phony driver builds in exhaustive parity. No new
  remote run exists because nothing was staged, committed, or pushed.
- Real-source self-application passes over all 1,522 current `src/self_hosted`
  sources with freshly compiled C and LLVM semantic checkers: 1,522/1,522 per
  backend, 3,044 checks total. The previous complete run accepted 1,520 sources
  per backend. The seven files renamed after that earlier run to remove native
  reserved-binding collisions were then accepted directly by both fresh
  checkers. This run closed two missing owner imports and reconciled
  the semantic diagnostic vocabulary with the actual emitters: the registered
  count is 33, while the literal-callsite audit observes 31 codes; the two
  remaining registered codes are non-literal guard diagnostics. The
  diagnostic surface audit is green.
- The current progress metric is 65,962 implementation/frontend/backend LOC,
  197,798 compiler-core LOC, 19.82%, default C emit `substituting`, full default
  compile `open`, and explicit DRV-2 `live`. These volume figures are state
  evidence, not completion units.
- `CompilerRetireArrayStorage` caller admission no longer has separate artifact
  and legacy-body policies. Both consume
  `SemanticCompilerRetireArrayStorageCallerContractReady` from the collection
  mutation policy owner. The focused lifetime gate passes, an ordinary external
  call and an exact-name/signature wrong-path impersonation now fail with
  `compiler_internal_builtin` in both C and LLVM. The complete allowed tuple is
  owned once by
  `src/common/compiler_internal_builtin_caller_registry.def`; native admission
  includes that registry and self-host admission consumes its generated row
  projection. Parser-owned top-level declaration module paths are carried by
  `AstSourceModuleFacts` into aligned semantic signature rows, so an
  import-composed artifact no longer guesses caller provenance from a name or
  the root input path. The production driver bootstrap now asks codegen for the
  typed `--source` artifact directly; its parser-executable plus
  `driver_bootstrap.ast.txt` detour is deleted and negative-gated. AST-text
  artifacts intentionally carry unknown module provenance and therefore fail
  closed if they attempt the compiler-internal builtin. The focused
  legacy/artifact provenance gate, lifetime
  gate, compiler-world contract, component structural inventory, build-source
  inventory, generated-registry check, and CI-profile gate pass. CI now owns the
  focused provenance gate as a serial self-host parity step. This closes the
  bounded internal-builtin provenance BRIDGE, but does not promote the wider
  `selfhost.semantic_artifact_admission` family while its intent-observability,
  zone, and composite-intent obligations remain open.
- The public default-runtime LLVM boundary now consumes one canonical
  LLVM-callable runtime object from `compiler_runtime_cache.c`. The previous
  first attempt was correctly rejected: the C-extern object lacked
  `pgy_file_exists`/`pgy_read_file`. The final change moved the existing native
  LLVM build/cache/publish recipe behind
  `compiler_llvm_runtime_object_ensure`, deleted its reconstruction from
  `compiler_llvm.c`, and made both native LLVM and installed self-host LLVM
  consume that owner. `io_probe.pgy` compiles and runs in both paths with exact
  output `exists` / `missing` / `has-main`; the focused installed LLVM gate and
  component inventory pass. The CI workflow already executes the focused
  target. This closes the admitted default-runtime host-I/O slice, not
  intent-observability or composite-intent LLVM.
- The two named local integration reds are closed without reopening semantic
  owners. `self_host_hard_contract_smoke.sh` now follows the current
  `routine_expression_runtime_abi_owner.pgy` and pins the stronger executable
  modulo-zero and signed-add contracts instead of retired fixture spellings.
  `runtime_cache_identity_smoke.sh` explicitly selects `--native-pipeline`, the
  path that actually owns and publishes the native C-extern cache, rather than
  making the installed self-contained C artifact runner prove a cache it does
  not consume. The exact Make owner run
  `runtime-cext-contract-test-smoke runtime-cache-identity-test-smoke
  self-host-hard-contract-test-smoke` is green. CI now executes the hard
  contract in the existing single serial self-host Make invocation, and the CI
  profile and step-runner gates are green. No current-worktree remote run exists.
- Resume objective card: keep new GraphPlan/V/owner expansion frozen and turn
  the current `.tmp` canonical driver receipt into one deliberate integration
  boundary. Priority is (1) preserve the current codegen fixed point and typed
  provenance, (2) run the existing current-driver/full-fixed-point falsifier,
  (3) separate or remove unrelated accumulated dirty paths, and only then
  (4) run remote exhaustive/platform CI on an intentional revision. Fact owner
  is the semantic expression graph plus the existing codegen text lifetime
  owners; the last consumer is final C statement/root materialization and the
  canonical compiler-build artifact transaction. Forbidden fallbacks are
  AST-text provenance loss, native source codegen re-entry, cache/shard/worker
  workarounds, cap/timeout increases, a new lifetime owner, or counting the
  781-path aggregate as closure. The next falsifier is current-driver fixed
  point/output identity with the candidate above; remote CI remains Unknown
  until an intentional revision is committed and pushed.
- The production bootstrap first exposed 82 native ownership errors even though
  the self-host C/LLVM source checkers accepted all 3,044 source legs. Those 82
  errors reduced to four root shapes rather than 82 independent fixes: borrowed
  target facts crossed value projectors, aggregate mutation/return owners used
  `ref` where value transfer was intended, three `own` capture calls received an
  unnamed `UnwrapOption(...)`, and four multiline `return` statements were
  parsed as bare terminators. After fixing those owner boundaries, native
  source-to-C emission reports 0 errors; the three existing redundant-intent
  warnings remain. The first full rerun then exposed two generated-C SSA name
  collisions (`literal` String versus admission fact, and `target` projection
  versus callable row); responsibility-specific local names removed the type
  merge without weakening semantic checks.
- Final production receipts are exit 0. `driver_seed.c` is 9,651,147 bytes
  (`5513A6B93B183DE7295F6A0E94BEF4EB507D9948755475B20342EB4626C282CD`)
  and `driver_seed.exe` is 5,765,044 bytes
  (`BB33DAF1739541783F7710CC343BC6409EEFCEC41CD2C424EE5D7CBF1D8509B4`).
  `driver_oracle.c` is 29,244,684 bytes
  (`641CDEF97CDF3A2B2F16A7EF9D40D89A7C5755E8B4E5EBD3A6C9B48D33BFE9E1`)
  and `driver_oracle.exe` is 6,405,399 bytes
  (`D123E2730BA7A781E9C20F85A084C30E8B52A366BD284027E1E8555F86AD0CD2`).
  The focused caller-provenance C/LLVM gate and component structural inventory
  also pass. This closes the current production integration blocker, not the
  36 wider BRIDGE rows.
- The ownership and generated-C SSA fixes now have source-level recurrence
  ratchets in the existing component contract rather than a new gate family.
  `DirectMirScalarProgramAppendExpression` must keep the typed
  `literal_admission` identity, the C/LLVM expression owners must keep the
  `callable_target` identity distinct from the target projection fact, and the
  three typed-return owners reject a standalone bare `return` line. The
  expression-admission owner remains within its existing cap at 445/445 lines.
  The component inventory, populated `Array<Int>`, logical-record array value
  parameter, and `Option<Int>` try-let C/LLVM gates all pass. Build-source
  inventory and the local CI profile also pass; `git diff --check` still reports
  only the pre-existing `program_routine_index_owner.pgy` CRLF next-touch
  warning. The production bootstrap was not rerun after these test-only
  ratchets because its immediately preceding exit-0 receipt already exercises
  the changed source owners.
- Project percentages do not move on this evidence-only closure: integrated
  forecast remains 78% (75-81%), strict beta remains 83%, and SoT hard closure
  remains exactly `49/86 CLOSED`, `36 BRIDGE`, `1 ACTIVE` (57.0%). No top-level
  SoT family was promoted. One production bootstrap's provenance-free AST-text
  bypass was deleted, but that bounded deletion does not close the wider ACTIVE
  semantic-admission family or justify a percentage increase.

## Historical checkpoint - nested Array<String> expression closure

- Current HEAD is `330c82ca5f679d2b41d42c16c72ddb2049113101` on
  `main`. The worktree is intentionally uncommitted and contains the user's
  accumulated GraphPlan work; do not discard or stage unrelated paths.
- The fixed producer remains
  `.tmp/multi-routine-generalization/runtime-value-current-producer/mir-lower-current.mir.json`:
  41,051,560 bytes, 1,660 routines, SHA-256
  `1F83A18848BC0A31E97F7825E430FC945933A1B40FB07C092B7CCF7A80DFF937`.
  `.tmp` is reproducible evidence, not semantic authority or a progress unit.
- FileExists and ReadFile are now sealed host-I/O GraphPlan rows. Their focused
  C/LLVM gates are green, and ReadFile still passes after the current expression
  change. The fixed canaries reached FileExists at row 1,302, ReadFile at row
  8,869, and then the `Array<String>` expression at global row 18,623 in routine
  1,550 `EmitRoutineTreeWithContractAtRowWithExpressionOrder`.
- The row-18,623 source is exactly
  `[Concat(routine_indent, Concat(callable_label, Concat(routine, "\n")))]`.
  One common array seed owner and one String-specific nested owner now admit a
  single non-scalar, already-normalized String expression into the existing
  ExpressionSet. Direct formal/local/literal leaves stay with their older exact
  owners; the `local-forged-formal` regression initially exposed and then
  closed an attempted bypass of that boundary.
- `direct_mir_scalar_array_string_nested_expression_literal_owner.sh` passes
  exact C/LLVM output plus six missing/wrong use, kind, seed, root, and formal
  identity mutations in 6.6 seconds. Existing local and formal literal gates,
  Array<String> index/borrowed-result, and ReadFile are green. The scalar owner
  cap scan and local CI profile pass; the full component inventory, full
  GraphPlan aggregate, and remote CI have not run after this delta.
- GraphPlan schema is now `v77`. The current Pergyra-built DRV-2 is
  `bin/pgy-self-driver.exe`, 5,742,915 bytes, mtime 2026-08-15 16:01:08 +09:00,
  SHA-256
  `E9DA9FCA07199CA60D2ECBAE92AB50E4608625AD5B5E074F9B903BA9906AF0AD`.
- Fixed canary
  `runtime-value-current-consumer-nested-array-string-20260815-v11` produced no
  semantic diagnostic and no artifact before the unchanged 300-second budget.
  It exited 124 at 300,457ms with peak private 1.131GiB, below the 3GiB limit.
  This proves neither full completion nor a next semantic frontier: the
  direct `--mir-json-backend=c` path has no opt-in stage receipt, so crossing
  row 18,623 is still `Unknown` at integration scale even though the exact
  focused falsifier is green. Do not raise the timeout, add a shard/cache, or
  report this as full current-source publication.
- Resume objective card: identify the smallest repeated owned operation that
  keeps this exact direct consumer beyond 300 seconds, or add only the reached
  owner receipt needed to distinguish progress. Priority is semantic identity,
  one owner, old-path rejection, then bounded performance. Fact owner is the
  persisted expression graph plus the existing GraphPlan ExpressionSet; the
  last legitimate consumer is direct scalar-program routine admission. The
  forbidden fallbacks are source-text reparsing, accepting arbitrary untyped or
  multiple dynamic array elements, timeout/cap increases, sharding, caching,
  and switching to the different full `--mir-json` consumer as if it measured
  this path. The next integration falsifier remains the same fixed input under
  3072MiB/300s; a new run must carry an exact reached-owner receipt rather than
  repeat v11 blindly.
- Project percentages do not move on this focused seam: integrated forecast
  remains 78% (75-81%), strict beta remains 83%, and SoT hard closure remains
  exactly `49/86 CLOSED`, `36 BRIDGE`, `1 ACTIVE` (57.0%).

## Historical checkpoint - collection value-result frontier

- Current HEAD is `330c82ca5f679d2b41d42c16c72ddb2049113101` on
  `main`. The worktree remains intentionally uncommitted and includes the
  user's accumulated GraphPlan work. The fixed current producer is
  `.tmp/multi-routine-generalization/runtime-value-current-producer/mir-lower-current.mir.json`:
  41,051,560 bytes, 1,660 routines, SHA-256
  `1F83A18848BC0A31E97F7825E430FC945933A1B40FB07C092B7CCF7A80DFF937`.
  `.tmp` is reproducible measurement evidence, not semantic authority or a
  progress unit.
- The reached `owner-handle String` formal now consumes the exact signature
  carriage and target-neutral owned-array literal identity. C and LLVM allocate
  the owner literal backing, call the canonical owned-String drop, and reject
  carriage/type/call-target mutations. The focused owner gate, owned-return,
  lifecycle, and String-array index regressions are green.
- `DirectMirScalarProgramArrayStringValueResultSignatureReady` no longer owns
  the accidental seven-parameter/four-copyout fixture shape. It admits `Bool`
  returns with validated scalar value parameters and one-or-more exact
  `Array<String>` value-result rows, while retaining the one-copyout `Void`
  contract. The new scalar-prefix/single-copyout and old four-copyout paths run
  byte-equivalent C/LLVM behavior; the focused gate passed in 6.0 seconds. The
  fixed canary then crossed routine 1,572
  `MirDeclarationMethodContractReadStringArray`.
- The Bool collection policy likewise no longer owns the accidental 11-row
  `5 Array<Int> + 1 Array<Bool> + 2 Array<String>` positions. It now validates
  scalar values plus collection value-results and requires at least one exact
  `Array<Bool>` owner row. Direct-call identity consumes the existing
  `ArrayBoolValueResultAt` receipt. The single-copyout fixture exposed a real
  LLVM multiple-exit bug; ArrayBool copyouts now use
  `%pgy.param.N.copyout.<exit-block>` just like ArrayString. The focused complex
  plus scalar-prefix gate passed in 12.1 seconds, and ArrayString, 2+2 mixed,
  and Push/Set/Pop regressions are green.
- The final Pergyra-built DRV-2 installation is `bin/pgy-self-driver.exe`:
  5,728,025 bytes, mtime 2026-08-15 11:49:46 +09:00, SHA-256
  `E7482523B5FE67856A2E1A37AF022A038099D6E02306F0F8B0A9D50E920B6E33`.
  The final local CI profile passed in 4.4 seconds. `git diff --check` is clean
  except for the pre-existing next-touch CRLF warning on
  `program_routine_index_owner.pgy`. Documentation quality, post-selfhost
  manifest, source UTF-8, and progress metric passed in 18.6, 2.7, 28.9, and
  27.5 seconds. The metric is 65,151 implementation LOC, 195,210 compiler-core
  LOC, 19.58%, default C emit `substituting`, full default compile `open`, and
  explicit DRV-2 `live`. The full component inventory, full GraphPlan aggregate,
  remote CI, commit, push, and publication have not run after this delta.
- Fixed canary `runtime-value-current-consumer-array-string-copyout-20260815-v2`
  crossed routine 1,572 and stopped at routine 1,574. After the ArrayBool and
  LLVM exit-owner fixes,
  `runtime-value-current-consumer-array-bool-copyout-20260815-v3` crossed
  routine 1,574 and exited 1 after 60,694ms without an artifact at
  `owner=callable-route-envelope stage=parameter-type-or-carriage routine=1575
  name=MirDeclarationMethodContractReadRequires parameter=3
  type=Array<String> carriage=value-result`. Peak private memory was 82.4 MB;
  this is a signature-policy frontier, not memory pressure.
- Strict SoT hard closure remains exactly `49/86 CLOSED`, `36 BRIDGE`,
  `1 ACTIVE` (57.0%). No top-level family was promoted. Integrated forecast
  remains 78% and strict language beta remains 83%.
- Resume objective card: replace the exact eight-parameter positional policy
  for Bool `2 x Array<String> + 2 x Array<Int>` copyouts with one type/count
  owner. Priority is exact signature carriage, two distinct canonical layout
  families, scalar-value validation, direct-call LocalRef identity, then the
  existing C/LLVM copy-in/out consumers. Fact owners are
  `direct_mir_routine_signature_fact_owner.pgy`, the ArrayString and ArrayInt
  ABI facts, and
  `direct_mir_scalar_program_bool_two_array_string_two_array_int_value_result_policy_owner.pgy`;
  the last consumers are callable admission, direct-call identity, and the
  existing target emitters. Forbidden fallbacks are parameter-count/ordinal or
  routine-name branches, copied layouts, accepting arbitrary collections,
  backend MIR rereads, or weakening carriage/ABI negatives. The focused
  falsifier must keep the arrays-first `GraphAddEdge` regression and add the
  production-shaped scalar-prefix/interleaved `ReadRequires` path with early
  and final C/LLVM copyout, then reject wrong family count, carriage, layout,
  scalar carriage, and cross-family ABI identity before rerunning the fixed
  canary.

## Previous completed self-host context - exact Long comparisons

- The former Int-only comparison owner was replaced by one typed comparison
  owner. Existing Int identities 7/11/60/61/66/67 stay stable; append-only
  identities 76 and 77 represent exact Long greater and equality with explicit
  Bool results. The old owner path is deleted and negative-gated.
- C and LLVM consume the same normalized identity through their existing signed
  64-bit comparison operations. The expanded true/false and wrong-type/kind
  gate passed in 17.9 seconds, and the Int comparison/wrap regression passed in
  11.8 seconds. GraphPlan schema is v54.
- Current driver installation, component inventory, and the fixed-canary
  receipt are recorded in the active card. No top-level SoT row was promoted,
  so hard closure remains 49/86 (57.0%), integrated forecast 78%, and strict
  beta 83%.

## Previous completed self-host context - common Long PhiValue

- Exact Long joins now consume the existing common PhiValue type classifier,
  operation 29, `MirPhiPredecessorBindingFact`, and unchanged memory-local
  C/LLVM emitters. No schema column, opcode, backend path, or routine exception
  was added.
- The focused fixture derives both Long incoming values through the already
  admitted checked-remainder expression, avoiding the still-separate Long
  local-literal-definition seam. It returns `29` for the true path and `11`
  for the false path in both targets. Wrong incoming type, a duplicated
  non-dominating definition, and missing incoming cardinality fail before
  artifact publication. The gate passed in 9.4 seconds; collection PhiValue
  regression passed in 8.3 seconds.
- Current-source driver installation, component inventory, and the fixed
  canary evidence are recorded in the active card above. No top-level SoT row
  was promoted, so hard closure remains 49/86 (57.0%), integrated forecast
  78%, and strict beta 83%.

## Previous completed self-host context - Long remainder safety receipt

- Current HEAD is `270e616d8128` on `main`; the worktree remains intentionally
  uncommitted and includes the user's accumulated GraphPlan work. The fixed
  canary remains
  `.tmp/multi-routine-generalization/current-producer-canary-vr-set/routine-index-owner-current.mir.json`:
  36,183,978 bytes, 1,492 routines, SHA-256
  `D4C1FA7993B087E0517EF2383A24DA49A8E883DC9E467F110F877CF95899C3A8`.
  `.tmp` is reproducible measurement evidence, not semantic authority or a
  progress unit.
- Expression admission now returns an immutable first-failure receipt carrying
  the exact rejecting stage and source-graph node. The routine boundary is its
  sole terminal diagnostic consumer; neither side reopens MIR or adds failure
  fields to the successful GraphPlan schema. The focused C/LLVM falsifier
  distinguishes `leaf-operand node=0` from `expression-kind node=2` and passed
  in 7.1 seconds after the final build.
- The first receipt on the fixed canary was
  `stage=builtin-call node=9 row=4338 source=AST_CALL`. Node 9 was the first
  partial argument row of nested `MirRoutineInstructionView(...)`; the
  declaration-keyed logical-record owner already knew that record, but rejected
  it because its type differed from the enclosing
  `MirRoutineInstructionSelection` return type. Constructor identity no longer
  depends on that outer expected type. Root result typing remains enforced at
  the typed-expression boundary.
- The scalar multi-record fixture now contains an actual
  `LocalDocumentFact(LocalTableFact(...), ...)` expression. C and LLVM execute
  the same nested constructor/member result (`8`) and retain all existing
  declaration-identity and ABI-absence negatives. The focused gate passed in
  12.1 seconds. An attempted ArrayBool fixture extension was withdrawn because
  its LLVM artifact exposed a separate unresolved
  `pgy_runtime_panic_out_of_bounds_export` link dependency; it is not counted as
  evidence for this seam.
- The final current-source Pergyra-built driver installed in 240.5 seconds.
  `bin/pgy-self-driver.exe` is 5,537,253 bytes, mtime
  2026-08-14 07:15:51 +09:00, SHA-256
  `46EB4248737E2C71B5B3987D22D60059E9C258FF458936601C15F23D50313188`.
  Component structural inventory passed in 343.5 seconds and the local CI
  profile passed in 6.1 seconds. Remote CI, the full GraphPlan aggregate,
  commit, push, and publication have not run.
- The post-generalization canary crossed row 4338 and exited 1 after 210,828ms
  without an artifact at `stage=expression-kind node=2 row=4360
  source=AST_LET_DECL`. Row 4360 maps exactly to raw routine row 258,
  `MirAbiLayoutMulMod`, source syntax ID 7413, block 0 instruction 0:
  `let left: Long = a % modulus`. Nodes 0 and 1 are formal parameters `a` and
  `modulus`; node 2 is the Long remainder expression.
- Strict SoT hard closure remains exactly `49/86 CLOSED`, `36 BRIDGE`,
  `1 ACTIVE` (57.0%). No top-level family was promoted. Integrated forecast
  remains 78% and strict language beta remains 83%.
- Resume objective card: define a target-neutral safety fact for Long remainder
  before adding a new expression identity. Priority is divisor nonzero/minus-one
  evidence, Pergyra overflow/trap semantics, then identical C/LLVM lowering.
  The current fact owner is expression-kind/readiness admission; the last
  consumers would be the existing scalar C/LLVM expression emitters. Forbidden
  fallbacks are treating dynamic Long remainder as safe Int remainder, a
  `MirAbiLayoutMulMod` spelling branch, assuming positive call arguments,
  backend-specific checks, or accepting C signed-remainder UB. The next focused
  falsifier must cover divisor `0`, `-1` with minimum Long, and an ordinary
  positive divisor before the fixed canary is run again.

## Previous completed self-host context - Option try-let and control-flow closure

- Current HEAD is `270e616d8128` on `main`; the worktree remains intentionally
  uncommitted and contains the user's accumulated GraphPlan work. The current
  focused canary is
  `.tmp/multi-routine-generalization/current-producer-canary-vr-set/routine-index-owner-current.mir.json`:
  36,183,978 bytes, 1,492 routines, SHA-256
  `D4C1FA7993B087E0517EF2383A24DA49A8E883DC9E467F110F877CF95899C3A8`.
  The older canary is not reusable because collection mutation rows now require
  an exact primary LocalRef. `.tmp` remains measurement evidence only, not a
  semantic authority or a progress unit.
- Completed objective card: collection mutation receivers are identified by
  producer-owned LocalRef carriage. Priority was exact routine/local-or-formal
  identity, parameter ordinal, ordered index/value expression identity,
  operation identity, then identical C/LLVM copy-in/out. Fact owners are the
  producer LocalRef attachment, routine signature/ABI facts, and the existing
  ArrayInt/ArrayString carriers. The last consumers are program operation
  storage/readiness and C/LLVM operation emission. Forbidden fallbacks are the
  old instruction-use receiver guess, unique-parameter inference, backend MIR
  rereads, spelling branches, and a second collection plan.
- `ArrayPush`, `ArraySet`, and `ArrayPop` now require a primary LocalRef on the
  producer wire. A local receiver joins its exact local row; a value-result
  receiver joins `parameter:<routine-source-syntax-id>:<ordinal>` to the
  existing ABI facts. ArrayInt value-result set is stable operation 37 and
  persists its index/value expression rows. C and LLVM consume the same target;
  LLVM allocates and writes back only the formal actually named by that row.
- The focused falsifier uses two same-typed `Array<Int>` value-result formals
  and mutates only ordinal 1. It proves the untouched array remains `7` and the
  selected array becomes `2`, so a unique-parameter or first-parameter guess
  cannot pass. Missing/wrong receiver, wrong type/index/value, broken graph,
  duplicate use, missing ABI, wrong owner, and out-of-range ordinal all fail
  closed. The renamed
  `tests/self_hosted/parity/direct_mir_scalar_array_mutation_owner.sh` passed
  C/LLVM execution in 15.1 seconds.
- The canonical self-driver was rebuilt after stable try operation 38 was
  admitted. Installed `bin/pgy-self-driver.exe` is 5,506,092 bytes, mtime
  2026-08-13 22:51:06 +09:00, SHA-256
  `4E018D71868AEE2AF7D5EFC5396EEE8C8D95D6982E5086659A17E8A5F54B8D90`.
  The final Option try focused gate passed in 11.6 seconds, the CI profile
  passed in 6.3 seconds, and `self_hosted_component_contract_smoke.sh`
  completed green in 406.9 seconds. The full GraphPlan aggregate, remote CI,
  commit, push, and publication were not run.
- A stable diagnostic first proved the reached topology rejection was GraphPlan
  ordinal 12, block 9, global row 131, `AST_CONTINUE`. GraphPlan ordinal 0 is
  `Main`, so ordinal 12 maps to JSON routine row 11 `JsonStringEnd`, not JSON
  row 12. Removing the earlier break marker did not move the rejection and
  falsified the first static guess.
- `DirectMirScalarCfgControlTransferFromOwners` now owns break/continue edge
  identity for both the single-routine and program GraphPlan consumers. Break
  targets must not dominate their source; continue targets must dominate it.
  Both require terminator position, one true successor, no false successor, and
  no uses. The new two-routine focused gate executes exact `25` in C and LLVM
  and rejects swapped break/continue targets. Existing continue (`42`) and
  break-exit (`3`, repeated-break `2`) gates remain green.
- `StringJoin` and its `Join` alias now join the existing semantic builtin
  signature to the existing `("string", "join")` runtime ABI row. The C path
  consumes the canonical String runtime block; LLVM consumes one bounded
  materializer for the same ABI. Wrong first-argument type and swapped
  arguments fail closed, while both targets execute exact joined output.
- Stable expression identities 69 (`StringJoin`) and 70 (`ToString(String)`)
  advance the wire schema from `graph-plan.v44` to `graph-plan.v46`. Stable
  control-flow operation 38 (`Option<Int>` try definition) then advances the
  current schema to `graph-plan.v47`.
  Historical labels such as `GraphPlan v45` are migration-rung numbers, not
  wire-schema suffixes; the two number lines must not be conflated.
- The next reached row was global 196, JSON routine row 15 `JsonFieldKey`:
  `ToString(field)` where `field` is already String. The semantic registry's
  `ToString^String^Unknown` signature now specializes String input to a
  target-neutral identity expression. Int input still consumes the existing
  formatting ABI. The focused nested String gate executes both specializations
  in C and LLVM and rejects a forged Bool argument.
- `DirectMirScalarProgramAppendOptionIntTryOperand` now consumes the persisted
  unary try graph, the admitted callable signature, and the existing OptionInt
  ABI receipt. It stores only the operand expression row and stable operation
  38. C and LLVM both return the admitted None value on absence and unwrap the
  Some payload into the declared Int local. The 16,766-byte focused MIR,
  SHA-256
  `48A785E1F75743D129F38491EA1CB7F1EF832A5E986156C74FFE699FD6B4AA35`,
  executes exact `8` then `0` in both targets. Wrong payload, enclosing return,
  operand edge, Option ABI, and value-result carriage fail closed without an
  artifact.
- The fixed canary now advances past row 257 and exits 1 after 231,489 ms with
  no artifact at global row 592: routine row 43 `JsonBoolValueOptWithin`, block
  0, branch
  `((start + 4) == end) && SubEqualsWithLen(json, end, start, 4, "true")`.
  Its graph is an equality on the left and a five-argument builtin call on the
  right of `logical_and`. Routine/global row remains a diagnostic coordinate,
  not a progress unit.
- A separate production producer run had completed MIR and JSON publication
  for a 201,952,842-byte, 6,460-routine artifact with SHA-256
  `320D314D8FB86569FAC3B17BCB0E04715B4FA658610EA09E244AEF65BF2CB96E`.
  That reproducible `.tmp` artifact and a disposable break-marker clone were
  retired to recover 238.6 MB after the linker reported a full D: volume. The
  36 MB focused canary and all source/log receipts remain. The full artifact
  was a different workload and was never a progress-ratio comparison.
- Strict SoT hard closure remains exactly `49/86 CLOSED`, `36 BRIDGE`,
  `1 ACTIVE` (57.0%). No top-level family was promoted. Integrated forecast
  remains 78% and strict language beta remains 83%; moving an internal canary
  boundary is executable evidence, not a percentage increment.
- Resume objective card: admit the reached Bool branch graph without flattening
  away short-circuit behavior. Priority is the persisted `logical_and`
  topology, left Bool value, right builtin signature/ordered arguments,
  conditional evaluation, then identical C/LLVM behavior. Fact owners are the
  semantic expression graph, builtin signature/runtime ABI rows, and the
  existing short-circuit projection owner. Last consumers are program branch
  expression storage and target branch emitters. Forbidden fallbacks are a
  `JsonBoolValueOptWithin`/`SubEqualsWithLen` spelling branch, eager right-side
  evaluation, raw JSON reread, duplicated builtin tables, or backend-specific
  graph reconstruction. The next focused falsifier must prove the right call is
  skipped when the left side is false, runs when true, and rejects a mutated
  call identity or argument order before one canary rerun.

## Previous completed self-host context - complete scalar/collection/enum parameter-role plan

- Current HEAD is `270e616d` on `main`; the worktree remains intentionally
  uncommitted. The fixed canary is the 35,814,796-byte, 1,484-routine MIR at
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`.
  GraphPlan remains `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v43`; this
  change adds no schema row, expression identity, target layout, or ABI.
- Objective card: classify each composable formal exactly once and compose the
  result with the bounded return families. Priority is declaration/type
  identity, carriage/resource/pass, required physical ABI, unique role claim,
  then the existing C/LLVM consumers. Fact owners remain
  `DirectMirRoutineSignatureFact`, `DirectMirScalarProgramLogicalRecordFact`,
  the ArrayInt/ArrayString facts, payload-free enum fact, and Option ABI
  receipts. Last consumers are the final callable signature and existing
  C/LLVM signature/type emitters. Forbidden fallbacks are routine/name/arity
  branches, generic Array widening, enum spelling exceptions, copied ABI
  policy, inferred layout, and backend MIR rereads.
- `direct_mir_scalar_program_callable_parameter_role_plan_owner.pgy` classifies
  every formal exactly once as a direct scalar value, logical-record input,
  by-value `Array<Int>`/`Array<String>`, `Array<Int>` value-result, or
  declaration-keyed payload-free enum value. Its role predicates consume
  `DirectMirScalarProgramCallableParameterSupported` and
  `DirectMirScalarProgramLogicalRecordParameterReady`; the plan owns only
  role cardinality and uniqueness. The three former direct-scalar,
  logical-record-input, and ArrayInt callable owner files are absent and
  negative-gated. Return composition remains in the final signature owner,
  which recognizes one nonempty-parameter Option family without treating
  Option as a scalar or copying its physical layout. The old String-return-only
  enum branch is deleted; String, Int, and Bool returns consume the same plan.
- The canonical current-source build exited zero with `0 error(s), 0 warning(s)`
  and installed `bin/pgy-self-driver.exe` at 5,445,510 bytes, mtime
  2026-08-13 03:49:45 +09:00, SHA-256
  `3AE645DA0F8D5AB6C338F7B8F691F2A25B1A3B515D90955966149677F589F43F`.
  The final build took 580,203 ms. Payload-free enum, ArrayInt/ArrayString value,
  and OptionInt/String/Bool focused C/LLVM gates pass. The role owner is 138/150
  lines and final signature owner is 135/140.
- All 44 direct GraphPlan gates pass in one final 357,270 ms run with the
  installed current driver. The structural component inventory passes in
  357,000 ms. Five copyout carriage mutations that had changed an unused formal
  into the newly valid by-value collection family now use unsupported
  `readonly-ref`; copy-in/out execution, caller misuse, ABI/layout, resource,
  pass, type, and publication negatives remain.
- The fixed LLVM canary exits 1 after 41,219 ms with no artifact. It passes
  routines 0..868 and fails closed at routine 869
  `MirIntentExecutionCaptureCompensations`: Int over direct String/Int values
  and one `MirIntentCompensationFacts` value-result. The next reached family
  boundary is a declaration-keyed logical-record value-result role, not a
  collection, enum, or routine/name/arity exception.
- SoT hard closure is now exactly `49/86 CLOSED`, `36 BRIDGE`, `1 ACTIVE`.
  This is a denominator correction, not two newly completed semantic owners:
  the Array<String> value-result and owned-return exact shapes shared
  `SFDirectMirScalarCfgProgramExtension` and explicitly were not top-level fact
  families. Their common ABI fact remains under the canonical GraphPlan owner;
  the existing boundary fact is the classified derived projection, and the
  duplicate-authority edge gate is green at 86 authorities and 164 derived
  carriers. The executable GraphPlan front advances from 144 to 869 routines.
  This does not by itself close a registry row; it raises only the weighted
  hard-self-host substitution component, producing the integrated 77% forecast.
  The
  five-family closure board in `docs/00_progress.md` still contains all 37
  nonclosed rows exactly once.
- Final documentation, live SoT mutation, authority-edge, protocol registry,
  and local CI-profile gates exit zero in 102,700 ms. This runner has no
  Rocq/Coq executable, so the proof model is a declared
  `PGY_ALLOW_MISSING_COQ=1` skip rather than a PASS; live owner/consumer and
  negative mutations did run. Remote CI, commit, push, and release publication
  were not performed.
- Windows execution ratchet: use `C:\msys64\usr\bin\bash.exe` with
  `/usr/bin:/ucrt64/bin` before repository make/bash gates. A Make focused
  target depends on the PHONY compiler build, so after one observed canonical
  install, call its focused script directly rather than rebuilding the compiler
  for every row. Long builds write explicit exit receipts; a blocked compound
  PowerShell cleanup/start command is execution policy, not product security.

## Previous completed self-host context - GraphPlan v76 positive ArrayInt copyout composition

- Current HEAD is `32bea428` on `main`; the worktree remains intentionally
  uncommitted. The fixed canary is still the 35,814,796-byte, 1,484-routine MIR
  at `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`.
  GraphPlan remains `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v43`; v73/v74
  add no schema row, expression identity, target layout, or physical ABI.
- V73 closes the coarse callable-inventory diagnostic seam. Route admission now
  consumes `DirectMirScalarProgramCallableSignatureSupportedWithFacts` once and
  preserves unsupported families as the stable receipt
  `owner=callable-signature stage=signature-family` with routine ordinal, name,
  and return type. `DirectMirScalarProgramRouteAdmissionDie` is still the last
  consumer; graph admission does not rescan MIR to reconstruct the failure.
  The zero-copyout mutation proves the exact stdout diagnostic and no-artifact
  behavior for both C and LLVM.
- The first exact fixed-canary receipt disproved the prior static hypothesis:
  routine 0 `JsonCharAt(String, Int) -> String`, not routine 1476, was the first
  rejected signature. The old callable inventory started at ordinal 1, so its
  coarse failure had hidden routine 0 rather than proving the late routine was
  the first unsupported owner.
- V74 replaces the exact Void-only scalar policy with
  `direct_mir_scalar_program_direct_scalar_callable_owner.pgy`. One declaration-
  independent owner now admits `Void` or scalar returns with zero-or-more
  scalar parameters only when every parameter is `value/none/direct` and has no
  ABI layout receipt. The old Void owner is deleted and negative-gated. The
  final signature's historical Int/Option narrowing now defers when this direct
  scalar owner has already admitted the complete signature.
- The focused fixture executes both `String(String, Int)` and
  `Int(String, Int, Int)` through C and LLVM, then rejects carriage, pass,
  resource, and ABI mutations without artifacts. The existing zero-or-more-
  parameter Void/process-exit gate also remains green. The V73 exact-receipt
  gate, CI profile, and structural component inventory pass; the component gate
  completed in 247.1 seconds. The full GraphPlan aggregate reused the canonical
  driver and exited zero after every C/LLVM parity and negative row passed.
- The final canonical DRV-2 build exited zero with no compiler warnings in its
  seed receipts. Installed `bin/pgy-self-driver.exe` is 5,443,434 bytes, mtime
  2026-08-12 21:54:30 +09:00, SHA-256
  `7F32E18072202A491AB8A4F6FE886FA73D108A94E7E9F13DA0A0A8D8D1F8CF2C`.
  An initial shell lacked `gcc`, and one foreground observation timed out after
  the native rebuild; neither was a security or repository failure. The build
  resumed from its owned object state and the final background exit receipt is
  zero. No remote CI, commit, push, or publication occurred.
- Windows execution ratchet: launch build wrappers with canonical
  `C:\msys64\usr\bin\bash.exe`, then prepend `C:\msys64\usr\bin` and
  `C:\msys64\ucrt64\bin` before invoking repository bash/make gates. Git-for-
  Windows bash is not a build carrier because its `/usr/bin` has no `make`. Do not use
  PowerShell `*>` as the success authority for native tools because stderr can
  be wrapped as `NativeCommandError` even when the owned compiler receipt says
  zero errors. Long canonical builds run through a hidden bash wrapper that
  writes an explicit exit file; inspect that receipt and the repository log
  before retrying. This avoids repeating the PATH/observation mistakes without
  changing product timeouts or security policy.
- The v74 fixed canary passed routines 0 and 1 and failed closed after 31.2
  seconds with no artifact at routine 3 `FindFrom`: `Option<Int>` return with
  direct `String`, `String`, and `Int` parameters.
- V75 closes that boundary without a new owner file or lifecycle axis.
  `DirectMirScalarProgramDirectScalarParametersReady` owns the zero-or-more
  scalar `value/none/direct/no-ABI` parameter proof. The complete direct-scalar
  callable composes it with `Void` or scalar returns; the final signature owner
  composes it separately with the existing `Option<Int>` return family. Option
  remains non-scalar and its physical ABI still comes from
  `DirectMirOptionMatchAbiFact`.
- The strengthened existing OptionInt fixture executes
  `String, String, Int -> Option<Int>` through C and LLVM and rejects carriage,
  pass, resource, ABI-required, and Option-layout mutations without artifacts.
  The focused direct-scalar gate, CI profile, and structural component inventory
  pass; the component gate completed in 288.3 seconds.
- The canonical v75 DRV-2 build exited zero and installed
  `bin/pgy-self-driver.exe` at 5,444,637 bytes, mtime
  2026-08-12 22:29:25 +09:00, SHA-256
  `46AAC4A24AE742469AFD9DC2B37CF22D3866A96093436B4D84FD2903FB47B2E2`.
  No remote CI, commit, push, or publication occurred.
- The fixed canary now passes routine 3 and fails closed after 32.4 seconds with
  no artifact at routine 13 `ReadJsonString`: String return, direct String/Int
  values, and one `Array<Int>` `value-result/direct` parameter with its admitted
  ABI receipt. The next objective is to compose the existing ArrayInt copyout
  owner with scalar return/value roles, without naming the routine, fixing the
  arity, or copying the collection ABI policy.
- V76 closes that exact-arity seam. The new 35-line
  `direct_mir_scalar_program_array_int_value_result_callable_owner.pgy`
  accepts a scalar return, direct scalar values, and one-or-more ArrayInt
  value-result parameters. It owns no physical layout. The final signature
  deletes `json_string_value_result`; persisted parameter identity and layout
  remain with `DirectMirScalarProgramArrayIntValueResultFact`.
- The existing ArrayInt focused fixture now contains both three- and four-
  parameter callables and proves C/LLVM copy-in/out for each. Layout, carriage,
  pass, resource, and ABI-required mutations all reject without artifacts. Its
  first uncached run passed in 88.8 seconds; the nested record regression, CI
  profile, component inventory, and full GraphPlan aggregate also pass. The
  component gate completed in 257.6 seconds after moving new mutations out of
  the capped common mutation bucket into a 35-line responsibility-named file.
  The first aggregate exposed one stale Void-owner source grep; after migrating
  that ratchet to the shared parameter-role proof, the complete rerun exited
  zero.
- The canonical v76 DRV-2 build exited zero and installed a 5,443,713-byte
  driver at 2026-08-12 22:57:19 +09:00, SHA-256
  `2A1CACAB0989A8BB10A552F2E9099DD1A9BC6AF77DC799F77DEE0DC5D81457AD`.
  No remote CI, commit, push, or publication occurred.
- The fixed canary passes routine 13 and fails closed after 37.2 seconds with no
  artifact at routine 59 `JsonObjectFactIndex`: `Option<Int>` return, one
  declaration-keyed logical record carried `readonly-ref/indirect`, and one
  direct String. The next objective is to compose the existing OptionInt return
  owner with positive readonly-record input roles, without inventing JSON-
  specific ancestry, fixed lifecycle, name, or arity policy.

## Previous completed self-host context - GraphPlan v72 one-or-more logical-record copyouts

- Current HEAD is `32bea428` on `main`; the worktree remains intentionally
  uncommitted. The fixed canary remains the 35,814,796-byte, 1,484-routine MIR
  at `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`.
  GraphPlan remains `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v43`; v72
  changes no schema, expression identity, target layout, or physical ABI.
- Objective card: collapse the reached one- and two-copyout signature paths
  behind the declaration-keyed logical-record owner. Priority is record
  identity, per-parameter carriage/pass/resource/ABI proof, positive copyout
  cardinality, readonly/scalar composition, then old-owner deletion. Fact
  owners are `DirectMirRoutineSignatureFact` and
  `DirectMirScalarProgramLogicalRecordFact`; last consumers remain the generic
  claimant/signature and C/LLVM parameter emitters. Forbidden fallbacks are an
  exact routine/arity policy, record-name or block-count matching, identity
  distinctness invented from spelling, physical ABI inference, backend MIR
  rereads, or keeping the old exact owner as an OR fallback.
- `DirectMirScalarProgramLogicalRecordValueResultSignatureReady` now admits
  one-or-more declaration-keyed `value-result/direct` records alongside
  zero-or-more `readonly-ref/indirect` records and direct ABI-free scalars.
  Every record row still requires resource `none`, `abi_required=false`, and
  layout id zero. The old
  `direct_mir_scalar_program_readonly_logical_record_two_logical_record_value_result_policy_owner.pgy`
  was deleted; its imports, cap row, final-signature OR, and target-parameter
  OR were removed. Component gates reject both the file and old function names.
- The existing one-copyout, readonly-one-copyout, and readonly-two-copyout
  focused gates all pass. The two-copyout gate now consumes the generic owner
  and rejects zero copyouts; equal record types are not declared invalid merely
  to preserve the deleted policy's former identity-distinctness rule. C and
  LLVM still enumerate each admitted parameter and copy both complete records
  on every return.
- The canonical compiler build passed in 714.9 seconds with zero errors and
  warnings. The installed driver is 5,443,432 bytes, mtime
  2026-08-12 20:17:30 +09:00, SHA-256
  `844BDB03B35E874567A546424D83654AD5B313D80C418EBA3A5F691E3B9C8259`;
  deleting the exact policy reduced it by 1,266 bytes. AST import-graph
  preflights for the generic policy and final signature owner pass. The
  structural component gate passes in 307.8 seconds after restoring the final
  signature owner to its 140-line cap, and the forty-three-gate aggregate
  reuses the same driver and passes in 340.4 seconds. No remote CI, commit,
  push, or publication occurred.
- The fixed canary advances past routines 1474/1475/1477..1480 and no longer
  stops at a record-copyout envelope. It runs for 175.8 seconds and then fails
  closed with no artifact at the coarse
  `direct MIR scalar CFG program callable inventory is invalid` boundary.
  Static elimination leaves routine 1476 `LoopFlowSummaryProjectionReady` as
  the first likely unsupported signature: Bool return, two readonly logical
  records, one direct Int, and zero copyouts. This is an inference, not yet an
  observed ordinal, because the inventory diagnostic omits owner/stage/routine.
- Next objective card: first make callable-inventory failure preserve the
  exact routine/parameter diagnostic already owned by route admission, then
  use that receipt to falsify the readonly-only signature hypothesis. If
  confirmed, compose zero-or-more readonly records with direct scalar values
  under scalar return without weakening positive-copyout admission. Forbidden
  shortcuts are naming routine 1476, treating the static inference as observed,
  allowing zero copyouts through the copyout owner, or adding another coarse
  exact-arity classifier.

## Last completed self-host context - GraphPlan v71 composed readonly-record inputs and one record copyout

- Current HEAD is `32bea428` on `main`; the worktree remains intentionally
  uncommitted. The fixed production canary is still
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`,
  1,484 routines). GraphPlan remains
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v43`; v71 adds no carrier row,
  expression identity, physical ABI, or backend-specific fact.
- Objective card: admit the reached logical-record signature by composing
  existing carriage roles. Priority is declaration identity, readonly/copyout
  carriage, exact copyout cardinality, direct scalar values, then target
  projection. Fact owners are `DirectMirRoutineSignatureFact` and
  `DirectMirScalarProgramLogicalRecordFact`; last consumers are the callable
  claimant/final signature owner and existing C/LLVM logical-record parameter
  emitters. Forbidden fallbacks are routine/record-name branches, exact block
  or parameter-count matching, copied field schemas, inferred physical ABI,
  backend MIR rereads, or admitting parameter SSA/body behavior through the
  signature owner.
- `DirectMirScalarProgramLogicalRecordValueResultSignatureReady` now accepts a
  scalar-returning routine whose parameters are direct ABI-free scalar values,
  declaration-keyed readonly records carried as `readonly-ref/indirect`, and
  exactly one declaration-keyed record carried as `value-result/direct`.
  Every record parameter still requires resource `none`, no physical ABI, and
  layout id zero. This is one compositional signature invariant, not a new
  routine-shaped policy or a claim that readonly and copyout records share a
  lifecycle.
- The original collection-bearing `ValidationSession` copyout fixture remains
  unchanged. The new
  `direct_mir_readonly_logical_record_single_value_result.pgy` fixture isolates
  two readonly records, two Int values, and one record copyout. Its C/LLVM gate
  proves readonly pointer inputs, ordinal-4 copy-in/out, exact runtime output,
  and rejection of readonly carriage/pass changes, a second copyout, and the
  unique copyout's carriage/pass changes. Attempts to combine unrelated local
  collection and inout-rebind surfaces into this fixture reached separate
  local-inventory owners; they were removed rather than silently widening the
  signature seam.
- The canonical compiler build passed in 543.2 seconds with zero errors and
  warnings. The installed `bin/pgy-self-driver.exe` is 5,444,698 bytes, mtime
  2026-08-12 14:51:58 +09:00, SHA-256
  `C8F12A82ADB2C281FABD4B7CD2B83EBCA59004CA4BD0A25361274BC3B57131DA`.
  Both focused gates pass, CI profile and the structural component inventory
  pass in 243 seconds, and the forty-three-gate GraphPlan aggregate reuses the
  same driver and passes every C/LLVM behavior/negative row in 275.2 seconds.
  No remote CI run, commit, push, or publication occurred.
- The fixed LLVM canary passes the former blocker at routine 1469
  `BlockCondWithExpressionOrder`, plus routines 1470..1473, then fails closed
  after 41.3 seconds with no artifact at routine 1474
  `EmitBlockStmtsWithExpressionOrder`. That String-returning routine has two
  readonly logical records, Int/Int/String/Bool direct values, and two distinct
  logical-record value-results at parameters 6 and 7. This is the next reached
  member of the same role composition axis, not evidence for a routine-name or
  eight-parameter exception.
- Next objective card: generalize logical-record copyout cardinality from
  exactly one to one-or-more while preserving declaration identity and the
  per-parameter carriage proof, then retire any now-subsumed exact two-copyout
  policy instead of leaving dual signature authority. The fixed routine 1474
  and the existing readonly-record/two-record-copyout executable gate are the
  falsifiers. Parameter SSA/rebinding and body expression gaps remain separate
  owners and must not be smuggled into this signature change.
- Tooling note: the desktop safety filter rejected one compound PowerShell
  command that combined deletion, redirection, and a dynamic exit. That was
  not a repository security failure or a compiler permission block. Using a
  unique temporary directory and simple non-destructive commands proceeded
  normally. MSYS `dirname` also requires `C:\\msys64\\usr\\bin` on PATH when a
  gate is launched directly from PowerShell.

## Last completed self-host context - GraphPlan v70 ABI-free Option of logical record

- Current HEAD is `32bea428` on `main`; the worktree remains intentionally
  uncommitted. The fixed production canary remains
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`,
  1,484 routines). GraphPlan remains
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v43`; v70 adds expression
  identities 56..59 but no persisted carrier row or interoperability ABI.
- The former handoff called routine 1261's return a nested Option. That was an
  incorrect reading of the spelling. `OptionStructRuntimeFact` is the payload
  record's declaration name, so `Option<OptionStructRuntimeFact>` is one
  ordinary `Option<T>` whose `T` is a declaration-keyed logical record. The
  declaration and every reached Option instruction explicitly carry
  `abi_layout_id=0`, `abi_layout_required=false`, and `abi_layout=null`.
- `DirectMirScalarProgramLogicalRecordFact` now unwraps Array and Option type
  constructors through the canonical semantic owners before resolving a
  declaration row. `DirectMirScalarProgramLogicalRecordOptionPayloadType`
  then joins `OptionPayloadTypeOpt` to that exact record identity. Contextual
  Some/None/IsSome/UnwrapOption builtin facts, expression readiness, callable
  returns/parameters, locals, phis, signatures, and direct calls all consume
  the same join. There is no routine/record-name allowlist and
  `Option<Unknown>` is accepted only as the persisted None leaf after the
  expected Option type is already owned.
- C and LLVM derive an internal tag-plus-record carrier from the admitted
  logical-record field order. This is a target-local GraphPlan representation,
  not a physical or external ABI receipt. The focused fixture uses a distinct
  `ProbeFact` and proves C/LLVM output `record-option` / `wrapped`; mutations of
  the return carrier, local carrier, or record physical-ABI flag fail before
  publication.
- The first focused LLVM run exposed a declaration-order bug: prepend-style
  output assembly placed `%Option<Record>` helpers before the payload record
  type. The second aggregate exposed both sides of a foreign-declaration SoT
  bug: an unconditional common `abort` declaration duplicated ArrayInt's
  declaration, while removing it entirely left OptionInt unwrap undefined.
  The final rule is now executable: logical record types precede dependent
  Option helpers, and `DirectMirScalarCfgLlvmForeignDeclarations` emits exactly
  one abort declaration when any Option family, logical-record Option, String
  length, or collection path needs it. Program emission is negative-gated
  against declaring abort itself.
- Sixteen changed roots passed parser preflight. The final structural component
  gate, including the foreign-declaration negative ratchet, passed in 182.2
  seconds. The final canonical build passed in 530.3 seconds with zero errors
  and warnings. The V70 focused and adjacent
  OptionString/logical-record/recursive-record gates pass. The final installed
  `bin/pgy-self-driver.exe` is 5,444,698 bytes, mtime
  2026-08-12 14:25:16 +09:00, SHA-256
  `B9CA69B451C36B93185C9F4F19421D6795BDB9D306AFA7BD11BCB56FA018A4B9`.
  The forty-two-gate behavior/negative aggregate reused that canonical binary
  and passed every row in 275.8 seconds. CI profile/source inventory is green;
  no remote CI run, commit, push, or publication occurred.
- The fixed LLVM canary passes routine 1261 and routines 1262..1468, then fails
  closed after 40.297 seconds with no artifact at routine 1469
  `BlockCondWithExpressionOrder`: parameter 4 has type
  `MirStructuredExpressionEmissionOrder`, carriage `value-result`, and no
  physical ABI. This is a separate 45-block/67-instruction String-returning
  callable with two readonly logical-record parameters, two direct Int values,
  and one declaration-keyed logical-record copyout.
- Next objective card: admit that exact logical-record value-result parameter
  by extending the existing declaration-keyed record copy-in/out owner, not by
  naming routine 1469. Priority is exact signature/carriage identity, record
  declaration join, all-return copyout, then body expressions. Fact owners are
  `DirectMirRoutineSignatureFact` and
  `DirectMirScalarProgramLogicalRecordFact`; last consumers are the claimant,
  final signature owner, and existing C/LLVM logical-record value-result
  emitters. Forbidden fallbacks are routine/type-name branches, count-only
  widening, copied field lists, physical ABI invention, backend MIR rereads,
  or admitting the body before the copyout carrier is proven.

## Last completed self-host context - GraphPlan v69 zero-parameter Void and direct Bool call

- Current HEAD is `32bea428` on `main`; the worktree remains intentionally
  uncommitted. The fixed production canary remains
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`,
  1,484 routines). GraphPlan remains
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v43`; v69 adds no carrier row,
  callable identity, target layout, or backend-specific decision.
- `DirectMirScalarProgramVoidScalarCallableSignatureReady` now owns the exact
  Void callable shape over zero or more direct ABI-free scalar value
  parameters. Zero arity is the empty member of that existing family, not a
  function-name exception or a second zero-Void policy. The zero-parameter
  branches in the claimant envelope and final signature owner now consume the
  same Void fact beside the existing non-Void zero-return owner.
- `DirectMirScalarProgramAppendExpression` consumes the existing
  `DirectMirScalarProgramZeroArgumentDirectCallFactFromGraph` for a persisted
  direct call marker only when its target callable has zero parameters. Calls
  with arguments remain owned by the existing CallArgument chain. Call-target
  SyntaxNodeId, callable ordinal, return type, and complete call-marker
  coverage remain authoritative; no spelling lookup or backend MIR reread was
  added.
- The changed owner roots passed parser preflights: Void policy 874,194-byte
  AST, route/signature roughly 2.19-MiB ASTs, and expression admission
  4,299,288-byte AST. The final signature owner remains exactly 140/140 lines.
  The final canonical build passed in 683.7 seconds with zero errors and
  warnings. The focused C/LLVM gate passed in 8.3 seconds and rejects five
  invalid return, carriage, exit-value, zero-Void return, and phantom-parameter
  mutations. Adjacent zero-parameter String-return and zero-call ArrayInt
  literal gates passed in 10.2 and 12.8 seconds. The structural component gate
  passed in 286.0 seconds.
- The CI-shaped forty-one-gate GraphPlan aggregate rebuilt the current final
  source and passed every row in 1002.6 seconds with zero errors and warnings.
  The final installed `bin/pgy-self-driver.exe` is 5,434,375 bytes, mtime
  2026-08-12 12:17:01 +09:00, SHA-256
  `F14D5A0050F92B5B4A315A6A01D6801F99F26FE50A3393D2CC99DED587A361EC`.
  No remote CI run, commit, push, or publication occurred.
- Two focused REDs were useful owner evidence. First, the Void policy alone
  did not move the zero-parameter early branches in the claimant/final
  consumers; those consumers were migrated to the same owner. Second, calling
  the new Void fixture from Main first exposed a separate unsupported
  standalone zero-argument Void call, while leaving it uncalled exposed the
  actual production-shaped `!ProbeReady()` Bool call. The final gate therefore
  compiles the complete zero-Void body and executes the pre-existing Log/Exit
  path; it does not claim general zero-argument Void-call expression support.
- The fixed LLVM canary remains deliberately RED after 48.840 seconds with no
  artifact. It passes routine 1106 and routines 1107..1260, then reports
  `owner=callable-route-envelope stage=return-type routine=1261
  name=OptionResultRuntimeStructOptionFact parameter=-1
  type=Option<OptionStructRuntimeFact> carriage=`.
- Next objective card: determine whether the existing Option physical ABI and
  logical-record inventory can jointly own routine 1261's nested
  `Option<OptionStructRuntimeFact>` return before touching its 30-block body.
  The exact signature has direct ABI-free `String` and `CodegenTypeEnv` value
  parameters, 39 instructions, and local Option/String/record/Bool/Int facts.
  Priority is nested Option identity and layout, declaration-keyed payload
  record, exact return transfer, then constructors/phi/member reads. Last
  consumers are claimant/final signature and existing C/LLVM Option/record
  emitters if their carried facts prove the shape. Forbidden fallbacks are a
  routine/record-name branch, flattening the nested Option to an existing
  scalar specialization, ABI inference from type spelling, whole-body
  admission before the return carrier exists, or backend MIR reread.

## Last completed self-host context - GraphPlan v68 unified Void record/String-tail ArrayString copyout

- Current HEAD is `32bea428` on `main`; the worktree remains intentionally
  uncommitted. The fixed production canary remains
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`,
  1,484 routines). GraphPlan remains
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v43`; v68 adds no carrier row,
  record layout, backend branch, or expression identity.
- `DirectMirScalarProgramVoidLogicalRecordArrayStringValueResultSignatureReady`
  is now the one exact family owner for a Void signature containing one direct
  `Array<String>` value-result with positive persisted ABI identity, one direct
  ABI-free declaration-keyed logical-record value, and exactly 0, 3, or 4
  ordered direct String values. The former separate three-String policy owner
  is deleted and component-gated against reappearance. An arbitrary String
  tail remains fail-closed.
- `DirectMirRoutineSignatureFact`, the logical-record inventory, and the
  ArrayString ABI fact remain authoritative. Existing C/LLVM collection
  copy-in/out, record-value, and String-value emitters are the last consumers.
  The v66, v67, and v68 runtime gates all consume that same owner; no routine or
  record-name allowlist exists.
- The structural component gate passed in 188.8 seconds. The first canonical
  attempt failed after 448.7 seconds because the consolidation edit omitted
  one `return false; }` boundary and the next record-check `if`; the parser seed
  correctly rejected the composed graph before installing a driver. After the
  two-line control-flow repair, a direct owner import-graph preflight passed in
  2.4 seconds and emitted a 2,008,258-byte AST. The canonical build then passed
  in 582.3 seconds with zero errors and warnings.
- The new four-String C/LLVM gate passed in 12.6 seconds, executed the actual
  mutation/copyout, and rejected twelve invalid carriage, type, pass,
  ABI/layout, record, String-tail, return, and cardinality mutations. The
  adjacent three-String and zero-String gates passed in 12.8 and 10.0 seconds.
  The CI-shaped forty-one-gate GraphPlan aggregate rebuilt the final driver
  once and passed every row in 990.3 seconds with zero errors and warnings.
- The final installed `bin/pgy-self-driver.exe` is 5,433,351 bytes, mtime
  2026-08-12 10:58:49 +09:00, SHA-256
  `E8E6D71ACA6729D9CE429B815ADA4FCB9073A7F8A21D2902AF54E6175FE4E7AE`.
  The local CI profile and `git diff --check` pass. No remote CI run, commit,
  push, or publication occurred.
- Same-mistake rules: before another full compiler rebuild, parse a changed
  owner root with the current `parser_ast_producer.exe`; source-inventory grep
  is not a syntax proof. Keep the full canonical parser/build as the installed-
  driver authority. The v67 ArrayString fixture rule also remains: push an
  owned String such as `ToString(...)`, never a borrowed parameter that the
  final owned-array drop would free.
- The fixed LLVM canary remains deliberately RED after 39.950 seconds with no
  artifact. It passes the former routine 926 boundary and routines 927..1105,
  then reports `owner=callable-route-envelope stage=return-type routine=1106
  name=CompilerSymbolRequireTable parameter=-1 type=Void carriage=`. This
  proves callable-envelope progress only.
- Next objective card: admit the zero-parameter Void envelope through the
  existing `DirectMirScalarProgramVoidScalarCallableSignatureReady` owner,
  whose current `param_count < 1` rule excludes the vacuous zero-parameter
  case. Priority is exact signature identity, zero-arity proof, then the
  reached conditional direct-call/Log/Exit body. The fact owner is
  `DirectMirRoutineSignatureFact`; the last signature consumer is the
  claimant/final callable envelope, followed by the existing graph/branch and
  call emitters. Forbidden fallbacks are a `CompilerSymbolRequireTable` name
  branch, body-spelling inference at the signature boundary, inventing a dummy
  parameter, treating every zero-parameter return as Void, or backend MIR
  reread. The exact falsifier has 3 blocks and 3 instructions and calls
  `CompilerSymbolTableReady()`, `Log(...)`, and `Exit(1)` on the false-ready
  branch.
- Language-modeling note: `ability` remains a subject-bound behavior contract,
  not a complete lifecycle, universal trait, or inheritance relation. Further
  syntax splitting requires an observed independent ownership, movement,
  failure, or authority boundary rather than resemblance to another language.

## Last completed self-host context - GraphPlan v67 Void record/three-String ArrayString copyout

- Current HEAD is `32bea428` on `main`; the worktree remains intentionally
  uncommitted. The fixed production canary remains
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`,
  1,484 routines). GraphPlan remains
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v43`; v67 adds no carrier row
  or physical record layout and does not widen the two-parameter v66 policy.
- `DirectMirScalarProgramVoidLogicalRecordThreeStringArrayStringValueResultSignatureReady`
  owns routine 922's complete five-parameter envelope: one direct
  `Array<String>` value-result with persisted layout identity `703020034`, one
  direct ABI-free logical-record value, and three ordered direct String values
  under a Void return.
  `DirectMirRoutineSignatureFact`, the logical-record inventory, and the
  ArrayString ABI fact remain authoritative. Existing C/LLVM collection
  copyout, record-value, and String-value emitters are the last consumers.
- The canonical build passed in 603.4 seconds with zero errors and warnings.
  The focused C/LLVM gate passed in 6.5 seconds, executed the actual mutation
  and copyout, and rejected twelve invalid carriage, type, pass, ABI/layout,
  missing-record, String-tail, return, and cardinality mutations. Adjacent v66
  passed in 6.5 seconds. The structural component gate passed in 294.5 seconds.
  The CI-shaped forty-gate GraphPlan aggregate rebuilt the final driver once
  and passed in 907.4 seconds with zero errors and warnings.
- The final installed `bin/pgy-self-driver.exe` is 5,435,645 bytes, mtime
  2026-08-12 09:50:54 +09:00, SHA-256
  `BA94C021EAA444DE74846BD88A86952BF0095C2262337E2FFBEC3A0E6B196B4E`.
  No remote CI run, commit, push, or publication occurred.
- The first focused artifact compiled with the exact signature, then crashed
  because the fixture pushed borrowed `c_name` directly into an owned
  `Array<String>` whose final drop frees its elements. The fixture now pushes
  `ToString(StringLength(c_name))`, an owned String, and the actual copyout
  passes on both backends. Recurrence rule: an ArrayString mutation fixture must
  not use a borrowed literal/parameter as an owned element merely to exercise
  the carrier.
- The fixed LLVM canary remains deliberately RED after 39.8 seconds with no
  artifact. It passes routine 922 and routines 923..925, then reports
  `owner=callable-route-envelope stage=return-type routine=926
  name=CodegenTypeEnvStateAppendTypedValueBinding parameter=-1 type=Void carriage=`.
  This proves callable-envelope progress only.
- Next objective card: close routine 926's exact six-parameter Void signature:
  one direct `Array<String>` value-result with layout identity `703020034`, one
  direct ABI-free `CodegenTypeEnv` value, and four direct String values. It has
  1 block and 2 instructions. Fact owners are
  `DirectMirRoutineSignatureFact`, the logical-record inventory, and the
  ArrayString ABI fact; last consumers are claimant/final signature and the
  existing C/LLVM record-value and collection copy-in/out emitters. Forbidden
  fallbacks are a routine-name allowlist, widening v67 with an arbitrary String
  count, treating the record as readonly/value-result, missing/copied
  ArrayString layout, or backend MIR reread.
- Language-modeling note: `ability` is documented as a subject-bound behavior
  contract, not a complete lifecycle, universal trait, or inheritance relation.
  Further syntax splitting requires an observed independent ownership, movement,
  failure, or authority boundary rather than resemblance to another language.

## Last completed self-host context - GraphPlan v46 logical-record Array copyout

- Current HEAD is `90302b7a` on `main`; the worktree remains intentionally
  uncommitted. The fixed production canary remains
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`,
  1,484 routines). GraphPlan stays at
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v39`; v46 adds no carrier
  column or expression identity.
- The existing logical-record inventory now derives the element declaration
  of canonical `Array<T>` signature types. `CompilerAbiNominalArrayLayoutFact`
  owns the compiler-internal three-field `data,len,cap` carrier, C length/type
  spelling, and LLVM `{ ptr, i64, i64 }` aggregate. The mixed callable policy
  admits exactly one declaration-keyed record-Array value-result, one-or-more
  persisted public `Array<Int>` value-results, and direct scalar values in a
  Void signature. C and LLVM copy both carrier families in and out on every
  explicit return. No routine or record-name allowlist was added.
- Canonical `make self-host-compiler` succeeds in 508.7 seconds with zero
  errors and warnings. The focused installed-driver gate passes in 4.5
  seconds: C and LLVM compile/run with exact output
  `logical-record-array-copyout-ready`, and carriage, missing-element
  declaration, and false physical-ABI mutations fail without publication.
  The component structural contract passes in 174.4 seconds. The CI-shaped
  twenty-two-gate GraphPlan aggregate performs one canonical rebuild and
  passes every focused gate in 588.7 seconds with zero errors and warnings.
  The documentation-quality aggregate, local CI-profile contract, and
  `git diff --check` also pass on the final tree. The documentation aggregate
  includes the agent-boundary sentinel, object/action boundary contract,
  documentation-quality scan, and frozen post-self-host validation manifest.
  The final installed `bin/pgy-self-driver.exe` is 5,374,839 bytes, mtime
  2026-08-11 15:38:45 +09:00, SHA-256
  `7820110808C73949A587C606F267F5FF5BBD7CB2BB686DDF0955978DC5222088`.
  No remote CI run, commit, push, or publication occurred.
- The fixed LLVM canary passes the former routine 495
  `CodegenAstTextNodeInventory(String, inout Array<CodegenAstTextNode>,
  inout Array<Int>) -> Void` boundary. It remains deliberately RED after
  18.535 seconds at:
  `owner=callable-route-envelope stage=parameter-type-or-carriage routine=563
  name=CodegenAstTextParentRows parameter=0 type=Array<CodegenAstTextNode>
  carriage=value`. No artifact is published. That next routine returns
  `Array<Int>` and has ten blocks, thirteen instructions, and local ArrayInt
  rows; its body also indexes the record Array and reads record members.
- Next objective card: admit the declaration-keyed three-field record Array as
  an exact by-value parameter only if the existing logical-record identity and
  target layout can also own parameter reads. Priority is signature identity,
  by-value carrier transfer, indexed element identity, member reads, ArrayInt
  return, then loop operations. Last consumers are claimant/final signature,
  parameter expression projection, index/member readiness, and C/LLVM return.
  Forbidden fallbacks are a `CodegenAstTextNode` name branch, treating the
  carrier as public ArrayInt/ArrayString, backend MIR rereads, or widening all
  nominal Arrays before the by-value read owner exists.
- Same-mistake rules: compiler-owned `Array<Record>` is the three-field
  nominal-array layout, not the public four-field Array layout. Derive its
  element through canonical Array shape and declaration identity. Copy out
  every carried value-result family on every return; fixing only ArrayInt is
  incomplete. Keep owner caps green before the long component gate. A focused
  runtime fixture may leave the copyout routine uncalled when the active rung
  is the callee ABI boundary, but it must compile both emitted artifacts and
  pin the generated copy lifecycle plus fail-closed mutations.

## Last completed self-host context - GraphPlan v45 populated ArrayInt call literal

- Current HEAD is `90302b7a` on `main`; the worktree remains intentionally
  uncommitted. The fixed production canary remains
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`,
  1,484 routines). GraphPlan is now
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v39`; stable expression kind
  54 is the populated `Array<Int>` literal whose elements are zero-argument
  direct calls returning `Int`.
- `DirectMirScalarProgramAppendArrayIntCallLiteral` owns the canonical
  persisted array spine and joins each call marker's SyntaxNodeId to the
  callable inventory. It admits one or more ordered zero-parameter `Int`
  calls without copying the reached 89 typed-tag values into another table.
  The existing ArrayInt ABI receipt owns storage layout. C emits one
  heap-backed materializer with sequential assignments after callable
  prototypes; LLVM emits the calls in graph order, truncates canonical Int
  values to the captured i32 element width, and stores them sequentially.
- Canonical `make self-host-compiler` succeeds in 578.2 seconds with zero
  errors and warnings. The focused installed-driver gate passes in 5.8
  seconds: C and LLVM both print exactly `first`, `second`, `third`, `0`, and
  missing-target, wrong-return-type, and ArrayInt-layout mutations fail before
  publication. The component structural contract passes in 195.5 seconds.
  The CI-shaped twenty-one-gate GraphPlan aggregate performs one canonical
  rebuild and passes every focused gate in 736.5 seconds with zero errors and
  warnings. The final installed `bin/pgy-self-driver.exe` is 5,363,614 bytes,
  mtime 2026-08-11 14:39:18 +09:00, SHA-256
  `7061FA32AC7C2FE276E59B7E4440AD013E6524A418EF0F868B96F31217DF5D54`.
  The local CI profile and `git diff --check` pass; no remote CI run, commit,
  push, or publication occurred.
- The final fixed LLVM canary passes the former routine 405
  `TypedAstKindTags() -> Array<Int>` boundary. It remains deliberately RED
  after 19.034 seconds at:
  `owner=callable-route-envelope stage=return-type routine=495
  name=CodegenAstTextNodeInventory parameter=-1 type=Void carriage=`. No
  artifact is published. That routine is not a zero-parameter Void case: it
  has `String value`, `Array<CodegenAstTextNode> value-result`, and
  `Array<Int> value-result` parameters.
- Next objective card: audit whether the existing logical-record collection
  and ArrayInt copyout facts can jointly own that exact mixed Void signature
  before touching its 25-block body. Priority is declaration identity,
  collection element layout, two distinct copyout identities, every-return
  copyout, then expression/operation support. Last consumers are the callable
  envelope, direct-call addressability, callee copy-in/out, and every return
  edge. Forbidden fallbacks are a routine-name allowlist, treating
  `Array<CodegenAstTextNode>` as ArrayString/ArrayInt, copying only the second
  copyout, or reopening MIR in a backend.
- Same-mistake rules: target prerequisites belong to their actual operation
  (`malloc`, not `strlen`). A fixture cardinality is not a semantic minimum;
  the owner admits one-or-more elements although the execution gate uses three
  to prove order. Do not widen standalone zero-argument calls merely to make a
  populated-literal fixture executable; execute the literal directly and keep
  that separate seam closed. C must not use an unsequenced initializer for
  side-effecting elements. Check owner caps before the long component gate.

## Last completed self-host context - GraphPlan v44 Void scalar Log/Exit

- Current HEAD is `90302b7a` on `main`; the worktree remains intentionally
  uncommitted. The fixed production canary remains
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`,
  1,484 routines). GraphPlan is now
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v38` because process exit is a
  new stable operation identity.
- `DirectMirScalarProgramVoidScalarCallableSignatureReady` owns the exact
  `Void` return plus one-or-more direct scalar value parameters. Both the broad
  claimant envelope and final callable signature consume that owner. Statement
  admission maps persisted `Exit(Int)` to
  `DirectMirScalarCfgOpProcessExit`, while the existing `Log(String)` row keeps
  source order. `CompilerRuntimeCallAbiProcessExitFact` projects the canonical
  `host-io|exit|int_to_noreturn` registry row to C and LLVM; emitters do not
  hardcode a second symbol or reopen MIR.
- The focused installed-driver C/LLVM gate passes in 4.0 seconds. Both programs
  print exactly `fail-closed: probe`, terminate with status 7 before the
  following log, and reject return-type, parameter-carriage, and Exit-argument
  mutations without publishing an artifact. The component structural contract
  passes in 179.1 seconds. The CI-shaped twenty-gate GraphPlan aggregate
  performs one canonical rebuild and passes all twenty focused gates in 570.4
  seconds with zero errors and zero warnings. Installed
  `bin/pgy-self-driver.exe` is 5,353,092 bytes, mtime
  2026-08-11 13:46:15 +09:00, SHA-256
  `DE9CCA8E36E5338642A63101020B5EB517D182DE373D48B36BA53EE9B8B8BFF7`.
  The local CI profile and `git diff --check` pass; no remote CI run, commit,
  push, or publication occurred.
- The fixed LLVM canary now passes the former routine 277
  `MirLowerFailClosed(String) -> Void` boundary. It remains deliberately RED
  after 19.942 seconds at:
  `owner=callable-route-envelope stage=return-type routine=405
  name=TypedAstKindTags parameter=-1 type=Array<Int> carriage=`. No artifact is
  published.
- Next objective card: determine whether the existing ArrayInt literal and
  direct-call facts can already own the zero-parameter
  `TypedAstKindTags() -> Array<Int>` body, whose populated literal contains
  direct calls to the typed tag owners. Priority is persisted ArrayInt ABI,
  zero-parameter return identity, populated literal element identity, direct
  call order, C/LLVM parity, then canary progress. Last consumers are the
  zero-parameter route envelope, ArrayInt literal admission, return emission,
  and caller materialization. Forbidden fallbacks are a routine-name allowlist,
  copying the 89 tag values into a new table, treating a populated literal as
  the existing empty-literal fact, or backend MIR rereads.
- Same-mistake rules: the broad route envelope and final signature must consume
  the same callable-shape owner; fixing only the later signature leaves the
  reached routine rejected earlier. A shared emitter signature change must
  migrate every caller, including routes whose current plans contain no exit.
  C/LLVM comparison alone is insufficient for a noreturn effect: the focused
  gate pins independent expected stdout and process status.

## Last completed self-host context - GraphPlan v43 logical-record value-result

- Current HEAD is `90302b7a` on `main`; the worktree remains intentionally
  uncommitted. The fixed production canary remains
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`,
  1,484 routines). GraphPlan remains
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v37`; v43 adds no carrier
  column or expression identity.
- `DirectMirScalarProgramLogicalRecordValueResultSignatureReady` admits one
  declaration-keyed logical-record `value-result` formal, direct scalar value
  formals, and a scalar return. The existing logical-record fact owns exact
  declaration identity and field order. C and LLVM load the whole aggregate at
  entry and copy it out before every explicit return. Direct-call readiness
  consumes the same target-neutral copyout fact; it cannot substitute the
  ArrayString copyout inventory.
- The focused installed-driver C/LLVM execution gate passes in 3.7 seconds and
  rejects carriage and pass-shape mutations without publishing an artifact.
  The CI-shaped nineteen-gate GraphPlan aggregate performs one canonical
  rebuild and passes all nineteen gates in 596.6 seconds with zero errors and
  zero warnings. The component structural contract passes in 180.6 seconds,
  and the CI profile plus `git diff --check` pass. Installed
  `bin/pgy-self-driver.exe` is 5,347,086 bytes, mtime
  2026-08-11 13:02:33 +09:00, SHA-256
  `621735768146B0A40C81E445E3A47DD897DB3E30CC34F34E6701F1A73365F777`.
  No remote CI run, commit, push, or publication occurred.
- The fixed LLVM canary passes routines 275 and 276. It remains deliberately
  RED after 19.3 seconds at:
  `owner=callable-route-envelope stage=return-type routine=277
  name=MirLowerFailClosed parameter=-1 type=Void carriage=`. No artifact is
  published.
- Next objective card: close exactly `MirLowerFailClosed(String) -> Void` only
  if the existing statement-effect facts can distinguish `Log(String)` and
  `Exit(Int)` and both targets preserve their order. Priority is Void signature
  identity, scalar value parameter, persisted statement identity, Log/Exit
  effect ordering, C/LLVM parity, then canary progress. Last consumers are the
  callable envelope, expression/statement admission, block emission, and
  process-exit runtime ABI. Forbidden fallbacks are routine-name allowlists,
  treating Exit as an ordinary returning expression, swallowing the following
  path, or backend MIR rereads.
- Same-mistake rules: callable admission, final plan identity, and backend
  copyout must consume the same target-neutral fact. The first focused fixture
  appended three `ArrayLength(session.field)` observations and hit a separate
  logical-record collection-member use seam; removing those observations kept
  the value-result gate owner-focused instead of widening it. Check all
  changed owner caps before rerunning the full structural gate.

## Last completed self-host context - GraphPlan v42 Bool plus four ArrayString copy-outs

- Current HEAD is `90302b7a` on `main`; the worktree remains intentionally
  uncommitted. The fixed production canary remains
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`,
  1,484 routines). GraphPlan stays at
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v37`: v42 admits an exact
  signature shape and reuses carried ArrayString ABI rows; it adds no carrier
  column or expression identity.
- `DirectMirScalarProgramArrayStringValueResultSignatureReady` owns exactly two
  admitted shapes: the preceding `Void` return with one ArrayString
  value-result formal, and `Bool` return with seven parameters whose first
  three are `String, Int, Int` values and whose last four are ArrayString
  value-result formals. Every other return, count, type, carriage, resource,
  pass shape, or ABI-row combination remains rejected.
- C and LLVM already iterate every carried ArrayString value-result row for
  signature, copy-in, and copy-out. v42 adds no backend-local policy: the gate
  proves all four rows are copied out before both early and final Bool returns.
  The fixture leaves the arrays unmodified; selecting one of several mutable
  formals remains a later operation-identity seam.
- Canonical build and the CI-shaped eighteen-gate aggregate complete with zero
  errors and zero warnings. Installed `bin/pgy-self-driver.exe` is 5,340,388
  bytes, mtime 2026-08-11 11:56:19 +09:00, SHA-256
  `65FD079F09E2DEEBFC0A4433120958127CCF7EE68385298BDC5A860FA7D67AFE`.
  The aggregate, including its canonical rebuild, passes in 553.3 seconds; the
  new installed-driver focused leg alone passes in 5.1 seconds. The component
  structural contract passes in 172.9 seconds, and the CI profile passes. No
  remote CI run, commit, push, or publication occurred.
- The fixed LLVM canary passes routine 268 and remains deliberately RED after
  18.9 seconds at:
  `owner=callable-route-envelope stage=parameter-type-or-carriage routine=275
  name=MirAbiRememberRequiredTuple parameter=0
  type=MirAbiLayoutValidationSession carriage=value-result`. No artifact is
  published.
- Next objective card: close that exact declaration-keyed logical-record
  value-result only after proving nominal identity, field order, copy-in,
  every-return copy-out, and caller addressability. Forbidden fallbacks are
  treating all logical records as mutable references, name/arity allowlists,
  copying only a field prefix, or reopening MIR in a backend.
- Same-mistake rules: inspect existing last consumers before adding a backend
  owner; both targets already had the needed multi-row copy loop. Join the
  signature-wide exception to the per-parameter policy, or valid rows still
  fail individually. Do not make a structural grep depend on a source line
  break. Keep fixtures owner-focused: the first draft used `end >= start` and
  correctly hit an unrelated comparison boundary, so the final fixture uses
  Bool literals to isolate return/copy-out behavior.

## Last completed self-host context - GraphPlan v41 zero-parameter Long literal return

- Current HEAD is `90302b7a` on `main`; the worktree remains intentionally
  uncommitted. The fixed production canary is still
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`,
  1,484 routines). GraphPlan schema is now
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v37` because v41 adds one new
  stable expression kind rather than only extending an existing fact.
- v41 admits canonical `Long` as the fourth scalar type for the reached
  zero-parameter literal-return shape. The existing ABI SoT already fixes Long
  to C `long long` and LLVM `i64`; the new expression fact strips the source
  `L` suffix once and target projections emit `LL`/bare i64 digits. It does not
  admit Long arithmetic, casts, or zero-argument call expressions.
- Stable expression identities are exact: ArrayInt empty literal=51,
  ArrayBool empty literal=52, Long literal=53. The first implementation reused
  51 and produced a digest-valid but semantically colliding plan; the focused
  and component gates now pin all three identities. The first build also used
  the three-argument form of four-argument `SubstringWithLen`; generated C
  compilation caught it, and the owner now passes the known source length.
- Canonical `make self-host-compiler` completes with zero errors and zero
  warnings. Installed `bin/pgy-self-driver.exe` is 5,339,880 bytes, mtime
  2026-08-11 11:14:41 +09:00, SHA-256
  `AA3A875AD1B5DE800F87D07E767E252511E56EE7F21CC08A74E4A46DC92BAEFB`.
  All seventeen focused GraphPlan C/LLVM gates pass in 101 seconds. The full
  component structural contract passes in 172.7 seconds, and the final CI
  profile passes. No remote CI run, commit, push, or publication occurred.
- CI/CD keeps all seventeen focused gates behind the one
  `SELFHOST_SCALAR_GRAPH_PLAN_GATE`; the Linux job still performs one serial
  make invocation with `self-host-compiler` first. The profile pins the exact
  dependency string, so the new Long gate cannot silently fall out of CI.
- The fixed LLVM canary passes the former routine 256/257 Long literal-return
  boundary and the following Long signature inventory. It remains deliberately
  RED after 19.1 seconds at:
  `owner=callable-route-envelope stage=parameter-type-or-carriage routine=268
  name=MirAbiLayoutFieldsCaptureWithin parameter=3 type=Array<String>
  carriage=value-result`. No artifact is published.
- Next objective card: close the exact `Bool` return plus four
  `Array<String> value-result` formals only if one owner proves copy-out on every
  return while preserving the Bool result. Priority is signature identity,
  existing ArrayString ABI, multiple value-result identities, return-value plus
  copy-out ordering, C/LLVM parity, then canary progress. Fact owners are
  `DirectMirRoutineSignatureFact` and `DirectMirScalarProgramArrayStringAbiFact`;
  last consumers are callable policy, call addressability, callee copy-in,
  every return edge, and Bool return emission. Forbidden fallbacks are widening
  every non-Void return, dropping the return value, copying only the first
  array, spelling/arity allowlists, or backend MIR reopen.
- Same-mistake rules: a new stable kind must be checked against identities in
  sibling owners, not only the central file. A generated-C compile is required
  because source gates do not validate runtime-call arity. `Long` sharing the
  same target width as `Int` does not authorize reusing the Int semantic kind.
  The focused fixture intentionally leaves zero-argument calls closed; it proves
  representation without claiming arithmetic or invocation support.

## Last completed self-host context - GraphPlan v40 direct ArrayInt return

- Current HEAD is `90302b7a` on `main`; the worktree remains intentionally
  uncommitted and contains the preceding Option, logical-record, readonly-ref,
  zero-parameter, collection value-result/value-parameter, owned-return, and
  collection-field closures. The fixed production canary remains
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`,
  1,484 routines). The carrier schema remains
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v36`; v39/v40 extend admitted
  facts and consumers rather than adding another CFG/SSA/phi carrier.
- GraphPlan v39 carries declaration-keyed nested logical records through return,
  local storage, by-value parameters, constructors, members, and C/LLVM target
  projection. `ProgramIndex` contains `ReachabilityRows`, which contains the
  exact admitted `Array<Bool>` storage ABI. An unreferenced same-shape record is
  not a candidate. Constructor consumers share one argument-row owner because
  normalized one/two-argument calls use `left/right` while larger calls use the
  nary rows; mixed storage is rejected. ABI-seal failures preserve their exact
  owner stage.
- GraphPlan v40 admits a direct `Array<Int>` return through the existing
  program-wide ArrayInt storage receipt. Callable return/signature policy and
  instruction capture accept the exact return row, while only formal
  `value-result` carriage creates copy-in/copy-out identity rows. The focused
  three-routine fixture deliberately avoids the older two-routine specialized
  Array-return classifier and exercises the scalar GraphPlan path: the returned
  carrier is stored in a caller local, passed by value, and prints exact `0` in
  both C and LLVM. Layout, return-kind, and parameter-layout mutations fail
  before artifact publication.
- Canonical `make self-host-compiler` completes with zero errors and zero
  warnings. Installed `bin/pgy-self-driver.exe` is 5,340,318 bytes, mtime
  2026-08-11 10:23:42 +09:00, SHA-256
  `2D7F4291F43B6996F6C25AD5366D231467741C2C29507362E8CE5BB6D72D8398`.
  All sixteen focused GraphPlan C/LLVM gates pass in one 117.3-second installed-
  driver run. The full component structural contract passes in 172.5 seconds,
  and `tests/self_host_ci_profile_smoke.sh` passes.
- CI/CD keeps the sixteen focused gates behind one
  `SELFHOST_SCALAR_GRAPH_PLAN_GATE`. The Linux job builds the phony
  `self-host-compiler` prerequisite and runs all consumers in one serial `make`
  invocation, so a second invocation cannot rebuild the driver. The profile
  gate pins the exact sixteen dependencies and the one-invocation workflow.
  No remote CI run, commit, push, or publication occurred.
- The fixed LLVM canary passes the former v39 boundary at routine 229
  `MirRoutineGraphDistances -> Array<Int>`. It remains deliberately RED after
  18.1 seconds at the next exact receipt:
  `owner=callable-route-envelope stage=return-type routine=256
  name=MirAbiLayoutModulus parameter=-1 type=Long carriage=`. No artifact is
  published. This is program-wide route progress, not proof that every prior
  routine body reached GraphPlan.
- Next objective card: admit the reached direct `Long` return only if the
  canonical scalar type/target owners already preserve its width and signed
  semantics. Priority is exact type identity, one return/signature policy,
  C/LLVM representation parity, negative width/type mutation, then canary
  progress. Fact owner is the admitted callable signature plus the existing
  scalar value-type projection; last consumers are route/signature, local,
  expression, return, and direct-call projection. Forbidden fallbacks are
  treating `Long` as `Int`, spelling-only admission, host-width inference,
  copied backend constants, or skipping routine 256.
- Same-mistake rules: physical storage identity and transfer identity are
  separate. A direct Array return consumes the shared storage receipt but never
  becomes a value-result copy-out row. Normalized constructor arity is a storage
  detail owned by one argument view, not by each consumer. Unsupported negatives
  must use a truly unsupported field family rather than preserving a stale
  expectation after support expands. A broad legacy route candidate must not
  intercept a larger scalar program, and the phony compiler prerequisite stays
  in the same CI make invocation as its consumers.

## Last completed self-host context - GraphPlan v38 exact by-value collection parameters

- Current HEAD is `90302b7a` on `main`; the worktree remains intentionally
  uncommitted and contains the preceding Option, logical-record, readonly-ref,
  zero-parameter, collection value-result, owned-return, and collection-field
  closures. The fixed production canary remains
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`,
  1,484 routines). GraphPlan schema remains
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v35`; these rungs extend exact
  admitted facts rather than changing the carrier schema.
- GraphPlan v37 admits direct value-carried `Array<Int>` parameters. The
  parameter ABI row and `DirectMirRoutineSignatureFact` remain the owners.
  `DirectMirScalarProgramArrayIntValueResultFact` now validates the one physical
  ArrayInt layout for both `value` and `value-result`, but stores routine and
  parameter identities only for value-result copy-in/copy-out. C and LLVM pass
  the value carrier directly and never synthesize a mutref.
- GraphPlan v38 applies the same ownership split to `Array<String>` through the
  existing `DirectMirScalarProgramArrayStringAbiFact`. The old caller-frame
  index/lifetime proof is not a generic ArrayString rule: one named
  `DirectMirScalarProgramArrayStringBoundarySignatureReady` predicate restricts
  it to the exact one-parameter `Array<String> value -> String` signature, and
  both ABI sealing and extension readiness consume that predicate. General
  value carriage therefore does not inherit the legacy boundary, while
  value-result identities and owned-return cleanup remain unchanged.
- The v37/v38 fixtures execute caller-owned empty arrays through a by-value
  callee copy. C and LLVM produce exact `7`/`8` output and prove value signatures,
  caller arguments, and callee locals without mutrefs. Mutated physical offsets,
  `readonly-ref` carriage, and indirect pass shape are rejected in both targets
  without artifact publication. All fourteen focused GraphPlan gates pass in
  56.0 seconds against the installed driver.
- Canonical `make self-host-compiler` exits 0; the final self-host stage reports
  zero errors and zero warnings. Installed `bin/pgy-self-driver.exe` is
  5,328,639 bytes, mtime 2026-08-11 08:20:50 +09:00, SHA-256
  `496D59B47E124AEDE8F6A224500BE9CFAC6DFB0B24A10678C0CF38ADD70FBCCE`.
  The full component structural contract and CI profile pass. No remote CI run,
  commit, push, or publication occurred.
- CI/CD keeps all fourteen focused gates behind the single
  `SELFHOST_SCALAR_GRAPH_PLAN_GATE`. The Linux self-host parity job now builds
  the phony `self-host-compiler` target and runs every consumer in one serial
  `make` invocation. The CI profile rejects a second make invocation, preventing
  the former redundant full driver rebuild while preserving driver-first order.
- The fixed LLVM canary passes both collection parameters of routine109 and
  remains deliberately RED after 16.5 seconds at the next exact receipt:
  `owner=callable-route-envelope stage=return-type routine=159
  name=BuildMirProgramRoutineIndexFromTable parameter=-1
  type=MirProgramRoutineIndex`. No artifact is published. This is program-wide
  route progress, not evidence that every preceding body reached GraphPlan.
- Next objective card: determine whether the declaration-keyed
  `MirProgramRoutineIndex` logical record can be carried as an exact return
  without adding a spelling or first-shape exception. The declaration field
  identity and existing logical-record fact are the candidate owners; last
  consumers are callable route/signature, C/LLVM return projection, call result,
  and local storage. Forbidden fallbacks are a name allowlist, first compatible
  declaration, copied native offsets, backend MIR reopen, or weakening all
  logical-record returns. The falsifier must include an unreferenced same-shape
  distractor, execute C/LLVM return flow, and reject identity/order/layout drift.
- Same-mistake rules: collection carriage and physical layout are independent
  facts; `value` must not be rewritten as `value-result`, and only value-result
  identities authorize copy-out. A type-family check must not silently trigger
  a narrower lifetime proof; seal and readiness consume one exact boundary
  predicate. Program-wide local ordinals come from the inventory, not fixture
  intuition. Finally, a phony build prerequisite must remain in the same make
  invocation as CI consumers or it will be rebuilt in each invocation.

## Last completed self-host context - logical ordered-record GraphPlan v29 closure

- Current HEAD is `90302b7a` on `main`; the worktree is intentionally
  uncommitted and contains the preceding Option<String>, Option<Bool>,
  Array<Int> value-result, and route-diagnostic work. The active production
  canary remains
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`,
  1,484 routines).
- Objective card: carry one callable-referenced ordered logical record
  `(Bool, Int, Bool, String)` through constructor, direct call, local storage,
  Bool control flow, and member reads without inventing a physical ABI.
  Priority is admitted declaration-field identity, exact instruction ABI
  absence, one program fact, target-neutral ordinal projection, C/LLVM value
  parity, negative ambiguity/layout ratchets, then production-canary progress.
  The fact owner is
  `DirectMirScalarProgramLogicalRecordFact`; last consumers are the C/LLVM
  preamble, signature, expression, and local-storage owners. Forbidden
  fallbacks are nominal spelling, field-shape-only selection, backend-local
  offsets, an unreferenced declaration candidate, native retry, or treating an
  absent layout as a zero-filled physical receipt.
- GraphPlan schema is
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v29`. The extension digest and
  readiness carry the logical record fact. C emits one bounded target-local
  value carrier and LLVM emits
  `%pgy.scalar.logical.record.value = type { i1, i64, i1, ptr }`; both obtain
  member ordinals from the ordered declaration fact and neither owns physical
  offsets. A Bool member is included in the existing non-trapping Bool proof,
  so `!record.valid || !record.target_found` remains admitted without weakening
  other call or expression kinds.
- The focused fixture declares an unused same-shape record, while only
  `ProbeFact` is referenced by callable signatures. Both targets compile and
  print exact `ok` and `3`. Field-order mutation, a forged instruction layout,
  and a second callable-referenced same-shape declaration are rejected without
  publishing an artifact. The installed focused gate exits 0 in 4.5 seconds.
- Canonical `make self-host-compiler` exits 0 in 573.7 seconds with zero errors
  and zero warnings. The installed `bin/pgy-self-driver.exe` is 5,281,949
  bytes, SHA-256
  `D784DF63105538B9D188C63760BF0A8A0380392A4614AD9BC3B76EB081BA203D`,
  mtime 2026-08-10 23:58:07 +09:00. The seven-gate GraphPlan aggregate exits
  0 in 27.2 seconds against that exact driver. The CI profile plus full
  component structural contract exits 0 in 182.8 seconds.
- CI/CD now has one canonical `SELFHOST_SCALAR_GRAPH_PLAN_GATE` dependency.
  It covers multi-routine scalar, Option<Int>, Option<String>, Option<Bool>,
  two-Int nominal, logical record, and Array<Int> value-result gates. The Linux
  exhaustive parity job invokes this aggregate explicitly, and
  `tests/self_host_ci_profile_smoke.sh` pins the exact Makefile/workflow edge so
  CI cannot silently fall back to the pre-v29 subset.
- The installed LLVM production canary passes the former routine 47
  `JsonArrayStringFact` logical-record boundary. It remains deliberately RED,
  now after 19.9 seconds at the next exact fail-closed receipt:
  `owner=callable-route-envelope stage=parameter-count routine=51
  name=JsonObjectFactTableSchema parameter=-1`. No artifact is published.
  This is progress evidence, not whole-program substitution completion.
- Next objective card: audit the zero-parameter non-entrypoint callable policy
  reached by `JsonObjectFactTableSchema() -> String`. The routine signature
  inventory and callable-route envelope are the fact/decision owners; the last
  consumers are the C/LLVM callable signature and direct-call owners.
  Forbidden fallbacks are a dummy parameter, skipping the unused routine,
  accepting all zero-parameter returns by spelling, or reopening source text.
  The smallest falsifier must carry an unreachable zero-parameter scalar
  callable through both targets and reject any mismatched direct-call edge.
- Same-mistake rule: logical declaration identity is not a physical layout
  receipt. Field order may authorize target-local value projection only while
  every persisted instruction explicitly proves ABI absence; it never
  authorizes copied offsets, `offsetof`, or native-layout inference. An unused
  same-shape declaration is not a candidate, and two callable-referenced
  candidates are ambiguity, not permission to choose the first.

## Last completed self-host context - Option<Bool> GraphPlan v28 closure

- Current HEAD is `90302b7a` on `main`; this session is uncommitted and preserves
  the preceding Array<Int> value-result work. The active production canary is
  `.tmp/multi-routine-generalization/routine-index-fixture.mir.json`
  (35,814,796 bytes, 1,484 routines).
- Objective card: carry persisted `Option<Bool>` physical identity through
  scalar-program return, direct call, local storage, Some, IsSome,
  UnwrapOption, and the generic persisted `None` leaf.
  Priority is one MIR ABI receipt, contextual generic-builtin specialization,
  GraphPlan digest carriage, C/LLVM value parity, ABI mutation rejection, then
  production-canary progress. Fact owner is the admitted
  `DirectMirOptionMatchAbiFact` instance produced by
  `DirectMirScalarProgramOptionBoolAbiFromInstruction`; last consumers are
  the C/LLVM preamble, signature, expression, and local-storage owners.
  Forbidden fallbacks are Option<Int/String> specialization, treating
  `Option<Unknown>` as a layout fact, backend-local offsets, native retry, or
  a second builtin lookup.
- GraphPlan schema is
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v28`. The extension digest and
  readiness include the Option<Bool> fact. C emits an asserted 8-byte,
  4-byte-aligned tag-plus-bool aggregate; LLVM emits
  `%pgy.scalar.option.bool = type { i32, i1 }`. Locals use those exact types,
  not the previous unknown-type-to-Int default.
- Generic Some/IsSome/UnwrapOption call admission joins actual/expected types
  once and carries the selected return type. `None` is not a zero-argument call
  in persisted MIR; the absence owner accepts only text `None`, binding kind
  `none`, expected Option<Int/String/Bool>, and the producer's
  `Option<Unknown>` spelling. The focused fixture executes Some -> two direct
  returns -> local Option<Bool> -> IsSome -> UnwrapOption and also admits the
  nonexecuted None return. Both targets print exact `true`; a mutated payload
  offset is rejected with no artifact.
- Canonical `make self-host-compiler` exits 0 in 478.9 seconds with zero errors
  and zero warnings. The canonical CI aggregate rebuild plus all six GraphPlan
  focused gates exits 0 in 459.5 seconds. Installed
  `bin/pgy-self-driver.exe` is 5,256,163 bytes, SHA-256
  `D5DBEBA8595EACFFD5AAF2C9293695A025CB16F19387B40B58B6FD816C9EF083`,
  mtime 2026-08-10 22:34:38 +09:00. The final component inventory exits 0 in
  159.8 seconds and the CI profile gate exits 0 in 1.6 seconds.
- CI/CD's canonical `SELFHOST_SCALAR_GRAPH_PLAN_GATE` includes multi-routine,
  Option<Int>, Option<String>, Option<Bool>, two-Int nominal, and Array<Int>
  value-result. The Linux parity workflow consumes that aggregate and
  `tests/self_host_ci_profile_smoke.sh` pins the exact dependency.
- The installed routine-index canary remains deliberately RED but now passes
  the former routine 43 `JsonBoolValueOptWithin -> Option<Bool>`
  callable-envelope boundary. After 43.7 seconds it reports the next exact
  assessment: `owner=callable-route-envelope stage=return-type routine=47
  name=JsonArrayStringFactWithin type=JsonArrayStringFact`. Route-envelope
  progress and focused behavior evidence are recorded separately; neither is
  overstated as whole-program completion.
- Next objective card: audit the declaration and persisted instruction ABI for
  `JsonArrayStringFact`, identify whether it is one eligible nominal carrier or
  a broader aggregate family, and add no target representation until field
  identity plus required layout agree. Reuse the admitted declaration index;
  do not infer from the nominal spelling or widen the existing two-Int owner.
- The read-only audit fixes that boundary more narrowly. The declaration is the
  only ordered `(Bool, Int, Bool, String)` struct in the canary, but it has
  `abi_layout_id=0`, `abi_layout_required=false`, and no physical layout row.
  Routine 47 constructs it; routines 48-49 immediately carry it through a
  direct call/local and read `valid`, `target_found`, `target_value`, and
  `count` through persisted member-access nodes. Therefore the next coherent
  seam is a logical ordered-record identity fact plus constructor/member
  projection, or an upstream producer ABI receipt if byte layout becomes an
  interoperability boundary. Copying C/LLVM offsets, accepting the spelling,
  or merely widening the callable return predicate is forbidden.
- Same-mistake rule: `None`'s `Option<Unknown>` is contextual absence, never a
  physical ABI receipt. A real Option value still requires its exact MIR ABI
  row, expression result type, callable return, local storage, and target
  aggregate to agree; an unknown local type must never silently become Int.

## Last completed self-host context - two-Int nominal GraphPlan representation

- Current HEAD is `f94b550d` on `main`; this session remains uncommitted and
  shares the larger installed capability/machine/AST/DIR closure below. Preserve
  the separate user-owned stdlib work: modified
  `docs/138_standard_library_scope.md` and
  `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`.
- Objective card: carry one physically admitted two-`Int` nominal value through
  the arbitrary routine GraphPlan. Priority is declaration field identity,
  exact required ABI row, complete routine signature/return admission, then
  C/LLVM execution. The derived fact owner is
  `DirectMirScalarProgramTwoIntNominalAbiFact`; last consumers are the target
  projection and C/LLVM nominal preamble, signature, and return owners.
  Forbidden fallbacks are nominal-spelling layout inference, copied offsets,
  routine-count routing, unused-routine deletion, declaration-table flattening
  per routine, source rescan, or native retry.
- The shared route now carries an optional one-candidate nominal fact beside
  the existing scalar and Option facts. Its owner scans the admitted declaration
  index once and selects exactly zero or one struct whose canonical declaration
  row and required MIR ABI prove two ordered `Int` fields. Unsupported unrelated
  declarations remain non-facts; two eligible candidates are ambiguous and
  fail closed. Matching formal parameters and every matching instruction ABI
  row must equal the selected receipt, and a selected fact that no signature
  references is rejected.
- C and LLVM consume one target-neutral projection carrying the selected target
  capability fingerprint. C emits size, alignment, and field-offset assertions
  from the receipt. LLVM emits `%pgy.scalar.nominal.value = type { i32, i32 }`
  only after the same shape proof. GraphPlan schema is now
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v25`.
- The focused four-routine fixture leaves `Keep(Pair) -> Pair` unreachable from
  `Main`, declares an unrelated `Metadata`, and still prints exact `11`. This
  forces both backends to compile the nominal signature without treating all
  declarations as representations. A five-routine negative actually references
  `Metadata` and is rejected by both targets without an artifact. The final
  focused gate exits 0 in 5.5 seconds and also rejects the second field offset
  mutation `4 -> 0`. The positive MIR is 8,473 bytes, SHA-256
  `D0C8B63A5ABA4A85B51673B847F2D2F29CC14A2A54DF13C690C6D0C22F95E186`.
- Canonical `make self-host-compiler` exits 0 in 483.4 seconds with zero errors
  and zero warnings. The installed `bin/pgy-self-driver.exe` is 5,193,688
  bytes, SHA-256
  `12F9FB52BF218DF10BB3A292786E18C6EDD06DBC84824CE6B89D5EE013EE847A`.
  The arbitrary scalar and Option regression gates exit 0 in 4.2 and 4.6
  seconds. The component inventory exits 0 in 152.8 seconds. The SoT gate exits
  0 with 86 authorities and 160 derived fact carriers
  (`CLOSED=49`, `BRIDGE=36`, `ACTIVE=1`).
- The installed LLVM routine-index canary remains intentionally RED after 50.7
  seconds at the unchanged
  `direct MIR terminal multi-routine graph is unsupported` boundary, with no
  native fallback. Its exact 35,814,796-byte MIR (SHA-256
  `86C6DF4B58F6C32152CB0759C2EDE8CD2DD8913670C1577D4A00652623A574DF`)
  contains 1,484 routines and 88 declarations. The nominal selector now returns
  its canonical empty fact because none is a two-`Int` candidate. A static
  route-policy audit identifies the first subsequent rejection as routine 12,
  `ReadJsonStringBounded(String, Int, Int, value-result Array<Int>) -> String`;
  the external canary diagnostic does not yet expose that row.
- Next falsifier: give the existing Array storage ABI a callable value-result
  boundary receipt and close copy-in/copy-out for the exact four-parameter
  `ReadJsonStringBounded` shape, or add a diagnostic receipt that proves a
  smaller preceding failure. Do not broaden the route without the Array ABI,
  copy declarations or parameter layouts per routine, accept
  `declaration_count > 0`, skip unreachable routines, add
  `routine_count == 1484`, or replace the installed LLVM substitution canary
  with a native oracle.
- Same-mistake rule: a successful physical carrier proves only the exact row it
  admitted. It does not legalize another nominal spelling, another layout, or
  a whole declaration table. Register a real `*_fact_owner.pgy` in the derived
  SoT inventory, carry it once, cross-seal every producer, make both targets
  consume its fingerprinted projection, and keep a persisted-layout mutation
  plus an unused-routine positive case in the executable gate. An unrelated
  declaration may remain unmaterialized only while no signature, local, or
  instruction ABI references it; the referenced-unsupported negative pins that
  distinction.

## Last completed self-host context - Option<Int> GraphPlan representation

- Current HEAD is `f94b550d` on `main`; this session remains uncommitted and
  shares the larger installed capability/machine/AST/DIR closure below. Preserve
  the separate user-owned stdlib work: modified
  `docs/138_standard_library_scope.md` and
  `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`.
- Objective card: carry `Option<Int>` value returns and nested direct calls
  through the arbitrary routine GraphPlan. Priority is persisted ABI identity,
  typed call/expression readiness, one program-wide receipt, then exact C/LLVM
  execution. The fact owner is the required MIR ABI row projected as
  `DirectMirOptionMatchAbiFact`; last consumers are the GraphPlan C/LLVM type,
  signature, expression, and preamble owners. Forbidden fallbacks are a
  type-name-only `{tag,value}` layout, copied offsets/tags, routine-count
  routing, source rescan, or native retry.
- `DirectMirScalarProgramOptionIntAbiFromInstruction` admits every reached
  `Option<Int>` instruction layout and rejects missing or disagreeing receipts.
  The existing extension digest carries that one fact. Some/None/IsSome/
  UnwrapOption identities come from the semantic builtin registry projection,
  and direct-call readiness now consumes the existing callable return-type
  policy instead of reapplying a scalar-only classifier.
- C and LLVM both project from `DirectMirOptionMatchAbiProjection`. C emits the
  admitted two-field carrier and helpers; LLVM emits the admitted aggregate,
  field indices, tags, and print-width extension. Neither target owns an
  independent Option layout. GraphPlan schema is now
  `pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v24`.
- The focused fixture is `Main -> Extract -> Relay -> Wrap`: two routines return
  `Option<Int>`, the caller unwraps the nested direct call, and both target
  artifacts compile and print exact `11`. The canonical target
  `make self-host-direct-mir-scalar-option-int-test-smoke` exits 0 in 440.9
  seconds including a full self-host rebuild. A value-field offset mutation
  from `4` to `0` is rejected by both backends without publishing an artifact.
  The direct Option gate and the prior arbitrary scalar multi-routine gate both
  exit 0 together in 5.8 seconds against the installed driver.
- The current exact-source installed carrier is `bin/pgy-self-driver.exe`,
  5,181,952 bytes, SHA-256
  `C9C9D85D262C53DBEB7BDD7AE9A1DCC875A6F1B36AD7169789950618C055040B`.
  The canonical build reports zero errors and zero warnings. The final full
  component inventory exits 0 in 128.6 seconds.
- The installed LLVM routine-index owner canary was rerun after this closure.
  Its C executable leg still compiles and prints the expected owner result; the
  installed LLVM leg remains RED after 43.9 seconds at the unchanged
  `direct MIR terminal multi-routine graph is unsupported` boundary. This is
  expected evidence, not a hidden pass: the exact 1,484-routine artifact still
  contains 88 nominal declarations, `Option<String>`, Array parameters, and
  other representation families outside this declaration-free Option rung.
- Next falsifier: identify the first representation fact required before that
  general artifact can enter the shared routine inventory. Keep the installed
  LLVM leg as the substitution canary; do not weaken the declaration guard,
  add a `routine_count == 1484` route, or replace it with a native LLVM oracle.
- Same-mistake rule: generic surface spelling is not physical ABI. A backend
  `{i32,i32}` literal or C `{tag,value}` typedef without a required MIR layout
  receipt repeats dual authority. Capture once, compare all producers, carry
  one receipt, project both targets, and mutate the persisted layout in the
  negative gate. Also do not let a post-admission scalar-only check override the
  callable return policy.

## Last completed self-host context - arbitrary scalar multi-routine legalization

- Current HEAD is `f94b550d` on `main`; this session remains uncommitted and
  shares the larger installed capability/machine/AST/DIR closure below. Preserve
  the separate user-owned stdlib work: modified
  `docs/138_standard_library_scope.md` and
  `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`.
- Objective card: legalize declaration-free scalar programs from admitted
  routine identity and typed call edges at any routine count; priority is exact
  callable syntax identity, ordered parameter/return types, one Main-first
  partition, C/LLVM parity, then old count-classifier removal. Fact owners are
  `MirProgramRoutineIndex` and expression-graph call-target syntax IDs; the
  derived owner is `DirectMirScalarCfgProgramCallableInventory`; last consumers
  are the scalar GraphPlan C/LLVM program emitters. Forbidden fallbacks are
  `routine_count == N` routing in this scalar family, display-name lookup,
  source/AST rescan, fabricated local/value rows, and native retry.
- `DirectMirScalarProgramRouteFact` now carries every exact admitted routine row
  in canonical Main-first order. The callable inventory seals every non-Main
  routine's ordinal, source syntax ID, name, return type, ordered parameter
  range/types, and signature digest once. Direct-call admission joins the
  persisted target syntax ID to that inventory. Per-routine GraphPlan admission,
  routine identity, partition ranges, expression readiness, and C/LLVM emitters
  now iterate the admitted inventory rather than selecting one optional
  callable.
- Program `Log` expressions infer their already-admitted Int or String root type
  and consume the matching runtime format. Pure parameter/literal/direct-call
  graphs may legitimately have zero CFG locals and zero SSA definition rows;
  `DirectMirScalarCfgMinimumPlanShapeReady` now requires the real program
  invariant (at least one block) instead of forcing dummy storage.
- The focused fixture is a four-routine chain
  `Main -> Top -> Middle -> Leaf`. The canonical target
  `make self-host-direct-mir-scalar-multi-routine-test-smoke` exits 0 in 439.1
  seconds including a full self-host compiler rebuild. C and LLVM artifacts both
  compile and print exact `10`; missing call-target syntax identity and duplicate
  routine identity are rejected by both targets without publishing an artifact.
  The direct focused script also exits 0 in 3.5 seconds against the installed
  driver.
- The current exact-source installed carrier is
  `bin/pgy-self-driver.exe`, 5,164,099 bytes, SHA-256
  `3111639093301ABFDC887ABC2CC64FD76A132209DAB42B2CCA4C28CBCF6D231B`.
  The build reports zero errors and zero warnings. The full component inventory
  exits 0 in 143.3 seconds. The shared scalar-owner cap table remains the single
  cap SoT: route 110/110, per-routine admission 260/260, routine identity 75/75,
  and the new callable inventory 164/180.
- The prior `self-host-mir-program-routine-index-owner-test-smoke` is still RED,
  and is not reported as closed. Its C executable leg succeeds, but the installed
  LLVM leg exits 2 after 46.5 seconds at
  `direct MIR terminal multi-routine graph is unsupported`. The exact produced
  MIR contains 1,484 functions and 88 declarations (87 structs, one enum), with
  non-scalar types such as `Option<Int>`. That is a different general typed/
  nominal program-representation rung; weakening the scalar declaration-free
  guard would skip required ABI facts. The legacy terminal `routine_count == 3`
  branch is likewise OPEN outside this scalar owner family.
- A read-only representation census of that exact 35,814,796-byte MIR fixes the
  boundary more precisely. Of 1,484 routines, 862 have scalar-only parameter and
  return signatures, but only 782 remain scalar-only after source-local and
  instruction ABI types are included. The remaining inventory includes 56
  `Option<Int>` returns, 54 `Option<String>` returns, 120 `Array<Int>` parameters,
  59 `Array<String>` parameters, and 87 distinct struct declarations. This is
  evidence against relaxing the declaration-free route: the missing owner is a
  typed program representation plan, not another cardinality case.
- Gate audit: keep the installed LLVM leg RED. The public `--backend=llvm`
  contract deliberately delegates to the installed self-host driver, so adding
  `--native-pipeline` would replace the substitution canary with an independent
  native oracle and weaken the gate. The routine-index fixture still owns its
  runtime result, while this current failure occurs earlier in the whole-program
  direct-MIR representation route. Record those as two distinct claims even
  though the executable gate carries both.
- Next falsifier: inventory the 1,484-routine artifact by admitted return/
  parameter representation and direct-call edge, then choose the first smallest
  unsupported typed family whose representation facts already exist. Migrate
  that family to one routine-inventory/representation plan and rerun the same
  routine-index LLVM leg. Do not add a `count == 1484` route, accept declarations
  without representation receipts, or replace installed LLVM with a native LLVM
  oracle merely to turn the gate green.
- Same-mistake rule: routine cardinality is not semantic program shape. A new
  `count == 3`, `count == 4`, or fixture-name branch repeats the defect. Use the
  routine inventory and carried call-target IDs; keep one catalog and one
  partition; let missing or duplicate identities fail closed. A shape gate must
  validate owned facts, not demand fake locals/SSA rows. Structural inventory
  cannot claim behavior: the four-routine execution/negative gate owns this
  rung, while the 1,484-routine installed LLVM RED remains explicit evidence.

## Last completed self-host context - public DIR installed substitution

- Current HEAD is `f94b550d` on `main`; this session remains uncommitted and
  shares the larger installed capability/machine/AST closure below. Preserve
  the separate user-owned stdlib work: modified
  `docs/138_standard_library_scope.md` and
  `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`.
- Objective card: make public `pgy --dir` consume the installed Pergyra DIR
  fact owner and text sink; preserve exact participant, ordered-step, syntax,
  and explicit-versus-derived provenance; forbid native retry, public-oracle
  self-comparison, AST provenance rescan, renderer default re-inference,
  count-to-row reconstruction, and cross-producer numeric source-ID equality.
- `src/self_hosted/dir/domain_graph_inventory_owner.pgy` now materializes the
  program inventory once from admitted declaration, role, party/roster,
  authority, zone-state, topology, and intent carriers. It preserves native
  edge order for effect/relation projection contracts and resolves topology
  owners across effect, relation, and zone declarations. Normal MIR domain
  production does not import or rebuild this debug inventory.
- `src/self_hosted/dir/zone_state_row_fact_owner.pgy` decodes the parser-owned
  state payload once at the DIR fact boundary and joins the state, layer slot,
  and participant slots to exact declaration identities. State rows are not
  also admitted as runtime-topology directives. The self parser now accepts
  the native grammar's semicolon-optional state line; its previous mandatory
  `Expect(";")` was an uncovered compatibility bug.
- `src/self_hosted/compiler/dir_text_artifact_owner.pgy` is the last renderer.
  It compares the independent domain census when that census is present, but
  renders only admitted inventory rows. Intent detail is delegated to
  `dir_intent_text_artifact_owner.pgy`, which consumes typed provenance rows
  instead of re-inferring defaults from resolved names. `--emit-dir-verified`
  is admitted by the installed driver's internal CLI. Public `pgy --dir` now
  delegates to that request; `--native-pipeline --dir` is the one independent
  native oracle.
- `tests/self_hosted/parity/dir_graph_inventory_owner.sh` most recently exits
  0 in 61.4 seconds with zero errors and zero warnings. Native and self-host
  rows match for
  `function_clause_order_minimal`, `party_role_bind`, the relation/effect
  refresh/publish/apply/link topology fixture, and a semicolon-optional
  zone-state fixture containing state rows both with and without `;`, plus a
  zero-step participant intent, a fully explicit step, and an action-default
  step, intent-level `who`/`where` defaults, derived and explicit transfer
  steps, the composite inline sub-intent example, and the established
  `on: NestedIntent(...)` spelling. The default-bearing
  compact AST is also byte-equal after the standard
  terminal-newline normalization. The
  comparator normalizes only producer-local
  node/topology source syntax fields; node indexes, edge
  `from`/`resolved`, kinds, names, labels, targets, ordering, and topology
  owners remain byte-significant. Count-preserving edge-kind and intent-
  provenance mutations fail.
- `tests/self_hosted_component_contract_smoke.sh` exits 0 in 137.2 seconds with
  the provenance/intent renderer owners, exact line caps, no-provenance-rescan
  rule, renderer/CLI boundaries, and removed-path ratchets.
- `tests/sot_authority_edge_smoke.sh` exits 0 with 86 authorities and 159
  derived fact carriers (`CLOSED=49`, `BRIDGE=36`, `ACTIVE=1`). The inventory
  bridge and text-artifact consumer remain last consumers of
  `dir.domain_graph`; they are not mislabeled as `*_fact_owner.pgy` derived
  carriers.
- Canonical `make self-host-compiler` most recently exits 0 in 448.2 seconds
  with zero errors and zero warnings. It installed a 5,156,760-byte
  `bin/pgy-self-driver.exe` with SHA-256
  `A08140CDF9F7806E828F0F4F54B08F8AFDCC4FB92A1379E75B05307430DB6A4A`.
  `tests/self_hosted/parity/public_dir_installed_self_host_owner.sh` then exits
  0 in 3.3 seconds: public bytes equal direct installed bytes, while the
  normalized installed rows equal the explicit native oracle for authority,
  defaults, transfer shorthand, inline `intent:`, and nested `on:` intent
  inputs. Missing-driver, unsupported-option, and installed-arity negatives
  produce no partial DIR artifact and do not retry the native pipeline.
- The first canonical `make -j4` exposed an adjacent stale-link guard bug:
  `pgy-lsp` linked the MIR nominal-ABI consumer but omitted the producer object
  that owns `mir_decl_header_storage_layout_matches`. The LSP target now takes
  the complete `MIR_CORE_OBJECTS` closure instead of maintaining a second
  partial MIR link inventory. `bin/pgy-lsp.exe` relinked successfully in 2.8
  seconds and a following full `make -j4` exits 0 with all targets current.
- Exact participant rows, explicit steps, action/intent-default provenance,
  participant-alias transfer carriage, and inline sub-intents are now closed on the internal
  request. Intent defaults are parser-owned, emitted as appended compact AST
  kind 88, and consumed as a typed clause row. Inline targets use appended kind
  89 and one target-kind-neutral expression-node carrier. A semantic intent
  target may carry kind 89 from `intent:` or the existing `On` row from
  `on: NestedIntent(...)`; action targets remain `On`-only. Transfer shorthand
  is exact, so public `pgy --dir` is SUBSTITUTING. Wrong zone-state endpoints
  continue to fail in the typed row owner and the independent native oracle.
- Same-mistake rule: never promote a graph census, hash, or anchor into row
  evidence. Never normalize all numeric fields in a differential test. Source
  syntax IDs are producer-epoch identities; DIR-local node and resolved-edge
  IDs are artifact-internal identities and must compare exactly. Syntax
  compatibility gates must execute optional spellings such as state lines with
  and without `;`; pinning one parser branch is not cross-parser evidence.
  A resolved step value does not prove how it was obtained: provenance belongs
  to a typed row and must not be reconstructed in the text renderer. A carried
  declaration index must resolve through its exact DIR kind, never through an
  assumed catch-all `type` node. Before adding a later-IR syntax parser, search
  semantic row owners first: `SemanticAstIntentStepHeaderFromText` is the one
  header parser consumed by semantic, codegen, and DIR. Do not register ordinary
  parser/row owners as derived fact carriers merely because they carry data;
  the registry's classified `*_fact_owner.pgy` convention is executable. Do
  not derive one AST tag from a semantic target kind: validate the exact carried
  `on:` or `intent:` node. After public delegation, a gate claiming a native
  DIR oracle must spell `--native-pipeline --dir`; plain public `--dir` is the
  subject under test and comparing it to itself is forbidden. A binary that
  links a MIR consumer must link the producer-owned ABI/layout receipt through
  the canonical MIR object closure; do not hand-maintain a smaller duplicate
  list that can omit the fail-closed guard after a header change.

## Last completed installed context - public capability manifest is installed Pergyra

- Current HEAD is `f94b550d` on `main`; the verified executable code checkpoint
  remains `37624de9`. The current session has an uncommitted SoT/gate closure
  described below. Preserve the separate
  user-owned stdlib work: modified `docs/138_standard_library_scope.md` and
  `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`.
- Exact public `pgy --capability-manifest <source>` now delegates to the
  installed Pergyra driver and never reaches the launcher's final native
  pipeline. `DriverCliSourceCapabilityManifestStdout` owns the request;
  `SemanticAstCapabilityFactsFromAdmittedBody` owns callable declared/direct/
  transitive masks; `CompileSourceCapabilityManifestVerified` is the last
  semantic-to-JSON consumer.
- `src/semantic/builtin_capability_registry.def`,
  `src/runtime/pgy_file_mode_capability.def`, and the callable-contract
  vocabulary are the policy inputs shared by native and generated self-host
  projections. The manifest renderer consumes the admitted program mask and
  does not rescan builtin spellings or treat declarations as usage.
- Clean, declared-ok, interprocedural under-declaration, `FileOpen` read/write/
  read-write/dynamic modes, missing sibling, installed arity, and rejected
  option combinations are executable negatives. The explicit independent
  oracle is `--native-pipeline --capability-manifest`; public-vs-public is not
  accepted as parity evidence.
- Exact public `pgy --machine-manifest-json` no longer reaches the launcher's
  final native pipeline. The native serializer remains the sole physical
  declaration producer during `self-host-compiler`; its immutable output is
  hashed into the driver build key and installed beside the Pergyra-built
  driver as `pgy-self-driver.machine-layer-manifest.json`.
- The launcher resolves that companion from the selected installed driver,
  invokes only `--emit-machine-manifest-verified`, and grants absolute-path I/O
  only to that exact delegated child request. The self-host declaration
  consumer parses the existing `pgy.machine-layer.declaration.v1` artifact,
  applies its full ready check, then returns the original payload. It owns no
  host-sim literal and no JSON serializer.
- Missing companion, invalid companion, internal arity drift, and unsupported
  public option combinations all fail without native retry. The public gate
  also runs from a different working directory so companion resolution cannot
  silently depend on the repository root.
- Exact public `pgy --ast <source>` no longer reaches the final native
  `driver_run_pipeline` fallback. `driver_self_host_source_read_mode` admits
  only one exact token/AST read-only request, the sibling receives `--ast`,
  `DriverRung2CliRequestFromArgsOrDie` assigns `DriverCliSourceAstStdout`, and
  `DriverRung2ExecuteReadRequest` consumes it through import-composed
  `ParseRootProgram`.
- Unsupported AST option combinations fail at the launcher selector. A missing
  installed sibling fails without native timing or retry. The installed request
  rejects missing source arity through its existing `Die` contract. No AST text
  or import policy is reconstructed in the launcher or sibling adapter.
- The current installed Pergyra-built carrier is `bin/pgy-self-driver.exe`,
  5,091,027 bytes, SHA-256
  `5DB8BCCC942A012A2D28B61EFF43353208FC0ECEC102D36B62A90E4BDB5BF1FA`.
  The 1,144-byte companion has SHA-256
  `0A83B0DB5EFE3C00C6D9413C63045C4B17AFF079781213B280442C588E5A9C19`.
  The latest canonical `make self-host-compiler` rebuild completed in 410.5
  seconds with zero errors and zero warnings. The matching native launcher is
  4,640,318 bytes, SHA-256
  `A5824D4929FB75CCA53B0188F79E033E98E9B553918EBCFAA02C560A09ABFE4D`.
- `tests/self_hosted/parity/public_machine_manifest_installed_self_host_owner.sh`
  exits 0 in 2.2 seconds. Installed direct, public, changed-working-directory,
  companion, and explicit native-oracle bytes are equal. Its first build smoke
  exposed Windows CRLF being passed through text-mode `Log` as CRCRLF; the
  consumer now removes only the already-owned CR before the text stream emits
  exactly one CRLF.
- `tests/self_hosted/parity/public_ast_installed_self_host_owner.sh` exits 0 in
  2.2 seconds. Direct/public AST bytes equal the committed arithmetic fixture
  and the explicit native oracle. The imported-source leg also equals the
  native artifact and retains `Intent: ImportedFrontendPipeline`, preventing a
  root-only parser from falsely passing.
- `tests/self_hosted/parity/parser_parity.sh` exits 0 in 324.8 seconds for all
  189 manifest rows on C and LLVM. Every native reference now uses explicit
  `pgy --native-pipeline --ast`; using public `pgy --ast` there would execute
  the installed candidate twice. Imported intent composition, generic default
  contracts, and language-word registry parser gates pass in 1.9, 128.7, and
  121.8 seconds respectively.
- The public token regression remains green in 3.0 seconds. The installed CLI
  mode gate now exits 0 in 2.7 seconds, and the current full component inventory
  exits 0 in 125.4 seconds with the manifest selector, companion, framing, and
  removed-native-fallback ratchets.
- Classification is bounded `SUBSTITUTING` for public capability-manifest
  semantics, public machine-manifest delivery, and public AST debug output. It
  does not promote the default source compilation pipeline or the whole
  compiler. Package execution and the earlier installed token, C, MIR, and
  runtime-free LLVM boundaries remain closed at their named gates.
- Same-mistake rule: once a public source-read selector delegates to the
  installed candidate, that public spelling cannot remain the independent
  oracle. Token and AST native voices must use `--native-pipeline`; the
  installed voice uses the direct sibling/public selector, and the gate compares
  the two artifacts explicitly. Import composition must be exercised when the
  public contract includes it; a root-only fixture is insufficient.
- Documentation quality, self-host progress metric, and substitution velocity
  gates passed at the committed checkpoint. The current session corrected the
  intent-observability receipt to state its executed non-positional identity
  and parameter-shape negatives; the focused registry gate exits 0. It also
  classifies `MirProgramRoutineBlockCaptureWithin` as a `mir.execution_graph`
  `local_view`, rather than inventing a new authority.
- The SoT edge gate now exits 0 with 86 authorities and 157 derived carriers.
  Its next failure exposed an observed-codegen dual bridge: the pressure CLI
  called `AstTreeArtifactFromText` directly beside the normal
  `program_entry_owner` path. `CodegenAstArtifactFromTextObserved` now owns the
  artifact markers and the only codegen-side AST-text constructor call; normal,
  check, and pressure paths all consume that owner. The semantic environment
  lifetime gate exits 0 in 16.9 seconds.
- The modified full component inventory is now green in 125.4 seconds. The
  entry owner remains exactly 100 lines, its focused source assertions pass
  through the SoT and semantic-lifetime gates. SoT adequacy remains unverified
  because this environment has no Coq/Rocq prover; no missing-prover skip is
  claimed.
- The current import-composed codegen source produces a 3,703,230-byte AST with
  SHA-256
  `3456BD632885C587B2F5F14373A469E33EACBA8549A23A43E2B23E7646E364F9`.
  The existing independent Pergyra-built codegen checker
  (`gen2.exe`, 2,365,804 bytes,
  `EF925958AB9C93016BED61C54030CA4F68DF5402F7B1E641CEC7A73948EDB0C3`)
  accepts its surface, event scan, and shape and reports `Status: ok`. This is a
  source syntax/type/import-composition oracle only; the checker predates the
  adapter change and is not evidence that a rebuilt observed CLI executed it.
- Same-mistake rule: an observation-only CLI may select an observed adapter but
  may not reproduce the artifact/semantic bridge in its orchestration owner.
  Gate receipts must name the negatives actually executed, and every new
  `*_fact_owner.pgy` must be registered immediately as an authority or a named
  derivative of one; documentation wording, file naming, or a green neighboring
  gate cannot substitute for that classification.
- Same-mistake rule for AST carriers invoked across PowerShell and Git Bash:
  pass the Bash script as one argument and preserve its exact single-quoted
  `tr -d '\r'`. Do not inject backslash-escaped quote characters around that
  set. In this session the malformed cross-shell command passed those quote
  bytes literally, made `tr` delete ordinary `r` bytes (`Program` became
  `Pogam`), and caused a silent exit-1 check. The repository script's exact
  form is byte-equal to `dos2unix`; the unchanged raw artifact passed after
  canonical invocation.
- Same-mistake rule for installed immutable companions: generate the payload
  only at its declared authority, include its hash in the installed build key,
  package it beside the selected binary, validate it through the existing
  consumer, and replay the admitted bytes. Do not copy physical literals into
  the self-host owner, rebuild JSON from the parsed view, accept a missing
  companion, or retry the native pipeline after installed failure.
- Same-mistake rule for cross-platform text artifacts: a stored CRLF already
  owns a CR. Passing that raw pair to a Windows text stream makes the LF emit a
  second CR. Remove only the transport-owned CR immediately before the text
  sink and require exact oracle bytes; do not broadly trim or normalize JSON
  content to conceal a framing error.
- The component-gate audit found two silent source-inventory weaknesses. Its
  shared function extractor stopped only at the next Pergyra `func`, so a C
  function check could read to EOF; one `reject_function_text` call also passed
  two forbidden terms even though the helper consumed only one. The extractor
  now stops at the outer column-zero closing brace for both Pergyra and C, the
  multi-term call uses `reject_function_terms`, and the static-call identity
  check uses that shared extractor. The next C function deliberately contains
  `lookup_typed_var` and `is_slot_var`, so the green component gate falsifies an
  EOF-wide extraction. Shell syntax, fixed-arity call census, and the full
  component inventory are green; the latest standalone run exits 0 in 126.0
  seconds.
- Same-mistake rule for structural function gates: never pass surplus terms to
  a single-term helper, never delimit a C body by the next Pergyra declaration,
  and never treat whole-file presence as function ownership. Use the explicit
  multi-term wrapper and keep a following-function-only negative as the scope
  falsifier. Behavioral correctness remains in the focused executable gate.
- The same outer-brace boundary now covers the hard-contract helper and the
  focused expression-graph, resource-graph, JSON-writer lifetime, semantic
  function-table, simple-statement graph-use, and routine-index source checks.
  The first six focused checks plus the hard contract are green. The
  routine-index C executable leg is green, but its installed LLVM leg remains
  RED at the pre-existing `direct MIR terminal multi-routine graph is
  unsupported` projector boundary; this gate is not reported as passed and the
  extractor cleanup is not evidence that the LLVM blocker closed.
- Completed objective card: objective = replace the source-specific public
  `--capability-manifest` fallback with admitted Pergyra semantic facts;
  priority = exact capability identity and declared-vs-used semantics, one
  builtin/file-mode policy projection, installed public reach, no-fallback
  negative, then output breadth; fact owners =
  `semantic.builtin_capability_policy`,
  `semantic.file_mode_capability_policy`, and
  `selfhost.source_capability_facts`; last legitimate consumer =
  `CompileSourceCapabilityManifestVerified`; forbidden fallback = fixed
  installation companion, declared-as-used, renderer builtin scan, public
  self-oracle, or native retry. All named focused falsifiers are green.
- The first direct installed run exposed two facts that the static design alone
  did not prove. `Now` was present in the native builtin inventory but absent
  from the self-host signature rows, and a `Call` node is not itself the
  `CallArgument` spine needed to classify `FileOpen` mode. Both now have owned
  projections and negative gates. A new ambient builtin or call view cannot be
  added on only one side and still pass the registry/public gates.
- Capability verification evidence: both registry generators pass `--check`;
  `tests/builtin_capability_registry_smoke.sh` exits 0 in 2.7 seconds;
  `tests/self_hosted/parity/public_capability_manifest_installed_self_host_owner.sh`
  exits 0 in 4.9 seconds; `tests/capability/run_manifest.sh` reports zero
  failures; public token, public AST, and installed CLI gates remain green; the
  full component inventory exits 0 in 126.7 seconds.
- The first documentation-quality run exposed one stale positive gate:
  `object_action_boundary_contract_smoke.sh` still required the retired direct
  `PGY_CAP_IO_WRITE` recording call. It now requires the canonical builtin
  lookup, keeps the mode projection, and explicitly rejects the direct mask.
  The complete documentation-quality target then exits 0 in 7.3 seconds.
- Same-mistake rule: source-derived semantic output cannot use an immutable
  companion. Capability declarations are constraints, not usage facts; the
  renderer cannot rediscover builtins; native parity must use the explicit
  `--native-pipeline`; and call-target identity must be joined to the exact
  argument-spine view before a literal-sensitive policy such as `FileOpen` is
  applied. Missing sibling or malformed options fail once, without fallback.
- Same-mistake rule for migration gates: when a direct read is retired, search
  for gates that require it as well as production consumers that call it. A
  green legacy positive can force the old authority back into the tree just as
  effectively as a fallback implementation. Replace that positive with the new
  owner requirement and an explicit negative for the retired spelling.
- Next objective card: inspect the remaining final native-fallback request at
  `src/pgy_driver.c` only after the current ownership/document gates remain
  green; choose one production-reachable public spelling, name its existing
  fact owner and last consumer, then add an explicit independent oracle and a
  missing-installed-owner negative. Do not broaden this capability closure into
  default source compilation or infer the next owner from option proximity.

## Historical checkpoint - current-source gen2/gen3 fixed point is closed

- Verified source checkpoint is `59d83a24` on `main`. Preserve the separate
  user-owned stdlib work: modified `docs/138_standard_library_scope.md` and
  `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`.
- Active production entry is the Pergyra-built full MIR producer and consumer.
  The current-source MIR is
  `.tmp/self_hosted/compiler/preseal-index-admitted-20260810/driver_source.focused.mir.json`,
  exactly 184,436,842 bytes with SHA-256
  `48F725A4F94940BC05C38FCB30DB5ACA8AEDBD3851D4CD472DD54D3013610981`.
  Its producer exits 0 in 76.599 seconds at 2.968 GiB peak private. The 2.4 GiB
  attention threshold is crossed, but the unchanged 3 GiB cap is not.
- Closed owner seam: `CodegenTypeEnv` preserves the original admitted global
  index, an ordered declaration/role/operator program delta, and function-local
  rows as three distinct lifetime layers. Declaration facts carry the current
  environment, runtime/operator producers append exact suffix rows, and the
  scheduler seals the program delta once after the last producer. Function and
  intent owners append only local rows.
- Lookup order is original global, then first program-delta row, then local.
  The sealed program index is consumed through admitted immutable lookup. It
  must not use the wrapper that recomputes `StringLength(preseal_rows)` for each
  query after exact length was already admitted.
- Two falsifiers document the same-mistake boundary. Storing program rows in the
  local layer let function-local construction overwrite them. Building an index
  but retaining per-query row-length validation removed role cost yet left
  definitions above 33 seconds and timed out. Both paths are now negative-gated:
  never combine the global serialization again, never copy program rows into
  every function, and never collapse program-global and local lifetimes.
- Focused evidence: `codegen_type_env_preseal_epoch_owner.sh` exits 0 in 4.6
  seconds for installed self-host C and the independent native LLVM oracle.
  `codegen_role_receiver_admission_owner.sh` passes exact base/metamorphic
  execution and its negative admission cases. The monolithic component
  inventory emitted no diagnostic but did not finish within its 60-second
  static budget, so it remains unverified rather than green.
- Gen2 pressure receipt
  `self-host-driver-preseal-index-admitted-gen2-20260810` exits 0 in 264.403
  seconds at 2.185 GiB peak private and publishes
  `.tmp/self_hosted/compiler/preseal-index-admitted-20260810/driver_gen2.c`,
  exactly 8,778,318 bytes with SHA-256
  `65BE9045D2990E66ABF61EF82FC53FAEFE8F01741F12D75167E0387879F8BB04`.
  Type declarations take 1.906 seconds, role dispatch 1 millisecond, base-env
  sealing 0.567 seconds, runtime usage 0.849 seconds, and definitions 3.107
  seconds.
- Gen3 pressure receipt
  `self-host-driver-preseal-index-admitted-gen3-20260810` exits 0 in 271.920
  seconds at 2.199 GiB peak private and publishes the same 8,778,318 bytes and
  SHA-256. Direct byte equality is true. Type declarations take 1.857 seconds,
  role dispatch 1 millisecond, base-env sealing 0.572 seconds, runtime usage
  0.850 seconds, and definitions 2.731 seconds. Current-source gen2 equals gen3
  under the unchanged 3 GiB/300-second boundary.
- Objective card for the next rung: objective = promote this exact fixed-point
  source through the canonical release/installed-driver build owner; priority =
  source and artifact identity, installed production reachability, old-carrier
  deletion, negative ratchet, then broader CI; fact owner = the fixed-point C
  receipt above plus the canonical self-host compiler build manifest; last
  legitimate consumer = the installed driver executing the named production
  slice; forbidden fallback = treating the manual O2 measurement carrier as a
  released substitute, stale MIR reuse, AST-built final substitution,
  timeout/cap increase, cache/shard/worker, or accepting hash drift. The next
  falsifier is a canonical release build from `59d83a24`, followed by the
  installed production slice and exact artifact/hash receipt. Full CI and
  released-driver promotion are not yet claimed.

## Historical checkpoint - prepatch current-source gen2 is proven; patched reproduction was RED

- Verified code checkpoint is `f3076c7e` on `main`. Preserve the separate
  user-owned stdlib work: modified `docs/138_standard_library_scope.md` and
  `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`.
- Active production entry remains the Pergyra-built full MIR consumer. The
  current patched input is
  `.tmp/self_hosted/compiler/generic-enum-prefix-mir-20260810/driver_source.focused.mir.json`,
  exactly 184,370,403 bytes with SHA-256
  `398DEFC5C47822D29D548A047281676FB36947BB4415C3EE54A718AF0F72594A`.
  It was published by the Pergyra-built current-source seed in 113.622 seconds
  at 2.954 GiB peak private. The attention threshold is crossed, but the 3 GiB
  cap is not.
- A fixed-point generation input is a source snapshot, not merely a convenient
  workload. The former 183,890,971-byte input did not contain the later
  `MirProgramRoutineBlockCaptureWithin` owner. A gen2 built from that MIR
  therefore reintroduced the retired multi-read block path even when the
  consumer binary itself came from current source. Never claim current-source
  gen2/gen3 evidence until the source-to-MIR artifact has been regenerated
  after the owner change and its byte/hash receipt has been recorded.
- Closed owner seam 1: persisted expression kind, call-target kind, and binding
  kind spellings are classified directly from exact JSON spans. The node reader
  no longer materializes transient enum Strings or remaps them in the sequence
  consumer. Unknown vocabulary fails closed.
- Closed owner seam 2: match, option, tagged-enum, and destructure extensions
  preserve the admitted expression-identity prefix and append identity rows
  only for new nodes. The old
  `SemanticExpressionGraphArenaFromRows` whole-prefix Unknown reconstruction is
  deleted and negative-gated throughout `mir_lower/`.
- Closed owner seam 3: `JsonArrayStringFactWithin` owns exact-bound string-array
  grammar, comma state, closing-boundary validation, and bounded decoding.
  The old readers rediscovered `StringLength(json)` inside a carried array span;
  on the full MIR that became repeated whole-document `strlen` work.
- Closed owner seam 4: statement typing seeds the 159-row enum environment once,
  records the admitted prefix length, and removes only row-local suffix bindings
  between 42,928 statement rows. Full reset plus per-row enum reseeding is
  forbidden by the lifetime gate.
- Closed owner seam 5: generic specialization now applies the same lifetime
  invariant. It seeds the admitted enum environment once, retains that prefix,
  and pops only function/intent-surface suffix bindings between expression
  surfaces. The old pop-to-zero plus per-surface enum reseed is negative-gated.
- Fixed-input progression under the unchanged 3 GiB/300 s boundary:
  - before identity-prefix preservation: memory cap near 247 s;
  - identity-prefix run: 1.057 GiB peak, stopped during expression surfaces;
  - exact string-array bound run: 1.407 GiB peak, MIR-to-AST done at 267.269 s,
    expression sequence/coverage valid and expression graph done at 269.311 s;
  - statement-prefix run: 1.665 GiB peak, statement typing done at 252.270 s,
    generic/verdict/body verification done, C emission started at 278.260 s,
    and the last marker was `type-declarations:start` at 278.417 s.
  The later peak reflects later stage reachability and is not a stage-aligned
  memory regression. That intermediate run emitted no gen2 C artifact; the
  type-declaration closure below supersedes its RED boundary.
- A focused type-declaration receipt separated three dependency epochs. Enum
  and nominal emission completed in every reached epoch, wrapper emission was
  below 1 ms, and the repeated operation was cumulative
  `CodegenTypeEnvFromPresealRows` reconstruction. Wrapper-env time grew from
  3.351 s to 4.347 s before the third rebuild hit the 300-second boundary.
- `CodegenTypeEnvAdvancePresealRows` now owns the exact epoch transition. It
  preserves one sealed global index, appends only newly admitted enum/nominal
  rows to an ordered delta lookup view, keeps global and first-delta-row
  precedence, and updates the raw row serialization once. The enum and nominal
  batch facts return both their C block and exact env-row delta; wrapper
  declarations consume the current view without rebuilding it.
- On the former fixed MIR, type declarations run from 266.747 s to 268.561 s
  (**1.814 s**) and the full consumer exits 0 in 284.921 s at 2.107 GiB peak
  private. It publishes exactly 8,752,013 bytes with SHA-256
  `039036D38ACFA3D814FFBF97ECFC54C044EE837FE4C0821655D376D56E86A119`.
  This remains valid evidence for the type-environment delta, but not a
  current-source fixed-point artifact.
- Regenerating source MIR before the next attempt exposed and closed that
  provenance error. The pre-generic-prefix current-source MIR is 184,351,609
  bytes (`C2636CA0665B41DCB55C58FA0E87FE7F32645AACF4E17B647481D62D2BF8C673`).
  Its current AST-built consumer exits 0 in 276.634 seconds at 2.117 GiB peak
  private and publishes an 8,774,599-byte gen2 C artifact with SHA-256
  `12C796311CB6AE170EC925AE0B626A61527114939BDC50E2451936307758B452`.
  The generated gen2 binary contains the one-pass block owner and reaches
  `base-env:start` in its gen3 attempt at 296.038 seconds, but still times out
  at 300.583 seconds with no artifact and 1.668 GiB peak private.
- The generic-prefix correction is focused-green: the semantic environment
  lifetime gate exits 0 in 22.1 seconds, and the generic-return C parity plus
  mismatch negatives exit 0 in 152.9 seconds. The full C/LLVM script remains
  RED only at the existing installed self-host LLVM multi-routine terminal
  projector boundary. The patched source AST is 7,941,880 bytes and passes
  surface, event-scan, and shape checks.
- Two patched full consumers did not reach generic specialization: the O2 run
  timed out after MIR-to-AST and the canonical O3 run ended at routine 5,952.
  Their common MIR-to-AST batches were uniformly about 1.3--1.5 times slower
  than the earlier successful run (203.9 s versus 281.9 s over the common
  measured span), rather than showing one new owner spike. Therefore the
  prefix's full-scale time effect is `Unknown`; neither timeout is evidence of
  a generic regression, and no patched gen2 artifact or gen2/gen3 parity is
  claimed.
- Green focused evidence observed on current source:
  `expression_graph_identity_prefix_owner_smoke.sh`,
  `mir_expression_graph_persisted_read_owner.sh`, and
  `semantic_expression_environment_owned_lifetime_smoke.sh`. The bounded JSON
  C fixture and independent native LLVM oracle both print
  `json-bounded-string-owner-ok`.
- `codegen_type_env_preseal_epoch_owner.sh` is also green for installed
  self-host C and native LLVM. It pins global-before-delta lookup, first delta
  row precedence, ordered serialization, and the malformed-delta diagnostic.
- Known unrelated/previous blockers remain explicit. The installed self-host
  LLVM leg of the bounded JSON gate stops at
  `direct MIR terminal multi-routine graph is unsupported`; it is not counted
  green or replaced by the native oracle. `self_host_hard_contract_smoke.sh`
  reaches the separate existing missing
  `projection.flow.call_abi.target_capability_fingerprint` term. The monolithic
  component inventory exposed and closed stale requirements for the retired
  `json.pgy` string-array owner, direct nominal env append, and an obsolete
  successor rule that incorrectly rejected the valid `-1` sentinel. After
  those repairs it produced no new diagnostic but did not finish inside the
  60-second static budget (exit 124), so it remains unverified rather than
  green.
- Objective card for the next rung: objective = make the current-source
  generated gen2 driver reproduce byte-identical gen3 C inside the existing
  300-second boundary; priority = source-generation identity, semantic facts,
  old-path deletion, negative ratchet, then throughput; fact owner = the
  source-to-MIR artifact receipt followed by the existing routine and semantic
  owners carried in that exact artifact; last legitimate consumer = C artifact
  commit; forbidden fallback = stale MIR substitution, AST-built final
  substitution, timeout/cap increase, cache/query engine, shard/worker, ordinal
  skip, or fixture-specific rendering. The next falsifier must use the
  184,370,403-byte patched MIR and first establish a comparable routine-batch
  control before attributing generic-stage time. Final acceptance remains exit
  0 plus raw byte equality between gen2 and gen3 C.

## Historical checkpoint - full MIR producer and gen2 admission/block-row closure

- Verified Git checkpoint is `bc0eca60` on `main`; the compiler changes through
  the one-pass MIR block-row capture rung are committed. The remaining worktree
  changes are separate user-owned stdlib work: modified
  `docs/138_standard_library_scope.md` and
  `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`.
- The current source closes two production-sized memory seams without raising
  the 3 GiB boundary. MIR lowering now retires 13 non-traversal typed-AST arena
  backings after domain projection and the remaining five traversal backings
  after routine/intent facts. Match lowering consumes the admitted semantic
  statement payload instead of rereading retired atom lanes. Routine-build
  final carriers are retired after append, and local-version/count restoration
  reuses its active backing rather than replacing it.
- The streaming MIR artifact writer no longer retains expression-identity field
  arrays or one pool per escape-free scalar. One identity projection view owns
  target/kind/ordinal readiness for both String and file renderers. Owned row
  fragments are retired after synchronous writes, while borrowed fact strings
  use a non-owning quote path. `JsonEscapeTokenAt` is the single escape mapping
  consumed by both the fast-path predicate and escaped renderer. The focused
  lifetime ratchet rejects direct borrowed Strings despite `String` currently
  being copy-only.
- Fresh self-host codegen seed and bounded integrated driver bootstrap exit 0.
  The bounded driver run takes 358.010 s, peaks at 2.065 GiB private, and passes
  sample, MIR producer, and MIR consumer parity.
- The current-source full producer is green. The Pergyra-built seed finishes
  6,049 routines and 14 intents, reaches `json-write:done`, and commits a
  186,071,774-byte MIR in 74.077 s at 2.974 GiB peak private. The independent
  native oracle emits the same byte count in 128.048 s at 2.365 GiB. Both have
  SHA-256
  `345DD2E30AF1B75CE1B7B6797A4ABC9F1A979449FF4A6130436E8ACDB359AE95`.
  This closes the former JSON-write memory blocker, but the self-host path has
  only about 26 MB private-memory headroom and remains above the 2.4 GiB
  attention threshold; do not call its memory profile complete.
- Green focused evidence: native compile of the writer probe; six-fixture raw
  String/file byte parity including empty, quote, backslash, LF, CR, and TAB;
  `mir_json_artifact_writer_lifetime_owner.sh` in 0.6 s; refreshed codegen seed;
  bounded driver bootstrap; full self/native MIR byte parity. The monolithic
  component contract still does not finish inside the 60-second static budget
  and is unverified, not green.
- Active executable rung remains the Pergyra-built
  `driver_seed.exe --mir-json <full MIR> -o driver_gen2.c` consumer. The old
  non-observed 900-second timeout was localized with the existing
  `--observe-mir-consumer-stages` path. Input read and parallel capture finish
  in 0.214 s, but declaration/topology admission takes about 130.1 s and the old
  routine index then spent another 136.5 s rebuilding the same declaration
  index from the 186 MB document.
- The current source carries `MirDocumentFactIndex.declarations` into one
  `MirProgramDeclarationIndex`, then passes that exact typed index into
  `BuildMirProgramRoutineIndexFromTable`. The routine owner is negative-gated
  against document, declaration, or raw-bounds reconstruction. A foreign
  declaration receipt and an extended declaration-table boundary both fail
  closed. The focused routine-index gate reaches its C leg, but the installed
  self-host LLVM leg remains RED at the known `direct MIR terminal
  multi-routine graph is unsupported` projector boundary; no native LLVM
  substitution or skip is accepted as closure.
- A fresh Pergyra-built driver first proved the declaration-index carry delta.
  On the same input, `routine-index:start` at 130.396 s reached
  `routine-index:done` at 131.268 s: **0.872 s instead of 136.5 s**. That fixed
  300-second run reached MIR-to-AST routine 1,600 instead of routine 576.
- The next reached declaration cost was not the remaining field-name scans. An
  exact-bounds object table still called `JsonSkipWhitespace`, whose internal
  `StringLength(json)` became `strlen` over the complete 186 MB document.
  `JsonObjectFactCount` and `JsonObjectFactIndex` now consume `table.end`
  through `JsonSkipWhitespaceWithin`; their structural gate rejects the
  unbounded call and an unfinished one-pass field-row/Set experiment was
  rolled back after red-team found grammar, allocation-lifetime, and fail-open
  insertion risks.
- The current Pergyra-built driver hash is
  `4D58168C525E4F61FEBAD687A64BDD68F82610FC133C1EC548D0B8FEBFB3FB3B`.
  On the unchanged 186,071,774-byte MIR, declaration admission is now 0.286 s
  (`parallel-capture:done` 1.398 s to `declaration-index:done` 1.684 s), topology
  takes 0.079 s, and routine indexing takes 1.221 s. The fixed 300-second run
  reaches routine 2,240, peaks at 422.2 MB private, and still publishes no C
  artifact. Therefore the exact-bounds sub-seam is closed but the consumer rung
  remains RED. The 1,024-to-1,088 batch still takes 39.946 s and contains the
  generated LanguageWord projection chains; that is the next reached owner.
- The generated LanguageWord projection now keeps the typed
  `LanguageWordId` spelling owner and the 70-row reserved compatibility view,
  but replaces five fragmented metadata files and eleven 146-case selector
  ladders with one complete immutable `LanguageWordRegistryRowAt` projection.
  Registry readiness and LSP completion bind one row per index; invalid `-1`
  and `count` lookups fail closed, the LSP no longer recounts the same rows,
  and the five retired files/APIs are negative-gated. Responsibility caps are
  identity 330, row 200, compatibility 250, and hub 80 lines.
- Focused evidence is green: the 146-row generator/registry gate in 4.7 s,
  native/self-host LSP byte parity for 28 items in 23.1 s, and typed parser
  boundary parity in 104.3 s. The current-source self-host producer exits 0
  in 82.265 s at 3,032.7 MB private and the native oracle exits 0 in 148.323 s
  at 2,418.9 MB. Both commit exactly 184,181,002 bytes with SHA-256
  `C4CC3F161F69E978127209A1857BD87F47F0284E39FF7478C222F6D086773EE2`;
  a direct byte comparison exits 0. The self-host producer has only about
  39 MB of 3 GiB headroom, so its memory profile remains attention debt.
- On the old fixed MIR workload, the new O2 measurement carrier still takes
  43.291 s for routines 1,024-to-1,088 and reaches 2,240; that control does not
  contain the aggregate-row workload. The current-source MIR reduces the same
  marker batch from 3,434 blocks to 1,247 and the interval to 15.389 s, a
  64.4% wall-time reduction matching the 63.7% block reduction. The 300-second
  run reaches routine 2,368 at 390.5 MB private but still publishes no C
  artifact. These binaries are comparable O2 measurement carriers, not the
  canonical O3 installed release.
- The intent-observability registry now owns positive unique non-positional ABI
  IDs and explicit `NONE`/`INT`/`INT_INT` parameter shapes. Native arity/kind
  and generated self-host signatures derive from that shape. Six generated
  51-case selectors are replaced by one complete immutable row, with invalid
  lower/upper sentinels and negative gates against positional identity or old
  selectors. A lexically middle ID-99 mutation preserves every existing ID;
  duplicate and zero IDs fail closed.
- Native registry, runtime contract, verified-plan, and self-host builtin C/LLVM
  gates are green. The new Pergyra-built producer exits 0 in 101.198 s at
  2.965 GiB private, and the native oracle exits 0 in 132.579 s at 2.325 GiB.
  Both commit 183,890,971 bytes with SHA-256
  `9B144FD5D25A18EA22BECA1BB78BA51484EC68BF6ADE846B0762F63F898D1A57`;
  byte comparison exits 0.
- The current-source 1,920-to-1,984 batch falls from 24.363 s to 15.952 s. The
  300-second run reaches routine 2,752 at 401.7 MB private instead of 2,368,
  but still publishes no C artifact. The next largest completed intervals are
  2,112-to-2,176 at 18.316 s and 2,176-to-2,240 at 18.146 s. The first is
  dominated by branch-heavy parser owners (`ParseIntentDecl` 287 blocks and
  `ParseNominalDecl` 196 blocks), not another parallel registry projection.
- A focused receipt then separated `ParseIntentDecl`: routine fact-index
  construction took 3.247 s, validation/header about 0.003 s, and region
  emission about 0.215 s. An allocation-free terminal-true-arm CFG proof was
  sound on focused C/native-LLVM fixtures but changed the fixed 2,112-to-2,176
  batch by only about 0.4%, so that experiment was rolled back rather than
  recorded as progress.
- The reached repeated operation was the block JSON read itself. The program
  index read block id/instruction bounds, then the routine index reopened the
  same object three times for reachability and successors, crossing the large
  instruction payload on each lookup. `MirProgramRoutineBlockCaptureWithin`
  now owns one exact order-independent scan and carries aligned reachability and
  successor facts in `MirProgramRoutineIndex`; the downstream routine owner is
  negative-gated against the old raw-key reads. Missing/null successor `-1`
  remains distinct from an explicitly negative invalid successor, preserving
  the `cfg_successor` diagnostic before graph validation.
- Focused C and native LLVM both print exact
  `mir-program-routine-index-owner-ok`. The repository focused script reaches
  and passes its C leg, while installed self-host LLVM remains RED at the known
  `direct MIR terminal multi-routine graph is unsupported` boundary. The
  bounded JSON C leg is green; its installed LLVM leg stops at the same known
  projector boundary. The current source AST is 7,931,132 bytes and passes
  self-host codegen surface/event-scan/shape `--check` before emission.
- On the unchanged 183,890,971-byte MIR (SHA-256
  `9B144FD5D25A18EA22BECA1BB78BA51484EC68BF6ADE846B0762F63F898D1A57`),
  the 2,112-to-2,176 batch fell from 18.243 s to 3.215 s. The 300-second run
  completed all top-level MIR-to-AST routines at 270.226 s and reached
  `consumer:mir-to-ast:done` at 270.556 s, instead of stopping at routine
  2,752. It timed out during expression-graph surface assembly at row 90,112.
  The 2.803 GiB private peak belongs to that newly reached stage and is not a
  stage-aligned comparison with the old 0.372 GiB mid-routine peak.
- Apparent compact-semantic regressions were corrupted manual AST carriers,
  not a language change. An ad-hoc CR-removal command deleted literal digits
  `0`, `1`, and `5`, producing expressions such as `bounds[]`. The official
  parser capture/normalization path preserves those bytes; the corrected
  8,058,031-byte AST passes self-host codegen `--check` in 3.25 s. Never accept a
  malformed AST by best effort or treat this carrier error as semantic evidence.
  A second nested-shell attempt interpreted `\r` as the literal letter `r` and
  damaged identifiers such as `cursor`. Use a byte-preserving CRLF converter
  such as `dos2unix`, then require codegen `--check` before emission.
- Objective card: production entry is the integrated driver's `--mir-json`
  mode; the closed sub-seam owners are the document-owned declaration index,
  exact-bounds JSON object fact table, complete LanguageWord registry row,
  complete intent-observability ABI row, and exact one-pass MIR block-row
  capture. The next open reached boundary is expression-graph surface assembly;
  its repeated owned operation is still Unknown and must be localized from the
  existing surface-row receipts before another structural change. The last
  legitimate consumer
  is C artifact publication followed by the gen2/gen3 byte comparator.
  Forbidden fallbacks are native substitution, skipped oracle parity, whole-
  document or per-routine reparsing, graph reconstruction, cache/query engine,
  shard/worker, timeout or memory-cap increase, and fixture-specific output.
  The next falsifier must use the same current-source MIR and distinguish graph
  surface admission from sequence/identity construction without adding a
  cache, global observation path, or a second graph fact owner.
  Final acceptance remains a bounded gen2 C
  artifact; only then may the generated binary attempt byte-identical gen3
  reproduction.

## Historical checkpoint - expression-graph memory blocker closure

- Verified code checkpoint: `ca60298e` on `main`. The expression-graph identity
  prefix correction is `8d3c913f`; the non-fragmenting scalar-CFG readiness and
  stale ownership-gate repair is `ca60298e`. Preserve the
  separate user-owned stdlib work: modified `docs/138_standard_library_scope.md`
  and `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`.
- Active executable rung: rebuild the Pergyra-built codegen/driver generation
  from current `ca60298e` (which contains `8d3c913f`), install that artifact,
  and run the same full MIR-to-C request.
  The native-generated measurement carrier below proves the owner correction
  but is not `SUBSTITUTING` self-host progress and must not be installed as the
  released driver.
- Exact input evidence: the production MIR is 84,972,718 bytes and contains
  2,774 routines, 45,071 instructions, 45,588 persisted expression graphs,
  257,457 graph nodes, and 1,917 producer-only collection parser-bridge
  occurrences (`ArrayPush` 1,448, `ArraySet` 450, `ArrayPop` 19).
- Root cause: `MirExpressionGraphSequenceAppendParserBridge` appended topology
  rows, then called `SemanticExpressionGraphArenaFromTopology`. That constructor
  allocated three Unknown identity arrays for the entire cumulative graph on
  every bridge occurrence. The same old constructor in
  `MirIntentExecutionGraphTargetProject` also discarded admitted identity rows.
  Source-order simulation gives about 10.014 GiB of capacity allocation for the
  parser bridge alone; a cache, worker, shard, or larger memory limit would only
  hide the repeated owner operation.
- The committed correction preserves the admitted prefix identity arrays,
  validates their lengths against the topology offset, appends `0 / none / -1`
  only for new parser nodes, and reconstructs through
  `SemanticExpressionGraphArenaFromTopologyWithIdentities`. Intent target
  projection adds no nodes and therefore passes the exact existing identities.
  Negative structural gates reject reintroduction of the whole-prefix Unknown
  constructor in either consumer.
- Focused evidence is green. The existing collection graph-use owner gate
  passes, and `expression_graph_identity_prefix_owner_smoke.sh` completes in
  54.3 seconds. Its executable falsifier starts with nonzero
  `call_target_syntax_id=713`, formal binding kind, and ordinal 4; those values
  must survive parser bridging and intent target projection. Installed self-host
  C and native LLVM both print exact `expression-graph-identity-prefix-ok`.
  Installed self-host LLVM still rejects this multi-routine import closure at
  its known bounded terminal projector; the gate records native LLVM evidence
  rather than silently skipping that independent blocker.
- Before the fix, the installed driver reached `mir-to-ast:done`, then rose from
  roughly 372 MiB private to 3.428 GiB during expression-graph surface assembly
  and was stopped at 270.371 seconds with no artifact. With the corrected
  measurement carrier, the same input reached `mir-to-ast:done` at 285.379
  seconds, `expression-graph:sequence:done:valid:true` at 286.025 seconds, and
  `expression-graph:done` at 286.069 seconds. The full integration exited 0 at
  320.355 seconds, peaked at 0.924 GiB private / 0.849 GiB working set, reached
  `consumer:c-emission:done`, and emitted a 3,956,147-byte C artifact.
- The measurement carrier exposed two separate bootstrap contracts. A monolithic
  native source-to-executable build crossed 3 GiB because the native compiler
  retained about 1.9 GiB while `cc1` allocated about 1.7 GiB. Splitting existing
  `--emit-c` and host compilation completed in 49.576 seconds / 1.901 GiB and
  92.826 seconds / 1.702 GiB respectively. Also, native-generated runtime headers
  enforce a 64 MiB file-read limit while the currently installed old self-host
  driver reads this 84.9 MB MIR. The final measurement carrier used a temporary
  96 MiB compile-time read limit only to match the installed binary's observed
  input capability; no repository cap was changed. This runtime-contract drift
  remains RED and must not be mistaken for the memory fix.
- Structural gate maintenance found and corrected a C-function extraction bug:
  the Pergyra `^func` scanner had read a C helper through EOF and falsely blamed
  a later function's `lookup_typed_var`. The C helper is now scoped through its
  column-zero closing brace. Four remaining hard-cap drifts were restored
  without raising any cap. Value-name/local-row/definition-block readiness was
  folded into its existing `DirectMirScalarCfgLocalRefPlan` owner instead of
  adding another owner file. The exhaustive `require_max_lines` census now has
  zero missing or over-cap files; current owner import closure compiles with
  `0 error(s), 0 warning(s)`, and the scalar routine-partition plus exact C/LLVM
  graph projection gates are green. The full component inventory still has no
  final end-to-end green receipt because it did not complete inside the
  60-second static-gate budget.
- Next falsifying case: produce a current Pergyra-built codegen/driver artifact
  without overlapping compiler and host-compiler lifetimes; install it only
  after its bounded source smoke passes. Then require the installed driver to
  consume the exact 84,972,718-byte MIR below 3 GiB, reach
  `expression-graph:sequence:done`, exit 0, and emit the same C artifact class.
  Separately reconcile the 64 MiB runtime read contract with the installed
  driver's observed capability through an owned large-artifact input protocol;
  do not raise the global file cap as an incidental fix.
- Objective card: production entry is installed `pgy-self-driver --mir-json`;
  fact owner is the `MirExpressionGraphSequence` identity prefix; last legitimate
  consumers are parser bridge and intent target projection; forbidden fallbacks
  are whole-prefix Unknown reconstruction, identity reset, native-driver release,
  file/memory cap increase, cache, shard, worker, timeout increase, or skipped
  parity. Acceptance is an installed Pergyra-built exit-zero artifact under the
  existing 3 GiB memory boundary plus the independent runtime input-contract
  decision.

## Historical checkpoint - full MIR parity and delegated-intent closure (inactive)

- Verified compiler checkpoint: `219f8568` on `main`. The DIR/HIR
  ResourceFlow, branch-merged `inout` copy-out, and delegated intent authority
  changes are committed locally; push remains pending with this handoff.
  Preserve the separate user-owned stdlib
  work: modified `docs/138_standard_library_scope.md` and
  `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`. The
  remaining task-owned tracked edits are documentation only.
- Active executable rung: make the Pergyra-built driver finish `gen2_emit`
  after the already-green full MIR seed/oracle parity, then prove the next
  generation rather than counting owner files or structural gates as
  self-host progress.
- Production evidence has moved beyond the previous DIR checkpoint. The
  isolated full bootstrap produced a 5,119,918-byte seed driver and a
  5,558,484-byte native oracle driver. Seed and native oracle each emitted an
  exact 185,290,446-byte full MIR (`~81s` and `~162s`) and the comparator found
  them byte-identical. This proves the committed DIR ResourceFlow closure and
  passes the former routine-1520 loop reachability boundary.
- Routine 1520 failed because branch-merged `inout` values were not considered
  implicit MIR exit uses. DCE removed their merge PHIs, C wrote the stale
  copy-in local, and LLVM also read its stale parameter alloca. The committed
  change preserves only value-result parameter PHIs and makes LLVM MIR
  copy-out consume the block's exact `ssa_exit_values`; the now-unreferenced
  generic stale-storage writeback was deleted and negative-gated. The
  independent runtime fixture now requires both C and LLVM to print exact `7`
  then `9`; three adjacent inout parity cases and the selective-DCE unit are
  green.
- With that seam closed, `gen2_emit` ran for about 17 minutes and failed
  naturally at `MIR intent step semantic carriers are incomplete`. Peak
  observed memory was about 603 MB working set / 624 MB private, not the former
  20 GB symptom. The exact missing direct carriers are
  `MiddleEndPipeline.Check`, `MiddleEndPipeline.Lower`, and
  `BackendPipeline.Emit`.
- These three steps deliberately delegate to a declared nested intent/action
  contract and must not duplicate `requires/authorized by` at the outer
  orchestration step. The self-host MIR-to-AST consumer nevertheless required
  one direct authority row for every step, while the compiler-world gate both
  forbade those source clauses and required the resulting AST carrier. The
  committed change admits either one exact direct carrier or one exact declared
  intent/action delegation, retains terminal action authority, emits no blank
  outer `AuthorizedBy`, and keeps missing/duplicate/mismatched facts
  fail-closed.
- Focused evidence is green: MIR unit `160/160`; C/LLVM inout parity `3/3`;
  MIR declaration inventory and backend fail-closed gates; exact native MIR ->
  updated self-host lower -> updated self-host codegen -> host C execution of
  `intent_nested_direct` with output `nested-intent-direct-ok`; and the
  compiler-world source/AST authority gate. The full `mir_json_parity.sh`
  wrapper did not finish its tool rebuild inside the focused five-minute
  budget, so its result is not claimed.
- The program-scale self-host lower over the exact 185,290,446-byte driver MIR
  completed naturally in 1,323,187 ms with exit code 0, zero stderr, and an
  8,497,137-byte UTF-8 recursive AST. Last observed resource use before exit
  was about 685 MiB working set / 718 MiB private. That executable contained
  the delegation correction; the subsequent stricter all-`Authorize` row
  count is separately compiled and green on the positive fixture plus the
  undeclared-delegation negative. Do not overstate the full run as evidence
  for that later negative-only tightening.
- The next self-host codegen pass exposed a distinct memory blocker. Over the
  exact 8.50 MB recursive AST it grew from about 1.38 GiB private at 34 seconds
  to 3.35 GiB at 83 seconds and 3.42 GiB at 118 seconds, while both C output
  and diagnostics remained empty. The task-owned process was stopped after it
  crossed the 3 GiB observation boundary; no unrelated process was touched.
- Next falsifier: reuse the existing codegen pressure-stage vocabulary through
  one opt-in standalone diagnostic route, and repeat only until the first
  incomplete owner boundary or 3 GiB. The current last completed substage is
  `Unknown`, because the normal standalone route hardcodes pressure observation
  off before semantic admission. Do not infer the culprit from memory growth
  alone, and do not raise timeout/memory/count caps or introduce a cache,
  shard, query engine, worker, or native fallback.
- Objective card: production entry is the full bootstrap `gen2_emit`; the
  intent fact owner and MIR-lower consumer are now green through program-scale
  recursive AST publication. The open owner is the first incomplete
  standalone-codegen pressure stage, not yet known. The last consumer is C
  artifact publication. Forbidden fallbacks are all-step authority synthesis,
  step-name allowlists, blank carriers, duplicated clauses, native C fallback,
  skipped oracle parity, memory/timeout increases, cache, shard, or worker.
  Acceptance is one bounded full-AST codegen artifact below the existing 3 GiB
  observation boundary, followed by the bootstrap generation comparison.
- Latest pushed GitHub run before this rung is `31282935770`. It is not a
  green claim. Evaluate its failures against the exact revision and do not mix
  them with local post-commit evidence.

## Historical checkpoint archive - inactive navigation evidence

### Previous Array<String> call/index boundary closure

- Executable checkpoint: `52715894` on `main`, with this handoff as its
  intended docs-only descendant. The final current-source Pergyra-built driver
  is 5,048,145 bytes with SHA-256
  `B698C2C4C86C6BACB96C3D7F3E6FABB030F8B2629DEA06800C574BF89822CD2A`.
- Intended post-handoff dirty state contains only user-owned stdlib work:
  modified `docs/138_standard_library_scope.md` and
  `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`. No
  compiler-rung file remains dirty.
- Closed executable rung:
  `src/self_hosted/codegen/fixture/string_array_index_return.pgy`. Its
  6,234-byte self-produced MIR has SHA-256
  `EE124A64CBFF373C365992E7EAC63084C8358A152F094AC13A2595C45BCF0DE6`.
  The same MIR emits 1,819-byte C
  (`FFD6E40F6100B917BA7E65B30E9EEF981238CD388C53E92BEC3675E2BD4B00DD`)
  and 4,660-byte LLVM
  (`B1B03043830710D151AC18D9FAD7C39B372DFC6D9F22CD801EC3E4826CB2A0F9`).
  Both host-compile and execute exact `one`.
- `DirectMirScalarCfgGraphPlan` v23 remains the sole CFG, SSA, local,
  operation, routine-range, expression-link, ABI-seal, digest, and mutation
  authority. One target-neutral boundary fact joins the caller literal,
  canonical `Array<String>` ABI, by-value parameter, callee literal index,
  borrowed String return, and last caller use.
- The literal fact is shared with the older local indexed-array route instead
  of introducing another String-array graph. Callable parameter policy now
  distinguishes scalar ABI absence from the required canonical aggregate ABI;
  the broad route envelope only claims the typed family and final readiness
  still fails malformed signatures inside the same owner.
- Lifetime is explicit rather than inferred from `Array<String>` alone. C uses
  block-lifetime compound-literal backing; LLVM uses caller-frame `alloca`
  backing. Both treat literal elements and the returned String as borrowed and
  suppress deep-drop only for the sealed borrowed local. Split-produced owned
  arrays retain deep cleanup. Negative and upper-bound indices fail before
  either artifact is published.
- The LLVM foreign-declaration compositor was lifted from program-only naming
  to `direct_mir_scalar_cfg_llvm_foreign_declaration_owner.pgy`. The scalar
  program and legacy indexed String-array projection now share it; this also
  fixed the reached legacy concat artifact's previously undeclared
  `malloc`/`strlen`/`memcpy` calls.
- The focused gate covers base, display and routine-order artifact equality,
  semantic value/index changes, exact dual-target execution, and malformed
  parameter type/carriage/pass/ABI/layout, local layout, call target/argument,
  return type, index bound/topology, and literal-spine families. All malformed
  cases are artifact-free. The focused gate and nine adjacent scalar/string/
  array regressions are green on the final driver; all reached owner hard caps
  remain green and no existing cap was raised.
- The final composition build passed in 128.8 seconds. No memory-pressure
  result is claimed. Full CI, current gen2==gen3, proofs, sanitizers, and
  released/default promotion did not run.
- Pergyra structure is intentional: this rung is pure value computation and
  value carriage, so `func`/`struct` are the native form. Adding `action`,
  `zone`, `intent`, or `tobject` would invent authority, resource, purpose, or
  nominal identity that the executable boundary does not own.
- Independent red evidence remains explicit. The component contract stops at
  the pre-existing `ast_expression_graph_fact_owner.pgy` 616/600 cap. The SoT
  registry gate stops on six duplicate Coq fact groups spanning 17 rows:
  CFG certificate, CFG projection plan, scalar CFG subfacts, ABI layout views,
  call-return identity/plan, and call-parameter identity/plan. This audit is
  recorded for a later owner-registry closure and did not become a parallel
  implementation track on this rung.
- Classification is bounded `SUBSTITUTING` only for this exact
  `Array<String>` literal/parameter/index/borrowed-result program through the
  installed direct C and LLVM projections. It does not promote arbitrary
  aggregate calls, owned String return lifetime, dynamic indices, multiple
  callables, or whole-compiler/default replacement.
- Next observed executable falsifier:
  `src/self_hosted/codegen/fixture/string_utils_core.pgy`. The current producer
  emits 7,229-byte MIR with SHA-256
  `46DC2EC9AF786D4D072608B32F6C29F919B99994CFA9749E1319794EFBFBD6D9`.
  C and LLVM publish no artifact and fail closed at `direct MIR scalar local
  type inventory is missing or invalid`; the native C oracle executes exact
  `hello world pergyra` and `3.500000`.
- Next objective card: keep `CompileAdmittedDirectMirForTarget` as production
  entry and GraphPlan v23 as fact owner; admit the reached Float local without
  reopening the legacy local-type inventory, then carry registry-owned
  `Join(Array<String>, String)` and `ToFloat(String)` facts to the installed C
  and LLVM consumers. Falsify local type, Join argument order/ABI, ToFloat
  result/runtime identity, and target projection before artifact publication.
- Forbidden fallback remains a fixture/name/output branch, source-text or
  first-local type inference, flattened Array values, backend-only builtin
  reconstruction, a second graph/emitter, count routing, claimant retry, or
  native C fallback.

### Previous StringIndexOf window program closure

- Executable checkpoint: `fb0561f6` on `main`, with this handoff as its
  intended docs-only descendant. The final current-source Pergyra-built driver
  is 5,019,513 bytes with SHA-256
  `699D7D7847AE07B1F6E6BB5AF22CACACAF23185EE4CD363939BB744342AC598E`.
- Intended post-handoff dirty state contains only user-owned stdlib work:
  modified `docs/138_standard_library_scope.md` and
  `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`. No
  compiler-rung file remains dirty.
- Closed executable rung: `src/self_hosted/codegen/fixture/str_indexof.pgy`.
  Its 14,215-byte self-produced MIR has SHA-256
  `28F0C0C026E62F749AEF2150B5100444962B6260D242F4560E9A2262954F1C75`.
  The same MIR emits 1,898-byte C
  (`DC0BB8CDFAEA1CE29E62AE2C2ED294FAA1EAE767BDE8708BDF0D9AA653C0430A`)
  and 5,632-byte LLVM
  (`68E1A169E649979F92BCFC7749824BEF0B75885D0ABB3337EE9774064F5D3E7F`).
  Both host-compile and execute exact `5`, `-1`, `hello`, and `world`.
- `DirectMirScalarCfgGraphPlan` v22 remains the sole CFG, SSA, phi, local,
  operation, routine-range, expression-link, digest, and mutation authority.
  A canonical StringIndexOf signature and sealed runtime subfact now carry its
  `-1`-or-byte-offset result contract into both backends.
- The reached `p + 1` and `StringLength(source) - p - 1` forms are admitted
  only when `p` has one earlier same-block StringIndexOf definition over the
  same source. This is bounded structural range evidence, not compile-time
  search evaluation or a general signed-arithmetic relaxation.
- Runtime parity was corrected at the same owner boundary: self-host C and
  LLVM Substring now reject/clamp invalid windows to the native empty-string
  behavior, StringLength preserves one signed-result headroom, and
  StringIndexOf handles missing and null input explicitly.
- Focused evidence covers base, semantic mutation, absent search, empty needle,
  display-only equality, and seven malformed signature/topology/type/target/
  range families. Every malformed family fails before artifact publication in
  both targets.
- The final composition build passed in 167.8 seconds. The final focused gate
  passed in 17.7 seconds; String window, case/math, collection, equality/concat,
  and routine-partition regressions are green on this source lineage. No
  memory-pressure result is claimed. Full CI, current gen2==gen3, proofs,
  sanitizers, and released/default promotion did not run.
- Layering closure: condition-bound and StringIndexOf range proofs have
  separate owners; LLVM checked substring materialization was split from its
  compositor; all new owners have explicit caps and no existing cap was
  raised. No helper bucket, duplicate production file, second graph/emitter,
  backend MIR read, fixture route, compile-time evaluator, or native fallback
  was added.
- The bounded audit still finds zero byte-identical production `.pgy` groups
  and zero generic helper paths. Four production owners remain above 600
  lines. Duplicate `CheckFunction` and `CharAt` spellings remain responsibility
  naming seams. Repeated `strstr`, `memcpy`, and `abort` declarations across
  LLVM runtime materializers are an integration seam, to be centralized only
  when a combined executable runtime reaches a real collision.
- Independent red evidence remains explicit. The component contract still
  stops at the pre-existing `ast_expression_graph_fact_owner.pgy` 616/600 cap,
  and the SoT registry still has the previously recorded duplicate Coq fact
  authorities. These are not this rung's blockers.
- Classification is bounded `SUBSTITUTING` only for this StringIndexOf/window
  program through installed direct C and LLVM. It does not promote arbitrary
  String search, temporary String ownership, general arithmetic, or whole-
  compiler/default replacement.
- Next observed executable falsifier:
  `src/self_hosted/codegen/fixture/str_trim.pgy`. The producer succeeds with
  11,463-byte MIR, SHA-256
  `1A10A12B315C2B48E715441966738724C0E1D8E5A120766DC87987E494D52BE8`.
  C and LLVM publish no artifact and fail closed at expression row 1
  (`AST_LET_DECL`) for `StringTrim(raw)`.
- Next objective card: keep `CompileAdmittedDirectMirForTarget` as production
  entry and GraphPlan v22 as fact owner; join the canonical StringTrim
  signature/runtime contract to typed expression admission and materialize it
  from the same sealed fact in C and LLVM. The last consumers are the installed
  direct target projections. Falsify result/argument type, argument chain,
  unregistered target, target syntax, and boundary whitespace semantics before
  artifact publication.
- Forbidden fallback remains a fixture/name/output branch, expression-text
  parsing, compile-time trim evaluation, a second graph/emitter, routine/block-
  count routing, backend MIR reads, claimant retry, or native C fallback.

### Previous ordered call and String case/math closure

- Executable checkpoint: `1b620f9b` on `main`, with this handoff as its
  intended docs-only descendant. The final current-source Pergyra-built driver
  is 5,006,609 bytes with SHA-256
  `FD3C0343318992F13A60FCBE8B4C7628AC3486466A236F317E4F2AFBC2B1FB42`.
- Intended post-handoff dirty state contains only user-owned stdlib work:
  modified `docs/138_standard_library_scope.md` and
  `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`. No
  compiler-rung file remains dirty.
- Closed executable rung: `src/self_hosted/codegen/fixture/str_case_math.pgy`.
  Its 24,283-byte self-produced MIR has SHA-256
  `D0E8EDFAF1B91AED04D5ED99BBDDCD3BB7B250DB673810DD5CCB224E29CDA7AF`.
  The same MIR emits 2,944-byte C
  (`09EE9010DC668710BE8F6615BBF7418715269B0CF1A27577B26327B77C94DDB7`)
  and 10,087-byte LLVM
  (`723A4AE4154280513F4779771E01B2276D71779250251FE0D714308341A3BD9C`).
  Both host-compile and execute exact `HELLO, WORLD!`, `hello, world!`,
  `Hello, Pergyra!`, `a+b+a+b`, `42`, `3`, `7`, `50`, and `7`.
- `DirectMirScalarCfgGraphPlan` v21 remains the sole CFG, SSA, phi, local,
  operation, routine-range, expression-link, digest, and mutation authority.
  The canonical signature now seals ordered parameter identity as typed
  parallel arrays and retains the first complete parameter only as the legacy
  compatibility projection. No unsupported `Array<struct>` ABI or backend
  parameter reconstruction was introduced.
- Persisted call-target SyntaxNodeId and argument-chain edges produce ordered
  direct-call operands. A sealed runtime subfact owns StringReplace/Abs/Min/Max
  identities; C and LLVM materialize real runtime bodies from that same fact.
  The addition in `Abs(-5) + Max(1, 2)` is admitted only through bounded
  constant-DAG magnitude evidence, not a general signed-overflow relaxation.
- Focused evidence: display-only and routine-order mutations are artifact
  equal; changing `Hello, World!` to `Hello, Codex!` changes both artifacts and
  exact output. Bad parameter ordinal/type, call chain/target syntax, return
  type, builtin argument/target, and unsafe magnitude all fail in both targets
  without artifact, terminal-graph retry, or generic-CFG diagnostic.
- Final composition build took 171.9 seconds. The final focused and partition
  gates passed in 17.9 seconds. Collection, window, nested-builtin,
  routine-partitioned String, and Bool regressions also passed on the preceding
  current-source driver. No memory-pressure result is claimed. Full CI,
  current gen2==gen3, proofs, sanitizers, and released/default promotion did
  not run.
- Layering closure: ordered parameter JSON admission and identity are separate;
  direct-call, signature, builtin expression, runtime requirement/projection,
  and C/LLVM materialization have responsibility owners; expression rejection
  reports routine/row context; plan construction delegates final verification.
  The pre-existing 87/85 seal-owner failure was closed at 73 lines without
  raising a cap. Every new owner is capped. No helper bucket, duplicate
  production file, second graph/emitter, backend MIR read, fixture route,
  compile-time evaluator, or native fallback was added.
- The bounded audit still finds no byte-identical production `.pgy` files and
  no generic helper path. Open debt remains the count-based backend dispatcher,
  four production owners above 600 lines, and the two production
  `CheckFunction` responsibilities. They stay inactive until reached by an
  executable rung or focused gate.
- Independent red evidence remains explicit. The component contract still
  stops at the pre-existing `ast_expression_graph_fact_owner.pgy` 616/600 cap,
  and the SoT registry still has the previously recorded duplicate Coq fact
  authorities. These are not this rung's blockers.
- Classification is bounded `SUBSTITUTING` only for this ordered-call and
  String case/math program through installed direct C and LLVM. It does not
  promote arbitrary callable counts, recursive calls, temporary String
  ownership, all numeric operations, or whole-compiler/default replacement.
- Next observed executable falsifier:
  `src/self_hosted/codegen/fixture/str_indexof.pgy`. The current producer emits
  14,215-byte MIR with SHA-256
  `28F0C0C026E62F749AEF2150B5100444962B6260D242F4560E9A2262954F1C75`.
  C and LLVM publish no artifact and fail closed at expression row 1
  (`AST_LET_DECL`) for `StringIndexOf(s, ",")`. The native oracle executes
  exact `5`, `-1`, `hello`, and `world`. The preceding `str_greet.pgy` probe is
  already green in both direct targets and is not the next gap.
- Next objective card: keep `CompileAdmittedDirectMirForTarget` as production
  entry and the same GraphPlan as fact owner; join registry-owned
  StringIndexOf signature/runtime identity to typed expression admission, then
  carry its `-1`-or-byte-index result contract into the reached `p + 1` and
  length subtraction consumers. Falsify argument type/chain, absent search,
  invalid result-range evidence, and unsafe index arithmetic before artifact.
- Forbidden fallback remains a fixture/name/output branch, expression-text
  parsing, compile-time StringIndexOf evaluation, unproved signed arithmetic,
  a second graph/emitter, routine/block-count routing, backend MIR reads,
  claimant retry, or native C fallback.

### Previous String window definition closure

- Executable checkpoint: `585776f0` on `main`, with this handoff as its
  intended docs-only descendant. The final current-source Pergyra-built driver
  is 4,942,644 bytes with SHA-256
  `40FAA8C611A2F8219CC1F22BFAC25EB0085049DCA8C270EE782CDE2AD7667619`.
- Intended post-handoff dirty state contains only user-owned stdlib work:
  modified `docs/138_standard_library_scope.md` and
  `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`. No
  compiler-rung file remains dirty.
- Closed executable rung: `src/self_hosted/codegen/fixture/str_builtins.pgy`.
  Its 11,879-byte self-produced MIR has SHA-256
  `0378770C6AF86E963E8C73B700B4F043250DDA397AE5D3B7E9290220520220C4`.
  It emits 1,798-byte C
  (`F5475B5CAA4865B63037805394C7D2048431201D00E6EF66412C930E5E3ABEC9`)
  and 4,937-byte LLVM
  (`253076BF5DCDD75308303D9DDAF231995F63AFF6CD25C7C3AE9FC693BFEDAC4`).
  Both host-compile and execute exact `7`, `perg`, and `perg-yra`.
- `DirectMirScalarCfgGraphPlan` v19 remains the sole CFG, SSA, phi, local,
  operation, routine-range, expression-link, digest, and mutation authority.
  Persisted argument-chain edges supply n-ary topology; the semantic builtin
  registry owns exact names, result types, arity, and argument types. Typed
  rows now cover StringLength, Substring, bounded SubstringWithLen, concat, and
  integer subtraction without expression-text recovery.
- One sealed runtime-ABI subfact owns length, substring, bounded substring, and
  concat requirements. C and LLVM project and materialize those concrete
  symbols from the same receipt. The C String runtime reuses one copy of each
  exact body.
- Focused evidence: display-only facts are artifact-equal. Mutating `pergyra`
  to `pergyralang` changes both artifacts and executes exact `11`, `perg`, and
  `perg-yralang`. Bad result ABI, final/intermediate argument edges, argument
  type, and unregistered target all fail in both targets without artifact or
  retry through legacy local inventory.
- The final composition build took 128.3 seconds. The final window-builtin,
  nested-builtin, String concat/equality, and Bool regressions all passed on
  the final driver in 45.9 seconds. No memory pressure result is claimed. Full
  CI, current gen2==gen3, proofs, sanitizers, and released/default promotion
  did not run.
- Layering audit result: the general builtin route is no longer hidden behind
  the branch-String claimant, and general minimum GraphPlan readiness is no
  longer owned by an Array-reverse-named file. Argument chains, n-ary operands,
  expression identities, ABI identity/readiness, and C/LLVM window projection
  have named owners. No `helper` bucket, second plan/graph, backend MIR read,
  fixture route, count-output branch, or native fallback was added.
- All old hard caps stayed fixed. New owners are registered in the shared
  `scalar_program_owner_caps.tsv`; the central expression owners shrank to 90
  C and 135 LLVM lines, and admission shrank from 224 to 178 lines despite the
  new semantics. `agent_boundary_sentinel_smoke.sh` is green.
- Independent red evidence remains explicit. The component contract still
  stops at the pre-existing `ast_expression_graph_fact_owner.pgy` 616/600 cap.
  `scripts/sot_registry_gate.py` still rejects duplicate Coq fact authorities.
  These are not this executable rung's blockers.
- Classification is bounded `SUBSTITUTING` only for this exact String-window
  program through installed direct C and LLVM. It does not close arbitrary
  builtin arity/signatures, effectful short circuit, multiple callables,
  Float/Array locals, temporary String lifetime, or whole-compiler/default
  replacement.
- The bounded layering/duplication audit is recorded in
  `docs/self_hosted/18_self_host_layering_duplication_audit.md`. Immediate P1
  debt is the count-based direct backend dispatcher and the 616/600 semantic
  expression graph owner. Those are not independent cleanup tracks: fix them
  only when reached by the next executable rung or when they block its gate.
- Next executable falsifier is deliberately unselected. Probe current producer
  fixtures after this closure and select the first real dual-target failure;
  do not revive a historical queue item or infer completion from fixture order.
- Forbidden fallback remains a fixture/name branch, expression-text parsing,
  source-local spelling inference, precomputed output, a builtin-specific
  graph/emitter, widening every signature at once, backend MIR reads, claimant
  retry, or native C fallback.

### Previous routine-partitioned String program closure

- Executable checkpoint: `be376971f87149f0131a9e2893db6e4e9320608b`
  on `main`, with this handoff as its intended docs-only descendant. The final
  current-source Pergyra-built driver is 4,896,518 bytes with SHA-256
  `D1002D338C1A9F515C8B77D83EAF1DD5AA3C44434F70186FA8783B7221CDD33E`.
- Intended post-handoff dirty state contains only user-owned stdlib work:
  modified `docs/138_standard_library_scope.md` and
  `docs/148_stdlib_architecture.md`; untracked `stdlib/math.pgy`,
  `stdlib/pgy_math_registry.pgy`, and `tests/cases/stdlib_math_matrix/`. No
  compiler-rung file remains dirty.
- Closed executable rung: `src/self_hosted/codegen/fixture/string_equality.pgy`.
  Its 14,698-byte self-produced MIR has SHA-256
  `1A9D856F377CCF27424E72F19B535EE8431B737D1ED61FF868E3CB3DC6638228`.
  The same MIR emits 1,180-byte C
  (`E038A9C4029FF42246CD8183C3903E7F737993CFD1AB2B2E161B9D8122315162`)
  and 3,939-byte LLVM
  (`023600C2083186D8CB6AD56D633CF501DF2983FCA58A8C10CB1A2B6FDBFF38C6`).
  Both host-compile and execute exact `I`, `S`, `S`, `?`, `eq`.
- `DirectMirScalarCfgGraphPlan` v16 remains the sole CFG, SSA, phi, local,
  operation, routine-range, digest, and mutation authority. Main and
  `Kind(String) -> String` pass through the same per-routine admission owner,
  flat storage, canonical Main-first partition, and one seal. Direct calls join
  persisted callee syntax identity; String equality/inequality consumes the
  runtime ABI registry's `strcmp` fact. C and LLVM have target-specific syntax
  renderers but consume the same routine schedule and typed expression rows.
- The first mutable aggregate implementation was rejected even though it
  compiled: rebinding a growable Array through a copied nested struct did not
  update the enclosing length after reallocation. Value, block, operation, and
  routine-count storage are now layered persistent owners whose mutations
  return reconstructed values. This is an ownership correction, not a cache or
  allocation-limit workaround. Typed-expression append failure is an
  `Option<Int>` rather than a new `-1` sentinel.
- Focused evidence: display-only and admitted routine-order mutations are
  artifact-identical for both targets. Wrong call target, formal-parameter
  identity, String-comparison kind, return type, callable edge, and missing
  terminal return all fail without an artifact. Static gates require exactly
  two calls to the one routine admission owner, exactly one GraphPlan seal, no
  old callable-specific backend symbol, no backend MIR read, and hard LoC caps
  of 260 lines or less across the active new owner set. The previous Bool gate
  and reallocating Array parameter gate remain green on the same driver.
- Final current-source composition build took 143.9 seconds. The focused String,
  Bool, and Array-parameter gates took 34.6 seconds together.
  This session deliberately did not pressure-sample every focused gate; no new
  memory peak is claimed. Full CI, current gen2==gen3, prover suites, sanitizer
  suites, and public release promotion did not run.
- `documentation_quality_smoke.sh` passed. The proof-spine structure passed only
  under the repository's explicit missing-prover declaration; Coq/Rocq proofs
  were not checked on this host. The global Pergyra-likeness gate remains red:
  sentinel `239 > 22` and zone-bound steps `26 < 29`. The executable diff has
  equal added/removed likeness sentinel matches after the new append failure was
  converted to `Option<Int>`; the pre-existing global debt was not widened or
  misreported as this String rung's blocker.
- Classification is bounded `SUBSTITUTING` only for this exact two-routine
  String comparison/return/direct-call program through installed direct C and
  LLVM. Arbitrary routine counts/signatures, recursion, String concatenation,
  effectful short-circuiting, and whole compiler/default replacement remain
  open.
- Bounded hierarchy/duplication audit: the active new owner set tops out at 256
  lines and contains no generic `helper` filename. Repository-wide observation
  found one 876-line probe and many 570-627-line historical owners. Repeated
  tool-local names such as `InputErrorJson`, `FixturePathFromArgs`, and
  `ArrayContainsString` are candidates for an explicit policy-free utility
  owner, but name/LoC evidence alone does not prove a safe extraction. They are
  inactive inventory until an executable rung reaches their fact boundary.
- Next observed executable falsifier: only
  `src/self_hosted/codegen/fixture/string_equality_concat.pgy` is active. The
  current producer emits a 6,109-byte MIR with SHA-256
  `85E6A08A02F7C6DB568455793D7EF777847C17C9A56366782DFB38B6D8014538`.
  Both targets publish no artifact and stop at
  `direct MIR scalar CFG condition fact is invalid`; required execution is
  exact `concat_eq_ok`.
- Next objective card: extend the existing typed expression arena with the
  source-carried String-concatenation graph and runtime ABI fact, then consume
  it through the same single-routine GraphPlan condition and both current
  expression renderers. `mir.execution_graph` remains fact supplier and
  `CompileAdmittedDirectMirForTarget` the last legitimate consumer. The
  falsifying negative is a repaired/missing concat operand or ABI row that must
  fail in the claimed scalar owner before either artifact.
- Forbidden fallback: a `string_equality_concat` fixture/name branch,
  expression-text parsing, precomputed concatenated output, a concat-specific
  program plan/emitter, routine/block-count classification, backend MIR reads,
  claimant retry, or native C fallback.

### Previous collection return/parameter carriage

- Executable checkpoint: `9e33ec3730fc276921adc1311b834dfc8502c7d1`
  on `main`, with this handoff as its intended docs-only descendant. The final
  current-source Pergyra-built driver is 4,808,442 bytes with SHA-256
  `F33FE0C478519AA24F7277EA42B8E6C396246FCC507FF6389D9E3BA26A6E2ADD`.
- Closed executable rung: `src/self_hosted/codegen/fixture/array_param.pgy`.
  Its 18,849-byte self-produced MIR has SHA-256
  `54AA601DCD8031B7A2857F42A5FD3ABDAD2BBB02822BA3AF3537999210B88A9C`.
  The same MIR emits a 1,373-byte C artifact
  (`DD6B9C1EEE8BC717E85DF68CBCE6C148F242D106B7CD88B0A48DB72897D9DA2C`)
  and a 3,621-byte LLVM artifact
  (`DE6DA8F6C23A6A85716111AE4736E0DDABD0207AC7E4FFABD2465C4BEAB304E8`).
  Both host-compile and execute exact `12`, then `4`; a graph-owned `Build(5)`
  variant executes exact `20`, then `5`.
- One target-neutral `CollectionProgramPlan` now carries the routine-local
  `CollectionPlan` across `Build` return, `Main` call result, and `SumAll`
  formal parameter. Value origins and dynamic inputs are explicit, ValueIds
  are routine-qualified, all three views bind one canonical `Array<Int>` ABI,
  and one storage trajectory permits producer reallocation with exactly one
  entrypoint cleanup.
- Routing no longer treats `three routines + zero declarations` as an Array
  argument identity. The old ArrayArgument route is explicitly limited to its
  one-block/no-loop legacy shape. A coarse collection-program claim runs before
  that classifier; once claimed, malformed input fails in the new owner and
  cannot retry ArrayArgument. C and LLVM emitters consume only the sealed plan
  and cannot reopen admitted MIR.
- Focused evidence covers baseline, `Build(5)`, display-only mutation, cyclic
  routine order, and a deliberate cross-routine raw-ValueId collision, plus
  repaired parameter ABI, call-target, return-use, and cross-routine endpoint
  negatives for both targets. Every negative publishes no artifact and does
  not report the legacy ArrayArgument diagnostic.
- Observed timings after the final source split: current-source driver build
  158.5 seconds; focused gate 7.7 seconds; old ArrayArgument regression 8.2
  seconds; indexed-assignment regression 17.7 seconds; component/removed-path
  contract 104.5 seconds; hard contract 17.4 seconds. Routine gates were not
  memory-sampled. Full integration, current gen2==gen3, proof suites, and full
  CI did not run. ASan/UBSan execution was unavailable because this Windows
  toolchain lacks the required sanitizer runtime libraries; it is not recorded
  as a pass.
- Classification is bounded `SUBSTITUTING` only for one reallocating
  `Array<Int>` producer, one entrypoint receipt, and one reducer parameter.
  Multiple collections, aliasing, escaping ownership, arbitrary reducers,
  ownership-sensitive elements, reserve policy, and general call graphs remain
  open. The independent SoT registry gate remains red only at its pre-existing
  duplicate Coq fact-authority conflict.
- Next observed executable falsifier: only
  `src/self_hosted/codegen/fixture/bool_logic.pgy` is active. The current
  producer emits a 17,188-byte MIR with SHA-256
  `77A60A6644C7D4BFE4B805D40306CCC90BD6F1612E2988CF4E2051BB4C6C1612`.
  C and LLVM both publish no artifact and incorrectly stop at
  `direct MIR returned Array<Int> foreach program is invalid`.
- Next objective card: admit the existing typed Bool literal, logical-not,
  short-circuit and direct `Int -> Bool` call/CFG facts through one scalar
  program plan. `mir.execution_graph` remains the semantic fact supplier and
  `CompileAdmittedDirectMirForTarget` the last legitimate consumer. The first
  falsifier is that this non-Array program must not be claimed by any returned-
  collection route; a malformed Bool/CFG fact must fail with the Bool/scalar
  owner diagnostic before either artifact.
- Forbidden fallback: a `bool_logic` fixture branch, expression-text parsing,
  precomputed `flag-on/other-off/and/logic/grouped/0/2/4`, block-count routing,
  retry through returned-Array or Option claimants, backend MIR reads, or the
  native C compiler path.

### Previous bounded indexed collection operations closure

- Executable checkpoint: `6bdc207d4d4c0b7c76809f847cf7acfbbc688619`
  on `main`, with this handoff as its intended docs-only descendant. The final
  current-source Pergyra-built driver is 4,740,820 bytes with SHA-256
  `226E98CEC879BB10125F230EEDB3C8ADD619800020D3E372CB96E18E01F6257B`.
- Closed executable rung:
  `src/self_hosted/codegen/fixture/array_index_assign.pgy`. Its 10,338-byte
  self-produced MIR has SHA-256
  `7043BFBA70252CC058D0E2B7B826796254D8F14B6D6C5358A09868DA554EABFA`.
  The same MIR emits a 1,168-byte C artifact
  (`483FE6F969C76992FA229ADB3126CF2639CF78A42BA2D642A1026F890DD7DDD2`)
  and a 5,855-byte LLVM artifact
  (`8EBE410A5D690373C7E8F7ED7E6B9C99C2B1BA288D8E1B1F7A1F1FCB02E66D15`).
  Both host-compile and execute exact `13`, then `zb`.
- `GraphPlan` schema v13 now embeds one target-neutral `CollectionPlan` with
  collection SSA versions, stable storage identity, predecessor rows,
  canonical ABI fields, and ordered `Initialize`/`Get`/`Set` operations. An
  exclusive selection owner prevents a claimed malformed indexed assignment
  from retrying the older String or ArrayInt plans. C and LLVM consume only
  the sealed plan; neither reads MIR JSON nor calls private set helpers.
- This is not general collection closure. Storage identity, versioning, and Set
  projection are reusable facts, but graph operations 23/24 still encode the
  admitted integer-sum and two-value String-concat observations. The embedded
  collection row is `BRIDGE/ACTIVE`; push, reserve, alias invalidation,
  reallocation, cleanup, arbitrary observations, parameters, and returns have
  not migrated. No topology-specific `IndexAssignmentProgramFact` landed.
- Focused evidence: eight positive/semantic variants, including coherent
  ValueId renumbering, reordered groups, overwritten source display, and
  signed-i32 overflow, plus 21 no-artifact negatives per target exited 0 in
  19.9 seconds. ArrayPop, ArrayMax, ArraySum, and String-mutation sibling gates
  remained green in 59.7 seconds. The component/removed-path ratchet exited 0
  in 95.4 seconds. The current-source driver build took 123.3 seconds; the
  unchanged composed AST reused its fingerprinted driver in 6.5 seconds.
- The real top-level scalar-CFG/AIR integration exited 0 in 275.534 seconds and
  reached the final indexed-assignment success row. Memory was sampled only at
  this integration boundary: the process tree peaked at 0.375 GiB working set
  and 0.328 GiB private memory, below the 2.4 GiB attention and 3 GiB stop
  limits. LLVM host compilation emitted only target-triple override warnings.
- No line cap was raised. The 492-line admission responsibility was split into
  a named 86/100 source owner and a 448/460 exact admission owner; the route is
  30/30. Readiness now proves every observation Get is claimed exactly once
  and every GraphPlan operation row has exactly one collection claimant. The C
  preamble emits each admitted collection typedef once.
- Classification is bounded `SUBSTITUTING` only for the exact nonescaping local
  literal Int/String sources, two static literal indexed writes, and the
  admitted sum/concat observations. Full CI, public matrices, proof suites,
  released-driver promotion, and current gen2==gen3 did not run.
- Known independent reds remain explicit. `python scripts/sot_registry_gate.py`
  exits 1 at the pre-existing `registry contains duplicate Coq fact
  authorities`. The old `one_mir_array_int_projection.sh` stops before
  behavior because its local cap is 160 while the unchanged ABI owner is 173
  lines; the central component owner cap is 180. Neither cap was changed in
  this rung.
- Next objective card: only `src/self_hosted/codegen/fixture/array_param.pgy`
  is active. The current producer emits an 18,849-byte MIR with SHA-256
  `54AA601DCD8031B7A2857F42A5FD3ABDAD2BBB02822BA3AF3537999210B88A9C`.
  Both targets currently reject before publication with
  `direct MIR Array argument program envelope is invalid`; expected execution
  is exact `12`, then `4`.
- Objective and owner boundary: reuse the routine-local `CollectionPlan` and
  add only a generic program-level receipt for parameter, call, return, and
  storage-carriage facts. Routine-qualified ValueIds must preserve the storage
  trajectory `Build return -> Main xs -> SumAll xs`; the existing GraphPlan and
  ABI owners remain fact suppliers, and `CompileAdmittedDirectMirForTarget`
  remains the last legitimate consumer. The first falsifiers are a stale
  parameter ABI, wrong call target, or broken return edge, each with no C or
  LLVM artifact.
- Forbidden fallback: an ArrayParam-specific planner or three-routine topology
  classifier, constant-folded `12/4`, backend MIR reads, retry through the old
  ArrayArgument envelope, or a fixed no-reallocation storage assumption.

### Previous fresh bounded `ArrayReverse` closure

- Executable checkpoint: `5877398b60f2d725f296407c910b4c50fd16b5ec` on
  `main`, with this handoff as its intended docs-only descendant. The final
  current-source Pergyra-built driver is 5,233,261 bytes with SHA-256
  `5EA829E9E344BA5770FC4D7BB243F0619944DB03033A9F01CB1F1C0D8AFD4798`.
- Closed executable rung: `src/self_hosted/codegen/fixture/array_reverse.pgy`.
  Its 7,260-byte self-produced MIR has SHA-256
  `7D4FDBA6CC28906541B406DC508FE6C0870DC8B5DB638C2A41787FF711C8F64D`.
  The same MIR emits a 1,136-byte C artifact
  (`6ED02FD672F59C9D7DEA890BE563004713FFD150E059A6A8D3F2D664F2477B30`)
  and a 5,133-byte LLVM artifact
  (`151E297EE76A0D357D1D5869AA138F3E3A241091D27587210D4A4AB55D4AB0C3`).
  Both host-compile and execute exact `3`, `2`, `1`, `3`.
- The one target-neutral Array program receipt now owns four mutually exclusive
  modes: dynamic push, initialized static set, initialized read-only maximum,
  and bounded fresh reverse. The reverse receipt seals distinct source/result
  definition, ValueId, source-local, storage, and canonical four-field ABI
  identities; no reverse-specific top-level planner exists.
- C and LLVM allocate separate source/result storage, load source positions
  `2,1,0`, store result positions `0,1,2`, and copy current source length only
  after the result stores. Neither target mutates or aliases source storage,
  reopens MIR, precomputes `[2,1,3]`, or calls the incompatible legacy
  `pgy_ai_reverse` helper.
- A final read-only audit caught two P1 ownership defects before commit. Exact
  reverse validity had doubled as the route claim, allowing malformed call
  targets/edges to retry the legacy one-block local-inventory route. The route
  now coarsely claims every `Array<Int>`-valued call graph or invalid expression
  sequence, and exact admission alone decides acceptance. Operation 20 was also
  missing from absent-program readiness; one named inventory now rejects every
  `Array<Int>`-owned operation without a program receipt.
- Focused evidence: baseline plus alternate `4,-5,6`, display/ValueId-renumber
  artifact equality, and 20 C/LLVM no-artifact negatives passed in 33.3
  seconds. Push, sum/set, and maximum sibling regressions passed in 157.7
  seconds; component/removed-path contracts passed in 269.6 seconds; the final
  cumulative CFG/AIR graph reached its final success row in 647.4 seconds.
- Memory was sampled only at the final cumulative boundary. The 500 ms process-
  tree sample observed 0.361 GiB peak working set and 0.316 GiB peak private
  memory, below the 2.4 GiB attention and 3 GiB stop thresholds. This is test-
  inclusive integration evidence, not ordinary self-host compile latency.
- No existing line cap was raised. Saturated mixed responsibilities were split
  into named owners; the new transform route fact is 19/25 lines, operation-
  absence owner 25/30, reverse route 37/40, and final program readiness 147/155.
  Classification is bounded `SUBSTITUTING` only for the exact local three-
  element fresh-reverse shape. Full CI, public matrices, current gen2==gen3,
  released-driver promotion, and proof suites did not run.
- Known independent red: `python scripts/sot_registry_gate.py` still exits 1 at
  the pre-existing `registry contains duplicate Coq fact authorities`. This
  rung did not add or weaken that conflict.
- Next objective card: only `src/self_hosted/codegen/fixture/array_pop.pgy` is
  active. The current driver produces a 14,007-byte MIR with SHA-256
  `AB92771EA08C27C73EDAB26E06441BF88CC3843C64FEFD784156694D321F73D4`.
  Both direct targets reject before publication with
  `direct MIR String array source plan is invalid`.
- Objective: close one program-level multi-collection source identity reached
  by the two `ArrayPop` effects. Priority is shared source identity and current-
  length semantics, owner-directed effect/result facts, fallback exclusion,
  then exact C/LLVM parity. The existing typed collection/ABI and scalar-CFG
  GraphPlan owners supply facts; `CompileAdmittedDirectMirForTarget` remains the
  last legitimate consumer. The first falsifier must distinguish both pop
  effects and reject stale or cross-collection receivers before publication.
- Forbidden fallback: fixture/text routing, partial Int-only success, a second
  collection planner, backend MIR reads, native helper retry, capacity as
  current length, or trying an older plan after the named owner rejects.

### Older checkpoint archive

### Previous read-only `Array<Int>` maximum closure

- Executable checkpoint `4be9ac088bc870f15b92d934c793700b74e6abeb`
  closes `array_max.pgy` with exact `9`, first/last/all-negative variants, 23
  no-artifact negatives, and one three-mode Array program receipt. It is now
  historical input to the four-mode receipt above.

### Previous initialized `Array<Int>` sum/set closure

- Executable checkpoint `e3423436995409438f1044570d5af4fb10b760d2`
  closes `array_sum.pgy` in direct C/LLVM with exact `60`, `99`, `3`, four
  semantic variants, and 35 no-artifact negatives. Its one-receipt two-mode
  design is now historical input to the three-mode program owner above.

### Previous bounded `Array<Int>` loop push closure

- Executable checkpoint: `bb1ad63ab2b7c03e108256527044360acb28bdef` on
  `main`, with this handoff as its intended docs-only descendant. The
  current-source Pergyra-built driver used for final evidence is 4,566,764
  bytes with SHA-256
  `F167FE76B3009CCDE5A1CD937B9655CD7A5C828E478505EADF62746CB7C4A73E`.
- Closed executable rung: `src/self_hosted/codegen/fixture/array_push.pgy`.
  Its 13,824-byte self-produced MIR has SHA-256
  `AE1427524AA69F54A180C99BB6F5D3678C5C95353F7BBC74963197787AE82220`.
  The same MIR emits a 1,306-byte C artifact
  (`382B455E0FF3F3C59E956EBDABCEB12E7EF82CA3AA63DABDACD42F0A08EC4A97`)
  and a 3,906-byte LLVM artifact
  (`DC78108CF6CB5953D2840D17AAD5615D7ECF4FCB09FE9E265BDEFF2CE882BB5B`).
  Both host-compile and execute exact `30` then `5`.
- The failure was architectural rather than syntactic: scalar local inventory
  had no collection claimant for the same typed `xs.1` identity. The fix did
  not classify `Array<Int>` as a scalar and did not add a fixture or block-count
  route. One target-neutral scalar-CFG `GraphPlan` now owns the empty local,
  canonical ABI, producer and consumer loop induction, `i * i` pushes, current
  length, indexed accumulation, and final length.
- Shared CFG responsibilities now live in responsibility-named owners for
  instruction graph/position, while induction, array guard dominance,
  collection expressions, length logs, and plan sealing. The older String
  paths delegate to them. These files compose one compiler program graph; they
  are not independent compiler copies or a policy-bearing helper subsystem.
- C uses bounded public four-field `PgyArray_Int` storage. LLVM mutates one
  four-field object, explicitly truncates the admitted scalar value to the
  canonical 32-bit element, and sign-extends reads. Both store before advancing
  length and use current length for the consumer guard and final log. Neither
  target reopens MIR or calls a native/runtime push fallback.
- Admission pins unique source-local identity and ABI, exact zero/unit producer
  and consumer induction, CFG topology and dominance, one push per producer
  backedge, accumulator phi recurrence, and a producer bound from 1 through
  46,340. Display and phi-order mutations are artifact-identical; graph and
  bound mutations change execution. Twenty-three receiver/use/graph/route/
  bound/edge/step/length/read/local/ABI negatives publish no artifact and do
  not retry a legacy route.
- Observed evidence: composed source check 8.4 seconds; current-source driver
  build 126.4 seconds; focused parity 29.6 seconds; indexed and mutation String
  regressions green; component/removed-path ratchet 239 seconds; cumulative
  scalar-CFG integration 326.321 seconds. The cumulative runner reached the
  new exact-parity message and exited 0. LLVM emitted only target-triple
  override warnings during host compilation.
- Memory was sampled only for that final cumulative boundary. The
  `array-int-loop-push-final-cumulative-20260804` run peaked at 0.489 GiB
  working set and 0.896 GiB private memory. Neither the 2.4 GiB attention
  threshold nor the 3 GiB hard stop fired.
- Fixed caps remain in force. Notable exact or near caps are graph identity
  100, graph readiness 240, typed readiness 177/180, graph admission 444/450,
  C operation emission 128/130, and LLVM operation emission 190/190. The
  component ratchet also pins every new responsibility owner and tightens the
  now-thin String wrapper caps; no cap was raised.
- Classification: bounded `SUBSTITUTING` for this exact nonescaping local
  `Array<Int>` dynamic loop-push/indexed-sum shape in both installed direct
  targets. It is not generic growth, reallocation, arbitrary push control flow,
  aliases, parameters, returns, arbitrary element types, or whole-compiler
  replacement.
- The independent Hacker News observation that language consistency is easier
  to steer than compiler architecture is recorded in
  `docs/131_ai_coding_atomic_units.md` as anecdote, not benchmark evidence. The
  local evidence is stronger and narrower: syntax was coherent while LocalRef
  ownership, evidence lifetime, last consumers, and fallback exclusion needed
  explicit architecture gates.
- Known independent reds: `python scripts/sot_registry_gate.py` still stops on
  the pre-existing duplicate Coq fact-authority rows, and the previously
  observed scaffold gate stops because
  `src/self_hosted/tools/expression_graph_persisted_read_probe/` has no
  `intent.md`. This rung did not modify or weaken either conflict. Full CI,
  public matrices, current gen2==gen3 fixpoint, and proof suites did not run.
- Next objective card: only `src/self_hosted/codegen/fixture/array_sum.pgy` is
  active. The current driver produces a 12,383-byte MIR with SHA-256
  `1496AA7537842ED4AECA0E417A3F0FE362E1A908147FD68FA4C7CE80087E7735`.
  Both direct targets currently fail before publication with
  `direct MIR scalar CFG Array<Int> definition is invalid`.
- Objective: reuse the canonical static `Array<Int>` definition and the shared
  while/guard/index owners for exact `60`, then admit the existing in-bounds
  `ArraySet(xs, 1, 99)` and execute exact `99` and `3`. Priority is collection
  identity and current-length ownership, one-plan reuse, old-route rejection,
  then C/LLVM parity. Existing Array ABI, collection expression, loop, guard,
  operation, ValueId, and LocalRef facts are the fact owners; the last
  legitimate consumer remains `CompileAdmittedDirectMirForTarget`.
- Forbidden fallback: fixture routing, a second Array compiler, backend MIR or
  graph reads, precomputed output, capacity-as-length, native/runtime escape,
  per-operation whole-program validation, or weakening the closed dynamic-push
  plan. First falsifiers are static definition identity, guard/read binding,
  ArraySet receiver/index/value, mutation ordering, current length, ABI, and
  no-artifact/no-retry behavior.

### Older historical checkpoints - mixed foreach and indexed-String setup

- Executable checkpoint: `c91def868b8c7d45dbb9cc4e212dad2b6095bcd2` on
  `main`. This handoff is its intended docs-only descendant. The installed
  current-source sibling is 4,441,513 bytes with SHA-256
  `A7AAD0A55D6F936E2CE2C7B45916EE4CAA43CFB00DC33F18729368E6DD11110A`.
- Closed executable rung: `src/self_hosted/codegen/fixture/for_each.pgy`. Its
  14,425-byte self-produced MIR has SHA-256
  `1D0771BFE62C6C20A5E671A82F0A0DD956A198D4495F08B43D5D86303DD40397`
  and one typed scalar-CFG plan executes sequential `Array<Int>` and
  `Array<String>` foreach loops through both installed direct targets.
- Final artifacts are 1,996-byte C
  (`BA6830C22D3CD427B07A63196DF7ED30F2CD3370EE9D78D153DB827FE1265D35`)
  and 6,962-byte LLVM
  (`D1BAEE876843D8112A5C3BA4F299C95AC0F4F87BE16738CFD561F63139DE67C2`).
  Both host-compile and execute exact `60` then `abbccc`; graph-only String
  payload mutation executes `xyyzzz` in both targets.
- One scalar type-family owner now classifies `Int`/`String` and their admitted
  Array iteration pairs. Element-neutral array-layout and literal-spine owners
  feed canonical `Array<Int>` and `Array<String>` ABI owners. A
  loop-syntax-keyed companion receipt owns element type and String pool while
  the existing primitive foreach receipt continues to own topology, storage
  identity, length, cursor, and binder identity.
- The sealed plan carries local/result types, typed String literal/copy/phi/
  concat/log operations, the companion foreach receipt, and one selected
  concat runtime ABI identity. C and LLVM operation/foreach emitters consume
  only these receipts. Stable public consumer names remain thin delegates; no
  second String foreach compiler or backend graph reader exists.
- The focused gate pins exact C/LLVM execution, graph-only String changes,
  iteration-order and display-`expr0` byte equality, and thirteen independent
  type, ABI, array-spine, concat-target/edge, LocalRef, and stale-use negatives.
  Invalid claimed programs publish no artifact and never retry Option match.
- The cumulative gate exposed and closed two compatibility seams: typed
  readiness now accepts an already-admitted `AddInt` literal slot without
  reparsing text, and a zero-foreach C plan preserves its previous preamble
  bytes. Single/nested range LocalRef negatives now pin their exact range/local
  inventory owner diagnostics rather than one stale generic message.
- Follow-up `c91def86` removes every sentinel introduced by the mixed rung.
  The C `Array<String>` projection now consumes the element-neutral storage
  owner's absent-index facts instead of comparing four duplicated `-1` values,
  and String element-start lookup returns `Option<Int>` rather than `-1`.
- Final observed evidence after that follow-up: current-source installed build
  124.5 seconds; mixed focused gate 14.0 seconds; corrected current-driver
  cumulative CFG integration 307.8 seconds; final structural component/removed-
  path gate 262.4 seconds. Route is 62/70, foreach set remains 320/320, graph admission
  remains 449/450, typed C is 221/240, typed LLVM is 219/220, and typed
  readiness is 154/180. No existing cap increased.
- Full CI, current gen2==gen3 fixpoint, public driver matrices, pressure
  integration, and Coq/Rocq did not run. Memory was not sampled per focused
  gate. The last accepted final-integration policy remains 2.4 GiB attention
  and 3 GiB hard stop.
- `self_host_pergyra_likeness_smoke.sh` remains known red and was not rebased:
  sentinel debt is 44/22 and compiler-zone-bound steps are 26/29. The mixed
  follow-up reduced the observed sentinel count from 49 to 44, while the other
  metrics include `core_string_munge=78/79`, `ast_string_surface=0/0`, and
  `result_use=3542/2254`. This cross-owner historical debt is not the active
  indexed-String executable rung.
- Classification: bounded local-literal `Array<Int>`/`Array<String>` foreach
  plus the earlier pure returned `Array<Int>` composition are `SUBSTITUTING`
  in installed C and LLVM. Arbitrary element ABI, indexed aggregate reads,
  effectful collection mutation/producers, and whole-compiler replacement
  remain open.
- Next objective card: `src/self_hosted/codegen/fixture/str_array_concat.pgy`
  is the only active rung. Its 7,709-byte self MIR has SHA-256
  `DBE3B8FF0D4DCFBF69A10A1D416BA08AA165C173416478CB4E61560BD428DAEE`
  and should execute exact `xyz`. Both direct targets currently fail before
  artifact publication with `direct MIR CFG single-node literal graph is
  invalid`.
- Priority is exact route ownership, canonical `ArrayLength(parts)` bound fact,
  dynamic `parts[i]` String element receipt, then reuse of the existing String
  concat/local/result plan. Fact owners are persisted expression graphs,
  `Array<String>` ABI, range iteration/LocalRef receipts, and typed ValueIds.
  The last legitimate consumer is `CompileAdmittedDirectMirForTarget`.
- Forbidden fallback: fixture-name or block-count routing, source/`expr0`
  reparsing, treating array capacity as length, backend-specific index
  reconstruction, a second range compiler, weakening the older direct-CFG
  claimant, or adding responsibility to the full foreach-set/graph-admission/
  typed-LLVM owners. The first gate must mutate length/index graph, ABI,
  ValueId, and LocalRef facts and prove no legacy claimant retry.
- Memory remains final-integration-only: attention at 2.4 GiB and hard stop at
  3 GiB. Do not add a cache, shard, worker, or per-test pressure loop before
  identifying a repeated owned operation at the reached boundary.

## Historical checkpoint archive — returned-array foreach composition

- Executable checkpoint: `6122051f` on `main`; this handoff is its docs-only
  descendant and should be clean after commit/push. `bin/pgy.exe` remains
  4,626,988 bytes, SHA-256
  `39798EA50105C9B48F26AE2FCABDB400B54AC153D15DC440B089C4E5E6402F9E`.
  The rebuilt and smoke-verified Pergyra sibling is 4,384,945 bytes, SHA-256
  `F29AC44465FB3B0B576C6716C735294232FEE5C1982CCE334CFA82684331F7B8`.
- Closed executable rung: `foreach_array_int_sum.pgy`. Its 6,761-byte
  self-produced MIR has SHA-256
  `34E49B953F380D3B3909A96FA4D266575C8A351497D1F000DC68468598C9D23E`.
  The source carries one local literal `Array<Int>`, one `Int` binder, a
  mutable total, one header phi, and an exact four-block CFG.
- That immutable MIR now produces a 913-byte C artifact
  (`2AF700B1022F5CA37D45B64E62F3C972C17084EEA89EF4551E70903AEA3EA1C5`)
  and a 3,121-byte LLVM artifact
  (`301654DB872A8FC2F6F133088A7636D38C7C75CE47A4DB24A168B6608828708E`).
  Both compile and execute exact `6`.
- `DirectMirScalarCfgForEachFact` is the target-neutral receipt. It joins the
  loop candidate, typed iteration row, collection ValueId and dominating
  definition, exact Array ABI, persisted literal graph, binding LocalRef,
  header/body/exit, and backedges once. The collection stays outside scalar
  local storage; only the binder obtains a scalar local slot.
- C and LLVM consume that same receipt for storage, ABI-owned length, data
  access, cursor, binder load, and latch increment. There is no `expr0` read,
  block-count route, capacity-as-length guess, hardcoded element count,
  backend collection-protocol reconstruction, or legacy range retry.
- The focused gate pins exact `6`, phi-input permutation byte equality, and a
  graph-only `[4,5]` mutation that executes exact `9` while display `expr0`
  remains unchanged. Twelve ABI, type-row, ValueId, graph, int32-bound, phi,
  and CFG negatives reject both targets before artifact publication.
- Fixed caps are green without increases: foreach fact 184/200, set 274/320,
  append 80/120, admission 264/280, local binding 150/180, graph readiness
  64/100, C projection 151/180, LLVM projection 187/220, general graph
  admission 445/450, C emitter 175/180, LLVM emitter 190/260, loop-flow
  admission 40/40, direct-local operand 40/40, and focused gate 103/180.
- Latest observed evidence: current-source installed build 128.7 seconds;
  focused foreach C/LLVM gate 14.6 seconds; existing scalar/range regressions
  11.0–14.9 seconds; public installed LLVM 22.2 seconds; current-driver
  cumulative CFG/AIR 209.5 seconds; structural component body 232–248 seconds.
  The final monitored component body printed its green sentinel with peak
  working 79,863,808 bytes (0.074 GiB) and private 46,170,112 bytes
  (0.043 GiB); 3 GiB hard stop did not fire. The monitoring wrapper itself did
  not recover the already-exited process code, so do not promote that wrapper
  invocation to an exit-0 claim.
- Classification: this identifier-backed local-literal `Array<Int>` foreach is
  bounded `SUBSTITUTING` in installed C and LLVM. It is not returned-array call
  composition, nested/sequential collection foreach, arbitrary element ABI,
  general foreach, whole-compiler replacement, or a current gen2==gen3 claim.
- Next objective card: `src/self_hosted/mir_lower/fixture/for_each_call.pgy` is
  the only active rung. The installed Pergyra producer emits a 17,155-byte MIR,
  SHA-256
  `F569D00CA64B92042203160511B969B2F695C12EE0EF884EF3C7BA489F269958`,
  with `MakeValues() -> Array<Int>`, nested outer/inner foreach loops, and a
  trailing foreach. Both direct targets fail before publication with
  `direct MIR Array<Int> return program envelope is invalid`.
- Priority is returned-array producer identity and call-result ValueId,
  composition with the existing foreach receipt, nested/sequential CFG
  ownership, then one C/LLVM integration gate. Fact owners are the existing
  Array return plan, call/use graph, typed iteration rows, and scalar-CFG
  collection receipt. The last legitimate consumer is
  `CompileAdmittedDirectMirMultiRoutineForTarget`; the over-broad competing
  claimant is `DirectMirArrayReturnProgramCandidate` when the two-routine
  program is not a bounded return-only consumer.
- Forbidden fallback: source/call expression reparsing, broadening the Array
  return candidate to swallow invalid main graphs, a fixture/topology-specific
  call-foreach compiler, block/routine-count semantics, backend-specific call
  composition, materializing the returned collection three times, or retrying
  another multi-routine family after a claimed invalid plan.
- The first unqualified cumulative CFG invocation still selects a stale
  `.tmp/.../driver_seed.exe` and fails at `unknown source MIR pressure token`.
  The same gate with `PGY_SELFHOST_ONE_MIR_DRIVER_BIN` bound to the installed
  sibling is green. Full CI, full bootstrap, current-source gen2==gen3, and
  Coq/Rocq did not run. `doc_link_checker_parity.sh` did not reach its link
  scan: the public C compile returned nonzero with an empty compile log; its
  cause is `Unknown` and this rung did not broaden into that toolchain repair.
  Memory remains final-integration-only: attention at 2.4 GiB and hard stop at
  3 GiB.

## Historical checkpoint archive — inactive evidence

### Previous pre-foreach snapshot

- Executable checkpoint: `2184c651` on `main`; this handoff is a docs-only
  descendant and should be clean after its commit/push. `bin/pgy.exe` remains
  4,626,988 bytes, SHA-256
  `39798EA50105C9B48F26AE2FCABDB400B54AC153D15DC440B089C4E5E6402F9E`.
  The rebuilt and smoke-verified Pergyra sibling is 4,341,923 bytes, SHA-256
  `F9947165C26884CEAE8A4E427BE2FB3B7ECCACB1291921F9D66A34A7B1486154`.
- Closed executable rung: `nested_iteration_continue_shadow.pgy` carries an
  outer declaration, two same-spelling integer range binders, an inner
  `continue`, and reads after both lexical boundaries. Its 8,363-byte
  self-produced MIR has SHA-256
  `A615CC9CE932217B5E1D752F37875C4167CA505CC6ABC6B2DF3BFAD145665130`.
  The exact nine-block CFG keeps the inner continue and normal fallthrough on
  inner header 3, then the outer latch on header 1.
- That one MIR produces an 898-byte C artifact
  (`2A0510677CB839653F6389EC3B0BA249AA99602D13F611CC618A8C3B62965E6C`)
  and a 2,368-byte LLVM artifact
  (`25A0A1945C252AF6242B2A1CBC814838DA48462A5E8FFA365D08DB4458B0B78A`).
  Both execute exact `0,2,0,0,2,1,40`; independent iteration-type and
  loop-flow row permutations remain artifact-equal.
- The positive program already executed before this change, but three
  falsifying mutations were incorrectly accepted by both direct backends:
  condition use of the outer same-spelling ValueId, inner `continue` retargeted
  to the outer header, and inner normal fallthrough retargeted to that header.
  Therefore output parity alone was not sufficient evidence.
- `DirectMirScalarCfgRangeTransfersReady` now proves each range backedge is
  owned by the innermost active range receipt. Wire-scope admission rejects an
  outer same-spelling SSA use while the inner binder is active. These decisions
  are target-neutral and both C and LLVM consume the same admitted graph; there
  is no backend scope reconstruction or nested-loop topology compiler.
- The focused negative gate also pins exact locals, syntax identities,
  instruction kinds, CFG successors, LocalRefs, range increment counts,
  permutation equality, stable failure diagnostics, and no-artifact behavior.
  Existing single/nested range gates remain in the cumulative integration hook.
- Fixed caps are green without increases: range transfer 53/90, graph admission
  394/450, wire range-scope admission 147/150, focused nested-continue gate
  105/160, cumulative hook 47/240, and public hook 60/70. Manifests remain 285
  driver, 31 core MIR, and 2 examples.
- Latest observed evidence: current-source installed build 119.0 seconds;
  focused nested-continue C/LLVM gate 6.4 seconds; cumulative CFG/AIR integration
  178.5 seconds; public installed LLVM 32.8 seconds; structural component and
  removed-path ratchet 235.7 seconds. All are green.
- Classification: innermost range ownership for nested same-spelling
  `continue` and fallthrough transfers is bounded `SUBSTITUTING` in installed C
  and LLVM. This is not general foreach, arbitrary collections, whole-compiler
  replacement, or a current gen2==gen3 claim.
- Next objective card: `foreach_array_int_sum.pgy` is the only active rung. The
  installed Pergyra producer emits a 6,761-byte MIR, SHA-256
  `34E49B953F380D3B3909A96FA4D266575C8A351497D1F000DC68468598C9D23E`,
  for `Array<Int>` values and a carried `Int` sum. Both direct C and LLVM fail
  before publication with `direct MIR range CFG block inventory is invalid`.
  Priority is producer-carried collection identity, ABI, bounds, index, and
  element facts, then one target-neutral scalar graph and C/LLVM parity. The
  fact owners are the typed iteration rows plus Array ABI/storage owners; the
  last legitimate consumer is the scalar-CFG plan. Forbidden are a
  topology-specific foreach compiler, block-count routing, source/expression
  reparsing, legacy range retry, or target-specific Array guesses.
- Rust's accepted 2026 `Move`/`Forget` direction and the bounded Pergyra
  comparison are recorded in `docs/106_ownership_model_comparison.md`. It is
  inactive research context for this rung: Pergyra has stronger owner/evidence
  composition in covered structured slices, but no general arbitrary-type
  `!Move` or `!Forget` equivalent may be inferred from that design direction.
- Full CI, full bootstrap, current-source gen2==gen3, and Coq/Rocq did not run.
  `tests/sot_authority_edge_smoke.sh` remains independently red on pre-existing
  duplicate Coq fact-authority rows; this rung did not broaden into that repair.
  Memory remains final-integration-only: attention at 2.4 GiB, hard stop at
  3 GiB, and only a final pressure result is recorded.

### Earlier checkpoint archive

### Previous iteration-binding scope snapshot

- Executable checkpoint: `aefebe13` on `main`, one commit ahead of
  `origin/main` before this documentation update. `bin/pgy.exe` remains
  4,626,988 bytes, SHA-256
  `39798EA50105C9B48F26AE2FCABDB400B54AC153D15DC440B089C4E5E6402F9E`.
  The current Pergyra-built sibling is 4,285,412 bytes, SHA-256
  `1A2BDC16E2C84C102B4C0698D3CEECB39221290CB87F442A1D723EBF78DFE091`.
- Closed executable rung: `examples/break_continue.pgy` produces one
  7,796-byte, eight-block MIR, SHA-256
  `B63639FD56B440D3B5B68E94249124E8F159127A7516F25295722932F5C036DD`.
  Header `sum.3 = phi(sum.1, sum.3, sum.9)` binds the preheader, block-5
  continue, and block-6 fallthrough predecessors. Both backedges target header
  block 1; the break targets exit block 7 and the final Log uses `sum.3`.
- The same MIR produces a 732-byte C artifact
  (`AA8FF900CAB41160BC1C817B0A659543AB71C305B24E549C60D7E9FC064EB530`)
  and a 1,950-byte LLVM artifact
  (`275C91A329195644439D0CBEF649491A53306338C579FC222A510DD74041D158`);
  both execute exact `42`.
- `routine_local_predecessor_snapshot_owner.pgy` now owns exact predecessor
  and local-version snapshots for break, continue, and fallthrough transfers.
  `routine_loop_header_backedge_binding_owner.pgy` validates each captured edge
  before binding its version. The former break-specific owner and the `for`/
  `while` post-lowering CFG rescans are removed.
- The general scalar CFG route admits `AST_CONTINUE` only as a use-free,
  unconditional edge to a dominating loop header. Range admission consumes all
  sealed backedge predecessors. Incoming permutation is artifact-equal;
  missing, stale, and retargeted continue rows reject before C/LLVM publication.
- Fixed caps remain green: predecessor snapshots 82/90, header preparation
  37/100, header binding 68/80, loop exit 83/90, for 170/180, while 118/130,
  graph admission 407/450, range receipt 269/280, and focused gate 109/160.
  No cap increased. Manifests own 285 driver rows, 31 core MIR fixtures, and 2
  example MIR fixtures.
- Latest observed gates: current-source driver rebuild 109.2 seconds; focused
  continue C/LLVM gate 6.7 seconds; prior for-break plus break/repeated-break
  gates 16.5 seconds; cumulative CFG/range integration 151.9 seconds; public
  LLVM file/stdout 15.1 seconds; full structural component/removed-path ratchet
  225.4 seconds.
- Classification: the bounded unique range binding with one reachable continue,
  one fallthrough latch, one break, and one outer mutable `Int` is
  `SUBSTITUTING`. This is not general foreach, nested/multiple loops, scoped
  iteration binding, arbitrary multi-phi, or whole-compiler replacement.
- Active objective card: make iteration binding identity survive same-spelling
  outer locals. Priority is semantic binding identity, producer active-scope
  restoration, target-neutral LocalRef identity, C/LLVM parity, then negative
  rejection. The source fact owners are the typed iteration row and complete
  source-local inventory; `routine_for_owner.pgy` owns active MIR scope, and
  `DirectMirScalarCfgRangeIterationFact` is the last legitimate consumer.
- Observed next falsifier: a temporary program declares outer `i: Int = 40`,
  runs `for i in 0..3`, then logs `i`. Native and self producers both emit two
  source-local rows named `i` but bind the body and post-loop Log to `i.2`.
  Direct C and LLVM fail closed with
  `direct MIR scalar CFG range iteration facts are invalid` and publish no
  artifact. This is observed evidence, not a completed fix.
- Forbidden fallback: name-only first/last matching, collapsing duplicate local
  rows, backend-side lexical scope recovery, source/`expr0` reconstruction,
  fixture dispatch, native semantic/AIR/libLLVM re-entry, planner retry, or
  raising a hard cap.
- No pressure probe, full CI, full bootstrap, current-source gen2==gen3, or
  Coq/Rocq suite ran. Memory remains final-integration-only: attention at
  2.4 GiB and hard stop at 3 GiB.

### Previous multi-backedge loop snapshot

- Executable checkpoint: `c27fa4e9` on `main`, one commit ahead of
  `origin/main` before this documentation update. `bin/pgy.exe` remains
  4,626,988 bytes, SHA-256
  `39798EA50105C9B48F26AE2FCABDB400B54AC153D15DC440B089C4E5E6402F9E`.
  The current Pergyra-built sibling is 4,282,817 bytes, SHA-256
  `91AC1AC29970D53DC7E48ACF77EEAD55A11065564D72FFE2EF20C11760DA4538`.
- Closed executable rung: `for_break_exit.pgy` produces one 7,229-byte MIR,
  SHA-256
  `222BBEF69BE73692E7F58A22695A910BB2A2DD793AF9A9239321B963B74048AE`.
  The producer emits header `total.3 = phi(total.1, total.5)`, exit
  `total.8 = phi(total.3, total.5)`, and binds the final Log to `total.8`.
  That same MIR produces a 580-byte C artifact
  (`DA274ADBCCA1873B30E8527960A233D9174520AE08742BD46E85EDEF74FEABA1`)
  and a 1,558-byte LLVM artifact
  (`1F9F69D4BC755CB4F46C8A7921D62DD003707CF25612F7E000DDA12F0196F87F`);
  both execute exact `3`.
- Producer ownership is no longer while-specific.
  `routine_loop_header_phi_owner.pgy` owns the admitted single-latch header
  version, and `routine_loop_exit_phi_owner.pgy` merges the feasible
  completion lane with every captured break snapshot for while and range
  lowering. No backend invents either phi.
- The target-neutral general scalar CFG plan now carries ValueId/LocalRef
  operands plus one sealed range iteration receipt. Fact, identity, readiness,
  route, operation assembly, operand spelling, range effect, and final C/LLVM
  emission have responsibility-named owners. Fixed caps remain 101/310,
  86/100, 225/240, 299/300, 397/450, 40/40, 269/280, 164/180, and 258/260;
  no cap was raised.
- The retired compiler range mini-path
  (`direct_mir_range_cfg_shape_owner.pgy`,
  `direct_mir_range_cfg_plan_fact_owner.pgy`, and
  `direct_mir_range_cfg_emission_owner.pgy`) is deleted. The remaining
  composite CFG plan is schema v9 and has no range plan/digest/emission arm.
  AIR may retain bounded range certificate evidence, but compiler consumers
  cannot retry it.
- Header and exit incoming-row permutations produce byte-identical artifacts.
  Stale duplicated header input, stale duplicated exit input, and a final Log
  that bypasses the exit phi all fail before publication for C and LLVM.
  Simple range bounds, changed bounds, zero-trip, summary/iteration/type/hoist/
  backedge/graph/binding mutations, while, break, and repeated-break regressions
  are green.
- Latest observed gates: final current-source driver graph reused its
  fingerprint in 7.1 seconds; focused for-break C/LLVM gate 6.6 seconds;
  existing break/repeated-break gate 12.3 seconds; cumulative CFG/range matrix
  149.3 seconds; public LLVM file/stdout 14.0 seconds; full structural
  component/removed-path ratchet 226.7 seconds. The manifests now own 284
  driver rows and 31 core MIR fixtures.
- Directly running the cumulative CFG gate without an explicit driver selected
  the stale `.tmp/self_hosted/driver/bootstrap/driver_seed.exe` and failed
  before the range rung with `unknown source MIR pressure token`. Current
  evidence explicitly selected `bin/pgy-self-driver.exe`; stale seed output is
  not current-source evidence.
- Classification: the bounded unique-`i`, outer-`total`, one-fallthrough-latch
  range/break slice is `SUBSTITUTING`. This is not general `for`, arbitrary
  `continue`, foreach, nested-loop, or whole-compiler replacement.
- Active objective card: close multiple actual backedge snapshots at the MIR
  producer. Priority is exact predecessor/version identity, one general loop
  header owner, general scalar CFG consumption, C/LLVM parity, then removal of
  the current single-latch admission bound. The fact owner is
  `routine_loop_header_phi_owner.pgy`; the last legitimate consumer is
  `DirectMirScalarCfgGraphPlan`.
- Next falsifier: add a range loop with both a reachable `continue` edge and a
  fallthrough latch while mutating an outer `Int`, then observe the producer
  header phi before changing it. The current range receipt deliberately rejects
  `backedge_blocks` cardinality other than one, because the producer currently
  snapshots only one aggregate backedge version.
- Forbidden fallback: consumer-side backedge value reconstruction, a
  continue-specific or block-count-specific compiler, duplicate incoming slots
  without predecessor snapshots, source/`expr0` recovery, planner retry,
  fixture dispatch, native semantic/AIR/libLLVM re-entry, or raising a hard cap
  to fit the implementation.
- No pressure probe, full CI, full bootstrap, current-source gen2==gen3, or
  Coq/Rocq suite ran. Memory remains final-integration-only: attention at
  2.4 GiB and hard stop at 3 GiB.

### Earlier checkpoint archive

### Previous for-loop break exit checkpoint

- Executable checkpoint: `6da669a4` on `main`. `bin/pgy.exe` remains
  4,626,988 bytes, SHA-256
  `39798EA50105C9B48F26AE2FCABDB400B54AC153D15DC440B089C4E5E6402F9E`.
  The rebuilt Pergyra sibling is 4,278,544 bytes, SHA-256
  `C274237F9D39B1123F8C6C8F3A75231F1A1008265176F4CD254E86A1C4DE2A44`.
- Closed executable rung: `multiple_break_exit.pgy` produces one 8,040-byte
  MIR with SHA-256
  `C2AF131C2AE49930FC9F9D1D6507DF3300A710771E8BF8BE39795CD40BBCA835`.
  Its exit phi has three predecessor slots `[i.2, i.4, i.4]`; C and LLVM both
  execute exact `2` from that same MIR.
- The pre-fix projector rejected that valid phi with
  `direct MIR scalar CFG predecessor/phi binding is invalid`. A first slot-only
  repair then exposed a false acceptance: forged stale incoming
  `[i.2, i.2]` could consume two distinct slots while omitting the latest
  definition. The landed owner therefore requires both per-predecessor slot
  consumption and the latest same-local definition dominating that edge.
- `routine_definition_dominance_fact_owner.pgy` now owns block dominance,
  strict definition ordering, and definition-to-use dominance. Phi binding
  scans all same-local routine definitions instead of treating the incoming
  list as its own proof. Caps remain 54/70 for dominance, 127/180 for binding,
  and 156/160 for the focused gate; no hard cap was raised.
- Permuting the incoming array to `[i.4, i.2, i.4]` produces byte-identical C
  and LLVM artifacts. The stale duplicate `[i.2, i.2]` and the four existing
  malformed predecessor/phi mutations still fail before publication.
- Green evidence: installed driver rebuild 130.8 seconds; focused repeated-slot
  C/LLVM gate 10.2 seconds; combined public multi-break plus nested scalar
  regression 16.8 seconds; full structural component/removed-path ratchet
  226.6 seconds. The manifest now owns 283 driver rows and 30 core MIR fixtures.
- Active objective card: make a range `for` with an outer mutable local and a
  feasible `break` carry its exit value in producer MIR. Priority is producer
  predecessor/version identity, one general loop-exit merge responsibility,
  C/LLVM parity, then negative rejection of missing or stale exit lanes. The
  current producer owner is `routine_for_owner.pgy`; the last legitimate
  consumer is the general scalar CFG plan through phi predecessor binding.
- Next falsifier: add `for_break_exit.pgy`, mutate an outer `Int` in `0..5`,
  break at a reachable iteration, and observe the installed producer and both
  projectors before changing ownership. `routine_for_owner.pgy` currently
  captures `SelfMirBreakExitFacts` but consumes only its block list when adding
  exit edges; it does not publish the captured local-version snapshots as an
  exit phi.
- Forbidden fallback: a second topology-specific for-break compiler, using the
  break block list without version snapshots, backend-created exit phi,
  fixture/block-count dispatch, source/`expr0` reconstruction, planner retry,
  or native semantic/AIR/libLLVM re-entry.
- No pressure probe, full CI, full bootstrap, current-source gen2==gen3, or
  Coq/Rocq suite ran. Memory remains final-integration-only: attention at
  2.4 GiB and hard stop at 3 GiB. The prover remains unavailable locally.

### Earlier checkpoint archive

### Previous first Array parameter checkpoint

- Executable checkpoint: `f8e91764` on `main`, one commit ahead of
  `origin/main` before this documentation update. Installed public C and LLVM
  artifact/compile/run remain target-specific `SUBSTITUTING` paths owned by the
  Pergyra-built sibling driver; this is not whole-compiler replacement.
- Closed multi-routine frontier:
  `src/self_hosted/codegen/fixture/array_return_literal.pgy` travels through
  source-to-MIR exactly once and the same 6,267-byte MIR feeds C and LLVM. A
  real producer fills caller-owned fixed storage, returns the admitted
  `Array<Int>` aggregate, and `Main` prints exactly `4\n3\n`. LLVM has zero
  `@pgy_` runtime references.
- `DirectMirArrayReturnProgramIdentity` owns exact-one `Main`, strict unique
  routine name/kind/syntax-id/return fields, zero-parameter signatures, typed
  direct-callee resolution, and semantic identity independent of routine row
  order. Row coordinates remain routing receipts, not identity.
- `DirectMirArrayReturnPlan` joins the producer literal, caller definition and
  two exact SSA uses, reachable terminal straight-line blocks, blank Log scalar
  fields, canonical ABI, target capability, and explicit
  `caller_owned_fixed_array` lifetime. C and LLVM emitters cannot reopen MIR.
- Local and returned arrays now consume one canonical captured `Array<Int>` ABI
  predicate, including every field offset, size, and alignment. A forged field
  shape with a correctly recomputed layout ID is rejected before artifact
  publication.
- Root dispatch reads routine cardinality before any row-zero shape. Once a
  program enters the multi-routine owner it cannot retry hello, scalar, local
  Array, Option, or CFG single-routine planners. Routine-order permutation is
  artifact-equal.
- The focused gate rejects thirteen independent mutations covering entrypoint,
  graph-valid unresolved callee, producer instruction/signature including
  missing and duplicate return fields, caller definition/use, ABI offset,
  repaired-ID field size/alignment drift, unreachable/nonterminal blocks, and
  forged Log result facts. It is wired into the LLVM-enabled self-host
  preparation parity aggregate, not left as a standalone smoke.
- Latest installed driver: 3,560,729 bytes, SHA-256
  `350A39D1DA6800657B24A5423B104057B4CFE33787AEDFE0F0442131ABC03EF3`.
  The final current-source DRV-2 rebuild completed in 93.9 seconds. It was not
  pressure-measured, so no memory peak is inferred from earlier builds.
- Latest green: Array-return focused C/LLVM parity and thirteen negatives;
  local Array regression; installed public LLVM compile/run; hard self-host
  contract; full component/removed-path ratchet; staged diff check. The full CI
  matrix, Coq adequacy suite, and current-source gen2==gen3 fixed point were not
  rerun.
- Active objective: compile and execute
  `src/self_hosted/mir_lower/fixture/array_literal_call_argument.pgy`. It has
  three routines (`Double`, `SumPair`, `Main`), passes a fixed `Array<Int>`
  literal into a typed parameter, nests a scalar call, and should print exactly
  `11`. This next fixture is selected but not yet admitted as completed
  evidence.
- Fact owner: strict routine identity/signature facts, typed direct-call targets,
  parameter carriage and Array ABI, caller/callee result-use identity, and the
  nested expression graph. Last legitimate consumer is one target-neutral
  multi-routine parameter plan feeding selected C or LLVM emission.
- Forbidden fallback: routine-name or row-order special cases, flattening the
  calls into constants, treating by-value Array carriage as an unowned raw
  pointer, C-only parameter reconstruction copied into LLVM, native
  semantic/AIR/libLLVM re-entry, or retrying the closed two-routine return plan
  after the three-routine graph is classified.
- Next falsifier: source-to-MIR once, the same MIR projected once per backend,
  exact `11`, routine permutation stability, and pre-artifact rejection of
  parameter type/carriage, call-target, argument-use, result-definition, and ABI
  mutations. Do not build a general query engine or jump to dynamic arrays.
- Memory policy remains one execution per changed semantic target followed by
  the final maximum only. Attention begins at 2.4 GiB and the hard stop is
  3 GiB; a below-threshold run does not redirect the active rung.

### Previous first multi-routine Array-return checkpoint

- Executable checkpoint: `76867abd` on `main`. Installed public C artifact,
  compile/link, and `--run` remain `SUBSTITUTING`. Plain public LLVM binary
  requests route only through the sibling Pergyra-built driver. The sealed
  runtime-free Option and local `Array<Int>` compile/run envelopes are now both
  executable `SUBSTITUTING` slices.
- Closed Array frontier:
  `src/self_hosted/mir_lower/fixture/array_literal_assignment.pgy` travels
  through source-to-MIR exactly once, one typed expression-graph owner, one
  target-neutral array plan, one selected ABI projection, and one C or LLVM
  emitter. Both executables print exactly `3\n10\n`; the LLVM artifact has zero
  `@pgy_` runtime references.
- `direct_mir_array_int_graph_fact_owner.pgy` owns the literal spine,
  assignment target, `ArrayLength`, indexing, and addition graphs.
  `direct_mir_array_int_plan_owner.pgy` owns local/result identities, element
  vectors, latest SSA uses, canonical layout facts, target capability, and the
  plan digest. The plan consumes instruction kind/source type from
  `MirProgramRoutineIndex`; blank scalar-capture display fields are not semantic
  authority.
- One plan drives both backends. The runtime-free representation is one
  stack-backed fixed aggregate with pointer, length, capacity, and owner fields;
  the selected ABI projection alone maps it to C or LLVM. Dispatch classifies
  the Array slice before the scalar slice and cannot retry scalar/hello after an
  Array rejection.
- The focused gate rejects seven pre-artifact mutations: element kind, index
  kind, length target, stale SSA use, ABI offset, source type, and unsupported
  static index. Source-to-MIR executes once, the same MIR is projected once per
  backend, output is exact, and the admitted MIR hash remains
  `9D056A3A9D9063207B9CD3A871E81E60684C0637A3CC4AA870E06952499C618F`.
- The installed public LLVM gate now uses the Array program, observes exact
  `3\n10\n`, and keeps exactly-once, stale-output, missing-driver, malformed-
  artifact, and no-native-fallback negatives. `clang -x ir` remains only the
  host compile/link boundary.
- Bootstrap source inputs are repository-relative. MSYS absolute spellings had
  caused the native compiler's absolute-source authority check to reject the
  import-composed driver before code generation. Output paths and cache
  identities remain explicit; component ratchets reject restoration of the old
  absolute source invocation.
- The refreshed Pergyra-built codegen seed completed in 410.451 seconds at
  2.705 GiB peak working set and 2.841 GiB peak private. This is above the
  2.4 GiB attention threshold but below the 3 GiB hard stop. An intermediate
  current-rung driver build completed in 98.359 seconds at 1.579/1.684 GiB
  working/private; later small rebuilds were not pressure-measured and must not
  inherit that number.
- Installed `bin/pgy-self-driver.exe` is 3,528,807 bytes with SHA-256
  `D3CDA2D90E2018F453DCA8ACE7B374F21E5B62EF7F4DFCB281282D1F86D2BE52`.
  Refreshed `.tmp/self_hosted/codegen/bootstrap/gen2.exe` is 2,257,728 bytes
  with SHA-256
  `BD6D3E074885CCA4C8308F873A212A04DDF4DD22E1C7244E22963B041ADCF28D`.
  The refreshed seed is current-source capable, but gen2==gen3 was not rerun at
  this checkpoint; retain the previous fixed-point result only as historical
  evidence.
- Evidence remains target-specific. General arrays, heap/runtime-bearing LLVM,
  package/dump/check/repl, arbitrary multi-routine calls, and a canonical
  compiler-purpose intent remain open. This is not whole-compiler self-host
  completion.
- Active objective: compile and execute
  `src/self_hosted/codegen/fixture/array_return_literal.pgy`, whose `Build`
  routine returns `Array<Int>` and whose `Main` routine prints `4\n3\n`.
  Installed source-to-MIR succeeds exactly once, producing a 6,267-byte,
  two-routine MIR with SHA-256
  `8AFFE11FE23F78554980FCCAA62E1DE8F024F679EC496702736FC0C47669D6DD`;
  direct LLVM currently fails closed before artifact publication.
- Fact owner: `MirProgramRoutineIndex` routine names, identities, declarations,
  and typed call/return facts. Last legitimate consumer is one target-neutral
  multi-routine plan/emitter; `clang -x ir` remains only the host boundary.
  The direct bypass to delete is hard-coded `admitted.routines[0]` and first-
  routine block/instruction shape dispatch, which currently misclassifies the
  single-instruction `Build` return as the hello slice.
- Forbidden fallback: source-name guessing, a first-routine default, copying a
  C-only call/return reconstruction into LLVM, native semantic/AIR/libLLVM
  re-entry, runtime inference from LLVM text, or scalar/hello retry after the
  multi-routine graph is classified.
- Next falsifier: the same produced MIR must feed C and LLVM once, select `Main`
  by the owned entrypoint/routine identity, carry the `Build` return into the
  caller without row-order reconstruction, execute exact `4\n3\n`, and reject a
  mutated entrypoint, call target, or return use before artifact publication.
  Do not build a general query engine.
- Latest green: Array C/LLVM focused parity plus seven mutations; installed
  public Array LLVM compile/run and exactly-once/stale/failure negatives; hard
  self-host contract; full component and removed-path ratchet; diff check. The
  full CI matrix, Coq adequacy suite, and refreshed codegen gen2==gen3 fixed
  point were not run.
- Memory policy remains one execution per changed semantic target, followed by
  the final summary only. Hard stop is 3 GiB and attention begins at 2.4 GiB;
  attention is recorded but does not redirect the active rung.

The former source-to-MIR timeout card begins below. It was correct for its
checkpoint but is superseded by `76867abd`; it must not be resumed as the active
P0. External reviews that observed `614cb5d5` likewise describe historical
evidence, not the current compiler state.

### Previous source-to-MIR full-bootstrap checkpoint

- Resume scope: read this card, verify the named owner and gate, then continue
  this one executable rung. Sections below `Historical checkpoint archive` are
  lookup evidence only and must not be treated as parallel work queues.
- Verified checkpoint: 8819acae on main, equal to origin/main before this dirty
  vertical slice. Verify exact HEAD and dirty state with Git before resuming.
- Active production entrypoint: driver_bootstrap_main.pgy,
  PgyCompilerWorld.source_mir, DriverSourceMirExecution,
  DriverRung2MirProjectionFromAdmittedAnalysisObserved, then
  SelfMirProgramFactsFromReadyArtifactObserved.
- Closed fact seams: SelfMirProgramFacts owns one immutable semantic expression
  graph. Instruction rows carry root/bounded range handles, and the program
  instruction index owns borrowed routing/text/graph bounds. Per-instruction
  whole-graph storage, graph text reconstruction, and
  SemanticExpressionGraphFactsEqual are forbidden.
- Closed cumulative-graph seam: sequence append and parser bridge carry the
  prior call-return vector and append only the new node fact. Target projection
  does not re-run whole-arena Ready. The final expression-graph fact owner
  validates the cumulative arena exactly once.
- Closed publication seam: stdout mode may materialize one MIR JSON payload,
  but artifact mode consumes verified SelfMirProgramFacts through
  SelfMirProgramJsonWriteArtifactVerified. SelfMirArtifactCommitPayload is
  forbidden in the source-MIR artifact action.
- Fixed-input release evidence: the prior artifact path completed semantic MIR
  work but crossed the 3 GiB stop at 3.098 GiB private after materializing an
  86 MB payload. The streaming path completed in 83.364 seconds at 1.525 GiB
  peak private and 1.404 GiB working set with attention_required=false.
- Current 90,304,012-byte MIR consumer evidence: r54 reached graph row 12,288
  and the 3.009 GiB hard stop at 311.431 seconds. After eliminating cumulative
  graph reconstruction, r55 reached row 28,672 during a 900-second timeout at
  only 0.965 GiB peak private and 0.904 GiB working set. This closes the memory
  defect, not the completion/throughput defect.
- r56 reached row 40,960 and was intentionally stopped after about 1,131
  seconds because another longer wait would not add implementation progress.
  It is incomplete evidence and must not be reported as green.
- The current SubstringWithLen-aware codegen seed-only build exited 0 in 400.6
  seconds. It produced a runnable Pergyra-built gen2 codegen and self parser in
  `.tmp/self_hosted/codegen/bootstrap_8819acae_r2`. This replaces the older
  pre-SubstringWithLen seed evidence. Full current-source gen2==gen3 remains
  open.
- SubstringWithLen now carries an existing length fact through runtime, native
  type/C/LLVM lowering, and self-host builtin signature. Unescaped bounded JSON
  strings and number tokens use one bounded copy instead of one allocation per
  character.
- Runtime-call ABI row 245 records the self-host helper and C/LLVM manifest
  parity is artifact-equal. The fresh gen2 seed and bounded production driver
  now execute that mapping; the earlier filtered str_builtins run still is not
  a PASS because its tool build did not finish inside 300 seconds.
- The first fresh bounded driver run reached the real readiness boundary and
  failed closed with `builtin_signature`. `SemanticBuiltinSignatureRows` had
  gained `SubstringWithLen`, while its readiness function separately mirrored
  the base row count as numeric literal 124. The numeric mirror was removed;
  seed/projection parity remains the owner-derived exact check. The new
  `builtin_signature_registry_owner_parity.sh` rejects numeric count mirrors,
  requires exactly one SubstringWithLen row, and executes the readiness probe
  under C/LLVM artifact parity.
- The repaired Pergyra-built bounded production driver exited 0 in 534.4
  seconds. Self/oracle sample C is byte-identical at SHA-256
  `0E32EC703F3B1237FC8C147BD8F395D89A53106D649F3E8F1AB4C608FC0FF25B`;
  bounded MIR JSON is byte-identical at
  `0C5E32D7E035F96C4F3EFCEFD569DA60EA8BEF98FFA3A11355DD3573C6F56739`;
  the MIR consumer emitted the same C artifact. The long section was the native
  oracle build, observed at about 0.967 GiB RSS, not repeated self-host graph
  validation.
- Emitted-C profile owner: emitted_c_runtime_header_owner.sh. The default
  self-host profile is release with -O3 -fwrapv -fno-strict-aliasing.
  PGY_SELFHOST_CC_PROFILE=test explicitly selects -O0 with the same semantic
  flags for debugging; O0 is not the normal-build benchmark.
- Open test-profile defect: the O0 generated driver reaches routine 397 and
  overflows the Windows stack in nested ApplyPostfixFact lowering because
  generated lowering frames are tens of KiB. Release mode completes that
  computation; do not hide the O0 defect by increasing the process stack.
- Evidence grade remains REACHABLE, not SUBSTITUTING. The bounded codegen
  fixed point exists, but the released default compiler still has no whole-root
  Pergyra replacement. Source files, owners, and green structural gates alone
  do not change that percentage.
- Latest focused green: fresh SubstringWithLen-aware Pergyra gen2/parser seed,
  repaired bounded production-driver sample/MIR producer/MIR consumer parity,
  builtin-signature readiness C/LLVM parity, native pgy incremental build,
  SubstringWithLen C/LLVM
  parity, bounded JSON exact-bound C/LLVM parity, expression-graph projection
  and persisted-read owner gates, MIR routine-index fixture, self-parser owner
  acceptance, source-MIR action negative gate, and the structural component
  contract through the graph/JSON slice. After the final self-host ABI addition,
  runtime ABI parity, shell syntax, line caps, and owner acceptance are green;
  the full component contract was not rerun. No full matrix is implied.
- Memory policy: execute one semantic target once, then read only the final
  peak_private_gib and attention_required summary. The hard stop remains 3 GiB
  and attention starts at 2.4 GiB. Do not poll live samples or optimize memory
  below that threshold without another reached owner.
- Forbidden fallback: graph copies, whole-graph equality, artifact payload
  materialization, default O0 self-host builds, a higher memory cap, repeated
  graph validation, per-character bounded-token strings, a general cache/query
  engine, timeout-only reruns, or unrelated library work.
- Next falsifier: execute the same full source-to-MIR target exactly once under
  the pressure owner using the current green seed and require native-oracle
  byte parity. Only a completed run may advance to current-source gen2==gen3
  evidence. Do not regenerate the seed through the Make dependency before this
  run; reuse the named current artifacts unless their imported source identity
  changes.
- Do not infer the full matrix or fixed point from focused results. If the one
  scheduled full consumer fails, its exact compile/parity diagnostic is the
  blocker. If it stays below memory attention but times out, profile the reached
  JSON/graph owner; do not merely extend the timeout or reopen already closed
  graph/streaming seams.

### Earlier historical checkpoints — inactive unless explicitly referenced

The remaining sections preserve exact revisions and prior falsifiers. They are
lookup evidence, not an ordered TODO list. Do not resume Insere/Zeno, an older
zone/ABI seam, or an architecture proposal unless the active card or the user
explicitly names it.

## Historical checkpoint - exhaustive self-host CI and executable-rung closure

- This continuation started at
  `ef1522821f9de89783f23ebdbcacbc34bec05705`, equal to `origin/main` before
  the current dirty change set. The containing repair commit and push must be
  verified with Git; this note cannot name its own containing revision.
- GitHub Actions run `30535237959` kept the backend comparison shards, formal
  proofs, sanitizers and self-host codegen bootstrap green, while exposing five
  independent contract failures: imported enum variants were absent from the
  lightweight semantic callable table; several standalone sources relied on
  transitive imports; language-word implementation inventory was stale;
  memory-concurrency and production-header gates still named the pre-zone-sync
  owners; and full bootstrap rejected compiler-stage nested intent calls at
  `mir-facts:start` because DIR admitted only direct subject actions.
- The semantic SoT repair projects declared enum variants into the canonical
  callable table with enum return and payload signature. The positive fixture
  covers qualified and bare zero/payload variants. The negative fixture locks
  `ImportedDecision.Missing -> undefined_symbol`; rewriting the driver or
  treating a missing qualified variant as an arbitrary member read is a
  forbidden fallback.
- The exhaustive checker also falsified three older scanner/import assumptions.
  The delimiter owner now distinguishes a spaced comparison `<` from a
  type-argument opener, nominal constructor scanning consumes `let mut`, and
  direct consumers own their imports. The intentional
  `expr_type_owner/result_call_type_owner` recursion remains one declared
  checker cluster rather than being turned into a circular source import.
- Last observed broad semantic evidence for this dirty set is the current
  TestHarness manifest plus C semantic checker accepting all 684 real
  self-host sources. This is source semantic acceptance, not full-bootstrap
  execution. The native
  production-header census passes at 717 headers; the self-host C/LLVM header
  checker is artifact-equal for clean and over-cap cases. The memory-concurrency
  model passes its C-focused path while following
  `pgy_runtime_zone_sync_abi.h` as the lock-diagnostic owner.
- Objective card for the now-reached nested-intent seam:
  - objective: admit the documented `FrontendPipeline -> IntakeSource ->
    SourceUnit.Read` composition without flattening or disguising an intent as
    an action;
  - priority: exact intent identity, action/intent discrimination, return/arity
    carriage, fail-closed negative cases, then bootstrap completion;
  - fact owner: `SemanticAstIntentSignatureFacts` plus the parser-owned
    expression graph; `SemanticAstIntentCallFromGraph` carries the exact chosen
    target and DIR/MIR consume that identity;
  - last consumer: self-host MIR intent routine construction during full
    bootstrap;
  - forbidden fallback: rewrite `stage_intents.pgy`, insert intents into the
    action/function table, accept name-only/ambiguous calls, or silently skip
    the compiler intent cluster;
  - observed falsifier: a fresh C-emitted self driver passed documented nested
    intent, single-step and two-step graph identity parity plus missing,
    ambiguous and wrong-arity target negatives. Classification is `REACHABLE`,
    not `SUBSTITUTING`.
- The adjacent authority seam is also reached: semantic facts now carry exact
  authority, zone, subject-slot and required-ability identities into DIR, and
  `zone_authority_fact_owner.sh` proves the old AST rescan is absent plus
  mutation negatives. It remains `BRIDGE` until the production MIR authority
  transition and runtime plan consume the same owner.
- Parser intent parameters are finalized after declaration/import composition,
  not in source order. Native C tracks the header-binding prefix and rebuilds
  the involved/value views once. The self parser emits neutral
  `IntentBinding` rows and resolves exact final subject/zone identities; its
  matcher is indentation-anchored so embedded contract strings are preserved.
  Native cross-module positive/unresolved-negative fixtures pass, and
  `bin/pgy --ast src/self_hosted/compiler/driver_bootstrap_main.pgy` resolves
  `SourceIntakeZone -> IntentInvolves` and `StagePathManifest -> IntentValue`.
  The focused self resolver reproduction passes; the whole-driver self-parser
  run was policy-stopped after 1,532.042 seconds with zero output. Its last
  observed private memory was 717,144,064 bytes, not an exact peak. Therefore
  full self-parser integration remains incomplete for performance, not a
  semantic PASS.
- Focused green evidence: component contract, zone-authority carriage, fresh
  nested-intent self-driver reachability, language-keyword registry, LSP
  latest-publication C/LLVM parity, changed-owner semantic checks, 684/684 C
  real-source semantic acceptance, and SoT live owner/consumer negative gates.
  Coq/Rocq is absent on this runner, so the formal model was explicitly skipped
  with `PGY_ALLOW_MISSING_COQ=1`; it was not claimed as checked.
- The attached architecture review's query-engine and opaque-admitted-artifact
  proposals remain valuable follow-up candidates, not concurrent owners in this
  repair. Its memory improvement still holds, but the CPU warning remains
  current: the latest full integration reached `mir-facts:start` and was
  stopped after 2,534,272 ms at 2,284.8 MB peak private. The next executable
  falsifier is a profile of that exact owner path with stable revision/query
  keys, followed by a full bootstrap under the unchanged limit. Do not raise
  the timeout or call this incomplete run green.

## Historical checkpoint - exact bootstrap pressure and zone runtime closure

- This material continuation started at
  `3f1416bd1f09864bb45dcea982af611e67fffb5b`, equal to `origin/main` before
  the current dirty change set. The final commit/push must be verified with Git;
  this note cannot name its own containing revision.
- Active executable rung: close self-host zone storage and synchronization from
  the existing declaration, DIR topology and
  `semantic.domain_runtime_assignment` owners. The last consumer is the
  self-host C runtime emitter. Empty sync bodies, name-only reconstruction,
  fabricated MIR/domain-graph identity and a copied native body detached from
  admitted facts are forbidden fallbacks.
- Exact pressure is no longer the blocker. The fixed 5,106,665-byte AST
  (`97EEFA34159BE8AFEA8D15F44BF5F74FB57D5DD1D8C03ABF565AF4A14B8D5190`)
  completed C emission in 158.020 seconds at 1,659.1MB peak private. A fresh
  parser artifact of 5,326,689 bytes
  (`49BFB21900867135FBAF6F51F23364BB108B88A65C62328541D6089DBD64844B`)
  completed in 164.252 seconds at 1,742.1MB peak private. After exact zone
  authority/where/slot carriage, r10d completed the same artifact in 145.719
  seconds at 1,759.6MB peak private and 1,666.9MB working set; its C output is
  byte-identical to r9. All used the unchanged 3,072MiB / 2,400-second pressure
  policy. The earlier 20GB symptom is not reproduced.
- A fully current r11 rerun removes the remaining stale-artifact ambiguity. Its
  fresh AST is 5,324,488 bytes. The zone-sync 9c codegen build completed in
  71.756 seconds at 911.2MB peak private and 865.2MB working set. Exact C
  emission completed in 123.632 seconds at 1,838.6MB peak private and 1,733.1MB
  working set, below the unchanged 3,072MiB limit. The output is 5,368,419
  bytes. The AST contains 20 zone identities and the emitted C contains exactly
  the same 20 `static void *Zone_sync` definitions: 18 compiler-world zones and
  two support zones. Host GCC compilation now succeeds and the executable
  reaches the expected driver argument boundary. The former 15 missing-symbol
  host-compile failure is closed. After the final owner/policy consolidation, a
  fresh current-source codegen repeated the exact emission in 136.249 seconds,
  produced 5,368,053 raw UTF-8 C bytes, preserved the 20/20 bijection and again
  host-compiled successfully. This last repeat was not pressure-sampled; the
  9c peak figures above remain the measured memory evidence.
- The measured closures are admitted constructor proof reuse, binary node-ID
  reads, carried call-return/place facts and one same-epoch global type-row
  index. Dynamic local rows retain their prepend/first-row rule. A general
  cross-revision query engine remains deferred until a stable key/revision owner
  and measured invalidation consumer exist.
- Parser intent parameters now keep one source-order stream. The interleaved
  zone/value/subject/value fixture passes through both native and freshly built
  self-host parsers. Intent codegen derives omitted `who` through the semantic
  actor owner, keeps actor distinct from authority, accepts by-value and inout
  `using`, requires explicit `where` to match that zone, and accepts authority
  only through an exact declared zone subject slot. Exact aliases win; a
  type-only slot match must be unique.
- Evidence grade for the new exact compiler-world zone result is `REACHABLE`,
  not hard `SUBSTITUTING`. It removes the self-host emitter's empty runtime
  fallback and produces executable C, but no released C-owned production route
  has yet been replaced by a freshly installed self driver. The already
  substituting admitted MIR domain-runtime slice remains intact: nonzero typed
  topology continues through its admitted plan, while the semantic-artifact
  fast path accepts only proven zero topology and fails closed before partial C
  on nonzero topology. The currently installed `bin/pgy-self-driver.exe` remains
  stale until the next installed-driver replacement rung.
- Last observed green focused evidence: language keyword registry
  (146 rows, 70 reserved, 76 contextual selectors, 9 fixtures), include-size
  gate, semantic environment lifetime/admission ratchet, source-MIR execution
  action gate, intent-step binding execution contract, MIR machine layer,
  domain-runtime assignment, zero-topology zone-sync execution and full
  self-host component contract. The zone gate proves a declaration/definition
  identity bijection, two generation increments with unchanged object/tobject
  projection tuples, default atomic execution, thread-safe execution under an
  explicit harness-owned init/destroy lifecycle and fail-closed
  semantic-artifact nonzero topology. Language-generated zone constructor/
  destructor lifecycle ownership is still open. The standalone leaf-place
  contract checker ran for more than ten minutes without output and was stopped;
  it is **not a PASS**. The broad C/LLVM real-source semantic selfcheck was also
  sampled only through the first 13 of 673 C targets and stopped because its
  projected duration exceeds the repository's focused integration budget; it
  is a scheduled/CI matrix and is **not recorded as a full PASS** here. The SoT
  adequacy mutation checks pass with `PGY_ALLOW_MISSING_COQ=1`, but the Coq model
  is an explicit skip because this runner has neither `rocq` nor `coqc`; the
  unmodified fail-closed invocation exits nonzero and is not recorded as green.
- The previous GitHub Actions selfcheck timeout on
  `compiler_world_direct_mir_owner.pgy` was reproduced locally beyond 86.383
  seconds. Sealed-length scanner calls and one-pass import-bundle assembly now
  produce the 760,066-byte bundle in 2.978 seconds and return semantic
  `Status: ok` in 2.681 seconds. The faster checker also exposed and closed the
  missing `zone`/`world` nominal-constructor and slot-row semantics; the
  60-second CI budget remains unchanged.
- The focused semantic parity runner completed all 113 C verdict fixtures.
  Its subsequent LLVM leg was stopped after the combined runner exceeded the
  five-minute focused budget; LLVM semantic parity is therefore not recorded as
  a PASS in this checkpoint and remains for CI/scheduled execution.
- GitHub CI run `30524796373` proved that the former 60-second
  `compiler_world_direct_mir_owner.pgy` timeout is closed: exhaustive C
  selfcheck passed that root and advanced to 155/677. It then exposed a
  separate standalone import defect in `direct_mir_llvm_text_format_owner.pgy`
  (`undefined_function: Die`). The owner now imports the existing codegen text
  boundary directly, and the component gate rejects removal of that edge. A
  replacement CI run is required before this fix is called green remotely.
- The attached 2026-07-30 architecture review's 40-minute/1.55GB timeout was
  valid for its older checkpoint, but it is superseded by the measured 9c exact
  result above. A general query/dependency engine is therefore an evidence-led
  future architecture target, not permission to start a parallel cache owner in
  the active rung.
- Next falsifying sequence:
  1. build and install a fresh self driver from the host-compilable exact C;
  2. run installed launcher parity plus live typed-intent execution and reverse
     compensation gates;
  3. prove the installed production entrypoint reaches the world/action owner
     and delete the replaced C-owned route without a dual read;
  4. only then classify that deleted-path replacement as `SUBSTITUTING`;
  5. keep composite-intent full DIR admission and thread-safe lock lifecycle as
     separately named open seams rather than weakening the zero-topology gate.

## Historical checkpoint - body admission and latest-only publication

- Material checkpoint: `835348ac318506031a375d8fc168a55e9ca94eb3`
  (`feat: seal self-host body and publication facts`), based on
  `6f83a7cd40ba6ff06ab1bb429fe5e877d41b1752`. At handoff-writing time `main`
  is one commit ahead of `origin/main` and the only intended dirty file is this
  handoff refresh. Verify the final handoff commit and remote equality with Git;
  this paragraph is not repository-state authority.
- Active self-host seam: producer-time `SemanticAstArtifactAnalysis` now carries
  function scopes, and `AstBodyAnalysisAdmission` performs one identity,
  parallel-row and reconstruction-free structural-shape admission before the
  body stages. Initializer, iteration, call-target, refinement, place,
  assignment, statement and generic owners consume the admitted analysis and
  its existing enum/scope/table facts instead of reopening the whole artifact.
- Fact owners are
  `ast_body_analysis_admission_owner.pgy` and
  `ast_body_analysis_shape_owner.pgy`; the fail-closed witnesses live in
  `ast_body_analysis_admission_contract_owner.pgy`. The last orchestration
  consumers are the body bundle, driver rung 2 and admitted codegen pipeline.
  Forbidden fallbacks are per-stage artifact reconstruction, repeated graph
  validation, and trusting a caller-supplied mutable analysis solely because
  its row counts still match.
- The driver boundary is intentionally split. Fresh analyses use the admitted
  entrypoints. Externally supplied raw analyses retain one deep
  `SemanticAstArtifactAnalysisMatches` proof. The mutable-analysis fixture
  changes a local name without changing shape and is rejected before
  `body-types:start`.
- Canonical MIR identity/epoch projection now obtains nested domain-runtime
  assignment facts through the typed owner accessors while preserving the
  whole borrow. The parity fixture remaps declaration, topology, directive,
  slot, field, path and runtime-assignment epochs atomically. The stale
  compiler-world smoke assertion was updated to require the admitted
  `GenerateCUnitFromReadySemanticFacts` path rather than a deleted fallback.
- Insere adoption is no longer documentation-only. The production self-host LSP
  `Main --document-store-probe` consumes one `LspDocumentRevision` fact owning
  URI, numeric version, exact text and `HostTaskSlot` ticket. Lower versions,
  same-version/different-text changes and stale diagnostics publication are
  rejected without partially mutating the document store. This is bounded
  `REACHABLE`; it does not yet replace the released C LSP loop.
- Zeno adoption remains the existing `SnapshotTicket` plus
  `BinaryProjectionPreflight` slice. It binds slot generation, the existing
  `MirAbiLayoutRowCapture` identity and explicit endian, then runs through C and
  LLVM. It is `REACHABLE` tooling/library evidence, not a second Layout IR and
  not `SUBSTITUTING` compiler progress.
- Last observed focused evidence for this material checkpoint:
  - body analysis admission owner and shape owner self-host semantic checks:
    PASS;
  - standalone admission-contract semantic check exceeded the focused CPU
    budget and was stopped after about 16 minutes: **not a PASS**. The same
    contract is reached by the driver readiness path exercised by the passing
    component gate;
  - raw-analysis mutation admission gate: PASS, fixture build `0 errors, 0
    warnings`;
  - semantic environment lifetime/admission ratchet: PASS;
  - self-host component contract: PASS;
  - compiler-world topology/source-shape contract: PASS;
  - canonical identity/epoch C execution and stale/wrong-kind negatives: PASS;
  - Insere-derived LSP latest-publication C/LLVM parity plus the existing
    document-store/session-state parity: PASS;
  - Zeno-derived binary-projection preflight C/LLVM executable parity: PASS;
  - build-source inventory, documentation quality, shell syntax and
    `git diff --check`: PASS.
- No new 2.9MB or 5,106,665-byte pressure run was performed for this checkpoint.
  The previous exact 5.1MB run stayed under 3GiB but timed out, so it remains a
  performance falsifier rather than an end-to-end PASS. A sampled final focused
  build showed the Pergyra process and `cc1` each near 0.9GiB, but that sample is
  not a formal peak measurement.
- Evidence grade remains `REACHABLE`, not `SUBSTITUTING`: this checkpoint closes
  repeated semantic admission and mutable-boundary defects and adds a real
  self-host LSP consumer, but does not yet delete another C-owned production
  compiler/LSP entrypoint.
- Next falsifiers, in order:
  1. rerun the exact 5,106,665-byte normalized parity fixture under the
     3GiB/2,400-second cap; only if body admission remains below the dominant
     cost, close the measured emission-side linear node-ID lookup seam;
  2. carry the Insere-derived revision ticket through the live read-exact
     diagnostics completion and delete released-C direct document mutation;
  3. derive Zeno-style ABI inspect/diff only from
     `MirAbiLayoutRowCapture`, rejecting offset/size/alignment/endian drift and
     identity collisions without introducing another layout owner.

## Historical checkpoint - admitted semantic artifact emission

- Exact remote base for this continuation was
  `18c105a75894d1b09c66da2cad5b1b380e3c7a73` on `main`, equal to
  `origin/main`. It contains `28e371df` (Insere/Zeno audits), `2c2b3028`
  (semantic admission), `3bc5e724` (cross-platform self-host CI contract
  closure) and `18c105a7` (handoff refresh).
- Latest local checkpoints are `22054b4e` (`fix: import option call target
  owner`) and `9d51ff5d` (`test: refresh source scan owner evidence`), after
  `0984ba77` closed the dynamic ability selfcheck target. At handoff-writing
  time the worktree is dirty only for this handoff refresh and `origin/main`
  remains at `15485120`; push is authorized and pending. Verify final remote
  equality after the handoff commit rather than treating this paragraph as Git
  authority.
- Active executable seam: a Pergyra-built codegen created one
  `SemanticAstArtifactAnalysis`, then C emission called
  `SemanticAstArtifactAnalysisMatches` and reconstructed signatures,
  constructors, locals, assignments, statements, enums, roles, expression
  surfaces/graph, type surfaces and kind surfaces from the whole AST again.
  On the 5.1MB bootstrap AST this was the last observed 3,072MB scaling RED.
- `AstTreeArtifact` payload schema v4 now carries one producer-time
  `identity_digest`. It binds tree text, node count and parser-owned expression
  graph roots/arena. Parser graph injection recomputes it once; deterministic
  owner-kind and destructure arena projections preserve that epoch. Semantic
  verdicts seal the same identity. Emission never recalculates the digest.
- `SemanticAstArtifactAdmissionReady` is the fixed-size comparison boundary:
  verdict status, node count, artifact identity and entrypoint policy only. It
  cannot call AST/graph readiness, hashing, `*FactsMatchArtifact`,
  `*FactsFromArtifact` or `*RowsFromArtifact`.
- The digest is an epoch witness, not an untrusted-input security seal. Fast
  callers are exact-allowlisted and receive owner-produced fact arrays that are
  immutable after admission. Arbitrary or mutable artifact/analysis pairs must
  use the public deep-checked compatibility entry.
- Production entrypoints now use one admitted/Ready graph:

  ```text
  codegen seed
    -> GenerateCUnitFromAstArtifact
    -> SemanticAstArtifactAnalyzeCompactBridge             # exactly one
    -> GenerateCUnitFromAdmittedSemanticArtifact
    -> GenerateCUnitFromReadySemanticFacts                 # reconstruction zero

  source-to-C / admitted MIR-to-C
    -> one semantic analysis + body-type admission
    -> GenerateCUnitFromReadySemanticFacts
  ```

  The unused `GenerateCUnitFromSemanticFacts` checked fallback was deleted.
  The externally callable raw semantic-artifact entrypoint retains exactly one
  deep match before entering the admitted path.
- Fail-closed evidence now includes same-node-count/different-text and
  same-tree/different-expression-graph artifacts. Both receive different
  producer identities and are rejected by admission. The lifetime gate fixes
  direct codegen, source-to-C and integrated driver call edges and forbids
  unbounded proof work inside the Ready core.
- Observed focused evidence for the material checkpoint:
  - `driver_rung2_main.pgy --emit-c`: `0 error(s), 0 warning(s)`;
  - executable `CompilerDriverPipelineReady` probe:
    `semantic-admission-contract:ok`, exit 0;
  - self-host component contract: PASS;
  - semantic environment/admitted emission ratchet: PASS after correcting its
    stale call-target assertion to follow the existing body-environment owner;
  - shell syntax and `git diff --check`: PASS;
  - fresh native-seed codegen build: exit 0, 47,749ms, peak
    1,138.2MB working / 1,190.8MB private;
  - 2,864,634-byte AST emission: exit 0, 1,098,757ms, peak 890.5MB working /
    968.4MB private, 2,785,703-byte C output;
  - 5,106,665-byte AST emission: timeout 124 at 2,400,686ms, peak 1,436.1MB
    working / 1,551.4MB private, no 3,072MB limit breach. This closes the
    observed memory RED but is not an end-to-end emission PASS;
  - compiling the successful 2.9MB C output into gen2: exit 0, 4,710ms,
    peak 244.5MB working / 229.7MB private;
  - gen2 consuming the same 2.9MB AST: exit 0, 1,059,367ms, peak 1,177.7MB
    working / 1,357.3MB private. Raw output differed only by one trailing blank
    line; the repository emitted-C normalization/comparator passed.
- Evidence grade remains `REACHABLE`, not `SUBSTITUTING`. This removes a real
  Pergyra-built codegen hot-path reconstruction and fixes its provenance
  boundary, but does not yet prove a fresh installed self driver or replace a
  new native C semantic owner.
- The 5.1MB CPU audit isolated two remaining costs. Before emission,
  `SemanticAstBodyTypeBundleFromAnalysis` repeatedly deep-matches or rebuilds
  signature/local/iteration/assignment/statement facts. During emission,
  `EmitStmtList` performs nested linear node-id searches for local, assignment,
  statement-kind and expression rows. The fixed fixture has 110,971 nonempty
  AST rows, 4,094 callables, 12,224 locals, 6,958 assignments and at least
  27,675 tracked statements. Local/assignment misses alone imply a
  530,861,850-comparison lower bound; statement-kind lookup is approximately
  3.50 billion comparisons from the current loop order and average row
  position. These are source/census calculations, not runtime counters.
- Next falsifier: add coarse, non-row-level stage markers, close body semantic
  proof admission without reopening whole-artifact reconstruction, then rerun
  the exact 5,106,665-byte fixture under the 3GiB/2,400-second cap with
  normalized-C parity. Only after that seam closes should a separate emission
  rung replace owner-local linear node-id scans with ordered lookup. The timed
  out 5.1MB run remains non-green.
- The body-admission implementation card is now concrete:
  - owner: the existing `SemanticAstArtifactVerdict` identity and analysis
    produced by `SemanticAstArtifactAnalyzeFromExpressionSurfaces`; do not add
    a parallel body authority;
  - last legitimate proof consumer: a new
    `SemanticAstBodyTypeBundleFromAdmittedAnalysisObserved` entry that checks
    artifact identity plus fixed parallel-row shapes before body work;
  - production callers: only `program_admitted_semantic_owner.pgy`,
    `driver_pipeline_owner.pgy` and `driver_rung2_owner.pgy`; probes and
    arbitrary/mutable-pair contracts retain the checked entry;
  - forbidden admitted closure: `SemanticAstArtifactAnalysisMatches`, every
    whole-program `*FactsMatchArtifact` and already-carried plural
    `*FactsFromArtifact`, repeated expression-surface/graph full readiness and
    a `fast ? checked : admitted` dual path;
  - the admitted family cores must receive analysis-owned enums, roles, intent
    signatures/transitions and function tables. The existing checked APIs
    perform their deep proof once and then descend into those cores;
  - hidden cost: `SemanticAstExpressionSeedVisibleMatchBindingsFromReadyArtifact`
    currently rebuilds enum and function-scope facts at match-visible use
    sites from initializer, assignment, statement, call-target, place and
    generic passes. Add an admitted-facts seeder consuming the existing scope
    local view; a single case pattern projection may remain, but plural
    whole-program reconstruction may not;
  - negative gate: stale identity or one malformed parallel row fails before
    `base-initializer:start`; admitted transitive forbidden-call count is zero;
    exact caller allowlist holds. Run component/lifetime and body parity first,
    then 2.9MB normalized parity, then the exact 5.1MB capped falsifier.
  This is still `REACHABLE` performance closure, not a new native-C owner
  substitution and therefore not `SUBSTITUTING` evidence.

## Historical checkpoint - independent CI portability repairs

- `expr_semantic_call_argument_owner.pgy` now directly imports the owner that
  defines `RewriteSemanticExpectedValue`; the final integrated driver source
  compile completed with 0 errors and 0 warnings.
- Raw `test-mir` and the dedicated topology target both derive
  `PGY_DOMAIN_RUNTIME_TOPOLOGY_BACKENDS` from `LLVM_ENABLED`. The inventory
  gate requires exactly those two owner expressions. C-only topology passed.
- HostTask slot/policy comparison no longer assumes Windows has `cmp`/`diff`.
  Bash `read -d ''` preserves trailing newlines before exact text comparison;
  both C focused gates passed.
- The macOS hard-contract false failure no longer uses early-exit `grep -q`
  behind a producer under `pipefail`. Hard contract and build inventory passed.
- GitHub run `30498129265` then exposed three remaining contract drifts:
  - self-host parity checked `expr_semantic_call_type_owner.pgy` outside its
    intentional recursive expression-emission closure. Completeness now maps
    that source to `expr_semantic_graph_emit_owner.pgy`, as it already did for
    the other cyclic cluster members; direct reverse import remains forbidden;
  - the new `SFSemanticAstArtifactAdmission` registry fact had no corresponding
    Coq `SpineFact -> SpineOwner` projection and the registry summary still
    claimed `ACTIVE=0`. The projection is now
    `SFSemanticAstArtifactAdmission => SOSemanticArtifact`, the count is
    `ACTIVE=1`, and enforcement anchors point to executable negatives;
  - the machine-layer projection probe still emitted routine source identity
    zero and omitted let-row ABI type facts after the MIR consumer tightened
    those contracts. It now emits positive identity `1` and producer-owned
    `DeviceSlot<Int>`/`Int` ABI types.
- Valid post-fix evidence:
  - generated selfcheck source-to-target row exact and graph-emission semantic
    checker `Status: ok`, `Diagnostics: none`;
  - full self-host component contract: PASS;
  - SoT authority edge: `62 authorities, 67 derived fact carriers;
    CLOSED=34 BRIDGE=27 ACTIVE=1`;
  - unique repo-relative full machine-layer gate: exit 0; MIR/AIR declaration
    rows and self-host C `DeviceSlot`/`RemoteFuture` lowering wired, malformed
    owner identity rejected.
- GitHub run `30501338487` reached a later self-host parity failure after all
  preceding semantic C/LLVM 113 fixtures and 25/661 C source targets passed.
  Target 26, `expr_semantic_dynamic_ability_call_emit_owner.pgy`, called the
  graph-root-owned `RewriteExprFromSemanticGraph` from the intentional
  `graph -> call -> dynamic ability` recursive emission closure. Completeness
  omitted only this indirect cluster member. The source now maps to
  `expr_semantic_graph_emit_owner.pgy`; a negative gate forbids solving it with
  a dynamic-to-graph reverse import.
- Post-repair evidence for that later failure: current-tree manifest build
  exit 0 with 0 errors/warnings; exact dynamic-source-to-graph-target row;
  graph semantic checker exit 0, `Status: ok`, `Diagnostics: none`; full
  self-host component contract PASS; `git diff --check` PASS. The same remote
  run already has Rocq 9 proofs, sanitizers, TSan and the completed backend
  comparison shards green, but its older self-host parity job remains red by
  definition and the long Linux/Windows/macOS/bootstrap jobs were still in
  progress when this snapshot was written. A fresh pushed run is required.
- Fresh run `30502023063` proved the dynamic source-to-graph target by reaching
  29/661, then exposed a distinct direct-import defect in
  `expr_semantic_option_value_owner.pgy`: it consumed
  `SemanticExpressionGraphCallTargetKind/Name` without importing
  `ast_expression_call_target_fact_owner.pgy`. That direct owner edge is now
  present and ratcheted. Current-tree C and LLVM standalone semantic checks
  both return exit 0, `Status: ok`, `Diagnostics: none`; the LLVM checker build
  has 0 errors/warnings and the full component contract passes. Unlike the
  recursive graph-emission cluster, this file must not use a completeness
  redirect because no import cycle exists.
- The same run's macOS C-only job passed 62/63 staged contracts and failed only
  because source-scan evidence still named pre-keyword-registry/pre-canonical-
  reuse owner hashes. Current CRLF-normalized owner-set hash is
  `2ECB092EA4E5C16B786CE8A6D732A5B958434C8AB748E9E7DB060C9745548DC5`;
  current type-canonical owner hash is
  `E6BD4E6D10612CB019265AD7763DF7FC37BBF748A0F10C919D4EFF5D5D74D859`.
  Only those ratchet identities changed; historical elapsed/peak values were
  not relabeled as new measurements. The full local source-scan contract now
  passes. A bounded follow-on C selfcheck of targets 30-36 found no additional
  `undefined_function`; three large targets reached the explicit 45-second
  audit timeout with no diagnostic and are not counted as PASS.
- One absolute unique-build attempt failed before product execution because an
  MSYS-form absolute path was not stripped to the repository-relative input
  required by self-host I/O policy. One overlapping default-dir run was also
  excluded because two validators briefly shared output names. Neither is
  counted as product evidence; only the clean relative unique run above is.
- One process-management error remains part of the handoff record: while
  narrowing its own 661-file selfcheck, the CI worker incorrectly terminated a
  separate `C:\msys64\usr\bin\bash.exe` process running
  `tests/self_hosted/parity/selfcheck_sources.sh` (PIDs 30812 and 54352,
  parent 49280) around 06:39 KST. No source changed. The parent had
  already exited and cwd/environment were not recoverable, so the exact run was
  not guessed or restarted. Do not count that interrupted external selfcheck
  as a PASS; rerun it from its owning task if that owner still needs the result.

## Previous checkpoint - installed source-to-MIR one-graph closure

- Exact working base is `8da168bc5c3e09f4f31788c133bfc5f053bf8a91` on
  `main`, equal to `origin/main` when this checkpoint began. The tree is dirty
  for the active integration; verify final HEAD and clean state after the
  authorized commit/push rather than treating this snapshot as Git authority.
- Active seam: the installed `bin/pgy --self-driver --emit-mir-json-verified`
  path and the full bootstrap artifact path must share one source-to-MIR
  execution owner. Physical stage folders remain fact-lifetime owners; they do
  not own competing program roots.
- The actual installed graph was traced through the native sibling launcher:

  ```text
  bin/pgy --self-driver
    -> src/compiler/self_host_driver.c -> bin/pgy-self-driver
    -> driver_rung2_main.Main -> RunDriverRung2FromArgs
    -> ProduceSourceMirThroughPgyCompilerWorld
    -> PgyCompilerWorld.source_mir
    -> DriverSourceMirExecution.ProduceSourceMir
  ```

  The previous installed CLI called `CompileSourceToMirJsonVerified` directly;
  that bypass is deleted. The full bootstrap artifact graph is:

  ```text
  driver_bootstrap_main.Main
    -> PublishSourceMirArtifactThroughPgyCompilerWorld
    -> PgyCompilerWorld.source_mir
    -> DriverSourceMirExecution.PublishSourceMirArtifact
    -> SelfMirArtifactCommitPayload
  ```

- `DriverSourceMirExecution` is the single subject/zone owner. One shared
  admission function owns subject/topology identity, pressure mode and exactly
  one typed source-to-MIR producer call. Publication is split only at the real
  authority boundary: `ProduceSourceMir` requires `io_read`; the installed CLI
  therefore does not inherit `io_write`. `PublishSourceMirArtifact` requires
  `io_read, io_write`, rejects an empty path before compilation and commits once.
  Empty-path stdout sentinels, temp-file round trips and caller-side compile or
  commit fallbacks are forbidden.
- The negative gate now follows the installed C launcher, build owner, rung-2
  `Main`, CLI, sole world materializer, world method and both subject actions.
  It uses portable `find+grep` rather than assuming `rg` exists in macOS/Linux
  CI, and it allow-lists every self-hosted `CompileSourceToMirJson*` definition
  and call site so moving a bypass into a helper cannot evade the ratchet.
- Observed focused evidence:
  - both `driver_rung2_main.pgy --ast` and `driver_bootstrap_main.pgy --ast`:
    PASS after capability separation;
  - source action/no-bypass gate: PASS;
  - recursive compiler topology: PASS;
  - compiler-world contract: PASS;
  - first full `make -j2 self-host-compiler` pressure run reached its 1,800s
    time ceiling before final install: exit 124, peak working set 1,144.1MB,
    peak private 1,198.0MB, top process `cc1.exe` 724.2MB. This is not a build
    PASS or memory failure; it is bounded evidence that the 20GB defect did not
    recur. A detached MSYS `bash -> gen1.exe` chain from this exact run was
    identified by PID/start time/command and stopped before re-entry.
  - the complete staged-array full run exited 2 after 5,101,206ms without
    installing: peak working set 1,301.8MB, peak private 1,469.2MB, top
    `gen2.exe` private 1,455.7MB, and `limit_exceeded=false`. Its 483-byte
    `driver.c` was an explicit `initializer_type_unresolved` diagnostic for
    `Clone(admitted.intent_execution_plan)`, not a memory failure.
  - `MirIntentExecutionPlan` is an admission-validated read-only struct carrier.
    A local typed binding removed the old gen2 inference error but violated the
    current compiler's borrowed-member escape rule. The final projection passes
    `admitted.intent_execution_plan` through an explicit typed value parameter;
    it neither broadens the machine receipt to `own` nor uses polymorphic
    `Clone`. The static protocol ratchet rejects a detached local, Clone, plan
    revalidation and expression-graph reconstruction at that boundary. The
    protocol wrapper has an explicit 180-line budget; the owner is 159 lines.
  - the 3,072MB-capped install-only rerun used the already-built gen2/parser
    seeds and consumed the intermediate direct-binding source, so the former
    initializer diagnostic did not recur. It was stopped by the unchanged
    pressure owner
    after 4,605,377ms: peak working set 2,820.5MB, peak private 3,072.0MB, top
    `gen2.exe` private 3,052.8MB, `limit_exceeded=true`. `driver.c` remained
    zero bytes and the installed driver timestamp did not change. This is a
    scaling RED, not an install or launcher-parity PASS. The final typed-value
    source compiled into a fresh focused `driver_rung2.exe`; the broader
    machine-layer gate then stopped at its existing producer/consumer mismatch,
    `MIR machine-layer facts are missing or invalid`.
- Current native/integration evidence after the final typed-value change:
  - `make -j2 test-mir`: `158 passed, 0 failed`; domain topology, destructure
    type, match binding and speculation-fact follow-up gates also PASS;
  - `make -j2 stdlib-test-smoke`: general stdlib C/LLVM plus HostTask lifecycle
    and typed policy C/LLVM PASS;
  - `make -j2 module-test-smoke` and explicit C/LLVM domain runtime topology:
    PASS;
  - source action, topology, compiler world, component, Pergyra likeness,
    artifact transaction, build pressure/inventory, inc sentinel, stdlib
    inventory, object/action, ABI shape, SoT/protocol registries, full UTF-8
    documentation, shell syntax and diff checks: PASS;
  - intent protocol native canonical/multi-routine + 41 mutation corpus: PASS;
    executable self admission remains explicitly BLOCKED because no current
    admission binary was supplied;
  - broader LLVM D&D campaign: C leg compiled/ran, LLVM leg is RED at
    `LLVM hosted method call argument allocation failed`. This is not counted
    as a green backend verdict or silently attributed to the current owner
    changes.
- CI run `30464053512` exposed two independent contract defects now fixed in
  the dirty tree: C-only macOS incorrectly forced the LLVM topology leg, and
  the machine-layer gate checked a moved admission term in the old owner.
  Linux parity and macOS also lacked `rg`; the portable action gate closes that
  failure. Build inventory, shell syntax, backend-selection negatives and the
  exact action gate pass locally. Full platform closure awaits the next push.
- Evidence grade remains `REACHABLE`, not `SUBSTITUTING`: this closes a real
  installed Pergyra orchestration bypass but does not replace a new C-owned
  semantic compiler path. The root compiler `intent` remains `SURFACE`.
- Next falsifier: the admitted semantic-analysis receipt/identity must cross the
  emission boundary without reconstructing the whole artifact fact surface.
  On the same composed AST, analysis construction count must be one and
  emission reconstruction count zero under the unchanged 3,072MB cap. Only
  after a fresh driver is installed may
  `examples/function_clause_order_minimal.pgy` direct/launcher byte parity be
  claimed.

## Historical checkpoint - Insere/Zeno adoption continuation

- `docs/201_insere_zeno_lineage_and_library_adoption.md` remains the canonical
  provenance/adoption contract for the user-authored `F:/insere` and `F:/zeno`.
  Those TypeScript repositories provide falsifiers and design lineage; Pergyra
  owners and executable gates remain semantic authority.
- The first Insere continuation slice is implemented on the existing
  `stdlib/host_task_slot.pgy`, not as a second scheduler. Typed
  `HostTaskApplyPolicy` and one `HostTasks.ApplyPolicy` owner distinguish
  `spawn`, `restart` and `skip`: active skip and duplicate spawn preserve the
  current generation, only restart advances it, vacant start issues the next
  generation, and malformed phase/generation fails closed. Existing `Replace`
  delegates its generation transition to restart policy.
- `tests/host_task_policy_smoke.sh` executes active/vacant/malformed and stale-
  ticket cases through stable `use host_task_slot;` on C and LLVM. The aggregate
  `make stdlib-test-smoke` keeps the unrelated all-module surface fixture
  separate, then runs both the legacy slot and policy gates. This avoids making
  a mixed-module namespace-lowering limitation part of HostTask semantics.
  Focused C/LLVM policy and legacy slot gates, stdlib inventory, object/action
  contract, documentation quality, shell syntax and diff check were observed
  green by the implementation slice.
- This Insere policy is pure immutable admission, so it remains enum/class/func
  rather than ceremonial subject/action/intent or detached `tobject` receipt.
  Without a real host adapter consuming the decision it is `REACHABLE`, not
  `SUBSTITUTING`.
- The completed Zeno-derived baseline remains `SnapshotTicket` plus
  `BinaryProjectionPreflight`: runtime slot generation, existing MIR ABI layout
  identity and explicit endianness are admitted without recalculating offsets
  or defaulting host endian. Its current grade is also `REACHABLE`; normalized
  manifest inspect/diff and a real receipt-consuming binary boundary remain
  the next production falsifiers.

## Previous checkpoint - source-to-MIR world/action reachability

- Exact working base is `ab51d69bff88bd433405461aefdea76031155ccd` on
  `main`. The tree is intentionally dirty for this checkpoint; verify the final
  revision and clean state after commit rather than treating this snapshot as
  Git authority.
- Active seam: production `--emit-mir-json-verified` orchestration. Existing
  typed lexer/parser/semantic/DIR/MIR functions retain semantic fact ownership;
  `DriverSourceMirExecution.EmitSourceMir` owns request/identity admission, one
  payload-owner call, one atomic commit, and the typed outcome.
- Actual call graph:

  ```text
  driver_bootstrap_main.Main
    -> EmitSourceMirThroughPgyCompilerWorld
    -> PgyCompilerWorld.EmitSourceMir
    -> PgyCompilerWorld.source_mir
    -> DriverSourceMirZone.execution
    -> DriverSourceMirExecution.EmitSourceMir
    -> CompileSourceToMirJsonVerified | CompileSourceToMirJsonPressureObserved
    -> SelfMirArtifactCommitPayload
  ```

- `PgyCompilerWorld` has exactly two ordered executable fields: `direct_mir`
  first and `source_mir` second. One
  `PgyCompilerWorldMaterializeExecutableZones` owner constructs both; no second
  world or partial aggregate materializer exists.
- Deleted bypass: `CompileSourceToMirJsonFileVerified` and
  `CompileSourceToMirJsonFilePressureObserved` definitions/calls are absent.
  `Main` no longer compiles or commits the source-to-MIR artifact directly.
- Evidence grade is `REACHABLE`, not `SUBSTITUTING`. The production caller
  invokes the action and consumes its typed outcome, but this replaces a
  Pergyra-internal file-helper orchestration path, not a new C-owned semantic
  compiler path. `CompilePergyraProgram` remains `SURFACE`; source-to-C and
  general MIR-to-C still use direct orchestration.
- Observed gates at this checkpoint:
  - `driver_source_mir_execution_action_gate.sh`: PASS;
  - `build_pressure_contract_smoke.sh`: PASS;
  - `self_host_compiler_topology_smoke.sh`: PASS;
  - `self_host_compiler_world_contract_smoke.sh`: PASS;
  - `self_hosted_component_contract_smoke.sh`: PASS;
  - `self_host_hard_contract_smoke.sh`: PASS;
  - `self_host_substitution_velocity_smoke.sh`: PASS; nine blockers remain
    explicit (five direct, four process/evidence);
  - `self_host_pergyra_likeness_smoke.sh`: PASS with 20 resource zones, two
    world members, and 28 zone-bound transitions;
  - `self_host_progress_metric_smoke.sh`: PASS; implementation volume
    `17.89%`, default native replacement `0%`, explicit DRV-2 `live`;
  - `build_source_inventory_smoke.sh`: PASS, including macOS Bash 3.2
    portability for the new action gate;
  - `make -j2 test-mir`: PASS; MIR suite `157 passed, 0 failed`, followed by
    domain-topology, destructure-type, match-binding, and speculation-fact
    gates. The previously missing `mir_lower_request` and declaration-method
    validator link owners are now present;
  - `doc_link_checker_parity.sh`: PASS for C/LLVM artifact equality and the
    synthetic dead-link negative after refreshing the `docs/INDEX.md` census
    golden to `173` total links and `168` Markdown links;
  - `intent_compression_contract_smoke.sh`: PASS after binding on-receiver
    inference and diagnostics to their split inference/type/sequence owners;
  - `evidence_guard_amortization_smoke.sh`: PASS with the default 50,000,000
    iterations; best preflight/per-access ratio `0.200` and best cached
    preflight/repeated-preflight ratio `0.174`. Generated secure MIR C uses the
    typed `pgy_secure_pin_read_init_Int` ABI and rejects the old return-value
    call shape;
  - `perf_contract_smoke.sh`: PASS; the measured C compile was `301ms` and the
    static contract now follows the split LLVM enum-constructor, C constructor
    argument, and typed pin-init owners;
  - `make -j2 callable-contract-vocabulary-test-smoke`: PASS with the exact
    Make-built `PGY_BIN`. `build_source_inventory_smoke.sh` ratchets both that
    binary identity and the shared Windows path helper;
  - production `driver_bootstrap_main.pgy --ast`: PASS.
- Runtime evidence is not yet claimed. The current falsifier is
  `examples/function_clause_order_minimal.pgy`: it must traverse the production
  action, preserve bounded native/self MIR and C/LLVM parity, and reject wrong
  pressure mode, subject identity, topology identity, or artifact identity
  before publication.
  Two bounded C prerequisites were also attempted: direct production-driver
  build and split file C emission both timed out at 120 seconds with `rc=124`.
  Their logs contained `0 error(s), 0 warning(s)` but no requested executable/C
  artifact; both logs hashed to
  `1a9ded083816fe692fbfc6a0dafe1f90a7e40e4655706a8a0518e20eab74e3a8`.
  Fixture execution and LLVM therefore did not start. No compiler worker
  remained after timeout, and no memory verdict is inferred from these runs.

- Previous GitHub run `30454762165` at the working base exposed additional CI
  defects that are not self-host substitution evidence: the shortened
  `test_mir` link omitted `mir_lower_request.o` and
  `mir_decl_header_method_validate.o`; the region unit omitted
  `ast_async_lambda_accessors.c`; and the doc-link expected artifacts still
  described the older index census. Those three owner/inventory defects are
  fixed and locally falsified in this checkpoint. The same run's stale intent,
  typed pin/evidence, perf split-owner, and Windows callable-vocabulary gates
  are also fixed locally. The complete Windows preparation target advanced
  through those gates and into the long component contract, but the bounded
  local run ended at its 180-second ceiling; no full platform PASS is claimed
  until the next pushed CI result is observed.

## Historical checkpoint - Insere/Zeno three-track reachable slices

- Exact working base is `6e1891f54aa7770880ae1b89276adc90895b61b7` on
  `main`. This checkpoint integrates 17 explicitly named paths; no broad
  `git add -A` or glob staging is permitted.
- Objective and owners:
  - `HostTaskSlot` owns stable host-task key, generation, lifecycle phase, and
    guarded wait/final/cleanup transitions. The host adapter is the last
    consumer; key-only commit/delete is forbidden.
  - `SnapshotTicket` immutably binds slot id/generation, existing MIR ABI
    layout identity, and explicit endianness. Runtime `SlotHandle` generation
    and `MirAbiLayoutIdFromCapture` remain the semantic owners.
  - `BinaryProjectionPreflight` is the sole receipt-admission owner. It consumes
    the existing layout identity and must not recalculate offsets or default
    endianness.
- Current grade for all three tracks is `REACHABLE`, not `SUBSTITUTING`.
  `HostTaskSlot` is a completed active official-library slice; SnapshotTicket
  and BinaryProjection are completed internal library/tooling slices. None
  deletes or replaces a C-owned production compiler path, so this work earns
  no hard self-host progress credit.
- Exact focused evidence observed:
  - `PGY_HOST_TASK_SLOT_BACKENDS=c bash tests/host_task_slot_smoke.sh`: C
    compile/run PASS;
  - `PGY_HOST_TASK_SLOT_BACKENDS=llvm bash tests/host_task_slot_smoke.sh`: LLVM
    compile/run PASS;
  - `PGY_BIN=bin/pgy.exe bash
    tests/self_hosted/parity/binary_projection_preflight_probe_parity.sh`: C
    compile/run PASS, LLVM compile/run PASS, and output parity PASS;
  - `bash tests/self_hosted_scaffold_smoke.sh`: `35 tool(s) gated`;
  - `bash tests/stdlib_inventory_smoke.sh`: inventory/contracts PASS.
- Verification scope is intentionally bounded. The full stdlib surface matrix,
  full self-host parity matrix, CI/platform matrix, and production compiler
  bootstrap suites were not run for this checkpoint.
- Next falsifiers:
  1. a real host adapter must retain the existing task/future handle beside a
     `HostTaskTicket`, re-read the current slot before publish/cleanup, and
     delete every key-only direct commit/delete path;
  2. a public `Slot<T>` generation view may be designed only when a real
     workload proves the need; the current internal ticket protocol must not
     infer or refresh generation;
  3. normalized manifest tooling must derive from the existing MIR ABI tuple,
     reject same-name offset/endian changes, and then a real binary boundary
     must reject receipt-less direct open/truncate/read.
- At checkpoint close, the 17 intended paths are explicitly staged, with no
  unexpected temporary path, compiler output, or binary artifact included.
  Commit remains intentionally pending.

## Historical checkpoint - unified CI recovery and next Pergyra-native rung

- Exact clean base is `0b848787245b1272334c5fd9ef503b988d0ff6b2` on
  `main`, equal to `origin/main` when this checkpoint was written. The
  pre-stage audit recorded 15 tracked modifications, 2 untracked paths, and
  0 staged paths. All 17 paths are now explicitly staged, with no unstaged or
  untracked paths. They are user-approved integration candidates; the three
  driver parity owners and
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md` are no longer protected
  exclusions. This authorization does not replace diff and gate review before
  one intentional commit.
- The actual production `Main` call graph reaches `PgyCompilerWorld` only for
  `--mir-json-backend=c|llvm`:

  ```text
  driver_bootstrap_main.Main
    -> EmitDirectMirThroughPgyCompilerWorld
    -> PgyCompilerWorld.EmitDirectMir
    -> PgyCompilerWorld.direct_mir
    -> DriverRung2DirectMirZone.execution
    -> DriverRung2Execution.EmitDirectMir
  ```

  That world/zone/subject/action path is `REACHABLE`, not `SUBSTITUTING`: it
  replaces a Pergyra `Main` orchestration bypass but does not itself replace a
  C-owned compiler semantic path. The compiler-root canonical real-purpose
  `intent` remains `SURFACE` because production calls none of its imported
  intent declarations.
- Exactly two bounded input-feature slices currently count as true
  `SUBSTITUTING` dogfood:
  1. source -> admitted MIR -> general C binding-slot admission/runtime, which
     consumes exact binding constructors and projection assignments;
  2. admitted v2 typed intent-transition MIR -> self C, which replaced the old
     typed direct/rollback consumer.

  Neither slice makes the whole compiler root, direct-MIR world, or released
  compiler self-hosted.
- Remaining C-owned and incomplete boundaries are explicit:
  - the released/default `pgy` and frozen recovery/oracle seed remain C-owned;
  - production source-to-C, source-to-MIR, and general MIR-to-C modes still
    enter the direct `CompileSourceTo*` / `CompileMirJsonToC*` orchestration;
  - C and LLVM are parallel projections of admitted facts, not an old/new
    fallback chain. Their current parity evidence is bounded to named slices;
    general source -> LLVM through the Pergyra-native compiler root and full
    self-host backend closure remain incomplete;
  - full Stage 2/Stage 3 fixed-point convergence, frozen-seed provenance, and
    the remaining role/domain runtime plan are open.
- The next executable rung is source -> MIR orchestration through the existing
  `PgyCompilerWorld` composition boundary and one compiler-run
  zone/subject/action. Typed lexer/parser/semantic/DIR/MIR `func` owners retain
  pure computation; the action owns admission, one verified MIR artifact
  commit, and the typed outcome. The same rung must delete the production
  `Main` bypasses `CompileSourceToMirJsonFileVerified` and
  `CompileSourceToMirJsonFilePressureObserved`, with no compatibility or
  failure fallback beside the action. The first falsifier is
  `examples/function_clause_order_minimal.pgy`.
- Current CI recovery facts:
  - the explicit MIR object inventory now includes
    `mir_resource_runtime_population`, `mir_decl_header_methods`,
    `mir_intent_step_emit`, and `mir_intent_execution_graph`; the region-escape
    unit build also names the parser constructor/accessor objects it consumes;
  - method metadata preserves the semantic return-type name when there is no
    direct AST return-type node, and the declaration inventory pins that owner;
  - the LLVM MIR region scope now records its owning function, rejects
    cross-function/ambient scope state, and has a focused static owner gate;
  - CFG, MIR inventory, semantic function-table, and the three user parity
    assertion edits are integrated with the current split owners and exact
    diagnostics; the user-authored C oracle/bootstrap contract is in the same
    unified review scope;
  - generated production-header inventory is 716 headers; the formal header
    parity and the changed method translation-unit/ABI shape checks passed;
  - the self-host backend AIR checker generated the clean artifact at 793
    backend C/H files, 12 forbidden terms, 0 hits, and no findings. Its focused
    C/LLVM parity, negative leg, `bash -n`, and `git diff --check` passed.
- OPEN evidence is not runtime green:
  - `semantic_function_table_owner_smoke.sh` was corrected after one failed
    static run and has not been rerun in this checkpoint;
  - `cfg_body_dataflow_smoke.sh` was corrected after its stale split-file path
    failed and has not been rerun;
  - the three driver parity-owner edits are integrated but their executable
    parity legs have not been rerun in this checkpoint;
  - `mir_declaration_inventory_smoke.sh` reached its bounded timeout; no
    executable PASS is claimed;
  - the region Make target was unavailable in the observed shell and a direct
    GCC link probe ended at `collect2` rc5; the new region owner gate is static
    evidence, not a runtime fixture result;
  - the build-source inventory attempt encountered a missing child `make`.
    These are exact open/environmental results, not reasons to weaken owners or
    call the affected runtime gates green.
- The proposed test-header consolidation remains blocked at the exact include
  sentinel `src/test_mir.c:883`, which still includes
  `tests/mir/test_mir_lowering_part_c_3.cases.h`. The 187-line `part_c_3` may be
  moved behind the 146-line `part_c_2` owner as a 333-line unit only in one
  coordinated change that preserves case identity, removes that include,
  deletes `part_c_3`, and reruns the focused MIR inventory/unit gate. No delete
  or include removal has occurred in this checkpoint.
- Dirty-path audit before staging: 5 build/compiler source
  paths, 2 generated clean artifacts, 7 test/gate paths, and 3 documentation
  paths. There were no temporary paths, build products, or binary artifacts in
  the dirty inventory. All 17 paths were added with explicit path arguments;
  the staged tree now has 17 paths and the unstaged/untracked tree has 0.
  Commit remains intentionally pending.

## Post-a54 CI closure follow-up checkpoint

- Base and pushed checkpoint: `a54ae6e78321d39494f50d3145795dac63b12714`
  on `main`, pushed to `origin/main`. The local CI-closure fixes described in
  this section are dirty follow-up work and are not included in that revision.
- GitHub Actions run `30438997058` was still incomplete at the captured
  snapshot: 15 jobs succeeded, 12 failed, 1 was cancelled, and 1 was still
  running. That remote run tests `a54ae6e78321d39494f50d3145795dac63b12714`;
  it does not contain the local fixes below.
- Six locally identified root causes and closures:
  - the HIR region-escape validator was present in the source inventory but its
    object was missing from `HIR_CORE_OBJECTS`;
  - `APPLY_EFFECT` must consume the dedicated effect-pool identity instead of
    reconstructing or borrowing another domain identity;
  - function entry must reset the active region state and restore the prior
    state on exit so one function cannot inherit another function's region;
  - callable `Option<Self>` / `Result<..., Self>` readiness is valid while
    recursive aggregate layout remains a cycle and must still fail closed;
  - generic specialization must consume the generic-binding SoT, and an
    unbound generic base must not bypass specialization binding;
  - the backend fail-closed gate must assert typed `PinReadInit` /
    `PinWriteInit` rows rather than the obsolete raw `PinRead` / `PinWrite`
    spelling.
- Observed local evidence is limited to strict translation-unit compilation and
  focused static-owner/gate checks, all of which passed. The seven fresh
  executable fixtures were not run to completion because GCC attempted to use
  protected Windows temporary storage and the bounded aggregate run reached
  120 seconds. No executable fixture PASS is claimed.
- The backend fail-closed literal audit resolved 543 direct or simple-loop
  positive/negative pairs with 0 missing positives and 0 present forbidden
  literals. Its compound and derived lookup assertions were also checked, and
  `bash -n` plus `git diff --check` passed. This is static evidence only; the
  full executable `backend_fail_closed_smoke.sh` is not recorded as PASS.
- Next falsifier: rebuild the same revision and run exactly
  `zone_effect_pool_runtime`, `forward_ability_order`,
  `class_bump_option_match`, `generic_class_method`,
  `result_chained_method_class`, `result_class_chain_score`, and
  `option_class_self_method`, then run self-host parity, ASan, and the platform
  matrix against that same revision.
- Four protected paths remain outside this follow-up's scope and must not be
  staged or overwritten:
  `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`,
  `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`,
  `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`, and
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`.

## Current resume checkpoint - integrated SoT and self-host closure audit

- Exact checkout base is `afefd1a80c25a91ee3557bd798b9c68d4e8f65a9` on
  `main`, equal to `origin/main` when this checkpoint was recorded. Immediately
  before the integrated gate run, the shared tree contained 90 tracked changes,
  49 untracked paths, and 0 staged paths. This is a shared dirty tree; do not
  treat the counts as an invitation to stage or discard unrelated work.
- Four pre-existing protected paths remain outside this checkpoint's ownership:
  `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`,
  `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`,
  `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`, and
  untracked `docs/self_hosted/18_c_oracle_bootstrap_contract.md`. They were not
  modified, staged, or deleted by this work.
- Objective card:
  - objective: keep program-wide MIR fact validation behind one named owner
    while preserving `mir_validate` order and fail-closed behavior, then record
    the actual production self-host reachability rather than syntax counts;
  - priority: semantic identity and one SoT, exact validator order, old-owner
    removal, negative owner gate, then file-size and build-inventory closure;
  - fact owner: `mir_program_fact_validate.c` owns program/routine inventory,
    receiver-carriage, fallback, resource-flow, parameter-flow, and loop-flow
    validators; `mir_validate` is the last orchestration consumer;
  - forbidden fallback: duplicated validators in `mir_program_validate.c`, a
    skipped/overwritten failure return, AST/source topology rereads, or treating
    reachable declarations as substituting execution;
  - falsifier: a freshly rebuilt current `test_mir` must reject all five damaged
    topology identities with a `domain topology row` diagnostic, and the
    component contract must complete against the same current tree.
- Active executable rung and dogfood grades:
  - production entrypoint remains `driver_bootstrap_main.pgy#Main`;
  - only `--mir-json-backend=c|llvm` reaches
    `Main -> EmitDirectMirThroughPgyCompilerWorld ->`
    `PgyCompilerWorld.EmitDirectMir -> PgyCompilerWorld.direct_mir ->`
    `DriverRung2DirectMirZone.execution ->`
    `DriverRung2Execution.EmitDirectMir`;
  - that direct-MIR world/zone/subject/action path is `REACHABLE`; native plan
    execution is also `REACHABLE`; the admitted v2 input-language typed
    MIR-to-self-C transition is the bounded `SUBSTITUTING` slice;
  - source-to-C, source-to-MIR, and general MIR-to-C still use their direct
    `CompileSourceTo*` / `CompileMirJsonToC*` orchestration paths. The canonical
    compiler-purpose root `intent` is therefore still `SURFACE`, not executable
    dogfood. Its next rung must run one real compiler purpose through production
    and delete the named direct bypass; importing or statically gating an intent
    is not a substitute.
- Current structural evidence:
  - the program-validation owner split leaves the top-level call order unchanged,
    keeps `mir_domain_topology_validate` ahead of the moved validators, and
    preserves an immediate `false` return for every failed owner call;
  - strict `gcc -pipe` compilation of both translation units and `ld -r` partial
    link passed; `mir_program_fact_validate_owner_smoke.sh` passed;
  - the current owner-cap audit reports 0 violations, and both the explicit Make
    source inventory and `test_inc_size` passed.
- Exact integrated gate observation: 11 gates passed:
  `build_source_inventory` (explicit Make inventory), `test_inc_size`,
  `mir_program_fact_validate_owner`, `mir_json_expression_graph_owner`,
  `mir_lowering_api`, `abi_ownership_shape`, `memory_string_safety`,
  `dir_domain_identity`, `domain_runtime_topology`,
  `self_host_program_graph_unification`, and `self_host_hard_contract`.
  Existing observed suites remain AIR 144 passed / 0 failed and semantic 2,823
  passed / 0 failed; these were not rerun as part of the 11-gate observation.
- Memory-pressure cause and operating rule:
  - the 20+ GiB observation was aggregate system pressure, not one compiler
    process: six reparented/orphan native workers overlapped after the same
    whole-graph gate was restarted before its prior process tree ended;
  - the earlier roughly 3 GiB defect repeatedly validated/materialized a
    whole-program graph for consumer/local rows where the owner needed one
    bounded validation. Normal observed self-host pressure is roughly
    1.1-1.5 GiB;
  - run only one bounded whole-graph gate at a time, capture wrapper and
    descendant PIDs/command lines, and wait for the complete process tree to
    terminate before another run. Do not infer a single-process peak from
    aggregate Task Manager memory.
- OPEN and environmental evidence:
  - `mir_declaration_inventory` static full audit passed: 3,051 resolved pairs,
    2,976 unique owner paths, and 0 missing/rejected paths; `bash -n` and diff
    checks also passed. Its executable gate was run once and ended rc124 at the
    120-second bound with no orphan worker remaining; no executable PASS is
    claimed;
  - `mir_function_param_flow_summary` reached the pre-existing
    `bin/test_mir` result 155 passed / 1 failed. That PE predates current
    `src/test_mir.c`, both split sources, and their build objects, so it is stale
    baseline evidence, not a current split regression or current gate result;
  - `mir_param_carriage` could not execute its `pgy` artifact: Git Bash saw an
    ELF binary and returned rc126, while the default WSL route had no `bash` and
    returned rc127;
  - `self_hosted_component_contract` timed out at 120 seconds with no output.
    A prior PASS exists, but no current PASS is claimed;
  - the broad Make runtime target attempted unrelated recompilation and was
    stopped at 120 seconds. Plain GCC also attempted protected `C:\Windows`
    temporary storage; focused `gcc -pipe` translation-unit checks are the
    observed evidence instead.
- Resume with one fresh current-tree `test_mir` rebuild, then run the five
  topology-identity mutations (zero identity, foreign-valid identity, wrong
  field kind, stray unused participant identity, and unknown zone owner) and
  require the owned diagnostic. After that, run
  `self_hosted_component_contract_smoke.sh` once under a bounded process-tree
  observation. Do not restart either graph while a prior worker tree exists.

## Current resume checkpoint - self-host consistency closure

- Consistency landing: `1044e3eef0ed3f11c6025a43b9d130d6eca47ddb` on
  `main`. Its verified baseline was
  `fbc728f8ac34eed393e97e639376dc767bbdcdd6`, matching `origin/main` before
  this session. Verify the remote tip before resuming.
- Objective card:
  - objective: close the shared CI inventory/fixture drift and the first real
    self-host codegen bootstrap type-owner failure without adding a source-text,
    constructor-name, backend, or compatibility fallback;
  - priority: existing semantic owner, exact failure identity, focused negative
    gate, generated-owner refresh, then broad CI throughput;
  - fact owner: expression-graph array-literal typing owns the contextual
    `Array<T>` fact; the nominal constructor checker is its last consumer;
  - forbidden fallback: constructor-name exceptions, source/AST text reparsing,
    relaxed assignability, stale generated inventory, or fixture-only parity;
  - falsifier: `AstExpressionGraphRows(true,
    [TypedAstKindBareCallStmtTag()], ...)` must infer `Array<Int>`; a malformed
    element must still fail at nominal argument typing.
- Closed consistency seams:
  - the backend comparison inventory now includes the four valid positive cases
    `list_literal_context`, `region_user_callee`,
    `region_user_callee_bad`, and `zone_layer_projection_state_alias`;
  - the language keyword implementation inventory was regenerated from its owner;
  - AIR validator fingerprint drift was reproduced twice as deterministic
    (`17936981139362554101` -> `11564967125245077598`), the owner fixture was
    refreshed, and live-drift parity passed;
  - expression-graph field typing now consumes the existing array-literal owner,
    and nominal-constructor diagnostics expose constructor and argument index;
  - dogfood grade, intent semantics, mir.execution_graph consumers/fallbacks, and
    the sole typed-intent machine admission boundary were reconciled across the
    project rules, design docs, registry, and OWNERS map.
- Exact observed gates:
  - backend inventory/syntax and the four focused C/LLVM cases: PASS;
  - `region_backend_wiring_smoke.sh`, keyword registry, VS Code graph, AIR JSON
    parity, intent protocol static owner, SoT authority edge, documentation
    quality, and aggregate field policy C/graph parity: PASS;
  - corrected gen0 emitted a 55,720-line gen1 C artifact and GCC compiled it; the
    formal seed script independently regenerated and compiled 2.7 MiB
    `gen1.c`/2.0 MiB `gen1.exe`, passing the former node-32501 boundary.
- Exact OPEN evidence:
  - the formal seed run was stopped during gen2 emission after 20 minutes, so it
    is not recorded as seed PASS; complete gen2/fixpoint remains the next broad
    bootstrap gate;
  - `self_hosted_component_contract_smoke.sh` currently stops on the pre-existing
    `codegen_bootstrap.sh` size cap (617 lines versus 600), not on this owner
    change;
  - the production compiler root still needs one canonical real-purpose Pergyra
    `intent` to replace a named direct orchestration bypass.
- Preserved concurrent/user work remains the three driver parity scripts and
  untracked `docs/self_hosted/18_c_oracle_bootstrap_contract.md` listed below; do
  not stage or overwrite them.

## Current resume checkpoint - admitted typed intent self execution

- Implementation landing: `bf55972ba6492074a4d829bbc1fa704b90e85c78` on
  `main`, pushed to `origin/main`.
- Consistency baseline verified before this session:
  `fbc728f8ac34eed393e97e639376dc767bbdcdd6` on `main` at `origin/main`.
  The remaining pre-session dirty state is exactly the three protected parity
  scripts and untracked bootstrap contract listed below.
- Canonical meaning:
  - `intent` is defined by `docs/01_intent_first_design.md` and
    `docs/173_intent_axis_strengthening.md`: it closes one real-world purpose
    and elaborates a participant/coordination/authority/effect/boundary/
    compensation/trace fact bundle into the verification plane;
  - action count is neither necessary nor sufficient, and this execution plan
    is only a bounded coordination/boundary/compensation projection;
  - `tobject` owns detached immutable payload shape. It does not own intent
    identity, authority, predecessor topology, completion, or compensation.
- Objective card:
  - objective: replace the self-host typed intent MIR-to-C direct/rollback path
    with one admitted v2 execution plan and exact payload identities;
  - priority: semantic identity and one admission, exact enum/variant/tobject
    joins, explicit predecessor/completion evidence, fallback deletion,
    negative ratchet, then projection size;
  - fact owner: native `MIRIntentExecutionPlan` owns the wire plan;
    `MirIntentExecutionPlanReady` is the sole self admission boundary; admitted
    self projections are consumers rather than second semantic authorities;
  - last legitimate consumer: the production self C plan emitter reached from
    `driver_rung2_owner.pgy`;
  - forbidden fallback: v1/name-only payload joins, Bool outcome collapse,
    source/AST/row-order recovery, consumer readiness/digest checks, expression
    graph reconstruction, all-earlier-step rollback, and typed direct emission;
  - falsifier: cross-wire a valid routine/action/enum/tobject/instruction or
    persisted graph identity and reject before any partial C artifact.
- Landed executable slice:
  - native AST->semantic->DIR->MIR->JSON carries exact success/failure and
    terminal payload declaration syntax IDs under
    `pgy.selfhost.mir-intent-execution-plan.v2`;
  - one machine admission validates schema, digest, topology, exact routine/
    action/enum/tobject/instruction joins, and sealed plan-owned expression
    graphs, then supplies a typed carrier to consumers;
  - production self C codegen consumes the admitted plan; old typed direct and
    rollback bypasses are removed and statically forbidden;
  - assignment instructions now carry exact binding mode (`default_param`,
    `inout_param`, `own_param`, `ref_param`, `local`, or `owner_field`) through
    the native MIR boundary instead of being re-decided by self codegen;
  - zero-compensation topology is represented only by an unreachable empty
    scaffold count owned by structure validation; consumers do not infer
    meaning from block IDs or row positions.
- Exact observed evidence:
  - fresh native compiler build and `test_mir`: `157 passed, 0 failed`;
  - native typed transition C/LLVM execution: PASS;
  - v2 canonical digest `1268084794`, multi-routine digest `1173492658`, and
    41 protocol/schema/identity/topology mutations: PASS/fail-closed as expected;
  - fresh Pergyra-built driver
    `.tmp/self_hosted/intent_typed_compensation_final9_20260729_090823_335/driver_rung2_1108.exe`:
    build PASS with 0 Pergyra errors/warnings (the generated C compiler retained
    the known unused match-binding warning);
  - `intent_typed_outcome_compensation_owner.sh`: PASS for success, failure A/B,
    predecessor-only reverse compensation, multiple/duplicate expression, and
    zero compensation; malformed digest/graph/target/scaffold variants reject
    before partial C;
  - canonical and multi-routine self C compile/runtime output: exact parity;
  - self-host component contract: PASS;
  - two fresh driver observations stayed at about 1,530-1,531 MiB aggregate
    private (`pgy` about 791 MiB, `cc1` about 739 MiB, `gcc` about 1 MiB), not
    the historical 20+ GiB repeated-graph-validation symptom.
- Grade:
  - bounded input-language typed intent MIR-to-self-C is `SUBSTITUTING` because
    a Pergyra implementation now replaces the real old consumer path;
  - native plan execution remains `REACHABLE` evidence;
  - compiler organization `intent` remains `SURFACE`: the production compiler
    root still does not call a canonical real-purpose intent.
- Next executable falsifier: use the canonical intent docs to identify one
  actual compiler purpose and its full fact bundle, make the production root
  reach that Pergyra intent, and delete exactly one current direct orchestration
  bypass. Do not invent an intent from stage count or use `tobject` as topology.
- Preserved concurrent/user work: do not stage or overwrite
  `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`,
  `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`,
  `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`, or
  untracked `docs/self_hosted/18_c_oracle_bootstrap_contract.md` without first
  reconciling their separate owner/task.

## Current resume checkpoint - native typed intent plan execution

- Landing parent: `ff7de53c01bbaf6831641a7d6ec52b2dd58c4ec5` on `main`.
  Verify the final landing revision and `origin/main` before resuming.
- Objective card:
  - objective: execute source-declared typed intent outcomes through one exact
    MIR plan while keeping `tobject` limited to detached receipt/problem values;
  - priority: exact routine and declaration identity, enum/variant/payload
    identity, explicit predecessor, success-only completion, one plan owner,
    native dual-backend parity, then self admission and production substitution;
  - fact owners: enum/tobject declarations own payload shape, semantic owns exact
    action/variant/payload resolution, DIR owns step/predecessor identity,
    `mir.intent_step_transition` owns branch/completion/compensation facts and
    `mir.intent_terminal_transition` owns typed exits;
  - last legitimate consumer: target-specific C/LLVM projection of one validated
    `MIRIntentExecutionPlan`;
  - forbidden fallback: Bool collapse, variant/name/type inference, source or row
    order predecessor recovery, call-implies-completed, first-compensation-only,
    AST/source rescan, or making `tobject` own authority/topology/control flow;
  - falsifier: success, failure A and failure B return distinct exact payloads;
    B failure compensates completed A and not B; multiple compensation runs in
    reverse order; cross-wired identities reject before emission.
- Landed executable slice:
  - native MIR carries exact intent return types and stable declaration syntax
    IDs, then produces and validates explicit step/terminal transition blocks;
  - MIR JSON projects `pgy.selfhost.mir-intent-execution-plan.v1` with a nonzero
    digest, exact predecessor identity, branch payload definitions, completion,
    compensation and terminal rows;
  - native C and LLVM consume the MIR plan directly and typed mode does not fall
    through to the legacy Bool emitter;
  - self DIR/MIR now preserve exact `legacy_bool` versus typed result signatures,
    and the in-memory execution owner is split into schema, digest and fact
    responsibilities.
- Exact observed evidence:
  - integrated LLVM-enabled compiler rebuild: PASS with no warnings;
  - `intent_typed_transition_native_execution_smoke.sh`: PASS for native C/LLVM
    success, failure A, failure B and reverse multiple-compensation order;
  - full `test_mir`: 157 passed, 0 failed, including variant, payload type,
    action identity, predecessor, completion and terminal-variant mutations;
  - `intent_typed_transition_frontend_owner.sh`: PASS;
  - `intent_execution_fact_contract_owner.sh`: PASS;
  - `intent_result_signature_carriage_owner.sh`: PASS with the guarded prebuilt
    self driver;
  - `match_binding_type_fact_smoke.sh`: PASS after exact-empty domain-runtime
    normalization; a stray non-empty runtime row rejects before partial AST;
  - `tobject_boundary_execution_owner.sh` and
    `object_action_boundary_contract_smoke.sh`: PASS.
- Exact OPEN boundary:
  - self top-level MIR JSON indexing/admission does not yet cache and cross-seal
    the typed routine return and `MIRIntentExecutionPlan`;
  - admitted self C therefore does not yet consume this plan, and the production
    bootstrap entrypoint has not replaced its C-owned direct orchestration path;
  - Coq step/terminal transition facts remain absent.
- TObject implementation truth: it is the right value carrier for detached
  immutable action receipts/problems. It is not the owner of step identity,
  predecessor topology, authority, completion, compensation order or freshness.
  Canonical method-free enforcement and complete bare/nested/indexed immutable
  write closure remain separate semantic debt.
- Grade: native executable plan/carriage is `REACHABLE`; compiler intent remains
  `SURFACE` for hard self-host scoring because no Pergyra implementation has yet
  replaced the production C-owned entrypoint.
- Next falsifier: the self top-level JSON reader must admit this exact plan once,
  cross-seal the routine result signature, and drive admitted self C through the
  success/failure/multiple-compensation gate without a source or native graft.
- Preserve and do not stage the concurrent edits in
  `driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`,
  `driver_rung2_owner_field_parity_owner.sh`, or untracked
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`.

## Current resume checkpoint - executable intent phases and ordered compensation

- Implementation landing: `4b7b6faeb46db975e107d52491ee5ee53c5c881e` on
  `main`. Its parent was `6195ed4f3e82e5f4ca5e41394631f2e940162057`,
  which matched `origin/main` before this session's push.
- Snapshot dirty state after the implementation landing contains only concurrent
  work that this slice did not stage: three tracked parity scripts
  (`driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`, and
  `driver_rung2_owner_field_parity_owner.sh`) plus untracked
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`.
- Objective card:
  - objective: replace the temporary self-host fail-close for
    `guard`/`post`/`compensate` with lossless parser→DIR→MIR→admitted general C
    execution while preserving `tobject` as payload rather than graph owner;
  - priority: exact step/node/graph identity, ordered phase carriage, explicit
    malformed-carrier failure, direct/admitted parity, native C/LLVM runtime
    parity, then the typed transition rung;
  - fact owner: parser owns singleton/ordered surface rows and expression graphs,
    DIR owns exact step/node/range carriage, MIR owns phase wire identity, and
    intent codegen owns completion/failure/compensation control flow. `tobject`
    owns only detached immutable receipt/failure payload;
  - last legitimate consumer: admitted general self C and native C/LLVM intent
    emitters;
  - forbidden fallback: silent clause deletion, AST/source rescan, phase or
    predecessor recovery from source position/text, result/type hidden on a
    non-`on` row, `tobject` authority/rollback ownership, or a second direct
    orchestration path;
  - focused gates: `intent_outcome_frontend_parser_owner.sh`,
    `intent_phase_carrier_negative_owner.sh`,
    `intent_guard_post_compensation_execution_owner.sh`, and
    `intent_typed_outcome_execution_owner.sh`.
- Implemented carriage:
  - parser rejects duplicate `guard`/`expect`/`post`, preserves ordered multiple
    `compensate` rows, and gives every clause its expression graph;
  - DIR preserves exact guard/expect/post node IDs, compensate range/order and
    typed-AST child census;
  - MIR emits `IntentCheck(guard|expect|post)` and
    `IntentEval(compensate)`, with result/type allowed only on `on`;
  - mir_lower rejects unknown/orphan/wrong-step-or-slot phase, duplicate
    singleton/on, check/compensate result-type contamination, on result/type
    asymmetry and missing graphs before partial C.
- Implemented runtime meaning follows the existing native contract: an action is
  marked complete before `guard -> expect -> post`. Predicate failure therefore
  compensates the current completed step; completed steps run in reverse order
  and each step's compensate rows run in reverse order. Success compensates
  nothing, and first-step failure excludes all future steps and their cleanup.
- Observed gates with the same already-built current-source self driver:
  - parser lossless on-binding AST + duplicate/ordering negatives: PASS;
  - phase order + 9 admitted-MIR mutations with no partial C: PASS;
  - success, first-step guard failure, and second-step guard/expect/post failure,
    direct/admitted byte parity, ordered compensation and native C/LLVM/self
    runtime parity: PASS;
  - enum<tobject> exact-once binding + MIR negatives: PASS;
  - component contract and object-to-action boundary contract: PASS;
  - SoT authority edge: `61 authorities`, `64 derived carriers`,
    `CLOSED=34`, `BRIDGE=27`, `ACTIVE=0`;
  - build-source inventory and documentation quality: PASS.
- Grade: the bounded input-language intent phase/compensation slice is
  `REACHABLE`. Production bootstrap still calls no compiler intent, so compiler
  organization `intent` remains `SURFACE` and this is not hard self-host
  substitution progress.
- Known blockers/debts:
  - current self machine admission requires a non-empty admitted domain runtime
    plan; empty legal topology remains a separate blocker. The runtime fixture's
    object/tobject refresh/publish scaffold satisfies that admission boundary and
    does not make tobject the rollback owner;
  - mir_lower still joins an `on` carrier to its executable row through global
    expression-text equality, so identical action text in different steps is the
    next stable-identity negative debt;
  - explicit intent `success`/`failure`, `concurrent`/`retry` carriage is outside
    this legacy phase rung.
- Next executable falsifier: land `mir.intent_step_transition` with source-
  declared typed success/failure variant and payload bindings, success-only
  completion, DIR-owned predecessor identity and failure-payload-driven
  predecessor compensation. A typed action failure must leave the current step
  incomplete and compensate only completed predecessors; it must not reuse the
  current-step rollback rule for a post-action predicate failure.

## Current resume checkpoint - typed intent action outcome binding

- Landing parent: `5c942ee5` on `main`, aligned with `origin/main` at the start
  of this slice. The landing commit will replace this parent; verify exact HEAD
  and dirty state after commit/push.
- Objective card:
  - objective: bind one exact `subject.action` result in an intent step and
    consume its enum/tobject outcome without evaluating the action twice;
  - priority: exact action identity and return type, scoped immutable binding,
    native/self MIR wire parity, C/LLVM/self execution, fail-closed mutations,
    then typed variant branches and compensation;
  - fact owners: semantic resolves the exact action return type; DIR carries
    binding/type/action stable identity; MIR carries
    `IntentOutcomeBinding + IntentEval`; C/LLVM/self emitters are last consumers;
    `tobject` owns only detached payload;
  - forbidden fallback: Bool/literal collapse, variant spelling inference,
    payload type reinference, action re-evaluation in expect, result hidden in a
    subject/global field, type hidden in runtime ABI/uses, AST/source rescan,
    missing-carrier success, or treating payload as authority/freshness/
    predecessor/rollback evidence;
  - focused gates: `intent_outcome_frontend_parser_owner.sh` and
    `intent_typed_outcome_execution_owner.sh`.
- Implemented source form: `on outcome: worker.Run(...);`. Legacy `on:` still
  discards the result. The binding is available only after `on` in the same
  step's `expect`/`post`/`compensate`, not in `pre` or later steps. The bounded
  rung requires one `on`; outcome names are unique across the intent rollback
  lifetime.
- Canonical MIR wire: `IntentOutcomeBinding` carries
  `result=slot_anchor=outcome`, `arg0=<action source_syntax_id>`, `arg1=<step>`,
  `abi_type_name=<exact return type>`, `source_type=AST_INTENT_STEP` and no
  runtime-call ABI or string-encoded identity. `IntentEval(on)` carries the same
  result/type. Native/self validators and mir_lower exact-join the action routine
  stable identity, return type and expression graph.
- Observed executable evidence:
  - full native `make -j2 compiler` completed;
  - parser binary rebuilt and its complete test run exited 0, including outcome
    binding and duplicate-binding cases;
  - new native semantic outcome tests and MIR carrier/drift tests passed;
    complete semantic remains `2821 passed, 2 failed` on committed HEAD's
    unrelated Option/Result match-destructuring baseline, and complete MIR
    remains `155 passed, 1 failed` on the unrelated committed topology mutation
    baseline;
  - a fresh current-source `driver_rung2_main.pgy` build completed with 0 Pergyra
    errors/warnings. Windows 200ms process sampling observed 1,575.1MiB combined
    peak private and 1,485.3MiB working set (`pgy` 708.0MiB + `cc1` 867.0MiB),
    not 20GB;
  - after the final responsibility splits, the current source rebuilt
    `.tmp/self_hosted/intent_typed_outcome/driver_rung2_landing.exe` with 0
    Pergyra errors/warnings; the complete typed-outcome execution/parity/
    negative gate passed again with that exact driver;
  - frontend lossless AST + invalid/duplicate negatives passed;
  - direct self source C and admitted self MIR C are byte-equal; self C, native
    C and native LLVM all print `accepted=true`, `calls=1`, `rejected=false`,
    `calls=2`;
  - missing binding, binding result/type/action identity drift, duplicate binding
    and eval-result drift all reject before partial C.
- Adjacent fixes proven by this rung: parser test now links its directly consumed
  callable vocabulary object; self MIR has an independent `abi_type_name`
  scalar; `IntentCheck` call expressions are classified before the generic
  statement-call allowlist.
- The final self-host module split keeps each responsibility below its existing
  component cap: intent parameter/outcome environments, exact action contract,
  DIR outcome validation, MIR scalar append, MIR-lower carrier/cleanup/action
  admission, and typed C outcome emission now have named owners. The component
  contract, SoT edge (`61 authorities`, `63 derived carriers`), build-source
  inventory, MIR declaration inventory, and documentation-quality gate passed.
  SoT adequacy live owner/consumer and mutation checks passed; the Coq model was
  explicitly skipped because no `rocq`/`coqc` executable is installed.
- Grade: the bounded input-language outcome-binding feature is `REACHABLE`.
  Compiler organization `intent` remains `SURFACE` because the production
  bootstrap entrypoint does not call a compiler intent and no C-owned path was
  replaced.
- Next falsifier: the `.todo` two-action fixture must add source-declared typed
  success/failure branches, success-only completion, exact DIR predecessor
  carriage and B-failure compensation where A undo executes once, B undo zero
  times and the failure tobject payload changes the observed result. Do not infer
  these facts from source order or variant spelling.
- Preserve and do not stage the three concurrent parity edits
  (`driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`, and
  `driver_rung2_owner_field_parity_owner.sh`) or the untracked
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`.

## Current resume checkpoint - fallible action tobject outcome consumption

- Landing parent: `49c097b2dbd42f1387349b4b1d751881a7a5dd27` on `main`,
  aligned with `origin/main` at the start of this slice. After landing, verify
  the exact revision with `git rev-parse HEAD`.
- Objective card:
  - objective: preserve the production direct-MIR action's typed success or
    failure payload through world composition to bootstrap Main and make the
    final caller consume the payload;
  - priority: exact outcome variant; transaction stage/status/recovery facts;
    exact target/path receipt; action/world/caller carriage; authority and
    freshness negatives; then typed intent transition outcome binding;
  - fact owner: `artifact_transaction_owner.pgy` owns receipt/failure facts;
    `DriverRung2Execution.EmitDirectMir` owns the terminal transition;
    world/composition only carries it and Main is the last consumer;
  - forbidden fallback: `ok + stage` double tag, Bool collapse before Main,
    failure-tag-only handling, diagnostic string recovery, raw writer/retry,
    unknown status, known-but-wrong target, or receipt-derived authority,
    source freshness and topology identity;
  - verification: `driver_rung2_fallible_tobject_outcome_owner.sh`, the action
    and atomic-transaction static gates, runtime transaction matrix, object/
    action boundary contract and the SoT registry gate.
- `DriverRung2ExecutionOutcome` now has distinct executed receipt, ordinary
  rejection and artifact failure variants. The old result struct and detached
  `execution_identity` field are gone. Success checks schema, exact target and
  output path, atomic visibility and non-durability. Failure preserves exact
  stage/status/prior-final/temp-cleanup facts to Main.
- The C type declaration scheduler now consumes hosted method/action by-value
  return and parameter facts. It does not create false cycles for implicit
  self, pointer-carried mutual subject parameters, or the host's own direct
  return/parameter type.
- Observed focused evidence:
  - incremental native compiler rebuild passed;
  - native C, native LLVM and production self C execute the action outcome
    probe as `ok=7`, `error=9`;
  - mutual subject action parameters compile without a false by-value cycle,
    while a host-self `ValueTool` method returns the executed value `3`;
  - a fresh current-source bootstrap driver publishes the success artifact and
    Main distinguishes a real begin failure exactly as schema v1, begin-temp,
    status 1, prior final preserved and temp removed, with no partial output;
  - the fresh build peak observed about 808 MiB in `pgy` plus 1.03 GiB in
    `cc1`, roughly 1.8 GiB combined rather than the old multi-process 20 GiB
    symptom.
- Grade: receipt and failure reach `OUTCOME_CONSUMED`; overall `tobject`,
  subject/action, zone and world remain `REACHABLE`, not `SUBSTITUTING`.
  Compiler `intent` remains `SURFACE` because intent lowering still discards
  action results and accepts only literal-success `expect`.
- Next falsifier: add typed outcome binding and success/failure branching for
  two real production actions in an intent, execute compensation with exact
  predecessor evidence, then consider root-intent takeover.
- Preserve and do not stage the three concurrent parity edits
  (`driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`, and
  `driver_rung2_owner_field_parity_owner.sh`) or the untracked
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`.

## Current resume checkpoint - tobject publication and domain admission boundary

- Landing parent: `553af9793433798a8b7c6bdea3badc80b1d345a6` on `main`, aligned
  with `origin/main` at the start of this slice. The landing commit contains
  this handoff; use `git rev-parse HEAD` and `git status --short --branch` after
  landing for the exact revision and dirty state.
- Objective card:
  - objective: keep `object`/`tobject` as topology-materialized projection
    destinations, make caller-supplied domain admission explicit, and prevent
    declaration initializers or constructor arguments from becoming a second
    materialization authority;
  - priority: projection identity and one materialization owner; explicit
    admission roles; native/self field-kind parity; exact DIR/MIR identity;
    C/LLVM execution; then examples and tooling projections;
  - fact owners: the language-word registry owns `binding`; nominal field-kind
    vocabulary owns `binding_slot`; domain declarations own slot identity;
    `dir.domain_graph` and `semantic.domain_runtime_assignment` own topology and
    exact projection assignments; MIR is a carrier;
  - last legitimate consumers: native C/LLVM constructor and topology
    renderers, and the self source-to-MIR producer/admission path;
  - forbidden fallback: object/tobject slot initializer, projection/layer/shared
    constructor injection, object slot used ambiguously as both admission and
    projection, subject-only endpoint validation, source-name recovery, or a
    detached tobject reused as a fresh projection source;
  - verification: `tobject_boundary_execution_owner.sh`, language-word and MIR
    field-kind registries, object/action boundary ratchet, VS Code grammar gate,
    and focused C/LLVM backend execution.
- `binding slot` is the explicit object-valued zone admission surface. Zone
  constructors accept source-order `subject slot`/`binding slot` values;
  relation/effect constructors accept only their `for ...` participants.
  `object`/`tobject` projection destinations and layer/shared storage are
  materialized after construction by topology/runtime owners.
- Native semantic and self parser now reject `object slot ... = ...` and
  `tobject slot ... = ...`. Those initializers previously parsed and type
  checked but had no DIR/MIR/backend carrier, so runtime silently observed
  zero-filled storage.
- The self path carries `BindingSlot` through typed AST, declaration
  vocabulary, DIR graph census, exact topology source identity, MIR declaration
  verification and runtime projection assignment. The effect/relation header
  label is produced by one immutable classifier; this avoids the observed
  generated-C SSA carry defect that erased the default subject participant.
- Observed focused evidence before landing:
  - a clean native `make compiler` rebuild completed successfully, then
    `tobject_boundary_execution_owner.sh` passed with the rebuilt `pgy` and the
    current Pergyra-built self driver;
  - production `CompileSourceToCVerified` emits byte-equal C from the binding
    source and its admitted MIR, and that C executes `door=5`, `key=9`,
    `view=5`, exactly matching native C and LLVM;
  - valid positive field IDs remain unchanged while `door` is mutated from
    `binding_slot` to `object_slot` and `key` from `binding_slot` to
    `tobject_slot`; both mutations fail in the nominal constructor policy with
    `expected: at_most_1`, `actual: 2` before any partial C artifact;
  - `language_keyword_registry_smoke.sh`,
    `mir_decl_field_kind_vocabulary_smoke.sh`,
    `object_action_boundary_contract_smoke.sh`, and
    `vscode_language_graph_smoke.sh` all passed after regenerating their owned
    projections;
  - a fresh Pergyra-built driver from the current source repeated the complete
    binding production gate after the collection-lane cleanup;
  - self MIR for `intent_callable_execution` is 46,384 bytes and for
    `binding_slot_constructor_source_order` is 10,394 bytes; the latter carries
    exact `binding_slot` identities and projection source IDs;
  - four self negatives reject projection constructor injection, detached
    tobject source reuse and unowned projection initializers;
  - native C/LLVM preserve interleaved source-order zone admission and execute
    `alpha=7`, `beta=9`, `view=7`, `receipt=9`; the binding fixture executes
    `door=5`, `key=9`, `view=5` on both backends;
  - the keyword registry now contains 145 words, including contextual
    `binding`, with one generated self/LSP/TextMate projection.
- The hard self-host contract was realigned with current owners: the MIR
  fixture inventory belongs to `driver_rung2_mir_manifest_owner.pgy`, resource
  receiver traversal uses the expression-graph accessor, and body call-target
  resolution consumes the shared expression-environment owner. Collection
  mutations were removed from `SelfMirSimpleStatementKind`; ArraySet secondary
  graph attachment now exists exactly once in the graph-owned collection lane.
- Focused current-source indexed-assignment evidence is green: direct-source C
  and admitted-MIR C are byte-equal, runtime prints `2`, and removing the target
  graph fails with `MIR instruction expression graph is missing or invalid`.
  The broader filtered `driver_rung2_body_parity.sh` run was not green: its
  oracle canonicalization stopped earlier at `MIR machine-layer facts are
  missing or invalid`. Do not report that broader runner as executed past this
  pre-existing admission blocker.
- Grade only the bounded binding admission/runtime slice `SUBSTITUTING`:
  production self source -> admitted MIR -> general C now replaces the C-owned
  oracle for this fixture, has byte-equal direct/MIR artifacts, exact native
  C/LLVM output parity, and valid-ID negative ratchets. Do not promote the
  compiler-organization grade of `object`, `tobject`, `zone`, `world`, or
  `intent`; their independently recorded grades remain unchanged.
- This next falsifier is completed by the newer fallible action outcome
  checkpoint at the top of this handoff. Typed intent transition outcome
  binding remains open.
- The stale tracked `testall_run.txt` transcript was removed and is ignored;
  generated builders remain under `.tmp/`.
- Preserve and do not stage the three concurrent parity edits
  (`driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`, and
  `driver_rung2_owner_field_parity_owner.sh`) or the untracked
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`.

## Current resume checkpoint - intent execution and tobject boundary

- Landing parent: `57cbc9d5bd600bb37fa0c1a56d7feeb60f6993aa` on `main`.
  The landing commit contains this handoff; after landing use `git rev-parse
  HEAD` and `git status --short --branch` for the exact revision and dirty state.
- Objective card:
  - objective: carry exact DIR intent facts to typed MIR and execute the bounded
    successful `Checkout` action through the general self-host C consumer while
    keeping `tobject` a detached payload rather than a second graph authority;
  - priority: participant/action identity; lossless MIR carriage; one admitted
    execution path; projection synchronization and caller writeback; tobject
    constructor/source negatives; then fallible intent semantics;
  - fact owners: semantic action/call owners decide callable contracts; DIR owns
    intent purpose/participants/ordered step graph and `dir.domain_graph`; MIR is
    a carrier; intent lowering/emission is the final bounded C consumer;
  - forbidden fallback: intent-as-func, source/AST rescan, native MIR graft,
    action beside an old direct path, projection storage as constructor input,
    or a published tobject reused as fresh projection source;
  - verification: `intent_callable_execution_owner.sh`,
    `tobject_boundary_execution_owner.sh`, the earlier intent reachability gate,
    and the self-host component contract.
- Observed executable evidence:
  - native `make -j4 compiler` rebuilt `bin/pgy.exe`; a fresh Pergyra-built
    `driver_rung2_main.pgy` completed with 0 Pergyra errors and 0 warnings;
    generated C retained only the three known unused-variable warnings and two
    unsupported warning-option notes;
  - direct-source self C and admitted-self-MIR C are byte-equal for the exact
    successful intent slice;
  - self C, native C and native LLVM all execute `Checkout` and print
    `buyer.total=3`, `payment.total=3`, ready projections, `Mina`, and ready world;
  - intent kind, commit, participant type, zone alias, authorization and rollback
    identity mutations fail before partial C;
  - native C/LLVM preserve subject input order across interleaved object/tobject
    zone storage, while a second projection constructor argument is rejected;
  - native source, self source and a valid-ID mutated self MIR all reject a
    detached tobject as projection source.
  - `self_hosted_component_contract_smoke.sh`, object/action boundary,
    documentation quality, build-source inventory, MIR declaration inventory,
    domain runtime topology, SoT edge, and single-owner gates passed. The
    registry reports 61 authorities, 62 derived carriers,
    `CLOSED=34 BRIDGE=27 ACTIVE=0`;
  - Coq/Rocq is unavailable, so `sot_authority_adequacy_smoke.sh` ran with the
    explicit `PGY_ALLOW_MISSING_COQ=1` declared skip. Live owner/consumer binding
    and negative mutations passed; proof compilation is not claimed.
- Grade: the bounded successful input-language intent path is `REACHABLE`, not
  whole-intent `SUBSTITUTING`. Fallible `expect`, compensation/effect outcome and
  `PgyCompilerWorld` root intent takeover remain open. Compiler-organization
  intent remains `SURFACE` because the real bootstrap entrypoint does not call
  a production root intent with a real purpose/fact bundle.
- Next executable falsifier: carry an actual fallible `expect` result, branch to
  explicit failure/compensation, observe the effect/outcome, and reject missing
  predecessor/rollback evidence. The root intent may replace the direct
  bootstrap bypass only after a real compiler purpose and its elaborated fact
  bundle reach that production path; action count is not the criterion.
- `tobject` owns only immutable materialized payload. Source identity, freshness,
  edge and authority remain with the enclosing directive plus
  `dir.domain_graph`; zone constructors accept only subject/binding inputs.
- Preserve and do not stage the three concurrent parity edits
  (`driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`, and
  `driver_rung2_owner_field_parity_owner.sh`) or the untracked
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`.

## Current resume checkpoint - explicit projection-map substitution

- The landing parent is `880a83c348021f3e126176a2f71ff0ad872e8223` on
  `main`, aligned with `origin/main` at the start of this slice. The landing
  commit contains this handoff; use `git rev-parse HEAD` after landing for the
  exact revision.
- Objective card:
  - objective: preserve `map { target <- source }` as a typed child of its
    refresh/publish directive, resolve it once to exact target/source
    declaration-field identity and semantic assignability, and execute that
    fact through production self C and native C/LLVM;
  - priority: syntax parent/entry identity, exact path and type verdict,
    canonical identity-epoch preservation, no explicit-to-implicit fold,
    direct-source execution, negative ratchet, then patch size;
  - fact owner: parser owns map spelling and parent structure;
    `semantic.domain_runtime_assignment` owns the resolved assignment. Self
    semantic `SemanticDomainProjectionTypeAssignable` owns compatibility;
    DIR/HIR/MIR are carriers and `MirDomainRuntimePlan` is a one-time admission
    receipt;
  - last legitimate consumers: native C/LLVM domain runtime renderers and the
    general self C method-prologue view reached by production
    `CompileSourceToCVerified`;
  - forbidden fallback: explicit map folded to implicit same-name, target/source
    string equality as type policy, backend source/AST rewalk, missing-source
    zero fill, native MIR graft, map-child omission followed by node-ID offset
    repair, or fixture-specific output;
  - verification gate:
    `tests/self_hosted/parity/domain_runtime_explicit_map_execution_owner.sh`,
    invoked by `domain_runtime_assignment_execution_owner.sh`, plus component,
    object/action, source/MIR inventory and SoT gates.
- Parser emits a typed `ProjectionMap:` child for effect/relation/zone
  refresh/publish directives. DIR binds each entry to its exact directive and
  rejects duplicate targets. The self runtime producer resolves the selected
  source path and calls the semantic assignability owner; the fixture uses
  `Int -> Long` deliberately, so byte-equal type strings cannot satisfy it.
- The canonical MIR identity epoch reconstructs admitted explicit-map children
  before dependent callable IDs are issued. It does not compare raw native and
  self producer IDs. General self C emits the exact assignments
  `life <- hp` and `label <- name` from admitted runtime rows.
- Observed executable evidence on this source tree:
  - a fresh DRV-2 self compiler build completed with 0 Pergyra errors and 0
    warnings; GCC emitted only the pre-existing unused-variable and unsupported
    warning-option notes;
  - native C, native LLVM and production self C for
    `zone_layer_projection_explicit_map_runtime` all printed `7` and `dst`;
  - production direct-source self C and explicit self-MIR C were byte-equal;
  - no-map, type mismatch, missing source and duplicate target variants all
    failed before an artifact;
  - the explicit gate, the combined implicit/explicit runtime gate, the full
    self component contract, object/action contract, build/MIR inventory,
    144-row keyword registry, targeted backend comparison, documentation
    quality and SoT edge/adequacy live-binding checks passed;
  - the SoT edge audit found and closed a pre-existing registry/Coq projection
    omission for `SFDomainRuntimeAssignment`. The final projection reports
    `59 authorities, 60 derived fact carriers; CLOSED=34 BRIDGE=25 ACTIVE=0`;
  - `make -j2 all` reported no pending native/LSP work. Coq proof compilation
    was a declared skip because neither `rocq` nor `coqc` is installed; live
    owner/consumer and mutation checks still ran and passed.
- Grade the explicit effect/relation eager method-entry map path
  `SUBSTITUTING`: it replaces a real source -> self MIR -> admitted plan -> C
  execution path and is checked against both native backends. Keep the whole
  `semantic.domain_runtime_assignment` family `BRIDGE`: self still produces
  the resolved semantic family at the MIR boundary, declaration-level source
  IDs, pool/materialization, dirty/epoch/detach/unlink/state scheduling and one
  shared native/self runtime plan remain open.
- The reusable `tobject -> object -> vessel -> subject -> action` rule is not a
  nominal promotion ladder. It is a set of orthogonal protocols: detached
  transfer, local observation, stable owned state, authority-bearing identity,
  and observable transition. At every boundary use the same closure pattern:
  semantic identity -> typed fact -> lossless carrier -> one admission receipt
  -> last production consumer -> negative ratchet. `effect`/`relation` bind
  exact destination roles and projection members; `zone` owns resource/lifetime
  frontier; `action` owns the observable transition; `intent` closes a real
  purpose and attributes its elaborated cross-axis facts.
- The next executable falsifier is
  `world_zone_projection_visibility`. Its renamed maps (`label <- displayName`,
  `user <- displayName`) must first become reachable through the self semantic
  artifact/world path; the current first blocker is the semantic initializer
  artifact for that world/intent source, not projection codegen. The current
  driver reports `ast_artifact_invalid`, `node_count: 96`, owner
  `SemanticAstInitializerTypeFacts`. Do not bypass it with native MIR or a
  fixture-specific reduced program.
- Preserve and do not stage the three concurrent parity edits (indexed
  assignment `1/0`, match `2/2`, owner-field `3/3`) and the untracked
  `docs/self_hosted/18_c_oracle_bootstrap_contract.md`. They are not part of
  this slice.

## Prior checkpoint - callable receiver carriage substitution

- The final parent for this executable slice is `6837a34d` on `main`. During
  the slice, the concurrent language-word task landed `206e0697`, `8d4c34d4`,
  and `6837a34d` above the original `ca01b7c0` base; this receiver commit
  preserves those commits and updates the SoT keyword evidence from 145 to 144 rows. The
  landing commit contains this handoff; after landing, use `git rev-parse HEAD`
  for the exact revision.
- Objective card:
  - objective: replace self general C's by-value identity receiver path with one
    callable-owned `none | value | mutable-identity` fact carried from native or
    self MIR through exact machine admission into signature and call emission;
  - priority: callable identity, exact declaration join, mandatory wire fact,
    semantic place/addressability fact, output-before-failure negatives, then
    patch size;
  - fact owner: `semantic.callable_receiver_carriage`, with the current self
    policy in `callable_receiver_carriage_policy_owner.pgy`; MIR rows and
    codegen views are projections, not second semantic owners;
  - last legitimate consumer: general self C function/prototype emission and
    member-call receiver argument emission;
  - forbidden fallback: missing or unknown carriage success, owner-name-only
    join, mutable identity by value, `Leaf || MemberAccess` addressability
    reconstruction, address-of-temporary, role/non-role guessing, or use of the
    callable fact as a general parameter ABI decision;
  - verification gate: receiver admission native/self parity plus production
    `zone_layer_projection_runtime` hard emission, exact canonical IDs, pointer
    signature/address call, carried-value mutation, temporary receiver negative,
    generated-C syntax, routine-index C/LLVM regression, and component caps.
- Native and self MIR now emit mandatory routine `receiver_carriage` rows.
  Admission binds each row to a positive unique routine `source_syntax_id` and,
  for methods, one exact declaration owner. The current wire values are `none`,
  `value`, and `mutable-identity`.
- The production canonical identities observed by the focused gate are
  `27 | method | BattleZone | Show | mutable-identity` and
  `35 | function | Main | none`. General self C emits
  `BattleZone_Show(BattleZone *self)` and calls it as
  `BattleZone_Show(&(battle))`; a `value` mutation is rejected before C output.
- Stable-address eligibility must come from the semantic expression place fact.
  Node-kind reconstruction such as `Leaf || MemberAccess` is forbidden because
  `factory().field` is a member node but not stable storage. The focused gate
  carries an executable temporary-receiver negative.
- Role-erased local ABI preserves a concrete mutable target as `T *self` behind
  `void *_pgy_raw_self`, and its direct-call projection requires a stable
  address. This is a local owner closure only: native semantic currently rejects
  a direct `Player.TakeDamage` lookup, while the observed native/self role
  method source IDs are `13` and `6`. Do not count the synthetic/local role gate
  as production reachability or substitution. A role body `return self.health`
  also fails closed at `statement_type_unresolved`, so close call-target
  resolution, the canonical role callable identity epoch, and role-body field
  type facts first.
- This is `SUBSTITUTING` progress for the self MIR -> general C receiver path.
  The registry remains `BRIDGE` because native C/LLVM and general parameter ABI
  still reuse the broader `uses_pointer_self` compatibility policy.
- The routine-index regression encountered during integration was a missing
  mandatory `reachable` fact in positive fixtures. The fixtures now state it;
  validation was not weakened. The C/LLVM routine-index smoke and the full
  self-host component contract are green, with the main index owner at its
  600-line cap.
- Last observed focused evidence on the final source tree:
  - fresh `driver_rung2_main.pgy` C build: 0 self-host errors and 0 warnings;
  - hard `zone_layer_projection_runtime` receiver gate: one MIR fixture PASS,
    including carriage mutation, semantic-place temporary receiver rejection,
    role owner positive/three negatives, ordinary self-codegen role definition,
    and generated-C GCC syntax;
  - MIR receiver admission: native/self value and mutable rows plus ten
    fail-closed mutations PASS;
  - routine-index C/LLVM smoke and self-host component contract PASS;
  - SoT authority live owner/consumer and negative gate PASS; the Coq compile
    was explicitly declared skipped because neither `rocq` nor `coqc` is
    installed on this runner.
- The protected concurrent user changes remain unstaged and must preserve these
  numstats exactly: indexed-assignment `1/0`, match `2/2`, owner-field `3/3`.
  They are not part of this executable commit.
- The concurrent language-word registry task is now committed in the three parent
  revisions above. Its separate untracked oracle-bootstrap document is not part
  of this receiver commit. The receiver registry gate and component contract
  passed from an isolated 54-path staged-snapshot worktree: the registry
  observed 58 authorities, 57 derived fact carriers, `CLOSED=34`, and
  `BRIDGE=24`; the protected parity edits were absent from that snapshot.
- The next falsifying runtime fixture remains `zone_layer_projection_runtime`
  output `7` and `dst`. Receiver identity is no longer its blocker. The active
  next seam is exact projection member assignment plus effect bearer/relation
  source-target destination roles, followed by layer materialization and
  refresh/publish synchronization in one admitted runtime plan.

## Prior checkpoint - domain runtime assignment boundary audit

- Exact checkout at the start of this supporting slice is
  `e8440ac3cf1bcdb5469a8dff75041bc416078714` on `main`, aligned with
  `origin/main`. The landing commit contains this handoff; after landing, use
  `git rev-parse HEAD` for the exact revision.
- Objective card:
  - objective: fix the repeated runtime boundary protocol from `tobject` through
    `object`, `vessel`, `subject`, `action`, `effect`, `relation`, and `zone`,
    then define the first lossless runtime-assignment carrier without creating
    another backend or AST-text authority;
  - priority: exact source/destination identity, owner-specific fact lifetime,
    callable receiver carriage, explicit lifecycle/materialization, one
    admitted target-neutral plan, then patch size;
  - fact owners: semantic callable ABI for `CallableReceiverCarriage`, domain
    semantic/DIR for participant roles and projection member assignments, domain
    runtime owner for lifecycle/materialization operations, MIR as lossless
    carrier, and `VerifiedDomainRuntimePlan` only as an admission receipt;
  - last legitimate consumers: general self-host C codegen view and direct C/LLVM
    target renderers after machine admission has built and fully validated the
    plan once;
  - forbidden fallback: backend same-name member search, missing-source
    `.field = 0`, first/0/1 bindable destination selection, `by participant` as
    destination role, aggregate zero as successful layer materialization,
    by-value zone identity, backend lifecycle AST rewalk, or old-epoch plan
    reuse;
  - verification gate: explicit/implicit map carrier parity; wrong valid member
    ID/type, bearer role and relation destination mutations; missing receiver,
    materialization and sync operations; structural fallback ratchet; final
    self MIR -> C and shared-plan C/LLVM execution output `7` and `dst`.
- Boundary judgment is now fixed in
  `docs/200_object_to_action_boundary_patterns.md`. These constructs are not a
  promotion ladder: tobject owns detached transfer, object local observation,
  vessel subject-owned state, subject stable identity, and action observable
  transition. Effect/relation/zone repeat the same identity-carriage-binding-
  operation-outcome protocol at the domain frontier.
- The native fact-lifetime audit found five distinct missing families:
  `DomainParticipantRoleFact`, `DomainProjectionMemberAssignment`,
  `DomainLifecycleOperation`, `DomainLayerMaterialization`, and
  `CallableReceiverCarriage`. A future `domain_runtime_assignments` namespace
  may carry them together, but one nullable mega-row or a DIR-owned receiver
  decision is forbidden.
- Projection mapping is currently lossy. Native semantic resolves explicit and
  implicit same-name paths only locally; native MIR retains explicit names in
  memory but does not serialize them. C and LLVM therefore re-decide mapping,
  and C hides a missing path with zero while LLVM fails. Implicit same-name
  remains a sound Pergyra default only when semantic resolves it once into exact
  field ID/type/path rows.
- Effect bearer and relation source/target destinations do not have role facts;
  native C/LLVM use first and 0/1 ordinal bindable slots. Receiver carriage is
  also lost from the MIR JSON wire, and self general C can emit
  `BattleZone_Show(BattleZone self)`. Zone layer storage can remain aggregate
  zero without a proved materialize/bind/sync operation.
- A trial parser/DIR-only `ProjectionMap:` patch was deliberately discarded.
  It inserted public compact-tree rows that change native parity/source IDs and
  still disappear at the MIR JSON consumer. The coherent implementation slice
  must start at semantic ownership and preserve all dependent identities through
  canonicalization; no source reparse or public-tree side channel is allowed.
- The next implementation order is: add lossless explicit/implicit member and
  participant-role facts; add callable-specific receiver carriage; serialize
  lifecycle/materialization operations; remap declaration/member/directive IDs
  atomically; index `domain_runtime_assignments` once in `MirDocumentFactIndex`;
  build and fully validate one runtime plan immediately after topology admission;
  pass it into the general C codegen view and direct C/LLVM renderers.
- No compiler source was landed in this audit slice. The observed evidence is
  source/read-path inspection plus document contract gates and `git diff
  --check`; the prior executable `e8440ac3` remains the last substitution
  checkpoint. This is supporting commit one after that executable checkpoint.
- The next executable falsifier remains self MIR -> C `7`/`dst`. A parser-only
  map row, plan comment/digest, zero-filled `.poison/.trust`, or by-value zone
  receiver cannot satisfy it. The three protected concurrent user parity files
  remain unstaged and unchanged by this slice.

## Current resume checkpoint - distinct apply topology and boundary contract

- Checkout base before this executable slice is
  `820e1ec32960a78ed73b37bd4f4046f0ba6270a9` on `main`, aligned with
  `origin/main`. The landing commit contains this handoff; after landing, use
  `git rev-parse HEAD` for the exact executable revision.
- Objective card:
  - objective: preserve `apply` as a distinct self-host-produced lifecycle fact
    through DIR/MIR/canonical admission while fixing the canonical
    `object/tobject -> vessel -> subject/action -> effect/relation/zone/intent`
    authoring boundary and naming the exact runtime facts still missing;
  - priority: distinct source identity, exact field-kind join, atomic canonical
    epoch, no-edge graph admission, honest dogfood grade, then patch size;
  - fact owner: native/self DIR domain-topology rows for directive identity,
    declaration-field identity for slot kind, MIR as the lossless carrier, and
    `MirDomainTopologyGraphPlan` only for the target-neutral dependency plan;
  - last legitimate consumer: machine admission and the one C/LLVM graph-plan
    attachment immediately before target projection;
  - forbidden fallback: folding apply into maintain, deriving runtime lifecycle
    from graph adjacency, source/AST reparse, same-name projection-member join,
    0/1 ordinal destination binding, `by participant` as effect bearer,
    zero-filled layer storage as success, or by-value zone receiver identity;
  - gates: native topology smoke, focused self producer, canonical identity
    epoch, one-plan C/LLVM consumer, component/grammar/keyword/object-action/SoT
    contracts, shell syntax, and `git diff --check`.
- `zone_layer_projection_runtime` now produces four exact rows from the
  production self source path: `Poisoned.refresh`, `TrustedLink.publish`,
  `BattleZone.apply-effect`, and `BattleZone.link-relation`. `apply-effect`
  exact-joins the `poison` effect slot and `player` subject slot and remains
  distinct from `maintain-effect`; native/self MIR agree on the four-kind
  sequence. Stale IDs, a valid relation-slot ID substituted for `poison`, a
  relation used as the apply layer or target, and a non-subject participant all
  fail closed.
- Native `apply stateAlias` is normalized by semantic ownership into the exact
  effect/target slots before DIR collection, and a focused fixture proves that
  it emits the same typed row. DIR no longer drops an unresolved apply or
  reduces the expected row count. The production self source parser still
  fail-closes this shorthand; typed state-declaration/alias carriage is an open
  parser/DIR parity seam, not a name-lookup fallback opportunity.
- Apply is a one-shot lifecycle/materialization transition, not a persistent
  recomputation dependency. Native and self graph builders therefore admit the
  exact kind while adding no edge. The BattleZone graph remains exactly
  `nodes=3 edges=2 depth=2 pass_limit=2`; maintain continues to own the
  layer-to-target dependency edge. Unknown kinds are not ignored.
- Evidence grades remain deliberately split: the four-row self source ->
  DIR/MIR producer is a narrow `SUBSTITUTING` C-owner replacement; the admitted
  target-neutral graph plan is `REACHABLE`; direct-MIR world/zone/subject/action
  is `REACHABLE`; object/effect/relation/vessel/intent bootstrap declarations do
  not become runtime dogfood merely from syntax or import reachability. The
  layer materialization/projection-sync runtime remains `BRIDGE` and RED.
- The canonical boundary pattern is now fixed in
  `docs/200_object_to_action_boundary_patterns.md`: `object` is a same-process
  refreshable read projection, `tobject` a detached immutable transfer value,
  `vessel` subject-owned passive state, `subject` the identity-bearing authority
  host, and `action` its observable transition. Effect is a temporal layer,
  relation an identity edge, zone the membership/lifetime/frontier owner, and
  intent is used only when multiple production actions share a real
  success/failure/compensation purpose. These are orthogonal boundary protocols,
  not a nominal promotion ladder and not a keyword-density target.
- Exact runtime blocker: the wire still lacks projection member paths and field
  types (`view.hp <- bearer.hp`, `packet.name <- target.name`), effect-bearer and
  relation source/target destination roles, receiver carriage, layer
  materialization/state/synchronization, and refresh/publish value operations.
  The self parser currently skips projection `map { ... }` bodies, so the next
  owner must first preserve them as typed facts. A separate DIR-owned
  `domain_runtime_assignments` family should carry exact directive/owner/slot/
  path IDs and types into one target-neutral runtime plan. C/LLVM only render
  admitted operations.
- The next executable falsifier remains self MIR -> C output `7`/`dst`, but only
  after those exact facts exist. Changing one member ID/type or relation
  destination role must reject the artifact; `.poison`/`.trust` zero storage or
  a by-value zone receiver must never be accepted as the target result.
- Last observed focused evidence is green: native build and topology C/LLVM
  smoke; native/self exact four-row MIR; focused self producer hard gate;
  canonical stale/wrong-kind negatives; one target-neutral C/LLVM plan with the
  unchanged 3/2 graph; component, language-word registry, grammar,
  object/action and single-Gate-SoT contracts; shell syntax and `git diff
  --check`. The previous fresh pressure build remains the current broad memory
  evidence at peak working set 1,038.0 MiB/private 1,132.4 MiB under 3,072 MiB;
  the 35-minute pressure build was not repeated for this bounded row/admission
  change.
- This is an executable producer replacement slice, so the supporting-only
  commit counter resets here. The only protected concurrent user files remain
  the three unstaged parity owners for indexed assignment, match, and owner
  field. Do not stage or edit them.

## Previous resume checkpoint - non-empty topology producer and one graph plan

- Checkout base before this executable slice is
  `09e00d29a82584e912534ed1e4cb8eefafe23ab0` on `main`, aligned with
  `origin/main`. The landing commit contains this handoff; after landing, use
  `git rev-parse HEAD` for the exact executable revision.
- Objective card:
  - objective: replace the first non-empty native C topology producer decision
    with self-host typed DIR/MIR facts, then let one ID-keyed target-neutral plan
    reach both production backends;
  - priority: exact producer identity, atomic canonical epoch, one admitted
    plan, bounded backend receipts, runtime blocker honesty, then patch size;
  - fact owner: `SelfDirDomainTopologyRows` and `SelfMirDomainTopologyFacts`
    for source production, `MirDomainTopologyGraphPlan` for the admitted plan;
  - last legitimate consumer: self-host C/LLVM plan attachment immediately
    before target projection;
  - forbidden fallback: native topology graft, source/provenance reparse,
    name-only or offset identity repair, non-empty-to-empty downgrade,
    backend plan rebuild, repeated whole-plan readiness, generic zero-fill of
    layer storage, or plan trace claimed as runtime execution;
  - gates: focused non-empty producer, canonical identity epoch, one-plan
    C/LLVM consumer, component contract, object/action boundary contract, and
    the unchanged 3 GiB pressure owner.
- Production DRV-2 now produces `zone_layer_projection_runtime` topology from
  self source through typed AST/DIR/MIR. Exact identity is
  `domain_graph_id=14937235025281185444` with three rows:
  `Poisoned.refresh(bearer -> view)`,
  `TrustedLink.publish(target -> packet)`, and
  `BattleZone.link-relation(player, enemy -> trust)`. This bounded non-empty
  producer is `SUBSTITUTING`; it replaces the native C-owned producer decision.
- Canonical reconstruction issues nominal owner, directive, and declaration
  field identities in one epoch. Restoring a stale raw ID or pairing the
  canonical `player` name with the canonical `enemy` ID fails. Numeric equality
  across native/self epochs, ordinal repair, and name-only joins are absent.
- Machine admission creates and fully validates exactly one
  `MirDomainTopologyGraphPlan`. Production C/LLVM consumers check only its
  bounded graph/digest/cardinality receipt. The exact BattleZone plan is
  `nodes=3 edges=2 depth=2 pass_limit=2` with `trust <- player` and
  `trust <- enemy`; forged edges and a gate-only digest mutation fail closed.
  This plan path is `REACHABLE`, not yet runtime `SUBSTITUTING`.
- Zone constructor policy now distinguishes caller-supplied
  subject/object/tobject/binding slots from effect/relation layer storage.
  Caller arity remains two for BattleZone; the layer fields remain in layout
  and must be materialized by the topology/runtime owner.
- Exact runtime blocker: `apply poison to player` is identity-checked in DIR
  but is not carried as a MIR topology row; no runtime owner materializes
  `.poison`/`.trust` or executes refresh/publish value synchronization.
  Therefore self MIR -> general C output `7`/`dst` remains RED and is the next
  executable falsifier. A zero-filled layer field must be rejected rather than
  accepted as a successful runtime result.
- Last observed focused gates are green: non-empty producer hard DRV-2,
  canonical identity epoch, one target-neutral C/LLVM plan, component contract,
  language-word registry, object/action boundary contract, and `git diff
  --check`. The fresh pressure-owned self-host compiler build also installed
  `bin/pgy-self-driver.exe` and passed its smoke in 2,138,300 ms, with peak
  working set 1,038.0 MiB and private memory 1,132.4 MiB under the unchanged
  3,072 MiB cap. The one-plan gate compiles and runs the C and LLVM
  artifacts with `Hello, Pergyra!`; it intentionally does not claim the open
  zone runtime result. After that pressure run, the focused plan rebuild also
  proved the final `own` graph-schedule transfer and absent-plan residual-array
  rejection. The 35-minute full pressure build was not repeated for those two
  bounded owner/gate changes.
- Hard self-host guard accounting resets here: this is the executable
  replacement required after the two supporting commits. Documentation,
  registries, tests, and the plan are supporting evidence around the actual
  non-empty self producer substitution.
- The only protected concurrent user files remain the three unstaged parity
  owners for indexed assignment, match, and owner field. Do not stage or edit
  them.

## Current resume checkpoint - declaration field exact identity

- Checkout base before this supporting slice is
  `d6fb4a61328394329fbf71ca736bedfb70a305ae` on `main`, aligned with
  `origin/main`. The landing commit contains this handoff; use
  `git rev-parse HEAD` for its exact revision. The last executable
  substitution checkpoint remains `0ac210261d199f5b188fbc66d5dfdcbdec4c223d`.
- Objective card:
  - objective: make every domain-topology field reference prove the exact
    declaration field `(owner, name, source_syntax_id, field_kind)` before a
    backend can consume it;
  - priority: field identity carriage, one declaration index, forged valid-ID
    rejection, producer-local identity honesty, then patch size;
  - fact owner: native `MIRDeclField` and self-host
    `MirProgramDeclarationFieldIdentityIndex`, scoped to one MIR revision;
  - last legitimate consumer: native MIR topology validation and self-host
    `MirDomainTopologyFacts` admission;
  - forbidden fallback: name-only lookup, declaration rescans per edge,
    native/self raw-ID equality, numeric offset repair, AST-text reparse, or
    non-empty canonicalization with stale topology IDs;
  - gates: MIR unit mutation, `domain_runtime_topology_smoke.sh`,
    `domain_topology_admission_owner.sh`, and focused hard DRV-2
    `function_clause_order_minimal`.
- Native MIR declaration fields capture their parser-assigned stable ID and
  serialize it in `pgy.mir.v1`. Validation rejects missing/global-duplicate
  field IDs and exact-joins topology fields by name, ID, and expected semantic
  kind. The unit mutation with `player` name + valid `enemy` ID fails; the
  same name/ID with subject kind changed to object kind also fails.
- The self-host semantic/MIR producer carries field IDs through constructor
  facts, declaration rows, validation, and JSON projection. `mir_lower` builds
  one declaration index with one flattened field-identity child index, then
  topology admission consumes it without reopening `declarations[]` for every
  row. Missing/zero/duplicate IDs, duplicate owner/name, wrong kind, and the
  forged `player`/`enemy` join fail closed.
- Raw native and self ID numbers are not compared. Native preorder identity
  and the current self-host compact typed-arena identity are different
  producer/revision epochs. A future lossless self parser identity graph must
  close that convergence; constant offsets and provenance-string parsing are
  forbidden. MIR-to-AST canonicalization must regenerate declaration IDs and
  every dependent topology ID atomically. Non-empty topology remains rejected
  until that remap exists.
- Evidence status is `REACHABLE` supporting, not a new `SUBSTITUTING` slice.
  The last observed focused hard DRV-2 gate passed one producer-first MIR
  fixture through self MIR, canonical reconstruction, emitted-C compile and
  execution. Native MIR 155/0, native C/LLVM topology, self topology admission,
  component contract, object-to-action contract, and six focused MIR-JSON
  declaration fixtures were also observed green during this slice.
- The dedicated `generic_default_contracts` gate rebuilt its driver but is RED
  before MIR emission at the existing bounded self DIR error
  `relation/party/world/event production is not implemented` because the
  fixture declares `StorageParty`. Its field JSON expectation was updated and
  shell/component checks are green, but this gate is not claimed green. Do not
  weaken the self DIR fail-close merely to exercise the downstream assertion.
- Hard self-host guard accounting: the documentation refresh `d6fb4a61` was
  supporting commit one after executable `0ac21026`; this exact-identity slice
  is supporting commit two. The next commit must land executable replacement
  evidence, not another SoT-only cleanup.
- Next executable falsifier: produce non-empty
  `zone_layer_projection_runtime` topology in self-host, canonicalize it into
  a new identity epoch by remapping declaration/topology IDs together, and
  feed one ID-keyed graph plan to the production C/LLVM path. Replacing one
  canonical topology ID with the old raw native ID and pairing `player` with
  canonical `enemy` ID must both fail. Owner declaration ID, vessel-slot
  carriage, apply/state/layout/sync facts remain open.
- The only protected concurrent user files remain the three unstaged parity
  owners for indexed assignment, match, and owner field. Do not stage or edit
  them.

## Previous resume checkpoint - self-host empty DIR graph substitution

- Exact executable checkpoint is
  `0ac210261d199f5b188fbc66d5dfdcbdec4c223d` on `main`, pushed to
  `origin/main`. The worktree is dirty only in the three protected concurrent
  user parity owners named below; none belongs to this checkpoint.
- Objective card:
  - objective: make the production self-host MIR producer replace the native
    C DIR census/anchor for the first proved-empty topology document;
  - priority: exact graph identity, typed authority, non-empty fail-close,
    one-shot admission, then patch size;
  - fact owner: `SelfDirDomainGraphFacts`, projected once into
    `SelfMirDomainTopologyFacts`;
  - last legitimate consumer: the DRV-2 MIR writer and admitted MIR consumer;
  - forbidden fallback: declaration-count ID, constant graph ID, native-oracle
    grafting, provenance-text directive recovery, or non-empty-to-empty
    downgrade;
  - gate: focused `function_clause_order_minimal` hard producer/consumer parity
    plus `domain_topology_nonempty_rejected`.
- Production self-host source-to-MIR now classifies zone `Authority` and nine
  distinct domain directive kinds in the typed arena. The bounded DIR owner
  joins declarations, role/ability completion, effect/zone slots and ordered
  authority abilities. For `function_clause_order_minimal` it independently
  reproduces the native census `nodes=9, edges=16` and exact uint64 decimal
  anchor `14937235029576152731`, then emits `domain_topology.rows=[]`.
- The same self-produced MIR passes admission, canonical native/self parity,
  emitted-C compilation and execution; the observed program output is
  `clause-order-minimal`. This bounded empty-topology producer is
  `SUBSTITUTING`: its production source path no longer needs native C DIR to
  create the graph identity. It does **not** make non-empty topology, the graph
  plan/runtime consumer, or the whole `dir.domain_graph` family `CLOSED`.
- `Refresh`, `Publish`, projection `Bind`, `Maintain`, `Link`, `Apply`,
  `Detach`, `Unlink`, and `State` retain distinct typed identities. The current
  bounded producer rejects every one rather than claiming an empty row set.
  The committed negative `apply layer to actor` fixture fails at the self DIR
  owner and emits no MIR document.
- MIR canonicalization now reads one `MirMachineLayerAdmittedJsonInput` and
  carries its already-admitted empty topology into the reconstructed MIR. It
  does not run a second document/graph admission, and it does not recompute a
  graph from the lossy MIR-to-AST declaration projection (which omits zone
  authority today).
- Last observed green gate:
  `PGY_SELFHOST_DRIVER_MIR_FIXTURE_FILTER=function_clause_order_minimal` with
  the Pergyra-built hard driver, reporting one producer-first source/MIR
  parity fixture. A direct emitted-C compile/run also printed
  `clause-order-minimal`. The broad `test-transpile` RED remains the independent
  expression `identifier -> same name` null/`strcmp` failure from the previous
  checkpoint and was not rerun here.
- The only protected concurrent user files remain the three unstaged parity
  owners for indexed assignment, match, and owner field. Do not stage or edit
  them.
- Next falsifier is still the declaration-field exact join: a topology row
  with name `player` and the valid `enemy` field ID must fail. Then add typed
  non-empty directive rows and the ID-keyed target-neutral graph/runtime plan
  for `zone_layer_projection_runtime`. Owner declaration stable identity,
  apply/state/layout/sync facts remain open; no AST/source compatibility path
  is permitted.

## Previous resume checkpoint - MIR JSON topology admission (superseded)

- Checkout base before this supporting slice is
  `da26dc09d0ad5c04ee94b122bb23e18f6073a611` on `main`. The last hard
  substitution checkpoint remains
  `c66e22ca6dd34b50ff2a7a3a8e183852943d3a9a`: native C/LLVM zone frontier
  topology consumes MIR instead of the deleted AST graph entrypoints. After
  landing this slice, use `git rev-parse HEAD` for its exact revision without
  relabeling it as a substitution boundary.
- The only protected concurrent user files are
  `driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`, and
  `driver_rung2_owner_field_parity_owner.sh`. They must remain unstaged and
  outside this checkpoint commit.
- Native `pgy.mir.v1` now carries `relation` declarations plus optional
  `domain_topology: { domain_graph_id, rows }`. Domain rows carry graph,
  owner, directive, participant/layer/endpoint slot identity values. A
  non-domain scalar document retains its exact five-field root shape.
- Self-host `mir_lower` indexes the topology object once and admits it as typed
  `MirDomainTopologyFacts`. Missing topology for a domain declaration, unknown
  kind, duplicate directive identity, damaged null/name-ID pairs, owner or
  field-kind mismatch, and invalid relation cardinality fail before backend
  emission. No source/AST recovery or compatibility read exists. This is not
  yet an exact name-to-ID proof: declaration JSON fields do not carry their
  `source_syntax_id`, so a valid field name paired with another field's valid ID
  cannot currently be rejected by a declaration-field identity join.
- Relation identity is also connected through the self-host declaration,
  typed-AST and semantic-constructor projections, so `TrustedLink` reconstructs
  as a relation with two subject slots and one tobject slot.
- Evidence status for the JSON/admission delta is `REACHABLE`, not
  `SUBSTITUTING`. The native C/LLVM frontier remains `SUBSTITUTING`, while the
  whole `dir.domain_graph` family remains `BRIDGE`. Admission and canonical
  reconstruction do not yet execute a Pergyra-owned graph plan.
- Last observed green gates: MIR 155/0; native
  `domain_runtime_topology_smoke.sh`; self-host
  `domain_topology_admission_owner.sh`; object/action boundary contract;
  `mir_lower` source compile; positive verify-input and relation reconstruction.
  The broad `test-transpile` remains independently RED before domain tests at
  expression `identifier -> same name`, where a null emission reaches `strcmp`.
- Focused DRV-2 `function_clause_order_minimal` producer parity now reaches the
  new fail-closed boundary and is RED because the self-host MIR producer does
  not own or emit a proved empty/domain topology fact. Do not weaken admission
  or graft the native oracle row onto self output. This exact producer gap is
  part of the next executable rung.
- Hard-guard accounting: this is the second consecutive supporting/SoT-only
  checkpoint after `c66e22ca`. The next commit must land executable replacement
  evidence; do not insert another documentation, registry, or admission-only
  commit.
- Exact `BLOCKED` record for the next rung: missing facts are declaration-field
  name/`source_syntax_id` identity join, self-host producer-owned typed topology,
  the target-neutral `MirDomainTopologyGraphPlan`, and the fixture's apply,
  state-count, hidden-layout and sync-operation facts. `dir.domain_graph` owns
  topology identity; its last legitimate consumers are the plan and self-host
  C/LLVM emitters. AST/source recovery, native-oracle grafting, and
  count-floor-only success are forbidden fallbacks.
- A bounded experiment to carry the missing topology through the current
  semantic graph was rejected and reverted: the `own` variant compiled, then
  the diagnostic executable panicked out of bounds while satisfying
  `graph_shape`. The current constructor path also conflates storage-field and
  exposed-parameter counts. No experimental source change or inferred topology
  fact remains in the tree.
- Next executable falsifiers: first, a forged row with name `player` and the
  valid `enemy` field ID must fail admission. Then `zone_layer_projection_runtime`
  must make the general DRV-2 C production path consume exact
  `nodes=3 edges=2 depth=2 pass_limit=2`, `trust <- player`, `trust <- enemy`,
  and the exact mutation/state/layout/sync result from one typed plan.
- Resume with `docs/200_object_to_action_boundary_patterns.md` sections 2.1 and
  4.2.1, the `dir.domain_graph` registry row,
  `tests/domain_runtime_topology_smoke.sh`, and
  `tests/self_hosted/parity/domain_topology_admission_owner.sh`.

## Previous compiler world/action boundary checkpoint (superseded)

The following section records the immediately preceding checkpoint. It is
historical context; the current executable state is the section above.

- Exact implementation checkpoint:
  `90ed9f82ae1b3af966739f2e324a989ccc3f4863` on `main`, with parent
  `806d2eb1a861b50f4edd5c9302a1cb33a1f9b5a0`. This handoff refresh follows as
  a documentation-only commit; use `git rev-parse HEAD` for the checkout
  revision while retaining `90ed9f82` as the executable boundary.
- Dirty state at the implementation checkpoint contains only three protected
  concurrent user files, none staged or included in `90ed9f82`:
  `driver_rung2_indexed_assignment_parity_owner.sh`,
  `driver_rung2_match_parity_owner.sh`, and
  `driver_rung2_owner_field_parity_owner.sh`.
- Active executable rung: `REACHABLE`, not `SUBSTITUTING`. Production direct
  MIR now follows exactly one graph:
  `driver_bootstrap_main.Main -> EmitDirectMirThroughPgyCompilerWorld ->
  PgyCompilerWorld.EmitDirectMir -> PgyCompilerWorld.direct_mir ->
  DriverRung2DirectMirZone.execution -> DriverRung2Execution.EmitDirectMir`.
  This removes Main's direct action/backend bypass but does not yet replace a
  C-owned compiler semantic path.
- Hard-substitution accounting is `BLOCKED` at the next rung, not complete:
  `dir.domain_graph` must own one typed `DomainRuntimeTopology` carrying stable
  field/layer identity, relation endpoints, pool capacity,
  refresh/authority/state/lifecycle and action transition binding. The current
  native carrier is `MIRDeclHeader`; the last legitimate consumers are the
  target-neutral topology plan and self-host C/LLVM runtime emitters. Backend
  AST/source topology rereads are the forbidden fallback. The next falsifying
  fixture is `zone_layer_projection_runtime`.
- The production import closure is 450 files with no missing import. Reachable
  Pergyra-native declarations include func 3,617, struct 179, enum 6, object
  18, tobject 3, subject 17, action 17, zone 19, world 1, and intent 14.
  Keyword/declaration counts are topology
  evidence only; only the direct-MIR world/zone/subject/action call chain is a
  production execution witness.
- `docs/200_object_to_action_boundary_patterns.md` is the canonical authoring
  contract for the value-to-authority boundary. Values remain in `struct` /
  `object` / `tobject`; identity-bearing state belongs to `subject`; an
  `action` owns the public authority/state/effect transition; the current
  direct-MIR `zone` owns its authority/lifetime boundary; the compiler `world`
  delegates once. The full audit grades `struct` as a reachable supporting
  construct, not an independently substituting feature; `class/object/vessel/intent`
  remain surface; artifact receipt/failure `tobject` values and only one
  subject/action/zone/world slice are reachable. The next
  source-to-MIR action must reuse/generalize the active execution boundary
  rather than mechanically add a zone per compiler stage. Root `intent`
  takeover follows only when a real compiler purpose binder and its fact
  bundle are executable through production and replace the direct bypass.
- Raw file-handle I/O had a real capability escape. Semantic analysis now
  refines literal `FileOpen` modes (`r`/`w`/`a`/`+`) and conservatively requires
  read+write for dynamic modes. Native C-inline and LLVM-linked runtime twins
  enforce actual open mode plus `FileRead`, `FileWrite`, and `FileExists` at
  runtime. Shell and PowerShell manifest gates cover read/write
  under-declaration; the runtime gate covers grant/deny and denied-write
  zero-artifact behavior.
- Compiler artifacts no longer use those raw handles. One shared runtime core
  now owns same-directory exclusive temp creation, checked write/flush/close,
  atomic replace, cleanup, and generation-tagged transaction handles for both
  C-inline and LLVM-linked output. The Pergyra owner maps scalar status
  immediately to `tobject SelfMirArtifactReceipt`/`SelfMirArtifactFailure`;
  the typed executed variant requires the receipt. It claims atomic visibility only,
  never crash durability. Production MIR JSON, direct-MIR action, bootstrap
  outputs, and rung-1 CLI outputs have no raw-final writer fallback.
- The source-to-MIR production path validates `SelfMirProgramFacts` once and
  calls `SelfMirProgramJsonWriteArtifactVerified`; the writer no longer repeats
  the whole graph validation that contributed to the multi-GiB symptom. The
  raw compatibility writer retains exactly one validation at its boundary.
- The codegen bootstrap's independent `0xC00000FD` failure was parser stack
  depth, not another multi-GiB graph allocation. A manually duplicated 123-row
  builtin-signature `&&` contract produced 123 nested precedence frames while
  reading the 2.46 MiB `main_ast.txt`. The signature registry now verifies its
  projection with one bounded owner loop, and the expression environment
  consumes that verifier. `make self-host-codegen-bootstrap-seed-test-smoke`
  is green through gen2 seed readiness with the normal 2 MiB PE stack reserve;
  the observed gen0/gen1 private-memory range was about 490/560 MiB.
- The next integrated-driver failure exposed a real self-host grammar gap:
  top-level dispatch recognized `export` but not native `public`/`private`, and
  nominal AST emission did not carry explicit visibility. It now maps
  `public`/`export` to the same `[export]` fact and `private` to non-export via
  `LanguageWordId`. Native/self-host AST is byte-equal for the committed
  `top_level_visibility_decl` witness, and the production `public zone
  DriverRung2DirectMirZone` parses through the self-host parser.
- The `selfhost.action_contract` supporting seam now has one semantic owner,
  `SemanticAstActionContractFacts`, keyed by callable `SyntaxNodeId`. The
  self-host parser preserves distinct Action/Function identity and exact
  requires/within/causes/authorized/caps/effects/body nodes; native and self
  MIR declarations emit the same `callable_kind + contract` wire; `mir_lower`
  validates it once and reconstructs the exact Action rows. Codegen does not
  skip clauses to find `Body:`. `semantic.callable_contract_vocabulary` now
  owns the 9 capability and 9 effect rows, canonical order, mask-symbol link,
  manifest spelling, and `local` zero-exclusive policy. Native/self/runtime
  consume direct or generated projections. The gate rejects missing/unknown
  fields, duplicates, noncanonical order, and `local + nonlocal` in both orders
  before backend output. The old AST node 88972 `Within:` / `expected Body:`
  result is retained in troubleshooting as the historical falsifier.
- `selfhost.action_contract` and its semantic vocabulary are now `CLOSED` as a
  declaration-carriage fact family. This still does not replace a C-owned
  compiler path. Production direct-MIR remains `REACHABLE`, not `SUBSTITUTING`;
  source-mode `Main -> CompileSourceTo*` is not deleted.
- The focused C shard now carries `Damage` as explicit `effect/effect` identity,
  requires `causes Damage` to resolve to that declaration, preserves
  `Damage.bearer=subject_slot` and `BattleZone.damage=effect_slot`, reconstructs
  exact domain-slot AST rows, and completes emitted-C compile/run. Zero-explicit
  parameter role impls also retain their implicit self ABI. This closes typed
  effect declaration plus C value-ABI admission only; relation declaration,
  stable field identity, pool capacity and zone runtime operations remain open.
- `mir_decl_field_kind_vocabulary.def` owns 14 stable wire spellings and AST
  labels, including distinct general/shared fields. Native C consumes the
  registry directly and self-host consumes a checked generated projection.
  Missing/unknown/invalid host kind, subject/effect-slot flattening and loss of
  the effect's exactly-one subject participant fail before backend output.
  `semantic.nominal_field_kind` remains `BRIDGE`, not `CLOSED`.
- The unfiltered `valid_array_builtins` failure was a separate runtime-header
  SoT omission. Array runtime emission already owned `uses_array` but did not
  pass it to header selection, while emitted owned-String helpers require
  `<string.h>` and `pgy_runtime_panic_contract.h`. Header selection now consumes
  `uses_array`; array-only output receives only those narrow dependencies and
  does not falsely claim String surface use or checked-arithmetic ownership.
- Match-case pattern identity no longer has a second physical graph. Typed
  `MatchCase` AST atoms feed one bounded HIR fact; `AstTreeArtifact` payload v3
  carries executable expression graphs only. The parser partition owner,
  `match_pattern_graphs`, and ordinal join are deleted, and the component gate
  rejects their return. Malformed/or-pattern/string/duplicate-binding patterns
  fail closed. The owner row remains `BRIDGE` only because four codegen helpers
  still structure a passed pattern string instead of receiving the typed fact.
- Authority evidence is deliberately bounded. `MIRDeclMethod` owns declaration
  clauses and `MIRDeclZoneAuthority` owns zone topology. The current C/LLVM
  world hook supports only the exact direct `world -> zone -> subject` receiver
  with one `authorized by self`; named, multiple, or indirect world-action
  authority shapes fail closed. The C helper separates "no check applies" from
  check-materialization failure with `bool + out`, so allocation failure cannot
  silently emit an unchecked call. Runtime validation currently proves
  non-null zone/participant presence, not identity-token or ability
  authorization.
- Nested construction is owner-preserving inline materialization with no
  surviving source alias. It is not a physical zero-copy/stable-address proof.
  Likewise one compiler world declaration/composition graph is not a runtime
  singleton; each composition call materializes a value aggregate. The world
  has one executable `direct_mir` member, while the other 18 declared zone
  types remain target topology. This removes the former 19-argument aggregate
  zero-fill fallback and keeps construction exact-arity.
- Hosted method scheduling is declaration-inventory owned. C emits nominal
  forwards/layouts, then domain value layouts, then nominal hosted bodies.
  LLVM registers nominal/domain layouts, then method signatures, then bodies.
  A later-declared by-value object fixture is green on both backends; missing
  metadata fails closed instead of guessing an opaque/scalar layout.
- The 3+ GiB semantic spike was a real native compiler defect. Each of 28,233
  dependency edges retained `bool[N] + size_t[N]` graph-sized scratch until
  context destruction for a 27,807-node graph. Per-edge path probing is now
  removed; the completed graph is validated once, and pass 2 revalidates only
  when node/edge generation changes. Exact-source C peak private memory fell
  from 3,522.4 MiB to 1,566.4 MiB; LLVM completed at 1,226.0 MiB under the
  unchanged 3,072 MiB cap.
- Last observed native build: incremental UCRT64 `make -j4` completed and linked both
  `bin/pgy.exe` and `bin/pgy-lsp.exe`. Current `world.pgy --emit-c` completed in
  28.1 seconds at 564.1 MiB peak private under the unchanged 3,072 MiB cap.
  Current `driver_bootstrap_main.pgy --backend=c` completed in 104.2 seconds at
  1,560.6 MiB peak private; `--backend=llvm` completed in 181.3 seconds at
  1,225.0 MiB peak private under the same cap. Both fresh drivers passed the
  hello/`let_log`/`multilet` one-MIR C/LLVM projection gate. Topology,
  compiler-world/component contracts, object/action, execution-action, C/LLVM
  authority ABI including unsupported named/multiple/indirect shapes, hosted
  later-value-object parity, and AIR 144/0 are green.
- Current focused capability evidence is green in both C and LLVM:
  `run_manifest.sh`, `run_manifest.ps1`, and `run_runtime_enforce.sh` cover
  literal read/write/update modes, dynamic-mode conservative inference,
  `FileExists`, host grant denial, and denied-write zero artifact. The
  object/action boundary, documentation-quality, and recursive compiler
  topology gates are also green.
- Current declaration evidence: the isolated native compiler rebuild, field-kind
  vocabulary projection, self-host component contract, semantic declaration
  identity, documentation/object-action and SoT edge/single-owner gates are
  green. Focused `function_clause_order_minimal` C DRV-2 observes native/self
  canonical MIR, seven effect/field-kind negative mutations, implicit role-self
  ABI, emitted C compile and runtime parity. The broad MIR JSON gate had three
  stale schema/harness expectations repaired, then reached the unchanged
  `for_continue` negative where a wrong-predecessor self phi is still accepted;
  the full gate is therefore RED and must not be reported as passed. No Coq
  prover is installed, so `SoTAuthority.v` reports explicit `DECLARED SKIP` and
  was not theorem-checked on this runner.
- Known unrelated RED: native semantic suite is 2,800 passed / 2 failed in the
  pre-existing Option/Result match-destructuring direct unit cases. The graph
  cycle/provenance cases pass; the full `type_resolution_dag_smoke.sh` wrapper
  inherits the same two failures. The likeness ratchet's stale
  `core_string_munge=72` and `sentinel=0` ceilings were audited against the
  exact pre-change HEAD, which already measured 79 and 11. The gate records
  those existing debts without adding a new String-to-String function or
  sentinel, and requires 19 declared zone types but only one
  production-reachable world member. Existing MIR
  inventory/link gates retain their separately documented pre-existing
  failures. `mir_json_parity.sh` additionally remains RED at the pre-existing
  `for_continue: wrong-slot self phi was accepted` negative. Do not weaken any
  semantic gate for this rung.
- The prior `valid_array_builtins` emitted-C failure has an owner-level fix:
  `uses_array` now reaches runtime-header selection and supplies `<string.h>`
  plus the panic contract. The focused emitted-C compile/run is green; the full
  unfiltered DRV-2 matrix must be rerun at the integration boundary.
- The artifact falsifier is now green: a pre-existing sentinel is preserved
  under injected open/write/flush/close/publish failure, no temp remains, no
  success receipt is issued, and C-inline/LLVM-export status agrees. The next
  falsifying fixture for ActionContract carriage and typed effect declaration
  is now green through self-host source -> native/self `pgy.mir.v1` ->
  `mir_lower` -> focused C compile/run plus field/vocabulary mutations. The
  shared caps/effects and field-kind vocabularies have single owners, but only
  the callable vocabulary is `CLOSED`. The next executable rung is typed
  `DomainRuntimeTopology` on `zone_layer_projection_runtime`; it must remove
  backend AST topology reads as its direct bypass. Production source-to-MIR
  action substitution and `Main -> CompileSourceTo*` deletion follow only after
  that runtime fact is executable. Root-intent takeover comes later.

The remainder of this file preserves earlier v63-v74 evidence as history. If a
historical statement below conflicts with this checkpoint, current source,
registries, and executable gates win and the stale statement must not be used
as a continuation fact.

## Historical v74 resume checkpoint

- Exact v74 executable revision: `bce4ae6f75a36dc014e19515732468a5de0de245`
  on `main`. Its direct parent is the v73 handoff correction `a9f5dfaa`; the
  v73 executable boundary remains `ed9fd179`. This handoff refresh is a
  documentation-only follow-up, so use `git rev-parse HEAD` for the checkout
  revision while retaining `bce4ae6f` as the loop-break boundary.
- v74 satisfies the hard executable-progress guard. One unchanged 7,054-byte
  `break_after_stmt.pgy` MIR has SHA-256
  `cb2d4f9fad6411ae9ce54e2d072d038735c29d2499a960909a09fae8eb59efbf`.
  C and LLVM compiled from that identity and matched normalized native output
  `3`, `3`.
- Certificate and plan schemas are v6. One break fact binds typed
  preheader/header/decision/break/empty-continuation/exit roles, one while
  summary, the actual `b4` continuation predecessor separately from the
  `i.4@b2` definition, one header phi, exact break row, two Log uses, and
  normal-exit `i.2` versus break-exit `i.4` lanes. Repaired digests cannot
  legitimize topology, SSA, or exit-selection drift.
- Ownership remains split by Pergyra responsibility: common fixed certificate
  identity/readiness, loop-break topology and SSA, target-neutral break shape,
  fixed break plan, and one break text emitter containing both C and LLVM. The
  common dispatcher remains the last artifact-producing full-plan consumer;
  emitters receive no MIR, JSON, index, or full plan. LLVM's exit phi is marked
  backend-only materialization and is not a second MIR fact.
- The fresh Pergyra-built bounded bootstrap is green: generated seed and
  native oracle match on sample C, MIR production, and bounded MIR consumption.
  That seed passes hello/`let_log`/`multilet`, every CFG predecessor through
  `forloop`, and original/late-break/zero-trip break execution. Phi storage
  permutation is byte-identical and all break/topology/SSA/graph/plan negatives
  reject before artifact acceptance.
- `src/lexer/language_keyword_registry.def` owns 145 sorted identities and
  all native/self-host stable IDs and metadata projections. Native lexer/debug,
  generated self-host projection, 27-row native/self-host completion, hover,
  and exact 92-row TextMate spelling/scope are registry-directed. The row stays
  `BRIDGE`: generated implementation census records typed native+self-host
  selectors for 80 words, direct-string-only self-host selectors for 18,
  native-only selectors for 46, no parser selector for `channel`, and 37 raw
  direct selectors across 34 words. Support flags, fixtures, and tooling do not
  promote implementation status.
- Pergyra-native dogfood status is now explicit. The bootstrap import closure
  has 403 files with no missing import; its non-fixture/generated/probe
  reachable declaration set has 2,664 `func`, 175 `struct`, four `enum`, one
  `subject`, and one `action`. `world`/`zone`/`intent`/`role`/`ability`/`effect`
  remain zero. `DriverRung2Execution.EmitDirectMir` is the first production
  `REACHABLE` action; `world.pgy`, `stage_intents.pgy`, and
  `authority_owner.pgy` remain unreachable, and the 16 declared compiler-world
  actions still consume readiness facts only. The world remains
  `SURFACE`/`BRIDGE`, not the executable root.
- The combined action ABI prerequisite is green on C and LLVM: subject action,
  aggregate request, enum-bearing aggregate result, capabilities, and action-
  internal `WriteFile`/`ReadFile` produced `ok / artifact-written / 17` and the
  same `driver-action-abi` file. The production direct-MIR action also owns
  requested -> target-admitted -> artifact-written/rejected, target admission,
  exact artifact acceptance, and the final `WriteFile`. `Main` no longer calls
  `CompilerTargetProjectionFactFromOwner`,
  `CompileMirJsonToDirectBackendVerified`, or direct-mode `WriteFile`.
- The production action exposed a native C declaration-order bug: hosted method
  bodies were emitted before early-eligible file-scope prototypes. The early
  function/intent prototype pass now precedes nominal method-body emission.
  `subject_action_global_helper` reproduces the old implicit/conflicting
  declaration failure and now passes C/LLVM with output `12`. The current
  current driver compiles with 0 errors/0 warnings, and the direct one-MIR
  hello/`let_log`/`multilet` C/LLVM parity plus negatives remain green.
- This action rung is `REACHABLE`, not `SUBSTITUTING`: it replaces a Pergyra
  `Main` orchestration bypass but does not yet replace another C-owned compiler
  path. Released/default replacement therefore remains 0%.
- The historical 20+ GiB / 3 GiB symptom came from cumulative graph copying
  and repeated whole-arena/readiness validation. The accepted direct CFG path
  keeps one typed admission/certificate issuance followed by fixed-size
  identity checks. During the successful v74 seed emission, an observed
  non-peak sample was 944.3/847.7 MB private/working set; it is not peak proof,
  but shows no 20 GiB-class recurrence.
- This is a real Pergyra-owned replacement for the bounded direct-CFG path.
  It does not replace the released/default C-owned `pgy`; released/default
  replacement remains 0%.

## Exact dirty state at this handoff

The exact executable/dogfood implementation checkpoint for this handoff is
`62d601f5e296aa88ecdbce9bbc88edad7b595c21`; the handoff-only refresh commit
`fce6b14654efb48acc0370a5ea97f9ecd4479d21` follows it without changing
compiler semantics. The object-to-action boundary audit is the next child of
that checkpoint; use `git rev-parse HEAD` for its exact commit after landing.
The v74 executable boundary
remains `bce4ae6f`; the later language-word/dogfood work is a supporting
SoT/contract checkpoint, and the direct-MIR production action is a reachable
dogfood boundary. Neither changes released/default replacement.
The following unstaged files are concurrent user work and must remain
unmodified and excluded from task commits:

- `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`.

No registry, dogfood-contract, action-rung, codegen-ordering, ABI-probe, v74
implementation, gate, or documentation file should remain dirty.

## Object-to-action boundary audit checkpoint

- `docs/200_object_to_action_boundary_patterns.md` is the canonical authoring
  matrix for `struct`, `class`, `object`, `tobject`, `vessel`, `subject`, and
  `action`. Parser/semantic/MIR/codegen owners and executable gates remain the
  semantic authority.
- Function-parameter carriage and hosted receiver are separate. `vessel` is
  value-carried but pointer-self when hosted; `subject` is identity-referenced
  and pointer-self; `class`/`object`/current `tobject` are value-self.
- `func` is not synonymous with pure and `action` is not synonymous with
  impure. Use action only for a subject-owned public state/authority/resource/
  stage transition with an explicit failure boundary.
- The current bootstrap closure reaches only one `subject` and one `action`.
  Unreachable `compiler/world.pgy` declares object 18, tobject 1, subject 16,
  zone 18, world 1, and action 16; all 16 actions are readiness facades and do
  not count as `REACHABLE` or `SUBSTITUTING`.
- The source-backed static ratchet is
  `tests/object_action_boundary_contract_smoke.sh`. It pins six nominal kinds,
  subject-only action, struct hosted-func rejection, subject/vessel pointer-self,
  object/tobject immutability, and the honest current tobject-helper debt.
- Open falsifiers: tobject hosted method, object bare-field mutation, class
  mutator persistence, subject bare/`self.` mutability drift, temporary subject
  action receiver, full action-contract MIR carriage, and duplicated C/LLVM
  post-action sync.
- This is the second consecutive supporting SoT/docs checkpoint after
  `fce6b146`. The next commit must be an executable zone/world replacement delta
  or record the exact blocking fact; do not start a third SoT-only commit.

## Completed Pergyra-native direct-MIR action objective card

- Objective: move the actual direct-MIR C/LLVM target admission and artifact
  write transition from `driver_bootstrap_main.pgy` into one production-
  reachable Pergyra subject action.
- Priority: stable target identity; requested -> target-admitted -> artifact-
  written/rejected state; actual bootstrap reachability; direct bypass deletion;
  unchanged MIR/certificate/plan facts; C/LLVM/native parity; then world/zone
  attachment.
- Fact owner: current MIR, semantic, ABI, target-projection, certificate, plan,
  and emitter owners remain authoritative. The new execution owner owns only
  CLI request-to-target admission, action state, failure, and the final output-
  write handoff.
- Last legitimate consumer: bootstrap execution action immediately before the
  artifact sink. `Main` owns argument spelling only; the direct backend owner
  continues to own artifact generation.
- Forbidden fallback: `Main` calling
  `CompilerTargetProjectionFactFromOwner` or
  `CompileMirJsonToDirectBackendVerified`; action failure re-entering the old
  path; target strings re-owned outside current owner; separate C/LLVM action
  or world graphs; semantic/MIR/ABI reconstruction inside the action.
- First falsifying fixture: unknown or corrupted direct-MIR target admission
  reaching `WriteFile`, or a rejected action still producing an artifact.
- Acceptance gate: the production import/call graph reaches the action; the two
  direct `Main` calls are statically forbidden; fixed MIR identity and all
  target/graph/certificate/plan negatives still reject before artifact; C,
  LLVM, and native outputs remain equal.
- Result: complete at `REACHABLE`. The current native driver compiles with
  0 errors/0 warnings; the static no-bypass gate, component contract,
  subject/action ABI parity, targeted C/LLVM global-helper regression, and
  hello/`let_log`/`multilet` one-MIR parity/negative gate are green.

## Active zone/world attachment objective card

- Objective: attach the reachable direct-MIR execution action to a real
  target/artifact zone boundary, then let one compiler world compose that
  boundary without copying MIR, ABI, target, certificate, plan, or artifact
  facts.
- Priority: preserve current target/artifact identity and action transitions;
  bind an actual resource/authority/lifetime boundary; reject missing authority;
  keep one C/LLVM-neutral graph; only then connect a root intent.
- Fact owner: existing typed target and artifact owners remain authoritative.
  The zone/world owns orchestration and authority only.
- Last legitimate consumer: the execution action at the artifact sink; the
  zone may admit and observe it but may not become another emitter owner.
- Forbidden fallback: importing the entire 5,919-LOC world closure merely to
  raise keyword counts; separate C/LLVM worlds; readiness-only action; direct
  `Main` re-entry; source/MIR JSON re-scan inside a zone.
- Falsifying case: missing or wrong zone authority still reaches the action, or
  a rejected action leaves an accepted artifact.
- Blocker: root-intent follow-up is blocked on six missing `authorized by`
  bindings:
  `bin/pgy.exe src/self_hosted/compiler/world.pgy --emit-c` currently exits 1
  with 6 errors/5 warnings. Do not import the whole world to fake progress.

## Current measured evidence

| Slice | Exit/time | Peak private / working set | Result |
| --- | ---: | ---: | --- |
| v63 observed current-driver build | 0 / 54,476 ms | 2,593.7 / 2,582.8 MB | Current parser/interpolation owners compiled below the cap. |
| C-oracle full MIR producer | 0 / 767,407 ms | 844.3 / 762.8 MB | 54,205,046-byte verified MIR emitted. |
| Pergyra gen2 full MIR producer | 0 / 1,210,574 ms | 1,091.0 / 963.4 MB | Byte-identical to the C-oracle MIR; no partial output. |
| full MIR consumer to gen2 C | 0 / 1,774,216 ms | 1,714.8 / 1,590.9 MB | Complete 3,378,704-byte C emitted. |
| gen2 host compile | 0 / 4,721 ms | 302.1 / 316.4 MB | `driver_gen2_v63.exe` created. |
| gen2 to gen3 C | 0 / 800,248 ms | 2,033.2 / 1,867.9 MB | Same MIR consumed; gen3 C byte-equal to gen2 C. |
| gen3 host compile | 0 / 4,942 ms | 337.0 / 351.6 MB | `driver_gen3_v63.exe` created. |
| fresh v64 codegen/parser seed refresh | 0 / 412,649 ms | 1,107.9 / 1,123.6 MB | Isolated current gen2 codegen and parser seeds created. |
| rewired full-bootstrap runner | 0 / 3,770,822 ms | 2,658.0 / 2,667.1 MB | Pergyra/C MIR parity, gen2 compile/bounded preflight, and gen2/gen3 C equality all passed. |
| v65 bounded integrated-driver rebuild | 0 / not separately timed | not separately sampled | Pergyra-built seed includes the backend-neutral direct MIR projection owner. |
| one-MIR direct C/LLVM gate | 0 / 12,596 ms | not separately sampled | One MIR SHA remained stable; both artifacts compiled, ran, and matched the native C oracle; graph/kind/target negatives passed. |
| v66 bounded integrated-driver rebuild | 0 / not separately timed | 2,108.9 / 2,096.3 MB observed sample | Pergyra-built seed includes typed instruction-use and scalar graph admission; bounded MIR consumer parity passed. |
| v66 hello + let_log direct C/LLVM gate | 0 / 17,371 ms | not separately sampled | Both MIR identities remained stable; C/LLVM compiled and matched native output; result/use/missing-use/operator/call-target negatives passed. |
| v67 final r3 bounded integrated-driver rebuild | 0 / not separately timed | 764.8 / 673.3 MB observed sample | Final source, document carrier, one-pass graph schema, and owner-directed ABI projection compiled into the Pergyra-built seed; bounded MIR consumer parity passed. |
| v67 hello + let_log + multilet direct C/LLVM gate | 0 / 24,462 ms | not separately sampled | All three MIR identities remained stable; C/LLVM compiled and matched native outputs; local/use/operator/order/ABI/reindex/bridge/target negatives passed. |
| v68 bounded integrated-driver rebuild | 0 / not separately timed | 882.5 / 782.0 MB observed sample | Current certificate, plan, and combined C/LLVM emission owner compiled into the Pergyra-built seed; bounded seed/oracle and consumer parity passed. The sample is not a peak. |
| v68 scalar regression + CFG/AIR plan gate | 0 / not separately timed | not separately sampled | Hello, `let_log`, and `multilet` remained green; one unchanged `ifelse` MIR drove one certificate/plan and both compiled backends with native-equal `pos`; CFG, AIR, certificate, plan, and target negatives rejected before output. |
| v69 bounded Pergyra-built r2 bootstrap | 0 / 441,708 ms | root-only summary invalid for gen2 | Seed/oracle MIR and bounded consumer parity passed. Git Bash reparented the native gen2 worker, so the root-only 27.7/9.8 MB summary is not memory evidence. |
| v69 detached-worker-aware gen2 seed emission | 0 / 355,226 ms | 1,022.1 / 937.2 MB measured peak | `gen2.exe` top private 1,005.8 MB; 3,366,105-byte C output SHA `ef8f0be...06637` was byte-identical to the bounded seed; 3,072 MB cap not exceeded. |
| v69 native-current + Pergyra-built r2 focused gate | 0 / not separately timed | not separately sampled | hello/let_log/multilet/no-phi ifelse/phi if_else_assign all green; C/LLVM/native output matched and CFG/phi/certificate/plan negatives rejected before output. |
| v70 Pergyra-built bounded bootstrap | 0 / not separately timed | 875.2 / 776.5 MB observed sample | Fresh generated driver seed matched the native oracle on sample C, MIR producer, and bounded MIR consumer. The memory row is an in-flight sample, not a peak. |
| v70 Pergyra-built direct-false CFG gate | 0 / not separately timed | not separately sampled | `reassign_block` MIR SHA stayed `c891...b223b`; C/LLVM/native output `10` matched and edge/predecessor/phi plus certificate/plan mutations rejected pre-artifact. |
| v71 final r2 Pergyra-built bounded bootstrap | 0 / not separately timed | not separately sampled | Fresh generated driver seed matched the native oracle on sample C, MIR producer, and bounded MIR consumer with certificate/plan v3. |
| v71 final r2 Pergyra-built nested CFG gate | 0 / not separately timed | not separately sampled | `nestedif` MIR SHA stayed `20e5...b3db0`; C/LLVM/native output `big` matched and inner-use/edge/merge plus repaired certificate/plan mutations rejected pre-artifact. |
| v72 native-current loop CFG gate | 0 / not separately timed | not separately sampled | `whileloop` MIR stayed 4,692 bytes / `c48c...e50fb0`; C/LLVM/native output `0`, `1`, `2` matched, phi-order permutation was byte-identical, and loop-summary/topology/SSA/graph/assignment-target mutations rejected pre-artifact. |
| v72 final r2 Pergyra-built bounded bootstrap | 0 / not separately timed | not separately sampled | Fresh generated driver seed matched the native oracle on sample C, MIR producer, and bounded MIR consumer with certificate/plan v4. |
| v72 final r2 Pergyra-built loop CFG gate | 0 / not separately timed | not separately sampled | Scalar rungs and every CFG predecessor remained green; the fresh seed passed `whileloop` C/LLVM/native execution, phi-order permutation, and all pre-artifact mutations. |
| v73 native-current range CFG gate | 0 / not separately timed | not separately sampled | `forloop` MIR stayed 3,197 bytes / `02a6...61720`; C/LLVM/native output `0`, `1`, `2` matched, generalized `2..5` and zero-trip `3..3` passed, and range fact/topology/graph/policy mutations rejected pre-artifact. |
| v73 Pergyra-built bounded bootstrap | 0 / not separately timed | 988.4 / 887.8 MB largest observed sample | Current generated seed matched the native oracle for sample C, MIR producer, and bounded MIR consumer with certificate/plan v5. The memory value is an in-flight sample, not a peak. |
| v73 Pergyra-built range CFG gate | 0 / not separately timed | not separately sampled | Scalar rungs and every CFG predecessor remained green; the fresh seed passed original/generalized/zero-trip range execution and all pre-artifact mutations. |
| v74 Pergyra-built bounded bootstrap | 0 / not separately timed | 944.3 / 847.7 MB observed sample | Current generated seed matched the native oracle for sample C, MIR producer, and bounded MIR consumer with certificate/plan v6. The memory value is an in-flight sample, not a peak. |
| v74 Pergyra-built loop-break CFG gate | 0 / not separately timed | not separately sampled | Scalar rungs and every CFG predecessor remained green; the fresh seed passed original/late-break/zero-trip execution, phi permutation, and all strengthened pre-artifact mutations. |
| direct-MIR action native build | 0 / not separately timed | not separately sampled | The current driver compiled with 0 errors/0 warnings after early global prototypes moved ahead of hosted method bodies. |
| subject action global-helper regression | 0 / not separately timed | not separately sampled | A subject action calling a nominal-return file-scope helper passed C/LLVM and produced `12`; the pre-fix C order reproduced implicit/conflicting declarations. |
| reachable direct-MIR action one-MIR gate | 0 / not separately timed | not separately sampled | hello, `let_log`, and `multilet` kept fixed MIR identities; direct C/LLVM outputs and all existing negative mutations passed through the action-owned artifact handoff. |

## Current gates and artifacts

Green:

- focused parser interpolation graph contract and 188-row parser manifest;
- native/self-host/fixture AST byte parity for `pipe_and_try`;
- DRV-2 C build and executable `let_log` readiness;
- native current-source `driver_bootstrap_main.pgy` C build: 0 errors and
  0 warnings;
- Pergyra-built bounded `tests/self_hosted/parity/driver_bootstrap.sh`, with
  seed/oracle production and bounded MIR consumer parity;
- `tests/self_hosted/parity/one_mir_dual_backend_projection.sh` using the
  current driver for hello, `let_log`, and `multilet`;
- `tests/self_hosted/parity/one_mir_cfg_air_plan_projection.sh` using the fresh
  Pergyra-built v74 seed for `ifelse`, `if_else_assign`, `reassign_block`,
  `nestedif`, `whileloop`, `forloop`, and `break_after_stmt`, including
  CFG/phi/nested/while/range/break/AIR/certificate/plan mutations;
- `tests/self_host_preparation_smoke.sh`;
- `tests/self_hosted_component_contract_smoke.sh`;
- `tests/language_keyword_registry_smoke.sh`: 145 registry rows, 71 reserved
  lexer rows, stable native/self-host word identity and metadata, native
  lookup/debug probe, eight generated projection owners below 600 lines, and
  exact generated-inventory drift checks;
- `tests/self_hosted/parity/parser_language_word_registry_parity.sh`: 80 typed
  word IDs reached by the current selectors, `action/impl/ref/own/type` native-
  selfhost AST parity, and matching rejection of unregistered `systemic`;
- `tests/self_hosted/parity/lexer_parity.sh`: all 9 sources byte-equal on C,
  LLVM, and live-native comparison;
- `tests/lsp_completion_registry_smoke.sh`: 27 registry-owned native/self-host
  completion rows; the old independent `items:[]` path is rejected;
- `tests/lsp_hover_registry_smoke.sh`: 25 lowercase language rows plus 7
  builtins, with C/self-host runtime parity and decoded multiline Markdown;
- `tests/vscode_language_graph_smoke.sh`: 92 exact highlighted rows, one full
  grammar, and no grammar ownership in the thin client;
- `tests/self_hosted/parity/driver_execution_action_abi_parity.sh`: C/LLVM
  subject/action aggregate ABI, enum result, capabilities, `WriteFile`/`ReadFile`,
  stdout, and artifact byte parity;
- `tests/self_hosted/parity/driver_rung2_execution_action_gate.sh`: production
  action reachability, requested/target-admitted/artifact-written/rejected
  transitions, exactly one backend-owner call and action-owned write, and no
  direct `Main` bypass;
- `tests/compare_backends.sh tests/cases/backend_compare/subject_action_global_helper`:
  C/LLVM output `12`, with the file-scope nominal-return helper prototype ahead
  of the subject action body;
- current `driver_bootstrap_main.pgy` native C build: 0 errors/0 warnings;
  the action-rung driver passed hello, `let_log`, and `multilet` one-MIR direct
  C/LLVM projection plus the existing graph/kind/target/ABI negatives;
- `tests/tooling_conformance_smoke.sh` and `make -j2 test`;
- `python scripts/sot_registry_gate.py`: 52 authorities, 54 derived carriers,
  `CLOSED=31 BRIDGE=21 ACTIVE=0`;
- `python scripts/protocol_registry_gate.py`: 7 protocol rows;
- `tests/build_pressure_contract_smoke.sh`;
- `tests/self_host_ci_profile_smoke.sh`;
- `PGY_DOC_QUALITY_FULL_UTF8=1 tests/documentation_quality_smoke.sh`;
- `PGY_ALLOW_MISSING_COQ=1 tests/formal_semantics_smoke.sh`: structural gate
  green, explicit missing-prover skip; 41 proofs not machine-checked;
- `git diff --check`;
- gen2/gen3 complete C byte equality and bounded gen2/gen3 parity.
- the rewired `tests/self_hosted/parity/driver_bootstrap.sh` full-fixpoint body
  with fresh isolated seeds under the 3,072 MB pressure owner.

Environment omission:

- `tests/formal_semantics_smoke.sh` passed its structural registry/load-path
  checks with `PGY_ALLOW_MISSING_COQ=1`, then declared the missing prover skip.
  No Coq/Rocq binary is installed, so the 41 proofs were not machine-checked;
  do not report this as proof-kernel success.
- `make` is not on the default PowerShell/Git-Bash PATH, but
  `C:\msys64\usr\bin\make.exe` is available. The action-rung native compiler
  rebuild used that MSYS2 make with `-j2` and succeeded. This does not
  retroactively prove an unrun full-fixpoint wrapper target.

Known RED, unchanged and not weakened:

- `tests/self_host_pergyra_likeness_smoke.sh` reports the newly explicit
  production reachability facts (`world_entry_imports=0`,
  `world_entry_refs=0`) but still fails its pre-existing smell baseline:
  `core_string_munge=79 > 72` and `sentinel=11 > 0`. The same 79 string-munge
  matches exist at the pre-task `HEAD`; this work did not loosen the ratchet.
- `bin/pgy.exe src/self_hosted/compiler/world.pgy --emit-c` exits 1 with 6
  errors/5 warnings because six authority-bearing intent steps omit required
  `authorized by` actors. AST/topology gates do not supersede this RED.

- `tests/self_host_hard_contract_smoke.sh` stops only because
  `driver_rung2_owner.pgy` lacks the pre-existing literal
  `"tests/cases/backend_compare/device_slot_machine_layer/main.pgy"`.
- `tests/self_host_compiler_world_contract_smoke.sh` still expects the retired
  `CompileSourceToMirJsonVerified(` spelling while the current entrypoint owns
  the pressure-observed/verified file variants. This mismatch predates v65 and
  was not weakened or folded into the active direct-backend rung.
- The separate full `self_host_compiler_build.sh` path stops before this CFG
  slice because its older gen2 seed does not recognize the current
  `ArrayPushOwnedString` builtin (`undefined_function`). The bounded integrated
  driver bootstrap used for v71 is green; do not conflate the stale full-build
  seed failure with the direct CFG implementation.

Current ignored evidence:

- `.tmp/instruction_writer_pressure/driver_source_v63_interpolation_graph.mir.json`;
- `.tmp/instruction_writer_pressure/driver_source_v63_gen2_owned.mir.json`;
- `.tmp/self_hosted/driver_bootstrap/v63_full.c`;
- `.tmp/self_hosted/driver_bootstrap/v63_gen3.c`;
- `.tmp/self_hosted/driver_bootstrap/driver_gen2_v63.exe`;
- `.tmp/self_hosted/driver_bootstrap/driver_gen3_v63.exe`.
- `.tmp/self_hosted/codegen/bootstrap_v64_formal/`;
- `.tmp/self_hosted/driver/bootstrap_v64_formal_r3/`;
- `.tmp/build-pressure/self-host-codegen-seed-v64-formal.summary.json`;
- `.tmp/build-pressure/self-host-driver-fixpoint-v64-formal-r3.summary.json`.
- `.tmp/self_hosted/driver/bootstrap_v65_one_mir/`;
- `.tmp/self_hosted/driver/one_mir_v65_formal/`.
- `.tmp/self_hosted/driver/bootstrap_v66_let_log/`;
- `.tmp/self_hosted/driver/one_mir_v66_formal/`;
- `.tmp/self_hosted/v66_falsifier/`.
- `.tmp/self_hosted/driver/bootstrap_v67_multilet_r3/`;
- `.tmp/self_hosted/driver/one_mir_v67_formal_r3/`;
- `.tmp/self_hosted/v67_falsifier/`;
- `.tmp/self_hosted/driver/bootstrap_v68_ifelse_native/`;
- `.tmp/self_hosted/driver/bootstrap_v68_ifelse_r1/`;
- `.tmp/self_hosted/driver/one_mir_cfg_air_plan/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v68_native/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v68_r1/`;
- `.tmp/self_hosted/driver/one_mir_v68_native/`;
- `.tmp/next_cfg_rung_audit/`.
- `.tmp/self_hosted/driver/bootstrap_v69_phi_native/`;
- `.tmp/self_hosted/driver/bootstrap_v69_phi_r2/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v69_native_fixed/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v69_r2/`;
- `.tmp/build-pressure/selfhost-v69-phi-r2.*`;
- `.tmp/build-pressure/selfhost-v69-phi-gen2-r2.*`;
- `.tmp/next_cfg_rung_audit_v69/`.
- `.tmp/self_hosted/driver/bootstrap_v70_reassign_r1/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v70_reassign_r1/`;
- `.tmp/reassign_rung_audit/`;
- `.tmp/next_cfg_rung_audit_v70/`.
- `.tmp/self_hosted/driver/v71_native_audit/`;
- `.tmp/self_hosted/driver/bootstrap_v71_nested_r1/`;
- `.tmp/self_hosted/driver/bootstrap_v71_nested_r2/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v71_native_r2/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v71_seed_r1/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v71_seed_r2/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v71_seed_final/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v71_full_r1/`.
- `.tmp/self_hosted/driver/v72_native_dev/`;
- `.tmp/self_hosted/driver/v72_native_final/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v72_native/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v72_native_r2/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v72_native_r3/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v72_native_final/`;
- `.tmp/self_hosted/driver/bootstrap_v72_loop_r1/`;
- `.tmp/self_hosted/driver/bootstrap_v72_loop_r2/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v72_seed_r2/`;
- `.tmp/next_cfg_rung_audit_v71/`;
- `.tmp/next_cfg_rung_audit_v72/`.
- `.tmp/self_hosted/driver/bootstrap_v73_range_r1/`;
- `.tmp/self_hosted/driver/bootstrap_v73_range_r2/`;
- `.tmp/v73_range_native_gate/`;
- `.tmp/v73_range_self_gate_r2/`;
- `.tmp/v73_range_self_gate_final/`;
- `.tmp/v73_forloop_audit/`.
- `.tmp/v74_break_native_r5/`;
- `.tmp/self_hosted/driver/bootstrap_v74_break_r1/`;
- `.tmp/self_hosted/driver/one_mir_cfg_v74_seed_final/`;
- `.tmp/v74_break_inspect.mir.json`;
- `.tmp/v74_break_inspect.ll`.

Current open boundary:

- `CompilerEmissionArtifact` still does not carry the verified plan revision
  and digest as a repository-wide artifact fact. The direct v74 emitter checks
  the plan immediately before artifact creation, so the bounded path is closed,
  but global artifact carriage remains open and must not be inferred from this
  fixture gate.

## Historical execution directive: gen2 takeover before global SoT closure

Effective 2026-07-26, freeze broad SoT expansion and new fixture breadth until
the integrated gen2 driver exists and takes over the compiler-source build.
This is a scheduling boundary, not permission to bypass an owner or weaken a
fail-closed check. The planning estimate for attempting to close the remaining
SoT globally is approximately one year because the unresolved ownership seams
are individually difficult; treating that global closure as a prerequisite
would prevent the executable bootstrap from reaching a terminus.

Count the active bootstrap in this order:

1. the existing C-owned seed consumes the complete compiler source and emits
   the full `driver_gen2.c`;
2. the native C compiler builds that artifact into the integrated gen2 driver;
3. gen2 consumes the same complete compiler source and emits `driver_gen3.c`;
4. only then compare gen2/gen3 artifacts and behavior for the fixed point.

The first hard self-host threshold is step 3: gen2 must take over the complete
compiler-source build currently performed by the C-owned seed. A bounded
component fixed point, additional owner document, registry closure, or fixture
count does not satisfy that threshold.

Apply SoT work only when the current executable rung exposes a concrete missing
fact. Name that fact, its owner, its last legitimate consumer, the forbidden
fallback, and the falsifying case; close only that blocking seam, then resume
the same gen2 run. Do not sweep unrelated `BRIDGE` rows or pursue global
registry closure. Do not add breadth fixtures. A new fixture is allowed only
as the smallest reproducer for the blocker observed on the active complete
gen2 path, and it must not become a substitute for rerunning that path.

Reassess the remaining SoT and fixture backlog only after gen2 has consumed the
same complete source successfully. Until then, executable artifacts and their
observed gates outrank SoT percentage, document volume, fixture count, and
bounded-only parity as progress evidence.

## Post-gen2 Coq gap audit (queued; not the active executable rung)

Do not start a broad proof expansion before the gen2 takeover above. Commits
`ae638458` and `58b3830d` establish the first vertical spine: 41 registered
`.v` files now include shared root `PergyraCore.v`, importers
`PergyraCoreComposition.v`, `UnifiedCore.v`, and
`PergyraCoreZoneBridge.v`, plus foundation-first/load-path wiring in the kernel
gate. The source audit found no `Admitted` or Coq `Axiom`, and only the two
declared `SlotCalculus` interface parameters (`MaxSlotId` and `verify_token`).
`tests/formal_semantics_smoke.sh` now registers all 41 files and compiles them
from the same sibling-module load path. No local Coq/Rocq binary was available,
so both new proofs and the migrated capstone remain pending the dedicated Rocq
9 kernel CI; the local structural run was an explicit prover skip, not proof
success.

The important proof gaps are refinement gaps, not unfinished `Qed` blocks:

1. the new shared core is not yet comprehensively bound to the live
   parser/semantic/AIR/MIR owner facts used by the integrated compiler;
2. the parser-to-AST boundary is still outside the machine-readable pass/loss
   manifest;
3. `IntentStepSoundness.v` proves a linear authority-guarded fragment, not the
   composed types/generics/world/zone/effect/slot/async language core;
4. exceptional and cancellation exits are not covered by the pin/resource
   cleanup proof;
5. the transitive world/zone/projection frontier scheduler and its termination
   are not closed;
6. cross-axis generic carriage and full call-site evidence attribution remain
   outside the current mechanized bindings.

The first post-gen2 Coq unit must therefore bind the exact gen2-accepted
compiler path to live owner facts and a negative adequacy gate. Do not add
another independent abstract law before that refinement bridge exists, and do
not turn whole-language soundness into the next global-closure project.

## Historical v60 resume checkpoint

- Implementation checkpoint: `3418b0f3` (v60 structured expression occurrence
  identity) on `main`. Structured MIR-to-AST emission carries
  `(global instruction row, AST lane, derived ordinal)` occurrences into one
  final graph arena. Repeated CFG visits repeat the producer key and receive a
  fresh range; source text is only an assertion. Required MIR producer coverage
  fails closed, the intermediate persisted sequence view is deleted, and the
  native range branch now projects its stop expression while loop-init retains
  the start. The complete run passed the v59 positional mismatch, completed
  graph construction and semantic analysis, and reached assignment body typing
  below 1,131 MB private. It advances the executable rung but is not gen2 or
  hard substitution.
- The v60 predecessor is `a4738c25`, following `7eef684b` (v59 prefix
  readiness) and `19ecce41` (linear expression arena assembly). v59 removed
  cumulative `place_kinds` rebuilding, per-append whole-arena readiness, and
  program/routine-index reconstruction, then exposed the positional identity
  mismatch at `ParsePrimaryFact` instead of crossing the 3 GiB cap.
- The accepted predecessor is `195d9b64` (v58 single-consumption loop branch
  projection) on `main`. It removes the second per-block branch
  selection and second per-branch scalar read from loop-summary readiness,
  preserves exact routine/block/span identity and FOR range/foreach semantics,
  and materially improves the adjacent v57 normalized markers through routine
  1,728. The accepted v57 predecessor is `ab3f9066` (direct match-local
  routine-index consumption). The preceding v56 implementation is
  `6f5c373d`, reverted by `c9e8011a`; its separate instruction-alignment pass
  remained slower than the adjacent v48 control after MIR-start normalization.
  v57 removes that redundant pass, retains one routine-index owner and one
  instruction loop, and materially improves the shared normalized markers.
  Do not add a third match-local read shape. The rejected v55 implementation
  is `2eeeec13`, reverted by `1f77b0bc`; focused gates and disassembly proved
  the local transformation, but the fixed full run regressed materially. The
  rejected v52 implementation
  is `8c49f74f`, reverted by `40037e52`.
  The successor-pair seam is abandoned after its first measured shape; do not
  re-express it as another pair struct, wrapper, or carrier. The rejected v51
  implementation is `e6abdeaa`; the rejected v50 carrier is `530682af`,
  reverted by `c5ee6e62`. Accepted compiler source
  retains `5e12cf43`'s isolated stray runtime-row fail-closed correction. Its
  accepted performance baseline remains `8074d6c8` branch selection plus that
  correction. The resource ABI performance seam is now abandoned after both
  carrier and local-scan shapes regressed materially. The earlier rejected v49
  implementation is
  `80a54268`, reverted by `85cee4ff`. Its phi-prefix
  admission predecessor is
  `a05aaf06` (`admit MIR phi prefixes once per routine`). Its phi-prefix carrier
  predecessor is `99e76e76` (`carry
  MIR phi prefixes in routine facts`). Its branch-row predecessor is `4ee29ce2` (`carry MIR
  branch rows in routine facts`). Its CFG negative predecessor is `ec4b9eef`
  (`cover invalid CFG backedge batch results`), with CFG owner implementation
  `73133678` (`batch MIR CFG backedge facts per routine`). Its scalar-key
  predecessor is `dfc8e406`, its optional ABI scalar predecessor is
  `bf8a56b8`, its
  exact ABI witness predecessor is `0da9c5c2`, its ABI
  row-capture predecessor is `a5d56f42`, its
  routine-scalar predecessor is `dd68d6f3`, its
  instruction-view predecessor is `06f6994d`, its
  evidence predecessor is `84f68161`, its
  admitted-structure predecessor is `190d0dbf`, its document-index predecessor
  is `67502f50`, its
  routine-consumer predecessor is `d62553ee`, its
  exact-span predecessor is `157c340b`, its
  machine-admission predecessor is `0857899e`, and the complete artifact
  predecessor is `6329356f` (`bound-mir-json-string-leaf-lifetime`).
- The verified driver now proves semantic readiness once and enters
  `SelfMirProgramFactsFromReadyArtifact`; the independently callable checked
  entrypoint still owns the complete validation contract.
- Direct local assignments still require local/target type equality. Member
  and indexed assignments validate the root local separately and no longer
  compare that root type with the final selected member/index type.
- Production `--emit-mir-json-verified` writes through
  `SelfMirProgramJsonWriteFile` instead of materializing one whole-program
  `String`. Program/routine/block and instruction-local unbounded graph/list
  rows are streamed. Escaped/quoted string leaves use a call-local allocator
  pool released immediately after synchronous `FileWrite`; numeric and fixed
  bounded projections remain unchanged.
- Initializer local visibility now advances through
  `SemanticAstInitializerEnvironmentCursor`. Function-base rows are seeded
  once, lexical locals are appended/popped in source order, destructure rows
  publish atomically, and the two per-row full-function local scans are absent
  from the production loop.
- Pergyra semantic and canonical MIR facts remain the SoT. C and LLVM remain
  peer native compiler projections with their existing execution/reference
  roles; self-hosted artifacts must be compared against the declared C/LLVM
  oracle class. The Pergyra-built DRV-2 is still a bounded self-host replacement
  lane; this checkpoint does not claim a fully self-hosted driver or a
  Pergyra-owned LLVM emitter. It does establish the first complete current
  full-driver MIR artifact below 3072 MB.
- The MIR consumer now creates one typed machine admission and carries the
  exact declaration and routine index used by that proof. Exact-bound JSON
  readers accept only structure-owner spans; declaration phases and the first
  AST reconstruction reuse their inventories instead of rebuilding root facts.
- Routine headers, match/destructure arrays, render/ABI facts, and phi result
  identity now consume one exact routine/instruction owner. CFG structural
  merge is a pure `mir_cfg_graph_owner.pgy` query with branch-local blocked
  reachability; the routine index no longer runs candidate-local BFS.
- The hard MIR input builds one `MirDocumentFactIndex` and carries its root and
  top-level array bounds through schema, capture, routine, and machine
  admission. Exact-bound string materialization no longer calls
  `Substring(json, ...)`, and null tokens use `SubEqualsWithLen`.
- The admitted `MirProgramRoutineIndex` captures the program-order
  routine/block/instruction structure, instruction kind/source type, and raw
  machine spans. Machine admission and `MirRoutineFactIndex` consume this
  derived `pgy.mir.v1` view. Whole-program readiness is proved once at
  admission; per-routine construction uses an O(1) row guard.
- Routine reconstruction now consumes a typed instruction view and a canonical
  CFG block-id projection from that admitted structure. Common no-layout and
  no-resource instructions are decided from exact bounds without repeatedly
  validating the same instruction object and rediscovering its field bounds.
- CFG successor identity is decoded once into `Array<Int>` rows. Missing edges
  alone use the internal negative sentinel; an explicit negative wire target
  fails closed at `cfg_successor` and is exercised through both C and LLVM.
- MIR phi `uses` is treated as the producer-owned incoming-value inventory, not
  a predecessor-indexed native phi table. Its accepted arity is
  `2 <= use_count <= predecessor_count`, and a self-result input requires a
  CFG-proven incoming backedge.
- Each `MirRoutineInstructionFactBundle` construction now captures `result`,
  `expr0`, `expr1`, `arg0`, `arg1`, `slot_anchor`, `abi_type_name`, and
  `match_variant` plus raw ABI value spans in one pass over a routine's
  program-owned spans. It remains
  routine-local rather than turning the program index into a second
  global/local aggregate. Render,
  match, graph, assignment, and phi consumers use that bundle. A malformed
  count cannot cross into the next routine, and duplicate or non-string scalar
  fields fail closed.
- Required ABI rows no longer rebuild a generic object table for every field
  and then repeat the same work during identity hashing. The ABI owner captures
  one nested row and its field rows, applies canonical hash order to that
  capture, and owns both producer and final-consumer identity. The old
  instruction-span validator and repeated-scan hash path are absent.
- One MIR-to-AST execution retains only successful exact ABI validation
  witnesses. A required hit needs the raw type value, canonical decimal ID,
  required state, and complete raw layout payload. ID-only and cross-run reuse
  are forbidden; a changed payload is revalidated and fails closed.
- The routine scalar pass carries whether the ABI type value was one valid
  string or exact optional `null`. The ABI owner remains the semantic owner and
  uses that observation only with exact optional `id=0`/`layout=null` tokens.
  Required tuples still take the complete raw witness path; wrong-kind or
  noncanonical values are not repaired or guessed.
- The same scalar owner scans each key for an escape and dispatches plain keys
  to their raw-length comparison group. Escaped keys retain full semantic
  comparison and duplicate detection. No scalar carrier, helper, cache, or ABI
  semantic owner was added.
- The existing CFG graph owner computes backedge headers once per routine from
  one entry-reachability result and one avoiding traversal per reachable
  distinct incoming target. The fact index consumes that result; the old
  per-edge function is deleted. Invalid batch input is an empty typed result
  and a nonempty consumer reports `cfg_backedge`. Structural merge and phi are
  unchanged.
- The routine-local instruction fact bundle now carries each block's unique
  branch global row from its existing scalar pass. Condition, loop-transfer,
  and match-binding consumers select that row through the admitted routine fact
  index instead of searching the block or repeating full bundle admission.
  Routine/block identity, local/global range, scalar span, and final branch kind
  are checked. Duplicate, out-of-block, scalar-span-mismatched, or non-branch
  rows fail closed; the old bundle accessor and routine-lowering search cannot
  return as fallbacks. The program index remains structure/identity-only rather
  than becoming a second global/local scalar aggregate.
- `BuildMirMatchBindingLocalFacts` now consumes the already-admitted
  `MirProgramRoutineIndex` row directly. One row-readiness proof and bounded
  block/instruction ownership checks precede one instruction loop; only
  canonical `AST_MATCH_CASE` branch rows contribute match local names/types.
  Invalid owners, zero-block parallel-array gaps, wrong-kind match rows, and
  name/type count mismatches fail closed, while forged non-match local arrays
  are ignored. No second graph, carrier, cache, backend split, or old-read
  fallback was introduced.
- `LoopFlowSummaryProjectionReady` consumes each block's owned branch global
  row once. Positive rows receive one exact branch selection and one scalar
  capture; no-branch rows use exact `-1`. Routine identity, block spans,
  instruction offsets/counts, malformed sentinels, FOR fields, and foreach
  iteration facts fail closed before projection. `BlockHasLoopTransfer` and
  rendered `BlockCond`/`"for "` classification are absent, and no second graph,
  cache, carrier, helper, backend split, or fallback was added.
- The same routine-local bundle carries each block's leading phi count. A phi
  after the first non-phi is an invalid sentinel. The phi semantic owner scans
  only that prefix while retaining predecessor, arity, result, incoming-use,
  and backedge validation. It admits routine identity, exact block counts, and
  bundle shape once at entry, then directly reads the prefix array. The one-use
  per-block accessor is deleted. Missing/invalid prefix facts cannot fall back
  to a whole-block scan or JSON kind recovery.
- A direct `EmitBlockStmts` block-slice experiment passed its fail-closed gates
  but regressed the fixed run by 8,169 ms at routine 1,920 and lost routine
  1,984. It is explicitly reverted. Current source retains the accepted v48
  block-accessor shape; the failed v49 shape is evidence, not an active
  fallback.
- A later resource-runtime experiment captured four top-level fact families in
  every instruction scalar and expanded the routine bundle. It removed about
  145.6 MB of repeated resource top-span reading by static estimate but built
  in 62,385 ms and reached only routine 1,728 at 296,959 ms. `c5ee6e62`
  reverts it. The review-discovered stray wrong-kind runtime row fail-open is
  retained alone in `5e12cf43`; a non-resource instruction can no longer treat
  an explicit runtime row as absence.

## Historical v60 dirty state

The semantic implementation checkpoint is `3418b0f3`; its handoff-only
successor carries no semantic change. After that checkpoint is pushed,
`main` and `origin/main` are synchronized and no task-owned implementation or
documentation change is dirty. These unstaged files are concurrent user work
and must remain unmodified and excluded from task commits:

- `tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_match_parity_owner.sh`;
- `tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh`.

## Historical v60 executable objective card

- Objective: finish MIR-to-AST lowering for the completed admitted full-driver
  MIR artifact, emit and compile the integrated gen2 driver, and immediately
  make gen2 consume the same complete compiler source to emit gen3.
- Priority: preserve the exact `pgy.mir.v1` artifact identity, keep the MIR
  consumer and semantic owners fail closed, stay below the fixed pressure cap,
  complete the gen2 takeover, then establish the fixed point. Do not widen SoT
  or fixtures before that takeover.
- Fact owner: the verified `SelfMirProgramFacts` producer and its completed
  `pgy.mir.v1` artifact. At the current boundary,
  `SemanticAstAssignmentTypeFactsFromArtifact` owns assignment body-type
  derivation and `SemanticAstBodyTypeBundle` is its receiving boundary. The
  structured occurrence order and final expression arena are already admitted
  inputs; they must not be rebuilt inside assignment typing.
- Last legitimate consumer: current `driver_oracle.exe --mir-json` emitting
  `driver_gen2.c`, followed by the native C compiler only as the bootstrap
  object-code boundary.
- Forbidden fallback: regenerating a native oracle MIR per generation,
  backend-specific JSON reads, source-text or graph-text recovery, a second
  expression graph/order/cache, per-assignment whole-program reconstruction,
  `new ? old` authority, or raising the 3,072 MB / 1,800-second bounds.
- Focused falsifier: on the same 51,807,108-byte MIR artifact, progress from
  `semantic-body-type-stage assignment:start` to `assignment:done` under the
  fixed limits, or expose the exact assignment row and repeated owned read that
  prevents completion. Do not reopen graph identity or broaden fixtures.
- Acceptance gate: pressure-owned full MIR consumption emits `driver_gen2.c`,
  that artifact builds, and the resulting gen2 consumes the same complete
  compiler source to emit `driver_gen3.c`. The bounded preflight remains a
  focused diagnostic, not a prerequisite track that may delay this takeover;
  compare gen2/gen3 only after both complete artifacts exist.

## Historical measured evidence through v60

The original 20+ GiB observation was dominated by repeated graph/readiness
validation. Closing those repeated validations brought the current driver into
the fixed 3 GiB pressure window. Sequential instruction projection plus
call-local string-leaf lifetime now completes the full artifact in that same
window. The latest fixed-cap observations are:

| Slice | Peak private | Peak working set | Last observed state |
| --- | ---: | ---: | --- |
| `mir-fact-ready` | 2865.8 MB | 2359.0 MB | Reached MIR lowering; exposed the composite-assignment invariant at syntax node 5290. |
| `assignment-composite-ready` | 3233.9 MB | 2716.4 MB | MIR facts completed; crossed the cap after `json:start`. |
| `json-builder-ready` | 3195.6 MB | 2680.9 MB | MIR facts completed; whole-program JSON still crossed the cap. |
| `json-file-ready` | 3290.1 MB | 2775.6 MB | Wrote 20,013,056 bytes before routine-string materialization crossed the cap. |
| `json-block-file-ready` | 3197.3 MB | 2678.8 MB | Wrote 20,901,888 bytes; per-instruction/field strings still accumulated. |
| `initializer-cursor-ready` | 3117.9 MB | 2601.7 MB | All 8,229 initializer rows and MIR facts completed; crossed after `json-write:start` with 13,709,312 bytes. |
| `instruction-stream-ready` | 3092.7 MB | 2574.5 MB | Unbounded instruction/graph rows streamed; crossed with a 40,263,680-byte partial artifact because leaf strings remained result-lived. |
| `instruction-string-pool-ready` | 3064.3 MB | 2544.9 MB | Exit 0; complete 51,807,108-byte artifact and `json-write:done`. |
| `full-mir-consumer-admitted` | 53.0 MB | 66.1 MB | Input schema/capture completed; timed out at machine admission. |
| `full-mir-consumer-bounded-cursor` | 54.8 MB | 67.8 MB | Timed out while building the routine index; cursor-only `strlen` debt remained in field reads. |
| `full-mir-consumer-exact-bound` | 59.3 MB | 72.0 MB | Reached `routine-index:done`; timed out after `instruction-scan:start`. |
| `full-mir-consumer-machine-twofield` | 63.6 MB | 76.0 MB | One-pass two-field instruction read; still timed out after `instruction-scan:start`. |
| `full-mir-consumer-key-compare` | 57.1 MB | 69.9 MB | Machine/input admission completed; timed out after `mir-to-ast:start`. |
| `full-mir-consumer-exact-span` | 58.0 MB | 70.7 MB | Declaration fields and routine ends consume carried spans; reached `declarations:done`. |
| `full-mir-consumer-routine-fact-exact` | 58.0 MB | 70.8 MB | Routine fact bundle consumes exact spans; reached `first-top-level-routine-fact-index:done`. |
| `full-mir-consumer-routine-indexed` | 58.0 MB | 70.7 MB | Result/match facts consume one routine index; first top-level routine completed, no gen2 output. |
| `full-mir-consumer-cfg-owner` | 57.8 MB | 68.7 MB | Structural merge uses branch-local blocked reachability; first top-level routine completed, no 16 marker or gen2 output. |
| `mir-document-index-driver-build-v2` | 2319.9 MB | 2322.4 MB | Integrated C driver compiled in 57,528 ms below the fixed cap. |
| `full-mir-consumer-document-index` | 63.4 MB | 74.0 MB | Timed out at 300,554 ms after the 16-routine marker; no gen2 output. |
| `mir-program-instruction-index-driver-build-v3` | 2405.9 MB | 2409.3 MB | Integrated C driver compiled in 50,974 ms below the fixed cap. |
| `full-mir-consumer-program-instruction-index-v3` | 85.2 MB | 93.6 MB | Timed out at 300,606 ms after the 16-routine marker; no gen2 output or cap crossing. |
| `full-mir-consumer-borrowed-fact-v9` | 82.6 MB | 92.8 MB | `ref` accessors alone did not help; routine 16 completed at 133,593 ms. |
| `full-mir-consumer-bounds-fast-v10` | 82.7 MB | 91.1 MB | Exact-bound common paths cut routine 16 to 69,919 ms, then exposed `FindTopLevelComma` phi inventory drift. |
| `full-mir-consumer-phi-inventory-v11` | 88.5 MB | 96.7 MB | Passed the phi counterexample and reached routine 64 at 99,411 ms; timed out with no gen2. |
| `full-mir-consumer-direct-block-v12` | 88.5 MB | 96.5 MB | Direct canonical block rows preserved behavior; routine 64 at 99,803 ms. |
| `full-mir-consumer-int-cfg-v13` | 88.6 MB | 96.6 MB | Timed out at 180,056 ms; routine 64 at 99,447 ms and routine 128 at 164,457 ms; no gen2. |
| `mir-int-cfg-negative-ratchet-driver-build-v14` | 2442.7 MB | 2430.8 MB | Final-source integrated C driver compiled in 48,451 ms below the cap. |
| `full-mir-consumer-int-cfg-v14-300s` | 94.3 MB | 102.1 MB | Timed out at 300,324 ms; routine 192 at 235,898 ms; no gen2. |
| `mir-routine-scalar-bundle-driver-build-v23` | 2509.8 MB | 2498.5 MB | Current-source integrated C driver compiled in 47,746 ms below the cap. |
| `full-mir-consumer-routine-scalar-bundle-v23` | 87.0 MB | 95.3 MB | Timed out at 180,343 ms; routine 64 at 96,607 ms and routine 128 at 160,331 ms; no gen2. |
| `full-mir-consumer-routine-instruction-detail-v37-300s` | 92.2 MB | 100.1 MB | Timed out at 300,186 ms; required ABI rows dominated and routine 248 completed at 290,268 ms. |
| `full-mir-consumer-abi-bounds-v38-300s` | 92.1 MB | 100.0 MB | Outer-bound capture alone was a negative result; routine 248 regressed to 293,877 ms. |
| `full-mir-consumer-abi-row-capture-v39-300s` | 134.7 MB | 140.8 MB | Timed out at 300,560 ms; routine 192 at 102,775 ms, routine 448 at 231,271 ms, and routine 640 at 298,374 ms; no gen2. |
| `full-mir-consumer-abi-owner-v40-build` | 2565.3 MB | 2554.5 MB | Exact final-source integrated C driver compiled in 55,007 ms below the fixed cap. |
| `full-mir-consumer-abi-exact-reuse-v41-build` | 2346.8 MB | 2336.6 MB | Exact-source integrated C driver compiled in 52,722 ms below the fixed cap. |
| `full-mir-consumer-abi-exact-reuse-v41-300s` | 157.2 MB | 162.3 MB | Timed out at 300,227 ms after routine 640 at 228,455 ms, routine 704 at 238,884 ms, and routine 896 at 288,574 ms; no gen2. |
| `full-mir-consumer-abi-optional-fast-v42-build` | 2515.0 MB | 2503.6 MB | Exact-source integrated C driver compiled in 53,265 ms below the fixed cap. |
| `full-mir-consumer-abi-optional-fast-v42-300s` | 214.4 MB | 216.6 MB | Timed out at 300,115 ms after routine 704 at 162,849 ms, routine 896 at 192,157 ms, routine 1,600 at 241,729 ms, and routine 1,920 at 293,147 ms; no gen2. |
| `full-mir-consumer-key-dispatch-v43-build` | 2523.0 MB | 2511.6 MB | Exact-source integrated C driver compiled in 52,451 ms below the fixed cap. |
| `full-mir-consumer-key-dispatch-v43-300s` | 215.1 MB | 217.1 MB | Timed out at 300,268 ms after routine 704 at 162,255 ms, routine 896 at 190,875 ms, routine 1,600 at 239,277 ms, and routine 1,920 at 290,054 ms; no routine 1,984 or gen2. |
| `full-mir-consumer-cfg-backedge-batch-v44-build` | 2433.5 MB | 2427.0 MB | Exact-source integrated C driver compiled in 52,316 ms below the fixed cap. |
| `full-mir-consumer-cfg-backedge-batch-v44-300s` | 202.7 MB | 205.0 MB | Timed out at 300,682 ms after routine 704 at 162,403 ms, routine 896 at 191,236 ms, routine 1,600 at 240,535 ms, and routine 1,920 at 291,308 ms; CPU negative/noise versus v43, no routine 1,984 or gen2. |
| `full-mir-consumer-branch-row-bundle-v45-build` | 2534.1 MB | 2522.6 MB | Exact-source integrated C driver compiled in 52,025 ms below the fixed cap. |
| `full-mir-consumer-branch-row-bundle-v45-300s` | 204.8 MB | 206.9 MB | Timed out at 300,345 ms after routine 704 at 161,510 ms, routine 896 at 189,756 ms, routine 1,600 at 238,576 ms, routine 1,920 at 288,324 ms, and the first routine 1,984 marker at 298,381 ms; no routine 2,048 or gen2. |
| `full-mir-consumer-phi-prefix-bundle-v46-build` | 2556.9 MB | 2546.0 MB | Exact-source integrated C driver compiled in 52,507 ms below the fixed cap. |
| `full-mir-consumer-phi-prefix-bundle-v46-300s` | 202.1 MB | 204.3 MB | Timed out at 300,163 ms after routine 704 at 163,937 ms, routine 896 at 193,024 ms, routine 1,600 at 242,500 ms, and routine 1,920 at 293,716 ms; CPU negative/noise versus v45, no routine 1,984/2,048 or gen2. |
| `full-mir-consumer-phi-prefix-admission-v47-build` | 2535.7 MB | 2524.3 MB | Exact-source integrated C driver compiled in 51,436 ms below the fixed cap. |
| `full-mir-consumer-phi-prefix-admission-v47-300s` | 207.7 MB | 209.7 MB | Timed out at 300,384 ms after routine 704 at 158,438 ms, routine 896 at 186,805 ms, routine 1,600 at 234,127 ms, routine 1,920 at 283,594 ms, and routine 1,984 at 293,201 ms; recovered v46 and improved on v45, no routine 2,048 or gen2. |
| `full-mir-consumer-branch-index-admission-v48-build` | 2567.8 MB | 2557.0 MB | Exact-source integrated C driver compiled in 51,479 ms below the fixed cap. |
| `full-mir-consumer-branch-index-admission-v48-300s` | 206.3 MB | 208.3 MB | Timed out at 300,615 ms after routine 704 at 158,817 ms, routine 896 at 187,672 ms, routine 1,600 at 235,166 ms, routine 1,920 at 285,333 ms, and routine 1,984 at 295,075 ms; CPU negative/noise versus v47, no routine 2,048 or gen2. |
| `full-mir-consumer-block-slice-admission-v49-build` | 2587.7 MB | 2578.1 MB | Rejected exact-source experiment compiled in 60,860 ms below the cap but materially slower than v48. |
| `full-mir-consumer-block-slice-admission-v49-300s` | 202.3 MB | 205.0 MB | Rejected experiment timed out at 300,269 ms after routine 704 at 166,252 ms, routine 896 at 194,769 ms, routine 1,600 at 243,264 ms, and routine 1,920 at 293,502 ms; 8,169 ms later than v48 and no routine 1,984/gen2. Reverted by `85cee4ff`. |
| `full-mir-consumer-resource-raw-capture-v50-build` | 2445.2 MB | 2438.9 MB | Rejected exact-source experiment compiled in 62,385 ms below the cap but 10,906 ms slower than v48. |
| `full-mir-consumer-resource-raw-capture-v50-300s` | 178.2 MB | 182.3 MB | Rejected experiment timed out at 300,680 ms after routine 704 at 189,951 ms, routine 896 at 222,884 ms, routine 1,600 at 279,085 ms, and routine 1,728 at 296,959 ms; no routine 1,792/2,048 or gen2. Reverted by `c5ee6e62`. |
| `full-mir-consumer-resource-local-scan-v51-build` | 2576.8 MB | 2565.8 MB | Rejected exact-source experiment compiled in 56,417 ms below the cap but 4,938 ms slower than v48. |
| `full-mir-consumer-resource-local-scan-v51-300s` | 192.6 MB | 195.6 MB | Rejected experiment timed out at 300,614 ms after routine 704 at 173,196 ms, routine 896 at 204,052 ms, routine 1,600 at 255,976 ms, routine 1,728 at 272,517 ms, and routine 1,792 at 287,519 ms; it lost v48's routine-1,984 marker and produced no gen2. Reverted by `6879f0c0`. |
| `full-mir-consumer-block-successor-pair-v52-build` | 2591.5 MB | 2580.9 MB | Rejected exact-source experiment compiled in 67,265 ms below the cap, 15,786 ms slower than v48. |
| `full-mir-consumer-block-successor-pair-v52-300s-observed` | 172.9 MB | 176.6 MB | Rejected experiment timed out at 300,560 ms after machine routine-index completion at 83,531 ms and routines 704/896/1,600/1,664 at 198,093/233,293/291,565/298,472 ms; no routine 1,728/2,048 or gen2. Reverted by `40037e52`. |
| `full-mir-consumer-llvm-performance-v53-build` | 2399.0 MB | 2389.0 MB | Accepted-source LLVM projection compiled successfully in 139,295 ms below the cap and preserved focused C/LLVM semantics. |
| `full-mir-consumer-llvm-performance-v53-300s-observed` | 214.0 MB | 210.8 MB | LLVM projection timed out at 300,518 ms after machine routine-index completion at 73,014 ms and routines 704/896/1,600/1,856 at 172,586/202,127/250,313/295,125 ms; it was slower than C v48 and produced no gen2. |
| `full-mir-consumer-c-clang-v54-build` | 2557.6 MB | 2546.5 MB | Accepted-source C projection compiled with the explicit Windows clang host toolchain in 42,649 ms, 8,830 ms faster than GCC v48, with byte/failure parity preserved. |
| `full-mir-consumer-c-clang-v54-300s-observed` | 206.0 MB | 208.0 MB | clang-built C projection timed out at 300,665 ms after routines 704/896/1,600/1,920/1,984 at 160,553/188,638/237,074/286,528/296,279 ms; build-time win but runtime negative/noise versus GCC v48, no gen2. |
| `full-mir-consumer-json-ascii-constants-v55-build` | 2516.9 MB | 2505.4 MB | Rejected exact-source experiment compiled in 51,536 ms; focused C/LLVM, bounded SHA, and wrong-ABI behavior remained exact. |
| `full-mir-consumer-json-ascii-constants-v55-300s-observed` | 202.9 MB | 205.3 MB | Rejected experiment timed out at 300,480 ms after routines 704/896/1,600/1,920 at 162,958/191,199/240,394/291,112 ms; 5,779 ms later than v48 at routine 1,920, no routine 1,984/gen2. Reverted by `1f77b0bc`. |
| `full-mir-consumer-match-owner-filter-v56-build` | 2587.0 MB | 2576.3 MB | Rejected exact-source experiment compiled in 69,158 ms; focused C/LLVM, component, bounded SHA, and wrong-ABI behavior remained exact. |
| `full-mir-consumer-match-owner-filter-v56-300s-observed` | 166.2 MB | 170.5 MB | Timed out at 300,772 ms after routine 1,408 at 296,916 ms. After adjacent-v48 MIR-start normalization it was 2,420/2,929/5,767 ms slower at routines 256/704/896; reverted by `c9e8011a`. |
| `full-mir-consumer-v48-current-control-300s-observed` | 174.2 MB | 177.9 MB | Adjacent unchanged-source control under the current load: MIR-to-AST start at 83,190 ms, routines 704/896/1,600/1,664 at 198,926/233,149/290,131/296,995 ms; no gen2. |
| `full-mir-consumer-match-routine-owner-v57-build` | 2588.3 MB | 2577.6 MB | Accepted exact-source C driver compiled in 56,640 ms; focused C/LLVM, component, bounded SHA, and wrong-ABI behavior passed. |
| `full-mir-consumer-match-routine-owner-v57-300s-observed` | 197.5 MB | 200.4 MB | Timed out at 300,609 ms after routines 704/896/1,600/1,664/1,728/1,792/1,856 at 172,807/202,276/251,736/258,128/267,628/281,858/296,651 ms. Normalized gains over adjacent v48 are 17,102/21,856/29,378/29,850 ms at 704/896/1,600/1,664; accepted, no gen2. |
| `full-mir-consumer-match-routine-owner-v57-adjacent-v58-control-300s-observed` | 177.5 MB | 181.1 MB | Adjacent accepted v57 control timed out at 300,250 ms; MIR-to-AST started at 80,208 ms and routines 256/704/896/1,600/1,664/1,728 completed at 104,993/191,418/224,809/280,783/287,747/298,614 ms; no gen2. |
| `full-mir-consumer-loop-branch-owner-v58-build` | 2587.9 MB | 2577.0 MB | Accepted exact-source C driver compiled in 60,952 ms below the fixed cap. |
| `full-mir-consumer-loop-branch-owner-v58-bounded` | 0.0 MB sampled | 0.0 MB sampled | Exit 0 in 1,688 ms; the process finished between 100 ms samples, output remained 414 bytes with the established SHA. |
| `full-mir-consumer-loop-branch-owner-v58-wrong-abi` | 0.0 MB sampled | 0.0 MB sampled | Exit 1 in 1,672 ms with the owned ABI diagnostic and no output; the process finished between samples. |
| `full-mir-consumer-loop-branch-owner-v58-300s-observed` | 197.3 MB | 200.0 MB | Timed out at 300,470 ms after routines 704/896/1,600/1,664/1,728/1,792/1,856 at 173,630/202,723/252,244/258,345/267,970/282,271/297,340 ms. Normalized gains over adjacent v57 are 13,115/17,413/23,866/24,729/25,971 ms through 1,728; accepted, no gen2. |
| `mir-lower-loop-branch-owner-v58-llvm-build` | 315.5 MB | 318.3 MB | Focused LLVM `mir_lower` compiled in 4,104 ms; C/LLVM valid output and invalid-ABI failure were byte-equal. |
| `full-mir-consumer-loop-branch-owner-v58-integration-completion` | 3072.1 MB | 2459.3 MB | Reached MIR-to-AST completion at 387,029 ms, then stopped on the unchanged memory limit at 1,059,616 ms inside expression graph construction; no output. |
| `full-mir-consumer-expression-arena-linear-v59-ready-proof-build` | 2590.1 MB | 2579.1 MB | Exact-source v59 driver compiled in 66,274 ms below the fixed cap. |
| `full-mir-consumer-expression-arena-linear-v59-ready-proof-bounded` | 0.0 MB sampled | 0.0 MB sampled | Exit 0 in 1,336 ms; 414 bytes with the established SHA. |
| `full-mir-consumer-expression-arena-linear-v59-ready-proof-wrong-abi` | 0.0 MB sampled | 0.0 MB sampled | Exit 1 in 486 ms with the owned ABI diagnostic and no output. |
| `full-mir-consumer-expression-arena-linear-v59-integration-completion` | 801.8 MB | 749.4 MB | Reached MIR-to-AST completion at 429,211 ms and failed closed at 1,645,538 ms on the positional graph/surface identity mismatch; no output and no memory-limit crossing. |
| `v59-expression-surface-count-probe-full` | 230.4 MB | 233.2 MB | Completed in 498,952 ms: 41,299 surfaces, 35,638 persisted-required lanes, and 1,758 parser-only lanes. Flat MIR contains only 34,962 roots. |
| `full-mir-consumer-structured-occurrence-v60-build` | 2480.3 MB | 2473.7 MB | Exact-source v60 C driver compiled in 69,368 ms below the fixed cap. |
| `full-mir-consumer-structured-occurrence-v60-observed-build` | 2575.8 MB | 2564.5 MB | Observed bootstrap driver compiled in 65,293 ms below the fixed cap. |
| `full-mir-consumer-structured-occurrence-v60-integration` | 1130.3 MB | 1041.1 MB | Expression graph done at 1,673,958 ms, semantic analysis done at 1,674,754 ms, then timed out at 1,800,768 ms during assignment body typing; no graph error, cap crossing, or gen2 output. |

The cursor run completed in 869,913 ms before the pressure owner stopped it
inside routine `SemanticExpressionGraphNodeKind`. `e5587bee` then removed the
complete production instruction and graph Strings. Its first fixed-cap run
completed all current 8,266 initializer rows and MIR facts, started JSON near
2,956 MB, and advanced to 40,263,680 bytes before escaped/quoted leaf results
crossed the cap at 810,472 ms.

`6329356f` moves only those file-boundary leaves into a call-local pool and
destroys it after synchronous `FileWrite`. The successor run exited 0 in
675,355 ms. Peak private was 3,064.3 MB, with `driver_oracle.exe` at
3,063.1 MB; two processes and no compiler/link subprocess were observed. The
artifact is valid `pgy.mir.v1` with 2,345 routines, 142 declarations, and
SHA-256
`1621adf4070bc778dd90493e29db857c22f13722d951bea8a94d1241e9ee884e`.
The full JSON parse and closing `]}` were observed. The production gate is
green, but its 7.7 MB sampled margin is narrow and does not close the broader
semantic/MIR live-state debt.

The consumer measurements are CPU failures, not memory failures. The first
cursor implementation called generated `strlen(json)` at least three times per
routine/block/instruction row, implying about 8.8 TB of avoidable length
walking before field reads. Exact-bound readers removed that debt and reached
`routine-index:done` for the first time. Allocation-free normal-key comparison
then completed the instruction scan, machine admission, and input boundary.
`157c340b` next removed about 2.45 TB of logical declaration-field walking and
at least 118.9 TB from the routine fact prefix. `d62553ee` captures routine
headers, instruction results, and instruction-local arrays once, then moves
structural-merge selection from worst-case O(B^3) candidate-local BFS to
O(B^2) branch-local BFS. The full artifact contains 20,022 blocks, 34,091
instructions, 3,532 phi rows, and 214,151 expression-graph nodes. Its first
top-level routine is only 2,063 bytes with one block/instruction, so the fixed
window is dominated by the admitted machine path and accumulated routine
work, not by that routine or memory.

`67502f50` closes another observed hidden length path. The 34,091 null
machine-layer tokens performed about 1.766 TB of whole-document length walking,
and the minimum kind/name routine decode added about 243 GB, because bounded
reads still materialized through native `Substring(json, ...)`. The common
JSON owner now uses the caller limit while materializing strings, and machine
null reads use `SubEqualsWithLen`. The unchanged 300-second run advanced from
the first routine to 16 routines at only 63.4 MB peak private. This remains
RED: no run opened a partial gen2 C artifact.

`190d0dbf` closes the next structural duplication. The admitted program view
captures 2,345 routine, 20,022 block, and 34,091 instruction spans once and
carries kind/source type plus machine contact/layer spans. Machine admission
and per-routine fact construction no longer rescan nested structure. Review
also found and removed a whole-program `StructureReady` call from every routine
builder; the component contract rejects its return. The v3 fixed-window run
still ended at the 16-routine marker, so the removed work was real but not the
dominant remaining cost. Routines 1-64 contain only 274,581 of 51,741,503
routine-object bytes (0.531%); neither marker is completion. Peak private was
85.2 MB, `limit_exceeded=false`, and no gen2 file was opened.

`06f6994d` closes the instruction-local repeat-scan seam reached by that run.
Merely changing fact-table accessors to `ref` did not improve the v9 timing.
Generated-C inspection corrected the earlier diagnosis: `String` is passed as
a `char *`, and `JsonObjectFactTable` stores that source pointer plus bounds;
it does not deep-copy 51.8 MB into every table. The real cost was repeatedly
revalidating the same instruction object and rediscovering fields/bounds from
the same 51.8 MB-backed source view. Exact-bound ABI/resource common paths
avoid those repeated object/table reads: the observed
instruction ABI step fell from 492 ms to 9 ms, the resource step from 646 ms to
0 ms, and routine 16 from 133,593 ms to 69,919 ms. The next real producer-wire
counterexample was `FindTopLevelComma`, whose loop header has seven CFG
predecessors but two incoming inventory values. The phi owner now preserves
that wire meaning and v11 passed it.

The v13 full-artifact run kept `output_capture_complete=true`,
`limit_exceeded=false`, and only 88.6 MB peak private while reaching routine 64
at 99,447 ms and routine 128 at 164,457 ms. This is a CPU bottleneck, not a
return of the 3 GiB memory defect. The final v14 driver build stayed below the
cap and its bounded output remained exactly 414 bytes with SHA-256
`0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`.
No run reached `consumer:mir-to-ast:done` or opened a complete
`driver_gen2.c`.

`dd68d6f3` closes the next measured routine-local seam. Each routine fact-index
construction now captures the render/result fields in one strict scalar pass,
while the admitted program index remains structure/identity-only. The active
MIR-to-AST reconstruction reuses that bundle, but the later expression-graph
and assignment post-passes still reconstruct a routine index and remain an
open re-entry seam. Phi context is computed lazily
only for blocks that actually contain a phi, and its incoming-backedge fact is
read from the canonical routine index instead of recomputing dominators. The
current v23 build completed in 47,746 ms below 3 GiB and preserved the exact
414-byte bounded SHA. Its 180-second run used 87.0 MB peak private / 95.3 MB
working set and moved routine 128 from the v14 300-second run's 165,019 ms to
160,331 ms. The improvement is real but modest; repeated scalar reads were not the
dominant remaining cost. `output_capture_complete=true`,
`limit_exceeded=false`, and no gen2 output was opened.

`a5d56f42` closes the required ABI-layout repeated-scan seam exposed by the
v29-v37 observation ladder. The v38 outer-bound-only experiment did not improve
the required row cost, proving the nested object/field validation and second
identity walk were dominant. The ABI owner now captures the nested row once,
validates at most eight fields, and hashes the captured values in canonical
semantic order. Raw instruction value spans remain location evidence, not a
second ABI authority. The producer compatibility entrypoint delegates to the
same captured identity owner, and component/ABI gates reject the deleted path.

The v39 300-second run used 134.7 MB peak private / 140.8 MB working set and
moved routine 192 from v38's 233,517 ms to 102,775 ms. It reached routine 640 at
298,374 ms, versus v38 ending near routine 248. The exact final-source v40
driver built in 55,007 ms below 3 GiB and preserved the exact 414-byte bounded
SHA. A bounded wrong-ID tuple exits 1 with the owned ABI diagnostic. This is
material executable progress but remains RED for bootstrap completion: no
`consumer:mir-to-ast:done` marker and no gen2 file exist.

`0da9c5c2` closes the identical-required-row revalidation seam without making
the 28-bit layout ID a cache authority. Before routine 640, 580 required rows
reduce to five complete tuples. The ABI owner remembers a tuple only after the
full order-independent capture and canonical hash succeed. Reordered JSON is a
safe miss and full revalidation; the same ID with a changed nested offset is a
miss and rejection. The focused C/LLVM fixture locks down both cases.

The exact-source v41 driver built in 52,722 ms at 2,346.8 MB peak private /
2,336.6 MB working set. Its 1,251 ms bounded result remains exactly 414 bytes,
and the wrong-ID input exits 1 without opening output. The full fixed-window
run moved routine 640 earlier by 69,919 ms (23.4%) relative to v39, passed the
old routine-704 falsifier, and reached routine 896 at 288,574 ms. It timed out
at 300,227 ms with 157.2/162.3 MB peak private/working set. This remains RED:
there is still no `consumer:mir-to-ast:done` marker or gen2 file.

`bf8a56b8` closes the duplicate optional ABI wire-read seam. The existing
routine scalar scan now carries type-value readiness, while the ABI owner keeps
the sole semantic decision and accepts the common optional case only with exact
raw `0`/`null` tokens. The v42 driver built in 53,265 ms below 3 GiB, preserved
the exact 414-byte bounded SHA, and rejected the wrong-ABI input in 551 ms with
no output. Its fixed-window run reached routine 704 at 162,849 ms, routine 896
at 192,157 ms, and routine 1,920 at 293,147 ms before timing out at 300,115 ms.
Peak private/working set was 214.4/216.6 MB. This is 76,035 ms and 96,417 ms
earlier at the shared 704/896 markers and 1,024 routines farther than v41, but
still RED for bootstrap completion: no `consumer:mir-to-ast:done` or gen2 file.

The v42 interval census covers all 29 completed 64-routine intervals. Interval
time versus instruction count has R-squared 97.43%; the remaining 425 routines
contain 7,873 instructions. The measured linear projection places
`top-level-routines:done` near process timestamp 355.9 seconds, before the still
unmeasured string join and AST inventory cost. That is a projection, not green
evidence and not permission to enlarge the 300-second diagnostic window. The
next measured CPU owner is `BuildMirRoutineFactIndex`: focused samples spend
1,051 of 1,464 ms (71.8%) in fact-index construction. Inside its scalar scan,
34,091 instruction objects expose 852,275 keys and currently trigger eleven
semantic key comparisons per key (9,375,025 calls). Dispatching plain keys by
their already-owned raw length reduces that to about 1,159,094 calls while an
escaped-key fallback preserves JSON equivalence and duplicate rejection. This
is the first minimal executable seam because it changes no fact owner, bundle,
or ABI decision. If linear cost remains after that, the broader candidate is
the second full instruction-object scan from
`BuildMirRoutineInstructionFactBundle` into
`MirRoutineInstructionScalarCaptureWithin`, after the admitted program index
already scanned every instruction for identity. A separate CFG census found
15,940 tail BFS calls but could not distinguish them from the strongly
collinear instruction/block volume; do not introduce a CFG cache or move phi
ownership on correlation alone.

`dfc8e406` executed the smaller falsifier first. Plain scalar keys now run only
their matching raw-length comparison group, while escaped keys retain the full
semantic fallback. The exact-source v43 driver built in 52,451 ms below 3 GiB,
preserved the 414-byte bounded SHA, and rejected the wrong-ABI input with no
output. The fixed-window run reached routine 1,920 at 290,054 ms, 3,093 ms
(1.06%) earlier than v42, then timed out at 300,268 ms without routine 1,984.
Peak private/working set was 215.1/217.1 MB. The comparison-count reduction is
real but not dominant. The next owner-directed move is inside the existing CFG
graph owner: compute the routine backedge result once, migrate the fact-index
consumer, and ratchet the per-edge dominator call. Keep structural merge and
phi unchanged for this slice.

`73133678` performs that owner migration and deletes the old edge-local
function; `ec4b9eef` proves the malformed result reaches an explicit consumer
failure. The static remaining-tail model reduces backedge BFS calls from 9,144
to 4,128, but the fixed-window v44 result is a CPU negative/noise observation.
The exact-source driver built in 52,316 ms below 3 GiB and preserved the bounded
SHA and wrong-ABI rejection. It reached routine 1,920 at 291,308 ms, 1,254 ms
(0.43%) later than v43, before timing out at 300,682 ms. Peak private/working
set was 202.7/205.0 MB. No routine 1,984, `mir-to-ast:done`, or gen2 file exists.
The single CFG owner and negative ratchet remain useful, but this evidence does
not authorize structural-merge or phi caching as the next CPU track.

`4ee29ce2` closes the next measured routine-lowering seam in the existing
routine-local fact bundle. Its scalar pass records one unique branch global row
per block, and condition, loop-transfer, and match-binding consumers no longer
reconstruct typed instruction views to search each block. The complete input
contains 20,022 blocks, 34,091 instructions, and 8,387 branch blocks; the three
mandatory searches removed at least 77,112 repeated view reconstructions.
Duplicate branches and forged row identity fail closed, and a component ratchet
forbids the old call in `routine_lower.pgy`.

The exact-source v45 driver built in 52,025 ms below 3 GiB, preserved the
414-byte bounded SHA, and rejected the wrong-ABI input without opening output.
The fixed-window run reached routine 1,920 at 288,324 ms and the first routine
1,984 marker at 298,381 ms before timing out at 300,345 ms. Peak
private/working set was 204.8/206.9 MB. That shared 1,920 marker is 2,984 ms
(1.02%) earlier than v44. This remains RED for bootstrap completion: no routine
2,048, `consumer:mir-to-ast:done`, or gen2 file exists.

`99e76e76` closes the remaining explicit whole-instruction phi scan. The
existing routine-local bundle records the leading phi count per block and a
late-phi invalid sentinel. The phi semantic owner reconstructs only those rows;
program-owned kind, predecessor, arity, result, incoming-use, and backedge
checks remain intact. The full artifact view count falls from 34,091 rows to
3,532, and the active 1,984-through-2,048 interval falls from 1,161 rows to 104.
The old all-instruction loop and fallback are statically rejected.

The exact-source v46 driver built in 52,507 ms below 3 GiB, preserved the
414-byte bounded SHA, and rejected the wrong-ABI input without opening output.
The fixed-window run reached routine 1,920 at 293,716 ms before timing out at
300,163 ms with 202.1/204.3 MB peak private/working set. That marker is 5,392
ms (1.87%) later than v45, and v46 did not recover v45's routine-1,984 marker.
This is a CPU negative/noise result rather than a speedup. The owner closure
remains, but the same revision must not be rerun for a favorable sample and the
window/cap must not be enlarged.

`a05aaf06` removes the v46 read-path regression at its exact boundary. The phi
owner admits program-row identity, block counts, and the routine-local bundle
once, reads block prefix counts directly, and rejects invalid counts. The
one-use accessor is deleted. This cuts full-artifact admission from 20,022
block calls to 2,345 routine calls, removing 17,677 admissions and at least
406,571 shape checks without adding a cache or global/local aggregate.

The exact-source v47 driver built in 51,436 ms below 3 GiB, preserved the
414-byte bounded SHA, and rejected the wrong-ABI input without opening output.
The fixed run reached routine 1,920 at 283,594 ms and routine 1,984 at 293,201
ms before timing out at 300,384 ms with 207.7/209.7 MB peak private/working
set. Routine 1,920 is 10,122 ms earlier than v46 and 4,730 ms earlier than v45;
routine 1,984 is 5,180 ms earlier than v45. This is measured CPU progress, but
routine 2,048, `consumer:mir-to-ast:done`, and gen2 output remain absent.

`8074d6c8` moves branch selection from the bundle accessor to the admitted
routine fact index. The branch row stays in the existing bundle, while the new
boundary checks routine/block identity, local/global range, scalar span, and
final program-owned kind. The old accessor is deleted and all three consumers
use the index owner. The full validation-loop lower bound removes 21,910 full
admissions and at least 503,930 shape checks without adding a cache or
aggregate.

The exact-source v48 driver built in 51,479 ms below 3 GiB, preserved the
414-byte bounded SHA, and rejected the wrong-ABI input without opening output.
The fixed run reached routine 1,920 at 285,333 ms and routine 1,984 at 295,075
ms before timing out at 300,615 ms with 206.3/208.3 MB peak private/working
set. Those markers are 1,739 and 1,874 ms later than v47. This is an
owner/fallback closure and CPU negative/noise result, not a speedup. Routine
2,048, `consumer:mir-to-ast:done`, and gen2 output remain absent.

`80a54268` tested the next larger static candidate by replacing
`EmitBlockStmts`' three checked accessors with one block-boundary guard and
direct instruction/scalar construction. Its C/LLVM cross-block negative and
component ratchet passed, but generated-code cost dominated the eliminated
shape checks. The driver build regressed from v48's 51,479 ms to 60,860 ms.
The full run reached routine 1,920 at 293,502 ms, 8,169 ms later than v48, and
lost routine 1,984 before timing out at 300,269 ms. Peak private/working set was
only 202.3/205.0 MB, so this was a CPU/code-shape regression, not memory.

`85cee4ff` reverts that experiment. `git diff 7dd78069..85cee4ff` is empty, so
the v49 revert restored byte-for-byte v48 source while the failed attempt
remains auditable in history. Do not reintroduce the same direct block
aggregate construction or equate lower static check count with lower
generated-program cost.

`530682af` then moved resource runtime ABI top-field capture into every routine
instruction scalar and bundle row. The focused C/LLVM and bounded gates were
green, but the driver build regressed to 62,385 ms. The full run reached only
routine 1,728 at 296,959 ms and timed out at 300,680 ms with 178.2/182.3 MB peak
private/working set. Even the machine routine-index marker moved from v48's
67,567 ms to 80,353 ms, so the regression is broader generated-program cost,
not resource-row validation alone or memory pressure. `c5ee6e62` reverts the
carrier experiment. `5e12cf43` keeps only the independently found correctness
ratchet: a non-resource instruction carrying a stray runtime ABI value now
fails closed, with current-source C/LLVM negatives and the component contract
green.

The focused instruction-writer gate now compares raw, unnormalized
String/file bytes for five small, graph-heavy, match, destructure, and
ABI/optional fixtures through both C and LLVM, then compares C/LLVM file bytes.
It also corrupts instruction row count and proves the sentinel output is not
opened or truncated. The earlier 11,262-byte small fixture SHA remains
`007d5dacdd8157a0d5dd0f87975f82c7abe2fa4987983afb3945bd61b29efc09`.
`FileOpen` failure is observable and fails closed; the current runtime does not
return a `FileWrite` status, so the writer must not claim write-error detection
that the runtime cannot provide.

Broad runs remain explicit RED evidence. `mir_machine_layer_smoke.sh` reaches
the MIR consumer and then fails at the existing `local declaration is missing
its MIR ABI type fact`. `mir_json_parity.sh` expects an enum variant substring
without the current `param_types:[]` field. A filtered `dir_walk` /
`break_after_stmt` attempt stops earlier because reconstructed C lacks current
`PGY_RUNTIME_PANIC` declarations. Update those owners only when their
executable slice is active; none is a green CFG/runtime verdict.
The current focused DRV-2 body attempt also stopped while compiling
`valid_array_builtins` because emitted C omitted `<string.h>` and runtime panic
declarations. The separately isolated `nested_if_in_loop` current-driver run
is green, and a forged one-predecessor header phi is rejected with
`MIR phi facts are missing or inconsistent`; this does not relabel the broad
body gate green.

## Historical observed gates through v60

Green on implementation checkpoint `3418b0f3` plus the retained predecessor
measurements:

- `tests/self_hosted_component_contract_smoke.sh`;
- `tests/self_hosted/parity/driver_rung2_structured_expression_order_owner.sh`;
- `tests/self_host_program_graph_unification_smoke.sh` with
  `phase=unified structural_owners=1`;
- focused native/self `forloop` `mir_json_parity.sh`: range loop-init graph is
  start `0`, range branch graph is stop `3`, and a start-graph regression is
  rejected;
- v60 exact-source and observed driver builds: exit 0 in 69,368/65,293 ms at
  2,480.3/2,575.8 MB peak private;
- v60 bounded consumer: exit 0, 414 LF-normalized bytes, established SHA;
- v60 wrong-ABI and missing/invalid graph mutations: exit 1 with owned
  diagnostics and no output;
- v60 full integration: graph and semantic completion observed before the
  1,800-second timeout in assignment body typing, with 1,130.3/1,041.1 MB peak
  private/working set and no memory-limit crossing;
- `tests/self_hosted/parity/json_bounded_string_owner_smoke.sh` (C/LLVM,
  plain, escaped, empty, and truncated exact-bound strings);
- `tests/self_hosted/parity/mir_program_routine_index_owner_smoke.sh` (C/LLVM,
  partitions, direct-field spans, malformed scalar tails, missing structure,
  corrupted counts, invalid row guards, explicit negative CFG successor
  rejection, missing/unique/duplicate/forged/out-of-block branch-row facts, and
  leading/late/truncated phi-prefix facts, plus invalid match owners,
  zero-block parallel-array misalignment, wrong-kind match rows, match
  name/type count mismatch, forged non-match local arrays, malformed FOR
  scalar rows, invalid branch sentinel, same-endpoint scalar range, and
  no-branch block-span mutation);
- `tests/self_hosted/parity/mir_cfg_graph_query_owner_smoke.sh` (C/LLVM,
  diamond, re-entry, unrestricted-ranking, self-loop, tie, fallback, and
  detached-component witnesses);
- `tests/self_hosted/parity/driver_rung2_mir_abi_layout_negative_owner.sh`;
- `tests/abi_ownership_shape_smoke.sh`;
- `tests/protocol_registry_smoke.sh`;
- `tests/gate_sot_single_owner_smoke.sh`;
- integrated `driver_bootstrap_main.pgy` C build under the 3072 MB pressure
  owner (`full-mir-consumer-loop-branch-owner-v58-build`): exit 0, 60,952 ms,
  2,587.9 MB peak private / 2,577.0 MB peak working set;
- v59 readiness-proof integrated C build: exit 0, 66,274 ms, 2,590.1 MB peak
  private / 2,579.1 MB peak working set;
- v59 bounded MIR consumer: exit 0 in 1,336 ms, 414 bytes, established SHA;
- v59 wrong-ABI mutation: exit 1 in 486 ms with the owned diagnostic and no
  output;
- v59 full completion attempt: MIR-to-AST done at 429,211 ms, fail-closed at
  1,645,538 ms, 801.8/749.4 MB peak private/working set, no gen2;
- v59 surface-count probe: 41,299 surfaces, 35,638 persisted-required lanes,
  1,758 parser-only lanes, proving the flat-root count mismatch;
- bounded MIR consumer byte check: 414 bytes, SHA-256
  `0e32ec703f3b1237fc8c147bd8f395d89a53106d649f3e8f1ab4c608fc0ff25b`;
- bounded wrong-ABI mutation: exit 1 with the owned ABI diagnostic and no
  output file;
- focused current-source resource runtime ABI negatives through C- and
  LLVM-built drivers, including missing/identity/payload/aux rows and a stray
  wrong-kind row on a non-resource instruction;
- `tests/build_pressure_contract_smoke.sh`;
- focused current-driver `nested_if_in_loop` MIR production/consumption plus a
  forged one-predecessor header-phi rejection;
- `tests/self_hosted/parity/module_manifest_resolver_parity.sh` (C/LLVM,
  clean plus malformed/missing manifest negatives);
- `tests/self_hosted/parity/air_graph_json_validator_parity.sh` (C/LLVM,
  clean, missing-key, and live-drift negatives);
- `tests/self_hosted/parity/mir_json_instruction_writer_byte_parity.sh`
  (C/LLVM, five raw String/file and cross-backend fixtures, plus invalid
  pre-open sentinel rejection);
- `instruction-string-pool-ready` pressure shard: exit 0, complete JSON below
  3072 MB;
- `tests/self_hosted/parity/semantic_initializer_environment_cursor_owner_smoke.sh`;
- `tests/self_hosted/parity/semantic_expression_environment_owned_lifetime_smoke.sh`;
- `tests/self_hosted/parity/initializer_projection_probe_parity.sh` (C/LLVM,
  including shadow/exit/destructure positives and self/sibling negatives);
- `tests/self_hosted/parity/driver_rung2_iteration_graph_use_owner.sh`;
- `python scripts/protocol_registry_gate.py`:
  `7 protocol rows valid; no authority duplicated`;
- `python scripts/sot_registry_gate.py`:
  `49 authorities, 41 derived fact carriers; CLOSED=29 BRIDGE=20 ACTIVE=0`;
- `git diff --check` and `git diff --cached --check`.

`tests/self_host_hard_contract_smoke.sh` remains RED at the unrelated existing
manifest assertion that `driver_rung2_owner.pgy` contain
`tests/cases/backend_compare/device_slot_machine_layer/main.pgy`. This was not
weakened or relabeled as success.

The shell gates must use `C:\Program Files\Git\bin\bash.exe` in the current
Windows environment. `C:\Windows\System32\bash.exe` resolves to WSL and fails
because `/bin/bash` is unavailable; that is an execution-environment failure,
not a project gate result.

## Historical temporary artifacts through v60

The ignored temporary tree is diagnostic evidence, not semantic authority.
During the 2026-07-28 cleanup, a command intended for exact probe binaries
traversed ignored `.tmp` paths and removed a broader set of ignored diagnostics.
No tracked file or protected dirty file was touched, but the historical
`.tmp/instruction_writer_pressure/driver_source_pool.mir.json` (formerly
51,807,108 bytes), `.tmp/driver_rung2_topology.exe`, and
`.tmp/native_zone_topology.c` are no longer present and are not recoverable from
Git. Any resumed v60 pressure run must regenerate and hash-check the exact full
MIR from its owner before use; the 40,263,680-byte RED partial must never be
substituted. The names below are historical references and their existence must
be checked rather than assumed. Pressure evidence was recorded under
`.tmp/build-pressure/instruction-stream-ready.*` and
`.tmp/build-pressure/instruction-string-pool-ready.*`. Consumer progression is
captured by `full-mir-consumer-admitted.*`,
`full-mir-consumer-exact-bound.*`,
`full-mir-consumer-machine-twofield.*`,
`full-mir-consumer-key-compare.*`, `full-mir-consumer-exact-span.*`, and
`full-mir-consumer-routine-fact-exact.*`,
`full-mir-consumer-routine-indexed.*`, and
`full-mir-consumer-cfg-owner.*`, and
`full-mir-consumer-document-index.*`, and
`full-mir-consumer-program-instruction-index-v3.*`,
`full-mir-consumer-int-cfg-v14-300s.*`, and
`full-mir-consumer-routine-scalar-bundle-v23.*`,
`full-mir-consumer-abi-bounds-v38-300s.*`, and
`full-mir-consumer-abi-row-capture-v39-300s.*`, and
`full-mir-consumer-abi-exact-reuse-v41-300s.*`,
`full-mir-consumer-abi-optional-fast-v42-300s.*`, and
`full-mir-consumer-key-dispatch-v43-300s.*`, and
`full-mir-consumer-cfg-backedge-batch-v44-300s.*`, and
`full-mir-consumer-branch-row-bundle-v45-300s.*`, and
`full-mir-consumer-phi-prefix-bundle-v46-300s.*`, and
`full-mir-consumer-phi-prefix-admission-v47-300s.*`, and
`full-mir-consumer-branch-index-admission-v48-300s.*`. The rejected/reverted
v49 evidence remains under
`full-mir-consumer-block-slice-admission-v49-300s.*`. The rejected/reverted v50
evidence remains under
`full-mir-consumer-resource-raw-capture-v50-300s.*`. The rejected/reverted v51
evidence remains under
`full-mir-consumer-resource-local-scan-v51-300s.*`. The rejected/reverted v52
successor-pair evidence remains under
`full-mir-consumer-block-successor-pair-v52-{build,bounded,wrong-abi,300s,300s-observed}.*`;
only the `300s-observed` run has valid routine-marker evidence. The v53 LLVM
projection evidence remains under
`full-mir-consumer-llvm-performance-v53-{build,bounded,wrong-abi,300s-observed}.*`.
The v54 explicit clang-via-C evidence remains under
`full-mir-consumer-c-clang-v54-{build,bounded,wrong-abi,300s-observed}.*`.
The rejected v55 local-call evidence remains under
`full-mir-consumer-json-ascii-constants-v55-{build,bounded,wrong-abi,300s-observed}.*`.
The rejected v56 evidence remains under
`full-mir-consumer-match-owner-filter-v56-{build,bounded,wrong-abi,300s-observed}.*`;
its adjacent unchanged-source control is
`full-mir-consumer-v48-current-control-300s-observed.*`. The accepted v57
evidence remains under
`full-mir-consumer-match-routine-owner-v57-{build,bounded,wrong-abi,300s-observed}.*`.
The adjacent v57 control for v58 is
`full-mir-consumer-match-routine-owner-v57-adjacent-v58-control-300s-observed.*`.
The accepted v58 evidence is
`full-mir-consumer-loop-branch-owner-v58-{build,bounded,wrong-abi,300s-observed}.*`;
its focused LLVM build is
`mir-lower-loop-branch-owner-v58-llvm-build.*`.
The first completion continuation is
`full-mir-consumer-loop-branch-owner-v58-integration-completion.*`; it reached
expression graph construction and stopped at the 3,072 MB cap. v59 evidence is
`full-mir-consumer-expression-arena-linear-v59-{integration-completion}.*` and
`full-mir-consumer-expression-arena-linear-v59-ready-proof-{build,bounded,wrong-abi}.*`.
v60 evidence is
`full-mir-consumer-structured-occurrence-v60-{build,observed-build,integration}.*`.
The current diagnostic executables are
`.tmp/self_hosted/driver_bootstrap/driver_rung2_v60_structured_occurrence.exe`
and
`.tmp/self_hosted/driver_bootstrap/driver_bootstrap_v60_structured_occurrence.exe`.
The temporary count
probe source/executable were deleted after their result was recorded; its
pressure evidence remains under `v59-expression-surface-count-probe-full.*`.
The latest full consumer evidence passes the former 35,638-vs-34,962
positional mismatch, completes graph construction and semantic analysis below
1,131 MB private, and times out at assignment body typing. The requested
`v60_full.c` does not exist because output is committed only after verified
completion. The rejected v50
executable is
`.tmp/self_hosted/driver_bootstrap/driver_rung2_v50_resource_raw_capture.exe`;
its 414-byte bounded result is
`.tmp/self_hosted/driver_bootstrap/v50_bounded.c`. These files are diagnostic
evidence only, not semantic authority or commit content.

## Historical v60 next executable work

1. The resource ABI and block-successor pair read seams are abandoned. Their
   focused correctness gates passed, but their carrier/local-scan/pair shapes
   materially regressed generated-driver CPU. Do not try another representation
   of either read consolidation.
2. The accepted-source LLVM v53 projection is connected and semantically
   byte-equal, but it is slower than C v48 and reaches only routine 1,856 in the
   fixed window. Keep LLVM's general performance-primary direction, but do not
   use the current LLVM-built DRV-2 as the active bootstrap executable and do
   not change semantics to make that positioning claim pass.
3. The explicit clang-via-C v54 projection improves integrated driver build
   time but is runtime negative/noise against GCC v48 and produces no gen2.
   Keep the existing Windows GCC-first default and do not confuse host compile
   speed with generated compiler progress.
4. The v55 JSON ASCII experiment removed the expected generated calls, but
   routine 1,920 regressed by 5,779 ms and routine 1,984 was lost. It is
   reverted. Do not retry literal constants, a shared ASCII helper, backend
   intrinsics, or unchecked character access; the static call-count hypothesis
   did not identify an integrated dominant cost.
5. The v56 match-local filter is reverted because its extra alignment pass
   regressed adjacent-v48 normalized markers. Accepted v57 directly consumes
   the routine-index owner; accepted v58 then consumes each loop-projection
   branch row once and improves every adjacent-v57 normalized marker through
   routine 1,728. Keep both closed shapes; do not add a third match-local read,
   a second branch pass, or rendered-condition fallback. v58 still produces no
   gen2, so count it as owner closure and generated-driver CPU improvement, not
   hard substitution progress or completion.
6. v60 closes the structured graph occurrence seam. Keep its repeated-key
   semantics, one final arena, producer coverage, deleted sequence view, and
   native range-stop producer ratchet. Do not reopen raw positional pairing,
   text lookup, deduplication, or a second graph/order.
7. The active seam is `SemanticAstAssignmentTypeFactsFromArtifact`, entered at
   `semantic-body-type-stage assignment:start` after graph and semantic
   completion. Add narrow stage/row evidence only as needed to locate repeated
   owned work; do not start a broad assignment fixture campaign.
8. Rerun the same complete artifact under the unchanged 1,800-second / 3,072 MB
   pressure gate. Acceptance for this slice is `assignment:done`, or one exact
   assignment row, owner read, and falsifying case if it still cannot finish.
9. Continue the same run through statement/body verification. If it emits a
   complete `driver_gen2.c`, compile that C as the bootstrap object-code
   boundary; do not regenerate another oracle MIR.
10. Make the generated gen2 driver consume the same complete compiler source
   and emit `driver_gen3.c`. Do not divert into global SoT closure or fixture
   expansion; close only a concrete owner seam that blocks this exact run.
11. Compare complete gen2/gen3 artifacts and behavior. Use the existing bounded
   MIR fixture only as a focused falsifier when diagnosing a failure on this
   path, not as an independent breadth campaign.
12. Keep the separate foreach assignment-binding, ABI-type, stale enum-parity,
   and reconstructed-runtime-header failures out of this active CPU seam. Do
   not raise the fixed integration time or memory limits as a substitute for
   closing the owner path.

## Historical v60 resume sequence

1. Read this file, `src/self_hosted/PROGRESS.md`, `src/self_hosted/OWNERS.md`,
   `docs/180_compiler_logical_spine_handles_gates.md`, and
   `docs/semantics/sot_owner_spine_registry.md`.
2. Verify HEAD/origin, `git status --short --branch`, and the three protected
   dirty files above.
3. Re-run the component, structured-expression-order, program-graph, and
   focused native `forloop` MIR parity gates through Git Bash before a broad
   build.
4. Confirm no unrelated `pgy`, `genN`, `driver_oracle`, `gcc`, `cc1`, or
   `clang` process is active before the pressure gate; concurrent broad builds
   invalidate attribution.
5. Regenerate and hash-check the exact v60 full MIR first; the former frozen
   temporary artifact was removed in the ignored-temp cleanup. Then continue
   the v60 executable under the fixed 1,800-second/3,072 MB pressure gate; the
   first required marker is `semantic-body-type-stage assignment:done`.
6. Treat current source, registries, and executable gates as authoritative if
   this snapshot disagrees with them.
