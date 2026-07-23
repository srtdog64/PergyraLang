# Current Work Handoff

Status: navigation snapshot, not semantic authority
Updated: 2026-07-23 (latest observed session, KST)

This is the short resume index. It does not replace the named owner documents
or executable gates. If this snapshot disagrees with the tree, trust the
current owner fact and update this file after verification.

## Repository checkpoint

- Captured code HEAD: `e3cc1375` (`Close LLVM call result type SoT`) on `main`.
- `origin/main` is at the same revision `e3cc1375`.
- The working tree has three intentional concurrent unstaged paths:
  `src/codegen/llvm_expr_call_args.c`, `src/codegen/llvm_internal.h`, and the
  concurrent hunk in `src/codegen/llvm_stmt_type_infer_call.c`; this handoff
  refresh does not stage or claim those changes.
- Commit `c2932d47` was created and pushed for the collection/let/try-let
  binding-consumer SoT follow-up. Collection mutation targets now require the
  owner-provided `cbind`, while `let`, `try-let`, range-loop, and foreach names
  consume `CodegenFunctionValueBindingFact`; a cref-only probe fails closed.
- Commit `e9ac5829` was created and pushed for the assignment C-binding SoT
  closure. `CodegenFunctionValueBindingFact` now owns source identity,
  semantic type, runtime kind, C binding name, and typed environment rows for
  function definitions, prototypes, locals, parameters, and assignment
  emission. The enum ABI closure remains at `373e210c`, and the nested
  value-wrapper closure remains at `1a850129`.
  Commit `657970f3` adds the root `/.tmp_*` ignore boundary, records the current
  owner/ABI documents, aligns stale hard-contract assertions, and preserves the
  existing component-region ratchet. At that code checkpoint only this handoff
  remained unstaged; committing this snapshot leaves 0 tracked changes, 0
  staged paths, and 0 untracked paths.
- Workspace cleanup recycled 87 root `.tmp*` files and reduced the ignored
  `.tmp` cache from 260,543 files / 9,489,609,184 bytes. Fresh verification
  rebuilt 1,869 files / 54,692,516 bytes under `.tmp`; that cache is ignored
  and may be deleted when no gate is running. No tracked temporary file exists.
- Current closed executable rung: `CodegenFunctionValueBindingFact` owns source
  identity, semantic type, runtime kind, C binding name, and typed environment
  rows together. Function definitions, ordinary and generic prototypes,
  parameters, locals, assignment emission, collection mutation, `let`,
  `try-let`, range-loop, and foreach consumers use that fact. `EmitAssign` and
  the migrated statement emitters only consume owner-provided bindings; their
  symbol-owner import and target-text/name recovery paths are absent.
- The latest SoT closure is `e3cc1375`: MIR source-local call-result facts own
  `UnwrapOption(Option<T>)` payload typing, and LLVM call type inference reads
  that active MIR owner. The assignment projection C/LLVM parity gate passed
  all positive and missing-fact negatives; the 8-fixture C/LLVM codegen shard
  passed `rung-0..21`.
- Executable witnesses: the assignment projection probe emits its five pinned
  Option/scalar/indexed rows and rejects missing expected type, indexed target
  type, direct call target, and C binding. `owner_field_assignment` proves
  `balance = balance + amount` reaches `self.balance` through the implicit
  owner-field binding row without source rewriting. `func_call` and
  `option_int_core` cover prototypes, parameters, locals, and Option assignment;
  `collection-cref-only` proves a raw reference row is not accepted as a C
  binding. The focused array/loop/try codegen shard covers
  `array_sum,array_push,array_pop,array_param,for_sum,for_each,option_try,result_try`.
- Fresh combined-tree evidence: the assignment projection C leg passes,
  including the `collection-cref-only` negative. Focused C codegen parity
  reports rung `0..21` green for the eight-fixture
  `array_sum,array_push,array_pop,array_param,for_sum,for_each,option_try,result_try`
  shard. Filtered producer-first DRV-2 for `class_with_array_param` reports
  `backends=1 body_fixtures=20 mir_fixtures=1`. MIR JSON coverage reports all
  nine control/value probes PASS. Shell syntax, component
  contract, hard-substitution contract, substitution-velocity contract,
  authority-edge (`43 authorities`, `38 derived`, `CLOSED=23 BRIDGE=20
  ACTIVE=0`), and live owner/negative-mutation adequacy pass. Coq/Rocq is
  unavailable, so the proof model was explicitly skipped and was not claimed
  checked. Manifests remain 85 codegen fixtures, 255 DRV-2 MIR rows, and 21
  TestHarness codegen paths; the full unfiltered matrix was omitted.
- Concurrent `option_match_owner.pgy`/`stmt_emit.pgy` work was preserved. Its
  direct text-owner import, OWNERS entry, and component size signal were aligned
  so the current combined tree passes the component contract.
- This session created and pushed `2151b840` for generic enum-payload
  declaration/match SoT closure. Semantic enum payload rows now flow through
  MIR declaration metadata and `param_types`, selfhost `mir_lower` validates
  ordered concrete facts, and tagged codegen consumes the same owner for
  constructors, tag conditions, and ordered payload bindings. Commit `8b60ebd7`
  refreshed the earlier checkpoint. The subsequent current-tree verification
  closed its stale blocker: direct source codegen produces `0`, `75`, `28`,
  `120`, `81`, filtered MIR JSON parity passes both enum fixtures, and
  producer-first DRV-2 source/MIR parity passes `enum_multi_payload` with
  `body_fixtures=20` and `mir_fixtures=1`. The HIR ordered binding owner,
  semantic environment, MIR match rows, and independent contiguous payload
  projection graphs carry arbitrary arity without variant-name or arity
  switches. Tagged enum equality remains a deliberate negative semantic gate.
- The prior active falsifier, LLVM `UnwrapOption` call-result inference, is
  closed by `e3cc1375`. No next executable falsifier is selected from the
  passing focused gates; the full matrix remains an explicit budget omission.
  `tests/mir_declaration_inventory_smoke.sh` still has a pre-existing baseline
  failure for `src/codegen/transpiler.c` missing
  `emit_class_decl_from_mir_header(header, ctx)` and is unrelated to this
  closure.
