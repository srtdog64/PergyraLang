# Current Work Collaboration Ledger

Updated: 2026-08-26 (Asia/Seoul)

This file coordinates concurrent Codex work. It is not semantic authority and
does not prove completion. Current source, the SoT registries, executable gates,
and `docs/current_work_handoff.md` remain authoritative in that order.

## DONE lease O — structured MatchCase carrier closure

- Source base: exact published `3726f14c90ce93e0bf1fb07389b16d2138bc4140`;
  replacement run `32961756130` is green 29/29 at that revision. The unrelated
  untracked `docs/compiler_architectures/` and `pgy-80135c2c/` paths are outside
  this lease and must remain untouched.
- Objective: admit each typed MatchCase atom once through the existing HIR
  `AstMatchCasePatternFact` owner, carry canonical pattern/variant/binding rows
  in `SemanticAstStatementFacts`, and make semantic use sites, MIR lowering,
  and self-C codegen consume that same SyntaxNodeId-keyed structure.
- Priority: stable SyntaxNodeId join; one owner admission; fail-closed carrier
  validation; semantic consumer migration; MIR consumer migration; self-C
  consumer migration; delete old reads; negative ratchet; then patch size.
- Fact owner: `AstMatchCasePatternFactFromArtifact` and its admitted
  `AstMatchCasePatternFactFromReadyArtifact` boundary in
  `ast_match_pattern_fact_owner.pgy`. `SemanticAstStatementFacts` is a carrier,
  not a second syntax authority.
- Production entrypoints: installed source-to-MIR/source-to-C through
  `driver_bootstrap_main` and `CompileSourceToCVerified`, plus the bounded
  self-host codegen `main --source` path. Last consumers are
  `SemanticAstExpressionSeedMatchCaseBindings`,
  `SelfMirMatchCaseFactForNode`, and the Option/tagged match emit/bind owners.
- Forbidden: `AstMatchCasePatternFactFromText` outside its HIR owner;
  `AstMatchCasePatternFactFromReadyArtifact` outside statement-fact admission;
  MIR or codegen parsing `payload_texts`; ordinal-only joins; invalid offset,
  count, variant, or binding rows succeeding; any String compatibility overload
  or structured-invalid-to-text fallback.
- Integration boundary: reverse stale component/lifetime assertions that
  require consumer reparsing; run semantic selfcheck, focused Option/tagged
  installed source-to-MIR parity, and bounded self-host source-C Option/tagged
  execution. Missing/wrong-kind/cross-wired binding ranges must fail closed.
- Only the primary task owns implementation and publication for this rung.
  Parallel agents completed read-only consumer/gate audits; no parallel edit
  scope is open.

### Result

- Implementation checkpoint `aafcadbd` admits the HIR pattern exactly once in
  `SemanticAstStatementFacts` and carries canonical pattern, variant, flat
  binding range/pool, and a mutation digest on the existing SyntaxNodeId row.
  Semantic environments, MIR, and self-C Option/tagged consumers now borrow
  that structure; `SelfMirMatchCaseFactFromText` and the raw codegen String
  accessor are deleted.
- Source inventory now finds `AstMatchCasePatternFactFromText` only inside its
  HIR owner and `AstMatchCasePatternFactFromReadyArtifact` only in that owner
  plus statement admission. The component ratchet scans every self-hosted Pergyra
  source and rejects either old read outside those boundaries.
- The statement contract compiled and executed with missing/wrong-kind plus
  changed variant, binding, and crossed range negatives. Both semantic and MIR
  lifetime gates pass. A freshly built self-C tool executes `enum_match`,
  `enum_multi_payload`, and `option_enum_with_payload` exactly as expected; a
  fresh isolated DRV-2 passes canonical source-MIR parity for those three plus
  `option_match`.
- The complete component inventory is not claimed green: after all modified
  owner caps and task-local structural assertions passed, it entered the
  broader source-MIR execution action and exceeded the local focused budget.
  No timeout or cap was raised.
- `selfhost.match_case_pattern` is now `CLOSED`; the registry census is
  `CLOSED=50 BRIDGE=35 ACTIVE=1`. Integrated progress remains 83% (81-85%),
  strict beta 83%, and hard replacement 75%. Publication and the replacement
  push matrix are the next action; no successor implementation lease is open.

## DONE lease N — carried ABI CI repair and replacement run

- Source base: published implementation `301309f9`; focused repair checkpoint
  `0a6a69b1` is local and awaiting publication with this ledger refresh.
- Objective: restore the push matrix without weakening the carried
  RuntimeCallAbiId owner chain. The only implementation-rung edit scope is the
  two direct transpiler negatives and the stale runtime/perf static assertions.
- Observed failure: Linux, Windows, and macOS in run `32960178361` all reached
  the same two direct C-backend tests without semantic admission and therefore
  correctly failed earlier on missing carried ABI identity.
- Forbidden: backend source-name lookup, implicit ABI admission in codegen,
  changing error precedence for production calls, staging either unrelated
  untracked path, or opening the match-pattern implementation before the
  replacement matrix is green.
- Local integration gate: `test-transpile` 925/0 plus the ABI registry, runtime
  intent-observability, and perf contracts are green. Publication and the
  replacement push run remain the active work.
- The read-only successor audit is complete. If this lease closes green, the
  primary task may open one new lease for `selfhost.match_case_pattern`; no
  parallel implementation is authorized.

### Result

- Repair `0a6a69b1` and handoff checkpoint `3726f14c` are published on `main`.
  The two direct backend arity negatives now carry their stable admitted ABI
  IDs, while runtime/perf gates reject the retired backend source-name lookup.
- Replacement run `32961756130` passed 29/29. Linux, Windows, and macOS all
  passed the exact test surface that failed in `32960178361`; Rocq, sanitizers,
  TSan, full self-host, codegen bootstrap, toolchain, and all 20 shards are
  green. No fallback or product behavior changed.

## DONE lease M — native intent-observability ABI-ID consumption

- Source base: `464a907a010b745c3ec1bdaecf783bbf9e31c037`. Published audit
  checkpoints `41a01815` and `acf6c94f` are documentation only; current
  artifact and exact-revision remote evidence correct their stale successor
  before any implementation edit.
- Objective: make the explicit native C and LLVM observability emitters consume
  the semantic-admitted `RuntimeCallAbiId` instead of reconstructing the ABI
  row from source spelling.
- Priority order: source-name admission once in semantics; stable ID carriage;
  row-by-ID consumption in both backends; source/ID mismatch rejection; old-
  lookup ratchet; then patch size.
- Fact owner: `PGY_INTENT_OBSERVABILITY_ABI_ROWS_OWNER` in
  `src/common/intent_observability_abi.def`. Semantic admission may resolve a
  source spelling to that row once; the AST call is only a stable carrier.
- Production entrypoint and last consumers: public explicit
  `pgy --native-pipeline --backend=c|llvm` on
  `intent_observability_history_count.pgy`; the last consumers are
  `emit_builtin_intent_observability` and
  `llvm_emit_intent_observability_call`.
- Direct bypass to delete: both consumers call
  `pgy_intent_observability_abi_row_by_source` even though installed self-host
  C/LLVM already execute from carried ABI IDs.
- Forbidden: sorted-row-index identity, backend source-name lookup, zero/default
  ID success, source/ID mismatch, a second ABI table, or native/installed retry.
- Integration gate: keep installed/native C/LLVM exact runtime parity in
  `intent_observability_installed_self_host_owner.sh`; preserve the existing
  carried-ID missing/mismatch/forged/syntax-conflict negatives; add a native
  carrier probe and a static rejection of the two backend source lookups.
