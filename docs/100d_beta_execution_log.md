# Beta Readiness Checklist - Execution Order And Progress Log

> Split from `docs/100_beta_readiness_checklist.md` on 2026-05-29.
> Keep active blocker edits in the shard that owns the relevant closure track.

## Progress Log - 2026-06-06 C/LLVM Function Signature Fail-Closed

- C and LLVM function forward declarations and MIR body emission now fail
  closed when an active MIR routine row exists without signature metadata. The
  fallback to source AST function parameters and return types remains available
  only for no-MIR compatibility paths.
- C forward eligibility policy and C forward emission both report
  `MIR-only C path missing function forward signature metadata` instead of
  silently reopening AST function signatures.
- LLVM early-forward eligibility and LLVM function forward declaration emission
  both report `MIR-only LLVM path missing function forward signature metadata`
  for the same active-MIR defect.
- C and LLVM MIR function body emission now report `MIR-only C path missing
  function body signature metadata` and `MIR-only LLVM path missing function
  body signature metadata` before attempting any AST signature fallback.
- C MIR SSA-local declaration emission and LLVM MIR parameter-allocation
  emission now apply the same active-MIR signature guard. Missing signature
  metadata is reported as `MIR-only C path missing function SSA local signature
  metadata` or `MIR-only LLVM path missing function parameter signature
  metadata` instead of reopening source AST parameters inside helper-level
  lowering.
- C MIR signature eligibility policy now fails closed before checking source
  AST parameter and return shapes when an active MIR routine exists without
  signature metadata. This keeps the C "can emit from MIR" decision behind the
  MIR routine row instead of using AST as a silent compatibility substitute.
- C MIR local type lookup now resolves direct function-call return types from
  active MIR routine signature metadata when available. Source AST callable
  return fallback is limited to no-MIR compatibility mode, and the transpile
  fixture for generic ability specialization now supplies explicit MIR
  signature metadata instead of relying on source AST fallback.
- C MIR local parameter type lookup now consumes active routine signature
  metadata before consulting source AST parameters. If an active MIR routine row
  exists without signature metadata, local parameter lookup reports
  `MIR-only C path missing local parameter signature metadata` instead of
  silently using source AST parameter types.
- `MIRRoutine` kind/name/owner metadata now has a core accessor surface
  (`mir_routine_kind`, `mir_routine_name`, `mir_routine_owner_name`,
  `mir_routine_owner_ast_type`) with C and LLVM backend inventory wrappers.
  C/LLVM MIR function and parameter emission now consume those accessors in
  the closed slice instead of reopening selected routine fields directly.
- The accessor contract now covers the C/LLVM codegen routine lookup and
  contract owners as well: LLVM function inventory, intent flow, MIR contract
  validation, C intent routine collection, C MIR local type lookup, C MIR
  mapping precheck, and C MIR resource-op emission. A `src/codegen` scan for
  direct `routine->kind/name/owner_*` reads returns no hits.
- `build_source_inventory_smoke.sh` now normalizes scanner output paths to
  forward slashes at the scan owner. This keeps Git Bash/Windows `rg` output
  from bypassing allowlists as `src/codegen\...` false positives while leaving
  the C/LLVM owner checks unchanged.
- C/LLVM backend-compare evidence was extended on the deterministic default
  order after the path-normalization fix. With ABI precheck disabled for the
  targeted oracle run, ranges `0..11`, `12..35`, and `36..71` passed
  (`84/84`) across slot/secure-slot, arrays/slices, tuples, enum/match,
  generics, class methods, lexical shadowing, dynamic scope capture,
  Option same-binding regressions, loop control, and recursive calls. This is
  evidence for the registered support matrix prefix, not a full-suite claim.
- LLVM statement type inference now owns the `SetHas -> Bool` and
  `SetSize -> Int` collection facts alongside existing List/Map/Queue facts.
  This closes the `set_intersection_manual` C/LLVM compare gap where lowering
  knew `SetHas` but condition type inference still demanded registered
  function metadata. Targeted evidence: `set_intersection_manual` passed, and
  range `72..119` passed (`48/48`) with `PGY_BIN=/e/PergyraLang/bin/pgy.exe`
  and ABI precheck disabled.
- Additional C/LLVM backend-compare evidence now covers deterministic ranges
  `120..143` (`24/24`), `144..165` (`22/22`), `166..167` (`2/2`), and
  `168..191` (`24/24`) with the same explicit `PGY_BIN` and ABI precheck
  disabled. These ranges extend parity coverage through string/array/map/set/
  queue algorithms, sorting/search loops, recursive numeric helpers, and
  pipeline-style collection fixtures without requiring a new compatibility
  fallback.
- Range `192..215` also passed (`24/24`) under the same backend-compare
  settings, extending C/LLVM parity evidence across short-circuit call chains,
  nested loop joins, unary long arithmetic, substring/string formatting
  helpers, and enum payload/no-payload match lowering.
- Ranges `216..239`, `240..263`, and `264..287` also passed (`72/72`) with
  explicit `PGY_BIN` and ABI precheck disabled. This extends evidence across
  enum/result pipelines, generic boxes, scalar math/runtime conversion, file
  handle I/O, device slot runtime, string interpolation/split/join predicates,
  module namespace/export visibility, role/operator overloads, and recursion.
  No new compatibility fallback was added for these slices.
- Range `288..311` passed (`24/24`) with the same settings, covering nested
  calls, host-method returns, subject-method recursion with defer, branch/loop
  defer cleanup, boilerplate-reduction surface fixtures, intent trace/failure/
  rollback/authority observability, cross-world transfer, handoff frontier
  sync, zone mutation, and zone host-method ABI fixtures.
- Ranges `312..335`, `336..359`, and `360..383` passed (`72/72`) with the
  same settings. These slices cover subject/class/action projection dispatch,
  ownership forwarding, generic future/spawn specialization, default generic
  contracts, nested generic containers, ability/role/party/roster host methods,
  Result method chains, Option/coalesce class chains, intent header
  interleaving, map/list mutation and lookup, and for-in lowering over arrays
  and lists.
- Range `384..407` passed (`24/24`) with the same settings. This slice covers
  queue/set operations, Rc/Weak lifecycle, typed pin read/write views,
  secure-slot pin views, pin cleanup across successor/return/branch/break/
  continue paths, unsafe lexical boundary, and world/zone projection mutation
  fixtures.
- Ranges `408..431`, `432..455`, and `456..479` passed (`72/72`) with the
  same settings. These slices cover world/zone embedded action frontiers,
  relation/effect propagation and projection sync, authority failure surfaces,
  higher-order/lambda/event handlers, async spawn/await and future annotations,
  cancellation propagation, string spawn, channel pressure/status/select
  fairness, parallel channel communication, class-heavy method composition,
  nested class fields, enum fields, and algorithmic array/class fixtures.
- Range `480..503` passed (`24/24`) with the same settings. This extends the
  verified prefix through multi-match collision handling, nested branching,
  deeper Option class-chain and map-lookup fixtures, range method tests, Result
  class fields, chained class methods, string-carrying Result payloads, and
  nested Result propagation.
- Ranges `504..527`, `528..551`, and `552..575` passed (`72/72`) with the
  same settings. These slices cover Result pipelines, string-array tagging,
  class/stat composition, while/class method returns, array binary search,
  balanced split, inversion/counting/sliding-window algorithms, in-place array
  mutation and sorting, enum array dispatch, match payload destructuring,
  match branches with loops/breaks/lets/returns, same-binding multi-match, and
  Option composition/counting loops.
- Range `576..599` passed (`24/24`) with the same settings. This extends the
  verified prefix through Option loop consumption, complex Option match
  branches, default arguments, Result class propagation, string extraction/
  Caesar/reverse/repeat/run-length/split/strip/window helpers, triangle checks,
  triple function composition, and chained class factory fixtures.
- Range `600..623` passed (`24/24`) with the same settings after closing the
  LLVM indexed collection source-of-truth seam for current-host class fields.
  `nums[i]` inside class methods now resolves the field's registered
  `PgyArray_*`/`PgySlice_*` LLVM type to element metadata instead of requiring
  a local Array variable registration fallback. Evidence run:
  `mingw32-make pgy`, targeted `class_field_array_method`, range `600..623`,
  `backend_fail_closed_smoke.sh`, `perf_contract_smoke.sh`, and
  `documentation_quality_smoke.sh` passed.
- Ranges `624..647`, `648..671`, `672..695`, `696..719`, `720..743`,
  `744..767`, `768..791`, and `792..794` passed (`171/171`) with the same
  explicit `PGY_BIN` and ABI precheck disabled. This completes the
  deterministic default-order backend compare prefix through all `795/795`
  registered cases for this local Windows/Git-Bash LLVM-enabled build. The
  tail slices cover class-heavy factory/field/method chains, nested class and
  recursive traversal patterns, for-range fixtures, HashMap/List/Set basics,
  intent/zone/world fixtures, probes, slots, subject methods, string helpers,
  tree recursion, and zone/subject-slot fixtures. This is backend parity
  evidence for the current default registered case set, not a proof that every
  language surface is complete.
- The same local build also passed `backend_compare_llvm_coverage_smoke.sh`,
  `build_source_inventory_smoke.sh`, and `llvm_smoke.sh`. These gates keep the
  compare coverage allowlist, Makefile source inventory, and LLVM smoke surface
  aligned with the completed default-order compare evidence.
- The Korean/UTF hygiene scan found no current common Latin-1 mojibake payload
  in source docs/tests. `slot???` remains an intentional forbidden sentinel in
  `formal_semantics_smoke.sh`, not user-facing broken Korean. Evidence:
  `documentation_quality_smoke.sh` passed.
- `mir-declaration-inventory-test-smoke` now freezes the active-MIR signature
  guard across C policy, C forward/body/SSA-local emission, LLVM policy, and
  LLVM forward/body/parameter emission, plus C MIR signature eligibility,
  C MIR local function-call return and parameter lookup, and the selected
  `MIRRoutine` metadata accessor contract. Evidence run:
  `git diff --check`, `mir_declaration_inventory_smoke.sh`, WSL `make pgy`,
  WSL `make llvm-test-smoke`, and WSL `make test-transpile` (`898/0`) passed.

## Progress Log - 2026-06-06 LLVM Domain Forward AST Compatibility Flag

- LLVM domain/role method forward declaration helpers now require an explicit
  `allow_ast_compat` flag before falling back from `MIRDeclMethod` metadata to
  source AST method names, parameters, or return types.
- Hosted domain/role method forward declarations pass `allow_ast_compat` only
  for no-MIR compatibility rows (`method_meta == NULL`). MIR-backed method rows
  consume MIR method metadata and fail through existing inventory diagnostics
  instead of silently reopening source AST.
- Ability vtable emission remains explicitly AST-compatible because that path
  still owns an AST-only declaration surface. Role operator forward emission is
  explicitly metadata-only.
- `mir-declaration-inventory-test-smoke` now freezes the flag contract for the
  shared method signature helpers and the domain/role/ability call sites.
  Evidence run: `git diff --check`, `mir_declaration_inventory_smoke.sh`, WSL
  `make pgy`, and WSL `make llvm-test-smoke` passed.

## Progress Log - 2026-06-06 C/LLVM Method Return Metadata Fallback Tightening

- LLVM method return inference now treats missing `MIRDeclMethod` return
  metadata as a MIR inventory defect when MIR is active, instead of reopening
  source AST method return types.
