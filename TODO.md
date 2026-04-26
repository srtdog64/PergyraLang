# Pergyra TODO (諛고룷 以鍮?

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

## ?꾩옱 ?곹깭 ?됱젙 ?됯? (2026-04-12 ?ъ젙??

### 醫낇빀 ?먮떒: Late-Stage Alpha

- 踰좏? readiness 異붿젙: ??`50%`
- ?꾩옱 ?쒗쁽: `late-stage alpha / beta-closure sprint`
- 蹂댁젙 ?댁쑀:
  - 湲곕뒫 ?쒕㈃留?蹂대㈃ core/foundation 援ы쁽? ?볦?留? beta??湲곕뒫 媛쒖닔媛 ?꾨땲??end-to-end ?좊ː?꾨떎
  - HIR/MIR CFG skeleton? ?대? ?덉?留? ?⑥닔/action/intent body ?덉쟾?깆쓽 semantic source-of-truth媛 ?꾩쭅 CFG/dataflow濡??밴꺽?섏? ?딆븯?? all-path return, use-before-init, move/borrow join, drop cleanup, zone/effect transition, parallel/channel boundary瑜?AST/helper traversal留뚯쑝濡??レ쑝硫?strict beta ?좊ː?꾧? 遺議깊븯??  - AIR abstraction safety??Phase 1 ?곗씠??援ъ“ / synthesis / drift checker baseline怨?driver semantic-validation wiring???ㅼ뼱?붾떎. Intent ??implementation drift 寃異쒖? `docs/104_air_compiler_architecture.md`? `make air-drift-test-smoke`濡?gate???ㅼ뼱?붽퀬, strict evidence??湲곕낯媛믪쑝濡??밴꺽?먮떎. missing RIR boundary/authority evidence??`PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`濡?hard-fail ?섎ŉ, `authorized by` participant ?대쫫怨?RIR authority fact / authorize op subject媛 ?쇱튂?댁빞 ?쒕떎. authority evidence ?꾨씫 吏꾨떒? `Reason:` ?덉뿉 expected authority participant list瑜??ы븿?쒕떎. AIR drift message? synthesized intent/boundary/authority name? owned lifetime?쇰줈 愿由щ릺怨? repeated drift check媛 ?댁쟾 message瑜??덉쟾?섍쾶 ?댁젣?섎뒗 ?뚭? ?뚯뒪?몄? parsed-source AIR teardown-safe boundary source ?뚭?媛 ?덈떎. `where + transfer`?????댁긽 zone boundary ?섎굹濡??묓엳吏 ?딄퀬 zone boundary? world-handoff boundary瑜?紐⑤몢 ?⑹꽦?쒕떎. world-handoff evidence???댁젣 matching RIR intent scope留뚯쑝濡??듦낵?섏? ?딄퀬 boundary source alias?????RIR `Move`/`Claim` transfer op瑜??붽뎄?쒕떎. parsed-source missing-authority-evidence negative? parsed-source IO execution-boundary missing-evidence negative??full driver JSON path?먯꽌 step source span怨?`stage/code/cause_ir/fix_source`源뚯? 怨좎젙?먮떎. expression boundary evidence?????댁긽 owner-name-only RIR scope match濡??듦낵?섏? ?딅뒗?? `PGY_AIR_STRICT_EVIDENCE=0`? 媛쒕컻/?붾쾭洹?opt-out?대떎. `make air-backend-nonimpact-test-smoke`??relaxed AIR? default strict AIR媛 intent/zone, cross-world transfer, handoff frontier, world projection, relation/effect, authority-failure fixture set?먯꽌 媛숈? C/LLVM ?띿뒪?몃? ?앹꽦?섎뒗吏 鍮꾧탳?쒕떎. `make air-backend-nonimpact-full-test-smoke`??full frozen backend-compare fixture sweep??媛숈? 諛⑹떇?쇰줈 ?뚮━怨?Linux CI gate濡??밴꺽?먮떎. `make air-strict-backend-compare-test-smoke`??strict evidence ?곹깭?먯꽌 C/LLVM ?ㅽ뻾 parity源뚯? 寃利앺븳?? parser/lexer baseline JSON routing? `stage`, `code`, `cause_ir`, `fix_source`源뚯? ?ロ삍?? ?⑥? blocker??AIR transfer/world source negative ?뺤옣, Windows native evidence, parser-specific code split / multi-error accumulation?대떎
  - Type-resolution DAG媛 ?꾩쭅 semantic source-of-truth媛 ?꾨땲誘濡?declaration order / module contract / generic consumer path drift ?꾪뿕???⑥븘 ?덈떎
  - ?κ린 紐⑤뱢??stop condition???꾩쭅 硫?? semantic 800 LOC 珥덇낵 `.inc` 議곌굔怨?runtime/codegen/compiler 1,000 LOC 珥덇낵 `.inc` 議곌굔? ?ロ삍吏留? ?щ윭 split? ?꾩쭅 include-order 蹂댁〈 ?곹깭???ㅼ젣 owner/TU extraction 遺梨꾧? ?⑥븘 ?덈떎
  - ?곕씪??怨듭떇 吏꾪뻾瑜좎? ?쒓린???쒕㈃ ?깆닕?꾟앷? ?꾨땲???쒕쿋? ?좊ː??readiness??湲곗??쇰줈 ??50%濡?蹂몃떎

## Beta taxonomy freeze: core / foundation / style

踰좏? 湲곗?? ?댁젣 湲곕뒫 ?섏뿴???꾨땲???몄뼱 ?뺤껜??湲곗??쇰줈 ?섎늿??

- Core language: `intent`, `world`, `zone`, `subject`, `relation`, `effect`, `projection`, `authority`, `handoff`, runtime observability, anchored ownership boundary, generic contract system, module visibility/export contract, `parallel`.
- Generic contract??core?? exact/ability/multi-bound/default type arg actual resolution? FP/OOP ?몄쓽媛 ?꾨땲??domain contract瑜??쒗쁽?섎뒗 ????몄뼱??
- Foundation layer: primitive values, `func`, `let`, control flow, callable/lambda baseline, `Option`/`Result`, stable collections, core ?ㅽ뻾???꾩슂??runtime ABI.
- Style / compatibility surface: OOP convenience, FP combinator libraries, app infra, richer async helpers.
- Execution family split: `parallel`? core execution primitive?닿퀬, `spawn`/`async`/`await`/`select`/`channel`/cancel? 洹??꾨옒 execution family?? fiber/coroutine? language core媛 ?꾨땲??runtime scheduling/suspension mechanism?대떎.
- Accelerator split: AI-first/GPU 諛⑺뼢? `pgy.accel.spray` ?쇰━ 紐⑤뱢濡??덉빟?쒕떎. ?대뒗 `parallel` / ownership / module visibility ?꾩뿉 ?щ씪媛??accelerator library/runtime 異뺤씠硫?core keyword ?뺤옣???꾨땲??
- Render split: Skia/shader/render graph 諛⑺뼢? `pgy.render.skia` ?쇰━ 紐⑤뱢濡??덉빟?쒕떎. renderer/shader??core keyword媛 ?꾨땲??Spray/Execution ?꾩쓽 ?앺깭怨?紐⑤뱢?대떎.
- Compatibility split: OOP/FP/DOP??媛곴컖 `pgy.compat.oop`, `pgy.compat.fp`, `pgy.compat.dop`濡?遺꾨━?쒕떎. 湲곗〈 ?몄뼱 ?ㅽ??쇱쓣 ?섏슜?섎릺 core identity濡??ㅻ챸?섏? ?딅뒗??
- Interop split: ?몃? ?몄뼱 ?곕룞(JVM 罹먯뒪??JNI 釉뚮┸吏, Python C-API ??? `pgy.interop.*` ?앺깭怨?紐⑤뱢濡?遺꾨쪟?섎ŉ, 踰좏? 留덉씪?ㅽ넠?먯꽌???꾩쟾???쒖쇅(Out of Beta)?쒕떎.

?낅뜲?댄듃 ?뺤콉:

- `pgy.core`??媛???먯＜ 媛쒖꽑?섎릺 媛???묎퀬 媛뺥븯寃?寃利앺븳??
- `pgy.foundation`? core蹂대떎 ?먮━寃??吏곸씠硫?ABI/backend parity瑜?源⑥? ?딅뒗??
- `pgy.accel.spray`, `pgy.render.skia`, `pgy.compat.*`, `pgy.std.*`, `pgy.kit.*`??紐⑤뱢 ?앺깭怨꾨줈 吏꾪솕?쒕떎. 鍮좊Ⅸ ?ㅽ뿕? ?덉슜?섏?留?core keyword瑜??섎━吏 ?딅뒗??

?ㅽ뻾 洹쒖튃:

- B0 blocker??`core + foundation stable subset`?먮쭔 遺숈씤??
- `pgy.fp`??Functor/HKT 異붿긽?? class-heavy OOP ?뺤옣, coroutine/fiber 怨좊룄?붾뒗 beta identity blocker媛 ?꾨땲??
- `pgy.accel.spray`??post-beta design surface?? 踰좏? ?꾩뿉????GPU ?ㅼ썙?쒕굹 backend-specific CUDA/ROCm/Metal 臾몃쾿???댁? ?딄퀬, module boundary? ownership ?먯튃留?怨좎젙?쒕떎.
- `pgy.render.skia`? `pgy.compat.dop`??post-beta design surface?? 踰좏? ?꾩뿉??shader/layout keyword瑜??댁? ?딄퀬 module boundary留?怨좎젙?쒕떎.
- ?? `parallel`? core?대?濡?slot/resource/effect conflict, cancellation/fairness, C/LLVM lowering parity??beta ?덉쭏 湲곗??쇰줈 怨꾩냽 愿由ы븳??
- Source of truth: `docs/99_language_module_taxonomy.md`
- Machine-readable manifest: `docs/language_module_manifest.json`
- Representative case tags: `docs/language_module_cases.json`
- Drift gate: `make module-taxonomy-test-smoke`
- Parallel core/execution split gate: `make parallel-core-contract-test-smoke`
- Operational beta checklist: `docs/100_beta_readiness_checklist.md`

## Formal semantics / mathematical proof obligations

踰좏????쒗뀒?ㅽ듃媛 ?듦낵?쒕떎?앸쭔?쇰줈 ?ロ엳吏 ?딅뒗?? stable subset留덈떎 ???蹂댁〈, 吏꾪뻾, ownership safety, authority soundness, projection freshness, DAG soundness, module visibility non-interference, backend parity 媛숈? ?섑븰??遺덈??앹씠 臾몄꽌?붾릺?댁빞 ?쒕떎.

- Source of truth: `docs/semantics/`
- Stable index: `docs/102_formal_semantics_and_proof_obligations.md`
- Drift gate: `make formal-semantics-test-smoke`
- ?곹깭: `IN PROGRESS / BLOCKER-DOC`
- 踰좏? 湲곗?:
  - [x] ?섑븰 library 臾몄꽌(`docs/45_math_layer_design.md`)? ?몄뼱 ?섎?濡?利앸챸 臾몄꽌瑜?遺꾨━?쒕떎.
  - [x] stable beta subset??semantic domain, judgment, theorem/proof-obligation vocabulary瑜?怨좎젙?쒕떎.
  - [ ] B0 ??ぉ留덈떎 theorem statement + current regression evidence + remaining proof obligation??理쒖떊 肄붾뱶 ?곹깭? 留욎텣??
  - [ ] runtime propagation, DAG, MIR declaration inventory, ABI ownership, C/LLVM parity???⑥? blocker瑜?proof obligation?쇰줈 異붿쟻?쒕떎.
  - [ ] beta 臾멸뎄?먯꽌 Lean/Coq/湲곌퀎利앸챸 ?꾨즺泥섎읆 蹂댁씠???쒗쁽??湲덉??쒕떎. 湲곌퀎利앸챸? 蹂꾨룄 executable model ?먮뒗 proof assistant artifact媛 ?앷린湲??꾧퉴吏 post-beta/v1 hardening?쇰줈 ?붾떎.
  - [~] **[NEW]** Runtime panic / unwinding model (abort vs unwind)???뺤콉 紐낆떆 諛?C/LLVM backend parity 利앸챸 異붽?. Panic class vocabulary? released-slot / invalid-secure-token / double-release / device-slot / out-of-bounds / authority-mismatch / OOM / divide-by-zero / internal-invariant hard-fail contract??`src/runtime/pgy_runtime_panic_contract.h`, `make runtime-panic-contract-test-smoke`, `make runtime-panic-abi-test-smoke`, `make runtime-panic-codegen-test-smoke`濡?怨좎젙?덈떎. Generated C/LLVM `Array<T>`/`Slice<T>` indexing, temporary function-return indexing, `ArraySet`, `ListGet`, `QueuePop`, `MapGet`, `ListSet`, `ListRemove`, `MapRemove` invalid access? `Unwrap(Err)` / `UnwrapOption(None)` misuse??checked runtime helper / panic contract濡?怨좎젙?덈떎. ?⑥? 寃껋? ??panic class媛 異붽????뚮쭏??媛숈? executable parity gate瑜??붽뎄?섎뒗 寃껋씠??
  - [~] **[NEW]** Secure slot 諛?authority token???꾨?議?遺덇??μ꽦(Unforgeability) ?뺤떇 遺덈???Formal Invariants) 臾몄꽌?? Secure slot invalid-token/denied-capability export path??silent fallback?먯꽌 panic contract濡??대룞?덈떎.
  - [ ] **[NEW]** Intent ?쒖뒪?쒖쓽 Rollback/Cleanup 蹂댁옣?????Formal Closure (?곹깭 湲곌퀎 利앸챸) 臾몄꽌??

?댁쁺 洹쒖튃:

- ?뚯뒪???ㅻえ??諛깆뿏??鍮꾧탳??proof evidence?댁? proof ?먯껜媛 ?꾨땲??
- undocumented mathematical assumption???꾩슂??surface??stable???꾨땲??`IN PROGRESS`, `explicit reject`, ?먮뒗 `OUT OF BETA`濡??대젮???쒕떎.
- FP functor/HKT, full ownership, full quantum, GPU/Spray, Skia/render graph???꾩옱 beta proof scope 諛뽰씠??

## Missing beta gate audit

?꾩옱 strict beta 湲곗??먯꽌???ㅼ쓬 ??ぉ??蹂꾨룄 gate濡?蹂몃떎. ????ぉ?ㅼ? 湲곕뒫 ?뺤옣???꾨땲???대? ?덈뒗 core/runtime/tooling ?쒕㈃???좊ː??怨꾩빟?대떎.

- [~] Runtime panic / unwinding model: OOM, divide-by-zero, out-of-bounds, slot violation, token mismatch, authority mismatch, invariant break??abort/unwind/recoverable ?뺤콉??`Runtime Panic Parity` proof obligation?쇰줈 ?щ졇?? `src/runtime/pgy_runtime_panic_contract.h`媛 panic class vocabulary瑜??뚯쑀?섍퀬, inline/exported typed slot read/write/release??released-slot 諛?double-release?먯꽌 ???댁긽 湲곕낯媛?no-op濡?鍮좎?吏 ?딅뒗?? `make runtime-panic-abi-test-smoke`媛 released-slot, invalid-secure-token, double-release, device-slot, out-of-bounds, authority-mismatch, OOM, divide-by-zero executable evidence瑜??쒓났?쒕떎. `make runtime-panic-codegen-test-smoke`??generated C/LLVM divide/modulo-by-zero? `Array<T>`/`Slice<T>` index, temporary function-return index, `ArraySet`, `ListGet`, `QueuePop`, `MapGet`, `ListSet`, `ListRemove`, `MapRemove` invalid access, `Unwrap(Err)`, `UnwrapOption(None)` parity瑜?寃利앺븳?? ?⑥? 寃껋? ??hard-fail class媛 異붽????뚮쭏??媛숈? executable parity gate瑜??붽뎄?섎뒗 寃껋씠??
- [~] Secure slot / authority secret invariant: token unforgeability, secure-slot mismatch denial, authority token non-forgeability, authority transfer single-owner invariant, runtime snapshot secret non-exposure瑜?`Secure Token Unforgeability` / `Authority Transfer Single-Owner` proof obligation?쇰줈 ?щ졇?? inline/exported secure slot read/write/release invalid-token 諛?denied-capability path??`PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN`濡?怨좎젙?덇퀬 secure-slot double-release??`PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE`濡?怨좎젙?덈떎. `make runtime-panic-abi-test-smoke`媛 invalid-token/double-release executable evidence瑜??쒓났?쒕떎. authority-token mismatch??`authority-token-mismatch` runtime code/reason, `make test-security`, `authority_failure_abi`, `authority_failure_surface`濡?C/LLVM parity regression源뚯? ?レ븯?? unsupported authority-token transport??channel send/receive/helper/close, cancellation payload, direct named `spawn`?먯꽌 explicit reject濡??レ븯?? ?⑥? 寃껋? richer domain-boundary denial?대떎.
- [ ] Intent formal closure: step ordering, compensation/rollback/invalidation, effect propagation, observability ABI stability瑜?beta-stable contract濡?怨좎젙?쒕떎.
- [ ] Zone/world/authority/handoff formal closure: zone generation, world embedding, handoff frontier, projection freshness, authority rejection query surface瑜?beta-stable contract濡?怨좎젙?쒕떎.
- [ ] Diagnostic quality gate: 紐⑤뱺 user-facing error媛 severity, stable code, source span when available, `Reason:`, `Fix:`瑜?媛뽯룄濡??덉쭏 湲곗???registry smoke? 蹂꾨룄 gate濡??붾떎.
  - 吏꾪뻾: intent clause explicit reject 以?`spawn`/channel control-transfer AST媛 parser source span??蹂댁〈?섎룄濡?怨좎낀怨? `make diagnostics-json-test-smoke`媛 `on: spawn ...`? `on: ch <- value`??`PGY_SEM_INTENT_STEP_INVALID` JSON line/column + `cause_ir` + `fix_source`瑜?怨좎젙?쒕떎.
- [ ] Cross-platform CI matrix: Linux/WSL, Windows native/MSYS2/MinGW, macOS??support level??stable/experimental/out-of-beta濡?紐낆떆?쒕떎.
  - 吏꾪뻾: Windows LLVM support detection? executable `llvm-config --libs core` evidence媛 ?덉쓣 ?뚮쭔 `WINDOWS_LLVM_READY=1`???섎룄濡?醫곹삍?? `C:/Program Files/LLVM/lib` 媛숈? library folder 議댁옱留뚯쑝濡?LLVM smoke/backend-compare瑜??ㅽ뻾?섏? ?딅뒗?? ?꾩옱 beta 怨꾩빟? Linux C+LLVM, Windows C-only?대ŉ Windows LLVM? ?ㅼ젣 MSYS2 runner green evidence媛 ?앷만 ?뚮쭔 ?밴꺽?쒕떎.
  - 吏꾪뻾: README support matrix??macOS??dedicated runner/support contract媛 ?앷만 ?뚭퉴吏 out-of-beta濡?紐낆떆?덈떎.
- [~] Beta stable subset definition: keyword, syntax, API, AST-visible shape, runtime ABI, backend parity 踰붿쐞瑜?`docs/107_beta_stable_subset.md`?먯꽌 freeze?쒕떎. ?⑥? ?쇱? ??臾몄꽌??媛?stable ??ぉ???대떦 semantic/runtime/C/LLVM regression row? 1:1濡??곌껐?섎뒗 寃껋씠??
- [~] Stdlib beta freeze list: stable/experimental/out-of-beta API? breaking-change policy瑜?紐낆떆?쒕떎.
  - 吏꾪뻾: `docs/108_stdlib_beta_freeze.md`媛 builtin stdlib, stable `use` modules, known experimental modules, out-of-beta ecosystem work瑜?遺꾨━?쒕떎. `make stdlib-test-smoke`媛 builtin stdlib probe? stable `use` module probe瑜?C/LLVM ?묒そ?먯꽌 怨좎젙?쒕떎. ?⑥? ?쇱? third-party package/version/supply-chain policy??
- [~] Tooling conformance: LSP/fmt/debugger??beta-stable 踰붿쐞瑜?紐낆떆?쒕떎.
  - 吏꾪뻾: `make tooling-conformance-test-smoke`媛 formatter idempotence/compile smoke, LSP initialize/hover/completion capability, debugger CLI parse+semantic+quit path瑜?executable gate濡?怨좎젙?쒕떎. DAP, binary breakpoint, variable watch, rich refactor, multi-file workspace LSP???꾩쭅 beta-stable tooling subset???꾨땲??
- [~] Package/module resolver surface: manifest, version resolution, import path, supply-chain integrity瑜?stable/experimental/out-of-beta濡?遺꾨쪟?쒕떎.
  - 吏꾪뻾: `docs/109_package_module_resolver_contract.md`媛 beta-stable module surface瑜?`import "relative/path.pgy";`, importing-file-relative resolution, namespace/export visibility, circular import rejection?쇰줈 怨좎젙?덈떎. package surface??`pgy init <name>` scaffolding留?stable?대떎.
  - 吏꾪뻾: `pgy install`? ???댁긽 ?뚯뒪 ?뚯씪 寃쎈줈濡??ㅼ씤?섏? ?딄퀬 explicit out-of-beta rejection???몃떎. `make package-module-resolver-test-smoke`媛 doc contract, `pgy init`, `pgy install` reject, missing import JSON, circular import JSON??怨좎젙?쒕떎.
  - ?⑥쓬: dependency version solving, lockfile, registry, checksum/signature verification, remote import, supply-chain integrity??beta ?댄썑 resolver/package-manager track?쇰줈 ?좎??쒕떎.
- [~] Test quality gate: pre-beta mandatory suite, fuzz/property status, coverage/perf baseline??異붿쟻?쒕떎.
  - 吏꾪뻾: `docs/111_beta_test_suite_freeze.md`媛 mandatory pre-beta gates, platform gates, fuzz/property/coverage non-claims, regression policy瑜?freeze?덈떎. `make beta-test-suite-freeze-test-smoke`媛 freeze doc怨?Makefile target 議댁옱瑜?寃?ы븳??
  - ?⑥쓬: ?ㅼ젣 fuzz corpus, property-based generator, coverage percentage threshold??beta ?댄썑 ?덉쭏 ?몃옓?쇰줈 ?좎??쒕떎. ?꾩옱 beta gate??named stable-surface coverage??
- [~] Observability/tracing schema: event schema, intent history, authority failure state, runtime registry, trace format version??怨좎젙?쒕떎.
  - 吏꾪뻾: `docs/112_observability_trace_schema.md`媛 beta-stable schema瑜?`IntentLast*`, `IntentHistory*`, `IntentActive*`, `IntentRecent*`, authority failure snapshot(`ok/zone/participant/code/reason`), runtime-borrowed string ABI, C/LLVM identical trace output?쇰줈 怨좎젙?덈떎.
  - 吏꾪뻾: `make observability-schema-test-smoke`媛 `intent_trace_abi`, `intent_recent_abi`, `intent_active_abi`, `intent_failure_abi`, `authority_failure_abi`瑜?C/LLVM ?묒そ?먯꽌 expected stdout怨?鍮꾧탳?쒕떎.
  - ?⑥쓬: general event streaming, structured JSON trace export, distributed trace correlation, user-code registry hooks, stable binary trace format, richer multi-instance timeline query??beta ?댄썑濡??좎??쒕떎.
- [~] Memory/concurrency model: `parallel`, task, channel, cancellation, visibility/happens-before 理쒖냼 怨꾩빟??臾몄꽌?뷀븳??
  - 吏꾪뻾: `docs/113_memory_concurrency_model.md`媛 beta-stable happens-before, channel, cancellation, explicit out-of-beta memory model 踰붿쐞瑜?怨좎젙?덈떎. `parallel` join visibility, shared `ref`/`ref` ?덉슜, `ref`/`own` 諛?`own`/`own` task-boundary reject, copy-only non-blocking receive/cancel/close瑜?stable contract濡?臾띠뿀??
  - 吏꾪뻾: `make memory-concurrency-model-test-smoke`媛 `parallel-core-contract-test-smoke`? targeted C/LLVM backend compare(`parallel_channel_sum`, `parallel_channel_dual`, `triple_paradigm`)瑜??ㅽ뻾?쒕떎.
  - ?⑥쓬: full weak-memory ordering, user-selectable memory order, scheduler fairness guarantee, lock-free correctness, anonymous async closure capture/lifetime, cross-thread `Arc<T>` / `Send` / `Sync` trait system? beta ?댄썑濡??좎??쒕떎.
- [~] String/unicode policy: normalization, comparison, locale, escape handling, unsupported policy瑜?紐낆떆?쒕떎.
  - 吏꾪뻾: `docs/110_string_unicode_policy.md`媛 UTF-8 string payload preservation, byte-length `StringLength`, byte-exact/normalization-blind equality/search瑜?beta-stable濡?怨좎젙?덈떎.
  - 吏꾪뻾: Unicode identifiers, normalization, locale-sensitive collation/case folding, grapheme iteration, display width, mixed-encoding source files??explicit out-of-beta濡?怨좎젙?덈떎. `make unicode-policy-test-smoke`媛 C/LLVM UTF-8 string execution怨?Unicode identifier reject瑜?寃利앺븳??
  - ?⑥쓬: full Unicode text model???꾩엯?섎젮硫?post-beta??scalar/grapheme/locale vocabulary? 蹂꾨룄 stdlib text module???ㅺ퀎?쒕떎.

Checklist source of truth:

- `docs/100_beta_readiness_checklist.md`
- AIR source of truth: `docs/104_air_compiler_architecture.md`
- Drift gate: `make beta-readiness-checklist-test-smoke`
- AIR drift gate: `make air-drift-test-smoke`
- AIR backend non-impact gate: `make air-backend-nonimpact-test-smoke`
- AIR full backend non-impact hardening: `make air-backend-nonimpact-full-test-smoke`
- AIR strict backend execution parity: `make air-strict-backend-compare-test-smoke`

## 援ъ“/?댁쁺 ?먯씤 ?ъ씤??蹂대뱶 (2026-04-20)

???뱀뀡? 湲곕뒫 backlog媛 ?꾨땲?? ?ㅼ젣 ?묒뾽 ?⑥쑉怨?踰좏? ?좊ː?꾨? 怨꾩냽 源롫뒗 援ъ“ debt / ?댁쁺 pain point瑜?怨좎젙?쒕떎.

?곗꽑?쒖쐞 ?쒖븞:
- `P0`: function/action/intent body CFG + dataflow瑜?semantic source-of-truth濡??밴꺽
- `P1`: `.inc` 遺꾪븷???ㅼ젣 `.c`/`.h` 紐⑤뱢濡??꾪솚
- `P2`: hint namespace (`code` / `cause_ir` / `fix_source`)瑜??덉??ㅽ듃由?湲곕컲?쇰줈 怨좎젙
- `P3`: type-category vocabulary瑜?2-3痢듭쑝濡??뺤텞
- `P4`: 鍮뚮뱶/?뚮뱶諛뺤뒪/以묎컙-stage JSON/artifact 臾몄젣瑜?怨듭떇 寃쎈줈 湲곗??쇰줈 ?뺣━
- `P9`: arena ?⑦꽩??scratch/result lifetime 湲곗??쇰줈 紐낆떆 ?꾩엯
- `P9b`: repeated `Slot` / `SecureSlot` hot-loop access??Pin/Lease 臾몄꽌 湲곗??쇰줈 遺꾨━?쒕떎. 湲곕낯 path??留??묎렐 寃利앹씠怨? fast path??scope-entry capability lease + automatic unpin cleanup?댁뼱???쒕떎. Runtime ABI baseline? `PgyPinnedView` / `PergyraSlotPin` / `PergyraSlotUnpin` + `make test-security` ?뚭?濡??쒖옉?덇퀬, plain token-bearing pin rejection, scope release while pinned, TTL cleanup skip while pinned, secure invalid-token/capability rejection, concurrent secure write rejection, release-after-unpin persistence瑜??レ븯?? Candidate source syntax `pin slot as view { ... }`??CFG cleanup/backend parity媛 ?ロ옄 ?뚭퉴吏 parser explicit reject濡?遊됱씤?덇퀬 `make diagnostics-json-test-smoke`媛 JSON route瑜?寃利앺븳?? Pin/Lease semantic diagnostic vocabulary??`PGY_SEM_PIN_ESCAPE`, `PGY_SEM_PIN_PARALLEL_CONFLICT`, `PGY_SEM_PIN_AWAIT_BOUNDARY`, `PGY_SEM_PIN_QUBIT_REJECT`, `PGY_SEM_PIN_TOKEN_INVALID`濡?registry/docs??怨좎젙?덇퀬 `make diagnostic-registry-test-smoke`? `make beta-readiness-checklist-test-smoke`媛 drift瑜?留됰뒗?? Existing `ViewRead(...)` / `ViewWrite(...)` semantic surface now enforces `WriteView<T>` exclusive access for the same source slot while keeping shared `ReadView<T>` / `ReadView<T>` accepted. It also emits pin-specific diagnostics for return escape, await boundary, parallel boundary/acquisition, and QubitSlot rejection, and `make diagnostics-json-test-smoke` verifies their CLI JSON route. Generic ownership baseline? unresolved `TYPE_KIND_GENERIC`??`BORROW_TRACKED`濡?遺꾨쪟??generic `own/ref`媛 議곗슜??copy-only濡??듦낵?섏? 紐삵븯寃?留됰뒗?? ?⑥? 寃껋? stable source syntax, block-scoped CFG cleanup edge, secure-token source diagnostic, C/LLVM parity?? Source of truth: `docs/74_slot_pinning_caching.md`
- `P9c`: `Rc<T>` / `Weak<T>` 理쒖냼 subset? beta-stable濡??レ븯?? 踰붿쐞??single-thread `Int|Long|Float|Double|Bool|String` payload, explicit lifecycle builtin(`RcNew`, `RcClone`, `RcGet`, `RcDrop`, `RcDowngrade`, `WeakUpgrade`, `WeakDrop`), resolver metadata, semantic builtin typing, C runtime/emitter, LLVM runtime export/lowering, ABI layout smoke, C/LLVM lifecycle backend-compare?? 踰붿쐞 諛?payload??backend fallback???꾨땲??semantic explicit reject?? `Arc<T>`, cross-thread shared ownership, generic/object payload ?뺤옣, default ARC??beta 諛뽰씠?? Source of truth: `docs/100_beta_readiness_checklist.md`, `docs/106_ownership_model_comparison.md`, `src/runtime/pgy_abi_spec.h`
- `P10`: 紐⑤뱢???꾪뙆 怨좊룄?붿쓽 compile/runtime ?띾룄 ?뚭?瑜?蹂꾨룄 baseline?쇰줈 異붿쟻

### P0. Function CFG / Body Dataflow Closure

?먯젙: `BLOCKER`

?듭떖 ?뺣━:

- CFG媛 ?녿뒗 ?곹깭???꾨땲?? HIR??function CFG v0, predecessor/reachability, dominator/frontier, loop depth, phi candidate skeleton??媛吏꾨떎.
- MIR??HIR CFG? RIR op瑜?臾띠뼱 routine/block/instruction/cleanup block, SSA version map, def/use, cleanup/rollback/invalidation exceptional CFG, liveness/DCE vertical slice源뚯? 媛吏怨??덈떎.
- ?⑥? blocker????CFG/MIR infra瑜?**?⑥닔 蹂몃Ц ?섎?濡좎쓽 source-of-truth**濡??밴꺽?섎뒗 寃껋씠?? ?꾩옱 body safety???쇰????ъ쟾??AST/helper traversal, local summary, backend fallback??湲곕?怨??덉뼱 strict beta 湲곗??쇰줈 遺議깊븯??

踰좏? ?꾨즺 議곌굔:

- [ ] Function/action/intent body留덈떎 `BasicBlock`, `Edge`, `Terminator`, reachability, exceptional cleanup edge媛 semantic pass?먯꽌 吏곸젒 ?뚮퉬?쒕떎.
- [x] 諛섑솚?뺤씠 ?덈뒗 routine? 紐⑤뱺 reachable normal path?먯꽌 return/value terminator瑜?媛吏꾨떎??all-path return 寃?щ? CFG body summary濡?怨좎젙?쒕떎.
- [~] definite assignment/use-before-init 寃?щ? CFG dataflow濡??대룞?섍퀬 branch/join/loop widening 吏꾨떒??怨좎젙?쒕떎. stable local `let` ?쒕㈃? parser `=` ?붽뎄? `PGY_SEM_UNINIT_LOCAL` backstop?쇰줈 遊됱씤?먭퀬, wider delayed-assignment lattice???꾩쭅 ?대젮 ?덈떎.
- [~] move/use-after-move, borrow/ref lifetime, boundary escape瑜?CFG join facts濡?怨꾩궛?쒕떎. `QubitSlot` loop break/continue join regression, anchored `Slot<T>` branch/join release-state regression, `own subject` branch/join consumed-state regression, parallel subject transfer join/conflict regression, parallel `ref`+`own` boundary conflict regression, parallel `ref`+`ref` shared-read acceptance regression, direct named-call `spawn ref` ownership-boundary rejection regression, anonymous async spawn explicit reject regression? ?ロ삍怨? closure/lambda/general longer-lived borrow lifetime? ?⑥븘 ?덈떎. `mut ref`/`ref mut` surface媛 ?놁쑝誘濡?mutable-borrow overlap? beta-out-of-scope濡?遊됱씤?쒕떎.
- [~] owned resource drop/cleanup insertion point瑜?normal return, early return, break/continue, intent cancel/rollback/invalidation edge?먯꽌 媛숈? 洹쒖튃?쇰줈 怨꾩궛?쒕떎. `defer` cleanup terminator? resource-state snapshot/restore 寃⑸━, direct `type_check_statement()` fallback convergence, anchored slot branch/join state tracking? ?ロ삍怨? full drop insertion/validation? ?⑥븘 ?덈떎.
- [ ] zone/effect/relation transition facts瑜?path-sensitive summary濡??щ젮 branch/join/handoff?먯꽌 stale state? conflict瑜?媛숈? vocabulary濡?吏꾨떒?쒕떎.
- [~] `parallel`/channel/task boundary?먯꽌 moved value, borrowed reference, authority-bearing token, cancellation cleanup fact瑜?CFG summary濡?寃利앺븳?? parallel task-local terminator isolation, moved/released resource/boundary join, duplicate resource/boundary consume diagnostic, `ref`+`own` boundary conflict, blocking channel-send resource consume/join, direct named-call `spawn ref` ownership-boundary rejection, direct named-call `spawn Token<T>` authority-boundary rejection, anonymous async spawn explicit reject, `SendTimeout`/`TrySendStatus`/`SendTimeoutStatus` transport rejection, `TryRecv`/`RecvTimeout` movable receive explicit reject, authority `Token<T>` channel helper rejection, copy-only cancellation payload reject, copy-only channel close???ロ삍怨? broader channel receive/backpressure summary, closure/lambda/general borrowed-reference task lifetime, cancellation cleanup fact???⑥븘 ?덈떎.
- [~] Interprocedural body summary瑜?怨좎젙?쒕떎: `may_return`, `may_escape_ref`, `moves_param`, `borrows_param`, `drops_resource`, `effects`, `requires_zone`, `spawns_task`, `sends_channel`. 1李?援ъ“濡?function type??`body_summary_mask`? semantic recorder???ㅼ뼱媛붾떎. direct function call? callee summary 以?caller-relevant transitive facts瑜??뚮퉬?섍퀬 declaration-known `own/ref` parameter boundary facts??湲곕줉?쒕떎. method/host call??媛숈? declaration-known summary facts瑜?湲곕줉?쒕떎. lambda body summary??lambda function type??寃⑸━?섏뼱 outer routine?쇰줈 ?덉? ?딄퀬, function-typed lambda binding ?몄텧? 媛숈? callee-summary path濡??꾪뙆?쒕떎. ?⑥? 寃껋? intent/helper call源뚯? ?볧엳怨?zone/effect/runtime propagation怨?C/LLVM lowering????summary bit瑜?吏곸젒 ?뚮퉬?섍쾶 留뚮뱶???쇱씠??
- [ ] 吏꾨떒? block/path provenance瑜??ы븿?쒕떎: source path, branch/join edge, previous state, Reason, Fix.
- [ ] MIR/C/LLVM lowering? 媛숈? CFG/dataflow facts瑜??뚮퉬?섍퀬, frozen subset parity regression?쇰줈 臾띕뒗??

?ㅽ뻾 ?쒖꽌:

1. ?꾩옱 HIR/MIR CFG fact inventory? semantic ?뚮퉬 吏?먯쓣 ?쒕줈 留뚮뱺??
2. `--hir-cfg`, `--mir`, RIR flow-block dump瑜?臾띕뒗 smoke瑜?異붽???CFG fact drift瑜?留됰뒗??
3. all-path return + reachability + definite assignment瑜?CFG 湲곕컲?쇰줈 癒쇱? ?밴꺽?쒕떎.
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

