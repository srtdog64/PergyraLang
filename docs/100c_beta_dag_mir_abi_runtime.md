# Beta Readiness Checklist - DAG, MIR, ABI, Runtime

> Split from `docs/100_beta_readiness_checklist.md` on 2026-05-29.
> Keep active blocker edits in the shard that owns the relevant closure track.

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
- `parser_intent.c` is now a 514 LOC declaration parser. Intent-level `who` /
  `where` propagation moved to `parser_intent_defaults.c` at 69 LOC, and step
  clause parsing remains in `parser_intent_step.h` at 297 LOC. The intent
  parser surface stays below the 600 LOC split-review threshold without
  changing parser exports.
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
  exhaustiveness, redundancy, and total-coverage lattice checks.
  `type_checker_flow_loop_control.c` owns `break` / `continue` validation and
  loop resource snapshots. The CFG/body flow owner `type_checker_flow.c` is
  down to 488 LOC and focuses on branch/join, block/defer/parallel boundary,
  return, and unreachable flow orchestration. `semantic-core-shape-test-smoke`
  keeps flow owners under the 600 LOC split-review threshold.
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
  extraction inside named headers/TUs. LLVM statement parallel/async lowering
  is isolated in `llvm_stmt_parallel_async.c`, select lowering is isolated in
  `llvm_stmt_select.c`, let-destructure lowering is isolated in
  `llvm_stmt_destructure.c`, match lowering is isolated in
  `llvm_stmt_match.c`, and LLVM intent MIR metadata, zone binding/sync helpers,
  effect provenance helpers, and MIR-backed intent flow/signature helpers are
  isolated in
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
  lowering, preserved source-order let emission, and source-order/claim
  scheduling now live in `transpiler_mir_destructure_emit.c`,
  `transpiler_mir_preserved_let_emit.h`, and
  `transpiler_mir_block_schedule_emit.h`. The main block statement owner is
  `transpiler_mir_block_emit.c` behind a declaration-only header.
- 2026-05-04 with-slot source-order repair: `src/compiler/mir_stmt_population.h`
  now propagates `source_statement_index` onto matched `MIR_INST_RESOURCE_OP`
  facts, including residual `with slot` Claim/Write/Read facts, before C
  backend source-order scheduling. `src/codegen/transpiler_mir_resource_op_emit.h`
  also emits `AST_WITH_STMT` Claim facts by AST kind rather than requiring a
  nonzero wrapper source location, and `src/codegen/llvm_mir_block_emit.h`
  uses the same AST-kind criterion for LLVM with-slot claim setup. The LLVM
  block-entry Claim prepass was also removed, so both backends now consume
  with-slot setup through the main MIR instruction order rather than a
  backend-local early materialization path. LLVM residual `MIR_INST_STMT`
  handling also no longer emits `AST_WITH_STMT` Claim setup; the resource-op
  Claim fact is the only stable with-slot materialization path. Gates:
  `cfg-body-dataflow-test-smoke`, `example-test-smoke`, `test-mir`, and
  `test-transpile` (`717/0`).
- MIR emission contract and inventory debt were split further:
  resource/cleanup hook emission now lives in
  `transpiler_mir_resource_hook_emit.h`, and SSA name-map utilities plus MIR
  block mapping comments now live in `transpiler_mir_ssa_map.h`.
  `transpiler_mir_emission_contract.c` and
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
  `transpiler_spawn_channel_emit.c`, leaving the matching header
  declaration-only and
  `transpiler_expr_call_spawn_emit.h` below the 600 LOC split-review threshold.