- C method return inference, member-call post-sync wrapping, and hosted-method
  forward declaration emission keep AST method return/parameter fallback only
  for no-MIR compatibility mode. MIR-active C inference no longer reopens
  source AST method return types and falls through to the existing
  unknown/concrete-type failure path; hosted forward declarations fail closed
  when MIR method rows are missing.
- LLVM identifier-call inference now consumes scalar builtin return facts before
  current-host method fallback. This prevents standard calls such as
  `ChannelClosed(ch)` inside a class method from being misclassified as
  `CurrentHost.ChannelClosed`.
- `mir-declaration-inventory-test-smoke` now requires the active-MIR guards
  around these return/forward-metadata fallbacks and freezes the
  builtin-before-host dispatch order. Evidence run: `git diff --check`,
  `mir_declaration_inventory_smoke.sh`, `backend_fail_closed_smoke.sh`, WSL
  `make pgy`, WSL `make llvm-test-smoke`, and WSL `make test-transpile`
  (`898/0`) passed.

## Progress Log - 2026-06-06 LLVM Member Call Metadata Fail-Closed

- LLVM general member-call lowering now applies the same MIR-active
  `MIRDeclMethod` requirement as hosted self-call lowering. If a member-call
  method row is missing while a MIR program is active, LLVM reports a MIR
  inventory-missing diagnostic instead of reopening source AST method lookup.
- The AST method fallback remains available only for no-MIR compatibility mode.
  This keeps legacy AST-only emission usable while preventing MIR-driven LLVM
  member calls from bypassing declaration metadata.
- `mir-declaration-inventory-test-smoke` now requires the LLVM member-call
  MIR-active guard and inventory-missing diagnostic. Evidence run:
  `git diff --check`, `mir_declaration_inventory_smoke.sh`,
  `backend_fail_closed_smoke.sh`, and WSL `make llvm-test-smoke` passed.

## Progress Log - 2026-06-06 C/LLVM Hosted Member Call Metadata Fail-Closed

- LLVM hosted self-call lowering now requires `MIRDeclMethod` metadata when a
  MIR program is active. Missing hosted-method metadata reports a MIR
  inventory-missing diagnostic instead of falling back to the current source
  host declaration.
- C nominal member-call lowering now applies the same policy: AST method lookup
  remains available only in no-MIR compatibility mode, while MIR-active mode
  fails closed if the member-call method row is absent from declaration
  metadata.
- `mir-declaration-inventory-test-smoke` now requires the MIR-active guards and
  inventory-missing diagnostics in both call sites. Evidence run:
  `git diff --check`, `mir_declaration_inventory_smoke.sh`,
  `backend_fail_closed_smoke.sh`, WSL `make pgy`, WSL `make test-transpile`
  (`898/0`), and WSL `make llvm-test-smoke` passed.

## Progress Log - 2026-06-06 C Return Callable Context Metadata Owner

- C backend return-expression lowering now consumes
  `TranspilerCtx.current_return_callable_type` instead of reopening
  `ast_func_return_type((ASTNode *)ctx->current_func_decl)` to recover an
  event-handler return contract.
- Function and MIR routine emission set the callable return context explicitly.
  The C MIR emit-state snapshot now saves/restores that context, and generated
  Bool/Void-style owners clear it when setting their return type so stale
  function metadata cannot leak into wrapper emission.
- `backend-fail-closed-test-smoke` rejects reintroducing the AST current-func
  return fallback in the C return policy owner and requires the context fact to
  stay wired. Evidence run: `git diff --check`, `backend_fail_closed_smoke.sh`,
  WSL `make pgy`, WSL `make test-transpile` (`898/0`), and WSL `make test-mir`
  (`78/0`) passed.

## Progress Log - 2026-06-06 C/LLVM Method Boundary Metadata Fail-Closed

- Zone action and world-effect synchronization now treat `MIRDeclMethod`
  metadata as required when a MIR program is active. If the method row is
  missing in MIR-active mode, C and LLVM set the MIR inventory-missing
  diagnostic instead of reopening the source AST method contract.
- AST method fallback remains only for no-MIR compatibility mode. This keeps
  legacy AST-only emission usable while preventing MIR-driven backends from
  silently bypassing the declaration metadata source of truth.
- `mir-declaration-inventory-test-smoke` now requires the MIR-active guard near
  each method-metadata fallback branch. Evidence run: `git diff --check`,
  `backend_fail_closed_smoke.sh`, `mir_declaration_inventory_smoke.sh`, and WSL
  `make llvm-test-smoke` passed.

## Progress Log - 2026-06-06 LLVM Zone Boundary Context Metadata Owner

- LLVM zone authority checks and current-host declaration lookup now consume
  `LLVMGenCtx.current_within_zone_name` instead of reopening
  `ast_func_within_zone(ctx->current_func_decl)` at the point of use.
- AST function emission sets the context fact directly. MIR routine emission
  consumes `MIRRoutine.within_zone` through the LLVM routine inventory view, so
  MIR codegen no longer reopens source AST to recover the active zone boundary.
- `backend-fail-closed-test-smoke` rejects reintroducing the AST current-func
  zone fallback in `llvm_decl_authority.c`, `llvm_inventory_decl_lookup.c`, and
  `llvm_mir_emit.c`. `mir-declaration-inventory-test-smoke` freezes the
  `MIRRoutine.within_zone` accessor surface for C and LLVM inventory consumers.

## Progress Log - 2026-06-06 LLVM Return Context Metadata Owner

- LLVM function, MIR routine, lambda, intent, main-wrapper, sync-wrapper,
  parallel/async wrapper, spawn-wrapper, and role-operator emission now keep
  function return metadata in `LLVMGenCtx` as explicit context facts:
  `current_function_ret_type`, `current_return_type_name`, and
  `current_return_callable_type`.
- Return statement lowering, Result suffix inference, callable return context,
  and the `?` operator no longer reopen
  `ast_func_return_type(ctx->current_func_decl)` to recover the active
  function return contract. Generated wrapper functions explicitly clear the
  source-level return name/callable metadata to prevent stale caller metadata
  from leaking across LLVM function emission.
- `backend-fail-closed-test-smoke` now rejects reintroducing the AST return
  fallback in LLVM return/Result lowering owners and requires the new context
  facts to remain wired.
- Evidence run: `backend_fail_closed_smoke.sh` passed under Git Bash,
  `git diff --check` passed for the touched LLVM return-context slice, and
  WSL `make llvm-test-smoke` passed with the Linux LLVM-18 toolchain.

## Progress Log - 2026-06-06 C Parallel Capture Metadata Owner

- Moved C parallel/async event-handler capture type discovery behind
  `transpiler_parallel_capture`. The C emitter now consumes captured type AST
  metadata instead of directly calling the local type-AST lookup from
  `transpiler_async_parallel_emit.c`.
- `backend-fail-closed-test-smoke` now rejects reintroducing that direct lookup
  in the emitter while requiring the capture owner to carry the type metadata.
- Evidence run: MinGW `gcc -fsyntax-only` on
  `transpiler_parallel_capture.c` and `transpiler_async_parallel_emit.c`,
  `git diff --check` for the touched files, and
  `make backend-fail-closed-test-smoke` passed locally.

## Progress Log - 2026-06-06 C MIR Residual DEF Type Registry

- C MIR residual `MIR_INST_DEF` emission now reads local binding type names
  from the active typed registry before expression inference. The block emitter
  no longer reopens function-body local type AST/name lookup for this path.
- `cfg-body-dataflow-test-smoke` now rejects reintroducing
  `transpiler_find_local_type_ast(ctx, ...)` or
  `transpiler_find_local_type_name(ctx, ...)` in
  `transpiler_mir_block_emit.c`.
- Evidence run: MinGW `gcc -fsyntax-only` on
  `transpiler_mir_block_emit.c`, `git diff --check` for the touched slice, and
  `make cfg-body-dataflow-test-smoke` passed locally.

## Progress Log - 2026-06-06 C MIR Destructure Type Registry

- C MIR destructuring now recovers identifier initializer types from the active
  typed registry before requiring a concrete lowered type. It no longer calls
  `transpiler_find_local_type_name(ctx, ctx->current_func_decl, ...)` from the
  destructure emitter.
- `cfg-body-dataflow-test-smoke` now rejects reintroducing that direct local
  type lookup in `transpiler_mir_destructure_emit.c`.
- Evidence run: MinGW `gcc -fsyntax-only` on
  `transpiler_mir_destructure_emit.c`, `git diff --check` for the touched
  slice, and `make cfg-body-dataflow-test-smoke` passed locally.

## Progress Log - 2026-06-06 C/LLVM Generic Declaration Metadata Seam

- Removed the remaining declaration-specific generic parameter consumers from
  `src/codegen` and `src/compiler/module_normalizer_refs.c`. C forward policy,
  LLVM forward policy, LLVM routine declaration inventory, C generic binding
  queries, LLVM generic spawn specialization, C generic class/ability lowering,
  and module import-name normalization now read generic parameters through
  `ast_declaration_generic_params(...)`.
- Tightened `mir-declaration-inventory-test-smoke` so codegen cannot reintroduce
  declaration-specific generic metadata reads. `semantic-core-shape` now applies
  the same rule to module normalization. This keeps generic eligibility,
  generic specialization, and import-name normalization attached to the
  declaration metadata seam rather than backend/local AST rediscovery.
- Evidence run: `make mir-declaration-inventory-test-smoke`, `make pgy`,
  `make llvm-test-smoke`, `make test-transpile` (`898/0`),
  `make perf-contract-test-smoke`, and `make semantic-core-shape-test-smoke`
  passed locally under WSL.
- Follow-up source-of-truth tightening: `MIRRoutine` now materializes
  `generic_param_count`, and C/LLVM forward-declaration policy consumes that
  routine metadata before the AST compatibility fallback. `MIRDeclHeader` also
  materializes declaration generic parameter rows (`name`, `constraint`,
  `default_type`) for hosted/domain declarations and abilities; LLVM generic
  formal-default resolution now reads those header rows before the AST fallback.
  `MIRDeclHeaderInventory` and `LLVMMIRDeclHeaderInventory` now expose the
  declaration header list through an explicit inventory view, so the LLVM
  type-map MIR-present path iterates header rows without direct `ctx->mir`
  probing. Function declarations are now also recorded as declaration headers,
  so generic formal-default lookup no longer reopens AST function inventory in
  the MIR-present path; the AST inventory loop remains only for the no-MIR
  compatibility path.
  Declaration-header lookup also now has a typed public query
  (`mir_find_decl_header_of_type`) with matching C/LLVM inventory wrappers, so
  adding function headers cannot make a name-only lookup shadow a different
  declaration kind.
  `mir-declaration-inventory-test-smoke` freezes these contracts, including the
  ban on raw backend `ctx->mir` reads outside inventory owners. Evidence for the
  follow-up slice: `make pgy`, `make mir-declaration-inventory-test-smoke`,
  `make test-mir` (`78/0`), and `make llvm-test-smoke` passed under WSL;
  changed C files also pass local MinGW `gcc -fsyntax-only`.

## Progress Log - 2026-06-06 Worker Boundary UB Source-Of-Truth

- Centralized growable/synchronization-backed worker-boundary storage
  classification behind `src/common/worker_boundary_storage_policy.{h,c}`.
  Semantic analysis, C lowering, and LLVM lowering consume that owner instead
  of carrying separate Array/Slice/List/Queue/Set/HashMap/Channel display
  vocabularies.