- This session created and pushed `c435b4c1` for the HIR region-fact carriage
  SoT closure. The HIR projection now owns stable copies of semantic region
  rows and the driver consumes only the HIR carrier; the semantic producer,
  HIR projection, and driver completeness boundary are covered by negative
  gates. Commit `59859903` refreshed its handoff. The later dirty MIR-region
  follow-up is concurrent and was not modified by the fixture slice.
- This session then created and pushed `26a73693` for MIR-owned region-fact
  retention. MIR now owns a validated copy of the HIR rows and the driver
  consumes only `mir->region_escape_facts`; missing carrier or function
  identity fails closed. The focused MIR, C/LLVM, region, self-host owner,
  component, and authority gates are green. Commit `c0bd8688` refreshed this
  handoff after that closure; both revisions are already on `origin/main` and
  remain separate from the dirty fixture work.
- This session created and pushed `18cf5e89` for the verified self-hosted
  `Double` emission SoT slice, refreshed this handoff in `00e8091b`, and
  created and pushed `2a21ef80` for the verified DRV-2 fixture-230 SoT slice.
  It then refreshed the handoff in `37c5e213` and created and pushed
  `d47f6bd0` for the explicit `Result<T,E>` runtime SoT slice, then refreshed
  this handoff in `d623d002`, committed and pushed the 25-path fixture-231
  match-binding slice as `dd916eaa`, and concurrent work committed and pushed
  the fixture-232 class-composition slice as `89e314be`, then refreshed this
  handoff in `0491f718`. This session verified fixture 233 and 234, while
  concurrent work created and pushed `23f06879` for the fixture-234
  `class_method_result_loop` closure, including its owner-directed match/phi
  negative gates and fresh hard producer evidence, then refreshed the handoff
  in `85a86791`. Fixture 235, its wrapper-owner migration, and the refreshed
  evidence remain unstaged; this session did not stage, commit, or push them.
  The pre-existing `stmt_emit.pgy` and concurrent region/AST slice were not
  modified by fixture 235. This session then created and pushed `b3216b62` for
  the region escape callee-identity SoT closure, followed by `e8730785` for the
  semantic region-escape producer SoT closure, then `f3979c7a` refreshed its
  handoff. Fixtures 235-240 and their documentation remain unstaged; this
  session did not stage, commit, or push that fixture slice. The later dirty
  HIR region-fact follow-up was closed and pushed as `c435b4c1`, then its
  handoff was pushed as `59859903`.
- This session created and pushed `4e4b5d9a` for the first bounded semantic
  retention-summary rung. `src/semantic/region_retention_summary.c` now owns
  the `BuiltinKind` plus argument-position fact for `Print` borrowing, and the
  region collector consumes that owner instead of checking `BUILTIN_PRINT`
  directly. Unknown, missing, or non-first-argument summaries remain HEAP by
  default. The focused region, self-host region-plan, component, authority,
  and C/LLVM compiler-build gates were green. Commit `1b2475c3` refreshed the
  handoff; both revisions are on `origin/main` and remain separate from the
  dirty fixture slice.
- This session then created and pushed `7a5ea2d0` to extend the same semantic
  retention owner to synchronous `Log`, `LogRaw`, `LogBanner`, and `LogBlock`
  consumers. The collector still reads only the summary owner; variadic `Log`
  positions are accepted, while missing/unknown non-owner facts remain HEAP.
  Region/plan unit gates and C/LLVM compiler builds passed. At that checkpoint,
  the full component gate was blocked by a concurrent 613-line
  `routine_fact_index_owner.pgy`; the fixture-244 CFG owner split has since
  restored the 600-line cap and the current component gate is green.
- This session created and pushed `e5dede8e` to extend the retention-summary
  owner to resolved user callees with `ref String` parameters. Only a direct
  approved synchronous sink is certified; return, assignment, nested-helper,
  and unresolved-identity cases fail closed to HEAP. The C/LLVM good/bad
  backend fixtures, region unit, self-host region-plan (16 projection pins and
  8 producer rejections), and component contract gates passed. The collector
  now routes `BUILTIN_NOT_BUILTIN` calls through the semantic user-callee
  callback instead of treating the populated enum slot as a builtin fallback.
  Commit `1cf9bbfa` refreshed that closure's handoff; both revisions are on
  `origin/main` and remain separate from the unstaged fixture slice.
- This session created and pushed `184febb9` for the shared text-artifact
  normalization SoT. CRLF and trailing blank-line framing are normalized by one
  parity owner, and lexer parity no longer carries a local duplicate. The
  244-hard framing falsifier, component contract, and shell syntax gates passed.
- This session created and pushed `eb73f1b8` for the DRV-2 executable frontier
  through fixture 244, including the current-iteration MIR merge owner and the
  migrated class/enum/array/recursive/match negative owners. It then created and
  pushed `beb7458f` for fixture 245, where the coalesce semantic owner carries
  `Array<Int> -> Option<Int> -> ?? -> loop phi`; 3-backend producer-first parity
  and component/shell gates passed. Both revisions are on `origin/main`.
- This session then created and pushed `15eb2903` for manifest row 246,
  `coalesce_in_if_condition`, and `93c04f46` refreshed the handoff. The existing
  coalesce semantic owner has a Boolean branch consumer covered by an indexed
  `Array<Int>` loop; the direct-target, index, operator-kind, and loop-phi
  mutations fail closed. Focused C/LLVM/245-hard parity, component, and shell
  gates passed. Because the pre-existing 245-hard driver already accepted the
  row and `15eb2903` contains no Pergyra semantic implementation change, this is
  coverage breadth rather than substitution progress. The counted executable
  frontier remains fixture 245.
- Concurrent work then created and pushed `ecb65c62` for manifest row 247,
  `nested_coalesce_chain`. The pre-existing 245-hard driver accepts the normal
  row, so this also remains a non-counting coverage ratchet rather than an
  executable replacement. The row pins two direct `HalvedIfPositive` targets,
  two nested `Option<Int> ?? Int` graph nodes, and fail-closed missing-target and
  coalesce-kind mutations. Focused C, LLVM, and 245-hard producer-first parity,
  shell syntax, and the current-tree component contract are green.
