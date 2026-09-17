# Current Work Handoff

Updated: 2026-09-17 (Asia/Seoul). This is navigation only. Compiler owners,
registries, and executable gates override it.

## Active self-host context — compatibility evolution CLOSED and published

Material code checkpoint: `85068c83fb7d4ff1c009ffa938dc45611bca36fe`
contains the compatibility closure. Documentation publication checkpoint
`d732ef9e740fc963a07a831d5e7f3e2406c4542b` and `origin/main` matched before
this final navigation refresh. Exact-head push CI run `35209339933` completed
30/30 green. The pre-existing PgyMath, Slice, proof-pipeline, and Makefile
changes remain dirty and were neither staged nor rewritten by this rung.

Objective card:
- Objective: finish `compatibility.evolution` by carrying the Pergyra-owned
  nine-row receipt into the actual diagnostic/ABI and artifact/package
  consumers, then delete the native serialized-manifest parser.
- Priority: exact row identity, direct last-consumer carriage, missing/crossed
  row failure, old C-path deletion, negative ratchet, then registry closure.
- Fact owner: `compatibility_evolution_owner.pgy` issues the canonical receipt
  and projects one immutable `CompilerCompatibilityExecutionView` with named
  source, ABI, behavior, diagnostic, AIR, MIR, trace, capability, and stdlib
  rows.
- Last legitimate consumers:
  `DriverRung2ExecuteReadRequest` at the diagnostic/ABI boundary and the five
  `DriverRung2InstalledPublish*` functions at the artifact/package boundary.
- Forbidden fallback: root-only readiness, native reads of
  `expected/compatibility_evolution.txt`, pipe-delimited row reparsing, local
  compatibility lists, or warnings without owner migration metadata.
- Gate/falsifier: `compatibility_evolution_manifest_parity.sh` rejects a valid-
  shape diagnostic crosswire and a missing package row, requires both direct
  consumers, and rejects restoration of the C text path.

Reached evidence:
- `make -j1 self-host-compiler` completed after the final source shape and
  installed a Pergyra-built DRV-2. SHA-256:
  `B5B2E78547843C2B2638BCD994A08C5D65EEE9375D462F23E7238BA807A0C3A4`.
- Compatibility C/LLVM output is artifact-equal; receipt and execution-view
  mutations fail closed. The explicit native oracle compiles without opening
  the self-host text projection.
- The exact installed binary passes the public MIR diagnostic boundary and the
  package MIR/C/LLVM gate. The broader installed CLI run also passed all typed
  diagnostic families, source-C/source-MIR/MIR-C separation, format, REPL, and
  device-manifest legs.
- The component contract passes 2,435 line-cap requests; compiler topology and
  the SoT single-owner edge pass. Registry census is now
  `CLOSED=57 / BRIDGE=30 / ACTIVE=2`.
- SoT adequacy live bindings and negative mutations pass with an explicit local
  Rocq/Coq skip because no prover is installed on this workstation.

Publication state and next falsifier:
- The code checkpoint and its handoff publication are on `origin/main`; the
  exact code-bearing push run is green. This navigation-only refresh does not
  change compiler semantics and does not absorb the concurrent
  PgyMath/Slice/proof packet.
- A restored `driver_diag_compatibility_manifest_validate_file`, native read of
  the expected text artifact, or consumer that stops at root-only readiness
  must make the focused gate and SoT edge fail before artifact publication.

## Previous self-host context — Zone authority transition CLOSED and published

Material compiler/test checkpoint:
`8f7837060f04afd533088a751fde8cdfe22136e7`. This handoff refresh is a
docs-only descendant; consult Git for its own commit ID. `origin/main` is
`e79083b838244a8662dcecac4b06d92019470c8f` until the Bash 3.2 portability
repair is pushed. The Zone/Future closure, five-sentinel `Option<Int>` repair,
restored diagnostic identity, and one-pass Zone transition consumption are
published. The bounded follow-up validates the sealed receipt at
execution-view admission, then lets the read-only source cross-seal and action
emitter consume admitted rows without repeating whole-receipt validation per
step. Its first remote run found only a test-script portability violation;
`8f783706` replaces the rejected case-pattern continuation without changing
the owner census. Six pre-existing Slice semantic/ratchet edits remain a
separate incomplete, unstaged packet and are not part of the material
checkpoint.

Objective card:
- Objective: replace final self-C reconstruction of an Intent step's actor,
  optional authority, and Zone subject slots with one exact MIR transition
  receipt, then consume it with the shared domain-runtime Zone owner.
- Priority: exact admitted identities, missing/crossed fact failure, old-path
  deletion, one negative ratchet, then documentation and publication.
- Fact owner: `SemanticAstZoneAuthorityFactsFromArtifact` remains the source
  semantic owner; MIR declaration admission and
  `MirIntentZoneAuthorityTransitionFacts` preserve its exact slot identities.
- Last legitimate consumer: the C-facing transition view immediately before
  Intent action emission. The final emitter may derive C spelling and address
  mode only.
- Forbidden fallback: AST authority reread, codegen type-environment slot
  selection, name-only actor/authority recovery, or coexistence with the
  retired `intent_step_binding_owner.pgy` family.
- Gate/falsifier:
  `tests/self_hosted/parity/intent_zone_authority_transition_owner.sh` drives
  a distinct actor and `authorized by` subject through installed source→MIR→C
  and direct MIR→C, compares emitted bytes and self/native C/LLVM behavior, and
  rejects missing, crossed, and non-Zone authority rows without a C artifact.

Bounded consumption refinement card:
- Objective: validate one immutable Zone transition snapshot at codegen-view
  admission and consume admitted rows without whole-receipt validation per
  source/action step.
- Priority: preserve semantic and diagnostic identity, keep one explicit
  validation owner, remove repeated global work, then ratchet the owner set.
- Fact owner: `CodegenIntentZoneAuthorityTransitionFactsReady` at
  `CodegenIntentExecutionViewReady`; construction bridges may validate their
  newly built result before publication.
- Last legitimate consumers: source cross-seal and action-step C emission.
- Forbidden fallback: a mutable validity cache, a per-step full validator,
  duplicate row lookup inside binding, or any return to slot/type redecision.
- Gate/falsifier: the focused transition gate pins the four allowed validator
  owner files, admission-before-cross-seal order, one ready-row lookup in the
  action binding path, exact C/runtime parity, and three no-artifact negatives.

Reached evidence:
- After replacing the five new `-1` lookup/ambiguity sentinels with
  `Option<Int>`, `make -j1 self-host-compiler` completed and installed the
  Pergyra-built DRV-2 composition root. After restoring the diagnostic
  identity, the same serial build completed again. Current installed SHA-256:
  `92626852ADEB87B57DC238B61659F0B0118BCC3B6A32F32157176B470EE1EC86`.
- The focused transition gate passes with `ok=true`, `worker=2`, `gate=2` on
  self C, native C, and native LLVM. Source and direct-MIR C are byte-equal.
- Three admitted-MIR mutations fail closed: missing Zone authority, authority
  slot drift, and authority rows on a non-Zone declaration. The retired binding
  owners and tool entrypoint are deleted, and C emission is structurally
  forbidden from importing semantic Zone authority facts.
- The bootstrap source seed uses one typed DIR/MIR-declaration predecessor
  adapter to create the same sealed codegen receipt before a MIR artifact
  exists. This is not a final-emitter semantic fallback; operating production
  source execution reaches the MIR-owned receipt.
- The SoT registry gate reports 89 authorities / 188 derived carriers and
  `CLOSED=56 / BRIDGE=31 / ACTIVE=2`. The structural component contract passes
  2,435 line-cap requests; Intent no-recovery/compression also passes.
- Push CI run `35067525088` observed 29/30 jobs green; its only failure was
  fast Linux step 28 because the new bridge raised the likeness sentinel census
  from 20 to 25. Checkpoint `f148a59b` converted participant and transition
  lookups to `Option<Int>`, and published descendant `ebc0ca0d` then completed
  ordinary Push CI run `35071144950` SUCCESS with 30/30 jobs green. The focused
  likeness gate remains at its strict 20/20 ceiling without raising the
  committed baseline.
- The `Option<Int>` repair briefly split one defensive diagnostic into
  `semantic Intent placement participant is invalid`. Checkpoint `9aa730dd`
  restores the established `semantic Intent Zone placement identity is
  invalid` contract and extends the focused transition gate to reject both
  diagnostic drift and reintroduced `return -1` lookup sentinels. Published
  descendant `2b1d5f64` completed ordinary Push CI run `35078820012` SUCCESS
  with 30/30 jobs green in about 27 minutes.
