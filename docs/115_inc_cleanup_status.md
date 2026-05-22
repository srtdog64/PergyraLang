# Include Cleanup Status

Last updated: 2026-05-19

This note records the current state of the beta include-cleanup track. It is a
progress ledger, not a new language surface.

## Closed In This Slice

- `.inc` cleanup is closed for the full `src` tree: there are now **0 `.inc`
  files / 0 LOC** under `src`, including test fixtures.
- 2026-04-29 audit rerun: `find src -name '*.inc'` returns zero files after the
  LLVM world frontier split. The split added a normal `.c` owner, not a new
  behavior-owning `.inc` lane.
- 2026-04-27 audit rerun: `find src tests -name '*.inc'` returns zero files,
  `make inc-sentinel-test-smoke`, `make semantic-inc-size-test-smoke`, and
  `make backend-inc-size-test-smoke` all pass. Stale references in the parallel
  proof doc and semantic owner comments were updated to the current `.cases.h`
  / owner-TU structure.
- Owner-size policy is now stricter than the historical `.inc` cleanup target:
  600 LOC is the default split-review threshold for any production `.c` or
  private owner `.h`; 1,000 LOC is only the hard stop / temporary risk line.
  A production owner above 600 LOC must either be split in the current sprint
  or be listed here with a named follow-up owner seam. New owners should aim
  below 600 LOC unless the file is a compact table, generated ABI surface, or a
  deliberately single-entry orchestration layer with no mixed responsibility.
- Architecture judgement update: 600 LOC is a review signal, not a mechanical
  split command. Splits must be by responsibility and source-of-truth seam.
  If the owner still has one coherent responsibility, keep it as one owner and
  improve its internal structure. Do not create new `_helpers` buckets just to
  satisfy a line-count target.
- The final pass-through and leaf helper shims were renamed to named private
  owner headers, including `pgy_runtime_inline_core.h`,
  `transpiler_base_a_emitters.h`, `transpiler_base_b_emitters.h`,
  `transpiler_expr_emitters.h`, `transpiler_helpers_core_{a,b}.h`,
  `transpiler_domain_role_emit.h`, and `llvm_expr_call_owners.h`.
- Compiler runtime cache freshness now tracks the renamed runtime owner
  headers instead of stale `.inc` dependency paths.
- `inc-sentinel-test-smoke` now rejects any `.inc` reintroduction under `src`.
  Test fragments use the explicit `.cases.h` suffix instead of a tolerated
  `.inc` fixture lane.
- `.cases.h` fragments are harness-owned, not fragment-owned: only
  `src/test_*.c` harness entrypoints may include them, and nested
  `.cases.h`-from-`.cases.h` aggregation is rejected. This keeps fixture
  ownership visible to the build inventory and prevents hidden test trees from
  reintroducing include-order debt.
- The root `src/test_*.c` harnesses now have a direct compile smoke:
  `source-test-harness-compile-test-smoke`. The gate catches missing standard
  includes, test-only portability leaks, and hidden fixture coupling without
  requiring full link/run. A native compiler sweep currently compiles all 16
  root harnesses after the security fixture was moved behind test-local
  mkdir/setenv wrappers and `test_security_comprehensive.c` gained its missing
  `<stdbool.h>` include.
- Test aggregator `.inc` shims have been removed from the semantic/transpile
  harnesses. `src/test_semantic.c` and `src/test_transpile.c` now include leaf
  fixture parts directly, and `test_semantic_async.cases.h` was split into
  `test_semantic_async_part_a.cases.h` /
  `test_semantic_async_part_b.cases.h` so every remaining test case include
  stays below the 990 LOC cap.
- C backend generated specialization registry logic no longer lives in a
  broad helper header. The implementation now lives in
  `src/codegen/transpiler_specialization_registry.c`, AST statement scanning
  lives in `src/codegen/transpiler_specialization_scan.c`, the header is
  declaration-only, and consumers now include their own type-mapping,
  role/ability, and Result suffix dependencies instead of relying on a hidden
  include-order body.
- C backend Result suffix parsing and `Result<T,E>` specialization discovery
  no longer live in `transpiler_type_result_mapping_helpers.h`. The
  implementation moved to `src/codegen/transpiler_type_result_mapping_helpers.c`,
  keeping the header as a declaration-only seam for Result/Option call lowering
  and MIR preserved-let emission.
- C backend HashMap stdlib lowering no longer lives in
  `transpiler_expr_stdlib_map_builtin.h`. The dispatch table and lowering body
  moved to `src/codegen/transpiler_expr_stdlib_map_builtin.c`, and concrete
  HashMap metadata validation moved into the shared collection support owner.
  The map builtin header is now declaration-only instead of an include-order
  implementation body.
- C backend Queue stdlib lowering now follows the same compiled-owner policy.
  `src/codegen/transpiler_expr_stdlib_queue_builtin.c` owns Queue dispatch and
  lowering, `transpiler_expr_stdlib_queue_builtin.h` is declaration-only, and
  unary collection metadata validation moved into the shared collection support
  owner. The collection builtin owner delegates HashMap and Queue before
  handling List/Set directly.
- C backend Result/Option builtin lowering moved from
  `transpiler_call_result_option_builtin_emit.h` into
  `src/codegen/transpiler_call_result_option_builtin_emit.c`. The header is now
  declaration-only. `src/codegen/transpiler_option_context.h` gives linked
  owners a narrow Option context seam instead of requiring the broad
  `transpiler_helpers_core_a.h` shim.
- C backend intent observability builtin lowering moved from
  `transpiler_intent_observability_builtin_emit.h` into
  `src/codegen/transpiler_intent_observability_builtin_emit.c`. The runtime
  observability contract smoke now checks the compiled owner, and the public
  header only exposes `emit_builtin_intent_observability(...)`.
- C backend projection/world lookup helpers no longer live as static
  implementation-header seams. `src/codegen/transpiler_projection.c` now owns
  overlay domain-slot lookup, projection-target detection, and world-state
  lookup; overlay invalidation, builtin query dispatch, world select emission,
  and domain constructor lowering consume the linked query API. The source
  inventory smoke rejects reopening the old local helper names in
  implementation headers.
- C backend domain query builtin lowering moved from
  `transpiler_expr_builtin_dispatch.h` into
  `src/codegen/transpiler_expr_domain_query_builtin.c`. `HasProjection`,
  `HasLayer`, `HasState`, `HasZone`, `HasZoneProjection`, `HasZoneLayer`, and
  `HasZoneState` now share a compiled owner; the dispatch header only routes
  those builtin families.
- C backend I/O and time builtin lowering moved from
  `transpiler_expr_builtin_dispatch.h` into
  `src/codegen/transpiler_expr_io_builtin.c`. File/runtime call bodies for
  `FileOpen`, `FileRead`, `FileWrite`, `FileClose`, `ReadFile`, `WriteFile`,
  `Input`, `Print`, `ReadLine`, `Now`, and `Sleep` now share a compiled owner
  instead of living in the central dispatch header.
- C backend domain constructor emission moved from
  `transpiler_call_constructor_result_emit.h` into
  `src/codegen/transpiler_domain_constructor_emit.c`. Class compound literals,
  party/roster/relation/effect/zone/world designated initializers, projection
  dirty defaults, world dirty defaults, and enum variant constructor call
  strings now share a compiled owner. The header is a thin dispatch wrapper
  because generic class specialization still depends on the local
  specialization seam.
- C backend expression core, composite literal, and array access lowering moved
  from implementation headers into compiled owners.
  `transpiler_expr_core_emit.c` owns binary/operator, coalescing, and checked
  div/mod lowering. `transpiler_expr_composite_literal_emit.c` owns tuple and
  Array literal lowering. `transpiler_expr_array_access_emit.c` owns
  Array/Slice checked access lowering. The public headers are now
  declaration-only, and perf/runtime panic contract smokes read the compiled
  owners.
- C backend Channel let lowering moved from `transpiler_let_channel_emit.h`
  into `src/codegen/transpiler_let_channel_emit.c`. The header is
  declaration-only, and the semantic core shape smoke checks the compiled owner
  for AST call accessor discipline.
- C backend Box/Rc let lowering moved from `transpiler_let_box_emit.h` into
  `src/codegen/transpiler_let_box_emit.c`. The compiled owner now carries
  `Box<T>`, `Box<Array<T>>`, and `Rc<T>` let-constructor lowering, while the
  header is declaration-only and guarded by `test-inc-size-test-smoke`.
- C backend Future/RemoteFuture type queries moved from
  `transpiler_func_forward_helpers.h` into
  `src/codegen/transpiler_future_type_query.c`. Spawn return type inference,
  RemoteFuture detection, and Future inner-type extraction now have a linked
  owner consumed by spawn, await, and let-registration paths.
- C backend post-let type registration moved from
  `transpiler_let_type_register_emit.h` into
  `src/codegen/transpiler_let_type_register_emit.c`. The header is
  declaration-only and the owner includes its `strdup_fmt(...)` dependency
  directly instead of relying on parent include order.
- C backend collection let lowering moved from `transpiler_let_collection_emit.h`
  into `src/codegen/transpiler_let_collection_emit.c`. The compiled owner now
  carries `Option<T>` `Some`/`None` let lowering plus stable `HashMap<String,T>`,
  `List<T>`, and `Queue<T>` constructor lowering. The header is
  declaration-only and is covered by `test-inc-size-test-smoke`.
- C backend Slot/View let lowering moved from `transpiler_let_slot_emit.h` into
  `src/codegen/transpiler_let_slot_emit.c`. The compiled owner now carries
  ClaimSlot/ClaimSecureSlot/ClaimDeviceSlot, ReadView/WriteView/MoveToken
  declarations, and Slot/SecureSlot sugar. MIR destructure/local-type lookup
  consume the same public helper declaration instead of include-order
  prototypes.
- C backend expression builtin dispatch moved from
  `transpiler_expr_builtin_dispatch.h` into
  `src/codegen/transpiler_expr_builtin_dispatch.c`. The compiled owner carries
  the central `BuiltinKind` switch and delegates to slot, domain query, I/O,
  allocator, Rc/Box, and intent observability owners through explicit
  declarations.
- C backend zone specialization emission is now source-inventory linked through
  `src/codegen/transpiler_zone_specialization_emit.c`; the header is
  declaration-only and uses the canonical hosted-method view declarations
  instead of a stale private include name.
- C backend zone struct/layer accessor emission moved from
  `transpiler_zone_struct_emit.h` into
  `src/codegen/transpiler_zone_struct_emit.c`. Zone slot/shared/state fields,
  projection readiness fields, hidden provenance fields, and generated layer
  accessors now have a compiled owner; the header is declaration-only.
- C backend control-flow statement lowering moved from
  `transpiler_control_flow_emit.h` into
  `src/codegen/transpiler_control_flow_emit.c`. The compiled owner carries
  `if`, `for`, `while`, loop-label lookup, and the shared condition-head
  formatter used by MIR branch terminator emission. The header is
  declaration-only, and `transpiler_mir_emit_state.h` no longer depends on a
  later include-order definition.
- C backend MIR CFG control lowering moved from
  `transpiler_mir_cfg_control_emit.h` into
  `src/codegen/transpiler_mir_cfg_control_emit.c`. MIR block/terminator
  emission now consumes linked APIs for loop init, for-in binding, backedge
  increment, branch condition rendering, and select readiness rendering.
- C backend MIR match condition lowering moved from
  `transpiler_mir_match_condition_emit.h` into
  `src/codegen/transpiler_mir_match_condition_emit.c`. Option/Result
  destructor pattern matching, payload binding, match guard composition, and
  match-subject discovery now have a compiled owner; CFG control lowering only
  consumes the public condition-rendering API.
- C MIR emission predicate wrappers no longer have a private implementation
  header. `transpiler_mir_emit_predicates.h` was deleted and its two callers now
  use the canonical reason-capable MIR contract APIs directly.
- The declaration-only header guardrail now includes the latest C backend split
  seams: Result/Option, domain constructor, expression core/composite/array
  access, expression builtin dispatch, domain query, I/O, HashMap/Queue stdlib,
  intent observability, and Future type query, Box/Rc let, Channel let,
  collection let, slot let, let
  type-register, zone specialization, zone struct/layer accessor,
  control-flow, MIR CFG control, and MIR match condition headers.
  `test-inc-size-test-smoke` rejects new function bodies in those headers.
- Shared AST type-to-C copy ownership now lives with type rendering:
  `pergyra_ast_type_to_c_copy(...)` moved from
  `transpiler_func_forward_helpers.h` to `src/codegen/transpiler_type_render.c`,
  matching its public declaration in `transpiler_type_render.h`.

## Current Owner-Size Audit

The active debt is no longer `.inc` inventory or hard-size overflow. It is
owner cohesion. As of the 2026-05-19 audit, all non-test production `.c` and
`.h` owners are below the 600 LOC split-review threshold. The immediate
priority is to keep the owner queue closed without reintroducing
behavior-owning `.inc` files, `_helpers` buckets, or mega-headers.

Current largest non-test production owners:

| File | LOC | Status |
| --- | ---: | --- |
| `src/parser/ast_api.h` | 581 | Parser accessor surface; below split threshold, watch API cohesion |
| `src/semantic/type_checker_internal.h` | 546 | Semantic internal declaration surface; below split threshold |
| `src/parser/ast.h` | 537 | Core AST declarations; below split threshold |
| `src/codegen/transpiler_expr_type_infer.c` | 537 | C expression type inference owner; below split threshold |
| `src/codegen/transpiler_specialization_registry.c` | 390 | C generated specialization registry owner; below split threshold |
| `src/codegen/transpiler_match_emit.c` | 517 | C match lowering owner; below split threshold |
| `src/runtime/pgy_parallel.h` | 516 | Runtime parallel API surface; below split threshold |
| `src/parser/ast_domain_constructors.c` | 516 | Parser domain constructor owner; below split threshold |
| `src/codegen/transpiler_intent_emit.c` | 524 | C intent orchestration compiled owner; below split threshold |
| `src/codegen/transpiler_intent_emit.h` | 8 | C intent orchestration declaration seam |
| `src/codegen/transpiler_mir_inventory_intent_collect.c` | 508 | MIR intent inventory collector; below split threshold |
| `src/semantic/slot_analyzer_summary.c` | 507 | Slot analyzer summary owner; below split threshold |
| `src/parser/ast_role_type_accessors.c` | 505 | AST role/type accessor owner; below split threshold |
| `src/codegen/llvm_internal.h` | 483 | LLVM internal API surface; below split threshold |
| `src/codegen/llvm_decl.c` | 278 | LLVM function declaration/body emission owner after authority/routine split |
| `src/compiler/mir_cfg_contract_validate.c` | 334 | MIR CFG contract validator for non-cleanup shape, source, loop, and unreachable-edge checks |
| `src/codegen/transpiler_let_slot_emit.c` | 390 | C slot/view let compiled owner; split from implementation header |
| `src/codegen/transpiler_let_collection_emit.c` | 340 | C collection let compiled owner; split from implementation header |
| `src/codegen/transpiler_mir_cfg_control_emit.c` | 303 | C MIR CFG control compiled owner; split from implementation header |
| `src/codegen/transpiler_control_flow_emit.c` | 279 | C control-flow compiled owner; split from implementation header |
| `src/codegen/transpiler_let_box_emit.c` | 248 | C Box/Rc let compiled owner; split from implementation header |
| `src/codegen/transpiler_expr_core_emit.c` | 162 | C expression core compiled owner; split from implementation header |
| `src/codegen/transpiler_expr_builtin_dispatch.c` | 158 | C expression builtin dispatch owner; split from implementation header |
| `src/codegen/llvm_decl_authority.c` | 141 | LLVM zone-authority declaration prelude owner; split from declaration orchestration |
| `src/codegen/transpiler_expr_composite_literal_emit.c` | 132 | C tuple/Array literal compiled owner; split from implementation header |
| `src/codegen/llvm_decl_routines.c` | 106 | LLVM function routine inventory orchestration owner; split from function emission |
| `src/codegen/transpiler_future_type_query.c` | 104 | C Future type query compiled owner; split from implementation header |
| `src/codegen/transpiler_expr_array_access_emit.c` | 77 | C Array/Slice access compiled owner; split from implementation header |
| `src/codegen/transpiler_let_channel_emit.c` | 53 | C Channel let compiled owner; split from implementation header |

MIR CFG cleanup validation is now intentionally split from the general
validator. `src/compiler/mir_cfg_contract_validate_cleanup.c` is 245 LOC and
owns cleanup-block shape, reachable cleanup-edge facts, rollback/invalidation
target checks, and cleanup convergence. This keeps both validator owners below
the 600 LOC signal threshold without hiding behavior in an implementation
header.

LLVM zone-authority declaration prelude emission is also split from the
function declaration owner. `src/codegen/llvm_decl_authority.c` is 141 LOC and
owns current-zone lookup, authority runtime-check emission, and structured
inventory-missing diagnostics. Function routine inventory orchestration lives
in `src/codegen/llvm_decl_routines.c` at 106 LOC, so
`src/codegen/llvm_decl.c` is 278 LOC and remains function forward/body emission.
- MIR CFG/body ownership is no longer a hard-size blocker:
  `src/compiler/mir.c` stays orchestration-only after SSA rename moved into
  `src/compiler/mir_ssa_rename.c`, versioned use-edge population moved into
  `src/compiler/mir_ssa_use_edges.c`, and liveness/value-summary/DCE plus
  statement-population families moved into their own compiled owners. The
  CFG/body smoke gate now keeps each owner below the 600 LOC split-review
  threshold and verifies the top-level MIR file only orchestrates block
  construction and pass ordering.
- LLVM world sync ownership is below the split threshold again:
  `src/codegen/llvm_domain_world_sync.c` is 164 LOC after moving bounded
  transitive frontier and derived-state recompute emission into
  `src/codegen/llvm_domain_world_frontier.c` at 470 LOC. The runtime frontier
  smoke gate now includes that owner directly.