- Classification: bounded SoT consumer substitution. It does not close the
  wider compiler-purpose intent obligation or change progress by itself.

### Result

- Semantic admission records the owner row's stable `RuntimeCallAbiId` on the
  AST call once. The native C and LLVM emitters consume that carrier through
  one ID lookup plus source/ID cross-seal; neither emitter calls the source-name
  lookup anymore.
- ID zero, unknown IDs, and source/ID mismatch fail closed in the registry
  probe. The static gate rejects either backend source lookup, and installed
  plus explicit-native C/LLVM execution remains byte-equal for the focused
  history-count fixture.
- `make -j2 pgy`, `intent_observability_abi_registry_smoke.sh`, and
  `intent_observability_installed_self_host_owner.sh` are local green. Remote
  CI is pending publication. The row remains `BRIDGE` because the wider
  compiler-purpose root intent is still open; all progress counts are unchanged.

## DONE lease L — stale fixed-MIR successor rejected before implementation

- The 48,531,749-byte routine-1197/global-row-18392 failure came from an older
  frontier. Current canonical evidence is a 236,684,385-byte MIR that emits
  byte-equal 10,464,651-byte gen2/gen3 C; remote run `32949495441` also passes
  the exact-revision full self-host gate.
- No implementation, test, timeout, cache, registry status, or progress change
  was made under lease L. The audit row moved from `READY_NEXT` to
  `EVIDENCE_GAP`; a fresh reached remaining consumer is required before it can
  be selected again.

## DONE lease K — nonclosed SoT dependency census

- Base revision: `464a907a010b745c3ec1bdaecf783bbf9e31c037`.
- Objective: account for all 37 current `BRIDGE|ACTIVE` registry rows and
  produce one dependency-ordered closure map before selecting another
  executable implementation rung.
- Fact owners remain the registry rows and current source. Three agents own
  disjoint read-only report files under `docs/audits/`; the primary task alone
  owns set-equality integration and successor selection.
- Forbidden: parallel implementation, registry/status/progress edits by audit
  agents, closure inferred from tests or filenames, duplicate row assignment,
  or selecting a successor before all dependency edges are reconciled.
- Integration gate: the exact union of report owner IDs must equal the current
  registry `BRIDGE|ACTIVE` set with no duplicates. The nonnumbered directive is
  `docs/agent_work_directives/sot_closure_dependency_map_2026-08-26.md`.

### Result

- All three disjoint reports are complete. Their table union is exactly the
  current 37-row registry set: 37 expected, 37 observed, 37 unique, with no
  missing, extra, or duplicate owner ID.
- The corrected dependency map classifies two rows `READY_NEXT`, 24
  `DEPENDENCY_BLOCKED`, and eleven `EVIDENCE_GAP`; no row is yet only a
  `PRODUCT_BOUNDARY`.
- Primary integration rejected the stale semantic-artifact successor before
  implementation and selected only `abi.intent_observability_rows`: its two
  native backend source-name lookups are live, while carried-ID installed
  C/LLVM execution and negative evidence already exist.
- This audit changed no compiler source, owner status, or progress percentage.
  It prevents parallel SoT implementation from turning dependencies into dual
  authority.

## DONE lease J — installed source-C machine declaration carriage

- Base revision: `5e946a5a2165c784f7028f967f08ae3a1b2aaa1c`.
- Editing and integration owner: the primary Codex task. No parallel
  implementation track is open.
- Objective: make public installed source-to-C carry the already installed
  machine-layer companion into the existing typed Pergyra source-C request so
  a real `DeviceSlot<Int>` program reaches its existing machine projection and
  codegen owners without native re-entry.
- Priority order: one installed physical declaration; typed request carriage;
  source-MIR instruction projection; fail closed on missing/corrupt evidence;
  negative old-path ratchet; then patch size and output familiarity.
- Fact owner: the installed sibling's immutable
  `.machine-layer-manifest.json`, admitted only by
  `SelfHostMachineLayerDeclarationFromPath` and
  `SourceCManifestVerified`. The launcher owns path carriage, not manifest
  contents or a second host-sim grant table.
- Production entrypoint and last consumer: public `pgy SOURCE --emit-c` and
  plain source compile enter `driver_materialize_self_host_c_artifact`; the
  source-to-MIR instruction projection is the last consumer before existing
  MIR validation and C emission.
- Observed RED: explicit native `--emit-c` succeeds for
  `tests/cases/backend_compare/device_slot_machine_layer/main.pgy`, while the
  installed path exits 1 with `instruction=0 machine-layer projection is
  invalid`. The artifact-mode child receives `SourceCDefault`, so the known
  `ClaimDeviceSlot` contact meets an empty declaration and fails closed.
- Forbidden fallback: repeating machine manifest/grant literals in C or
  Pergyra, scanning source text for `DeviceSlot`, treating a nonempty machine
  contact as an empty row, retrying `driver_run_pipeline`, accepting a missing
  or malformed companion, or opening the broader Channel/product-tool surface.
- Verification and falsifier: real public DeviceSlot C emission and execution
  must match the explicit-native oracle; a counting child is invoked exactly
  once with the companion operand; missing and corrupt companions publish no C
  artifact and show no native timing; static gates reject a public artifact
  child request that omits `--machine-manifest-json`. Reuse the existing
  installed-driver Make target and CI job.

### Result

- The C adapter now derives the installed sibling manifest path and passes it
  through the typed three-field source-C artifact request. The installed
  composition root admits it as `SourceCManifestVerified` and carries that
  request through the existing `PgyCompilerWorld` source-C action; no grant or
  machine fact was duplicated.
- Extending declaration carriage exposed an older usage mismatch: a valid
  declaration caused a startup call in programs without machine operations,
  while the matching definition block was usage-gated. Both block and call are
  now controlled by the same `usage.uses_machine_layer` fact. Non-machine
  source/MIR emits no startup call; DeviceSlot source/MIR emits both.
- A fresh typed-source DRV-2 install is green. The focused DeviceSlot gate,
  general source-C action/default emit gates, and the full installed-driver CLI
  parent are green. Missing and corrupt installed companions fail without an
  artifact or native timing. The full component inventory is not claimed: its
  earlier run reached the static 60-second budget and found only the then-fixed
  script line cap; exact owner/cap ratchets are covered by the focused gates.
- Implementation checkpoint `10055d0b` and SoT-gate repair `464a907a` are on
  local and remote `main`. Replacement run `32949495441` passed 29/29; it
  includes Linux structural/SoT edge, full self-host, Windows, sanitizers,
  Rocq, codegen bootstrap, macOS, TSan, and all 20 backend shards.

## DONE lease I — readiness and progress evidence reconciliation

- Base revision: `ab816bc923df2a7d0121a8d74134b2af2fa05a3e`.
- Editing and integration owner: the primary Codex task. No parallel
  implementation track is open.
- Objective: reconcile the hard-self-host readiness table and the published
  progress denominator with the executable scorecard, completed installed
  fixed point, and latest same-source remote CI evidence.
- Priority order: evidence-owner consistency; no completion inflation; keep
  compiler substitution separate from native product-tool ownership; negative
  stale-wording ratchet; then compact documentation.
- Fact owners: `tests/self_host_readiness_scorecard.sh` for the ten Phase-1
  substrate capabilities, the canonical full bootstrap and installed-driver
  gates for the installed fixed point, and remote run `32938125698` for the
  current implementation checkpoint's 29-job platform/release matrix.
- Last legitimate consumers: the current table in
  `docs/self_hosted/07_hard_self_host_scorecard.md`, the percentage baseline in
  `docs/00_progress.md`, and the top progress summary in
  `docs/current_work_handoff.md`.