- Tightened the policy to classify both concrete generic names
  (`HashMap<String, Int>`, `Channel<Int>`) and raw constructor names
  (`HashMap`, `Channel`) as unsafe worker-boundary storage. Missing or
  partially rendered generic metadata can no longer make a storage type look
  safe by losing its type arguments.
- Added semantic regressions for borrowed `Slice<T>` and `HashMap<K, V>`
  capture/transport across `parallel` and `spawn`, and pinned the regression
  names in `worker-boundary-ub-test-smoke`.
- Evidence run: `make test-semantic`, `make test-transpile`,
  `make worker-boundary-ub-test-smoke`,
  `make memory-concurrency-model-test-smoke`,
  `make backend-fail-closed-test-smoke`,
  `make mir-declaration-inventory-test-smoke`,
  `make runtime-frontier-contract-test-smoke runtime-frontier-policy-test-smoke`,
  `make build-source-inventory-test-smoke`, targeted backend compare for
  `await_inline_spawn`, and backend compare shards 1/20 and 2/20 all passed
  locally under WSL.
- Follow-up: `tests/build_source_inventory_smoke.sh` now routes repeated
  recursive source scans through an executable `rg` fast path, a `git grep`
  fallback that also scans untracked files, and only then portable `grep -R`.
  This keeps the source-of-truth sentinel broad without forcing every local/CI
  check to pay repeated full-tree traversal cost.

## Progress Log - 2026-06-05 Backend Fail-Closed Hardening

- Promoted backend fail-open guards into the frozen smoke surface:
  `backend-fail-closed-test-smoke` now runs in Linux, macOS, and Windows CI and
  is listed in the beta freeze contract.
- Tightened the fast gate beyond literal C fallback strings and synthetic LLVM
  branch conditions. It now also rejects direct LLVM `i32 0` expression-result
  placeholders outside the central `llvm_void_expression_placeholder(...)`
  owner and rejects partial LLVM host-declaration lookup chains that bypass
  `host_decl_compat.c`.
- Removed two direct LLVM void-call placeholders in `RcDrop` and `WeakDrop`;
  both now consume `llvm_void_expression_placeholder(...)`.
- Closed two silent-skip metadata holes: C/LLVM role method emission now errors
  when MIR-backed role/method name metadata is absent, and LLVM destructuring
  let emission errors when a binding name is missing instead of skipping the
  binding.
- Removed empty generated-expression fallbacks from the C domain-query, I/O,
  and misc builtin format owners. Formatting/allocation failures now set a
  backend diagnostic and return `NULL` instead of emitting an empty C fragment.
- Tightened the shared C formatting owner as well: `strdup_fmt(...)` now returns
  `NULL` on `vsnprintf`/allocation failure instead of materializing `""`.
  Literal string escaping keeps its separate input-policy empty-string path.
- Tightened the same empty-string failure pattern in the C type-name renderer
  for `Channel<T>`/`Future<T>` wrappers. A type-name formatting/allocation
  failure now reports a backend diagnostic instead of returning `""`.
- Closed the missing-type C mapping fallback:
  `pergyra_primitive_to_c(NULL)` now returns `NULL`, and
  `pergyra_type_to_c_copy(NULL, ...)` now fails closed and leaves the caller
  buffer empty instead of materializing `int32_t`. The backend fail-closed
  smoke gate freezes this as a regression test because missing semantic type
  facts must not become concrete C types.
- Closed the matching AST type-name rendering fallback:
  `render_type_name_in_ctx(ctx, NULL)` now returns `NULL` instead of
  materializing `Int`, and malformed `AST_TYPE` / nameless generic-argument
  render paths now fail the whole render instead of writing `Int` into the
  type-name buffer. `pergyra_ast_type_to_c_copy(NULL, ...)` remains the
  explicit ABI owner for the intentional no-return-type-to-`void` policy.
- Closed the C slot-reference empty-expression fallback: `slot_ref_expr(...)`
  now reports a backend diagnostic and returns `NULL` when the lowered slot
  expression is absent instead of emitting `""`, and the slot builtin/member/
  dispatch call sites now propagate that failure instead of formatting an
  invalid C expression.
- Tightened the slot builtin formatter owner: generated slot builtin fragments
  now use a context-aware formatter that reports formatting/allocation failure
  through the backend diagnostic channel instead of returning an unannotated
  `NULL`.
- Closed C declarator type fallbacks: malformed or unmapped AST type facts in
  typed declarators, event-handler parameter/return declarators, function
  pointer declarators, and function signatures now report a backend diagnostic
  and fail closed instead of being materialized as `void *`, `int32_t`, or
  implicit `void`. The intentional no-return-type case still renders `void`.
- Locked function-typed values behind the declarator owner: raw AST type-to-C
  copying now rejects `func(...) -> ...` / `AST_EVENT_HANDLER_TYPE` instead of
  returning `void *`, while `pergyra_ast_typed_declarator_in_ctx(...)` remains
  the explicit owner that renders the correct function-pointer ABI. Event
  declaration emission now requires concrete parameter types instead of falling
  back to `void*`.
- Locked MIR type rendering behind explicit type facts:
  `mir_render_type_name(...)` now propagates missing type nodes, nameless
  `AST_TYPE`s, unsupported `Future<T>` / `Channel<T>`, and generic inner-type
  failures instead of silently materializing `Int`, `Future<Int>`, or
  `Channel<Int>`. MIR intent metadata no longer recovers a failed rendered
  value type by emitting only the outer AST type name.
- Locked the matching DIR type-ref rendering path: DIR type references now
  propagate missing base names and unsupported generic/async inner types instead
  of manufacturing `Int` inside the domain graph.
- Centralized the backend `Unknown` sentinel policy in the type mapping owner:
  inferred `Unknown` and generic sentinel wrappers such as `Option<Unknown>` /
  `Future<Unknown>` are no longer registered as typed-var facts after C let
  emission, and LLVM Result layout now consumes the same token policy instead
  of owning a separate scanner. User-defined names such as `UnknownError`
  remain valid because the guard only rejects standalone `Unknown` sentinels.
- Closed the matching LLVM callable-parameter metadata hole: MIR-backed
  function-typed parameters now register callable facts in the parameter emit
  owner, and LLVM call type inference consumes the same callable registry before
  falling back to visible function declarations. This keeps higher-order calls
  such as `f(x)` fail-closed without requiring a duplicated AST scan.
- Removed the old callable-variable emission fallback that rescanned
  `current_func_decl` parameters to rediscover function-typed callable
  signatures. Callable variable calls now consume the callable registry as the
  single source of truth, and the fail-closed / inventory / perf smoke gates
  reject reintroducing the AST parameter rescan.
- Removed the matching LLVM slot-identifier fallback that rescanned
  `current_func_decl` parameters to rediscover `Slot<T>` / `SecureSlot<T>`
  inner types. Slot operations now require the slot registry populated by
  parameter/local emission, and the fail-closed / inventory / perf smoke gates
  reject reintroducing the AST parameter rescan.
- Removed the LLVM call type-inference fallback that rescanned `AST_WITH_STMT`
  bodies to recover with-slot alias inner types. `with slot<T> as s` now relies
  on the slot registry populated at with-block entry, matching the same
  registry-only rule used by slot parameters and slot locals.
- Removed the LLVM nominal type-inference fallbacks that rescanned the current
  function body to rediscover local `let` annotations. Nominal identifier
  lowering now consumes the active LLVM scope / var-class registry / host-field
  and enum metadata owners only, so declaration order and shadowing cannot be
  bypassed by a whole-body AST scan.
- Blocked C/LLVM backend `parallel` / `async` pointer-capture of mutable collection
  values (`Array<T>`, `Slice<T>`, `List<T>`, `Queue<T>`, `Set<T>`,
  `HashMap<K,V>`). These runtime containers can grow or rehash, so sharing them
  into worker wrappers by raw pointer would recreate the classic concurrent
  rehash/read UB pattern. Captured collection state must now cross through a
  channel/result boundary or an explicit copy.
- Removed the duplicate C parallel-capture local type-AST walker. The
  `transpiler_parallel_capture` owner now only discovers capture names, while
  function/event-handler type AST recovery goes through the existing
  `transpiler_mir_local_type_ast_lookup` owner. The perf contract rejects
  reintroducing a parallel-capture-specific local type walker.
- Removed the remaining C `parallel` emit fallback that rediscovered captured
  local types by calling `transpiler_find_local_type_name(...)` during wrapper
  field emission. Capture discovery now resolves/registers typed captures
  before emission, and the `parallel` / `async` emit owner consumes the typed
  registry only.
- Centralized the C/LLVM MIR match-case subject compatibility lookup behind
  `pgy_codegen_match_subject_for_case(...)`. This does not claim that the AST
  compatibility path is gone; the owner still calls
  `ast_find_match_subject_for_case(...)` until MIR carries a subject fact.
  The important closure is that C/LLVM match condition emitters no longer own
  separate function-body rescans, and smoke gates now reject reintroducing that
  direct backend rescan.
- Added `llvm_scope_lookup_snapshot(...)` as the registry-owned way to carry
  a scope entry across later scope mutation. `llvm_scope_lookup(...)` remains
  explicitly borrowed because `llvm_scope_declare/push/pop` can invalidate frame
  storage and lookup caches. MIR match payload remapping, MIR pin token aliasing,
  MIR pin enter/exit locals, MIR resource-view aliasing, MIR for-in list/index
  metadata, MIR range-loop body aliasing, MIR block-local copy rebinding,
  legacy LLVM statement-loop for-in bindings, and LLVM let-resource/view
  lowering now snapshot `alloca/type/name` before any alias declaration can grow
  the scope frame. Existence-only checks now use `llvm_scope_contains(...)`, and
  the perf contract rejects mixing borrowed `llvm_scope_lookup(...)` with
  `llvm_scope_declare/push/pop` outside the registry owner, API declaration, and
  the intentional parallel-capture frame identity check.
- Extended the same fail-closed policy to role operator/vtable lowering:
  C/LLVM role operator method-name metadata, LLVM role operator forward-name
  metadata, LLVM registered operator-method functions, and C/LLVM role vtable
  method-name/ability-name metadata now fail through the backend metadata path
  instead of being skipped or materialized as null function slots. LLVM role
  forward declaration emission also fails closed when role name metadata is
  absent. The C role operator alias path fails closed when MIR role declaration
  or subject-type metadata is missing, leaving `continue` only for the
  legitimate "this operator is not implemented by the role" probe result.
- Local verification: `make test-transpile` (`893/0`),
  `make test-dir` (`9/0`), `make test-rir` (`18/0`), `make test-air`
  (`119/0`), `make test-mir` (`77/0`), and
  `make backend-fail-closed-test-smoke`; latest narrow LLVM/source-of-truth
  gates: `make backend-fail-closed-test-smoke
  mir-declaration-inventory-test-smoke perf-contract-test-smoke
  llvm-test-smoke`.

## Progress Log - 2026-06-05 Real-Coverage Scope Test And Strict-Mode Repair

- Real-coverage scope: compiled every `func Main` entrypoint across
  `examples/`, `tests/cases/backend_compare/`, and `src/self_hosted/`
  (~976 sources) through both C and LLVM backends using
  `/tmp/pgy-PergyraLang-bin/pgy` (Linux ELF). Diagnosed earlier scope-test
  miscount as path mangling by a stale Windows PE `bin/pgy.exe` — real
  measurements require explicit POSIX-native binary selection.