- Semantic owner TU size is also back under the 600 LOC review threshold after
  domain, intent, ownership, and slot-view diagnostics moved into named owners.
- CFG/body flow keeps `type_checker_flow.c` as the if/match/block orchestration
  owner required by the CFG smoke, while `type_checker_flow_loop_control.c`
  owns `break` / `continue` loop-depth, label validation, and loop resource
  snapshot recording. `type_checker_flow.c` is now 488 LOC and the new
  loop-control owner is 55 LOC.
- Runtime slot utility ownership now has a separate TU:
  `src/runtime/slot_type_utils.c` owns `TypeTagHash`, `TypeTagToString`,
  `TypeIsPrimitive`, `TypeGetSize`, `SlotHashFunction`,
  `SlotCompareAndSwap`, and `SlotMemoryBarrier`. This was the first reduction
  slice before monitoring/scope and secure operations were extracted without
  changing the Slot ABI.
- Slot security owner boundaries are split as well:
  `src/runtime/slot_security_memory.c` owns secure memory primitive fallbacks,
  and `src/runtime/slot_security_platform.c` owns Windows/Linux hardware
  fingerprint retrieval, `src/runtime/slot_security_crypto.c` owns AES/HMAC
  token encryption, and `src/runtime/slot_security_sealed_payload.c` owns sealed
  payload obfuscation/MAC/shadow recovery. `slot_security.c` is now only the
  token/context/audit owner and is below the 1,000 LOC hard cap at 794 LOC.
- Slot manager monitoring has a separate owner:
  `src/runtime/slot_manager_security_stats.c` owns security event logging,
  anomaly detection, and security stats printing. `src/runtime/slot_manager_scope.c`
  owns secure scope lifecycle plus the high-level `pergyra_*` secure slot
  wrappers. That slice moved monitoring and scope code out of the lifecycle
  owner; the later secure-ops split below reduces it further.
- Slot manager secure operation ownership now has a separate owner:
  `src/runtime/slot_manager_secure_ops.c` owns security enable/disable, secure
  claim/read/write/release, token validation/refresh/revoke, and secure manager
  lifecycle wrappers. `src/runtime/slot_manager_internal.h` exposes the narrow
  lock/table/release helper seam needed by that owner. `slot_manager.c` now
  drops to 963 LOC, below the 1,000 LOC risk line, while the new secure owner is
  379 LOC.
- Slot manager pin ownership now has a separate owner:
  `src/runtime/slot_manager_pin.c` owns `PergyraSlotPin` / `PergyraSlotUnpin`,
  pinned view validation, secure payload open/seal, stale-generation rejection,
  release-while-pinned rejection, and Pin token validation. `slot_manager.c`
  now drops to 791 LOC and remains the core claim/read/write/release lifecycle
  owner. Verified with `make test-security` (142/0) and `make test-abi`
  (58/0 plus C/LLVM ABI pipeline smoke).
- Slot manager query/locking ownership now has a separate owner:
  `src/runtime/slot_manager_query_lock.c` owns type/validity queries, TTL
  refresh/cleanup, lock/unlock/try-lock, stats, and fast wrappers.
  `slot_manager.c` is now 564 LOC and stays focused on claim/read/write/release
  lifecycle and shared storage helpers. Verified with `make test-security`,
  `make test-abi`, `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.
- Lexer debug ownership now has a separate TU:
  `src/lexer/lexer_token_debug.c` owns token stringification and debug printing.
  `src/lexer/lexer.c` is now 573 LOC and stays focused on source scanning,
  token creation, and lexical diagnostics. Verified with `make test-parser`,
  `make test-semantic`, `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.
- Parallel runtime ownership is split without changing the public umbrella:
  `src/runtime/pgy_parallel.h` is now a 494 LOC shared task/await facade.
  `src/runtime/pgy_parallel_blocking.h` owns the blocking pool at 146 LOC, and
  `src/runtime/pgy_parallel_coroutine.h` owns coroutine scheduling at 292 LOC.
  This keeps the header-only ABI surface stable while moving scheduler bodies
  below the 600 LOC split-review threshold. Verified with `make pgy` and
  `make test-abi`.
- Intent parser ownership is split without changing parser exports:
  `src/parser/parser_intent.c` is now a 514 LOC declaration parser,
  `src/parser/parser_intent_defaults.c` owns intent-level `who` / `where`
  propagation at 69 LOC, and `src/parser/parser_intent_step.h` owns step
  clause parsing at 297 LOC. Verified with direct GCC compile probes plus
  `intent_compression_contract_smoke.sh`, `build_source_inventory_smoke.sh`,
  and `source_utf8_smoke.sh`.
- Expression parser string ownership is split:
  `src/parser/parser_expr.c` is now a 524 LOC expression precedence/call/primary
  owner, while `src/parser/parser_expr_string.h` owns multiline/interpolation
  helpers at 150 LOC. Verified with `make test-parser` and
  `make test-semantic`.
- Slot pool performance ownership is split:
  `src/runtime/slot_pool.c` is now below the 600 LOC split-review threshold and
  stays focused on pool/list allocation. `src/runtime/slot_pool_perf.c` owns
  timestamp, cache prefetch/alignment, and linked-list benchmark helpers.
  Verified with `make test-datastructures`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`.
- RIR builder body-walk ownership is split:
  `src/compiler/rir_builder.c` is now a 281 LOC scope-orchestration owner.
  `src/compiler/rir_builder_walk.c` owns AST body walking, slot/call/resource
  op materialization, and block-condition walking at 363 LOC. Verified with
  `make test-rir`, `make test-air`, `make test-mir`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`.
- Runtime LLVM slot/array/IO/string export ownership is split without changing
  the public runtime include seam:
  `src/runtime/pgy_runtime_lib_slot_array_io_string_exports.h` is now an 8 LOC
  facade over `pgy_runtime_lib_secure_slot_exports.h`,
  `pgy_runtime_lib_device_slot_exports.h`,
  `pgy_runtime_lib_array_map_exports.h`, and
  `pgy_runtime_lib_io_string_exports.h`. The split owners are 161, 84, 239, and
  296 LOC respectively and preserve the existing LLVM-linkable ABI symbol
  names. `compiler_runtime_cache_is_fresh(...)` also tracks each leaf owner so
  cached LLVM runtime objects cannot miss a split-header edit. Verified with
  `make pgy`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`.
- Runtime LLVM channel/qubit export ownership is split without changing the
  public runtime include seam:
  `src/runtime/pgy_runtime_lib_channel_quantum_exports.h` is now a 7 LOC facade
  over `pgy_runtime_lib_channel_int_exports.h`,
  `pgy_runtime_lib_channel_string_exports.h`, and
  `pgy_runtime_lib_qubit_state_exports.h`. The split owners are 327, 319, and
  69 LOC respectively and preserve the existing channel/qubit runtime symbols.
  `compiler_runtime_cache_is_fresh(...)` also tracks each leaf owner so cached
  LLVM runtime objects cannot miss a split-header edit. Verified with
  `make pgy`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`.
- Runtime LLVM raw collection export ownership is split without changing the
  public runtime include seam:
  `src/runtime/pgy_runtime_lib_raw_collection_exports.h` is now an 8 LOC facade
  over `pgy_runtime_lib_raw_collection_common_exports.h`,
  `pgy_runtime_lib_raw_queue_exports.h`,
  `pgy_runtime_lib_raw_map_exports.h`, and
  `pgy_runtime_lib_raw_set_exports.h`. The split owners are 13, 117, 431, and
  153 LOC respectively and preserve the existing raw Queue/HashMap/Set runtime
  symbols. `compiler_runtime_cache_is_fresh(...)` also tracks each leaf owner
  so cached LLVM runtime objects cannot miss a split-header edit. Verified with
  `make pgy`, `make test-abi`,
  `make production-header-size-test-smoke`, and `make backend-inc-size-test-smoke`.
- Driver scaffold ownership now has a separate TU:
  `src/compiler/driver_scaffold.c` owns `pgy scaffold` / `pgy new` file and
  project generation, while `src/compiler/driver_app.c` stays focused on
  diagnostics, dump modes, pipeline orchestration, runtime-mode gating, and
  backend dispatch. `driver_app.c` is now 831 LOC and the scaffold owner is
  812 LOC, so both are below the 1,000 LOC hard risk line.
- Compiler host-toolchain ownership now has a separate TU:
  `src/compiler/compiler_toolchain.c` owns safe process execution, C compiler
  discovery, target-flag selection, runtime object cache freshness, timing, LLD
  selection, and path safety. `compiler.c` is now 738 LOC and stays focused on
  compiler result ownership plus C/LLVM emit/link orchestration.
- Slot analyzer summary ownership now has a separate TU:
  `src/semantic/slot_analyzer_summary.c` owns Slot access summaries, escape
  summaries, helper-call propagation, and parameter summary facts.
  `slot_analyzer.c` is now 434 LOC and stays focused on pass lifecycle,
  function/block/if/parallel traversal, diagnostics, and program entry.
- HIR public surface ownership now has a separate TU:
  `src/compiler/hir_public.c` owns HIR dump modes, declaration/routine queries,
  and routine/block pass runners. `hir.c` is now 926 LOC and stays focused on
  top-level classification, hidden routine materialization, synthetic
  executable lowering, and reachability propagation.
- DIR ownership is now below the split-review threshold:
  `src/compiler/dir_collect.c` owns node, role, party, roster, world, and intent
  collection; `src/compiler/dir_collect_domain.c` owns zone / relation / effect
  slot and projection contract collection; `src/compiler/dir_validate.c` owns
  validation/dump/public naming; and `src/compiler/dir_internal.h` exposes only
  the lowering-local builder/find seam. `src/compiler/dir.c` is now 467 LOC,
  `dir_collect.c` is 546 LOC, and `dir_collect_domain.c` is 274 LOC. Verified
  with `make test-dir test-air test-rir`.
- Type environment ownership now has a separate TU:
  `src/semantic/type_env.c` owns `TypeEnv` create/destroy/add/lookup helpers.
  Lightweight inference/unification moved to `src/semantic/type_infer.c`, and
  function/resource effect helpers moved to `src/semantic/type_effects.c`.
  `src/semantic/type_system.c` is now 598 LOC and stays focused on built-in
  singleton lifecycle, type constructors, equality/assignability, constraints,
  and generic instantiation. Verified with `make test-semantic` (2357/0).
- Stdlib builtin semantic ownership now has scalar and map owners:
  `src/semantic/type_checker_builtins_stdlib_scalar.c` owns scalar, string, and
  math builtin calls, while
  `src/semantic/type_checker_builtins_stdlib_map.c` owns `HashMap` builtin
  calls. `src/semantic/type_checker_builtins_stdlib_body.c` is now 834 LOC and
  stays below the 1,000 LOC hard cap. Verified with `make test-semantic pgy`
  (2357/0).
- Zone declaration authority ownership now has a separate TU:
  `src/semantic/type_checker_zone_decl_authority.c` owns zone authority
  ability validation, duplicate authority diagnostics, layer-slot type
  validation, and relation/effect pool beta rejects. `type_checker_zone_decl.c`
  is now 929 LOC and stays focused on zone lifecycle/state rule validation.
  Verified with `make test-semantic pgy` (2357/0).
- Zone declaration lifecycle/state ownership is now split below the 600 LOC
  threshold: `src/semantic/type_checker_zone_shape.c` owns zone shape and
  lifecycle-density warnings, `type_checker_zone_projection_rules.c` owns
  refresh/publish/bind projection rule validation, and
  `type_checker_zone_state.c` owns maintained-state and state-alias validation.
  `type_checker_zone_decl.c` is now 558 LOC and stays focused on apply/link/
  detach/unlink/maintain lifecycle orchestration. Verified with
  `make test-semantic` (2357/0).
- Intent helper ownership now has role-field and control-transfer owners:
  `src/semantic/type_checker_intent_role_fields.c` owns role require-field
  validation plus intent transfer/zone-binding derivation helpers, and
  `src/semantic/type_checker_intent_control.c` owns intent-clause
  control-transfer rejection. `type_checker_intent_helpers.c` is now 195 LOC.
  Verified with `make test-semantic pgy` (2357/0).
- Intent declaration transfer/handoff contract ownership now has a separate
  TU: `src/semantic/type_checker_intent_transfer.c` owns transfer source/target
  alias validation, zone-binding checks, transfer target versus current zone
  contract checks, and `using` versus transfer-target consistency diagnostics.
  Intent step required-ability contract ownership now lives in
  `src/semantic/type_checker_intent_ability.c`, leaving
  `type_checker_intent_decl.c` focused on declaration/step orchestration,
  inference calls, and top-level priority/success/failure checks.
  `type_checker_intent_decl.c` is now 464 LOC and remains below the 600 LOC
  split-review threshold. Verified with targeted `gcc` object builds for both
  intent owners and `build-source-inventory`.
- Domain helper projection/overlay/contract ownership now has separate TUs:
  `src/semantic/type_checker_domain_projection.c` owns projection contract
  diagnostics, and `src/semantic/type_checker_overlay_common.c` owns overlay
  symbol/shared-field/hosted-method scope setup. Zone relation/effect contract
  arity checks, endpoint-kind matching, and provenance-heavy diagnostics now
  live in `src/semantic/type_checker_domain_contracts.c`.
  `type_checker_decls_domain_helpers.c` is now 448 LOC and
  `type_checker_domain_contracts.c` is 537 LOC, so the domain helper family is
  below the 600 LOC split-review threshold. Verified with `make test-semantic`
  (2357/0).
- Late function-call constructor ownership now has a separate TU:
  `src/semantic/type_checker_call_constructor.c` owns constructor-like calls
  for subject/class, relation/effect/roster/world/zone overlays, field
  initialization type checks, borrowed-boundary constructor-field escape
  validation, and world-zone embedding handoff diagnostics.
  `type_checker_helpers_late.c` is now 799 LOC and remains the callable
  dispatch / argument ownership / generic call-site owner; the constructor
  owner is 220 LOC. Verified with `make test-semantic` (2357/0).
- Parser domain ownership now has separate roster/world/zone/event TUs:
  `src/parser/parser_domain_roster.c` owns roster body parsing,
  `src/parser/parser_domain_world.c` owns world body parsing,
  `src/parser/parser_domain_zone.c` owns zone body parsing, and
  `src/parser/parser_domain_event.c` owns event signatures.
  Relation/effect declaration parsing now lives in
  `src/parser/parser_domain_relation_effect.c`; projection group parsing,
  domain group keyword matching, and projection field maps now live in
  `src/parser/parser_domain_projection.c`. `parser_domain.c` is now 493 LOC
  and keeps party/ability/role parsing plus the shared domain slot/child helper
  seam. Verified with `make test-parser pgy`.
- Parser declaration hint ownership now has a separate TU:
  `src/parser/parser_decl_hints.c` owns top-level declaration hint name
  extraction, registration, capacity growth, and lookup. `parser.c` is now
  867 LOC and stays focused on parser lifecycle, token movement, diagnostics,
  synchronization, statement finalization, and program/statement dispatch.
  Verified with `make test-parser pgy`.
- Parser declaration/type ownership is now split below the 600 LOC threshold:
  `src/parser/parser_decl.c` owns function/action, nominal type declaration,
  and extern-block parsing; `src/parser/parser_type.c` owns name-token helpers,
  generic parameters, type arguments, where clauses, type aliases, and type
  parsing; `src/parser/parser_decl_function_clause.c` owns function/action
  clause parsing and diagnostics. Current sizes are `parser_decl.c` 327 LOC,
  `parser_type.c` 351 LOC, and `parser_decl_function_clause.c` 230 LOC.
  Verified with `make test-parser`.
- Parser statement dispatch now has a separate owner:
  `src/parser/parser_statement_dispatch.c` owns declaration and statement
  dispatch. `src/parser/parser.c` stays focused on parser lifecycle, token
  movement, error handling, program parsing, and block/let/with/parallel leaf
  parsers. Current sizes are `parser.c` 414 LOC and
  `parser_statement_dispatch.c` 460 LOC. Verified with `make test-parser`.
- AST print ownership now has separate inline, generic, intent, event, domain,
  and misc owners: `src/parser/ast_print_inline.c` owns escaped string
  rendering, operator spelling, inline expression printing, and compact
  one-line printing; `src/parser/ast_print_generics.c` owns generic parameter
  and where-clause inline rendering; `src/parser/ast_print_intent.c` owns
  intent printers plus intent contract provenance printing;
  `src/parser/ast_print_event.c` owns event printers;
  `src/parser/ast_print_domain.c` owns domain/world/zone printers;
  `src/parser/ast_print_expr.c` owns compact expression-node debug printing;
  and `src/parser/ast_print_misc.c` owns trailing-newline policy. Current owner
  sizes are `ast_print.c` 469 LOC, `ast_print_domain.c` 539 LOC,
  `ast_print_inline.c` 382 LOC, `ast_print_intent.c` 253 LOC,
  `ast_print_expr.c` 108 LOC, `ast_print_event.c` 76 LOC, and
  `ast_print_generics.c` 63 LOC. The AST print family is now below the 600 LOC
  split-review threshold. Verified with targeted `gcc` object builds for
  `ast_print.c` and `ast_print_expr.c`.
- AST constructor ownership now has constructor, async-constructor,
  domain-constructor, and clone
  owners: `src/parser/ast_constructors.c` owns core statement/expression/basic
  type constructors, `src/parser/ast_async_constructors.c` owns async/channel
  constructors, `src/parser/ast_domain_constructors.c` owns domain/intent/
  party/event constructors, `src/parser/ast_domain_accessors.c` owns read-only
  ability/role/roster/world/relation/effect accessors,
  `src/parser/ast_zone_accessors.c` owns read-only zone accessors, and
  `src/parser/ast_clone.c` owns AST clone helpers.
