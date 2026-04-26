# Pergyra TODO (배포 ?

## UTF-8 Progress Note - 2026-04-27 - Production .inc Closure

- Production .inc debt is closed as a zero-inventory beta gate:
  src/runtime, src/codegen, src/compiler, and src/semantic now have
  **0 production .inc files / 0 LOC** under src, excluding
  src/tests/**/*.inc fixtures.
- Former pass-through seams now live in named private owner headers such as
  pgy_runtime_inline_core.h, 	ranspiler_base_a_emitters.h,
  	ranspiler_base_b_emitters.h, 	ranspiler_expr_emitters.h,
  	ranspiler_helpers_core_{a,b}.h, 	ranspiler_domain_role_emit.h, and
  llvm_expr_call_owners.h.
- Compiler runtime cache freshness no longer points at stale runtime .inc
  dependency paths; it tracks the renamed runtime owner headers.
- inc-sentinel-test-smoke now treats src/tests/**/*.inc as the only
  tolerated fixture lane and caps it at the current 47 files; production
  .inc reintroduction is a hard failure.
- Follow-up owner-header debt slice: C backend scalar/math/string stdlib call
  lowering moved from the monolithic 	ranspiler_expr_stdlib_builtin.h
  dispatcher into 	ranspiler_expr_stdlib_scalar_builtin.h. The dispatcher
  drops from 917 LOC to 751 LOC without changing builtin names or generated C.
- production-header-size-test-smoke now caps production owner headers at
  1,000 LOC by default, with a narrow temporary 1,600 LOC allowance for...
  <!-- TODO: AI Agent lost the rest of this sentence during Git restore. Please fix! -->

## UTF-8 Progress Note - 2026-04-27 - Production `.inc` And Owner Header Closure

- Production `.inc` debt is closed as a zero-inventory beta gate:
  `src/runtime`, `src/codegen`, `src/compiler`, and `src/semantic` now have
  **0 production `.inc` files / 0 LOC** under `src`, excluding
  `src/tests/**/*.inc` fixtures.
- `inc-sentinel-test-smoke` now treats `src/tests/**/*.inc` as the only
  tolerated fixture lane and caps it at the current 47 files; production
  `.inc` reintroduction is a hard failure.
- C backend scalar/math/string stdlib call lowering moved into
  `transpiler_expr_stdlib_scalar_builtin.h`; Map/List/Set/Queue lowering
  moved into `transpiler_expr_stdlib_collection_builtin.h`. The main stdlib
  dispatcher drops from 917 LOC to 432 LOC while preserving dispatch order.
- `production-header-size-test-smoke` now caps production owner headers at
  1,000 LOC by default, with a narrow temporary 1,600 LOC allowance for
  `llvm_internal.h` until the LLVM context/API declarations are split.

## AIR Beta Gate Note - UTF-8 Canonical Terms

- 베타 readiness 추정: 약 `50%`.
- AIR abstraction safety는 Phase 1 데이터 구조 / synthesis / drift checker baseline
  plus driver semantic-validation wiring까지 gate로 묶는다.
- strict evidence는 기본값으로 승격됐다.
- `PGY_AIR_STRICT_EVIDENCE=0`은 development/debug opt-out이다.
- Missing RIR boundary/authority evidence는
  `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`로 hard-fail 된다.
- AIR source of truth: `docs/104_air_compiler_architecture.md`.
- AIR drift gate: `make air-drift-test-smoke`.
- Backend non-impact gate: `air-backend-nonimpact-test-smoke`.

## UTF-8 Progress Note - 2026-04-26 - Formal Semantics Proof Boundary

- Slot capability calculus is now part of the formal proof pack via
  `docs/semantics/08_slot_capability_calculus.md`.
- `docs/semantics/proofs/SlotCalculus.v` is intentionally labeled as a
  proof-sketch, not completed beta mechanized proof. It now models selected
  Slot capability invariants: stale handle rejection, issued-token read/pin
  requirements, unissued-token rejection, and pin non-eviction.
- `make formal-semantics-test-smoke` now forbids overclaim terms in the Coq
  artifact and runs `coqc` when the local toolchain provides it.
- Linux GitHub Actions now installs `coq`, so the formal semantics smoke becomes
  an actual Coq type-check gate in CI instead of a local optional check.
- Runtime evidence for the Slot capability calculus was rechecked with
  `make test-security` (132/132 passed): generation guard coverage now includes stale-generation
  read/write/pin/release rejection and `SlotIsValid` false, plus
  release-while-pinned, scope-release-while-pinned, TTL cleanup skip while
  pinned, secure invalid token rejection, revoked-token rejection, concurrent
  secure write rejection, raw secure-slot release rejection, and
  release-after-unpin.
- `runtime-panic-abi-test-smoke` now covers forged zero-token read/write/release
  rejection for inline C runtime and exported C/LLVM-linkable secure-slot
  entrypoints. SecureSlot token ABI is now build-mode stable: inline C,
  exported runtime, and LLVM-linkable runtime use the same `PgyToken<T>` layout
  with read/write capability bits, and no-`PGY_SAFE_SLOTS` invalid-token /
  released-slot secure paths remain hard-fail checked. The old release-mode
  SecureSlot macro has been removed so future inline ABI drift is blocked.
  `pgy_abi_spec.h` now includes debug/release SecureSlot layout rows for all
  stable primitive payloads (`Int`, `Long`, `Float`, `Double`, `Bool`,
  `String`), and `make test-abi` checks runtime size/token offsets against the
  spec.
  Authority-token mismatch is now a real runtime contract surface:
  `authority-token-mismatch` code/reason, queryable snapshot state, `make
  test-security` direct coverage, `authority_failure_abi` C/LLVM ABI coverage,
  and `authority_failure_surface` backend-compare coverage. The remaining
  secure/authority invariant parity work is richer domain-boundary denial.
  Unsupported authority-token transport is now explicitly rejected on the
  current beta transport surfaces: blocking channel send/receive,
  non-blocking/timeout channel helpers, channel close, cancellation payloads,
  and direct named `spawn` boundaries.
- This keeps the beta proof line honest: theorem statements and regression
  evidence are required now; completed machine-checked proof remains a separate
  hardening gate until CI type-checks it.

## UTF-8 Progress Note - 2026-04-26 - DAG Metadata Materialization Tightening

- Non-generic nominal class type references now materialize through
  `semantic_type_resolution_lookup_or_materialize(...)` metadata instead of
  falling through to the central recursive resolver.
- Generic class references with explicit/default type parameters are
  deliberately excluded from this shortcut so default type argument resolution
  and generic mismatch provenance remain owned by the generic contract path.
- Intermediate DAG smoke stats before the follow-up tightening:
  `graph-backed skips=3137 metadata_entries=3248
  metadata_owned=244 metadata_hits=4724 materializer_fallbacks=1601
  metadata_fallback_named=1594 metadata_fallback_generic_named=7
  metadata_fallback_compound=0 metadata_fallback_other=0 legacy_alias=83
  legacy_non_alias=0 alias_materialized=5 alias_diagnostic_fallback=78
  alias_fallback_resolved=0 alias_fallback_unresolved=78`.
- `type_resolution_dag_smoke.sh` now gates the tighter beta line:
  `metadata_entries>=3000`, `metadata_hits>=4500`, `metadata_owned>=200`, and
  `materializer_fallbacks<=1601`, with fallback family accounting required to
  sum exactly to the total fallback count.
- That slice moved the remaining DAG closure mostly to named-symbol
  materialization (`1594/1601` fallback events), not compound type
  construction. The next target is to split
  imported/non-class nominal, alias-diagnostic, and visibility-sensitive named
  references instead of widening the generic shortcut.
- Follow-up tightening: known non-class scope symbols now materialize through
  metadata using the same `scope-type lookup` contract as `resolve_named_type`.
  Current stats are `metadata_entries=3346 metadata_hits=4935
  materializer_fallbacks=1296 metadata_fallback_named=1289
  metadata_fallback_generic_named=7 metadata_named_builtin_shell=2
  metadata_named_generic_class=0 metadata_named_alias=1281
  metadata_named_non_class_symbol=0 metadata_named_missing_symbol=6`.
  The DAG smoke gate now requires `metadata_entries>=3300`,
  `metadata_hits>=4900`, and `materializer_fallbacks<=1296`.
- Verified locally: `make type-resolution-dag-test-smoke` and
  `make type-resolution-resolver-inventory-test-smoke`.

## UTF-8 Progress Note - 2026-04-26 - Overall Beta Audit Follow-up

- Tooling conformance is green locally with `make tooling-conformance-test-smoke`.
  The formatter smoke is invoked through `bash`, so Linux execute-bit drift on
  mounted worktrees should not reproduce the old `fmt_smoke.sh Permission
  denied` failure.
- Production runtime/codegen/compiler `.inc` size gate is green, but the next
  cleanup should target near-cap files instead of adding more split fragments:
  `transpiler_emitters_base_a_part_c.inc` at 964 LOC,
  `transpiler_emitters_intent.inc` at 962 LOC.
- Lean debt-slice follow-up: C backend type-alias declaration emission now has
  a real owner in `src/codegen/transpiler_type_alias.c`; the old body was
  removed from `transpiler_emitters_base_b_part_c.inc`. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`.
- Lean debt-slice follow-up: C backend type-requirement checks now have a real
  owner in `src/codegen/transpiler_type_require.c`; the old
  `src/codegen/transpiler_emitters_type_require.inc` include body was deleted,
  reducing the source `.inc` cap to 159 and keeping
  `transpiler_emitters_base_a_part_a.inc` at 905 LOC. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus touched
  path `git diff --check`.
- Lean debt-slice follow-up: C backend extern declaration emission now has a
  real owner in `src/codegen/transpiler_extern.c`; `emit_extern_block(...)` was
  removed from `transpiler_emitters_base_b_part_b.inc`, reducing that near-cap
  include body from 998 LOC to 957 LOC. `tests/inc_sentinel_smoke.sh` now uses
  the current 159 source-`.inc` cap by default. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus touched
  path `git diff --check`.
- Lean debt-slice follow-up: C backend type declarator rendering now has a real
  owner in `src/codegen/transpiler_type_declarator.c`; event-handler
  declarators, function pointer declarators, and function signatures were
  removed from `transpiler_helpers_core_b_part_c.inc`, reducing it from 992 LOC
  to 849 LOC. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus touched
  path `git diff --check`.
- Lean debt-slice follow-up: C backend LogBanner normalization now has a real
  owner in `src/codegen/transpiler_log_normalize.c`; multiline indentation
  normalization was removed from `transpiler_expr_emitters_part_a.inc`,
  reducing it from 991 LOC to 878 LOC. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus touched
  path `git diff --check`.
- Lean debt-slice follow-up: generated-C runtime intent exit cleanup now has a
  private inline owner in `src/runtime/pgy_runtime_intent_exit.h`;
  `pgy_intent_exit_export(...)` keeps the same inline ABI name, while
  `pgy_runtime_part_ba_part_b.inc` drops from 996 LOC to 894 LOC. Local gate
  used: `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, plus touched path
  `git diff --check`.
- Lean debt-slice follow-up: generated-C DeviceSlot/SecureSlot macro bodies now
  have a private inline owner in `src/runtime/pgy_runtime_slot_macros.h`;
  built-in instantiation remains in `pgy_runtime_builtin_storage_inline.h`, which
  drops from 996 LOC to 808 LOC. Local gate used:
  `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, plus touched path
  `git diff --check`.
- Lean debt-slice follow-up: generated-C intent last-history step accessors now
  have a private inline owner in `src/runtime/pgy_runtime_intent_history.h`;
  `pgy_runtime_part_ba_part_a.inc` drops from 989 LOC to 867 LOC while the
  borrowed string ABI remains guarded. `runtime_abi_lifetime_smoke.sh` now reads
  the private inline headers that participate in the generated-C runtime family.
  Local gate used: `make backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, plus touched path
  `git diff --check`.
- Lean debt-slice follow-up: generated-C intent last/active borrowed exports now
  have a private inline owner in
  `src/runtime/pgy_runtime_intent_active_exports.h`; the registry/state half
  remains in `pgy_runtime_part_ba_part_a.inc`, which drops from 867 LOC to
  558 LOC. `runtime_abi_lifetime_smoke.sh` now tracks active and recent export
  owners separately so future movement cannot hide behind concatenated runtime
  text. Local gate used: `make runtime-abi-lifetime-test-smoke
  backend-inc-size-test-smoke inc-sentinel-test-smoke`, plus `make -B pgy
  runtime-panic-codegen-test-smoke runtime-panic-abi-test-smoke test-abi`.
- Lean debt-slice follow-up: LLVM-linkable intent borrowed exports now have a
  matching private owner in `src/runtime/pgy_runtime_lib_intent_exports.h`;
  `pgy_runtime_lib_part_b_part_c.inc` drops from 852 LOC to 315 LOC while
  keeping `intent_active`, `intent_recent`, and `intent_failure` ABI pipeline
  cases green on C and LLVM. This keeps generated-C inline and LLVM-linkable
  runtime export ownership symmetric instead of letting `part_b_part_c.inc`
  carry mixed intent-observability and slot-operation bodies.
- Lean debt-slice follow-up: LLVM method-call projection sync helpers now have
  a private owner in `src/codegen/llvm_expr_call_projection_sync.h`;
  `llvm_expr_call_methods_part_a.inc` drops from 880 LOC to 671 LOC while the
  world/zone projection sync call sites keep the same include order. Local gate
  used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke` plus
  targeted backend compare for `world_embedded_branch_projection_visibility`,
  `world_embedded_action_frontier`, `world_embedded_action_pool_frontier`, and
  `world_zone_projection_visibility`.
- Lean debt-slice follow-up: LLVM method-call domain action sync and
  slice/member-call helpers now have a private owner in
  `src/codegen/llvm_expr_call_methods_domain_slice.h`; the remaining
  `llvm_expr_call_methods_part_a.inc` body is removed while `llvm_expr.c`
  include order remains stable. The production source `.inc` inventory is now
  92 files / 28,467 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM call dispatch now has a private owner in
  `src/codegen/llvm_expr_call_dispatch.h`; the former
  `llvm_expr_calls_main.inc` body is removed while the call-family shim order
  remains stable. The production source `.inc` inventory is now 91 files /
  27,842 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM expression host/self, projection binding,
  spawn expression, operator suffix, enum lookup, and number/string literal
  helpers now have a private owner in
  `src/codegen/llvm_expr_host_spawn_literal_helpers.h`; the former
  `llvm_expr_helpers_part_b.inc` body is removed while `llvm_expr.c` helper
  include order remains stable. The production source `.inc` inventory is now
  90 files / 27,221 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend role method emission, ability/vtable
  emission, hidden provenance helpers, and role operator aliases now have a
  private owner in `src/codegen/transpiler_domain_role_ability_emit.h`; the
  former `transpiler_domain_role_part_a.inc` body is removed while the
  domain-role shim order remains stable. The production source `.inc` inventory
  is now 89 files / 26,601 LOC. Local gate to use for this slice:
  `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM expression boundary call argument helpers,
  projection field helpers, world/zone lookup helpers, and host-class lookup
  helpers now have a private owner in
  `src/codegen/llvm_expr_boundary_projection_helpers.h`; the former
  `llvm_expr_helpers_part_a.inc` body is removed while `llvm_expr.c` helper
  include order remains stable. The production source `.inc` inventory is now
  88 files / 25,996 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend MIR routine lookup, active SSA name
  resolution/rendering, token-local filtering, and local type-name lookup now
  have a private owner in `src/codegen/transpiler_mir_ssa_names.h`; the former
  `transpiler_emitters_mir_inventory_ssa_names.inc` body is removed while the
  MIR inventory/SSA shim order remains stable. The production source `.inc`
  inventory is now 87 files / 25,395 LOC. Local gate to use for this slice:
  `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend primitive, slot/channel, constructed
  generic, and local type-name rendering now have a private owner in
  `src/codegen/transpiler_type_mapping_helpers.h`; the former
  `transpiler_helpers_core_types.inc` body is removed while the helper-core shim
  order remains stable. The production source `.inc` inventory is now 86 files /
  24,796 LOC. Local gate to use for this slice: `make -B pgy test-transpile
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend world sync declaration, select lowering,
  and event declaration/subscription lowering now have a private owner in
  `src/codegen/transpiler_world_select_event_emit.h`; the former
  `transpiler_domain_role_part_d.inc` body is removed while the domain-role shim
  order remains stable. The production source `.inc` inventory is now 85 files /
  24,198 LOC. Local gate to use for this slice: `make -B pgy test-transpile
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM expression assignment, member lvalue/member
  access, projection invalidation, and embedded world projection assignment sync
  now have a private owner in
  `src/codegen/llvm_expr_assignment_member_projection.h`; the former
  `llvm_expr_values.inc` body is removed while `llvm_expr.c` include order
  remains stable. The production source `.inc` inventory is now 84 files /
  23,617 LOC. Local gate to use for this slice: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM-linkable runtime authority rejection state,
  checked arithmetic exports, panic invariant export, and file-path
  normalization helpers now have a private owner in
  `src/runtime/pgy_runtime_lib_authority_file_core.h`; the former
  `pgy_runtime_lib_part_a.inc` body is removed while `pgy_runtime_lib.c` include
  order remains stable. The production source `.inc` inventory is now 83 files /
  23,031 LOC. Local gate to use for this slice: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-abi-test-smoke test-abi
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM-linkable raw set tail exports, intent
  active/recent registry helpers, intent trace mutation, and MIR trace hooks now
  have a private owner in
  `src/runtime/pgy_runtime_lib_set_intent_trace_exports.h`; the former
  `pgy_runtime_lib_part_b_part_b.inc` body is removed while `pgy_runtime_lib.c`
  include order remains stable. The production source `.inc` inventory is now
  82 files / 22,449 LOC. Local gate to use for this slice: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-abi-test-smoke test-abi
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: RIR flow semantic flags, state merge rules, and
  HIR CFG enrichment now have a private owner in `src/compiler/rir_flow.h`; the
  former `rir_flow.inc` body is removed while `rir.c` include order remains
  stable. The production source `.inc` inventory is now 81 files / 21,877 LOC.
  Local gate to use for this slice: `make -B pgy type-resolution-dag-test-smoke
  air-drift-test-smoke cfg-body-dataflow-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend MIR local type lookup, explicit binding
  registration, MIR function signature support checks, SSA expression emission,
  phi copy emission, and exit-SSA lookup now have a private owner in
  `src/codegen/transpiler_mir_ssa_emit.h`; the former
  `transpiler_emitters_mir_inventory_ssa_emit.inc` body is removed while the MIR
  inventory/SSA shim order remains stable. The production source `.inc`
  inventory is now 80 files / 21,313 LOC. Local gate to use for this slice:
  `make -B pgy test-transpile type-resolution-dag-test-smoke
  air-drift-test-smoke cfg-body-dataflow-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: generated-C threaded channel and SPSC channel
  inline macro definitions plus stable `Int`/`String` instantiations now have a
  private owner in `src/runtime/pgy_runtime_channel_inline.h`; the former
  `pgy_runtime_part_bb.inc` body is removed while `pgy_runtime.h` include order
  remains stable. The production source `.inc` inventory is now 79 files /
  20,752 LOC. Local gate to use for this slice: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke test-abi llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend zone struct emission, projection
  readiness/dirty fields, layer/state frontier sync, bounded recompute, and
  hosted zone method lowering now have a private owner in
  `src/codegen/transpiler_zone_decl_emit.h`; the former
  `transpiler_domain_role_part_c.inc` body is removed while the domain-role shim
  order remains stable. The production source `.inc` inventory is now 78 files /
  20,198 LOC. Local gate to use for this slice: `make -B pgy test-transpile
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend block emission and intent step helper
  tails now have a private owner in
  `src/codegen/transpiler_block_intent_helpers.h`; the former
  `transpiler_emitters_base_b_part_c.inc` body is removed while the base-B shim
  order remains stable. The production source `.inc` inventory is now 77 files /
  19,652 LOC. Local gate to use for this slice: `make -B pgy test-transpile
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: generated-C inline file/string helpers and Qubit
  toy runtime helpers now have a private owner in
  `src/runtime/pgy_runtime_io_qubit_inline.h`; the former
  `pgy_runtime_part_c.inc` body is removed while `pgy_runtime.h` include order
  remains stable. Runtime cache freshness dependencies were also updated away
  from previously deleted runtime include paths. The production source `.inc`
  inventory is now 76 files / 19,110 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke test-abi llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend domain/party constructor lowering and
  Result/Option builtin call lowering now have a private owner in
  `src/codegen/transpiler_call_constructor_result_emit.h`; the former
  `transpiler_expr_emitters_part_c.inc` body is removed while the expression
  emitter shim order remains stable. The production source `.inc` inventory is
  now 75 files / 18,573 LOC. Local gate to use for this slice: `make -B pgy
  test-transpile backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: generated-C builtin storage instantiations and
  inline HashMap helpers now have a private owner in
  `src/runtime/pgy_runtime_builtin_storage_inline.h`; the former
  `pgy_runtime_part_ba_part_c.inc` body is removed while
  `pgy_runtime_part_ba.inc` include order remains stable. Runtime ABI/panic
  smokes and compiler cache freshness dependencies now read the new owner path.
  The production source `.inc` inventory is now 74 files / 18,038 LOC. Local
  gate to use for this slice: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-abi-lifetime-test-smoke
  runtime-panic-abi-test-smoke runtime-panic-contract-test-smoke test-abi
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: semantic overlay/host method typing, nominal
  boundary classification, zone effect-layer checks, and movable resource
  predicates now have a private owner in
  `src/semantic/type_checker_host_helpers.h`; the former
  `type_checker_helpers_host.inc` body is removed while `type_checker.c`
  include order remains stable. The production source `.inc` inventory is now
  73 files / 17,448 LOC. Local gate to use for this slice: `make -B pgy
  test-semantic semantic-core-shape-test-smoke type-resolution-dag-test-smoke
  cfg-body-dataflow-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: semantic builtin name resolution and Slot/
  SecureSlot/DeviceSlot operation validation now have a private owner in
  `src/semantic/type_checker_builtins_slotops.h`; the former
  `type_checker_builtins_slotops.inc` body is removed while
  `type_checker_builtins.c` include order remains stable. The production source
  `.inc` inventory is now 72 files / 16,923 LOC. Local gate to use for this
  slice: `make -B pgy test-semantic semantic-core-shape-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: generated-C parallel macros, zone authority/
  generation validation, Result helpers, remote Result helpers, and Option
  helpers now have a private owner in
  `src/runtime/pgy_runtime_zone_result_option_inline.h`; the former
  `pgy_runtime_part_ba_part_e.inc` body is removed while
  `pgy_runtime_part_ba.inc` include order remains stable. Runtime ABI/panic/
  authority smokes and compiler cache freshness dependencies now read the new
  owner path. The production source `.inc` inventory is now 71 files /
  16,402 LOC. Local gate to use for this slice: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-authority-contract-test-smoke
  runtime-panic-contract-test-smoke test-abi llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend overlay projection invalidation,
  zone/effect propagation snippets, and world-state lookup helpers now have a
  private owner in `src/codegen/transpiler_projection_sync_helpers.h`; the
  former `transpiler_helpers_core_a_part_c.inc` body is removed while the
  helper-core-A shim order remains stable. The production source `.inc`
  inventory is now 70 files / 15,883 LOC. Local gate to use for this slice:
  `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: generated-C intent trace storage/registry helpers,
  active/recent trace updates, step ok/fail tracing, and MIR resource trace
  hooks now have a private owner in
  `src/runtime/pgy_runtime_intent_trace_inline.h`; the former
  `pgy_runtime_part_ba_part_a.inc` body is removed while
  `pgy_runtime_part_ba.inc` include order remains stable. Runtime ABI lifetime
  smoke and compiler cache freshness dependencies now read the new owner path.
  The production source `.inc` inventory is now 69 files / 15,370 LOC. Local
  gate to use for this slice: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-abi-lifetime-test-smoke test-abi
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM extended List/Set/HashMap raw-call lowering
  now has a private owner in
  `src/codegen/llvm_expr_call_collections_extended.h`; the former
  `llvm_expr_call_collections_extended.inc` body is removed while
  `llvm_expr_calls.inc` include order remains stable. The production source
  `.inc` inventory is now 68 files / 14,862 LOC. Local gate to use for this
  slice: `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend helper-root string helpers, MIR
  resource-op/DEF helper emission, and the expression-emitter include root now
  have a private owner in `src/codegen/transpiler_helpers.h`; the former
  `transpiler_helpers.inc` body is removed while `transpiler.c` include order
  remains stable. The production source `.inc` inventory is now 67 files /
  14,356 LOC. Local gate to use for this slice: `make -B pgy test-transpile
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend Log/LogRaw/LogBanner lowering and core
  binary expression lowering now have a private owner in
  `src/codegen/transpiler_expr_core_emit.h`; the former
  `transpiler_expr_emitters_part_a.inc` body is removed while
  `transpiler_expr_emitters.inc` include order remains stable. The production
  source `.inc` inventory is now 66 files / 13,869 LOC. Local gate to use for
  this slice: `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend role ability/method lookup and
  Result/Option/collection specialization collection now have a private owner in
  `src/codegen/transpiler_specialization_helpers.h`; the former
  `transpiler_helpers_core_b_part_b.inc` body is removed while
  `transpiler_helpers_core_b.inc` include order remains stable. The production
  source `.inc` inventory is now 65 files / 13,402 LOC. Local gate to use for
  this slice: `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend ability/role/party/roster/relation/
  effect declaration emission now has a private owner in
  `src/codegen/transpiler_domain_nominal_emit.h`; the former
  `transpiler_domain_role_part_b.inc` body is removed while
  `transpiler_domain_role.inc` include order remains stable. The production
  source `.inc` inventory is now 64 files / 12,937 LOC. Local gate to use for
  this slice: `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend `emit_expression()` dispatch now has a
  private owner in `src/codegen/transpiler_expr_dispatch_emit.h`; the former
  `transpiler_expr_emitters_part_f.inc` body is removed while
  `transpiler_expr_emitters.inc` include order remains stable. Runtime panic
  contract smoke now reads the new owner path for checked array/slice lowering.
  The production source `.inc` inventory is now 63 files / 12,486 LOC. Local
  gate to use for this slice: `make -B pgy test-transpile
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-contract-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: MIR public names/destroy/validate/dump surface now
  has a private owner in `src/compiler/mir_public_surface.h`; the former
  `mir_public_part_b.inc` body is removed while `mir.c` include order remains
  stable. The production source `.inc` inventory is now 62 files / 12,066 LOC.
  Local gate to use for this slice: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke type-resolution-dag-test-smoke air-drift-test-smoke
  cfg-body-dataflow-test-smoke mir-declaration-inventory-test-smoke
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: semantic generic parameter lookup, default-bound
  validation, and class-specialization where-bound validation now have a private
  owner in `src/semantic/type_checker_generic_contracts.h`; the former
  `type_checker_generic_contracts.inc` body is removed while
  `type_checker_generic_support.h` preserves include order. The production
  source `.inc` inventory is now 61 files / 11,663 LOC. Local gate to use for
  this slice: `make -B pgy test-semantic semantic-core-shape-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM member-call dispatch and nominal hosted-method
  self argument lowering now have a private owner in
  `src/codegen/llvm_member_call_emit.h`; the former
  `llvm_expr_call_methods_part_b.inc` body is removed while `llvm_expr.c`
  preserves include order. The production source `.inc` inventory is now 60
  files / 11,262 LOC. Local gate to use for this slice: `make -B pgy
  test-transpile backend-inc-size-test-smoke inc-sentinel-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: generated-C runtime platform includes, contract
  headers, warning helpers, path normalization, and IO sandbox checks now have a
  private owner in `src/runtime/pgy_runtime_platform_io_core.h`; the former
  `pgy_runtime_part_a.inc` body is removed while `pgy_runtime.h` preserves
  include order. Runtime authority and panic contract smokes now read the named
  owner path. The production source `.inc` inventory is now 59 files / 10,879
  LOC. Local gate to use for this slice: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-authority-contract-test-smoke
  runtime-panic-contract-test-smoke runtime-abi-lifetime-test-smoke test-abi
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: semantic alias resolution stack handling, alias
  materialization, function-type formatting, and embedded world-zone mutation
  rejection now have a private owner in
  `src/semantic/type_checker_resolution_helpers.h`; the former
  `type_checker_helpers_resolution.inc` body is removed while `type_checker.c`
  preserves include order. The production source `.inc` inventory is now 58
  files / 10,500 LOC. Local gate to use for this slice: `make -B pgy
  test-semantic semantic-core-shape-test-smoke type-resolution-dag-test-smoke
  cfg-body-dataflow-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: generated-C List and Set inline runtime
  definitions now have a private owner in
  `src/runtime/pgy_runtime_list_set_inline.h`; the former
  `pgy_runtime_part_ba_part_d.inc` body is removed while
  `pgy_runtime_part_ba.inc` preserves include order. Runtime ABI and panic
  contract smokes now read the named owner path. The production source `.inc`
  inventory is now 57 files / 10,123 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-contract-test-smoke test-abi
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM identifier emission, direct Slot/SecureSlot
  fallback lowering, slot target resolution, and banner literal normalization
  now have a private owner in
  `src/codegen/llvm_expr_identifier_slot_helpers.h`; the former
  `llvm_expr_helpers_part_c.inc` body is removed while `llvm_expr.c` preserves
  include order. The production source `.inc` inventory is now 56 files / 9,753
  LOC. Local gate to use for this slice: `make -B pgy test-transpile
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-contract-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: semantic spawn token boundary checks and channel
  send/recv ownership diagnostics now have a private owner in
  `src/semantic/type_checker_async_channel.h`; the former
  `type_checker_async_channel.inc` body is removed while `type_checker.c`
  preserves include order. The production source `.inc` inventory is now 55
  files / 9,384 LOC. Local gate to use for this slice: `make -B pgy
  test-semantic semantic-core-shape-test-smoke type-resolution-dag-test-smoke
  cfg-body-dataflow-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM callable/event signature helpers, scalar
  string coercion, binary lowering, unary lowering, and `?` propagation lowering
  now have a private owner in `src/codegen/llvm_expr_scalar_core.h`; the former
  `llvm_expr_core.inc` body is removed while `llvm_expr.c` preserves include
  order. Runtime panic contract smoke now reads the named owner path. The
  production source `.inc` inventory is now 54 files / 9,024 LOC. Local gate to
  use for this slice: `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-panic-contract-test-smoke
  llvm-test-backend-compare beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: semantic generic subject signature formatting and
  effective default generic argument derivation now have a private owner in
  `src/semantic/type_checker_generic_support.h`; the former
  `type_checker_generic_support.inc` body is removed while `type_checker.c`
  preserves include order. The production source `.inc` inventory is now 53
  files / 8,666 LOC. Local gate to use for this slice: `make -B pgy
  test-semantic semantic-core-shape-test-smoke type-resolution-dag-test-smoke
  cfg-body-dataflow-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM projection field-copy lowering and bounded
  projection sync loop generation now have a private owner in
  `src/codegen/llvm_domain_projection_sync_helpers.h`; the former
  `llvm_domain_helpers_part_b.inc` body is removed while `llvm_domain.c`
  preserves include order. Runtime frontier contract smoke now reads the named
  owner path. The production source `.inc` inventory is now 52 files / 8,333
  LOC. Local gate to use for this slice: `make -B pgy test-transpile
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-frontier-contract-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend parallel block emission and async block
  spawning now have a private owner in
  `src/codegen/transpiler_async_parallel_emit.h`; the former
  `transpiler_emitters_async_parallel.inc` body is removed while
  `transpiler_func_class_flow_emit.h` preserves include order. The production
  source `.inc` inventory is now 51 files / 8,016 LOC. Local gate to use for
  this slice: `make -B pgy test-transpile parallel-core-contract-test-smoke
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: semantic type resolution now has a private owner
  in `src/semantic/type_checker_resolve.h`; the former
  `type_checker_resolve.inc` body is removed while `type_checker_expr.h`
  preserves include order. The production source `.inc` inventory is now 50
  files / 7,701 LOC. Local gate to use for this slice: `make -B pgy
  test-semantic semantic-core-shape-test-smoke type-resolution-dag-test-smoke
  cfg-body-dataflow-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: semantic domain-query builtin validation now has a
  private owner in `src/semantic/type_checker_builtins_query_domain.h`; the
  former `type_checker_builtins_query_domain.inc` body is removed while
  `type_checker_builtins.c` preserves include order. The production source
  `.inc` inventory is now 49 files / 7,387 LOC. Local gate to use for this
  slice: `make -B pgy test-semantic semantic-core-shape-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: CFG/body-flow loop analysis now has a private
  owner in `src/semantic/type_checker_flow_loops.h`; the former
  `type_checker_flow_loops.inc` body is removed while `type_checker_flow.c`
  preserves include order. CFG/body dataflow smoke now reads the named owner
  path. The production source `.inc` inventory is now 48 files / 7,086 LOC.
  Local gate to use for this slice: `make -B pgy test-semantic
  semantic-core-shape-test-smoke type-resolution-dag-test-smoke
  cfg-body-dataflow-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend function-forward helpers now have a
  private owner in `src/codegen/transpiler_func_forward_helpers.h`; the former
  `transpiler_helpers_core_b_part_c.inc` body is removed while
  `transpiler_helpers_core_b.inc` preserves include order. The production
  source `.inc` inventory is now 47 files / 6,790 LOC. Local gate to use for
  this slice: `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: MIR lowering public API now has a private owner in
  `src/compiler/mir_lower_public_api.h`; the former `mir_public_part_a.inc`
  body is removed while `mir.c` preserves include order. MIR declaration
  inventory smoke now reads the named owner path. The production source `.inc`
  inventory is now 46 files / 6,500 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke air-drift-test-smoke
  cfg-body-dataflow-test-smoke mir-declaration-inventory-test-smoke
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: LLVM-linkable runtime intent/slot-core exports now
  have a private owner in
  `src/runtime/pgy_runtime_lib_intent_slot_core_exports.h`; the former
  `pgy_runtime_lib_part_b_part_c.inc` body is removed while
  `pgy_runtime_lib.c` preserves include order. Runtime ABI lifetime and panic
  contract smokes now read the named owner path. The production source `.inc`
  inventory is now 45 files / 6,212 LOC. Local gate to use for this slice:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-contract-test-smoke
  runtime-panic-abi-test-smoke test-abi llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend match lowering now has a private owner
  in `src/codegen/transpiler_match_emit.h`; the former
  `transpiler_emitters_match.inc` body is removed while
  `transpiler_func_class_flow_emit.h` preserves include order. The production
  source `.inc` inventory is now 44 files / 5,932 LOC. Local gate to use for
  this slice: `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke llvm-test-backend-compare
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend MIR SSA identifier contract helpers now
  have a private owner in `src/codegen/transpiler_mir_ssa_contract.h`;
  `transpiler_emitters_base_a_part_d.inc` drops from 849 LOC to 677 LOC. Local
  gate used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  test-transpile`.