- Member-style slot/host/slice call lowering now lives in
  `transpiler_expr_call_member_emit.c`, leaving `transpiler_expr_call_spawn_emit.h`
  as the call-dispatch shim.
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
  `src/**/*.inc = 0`, `.cases.h` under `src/tests` only, inventory cap 88,
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
- `transpiler_intent_zone_binding_emit.c` owns intent forward declaration and
  zone-bound alias restore emission, while `transpiler_intent_zone_binding_emit.h`
  is declaration-only. `transpiler_emitters_intent.inc` no longer leaves
  dangling `static void` return-type fragments across include boundaries.
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
- Current gate: `metadata_entries>=3600`, `metadata_hits>=8500`,
  `metadata_owned>=240`, `materializer_fallbacks==0`, plus exact
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
- Pass-2 owner resolver seams now query DAG metadata through metadata-only
  owner helpers instead of owning recursive fallback seams.
- The recursive `resolve_type_node(...)` evaluator is removed from the beta
  owner path. Retired counters remain only as audit signals, and beta owner
  paths are gated so they cannot re-enter a recursive resolver through metadata
  materialization.
- Owner-local resolver seams now converge through
  `semantic_type_resolution_lookup_metadata_type_ref(...)`; direct
  `resolve_type_node(...)` calls are statically blocked outside explicit
  audit/test references, and the central metadata materializer compatibility
  API has been removed.
- The retired compatibility audit path stays at 0 calls. Bare builtin/named
  stable refs stay on the metadata owner path through
  `semantic_type_resolution_lookup_metadata_type_ref(...)` instead of reopening
  a compatibility resolver body.
- The type-ref metadata API records stable constructed refs before returning
  unresolved, and the signature-stage quiet resolver consumes that API without
  compatibility fallback accounting. `type-resolution-resolver-inventory-test-smoke`
  gates the metadata-only seam.
- Semantic owner helpers that still require unresolved-ref diagnostics now
  consume metadata facts only and leave diagnostics to their local owner instead
  of entering a materializing type-ref helper.
- Direct `semantic_type_resolution_lookup_or_materialize(...)` calls are now
  blocked everywhere by `type-resolution-resolver-inventory-test-smoke`.
- Enum payload projection lookup and recursive projection-path field lookup are
  now metadata-only DAG consumers. They use
  `semantic_type_resolution_lookup_metadata_type_ref(...)` and leave missing
  graph facts to their existing enum/projection diagnostics instead of entering
  the materializing type-ref seam. The active materializing helper inventory is
  capped at 0 after ability declaration signatures, ability `fields`
  requirements, ability where-bound validation, intent involves/value/where
  resolution, and domain slot/shared/named refs moved to metadata-only lookup.
  The unused materializing type-ref compatibility APIs were removed.
- `resolve_generic_type_arg(...)` is also metadata-first, so constructed builtin and generic consumer paths reuse graph facts before recursive fallback.
- `make type-resolution-dag-test-smoke` now gates graph-backed stage skips, metadata entries, metadata owned entries, metadata hits, metadata materializer fallback count, and alias-stage split accounting. Current local stats for this slice are `graph-backed skips=2064 generic_param_nodes=102 dag_generic_contract_evidence=165 dag_ability_consumer_evidence=72 metadata_entries=3735 metadata_owned=261 metadata_hits=8771 metadata_dead_ends=0 materializer_unresolved=0 metadata_unresolved_named=0 metadata_unresolved_generic_named=0 metadata_unresolved_compound=0 metadata_unresolved_other=0 metadata_unresolved_builtin_shell=0 metadata_unresolved_generic_class=0 metadata_unresolved_alias=0 metadata_unresolved_non_class_symbol=0 metadata_unresolved_missing_symbol=0 alias_materialized=6 alias_diagnostic_unresolved=78 alias_diagnostic_cycle_unresolved=78`.
- The DAG smoke now enforces beta floors for graph-backed usage and metadata materialization instead of accepting any non-zero metadata activity.
- The central metadata materializer fallback is closed, not merely capped:
  `materializer_fallbacks==0` and every metadata unresolved audit family must stay at
  zero.