- This dirty session closed the language-wide match-binding type carrier, not a
  numbered fixture. Native semantic analysis records stable Option, Result, and
  user-enum payload binding types; HIR and MIR copy the facts, MIR JSON reads
  only the execution owner, and Pergyra `mir_lower` rejects missing/Unknown
  rows and emits typed bindings. The real Pergyra path produced runtime `42`
  equal to the native oracle. The numbered executable frontier is still 245;
  the carrier is an additional executable integration rung. At that checkpoint
  generic user-enum payload reconstruction was still open; commit `2151b840`
  closes that later MIR→C/codegen SoT seam as recorded above.
- Exact safe-directory exception: `D:/PergyraLang`. It is present in both the
  normal Windows Git config and the MSYS gate user's config at
  `C:/msys64/home/user/.gitconfig`; this is required because those shells use
  different global homes. Repository-local `core.autocrlf=false` preserves the
  LF policy in `.gitattributes`.

## Workstation recovery

- Global Codex rules at `C:/Users/user/.codex/AGENTS.md` were reviewed on
  2026-07-22. They define a cross-project baseline while the nearest repository
  `AGENTS.md` and existing owner contracts remain more specific. The baseline
  was reduced from 748 example-heavy lines to 214 enforceable lines covering
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
  `sot-authority-edge-test-smoke` passes with `CLOSED=23 BRIDGE=20 ACTIVE=0`
  and seven valid protocol rows with no duplicated authority;
  `self-host-substitution-velocity-test-smoke` also passes with the accepted
  nine-blocker executable-first process contract.
- Coq/Rocq is not installed locally. Windows CI intentionally declares this
  with `PGY_ALLOW_MISSING_COQ=1`; formal proof acceptance remains the dedicated
  Linux/Rocq gate.

## Active source-of-truth work

### 1. Hard self-host DRV-2 executable replacement

- Resume owner: `src/self_hosted/PROGRESS.md`.
- The current-tree closed executable frontier is fixture 245,
  `coalesce_accumulate_loop`. It is the latest rung with an observed rejection
  by the previous hard driver and a Pergyra semantic implementation change that
  replaces the failed path. Manifest row 246, `coalesce_in_if_condition`, is a
  non-counting Option<Bool>/coalesce/array-loop coverage ratchet: its negative
  owner is useful, but the pre-existing 245-hard driver already accepted it and
  no semantic owner changed. Manifest row 247 is the same kind of non-counting
  coverage for a nested `Option<Int>` coalesce chain; 245-hard also accepts it
  without a semantic implementation change. The last complete unfiltered
  current-hard matrix remains 230/230; released/default-driver replacement
  remains open, and fixture 248 has not been selected.
- Above that numbered fixture frontier, the language-wide
  `semantic.match_binding_type` carrier is closed, and the generic enum-payload
  declaration/match slice is now also closed in the MIR→C/codegen rung. Native
  semantic rows, HIR/MIR copies, MIR JSON `param_types`, selfhost `mir_lower`
  validation, and tagged C constructors/match bindings all use owner-directed
  facts; missing, `Unknown`, singular-wire, and old payload-rejection paths are
  negative-gated. `option_match` still reaches runtime output `42`, while the
  enum payload fixtures cover one and multiple ordered bindings. Direct-source
  DRV-2 for `enum_multi_payload`, `Option<Cell>`, and the nominal aggregate
  payload now passes. Declaration scheduling is owner-directed and its direct
  cycle and missing-inventory cases fail closed.
- Focused evidence for that integration delta: native `Option`, `Result`, and
  multi-arity enum MIR probes carry exact type rows;
  `match-binding-type-fact-test-smoke` passes; the native MIR unit slice passes
  `152/152`; C and LLVM compiler builds pass with pre-existing warnings; and
  component, substitution-velocity, AST-to-MIR loss, and SoT authority gates
  pass. An earlier unfiltered `mir_json_parity.sh` run stopped before
  `option_match` on concurrent Pergyra codegen compile errors. The later
  current-tree `option_match`-filtered lane passes end to end (`1 fixtures, 0
  clean rejects`), so that compile blocker no longer reproduces on this slice.
  The full unfiltered matrix was not rerun; do not report it as green.
- The current whole-tree component contract is green. It briefly exposed an
  in-progress missing `hir->region_escape_facts` connection while the
  concurrent MIR-region slice was changing; the current dirty tree has closed
  that connection. Preserve that concurrent slice rather than folding it into
  DRV-2.
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
- Fixture 231, `tests/cases/backend_compare/dish_result_collect/main.pgy`,
  closes the active `Result<Dish,CookErr>` seam through Pergyra semantic
  statement types, carried MIR `match_binding_types`, explicit Result runtime
  facts, nested enum matching, and class method calls. Missing binding type
  fails ordinary MIR graph admission; only the named native-oracle
  canonicalization bridge may reconstruct an inferred legacy `Let` before
  producing canonical Pergyra MIR. Version-zero arm locals no longer leak or
  generate an outer phi.
- Focused C-built, LLVM-built, and freshly Pergyra-built hard lanes pass with
  20 body fixtures and one MIR fixture. The hard driver is
  `.tmp/bin_dish_result_231_hard/pgy-self-driver.exe`, SHA-256
  `DD4A4CD6913A0EF3F329487DDAF9C34E0B4C858DC625810910374448A308DC97`.
  Hard native/self canonical MIR SHA-256 is
  `57D74F8EC14255E63C1A0AC05650FF3DBF9460618F78FCB088878D2B5905AA9A`;
  source/self-MIR C SHA-256 is
  `49971A30E6FF299EEE6122670BDF0013DC3F5E44491868857957131268F2D123`;
  runtime output is `175`, `-1`, `-2`.
- A seven-row hard Result/Option/match/enum/frontier shard passes. An
  unfiltered 231-row attempt was intentionally stopped after 21 runtime rows
  when its projected duration exceeded the integration-shard budget; do not
  call it green. The last complete unfiltered matrix is still 230/230.