- Forbidden fallback: retaining `9/10 READY`, bootstrap or CI `3/4`, reviving
  already deleted public native compiler bypasses as open work, or calling the
  native formatter/debugger/scaffold/package-metadata/REPL-session products
  Pergyra-owned.
- Verification and falsifier: the static scorecard must reject the old
  capability-4 `SUBSET` row and the old `9/10` progress row; documentation
  quality and source UTF-8 gates must pass. This lease changes no SoT registry
  row and opens no new compiler implementation rung.

### Result

- The scorecard table now agrees with its executable owner and measured
  closure: all ten Phase-1 capabilities are `READY`. Remaining compiler-scale
  String scope reclamation is recorded as an efficiency frontier, not as a
  missing substrate capability or permission for a native fallback.
- Installed fixed-point and latest same-source remote evidence close the
  bootstrap and CI/release evidence axes at `4/4`. With hard replacement kept
  at 75%, strict beta at 83%, and SoT migration at 78.2%, the unchanged
  weighting yields 83.20%, displayed as 83% (81-85%).
- The static scorecard passed with all ten READY rows and now rejects both stale
  capability-4 `SUBSET` and `9/10` progress text. Documentation quality and
  source UTF-8 gates also passed. No source compiler path, SoT registry row, or
  product-tool ownership changed, so no successor implementation lease is
  inferred.

## DONE lease H — REPL compile/run native-bypass substitution

- Base revision: `acdab822b7d1ce27c636f73392ebb1d7738bf08a`.
- Editing and integration owner: the primary Codex task. Peer agents completed
  three read-only readiness reports and own only their assigned files under
  `docs/audits/`.
- Objective: retain the native C REPL session UI and declaration accumulation,
  but replace its per-evaluation `driver_run_pipeline` compiler call with the
  existing installed Pergyra C compile/run boundary.
- Priority order: one installed source-to-C fact owner; no native retry;
  preserve the current REPL session observable; retire transient artifacts;
  negative ratchet; then patch size and product-tool ownership.
- Fact owner: the already production-reachable installed source-C path through
  `c_runner_execute_installed_self_host_c` and the Pergyra compiler world. This
  lease does not claim a Pergyra owner for prompts, multiline admission,
  accumulated declarations, or REPL session transitions.
- Last legitimate consumer: the C REPL evaluation loop after it publishes one
  synthesized temporary source and before it compiles or runs that source.
- Direct bypass to delete: `src/compiler/repl.c` calls
  `driver_run_pipeline(&rf)` for every executable input without a native
  opt-out.
- Forbidden fallback: calling `driver_run_pipeline`, retrying native after a
  missing or failed installed driver, reporting the entire REPL as Pergyra-
  owned, leaking a produced binary/source artifact, or adding a second
  self-host compiler build or CI job.
- Falsifier and integration gate: public `pgy --repl` executes one Log input
  through the installed sibling and preserves the prompt/program/Bye
  transcript; a missing sibling emits its owned failure and no program line;
  unsupported source emits no program line and does not retry; static source
  rejects `driver_run_pipeline` in `repl.c`; the existing installed-driver CLI
  Make target sources the focused gate and reuses its one compiler build.
- Classification: only the REPL's compiler-bearing interior can become bounded
  `SUBSTITUTING`. The C-owned REPL product/session remains native and
  `NOT READY`; this lease does not change whole-product ownership or progress
  percentages by itself.

### Result

- RED with a nonexistent `PGY_SELF_DRIVER_BIN` still ran
  `repl-native-bypass` through native compilation. The direct call is now
  deleted; `repl_run` receives `argv[0]` and enters exactly one installed C
  compile/run boundary using the REPL's existing dev profile.
- The focused gate passed in 8 seconds and covers real installed evaluation,
  one counting-driver invocation, missing-driver and invalid-source failures,
  absence of native timing or rejected program publication, cleanup, and the
  static old-call ban. Incremental `make pgy` passed.
- `make self-host-installed-driver-cli-mode-test-smoke` passed with the new
  gate sourced by its existing script. Its one seed/bootstrap preparation took
  about five minutes; no new target, job, timeout, or second driver build was
  introduced.
- Directive/audit checkpoint `36af9496` and implementation checkpoint
  `48aeccca` are published on `main`. Push run `32938125698` passed 29/29 in
  30m31: Linux aggregate 15m39, full self-host 30m27, Windows and sanitizers
  8m50, codegen bootstrap 7m46, backend toolchain 9m27, and all 20 shards in
  41-58 seconds. Lease H is closed; no second product-tool track is inferred.

## DONE lease G — explicit-native isolation for unowned IR diagnostics

- Base revision: `8b8c78f0d6f5efd0eecaeaec7ee2b1796b6723dd`.
- Editing and integration owner: the primary Codex task. Peer agents completed
  read-only RIR, AIR, and HIR readiness reports and own no implementation file.
- Objective: remove the launcher's final implicit native dispatch for bare
  public `--rir`, `--rir-json`, `--air`, `--air-json`, `--hir`, `--hir-cfg`,
  `--hir-dom`, and `--hir-ssa`. Until an installed Pergyra producer owns a
  complete payload, these modes fail closed and the existing native diagnostics
  remain available only through the declared `--native-pipeline` opt-out.
- Priority order: no invented IR facts; no implicit C authority; preserve an
  explicit native oracle; deterministic mode-specific diagnostics; negative
  ratchet; then compatibility and patch size.
- Fact owner: none exists for a complete Pergyra RIR/AIR/HIR payload. Native
  `rir_lower`, `air_synthesize`, and `hir_lower` remain explicit-oracle owners;
  the launcher request selector owns only whether callers explicitly requested
  that native authority.
- Last legitimate consumer: the public argv selector before any installed
  child or `driver_run_pipeline` execution. A missing required Pergyra fact
  fails here rather than being guessed or silently sourced from C.
- Deleted bypass target: the final default `return driver_run_pipeline(&flags)`
  after all installed self-host selectors in `src/pgy_driver.c`.
- Forbidden fallback: native retry, native execution selected only because no
  Pergyra request variant exists, partial MIR/AST projection presented as RIR,
  AIR, or HIR, native dump parsing, or a fabricated general IR owner.
- Integration gate: all eight bare modes must exit nonzero with empty stdout
  and no pipeline-timing marker even when the installed driver is missing;
  every corresponding explicit `--native-pipeline` mode must remain
  executable; source ratchets must reject a third/default launcher call to
  `driver_run_pipeline`. Existing native IR tests must declare the opt-out.
- Classification: fallback/SoT closure only. It is not `SUBSTITUTING`, does not
  change the 78%/83% progress lines, and closes no top-level registry row.

### Result

- The final implicit `driver_run_pipeline` dispatch is deleted. Bare public
  RIR/AIR/HIR diagnostics now fail at the launcher with a mode-specific
  missing-Pergyra-owner diagnostic, empty stdout, and no timing marker. The
  same eight diagnostics remain executable only through explicit
  `--native-pipeline`.
- The RIR, AIR, and HIR readiness reports all returned `NOT READY`. They name,
  respectively, the missing ordered RIR program, general AIR graph issuance,
  and identity-bearing post-semantic HIR routine/CFG facts. No native dump was
  parsed, no MIR/AST reconstruction was presented as another IR, and no
  request variant or general producer was invented.