- Baseline before today's closures: `bc_cases` 779/0 C, 779/0 LLVM;
  `examples` 102/12 C, 89/25 LLVM; `self_hosted` 71/12 C, 70/13 LLVM
  (parser fixtures are deliberate edge cases).
- Closure #72: parallel/async block capture now restores the slot inner
  type and security flag via
  `llvm_lookup_slot_inner`/`llvm_lookup_slot_is_secure` and
  `llvm_register_slot_var_binding`, mirroring the existing channel/future
  restoration. Without this restore, slot-typed bindings (`Slot<Int>`)
  captured into a `parallel { ... }` task lost their inner type metadata at
  the wrapper boundary and tripped
  `LLVM slot operation on '<name>' requires a concrete slot inner type`
  under strict mode. Recovered `channel_parallel.pgy`,
  `concurrency_demo.pgy`, `slots_simple.pgy`, `spawn_test.pgy`,
  `test_parallel.pgy` (+1 additional example knock-on).
- Closure #73: bare `Clone(x)` call sites now fall through type inference
  to their argument's inferred type. `llvm_expr_call_dispatch.c:92` already
  lowers `Clone` as an identity pass-through; the type-inference pass had
  no matching shortcut, so concurrent strict-mode tightening
  (`b3296be2`) made it surface as `call 'Clone' requires registered
  function or expected type metadata`. Recovered
  `relation_effect_propagation`, `world_derived_states`,
  `world_zone_cross_queries`, and `world_zone_projection_visibility` in
  `tests/cases/backend_compare/`.
- Closure #74: when a MIR block reaches the backend with no successors and
  no terminating return — the canonical shape produced by an exhaustive
  `match` where every case returns — the LLVM emit now emits
  `unreachable` instead of erroring with
  `LLVM MIR block %d reached backend without a terminal return value`.
  The earlier error was load-bearing as a fail-closed for genuinely missing
  returns, but it consumed legitimate exhaustive matches as collateral. The
  LLVM verifier still rejects truly live paths, so the safety contract is
  preserved. Recovered 150 backend-compare cases at once
  (every `match`-everything-returns shape across enum/option/result/match
  fixtures).
- Backend evidence after #72 + #73 + #74:
  `tests/compare_backends.sh tests/cases/backend_compare/*/main.pgy`
  reports 794/794 passed, 0 failed; `make llvm-dnd-campaign-test-smoke`
  reports `dnd_tavern_campaign C/LLVM parity ok`;
  `make llvm-campaign-projection-test-smoke` reports
  `campaign_graph_fsm LLVM projection parity ok`. Real-coverage scope:
  `bc_cases` 779/0 C, 779/0 LLVM; `examples` 102/12 C, 95/19 LLVM
  (LLVM gap closed by 6 vs baseline); `self_hosted` 71/12 C, 70/13 LLVM
  (deliberate parser fixtures untouched).

## Progress Log - 2026-06-05 Intent Declaration Carrier Closure

- MIR intent lowering now materializes declaration-level `priority` and
  `success` as semantic carriers: `IntentEval(priority)` and
  `IntentCheck(success)`, anchored by the intent routine name. Step-level
  carriers remain unchanged and top-level carriers do not enter the step-name
  sequence.
- C and LLVM intent declaration emission consume those carriers when a MIR
  routine exists and fail closed through the existing MIR intent-carrier
  diagnostic path if a carrier row exists without its expression payload.
- Tightened the MIR negative test lifecycle: the signature-drift case now
  restores the mutated `param_count` before `mir_destroy(...)`, preventing a
  validator fixture from turning its deliberate metadata corruption into a
  teardown-time heap error.
- Evidence: `mir-declaration-inventory-test-smoke`, `test-mir` (`76/0`),
  `pgy`, and targeted backend compare for `intent_no_step_call`,
  `probe_cursor`, and `probe_dnd_minimal` pass locally.

## Progress Log - 2026-06-04 Backend Runner Stale-Binary Guard Closure

- Backend runner executable selection is now fail-closed for stale or
  cross-platform binaries. `tests/pgy_binary_path_helpers.sh` owns the
  current-host runnable check and classifies PE, ELF, and Mach-O from magic
  bytes before falling back to `file(1)` text. This prevents a stale Linux
  `bin/pgy.exe` from being launched under Windows Git Bash simply because the
  filename ends in `.exe`.
- `tests/compare_backends.sh` applies the same runnable-binary policy to both
  `PGY_BIN` and `PGY_ABI_PIPELINE_TEST_BIN` before the C/LLVM compare or ABI
  same-process precheck can launch. The PowerShell fallback now quotes both
  the PATH prefix and ABI executable path through the shared quote helper.
- The PowerShell runtime PATH prefix now excludes Git for Windows'
  `mingw64\bin` when `MSYSTEM_PREFIX=/mingw64` resolves there. That directory
  can shadow the real MinGW runtime and crash `test_abi_pipeline.exe` before
  the ABI same-process test prints diagnostics, so explicit LLVM/MinGW runtime
  roots own PowerShell launch priority.
- Local bench/perf/dogfood scripts now consume the same helper seam. The bench
  script no longer hardcodes `/usr/bin/time`; it uses a shell clock fallback
  so Git Bash/macOS layouts do not turn benchmarking into a tool-path failure.
- The P0 CI smoke layer now uses the same runnable-binary seam for formatter,
  stdlib, AIR JSON schema, runtime-none, raw-escape, and semantic
  fixture-isolation probes. `pgy_select_optional_exe_binary(...)` selects an
  existing `.exe` even when it is stale, and
  `pgy_require_runnable_binary_here(...)` rejects it before launch; default
  source-only smokes may still skip when no explicit binary was supplied.
- Bash and PowerShell runtime PATH setup both reject Git for Windows runtime
  mounts as explicit MinGW/LLVM priority candidates. Existing Git Bash PATH
  entries may remain later in `PATH`, but they no longer shadow the real
  runtime directories prepended by the helper.
- Evidence: normal ABI precheck plus targeted backend compare passes for
  `probe_cursor`; stale `bin/pgy.exe` is rejected for backend compare ABI
  precheck, bench, perf baseline, and dogfood WebGL; `backend-compare-
  inventory-test-smoke`, `backend-compare-llvm-coverage-test-smoke`,
  `build-source-inventory-test-smoke`, `dogfood_webgl_smoke.sh`, and a reduced
  `perf_c_baseline_smoke.sh` run all pass. A Makefile shard with precheck
  enabled passed ABI same-process and then exceeded the 300s interactive tool
  limit while running backend cases; this is time budget, not a code failure.
  `llvm-test-abi-same-process` now passes through the Makefile path with
  `196 passed, 0 failed`. Follow-up gates on 2026-06-05:
  `fmt-test-smoke`, `stdlib-test-smoke`, `air-json-schema-test-smoke`,
  `runtime-none-contract-test-smoke`, `raw-escape-contract-test-smoke`,
  `semantic-fixture-isolation-test-smoke`, and
  `build-source-inventory-test-smoke`.

## Progress Log - 2026-06-04 LLVM Bare-Identifier Host-Field Read Closure

- Closure #71: bare identifier reads inside a host method now ALWAYS lower
  to a host-struct GEP+load when the name matches a shared field, rather
  than first preferring an SSA-versioned local mirror. The local mirror is
  authoritative only between explicit reassignments in the same function —
  any opaque callee that touches `self->field` invalidates the mirror, and
  the caller has no way to express "reload after call". This was the gap
  behind `RunCampaign`'s `cursor=ToString(choiceCursor + 1)` print drifting
  from C (cursor=2) to LLVM (cursor=1) after `RollChoice(gameState)` had
  bumped `self->choiceCursor` via `ScriptedChoice`. The fix flips the
  identifier-lookup order: host-field GEP first, scope alloca second.
- Probe coverage stays green (probe_record, probe_intent_array,
  probe_field_index, probe_zone_chain, probe_cursor, probe_dnd_minimal,
  intent_header_interleaved) because writes still go through
  `llvm_emit_current_host_field_assignment` (closure #67) and the host
  store is therefore observed by subsequent host-field reads in the same
  block. The local-mirror dual-store from #67 becomes belt-and-suspenders
  rather than load-bearing.
- Backend evidence after #71:
  `tests/compare_backends.sh tests/cases/backend_compare/*/main.pgy` reports
  794/794 passed, 0 failed; `make llvm-dnd-campaign-test-smoke` reports
  `dnd_tavern_campaign C/LLVM parity ok`;
  `make llvm-campaign-projection-test-smoke` reports
  `campaign_graph_fsm LLVM projection parity ok`;
  `make test-all` reports 2624/870/74/58/9/119/18/74/20 across
  lexer/semantic/transpile/memory/concurrency/AIR/RIR/MIR/HIR with 0 failed.

## Progress Log - 2026-06-04 LLVM Host-Field Local Pre-Init And Memory Error Closure

- Closure #70: when emitting an SSA-versioned local alloca whose base name
  matches a host field of the current method's host class, the entry block
  now pre-initializes the alloca from the host field. Without this, branches
  that read the local without a prior SSA-DEF observed uninitialised stack
  memory. `W_Record` was the canonical failure: the `if/else` body of
  `transcript = transcript + "\n" + line` lowered the else branch to
  `load ptr, ptr %transcript.1` where `%transcript.1` had only ever been
  stored in the sibling `if` branch — so the second `Record` call inside the
  same `Broadcast` chain consumed garbage and segfaulted the program.
  Pre-init guarded to fire only when `llvm_current_host_class_name(ctx)`,
  `llvm_scope_lookup(ctx, "self")`, and `llvm_class_field_index(host_cls,
  base_name) >= 0` all hold; otherwise free functions whose locals happen to
  share a name with a host field would have produced bogus loads.
- Closure #70 was bisected to confirm it is not the source of the previously
  surfaced `dnd_campaign` memory error: compiling with #67 disabled
  reproduced the identical `rc=255` segfault at
  `WeaponCard.EffectLine`, confirming the bug pre-existed.
- Backend evidence: `tests/compare_backends.sh tests/cases/backend_compare/*/main.pgy`
  reports 793/793 passed, 0 failed (4 probes added: `probe_record`,
  `probe_intent_array`, `probe_field_index`, `probe_zone_chain`,
  `probe_cursor`, `probe_dnd_minimal`). `make test-all` reports
  `2624/0`, `870/0`, `74/0`, `58/0`, `9/0`, `119/0`, `18/0`, `74/0`, `20/0`.
  `make llvm-campaign-projection-test-smoke` is now green
  (projection parity restored). `make llvm-dnd-campaign-test-smoke` no
  longer segfaults but still diffs because the broader campaign exercises a
  separate `journey.morale`/`choiceCursor` projection path not closed by
  #70 alone; minimal probes (`probe_cursor`, `probe_dnd_minimal` of 4-strat
  Broadcast chain) all pass byte-equal.

## Progress Log - 2026-06-04 LLVM Host-Field Sync And MIR Intent Value Type Fidelity

- LLVM `llvm_emit_current_host_field_assignment` now also stores into a
  same-name local alloca (when present and field-type-matched) so that
  chained `host_field = host_field + X` reads do not return the stale
  alloca-cached value. Source-only reproducer captured at
  `tests/cases/backend_compare/probe_record/main.pgy` (was: `"a"`, expected
  `"abc"`); the same gap was the root cause of `examples/dnd_tavern_campaign`
  runtime crash and the `examples/campaign_graph_fsm` projection drift.