- Before `19fe6b9e`, each source/action step could reach complete receipt
  validation through row lookup and then repeat the lookup for binding. The
  bounded follow-up leaves full validation in exactly four owner files: the
  validator definition, execution-view admission, MIR projection, and semantic
  seed projection. Ready consumers perform one row lookup, and a negative
  owner-census ratchet forbids validator escape back into per-step code. This
  changes the source-level call structure from an `S * N^2` family toward
  `N^2 + S * N`; no wall-time speedup is claimed without a pinned benchmark.
- `make -j1 self-host-compiler` completed with exit 0 and installed the
  Pergyra-built DRV-2 from the new source. Installed SHA-256:
  `E39FF951B63054753684715E0DC576C4CC0D21999782307AFD6557550C76FEFF`.
  Post-install transition parity, Intent compression, likeness, and the full
  component contract all pass. The component contract reports 2,435 line-cap
  requests; the likeness sentinel remains at its strict 20/20 ceiling.
- Ordinary Push CI run `35187450200` on `e79083b8` completed 29/30. Both
  self-host bootstraps, backend matrices, sanitizers, platforms, and proofs
  passed. `build-linux` failed only fast step 7 because the new validator-owner
  allowlist used `case` pattern line continuations forbidden by the repository's
  Bash 3.2 portability inventory. Checkpoint `8f783706` gives each allowed path
  its own case arm; the focused transition gate and the MSYS2/UCRT64
  `build_source_inventory_smoke.sh` both pass locally. Exact repaired-head
  remote CI remains pending.
- The accumulated Future aggregate packet passes its native/public MIR+C+LLVM
  no-artifact gate, and the broader structured-spawn lifecycle gate passes on
  both C and LLVM. Those results establish the reached storage-admission slice,
  not a separate SoT-row closure.

Dirty-state boundary and next falsifier:
- The Zone closure, Future aggregate packet, audits, and their executable gates
  are published in `25722b7c`/`60321012`; the bounded publication-CI repair and
  diagnostic preservation are published through `2b1d5f64` with exact-current
  green CI. One-pass ready-snapshot consumption is published in `19fe6b9e` and
  its Bash 3.2 test repair is committed in `8f783706`. Preserve all unrelated
  edits; no reset or clean is authorized.
- Keep the six Slice edits outside this publication:
  `array_type_shape_owner.pgy`, the three expression graph type owners,
  `builtin_signature_owner.pgy`, and `self_host_pergyra_likeness_smoke.sh`.
  A direct `slice_copy` source probe on the rebuilt installed driver still
  fails closed at `SemanticAstInitializerTypeFacts` with
  `ast_artifact_invalid`; no self/native runtime parity is claimed for it.
- No successor executable rung is opened in this handoff. After publication,
  select the next reached BRIDGE from current production execution evidence;
  do not revive an older queue merely because it appears below this boundary.
- Push `8f783706` plus this checkpoint, then observe ordinary remote Push CI
  green for the resulting revision. Run `35187450200` proves 29 unaffected
  jobs and isolates the portability failure, while local gates alone are not
  green remote-CI evidence for the repaired descendant.

## Historical archive boundary — previous Zone/spawn checkpoint

Everything below this heading is lookup evidence, not an active work queue.

### Zone/spawn installed admission green, Zone BRIDGE still open

Current HEAD and `origin/main`: `ebc9f28720c77ffee6b9773bb565a2effa218879`.
Ordinary Push CI on `aeccbd64` failed only `build-linux` step 27 (run
`34968351911`): the `dir.domain_graph` plan-consumer pin still required a
retired direct C/LLVM success sentence. The registry pins were corrected in
`ebc9f287`; the full CI dispatched on that exact HEAD (run `34981095622`)
completed SUCCESS in 43m45s with 30/30 jobs green. Linux `build-linux`
completed in 23m24s and its fast Push runner reported all 27 steps ok,
including the Zone/spawn gate and SoT authority-edge census. Registry status
is unchanged at CLOSED 55 / BRIDGE 32 / ACTIVE 2.

Objective card:
- Objective: stop a Zone value/reference from crossing a spawn worker boundary
  before source-MIR or C/LLVM artifact publication. Native admission previously
  accepted both modes; the old public driver accepted `ref Zone`, and an `own
  Zone` refusal produced no public receipt. A `shared` Zone field also failed
  silently in the public parser before reaching semantics.
- Priority: admitted target/type identity, one Zone transport refusal, exact
  public diagnostic receipt, no backend artifact, ordinary scalar-spawn
  preservation, then installation/performance evidence.
- Fact owners: indexed Zone declaration in native semantics; Pergyra
  `decl_zone_owner.pgy` for `shared` field AST facts; resolved expression call
  identity plus `ast_zone_spawn_transport_verdict_owner.pgy` for the source-MIR
  admission verdict; public diagnostic code/receipt owners for its wire fact.
- Last legitimate consumer: body-type bundle after expression identity
  resolution and before MIR emission; native semantic spawn boundary before
  backend projection.
- Forbidden fallback: copying a Zone's synchronization/identity storage into
  a C/LLVM worker wrapper, treating an ordinary synchronous `ref` or `own`
  parameter as an admitted worker transport plan, or exiting without an owned
  public receipt on a missing call target or refused Zone argument.
- Gate/falsifier: `zone_spawn_transport_admission_owner.sh` checks four
  subject-slot/shared × `ref`/`own` refusals on native C/LLVM and the public
  source-MIR route, zero negative artifacts, scalar-spawn admission, and a
  mutated missing target syntax identity with an exact receipt.

Reached evidence:
- The isolated native compiler and a Pergyra-built candidate from the actual
  installed composition root `driver_bootstrap_main.pgy` pass the focused
  gate. Candidate SHA-256:
  `86BC7B763A74A90BD119419D834DA5CAB5B2C5D675DE7F0E255F8929C4E55C04`.
  Six further valid async/generic spawn controls pass on that root.
- The receipt-bound installer rebuilt `bin/pgy-self-driver.exe` from the same
  production composition root. The exact Make target
  `self-host-zone-spawn-transport-admission-test-smoke` rebuilt the native
  compiler and codegen seed, then reinstalled and reran the focused gate:
  PASS. At that published packet checkpoint, installed SHA-256 and artifact
  receipt agreed on
  `B4BAFBF7534494CA5A426EB92BA7FEC9D18418A17CCD64CB74EE4A910F282BA1`.
- The structural component contract passes 2,427 line-cap requests, including
  the extracted Zone spawn owner and a separately sourced one-MIR dual-backend
  case verdict owner. The dual-backend C/LLVM positive and mutation gate passes
  after that split; `git diff --check`, shell syntax, Make target
  dry-run and execution, build-source inventory, and CI profile checks pass. The first CI
  profile invocation had no Git in its MSYS PATH; a rerun with the existing
  host Git path passed. The focused gate is wired into fast Linux Push CI,
  and `aeccbd64` remote fast Push observed it PASS before the later SoT pin
  failure. The `ebc9f287` full-CI dispatch then observed 30/30 green.
- This closes the reached parser-to-semantic and Zone-spawn diagnostic/admission
  seams, not two whole SoT rows. `selfhost.zone_authority_rows` retains legacy
  self-C intent binding and shared zone-sync/runtime work; `diagnostic.catalog`
  retains other diagnostic consumers. No C-owned compiler path was deleted and
  the old native spawn-wrapper transport path still exists behind source
  admission. Before installation, the old driver exited 0 on the tracked
  `zone_spawn_ref_boundary_rejected.pgy` falsifier; the new installed driver
  rejects it with an owned receipt. This is installed REACHABLE evidence for
  the exact semantic path, not whole-root SUBSTITUTING or registry CLOSED.

Dirty-state boundary and next falsifier:
- The code/test packet is committed and pushed. Six unrelated `Slice<T>`
  semantic/test edits and two concurrent audit-document edits remain unstaged
  and untouched. The separate SoT pin repair is also committed and pushed.
  The later dirty Future override below adds its own isolated source/test edits.
- The reached `selfhost.zone_authority_rows` BRIDGE was audited, not closed.
  `zone_authority_fact_owner.sh` still passes its admitted identity/DIR
  no-rescan gate, and `intent_step_binding_contract_owner.sh` still executes
  the legacy self-C binding with actor/authority/where/slot negative cases.
  The live call chain is `intent_action_step_emit_owner.pgy` →
  `intent_step_binding_owner.pgy` → `intent_emit_owner.pgy` →
  `program_emit.pgy`; no C-owned compiler bypass was deleted. Production MIR
  authority transition and a shared Zone-sync runtime plan are still absent.