- The remaining stage metadata materialization surface is alias-only diagnostic inventory.
  Successful alias materialization and diagnostic unresolved inventory are
  reported separately, and the retired `resolver_calls` / `resolved` alias
  diagnostic columns are removed instead of kept as zero-only telemetry. The
  78 unresolved entries come from intentional alias-cycle diagnostic coverage,
  not hidden non-alias recursive resolution.
  `type_checker_resolution_stage_alias.c` owns that diagnostic inventory so
  the top-level stage replay owner stays orchestration-only.
- Ability declarations are now predeclared in the program-level symbol inventory, and `type_check_ability_decl(...)` reuses only its own predeclare. This closes the forward declaration-order gap for generic default/where consumers, zone authority ability consumers, and party role-slot ability consumers without weakening duplicate-ability diagnostics.
- C/LLVM parity now includes `tests/cases/backend_compare/forward_ability_order/main.pgy`, which keeps provider-after-consumer ordering for generic defaults, aliases, zone authority ability consumers, and party role-slot ability consumers from regressing outside semantic-only tests.
- `tests/compare_backends.sh` now fails its default run when a `tests/cases/backend_compare/*/main.pgy` directory is not registered in the default case array. Targeted runs with explicit arguments remain allowed for development, but CI can no longer silently skip a new parity case. The gate pulled eight previously passing but unregistered cases into the default C/LLVM parity suite: array builtins/inline access, slice inline access, intent observability rollback, list/map/queue get-string, and try-operator result.
- `type-resolution-resolver-inventory-test-smoke` now treats the fallback seam count as a one-way debt cap instead of a coverage floor. The current named fallback seam cap is 0, prints the active fallback seam count, and shrinking below the old floor no longer fails CI.
- `type_checker_module_contract.c` no longer calls the recursive fallback helper for ability contract bookkeeping. It records and checks ability contract shape/provenance through ability-specific logic and only performs DAG metadata lookup for already-materialized type facts, reducing the fallback seam inventory from 39 to 38.
- `type_checker_ability_fields.c` now follows the same lookup-only pattern for ability `fields` requirements: the ability-specific validator owns field-contract diagnostics, while DAG metadata provides already-materialized type facts without recursive fallback.
- Domain and intent declaration resolution now converge through owner-local type-reference seams. Domain slot/shared/named refs and intent involves/value/where refs share their local owner seam, reducing the fallback seam inventory from 38 to 34.
- Ability declaration signatures, ability `fields` requirements, and ability where-bound validation now share one metadata-only `ability_resolve_type_ref(...)` owner seam. They no longer consume the materializing type-ref helper, and the resolver inventory smoke keeps fallback seams, annotation-only reads, and nullable annotation reads at 0.
- The previous materializing type-ref helper cap staircase has converged to
  zero. Projection, role, intent, class, event, world, method-call, type-alias,
  async spawn-boundary, and generic/default consumers now use metadata-only
  owner seams, and `type-resolution-resolver-inventory-test-smoke` fails on
  any remaining `semantic_type_resolution_lookup_type_ref_or_materialize`
  occurrence under `src/semantic`.
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
- DAG stage 내부의 retired resolver compatibility surface no longer exports
  zero-only `stage-metadata-materialize` / `stage-materialize-family` telemetry.
  `PGY_TYPE_RES_STATS=1` exposes graph-backed skips, metadata inventory/reuse,
  DAG evidence, and alias diagnostics instead.
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
  `type_checker_resolution_metadata_dead_end.c`. `type_checker_resolution_metadata.c`
  now owns metadata lookup/materialization while fallback taxonomy counters have
  a single owner.