- The installed-driver parent gate, public MIR diagnostic, new eight-mode
  negative/opt-in gate, AIR graph validator parity, IR probe, AIR schema/MIR
  binding, machine-neutral/machine-layer, RIR flow, SEA lane, proof envelope,
  and C/LLVM observability gates are locally green. The committed AIR fixture
  had one deterministic July-era MIR binding fingerprint drift and now matches
  three repeated current-owner observations.
- `tests/self_hosted/mir_machine_layer_smoke.sh` was stopped after its five-
  minute focused budget while recompiling `driver_rung2_main.pgy`; no green is
  claimed for that run. The already-installed parent CLI gate completed in
  23 seconds, so no timeout or CI allowance was raised.
- Implementation checkpoint `4eef51ad` is published. First run `32932076025`
  exposed one structural violation: `src/pgy_driver.c` grew to 359 lines over
  its unchanged 340-line cap. The repair keeps that cap, moves mode identity to
  `driver_self_host_selection_owner` and diagnostic emission to `driver_diag`,
  and leaves the launcher at 340 lines and selection owner at its 140-line cap.
  Repair checkpoint `45a2cfae` is published and replacement run `32933640461`
  passed 29/29 in 30m09. `build-linux` passed in 14m41, full self-host in
  29m49, and all 20 backend shards in 40-59 seconds. This closes an implicit
  fallback only; it is not `SUBSTITUTING`, changes no percentage, and closes no
  registry row.

## DONE lease F — installed public `--mir` diagnostic substitution

- Base revision: `9ca4a69517142a4c87eb47862afcd55a9a9f2011`.
- During lease F, the primary Codex task owned editing and integration. Peer
  tasks were read-only auditors and did not own this executable rung.
- Objective: make the public installed `pgy --mir SOURCE` request execute the
  Pergyra source-to-MIR owner, admit that canonical MIR document once, and
  render one Pergyra-owned human diagnostic projection instead of entering the
  native `driver_run_pipeline -> mir_dump` path.
- Priority order: one canonical `pgy.mir.v1` semantic owner; typed admission
  before rendering; no invented lifecycle or source facts; explicit stable
  diagnostic-view contract; fail-closed child execution; delete the default
  native selector; then output familiarity, patch size, and formatting detail.
- Fact owner: `CompileSourceToMirJsonVerified` owns source-to-MIR production and
  `MirMachineLayerAdmittedJsonInput` carries the admitted document/routine fact
  views. The new diagnostic projection may consume those typed views but must
  not treat serialized JSON text, the legacy C `MIRProgram`, or reconstructed
  source/AST scans as a second semantic owner.
- Production entrypoint and last consumer: public `pgy --mir SOURCE`, through
  the installed self-driver child and its stdout payload consumer. The child
  must compile, admit, and render exactly once; the native launcher may only
  relay its exit status and bytes.
- Deleted pre-change bypass: default `--mir` skipped the installed driver and
  fell through to `driver_run_pipeline(&flags)`, which called native
  `mir_dump`. Explicit `--native-pipeline --mir` remains the bounded oracle and
  escape hatch; it is not an automatic retry or missing-driver fallback.
- Forbidden fallback: guessed SSA/liveness/lifecycle counts absent from the
  admitted MIR contract; JSON substring formatting without typed admission;
  reparsing source or AST in the diagnostic owner; native retry after a
  self-driver failure; silent use of the default native path when the installed
  driver is missing; or a temporary artifact where a stdout payload suffices.
- Observed RED: with `PGY_SELF_DRIVER_BIN` naming a missing executable, public
  `pgy --mir examples/hello.pgy` exits 0 and emits the same bytes as explicit
  native `--mir`, proving the installed self-driver is bypassed. The legacy
  dump also exposes lifecycle/source details not present in canonical MIR JSON,
  so byte-copying it would require guessed or second-owner facts.
- Falsifier and integration gate: public `--mir` must emit the newly declared
  canonical diagnostic view for a simple routine and one meaningful CFG/local
  fixture; an explicit native invocation remains independently observable;
  a missing installed driver, invalid source, malformed required MIR fact, or
  renderer admission failure must exit nonzero with no MIR payload; and a
  static selector ratchet must reject default `--mir` reachability to
  `driver_run_pipeline`/`mir_dump`. Reuse the existing installed-driver build
  and CI target; add no workflow job and perform no second self-host build.

### Agent directive boundary

- Agent work directives are not numbered project architecture documents. They
  live separately under `docs/agent_work_directives/`; read-only audit outputs
  live under `docs/audits/` and are navigation evidence, not semantic authority.
- The completed semantic-hop/direct-MIR/navigation directive is
  `docs/agent_work_directives/semantic_hop_parallel_audit_2026-08-26.md`.
  It opens no follow-up implementation rung by itself.

### Result

- Implementation checkpoint `c2ff6548` and closure checkpoint `b3da55a3` are
  on local and remote `main`. The repair sequence removed a cross-function
  `TextBuilder` lifetime violation, removed the added text-to-text helper,
  kept the projection owner at 198 lines, restored its 200-line cap, and
  preserved stable diagnostic bytes.
- Public installed `pgy --mir SOURCE` now selects the self-host diagnostic
  relay after the explicit native opt-out and before final native dispatch.
  The relay invokes only `--emit-mir-diagnostic-verified`, captures one bounded
  stdout payload, and never calls `driver_run_pipeline` or `mir_dump`. Explicit
  `--native-pipeline --mir` remains the independent lifecycle oracle.
- The Pergyra child reuses `ProduceSourceMirThroughPgyCompilerWorld` and the
  existing `DriverSourceMirPayloadReceipt`; no mode-specific world, zone,
  protocol enum, or temporary MIR artifact was added. Borrowed MIR text enters
  the full schema/parallel/topology/machine/intent admission once, then a typed
  diagnostic projection renders only admitted routine/block/instruction facts.
- The native relay owns a 128 MiB payload limit and 300-second child budget.
  Windows uses a kill-on-close Job Object and process-state polling; POSIX uses
  a process group plus nonblocking poll. Child failure, timeout, overflow,
  crash, empty success, descendant-held stdout, stdout-close-before-exit, and
  final stdout write failure remain distinguishable or fail closed with no
  child-failure payload prefix relayed.
- A final current-source Pergyra-built DRV-2 is installed. The full installed
  CLI gate passes, including simple and four-block CFG diagnostics, adjacency
  lookup, malformed shared admission, invalid source, missing driver,
  unsupported options, silent success, descendant-held stdout, and Windows
  broken-pipe ordering. Public/internal simple diagnostics are byte-identical;
  public MIR JSON and the explicit native IR probe remain green.
- `make -n` for the weekly public-MIR/default-C target pair reports one
  self-host build and one installed-driver gate. The diagnostic sibling is
  sourced by that existing gate, so no new workflow job, standalone target, or
  second self-host build exists. SoT edge is unchanged at 86 authorities / 180
  derived carriers / `CLOSED=49 BRIDGE=36 ACTIVE=1`; likeness remains at its
  prior core text-munging ceiling of 76.
- The complete component inventory is not claimed locally: its primary scan
  exceeded the static-loop budget and was stopped. Remote run `32926584459`
  at exact HEAD `b3da55a3` completed 29/29 in 18m26, including `build-linux`
  in 15m06, full self-host in 18m04, sanitizers, Windows/macOS, proofs, and all
  backend shards. The remote Linux aggregate owns complete component and POSIX
  capture evidence for this closure.
- This bounded public diagnostic is `SUBSTITUTING`: default public `--mir` no
  longer reaches the real native C `MIRProgram -> mir_dump` path. It does not
  close a top-level SoT row or claim native-only lifecycle facts, so integrated
  78%, strict beta 83%, and hard SoT counts remain unchanged.