- Next falsifier: add one production intent input with distinct actor and
  `authorized by` subject in a Zone with two subject slots, then drive the
  installed source/MIR-to-C entrypoint through one MIR-owned authority/slot
  transition. Require distinct identities, missing-slot refusal, and no old
  emitter reachability. The existing intent-step probe only executes the
  legacy self-C binding fact; `dir_intent_defaults.pgy` uses the same `hero`
  for actor and authority, so neither proves the missing MIR transition.
  The published Zone/spawn negative gate has since been observed in remote
  fast Push and the green full-CI run; the next rung is the MIR transition.
  Only after a reached C-owned bypass is deleted and all
  registry-row consumers/negative gates migrate may a BRIDGE row be marked
  CLOSED. External MIR Zone resource legality and a real worker handoff plan
  remain separate, unverified successors; do not infer them from this gate.

Future aggregate safety override — dirty, installed parity for reached forms,
not self-host substitution:
- Objective card: reject a physically stored affine Future/RemoteFuture before
  MIR or backend artifact publication. Prefer semantic identity and no artifact,
  then native/public diagnostic parity, then ordinary direct-Future/value-array
  preservation. The native and Pergyra admission owners named below own the
  fact; body admission is the last consumer. A backend copy, native retry, or
  generic rejection misreported as storage proof is forbidden. The focused
  Make gate and its positive value-array control are the falsifier.
- HEAD and `origin/main` remain `ebc9f287`. The earlier 30/30 remote CI proves
  that published HEAD, not the present dirty source packet. Existing Slice and
  audit edits were preserved; this packet is unstaged and unpushed.
- The native C and LLVM pipeline previously emitted the reproduced
  `Array<Future<Int>>` alias/double-await source. A bounded known-storage and
  nominal-field classifier now rejects that source, `Option`/Set/Map stores,
  empty typed arrays, concrete and generic nominal fields (including nested
  and default-initialized fields), enum payloads, and aggregate
  parameter/return signatures. Native phantom type arguments with no stored
  Future field remain admitted.
- Native owner: `type_checker_future_lifecycle.c`; Pergyra owner:
  `ast_future_storage_admission_owner.pgy`. Their last source consumers are
  aggregate literal/local binding, nominal field or enum payload, and callable
  signature admission before MIR emission. No backend copy fallback was
  introduced and no C-owned self-host bypass was deleted.
- The receipt-bound bootstrap rebuilt the installed production composition
  root after the owner extraction. Current `bin/pgy-self-driver.exe` SHA-256:
  `26EEC380BD21C0D5C0CB91A22FC51F912E0DF12F0B38EC2CA0E434A021D4CF0E`.
  The direct installed driver rejects eight reached array/Option/Set/nominal/
  signature/enum cases on its owned stdout channel with
  `PGY_SEM_TASK_LIFECYCLE`, `semantic:task:lifecycle`, and
  `await-task-before-exit`. The public C and LLVM array route relays that JSON
  body byte-for-byte, does not retry native admission, and publishes no
  artifact. Native C/LLVM independently reject the array and enum cases with
  the same identity and no artifact. A direct Future beside `Array<Int>`
  remains admitted and carries both MIR ABI types.
- Focused Make target
  `self-host-future-aggregate-storage-admission-test-smoke` passes in about six
  seconds against the already installed driver. It is wired after the Zone
  gate in fast Linux Push; the next dirty-source Push run would therefore have
  28 steps, but no remote run contains this uncommitted gate yet. The broader
  native `structured-spawn-lifecycle-test-smoke` passed C+LLVM, semantic tests
  passed 2,944/0, the self-host component contract passed 2,428 line-cap
  requests / 1,029 function extractions / 694 reuses, and CI-profile and build
  inventory gates passed. Full dirty-source CI was not run.
- This does not close all generic storage parity. The installed self-host still
  reaches unrelated generic-constructor type errors on both stored-Future and
  positive phantom controls, and the Map spelling is rejected earlier by its
  parser. Those are explicit predecessor gaps; do not infer physical-storage
  discrimination from their rejection. External-MIR resource legality also
  remains a separate successor.
- The active self-host executable rung remains Zone authority/slot MIR
  transition and legacy self-C binding deletion; this source safety override
  does not close a SoT row or promote whole-root dogfood.

CI repair override, not a second self-host substitution rung:
- Linux `build-linux` fast Push step 27 reached
  `tests/sot_authority_edge_smoke.sh` and failed because the registry still
  cited `domain_topology_graph_plan_consumer_owner.sh` as publishing one
  target-neutral direct C/LLVM multi-declaration plan. Its executable contract
  now publishes and executes one general-C BattleZone plan, rejects a forged
  player-name/enemy-ID edge without an artifact, and explicitly refuses the
  unsupported direct C/LLVM multi-declaration route. The retired success pin
  was replaced with that exact current marker, not reintroduced as a comment.
- The next SoT edge failure was `mir.execution_graph`: the one-MIR positive
  corpus and its output marker moved from `one_mir_dual_backend_projection.sh`
  to the sourced `one_mir_dual_backend_case_verdict_owner.sh`. The registry
  enforcement pin now points to the sourced owner; the projection entrypoint
  still reaches it and its C/LLVM positive and mutation gate passes.
- `tests/sot_authority_edge_smoke.sh` passes 89 authorities and 187 derived
  fact carriers after both pin repairs. Both pinned executable gates pass on
  the installed local binaries. The whole local
  `self-host-preparation-contract-test-smoke` target exited 0 with
  `PGY_ALLOW_MISSING_COQ=1`; its Rocq/Coq model was a declared local SKIP,
  while the previous remote formal-proofs job passed. Do not count the local
  SKIP as proof execution. The `ebc9f287` full-CI dispatch proved this
  published HEAD green remotely, including the Rocq 9 job, 20 backend-compare
  shards, Windows, macOS, sanitizer, TSAN, and both self-host bootstrap jobs.

Explicit test-harness repair override, not a second self-host substitution rung:
- Three named native C oracle call sites now request `--native-pipeline` locally;
  the mixed self-host candidate remains unchanged. The Push Linux
  `gate_subject_declaration_smoke.sh` checks all three and rejects synthetic
  missing/comment-only flag controls. `protocol_registry_smoke.sh` also runs
  on every Linux Push, not only Markdown changes. Local versions of these two
  gates and `self_host_ci_profile_smoke.sh` pass; `aeccbd64` remote fast Push
  reached them before its later SoT pin failure.
- `mir_json_parity.sh` now allocates a fresh work directory per invocation and
  compiles generated C directly with its owned runtime include path. Its
  `PGY_SELFHOST_MIR_FIXTURES=hello` run passed 1/1, comparing reconstructed
  C against an independently selected native C oracle. The 118-fixture full
  matrix was not run. `mir_json_coverage_probe.sh --limit=1` uses a fresh
  directory and runtime-header flags and observed `if_else PASS`; it is a
  boundary probe, not an acceptance gate. The third oracle leg's positive
  CFG case executed native/C/LLVM with identical `pos` output, but the full
  `one_mir_cfg_air_plan_projection.sh` remains RED after that positive case.
  Stale loop C/LLVM spelling pins were removed; the reached falsifier is now
  `loop_summary_kind`: changing the MIR LoopFlowSummary kind from `while` to
  `for` leaves direct C and LLVM artifacts byte-identical to the baseline,
  although the MIR bytes differ. Both direct backends accepted it. The
  routine-lower owner has a summary-to-CFG projection check; direct admission
  has not been proved to carry that correspondence. Keep the negative gate
  red; do not relabel this unsupported fact drift as a valid metamorphic case.
- A literal test-script reachability ratchet reports 814 scripts:
  756 target-reachable and 58 individually declared manual, with zero
  undeclared/stale/dual rows; two manual concept scripts cited in the SoT
  registry are explicitly historical, not current CI evidence. The new
  Platform-full Linux `harness-evidence` shard ran all 16 formerly dormant
  evidence steps through a collecting runner and passed 16/16 locally. The
  shard no longer rebuilds the full installed driver inside receiver/topology
  gates. A second local 16/16 run observed the revision and installed
  compiler/driver/manifest SHA-256 print at shard entry. Remote CI remains
  unobserved. These source-graph and local-shard facts are not proof
  that every manual or target-reachable script ran.
- At the test-harness packet checkpoint, installed driver SHA-256 was
  `B4BAFBF7534494CA5A426EB92BA7FEC9D18418A17CCD64CB74EE4A910F282BA1`
  and machine-layer manifest SHA-256 was
  `0A83B0DB5EFE3C00C6D9413C63045C4B17AFF079781213B280442C588E5A9C19`.
  The later Future bootstrap supersedes the installed-driver artifact with
  `26EEC380BD21C0D5C0CB91A22FC51F912E0DF12F0B38EC2CA0E434A021D4CF0E`;
  current native compiler SHA-256 is
  `777F84A323FE3BE519BF60E56E763B955516F330D195A50530CD90686D12744F`.
  HEAD and `origin/main` are `ebc9f287`; preserve the unrelated Slice and
  audit edits. No dirty Future changes were staged, committed, or pushed.
  Next test-harness falsifiers remain the full CFG gate's
  LoopFlowSummary/CFG correspondence at its direct-MIR admission owner and a
  remote Push/Platform-full run only after an authorized published revision.

