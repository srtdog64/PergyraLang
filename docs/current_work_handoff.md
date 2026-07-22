# Current Work Handoff

Status: navigation snapshot, not semantic authority
Updated: 2026-07-23 (latest observed session, KST)

This is the short resume index. It does not replace the named owner documents
or executable gates. If this snapshot disagrees with the tree, trust the
current owner fact and update this file after verification.

## Repository checkpoint

- Captured HEAD: `18cf5e89bb3d7c3f7f7b11f2d7c0adaefeec91ca` on `main`.
- `main` and `origin/main` are equal at the captured HEAD.
- The live worktree is dirty and none of the remaining changes is staged.
  The latest observed state has 70 modified tracked paths and 53 untracked
  paths. The tracked set includes the recovery/docs edits, fixture-226..229
  parser/semantic/MIR/codegen owners, the LLVM runtime-bitcode fix, allocator/
  BoxArray/defer/TextBuilder consumers, and emitted-C/bootstrap compile-owner
  corrections. Untracked source owners include allocator-backed BoxArray,
  runtime-header composition, defer graph, operator-kind, and emitted-C
  runtime-header owners; `.tmp_allocator_*`, `.tmp_defer_*`,
  `.tmp_text_builder_*`, bootstrap logs, and wrapper-policy paths are diagnostic
  artifacts. Preserve them until this slice is explicitly audited. Build
  artifacts remain ignored; run `git status --short --branch` before resuming
  because this count can change.
- This session created and pushed `18cf5e89` for the verified self-hosted
  `Double` emission SoT slice. The remaining dirty paths are preserved
  user/prior work and are not included in that commit.
- Exact safe-directory exception: `D:/PergyraLang`. Repository-local
  `core.autocrlf=false` preserves the LF policy in `.gitattributes`.

## Workstation recovery

- Global Codex rules at `C:/Users/user/.codex/AGENTS.md` were reviewed on
  2026-07-22. They define a cross-project baseline while the nearest repository
  `AGENTS.md` and existing owner contracts remain more specific. The baseline
  was reduced from 748 example-heavy lines to 215 enforceable lines covering
  authorization, evidence, SoT, explicit failure, retry/idempotency,
  observability, safe filesystem/Git work, verification, and handoff. Current
  SHA-256 is `4A37B095AF2DBFD6E1DAD49CCB3F1C93CF0EEEA80EEAE94E92C05DBDCEA3D6AC`.
- Installed Git for Windows `2.55.0.windows.3`, Git LFS, and GitHub CLI
  `2.96.0`. Public `origin` reads succeed. GitHub CLI authentication is still a
  user-owned step (`gh auth login`); do not infer credentials.
- Installed the official Windows acceptance environment at `C:/msys64`:
  UCRT64 GCC `16.1.0`, GNU Make `4.4.1`, Python `3.14.6`, and
  LLVM/Clang/LLD `22.1.8`, plus ripgrep `15.2.0`. `gcc -dumpmachine` reports
  `x86_64-w64-mingw32`, and `llvm-config --libs core` succeeds.
- The user PATH now starts with `C:/msys64/ucrt64/bin` exactly once. This makes
  a newly launched PowerShell able to load `libLLVM-22.dll` and
  `libwinpthread-1.dll` when invoking `bin/pgy.exe` directly. The current Codex
  process predates that PATH change, so its direct PowerShell probes must still
  prepend the same directory locally; the MSYS2 test owners already do this
  through `pgy_prepend_windows_runtime_paths`.
- Node.js LTS `24.18.0` and npm `11.16.0` are available. The
  `editors/vscode` lockfile was restored with `npm ci`, and `npm run compile`
  passes. `npm audit` reports one transitive high-severity
  `brace-expansion <2.1.2` denial-of-service advisory; the lockfile was not
  rewritten during environment recovery.
- `make LLVM_ENABLED=0 check-build-tools` and
  `make LLVM_ENABLED=1 check-build-tools` pass in an MSYS2 UCRT64 login shell.
  Use serial `make` on this workstation: the first `make -j2` attempt waited in
  the Windows jobserver without compiler children, while serial compilation
  progressed normally.