- 臾몄젣:
  - ?꾩옱 `type_checker.c` 諛?transpiler/LLVM ?쇰????쒕え?덊솕?앷? ?꾨땲???쒗뙆??遺꾪븷???⑥씪 translation unit?앹뿉 媛源앸떎
  - IDE jump/symbol lookup/forward decl ?쒖꽌 愿由ш? 紐⑤몢 ?섎룞
  - formatter/linter/?몃? edit媛 include ?쒖꽌/?뚯씪 媛깆떊 ??대컢??誘쇨컧?섍쾶 源⑥쭊??- ?곹뼢:
  - ????섏젙 ??edit conflict / implicit declaration / include ordering failure媛 諛섎났??  - ownership/generic/provenance 媛숈? ?〓떒 ?묒뾽??遺덊븘?뷀븯寃??먮젮吏꾨떎
- 湲곕낯 諛⑹묠:
  - ?곗꽑 `semantic/type_checker_*`?먯꽌 ownership / generic / module-contract / diagnostics 異뺣????ㅼ젣 `.c`/`.h` export 援ъ“濡??덈떒
  - declaration-side MIR-only hot path??helper family瑜?`.c` 寃쎄퀎濡?遺꾨━
  - ?κ린 紐⑺몴?좎? `docs/92_inc_split_roadmap.md`??Target State A-D濡?怨좎젙?쒕떎
  - stop condition: semantic?먮뒗 800 LOC 珥덇낵 `.inc` ?놁쓬, codegen/runtime?먮뒗 1,000 LOC 珥덇낵 `.inc` ?놁쓬, `type_checker.c`??orchestration-only, backend declaration path??dedicated inventory reader ?먮뒗 hard error留??덉슜
  - speed stop condition: `test-abi-perf`? `perf-summary` baseline???좎??섍퀬, 紐⑤뱢??slice ??worst-case compile time??2諛??댁긽 ?硫??뚭? ?꾨낫濡?湲곕줉
  - `.inc`??generated table / local macro table / private test fixture 媛숈? ?쒗븳 ?⑸룄濡쒕쭔 ?④릿??- 以鍮??묒뾽:
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
  - 吏꾪뻾: ownership 怨듭슜 enum/entrypoint瑜?`type_checker_ownership_internal.h`濡?遺꾨━ ?쒖옉
  - 吏꾪뻾: ownership diagnostics forward declaration??`type_checker_ownership_diag_internal.h`濡?遺꾨━ ?쒖옉
  - 吏꾪뻾: ownership escape diagnostic renderer/helper family??`type_checker_ownership_diag.c`濡??ㅼ젣 TU 遺꾨━ ?꾨즺
  - 吏꾪뻾: ownership support helper(`semantic_assignment_target_path`, `semantic_borrowed_boundary_root_name`)??`type_checker_ownership_support_internal.h`濡?遺꾨━ ?쒖옉
  - 吏꾪뻾: ownership consumer seam(`return` / `assign` / `call`)??`type_checker_ownership_consumers_internal.h`濡?遺꾨━ ?쒖옉
  - 吏꾪뻾: `param_summary`??raw include block???꾨땲??`semantic_check_param_summary_escapes(...)` consumer helper濡??밴꺽
  - 吏꾪뻾: channel transport seam??`type_checker_channel_transport_internal.h`濡?遺꾨━ ?쒖옉
  - 吏꾪뻾: channel transport validator/reporters??`type_checker_channel_transport.c`濡??ㅼ젣 TU 遺꾨━ ?꾨즺
  - 吏꾪뻾: high-arity generic mismatch helper??`type_checker_generic_diag.c`濡??ㅼ젣 TU 遺꾨━ ?꾨즺
  - 吏꾪뻾: module contract consumer ?좏뻾 seam??ability reference display/name/signature helper??`type_checker_ability_ref.c`濡??ㅼ젣 TU 遺꾨━ ?꾨즺
  - 吏꾪뻾: stdlib use validator??`type_checker_stdlib_use.c`濡??ㅼ젣 TU 遺꾨━ ?꾨즺
  - 吏꾪뻾: subject ability mismatch diagnostic? `type_checker_module_contract_diag.c`濡??ㅼ젣 TU 遺꾨━ ?꾨즺
  - 吏꾪뻾: ability `fields` validator??`type_checker_ability_fields.c`濡??ㅼ젣 TU 遺꾨━ ?꾨즺
  - 吏꾪뻾: `find_type_decl_by_name`??include-order static helper?먯꽌 `type_checker_internal.h` internal API濡??밴꺽
  - 吏꾪뻾: ability ref matching / role ability lookup / subject ability lookup? `type_checker_ability_match.c`濡??ㅼ젣 TU 遺꾨━ ?꾨즺
  - 吏꾪뻾: `find_ability_decl_by_name` / `collect_effective_generic_arg_nodes`??include-order static helper?먯꽌 `type_checker_internal.h` internal API濡??밴꺽
  - 吏꾪뻾: ability where-bound consumer validation? `type_checker_ability_where.c`濡??ㅼ젣 TU 遺꾨━ ?꾨즺
  - 吏꾪뻾: `format_type_constraint_bounds`??include-order static helper?먯꽌 `type_checker_internal.h` internal API濡??밴꺽 ??蹂꾨룄 TU濡?遺꾨━
  - 吏꾪뻾: `semantic_type_resolution_record_type_ref_dependency`??graph core TU濡??대룞??include-order static helper ?섏〈???쒓굅
  - 吏꾪뻾: `semantic_type_resolution_collect_type_refs`??`type_checker_resolution_graph_collect.c`濡??대룞??DAG inventory collector??泥??ㅼ젣 TU seam??留뚮뱾?덈떎
  - 吏꾪뻾: generic contract inventory / string dependency / required ability collector helpers??`type_checker_resolution_graph_collect.c`濡??대룞
  - 吏꾪뻾: top-level declaration graph registration??`type_checker_resolution_graph_collect.c`濡??대룞??inventory pass??bootstrap helper debt瑜???以꾩???  - 吏꾪뻾: local-contract graph node/dependency + zone/world/projection label formatters??`type_checker_resolution_graph_labels.c`濡??대룞??graph inventory `.inc`瑜?1,835 LOC源뚯? 異뺤냼?덈떎
  - 吏꾪뻾: projection source resolver??`type_checker_resolution_graph_domain.c`濡??대룞?섍퀬 `find_zone_domain_slot`??internal API濡??밴꺽??graph/domain split ?좏뻾 seam??留뚮뱾?덈떎
  - 吏꾪뻾: event declaration precollector??`type_checker_resolution_graph_decl.c`濡??대룞??declaration-kind collector 遺꾨━???쒖옉
  - 吏꾪뻾: enum declaration precollector??`type_checker_resolution_graph_decl.c`濡??대룞?섍퀬 `semantic_stage_method_array`瑜?internal API濡??밴꺽??inventory `.inc`瑜?1,765 LOC源뚯? 異뺤냼
  - 吏꾪뻾: ability declaration precollector? action-contract precollector??`type_checker_resolution_graph_decl.c`濡??대룞??inventory `.inc`瑜?1,648 LOC源뚯? 異뺤냼
  - 吏꾪뻾: role/class/party/roster declaration precollector??`type_checker_resolution_graph_decl.c`濡??대룞?섍퀬, relation/effect domain inventory precollector??`type_checker_resolution_graph_domain.c`濡??대룞??inventory `.inc`瑜?1,299 LOC源뚯? 異뺤냼
  - 吏꾪뻾: intent declaration precollector??`type_checker_resolution_graph_decl.c`濡? world inventory precollector??`type_checker_resolution_graph_world.c`濡??대룞??inventory `.inc`瑜?870 LOC源뚯? 異뺤냼
  - 吏꾪뻾: zone refresh projection field-map DAG collector??`type_checker_resolution_graph_zone.c`濡??대룞?덇퀬, graph inventory body??`type_checker_resolution_graph_inventory.c`濡??밴꺽?덈떎. `type_checker_resolution_graph_inventory.inc`???쒓굅?섏뼱 DAG inventory include-order debt媛 ?ロ삍??  - 吏꾪뻾: projection builtin target-field resolver??recursive fallback ???DAG metadata lookup-only seam?쇰줈 ??톬?? projection source/target mismatch 吏꾨떒? projection validator媛 ?뚯쑀?섍퀬, target field type materialization? DAG metadata媛 ?뚯쑀?쒕떎. fallback seam cap? 31?먯꽌 30?쇰줈 ?대젮媛붾떎. ?댄썑 type graph precollect瑜?top-level symbol pass ?욎뿉 諛곗튂?섍퀬 `program_resolve_type_quiet(...)`瑜?metadata lookup-only濡???떠 event/function placeholder媛 recursive fallback ?놁씠 DAG metadata瑜??곌쾶 ?덈떎. fallback seam cap? 30?먯꽌 29濡??대젮媛붾떎. domain query projection source-field resolver??class/vessel field DAG metadata lookup-only濡???떠 cap? 28濡??대젮媛붾떎. party/roster shared-field resolver??declaration metadata lookup-only濡???떠 cap? 26?쇰줈 ?대젮媛붾떎. ability abstract method signature resolver? role host-type resolver??lookup-only濡???떠 cap? 24濡??대젮媛붾떎. function/action body expression/lambda/event handler precollect ?뺤옣 ??event/lambda handler resolver??lookup-only濡???떠 cap? 23?쇰줈 ?대젮媛붾떎. body flow resolver??graph metadata lookup-only濡???떠 cap? 22濡??대젮媛붾떎. type-alias statement resolver??DAG metadata lookup-only濡???떠 cap? 21濡??대젮媛붾떎. `world_decl` lookup-only ?꾪솚? subject/zone nominal materialization???꾩쭅 遺議깊빐 semantic 77媛??ㅽ뙣瑜?留뚮뱾?덉쑝誘濡?蹂대쪟?덈떎
  - 吏꾪뻾: world/zone local-contract stage replay??`type_checker_resolution_stage_domain.c`濡??대룞?덇퀬, top-level DAG stage replay??`type_checker_resolution_stage.c`濡??밴꺽??`type_checker_resolution_stage.inc`瑜??쒓굅
  - 吏꾪뻾: `type_checker_ability_decl.c`, `type_checker_zone_decl.c`, `type_checker_world_decl.c`??standalone TU濡?鍮뚮뱶?섎ŉ hidden include-order helper ?섏〈??internal/header 怨꾩빟?쇰줈 ?밴꺽
  - 吏꾪뻾: `type_checker_intent_decl.c` standalone TU ?밴꺽 以??쒕윭??implicit helper dependency瑜?internal/header 怨꾩빟?쇰줈 ?밴꺽?섍퀬, `-Werror=implicit-function-declaration -Werror=implicit-int`瑜?湲곕낯 CFLAGS濡?怨좎젙??媛숈? 醫낅쪟??C 紐⑤뱢??踰꾧렇瑜?鍮뚮뱶 ?④퀎?먯꽌 李⑤떒
  - 吏꾪뻾: `type_checker_role_decl.c`, `type_checker_party_decl.c`, `type_checker_roster_decl.c`??hard implicit-declaration CFLAGS ?꾨옒?먯꽌 鍮뚮뱶?섎룄濡?helper/header ?섏〈??紐낆떆
  - 吏꾪뻾: `type_checker_class_decl.c`媛 class/extern declaration checking???뚯쑀?섍퀬, `type_checker_program.c`媛 top-level semantic orchestration???뚯쑀?쒕떎. 愿??graph/worklist/effect/stats helper瑜?internal API濡??밴꺽??`type_checker_program.inc`瑜?624 LOC源뚯? 異뺤냼
  - 吏꾪뻾: `type_checker_builtins_projection.c`媛 `ToObject` / `ToTObject` semantic projection checker瑜??뚯쑀?섍퀬, `type_checker_builtins_nominal.inc`瑜?659 LOC源뚯? 異뺤냼
  - 吏꾪뻾: expression operator/indexed-access checker瑜?`type_checker_expr_ops.c`濡?遺꾨━?섍퀬, static member path / consumed-boundary helper瑜?`type_checker_expr_names.c`濡??대룞?덈떎. `type_checker_expr.inc`??758 LOC, `type_checker_helpers_late.inc`??773 LOC媛 ?섏뼱 ????semantic 800 LOC stop condition ?꾨옒濡??대젮媛붾떎
  - 吏꾪뻾: event declaration/subscription/invoke semantic? `type_checker_event.c`濡??밴꺽?덇퀬, QubitSlot compile-time state / entangle pool / movable-resource-use validation? `type_checker_qubit.c`濡??밴꺽?덈떎. `type_checker.c`??481 LOC濡??대젮媛 600 LOC ?댄븯 stop condition??留뚯”?쒕떎
  - 吏꾪뻾: domain slot/projection/overlay helper body瑜?`type_checker_decls_domain_helpers.c`濡??밴꺽?섍퀬, intent inheritance/derivation helper body瑜?`type_checker_intent_helpers.c`濡??밴꺽?덈떎. `type_checker_decls_domain_helpers.inc`???쒓굅?먭퀬 `type_checker_decls_a.inc`??1-line forwarding stub?쇰줈 異뺤냼
  - ?꾨즺: semantic `.inc` 800 LOC stop condition? `make semantic-inc-size-test-smoke`濡?怨좎젙. ?꾩옱 `src/semantic`?먮뒗 800 LOC 珥덇낵 `.inc`媛 ?녿떎
  - ?꾨즺: semantic core shape stop condition? `make semantic-core-shape-test-smoke`濡?怨좎젙. `type_checker.c <= 600 LOC`, event/qubit owner TU, DAG inventory `.c` ownership??CI?먯꽌 寃?ы븳??  - 吏꾪뻾: C backend MIR inventory/SSA emitter include瑜?5-line shim + `transpiler_emitters_mir_inventory_intent.inc` / `transpiler_emitters_mir_inventory_ssa_names.inc` / `transpiler_emitters_mir_inventory_ssa_emit.inc`濡?遺꾨━???대떦 debt瑜?紐⑤몢 1,000 LOC ?꾨옒濡???톬??  - 吏꾪뻾: C backend `emit_program(...)` bootstrap? direct declaration-array reads ???`transpiler_active_inventory(...)` / `transpiler_active_externs(...)` view瑜??뚮퉬?쒕떎. `make mir-declaration-inventory-test-smoke`媛 C/LLVM declaration-side codegen??raw declaration inventory access瑜?helper owner濡??쒗븳?쒕떎
  - 吏꾪뻾: C backend expression emitter include瑜?7-line shim + `transpiler_expr_emitters_builtins.inc` / `transpiler_expr_emitters_call_a.inc` / `transpiler_expr_emitters_call_b.inc` / `transpiler_expr_emitters_members.inc` / `transpiler_expr_emitters_tail.inc`濡?遺꾨━???대떦 debt瑜?紐⑤몢 1,000 LOC ?꾨옒濡???톬?? 寃利? `make test-transpile -j2`, `make llvm-test-backend-compare -j2`
  - 吏꾪뻾: LLVM call emitter include瑜?17-line shim + `llvm_expr_call_constructors.inc` / `llvm_expr_call_arrays.inc` / `llvm_expr_call_collections_base.inc` / `llvm_expr_call_domain_queries.inc` / `llvm_expr_call_events.inc` / `llvm_expr_call_intent_observability.inc` / `llvm_expr_call_log.inc` / `llvm_expr_call_math.inc` / `llvm_expr_call_result_option.inc` / `llvm_expr_call_slots.inc` / `llvm_expr_call_task_channel.inc` / `llvm_expr_calls_part_a.inc` / `llvm_expr_calls_part_b.inc` / `llvm_expr_calls_part_c.inc` / `llvm_expr_calls_part_d.inc`濡?遺꾨━???대떦 debt瑜?紐⑤몢 1,000 LOC ?꾨옒濡???톬?? enum/class constructor, array builtin, `ListNew`/`Set*` base collection, domain query builtin, event invocation, intent observability, log, scalar math, Result/Option, slot/device-slot builtin, task/channel lowering? `llvm_emit_call`?먯꽌 遺꾨━?섏뼱 蹂꾨룄 owner include媛 ?먮떎. 寃利? `make test-transpile -j2`, `make backend-inc-size-test-smoke`, `make llvm-test-backend-compare -j2`
  - 吏꾪뻾: C backend base emitter B include瑜?6-line shim + `transpiler_emitters_base_b_part_a.inc` / `transpiler_emitters_base_b_part_b.inc` / `transpiler_emitters_base_b_part_c.inc` / `transpiler_emitters_base_b_part_d.inc`濡?遺꾨━???대떦 debt瑜?紐⑤몢 1,000 LOC ?꾨옒濡???톬?? 寃利? `make test-transpile -j2`, `make llvm-test-backend-compare -j2`
  - ?꾨즺: Tier 1 runtime/codegen/compiler `.inc > 1000 LOC` gate???ロ옒. `pgy_runtime_part_ba.inc`, `pgy_runtime_lib_part_b.inc`, `transpiler_emitters_base_a.inc`, `transpiler_helpers_core_a.inc`, `transpiler_helpers_core_b.inc`, `transpiler_domain_role.inc`, `llvm_expr_helpers.inc`, `mir_public.inc`, `llvm_expr_call_methods.inc`, `llvm_domain_helpers.inc`瑜?紐⑤몢 safe mechanical split?쇰줈 1,000 LOC ?꾨옒濡???톬??  - ?꾨즺: `tests/backend_inc_size_smoke.sh` / `make backend-inc-size-test-smoke` 異붽?. `src/runtime`, `src/codegen`, `src/compiler`??`.inc <= 1000 LOC`瑜?CI?먯꽌 怨좎젙
  - 寃利? `make backend-inc-size-test-smoke`, `make test-mir test-transpile test-abi -j2`, `make llvm-test-backend-compare -j2`
  - 吏꾪뻾: `type_checker_helpers_late.c` standalone TU 鍮뚮뱶 以??쒕윭??call-path helper include-order ?섏〈??`type_checker_internal.h` prototype怨?吏곸젒 include 怨꾩빟?쇰줈 怨좎젙?덈떎
  - 吏꾪뻾: `type_checker_decls_a.inc -> type_checker_decls_domain_helpers.inc`, `type_checker_decls_intent.inc -> type_checker_world_decl.c`, `type_checker_helpers_effects.inc -> type_checker_helpers_host.inc` ?ъ씠 dangling return-type seams ?쒓굅
  - 吏꾪뻾: `type_checker_resolution_graph_core.inc` ??inventory include 寃쎄퀎??dangling `static void` seam 2媛쒕? 紐낆떆 return type?쇰줈 ?뺣━
  - 吏꾪뻾: `generic_params_required_count`??include-order static helper?먯꽌 `type_checker_internal.h` internal API濡??밴꺽
  - ?꾨즺: required ability resolver? action required-ability validator??`type_checker_module_contract.c`濡??ㅼ젣 TU 遺꾨━ ?꾨즺
  - ?꾨즺: `type_checker_module_contracts.inc` ?쒓굅. module contract include-order 援ъ“ debt???ロ옒
  - [ ] `.inc` ?대? static helper 以?援먯감 李몄“ ?ы븳 ?щ낵 紐⑸줉 ?묒꽦
  - [x] include-order???섏〈?섎뒗 implicit declaration 寃쎈줈 ?쒓굅瑜?鍮뚮뱶 怨꾩빟?쇰줈 ?밴꺽 (`-Werror=implicit-function-declaration`, `-Werror=implicit-int`)
  - [~] declaration-side MIR-only debt??helper-gated state源뚯? ?ロ삍?? ?⑥? ?④퀎??`MIRProgram` ??AST-shaped declaration inventory瑜?dedicated declaration metadata model濡?遺꾨━?섎뒗 ?쇱씠??
  - 吏꾪뻾: ownership return / assignment rebind / array literal store / boundary validation / call argument / destructuring / let-binding / parameter escape-summary consumers??`.inc`?먯꽌 ?ㅼ젣 TU濡??밴꺽?덈떎. ??젣???뚯씪: `type_checker_ownership_return.inc`, `type_checker_ownership_assign.inc`, `type_checker_ownership_array_store.inc`, `type_checker_ownership_boundaries.inc`, `type_checker_ownership_call.inc`, `type_checker_ownership_destructure.inc`, `type_checker_ownership_destructure_stmt.inc`, `type_checker_ownership_let.inc`, `type_checker_ownership_let_boundary.inc`, `type_checker_ownership_let_claim.inc`, `type_checker_ownership_let_infer.inc`, `type_checker_ownership_let_slot.inc`, `type_checker_ownership_let_value.inc`, `type_checker_ownership_param_summary.inc`. ?꾩옱 `src/semantic/type_checker_ownership_*.inc`??0媛쒕떎
  - ?먯튃 媛뺥솕: 踰좏? 湲곗??먯꽌??behavior-owning `.inc`瑜?beta+1 ?뺣━媛 ?꾨땲??blocker濡?蹂몃떎. generated table / local macro table / private test fixture ??`.inc`??owner `.c` ?먮뒗 紐낆떆??generated artifact濡???릿??  - ?먯튃 媛뺥솕: `.inc` ?쒓굅 怨쇱젙?먯꽌 ?щ윭 behavior family瑜??섎굹??mega-TU濡??⑹튂吏 ?딅뒗?? `make semantic-tu-size-test-smoke`媛 ??semantic owner TU??1,000 LOC ?댄븯濡??쒗븳?섍퀬, 湲곗〈 珥덈???TU??媛쒕퀎 cap?쇰줈 ??而ㅼ?吏 紐삵븯寃?留됰뒗??  - ?⑥? ?꾪뿕 seam: `type_checker_builtins_query.h`??`type_checker_builtins_slotops.h`? `BuiltinKind builtin_resolve(...)` ?쒓렇?덉쿂媛 include-chain?쇰줈 遺숈뼱 ?덈떎. query/slot/nominal builtin? dispatcher contract瑜?癒쇱? 遺꾨━????TU濡??щ┛??
### P10. ?띾룄 / 鍮뚮뱶 ?깅뒫 baseline

- 臾몄젣:
  - ?κ린 紐⑤뱢?붽? translation unit ?섎? ?섎━硫?incremental build??醫뗭븘吏????덉?留?full build/link ?먮뒗 generated backend compile ?쒓컙???????덈떎
  - ?꾩옱 `test-abi-perf`??議댁옱?섏?留?raw log媛 湲몄뼱 worst-case 異붿쟻???대졄??- 湲곕낯 諛⑹묠:
  - `make test-abi-perf`濡?benchmark-only ABI/runtime baseline??罹≪쿂?쒕떎
  - `make perf-summary PERF_LOG=<log>`濡?C/LLVM compile/run ?됯퇏怨?worst-case瑜??붿빟?쒕떎
  - representative case??`tests/bench_backend.sh <source.pgy> dev`濡?C/LLVM wall time + RSS瑜?吏곸젒 ?뺤씤?쒕떎
  - generated/native compile warning? ?띾룄 noise媛 ?꾨땲??build hygiene bug濡?蹂닿퀬 利됱떆 ?ル뒗??- ?꾩옱 baseline (2026-04-24, local WSL):
  - `make test-abi-perf`: 320 passed, 0 failed
  - `perf-summary`: C 32 cases, avg compile 0.569s, max 1.783s (`intent_authority_snapshot_abi`), avg run 0.001s
  - `perf-summary`: LLVM 32 cases, avg compile 0.187s, max 0.251s (`projection_abi`), avg run 0.002s
- 吏꾪뻾: `make perf-contract-test-smoke`媛 synthetic `test-abi-perf` log瑜??듯빐 `perf_summary` log grammar, C/LLVM case count, average compile/run, worst-case compile/run case selection??CI?먯꽌 怨좎젙?쒕떎. ??gate??baseline ?レ옄 ?먯껜瑜?怨좎젙?섏? ?딄퀬, perf evidence媛 machine-readable ?곹깭瑜??좎??섎뒗吏 寃?ы븳??
  - representative `relation_effect_propagation/main.pgy`: C dev 1.03s / 46MB, LLVM dev 0.72s / 60MB after `realpath` warning fix
- 吏꾪뻾:
  - [x] `tests/perf_summary.sh` 異붽?
  - [x] `make perf-summary PERF_LOG=<log>` 異붽?
  - [x] generated C/LLVM compile path??POSIX `realpath` implicit declaration 寃쎄퀬 ?쒓굅
- ?⑥쓬:
  - [ ] CI?먯꽌 benchmark-only ?섏튂瑜?artifact濡???ν븷吏 寃곗젙
  - [ ] release/beta notes??perf-summary baseline 泥⑤?
  - [ ] worst-case compile 2諛??댁긽 利앷? ??regression ?꾨낫濡??먮룞 ?쒖떆

### P2. hint namespace ?덉??ㅽ듃由ы솕

- 臾몄젣:
  - `cause_ir` / `fix_source` literal???몄뀡 ?⑥쐞濡?怨꾩냽 ?섏뼱?섎뒗??以묒븰 ?덉??ㅽ듃由ш? ?녿떎
  - `docs/72`瑜?臾몄꽌??`code` ?꾩＜怨? `cause_ir` / `fix_source` variant drift瑜?媛뺤젣?섏? 紐삵븳??- ?곹뼢:
  - downstream??diagnostic routing????媛믪쓣 ?곌린 ?쒖옉?섎㈃ ?ㅽ?/drift媛 利됱떆 breaking change媛 ?쒕떎
- 湲곕낯 諛⑹묠:
  - `code`, `cause_ir`, `fix_source`瑜?紐⑤몢 registry/enum-like literal set?쇰줈 愿由?  - 臾몄꽌? 肄붾뱶 由щ럭 湲곗??먯꽌 ?쒖깉 literal 異붽? ??registry + docs ?숈떆 媛깆떊?앹쓣 媛뺤젣
- 以鍮??묒뾽:
  - [x] diagnostic literal registry 珥덉븞 異붽?
    - ?꾨즺: `src/semantic/diag_codes.h`媛 `PGY_CODE_*`, `PGY_CAUSE_*`, `PGY_FIX_*` registry source of truth濡??숈옉?섍퀬 `docs/72_diagnostic_codes.md`媛 ?대? 臾몄꽌??  - [x] `cause_ir` / `fix_source` ?ㅼ씠諛?洹쒖튃 臾몄꽌??    - ?꾨즺: `docs/72_diagnostic_codes.md`??`cause_ir` stage/subsystem/condition 洹쒖튃怨?`fix_source` source-action token 洹쒖튃 怨좎젙
  - [x] free-form 臾몄옄???좉퇋 異붽? 吏?먯뿉 smoke gate 留덈젴
    - ?꾨즺: `tests/diagnostic_registry_smoke.sh` / `make diagnostic-registry-test-smoke`媛 semantic diagnostic call-site??`PGY_CODE_*`, `PGY_CAUSE_*`, `PGY_FIX_*` macro ?ъ슜怨?diagnostic code 臾몄꽌 sync瑜?寃??
### P3. ???ownership ?⑹뼱 ?뺤텞

- 臾몄젣:
  - anchored handle / movable resource / subject / subject-host / boundary value / capability-bearing / move token ???⑹뼱媛 怨쇰떎
  - 媛숈? semantic family媛 硫붿떆吏留덈떎 ?ㅻⅨ ?대쫫?쇰줈 ?몄텧?쒕떎
- ?곹뼢:
  - ?ъ슜?먮룄 ?룰컝由ш퀬, 援ы쁽?먮룄 硫붿떆吏/臾몄꽌/?뚯뒪???뺣젹 ??drift媛 ?쒕떎
- 湲곕낯 諛⑹묠:
  - ?ъ슜??facing ?듭떖 ?⑹뼱瑜?2-3痢듭쑝濡??뺤텞
  - ?몃? 遺꾨쪟???쏼???섏쐞遺꾨쪟?앸줈留??몄텧
- 以鍮??묒뾽:
  - [ ] user-facing canonical vocabulary ?뺣━
  - [ ] diagnostics/README/docs ?⑹뼱 留ㅽ븨???묒꽦
  - [ ] old wording grep inventory ??移섑솚 怨꾪쉷 ?섎┰

### P4. 鍮뚮뱶/?뚮뱶諛뺤뒪 寃쎈줈 ?⑥닚??
- 臾몄젣:
  - bash / PowerShell / cmd / MSYS2 / stale object / path rewrite / sed 湲곕컲 stamp媛 ?쒕줈 ?ㅻⅨ 諛⑹떇?쇰줈 源⑥쭊??  - ?쏯othing to be done??+ stale artifact 媛숈? ?뚭?媛 ?앹궛?깆쓣 ?ш쾶 源롫뒗??  - smoke test媛 repo root??runtime artifact瑜??④린硫?dirty worktree? ?ㅼ젣 ?뚯뒪 蹂寃쎌쓣 援щ텇?섍린 ?대젮?뚯쭊??- 湲곕낯 諛⑹묠:
  - ?⑥씪 怨듭떇 鍮뚮뱶 寃쎈줈瑜??뺥븯怨??섎㉧吏??document-only ?먮뒗 best-effort濡??대┛??  - stale artifact ?뚰뵾瑜??꾪빐 媛뺤젣 ?щ퉴??寃쎈줈瑜?怨듭떇??- 以鍮??묒뾽:
  - [x] 怨듭떇 Windows 鍮뚮뱶 寃쎈줈 1媛쒕줈 臾몄꽌??    - 湲곗?: GitHub Actions `windows-latest` + `msys2/setup-msys2` native MinGW/MSYS2 runtime
    - plain Linux-hosted `gcc`??`ci-windows` acceptance line???꾨떂
  - [x] `llvm_smoke.sh`??`string_io` smoke媛 repo root??`io.txt`瑜??④린吏 ?딅룄濡?媛?case瑜?source directory?먯꽌 ?ㅽ뻾?섍쾶 ?뺣젹
  - [x] LLVM runtime object freshness媛 split runtime `.inc` subpart 蹂寃쎌쓣 蹂대룄濡?`compiler_runtime_cache_is_fresh(...)` dependency list瑜??뺤옣. `pgy_runtime_lib_part_b_part_d.inc` 媛숈? ?섏쐞 include ?섏젙 ??stale runtime object媛 留곹겕?섎뒗 臾몄젣瑜?李⑤떒
  - [ ] `clean && build` 媛뺤젣 wrapper / recommended entrypoint ?뺤쓽
  - [ ] stale `.o` / `.d` 吏꾨떒 媛?대뱶? 媛뺤젣 ?щ퉴???듭뀡 ?뺣━

### P5. printf-style 吏꾨떒 ?щ㎎??異뺤냼

- 臾몄젣:
  - ?쇰? semantic diagnostic helper???몄옄 媛쒖닔媛 留ㅼ슦 留롪퀬, placeholder drift??痍⑥빟?섎떎
  - ?꾩옱 援ъ“??`fmt ?섎뱶肄붾뵫 + structured tags(code/cause/fix)`媛 ?댁쨷?쇰줈 怨듭〈?쒕떎
- 湲곕낯 諛⑹묠:
  - 吏꾨떒 payload瑜?struct濡?紐⑥쑝怨? human-readable render??renderer/helper layer媛 ?대떦
  - 理쒖냼??怨좎씤??helper遺??payload-builder ?⑦꽩?쇰줈 ?꾪솚
- 以鍮??묒뾽:
  - [ ] high-arity diagnostic helper inventory ?묒꽦
  - [ ] generic mismatch / authority mismatch / ownership escape?먯꽌 payload struct ?쒕쾾 ?꾩엯

### P6. channel transport 洹쒖튃 怨듯넻 validator ?섎졃

- 臾몄젣:
  - `type_checker_async_channel.h`? builtin/send-query 怨꾩뿴??ownership/channel transport 洹쒖튃??以묐났 援ы쁽?쒕떎
- 湲곕낯 諛⑹묠:
  - channel transport??怨듯넻 validator ?섎굹濡??섎졃
  - builtin/send wrappers??surface adapter留??대떦
- 以鍮??묒뾽:
  - [x] send/try-send/send-timeout/status variants 怨듯넻 validator 異붿텧
  - [ ] subject / movable / anchored / boundary mismatch wording ?듭씪
  - 吏꾪뻾: named-transfer requirement? subject/boundary/anchored borrowed-send/mismatch??`semantic_channel_transfer_requires_named_binding(...)`, `semantic_report_named_channel_transfer_required(...)`, `semantic_validate_channel_transport_ownership(...)` helper濡?1李??섎졃
  - 吏꾪뻾: token / move-only send-recv restriction wording??`semantic_report_channel_transport_policy(...)` helper濡??뺣젹 ?쒖옉
  - 吏꾪뻾: validator/reporting 援ы쁽? `type_checker_async_channel.h`?먯꽌 ?쒓굅?섍퀬 `type_checker_channel_transport.c`媛 source of truth媛 ?먮떎

### P7. 以묎컙 stage JSON routing closure

- 臾몄젣:
  - HIR/DIR/RIR/MIR ?ㅽ뙣 寃쎈줈 ?쇰?媛 ?ъ쟾??plain text 以묒떖?대씪 `?⑥씪 JSON 諛곗뿴` 怨꾩빟??源⑤쑉由곕떎
- 湲곕낯 諛⑹묠:
  - frontend/backend ?앸떒肉??꾨땲??以묎컙 stage ?ㅽ뙣??structured output 怨꾩빟???ㅼ뼱?ㅺ쾶 ?쒕떎
- 以鍮??묒뾽:
  - [ ] HIR/DIR/RIR/MIR failure emitter inventory ?묒꽦
  - [ ] plain-text fallback ?쒓굅 ?곗꽑?쒖쐞 ?섎┰

### P8. stale binary / artifact ?뚭? 怨좎젙

- 臾몄젣:
  - stale object/dependency ?뚯씪 ?뚮Ц???뚯뒪 ?섏젙??諛섏쁺?섏? ?딅뒗 寃쎌슦媛 ?덈떎
- 湲곕낯 諛⑹묠:
  - ?쒕튌瑜?利앸텇 鍮뚮뱶?앸낫???쒖떊猶?媛?ν븳 ?щ퉴?쒋?寃쎈줈瑜??곗꽑
- 以鍮??묒뾽:
  - [ ] stale artifact ?ы쁽 議곌굔 臾몄꽌??  - [ ] 沅뚯옣 鍮뚮뱶 吏꾩엯?먯뿉??clean rebuild ?좏깮吏瑜?湲곕낯 ?몄텧

### P9. arena ?⑦꽩 紐낆떆 ?꾩엯

- 臾몄젣:
  - transpiler / semantic / diagnostics / type rendering 寃쎈줈???꾩떆 臾몄옄??踰꾪띁 churn??留롫떎
  - `malloc/free`? context-lifetime scratch allocation???욎뿬 ?덉뼱, early-return/fail path?먯꽌 ?뚯쑀沅뚯씠 ?곕컻?곸씠??  - cache? ?꾩떆 臾몄옄?댁씠 ?욎씠硫?dangling ?먮뒗 怨쇰룄??copy churn ?꾪뿕??而ㅼ쭊??- 湲곕낯 諛⑹묠:
  - arena??紐낆떆?곸쑝濡??꾩엯?쒕떎
  - ?? ?꾨㈃ 移섑솚???꾨땲??`scratch arena`? `result arena`瑜??섎챸 湲곗??쇰줈 遺꾨━?쒕떎
  - cache / long-lived metadata / AST-owned field?먮뒗 arena-owned ?ъ씤?곕? ??ν븯吏 ?딅뒗??  - arena 媛?援먯감 李몄“??raw pointer蹂대떎 `index` / stable handle 李몄“瑜?湲곕낯?쇰줈 ?쒕떎
  - arena??理쒖냼??`transpiler`, `semantic scratch`, `semantic result`, ?꾩슂 ??`type/render scratch`泥섎읆 ??븷/?섎챸蹂꾨줈 遺꾨━?쒕떎
  - ?????븷蹂?arena 遺꾨━???쒕늻媛 free?섎뒓?먥앸낫???쒖뼵??reset?섎뒓?먥앸? 湲곗??쇰줈 ?ㅺ퀎?쒕떎
  - 泥??④퀎??transpiler / semantic diagnostics / type render helper??scratch allocation ?섎졃?대떎
- ??寃곗젙??留욌뒗 ?댁쑀:
  - ?꾩옱 肄붾뱶踰좎씠?ㅻ뒗 long-lived cache? short-lived formatting string??媛뺥븯寃??욎뿬 ?덉뼱, raw pointer 怨듭쑀蹂대떎 index 李몄“媛 ?⑥뵮 ?덉쟾?섎떎
  - Pergyra??early-return/fail path? pass-local scratch data媛 留롮븘?? ?⑥씪 arena蹂대떎 ??븷/?섎챸蹂?arena 遺꾨━媛 ?붾쾭源낃낵 reset 鍮꾩슜 硫댁뿉???ル떎
  - 利? `Arena + Index 李몄“ + ??낅퀎 arena 遺꾨━`媛 吏湲?援ъ“ debt瑜?以꾩씠??媛??蹂댁닔?곸씠怨??덉젙?곸씤 諛⑺뼢?대떎