## Historical archive boundary

### Archived self-host context — MIR root/InstructionId focused admission green

Green code/documentation checkpoint HEAD and `origin/main`:
`09d514cbe0eb9dced9a9755bd8b69a179b7b5a90`. This handoff refresh is a
docs-only descendant; consult Git for its own commit ID.
Executable repair commits: `c268b695cdecfd1dcbb8835874ac8fa779be4627`
and corrective `cc3a21ef7fcfc87b445b1f72fde9c63a263b8d21`.
Published red checkpoint `87d003fee03d601813e405bf9da0898f183f9ac2`;
ordinary Push CI `34932122840` failed in `build-linux` and
`self-host-bootstrap-linux`. Corrected ordinary Push CI `34935370223` on
`09d514cb` completed 30/30 success; the full self-host job proved
`gen2 == gen3 (190082 lines)` and the policy corpus `3 in_subset / 0
out_of_subset`.

Objective card:
- Objective: prevent the reproduced root-syntax residue and duplicate
  routine-local `InstructionId` values in external `pgy.mir.v1` from reaching
  either direct backend publication boundary.
- Priority: exact document grammar and EOF, routine-scoped ID uniqueness
  without false density, explicit refusal, no artifact publication, then
  focused C/LLVM projection and existing installed parity gates.
- Fact owners: `BuildMirDocumentFactIndex` owns the root grammar boundary;
  `program_instruction_identity_owner.pgy` owns canonical nonnegative,
  routine-local ID uniqueness over program-index bounds. Sparse IDs remain
  valid when an oracle comparison erases a synthetic Void exit.
- Last legitimate consumer: machine-layer admission immediately before the
  direct C/LLVM projectors.
- Forbidden fallback: skipping commas independent of parser state, accepting
  bytes after the root object, treating physical instruction order as identity,
  requiring dense numbering after an erased instruction, or publishing an
  artifact from a partial parse.
- Verification/falsifier: one current valid scalar MIR must project to both C
  and LLVM, including a valid sparse-ID control; leading-root-comma,
  extra-root-close and cross-block duplicate-ID mutations must fail on both
  backends with the owned diagnostic and no output artifact. Existing enum,
  role-override and nested-intent gates falsify overstrict ID admission.

Reached repair and observed evidence:
- The pre-repair candidate accepted all six malformed-input/backend pairs and
  published all six artifacts. The current seed still succeeded on both
  backends, so the reproduction did not rely on the stale September 5 seed.
- Root parsing enforces member/comma state and whitespace-only EOF. The first
  ID owner wrongly required a dense `0..N-1` permutation. CI exposed native
  oracle normalization and a missing-priority negative that preserve unique
  but sparse IDs. `cc3a21ef` checks canonical nonnegative uniqueness after
  routine-local sorting; physical row order remains non-authoritative.
- A Pergyra-built DRV-2 was rebuilt from the isolated `cc3a21ef` source graph.
  SHA-256:
  `99E211EA177998A3E7C6A7BB5DE0ACF33F760F4CDDA3FC292C861B12185A57FB`.
- The corrected `direct_mir_document_admission_owner.sh` passes: four valid
  C/LLVM controls (including sparse IDs) and six owned refusals with zero
  negative artifacts. The ID mutation collides instructions in two CFG blocks.
- The isolated installed enum-variant/builtin collision, role-override MIR,
  and nested-intent C/LLVM parity/negative gates all pass, matching the three
  remotely reached failing surfaces. `git diff --check`, shell syntax and
  Python syntax checks pass; all four corrective source/test files are
  SHA-256-equal between main and the isolated source graph.
- The prior isolated structural component contract passed all 2,424 line-cap
  requests before the correction. The corrected isolated invocation stopped
  at its installed-frontier dry-run because this new worktree lacks the native
  `build/pgy.exe.rsp`; do not call that isolated invocation green. The exact
  corrected remote `build-linux` run did pass its 2,424-request component
  contract, role-override, and enum-variant/builtin collision gates. The full
  remote self-host run passed nested-intent LLVM/C parity and twelve
  no-artifact negatives.
- The routine-index executable fixture passes through C. Its LLVM build
  succeeds but the executable exits with Windows status `0xC0000374`; an
  untouched `bf329e99` worktree produces the same status. This is retained as
  pre-existing LLVM fixture evidence, not attributed to this patch and not
  relabelled green.
- No Platform full, Self-host parity or exhaustive language-word matrix was
  run. The review's Future-aggregate and Zone/spawn findings, plus broader
  external-MIR reference/CFG/SSA/resource legality, remain separate unverified
  successor rungs.

Dirty-state boundary:
- The first packet was published as `c268b695` plus documentation `87d003fe`;
  corrective executable delta is `cc3a21ef` plus documentation `09d514cb`.
  Corrected ordinary Push CI is green. This final navigation refresh is
  documentation-only and does not alter compiler semantics.
- Six unrelated `Slice<T>` edits remain in five semantic owners plus
  `tests/self_host_pergyra_likeness_smoke.sh`. Preserve them and do not stage,
  reset or fold them into this packet.
- Next action: choose exactly one resource-boundary successor (Future aggregate
  or Zone/spawn) with its production entrypoint, old bypass, fact owner and
  falsifying fixture before implementation. Do not rerun exhaustive verification
  in this edit loop; the broader external-MIR legality cases remain separate.

### Archived self-host context — exhaustive evidence retained; ordinary push green

Last fully verified code HEAD: `1256f68d4a8e33268dad2d7f5c6518774273a505`.
The cadence commits `23dbd6a7` and `6b68940d` keep the exhaustive
146-language-word inventory out of ordinary push CI and reserve it for an
explicit major-patch/manual run or a `v*` release boundary.

Objective card:
- Objective: close the three exact LLVM tails exposed by the single exhaustive
  Self-host parity run `34877515733`, publish the repair, and return to the
  short ordinary push loop without rerunning that exhaustive matrix.
- Priority: exact owned-value consumption, program-global local identity,
  runtime-value lifecycle, focused executable falsifiers, then ordinary CI.
- Fact owners: generic function emission owns its `Array<String>` consuming
  return; the indexed-assignment route owns routine-local to program-local row
  translation; `ParseDecls` owns its fatal source-location precondition before
  allocator creation and the runtime-value lifecycle owner verifies the single
  terminal consumer.
- Last legitimate consumers: the sealed direct-MIR C/LLVM projectors after
  GraphPlan readiness.
- Forbidden fallback: a builtin drop impersonating a declared consuming-call
  identity, an unoffset routine-local row, an `Exit` path after allocator
  creation, a raised timeout, or another exhaustive run for this edit loop.
- Verification/falsifier: the three focused C/LLVM parity/negative gates, then
  one exact 3,480-routine `driver_rung0_main.pgy` MIR-to-LLVM projection and
  LLVM link. The next external falsifier is ordinary Push CI on the publication
  commit; the exhaustive matrix remains deferred to the next major boundary.

Reached repair and observed evidence:
- The exhaustive-tail repair was published as `3d680267`. Ordinary Push CI
  `34911681025` kept the keyword inventory out as intended. Twenty-eight jobs
  were green when `build-linux` reached its only failure: the new parser guard
  made `decl_dispatch_owner.pgy` 601 lines against the existing 600-line cap.
- The guard is now expressed in 599 lines without changing its condition or
  ownership. `self_host_preparation_smoke.sh` passes, and the focused
  `direct_mir_scalar_runtime_value_lifecycle_owner.sh` C/LLVM execution and
  negative gate still passes.
- Publication `5162704a` reached ordinary Push CI `34913593556`. The parser cap
  and CI-profile isolation passed; the keyword inventory did not run. With 28
  jobs green, `build-linux` then exposed the next and only failure: the focused
  local indexed-assignment parity script was 93 lines against its 90-line cap.
- Three blank separators are removed without deleting a check, making the
  script exactly 90 lines. Its C/LLVM execution and ten negative mutations
  pass with the rebuilt candidate self driver. The complete component contract
  passes all 2,423 line-cap requests, and the impact-manifest and substrate
  tail gates also pass.
- Ordinary Push CI `34915847404` completed 30/30 green on `1256f68d` in
  26m41s. The exhaustive 146-language-word job was absent. No Platform full,
  Self-host parity, or replacement exhaustive run was dispatched, so the
  earlier one-shot exhaustive workflow is not being relabelled all-green.
- `EmitFunctionSet` now sends its empty specialization list through
  `CodegenJoinOwnedStringFragments`, preserving the declared `own Array<String>`
  consuming identity on every return path.
- Local `Array<Int>` indexed assignment now adds the routine `local_offset`.
  Its fixture includes a preceding routine with a stored local so offset zero
  can no longer make the gate false-green.
