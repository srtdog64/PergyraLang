# Beta Readiness Checklist

마지막 업데이트: 2026-04-29

이 문서는 베타 진입 전 반드시 닫아야 하는 실행 체크리스트다. 기준은 기능 개수가 아니라 **surface trust + 구조 지속 가능성 + C/LLVM parity + CFG-backed body safety + AIR-backed abstraction safety**다. 현재 공식 beta readiness는 약 50%로 본다.

베타 진입 후 약 1년간 코어 패치를 멈추고 생태계 (`pgy.compat.*`, `pgy.kit.*`, `pgy.std.*`, `pgy.accel.spray`, `pgy.render.skia` 등) 를 올리는 것이 다음 단계이므로, 베타 closure 는 **"이 1년동안 코어가 자력으로 버틸 수 있는가"** 기준으로 본다. 이 의미에서 AIR/CFG/runtime invariant 는 모두 closure 직전까지 실 구현이 끝나야 하며, 단순 문서 합의로 끝나지 않는다.

Operational mode:

- 2026-04-30 AIR payload-containment update:
  AIR boundary walking and HIR containment now also descend through event
  subscribe/unsubscribe handler payloads, party-instance assignment values,
  party shared-field initializers, world roster/zone initializers, and
  domain-slot initializers. These carrier nodes are not new AIR boundary kinds;
  they only prevent existing IO/parallel/channel/execution boundaries from
  being hidden behind a payload container. Gate: `make test-air
  air-drift-test-smoke` (`41/0` AIR tests).
- 2026-04-30 AIR event execution boundary update:
  `AST_EVENT_SUBSCRIBE` and `AST_EVENT_UNSUBSCRIBE` are now AIR execution
  boundaries with `event-subscribe` / `event-unsubscribe` sources. The handler
  payload is still traversed, so an event subscription can produce both the
  outer execution boundary and nested IO/parallel/channel boundaries. This is
  a verification-layer change only; AIR remains absent from codegen IR.
- 2026-04-30 AIR evidence provenance tightening:
  `air_validate(...)` now rejects empty HIR routine, RIR boundary, and RIR
  authority evidence provenance names. Evidence flags must carry named proof
  provenance; boolean-only or empty-string evidence is treated as
  `PGY_AIR_INVARIANT_INVALID`. Gate: `make test-air air-drift-test-smoke`
  (`41/0` AIR tests).
- 2026-04-30 AIR 1.0 scope freeze:
  AIR is now documented as the 1.0 closure target for abstraction safety, not
  as a replacement for CFG, DAG, MIR, ownership, or runtime propagation. Beta
  keeps Phase 1 narrow (`IntentNode`, `BoundaryNode`, strict evidence, drift
  facts); 1.0 requires first-class `EvidenceNode`s that audit HIR CFG, DIR,
  RIR, MIR cleanup/pin, and DAG generic/ability/module facts without becoming a
  codegen IR.
- 2026-04-30 AIR evidence-node implementation step:
  `AIREvidenceNode` is now present in the AIR data model and dump output.
  HIR routine, HIR CFG, RIR boundary, and RIR authority evidence are recorded as
  provenance-carrying nodes while the legacy per-boundary flags remain as the
  current driver compatibility seam. `air_validate(...)` rejects malformed
  evidence-node inventory.
- 2026-04-30 AIR MIR pin-cleanup evidence step:
  `air_collect_mir_evidence(...)` records MIR-owned `pin-unpin-cleanup-edge`
  facts as `AIR_EVIDENCE_MIR_PIN_CLEANUP` nodes for matching AIR `pin`
  execution boundaries. This keeps MIR as the cleanup source of truth while
  giving AIR a provenance-carrying audit hook for 1.0 abstraction safety.
- 2026-04-29 DAG retired-audit label freeze:
  `PGY_TYPE_RES_STATS=1` now reports the removed recursive resolver counters as
  `retired-compatibility-resolver`, `retired-compatibility-resolver-kind`, and
  `retired-compatibility-cache`. This is a wording/contract tightening: the
  counters still gate `0` calls and `0` cache misses, but logs no longer make
  the removed resolver look like an active compatibility implementation.
- 2026-04-28 semantic owner update: function declaration and host-helper
  implementation-header debt is closed. `type_checker_func_decl.c`,
  `type_checker_func_action_contract.c`, and `type_checker_host_helpers.c`
  replace the old implementation bodies in `type_checker_program.h` /
  `type_checker_host_helpers.h`; the semantic shape gate tracks the new
  owners under the 600 LOC review threshold.
- 2026-04-28 LLVM owner update: `llvm_intent.c` and `llvm_domain.c` are now
  below the 600 LOC review threshold. Intent setup/context/cleanup ownership
  lives in `llvm_intent_setup.c`, `llvm_intent_step_context.c`, and
  `llvm_intent_cleanup.c`; domain forward declarations and struct-field
  helpers live in `llvm_domain_forward.c` and
  `llvm_domain_struct_fields.c`. Local gate: `make llvm-test-smoke`.
- 2026-04-28 C backend owner update: `transpiler.c` is now below the 600 LOC
  review threshold. Public entry/result lifecycle moved to
  `transpiler_entry.c`, runtime thread-pool requirement scanning moved to
  `transpiler_thread_pool.c`, and small declaration stubs moved to
  `transpiler_misc_decl.c`. Parity gate: `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `64/64` backend compare).
- 2026-04-28 C projection overlay owner update:
  `transpiler_overlay_projection.h` is now below the 600 LOC review
  threshold. Host-field/self-cell probes live in
  `transpiler_overlay_host_fields.h`, while zone relation/effect bind-layer
  emission lives in `transpiler_overlay_zone_bind.h`. Parity gate:
  `make llvm-test-backend-compare` (`196/0` ABI same-process, `64/64`
  backend compare).
- 2026-04-28 LLVM zone sync owner update:
  `llvm_domain_zone_sync.c` is now below the 600 LOC review threshold.
  Relation clause lowering (`link`, maintained relation, and `unlink`) lives
  in `llvm_domain_zone_sync_relations.c`, leaving zone sync orchestration and
  effect/state clause lowering in the main owner. Gates: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare` (`196/0`
  ABI same-process, `64/64` backend compare).
- 2026-04-28 LLVM world sync owner update:
  `llvm_domain_world_sync.c` is now below the 600 LOC review threshold.
  World command directive lowering and world state/zone-slot lookup helpers
  live in `llvm_domain_world_sync_directives.c` behind
  `llvm_domain_world_sync_internal.h`. Gates: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare` (`196/0`
  ABI same-process, `64/64` backend compare).
- 2026-04-29 LLVM world frontier owner update:
  bounded world frontier scheduling has a dedicated owner,
  `llvm_domain_world_frontier.c`. The main `llvm_domain_world_sync.c` now keeps
  sync orchestration only, while the frontier owner keeps
  `pgy_frontier_world_transitive_pass_limit(...)`, zone-generation dirty
  detection, derived-state recompute, and overflow abort emission. Gate:
  `make runtime-frontier-contract-test-smoke`; local sanity gate:
  `make llvm-test-smoke`.
- 2026-04-29 CFG/MIR correction:
  `parallel { ... }` is not classified as CFG-owned until HIR/MIR has a real
  parallel CFG lowering. It remains AIR-visible and semantic-flow checked, but
  MIR DCE must preserve it as a side-effecting statement. This prevents channel
  sends inside `parallel` from disappearing before LLVM select/channel tests.
  `make cfg-body-dataflow-test-smoke` now gates this distinction directly with
  a parallel-send/select MIR preservation fixture.
- 2026-04-29 CFG loop fixed-point correction:
  resource snapshot equality now compares `used_states` in addition to
  consumed/released state. Loop convergence can no longer ignore borrow/use
  facts while still treating ownership facts as stable. Gate:
  `make cfg-body-dataflow-test-smoke`.
- 2026-04-29 C MIR parallel residual emission correction:
  resource hooks for `parallel`/`async`/`spawn`/`await` are treated as
  observability hooks only; they do not mirror or replace the executable
  residual statement. This fixes the C backend `parallel_channel_sum` hang
  where receives were emitted without the send task body. Backend compare also
  has a generated-executable timeout guard via
  `PGY_BACKEND_COMPARE_RUN_TIMEOUT_SECONDS`. Gate:
  `make llvm-test-backend-compare` (`196/0` ABI same-process, `65/65`
  backend compare).
- 2026-04-29 CFG/MIR pin cleanup early-exit gate:
  `src/test_mir.c` now covers a pin-region block whose terminator is
  `HIR_BLOCK_RETURN`. `cfg-body-dataflow-test-smoke` requires that fixture so
  `pin-unpin-cleanup-edge` remains an all-exit fact, not only a fallthrough
  convention.
- 2026-04-29 CFG/MIR pin cleanup branch-return gate:
  `PinBranchReturns` covers terminating `if`/`else` arms inside a pin region.
  Both arms must keep cleanup successor routing and the
  `pin-unpin-cleanup-edge` fact. This is still narrower than full branch/join
  ownership closure, but it locks another concrete all-exit cleanup case.
- 2026-04-29 CFG/MIR pin cleanup loop-control gate:
  `PinLoopControl` covers `break` and `continue` lowered as `HIR_BLOCK_GOTO`
  inside a pin region. Those loop-control exits must keep cleanup successor
  routing and the `pin-unpin-cleanup-edge` fact before the broader loop
  ownership/lifetime lattice is considered closed.
- 2026-04-30 MIR cleanup fact gate:
  `test_mir` now corrupts rollback and invalidation cleanup fact names and
  requires the MIR validator to reject both. Cleanup topology fields are not
  sufficient beta evidence unless the named MIR cleanup fact inventory is
  preserved.
- 2026-04-29 AIR inspection update:
  `pgy --air <source.pgy>` dumps the AIR verification summary after evidence
  collection and before drift failure. AIR remains verification-only and is not
  carried in `CompilerIRBundle`, but reviewers can now inspect intent,
  boundary, evidence, and drift state without reading unit-test internals.
- 2026-04-29 runtime LLVM export owner update:
  `pgy_runtime_lib_slot_array_io_string_exports.h` is now only an 8 LOC stable
  include facade. Secure-slot, device-slot, array/map, and IO/string exports
  live in separate runtime owners at 161/84/239/296 LOC without changing the
  ABI symbol names or `pgy_runtime_lib.c` include seam. The runtime object cache
  freshness list tracks the new leaf owners directly. Gates: `make pgy`,
  `make test-abi`, `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.
- 2026-04-28 LLVM runtime registry owner update:
  `llvm_runtime.c` is now below the 600 LOC review threshold. Raw collection
  export declarations live in `llvm_runtime_raw_collections.c`; channel export
  declarations live in `llvm_runtime_channels.c` behind
  `llvm_runtime_internal.h`. Gates: `make pgy`, `make llvm-test-smoke`, and
  `make llvm-test-backend-compare` (`196/0` ABI same-process, `64/64`
  backend compare).
- 2026-04-28 LLVM expression projection helper owner update:
  `llvm_expr_boundary_projection_helpers.h` is now below the 600 LOC review
  threshold. Projection nominal lookup, nested vessel path resolution,
  projection-path value loading, and `ProjectSubject` emission live in
  `llvm_expr_projection_path_helpers.h`. Gates: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare` (`196/0`
  ABI same-process, `64/64` backend compare).
- 2026-04-28 LLVM spawn/call helper owner update:
  `llvm_expr_host_spawn_literal_helpers.h` is now below the 600 LOC review
  threshold. Await-task result materialization, direct function-call argument
  emission, generic callee monomorphization, and spawn-expression wrapper
  lowering live in `llvm_expr_spawn_call_helpers.h`. Gates: `make pgy`,
  `make llvm-test-smoke`, and `make llvm-test-backend-compare` (`196/0`
  ABI same-process, `64/64` backend compare).
- 2026-04-29 C declaration lookup owner update:
  `transpiler_decl_lookup.c` is now below the 600 LOC review threshold.
  Current-host, owner-host, nominal-host, and nominal-method lookup live in
  `transpiler_decl_host_lookup.c`; the original owner keeps named declaration,
  alias, inventory, and method-list lookup. Gates: `make pgy`,
  `make test-transpile`, `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).
- 2026-04-29 C type mapping owner update:
  `transpiler_type_mapping_helpers.h` is now below the 600 LOC review
  threshold. AST type-name rendering lives in
  `transpiler_type_render_helpers.h`; the original owner keeps primitive,
  collection, slot, result, and suffix mapping. Gates: `make pgy`,
  `make test-transpile`, `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).
- 2026-04-29 CFG contract validator owner update:
  `mir_cfg_contract_validate.h` is now below the 600 LOC review threshold.
  CFG-owned AST control classification lives in
  `mir_cfg_contract_control.h`; pin cleanup edge validation lives in
  `mir_cfg_contract_pin.h`. The original owner keeps cleanup, successor, and
  predecessor contract validation. Gates: `make test-mir`,
  `make cfg-body-dataflow-test-smoke`, `make abi-ownership-shape-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.
- 2026-04-29 MIR SSA/local type owner update:
  `transpiler_mir_ssa_names.h` is now below the 600 LOC review threshold.
  AST body local type lookup and expression fallback inference live in
  `transpiler_mir_local_type_lookup.h`; the original owner keeps SSA name
  resolution, SSA map setup, claim-shape predicates, and implicit-field
  rendering. Gates: `make pgy`, `make test-mir`,
  `make cfg-body-dataflow-test-smoke`, `make test-transpile`,
  `make production-header-size-test-smoke`, `make backend-inc-size-test-smoke`,
  and `make llvm-test-backend-compare` (`196/0` ABI same-process,
  `65/65` backend compare).
- 2026-04-29 C let slot owner update:
  `transpiler_let_emit.h` no longer owns Slot/DeviceSlot claims,
  ReadView/WriteView/MoveToken declarations, or Slot/SecureSlot sugar
  lowering directly. Those paths live in `transpiler_let_slot_emit.h`.
  Current sizes are 505 LOC and 297 LOC, so the let-declaration owner family
  is below the 600 LOC split-review threshold without reintroducing `.inc`
  files. Gates: `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).
- 2026-04-29 C domain provenance owner update:
  projection-chain bounded recompute and hidden epoch/cause field stamping now
  live in `transpiler_domain_provenance_emit.h`. Role/ability lowering remains
  in `transpiler_domain_role_ability_emit.h`. Current sizes are 237 LOC and
  452 LOC, so this mixed propagation/role owner is below the 600 LOC
  split-review threshold. Gates: `make pgy`, `make test-transpile`, and
  `make runtime-frontier-contract-test-smoke`; parity gate:
  `make llvm-test-backend-compare` (`196/0` ABI same-process, `65/65`
  backend compare).
- 2026-04-29 C class declaration owner update:
  non-generic class declaration lowering now lives in
  `transpiler_class_decl_emit.h`. `transpiler_func_class_flow_emit.h` keeps
  function fallback, generic class specialization, with-slot, and return
  lowering, and is now 594 LOC. The class owner is 138 LOC. Gates:
  `make pgy`, `make test-transpile`, `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).