## DONE lease E — nested priority/observability direct-C substitution

- Base revision: `5d2f7e6e060a67e2950deba574f334d59889f6f6`.
- Editing and integration owner: the primary Codex task. Peer Codex tasks are
  read-only auditors and must not edit, stage, commit, push, or run a self-host
  build while this lease is active.
- Objective: make the exact admitted one-subject/one-zone/two-intent nested
  priority/observability family produce C from the same sealed
  `DirectMirNestedIntentProgramPlan` already consumed by direct LLVM, then make
  production source-C and direct-MIR C consume that one C projection instead of
  reconstructing an AST or falling through to the scalar route.
- Priority order: exact native/public behavior; one route/graph/header/policy
  plan; MIR-blind target-specific C materialization; byte-identical source-C and
  direct-C artifacts; fail-closed claimed-family mutations; old-path rejection;
  then emitted warning cleanliness and patch size.
- Fact owner: `DirectMirNestedIntentProgramPlanFromAdmitted`, with its admitted
  route, graph, routine-header, outer-policy, and inner-policy facts. Existing
  intent observability ABI rows and runtime symbols remain ABI authority. No C
  emitter may reopen admitted MIR, reconstructed AST, source text, or raw JSON.
- Production entrypoints and last consumers: installed
  `DriverCliSourceCArtifact`/stdout through
  `CompileMachineAdmittedMirJsonToCForTargetVerifiedObserved`, and installed
  `DriverCliDirectMirCArtifact` through
  `CompileAdmittedDirectMirMultiRoutineForTargetObserved`. Both must consume one
  shared claimed-plan C projection before artifact publication.
- Direct bypass to delete: for this exact claimed family, source-C currently
  enters `DriverRung2IntentTreeEmissionOrDie` and reconstructs/reanalyzes an AST;
  direct-MIR C skips the LLVM-only claimed projection and dies later at
  `scalar-program-route`. The claimed family must not retry either path.
- Observed RED: the current admitted MIR projects and executes through direct
  LLVM, but direct C exits 1 with `direct MIR scalar program route rejected` and
  publishes no artifact. Public installed C and native C both execute the exact
  nine-line `active.count/name/priority/concurrent`, `outer.ok`, and `captures`
  output, proving an executable behavior oracle while source-C still uses the
  reconstructed-AST path.
- Falsifier and integration gate: one current-source MIR must produce
  byte-identical source-C and direct-C artifacts, both compile warning-clean and
  execute the exact nine-line output also produced by native C/LLVM and direct
  LLVM. Existing missing-priority, graph-drift, duplicate-source, method-owner,
  and action-name mutations must fail before either C artifact is published,
  and static ordering must reject MIR-to-AST/scalar retry after a nested route
  claim. Source a bounded C sibling from the existing nested-intent target so no
  CI job or second self-host compiler build is added. This exact family is a
  `SUBSTITUTING` delta; do not promote broader intent or top-level SoT status
  without denominator evidence.

### Parallel read-only assignments

1. Plan/emitter auditor: inspect the sealed nested plan, existing LLVM emitter,
   runtime ABI owners, and source-C admission point. Report the minimum C
   materialization seam and any lifetime/dual-authority hazards; make no edits.
2. Gate/integration auditor: inspect the existing nested-intent gate, Make/CI
   reuse, owner caps, exact public/direct/native evidence, and mutation set.
   Report the smallest non-duplicating executable/negative ratchet; make no
   edits.

### Result

- Code checkpoint `9ad47dd782915aaf0d200d7efa0fb781c8d51736` makes the
  nested projection claim its route before target selection, seal one plan,
  and dispatch that plan to MIR-blind C or LLVM emitters. Direct C no longer
  falls through to scalar admission for the claimed family.
- `DriverRung2NestedIntentCSubstitutionIfClaimed` gives the exact four-routine,
  two-declaration source/MIR-to-C family the same projection before
  `DriverRung2IntentTreeEmissionOrDie`. Unclaimed programs alone continue to
  the general reconstruction path. File-backed MIR source JSON is retired only
  after the shared payload and topology projection are complete.
- The final current-source Pergyra-built DRV-2 install is GREEN. The existing
  nested gate plus its sourced C sibling passed in 11.6 seconds: source-C and
  direct-MIR C are byte-identical at 2,488 bytes, SHA-256
  `4F2B9434...23E644`; thread-safe C compiles with
  `-Wall -Wextra -Werror`; runtime output is the exact nine-line oracle; and
  five LLVM plus ten C no-artifact mutations fail at owned boundaries.
- No Make target, workflow job, or second target-local self-host build was
  added. The existing target already reaches the sourced sibling. A local full
  component inventory scan was stopped after 90 seconds with no output to
  honor the static-loop budget; focused owner caps, ordering ratchets, shell
  syntax, `git diff --check`, the real DRV-2 build, and the executable gate are
  GREEN. Its first push run `32911287910` exposed one real self-host CFG
  falsifier: the claimed-invalid target branch called `Die` but lacked the
  explicit unreachable `return None` required by the current body-safety
  proof. Repair checkpoint `2f4dfe2844a8dffd813081b9381e236829204f02`
  follows the repository's fail-closed return convention. The exact native-
  oracle driver emission now completes locally with zero errors and the
  focused executable gate remains GREEN. Replacement run `32912230440`
  advanced through fixed-point equality and installed DRV-2, then exposed a
  Linux-only harness mismatch: its thread-safe C invocation omitted the POSIX
  feature macros already used by the bootstrap emitted-C profile. Checkpoint
  `60e9fb8a2d3ed32535c6ceee7d67246f6c32ddba` mirrors that profile in the
  focused sibling without changing generated code. Local shell syntax and the
  exact executable gate remain GREEN. Final run `32913743277` completed 29/29
  GREEN in 29m25; `build-linux` took 15m24 and full self-host took 29m20. All
  20 backend shards, sanitizers, Windows/macOS, codegen bootstrap, TSan, and
  Rocq passed at checkpoint `60e9fb8a`.
- Windows repeatability rule: use `C:\msys64\usr\bin\bash.exe` with
  `/ucrt64/bin` first on `PATH`. Bare `bash` selects the unavailable WSL
  `/bin/bash` here, and `/mingw64/bin` is not the compiler runtime used by the
  installed artifacts. Once a current DRV-2 is installed, invoke the focused
  script directly; immediately invoking the phony self-host Make prerequisite
  repeats the bootstrap and obscures the actual gate cost.
- This is bounded `SUBSTITUTING` for the exact nested priority/observability C
  family. It does not close arbitrary intent C, the wider intent declaration
  row, or any top-level SoT authority; 78%, strict beta 83%, and
  `49 CLOSED / 36 BRIDGE / 1 ACTIVE` remain unchanged.

## DONE lease D — installed MIR-C stdout world/action boundary

- Base revision: `5bbb62877e42216a949d024c603ab3392f2eef84`.
- Editing and integration owner: the primary Codex task. Peer Codex tasks are
  read-only auditors and must not edit, stage, commit, push, or run a self-host
  build while this lease is active.
- Objective: make both installed MIR-C stdout requests (`--mir-json INPUT` and
  its explicit machine-manifest form) enter the existing
  `PgyCompilerWorld.direct_mir` zone and consume the same typed MIR-C payload
  admission as artifact publication before the read executor logs C text.
- Priority order: preserve the existing observation request identity; make
  default versus explicit machine declaration an independent typed fact;
  preserve canonical C target projection and the original compiler artifact;
  compile once; retain byte-exact stdout and artifact behavior; then reject the
  old bypass.