- MIR `IntentValue` emission no longer stores only `ast_type_name(...)` for
  the value's declared type. `mir_intent.c` now renders the full type via
  `mir_render_type_name(...)` (lifted to a public mir helper) and interns
  the rendered string in the routine's scratch arena, so a binding like
  `adjustments: Array<Int>` reaches the transpiler intent prologue as
  `Array<Int>` instead of `Array`. The C backend `ArrayLength` builtin
  resolver then accepts `adjustments` because the registered typed-var has
  the generic argument. Reproducer:
  `tests/cases/backend_compare/probe_intent_array/main.pgy`. The same fix
  also closes the residual `examples/campaign_graph_fsm` numerical drift
  because intent value parameters there reuse generic container types.
- Backend evidence: `tests/compare_backends.sh tests/cases/backend_compare/*/main.pgy`
  reports 789/789 passed, 0 failed (788 prior fixtures + the two new probes).
  `tests/llvm_dnd_campaign_smoke.sh` and
  `tests/llvm_campaign_projection_smoke.sh` are RC=0 with no diff. `make
  test-all` reports `2624/0`, `870/0`, `74/0`, `58/0`, `9/0`, `119/0`,
  `18/0`, `74/0`, `20/0` across parser/lexer/semantic/transpile/memory/
  concurrency/AIR/RIR/MIR/HIR.

## Progress Log - 2026-06-03 LLVM Builder Restoration Tightening

- LLVM generated-function emitters no longer restore caller builder state by
  guessing `LLVMGetLastBasicBlock(saved_fn)`. Function declaration emission,
  role operator bridge emission, and domain sync finish now restore the exact
  caller insertion block captured as `saved_bb`.
- `llvm_finish_domain_sync_emit(...)` now consumes the caller-owned `saved_bb`
  snapshot, so zone sync no longer performs a duplicate restore after the
  helper returns.
- Source-contract evidence: `perf_contract_smoke.sh` rejects reintroducing the
  saved-function last-block restore pattern. Local verification in this slice
  was limited to script syntax, `git diff --check`, and source scans because
  the local LLVM toolchain is not available.

## Immediate Execution Order

1. Continue the 600-1,000 LOC production owner queue without reintroducing
   behavior-owning `.inc` files.
2. DAG source-of-truth audit and migration.
3. CFG consumer migration: make body-safety facts the consumer-facing source
   for ownership/resource/return/drop-sensitive checks.
4. AIR consumer migration: make abstraction-boundary checks consume
   `air_verify(...)` evidence instead of re-reading AST/DIR strings.
5. Runtime propagation full transitive frontier scheduler.
6. MIR declaration inventory view and C/LLVM parity edge cleanup.
7. ABI ownership audit: Slot/Pin/Zone-bound handle/runtime-none/raw escape.
   `make abi-ownership-shape-test-smoke` gates the implemented Slot/Pin ABI
   shape, MIR cleanup evidence, C/LLVM pin/unpin lowering, and the docs
   contract that Zone-Bound Handle remains the missing non-pin expiration type
   piece.
8. parallel/core keyword matrix.
9. pain point sweep and beta wording freeze.

## Progress Log - 2026-05-29 Checklist Shard And Semantic Field Owner

- Split the oversized beta readiness checklist into four active shards plus a
  small index:
  `docs/100a_beta_active_status.md`,
  `docs/100b_beta_p0_semantics_systems_air.md`,
  `docs/100c_beta_dag_mir_abi_runtime.md`, and
  `docs/100d_beta_execution_log.md`.
- Updated the checklist/documentation/CFG/AIR/formal/runtime/MIR smoke scripts
  to treat the split files as one logical contract without letting the index
  grow back into a mega-file.
- Moved semantic projection-field consumers onto
  `projection_source_field_count(...)` / `projection_source_field_at(...)`.
  `ToObject`/`ToTObject`, zone projection field contracts, intent role field
  checks, target-field existence checks, and constructor arity checks no longer
  reopen `ast_class_fields(...)` directly.
- Removed duplicate projection helper declarations from
  `type_checker_internal.h`.
- Verified locally with `mingw32-make test-semantic` (`2612/0`) and
  `mingw32-make cfg-body-dataflow-test-smoke`. Earlier P0 shard verification
  also passed `beta-readiness-checklist-test-smoke`,
  `documentation-quality-test-smoke`, `air-drift-test-smoke`,
  `formal-semantics-test-smoke`, `runtime-panic-contract-test-smoke`,
  `abi-ownership-shape-test-smoke`, `raw-escape-contract-test-smoke`,
  `mir-declaration-inventory-test-smoke`, and
  `runtime-frontier-contract-test-smoke`.
- Known test-debt note: `perf-contract-test-smoke` remains too large/slow on
  local Windows Git Bash and timed out; it is P10 and should be split or
  batched separately rather than blocking this P0 semantic owner slice.
- MIR inventory surface-usage consumption is now accessor-gated:
  thread-pool and intent-observability runtime-selection code reads recorded
  inventory facts through `mir_surface_usage.c` instead of raw `MIRProgram`
  inventory booleans in codegen. The perf contract rejects raw
  `mir->has_inventory_surface_usage_facts` / `mir->inventory_uses_*` reads
  under `src/codegen`.
- MIR routine/program structure consumption is now accessor-gated:
  C/LLVM inventory views read routine inventory and `main`/top-level executable
  flags through `mir_program_inventory.c`, and AIR MIR evidence now consumes the
  same public routine inventory instead of reopening `mir->routines` directly.
  `mir-declaration-inventory-test-smoke` gates the C/LLVM side; the semantic
  core-shape smoke gates the AIR evidence side.
- HIR routine inventory consumption is now accessor-gated for AIR:
  `air_evidence_hir.c` consumes `hir_public.c` routine inventory accessors
  rather than reopening `hir->routines` / `hir->routine_count`. The semantic
  core-shape smoke rejects regressions.
- HIR routine inventory consumption is now accessor-gated for validation and
  mutable callgraph pass owners as well: `hir_validate.c` consumes the const
  routine inventory view, and `hir_callgraph.c` consumes the mutable routine
  inventory view. This leaves raw HIR routine arrays to HIR construction,
  destruction, and public-surface owners. Verified with
  `mingw32-make test-hir`, `mingw32-make semantic-core-shape-test-smoke`, and
  `mingw32-make perf-contract-test-smoke`.
- MIR public pass wrappers now consume routine inventory views:
  `mir_count_non_cfg_body_fallback_inventory(...)` uses the const routine
  inventory view, while liveness/DCE pass wrappers use the mutable routine
  inventory view. `mir-declaration-inventory-test-smoke` rejects raw
  `mir->routines` / `mir->routine_count` reads in `mir_public_surface.c`.
  Verified with `mingw32-make mir-declaration-inventory-test-smoke` and
  `mingw32-make test-mir`.
- LLVM routine inventory consumers now consume the LLVM routine-inventory
  accessor instead of indexing the wrapped array directly. The covered owners
  are function declaration emission, intent forward/emission paths, intent
  routine lookup, and MIR emission contract validation. Verified with targeted
  codegen TU builds and `mingw32-make mir-declaration-inventory-test-smoke`.
- RIR scope inventory consumption is now accessor-gated for AIR:
  `air_evidence_rir.c` consumes `rir_public_surface.c` scope inventory
  accessors rather than reopening `rir->scopes` / `rir->scope_count`. The
  semantic core-shape smoke rejects regressions.
- RIR scope item consumption is now accessor-gated for AIR:
  `air_evidence_rir*.c` consumes `rir_public_surface.c` fact/op/state-summary
  accessors rather than reopening `scope->facts`, `scope->ops`, or
  `scope->state_summaries`. The semantic core-shape smoke rejects regressions.
- RIR public dump output now consumes the same RIR scope inventory and
  fact/op/state-summary accessors as validators and evidence consumers.
  `rir_dump(...)` and `rir_dump_json(...)` no longer walk raw scope/fact/op
  arrays directly. Verified with `mingw32-make build/compiler/rir_public_surface.o`,
  `mingw32-make semantic-core-shape-test-smoke`, and `mingw32-make test-rir`.
- RIR flow enrichment now consumes the mutable RIR scope inventory for scope
  matching instead of reopening `rir->scopes` / `rir->scope_count`. This keeps
  mutation local to the flow owner while putting program-level scope iteration
  behind the public RIR surface. Verified with
  `mingw32-make build/compiler/rir_public_surface.o build/compiler/rir_flow.o`,
  `mingw32-make semantic-core-shape-test-smoke`, and `mingw32-make test-rir`.
- RIR flow enrichment read paths now consume `rir_scope_fact_at(...)`,
  `rir_scope_op_at(...)`, and `rir_scope_state_summary_at(...)`. The remaining
  raw state-summary writes in `rir_flow.c` are the owner-local summary reset
  before rebuilding flow facts. Verified with `mingw32-make build/compiler/rir_flow.o`,
  `mingw32-make test-rir`, and a longer-timeout rerun of
  `mingw32-make semantic-core-shape-test-smoke`.
- RIR validation and public dump flow-block fact reads now consume
  `rir_scope_flow_block_*` and `rir_flow_block_fact_*` accessors instead of
  reopening `scope->flow_blocks` / `block->facts`. Verified with
  `mingw32-make build/compiler/rir_public_surface.o build/compiler/rir_validation.o`,
  `mingw32-make semantic-core-shape-test-smoke`, and `mingw32-make test-rir`.
- MIR cleanup invalidation checks now consume the same RIR flow-block accessors
  instead of reopening `rir_scope->flow_blocks` / `flow->facts`. Verified with
  `mingw32-make build/compiler/mir_cleanup.o`,
  `mingw32-make semantic-core-shape-test-smoke`, and `mingw32-make test-mir`.
- RIR flow semantic-bit reads are now public-surface owned too:
  `rir_scope_conservative_semantics(...)`,
  `rir_flow_block_entry_semantics(...)`, and
  `rir_flow_block_exit_semantics(...)` are the consumer-facing API for dump and
  MIR cleanup invalidation checks. Verified with
  `mingw32-make build/compiler/rir_public_surface.o build/compiler/mir_cleanup.o`
  and `mingw32-make semantic-core-shape-test-smoke`.
- RIR flow-block identity reads now use
  `rir_flow_block_id(...)`, `rir_flow_block_is_reachable(...)`, and
  `rir_flow_block_is_join(...)` in validation/public dump consumers. Verified
  with `mingw32-make build/compiler/rir_public_surface.o build/compiler/rir_validation.o`
  and `mingw32-make semantic-core-shape-test-smoke`.
- RIR validation/public dump scope metadata reads now use
  `rir_scope_kind(...)`, `rir_scope_name(...)`, `rir_scope_owner_name(...)`,
  `rir_scope_display_name(...)`, and `rir_scope_has_state_errors(...)`.
  Verified with
  `mingw32-make build/compiler/rir_public_surface.o build/compiler/rir_validation.o build/compiler/rir_validation_dir.o`,
  `mingw32-make semantic-core-shape-test-smoke`, and `mingw32-make test-rir`.
- RIR validation lookup ownership tightened: const state-summary lookup and
  projection-fact lookup now live in `rir_public_surface.c`. The validators
  consume `rir_scope_find_state_summary(...)` /
  `rir_scope_find_projection_fact(...)`, and the shape smoke rejects mutable
  `RIRScope` casts or local projection lookup reintroduction. Verified with
  `mingw32-make semantic-core-shape-test-smoke` and `mingw32-make test-rir`.
