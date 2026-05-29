# Beta Readiness Checklist - Execution Order And Progress Log

> Split from `docs/100_beta_readiness_checklist.md` on 2026-05-29.
> Keep active blocker edits in the shard that owns the relevant closure track.

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
- Verified with direct MinGW TU compiles and `mingw32-make test-semantic`
  (`2615/0`). A stale Git Bash/make child process briefly blocked the first
  clean-shell run, but the rerun completed after the process cleared.

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

## Progress Log — 2026-04-24 Parser/Lexer Diagnostic Routing

- `parser_error`와 lexer error token이 stage code, reason, fix를 갖도록 1차 routing gate를 닫았다.
- 새 코드: `PGY_PARSE_SYNTAX`, `PGY_LEX_INVALID_TOKEN`.
- 새 gate: `make parser-lexer-diagnostic-test-smoke`.
- CI 연결: `ci-linux`가 parser/lexer diagnostic gate를 실행한다.
- 남은 beta debt: parse/lex baseline message surface와 JSON diagnostic object routing은 닫혔다. 남은 것은 parser-specific code split과 multi-error accumulation이다.