- `make LLVM_ENABLED=0 all-with-tests` passes. The generated compiler also
  compiles and runs `examples/hello.pgy`, producing `Hello, Pergyra!` with zero
  errors and warnings. Focused gates `mir-lowering-api-test-smoke` and
  `runtime-context-test-smoke` pass after installing MSYS2 ripgrep.
- After the documentation consistency corrections,
  `sot-authority-edge-test-smoke` passes with `CLOSED=22 BRIDGE=20 ACTIVE=0`
  and seven valid protocol rows with no duplicated authority;
  `self-host-substitution-velocity-test-smoke` also passes with the accepted
  nine-blocker executable-first process contract.
- Coq/Rocq is not installed locally. Windows CI intentionally declares this
  with `PGY_ALLOW_MISSING_COQ=1`; formal proof acceptance remains the dedicated
  Linux/Rocq gate.

## Active source-of-truth work

### 1. Hard self-host DRV-2 executable replacement

- Resume owner: `src/self_hosted/PROGRESS.md`.
- The closed executable frontier is fixture 230, `class_suit_score`, after the
  closed `text_builder_lifecycle` fixture 229. The current-hard 230/230 matrix
  is green. Released/default-driver replacement remains open, and fixture 231
  has not been selected.
- A focused C/LLVM producer-first parity result counts. Owner files, docs,
  manifests, and fixture count do not count as substitution by themselves.
- Fixture 226 adds `coalesce` as a stable typed expression-graph node. Grouped
  expressions re-enter `ParseExprFact`; postfix `?` explicitly excludes `??`;
  `OptionCoalescePayloadTypeOpt` owns payload compatibility; and C emission
  consumes the existing Option runtime ABI names. No source-text reparse,
  native-MIR injection, fixture-specific compatibility helper, or backend-
  local representation was added.
- Exact focused evidence observed on 2026-07-22: `make LLVM_ENABLED=0
  self-host-parser-parity-test-smoke` passed with 188 byte-equal sources;
  the current-tree LLVM-enabled component contract passed; and filtered C,
  LLVM, and refreshed Pergyra-built hard producer-first parity each passed
  with `body_fixtures=20` and `mir_fixtures=1`. Self source and self-MIR
  consumption emitted byte-identical C; all three runtime outputs were `10`,
  `0`, `6`, `100`, `0`; missing/invalid graphs plus a
  `coalesce -> logical_or` mutation failed closed. `make -s LLVM_ENABLED=1
  compiler` and `make -s LLVM_ENABLED=1 self-host-compiler` completed; the
  native compiler build emitted existing warnings that remain visible in the
  session evidence. `tests/runtime_bc_contract_smoke.sh` also passed after the
  LLVM runtime module stopped defining externally linked stateful globals.
- Fixture 227 closes allocator construction, allocator-backed `Box<Array<T>>`,
  and `AllocatorDestroy` defer behind typed expression/MIR graphs. The old
  `defer_body` string owner is deleted; missing/invalid graphs and a forged
  target fail closed. Focused C, LLVM, and freshly Pergyra-built hard parity
  all pass, with runtime output `1201`, `1202`, `1203`, `1204`.
- Fixture 228 proves `AllocatorScratch`, `AllocatorResult`, and
  `AllocatorPersistent` consume the same BoxArray owner facts with no
  allocator-name C switch or fixture helper. C, LLVM, and a newly built hard
  driver pass focused parity with runtime output `401`, `402`, `403`.
- Fixture 229 deletes the generated-C `TextBuilderRuntimeCBlock`.
  `runtime_header_owner.pgy` selects canonical runtime inline owners without
  duplicating their implementations. One emitted-C runtime-header classifier
  is consumed by both parity compilation and the hard installer. C, LLVM, and
  a newly Pergyra-built hard driver each pass focused producer-first parity
  with `body_fixtures=20`, `mir_fixtures=1`, and output `PergyraLang`.
  Native/self canonical MIR SHA-256 in every lane is
  `CBA1C55B664BEC216DF874043186E3FC0FC40DEFE4BC61EC7096667163168779`.
- Final current-tree focused gates passed: self-host component contract,
  runtime-bitcode twin contract, hard-substitution contract, substitution-
  velocity contract, shell syntax for the changed parity/install owners, and
  `git diff --check`.