- Fixture 232,
  `tests/cases/backend_compare/class_factory_result_wrap/main.pgy`, extends the
  same Pergyra owners through a value-returning function:
  `MakeTax -> Result<Tax,TaxErr> -> match -> t.Compute()`. MIR explicitly
  carries `Ok(t): Tax`, `Err(e): TaxErr`, and the member target `Tax_Compute`;
  the hard consumer rejects either missing fact. No source re-scan,
  pattern-string inference, fixture branch in compiler semantics, C fallback,
  or new runtime fragment was added.
- Focused C-built, LLVM-built, previous-231-hard, and freshly Pergyra-built
  232-hard lanes pass with 20 body fixtures and one MIR fixture. The new hard
  driver is
  `.tmp/bin_class_factory_result_wrap_232_hard/pgy-self-driver.exe`, SHA-256
  `D565A28EF6B5C5750AE5EE45D77D0BE46A323FE22B77DCB780B62C0CCFE54F53`,
  with a 232-row manifest ending in `class_factory_result_wrap`.
- Hard native/self canonical MIR SHA-256 is
  `038B15579E570FC780A7CD891EDABEC7DE8863CDB2753378A637DB8A1658909B`;
  oracle/self/source emitted-C SHA-256 is
  `91C40FC856A00A4E1944380D892BE553D4D5807A1B1103FAAEEA4A2739CDAD0F`;
  runtime output is `10`, `25`, `-1`, `-2`, `0`. The eight-fixture hard impact
  shard passes. Component, shell syntax, diff, SoT authority, gate-owner,
  protocol-registry, and substitution-velocity gates also pass.
- The full 232-row matrix was not run because the observed 231 projection
  exceeds the 30-minute integration budget. The last complete unfiltered
  matrix remains 230/230; this is an explicit omission, not a green result.
- Fixture 234,
  `tests/cases/backend_compare/class_method_result_loop/main.pgy`, carries a
  class method `Calc.DivBy` through `Result<Int,DivErr>`, typed `Ok(v)`/`Err(e)`
  match arms, accumulator merge, and a while backedge. MIR owns the member
  target `Calc_DivBy` and the accumulator chain
  `acc.1 -> acc.4 -> acc.8/acc.12 -> acc.13 -> acc.4`; no fixture/name branch,
  source re-scan, pattern inference, native-MIR injection, C fallback, or new
  runtime fragment was added.
- Focused C-built, LLVM-built, previous-233-hard, and freshly Pergyra-built
  234-hard lanes pass with 20 body fixtures and one MIR fixture. The fresh hard
  driver is
  `.tmp/bin_class_method_result_loop_234_hard/pgy-self-driver.exe`, SHA-256
  `AFE689EDCB93AEAE7FE9CC9FDFCAD16E4F03C6AE244053EAA59A01DA27FDCE2E`,
  with a 234-row manifest ending in `class_method_result_loop`.
  Hard native/self canonical MIR SHA-256 is
  `B11981A79C4A892A20ADC489254E896A4B01262119845DB972F92120584C1CDA`;
  hard source/self-MIR C SHA-256 is
  `1C0F1419F35B8F0F7AE43E47C8772A71516C11254C19C79C54F2439072495D0F`;
  runtime output is `104`, `-2`, `0`. Removing `match_binding_types` or
  `Calc_DivBy` fails graph admission, while removing the `acc.8` match-success
  merge input fails with `MIR phi facts are missing or inconsistent:
  RunSeries`.
- The eleven-fixture hard impact shard passes. Component, shell syntax, diff,
  SoT authority, gate-owner, protocol-registry, and substitution-velocity
  gates also pass. The full 234-row matrix was not run under the 30-minute
  budget; the last complete unfiltered matrix remains 230/230. The next
  bounded falsifier, fixture 235, was not yet selected at that checkpoint.
- Fixture 235,
  `tests/cases/backend_compare/class_bump_option_match/main.pgy`, carries
  `Counter` through `Counter.Bump -> Option<Counter> -> Some(next)`, a match
  merge, and a while backedge. MIR owns `Some(next): Counter`, `Counter_Bump`,
  and `c.1 -> c.3 -> c.7/c.3 -> c.10 -> c.3`. Compiler semantics gained no
  fixture/name branch, source re-scan, wrapper representation guess,
  native-MIR injection, C fallback, or runtime shard.
- The Result-specific loop-phi test owner was replaced by
  `driver_rung2_wrapper_match_loop_phi_parity_owner.sh`. Result and Option now
  share one wrapper-loop state contract; the component gate rejects the old
  path so dual ownership cannot return.
- Focused C-built, LLVM-built, previous-234-hard, and freshly Pergyra-built
  235-hard lanes pass with 20 body fixtures and one MIR fixture. The hard
  driver is `.tmp/bin_class_bump_option_match_235_hard/pgy-self-driver.exe`,
  SHA-256
  `AE573D90C1266DE447E9CC63EA71466E9F62ACFA3D348894DCB865B8C5798904`,
  with a 235-row manifest ending in `class_bump_option_match`.
- Hard native/self canonical MIR SHA-256 is
  `4EA70E1B407EADDE4B21F0F928CC82A2B6DDBBC39B9D3A3A9EEC0004500A7B7B`;
  oracle/self/source emitted-C SHA-256 is
  `BFA6F1EA4FA410122B51808BE04CCF4F953CAF191F058F201B8144C932098506`;
  runtime output is `5`, `10`, `10`, `0`. Missing Option payload type or
  `Counter_Bump` fails graph admission; removing `c.7` fails with
  `MIR phi facts are missing or inconsistent: Steps`.
- The twelve-fixture hard impact shard and component, shell syntax, diff, SoT
  authority, gate-owner, protocol-registry, and substitution-velocity gates
  pass. The full 235-row matrix was not run under the 30-minute budget; the
  last complete unfiltered matrix remains 230/230. Fixture 236 is not yet
  selected.