- RIR/DIR validation fact lookup ownership tightened further:
  `rir_scope_find_fact_by_name_kind(...)` and
  `rir_scope_has_capability_fact(...)` now live in `rir_public_surface.c`, so
  authority/resource/capability checks no longer own local fact scans. Verified
  with `mingw32-make build/compiler/rir_public_surface.o build/compiler/rir_validation_dir.o`,
  `mingw32-make semantic-core-shape-test-smoke`, and `mingw32-make test-rir`.
- Backend compare collection parity was widened with
  `list_push_get_loop`, `map_long_values`, and `set_intersection_manual`.
  Each fixture passed a targeted C/LLVM compare with `bin-llvm/pgy.exe`.
- Backend compare algorithmic parity was widened with 16 small fixtures:
  `string_starts_with_prefix`, `loop_collect_max`, `sort_three_ints`,
  `count_letters_in_word`, `binary_search_int`, `gcd_recursive`,
  `reverse_array_in_place`, `palindrome_check`, `bubble_sort_small`,
  `fizzbuzz_loop`, `multi_array_find`, `bool_state_toggle`, `primes_below_n`,
  `string_repeat_pattern`, `linear_search_first_match`, and
  `map_word_grouping`. Targeted `tests/compare_backends.sh` passed 15/15, plus
  a targeted `map_word_grouping` run, with `bin-llvm/pgy.exe`.
- Rechecked the systems-baseline evidence commands after the P0 shard split:
  `mingw32-make codegen-determinism-test-smoke`,
  `mingw32-make runtime-none-contract-test-smoke`, and
  `mingw32-make raw-escape-contract-test-smoke` all pass locally.
- Tightened C MIR loop-local typing: `for` range variables still resolve to
  `Int`, but `for x in List<T>|Array<T>|Slice<T>` now derives `x: T` through
  the shared MIR local-type owner instead of registering every loop variable as
  `Int`. The transpile regression now checks that `for event in List<Event>`
  emits an `Event` local from list storage.
- Verified this loop-local slice with `mingw32-make test-transpile`
  (`860/0`) and `mingw32-make cfg-body-dataflow-test-smoke`.
- Tightened hosted method body-summary consumption: current-host and instance
  method calls now resolve the checked `Host_Method` function symbol through
  `expr_host_method_function_type(...)` and record the typed callee body
  summary instead of relying only on AST-declared method clauses. The semantic
  regression now requires an instance method call to propagate a body-derived
  `BODY_SUMMARY_SPAWNS_TASK` fact.
- Intent authority-sensitive call detection now uses the same typed hosted
  method effect source, so body-derived secure method effects are not lost when
  an intent `expect:` clause calls `receiver.Method()`. The parallel semantic
  regression also covers a body-derived secure method call.
- Narrowed the ref-parameter escape compatibility seam: when a checked
  function type already has a body summary fact without
  `BODY_SUMMARY_MAY_ESCAPE_REF`, `semantic_callable_param_escape_summary(...)`
  returns no escape without invoking the legacy AST param analyzer. The legacy
  analyzer remains only for unresolved/undecisive compatibility paths.
  The typed-summary fast path now first verifies that the callable declaration
  owner seam returns the same AST declaration as the checked function symbol, so
  name/scope drift cannot incorrectly bypass the legacy analyzer.
  `semantic-core-shape-test-smoke` now gates that typed body-summary facts are
  checked before legacy AST analysis.
- Verified with direct MinGW TU compiles and `mingw32-make test-semantic`
  (`2615/0`). A stale Git Bash/make child process briefly blocked the first
  clean-shell run, but the rerun completed after the process cleared.
- Added an explicit `has_body_summary_facts` bit to function types, so a checked
  empty summary is still decisive evidence and no longer falls back through the
  legacy AST param analyzer. `semantic-core-shape-test-smoke` now gates this
  distinction.
- Closed a CFG smoke violation in C MIR SSA-local emission:
  `transpiler_mir_func_ssa_locals_emit.c` no longer falls back to raw
  `inst->ast`; receive payload recovery now goes through
  `mir_instruction_source_payload(...)`. Verified with
  `mingw32-make cfg-body-dataflow-test-smoke`.
- Promoted twenty-three parity fixtures into the default backend-compare inventory:
  `for_in_array_int`, `nested_array_subarray`, `float_to_string_precision`, and
  `map_key_lookup_branch` from the array/string/map slice, plus
  `phi_branch_value` from the LLVM PHI smoke surface, `queue_string_ops` /
  `list_int_loop` from the collection-runtime parity slice, and
  `compose_two_functions` / `negative_index_check` from the callable and array
  guard surfaces, plus `multi_return_paths` / `bool_expr_chain` from the
  control-flow/expression parity surface, plus `fibonacci_iterative` /
  `map_count_unique` from the loop/map parity surface, plus
  `for_range_explicit`, `if_short_circuit_pure`, `nested_match_int`,
  `option_param_pass`, `result_via_unwrap`, `string_compare_branch`,
  `string_split_simple`, `subject_class_pair`, `substring_extract`, and
  `sum_filter_loop` from the common syntax/control-flow parity surface. The
  targeted MinGW/Git Bash runs passed for these fixtures (`5/5`, `2/2`, `2/2`,
  `2/2`, `2/2`, then `10/10`). The full default registry now contains 239 cases,
  but this slice did not run the whole backend-compare suite. Field-channel
  storage was checked and kept out of backend-compare
  because `Channel<T>` aggregate fields are an intentional C/LLVM stable-surface
  reject until movable channel-handle ABI exists.
- Repaired `mir_declaration_inventory_smoke.sh` for the build-source inventory
  portability gate: the declaration-field whitelist no longer uses macOS Bash
  3.2-hostile `case` pattern line continuations. Verified with
  `mingw32-make build-source-inventory-test-smoke` and
  `mingw32-make mir-declaration-inventory-test-smoke`.
- Repaired the perf/source-of-truth contract smoke after the general call
  return-type owner moved: the gate now checks
  `type_checker_helpers_late.c` for `type_function_return_type(sym->type)`
  instead of expecting `type_checker_expr_call.c` to own the function signature
  return path. Verified with `mingw32-make perf-contract-test-smoke`.
- Tightened the AIR source-of-truth regression gate in
  `semantic_core_shape_smoke.sh`: raw AIR graph fields
  (`intent_count`/`intents`, `boundary_count`/`boundaries`,
  `drift_count`/`drifts`, input flags, strict-evidence flag, and evidence-node
  storage) are only allowed in their AIR owner TUs. Consumers must use the
  public AIR graph/EvidenceNode accessors. Verified with
  `mingw32-make semantic-core-shape-test-smoke`.
- Tightened the no-Python CFG/body smoke fallback for MIR source-shape
  ownership: raw `inst->source_ast_type`, `inst->source_line`,
  `inst->source_column`, and matching block source fields are allowed only in
  MIR construction/public-surface/source-shape owners. Consumers must use the
  MIR source-shape accessors. Verified with
  `mingw32-make cfg-body-dataflow-test-smoke`.
- Tightened HIR/RIR inventory consumption in the lowering path: MIR lowering
  and RIR flow enrichment now consume HIR routines through
  `hir_routine_inventory_from_program(...)`, and MIR cleanup/lowering consumes
  RIR scopes/facts/ops through `rir_public_surface.c` accessors instead of
  reopening raw `hir->routines`, `rir->scopes`, `scope->facts`, or
  `scope->ops`. Verified with targeted TU builds,
  `mingw32-make semantic-core-shape-test-smoke`, `mingw32-make test-mir`, and
  `mingw32-make cfg-body-dataflow-test-smoke`.
- Tightened MIR declaration method routine linking/validation: the declaration
  header linker and validator now consume routines through
  `mir_routine_inventory_from_program(...)` / `mir_routine_inventory_get(...)`
  rather than raw MIR routine arrays. The MIR program validator now uses the
  same inventory view for routine inventory-shape checks. Verified with targeted
  TU builds, `mingw32-make mir-declaration-inventory-test-smoke`, and
  `mingw32-make test-mir`.
- Tightened RIR validation source-of-truth consumption: `rir_validate(...)` and
  `rir_validate_against_dir(...)` now consume RIR scope inventory plus
  fact/op/state-summary accessors rather than raw `rir->scopes` or
  `scope->facts` / `scope->ops` arrays. Verified with targeted TU builds,
  `mingw32-make semantic-core-shape-test-smoke`, and `mingw32-make test-rir`.
- Fixed a local Windows build-system trap where `pgy_mkdir_p` used a nested
  login-shell bash invocation while `SHELL` was already Git Bash. The owner
  now invokes bash with `-c` for `mkdir`/`touch`, and
  `build-source-inventory-test-smoke` rejects regressing to `-lc`. Verified
  with `mingw32-make bin/pgy.exe` and
  `mingw32-make build-source-inventory-test-smoke`.

## Progress Log — 2026-04-28 AST Owner Split

- `src/parser/ast.c` no longer owns node construction or clone helpers.
  Construction moved to `src/parser/ast_constructors.c` and
  `src/parser/ast_domain_constructors.c`; clone helpers moved to
  `src/parser/ast_clone.c`.
- `src/parser/ast_print.c` no longer owns domain/intent/event printers.
  Those moved to `src/parser/ast_print_domain.c`, with misc print policy in
  `src/parser/ast_print_misc.c`.
- `src/parser/ast_print.c` also no longer owns inline expression rendering,
  compact print rendering, operator spelling, escaped string rendering, or
  generic/where-clause inline rendering. Those moved to
  `src/parser/ast_print_inline.c` and `src/parser/ast_print_generics.c`.
- `src/parser/ast_print_domain.c` no longer owns intent or event printing.
  Intent printing plus contract provenance moved to
  `src/parser/ast_print_intent.c`; event printing moved to
  `src/parser/ast_print_event.c`.
- `src/parser/parser.c` no longer owns declaration hint inventory.
  `src/parser/parser_decl_hints.c` now owns top-level declaration hint
  extraction, registration, capacity growth, and lookup.
- `src/parser/parser_domain.c` no longer owns relation/effect declaration
  parsing or projection-sync helper parsing. Relation/effect declarations
  moved to `src/parser/parser_domain_relation_effect.c`; projection group
  parsing and projection field maps moved to
  `src/parser/parser_domain_projection.c`.
- `src/parser/ast.h` no longer owns shared AST vocabulary directly.
  `src/parser/ast_types.h` now owns AST enums, forward declarations, generic
  parameter structs, function parameter structs, and class field structs.
- `src/parser/ast.h` also no longer owns the public AST
  constructor/manipulation prototype surface. Those declarations moved to
  `src/parser/ast_api.h`, which `ast.h` includes for source compatibility.
- Verified with `make test-parser pgy`, `make test-semantic` (2357/0),
  owner/sentinel/doc checklist gates, and
  `make runtime-frontier-contract-test-smoke`.
- Result: no production `.c` or `.h` owner remains above the 1,000 LOC hard
  risk line. The AST print family is now below the 600 LOC split-review
  threshold: `ast_print.c` 553 LOC, `ast_print_domain.c` 539 LOC,
  `ast_print_inline.c` 382 LOC, `ast_print_intent.c` 253 LOC, and
  `ast_print_event.c` 76 LOC. `parser.c` is now 867 LOC after the declaration
  hint split. The parser domain family is below the 600 LOC split-review
  threshold: `parser_domain.c` 493 LOC, `parser_domain_relation_effect.c` 283
  LOC, and `parser_domain_projection.c` 184 LOC. `ast.h` is now 848 LOC after
  the public API header split, with `ast_api.h` at 137 LOC.