- On 2026-07-23, isolated current-tree bootstraps passed through gen2. The 229
  bootstrap at `.tmp/self_hosted/codegen/bootstrap_text_builder_229` rebuilt
  gen0/gen1/gen2 with an empty `gen1_cc.log`, then produced the current hard
  driver in `.tmp/bin_text_builder_229_hard`. This closes the previous
  generated-helper/runtime-header drift for the admitted executable surface.
- The driver build cache had a real ownership gap: its stamp did not bind the
  installed output path, so an isolated stamp could accept an unrelated old
  `bin/pgy-self-driver.exe`. `tests/self_hosted/parity/self_host_compiler_build.sh`
  now includes the normalized output identity in the stamp input. The
  falsifier is switching output paths while reusing one build directory; the
  old binary must not be accepted as the current owner artifact.
- The standard `bin/pgy.exe` is the isolated LLVM-enabled build; its installed
  SHA-256 is
  `047C727E67C9546583A20F9A472729DE4E30988E5611FEE366E33A8C551E73C2`.
  Direct LLVM compile/run of fixtures 227 and 229 passes when the UCRT64 DLL
  directory is in the launch PATH. The fixture-229 compiler stage trace reaches
  LLVM object emission, runtime preparation, native link, and return; its
  executable prints `PergyraLang` and exits 0. The earlier direct PowerShell
  failure was a launch-environment defect, not evidence of a TextBuilder ABI or
  linker defect, so no TextBuilder-specific compiler branch was added.
- The current-hard 229/229 integration matrix is green by an observed union of
  ordered shards: rows 1-72 in
  `.tmp/self_hosted/driver_rung2_hard_full_20260723_4`; focused rows 73 and 74
  in `.tmp/driver_scalar_229_hard_v2` and
  `.tmp/driver_double_229_hard_v2`; rows 75-114 in
  `.tmp/driver_229_hard_remaining_after_double`; focused row 115 in
  `.tmp/driver_owner_field_229_hard_v2`; and rows 116-229 in
  `.tmp/driver_229_hard_remaining_after_owner_field`. The final shard reported
  `producer-first source/MIR parity ok: backends=1 body_fixtures=20
  mir_fixtures=114`. Every shard used the native C oracle and the freshly
  Pergyra-built hard driver at `.tmp/bin_double_229_hard/pgy-self-driver.exe`.
- The integration run closed a shared `Float -> Double` semantic omission at
  `ExpressionAssignableTo`, matching native `type_is_assignable`; the existing
  113-fixture semantic corpus passed on C and LLVM with positive widening and
  reverse-narrowing rejection. `array_double_aggregate_core` now owns its
  static `Array<Double>` row and missing/wrong-ID negatives. `EmitLog` consumes
  `Double` explicitly and fails closed for unsupported types instead of using
  the string logger as a fallback. Focused hard outputs for the Double fixture
  are `1.250000` from both oracle and MIR-consumed C.
- Three integration-only stale expectations were corrected without changing a
  semantic owner: `for_each_call` now checks graph/type/count facts instead of
  obsolete JSON adjacency; `array_index_assign` uses the explicit oracle MIR
  bridge while direct native residual-assignment input remains a fail-closed
  negative; and `owner_field_assignment` expects only defined SSA versions in
  `uses`, with the version-zero `amount` parameter retained in the expression
  graph. The filtered selector now indexes the 230 manifest once and rejects
  empty, duplicate, or unknown bases.
- Fixture 230, `tests/cases/backend_compare/class_suit_score/main.pgy`, is now
  admitted using only the existing class-field, typed expression-graph,
  string-return, direct-call, and Log owners. Focused C-built, LLVM-built, and
  newly Pergyra-built hard lanes passed with 20 body fixtures and one MIR
  fixture. Hard native/self canonical MIR SHA-256 is
  `047E19BC06678B64D5D0843FC869CB979CEF34B9EDD55EC8813AD4DCD6476547`;
  source/self-MIR C SHA-256 is
  `B7EC186FB9B6F66D368AD3FCB88F777ACEB644F7DB69301385F1A44468A0F3AE`;
  runtime output is `Heart`, `14`, `Spade`, `7`, `Club`, `13`. No fixture
  helper, source rescan, native-MIR injection, C fallback, or backend-local
  policy was added.