- Fact owners: `CompileMirJsonToCVerified` and
  `CompileMirJsonToCVerifiedObserved` remain semantic/emission owners;
  `DriverRung2Execution` owns direct-MIR request/outcome orchestration;
  `PgyCompilerWorld.direct_mir` owns executable composition;
  `SelfHostMachineLayerDeclaration` remains manifest authority; and the
  existing compiler target-projection owner remains C target authority.
- Last legitimate consumer: `DriverRung2ExecuteReadRequest`, through one
  MIR-C payload logger that accepts only a ready typed admission; artifact
  publication consumes that same admission before the existing atomic commit.
- Forbidden fallback: either direct `CompileMirJsonToCVerified` call in
  `driver_rung2_cli_read_execution_owner.pgy`; invalid explicit manifest
  collapse to the default declaration; a second world/zone/compiler call;
  reconstructed target projection; temp-artifact publication for stdout; or
  duplicated default/manifest and observed/unobserved policy in consumers.
- Observed RED: the read executor contains two direct MIR-C compiler calls.
  Default stdout is 9,430 bytes at SHA-256 `A29997AD...B8749`; an admitted
  manifest is 9,472 bytes at `CB37D99B...19BA`; artifact output is byte-equal
  after host-newline normalization. An explicitly malformed manifest currently
  exits 0 and emits the 9,430-byte default output, proving a hidden fallback.
- Falsifier and integration gate: default and admitted-manifest stdout must
  preserve the observed bytes through one ready typed admission; default
  artifact publication must consume that same admission; malformed explicit
  manifest must exit nonzero with no C payload; and the read executor must have
  no direct compiler call. Source a bounded sibling ratchet from the existing
  installed CLI mode gate so no Make target, CI job, or second compiler build is
  added. Then run component/world/topology/hard/likeness/document gates and the
  existing remote push matrix. This is `REACHABLE` dogfood closure, not a new
  `SUBSTITUTING` replacement, so progress and SoT counts do not change.

### Parallel read-only assignments

1. Typed-owner auditor: inspect the proposed two-axis request/admission seam,
   canonical target authority, and shared stdout/artifact consumption. Report
   exact dual-authority, hidden-default, or identity risks; make no edits.
2. Gate/CI auditor: inspect the existing installed CLI gate, Make/workflow
   invocation, topology/component budgets, and the proposed sourced sibling.
   Report the smallest non-duplicating falsifier; make no edits.

### Current observed evidence

- Both read-only audits are complete. The typed-owner audit retained
  `DriverRung2MirCRequest` as the observation axis and required a separate
  machine-request axis plus one common producer carrying the canonical target
  fact and original compiler artifact. The gate audit reused the existing
  installed-driver target and required only admitted-manifest and malformed-
  manifest invocations in a sourced sibling; neither auditor edited the tree.
- Both read-executor compiler calls are removed. Stdout and artifact publication
  consume `DriverRung2MirCProducePayloadAdmitted`; compiler selection exists
  only inside that producer, and the stdout owner cannot write or commit.
- The malformed explicit manifest baseline exited 0 and emitted the 9,430-byte
  default C payload. It now exits nonzero with `MIR C machine declaration is
  invalid` and emits no C. Default stdout remains 9,430 bytes at
  `A29997AD...B8749`, the host-normalized artifact remains 9,174 bytes at
  `F36551DE...96A33`, and admitted-manifest stdout remains 9,472 bytes at
  `CB37D99B...19BA`.
- A current-source Pergyra-built DRV-2 was installed and the existing focused
  target is GREEN. Component, compiler-world, recursive topology, hard,
  likeness, progress, SoT authority-edge, protocol, documentation, and diff
  gates are locally GREEN. Likeness remains sentinel `24/24`, Result/Option
  `4287/4287`, one world, 22 zones, and four members; SoT remains 86 authorities
  / 180 derived carriers and `CLOSED=49 BRIDGE=36 ACTIVE=1`.
- The push workflow invokes the same installed-driver target inside the existing
  Make call, so no job or second self-host compiler build is added. Code
  checkpoint `e5b159c3` is on local and remote `main`; push run `32905167784`
  completed 29/29 green in 29m16. `build-linux` passed in 15m04 and full
  self-host in 29m12; all backend shards, sanitizers, platforms, codegen
  bootstrap, TSan, and Rocq are green. Lease D is released, and no successor
  rung is inferred from this result.

## DONE lease C — installed source-C stdout world/action boundary

- Base revision: `e4bc4b4d7cb96ce7fe33478369cb7de00e1e2310`.
- Editing and integration owner: the primary Codex task. Peer Codex tasks are
  read-only auditors and must not edit, stage, commit, push, or run the long
  self-host build while this lease is active.
- Objective: make both installed source-C stdout requests (`SOURCE` and
  `--emit-c-verified`, with or without a machine manifest) enter the existing
  `PgyCompilerWorld.source_c` zone and consume one typed payload admission
  before the read executor logs C text.
- Priority order: preserve request and manifest identity; validate the existing
  source-C subject and topology identity; preserve the compiler artifact target
  projection and capability fingerprint; return a typed payload outcome;
  retain byte-exact stdout and failure behavior; then reject the old bypass.
- Fact owners: `CompileSourceToCVerified` remains the semantic/emission owner;
  `DriverSourceCExecution` owns source-C request/outcome orchestration;
  `PgyCompilerWorld.source_c` owns executable composition; and
  `SelfHostMachineLayerDeclaration` remains the manifest owner.
- Last legitimate consumer: `DriverRung2ExecuteReadRequest`, through one
  source-C payload logger that accepts only a ready typed admission.
- Forbidden fallback: a direct `CompileSourceToCVerified` call in
  `driver_rung2_cli_read_execution_owner.pgy`, native retry, artifact-temp
  publication for stdout, a second world or source-C zone, an empty/default
  payload, or duplicate semantic/emission validation outside the existing
  compiler artifact owner.
- Observed RED: the current read executor contains exactly two direct
  `CompileSourceToCVerified` calls for `DriverCliSourceCStdout` and
  `DriverCliSourceCManifestStdout`; both bypass the already production-reachable
  source-C world/action boundary.
- Falsifier and integration gate: the installed default and
  `--emit-c-verified` paths, including a manifest form, must produce byte-exact
  C through a ready typed admission; the read executor must contain no direct
  compiler call; invalid subject/topology/artifact identity must remain typed
  failure with no fallback. Run the bounded source-C parity gate first, then
  component/world/topology/hard/likeness/document gates and the existing remote
  push matrix. This is `REACHABLE` dogfood closure, not a new hard
  `SUBSTITUTING` replacement, so progress and SoT counts do not change.

### Current observed evidence

- The typed-owner audit required stdout and artifact publication to consume one
  payload admission carrying the original `CompilerEmissionArtifact`; no copied
  payload/projection/fingerprint authority was added. The gate audit kept the
  existing Make target and workflow invocation, so this rung adds no job and no
  second self-host compiler build.
- The peer audit also found a real hidden fallback: an explicitly supplied
  malformed machine manifest collapsed to the same empty declaration as the
  default request. The new `SourceCDefault` / `SourceCManifestVerified` request
  identity makes the default absence explicit and rejects an invalid explicit
  declaration before compilation.
- The focused installed gate is GREEN. Default and explicit
  `--emit-c-verified` stdout are raw-byte-equal, while the artifact action has
  the same payload after host-newline normalization; an admitted manifest
  preserves its machine-layer mapping; and the invalid manifest exits 1 with
  `source C machine declaration is invalid` and no C payload.