- Fixture 236, `tests/cases/backend_compare/class_within_class_chain/main.pgy`,
  preserves nested `Inner` identity through `Outer.WithNewTag` and a temporary
  member-call chain. MIR owns `Outer_WithNewTag` and `Outer_InnerId`; each
  missing target fails graph admission without dotted-text or C-type fallback.
  Focused C/LLVM/current-hard/new-hard and the seven-fixture nested-class shard
  pass. The new hard driver is
  `.tmp/bin_class_within_class_chain_236_hard/pgy-self-driver.exe`, SHA-256
  `48BCCE98B059CAE485420EFCF769262B9F4039073DE507AD5B28AAA07543D4BC`.
  Runtime output is `42`, `1`, `42`, `99`, `100`, `10`; full 236 remains an
  explicit budget omission and fixture 237 is not selected.
- Fixture 237,
  `tests/cases/backend_compare/class_method_short_circuit/main.pgy`, carries
  typed Bool-returning member calls through `logical_or`, `logical_and`, and
  `logical_not`. C/LLVM/current-hard/new-hard focused lanes and the seven-row
  Bool/class shard pass. The new driver is
  `.tmp/bin_class_method_short_circuit_237_hard/pgy-self-driver.exe`, SHA-256
  `D63ACF6742DC35657474E4F598E3462DBBE5EEC108F7CEEA5F78BE37BD121C02`.
  Runtime output is `1`, `2`, `0`, `1`; target and operator-kind mutations fail
  closed. Full 237 remains omitted and fixture 238 is not selected.
- Fixture 238,
  `tests/cases/backend_compare/class_recursive_factory/main.pgy`, carries a
  class-valued state through recursive `Train(LevelUp(Charge(...)))` calls and
  the final `State.Power()` projection. MIR owns the direct targets `Train`,
  `LevelUp`, and `Charge`, plus `State_Power`; removing any target fails graph
  admission without source-call reconstruction or a C-shaped fallback.
  Focused C/LLVM/current-hard/new-hard lanes and the six-fixture recursion/class
  shard pass. The hard driver is
  `.tmp/bin_class_recursive_factory_238_hard/pgy-self-driver.exe`, SHA-256
  `2124BAFB7A32A02315DE68653588DDA9E29740B83965F446DA550081E1FCEFF1`.
  Canonical MIR SHA-256 is
  `B913B640CAC865090F25904D98A8BC6E775C5EDB221151AF595A51425851B8DC`;
  emitted-C SHA-256 is
  `8659DBEC896573DE8D1D465ADD332935CEBF7039BB9A9EA6AB105426F7C3A712`;
  runtime output is `10`, `20`, `40`, `60`. Full 238 remains omitted and
  fixture 239 is not selected.
- Fixture 239,
  `tests/cases/backend_compare/enum_to_class_match/main.pgy`, returns the
  nominal `Stat` value from all three exhaustive `Class` match arms and carries
  it through `StatOf` into `s.val * s.scale`. One Pergyra-level parity owner
  verifies the complete variant/constructor/call/local/member spine instead of
  dividing it into C-shaped compiler paths. Removing `Stat`, `StatOf`, or the
  `Tank` declaration fails closed.
- Focused C/LLVM/previous-hard/new-hard lanes and the eight-fixture enum/class/
  wrapper/recursive shard pass. The new hard driver is
  `.tmp/bin_enum_to_class_match_239_hard/pgy-self-driver.exe`, SHA-256
  `7DFAC543959457B623423BF72451EC3D7273E99B4E648B6D5DD92D33CAAA3109`,
  with 239 manifest rows. Canonical MIR SHA-256 is
  `C56176FBC7A957839E6564C97762D9E5E38EBB4A2D35E8ABE4CBACF8271A1C12`;
  emitted-C SHA-256 is
  `E528C2F62DD94ACE037EA1EA78A850AEC3DAD78A8EF55B2B3F36FA6E2667F4A0`;
  runtime output is `100`, `100`, `90`. Full 239 remains omitted and fixture
  240 is not selected.
- Fixture 240,
  `tests/cases/backend_compare/class_method_enum_classify/main.pgy`, carries
  typed Bool decisions from `Counter.IsZero`, `IsBig`, and `IsPos` through
  `Classify -> Verdict`, an exhaustive match, and `Counter.value` consumers.
  The 239 enum-to-class test owner was replaced by one bidirectional
  class/enum composition owner; the old path is rejected so the semantic seam
  cannot split back into direction-specific fragments.
- Focused C/LLVM/previous-hard/new-hard lanes and the seven-fixture class/enum
  shard pass. The new hard driver is
  `.tmp/bin_class_method_enum_classify_240_hard/pgy-self-driver.exe`, SHA-256
  `D8FD169659A41883253ABCBBF636624E82E80DD8814FE6B84B57308C3EAA61EF`,
  with 240 manifest rows. Canonical MIR SHA-256 is
  `9D36D5AD76893D408F236D4A855E8DBB67C5C457E6E4108E9F6FA948ACE07D52`;
  emitted-C SHA-256 is
  `CD0E1060F1F0EABF650B8EEB1451B05060E5A5128E3275AB776512B87298DE1E`;
  runtime output is `0`, `5`, `1500`, `3`, `99`, `1010`. Predicate-member,
  `Classify`, and `Zero` mutations fail closed. Full 240 remains omitted and
  fixture 241 is not selected.
- Fixture 241, `tests/cases/backend_compare/class_user_box/main.pgy`, proves a
  user-declared nominal `Box` does not collapse into the generic builtin
  `Box<T>` owner. MIR carries the exact `Box`, `New`, `Box_WithWeight`, and
  `Box_Heavy` targets through fluent temporaries and field projections. Missing
  targets fail graph admission; removing the method routine identity fails at
  the class-method owner instead of falling back to a builtin interpretation.