- Stable constructed type materialization is split into
  `type_checker_resolution_metadata_constructed.c`, and owned metadata cleanup
  is split into `type_checker_resolution_metadata_storage.c`. Alias-chain
  materialization and alias cycle formatting are split into
  `type_checker_resolution_metadata_alias.c`. Metadata cache index ownership is
  split into `type_checker_resolution_metadata_index.c`, so hashing,
  open-addressed lookup/insert, rebuild, and index-capacity growth no longer
  live in the materialization policy owner. The DAG metadata, alias, stage,
  declaration, body, constructed, fallback, index, and storage owners are all
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
- generic where/default validation은 `type_checker_generic_validation.c`가 소유한다. `type_checker_resolution_graph_*.c`는 resolver-free graph layer로 고정됐고, `semantic-core-shape-test-smoke`가 graph layer의 직접 `resolve_type_node(...)` 호출을 금지한다. The old `type_checker_resolution_graph_core.h` implementation header is gone; graph validation/topo ownership now lives in `type_checker_resolution_graph_validate.c`.
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
- zero-only `stage-metadata-materialize` family counters are removed. The
  `stage-graph-backed` skip count is the public indicator that stage consumers
  are using DAG facts, and `stage-alias` remains the alias diagnostic inventory.
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
- LLVM host method lookup now uses `llvm_find_host_decl_header_in_context(...)` / `llvm_host_decl_methods(...)` metadata-first. When MIR is active, missing host declaration metadata fails closed instead of falling through to the compatibility active-inventory loop; the AST-shaped compatibility loop remains only for no-MIR paths.
- `MIRDeclMethod` now carries method `name`, `owner_name`, `is_action_like`, and `within_zone` metadata beside the temporary AST payload. LLVM host method lookup compares `MIRDeclMethod.name` before falling back to AST method arrays.
- `MIRDeclMethod` also links to method body MIR through `has_routine` / `routine_index`, so LLVM method emission can consume the declaration-header method row before falling back to AST-method based routine lookup.
- `MIRDeclMethod` now carries hosted method signature metadata (`params`, `param_count`, `return_type`). LLVM nominal/enum method prototype registration consumes this metadata through helper accessors before falling back to AST method payloads.
- LLVM role target-type reads now route through the role lookup owner
  (`llvm_role_for_type_node(...)` / `llvm_role_for_type_name(...)`). This does
  not make role target type metadata MIR-owned yet, but it removes duplicated
  `role_decl.for_type` reads from forward declaration and operator emission and
  gives the eventual MIR role-target lift one compatibility seam.
- C role operator lookup and role operator alias emission mirror the same
  owner-boundary policy through `transpiler_role_subject_type_node_local(...)` /
  `transpiler_role_subject_type_name_local(...)`, so C/LLVM role target-type
  compatibility has matching helper seams while role-target metadata remains
  AST-carried.
- Semantic role target-type reads mirror the backend policy through
  `semantic_role_for_type_node(...)` / `semantic_role_for_type_name(...)` in
  `type_checker_domain_role_lookup.c`. Operator overload checks and ability
  role matching now consume that helper instead of carrying local
  `role_decl.for_type` filters.
- Semantic role declaration lookup for included-role traversal is centralized
  in the same owner as `semantic_find_role_decl(...)`; operator overload and
  ability include traversal consume that helper instead of carrying duplicate
  program scans. Those semantic include traversals now also guard
  `AST_INCLUDE_STMT` shape before reading include payloads, matching the C/LLVM
  role lookup guards.
- Intent role-field validation also consumes `semantic_role_for_type_name(...)`
  when binding a role to its subject type, so role contract validation shares
  the same target-type seam as operator overload and ability matching.
- Role declaration include validation also consumes
  `semantic_find_role_decl(...)` instead of carrying a local program scan, and
  it guards `AST_INCLUDE_STMT` shape before reading include payloads.
- Role declaration host-type validation also consumes
  `semantic_role_for_type_node(...)` / `semantic_role_for_type_name(...)`
  instead of reading `role_decl.for_type` directly.
- DAG graph precollect and staged nominal resolution now guard role include
  names before recording or resolving include dependencies, aligning DAG role
  include traversal with semantic and backend traversal guards.
- DAG role host-type precollect and staged nominal resolution now consume
  `semantic_role_for_type_node(...)`, so direct role target-type AST access is
  limited to the semantic/backend role lookup owner seams.
- Role include payload access now goes through `ast_include_role_name(...)` /
  `ast_include_type_args(...)`. Semantic, DAG, C backend, and LLVM role
  traversal paths no longer duplicate the include-node shape/name guard, and
  non-parser include payload consumers are smoke-gated away from direct
  `data.include_stmt` access.