- Exact pre/post evidence is stable: default and explicit output remain 9,430
  bytes at SHA-256 `A29997AD...B8749`; admitted-manifest output remains 9,472
  bytes at `CB37D99B...19BA`. Before this rung the invalid manifest exited 0 and
  emitted the 9,430-byte default artifact; it now fails closed.
- Complete component inventory, compiler-world contract, recursive topology,
  hard substitution, likeness, and SoT authority-edge gates are locally GREEN.
  Likeness remains sentinel `24/24`, Result/Option `4287/4287`, one compiler
  world, 22 zones, and four world members. SoT remains
  `CLOSED=49 BRIDGE=36 ACTIVE=1`.
- Code checkpoint `20ffa7c7` is on local and remote `main`. Push run
  `32897701600` completed 29/29 green in 29m41; `build-linux` passed in 15m21
  and full self-host passed in 29m38. All backend shards, sanitizers, platforms,
  codegen bootstrap, and Rocq are green. Lease C is released; no successor
  executable rung is inferred by this result.

### Parallel read-only assignments

1. Typed-owner auditor: inspect only the proposed source-C payload admission
   and its reuse by artifact/stdout consumers for dual authority, invalid
   identity/fingerprint handling, or a hidden default. Report exact file/line
   findings; make no edits.
2. Gate/CI auditor: inspect only the bounded falsifier, existing Make target,
   workflow invocation, and line/duplication budgets. Recommend the smallest
   gate change that adds no job and no second self-host build; make no edits.

## DONE lease A — intent mode/priority C-codegen last consumer

- Base revision: `3698ab198fd2d84ca66834db0ff90a22cb2ac9f1`.
- Completed owner: Codex task `019f8921-1147-70c1-8eff-b6fee8e59aec`.
- Objective: delete the reconstructed-AST read of intent mode/priority from C
  emission and feed the last consumer one exact semantic-DIR or admitted-MIR
  policy receipt.
- Fact owner: production C uses admitted MIR `IntentMode` and
  `IntentEval(priority)` carriers plus the canonical expression occurrence;
  the direct semantic codegen entrypoint materializes the same receipt from
  admitted DIR policy facts.
- Last legitimate consumer: `CodegenIntentObservabilityEmitPrologue`.
- Forbidden fallback: `TypedAstArena*` or `AstTreeArtifact` in the mode/priority
  emission owners, missing-receipt defaults, name-only joins, graph
  reconstruction, or a dual MIR/AST read.
- Integration gate: complete component contract; current-source driver build;
  installed nested mode/priority C parity including missing, duplicate,
  missing-graph, and graph-drift negatives; composite-intent LLVM parity; then
  documentation/registry gates and `git diff --check`.

### Released edit lease

The following paths were exclusive to lease A while it was active. The lease is
now released; this list is historical overlap evidence:

- `src/self_hosted/codegen/input/intent_policy_codegen_view_owner.pgy`
- `src/self_hosted/compiler/intent_policy_c_codegen_bridge_owner.pgy`
- `src/self_hosted/compiler/codegen_callable_receiver_bridge_owner.pgy`
- `src/self_hosted/compiler/driver_rung2_owner.pgy`
- `src/self_hosted/codegen/emission/intent_*emit_owner.pgy`
- `src/self_hosted/codegen/emission/program_{emit,entry,admitted_semantic_owner}.pgy`
- `tests/self_hosted/parity/intent_{mode,priority}_nested_observability_owner.sh`
- `tests/self_hosted_component_contract_smoke.sh`
- `src/self_hosted/OWNERS.md`
- `docs/current_work_handoff.md`

Preserve the unrelated user-owned untracked `pgy-80135c2c/` directory. Do not
inspect it as project evidence, stage it, delete it, or rewrite it.

### Current observed evidence

- `tests/self_hosted_component_contract_smoke.sh`: PASS after the AST-read
  residue ratchet and owner inventory update.
- Current source graph through the Pergyra-built codegen seed: PASS,
  10,609,620-byte C artifact, 114.27 seconds, no `CODEGEN ERROR`.
- Isolated current-source driver C compile: PASS, 17.12 seconds.
- Nested intent priority/mode MIR carriage, C execution parity, missing,
  duplicate, missing-graph, and graph-drift rejection: PASS.
- Composite-intent direct-MIR LLVM success/failure parity and four no-artifact
  negatives: PASS.
- Zero-intent source-to-C through the isolated driver: PASS.
- Official current-source `make self-host-compiler`: PASS in 515.18 seconds;
  `bin/pgy-self-driver.exe` was installed by the Pergyra-built DRV-2 path.
- Installed nested mode/priority MIR carriage and public/native C execution
  parity: PASS, including missing, duplicate, missing-graph, and graph-drift
  rejection.
- Installed composite-intent direct-MIR LLVM success/failure parity and four
  no-artifact negatives: PASS.
- Final complete component contract: PASS after its internal source-MIR action
  ratchet and the syntax-ID/name stale-identity ratchet both passed.
- Next executable falsifier is observed RED rather than inferred: projecting
  the nested value-priority fixture's admitted MIR through
  `--mir-json-backend=llvm` fails closed at `scalar-program-route` stage
  `referenced-enum` and publishes no artifact. This is outside lease A and must
  not be repaired until lease A is published and a new objective card is set.
- First publication checkpoint `b6de9ba7` produced CI run `32862729216` at
  28/29. All full self-host, codegen bootstrap, platform, sanitizer, proof, and
  20 backend shards passed; `build-linux` alone caught likeness drift:
  sentinel `25 > 24` and Result/Option use `4264 < 4267`.
- The pending fix does not loosen either ratchet. Absent priority uses the
  existing explicit `priority_present` bit with a non-semantic zero storage
  value, while routine lookup carries absence as local `Option<Int>`. Local
  likeness is back at sentinel `24/24` and Result/Option `4267/4267`.
- The fixed current-source graph generated and compiled; nested mode/priority C
  parity and composite-intent LLVM parity pass with that isolated driver. The
  exact failed Linux target passed component, hard, and graph gates locally,
  then stopped only because local Coq/Rocq is unavailable; remote Rocq passed.
- Fix checkpoint `eba0103d` completed run `32866213832` at 29/29 green in
  29m53, including the repaired Linux likeness row, full self-host, all three
  platforms, sanitizers, Rocq, codegen bootstrap, and all 20 backend shards.
- A peer follow-up after that run required absent priority receipts to keep the
  canonical zero storage value. Final code checkpoint `a9e07841` adds that exact
  readiness invariant and its structural negative. Run `32870231909` completed
  29/29 green in 29m24, including full self-host, codegen bootstrap, all three
  platforms, sanitizers, Rocq, and all 20 backend shards. Lease A is closed.

### CI ratchet lesson

When deleting an Option-heavy AST scan, run
`make self-host-pergyra-likeness-test-smoke` before publication. Do not replace
typed absence with a numeric sentinel or raise the likeness ceiling to hide the
drop. If a view already has an explicit presence bit, its unused numeric slot
is storage only; lookup failure itself remains `Option`/`Result`.

## DONE lease B — nested intent direct-MIR LLVM route

Lease A's publication preconditions are satisfied: correction checkpoint
`a9e07841` completed CI run `32870231909` at 29/29 green. Codex goal
`019f8921-1147-70c1-8eff-b6fee8e59aec` owns this executable rung. Other tasks
remain read-only auditors; do not open parallel implementation tracks on the
same route.

- Objective: make the installed direct-MIR `--mir-json-backend=llvm` path
  execute the nested method/intent priority fixture through one exclusive mixed
  callable route. Do not treat the first `referenced-enum` rejection as the
  entire objective.