- AST mutation/destruction ownership is now split below the 600 LOC threshold:
  `src/parser/ast.c` owns only mutation helpers, `src/parser/ast_destroy.c`
  owns generic/where/comment destruction plus non-domain destroy cases, and
  `src/parser/ast_destroy_domain.c` owns domain/world/zone/intent/party/
  ability/event destroy cases. Current sizes are `ast.c` 65 LOC,
  `ast_destroy.c` 393 LOC, `ast_destroy_domain.c` 456 LOC,
  `ast_constructors.c` 445 LOC and `ast_async_constructors.c` 117 LOC.
  Production owners are now below the 600 LOC gate. Verified with targeted
  object builds and source-inventory / inc-size smokes; local Git Bash does not
  provide `make`, and native `mingw32-make` still collides with Windows `find`.
- AST public type ownership now has a shared type header:
  `src/parser/ast_types.h` owns AST enums, forward declarations, generic
  parameter structs, function parameter structs, and class field structs.
  `src/parser/ast_api.h` owns the public AST constructor/manipulation
  prototype surface. `src/parser/ast.h` now owns the `ASTNode` shape and
  includes `ast_api.h` for source compatibility. `ast.h` is now 848 LOC and
  `ast_api.h` is 137 LOC. Verified with `make test-parser pgy` and
  `make production-header-size-test-smoke inc-sentinel-test-smoke`.
- LLVM backend type mapping now has a separate TU:
  `src/codegen/llvm_backend_type_map.c` owns type-name rendering, generic
  container argument extraction, `pergyra_type_to_llvm`, `ast_type_to_llvm`,
  and early forward-declare eligibility. `llvm_backend.c` is now 379 LOC and
  remains the context lifecycle/backend entry owner. Verified with `make pgy`
  and `make llvm-test-smoke`.
- LLVM zone frontier state ownership now has a separate TU:
  `src/codegen/llvm_domain_zone_frontier_state.c` owns previous-state storage,
  snapshotting, reset, and bounded frontier change detection for zone sync.
  `llvm_domain_zone_sync.c` is now 776 LOC and stays focused on zone
  propagation action/maintain/link/unlink orchestration. Verified with
  `make runtime-frontier-contract-test-smoke` and `make llvm-test-smoke`.
- LLVM backend generic/temp ownership now has a separate TU:
  the stale `#if 0` copy of old `LLVMGenCtx` inventory was removed from
  `src/codegen/llvm_backend.c`, and `src/codegen/llvm_backend_generic.c` now
  owns temp-name generation, generic template lookup, monomorphization tracking,
  LLVM type suffix mapping, and entry-block alloca creation. `llvm_backend.c`
  is now 999 LOC and `make pgy` remains green.
- LLVM domain sync frontier ownership now has a separate TU:
  `src/codegen/llvm_domain_sync_frontier.c` owns sync-generation increments,
  frontier overflow abort lowering, and post-sync builder restoration.
  `src/codegen/llvm_domain_zone_sync.c` is now 997 LOC and remains focused on
  zone bounded-frontier sync body emission. `make pgy llvm-test-smoke
  production-header-size-test-smoke` remains green.
- Parser declaration/core ownership now has focused TUs:
  `src/parser/parser_decl_clause.c` owns contextual function/action clause and
  effect-mask parsing, `src/parser/parser_doc.c` owns structured comment
  collection/attachment, `src/parser/parser_pin.c` owns pin-block parsing,
  `src/parser/parser_zone_context.c` owns lexical `within Zone { ... }`
  propagation, `src/parser/parser_enum.c` owns enum-body parsing,
  `src/parser/parser_export.c` owns export declaration dispatch, and
  `src/parser/parser_decl_start.c` owns declaration lookahead. `parser_decl.c`
  is now 887 LOC and `parser.c` is now 977 LOC, both below the 1,000 LOC hard
  cap. Verified with `make test-parser pgy
  backend-inc-size-test-smoke production-header-size-test-smoke
  inc-sentinel-test-smoke`.
- C backend scalar/math/string stdlib call lowering now lives in
  `src/codegen/transpiler_expr_stdlib_scalar_builtin.h`. The main
  `transpiler_expr_stdlib_builtin.h` dispatcher drops from 917 LOC to 751 LOC
  while preserving dispatch order.
- C backend Map/List/Set/Queue stdlib call lowering now lives in
  `src/codegen/transpiler_expr_stdlib_collection_builtin.h`. The main
  dispatcher drops further to 432 LOC while preserving collection runtime
  specialization order.
- `production-header-size-test-smoke` caps production owner headers at 1,000
  LOC by default, with no temporary per-header allowance. LLVM declaration
  inventory helpers now live behind `src/codegen/llvm_inventory_internal.h`,
  with lookup and host-method metadata split into dedicated helper owners, so
  `src/codegen/llvm_internal.h` stays under the same production cap.
- LLVM statement parallel/async/select lowering now lives in
  `src/codegen/llvm_stmt_parallel_async.c`. `src/codegen/llvm_stmt.c` drops to
  3,078 LOC, and the full `llvm-test-backend-compare` suite remains green.
- LLVM statement loop/match lowering now lives in
  `src/codegen/llvm_stmt_loop_match.c`. `src/codegen/llvm_stmt.c` drops to
  2,582 LOC, while the control-flow owner keeps `while`, `for`, and `match`
  parity covered by the full backend compare suite.
- LLVM statement ownership now has separate real TUs for type inference,
  let helpers, let lowering, with lowering, loop/match lowering, and
  parallel/async/select lowering:
  `src/codegen/llvm_stmt_type_infer.c`,
  `src/codegen/llvm_stmt_let_helpers.c`,
  `src/codegen/llvm_stmt_let_with.c`, `src/codegen/llvm_stmt_with.c`,
  `src/codegen/llvm_stmt_loop_match.c`, and
  `src/codegen/llvm_stmt_parallel_async.c`. `src/codegen/llvm_stmt.c` is now
  914 LOC, and every statement owner TU is below 1,000 LOC while backend
  compare remains green.
- LLVM intent MIR metadata readers now live in
  `src/codegen/llvm_intent_mir_meta.c`, with the private seam declared in
  `src/codegen/llvm_intent_internal.h`. `src/codegen/llvm_intent.c` drops from
  2,394 LOC to 2,118 LOC while `make llvm-test-backend-compare` remains green.
- LLVM intent zone binding/sync helpers now live in
  `src/codegen/llvm_intent_zone.c`, with the private seam declared in
  `src/codegen/llvm_intent_internal.h`. Zone slot-name resolution, bound-zone
  materialization, handoff transfer tracing, projection dirty/ready stamping,
  effective-zone sync, and alias restore moved out of
  `src/codegen/llvm_intent.c`; the orchestration owner drops further to
  1,665 LOC while `llvm_intent_zone.c` stays below the 600 LOC split-review
  threshold at 463 LOC. `make LLVM_ENABLED=1 /tmp/pgy-PergyraLang-bin/pgy
  llvm-test-backend-compare` remains green.
- LLVM intent effect provenance helpers now live in
  `src/codegen/llvm_intent_effect.c`, with the private seam declared in
  `src/codegen/llvm_intent_internal.h`. Caused-effect inference and
  layer/state epoch/cause stamping moved out of `src/codegen/llvm_intent.c`;
  the orchestration owner drops further to 1,496 LOC while
  `llvm_intent_effect.c` stays below the 600 LOC split-review threshold at
  181 LOC. `make LLVM_ENABLED=1 /tmp/pgy-PergyraLang-bin/pgy
  llvm-test-backend-compare` remains green.
- LLVM intent flow/signature helpers now live in
  `src/codegen/llvm_intent_flow.c`, with the private seam declared in
  `src/codegen/llvm_intent_internal.h`. MIR routine lookup, MIR step/check/eval
  collection, dispatch alias collection, MIR resource hooks, authority
  validation, and forward declaration signature generation moved out of
  `src/codegen/llvm_intent.c`; the body emission owner drops further to
  953 LOC, below the 1,000 LOC hard cap, while `llvm_intent_flow.c` stays below
  the 600 LOC split-review threshold at 563 LOC. `make LLVM_ENABLED=1
  /tmp/pgy-PergyraLang-bin/pgy llvm-test-backend-compare` remains green.
- LLVM domain method lookup, implicit-self classification, operator alias
  helpers, and propagation provenance stamping now live in
  `src/codegen/llvm_domain_method_helpers.c`. `src/codegen/llvm_domain.c`
  drops to 3,340 LOC.
- LLVM world sync emission now lives in
  `src/codegen/llvm_domain_world_sync.c`. `src/codegen/llvm_domain.c` drops to
  2,663 LOC, and the helper include width was reduced so the build remains
  warning-clean under the current `-Wall -Wextra` gate.
- LLVM zone sync emission now lives in
  `src/codegen/llvm_domain_zone_sync.c`. `src/codegen/llvm_domain.c` drops to
  1,649 LOC, and the former `llvm_domain_core_helpers.h` mega-header is split
  into focused owner headers for role lookup, declaration parts, projection
  count/value/sync body, and zone-layer binding. This keeps the extracted zone
  TU warning-clean without adding unused attributes or new `.inc` files.
- LLVM event helper generation now lives in `src/codegen/llvm_domain_event.c`.
  Event type registration plus `INIT` / `SUBSCRIBE` / `UNSUBSCRIBE` /
  `INVOKE` helper lowering no longer lives in `src/codegen/llvm_domain.c`,
  which drops further to 1,356 LOC. The extraction also removes the previous
  fixed 8-entry handler parameter type array; wider events now materialize
  LLVM handler parameter types from the full event arity.
- LLVM role method body/operator/vtable emission now lives in
  `src/codegen/llvm_domain_role_emit.c`. `src/codegen/llvm_domain.c` drops
  further to 1,125 LOC while preserving the MIR-routine-missing hard-error
  path through a `bool` helper return.
- LLVM domain sync and domain method body emission now lives in
  `src/codegen/llvm_domain_method_emit.c`. `src/codegen/llvm_domain.c` drops
  below the 1,000 LOC hard cap to 895 LOC; remaining domain work is
  declaration/type orchestration and can be treated as split-review debt rather
  than a hard-size blocker.
- MIR slot/claim type helper extraction now lives in
  `src/compiler/mir_type_helpers.c` / `.h`. `src/compiler/mir.c` drops from
  2,927 LOC to 2,742 LOC without changing MIR lowering behavior, and
  `make test-mir` remains green.
- MIR cleanup/rollback/invalidation edge ownership now lives in
  `src/compiler/mir_cleanup.c` / `.h`. Cleanup instruction creation,
  rollback-policy invalidation, cleanup block creation, and cleanup edge
  materialization moved out of `src/compiler/mir.c`; `mir.c` drops further to
  2,485 LOC without changing MIR lowering behavior. `make test-mir`,
  `make mir-declaration-inventory-test-smoke`, `make backend-inc-size-test-smoke`,
  and `make production-header-size-test-smoke` remain green.
- MIR intent instruction materialization now lives in
  `src/compiler/mir_intent.c` / `.h`. Intent participant, step, zone alias,
  authority, check/eval, dispatch, compensation, and invalidation marker
  lowering moved out of `src/compiler/mir.c`; `mir.c` drops further to 2,132
  LOC and `mir_intent.c` is 386 LOC. `make test-mir`,
  `make mir-declaration-inventory-test-smoke`, `make backend-inc-size-test-smoke`,
  and `make production-header-size-test-smoke` remain green.
- AIR evidence collection now lives in `src/compiler/air_evidence.c`, with
  internal name/error helper declarations in `src/compiler/air_internal.h`.
  `src/compiler/air.c` drops from 1,279 LOC to 1,111 LOC, and
  `tests/air_drift_smoke.sh` now validates AIR implementation terms across
  `air.c` and `air_evidence.c`. `make test-air air-drift-test-smoke
  air-backend-nonimpact-test-smoke air-strict-backend-compare-test-smoke`
  remains green.
- AIR DAG evidence collection now lives in `src/compiler/air_evidence_dag.c`.
  `src/compiler/air_evidence.c` is now 474 LOC and stays focused on HIR/MIR
  evidence plus runtime schema/frontier evidence; the DAG owner is 67 LOC and
  consumes only `SemanticResult` DAG counters and metadata dead-end facts.
  Verified with direct GCC probes, `air_drift_smoke.sh`,
  `type_resolution_resolver_inventory_smoke.sh`, `perf_contract_smoke.sh`, and
  `build_source_inventory_smoke.sh`.
- AIR boundary traversal now lives in `src/compiler/air_boundary.c`. Boundary
  classification, source derivation, sync-class mapping, step boundary counting,
  and boundary node append moved out of `src/compiler/air.c`; `air.c` drops
  further to 671 LOC and is below the 1,000 LOC hard cap. The AIR drift smoke
  now validates implementation terms across `air.c`, `air_boundary.c`, and
  `air_evidence.c`, and the AIR strict/nonimpact gates remain green.
- AIR global verification now lives in `src/compiler/air_verify.c`.
  Inventory validation, authority participant validation, evidence provenance
  validation, sync/async drift detection, and strict evidence drift emission
  moved out of `src/compiler/air.c`; the synthesis owner is back below the
  600 LOC split-review threshold while `air_verify.c` is the single verification
  owner. `tests/air_drift_smoke.sh` now validates AIR implementation terms
  across `air.c`, `air_boundary.c`, `air_dump.c`, `air_evidence.c`, and
  `air_verify.c`.
- HIR analysis extraction now lives in `src/compiler/hir_analysis.c` / `.h`.
  Signature type-reference collection, direct-call collection, and
  control-flow presence detection moved out of `src/compiler/hir.c`; `hir.c`
  drops from 2,445 LOC to 2,109 LOC. `make test-hir test-rir test-mir` and
  `make air-drift-test-smoke` remain green.
- HIR CFG extraction now lives in `src/compiler/hir_cfg.c` / `.h`. CFG
  predecessor finalization, reachability, dominator/frontier, dominator tree,
  loop-depth, local-def, phi-candidate, phi-materialization, and CFG summary
  ownership moved out of `src/compiler/hir.c`. HIR CFG construction lowering
  now lives in `src/compiler/hir_lower_cfg.c` / `.h`, so the AST-body to
  basic-block construction path is separate from post-construction CFG facts.
  `hir.c` drops further to 1,255 LOC, `hir_cfg.c` is 599 LOC, and
  `hir_lower_cfg.c` is 270 LOC. `make test-hir test-rir test-mir`,
  `make air-drift-test-smoke`, `make backend-inc-size-test-smoke`, and
  `make production-header-size-test-smoke` remain green.
- Type-resolution DAG fallback classification now lives in
  `src/semantic/type_checker_resolution_metadata_dead_end.c`. The metadata
  materializer file drops from 907 LOC to 806 LOC while the exact fallback
  family counters remain unchanged under `make type-resolution-dag-test-smoke`.
- Type-resolution DAG stable constructed materialization now lives in
  `src/semantic/type_checker_resolution_metadata_constructed.c`, and owned
  metadata cleanup now lives in `src/semantic/type_checker_resolution_metadata_storage.c`.
  Metadata cache index operations now live in
  `src/semantic/type_checker_resolution_metadata_index.c` at 163 LOC.
  `src/semantic/type_checker_resolution_metadata.c` drops further to 373 LOC,
  so the metadata lookup/materialization policy owner is comfortably below the
  600 LOC split-review threshold.
- Type-resolution DAG stage signature replay now lives in
  `src/semantic/type_checker_resolution_stage_signature.c`. The top-level stage
  replay owner drops from 914 LOC to 594 LOC and now stays under the 600 LOC
  split-review threshold.
- Type-resolution DAG body type-reference precollection now lives in
  `src/semantic/type_checker_resolution_graph_body.c`. The declaration
  inventory owner drops from 892 LOC to 561 LOC and now stays under the 600 LOC
  split-review threshold.
- Type-resolution DAG program inventory is now a dispatcher owner:
  `src/semantic/type_checker_resolution_graph_inventory.c` drops to 98 LOC and
  delegates zone inventory to
  `src/semantic/type_checker_resolution_graph_zone_inventory.c`.
- Type-resolution DAG zone inventory is split at the state/authority tail seam:
  `src/semantic/type_checker_resolution_graph_zone_inventory.c` is 554 LOC, and
  `src/semantic/type_checker_resolution_graph_zone_tail.c` is 169 LOC. After
  this split, every `src/semantic/type_checker_resolution_*.c` DAG owner is
  below the 600 LOC split-review threshold while the graph/resolver smoke gates
  remain green.
- Type-resolution retired-resolver ownership is now explicit: the obsolete
  `src/semantic/type_checker_resolve.c` owner is gone, retired compatibility
  counters live in `src/semantic/type_checker_resolution_retired.c`, and
  general assignability / constructed-type helpers live in
  `src/semantic/type_checker_type_helpers.c`. The resolver inventory and
  semantic shape smokes reject bringing the old resolver owner back.

- `src/codegen/transpiler_context.c` now owns the C backend output/context
  primitives that used to live in include bodies:
  - `CodeBuf` allocation, growth, raw/formatted writes, and file dump
  - `TranspilerCtx` create/destroy ownership
  - indentation emission helpers
  - backend error message/hint allocation
  - transpiler scratch arena string helpers
- `src/codegen/transpiler_context.h` is the private C backend seam consumed by
  `transpiler.c`; these helpers are no longer behavior owned by
  `transpiler_helpers_core_a_part_a.inc`.
- `src/codegen/transpiler_symbols.c` now owns C backend local symbol
  bookkeeping that used to live in include bodies:
  - slot variable registration and token lookup
  - typed local registration and lookup
  - alias expression registration and lookup
  - view-like and projection-borrow local metadata
- `src/codegen/transpiler_symbols.h` is the private C backend seam consumed by
  `transpiler.c` and the included emitter fragments. This keeps ownership ABI
  tracking out of `transpiler_helpers_core_a_part_a.inc`.
- `src/codegen/transpiler_decl_lookup.c` now owns C backend declaration
  lookup helpers that used to live in include bodies:
  - MIR-backed named declaration lookup
  - function/intent/callable lookup including extern functions
  - nominal/domain lookup for class, zone, world, relation, effect, party, and
    roster declarations
  - private C backend type-alias target resolution
  - declaration inventory cache lookup
  - current host binding lookup
  - nominal host declaration cache lookup
  - declaration method list projection
  - current-host method lookup
  - nominal-host method lookup and cache update
  - ability and event declaration lookup