- Focused C/LLVM/previous-hard/new-hard lanes and the six-fixture nominal/class
  shard pass. The new hard driver is
  `.tmp/bin_class_user_box_241_hard/pgy-self-driver.exe`, SHA-256
  `61DDEF412F281EFBF3DE8D72220C2D590256D08EDD634615E38B68E2AF5CD3FF`,
  with 241 manifest rows. Canonical MIR SHA-256 is
  `B0FD14CCFE846A752E345D4AA2DE8F6976B13AA80BAFA3B48682AFD509205AB1`;
  emitted-C SHA-256 is
  `AFFB7BE44EB3404D306C6AC986A5723E187BAB6799727E4FF96BFBE540B5EBFB`;
  runtime output is `false`, `true`, `0`, `5`. Full 241 remains omitted and
  fixture 242 is not selected.
- Fixture 242,
  `tests/cases/backend_compare/class_with_array_param/main.pgy`, carries one
  typed `Array<Int>` from `FillArr` creation, indexed mutation, and loop phi
  through the return boundary into `SumWith`, where it composes with the
  nominal `Slot2` value and a second indexed loop. One Pergyra-level
  class/array composition owner verifies the signatures, array and scalar phi
  chains, nominal member graphs, indexed-assignment graph, and target
  cardinality. No C pointer/array guess, source reparse, fixture compiler
  branch, native-MIR injection, backend fallback, or runtime fragment exists.
- Focused C/LLVM/previous-hard/new-hard lanes and the seven-fixture class/array
  shard pass. The new hard driver is
  `.tmp/bin_class_with_array_param_242_hard/pgy-self-driver.exe`, SHA-256
  `73499B3EAE8688A7DB9E2E8FD72467E6F3628E5CF61BB9FC446CC9B24C4BADDC`,
  with 242 manifest rows. Canonical MIR SHA-256 is
  `1DBD9F2297163F4C725FCE3C90ADD59DF71E9BDA3741F06669C455CF7AE9CB65`;
  emitted-C SHA-256 is
  `08A649224DB7521A377B401DF2450A14F065AAD5DABFB38CB95A392E2C6F27A6`;
  runtime output is `93`, `146`, `138`, `225`. Missing targets or the indexed
  target graph fail graph admission, and an `Unknown` array parameter fails
  with `assignment_type_unresolved`. Full 242 remains omitted and fixture 243
  is not selected.
- Fixture 243,
  `tests/cases/backend_compare/class_param_method_arr/main.pgy`, carries the
  `Int` identity of `rates[i]` from an `Array<Int>` parameter through the index
  node into `Bag2.Worth(rate: Int)`, then carries the method result through the
  `total` loop phi. The existing class/array composition owner now covers both
  directions instead of adding an array-to-method or C-emission fragment.
  Removing `Bag2`, `Bag2_Worth`, or `TotalWorth`, changing the index node to a
  leaf, or changing the array to `Array<String>` fails closed; the type
  mutation reports `call_arg_type_mismatch`.
- Focused C/LLVM/previous-hard/new-hard lanes and the eight-fixture class/array
  shard pass. The new hard driver is
  `.tmp/bin_class_param_method_arr_243_hard/pgy-self-driver.exe`, SHA-256
  `4A60C32EDA22778441FB3A309C88F0CF3378006AA6807407EAB82B6DF85F8697`,
  with 243 manifest rows. All four lanes produce canonical MIR SHA-256
  `854D22B250D3FA04F067050079FA7D10581316EDA0258C5769C2F4FF53D7848F`.
  Hard oracle/self/source emitted-C SHA-256 is
  `848F3290CF90348203718BF88B7B2E05FA88B64D9685837CBDFE9D15E61EB882`;
  runtime output is `1800`, `100`, `0`, `0`. Full 243 remains omitted and
  fixture 244 was not selected at that checkpoint.
- Fixture 244,
  `tests/cases/backend_compare/array_match_action_sim/main.pgy`, closes one
  Pergyra semantic path from the typed `prices[i]: Int` collection element to
  `DecideOf -> Action`, exhaustive `match`, and the `cash`/`shares`/`i` loop
  phis. Its objective priority is semantic identity and one CFG owner before
  consumer migration and fallback prevention. The fact owner is
  `MirRoutineGraphIsSameIterationMerge`; the last consumer is the Pergyra
  MIR-to-structured-AST routine index. A merge is now valid only when both arms
  reach it without re-entering the branch through a loop backedge. The old
  cyclic-distance choice, source reparse, fixture-name branch, native-MIR
  injection, backend reconstruction, and C runtime fragment are forbidden.
- The 243-hard baseline reconstructed the increment and `Continue` twice and
  failed canonical MIR consumption. Focused C, LLVM, and freshly Pergyra-built
  244-hard lanes pass with one increment and one `Continue`; the eight-fixture
  impact shard also passes. The new hard driver is
  `.tmp/bin_array_match_action_sim_244_hard/pgy-self-driver.exe`, SHA-256
  `F742594D3F60704CA5FA24153E7CB9364C3679974FD3308543B8E3CFFDE6DE9A`,
  with 244 manifest rows. Native/self canonical MIR SHA-256 is
  `751E8420182B99A7BEF93D45FF5B0D811F2D588D15A8FDAE6068CDD5CEF86EBD`;
  hard oracle/self/source emitted-C SHA-256 is
  `DC42840CD11F49A56512158289E66B35A84372713160224F5C4BACF5F2810773`;
  runtime output is `1060`, `1000`, `1000`.
- The fixture-244 falsifiers remove a required call target, change the index
  graph kind, remove `Hold`, delete a `cash` loop-phi input, or change
  `prices` to `Array<String>`; each fails closed at its owned graph, enum, phi,
  or semantic type boundary. The focused component, loop-flow, structural,
  shell, diff, SoT, protocol, and substitution-velocity gates pass. A broader
  unfiltered `mir_json_parity.sh` run stops at the pre-existing `option_match`
  carriage gap because native MIR has `match_bindings` without
  `match_binding_types`; this is an open broader gate, not a green fixture-244
  claim. Full 244 remains omitted, the last complete matrix is 230/230, and
  fixture 245 was selected and closed below.
- One diagnostic combined LLVM/previous-hard invocation stopped before the MIR
  fixture on the existing `valid_compound_local` body artifact: the runtimes
  frame identical C content with two terminal LF bytes versus one. The
  separate lanes and canonical semantic artifacts are green; do not describe
  this stdout framing as byte-equal cross-lane emitted C.