- Lean debt-slice follow-up: C backend MIR emission contract/resource-hook
  helpers now have a private owner in
  `src/codegen/transpiler_mir_emission_contract.h`; the remaining
  `transpiler_emitters_base_a_part_d.inc` body is removed while the base-A shim
  keeps include order stable. The production source `.inc` inventory is now
  95 files / 30,368 LOC. Local gate to use for this slice:
  `make -B pgy test-transpile backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: RIR lowering/enrichment now has a private owner in
  `src/compiler/rir_builder.h`; the former `rir_builder.inc` body is removed
  while `rir.c` keeps the flow -> build -> names -> validation include order.
  The production source `.inc` inventory is now 94 files / 29,733 LOC. Local
  gate to use for this slice: `make -B pgy type-resolution-dag-test-smoke
  air-drift-test-smoke cfg-body-dataflow-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke beta-readiness-checklist-test-smoke
  documentation-quality-test-smoke`.
- Lean debt-slice follow-up: semantic function-body checking now has a private
  owner in `src/semantic/type_checker_program.h`; the former
  `type_checker_program.inc` body is removed while the top-level semantic TU
  include order remains stable. The production source `.inc` inventory is now
  93 files / 29,099 LOC. Local gate to use for this slice:
  `make -B pgy test-semantic semantic-core-shape-test-smoke
  cfg-body-dataflow-test-smoke type-resolution-dag-test-smoke
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  beta-readiness-checklist-test-smoke documentation-quality-test-smoke`.
- Lean debt-slice follow-up: C backend slot/device builtin expression emitters
  now have a private owner in `src/codegen/transpiler_slot_builtin_emit.h`;
  `transpiler_expr_emitters_part_a.inc` drops from 797 LOC to 531 LOC while
  preserving slot sugar, secure slot token, and runtime panic codegen smoke.
  Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke test-transpile runtime-panic-codegen-test-smoke`.
- Lean debt-slice follow-up: C backend expression type inference now has a
  private owner in `src/codegen/transpiler_expr_type_infer.h`;
  `transpiler_helpers_core_b_part_c.inc` drops from 797 LOC to 296 LOC. This
  keeps generic/default-return inference in the same include order while
  separating the expression-type owner from spawn/generic helper tails. Local
  gate used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  test-transpile`.
- Lean debt-slice follow-up: C backend statement dispatch now has a private
  owner in `src/codegen/transpiler_statement_dispatch.h`;
  `transpiler_emitters_base_b_part_c.inc` drops from 803 LOC to 546 LOC. This
  leaves `part_c` focused on block emission and intent helper tails instead of
  carrying the top-level statement switch. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile` plus
  targeted backend compare for `break_continue`, `parallel_channel_sum`, and
  `intent_header_interleaved`.
- Lean debt-slice follow-up: generated-C `HashMap<String>` and map-keys inline
  runtime now has a private owner in `src/runtime/pgy_runtime_map_string_inline.h`;
  `pgy_runtime_part_ba_part_d.inc` drops from 767 LOC to 377 LOC and is now
  focused on List/Set inline runtime. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-codegen-test-smoke test-abi`
  plus targeted backend compare for `map_get_string`, `map_keys`,
  `list_get_string`, `queue_pop_string`, and
  `intent_failure_observability_strings`.
- Lean debt-slice follow-up: C backend MIR function emission now has a private
  owner in `src/codegen/transpiler_mir_func_emit.h`;
  `transpiler_emitters_base_b_part_a.inc` drops from 766 LOC to 162 LOC. This
  keeps the MIR emit-state snapshot helpers in the original part while moving
  the large `emit_func_decl_from_mir_named(...)` body behind a named owner.
- Lean debt-slice follow-up: generated-C runtime array sort kernels and scalar
  std/log/math helpers now have private owners in
  `src/runtime/pgy_runtime_array_sort_inline.h` and
  `src/runtime/pgy_runtime_scalar_std_inline.h`;
  `pgy_runtime_part_ba_part_c.inc` drops from 759 LOC to 535 LOC and is now
  focused on built-in type instantiation plus HashMap core.
- Lean debt-slice follow-up: LLVM-linkable runtime core exports now have a
  private owner in `src/runtime/pgy_runtime_lib_core_exports.h`; logging,
  time/sleep, and `pgy_int_to_string(...)` moved out of
  `pgy_runtime_lib_part_b_part_a.inc`, reducing it from 986 LOC to 909 LOC.
  Local gate used: `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-abi-lifetime-test-smoke test-abi`, plus touched path
  `git diff --check`.
- Lean debt-slice follow-up: C backend `let` destructuring lowering now has a
  private owner in `src/codegen/transpiler_destructure_emit.h`;
  `transpiler_emitters_base_b_part_c.inc` drops from 976 LOC to 873 LOC. Local
  gate used: `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  targeted backend compare for `destructure_array` and
  `destructure_tuple_return`, plus touched path `git diff --check`.
- Lean debt-slice follow-up: generated-C queue macro and built-in queue
  implementations now have a private owner in
  `src/runtime/pgy_runtime_queue_inline.h`; `pgy_runtime_part_ba_part_e.inc`
  drops from 969 LOC to 773 LOC. Local gate used:
  `make pgy backend-inc-size-test-smoke inc-sentinel-test-smoke`,
  `make runtime-panic-codegen-test-smoke runtime-abi-lifetime-test-smoke
  test-abi`, targeted backend compare for `queue_pop_string` and
  `parallel_channel_sum`, plus touched path `git diff --check`.
- Lean debt-slice follow-up: generated-C `HashMap<Int>` key adapters for
  `Int`/`Long`/`Bool` keys now have a private owner in
  `src/runtime/pgy_runtime_map_int_key_inline.h`; `pgy_runtime_part_ba_part_d.inc`
  drops from 963 LOC to 815 LOC. Local gate used: `make -B pgy`,
  `make backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-codegen-test-smoke`, targeted backend compare for `map_keys` and
  `map_get_string`, plus touched path `git diff --check`.
- Lean debt-slice follow-up: LLVM-linkable primitive slot exports for
  `Slot<Double>`, `Slot<Bool>`, and `Slot<String>` now have a private owner in
  `src/runtime/pgy_runtime_lib_slot_exports.h`; `pgy_runtime_lib_part_b_part_d.inc`
  drops from 947 LOC to 790 LOC while exported ABI symbol names remain
  unchanged. Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-panic-abi-test-smoke
  runtime-panic-codegen-test-smoke runtime-abi-lifetime-test-smoke test-abi`.
- Lean debt-slice follow-up: LLVM-linkable standard string/conversion/math/random
  exports now have a private owner in `src/runtime/pgy_runtime_lib_std_exports.h`;
  `pgy_runtime_lib_part_b_part_e.inc` drops from 817 LOC to 761 LOC and now
  starts at the channel runtime section. `runtime_abi_lifetime_smoke.sh` now
  reads runtime-lib private owner headers so result-owned string checks follow
  the real include order. Local gate used: `make runtime-abi-lifetime-test-smoke
  test-abi backend-inc-size-test-smoke inc-sentinel-test-smoke`.
- Lean debt-slice follow-up: LLVM-linkable raw `List<T>` collection exports now
  have a private owner in `src/runtime/pgy_runtime_lib_list_raw_exports.h`;
  `pgy_runtime_lib_part_b_part_a.inc` drops from 909 LOC to 759 LOC and is now
  focused on raw queue/map exports. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-codegen-test-smoke runtime-abi-lifetime-test-smoke test-abi`.
- Lean debt-slice follow-up: MIR declaration-header inventory helpers now have
  a private owner in `src/compiler/mir_decl_headers.h`; `mir_public_part_a.inc`
  drops from 959 LOC to 789 LOC and now starts at `mir_lower(...)`. Local gate
  used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke air-drift-test-smoke test-abi`.
- Lean debt-slice follow-up: RIR public vocabulary name helpers now have a
  private owner in `src/compiler/rir_names.h`; `rir_public.inc` drops from
  911 LOC to 804 LOC while RIR validation/dump vocabulary remains unchanged.
  Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke type-resolution-dag-test-smoke air-drift-test-smoke
  test-abi`.
- Lean debt-slice follow-up: C backend parallel capture analysis now has a
  private owner in `src/codegen/transpiler_parallel_capture.h`;
  `transpiler_emitters_base_b_part_b.inc` drops from 957 LOC to 730 LOC while
  parallel capture typing and slot capture behavior remain unchanged. Local
  gate used: `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  parallel-core-contract-test-smoke runtime-panic-codegen-test-smoke` plus
  targeted backend compare for `parallel_channel_sum`.
- Lean debt-slice follow-up: C backend stdlib call lowering now has a private
  owner in `src/codegen/transpiler_expr_stdlib_builtin.h`;
  `transpiler_expr_emitters_part_d.inc` drops from 946 LOC to 26 LOC while
  stdlib/string/collection call behavior remains unchanged. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-panic-codegen-test-smoke` plus targeted backend compare for
  `string_io`, `array_builtins`, `list_get_string`, and `map_get_string`.
- Lean debt-slice follow-up: C backend overlay/projection invalidation and
  zone-layer bind helpers now have a private owner in
  `src/codegen/transpiler_overlay_projection.h`; the old
  `transpiler_helpers_core_a_part_b.inc` include body was removed, lowering the
  source `.inc` count to 158. `runtime_frontier_contract_smoke.sh` now checks
  the real world frontier owner in `transpiler_domain_role_part_d.inc` instead
  of the adjacent zone frontier part. Local gate used:
  `make runtime-frontier-contract-test-smoke backend-inc-size-test-smoke
  inc-sentinel-test-smoke` plus targeted backend compare for
  `world_embedded_branch_projection_visibility` and
  `world_embedded_action_frontier`.
- Lean debt-slice follow-up: C backend `let` declaration lowering now has a
  private owner in `src/codegen/transpiler_let_emit.h`;
  `transpiler_emitters_base_a_part_a.inc` drops from 905 LOC to 138 LOC while
  MIR inventory/SSA helper declarations remain in the original base-A part.
  Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke test-transpile` plus targeted backend compare for
  `destructure_array`, `array_builtins`, and `map_keys`.
- Lean debt-slice follow-up: C backend MIR block statement emission now has a
  private owner in `src/codegen/transpiler_mir_block_emit.h`; the old
  `transpiler_emitters_base_a_part_c.inc` include body was removed. Source
  `.inc` total drops to 49,911 LOC, with only `transpiler_emitters_intent.inc`
  still above 900 LOC. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke test-transpile
  type-resolution-dag-test-smoke air-drift-test-smoke` plus targeted backend
  compare for `destructure_array`, `destructure_tuple_return`,
  `host_method_class_return`, and `world_embedded_branch_projection_visibility`.
- Lean debt-slice follow-up: C backend intent declaration emission now has a
  private owner in `src/codegen/transpiler_intent_emit.h`; the old
  `transpiler_emitters_intent.inc` include body was removed. Source `.inc`
  total drops to 48,949 LOC, and no production `.inc` file remains above 900
  LOC. Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke test-transpile runtime-panic-codegen-test-smoke` plus
  targeted backend compare for `intent_authority_snapshot` and
  `intent_failure_observability_strings`.
- Lean debt-slice follow-up: generated-C runtime intent-recent accessors,
  panic helpers, and checked arithmetic exports now have a private owner in
  `src/runtime/pgy_runtime_panic_checked_inline.h`;
  `pgy_runtime_part_ba_part_b.inc` drops from 894 LOC to 705 LOC and the
  runtime ABI lifetime inventory reads the new header in generated-runtime
  include order. Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-panic-codegen-test-smoke
  runtime-panic-abi-test-smoke runtime-abi-lifetime-test-smoke test-abi`.
- Current highest-value implementation order is now:
  1. CFG/body dataflow source-of-truth for function/action/intent safety.
  2. DAG source-of-truth completion for named symbols, module contracts, and
     generic consumer paths.
  3. AIR strict-evidence negative expansion for transfer/world/boundary cases.
  4. Runtime frontier scheduler generalization beyond the already-covered
     bounded recompute slices.
  5. ABI ownership/pinning parity and diagnostic quality gate hardening.
  6. Cross-platform support matrix enforcement.
- The first removable blocker under that list is still owner debt that slows
  every P0/P1/DAG/AIR change. MIR ABI layout lookup now has a private owner in
  `src/compiler/mir_abi_layout.h`; `mir_public_part_b.inc` drops from 753 LOC
  to 420 LOC and now focuses on MIR validation/dump surfaces. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke air-drift-test-smoke test-abi`.
- CFG contract validation now has a private owner in
  `src/compiler/mir_cfg_contract_validate.h`; `mir_public_part_a.inc` drops
  from 743 LOC to 290 LOC and no longer mixes public MIR entry points with
  cleanup/rollback/invalidation graph contract checks. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- RIR validation now has a private owner in `src/compiler/rir_validation.h`;
  `rir_public.inc` drops from 741 LOC to 269 LOC and now keeps only
  destroy/dump public surfaces. This makes AIR/CFG evidence validation a named
  owner instead of a mixed public include body. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- C backend MIR intent inventory helpers now have a named owner in
  `src/codegen/transpiler_mir_inventory_intent.h`; the old
  `transpiler_emitters_mir_inventory_intent.inc` include body is gone and the
  existing SSA include-order shim now references the owner header directly.
  Local gate used: `make -B pgy backend-inc-size-test-smoke
  inc-sentinel-test-smoke type-resolution-dag-test-smoke
  cfg-body-dataflow-test-smoke air-drift-test-smoke test-abi`.