- `ParseDecls` validates missing script source locations before either
  `AllocatorResult` call. The normal region retains exactly one destroy for
  each allocator; no conditional duplicate terminal was introduced.
- The final rebuilt Pergyra driver passes
  `direct_mir_owned_array_string_terminal_flow_owner.sh`,
  `direct_mir_scalar_local_array_int_indexed_direct_call_owner.sh`, and
  `direct_mir_scalar_runtime_value_lifecycle_owner.sh`, including C/LLVM
  execution and their negative mutations.
- The exact 127,426,857-byte, 3,480-routine MIR rooted at
  `driver_rung0_main.pgy` projected to a 29,217,065-byte LLVM artifact with
  exit 0 and linked with the runtime into a 30,957,237-byte Windows executable.
  Runtime compilation reported six existing Clang 22 `ATOMIC_VAR_INIT`
  deprecation warnings; they are not failures and were not changed here.
- The one remote exhaustive run `34877515733` already completed. Its language
  inventory and earlier ledgers passed before it exposed these three successive
  LLVM tails. It is intentionally not rerun here, so this card does not claim
  a new all-matrix green result. Earlier Push CI `34867733174` and Platform full
  `34870863078` were green on the parent cadence state, not on this unpublished
  repair.
- A pressure run against `driver_bootstrap_main.pgy` produced a distinct
  7,870-routine input and reached an unrelated generic inventory refusal. It is
  not the failed CI's rung-0 input and is excluded from this repair verdict.

Dirty-state boundary:
- The two CI-cap repairs are published as `5162704a` and `1256f68d`; this final
  navigation refresh is documentation-only. Generated test artifacts remain
  ignored under `.tmp`.
- The main worktree still has six unrelated Slice-related edits in five
  self-host semantic owners and `tests/self_host_pergyra_likeness_smoke.sh`.
  They remain preserved and must not be staged, reset or folded into this
  publication.
- Next action: return to focused executable-rung work. Do not dispatch Platform
  full, Self-host parity, or the exhaustive language-word inventory again until
  an explicit major-patch or `v*` release boundary.

### Archived self-host context — close the reached tail, then run exhaustive once

Pre-publication HEAD/origin/main: `3daf318f42b213c23cae4bc6e1d4557a156a4c10`.
Reached-tail repairs were published as `713be04b` and `3daf318f`; this card
accompanies the next CI-cadence commit, whose identity Git will own.
The parent Push CI `34810531883` and Platform full `34810549151` are green.
Self-host parity `34810551045` ran for 2h07m and failed in the LLVM projection
of `LexerTokenFactsReady` at the record-array value-parameter boundary.

Objective card:
- Objective: close each semantic boundary reached by that exhaustive run with
  owner-carried facts and focused C/LLVM falsifiers, then execute the exhaustive
  proof once on the final major-patch SHA.
- Priority: exact semantic identity, typed predecessor carriage, missing-fact
  refusal, focused executable parity, ordinary CI, then one exhaustive run.
- Fact owner: callable envelopes own logical-record array parameters; the CFG
  owners carry payload-free enum phi facts; the indexed-assignment route owns
  target/value predecessor use; expression graph topology owns whether a leaf
  is a lexical binding or a member name.
- Last legitimate consumers: the C/LLVM logical-record, enum and array mutation
  projectors after graph-use completeness admission.
- Forbidden fallback: spelling allowlists, backend-specific acceptance,
  reconstructed linear predecessor state, member-name lexical binding, skipped
  rows, raised timeout/cap, or repeated whole-program projection per edit.
- Verification/falsifier: the five focused parity gates below must pass on the
  rebuilt Pergyra driver; remote Push CI and Platform full must be green before
  Self-host parity is manually dispatched exactly once on the final SHA.

Reached repair and local evidence:
- Logical-record array parameters now carry a typed by-value callable envelope.
  Payload-free enum values carry phi identity through the CFG. Mixed
  `Array<Int>`/`Array<Bool>` record writes and local indexed writes use the new
  owner-directed predecessor route in both backends.
- `BuildStateFromFormal(last_row)` falsifies the same-spelled field/formal case.
  The producer leaves the member leaf unbound and binds only the RHS formal;
  forged member binding is refused by both C and LLVM projectors.
- The record-array, payload-free-enum, mixed record-array assignment, local
  indexed assignment and branch-member-rebind C/LLVM parity/negative gates pass.
  Expression identity carriage, component contract, owner-size and CI-profile
  gates pass. A Pergyra-built DRV-2 was rebuilt and installed; the existing CFG
  unreachable warning remains one warning and zero errors.
- The 127,520,663-byte production MIR was projected twice while locating the
  reached seam. The second run advanced to row 43,522 and exposed the exact
  member/formal collision. It is not repeated after every edit.
- Exhaustive Self-host parity no longer has a weekly schedule. It remains a
  manual major-patch and `v*` release boundary with the existing 180-minute cap;
  the CI-profile gate rejects restoration of branch or calendar triggers.
- Push CI `34853512173` has 28 green jobs, one still-running full bootstrap,
  and one red `build-linux` job. Its only reached failure is the generated
  language-word implementation inventory after the new focused fixtures changed
  evidence counts. The ordinary push and Markdown-only paths no longer run that
  exhaustive 146-row inventory; the manual/release exhaustive parity target
  owns it exactly once. The other platform-independent contract gates remain in
  push CI.
- Cadence publication `23dbd6a7` reached Push CI `34857142986`. Its profile
  gate passed and the keyword inventory did not run, but the component contract
  rejected the exhaustive Make target because adding the new prerequisite at
  the start changed its pinned header prefix. The prerequisite is moved to the
  end of the same target header, preserving both the established component
  contract and exactly-one major-patch inventory execution.
- Header repair `6b68940d` reached Push CI `34860319497`. The component
  contract and CI profile passed and no language-word inventory ran. The next
  fail-closed edge was the stale callable-owner fingerprint in
  `selfhost_source_scan_owner_evidence.json`, caused by the reached
  member/formal identity repair. The source-only fingerprint is refreshed;
  historical performance evidence remains explicitly unremeasured.
- The inventory was regenerated through its owner and the one requested local
  major-patch run passed: 146 rows, 70 reserved lexer rows, 76 parser selectors,
  nine fixtures, and no dead reserved spelling. CI-profile, documentation-quality,
  and diff checks pass after the cadence split.
- Publication attempt `713be04b` reached Push CI `34851517788`; its full
  self-host bootstrap rejected reserved local names `use` and `local` in the
  new indexed-assignment owner. They are now `use_fact` and `local_fact`. The
  focused gate first native-compiles that owner, then runs self-host MIR and
  C/LLVM parity/negative cases, so this parser split fails in the short lane.

Dirty-state boundary:
- Existing uncommitted Slice-related edits in five self-host semantic owners
  plus `tests/self_host_pergyra_likeness_smoke.sh` remain preserved and outside
  this repair packet. They must not be reset, folded into the publication, or
  treated as verified substitution progress.
- Next action: publish the cadence repair, observe ordinary CI, and manually
  dispatch the one exhaustive Self-host parity run only after the publication
  SHA is stable.

### Archived native CI integration repair — 2026-09-09

Updated: 2026-09-09 (Asia/Seoul), native CI regression repair; navigation only.
Compiler owners, registries and executable gates override this snapshot.

#### Archived self-host context — native CI integration repair

Pre-publication HEAD/origin/main: `975b703a1069dfcddb235b8a221bd1d7f4555bdb`.
The user authorized correction, commit and push after failed CI. This card
accompanies the next repair; Git owns its publication identity. Primary-only
[edit lease](current_work_collaboration.md) and
[objective/owner boundaries](agent_work_directives/source_admission_parity_2026-09-07.md).
No shared installation, skipped job, raised cap or native-built replacement driver.

Observed remote run `34294460519` is RED, now completed. All non-backend-shard
jobs passed, including Linux preparation and the full self-host bootstrap:
gen2 == gen3, 189,016 C lines (`.tmp/ci-975b703a-bootstrap.log`). Seventeen
of twenty backend shards failed on 36 distinct native C/LLVM cases; the earlier
shared inventory stop is gone. This packet repairs those reached cases.
It is native correctness/CI work, not new Pergyra substitution progress.
No SoT registry status changed: last classification remains
CLOSED=55 / BRIDGE=32 / ACTIVE=2, not a newly remeasured closure percentage.

Reached facts and consumers:
- RIR resource rows retain effect/ABI evidence. Lexical DEF/STMT expressions
  own executable reads; only explicit with-scope resource operations keep
  direct resource-row uses. Delete obsolete entry-summary borrow alias emission.
  LLVM's exact SSA-storage refusal remains in force.