- Role impl ability access now has AST accessors:
  `ast_impl_ability_ref(...)`, `ast_impl_ability_name(...)`,
  `ast_impl_ability_method_count(...)`, and `ast_impl_ability_method(...)`.
  Operator lookup, ability matching, role declaration validation, DAG role impl
  precollect/staged resolution, DIR/HIR/MIR role impl collection, and C/LLVM
  role vtable/method emission consume these accessors instead of directly
  reading impl ability payloads. The accessors are const-correct for read-only
  scanners, and the smoke gate rejects direct non-parser `data.impl_ability`
  consumers under semantic/compiler/codegen.
- Role child-list access now has AST accessors:
  `ast_role_for_type(...)`, `ast_role_include_count(...)`,
  `ast_role_include(...)`, `ast_role_impl_count(...)`, and
  `ast_role_impl(...)`. Semantic, compiler, C backend, and LLVM source-of-truth
  paths no longer read `role_decl.for_type` / include arrays / impl arrays
  directly.
- Ability method-list access now has AST accessors:
  `ast_ability_name(...)`, `ast_ability_method_count(...)`, and
  `ast_ability_method(...)`. Compiler/codegen consumers use these for ability
  vtable/forward declaration/DIR completeness/name lookup scans instead of
  reading ability name/method payloads directly. The module normalizer remains
  the explicit exception because it owns mutable declaration-name rewriting.
- Role declaration names now also have a read-only AST accessor:
  `ast_role_name(...)`. Compiler/codegen role-name consumers use it for DIR,
  HIR, MIR declaration headers, C declaration lookup, and LLVM declaration
  inventory/role emission. The module normalizer is the only direct
  compiler/codegen exception because it owns mutable declaration-name
  rewriting, and `semantic_core_shape_smoke.sh` gates that exception.
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
  reading include payloads via the same AST include accessor as the C operator
  lookup, LLVM role lookup, and semantic include traversal.
- C backend hosted-method body emission now uses `transpiler_mir_decl_method_*` and `transpiler_hosted_method_view_routine(...)` for class, enum, generic-class, and domain hosted-method paths. AST compatibility remains for body/type payloads, but routine identity is no longer rediscovered from AST first on those paths.
- C backend routine scans now use `TranspilerMIRRoutineInventory` and `transpiler_routine_inventory_get(...)`, so direct `ctx->mir->routines` walking is limited to helper owners and diagnostic count reporting.
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
- MIR CFG/body ownership is split further: SSA rename materialization lives in
  `src/compiler/mir_ssa_rename.c`, versioned use-edge population lives in
  `src/compiler/mir_ssa_use_edges.c`, liveness/value summaries and DCE live in
  `src/compiler/mir_liveness_dce.c` / `src/compiler/mir_dce.c`, and
  source-order statement population lives in
  `src/compiler/mir_stmt_population.c`. `src/compiler/mir.c` is now an
  orchestration owner for block construction plus pass ordering.
  `cfg-body-dataflow-test-smoke` keeps every MIR CFG/body owner below the 600
  LOC split-review threshold.
- AIR evidence collection is split into `src/compiler/air_evidence.c`, with
  shared internal name/error ownership declared in `src/compiler/air_internal.h`.
  HIR/RIR evidence matching no longer lives in the AIR synthesis/drift owner.
  `air-drift-test-smoke` now treats `air.c + air_evidence.c` as the AIR
  implementation inventory. DAG evidence is further split into
  `src/compiler/air_evidence_dag.c`, so `SemanticResult` DAG counters and
  metadata dead-end facts enter AIR through a single source-of-truth owner.
- AIR boundary traversal is split into `src/compiler/air_boundary.c`. AST
  boundary classification, boundary source derivation, sync-class mapping,
  step boundary counting, and boundary node append now have a dedicated owner.
  `src/compiler/air.c` is now below the 600 LOC split-review threshold.