- C backend call/spawn/channel expression emission now has a named owner in
  `src/codegen/transpiler_expr_call_spawn_emit.h`; the old
  `transpiler_expr_emitters_part_e.inc` body is gone and the expression emitter
  shim includes the owner header directly. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- C backend builtin-call dispatch now has a named owner in
  `src/codegen/transpiler_expr_builtin_dispatch.h`; the old
  `transpiler_expr_emitters_part_b.inc` body is gone and the expression emitter
  shim includes the owner header directly. This keeps builtin dispatch out of
  split `.inc` ownership without changing call lowering order. Local gate used:
  `make -B pgy backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- Semantic builtin-query checks now have a named owner in
  `src/semantic/type_checker_builtins_query.h`; the old
  `type_checker_builtins_query.inc` body is gone. The split
  `BuiltinKind builtin_resolve(...)` signature was also fixed so
  `type_checker_builtins_slotops.inc` owns a complete function boundary instead
  of inheriting a dangling return type from the query file.
- Semantic builtin nominal/type contract checks now have a named owner in
  `src/semantic/type_checker_builtins_nominal.h`; the old
  `type_checker_builtins_nominal.inc` body is gone while preserving
  `Rc`/`Weak`/`Box`/allocator and intent-observability builtin dispatch order.
- Generated-C runtime pool/FSM/timer helpers now have a named owner in
  `src/runtime/pgy_runtime_pool_fsm_timer_inline.h`; `pgy_runtime_part_ba_part_e.inc`
  now starts at parallel/zone authority support instead of mixing object-pool,
  FSM, timer, cooldown, authority, result, and option helpers in one body.
  Runtime ABI lifetime inventory and compiler runtime-cache freshness track
  the new owner header directly.
- Semantic expression checking now has a named owner in
  `src/semantic/type_checker_expr.h`; the old `type_checker_expr.inc` body is
  gone and CFG body-dataflow smoke follows the new owner path.
- C backend function/class/control-flow emission now has a named owner in
  `src/codegen/transpiler_func_class_flow_emit.h`; the old
  `transpiler_emitters_base_b_part_b.inc` body is gone while preserving the
  base-B include order.
- Generated-C runtime Box/Arena/Allocator/Array/Rc/primitive-slot helpers now
  have a named owner in `src/runtime/pgy_runtime_memory_array_slot_inline.h`;
  the old `pgy_runtime_part_ba_part_b.inc` body is gone. Runtime panic contract,
  ABI lifetime inventory, and compiler runtime-cache freshness track the new
  owner header directly.
- Semantic relation/effect/projection helper logic now has a named owner in
  `src/semantic/type_checker_helpers_effects.h`; the old
  `type_checker_helpers_effects.inc` body is gone and CFG body-dataflow smoke
  tracks the new helper path.
- LLVM domain core helpers now have a named owner in
  `src/codegen/llvm_domain_core_helpers.h`; the old
  `llvm_domain_helpers_part_a.inc` body is gone and `llvm_domain.c` includes
  the owner header directly. Local gate used: `make -B pgy
  backend-inc-size-test-smoke inc-sentinel-test-smoke
  type-resolution-dag-test-smoke cfg-body-dataflow-test-smoke
  air-drift-test-smoke test-abi`.
- LLVM-linkable runtime channel/qubit exports now have a named owner in
  `src/runtime/pgy_runtime_lib_channel_quantum_exports.h`; the old
  `pgy_runtime_lib_part_b_part_e.inc` body is gone and
  `runtime_abi_lifetime_smoke.sh` reads the new owner header in generated
  runtime include order. Local gate used: `make backend-inc-size-test-smoke
  inc-sentinel-test-smoke runtime-abi-lifetime-test-smoke test-abi`.
- LLVM-linkable raw Queue/Map/Set exports now have a named owner in
  `src/runtime/pgy_runtime_lib_raw_collection_exports.h`, and secure/device
  slot, array, file IO, and string helper exports now have a named owner in
  `src/runtime/pgy_runtime_lib_slot_array_io_string_exports.h`. The old
  `pgy_runtime_lib_part_b_part_a.inc` and `pgy_runtime_lib_part_b_part_d.inc`
  bodies are gone. Runtime panic/lifetime smokes now check the new owner
  headers and compiler runtime cache freshness tracks them directly. Local
  gate used: `make backend-inc-size-test-smoke inc-sentinel-test-smoke
  runtime-abi-lifetime-test-smoke runtime-panic-contract-test-smoke
  runtime-panic-codegen-test-smoke test-abi`.
- Rejected shortcut: using the alias symbol's already-materialized `sym->type`
  directly inside metadata alias lookup breaks module visibility and generic
  ability provenance tests. Alias DAG closure must preserve export/private
  provenance and effective generic-bound facts instead of trusting the symbol
  cache as the source of truth.
- Sprint process change: beta closure now uses a lean debt-slice loop. Pick one
  owner, complete the implementation slice, run the slice-local gate, and defer
  wider regression to the slice boundary. Full regression is still required
  before closure, but the inner loop must be implementation-first, not
  test-threshold-first.

## UTF-8 Progress Note - 2026-04-26 - DAG Owner Seam Centralization

- All owner-local type resolver seams now route through
  `semantic_type_resolution_lookup_or_materialize(...)` instead of owning direct
  fallback helper calls.
- The old `semantic_type_resolution_resolve_or_fallback(...)` helper is removed;
  `type-resolution-resolver-inventory-test-smoke` caps named fallback seams at 0
  and fails if new owner-local fallback users appear.
- Previous DAG smoke stats before nominal metadata materialization tightening:
  `graph-backed skips=3137 metadata_entries=2044
  metadata_owned=123 metadata_hits=3300 materializer_fallbacks=4135
  legacy_alias=83 legacy_non_alias=0 alias_materialized=5
  alias_diagnostic_fallback=78 alias_fallback_resolved=0
  alias_fallback_unresolved=78`.
- This is still not full DAG source-of-truth. The remaining closure is replacing
  the central fallback itself with graph/topo materialization for the imported
  ability/default/bound/module/nominal cases that still need legacy
  materialization.
- Verified locally: `make type-resolution-resolver-inventory-test-smoke
  type-resolution-dag-test-smoke` and `make test-semantic`.

留덉?留??낅뜲?댄듃: 2026-04-25

## 현재 ?태 ?정 ?? (2026-04-12 ?정??

### 醫낇빀 ?먮떒: Late-Stage Alpha

- 踰좏? readiness 異붿젙: ??`50%`
- 현재 ?현: `late-stage alpha / beta-closure sprint`
- 蹂댁젙 ?댁쑀:
  - 기능 측면만 보면 core/foundation 구현? ??? beta??기능 개수 아니며 end-to-end 신뢰도다
  - HIR/MIR CFG skeleton? ?? ??? 개수/action/intent body ?전의 semantic source-of-truth 아직 CFG/dataflow??격?? ?았?? all-path return, use-before-init, move/borrow join, drop cleanup, zone/effect transition, parallel/channel boundary?AST/helper traversal만으??으?strict beta ?뢰?? 족하??  - AIR abstraction safety??Phase 1 ?이??구조 / synthesis / drift checker baseline?driver semantic-validation wiring??되어한다. Intent ??implementation drift 출? `docs/104_air_compiler_architecture.md`? `make air-drift-test-smoke`?gate??되어?고, strict evidence??기본값으??격한다. missing RIR boundary/authority evidence??`PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`?hard-fail ?며, `authorized by` participant ?름?RIR authority fact / authorize op subject ?치?야 한다. authority evidence ?락 진단? `Reason:` 에 expected authority participant list??함한다. AIR drift message? synthesized intent/boundary/authority name? owned lifetime으로 리되? repeated drift check ?전 message??전?게 ?제는 ?? ?스?? parsed-source AIR teardown-safe boundary source ?? 한다. `where + transfer`?????상 zone boundary ?나??히 하고 zone boundary? world-handoff boundary?모두 ?성한다. world-handoff evidence???제 matching RIR intent scope만으??과?? 하고 boundary source alias?????RIR `Move`/`Claim` transfer op??구한다. parsed-source missing-authority-evidence negative? parsed-source IO execution-boundary missing-evidence negative??full driver JSON path?서 step source span?`stage/code/cause_ir/fix_source`까? 고정한다. expression boundary evidence?????상 owner-name-only RIR scope match??과?? ?는?? `PGY_AIR_STRICT_EVIDENCE=0`? 개발/?버?opt-out한다. `make air-backend-nonimpact-test-smoke`??relaxed AIR? default strict AIR intent/zone, cross-world transfer, handoff frontier, world projection, relation/effect, authority-failure fixture set?서 같? C/LLVM ?스?? ?성는 비교한다. `make air-backend-nonimpact-full-test-smoke`??full frozen backend-compare fixture sweep??같? 방식으로 ?리?Linux CI gate??격한다. `make air-strict-backend-compare-test-smoke`??strict evidence ?태?서 C/LLVM ?행 parity까? 증한?? parser/lexer baseline JSON routing? `stage`, `code`, `cause_ir`, `fix_source`까? ?혔?? ?? blocker??AIR transfer/world source negative ?장, Windows native evidence, parser-specific code split / multi-error accumulation한다
  - Type-resolution DAG 아직 semantic source-of-truth 아니?declaration order / module contract / generic consumer path drift ?험???아 한다
  - ?기 모듈??stop condition??아직 ?? semantic 800 LOC 초과 `.inc` 조건?runtime/codegen/compiler 1,000 LOC 초과 `.inc` 조건? ?혔? ?러 split? 아직 include-order 보존 ?태???제 owner/TU extraction 채? ?아 한다
  - ?라??공식 진행률? ?기???면 ?숙?? 아니며 ?베? ?뢰??readiness??기?으로 ??50%?본다

## Beta taxonomy freeze: core / foundation / style

베? 기?? ?제 기능 ?열??아니며 되어 ?체??기?으로 ?눈??

- Core language: `intent`, `world`, `zone`, `subject`, `relation`, `effect`, `projection`, `authority`, `handoff`, runtime observability, anchored ownership boundary, generic contract system, module visibility/export contract, `parallel`.
- Generic contract??core?? exact/ability/multi-bound/default type arg actual resolution? FP/OOP 의 아니며 domain contract??현는 ???되어??
- Foundation layer: primitive values, `func`, `let`, control flow, callable/lambda baseline, `Option`/`Result`, stable collections, core ?행???요??runtime ABI.
- Style / compatibility surface: OOP convenience, FP combinator libraries, app infra, richer async helpers.
- Execution family split: `parallel`? core execution primitive?고, `spawn`/`async`/`await`/`select`/`channel`/cancel? ??래 execution family?? fiber/coroutine? language core 아니며 runtime scheduling/suspension mechanism한다.
- Accelerator split: AI-first/GPU 방향? `pgy.accel.spray` ?리 모듈??약한다. 는 `parallel` / ownership / module visibility 에 ?라??accelerator library/runtime 축이?core keyword ?장??아니며 
- Render split: Skia/shader/render graph 방향? `pgy.render.skia` ?리 모듈??약한다. renderer/shader??core keyword 아니며 Spray/Execution 의 ?태?모듈한다.
- Compatibility split: OOP/FP/DOP??각각 `pgy.compat.oop`, `pgy.compat.fp`, `pgy.compat.dop`?분리한다. 기존 되어 ??을 ?용?되 core identity??명?? ?는??
- Interop split: ?? 되어 ?동(JVM 캐스??JNI 브릿, Python C-API ??? `pgy.interop.*` ?태?모듈?분류?며, 베? 마일?톤?서???전???외(Out of Beta)한다.

?낅뜲?댄듃 ?뺤콉:

- `pgy.core`?????주 개선?되 ??하고 강하?증한??
- `pgy.foundation`? core보다 ?리??직이?ABI/backend parity?깨? ?는??
- `pgy.accel.spray`, `pgy.render.skia`, `pgy.compat.*`, `pgy.std.*`, `pgy.kit.*`??모듈 ?태계로 진화한다. 빠른 ?험? ?용???core keyword??리 ?는??

?ㅽ뻾 洹쒖튃:

- B0 blocker??`core + foundation stable subset`?먮쭔 遺숈씤??
- `pgy.fp`??Functor/HKT 추상?? class-heavy OOP ?장, coroutine/fiber 고도는 beta identity blocker 아니며 
- `pgy.accel.spray`??post-beta design surface?? 베? ?에????GPU ?워?나 backend-specific CUDA/ROCm/Metal 문법???? ?고, module boundary? ownership ?칙?고정한다.
- `pgy.render.skia`? `pgy.compat.dop`??post-beta design surface?? 베? ?에??shader/layout keyword??? 하고 module boundary?고정한다.
- ?? `parallel`? core???slot/resource/effect conflict, cancellation/fairness, C/LLVM lowering parity??beta ?질 기?으로 계속 리한??
- Source of truth: `docs/99_language_module_taxonomy.md`
- Machine-readable manifest: `docs/language_module_manifest.json`
- Representative case tags: `docs/language_module_cases.json`
- Drift gate: `make module-taxonomy-test-smoke`
- Parallel core/execution split gate: `make parallel-core-contract-test-smoke`
- Operational beta checklist: `docs/100_beta_readiness_checklist.md`

## Formal semantics / mathematical proof obligations

베????테?트 ?과?다?만으로 ?히 ?는?? stable subset마다 ???보존, 진행, ownership safety, authority soundness, projection freshness, DAG soundness, module visibility non-interference, backend parity 같? ?학??불?이 문서?되?야 한다.

- Source of truth: `docs/semantics/`
- Stable index: `docs/102_formal_semantics_and_proof_obligations.md`
- Drift gate: `make formal-semantics-test-smoke`
- ?곹깭: `IN PROGRESS / BLOCKER-DOC`
- 踰좏? 湲곗?:
  - [x] ?학 library 문서(`docs/45_math_layer_design.md`)? 되어 ???증명 문서?분리한다.
  - [x] stable beta subset??semantic domain, judgment, theorem/proof-obligation vocabulary?고정한다.
  - [ ] B0 ??ぉ留덈떎 theorem statement + current regression evidence + remaining proof obligation??理쒖떊 肄붾뱶 ?곹깭? 留욎텣??
  - [ ] runtime propagation, DAG, MIR declaration inventory, ABI ownership, C/LLVM parity???⑥? blocker瑜?proof obligation?쇰줈 異붿쟻?쒕떎.
  - [ ] beta 문구?서 Lean/Coq/기계증명 ?료처럼 보이???현??금?한다. 기계증명? 별도 executable model 는 proof assistant artifact ?기??까 post-beta/v1 hardening으로 한다.
  - [~] **[NEW]** Runtime panic / unwinding model (abort vs unwind)???책 명시 ?C/LLVM backend parity 증명 추?. Panic class vocabulary? released-slot / invalid-secure-token / double-release / device-slot / out-of-bounds / authority-mismatch / OOM / divide-by-zero / internal-invariant hard-fail contract??`src/runtime/pgy_runtime_panic_contract.h`, `make runtime-panic-contract-test-smoke`, `make runtime-panic-abi-test-smoke`, `make runtime-panic-codegen-test-smoke`?고정한다. Generated C/LLVM `Array<T>`/`Slice<T>` indexing, temporary function-return indexing, `ArraySet`, `ListGet`, `QueuePop`, `MapGet`, `ListSet`, `ListRemove`, `MapRemove` invalid access? `Unwrap(Err)` / `UnwrapOption(None)` misuse??checked runtime helper / panic contract?고정한다. ?? 것? ??panic class 추????마??같? executable parity gate??구는 것이??
  - [~] **[NEW]** Secure slot ?authority token?????불??성(Unforgeability) ?식 불???Formal Invariants) 문서?? Secure slot invalid-token/denied-capability export path??silent fallback?서 panic contract??동한다.
  - [ ] **[NEW]** Intent ?스의 Rollback/Cleanup 보장?????Formal Closure (?태 기계 증명) 문서??

?댁쁺 洹쒖튃:

- ?스???모??백엔??비교??proof evidence?? proof ?체 아니며 
- undocumented mathematical assumption???요??surface??stable??아니며 `IN PROGRESS`, `explicit reject`, 는 `OUT OF BETA`??려??한다.
- FP functor/HKT, full ownership, full quantum, GPU/Spray, Skia/render graph??현재 beta proof scope 밖이??

## Missing beta gate audit

현재 strict beta 기??서???음 ????별도 gate?본다. ?????? 기능 ?장??아니며 ?? 는 core/runtime/tooling ?면???뢰??계약한다.

- [~] Runtime panic / unwinding model: OOM, divide-by-zero, out-of-bounds, slot violation, token mismatch, authority mismatch, invariant break??abort/unwind/recoverable ?책??`Runtime Panic Parity` proof obligation으로 ?렸?? `src/runtime/pgy_runtime_panic_contract.h` panic class vocabulary??유?고, inline/exported typed slot read/write/release??released-slot ?double-release?서 ???상 기본?no-op?빠? ?는?? `make runtime-panic-abi-test-smoke` released-slot, invalid-secure-token, double-release, device-slot, out-of-bounds, authority-mismatch, OOM, divide-by-zero executable evidence??공한다. `make runtime-panic-codegen-test-smoke`??generated C/LLVM divide/modulo-by-zero? `Array<T>`/`Slice<T>` index, temporary function-return index, `ArraySet`, `ListGet`, `QueuePop`, `MapGet`, `ListSet`, `ListRemove`, `MapRemove` invalid access, `Unwrap(Err)`, `UnwrapOption(None)` parity?증한?? ?? 것? ??hard-fail class 추????마??같? executable parity gate??구는 것이??
- [~] Secure slot / authority secret invariant: token unforgeability, secure-slot mismatch denial, authority token non-forgeability, authority transfer single-owner invariant, runtime snapshot secret non-exposure?`Secure Token Unforgeability` / `Authority Transfer Single-Owner` proof obligation으로 ?렸?? inline/exported secure slot read/write/release invalid-token ?denied-capability path??`PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN`?고정하고 secure-slot double-release??`PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE`?고정한다. `make runtime-panic-abi-test-smoke` invalid-token/double-release executable evidence??공한다. authority-token mismatch??`authority-token-mismatch` runtime code/reason, `make test-security`, `authority_failure_abi`, `authority_failure_surface`?C/LLVM parity regression까? ?았?? unsupported authority-token transport??channel send/receive/helper/close, cancellation payload, direct named `spawn`?서 explicit reject??았?? ?? 것? richer domain-boundary denial한다.
- [ ] Intent formal closure: step ordering, compensation/rollback/invalidation, effect propagation, observability ABI stability?beta-stable contract?고정한다.
- [ ] Zone/world/authority/handoff formal closure: zone generation, world embedding, handoff frontier, projection freshness, authority rejection query surface?beta-stable contract?고정한다.
- [ ] Diagnostic quality gate: 모든 user-facing error severity, stable code, source span when available, `Reason:`, `Fix:`?갖도??질 기???registry smoke? 별도 gate?한다.
  - 진행: intent clause explicit reject ?`spawn`/channel control-transfer AST parser source span??보존?도?고쳤? `make diagnostics-json-test-smoke` `on: spawn ...`? `on: ch <- value`??`PGY_SEM_INTENT_STEP_INVALID` JSON line/column + `cause_ir` + `fix_source`?고정한다.
- [ ] Cross-platform CI matrix: Linux/WSL, Windows native/MSYS2/MinGW, macOS??support level??stable/experimental/out-of-beta濡?紐낆떆?쒕떎.
  - 진행: Windows LLVM support detection? executable `llvm-config --libs core` evidence 을 ?만 `WINDOWS_LLVM_READY=1`???도?좁혔?? `C:/Program Files/LLVM/lib` 같? library folder 존재만으?LLVM smoke/backend-compare??행?? ?는?? 현재 beta 계약? Linux C+LLVM, Windows C-only?며 Windows LLVM? ?제 MSYS2 runner green evidence ?길 ?만 ?격한다.
  - 진행: README support matrix??macOS??dedicated runner/support contract ?길 ?까 out-of-beta?명시한다.
- [~] Beta stable subset definition: keyword, syntax, API, AST-visible shape, runtime ABI, backend parity 범위?`docs/107_beta_stable_subset.md`?서 freeze한다. ?? ?? ??문서???stable ?????당 semantic/runtime/C/LLVM regression row? 1:1??결는 것이??
- [~] Stdlib beta freeze list: stable/experimental/out-of-beta API? breaking-change policy瑜?紐낆떆?쒕떎.
  - 진행: `docs/108_stdlib_beta_freeze.md` builtin stdlib, stable `use` modules, known experimental modules, out-of-beta ecosystem work?분리한다. `make stdlib-test-smoke` builtin stdlib probe? stable `use` module probe?C/LLVM ?쪽?서 고정한다. ?? ?? third-party package/version/supply-chain policy??
- [~] Tooling conformance: LSP/fmt/debugger??beta-stable 踰붿쐞瑜?紐낆떆?쒕떎.
  - 진행: `make tooling-conformance-test-smoke` formatter idempotence/compile smoke, LSP initialize/hover/completion capability, debugger CLI parse+semantic+quit path?executable gate?고정한다. DAP, binary breakpoint, variable watch, rich refactor, multi-file workspace LSP??아직 beta-stable tooling subset??아니며 
- [~] Package/module resolver surface: manifest, version resolution, import path, supply-chain integrity?stable/experimental/out-of-beta?분류한다.
  - 진행: `docs/109_package_module_resolver_contract.md` beta-stable module surface?`import "relative/path.pgy";`, importing-file-relative resolution, namespace/export visibility, circular import rejection으로 고정한다. package surface??`pgy init <name>` scaffolding?stable한다.
  - 진행: `pgy install`? ???상 ?스 ?일 경로??인?? 하고 explicit out-of-beta rejection??한다. `make package-module-resolver-test-smoke` doc contract, `pgy init`, `pgy install` reject, missing import JSON, circular import JSON??고정한다.
  - ?⑥쓬: dependency version solving, lockfile, registry, checksum/signature verification, remote import, supply-chain integrity??beta ?댄썑 resolver/package-manager track?쇰줈 ?좎??쒕떎.
- [~] Test quality gate: pre-beta mandatory suite, fuzz/property status, coverage/perf baseline??異붿쟻?쒕떎.
  - 진행: `docs/111_beta_test_suite_freeze.md` mandatory pre-beta gates, platform gates, fuzz/property/coverage non-claims, regression policy?freeze한다. `make beta-test-suite-freeze-test-smoke` freeze doc?Makefile target 존재??한??
  - ?음: ?제 fuzz corpus, property-based generator, coverage percentage threshold??beta ?후 ?질 ?랙으로 ??한다. 현재 beta gate??named stable-surface coverage??
- [~] Observability/tracing schema: event schema, intent history, authority failure state, runtime registry, trace format version??고정한다.
  - 진행: `docs/112_observability_trace_schema.md` beta-stable schema?`IntentLast*`, `IntentHistory*`, `IntentActive*`, `IntentRecent*`, authority failure snapshot(`ok/zone/participant/code/reason`), runtime-borrowed string ABI, C/LLVM identical trace output으로 고정한다.
  - 진행: `make observability-schema-test-smoke` `intent_trace_abi`, `intent_recent_abi`, `intent_active_abi`, `intent_failure_abi`, `authority_failure_abi`?C/LLVM ?쪽?서 expected stdout?비교한다.
  - ?⑥쓬: general event streaming, structured JSON trace export, distributed trace correlation, user-code registry hooks, stable binary trace format, richer multi-instance timeline query??beta ?댄썑濡??좎??쒕떎.
- [~] Memory/concurrency model: `parallel`, task, channel, cancellation, visibility/happens-before 최소 계약을 문서화한다.
  - 진행: `docs/113_memory_concurrency_model.md` beta-stable happens-before, channel, cancellation, explicit out-of-beta memory model 범위?고정한다. `parallel` join visibility, shared `ref`/`ref` ?용, `ref`/`own` ?`own`/`own` task-boundary reject, copy-only non-blocking receive/cancel/close?stable contract?묶었??
  - 진행: `make memory-concurrency-model-test-smoke` `parallel-core-contract-test-smoke`? targeted C/LLVM backend compare(`parallel_channel_sum`, `parallel_channel_dual`, `triple_paradigm`)??행한다.
  - ?⑥쓬: full weak-memory ordering, user-selectable memory order, scheduler fairness guarantee, lock-free correctness, anonymous async closure capture/lifetime, cross-thread `Arc<T>` / `Send` / `Sync` trait system? beta ?댄썑濡??좎??쒕떎.
- [~] String/unicode policy: normalization, comparison, locale, escape handling, unsupported policy瑜?紐낆떆?쒕떎.
  - 진행: `docs/110_string_unicode_policy.md` UTF-8 string payload preservation, byte-length `StringLength`, byte-exact/normalization-blind equality/search?beta-stable?고정한다.
  - 진행: Unicode identifiers, normalization, locale-sensitive collation/case folding, grapheme iteration, display width, mixed-encoding source files??explicit out-of-beta?고정한다. `make unicode-policy-test-smoke` C/LLVM UTF-8 string execution?Unicode identifier reject?증한??
  - ?음: full Unicode text model???입?려?post-beta??scalar/grapheme/locale vocabulary? 별도 stdlib text module???계한다.

Checklist source of truth:

- `docs/100_beta_readiness_checklist.md`
- AIR source of truth: `docs/104_air_compiler_architecture.md`
- Drift gate: `make beta-readiness-checklist-test-smoke`
- AIR drift gate: `make air-drift-test-smoke`
- AIR backend non-impact gate: `make air-backend-nonimpact-test-smoke`
- AIR full backend non-impact hardening: `make air-backend-nonimpact-full-test-smoke`
- AIR strict backend execution parity: `make air-strict-backend-compare-test-smoke`

## 구조/?영 ?인 ?인??보드 (2026-04-20)

???션? 기능 backlog 아니며  ?제 ?업 ?율?베? ?뢰?? 계속 깎는 구조 debt / ?영 pain point?고정한다.

?곗꽑?쒖쐞 ?쒖븞:
- `P0`: function/action/intent body CFG + dataflow瑜?semantic source-of-truth濡??밴꺽
- `P1`: `.inc` 遺꾪븷???ㅼ젣 `.c`/`.h` 紐⑤뱢濡??꾪솚
- `P2`: hint namespace (`code` / `cause_ir` / `fix_source`)????트?기반으로 고정
- `P3`: type-category vocabulary?2-3층으??축
- `P4`: 빌드/?드박스/중간-stage JSON/artifact 문제?공식 경로 기?으로 ?리
- `P9`: arena ?턴??scratch/result lifetime 기?으로 명시 ?입
- `P9b`: repeated `Slot` / `SecureSlot` hot-loop access??Pin/Lease 문서 기?으로 분리한다. 기본 path????근 증이? fast path??scope-entry capability lease + automatic unpin cleanup되어??한다. Runtime ABI baseline? `PgyPinnedView` / `PergyraSlotPin` / `PergyraSlotUnpin` + `make test-security` ????작?고, plain token-bearing pin rejection, scope release while pinned, TTL cleanup skip while pinned, secure invalid-token/capability rejection, concurrent secure write rejection, release-after-unpin persistence??았?? Candidate source syntax `pin slot as view { ... }`??CFG cleanup/backend parity ?힐 ?까 parser explicit reject?봉인하고 `make diagnostics-json-test-smoke` JSON route?증한?? Pin/Lease semantic diagnostic vocabulary??`PGY_SEM_PIN_ESCAPE`, `PGY_SEM_PIN_PARALLEL_CONFLICT`, `PGY_SEM_PIN_AWAIT_BOUNDARY`, `PGY_SEM_PIN_QUBIT_REJECT`, `PGY_SEM_PIN_TOKEN_INVALID`?registry/docs??고정하고 `make diagnostic-registry-test-smoke`? `make beta-readiness-checklist-test-smoke` drift?막는?? Existing `ViewRead(...)` / `ViewWrite(...)` semantic surface now enforces `WriteView<T>` exclusive access for the same source slot while keeping shared `ReadView<T>` / `ReadView<T>` accepted. It also emits pin-specific diagnostics for return escape, await boundary, parallel boundary/acquisition, and QubitSlot rejection, and `make diagnostics-json-test-smoke` verifies their CLI JSON route. Generic ownership baseline? unresolved `TYPE_KIND_GENERIC`??`BORROW_TRACKED`?분류??generic `own/ref` 조용??copy-only??과?? 못하?막는?? ?? 것? stable source syntax, block-scoped CFG cleanup edge, secure-token source diagnostic, C/LLVM parity?? Source of truth: `docs/74_slot_pinning_caching.md`
- `P9c`: `Rc<T>` / `Weak<T>` 최소 subset? beta-stable??았?? 범위??single-thread `Int|Long|Float|Double|Bool|String` payload, explicit lifecycle builtin(`RcNew`, `RcClone`, `RcGet`, `RcDrop`, `RcDowngrade`, `WeakUpgrade`, `WeakDrop`), resolver metadata, semantic builtin typing, C runtime/emitter, LLVM runtime export/lowering, ABI layout smoke, C/LLVM lifecycle backend-compare?? 범위 ?payload??backend fallback??아니며 semantic explicit reject?? `Arc<T>`, cross-thread shared ownership, generic/object payload ?장, default ARC??beta 밖이?? Source of truth: `docs/100_beta_readiness_checklist.md`, `docs/106_ownership_model_comparison.md`, `src/runtime/pgy_abi_spec.h`
- `P10`: 모듈???파 고도의 compile/runtime ?도 ???별도 baseline으로 추적

### P0. Function CFG / Body Dataflow Closure

?먯젙: `BLOCKER`

?듭떖 ?뺣━:

- CFG 는 ?태??아니며  HIR??function CFG v0, predecessor/reachability, dominator/frontier, loop depth, phi candidate skeleton??진다.
- MIR??HIR CFG? RIR op?묶어 routine/block/instruction/cleanup block, SSA version map, def/use, cleanup/rollback/invalidation exceptional CFG, liveness/DCE vertical slice까? ?한다.
- ?? blocker????CFG/MIR infra?**개수 본문 ??론의 source-of-truth**??격는 것이?? 현재 body safety???????전??AST/helper traversal, local summary, backend fallback??기??되어 strict beta 기?으로 족하??

베? ?료 조건:

- [ ] Function/action/intent body마다 `BasicBlock`, `Edge`, `Terminator`, reachability, exceptional cleanup edge semantic pass?서 직접 ?비한다.
- [x] 반환이 는 routine? 모든 reachable normal path?서 return/value terminator?진다??all-path return ?? CFG body summary?고정한다.
- [~] definite assignment/use-before-init ?? CFG dataflow??동하고 branch/join/loop widening 진단??고정한다. stable local `let` ?면? parser `=` ?구? `PGY_SEM_UNINIT_LOCAL` backstop으로 봉인?고, wider delayed-assignment lattice??아직 ?려 한다.
- [~] move/use-after-move, borrow/ref lifetime, boundary escape?CFG join facts?계산한다. `QubitSlot` loop break/continue join regression, anchored `Slot<T>` branch/join release-state regression, `own subject` branch/join consumed-state regression, parallel subject transfer join/conflict regression, parallel `ref`+`own` boundary conflict regression, parallel `ref`+`ref` shared-read acceptance regression, direct named-call `spawn ref` ownership-boundary rejection regression, anonymous async spawn explicit reject regression? ?혔? closure/lambda/general longer-lived borrow lifetime? ?아 한다. `mut ref`/`ref mut` surface ?으?mutable-borrow overlap? beta-out-of-scope?봉인한다.
- [~] owned resource drop/cleanup insertion point?normal return, early return, break/continue, intent cancel/rollback/invalidation edge?서 같? 규칙으로 계산한다. `defer` cleanup terminator? resource-state snapshot/restore 격리, direct `type_check_statement()` fallback convergence, anchored slot branch/join state tracking? ?혔? full drop insertion/validation? ?아 한다.
- [ ] zone/effect/relation transition facts?path-sensitive summary??려 branch/join/handoff?서 stale state? conflict?같? vocabulary?진단한다.
- [~] `parallel`/channel/task boundary?서 moved value, borrowed reference, authority-bearing token, cancellation cleanup fact?CFG summary?증한?? parallel task-local terminator isolation, moved/released resource/boundary join, duplicate resource/boundary consume diagnostic, `ref`+`own` boundary conflict, blocking channel-send resource consume/join, direct named-call `spawn ref` ownership-boundary rejection, direct named-call `spawn Token<T>` authority-boundary rejection, anonymous async spawn explicit reject, `SendTimeout`/`TrySendStatus`/`SendTimeoutStatus` transport rejection, `TryRecv`/`RecvTimeout` movable receive explicit reject, authority `Token<T>` channel helper rejection, copy-only cancellation payload reject, copy-only channel close???혔? broader channel receive/backpressure summary, closure/lambda/general borrowed-reference task lifetime, cancellation cleanup fact???아 한다.
- [~] Interprocedural body summary?고정?다: `may_return`, `may_escape_ref`, `moves_param`, `borrows_param`, `drops_resource`, `effects`, `requires_zone`, `spawns_task`, `sends_channel`. 1?구조?function type??`body_summary_mask`? semantic recorder??되어갔다. direct function call? callee summary ?caller-relevant transitive facts??비하고 declaration-known `own/ref` parameter boundary facts??기록한다. method/host call??같? declaration-known summary facts?기록한다. lambda body summary??lambda function type??격리되어 outer routine으로 ?? ?고, function-typed lambda binding ?출? 같? callee-summary path??파한다. ?? 것? intent/helper call까? ?히?zone/effect/runtime propagation?C/LLVM lowering????summary bit?직접 ?비?게 만드???이??
- [ ] 진단? block/path provenance??함?다: source path, branch/join edge, previous state, Reason, Fix.
- [ ] MIR/C/LLVM lowering? 같? CFG/dataflow facts??비?고, frozen subset parity regression으로 묶는??

?행 ?서:

1. 현재 HIR/MIR CFG fact inventory? semantic ?비 을 으로 만든??
2. `--hir-cfg`, `--mir`, RIR flow-block dump?묶는 smoke?추???CFG fact drift?막는??
3. all-path return + reachability + definite assignment?CFG 기반으로 먼? ?격한다.
4. ownership move/borrow/drop cleanup??CFG dataflow濡??대룞?쒕떎.
5. zone/effect/relation transition, handoff frontier, projection freshness瑜?body CFG summary? runtime propagation scheduler???곌껐?쒕떎.
6. parallel/channel/task boundary summary瑜?異붽??섍퀬 C/LLVM backend compare??frozen cases瑜??ｋ뒗??

寃利?紐⑺몴:

- `make test-semantic`
- `make ir-pipeline-test-smoke`
- `make type-resolution-dag-test-smoke`
- `make cfg-body-dataflow-test-smoke`
- `make llvm-test-backend-compare`

Source of truth:

- `docs/103_cfg_body_dataflow_need.md`

### P1. `.inc` ?ㅽ뙆寃뚰떚瑜??ㅼ젣 紐⑤뱢濡??덈떒

- 문제:
  - 현재 `type_checker.c` ?transpiler/LLVM ?????모?화?? 아니며 ?파??분할???일 translation unit에 깝다
  - IDE jump/symbol lookup/forward decl ?서 리? 모두 ?동
  - formatter/linter/?? edit include ?서/?일 갱신 ??밍??민감?게 깨진??- ?향:
  - ????정 ??edit conflict / implicit declaration / include ordering failure 반복??  - ownership/generic/provenance 같? ?단 ?업??불필?하??려진다
- 기본 방침:
  - ?선 `semantic/type_checker_*`?서 ownership / generic / module-contract / diagnostics 축????제 `.c`/`.h` export 구조??단
  - declaration-side MIR-only hot path??helper family?`.c` 경계?분리
  - ?기 목표?? `docs/92_inc_split_roadmap.md`??Target State A-D?고정한다
  - stop condition: semantic?먮뒗 800 LOC 珥덇낵 `.inc` ?놁쓬, codegen/runtime?먮뒗 1,000 LOC 珥덇낵 `.inc` ?놁쓬, `type_checker.c`??orchestration-only, backend declaration path??dedicated inventory reader ?먮뒗 hard error留??덉슜
  - speed stop condition: `test-abi-perf`? `perf-summary` baseline?????고, 모듈??slice ??worst-case compile time??2??상 ???? ?보?기록
  - `.inc`??generated table / local macro table / private test fixture 같? ?한 ?도로만 ?긴??- ??업:
  - [ ] `type_checker`瑜?理쒖냼 5異뺤쑝濡??덈떒
    - [x] diagnostic emission/snapshot: `type_checker_diag.c`
    - [x] ownership classification: `type_checker_ownership_classify.c`
    - [x] channel transport validator: `type_checker_channel_transport.c`
    - [x] ownership diagnostics/consumers: `type_checker_ownership_diag.c`
    - [x] generic contract diagnostics: `type_checker_generic_diag.c`
    - [x] ability reference formatting seam: `type_checker_ability_ref.c`
    - [x] stdlib use validator seam: `type_checker_stdlib_use.c`
    - [x] module contract diagnostic seam: `type_checker_module_contract_diag.c`
    - [x] ability fields validator seam: `type_checker_ability_fields.c`
    - [x] ability matcher / subject ability lookup seam: `type_checker_ability_match.c`
    - [x] ability where-bound validator seam: `type_checker_ability_where.c`
    - generic consumer pipeline
    - [x] module contract / authority consumer: `type_checker_module_contract.c`
  - 진행: ownership 공용 enum/entrypoint?`type_checker_ownership_internal.h`?분리 ?작
  - 진행: ownership diagnostics forward declaration??`type_checker_ownership_diag_internal.h`?분리 ?작
  - 진행: ownership escape diagnostic renderer/helper family??`type_checker_ownership_diag.c`??제 TU 분리 ?료
  - 진행: ownership support helper(`semantic_assignment_target_path`, `semantic_borrowed_boundary_root_name`)??`type_checker_ownership_support_internal.h`?분리 ?작
  - 진행: ownership consumer seam(`return` / `assign` / `call`)??`type_checker_ownership_consumers_internal.h`?분리 ?작
  - 진행: `param_summary`??raw include block??아니며 `semantic_check_param_summary_escapes(...)` consumer helper??격
  - 진행: channel transport seam??`type_checker_channel_transport_internal.h`?분리 ?작
  - 진행: channel transport validator/reporters??`type_checker_channel_transport.c`??제 TU 분리 ?료
  - 진행: high-arity generic mismatch helper??`type_checker_generic_diag.c`??제 TU 분리 ?료
  - 진행: module contract consumer ?행 seam??ability reference display/name/signature helper??`type_checker_ability_ref.c`??제 TU 분리 ?료
  - 진행: stdlib use validator??`type_checker_stdlib_use.c`??제 TU 분리 ?료
  - 진행: subject ability mismatch diagnostic? `type_checker_module_contract_diag.c`??제 TU 분리 ?료
  - 진행: ability `fields` validator??`type_checker_ability_fields.c`??제 TU 분리 ?료
  - 진행: `find_type_decl_by_name`??include-order static helper?서 `type_checker_internal.h` internal API??격
  - 진행: ability ref matching / role ability lookup / subject ability lookup? `type_checker_ability_match.c`??제 TU 분리 ?료
  - 진행: `find_ability_decl_by_name` / `collect_effective_generic_arg_nodes`??include-order static helper?서 `type_checker_internal.h` internal API??격
  - 진행: ability where-bound consumer validation? `type_checker_ability_where.c`??제 TU 분리 ?료
  - 진행: `format_type_constraint_bounds`??include-order static helper?서 `type_checker_internal.h` internal API??격 ??별도 TU?분리
  - 진행: `semantic_type_resolution_record_type_ref_dependency`??graph core TU??동??include-order static helper ?존???거
  - 진행: `semantic_type_resolution_collect_type_refs`??`type_checker_resolution_graph_collect.c`??동??DAG inventory collector????제 TU seam??만들한다
  - 진행: generic contract inventory / string dependency / required ability collector helpers??`type_checker_resolution_graph_collect.c`??동
  - 진행: top-level declaration graph registration??`type_checker_resolution_graph_collect.c`??동??inventory pass??bootstrap helper debt???줄???  - 진행: local-contract graph node/dependency + zone/world/projection label formatters??`type_checker_resolution_graph_labels.c`??동??graph inventory `.inc`?1,835 LOC까? 축소한다
  - 진행: projection source resolver??`type_checker_resolution_graph_domain.c`??동하고 `find_zone_domain_slot`??internal API??격??graph/domain split ?행 seam??만들한다
  - 진행: event declaration precollector??`type_checker_resolution_graph_decl.c`??동??declaration-kind collector 분리???작
  - 진행: enum declaration precollector??`type_checker_resolution_graph_decl.c`??동하고 `semantic_stage_method_array`?internal API??격??inventory `.inc`?1,765 LOC까? 축소
  - 진행: ability declaration precollector? action-contract precollector??`type_checker_resolution_graph_decl.c`??동??inventory `.inc`?1,648 LOC까? 축소
  - 진행: role/class/party/roster declaration precollector??`type_checker_resolution_graph_decl.c`??동?고, relation/effect domain inventory precollector??`type_checker_resolution_graph_domain.c`??동??inventory `.inc`?1,299 LOC까? 축소
  - 진행: intent declaration precollector??`type_checker_resolution_graph_decl.c`? world inventory precollector??`type_checker_resolution_graph_world.c`??동??inventory `.inc`?870 LOC까? 축소
  - 진행: zone refresh projection field-map DAG collector??`type_checker_resolution_graph_zone.c`??동?고, graph inventory body??`type_checker_resolution_graph_inventory.c`??격한다. `type_checker_resolution_graph_inventory.inc`???거되어 DAG inventory include-order debt ?혔??  - 진행: projection builtin target-field resolver??recursive fallback ???DAG metadata lookup-only seam으로 ???? projection source/target mismatch 진단? projection validator ?유?고, target field type materialization? DAG metadata ?유한다. fallback seam cap? 31?서 30으로 ?려갔다. ?후 type graph precollect?top-level symbol pass 에 배치하고 `program_resolve_type_quiet(...)`?metadata lookup-only??? event/function placeholder recursive fallback 이 DAG metadata??게 한다. fallback seam cap? 30?서 29??려갔다. domain query projection source-field resolver??class/vessel field DAG metadata lookup-only??? cap? 28??려갔다. party/roster shared-field resolver??declaration metadata lookup-only??? cap? 26으로 ?려갔다. ability abstract method signature resolver? role host-type resolver??lookup-only??? cap? 24??려갔다. function/action body expression/lambda/event handler precollect ?장 ??event/lambda handler resolver??lookup-only??? cap? 23으로 ?려갔다. body flow resolver??graph metadata lookup-only??? cap? 22??려갔다. type-alias statement resolver??DAG metadata lookup-only??? cap? 21??려갔다. `world_decl` lookup-only ?환? subject/zone nominal materialization??아직 족해 semantic 77??패?만들?으?보류한다
  - 진행: world/zone local-contract stage replay??`type_checker_resolution_stage_domain.c`??동?고, top-level DAG stage replay??`type_checker_resolution_stage.c`??격??`type_checker_resolution_stage.inc`??거
  - 진행: `type_checker_ability_decl.c`, `type_checker_zone_decl.c`, `type_checker_world_decl.c`??standalone TU?빌드?며 hidden include-order helper ?존??internal/header 계약으로 ?격
  - 진행: `type_checker_intent_decl.c` standalone TU ?격 ??러??implicit helper dependency?internal/header 계약으로 ?격?고, `-Werror=implicit-function-declaration -Werror=implicit-int`?기본 CFLAGS?고정??같? 종류??C 모듈??버그?빌드 ?계?서 차단
  - 진행: `type_checker_role_decl.c`, `type_checker_party_decl.c`, `type_checker_roster_decl.c`??hard implicit-declaration CFLAGS ?래?서 빌드?도?helper/header ?존??명시
  - 진행: `type_checker_class_decl.c` class/extern declaration checking???유?고, `type_checker_program.c` top-level semantic orchestration???유한다. ??graph/worklist/effect/stats helper?internal API??격??`type_checker_program.inc`?624 LOC까? 축소
  - 진행: `type_checker_builtins_projection.c` `ToObject` / `ToTObject` semantic projection checker??유?고, `type_checker_builtins_nominal.inc`?659 LOC까? 축소
  - 진행: expression operator/indexed-access checker?`type_checker_expr_ops.c`?분리?고, static member path / consumed-boundary helper?`type_checker_expr_names.c`??동한다. `type_checker_expr.inc`??758 LOC, `type_checker_helpers_late.inc`??773 LOC 되어 ????semantic 800 LOC stop condition ?래??려갔다
  - 진행: event declaration/subscription/invoke semantic? `type_checker_event.c`??격?고, QubitSlot compile-time state / entangle pool / movable-resource-use validation? `type_checker_qubit.c`??격한다. `type_checker.c`??481 LOC??려 600 LOC ?하 stop condition??만족한다
  - 진행: domain slot/projection/overlay helper body?`type_checker_decls_domain_helpers.c`??격?고, intent inheritance/derivation helper body?`type_checker_intent_helpers.c`??격한다. `type_checker_decls_domain_helpers.inc`???거하고 `type_checker_decls_a.inc`??1-line forwarding stub으로 축소
  - ?료: semantic `.inc` 800 LOC stop condition? `make semantic-inc-size-test-smoke`?고정. 현재 `src/semantic`는 800 LOC 초과 `.inc` 한다
  - ?료: semantic core shape stop condition? `make semantic-core-shape-test-smoke`?고정. `type_checker.c <= 600 LOC`, event/qubit owner TU, DAG inventory `.c` ownership??CI?서 ?한??  - 진행: C backend MIR inventory/SSA emitter include?5-line shim + `transpiler_emitters_mir_inventory_intent.inc` / `transpiler_emitters_mir_inventory_ssa_names.inc` / `transpiler_emitters_mir_inventory_ssa_emit.inc`?분리???당 debt?모두 1,000 LOC ?래?????  - 진행: C backend `emit_program(...)` bootstrap? direct declaration-array reads ???`transpiler_active_inventory(...)` / `transpiler_active_externs(...)` view??비한다. `make mir-declaration-inventory-test-smoke` C/LLVM declaration-side codegen??raw declaration inventory access?helper owner??한한다
  - 진행: C backend expression emitter include?7-line shim + `transpiler_expr_emitters_builtins.inc` / `transpiler_expr_emitters_call_a.inc` / `transpiler_expr_emitters_call_b.inc` / `transpiler_expr_emitters_members.inc` / `transpiler_expr_emitters_tail.inc`?분리???당 debt?모두 1,000 LOC ?래????? ? `make test-transpile -j2`, `make llvm-test-backend-compare -j2`
  - 진행: LLVM call emitter include?17-line shim + `llvm_expr_call_constructors.inc` / `llvm_expr_call_arrays.inc` / `llvm_expr_call_collections_base.inc` / `llvm_expr_call_domain_queries.inc` / `llvm_expr_call_events.inc` / `llvm_expr_call_intent_observability.inc` / `llvm_expr_call_log.inc` / `llvm_expr_call_math.inc` / `llvm_expr_call_result_option.inc` / `llvm_expr_call_slots.inc` / `llvm_expr_call_task_channel.inc` / `llvm_expr_calls_part_a.inc` / `llvm_expr_calls_part_b.inc` / `llvm_expr_calls_part_c.inc` / `llvm_expr_calls_part_d.inc`?분리???당 debt?모두 1,000 LOC ?래????? enum/class constructor, array builtin, `ListNew`/`Set*` base collection, domain query builtin, event invocation, intent observability, log, scalar math, Result/Option, slot/device-slot builtin, task/channel lowering? `llvm_emit_call`?서 분리되어 별도 owner include 한다. ? `make test-transpile -j2`, `make backend-inc-size-test-smoke`, `make llvm-test-backend-compare -j2`
  - 진행: C backend base emitter B include?6-line shim + `transpiler_emitters_base_b_part_a.inc` / `transpiler_emitters_base_b_part_b.inc` / `transpiler_emitters_base_b_part_c.inc` / `transpiler_emitters_base_b_part_d.inc`?분리???당 debt?모두 1,000 LOC ?래????? ? `make test-transpile -j2`, `make llvm-test-backend-compare -j2`
  - ?료: Tier 1 runtime/codegen/compiler `.inc > 1000 LOC` gate???힘. `pgy_runtime_part_ba.inc`, `pgy_runtime_lib_part_b.inc`, `transpiler_emitters_base_a.inc`, `transpiler_helpers_core_a.inc`, `transpiler_helpers_core_b.inc`, `transpiler_domain_role.inc`, `llvm_expr_helpers.inc`, `mir_public.inc`, `llvm_expr_call_methods.inc`, `llvm_domain_helpers.inc`?모두 safe mechanical split으로 1,000 LOC ?래?????  - ?료: `tests/backend_inc_size_smoke.sh` / `make backend-inc-size-test-smoke` 추?. `src/runtime`, `src/codegen`, `src/compiler`??`.inc <= 1000 LOC`?CI?서 고정
  - 寃利? `make backend-inc-size-test-smoke`, `make test-mir test-transpile test-abi -j2`, `make llvm-test-backend-compare -j2`
  - 진행: `type_checker_helpers_late.c` standalone TU 빌드 ??러??call-path helper include-order ?존??`type_checker_internal.h` prototype?직접 include 계약으로 고정한다
  - 진행: `type_checker_decls_a.inc -> type_checker_decls_domain_helpers.inc`, `type_checker_decls_intent.inc -> type_checker_world_decl.c`, `type_checker_helpers_effects.inc -> type_checker_helpers_host.inc` 이 dangling return-type seams ?거
  - 진행: `type_checker_resolution_graph_core.inc` ??inventory include 경계??dangling `static void` seam 2개? 명시 return type으로 ?리
  - 진행: `generic_params_required_count`??include-order static helper?서 `type_checker_internal.h` internal API??격
  - ?료: required ability resolver? action required-ability validator??`type_checker_module_contract.c`??제 TU 분리 ?료
  - ?료: `type_checker_module_contracts.inc` ?거. module contract include-order 구조 debt???힘
  - [ ] `.inc` ?? static helper ?교차 참조 ?한 ?볼 목록 ?성
  - [x] include-order???존는 implicit declaration 경로 ?거?빌드 계약으로 ?격 (`-Werror=implicit-function-declaration`, `-Werror=implicit-int`)
  - [~] declaration-side MIR-only debt??helper-gated state까? ?혔?? ?? ?계??`MIRProgram` ??AST-shaped declaration inventory?dedicated declaration metadata model?분리는 ?이??
  - 진행: ownership return / assignment rebind / array literal store / boundary validation / call argument / destructuring / let-binding / parameter escape-summary consumers??`.inc`?서 ?제 TU??격한다. ?????일: `type_checker_ownership_return.inc`, `type_checker_ownership_assign.inc`, `type_checker_ownership_array_store.inc`, `type_checker_ownership_boundaries.inc`, `type_checker_ownership_call.inc`, `type_checker_ownership_destructure.inc`, `type_checker_ownership_destructure_stmt.inc`, `type_checker_ownership_let.inc`, `type_checker_ownership_let_boundary.inc`, `type_checker_ownership_let_claim.inc`, `type_checker_ownership_let_infer.inc`, `type_checker_ownership_let_slot.inc`, `type_checker_ownership_let_value.inc`, `type_checker_ownership_param_summary.inc`. 현재 `src/semantic/type_checker_ownership_*.inc`??0개다
  - ?칙 강화: 베? 기??서??behavior-owning `.inc`?beta+1 ?리 아니며 blocker?본다. generated table / local macro table / private test fixture ??`.inc`??owner `.c` 는 명시??generated artifact?????  - ?칙 강화: `.inc` ?거 과정?서 ?러 behavior family??나??mega-TU??치 ?는?? `make semantic-tu-size-test-smoke` ??semantic owner TU??1,000 LOC ?하??한?고, 기존 초???TU??개별 cap으로 ??커? 못하?막는??  - ?? ?험 seam: `type_checker_builtins_query.h`??`type_checker_builtins_slotops.h`? `BuiltinKind builtin_resolve(...)` ?그?처 include-chain으로 붙어 한다. query/slot/nominal builtin? dispatcher contract?먼? 분리????TU??린??
### P10. ?도 / 빌드 ?능 baseline

- 문제:
  - ?기 모듈?? translation unit ?? ?리?incremental build??좋아??????full build/link 는 generated backend compile ?간??????한다
  - 현재 `test-abi-perf`??존재???raw log 길어 worst-case 추적???렵??- 기본 방침:
  - `make test-abi-perf`濡?benchmark-only ABI/runtime baseline??罹≪쿂?쒕떎
  - `make perf-summary PERF_LOG=<log>`?C/LLVM compile/run ?균?worst-case??약한다
  - representative case??`tests/bench_backend.sh <source.pgy> dev`?C/LLVM wall time + RSS?직접 ?인한다
  - generated/native compile warning? ?도 noise 아니며 build hygiene bug?보고 즉시 ?는??- 현재 baseline (2026-04-24, local WSL):
  - `make test-abi-perf`: 320 passed, 0 failed
  - `perf-summary`: C 32 cases, avg compile 0.569s, max 1.783s (`intent_authority_snapshot_abi`), avg run 0.001s
  - `perf-summary`: LLVM 32 cases, avg compile 0.187s, max 0.251s (`projection_abi`), avg run 0.002s
- 진행: `make perf-contract-test-smoke` synthetic `test-abi-perf` log?대해 `perf_summary` log grammar, C/LLVM case count, average compile/run, worst-case compile/run case selection??CI?서 고정한다. ??gate??baseline ?자 ?체?고정?? ?고, perf evidence machine-readable ?태???는 ?한??
  - representative `relation_effect_propagation/main.pgy`: C dev 1.03s / 46MB, LLVM dev 0.72s / 60MB after `realpath` warning fix
- 진행:
  - [x] `tests/perf_summary.sh` 異붽?
  - [x] `make perf-summary PERF_LOG=<log>` 異붽?
  - [x] generated C/LLVM compile path??POSIX `realpath` implicit declaration 寃쎄퀬 ?쒓굅
- ?⑥쓬:
  - [ ] CI?서 benchmark-only ?치?artifact???할 결정
  - [ ] release/beta notes??perf-summary baseline 泥⑤?
  - [ ] worst-case compile 2??상 증? ??regression ?보??동 ?시

### P2. hint namespace ???트리화

- 문제:
  - `cause_ir` / `fix_source` literal???션 ?위?계속 되어?는??중앙 ???트리? 한다
  - `docs/72`?문서??`code` ?주? `cause_ir` / `fix_source` variant drift?강제?? 못한??- ?향:
  - downstream??diagnostic routing????값을 ?기 ?작?면 ??/drift 즉시 breaking change 한다
- 기본 방침:
  - `code`, `cause_ir`, `fix_source`?모두 registry/enum-like literal set으로 ?  - 문서? 코드 리뷰 기??서 ?새 literal 추? ??registry + docs ?시 갱신을 강제
- ??업:
  - [x] diagnostic literal registry 珥덉븞 異붽?
    - ?료: `src/semantic/diag_codes.h` `PGY_CODE_*`, `PGY_CAUSE_*`, `PGY_FIX_*` registry source of truth??작하고 `docs/72_diagnostic_codes.md` ?? 문서??  - [x] `cause_ir` / `fix_source` ?이?규칙 문서??    - ?료: `docs/72_diagnostic_codes.md`??`cause_ir` stage/subsystem/condition 규칙?`fix_source` source-action token 규칙 고정
  - [x] free-form 문자???규 추? 에 smoke gate 마련
    - ?료: `tests/diagnostic_registry_smoke.sh` / `make diagnostic-registry-test-smoke` semantic diagnostic call-site??`PGY_CODE_*`, `PGY_CAUSE_*`, `PGY_FIX_*` macro ?용?diagnostic code 문서 sync???
### P3. ???ownership 되어 ?축

- 문제:
  - anchored handle / movable resource / subject / subject-host / boundary value / capability-bearing / move token ??되어 과다
  - 같? semantic family 메시마다 ?른 ?름으로 ?출한다
- ?곹뼢:
  - ?용?도 ?갈리고, 구현?도 메시/문서/?스???렬 ??drift 한다
- 기본 방침:
  - ?용??facing ?심 되어?2-3층으??축
  - ?? 분류???X???위분류?로??출
- ??업:
  - [ ] user-facing canonical vocabulary ?뺣━
  - [ ] diagnostics/README/docs ?⑹뼱 留ㅽ븨???묒꽦
  - [ ] old wording grep inventory ??치환 계획 ?립

### P4. 빌드/?드박스 경로 ?순??
- 문제:
  - bash / PowerShell / cmd / MSYS2 / stale object / path rewrite / sed 기반 stamp 으로 ?른 방식으로 깨진??  - ?Nothing to be done??+ stale artifact 같? ?? ?산을 ?게 깎는??  - smoke test repo root??runtime artifact??기?dirty worktree? ?제 ?스 경을 구분?기 ?려?진??- 기본 방침:
  - ?일 공식 빌드 경로??하??머??document-only 는 best-effort??린??  - stale artifact ?피?대해 강제 ?빌??경로?공식??- ??업:
  - [x] 공식 Windows 빌드 경로 1개로 문서??    - 기?: GitHub Actions `windows-latest` + `msys2/setup-msys2` native MinGW/MSYS2 runtime
    - plain Linux-hosted `gcc`??`ci-windows` acceptance line???님
  - [x] `llvm_smoke.sh`??`string_io` smoke repo root??`io.txt`??기 ?도??case?source directory?서 ?행?게 ?렬
  - [x] LLVM runtime object freshness split runtime `.inc` subpart 경을 보도?`compiler_runtime_cache_is_fresh(...)` dependency list??장. `pgy_runtime_lib_part_b_part_d.inc` 같? ?위 include ?정 ??stale runtime object 링크는 문제?차단
  - [ ] `clean && build` 강제 wrapper / recommended entrypoint ?의
  - [ ] stale `.o` / `.d` 진단 ?드? 강제 ?빌???션 ?리

### P5. printf-style 진단 ?맷??축소

- 문제:
  - ?? semantic diagnostic helper???자 개수 매우 많고, placeholder drift??취약한다
  - 현재 구조??`fmt ?드코딩 + structured tags(code/cause/fix)` ?중으로 공존한다
- 기본 방침:
  - 진단 payload?struct?모으? human-readable render??renderer/helper layer ?당
  - 최소??고인??helper??payload-builder ?턴으로 ?환
- ??업:
  - [ ] high-arity diagnostic helper inventory ?묒꽦
  - [ ] generic mismatch / authority mismatch / ownership escape?서 payload struct ?범 ?입

### P6. channel transport 규칙 공통 validator ?렴

- 문제:
  - `type_checker_async_channel.h`? builtin/send-query 계열??ownership/channel transport 규칙??중복 구현한다
- 기본 방침:
  - channel transport??공통 validator ?나??렴
  - builtin/send wrappers??surface adapter留??대떦
- ??업:
  - [x] send/try-send/send-timeout/status variants 공통 validator 추출
  - [ ] subject / movable / anchored / boundary mismatch wording ?듭씪
  - 진행: named-transfer requirement? subject/boundary/anchored borrowed-send/mismatch??`semantic_channel_transfer_requires_named_binding(...)`, `semantic_report_named_channel_transfer_required(...)`, `semantic_validate_channel_transport_ownership(...)` helper?1??렴
  - 진행: token / move-only send-recv restriction wording??`semantic_report_channel_transport_policy(...)` helper??렬 ?작
  - 진행: validator/reporting 구현? `type_checker_async_channel.h`?서 ?거하고 `type_checker_channel_transport.c` source of truth 한다

### P7. 중간 stage JSON routing closure

- 문제:
  - HIR/DIR/RIR/MIR ?패 경로 ?? ?전??plain text 중심?라 `?일 JSON 배열` 계약??깨뜨린다
- 기본 방침:
  - frontend/backend ?단?아니며 중간 stage ?패??structured output 계약??되어?게 한다
- ??업:
  - [ ] HIR/DIR/RIR/MIR failure emitter inventory ?묒꽦
  - [ ] plain-text fallback ?쒓굅 ?곗꽑?쒖쐞 ?섎┰

### P8. stale binary / artifact ?? 고정

- 문제:
  - stale object/dependency ?일 ?문???스 ?정??반영?? 는 경우 한다
- 기본 방침:
  - ?빠?증분 빌드?보???신??한 ?빌??경로??선
- ??업:
  - [ ] stale artifact ?현 조건 문서??  - [ ] 권장 빌드 진입?에??clean rebuild ?택?기본 ?출

### P9. arena ?턴 명시 ?입

- 문제:
  - transpiler / semantic / diagnostics / type rendering 경로???시 문자??버퍼 churn??많다
  - `malloc/free`? context-lifetime scratch allocation???여 되어, early-return/fail path?서 ?유권이 ?발?이??  - cache? ?시 문자이 ?이?dangling 는 과도??copy churn ?험??커진??- 기본 방침:
  - arena??명시?으??입한다
  - ?? ?면 치환??아니며 `scratch arena`? `result arena`?증명 기?으로 분리한다
  - cache / long-lived metadata / AST-owned field는 arena-owned ?인?? ??하 ?는??  - arena ?교차 참조??raw pointer보다 `index` / stable handle 참조?기본으로 한다
  - arena??최소??`transpiler`, `semantic scratch`, `semantic result`, ?요 ??`type/render scratch`처럼 ??/?명별로 분리한다
  - ??????arena 분리???누 free?느?보???언??reset?느?? 기?으로 ?계한다
  - 泥??④퀎??transpiler / semantic diagnostics / type render helper??scratch allocation ?섎졃?대떎
- ??寃곗젙??留욌뒗 ?댁쑀:
  - 현재 코드베이는 long-lived cache? short-lived formatting string??강하??여 되어, raw pointer 공유보다 index 참조 ?씬 ?전한다
  - Pergyra??early-return/fail path? pass-local scratch data 많아?? ?일 arena보다 ??/?명?arena 분리 ?버깅과 reset 비용 면에??한다
  - ? `Arena + Index 참조 + ??별 arena 분리` ?구조 debt?줄이????보수?이??정?인 방향한다
- ??업:
  - [x] `scratch arena` / `result arena` lifetime 규칙 문서??  - [x] arena ?cross-reference?`index` / stable handle 기?으로 문서??  - [x] `TranspilerCtx` scratch arena ?용 범위 ?정
  - [x] semantic analyze pass??scratch arena ?입 ???리
  - [x] diagnostic payload/result-owned arena 분리 ?? 결정
  - [x] ?????븷蹂?arena 遺꾪븷??珥덉븞 ?묒꽦
  - [x] `strdup_fmt` / type render / projection path / generic formatter helper??arena ?꾪솚 ?곗꽑?쒖쐞 ?묒꽦
  - [x] cache??arena-owned ?인?????금? 규칙 문서??  - [x] ?vertical slice:
    - transpiler temporary strings
    - semantic diagnostic formatting scratch strings
    - type-name rendering scratch helpers
  - 진행: `docs/94_arena_index_lifetime_plan.md`?방향 고정
  - 진행: `TranspilerCtx`??`arena`?scratch arena?명시
  - 진행: transpiler scratch-only temporary 1?vertical slice ?료
    - zone authority temporary expression
    - intent priority default literal
    - projection refresh `source_expr`
    - event declaration `event_type`
  - 진행: semantic diagnostics result seam 1??입
    - `Diagnostic` optional payload snapshot??보존
    - payload emit 寃쎈줈??result-owned snapshot?쇰줈 蹂듭궗
    - semantic JSON 출력??payload ?드??께 ?출 ??  - 진행: semantic scratch arena 1??입
    - `SemanticContext`??scratch arena 異붽?
    - ownership diagnostic path string? scratch arena瑜??곗꽑 ?ъ슜
    - payload snapshot??result濡?蹂듭궗?섎?濡?helper ?대? free churn ?쒓굅
  - 진행: LLVM arena lane 1?closure
    - `LLVMGenCtx`??`scratch` + `persistent` lane으로 분리
    - `LLVMGenResult`??result-owned arena瑜?蹂댁쑀
    - intent MIR collector / projection path / local grow helper / event invoke / type render helper scratch??렴
    - synthetic event-handler AST field ??μ? callable signature registry濡?移섑솚
    - `*error_message` heap return contract??result-owned lane?쇰줈 ?섎졃
    - ?? heap 경계??owner shell(`ctx`, registry destroy, result outer shell)?runtime ABI contract ??으로 축소
    - 진행: intent observability(`last/history/active/recent`)? authority failure snapshot??stable runtime string exports??`runtime-borrowed string` ABI?고정한다. caller??free?? 하고 ?음 runtime registry/snapshot mutation ?까??효한다
    - 진행: `runtime-abi-lifetime-test-smoke` stable intent last/history/active/recent ?authority 문자??export body?서 allocation/free/strdup??발생?? ?도??한??    - 진행: stable string helper returns??`result-owned string`, stable string-array helper returns??`result-owned array` ABI?고정한다. `runtime-abi-lifetime-test-smoke` helper payload borrowed input pointer, stack buffer, string literal??반환?? 하고 allocation/copy??payload?반환는 ?한??    - 진행: stable file descriptor??`runtime-owned handle` ABI?고정한다. `pgy_file_open`? ?힌 runtime table slot???사?하? `pgy_file_close`??table entry?NULL?비워 ?사?????태?만든?? `runtime-abi-lifetime-test-smoke` ??release/reuse contract??한??    - ?음: file descriptor ??runtime-owned handle ownership??같? ????smoke/문서 계약으로 ?장?야 한다
  - 주의: 반환 계약??는 expression string? 아직 arena??? ?음
  - 주의: `slot_ref_expr(...)` scratch ?환 ?도???돌? 반환 ownership 경계?먼? ?눠????
### 최근 closure 진행 (2026-04-18)

- declaration-side MIR-only host context瑜????뺣━
  - transpiler host context `current_host_decl -> within_zone -> saved host-name inventory` ?으?복원?도??렬
  - class/zone/relation/effect/world field query helper raw host-name state보다 inventory-backed host handle???선 ?용
  - direct `current_*_name` restore chain ???`transpiler_restore_host_context_local(...)` helper?되어 ?발??context 복구 코드?축소
  - emitter hot path??direct `current_*_name` 참조????걷어?고, ?? ?용처? helper/restore layer????  - LLVM declaration helper??current host lookup??공용 active-inventory host helper?되어 naming chain??축소
  - LLVM MIR/domain emission??direct `current_class_name` save/restore??host-name bind/restore helper?되어 state ?중복??줄임
  - LLVM expr/stmt hot path??`llvm_current_host_decl_name(...)` 기?으로 ?렬??direct raw host-name read???줄임
  - `HasProjection/HasLayer/HasState/HasZone*` ?method/field helper raw `current_class_name` ???host helper??과?도??리
  - LLVM pipeline??nominal registration / class method emission??raw nominal AST array보다 `mir->decl_headers`?직접 ?회?도??렬
  - LLVM domain pass??raw `ctx->mir->{relations,effects,zones,...}` 직접 ?근 ???`llvm_active_domain_inventory(...)` helper??과?도??렬
  - ? declaration-side debt???제 emitter 본문보다 inventory bootstrap + helper/restore layer ? 으로 ???축??  - C transpiler domain/hosted method emission??`emit_hosted_methods_from_mir_or_error_local(...)` helper??렴
  - party / roster / relation / effect / zone / world method emit??같? MIR routine gate? 같? explicit backend error ?책???용
  - relation/effect/zone/world method??dead AST signature fallback ?쒓굅
  - party / roster / relation / effect / zone / world declaration emit entrypoint??inventory decl???곗꽑 ?ъ슜
  - bootstrap residual? ?제 per-domain AST array 직접 ?회보다 inventory-backed bootstrap helper 본체 쪽으????축
- generic contract + type-resolution DAG ?뚭?瑜????볧옒
  - `role impl ability` 경로 generic default/where-bound cycle provenance regression??추???  - ? action/intent-step/zone-authority/party-role-slot??대해 role impl consumer??staged DAG path ?? 범위???함
- 현재 증선
  - `test-semantic`: `1617 passed, 0 failed`
  - `test-transpile`: `670 passed, 0 failed`
  - `test-abi`: `84 passed, 0 failed`
  - `ci-linux`: full green ?좎?
  - LLVM expr/stmt host-helper ?리 ?후?도 `test-transpile`, `test-abi` ?통??인

### 최근 closure 진행 (2026-04-24)

- runtime propagation/provenance 1李?closure
  - C/LLVM domain hidden cell??`ready/dirty` bool????태?서 `epoch/cause` provenance cell까? 같? schema??장??  - relation/effect/zone/world projection, layer, state, world-derived state recompute ?점??cause-stamped provenance??기?록 C/LLVM???렬??  - LLVM domain struct layout??그동??빠뜨리고 ?던 `__projection_dirty_*` field?relation/effect/zone???시 ?함?도?parity ?정
  - LLVM projection sync??C? 같? dirty-gated recompute 경로??렬??  - LLVM host-field assignment zone/relation/effect host method ?에??projection invalidation??만들?록 복구
  - LLVM intent step rebound-zone 寃쎈줈??effective zone projection cell??蹂댁닔?곸쑝濡?dirty-mark + sync ?섎룄濡?蹂닿컯
  - 寃곌낵: `relation_effect_propagation_abi`, `intent_zone_binding`, `intent_cross_world_transfer`, `intent_rich_history_identity` backend compare drift ?쒓굅
  - ????: transpile domain async/world tests provenance hidden field? stamp write까? 직접 ?인
  - ??진행: `world` derived-state recompute C/LLVM ?쪽?서 bounded pass loop??록 ?라?고, single-pass declaration-order replay?만 ?존?? ?게 ??  - ??진행: bounded recompute pass-limit overflow??C??`PGY_PANIC`?LLVM??`abort()` 경로?hard-fail?도?고정??- ????: transpile world-derived chain test + `world_fixpoint_abi` smoke C/LLVM ?쪽?서 ?색
- 현재 ?석: runtime propagation provenance baseline(`dirty/ready + epoch/cause`)? ?제 beta 계약?????간주하고 ?시 ?화?키 ?음
- 추? closure: zone lifecycle sync???제 C/LLVM ?쪽?서 bounded frontier loop?? state/layer replay single-batch?만 묶이 ?는??- 추? closure: embedded world-zone source assignment???제 projection dirty mark 에 같? turn??zone sync??워 stale `ready/value` drift 이 projection recompute??는??- 추? ??: `world_embedded_projection_abi`, `world_embedded_method_projection_abi`, `world_embedded_branch_projection_abi` C/LLVM ABI smoke?서 ?색?며 embedded zone projection read-after-mutate path?straight-line assignment, method-call, branch-join slice까? ?근??- 추? ??: `handoff_projection_frontier_abi` C/LLVM ABI smoke?서 ?색하고 `handoff_projection_frontier` backend-compare?서 ?색한다. v1 handoff materialization ?후 source projection? source snapshot?? target projection? target mutation 결과?보도??근??- 추? ??: `handoff_world_state_frontier_abi`? `handoff_world_state_frontier` C/LLVM?서 ?색한다. active world-owned zone??`transfer:` ??으??긴 ??projection-backed world state? `all` composed state 같? tick?서 fresh?게 보이??최소 frontier??근??- 추? ??: `handoff_layer_state_frontier_abi`? `handoff_layer_state_frontier` C/LLVM?서 ?색한다. `transfer:` ?후 action-caused effect target zone layer/state? active world-derived layer/state alias까? 같? tick?서 fresh?게 ?파는 경로??근??- 추? ??: `world_embedded_action_frontier_abi`? `world_embedded_action_frontier` C/LLVM?서 ?색한다. embedded world-zone subject action call??action-caused effect layer/state? active world-derived layer/state alias까? 같? tick?서 fresh?게 ?파는 경로??근??- 추? ??: `world_embedded_action_pool_frontier_abi`? `world_embedded_action_pool_frontier` C/LLVM?서 ?색한다. embedded world-zone subject action call??fixed-capacity effect pool 경로??같? frontier 계약으로 ?근??- 강한 ?? 과제: full bounded fixpoint / transitive frontier scheduler??**명시??beta blocker**???. ?만 ?? debt??zone/world frontier loop???? 아니며 remaining authority/failure handoff family? ???? world-zone propagation family?같? source-of-truth??반?하???이??- 추? closure: relation/effect/zone projection sync??bounded transitive recompute loop??라하고 declaration order??기? ?는??- 추? ??: `projection_chain_abi` C/LLVM ABI smoke, `make test-all`, `make llvm-test-backend-compare`?서 ?겼??- 추? gate: `make runtime-frontier-contract-test-smoke` C emitter? LLVM emitter?서 world derived-state bounded recompute, zone lifecycle bounded frontier loop, projection-chain bounded recompute, embedded world-zone action-caused layer/state freshness, pass-limit overflow hard-fail, ABI smoke ?록, backend-compare ?록???한?? ??gate??full bounded fixpoint / transitive frontier scheduler ?시 single-pass 구현으로 ?퇴?? 못하?막는 beta blocker gate?? ?? runtime propagation closure??remaining authority/failure handoff family? broader world-zone propagation family?같? source-of-truth frontier policy??반?하???이??- Beta readiness audit: `docs/98_beta_closure_readiness_report.md` records the current codebase verdict, remaining blockers, and concrete closure order. It narrows the next highest-value implementation target to handoff propagation and broader world-zone scheduler generalization.

### 최근 closure 진행 (2026-04-23)

- AST ????스?치 partition 규칙 공식????`docs/95_ast_dispatch_partition.md`
  - ?체 AST ???(현재 93? ??4 카테고리 (type annotation / decl sub-metadata / top-level decl / root) disjoint 분할
  - ?카테고리별로 "???정 switch ?서 ?달 불???" ??**?서 invariant 근거** ?문서??  - case label 추?/금?/safety-net 결정 기? ?정
  - ??AST ???추? ??체크리스???함
  - `llvm_stmt.c` ??top-level decl skip 리스??+ Zone/World forward  ??문서 기?으로 ?렬??(`AST_INTENT_DECL` skip ?락 ?정, Zone/World 11?forward 주석 ?확?? `llvm_expr.c` explicit diagnostic ??)
  - ??AST ???異붽? ??docs/95 ?낅뜲?댄듃 梨낆엫 紐낆떆

### 최근 closure 진행 (2026-04-22)

- arena scratch slice 3嫄?異붽? ?≪닔 ??`docs/94_arena_index_lifetime_plan.md` ?낅뜲?댄듃
  - `semantic.c:50` `semantic_preload_stdlib_uses` ??per-iteration `malloc/free` module path 조립??function-local `PgyArena` ??동. 배치 alloc ?나??렴
  - `type_checker.c:1109` enum method name mangling??`malloc/snprintf/free` 瑜?`pgy_arena_fmt(&ctx->scratch_arena, ...)` 濡??대룞. `symbol_create_function` ???대? ?대? `pergyra_strdup` ?쇰줈 ?대쫫??蹂듭궗?섎?濡?arena ?덉텧 ?놁쓬
  - `slot_analyzer.c:1067` `slot_analyze_parallel_block` ??outer task metadata 배열 3?(`task_accesses`/`task_counts`/`task_caps`) ??`sa->ctx->scratch_arena` ??동. per-task inner 배열? ?전??`collect_slot_accesses`  heap-owned??- arena scratch 2?slice 추? (같? ??
  - `type_checker.c:355` type resolution cycle detection ??`visited`/`path` 배열 ??`ctx->scratch_arena`. cycle text??return-contract helper??보류
  - `type_checker_flow.c:499` match redundancy ??`seen` 배열 ??`ctx->scratch_arena`
- arena scratch 3?slice ??HIR/MIR ?진입 (같? ?? ?후 4차에??routine-scope??합??
  - `hir.c:hir_compute_cfg_dominance` ??`visited`/`postorder`/`idoms` 3배열 ??function-local `PgyArena`
  - `hir.c:hir_mark_natural_loop` ??`in_loop`/`stack` 2배열 ??function-local `PgyArena`
  - `mir.c:mir_apply_ssa_rename` outer 3배열 ??function-local `PgyArena`
- arena scratch 5?slice ??LLVM 백엔???진입 (같? ?? ?후 6차에??ctx-scope ??합)
  - `llvm_register.c:llvm_register_enum_decl` ??`enum_fields` + per-variant `payload_fields` type-ref 踰꾪띁瑜?function-local `PgyArena` 濡??섎졃
  - `llvm_intent.c:llvm_collect_mir_intent_participants` ??return-ownership 계약?라 deferred
- arena scratch 6?slice ??**LLVMGenCtx ctx-scope scratch arena ?입** (같? ??
  - `LLVMGenCtx` ??`PgyArena scratch` ?드 추?
  - `llvm_ctx_create` / `llvm_ctx_destroy` ?서 lifecycle ?  - 5차에 function-local ??작??enum type-ref arena ?`ctx->scratch` ??렴. LLVMGenCtx ?나??init/destroy ??번만
  - ?속 LLVM scratch ?이??(미래??발굴?는) ????arena ?사????- arena scratch 7?slice ??**LLVM 9 ?이???괄 개수** (같? ??
  - tuple literal (`llvm_expr.c`) ??vals + tys
  - event handler type / tuple type (`llvm_backend.c:ast_type_to_llvm`) ??param_types + fields
  - event INVOKE (`llvm_domain.c`) ??inv_params + call_args
  - class/enum/extern ?깅줉 (`llvm_register.c`) ??4 param-type 踰꾪띁
  - ability vtable (`llvm_domain.c`) ??outer vt_fields + per-method ptypes
  - 공통: LLVM C API  type/value 배열???? 복사???scratch-safe
  - 결과: LLVM ?체??short-lived type 배열 assembly  ctx arena ?나??렴
- arena scratch 8?slice ??**LLVM 17 ?이??추? 개수** (같? ??
  - `llvm_stmt.c`: lambda param, parallel closure ctx/wrapper/handles, async closure fields, select rotation BBs
  - `llvm_intent.c`: intent function param_types, step completion `completed_allocas`, `saved_participant_ptrs`
  - `llvm_domain.c`: world sync `prev_active_addrs`, domain struct `ftypes` (4 분기), role/class method `ptypes` (2 ?이??, vtable `vals`
  - LLVM ?scratch-safe calloc/malloc ? 거의 개수 `ctx->scratch` ??렴. ?? 것? return-ownership 현재 helper ? AST-field stored ?스

- arena scratch 4?slice ??**HIR/MIR routine-scope arena ?입** (같? ??
  - `hir.h` HIRRoutine / `mir.h` MIRRoutine ??`PgyArena scratch` ?드 추?
  - ?성: `hir_append_*`, `mir_lower` 루프 ??`memset` 직후 `pgy_arena_init(&routine.scratch, 0)`
  - ?괴: `hir_destroy()` / `mir_destroy()` per-routine cleanup + OOM 경로 (배열 ?입 ?패 ?스)
  - 3차에 function-local ??작??3?arena ?모두 `&routine->scratch` ??합 ??routine ?나??init/destroy ??번만. ?러 HIR/MIR pass  같? arena ??사??  - MIR pass??`routine->scratch` ??. `routine->hir_routine->scratch` ??HIR frozen 계약?라 ?근 금? (코멘으로 고정)
- ?칙 ??: `scratch-only local temp 먼?, returned string ?중`. `slot_ref_expr(...)` 같? 반환 ownership 현재 helper??아직 보류
- 베? acceptance line #8 ("scratch/result lifetime?cache boundary 문서/구현 기?으로 증명 ?하??) ???당 slice 기여

### 최근 closure 진행 (2026-04-21)

- C/LLVM init idiom ?감사 + 1??비 ?료 (`docs/93_codegen_idiom_audit.md`)
  - 6 case × 2 backend 매트? 고정
  - **Case 1 HIGH divergence ?소**: 개수-바디 `let x: T;` (annotation + no init)??`PGY_CODE_SEM_UNINIT_LOCAL` ?거?. C??scalar-zero, LLVM? store ?략으로 ?read?서 ??? 갈라???복 경로?semantic ?벨?서 차단
  - **Case 2 C backend L815 ?뺣━**: `transpiler_c_type_uses_scalar_zero` helper濡?scalar/aggregate 遺꾧린. 湲곗〈 ?좊났 踰꾧렇 (`struct Foo x = 0;` invalid C) ?쒓굅 (defense in depth)
  - **Case 3 MEDIUM ?도 비????정**: slot claim? C ????helper, LLVM??IR-direct. 현재 runtime observability ???서 ?side effect 0. runtime observability ?장 ???감으로 deferral
  - ?뚭? 3醫?異붽?:
    - `function-body let with annotation and no initializer is rejected`
    - `function-body let with aggregate annotation and no initializer is rejected`
    - `subject field let with no initializer does not trigger the uninit-local guard` (negative)
  - ?서 구조 ?확?? class/subject field??ClassField 경로?분리되어 `AST_LET_DECL`???님 ??guard field-level ???침범?? ?음
  - docs/72 ??`PGY_SEM_UNINIT_LOCAL` ?뱀뀡 + docs/93 cross-link 異붽?

### 최근 closure 진행 (2026-04-20)

- own/ref broader audit瑜?helper family 湲곗??쇰줈 ???뺣젹
  - helper call boundary??`subject` / general boundary value 경로?공용 borrowed-boundary validator??음
  - container store / array literal store borrow-escape?공용 ownership diagnostic helper??합
  - semantic channel send borrow-escape??공용 ownership diagnostic helper??격
  - ? `assignment / helper call / channel send / container store / array literal store / constructor field store` ?점 같? provenance wording family??렴 ?- intent authority mismatch provenance???직접?으??출
  - `authorized by` unknown participant / non-subject participant / zone subject-slot mismatch / zone authority mismatch??`approval boundary provenance` ?뱀뀡 異붽?
  - provenance 비어 ?으?`no inherited/derived authority provenance was recorded`?명시?으?보고
- relation/effect/projection failure depth瑜?異붽? 蹂닿컯
  - invalid projection source / tobject source rejection??target/source consumer path? projection contract origin??직접 보고
  - ? projection diagnostics ?순 type mismatch 아니며 `target slot <- source slot` 경로?기?으로 ?명?기 ?작??- 현재 베? blocker ?정??  - Windows backend-compare / LLVM parity 복구
  - declaration-side MIR-only ?⑥? host/inventory helper debt ?쒓굅
  - own/ref ?쇰컲?붿쓽 broader assignment / container / rebind / summary path closure
  - intent/zone/world ?relation/effect/projection provenance 마???화
- Windows-native compile hygiene瑜?異붽? ?뺣━
  - `type_checker_builtins_query.inc`, `type_checker_builtins_nominal.inc`??`%zu` / extra-arg formatting drift瑜??쒓굅
  - `type_checker_decls_world.inc`??world lifecycle diagnostics placeholder-arg mismatch瑜??쒓굅
  - `type_checker_builtins.c`??ownership/channel helper?full internal header include ???최소 forward declaration으로 고정??enum/static helper ?선??충돌???함
  - 현재 기???
    - `test-semantic`: `1855 passed, 0 failed`
    - `test-transpile`: `601 passed, 0 failed`
  - ?? Windows blocker??semantic compile ?계 아니며 native MSYS2/MinGW ?행 ?경?서??backend/runtime parity ?인 축으??동

### 최근 closure 진행 (2026-04-16)

- declaration-side host context瑜?inventory-backed handle 履쎌쑝濡????④퀎 ???뺣젹
  - transpiler host lookup??`current_host_decl -> within_zone -> saved host-name inventory` ?쒖쑝濡?蹂듭썝?섎룄濡?議곗젙
  - zone/relation/effect/world field query helper raw `current_*_name` 분기보다 inventory-backed `current_host_decl`??선 ?비
  - ? declaration-side C backend context 복원?서 string name state???점 restore hint로만 ?고, ?제 host truth??active inventory 기반 handle??렴 ?- explicit/compressed canonical pair examples?intent-first 대해 규칙으로 ?시 ?렬
  - large/composite pair source??`intent -> world/zone -> subject` read order?직접 명시
- world embedding implicit copy?warning??아니며 hard contract??격 ?작
  - world constructor??zone binding??洹몃?濡??섍린硫?explicit `Clone(...)`瑜??붽뎄
  - hidden copy semantics????상 benign warning으로 ?기 ?음
- generic contract consumer path瑜????④퀎 ???レ쓬
  - omitted trailing default type arg user-defined generic class specialization path?서??effective arg 기?으로 증되?록 ?렬
  - role impl / action requires / zone authority / party role slot?서 `default arg omission + where-bound violation` negative regressions 추?
  - multi-bound / omitted-default / consumer provenance 조합 ???semantic 기?으로 고정
  - ability consumer path / class instantiation-specialization path?서 unresolved effective generic arg?silent skip?? 하고 structured error??격
  - role-side ability require-field type resolution?서??unresolved effective generic arg?silent skip?? 하고 structured error??격
  - malformed impl ability generic arg 되어???쪽 where/require-field 증으?partial 진행?던 경로?차단
  - default generic bound validation?서 unknown parameter / unresolved default type??structured error??격
  - generic function call-site where-clause validation?서??missing/unresolved effective arg?silent skip?? 하고 structured error??격
- own/ref 泥??쇰컲??vertical slice ?쒖옉
  - existing movable resource value(`QubitSlot`)??function boundary?서 explicit `own` transfer parameter??용
  - `ref QubitSlot`??아직 미닫??subset으로 ???되, ?유/consumer path/fix ?함??structured diagnostic으로 고정
  - ? `own/ref`???전???역 closure ?이? move semantics ?? 는 resource value????서??explicit transfer boundary 분적으로 ?리??작??  - return/channel boundary ownership diagnostics??`Reason:` / `Fix:` 구조??렬
  - function signature anchored-return rejection??`Reason:` / `Fix:` 구조??렬
  - unnamed movable-resource channel send??moved-here provenance??명는 hard error?고정
  - local binding ?계?서??`recv/await` unnamed boundary use, subject rebinding, released-slot move, anchored-handle rebinding??`Reason:` / `Fix:` 구조??렬
  - slot escape analyzer 경고??return/helper-call/channel/unterminated local claim 경로?서 provenance??`Reason:` / `Fix:` 구조??렬
- relation/effect/projection contract????드?게 조???  - `intent step causes` zone effect slot 이 ?과?던 경로?hard error??격
  - `action causes`??zone effect slot 이 는 경로?structured hard error??격
  - authority-bearing `apply/link/detach/unlink/maintain` `by <subjectSlot>` 이 는 경로?hard error??격
  - duplicate authority, unknown layer relation/effect type?????상 benign warning으로 ?기 ?음
  - maintain/detach/unlink duplicate/conflict diagnostics??`Reason:` / `Fix:` 구조??렬
- unresolved declaration entrypoint???줄???  - role include unknown role, roster slot unknown party, world roster/zone unknown type??hard error??격
  - generic where-clause consumer path?서 unresolved effective arg?????상 silent skip?? ?음
- declaration-side MIR-only domain method gate???조???  - party / roster / relation / effect / zone / world method emission??MIR routine 이 AST body?조용??fallback?? ?도?C backend??렬
  - role / domain method emission?서 MIR routine 미존?? LLVM backend hard error??격
  - ? declaration-side domain method??MIR inventory 존재는 빌드?서 silent fallback??아니며 explicit backend failure?계약으로 ?음

### 최근 closure 진행 (2026-04-14)

- declaration-side MIR-only intent inventory???한다
  - MIR `IntentParticipant(alias,type)` metadata?직접 ?반
  - C/LLVM intent declaration emission??participant alias/type?AST 대해??이 MIR metadata??선 ?비
- step-level MIR-only validation??AST field 議댁옱 寃?ъ뿉??metadata 議댁옱 寃?щ줈 ??꼈??  - `IntentCheck`
  - `IntentEval`
  - `IntentZoneWhere/IntentZoneAlias/IntentZoneFrom`
  - `IntentWho/IntentDispatch`
  - `compensate` 議댁옱 ?먯젙
- intent emission cleanup/rollback 寃쎈줈??metadata gate瑜?C/LLVM ?????뺣젹?덈떎
- 愿???뚭?:
  - `test-mir` green
  - `test-transpile` green

? intent declaration/step emission? 아직 ?전 MIR-only ?언???난 것? 아니?
`participant/step contract inventory`?AST presence??기?????거친 fallback?????계 ???거한다.

### 踰좏? 湲곗???異붽? (2026-04-15)

- `docs/70_beta_closure_master_board.md` 異붽?
  - B0 4? declaration-side MIR-only debt, parity, runtime observability, surface trust????으?고정
  - 베? acceptance line?exit rule??명시
  - ?으?TODO??개별 ?업? ??보드 기?으로 ?선?위??른??
### 베? 최종 ?(2026-04-18)

- [ ] **declaration-side MIR-only?구조?으??기**
  - zone/world/relation/effect declaration/method emission?서 ?? AST/HIR-carried inventory dependency????거
  - `current_*_name` / host-name 異붿젙 helper蹂대떎 inventory-backed host handle / metadata ?뚮퉬瑜??곗꽑?섎룄濡??뺣젹
  - transpiler/LLVM ?쪽?서 raw host-name read?helper/restore layer 밖으??시 ?? 못하????고정
  - declaration emission failure??comment/skip/fallback return??아니며 explicit backend error??격
  - C/LLVM ????declaration-side path?서 `Unknown` / surface-trust-breaking fallback type emission??계속 ?거
  - 문서?서 `MIR-led / HIR-assisted`하고 ?겨??debt??제 구현 기?으로 ??축소?고, 베? ?점 ?현?구현???치?킨??
- [x] **AST dispatch / backend fallback trust gate 고정**
  - `docs/95_ast_dispatch_partition.md` 기?으로 AST ???partition??문서??  - LLVM `stmt/expr` default path??warning-only 아니며 structured backend error?고정
  - Zone/World declaration verb expression fallback으로 조용??`0/null`??는 경로?explicit backend diagnostic으로 차단
  - `tests/ast_dispatch_partition_smoke.sh`? `make ast-dispatch-test-smoke`?추???partition drift? silent fallback ???CI?서 차단
  - Linux `ci-linux` acceptance line??AST dispatch smoke瑜??곌껐

- [x] **type-resolution DAG?beta blocker??함**
  - import resolver? 별개?semantic type dependency graph?beta acceptance line???함
  - generic default / multi-bound / role impl / action / intent step / party role slot / zone authority / module contract consumer?같? graph inventory?추적
  - alias depth limit / ad-hoc recursive failure蹂대떎 path-aware cycle diagnostic???곗꽑 湲곗??쇰줈 ?뚯뼱?щ┝
  - 1?계 진행: `topo_order`?버리 하고 declaration staged worklist???결 ?작
  - 반영 문서:
    - `docs/70_beta_closure_master_board.md`
    - `docs/63_feature_depth_matrix.md`
  - 1?계 진행: `world/zone` local contract? `refresh` projection path?synthetic graph node??리??작
  - 1?계 진행: topo worklist `LOCAL_CONTRACT` / `PROJECTION_PATH` synthetic node???시 ?비?기 ?작
  - 1?계 진행: synthetic node ?비?host ?체 ?실이 아니며 label?narrow handler?축소
  - 1?계 진행: role impl consumer까? cycle provenance ???추???ability consumer family????성
  - ?? ?? staged declaration prepass 범위??히?graph-backed evaluator?semantic source-of-truth??격
  - ecosystem ?뺤옣(`stdlib/pkg/tooling`)? ??DAG closure ?댄썑 ?④퀎濡?誘몃８

- [x] **own/ref ?쇰컲??audit 留덇컧**
  - own/ref??ownership classifier 湲곗? stable subset?쇰줈 ?ロ옒
  - borrowed value escape??helper call / channel / return / container store?아니며 broader assignment/member/store path까? provenance 기?으로 ??
  - 진행: constructor field store(`Holder(packet)` 같? boundary-visible store)?borrowed escape 경로??격하고 semantic regression 추?
  - 진행: constructor field store??borrowed member/aggregate source path provenance(`holder.packet`, `items[0]`)?직접 보고?도??렬
  - 진행: array literal store(`[packet]`)??borrowed escape 경로??격하고 semantic regression 추?
  - 진행: member assignment / array overwrite 진단??identifier-only 아니며 `holder.packet`, `items[0]` 같? target path provenance?직접 보고?도??렬
  - 진행: new-binding escape??identifier-only 아니며 borrowed member/aggregate source path provenance(`packet.view`, `items[0]`)까? 추적?도??장
  - 진행: new-binding escape regression??member source path(`packet.items`)? array source path(`items[0]`)?fixture?고정
  - 진행: container store(`ArrayPush`/`ListPush`/`SetAdd`/`QueuePush`/`MapSet`)??borrowed member/aggregate source path provenance?직접 보고?도??렬
  - 진행: helper forwarding / builtin channel send(`Send`/`TrySend`/`SendTimeout`/status variants)??unnamed borrowed member/aggregate source path provenance?직접 보고?도??렬
  - 진행: direct `return` escape??borrowed member/aggregate source path provenance(`holder.packet`, `items[0]`)?직접 보고?도??렬
  - 진행: slot/resource summary 기반 `return/channel/helper` diagnostics??`summary provenance root` vocabulary?direct semantic wording????깝게 ?렬
  - 진행: summary-based helper escape??direct callee wording ???`helper/function summary in '<fn>'` 경로?분리??drift?줄임
  - 진행: summary-based return/channel escape??direct consumer wording ???`return summary in '<fn>'` / `channel summary in '<fn>'` 경로?분리??drift?줄임
  - 진행: anchored-handle summary escape??direct `return/channel/helper` wording ???summary wording으로 분리??own/ref bridge 문구??렬
  - 진행: helper-call / container-store / array-literal-store / semantic channel-send diagnostic family?공용 helper??합
  - 진행: nested projection + transitive helper + member rebind 조합??semantic regression fixture?추?
  - 진행: movable-resource + nested member source + member rebind target 조합??semantic regression fixture?추?
  - 진행: declaration-side MIR-only host truth??`current_host_decl` / inventory 기?으로 ??좁혔? `within_zone`??라??transpiler host recovery fallback?role-owner direct AST lookup???거
  - 진행: own/ref anchored-handle wording??assignment / let-binding / return / channel / helper family??맞춰 `boundary-visible handle binding` / `anchored-handle provenance` 기?으로 ?렬
  - ?료 ?정: direct/summary helper-chain, return/channel/helper, destructure, assignment/member/container/constructor/array path current semantic regression으로 고정??  - explicit reject: authority-bearing `Token<T>` escape/transport
  - beta-out-of-scope: region/lifetime solver? universal ownership lattice

- [ ] **generic contract ?꾧꼍濡?audit 留덇컧**
  - generic contract??`default type arg`, `multi-bound where`, `ability<T> consumer`, `zone authority`, `party role slot`, `impl/reference`, cross-module consumer path?마?막까 audit
  - 진행: `party role slot` generic mismatch consumer??actual/expected type arg + consumer path provenance regression으로 고정
  - ?? generic consumer path ?다??것을 regression으로 증명?고, partial acceptance처럼 보이??경로??기 ?는??
- [ ] **Intent/Zone/World, relation/effect/projection 진단?provenance 마감**
  - intent/zone/world??embedding / handoff / authority mismatch?서 contract source, derived zone/using, transfer edge provenance?계속 강화
  - relation/effect/projection? propagation edge failure, contract mismatch, branch/join/handoff path??`Contract source:` / `Reason:` / `Fix:`? source/target provenance????게 ?  - 진행: world embedding/handoff? intent transfer/authority mismatch???심 경로?`Contract source:` / `Reason:` / `Fix:` 구조??정??  - runtime contract provenance? diagnostic wording?????렬???왜 ?패는 + 계약???디??는 + ?떻?고칠?? ??번에 보이?한다
  - helper-heavy edge path?줄이? compile-time contract ?패?silent/best-effort runtime sync??기 ?는??  - 진행: intent step contract-source summary `authorized by`, transfer handoff, derived transfer zone provenance???직접?으??명?도??렬
  - 진행: zone-within action authority mismatch `within` / `causes` header?contract source?직접 보고?도??렬
  - 진행: world embedding / post-embedding mutation diagnostics `world <name> zone slot <slot>` contract source? world-owned authority/handoff destination??직접 보고?도??렬

- [ ] **C/LLVM parity + full CI green??베? 최종 문으?고정**
  - Linux 湲곗? `parser / semantic / transpile / ABI / backend-compare / llvm smoke / ir-pipeline / example smoke`瑜?full green?쇰줈 ?좎?
  - Windows??로컬 Linux host?서 강행?? ?고, MSYS2/MinGW + LLVM runner?서 `ci-windows` full green???시 고정
  - backend compare??domain semantics 기? parity?계속 ???고, same-process ABI / launch / runtime environment 차이??발?? ?게 ?는??  - 현재 immediate blocker: Windows `backend-compare`? LLVM parity??마??crash / launch / runtime mismatch ?거
  - 베? ?언 ??acceptance line? ???green이 아니며 C/LLVM parity? expected stdout/stderr/result parity까? ?함??CI green으로 한다

?행 ?한 ?구??컴파?러 ?계???겼? 아직 베?하고 ?는 한다.

?먯젙 湲곗?:
- 베? ?칙??`?구현 ?태??기 ?는???아직 충족?? 못함
- ?워??족이 아니며 `구현 depth 불균????문제??- parser 받는 surface ??? semantic/C/LLVM/runtime/test/documentation까? ?전???히 ?음

### ?? ?힌 축과 ???상 베? 차단???닌 ?
- `public/private/export` module boundary
  - top-level nominal/domain/callable visibility ?렬 ?료
  - private `func/intent/event` cross-module call 차단 ?료
  - private `zone/effect` action-contract leakage 차단 ?료
- nominal token split
  - `subject/class/struct/object/tobject`??lexer token ?벨?서 ?? 분리??- ability field surface
  - legacy `require` alias ?거, `fields` canonical surface 고정
- generic ability baseline
- `ability<T>`, `requires Ability<T>`, `impl ability Ability<T>`, zone authority generic ref, mismatch diagnostics baseline 議댁옱
- cross-module imported generic ability??multi-bound zone-authority consumer regression 異붽?
- ?묒옄 surface
  - 踰좏? ??곸뿉???쒖쇅
  - `v2 / experimental`濡쒕쭔 異붿쟻

### 현재 베??막는 ?제 B0 ?
#### 1. Intent / Zone / World closure

?재:
- intent orchestration, inherited/derived contract, rollback/cleanup carrier, zone/world declaration?기본 lowering? 존재
- zone/world projection/layer/state query??議댁옱
- intent runtime observability baseline??議댁옱
  - `IntentLast*`
  - `IntentHistoryStep*`
  - `IntentActive*`
  - `IntentRecent*`
  - active/recent handle + active-step field query builtin??semantic/transpiler/runtime/LLVM baseline ?결 ?료
  - runtime ?? recent ring + active registry + typed step history storage ?결 ?료
  - ABI regression: `IntentRecent*` trace/failure baseline, failed-intent provenance, world zone query, relation/effect zone state parity 고정
  - backend parity: embedded world -> zone projection visibility regression 고정

?⑥? 寃?
- embedding ownership / handoff policy?surface trust ??까? 명확??고정
- richer multi-instance timeline query? failure provenance ?뺢탳??- cross-layer propagation policy????源딆? closure
- C/LLVM parity?declaration/runtime/diagnostic까? 같? ?질??렬

#### 2. relation / effect / projection closure

?재:
- declaration, lifecycle shorthand, `refresh/publish/bind`, layer/state query, overlay sync baseline 議댁옱
- effect join/meet/conflict API? basic closure 議댁옱
- projection contract diagnostics??target/source/mode/fix??함는 structured error 쪽으?보강??- backend parity:
  - embedded world -> zone projection visibility regression 고정
  - relation/effect layer + state propagation parity regression 고정

?⑥? 寃?
- authority/resource? effect partial order?????전???합
- projection propagation policy ?화
- runtime contract? deeper propagation failure provenance???증명 ?하??리
- C/LLVM parity?서 helper-heavy edge path 감소

#### 3. generic contract closure

?재:
- generic ability declaration/reference baseline 議댁옱
- action / intent step / zone authority / party role slot generic mismatch diagnostics stable 議댁옱
- hidden/default-export generic ability visibility??action/role impl?아니며 zone authority/party role slot consumer path까? ???고정
- `ability<T> where ...` bound??`requires` / `impl ability` / party role slot ref?서 ?시 증됨
- default type argument??semantic + transpiler + backend compare까? baseline closure ?료
  - user-defined `class/ability<T = ...>` omitted arg 경로?서??effective specialization으로 ?렬??  - non-deduced trailing generic parameter default??function call `where` validation 경로?서 ???고정
  - cross-module omitted default generic ability consumer(`party role slot` / `zone authority`)?????고정
- multi-bound `where T: A + B` baseline? 현재 ?작??- hidden/default-export? generic ability ref 규칙 ?렬 ?료

?⑥? 寃?
- broader type-family generalization??beta 범위 밖으?명시
- richer generic constraint validation??beta contract 범위?문서/board???치?켜 고정
- import/use surface? diagnostics/tooling ?현??module contract 기?으로 ?????게 ?리

#### 4. own/ref closure

?재:
- anchored subset? ?ロ? ?덉쓬
  - `ref Slot<subject-host>`
  - `own SecureSlot<subject-host>`
- first movable-value transfer slice???쒖옉??  - explicit `own QubitSlot` parameter???덉슜
  - `ref QubitSlot` borrow boundary baseline ?덉슜
  - call-site??`own/default`硫?consume, `ref`硫?borrow ?좎?濡?遺꾧린
  - borrowed `ref QubitSlot`??`return` / `channel send` escape??semantic?서 명시 차단
- ??진단/?제/문서??현재 구현 기?으로 ?렬??
?먯젙:
- anchored subset baseline? ?? ??? beta-quality 기??서??own/ref??시 ?성 blocker?본다
- ?? ?? ?반 movable type ownership model, copy vs move-only 분류, assignment/call/return/channel/container/rebind ?경?analysis, richer provenance diagnostics?는 것이??- ?히 borrowed movable-resource ownership??helper-call/return/channel-send baseline???혔? ?음? wider movable type generalization?container/rebind provenance????아??한다
- anchored subset?stable?라?보고 되어?ownership story partial acceptance??는??
### ?이?별 현재 진실

#### ?쒕㎤??
- 강한 ?
  - nominal family
  - subject/action
  - async/channel/select
  - generic ability baseline
  - visibility/export boundary
- 아직 ?? ?
  - richer generic constraint validation
  - general own/ref
  - event closure???붿뿬 negative path
  - collection semantic depth

#### 肄붾뱶 ?앹꽦

- C backend:
  - 코어 surface?????숙
  - method owner metadata HIR->MIR??려? declaration-side zone/relation/effect/world context 복원 ???름 추정보다 MIR metadata??선 ?용
  - 진행: `transpiler_emit_host_method_body_local`??manual save/restore ?태?`TranspilerMirEmitState` snapshot helper?축소
  - 진행: `emit_func_decl_from_mir_named` / AST fallback `emit_func_decl_named`??`TranspilerMirEmitState` snapshot helper??렴
  - 진행: `emit_intent_decl`??function-scope out/render/return/local-count restore??`TranspilerMirEmitState` snapshot helper??렴
  - 진행: generic class specialization method body??MIR inventory 존재 ??AST fallback ???MIR routine gate / explicit backend error??렬
  - 진행: LLVM domain/role missing-routine errors??`PGY_CODE_LLVM_MIR_ROUTINE_MISSING` / cause / fix structured path??렬
- LLVM backend:
  - MIR-led / HIR-assisted hybrid
  - ordinary routine? MIR 중심???domain declaration??? bootstrap/helper path??HIR/AST ?존 ?존
  - pure MIR-only하고 르기는 아직 ?름??과함

#### ?고???
- 강한 ?
  - slot / secure baseline
  - async/channel basic runtime
  - basic intent execution/rollback
  - intent observability baseline (`last` / `history` / `active` / `recent`)
- 아직 ?? ?
  - richer multi-instance timeline / failure provenance
  - channel backpressure protocol
  - party edge-path completeness
  - richer zone/world runtime policy

### 而щ젆??/ ?쒕㈃ ?좊ː

- `Map<K, V>`??현재 `String | Int | Long | Bool` key stable subset까? ?린??- ?것? 버그 아니며 현재 contract
- arbitrary key-universal map contract??아직 generic closure debt??는??
### ?대쭅

- LSP / formatter??베? 차단 ?심???님
- debugger / package manager / WASM??베? 차단 ?심???님
- ?들? B0 closure ?후???루??것이 맞음

### 베? 직전 ?리 ?칙

1. ???워????축을 ??추??? ?는??2. ?? 미완??surface?`?성`?거??`experimental`??린??3. `?자`, `WASM`, `?키 매니?`, `고급 ?버???베? ??에???외한다
4. B0 4개? ?기 ?에??베?하고 르? ?는??
---