- `src/codegen/transpiler_decl_lookup.h` is the private C backend seam consumed
  by `transpiler.c` and the included emitter fragments. This removes another
  AST-carried declaration inventory helper family from `.inc` ownership.
- `src/codegen/transpiler_projection.c` now owns C backend projection
  provenance, world/zone query lookup, and nominal type predicates:
  - C-backend-prefixed zone domain slot lookup
  - current world field predicate lookup
  - zone state and layer slot lookup
  - world zone slot lookup
  - world-zone declaration resolution
  - nested vessel-backed projection source path resolution
  - projection literal lowering shared by `ToObject`, `ToTObject`, and zone
    refresh-map emission
  - subject type predicate lookup
  - nominal host type predicate lookup
- `src/codegen/transpiler_projection.h` is the private C backend seam consumed
  by `transpiler.c` and the included emitter fragments. This removes the last
  behavior-heavy helper body from `transpiler_helpers_core_a_part_a.inc`.
- `src/codegen/transpiler_nominal.c` now owns C backend nominal member type
  lookup and host-expression type resolution:
  - current-host field type lookup
  - class/subject/zone/world/relation/effect member type lookup
  - nominal host expression type resolution for lowered member access
- `src/codegen/transpiler_type_render.h` exposes the private
  `transpiler_render_type_name_local()` seam so real translation units can
  reuse the existing generic-binding-aware type renderer without static buffer
  ownership.
- `src/codegen/transpiler_enum.c` now owns enum variant qualification lookup.
- `src/codegen/transpiler_operator.c` now owns C backend operator-overload
  lookup, operator method alias matching, and role operator method traversal.
- `src/codegen/llvm_domain_role_lookup.c` now owns LLVM role target-type access
  through `llvm_role_for_type_node(...)` / `llvm_role_for_type_name(...)`.
  Forward declarations and role operator emission no longer read
  `role_decl.for_type` directly, so the future MIR role-target metadata lift has
  one compatibility seam instead of duplicated AST reads.
- The C backend mirrors the role target-type seam in
  `src/codegen/transpiler_decl_host_lookup.c` through
  `transpiler_role_subject_type_node_local(...)` and
  `transpiler_role_subject_type_name_local(...)`. Operator lookup and alias
  emission consume these helpers instead of reading `role_decl.for_type`
  directly.
- The semantic layer mirrors the same role target-type seam in
  `src/semantic/type_checker_domain_role_lookup.c` through
  `semantic_role_for_type_node(...)` and `semantic_role_for_type_name(...)`.
  Operator overload and ability role-matching checks now consume the helper
  instead of revalidating `role_decl.for_type` locally.
- `src/semantic/type_checker_domain_role_lookup.c` also owns semantic role
  declaration lookup through `semantic_find_role_decl(...)`. Operator overload
  and ability include traversal now share that program-scan seam instead of
  carrying duplicate local lookup functions. Both semantic include traversals
  now guard `AST_INCLUDE_STMT` shape before reading include payloads.
- Intent role-field validation now consumes the same
  `semantic_role_for_type_name(...)` seam when binding a role to its subject
  type, keeping role contract validation aligned with operator overload and
  ability matching.
- Role declaration include validation now consumes
  `semantic_find_role_decl(...)` and guards `AST_INCLUDE_STMT` shape before
  reading include payloads, matching the other semantic role include traversals.
- Role declaration host-type validation now consumes
  `semantic_role_for_type_node(...)` / `semantic_role_for_type_name(...)`,
  leaving direct role target-type AST access in the semantic role lookup owner.
- DAG graph precollect and staged nominal resolution now guard role include
  names before recording or resolving include dependencies, matching the
  semantic and backend role include traversal guards.
- DAG role host-type precollect and staged nominal resolution now consume
  `semantic_role_for_type_node(...)`, leaving direct role target-type AST access
  only in the semantic/backend role lookup owner seams.
- Role include payload access now goes through `ast_include_role_name(...)` /
  `ast_include_type_args(...)`. Semantic, DAG, C backend, and LLVM role
  traversal paths no longer duplicate include-node shape/name guards. The
  smoke gates now reject non-parser direct `data.include_stmt` reads.
- Role impl ability access now has AST accessors:
  `ast_impl_ability_ref(...)`, `ast_impl_ability_name(...)`,
  `ast_impl_ability_method_count(...)`, and `ast_impl_ability_method(...)`.
  Operator lookup, ability matching, role declaration validation, DAG role impl
  precollect/staged resolution, DIR/HIR/MIR role impl collection, and C/LLVM
  role vtable/method emission now consume these accessors instead of direct
  impl ability payload reads. The accessors are const-correct for read-only
  scanners, and the smoke gate now rejects direct non-parser
  `data.impl_ability` consumers under semantic/compiler/codegen.
- Role child-list access now has AST accessors:
  `ast_role_for_type(...)`, `ast_role_include_count(...)`,
  `ast_role_include(...)`, `ast_role_impl_count(...)`, and
  `ast_role_impl(...)`. Semantic, compiler, C backend, and LLVM paths no longer
  read `role_decl.for_type` / include arrays / impl arrays directly; only role
  declaration metadata such as names, generics, and parallel block remain as
  explicit role declaration surface reads.
- Ability method-list access now has AST accessors:
  `ast_ability_name(...)`, `ast_ability_method_count(...)`, and
  `ast_ability_method(...)`. Compiler/codegen consumers use these for ability
  vtable/forward declaration/DIR completeness/name lookup scans instead of
  reading ability name/method payloads directly. The module normalizer remains
  the explicit exception because it owns mutable declaration-name rewriting.
- Role declaration names now also have a read-only AST accessor:
  `ast_role_name(...)`. Compiler/codegen role-name consumers use it for DIR,
  HIR, MIR declaration headers, C declaration lookup, and LLVM declaration
  inventory/role emission. `module_normalizer.c` remains the explicit
  exception because it owns mutable declaration-name rewriting.
- Party and roster declaration names now follow the same read-only accessor
  policy through `ast_party_name(...)` and `ast_roster_name(...)`. DIR/HIR/MIR,
  C declaration lookup, C domain emission, and LLVM declaration inventory now
  consume these accessors; `module_normalizer.c` remains the sole mutable-name
  rewrite exception for compiler/codegen.
- Party and roster child-list reads now have AST accessors:
  `ast_party_role(...)`, `ast_party_shared(...)`, `ast_party_method(...)`,
  `ast_roster_party(...)`, `ast_roster_shared(...)`, and
  `ast_roster_method(...)` plus their count helpers. DIR edge collection,
  runtime-none scanning, module reference normalization, C constructor/domain
  emission, bind/member helper emission, and LLVM struct field registration now
  consume these accessors. Read-only array-view helpers
  `ast_party_methods(...)`, `ast_roster_methods(...)`,
  `ast_party_shared_fields(...)`, and `ast_roster_shared_fields(...)` close
  the remaining method compatibility and shared-field view owners without
  exposing raw party/roster child-list payloads to compiler/codegen.
- World/relation/effect/zone declaration names now have read-only AST
  accessors: `ast_world_name(...)`, `ast_relation_name(...)`,
  `ast_effect_name(...)`, and `ast_zone_name(...)`. DIR/HIR/MIR declaration
  headers, RIR scope/fact collection, C declaration lookup/emission, LLVM
  declaration inventory, and C/LLVM projection/sync hot paths now consume these
  accessors for declaration names. `module_normalizer.c` remains the explicit
  exception because it owns mutable declaration-name rewriting; the semantic
  core shape smoke gates that boundary. Semantic builtin query diagnostics,
  DAG label formatting, and domain lookup/precollect owners now also consume
  the same accessors for the closed DAG/builtin declaration lookup paths.
  Relation/effect/world declaration validators plus small zone shape/projection
  validators now follow the same read-only name seam for diagnostics and
  contract validation. DAG domain precollect/stage owners and zone command/tail
  dependency labels also consume the accessors instead of raw declaration-name
  payloads. Program-level domain placeholders and action-contract lexical-zone
  derivation now also consume the same accessor seam, so forward placeholder
  setup and inferred `within` metadata no longer rediscover domain names from
  raw AST payloads. Projection contract diagnostics, systemic world method
  staging, intent effect-slot diagnostics, and zone authority diagnostics are
  also smoke-gated on the same accessor boundary. World graph precollect now
  threads the resolved world name once through activate/deactivate/maintain and
  action-contract dependency labels instead of reopening the AST payload.
  Intent participant transfer diagnostics and zone state/maintain diagnostics
  now also resolve the zone name once through `ast_zone_name(...)`. Intent
  authority diagnostics share the same resolved zone-name value across missing,
  ambiguous, and non-authority approval paths. The main zone declaration
  validator now uses the accessor seam for overlay registration and all
  lifecycle diagnostics. Zone relation/effect contract validation now resolves
  zone/relation/effect names through accessors, closing the remaining semantic
  raw-name payload reads for world/relation/effect/zone declarations. The same
  boundary is now applied to party/roster names across semantic placeholder,
  DAG precollect/stage, and declaration validation owners; only
  `module_normalizer.c` may take mutable name slots for rewrite. Relation/effect
  declaration slot/refresh child lists now have read-only AST accessors, and
  zone relation/effect contract validation consumes those accessors instead of
  opening declaration payload arrays directly. DAG domain staging also consumes
  relation/effect/zone child-list accessors for slots, shared fields,
  authorities, layer slots, and methods. Semantic host overlay helpers now use
  the same child-list accessor seam for roster/world/zone/relation/effect field
  counting, field lookup, authority scans, and effect-layer checks. DAG domain
  local-contract staging now uses world/zone lifecycle child-list accessors for
  state, activation, refresh, apply/link, detach/unlink, and maintain scans.
  World declaration validation now consumes world roster/zone/lifecycle/shared/
  method child-list accessors instead of raw payload arrays. Zone declaration
  validation now consumes zone child-list accessors for overlay registration,
  slot validation, lifecycle scans, authority checks, and maintain conflict
  checks. DAG world precollect now consumes world child-list accessors for
  roster, zone, shared field, state, lifecycle, maintain, and method scans.
  Domain builtin query helpers now consume world/zone/relation/effect
  child-list accessors for slot, layer-slot, refresh, state, and projection
  host lookup seams. Relation/effect declaration validation now consumes
  relation/effect child-list accessors for overlay registration, slot
  validation, projection contracts, and bindable endpoint/target density
  checks; scalar `between` endpoint metadata remains declaration-owned payload.
  Zone state validation now consumes zone child-list accessors for maintained
  state aliases, detach/unlink conflict scans, authority presence checks, and
  duplicate state diagnostics. DAG zone command precollect now consumes zone
  child-list accessors for refresh/apply/link/detach/unlink and maintain
  command dependency scans. DAG zone state/authority tail precollect now
  consumes zone child-list accessors for state, maintained-state, authority,
  and method dependency scans. Zone shape warning density checks now consume
  zone child-list accessors for slot counts, lifecycle command totals, and
  authority presence. Shared domain declaration helpers now consume zone
  child-list accessors for slot, layer-slot, state, authority, and participant
  subject-slot lookup seams. DAG relation/effect precollect now consumes
  relation/effect child-list accessors for slot/shared/method type and
  action-contract scans; scalar relation `between` endpoint metadata remains
  declaration-owned payload. Host expression lookup now consumes world,
  relation, effect, and zone child-list accessors for world field resolution
  and host method dispatch. World helper lookup now consumes world/zone
  child-list accessors for world zone/state lookup and nested zone layer/state
  lookup. Builtin domain query predicates now consume relation/effect/zone
  child-list accessors for projection refresh and zone state predicate checks.
  DAG systemic stage replay now consumes party/roster/world child-list
  accessors for role/party slots, shared fields, world roster/zone slots, and
  method scans. Zone authority validation now consumes zone child-list
  accessors for authority and layer-slot scans. DAG zone inventory precollect
  now consumes zone child-list accessors for slot, shared field, and layer-slot
  inventory scans. DAG systemic precollect now consumes party/roster child-list
  accessors for role/party slot, shared field, and method inventory scans.
  Party/roster declaration validation now consumes party/roster child-list
  accessors for role/party slot, shared field, and method checks. Action
  contract validation now consumes zone/effect child-list accessors for
  within-zone subject-slot checks and caused-effect target checks.
  Intent authority validation now consumes zone child-list accessors for
  authority-count and subject-slot matching checks.
  Overlay hosted scope registration now consumes zone/world child-list
  accessors for bare slot and world-zone symbol registration. Intent
  participant validation now consumes zone child-list accessors for participant
  subject-slot matching and transfer source-zone checks. World state validation
  and world embedding handoff diagnostics now consume world child-list accessors
  for state and zone-slot scans. World query/member/constructor checks and
  zone projection contract/rule checks now consume AST child-list accessors;
  the semantic owner raw child-list audit for world/zone/relation/effect/
  party/roster declaration payloads is at zero. C intent/overlay zone-slot
  helpers and RIR intent effect-slot lookup now also consume zone child-list
  accessors instead of reopening zone declaration payload arrays. LLVM world
  sync now consumes world zone child-list accessors for active/dirty pass
  emission. LLVM zone authority checks, projection sync calls, intent effect
  provenance emission, and MIR declaration-header validation now consume AST
  child-list accessors for authority/slot/refresh/layer/state/method scans.
  C intent effect provenance and MIR-function zone-authority guard emission now
  use the same zone child-list accessor seam. LLVM world frontier recompute now
  consumes world zone/state child-list accessors for transitive frontier and
  derived-state passes. C MIR SSA implicit-field detection and RIR domain slot
  lookup now consume zone/relation/effect child-list accessors. HIR routine
  collection and MIR declaration-header method metadata now consume domain
  method child-list accessors for world/relation/effect/zone methods. C/LLVM
  hosted method views now consume the same domain method accessor seam, and LLVM
  world/zone effect propagation consumes zone/effect child-list accessors for
  slot, layer, state, and projection-refresh scans. Domain frontier pass-limit
  policy now consumes world/zone child-list accessors for zone/state/layer
  counts instead of reopening declaration payload counters. LLVM domain lookup
  and world sync directive emission now consume world/zone child-list accessors
  for world-zone, world-state, zone-state, zone-slot, layer-slot, and
  activate/maintain/deactivate scans. LLVM zone sync clause emission now
  consumes zone layer/state/detach child-list accessors for action-caused state
  updates and detach-driven layer/state invalidation. C overlay projection
  invalidation now consumes relation/effect/zone slot and refresh child-list
  accessors instead of reopening host declaration payload lists. C zone struct
  emission now consumes zone slot/layer/shared/state child-list accessors for
  field layout and layer accessor generation. DIR collection now consumes
  world/relation/effect/zone child-list accessors for world roster/zone edges,
  relation/effect slot-refresh edges, and zone slot/layer/authority/refresh/state
  edges. LLVM assignment projection invalidation now consumes
  zone/relation/effect slot-refresh child-list accessors for host and
  world-embedded projection scans.
  C overlay host-field lookup now consumes zone/relation/effect slot,
  layer-slot, and shared-field child-list accessors.
  C projection lookup helpers now consume world/zone child-list accessors for
  world field lookup, zone slot/state/layer lookup, and world-zone resolution.
  LLVM zone sync now consumes zone apply/state/maintained-effect/maintained-state
  child-list accessors for apply/maintain provenance and binding propagation.
  C projection sync helpers now consume world/zone/effect child-list accessors
  for zone action effects, world-embedded action/effect sync, and world-state
  lookup.
  C relation/effect emission now consumes relation/effect slot/shared/refresh
  child-list accessors for struct fields and projection sync loops.
  Runtime-none contract scanning now consumes relation/effect
  slot/refresh/shared/method child-list accessors for no-runtime surface
  rejection.
  LLVM zone bind helpers now consume zone/effect/relation
  layer/slot/refresh child-list accessors for effect/relation layer binding.
  LLVM zone relation sync now consumes zone link/state/maintained-relation/unlink
  child-list accessors for relation lifecycle propagation.
  LLVM domain declaration parts now consume world/relation/effect/zone
  shared/slot/refresh child-list accessors before handing child inventories to
  declaration emitters.
  LLVM zone frontier state tracking now consumes zone state/layer child-list
  accessors for previous-state allocation, snapshot, reset, and frontier
  continue checks.
  LLVM constructor calls now consume world/zone/relation/effect
  zone/slot/refresh/shared child-list accessors for constructor dirty/default
  initialization.
  C overlay zone bind helpers now consume zone/effect/relation
  layer/slot/refresh child-list accessors for effect/relation layer binding.
  C world emission now consumes world roster/zone/shared/state/directive
  child-list accessors for struct layout, world sync, derived recompute, and
  frontier continuation checks.
- C backend included-role emission now guards `AST_INCLUDE_STMT` shape before
  reading include payloads through the same AST include accessor as the
  semantic/C-operator/LLVM role lookup traversal paths.
- `src/codegen/transpiler_type_alias.c` now owns C backend type-alias
  declaration emission. The old `emit_type_alias_decl(...)` body was removed
  from `transpiler_emitters_base_b_part_c.inc`, and the implementation now uses
  the existing private type-render/type-specialization seams from a real
  translation unit.
- `src/codegen/transpiler_type_require.c` now owns C backend type requirement
  checks for AST type nodes and resolved type names. The old
  `src/codegen/transpiler_emitters_type_require.inc` include body was deleted,
  `transpiler_emitters_base_a_part_a.inc` no longer includes it, and the helper
  is now a real translation-unit seam shared by extern, declaration, and domain
  emitters.
- `src/codegen/transpiler_extern.c` now owns C backend extern declaration
  emission. `emit_extern_block(...)` was removed from
  `transpiler_emitters_base_b_part_b.inc`, keeping the extern pass as a real
  declaration owner instead of another include-body function.
- `src/codegen/transpiler_type_declarator.c` now owns C backend declarator
  rendering for ordinary types, event-handler function pointers, function
  pointer values, and function signatures. This removes another shared helper
  family from `transpiler_helpers_core_b_part_c.inc`.