## Progress Log — 2026-04-28 LLVM Backend Type Map Split

- `src/codegen/llvm_backend.c` is reduced to context lifecycle and backend
  entry ownership. AST/Pergyra type-name rendering, generic container type
  extraction, `pergyra_type_to_llvm`, `ast_type_to_llvm`, and early
  forward-declare eligibility now live in
  `src/codegen/llvm_backend_type_map.c`.
- Verified with `make pgy` and `make llvm-test-smoke`.
- Remaining LLVM blocker focus: `llvm_domain_zone_sync.c` / domain frontier
  parity and declaration inventory/bootstrap seams, not the generic backend
  context owner.

## Progress Log — 2026-04-28 LLVM Zone Frontier State Split

- Zone sync bounded-frontier bookkeeping now has a named LLVM owner:
  `src/codegen/llvm_domain_zone_frontier_state.c` owns previous-state
  allocation, snapshotting, reset, and change-detection continuation updates.
- `src/codegen/llvm_domain_zone_sync.c` is reduced to zone propagation
  orchestration for projection sync, action-caused effects, apply/maintain,
  detach, link, relation maintain, and unlink.
- Verified with `make runtime-frontier-contract-test-smoke` and
  `make llvm-test-smoke`.
- Remaining runtime propagation blocker is now broader-family coverage, not
  backend-local world frontier policy: stable world sync consumes
  `pgy_frontier_world_transitive_pass_limit(...)` in C and LLVM, while future
  world-zone propagation paths must be forced through the same policy family.

## Progress Log — 2026-04-29 Runtime Frontier Policy And C Owner Split

- Stable world outer frontier scheduling now consumes
  `pgy_frontier_world_transitive_pass_limit(...)` from
  `src/codegen/domain_frontier_policy.h` in both the C world emitter and LLVM
  world sync emitter.
- `make runtime-frontier-contract-test-smoke` now gates that named transitive
  policy source of truth in addition to the existing zone, world-derived, and
  projection pass-limit helpers. It also requires
  `make runtime-frontier-policy-test-smoke` to stay wired so saturating
  pass-limit arithmetic is checked by a compiled executable, not only by
  string terms.
- C world/select/event lowering is split into focused owners:
  `transpiler_world_select_event_emit.c` (457 LOC), `transpiler_select_emit.h`
  (155 LOC), and `transpiler_event_emit.h` (103 LOC). This removes the
  600+ LOC mixed owner without reintroducing `.inc` files.
- Verified with `make pgy`, `make runtime-frontier-contract-test-smoke`,
  `make runtime-frontier-policy-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.

## Progress Log — 2026-04-29 C Let Slot Owner Split

- Slot-related let lowering is now a named C backend owner:
  `transpiler_let_slot_emit.c` owns ClaimSlot/ClaimSecureSlot/ClaimDeviceSlot,
  ReadView/WriteView/MoveToken declarations, and Slot/SecureSlot sugar.
  `transpiler_let_slot_emit.h` is declaration-only.
- `transpiler_let_emit.c` is back to let-declaration orchestration plus
  non-slot specialization paths and stays under the 600 LOC split-review
  threshold.
- Verified with `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).

## Progress Log — 2026-04-29 C Domain Provenance Owner Split

- Hidden domain provenance field/stamp emission and projection-chain bounded
  recompute moved to `transpiler_domain_provenance_emit.h`.
- `transpiler_domain_role_ability_emit.h` no longer mixes role/ability vtable
  lowering with runtime propagation frontier helpers.
- Verified with `make pgy`, `make test-transpile`, and
  `make runtime-frontier-contract-test-smoke`; `make
  llvm-test-backend-compare` remains green (`196/0` ABI same-process,
  `65/65` backend compare).

## Progress Log — 2026-04-29 C Class Declaration Owner Split

- Non-generic class declaration lowering moved to the compiled owner
  `transpiler_class_decl_emit.c`.
- `transpiler_func_class_flow_emit.h` is now below the 600 LOC split-review
  threshold and no longer owns class field/container/method emission directly.
- `transpiler_func_flow_policy.c` owns function fallback policy helpers, keeping
  the function-flow shim focused on emission orchestration.
- Verified with `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).

## Progress Log — 2026-04-29 C MIR Block Owner Split

- MIR emission predicate wrappers moved to `transpiler_mir_emit_predicates.h`.
- `transpiler_mir_block_emit.c` owns block statement emission; the header is
  declaration-only and no longer participates in implementation LOC debt.
- Verified with `make test-mir`, `make cfg-body-dataflow-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.

## Progress Log - 2026-04-29 C Declaration Lookup Owner Split

- Host and method declaration lookup moved to
  `transpiler_decl_host_lookup.c`.
- `transpiler_decl_lookup.c` is now 419 LOC and keeps named declaration,
  alias, inventory, and method-list lookup ownership focused.
- `transpiler_decl_host_lookup.c` is 216 LOC and owns current-host,
  owner-host, nominal-host, and nominal-method lookup cache paths.
- Verified with `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).

## Progress Log - 2026-04-29 C Type Mapping Owner Split

- AST type-name rendering moved to `transpiler_type_render_helpers.h`.
- `transpiler_type_mapping_helpers.h` is now 563 LOC and keeps primitive,
  collection, slot, result, and suffix mapping ownership focused.
- `transpiler_type_render_helpers.h` is 102 LOC and owns recursive AST
  type-name rendering plus arena-stable local render results.
- Verified with `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).

## Progress Log - 2026-04-29 CFG Contract Validator Owner Split

- CFG-owned AST control classification moved to
  `mir_cfg_contract_control.h`.
- `mir_cfg_contract_validate.h` is now 551 LOC and keeps cleanup, successor,
  and predecessor contract validation ownership focused.
- Pin cleanup edge validation moved to `mir_cfg_contract_pin.h`, preserving the
  existing Slot/Pin ABI shape smoke contract while keeping the main CFG
  contract owner below the split-review threshold.
- Verified with `make test-mir`, `make cfg-body-dataflow-test-smoke`,
  `make abi-ownership-shape-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.

## Progress Log - 2026-04-29 MIR SSA Local Type Owner Split

- AST body local type lookup and expression fallback inference moved to
  `transpiler_mir_local_type_lookup.c`.
- `transpiler_mir_ssa_names.h` is now 357 LOC and keeps SSA name resolution,
  SSA map setup, claim-shape predicates, and implicit-field rendering focused.
- `transpiler_mir_local_type_lookup.c` owns MIR local type
  recovery for let declarations, destructuring, with aliases, branch bodies,
  member calls, and nominal constructor calls.
- Verified with `make pgy`, `make test-mir`,
  `make cfg-body-dataflow-test-smoke`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).

## Progress Log - 2026-04-29 DAG Signature Stage Seam Tightening

- `semantic_stage_resolve_type_quiet(...)` no longer calls
  `semantic_type_resolution_lookup_or_materialize(...)` directly from the
  signature stage. The current implementation now consumes DAG metadata facts
  and owner-local diagnostics directly; the intermediate materializing
  type-ref API has since been removed.
- `type-resolution-resolver-inventory-test-smoke` removed
  `type_checker_resolution_stage_signature.c` from the direct materializer
  allowlist. Direct diagnostic materializer calls are limited to central
  metadata/diagnostic compatibility owners.
- Stable constructed-type diagnostic argument resolution now uses the
  metadata-first type-ref helper too. The only remaining direct
  `semantic_type_resolution_lookup_or_materialize(ctx, ...)` call is the
  central metadata type-ref helper's fallback branch.
- The direct materializer smoke allowlist is now narrowed to that central
  metadata owner only; `type_checker_resolve.c` remains counter-only.
- Local gates: `make type-resolution-resolver-inventory-test-smoke
  type-resolution-dag-test-smoke test-semantic` is green with
  `materializer_fallbacks=0` and semantic suite `2359/0`.

## Progress Log - 2026-04-29 Runtime Channel/Qubit Export Owner Split

- Runtime channel/qubit export ownership is now split without changing the
  public runtime include seam.
- `pgy_runtime_lib_channel_quantum_exports.h` is a 7 LOC facade over
  `pgy_runtime_lib_channel_int_exports.h`,
  `pgy_runtime_lib_channel_string_exports.h`, and
  `pgy_runtime_lib_qubit_state_exports.h`.
- The split owners are 327, 319, and 69 LOC respectively, keeping the
  channel/qubit export surface below the 600 LOC split-review threshold without
  reintroducing `.inc` files.
- `compiler_runtime_cache_is_fresh(...)` tracks the leaf owners, so cached LLVM
  runtime objects cannot stay stale after a channel or qubit export edit.
- Verified with `make pgy`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`.

## Progress Log - 2026-04-29 Runtime Raw Collection Export Owner Split

- Runtime raw collection export ownership is now split without changing the
  public runtime include seam.
- `pgy_runtime_lib_raw_collection_exports.h` is an 8 LOC facade over
  `pgy_runtime_lib_raw_collection_common_exports.h`,
  `pgy_runtime_lib_raw_queue_exports.h`,
  `pgy_runtime_lib_raw_map_exports.h`, and
  `pgy_runtime_lib_raw_set_exports.h`.
- The split owners are 13, 117, 431, and 153 LOC respectively, keeping raw
  Queue/HashMap/Set export ownership below the 600 LOC split-review threshold
  without reintroducing `.inc` files.
- `compiler_runtime_cache_is_fresh(...)` tracks the leaf owners, so cached LLVM
  runtime objects cannot stay stale after a raw collection export edit.