## ?료 (P0 ??Pain Point ?정, 2026-04-12)

- [x] **P0-1: Array for-in `.count` ??`.length`** ??`transpiler.c`?서 Array??`.length`, List??`.count` ?용
- [x] **P0-2: `StringSplit`/`StringJoin` ????구현** ??`pgy_runtime.h`???제 구현 추?, ?맨??C 백엔???치
- [x] **P0-3: `None` ?볼 ?의** ??`type_checker.c`?서 AST_IDENTIFIER 처리, `type_system.c`?서 `Option<unknown>` ??`Option<T>` ?당 ?용, 코드?에??`expected_type` 기반 ????결
- [x] **P0-6: defer ???코??버그 ?정** ??`type_checker_flow.c`?서 defer body 처리 ????resource-state snapshot/restore. cleanup body??`return`/`break`/`continue`? QubitSlot release/move???하?주? CFG path? outer loop flow??비?? ?는?? direct `type_check_statement()` fallback??같? helper??용한다.
- [x] **P1-7: struct/subject Slot 매크?warning ?제** ??`transpiler.c`?서 `#pragma GCC diagnostic push/pop`으로 `-Wunused-function` ?제
- [x] **P1-emit_call ?메우?* ??`BUILTIN_BOX_ARRAY`, `BUILTIN_PARALLEL` ?스 추?
- [x] **P0-4: enum match OR ?턴 ?정** ??`type_checker_flow.c`?서 named variant OR ?턴 ?용 + coverage 체크 ?정
- [x] **P2-13: match 기반 개수 default return ?동 ?성** ??`transpiler_emitters_base_b.inc`?서 non-void 개수 ??fallback return 추?
- [x] **Pain Point 보고??* ??`docs/68_pain_point_report.md`???정 ?역 기록