- 以鍮??묒뾽:
  - [x] `scratch arena` / `result arena` lifetime 洹쒖튃 臾몄꽌??  - [x] arena 媛?cross-reference瑜?`index` / stable handle 湲곗??쇰줈 臾몄꽌??  - [x] `TranspilerCtx` scratch arena ?곸슜 踰붿쐞 ?뺤젙
  - [x] semantic analyze pass??scratch arena ?꾩엯 吏???뺣━
  - [x] diagnostic payload/result-owned arena 遺꾨━ ?щ? 寃곗젙
  - [x] ?????븷蹂?arena 遺꾪븷??珥덉븞 ?묒꽦
  - [x] `strdup_fmt` / type render / projection path / generic formatter helper??arena ?꾪솚 ?곗꽑?쒖쐞 ?묒꽦
  - [x] cache??arena-owned ?ъ씤?????湲덉? 洹쒖튃 臾몄꽌??  - [x] 泥?vertical slice:
    - transpiler temporary strings
    - semantic diagnostic formatting scratch strings
    - type-name rendering scratch helpers
  - 吏꾪뻾: `docs/94_arena_index_lifetime_plan.md`濡?諛⑺뼢 怨좎젙
  - 吏꾪뻾: `TranspilerCtx`??`arena`瑜?scratch arena濡?紐낆떆
  - 吏꾪뻾: transpiler scratch-only temporary 1李?vertical slice ?꾨즺
    - zone authority temporary expression
    - intent priority default literal
    - projection refresh `source_expr`
    - event declaration `event_type`
  - 吏꾪뻾: semantic diagnostics result seam 1李??꾩엯
    - `Diagnostic`媛 optional payload snapshot??蹂댁〈
    - payload emit 寃쎈줈??result-owned snapshot?쇰줈 蹂듭궗
    - semantic JSON 異쒕젰??payload ?꾨뱶瑜??④퍡 ?몄텧 媛??  - 吏꾪뻾: semantic scratch arena 1李??꾩엯
    - `SemanticContext`??scratch arena 異붽?
    - ownership diagnostic path string? scratch arena瑜??곗꽑 ?ъ슜
    - payload snapshot??result濡?蹂듭궗?섎?濡?helper ?대? free churn ?쒓굅
  - 吏꾪뻾: LLVM arena lane 1李?closure
    - `LLVMGenCtx`??`scratch` + `persistent` lane?쇰줈 遺꾨━
    - `LLVMGenResult`??result-owned arena瑜?蹂댁쑀
    - intent MIR collector / projection path / local grow helper / event invoke / type render helper媛 scratch濡??섎졃
    - synthetic event-handler AST field ??μ? callable signature registry濡?移섑솚
    - `*error_message` heap return contract??result-owned lane?쇰줈 ?섎졃
    - ?⑥? heap 寃쎄퀎??owner shell(`ctx`, registry destroy, result outer shell)怨?runtime ABI contract ?섏??쇰줈 異뺤냼
    - 吏꾪뻾: intent observability(`last/history/active/recent`)? authority failure snapshot??stable runtime string exports??`runtime-borrowed string` ABI濡?怨좎젙?덈떎. caller??free?섏? ?딄퀬 ?ㅼ쓬 runtime registry/snapshot mutation ?꾧퉴吏留??좏슚?섎떎
    - 吏꾪뻾: `runtime-abi-lifetime-test-smoke`媛 stable intent last/history/active/recent 諛?authority 臾몄옄??export body?먯꽌 allocation/free/strdup??諛쒖깮?섏? ?딅룄濡?寃?ы븳??    - 吏꾪뻾: stable string helper returns??`result-owned string`, stable string-array helper returns??`result-owned array` ABI濡?怨좎젙?덈떎. `runtime-abi-lifetime-test-smoke`媛 helper payload媛 borrowed input pointer, stack buffer, string literal??諛섑솚?섏? ?딄퀬 allocation/copy??payload瑜?諛섑솚?섎뒗吏 寃?ы븳??    - 吏꾪뻾: stable file descriptor??`runtime-owned handle` ABI濡?怨좎젙?덈떎. `pgy_file_open`? ?ロ엺 runtime table slot???ъ궗?⑺븯怨? `pgy_file_close`??table entry瑜?NULL濡?鍮꾩썙 ?ъ궗??媛???곹깭濡?留뚮뱺?? `runtime-abi-lifetime-test-smoke`媛 ??release/reuse contract瑜?寃?ы븳??    - ?⑥쓬: file descriptor ??runtime-owned handle ownership??媛숈? ?섏???smoke/臾몄꽌 怨꾩빟?쇰줈 ?뺤옣?댁빞 ?쒕떎
  - 二쇱쓽: 諛섑솚 怨꾩빟???덈뒗 expression string? ?꾩쭅 arena濡???린吏 ?딆쓬
  - 二쇱쓽: `slot_ref_expr(...)` scratch ?꾪솚 ?쒕룄???섎룎由? 諛섑솚 ownership 寃쎄퀎瑜?癒쇱? ?섎닠????
### 理쒓렐 closure 吏꾪뻾 (2026-04-18)

- declaration-side MIR-only host context瑜????뺣━
  - transpiler host context媛 `current_host_decl -> within_zone -> saved host-name inventory` ?쒖쑝濡?蹂듭썝?섎룄濡??뺣젹
  - class/zone/relation/effect/world field query helper媛 raw host-name state蹂대떎 inventory-backed host handle???곗꽑 ?ъ슜
  - direct `current_*_name` restore chain ?쇰?瑜?`transpiler_restore_host_context_local(...)` helper濡??묒뼱 ?곕컻??context 蹂듦뎄 肄붾뱶瑜?異뺤냼
  - emitter hot path??direct `current_*_name` 李몄“???遺遺?嫄룹뼱?닿퀬, ?⑥? ?ъ슜泥섎? helper/restore layer濡?援?냼??  - LLVM declaration helper??current host lookup??怨듭슜 active-inventory host helper濡??묒뼱 naming chain??異뺤냼
  - LLVM MIR/domain emission??direct `current_class_name` save/restore??host-name bind/restore helper濡??묒뼱 state 愿由?以묐났??以꾩엫
  - LLVM expr/stmt hot path??`llvm_current_host_decl_name(...)` 湲곗??쇰줈 ?뺣젹??direct raw host-name read瑜???以꾩엫
  - `HasProjection/HasLayer/HasState/HasZone*` 諛?method/field helper媛 raw `current_class_name` ???host helper瑜??듦낵?섎룄濡??뺣━
  - LLVM pipeline??nominal registration / class method emission??raw nominal AST array蹂대떎 `mir->decl_headers`瑜?吏곸젒 ?쒗쉶?섎룄濡??뺣젹
  - LLVM domain pass??raw `ctx->mir->{relations,effects,zones,...}` 吏곸젒 ?묎렐 ???`llvm_active_domain_inventory(...)` helper瑜??듦낵?섎룄濡??뺣젹
  - 利? declaration-side debt???댁젣 emitter 蹂몃Ц蹂대떎 inventory bootstrap + helper/restore layer 援?냼 遺?꾨줈 ???뺤텞??  - C transpiler domain/hosted method emission??`emit_hosted_methods_from_mir_or_error_local(...)` helper濡??섎졃
  - party / roster / relation / effect / zone / world method emit??媛숈? MIR routine gate? 媛숈? explicit backend error ?뺤콉???ъ슜
  - relation/effect/zone/world method??dead AST signature fallback ?쒓굅
  - party / roster / relation / effect / zone / world declaration emit entrypoint??inventory decl???곗꽑 ?ъ슜
  - bootstrap residual? ?댁젣 per-domain AST array 吏곸젒 ?쒗쉶蹂대떎 inventory-backed bootstrap helper 蹂몄껜 履쎌쑝濡????뺤텞
- generic contract + type-resolution DAG ?뚭?瑜????볧옒
  - `role impl ability` 寃쎈줈媛 generic default/where-bound cycle provenance regression??異붽???  - 利? action/intent-step/zone-authority/party-role-slot???뷀빐 role impl consumer??staged DAG path ?뚭? 踰붿쐞???ы븿
- ?꾩옱 寃利앹꽑
  - `test-semantic`: `1617 passed, 0 failed`
  - `test-transpile`: `670 passed, 0 failed`
  - `test-abi`: `84 passed, 0 failed`
  - `ci-linux`: full green ?좎?
  - LLVM expr/stmt host-helper ?뺣━ ?댄썑?먮룄 `test-transpile`, `test-abi` ?ы넻怨??뺤씤

### 理쒓렐 closure 吏꾪뻾 (2026-04-24)

- runtime propagation/provenance 1李?closure
  - C/LLVM domain hidden cell??`ready/dirty` bool留?媛吏???곹깭?먯꽌 `epoch/cause` provenance cell源뚯? 媛숈? schema濡??뺤옣??  - relation/effect/zone/world projection, layer, state, world-derived state媛 recompute ?쒖젏??cause-stamped provenance瑜??④린?꾨줉 C/LLVM???뺣젹??  - LLVM domain struct layout??洹몃룞??鍮좊쑉由ш퀬 ?덈뜕 `__projection_dirty_*` field瑜?relation/effect/zone???ㅼ떆 ?ы븿?섎룄濡?parity ?섏젙
  - LLVM projection sync??C? 媛숈? dirty-gated recompute 寃쎈줈濡??뺣젹??  - LLVM host-field assignment媛 zone/relation/effect host method ?덉뿉??projection invalidation??留뚮뱾?꾨줉 蹂듦뎄
  - LLVM intent step rebound-zone 寃쎈줈??effective zone projection cell??蹂댁닔?곸쑝濡?dirty-mark + sync ?섎룄濡?蹂닿컯
  - 寃곌낵: `relation_effect_propagation_abi`, `intent_zone_binding`, `intent_cross_world_transfer`, `intent_rich_history_identity` backend compare drift ?쒓굅
  - ???뚭?: transpile domain async/world tests媛 provenance hidden field? stamp write源뚯? 吏곸젒 ?뺤씤
  - ??吏꾪뻾: `world` derived-state recompute媛 C/LLVM ?묒そ?먯꽌 bounded pass loop瑜?媛吏?꾨줉 ?щ씪?붽퀬, single-pass declaration-order replay?먮쭔 ?섏〈?섏? ?딄쾶 ??  - ??吏꾪뻾: bounded recompute pass-limit overflow??C??`PGY_PANIC`怨?LLVM??`abort()` 寃쎈줈濡?hard-fail?섎룄濡?怨좎젙??- ???뚭?: transpile world-derived chain test + `world_fixpoint_abi` smoke媛 C/LLVM ?묒そ?먯꽌 ?뱀깋
- ?꾩옱 ?댁꽍: runtime propagation provenance baseline(`dirty/ready + epoch/cause`)? ?댁젣 beta 怨꾩빟???쇰?濡?媛꾩＜?섍퀬 ?ㅼ떆 ?쏀솕?쒗궎吏 ?딆쓬
- 異붽? closure: zone lifecycle sync???댁젣 C/LLVM ?묒そ?먯꽌 bounded frontier loop瑜?媛吏硫? state/layer replay媛 single-batch?먮쭔 臾띠씠吏 ?딅뒗??- 異붽? closure: embedded world-zone source assignment???댁젣 projection dirty mark ?ㅼ뿉 媛숈? turn??zone sync瑜??쒖썙 stale `ready/value` drift ?놁씠 projection recompute瑜??ル뒗??- 異붽? ?뚭?: `world_embedded_projection_abi`, `world_embedded_method_projection_abi`, `world_embedded_branch_projection_abi`媛 C/LLVM ABI smoke?먯꽌 ?뱀깋?대ŉ embedded zone projection read-after-mutate path瑜?straight-line assignment, method-call, branch-join slice源뚯? ?좉렐??- 異붽? ?뚭?: `handoff_projection_frontier_abi`媛 C/LLVM ABI smoke?먯꽌 ?뱀깋?닿퀬 `handoff_projection_frontier`媛 backend-compare?먯꽌 ?뱀깋?대떎. v1 handoff materialization ?댄썑 source projection? source snapshot?? target projection? target mutation 寃곌낵瑜?蹂대룄濡??좉렐??- 異붽? ?뚭?: `handoff_world_state_frontier_abi`? `handoff_world_state_frontier`媛 C/LLVM?먯꽌 ?뱀깋?대떎. active world-owned zone??`transfer:` ??곸쑝濡??섍릿 ??projection-backed world state? `all` composed state媛 媛숈? tick?먯꽌 fresh?섍쾶 蹂댁씠??理쒖냼 frontier瑜??좉렐??- 異붽? ?뚭?: `handoff_layer_state_frontier_abi`? `handoff_layer_state_frontier`媛 C/LLVM?먯꽌 ?뱀깋?대떎. `transfer:` ?댄썑 action-caused effect媛 target zone layer/state? active world-derived layer/state alias源뚯? 媛숈? tick?먯꽌 fresh?섍쾶 ?꾪뙆?섎뒗 寃쎈줈瑜??좉렐??- 異붽? ?뚭?: `world_embedded_action_frontier_abi`? `world_embedded_action_frontier`媛 C/LLVM?먯꽌 ?뱀깋?대떎. embedded world-zone subject action call??action-caused effect layer/state? active world-derived layer/state alias源뚯? 媛숈? tick?먯꽌 fresh?섍쾶 ?꾪뙆?섎뒗 寃쎈줈瑜??좉렐??- 異붽? ?뚭?: `world_embedded_action_pool_frontier_abi`? `world_embedded_action_pool_frontier`媛 C/LLVM?먯꽌 ?뱀깋?대떎. embedded world-zone subject action call??fixed-capacity effect pool 寃쎈줈??媛숈? frontier 怨꾩빟?쇰줈 ?좉렐??- 媛뺥븳 ?⑥? 怨쇱젣: full bounded fixpoint / transitive frontier scheduler??**紐낆떆??beta blocker**濡??좎?. ?ㅻ쭔 ?⑥? debt??zone/world frontier loop??遺?ш? ?꾨땲??remaining authority/failure handoff family? ???볦? world-zone propagation family瑜?媛숈? source-of-truth濡??쇰컲?뷀븯???쇱씠??- 異붽? closure: relation/effect/zone projection sync??bounded transitive recompute loop濡??щ씪?붽퀬 declaration order??湲곕?吏 ?딅뒗??- 異붽? ?뚭?: `projection_chain_abi`媛 C/LLVM ABI smoke, `make test-all`, `make llvm-test-backend-compare`?먯꽌 ?좉꼈??- 異붽? gate: `make runtime-frontier-contract-test-smoke`媛 C emitter? LLVM emitter?먯꽌 world derived-state bounded recompute, zone lifecycle bounded frontier loop, projection-chain bounded recompute, embedded world-zone action-caused layer/state freshness, pass-limit overflow hard-fail, ABI smoke ?깅줉, backend-compare ?깅줉??寃?ы븳?? ??gate??full bounded fixpoint / transitive frontier scheduler媛 ?ㅼ떆 single-pass 援ы쁽?쇰줈 ?꾪눜?섏? 紐삵븯寃?留됰뒗 beta blocker gate?? ?⑥? runtime propagation closure??remaining authority/failure handoff family? broader world-zone propagation family瑜?媛숈? source-of-truth frontier policy濡??쇰컲?뷀븯???쇱씠??- Beta readiness audit: `docs/98_beta_closure_readiness_report.md` records the current codebase verdict, remaining blockers, and concrete closure order. It narrows the next highest-value implementation target to handoff propagation and broader world-zone scheduler generalization.

### 理쒓렐 closure 吏꾪뻾 (2026-04-23)