- Positional destructure binding IDs project into SSA output names, liveness,
  use validation and LLVM storage. The projection does not reread the statement
  AST. Missing/crosswired IDs refuse; no late spelling-based storage recovery.
- Select receive bindings carry their checker-owned type. For-in desugaring
  extends missing AST identities after growth without renumbering existing
  nodes, destructure bindings or delayed formal IDs.
- Builtin/stdlib callees retain an admitted nonlocal target fact even before a
  later same-spelled callable local. A local callable still requires its ID.
- Abstract ability signatures join role-admitted implementation equations at
  the callable capability seal, including callable actuals. This is a
  conservative implementation union, not source-order binding speculation.
  Missing facts do not imply purity; narrow caps/effects still refuse.
  The fixed point and dispatch projection share one private equation store.

Observed verification:
- Publication rebuild passed. Native MIR 215/0, semantic 2,944/0, C transpile
  978/0 and AIR 145/0 (`.tmp/ci-975b703a-final-{mir,semantic,transpile,air}.log`).
- Reached regression set: 36/36 native C/LLVM executions passed, twice
  (`.tmp/ci-975b703a-reached-36-final.log`). The second run used native
  SHA-256 `08e33d81bd2b8ba84a86ebf793c266cfc64824d900ec6ce7cf8029f0ba3f1504`;
  only test-fragment consolidation and equivalent bool declaration formatting
  followed before the publication rebuild, not a semantic behavior change.
- Source-admission gate: 50 claims / zero failures with the Pergyra-built driver
  (`.tmp/ci-975b703a-source-admission-final.log`).
- Stable identity, ABI ownership, CFG body/dataflow, lexical identity, AIR drift
  and HIR routine identity gates passed. CFG was also rerun with the explicit
  publication native candidate; its six-program non-CFG corpus passed.
- Fragment inventory returned to 145/145 without raising its cap or removing
  any test invocation. MIR/AIR cases from three small fragments were merged
  into their existing family files; source pins moved with them. Production
  header, backend and shared size gates passed. The complete perf contract
  passed (end-to-end C compile: 2,272 ms under concurrent build load; not a
  compiler-scale self-host performance claim). Static shell wall time exceeded
  the 60-second edit-loop budget: thousands of separate grep processes/read
  passes remain a tooling cost, not compiler substitution.
- Old perf pins were migrated from deleted borrow/assignment/generic emission
  paths to their current fact consumers, with old-path rejection retained.
- Makefile source inventory and source UTF-8 gates passed. A transient handoff
  write failure left the file intact; a subsequent apply_patch succeeded.
  D: had 7,093,014,528 bytes free at that check; no cleanup was performed.

Current native: `.tmp/ci-34251704201-native/pgy.exe`.
SHA-256: `2E254FCBF6DE4A018B60AF13BD927249DA4BB51082613B4E2F4C6D41A34A5BDE`.
Pergyra-built driver: `.tmp/ci-34280162606-driver/pgy-self-driver.exe`;
SHA-256: `FBF4F91DEAB683959B4FFDECF01A50C40C8936F505C408F91727B5A2F7EC679E`.
Its sibling in the isolated native directory has the same hash. Pergyra source
did not change in this repair; no new local full-driver fixed point is claimed.

Review remeasurement is in the existing
[word-deletion audit](audits/2026-09-06_language_word_deletion_execution_matrix.md).
On the measured repaired native candidate (08e33... above), 104 source-to-MIR
programs gave 78 admitted/admitted, 19 refused/refused and seven native-admitted/
public-refused, with zero reverse differences and zero operational failures.
Nine classifier controls passed; no census input was executed. The seventh
difference is restored native dynamic-role admission, exposing the driver's
existing `Team()` arity refusal. This is not the historical runtime metric.
Binary evaluation-order observations remain 10 passes / 2 public-LLVM Intent
refusals at legacy Main identity/envelope admission, not general call-order closure.

Next falsifier: the first failing job on this repair's push CI. Full Linux/
macOS/Windows, sanitizer and gen2/gen3 matrices must run on the new commit;
the previous remote green jobs are not this packet's green evidence. Keep the
same one executable CI rung until that boundary is observed.

Parked gaps remain explicit: generic call occurrence 70 checks / 2 failures;
capability admission 53 / 7 (public clock/Now execution and synthetic-body HIR);
the false-loop `let v: Int = await t;` residual native MIR rejection; the
seven supported-input census differences and two Intent public-LLVM observations.
Their previous logs are under `.tmp/ci-{baseline,verified,option}-*` and
`.tmp/review-975b703a-*`. These were not repaired by the native CI packet.
Future/Zone aggregate carriage, full Intent GraphPlan, external MIR admission,
physical evidence-memory accounting and research proposals are parked, not a
parallel implementation queue. The September 9 review's cited research has
not been independently verified here.

### Parked source-admission evidence — lookup, not a parallel work queue

Pre-publication HEAD/local origin/main: `5b97f2e10ffa7ecf9cfe932829a83ffffaa3ba12`.
On 2026-09-09 the user explicitly requested commit/push now to inspect CI.
This supersedes the earlier publication hold, not the remaining language,
bootstrap or integration obligations. This snapshot accompanies the pending
source changes; use Git for its eventual commit identity. No shared install.
Read the [edit lease](current_work_collaboration.md) and
[objective card](agent_work_directives/source_admission_parity_2026-09-07.md).
Primary only; no new syntax, research lane or parallel implementation lane.

One active rung: driver source-LLVM purpose -> admitted MIR ->
existing Intent/GraphPlan owners -> ordinary Main composition. Do not replace
this with another independent SoT cleanup queue. The active contract is
[self-host Intent execution](self_hosted/19_intent_execution_transition_contract.md).

### Newly verified reached boundary

`mir_lower/intent_routine_plan_owner.pgy` now admits the existing routine
mode, priority, binding/phase inventory and cleanup contracts independently of
AST tree reconstruction. The tree consumes that plan. Step admission remains
in `intent_routine_step_plan_owner.pgy`; the binding owner resolves aliases
and placement reuses the action's already selected receiver row.

Each placed step now carries the Zone/participant declaration rows and exact
slot SyntaxNodeId. `semantic/intent_subject_slot_policy_owner.pgy` owns the
existing exact-compatible-alias / unique-compatible-slot rule. The source-C
environment adapter and MIR declaration-index consumer share that rule; a C
field absent from the declared field inventory is no longer silently selected.
Names choose a slot only at this semantic owner. A direct target consumer must
join its carried identity to cell layout, not choose the slot again.

Both producer MIR inputs execute the exact-name control with two same-type
slots and the differently named unique-type control: `true, 1, 50, 1`.
Native C/LLVM and public C independently produce those observations too.
Public LLVM still refuses both, as it did the existing nine controls. The
expanded four-source-leg gate is 49 passed / 11 failed; the former nine-case
gate on this same candidate was 43 / 9. The extra two failures are additional
coverage of the open execution route, not a green reclassification.

MIR-to-C completion/placement/nested regression has 88 passing checks. Typed
transition/compensation has 64 observations and 19 no-artifact refusals.
The native-compiled binding contract separately checks owner-scoped selection,
ambiguity, incomplete inventory and unlisted fields. The wrong-slot-kind MIR
is refused by the earlier nominal declaration owner, before placement.

The prior nested `on`/`intent` repair, exact callable SyntaxNodeId cross-seal,
indexed typed topology and retirement of legacy executable mirrors remain
gated. None of these admissions establish general Intent GraphPlan execution
or new hard self-host substitution. Typed mirror graph verification still
uses `MirIntentExecutionCoverLegacyGraphMirrors` with expression-order
coverage; a future typed direct consumer must not silently skip it.

### Next falsifying case — still open

The shared target transfer kernel is now implemented in
`compiler/direct_mir_intent_cell_placement_owner.pgy` and
`compiler/direct_mir_intent_cell_transfer_projection_owner.pgy`. It joins the
admitted declaration/slot IDs to cell layout and copies Subject state into the
existing slot storage or back to the participant. It never replaces the slot
pointer. `tests/self_hosted/parity/intent_cell_transfer.py` passed 42 checks:
12 C/LLVM executions across both producer inputs and 30 pre-emission refusals.
The three valid layouts cover the sole slot, exact second same-type slot and
unique differently named slot. Slot storage, participant storage and other
slots remain independent. Evidence: `.tmp/self_hosted/intent_cell_transfer/run.wz4rdfy8/`;
native-compiled validator SHA-256
`A85420347780BD9BBF2DC5F4924259595F8349A996DC9BB20A609D4C00125315`.
This kernel is not connected to production GraphPlan yet. It neither performs
nor proves synchronization, phase execution, compensation or general Intent
substitution. Its new source files are not in the older candidate's source
checksum manifest. The unchanged final driver only produced its valid MIR
inputs. The probe used explicit C emission and the existing O0 test profile
after the default native compile exceeded 60 seconds; that failed candidate
was not executed and all handles are terminal.