- The unfiltered current-hard integration gate using
  `.tmp/bin_class_suit_230_hard/pgy-self-driver.exe` passed with
  `producer-first source/MIR parity ok: backends=1 body_fixtures=20
  mir_fixtures=230`. Its build directory is
  `.tmp/driver_class_suit_230_hard_full`. One existing worker-pool-inactive
  message exposed the documented serial execution choice; it did not fail the
  gate. Released/default replacement remains 0%.
- The next bounded falsifier is not yet selected. Before fixture 231 admission,
  choose exactly one unsupported Pergyra-level semantic surface, write its
  objective card, and probe it without changing the manifest. Do not open a
  C-shaped runtime fragment or a parallel cleanup track merely to increase the
  fixture count.

### 2. MIR-only / ABI-first backend closure

- Resume owners: `docs/193_mir_only_abi_first_backend_closure.md`,
  `docs/192_protocol_abi_api_registry.md`, and
  `docs/semantics/sot_owner_spine_registry.md`.
- Core ABI, MIR JSON, runtime-call ABI, parallel-capture projection,
  machine-layer declaration, LSP JSON-RPC, and compiler-lowering API rows are
  still `BRIDGE`, not globally `CLOSED`.
- The highest-value open edges are full aggregate/runtime consumer and
  compatibility-corpus migration, global missing-fact negatives, the
  unregistered LSP protocol authority, and the unregistered MIR-lowering API
  authority. Do not promote a bounded fixture result to global closure.

### 3. Region/Arena correction train

- Resume owner: `docs/197_region_arena_strategy.md`, especially Appendix A.
- REG-1a runtime, REG-1b verified plan, REG-1c narrow escape analysis plus
  driver/backend wiring, and REG-1d self-host contract surface are recorded as
  landed. Current rows use stable syntax/allocation IDs, reset reuses retained
  blocks, and an uncertified site defaults to HEAP while missing or
  contradictory certificate evidence refuses plan publication.
- Before widening the certified class, rerun the region unit/backend gates and
  preserve the explicit HEAP default. Do not widen the AST-owned
  `Print`/`PrintLn` prototype into a final semantic owner.

### 4. Runtime instance ownership rung

- Resume owner: `docs/196_content_instance_runtime_context.md` and
  `src/runtime/pgy_runtime_context.h`.
- Capability/budget state has a TLS-bound `PgyRuntimeContext` first rung.
  Cancellation roots, schedulers, task handles, asset namespaces, linear
  memory, random streams, and diagnostics still need explicit carriage and
  cross-instance negative tests. Do not claim complete multi-tenant isolation.

### 5. World composition

- `docs/195_world_universe_composition.md` is `proposed, out-of-beta`.
  `WorldGraph`/`UniverseManifest` must precede any DLL loader, and the loader
  may not infer semantics from symbols. Keep this out of the active beta
  implementation rung until its owner facts and negative gate exist.

## Resume order

1. Run `git status --short --branch` and compare HEAD with this checkpoint.
2. Read the newest entries in `src/self_hosted/PROGRESS.md`, then the relevant
   section of `docs/193` or `docs/197`.
3. Choose one active executable rung and write its objective card: owner, last
   consumer, forbidden fallback, focused gate, and falsifying fixture.
4. Run the narrow owner gate first. For the current tree, useful focused gates
   include `mir-lowering-api-test-smoke`,
   `parallel-capture-projection-test-smoke`, `region-arena-test-smoke`,
   `region-plan-unit-test-smoke`, `region-escape-unit-test-smoke`,
   `runtime-context-test-smoke`, and the selected DRV-2 producer-first parity
   lane. Run the broad self-host matrix only at an integration boundary.
5. Before a third consecutive SoT-only commit, land an executable replacement
   delta or record the exact missing fact, owner, last consumer, and falsifying
   fixture as blocked, per `AGENTS.md`.

## Handoff refresh contract

After material work, update this file with:

- exact HEAD and dirty-state summary;
- active executable rung and owner;
- exact focused gates that passed or failed;
- next falsifying fixture or missing fact;
- blockers that require authority, credentials, or an external state change.

Never replace a registry row, owner document, or test result with a prose
memory claim.