## ?료 (최근)

- [x] **Windows ABI/backend-compare precheck ?ㅽ뻾 寃쎈줈 ?뺢퇋??*
  - `compiler_run_binary()` MSYS ????`/tmp/...` ?`/<drive>/...` ?행 ?일 경로?그??`_spawnvp()`???기??문제??정
  - Windows?서 executable launch??native Win32 경로??규?한 ???행?도??렬
- [x] **nested vessel-source projection ambiguity closure**
  - zone `refresh/publish/bind` projection contract 경로?서 ambiguous source path `missing`으로 ?진?던 분기 ?서??정
  - builtin `ToObject` / `ToTObject`???숈씪??structured `Reason/Fix` ambiguity diagnostic?쇰줈 ?뺣젹
  - nested vessel ambiguity semantic regressions 異붽?
- [x] **generic consumer provenance diagnostics 蹂닿컯**
  - `action requires` / `zone authority` / `party role slot` / `intent step requires`?서 generic ability mismatch `actual type argument` / `actual implementation` provenance??께 보고?도??렬
  - 愿??semantic ?뚭? 異붽?
- [x] **anchored own/ref provenance diagnostics 蹂닿컯**
  - closed-subset / local-only / missing `own/ref` / `ref` escape 진단??`Reason/Fix`? borrowed-here provenance?추?
  - 愿??semantic ?뚭? 異붽?
- [x] **world embedding structured diagnostics ?? 고정**
  - embedded zone old-binding mutation??assignment / hosted func-action call 모두?서 `Reason/Fix`? world-owned-copy provenance??기?록 semantic ?? 강화
- [x] **Windows shell smoke portability 蹂닿컯**
  - `abi_pipeline_smoke.sh`, `compare_backends.sh` `cmp`/`diff` ???경?서??`git` 는 Python fallback으로 비교/차이 출력???행?도??리
- [x] **surface trust docs ?뺣젹 ??collection/result/struct baseline**
  - `Array<T>`??`[]`, `List<T>`??`ListNew()`, `HashMap<K,V>`??`MapNew()`?canonical ?성 surface?고정
  - `Result<T>` 추출 API??`Unwrap` / `UnwrapOr` / postfix `?`?고정, `UnwrapResult()` ?면? 비채??  - `struct` field??legacy `let`? 불? ?식??아니며 declaration introducer을 문서?하? ?기 ?용 계약? `object/tobject`?만 한다
- [x] **generic default-arg closure 1?복구** ??declaration acceptance만이 아니며 user-defined generic class omission, generic ability impl-reference omission, arity diagnostics range?? semantic/backend parity까? ?시 ?색으로 ?렬
- [x] **ABI Unification Infrastructure** ??`pgy_abi_spec.h`, `test_abi_spec.c` (28 PASS), `MIRTypeLayout`, `mir_abi_lookup()`, `rir_dump_json()`, dumb emitter Visitor
- [x] **Windows CI Fix** ??`TOKEN_TYPE` ??`PGY_TOKEN_TYPE`, `TokenType` ??`PgyTokenType` (~20??일)
- [x] **v2 Quantum Planning** ???자 ?산 미???명시, v2 계획 문서??- [x] **Documentation Index** ??`docs/INDEX.md` ?성, ?체 문서 체계??- [x] **`HashMap<K, V>` stable key subset surface trust ?렬** ??semantic annotation/builtins/runtime comment/test?`String | Int | Long | Bool` key ?으??치?킴
- [x] **mixed `ability + zone` module export 충돌 ?정** ??default-export `ability` sibling zone visibility?깨뜨리던 ?규??버그 ?거, module smoke ?? 추?
- [x] **nominal host receiver type ?염 ?정** ??C backend member-call emit ?static type-name overwrite??거??`Int_Advance`??발??복구
- [x] **MIR cleanup exceptional topology ?? 복구** ??cleanup/rollback/invalidation block edge materialization?test expectation ?렬
- [x] **`order_analytics` example ?ㅼ쟾??* ??sketch ?섏? surface瑜??뺣━?섍퀬 compile-smoke covered example濡??밴꺽
- [x] **declaration name surface tightening** ??declaration name???쇰컲 ?앸퀎?먮줈留??쒗븳?섍퀬 reserved keyword ?ъ궗??surface ?쒓굅
- [x] **anchored-handle diagnostics/test ?렬** ??`own/ref` closed-subset 진단 문구? `DeviceSlot`/anchored-handle semantic test expectation??현재 구현 기?으로 ?치?킴
- [x] **계층??stdlib/domain kit v0 고정** ??`money`, `datetime(Duration/Instant)`, `timer`, `versioning`, `ledger`, `obligation`, `device_adapter` 모듈?probe ?제 추?, 코어 추? 금? ?칙 문서??
## 踰좏? ?대줈? 蹂대뱶

踰좏? ???먯튃:
- `?구현` ?태??기 ?는??- ?료?키 못하??surface???리거나 experimental?격리한다
- parser 받는 ?면? semantic/C/LLVM/runtime/test/documentation까? ?는??
### B0 ??????로? 개수

- [ ] **Intent/Zone/World semantics ?전 closure**
  - contract reuse/derivation / authority / lifecycle / embedding ownership / runtime observability / C/LLVM parity / regression
  - ?대? 議댁옱: intent orchestration, inherited/derived contract, zone/world query, observability baseline
  - 진행: runtime zone/world propagation cell??`epoch/cause` provenance baseline??되어갔고, LLVM intent rebound-zone sync??같? truth??렬??  - 진행: world derived-state chain? ?제 bounded recompute loop?대해 C/LLVM ?쪽?서 같? 규칙으로 계산??  - 강한 기?: ??축? ?제 "?? single-pass sync로도 beta ?? 같? ?석???용?? ?음
- ?음: embedding ownership/handoff policy, **handoff? ???? world-zone propagation family까? ?반?된 bounded fixpoint 기반 cross-layer propagation policy**, richer provenance query surface, declaration/runtime/diagnostic parity
  - ??축? 되어 ?체???체???beta 직전까? 되어?? ?는??- [ ] **relation/effect/projection semantics ?전 closure**
  - effect lattice, authority-resource partial order ?듯빀, refresh/publish/bind/causes ?쇨??? diagnostics, C/LLVM parity
  - ?대? 議댁옱: declaration, lifecycle shorthand, `refresh/publish/bind`, layer/state query, overlay sync, effect join/meet/conflict, projection contract diagnostics baseline
- 진행: relation/effect/zone projection hidden cell??C/LLVM 모두 `dirty/ready + epoch/cause` schema??렬하고 runtime contract provenance baseline????
- 진행: world-derived recompute??bounded pass loop??라?고, relation/effect/zone projection chain??bounded transitive recompute loop??라한다
- 강한 기?: projection propagation? ???상 "helper replay ?체로 맞음" ??으로 ?? ?고, transitive semantics ?히??까 beta blocker???
- ?음: authority-resource partial order ?합, projection/layer/state?되어??**authority/failure handoff? ???? world-zone propagation family까???full transitive frontier propagation policy**, helper-heavy edge path 감소, declaration/runtime/diagnostic/backend parity??마??shrink
  - ??축? domain semantics ?심???partial ?태?beta???리 ?는??  - projection diagnostics??`target/source/projection kind/field path/fix`??함하고 `Reason:` / `Fix:` ?맷으로 고정한다
- [x] **generic contract ?전 closure**
  - strict beta-quality 기?으로 stable subset closure?서 ?개?  - `default type arg` actual resolution, `where T: A + B` ?경?enforcement, `ability<T>` mismatch provenance, instantiation-path parity까? ?는??  - ?료: default type arg declaration acceptance / omitted trailing default resolution / generic ability impl-reference omission / arity diagnostics provenance
  - ?대? 議댁옱: `ability<T>` baseline, default type arg baseline, omitted trailing default resolution, generic mismatch provenance baseline
  - 진행: `party role slot` generic mismatch??`consumer path / expected type args / actual type args` vocabulary ???고정
  - ?⑥쓬: multi-bound ?꾧꼍濡?enforcement, module-contract propagation, instantiation-path parity, richer mismatch diagnostics, wider C/LLVM regression ?뺣?
  - generic mismatch??`generic subject / expected type args / actual type args / broken bound / consumer path / fix`??함하고 `Reason:` / `Fix:` ?맷으로 고정한다
  - generic? partial acceptance?beta???리 ?는??- [x] **own/ref ?전 closure**
  - strict beta-quality 기?으로 anchored subset closure?서 ?개방했? classifier-backed stable subset으로 마감
  - ?쇰컲 movable type ownership, move/borrow/escape/rebind/channel/return provenance, diagnostics/test parity源뚯? ?レ쓬
  - ?대? 議댁옱: anchored slot subset, anchored diagnostics baseline, anchored regression/docs alignment
  - ?료: summary/direct path family audit? classifier/docs 최종 ?렬
  - 진행: constructor field store escape 경로?boundary-visible store?고정하고 ?? 추?
  - 진행: array literal store escape 경로?boundary-visible store?고정하고 ?? 추?
  - 진행: assignment rebind escape diagnostic??member/aggregate target path(`holder.packet`, `items[0]`) provenance?직접 보고?도??렬
  - 진행: nested projection provenance constructor field store / member rebind / list/set/queue/map store / array overwrite / helper return summary / channel send / direct return까? ???고정??  - 진행: class/subject consumer matrix??return / channel / helper / list / set / queue / map / array push / array overwrite / member rebind / constructor field store까? 거의 ?형으로 ?렬
  - 진행: tuple/object 경로??기존 `test_semantic.c` ?? 축에??channel/new-binding/rebind/return/helper forwarding/queue-map-array overwrite/projection provenance coverage ??
  - 진행: slot-handle/class helper-chain ????ownership-boundaries 계열??추???direct helper/function call family transitive chain까? 고정??  - 진행: helper/return/channel wording family?`through ...` 기?으로 ?렬
  - ownership diagnostics??`value / ownership mode / moved|borrowed here / escaped|rebound here / consumer path / fix`??함하고 `Reason:` / `Fix:` ?맷으로 고정한다
  - explicit reject: authority-bearing `Token<T>` escape/transport
  - beta-out-of-scope: region/lifetime solver? universal ownership lattice

### B1 ??베? ?뢰??개수

- [x] **surface trust 문서 ?분?*
  - ?료: `docs/18_language_status.md`, `docs/63_feature_depth_matrix.md`, `README.md`?서 `stable subset / explicit reject / beta-out-of-scope` 기?으로 ?렬
  - 규칙: "컴파?? ???partial"???면??stable처럼 ?? ?고, ?디까???힌 계약으로 ?속는 먼? 명시
  - 규칙: broader generalization, arbitrary key support, general ownership, richer observability query 같? ??? `beta-out-of-scope`?분리
- [ ] **stable example / smoke source of truth ?뺣?**
  - canonical examples? closure examples?smoke??직접 ?결
  - explicit surface vs compressed surface?같? ???보여주는 pair example 최소 4??고정
  - ??? app/web orchestration, game/simulation, async/worker/device, world-handoff/domain propagation
- [ ] **Backend parity final closure**
  - C/LLVM??domain semantics 기?으로 같? 결과?는 고정
  - ??? intent/zone/world, relation/effect/projection, ownership boundary, refresh/publish/bind, world embedding/handoff
  - 기?: backend compare / llvm smoke / example smoke / ABI-runtime probe Linux/Windows 모두 ?색
- [ ] **experimental surface ?쒓굅 ?먮뒗 寃⑸━**
  - ?? 못한 parser surface??명시 거? 는 문법 ?거

## Pain point freeze board

?먯튃:
- 기능?????히?에 반복?서 ?시 깨????성/진단 pain point?먼? 고정한다
- ?pain point??`stable contract + regression + docs wording`까? 같이 ?근??- recoverable failure? invariant break?같? 방식으로 처리?? ?는??
### Failure handling policy freeze

분류:
- `recoverable failure`
  - ?용??코드 ?상 ?한 ?패
  - ?? intent failure, authority/boundary rejection, timeout, remote failure, empty/closed operational state
  - ?먯튃:
    - ?로?스?죽이 ?는??    - `Bool` / `Result<T>` / queryable runtime state??러한다
    - reason / boundary / authority / step provenance?조회 ?하??긴??- `contract violation`
  - ?칙?으?semantic ?계?서 차단
  - ???까 ?면 structured panic
  - ?? released slot access, invalid secure token, ownership boundary ?반
- `internal compiler/runtime bug`
  - 즉시 중단
  - internal error / panic?명확??분리
  - ?용??코드 ?패처럼 ?장?? ?는??
현재 고정:
- intent/zone/world ??패???기?으?`recoverable failure`??렴?킨??- slot/token/invariant 계열? 계속 hard fail?한다
- `Unwrap(...)`??panic ?격??sharp tool????고, recoverable path??기본 계약으로 ?? ?는??
- [ ] **large canonical pair ?덉젣 異붽?**
  - ???제?서 `explicit`? `compressed`?????stable source of truth???한다
  - 최소 4??일 기?으로 리한??    - `calendar manage-event`: explicit/compressed
    - `composite intent orchestration`: explicit/compressed
  - 紐⑹쟻:
    - ???제???체 계약??명시?으?을 ???게 ??
    - 같? ???축약?으로도 바로 복사???작?????게 ??
    - smoke?서 ???제 모두 ?행 ?하?록 고정
- ??보드??sugar backlog 아니며 beta surface trust??기 ?한 고정?이??- P0 pain point ?기??에??declaration-side MIR-only debt?? 복구 ?에???게 건드리? ?는??- backend ?? ?리??pain point 기?과 ?? 먼? 고정???에??시 ?장한다

### P0 ???성/계약 pain point

- [ ] **contract clause density 고정**
  - ??? `requires / within / authorized by / causes / refresh / publish / bind`
  - 문제: 같? ???action / intent step / zone?서 중복 기술?게 되어 ?성 으로 커짐
  - 고정 기?:
    - ?디까? inherited/derived 는 vocabulary?고정
    - 길게 는 버전??축 버전???? 차이 문서/진단/?제?서 같아????    - canonical pair? minimal subset example??????분리??source-of-truth?고정
  - ?뚭? 湲곗?:
    - semantic regression: inherited/derived contract source 진단???출
    - example smoke: long-form vs compressed-form ?덉젣 ?????좎?

현재 source-of-truth:
- canonical pair
  - `examples/intent_contract_pair_minimal.pgy`
  - `examples/authority_contract_pair_minimal.pgy`
  - `examples/transfer_contract_pair_minimal.pgy`
- stable minimal subset
  - `examples/action_contract_inheritance_minimal.pgy`
  - `examples/intent_contract_derivation_minimal.pgy`
  - `examples/transfer_move_minimal.pgy`
  - `examples/transfer_move_typed_minimal.pgy`
  - `examples/zone_context_minimal.pgy`

- [x] **contract provenance vocabulary 고정**
  - ?료: beta closure 문서??contract provenance ???? `derived / inherited`?고정
  - 규칙: contract source ?명?서??`inferred`??? ?고, action?서 ?사?된 step clause??`inherited`, `using/transfer` ??현재 step?서 계산??clause??`derived`?른다
  - 규칙: diagnostics / AST print / docs 같? 되어??도?맞추? `inferred`???반 ???계산?나 non-contract internal analysis 문맥?만 ?긴??  - ??? contract provenance ?여 ?현, contract source wording, docs/example terminology
  - 문제: compiler type/effect inference? domain contract ?속/?생??같? 되어??이??명이 무너?  - 고정 기?:
    - domain contract??`?속 / ?생`?`inherited / derived`로만 른다
    - ?쇰컲 compiler ?섎???type/effect `inference`?먮쭔 ?④릿??  - ?뚭? 湲곗?:
    - parser/semantic diagnostics 기? 문자??고정

### P0.5 ??recoverable failure 분류/고정

- [x] **failure class inventory ?뺣━**
  - ?료: `docs/07_error_handling.md`, `docs/18_language_status.md`, `README.md` 기?으로 `recoverable failure / contract violation / internal bug` inventory??리
  - ?료: 현재 recoverable ?? ??, hard-fail ?? ??, ?속 downshift ???authority rejection ????구분
  - 규칙: runtime invariant guard? real domain rejection??같? ?패 층으??? ?음
- 현재 inventory baseline:
  - recoverable ?좎?:
    - `Result<T>` / `?`
    - `RemoteFuture<T> -> Result<T>`
    - channel timeout / non-blocking / closed state
    - world roster timeout
    - `IntentLast* / History* / Active* / Recent*`
  - hard-fail ?좎?:
    - released slot / invalid token / token permission mismatch
    - `Unwrap(...)` on `Err`, option unwrap on `None`
    - allocator / box / rc / weak invariant break
    - array / slice bounds violation
    - current runtime zone authority null-guard
      - 참고: ?건 아직 real authority rejection??아니며 invariant check?서 hard-fail ?? 쪽이 맞다
  - first-wave conversion targets:
    - future real runtime authority rejection
    - intent boundary/authority mismatch provenance at runtime
- [ ] **intent/zone/world recoverable failure baseline**
  - intent failure, authority rejection, boundary mismatch??process abort ???queryable reason/state??출
  - runtime observability? diagnostics wording??같? provenance vocabulary??렬
  - 참고: runtime propagation provenance(`epoch/cause`) baseline? ?료?본다
  - 진행: runtime zone authority invariant guard??`last_ok / zone / participant / code / reason` thread-local snapshot???기?록 ?렬되어, hard-fail guard? 별개?최소 queryable failure snapshot baseline? ?겼??  - 진행: authority failure code/reason/stderr format? `src/runtime/pgy_runtime_authority_contract.h`??격한다. inline C runtime?LLVM runtime library export 같? contract macro??용하고 `runtime-authority-contract-test-smoke` raw literal drift?차단한다
  - 진행: intent emitter??MIR `IntentAuthorizedBy` metadata?C/LLVM ?쪽?서 ?집?고, step-local approval??`pgy_zone_authority_validate_flags_export(...)`?증해 `authority:<step>` recoverable intent failure? runtime authority snapshot??같? 경로??긴??  - 진행: intent `authorized by`??concrete zone subject slot으로 ?석?며, 같? ?의 non-authority slot 는 ambiguous same-type slot mapping? semantic hard error??혔??  - 진행: concrete direct-slot participant alias??ambiguous same-type ?보보다 ?선한다. `subject slot rogue: Adventurer` 존재?면 `authorized by rogue`??concrete authority slot으로 ?히? ?전 ?보 ?운 stale ambiguity flag??무시한다
  - ?뚭?: `intent authorized participant must resolve to authority slot`, `intent authorized participant reports ambiguous authority slot`
  - ??: `dnd_tavern_campaign` example smoke multi-subject same-type zone?서 direct authority aliases?end-to-end?고정한다
  - ?뚭?: `intent_authority_snapshot_abi`, `intent_authority_snapshot`
  - ?뚭?: `authority_failure_abi`, `authority_failure_surface`, `runtime-authority-contract-test-smoke`
  - ?음: missing-zone/missing-participant ?후??richer authority mismatch/domain-boundary denial reason??같? queryable contract??장?야 한다