- `src/codegen/transpiler_log_normalize.c` now owns C backend LogBanner
  indentation normalization. The remaining expression-core lowering later moved
  to `transpiler_expr_core_emit.h`.
- `src/runtime/pgy_runtime_intent_exit.h` now owns the generated-C inline
  intent exit implementation. The ABI surface remains `static inline
  pgy_intent_exit_export(...)`, but the large observability snapshot/cleanup
  body no longer lives in `pgy_runtime_part_ba_part_b.inc`.
- `src/runtime/pgy_runtime_slot_macros.h` now owns the generated-C inline
  `DeviceSlot<T>` and `SecureSlot<T>` macro families. The built-in slot
  instantiation order now stays in `pgy_runtime_builtin_storage_inline.h`, but the macro
  bodies no longer inflate that split include.
- `src/runtime/pgy_runtime_intent_history.h` now owns the generated-C inline
  last-intent history step accessors. The exported inline ABI names remain
  unchanged, while `pgy_runtime_intent_trace_inline.h` now carries the
  per-field borrowed string accessor block.
- `src/runtime/pgy_runtime_lib_core_exports.h` now owns LLVM-linkable runtime
  core exports for logging, wall-clock sleep/time, and integer string
  conversion. `pgy_runtime_lib_part_b_part_a.inc` is now focused on collection
  raw export bodies instead of carrying unrelated core runtime functions.
- `src/runtime/pgy_runtime_lib_list_raw_exports.h` now owns LLVM-linkable raw
  `List<T>` collection exports. Runtime lib part A is now focused on raw queue
  and map exports instead of carrying all raw collection families together.
- `src/codegen/transpiler_destructure_emit.c` now owns C backend
  `let`-destructuring statement lowering. The declaration-only header keeps
  the base-B statement dispatcher surface stable, but tuple/array destructuring
  lowering no longer depends on include order.
- `src/runtime/pgy_runtime_queue_inline.h` now owns generated-C inline queue
  macro and built-in `Int`/`String` queue implementations. Runtime part E is no
  longer a mixed queue/pool/FSM/authority/result include body.
- `src/runtime/pgy_runtime_map_int_key_inline.h` now owns generated-C inline
  `HashMap<Int>` key adapters for `Int`, `Long`, and `Bool` keys. Runtime part
  D is now focused on string map/list/set bodies instead of carrying those
  adapter wrappers inline.
- `src/runtime/pgy_runtime_lib_slot_exports.h` now owns LLVM-linkable primitive
  slot exports for `Slot<Double>`, `Slot<Bool>`, and `Slot<String>`. Runtime
  lib part D now starts at secure-slot exports instead of carrying primitive
  slot ABI bodies inline.
- `src/runtime/pgy_runtime_lib_std_exports.h` now owns LLVM-linkable standard
  string/conversion/math/random exports. Runtime lib part E now starts at the
  channel runtime section, and the lifetime smoke inventory reads these private
  owner headers in runtime-lib include order.
- `src/compiler/mir_decl_headers.h` now owns MIR declaration-header inventory
  helpers and method-routine linking. `mir_public_part_a.inc` now starts at
  `mir_lower(...)` instead of carrying declaration inventory helper bodies.
- `src/compiler/rir_names.h` now owns public RIR vocabulary name helpers for
  scope, fact, resource, state, and op kinds. `rir_public_surface.h` now focuses
  on dump surfaces instead of carrying name vocabulary bodies.
- `src/codegen/transpiler_parallel_capture.h` now owns C backend parallel
  capture discovery and capture-list deduplication. The async/parallel emitter
  keeps the same lowering surface, but `transpiler_emitters_base_b_part_b.inc`
  no longer carries the capture-analysis helper family inline.
- `src/codegen/transpiler_expr_stdlib_builtin.h` now owns C backend stdlib call
  lowering. The expression emitter shim still preserves include order, but
  `transpiler_expr_emitters_part_d.inc` now only carries the event-call builtin
  helper instead of the full stdlib dispatch body.
- `src/codegen/transpiler_overlay_projection.h` now owns C backend
  overlay/projection invalidation and zone-layer bind helpers. The
  `transpiler_helpers_core_a_part_b.inc` include body was removed, so this
  cleanup reduces the source `.inc` count instead of creating another split.
- `src/codegen/transpiler_let_emit.h` now owns C backend `let` declaration
  lowering. The base-A part keeps MIR inventory/SSA helper declarations, but no
  longer carries the entire `emit_let_decl(...)` body.
- `src/codegen/transpiler_mir_block_emit.h` now owns C backend MIR block
  statement emission and MIR emission eligibility wrappers. The
  `transpiler_emitters_base_a_part_c.inc` include body was removed instead of
  split further.
- `src/codegen/transpiler_intent_emit.c` now owns C backend intent declaration
  emission. The `transpiler_emitters_intent.inc` include body was removed, so no
  production `.inc` file remains above 900 LOC. The public
  `transpiler_intent_emit.h` seam is now declaration-only.
- `src/runtime/pgy_runtime_intent_query_inline.h` now owns generated-C inline
  intent active-step/recent query accessors, while
  `src/runtime/pgy_runtime_panic_checked_inline.h` owns panic helpers and
  checked arithmetic exports.
  Runtime part B now starts at stack/box/arena/allocator helpers, and runtime
  ABI lifetime inventory reads this private header in generated-runtime include
  order.
- The empty C backend tail include
  `src/codegen/transpiler_helpers_core_a_part_d.inc` was removed from the
  `transpiler_helpers_core_a.inc` shim.
- The empty compiler/runtime tail sentinels were removed from the current split
  families:
  - `src/compiler/mir_public_part_c.inc`
  - `src/runtime/pgy_runtime_part_ba_part_f.inc`
  Their shims now include only files that carry real implementation content,
  and contract tests point at the implementation-owning parts instead of empty
  tail placeholders.
- `src/codegen/transpiler_expr_emitters.inc` is now a shim over named private
  expression owners such as `transpiler_expr_core_emit.h`,
  `transpiler_expr_dispatch_emit.h`, and the remaining focused helper parts.
- The old expression-emitter split that crossed `emit_call`,
  `emit_binary`, and helper function bodies was removed.
- `emit_call` was reduced from a multi-thousand-line mixed dispatcher into
  dedicated helpers:
  - builtin dispatch
  - domain constructor lowering
  - `Result` / `Option` lowering
  - stable stdlib lowering
  - event call lowering
  - member-style call lowering
  - final user-call lowering
- `src/codegen/transpiler_intent_zone_binding_emit.c` now owns intent
  forward-declaration and zone-bound alias restore emission; the matching
  header is declaration-only and no longer leaves dangling `static void`
  return-type fragments for the intent emitter.
- `src/codegen/transpiler_emitters_intent.inc` now owns the full
  `emit_intent_decl` signature at its file boundary.
- Runtime ABI lifetime smoke was updated so runtime split-file checks read the
  whole split family instead of assuming a symbol remains in a fixed old part.

## Current Gate

The production include debt gate is green:

```sh
make backend-inc-size-test-smoke
make semantic-inc-size-test-smoke
find src -path src/tests -prune -o -name '*.inc' -print
```

The contract is now:

```text
production .inc under src/runtime  = 0
production .inc under src/codegen  = 0
production .inc under src/compiler = 0
production .inc under src/semantic = 0
test .inc under src/tests          = 0
test case includes under src/tests = 84 .cases.h files
```

Empty include sentinels are rejected:

```sh
make inc-sentinel-test-smoke
```

This gate rejects any `.inc` file under `src`, rejects `.cases.h` fragments
outside `src/tests`, rejects empty test case include fragments, and caps the
test fragment inventory at the current 84 files unless
`PGY_MAX_TEST_CASE_INCLUDES` is deliberately raised with this ledger. There is
also a usage check: `.cases.h` can only be included by the dedicated test
harnesses, every include must resolve under `src/tests`, and every `.cases.h`
must be referenced by a test harness or a smoke script. There is no
empty-sentinel allowlist. New behavior-owning `.inc` splits are blocked by
default.

Owner-size policy is separate from the `.inc` gate:

```text
600 LOC  = split-review threshold for production .c and private owner .h
1000 LOC = hard cap for new owner headers and active risk line for owner TUs
```

The current large-owner snapshot was last refreshed on 2026-04-28. The leading
production split candidates are:

```text
986 src/parser/ast_print.c
977 src/parser/parser.c
973 src/parser/ast.h
972 src/semantic/type_checker_helpers_late.c
972 src/semantic/type_checker_decls_domain_helpers.c
970 src/parser/parser_domain.c
965 src/codegen/transpiler_intent_emit.h
963 src/runtime/slot_manager.c
962 src/semantic/type_checker_intent_decl.c
953 src/codegen/llvm_intent.c
946 src/lsp/pgy_lsp.c
940 src/semantic/type_system.c
935 src/codegen/llvm_internal.h
932 src/codegen/llvm_registry.c
929 src/semantic/type_checker_zone_decl.c
928 src/codegen/transpiler_overlay_projection.h
```

Test harness files are intentionally excluded from the first owner-split queue
even when they exceed 600 LOC; they should be reduced after the production
compiler/runtime/codegen seams are stable.

The MIR public implementation split is also below the production cap after
moving the public lowering entry points into a named private owner and the
public name helpers / `mir_destroy(...)` into the second public owner:

```text
src/compiler/mir_lower_public_api.h 290
src/compiler/mir_public_surface.h 420
```

The production `.inc` gate is now stricter than the previous 1,000 LOC cap:
no production `.inc` files remain under `src` outside test fixtures.

```text
production_inc_count=0
production_inc_loc=0
```

After the local symbol/slot tracking extraction,
`src/codegen/transpiler_helpers_core_a_part_a.inc` is down to 422 LOC.
After the declaration lookup extraction, it is down further to the remaining
projection/type predicate helpers at 169 LOC.
After the projection/type predicate extraction, it is now a forward-declaration
shim only at 14 LOC.
That forward-declaration part file has now been folded into
`src/codegen/transpiler_helpers_core_a.inc`, deleting
`src/codegen/transpiler_helpers_core_a_part_a.inc`.
After the declaration cache/current-host extraction,
`src/codegen/transpiler_helpers_core_b_part_a.inc` is down to 687 LOC.
After the current-host/nominal-host method lookup extraction and world/zone
query extraction, `src/codegen/transpiler_helpers_core_b_part_a.inc` is down to
578 LOC, while `src/codegen/transpiler_helpers_core_a_part_c.inc` is down to
577 LOC.
After the ability/event declaration lookup extraction,
`src/codegen/transpiler_helpers_core_b_part_a.inc` is down to 565 LOC.
After the nominal member type lookup extraction, it is down to 298 LOC.
After the enum/operator lookup extraction, it is down to 161 LOC.
After the projection literal lowering extraction, it is down to 70 LOC.
That remaining small part file has now been folded into
`src/codegen/transpiler_helpers_core_b.inc`, deleting
`src/codegen/transpiler_helpers_core_b_part_a.inc`.

The codegen helper shim now has no empty tail part:

```text
src/codegen/transpiler_helpers_core_a.inc
  -> inline declarations, part_b, part_c
src/codegen/transpiler_helpers_core_b.inc
  -> inline small bridge helpers, part_b, part_c, part_d
```

## Verification

The cleanup was verified with:

```sh
make pgy test-transpile backend-inc-size-test-smoke
make test-semantic test-transpile test-inc-size-test-smoke
make inc-sentinel-test-smoke
make LLVM_ENABLED=1 llvm-test-smoke test-abi runtime-abi-lifetime-test-smoke
git diff --check -- src/codegen src/runtime src/compiler src/semantic tests
```

Observed results:

- `test-transpile`: 673 passed, 0 failed.
- `test-semantic`: 2337 passed, 0 failed.
- `llvm-test-smoke`: all listed LLVM smoke cases passed.
- Latest local slice reran `make pgy`,
  `make test-transpile backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make test-semantic`, and `make LLVM_ENABLED=1 llvm-test-smoke`.
- Latest follow-up reran `make pgy`,
  `make test-transpile backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  and `make LLVM_ENABLED=1 llvm-test-smoke`.
- Latest enum/operator extraction reran `make pgy`,
  `make test-transpile backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  and `make LLVM_ENABLED=1 llvm-test-smoke`.
- Latest projection-literal extraction reran `make pgy`,
  `make test-transpile backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  and `make LLVM_ENABLED=1 llvm-test-smoke`.
- Latest `.inc` count reduction deleted
  `transpiler_helpers_core_a_part_a.inc` and
  `transpiler_helpers_core_b_part_a.inc`, reran `make pgy`, and tightened
  `make inc-sentinel-test-smoke` with a source `.inc` file-count cap.
- Latest pass-through shim reduction deleted `transpiler_emitters.inc`,
  `transpiler_emitters_base.inc`, `transpiler_helpers_core.inc`,
  `llvm_expr_helpers.inc`, `llvm_expr_call_methods.inc`,
  `llvm_domain_helpers.inc`, `mir_public.inc`, `pgy_runtime_part_b.inc`, and
  `pgy_runtime_lib_part_b.inc`. Their owning `.c` / `.h` files now include the
  concrete implementation chunks directly.
- Latest MIR public split cleanup keeps `src/compiler/mir_public_part_a.inc`
  at 959 LOC and `src/compiler/mir_public_part_b.inc` at 800 LOC. Verified by
  `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make mir-declaration-inventory-test-smoke`, and `make pgy`.
- Latest lean debt-slice extraction moved C backend type-alias declaration
  emission into `src/codegen/transpiler_type_alias.c`, reducing
  `src/codegen/transpiler_emitters_base_b_part_c.inc` to 976 LOC. Verified by
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`.
- Latest type-require extraction deleted
  `src/codegen/transpiler_emitters_type_require.inc`, moved the checks into
  `src/codegen/transpiler_type_require.c`, and reduced
  `src/codegen/transpiler_emitters_base_a_part_a.inc` to 905 LOC. Verified by
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` and touched
  path `git diff --check`.
- Latest extern-emitter extraction moved `emit_extern_block(...)` into
  `src/codegen/transpiler_extern.c`, reducing
  `src/codegen/transpiler_emitters_base_b_part_b.inc` from 998 LOC to 957 LOC.
  The source `.inc` sentinel now uses the current 159-file cap by default.
  Verified by `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`
  and touched path `git diff --check`.
- Latest type-declarator extraction moved event-handler/function declarator
  rendering into `src/codegen/transpiler_type_declarator.c`, reducing
  `src/codegen/transpiler_helpers_core_b_part_c.inc` from 992 LOC to 849 LOC.
  Verified by `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`
  and touched path `git diff --check`.
- Latest LogBanner normalization extraction moved multiline string
  normalization into `src/codegen/transpiler_log_normalize.c`, reducing
  `src/codegen/transpiler_expr_emitters_part_a.inc` from 991 LOC to 878 LOC.
  Verified by `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`
  and touched path `git diff --check`.
- Latest runtime intent-exit extraction moved generated-C inline intent exit
  cleanup/snapshot logic into `src/runtime/pgy_runtime_intent_exit.h`, reducing
  `src/runtime/pgy_runtime_part_ba_part_b.inc` from 996 LOC to 894 LOC without
  changing the exported inline ABI name. Verified by
  `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, and touched path
  `git diff --check`.
- Latest runtime slot-macro extraction moved generated-C inline DeviceSlot and
  SecureSlot macro bodies into `src/runtime/pgy_runtime_slot_macros.h`, reducing
  `src/runtime/pgy_runtime_part_ba_part_c.inc` from 996 LOC to 808 LOC while
  preserving the built-in slot instantiation order. Verified by
  `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, and touched path
  `git diff --check`.
- Latest runtime intent-history extraction moved generated-C inline last-history
  step accessors into `src/runtime/pgy_runtime_intent_history.h`, reducing
  `src/runtime/pgy_runtime_part_ba_part_a.inc` from 989 LOC to 867 LOC while
  preserving the borrowed string ABI. `runtime_abi_lifetime_smoke.sh` now reads
  the private runtime inline headers as part of the generated-C runtime family.
  Verified by `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, and touched path
  `git diff --check`.
- Latest LLVM runtime-lib core export extraction moved logging/time/sleep and
  `pgy_int_to_string(...)` into `src/runtime/pgy_runtime_lib_core_exports.h`,
  reducing `src/runtime/pgy_runtime_lib_part_b_part_a.inc` from 986 LOC to
  909 LOC while preserving exported runtime symbol names. Verified by
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, and touched path
  `git diff --check`.
- Latest C backend destructuring extraction moved `AST_LET_DESTRUCTURE`
  lowering into `src/codegen/transpiler_destructure_emit.c`, superseding the
  temporary implementation header and keeping `transpiler_destructure_emit.h`
  declaration-only. The original extraction reduced
  `src/codegen/transpiler_emitters_base_b_part_c.inc` from 976 LOC to 873 LOC.
  Verified by `test-transpile`, `perf-contract-test-smoke`,
  `semantic-core-shape-test-smoke`, `test-inc-size-test-smoke`, and
  `build-source-inventory-test-smoke`.
- Latest generated-C queue extraction moved queue macro and built-in queue
  implementations into `src/runtime/pgy_runtime_queue_inline.h`, reducing
  `src/runtime/pgy_runtime_part_ba_part_e.inc` from 969 LOC to 773 LOC.
  Verified by `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-panic-codegen-test-smoke runtime-abi-lifetime-test-smoke
  test-abi`, targeted backend compare for `queue_pop_string` and
  `parallel_channel_sum`, and touched path `git diff --check`.
- Latest generated-C map key-adapter extraction moved `HashMap<Int>` adapters
  for `Int`/`Long`/`Bool` keys into
  `src/runtime/pgy_runtime_map_int_key_inline.h`, reducing
  `src/runtime/pgy_runtime_part_ba_part_d.inc` from 963 LOC to 815 LOC.
  Verified by `make -B pgy`, `make backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-panic-codegen-test-smoke`, targeted backend
  compare for `map_keys` and `map_get_string`, and touched path
  `git diff --check`.