- 2026-04-29 C MIR block owner update:
  small MIR emission predicate wrappers now live in
  `transpiler_mir_emit_predicates.h`. `transpiler_mir_block_emit.h` is 589 LOC
  and remains focused on MIR block statement emission. Gates:
  `make test-mir`, `make cfg-body-dataflow-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.
- 2026-04-29 CFG consumer update:
  MIR statement population no longer preserves HIR-expanded control
  containers (`if`, `while`, `for`, `select`, `match`, `break`, `continue`) as
  fallback `MIR_INST_STMT` instructions when a block already has CFG successor
  edges. `for` preheader initialization is now a dedicated
  `MIR_INST_LOOP_INIT` fact consumed by C and LLVM. For-loop condition and
  backedge emission now consume the header `MIR_INST_BRANCH` metadata instead
  of re-reading `target->source_ast`. The loop variable and start/end
  expressions are carried on MIR instructions (`arg0`, `expr0`, `expr1`) and
  validated by `mir_validate()`. `mir_validate()` rejects CFG-owned control
  statements that reappear as fallback STMTs, so C/LLVM backends cannot silently
  mix MIR CFG edges with AST control-flow emission. `for value in List<T>` is
  now on the same contract: MIR owns the loop index, list-size condition,
  list-get body binding, and backedge increment in both C and LLVM. Gates:
  `make test-mir`, `make cfg-body-dataflow-test-smoke`, and
  `make llvm-test-backend-compare` (`196/0` ABI same-process, `65/65` backend
  compare). MIR DCE also consumes `mir_stmt_ast_is_cfg_owned_control(...)`
  instead of a private CFG-control AST switch, so statement population,
  validation, and DCE share one classifier.
- 2026-04-29 CFG/AIR handoff update:
  `with`, `parallel`, `unsafe`, and `defer` are now part of the CFG-owned
  boundary set when a MIR block already has successor edges. MIR statement
  population skips these boundary containers in expanded CFG blocks, and
  `mir_validate()` rejects them if they reappear as fallback `MIR_INST_STMT`
  instructions. This is the handoff point for the next AIR sprint: AIR should
  consume the boundary facts produced by CFG lowering, not duplicated AST
  fallback body containers.
- 2026-04-29 AIR execution-boundary update:
  AIR now has an explicit `execution` boundary kind for `with`, `unsafe`,
  `defer`, and pin-block AST metadata. These are sync body/execution boundaries
  and strict evidence checks HIR/CFG evidence for them, not RIR
  resource-boundary evidence. The AIR walker also descends into `with` bodies,
  so nested IO/time boundaries inside `with` are not hidden by the execution
  container. This narrows the previous abstraction-boundary gap: AIR can
  distinguish execution boundaries from ordinary AST syntax while leaving
  zone/world/parallel/channel and IO evidence rules unchanged.
- 2026-04-29 AIR await-boundary update:
  `await` is now synthesized as a stable AIR `parallel` boundary source instead
  of being only recursively scanned through its operand. Strict AIR accepts RIR
  evidence only from the exact `AwaitRemote` operation attached to the same AST
  boundary; a generic scope named `await` is rejected. It still requires HIR/CFG
  evidence for the implementation boundary.
  The AIR boundary AST walk now lives in `src/compiler/air_boundary_walk.c`,
  leaving `src/compiler/air_boundary.c` focused on boundary taxonomy/policy.
- 2026-04-29 AIR task-group boundary update:
  `AST_TASK_GROUP` is now synthesized as a stable AIR `parallel` boundary source
  named `task-group`. Strict AIR now requires both HIR/CFG evidence and matching
  same-AST RIR operation evidence for every stable parallel boundary. RIR
  materializes `AwaitRemote`, `Spawn`, `Async`, `Parallel`, and `TaskGroup`, so
  local grouped-task orchestration is no longer a HIR-only exception.
- 2026-04-29 AIR world-transfer evidence update:
  world handoff evidence is now same-AST specific when the AIR boundary has
  source provenance. A matching RIR `Move` / `Claim` must carry the same AST as
  the world boundary; an unrelated same-alias transfer op in the same RIR scope
  no longer satisfies strict evidence. Gate: `make test-air` and
  `make air-drift-test-smoke`.
- 2026-04-29 AIR channel evidence update:
  RIR now materializes `ChannelSend`, `ChannelRecv`, and `ChannelSelect` ops for
  channel AST boundaries. AIR channel strict evidence consumes those exact
  same-AST ops instead of treating a same-owner/same-name RIR scope as enough.
  This keeps channel evidence aligned with the already tightened `await`
  `AwaitRemote` policy. `make test-rir` now gates parsed-source channel send,
  receive, and select lowering into the same operations.
- 2026-04-29 AIR IO evidence update:
  RIR now materializes beta-stable IO calls as `IO` ops, and AIR IO strict
  evidence consumes only a matching source/provenance op. The parsed-source
  `ReadFile` fixture no longer remains a deliberate missing-evidence negative;
  it is a positive exact-evidence test. Builtin call source spans now reach
  `AST_CALL`, so the common parsed path no longer depends on step-level span
  fallback.
- 2026-04-29 AIR HIR evidence containment update:
  HIR CFG evidence now accepts nested boundary ASTs inside CFG-carried
  statements and terminator values, not only direct statement-pointer equality.
  This closes the execution-boundary seam where `with { ReadFile(...) }` could
  have RIR IO evidence but still miss HIR CFG evidence. The containment matcher
  now mirrors the AIR boundary walk across loops, parallel/async/task-group,
  spawn/call/assignment, arrays/tuples, await/channel/select, match, unsafe,
  defer, event invoke, and lambda bodies; a loop-condition `ReadFile` fixture
  locks the body-control case.
- 2026-04-30 AIR initializer-boundary update:
  AIR boundary walking and HIR evidence containment now descend into
  `AST_LET_DECL` and `AST_LET_DESTRUCTURE` initializers. This closes the seam
  where an implementation boundary hidden behind `let x = ReadFile(...)` inside
  an intent-step block was ordinary syntax to AIR instead of an abstraction
  boundary. Gate: `make test-air` with the `AIR synthesis captures boundary
  from let initializer` regression and `make air-drift-test-smoke`.
- 2026-04-29 HIR intent CFG evidence update:
  parsed-source intent routines now get a minimal ordered clause CFG from
  `src/compiler/hir_lower_intent_cfg.c`. `hir_lower_cfg.c` remains focused on
  function-body CFG at 598 LOC; the intent owner is 184 LOC and materializes
  priority/success/failure expressions plus each intent step's `where`,
  `using`, `intent`, contract, `on`, and `compensate` clauses as HIR CFG
  statements. This is an AIR evidence closure, not a runtime scheduler:
  strict AIR can now require HIR CFG evidence for parsed-source intent
  boundaries without accepting routine-only provenance. MIR population also
  preserves intent `MIR_INST_STMT` semantic carriers after CFG statement
  reconstruction, so participant/zone/authority/causes metadata remains MIR
  inventory instead of being treated as disposable AST fallback emission.
- 2026-04-29 DAG compatibility inventory update:
  type-resolution DAG fallback remains closed (`materializer_fallbacks=0`,
  alias/non-alias stage metadata materialization 0). The
  `retired-compatibility-resolver` audit calls are now AST-kind accounted by
  smoke, and the current semantic-suite max inventory is 0 calls after public
  semantic regression helpers moved to
  `semantic_type_resolution_lookup_type_ref_or_materialize(...)`. The same gate
  reads the global retired counters as max/last inventory rather than summing
  repeated per-context stats lines, caps retired compatibility API calls at 0,
  and requires retired compatibility body fallbacks (`cache misses`) to remain
  0. This makes the next DAG cleanup target explicit: the recursive resolver is
  gone from the beta path, and the remaining counters are audit-only debt
  detectors. The resolver is also no longer exposed from public
  `type_checker.h`; the resolver inventory smoke rejects public header
  re-exposure and semantic regression tests that call it directly.
- 2026-04-29 CFG-owned control classifier update:
  `mir_cfg_contract_control.h` now has a real header guard and is consumed by
  both MIR statement population and MIR CFG validation. This removes the
  duplicated CFG-owned control list from `mir_stmt_population.h`, so fallback
  `MIR_INST_STMT` filtering and validator rejection use the same source of
  truth.
- 2026-04-29 ABI ownership gate update:
  `make abi-ownership-shape-test-smoke` now gates the implemented Slot/Pin ABI
  shape, runtime pin generation/thread/token invariants, C/LLVM pin/unpin
  lowering, MIR cleanup evidence, backend compare pin fixtures, and the
  Zone-Bound Handle docs contract. This does not claim non-pin handle lifetime
  is fully solved; it keeps the implemented ABI subset and the missing
  first-class Zone-Bound Handle piece in one visible gate.
- 2026-04-29 MIR declaration inventory smoke update:
  `make mir-declaration-inventory-test-smoke` is shell-only. It still rejects
  raw MIR declaration/routine inventory access outside helper owners and keeps
  MIR method metadata accessor requirements in the beta gate, but no longer
  needs Python on CI runners.
- 2026-04-29 Runtime ABI lifetime smoke update:
  `make runtime-abi-lifetime-test-smoke` is shell-only. It keeps the borrowed
  runtime string, result-owned string/array, runtime-owned file-handle, macro
  export, and ownership proof-doc checks while removing the last Python
  dependency from this ABI lifetime gate.
- Beta closure now follows the lean sprint loop in
  `docs/71_beta_execution_tickets.md`: close one implementation debt slice
  first, run the slice-local gate, then run wider regression at the slice or
  sprint boundary.
- Full regression is still mandatory before declaring a blocker closed, but it
  is not the inner edit loop. The inner loop should remove source-of-truth
  duplication, fallback seams, or owner-boundary debt.
- A test-only tightening sprint is not beta progress unless it also removes or
  constrains the underlying implementation debt.
- Production owner size is part of beta readability, not style polish:
  600 LOC is the split-review threshold for production `.c` and private owner
  `.h` files. 1,000 LOC remains the hard stop / risk line, but files between
  600 and 1,000 LOC still need a named owner-seam plan unless they are compact
  generated tables, ABI declarations, or single-purpose orchestration layers.
- Current owner-size baseline: production `.inc` debt under `src/` is closed,
  but production `.c` and private owner `.h` files are not yet all below the
  600 LOC split-review threshold. The remaining 600-1,000 LOC review-band
  queue includes backend/tooling owners such as `pgy_lsp.c`, C expression
  emitters, and runtime/tooling headers.
  LLVM intent/domain declaration owners are now below the threshold after the
  setup/context/cleanup and forward/struct-field splits, and `transpiler.c`
  is below the threshold after the entry/thread-pool/misc-decl split.
  `transpiler_overlay_projection.h` is also below the threshold after the
  host-field and zone-bind splits, and `llvm_domain_zone_sync.c` is below
  the threshold after the relation-clause split. `llvm_domain_world_sync.c`
  is below the threshold after the directive-pass split. `llvm_runtime.c` is
  below the threshold after the raw collection/channel registry split, and
  `llvm_expr_boundary_projection_helpers.h` is below the threshold after the
  projection-path helper split. `llvm_expr_host_spawn_literal_helpers.h` is
  below the threshold after the spawn/call helper split. This is no longer `.inc` debt, but it is still
  beta readability debt. `llvm_internal.h` has moved below the
  threshold by splitting private API declarations into `llvm_internal_api.h`
  and fixed limits / dynamic-array helpers into `llvm_limits_internal.h`. The
  LLVM registry owner is also below the threshold after splitting resource/type
  registry behavior into `llvm_registry_resources.c`. The
  world semantic owner family is below the threshold after moving lookup/
  lifecycle helpers to `type_checker_world_helpers.c` and shared domain slot
  validation to `type_checker_domain_slots.c`. The
  AST print/constructor/type split and
  semantic domain contract / constructor-call / intent-transfer /
  intent-action-contract / intent-authority / intent-participant /
  ownership-constructor-diagnostic splits plus the
  runtime Slot Pin owner split, type-system inference/effect split,
  AST destroy/domain-destroy split, parser declaration/type split, and parser
  statement-dispatch split closed their named owner families. Semantic zone
  declaration ownership is also below the 600 LOC split-review threshold after
  shape/projection/state splits, and lifecycle authority-presence diagnostics
  now live in the zone authority owner instead of the declaration
  orchestration body.
  Expression semantic ownership is below the 600 LOC split-review threshold:
  `type_checker_expr.c` owns expression/member dispatch, `type_checker_expr_call.c`
  owns call dispatch and slot/host call behavior, and
  `type_checker_expr_host.c` owns nominal host field/method lookup through
  explicit `expr_*` seams.
  Stdlib builtin semantic ownership is also below the threshold after moving
  `List` / `Set` / `Queue` / `Array` typing into
  `type_checker_builtins_stdlib_collections.c`; the body dispatcher now
  delegates scalar, map, and collection families through focused owner seams.
  HIR
  construction/destruction owners are split into `hir.c`, `hir_routines.c`, and
  `hir_destroy.c`, while compiler driver/result/LLVM/runtime-cache ownership is
  split into `compiler.c`, `compiler_result.c`, `compiler_llvm.c`,
  `compiler_toolchain.c`, and `compiler_runtime_cache.c`. Driver pipeline
  ownership is also split so `driver_app.c` owns orchestration and
  `driver_diag.c` owns JSON diagnostic routing / AIR drift diagnostic wording.
  Module normalization is split so `module_normalizer.c` owns module-level
  orchestration / namespace shells / export scanning, while
  `module_normalizer_refs.c` owns rename-scope, shadow-name, type/generic/call,
  and AST-reference rewriting behind `module_normalizer_internal.h`. Scaffold
  ownership is split so `driver_scaffold.c` owns filesystem helpers,
  single-file scaffold templates, and command dispatch, while
  `driver_scaffold_project.c` owns simulator/project directory templates behind
  `driver_scaffold_internal.h`. RIR builder lowering is no longer carried by an
  implementation-style header: `rir_builder.c` owns general RIR lowering,
  `rir_builder_intent.c` owns intent-scope collection, `rir_facts.c` owns RIR
  fact/utility materialization, `rir_names.c` owns RIR vocabulary names,
  `rir_public_surface.c` owns RIR dump/destroy public-surface behavior, and
  `rir_validation.c` owns RIR validation / DIR contract checks, and
  `rir_flow.c` owns HIR-backed RIR flow enrichment.
  `rir_internal.h` declares the shared private seam. `rir.c` is now below the
  600 LOC split-review threshold, so the active compiler-owner queue has moved
  from these closed owner families to the next remaining source-of-truth seams:
  CFG consumers, AIR boundary consumers, DAG evaluator fallback seams, and
  MIR/LLVM declaration bootstrap parity.
- LLVM MIR CFG control owner debt is partially closed: CFG-expanded range
  `for`, `select`, and `match` lowering now lives in
  `src/codegen/llvm_mir_cfg_control.c`, and `llvm_mir_block_emit.h` is below
  the 600 LOC threshold. This does not close the wider LLVM owner queue because
  declaration inventory bootstrap still has AST-carried seams and several
  backend emitters remain in the review band.
- LLVM statement ownership is also below the 600 LOC review threshold:
  `llvm_stmt.c` owns statement dispatch, defers, return/if/block emission, and
  expression-statement forwarding; `llvm_stmt_zone_action.c` owns zone-action
  effect runtime propagation; `llvm_stmt_type_render.c` owns generic
  type-argument rendering; `llvm_stmt_let_collections.c` owns
  collection/channel/array let specializations; and
  `llvm_stmt_let_callable.c` owns callable/lambda let registration.
- LLVM MIR CFG match destructor parity is closed for the direct ABI probe:
  `llvm_mir_cfg_control.c` handles `Some/None` and `Ok/Err` tag checks and
  payload bindings, so `projection_abi` no longer compares aggregate Option
  values with `icmp` or drops `Some(v)` to `0`.
- C MIR CFG consumer parity for the same frozen surface is closed:
  `transpiler_mir_cfg_control_emit.h` owns range-loop init/header/backedge
  lowering and `Option`/`Result` match-case branch conditions for the C backend.
  Explicit CFG containers no longer fall through to opaque AST statement or
  expression emission, and pin-view SSA values are blocked from escaping a pin
  region through phi copies. The phi-copy owner was split to
  `transpiler_mir_phi_emit.h`, bringing `transpiler_mir_ssa_emit.h` below the
  600 LOC split-review threshold. MIR terminator emission was split to
  `transpiler_mir_terminator_emit.h`, and residual statement helpers were split
  to `transpiler_mir_stmt_emit.h`, so `transpiler_mir_func_emit.h` and
  `transpiler_mir_block_emit.h` are also below the 600 LOC threshold. `make
  llvm-test-backend-compare` is green with ABI same-process `196 passed, 0
  failed` and backend compare `64/64 passed, 0 failed`.
- C intent declaration emission is also below the 600 LOC review threshold:
  `transpiler_intent_emit.h` now owns orchestration, while
  `transpiler_intent_prologue_emit.h` owns signature/runtime-entry emission and
  `transpiler_intent_cleanup_emit.h` owns cleanup/rollback/invalidation tail
  emission. Current sizes are 577 / 186 / 278 LOC, and
  `make llvm-test-backend-compare` remains green at ABI same-process
  `196 passed, 0 failed` and backend compare `64/64 passed, 0 failed`.
- Remaining backend debt for this area is no longer C MIR emitter owner size;
  it is the higher-level source-of-truth work: declaration/top-level inventory
  bootstrap, broader CFG/dataflow semantic consumption, and AIR boundary
  consumption.
- MIR declaration inventory has a shared active-read API seam:
  `mir_active_inventory()` and `mir_active_externs()` are the compiler-owned
  mapping from declaration kind to the current `MIRProgram` declaration
  inventory. C `transpiler_active_inventory()` and LLVM
  `llvm_active_inventory()` consume this seam instead of carrying duplicate
  backend-local `ASTNodeType -> mir->...` switches. This tightens the future
  dedicated declaration-IR migration boundary, but it does not close the
  remaining debt that the current inventory payloads are still AST-carried.
  Gate: `make mir-declaration-inventory-test-smoke`.
- Intent helper ownership is now also split below the 600 LOC review threshold:
  `type_checker_intent_helpers.c` owns condition/involves/projection-adjacent
  utilities, `type_checker_intent_action_contract.c` owns action-contract
  inheritance and redundant-step warnings, and
  `type_checker_intent_contract_summary.c` owns contract-source summary
  formatting.
- Ownership escape diagnostic ownership is split below the 600 LOC review
  threshold: `type_checker_ownership_diag.c` owns the shared borrow/escape
  diagnostic family and `type_checker_ownership_diag_constructor.c` owns the
  constructor-field escape path.
- Function-call late-helper ownership is split below the 600 LOC review
  threshold: `type_checker_helpers_late.c` owns callable dispatch, argument
  ownership flow, and return materialization; `type_checker_slot_view_active.c`
  owns active slot-view discovery and owner-escape rejection;
  `type_checker_call_contract_helpers.c` owns callee parameter contract /
  escape-summary lookup; and `type_checker_call_generic_where.c` owns call-site
  generic where-clause validation.
- Intent authority/participant ownership is split below the 600 LOC review
  threshold: `type_checker_intent_decl.c` keeps intent declaration
  orchestration, `type_checker_intent_authority.c` owns missing `authorized by`
  diagnostics and authorized participant-to-zone-authority resolution, and
  `type_checker_intent_participants.c` owns `who` participant validation plus
  zone/transfer subject-slot matching. Current sizes are 504 LOC, 242 LOC, and
  115 LOC respectively.
- CFG body-flow effect diagnostics are split into a real implementation owner:
  `type_checker_flow.c` owns body-flow orchestration and CFG fact consumption,
  `type_checker_flow_effects.c` owns branch-effect conflict,
  unreachable-statement, and effect-delta merge diagnostics, and
  `type_checker_flow_effects.h` is declaration-only. Current sizes are 457
  LOC, 122 LOC, and 33 LOC respectively. This removes another
  implementation-style private-header seam from the body-safety path.
- HIR CFG ownership is split below the 600 LOC review threshold:
  `hir_cfg.c` owns predecessor finalization, reachability,
  dominance/frontier, dominator tree, natural loops, and CFG summary
  finalization; `hir_cfg_phi.c` owns local-def collection, SSA-name
  collection, phi-candidate placement, and phi materialization behind the
  private `hir_cfg_internal.h` seam. Current sizes are 388 LOC, 222 LOC, and 8
  LOC respectively.
- Semantic effect/helper implementation-header debt is split:
  `type_checker_helpers_effects.h` is declaration-only, while
  `type_checker_helpers_effects.c` owns effect/type helper behavior,
  `type_checker_projection_path.c` owns projection source field-path
  resolution, and `type_checker_world_embedding.c` owns world constructor
  zone-embedding handoff diagnostics. Current sizes are 504 LOC, 177 LOC, 132
  LOC, and 11 LOC respectively.
- Expression resolver implementation-header debt is split:
  `type_checker_expr.h` is declaration-only, the obsolete
  `type_checker_resolve.c` / `type_checker_resolve.h` compatibility owner is
  deleted, retired compatibility counters live in
  `type_checker_resolution_retired.c`, and assignment/constructed-wrapper
  helpers live in `type_checker_type_helpers.c`.
  `type_checker_resolution_helpers.h` is also declaration-only now;
  `type_checker_resolution_helpers.c` owns
  metadata-first `resolve_named_type(...)`, alias lookup, symbol-kind labels,
  and the embedded-world-zone mutation guard. Expression dispatch, call typing,
  and host lookup/call behavior are now split across
  `type_checker_expr.c`, `type_checker_expr_call.c`, and
  `type_checker_expr_host.c`, all below the 600 LOC review threshold.
- Builtin query/slot operation implementation-header debt is split:
  `type_checker_builtins_query.c`, `type_checker_builtins_query_world.c`,
  `type_checker_builtins_query_channel.c`, and
  `type_checker_builtins_query_domain.c` own query, world-query,
  channel-query, and domain-helper behavior; `type_checker_builtins_slotops.c`,
  `type_checker_builtins_secure_token.c`, and
  `type_checker_builtins_resolve.c` own slot lifecycle/view/device-slot
  builtins, secure-token validation, and builtin name resolution. Their
  headers are declaration-only.
- Nominal builtin dispatch no longer lives in an implementation header:
  `type_checker_builtins_nominal.c` owns the main dispatcher and
  `type_checker_builtins_intent_observability.c` owns the intent
  observability builtin family. `type_checker_builtins_nominal.h` is
  declaration-only and both owners remain under the 600 LOC review threshold.
- Slot analyzer summary/escape behavior is split:
  `slot_analyzer_summary.c` owns access/function-alias/parameter summaries and
  `slot_analyzer_escape.c` owns escape collection/mask materialization. The
  semantic shape gate now tracks both owners under the 600 LOC review
  threshold.

상태 표기:

- `DONE`: 구현/문서/회귀가 같은 말을 한다.
- `IN PROGRESS`: 핵심 경로는 있으나 source-of-truth 또는 coverage가 부족하다.
- `BLOCKER`: 베타 이름을 붙이기 전 반드시 닫아야 한다.
- `OUT OF BETA`: 베타 뒤로 명시 이동한다.

---

## 0. Formal Semantics / Proof Obligations

Status: `IN PROGRESS / BLOCKER-DOC`

Goal:

- The beta stable subset must have an explicit mathematical contract before it is called beta-complete.
- The proof source of truth is the split proof pack in `docs/semantics/`, with `docs/102_formal_semantics_and_proof_obligations.md` kept as the stable index.
- Proof evidence is not the same as proof: regression, smoke, and backend-compare runs are supporting evidence, while the stable theorem/invariant statements live in the formal semantics doc.
- The proof scope is intentionally narrow: core declarations, stable generics, anchored own/ref, stable collections, observability baseline, `parallel` baseline, CFG-backed body safety, runtime propagation, DAG, ABI ownership, and C/LLVM parity.

Closed now:

- `docs/semantics/` is the source of truth for beta proof vocabulary, judgments, theorem statements, and remaining proof obligations.
- `docs/102_formal_semantics_and_proof_obligations.md` now points to the split proof pack and remains as the stable English index.
- The doc explicitly separates language proof obligations from the math library design in `docs/45_math_layer_design.md`.
- Out-of-beta proof claims are sealed for full quantum, full FP/HKT/functor algebra, arbitrary ownership, arbitrary map keys, GPU/Spray, Skia/render, package manager, and advanced debugger semantics.
- `Runtime Panic Parity` now has a formal theorem slot in `docs/semantics/06_backend_parity.md`.
- Secure token unforgeability and authority transfer single-owner now have formal theorem slots in `docs/semantics/04_ownership_abi.md`.
- Slot capability calculus now has a formal theorem slot in `docs/semantics/08_slot_capability_calculus.md`; `docs/semantics/proofs/SlotCalculus.v` is explicitly proof-sketch only until a Coq CI gate type-checks it.
- Slot capability calculus now explicitly records the negative claim that Slot
  is not a borrow checker by itself. Runtime generation/token/pin-state safety
  and borrow-checker-equivalent static safety are separate proof claims.
- Slot capability calculus now also records the positive claim: Pergyra does
  not expose memory as address ownership; it exposes memory as a modular
  resource boundary. A Slot is the stable source-level boundary, while the
  backend handle below it remains replaceable.
- Canonical Slot thesis: Pergyra does not expose memory as address ownership;
  Pergyra exposes memory as a modular resource boundary with a replaceable
  backend handle.
- Canonical short form: Pergyra exposes memory as a modular resource boundary;
  Slot has a replaceable backend handle.
- The Slot Coq sketch now models access modes explicitly (`Read`, `Write`,
  `Release`, `Pin`) and carries proof obligations for stale
  read/write/release handles, issued-token read/write/pin/release,
  unissued-token read/write/pin/release rejection, pinned-handle release
  rejection, and pin non-eviction.
- Linux CI now installs `coq`, so `make formal-semantics-test-smoke` type-checks `docs/semantics/proofs/SlotCalculus.v` in CI instead of only checking proof-pack text.
- Slot capability runtime evidence was rechecked with `make test-security` (142/142 passed): stale-generation read/write/pin/release rejection, stale `SlotIsValid` false, zero-id sentinel and slot-id wrap tombstone before ABA reuse, tampered-view generation unpin rejection, double-unpin rejection, release-while-pinned, TTL cleanup skip while pinned, invalid secure token rejection, revoked-token rejection, raw secure-slot release rejection, concurrent secure write rejection, and release-after-unpin are covered.
- `runtime-panic-abi-test-smoke` now covers forged zero-token read/write/release
  rejection for inline C and exported C/LLVM-linkable secure-slot entrypoints.
- SecureSlot token ABI is now build-mode stable: inline C, exported runtime, and
  LLVM-linkable runtime all use the same `PgyToken<T>` layout with read/write
  capability bits. The old release-mode SecureSlot macro has been removed, and
  `runtime-panic-abi-test-smoke` covers no-`PGY_SAFE_SLOTS` invalid-token and
  released-slot hard-fail paths.
- `pgy_abi_spec.h` now carries matching debug/release SecureSlot layout rows for
  all stable primitive payloads (`Int`, `Long`, `Float`, `Double`, `Bool`,
  `String`), and `make test-abi` checks runtime size/token offsets against the
  ABI spec.
- Non-pin handle expiration is a layered contract, not a pin-only story. The
  beta contract is: arena lane checks, CFG/body dataflow, zone/world
  channel-only crossing, token transport rejection, and generation/token
  runtime validation together cover stale-handle scenarios. First-class
  Zone-Bound Handle typing (`SlotHandle<T> in Zone` or equivalent `handle@zone`
  sugar) remains a beta-freeze design decision: implement it before freeze or
  keep the current `BORROW_TRACKED` / anchored-handle conservative rejection as
  the documented stable behavior.
- Authority token mismatch now has a shared runtime contract code/reason
  (`authority-token-mismatch`), queryable runtime state, C/LLVM ABI coverage in
  `authority_failure_abi`, backend-compare coverage in `authority_failure_surface`,
  and direct runtime coverage in `make test-security` (142/142 passed).

Remaining:

- Tie each B0 closure item to a theorem/invariant row before calling that item beta-complete.
- Keep DAG, runtime propagation, MIR declaration inventory, ABI ownership, panic parity, secure token invariants, and backend parity blockers open until their theorem statements and regression evidence match.
- Do not advertise mechanized proof for beta unless a separate Lean/Coq or executable small-step model is added and CI type-checks it.
- Do not advertise "Slot as borrow checker" or "Slot proves borrow safety";
  borrow-checker-equivalent claims require the section `0b` CFG bridge facts
  plus section `4` ABI ownership parity.
- Keep Slot wording positive and precise: Slot is an address abstraction,
  ownership boundary, capability gate, and replaceable backend handle. Do not
  frame it as raw pointer ownership or Rust-style lifetime ownership.
- Decide the Zone-Bound Handle direction before beta freeze. If it is in beta
  scope, add type-level zone scope facts and diagnostics for handle escape past
  zone lifetime. If it is out of beta, document conservative rejection as the
  stable subset and forbid docs from implying non-pin handles have Rust-style
  lifetime proof.
- Keep C/LLVM panic-class regressions green for divide-by-zero, out-of-bounds, released slot, double release, invalid secure-slot token, OOM, authority token mismatch, and internal-invariant unwrap misuse.
- **[NEW]** Add state-machine proofs for the Intent system's rollback and cleanup closure.

Evidence command:

```sh
make formal-semantics-test-smoke
```

## 0a. Systems Language Baseline Closure

Status: `BLOCKER`

Source of truth: `docs/19_design_philosophy.md`

Goal:

- Pergyra is a systems language with domain extensions. The systems-language
  baseline is non-negotiable: no mandatory GC, predictable memory, C FFI, ABI
  stability, raw escape, optional runtime, and compile-time determinism.
- Domain primitives (`intent`, `zone`, `world`, `authority`, `handoff`,
  `Channel`, `parallel`) are first-class, but they are layered on top of the
  systems baseline. They must not replace or weaken it.
- Beta must not claim ecosystem readiness until the systems substrate can
  survive domain-layer evolution without ABI drift, hidden runtime cost, or
  nondeterministic codegen.

Closed now:

- `docs/19_design_philosophy.md` now states the systems-language identity before
  the Slot/resource philosophy: Pergyra is a systems language first, and domain
  extensions are layered above that substrate.
- The stable identity explicitly ties abstraction portability to systems
  portability: if Pergyra cannot reach the target platform with predictable ABI
  and memory behavior, the domain abstraction portability claim is hollow.
- ABI stability is already partially enforced by `src/runtime/pgy_abi_spec.h`,
  ABI static assertions, `make test-abi`, runtime panic ABI smoke, and C/LLVM
  backend compare gates.
- Slot wording is aligned with this identity: source code observes a modular
  resource boundary and capability gate, while the backend handle below Slot
  remains replaceable.
- `--runtime=none` is now a parsed driver mode with structured diagnostics.
  It rejects runtime-dependent surfaces (`parallel`, `spawn`, `Channel`,
  `intent`, `zone`, `world`, `event`, async/future/select/task-group) through
  `PGY_DRIVER_RUNTIME_NONE_UNSUPPORTED`, and it separately blocks false
  freestanding success until C/LLVM no-runtime lowering exists.
- `SlotRawPointer(...)` is now reserved as an explicit unstable raw-escape
  surface and rejected with `PGY_SEM_RAW_ESCAPE_UNSTABLE`. `unsafe { ... }`
  remains a lexical marker only; it does not grant raw pointer capability.

Remaining:

- Define the system-tier raw escape contract. `unsafe { ... }` exists, but
  raw pointer / inline-asm escape from Slot is not beta-stable until a syntax,
  semantic gate, ABI lowering rule, and diagnostics are implemented. Until then
  docs must not imply that `pin slot as view { ... }` is enough for driver,
  kernel, embedded ISR, or MMIO code. Pin/Lease is a typed lexical lease, not
  the system-tier raw escape; source of truth:
  `docs/74_slot_pinning_caching.md`.
- Implement verified freestanding C/LLVM lowering for `--runtime=none`.
  Current mode is intentionally conservative: it defines the CLI contract and
  rejection surface, but it does not emit a no-runtime binary yet.
- Elevate ABI non-leakage to a beta contract: intent/zone/world changes must
  not break C FFI ABI. Domain-layer evolution is allowed only inside the ABI
  envelope guarded by `pgy_abi_spec.h`, ABI tests, and backend parity.
- Add deterministic codegen evidence. Type resolution, generic resolution, AIR
  verification, MIR inventory traversal, C emission, and LLVM emission must not
  depend on hash-map or pointer iteration order. A repeat-build artifact hash
  smoke now exists as `make codegen-determinism-test-smoke`; beta completion
  requires expanding it to the full frozen backend fixture set.

Evidence command:

```sh
make beta-readiness-checklist-test-smoke
make codegen-determinism-test-smoke
make runtime-none-contract-test-smoke
make raw-escape-contract-test-smoke
```

## 0b. Function CFG / Body Dataflow Closure

Status: `IN PROGRESS / BLOCKER`

Source of truth: `docs/103_cfg_body_dataflow_need.md`

Goal:

- Strict beta must not depend on AST-shaped local traversal for routine body safety.
- HIR/MIR CFG already exists, so the blocker is not "add a CFG from zero". The blocker is promoting CFG/dataflow facts to the semantic source of truth for function/action/intent bodies.
- Body safety must cover normal control flow, exceptional cleanup flow, ownership/resource flow, zone/effect transitions, and parallel/channel boundaries before the language is advertised as ecosystem-safe beta.

Closed now:

- HIR has function CFG v0 with predecessor/reachability, dominator/frontier, loop-depth, local-def, and phi-candidate skeleton facts.
- HIR CFG construction now has a hard structural contract before downstream
  consumers run: `hir_validate_cfg_shape()` rejects open fallthrough blocks,
  invalid successor indices, inconsistent terminator successor flags, missing
  branch conditions, and block-id drift before dominance/frontier/loop/phi
  analysis. `hir_validate_cfg_predecessors()` then verifies that materialized
  predecessor lists mirror every successor edge. This closes the previous
  "CFG consumers trust generated shape by convention" seam.
- HIR CFG summaries now expose `return_block_count` and
  `normal_exit_block_count`. Reachable `HIR_BLOCK_UNREACHABLE` blocks are the
  normalized normal-fallthrough exits, so all-path-return consumers can move
  toward a direct CFG fact instead of re-walking AST body shape.
- HIR CFG ownership is now a named compiler owner seam:
  `src/compiler/hir_cfg.c` owns CFG finalization, reachability,
  dominator/frontier, dominator tree, loop-depth, local-def, phi-candidate,
  phi-materialization, and CFG summary facts. `src/compiler/hir_lower_cfg.c`
  owns AST-body to basic-block CFG construction. `src/compiler/hir.c` is
  reduced to the declaration/routine lowering orchestration owner, while
  `src/compiler/hir_analysis.c` owns signature/direct-call/control-flow
  detection.
- HIR CFG lowering now represents loop exits and loop backedges explicitly for
  `break` / `continue`. `while` and `for` bodies carry a loop context, so
  `break` terminates the current block with a `goto` to the loop exit and
  `continue` terminates with a `goto` to the loop header. This keeps HIR CFG
  dominance/frontier/loop-depth facts aligned with semantic loop flow instead
  of leaving loop control as opaque AST payload.
- HIR CFG loop control is now label-aware: nested `break outer` and
  `continue outer` resolve to the named loop's exit/header rather than the
  nearest loop. This keeps HIR CFG edge facts aligned with semantic loop-label
  validation.
- HIR CFG lowering now represents `match` as a case dispatch chain instead of
  a single opaque statement payload. Each `case` is a CFG branch condition,
  case/default bodies flow to a join block when they fall through, and
  terminating cases stay closed. `src/test_hir.c` locks this with
  `HIR CFG lowers match cases and default as explicit edges`.
- HIR CFG lowering now represents `select` as the same dispatch/join shape.
  Channel readiness cases and default bodies are explicit CFG edges instead of
  an opaque select payload. `src/test_hir.c` locks this with
  `HIR CFG lowers select cases and default as explicit edges`.
- HIR CFG lowering now traverses `unsafe` block bodies instead of treating
  `unsafe` as an opaque statement. Nested terminators inside `unsafe` blocks are
  visible to the same CFG dominance/reachability consumers as ordinary block
  terminators.
- MIR has routine/block/instruction/cleanup blocks, SSA version maps, def/use summaries, rollback/invalidation exceptional CFG, liveness/DCE slices, and backend vertical slices.
- RIR already carries flow-block summaries for resource/projection/world-handoff/invalidation/authority-loss style facts.
- MIR cleanup consumes RIR flow/fact/semantic summaries for rollback and
  invalidation block decisions. The previous intent-step AST invalidation
  fallback is removed and gated out by `cfg-body-dataflow-test-smoke`.
- MIR validation now requires each reachable pin-region block to carry the
  matching `pin-unpin-cleanup-edge` fact for its source slot, view binding, and
  read/write mode. `test-mir` includes a negative corruption regression so this
  fact cannot silently become a backend convention again.
- MIR validation now requires every reachable non-cleanup block with a cleanup
  successor to also carry a materialized `cleanup-edge` MIR fact. Rollback and
  invalidation cleanup blocks must likewise carry their named cleanup-edge
  facts. This closes the field-vs-instruction drift seam: backend consumers can
  trust cleanup topology only when the explicit MIR fact inventory exists.
- MIR validation now also rejects cleanup blocks that carry normal CFG
  successors or pin-region state. Cleanup/rollback/invalidation blocks must stay
  on the exceptional cleanup chain rather than becoming normal body-flow blocks.
- Non-`Void` functions now consume the CFG body flow summary for all-path return. If any reachable normal path can fall through without a return terminator, semantic analysis emits `PGY_SEM_MISSING_RETURN` with `Reason:` and `Fix:`.
- Semantic CFG body-flow flag consumption is centralized through named
  fallthrough/terminator helpers, so branch/join decisions are no longer
  repeated as open-coded flag masks in each consumer.
- Statements after direct terminators and after `if`/`match` bodies whose
  reachable paths all terminate now emit
  `PGY_SEM_UNREACHABLE_CODE` with `Reason:` and `Fix:` instead of being
  silently skipped by the body-flow walk.
- `QubitSlot` loop move/join now has source-level regression for break-exit
  consumption and continue-backedge consumed-resource detection.
- `defer` cleanup-body terminators are isolated from the surrounding CFG path:
  they do not make following statements unreachable and do not satisfy
  non-`Void` all-path return.
- `defer` cleanup-body resource facts are isolated by snapshot/restore:
  cleanup moves, releases, and cleanup-only loop terminators are checked without
  consuming the surrounding path's live resource state or outer loop flow.
- The direct `type_check_statement()` fallback path delegates `defer` body
  checking to the same cleanup snapshot helper as CFG body flow, closing the
  previous split-brain semantic path.
- Resource snapshots now cover anchored slot state (`Slot<T>`, `SecureSlot<T>`,
  `DeviceSlot<T>`) as well as `QubitSlot` consumption facts. This closes the
  branch/join case where a release on a terminating branch used to leak into the
  reachable fallthrough path.
- CFG ownership snapshots now also track classifier-backed ownership boundary
  values (`subject` identity, borrow-tracked aggregates, movable resources, and
  anchored handles). `own subject` movement in terminating branches no longer
  poisons reachable fallthrough paths, fallthrough moves are joined as consumed,
  and parallel subject transfers participate in the same duplicate-consume
  conflict gate as slot resources.
- Parallel ownership snapshots now carry task-local `is_used` as well as
  consumed/released state. This closes the stable `ref` + `own` task-boundary
  conflict for ownership-bearing values: a task that borrows a subject cannot
  run in parallel with another task that consumes the same subject.
- Shared `ref` reads of the same ownership-bearing value across parallel tasks
  remain accepted. The beta contract is therefore explicit: shared `ref`/`ref`
  read boundaries are allowed, while `ref`/`own` and `own`/`own` task-boundary
  conflicts are rejected for the stable ownership subset.
- `spawn` direct named-call boundaries now reject borrowed `ref` parameters for
  ownership-bearing values (`subject`, borrow-tracked aggregate, movable
  resource, anchored handle). Copy-only `ref` arguments remain accepted, and
  the diagnostic uses `PGY_SEM_BORROW_ESCAPE` with `Reason:` / `Fix:` wording.
- Direct named `spawn` boundaries now also reject authority-bearing `Token<T>`
  parameters. This closes the stable beta token-transport rule across channel
  send/receive helpers, cancellation payloads, channel close, and spawn.
- Function types now carry first-stage interprocedural body summaries through
  `body_summary_mask`. The current seam records `may_return`, `may_escape_ref`,
  `moves_param`, `borrows_param`, `drops_resource`, `effects`,
  `requires_zone`, `spawns_task`, and `sends_channel`, giving later CFG/runtime
  propagation and backend parity work one stable fact surface instead of
  repeatedly rediscovering those facts from AST-shaped helpers. Direct function
  calls now consume callee summaries and propagate transitive caller-relevant
  facts while keeping callee-local `may_return` local to the callee.
  Direct function calls, method calls, and host calls also record
  declaration-known summary facts (`effects`, `requires_zone`, and `own/ref`
  parameter modes).
- Parser-accepted anonymous async spawn bodies (`spawn async () { ... }`) are
  explicit beta rejects until closure capture/lifetime analysis is closed. The
  stable beta surface is named `spawn Worker(args...)`, where parameters,
  effects, and ownership boundaries are checked through declarations.
- `parallel` task bodies now consume CFG/resource snapshots directly: task-local
  terminators stay local to the task, task resource moves/releases are joined
  after the parallel block, and duplicate cross-task consumption is rejected with
  `PGY_SEM_PARALLEL_SLOT_CONFLICT`. Blocking channel send of a movable resource
  in a parallel task is fixed to the same consume/join contract.
- Non-blocking/timeout channel receive for ownership-bearing payloads is
  explicitly rejected (`TryRecv(Channel<QubitSlot>)`,
  `TryRecv(Channel<Slot<Int>>)`, `RecvTimeout(Channel<Array<Int>>, t)`).
  The stable non-blocking receive surface is copy-only; movable, subject,
  boundary-value, anchored-handle, and token payloads must use blocking `<-`
  into a named binding or a plain projection/value channel.
- Timeout/status channel send surfaces now share the same explicit transport
  policy as `TrySend`: movable resources remain blocked on builtin send
  helpers, authority-bearing `Token<T>` is rejected, and blocking `ch <- value`
  stays the explicit ownership-transfer path for named resources.
- `Cancel(Future<T>)` and `Cancel(RemoteFuture<T>)` are copy-only for beta.
  Ownership-bearing payload futures are explicitly rejected until task-boundary
  cleanup summaries can prove where movable/anchored/subject/token payloads are
  released or observed.
- `ChannelClose(Channel<T>)` is copy-only for beta. Closing a channel with
  ownership-bearing queued payloads would need a cleanup/backpressure summary,
  so movable, subject, boundary-value, anchored-handle, and token channels must
  be drained explicitly before close.
- Slot borrow-safety bridge facts are now named in both
  `docs/103_cfg_body_dataflow_need.md` and
  `docs/semantics/08_slot_capability_calculus.md`: `NoEscape(view, region)`,
  `NoSuspend(view, region)`, `WriteExclusive(slot, region)`,
  `DropOnce(owner, all_cfg_exits)`,
  `ReleaseAfterUnpin(slot, all_cfg_exits)`, and
  `NoUnsupportedTokenTransport(token, boundary)`.
- Existing `ViewRead(...)` / `ViewWrite(...)` semantic constructors and the
  source-level `pin slot as view: ReadView<T>|WriteView<T> { ... }` block now
  cover the first bridge slice: `ReadView<T>` return escape uses
  `PGY_SEM_PIN_ESCAPE`, active view + `await` uses
  `PGY_SEM_PIN_AWAIT_BOUNDARY`, active view + direct named `spawn`, `async`
  block, and event lambda callback registration use the same
  suspension-boundary diagnostic, active view + channel send/receive/close uses
  the same handoff-boundary diagnostic, active view + `Cancel(...)` uses the
  same cleanup-boundary diagnostic, active view + `defer` registration uses
  the same cleanup-boundary diagnostic,
  active/acquired view across
  `parallel` uses `PGY_SEM_PIN_PARALLEL_CONFLICT`, `QubitSlot` pin attempts
  use `PGY_SEM_PIN_QUBIT_REJECT`, and `WriteView<T>` exclusive access is
  covered in semantic regression plus `diagnostics-json-test-smoke`. Source
  pin typed-view read parity is covered for plain, secure, and sequential
  mixed slot cases by `pin_read_view_block`, `pin_secure_read_view_block`, and
  `pin_mixed_read_view_sequence`; typed-view write parity is covered by
  `pin_write_view_block` and `pin_secure_write_view_block`; cleanup-edge
  parity is now fixed for normal successor exit, direct return inside a pin
  block, branch-to-return exit, and loop `break`/`continue` exit by
  `pin_successor_cleanup_block`, `pin_return_value_block`,
  `pin_branch_return_block`, `pin_continue_cleanup_block`, and
  `pin_break_cleanup_block`. Secure boundary-slot parameter pinning is covered
  by `pin_secure_param_read_view_block`.

Remaining:

- Richer reachability provenance across nested/exceptional CFG edges, the
  general branch/join assignment lattice beyond the current sealed local-`let`
  surface and longer-lived borrow lifetime beyond the current task-local
  borrow/use snapshot baseline, full drop/cleanup insertion and validation
  beyond current `defer` isolation, zone/effect transition, projection
  freshness, broader channel receive/backpressure, and richer cancellation
  task-boundary checks must consume CFG/dataflow facts directly. Anchored slot branch-join,
  `own subject` branch-join, parallel resource/boundary consume state, and
  parallel `ref`+`own` boundary conflicts, plus direct named-call `spawn ref`
  boundary rejection, anonymous async spawn explicit reject, timeout/status
  channel-send transport rejection, non-blocking ownership-bearing receive
  explicit reject, copy-only cancellation payload reject, and copy-only channel
  close are already covered by regression and remain as closed baseline
  evidence.
- Full mutable-borrow overlap is not a beta blocker because beta has no
  `mut ref`/`ref mut` surface. If such a surface is introduced after beta, it
  must be added as a new CFG lattice fact instead of being inferred from current
  `ref` parameters.
- Interprocedural body summaries must be fixed: `may_return`, `may_escape_ref`, `moves_param`, `borrows_param`, `drops_resource`, `effects`, `requires_zone`, `spawns_task`, and `sends_channel`.
  First-stage `body_summary_mask` storage and semantic recording exist now; the
  direct function-call consumer and method declaration-summary consumer also
  exist. Lambda body checking is isolated now: lambda-local effect/body facts
  are stored on the lambda function type and do not leak into the enclosing
  routine before the lambda is called; function-typed lambda bindings propagate
  those facts through the same callee-summary path as named functions. The
  remaining blocker is making zone/effect/runtime/codegen consumers use those
  bits instead of local rediscovery.
- Diagnostics must report path provenance with branch/join edge, previous state, `Reason`, and `Fix`.
- C and LLVM must lower the frozen subset from the same CFG/dataflow facts and be covered by backend compare.
- Option C ownership lift now has the block-scoped
  `pin slot as view: ReadView<T>|WriteView<T> { ... }` parser/semantic surface.
  The source-level block desugars to the same typed-view semantic slice; that
  slice has C/LLVM read/write parity for plain and secure slot cases, including
  a sequential mixed read case. C source-block cleanup and C/LLVM MIR
  successor/return cleanup now emit explicit typed pin/unpin calls for the
  frozen pin backend-compare fixtures, including normal successor exit, direct
  return, conditional branch-to-return exit, and loop `break`/`continue` exit.
  Active view + `defer` registration is rejected semantically. The remaining
  runtime pin-block closure is broader exceptional/cancellation
  exit coverage plus the `DropOnce` / `ReleaseAfterUnpin` theorem row.

Evidence command:

```sh
make cfg-body-dataflow-test-smoke
make ir-pipeline-test-smoke
make test-semantic
make llvm-test-backend-compare
make llvm-campaign-projection-test-smoke
make llvm-dnd-campaign-test-smoke
```

Step skeleton:

1. CFG fact inventory gate: `cfg-body-dataflow-test-smoke` keeps HIR CFG, HIR dom, RIR flow-block, and MIR cleanup/SSA facts visible.
2. Semantic control-flow gate: all-path return and reachability use CFG facts;
   local delayed initialization stays sealed by `let = initializer`, while any
   future wider assignment surface needs explicit CFG lattice facts.
3. Ownership gate: move/use-after-move, borrow lifetime, mutable borrow overlap, and drop/cleanup use CFG join facts.
4. Orchestration gate: zone/effect/relation/projection/handoff facts use body summaries at branch/join.
5. Execution gate: `parallel`, task, cancellation, and channel boundaries use interprocedural body summaries.
6. Backend gate: C and LLVM consume the same frozen facts and backend-compare covers representative cases.

## 0c. Core Language Semantic Closure

Status: `BLOCKER`

Goal:

- The beta stable subset must be defined in one place, not inferred from scattered README/TODO/status notes.
- Intent, zone/world/authority/handoff, and projection freshness are core language semantics, not library polish.
- This section is the checklist entry for the formal proof documents under `docs/semantics/01_intent_world_zone.md` and the stable subset contract.
- Stable subset source of truth: `docs/107_beta_stable_subset.md`.

Stable subset that must be frozen:

- Core declarations: `subject`, `zone`, `world`, `intent`, `relation`, `effect`, `projection`, `authority`, `handoff`.
- Core execution: `func`, `let`, `if`, `match`, `for`, `while`, `return`, `parallel`, stable channel/task baseline.
- Generic contracts: exact type parameters, ability bounds, multi-bound `where T: A + B`, implemented default type argument resolution.
- Ownership: anchored slot-handle boundary subset, boundary-visible aggregate provenance, copy-value trivial `own/ref`, explicit reject for `Token<T>` transport.
- Collections: `List<T>`, `Set<T>`, `HashMap<String, T>`, `HashMap<Int, T>`, plus any currently implemented additional key families only if docs/tests/backend parity list them explicitly.
- Observability: `last`, `history`, `active`, `recent` baseline.
- Option C ownership lift decision: a generic param ownership classifier is a
  beta blocker if ownership-sensitive generic code is accepted. Without that
  classifier, generic `own/ref` uses that depend on unknown `T` ownership class
  must be explicitly rejected instead of inferred.
- Current conservative baseline: unresolved `TYPE_KIND_GENERIC` classifies as
  `BORROW_TRACKED`, so generic `own/ref` boundaries are rejected unless a later
  classifier can prove a stable ownership class for `T`.
- Minimal single-thread `Rc<T>` / `Weak<T>` is beta-stable for
  `Int`, `Long`, `Float`, `Double`, `Bool`, and `String`. The closed contract
  includes resolver metadata, semantic builtin typing, C runtime, LLVM runtime
  exports, C emitter lowering, LLVM builtin lowering parity, ABI layout tests,
  and lifecycle backend-compare regression.
  Fractional numeric literals infer as `Float`; `Rc<Double>` is stable through
  explicit `Double`-typed values or annotations, not a separate double-literal
  surface.
  Payloads outside that set are explicitly rejected in semantic analysis before
  backend lowering.
  Gate phrase: shared ownership stable subset requires C/LLVM lifecycle parity.
  `Arc<T>`, cross-thread shared ownership, generic/object payloads beyond the
  frozen primitive/String set, and default ARC remain outside the beta contract.

Intent closure:

- Intent step ordering must be deterministic and backend-independent.
- Compensation, rollback, cancellation, and invalidation paths must have a formal meaning and an ABI smoke surface.
- Intent effect propagation must use the same runtime provenance vocabulary as zone/effect/projection propagation.
- Intent observability ABI fields and trace order must be versioned or explicitly frozen for beta.

Zone/world/authority/handoff closure:

- Zone generation and world embedding must define ownership and handoff behavior.
- Handoff frontier recompute must define pass limit, stale-read behavior, and hard-fail boundary.
- Projection freshness must state when `refresh`, `publish`, and `bind` make data visible.
- Authority rejection must expose queryable recoverable state for beta-stable recoverable failures.

Runtime frontier scheduler closure:

- The current beta evidence covers world derived-state bounded recompute, zone
  lifecycle bounded frontier loop, projection-chain bounded recompute, embedded
  world-zone projection freshness, embedded world-zone action-caused layer/state
  freshness, v1 handoff projection/world/layer-state freshness, and the
  authority/failure handoff queryable baseline.
- `make runtime-frontier-contract-test-smoke` gates that C and LLVM both keep
  bounded frontier loops, pass limits, and hard-fail overflow boundaries for the
  covered world/zone/projection slices, and that authority rejection remains a
  recoverable queryable failure surface (`last_ok / zone / participant / code /
  reason`) with C/LLVM parity cases. The same gate now requires the C and LLVM
  emitters to consume `src/codegen/domain_frontier_policy.h` for frontier
  pass-limit formulas instead of reintroducing helper-local constants.
- 2026-04-29 update: the stable world outer frontier now consumes the named
  `pgy_frontier_world_transitive_pass_limit(...)` policy in both C and LLVM.
  This makes the world zone-sync plus derived-state recompute family a shared
  source-of-truth contract instead of two backend-local helper choices.
- 2026-04-29 update: frontier pass-limit formulas now saturate through the same
  u32-bounded helper family before emission. This keeps C `size_t` loops and
  LLVM i32 loop counters on the same bounded contract for oversized generated
  frontier families.
- 2026-04-29 update: `make runtime-frontier-policy-test-smoke` compiles and
  executes the `src/codegen/domain_frontier_policy.h` arithmetic directly. This
  keeps the frontier policy gate from being only a string-contract check.
- Remaining blocker: the full bounded fixpoint / transitive frontier scheduler
  must broaden that same transitive frontier policy beyond the currently
  covered world/zone/projection slices so the broader world-zone propagation
  family cannot grow helper-specific edge policies.

Evidence command:

```sh
make formal-semantics-test-smoke
make runtime-authority-contract-test-smoke
make runtime-frontier-contract-test-smoke
make runtime-frontier-policy-test-smoke
make projection-diagnostic-contract-test-smoke
make llvm-test-backend-compare
```

## 0d. Runtime Panic And Secure Authority Invariants

Status: `BLOCKER`

Goal:

- Runtime failure behavior must be the same on C and LLVM for the frozen subset.
- Security-bearing surfaces must have explicit invariants before the language claims security semantics.

Runtime panic/unwinding policy that must be frozen:

- OOM
- divide-by-zero
- array/slice/list/map out-of-bounds
- released slot use
- double release
- released or double-released device slot use
- invalid secure-slot token
- authority token mismatch
- internal compiler/runtime invariant break

Implementation progress:

- `src/runtime/pgy_runtime_panic_contract.h` owns the panic class vocabulary and
  shared `PGY_RUNTIME_PANIC` emitter.
- Inline runtime `PGY_PANIC` delegates to the shared panic contract.
- LLVM exported typed slot read/write now hard-fails on released-slot access
  instead of logging and returning a default value.
- LLVM exported secure slot read/write/release now hard-fails on released secure
  slot, invalid token, and denied token capability.
- Inline and LLVM exported device slot read/write/release now hard-fails on
  released or double-released device slots instead of silently no-oping or
  returning a default value.
- Generated C and LLVM `Array<T>`/`Slice<T>` indexing, including temporary
  function-return access (`Words()[0]`, `Words().Slice(...)[0]`), and
  `ArraySet` now lower through checked runtime helpers, so stable collection
  out-of-bounds reaches the shared `out-of-bounds` panic class instead of
  direct memory access.
- Stable value-demanding collection APIs now share the same hard-fail policy:
  `ListGet` out-of-range, `QueuePop` on an empty queue, and `MapGet` on a
  missing key panic with `out-of-bounds` in generated C and LLVM. Recoverable
  absence checks stay on `ListSize`, `QueueEmpty`, and `MapHas`.
- Stable mutation collection APIs no longer silently no-op on invalid targets:
  `ListSet`, `ListRemove`, and `MapRemove` invalid access panic with
  `out-of-bounds` in generated C and LLVM.
- Stable unwrap misuse no longer drifts between backends: `Unwrap(Err)` and
  `UnwrapOption(None)` panic with `internal-invariant` in generated C and LLVM.
- `docs/105_runtime_panic_contract.md` records the runtime panic contract and
  the stable collection access/mutation hard-fail split.
- `runtime-panic-abi-test-smoke` executes inline and exported runtime hanesses
  for released-slot, invalid-secure-token, double-release, device-slot,
  authority-mismatch, OOM, and divide-by-zero panic classes.
- Authority token mismatch now records a stable recoverable query state before
  hard-fail use: code `authority-token-mismatch`, reason `zone authority
  validation failed: authority token mismatch`, and stderr without secret token
  material. `authority_failure_abi` and `authority_failure_surface` keep the
  C/LLVM ABI and backend outputs aligned.
- Authority-bearing `Token<T>` transport is explicitly rejected on the current
  beta transport surfaces: blocking channel send/receive, non-blocking/timeout
  channel helpers, channel close, cancellation payloads, and direct named
  `spawn` boundaries.

Required policy decision:

- Recoverable user/runtime contract failures expose `Bool`, `Result<T>`, or queryable runtime state.
- Contract violations at ownership/security boundaries are hard-fail unless explicitly modeled as recoverable.
- Intenal compiler/runtime invariant breaks are hard-fail.
- No beta path may silently fallback to a different backend behavior.

Secure slot / authority invariant obligations:

- Secure tokens are unforgeable by source-level code.
- Secure slot token mismatch cannot read or write the protected slot.
- Authority-bearing tokens cannot be copied into an untrusted boundary or transported through unsupported channels.
- Zone authority transfer cannot create two active owners for one authority boundary.
- Runtime snapshots must not expose secret token material.

Evidence command:

```sh
make runtime-authority-contract-test-smoke
make runtime-panic-contract-test-smoke
make runtime-panic-abi-test-smoke
make runtime-panic-codegen-test-smoke
make runtime-abi-lifetime-test-smoke
make llvm-test-backend-compare
```

## 0e. User-Facing Beta Quality Gates

Status: `IN PROGRESS / BLOCKER`

Goal:

- Beta must be honest about platform support, diagnostics, stdlib API stability, tooling stability, and performance regression limits.

Diagnostic quality gate:

- Every user-facing parser, lexer, semantic, backend, and runtime error must have severity, stable code, source span when available, `Reason:`, and `Fix:`.
- `diagnostic-registry-test-smoke` verifies code registry drift, but beta also requires representative quality checks for parser/lexer/backend/runtime messages.
- Parser and lexer JSON routing now preserves `stage`, `code`, `cause_ir`, and
  `fix_source` for `PGY_PARSE_SYNTAX` and `PGY_LEX_INVALID_TOKEN`; remaining
  debt is richer parser code splitting and parser multi-error accumulation.
- Intent clause explicit rejects for control-transfer constructs now preserve
  source span through parser AST nodes. `make diagnostics-json-test-smoke`
  covers `PGY_SEM_INTENT_STEP_INVALID` for `on: spawn ...` and
  `on: ch <- value` with line/column, `cause_ir`, and `fix_source`.

Cross-platform support matrix:

- Linux/WSL native: required beta gate for C + LLVM.
- Windows native/MSYS2/MinGW: required support matrix entry; LLVM may be marked unsupported until a real runner is green.
- macOS: C-only CI preflight is required through `make ci-macos`; macOS LLVM/backend parity remains out-of-beta until a dedicated LLVM support contract is green.
- 2026-04-25 local check: `make ci-windows` was not runnable in this WSL/Linux shell because `gcc -dumpmachine` reports `x86_64-linux-gnu`, not MSYS2/MinGW. This is an environment gap, not a code green signal; Windows beta evidence still requires a real MSYS2/MinGW runner.
- 2026-04-26 support-matrix guard: `WINDOWS_LLVM_READY` now requires executable `llvm-config --libs core` evidence. A `C:/Program Files/LLVM/lib` directory alone is not accepted as Windows LLVM support because it can make MSYS2 CI run LLVM smoke/backend-compare without runnable LLVM tooling and fail with command-not-found status 127.
- 2026-04-26 release wording: macOS now has a C-only CI preflight (`make ci-macos`) while macOS LLVM/backend parity remains out-of-beta.

Stdlib beta freeze:

- Source of truth: `docs/108_stdlib_beta_freeze.md`.
- The stable stdlib API list identifies beta-stable builtin helpers, stable
  `use` modules, known experimental modules, and out-of-beta ecosystem work.
- `make stdlib-test-smoke` gates stable builtin stdlib behavior and stable
  `use` module behavior on C and LLVM when both backends are requested.
- `type_checker_stdlib_use.c` must stay aligned with the public freeze list.

Tooling beta conformance:

- Stable tooling subset is executable through `make tooling-conformance-test-smoke`.
- LSP beta-stable: initialize capability response, keyword hover, and keyword completion.
- Formatter beta-stable: `--check` detects drift, `--write` is idempotent, and formatted code compiles.
- Debugger beta-stable: CLI `pgy debug <file>` parse + semantic gate and interactive quit path.
- Out-of-beta: DAP, binary breakpoints, variable watch, multi-file workspace indexing, refactor edits, full editor-grade diagnostic streaming.

Package/module resolver beta surface:

- Source of truth: `docs/109_package_module_resolver_contract.md`.
- Stable module surface is `import "relative/path.pgy";`, resolved relative
  to the importing file with namespace/export visibility and circular import
  rejection.
- Stable package surface is only `pgy init <name>` manifest/project
  scaffolding.
- `pgy install`, dependency version solving, lockfiles, registries, remote
  imports, checksum/signature verification, and supply-chain integrity are
  explicitly out-of-beta.
- `make package-module-resolver-test-smoke` gates the doc contract, `pgy init`,
  explicit `pgy install` rejection, and JSON diagnostics for module-load
  failures.

Test quality gate:

- Source of truth: `docs/111_beta_test_suite_freeze.md`.
- `make beta-test-suite-freeze-test-smoke` checks that the mandatory pre-beta
  gates have a stable target and remain listed in the freeze doc.
- Fuzz/property tests remain out-of-beta until they have a seed corpus,
  minimization policy, and proof-pack property mapping.
- Coverage percentage is not yet a beta acceptance metric; named stable-surface
  coverage is the beta gate.

Performance gate:

- Compile/runtime perf baselines must be captured before major CFG/DAG/runtime propagation rewrites.
- Regressions beyond the chosen threshold must block beta unless explicitly waived.
- `make perf-contract-test-smoke` gates the `perf_summary` log grammar and C/LLVM average/worst-case summary output so `test-abi-perf` evidence remains machine-readable.
- `make tooling-conformance-test-smoke` gates the tested formatter/LSP/debugger beta subset so tool maturity is not inferred from binaries merely existing.

Observability/tracing schema gate:

- Source of truth: `docs/112_observability_trace_schema.md`.
- Stable schema is intentionally narrow: `IntentLast*`, `IntentHistory*`,
  `IntentActive*`, `IntentRecent*`, authority failure snapshot
  (`ok/zone/participant/code/reason`), and backend-identical trace strings.
- Runtime string exports are `runtime-borrowed string` values: callers must not
  free them, and values are valid until the next registry/snapshot mutation.
- Rich event streaming, structured JSON trace export, distributed trace
  correlation, user-code registry hooks, stable binary trace format, and richer
  multi-instance timeline queries are explicitly out-of-beta.
- `make observability-schema-test-smoke` gates the C/LLVM stable schema
  fixtures.

Docs freeze:

- Language reference, getting-started tutorial, and migration/release notes must describe the frozen beta subset without overclaiming future surfaces.
- Documentation quality audit: `docs/116_documentation_quality_audit.md`
  records stale-path, mojibake, and async wording risks. User-facing docs should
  prefer 100-series source-of-truth contracts over older alpha-era design notes.

Memory/concurrency model gate:

- Source of truth: `docs/113_memory_concurrency_model.md`.
- Async positioning rationale: `docs/114_async_model_positioning.md`.
- Beta promise: Pergyra keeps suspension visibility but decomposes coloring;
  `await` is a completion join only, and `Future<T>` / `RemoteFuture<T>` are
  typed completion handles rather than a general user-level effect system.
- Stable contract: `parallel` joins before following control flow, accepted
  writes become visible after join, shared `ref`/`ref` reads are allowed, and
  `ref`/`own` plus `own`/`own` task-boundary conflicts are rejected.
- Stable channel contract: blocking send/receive is the ownership-transfer path;
  non-blocking/timeout receive, status send helpers, cancellation payloads, and
  channel close are copy-only for beta.
- Anonymous async spawn bodies, full weak-memory vocabulary, user-selectable
  memory orders, scheduler fairness guarantees, lock-free correctness claims,
  capture-bearing detached async block stability, and cross-thread `Arc<T>` /
  `Send` / `Sync` style trait systems are explicitly out-of-beta.
- `make memory-concurrency-model-test-smoke` gates the contract with
  `make async-model-positioning-test-smoke`,
  `parallel-core-contract-test-smoke`, and targeted C/LLVM backend compare for
  `parallel_channel_sum`,
  `parallel_channel_dual`, and `triple_paradigm`.

String/unicode policy:

- Source of truth: `docs/110_string_unicode_policy.md`.
- Beta-stable string policy is UTF-8 payload preservation in string literals and
  generated C/LLVM output.
- `StringLength` is byte-length for beta; equality/search are byte-exact and
  normalization-blind.
- Unicode identifiers, Unicode normalization, locale-sensitive comparison,
  case folding, collation, grapheme iteration, display width, and mixed-encoding
  source files are explicitly out-of-beta.
- `make unicode-policy-test-smoke` gates C/LLVM UTF-8 string execution and
  explicit Unicode identifier rejection.

Evidence command:

```sh
make diagnostic-registry-test-smoke
make parser-lexer-diagnostic-test-smoke
make module-taxonomy-test-smoke
make package-module-resolver-test-smoke
make unicode-policy-test-smoke
make beta-test-suite-freeze-test-smoke
make observability-schema-test-smoke
make memory-concurrency-model-test-smoke
make parallel-core-contract-test-smoke
make documentation-quality-test-smoke
```

## 0f. AIR Abstraction Safety Closure

Status: `BLOCKER`

Source of truth: `docs/104_air_compiler_architecture.md`

Goal:

- Pergyra의 killer 기능인 abstraction safety (intent ↔ implementation drift 검출) 에 명시적 verification IR 을 가진다.
- AST 기반 traversal 에 ownership 을 분산시켰다 사고친 패턴을 abstraction safety 도메인에서 반복하지 않는다. 분산된 metadata + cross-IR query 가 아니라 **단일 source of truth (AIR) + read-only synthesis** 로 푼다.
- 베타 후 ~1년간 코어 패치 freeze 가 예정되어 있으므로 AIR Phase 1 은 **문서 합의가 아니라 실 구현 완료 + 회귀 smoke 통과** 까지 닫는다.
- AIR 는 codegen path 위가 아니라 옆에 위치하는 **verification-only synthesis IR** 이다 (HIR + DIR + RIR → AIR, 단방향 read-only). codegen 출력에 영향이 없으므로 stale 위험이 codegen IR 보다 작다.
- 1.0 기준에서 AIR는 Pergyra의 abstraction-safety closure layer다. 단,
  타입/DAG, CFG/body safety, ownership, MIR cleanup, runtime propagation을
  대신하지 않는다. 각 layer가 자기 evidence를 만들고 AIR는 그 evidence가
  intent/zone/world/effect/IO/parallel/event/pin 계약과 일치하는지 감사한다.

Closed now:

- AIR 컴파일러 아키텍처 결정과 단방향 synthesis IR 포지셔닝이 `docs/104_air_compiler_architecture.md` 에 고정됐다.
- AIR 가 Rust MIR 과 의식적으로 다른 위치 (codegen path 옆) 에 산다는 architectural choice 가 명시됐다.
- Phase 1 / 2 / 3 scope 가 명시적으로 분리됐고, Phase 1 은 Intent Node + Boundary Node + 1 개 drift check 로 좁혀졌다.
- 1.0 AIR blueprint가 문서화됐다: Phase 1 beta는 `IntentNode` /
  `BoundaryNode` / strict evidence / drift facts를 닫고, 1.0은
  `EvidenceNode`를 1급화해 HIR CFG, DIR, RIR, MIR cleanup/pin, DAG
  generic/ability/module facts를 cross-layer로 감사한다.
- AIR 가 아닌 것 (codegen IR 아님, ownership/borrow 검사 home 아님, type 검사 home 아님, effect propagation 자체 아님, 새 keyword 추가 안 함) 이 명시적 negative space 로 docs 에 고정됐다.
- CFG 사고 (AST 기반 ownership 분산) 와의 동형 비교가 docs 에 고정되어, 같은 함정에 빠지지 않는 이유가 추적 가능하다.
- `src/compiler/air.h` defines the AIR Phase 1 data model,
  `src/compiler/air.c` owns DIR-based read-only synthesis, and
  `src/compiler/air_verify.c` owns global AIR validation plus sync/async drift
  and strict evidence diagnostics.
- AIR synthesis가 HIR routine evidence와 RIR boundary/authority evidence를 read-only로 수집하고 각 `Boundary Node`에 evidence flag를 부착한다. Default strict evidence에서 missing HIR CFG, RIR boundary, or RIR authority evidence는 `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`로 hard-fail 된다.
- 2026-04-29 AIR HIR provenance split: AIR boundary evidence now records
  `has_hir_routine_evidence` separately from `has_hir_cfg_evidence`. A lowered
  intent routine summary can still prove routine provenance, but only a routine
  with generated CFG containing the same boundary AST increments
  `hir_cfg_evidence_count` when a boundary AST is available. This closes the
  routine-only-vs-CFG-backed wording drift without changing public syntax.
- 2026-04-29 update: AIR evidence policy is exposed through
  `air_boundary_requires_hir_evidence(...)` and
  `air_boundary_requires_rir_evidence(...)`, and the driver diagnostic consumes
  those AIR facts to distinguish missing HIR CFG evidence from missing RIR
  boundary evidence instead of emitting a stale RIR-only explanation.
- 2026-04-29 HIR evidence tightening: `HIR_TOPLEVEL_INTENT` no longer grants
  blanket HIR evidence to every AIR boundary. HIR evidence must match the
  intent owner, step, or boundary source name. `test_air` now locks the negative
  case where an unmatched top-level intent routine is present but an
  implementation boundary still reports missing HIR CFG evidence.
- AIR read-only evidence is now regression-backed at the owner/evidence seam:
  `src/test_air.c` snapshots representative DIR step fields, HIR routine fields,
  and RIR scope/op/fact fields across `air_synthesize(...)`.
- `src/test_air.c`가 direct AIR 케이스와 parser/semantic/DIR/HIR/RIR source integration 케이스를 함께 고정한다: sync intent + sync boundary pass / sync intent + async boundary drift / async intent + async boundary pass / strict missing-boundary drift / mismatched authority participant drift / HIR+RIR evidence collection / parsed intent source no-drift.
- AIR synthesis now scans stable intent-step execution clauses (`using`,
  `intent`, `pre`, `guard`, `post`, `invariant`, `expect`, `on`,
  `compensate`) for `spawn` / `async` / `parallel`, `channel` / `select`, and
  stable resource IO/time calls. The current stable AIR boundary set is
  `FileOpen`, `FileRead`, `FileWrite`, `FileClose`, `ReadFile`, `WriteFile`,
  `Input`, `ReadLine`, `Now`, and `Sleep`. `Print` / `Log*` remain
  observability output calls, not AIR resource-boundary evidence in Phase 1.
  `src/test_air.c` covers AST-backed spawn boundary drift, IO `either`
  boundary non-drift, the stable execution boundary set (`parallel`, `async`,
  `channel-send`, `channel-recv`, `select`), and the full stable boundary
  builtin set so semantic builtin growth cannot silently bypass AIR.
- AIR drift messages are owned by AIR and `air_check_drift()` clears existing
  drift messages before recomputing; `src/test_air.c` covers repeated drift
  checking on the same AIRProgram so the validation path does not leak or retain
  stale diagnostics.
- AIR synthesis now hard-fails if the precomputed intent/boundary node counts
  diverge from the actual append counts. `make air-drift-test-smoke` gates the
  invariant so future boundary scanners cannot silently underfill or overrun the
  AIR node inventory.
- `air_verify(...)` is now the global AIR validation entry point. It validates
  AIR inventory invariants, authority participant shape, and evidence
  provenance before computing drift/evidence failures. `air_check_drift(...)`
  remains only as a compatibility wrapper.
- AIR inventory validation now rejects non-zero intent/boundary/drift counts
  without matching arrays and rejects boundary step-index drift from the
  referenced intent node before recomputing drift facts. `src/test_air.c`
  covers both crash-prevention paths.
- AIR inventory validation also rejects empty intent owner/step names, empty
  boundary owner/source names, boundary-owner mismatch against the referenced
  intent owner, and invalid boundary sync-class shape (`world`, `parallel`, and
  `channel` async; IO either-sync) before drift computation. These are
  `PGY_AIR_INVARIANT_INVALID` compiler IR failures, not user-facing drift facts.
- AIR drift inventory is validated before recomputation: stale drift nodes with
  placeholder kind, invalid intent/boundary references, or empty messages are
  rejected as `PGY_AIR_INVARIANT_INVALID` instead of being silently cleared.
- AIR evidence validation now rejects RIR authority evidence without prior RIR
  boundary evidence and rejects authority evidence on a non-authority boundary,
  keeping evidence provenance as a layered proof instead of a boolean flag.
- AIR inventory validation failures are now routed as
  `PGY_AIR_INVARIANT_INVALID` / `air:invariant:invalid` /
  `report-compiler-bug`, separate from user-facing
  `PGY_SEM_INTENT_BOUNDARY_*` drift diagnostics.
- AIR now owns synthesized intent/boundary/authority names instead of borrowing
  DIR/AST strings. Parsed-source AIR remains valid after DIR/parser teardown,
  and the parsed `where + transfer` regression asserts `PaymentZone` zone source
  plus `payment` world-handoff source.
- AIR expression-derived boundary nodes keep the expression span when available
  and fall back to the enclosing intent-step AST span when parser call nodes have
  no location. Parsed IO boundary regression ties the missing-evidence drift to
  the synthesized `ReadFile` boundary node instead of only checking that some
  drift exists.
- AIR dump/vocabulary ownership now lives in `src/compiler/air_dump.c`, and AIR
  validation/drift ownership now lives in `src/compiler/air_verify.c`, leaving
  `src/compiler/air.c` below the 600 LOC split-review threshold while keeping
  synthesis behavior focused.
- `where + transfer` no longer collapses to only a zone boundary. AIR emits a
  zone boundary for `where: Type` and a separate world boundary for the transfer
  handoff, with the world source anchored to the transfer target alias when
  present.
- World boundary evidence is now source/op-specific. A matching RIR intent
  scope alone does not discharge a transfer boundary; AIR requires RIR `Move` or
  `Claim` evidence for the boundary source alias.
- Implementation boundary evidence now requires HIR CFG proof. `parallel`,
  `channel`, IO, and execution boundaries cannot be discharged by RIR evidence
  alone; `src/test_air.c` gates this with the `AIR strict evidence requires HIR
  for implementation boundary` regression.
- HIR proof matching is source-specific; a top-level intent HIR routine is not
  accepted unless it matches the AIR owner/step/source identity.
- `tests/diagnostics_json_smoke.sh` now includes a parsed-source AIR negative
  case: a valid semantic intent requiring `authorized by: buyer` without a
  lowering-visible zone authority declaration is rejected by default strict AIR
  as `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`, with JSON
  `cause_ir`/`fix_source` and `expected authority participant(s): buyer`.
- `tests/diagnostics_json_smoke.sh` also covers a parsed-source execution
  boundary negative: an intent step calling `ReadFile(...)` is rejected by
  default strict AIR until AIR/RIR synthesis exposes real IO boundary evidence.
  This prevents owner-name-only RIR scope matching from falsely satisfying
  expression boundary evidence.
- Intent step source locations now flow parser → DIR → AIR, so AIR driver
  diagnostics for parsed sources report the offending step span instead of
  falling back to `line 0, column 0`.
- `docs/semantics/07_air_abstraction_safety.md`가 AIR synthesis read-only, Intent Node coverage, Boundary Closure, Drift Detection Soundness, Codegen Non-Impact proof obligation을 고정한다.
- `src/compiler/driver_app.c`가 AIR를 MIR lowering 전에 semantic-validation 단계로 실행하고, drift 발생 시 `PGY_SEM_INTENT_BOUNDARY_DRIFT` + `PGY_CAUSE_INTENT_BOUNDARY_DRIFT` + `PGY_FIX_ALIGN_INTENT_BOUNDARY_SYNC`를 text/JSON diagnostic으로 노출한다.
- `CompilerIRBundle`은 AIR를 담지 않는다. C / LLVM backend 가 AIR를 consume하지 못하게 막는 것이 Phase 1 설계다.
- `make air-drift-test-smoke`가 AIR source-of-truth 문서, checklist section, TODO readiness gate, Makefile wiring, AIR implementation/test/driver validation presence를 함께 검사한다.
- `make air-backend-nonimpact-test-smoke` compares generated C and LLVM text for
  the intent/zone, cross-world transfer, handoff frontier, world projection,
  relation/effect propagation, and authority-failure fixture set with relaxed AIR
  (`PGY_AIR_STRICT_EVIDENCE=0`) and default strict AIR, proving the no-drift AIR
  validation path does not mutate backend output.
- `make air-backend-nonimpact-full-test-smoke` runs the full frozen
  backend-compare fixture sweep (`PGY_AIR_NONIMPACT_SOURCE=all`) and is now the
  CI Linux AIR backend non-impact gate.
- `make air-strict-backend-compare-test-smoke` runs the normal C/LLVM backend
  execution compare under default strict AIR validation, so strict AIR
  validation is covered by real binary parity, not only generated text
  comparison.

Strict evidence update:

- Strict evidence is now the default AIR validation mode.
- Missing HIR CFG, RIR boundary, or RIR authority evidence becomes
  `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`, with dedicated
  `PGY_CAUSE_INTENT_BOUNDARY_EVIDENCE` and
  `PGY_FIX_ALIGN_INTENT_BOUNDARY_EVIDENCE`.
- Authority evidence is participant-sensitive: a boundary declared with
  `authorized by: X` is not satisfied by unrelated authority facts or authorize
  ops in the same RIR scope.
- AIR boundary evidence is now provenance-carrying: each boundary stores
  AIR-owned HIR routine, RIR boundary scope, and RIR authority participant names
  when evidence is found. This makes strict evidence failures debuggable without
  borrowing source IR lifetimes.
- AIR synthesis now records whether HIR input was present. In default strict
  evidence mode, any boundary synthesized with HIR input must have matching HIR
  routine provenance before it can be accepted as an abstraction-boundary fact.
  This is weaker than requiring HIR CFG proof for every boundary, but it closes
  the prior gap where RIR-only zone/world evidence could look complete even
  though AIR had no matching body-level routine owner.
- AIR strict-evidence diagnostics now print the same provenance summary
  (`evidence hir=... hir_cfg=... rir_boundary=... rir_authority=...`) in text/JSON output,
  so tooling does not need to infer which proof leg was absent.
- `air_dump()` now prints per-boundary evidence provenance names and the AIR unit
  suite gates that debug surface, so compiler-debug output stays aligned with
  text/JSON diagnostics.
- Parsed `where + transfer` coverage now requires both the emitted zone boundary
  and emitted world boundary to carry RIR boundary/authority evidence
  provenance, not just to exist.
- Authority evidence diagnostics include the expected authority participant
  list in `Reason:` when required RIR authority evidence is missing.
- `PGY_AIR_STRICT_EVIDENCE=0` remains as a development/debug opt-out for
  isolating AIR evidence coverage regressions; it is not the beta default.

Remaining (Phase 1 — beta 진입 전 반드시 실 구현):

- HIR + DIR + RIR synthesis edge coverage: stable intent subset evidence 누락은 default strict hard-fail로 승격됐다. Direct AST-backed execution-boundary coverage now exists for `spawn` and IO. Direct AIR coverage now verifies world boundaries require source-specific RIR transfer op evidence. Parsed-source positive coverage now verifies `where + transfer` emits both zone and world boundaries with owned source names. Parsed-source negative baseline now exists for missing authority evidence and missing IO execution-boundary evidence through the full driver JSON path. Remaining work is expanding source-backed negatives to transfer/world boundary drift and any later parsed execution-boundary drift that becomes semantically valid instead of pre-AIR rejected.
- backend non-consumption regression: source scanning and generated C/LLVM
  non-impact smoke now cover the full frozen backend-compare fixture set in
  Linux CI. Strict evidence also runs through the backend execution compare.
  Remaining work is Windows native evidence and additional parsed-source
  negative diagnostics beyond the current authority-evidence and IO-boundary
  evidence cases.
- Phase 1 invariant docs exist in `docs/semantics/07_air_abstraction_safety.md`
  for drift detection soundness, synthesis read-only, codegen non-impact,
  intent node coverage, boundary closure, and strict evidence failure
  soundness. Remaining work is expanding source-backed transfer/world boundary
  negative regressions, not writing the first invariant section.
- Phase 1 schema 가 Phase 2 (Constraint Node, Effect Node) 와 Phase 3 (drift fact 종류 확장) 를 막지 않도록 **future-compatible 하게 enum/struct** 설계 (`drift_kind` 가 enum 1 종에서 시작해 추가 가능, Boundary `kind` 가 5 종에서 시작해 추가 가능).
- AIR 를 codegen path 에 연결하지 않는다. AIR drift 검사는 semantic/compiler validation 단계에 머물고 C / LLVM output을 직접 바꾸지 않는다.

Out of Phase 1 (베타 후 생태계 단계에서 추가 가능):

- Phase 2: Constraint Node (sync/async, local/distributed, fallible/infallible, persistence), Effect Node (DB/Network/FS/Extenal), 추가 drift fact (failure-class mismatch, transactional-scope mismatch).
- Phase 3: AIR 가 안정되면 일부 metadata 의 단일 source-of-truth 화 (예: zone boundary 정보가 DIR 와 AIR 양쪽에 있던 것을 AIR 단일화).

Evidence command:

```sh
make air-drift-test-smoke
make air-backend-nonimpact-test-smoke
make air-backend-nonimpact-full-test-smoke
make air-strict-backend-compare-test-smoke
make formal-semantics-test-smoke
make diagnostic-registry-test-smoke
make llvm-test-backend-compare
```

## 0g. Compiler Design Quality Verification

Status: `IN PROGRESS / VERIFICATION`

Source of truth: this section + cross-reference into `docs/19` §0 (systems
language identity), `docs/20_compiler_pipeline_guide.md`, and
`docs/118_slot_model_rigor_audit.md`.

Goal:

- Pergyra의 컴파일러 구현이 production-grade design quality를 만족함을
  명시적으로 검증한다.
- CS 석사-tier 평가 체크리스트 (textbook 4가지 패턴) 와 modern production
  컴파일러 architecture 기준 (multi-IR, pattern dispatch, persistent scope,
  rich diagnostics) 둘 다 통과해야 한다.
- 베타 closure 직전 미통과 항목은 lift 또는 명시적 out-of-beta로 분류한다.
- 이 검증은 *compiler engineering quality*에 대한 것이고, runtime safety
  (docs/security/), formal semantics (docs/semantics/), abstraction
  portability (docs/117) 는 별도 layer 이다.

### Textbook checklist (CS 석사-tier 평가)

학생/junior 컴파일러 평가 시 표준 4점 체크리스트와 Pergyra 매핑:

| Textbook 제약 | 의도 | Pergyra 답 | 상태 |
|---|---|---|---|
| AST nodes are immutable (분석이 트리 오염 안 함) | 파싱 결과 보존 | 5-IR pipeline (AST → HIR → DIR → RIR → MIR). AST는 read-only entry IR이고 분석은 다음 IR에서 진행. mutate 안 함 | ✅ 교과서 답 *초과* |
| AST와 분석 로직 디커플링 (Visitor / Double Dispatch) | 데이터-로직 분리 | C tagged-union + switch on `node->kind`. AST는 class hierarchy 아니라 tagged union이므로 method 첨부 자체가 불가능. Visitor 흉내 안 함 | ✅ 교과서 답 *idiom 적합* |
| 심볼 테이블 = HashMap stack (블록 스코프 shadowing) | 변수 가시성 | **Frame chain + flat array per scope, linear scan lookup** (`src/semantic/symbol_table.c`, 296 LOC). textbook의 HashMap-of-HashMap 보다 단순. 작은 scope에는 cache-friendly로 우월할 수 있고, 큰 scope에는 lift 필요 | 🟢 audit 완료 / 큰 scope 워크로드에서 lift 후보 |
| CompilerDiagnostic 객체 (line/col/hint, exception 아님) | 진단 누적 | `diag_codes.h` 100+ stable codes + level/stage/`Reason:`/`Fix:`/span/multi-span/JSON 회귀 (`diagnostics-json-test-smoke`) | ✅ 교과서 답 *대폭 초과* |

→ **4점 textbook 중 3개 *초과*, 1개 audit 필요 (스코프 implementation pattern).** textbook 채점은 대부분 통과.

### Production architecture 기준 (textbook 너머)

modern 컴파일러 (rustc, Clang, TS, GHC, Roc) 가 textbook 4점을 *넘어서*
공통적으로 가진 추가 criteria 와 Pergyra 매핑:

| Production criterion | Pergyra 답 | 상태 |
|---|---|---|
| Multi-IR pipeline (single AST analysis pass 아님) | AST/HIR/DIR/RIR/MIR + AIR (verification IR) = 6 IR | ✅ |
| Pattern matching dispatch (Visitor 안 씀) | C tagged union switch (idiomatic in C; pattern matching 등가) | ✅ |
| Persistent / versioned scope (naive HashMap-of-HashMap 아님) | **Frame chain + flat array per scope** (linear scan). 작은 scope에 cache-friendly, 큰 scope에서는 hash 기반 production 컴파일러 (rustc/Clang/GCC) 보다 느림 | 🟢 audit 완료 / 베타 acceptable / 프로파일링 후 lift 후보 |
| Stable diagnostic codes (free-text 아님) | `PGY_*` 100+ stable codes, `diag_codes.h` | ✅ |
| Span representation (range + snippet, line/col만 아님) | **point-span only** (`Diagnostic` line/col 단일, ASTNode도 line/column 단일). end_line/end_column 또는 byte range 미지원. snippet 렌더링은 별도 도구가 line/col로 부분 재구성 | 🟡 lift 후보 (범위 표현 추가) |
| Multi-span 진단 (offending site + related def site) | **API single-span only**. `Diagnostic` 단일 line/col, `semantic_error*` 단일 `node`. 일부 site (slot_analyzer)가 메시지 텍스트에 prior line 숫자 임베드하지만 JSON/LSP consumer는 추출 불가. 대부분 diagnostic은 prior site 0 정보 (e.g., class redeclaration) | 🔴 명시적 out-of-beta / LSP 통합 시 lift |
| Suggestion / fix-it 힌트 | `Reason:` / `Fix:` 구조 + auto-fix 일부 | 🟡 |
| Error recovery (parse-through-error / sema-through-error) | 부분 — 베타 closure에서 lift 후보 | 🟡 |
| Diagnostic JSON 회귀 (진단 자체가 stable) | `make diagnostics-json-test-smoke` | ✅ |
| Side-table for analysis (mutable AST annotation 아님) | IR pipeline이 자체적으로 side-table 역할 (각 IR이 별도 store) | ✅ |

→ **10개 production criteria audit 결과: 5 ✅ + 1 🟢 (스코프 audit 완료) + 3 🟡 lift 후보 + 1 🔴 명시적 out-of-beta:**
- 스코프 pattern: 🟢 audit 완료 (frame chain + flat array, 베타 acceptable, 프로파일링 후 lift)
- Span 범위 표현: 🟡 point-span only, range/snippet 추가 lift 후보
- Multi-span 진단: 🔴 명시적 out-of-beta (API 자체 미지원, LSP 통합 시 lift)
- Fix-it 확대: 🟡 lift 후보 (LSP 통합과 묶음)
- Error recovery 확대: 🟡 lift 후보

### Closed now

- 5-IR + AIR 6-IR pipeline 운영 (AST/HIR/DIR/RIR/MIR + AIR)
- C tagged union switch dispatch가 모든 semantic / codegen pass의 표준
- 100+ stable diagnostic codes, level/stage/Reason/Fix 구조 강제
- Diagnostic JSON 회귀 gate (`make diagnostics-json-test-smoke`)
- Span + 부분 multi-span
- AST는 mutate 안 됨 — 분석 결과는 HIR+에 들어감 (immutable AST 교과서 답 초과)
- Visitor pattern 흉내 안 함 — C에 idiom-적합한 tagged union switch 채택
  (Visitor는 OOP without pattern matching의 workaround이므로 C에서는
  anti-idiomatic)

### Remaining

- **스코프 manager pattern audit** — *완료 (2026-04-27).* 패턴은 *(e) Frame
  chain + flat array per scope*, linear scan lookup
  (`src/semantic/symbol_table.c` 296 LOC). textbook (a)~(d) 어느 쪽도 아니고
  *더 단순한 (e)*. 작은 scope에서는 cache-friendly로 hash 기반보다 *빠를 수
  있음*. 큰 scope (전역에 수천 symbol, 큰 함수에 수백 local) 워크로드에서는
  hash 기반으로 lift 필요. **베타 acceptable, 프로파일링이 hot path 보일 때
  lift 후보.** Lift 시 권장 패턴: linear-array 일정 크기까지 유지하다가
  threshold 넘으면 hash로 promote (Clang/GCC와 유사).
- **Multi-span 진단 audit** — *완료 (2026-04-27).* `Diagnostic` struct가 단일
  line/col만 갖고 `semantic_error*` API가 단일 `node`만 받음. 즉 *API 자체에
  multi-span 미지원*. 일부 site (예: `slot_analyzer.c`)는 메시지 텍스트에
  prior line 숫자를 임베드하지만, JSON consumer / LSP / IDE 도구는 그 secondary
  위치를 추출 불가. 대부분 diagnostic (class/ability/role redeclaration 등)은
  prior site 정보를 *전혀* 안 줌. **베타 closure 결정: 명시적 out-of-beta.**
  Lift 비용 ~2-3일 (Diagnostic struct 확장 + API 추가 + 핵심 5-10개 마이그레이션
  + JSON 회귀 + 텍스트 렌더러). LSP 통합 작업과 묶어서 한 번에 처리 권장.
- **Span 범위 표현 audit** — *완료 (2026-04-27).* ASTNode와 Diagnostic 모두
  단일 line/column만 보유. end_line/end_col 없음 → 범위 표현 미지원. snippet
  렌더링은 도구가 line/col로 부분 재구성. **lift 후보 (베타 acceptable, post-1.0
  LSP/IDE 통합 시 lift).**
- Fix-it / suggestion 확대 — `Reason:` / `Fix:` 구조에 textual fix는 있는데,
  *machine-applicable* fix-it (rustc `--fix` 같은 자동 적용) 은 없음.
  베타 closure 후 LSP 통합 시 lift 후보.
- Error recovery 확대 — parse-through-error / sema-through-error / type-error
  recovery. 현재 부분. 베타 closure 안에 어디까지 lift할지 결정.
- `compiler-design-quality-test-smoke` 신설 — 위 항목들을 자동 검증하는 회귀.
  현재 `make diagnostics-json-test-smoke` 가 진단 layer만 cover하고, 스코프
  pattern / IR pipeline shape는 별도 검증 필요.

### Out of scope (명시적 reject)

- **Visitor pattern wrapper 도입**. C에서 anti-idiomatic. tagged union +
  switch가 production-correct. Visitor 강제 시 N×M class explosion + indirect
  dispatch overhead + cache miss + 새 노드 추가 시 모든 visitor 깨짐. 교과서
  답이지만 implementation language (C) 와 맞지 않으므로 의도적 reject.
- **Mutable AST annotation**. 5-IR pipeline 전체 거부. 분석 결과는 다음 IR
  store로 가지 AST에 leak되지 않음.
- **Single-pass interpreter-style 분석**. 5-IR pipeline 정체성과 충돌.

### Evidence command

```sh
make test-semantic
make diagnostics-json-test-smoke
make diagnostic-registry-test-smoke
make ir-pipeline-test-smoke
# 신설 후보:
# make compiler-design-quality-test-smoke
```

## 0h. Type-Resolution DAG Closure

Status: `IN PROGRESS / BLOCKER`

Source of truth: `TODO.md` type-resolution DAG section,
`tests/type_resolution_dag_smoke.sh`, and
`tests/type_resolution_resolver_inventory_smoke.sh`.

Goal:

- Stable type refs must resolve through graph/topo metadata rather than
  owner-local recursive resolver fallback.
- Declaration order, generic defaults, where bounds, ability consumers, module
  contracts, zone/world authority consumers, and alias cycles must share one
  dependency vocabulary and one diagnostic provenance model.
- The retired recursive resolver implementation must stay absent; only audit
  counters may remain for zero-call reporting.

Closed now:

- DAG graph/topo inventory is active and smoke-gated.
- Owner-local resolver seams are gone: new direct `resolve_type_node(...)`
  calls outside the resolver body / metadata owner fail
  `type-resolution-resolver-inventory-test-smoke`.
- Central metadata materializer fallback is dormant in the semantic suite:
  `materializer_fallbacks=0`.
- Current local stats are `graph-backed skips=3140`,
  `resolve_calls=0`, `resolve_unique_nodes=0`,
  `metadata_entries=3436`, `metadata_owned=257`,
  `metadata_hits=6756`, and `materializer_fallbacks=0`.
- Metadata fallback families are all zero, including named, generic-named,
  compound, other, builtin shell, generic class, alias, non-class symbol, and
  missing-symbol fallback.
- Alias compatibility surface is closed at the DAG stage: `compat_alias=0`,
  `compat_non_alias=0`, `alias_materialized=6`,
  `alias_diagnostic_unresolved=78`, and
  `alias_diagnostic_resolver_calls=0`.
- Valid alias stage replay now uses metadata-only lookup before the quiet
  diagnostic unresolved path. The DAG smoke gates `compat_alias == 0`, so any
  alias replay that leaks back into the recursive resolver path fails the beta
  DAG contract.
- Non-metadata `semantic_type_resolution_lookup_resolved_annotation(...)`
  readers are now smoke-gated at exactly 12. They are not recursive resolver
  fallback, but they remain a narrow source-of-truth seam for effective generic
  defaults and ability references until those annotation facts move into DAG
  metadata.
- Central metadata materialization no longer falls through to
  `resolve_type_node(type_node, ctx)`. Unsupported shapes are recorded as
  explicit fallback inventory and return unresolved; the DAG smoke keeps
  `materializer_fallbacks=0`, and resolver-inventory smoke gates recursive
  fallback escape hatches at zero.
- Metadata alias chain and cycle handling now has a dedicated owner:
  `type_checker_resolution_metadata_alias.c` owns alias-chain materialization,
  cycle formatting, and `semantic_type_resolution_lookup_metadata_name_or_alias(...)`.
  The central metadata owner is now orchestration-only for lookup and
  materialization dispatch.
- The previous recursive alias resolver and `SemanticContext.alias_resolution_*`
  stack are removed. Direct named alias resolution now goes through the same
  metadata alias owner as staged alias replay.
- `resolve_named_type(...)` is now metadata-first for stable builtin, scope,
  generic-parameter, nominal, and alias names. It only falls back to the old
  diagnostic path when DAG metadata cannot answer the named string.
- Stable named builtin/shell lookup is no longer duplicated in the
  compatibility helper. `resolve_named_type(...)` delegates scalar builtin and
  stable shell recognition to
  `semantic_type_resolution_metadata_named_builtin_or_shell_singleton(...)`,
  and `type-resolution-resolver-inventory-test-smoke` rejects reintroducing a
  local `strcmp(name, "...")` builtin/shell table in
  `type_checker_resolution_helpers.c`.
- 2026-04-29 update: the metadata type-ref API now materializes stable
  constructed refs before the compatibility resolver can run, and
  `semantic_stage_resolve_type_quiet(...)` consumes that same type-ref API
  before compatibility fallback accounting. This closes a small but important
  signature-stage seam: constructed stable refs reached by compatibility
  callers stay DAG-metadata-first instead of silently reopening recursive
  resolver fallback.
- 2026-04-29 API update:
  `semantic_type_resolution_lookup_type_ref_or_materialize(...)` is the named
  semantic-owner API for "metadata-first, diagnostic-materializer second" type
  refs. It prevents each checker owner from hand-rolling the same preflight and
  makes the remaining compatibility seam easier to gate.
- 2026-04-29 domain seam update: intent participant/value/where type refs and
  zone authority subject-slot type refs now consume that API. Ability where,
  class/function signature, action contract, domain slot, and world slot refs
  use the same helper. Expression/member/operator annotation refs, generic
  default/contract refs, async channel parameter refs, ownership refs, and
  projection path refs also use it. `type-resolution-resolver-inventory-test-smoke`
  now fails if a semantic owner bypasses the metadata-first helper and calls the
  diagnostic materializer directly; only central metadata/diagnostic
  compatibility owners may call `semantic_type_resolution_lookup_or_materialize(...)`.
  This keeps the first semantic-owner compatibility seam metadata-owned without
  starting the beta+1 Domain AST -> Core AST rewrite.
- Semantic regression now covers provider-after-consumer alias materialization
  for a nested constructed alias (`Later = Channel<Slot<Int>>`) consumed by a
  function signature before the alias declaration.
- The resolver inventory smoke now also gates the materializer fallback
  recorder and rejects recursive metadata escape hatches at zero.

Remaining:

- Keep the recursive resolver retired as an evaluator source for stable type
  refs. The central metadata escape hatch and private compatibility body are
  removed; remaining work is to keep every non-semantic driver/backend path on
  DAG/topo facts and prevent evaluator-body reintroduction.
- Keep provider-after-consumer generic/default/ability/module/zone-world
  regressions in semantic and C/LLVM parity suites.

Evidence command:

```sh
make type-resolution-dag-test-smoke
make type-resolution-resolver-inventory-test-smoke
make test-semantic
```

## 1. Core Module / Module Boundary Closure

상태: `IN PROGRESS / BLOCKER`

목표:

- core / foundation / execution / compatibility surface가 문서와 테스트에서 같은 경계로 유지된다.
- 장기 모듈화는 `.inc` 제거율 자체가 아니라 owner boundary와 dependency direction을 명확히 한다.
- 신규 core 변경이 multi-thousand-line include fragment를 직접 수정하지 않아도 된다.

현재 닫힌 것:

- 2026-04-25 local acceptance: `make ci-linux` completed green on WSL/Linux after the DAG metadata floor gate, fallback seam cap reduction to 21, CFG/body dataflow gate, runtime panic contract gates, authority direct-slot fixes, parser/lexer JSON routing, AIR strict-evidence/default non-impact gates, AIR drift-message ownership cleanup, C/LLVM MIR declaration inventory gate, and C backend active-inventory bootstrap. This covers `test-all`, LLVM smoke, fmt/stdlib/module/example smoke, taxonomy/inc-size/core-shape/DAG/MIR-inventory/diagnostic/parser-lexer/JSON/IR/AST/AIR gates, ABI same-process, and backend compare.
- 2026-04-26 LLVM projection parity update: current-zone subject method calls now dirty and sync zone projection targets in the LLVM backend, matching the C backend for `campaign_graph_fsm`. `make llvm-campaign-projection-test-smoke` locks the exact stdout against the stable C expectation.
- 2026-04-26 DND campaign parity update: LLVM MIR `with slot` lowering now emits claim setup without re-emitting the already-flattened body, and the LLVM field registry cap now covers larger zone/class layouts with hidden projection fields. `make llvm-dnd-campaign-test-smoke` locks exact C/LLVM stdout, one epilogue, five choice lines, and final `ready=true/true`.
- `docs/99_language_module_taxonomy.md`로 core/foundation/execution/compat layer를 고정했다.
- `docs/language_module_manifest.json`, `docs/language_module_cases.json`가 machine-readable source다.
- `make module-taxonomy-test-smoke`가 taxonomy drift를 검사한다.
- semantic leaf/helper split이 진행되어 diagnostics, ownership, generic, ability, module contract, DAG primitive/collector/label/domain/body/decl/world/stage 일부가 실제 `.c` translation unit으로 이동했다.
- `type_checker_ability_decl.c`, `type_checker_zone_decl.c`, `type_checker_world_decl.c`는 standalone semantic TU로 빌드된다.
- `type_checker_intent_decl.c`도 standalone semantic TU로 빌드되며, helper boundary 누락은 기본 CFLAGS의 implicit-declaration hard error로 차단된다. Intent authority validation now has a named owner in `type_checker_intent_authority.c`, so the declaration owner no longer carries the full authority/authorized-by diagnostic body.
- `type_checker_role_decl.c`, `type_checker_party_decl.c`, `type_checker_roster_decl.c`도 standalone semantic TU hard-CFLAGS path에서 빌드된다.
- `type_checker_resolution_graph_inventory.c`가 graph inventory axis를 소유한다. 기존 `type_checker_resolution_graph_inventory.inc`는 제거됐다.
- `type_checker_resolution_stage_domain.c`가 world/zone local-contract stage replay를 소유하고, `type_checker_resolution_stage_signature.c`가 generic/ability/function/event signature staging을 소유한다. `type_checker_resolution_stage_alias.c`는 alias diagnostic unresolved accounting과 trace를 소유하고, `type_checker_resolution_stage_nominal.c`는 class/enum/ability/role nominal replay를 소유한다. `type_checker_resolution_stage_systemic.c`는 party/roster/world/intent replay를 소유하고, `type_checker_resolution_stage_domain_decl.c`는 relation/effect/zone declaration replay를 소유한다. `type_checker_resolution_stage.c`는 88 LOC top-level dispatch owner가 됐고, split owner들도 239 LOC 이하라 모두 600 LOC split-review threshold 아래에 있다. `type_checker_resolution_stage.inc`는 제거됐다.
- `type_checker_class_decl.c`가 class/extern declaration checking을 소유하고, `type_checker_program.c`가 top-level semantic orchestration을 소유한다. `type_checker_program.inc`는 624 LOC까지 줄어 semantic 800 LOC stop condition 아래로 내려갔다.
- `type_checker_builtins_projection.c`가 `ToObject` / `ToTObject` projection diagnostics를 소유하며, `type_checker_builtins_nominal.inc`는 659 LOC까지 줄어 semantic 800 LOC stop condition 아래로 내려갔다.
- `type_checker_expr_ops.c`가 binary/unary/array literal/indexed access를 소유하고, `type_checker_expr_names.c`가 static member path / consumed-boundary name helper를 소유한다. `type_checker_expr.inc`는 758 LOC, `type_checker_helpers_late.inc`는 773 LOC까지 줄어 semantic 800 LOC stop condition 아래로 내려갔다.
- `type_checker_ownership_return.c`, `type_checker_ownership_assign.c`,
  `type_checker_ownership_array_store.c`, `type_checker_ownership_boundaries.c`,
  `type_checker_ownership_call.c`, `type_checker_ownership_destructure.c`,
  `type_checker_ownership_let.c`, and `type_checker_ownership_param_summary.c`
  now own return, assignment rebind, array-literal store, boundary validation,
  call-argument, destructuring, let-binding, and parameter escape-summary
  ownership consumers. The old behavior-owning ownership `.inc` files were
  deleted; `src/semantic/type_checker_ownership_*.inc` is now zero.
- `type_checker_decls_domain_helpers.c`가 domain slot/projection/overlay helper body를 소유한다. `type_checker_decls_domain_helpers.inc`는 제거됐다.
- `type_checker_intent_helpers.c`가 intent inheritance/derivation/helper body를 소유한다. `type_checker_decls_a.inc`는 1-line forwarding stub으로 축소됐다.
- `type_checker_event.c`가 event declaration/subscription/invoke semantic을 소유한다.
- `type_checker_qubit.c`가 QubitSlot compile-time state, entangle pool, movable-resource-use validation을 소유한다.
- `type_checker.c`는 481 LOC로 내려갔고, semantic stop condition의 600 LOC 이하 조건을 만족한다.
- `type_checker_slot_view_boundary.c` now owns active slot view boundary
  diagnostics; `type_checker_helpers_late.c` is back under the semantic TU hard
  cap at 974 LOC.
- `slot_type_utils.c` now owns runtime type-tag/hash/CAS/memory-barrier utility
  exports; `slot_manager.c` is no longer the owner for those ABI-neutral helper
  routines.
- `slot_manager_secure_ops.c` now owns secure manager enable/disable, secure
  claim/read/write/release, token validation/refresh/revoke, and secure manager
  lifecycle wrappers. `slot_manager_query_lock.c` owns type/validity queries,
  TTL cleanup, lock/unlock/try-lock, stats, and fast wrappers.
  `slot_manager.c` is down to 564 LOC and stays focused on claim/read/write/
  release lifecycle plus shared storage helpers.
- `pgy_parallel.h` is now a 494 LOC shared task/await facade. Blocking pool
  lifecycle moved to `pgy_parallel_blocking.h` at 146 LOC, and coroutine
  scheduling moved to `pgy_parallel_coroutine.h` at 292 LOC. The split keeps the
  header-only runtime ABI stable and removes `pgy_parallel.h` from the 600 LOC
  split-review queue.
- `parser_intent.c` is now a 468 LOC declaration/default propagation owner.
  Step clause parsing moved to `parser_intent_step.h` at 297 LOC, so the intent
  parser surface is below the 600 LOC split-review threshold without changing
  parser exports.
- `parser_expr.c` is now a 524 LOC expression precedence/call/primary owner.
  Multiline/interpolated string helpers moved to `parser_expr_string.h` at
  150 LOC, removing the expression parser from the 600 LOC split-review queue.
- `slot_pool.c` is now below the 600 LOC split-review threshold and focuses on
  pool/list allocation. Timestamp, cache prefetch/alignment, and linked-list
  benchmark helpers moved to `slot_pool_perf.c`; `test-datastructures`,
  `test-abi`, header-size, and inc-size smoke gates are green.
- `rir_builder.c` is now a 281 LOC scope-orchestration owner. AST body walking,
  slot/call/resource op materialization, and block-condition walking moved to
  `rir_builder_walk.c` at 363 LOC; RIR/AIR/MIR tests and header/inc smoke gates
  are green.
- LLVM hosted domain method forward declarations now consume `MIRDeclMethod`
  metadata first for name, params, and return type before falling back to the
  temporary AST payload. `mir-declaration-inventory-test-smoke` rejects direct
  AST method `param_count` / `return_type` reads in that forward-declaration
  section.
- `type_checker_domain_role_lookup.c` now owns subject-role ability lookup used
  by party/role contract checks. `type_checker_decls_domain_helpers.c` is down
  to 1581 LOC; the remaining split candidates are projection, effect, and
  relation contract diagnostics.
- `type_checker_flow_match.c` now owns match pattern binding, match
  exhaustiveness, redundancy, and total-coverage lattice checks. The CFG/body
  flow owner `type_checker_flow.c` is down to 435 LOC and focuses on
  branch/join, loop/defer/parallel boundary, return, and unreachable flow
  orchestration. `semantic-core-shape-test-smoke` keeps both flow owners under
  the 600 LOC split-review threshold.
- `mir_cleanup.c` no longer scans intent-step AST fields to decide
  invalidation cleanup. Rollback/invalidation block creation now consumes
  RIR policy ops, conservative semantics, flow-block summaries, and resource
  facts; the CFG/body gate rejects reintroducing the AST fallback.
- `rir_flow_state.h` now owns the RIR resource-state merge lattice and helper
  predicates. `rir_flow.h` is down to 420 LOC and focuses on HIR CFG
  enrichment, flow-block preparation, and bounded dataflow iteration.
- `make semantic-inc-size-test-smoke`가 `src/semantic` production `.inc = 0`를 검사한다.
- `make semantic-core-shape-test-smoke`가 `type_checker.c <= 600 LOC`, DAG inventory `.c` ownership, event/qubit owner TU 존재를 검사한다.
- `.inc` cleanup is closed for the full `src` tree: `src/runtime`,
  `src/codegen`, `src/compiler`, `src/semantic`, and `src/tests` now have
  **0 `.inc` files / 0 LOC**. Test fragments use `.cases.h` instead.
- Former production shims are named private owner headers:
  `transpiler_mir_inventory_ssa_emitters.h`,
  `transpiler_expr_emitters.h`, `llvm_expr_call_owners.h`,
  `transpiler_base_a_emitters.h`, `transpiler_base_b_emitters.h`,
  `transpiler_helpers_core_{a,b}.h`, `transpiler_domain_role_emit.h`, and
  `pgy_runtime_inline_core.h`.
- Remaining backend debt is no longer pass-through `.inc` debt; it is owner
  extraction inside named headers/TUs. LLVM statement parallel/async/select
  lowering is now isolated in `llvm_stmt_parallel_async.c`, and LLVM intent
  MIR metadata, zone binding/sync helpers, effect provenance helpers, and
  MIR-backed intent flow/signature helpers are isolated in
  `llvm_intent_mir_meta.c`, `llvm_intent_zone.c`, `llvm_intent_effect.c`, and
  `llvm_intent_flow.c`. Parser orchestration/declaration debt is no longer a
  1,000+ LOC blocker: `parser.c` is 977 LOC and `parser_decl.c` is 887 LOC
  after extracting doc comments, export dispatch, enum parsing, pin-block
  parsing, lexical-zone context propagation, declaration-clause parsing, and
  named-declaration lookahead into real TUs. Remaining owner extraction should
  target parser AST / AST-print / domain owners, the largest semantic domain
  helpers, and body-loop subowners inside `llvm_emit_intent_decl`.
- LLVM domain helper extraction continues without reintroducing `.inc`:
  method lookup, implicit-self classification, operator alias helpers, and
  propagation provenance stamping live in `llvm_domain_method_helpers.c`.
  World sync now lives in `llvm_domain_world_sync.c`; zone bounded-frontier
  sync now lives in `llvm_domain_zone_sync.c`. `llvm_domain.c` is below the
  1,000 LOC hard cap; remaining domain debt is split-review readability around
  declaration/type orchestration, not hidden include-order execution.
- LLVM intent zone binding extraction now lives in `llvm_intent_zone.c`:
  zone slot-name resolution, bound-zone materialization, handoff transfer
  tracing, projection dirty/ready stamping, effective-zone sync, and alias
  restore are no longer mixed into the intent declaration orchestration file.
  LLVM intent effect provenance extraction now lives in
  `llvm_intent_effect.c`: caused-effect inference and layer/state epoch/cause
  stamping are no longer mixed into the intent declaration orchestration file.
  LLVM intent flow extraction now lives in `llvm_intent_flow.c`: MIR routine
  lookup, MIR step/check/eval/dispatch collection, MIR resource hooks,
  authority validation, and forward declaration signature generation are no
  longer mixed into the body emission owner. `llvm_intent.c` drops from 2,118
  LOC to 953 LOC, below the 1,000 LOC hard cap, while `llvm_intent_flow.c`
  stays below the 600 LOC split-review threshold at 563 LOC.
  `make LLVM_ENABLED=1 /tmp/pgy-PergyraLang-bin/pgy llvm-test-backend-compare`
  remains green with 196 ABI checks and 53/53 backend-compare cases.
- `make production-header-size-test-smoke` prevents the new named owner
  headers from becoming replacement mega-includes. The default cap is 1,000
  LOC and no production header has a temporary allowance. LLVM declaration
  inventory helpers now live behind `llvm_inventory_internal.h`, with lookup
  and host-method metadata split into dedicated helper owners, leaving
  `llvm_internal.h` as context declarations plus public backend contracts.
- MIR slot/claim type helper extraction now lives in
  `src/compiler/mir_type_helpers.c` / `.h`, reducing `src/compiler/mir.c`
  without changing lowering behavior. `make test-mir` and
  `make mir-declaration-inventory-test-smoke` cover the split.
- C MIR block emission is no longer a replacement mega-header: destructuring
  lowering, preserved source-let fallback emission, and source-order/claim
  scheduling now live in `transpiler_mir_destructure_emit.h`,
  `transpiler_mir_fallback_let_emit.h`, and
  `transpiler_mir_block_schedule_emit.h`. The main
  `transpiler_mir_block_emit.h` owner is below the 600 LOC split-review
  threshold.
- MIR emission contract and inventory debt were split further:
  resource/cleanup hook emission now lives in
  `transpiler_mir_resource_hook_emit.h`, and SSA name-map utilities plus MIR
  block mapping comments now live in `transpiler_mir_ssa_map.h`.
  `transpiler_mir_emission_contract.h` and
  `transpiler_mir_inventory_intent.h` are both below the 600 LOC split-review
  threshold.
- LLVM member-call method debt is split further: vtable dispatch now lives in
  `llvm_expr_call_methods_vtable_dispatch.h`, leaving
  `llvm_expr_call_methods_domain_slice.h` below the 600 LOC split-review
  threshold while preserving include order and C/LLVM backend parity.
- LLVM general call dispatch is no longer a stdlib mega-dispatcher:
  string/file/io/time builtin emission now lives in
  `llvm_expr_stdlib_scalar_io_calls.h`, leaving
  `llvm_expr_call_dispatch.h` below the 600 LOC split-review threshold.
- C backend builtin dispatch is no longer the intent observability export
  owner: intent last/history/active/recent builtin emission now lives in
  `transpiler_intent_observability_builtin_emit.h`, leaving
  `transpiler_expr_builtin_dispatch.h` below the 600 LOC split-review
  threshold.
- C backend expression call/spawn ownership is split further: spawn wrapper
  emission and channel send/receive emission now live in
  `transpiler_spawn_channel_emit.h`, leaving
  `transpiler_expr_call_spawn_emit.h` below the 600 LOC split-review threshold.
- MIR cleanup/rollback/invalidation edge ownership now lives in
  `src/compiler/mir_cleanup.c` / `.h`. Cleanup block creation, rollback-policy
  invalidation, and cleanup edge materialization are no longer mixed into
  `src/compiler/mir.c`; `make test-mir` and
  `make mir-declaration-inventory-test-smoke` cover the split.
- HIR analysis extraction now lives in `src/compiler/hir_analysis.c` / `.h`.
  Type-reference, direct-call, and control-flow-presence analysis no longer
  lives in the top-level HIR lowering owner. `make test-hir`, `make test-rir`,
  `make test-mir`, and `make air-drift-test-smoke` cover the split.
- Tier 1 runtime/codegen/compiler `.inc` split gate를 닫았다. Pass-through shim `.inc` files for runtime part B, LLVM expr helpers, LLVM method calls, LLVM domain helpers, MIR public API, and C transpiler emitter/helper seams have now been removed; owning `.c` / `.h` files carry named owner seams directly. 600 LOC is the split-review threshold; 1,000 LOC is only the hard cap.
- `make backend-inc-size-test-smoke`가 `src/runtime`, `src/codegen`, `src/compiler`의 production `.inc = 0`를 검사한다. It also rejects the old RIR implementation-style headers (`rir_builder.h`, `rir_flow.h`, `rir_names.h`, `rir_public_surface.h`, `rir_validation.h`) if they reappear or remain referenced after the real-TU split.
- `type_checker_helpers_late.c` standalone TU가 hidden include-order helper 없이 빌드되도록 call-path helper prototypes와 slot analyzer / visibility / generic diagnostic include 계약을 명시했다.
- string literal / interpolation stable subset을 grammar docs에 고정했다. Stable은 `"..."`, `"""..."""`, `"${expr}"`, `f"{expr}"`, escaped f-string brace까지이며 nested brace matching / format specifier / multiline interpolation은 beta-out-of-scope다.
- `diagnostic-registry-test-smoke`가 `diag_codes.h` / `docs/72_diagnostic_codes.md` code sync와 `semantic_error_with_hints` / `semantic_warning_with_hints` macro usage를 검사한다.
- runtime authority failure surface는 `pgy_runtime_authority_contract.h`를 공통 source-of-truth로 사용한다. inline C runtime과 LLVM runtime library export가 같은 `missing-zone` / `missing-participant` code, reason, stderr format을 쓰며, `runtime-authority-contract-test-smoke`가 literal drift를 막는다.
- projection diagnostics는 missing source field / ambiguous source path / wrong projection kind / duplicate field map 4개 베타 필수 케이스를 `projection-diagnostic-contract-test-smoke`로 고정한다.
- AI-first/GPU 방향은 `pgy.accel.spray`로 module taxonomy와 manifest에 예약했다. 이는 post-beta accelerator library/runtime surface이며 core keyword나 beta blocker가 아니다.
- Skia/shader/render 방향은 `pgy.render.skia`로, DOP style은 `pgy.compat.dop`로 module taxonomy와 manifest에 예약했다. 둘 다 post-beta ecosystem surface이며 core keyword나 beta blocker가 아니다.
- **[NEW]** 외부 언어 연동(JVM JNI, Python C-API 등)은 `pgy.interop.*` 생태계 모듈로 분류하며, core 언어가 안정화된 이후(v1 또는 별도 마일스톤)의 post-beta 영역으로 명시하여 제외한다.
- module ecosystem update policy를 taxonomy에 고정했다. `pgy.core`는 가장 자주 개선하되 가장 강하게 검증하고, OOP/FP/DOP/GPU/render/interop/std/kit은 모듈 생태계로 진화한다.

남은 것:

- Behavior-owning production `.inc` files are closed and must stay closed. Any
  reintroduction is a beta blocker, not beta+1 cleanup.
- TU mixing remains the active risk: `.inc` removal cannot mean dumping several
  behavior families into one large `.c` or replacement mega-header. New
  production owners above 600 LOC require a named follow-up seam; owners above
  1,000 LOC are hard-cap failures unless explicitly listed as legacy debt under
  an active split.
- Tier 1 `.inc` cleanup is closed, but several owner TUs remain larger than the
  review threshold. `compiler/mir.c` is no longer in that queue after the
  SSA/use-edge, liveness/value-summary/DCE, and statement-population owner
  split. The next Tier 2 targets are parser AST/domain owners, DIR declaration
  graph ownership, `llvm_backend.c`, `type_checker_decls_domain_helpers.c`,
  and the remaining 600-plus backend statement/frontier owners.
- `type_checker.c` is below 600 LOC and functions as semantic orchestration.
  Further semantic debt is now in specific owner TUs, not the top-level
  dispatcher.
- core module boundary와 compiler implementation module boundary가 아직 완전히 대응하지 않는다.
- parser/lex baseline error code routing은 text + JSON 모두 닫혔다. 남은 것은 semantic diagnostic registry 수준의 세분화된 parser code split과 multi-error accumulation이다.
- `pgy.accel.spray`는 아직 구현/stdlib/API가 없다. 베타 전에는 설계 경계만 유지하고, 베타 이후 CPU fallback + explicit device/context + owned buffer/tensor API부터 별도 closure로 진행한다.
- `pgy.render.skia`와 `pgy.compat.dop`도 아직 구현/stdlib/API가 없다. 베타 전에는 module boundary만 유지하고, 베타 이후 render/shader graph 및 data-layout helper를 별도 closure로 진행한다.

완료 조건:

- `src/semantic` production `.inc = 0`를 `make semantic-inc-size-test-smoke`로 고정한다.
- `src/codegen`, `src/runtime`, `src/compiler` production `.inc = 0`를
  `make backend-inc-size-test-smoke`로 고정한다.
- 신규 `.inc` 증가는 금지한다. 현재 `make inc-sentinel-test-smoke`가
  `src/**/*.inc = 0`, `.cases.h` under `src/tests` only, `.cases.h <= 30`,
  `.cases.h` include from dedicated test harnesses only, empty `.cases.h` test
  fragment 금지, orphan `.cases.h` fragment 금지를 함께 검사한다.
- core semantic/DAG/backend/runtime owner boundary가 문서와 파일 구조에서 추적 가능하다.
- `.inc`는 `src` tree 안에는 남기지 않는다. generated table이나 local
  macro table이 필요하면 named `.h` / `.c` owner로 만든다.

2026-04-27 audit note:

- Production include-size gate is green and production `.inc` inventory is zero.
  The active cleanup metric is no longer `.inc` LOC; it is owner cohesion:
  production `.c` and private owner `.h` files above 600 LOC must be reviewed as
  split candidates, while 1,000 LOC remains the hard cap for new owner headers.
- Current high-value owner split candidates by responsibility are:
  parser AST/domain owners for parser shape ownership,
  `dir.c` for declaration graph ownership,
  `llvm_backend.c` for backend context / module orchestration,
  `llvm_intent.c` for intent runtime/declaration lowering,
  `type_checker_decls_domain_helpers.c` for semantic domain helper families,
  and `runtime/slot_security.c` / `runtime/slot_manager.c` for slot authority
  and cache/lease seams.

증거 명령:

```sh
make module-taxonomy-test-smoke
make test-semantic
make test-all
make llvm-test-backend-compare
make backend-inc-size-test-smoke
make inc-sentinel-test-smoke
find src -path src/tests -prune -o -name '*.inc' -print
```

2026-04-26 include-cleanup update:

- `transpiler_expr_emitters.inc` pass-through shim has been removed.
  `transpiler.c` includes named concrete emitter chunks directly. The old split
  that crossed `emit_call` was replaced with helper owners for builtin
  dispatch, domain constructors, `Result`/`Option`, stdlib, event, member-call,
  and user-call lowering. Each part is under 1,000 LOC.
- `transpiler_intent_zone_binding_emit.h` and
  `transpiler_emitters_intent.inc` no longer leave dangling `static void`
  return-type fragments across include boundaries.
- All production `.inc` files under `src` are gone. Production code remains
  guarded as a zero-inventory contract by `make backend-inc-size-test-smoke`
  and `make semantic-inc-size-test-smoke`.
- Test fixtures are now guarded by `make test-inc-size-test-smoke`; the large
  semantic/transpile fixture includes were split into ordered part shims and
  rechecked with `make test-semantic test-transpile`.
- Detailed ledger: `docs/115_inc_cleanup_status.md`.

---

## 2. Type-Resolution DAG Closure

2026-04-26 update:

- Non-generic nominal class type references and known non-class scope symbols
  now resolve through `semantic_type_resolution_lookup_or_materialize(...)`
  metadata instead of falling through to the central recursive resolver.
- Generic class references are intentionally excluded so default type argument
  resolution and generic mismatch provenance stay on the generic contract path.
- Current gate: `metadata_entries>=3300`, `metadata_hits>=4900`,
  `metadata_owned>=200`, `materializer_fallbacks==0`, plus exact
  fallback-family accounting.
- Current fallback family distribution is `named=0`, `generic_named=0`,
  `compound=0`, `other=0`; named details are `builtin_shell=0`,
  `generic_class=0`, `alias=0`, `non_class_symbol=0`, `missing_symbol=0`.
  Alias chains now materialize or fail with metadata-stage cycle diagnostics
  before recursive materialization, and the smoke gate now requires
  `metadata_named_alias == 0`. The smoke gate also requires the full fallback
  total to equal the diagnostic-only families:
  `builtin_shell + generic_named + missing_symbol`. Compound, other,
  generic-class, non-class-symbol, and alias fallback must stay at zero. Do not
  widen constructed-shell expansion just to hide negative diagnostics.
- Verified by `make type-resolution-dag-test-smoke` and
  `make type-resolution-resolver-inventory-test-smoke`.
- `type_checker_resolution_metadata.c` is now 268 LOC and owns metadata lookup
  and central materializer orchestration. `type_checker_resolution_metadata_alias.c`
  is 315 LOC and owns alias-chain materialization, alias cycle formatting, and
  `semantic_type_resolution_lookup_metadata_name_or_alias(...)`.
  `type_checker_resolution_metadata_diagnostics.c` owns stable-shell arity
  rejection, invalid constructed stable shell diagnostics, and unknown bare
  named type diagnostics. This keeps central fallback closure, alias-cycle
  provenance, and user-facing diagnostic wording in separate owners. The old
  recursive `resolve_type_alias_decl(...)` path and alias-resolution stack are
  removed, so alias chain/cycle semantics no longer have a second evaluator.
  The resolver-inventory smoke also rejects reintroducing that alias-stack
  debt.

2026-04-25 update:

- Graph precollect now materializes context-independent builtin type refs (`Int`, `Long`, `Float`, `Double`, `Bool`, `String`, `QubitSlot`, `Void`) into `SemanticContext.type_resolution_metadata`.
- Graph metadata now materializes resolver-stable constructed and anchored-handle shells (`Array<T>`, `Slice<T>`, `List<T>`, `Queue<T>`, `Set<T>`, `Box<T>`, `Rc<T>`, `Weak<T>`, `Channel<T>`, `Future<T>`, `RemoteFuture<T>`, `Token<T>`, `DeviceSlot<T>`, `HashMap<String|Int|Long|Bool, T>`, `Option<T>`, `Result<T,E>`, `Slot<T>`, `SecureSlot<T>`, `ReadView<T>`, `WriteView<T>`, `MoveToken<T>`) when the argument facts are already available. Metadata-owned `Type` shells are released on semantic context destroy.
- Graph metadata now also materializes tuple shells and event-handler/function shells when all element/parameter/return facts are available. Channel/future AST nodes now record their constructed shell during graph collect instead of waiting for recursive fallback.
- Pass-2 owner resolver seams now query DAG metadata through the shared
  materializer instead of owning recursive fallback seams.
- The recursive `resolve_type_node(...)` evaluator is removed from the beta
  owner path. Retired counters remain only as audit signals, and beta owner
  paths are gated so they cannot re-enter a recursive resolver through metadata
  materialization.
- Owner-local resolver seams now converge through `semantic_type_resolution_lookup_or_materialize(...)`; direct `resolve_type_node(...)` calls are statically blocked outside explicit audit/test references, and the central metadata materializer no longer has a recursive escape hatch.
- The retired compatibility audit path stays at 0 calls. Bare builtin/named
  stable refs stay on the metadata owner path through
  `semantic_type_resolution_lookup_metadata_type_ref(...)` instead of reopening
  a compatibility resolver body.
- The type-ref metadata API also records stable constructed refs before
  returning unresolved, and the signature-stage quiet resolver consumes that API
  before compatibility fallback accounting. `type-resolution-resolver-inventory-test-smoke`
  gates both seams.
- Semantic owner helpers that still require unresolved-ref diagnostics now
  consume `semantic_type_resolution_lookup_type_ref_or_materialize(...)`, which
  centralizes the metadata type-ref preflight before diagnostic materialization.
- Direct `semantic_type_resolution_lookup_or_materialize(...)` calls are now
  blocked outside central metadata/diagnostic compatibility owners by
  `type-resolution-resolver-inventory-test-smoke`.
- `resolve_generic_type_arg(...)` is also metadata-first, so constructed builtin and generic consumer paths reuse graph facts before recursive fallback.
- `make type-resolution-dag-test-smoke` now gates graph-backed stage skips, retired compatibility resolver calls (`retired_resolver_calls<=0`), metadata entries, metadata owned entries, metadata hits, metadata materializer fallback count, zero non-alias stage metadata materialization, and alias-stage split accounting. Earlier local stats for this slice were `graph-backed skips=3137 metadata_entries=2044 metadata_owned=123 metadata_hits=3300 materializer_fallbacks=4135 stage_materialize_alias=83 stage_materialize_non_alias=0 alias_materialized=5 alias_diagnostic_unresolved=78 alias_diagnostic_resolved=0 alias_diagnostic_cycle_unresolved=78`.
- The DAG smoke now enforces beta floors for graph-backed usage and metadata materialization instead of accepting any non-zero metadata activity.
- The central metadata materializer fallback is closed, not merely capped:
  `materializer_fallbacks==0` and every metadata unresolved audit family must stay at
  zero.
- The remaining stage metadata materialization surface is alias-only diagnostic inventory.
  Successful alias materialization and diagnostic unresolved inventory are
  reported separately, valid alias diagnostic resolution is gated at zero, and
  `alias_diagnostic_resolver_calls==0` proves the diagnostic path no longer
  re-enters the recursive resolver. The 78 unresolved entries come from
  intentional alias-cycle diagnostic coverage, not hidden non-alias recursive
  resolution.
  `type_checker_resolution_stage_alias.c` owns that diagnostic inventory so
  the top-level stage replay owner stays orchestration-only.
- Ability declarations are now predeclared in the program-level symbol inventory, and `type_check_ability_decl(...)` reuses only its own predeclare. This closes the forward declaration-order gap for generic default/where consumers, zone authority ability consumers, and party role-slot ability consumers without weakening duplicate-ability diagnostics.
- C/LLVM parity now includes `tests/cases/backend_compare/forward_ability_order/main.pgy`, which keeps provider-after-consumer ordering for generic defaults, aliases, zone authority ability consumers, and party role-slot ability consumers from regressing outside semantic-only tests.
- `tests/compare_backends.sh` now fails its default run when a `tests/cases/backend_compare/*/main.pgy` directory is not registered in the default case array. Targeted runs with explicit arguments remain allowed for development, but CI can no longer silently skip a new parity case. The gate pulled eight previously passing but unregistered cases into the default C/LLVM parity suite: array builtins/inline access, slice inline access, intent observability rollback, list/map/queue get-string, and try-operator result.
- `type-resolution-resolver-inventory-test-smoke` now treats the fallback seam count as a one-way debt cap instead of a coverage floor. The current named fallback seam cap is 0, prints the active fallback seam count, and shrinking below the old floor no longer fails CI.
- `type_checker_module_contract.c` no longer calls the recursive fallback helper for ability contract bookkeeping. It records and checks ability contract shape/provenance through ability-specific logic and only performs DAG metadata lookup for already-materialized type facts, reducing the fallback seam inventory from 39 to 38.
- `type_checker_ability_fields.c` now follows the same lookup-only pattern for ability `fields` requirements: the ability-specific validator owns field-contract diagnostics, while DAG metadata provides already-materialized type facts without recursive fallback.
- Domain and intent declaration resolution now converge through owner-local type-reference seams. Domain slot/shared/named refs and intent involves/value/where refs share their local owner seam, reducing the fallback seam inventory from 38 to 34.
- Alias/generic-parameter helpers and resolution-stage diagnostic fallback now converge through owner-local seams, reducing the fallback seam inventory from 34 to 32. Ability-field validation lookup-only resolution reduced the active seam cap to 31. Projection builtin target-field resolution now follows the same lookup-only pattern: graph metadata owns the materialized target field type, while projection diagnostics own source/target field mismatch. This reduces the active fallback seam cap to 30. Program-level quiet placeholder resolution now uses precollected DAG metadata lookup only, so event/function forward placeholders no longer need recursive fallback and the active cap is 29. Domain query projection source-field resolution now follows class/vessel field metadata lookup-only and reduces the active cap to 28. Party/roster shared-field resolution now follows declaration metadata lookup-only and reduces the active cap to 26. Ability abstract method signature resolution and role host-type resolution now follow metadata lookup-only and reduce the active cap to 24. Function/action body precollect now walks expression subtrees, call type args, lambda param/return/body types, event subscription handlers, spawn/channel/return/branch expressions; event/lambda handler signature resolution now uses DAG metadata lookup-only and reduces the active cap to 23. Body flow type resolution now uses DAG metadata lookup-only and reduces the active cap to 22. Type-alias statement resolution now uses DAG metadata lookup-only and reduces the active cap to 21. Generic where/default validation moved to the shared metadata materialization API and every owner-local resolver seam now calls `semantic_type_resolution_lookup_or_materialize(...)`; the old named fallback helper is removed and its cap is 0.
- This is not full DAG source-of-truth yet, but owner-local recursive fallback
  debt and central metadata materializer fallback are both closed:
  `type-resolution-resolver-inventory-test-smoke` caps direct fallback seams at
  0, and `type-resolution-dag-test-smoke` requires
  `materializer_fallbacks==0`. Constructor-shell provenance, generic default
  specialization, and missing-symbol diagnostics now stay on metadata-owned
  paths without re-entering recursive materialization.
- Authority direct-slot resolution now clears stale ambiguity when the participant alias resolves to a concrete zone subject slot after earlier same-type candidates. This keeps `authorized by rogue/mage` valid when the zone has matching `subject slot rogue/mage: Adventurer`, while still preserving the hard error for genuinely ambiguous same-type participants.

상태: `IN PROGRESS / BLOCKER`

목표:

- DAG가 frozen subset의 type dependency ordering source-of-truth가 된다.
- declaration order, module contract, generic consumer path가 recursive lookup 순서에 묶이지 않는다.
- cycle/provenance diagnostics는 graph-backed vocabulary를 쓴다.

현재 닫힌 것:

- graph inventory, cycle diagnostic, topo derivation이 존재한다.
- provider-first staged worklist가 top-level declaration과 local/projection synthetic node 일부를 소비한다.
- generic default/constraint/where-bound staged resolution이 들어왔다.
- role/action/intent/zone/party ability consumer pre-stage가 graph path와 연결됐다.
- event/enum/ability/action-contract/role/class/party/roster/intent precollector는 graph declaration TU로 이동해 inventory pass의 declaration-kind seam을 줄였다.
- Function/lambda/body expression type-reference walking is now owned by
  `type_checker_resolution_graph_body.c`; `type_checker_resolution_graph_decl.c`
  is back to declaration inventory ownership and is under the 600 LOC
  split-review threshold.
- relation/effect domain inventory precollector는 graph domain TU로 이동했다.
- world inventory precollector는 graph world TU로 이동했다.
- zone refresh projection field-map collector는 graph zone TU로 이동했다.
- world/zone local-contract stage replay는 stage domain TU로 이동했다.
- DAG stage 내부의 retired resolver compatibility surface는 `PGY_TYPE_RES_STATS=1`에서 `stage-metadata-materialize: calls/failed/suppressed_diagnostics`와 `stage-materialize-family: generic_contract/signature/ability_consumer/domain_contract/alias/other`로 노출된다.
- DAG edge가 이미 있는 named type-ref는 generic argument를 포함해 stage에서 다시 materialize하지 않고 graph-backed skip으로 처리한다. `stage-graph-backed: skips=N`이 이 경로의 공개 지표이며 `type-resolution-dag-test-smoke`는 skip 합계가 0으로 퇴행하면 실패한다.
- graph precollect TU는 더 이상 stage runner를 호출하지 않는다. enum methods도 `semantic_stage_method_array(...)`가 아니라 precollect action contract 경로로 edge를 수집한다.
- stage lookup, stage stats, signature/materialization helpers, alias
  diagnostic replay, nominal declaration replay, systemic declaration replay,
  and domain declaration replay are split
  into `type_checker_resolution_stage_lookup.c`,
  `type_checker_resolution_stage_stats.c`, and
  `type_checker_resolution_stage_signature.c`,
  `type_checker_resolution_stage_alias.c`,
  `type_checker_resolution_stage_nominal.c`,
  `type_checker_resolution_stage_systemic.c`, and
  `type_checker_resolution_stage_domain_decl.c`.
  `type_checker_resolution_stage.c` is now 88 LOC and owns top-level stage
  replay orchestration only.
- Materializer fallback family accounting is split into
  `type_checker_resolution_metadata_fallback.c`. `type_checker_resolution_metadata.c`
  now owns metadata lookup/materialization while fallback taxonomy counters have
  a single owner.
- Stable constructed type materialization is split into
  `type_checker_resolution_metadata_constructed.c`, and owned metadata cleanup
  is split into `type_checker_resolution_metadata_storage.c`. Alias-chain
  materialization and alias cycle formatting are split into
  `type_checker_resolution_metadata_alias.c`. The DAG metadata, alias,
  stage, declaration, body, constructed, fallback, and storage owners are all
  below the 600 LOC split-review threshold.
- Program-level graph inventory is now a dispatcher owner:
  `type_checker_resolution_graph_inventory.c` is 98 LOC and delegates zone
  inventory to `type_checker_resolution_graph_zone_inventory.c`.
- Zone graph inventory is split at the state/authority tail seam:
  `type_checker_resolution_graph_zone_inventory.c` owns role/authority/domain
  slot collection and is 554 LOC, while
  `type_checker_resolution_graph_zone_tail.c` owns zone state, maintained-state,
  authority, and method-tail collection at 169 LOC. All
  `type_checker_resolution_*.c` DAG owners are now below the 600 LOC
  split-review threshold.
- generic where/default validation은 `type_checker_generic_validation.c`가 소유한다. `type_checker_resolution_graph_*.c`와 `type_checker_resolution_graph_core.h`는 resolver-free graph layer로 고정됐고, `semantic-core-shape-test-smoke`가 graph layer의 직접 `resolve_type_node(...)` 호출을 금지한다.
- intent declaration resolution은 participant/value/where local seam 3개로 수렴했고, 이제 graph metadata-first 조회 후 recursive fallback으로 내려간다. 단순 lookup-only 전환은 semantic suite 후반 parallel execution path에서 segfault를 만들었으므로, direct semantic/bootstrap path와 step/local binding materialization이 lookup-only 계약을 만족할 때까지 explicit fallback seam으로 남긴다.
- domain contract resolution은 slot/shared/named-ref local seam 3개로 수렴했고, projection/relation/effect contract도 graph metadata-first 조회 후 fallback으로 내려간다.
- intent helper resolution은 `intent_helper_resolve_type_ref(...)` 단일 seam으로 수렴했다. transfer-derived using/where, ability generic arg, role-field checks는 이 seam에서 graph-backed metadata로 교체할 수 있다.
- host helper resolution은 `host_helper_resolve_type_ref(...)` 단일 seam으로 수렴했다. projection source fields, hosted method return/param, zone authority/domain slot checks는 이 seam에서 graph-backed metadata로 교체할 수 있다.
- program declaration/body resolution은 quiet/body resolver seam으로 수렴했다. function-body materialization seam은 graph metadata-first 조회 후 fallback으로 내려간다.
- event signature resolution은 `semantic_event_resolve_type_ref(...)` 단일 seam으로 수렴했다. event params, return type, lambda handler signature를 graph-backed signature metadata로 교체할 수 있다.
- world shared/domain-slot resolution은 `world_resolve_type_ref(...)` / `world_resolve_domain_slot_type(...)` seam으로 수렴했다. 단순 lookup-only 전환은 `subject slot ... requires a subject type` 계열 semantic regression을 만들었으므로, world domain-slot subject/zone nominal metadata가 DAG에 보존될 때까지 explicit fallback seam으로 남긴다.
- role/generic-contract/late-helper/expr resolution은 각각 local seam 1개로 수렴했다. remaining direct resolver inventory에서 이 파일들은 이제 metadata replacement owner를 명확히 가진다.
- generic validation, ability where/module contract/declaration, class field, operator overload, ownership destructure resolution도 local seam으로 수렴했다. remaining direct resolver inventory는 resolver implementation, comments, or explicit seam sites로 압축됐다.
- statement, ability field, builtin projection/query, flow, generic support, helper effects, ownership let, party/roster/zone single-call resolver paths도 local seam으로 수렴했다.
- `make type-resolution-resolver-inventory-test-smoke`가 새 direct `resolve_type_node(...)` 호출을 resolver implementation/stage metadata materialization/core fallback/local seam allowlist 밖에서 금지한다. It also blocks any new `semantic_type_resolution_resolve_or_fallback(...)` users and caps the named fallback seam inventory at 0, so owner-local fallback consumers cannot grow silently. This is now a debt ceiling only, not a lower-bound coverage floor.
- `type_checker_decls_domain_helpers.c`의 zone authority participant resolver가 exact/qualified-tail direct slot match를 먼저 인정하고, direct match 반환 시 stale ambiguity flag를 지운다. `dnd_tavern_campaign` 같은 multi-slot same-type zone에서 concrete participant alias가 false-positive ambiguous로 떨어지는 경로를 닫았다.
- `make type-resolution-dag-test-smoke`가 graph stats, topo validation, stage metadata materialization inventory를 CI gate로 검사한다.
- intent/standalone helper dependency는 internal headers와 hard CFLAGS로 고정되어 DAG split 중 hidden include-order failure를 즉시 잡는다.
- graph cycle과 compatibility alias cycle 모두 `Contract source`, `Reason`, `Fix` vocabulary를 쓴다.
- provider-after-consumer source order is covered for a frozen DAG slice: function generic default/where references, type alias providers, zone authority ability consumers, and party role-slot ability consumers can appear before their ability/type providers. This is now covered by both semantic graph regression and C/LLVM backend compare.

남은 것:

- `resolve_type_node` 중심 recursive resolver는 central materializer fallback
  경로에서 더 이상 호출되지 않는다. Owner-local fallback consumer allowlist는
  0이고, central materializer fallback도 0이다.
- 2026-04-28 DAG materializer closure: stable constructed type arguments share
  a metadata-only arg resolver, nested wrapper chains such as
  `Channel<Slot<T>>` materialize before fallback, no-arg default generic class
  specializations preserve provenance such as `Box<Item>` while materializing
  in DAG metadata, and bare unknown named types emit `PGY_SEM_UNKNOWN_TYPE`
  directly from the metadata path. The current gate is green with
  `metadata_entries=3358`, `metadata_hits=6744`, `metadata_owned=253`,
  `materializer_fallbacks=0`, and every metadata unresolved audit family at `0`.
- Alias-only diagnostic inventory no longer calls the recursive resolver. The
  current gate is green with `alias_diagnostic_unresolved=78`,
  `alias_diagnostic_resolver_calls=0`, `alias_diagnostic_resolved=0`, and
  `alias_diagnostic_cycle_unresolved=78`. The remaining count is semantic alias-cycle
  diagnostic coverage, not materialization debt.
- The next DAG closure target is no longer central materializer fallback; it is
  making more stage/declaration consumers use graph facts directly while
  preserving alias cycle provenance and source-order diagnostics.
- `stage-metadata-materialize` non-alias family는 0이고 alias family는 diagnostic
  fallback으로만 남아 있다. `stage-graph-backed` skip 수는 DAG가 실제 stage
  source-of-truth로 옮겨간 양을 보여주는 공개 지표다.
- frozen subset에서 declaration order에만 기대는 type dependency가 없어야 한다.
- ability provider predeclaration is closed for the tested frozen DAG slice; remaining declaration-order debt should be narrowed to module/backend metadata reuse, not top-level ability visibility.
- graph inventory metadata를 backend/declaration inventory와 더 잘 연결해야 한다.

완료 조건:

- frozen subset의 known type dependency가 declaration order에만 의존하지 않는다.
- DAG staged schedule이 generic/module/authority/party/local projection path에서 source-of-truth로 쓰인다.
- cycle/provenance diagnostics가 graph path 기준으로 안정적이다.
- docs가 “full rewrite”가 아니라 남은 migration path를 정확히 부른다.

증거 명령:

```sh
make ci-linux
make test-semantic
make type-resolution-dag-test-smoke
make module-test-smoke
grep -R "resolve_type_node" -n src/semantic
```

---

## 3. MIR Declaration Debt Removal

상태: `IN PROGRESS / BLOCKER`

목표:

- routine body뿐 아니라 declaration/top-level backend metadata도 dedicated inventory reader를 통해 소비한다.
- AST-carried declaration inventory는 frozen subset에서 user-visible backend truth가 되지 않는다.
- incomplete inventory는 partial output이 아니라 structured backend error로 실패한다.

현재 닫힌 것:

- ordinary function/method/intent carrier 누락은 hard error다.
- domain method MIR-missing 경로는 partial emit 대신 explicit backend error로 정렬됐다.
- declaration emit entrypoint는 inventory decl을 우선 사용한다.
- C/LLVM backend compare가 frozen subset parity를 넓게 커버한다.
- `make mir-declaration-inventory-test-smoke`가 C/LLVM declaration-side codegen을 active inventory helper path에 묶는다.
- C backend `emit_program(...)`는 ability/type/extern/function/intent/domain/event bootstrap을 `transpiler_active_inventory(...)` / `transpiler_active_externs(...)` view로 소비한다. Direct declaration-array reads are now confined to the helper owner in `transpiler.h`.
- LLVM declaration raw inventory reads are confined to the inventory helper
  family (`llvm_inventory_internal.h`, `llvm_inventory_decl_lookup.h`,
  `llvm_inventory_host_methods.h`); pipeline/domain/intent emitters must go
  through active inventory and lookup helpers.
- LLVM routine traversal in pipeline/domain/intent now goes through `llvm_active_routine_inventory(...)`; direct `mir->routine_count` / `mir->routines` traversal is confined to the helper owner.
- LLVM intent MIR metadata readers now live in
  `src/codegen/llvm_intent_mir_meta.c`, with the private owner seam declared in
  `src/codegen/llvm_intent_internal.h`. `llvm_intent.c` no longer owns the
  who/authorized/participant MIR statement readers and drops to 2,118 LOC while
  backend compare remains green.
- LLVM host method lookup now uses `llvm_find_host_decl_header_in_context(...)` / `llvm_host_decl_methods(...)` metadata-first. AST union method-array fallback remains only when a MIR declaration header is absent.
- `MIRDeclMethod` now carries method `name`, `owner_name`, `is_action_like`, and `within_zone` metadata beside the temporary AST payload. LLVM host method lookup compares `MIRDeclMethod.name` before falling back to AST method arrays.
- `MIRDeclMethod` also links to method body MIR through `has_routine` / `routine_index`, so LLVM method emission can consume the declaration-header method row before falling back to AST-method based routine lookup.
- `MIRDeclMethod` now carries hosted method signature metadata (`params`, `param_count`, `return_type`). LLVM nominal/enum method prototype registration consumes this metadata through helper accessors before falling back to AST method payloads.
- LLVM domain sync/event/role ownership is no longer concentrated in `llvm_domain.c`: method/provenance helpers live in `llvm_domain_method_helpers.c`, event type/helper lowering lives in `llvm_domain_event.c`, role method/operator/vtable emission lives in `llvm_domain_role_emit.c`, domain sync/method body emission lives in `llvm_domain_method_emit.c`, world sync lives in `llvm_domain_world_sync.c`, zone sync lives in `llvm_domain_zone_sync.c`, and the declaration/projection/zone-binding helper families are split into focused owner headers. `llvm_domain.c` is now 895 LOC and remains backend-compare green.
- LLVM statement ownership is now split by real TU owner: type inference lives in `llvm_stmt_type_infer.c`, let helper/type rendering lives in `llvm_stmt_let_helpers.c`, let lowering lives in `llvm_stmt_let_with.c`, collection/channel/array let specializations live in `llvm_stmt_let_collections.c`, callable/lambda let registration lives in `llvm_stmt_let_callable.c`, with lowering lives in `llvm_stmt_with.c`, `while`/`for`/`match` lowering lives in `llvm_stmt_loop_match.c`, `parallel`/`async`/`select` lowering lives in `llvm_stmt_parallel_async.c`, zone-action effect propagation lives in `llvm_stmt_zone_action.c`, and generic type-argument rendering lives in `llvm_stmt_type_render.c`. `llvm_stmt.c` is now 573 LOC, `llvm_stmt_let_with.c` is now 562 LOC, and both dispatcher and let owners are below the 600 LOC split-review threshold.
- MIR cleanup/rollback/invalidation CFG edge ownership is split into
  `src/compiler/mir_cleanup.c`. This does not complete MIR declaration debt,
  but it removes another execution-flow owner family from `src/compiler/mir.c`
  before the larger SSA/liveness/DCE split.
- MIR intent instruction materialization is split into
  `src/compiler/mir_intent.c`. Intent participant, step, zone alias,
  authority, check/eval, dispatch, compensation, and invalidation marker
  lowering no longer lives in the MIR orchestration file.
- MIR CFG/body ownership is split further: SSA rename/use-edge materialization
  lives in `src/compiler/mir_ssa_rename.h`, liveness/value summaries and DCE
  live in `src/compiler/mir_liveness_dce.h` / `src/compiler/mir_dce.h`, and
  source-order statement population lives in
  `src/compiler/mir_stmt_population.h`. `src/compiler/mir.c` is now 512 LOC
  and only owns block construction plus pass ordering for the MIR body path.
  `cfg-body-dataflow-test-smoke` keeps every MIR CFG/body owner below the 600
  LOC split-review threshold.
- AIR evidence collection is split into `src/compiler/air_evidence.c`, with
  shared internal name/error ownership declared in `src/compiler/air_internal.h`.
  HIR/RIR evidence matching no longer lives in the AIR synthesis/drift owner.
  `air-drift-test-smoke` now treats `air.c + air_evidence.c` as the AIR
  implementation inventory.
- AIR boundary traversal is split into `src/compiler/air_boundary.c`. AST
  boundary classification, boundary source derivation, sync-class mapping,
  step boundary counting, and boundary node append now have a dedicated owner.
  `src/compiler/air.c` is now below the 600 LOC split-review threshold.
- AIR verification is split into `src/compiler/air_verify.c`. Inventory shape,
  authority participant shape, evidence provenance invariants, drift emission,
  and strict evidence failures no longer live in the synthesis owner.
  `air-drift-test-smoke` treats `air.c + air_boundary.c + air_dump.c +
  air_evidence.c + air_verify.c` as the implementation inventory and requires
  both `$(COMPILER_DIR)/air_verify.c` and `$(BUILD_DIR)/compiler/air_verify.o`
  wiring.

남은 것:

- declaration inventory bootstrap은 아직 AST-shaped metadata를 많이 들고 있다.
- zone/world/relation/effect declaration metadata를 dedicated view로 잘라야 한다.
- raw host-name state와 duplicated named-decl lookup helper를 더 줄여야 한다.
- dedicated declaration IR is still not complete: the current beta-safe line is helper-gated MIRProgram inventory, not a fully separated declaration metadata model.

완료 조건:

- `MIRDeclInventory` 또는 equivalent dedicated declaration metadata view가 있다.
- C/LLVM이 frozen declaration metadata를 같은 reader에서 소비한다.
- 누락된 declaration inventory는 backend error로 실패한다.
- docs는 남은 AST reference를 internal representation debt로만 설명한다.

증거 명령:

```sh
make test-mir
make mir-declaration-inventory-test-smoke
make llvm-test-backend-compare
make ast-dispatch-test-smoke
```

Inventory regression gate: `make mir-declaration-inventory-test-smoke` keeps C/LLVM declaration-side codegen on the active MIR inventory helper path. It verifies the nominal/domain/declaration helper seam and blocks new raw MIR declaration array reads outside the current owner files. C backend `emit_program(...)` now gets ability/type/extern/function/intent/domain/event declaration views through `transpiler_active_inventory(...)` / `transpiler_active_externs(...)`, not direct `mir->...` declaration arrays.

2026-04-26 LLVM routine inventory update: `llvm_active_routine_inventory(...)` is now the single reader for MIR routine traversal in LLVM pipeline/domain/intent code. This is not a dedicated declaration IR yet, but it removes another raw bootstrap seam before the final `MIRDeclInventory` representation work.

2026-04-26 LLVM declaration metadata update: host method lookup now consumes `MIRDeclHeader` methods before falling back to AST union method arrays. This keeps method inventory access behind the declaration-header seam while the remaining AST-carried method payload is replaced by dedicated decl IR.

2026-04-26 MIR method metadata update: `MIRDeclHeader` now owns `MIRDeclMethod` rows for hosted methods. The row still carries an AST pointer for body/type payload compatibility, but lookup-visible method identity is now a dedicated MIR metadata field. This is the next concrete step toward replacing AST-carried declaration inventory with `MIRDeclInventory`.

2026-04-26 MIR method routine-link update: `mir_link_decl_method_routines(...)` connects each `MIRDeclMethod` to its method body routine by stable `routine_index` after routine lowering completes. LLVM method emission now uses this row-level link first, keeping AST method lookup as compatibility fallback only.

2026-04-26 MIR method signature update: hosted method prototype registration now reads `MIRDeclMethod` signature fields through `llvm_mir_decl_method_*` helpers. The remaining AST pointer is compatibility payload for body/type nodes, not the primary lookup-visible method row.

2026-04-26 C backend structure update: `src/codegen/transpiler_context.c` now owns the output/context primitive layer that had been carried by `transpiler_helpers_core_a_part_a.inc`: `CodeBuf`, `TranspilerCtx` create/destroy, indentation, backend error/hint allocation, and scratch arena string helpers. The private seam is `src/codegen/transpiler_context.h`; the remaining forward declarations were folded into `transpiler_helpers_core_a.inc`, so `transpiler_helpers_core_a_part_a.inc` has been deleted.

2026-04-27 LLVM domain owner update: `src/codegen/llvm_domain_zone_sync.c` now owns zone bounded-frontier sync lowering, `src/codegen/llvm_domain_world_sync.c` owns world sync lowering, and `src/codegen/llvm_domain.c` is reduced to the remaining domain declaration/type orchestration. The old broad `llvm_domain_core_helpers.h` static-helper include was replaced with focused owner headers so `make -B pgy ...` is warning-clean under `-Wall -Wextra`, and `make -B pgy llvm-test-backend-compare` remains 53/53 green.

2026-04-27 LLVM domain event owner update: `src/codegen/llvm_domain_event.c` now owns event type registration and `INIT` / `SUBSCRIBE` / `UNSUBSCRIBE` / `INVOKE` helper lowering. `llvm_domain.c` delegates event lowering through `llvm_emit_domain_event_helpers(...)`, drops to 1,356 LOC, and the old fixed 8-entry event handler parameter array is replaced by full-arity scratch materialization. `make LLVM_ENABLED=1 /tmp/pgy-PergyraLang-bin/pgy llvm-test-backend-compare` remains green with 196 ABI checks and backend compare 53/53.

2026-04-27 LLVM domain role owner update: `src/codegen/llvm_domain_role_emit.c` now owns role method body emission, role operator thunk emission, and role vtable global materialization. The helper returns `bool` so MIR-routine-missing diagnostics still abort the domain pass immediately. `llvm_domain.c` drops to 1,125 LOC, and `make LLVM_ENABLED=1 /tmp/pgy-PergyraLang-bin/pgy llvm-test-backend-compare` remains green with 196 ABI checks and backend compare 53/53.

2026-04-27 LLVM domain method/sync owner update: `src/codegen/llvm_domain_method_emit.c` now owns domain sync helper dispatch and domain method body emission, keeping projection sync helper include-order local to the owner TU. `llvm_domain.c` drops below the 1,000 LOC hard cap to 895 LOC. `make LLVM_ENABLED=1 /tmp/pgy-PergyraLang-bin/pgy llvm-test-backend-compare` remains warning-clean and green with 196 ABI checks and backend compare 53/53.

2026-04-27 LLVM statement owner update: `src/codegen/llvm_stmt_type_infer.c`, `src/codegen/llvm_stmt_let_helpers.c`, `src/codegen/llvm_stmt_let_with.c`, `src/codegen/llvm_stmt_with.c`, `src/codegen/llvm_stmt_loop_match.c`, and `src/codegen/llvm_stmt_parallel_async.c` now own the statement subfamilies that were previously concentrated in `llvm_stmt.c`. The full backend compare suite remains 53/53 green after the split, so let/with, expression type inference, break/continue, collection iteration, range loops, Option/Result match destructuring, parallel, async, and select lowering keep parity across C/LLVM.

2026-04-27 HIR owner update: `src/compiler/hir_analysis.c` owns signature type-reference collection, direct-call discovery, and control-flow presence detection. `src/compiler/hir_lower_cfg.c` owns AST-body to basic-block CFG construction. `src/compiler/hir_cfg.c` owns CFG finalization, reachability, dominator/frontier, dominator tree, natural loop marking, local-def collection, phi candidates, phi materialization, and CFG summary. `src/compiler/hir_routines.c` owns declaration/routine construction and hidden method routine extraction behind `src/compiler/hir_internal.h`. `src/compiler/hir_destroy.c` owns `hir_destroy()` and synthetic executable teardown, keeping free/cleanup ownership out of lowering orchestration. Current HIR owner sizes are `hir.c` 421 LOC, `hir_routines.c` 419 LOC, `hir_destroy.c` 72 LOC, `hir_lower_cfg.c` 598 LOC, and `hir_cfg.c` 599 LOC, so the active HIR owner set is under the 600 LOC split-review threshold. `make test-hir test-rir test-mir air-drift-test-smoke backend-inc-size-test-smoke production-header-size-test-smoke` remains the owner gate.

2026-04-28 driver scaffold owner update: `src/compiler/driver_scaffold.c` now
owns `pgy scaffold` / `pgy new` file and project generation. `src/compiler/driver_app.c`
stays focused on diagnostics, dump modes, pipeline orchestration,
runtime-mode gating, and backend dispatch. `driver_app.c` is now 831 LOC and
the scaffold owner is 812 LOC, so both are below the 1,000 LOC hard risk line
while remaining split-review candidates under the 600 LOC policy.

2026-04-28 compiler host-toolchain owner update: `src/compiler/compiler_toolchain.c`
now owns safe process execution, C compiler discovery, target-flag selection,
runtime object cache freshness, timing, LLD selection, and path safety.
`src/compiler/compiler.c` is now 738 LOC and remains the compiler result plus
C/LLVM emit/link orchestration owner. This removes another 1,000+ owner from
the beta structural debt queue without changing backend output semantics.

2026-04-28 Slot analyzer owner update: `src/semantic/slot_analyzer_summary.c`
now owns Slot access summaries, escape summaries, helper-call propagation, and
parameter summary facts. `src/semantic/slot_analyzer.c` is now 434 LOC and owns
only pass lifecycle plus function/block/if/parallel traversal and diagnostics.
`make test-semantic` remains green at 2357/0 after the split.

2026-04-28 HIR public surface owner update: `src/compiler/hir_public.c` now
owns HIR dump modes, declaration/routine queries, and routine/block pass
runners. `src/compiler/hir.c` is now 926 LOC and remains the top-level
classification, hidden routine materialization, synthetic executable lowering,
and reachability propagation owner. `make test-hir test-rir test-mir
air-drift-test-smoke` remains green after the split.

2026-04-28 DIR owner update: `src/compiler/dir_collect.c` now owns node, role,
party, roster, world, and intent collection; `src/compiler/dir_collect_domain.c`
owns zone / relation / effect slot and projection contract collection;
`src/compiler/dir_validate.c` owns validation/dump/public naming; and
`src/compiler/dir_internal.h` keeps the lowering-local builder/find seam
explicit. `src/compiler/dir.c` is now 467 LOC, `dir_collect.c` is 546 LOC, and
`dir_collect_domain.c` is 274 LOC, so the DIR family is below the 600 LOC
split-review threshold. `make test-dir test-air test-rir` remains green after
the split.

2026-04-28 type environment owner update: `src/semantic/type_env.c` now owns
`TypeEnv` create/destroy/add/lookup helpers. `src/semantic/type_system.c` drops
to 940 LOC and remains the owner for type constructors, equality/assignability,
inference, unification, and generic instantiation. `make test-semantic` remains
green at 2357/0 after the split.

2026-04-28 LLVM backend owner update: the stale `#if 0` copy of the pre-split
`LLVMGenCtx` inventory was removed from `src/codegen/llvm_backend.c`, and
`src/codegen/llvm_backend_generic.c` now owns temp names, generic template
lookup, monomorphization tracking, suffix mapping, and entry-block alloca
creation. `src/codegen/llvm_backend.c` is now 999 LOC and `make pgy` remains
green after the split.

2026-04-28 LLVM domain sync frontier owner update:
`src/codegen/llvm_domain_sync_frontier.c` now owns sync-generation increments,
frontier overflow abort lowering, and post-sync builder restoration.
`src/codegen/llvm_domain_zone_sync.c` is now 997 LOC and remains focused on zone
bounded-frontier sync body emission. `make pgy llvm-test-smoke
production-header-size-test-smoke` remains green after the split.

2026-04-28 stdlib semantic builtin owner update:
`src/semantic/type_checker_builtins_stdlib_scalar.c` now owns scalar, string,
and math builtin checks, while
`src/semantic/type_checker_builtins_stdlib_map.c` owns `HashMap` builtin checks.
`src/semantic/type_checker_builtins_stdlib_body.c` is now 834 LOC and below the
1,000 LOC hard cap. `make test-semantic pgy` remains green at 2357/0 after the
split.

2026-04-28 zone declaration owner update:
`src/semantic/type_checker_zone_decl_authority.c` now owns zone authority
ability validation, duplicate authority diagnostics, layer-slot type
validation, and relation/effect pool beta rejects. `type_checker_zone_decl.c`
is now 929 LOC and remains the zone lifecycle/state rule validation owner.
`make test-semantic pgy` remains green at 2357/0 after the split.

2026-04-28 intent helper owner update:
`src/semantic/type_checker_intent_role_fields.c` now owns role require-field
validation plus intent transfer/zone-binding derivation helpers, and
`src/semantic/type_checker_intent_control.c` owns intent-clause control-transfer
rejection. `type_checker_intent_helpers.c` is now 883 LOC, and both new owners
stay below the 600 LOC split-review threshold. `make test-semantic pgy`
remains green at 2357/0 after the split.

2026-04-28 domain helper owner update:
`src/semantic/type_checker_domain_projection.c` now owns projection contract
diagnostics, and `src/semantic/type_checker_overlay_common.c` owns overlay
symbol/shared-field/hosted-method scope setup.
`type_checker_decls_domain_helpers.c` is now 972 LOC and remains the
zone/effect/relation slot helper owner. `make test-semantic pgy` remains green
at 2357/0 after the split.

2026-04-28 parser domain owner update:
`src/parser/parser_domain_roster.c`, `src/parser/parser_domain_world.c`,
`src/parser/parser_domain_zone.c`, and `src/parser/parser_domain_event.c` now
own roster, world, zone, and event parsing respectively. `parser_domain.c` is
now 970 LOC and keeps relation/effect plus party/ability/role parsing and the
shared domain helper seam. `make test-parser pgy` remains green after the
split, and the active 1,000+ production `.c` owner queue is now AST-only:
`ast.c` and `ast_print.c`.

---

## 4. ABI Ownership / Arena Lifetime Closure

상태: `IN PROGRESS / BLOCKER`

목표:

- scratch/result/persistent/runtime ownership lane이 review 가능해야 한다.
- returned string/helper payload ownership이 함수명과 문서로 구분된다.
- runtime ABI returned values가 scratch pointer에 기대지 않는다.

현재 닫힌 것:

- `docs/94_arena_index_lifetime_plan.md`로 `Arena + Index + 역할별 arena` 방향을 고정했다.
- `docs/74_slot_pinning_caching.md`로 repeated slot access hot path의 Pin/Lease 계약을 고정했다. Pin은 보안 우회가 아니라 scope-entry capability lease이며, block-scoped `pin ... { ... }`만 beta 후보로 둔다.
- `PgyPinnedView`, `PergyraSlotPin(...)`, `PergyraSlotUnpin(...)` runtime ABI baseline이 들어왔다. Normal slot pin은 release/scope-release/TTL-cleanup 파괴를 막고, secure slot write pin은 token 검증 후 sealed payload를 lease buffer로 열며 unpin 시 다시 seal한다. Invalid token, missing capability, concurrent secure write, and release while pinned are covered by `make test-security`.
- `ViewRead(...)` / `ViewWrite(...)` semantic surface now enforces
  `WriteView<T>` exclusive access for the same source slot and keeps
  `ReadView<T>` / `ReadView<T>` sharing accepted.
- The existing view surface now also uses pin-specific diagnostics for the
  first escape/boundary cases: return escape (`PGY_SEM_PIN_ESCAPE`),
  await/spawn/channel/cancel boundary (`PGY_SEM_PIN_AWAIT_BOUNDARY`),
  parallel boundary/acquisition (`PGY_SEM_PIN_PARALLEL_CONFLICT`), and QubitSlot rejection
  (`PGY_SEM_PIN_QUBIT_REJECT`). These codes are now covered through both
  semantic regression and `make diagnostics-json-test-smoke`.
- Source syntax `pin slot as view: ReadView<T>|WriteView<T> { ... }` is accepted
  as a typed-view lexical block and shares the existing `ViewRead(...)` /
  `ViewWrite(...)` semantic gates. Explicit runtime `PgyPinnedView` lowering
  remains internal until cleanup-edge backend parity is implemented.
- The source-level pin block is no longer lost during CFG lowering: HIR and MIR
  carry pin-region metadata for source slot, view name, and read/write mode.
  MIR cleanup now materializes `pin-unpin-cleanup-edge` metadata for those
  blocks. This closes the representation/cleanup-fact seam without claiming
  that explicit runtime pin/unpin lowering is complete.
- Generated inline `PgySlot_*` / `PgySecureSlot_*` now has a typed pin wrapper
  ABI (`PgyPinnedSlotView_*`, `PgyPinnedSecureSlotView_*`,
  `pgy_pin_read_*`, `pgy_pin_write_*`, `pgy_unpin_*`, and secure variants)
  with LLVM-linkable exports. `make test-memory` covers occupied/token
  validation, double-unpin invariant hard-fail, and secure invalid-token pin
  rejection without changing existing slot layouts.
- C source-block emission now consumes pin-block metadata by emitting a typed
  wrapper local with `pgy_unpin_cleanup_*` / `pgy_secure_unpin_cleanup_*` cleanup
  hooks. The owner is `src/codegen/transpiler_block_emit.h`, split out of the
  old block/intent helper so both files stay below the 600 LOC review threshold.
- Source-level typed-view pin now rejects `Release(source_slot)` and
  `Move(source_slot)` while a `ReadView<T>` / `WriteView<T>` over that source is
  live. This closes the immediate marketing-vs-implementation drift for
  "release/move while pinned" at the semantic layer; the lower-level runtime
  pin-state hard-fail remains required for direct SlotManager users and future
  explicit `PgyPinnedView` lowering.
- Source-level typed-view pin also rejects direct owner writes while any typed
  view is live, and direct owner reads while a `WriteView<T>` is live. This
  prevents bypassing the lease by spelling the original slot identifier.
- Slot sugar is covered by the same rule: `slot = value` is treated as an owner
  write and value-position `slot` use is treated as an owner read for active
  pin/view conflict checks.
- Helper calls are covered too: passing the owning source slot to an
  `own`/`ref Slot<T>` helper while a typed view over that source is live is a
  semantic error. Helper code must accept the typed view or run outside the
  pin/view scope.
- Stable container stores are covered too: array literals and `ArrayPush`,
  `ArraySet`, `ListPush`, `ListSet`, `SetAdd`, `MapSet`, and `QueuePush`
  reject the owning source slot while a typed view over that source is live.
- Return-boundary owner forwarding is covered too: `return source_slot` while a
  typed view over that source is live is rejected semantically, before backend
  auto-read lowering can drift into a native compiler error.
- `Box<T>` is kept honest for beta: `Box(source_slot)` and
  `BoxSet(box, source_slot)` reject resource-handle payloads instead of
  accepting parser surface that the current CFG/ABI proof layer cannot own.
- semantic scratch arena, diagnostic result-owned payload seam, HIR/MIR routine scratch, LLVM scratch/result-owned lane이 들어왔다.
- `make test-abi-perf`와 `make perf-summary`로 speed baseline도 관리한다.
- POSIX `realpath` implicit declaration warning을 제거했다.
- intent observability (`last/history/active/recent`) and authority failure stable string exports are `runtime-borrowed string` ABI values: callers must not free them, and values are valid until the next mutation of the corresponding runtime registry/snapshot.
- `runtime-abi-lifetime-test-smoke` gates stable intent last/history/active/recent and authority string export bodies so they do not allocate/free/strdup in the ABI return path.
- stable string helper returns are `result-owned string` ABI values and stable
  string-array helper returns are `result-owned array` ABI values; callers own
  and must eventually release the returned payloads unless a higher-level
  runtime owner consumes them immediately.
- stable file descriptors are `runtime-owned handle` ABI values: callers receive
  numeric handles, while the runtime owns the backing `FILE *` table slot until
  `pgy_file_close` releases it for reuse.
- `runtime-abi-lifetime-test-smoke` also gates result-owned string and
  string-array helpers so they allocate/copy payloads instead of returning
  borrowed input pointers, stack buffers, or string literals.
- `runtime-abi-lifetime-test-smoke` gates runtime-owned file handles so
  `pgy_file_open` reuses closed slots and `pgy_file_close` clears the runtime
  table entry.

남은 것:

- owner shell과 runtime ABI contract가 섞인 helper가 남아 있다.
- runtime-owned handle return helpers beyond file descriptors still need the
  same ownership audit.
- runtime query/diagnostic string이 scratch teardown 이후에도 안전한지 회귀가 더 필요하다.
- Slot Pin/Lease는 runtime primitive baseline, source-level typed-view block
  syntax, existing `WriteView<T>` exclusive semantic gate, return escape
  diagnostic, QubitSlot reject, await/spawn/async/callback/channel/cancel
  boundary reject, active-view `Release(source)` / `Move(source)` reject, direct
  owner access, slot-sugar bypass, helper-boundary owner bypass, and
  container-store owner bypass, return-boundary owner bypass,
  Box resource-handle payload reject, HIR/MIR pin-region metadata,
  MIR `pin-unpin-cleanup-edge` metadata, generated inline pin wrapper ABI, C
  source-block cleanup emission, and
  parallel boundary/acquisition reject가 닫혔다. 남은
  blocker는 MIR cleanup fact를 LLVM/MIR backend explicit pin/unpin call로 낮추는 lowering parity and
  secure-token source diagnostic이다.
- Option C ownership lift keeps Pin/Lease narrow: `pin slot as view { ... }`
  and `PinnedView<T>` are §4 ABI ownership blockers only after §0b proves
  cleanup/escape facts. User-facing raw `void *` remains rejected; only typed
  `ReadView<T>` / `WriteView<T>` may be exposed. `DeviceSlot<T>` pinning is a
  candidate surface, not stable, until device mapping failure classes and C/LLVM
  parity are implemented.

완료 조건:

- helper ownership이 `borrowed`, `scratch-owned`, `result-owned`, `persistent-owned`, `runtime-owned` 중 하나로 분류된다.
- runtime ABI return ownership이 문서화되고 테스트된다.
- frozen subset diagnostic/runtime query가 scratch lifetime 이후 dangling되지 않는다.
- repeated slot access는 per-access validation path와 Pin/Lease path 중 하나로 명확히 분류되고, Pin/Lease는 manual raw pointer API 없이 scoped cleanup으로만 노출된다.

증거 명령:

```sh
make test-abi
make runtime-abi-lifetime-test-smoke
make test-security
make diagnostics-json-test-smoke
make test-abi-perf
make perf-summary PERF_LOG=/path/to/test-abi-perf.log
```

---

## 5. `parallel` Keyword And Core Keyword Tests

상태: `IN PROGRESS / BLOCKER`

목표:

- `parallel`은 core execution primitive로 테스트된다.
- `spawn`/`async`/`await`/`select`/`channel`/cancel은 execution family로 분리하되 smoke/parity는 유지한다.
- core keywords는 parser/semantic/runtime/C/LLVM/docs가 같은 subset을 말한다.

현재 닫힌 것:

- `parallel-core-contract-test-smoke`가 taxonomy/manifest/case-tag/semantic/backend/module-smoke evidence를 하나로 묶는다.
- backend compare에 `parallel_channel_sum`, `parallel_channel_dual`, `triple_paradigm`이 있다.
- `llvm_smoke.sh`는 `select_ready`, `select_fairness`, channel pressure, spawn/generic spawn/string spawn을 가진다.
- `test-concurrency`는 worker spawn, channel send/recv, cancellation, descendant cancellation, zone HasLayer stress를 검증한다.
- module smoke에는 `parallel_ref_slot_conflict` semantic rejection이 있다.

남은 것:

- `parallel` 외 core keyword별 stable/reject/out-of-beta matrix가 아직 하나의 체크리스트로 묶여 있지 않다.
- `parallel`과 zone/effect/resource conflict의 C/LLVM parity coverage를 더 명시해야 한다.
- execution family가 core identity로 과장되지 않도록 README/status docs wording을 마지막에 다시 맞춰야 한다.

완료 조건:

- core keyword matrix가 parser/semantic/runtime/C/LLVM/doc status를 가진다.
- `parallel` core path와 execution family path가 다른 layer로 문서화된다.
- C/LLVM backend compare에 대표 `parallel + resource/effect/channel` cases가 있다.

증거 명령:

```sh
make parallel-core-contract-test-smoke
make test-concurrency
make llvm-test-backend-compare
bash tests/llvm_smoke.sh
```

---

## 6. Pain Point Discovery And Fix Loop

상태: `IN PROGRESS / BLOCKER`

목표:

- 베타 전 pain point는 새 기능으로 덮지 않고, surface trust / diagnostics / examples / structure debt로 해결한다.
- parser가 받지만 끝까지 닫히지 않은 surface를 stable처럼 보이게 두지 않는다.

현재 주요 pain point:

- DAG source-of-truth 미완성.
- long-term modularization stop condition 미달.
- runtime propagation generalization 미완성.
- recoverable runtime failure query surface 부족.
- contract clause density.
- projection diagnostics final wording freeze.
- MIR declaration inventory bootstrap debt.
- arena/runtime ABI ownership seam.

완료 조건:

- pain point마다 `fix`, `explicit reject`, `beta-out-of-scope` 중 하나로 분류된다.
- B0 pain point는 semantic regression, example/smoke, C/LLVM parity, docs wording을 가진다.
- B2 pain point는 베타 보드에서 빠지고 beta+1/backlog로 이동한다.

증거 명령:

```sh
make test-all
make llvm-test-backend-compare
make module-taxonomy-test-smoke
make ast-dispatch-test-smoke
```

---

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
  `transpiler_world_select_event_emit.h` (370 LOC), `transpiler_select_emit.h`
  (155 LOC), and `transpiler_event_emit.h` (103 LOC). This removes the
  600+ LOC mixed owner without reintroducing `.inc` files.
- Verified with `make pgy`, `make runtime-frontier-contract-test-smoke`,
  `make runtime-frontier-policy-test-smoke`,
  `make production-header-size-test-smoke`, and
  `make backend-inc-size-test-smoke`.

## Progress Log — 2026-04-29 C Let Slot Owner Split

- Slot-related let lowering is now a named C backend owner:
  `transpiler_let_slot_emit.h` owns ClaimSlot/ClaimSecureSlot/ClaimDeviceSlot,
  ReadView/WriteView/MoveToken declarations, and Slot/SecureSlot sugar.
- `transpiler_let_emit.h` is back to let-declaration orchestration plus
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

- Non-generic class declaration lowering moved to
  `transpiler_class_decl_emit.h`.
- `transpiler_func_class_flow_emit.h` is now below the 600 LOC split-review
  threshold and no longer owns class field/container/method emission directly.
- Verified with `make pgy`, `make test-transpile`,
  `make production-header-size-test-smoke`,
  `make backend-inc-size-test-smoke`, and `make llvm-test-backend-compare`
  (`196/0` ABI same-process, `65/65` backend compare).

## Progress Log — 2026-04-29 C MIR Block Owner Split

- MIR emission predicate wrappers moved to `transpiler_mir_emit_predicates.h`.
- `transpiler_mir_block_emit.h` is below the 600 LOC split-review threshold
  and keeps block statement emission ownership focused.
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
  `transpiler_mir_local_type_lookup.h`.
- `transpiler_mir_ssa_names.h` is now 357 LOC and keeps SSA name resolution,
  SSA map setup, claim-shape predicates, and implicit-field rendering focused.
- `transpiler_mir_local_type_lookup.h` is 293 LOC and owns MIR local type
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
  signature stage. It now routes through
  `semantic_type_resolution_lookup_type_ref_or_materialize(...)`, keeping
  metadata-first type-ref behavior centralized.
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
  `resolve_calls=0`, `resolver_body_fallbacks=0`,
  `materializer_fallbacks=0`, and semantic suite `2359/0`.

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

## Progress Log — 2026-04-24 Parser/Lexer Diagnostic Routing

- `parser_error`와 lexer error token이 stage code, reason, fix를 갖도록 1차 routing gate를 닫았다.
- 새 코드: `PGY_PARSE_SYNTAX`, `PGY_LEX_INVALID_TOKEN`.
- 새 gate: `make parser-lexer-diagnostic-test-smoke`.
- CI 연결: `ci-linux`가 parser/lexer diagnostic gate를 실행한다.
- 남은 beta debt: parse/lex baseline message surface와 JSON diagnostic object routing은 닫혔다. 남은 것은 parser-specific code split과 multi-error accumulation이다.