The reached common-path blocker is lowering the admitted step plan into actual
Zone placement, phase execution and cleanup in ordinary GraphPlan. The new
step plan is not that runtime/CFG protocol. The exact next materializer must
join `placement_binding.slot_source_syntax_id` to the existing
`logical_record.identity_cells` field identity, copy Subject state without
aliasing the Zone slot to the original participant, and preserve receiver
binding, synchronization, phase branches and cleanup. The common issuer is
`direct_mir_scalar_cfg_program_graph_admission_owner.pgy`; its ordinary
`DirectMirIdentityCellLifetimeReady` still forbids mutable calls through a
slot without that boundary plan. Do not relax this guard in isolation.
The existing
`tests/concept_semantics/intent_predicates/header_success_observed.pgy` is the
four-routine falsifier; do not remove the `signature.kind == "intent"` execution
guard until that owned plan is actually consumed. Parameter classification is
not sufficient, and a new role number or an ordinary pointer argument alone
would not supply the missing protocol.

The end-to-end regression also remains the existing
`tests/self_hosted/parity/fixture/direct_mir_legacy_intent_program_llvm.pgy`.
Its success condition is a field comparison, not literal true. The old bounded
emitter ignored it; its newly strict plan refuses with
`direct MIR legacy intent phase plan is invalid`. The formerly green legacy
LLVM gate is RED on the current pair: `intent-placement-final-legacy.log`,
scratch `self_hosted/direct_mir_legacy_intent_program_llvm/run.M29rGq/`.
Do not remove the completion guard, change the fixture to true, or count this
valid-source refusal as closure. Carry executable completion into the owned
Intent/GraphPlan path; do not add another fixed Main envelope alternative.

`tests/concept_semantics/word_deletion/cases/28_intent_header_policies/orig.pgy`
previously refused on public LLVM with
`direct MIR legacy intent Main instruction identity is invalid`.
That observation is preserved in
`.tmp/generic_execution_20260907/intent-call-spine-full-llvm.log`.
The preceding four-source-leg matrix refused every public LLVM Main shape.
Last consumers are `direct_mir_legacy_intent_program_plan_owner.pgy` and
`direct_mir_legacy_intent_program_graph_fact_owner.pgy`; the old emitter also
assumes inline Zone/Subject layout rather than the common identity-cell owner.
Do not splice that ABI into ordinary Main, silently omit instructions or retry
a failed claim through native/MIR-to-AST.

Both producers' observed-completion Main still stop at
`owner=callable-signature stage=signature-family routine=3 name=Complete`;
neither publishes an artifact: `intent-placement-final-{native,self}-direct.log`.
The nested direct-LLVM gate also stops at callable-signature for `InnerPriority`
(routine 2): `intent-placement-final-nested.log`, scratch
`self_hosted/direct_mir_nested_intent_program_llvm/run.q1VlEv/`. Earlier and new
drivers refused the same direct route. The now-green nested MIR-to-C execution
is not direct GraphPlan execution. Its direct LLVM/C and negative gate legs
remain unexecuted after that first failure. The gate now preserves each run
directory and reports both diagnostic streams instead of deleting old evidence
or hiding stdout errors. Do not restore fixed Main envelopes to green it.

The richer typed control previously refused at direct GraphPlan routine-instance
signature sealing on both producers; this direct-route observation is older:
`intent-call-spine-{native,self}-direct.log`; no C artifact. This is a distinct
boundary from the now-green explicit MIR-to-C projection. No universal signature
or target-execution claim follows from the binder/signature probes.

### Candidate and observed evidence

Directory: `.tmp/generic_execution_20260907/`.

| Artifact | SHA-256 |
| --- | --- |
| `pgy-intent-phase-single.exe` | `288ED56D8B331BC5342FDE1FAA0119A68257A4C44A69BBB88A898130E9227F57` |
| `driver.intent-placement-final.exe` | `1EFD357C8A5CD30F1CDD8250648D50CCFD03A91521ECDD4DA59D4ED09EE579D4` |
| `driver.intent-placement-final.c` | `69FE593FB59D78499E9CEFEFACFA0E9DC4F60FE1FC47753C0186AED2168A776D` |
| Machine companion | `0A83B0DB5EFE3C00C6D9413C63045C4B17AFF079781213B280442C588E5A9C19` |

Native C emission, source freeze/recheck, isolated GCC and machine replay/cmp
pass: `intent-placement-final-driver-build.log`, `intent-placement-final-driver-inputs.sha256`
and `intent-placement-final-source-inputs.sha256`. The isolated native build is
unchanged from `intent-phase-single-native-build.log`. Emission has 0 errors /
4 existing warnings (three redundant Intent clauses and one unlocated unreachable
statement). C is 42,023,840 bytes. Final source/artifact checksum rechecks pass;
the frozen source list and all four candidate artifacts were rechecked on
2026-09-09 at the final handoff boundary.

| Observed gate on the final pair | Result | Evidence relative to .tmp/ |
| --- | --- | --- |
| Completion / step plan / nested MIR-to-C | 88 checks PASS; named/unique placement, nested observations, repeated calls and no-artifact refusals | self_hosted/intent_completion/run.la_tgmic/ |
| Typed role/compensation MIR-to-C | 64 observations + 19 no-artifact refusals PASS | self_hosted/intent_block_roles/run.ns2pfhkb/ |
| Phase carriage | Phase order and admitted MIR negatives PASS | generic_execution_20260907/intent-placement-final-phase.log |
| Source admission | 50/0 | self_hosted/concept_semantics_20260905/source_admission/run.dRqAo6/ |
| Four-source-leg predicates | 49 passed / 11 failed; all eleven public LLVM valid controls refuse | concept_semantics/intent_predicates/run.s8esme6i/ |
| Observed-completion common direct C route | Both producers REFUSED at callable-signature, no artifact | generic_execution_20260907/intent-placement-final-{native,self}-direct.log |
| Legacy / nested direct LLVM | RED: nonconstant completion / Intent body execution unsupported | generic_execution_20260907/intent-placement-final-{legacy,nested}.log |
| Intent compression source contract | PASS; retired mirrors, tree-owned admission, C-owned slot selection and repeated binding lookup absent | generic_execution_20260907/intent-placement-final-static.log |

Only valid controls and validator programs execute. Altered source/MIR is
admission/refusal-only; the plan validator never emits its altered owner facts.
These gates are not full CI, bootstrap or installed-driver proof. The final
native-compiled plan validator SHA is
`BDBA1ABC398235CF096B0834839DFD4F4417D202A8F0C6B225C6FD4B5DBB975E`.
It has one existing unlocated unreachable-statement warning and uses both
`run.la_tgmic/*-repeated.mir.json` controls.

The separate native binding-contract executable is
`intent-placement-binding-checked-probe.exe`, SHA
`E848E9232B6C4922D8D696B0F90F058DEA1A2A30B0A0636CFBC279D84D96FF73`.
Its build passed and execution prints `intent step binding contract: PASS`
with empty stderr; matching `*-build.log`, `*.out` and `*.err` record it.
The earlier probe had untyped empty-array constructor arguments; typed local
arrays repaired the fixture without weakening the checker.

The first placement build failed because `eval_text` was removed despite two
remaining uses. It is restored; the later TextBuilder errors were cascading
and do not recur on the final build. Do not reopen those as a runtime regression.

Native MIR/HIR units are the prior 191/0 and 25/0 observations
(`intent-phase-single-{mir,hir}-test.log`); native compiler code/binary did not
change in this slice. No new unit run is claimed. The earlier binder/participant
probe was not recompiled for this pair; its evidence remains
`intent-phase-owner-probe-{native,self}.log`.

The routine-index fixture had stale missing module provenance and declared method
identity. It proceeded from an invalid setup into an array bounds panic.
The repaired controls carry those facts and stop on invalid positive setup
before mutation. The earlier native C probe passed; it was not rerun on this
pair. Its full public C/LLVM smoke remains blocked by the size pin below.

### Retained completed prerequisites — not an active work queue

- Intent formal declaration IDs survive both producers and matching mirrors.
  The binding projection requires positive distinct IDs and exact purpose/order.
  Common callable signatures retain Intent kind, participant/value carriage and
  indirect/direct ABI, never Main-local IDs or invented ordinals.
- Public C executes pre/invariant phases after placement binding. Common source
  admission rejects non-Bool/duplicate predicates and unavailable current-step
  outcomes; native invariant checking was corrected before outcome scope.
  Explicit full rollback remains; current/none and typed predicate-failure
  execution are not complete.