- Verified with `make pgy`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`.

## Progress Log - 2026-04-29 DAG Retired Resolver Owner Split

- The obsolete `type_checker_resolve.c` owner is removed from the beta path.
- Retired compatibility counters now live in
  `type_checker_resolution_retired.c`; general type helper functions
  `require_assignable(...)` and `wrap_constructed(...)` now live in
  `type_checker_type_helpers.c`.
- `type-resolution-resolver-inventory-test-smoke` rejects reintroducing
  `type_checker_resolve.c` or `type_checker_resolve.h` and requires the retired
  counter owner to keep its audit marker.
- `semantic-core-shape-test-smoke` requires the new owners and verifies
  assignability helpers do not move back into the retired counter owner.
- Verified with `make type-resolution-resolver-inventory-test-smoke`,
  `make type-resolution-dag-test-smoke`, `make test-semantic`,
  `make semantic-core-shape-test-smoke`, and `make semantic-tu-size-test-smoke`.

## Progress Log - 2026-05-02 AIR/DAG Source-Of-Truth Tightening

- AIR retains the legacy `authority_from_zone` schema field, but active beta
  semantics no longer derive approval from a local `who`. Action-inherited
  authority becomes `authority_from_action`. Action-inherited
  zone source becomes `source_from_action`, while action-inherited ability/effect
  contracts become `requires_from_action` and `causes_from_action`. AIR JSON plus
  drift diagnostics expose
  `authority_provenance=action-inherited|explicit|none` on active beta paths;
  compatibility-only zone authority is labeled `legacy-zone-field`.
- Action-derived intent `causes` now also reaches RIR propagation evidence:
  intent RIR lowering emits `RIR_RESOURCE_EFFECT_INSTANCE` and
  `RIR_OP_ATTACH_EFFECT`, prefers the unique zone effect-slot anchor when one
  exists, and AIR observes it as `AIR_EVIDENCE_RIR_EFFECT_PROPAGATION`.
- Action-derived intent `authorized by` is pinned to RIR authority evidence in
  the same parsed on-receiver fixture. This keeps `authority_from_action`
  honest by requiring `AIR_EVIDENCE_RIR_AUTHORITY` and a matching
  `rir_authority_evidence_name`.
- MIR cleanup evidence accounting is stricter and more CFG-backed:
  `AIR_EVIDENCE_MIR_CLEANUP` consumes MIR cleanup successors first, while pin
  cleanup remains boundary-specific `AIR_EVIDENCE_MIR_PIN_CLEANUP`.
- DAG materializer owner inventory shrank from `25` to `18`; intent participant,
  transfer, inherited-action parameter, and zone-authority subject-slot type
  annotations now use the centralized annotation read API instead of the
  materializer seam. Abstract ability method signature validation also now
  consumes annotation facts directly. Projection field-path type reads also
  consume annotation facts, keeping projection diagnostics read-only with
  respect to DAG metadata creation. Destructuring ownership type reads now do
  the same, so that CFG/body-safety-adjacent path no longer materializes DAG
  metadata as a side effect.
- The ownership-let materializing semantic owner seam is now closed without
  pretending that annotation-only lookup was sufficient. The negative probe
  showed that annotation-only lookup caused broad semantic drift and lost
  unsupported `HashMap` key diagnostics. The accepted path consumes DAG
  metadata type-ref facts and then calls the shared stable-shell arity,
  constructed-type, and unknown-bare-name diagnostic helpers. Effective
  generic-argument derivation, generic contract validation, generic
  where/default validation, host/domain slot reads, intent-local type reads,
  function signatures, expression annotations, action contract reads,
  ownership-let annotations/type arguments, and compressed intent role/ability
  field checks now consume centralized metadata/effective-argument evidence
  instead of owning local materializer seams.
- Class/ability signature staging now opens a generic-parameter scope before
  resolving staged fields and methods, aligning DAG staging with the full
  semantic checker even where the materializer allowlist is still required.
- Local gates: `make test-air`, `make test-semantic`,
  `make type-resolution-resolver-inventory-test-smoke`,
  `make type-resolution-dag-test-smoke`, `air-drift-test-smoke`,
  `intent_compression_contract_smoke.sh`.

## Progress Log - 2026-06-06 MIR Host Declaration Header Lookup

- Function declaration headers are now part of the MIR declaration-header
  inventory, so host metadata lookups must not use name-only header lookup.
  A function and a host declaration can legally share a name at different AST
  kinds; host metadata must select by host declaration kind.
- LLVM `llvm_find_host_decl_header_in_context` now iterates
  `pgy_host_decl_compat_types` and resolves through typed declaration-header
  lookup. The C backend has the matching
  `transpiler_active_host_decl_header` owner and hosted field/method/slot
  metadata consumers now use it.
- The C/LLVM codegen-level name-only declaration-header wrappers were removed;
  codegen callers must use typed declaration-header lookup or the host-specific
  owner. The MIR core still keeps `mir_find_decl_header` for direct MIR tests.
- The smoke gate now requires typed declaration-header lookup for C and LLVM
  host metadata and keeps the backend from falling back to partial hard-coded
  host-kind chains.
- Verified locally with `gcc -fsyntax-only` on the changed C slices,
  `make pgy mir-declaration-inventory-test-smoke test-mir`, and
  `make llvm-test-smoke`.
- Full `llvm-test-backend-compare` is still too large for the current
  interactive 15-minute window: the last run reached `570/795` PASS with no
  mismatch before timeout. Treat it as a long-running parity gate, not as a
  failed parity case; use the existing `PGY_BACKEND_COMPARE_SHARD_TOTAL` /
  `PGY_BACKEND_COMPARE_SHARD_INDEX` path for interactive or CI shard runs.

## Progress Log - 2026-06-06 Backend Compare Range Gate

- C/LLVM parity work no longer has to choose between one explicit fixture and
  the full 795-case backend compare. `tests/compare_backends.sh` now accepts
  deterministic `PGY_BACKEND_COMPARE_START_INDEX` and
  `PGY_BACKEND_COMPARE_MAX_CASES` controls after shard selection.
- The CI shard matrix remains the full source of truth for broad coverage.
  The range gate is a developer feedback loop for fixing a narrow C/LLVM seam
  without re-running hundreds of unrelated fixtures on every edit.
- Makefile forwards the range controls through `llvm-test-backend-compare` and
  `air-strict-backend-compare-test-smoke`, and the source-inventory smoke gate
  now keeps that wiring from regressing.
- Verified locally with `make build-source-inventory-test-smoke`,
  `PGY_BACKEND_COMPARE_PRECHECK=0 PGY_BACKEND_COMPARE_MAX_CASES=3 make
  llvm-test-backend-compare`, and direct non-zero range execution
  (`START_INDEX=1`, `MAX_CASES=1`).
- Additional direct C/LLVM evidence with `bin/pgy`: first 20 default fixtures
  pass with `PGY_BACKEND_COMPARE_MAX_CASES=20`, and the known same-name
  binding/scope-collision set (`option_same_binding_guard`,
  `option_multi_same_binding_loop`, `option_nested_same_binding_shadow`,
  `match_nested_same_binding_shadow`, `lexical_shadow_class_method`,
  `llvm_dynamic_scope_capture`, `list_shadow_scope_metadata`) passes 7/7.

## Progress Log - 2026-06-06 AIR Strict Evidence Policy Tightening

- AIR boundary evidence policy now owns the pin-cleanup requirement shape:
  `AIR_BOUNDARY_EXECUTION` carries `mir_pin_cleanup_source_name = "pin"` in
  `kBoundaryEvidencePolicies` instead of re-stating the source-name rule in
  verification code.
- AIR global verification now consumes data-driven strict evidence requirement
  tables for MIR counter proofs, DAG counter proofs, and runtime singleton
  evidence. Summary counters remain observability only; `EvidenceNode` inventory
  is the proof source of truth.
- `air-drift-test-smoke` now gates the policy tables so future AIR evidence
  kinds are added by extending the requirement owners rather than by copying
  ad-hoc strict-evidence checks.
- Verified locally with `gcc -fsyntax-only` for the touched AIR owners,
  `make air-drift-test-smoke`, `make air-json-schema-test-smoke`,
  `make backend-fail-closed-test-smoke`, and
  `make build-source-inventory-test-smoke`.

## Progress Log - 2026-06-06 C Projection Nominal Lookup Tightening

- C `transpiler_find_projection_nominal_decl_local(...)` now consumes
  `transpiler_find_named_decl_local(ctx, AST_CLASS_DECL, name)`, matching the
  LLVM path's typed declaration-header-first lookup. This removes one
  inventory-first compatibility seam from projection nominal recovery.
- `mir-declaration-inventory-test-smoke` now rejects returning the projection
  nominal owner to `transpiler_find_decl_in_inventory_local(...)`.
- Verified locally with `make mir-declaration-inventory-test-smoke`, targeted
  projection/zone compare fixtures (`probe_record`, `probe_field_index`,
  `probe_zone_chain`, `zone_with_subject_slot`) and actual projection fixtures
  (`subject_projection`, `relation_effect_projection_sync`,
  `world_zone_projection_visibility`,
  `world_embedded_branch_projection_visibility`).

## Progress Log - 2026-06-06 C Typed Declaration Recovery Tightening

- C backend type-specific declaration recovery no longer calls
  `transpiler_find_decl_in_inventory_local(ctx, AST_*_DECL, ...)` directly
  outside the declaration-lookup owner. Class/enum host method body lookup,
  nominal host lookup, relation/effect/party/roster/world/zone declaration
  emitters, overlay relation/effect bind, world-zone projection recovery,
  intent zone lookup, effect sync lookup, MIR SSA zone recovery, and function
  forward world checks now go through `transpiler_find_named_decl_local(...)`,
  which prefers typed MIR declaration headers before AST inventory fallback.
- `mir-declaration-inventory-test-smoke` now rejects reintroduced literal
  `AST_*_DECL` direct inventory recovery under `src/codegen`.
- Verified locally with `gcc -fsyntax-only` on the touched C backend owners,
  `make mir-declaration-inventory-test-smoke`, and targeted C/LLVM fixture
  batches: class/enum/world/projection baseline (`8/8`) plus declaration/
  overlay/world-zone coverage (`role_operator`, `zone_host_method_abi_combo`,
  `relation_effect_projection_sync`, `world_with_zones`,
  `world_zone_cross_queries`, `world_zone_projection_visibility`,
  `zone_action_effect_runtime`, `zone_layer_projection_runtime`) with `10/10`
  passing.

## Progress Log - 2026-05-02 Intent Single-Subject Who Inference

- Closed the first safe Intent-Compress `who` rule: a step with omitted `who`
  derives it from the enclosing intent only when there is exactly one
  subject participant and no action/default has already supplied a `who`.
- Multi-subject intents remain explicit. This keeps the rule fail-closed and
  prevents intent compression from becoming an authority/effect owner.
- The provenance now flows through AST print, semantic contract summary, DIR,
  AIR, and `pgy.air.graph.v1` JSON as `who_from_single_participant`.
- Added positive and negative semantic regressions plus source-gated smoke
  checks for the derivation owner and AIR JSON schema field.
- Split the AIR evidence test case owner so `src/tests/*.cases.h` stays below
  the size gate without weakening AIR coverage.
- Verified locally with `make test-semantic` (`2430/0`), `make test-air`
  (`65/0`), `make intent-compression-contract-test-smoke`,
  `make air-json-schema-test-smoke`, `make test-inc-size-test-smoke`, and
  `make source-utf8-test-smoke`.

## Progress Log - 2026-04-30 C/LLVM Defer Cleanup Parity

- C `defer` lowering now uses lexical inline cleanup instead of a file-scope GCC
  cleanup helper. This keeps local state such as method `self` visible to the
  deferred body and aligns the C backend with LLVM's defer stack model.
- MIR-emitted C functions now register `AST_DEFER_STMT` through the same defer
  stack and emit active defers on MIR return/fallthrough returns, so subject
  method recursion with deferred state mutation is backend-parity gated.
- Nested branch defer is now MIR-preserved rather than treated as CFG-owned
  control, so `if { defer { ... } }` survives DCE and is smoke/parity gated.
- Dynamic `defer` inside runtime-dependent `if`/match/loop control is not beta-stable
  and is now rejected with `PGY_SEM_DEFER_DYNAMIC_CONTROL`. This avoids a false
  parity state where C and LLVM both run the same wrong cleanup.
- The old sentinel path is now a regression smell: C tests reject
  `__attribute__((cleanup(_pgy_defer_...)))` for source-level `defer`.
- Current evidence: `make test-transpile` (`682/0`), `make llvm-test-smoke`,
  `make llvm-test-backend-compare` (`69/69`), and the CFG/AIR/DAG smoke gates
  pass. A full monolithic `make ci-linux` was not completed locally because the
  command exceeded the 15 minute execution window; the CI target groups were
  run in slices instead.

## Progress Log - 2026-04-24 Parser/Lexer Diagnostic Routing

- `parser_error` and lexer error-token paths now route stage code, reason, and
  fix metadata through the first diagnostic-routing gate.
- Stable codes: `PGY_PARSE_SYNTAX`, `PGY_LEX_INVALID_TOKEN`.
- Gate: `make parser-lexer-diagnostic-test-smoke`.
- CI wiring: `ci-linux` runs the parser/lexer diagnostic gate.
- Remaining beta debt: parser-specific code split and multi-error accumulation.