- [ ] **runtime authority guard downshift**
  - 현재 `pgy_zone_authority_check_export(...)`??null self/null participant invariant guard??  - ??guard ?체??hard-fail ??
  - 진행: C inline validator, LLVM runtime export, intent step-local `authorized by` validation 모두 마??authority validation 결과?같? vocabulary(`last_ok`, `zone`, `participant`, `code`, `reason`)??긴??  - 별도 real authority rejection runtime path ?기?그쪽??`recoverable authority failure` 경로??계
- [x] **hard-fail boundary 紐낆떆**
  - ?료: `README.md`? `docs/07_error_handling.md`??hard-fail boundary?명시
  - 고정 ?용: released slot, invalid token, ownership invariant break, unwrap misuse, bounds violation, runtime invariant guard??계속 panic / hard-fail territory?한다
  - 고정 ?용: recoverable authority rejection?invariant guard?같? 층으??? ?는는 을 문서 wording으로 못박??
- [ ] **projection contract diagnostics 고정**
  - ??? `refresh/publish/bind` source/target/path/field-map ?ㅽ뙣
  - 문제: projection? 되어 강점?데 ?패 ?유 ?하???먼? ?로??  - 고정 기?:
    - target slot / source slot / projection kind / field path / fix 모두 진단??되어?    - structured `Reason:` / `Fix:` formatting??source-of-truth?고정
  - ?뚭? 湲곗?:
    - semantic regression: missing source field / ambiguous path / wrong projection kind / duplicate field map
  - 진행: `projection-diagnostic-contract-test-smoke` ??4?베? 개수 진단 ?스? `Reason:` / `Fix:` / projection consumer path vocabulary?semantic regression, implementation, proof doc 기?으로 ?께 ?한??
현재 source-of-truth:
- stable example
  - `examples/projection_bind_group_minimal.pgy`
  - `examples/projection_refresh_publish_group_minimal.pgy`
- semantic regression
  - `src/test_semantic.c:test_projection_contract_diagnostics`
  - `make projection-diagnostic-contract-test-smoke`

- [x] **surface trust subset 분류 고정**
  - ??? generics, own/ref, collections, runtime observability
  - 문제: 는 것처??보이?데 ?제로는 subset?는 surface ?????뢰 ?상 ??  - 고정 기?:
    - `stable subset / explicit reject / beta-out-of-scope`?TODO/docs/diagnostic?서 같? 말로 한다
  - ?뚭? 湲곗?:
    - semantic tests? depth docs 같? subset??리킴
  - 현재 기? 문서:
    - `README.md`??`Surface trust policy`
    - `docs/18_language_status.md`
    - `docs/63_feature_depth_matrix.md`
    - `docs/64_depth_filling_roadmap.md`

현재 고정?려??baseline:
- generics
  - stable subset: exact/ability/multi-bound baseline
  - stable subset extension: default type argument actual resolution on implemented declaration/call/module-consumer paths
  - beta-out-of-scope: broader generic generalization
- own/ref
  - stable subset: classifier-backed own/ref surface on copy values + boundary-visible aggregates + movable values + slot handles
  - explicit reject: authority-bearing `Token<T>` escape/transport
  - beta-out-of-scope: arbitrary universal ownership lattice beyond current classifier/summary model
  - beta blocker: ?놁쓬
- collections
  - stable subset: `List<T>`, `Set<T>`, `HashMap<String, T>`, `HashMap<Int, T>`, `HashMap<Long, T>`, `HashMap<Bool, T>`
  - explicit reject: unsupported map key kinds
  - beta-out-of-scope: arbitrary key-universal collection contracts
- runtime observability
  - stable subset: `last / history / active / recent`
  - explicit reject: ?놁쓬
  - beta-out-of-scope: richer multi-instance timeline query? deeper failure provenance query

### P1 ???? 구조 pain point

- [ ] **declaration-side MIR-only debt 고정**
  - ??? declaration inventory / metadata helper / duplicated named-decl lookup
  - 문제: routine body??MIR??리?도 decl-side helper debt ?으?parity bug 반복??  - 고정 기?:
    - backend lookup? 공통 inventory helper??용
    - ?? debt???기??미구이 아니며 ?AST-carried decl metadata 구조 debt으로 분리?서 기록
  - ?뚭? 湲곗?:
    - LLVM/C backend helper duplication 감소
    - debt ledger? TODO ?현 ?렬
  - ?꾪솴:
    - 진행: MIR declaration emit state restore??helper ?나?묶?? role host lookup? active inventory-only 쪽으???좁아졌다
    - 진행: 조기 return 경로??`current_host_decl` / `current_func_decl` 복구 emitter 본문 중복 ???공용 restore helper???한다
    - role / party / roster / relation / effect / zone / world declaration method body??AST fallback???거??    - ?? debt??declaration inventory / naming helper / named-decl lookup??구조 ?리 쪽으?축소??    - 진행: `emit_func_decl_from_mir_named(...)` outer host restore?서 raw saved host-name fallback보다 `saved_host_decl + current_func_decl`??선 ?도??렬
    - 진행: host restore/current-host lookup??inventory?서 host decl???찾으?raw `current_*_name` ?태????? 하고 host handle??비우?록 ?렬
    - 진행: `transpiler_restore_host_context_local(...)` ?그?처??`saved_host_decl` 중심으로 축소??decl-side restore?서 raw name ?자??거
    - 현재 inventory:
      - `src/codegen/transpiler_helpers_core_b.inc`: `current_host_decl_name` ?곹깭 ?먯껜? ?쇰? host naming helper ?뺣━
      - `src/codegen/llvm_pipeline.c`: AST-carried declaration inventory瑜??대뒗 `MIRProgram` bootstrap 寃쎈줈
      - 공통 과제: current_* name ?태? ad-hoc named lookup?MIR declaration metadata query?치환
    - 理쒓렐 ?뺣━:
      - `current_field_type_name`, `current_host_method_decl`, `find_nominal_host_method_decl`??active inventory 경유 lookup??렬??      - transpiler host context 복구??`current_host_decl -> within_zone -> saved host-name inventory` ?으??렬??      - transpiler emitter hot path??direct `current_*_name` 참조??helper/restore layer ?주?축소??      - LLVM declaration helper / MIR-domain emission / expr-call builtin path??`llvm_current_host_decl_name(...)`? bind/restore helper 쪽으??동??      - LLVM `llvm_current_host_decl(...)`?????상 `current_class_name` ?조??fallback???존?? 하고 bound host handle / `within_zone`만을 truth??용??      - `llvm_pipeline.c`??nominal declaration registration?class-method enumeration??raw `decl_header->methods` 직접 ?근보다 active nominal inventory / `llvm_find_host_decl_methods_in_context(...)` 경유??동??      - `llvm_register.c`??active nominal registration??`mir->decl_headers` 직접 ?회 ???active nominal inventory 기?으로 ?렬??      - `make mir-declaration-inventory-test-smoke`?추???C/LLVM declaration/domain/nominal active inventory helper seam?pipeline/domain ?비 경로?static gate?고정한다. ??raw MIR declaration array access??owner ?일 밖에??조용??되어????한다
      - C backend `emit_program(...)`??executable metadata??`mir->has_*` / `mir_find_function_decl(...)` 직접 ?근 ???`transpiler_active_*` helper??과?도??렬한다
      - C backend `emit_program(...)`??ability/type/extern/function/intent/domain/event declaration bootstrap ?쒗쉶??direct `mir->...` array/count ?묎렐 ???`transpiler_active_inventory(...)` / `transpiler_active_externs(...)` view瑜??ъ슜?섎룄濡??뺣젹?덈떎
      - `MIRDeclMethod`??hosted method identity, routine link, signature metadata源뚯? ?닿퀬 LLVM nominal/enum prototype registration? `llvm_mir_decl_method_*` helper瑜??듯빐 ??row瑜?癒쇱? ?뚮퉬?쒕떎
      - ?? ?심 debt??LLVM pipeline??AST-carried declaration inventory bootstrap? helper/restore layer 바깥??raw host-name state ?거

- [x] **ownership vocabulary / payload cleanup 1?고정**
  - ??? semantic ownership diagnostics / payload helper family / wording drift
  - ?료:
    - `src/semantic/type_checker_ownership_boundaries.inc`??ownership helper 9종이 `DiagPayload`/`semantic_emit_payload(...)` ?턴으로 ?렬??    - semantic direct `semantic_error_with_hints(...)` ?출? ownership-boundary helper ???서 ?거??    - vocabulary 1??리:
      - `anchored handle` ??`slot handle (anchored)`
      - `movable resource handle` / `movable resource` ??`slot handle (movable)`
      - `capability-bearing` ??`authority-bearing` (ownership/domain wording 湲곗?)
    - semantic ????현재 wording 기?으로 ?시 고정??  - ?
    - `make test-semantic` ??`1872 passed, 0 failed`
    - `make test-transpile` ??`601 passed, 0 failed`
  - ?⑥? 寃?
    - P3 ?여 ?분?`boundary value (subject)` ?? 추? ?축
    - payload/helper family?ownership 바깥 semantic diagnostics????장
    - own/ref call/consumer path?서 classifier 기반 trivial copy-only semantics????게 ?용
    - destructure target binding / nested projection / helper-chain wording??consumer kind 湲곗??쇰줈 ???몃텇??
- [ ] **type-resolution DAG ?진 ?입**
  - ??? semantic type resolution / generic consumer resolution / declaration dependency scheduling
  - 문제: ?재??`resolve_type_node(...)` 중심???? ?석 + scope lookup + ad-hoc validation??주축?라, module import graph??분명???type dependency ?체??compiler-wide DAG?리되 ?는??  - 최근 진행:
    - `TypeResolutionGraph` inventory + cycle diagnostic + topo derivation? ?ㅼ젣 ?쒖꽦 ?곹깭
    - staged worklist??provider-first ?? topo ?회?고정??    - local contract / projection synthetic node??label?narrow handler??비??    - generic `default_type` / generic constraint / `where` bound??staged DAG resolver 경로???입??    - graph regression? world lifecycle / relation-effect propagation / generic consumer schedule / alias cycle provenance / generic default-bound cycle provenance / action-intent-zone-party ability consumer provenance까? ?함
    - graph validator cycle?legacy alias-resolution cycle??모두 `Contract source:` / `Reason:` / `Fix:` 구조??렬??    - 진행: type constraint bound formatter??`type_checker_type_constraint.c`??제 TU 분리 ?료
    - 진행: graph node/edge/path/cycle-format primitive??`type_checker_resolution_graph_core.c`??제 TU 분리 ?료
    - 진행: named dependency edge recorder? 즉시 cycle diagnostic 발행 경로??`type_checker_resolution_graph_core.c`??제 TU 분리 ?료
    - 진행: type-ref dependency recorder??`type_checker_resolution_graph_core.c`??동?고, `find_type_alias_decl`??cross-include dangling return-type seam??명시 ?언으로 ?리
    - 진행: type-ref collector??`type_checker_resolution_graph_collect.c`??동?고, graph core/include 경계??dangling `static void` seam???거
    - 진행: generic contract inventory / string dependency / required ability collector helpers??`type_checker_resolution_graph_collect.c`??동??declaration collector의 공통 ?존??TU 경계??격
    - 진행: top-level declaration graph registration? `type_checker_resolution_graph_collect.c`??동??inventory `.inc`?1,962 LOC까? 축소
    - 진행: local-contract graph node/dependency + zone/world/projection label formatters??`type_checker_resolution_graph_labels.c`??동??inventory `.inc`?1,835 LOC까? 축소
    - 진행: projection source resolver??`type_checker_resolution_graph_domain.c`??동하고 `find_zone_domain_slot`??internal API??격??inventory `.inc`?1,809 LOC까? 축소
    - 진행: event declaration precollector??`type_checker_resolution_graph_decl.c`??동??inventory 본체?서 declaration-kind collector???단
    - 진행: enum declaration precollector??`type_checker_resolution_graph_decl.c`??동하고 `semantic_stage_method_array`?internal API??격??inventory `.inc`?1,765 LOC까? 축소
    - 진행: ability declaration precollector? action-contract precollector??`type_checker_resolution_graph_decl.c`??동??inventory `.inc`?1,648 LOC까? 축소
    - 진행: role/class/party/roster declaration precollector??`type_checker_resolution_graph_decl.c`??동?고, relation/effect domain inventory precollector??`type_checker_resolution_graph_domain.c`??동??inventory `.inc`?1,299 LOC까? 축소
    - 진행: intent declaration precollector? world inventory precollector?각각 `type_checker_resolution_graph_decl.c`, `type_checker_resolution_graph_world.c`??동??inventory `.inc`?870 LOC까? 축소
    - 진행: zone projection field-map collector?`type_checker_resolution_graph_zone.c`?분리?고, ?? inventory body?`type_checker_resolution_graph_inventory.c`??격??inventory `.inc`??거
    - 진행: world/zone local-contract stage replay?`type_checker_resolution_stage_domain.c`?분리?고, ?? stage 본체?`type_checker_resolution_stage.c`??격??stage `.inc` ?거
    - 진행: class/extern declaration checker?`type_checker_class_decl.c`? top-level semantic orchestration??`type_checker_program.c`?분리??program `.inc`?624 LOC까? 축소
    - 진행: `ToObject` / `ToTObject` projection checker?`type_checker_builtins_projection.c`?분리??builtins nominal `.inc`?659 LOC까? 축소
    - 진행: domain helper? intent helper?각각 `type_checker_decls_domain_helpers.c`, `type_checker_intent_helpers.c`??격??semantic `.inc` 800 LOC stop condition???성하고 `make semantic-inc-size-test-smoke`??? 방?
    - 진행: C backend `transpiler_emitters_mir_inventory_ssa.inc`?3??위 slice?분리하고 `make test-transpile`, `make llvm-test-backend-compare`?parity ?? ?과
    - 진행: standalone TU ?격 ??러??dangling return-type seams? implicit helper dependency??거??`make test-all`, `make llvm-test-backend-compare` ?? ?과
    - 진행: implicit declaration / implicit int??기본 CFLAGS?서 ?러?고정되어 ?후 DAG/semantic split ?hidden helper dependency 즉시 ?패?도??렬
    - 진행: `type_resolution_intern_node` / `type_resolution_add_edge` / `type_resolution_find_path` / `type_resolution_format_cycle`??include-order static helper?서 `type_checker_internal.h` internal API??격
    - 진행: DAG stage ?에??아직 `resolve_type_node(...)`??려??legacy fallback??`PGY_TYPE_RES_STATS=1` ?계???출한다. `stage-legacy-resolve: calls/failed/suppressed_diagnostics`? `stage-legacy-family: generic_contract/signature/ability_consumer/domain_contract/alias/other` 출력?며, `make type-resolution-dag-test-smoke` graph stats, topo validation, legacy fallback inventory 존재?CI gate?고정한다
    - 진행: type-alias stage??quiet resolve ?공 결과??사?하?록 ?리???공 경로??중복 `resolve_type_node(...)` ?출???거한다. ?패 경로??기존 diagnostic fallback????한다
    - 진행: DAG edge ?? 존재는 named type-ref??generic argument??함??stage?서 `resolve_type_node(...)`??시 ?출?? 하고 graph-backed skip으로 처리한다. `stage-graph-backed: skips=N` ?계 추?하고 `type-resolution-dag-test-smoke` skip ?계 0으로 ?행?? 는 ?한??    - 진행: graph precollect TU ?에??enum methods `semantic_stage_method_array(...)`??출?던 impurity??거한다. ?제 enum method signature/contract??precollect action contract 경로로만 graph edge??집한다
    - 진행: DAG stage helper?`type_checker_resolution_stage_lookup.c` / `type_checker_resolution_stage_stats.c`?분리??`type_checker_resolution_stage.c`?895 LOC????? graph precollect, stage lookup, stage stats, stage replay owner ?일 경계?분리한다
    - 진행: generic where/default validation? `type_checker_generic_validation.c`??동한다. `type_checker_resolution_graph_*.c`? `type_checker_resolution_graph_core.inc`?????상 `resolve_type_node(...)`?직접 ?출?? ?으? `semantic-core-shape-test-smoke` ??resolver-free graph-layer 경계??한??    - 진행: graph precollect context-independent builtin type refs(`Int`, `Long`, `Float`, `Double`, `Bool`, `String`, `QubitSlot`, `Void`)?`SemanticContext.type_resolution_metadata`??기록한다. owner resolver seams????metadata?먼? 조회????recursive fallback으로 ?려간다
    - 진행: graph metadata resolver-stable constructed/anchored-handle shells(`Array<T>`, `Slice<T>`, `List<T>`, `Queue<T>`, `Set<T>`, `Box<T>`, `Rc<T>`, `Weak<T>`, `Channel<T>`, `Future<T>`, `RemoteFuture<T>`, `Token<T>`, `DeviceSlot<T>`, `HashMap<String|Int|Long|Bool, T>`, `Option<T>`, `Result<T,E>`, `Slot<T>`, `SecureSlot<T>`, `ReadView<T>`, `WriteView<T>`, `MoveToken<T>`)?materialize????한다. graph 만든 `Type` shell? metadata owned lane으로 기록하고 semantic context destroy?서 ?제한다
    - 진행: graph metadata tuple shell?event-handler/function shell??materialize한다. channel/future AST node??inner fact collect 직후 constructed shell??기록???recursive fallback?????존한다
    - 진행: `resolve_type_node(...)` wrapper ?체 metadata-first 되어, ?? explicit legacy allowlist??recursive materialization 에 DAG facts?먼? ?비한다
    - 진행: `resolve_generic_type_arg(...)`??metadata-first 조회 ??fallback으로 ?려간다. constructed builtin/generic consumer path??recursive resolver ?존 면적??줄???    - 진행: owner-local resolver seams??`semantic_type_resolution_lookup_or_materialize(...)` 공용 materializer??렴한다. resolver 구현체? central metadata materializer 밖에??직접 `resolve_type_node(...)`??출?면 `type-resolution-resolver-inventory-test-smoke` ?패한다
    - 진행: `type-resolution-dag-test-smoke` graph-backed skips?아니며 metadata entries/owned/hits, metadata materializer fallback count, zero non-alias stage legacy fallback, alias-stage split accounting???한?? 최신 local stats: `graph-backed skips=3133 metadata_entries=1877 metadata_owned=111 metadata_hits=3267 materializer_fallbacks=4135 legacy_alias=83 legacy_non_alias=0 alias_materialized=5 alias_diagnostic_fallback=78 alias_fallback_resolved=0 alias_fallback_unresolved=78`
    - 진행: DAG smoke???제 graph-backed skip/metadata entry/metadata hit/owned metadata ?순??0보다 ???보? 하고 beta floor(`skips>=3000`, `entries>=1500`, `hits>=2400`, `owned>=45`)??한?? DAG source-of-truth ?용이 ?게 ?퇴?면 CI?서 즉시 ?는??    - 진행: 중앙 metadata materializer??마??recursive fallback??`materializer_fallbacks` ?계??출하고 semantic suite ?산 cap??4135????? ??cap? ?장 방??이? ?음 DAG ?업? ??값을 계속 ????것이??    - 진행: ?? stage legacy surface??alias-only?고정한다. ?공 alias materialization?diagnostic fallback??별도 계측하고 valid alias fallback? 0으로 gate한다. ?? 78건? alias-cycle diagnostic coverage?서 ?오??unresolved fallback?며 hidden non-alias recursive resolution??아니며     - 진행: program-level symbol inventory ability declarations??predeclare한다. `type_check_ability_decl(...)`? ?기 ?신??predeclare??사?하?같? ?름???른 ability??기존처럼 duplicate diagnostic으로 처리한다. forward source order?서 generic default/where, zone authority, party role-slot ability consumer provider ?행되어???과는 regression??추?한다
    - 진행: `tests/cases/backend_compare/forward_ability_order/main.pgy`?backend compare suite??추?한다. provider-after-consumer generic default/alias/zone-authority/party-role-slot ability ordering??semantic-only 아니며 C/LLVM 출력 ?등?까 ??는 ?한??    - 진행: `tests/compare_backends.sh` 기본 ?행? `tests/cases/backend_compare/*/main.pgy` default case array??빠져 ?으??패한다. 명시 ?자 기반 targeted run? ???되, CI/default path?서 ??parity case 조용???락는 drift?차단한다. ??gate?기존 passing case 8?array builtins/inline access, slice inline access, intent observability rollback, list/map/queue get-string, try-operator result)?default C/LLVM parity suite???입한다
    - 진행: `type-resolution-resolver-inventory-test-smoke` direct resolver allowlist? ?께 metadata-first wrapper, execution/anchored-handle metadata materializer coverage?static gate?고정한다
    - 진행: `type-resolution-resolver-inventory-test-smoke` ??`semantic_type_resolution_resolve_or_fallback(...)` ?용?? 금?하고 named fallback seam 총량??0개로 고정한다. gate 출력? 현재 fallback seam count?직접 보여주며, remaining fallback? `semantic_type_resolution_lookup_or_materialize(...)` ????central escape hatch 교체 ??이??    - 진행: fallback seam gate??기존 ?한??`30?미만?면 ?패`)??debt-reduction??맞? 는 규칙으로 보고 ?거한다. ?제 0??한?growth guard????며, seam 축소??CI ?공 경로??    - 진행: `type_checker_module_contract.c`??ability contract bookkeeping? recursive fallback helper??출?? 하고 DAG metadata lookup-only seam으로 ???? ability 존재/visibility/generic arity/where provenance??ability-specific validator 계속 ?유?며, fallback seam inventory??39?서 38?감소한다
    - 진행: `type_checker_ability_fields.c`??ability `fields` requirement validation??recursive fallback helper??출?? 하고 DAG metadata lookup-only????? field contract diagnostics??ability-specific validator 계속 ?유?며, fallback seam cap? 32?서 31?감소한다
    - 진행: `type_checker_builtins_projection.c`??projection target-field resolver??recursive fallback helper??출?? 하고 DAG metadata lookup-only????? projection field diagnostics??projection validator 계속 ?유?며, fallback seam cap? 31?서 30으로 감소한다
    - 진행: `type_checker_program.c`??quiet top-level placeholder resolver??graph precollect ?후 metadata lookup-only??환한다. event/function forward placeholders recursive fallback 이 precollected DAG facts??비?면??fallback seam cap? 30?서 29?감소한다
    - 진행: `type_checker_builtins_query_domain.inc`??projection source-field resolver??recursive fallback helper??출?? 하고 DAG metadata lookup-only????? HasProjection/HasZoneProjection 계열 field diagnostics??domain query validator 계속 ?유?며, fallback seam cap? 29?서 28?감소한다
    - 진행: `type_checker_party_decl.c`? `type_checker_roster_decl.c`??shared-field type resolver??recursive fallback helper??출?? 하고 DAG metadata lookup-only????? party/roster shared field diagnostics???declaration validator 계속 ?유?며, fallback seam cap? 28?서 26으로 감소한다
    - 진행: `type_checker_ability_decl.c`??abstract method signature resolver? `type_checker_role_decl.c`??host-type resolver??recursive fallback helper??출?? 하고 DAG metadata lookup-only????? ability/role declaration diagnostics???owner validator 계속 ?유?며, fallback seam cap? 26?서 24?감소한다
    - 진행: function/action body precollector local let / with-slot annotation?아니며 expression subtree, call type args, lambda param/return/body, event subscription handler, spawn/channel/return/branch expressions까? ?라간다. ??기반으로 `type_checker_event.c`??event/lambda handler type-ref resolver?DAG metadata lookup-only????fallback seam cap? 24?서 23으로 감소한다. `type_checker_flow.c`??flow-local type resolver??DAG metadata lookup-only??? cap? 22?감소한다. `type_checker.c`??type-alias statement resolver??DAG metadata lookup-only??? cap? 21?감소한다
    - ?인???? blocker: `type_checker_program.inc`??function body param/return/domain-slot materialization seam? ?순 lookup-only????direct semantic unit path?서 graph metadata bootstrap 이 segfault 한다. ??seam? direct semantic unit bootstrap 는 null-safe diagnostic path 먼? ?요한다
    - ?인???? blocker: `type_checker_intent_decl.c`??intent participant/value/where resolver seam? ?순 lookup-only????semantic suite ?반 parallel execution path?서 segfault 한다. intent declaration? graph precollect ???direct semantic/bootstrap path? step/local binding materialization??아직 lookup-only 계약??만족?? ?으?explicit fallback seam으로 ?긴??    - ?인???? blocker: `type_checker_host_helpers.h`??host helper resolver???순 lookup-only????intent/zone authority positive path subject-slot type metadata 족으?무너진다. ??seam? zone/world/host subject-slot nominal metadata?DAG??보존?????거?야 한다
    - ?인???? blocker: `type_checker_generic_validation.c`??generic where/default validation resolver???순 lookup-only????default type argument where-bound validation positive path 깨진?? ??seam? generic default effective-arg fact? where-bound provenance?DAG metadata???린 ???거?야 한다
    - ?인???? blocker: `type_checker_generic_support.inc`??boundary type helper seam? ?순 lookup-only????`ref class` / `ref subject` escape diagnostics 150개? 빠진?? ??seam? generic/nominal boundary category fact? ref/own escape classifier DAG metadata?서 같? type category????을 ???거?야 한다
    - ?인???? blocker: `type_checker_ability_where.c`??ability where-bound resolver???순 lookup-only????generic ability multi-bound mismatch provenance ?라??`Cloneable` bound mismatch 진단 ?? 한다. ??seam? ability where-bound effective-arg / multi-bound provenance fact?DAG metadata???린 ???거?야 한다
    - ?인???? blocker: `type_checker_operator_expr.inc`??operator overload method signature resolver???순 lookup-only????semantic suite event/misc path 진입 ?후??segfault????한다. ??seam? method param/return signature metadata? operator overload candidate summary?DAG???린 ???거?야 한다
    - ?인???? blocker: `type_checker_zone_decl.c`??zone authority subject-slot type seam? ?순 lookup-only????generic ability mismatch provenance ?라진다. ??seam? zone authority generic ability fact?DAG metadata???린 ???거?야 한다
    - ?인???? blocker: `type_checker_class_decl.c`??class/vessel field resolver???순 lookup-only????vessel/subject-vessel field acceptance 깨진?? ??seam? class/vessel field nominal flavor metadata?DAG??보존?????거?야 한다
    - ?인???? blocker: `type_checker_world_decl.c`??shared/domain-slot resolver???순 lookup-only????zone/world/intent positive paths `subject slot ... requires a subject type`?무너진다. ??seam? world domain-slot subject/zone nominal materialization??DAG metadata???린 ???거?야 한다
    - ?인???? blocker: `type_checker_ownership_let.c`??let annotation resolver???순 lookup-only????direct semantic unit path?서 graph metadata 이 `ClaimSlot` annotation??되어? segfault?????고, broader program path?서??`Slot`/`ReadView`/`WriteView`/`QubitSlot`/anchored own-ref paths `<unknown>`으로 무너???한다. ??seam? direct semantic unit bootstrap 는 null-safe diagnostic path? anchored-handle constructed-type metadata coverage?같이 ?? ???거?야 한다
    - 진행: domain/intent declaration resolver??owner-local type-ref seam으로 ?렴한다. slot/shared/named domain refs? intent involves/value/where refs 각각 ?나??owner seam??공유?면??fallback seam inventory??38?서 34?감소한다
    - 진행: alias/generic-parameter helper? resolution-stage diagnostic fallback??owner-local seam으로 ?렴한다. fallback seam inventory??34?서 32?감소한다
    - 진행: zone authority participant resolver exact/qualified-tail direct slot match?먼? ?정?고, direct match 반환 ??stale ambiguity flag?한다. 같? ???subject slot???럿 되어??`authorized by rogue` ?제 `subject slot rogue: Adventurer`?concrete?게 ?히?false-positive ambiguous?되어 ?는??    - 진행: `type_checker_intent_decl.c`??participant/value/where local seam 3개는 graph metadata-first 조회 ??recursive fallback으로 ?려간다
    - 진행: `type_checker_decls_domain_helpers.c`??slot/shared/named-ref local seam 3개는 graph metadata-first 조회 ??recursive fallback으로 ?려간다
    - 진행: `type_checker_intent_helpers.c`??direct resolver ?출? `intent_helper_resolve_type_ref(...)` ?일 seam으로 ?렴한다. transfer-derived using/where, ability generic arg, role-field checks????seam??대해 ?음 DAG metadata ?환??한다
    - 진행: `type_checker_host_helpers.h`??direct resolver ?출? `host_helper_resolve_type_ref(...)` ?일 seam으로 ?렴한다. projection source fields, hosted method return/param, zone authority/domain slot checks????seam??대해 ?음 DAG metadata ?환??한다
    - 진행: `type_checker_program.c`??forward-declaration type materialization? quiet resolver seam 1개로 ?렴?고, `type_checker_program.inc`??function-body param/return/domain-slot materialization body resolver seam? graph metadata-first 조회 ??fallback으로 ?려간다
    - 진행: `type_checker_event.c`??event signature/lambda handler materialization? graph-backed metadata lookup-only??환한다. ?음 DAG slice??ownership let / zone authority / world domain-slot / ability where-bound처럼 semantic provenance ?? owner seams??    - 진행: `type_checker_world_decl.c`??shared field/domain slot materialization? `world_resolve_type_ref(...)` / `world_resolve_domain_slot_type(...)` seam으로 ?렴한다. world shared/slot checks????seam?서 graph-backed metadata?교체????한다
    - 진행: `type_checker_role_decl.c`, `type_checker_generic_contracts.h`, `type_checker_helpers_late.c`, `type_checker_expr.inc`??직접 resolver ?출??각각 role/generic-contract/late-helper/expr local seam 1개로 ?렴한다
    - 진행: `type_checker_generic_validation.c`, `type_checker_ability_where.c`, `type_checker_module_contract.c`, `type_checker_ability_decl.c`, `type_checker_class_decl.c`, `type_checker_operator_expr.inc`, `type_checker_ownership_destructure_stmt.inc`??local resolver seam으로 ?렴한다. ?? direct count????resolver 본체, 주석, 는 명시 seam한다
    - 진행: `type_checker.c`, `type_checker_ability_fields.c`, `type_checker_builtins_projection.c`, `type_checker_builtins_query_domain.inc`, `type_checker_flow.c`, `type_checker_generic_support.inc`, `type_checker_helpers_effects.inc`, `type_checker_ownership_let*.inc`, `type_checker_party_decl.c`, `type_checker_roster_decl.c`, `type_checker_zone_decl.c`???발 direct resolver ?출??local seam으로 ?렴?고, zone domain-slot seam? graph metadata-first 조회??용한다
    - ?료: `make type-resolution-resolver-inventory-test-smoke`?추?????`resolve_type_node(...)` 직접 ?출??resolver 본체/stage legacy fallback/core fallback/local seam allowlist 밖에 ?기??패?도?고정한다. `ci-linux`?도 ?결한다
    - ? 2026-04-25 local WSL/Linux `make ci-linux` full green. Windows/MSYS2 native runner????머신???으?별도 CI ?경 acceptance line으로 ??
  - 紐⑺몴:
    - import graph? 별개?`type provider -> type consumer` 그래?? 분리 구축한다
    - declaration / alias / generic default / where-bound / ability consumer / zone authority consumer瑜?DAG node/edge濡??밴꺽?쒕떎
    - namespace-only reference??declaration inventory 조회 불필?한 concrete type materialization??강제?? ?게 한다
    - cycle??generic/alias/type consumer path 湲곗??쇰줈 path-aware diagnostic?쇰줈 蹂닿퀬?쒕떎
    - incremental compile ??invalidation 범위?declaration/type dependency ?위?줄인??  - 1?구현 ?칙:
    - 湲곗〈 `resolve_type_node(...)`瑜???踰덉뿉 ?먭린?섏? ?딅뒗??    - 癒쇱? graph inventory + topo scheduling + cycle diagnostic??異붽??섍퀬, 洹??ㅼ쓬 recursive resolver瑜?graph-backed evaluator濡?移섑솚?쒕떎
    - import/module loader??DFS cycle detection?type-resolution DAG??합?? ?는??  - ?계:
    - Phase A: declaration/type provider inventory? consumer edge ?섏쭛
    - Phase B: topo evaluation + SCC/cycle diagnostic 고정
    - Phase C: generic default arg / multi-bound / ability consumer / zone authority?DAG consumer??입
    - Phase D: incremental invalidation / cache / backend-facing resolved metadata ?ъ궗??  - ?뚭? 湲곗?:
    - dependency loop diagnostic??cycle path/provenance ?온??    - graph-backed cycle?alias fallback cycle 모두 `Contract source:`??함한다
    - namespace-only reference??불필?한 full type materialization???발?? ?는??    - generic consumer/default/bound resolution??graph-backed evaluation?서??기존 semantic 계약?같? 결과?한다
    - C/LLVM compile path ?일??resolved-type metadata??사?한??    - `PGY_TYPE_RES_STATS=1`?서 stage graph-backed skip ?? legacy fallback ?출?? family breakdown, suppressed diagnostic ?? 보인?? ??값? ?? DAG migration debt??직접 ?이??겨?fallback??추??면 smoke?서 즉시 ?러?야 한다

- [x] **runtime observability baseline vs richer query 구분 고정**
  - ??? `IntentLast* / IntentHistory* / IntentActive* / IntentRecent*`, zone/world inspection
  - 문제: baseline???? ?는??문서 thin?라??면 반??surface trust?깎음
  - 고정 기?:
    - baseline observability??complete? richer timeline/provenance??open debt?분리
  - ?뚭? 湲곗?:
    - docs/board/status 문구 ?치
    - observability regression??baseline API?계속 고정

## ?료 (P0 ??즉시 ?정)

- [x] **`system()` 명령 주입 ?거** ??`_spawnvp`/`execvp`?교체, 경로 ?추? (`pgy_path_is_safe`)
- [x] **AES-256 ?구??* ??XOR ??호?FIPS 197 AES-256-CTR + HMAC-SHA256 ?증으로 교체 (?? ?존???음)
- [x] **`auto __tmp` ?쒓굅** ??`PGY_RESULT_TRY` 留ㅽ겕濡쒖뿉??GCC ?뺤옣 `auto` ?쒓굅, C11 ?명솚 (紐낆떆??????뚮씪誘명꽣)
- [x] **REPL 고정 ?일?* ??`_pgy_repl_tmp.*` ??`TMPDIR/pgy_repl_{pid}.*` (PID 기반 아니며 경로)
- [x] **`type alias` vertical slice** ??`type UserId = Int;` parser/semantic/C/LLVM lowering ?곌껐, ?ㅼ쟾 annotation/typedef 寃쎈줈 ?뺣낫

## P1 ???ㅼ쓬 ?④퀎