- Ordinary GraphPlan constructs frame-owned Zone/Subject cells, preserves
  declaration kind/type/ID and explicit empty authority facts, initializes
  generation storage and supports readonly-ref borrowing/reborrow.
  Native signature/RIR owners preserve BorrowedRead and remove only invented
  owned cleanup. Explicit invalidation and Intent obligations remain.
- Passive class/object value records use the existing logical-record GraphPlan;
  mutable subject/vessel identity stays separate. Named field IDs select layout;
  initializer and eager operand effects retain source order. Four obsolete
  passive literal owners and the String-only control-flow route were deleted
  earlier and remain absence-gated.
- Local/SSA declaration identity, scalar inout copy-in/copy-out, finite loop CFG,
  named-enum match and unsafe effect/scope behavior have focused execution
  evidence. Missing facts fail before publication; no invalid execution or
  source-renaming workaround substitutes for those facts.
- Retry is rejected by common admission; timeout/backoff retain parser refusal.
  Their stable language-word rows have support mask 0. Action authority/binding,
  required role impl and Future lexical lifecycle checks remain in source gates.
- Checked Int division uses the existing runtime ABI at normalization and C/LLVM
  materialization; raw dynamic sdiv/literal-only guard are deleted together.
  Runtime manifest has 267 rows, preserving the previous 266.

### Earlier evidence — rerun at integration, not current-pair claims

The preceding 2BC23CA8/19922BCA participant-policy pair passed source admission
50/0 (`self_hosted/concept_semantics_20260905/source_admission/run.eHZqwT/`),
receiver/Zone 192/0 (`concept_semantics/identity_cell_receiver/run.vtjtacic/`) and
generic execution/constraints/cell lifetime 120/0
(`self_hosted/generic-instantiation.MRrUCq/`). These broader gates were not rerun
for the current Intent phase-occurrence slice and are not current-pair claims.

The prior 253898BE/83E504A7 binder/callable pair is superseded. Earlier focused
results remain attributable to their original artifacts/logs; no aliases or
retroactive green labels:

| Earlier gate | Observed result | Evidence relative to .tmp/ |
| --- | --- | --- |
| Independent binary/Intent order | 10/2; public LLVM refuses both Intent Main shapes | concept_semantics/binary_evaluation_order/run.heagdgq9/ |
| Intent pre/invariant | 27/5; 15 native C/LLVM/public C observations + 12 source negatives pass, five public LLVM legs refuse | concept_semantics/intent_predicates/run.2exzs9u_/ |
| Loop execution | 136/0 on 668AF768/A8974C89; three absent native mutations omitted | concept_semantics/loop_statement/run.rh0vxyj6/ |
| Unsafe execution / scalar inout identity | 146/0 and 38/0 on 668AF768/A8974C89 | concept_semantics/unsafe_block/run.zhml5ief/; concept_semantics/scalar_value_result/run.3d3cfxrc/ |
| Unsafe common owner / Future admission | 36/0 and 74/0; no invalid input execution | concept_semantics/unsafe_block/run.f3w_24n0/; concept_semantics/future_lifecycle/run.5u0q14fg/ |
| Action / enum / resilience admission | 52/0, 66/0, 26/0 | concept_semantics/action_authority/run.crbxos4d/; concept_semantics/named_enum_match/run.hDZEjF/; concept_semantics/resilience/run.goe002fj/ |
| Checked Int divide shared targets | PASS, valid execution + ABI/type refusal | self_hosted/direct_mir_scalar_int_divide/run.ViyUyr/ |
| Native semantic / C units | 2930/0 and 978/0 | generic_execution_20260907/intent-invariant-semantic-unit.log; generic_execution_20260907/binary-order-transpile-hygiene-unit.log |
| Native RIR / HIR units | 26/0 and 25/0 | generic_execution_20260907/zone-borrowed-native-rir-units.log; generic_execution_20260907/binding-world-arg-hir-unit.log |
| SoT authority edges | 89 authorities / 186 derived; CLOSED55 / BRIDGE32 / ACTIVE2 PASS at that checkpoint | generic_execution_20260907/unsafe-sot.log |

Earlier passive/mutable nominal gates, generic occurrence/instance/role
constraints, scalar/Array effects, typed Intent v3 publication/cross-seal and
word-registry gates also require integration reruns. The inferred-generic fixed
matrix failed on native/self Main instruction-count drift before target
projection (`intent-callable-owned-inferred-gate.log`).

### Remaining reported obligations

The source-only audit of 104 deletion fixtures on 60444203/7CCF05AD found six
native-positive public refusals and zero operational failures, with all 20
native refusals also refused publicly:
`generic_execution_20260907/intent-phase-owned-deletion-audit/report.json`.
This is not the historical 34/104 compile/runtime metric or runtime equality.

Queue after the active Intent rung, not parallel implementation tracks:

- Public positives for parallel joins, event/party/Slot/object projections.
- Future aggregate stored-type containment and Zone spawn callee ABI.
  Phantom generic arguments are not automatically stored Future handles.
- Ability-only generic field witnesses per instance; do not infer nominal
  equality from one call tuple or relabel valid controls as negatives.
- Nested callable ABI, public Now and broader field/record execution; native
  Option<subject> ABI and Option<Array<String>> C type spelling.
- World LLVM lifecycle compiles but crashes; do not rerun the unchanged crash.
  The world C control passes. LLVM compilation of the imported admission probe
  previously failed on ArrayPush(record.field); native C probe evidence is not
  public/LLVM compiler-source closure.
- Absolute Windows MIR paths currently produce silent unreadable/empty failures.
  Repository-relative arguments isolate language tests; CLI portability remains.
- Runtime progress/quiescence, Slot protected access and evidence-erasure
  measurements remain separate obligations, not proved by current compiler gates.

The user's boundary-closure proposal is reconciled in the existing
[review intake](audits/2026-09-06_architecture_review_5b97_reconciliation.md).
The plain collection-parameter mutation document now states the owned refusal
policy. The linked full proposal was not locally attached and all 28 claimed
acceptance cases were not supplied. No new syntax/IR layer or proof claim was
introduced. Module Build remains after self-host closure.

### Integration, workspace and publication

998 porcelain entries, zero staged before publication preparation; inherited work preserved.
Commit/push is now user-requested. No manual dispatch or shared installation.
The earlier no-publication condition is no longer active. This step-plan/phase work
deleted no material files. Earlier retired owners remain recoverable from Git.
D: free was 7.63 GiB at the earlier 01:06 observation. Publication preparation
reviewed the changed path/retirement inventory, build registrations and generated
owners; full semantic integration review is still owed, not implied by staging.
Full diff whitespace validation passes (six existing CRLF-to-LF warnings).
Changed shell syntax and this snapshot's five relative links pass. The Intent
compression contract was rerun successfully at this final handoff boundary.
Before the user-requested snapshot commit, the fixed builtin-effect registry,
language-word registry, SoT authority edges and Intent compression gates passed.
The language-word occurrence inventory initially drifted after fixture changes;
its owner generator refreshed it and the complete 146-row gate then passed.
No support flag, size limit or failed valid-source expectation was weakened.

New/moved owners are within their recorded caps: routine plan 215/240,
step plan 325/330, placement binding 54/100 and slot policy 50/70; the existing
binding-contract fixture is 173/180. No limit was raised.
Current selected exceeded caps: phase projection291/260 and expression-carrier
contract62/50. Routine-index owner remains 552/530. Its earlier selected smoke
is RED; these limits were not raised. A proposed projection-field ArrayPush
cleanup was withdrawn: current member-array inout gates prohibit it, and empty
array literals lack constructor-context type inference. Keep typed local
construction arrays; do not remove those guards to meet a line count.
The smoke exits 1 at that exact pin:
`generic_execution_20260907/intent-role-routine-index-smoke.log`.
Declaration index120/120, block170/180, routine facts600/600 and machine
facts431/440 retain their earlier in-cap inventory observations. The method
epoch owner is now 405 lines. No full component/performance gate was rerun.

Other observed size failures remain: stmt_emit816/800, assignment611/600,
expression_environment608/599, concrete_scalar_verdict641/599,
signature_fact613/599, statement_fact602/600 and diagnostic712/660.
`test_mir_lowering_part_b_2.cases.h` is 739/699 and the size script's first
failure still hides later sections. Component/perf gates previously timed out
at 60 seconds; find the repeated owned operation, do not extend the allowance.
String parity also stops at stale caps and retired two-Int paths.

Current full CI/platform/parity, bootstrap gen2/gen3, installed-driver and Rocq
checks remain unrun. The earlier published-HEAD push CI was green; Platform full
and weekly parity were red. The user-requested main push will trigger CI; no
new green result is claimed by this pre-publication snapshot.
Budgets remain static60 s / focused300 s / integration1,800 s.

## Historical lookup — never an active queue

Earlier long records remain in [audits/archive](audits/archive/). Evidence tables
above replace successive active narratives; do not append another execution log.