- AIR verification is split into `src/compiler/air_verify.c`. Inventory shape,
  authority participant shape, evidence provenance invariants, drift emission,
  and strict evidence failures no longer live in the synthesis owner.
  `air-drift-test-smoke` treats `air.c + air_boundary.c + air_dump.c +
  air_dump_json.c + air_evidence_* + air_verify.c` as the implementation
  inventory and requires the corresponding Makefile source/object wiring.

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

2026-05-03 C hosted-method forward declaration update: C backend hosted-method
forward declarations now read `MIRDeclMethod` signature fields through
`transpiler_mir_decl_method_*` helpers and the metadata-first forward helper.
The previous AST-only hosted-method forward declaration helper is removed, so
class/enum/generic/domain hosted prototypes no longer rediscover method
signatures from AST before checking MIR declaration metadata.
`mir_validate(...)` now also rejects declaration-header hosted-method metadata
drift: declaration header name/type/method-list compatibility, method metadata
count, row AST payload compatibility, owner/name/signature compatibility, and
linked routine indexes are checked before codegen consumes the declaration
inventory.
2026-05-29 follow-up: linked hosted-method routines are now validated as
metadata links, not only as in-range indexes. A `MIRDeclMethod.has_routine`
row must point to a `MIR_SCOPE_METHOD` routine with the same owner/name, and
`mir_link_decl_method_routines(...)` uses the same owner/name predicate before
forming a link. The MIR declaration inventory smoke gates the linker negative
test, the validator negative test, and the row-level proof table in
`docs/125_source_of_truth_spine.md`.
MIR declaration headers also preserve pointer-self ABI shape for subject/vessel
and domain hosts, and roster hosted methods are now recorded in declaration
metadata instead of being omitted from `MIRDeclHeader`.
The next row is field metadata: `host_decl_compat.c` now owns class-field and
domain shared-field compatibility views plus name-based field lookup helpers.
The C/LLVM constructor Channel guards, LLVM current-host Channel target
resolution, LLVM current field class lookup, C member/local type inference,
class/domain constructor lowering, generic class specialization emission, LLVM
domain-parts splitting, MIR SSA implicit zone-field lookup, overlay projection
invalidation, and C
nominal/overlay current-field helpers plus C/LLVM projection-path helpers
consume those owner seams instead of reopening class/shared field arrays
locally. This is not a full
declaration-field metadata model yet; it removes the duplicated backend field
traversal families that were safe to close without introducing the dedicated
declaration-field IR. The smoke also keeps a global codegen whitelist so new
field-array reopens are rejected unless they live in `host_decl_compat.c` or an
approved declaration/register emit owner.
Duplicate declaration header names are rejected by `mir_validate(...)`, keeping
`mir_find_decl_header(...)` from resolving ambiguous declaration inventory rows.

2026-05-21 hosted declaration compatibility update: C and LLVM no longer keep
separate hosted-declaration type sets or separate AST compatibility
classification switches for hosted methods. `src/codegen/host_decl_compat.c`
owns the class/enum/party/roster/role/world/relation/effect/zone type set, host
declaration-name accessor, pointer-self host policy, C nominal-host lookup
order, known-nominal forwarding, compatibility method view, and shared-field
compatibility view, while the C and
LLVM hosted-method view owners only adapt that shared view to their MIR metadata
readers. C expression type inference also consumes the same known-nominal seam
for nominal call result inference instead of carrying a separate host-chain
copy. C nominal host-type predicates resolve through the same nominal-host
lookup seam while preserving their class/struct/vessel/object filter locally,
and C `self.member` dispatch consumes the same pointer-self policy instead of
carrying a local domain-host chain. C/LLVM `HasProjection` lowering consumes the
same projection-ready host policy instead of each backend repeating
relation/effect/zone eligibility, and LLVM constructor shared-field defaults
consume the same shared-field compatibility view instead of repeating
party/roster/relation/effect/zone/world eligibility. C constructor dispatch keeps
its class-first precedence but consumes the centralized constructor-domain type
order for party/roster/relation/effect/zone/world lookup, and C MIR local type
lookup consumes that same seam for nominal call fallback while keeping class
constructors separate and preserving enum/role exclusion. Annotated C `let`
constructor fallback also consumes the same seam when deciding whether
metadata-bearing domain literals must be emitted through expression lowering
instead of zero initialization. LLVM constructor dispatch consumes the same
constructor-domain lookup seam before shared-field defaults and projection/world
dirty initialization, so C and LLVM constructor-domain order now share one
compatibility owner. This does not remove the AST compatibility
payload yet, but it removes one C/LLVM drift seam before the final dedicated
declaration inventory model.
Gates: `mir-declaration-inventory-test-smoke`,
`build-source-inventory-test-smoke`, `test-mir`, `test-transpile`, and
`llvm-test-smoke`.