- [ ] **CI ?드??* ??Ubuntu + Windows 빌드 매트? ??, AddressSanitizer/UBSan, ??촘촘??smoke coverage
- [ ] **CodeQL + secret scanning ?성??* ??C/C++ 분석 모드, push protection
- [x] **CHANGELOG.md + 버전 ?책 ?립** ??SemVer, 릴리???깅 규칙
  - ?료: `CHANGELOG.md` 존재, Keep a Changelog ?맷, SemVer 명시
- [x] **SECURITY.md** ??보안 취약???보 채널, 책임 는 공개 ?책
  - ?료: `SECURITY.md` ?성 (2026-04-18). ??버전, 보고 채널, in/out scope, 공격 ?면?mitigation, advisory format ?함

## P1.5 ??되어/컴파?러 보강

- [ ] **MIR DCE statement-level ?뺤옣**
  - ?재??dead SSA/PHI ?거 + `HasState`/`ChannelLength`?pure-query stmt ?거까????작??  - ?? ?계: pure expression stmt / dead call / dead resource-op / carrier stmt????분?하? side-effect lattice 기?으로 ?거 ?책???교??  - 목표: MIR-only emitter 기?는 metadata carrier??? ?으면서??불필?한 stmt ?거 범위??힘

- [x] **IR 계층 ?계 ??* ??HIR/DIR/RIR/MIR 분리 ??성 ??
  - **DIR ?? 결정**: intent domain structure 증에 개수 (step dependency, zone binding, post-condition)
  - **RIR ?? 결정**: resource state lattice (20-state)??slot/projection/authority lifecycle 증에 ?요
  - **MIR ?? 결정**: SSA/CFG/cleanup edge??intent compensation execution path??개수
  - ~~?? 과제~~: Backend?HIR 기반 ??MIR 기반으로 ?환?야 IR ?자 ROI ?현 ??**?료**
  - 李멸퀬: Rust??AST?뭈HIR?묺IR?묹LVM 4?④퀎, Pergyra??AST?묱IR?묭IR?뭃IR?묺IR?묪ackend 6?④퀎
  - DIR? domain graph?HIR? 구조 ?라 별도 IR???는 것이 ???  - RIR 20-state lattice???순???성 ??(?재: Owned/Borrowed/Synced/Dirty/Stale/Published/Authorized ??
- [ ] **ability 기반 ?산??dispatch 고도??* ???재??`role/impl ability` 메서?에??`operator_<suffix>_<Type>` alias??성??C/LLVM???적으로 ?출는 방식. ?기?으로는 ability/vtable 기반??직접 dispatch? ???교??overload ?선?위 규칙???요
- [ ] **LLVM ?산???버로드 ?? ?스???장** ??현재 ?모는 `role IntMath for Int` 1?중심. 비교 ?산, ?함??role, enum/custom type, namespace 경로까? ?동 ?스????

## P1.58 ???? ?이브러??프??
- [x] **`use datetime;` ?ㅼ젣 stdlib module??*
- [x] **`use http;` v0.1**
  - `HttpRequest`, `HttpResponse`, `RouteSpec`
  - `OkResponse`, `ErrorResponse`, `JsonResponse`
  - intent adapter handler ?덉젣? ?곌껐
- [x] **`use storage;` v0.1**
  - `SnapshotMeta`, `SnapshotRecord`
  - `StorageSave`, `StorageLoad`, `StorageAppendLog`
  - world/session snapshot ?덉젣? ?곌껐
- [x] **`use page;` v0.1**
  - `PageRoute`, `PageAction`, `PageMessage`
  - `MountPage`, `BindAction`, `RenderSection`
  - projection surface / action binder ?덉젣? ?곌껐
- [x] **?핑??제?stdlib ?프???용 버전으로 리프??*
  - `pages/` -> `use page;`
  - `api/` -> `use http;`
  - `report/storage` -> `use storage;`

- [ ] **`pgy scaffold project`??app-infra starter 異붽?**
  - intent-first layout + `intents/ subjects/ zones/ world.pgy main.pgy`
  - optional `pages/ api/ report/` app adapter starter

## P1.58 ???? ?이브러?개선 (2026-04-06 분석)

- [ ] **stdlib page.pgy ?제 ?더?컴포?트 ?스?으??장**
  - ?재: ?순 ?이??구조 + ?더?문자??개수?  - 목표: 이 ?이?사?클(마운???마?트/?데?트), 컴포?트 ?리, ?태 ?  - ?안: `Component` abstract base, `mount()`, `render()`, `update()`, `unmount()` ?이?사?클 ??- [ ] **stdlib storage.pgy WriteFile 추상??*
  - ?재: `WriteFile` ?장 개수 직접 ?출 ???랫???존??  - 목표: Slot/Device ?터?이으로 분리 (`StorageDevice` ability)
  - ?쒖븞: `ability StorageDevice { Write(path, data) -> Result<Void, Error>; Read(path) -> Result<String, Error> }`
- [ ] **stdlib ?반 Result<T, Error> ?턴 ?용**
  - ?재: `WriteFile`, `ReadFile` ?패 ???래???성
  - 목표: 모든 I/O ?산??`Result<T, Error>` 반환
  - ?안: `?` ?산?? 조합???러 ?파 ?동??- [ ] **datetime.pgy 메서??????개선**
  - ?재: `export class LocalDate` + `export func SameDate()` ?재
  - ?안: 메서??????(`a.SameDate(b)` vs `SameDate(a, b)`) ???나??기거나 ????문서??
## IR ?이?라??
- [x] **DIR code layer ?쒖옉**
  - declaration graph
  - intent participant/step edge
  - role/ability completeness edge
- [x] **RIR code layer ?쒖옉**
  - explicit resource/projection/authority/capability/intent-policy fact
  - explicit resource op
  - scope-level normalized state summary
  - HIR-enriched branch/join `flow-block[...]` lattice summary
- [x] **MIR code layer ?쒖옉**
  - block/instruction skeleton
  - phi materialization
  - block-local SSA rename
  - instruction-level `def/use` ?쒖옉
  - rollback/invalidation exceptional CFG ?쒖옉
- [ ] **RIR lattice propagation ?화**
  - relation/effect/zone/world handle merge???작?? conditional handle invalidation?world-handoff lattice????  - conditional authority/projection invalidation fact ?장
- [ ] **MIR full SSA / flow merge**
  - block-level version map? ?쒖옉?? rename??full def-use chain/liveness ?섏??쇰줈 ?뺤옣
  - cleanup convergence root???작?? MIR-level `RIR-flow` merge? cleanup convergence policy???고도??- [ ] **MIR DCE ?장 (statement-level)**
  - dead DEF/PHI ?쒓굅瑜??섏뼱 side-effect-free STMT/unused call ?쒓굅
  - ?재??pure query builtin (`Has*`, `ChannelLength/Capacity/Space/Full/Closed`)??전 ?거 ?작
  - `unused pure let initializer` ?쒓굅??source-local/runtime-backed storage? 異⑸룎???ㅼ떆 蹂대쪟
  - dead identifier-assign ?거??loop/phi/live-out ?판???아 되어 계속 보수 보류
  - ?음 reopen 조건: value summary??block-boundary / phi provenance??용??loop-carried DEF? 진짜 dead local DEF?분리
  - user call purity??아직 보수?으?side-effect ?다?간주
  - RESOURCE_OP/CLEANUP_EDGE/abort/IO ??side-effect 蹂댁〈 洹쒖튃 紐낆떆
  - RPO 기반 liveness? 결합???거 ?확??개선
## P2.0 ??Backend MIR 기반 ?환 ???료

- [x] **emit_program()??HIR 기반 ??MIR 기반으로 ?환**
  - **?료**: `emit_func_decl_from_mir_named()` ?전 구현
  - **寃곌낵**: MIR routine ??SSA locals + CFG ??C 肄붾뱶 ?앹꽦
  - **??기능**:
    - Intent compensation (cleanup blocks)
    - SSA versioned locals (`_pgy_ssa_name_N`)
    - PHI ?드 복사 (join block 진입)
    - BRANCH ??if/else gotos
    - RESOURCE_OP ??????개수 ?출
  - **?뚯뒪??*: 428 passed, 0 failed (湲곗〈 403 passed, 5 failed)
  - **?꾪궎?띿쿂**:
    ```
    Domain IR:   Intent Recover ??policy exclusive ??step Heal ??zone main ??participant unit
    Resource IR: IntentBegin I1 ??ConflictCheck exclusive ??BindZone main ??CallAction Recover
    MIR:         bb0: conflict_check(unit) ??br !r0, bb_fail, bb1
                 bb1: call recover(unit) ??call sync_projection(main, unit)
                 bb_commit: intent_commit(I1) ??ret true
                 bb_fail: intent_abort(I1) ??ret false
    ```

## P2.1 ??LLVM 백엔??MIR 기반 ?환 ???료

- [x] **LLVM 백엔??MIR 기반 ?환 ?료**
  - `src/codegen/llvm_pipeline.c`: MIR routine ??LLVM IR 직접 ?성
  - `src/codegen/llvm_mir_emit.c`: `llvm_emit_func_from_mir()` ?전 구현
  - SSA locals, PHI nodes, branch terminators, intent compensation 모두 ??  - 기? 과 ?성: LLVM 최적???스 ?전 ?용, C/LLVM 백엔???키?처 ?일
  - C/LLVM ????MIR 기반으로 ?일 ??IR ?자 ROI ?현

## P1.55 ??되어 기능 ?장

### 기반 ????스??- [x] **?그??아니며 (enum with data)** ??`enum Shape { Circle(Int), Rect(Int, Int) }` ?이?? ?enum
  - ?료: variant payload ?싱, variant ?성?????추론, C tagged union / LLVM discriminated struct, LLVM tagged-union regression ??제 ?행
- [x] **Option<T> / None** ??"?자 비어을 ???다"???으??현. `-1` sentinel ?거
  - ?료: `Option<T>` constructed type, `Some/None`, `IsSome/IsNone/UnwrapOption`, C/LLVM lowering
  - ?료: `match opt { case Some(v): ... case None: ... }` destructuring
- [x] **?스?럭처링 (SecureSlot)** ??`let (slot, token) = ClaimSecureSlot<Int>(lvl)` ?턴 바인??  - ?료 (2026-04-19): ?서 `ClaimSlot`/`ClaimSecureSlot` 의 `<T>`????상 버리 하고 `AST_CALL.generic_args`??첨? (?반 call-site ?네??프??, ?맨이 destructuring?서 ??generic arg?SYMBOL_SLOT + SYMBOL_TOKEN ???록, MIR emit??`PgyToken_T token; PgySecureSlot_T slot = pgy_claim_secure_T(&token);` 출력, `transpiler_find_local_type_name_in_block`??바인?별 `SecureSlot<T>`/`Token<T>` 반환??MIR header??????약 ?리, SSA 맵에 self-mapping ?록으로 emission contract ?과
  - ?일: `src/parser/ast.h`, `src/parser/ast.c`, `src/parser/parser.h`, `src/parser/parser_expr.c` (?네??자 보존), `src/semantic/type_checker.c` (destructuring ?맨??, `src/codegen/transpiler_emitters_base_a.inc` (MIR-level claim emit + ssa map ?록)
  - ?뚭?: `src/test_transpile.c` "let (slot, token) = ClaimSecureSlot<T>(lvl) emits paired claim"
  - SecureSlot MIR auto-Read + claim ?큰 emit ?? 버그 ?정 (2026-04-19): (a) SSA-aware identifier 경로 `suppress_slot_auto_read` 무시?던 버그?`pgy_secure_write_Int(&pgy_read_Int(&slot),...)` 같? ?못??C 출력 ??`!ctx->suppress_slot_auto_read` ??추? + Secure 경로?서 `pgy_secure_read_*` 분기. (b) MIR DCE `AST_LET_DECL`???용 ?음으로 ?정???거?던 버그 ??`mir_stmt_has_side_effect`??추?. (c) `transpiler_emit_mir_resource_op` Claim 룰이 SecureSlot?도 `pgy_claim_secure_T()`?emit하고 ?큰? ?략?던 버그 ??`PgyToken_T anchor_token;` + `= pgy_claim_secure_T(&anchor_token)` 방식으로 ?정. (d) `Token<T>`??"claim shape"??식??MIR header pre-decl 건너?도?`transpiler_type_name_is_claim_shape` ?입 (slot-like???구별 ??auto-Read???전??Slot ?용). 결과: destructuring + ?destructuring SecureSlot 모두 E2E ?작 (`Write/Read/Release` ?함)
  - ?일: `src/compiler/mir.c` (DCE), `src/codegen/transpiler_expr_emitters.inc` (suppress ??, `src/codegen/transpiler_emitters_base_a.inc` (claim_shape 분리), `src/codegen/transpiler_emitters_base_b.inc` (MIR header 체크), `src/codegen/transpiler_helpers.h` (claim ?큰 emit), `src/parser/parser_decl.c` (class-body destructuring ?러 메시)
  - 미처? LLVM 백엔??SecureSlot destructuring (LLVM? ?? "requires explicit annotation" ?러 ??별도 ?션), class-body destructuring (`private let (slot, token) = ClaimSecureSlot()`??명확???러 메시로만 처리 ??별도 ?션)
- [x] **?플 반환 ???+ ?스?럭처링** ??`func f() -> (Int, String)` ?`let (n, s) = f()` ??  - ?료 (2026-04-19): Type ?프에 `TYPE_KIND_TUPLE` ?성??(union??`tuple.elements/element_count` ?드 + `type_create_tuple`/`type_is_tuple`/`type_tuple_arity`/`type_tuple_get_element`), AST_TYPE??`tuple_elements` ?드?`(T, U, ...)` ?현, `AST_TUPLE_LITERAL` ?규 ?드?`(a, b, ...)` ?현????  - ?서: `parse_type()`??`LPAREN` 분기??플 ???구문 처리 (?일 `(T)`??기존 `T`??원, ?`()`??`Void`, 2??상???만 ?플), `parser_parse_primary`??괄호 ?현??경로??콤마 감? ???플 리터으로 분기
  - ?맨?? `resolve_type_node`??tuple 분기 추? ??`type_create_tuple` 반환, `type_check_expression`??`AST_TUPLE_LITERAL` ?스??소 ????집, `AST_LET_DESTRUCTURE`?서 RHS tuple?면 arity ?+ positional element ????당
  - C 백엔?? `append_type_name`???플??`(T, U)`??더, `pergyra_type_to_c` `(Int, String)` ??`PgyTuple_Int_String_t`?매핑 (depth-tracking ?서), `ensure_tuple_specialization_to` `typedef struct { T0 f0; T1 f1; ... } PgyTuple_<suffix>_t;`?ctx->out??중복 이 방출, `emit_expression(AST_TUPLE_LITERAL)`??compound literal `((PgyTuple_T_U_t){.f0=..., .f1=...})` emit, AST_LET_DESTRUCTURE MIR 경로/기본 경로 ????tuple 분기?`.f0/.f1/...` ?드 추출
  - LLVM 백엔?? `ast_type_to_llvm`??tuple AST_TYPE ??literal anonymous struct `{T0, T1, ...}`, `llvm_emit_expression(AST_TUPLE_LITERAL)`??`LLVMGetUndef + InsertValue` 체인으로 집계?구성, `llvm_emit_let_destructure` struct ?드 개수 + ??드 비포?터 heuristic으로 tuple ?정 ??`ExtractValue` per-binding
  - ?뚭?: `tests/cases/backend_compare/destructure_tuple_return/main.pgy` (C/LLVM ?숈씪: `42/hello/7/11/true`), `compare_backends.sh` case ?깅줉, `test-semantic 1653 passed`, `test-transpile 584 passed`
  - ?뚯씪: `src/semantic/type_system.{h,c}`, `src/parser/ast.{h,c}`, `src/parser/parser_decl.c`, `src/parser/parser_expr.c`, `src/semantic/type_checker.{c,_helpers.inc}`, `src/codegen/transpiler.h`, `src/codegen/transpiler_helpers_core_b.inc`, `src/codegen/transpiler_expr_emitters.inc`, `src/codegen/transpiler_emitters_base_{a,b}.inc`, `src/codegen/llvm_backend.c`, `src/codegen/llvm_expr.c`, `src/codegen/llvm_stmt.c`, `src/codegen/llvm_pipeline.c`
  - ?속 ?정 (destructure + if ??: `transpiler_register_with_alias_bindings_in_block`??Claim-only ?한 ?거 ??모든 destructuring 바인??array/slice/tuple/?반 call)???름??self-mapping으로 precheck ssa_map???록. ?제 emit 경로???전??`<name>.1` 버전???름??MIR emit ?점??ssa_map??되어???용 (self-map? verifier ?과???일 ?. 결과: `let (a, b, flag) = f(); if flag { ... } else { ... }` 같? ?턴??array/tuple ????C/LLVM?서 ?작. ?일: `src/codegen/transpiler_emitters_base_a.inc` (register_with_alias_bindings_in_block)
- [ ] **sealed ability** ??구현 ?한 role???한 (`sealed ability Combatable` ??같? 모듈 ??role?impl ??
- [x] **문자??보간** ??`f"값? {x}"` ??`StringConcat(...)` series?lowering
  - ?료: lexer?서 `f"..."` ??`TOKEN_INTERPOLATED_STRING`
  - ?료: parser?서 `{expr}` ?싱, `ToString(expr)` + `+` concatenation으로 분해
  - ?료: 기존 `"${expr}"` ?거??문법???환 ??
  - ?료: 베? stable subset??`"..."`, `"""..."""`, `"${expr}"`, `f"{expr}"`, escaped f-string brace?문서??  - ?료: unmatched interpolation brace??보간?? 하고 literal text?보존?도?parser ?? 추?
  - beta-out-of-scope: nested brace matching, format specifier, multiline interpolation, custom interpolation protocol

### ?먮윭 泥섎━
- [x] **`?` ?산??* ??`Result<T>` ?러 ?동 ?파. `let val = riskyFunc()?;` ???러 ??즉시 반환
  - ?료: ?맨??? C early-return lowering, LLVM `Result<T>` ?이?웃/unwrap/early-return lowering, `pipe_and_try.pgy` C/LLVM ?행 ?  - LLVM try.err ?구??버그 ?정 (2026-04-19): `let val = Validate(x)?;` ?턴?서 let_decl??`current_ret_type`??LHS var ???i32)으로 ?시 ??하고 되어, `?`??try.err 블록??개수 return ???struct ???i32??정 ??`unreachable` emit ??????crash. `ctx->current_func_decl`?서 AST 반환 ?을 ?조대해 복구 + Err ??구??(src_err ??dst_err 개수/?인??강제 ???함)
  - ?뚯씪: `src/codegen/llvm_expr_scalar_core.h`
  - ?뚭?: `tests/cases/backend_compare/try_operator_result/main.pgy` (C/LLVM ?숈씪), `examples/pipe_and_try.pgy`

### 의 문법
- [x] **?이???산??* ??`data |> Transform |> Validate |> Persist` ?방???이???름
- [x] **defer** ??`defer Release(s)` ?ㅼ퐫??醫낅즺 ???먮룞 ?ㅽ뻾
- [x] **`let` ???추론** ??initializer 기반 기본 추론? 현재 구현??  - ?료: annotation??을 ??initializer ??으?추론
  - ?음: 문서/?면 ?시???공격?으????추론 중심으로 ?리?? 결정

### ?네??래??- [x] **?네??래??* ??`class Pair<T>` 문법 + ?맨??+ C 코드??(?형??. ?제: `examples/generic_class.pgy`

### Slot ?뚯쑀沅?紐⑤뜽
- [x] **`own`/`ref` ?유?모델 ?정 ?구현** ??move 기본, 개수 ?그?처??명시
  - ?료: `own`/`ref` ?워??(?서/?서/AST), Slot ?????move ?맨?? Clone() 명시??복사
  - `func Upload(own tex: Slot<Texture>)` ???유??전, ?본 무효
  - `func Render(ref tex: Slot<Texture>)` ??빌림, ?본 ?효
  - 문서?? `docs/22_ownership_model.md`

### Slot ?면 문법 개선 (P0 ?선?위)
- [x] **?묵??Read + ???기반 Write** ??Slot??기본 ?용 ?면???반 ?처??  - ?료: ?기 문맥?서 `Slot<T>` auto-read
  - ?료: `slot = expr` ??`Write(slot, expr)` lowering
  - ??: `Release(slot)`??계속 명시??
### Slot 理쒖쟻??(P0 ?곗꽑?쒖쐞)
- [x] **?택 ?당 최적??* ???코?? 벗어?? 는 Slot? malloc ???alloca
  - ?료: `slot_analyze_escape_flags()` (slot_analyzer.c)
  - ?료: LLVM 백엔?에??`slot_escapes == false` ??alloca ?성 (llvm_stmt.c:145-146)
  - ?료: escape analysis?non-escaping slot ?동 ?택 ?당

### View 범위 ??(리뷰 ?요 ??미결??
- [ ] **View??바이???덱??범위 ??* ???제 ?용 ?? 만들?보?결정
  - ??A: Slice 기반 ??`SliceOf(buf, 0, 1024)` ??Slot??"창문"
  - ??B: View??踰붿쐞 遺????`ViewRead(buf, offset, length)`
  - **미결?????일 I/O, ?트?크 버퍼, GPU ?스????만들?보?결정**

### 병렬/채널
- [x] **select ?체??* ???러 채널 ?먼? 비된 것을 처리

### 되어 ?성??Tier 1 ??범용 개수
- [x] **for-in 컬렉??루프** ??`for item in array { }` 배열/컬렉???회
  - ?료: Array<T>/Slice<T> 개수??(index loop lowering), ?맨??element type 추론
  - ?음: ability 기반 Iterable<T> ?로?콜 (Tier 2)
- [x] **StringSplit / StringJoin** ??문자??분리/결합 빌트???체??  - ?료: `Split(s, delim) ??Array<String>`, `Join(arr, sep) ??String`
- [x] **ToInt / ToFloat** ??문자?→?자 ??빌트??- [x] **기본 Math 빌트??* ??Sqrt, Pow, Floor, Ceil, Random 추? (기존 Abs/Min/Max + ?규 5?
- [x] **ArraySort / ArrayMap / ArrayFilter / ArrayReverse** ??고차 개수 기반 컬렉???산
  - ?료: ArraySort(arr) ??qsort, ArrayMap(arr, fn) ????배열, ArrayFilter(arr, fn) ??조건 ?터, ArrayReverse(arr) ???집?  - fn? 개수 ?름 는 ?다 (C 개수 ?인으로 lowering)
- [x] **?스?럭처링** ??`let (a, b, c) = expr` 배열/컬렉??positional 바인??  - ?료: Array<T> ???덱??기반 추출 (`result.data[0]`, `result.data[1]`, ...)
  - MIR ?합 (2026-04-19): MIR DCE `AST_LET_DESTRUCTURE` 문을 "?용 ?음"으로 ?정???거?던 버그 ?정 (`mir_stmt_has_side_effect`). ?랜?파?러 MIR emit 루프?서 destructuring??SSA-renamed ?겟으?emit, `transpiler_find_local_type_name_in_block`??AST_LET_DESTRUCTURE ?스 추???로컬 ????석 복구
  - LLVM parity (2026-04-19): `llvm_emit_statement`??AST_LET_DESTRUCTURE ?스 추? ??초기?식??struct 값으???, `ExtractValue(0)`으로 data pointer 추출, ?바인?마??`GEP+Load`??소 추출 ??`alloca+store`+`llvm_scope_declare`?로컬 ?록. `llvm_lookup_array_var`?elem_type ?석
  - ?일: `src/compiler/mir.c`, `src/codegen/transpiler_emitters_base_a.inc` (C 백엔??, `src/codegen/llvm_stmt.c` (LLVM 백엔??
  - ?뚭?: `tests/cases/backend_compare/destructure_array/main.pgy` (C/LLVM ?숈씪 異쒕젰), `examples/collection_ops.pgy` (hello/world/foo 異쒕젰)

### 메??로그래??장 (결정 ?료)
- [x] **TMP 비채??* ???네?monomorphization + ability dispatch?95% 커버. 문서: `docs/23_metaprogramming_position.md`
- [ ] **?후 코드 ?성 ?요 ??* ??컴파??????러그인 (proc_macro 모델) 는 ?스 ?성???
### 되어 ?성??Tier 2 ???사???의
- [ ] **innate ability** ??같? 모듈 ??role?impl ?용 (sealed ???innate 채택. 문서: `docs/24_visibility_model.md`)
  - ?서 ?료, ?맨?에??`innate` ?워???식 (type_checker_decls.inc 참조)
  - ?음: 모듈 경계 ?로직 ?성
- [x] **?네?constraint ?맨??* ??`where T: Comparable` ?맨???  - ?료: ?서 + ?맨???(type_checker_helpers.inc:1847)
  - ?료: Generic function where-clause constraint validation
- [x] **OR ?턴** ??`case 1 | 2 | 3:` match?서
  - ?료: lexer `TOKEN_PATTERN_OR`, parser ?싱, ?맨???  - ?료: 리터??OR ?턴 ??(`case 1 | 2 | 3:`)
  - ?한: variant destructuring OR ?턴? 아직 미???(`case .Some(v) | .None:`)
- [x] **enum 메서??* ??`enum Direction { ... func Name(self) -> String }`
  - ?료: enum body?서 `func` ?언 + `self` ?라미터?match self 본문 ?? C 컴파???- [x] **labeled break/continue** ??`outer: while { ... break outer; }`
  - ?료: ?서 (`parser.c:1270`), AST (`break_stmt.label`), ?맨??(`test_semantic.c:680,714,739`), C 코드??(`loop_break_labels[]` + `loop_continue_labels[]`)
  - ? outer label break, ????는 label 거?, continue outer 모두 ?? ?스???과
- [x] **Custom error ???* ??`Result<T, E>` where E is user type (현재 String?
  - ?료 (2026-04-18): ?증명 ?더 `PgyResult_Int_NetError` sanitize, `PGY_RESULT_DEFINE(Int_NetError, int32_t, NetError)` ?동 instantiation (`ensure_result_specialization_to` ?설), 의 매크?(`Ok_T_E`, `Err_T_E`, `IsOk_T_E`, `Unwrap_T_E`, `UnwrapOr_T_E`) ?동 ?성, Ok/Err builtin??`ctx->current_return_type`?서 suffix 추출, match pattern Ok/Err 바인??`__typeof__` 기반 ???추론
  - ?뚯씪: `src/codegen/transpiler_helpers_core_b.inc` (generic_args_to_c_suffix + ensure_result_specialization_to), `src/codegen/transpiler_expr_emitters.inc` (Ok/Err/Unwrap suffix), `src/codegen/transpiler_emitters_base_b.inc` (match __typeof__), `src/codegen/transpiler.h` (result_specs_*)
  - ?뚭?: `src/test_semantic.c` "Result<T, E> with enum error type accepts Ok/Err and match destructuring"

### ability 차별??- [x] **ability ??interface 문서??* ??ability??"?업 ?로?콜???격 조건"?며 ?롯??착됨
  - ?료: `docs/24_visibility_model.md`??`ability ??interface` ?션 추?
  - ?리 ?용: ability??nominal object??메서??집합??직접 모델링하??interface 아니며  `requires Ability`, `dyn role slot: Ability`, `zone authority requires Ability`처럼 ?업 계약/?격 조건으로 ?비는 surface을 고정
  - ?리 ?용: ability??subject/role/slot/orchestration contract? 결합?며, 구현 ?당? role impl하고 ability ?체??"무엇??구현?라"보다 "?떤 ?격으로 참여?라"??현?다??을 명시

## P1.6 ???원/???트?이??방향 고정

### 분산 ?계 결정 (2026-04-03 ?정)
- [x] **RemoteFuture `await` ??`Result<T>` 강제** ???격 ?원?????패?????스?에??강제 ?출
  - `Future<T>` (濡쒖뺄) ??await ??`T` (?ㅽ뙣 ?놁쓬)
  - `RemoteFuture<T>` (?격) ??await ??`Result<T>` (?패 ??
  - ?맨??체커 + C 코드??+ ????매크?구현 ?료
  - ?뚯뒪?? 205 semantic + 141 transpile ?듦낵
- [x] **RemoteFuture??Claim/Read/Write/Release 차단** ???격 ?원???사??Submit/Await?  - Read/Write/Release ?출 ??친절???러 메시 출력
  - "RemoteFuture does not support Read(); use 'await' to obtain Result<T>"
- [ ] **?격 Slot? Claim 이 Channel 기반 메시 ?싱?* ??분산 ???피
  - ?щ줈??World ?듭떊? `Channel<T>`留??덉슜
  - ?먭꺽 ?먯썝??Claim ?숈궗瑜??ъ슜?섎㈃ 而댄뙆???먮윭
- [x] **World 경계 = ?패 ?메??경계** ???로??World ?신? Channel?  - ?료: World ?맨??체커 (`type_check_world_decl`, type_checker_decls.inc)
  - ?료: World 코드??(C 백엔?? transpiler_helpers.h)
  - ?료: `HasZoneProjection`, `HasZoneLayer`, `HasZoneState` builtin

### Projection / Domain Query
- [x] **Projection query surface** ??`HasProjection(slotName)`으로 relation/effect/zone 문맥?서 object/tobject projection slot??sync-ready ???질의
  - ?료: semantic + C/LLVM lowering
  - World ????Slot? 로컬 (zero-cost), World 간? Channel (명시??비용)

### ???링 ???(?드? ?드?기반)
- [ ] **백엔???? 컷오??고정** ??C = reference/fallback, LLVM = optimization/mainline
  - 같? ??론을 ??백엔에 ???되, 공격??최적?? type-erased fast path??LLVM?만 집중
  - C 백엔는 MVP ?환?? ?버? ?백, ?스?래??????한
  - ??기능 추? ??"C?서??반드??최적??경로까? 구현?야 ?는?"?기본?으?`아니며 ???- [ ] **매크?조합 ?? ???* ??C 매크?monomorphization???기 ???  - ?재: `PGY_SLOT_DEFINE`, `PGY_CHANNEL_DEFINE` ????별 ?개 (?스?래???략)
  - ??? LLVM 백엔?에??type-erased 경로 (opaque ptr + vtable) 추?
  - LTO + dead code elimination으로 바이?리 비????제
- [ ] **코드???중???제 규칙** ??bifurcation trap 방?
  - ?일 기능??C/LLVM lowering???원???으?비??? ?게 공통 ????스???선
  - backend compare / smoke?계약으로 ???고, backend-specific fast path??명시?으?분리
- [ ] **Async ???당 ?버?드 감소** ??고성??분산 I/O??한 ????최적??  - ?재: `pgy_spawn` + `malloc` per task
  - ??? Arena allocator 기반 task pool, io_uring/IOCP zero-copy I/O
  - 코루???택? ?? fiber 기반 (pgy_parallel.h)
  - ?? 되어 코어? OS ?용 ??줄러?강결?하 ??- [ ] **BYOS (Bring Your Own Scheduler) 경로 ?계** ??async ??론과 ??줄러/I/O 모델 분리
  - 되어??task/future/channel ???고정
  - ?제 polling/runtime? ?랫?별 주입 ??계층으로 분리
- [ ] **ABI ?형???략** ???기 ?른 ?롯 ?의 ?네?처리
  - ?도???계: `Slot<T>` ??`SecureSlot<T>` (보안 차원 분리)
  - ?형???요 ?? `ability` vtable dispatch (Party ?스에 ?? 구현)
  - Boxing ?요 ?? `Rc<T>` + ability 조합
  - `Rc<T> + dyn ability`??explicit high-cost path?문서??  - ?경로(struct), 객체 경로(class), ?적 경로(Rc + dyn ability)??능 계약으로 구분

### 湲곗〈 ??ぉ
- [x] **Slot Protocol 고정** ??Claim/Access/Mutate/Transfer/Release 불? 계약
- [x] **Slot/View 계층 마감** ??ReadView/WriteView/MoveToken 권한 축소/?전 계층
- [ ] **?щ’??異붿긽 ?먯썝 ?몃뱾濡??쇰컲??* ???κ린?곸쑝濡?MemorySlot, DeviceSlot, SessionSlot ???먯썝 ?대옒???뺤옣
- [ ] **채널 ???강화** ??비동??출/???거/?처??름 보강
- [x] **`Future<T>`?transfer boundary?고정** ??await/recv? 같? ownership 경계
- [ ] **effect/resource capability ?기 ?입** ??`local cpu`, `secure device`, `remote` ?????과 ?스??  - ?재: derived effect mask + spawn/await/channel?서 remote 추론
  - ?재: `/// @effects ...` ?언???으?body derived effect? mismatch 진단
  - ?음: ?그?처 문법 차원???언??annotation ?면
- [ ] **?능 목표?orchestration overhead 중심으로 ?정??*

## P1.7 ???? ?일 되어로서???음 ?계

### 비용 모델 / effect
- [ ] **비용 모델 ?면??* ??"semantic unity, visible cost" ?칙
  - `local / secure / remote / device` ?원군의 비용 차이??면???러?기
- [ ] **effect system 2?계** ???언??effect ?기, mismatch 진단
  - ??료: structured comment `@effects` 기반 mismatch 진단
  - ??료: source-level `with effects ...` ?그?처 surface
  - ?⑥쓬: ???뺢탳??effect lattice, call-site contract surface

### ?위 계층 모델
- [x] **최종 문맥 계층 / ?계 ?서 분리 고정**
  - 조립 계층: `ability -> role -> party -> relation -> effect -> zone -> world`
  - ?용??facing ?계 ?서: `intent -> world -> zone -> subject`
  - ?료: `world`?최상???행/?뢰/?패 경계는 목표 ?의?문서??  - ?료: ?위 ?이으로 갈수???구속?이는 ?계 ?칙 문서??  - ?료: `relation`, `effect`, `zone` declaration keyword? 최소 `subject slot` / `object slot` surface?parser/semantic ?면???결
  - ?료: `zone -> relation/effect`, `world -> zone` 최소 조립 slot surface?parser/semantic???결
  - ?료: `relation`, `effect`??optional `for ...` header?subject endpoint/target 최소 surface??결
  - ?료: `zone`??`apply effectSlot to targetSlot` 최소 attachment surface?parser/semantic???결
  - ?료: `zone`??`link relationSlot between left, right` 최소 relation wiring surface?parser/semantic???결
  - ?료: `zone`??`detach effectSlot from targetSlot`, `unlink relationSlot between left, right` 최소 release surface?parser/semantic???결
  - ?료: `zone`??`apply/detach`, `link/unlink`?`effect/relation` declaration contract? 기본 ???arity ??으로 ?결
  - ?료: `zone` subject shape?????권장 lint 추?
  - ?료: `tobject` keyword?`struct` ?환 projection alias?추?
  - ?료: `ToObject(TargetStruct, subjectBinding)` 최소 passive projection surface?semantic/C backend???결
  - ?료: `ToTObject(TargetDto, subjectBinding)` 최소 projection surface?semantic/C backend???결
  - ?료: `relation/effect/zone`??`tobject slot` surface??결
  - ?료: `relation/effect/zone`??domain slot??optional initializer??결??`object slot view: View = ToObject(View, subject)` 같? projection wiring??직접 ?현 ?하???  - ?료: `zone`??`refresh objectSlot from subjectSlot` surface?projection 갱신 ?름??parser/semantic???결
  - ?료: `zone`??`publish dtoSlot from subjectSlot` surface?tobject projection 갱신 ?름??parser/semantic???결
  - ?료: `zone`??`maintain effectSlot on targetSlot`, `maintain relationSlot between left, right` surface???lifecycle rule??parser/semantic???결
  - ?료: `maintain` duplicate/conflict warning (`maintain` + `detach/unlink`) 추?
  - ?료: `zone`??`authority subjectSlot` surface? optional `by subjectSlot` authority annotation??parser/semantic???결
  - ?료: `authority subjectSlot requires Ability[, Ability]` ability-gated authority surface?parser/semantic???결
  - ?료: `zone`??`state name: effect ... on ...` / `state name: relation ... between ..., ...` lifecycle alias surface?parser/semantic???결
  - ?료: `zone`??`apply/link/detach/unlink/maintain stateName` shorthand?parser/semantic???결
  - ?료: `HasState(stateName)` zone query builtin??parser/semantic???결하고 C backend?서 zone state field query?lowering
  - ?료: `HasLayer(layerSlot)` zone query builtin??parser/semantic???결하고 C/LLVM backend?서 zone layer field query?lowering
  - ?료: `HasState(effectState, targetSlot)` / `HasState(relationState, leftSlot, rightSlot)` slot-aware state query?semantic???결
  - ?료: `world`??`state name: zone zoneSlot`, `activate/deactivate/maintain zoneOrState` lifecycle surface?parser/semantic???결
  - ?료: `HasZone(zoneOrState)` world query builtin??parser/semantic???결하고 C backend?서 world zone-state/active field query?lowering
  - ?료: C backend zone/world마다 sync helper??성하고 method ?후??`refresh`/`publish` projection?lifecycle flag?incremental?게 ?기??  - ?료: `relation`, `effect` declaration??C/LLVM backend?서 struct + method wrapper?codegen하고 runtime instance constructor/method path ?결??  - ?료: `zone` layer slot??C/LLVM?서 typed overlay runtime instance???하고 sync subject slot??layer endpoint/target??바인?한 ??projection sync까? ?행
  - ?료: direct `apply/link/detach/unlink`? `maintain effect/relation/state` C/LLVM zone sync?서 ?제 layer/state propagation으로 ?결??  - ?료: zone embedded overlay projection read (`self.poison.view.hp`, `self.trust.packet.name`) LLVM runtime smoke?증됨
  - ?료: `world` `HasZoneProjection(zoneSlot, projectionSlot)` / `HasZoneLayer(zoneSlot, layerSlot)` / `HasZoneState(zoneSlot, stateName)`?embedded zone runtime flag?직접 질의?????음
  - ?료: `ability/role/party/relation/effect/zone/roster/world` ?체 구현
  - ?료: `world` `state name: all zoneOrState[, ...]` / `state name: any zoneOrState[, ...]`??서 ?언??zone/state alias?최소 조합 contract??성
  - ?⑥쓬: richer world-level runtime semantics, ??源딆? cross-layer propagation policy

### 議댁옱濡?紐⑤뜽
- [x] **intent-first ?계 ?/ subject-core host ?분리 고정**
  - ?료: ?용??facing ?계 ?서??`intent -> world -> zone -> subject`?문서??  - ?료: `subject = ?태? identity??주체 ???? host/naming/lowering 축으??정??문서??  - ?료: `subject`? `class`?으로 ?른 nominal flavor?분리하고 ??론도 1?분기
  - ?료: legacy host-profile surface??거하고 `subject`/`object`/`intent` 중심으로 ?리
  - ?료: `entity`??코어 되어 존재론에 ?? 하고 ?레?워???메??되어??긴하고 문서??  - ?료: `object`??intent??작?? 는 passive state target?라?문서??  - ?료: `tobject`??object???? 경계??축약 ?영?라?문서??  - ?료: `subject`, `class`, `struct`, `object`, `tobject` declaration flavor?parser AST??분리 기록
  - ?료: `subject slot`?`ToObject` / `ToTObject` source `subject` host?받도?semantic 분기
  - ?료: `object` keyword alias?parser/LSP surface??반영
  - ?료: `object`?passive state/value ?식?로, `tobject`???좁? projection/value ?식으로 ?리하고 helper method??용
  - ?료: `vessel` declaration?`subject` ?? `vessel` field surface 추?
  - ?료: `subject` ?용 `action` declaration?최소 clause (`requires/within/causes/authorized by`) parser/semantic ?결
  - ?료: `subject` 의 legacy `func` ?거, `action` only ?책으로 ?격
  - ?료: `role`/`party`/`authority`?subject-core host 축으???강하??한
  - ?료: C/LLVM method lowering?서 `subject=self-cell`, `class=value self` 1?분기
  - ?료: legacy host-profile surface??거하고 ??규칙??`subject`???합
  - ?료: `subject` ?일 host surface??일
  - ?료: standalone host-profile surface ??
  - ?료: object?effect/relation target으로 semantic/C/LLVM???결
  - ?료: domain-local `refresh` / `publish` source?subject/object까? ?장하고 tobject source??금?
  - ?료: relation/projection 중심 surface 고정

### 문서 / ?????렬
- [ ] **BSD (Allman) canonical style ?면 고정**
  - 문서/?제/scaffold/formatter 출력? BSD 기?으로 ?일
  - K&R? parser compatibility로만 ?기?canonical surface로는 취급?? ?음
- [x] **문서 ?제 ?시 ?서 강제**
  - ?료: README entrypoint? ?심 ?계 문서?서 ?제 대해 ?서?`intent -> world -> zone -> subject`?명시
  - 기? 문서: `README.md`, `docs/00_vision.md`, `docs/01_intent_first_design.md`, `docs/22_class_object_model.md`
  - 규칙: `subject`??core host??명?되, ?계???축으?르치 ?음
  - 규칙: compile-order? teaching-order?분리?서 명시

### slot 권한 / ?원??장
- [ ] **slot 권한 모델 고도??* ??공유 ?기 vs ?점 ?기, capability narrowing
- [ ] **?제 ?원??장** ??SessionSlot, ChannelSlot, RemoteJob 고도??- [x] **subject/class/object model 구현 ?렬**
  - ?료: subject direct copy/plain value parameter/return 금?, positional constructor
  - ?료: C/LLVM lowering 1?분기 (`subject=self-cell`, `class=value self`)
  - ?료: legacy host-profile??`subject` 규칙으로 ?합
  - ?료: `subject` ?일 host surface??일
  - ?료: plain/secure `Slot<subject>` local object-cell anchor ??  - ?료: `own/ref Slot<subject-host>` / `SecureSlot<subject-host>` 개수 경계 ?달??semantic + C/LLVM backend??반영
  - ?료: `Box<class>` explicit handle surface (`Box`, `BoxGet`, `BoxSet`, `BoxDrop`, `BoxIsValid`)
  - ?료: richer object-handle cell propagation

### orchestration ?성??- [ ] **???트?이??모델 강화** ??select 공정?? timeout, cancellation, backpressure
  - ??료: `TryRecv/RecvTimeout -> Option<T>`, `TrySend/SendTimeout -> Bool`
  - ??료: `TrySendStatus/SendTimeoutStatus -> Option<Bool>`?full/timeout vs closed?값으?구분
  - ??료: `ChannelLength/ChannelCapacity/ChannelSpace -> Int`, `ChannelFull/ChannelClosed -> Bool`
  - ??료: `select` round-robin ?작 ?덱??fairness
  - ??료: `Cancel(task)` / `IsCancelled()` cooperative cancellation
  - ??료: spawned descendant cancellation propagation
  - 현재 ?한: movable resource channel??non-blocking/timeout transfer??미???  - 현재 ?한: pressure observation? ?하?bounded policy/backpressure protocol? 아직 미구??  - 현재 ?한: preemptive cancellation, blocked thread task interruption, structured cancellation scope/lattice??미???- [x] **async/await runtime 고도??* ??POSIX ucontext + Windows Fiber 기반 coroutine
- [ ] **Windows coroutine ?고정**

### ?대쭅 / ?쒖?硫?- [ ] **stable stdlib surface ?ш퀬??*
- [ ] **?링 ?계 진입** ??formatter, LSP 진단 ?질
- [x] **ontology-first scaffold ?뺣젹**
  - ?료: `pgy scaffold` help?`subject/class/object/tobject` ?선 분기??렬
  - ?료: `class` scaffold kind 추?
  - ?료: `project/simulator` scaffold `subject` `class`??유하고 `object/tobject`??영는 starter shape??성
  - ?료: `project` scaffold intent-first layout(`intents/`, `subjects/`, `zones/`, `world.pgy`, `main.pgy`)???제??성
  - ?료: `pgy new` `intent-first` / `class-first` / `projection-first` starter??택?게 ?? ??  - ?료: `pgy new` / scaffold output??ontology decision guide file 별도 ?성 ??  - ?료: intent-first project guide 문서??scaffold output??같이 ?성?? ??    - `intents/`??로?트 table-of-contents??명는 guide ?함
    - intent declaration???요??subject/zone/ability/effect TODO???는 workflow ?시 ?함
  - ?료: intent runtime follow-up
    - rollback policy瑜?current reverse-order `compensate` beyond v1濡??뺤옣?섍린
    - intent??cross-world transfer / identity handoff semantics ?계 ?구현
    - current last-intent typed history瑜?trace id / stream / multi-instance observability濡??뺤옣?섍린

### ????로그램
- [ ] **????플리??션 3?* ???종 ?원 ?이?라?? secure+device+channel, slot/orchestration 철학 증명

## P1.85 ??게임 ?레?워??계층

- [ ] **게임 ?레?워???이브러?경계 고정**
  - ?칙: `entity/object pool`? 되어 코어 기능??아니며 `use pool;` 같? 게임/???이브러?계층으로 한다
  - ?칙: `encounter/turn/state machine`, `strategy/AI`, `content tables`???일?게 코어 문법??아니며 ?레?워??surface??는??  - ?칙: ??계층? ?도메인 ?이브러리보???generic pattern library + domain injection?으??의한다
  - ?유: 코어 되어??`subject / vessel / object / tobject / relation / effect / zone / world / Slot<T>` ??론을 ???고, ?규모 게임 ?계???의 library/DSL 계층으로 ?리??이 ?장과 ?명이 ??좋다
  - 목표: ?게을 만들 ??는 코어 되어?? ?게을 ?제?만드???레?워?? 분리
- [ ] **寃뚯엫 stdlib/use surface 珥덉븞**
  - ?보: `use pool;`, `use fsm;`, `use encounter;`, `use strategy;`, `use tables;`
  - 방향: pool/fsm/strategy/table? `.pgy` 는 stdlib 모듈??공?고, 되어 ?워으로 ?격?? ?는??  - 방향: `Pool<T>`, `StateMachine<TState, TEvent>`, `StrategyTable<TContext, TChoice>`, `WeightedTable<T>`처럼 generic-first naming???선한다
  - 방향: GOF 기초 ?턴??inheritance/object graph 아니며 Pergyra host 기?으로 번역한다
    - `singleton` -> contextual runtime registry / host-local shared state
    - `factory` -> staged template/spec builder
    - `strategy` -> policy card / policy table + function injection
    - `state` -> explicit FSM / transition rule + context application
    - `observer` -> relay bundle / sink spec / report sink / event bus
  - 방향: generic pattern library??static spec/table만이 아니며 function-typed picker/resolver 주입??기본 ?면으로 ?함한다
    - ?? `Picker<TInput, TChoice>`
    - ?? `Resolver<TContext, TResult>`
    - ?? `StrategyApply(context, AggressivePolicy)`
  - 현재 ?태: `data/card/table` 경로???정, custom function injection??V1 ?면???라??  - 현재 ?략 ?턴???정 ?계:
    - `StrategyCard`
    - `StrategyContext`
    - `ApplyStrategy(card, context)`
  - ?번 ?제 기? ?이브러리화 ?보:
    - `use strategy;`
      - `WeaponCard` / `CombatStrategyCard`
      - `WeaponFactory<TClass>` ?먮뒗 `LoadoutTable<TArchetype>`
      - `StrategyTable<TContext, TChoice>`
      - `ActionTextFactory<TContext>` / `EffectTextFactory<TContext>`
    - `use tables;`
      - `SceneChoiceCard`
      - `CompanionEventCard`
      - `BossPhaseCard`
      - `WeightedTable<T>`
      - `ChoiceTable<TState, TOption>`
    - `use encounter;`
      - `EncounterStateMachine<TState, TEvent>`
      - `TurnLoop<TActor, TAction>`
      - `BossPhaseMachine<TPhase>`
      - `ResolutionLedger<TSnapshot>`
    - `use report;`
      - transcript accumulator
      - exact report writer
      - stdout/results dual sink
    - `use campaign;`
      - scripted / random / player mode runner
      - input script playback
      - seeded choice resolver
- [ ] **GOF 湲곗큹 ?⑦꽩??Pergyra??pattern catalog濡??뺣━**
  - 기? 문서: `docs/31_gof_pattern_catalog.md`
  - 湲곗? ?덉젣: `examples/pattern_library_basics/`
  - 목표: ?통 OOP ?턴 ?름?????더?도 ?제 구현 shape??`subject / vessel / shared / spec / card / relay`??정??  - 비목?? inheritance / `super` / hidden callback graph??턴 구현??기본값으?채택?? ?음
- [ ] **DND/campaign ?나리오?게임 ?레?워??증장으로 ?용**
  - `dnd_tavern_campaign`?기?으로 pool/fsm/strategy/table???제?충분?? ?  - language core 족이 아니며 framework layer 족인 계속 분리?서 기록
  - 금까 뽑힌 ?제 ?턴:
    - ?소/?면 진입 ?토?(`OpenTavernCampaign`)
    - 게임 ?태 머신 (`tavern -> floor1 -> floor2 -> floor3 -> dragon -> epilogue`)
    - ?좏깮 ?댁꽍湲?(`scripted` / `random` / `player`)
    - ?면 카드 / ?료 반응 카드 / 보스 ?이?카드
    - ?꾪닾 loadout/strategy 移대뱶
    - transcript-first report writer
  - ?ㅼ쓬 紐⑺몴:
    - ???턴을 `examples/` ?용 코드 아니며 `use` ?이브러??보??구??    - `world.pgy`??orchestration 을 줄이?encounter/strategy/report 계층으로 분리

## P1.8 ??硫???寃?
- [ ] **공통 UI IR 고정** ??Kotlin/Android 개별 백엔?보??먼?, 모든 ?랫이 공유는 scene/projection UI IR???의
  - 목적: native / web / mobile??같? UI ??론과 projection ?름??공유?게 ??  - ?칙: 기술 기반? Qt 방향(native shell / render loop), ?언 철학? WPF??projection/binding, 최종 ?체?? Pergyra scene/projection UI
  - 踰붿쐞: `Window`, `Scene`, `Node`, `Layout`, `DrawCommand`, `InputEvent`, `ProjectionBinding`, `DirtyScope`
  - ?칙: `subject`?직접 ?면??그리 하고 `object` / `tobject` / projection surface?UI ?비 ?면으로 ?용
  - ?칙: `zone` / `world` state? projection dirty sync UI IR??갱신 계약????  - ?서: UI IR 고정 ??native backend 1???JS/web backend 1??????mobile shell / Kotlin ?요???평
  - 비목?? ?랫?별 UI ???Qt widget tree, WPF object model, Android View/Compose semantics)??코어 되어??직접 이 ?음
- [~] **JavaScript 백엔??* ??`.pgy ??JS` ?으?브라??/Node.js ?행 ??  - ?료: 코어 ??론? inheritance/super 이 ???고, JS lowering? delegation/composition 중심으로 간다???책 초안 문서??  - ?료: Kotlin backend보다 공통 UI IR???선?라???플?폼 ?책 문서??  - ?음: JS IR/lowering shape, runtime shim, interop surface (`extern js`) ?계
- [ ] **mobile shell ?략** ??Android/iOS???선 공통 UI IR consumer??근
  - ?칙: 초기 mobile ??? JS/web-compatible UI backend 는 native shell bridge??선 ??  - ?음: Android ?용 Kotlin backend??공통 UI IR + web/native backend ????요을 ?평
- [ ] **WebAssembly ?寃?* ??LLVM wasm32 backend ?쒖슜

## P1.9 ??AI-first ?명봽??(2026-04-19 positioning ?뺤젙)

**맥락**: 경쟁 ??? C#/Java ??Rust 이 ?치?고, 1??용는 frontier LLM(Claude ????주도 + ?간??리뷰/?정는 ?크?로. "AI ?성 ??컴파?러/?스?? ????간??리뷰"??loop????트?게 ?아??것이 positioning ?심.

현재 ?도??게 갖춰?AI-friendly ?프??
- backend-compare ?? (C/LLVM 출력 ?? ??AI self-verification loop ?네??- 2000+ test suite + ?모??체인 ???성?즉시 ??한 규모
- Result-first + throw 금? ??AI stack trace보다 ErrorCode enum 분기 ??
- 구조??주석 (WHAT/WHY/ALT/NEXT/EFFECTS/INVARIANTS/RETURNS/THROWS) ??prompt-as-code, ?도 보존

족하?채워?????

- [ ] **Language Reference Spec 문서** ??현재 `docs/`???계 ??(?사결정 ?름 기록). AI?게 ?확??????공?려?"??되어??보장"????문서???리?야 ??  - ?용: ????스??규칙 / Slot ?유?계약 / effect subsumption / intent rollback ?? / Result ?파 규칙 / MIR 계약
  - ?태: ?일 ?일 (~2000-5000?, in-context???번에 로드 ??  - 목적: "Claude Pergyra 코드????션?서 ?성????reference??용 ?? ??
  - 현재 `docs/`? ?른 ?? ????"???렇?결정?는", spec? "현재 되어 무엇??보장?는"
- [~] **AI-parseable 구조???러 메시** ??현재 진단? ?????현. AI?? 기계 ?독 ?한 구조???드 ?요
  - ?재: `MIR contract breach in Main at line 0: unresolved identifier 'flag' (expected SSA-mapped local)`
  - 紐⑺몴 ?뺥깭 (?덉떆):
    ```json
    {
      "severity": "error",
      "stage": "MIR_validation",
      "code": "PGY_MIR_UNRESOLVED_IDENT",
      "location": {"file": "main.pgy", "line": 7, "column": 8},
      "summary": "destructuring binding 'flag' is not SSA-mapped at use site",
      "cause_ir": "a.1 DEF is emitted in block 0 but not propagated to branch-consumer block via ssa_entry_values",
      "fix_source": "ensure destructure binding is referenced within the same block as the destructure, or use let_decl with explicit type to trigger SSA renaming",
      "related_rules": ["MIR.SSA.entry_values", "destructure.binding"]
    }
    ```
  - `--error-format=json` ?뚮옒洹몃줈 ?좉?, ?멸컙?⑹? 湲곗〈 ?뺤떇 ?좎?
  - ??? compile, semantic, MIR/LLVM IR ?계 ?체
  - 1?증분 ?료 (2026-04-19):
    - `DriverFlags.diag_format` + `--error-format=json|text` CLI ?뚮옒洹?異붽? (`src/pgy_driver.c`, `src/compiler/driver_app.h`)
    - `semantic_result_print_json` ??semantic 진단??JSON 배열?방출 (severity/stage/location/message ?드, RFC 8259 ???스?프)
    - `driver_emit_single_diag_json` ???일 ?러 JSON 방출 ?퍼 (module_load / backend_c_emit / backend_c_native / backend_llvm_emit / backend_llvm_native ?계 커버)
    - stage ?쒓렇: `semantic` / `module_load` / `backend_c_emit` / `backend_c_native` / `backend_llvm_emit` / `backend_llvm_native`
    - ?공 ??`[]` (?배열), ?패 ??`[{...}]` ???출는 ?? JSON 기? ??    - ??: `tests/diagnostics_json_smoke.sh` (Python ?서?shape ? 3 ?스: semantic / parse / success)
    - 寃利? PowerShell濡?3 耳?댁뒪 紐⑤몢 ?뺤긽 ?숈옉 ?뺤씤 (1668 semantic + 601 transpile ?뚭? pass)
  - 2?증분 ?료 (2026-04-19):
    - `Diagnostic` 구조체에 `code` ?드 추? (non-owning `const char*`, ?적 문자??리터??보?) ??`src/semantic/type_checker.h`
    - `semantic_error_code` / `semantic_warning_code` ?규 variant ??코드 ?자 받아 diagnostic??되어?(?거??`semantic_error` ??그??NULL 코드??작, ???일 ?이??중복 emit ??코드 ?으??그?이??
    - JSON 출력??`"code"` ?드 ?택???함 (NULL?면 ?략 ???환????)
    - parser stage 분리: module_load msg `"parse error in"`으로 ?작?면 `"stage":"parse"`, ???`"module_load"`
    - 초기 코드 ???이??(6?:
      - `PGY_SEM_TYPE_MISMATCH` (assignment)
      - `PGY_SEM_BINOP_TYPE_MISMATCH`
      - `PGY_SEM_UNKNOWN_TYPE`
      - `PGY_SEM_UNDEFINED_SYMBOL` (identifier / member 3 ?이??
      - `PGY_SEM_INFER_COLLECTION` / `PGY_SEM_INFER_GENERIC` / `PGY_SEM_INFER_REQUIRED`
    - smoke test ?뺤옣: `code == "PGY_SEM_TYPE_MISMATCH"` 寃利?+ `stage == "parse"` 寃利?(`tests/diagnostics_json_smoke.sh`)
    - ?뚭?: 1688 semantic + 601 transpile, 0 failed
  - 3?증분 ?료 (2026-04-19):
    - Slot/ownership/parallel/effect 계열 코드 9?추?:
      - `PGY_SEM_SLOT_RELEASED` (method dispatch 4 ?이??+ builtin Read/Write 2 ?이??
      - `PGY_SEM_RELEASE_REQUIRES_OWNER`
      - `PGY_SEM_SLOT_DOUBLE_RELEASE` (method + builtin Release 2 ?이??
      - `PGY_SEM_VIEW_KIND_MISMATCH` (ReadView write / WriteView read)
      - `PGY_SEM_MOVE_TOKEN_MISUSE` (read/write through MoveToken)
      - `PGY_SEM_MOVE_FROM_RELEASED` (let/call/builtin 3 ?이??
      - `PGY_SEM_PARALLEL_SLOT_CONFLICT` (error: mutate-mutate across tasks)
      - `PGY_SEM_PARALLEL_SLOT_RACE_RISK` (warning: read-mutate across tasks)
      - `PGY_SEM_EFFECT_CONFLICT` (warning: effect class 異⑸룎)
    - `docs/72_diagnostic_codes.md` 카탈로그 문서 ?규 ??16?코드 ??/?인/교정 방법, AI ?우???드, ?후 ?장 ?드 문서??    - smoke test ?장: `PGY_SEM_SLOT_RELEASED` 감? ?스 추?
    - ?ъ슜??湲곗뿬: `semantic_error_code` / `semantic_warning_code` ?좎뼵??`PGY_PRINTF_LIKE` ?띿꽦 異붽? (clang/gcc format 寃쎄퀬 泥댄겕)
    - ?뚭?: 1694 semantic + 601 transpile, 0 failed
    - 현재 ?16??정 코드, ~25 ?이??커버. ?머 ~460 ?이는 4? 증분 ???  - 4?증분 ?료 (2026-04-19):
    - `CompilerResult.error_code` / `TranspileResult.error_code` / `LLVMGenResult.error_code` ?드 추? (모두 owning strdup, destroy?서 free)
    - `TranspilerCtx.backend_error_code` / `LLVMGenCtx.error_code` non-owning `const char *` (?뺤쟻 literal留?
    - ?좉퇋 setter variants: `transpiler_set_backend_error_with_code` / `llvm_set_error_with_code` / `llvm_set_error_at_with_code` (?덇굅??setter??code=NULL 寃쎈줈濡??좎?)
    - `driver_emit_single_diag_json_with_code(stage, code, message)` ??JSON??code ?드 ?택???함
    - `driver_route_stage(default_stage, code)` ??prefix whitelist (`PGY_SEM_`/`PGY_MIR_`/`PGY_LLVM_`/`PGY_PARSE_`). 紐⑤Ⅴ??prefix??default_stage ?좎?
    - Runner ?데?트: `c_runner.c` (2 ?이?? + `llvm_runner.c` (2 ?이?? ??기존 ?출??`_with_code` + `driver_route_stage`?교체
    - MIR/LLVM 肄붾뱶 5醫??좉퇋:
      - `PGY_MIR_UNRESOLVED_LOCAL` ??branch terminator??identifier SSA 매핑 ?음
      - `PGY_MIR_TOPOLOGY_INVALID` ??MIR routine ?락 / kind 불일?/ AST ?음
      - `PGY_MIR_SIGNATURE_UNSUPPORTED` ?????되??개수 ?그?처
      - `PGY_MIR_SSA_LIMIT` ??SSA local 4096 珥덇낵
      - `PGY_MIR_INTENT_CARRIER_MISSING` ??intent step metadata ?락 (C/LLVM 공통, 21 ?이???괄 ?그?이??
      - `PGY_LLVM_SPEC_LIMIT` ??Result\<T,E\> ?뱀닔???쒕룄(MAX_LLVM_RESULT_SPECS=32) 珥덇낵
    - 카탈로그 ?장: `docs/72_diagnostic_codes.md`??"MIR Contract" ?션 5??트?+ "LLVM Backend" ?션 1??트?    - smoke test ?장: 33?Result\<Int, E*\> 개수으로 `PGY_LLVM_SPEC_LIMIT` + `stage=llvm_codegen` ?(`tests/diagnostics_json_smoke.sh`)
    - 寃利? `[{"severity":"error","stage":"llvm_codegen","code":"PGY_LLVM_SPEC_LIMIT",...}]` end-to-end ?뺤씤
    - ??: 1694 semantic + 601 transpile, 0 failed (?거??경로 무손??
    - 현재 ?22??정 코드 (`PGY_SEM_*` 16 + `PGY_MIR_*` 5 + `PGY_LLVM_*` 1), ~50 ?이??커버. `mir_validation` / `llvm_codegen` stage  기존 `backend_*_native`? 분리??  - ?? ?업 (5?증분 ?보):
    - intent/zone/world / class/ability ??`PGY_SEM_*` 코드 ?진????(?머 ~460 semantic ?이??
    - LLVM 추? 코드: `PGY_LLVM_TYPE_UNSUPPORTED`, `PGY_LLVM_RUNTIME_MISSING`, `PGY_LLVM_OOM` (개별 ?이???그?이??
    - `cause_ir` / `fix_source` ?드 ??현재 message? MIR/IR ?벨 ?인 + ?스 ?벨 교정 ?인??분리??AI 구분 ?하?    - parser ?벨 코드 (`PGY_PARSE_*` prefix ?약?? ??parser error ?적??리팩???요
    - `related_rules` ?드 ??Language Reference Spec ?후 ?결
- [ ] **In-context example corpus ?레?션** ??GitHub??Pergyra 코드 0? ?련 ?이???? in-context examples?보완
  - `docs/ai_prompt_bundle/` ?렉?리?????벨??번들 ?
    - `minimal.md` ??되어 ?심?(~20KB)
    - `standard.md` ??core + stdlib + 5??턴 ?제 (~100KB)
    - `complete.md` ????+ ?체 examples + reference spec (~500KB-1MB)
  - ?번들? "??번들만으????션?서 AI Pergyra 코드??뢰???게 ?성 ?한"??기??로
  - ?략??결정: 1?audience??frontier 모델(Claude Opus, Sonnet) ?용?? ?형/? 모델? 2?- [ ] **AI iteration-friendly 빌드 ?체??* ??빠른 컴파??+ 기계 ?독 출력 + LSP 진단
  - 증분 컴파????현재 ?일 TU??체 빌드. module ?위 증분으로 ?환
  - ?스??결과 JSON 출력 ??현재 stdout ?????식. AI ?싱???음 ?션 결정????는 JSON 모드
  - LSP 진단 기계 ?독 ????의 구조???러 메시? 공유 ?맷
  - backend-compare ?패 ??diff?구조????현재 unified diff. AI "?느 개수???번째 stdout ?인???름"??바로 ?? ?한 ?맷
  - ?? 기반 ?음 (`src/lsp/` ?렉?리, `tests/compare_backends.sh` 구조)

**?공 기?**: Frontier 모델??Pergyra spec bundle??in-context??고, 비자명한 비즈?스 로직 (?? 결제 + 멱등??+ ?시???책) 구현??one-shot??깝게 ?성?????음. 컴파???스???패 ??구조???러로????기 교정 루프 ~3???내 ?렴.

## P2 ??배포 ?작 ??
- [ ] **문서-구현 ?기??* ???스????기능 범위 ?치
- [ ] **SBOM (SPDX) + provenance (SLSA)** ??공급??명??- [ ] **릴리???티?트** ???명??바이?리, 체크?? ?치 ?크립트
- [ ] **3rd-party NOTICE** ??OpenSSL/LLVM/pthread ?이?스 ?리

## IR ?이?라???구??
- [x] **컴파?러 계약 고정** ??`HIR/DIR/RIR/MIR`, resource lattice, intent compensation, projection sync, authority/capability?`docs/37_compiler_contracts.md`??고정

- [~] **DIR (Domain IR)** ??declaration graph / intent step graph ?쒖옉
  - ?료: `src/compiler/dir.h`, `src/compiler/dir.c`, `pgy --dir`, `test-dir`
  - ?료: intent participant/type edge, step zone/ability/authority/effect edge, step predecessor dependency
  - ?료: role/ability completeness edge, missing-ability-method edge
  - ?⑥쓬: richer zone/world membership graph
- [~] **RIR (Resource IR)** ??slot/resource/authority/lifecycle ????용 계층
  - 踰붿쐞: `Slot`, `SecureSlot`, `DeviceSlot`, projection validity, authority, effect/relation lifecycle, intent compensation resource edge
  - ?료: `src/compiler/rir.h`, `src/compiler/rir.c`, `pgy --rir`, `test-rir`
  - ?료: scope?normalized state summary (`initial_state`, `final_state`, `last_op`, `transition error`)
  - ?료: relation/effect layer slot? world zone slot??resource fact?materialize
  - 출력: ?순 map??아니며 `Resource Graph + Transfer Ops + Static Ownership Facts`
  - explicit op ?뺢퇋??
    - `Claim/Read/Write/Release`
    - `Move/BorrowRead/BorrowWrite`
    - `ProjectRefresh/ProjectPublish`
    - `AttachEffect/DetachEffect`
    - `LinkRelation/UnlinkRelation`
    - `Authorize/AwaitRemote`
    - `CommitIntent/AbortIntent/CompensateIntentStep`
  - state lattice 珥덉븞:
    - `Uninit`
    - `Owned`
    - `BorrowedRead`
    - `BorrowedWrite`
    - `Moved`
    - `Released`
    - `Invalid`
    - `Measured`
    - `RemotePending`
  - CFG ?섏〈 branch/join/loop/phi merge??MIR濡??댁썡
- [~] **MIR (Machine / Execution IR)** ??CFG/SSA/liveness/optimization 계층
  - 踰붿쐞: basic block, explicit instruction, phi, liveness, CFG-dependent resource merge, dead code elimination
  - ?료: `src/compiler/mir.h`, `src/compiler/mir.c`, `pgy --mir`, `test-mir`
  - ?료: HIR CFG -> MIR block bridge
  - ?료: RIR op -> MIR instruction bridge
  - ?료: intent cleanup block skeleton
  - ?료: phi materialization + incoming predecessor value list
  - ?료: block-local SSA rename skeleton
  - ?료: intent cleanup successor edge skeleton
  - ?요: `RIR-flow` merge ?책
  - ?요: richer phi merge policy
  - ?요: cleanup / rollback / detach-invalidation edge 고도??## Progress Log ??2026-04-24 Parser/Lexer Diagnostic Routing

- ?료: parser/lexer diagnostic routing 1?gate??았??
- 구현: `parser_error`??`PGY_PARSE_SYNTAX`, `parse:unexpected_token`, `check-syntax`?`Code:` / `Reason:` / `Fix:` ?면으로 출력한다.
- 구현: lexer error token? `PGY_LEX_INVALID_TOKEN`, `lex:invalid_token`, `remove-or-escape-character`?같? ?면으로 출력한다.
- 寃利? `make parser-lexer-diagnostic-test-smoke`, `make diagnostic-registry-test-smoke`, `make test-parser`.
- ?음: parse/lex diagnostics?driver JSON diagnostic object?직접 ?리??refactor??별도 Tier 2 ?업으로 ??한다.

## UTF-8 Progress Note - 2026-04-25

- `TryRecv` / `RecvTimeout` are now copy-only for the beta surface.
- Ownership-bearing payloads (`QubitSlot`, `Slot<T>`, `SecureSlot<T>`,
  `subject`, boundary-value aggregates, and `Token<T>`) are explicitly rejected.
- Use blocking `<-` receive into a named binding or a plain projection/value
  channel when ownership provenance must cross a channel boundary.

## UTF-8 Progress Note - 2026-04-25 - Cancellation Payload Boundary

- `Cancel(Future<T>)` / `Cancel(RemoteFuture<T>)` are copy-only for beta.
- Ownership-bearing payload futures (`QubitSlot`, `Slot<T>`, `SecureSlot<T>`,
  `subject`, boundary-value aggregates, and `Token<T>`) are explicitly rejected
  until task-boundary cleanup summaries can prove observation/release.

## UTF-8 Progress Note - 2026-04-25 - Channel Close Boundary

- `ChannelClose(Channel<T>)` is copy-only for beta.
- Ownership-bearing queued payload channels (`QubitSlot`, `Slot<T>`,
  `SecureSlot<T>`, `subject`, boundary-value aggregates, and `Token<T>`) are
  explicitly rejected until channel cleanup/backpressure summaries can prove
  drain/release behavior.
## Progress Log - 2026-04-26 - DAG Fallback Seam Cap

- Owner-local resolver files no longer own direct fallback helper seams. They now call
  `semantic_type_resolution_lookup_or_materialize(...)`, which checks DAG
  metadata, materializes stable constructed shells, then falls through to the
  centralized resolver fallback only when imported ability/default/bound/module
  cases still need legacy materialization.
- `tests/type_resolution_resolver_inventory_smoke.sh` now caps active
  named fallback seams at 0, down from 20. This is still not full DAG
  source-of-truth, but it removes the old fallback helper API and prevents
  owner-local fallback seams from returning.
- Verified locally: `make type-resolution-resolver-inventory-test-smoke
  type-resolution-dag-test-smoke` and `make test-semantic`.