- Priority order: canonical declaration and routine identity; routine
  kind/owner/source-syntax identity; exact intent mode/priority carrier and
  expression occurrence; owner-directed call/field graph; fail-closed
  negatives; then installed LLVM execution parity.
- Fact owners: the program declaration index owns exact declaration row/bounds,
  the graph owns its bounded source-syntax projection, and the routine index
  owns normalized callable kind/owner/source syntax ID;
  `MirIntentRoutineCarrierProjection`,
  `MirIntentModeProjection`, and `MirIntentPriorityProjection` own intent policy
  carriage and its exact semantic expression root.
- Last legitimate consumer: an exclusive mixed function/method/intent route in
  `DirectMirMultiRoutineProjection`, before the scalar-only route, handing its
  sealed graph to the direct-MIR LLVM terminal projector.
- Forbidden fallback: priority `0` hardcode, source-text reparse, AST
  reconstruction, widening the function-only signature fact to disguise a
  mixed-callable route, fixture branching, native retry, per-fixture special
  cases, or starting a general query/cache track.
- Observed RED: admitted MIR from
  `tests/self_hosted/parity/fixture/intent_priority_nested_observability.pgy`
  through the installed LLVM backend publishes no artifact and first fails at
  `owner=scalar-program-route stage=referenced-enum`.
- Dispatch cause: the fixture currently falls into the scalar-only route, whose
  first rejection is `referenced-enum`; a local no-enum repair would then expose
  `callable-route-envelope stage=signature`. The executable delta instead
  claims one mixed-callable route after composite intent and before scalar
  admission. The referenced-enum owner is not reached and is outside this
  lease.
- Minimal executable delta: add the exclusive mixed-callable route, sealed
  graph/plan, LLVM emitter, and thin projection; exact-cross-seal declaration,
  routine, intent-policy, ordered intent-binding, and expression-graph identities;
  derive `Main -> Outer`, `Outer -> Inner`, `Inner -> Capture`, and subject/zone
  field identity from admitted facts. Preserve priority as a literal-or-formal-
  parameter operand rather than collapsing it to `Int`; evaluate Inner's
  dynamic parameter `requested` in LLVM routine scope, and never substitute
  priority `0` for absence.
- Falsifiers: installed direct-MIR LLVM emits, links, and runs with byte-equal
  expected output and no scalar-route receipt; Outer observes literal priority
  `1`, Inner observes runtime `requested`; graph drift, missing priority, and
  syntax/name crosswire publish no artifact. Referenced-enum name/source-ID/
  payload drift remains a separate owner negative because this fixture has no
  enums. Retain composite-intent no-artifact negatives and finish with the
  component, hard, graph, documentation, and installed parity gates.

### Current observed evidence

- The exclusive route now claims after composite intent and before scalar
  admission. It seals `Main -> OuterPriority -> InnerPriority -> Capture`, the
  subject/zone fields, literal Outer priority `1`, and Inner priority from the
  unique `value/Int/requested` intent binding. It does not touch the unrelated
  referenced-enum owner.
- Actual MIR contradicted the initial routine-parameter assumption: intent
  header `params` are empty and `world/probe/requested` live in ordered intent
  binding carriers. Method `self` is an implicit receiver with null type/ABI,
  so it is validated at the receiver boundary and excluded from the explicit
  typed parameter set. Ownerless routine identity comes only from the admitted
  routine index rather than a second raw-JSON owner read.
- An isolated current-source driver emitted LLVM, linked through the public
  self-host path, and matched native LLVM against the exact nine-line golden.
  The focused gate completes in about seven seconds with five no-artifact
  negatives: missing Inner priority, priority graph drift, duplicate source
  identity, method-owner crosswire, and semantic action-name/target-row
  crosswire.
- `make self-host-direct-mir-nested-intent-program-llvm-test-smoke` rebuilt and
  installed Pergyra-built DRV-2, then passed the same LLVM parity and five
  negatives using the installed driver. The complete component contract also
  passed, including its source-MIR execution ratchet.
- The new Make target is in the existing full fixed-point invocation in both
  push CI and weekly self-host parity; it adds no job and no second self-host
  compiler build. The dispatcher remains at its existing 110-line ratchet.
- Hard contract, progress metric, UTF/documentation, and diff gates pass.
  Likeness passes at sentinel `24/24` and Result/Option `4287/4287`; the improved
  typed-error count is tightened into the baseline rather than left as a CI
  warning.
- Checkpoint `2d43bd66` is published. Run `32884881665` completed 28/29: all
  executable, proof, platform, sanitizer, and backend jobs passed, but Linux
  preparation found that the two new fact owners lacked derived-registry rows.
  The bounded repair classifies both under existing `mir.execution_graph` as
  `projection`; the exact edge is locally green at 86 authorities / 180 derived
  carriers without changing `CLOSED=49 BRIDGE=36 ACTIVE=1`.
- Repair checkpoint `6be30daa` completed run `32888031601` at 29/29 green in
  29m19. `build-linux` passed in 15m18, full self-host passed in 29m15, and all
  20 backend shards, sanitizers, platforms, codegen, and Rocq remained green.
  This lease is closed. Overall stays 78%, strict beta stays 83%, and hard SoT
  stays `CLOSED=49 BRIDGE=36 ACTIVE=1`.

## Peer Codex assignment — read-only audit only

If another Codex is operating on this checkout, perform this bounded task and
do not implement changes in lease B:

1. Read the active objective and diff without modifying files.
2. Look only for dual authority, a hidden AST/default fallback, an inexact
   canonical intent join, a missing negative gate, or a mismatched call
   signature/lifetime.
3. Report findings with exact file and line evidence to the user or append them
   under `Peer review notes` below. If there are no findings, say which claims
   were checked; do not report a generic approval.
4. Do not commit or push shared `main` while lease B is `ACTIVE` or
   `PUBLISHING`.

## Peer review notes

- Peer Codex (read-only) reviewed the lease A diff before its final
  integration evidence was appended. Two findings were fixed by the lease
  owner in the current worktree:
  1. `CodegenIntentExecutionPlanDefinitionBlock` still accepted an unused
     `expression_surfaces` parameter after the policy-view migration;
     removed together with the stale call argument in `program_emit.pgy`.
  2. `CodegenIntentPolicyMirRoutineRowOrDie` could skip a same-syntax-id MIR
     intent row with a mismatched name instead of rejecting it. The lease owner
     tightened the join further after re-audit: canonical syntax ID is claimed
     first, any mismatched intent name fails immediately with its own stale-
     identity diagnostic, and duplicate syntax ownership is rejected even if a
     second row has the expected name.
- No dual authority, hidden AST/default fallback, or missing negative gate was
  found in the reviewed slice. The semantic entrypoint's DIR receipt scan is
  documented as an admission-time read, not a C-emission AST fallback.
- Follow-up read-only finding: the later sentinel cleanup changed absent
  priority rows from `-1` to `0`, but readiness initially did not reject an
  absent row with nonzero root. The lease owner restored the exact invariant
  `priority_present[row] || root == 0` and added a component-contract ratchet
  for the absent-row nonzero receipt.
- Lease B read-only header audit found two impossible assumptions before the
  executable path was sealed: ownerless routines store `owner:null` while the
  admitted index owns normalized `""`, and method `self` has null type/ABI so
  it cannot enter the explicit typed-formal owner. Both were corrected without
  weakening the shared parameter fact.
- Lease B read-only gate audit kept LLVM behavior out of the 174/180-line
  C-only priority gate, selected a separate 160-line sibling gate, preserved
  the dispatcher 110-line cap, and connected both CI workflows through the
  existing single Make invocation.