2026-05-03 MIR inventory surface-usage update: declaration/function inventory
surface scans for intent observability and thread-pool usage are now
materialized once into `MIRProgram` inventory facts. C/LLVM codegen consumes
`inventory_uses_intent_observability_surface` instead of rescanning declaration
AST payloads for no-trace intent observability decisions. `mir_validate(...)`
owns the stale-fact check, and the MIR regression corrupts the inventory fact
explicitly to keep this a MIR contract rather than a backend heuristic. Local
gate: native MinGW `test-mir` (`40/0`) and `perf_contract_smoke`.

2026-05-03 AST analysis owner update: `src/parser/ast_analysis.c` is narrowed to
generic identifier-call traversal and intent-observability prefix detection.
Thread-pool surface traversal moved to `src/parser/ast_thread_pool_analysis.c`
and is wired through `PARSER_SOURCES`. This keeps the split responsibility-based
(`identifier-call scan` vs `thread-pool surface scan`) and returns the parser
analysis owner below the 600 LOC split-review threshold. Local gates:
`test-parser`, `test-mir`, `build-source-inventory-test-smoke`, and
`test_inc_size_smoke`.

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
`src/semantic/type_checker_builtins_stdlib_body.c` is now 415 LOC after the
follow-up variant/channel transport split. `make test-semantic pgy` remains
green at 2500/0 after the split.

2026-05-04 stdlib HashMap dispatch tightening:
`src/semantic/type_checker_builtins_stdlib_map.c` now resolves stable `Map*`
builtins through a `StdlibMapBuiltinSpec` table and enum dispatch, keeping the
HashMap stable-surface owner from growing another local `strcmp` branch chain.
`src/semantic/type_checker_builtins_stdlib_channel_transport.c` routes the
stable channel transport family (`TryRecv`, `RecvTimeout`, `TrySend`,
`SendTimeout`, `TrySendStatus`, `SendTimeoutStatus`) through a
`StdlibChannelTransportSpec` table before calling the channel transport typing
helpers. `src/semantic/type_checker_builtins_stdlib_variant.c` routes
Option/Result variant builtins (`Some`, `None`, `IsSome`, `IsNone`, `IsOk`,
`IsErr`, `UnwrapOption`, `Unwrap`, `UnwrapOr`) through
`StdlibVariantBuiltinSpec`, keeping variant typing policy in one dispatch table
instead of a branch-local name chain.
`src/semantic/type_checker_builtins_stdlib_collections.c` also routes the
stable List/Set/Queue/Array builtin names through
`StdlibCollectionBuiltinSpec`; the per-family semantic checks remain unchanged,
but name classification is now table-owned.
`make stdlib-test-smoke LLVM_ENABLED=0` and `make pgy LLVM_ENABLED=0` passed.

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
The former `type_checker_decls_domain_helpers.c` large-owner queue has since
been closed; projection and contract responsibilities live in named semantic
owners, and semantic production owners are below the 600 LOC review threshold.
`make test-semantic pgy` remained green at 2357/0 after the original split.

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
  hooks. The implementation owner is `src/codegen/transpiler_block_emit.c`;
  `src/codegen/transpiler_block_emit.h` is declaration-only.
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
- intent observability (`last/history/active/recent`) stable string exports are `runtime-borrowed string` ABI values: callers must not free them; intent observability strings are copied into thread-local borrowed snapshots, so returned pointers do not alias mutable registry storage and remain valid until a later borrowed string query on the same thread reuses that snapshot slot.
- `runtime-abi-lifetime-test-smoke` gates stable intent last/history/active/recent and authority string export bodies so they do not allocate/free/strdup in the ABI return path.
- stable string helper returns are `result-owned string` ABI values and stable
  string-array helper returns are `result-owned array` ABI values; callers own
  and must eventually release the returned payloads unless a higher-level
  runtime owner consumes them immediately.