- Latest LLVM runtime-lib primitive slot export extraction moved `Slot<Double>`,
  `Slot<Bool>`, and `Slot<String>` exported bodies into
  `src/runtime/pgy_runtime_lib_slot_exports.h`, reducing
  `src/runtime/pgy_runtime_lib_part_b_part_d.inc` from 947 LOC to 790 LOC while
  preserving exported ABI names. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-abi-test-smoke runtime-panic-codegen-test-smoke
  runtime-abi-lifetime-test-smoke test-abi`.
- Latest LLVM runtime-lib std export extraction moved `StringJoin`, `ToInt`,
  `ToFloat`, math functions, and random seeding into
  `src/runtime/pgy_runtime_lib_std_exports.h`, reducing
  `src/runtime/pgy_runtime_lib_part_b_part_e.inc` from 817 LOC to 761 LOC.
  Verified by `make runtime-abi-lifetime-test-smoke test-abi
  backend-inc-size-test-smoke inc-sentinel-test-smoke`.
- Latest LLVM runtime-lib raw list export extraction moved raw `List<T>`
  collection exports into `src/runtime/pgy_runtime_lib_list_raw_exports.h`,
  reducing `src/runtime/pgy_runtime_lib_part_b_part_a.inc` from 909 LOC to
  759 LOC. Verified by `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-panic-codegen-test-smoke
  runtime-abi-lifetime-test-smoke test-abi`.
- Latest MIR declaration-header extraction moved declaration inventory helper
  bodies into `src/compiler/mir_decl_headers.h`, reducing
  `src/compiler/mir_public_part_a.inc` from 959 LOC to 789 LOC. Verified by
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke air-drift-test-smoke test-abi`.
- Latest RIR vocabulary extraction moved public name helpers into
  `src/compiler/rir_names.h`, reducing `src/compiler/rir_public.inc` from
  911 LOC to 804 LOC. Verified by `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke type-resolution-dag-test-smoke air-drift-test-smoke
  test-abi`.
- Latest parallel-capture extraction moved C backend capture discovery and
  capture-list deduplication into `src/codegen/transpiler_parallel_capture.h`,
  reducing `src/codegen/transpiler_emitters_base_b_part_b.inc` from 957 LOC to
  730 LOC. Verified by `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke parallel-core-contract-test-smoke
  runtime-panic-codegen-test-smoke` and targeted backend compare for
  `parallel_channel_sum`.
- Latest stdlib-call extraction moved C backend stdlib call lowering into
  `src/codegen/transpiler_expr_stdlib_builtin.h`, reducing
  `src/codegen/transpiler_expr_emitters_part_d.inc` from 946 LOC to 26 LOC.
  Verified by `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-codegen-test-smoke` and targeted backend compare for
  `string_io`, `array_builtins`, `list_get_string`, and `map_get_string`.
- Latest overlay/projection extraction moved C backend overlay invalidation and
  zone-layer bind helpers into `src/codegen/transpiler_overlay_projection.h`
  and removed `src/codegen/transpiler_helpers_core_a_part_b.inc`, lowering the
  source `.inc` count to 158/159. `runtime_frontier_contract_smoke.sh` was also
  corrected to read the real world frontier owner in
  `transpiler_domain_role_part_d.inc`. Verified by
  `make runtime-frontier-contract-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke` and targeted backend compare for
  `world_embedded_branch_projection_visibility` and
  `world_embedded_action_frontier`.
- Latest let-emitter extraction moved C backend `let` declaration lowering into
  `src/codegen/transpiler_let_emit.h`, reducing
  `src/codegen/transpiler_emitters_base_a_part_a.inc` from 905 LOC to 138 LOC.
  Verified by `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  test-transpile` and targeted backend compare for `destructure_array`,
  `array_builtins`, and `map_keys`.
- Latest MIR block-emitter extraction moved C backend MIR block statement
  emission into `src/codegen/transpiler_mir_block_emit.h` and removed
  `src/codegen/transpiler_emitters_base_a_part_c.inc`. The source `.inc` total
  is now 49,911 LOC. Verified by `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke test-transpile type-resolution-dag-test-smoke
  air-drift-test-smoke` and targeted backend compare for `destructure_array`,
  `destructure_tuple_return`, `host_method_class_return`, and
  `world_embedded_branch_projection_visibility`.
- Latest intent-emitter extraction moved C backend intent declaration emission
  into `src/codegen/transpiler_intent_emit.c` and removed
  `src/codegen/transpiler_emitters_intent.inc`. The source `.inc` total is now
  48,949 LOC, and no production `.inc` remains above 900 LOC. A later cleanup
  made `src/codegen/transpiler_intent_emit.h` declaration-only. Verified by
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  test-transpile runtime-panic-codegen-test-smoke` and targeted backend compare
  for `intent_authority_snapshot` and `intent_failure_observability_strings`.
- Latest runtime query/panic extraction moved generated-C inline intent
  active-step/recent query accessors into
  `src/runtime/pgy_runtime_intent_query_inline.h` and panic helpers / checked
  arithmetic exports into `src/runtime/pgy_runtime_panic_checked_inline.h`,
  reducing
  `src/runtime/pgy_runtime_part_ba_part_b.inc` from 894 LOC to 705 LOC and
  source `.inc` total to 48,761 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-codegen-test-smoke runtime-panic-abi-test-smoke
  runtime-abi-lifetime-test-smoke test-abi`.
- Latest runtime intent-active extraction moved generated-C inline last/active
  borrowed exports into `src/runtime/pgy_runtime_intent_active_exports.h`,
  reducing `src/runtime/pgy_runtime_part_ba_part_a.inc` from 867 LOC to
  558 LOC and source `.inc` total to 48,453 LOC. The active and recent ABI
  smoke groups now point at their real owners instead of relying on one broad
  concatenated source family. Verified by `make runtime-abi-lifetime-test-smoke
  backend-inc-size-test-smoke inc-sentinel-test-smoke` and `make -B pgy
  runtime-panic-codegen-test-smoke runtime-panic-abi-test-smoke test-abi`.
- Latest LLVM runtime intent-export extraction moved non-inline intent
  borrowed exports into `src/runtime/pgy_runtime_lib_intent_exports.h`,
  reducing `src/runtime/pgy_runtime_lib_part_b_part_c.inc` from 852 LOC to
  315 LOC and source `.inc` total to 47,916 LOC. This makes generated-C inline
  and LLVM-linkable intent export ownership symmetric. Verified by `make
  runtime-abi-lifetime-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke` and `make -B pgy runtime-panic-codegen-test-smoke
  runtime-panic-abi-test-smoke test-abi`.
- Latest LLVM method-call projection extraction moved world/zone projection
  sync helpers into `src/codegen/llvm_expr_call_projection_sync.h`, reducing
  `src/codegen/llvm_expr_call_methods_part_a.inc` from 880 LOC to 671 LOC and
  source `.inc` total to 43,918 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke` and targeted backend
  compare for `world_embedded_branch_projection_visibility`,
  `world_embedded_action_frontier`, `world_embedded_action_pool_frontier`, and
  `world_zone_projection_visibility`.
- Latest C backend MIR SSA contract extraction moved identifier mapping and
  verification helpers into `src/codegen/transpiler_mir_ssa_contract.h`,
  reducing `src/codegen/transpiler_emitters_base_a_part_d.inc` from 849 LOC to
  677 LOC and source `.inc` total to 43,715 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile`.
- Latest C backend slot builtin extraction moved slot/device expression
  emitters into `src/codegen/transpiler_slot_builtin_emit.h`, reducing
  `src/codegen/transpiler_expr_emitters_part_a.inc` from 797 LOC to 531 LOC and
  source `.inc` total to 43,406 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile
  runtime-panic-codegen-test-smoke`.
- Latest C backend expression type-inference extraction moved
  `infer_expression_type_name(...)` into
  `src/codegen/transpiler_expr_type_infer.h`, reducing
  `src/codegen/transpiler_helpers_core_b_part_c.inc` from 797 LOC to 296 LOC
  and source `.inc` total to 42,906 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile`.
- Latest C backend statement-dispatch extraction moved `emit_statement(...)`
  into `src/codegen/transpiler_statement_dispatch.h`, reducing
  `src/codegen/transpiler_emitters_base_b_part_c.inc` from 803 LOC to 546 LOC
  and source `.inc` total to 42,650 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile` and
  targeted backend compare for `break_continue`, `parallel_channel_sum`, and
  `intent_header_interleaved`.
- Latest generated-C runtime string-map extraction moved `HashMap<String>` and
  map-key inline runtime into `src/runtime/pgy_runtime_map_string_inline.h`,
  reducing `src/runtime/pgy_runtime_part_ba_part_d.inc` from 767 LOC to 377 LOC
  and source `.inc` total to 42,261 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-codegen-test-smoke test-abi`
  and targeted backend compare for `map_get_string`, `map_keys`,
  `list_get_string`, `queue_pop_string`, and
  `intent_failure_observability_strings`.
- Latest lean debt batch moved C backend MIR function emission into
  `src/codegen/transpiler_mir_func_emit.h`, reducing
  `src/codegen/transpiler_emitters_base_b_part_a.inc` from 766 LOC to 162 LOC.
  The same batch moved generated-C runtime array sort kernels and scalar
  std/log/math helpers into `src/runtime/pgy_runtime_array_sort_inline.h` and
  `src/runtime/pgy_runtime_scalar_std_inline.h`, reducing
  `src/runtime/pgy_runtime_part_ba_part_c.inc` from 759 LOC to 535 LOC and
  source `.inc` total to 41,436 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile
  runtime-abi-lifetime-test-smoke runtime-panic-codegen-test-smoke test-abi`
  and targeted backend compare for `intent_header_interleaved`,
  `destructure_tuple_return`, `host_method_class_return`,
  `world_embedded_branch_projection_visibility`, `map_get_string`, `map_keys`,
  and `string_io`.
- Latest MIR ABI owner extraction moved ABI layout table/lookup into
  `src/compiler/mir_abi_layout.h`, reducing
  `src/compiler/mir_public_part_b.inc` from 753 LOC to 420 LOC and source
  `.inc` total to 41,103 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke air-drift-test-smoke test-abi`.
- Earlier CFG contract owner extraction moved cleanup/rollback/invalidation MIR
  validation into `src/compiler/mir_cfg_contract_validate.h`, reducing
  `src/compiler/mir_public_part_a.inc` from 743 LOC to 290 LOC and source
  `.inc` total to 40,650 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- Latest MIR CFG validator split moved cleanup/rollback/invalidation contract
  checks into `src/compiler/mir_cfg_contract_validate_cleanup.c`, leaving
  `src/compiler/mir_cfg_contract_validate.c` focused on non-cleanup CFG shape,
  source, fallback, loop, and unreachable-edge validation. Current sizes are
  334 LOC and 245 LOC. Verified by `make test-mir
  cfg-body-dataflow-test-smoke build-source-inventory-test-smoke
  test-inc-size-test-smoke abi-ownership-shape-test-smoke`.
- Latest RIR validation owner extraction moved `rir_validate`,
  `rir_validate_against_dir`, and projection-kind validation helpers into
  `src/compiler/rir_validation.h`, reducing `src/compiler/rir_public.inc` from
  741 LOC to 269 LOC and source `.inc` total to 40,178 LOC. Verified by
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- Latest C backend MIR intent inventory cleanup moved the former
  `src/codegen/transpiler_emitters_mir_inventory_intent.inc` body into
  `src/codegen/transpiler_mir_inventory_intent.h` and made the existing SSA
  shim include that owner header directly. This removes one production `.inc`
  body and reduces source `.inc` total to 39,485 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- Latest C backend call/spawn/channel emitter cleanup moved the former
  `src/codegen/transpiler_expr_emitters_part_e.inc` body into
  `src/codegen/transpiler_expr_call_spawn_emit.h` and made the expression
  emitter shim include that owner header directly. This removes another
  production `.inc` body and reduces source `.inc` total to 38,763 LOC.
  Verified by `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke type-resolution-dag-test-smoke
  cfg-body-dataflow-test-smoke air-drift-test-smoke test-abi`.