- Fixture 245,
  `tests/cases/backend_compare/coalesce_accumulate_loop/main.pgy`, carries
  `Array<Int>` element identity through `ParityVal(arr[i]): Option<Int>`, the
  coalesce node, and `total`/`i` loop state. The Pergyra semantic owner admits
  the coalesce only after `OptionCoalescePayloadTypeOpt` proves payload
  compatibility; no source reparse, fixture branch, native-MIR injection, or
  backend fallback exists. The previous 244-hard and native-oracle bridge both
  reject the fixture with `binop_type_mismatch` (`Int + Option<Int>`), providing
  the executable replacement witness.
- Focused C/LLVM/fresh-hard producer-first parity passes with
  `body_fixtures=20` and `mir_fixtures=1`; the eight-fixture current-hard
  Option/coalesce/array impact shard also passes. The current-tree hard driver
  is `.tmp/bin_coalesce_accumulate_loop_245_hard/pgy-self-driver.exe`, SHA-256
  `30D5204624512EEBDF39827F271E611AAC0C4AC73CAAA616CC4BC5729ED79ED3`,
  with 245 manifest rows. Hard oracle/self canonical MIR SHA-256 is
  `779AC39186B42C828EB671016B4A3D9B02FBAED97550F2BE899FF37A63E2B84D`;
  oracle/self/source emitted-C SHA-256 is
  `2D044764A426ACF6AB5BFFE44D7E639668197CD44D8A74CD298817DD0ED0D549`;
  runtime output is `117`, `18`, `-1`, `0`.
- Missing `ParityVal`, index identity, coalesce kind, or a loop-phi input fails
  closed. Component, shell syntax, diff, loop-flow, SoT authority,
  single-gate-owner, seven-row protocol-registry, and substitution-velocity
  gates pass. Full 245 remains an explicit budget omission, the last complete
  unfiltered matrix is 230/230, and the next executable replacement rung has
  not been selected. Row 246 below is coverage only.
- Manifest row 246,
  `tests/cases/backend_compare/coalesce_in_if_condition/main.pgy`, carries
  `MaybeFlag(arr[i]): Option<Bool>` through `?? false` into a Boolean branch
  inside the `count`/`i` loop. The existing coalesce payload owner is the
  semantic fact owner; the new parity owner consumes direct target, indexed
  element, coalesce, and loop-phi facts and rejects missing target, index,
  operator-kind, and phi-input mutations. Focused C/LLVM/fresh-hard
  producer-first parity passes with `body_fixtures=20` and `mir_fixtures=1`;
  canonical MIR SHA-256 is
  `CB6F19B233F03FC3C16551F9DEA57A80801020B509FB658E2601AE4B9CF79138`,
  emitted-C SHA-256 is
  `CDFFD8220B8FD9943D0DB116E55D1B687BB046BDB8E79EFB486A5DE6A0BF767B`,
  and runtime output is `3`, `0`, `1`. Component and shell syntax gates pass.
  The pre-existing 245-hard driver already accepted this row, and the slice has
  no Pergyra semantic implementation delta, so it must not be counted as an
  executable substitution frontier. The full 246-row manifest matrix remains
  an explicit budget omission.
- Manifest row 247,
  `tests/cases/backend_compare/nested_coalesce_chain/main.pgy`, carries two
  `HalvedIfPositive` calls through `Option<Int> ?? Int` into the local
  `first -> second` value chain. The parity owner checks both graph nodes and
  exactly two direct target facts; deleting a target or changing the first
  coalesce node fails closed, with the latter pinned to
  `initializer_type_unresolved` at the owned initializer boundary.
- Focused C, LLVM, and pre-existing 245-hard producer-first parity pass with
  `body_fixtures=20` and `mir_fixtures=1`. Hard canonical MIR SHA-256 is
  `3C518BBC3E89A82FFA538F99F6E205F8F60A8A4E16DF18E6BA20283A0ACDF7CF`;
  emitted-C SHA-256 is
  `B5E682F33D9CED51C492C5C4ED6BDC5AC12A47CE21519457DFB7543BE8F50F6E`;
  runtime output is `10`, `5`, `2`, `49`, `0`. Shell syntax and the current-tree
  component contract pass. Because 245-hard accepts the normal row and no
  Pergyra semantic implementation owner changed, row 247 is coverage only; it
  does not move the counted executable frontier beyond 245. The next executable
  replacement fixture has not been selected.
- Fixture 233,
  `tests/cases/backend_compare/class_result_chain_loop/main.pgy`, carries a
  `Wizard` through `Result<Wizard,DraftErr>`, match-bound next state, match
  merge, and a while backedge. MIR owns the chain
  `w.1 -> w.3 -> w.7 -> w.13 -> w.3`; error variants return explicitly.
  Compiler semantics gained no fixture/name branch, source re-scan,
  struct-copy guess, native-MIR injection, C fallback, or runtime shard.
- Focused C-built, LLVM-built, previous-232-hard, and freshly Pergyra-built
  233-hard lanes pass with 20 body fixtures and one MIR fixture. The new hard
  driver is `.tmp/bin_class_result_chain_loop_233_hard/pgy-self-driver.exe`,
  SHA-256
  `19CC79B10900099F60FFF64D81B9CE13BC527E6BF831CCE7108A69BE73D91E6A`,
  with a 233-row manifest ending in `class_result_chain_loop`.
- Hard native/self canonical MIR SHA-256 is
  `4130D3F3B898DD0FC917A64E58483517C3CAB528645125CC5FF7243B6410BBDE`;
  oracle/self/source emitted-C SHA-256 is
  `DDF21027CE91D240312391538980571B531EF5CDDF6AFDCD702D3FBA1FE42BA1`;
  runtime output is `100`, `0`, `20`, `0`. Removing `Ok(after): Wizard` fails
  expression-graph admission, while removing `w.7` from the merge phi fails
  with `MIR phi facts are missing or inconsistent: ManaPoints`.