- stable file descriptors are `runtime-owned handle` ABI values: callers receive
  numeric handles, while the runtime owns the backing `FILE *` table slot until
  `pgy_file_close` releases it for reuse.
- pool object handles share the same `runtime-owned handle` ABI contract:
  `pgy_pool_spawn(...)` returns a numeric `int32_t` slot index that the runtime
  owns until `pgy_pool_despawn(...)` releases it. `pgy_pool_get(...)` validates
  the handle against the `alive` flag before returning a pointer into runtime
  storage, so a despawned or out-of-range handle cannot expose dangling memory.
- `runtime-abi-lifetime-test-smoke` also gates result-owned string and
  string-array helpers so they allocate/copy payloads instead of returning
  borrowed input pointers, stack buffers, or string literals.
- `runtime-abi-lifetime-test-smoke` gates runtime-owned file handles so
  `pgy_file_open` reuses closed slots and `pgy_file_close` clears the runtime
  table entry.
- `runtime-abi-lifetime-test-smoke` also gates the pool runtime-owned handle
  contract so `pgy_pool_spawn` reuses freed slots, `pgy_pool_despawn` actually
  clears the alive flag, and `pgy_pool_get` rejects despawned or out-of-range
  handles instead of exposing dangling pointers.

남은 것:

- owner shell과 runtime ABI contract가 섞인 helper가 남아 있다.
- pool object handles (`pgy_pool_spawn`/`pgy_pool_despawn`/`pgy_pool_get`) now
  share the same runtime-owned handle audit as file descriptors. Remaining
  numeric-handle audit targets are scoped to any new export that returns a
  runtime-owned slot index in the future.
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
  parallel boundary/acquisition reject가 닫혔다.
- Source-level `PGY_SEM_PIN_TOKEN_INVALID` now fires when
  `ViewRead(...)` / `ViewWrite(...)` is applied to a `SecureSlot<T>` and the
  paired capability token symbol is not reachable in the current scope. This
  closes the previous gap where invalid-token pin only failed at runtime ABI.
  Runtime hard-fail remains the deeper backstop.
- Backend-compare parity for pin block lowering now also covers
  nested-if-in-pin (`pin_nested_if_in_block`), nested-pin over two distinct
  slots (`pin_nested_two_slots`), and pin inside a `for` loop
  (`pin_inside_for_loop`). C/LLVM both emit the same pin enter / per-iteration
  unpin sequence for these patterns.
- 남은 blocker는 MIR cleanup fact를 LLVM/MIR backend explicit pin/unpin call로
  낮추는 lowering parity (broader exceptional/cancellation all-exit coverage).
- 별도 issue: `pin` inside a `while` loop is currently blocked by a C backend
  MIR-mapping bug where the loop body's first iteration drops its `Log(...)`
  call when the loop reads from a `Slot<T>` (`probe5` minimal reproducer:
  `let v: Int = Read(counter); Log(v);` inside `while Read(counter) < N`
  prints only iterations 1..N-1, not 0..N-1; LLVM backend is correct). This is
  not pin-specific; the fix path belongs to the C-side MIR while-loop /
  Slot-read lowering owner, not §4.
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