- AST ????붿뒪?⑥튂 partition 洹쒖튃 怨듭떇????`docs/95_ast_dispatch_partition.md`
  - ?꾩껜 AST ???(?꾩옱 93醫? ??4 移댄뀒怨좊━ (type annotation / decl sub-metadata / top-level decl / root) disjoint 遺꾪븷
  - 媛?移댄뀒怨좊━蹂꾨줈 "???뱀젙 switch ?먯꽌 ?꾨떖 遺덇??몄?" ??**?뚯꽌 invariant 洹쇨굅** 瑜?臾몄꽌??  - case label 異붽?/湲덉?/safety-net 寃곗젙 湲곗? ?뺤젙
  - ??AST ???異붽? ??泥댄겕由ъ뒪???ы븿
  - `llvm_stmt.c` ??top-level decl skip 由ъ뒪??+ Zone/World forward 媛 ??臾몄꽌 湲곗??쇰줈 ?뺣젹??(`AST_INTENT_DECL` skip ?꾨씫 ?섏젙, Zone/World 11醫?forward 二쇱꽍 ?뺥솗?? `llvm_expr.c` explicit diagnostic ?좎?)
  - ??AST ???異붽? ??docs/95 ?낅뜲?댄듃 梨낆엫 紐낆떆

### 理쒓렐 closure 吏꾪뻾 (2026-04-22)

- arena scratch slice 3嫄?異붽? ?≪닔 ??`docs/94_arena_index_lifetime_plan.md` ?낅뜲?댄듃
  - `semantic.c:50` `semantic_preload_stdlib_uses` ??per-iteration `malloc/free` module path 議곕┰??function-local `PgyArena` 濡??대룞. 諛곗튂 alloc ?섎굹濡??섎졃
  - `type_checker.c:1109` enum method name mangling??`malloc/snprintf/free` 瑜?`pgy_arena_fmt(&ctx->scratch_arena, ...)` 濡??대룞. `symbol_create_function` ???대? ?대? `pergyra_strdup` ?쇰줈 ?대쫫??蹂듭궗?섎?濡?arena ?덉텧 ?놁쓬
  - `slot_analyzer.c:1067` `slot_analyze_parallel_block` ??outer task metadata 諛곗뿴 3醫?(`task_accesses`/`task_counts`/`task_caps`) ??`sa->ctx->scratch_arena` 濡??대룞. per-task inner 諛곗뿴? ?ъ쟾??`collect_slot_accesses` 媛 heap-owned濡?愿由?- arena scratch 2李?slice 異붽? (媛숈? ??
  - `type_checker.c:355` type resolution cycle detection ??`visited`/`path` 諛곗뿴 ??`ctx->scratch_arena`. cycle text??return-contract helper??蹂대쪟
  - `type_checker_flow.c:499` match redundancy ??`seen` 諛곗뿴 ??`ctx->scratch_arena`
- arena scratch 3李?slice ??HIR/MIR 泥?吏꾩엯 (媛숈? ?? ?댄썑 4李⑥뿉??routine-scope濡??듯빀??
  - `hir.c:hir_compute_cfg_dominance` ??`visited`/`postorder`/`idoms` 3諛곗뿴 ??function-local `PgyArena`
  - `hir.c:hir_mark_natural_loop` ??`in_loop`/`stack` 2諛곗뿴 ??function-local `PgyArena`
  - `mir.c:mir_apply_ssa_rename` outer 3諛곗뿴 ??function-local `PgyArena`
- arena scratch 5李?slice ??LLVM 諛깆뿏??泥?吏꾩엯 (媛숈? ?? ?댄썑 6李⑥뿉??ctx-scope 濡??듯빀)
  - `llvm_register.c:llvm_register_enum_decl` ??`enum_fields` + per-variant `payload_fields` type-ref 踰꾪띁瑜?function-local `PgyArena` 濡??섎졃
  - `llvm_intent.c:llvm_collect_mir_intent_participants` ??return-ownership 怨꾩빟?대씪 deferred
- arena scratch 6李?slice ??**LLVMGenCtx ctx-scope scratch arena ?꾩엯** (媛숈? ??
  - `LLVMGenCtx` ??`PgyArena scratch` ?꾨뱶 異붽?
  - `llvm_ctx_create` / `llvm_ctx_destroy` ?먯꽌 lifecycle 愿由?  - 5李⑥뿉 function-local 濡??쒖옉??enum type-ref arena 瑜?`ctx->scratch` 濡??섎졃. LLVMGenCtx ?섎굹??init/destroy ??踰덈쭔
  - ?꾩냽 LLVM scratch ?ъ씠??(誘몃옒??諛쒓뎬?섎뒗) ????arena ?ъ궗??媛??- arena scratch 7李?slice ??**LLVM 9 ?ъ씠???쇨큵 ?≪닔** (媛숈? ??
  - tuple literal (`llvm_expr.c`) ??vals + tys
  - event handler type / tuple type (`llvm_backend.c:ast_type_to_llvm`) ??param_types + fields
  - event INVOKE (`llvm_domain.c`) ??inv_params + call_args
  - class/enum/extern ?깅줉 (`llvm_register.c`) ??4 param-type 踰꾪띁
  - ability vtable (`llvm_domain.c`) ??outer vt_fields + per-method ptypes
  - 怨듯넻: LLVM C API 媛 type/value 諛곗뿴???대? 蹂듭궗?섎?濡?scratch-safe
  - 寃곌낵: LLVM ?꾩껜??short-lived type 諛곗뿴 assembly 媛 ctx arena ?섎굹濡??섎졃
- arena scratch 8李?slice ??**LLVM 17 ?ъ씠??異붽? ?≪닔** (媛숈? ??
  - `llvm_stmt.c`: lambda param, parallel closure ctx/wrapper/handles, async closure fields, select rotation BBs
  - `llvm_intent.c`: intent function param_types, step completion `completed_allocas`, `saved_participant_ptrs`
  - `llvm_domain.c`: world sync `prev_active_addrs`, domain struct `ftypes` (4 遺꾧린), role/class method `ptypes` (2 ?ъ씠??, vtable `vals`
  - LLVM 履?scratch-safe calloc/malloc ? 嫄곗쓽 ?꾩닔 `ctx->scratch` 濡??섎졃. ?⑥? 寃껋? return-ownership ?쇱옱 helper ? AST-field stored 耳?댁뒪

- arena scratch 4李?slice ??**HIR/MIR routine-scope arena ?꾩엯** (媛숈? ??
  - `hir.h` HIRRoutine / `mir.h` MIRRoutine ??`PgyArena scratch` ?꾨뱶 異붽?
  - ?앹꽦: `hir_append_*`, `mir_lower` 猷⑦봽 ??`memset` 吏곹썑 `pgy_arena_init(&routine.scratch, 0)`
  - ?뚭눼: `hir_destroy()` / `mir_destroy()` per-routine cleanup + OOM 寃쎈줈 (諛곗뿴 ?몄엯 ?ㅽ뙣 耳?댁뒪)
  - 3李⑥뿉 function-local 濡??쒖옉??3媛?arena 瑜?紐⑤몢 `&routine->scratch` 濡??듯빀 ??routine ?섎굹??init/destroy ??踰덈쭔. ?щ윭 HIR/MIR pass 媛 媛숈? arena 瑜??ъ궗??  - MIR pass??`routine->scratch` 留??. `routine->hir_routine->scratch` ??HIR frozen 怨꾩빟?대씪 ?묎렐 湲덉? (肄붾찘?몃줈 怨좎젙)
- ?먯튃 ?좎?: `scratch-only local temp 癒쇱?, returned string ?섏쨷`. `slot_ref_expr(...)` 媛숈? 諛섑솚 ownership ?쇱옱 helper???꾩쭅 蹂대쪟
- 踰좏? acceptance line #8 ("scratch/result lifetime怨?cache boundary媛 臾몄꽌/援ы쁽 湲곗??쇰줈 ?ㅻ챸 媛?ν븯??) ???대떦 slice 湲곗뿬

### 理쒓렐 closure 吏꾪뻾 (2026-04-21)

- C/LLVM init idiom 異?媛먯궗 + 1李??뺣퉬 ?꾨즺 (`docs/93_codegen_idiom_audit.md`)
  - 6 case 횞 2 backend 留ㅽ듃由?뒪 怨좎젙
  - **Case 1 HIGH divergence ?댁냼**: ?⑥닔-諛붾뵒 `let x: T;` (annotation + no init)??`PGY_CODE_SEM_UNINIT_LOCAL` 濡?嫄곕?. C??scalar-zero, LLVM? store ?앸왂?쇰줈 泥?read?먯꽌 媛??섎?媛 媛덈씪吏???좊났 寃쎈줈瑜?semantic ?덈꺼?먯꽌 李⑤떒
  - **Case 2 C backend L815 ?뺣━**: `transpiler_c_type_uses_scalar_zero` helper濡?scalar/aggregate 遺꾧린. 湲곗〈 ?좊났 踰꾧렇 (`struct Foo x = 0;` invalid C) ?쒓굅 (defense in depth)
  - **Case 3 MEDIUM ?섎룄 鍮꾨?移?쑝濡??뺤젙**: slot claim? C媛 ?고???helper, LLVM??IR-direct. ?꾩옱 runtime observability ?섏??먯꽌 愿痢?side effect 0. runtime observability ?뺤옣 ???ш컧?щ줈 deferral
  - ?뚭? 3醫?異붽?:
    - `function-body let with annotation and no initializer is rejected`
    - `function-body let with aggregate annotation and no initializer is rejected`
    - `subject field let with no initializer does not trigger the uninit-local guard` (negative)
  - ?뚯꽌 援ъ“ ?ы솗?? class/subject field??ClassField 寃쎈줈濡?遺꾨━?섏뼱 `AST_LET_DECL`???꾨떂 ??guard媛 field-level ?섎?瑜?移⑤쾾?섏? ?딆쓬
  - docs/72 ??`PGY_SEM_UNINIT_LOCAL` ?뱀뀡 + docs/93 cross-link 異붽?

### 理쒓렐 closure 吏꾪뻾 (2026-04-20)

- own/ref broader audit瑜?helper family 湲곗??쇰줈 ???뺣젹
  - helper call boundary??`subject` / general boundary value 寃쎈줈瑜?怨듭슜 borrowed-boundary validator濡??묒쓬
  - container store / array literal store borrow-escape瑜?怨듭슜 ownership diagnostic helper濡??듯빀
  - semantic channel send borrow-escape??怨듭슜 ownership diagnostic helper濡??밴꺽
  - 利? `assignment / helper call / channel send / container store / array literal store / constructor field store`媛 ?먯젏 媛숈? provenance wording family濡??섎졃 以?- intent authority mismatch provenance瑜???吏곸젒?곸쑝濡??몄텧
  - `authorized by` unknown participant / non-subject participant / zone subject-slot mismatch / zone authority mismatch??`approval boundary provenance` ?뱀뀡 異붽?
  - provenance媛 鍮꾩뼱 ?덉쑝硫?`no inherited/derived authority provenance was recorded`瑜?紐낆떆?곸쑝濡?蹂닿퀬
- relation/effect/projection failure depth瑜?異붽? 蹂닿컯
  - invalid projection source / tobject source rejection??target/source consumer path? projection contract origin??吏곸젒 蹂닿퀬
  - 利? projection diagnostics媛 ?⑥닚 type mismatch媛 ?꾨땲??`target slot <- source slot` 寃쎈줈瑜?湲곗??쇰줈 ?ㅻ챸?섍린 ?쒖옉??- ?꾩옱 踰좏? blocker ?ъ젙??  - Windows backend-compare / LLVM parity 蹂듦뎄
  - declaration-side MIR-only ?⑥? host/inventory helper debt ?쒓굅
  - own/ref ?쇰컲?붿쓽 broader assignment / container / rebind / summary path closure
  - intent/zone/world 諛?relation/effect/projection provenance 留덉?留??ы솕
- Windows-native compile hygiene瑜?異붽? ?뺣━
  - `type_checker_builtins_query.inc`, `type_checker_builtins_nominal.inc`??`%zu` / extra-arg formatting drift瑜??쒓굅
  - `type_checker_decls_world.inc`??world lifecycle diagnostics placeholder-arg mismatch瑜??쒓굅
  - `type_checker_builtins.c`??ownership/channel helper瑜?full internal header include ???理쒖냼 forward declaration?쇰줈 怨좎젙??enum/static helper ?ъ꽑??異⑸룎???쇳븿
  - ?꾩옱 湲곗???
    - `test-semantic`: `1855 passed, 0 failed`
    - `test-transpile`: `601 passed, 0 failed`
  - ?⑥? Windows blocker??semantic compile ?④퀎媛 ?꾨땲??native MSYS2/MinGW ?ㅽ뻾 ?섍꼍?먯꽌??backend/runtime parity ?뺤씤 異뺤쑝濡??대룞

### 理쒓렐 closure 吏꾪뻾 (2026-04-16)

- declaration-side host context瑜?inventory-backed handle 履쎌쑝濡????④퀎 ???뺣젹
  - transpiler host lookup??`current_host_decl -> within_zone -> saved host-name inventory` ?쒖쑝濡?蹂듭썝?섎룄濡?議곗젙
  - zone/relation/effect/world field query helper媛 raw `current_*_name` 遺꾧린蹂대떎 inventory-backed `current_host_decl`瑜??곗꽑 ?뚮퉬
  - 利? declaration-side C backend context 蹂듭썝?먯꽌 string name state???먯젏 restore hint濡쒕쭔 ?④퀬, ?ㅼ젣 host truth??active inventory 湲곕컲 handle濡??섎졃 以?- explicit/compressed canonical pair examples瑜?intent-first ?낇빐 洹쒖튃?쇰줈 ?ㅼ떆 ?뺣젹
  - large/composite pair source??`intent -> world/zone -> subject` read order瑜?吏곸젒 紐낆떆
- world embedding implicit copy瑜?warning???꾨땲??hard contract濡??밴꺽 ?쒖옉
  - world constructor??zone binding??洹몃?濡??섍린硫?explicit `Clone(...)`瑜??붽뎄
  - hidden copy semantics瑜????댁긽 benign warning?쇰줈 ?④린吏 ?딆쓬
- generic contract consumer path瑜????④퀎 ???レ쓬
  - omitted trailing default type arg媛 user-defined generic class specialization path?먯꽌??effective arg 湲곗??쇰줈 寃利앸릺?꾨줉 ?뺣젹
  - role impl / action requires / zone authority / party role slot?먯꽌 `default arg omission + where-bound violation` negative regressions 異붽?
  - multi-bound / omitted-default / consumer provenance 議고빀 ?뚭?瑜?semantic 湲곗??쇰줈 怨좎젙
  - ability consumer path / class instantiation-specialization path?먯꽌 unresolved effective generic arg瑜?silent skip?섏? ?딄퀬 structured error濡??밴꺽
  - role-side ability require-field type resolution?먯꽌??unresolved effective generic arg瑜?silent skip?섏? ?딄퀬 structured error濡??밴꺽
  - malformed impl ability generic arg媛 ?덉뼱???ㅼそ where/require-field 寃利앹쑝濡?partial 吏꾪뻾?섎뜕 寃쎈줈瑜?李⑤떒
  - default generic bound validation?먯꽌 unknown parameter / unresolved default type??structured error濡??밴꺽
  - generic function call-site where-clause validation?먯꽌??missing/unresolved effective arg瑜?silent skip?섏? ?딄퀬 structured error濡??밴꺽
- own/ref 泥??쇰컲??vertical slice ?쒖옉
  - existing movable resource value(`QubitSlot`)??function boundary?먯꽌 explicit `own` transfer parameter瑜??덉슜
  - `ref QubitSlot`???꾩쭅 誘몃떕??subset?쇰줈 ?좎??섎릺, ?댁쑀/consumer path/fix媛 ?ы븿??structured diagnostic?쇰줈 怨좎젙
  - 利? `own/ref`???ъ쟾???꾩뿭 closure ?꾩씠吏留? move semantics媛 ?대? ?덈뒗 resource value????댁꽌??explicit transfer boundary媛 遺遺꾩쟻?쇰줈 ?대━湲??쒖옉??  - return/channel boundary ownership diagnostics??`Reason:` / `Fix:` 援ъ“濡??뺣젹
  - function signature anchored-return rejection??`Reason:` / `Fix:` 援ъ“濡??뺣젹
  - unnamed movable-resource channel send??moved-here provenance瑜??ㅻ챸?섎뒗 hard error濡?怨좎젙
  - local binding ?④퀎?먯꽌??`recv/await` unnamed boundary use, subject rebinding, released-slot move, anchored-handle rebinding??`Reason:` / `Fix:` 援ъ“濡??뺣젹
  - slot escape analyzer 寃쎄퀬??return/helper-call/channel/unterminated local claim 寃쎈줈?먯꽌 provenance??`Reason:` / `Fix:` 援ъ“濡??뺣젹
- relation/effect/projection contract瑜????섎뱶?섍쾶 議곗???  - `intent step causes`媛 zone effect slot ?놁씠 ?듦낵?섎뜕 寃쎈줈瑜?hard error濡??밴꺽
  - `action causes`??zone effect slot ?놁씠 ?⑤뒗 寃쎈줈瑜?structured hard error濡??밴꺽
  - authority-bearing `apply/link/detach/unlink/maintain`媛 `by <subjectSlot>` ?놁씠 ?⑤뒗 寃쎈줈瑜?hard error濡??밴꺽
  - duplicate authority, unknown layer relation/effect type?????댁긽 benign warning?쇰줈 ?④린吏 ?딆쓬
  - maintain/detach/unlink duplicate/conflict diagnostics??`Reason:` / `Fix:` 援ъ“濡??뺣젹
- unresolved declaration entrypoint瑜???以꾩???  - role include unknown role, roster slot unknown party, world roster/zone unknown type??hard error濡??밴꺽
  - generic where-clause consumer path?먯꽌 unresolved effective arg?????댁긽 silent skip?섏? ?딆쓬
- declaration-side MIR-only domain method gate瑜???議곗???  - party / roster / relation / effect / zone / world method emission??MIR routine ?놁씠 AST body濡?議곗슜??fallback?섏? ?딅룄濡?C backend瑜??뺣젹
  - role / domain method emission?먯꽌 MIR routine 誘몄〈?щ? LLVM backend hard error濡??밴꺽
  - 利? declaration-side domain method??MIR inventory媛 議댁옱?섎뒗 鍮뚮뱶?먯꽌 silent fallback???꾨땲??explicit backend failure瑜?怨꾩빟?쇰줈 ?쇱쓬

### 理쒓렐 closure 吏꾪뻾 (2026-04-14)

- declaration-side MIR-only intent inventory瑜???諛?덈떎
  - MIR媛 `IntentParticipant(alias,type)` metadata瑜?吏곸젒 ?대컲
  - C/LLVM intent declaration emission??participant alias/type瑜?AST ?ы빐???놁씠 MIR metadata濡??곗꽑 ?뚮퉬
- step-level MIR-only validation??AST field 議댁옱 寃?ъ뿉??metadata 議댁옱 寃?щ줈 ??꼈??  - `IntentCheck`
  - `IntentEval`
  - `IntentZoneWhere/IntentZoneAlias/IntentZoneFrom`
  - `IntentWho/IntentDispatch`
  - `compensate` 議댁옱 ?먯젙
- intent emission cleanup/rollback 寃쎈줈??metadata gate瑜?C/LLVM ?????뺣젹?덈떎
- 愿???뚭?:
  - `test-mir` green
  - `test-transpile` green

利? intent declaration/step emission? ?꾩쭅 ?꾩쟾 MIR-only ?좎뼵???앸궃 寃껋? ?꾨땲吏留?
`participant/step contract inventory`瑜?AST presence??湲곕???媛??嫄곗튇 fallback?????④퀎 ???쒓굅?먮떎.

### 踰좏? 湲곗???異붽? (2026-04-15)

- `docs/70_beta_closure_master_board.md` 異붽?
  - B0 4異? declaration-side MIR-only debt, parity, runtime observability, surface trust瑜????μ쑝濡?怨좎젙
  - 踰좏? acceptance line怨?exit rule??紐낆떆
  - ?욎쑝濡?TODO??媛쒕퀎 ?묒뾽? ??蹂대뱶 湲곗??쇰줈 ?곗꽑?쒖쐞瑜??곕Ⅸ??
### 踰좏? 理쒖쥌 愿臾?(2026-04-18)

- [ ] **declaration-side MIR-only瑜?援ъ“?곸쑝濡??リ린**
  - zone/world/relation/effect declaration/method emission?먯꽌 ?⑥? AST/HIR-carried inventory dependency瑜????쒓굅
  - `current_*_name` / host-name 異붿젙 helper蹂대떎 inventory-backed host handle / metadata ?뚮퉬瑜??곗꽑?섎룄濡??뺣젹
  - transpiler/LLVM ?묒そ?먯꽌 raw host-name read瑜?helper/restore layer 諛뽰쑝濡??ㅼ떆 ?덉? 紐삵븯寃??뚭?濡?怨좎젙
  - declaration emission failure??comment/skip/fallback return???꾨땲??explicit backend error濡??밴꺽
  - C/LLVM ????declaration-side path?먯꽌 `Unknown` / surface-trust-breaking fallback type emission??怨꾩냽 ?쒓굅
  - 臾몄꽌?먯꽌 `MIR-led / HIR-assisted`?쇨퀬 ?④꺼??debt瑜??ㅼ젣 援ы쁽 湲곗??쇰줈 ??異뺤냼?섍퀬, 踰좏? ?쒖젏 ?쒗쁽怨?援ы쁽???쇱튂?쒗궓??
- [x] **AST dispatch / backend fallback trust gate 怨좎젙**
  - `docs/95_ast_dispatch_partition.md` 湲곗??쇰줈 AST ???partition??臾몄꽌??  - LLVM `stmt/expr` default path??warning-only媛 ?꾨땲??structured backend error濡?怨좎젙
  - Zone/World declaration verb媛 expression fallback?쇰줈 議곗슜??`0/null`???섎뒗 寃쎈줈瑜?explicit backend diagnostic?쇰줈 李⑤떒
  - `tests/ast_dispatch_partition_smoke.sh`? `make ast-dispatch-test-smoke`瑜?異붽???partition drift? silent fallback ?뚭?瑜?CI?먯꽌 李⑤떒
  - Linux `ci-linux` acceptance line??AST dispatch smoke瑜??곌껐

- [x] **type-resolution DAG瑜?beta blocker濡??ы븿**
  - import resolver? 蹂꾧컻濡?semantic type dependency graph瑜?beta acceptance line???ы븿
  - generic default / multi-bound / role impl / action / intent step / party role slot / zone authority / module contract consumer瑜?媛숈? graph inventory濡?異붿쟻
  - alias depth limit / ad-hoc recursive failure蹂대떎 path-aware cycle diagnostic???곗꽑 湲곗??쇰줈 ?뚯뼱?щ┝
  - 1?④퀎 吏꾪뻾: `topo_order`瑜?踰꾨━吏 ?딄퀬 declaration staged worklist???곌껐 ?쒖옉
  - 諛섏쁺 臾몄꽌:
    - `docs/70_beta_closure_master_board.md`
    - `docs/63_feature_depth_matrix.md`
  - 1?④퀎 吏꾪뻾: `world/zone` local contract? `refresh` projection path瑜?synthetic graph node濡??щ━湲??쒖옉
  - 1?④퀎 吏꾪뻾: topo worklist媛 `LOCAL_CONTRACT` / `PROJECTION_PATH` synthetic node???ㅼ떆 ?뚮퉬?섍린 ?쒖옉
  - 1?④퀎 吏꾪뻾: synthetic node ?뚮퉬瑜?host ?꾩껜 ?ъ떎?됱씠 ?꾨땲??label蹂?narrow handler濡?異뺤냼
  - 1?④퀎 吏꾪뻾: role impl consumer源뚯? cycle provenance ?뚭?瑜?異붽???ability consumer family瑜????꾩꽦
  - ?⑥? ?? staged declaration prepass 踰붿쐞瑜??볧엳怨?graph-backed evaluator瑜?semantic source-of-truth濡??밴꺽
  - ecosystem ?뺤옣(`stdlib/pkg/tooling`)? ??DAG closure ?댄썑 ?④퀎濡?誘몃８

- [x] **own/ref ?쇰컲??audit 留덇컧**
  - own/ref??ownership classifier 湲곗? stable subset?쇰줈 ?ロ옒
  - borrowed value escape??helper call / channel / return / container store肉??꾨땲??broader assignment/member/store path源뚯? provenance 湲곗??쇰줈 ?먭?
  - 吏꾪뻾: constructor field store(`Holder(packet)` 媛숈? boundary-visible store)瑜?borrowed escape 寃쎈줈濡??밴꺽?섍퀬 semantic regression 異붽?
  - 吏꾪뻾: constructor field store??borrowed member/aggregate source path provenance(`holder.packet`, `items[0]`)瑜?吏곸젒 蹂닿퀬?섎룄濡??뺣젹
  - 吏꾪뻾: array literal store(`[packet]`)??borrowed escape 寃쎈줈濡??밴꺽?섍퀬 semantic regression 異붽?
  - 吏꾪뻾: member assignment / array overwrite 吏꾨떒??identifier-only媛 ?꾨땲??`holder.packet`, `items[0]` 媛숈? target path provenance瑜?吏곸젒 蹂닿퀬?섎룄濡??뺣젹
  - 吏꾪뻾: new-binding escape??identifier-only媛 ?꾨땲??borrowed member/aggregate source path provenance(`packet.view`, `items[0]`)源뚯? 異붿쟻?섎룄濡??뺤옣
  - 吏꾪뻾: new-binding escape regression??member source path(`packet.items`)? array source path(`items[0]`)瑜?fixture濡?怨좎젙
  - 吏꾪뻾: container store(`ArrayPush`/`ListPush`/`SetAdd`/`QueuePush`/`MapSet`)??borrowed member/aggregate source path provenance瑜?吏곸젒 蹂닿퀬?섎룄濡??뺣젹
  - 吏꾪뻾: helper forwarding / builtin channel send(`Send`/`TrySend`/`SendTimeout`/status variants)??unnamed borrowed member/aggregate source path provenance瑜?吏곸젒 蹂닿퀬?섎룄濡??뺣젹
  - 吏꾪뻾: direct `return` escape??borrowed member/aggregate source path provenance(`holder.packet`, `items[0]`)瑜?吏곸젒 蹂닿퀬?섎룄濡??뺣젹
  - 吏꾪뻾: slot/resource summary 湲곕컲 `return/channel/helper` diagnostics??`summary provenance root` vocabulary濡?direct semantic wording????媛源앷쾶 ?뺣젹
  - 吏꾪뻾: summary-based helper escape??direct callee wording ???`helper/function summary in '<fn>'` 寃쎈줈濡?遺꾨━??drift瑜?以꾩엫
  - 吏꾪뻾: summary-based return/channel escape??direct consumer wording ???`return summary in '<fn>'` / `channel summary in '<fn>'` 寃쎈줈濡?遺꾨━??drift瑜?以꾩엫
  - 吏꾪뻾: anchored-handle summary escape??direct `return/channel/helper` wording ???summary wording?쇰줈 遺꾨━??own/ref bridge 臾멸뎄瑜??뺣젹
  - 吏꾪뻾: helper-call / container-store / array-literal-store / semantic channel-send diagnostic family瑜?怨듭슜 helper濡??듯빀
  - 吏꾪뻾: nested projection + transitive helper + member rebind 議고빀??semantic regression fixture濡?異붽?
  - 吏꾪뻾: movable-resource + nested member source + member rebind target 議고빀??semantic regression fixture濡?異붽?
  - 吏꾪뻾: declaration-side MIR-only host truth??`current_host_decl` / inventory 湲곗??쇰줈 ??醫곹삍怨? `within_zone`瑜??곕씪媛??transpiler host recovery fallback怨?role-owner direct AST lookup???쒓굅
  - 吏꾪뻾: own/ref anchored-handle wording??assignment / let-binding / return / channel / helper family??留욎떠 `boundary-visible handle binding` / `anchored-handle provenance` 湲곗??쇰줈 ?뺣젹
  - ?꾨즺 ?먯젙: direct/summary helper-chain, return/channel/helper, destructure, assignment/member/container/constructor/array path媛 current semantic regression?쇰줈 怨좎젙??  - explicit reject: authority-bearing `Token<T>` escape/transport
  - beta-out-of-scope: region/lifetime solver? universal ownership lattice

- [ ] **generic contract ?꾧꼍濡?audit 留덇컧**
  - generic contract??`default type arg`, `multi-bound where`, `ability<T> consumer`, `zone authority`, `party role slot`, `impl/reference`, cross-module consumer path瑜?留덉?留됯퉴吏 audit
  - 吏꾪뻾: `party role slot` generic mismatch consumer??actual/expected type arg + consumer path provenance regression?쇰줈 怨좎젙
  - ?⑥? generic consumer path媛 ?녿떎??寃껋쓣 regression?쇰줈 利앸챸?섍퀬, partial acceptance泥섎읆 蹂댁씠??寃쎈줈瑜??④린吏 ?딅뒗??
- [ ] **Intent/Zone/World, relation/effect/projection 吏꾨떒怨?provenance 留덇컧**
  - intent/zone/world??embedding / handoff / authority mismatch?먯꽌 contract source, derived zone/using, transfer edge provenance瑜?怨꾩냽 媛뺥솕
  - relation/effect/projection? propagation edge failure, contract mismatch, branch/join/handoff path??`Contract source:` / `Reason:` / `Fix:`? source/target provenance瑜??쇨??섍쾶 遺李?  - 吏꾪뻾: world embedding/handoff? intent transfer/authority mismatch???듭떖 寃쎈줈瑜?`Contract source:` / `Reason:` / `Fix:` 援ъ“濡??ъ젙??  - runtime contract provenance? diagnostic wording?????뺣젹???쒖솢 ?ㅽ뙣?덈뒗吏 + 怨꾩빟???대뵒???붾뒗吏 + ?대뼸寃?怨좎튌吏?앸? ??踰덉뿉 蹂댁씠寃??쒕떎
  - helper-heavy edge path瑜?以꾩씠怨? compile-time contract ?ㅽ뙣瑜?silent/best-effort runtime sync濡??섍린吏 ?딅뒗??  - 吏꾪뻾: intent step contract-source summary媛 `authorized by`, transfer handoff, derived transfer zone provenance瑜???吏곸젒?곸쑝濡??ㅻ챸?섎룄濡??뺣젹
  - 吏꾪뻾: zone-within action authority mismatch媛 `within` / `causes` header瑜?contract source濡?吏곸젒 蹂닿퀬?섎룄濡??뺣젹
  - 吏꾪뻾: world embedding / post-embedding mutation diagnostics媛 `world <name> zone slot <slot>` contract source? world-owned authority/handoff destination??吏곸젒 蹂닿퀬?섎룄濡??뺣젹

- [ ] **C/LLVM parity + full CI green??踰좏? 理쒖쥌 愿臾몄쑝濡?怨좎젙**
  - Linux 湲곗? `parser / semantic / transpile / ABI / backend-compare / llvm smoke / ir-pipeline / example smoke`瑜?full green?쇰줈 ?좎?
  - Windows??濡쒖뺄 Linux host?먯꽌 媛뺥뻾?섏? ?딄퀬, MSYS2/MinGW + LLVM runner?먯꽌 `ci-windows` full green???ㅼ떆 怨좎젙
  - backend compare??domain semantics 湲곗? parity瑜?怨꾩냽 ?뺣??섍퀬, same-process ABI / launch / runtime environment 李⑥씠瑜??щ컻?섏? ?딄쾶 ?〓뒗??  - ?꾩옱 immediate blocker: Windows `backend-compare`? LLVM parity??留덉?留?crash / launch / runtime mismatch ?쒓굅
  - 踰좏? ?좎뼵 ??acceptance line? ?쒕?遺?green?앹씠 ?꾨땲??C/LLVM parity? expected stdout/stderr/result parity源뚯? ?ы븿??CI green?쇰줈 ?붾떎

?ㅽ뻾 媛?ν븳 ?곌뎄??而댄뙆?쇰윭 ?④퀎???섍꼈吏留? ?꾩쭅 踰좏??쇨퀬 遺瑜??섎뒗 ?녿떎.

?먯젙 湲곗?:
- 踰좏? ?먯튃??`遺遺?援ы쁽 ?곹깭瑜??④린吏 ?딅뒗??瑜??꾩쭅 異⑹”?섏? 紐삵븿
- ?ㅼ썙??遺議깆씠 ?꾨땲??`援ы쁽 depth 遺덇퇏????臾몄젣??- parser媛 諛쏅뒗 surface 以??쇰?媛 semantic/C/LLVM/runtime/test/documentation源뚯? ?꾩쟾???ロ엳吏 ?딆쓬

### ?대? ?ロ엺 異뺢낵 ???댁긽 踰좏? 李⑤떒???꾨땶 寃?
- `public/private/export` module boundary
  - top-level nominal/domain/callable visibility ?뺣젹 ?꾨즺
  - private `func/intent/event` cross-module call 李⑤떒 ?꾨즺
  - private `zone/effect` action-contract leakage 李⑤떒 ?꾨즺
- nominal token split
  - `subject/class/struct/object/tobject`??lexer token ?덈꺼?먯꽌 ?대? 遺꾨━??- ability field surface
  - legacy `require` alias ?쒓굅, `fields` canonical surface 怨좎젙
- generic ability baseline
- `ability<T>`, `requires Ability<T>`, `impl ability Ability<T>`, zone authority generic ref, mismatch diagnostics baseline 議댁옱
- cross-module imported generic ability??multi-bound zone-authority consumer regression 異붽?
- ?묒옄 surface
  - 踰좏? ??곸뿉???쒖쇅
  - `v2 / experimental`濡쒕쭔 異붿쟻

### ?꾩옱 踰좏?瑜?留됰뒗 ?ㅼ젣 B0 媛?
#### 1. Intent / Zone / World closure

?꾩옱:
- intent orchestration, inherited/derived contract, rollback/cleanup carrier, zone/world declaration怨?湲곕낯 lowering? 議댁옱
- zone/world projection/layer/state query??議댁옱
- intent runtime observability baseline??議댁옱
  - `IntentLast*`
  - `IntentHistoryStep*`
  - `IntentActive*`
  - `IntentRecent*`
  - active/recent handle + active-step field query builtin??semantic/transpiler/runtime/LLVM baseline ?곌껐 ?꾨즺
  - runtime ?대? recent ring + active registry + typed step history storage ?곌껐 ?꾨즺
  - ABI regression: `IntentRecent*` trace/failure baseline, failed-intent provenance, world zone query, relation/effect zone state parity 怨좎젙
  - backend parity: embedded world -> zone projection visibility regression 怨좎젙

?⑥? 寃?
- embedding ownership / handoff policy瑜?surface trust ?섏?源뚯? 紐낇솗??怨좎젙
- richer multi-instance timeline query? failure provenance ?뺢탳??- cross-layer propagation policy????源딆? closure
- C/LLVM parity瑜?declaration/runtime/diagnostic源뚯? 媛숈? ?덉쭏濡??뺣젹

#### 2. relation / effect / projection closure

?꾩옱:
- declaration, lifecycle shorthand, `refresh/publish/bind`, layer/state query, overlay sync baseline 議댁옱
- effect join/meet/conflict API? basic closure 議댁옱
- projection contract diagnostics??target/source/mode/fix瑜??ы븿?섎뒗 structured error 履쎌쑝濡?蹂닿컯??- backend parity:
  - embedded world -> zone projection visibility regression 怨좎젙
  - relation/effect layer + state propagation parity regression 怨좎젙

?⑥? 寃?
- authority/resource? effect partial order?????꾩쟾???듯빀
- projection propagation policy ?ы솕
- runtime contract? deeper propagation failure provenance瑜????ㅻ챸 媛?ν븯寃??뺣━
- C/LLVM parity?먯꽌 helper-heavy edge path 媛먯냼

#### 3. generic contract closure

?꾩옱:
- generic ability declaration/reference baseline 議댁옱
- action / intent step / zone authority / party role slot generic mismatch diagnostics stable 議댁옱
- hidden/default-export generic ability visibility??action/role impl肉??꾨땲??zone authority/party role slot consumer path源뚯? ?뚭?濡?怨좎젙
- `ability<T> where ...` bound??`requires` / `impl ability` / party role slot ref?먯꽌 ?ㅼ떆 寃利앸맖
- default type argument??semantic + transpiler + backend compare源뚯? baseline closure ?꾨즺
  - user-defined `class/ability<T = ...>`媛 omitted arg 寃쎈줈?먯꽌??effective specialization?쇰줈 ?뺣젹??  - non-deduced trailing generic parameter default??function call `where` validation 寃쎈줈?먯꽌 ?뚭?濡?怨좎젙
  - cross-module omitted default generic ability consumer(`party role slot` / `zone authority`)???뚭?濡?怨좎젙
- multi-bound `where T: A + B` baseline? ?꾩옱 ?숈옉??- hidden/default-export? generic ability ref 洹쒖튃 ?뺣젹 ?꾨즺

?⑥? 寃?
- broader type-family generalization??beta 踰붿쐞 諛뽰쑝濡?紐낆떆
- richer generic constraint validation??beta contract 踰붿쐞瑜?臾몄꽌/board???쇱튂?쒖폒 怨좎젙
- import/use surface? diagnostics/tooling ?쒗쁽??module contract 湲곗??쇰줈 ???쇨??섍쾶 ?뺣━

#### 4. own/ref closure

?꾩옱:
- anchored subset? ?ロ? ?덉쓬
  - `ref Slot<subject-host>`
  - `own SecureSlot<subject-host>`
- first movable-value transfer slice???쒖옉??  - explicit `own QubitSlot` parameter???덉슜
  - `ref QubitSlot` borrow boundary baseline ?덉슜
  - call-site??`own/default`硫?consume, `ref`硫?borrow ?좎?濡?遺꾧린
  - borrowed `ref QubitSlot`??`return` / `channel send` escape??semantic?먯꽌 紐낆떆 李⑤떒
- 愿??吏꾨떒/?덉젣/臾몄꽌???꾩옱 援ы쁽 湲곗??쇰줈 ?뺣젹??
?먯젙:
- anchored subset baseline? ?대? ?덉?留? beta-quality 湲곗??먯꽌??own/ref瑜??ㅼ떆 ?쒖꽦 blocker濡?蹂몃떎
- ?⑥? ?쇱? ?쇰컲 movable type ownership model, copy vs move-only 遺꾨쪟, assignment/call/return/channel/container/rebind ?꾧꼍濡?analysis, richer provenance diagnostics瑜??ル뒗 寃껋씠??- ?뱁엳 borrowed movable-resource ownership??helper-call/return/channel-send baseline???ロ삍怨? ?ㅼ쓬? wider movable type generalization怨?container/rebind provenance瑜????レ븘???쒕떎
- anchored subset留?stable?대씪怨?蹂닿퀬 ?섏뼱媛硫?ownership story媛 partial acceptance濡??⑤뒗??
### ?덉씠?대퀎 ?꾩옱 吏꾩떎

#### ?쒕㎤??
- 媛뺥븳 遺遺?
  - nominal family
  - subject/action
  - async/channel/select
  - generic ability baseline
  - visibility/export boundary
- ?꾩쭅 ?뺤? 遺遺?
  - richer generic constraint validation
  - general own/ref
  - event closure???붿뿬 negative path
  - collection semantic depth

#### 肄붾뱶 ?앹꽦

- C backend:
  - 肄붿뼱 surface??媛???깆닕
  - method owner metadata媛 HIR->MIR濡??대젮? declaration-side zone/relation/effect/world context 蹂듭썝 ???대쫫 異붿젙蹂대떎 MIR metadata瑜??곗꽑 ?ъ슜
  - 吏꾪뻾: `transpiler_emit_host_method_body_local`??manual save/restore ?곹깭瑜?`TranspilerMirEmitState` snapshot helper濡?異뺤냼
  - 吏꾪뻾: `emit_func_decl_from_mir_named` / AST fallback `emit_func_decl_named`??`TranspilerMirEmitState` snapshot helper濡??섎졃
  - 吏꾪뻾: `emit_intent_decl`??function-scope out/render/return/local-count restore??`TranspilerMirEmitState` snapshot helper濡??섎졃
  - 吏꾪뻾: generic class specialization method body??MIR inventory 議댁옱 ??AST fallback ???MIR routine gate / explicit backend error濡??뺣젹
  - 吏꾪뻾: LLVM domain/role missing-routine errors??`PGY_CODE_LLVM_MIR_ROUTINE_MISSING` / cause / fix structured path濡??뺣젹
- LLVM backend:
  - MIR-led / HIR-assisted hybrid
  - ordinary routine? MIR 以묒떖?댁?留?domain declaration怨??쇰? bootstrap/helper path??HIR/AST ?섏〈 ?붿〈
  - pure MIR-only?쇨퀬 遺瑜닿린?먮뒗 ?꾩쭅 ?대쫫??怨쇳븿

#### ?고???
- 媛뺥븳 遺遺?
  - slot / secure baseline
  - async/channel basic runtime
  - basic intent execution/rollback
  - intent observability baseline (`last` / `history` / `active` / `recent`)
- ?꾩쭅 ?뺤? 遺遺?
  - richer multi-instance timeline / failure provenance
  - channel backpressure protocol
  - party edge-path completeness
  - richer zone/world runtime policy

### 而щ젆??/ ?쒕㈃ ?좊ː

- `Map<K, V>`???꾩옱 `String | Int | Long | Bool` key stable subset源뚯? ?щ┛??- ?닿쾬? 踰꾧렇媛 ?꾨땲???꾩옱 contract
- arbitrary key-universal map contract???꾩쭅 generic closure debt濡??⑤뒗??
### ?대쭅

- LSP / formatter??踰좏? 李⑤떒 ?듭떖???꾨떂
- debugger / package manager / WASM??踰좏? 李⑤떒 ?듭떖???꾨떂
- ?대뱾? B0 closure ?댄썑???ㅻ（??寃껋씠 留욎쓬

### 踰좏? 吏곸쟾 ?뺣━ ?먯튃

1. ???ㅼ썙????異뺤쓣 ??異붽??섏? ?딅뒗??2. ?⑥? 誘몄셿??surface瑜?`?꾩꽦`?섍굅??`experimental`濡??대┛??3. `?묒옄`, `WASM`, `?⑦궎吏 留ㅻ땲?`, `怨좉툒 ?붾쾭嫄???踰좏? ??곸뿉???쒖쇅?쒕떎
4. B0 4媛쒕? ?リ린 ?꾩뿉??踰좏??쇨퀬 遺瑜댁? ?딅뒗??
---

## ?꾨즺 (P0 ??Pain Point ?섏젙, 2026-04-12)

- [x] **P0-1: Array for-in `.count` ??`.length`** ??`transpiler.c`?먯꽌 Array??`.length`, List??`.count` ?ъ슜
- [x] **P0-2: `StringSplit`/`StringJoin` ?고???援ы쁽** ??`pgy_runtime.h`???ㅼ젣 援ы쁽 異붽?, ?쒕㎤??C 諛깆뿏???쇱튂
- [x] **P0-3: `None` ?щ낵 ?뺤쓽** ??`type_checker.c`?먯꽌 AST_IDENTIFIER 泥섎━, `type_system.c`?먯꽌 `Option<unknown>` ??`Option<T>` ?좊떦 ?덉슜, 肄붾뱶?좎뿉??`expected_type` 湲곕컲 ????닿껐
- [x] **P0-6: defer 蹂???ㅼ퐫??踰꾧렇 ?섏젙** ??`type_checker_flow.c`?먯꽌 defer body 泥섎━ ????resource-state snapshot/restore. cleanup body??`return`/`break`/`continue`? QubitSlot release/move??寃?ы븯吏留?二쇰? CFG path? outer loop flow瑜??뚮퉬?섏? ?딅뒗?? direct `type_check_statement()` fallback??媛숈? helper瑜??ъ슜?쒕떎.
- [x] **P1-7: struct/subject Slot 留ㅽ겕濡?warning ?듭젣** ??`transpiler.c`?먯꽌 `#pragma GCC diagnostic push/pop`?쇰줈 `-Wunused-function` ?듭젣
- [x] **P1-emit_call 媛?硫붿슦湲?* ??`BUILTIN_BOX_ARRAY`, `BUILTIN_PARALLEL` 耳?댁뒪 異붽?
- [x] **P0-4: enum match OR ?⑦꽩 ?섏젙** ??`type_checker_flow.c`?먯꽌 named variant OR ?⑦꽩 ?덉슜 + coverage 泥댄겕 ?섏젙
- [x] **P2-13: match 湲곕컲 ?⑥닔 default return ?먮룞 ?앹꽦** ??`transpiler_emitters_base_b.inc`?먯꽌 non-void ?⑥닔 ??fallback return 異붽?
- [x] **Pain Point 蹂닿퀬??* ??`docs/68_pain_point_report.md`???섏젙 ?댁뿭 湲곕줉

## ?꾨즺 (理쒓렐)

- [x] **Windows ABI/backend-compare precheck ?ㅽ뻾 寃쎈줈 ?뺢퇋??*
  - `compiler_run_binary()`媛 MSYS ?ㅽ???`/tmp/...` 諛?`/<drive>/...` ?ㅽ뻾 ?뚯씪 寃쎈줈瑜?洹몃?濡?`_spawnvp()`???섍린??臾몄젣瑜??섏젙
  - Windows?먯꽌 executable launch??native Win32 寃쎈줈濡??뺢퇋?뷀븳 ???ㅽ뻾?섎룄濡??뺣젹
- [x] **nested vessel-source projection ambiguity closure**
  - zone `refresh/publish/bind` projection contract 寃쎈줈?먯꽌 ambiguous source path媛 `missing`?쇰줈 ?ㅼ쭊?섎뜕 遺꾧린 ?쒖꽌瑜??섏젙
  - builtin `ToObject` / `ToTObject`???숈씪??structured `Reason/Fix` ambiguity diagnostic?쇰줈 ?뺣젹
  - nested vessel ambiguity semantic regressions 異붽?
- [x] **generic consumer provenance diagnostics 蹂닿컯**
  - `action requires` / `zone authority` / `party role slot` / `intent step requires`?먯꽌 generic ability mismatch媛 `actual type argument` / `actual implementation` provenance瑜??④퍡 蹂닿퀬?섎룄濡??뺣젹
  - 愿??semantic ?뚭? 異붽?
- [x] **anchored own/ref provenance diagnostics 蹂닿컯**
  - closed-subset / local-only / missing `own/ref` / `ref` escape 吏꾨떒??`Reason/Fix`? borrowed-here provenance瑜?異붽?
  - 愿??semantic ?뚭? 異붽?
- [x] **world embedding structured diagnostics ?뚭? 怨좎젙**
  - embedded zone old-binding mutation??assignment / hosted func-action call 紐⑤몢?먯꽌 `Reason/Fix`? world-owned-copy provenance瑜??④린?꾨줉 semantic ?뚭? 媛뺥솕
- [x] **Windows shell smoke portability 蹂닿컯**
  - `abi_pipeline_smoke.sh`, `compare_backends.sh`媛 `cmp`/`diff` 遺???섍꼍?먯꽌??`git` ?먮뒗 Python fallback?쇰줈 鍮꾧탳/李⑥씠 異쒕젰???섑뻾?섎룄濡??뺣━
- [x] **surface trust docs ?뺣젹 ??collection/result/struct baseline**
  - `Array<T>`??`[]`, `List<T>`??`ListNew()`, `HashMap<K,V>`??`MapNew()`瑜?canonical ?앹꽦 surface濡?怨좎젙
  - `Result<T>` 異붿텧 API??`Unwrap` / `UnwrapOr` / postfix `?`濡?怨좎젙, `UnwrapResult()` ?쒕㈃? 鍮꾩콈??  - `struct` field??legacy `let`? 遺덈? ?쒖떇???꾨땲??declaration introducer?꾩쓣 臾몄꽌?뷀븯怨? ?쎄린 ?꾩슜 怨꾩빟? `object/tobject`?먮쭔 ?붾떎
- [x] **generic default-arg closure 1李?蹂듦뎄** ??declaration acceptance留뚯씠 ?꾨땲??user-defined generic class omission, generic ability impl-reference omission, arity diagnostics range?? semantic/backend parity源뚯? ?ㅼ떆 ?뱀깋?쇰줈 ?뺣젹
- [x] **ABI Unification Infrastructure** ??`pgy_abi_spec.h`, `test_abi_spec.c` (28 PASS), `MIRTypeLayout`, `mir_abi_lookup()`, `rir_dump_json()`, dumb emitter Visitor
- [x] **Windows CI Fix** ??`TOKEN_TYPE` ??`PGY_TOKEN_TYPE`, `TokenType` ??`PgyTokenType` (~20媛??뚯씪)
- [x] **v2 Quantum Planning** ???묒옄 ?곗궛 誘몄???紐낆떆, v2 怨꾪쉷 臾몄꽌??- [x] **Documentation Index** ??`docs/INDEX.md` ?앹꽦, ?꾩껜 臾몄꽌 泥닿퀎??- [x] **`HashMap<K, V>` stable key subset surface trust ?뺣젹** ??semantic annotation/builtins/runtime comment/test瑜?`String | Int | Long | Bool` key 吏?먯쑝濡??쇱튂?쒗궡
- [x] **mixed `ability + zone` module export 異⑸룎 ?섏젙** ??default-export `ability`媛 sibling zone visibility瑜?源⑤쑉由щ뜕 ?뺢퇋??踰꾧렇 ?쒓굅, module smoke ?뚭? 異붽?
- [x] **nominal host receiver type ?ㅼ뿼 ?섏젙** ??C backend member-call emit 以?static type-name overwrite瑜??쒓굅??`Int_Advance`瑜??ㅻ컻??蹂듦뎄
- [x] **MIR cleanup exceptional topology ?뚭? 蹂듦뎄** ??cleanup/rollback/invalidation block edge materialization怨?test expectation ?뺣젹
- [x] **`order_analytics` example ?ㅼ쟾??* ??sketch ?섏? surface瑜??뺣━?섍퀬 compile-smoke covered example濡??밴꺽
- [x] **declaration name surface tightening** ??declaration name???쇰컲 ?앸퀎?먮줈留??쒗븳?섍퀬 reserved keyword ?ъ궗??surface ?쒓굅
- [x] **anchored-handle diagnostics/test ?뺣젹** ??`own/ref` closed-subset 吏꾨떒 臾멸뎄? `DeviceSlot`/anchored-handle semantic test expectation???꾩옱 援ы쁽 湲곗??쇰줈 ?쇱튂?쒗궡
- [x] **怨꾩링??stdlib/domain kit v0 怨좎젙** ??`money`, `datetime(Duration/Instant)`, `timer`, `versioning`, `ledger`, `obligation`, `device_adapter` 紐⑤뱢怨?probe ?덉젣 異붽?, 肄붿뼱 異붽? 湲덉? ?먯튃 臾몄꽌??
## 踰좏? ?대줈? 蹂대뱶

踰좏? ???먯튃:
- `遺遺?援ы쁽` ?곹깭瑜??④린吏 ?딅뒗??- ?꾨즺?쒗궎吏 紐삵븯??surface???대━嫄곕굹 experimental濡?寃⑸━?쒕떎
- parser媛 諛쏅뒗 ?쒕㈃? semantic/C/LLVM/runtime/test/documentation源뚯? ?ル뒗??
### B0 ???섎?濡??대줈? ?꾩닔

- [ ] **Intent/Zone/World semantics ?꾩쟾 closure**
  - contract reuse/derivation / authority / lifecycle / embedding ownership / runtime observability / C/LLVM parity / regression
  - ?대? 議댁옱: intent orchestration, inherited/derived contract, zone/world query, observability baseline
  - 吏꾪뻾: runtime zone/world propagation cell??`epoch/cause` provenance baseline???ㅼ뼱媛붽퀬, LLVM intent rebound-zone sync??媛숈? truth濡??뺣젹??  - 吏꾪뻾: world derived-state chain? ?댁젣 bounded recompute loop瑜??듯빐 C/LLVM ?묒そ?먯꽌 媛숈? 洹쒖튃?쇰줈 怨꾩궛??  - 媛뺥븳 湲곗?: ??異뺤? ?댁젣 "?뺤? single-pass sync濡쒕룄 beta 媛?? 媛숈? ?댁꽍???덉슜?섏? ?딆쓬
- ?⑥쓬: embedding ownership/handoff policy, **handoff? ???볦? world-zone propagation family源뚯? ?쇰컲?붾맂 bounded fixpoint 湲곕컲 cross-layer propagation policy**, richer provenance query surface, declaration/runtime/diagnostic parity
  - ??異뺤? ?몄뼱 ?뺤껜???먯껜?대?濡?beta 吏곸쟾源뚯? ?댁뼱?먯? ?딅뒗??- [ ] **relation/effect/projection semantics ?꾩쟾 closure**
  - effect lattice, authority-resource partial order ?듯빀, refresh/publish/bind/causes ?쇨??? diagnostics, C/LLVM parity
  - ?대? 議댁옱: declaration, lifecycle shorthand, `refresh/publish/bind`, layer/state query, overlay sync, effect join/meet/conflict, projection contract diagnostics baseline
- 吏꾪뻾: relation/effect/zone projection hidden cell??C/LLVM 紐⑤몢 `dirty/ready + epoch/cause` schema濡??뺣젹?먭퀬 runtime contract provenance baseline???앷?
- 吏꾪뻾: world-derived recompute??bounded pass loop濡??щ씪?붽퀬, relation/effect/zone projection chain??bounded transitive recompute loop濡??щ씪?붾떎
- 媛뺥븳 湲곗?: projection propagation? ???댁긽 "helper replay媛 ?泥대줈 留욎쓬" ?섏??쇰줈 ?먯? ?딄퀬, transitive semantics媛 ?ロ엳湲??꾧퉴吏 beta blocker濡??좎?
- ?⑥쓬: authority-resource partial order ?듯빀, projection/layer/state瑜??섏뼱??**authority/failure handoff? ???볦? world-zone propagation family源뚯???full transitive frontier propagation policy**, helper-heavy edge path 媛먯냼, declaration/runtime/diagnostic/backend parity??留덉?留?shrink
  - ??異뺤? domain semantics ?듭떖?대?濡?partial ?곹깭濡?beta???щ━吏 ?딅뒗??  - projection diagnostics??`target/source/projection kind/field path/fix`瑜??ы븿?섍퀬 `Reason:` / `Fix:` ?щ㎎?쇰줈 怨좎젙?쒕떎
- [x] **generic contract ?꾩쟾 closure**
  - strict beta-quality 湲곗??쇰줈 stable subset closure?먯꽌 ?ш컻諛?  - `default type arg` actual resolution, `where T: A + B` ?꾧꼍濡?enforcement, `ability<T>` mismatch provenance, instantiation-path parity源뚯? ?ル뒗??  - ?꾨즺: default type arg declaration acceptance / omitted trailing default resolution / generic ability impl-reference omission / arity diagnostics provenance
  - ?대? 議댁옱: `ability<T>` baseline, default type arg baseline, omitted trailing default resolution, generic mismatch provenance baseline
  - 吏꾪뻾: `party role slot` generic mismatch??`consumer path / expected type args / actual type args` vocabulary ?뚭?濡?怨좎젙
  - ?⑥쓬: multi-bound ?꾧꼍濡?enforcement, module-contract propagation, instantiation-path parity, richer mismatch diagnostics, wider C/LLVM regression ?뺣?
  - generic mismatch??`generic subject / expected type args / actual type args / broken bound / consumer path / fix`瑜??ы븿?섍퀬 `Reason:` / `Fix:` ?щ㎎?쇰줈 怨좎젙?쒕떎
  - generic? partial acceptance瑜?beta???щ━吏 ?딅뒗??- [x] **own/ref ?꾩쟾 closure**
  - strict beta-quality 湲곗??쇰줈 anchored subset closure?먯꽌 ?ш컻諛⑺뻽怨? classifier-backed stable subset?쇰줈 留덇컧
  - ?쇰컲 movable type ownership, move/borrow/escape/rebind/channel/return provenance, diagnostics/test parity源뚯? ?レ쓬
  - ?대? 議댁옱: anchored slot subset, anchored diagnostics baseline, anchored regression/docs alignment
  - ?꾨즺: summary/direct path family audit? classifier/docs 理쒖쥌 ?뺣젹
  - 吏꾪뻾: constructor field store escape 寃쎈줈瑜?boundary-visible store濡?怨좎젙?섍퀬 ?뚭? 異붽?
  - 吏꾪뻾: array literal store escape 寃쎈줈瑜?boundary-visible store濡?怨좎젙?섍퀬 ?뚭? 異붽?
  - 吏꾪뻾: assignment rebind escape diagnostic??member/aggregate target path(`holder.packet`, `items[0]`) provenance瑜?吏곸젒 蹂닿퀬?섎룄濡??뺣젹
  - 吏꾪뻾: nested projection provenance媛 constructor field store / member rebind / list/set/queue/map store / array overwrite / helper return summary / channel send / direct return源뚯? ?뚭?濡?怨좎젙??  - 吏꾪뻾: class/subject consumer matrix??return / channel / helper / list / set / queue / map / array push / array overwrite / member rebind / constructor field store源뚯? 嫄곗쓽 ?숉삎?쇰줈 ?뺣젹
  - 吏꾪뻾: tuple/object 寃쎈줈??湲곗〈 `test_semantic.c` ?뚭? 異뺤뿉??channel/new-binding/rebind/return/helper forwarding/queue-map-array overwrite/projection provenance coverage ?좎?
  - 吏꾪뻾: slot-handle/class helper-chain ?뚭???ownership-boundaries 怨꾩뿴??異붽???direct helper/function call family媛 transitive chain源뚯? 怨좎젙??  - 吏꾪뻾: helper/return/channel wording family瑜?`through ...` 湲곗??쇰줈 ?뺣젹
  - ownership diagnostics??`value / ownership mode / moved|borrowed here / escaped|rebound here / consumer path / fix`瑜??ы븿?섍퀬 `Reason:` / `Fix:` ?щ㎎?쇰줈 怨좎젙?쒕떎
  - explicit reject: authority-bearing `Token<T>` escape/transport
  - beta-out-of-scope: region/lifetime solver? universal ownership lattice

### B1 ??踰좏? ?좊ː???꾩닔

- [x] **surface trust 臾몄꽌 ?щ텇瑜?*
  - ?꾨즺: `docs/18_language_status.md`, `docs/63_feature_depth_matrix.md`, `README.md`?먯꽌 `stable subset / explicit reject / beta-out-of-scope` 湲곗??쇰줈 ?뺣젹
  - 洹쒖튃: "而댄뙆?쇱? ?섏?留?partial"???쒕㈃??stable泥섎읆 ?곗? ?딄퀬, ?대뵒源뚯?瑜??ロ엺 怨꾩빟?쇰줈 ?쎌냽?섎뒗吏 癒쇱? 紐낆떆
  - 洹쒖튃: broader generalization, arbitrary key support, general ownership, richer observability query 媛숈? ??ぉ? `beta-out-of-scope`濡?遺꾨━
- [ ] **stable example / smoke source of truth ?뺣?**
  - canonical examples? closure examples瑜?smoke??吏곸젒 ?곌껐
  - explicit surface vs compressed surface瑜?媛숈? ?섎?濡?蹂댁뿬二쇰뒗 pair example 理쒖냼 4??怨좎젙
  - ??? app/web orchestration, game/simulation, async/worker/device, world-handoff/domain propagation
- [ ] **Backend parity final closure**
  - C/LLVM??domain semantics 湲곗??쇰줈 媛숈? 寃곌낵瑜??대뒗吏 怨좎젙
  - ??? intent/zone/world, relation/effect/projection, ownership boundary, refresh/publish/bind, world embedding/handoff
  - 湲곗?: backend compare / llvm smoke / example smoke / ABI-runtime probe媛 Linux/Windows 紐⑤몢 ?뱀깋
- [ ] **experimental surface ?쒓굅 ?먮뒗 寃⑸━**
  - ?レ? 紐삵븳 parser surface??紐낆떆 嫄곕? ?먮뒗 臾몃쾿 ?쒓굅

## Pain point freeze board

?먯튃:
- 湲곕뒫?????볧엳湲??꾩뿉 諛섎났?댁꽌 ?ㅼ떆 源⑥????묒꽦/吏꾨떒 pain point瑜?癒쇱? 怨좎젙?쒕떎
- 媛?pain point??`stable contract + regression + docs wording`源뚯? 媛숈씠 ?좉렐??- recoverable failure? invariant break瑜?媛숈? 諛⑹떇?쇰줈 泥섎━?섏? ?딅뒗??
### Failure handling policy freeze

遺꾨쪟:
- `recoverable failure`
  - ?ъ슜??肄붾뱶媛 ?덉긽 媛?ν븳 ?ㅽ뙣
  - ?? intent failure, authority/boundary rejection, timeout, remote failure, empty/closed operational state
  - ?먯튃:
    - ?꾨줈?몄뒪瑜?二쎌씠吏 ?딅뒗??    - `Bool` / `Result<T>` / queryable runtime state濡??쒕윭?몃떎
    - reason / boundary / authority / step provenance瑜?議고쉶 媛?ν븯寃??④릿??- `contract violation`
  - ?먯튃?곸쑝濡?semantic ?④퀎?먯꽌 李⑤떒
  - ?고??꾧퉴吏 ?ㅻ㈃ structured panic
  - ?? released slot access, invalid secure token, ownership boundary ?꾨컲
- `internal compiler/runtime bug`
  - 利됱떆 以묐떒
  - internal error / panic濡?紐낇솗??遺꾨━
  - ?ъ슜??肄붾뱶 ?ㅽ뙣泥섎읆 ?꾩옣?섏? ?딅뒗??
?꾩옱 怨좎젙:
- intent/zone/world 履??ㅽ뙣???κ린?곸쑝濡?`recoverable failure`濡??섎졃?쒗궓??- slot/token/invariant 怨꾩뿴? 怨꾩냽 hard fail濡??붾떎
- `Unwrap(...)`??panic ?깃꺽??sharp tool濡??좎??섍퀬, recoverable path??湲곕낯 怨꾩빟?쇰줈 ?곗? ?딅뒗??
- [ ] **large canonical pair ?덉젣 異붽?**
  - ???덉젣?먯꽌 `explicit`? `compressed`瑜?????stable source of truth濡??좎??쒕떎
  - 理쒖냼 4媛??뚯씪 湲곗??쇰줈 愿由ы븳??    - `calendar manage-event`: explicit/compressed
    - `composite intent orchestration`: explicit/compressed
  - 紐⑹쟻:
    - ???덉젣???꾩껜 怨꾩빟??紐낆떆?뺤쑝濡??쎌쓣 ???덇쾶 ?좎?
    - 媛숈? ?섎?瑜?異뺤빟?뺤쑝濡쒕룄 諛붾줈 蹂듭궗???쒖옉?????덇쾶 ?좎?
    - smoke?먯꽌 ???덉젣媛 紐⑤몢 ?ㅽ뻾 媛?ν븯?꾨줉 怨좎젙
- ??蹂대뱶??sugar backlog媛 ?꾨땲??beta surface trust瑜?吏?ㅺ린 ?꾪븳 怨좎젙?먯씠??- P0 pain point媛 ?좉린湲??꾩뿉??declaration-side MIR-only debt瑜?援?냼 蹂듦뎄 ?몄뿉???볤쾶 嫄대뱶由ъ? ?딅뒗??- backend ?대? ?뺣━??pain point 湲곗??좉낵 ?뚭?媛 癒쇱? 怨좎젙???ㅼ뿉留??ㅼ떆 ?뺤옣?쒕떎

### P0 ???묒꽦/怨꾩빟 pain point

- [ ] **contract clause density 怨좎젙**
  - ??? `requires / within / authorized by / causes / refresh / publish / bind`
  - 臾몄젣: 媛숈? ?섎?瑜?action / intent step / zone?먯꽌 以묐났 湲곗닠?섍쾶 ?섏뼱 ?묒꽦 ?쇰줈媛 而ㅼ쭚
  - 怨좎젙 湲곗?:
    - ?대뵒源뚯? inherited/derived ?섎뒗吏 vocabulary瑜?怨좎젙
    - 湲멸쾶 ?곕뒗 踰꾩쟾怨??뺤텞 踰꾩쟾???섎? 李⑥씠媛 臾몄꽌/吏꾨떒/?덉젣?먯꽌 媛숈븘????    - canonical pair? minimal subset example????븷??遺꾨━??source-of-truth瑜?怨좎젙
  - ?뚭? 湲곗?:
    - semantic regression: inherited/derived contract source媛 吏꾨떒???몄텧
    - example smoke: long-form vs compressed-form ?덉젣 ?????좎?

?꾩옱 source-of-truth:
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

- [x] **contract provenance vocabulary 怨좎젙**
  - ?꾨즺: beta closure 臾몄꽌??contract provenance ?쒖??대? `derived / inherited`濡?怨좎젙
  - 洹쒖튃: contract source ?ㅻ챸?먯꽌??`inferred`瑜??곗? ?딄퀬, action?먯꽌 ?ъ궗?⑸맂 step clause??`inherited`, `using/transfer` ???꾩옱 step?먯꽌 怨꾩궛??clause??`derived`濡?遺瑜몃떎
  - 洹쒖튃: diagnostics / AST print / docs媛 媛숈? ?⑹뼱瑜??곕룄濡?留욎텛怨? `inferred`???쇰컲 ???怨꾩궛?대굹 non-contract internal analysis 臾몃㎘?먮쭔 ?④릿??  - ??? contract provenance ?붿뿬 ?쒗쁽, contract source wording, docs/example terminology
  - 臾몄젣: compiler type/effect inference? domain contract ?곸냽/?뚯깮??媛숈? ?⑥뼱濡??욎씠硫??ㅻ챸?μ씠 臾대꼫吏?  - 怨좎젙 湲곗?:
    - domain contract??`?곸냽 / ?뚯깮`怨?`inherited / derived`濡쒕쭔 遺瑜몃떎
    - ?쇰컲 compiler ?섎???type/effect `inference`?먮쭔 ?④릿??  - ?뚭? 湲곗?:
    - parser/semantic diagnostics 湲곕? 臾몄옄??怨좎젙

### P0.5 ??recoverable failure 遺꾨쪟/怨좎젙

- [x] **failure class inventory ?뺣━**
  - ?꾨즺: `docs/07_error_handling.md`, `docs/18_language_status.md`, `README.md` 湲곗??쇰줈 `recoverable failure / contract violation / internal bug` inventory瑜??뺣━
  - ?꾨즺: ?꾩옱 recoverable ?좎? ??ぉ, hard-fail ?좎? ??ぉ, ?꾩냽 downshift ???authority rejection ????援щ텇
  - 洹쒖튃: runtime invariant guard? real domain rejection??媛숈? ?ㅽ뙣 痢듭쑝濡??욎? ?딆쓬
- ?꾩옱 inventory baseline:
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
      - 李멸퀬: ?닿굔 ?꾩쭅 real authority rejection???꾨땲??invariant check?쇱꽌 hard-fail ?좎? 履쎌씠 留욌떎
  - first-wave conversion targets:
    - future real runtime authority rejection
    - intent boundary/authority mismatch provenance at runtime
- [ ] **intent/zone/world recoverable failure baseline**
  - intent failure, authority rejection, boundary mismatch??process abort ???queryable reason/state濡??몄텧
  - runtime observability? diagnostics wording??媛숈? provenance vocabulary濡??뺣젹
  - 李멸퀬: runtime propagation provenance(`epoch/cause`) baseline? ?꾨즺濡?蹂몃떎
  - 吏꾪뻾: runtime zone authority invariant guard??`last_ok / zone / participant / code / reason` thread-local snapshot???④린?꾨줉 ?뺣젹?섏뼱, hard-fail guard? 蹂꾧컻濡?理쒖냼 queryable failure snapshot baseline? ?앷꼈??  - 吏꾪뻾: authority failure code/reason/stderr format? `src/runtime/pgy_runtime_authority_contract.h`濡??밴꺽?덈떎. inline C runtime怨?LLVM runtime library export媛 媛숈? contract macro瑜??ъ슜?섍퀬 `runtime-authority-contract-test-smoke`媛 raw literal drift瑜?李⑤떒?쒕떎
  - 吏꾪뻾: intent emitter??MIR `IntentAuthorizedBy` metadata瑜?C/LLVM ?묒そ?먯꽌 ?섏쭛?섍퀬, step-local approval??`pgy_zone_authority_validate_flags_export(...)`濡?寃利앺빐 `authority:<step>` recoverable intent failure? runtime authority snapshot??媛숈? 寃쎈줈濡??④릿??  - 吏꾪뻾: intent `authorized by`??concrete zone subject slot?쇰줈 ?댁꽍?섎ŉ, 媛숈? ??낆쓽 non-authority slot ?먮뒗 ambiguous same-type slot mapping? semantic hard error濡??ロ삍??  - 吏꾪뻾: concrete direct-slot participant alias??ambiguous same-type ?꾨낫蹂대떎 ?곗꽑?쒕떎. `subject slot rogue: Adventurer`媛 議댁옱?섎㈃ `authorized by rogue`??concrete authority slot?쇰줈 ?ロ엳硫? ?댁쟾 ?꾨낫媛 ?몄슫 stale ambiguity flag??臾댁떆?쒕떎
  - ?뚭?: `intent authorized participant must resolve to authority slot`, `intent authorized participant reports ambiguous authority slot`
  - ?뚭?: `dnd_tavern_campaign` example smoke媛 multi-subject same-type zone?먯꽌 direct authority aliases瑜?end-to-end濡?怨좎젙?쒕떎
  - ?뚭?: `intent_authority_snapshot_abi`, `intent_authority_snapshot`
  - ?뚭?: `authority_failure_abi`, `authority_failure_surface`, `runtime-authority-contract-test-smoke`
  - ?⑥쓬: missing-zone/missing-participant ?댄썑??richer authority mismatch/domain-boundary denial reason??媛숈? queryable contract濡??뺤옣?댁빞 ?쒕떎
- [ ] **runtime authority guard downshift**
  - ?꾩옱 `pgy_zone_authority_check_export(...)`??null self/null participant invariant guard??  - ??guard ?먯껜??hard-fail ?좎?
  - 吏꾪뻾: C inline validator, LLVM runtime export, intent step-local `authorized by` validation 紐⑤몢 留덉?留?authority validation 寃곌낵瑜?媛숈? vocabulary(`last_ok`, `zone`, `participant`, `code`, `reason`)濡??④릿??  - 蹂꾨룄 real authority rejection runtime path媛 ?앷린硫?洹몄そ??`recoverable authority failure` 寃쎈줈濡??ㅺ퀎
- [x] **hard-fail boundary 紐낆떆**
  - ?꾨즺: `README.md`? `docs/07_error_handling.md`??hard-fail boundary瑜?紐낆떆
  - 怨좎젙 ?댁슜: released slot, invalid token, ownership invariant break, unwrap misuse, bounds violation, runtime invariant guard??怨꾩냽 panic / hard-fail territory濡??붾떎
  - 怨좎젙 ?댁슜: recoverable authority rejection怨?invariant guard瑜?媛숈? 痢듭쑝濡??욎? ?딅뒗?ㅻ뒗 ?먯쓣 臾몄꽌 wording?쇰줈 紐삳컯??
- [ ] **projection contract diagnostics 怨좎젙**
  - ??? `refresh/publish/bind` source/target/path/field-map ?ㅽ뙣
  - 臾몄젣: projection? ?몄뼱 媛뺤젏?몃뜲 ?ㅽ뙣 ?댁쑀媛 ?쏀븯硫?媛??癒쇱? ?쇰줈瑜?以?  - 怨좎젙 湲곗?:
    - target slot / source slot / projection kind / field path / fix媛 紐⑤몢 吏꾨떒???ㅼ뼱媛?    - structured `Reason:` / `Fix:` formatting??source-of-truth濡?怨좎젙
  - ?뚭? 湲곗?:
    - semantic regression: missing source field / ambiguous path / wrong projection kind / duplicate field map
  - 吏꾪뻾: `projection-diagnostic-contract-test-smoke`媛 ??4媛?踰좏? ?꾩닔 吏꾨떒 耳?댁뒪? `Reason:` / `Fix:` / projection consumer path vocabulary瑜?semantic regression, implementation, proof doc 湲곗??쇰줈 ?④퍡 寃?ы븳??
?꾩옱 source-of-truth:
- stable example
  - `examples/projection_bind_group_minimal.pgy`
  - `examples/projection_refresh_publish_group_minimal.pgy`
- semantic regression
  - `src/test_semantic.c:test_projection_contract_diagnostics`
  - `make projection-diagnostic-contract-test-smoke`

- [x] **surface trust subset 遺꾨쪟 怨좎젙**
  - ??? generics, own/ref, collections, runtime observability
  - 臾몄젣: ?섎뒗 寃껋쿂??蹂댁씠?붾뜲 ?ㅼ젣濡쒕뒗 subset留??섎뒗 surface媛 媛?????좊ː ?먯긽 吏??  - 怨좎젙 湲곗?:
    - `stable subset / explicit reject / beta-out-of-scope`瑜?TODO/docs/diagnostic?먯꽌 媛숈? 留먮줈 ?대떎
  - ?뚭? 湲곗?:
    - semantic tests? depth docs媛 媛숈? subset??媛由ы궡
  - ?꾩옱 湲곗? 臾몄꽌:
    - `README.md`??`Surface trust policy`
    - `docs/18_language_status.md`
    - `docs/63_feature_depth_matrix.md`
    - `docs/64_depth_filling_roadmap.md`

?꾩옱 怨좎젙?섎젮??baseline:
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

### P1 ???대? 援ъ“ pain point

- [ ] **declaration-side MIR-only debt 怨좎젙**
  - ??? declaration inventory / metadata helper / duplicated named-decl lookup
  - 臾몄젣: routine body??MIR濡??뺣━?쇰룄 decl-side helper debt媛 ?⑥쑝硫?parity bug媛 諛섎났??  - 怨좎젙 湲곗?:
    - backend lookup? 怨듯넻 inventory helper瑜??ъ슜
    - ?⑥? debt???쒓린??誘멸뎄?꾟앹씠 ?꾨땲???쏛ST-carried decl metadata 援ъ“ debt?앸줈 遺꾨━?댁꽌 湲곕줉
  - ?뚭? 湲곗?:
    - LLVM/C backend helper duplication 媛먯냼
    - debt ledger? TODO ?쒗쁽 ?뺣젹
  - ?꾪솴:
    - 吏꾪뻾: MIR declaration emit state restore??helper ?섎굹濡?臾띠?怨? role host lookup? active inventory-only 履쎌쑝濡???醫곸븘議뚮떎
    - 吏꾪뻾: 議곌린 return 寃쎈줈??`current_host_decl` / `current_func_decl` 蹂듦뎄媛 emitter 蹂몃Ц 以묐났 ???怨듭슜 restore helper瑜??寃??먮떎
    - role / party / roster / relation / effect / zone / world declaration method body??AST fallback???쒓굅??    - ?⑥? debt??declaration inventory / naming helper / named-decl lookup??援ъ“ ?뺣━ 履쎌쑝濡?異뺤냼??    - 吏꾪뻾: `emit_func_decl_from_mir_named(...)`媛 outer host restore?먯꽌 raw saved host-name fallback蹂대떎 `saved_host_decl + current_func_decl`瑜??곗꽑 ?곕룄濡??뺣젹
    - 吏꾪뻾: host restore/current-host lookup??inventory?먯꽌 host decl??紐?李얠쑝硫?raw `current_*_name` ?곹깭瑜??좎??섏? ?딄퀬 host handle??鍮꾩슦?꾨줉 ?뺣젹
    - 吏꾪뻾: `transpiler_restore_host_context_local(...)` ?쒓렇?덉쿂??`saved_host_decl` 以묒떖?쇰줈 異뺤냼??decl-side restore?먯꽌 raw name ?몄옄瑜??쒓굅
    - ?꾩옱 inventory:
      - `src/codegen/transpiler_helpers_core_b.inc`: `current_host_decl_name` ?곹깭 ?먯껜? ?쇰? host naming helper ?뺣━
      - `src/codegen/llvm_pipeline.c`: AST-carried declaration inventory瑜??대뒗 `MIRProgram` bootstrap 寃쎈줈
      - 怨듯넻 怨쇱젣: current_* name ?곹깭? ad-hoc named lookup瑜?MIR declaration metadata query濡?移섑솚
    - 理쒓렐 ?뺣━:
      - `current_field_type_name`, `current_host_method_decl`, `find_nominal_host_method_decl`??active inventory 寃쎌쑀 lookup濡??뺣젹??      - transpiler host context 蹂듦뎄??`current_host_decl -> within_zone -> saved host-name inventory` ?쒖쑝濡??뺣젹??      - transpiler emitter hot path??direct `current_*_name` 李몄“??helper/restore layer ?꾩＜濡?異뺤냼??      - LLVM declaration helper / MIR-domain emission / expr-call builtin path??`llvm_current_host_decl_name(...)`? bind/restore helper 履쎌쑝濡??대룞??      - LLVM `llvm_current_host_decl(...)`?????댁긽 `current_class_name` ?ъ“??fallback???섏〈?섏? ?딄퀬 bound host handle / `within_zone`留뚯쓣 truth濡??ъ슜??      - `llvm_pipeline.c`??nominal declaration registration怨?class-method enumeration??raw `decl_header->methods` 吏곸젒 ?묎렐蹂대떎 active nominal inventory / `llvm_find_host_decl_methods_in_context(...)` 寃쎌쑀濡??대룞??      - `llvm_register.c`??active nominal registration??`mir->decl_headers` 吏곸젒 ?쒗쉶 ???active nominal inventory 湲곗??쇰줈 ?뺣젹??      - `make mir-declaration-inventory-test-smoke`瑜?異붽???C/LLVM declaration/domain/nominal active inventory helper seam怨?pipeline/domain ?뚮퉬 寃쎈줈瑜?static gate濡?怨좎젙?덈떎. ??raw MIR declaration array access??owner ?뚯씪 諛뽰뿉??議곗슜???섏뼱?????녿떎
      - C backend `emit_program(...)`??executable metadata??`mir->has_*` / `mir_find_function_decl(...)` 吏곸젒 ?묎렐 ???`transpiler_active_*` helper瑜??듦낵?섎룄濡??뺣젹?덈떎
      - C backend `emit_program(...)`??ability/type/extern/function/intent/domain/event declaration bootstrap ?쒗쉶??direct `mir->...` array/count ?묎렐 ???`transpiler_active_inventory(...)` / `transpiler_active_externs(...)` view瑜??ъ슜?섎룄濡??뺣젹?덈떎
      - `MIRDeclMethod`??hosted method identity, routine link, signature metadata源뚯? ?닿퀬 LLVM nominal/enum prototype registration? `llvm_mir_decl_method_*` helper瑜??듯빐 ??row瑜?癒쇱? ?뚮퉬?쒕떎
      - ?⑥? ?듭떖 debt??LLVM pipeline??AST-carried declaration inventory bootstrap? helper/restore layer 諛붽묑??raw host-name state ?쒓굅

- [x] **ownership vocabulary / payload cleanup 1李?怨좎젙**
  - ??? semantic ownership diagnostics / payload helper family / wording drift
  - ?꾨즺:
    - `src/semantic/type_checker_ownership_boundaries.inc`??ownership helper 9醫낆씠 `DiagPayload`/`semantic_emit_payload(...)` ?⑦꽩?쇰줈 ?뺣젹??    - semantic direct `semantic_error_with_hints(...)` ?몄텧? ownership-boundary helper ?대??먯꽌 ?쒓굅??    - vocabulary 1李??뺣━:
      - `anchored handle` ??`slot handle (anchored)`
      - `movable resource handle` / `movable resource` ??`slot handle (movable)`
      - `capability-bearing` ??`authority-bearing` (ownership/domain wording 湲곗?)
    - semantic ?뚭????꾩옱 wording 湲곗??쇰줈 ?ㅼ떆 怨좎젙??  - 寃利?
    - `make test-semantic` ??`1872 passed, 0 failed`
    - `make test-transpile` ??`601 passed, 0 failed`
  - ?⑥? 寃?
    - P3 ?붿뿬 ?몃텇瑜?`boundary value (subject)` ?? 異붽? ?뺤텞
    - payload/helper family瑜?ownership 諛붽묑 semantic diagnostics濡????뺤옣
    - own/ref call/consumer path?먯꽌 classifier 湲곕컲 trivial copy-only semantics瑜????볤쾶 ?곸슜
    - destructure target binding / nested projection / helper-chain wording??consumer kind 湲곗??쇰줈 ???몃텇??
- [ ] **type-resolution DAG ?붿쭊 ?꾩엯**
  - ??? semantic type resolution / generic consumer resolution / declaration dependency scheduling
  - 臾몄젣: ?꾩옱??`resolve_type_node(...)` 以묒떖???ш? ?댁꽍 + scope lookup + ad-hoc validation??二쇱텞?대씪, module import graph??遺꾨챸?섏?留?type dependency ?먯껜??compiler-wide DAG濡?愿由щ릺吏 ?딅뒗??  - 理쒓렐 吏꾪뻾:
    - `TypeResolutionGraph` inventory + cycle diagnostic + topo derivation? ?ㅼ젣 ?쒖꽦 ?곹깭
    - staged worklist??provider-first ??닚 topo ?쒗쉶濡?怨좎젙??    - local contract / projection synthetic node??label蹂?narrow handler濡??뚮퉬??    - generic `default_type` / generic constraint / `where` bound??staged DAG resolver 寃쎈줈???몄엯??    - graph regression? world lifecycle / relation-effect propagation / generic consumer schedule / alias cycle provenance / generic default-bound cycle provenance / action-intent-zone-party ability consumer provenance源뚯? ?ы븿
    - graph validator cycle怨?legacy alias-resolution cycle??紐⑤몢 `Contract source:` / `Reason:` / `Fix:` 援ъ“濡??뺣젹??    - 吏꾪뻾: type constraint bound formatter??`type_checker_type_constraint.c`濡??ㅼ젣 TU 遺꾨━ ?꾨즺
    - 吏꾪뻾: graph node/edge/path/cycle-format primitive??`type_checker_resolution_graph_core.c`濡??ㅼ젣 TU 遺꾨━ ?꾨즺
    - 吏꾪뻾: named dependency edge recorder? 利됱떆 cycle diagnostic 諛쒗뻾 寃쎈줈??`type_checker_resolution_graph_core.c`濡??ㅼ젣 TU 遺꾨━ ?꾨즺
    - 吏꾪뻾: type-ref dependency recorder??`type_checker_resolution_graph_core.c`濡??대룞?덇퀬, `find_type_alias_decl`??cross-include dangling return-type seam??紐낆떆 ?좎뼵?쇰줈 ?뺣━
    - 吏꾪뻾: type-ref collector??`type_checker_resolution_graph_collect.c`濡??대룞?덇퀬, graph core/include 寃쎄퀎??dangling `static void` seam???쒓굅
    - 吏꾪뻾: generic contract inventory / string dependency / required ability collector helpers??`type_checker_resolution_graph_collect.c`濡??대룞??declaration collector?ㅼ쓽 怨듯넻 ?섏〈??TU 寃쎄퀎濡??밴꺽
    - 吏꾪뻾: top-level declaration graph registration? `type_checker_resolution_graph_collect.c`濡??대룞??inventory `.inc`瑜?1,962 LOC源뚯? 異뺤냼
    - 吏꾪뻾: local-contract graph node/dependency + zone/world/projection label formatters??`type_checker_resolution_graph_labels.c`濡??대룞??inventory `.inc`瑜?1,835 LOC源뚯? 異뺤냼
    - 吏꾪뻾: projection source resolver??`type_checker_resolution_graph_domain.c`濡??대룞?섍퀬 `find_zone_domain_slot`??internal API濡??밴꺽??inventory `.inc`瑜?1,809 LOC源뚯? 異뺤냼
    - 吏꾪뻾: event declaration precollector??`type_checker_resolution_graph_decl.c`濡??대룞??inventory 蹂몄껜?먯꽌 declaration-kind collector瑜?泥??덈떒
    - 吏꾪뻾: enum declaration precollector??`type_checker_resolution_graph_decl.c`濡??대룞?섍퀬 `semantic_stage_method_array`瑜?internal API濡??밴꺽??inventory `.inc`瑜?1,765 LOC源뚯? 異뺤냼
    - 吏꾪뻾: ability declaration precollector? action-contract precollector??`type_checker_resolution_graph_decl.c`濡??대룞??inventory `.inc`瑜?1,648 LOC源뚯? 異뺤냼
    - 吏꾪뻾: role/class/party/roster declaration precollector??`type_checker_resolution_graph_decl.c`濡??대룞?섍퀬, relation/effect domain inventory precollector??`type_checker_resolution_graph_domain.c`濡??대룞??inventory `.inc`瑜?1,299 LOC源뚯? 異뺤냼
    - 吏꾪뻾: intent declaration precollector? world inventory precollector瑜?媛곴컖 `type_checker_resolution_graph_decl.c`, `type_checker_resolution_graph_world.c`濡??대룞??inventory `.inc`瑜?870 LOC源뚯? 異뺤냼
    - 吏꾪뻾: zone projection field-map collector瑜?`type_checker_resolution_graph_zone.c`濡?遺꾨━?덇퀬, ?⑥? inventory body瑜?`type_checker_resolution_graph_inventory.c`濡??밴꺽??inventory `.inc`瑜??쒓굅
    - 吏꾪뻾: world/zone local-contract stage replay瑜?`type_checker_resolution_stage_domain.c`濡?遺꾨━?섍퀬, ?⑥? stage 蹂몄껜瑜?`type_checker_resolution_stage.c`濡??밴꺽??stage `.inc` ?쒓굅
    - 吏꾪뻾: class/extern declaration checker瑜?`type_checker_class_decl.c`濡? top-level semantic orchestration??`type_checker_program.c`濡?遺꾨━??program `.inc`瑜?624 LOC源뚯? 異뺤냼
    - 吏꾪뻾: `ToObject` / `ToTObject` projection checker瑜?`type_checker_builtins_projection.c`濡?遺꾨━??builtins nominal `.inc`瑜?659 LOC源뚯? 異뺤냼
    - 吏꾪뻾: domain helper? intent helper瑜?媛곴컖 `type_checker_decls_domain_helpers.c`, `type_checker_intent_helpers.c`濡??밴꺽??semantic `.inc` 800 LOC stop condition???ъ꽦?섍퀬 `make semantic-inc-size-test-smoke`濡??뚭? 諛⑹?
    - 吏꾪뻾: C backend `transpiler_emitters_mir_inventory_ssa.inc`瑜?3媛??섏쐞 slice濡?遺꾨━?섍퀬 `make test-transpile`, `make llvm-test-backend-compare`濡?parity ?뚭? ?듦낵
    - 吏꾪뻾: standalone TU ?밴꺽 以??쒕윭??dangling return-type seams? implicit helper dependency瑜??쒓굅??`make test-all`, `make llvm-test-backend-compare` ?뚭? ?듦낵
    - 吏꾪뻾: implicit declaration / implicit int??湲곕낯 CFLAGS?먯꽌 ?먮윭濡?怨좎젙?섏뼱 ?댄썑 DAG/semantic split 以?hidden helper dependency媛 利됱떆 ?ㅽ뙣?섎룄濡??뺣젹
    - 吏꾪뻾: `type_resolution_intern_node` / `type_resolution_add_edge` / `type_resolution_find_path` / `type_resolution_format_cycle`??include-order static helper?먯꽌 `type_checker_internal.h` internal API濡??밴꺽
    - 吏꾪뻾: DAG stage ?덉뿉???꾩쭅 `resolve_type_node(...)`濡??대젮媛??legacy fallback??`PGY_TYPE_RES_STATS=1` ?듦퀎???몄텧?덈떎. `stage-legacy-resolve: calls/failed/suppressed_diagnostics`? `stage-legacy-family: generic_contract/signature/ability_consumer/domain_contract/alias/other`媛 異쒕젰?섎ŉ, `make type-resolution-dag-test-smoke`媛 graph stats, topo validation, legacy fallback inventory 議댁옱瑜?CI gate濡?怨좎젙?쒕떎
    - 吏꾪뻾: type-alias stage??quiet resolve ?깃났 寃곌낵瑜??ъ궗?⑺븯?꾨줉 ?뺣━???깃났 寃쎈줈??以묐났 `resolve_type_node(...)` ?몄텧???쒓굅?덈떎. ?ㅽ뙣 寃쎈줈??湲곗〈 diagnostic fallback???좎??쒕떎
    - 吏꾪뻾: DAG edge媛 ?대? 議댁옱?섎뒗 named type-ref??generic argument瑜??ы븿??stage?먯꽌 `resolve_type_node(...)`瑜??ㅼ떆 ?몄텧?섏? ?딄퀬 graph-backed skip?쇰줈 泥섎━?쒕떎. `stage-graph-backed: skips=N` ?듦퀎媛 異붽??먭퀬 `type-resolution-dag-test-smoke`媛 skip ?⑷퀎媛 0?쇰줈 ?댄뻾?섏? ?딅뒗吏 寃?ы븳??    - 吏꾪뻾: graph precollect TU ?덉뿉??enum methods媛 `semantic_stage_method_array(...)`瑜??몄텧?섎뜕 impurity瑜??쒓굅?덈떎. ?댁젣 enum method signature/contract??precollect action contract 寃쎈줈濡쒕쭔 graph edge瑜??섏쭛?쒕떎
    - 吏꾪뻾: DAG stage helper瑜?`type_checker_resolution_stage_lookup.c` / `type_checker_resolution_stage_stats.c`濡?遺꾨━??`type_checker_resolution_stage.c`瑜?895 LOC濡???톬?? graph precollect, stage lookup, stage stats, stage replay owner媛 ?뚯씪 寃쎄퀎濡?遺꾨━?먮떎
    - 吏꾪뻾: generic where/default validation? `type_checker_generic_validation.c`濡??대룞?덈떎. `type_checker_resolution_graph_*.c`? `type_checker_resolution_graph_core.inc`?????댁긽 `resolve_type_node(...)`瑜?吏곸젒 ?몄텧?섏? ?딆쑝硫? `semantic-core-shape-test-smoke`媛 ??resolver-free graph-layer 寃쎄퀎瑜?寃?ы븳??    - 吏꾪뻾: graph precollect媛 context-independent builtin type refs(`Int`, `Long`, `Float`, `Double`, `Bool`, `String`, `QubitSlot`, `Void`)瑜?`SemanticContext.type_resolution_metadata`??湲곕줉?쒕떎. owner resolver seams????metadata瑜?癒쇱? 議고쉶????recursive fallback?쇰줈 ?대젮媛꾨떎
    - 吏꾪뻾: graph metadata媛 resolver-stable constructed/anchored-handle shells(`Array<T>`, `Slice<T>`, `List<T>`, `Queue<T>`, `Set<T>`, `Box<T>`, `Rc<T>`, `Weak<T>`, `Channel<T>`, `Future<T>`, `RemoteFuture<T>`, `Token<T>`, `DeviceSlot<T>`, `HashMap<String|Int|Long|Bool, T>`, `Option<T>`, `Result<T,E>`, `Slot<T>`, `SecureSlot<T>`, `ReadView<T>`, `WriteView<T>`, `MoveToken<T>`)瑜?materialize?????덈떎. graph媛 留뚮뱺 `Type` shell? metadata owned lane?쇰줈 湲곕줉?섍퀬 semantic context destroy?먯꽌 ?댁젣?쒕떎
    - 吏꾪뻾: graph metadata媛 tuple shell怨?event-handler/function shell??materialize?쒕떎. channel/future AST node??inner fact collect 吏곹썑 constructed shell??湲곕줉?섎?濡?recursive fallback?????섏〈?쒕떎
    - 吏꾪뻾: `resolve_type_node(...)` wrapper ?먯껜媛 metadata-first媛 ?섏뼱, ?⑥? explicit legacy allowlist??recursive materialization ?꾩뿉 DAG facts瑜?癒쇱? ?뚮퉬?쒕떎
    - 吏꾪뻾: `resolve_generic_type_arg(...)`??metadata-first 議고쉶 ??fallback?쇰줈 ?대젮媛꾨떎. constructed builtin/generic consumer path??recursive resolver ?섏〈 硫댁쟻??以꾩???    - 吏꾪뻾: owner-local resolver seams??`semantic_type_resolution_lookup_or_materialize(...)` 怨듭슜 materializer濡??섎졃?덈떎. resolver 援ы쁽泥댁? central metadata materializer 諛뽰뿉??吏곸젒 `resolve_type_node(...)`瑜??몄텧?섎㈃ `type-resolution-resolver-inventory-test-smoke`媛 ?ㅽ뙣?쒕떎
    - 吏꾪뻾: `type-resolution-dag-test-smoke`媛 graph-backed skips肉??꾨땲??metadata entries/owned/hits, metadata materializer fallback count, zero non-alias stage legacy fallback, alias-stage split accounting??寃?ы븳?? 理쒖떊 local stats: `graph-backed skips=3133 metadata_entries=1877 metadata_owned=111 metadata_hits=3267 materializer_fallbacks=4135 legacy_alias=83 legacy_non_alias=0 alias_materialized=5 alias_diagnostic_fallback=78 alias_fallback_resolved=0 alias_fallback_unresolved=78`
    - 吏꾪뻾: DAG smoke???댁젣 graph-backed skip/metadata entry/metadata hit/owned metadata媛 ?⑥닚??0蹂대떎 ?곗?留?蹂댁? ?딄퀬 beta floor(`skips>=3000`, `entries>=1500`, `hits>=2400`, `owned>=45`)瑜?寃?ы븳?? DAG source-of-truth ?ъ슜?됱씠 ?ш쾶 ?꾪눜?섎㈃ CI?먯꽌 利됱떆 ?〓뒗??    - 吏꾪뻾: 以묒븰 metadata materializer??留덉?留?recursive fallback??`materializer_fallbacks` ?듦퀎濡??몄텧?섍퀬 semantic suite ?⑹궛 cap??4135濡???톬?? ??cap? ?깆옣 諛⑹??⑹씠硫? ?ㅼ쓬 DAG ?묒뾽? ??媛믪쓣 怨꾩냽 ??텛??寃껋씠??    - 吏꾪뻾: ?⑥? stage legacy surface??alias-only濡?怨좎젙?먮떎. ?깃났 alias materialization怨?diagnostic fallback??蹂꾨룄 怨꾩륫?섍퀬 valid alias fallback? 0?쇰줈 gate?쒕떎. ?⑥? 78嫄댁? alias-cycle diagnostic coverage?먯꽌 ?섏삤??unresolved fallback?대ŉ hidden non-alias recursive resolution???꾨땲??    - 吏꾪뻾: program-level symbol inventory媛 ability declarations??predeclare?쒕떎. `type_check_ability_decl(...)`? ?먭린 ?먯떊??predeclare留??ъ궗?⑺븯怨?媛숈? ?대쫫???ㅻⅨ ability??湲곗〈泥섎읆 duplicate diagnostic?쇰줈 泥섎━?쒕떎. forward source order?먯꽌 generic default/where, zone authority, party role-slot ability consumer媛 provider ?꾪뻾?댁뼱???듦낵?섎뒗 regression??異붽??덈떎
    - 吏꾪뻾: `tests/cases/backend_compare/forward_ability_order/main.pgy`瑜?backend compare suite??異붽??덈떎. provider-after-consumer generic default/alias/zone-authority/party-role-slot ability ordering??semantic-only媛 ?꾨땲??C/LLVM 異쒕젰 ?숇벑?깃퉴吏 ?좎??섎뒗吏 寃?ы븳??    - 吏꾪뻾: `tests/compare_backends.sh` 湲곕낯 ?ㅽ뻾? `tests/cases/backend_compare/*/main.pgy`媛 default case array??鍮좎졇 ?덉쑝硫??ㅽ뙣?쒕떎. 紐낆떆 ?몄옄 湲곕컲 targeted run? ?좎??섎릺, CI/default path?먯꽌 ??parity case媛 議곗슜???꾨씫?섎뒗 drift瑜?李⑤떒?덈떎. ??gate濡?湲곗〈 passing case 8媛?array builtins/inline access, slice inline access, intent observability rollback, list/map/queue get-string, try-operator result)瑜?default C/LLVM parity suite???몄엯?덈떎
    - 吏꾪뻾: `type-resolution-resolver-inventory-test-smoke`媛 direct resolver allowlist? ?④퍡 metadata-first wrapper, execution/anchored-handle metadata materializer coverage瑜?static gate濡?怨좎젙?쒕떎
    - 吏꾪뻾: `type-resolution-resolver-inventory-test-smoke`媛 ??`semantic_type_resolution_resolve_or_fallback(...)` ?ъ슜?먮? 湲덉??섍퀬 named fallback seam 珥앸웾??0媛쒕줈 怨좎젙?쒕떎. gate 異쒕젰? ?꾩옱 fallback seam count瑜?吏곸젒 蹂댁뿬二쇰ŉ, remaining fallback? `semantic_type_resolution_lookup_or_materialize(...)` ?대???central escape hatch 援먯껜 ??곸씠??    - 吏꾪뻾: fallback seam gate??湲곗〈 ?섑븳??`30媛?誘몃쭔?대㈃ ?ㅽ뙣`)??debt-reduction??留욎? ?딅뒗 洹쒖튃?쇰줈 蹂닿퀬 ?쒓굅?덈떎. ?댁젣 0媛??곹븳留?growth guard濡??좎??섎ŉ, seam 異뺤냼??CI ?깃났 寃쎈줈??    - 吏꾪뻾: `type_checker_module_contract.c`??ability contract bookkeeping? recursive fallback helper瑜??몄텧?섏? ?딄퀬 DAG metadata lookup-only seam?쇰줈 ??톬?? ability 議댁옱/visibility/generic arity/where provenance??ability-specific validator媛 怨꾩냽 ?뚯쑀?섎ŉ, fallback seam inventory??39?먯꽌 38濡?媛먯냼?덈떎
    - 吏꾪뻾: `type_checker_ability_fields.c`??ability `fields` requirement validation??recursive fallback helper瑜??몄텧?섏? ?딄퀬 DAG metadata lookup-only濡???톬?? field contract diagnostics??ability-specific validator媛 怨꾩냽 ?뚯쑀?섎ŉ, fallback seam cap? 32?먯꽌 31濡?媛먯냼?덈떎
    - 吏꾪뻾: `type_checker_builtins_projection.c`??projection target-field resolver??recursive fallback helper瑜??몄텧?섏? ?딄퀬 DAG metadata lookup-only濡???톬?? projection field diagnostics??projection validator媛 怨꾩냽 ?뚯쑀?섎ŉ, fallback seam cap? 31?먯꽌 30?쇰줈 媛먯냼?덈떎
    - 吏꾪뻾: `type_checker_program.c`??quiet top-level placeholder resolver??graph precollect ?댄썑 metadata lookup-only濡??꾪솚?덈떎. event/function forward placeholders媛 recursive fallback ?놁씠 precollected DAG facts瑜??뚮퉬?섎㈃??fallback seam cap? 30?먯꽌 29濡?媛먯냼?덈떎
    - 吏꾪뻾: `type_checker_builtins_query_domain.inc`??projection source-field resolver??recursive fallback helper瑜??몄텧?섏? ?딄퀬 DAG metadata lookup-only濡???톬?? HasProjection/HasZoneProjection 怨꾩뿴 field diagnostics??domain query validator媛 怨꾩냽 ?뚯쑀?섎ŉ, fallback seam cap? 29?먯꽌 28濡?媛먯냼?덈떎
    - 吏꾪뻾: `type_checker_party_decl.c`? `type_checker_roster_decl.c`??shared-field type resolver??recursive fallback helper瑜??몄텧?섏? ?딄퀬 DAG metadata lookup-only濡???톬?? party/roster shared field diagnostics??媛?declaration validator媛 怨꾩냽 ?뚯쑀?섎ŉ, fallback seam cap? 28?먯꽌 26?쇰줈 媛먯냼?덈떎
    - 吏꾪뻾: `type_checker_ability_decl.c`??abstract method signature resolver? `type_checker_role_decl.c`??host-type resolver??recursive fallback helper瑜??몄텧?섏? ?딄퀬 DAG metadata lookup-only濡???톬?? ability/role declaration diagnostics??媛?owner validator媛 怨꾩냽 ?뚯쑀?섎ŉ, fallback seam cap? 26?먯꽌 24濡?媛먯냼?덈떎
    - 吏꾪뻾: function/action body precollector媛 local let / with-slot annotation肉??꾨땲??expression subtree, call type args, lambda param/return/body, event subscription handler, spawn/channel/return/branch expressions源뚯? ?곕씪媛꾨떎. ??湲곕컲?쇰줈 `type_checker_event.c`??event/lambda handler type-ref resolver瑜?DAG metadata lookup-only濡???톬怨?fallback seam cap? 24?먯꽌 23?쇰줈 媛먯냼?덈떎. `type_checker_flow.c`??flow-local type resolver??DAG metadata lookup-only濡???떠 cap? 22濡?媛먯냼?덈떎. `type_checker.c`??type-alias statement resolver??DAG metadata lookup-only濡???떠 cap? 21濡?媛먯냼?덈떎
    - ?뺤씤???⑥? blocker: `type_checker_program.inc`??function body param/return/domain-slot materialization seam? ?⑥닚 lookup-only濡???텛硫?direct semantic unit path?먯꽌 graph metadata bootstrap ?놁씠 segfault媛 ?쒕떎. ??seam? direct semantic unit bootstrap ?먮뒗 null-safe diagnostic path媛 癒쇱? ?꾩슂?섎떎
    - ?뺤씤???⑥? blocker: `type_checker_intent_decl.c`??intent participant/value/where resolver seam? ?⑥닚 lookup-only濡???텛硫?semantic suite ?꾨컲 parallel execution path?먯꽌 segfault媛 ?쒕떎. intent declaration? graph precollect媛 ?덉?留?direct semantic/bootstrap path? step/local binding materialization???꾩쭅 lookup-only 怨꾩빟??留뚯”?섏? ?딆쑝誘濡?explicit fallback seam?쇰줈 ?④릿??    - ?뺤씤???⑥? blocker: `type_checker_host_helpers.h`??host helper resolver???⑥닚 lookup-only濡???텛硫?intent/zone authority positive path媛 subject-slot type metadata 遺議깆쑝濡?臾대꼫吏꾨떎. ??seam? zone/world/host subject-slot nominal metadata瑜?DAG??蹂댁〈?????쒓굅?댁빞 ?쒕떎
    - ?뺤씤???⑥? blocker: `type_checker_generic_validation.c`??generic where/default validation resolver???⑥닚 lookup-only濡???텛硫?default type argument where-bound validation positive path媛 源⑥쭊?? ??seam? generic default effective-arg fact? where-bound provenance瑜?DAG metadata???щ┛ ???쒓굅?댁빞 ?쒕떎
    - ?뺤씤???⑥? blocker: `type_checker_generic_support.inc`??boundary type helper seam? ?⑥닚 lookup-only濡???텛硫?`ref class` / `ref subject` escape diagnostics 150媛쒓? 鍮좎쭊?? ??seam? generic/nominal boundary category fact? ref/own escape classifier媛 DAG metadata?먯꽌 媛숈? type category瑜?蹂????덉쓣 ???쒓굅?댁빞 ?쒕떎
    - ?뺤씤???⑥? blocker: `type_checker_ability_where.c`??ability where-bound resolver???⑥닚 lookup-only濡???텛硫?generic ability multi-bound mismatch provenance媛 ?щ씪??`Cloneable` bound mismatch 吏꾨떒 ?뚭?媛 ?쒕떎. ??seam? ability where-bound effective-arg / multi-bound provenance fact瑜?DAG metadata???щ┛ ???쒓굅?댁빞 ?쒕떎
    - ?뺤씤???⑥? blocker: `type_checker_operator_expr.inc`??operator overload method signature resolver???⑥닚 lookup-only濡???텛硫?semantic suite媛 event/misc path 吏꾩엯 ?꾪썑??segfault?????덈떎. ??seam? method param/return signature metadata? operator overload candidate summary瑜?DAG???щ┛ ???쒓굅?댁빞 ?쒕떎
    - ?뺤씤???⑥? blocker: `type_checker_zone_decl.c`??zone authority subject-slot type seam? ?⑥닚 lookup-only濡???텛硫?generic ability mismatch provenance媛 ?щ씪吏꾨떎. ??seam? zone authority generic ability fact瑜?DAG metadata???щ┛ ???쒓굅?댁빞 ?쒕떎
    - ?뺤씤???⑥? blocker: `type_checker_class_decl.c`??class/vessel field resolver???⑥닚 lookup-only濡???텛硫?vessel/subject-vessel field acceptance媛 源⑥쭊?? ??seam? class/vessel field nominal flavor metadata瑜?DAG??蹂댁〈?????쒓굅?댁빞 ?쒕떎
    - ?뺤씤???⑥? blocker: `type_checker_world_decl.c`??shared/domain-slot resolver???⑥닚 lookup-only濡???텛硫?zone/world/intent positive paths媛 `subject slot ... requires a subject type`濡?臾대꼫吏꾨떎. ??seam? world domain-slot subject/zone nominal materialization??DAG metadata???щ┛ ???쒓굅?댁빞 ?쒕떎
    - ?뺤씤???⑥? blocker: `type_checker_ownership_let.c`??let annotation resolver???⑥닚 lookup-only濡???텛硫?direct semantic unit path?먯꽌 graph metadata ?놁씠 `ClaimSlot` annotation???ㅼ뼱? segfault?????덇퀬, broader program path?먯꽌??`Slot`/`ReadView`/`WriteView`/`QubitSlot`/anchored own-ref paths媛 `<unknown>`?쇰줈 臾대꼫吏????덈떎. ??seam? direct semantic unit bootstrap ?먮뒗 null-safe diagnostic path? anchored-handle constructed-type metadata coverage瑜?媛숈씠 ?レ? ???쒓굅?댁빞 ?쒕떎
    - 吏꾪뻾: domain/intent declaration resolver??owner-local type-ref seam?쇰줈 ?섎졃?덈떎. slot/shared/named domain refs? intent involves/value/where refs媛 媛곴컖 ?섎굹??owner seam??怨듭쑀?섎㈃??fallback seam inventory??38?먯꽌 34濡?媛먯냼?덈떎
    - 吏꾪뻾: alias/generic-parameter helper? resolution-stage diagnostic fallback??owner-local seam?쇰줈 ?섎졃?덈떎. fallback seam inventory??34?먯꽌 32濡?媛먯냼?덈떎
    - 吏꾪뻾: zone authority participant resolver媛 exact/qualified-tail direct slot match瑜?癒쇱? ?몄젙?섍퀬, direct match 諛섑솚 ??stale ambiguity flag瑜?吏?대떎. 媛숈? ???subject slot???щ읉 ?덉뼱??`authorized by rogue`媛 ?ㅼ젣 `subject slot rogue: Adventurer`濡?concrete?섍쾶 ?ロ엳硫?false-positive ambiguous濡??⑥뼱吏吏 ?딅뒗??    - 吏꾪뻾: `type_checker_intent_decl.c`??participant/value/where local seam 3媛쒕뒗 graph metadata-first 議고쉶 ??recursive fallback?쇰줈 ?대젮媛꾨떎
    - 吏꾪뻾: `type_checker_decls_domain_helpers.c`??slot/shared/named-ref local seam 3媛쒕뒗 graph metadata-first 議고쉶 ??recursive fallback?쇰줈 ?대젮媛꾨떎
    - 吏꾪뻾: `type_checker_intent_helpers.c`??direct resolver ?몄텧? `intent_helper_resolve_type_ref(...)` ?⑥씪 seam?쇰줈 ?섎졃?덈떎. transfer-derived using/where, ability generic arg, role-field checks????seam???듯빐 ?ㅼ쓬 DAG metadata ?꾪솚???꾨떎
    - 吏꾪뻾: `type_checker_host_helpers.h`??direct resolver ?몄텧? `host_helper_resolve_type_ref(...)` ?⑥씪 seam?쇰줈 ?섎졃?덈떎. projection source fields, hosted method return/param, zone authority/domain slot checks????seam???듯빐 ?ㅼ쓬 DAG metadata ?꾪솚???꾨떎
    - 吏꾪뻾: `type_checker_program.c`??forward-declaration type materialization? quiet resolver seam 1媛쒕줈 ?섎졃?덇퀬, `type_checker_program.inc`??function-body param/return/domain-slot materialization body resolver seam? graph metadata-first 議고쉶 ??fallback?쇰줈 ?대젮媛꾨떎
    - 吏꾪뻾: `type_checker_event.c`??event signature/lambda handler materialization? graph-backed metadata lookup-only濡??꾪솚?먮떎. ?ㅼ쓬 DAG slice??ownership let / zone authority / world domain-slot / ability where-bound泥섎읆 semantic provenance媛 ?⑥? owner seams??    - 吏꾪뻾: `type_checker_world_decl.c`??shared field/domain slot materialization? `world_resolve_type_ref(...)` / `world_resolve_domain_slot_type(...)` seam?쇰줈 ?섎졃?덈떎. world shared/slot checks????seam?먯꽌 graph-backed metadata濡?援먯껜?????덈떎
    - 吏꾪뻾: `type_checker_role_decl.c`, `type_checker_generic_contracts.h`, `type_checker_helpers_late.c`, `type_checker_expr.inc`??吏곸젒 resolver ?몄텧??媛곴컖 role/generic-contract/late-helper/expr local seam 1媛쒕줈 ?섎졃?덈떎
    - 吏꾪뻾: `type_checker_generic_validation.c`, `type_checker_ability_where.c`, `type_checker_module_contract.c`, `type_checker_ability_decl.c`, `type_checker_class_decl.c`, `type_checker_operator_expr.inc`, `type_checker_ownership_destructure_stmt.inc`??local resolver seam?쇰줈 ?섎졃?덈떎. ?⑥? direct count???遺遺?resolver 蹂몄껜, 二쇱꽍, ?먮뒗 紐낆떆 seam?대떎
    - 吏꾪뻾: `type_checker.c`, `type_checker_ability_fields.c`, `type_checker_builtins_projection.c`, `type_checker_builtins_query_domain.inc`, `type_checker_flow.c`, `type_checker_generic_support.inc`, `type_checker_helpers_effects.inc`, `type_checker_ownership_let*.inc`, `type_checker_party_decl.c`, `type_checker_roster_decl.c`, `type_checker_zone_decl.c`???⑤컻 direct resolver ?몄텧??local seam?쇰줈 ?섎졃?덇퀬, zone domain-slot seam? graph metadata-first 議고쉶瑜??ъ슜?쒕떎
    - ?꾨즺: `make type-resolution-resolver-inventory-test-smoke`瑜?異붽?????`resolve_type_node(...)` 吏곸젒 ?몄텧??resolver 蹂몄껜/stage legacy fallback/core fallback/local seam allowlist 諛뽰뿉 ?앷린硫??ㅽ뙣?섎룄濡?怨좎젙?덈떎. `ci-linux`?먮룄 ?곌껐?덈떎
    - 寃利? 2026-04-25 local WSL/Linux `make ci-linux` full green. Windows/MSYS2 native runner????癒몄떊???놁쑝誘濡?蹂꾨룄 CI ?섍꼍 acceptance line?쇰줈 ?좎?
  - 紐⑺몴:
    - import graph? 蹂꾧컻濡?`type provider -> type consumer` 洹몃옒?꾨? 遺꾨━ 援ъ텞?쒕떎
    - declaration / alias / generic default / where-bound / ability consumer / zone authority consumer瑜?DAG node/edge濡??밴꺽?쒕떎
    - namespace-only reference??declaration inventory 議고쉶媛 遺덊븘?뷀븳 concrete type materialization??媛뺤젣?섏? ?딄쾶 ?쒕떎
    - cycle??generic/alias/type consumer path 湲곗??쇰줈 path-aware diagnostic?쇰줈 蹂닿퀬?쒕떎
    - incremental compile ??invalidation 踰붿쐞瑜?declaration/type dependency ?⑥쐞濡?以꾩씤??  - 1李?援ы쁽 ?먯튃:
    - 湲곗〈 `resolve_type_node(...)`瑜???踰덉뿉 ?먭린?섏? ?딅뒗??    - 癒쇱? graph inventory + topo scheduling + cycle diagnostic??異붽??섍퀬, 洹??ㅼ쓬 recursive resolver瑜?graph-backed evaluator濡?移섑솚?쒕떎
    - import/module loader??DFS cycle detection怨?type-resolution DAG瑜??쇳빀?섏? ?딅뒗??  - ?④퀎:
    - Phase A: declaration/type provider inventory? consumer edge ?섏쭛
    - Phase B: topo evaluation + SCC/cycle diagnostic 怨좎젙
    - Phase C: generic default arg / multi-bound / ability consumer / zone authority瑜?DAG consumer濡??몄엯
    - Phase D: incremental invalidation / cache / backend-facing resolved metadata ?ъ궗??  - ?뚭? 湲곗?:
    - dependency loop diagnostic??cycle path/provenance媛 ?섏삩??    - graph-backed cycle怨?alias fallback cycle 紐⑤몢 `Contract source:`瑜??ы븿?쒕떎
    - namespace-only reference??遺덊븘?뷀븳 full type materialization???좊컻?섏? ?딅뒗??    - generic consumer/default/bound resolution??graph-backed evaluation?먯꽌??湲곗〈 semantic 怨꾩빟怨?媛숈? 寃곌낵瑜??몃떎
    - C/LLVM compile path媛 ?숈씪??resolved-type metadata瑜??ъ궗?⑺븳??    - `PGY_TYPE_RES_STATS=1`?먯꽌 stage graph-backed skip ?? legacy fallback ?몄텧?? family breakdown, suppressed diagnostic ?섍? 蹂댁씤?? ??媛믪? ?⑥? DAG migration debt??吏곸젒 吏?쒖씠硫??④꺼吏?fallback??異붽??섎㈃ smoke?먯꽌 利됱떆 ?쒕윭?섏빞 ?쒕떎

- [x] **runtime observability baseline vs richer query 援щ텇 怨좎젙**
  - ??? `IntentLast* / IntentHistory* / IntentActive* / IntentRecent*`, zone/world inspection
  - 臾몄젣: baseline???대? ?덈뒗??臾몄꽌媛 thin?대씪怨??곕㈃ 諛섎?濡?surface trust瑜?源롮쓬
  - 怨좎젙 湲곗?:
    - baseline observability??complete濡? richer timeline/provenance??open debt濡?遺꾨━
  - ?뚭? 湲곗?:
    - docs/board/status 臾멸뎄 ?쇱튂
    - observability regression??baseline API瑜?怨꾩냽 怨좎젙

## ?꾨즺 (P0 ??利됱떆 ?섏젙)

- [x] **`system()` 紐낅졊 二쇱엯 ?쒓굅** ??`_spawnvp`/`execvp`濡?援먯껜, 寃쎈줈 寃利?異붽? (`pgy_path_is_safe`)
- [x] **AES-256 ?ㅺ뎄??* ??XOR 媛吏??뷀샇瑜?FIPS 197 AES-256-CTR + HMAC-SHA256 ?몄쬆?쇰줈 援먯껜 (?몃? ?섏〈???놁쓬)
- [x] **`auto __tmp` ?쒓굅** ??`PGY_RESULT_TRY` 留ㅽ겕濡쒖뿉??GCC ?뺤옣 `auto` ?쒓굅, C11 ?명솚 (紐낆떆??????뚮씪誘명꽣)
- [x] **REPL 怨좎젙 ?뚯씪紐?* ??`_pgy_repl_tmp.*` ??`TMPDIR/pgy_repl_{pid}.*` (PID 湲곕컲 ?좊땲??寃쎈줈)
- [x] **`type alias` vertical slice** ??`type UserId = Int;` parser/semantic/C/LLVM lowering ?곌껐, ?ㅼ쟾 annotation/typedef 寃쎈줈 ?뺣낫

## P1 ???ㅼ쓬 ?④퀎

- [ ] **CI ?섎뱶??* ??Ubuntu + Windows 鍮뚮뱶 留ㅽ듃由?뒪 ?좎?, AddressSanitizer/UBSan, ??珥섏킌??smoke coverage
- [ ] **CodeQL + secret scanning ?쒖꽦??* ??C/C++ 遺꾩꽍 紐⑤뱶, push protection
- [x] **CHANGELOG.md + 踰꾩쟾 ?뺤콉 ?섎┰** ??SemVer, 由대━???쒓퉭 洹쒖튃
  - ?꾨즺: `CHANGELOG.md` 議댁옱, Keep a Changelog ?щ㎎, SemVer 紐낆떆
- [x] **SECURITY.md** ??蹂댁븞 痍⑥빟???쒕낫 梨꾨꼸, 梨낆엫 ?덈뒗 怨듦컻 ?뺤콉
  - ?꾨즺: `SECURITY.md` ?앹꽦 (2026-04-18). 吏??踰꾩쟾, 蹂닿퀬 梨꾨꼸, in/out scope, 怨듦꺽 ?쒕㈃蹂?mitigation, advisory format ?ы븿

## P1.5 ???몄뼱/而댄뙆?쇰윭 蹂닿컯

- [ ] **MIR DCE statement-level ?뺤옣**
  - ?꾩옱??dead SSA/PHI ?쒓굅 + `HasState`/`ChannelLength`瑜?pure-query stmt ?쒓굅源뚯????숈옉??  - ?⑥? ?④퀎: pure expression stmt / dead call / dead resource-op / carrier stmt瑜????몃텇?뷀븯怨? side-effect lattice 湲곗??쇰줈 ?쒓굅 ?뺤콉???뺢탳??  - 紐⑺몴: MIR-only emitter媛 湲곕??섎뒗 metadata carrier瑜??껋? ?딆쑝硫댁꽌??遺덊븘?뷀븳 stmt ?쒓굅 踰붿쐞瑜??볧옒

- [x] **IR 怨꾩링 ?ㅺ퀎 寃??* ??HIR/DIR/RIR/MIR 遺꾨━ ??뱀꽦 ?됯?
  - **DIR ?좎? 寃곗젙**: intent domain structure 寃利앹뿉 ?꾩닔 (step dependency, zone binding, post-condition)
  - **RIR ?좎? 寃곗젙**: resource state lattice (20-state)??slot/projection/authority lifecycle 寃利앹뿉 ?꾩슂
  - **MIR ?좎? 寃곗젙**: SSA/CFG/cleanup edge??intent compensation execution path???꾩닔
  - ~~?⑥? 怨쇱젣~~: Backend瑜?HIR 湲곕컲 ??MIR 湲곕컲?쇰줈 ?꾪솚?댁빞 IR ?ъ옄 ROI ?ㅽ쁽 ??**?꾨즺**
  - 李멸퀬: Rust??AST?뭈HIR?묺IR?묹LVM 4?④퀎, Pergyra??AST?묱IR?묭IR?뭃IR?묺IR?묪ackend 6?④퀎
  - DIR? domain graph濡?HIR? 援ъ“媛 ?щ씪 蹂꾨룄 IR濡??좎??섎뒗 寃껋씠 ???  - RIR 20-state lattice???⑥닚??媛?μ꽦 寃??(?꾩옱: Owned/Borrowed/Synced/Dirty/Stale/Published/Authorized ??
- [ ] **ability 湲곕컲 ?곗궛??dispatch 怨좊룄??* ???꾩옱??`role/impl ability` 硫붿꽌?쒖뿉??`operator_<suffix>_<Type>` alias瑜??⑹꽦??C/LLVM???뺤쟻?쇰줈 ?몄텧?섎뒗 諛⑹떇. ?κ린?곸쑝濡쒕뒗 ability/vtable 湲곕컲??吏곸젒 dispatch? ???뺢탳??overload ?곗꽑?쒖쐞 洹쒖튃???꾩슂
- [ ] **LLVM ?곗궛???ㅻ쾭濡쒕뱶 ?뚭? ?뚯뒪???뺤옣** ???꾩옱 ?ㅻえ?щ뒗 `role IntMath for Int` 1嫄?以묒떖. 鍮꾧탳 ?곗궛, ?ы븿??role, enum/custom type, namespace 寃쎈줈源뚯? ?먮룞 ?뚯뒪???뺣?

## P1.58 ???쒖? ?쇱씠釉뚮윭由??명봽??
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
- [x] **?쇳븨紐??덉젣瑜?stdlib ?명봽???ъ슜 踰꾩쟾?쇰줈 由ы봽??*
  - `pages/` -> `use page;`
  - `api/` -> `use http;`
  - `report/storage` -> `use storage;`

- [ ] **`pgy scaffold project`??app-infra starter 異붽?**
  - intent-first layout + `intents/ subjects/ zones/ world.pgy main.pgy`
  - optional `pages/ api/ report/` app adapter starter

## P1.58 ???쒖? ?쇱씠釉뚮윭由?媛쒖꽑 (2026-04-06 遺꾩꽍)

- [ ] **stdlib page.pgy ?ㅼ젣 ?뚮뜑留?而댄룷?뚰듃 ?쒖뒪?쒖쑝濡??뺤옣**
  - ?꾩옱: ?⑥닚 ?곗씠??援ъ“ + ?뚮뜑留?臾몄옄???⑥닔留?  - 紐⑺몴: ?섏씠吏 ?쇱씠?꾩궗?댄겢(留덉슫???몃쭏?댄듃/?낅뜲?댄듃), 而댄룷?뚰듃 ?몃━, ?곹깭 愿由?  - ?쒖븞: `Component` abstract base, `mount()`, `render()`, `update()`, `unmount()` ?쇱씠?꾩궗?댄겢 ??- [ ] **stdlib storage.pgy WriteFile 異붿긽??*
  - ?꾩옱: `WriteFile` ?댁옣 ?⑥닔 吏곸젒 ?몄텧 ???뚮옯???섏〈??  - 紐⑺몴: Slot/Device ?명꽣?섏씠?ㅻ줈 遺꾨━ (`StorageDevice` ability)
  - ?쒖븞: `ability StorageDevice { Write(path, data) -> Result<Void, Error>; Read(path) -> Result<String, Error> }`
- [ ] **stdlib ?꾨컲 Result<T, Error> ?⑦꽩 ?쒖슜**
  - ?꾩옱: `WriteFile`, `ReadFile` ?ㅽ뙣 ???щ옒??媛?μ꽦
  - 紐⑺몴: 紐⑤뱺 I/O ?곗궛??`Result<T, Error>` 諛섑솚
  - ?쒖븞: `?` ?곗궛?먯? 議고빀???먮윭 ?꾪뙆 ?먮룞??- [ ] **datetime.pgy 硫붿꽌???쇨???媛쒖꽑**
  - ?꾩옱: `export class LocalDate` + `export func SameDate()` ?쇱옱
  - ?쒖븞: 硫붿꽌???쇨???(`a.SameDate(b)` vs `SameDate(a, b)`) ???섎굹留??④린嫄곕굹 ????臾몄꽌??
## IR ?뚯씠?꾨씪??
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
- [ ] **RIR lattice propagation ?ы솕**
  - relation/effect/zone/world handle merge???쒖옉?? conditional handle invalidation怨?world-handoff lattice瑜???諛湲?  - conditional authority/projection invalidation fact ?뺤옣
- [ ] **MIR full SSA / flow merge**
  - block-level version map? ?쒖옉?? rename??full def-use chain/liveness ?섏??쇰줈 ?뺤옣
  - cleanup convergence root???쒖옉?? MIR-level `RIR-flow` merge? cleanup convergence policy瑜???怨좊룄??- [ ] **MIR DCE ?뺤옣 (statement-level)**
  - dead DEF/PHI ?쒓굅瑜??섏뼱 side-effect-free STMT/unused call ?쒓굅
  - ?꾩옱??pure query builtin (`Has*`, `ChannelLength/Capacity/Space/Full/Closed`)留??덉쟾 ?쒓굅 ?쒖옉
  - `unused pure let initializer` ?쒓굅??source-local/runtime-backed storage? 異⑸룎???ㅼ떆 蹂대쪟
  - dead identifier-assign ?쒓굅??loop/phi/live-out ?ㅽ뙋???⑥븘 ?덉뼱 怨꾩냽 蹂댁닔 蹂대쪟
  - ?ㅼ쓬 reopen 議곌굔: value summary??block-boundary / phi provenance瑜??댁슜??loop-carried DEF? 吏꾩쭨 dead local DEF瑜?遺꾨━
  - user call purity???꾩쭅 蹂댁닔?곸쑝濡?side-effect ?덈떎怨?媛꾩＜
  - RESOURCE_OP/CLEANUP_EDGE/abort/IO ??side-effect 蹂댁〈 洹쒖튃 紐낆떆
  - RPO 湲곕컲 liveness? 寃고빀???쒓굅 ?뺥솗??媛쒖꽑
## P2.0 ??Backend MIR 湲곕컲 ?꾪솚 ???꾨즺

- [x] **emit_program()??HIR 湲곕컲 ??MIR 湲곕컲?쇰줈 ?꾪솚**
  - **?꾨즺**: `emit_func_decl_from_mir_named()` ?꾩쟾 援ы쁽
  - **寃곌낵**: MIR routine ??SSA locals + CFG ??C 肄붾뱶 ?앹꽦
  - **吏??湲곕뒫**:
    - Intent compensation (cleanup blocks)
    - SSA versioned locals (`_pgy_ssa_name_N`)
    - PHI ?몃뱶 蹂듭궗 (join block 吏꾩엯)
    - BRANCH ??if/else gotos
    - RESOURCE_OP ???고????⑥닔 ?몄텧
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

## P2.1 ??LLVM 諛깆뿏??MIR 湲곕컲 ?꾪솚 ???꾨즺

- [x] **LLVM 諛깆뿏??MIR 湲곕컲 ?꾪솚 ?꾨즺**
  - `src/codegen/llvm_pipeline.c`: MIR routine ??LLVM IR 吏곸젒 ?앹꽦
  - `src/codegen/llvm_mir_emit.c`: `llvm_emit_func_from_mir()` ?꾩쟾 援ы쁽
  - SSA locals, PHI nodes, branch terminators, intent compensation 紐⑤몢 吏??  - 湲곕? ?④낵 ?ъ꽦: LLVM 理쒖쟻???⑥뒪 ?꾩쟾 ?쒖슜, C/LLVM 諛깆뿏???꾪궎?띿쿂 ?듭씪
  - C/LLVM ????MIR 湲곕컲?쇰줈 ?듭씪 ??IR ?ъ옄 ROI ?ㅽ쁽

## P1.55 ???몄뼱 湲곕뒫 ?뺤옣

### 湲곕컲 ????쒖뒪??- [x] **?쒓렇???좊땲??(enum with data)** ??`enum Shape { Circle(Int), Rect(Int, Int) }` ?곗씠?곕? 媛吏?enum
  - ?꾨즺: variant payload ?뚯떛, variant ?앹꽦?????異붾줎, C tagged union / LLVM discriminated struct, LLVM tagged-union regression 諛??덉젣 ?ㅽ뻾
- [x] **Option<T> / None** ??"?곸옄媛 鍮꾩뼱?덉쓣 ???덈떎"瑜???낆쑝濡??쒗쁽. `-1` sentinel ?쒓굅
  - ?꾨즺: `Option<T>` constructed type, `Some/None`, `IsSome/IsNone/UnwrapOption`, C/LLVM lowering
  - ?꾨즺: `match opt { case Some(v): ... case None: ... }` destructuring
- [x] **?붿뒪?몃윮泥섎쭅 (SecureSlot)** ??`let (slot, token) = ClaimSecureSlot<Int>(lvl)` ?⑦꽩 諛붿씤??  - ?꾨즺 (2026-04-19): ?뚯꽌 `ClaimSlot`/`ClaimSecureSlot` ?ㅼ쓽 `<T>`瑜????댁긽 踰꾨━吏 ?딄퀬 `AST_CALL.generic_args`??泥⑤? (?쇰컲 call-site ?쒕꽕由??명봽??, ?쒕㎤?깆씠 destructuring?먯꽌 ??generic arg濡?SYMBOL_SLOT + SYMBOL_TOKEN ???깅줉, MIR emit??`PgyToken_T token; PgySecureSlot_T slot = pgy_claim_secure_T(&token);` 異쒕젰, `transpiler_find_local_type_name_in_block`??諛붿씤?⑸퀎 `SecureSlot<T>`/`Token<T>` 諛섑솚??MIR header??????덉빟 ?뺣━, SSA 留듭뿉 self-mapping ?깅줉?쇰줈 emission contract ?듦낵
  - ?뚯씪: `src/parser/ast.h`, `src/parser/ast.c`, `src/parser/parser.h`, `src/parser/parser_expr.c` (?쒕꽕由??몄옄 蹂댁〈), `src/semantic/type_checker.c` (destructuring ?쒕㎤??, `src/codegen/transpiler_emitters_base_a.inc` (MIR-level claim emit + ssa map ?깅줉)
  - ?뚭?: `src/test_transpile.c` "let (slot, token) = ClaimSecureSlot<T>(lvl) emits paired claim"
  - SecureSlot MIR auto-Read + claim ?좏겙 emit ?곌? 踰꾧렇 ?섏젙 (2026-04-19): (a) SSA-aware identifier 寃쎈줈媛 `suppress_slot_auto_read` 臾댁떆?섎뜕 踰꾧렇濡?`pgy_secure_write_Int(&pgy_read_Int(&slot),...)` 媛숈? ?섎せ??C 異쒕젰 ??`!ctx->suppress_slot_auto_read` 媛??異붽? + Secure 寃쎈줈?먯꽌 `pgy_secure_read_*` 遺꾧린. (b) MIR DCE媛 `AST_LET_DECL`??遺?묒슜 ?놁쓬?쇰줈 ?먯젙???쒓굅?섎뜕 踰꾧렇 ??`mir_stmt_has_side_effect`??異붽?. (c) `transpiler_emit_mir_resource_op` Claim 猷곗씠 SecureSlot?먮룄 `pgy_claim_secure_T()`留?emit?섍퀬 ?좏겙? ?앸왂?섎뜕 踰꾧렇 ??`PgyToken_T anchor_token;` + `= pgy_claim_secure_T(&anchor_token)` 諛⑹떇?쇰줈 ?섏젙. (d) `Token<T>`??"claim shape"濡??몄떇??MIR header pre-decl 嫄대꼫?곕룄濡?`transpiler_type_name_is_claim_shape` ?꾩엯 (slot-like???援щ퀎 ??auto-Read???ъ쟾??Slot ?꾩슜). 寃곌낵: destructuring + 鍮?destructuring SecureSlot 紐⑤몢 E2E ?숈옉 (`Write/Read/Release` ?ы븿)
  - ?뚯씪: `src/compiler/mir.c` (DCE), `src/codegen/transpiler_expr_emitters.inc` (suppress 媛??, `src/codegen/transpiler_emitters_base_a.inc` (claim_shape 遺꾨━), `src/codegen/transpiler_emitters_base_b.inc` (MIR header 泥댄겕), `src/codegen/transpiler_helpers.h` (claim ?좏겙 emit), `src/parser/parser_decl.c` (class-body destructuring ?먮윭 硫붿떆吏)
  - 誘몄쿂由? LLVM 諛깆뿏??SecureSlot destructuring (LLVM? ?대? "requires explicit annotation" ?먮윭 ??蹂꾨룄 ?몄뀡), class-body destructuring (`private let (slot, token) = ClaimSecureSlot()`??紐낇솗???먮윭 硫붿떆吏濡쒕쭔 泥섎━ ??蹂꾨룄 ?몄뀡)
- [x] **?쒗뵆 諛섑솚 ???+ ?붿뒪?몃윮泥섎쭅** ??`func f() -> (Int, String)` 諛?`let (n, s) = f()` 吏??  - ?꾨즺 (2026-04-19): Type ?명봽?쇱뿉 `TYPE_KIND_TUPLE` ?쒖꽦??(union??`tuple.elements/element_count` ?꾨뱶 + `type_create_tuple`/`type_is_tuple`/`type_tuple_arity`/`type_tuple_get_element`), AST_TYPE??`tuple_elements` ?꾨뱶濡?`(T, U, ...)` ?쒗쁽, `AST_TUPLE_LITERAL` ?좉퇋 ?몃뱶濡?`(a, b, ...)` ?쒗쁽??吏??  - ?뚯꽌: `parse_type()`??`LPAREN` 遺꾧린濡??쒗뵆 ???援щЦ 泥섎━ (?⑥씪 `(T)`??湲곗〈 `T`濡??섏썝, 鍮?`()`??`Void`, 2媛??댁긽???뚮쭔 ?쒗뵆), `parser_parse_primary`??愿꾪샇 ?쒗쁽??寃쎈줈??肄ㅻ쭏 媛먯? ???쒗뵆 由ы꽣?대줈 遺꾧린
  - ?쒕㎤?? `resolve_type_node`??tuple 遺꾧린 異붽? ??`type_create_tuple` 諛섑솚, `type_check_expression`??`AST_TUPLE_LITERAL` 耳?댁뒪濡??붿냼 ????섏쭛, `AST_LET_DESTRUCTURE`?먯꽌 RHS媛 tuple?대㈃ arity 寃利?+ positional element ????좊떦
  - C 諛깆뿏?? `append_type_name`???쒗뵆??`(T, U)`濡??뚮뜑, `pergyra_type_to_c`媛 `(Int, String)` ??`PgyTuple_Int_String_t`濡?留ㅽ븨 (depth-tracking ?뚯꽌), `ensure_tuple_specialization_to`媛 `typedef struct { T0 f0; T1 f1; ... } PgyTuple_<suffix>_t;`瑜?ctx->out??以묐났 ?놁씠 諛⑹텧, `emit_expression(AST_TUPLE_LITERAL)`??compound literal `((PgyTuple_T_U_t){.f0=..., .f1=...})` emit, AST_LET_DESTRUCTURE MIR 寃쎈줈/湲곕낯 寃쎈줈 ????tuple 遺꾧린濡?`.f0/.f1/...` ?꾨뱶 異붿텧
  - LLVM 諛깆뿏?? `ast_type_to_llvm`??tuple AST_TYPE ??literal anonymous struct `{T0, T1, ...}`, `llvm_emit_expression(AST_TUPLE_LITERAL)`??`LLVMGetUndef + InsertValue` 泥댁씤?쇰줈 吏묎퀎媛?援ъ꽦, `llvm_emit_let_destructure`媛 struct ?꾨뱶 媛쒖닔 + 泥??꾨뱶 鍮꾪룷?명꽣 heuristic?쇰줈 tuple ?먯젙 ??`ExtractValue` per-binding
  - ?뚭?: `tests/cases/backend_compare/destructure_tuple_return/main.pgy` (C/LLVM ?숈씪: `42/hello/7/11/true`), `compare_backends.sh` case ?깅줉, `test-semantic 1653 passed`, `test-transpile 584 passed`
  - ?뚯씪: `src/semantic/type_system.{h,c}`, `src/parser/ast.{h,c}`, `src/parser/parser_decl.c`, `src/parser/parser_expr.c`, `src/semantic/type_checker.{c,_helpers.inc}`, `src/codegen/transpiler.h`, `src/codegen/transpiler_helpers_core_b.inc`, `src/codegen/transpiler_expr_emitters.inc`, `src/codegen/transpiler_emitters_base_{a,b}.inc`, `src/codegen/llvm_backend.c`, `src/codegen/llvm_expr.c`, `src/codegen/llvm_stmt.c`, `src/codegen/llvm_pipeline.c`
  - ?꾩냽 ?섏젙 (destructure + if 吏??: `transpiler_register_with_alias_bindings_in_block`??Claim-only ?쒗븳 ?쒓굅 ??紐⑤뱺 destructuring 諛붿씤??array/slice/tuple/?쇰컲 call)???대쫫??self-mapping?쇰줈 precheck ssa_map???깅줉. ?ㅼ젣 emit 寃쎈줈???ъ쟾??`<name>.1` 踰꾩쟾???대쫫??MIR emit ?쒖젏??ssa_map???ｌ뼱???ъ슜 (self-map? verifier ?듦낵??媛?쒖씪 肉?. 寃곌낵: `let (a, b, flag) = f(); if flag { ... } else { ... }` 媛숈? ?⑦꽩??array/tuple ????C/LLVM?먯꽌 ?숈옉. ?뚯씪: `src/codegen/transpiler_emitters_base_a.inc` (register_with_alias_bindings_in_block)
- [ ] **sealed ability** ??援ы쁽 媛?ν븳 role???쒗븳 (`sealed ability Combatable` ??媛숈? 紐⑤뱢 ??role留?impl 媛??
- [x] **臾몄옄??蹂닿컙** ??`f"媛믪? {x}"` ??`StringConcat(...)` series濡?lowering
  - ?꾨즺: lexer?먯꽌 `f"..."` ??`TOKEN_INTERPOLATED_STRING`
  - ?꾨즺: parser?먯꽌 `{expr}` ?뚯떛, `ToString(expr)` + `+` concatenation?쇰줈 遺꾪빐
  - ?꾨즺: 湲곗〈 `"${expr}"` ?덇굅??臾몃쾿???명솚 ?좎?
  - ?꾨즺: 踰좏? stable subset??`"..."`, `"""..."""`, `"${expr}"`, `f"{expr}"`, escaped f-string brace濡?臾몄꽌??  - ?꾨즺: unmatched interpolation brace??蹂닿컙?섏? ?딄퀬 literal text濡?蹂댁〈?섎룄濡?parser ?뚭? 異붽?
  - beta-out-of-scope: nested brace matching, format specifier, multiline interpolation, custom interpolation protocol

### ?먮윭 泥섎━
- [x] **`?` ?곗궛??* ??`Result<T>` ?먮윭 ?먮룞 ?꾪뙆. `let val = riskyFunc()?;` ???먮윭 ??利됱떆 諛섑솚
  - ?꾨즺: ?쒕㎤??寃利? C early-return lowering, LLVM `Result<T>` ?덉씠?꾩썐/unwrap/early-return lowering, `pipe_and_try.pgy` C/LLVM ?ㅽ뻾 寃利?  - LLVM try.err ?ш뎄??踰꾧렇 ?섏젙 (2026-04-19): `let val = Validate(x)?;` ?⑦꽩?먯꽌 let_decl??`current_ret_type`??LHS var ???i32)?쇰줈 ?좎떆 ??뼱?곌퀬 ?덉뼱, `?`??try.err 釉붾줉???⑥닔 return ???struct ???i32濡??먯젙 ??`unreachable` emit ???고???crash. `ctx->current_func_decl`?먯꽌 AST 諛섑솚 ??낆쓣 ?ъ“?뚰빐 蹂듦뎄 + Err 媛??ш뎄??(src_err ??dst_err ?뺤닔/?ъ씤??媛뺤젣 蹂???ы븿)
  - ?뚯씪: `src/codegen/llvm_expr_scalar_core.h`
  - ?뚭?: `tests/cases/backend_compare/try_operator_result/main.pgy` (C/LLVM ?숈씪), `examples/pipe_and_try.pgy`

### ?몄쓽 臾몃쾿
- [x] **?뚯씠???곗궛??* ??`data |> Transform |> Validate |> Persist` ?⑤갑???곗씠???먮쫫
- [x] **defer** ??`defer Release(s)` ?ㅼ퐫??醫낅즺 ???먮룞 ?ㅽ뻾
- [x] **`let` ???異붾줎** ??initializer 湲곕컲 湲곕낯 異붾줎? ?꾩옱 援ы쁽??  - ?꾨즺: annotation???놁쓣 ??initializer ??낆쑝濡?異붾줎
  - ?⑥쓬: 臾몄꽌/?쒕㈃ ?덉떆瑜???怨듦꺽?곸쑝濡????異붾줎 以묒떖?쇰줈 ?뺣━?좎? 寃곗젙

### ?쒕꽕由??대옒??- [x] **?쒕꽕由??대옒??* ??`class Pair<T>` 臾몃쾿 + ?쒕㎤??+ C 肄붾뱶??(?⑦삎??. ?덉젣: `examples/generic_class.pgy`

### Slot ?뚯쑀沅?紐⑤뜽
- [x] **`own`/`ref` ?뚯쑀沅?紐⑤뜽 ?뺤젙 諛?援ы쁽** ??move 湲곕낯, ?⑥닔 ?쒓렇?덉쿂??紐낆떆
  - ?꾨즺: `own`/`ref` ?ㅼ썙??(?됱꽌/?뚯꽌/AST), Slot ?????move ?쒕㎤?? Clone() 紐낆떆??蹂듭궗
  - `func Upload(own tex: Slot<Texture>)` ???뚯쑀沅??댁쟾, ?먮낯 臾댄슚
  - `func Render(ref tex: Slot<Texture>)` ??鍮뚮┝, ?먮낯 ?좏슚
  - 臾몄꽌?? `docs/22_ownership_model.md`

### Slot ?쒕㈃ 臾몃쾿 媛쒖꽑 (P0 ?곗꽑?쒖쐞)
- [x] **?붾У??Read + ???湲곕컲 Write** ??Slot??湲곕낯 ?ъ슜 ?쒕㈃???쇰컲 蹂?섏쿂??  - ?꾨즺: ?쎄린 臾몃㎘?먯꽌 `Slot<T>` auto-read
  - ?꾨즺: `slot = expr` ??`Write(slot, expr)` lowering
  - ?좎?: `Release(slot)`??怨꾩냽 紐낆떆??
### Slot 理쒖쟻??(P0 ?곗꽑?쒖쐞)
- [x] **?ㅽ깮 ?좊떦 理쒖쟻??* ???ㅼ퐫?꾨? 踰쀬뼱?섏? ?딅뒗 Slot? malloc ???alloca
  - ?꾨즺: `slot_analyze_escape_flags()` (slot_analyzer.c)
  - ?꾨즺: LLVM 諛깆뿏?쒖뿉??`slot_escapes == false` ??alloca ?앹꽦 (llvm_stmt.c:145-146)
  - ?꾨즺: escape analysis濡?non-escaping slot ?먮룞 ?ㅽ깮 ?좊떦

### View 踰붿쐞 遺??(由щ럭 ?꾩슂 ??誘멸껐??
- [ ] **View??諛붿씠???몃뜳??踰붿쐞 遺??* ???ㅼ젣 ?ъ슜 ?щ? 留뚮뱾?대낫怨?寃곗젙
  - ??A: Slice 湲곕컲 ??`SliceOf(buf, 0, 1024)` ??Slot??"李쎈Ц"
  - ??B: View??踰붿쐞 遺????`ViewRead(buf, offset, length)`
  - **誘멸껐?????뚯씪 I/O, ?ㅽ듃?뚰겕 踰꾪띁, GPU ?띿뒪泥??щ?瑜?留뚮뱾?대낫怨?寃곗젙**

### 蹂묐젹/梨꾨꼸
- [x] **select ?ㅼ껜??* ???щ윭 梨꾨꼸 以?癒쇱? 以鍮꾨맂 寃껋쓣 泥섎━

### ?몄뼱 ?꾩꽦??Tier 1 ??踰붿슜 ?꾩닔
- [x] **for-in 而щ젆??猷⑦봽** ??`for item in array { }` 諛곗뿴/而щ젆???쒗쉶
  - ?꾨즺: Array<T>/Slice<T> ?뱀닔??(index loop lowering), ?쒕㎤??element type 異붾줎
  - ?⑥쓬: ability 湲곕컲 Iterable<T> ?꾨줈?좎퐳 (Tier 2)
- [x] **StringSplit / StringJoin** ??臾몄옄??遺꾨━/寃고빀 鍮뚰듃???ㅼ껜??  - ?꾨즺: `Split(s, delim) ??Array<String>`, `Join(arr, sep) ??String`
- [x] **ToInt / ToFloat** ??臾몄옄?닳넂?レ옄 蹂??鍮뚰듃??- [x] **湲곕낯 Math 鍮뚰듃??* ??Sqrt, Pow, Floor, Ceil, Random 異붽? (湲곗〈 Abs/Min/Max + ?좉퇋 5媛?
- [x] **ArraySort / ArrayMap / ArrayFilter / ArrayReverse** ??怨좎감 ?⑥닔 湲곕컲 而щ젆???곗궛
  - ?꾨즺: ArraySort(arr) ??qsort, ArrayMap(arr, fn) ????諛곗뿴, ArrayFilter(arr, fn) ??議곌굔 ?꾪꽣, ArrayReverse(arr) ???ㅼ쭛湲?  - fn? ?⑥닔 ?대쫫 ?먮뒗 ?뚮떎 (C ?⑥닔 ?ъ씤?곕줈 lowering)
- [x] **?붿뒪?몃윮泥섎쭅** ??`let (a, b, c) = expr` 諛곗뿴/而щ젆??positional 諛붿씤??  - ?꾨즺: Array<T> ???몃뜳??湲곕컲 異붿텧 (`result.data[0]`, `result.data[1]`, ...)
  - MIR ?듯빀 (2026-04-19): MIR DCE媛 `AST_LET_DESTRUCTURE` 臾몄쓣 "遺?묒슜 ?놁쓬"?쇰줈 ?먯젙???쒓굅?섎뜕 踰꾧렇 ?섏젙 (`mir_stmt_has_side_effect`). ?몃옖?ㅽ뙆?쇰윭 MIR emit 猷⑦봽?먯꽌 destructuring??SSA-renamed ?寃잛쑝濡?emit, `transpiler_find_local_type_name_in_block`??AST_LET_DESTRUCTURE 耳?댁뒪 異붽???濡쒖뺄 ????댁꽍 蹂듦뎄
  - LLVM parity (2026-04-19): `llvm_emit_statement`??AST_LET_DESTRUCTURE 耳?댁뒪 異붽? ??珥덇린?붿떇??struct 媛믪쑝濡??됯?, `ExtractValue(0)`?쇰줈 data pointer 異붿텧, 媛?諛붿씤?⑸쭏??`GEP+Load`濡??붿냼 異붿텧 ??`alloca+store`+`llvm_scope_declare`濡?濡쒖뺄 ?깅줉. `llvm_lookup_array_var`濡?elem_type ?댁꽍
  - ?뚯씪: `src/compiler/mir.c`, `src/codegen/transpiler_emitters_base_a.inc` (C 諛깆뿏??, `src/codegen/llvm_stmt.c` (LLVM 諛깆뿏??
  - ?뚭?: `tests/cases/backend_compare/destructure_array/main.pgy` (C/LLVM ?숈씪 異쒕젰), `examples/collection_ops.pgy` (hello/world/foo 異쒕젰)

### 硫뷀??꾨줈洹몃옒諛??낆옣 (寃곗젙 ?꾨즺)
- [x] **TMP 鍮꾩콈??* ???쒕꽕由?monomorphization + ability dispatch濡?95% 而ㅻ쾭. 臾몄꽌: `docs/23_metaprogramming_position.md`
- [ ] **?ν썑 肄붾뱶 ?앹꽦 ?꾩슂 ??* ??而댄뙆??????뚮윭洹몄씤 (proc_macro 紐⑤뜽) ?먮뒗 ?뚯뒪 ?앹꽦湲?寃??
### ?몄뼱 ?꾩꽦??Tier 2 ???ㅼ궗???몄쓽
- [ ] **innate ability** ??媛숈? 紐⑤뱢 ??role留?impl ?덉슜 (sealed ???innate 梨꾪깮. 臾몄꽌: `docs/24_visibility_model.md`)
  - ?뚯꽌 ?꾨즺, ?쒕㎤?깆뿉??`innate` ?ㅼ썙???몄떇 (type_checker_decls.inc 李몄“)
  - ?⑥쓬: 紐⑤뱢 寃쎄퀎 寃利?濡쒖쭅 ?꾩꽦
- [x] **?쒕꽕由?constraint ?쒕㎤??* ??`where T: Comparable` ?쒕㎤??寃利?  - ?꾨즺: ?뚯꽌 + ?쒕㎤??寃利?(type_checker_helpers.inc:1847)
  - ?꾨즺: Generic function where-clause constraint validation
- [x] **OR ?⑦꽩** ??`case 1 | 2 | 3:` match?먯꽌
  - ?꾨즺: lexer `TOKEN_PATTERN_OR`, parser ?뚯떛, ?쒕㎤??寃利?  - ?꾨즺: 由ы꽣??OR ?⑦꽩 吏??(`case 1 | 2 | 3:`)
  - ?쒗븳: variant destructuring OR ?⑦꽩? ?꾩쭅 誘몄???(`case .Some(v) | .None:`)
- [x] **enum 硫붿꽌??* ??`enum Direction { ... func Name(self) -> String }`
  - ?꾨즺: enum body?먯꽌 `func` ?좎뼵 + `self` ?뚮씪誘명꽣濡?match self 蹂몃Ц 媛?? C 而댄뙆??寃利?- [x] **labeled break/continue** ??`outer: while { ... break outer; }`
  - ?꾨즺: ?뚯꽌 (`parser.c:1270`), AST (`break_stmt.label`), ?쒕㎤??(`test_semantic.c:680,714,739`), C 肄붾뱶??(`loop_break_labels[]` + `loop_continue_labels[]`)
  - 寃利? outer label break, ?????녿뒗 label 嫄곕?, continue outer 紐⑤몢 ?뚭? ?뚯뒪???듦낵
- [x] **Custom error ???* ??`Result<T, E>` where E is user type (?꾩옱 String留?
  - ?꾨즺 (2026-04-18): ??낅챸 ?뚮뜑 `PgyResult_Int_NetError` sanitize, `PGY_RESULT_DEFINE(Int_NetError, int32_t, NetError)` ?먮룞 instantiation (`ensure_result_specialization_to` ?좎꽕), ?몄쓽 留ㅽ겕濡?(`Ok_T_E`, `Err_T_E`, `IsOk_T_E`, `Unwrap_T_E`, `UnwrapOr_T_E`) ?먮룞 ?앹꽦, Ok/Err builtin??`ctx->current_return_type`?먯꽌 suffix 異붿텧, match pattern Ok/Err 諛붿씤??`__typeof__` 湲곕컲 ???異붾줎
  - ?뚯씪: `src/codegen/transpiler_helpers_core_b.inc` (generic_args_to_c_suffix + ensure_result_specialization_to), `src/codegen/transpiler_expr_emitters.inc` (Ok/Err/Unwrap suffix), `src/codegen/transpiler_emitters_base_b.inc` (match __typeof__), `src/codegen/transpiler.h` (result_specs_*)
  - ?뚭?: `src/test_semantic.c` "Result<T, E> with enum error type accepts Ok/Err and match destructuring"

### ability 李⑤퀎??- [x] **ability ??interface 臾몄꽌??* ??ability??"?묒뾽 ?꾨줈?좎퐳???먭꺽 議곌굔"?대ŉ ?щ’??遺李⑸맖
  - ?꾨즺: `docs/24_visibility_model.md`??`ability ??interface` ?뱀뀡 異붽?
  - ?뺣━ ?댁슜: ability??nominal object??硫붿꽌??吏묓빀??吏곸젒 紐⑤뜽留곹븯??interface媛 ?꾨땲?? `requires Ability`, `dyn role slot: Ability`, `zone authority requires Ability`泥섎읆 ?묒뾽 怨꾩빟/?먭꺽 議곌굔?쇰줈 ?뚮퉬?섎뒗 surface?꾩쓣 怨좎젙
  - ?뺣━ ?댁슜: ability??subject/role/slot/orchestration contract? 寃고빀?섎ŉ, 援ы쁽 ?대떦? role impl?닿퀬 ability ?먯껜??"臾댁뾿??援ы쁽?섎씪"蹂대떎 "?대뼡 ?먭꺽?쇰줈 李몄뿬?섎씪"瑜??쒗쁽?쒕떎???먯쓣 紐낆떆

## P1.6 ???먯썝/?ㅼ??ㅽ듃?덉씠??諛⑺뼢 怨좎젙

### 遺꾩궛 ?ㅺ퀎 寃곗젙 (2026-04-03 ?뺤젙)
- [x] **RemoteFuture `await` ??`Result<T>` 媛뺤젣** ???먭꺽 ?먯썝??吏???ㅽ뙣瑜?????쒖뒪?쒖뿉??媛뺤젣 ?몄텧
  - `Future<T>` (濡쒖뺄) ??await ??`T` (?ㅽ뙣 ?놁쓬)
  - `RemoteFuture<T>` (?먭꺽) ??await ??`Result<T>` (?ㅽ뙣 媛??
  - ?쒕㎤??泥댁빱 + C 肄붾뱶??+ ?고???留ㅽ겕濡?援ы쁽 ?꾨즺
  - ?뚯뒪?? 205 semantic + 141 transpile ?듦낵
- [x] **RemoteFuture??Claim/Read/Write/Release 李⑤떒** ???먭꺽 ?먯썝???숈궗??Submit/Await留?  - Read/Write/Release ?몄텧 ??移쒖젅???먮윭 硫붿떆吏 異쒕젰
  - "RemoteFuture does not support Read(); use 'await' to obtain Result<T>"
- [ ] **?먭꺽 Slot? Claim ?놁씠 Channel 湲곕컲 硫붿떆吏 ?⑥떛留?* ??遺꾩궛 ???뚰뵾
  - ?щ줈??World ?듭떊? `Channel<T>`留??덉슜
  - ?먭꺽 ?먯썝??Claim ?숈궗瑜??ъ슜?섎㈃ 而댄뙆???먮윭
- [x] **World 寃쎄퀎 = ?ㅽ뙣 ?꾨찓??寃쎄퀎** ???щ줈??World ?듭떊? Channel留?  - ?꾨즺: World ?쒕㎤??泥댁빱 (`type_check_world_decl`, type_checker_decls.inc)
  - ?꾨즺: World 肄붾뱶??(C 諛깆뿏?? transpiler_helpers.h)
  - ?꾨즺: `HasZoneProjection`, `HasZoneLayer`, `HasZoneState` builtin

### Projection / Domain Query
- [x] **Projection query surface** ??`HasProjection(slotName)`?쇰줈 relation/effect/zone 臾몃㎘?먯꽌 object/tobject projection slot??sync-ready ?щ?瑜?吏덉쓽
  - ?꾨즺: semantic + C/LLVM lowering
  - World ?대???Slot? 濡쒖뺄 (zero-cost), World 媛꾩? Channel (紐낆떆??鍮꾩슜)

### ?ㅼ??쇰쭅 ???(?덈뱶? ?쇰뱶諛?湲곕컲)
- [ ] **諛깆뿏????븷 而룹삤??怨좎젙** ??C = reference/fallback, LLVM = optimization/mainline
  - 媛숈? ?섎?濡좎쓣 ??諛깆뿏?쒖뿉 ?좎??섎릺, 怨듦꺽??理쒖쟻?붿? type-erased fast path??LLVM?먮쭔 吏묒쨷
  - C 諛깆뿏?쒕뒗 MVP ?명솚?? ?붾쾭源? ?대갚, 遺?몄뒪?몃옒????븷濡??쒗븳
  - ??湲곕뒫 異붽? ??"C?먯꽌??諛섎뱶??理쒖쟻??寃쎈줈源뚯? 援ы쁽?댁빞 ?섎뒗媛?"瑜?湲곕낯?곸쑝濡?`?꾨땲??濡???- [ ] **留ㅽ겕濡?議고빀 ??컻 ???* ??C 留ㅽ겕濡?monomorphization???κ린 ???  - ?꾩옱: `PGY_SLOT_DEFINE`, `PGY_CHANNEL_DEFINE` ????낅퀎 ?꾧컻 (遺?몄뒪?몃옒???꾨왂)
  - ??? LLVM 諛깆뿏?쒖뿉??type-erased 寃쎈줈 (opaque ptr + vtable) 異붽?
  - LTO + dead code elimination?쇰줈 諛붿씠?덈━ 鍮꾨????듭젣
- [ ] **肄붾뱶???댁쨷???듭젣 洹쒖튃** ??bifurcation trap 諛⑹?
  - ?숈씪 湲곕뒫??C/LLVM lowering???곸썝???띿쑝濡?鍮꾨??댁?吏 ?딄쾶 怨듯넻 ?섎?濡??뚯뒪???곗꽑
  - backend compare / smoke瑜?怨꾩빟?쇰줈 ?좎??섍퀬, backend-specific fast path??紐낆떆?곸쑝濡?遺꾨━
- [ ] **Async ???좊떦 ?ㅻ쾭?ㅻ뱶 媛먯냼** ??怨좎꽦??遺꾩궛 I/O瑜??꾪븳 ?고???理쒖쟻??  - ?꾩옱: `pgy_spawn` + `malloc` per task
  - ??? Arena allocator 湲곕컲 task pool, io_uring/IOCP zero-copy I/O
  - 肄붾（???ㅽ깮? ?대? fiber 湲곕컲 (pgy_parallel.h)
  - ?? ?몄뼱 肄붿뼱? OS ?꾩슜 ?ㅼ?以꾨윭瑜?媛뺢껐?⑺븯吏 留?寃?- [ ] **BYOS (Bring Your Own Scheduler) 寃쎈줈 ?ㅺ퀎** ??async ?섎?濡좉낵 ?ㅼ?以꾨윭/I/O 紐⑤뜽 遺꾨━
  - ?몄뼱??task/future/channel ?섎?留?怨좎젙
  - ?ㅼ젣 polling/runtime? ?뚮옯?쇰퀎 二쇱엯 媛??怨꾩링?쇰줈 遺꾨━
- [ ] **ABI ?ㅽ삎???꾨왂** ???ш린媛 ?ㅻⅨ ?щ’ ??낆쓽 ?쒕꽕由?泥섎━
  - ?섎룄???ㅺ퀎: `Slot<T>` ??`SecureSlot<T>` (蹂댁븞 李⑥썝 遺꾨━)
  - ?ㅽ삎???꾩슂 ?? `ability` vtable dispatch (Party ?쒖뒪?쒖뿉 ?대? 援ы쁽)
  - Boxing ?꾩슂 ?? `Rc<T>` + ability 議고빀
  - `Rc<T> + dyn ability`??explicit high-cost path濡?臾몄꽌??  - 媛?寃쎈줈(struct), 媛앹껜 寃쎈줈(class), ?숈쟻 寃쎈줈(Rc + dyn ability)瑜??깅뒫 怨꾩빟?쇰줈 援щ텇

### 湲곗〈 ??ぉ
- [x] **Slot Protocol 怨좎젙** ??Claim/Access/Mutate/Transfer/Release 遺덈? 怨꾩빟
- [x] **Slot/View 怨꾩링 留덇컧** ??ReadView/WriteView/MoveToken 沅뚰븳 異뺤냼/?댁쟾 怨꾩링
- [ ] **?щ’??異붿긽 ?먯썝 ?몃뱾濡??쇰컲??* ???κ린?곸쑝濡?MemorySlot, DeviceSlot, SessionSlot ???먯썝 ?대옒???뺤옣
- [ ] **梨꾨꼸 ?섎?濡?媛뺥솕** ??鍮꾨룞湲??쒖텧/?湲??섍굅/?꾩쿂由??먮쫫 蹂닿컯
- [x] **`Future<T>`瑜?transfer boundary濡?怨좎젙** ??await/recv? 媛숈? ownership 寃쎄퀎
- [ ] **effect/resource capability ?쒓린 ?꾩엯** ??`local cpu`, `secure device`, `remote` ??????④낵 ?쒖뒪??  - ?꾩옱: derived effect mask + spawn/await/channel?먯꽌 remote 異붾줎
  - ?꾩옱: `/// @effects ...` ?좎뼵???덉쑝硫?body derived effect? mismatch 吏꾨떒
  - ?ㅼ쓬: ?쒓렇?덉쿂 臾몃쾿 李⑥썝???좎뼵??annotation ?쒕㈃
- [ ] **?깅뒫 紐⑺몴瑜?orchestration overhead 以묒떖?쇰줈 ?ъ젙??*

## P1.7 ???섎? ?듭씪 ?몄뼱濡쒖꽌???ㅼ쓬 ?④퀎

### 鍮꾩슜 紐⑤뜽 / effect
- [ ] **鍮꾩슜 紐⑤뜽 ?쒕㈃??* ??"semantic unity, visible cost" ?먯튃
  - `local / secure / remote / device` ?먯썝援곗쓽 鍮꾩슜 李⑥씠瑜??쒕㈃???쒕윭?닿린
- [ ] **effect system 2?④퀎** ???좎뼵??effect ?쒓린, mismatch 吏꾨떒
  - 遺遺??꾨즺: structured comment `@effects` 湲곕컲 mismatch 吏꾨떒
  - 遺遺??꾨즺: source-level `with effects ...` ?쒓렇?덉쿂 surface
  - ?⑥쓬: ???뺢탳??effect lattice, call-site contract surface

### ?곸쐞 怨꾩링 紐⑤뜽
- [x] **理쒖쥌 臾몃㎘ 怨꾩링 / ?ㅺ퀎 ?쒖꽌 遺꾨━ 怨좎젙**
  - 議곕┰ 怨꾩링: `ability -> role -> party -> relation -> effect -> zone -> world`
  - ?ъ슜??facing ?ㅺ퀎 ?쒖꽌: `intent -> world -> zone -> subject`
  - ?꾨즺: `world`瑜?理쒖긽???ㅽ뻾/?좊ː/?ㅽ뙣 寃쎄퀎?쇰뒗 紐⑺몴 ?뺤쓽濡?臾몄꽌??  - ?꾨즺: ?곸쐞 ?덉씠?대줈 媛덉닔濡???援ъ냽?곸씠?쇰뒗 ?ㅺ퀎 ?먯튃 臾몄꽌??  - ?꾨즺: `relation`, `effect`, `zone` declaration keyword? 理쒖냼 `subject slot` / `object slot` surface瑜?parser/semantic ?쒕㈃???곌껐
  - ?꾨즺: `zone -> relation/effect`, `world -> zone` 理쒖냼 議곕┰ slot surface瑜?parser/semantic???곌껐
  - ?꾨즺: `relation`, `effect`??optional `for ...` header濡?subject endpoint/target 理쒖냼 surface瑜??곌껐
  - ?꾨즺: `zone`??`apply effectSlot to targetSlot` 理쒖냼 attachment surface瑜?parser/semantic???곌껐
  - ?꾨즺: `zone`??`link relationSlot between left, right` 理쒖냼 relation wiring surface瑜?parser/semantic???곌껐
  - ?꾨즺: `zone`??`detach effectSlot from targetSlot`, `unlink relationSlot between left, right` 理쒖냼 release surface瑜?parser/semantic???곌껐
  - ?꾨즺: `zone`??`apply/detach`, `link/unlink`瑜?`effect/relation` declaration contract? 湲곕낯 ???arity ?섏??쇰줈 ?곌껐
  - ?꾨즺: `zone` subject shape?????沅뚯옣 lint 異붽?
  - ?꾨즺: `tobject` keyword瑜?`struct` ?명솚 projection alias濡?異붽?
  - ?꾨즺: `ToObject(TargetStruct, subjectBinding)` 理쒖냼 passive projection surface瑜?semantic/C backend???곌껐
  - ?꾨즺: `ToTObject(TargetDto, subjectBinding)` 理쒖냼 projection surface瑜?semantic/C backend???곌껐
  - ?꾨즺: `relation/effect/zone`??`tobject slot` surface瑜??곌껐
  - ?꾨즺: `relation/effect/zone`??domain slot??optional initializer瑜??곌껐??`object slot view: View = ToObject(View, subject)` 媛숈? projection wiring??吏곸젒 ?쒗쁽 媛?ν븯寃???  - ?꾨즺: `zone`??`refresh objectSlot from subjectSlot` surface濡?projection 媛깆떊 ?먮쫫??parser/semantic???곌껐
  - ?꾨즺: `zone`??`publish dtoSlot from subjectSlot` surface濡?tobject projection 媛깆떊 ?먮쫫??parser/semantic???곌껐
  - ?꾨즺: `zone`??`maintain effectSlot on targetSlot`, `maintain relationSlot between left, right` surface濡?吏??lifecycle rule??parser/semantic???곌껐
  - ?꾨즺: `maintain` duplicate/conflict warning (`maintain` + `detach/unlink`) 異붽?
  - ?꾨즺: `zone`??`authority subjectSlot` surface? optional `by subjectSlot` authority annotation??parser/semantic???곌껐
  - ?꾨즺: `authority subjectSlot requires Ability[, Ability]` ability-gated authority surface瑜?parser/semantic???곌껐
  - ?꾨즺: `zone`??`state name: effect ... on ...` / `state name: relation ... between ..., ...` lifecycle alias surface瑜?parser/semantic???곌껐
  - ?꾨즺: `zone`??`apply/link/detach/unlink/maintain stateName` shorthand瑜?parser/semantic???곌껐
  - ?꾨즺: `HasState(stateName)` zone query builtin??parser/semantic???곌껐?섍퀬 C backend?먯꽌 zone state field query濡?lowering
  - ?꾨즺: `HasLayer(layerSlot)` zone query builtin??parser/semantic???곌껐?섍퀬 C/LLVM backend?먯꽌 zone layer field query濡?lowering
  - ?꾨즺: `HasState(effectState, targetSlot)` / `HasState(relationState, leftSlot, rightSlot)` slot-aware state query瑜?semantic???곌껐
  - ?꾨즺: `world`??`state name: zone zoneSlot`, `activate/deactivate/maintain zoneOrState` lifecycle surface瑜?parser/semantic???곌껐
  - ?꾨즺: `HasZone(zoneOrState)` world query builtin??parser/semantic???곌껐?섍퀬 C backend?먯꽌 world zone-state/active field query濡?lowering
  - ?꾨즺: C backend媛 zone/world留덈떎 sync helper瑜??앹꽦?섍퀬 method ?꾪썑??`refresh`/`publish` projection怨?lifecycle flag瑜?incremental?섍쾶 ?숆린??  - ?꾨즺: `relation`, `effect` declaration??C/LLVM backend?먯꽌 struct + method wrapper濡?codegen?섍퀬 runtime instance constructor/method path媛 ?곌껐??  - ?꾨즺: `zone` layer slot??C/LLVM?먯꽌 typed overlay runtime instance濡??좎??섍퀬 sync媛 subject slot??layer endpoint/target??諛붿씤?⑺븳 ??projection sync源뚯? ?섑뻾
  - ?꾨즺: direct `apply/link/detach/unlink`? `maintain effect/relation/state`媛 C/LLVM zone sync?먯꽌 ?ㅼ젣 layer/state propagation?쇰줈 ?곌껐??  - ?꾨즺: zone embedded overlay projection read (`self.poison.view.hp`, `self.trust.packet.name`)媛 LLVM runtime smoke濡?寃利앸맖
  - ?꾨즺: `world`媛 `HasZoneProjection(zoneSlot, projectionSlot)` / `HasZoneLayer(zoneSlot, layerSlot)` / `HasZoneState(zoneSlot, stateName)`濡?embedded zone runtime flag瑜?吏곸젒 吏덉쓽?????덉쓬
  - ?꾨즺: `ability/role/party/relation/effect/zone/roster/world` ?꾩껜 援ы쁽
  - ?꾨즺: `world`媛 `state name: all zoneOrState[, ...]` / `state name: any zoneOrState[, ...]`濡??욎꽌 ?좎뼵??zone/state alias瑜?理쒖냼 議고빀 contract濡??⑹꽦
  - ?⑥쓬: richer world-level runtime semantics, ??源딆? cross-layer propagation policy

### 議댁옱濡?紐⑤뜽
- [x] **intent-first ?ㅺ퀎 異?/ subject-core host 異?遺꾨━ 怨좎젙**
  - ?꾨즺: ?ъ슜??facing ?ㅺ퀎 ?쒖꽌??`intent -> world -> zone -> subject`濡?臾몄꽌??  - ?꾨즺: `subject = ?곹깭? identity瑜?媛吏?二쇱껜 ???? host/naming/lowering 異뺤쑝濡??쒖젙??臾몄꽌??  - ?꾨즺: `subject`? `class`瑜??쒕줈 ?ㅻⅨ nominal flavor濡?遺꾨━?섍퀬 ?섎?濡좊룄 1李?遺꾧린
  - ?꾨즺: legacy host-profile surface瑜??쒓굅?섍퀬 `subject`/`object`/`intent` 以묒떖?쇰줈 ?뺣━
  - ?꾨즺: `entity`??肄붿뼱 ?몄뼱 議댁옱濡좎뿉 ?ｌ? ?딄퀬 ?꾨젅?꾩썙???꾨찓???⑹뼱濡??④릿?ㅺ퀬 臾몄꽌??  - ?꾨즺: `object`??intent瑜??쒖옉?섏? ?딅뒗 passive state target?대씪怨?臾몄꽌??  - ?꾨즺: `tobject`??object???몃? 寃쎄퀎??異뺤빟 ?ъ쁺?대씪怨?臾몄꽌??  - ?꾨즺: `subject`, `class`, `struct`, `object`, `tobject` declaration flavor瑜?parser AST??遺꾨━ 湲곕줉
  - ?꾨즺: `subject slot`怨?`ToObject` / `ToTObject` source媛 `subject` host留?諛쏅룄濡?semantic 遺꾧린
  - ?꾨즺: `object` keyword alias瑜?parser/LSP surface??諛섏쁺
  - ?꾨즺: `object`瑜?passive state/value ?뺤떇?쇰줈, `tobject`瑜???醫곸? projection/value ?뺤떇?쇰줈 ?뺣━?섍퀬 helper method瑜??덉슜
  - ?꾨즺: `vessel` declaration怨?`subject` ?대? `vessel` field surface 異붽?
  - ?꾨즺: `subject` ?꾩슜 `action` declaration怨?理쒖냼 clause (`requires/within/causes/authorized by`) parser/semantic ?곌껐
  - ?꾨즺: `subject` ?덉쓽 legacy `func` ?쒓굅, `action` only ?뺤콉?쇰줈 ?밴꺽
  - ?꾨즺: `role`/`party`/`authority`瑜?subject-core host 異뺤쑝濡???媛뺥븯寃??쒗븳
  - ?꾨즺: C/LLVM method lowering?먯꽌 `subject=self-cell`, `class=value self` 1李?遺꾧린
  - ?꾨즺: legacy host-profile surface瑜??쒓굅?섍퀬 愿??洹쒖튃??`subject`???듯빀
  - ?꾨즺: `subject` ?⑥씪 host surface濡??듭씪
  - ?꾨즺: standalone host-profile surface ??젣
  - ?꾨즺: object瑜?effect/relation target?쇰줈 semantic/C/LLVM???곌껐
  - ?꾨즺: domain-local `refresh` / `publish` source瑜?subject/object源뚯? ?뺤옣?섍퀬 tobject source??湲덉?
  - ?꾨즺: relation/projection 以묒떖 surface 怨좎젙

### 臾몄꽌 / ?ㅽ????뺣젹
- [ ] **BSD (Allman) canonical style ?꾨㈃ 怨좎젙**
  - 臾몄꽌/?덉젣/scaffold/formatter 異쒕젰? BSD 湲곗??쇰줈 ?듭씪
  - K&R? parser compatibility濡쒕쭔 ?④린怨?canonical surface濡쒕뒗 痍④툒?섏? ?딆쓬
- [x] **臾몄꽌 ?덉젣 ?쒖떆 ?쒖꽌 媛뺤젣**
  - ?꾨즺: README entrypoint? ?듭떖 ?ㅺ퀎 臾몄꽌?먯꽌 ?덉젣 ?낇빐 ?쒖꽌瑜?`intent -> world -> zone -> subject`濡?紐낆떆
  - 湲곗? 臾몄꽌: `README.md`, `docs/00_vision.md`, `docs/01_intent_first_design.md`, `docs/22_class_object_model.md`
  - 洹쒖튃: `subject`??core host濡??ㅻ챸?섎릺, ?ㅺ퀎??泥?異뺤쑝濡?媛瑜댁튂吏 ?딆쓬
  - 洹쒖튃: compile-order? teaching-order瑜?遺꾨━?댁꽌 紐낆떆

### slot 沅뚰븳 / ?먯썝援??뺤옣
- [ ] **slot 沅뚰븳 紐⑤뜽 怨좊룄??* ??怨듭쑀 ?쎄린 vs ?낆젏 ?곌린, capability narrowing
- [ ] **?ㅼ젣 ?먯썝援??뺤옣** ??SessionSlot, ChannelSlot, RemoteJob 怨좊룄??- [x] **subject/class/object model 援ы쁽 ?뺣젹**
  - ?꾨즺: subject direct copy/plain value parameter/return 湲덉?, positional constructor
  - ?꾨즺: C/LLVM lowering 1李?遺꾧린 (`subject=self-cell`, `class=value self`)
  - ?꾨즺: legacy host-profile??`subject` 洹쒖튃?쇰줈 ?듯빀
  - ?꾨즺: `subject` ?⑥씪 host surface濡??듭씪
  - ?꾨즺: plain/secure `Slot<subject>` local object-cell anchor 吏??  - ?꾨즺: `own/ref Slot<subject-host>` / `SecureSlot<subject-host>` ?⑥닔 寃쎄퀎 ?꾨떖??semantic + C/LLVM backend??諛섏쁺
  - ?꾨즺: `Box<class>` explicit handle surface (`Box`, `BoxGet`, `BoxSet`, `BoxDrop`, `BoxIsValid`)
  - ?꾨즺: richer object-handle cell propagation

### orchestration ?꾩꽦??- [ ] **?ㅼ??ㅽ듃?덉씠??紐⑤뜽 媛뺥솕** ??select 怨듭젙?? timeout, cancellation, backpressure
  - 遺遺??꾨즺: `TryRecv/RecvTimeout -> Option<T>`, `TrySend/SendTimeout -> Bool`
  - 遺遺??꾨즺: `TrySendStatus/SendTimeoutStatus -> Option<Bool>`濡?full/timeout vs closed瑜?媛믪쑝濡?援щ텇
  - 遺遺??꾨즺: `ChannelLength/ChannelCapacity/ChannelSpace -> Int`, `ChannelFull/ChannelClosed -> Bool`
  - 遺遺??꾨즺: `select` round-robin ?쒖옉 ?몃뜳??fairness
  - 遺遺??꾨즺: `Cancel(task)` / `IsCancelled()` cooperative cancellation
  - 遺遺??꾨즺: spawned descendant cancellation propagation
  - ?꾩옱 ?쒗븳: movable resource channel??non-blocking/timeout transfer??誘몄???  - ?꾩옱 ?쒗븳: pressure observation? 媛?ν븯吏留?bounded policy/backpressure protocol? ?꾩쭅 誘멸뎄??  - ?꾩옱 ?쒗븳: preemptive cancellation, blocked thread task interruption, structured cancellation scope/lattice??誘몄???- [x] **async/await runtime 怨좊룄??* ??POSIX ucontext + Windows Fiber 湲곕컲 coroutine
- [ ] **Windows coroutine 寃利?怨좎젙**

### ?대쭅 / ?쒖?硫?- [ ] **stable stdlib surface ?ш퀬??*
- [ ] **?대쭅 ?④퀎 吏꾩엯** ??formatter, LSP 吏꾨떒 ?덉쭏
- [x] **ontology-first scaffold ?뺣젹**
  - ?꾨즺: `pgy scaffold` help瑜?`subject/class/object/tobject` ?곗꽑 遺꾧린濡??뺣젹
  - ?꾨즺: `class` scaffold kind 異붽?
  - ?꾨즺: `project/simulator` scaffold媛 `subject`媛 `class`瑜??뚯쑀?섍퀬 `object/tobject`濡??ъ쁺?섎뒗 starter shape瑜??앹꽦
  - ?꾨즺: `project` scaffold媛 intent-first layout(`intents/`, `subjects/`, `zones/`, `world.pgy`, `main.pgy`)???ㅼ젣濡??앹꽦
  - ?꾨즺: `pgy new`媛 `intent-first` / `class-first` / `projection-first` starter瑜??좏깮?섍쾶 ?좎? 寃??  - ?꾨즺: `pgy new` / scaffold output??ontology decision guide file 蹂꾨룄 ?앹꽦 寃??  - ?꾨즺: intent-first project guide 臾몄꽌??scaffold output??媛숈씠 ?앹꽦?좎? 寃??    - `intents/`瑜??꾨줈?앺듃 table-of-contents濡??ㅻ챸?섎뒗 guide ?ы븿
    - intent declaration???꾩슂??subject/zone/ability/effect TODO瑜???궛?섎뒗 workflow ?덉떆 ?ы븿
  - ?꾨즺: intent runtime follow-up
    - rollback policy瑜?current reverse-order `compensate` beyond v1濡??뺤옣?섍린
    - intent??cross-world transfer / identity handoff semantics ?ㅺ퀎 諛?援ы쁽
    - current last-intent typed history瑜?trace id / stream / multi-instance observability濡??뺤옣?섍린

### ????꾨줈洹몃옩
- [ ] **????좏뵆由ъ??댁뀡 3醫?* ???댁쥌 ?먯썝 ?뚯씠?꾨씪?? secure+device+channel, slot/orchestration 泥좏븰 利앸챸

## P1.85 ??寃뚯엫 ?꾨젅?꾩썙??怨꾩링

- [ ] **寃뚯엫 ?꾨젅?꾩썙???쇱씠釉뚮윭由?寃쎄퀎 怨좎젙**
  - ?먯튃: `entity/object pool`? ?몄뼱 肄붿뼱 湲곕뒫???꾨땲??`use pool;` 媛숈? 寃뚯엫/???쇱씠釉뚮윭由?怨꾩링?쇰줈 ?붾떎
  - ?먯튃: `encounter/turn/state machine`, `strategy/AI`, `content tables`???숈씪?섍쾶 肄붿뼱 臾몃쾿???꾨땲???꾨젅?꾩썙??surface濡??볥뒗??  - ?먯튃: ??怨꾩링? ?쒕룄硫붿씤 ?쇱씠釉뚮윭由р앸낫???쐅eneric pattern library + domain injection?앹쑝濡??뺤쓽?쒕떎
  - ?댁쑀: 肄붿뼱 ?몄뼱??`subject / vessel / object / tobject / relation / effect / zone / world / Slot<T>` ?섎?濡좎쓣 ?좎??섍퀬, ?洹쒕え 寃뚯엫 ?ㅺ퀎??洹??꾩쓽 library/DSL 怨꾩링?쇰줈 ?щ━???몄씠 ?뺤옣?깃낵 ?ㅻ챸?μ씠 ??醫뗫떎
  - 紐⑺몴: ?쒓쾶?꾩쓣 留뚮뱾 ???덈뒗 肄붿뼱 ?몄뼱?앹? ?쒓쾶?꾩쓣 ?ㅼ젣濡?留뚮뱶???꾨젅?꾩썙?р앸? 遺꾨━
- [ ] **寃뚯엫 stdlib/use surface 珥덉븞**
  - ?꾨낫: `use pool;`, `use fsm;`, `use encounter;`, `use strategy;`, `use tables;`
  - 諛⑺뼢: pool/fsm/strategy/table? `.pgy` ?먮뒗 stdlib 紐⑤뱢濡??쒓났?섍퀬, ?몄뼱 ?ㅼ썙?쒕줈 ?밴꺽?섏? ?딅뒗??  - 諛⑺뼢: `Pool<T>`, `StateMachine<TState, TEvent>`, `StrategyTable<TContext, TChoice>`, `WeightedTable<T>`泥섎읆 generic-first naming???곗꽑?쒕떎
  - 諛⑺뼢: GOF 湲곗큹 ?⑦꽩??inheritance/object graph媛 ?꾨땲??Pergyra host 湲곗??쇰줈 踰덉뿭?쒕떎
    - `singleton` -> contextual runtime registry / host-local shared state
    - `factory` -> staged template/spec builder
    - `strategy` -> policy card / policy table + function injection
    - `state` -> explicit FSM / transition rule + context application
    - `observer` -> relay bundle / sink spec / report sink / event bus
  - 諛⑺뼢: generic pattern library??static spec/table留뚯씠 ?꾨땲??function-typed picker/resolver 二쇱엯??湲곕낯 ?쒕㈃?쇰줈 ?ы븿?쒕떎
    - ?? `Picker<TInput, TChoice>`
    - ?? `Resolver<TContext, TResult>`
    - ?? `StrategyApply(context, AggressivePolicy)`
  - ?꾩옱 ?곹깭: `data/card/table` 寃쎈줈???덉젙, custom function injection??V1 ?쒕㈃???щ씪??  - ?꾩옱 ?꾨왂 ?⑦꽩???덉젙 ?④퀎:
    - `StrategyCard`
    - `StrategyContext`
    - `ApplyStrategy(card, context)`
  - ?대쾲 ?덉젣 湲곗? ?쇱씠釉뚮윭由ы솕 ?꾨낫:
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
  - 湲곗? 臾몄꽌: `docs/31_gof_pattern_catalog.md`
  - 湲곗? ?덉젣: `examples/pattern_library_basics/`
  - 紐⑺몴: ?꾪넻 OOP ?⑦꽩 ?대쫫???좎??섎뜑?쇰룄 ?ㅼ젣 援ы쁽 shape??`subject / vessel / shared / spec / card / relay`濡??ъ젙??  - 鍮꾨ぉ?? inheritance / `super` / hidden callback graph瑜??⑦꽩 援ы쁽??湲곕낯媛믪쑝濡?梨꾪깮?섏? ?딆쓬
- [ ] **DND/campaign ?쒕굹由ъ삤瑜?寃뚯엫 ?꾨젅?꾩썙??寃利앹옣?쇰줈 ?ъ슜**
  - `dnd_tavern_campaign`瑜?湲곗??쇰줈 pool/fsm/strategy/table???ㅼ젣濡?異⑸텇?쒖? 寃利?  - language core 遺議깆씠 ?꾨땲??framework layer 遺議깆씤吏 怨꾩냽 遺꾨━?댁꽌 湲곕줉
  - 吏湲덇퉴吏 戮묓엺 ?ㅼ젣 ?⑦꽩:
    - ?μ냼/?λ㈃ 吏꾩엯 ?⑺넗由?(`OpenTavernCampaign`)
    - 寃뚯엫 ?곹깭 癒몄떊 (`tavern -> floor1 -> floor2 -> floor3 -> dragon -> epilogue`)
    - ?좏깮 ?댁꽍湲?(`scripted` / `random` / `player`)
    - ?λ㈃ 移대뱶 / ?숇즺 諛섏쓳 移대뱶 / 蹂댁뒪 ?섏씠利?移대뱶
    - ?꾪닾 loadout/strategy 移대뱶
    - transcript-first report writer
  - ?ㅼ쓬 紐⑺몴:
    - ???⑦꽩?ㅼ쓣 `examples/` ?꾩슜 肄붾뱶媛 ?꾨땲??`use` ?쇱씠釉뚮윭由??꾨낫濡??ш뎄??    - `world.pgy`??orchestration ?묒쓣 以꾩씠怨?encounter/strategy/report 怨꾩링?쇰줈 遺꾨━

## P1.8 ??硫???寃?
- [ ] **怨듯넻 UI IR 怨좎젙** ??Kotlin/Android 媛쒕퀎 諛깆뿏?쒕낫??癒쇱?, 紐⑤뱺 ?뚮옯?쇱씠 怨듭쑀?섎뒗 scene/projection UI IR???뺤쓽
  - 紐⑹쟻: native / web / mobile??媛숈? UI ?섎?濡좉낵 projection ?먮쫫??怨듭쑀?섍쾶 ??  - ?먯튃: 湲곗닠 湲곕컲? Qt 諛⑺뼢(native shell / render loop), ?좎뼵 泥좏븰? WPF??projection/binding, 理쒖쥌 ?뺤껜?깆? Pergyra scene/projection UI
  - 踰붿쐞: `Window`, `Scene`, `Node`, `Layout`, `DrawCommand`, `InputEvent`, `ProjectionBinding`, `DirtyScope`
  - ?먯튃: `subject`瑜?吏곸젒 ?붾㈃??洹몃━吏 ?딄퀬 `object` / `tobject` / projection surface瑜?UI ?뚮퉬 ?쒕㈃?쇰줈 ?ъ슜
  - ?먯튃: `zone` / `world` state? projection dirty sync媛 UI IR??媛깆떊 怨꾩빟????  - ?쒖꽌: UI IR 怨좎젙 ??native backend 1媛???JS/web backend 1媛???洹???mobile shell / Kotlin ?꾩슂???ы룊媛
  - 鍮꾨ぉ?? ?뚮옯?쇰퀎 UI ?섎?濡?Qt widget tree, WPF object model, Android View/Compose semantics)??肄붿뼱 ?몄뼱??吏곸젒 ?ㅼ씠吏 ?딆쓬
- [~] **JavaScript 諛깆뿏??* ??`.pgy ??JS` 蹂?섏쑝濡?釉뚮씪?곗?/Node.js ?ㅽ뻾 吏??  - ?꾨즺: 肄붿뼱 ?섎?濡좎? inheritance/super ?놁씠 ?좎??섍퀬, JS lowering? delegation/composition 以묒떖?쇰줈 媛꾨떎???뺤콉 珥덉븞 臾몄꽌??  - ?꾨즺: Kotlin backend蹂대떎 怨듯넻 UI IR???곗꽑?대씪??硫?고뵆?ロ뤌 ?뺤콉 臾몄꽌??  - ?⑥쓬: JS IR/lowering shape, runtime shim, interop surface (`extern js`) ?ㅺ퀎
- [ ] **mobile shell ?꾨왂** ??Android/iOS???곗꽑 怨듯넻 UI IR consumer濡??묎렐
  - ?먯튃: 珥덇린 mobile ??묒? JS/web-compatible UI backend ?먮뒗 native shell bridge瑜??곗꽑 寃??  - ?⑥쓬: Android ?꾩슜 Kotlin backend??怨듯넻 UI IR + web/native backend 寃利????꾩슂?깆쓣 ?ы룊媛
- [ ] **WebAssembly ?寃?* ??LLVM wasm32 backend ?쒖슜

## P1.9 ??AI-first ?명봽??(2026-04-19 positioning ?뺤젙)

**留λ씫**: 寃쎌웳 ??곸? C#/Java ??Rust ?ъ씠 ?덉튂?닿퀬, 1李??ъ슜?먮뒗 frontier LLM(Claude ????二쇰룄 + ?멸컙??由щ럭/?섏젙?섎뒗 ?뚰겕?뚮줈. "AI媛 ?앹꽦 ??而댄뙆?쇰윭/?뚯뒪?멸? 寃利????멸컙??由щ럭"??loop????댄듃?섍쾶 ?뚯븘媛??寃껋씠 positioning ?듭떖.

?꾩옱 ?섎룄移??딄쾶 媛뽰떠吏?AI-friendly ?명봽??
- backend-compare ?뚭? (C/LLVM 異쒕젰 ?議? ??AI self-verification loop ?섎꽕??- 2000+ test suite + ?ㅻえ??泥댁씤 ???앹꽦臾?利됱떆 寃利?媛?ν븳 洹쒕え
- Result-first + throw 湲덉? ??AI媛 stack trace蹂대떎 ErrorCode enum 遺꾧린媛 ?ъ?
- 援ъ“??二쇱꽍 (WHAT/WHY/ALT/NEXT/EFFECTS/INVARIANTS/RETURNS/THROWS) ??prompt-as-code, ?섎룄 蹂댁〈

遺議깊븯怨?梨꾩썙????寃?

- [ ] **Language Reference Spec 臾몄꽌** ???꾩옱 `docs/`???ㅺ퀎 ?쇱?(?섏궗寃곗젙 ?먮쫫 湲곕줉). AI?먭쾶 ?뺥솗???섎?濡??쒓났?섎젮硫?"???몄뼱??蹂댁옣"????臾몄꽌???뺣━?쇱빞 ??  - ?댁슜: ????쒖뒪??洹쒖튃 / Slot ?뚯쑀沅?怨꾩빟 / effect subsumption / intent rollback ?섎? / Result ?꾪뙆 洹쒖튃 / MIR 怨꾩빟
  - ?뺥깭: ?⑥씪 ?뚯씪 (~2000-5000以?, in-context濡???踰덉뿉 濡쒕뱶 媛??  - 紐⑹쟻: "Claude媛 Pergyra 肄붾뱶瑜????몄뀡?먯꽌 ?앹꽦????reference濡??몄슜 媛?? ?섏?
  - ?꾩옱 `docs/`? ?ㅻⅨ ?? ?쇱???"???대젃寃?寃곗젙?덈뒗媛", spec? "?꾩옱 ?몄뼱媛 臾댁뾿??蹂댁옣?섎뒗媛"
- [~] **AI-parseable 援ъ“???먮윭 硫붿떆吏** ???꾩옱 吏꾨떒? ?대????쒗쁽. AI?⑹? 湲곌퀎 ?먮룆 媛?ν븳 援ъ“???꾨뱶 ?꾩슂
  - ?꾩옱: `MIR contract breach in Main at line 0: unresolved identifier 'flag' (expected SSA-mapped local)`
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
  - ??? compile, semantic, MIR/LLVM IR ?④퀎 ?꾩껜
  - 1李?利앸텇 ?꾨즺 (2026-04-19):
    - `DriverFlags.diag_format` + `--error-format=json|text` CLI ?뚮옒洹?異붽? (`src/pgy_driver.c`, `src/compiler/driver_app.h`)
    - `semantic_result_print_json` ??semantic 吏꾨떒??JSON 諛곗뿴濡?諛⑹텧 (severity/stage/location/message ?꾨뱶, RFC 8259 以???댁뒪耳?댄봽)
    - `driver_emit_single_diag_json` ???⑥씪 ?먮윭 JSON 諛⑹텧 ?ы띁 (module_load / backend_c_emit / backend_c_native / backend_llvm_emit / backend_llvm_native ?④퀎 而ㅻ쾭)
    - stage ?쒓렇: `semantic` / `module_load` / `backend_c_emit` / `backend_c_native` / `backend_llvm_emit` / `backend_llvm_native`
    - ?깃났 ??`[]` (鍮?諛곗뿴), ?ㅽ뙣 ??`[{...}]` ???몄텧?먮뒗 ??긽 JSON 湲곕? 媛??    - ?뚭?: `tests/diagnostics_json_smoke.sh` (Python ?뚯꽌濡?shape 寃利? 3 耳?댁뒪: semantic / parse / success)
    - 寃利? PowerShell濡?3 耳?댁뒪 紐⑤몢 ?뺤긽 ?숈옉 ?뺤씤 (1668 semantic + 601 transpile ?뚭? pass)
  - 2李?利앸텇 ?꾨즺 (2026-04-19):
    - `Diagnostic` 援ъ“泥댁뿉 `code` ?꾨뱶 異붽? (non-owning `const char*`, ?뺤쟻 臾몄옄??由ы꽣??蹂닿?) ??`src/semantic/type_checker.h`
    - `semantic_error_code` / `semantic_warning_code` ?좉퇋 variant ??肄붾뱶 ?몄옄 諛쏆븘 diagnostic???ㅼ뼱以?(?덇굅??`semantic_error` ??洹몃?濡?NULL 肄붾뱶濡??숈옉, ???숈씪 ?ъ씠??以묐났 emit ??肄붾뱶媛 ?덉쑝硫??낃렇?덉씠??
    - JSON 異쒕젰??`"code"` ?꾨뱶 ?좏깮???ы븿 (NULL?대㈃ ?앸왂 ???명솚???좎?)
    - parser stage 遺꾨━: module_load msg媛 `"parse error in"`?쇰줈 ?쒖옉?섎㈃ `"stage":"parse"`, 洹???`"module_load"`
    - 珥덇린 肄붾뱶 遺???ъ씠??(6醫?:
      - `PGY_SEM_TYPE_MISMATCH` (assignment)
      - `PGY_SEM_BINOP_TYPE_MISMATCH`
      - `PGY_SEM_UNKNOWN_TYPE`
      - `PGY_SEM_UNDEFINED_SYMBOL` (identifier / member 3 ?ъ씠??
      - `PGY_SEM_INFER_COLLECTION` / `PGY_SEM_INFER_GENERIC` / `PGY_SEM_INFER_REQUIRED`
    - smoke test ?뺤옣: `code == "PGY_SEM_TYPE_MISMATCH"` 寃利?+ `stage == "parse"` 寃利?(`tests/diagnostics_json_smoke.sh`)
    - ?뚭?: 1688 semantic + 601 transpile, 0 failed
  - 3李?利앸텇 ?꾨즺 (2026-04-19):
    - Slot/ownership/parallel/effect 怨꾩뿴 肄붾뱶 9醫?異붽?:
      - `PGY_SEM_SLOT_RELEASED` (method dispatch 4 ?ъ씠??+ builtin Read/Write 2 ?ъ씠??
      - `PGY_SEM_RELEASE_REQUIRES_OWNER`
      - `PGY_SEM_SLOT_DOUBLE_RELEASE` (method + builtin Release 2 ?ъ씠??
      - `PGY_SEM_VIEW_KIND_MISMATCH` (ReadView write / WriteView read)
      - `PGY_SEM_MOVE_TOKEN_MISUSE` (read/write through MoveToken)
      - `PGY_SEM_MOVE_FROM_RELEASED` (let/call/builtin 3 ?ъ씠??
      - `PGY_SEM_PARALLEL_SLOT_CONFLICT` (error: mutate-mutate across tasks)
      - `PGY_SEM_PARALLEL_SLOT_RACE_RISK` (warning: read-mutate across tasks)
      - `PGY_SEM_EFFECT_CONFLICT` (warning: effect class 異⑸룎)
    - `docs/72_diagnostic_codes.md` 移댄깉濡쒓렇 臾몄꽌 ?좉퇋 ??16媛?肄붾뱶 ?섎?/?먯씤/援먯젙 諛⑸쾿, AI ?쇱슦??媛?대뱶, ?ν썑 ?뺤옣 ?꾨뱶 臾몄꽌??    - smoke test ?뺤옣: `PGY_SEM_SLOT_RELEASED` 媛먯? 耳?댁뒪 異붽?
    - ?ъ슜??湲곗뿬: `semantic_error_code` / `semantic_warning_code` ?좎뼵??`PGY_PRINTF_LIKE` ?띿꽦 異붽? (clang/gcc format 寃쎄퀬 泥댄겕)
    - ?뚭?: 1694 semantic + 601 transpile, 0 failed
    - ?꾩옱 珥?16媛??덉젙 肄붾뱶, ~25 ?ъ씠??而ㅻ쾭. ?섎㉧吏 ~460 ?ъ씠?몃뒗 4李? 利앸텇 ???  - 4李?利앸텇 ?꾨즺 (2026-04-19):
    - `CompilerResult.error_code` / `TranspileResult.error_code` / `LLVMGenResult.error_code` ?꾨뱶 異붽? (紐⑤몢 owning strdup, destroy?먯꽌 free)
    - `TranspilerCtx.backend_error_code` / `LLVMGenCtx.error_code` non-owning `const char *` (?뺤쟻 literal留?
    - ?좉퇋 setter variants: `transpiler_set_backend_error_with_code` / `llvm_set_error_with_code` / `llvm_set_error_at_with_code` (?덇굅??setter??code=NULL 寃쎈줈濡??좎?)
    - `driver_emit_single_diag_json_with_code(stage, code, message)` ??JSON??code ?꾨뱶 ?좏깮???ы븿
    - `driver_route_stage(default_stage, code)` ??prefix whitelist (`PGY_SEM_`/`PGY_MIR_`/`PGY_LLVM_`/`PGY_PARSE_`). 紐⑤Ⅴ??prefix??default_stage ?좎?
    - Runner ?낅뜲?댄듃: `c_runner.c` (2 ?ъ씠?? + `llvm_runner.c` (2 ?ъ씠?? ??湲곗〈 ?몄텧??`_with_code` + `driver_route_stage`濡?援먯껜
    - MIR/LLVM 肄붾뱶 5醫??좉퇋:
      - `PGY_MIR_UNRESOLVED_LOCAL` ??branch terminator??identifier媛 SSA 留ㅽ븨 ?놁쓬
      - `PGY_MIR_TOPOLOGY_INVALID` ??MIR routine ?꾨씫 / kind 遺덉씪移?/ AST ?놁쓬
      - `PGY_MIR_SIGNATURE_UNSUPPORTED` ??吏???덈릺???⑥닔 ?쒓렇?덉쿂
      - `PGY_MIR_SSA_LIMIT` ??SSA local 4096 珥덇낵
      - `PGY_MIR_INTENT_CARRIER_MISSING` ??intent step metadata ?꾨씫 (C/LLVM 怨듯넻, 21 ?ъ씠???쇨큵 ?낃렇?덉씠??
      - `PGY_LLVM_SPEC_LIMIT` ??Result\<T,E\> ?뱀닔???쒕룄(MAX_LLVM_RESULT_SPECS=32) 珥덇낵
    - 移댄깉濡쒓렇 ?뺤옣: `docs/72_diagnostic_codes.md`??"MIR Contract" ?뱀뀡 5媛??뷀듃由?+ "LLVM Backend" ?뱀뀡 1媛??뷀듃由?    - smoke test ?뺤옣: 33媛?Result\<Int, E*\> ?뱀닔?붾줈 `PGY_LLVM_SPEC_LIMIT` + `stage=llvm_codegen` 寃利?(`tests/diagnostics_json_smoke.sh`)
    - 寃利? `[{"severity":"error","stage":"llvm_codegen","code":"PGY_LLVM_SPEC_LIMIT",...}]` end-to-end ?뺤씤
    - ?뚭?: 1694 semantic + 601 transpile, 0 failed (?덇굅??寃쎈줈 臾댁넀??
    - ?꾩옱 珥?22媛??덉젙 肄붾뱶 (`PGY_SEM_*` 16 + `PGY_MIR_*` 5 + `PGY_LLVM_*` 1), ~50 ?ъ씠??而ㅻ쾭. `mir_validation` / `llvm_codegen` stage 媛 湲곗〈 `backend_*_native`? 遺꾨━??  - ?⑥? ?묒뾽 (5李?利앸텇 ?꾨낫):
    - intent/zone/world / class/ability 愿??`PGY_SEM_*` 肄붾뱶 ?먯쭊??遺??(?섎㉧吏 ~460 semantic ?ъ씠??
    - LLVM 異붽? 肄붾뱶: `PGY_LLVM_TYPE_UNSUPPORTED`, `PGY_LLVM_RUNTIME_MISSING`, `PGY_LLVM_OOM` (媛쒕퀎 ?ъ씠???낃렇?덉씠??
    - `cause_ir` / `fix_source` ?꾨뱶 ???꾩옱 message留? MIR/IR ?덈꺼 ?먯씤 + ?뚯뒪 ?덈꺼 援먯젙 ?ъ씤??遺꾨━??AI媛 援щ텇 媛?ν븯寃?    - parser ?덈꺼 肄붾뱶 (`PGY_PARSE_*` prefix ?덉빟?? ??parser error ?꾩쟻??由ы뙥???꾩슂
    - `related_rules` ?꾨뱶 ??Language Reference Spec ?댄썑 ?곌껐
- [ ] **In-context example corpus ?먮젅?댁뀡** ??GitHub??Pergyra 肄붾뱶 0媛? ?덈젴 ?곗씠??遺?щ? in-context examples濡?蹂댁셿
  - `docs/ai_prompt_bundle/` ?붾젆?좊━??紐?媛??덈꺼??踰덈뱾 以鍮?
    - `minimal.md` ???몄뼱 ?듭떖留?(~20KB)
    - `standard.md` ??core + stdlib + 5媛??⑦꽩 ?덉젣 (~100KB)
    - `complete.md` ????+ ?꾩껜 examples + reference spec (~500KB-1MB)
  - 媛?踰덈뱾? "??踰덈뱾留뚯쑝濡????몄뀡?먯꽌 AI媛 Pergyra 肄붾뱶瑜??좊ː???덇쾶 ?앹꽦 媛?ν븳媛"瑜?寃利?湲곗??쇰줈
  - ?꾨왂??寃곗젙: 1李?audience??frontier 紐⑤뜽(Claude Opus, Sonnet) ?ъ슜?? ?뚰삎/?媛 紐⑤뜽? 2李?- [ ] **AI iteration-friendly 鍮뚮뱶 ?댁껜??* ??鍮좊Ⅸ 而댄뙆??+ 湲곌퀎 ?먮룆 異쒕젰 + LSP 吏꾨떒
  - 利앸텇 而댄뙆?????꾩옱 ?⑥씪 TU濡??꾩껜 鍮뚮뱶. module ?⑥쐞 利앸텇?쇰줈 ?꾪솚
  - ?뚯뒪??寃곌낵 JSON 異쒕젰 ???꾩옱 stdout ?????뺤떇. AI媛 ?뚯떛???ㅼ쓬 ?≪뀡 寃곗젙?????덈뒗 JSON 紐⑤뱶
  - LSP 吏꾨떒 湲곌퀎 ?먮룆 媛?????꾩쓽 援ъ“???먮윭 硫붿떆吏? 怨듭쑀 ?щ㎎
  - backend-compare ?ㅽ뙣 ??diff瑜?援ъ“?????꾩옱 unified diff. AI媛 "?대뒓 ?⑥닔??紐?踰덉㎏ stdout ?쇱씤???ㅻ쫫"??諛붾줈 ?몄? 媛?ν븳 ?щ㎎
  - ?쇰? 湲곕컲 ?덉쓬 (`src/lsp/` ?붾젆?좊━, `tests/compare_backends.sh` 援ъ“)

**?깃났 湲곗?**: Frontier 紐⑤뜽??Pergyra spec bundle??in-context濡??ㅺ퀬, 鍮꾩옄紐낇븳 鍮꾩쫰?덉뒪 濡쒖쭅 (?? 寃곗젣 + 硫깅벑??+ ?ъ떆???뺤콉) 援ы쁽??one-shot??媛源앷쾶 ?앹꽦?????덉쓬. 而댄뙆???뚯뒪???ㅽ뙣 ??援ъ“???먮윭濡쒕????먭린 援먯젙 猷⑦봽媛 ~3???대궡 ?섎졃.

## P2 ??諛고룷 ?쒖옉 ??
- [ ] **臾몄꽌-援ы쁽 ?숆린??* ???뚯뒪????湲곕뒫 踰붿쐞 ?쇱튂
- [ ] **SBOM (SPDX) + provenance (SLSA)** ??怨듦툒留??щ챸??- [ ] **由대━???꾪떚?⑺듃** ???쒕챸??諛붿씠?덈━, 泥댄겕?? ?ㅼ튂 ?ㅽ겕由쏀듃
- [ ] **3rd-party NOTICE** ??OpenSSL/LLVM/pthread ?쇱씠?좎뒪 ?뺣━

## IR ?뚯씠?꾨씪???ш뎄??
- [x] **而댄뙆?쇰윭 怨꾩빟 怨좎젙** ??`HIR/DIR/RIR/MIR`, resource lattice, intent compensation, projection sync, authority/capability瑜?`docs/37_compiler_contracts.md`??怨좎젙

- [~] **DIR (Domain IR)** ??declaration graph / intent step graph ?쒖옉
  - ?꾨즺: `src/compiler/dir.h`, `src/compiler/dir.c`, `pgy --dir`, `test-dir`
  - ?꾨즺: intent participant/type edge, step zone/ability/authority/effect edge, step predecessor dependency
  - ?꾨즺: role/ability completeness edge, missing-ability-method edge
  - ?⑥쓬: richer zone/world membership graph
- [~] **RIR (Resource IR)** ??slot/resource/authority/lifecycle ?섎?濡??꾩슜 怨꾩링
  - 踰붿쐞: `Slot`, `SecureSlot`, `DeviceSlot`, projection validity, authority, effect/relation lifecycle, intent compensation resource edge
  - ?꾨즺: `src/compiler/rir.h`, `src/compiler/rir.c`, `pgy --rir`, `test-rir`
  - ?꾨즺: scope蹂?normalized state summary (`initial_state`, `final_state`, `last_op`, `transition error`)
  - ?꾨즺: relation/effect layer slot? world zone slot??resource fact濡?materialize
  - 異쒕젰: ?⑥닚 map???꾨땲??`Resource Graph + Transfer Ops + Static Ownership Facts`
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
- [~] **MIR (Machine / Execution IR)** ??CFG/SSA/liveness/optimization 怨꾩링
  - 踰붿쐞: basic block, explicit instruction, phi, liveness, CFG-dependent resource merge, dead code elimination
  - ?꾨즺: `src/compiler/mir.h`, `src/compiler/mir.c`, `pgy --mir`, `test-mir`
  - ?꾨즺: HIR CFG -> MIR block bridge
  - ?꾨즺: RIR op -> MIR instruction bridge
  - ?꾨즺: intent cleanup block skeleton
  - ?꾨즺: phi materialization + incoming predecessor value list
  - ?꾨즺: block-local SSA rename skeleton
  - ?꾨즺: intent cleanup successor edge skeleton
  - ?꾩슂: `RIR-flow` merge ?뺤콉
  - ?꾩슂: richer phi merge policy
  - ?꾩슂: cleanup / rollback / detach-invalidation edge 怨좊룄??## Progress Log ??2026-04-24 Parser/Lexer Diagnostic Routing

- ?꾨즺: parser/lexer diagnostic routing 1李?gate瑜??レ븯??
- 援ы쁽: `parser_error`??`PGY_PARSE_SYNTAX`, `parse:unexpected_token`, `check-syntax`瑜?`Code:` / `Reason:` / `Fix:` ?쒕㈃?쇰줈 異쒕젰?쒕떎.
- 援ы쁽: lexer error token? `PGY_LEX_INVALID_TOKEN`, `lex:invalid_token`, `remove-or-escape-character`瑜?媛숈? ?쒕㈃?쇰줈 異쒕젰?쒕떎.
- 寃利? `make parser-lexer-diagnostic-test-smoke`, `make diagnostic-registry-test-smoke`, `make test-parser`.
- ?⑥쓬: parse/lex diagnostics瑜?driver JSON diagnostic object濡?吏곸젒 ?섎━??refactor??蹂꾨룄 Tier 2 ?묒뾽?쇰줈 ?좎??쒕떎.

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