- Latest LLVM domain helper cleanup moved the former
  `src/codegen/llvm_domain_helpers_part_a.inc` body into
  `src/codegen/llvm_domain_core_helpers.h` and made `llvm_domain.c` include the
  owner header directly. This removes another production `.inc` body and
  reduces source `.inc` total to 38,041 LOC. Verified by `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- Latest runtime channel/qubit export cleanup moved the former
  `src/runtime/pgy_runtime_lib_part_b_part_e.inc` body into
  `src/runtime/pgy_runtime_lib_channel_quantum_exports.h` and made
  `pgy_runtime_lib.c` include the owner header directly. The runtime ABI
  lifetime smoke now reads the new owner header as the split continuation for
  generated-runtime checks. This removes another production `.inc` body and
  reduces source `.inc` total to 37,327 LOC. Verified by `make
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke test-abi`.
- Latest runtime raw collection and slot/array/io/string export cleanup moved
  the former `src/runtime/pgy_runtime_lib_part_b_part_a.inc` body into
  `src/runtime/pgy_runtime_lib_raw_collection_exports.h` and the former
  `src/runtime/pgy_runtime_lib_part_b_part_d.inc` body into
  `src/runtime/pgy_runtime_lib_slot_array_io_string_exports.h`. Runtime panic
  and ABI lifetime smokes now read the new owner headers, and compiler runtime
  cache freshness tracks them directly. This removes two more production
  `.inc` bodies and reduces source `.inc` total to 35,901 LOC. Verified by
  `make backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-contract-test-smoke
  runtime-panic-codegen-test-smoke test-abi`.
- Latest C backend builtin-call dispatch cleanup moved the former
  `src/codegen/transpiler_expr_emitters_part_b.inc` body into
  `src/codegen/transpiler_expr_builtin_dispatch.h` and made the expression
  emitter shim include the owner header directly. This removes another
  production `.inc` body, leaves builtin-call lowering in the original include
  order, and reduces the current source `.inc` inventory to 102 files / 35,191
  LOC. Verified by `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke type-resolution-dag-test-smoke
  cfg-body-dataflow-test-smoke air-drift-test-smoke test-abi`.
- Latest semantic builtin-query cleanup moved the former
  `src/semantic/type_checker_builtins_query.inc` body into
  `src/semantic/type_checker_builtins_query.h` and fixed the chained
  `BuiltinKind builtin_resolve(...)` signature so
  `type_checker_builtins_slotops.inc` owns a complete function boundary. This
  removes another production `.inc` body and reduces the current source `.inc`
  inventory to 101 files / 34,490 LOC.
- Latest semantic builtin-nominal cleanup moved the former
  `src/semantic/type_checker_builtins_nominal.inc` body into
  `src/semantic/type_checker_builtins_nominal.h`. This keeps the
  `Rc`/`Weak`/`Box`/allocator and intent-observability builtin type contract in
  the original dispatch order while removing another production `.inc` body;
  the current source `.inc` inventory is now 100 files / 33,809 LOC.
- Latest generated-C runtime pool/FSM/timer cleanup moved object-pool,
  finite-state-machine, timer, and cooldown inline helpers into
  `src/runtime/pgy_runtime_pool_fsm_timer_inline.h`. Runtime include order,
  ABI lifetime inventory, and compiler runtime-cache freshness now track that
  owner header directly; `pgy_runtime_part_ba_part_e.inc` is reduced to
  parallel/zone authority/effect-pool/unsafe/result/option helpers and the
  current source `.inc` total is 33,653 LOC.
- Latest semantic expression owner cleanup moved the former
  `src/semantic/type_checker_expr.inc` body into
  `src/semantic/type_checker_expr.h`. The CFG body-dataflow smoke now points at
  the new expression owner, and the current source `.inc` inventory is 99 files
  / 32,983 LOC.
- Latest C backend function/class/control-flow owner cleanup moved the former
  `src/codegen/transpiler_emitters_base_b_part_b.inc` body into
  `src/codegen/transpiler_func_class_flow_emit.h`. This preserves the existing
  base-B include order while removing another production `.inc` body; the
  current source `.inc` inventory is 98 files / 32,322 LOC.
- Latest generated-C runtime memory/array/slot cleanup moved the former
  `src/runtime/pgy_runtime_part_ba_part_b.inc` body into
  `src/runtime/pgy_runtime_memory_array_slot_inline.h`. Runtime include order,
  panic-contract smoke, ABI lifetime inventory, and compiler runtime-cache
  freshness now track that owner header directly; the current source `.inc`
  inventory is 97 files / 31,662 LOC.
- Latest semantic relation/effect/projection helper cleanup moved the former
  `src/semantic/type_checker_helpers_effects.inc` body into
  `src/semantic/type_checker_helpers_effects.h`. CFG body-dataflow smoke now
  tracks the new helper owner path, and the current source `.inc` inventory is
  96 files / 31,013 LOC.
- Latest C backend MIR emission contract cleanup moved the former
  `src/codegen/transpiler_emitters_base_a_part_d.inc` body into
  `src/codegen/transpiler_mir_emission_contract.h`. The base-A shim still
  preserves include order, but the remaining MIR emission/resource-hook owner
  is no longer an anonymous part file; the current production source `.inc`
  inventory is 95 files / 30,368 LOC.
- Latest RIR lowering/enrichment cleanup moved the former
  `src/compiler/rir_builder.inc` body into `src/compiler/rir_builder.h`.
  `rir.c` still includes it in the same position between flow and name/validation
  owners, but RIR construction is no longer an anonymous include body; the
  current production source `.inc` inventory is 94 files / 29,733 LOC.
- Latest semantic function-body/program owner cleanup moved the former
  `src/semantic/type_checker_program.inc` body into
  `src/semantic/type_checker_program.h`. The top-level semantic TU still
  includes it after helper/orchestration definitions, but function body
  checking is no longer carried by an anonymous `.inc`; the current production
  source `.inc` inventory is 93 files / 29,099 LOC.
- Latest LLVM method-call domain/slice cleanup moved the former
  `src/codegen/llvm_expr_call_methods_part_a.inc` body into
  `src/codegen/llvm_expr_call_methods_domain_slice.h`. `llvm_expr.c` still
  includes it before the remaining method-call tail, but domain action sync and
  slice/member-call helpers are no longer anonymous part-A code; the current
  production source `.inc` inventory is 92 files / 28,467 LOC.
- Latest LLVM call dispatcher cleanup moved the former
  `src/codegen/llvm_expr_calls_main.inc` body into
  `src/codegen/llvm_expr_call_dispatch.h`. The call-family shim still includes
  constructor/collection/domain/event/log/slot/task helpers before the final
  dispatcher, but `llvm_emit_call` now has a named owner; the current production
  source `.inc` inventory is 91 files / 27,842 LOC.
- Latest LLVM expression helper cleanup moved the former
  `src/codegen/llvm_expr_helpers_part_b.inc` body into
  `src/codegen/llvm_expr_host_spawn_literal_helpers.h`. Host/self, projection
  binding, spawn expression, operator suffix, enum lookup, and number/string
  literal helpers now have a named owner; the current production source `.inc`
  inventory is 90 files / 27,221 LOC.
- Latest C backend role/ability cleanup moved the former
  `src/codegen/transpiler_domain_role_part_a.inc` body into
  `src/codegen/transpiler_domain_role_ability_emit.h`. Role method emission,
  ability/vtable emission, hidden provenance helpers, and operator aliases now
  have a named owner while the domain-role shim preserves include order; the
  current production source `.inc` inventory is 89 files / 26,601 LOC.
- Latest LLVM expression boundary/projection helper cleanup moved the former
  `src/codegen/llvm_expr_helpers_part_a.inc` body into
  `src/codegen/llvm_expr_boundary_projection_helpers.h`. Boundary call argument
  helpers, projection field helpers, world/zone lookup helpers, and host-class
  lookup helpers now have a named owner; the current production source `.inc`
  inventory is 88 files / 25,996 LOC.
- Latest C backend MIR SSA naming cleanup moved the former
  `src/codegen/transpiler_emitters_mir_inventory_ssa_names.inc` body into
  `src/codegen/transpiler_mir_ssa_names.h`. MIR routine lookup, active SSA name
  resolution/rendering, token-local filtering, and local type-name lookup now
  have a named owner; the current production source `.inc` inventory is
  87 files / 25,395 LOC.
- Latest C backend type mapping cleanup moved the former
  `src/codegen/transpiler_helpers_core_types.inc` body into
  `src/codegen/transpiler_type_mapping_helpers.h`. Primitive, slot/channel,
  constructed generic, and local type-name rendering now have a named owner
  while the helper-core shim preserves include order; the current production
  source `.inc` inventory is 86 files / 24,796 LOC.
- Latest C backend world/select/event cleanup moved the former
  `src/codegen/transpiler_domain_role_part_d.inc` body into
  `src/codegen/transpiler_world_select_event_emit.h`. World sync declaration,
  select lowering, and event declaration/subscription lowering now have a named
  owner while the domain-role shim preserves include order; the current
  production source `.inc` inventory is 85 files / 24,198 LOC.
- Latest LLVM expression assignment/member/projection cleanup moved the former
  `src/codegen/llvm_expr_values.inc` body into
  `src/codegen/llvm_expr_assignment_member_projection.h`. Member lvalue/member
  access, projection invalidation, embedded world projection assignment sync,
  and assignment emission now have a named owner while `llvm_expr.c` preserves
  include order; the current production source `.inc` inventory is 84 files /
  23,617 LOC.
- Latest LLVM-linkable runtime authority/file/path bootstrap cleanup moved the
  former `src/runtime/pgy_runtime_lib_part_a.inc` body into
  `src/runtime/pgy_runtime_lib_authority_file_core.h`. Runtime authority
  rejection state, checked arithmetic exports, panic invariant export, and
  file-path normalization helpers now have a named owner while
  `pgy_runtime_lib.c` preserves include order; the current production source
  `.inc` inventory is 83 files / 23,031 LOC.
- Latest LLVM-linkable runtime set/intent trace cleanup moved the former
  `src/runtime/pgy_runtime_lib_part_b_part_b.inc` body into
  `src/runtime/pgy_runtime_lib_set_intent_trace_exports.h`. Raw set tail
  exports, intent active/recent registry helpers, intent trace mutation, and
  MIR trace hooks now have a named owner while `pgy_runtime_lib.c` preserves
  include order; the current production source `.inc` inventory is 82 files /
  22,449 LOC.
- Latest RIR flow cleanup moved the former `src/compiler/rir_flow.inc` body
  into `src/compiler/rir_flow.h`. RIR flow semantic flags, state merge rules,
  and HIR CFG enrichment now have a named owner while `rir.c` preserves include
  order; the current production source `.inc` inventory is 81 files /
  21,877 LOC.
- Later C backend MIR SSA cleanup retired
  `src/codegen/transpiler_mir_ssa_emit.h` after moving MIR local type lookup,
  explicit binding registration, effective local type rendering, MIR function
  signature checks, SSA expression emission, phi copy emission, and exit-SSA
  lookup behind concrete owners. MIR inventory/SSA consumers now include the
  owners they use directly instead of preserving a compatibility shim.
- Latest generated-C channel runtime cleanup moved the former
  `src/runtime/pgy_runtime_part_bb.inc` body into
  `src/runtime/pgy_runtime_channel_inline.h`. Threaded channel and SPSC channel
  inline macro definitions plus stable `Int`/`String` instantiations now have a
  named owner while `pgy_runtime.h` preserves include order; the current
  production source `.inc` inventory is 79 files / 20,752 LOC.
- Latest C backend zone declaration cleanup promoted the zone declaration body
  from `src/codegen/transpiler_zone_decl_emit.h` into the compiled owner
  `src/codegen/transpiler_zone_decl_emit.c`. Zone sync, projection
  readiness/dirty fields, layer/state frontier sync, bounded recompute, and the
  MIR hosted-method metadata guard now live outside the header; the header is an
  8 LOC declaration-only seam. Hosted zone method body emission still bridges
  through the existing `transpiler.c` include-order chain, leaving that smaller
  helper-chain extraction for a later slice.
- Latest C backend block/intent helper cleanup moved the former
  `src/codegen/transpiler_emitters_base_b_part_c.inc` body into
  `src/codegen/transpiler_block_intent_helpers.c`. Block auto-release emission,
  intent participant/action lookup, inferred causes lookup, and effective-zone
  sync helpers now have a named owner while the base-B shim preserves include
  order; the current production source `.inc` inventory is 77 files /
  19,652 LOC.
- Latest C backend intent cleanup moved cleanup/rollback/invalidation
  tail emission from `src/codegen/transpiler_intent_cleanup_emit.h` into
  `src/codegen/transpiler_intent_cleanup_emit.c`. The header is now
  declaration-only, and MIR carrier-missing diagnostics stay on the shared
  `transpiler_set_mir_intent_carrier_missing(...)` path.
- Latest C backend intent prologue cleanup moved signature/runtime-entry
  emission from `src/codegen/transpiler_intent_prologue_emit.h` into
  `src/codegen/transpiler_intent_prologue_emit.c`. The header is now
  declaration-only.
- Latest generated-C IO/Qubit runtime cleanup moved the former
  `src/runtime/pgy_runtime_part_c.inc` body into
  `src/runtime/pgy_runtime_io_qubit_inline.h`. Inline file/string helpers,
  `StringSplit` allocation, and the toy Qubit runtime now have a named owner
  while `pgy_runtime.h` preserves include order; compiler runtime cache
  freshness dependencies were also updated to stop pointing at deleted runtime
  include paths. The current production source `.inc` inventory is 76 files /
  19,110 LOC.
- Latest C backend constructor/Result-Option call cleanup moved the former
  `src/codegen/transpiler_expr_emitters_part_c.inc` body into
  `src/codegen/transpiler_call_constructor_result_emit.h`. Domain/party
  constructor lowering and Result/Option builtin call lowering now have a named
  owner while the expression emitter shim preserves include order; the current
  production source `.inc` inventory is 75 files / 18,573 LOC.
- Latest generated-C builtin storage cleanup moved the former
  `src/runtime/pgy_runtime_part_ba_part_c.inc` body into
  `src/runtime/pgy_runtime_builtin_storage_inline.h`. Slot/device-slot/
  secure-slot instantiations, Box/Array/Rc builtins, and inline HashMap helpers
  now have a named owner while `pgy_runtime_part_ba.inc` preserves include
  order; compiler cache freshness and runtime panic/ABI smoke tests now read
  the new owner path. The current production source `.inc` inventory is
  74 files / 18,038 LOC.
- Latest semantic host-helper cleanup moved the former
  `src/semantic/type_checker_helpers_host.inc` body into
  `src/semantic/type_checker_host_helpers.h`. Overlay field lookup, host method
  call typing, subject/nominal boundary classification, zone effect-layer
  checks, and movable resource predicates now have a named owner while
  `type_checker.c` preserves include order; the current production source
  `.inc` inventory is 73 files / 17,448 LOC.
- Latest semantic builtin slotops cleanup moved the former
  `src/semantic/type_checker_builtins_slotops.inc` body into
  `src/semantic/type_checker_builtins_slotops.h`. Builtin name resolution,
  Slot/SecureSlot/DeviceSlot semantic validation, release diagnostics, and
  device handle argument checks now have a named owner while
  `type_checker_builtins.c` preserves include order; the current production
  source `.inc` inventory is 72 files / 16,923 LOC.
- Latest generated-C zone/result-option runtime cleanup moved the former
  `src/runtime/pgy_runtime_part_ba_part_e.inc` body into
  `src/runtime/pgy_runtime_zone_result_option_inline.h`. Parallel section
  macros, zone lock/generation/authority validation, Result helpers, remote
  Result helpers, and Option helpers now have a named owner while
  `pgy_runtime_part_ba.inc` preserves include order; compiler cache freshness
  and runtime ABI/panic/authority smoke tests now read the new owner path. The
  current production source `.inc` inventory is 71 files / 16,402 LOC.
- Latest C backend projection/sync cleanup moved the former
  `src/codegen/transpiler_helpers_core_a_part_c.inc` body through the
  projection-sync seam and later promoted zone/world projection sync emission
  into `src/codegen/transpiler_projection_sync.c`. The
  `transpiler_projection_sync.h` header is declaration-only; overlay
  projection invalidation scanning and world-state lookup remain separate
  staged seams while the projection-sync action/effect owner is linked through
  the Makefile inventory.
- Latest generated-C intent trace runtime cleanup moved the former
  `src/runtime/pgy_runtime_part_ba_part_a.inc` body into
  `src/runtime/pgy_runtime_intent_trace_inline.h`. `pgy_runtime_strdup`,
  active/recent intent registry storage, trace append helpers, step ok/fail
  tracing, and MIR resource trace hooks now have a named owner while
  `pgy_runtime_part_ba.inc` preserves include order; compiler cache freshness
  and runtime ABI lifetime smoke now read the new owner path. The current
  production source `.inc` inventory is 69 files / 15,370 LOC.
- Latest LLVM collection-call cleanup moved the former
  `src/codegen/llvm_expr_call_collections_extended.inc` body into
  `src/codegen/llvm_expr_call_collections_extended.h`. Extended List/Set/
  HashMap raw-call lowering now has a named private owner while
  `llvm_expr_calls.inc` preserves dispatcher include order. The current
  production source `.inc` inventory is 68 files / 14,862 LOC.
- Latest C backend helper-root cleanup moved the former
  `src/codegen/transpiler_helpers.inc` body into
  `src/codegen/transpiler_helpers.h`. C string escaping/formatting, MIR
  resource-op/DEF helper emission, and the expression-emitter include root now
  have a named private owner while `transpiler.c` preserves top-level include
  order. The current production source `.inc` inventory is 67 files /
  14,356 LOC.
- Latest C backend expression-core cleanup moved the former
  `src/codegen/transpiler_expr_emitters_part_a.inc` body into
  `src/codegen/transpiler_expr_core_emit.h`. Log/LogRaw/LogBanner lowering and
  core binary expression lowering now have a named private owner while
  `transpiler_expr_emitters.inc` preserves include order. The current
  production source `.inc` inventory is 66 files / 13,869 LOC.
- Latest C backend specialization-registry cleanup moved the former
  `src/codegen/transpiler_helpers_core_b_part_b.inc` body into
  a responsibility-named specialization registry owner. Role ability/method
  lookup and Result/Option/collection specialization collection now had a
  named private owner while `transpiler_helpers_core_b.inc` preserved include
  order. The current production source `.inc` inventory was 65 files /
  13,402 LOC.
- Latest C backend domain nominal cleanup moved the former
  `src/codegen/transpiler_domain_role_part_b.inc` body into
  `src/codegen/transpiler_domain_nominal_emit.h`. Ability, role, party, roster,
  relation, and effect declaration emission now have a named private owner
  while `transpiler_domain_role.inc` preserves include order. The current
  production source `.inc` inventory is 64 files / 12,937 LOC.
- Latest C backend expression-dispatch cleanup moved the former
  `src/codegen/transpiler_expr_emitters_part_f.inc` body into
  `src/codegen/transpiler_expr_dispatch_emit.h`. The `emit_expression()`
  dispatcher now has a named private owner while
  `transpiler_expr_emitters.inc` preserves include order; runtime panic
  contract smoke now reads the new owner path for checked array/slice lowering.
  The current production source `.inc` inventory is 63 files / 12,486 LOC.
- Latest MIR public-surface cleanup moved the former
  `src/compiler/mir_public_part_b.inc` body into
  `src/compiler/mir_public_surface.h`. MIR kind names, destroy, validation,
  emission-topology validation, and dump now have a named private owner while
  `mir.c` preserves include order. The current production source `.inc`
  inventory is 62 files / 12,066 LOC.
- Latest semantic generic-contract cleanup moved the former
  `src/semantic/type_checker_generic_contracts.inc` body into
  `src/semantic/type_checker_generic_contracts.c`. Generic parameter lookup,
  default-bound validation, and class-specialization where-bound validation now
  have a compiled owner instead of relying on `type_checker.c` include order.
  The historical production source `.inc` inventory at that extraction point
  was 61 files / 11,663 LOC.
- Latest LLVM member-call cleanup moved the former
  `src/codegen/llvm_expr_call_methods_part_b.inc` body into
  `src/codegen/llvm_member_call_emit.h`. `llvm_emit_member_call()` and nominal
  hosted-method dispatch now have a named private owner while `llvm_expr.c`
  preserves include order. The current production source `.inc` inventory is 60
  files / 11,262 LOC.
- Latest generated-C runtime root cleanup moved the former
  `src/runtime/pgy_runtime_part_a.inc` body into
  `src/runtime/pgy_runtime_platform_io_core.h`. Platform includes, contract
  headers, warning helpers, path normalization, and IO sandbox checks now have a
  named private owner while `pgy_runtime.h` preserves include order. The current
  production source `.inc` inventory is 59 files / 10,879 LOC.
- Latest semantic resolution-helper cleanup moved the former
  `src/semantic/type_checker_helpers_resolution.inc` body into
  `src/semantic/type_checker_resolution_helpers.h`. Alias resolution stack
  handling, alias materialization, function-type formatting, and embedded
  world-zone mutation rejection now have a named private owner while
  `type_checker.c` preserves include order. The current production source
  `.inc` inventory is 58 files / 10,500 LOC.
- Latest generated-C collection-runtime cleanup moved the former
  `src/runtime/pgy_runtime_part_ba_part_d.inc` body into
  `src/runtime/pgy_runtime_list_set_inline.h`. List and Set inline runtime
  definitions now have a named private owner while `pgy_runtime_part_ba.inc`
  preserves include order. The current production source `.inc` inventory is 57
  files / 10,123 LOC.
- Latest LLVM identifier/slot-helper cleanup moved the former
  `src/codegen/llvm_expr_helpers_part_c.inc` body into
  `src/codegen/llvm_expr_identifier_slot_helpers.h`. Identifier emission,
  direct Slot/SecureSlot fallbacks, slot target resolution, and banner literal
  normalization now have a named private owner while `llvm_expr.c` preserves
  include order. The current production source `.inc` inventory is 56 files /
  9,753 LOC.
- Latest semantic async/channel cleanup moved the former
  `src/semantic/type_checker_async_channel.inc` body into
  a named owner, and that owner has since been promoted from the temporary
  `src/semantic/type_checker_async_channel.h` implementation header into
  `src/semantic/type_checker_async_channel.c`. Spawn token boundary checks and
  channel send/recv ownership diagnostics now compile independently instead of
  depending on `type_checker.c` include order. The historical production source
  `.inc` inventory at this step was 55 files / 9,384 LOC.
- Latest LLVM scalar-expression cleanup moved the former
  `src/codegen/llvm_expr_core.inc` body into
  `src/codegen/llvm_expr_scalar_core.h`. Callable/event signature helpers,
  scalar string coercion, binary lowering, unary lowering, and `?` propagation
  lowering now have a named private owner while `llvm_expr.c` preserves include
  order. The current production source `.inc` inventory is 54 files / 9,024
  LOC.
- Latest semantic generic-support cleanup moved the former
  `src/semantic/type_checker_generic_support.inc` body into
  `src/semantic/type_checker_generic_support.c`. Generic subject signature
  formatting and effective default generic argument derivation now have a
  compiled owner instead of relying on `type_checker.c` include order. The
  historical production source `.inc` inventory at that extraction point was
  53 files / 8,666 LOC.
- Latest LLVM domain projection-sync cleanup moved the former
  `src/codegen/llvm_domain_helpers_part_b.inc` body into
  `src/codegen/llvm_domain_projection_sync_helpers.h`. Projection field-copy
  lowering and bounded projection sync loop generation now have a named private
  owner while `llvm_domain.c` preserves include order. The current production
  source `.inc` inventory is 52 files / 8,333 LOC.
- Latest C backend async/parallel cleanup moved the former
  `src/codegen/transpiler_emitters_async_parallel.inc` body into
  `src/codegen/transpiler_async_parallel_emit.h`. Parallel block emission and
  async block spawning now have a named private owner while
  `transpiler_func_class_flow_emit.h` preserves include order. The current
  production source `.inc` inventory is 51 files / 8,016 LOC.
- Latest semantic type resolver cleanup moved the former
  `src/semantic/type_checker_resolve.inc` body into
  `src/semantic/type_checker_resolve.c`. The memoized `resolve_type_node(...)`
  wrapper is now TU-local, the obsolete `type_checker_resolve.h` compatibility
  header is deleted, and assignment compatibility / constructed-type helpers
  stay in the named private owner while metadata-first public APIs replace
  direct resolver entry. The current production source `.inc` inventory is
  50 files / 7,701 LOC.
- Latest semantic domain-query builtin cleanup moved the former
  `src/semantic/type_checker_builtins_query_domain.inc` body into
  `src/semantic/type_checker_builtins_query_domain.h`. HasProjection/
  HasZoneProjection source-field lookup, zone/world slot lookup, and domain
  projection query validation now have a named private owner while
  `type_checker_builtins.c` preserves include order. The current production
  source `.inc` inventory is 49 files / 7,387 LOC.
- Latest CFG/body-flow loop cleanup moved the former
  `src/semantic/type_checker_flow_loops.inc` body into
  `src/semantic/type_checker_flow_loops.h`. Loop resource snapshot comparison,
  bounded loop analysis, and loop effect merge logic now have a named private
  owner while `type_checker_flow.c` preserves include order. The CFG/body
  dataflow smoke now reads the named owner path. The current production source
  `.inc` inventory is 48 files / 7,086 LOC.
- Latest C backend function-forward helper cleanup moved the former
  `src/codegen/transpiler_helpers_core_b_part_c.inc` body into
  `src/codegen/transpiler_func_forward_helpers.h`. Spawn/future return type
  inference, early type forward-declaration checks, generic call binding
  inference, and hosted-method forward declarations now have a named private
  owner while `transpiler_helpers_core_b.inc` preserves include order. The
  current production source `.inc` inventory is 47 files / 6,790 LOC.
- Latest MIR lowering public API cleanup moved the former
  `src/compiler/mir_public_part_a.inc` body into
  `src/compiler/mir_lower_public_api.h`. `mir_lower(...)`, MIR routine lookup,
  declaration header lookup, liveness pass entry, and DCE pass entry now have a
  named private owner while `mir.c` preserves include order. MIR declaration
  inventory smoke now reads the named owner path. The current production source
  `.inc` inventory is 46 files / 6,500 LOC.
- Latest LLVM-linkable runtime intent/slot-core export cleanup moved the former
  `src/runtime/pgy_runtime_lib_part_b_part_c.inc` body into
  `src/runtime/pgy_runtime_lib_intent_slot_core_exports.h`.
  `pgy_intent_exit_export(...)`, the runtime deadline helper, and primitive
  `Slot<Int/Long/Float>` exports now have a named private owner while
  `pgy_runtime_lib.c` preserves include order. Runtime ABI lifetime and panic
  contract smokes now read the named owner path. The current production source
  `.inc` inventory is 45 files / 6,212 LOC.
- Latest C backend match lowering cleanup moved the former
  `src/codegen/transpiler_emitters_match.inc` body through
  `src/codegen/transpiler_match_emit.h` and now into the compiled owner
  `src/codegen/transpiler_match_emit.c`. Result/Option/enum destructor pattern
  helpers and `emit_match_stmt(...)` no longer live in the include chain.
  The current production source `.inc` inventory is 44 files / 5,932 LOC.
- Latest LLVM domain query call cleanup moved the former
  `src/codegen/llvm_expr_call_domain_queries.inc` body into
  `src/codegen/llvm_expr_domain_query_calls.h`. `HasProjection`, `HasLayer`,
  `HasState`, `HasZone`, and zone-detail query lowering now have a named
  private owner while `llvm_expr_calls.inc` preserves include order. The current
  production source `.inc` inventory is 43 files / 5,657 LOC.
- Latest MIR/RIR owner cleanup moved the former `src/compiler/mir_base.inc`
  body into `src/compiler/mir_base_helpers.h` and the former
  `src/compiler/rir_public.inc` body into `src/compiler/rir_public_surface.h`.
  MIR low-level helpers and RIR dump/destroy surfaces now have explicit owners
  while preserving the existing include order. The current production source
  `.inc` inventory is 41 files / 5,119 LOC before the next runtime/codegen
  cleanup slice.
- Latest runtime quantum export cleanup moved the former
  `src/runtime/pgy_runtime_lib_part_b_part_f.inc` body into
  `src/runtime/pgy_runtime_lib_quantum_exports.h`. Runtime source packaging,
  LLVM runtime library include order, and ABI lifetime smoke now refer to the
  named quantum export owner. The current production source `.inc` inventory is
  40 files / 4,866 LOC.
- Latest LLVM slot/device call cleanup moved the former
  `src/codegen/llvm_expr_call_slots.inc` body into
  `src/codegen/llvm_expr_slot_device_calls.h`. `ClaimSlot`, `Write`, `Read`,
  `Release`, and `Device*` lowering now have a named owner while
  `llvm_expr_calls.inc` remains the dispatcher-order shim. The current
  production source `.inc` inventory is 39 files / 4,621 LOC.
- Latest C/LLVM call-owner cleanup moved the former
  `src/codegen/transpiler_emitters_base_b_part_d.inc` body into
  `src/codegen/transpiler_intent_zone_binding_emit.c`, the former
  `src/codegen/llvm_expr_call_constructors.inc` body into
  `src/codegen/llvm_expr_constructor_calls.h`, the former
  `src/codegen/llvm_expr_call_rc.inc` body into
  `src/codegen/llvm_expr_rc_calls.h`, and the former
  `src/codegen/llvm_expr_call_task_channel.inc` body into
  `src/codegen/llvm_expr_task_channel_calls.h`. The current production source
  `.inc` inventory is 35 files / 3,689 LOC.
- Latest flow/resource and LLVM collection/result cleanup moved
  `src/codegen/transpiler_emitters_control_flow_loops.inc` into
  `src/codegen/transpiler_control_flow_emit.h`,
  `src/semantic/type_checker_flow_resources.inc` into
  `src/semantic/type_checker_flow_resources.h`,
  `src/codegen/llvm_expr_call_collections_base.inc` into
  `src/codegen/llvm_expr_collection_base_calls.h`, and
  `src/codegen/llvm_expr_call_result_option.inc` into
  `src/codegen/llvm_expr_result_option_calls.h`. The current production source
  `.inc` inventory is 31 files / 2,814 LOC.
- Latest semantic/LLVM MIR owner cleanup moved
  `src/semantic/type_checker_builtins_query_channel.inc` into
  `src/semantic/type_checker_builtins_query_channel.h`,
  `src/semantic/type_checker_operator_expr.inc` into
  `src/semantic/type_checker_operator_expr.h`,
  `src/codegen/llvm_mir_locals.inc` into
  `src/codegen/llvm_mir_local_emit.h`,
  `src/codegen/llvm_mir_blocks.inc` into
  `src/codegen/llvm_mir_block_emit.h`,
  `src/semantic/type_checker_resolution_graph_core.inc` into
  `src/semantic/type_checker_resolution_graph_core.c`, and
  `src/codegen/transpiler_emitters_enum_decl.inc` into
  `src/codegen/transpiler_enum_decl_emit.h`. The current production source
  `.inc` inventory is 25 files / 1,675 LOC.
- Latest helper/call owner cleanup moved
  `src/semantic/type_checker_helpers_context.inc` into
  `src/semantic/type_checker_context_helpers.h` (later promoted to
  `src/semantic/type_checker_context_helpers.c`),
  `src/codegen/llvm_expr_call_log.inc` into
  `src/codegen/llvm_expr_log_calls.h`,
  `src/codegen/llvm_expr_call_arrays.inc` into
  `src/codegen/llvm_expr_array_calls.h`,
  `src/codegen/transpiler_emitters_base_b_part_a.inc` into
  `src/codegen/transpiler_mir_emit_state.h`,
  `src/codegen/transpiler_emitters_base_a_part_a.inc` into
  `src/codegen/transpiler_mir_emit_decls.h`, and
  `src/codegen/transpiler_emitters_base_a_part_b.inc` into
  `src/codegen/transpiler_mir_pending_uses.h`. The current production source
  `.inc` inventory is 19 files / 835 LOC.
- Latest formatter/flow/observability cleanup moved `src/compiler/fmt_layout.inc`
  into `src/compiler/fmt_layout.h`, `src/compiler/fmt_io.inc` into
  `src/compiler/fmt_io.h`, `src/semantic/type_checker_flow_effects.inc` into
  `src/semantic/type_checker_flow_effects.h`,
  `src/semantic/type_checker_flow_parallel.inc` into the later compiled owner
  `src/semantic/type_checker_flow_parallel.c`,
  `src/codegen/llvm_expr_call_intent_observability.inc` into
  `src/codegen/llvm_expr_intent_observability_calls.h`, and
  `src/semantic/type_checker_assignment.inc` into
  `src/semantic/type_checker_assignment.h`. The current production source `.inc`
  inventory is 13 files / 297 LOC.
- Latest LLVM domain event cleanup moved event type/helper lowering into
  `src/codegen/llvm_domain_event.c` and declared the seam in
  `src/codegen/llvm_domain_event.h`. `llvm_domain.c` now delegates event
  generation instead of carrying the full helper body, and the event handler
  parameter type materialization uses the full event arity instead of the old
  8-entry local array. `src/codegen/llvm_domain.c` is now 1,356 LOC and
  `llvm_domain_event.c` is 322 LOC. Verified by `make LLVM_ENABLED=1
  /tmp/pgy-PergyraLang-bin/pgy llvm-test-backend-compare` with 196 ABI checks
  and backend compare 53/53 green.
- Latest LLVM domain role emission cleanup moved role method body emission,
  role operator thunk emission, and role vtable global materialization into
  `src/codegen/llvm_domain_role_emit.c` with the seam declared in
  `src/codegen/llvm_domain_role_emit.h`. `src/codegen/llvm_domain.c` now
  carries the remaining domain declaration/type/sync orchestration at 1,125
  LOC. Verified by `make LLVM_ENABLED=1 /tmp/pgy-PergyraLang-bin/pgy
  llvm-test-backend-compare` with 196 ABI checks and backend compare 53/53
  green.
- Latest LLVM domain method/sync cleanup moved domain sync helper dispatch and
  domain method body emission into `src/codegen/llvm_domain_method_emit.c`,
  declared by `src/codegen/llvm_domain_method_emit.h`. This keeps projection
  sync helper include-order local to the new owner TU and reduces
  `src/codegen/llvm_domain.c` to 895 LOC. Verified warning-clean by `make
  LLVM_ENABLED=1 /tmp/pgy-PergyraLang-bin/pgy llvm-test-backend-compare` with
  196 ABI checks and backend compare 53/53 green.
- Latest overall audit also reran `make tooling-conformance-test-smoke`; the
  formatter smoke is invoked through `bash`, so Linux execute-bit drift no
  longer blocks the tooling conformance gate.
- `test-abi`: ABI spec 49 passed, 0 failed, plus C/LLVM ABI pipeline smoke.
- `runtime-abi-lifetime-test-smoke`: passed.
- `test-inc-size-test-smoke`: all `src/tests/**/*.cases.h` files are below
  990 LOC.
- `inc-sentinel-test-smoke`: no `.inc` files are allowed under `src`,
  `.cases.h` is allowed only under `src/tests`, the current `.cases.h`
  inventory is capped at 85 files, `.cases.h` includes are allowed only from
  dedicated test harnesses, no empty test fragments are allowed, and no orphan
  test fragments are allowed. The sentinel is shell-only and does not require a
  Python runtime on CI.
- `git diff --check`: passed; only line-ending warnings were reported.

## Remaining Include Debt

- There is no remaining `.inc` debt under `src`: production `.inc` inventory is
  zero and test fixtures use `.cases.h`, not `.inc`.
- The next structural cleanup queue is no longer `.inc` removal; it is
  600-plus production owner reduction. Current high-priority examples include
  `src/parser/ast.c`, `src/parser/ast_print.c`, `src/parser/ast.h`, and the
  900-plus
  split-review candidates such as `src/codegen/llvm_backend.c`,
  `src/codegen/llvm_domain_zone_sync.c`,
  `src/parser/parser_domain.c`,
  `src/semantic/type_checker_decls_domain_helpers.c`, and
  `src/runtime/slot_manager.c`. `air.c` is no longer
  in this queue after the boundary/dump/evidence/verify split, and
  `slot_manager.c` is now below the hard risk line after the secure-ops split.
  `driver_app.c` is also below the hard risk line after the scaffold owner
  split, and `compiler.c` is below the hard risk line after the host-toolchain
  split. `slot_analyzer.c` is below both the 1,000 LOC hard line and the 600
  LOC split-review threshold after the summary owner split. `hir.c` is below
  the hard line after the HIR public surface split. The driver/compiler/HIR
  owners are now below the 600 LOC split-review threshold. Future splits should
  only happen when a named responsibility seam appears, not by blind line-count
  sharding.
- The long-term target remains real `.c` / `.h` ownership for behavior-heavy
  families. The current state removes the worst include-order debt; new source
  `.inc` files are not allowed in the `src` tree.
- Ignored local backup files (`*.backup`) were removed from `src` so inventory
  scans, grep, and LOC reports do not count stale pre-split source snapshots.
- Runtime inline owners may live in private `.h` files when generated C must
  include the implementation directly. This is still preferable to behavior
  growth inside split `.inc` files, but those headers must keep ABI names and
  include order explicit.
- New `.inc` split files are not allowed by default. Raising the file-count cap
  must be treated as beta debt and justified in this ledger.
- C backend context and local symbol tracking now have real TU owners:
  `transpiler_context.c`, `transpiler_symbols.c`, and
  `transpiler_decl_lookup.c`, `transpiler_projection.c`,
  `transpiler_nominal.c`, `transpiler_enum.c`, `transpiler_operator.c`, and
  `transpiler_type_alias.c`, `transpiler_type_require.c`, and
  `transpiler_extern.c`, `transpiler_type_declarator.c`,
  `transpiler_log_normalize.c`, `transpiler_parallel_capture.h`,
  `transpiler_expr_builtin_dispatch.h`, and
  `transpiler_expr_stdlib_builtin.h`, `transpiler_overlay_projection.h`, and
  `transpiler_let_emit.h`, `transpiler_mir_block_emit.h`,
  `transpiler_intent_emit.c`, `pgy_runtime_intent_active_exports.h`, and
  `pgy_runtime_lib_intent_exports.h`, and
  `llvm_expr_call_projection_sync.h`, `transpiler_mir_ssa_contract.h`, and
  `transpiler_slot_builtin_emit.h`, `transpiler_type_mapping_helpers.h`,
  `transpiler_world_select_event_emit.h`, and
  `llvm_expr_assignment_member_projection.h`, plus
  `pgy_runtime_lib_authority_file_core.h` and
  `pgy_runtime_lib_set_intent_trace_exports.h`, plus `rir_flow.h`,
  `rir_flow_state.h`, `llvm_domain_world_sync.c`, and
  `llvm_domain_zone_sync.c`. The next
  high-value extraction candidate is not more blind line-count splitting; it is
  choosing a real owner seam for parser AST/domain owners, DIR declaration
  graph ownership, or the remaining zone/frontier bodies.
- Latest owner cleanup moved exported intent trace events into
  `src/runtime/pgy_runtime_lib_intent_trace_events_exports.c` and inline
  active-index helpers into `src/runtime/pgy_runtime_intent_active_index_inline.h`.
  The corresponding runtime cache and smoke gates now track those owners, and
  the current production C/H owner inventory has no file above 600 LOC. The
  remaining 560-plus review queue is `llvm_internal.h` and
  `llvm_expr_task_channel_calls.c`; neither should be split without a named
  responsibility seam.
- Latest slot-manager cleanup moved shared plain-payload storage/checksum/free
  helpers into `src/runtime/slot_manager_storage.c`. This is a responsibility
  split, not a blind line-count split: `slot_manager.c`, secure-slot flows, and
  pin views all share that storage seam through `slot_manager_internal.h`.
- Latest HIR cleanup moved routine-name indexing, direct-call edge
  materialization, and entry-reachability propagation into
  `src/compiler/hir_callgraph.c`. `src/compiler/hir.c` is now focused on
  top-level lowering orchestration and declaration classification.
- Latest CFG/RIR consumer cleanup moved the RIR resource-state merge lattice
  from `src/compiler/rir_flow.h` into `src/compiler/rir_flow_state.h`.
  `rir_flow.h` is now 420 LOC and owns HIR CFG enrichment / bounded dataflow
  iteration; `rir_flow_state.h` is 214 LOC and owns authority, projection,
  handoff, handle, and generic state merge semantics.
- Empty `.inc` tails are no longer allowed. A split-order shim may include only
  real implementation chunks; if a tail becomes empty, remove it and update the
  shim, dependency list, tests, and this ledger in the same change.
- Future splits must not move dangling return-type fragments across include
  boundaries. The build now enforces this through
  `-Werror=implicit-function-declaration` and `-Werror=implicit-int`.
- Latest AIR/CFG/DAG owner cleanup moved AIR drift storage to
  `src/compiler/air_drift.c`, boundary evidence shape validation to
  `src/compiler/air_validate_boundary_evidence.c`, CFG branch flow to
  `src/semantic/type_checker_flow_branch.c`, CFG parallel/defer flow to
  `src/semantic/type_checker_flow_parallel.c`, and type-resolution stats to
  `src/semantic/type_checker_program_stats.c`. `perf_contract_smoke.sh` was
  updated to follow these current owner seams and accessor-based payload reads
  instead of stale pre-split locations.