- The ten-fixture hard impact shard passes. Component, shell syntax, diff, SoT
  authority, gate-owner, protocol-registry, and substitution-velocity gates
  also pass. The full 233-row matrix was not run under the 30-minute budget;
  the last complete unfiltered matrix remains 230/230.
- Before the next fixture admission, choose exactly one unsupported
  Pergyra-level semantic surface, write its objective card, and probe it
  without changing the manifest. Do not open a C-shaped runtime fragment or a
  parallel cleanup track merely to increase the fixture count.

### 1a. Explicit Result runtime SoT closure

- The explicit `Result<T,E>` runtime path is now owned by the self-hosted
  Result usage, type, runtime-ABI, declaration, and emission owners. Contextual
  constructors, `UnwrapErr`, returns, and local declarations consume the typed
  Result facts; the old generic runtime-name fallback is not part of this
  closed slice.
- Focused evidence observed on 2026-07-23: hard substitution contract passed;
  isolated codegen fixpoint passed with `gen2 == gen3` at 36,402 lines;
  self-host compiler build passed; the explicit custom-error fixture emitted
  typed Result ABI names, compiled, and ran with output `7`, `err`, `42`; and
  `UnwrapErr(Option<Int>)` failed closed with `builtin_arg_type_mismatch`.
- The current component contract is green. Match binding local-index and
  reconstruction responsibilities now live in named Pergyra owners;
  `routine_fact_index_owner.pgy` and `stmt_emit.pgy` both meet the 600-line
  cap. Fixtures 231-236 prove that the wrapper/runtime and class owners reach class
  payloads, enum errors, value-returning match control flow, typed class method
  calls, method-produced Results, and Result/Option loop-carried class/scalar
  state without adding a C-side inference path.

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
- The 2026-07-23 closure moved bounded direct-`Print` escape-fact production
  into semantic ownership at `src/semantic/region_escape_fact.c`. Semantic
  analysis emits stable allocation/scope/function IDs from the resolved
  `BuiltinKind` fact; the driver only validates those rows into the AIR-gated
  verified plan. `src/compiler/region_escape_v1.{h,c}` was deleted, and the
  component contract rejects its return. This executable closure is committed
  and pushed as `e8730785`.
- The follow-up moved the semantic region rows through the HIR projection owner
  at `src/compiler/hir_region_escape_facts.c`. HIR retains an owned copy,
  validates stable allocation/function identity, and the driver consumes only
  `hir->region_escape_facts`; missing or incomplete HIR carriage fails closed.
  This HIR SoT closure is committed and pushed as `c435b4c1`.
- The MIR retention follow-up moved the rows through
  `src/compiler/mir_region_escape_facts.c`. MIR owns a second stable copy,
  validates allocation-site/function identity, and the driver consumes only
  the MIR carrier before AIR-gated plan materialization. This closure is
  committed and pushed as `26a73693`.
- The next retention-summary follow-up moved the bounded `Print` argument
  policy behind `src/semantic/region_retention_summary.c`; the collector now
  asks that semantic owner for each argument and fails closed to HEAP when the
  summary is absent, unknown, or not for the first argument. Its self-host
  region-plan contract passed with 15 projection pins and 8 producer
  rejections, and the component contract rejects a direct `BUILTIN_PRINT`
  read in the collector.
- The following retention-summary extension added synchronous `Log`, `LogRaw`,
  `LogBanner`, and `LogBlock` consumers to the same owner. `Log` accepts all
  argument positions; the other log consumers remain first-argument-only, and
  absent or unknown summaries still fail closed to HEAP.
- The user-callee retention extension is now owned by
  `src/semantic/region_retention_summary_user.c`; the direct-sink fixture is
  `tests/cases/backend_compare/region_user_callee/main.pgy` and its
  return-rooted falsifier is the adjacent `_bad` fixture. The old broad
  callee-parameter class remains open.
- Observed green gates for this slice are region unit, verified-plan unit,
  arena, C/LLVM backend wiring, self-host region-plan, component contract,
  alternate-path LLVM-disabled/enabled compiler builds, SoT authority, and
  substitution velocity. The adequacy gate's Coq model was declared skipped
  because no prover is installed on this runner; its live owner checks passed
  with `PGY_ALLOW_MISSING_COQ=1`. The hard contract remains red only because
  concurrent fixture-235 work removed an expected `P_V` term; that file was
  not changed by this slice. The HIR-specific `test-hir` gate passed with
  25/25 tests, the region unit/backend gates passed, the self-host region-plan
  contract passed with 13 projection pins and 7 producer rejections, and the
  LLVM-enabled/disabled compiler builds passed. The MIR-specific test gate
  passed with 152/152 tests, the self-host region-plan contract passed with
  14 projection pins and 8 producer rejections, and both compiler build lanes
  passed. The retention-summary owner/collector slice then passed the region
  escape and plan unit gates, the component contract, and the 15-pin/8-
  rejection self-host region-plan contract. The Log-family extension passed
  the region escape/plan gates and both C/LLVM compiler builds; the aggregate
  component gate stopped on the concurrent 613-line routine owner cap. The
  user-callee extension then passed the region unit, backend wiring,
  self-host-region-plan 16-pin, component, and both compiler build gates.
- This closes the semantic producer-to-HIR-to-MIR retention and driver
  direct-read fallback seams, plus the bounded synchronous-builtin and
  direct user-callee retention-summary classes. The
  `resource.region_allocation_plan` registry row remains `BRIDGE` until the
  broader callee-parameter cases and complete region allocation ownership are
  migrated. Before widening the certified class, preserve the explicit HEAP
  default and choose the next missing owner fact with a falsifying fixture.

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
3. Resume the observed LLVM assignment-probe failure. Write its objective card
   only after locating the semantic/MIR owner of `UnwrapOption`'s concrete
   result type and the last LLVM consumer. The forbidden fallback is a
   call-name special case or text-derived return type; the focused gate is
   `tests/self_hosted/parity/assignment_projection_probe_parity.sh`.
4. Run the narrow owner gate first. For the current tree, useful focused gates
   include `match-binding-type-fact-test-smoke`, `mir-lowering-api-test-smoke`,
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
