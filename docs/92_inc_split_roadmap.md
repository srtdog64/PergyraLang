# `.inc` Split Roadmap

마지막 업데이트: 2026-04-24

`type_checker.c` 및 transpiler/LLVM의 일부 `.inc` 파일은 “모듈화”가 아니라 **파일이 분할된 단일 translation unit**에 가깝다. 이 문서는 P1 (TODO.md 폐인 포인트 보드) 항목을 단계적으로 닫기 위한 axis-by-axis 절단 계획.

---

## 현 상태 (2026-04-24)

`src/semantic/`:
- `.c` 45개, `.h` 24개, `.inc` **47개**
- `.inc` 개수는 split용 shim/include가 섞여 단순 감소 지표로 쓰지 않는다. 베타 debt 지표는 큰 `.inc` LOC, static cascade, direct include chain 축소로 본다.
- 분리된 internal header 16개 (`type_checker_*_internal.h`) — leaf/axis 인터페이스는 translation unit 경계로 이동 중

`type_checker.c` include chain (depth 4):
```
type_checker.c
├─ type_checker_helpers.inc → core / host / late
├─ type_checker_visibility.inc
├─ type_checker_resolution.inc → graph / stage
├─ type_checker_expr.inc → resolve
├─ type_checker_ownership_boundaries.inc
├─ type_checker_ownership_param_summary.inc
├─ type_checker_decls.inc → a / b (5 추가 .inc)
├─ type_checker_async_channel.h
├─ type_checker_program.inc
├─ type_checker_program.c
└─ type_checker_class_decl.c
```

가장 큰 .inc 파일 Top 5 (LOC, 2026-04-24 snapshot):
1. `type_checker_helpers_late.inc` — 773
2. `type_checker_builtins_query.inc` — 769
3. `type_checker_expr.inc` — 758
4. `type_checker_helpers_effects.inc` — 670
5. `type_checker_builtins_nominal.inc` — 660

Cross-subsystem `.inc` debt after Tier 1 split (LOC, 2026-04-24 snapshot):
- No `src/runtime`, `src/codegen`, or `src/compiler` `.inc` exceeds 1,000 LOC.
- `make backend-inc-size-test-smoke` enforces this gate.
- Current largest files:
  1. `codegen/transpiler_emitters_intent.inc` — 961
  2. `compiler/rir_public.inc` — 911
  3. `codegen/transpiler_expr_emitters_members.inc` — 900
  4. `codegen/transpiler_expr_emitters_call_b.inc` — 900
  5. `codegen/transpiler_expr_emitters_call_a.inc` — 900

Recently closed:
- `semantic/type_checker_ownership_return.inc`, `semantic/type_checker_ownership_assign.inc`,
  `semantic/type_checker_ownership_array_store.inc`, `semantic/type_checker_ownership_call.inc`,
  `semantic/type_checker_ownership_boundaries.inc`,
  `semantic/type_checker_ownership_destructure.inc`,
  `semantic/type_checker_ownership_destructure_stmt.inc`,
  `semantic/type_checker_ownership_let.inc`, `semantic/type_checker_ownership_let_boundary.inc`,
  `semantic/type_checker_ownership_let_claim.inc`, `semantic/type_checker_ownership_let_infer.inc`,
  `semantic/type_checker_ownership_let_slot.inc`, `semantic/type_checker_ownership_let_value.inc`,
  and `semantic/type_checker_ownership_param_summary.inc` — removed; ownership return,
  assignment-rebind, array-literal store, boundary validation, call-argument,
  destructuring, let-binding, and parameter escape-summary consumers now build as
  real translation units. There are no `type_checker_ownership_*.inc` files left.
- Current `.inc` contract: **0 `.inc` files / 0 LOC** under `src`, including
  test fixtures. Test case include fragments use `.cases.h`. Former runtime,
  codegen, compiler, and semantic production seams now live in named owner
  `.h` / `.c` files. Older bullets below remain as split history, not as the
  active target state.
- `semantic/type_checker_decls_domain_helpers.inc` — removed; body lives in `type_checker_decls_domain_helpers.c`.
- `semantic/type_checker_decls_a.inc` — reduced to a one-line shim; body lives in `type_checker_intent_helpers.c`.
- `codegen/transpiler_emitters_mir_inventory_ssa.inc` — reduced to a five-line shim that includes three sub-1000 LOC slices:
  `transpiler_emitters_mir_inventory_intent.inc`,
  `transpiler_emitters_mir_inventory_ssa_names.inc`,
  `transpiler_emitters_mir_inventory_ssa_emit.inc`.
- `codegen/transpiler_expr_emitters.inc` — removed as a pass-through shim; `transpiler.c` now includes the concrete emitter chunks directly:
  `transpiler_expr_emitters_builtins.inc`,
  `transpiler_expr_emitters_call_a.inc`,
  `transpiler_expr_emitters_call_b.inc`,
  `transpiler_expr_emitters_members.inc`,
  `transpiler_expr_emitters_tail.inc`.
- `codegen/llvm_expr_calls.inc` — reduced to a shim that includes constructor, array, collection-base, domain query, event invocation, intent observability, log, scalar math, result/option, slot/device-slot builtin, task/channel owners, plus four sub-1,000 LOC call slices:
  `llvm_expr_constructor_calls.h`,
  `llvm_expr_array_calls.h`,
  `llvm_expr_collection_base_calls.h`,
  `llvm_expr_domain_query_calls.h`,
  `llvm_expr_call_events.inc`,
  `llvm_expr_intent_observability_calls.h`,
  `llvm_expr_log_calls.h`,
  `llvm_expr_call_math.inc`,
  `llvm_expr_result_option_calls.h`,
  `llvm_expr_slot_device_calls.h`,
  `llvm_expr_task_channel_calls.h`,
  `llvm_expr_calls_part_a.inc`,
  `llvm_expr_calls_part_b.inc`,
  `llvm_expr_calls_part_c.inc`,
  `llvm_expr_calls_part_d.inc`.
- `codegen/transpiler_emitters_base_b.inc` — reduced to a six-line shim that includes four sub-1,000 LOC mechanical slices:
  `transpiler_mir_emit_state.h`,
  `transpiler_emitters_base_b_part_b.inc`,
  `transpiler_emitters_base_b_part_c.inc`,
  `transpiler_intent_zone_binding_emit.h`.
- Tier 1 runtime/codegen/compiler `.inc` gate — closed by safe mechanical split that avoids block-comment and continuation-line boundaries:
  `runtime/pgy_runtime_part_ba.inc`,
  `runtime/pgy_runtime_lib_part_b_part_a.inc` through
  `runtime/pgy_runtime_lib_part_b_part_f.inc`,
  `codegen/transpiler_emitters_base_a.inc`,
  `codegen/transpiler_helpers_core_a.inc`,
  `codegen/transpiler_helpers_core_b.inc`,
  `codegen/transpiler_domain_role.inc`,
  `codegen/llvm_expr_helpers_part_a.inc` through
  `codegen/llvm_expr_identifier_slot_helpers.h`,
  `compiler/mir_lower_public_api.h` / `compiler/mir_public_surface.h`,
  `codegen/llvm_expr_call_methods_part_a.inc` /
  `codegen/llvm_member_call_emit.h`,
  `codegen/llvm_domain_helpers_part_a.inc` /
  `codegen/llvm_domain_projection_sync_helpers.h`.
- `semantic/type_checker_helpers_late.c` — standalone TU hidden include-order dependency fixed by promoting call-path helpers to `type_checker_internal.h` and explicitly including slot analyzer / visibility / generic diagnostic contracts.

---

## 진행 sprint — 2026-04-24 semantic 템플릿 적용

분리 작업이 동일 템플릿 반복으로 수렴하도록 [docs/101_semantic_split_template.md](101_semantic_split_template.md) 를 source-of-truth 로 도입. 템플릿 A (body-only TU), 템플릿 B (DAG precollect 포함).

완료 slice:

| slice | 대상 | 새 TU | 구 `.inc` 폐기 | LOC |
|---|---|---|---|---|
| 1 차 | relation | `src/semantic/type_checker_relation_decl.c` | `type_checker_decls_relation.inc` | 94 |
| 1 차 | effect | `src/semantic/type_checker_effect_decl.c` | `type_checker_decls_effect.inc` | 66 |
| 2 차 | zone | `src/semantic/type_checker_zone_decl.c` | `type_checker_decls_zone.inc` | 1076 |
| 3 차 | ability | `src/semantic/type_checker_ability_decl.c` | (type_checker.c 에서 본체 추출) | 80 |
| 4 차 | world | `src/semantic/type_checker_world_decl.c` | `type_checker_decls_world.inc` | 856 |
| 4-B | intent | `src/semantic/type_checker_intent_decl.c` + `type_checker_intent_helpers_internal.h` | `type_checker_decls_intent.inc` + `type_checker_decls_intent_world.inc` | 919 |
| 3-B | role | `src/semantic/type_checker_role_decl.c` + `type_checker_decls_a_helpers_internal.h` | (decls_a.inc 에서 body 추출) | 383 |
| 3-B | party | `src/semantic/type_checker_party_decl.c` | (decls_a.inc 에서 body 추출) | 147 |
| 3-B | roster | `src/semantic/type_checker_roster_decl.c` | (decls_a.inc 에서 body 추출) | 80 |
| 4-C | class/extern | `src/semantic/type_checker_class_decl.c` | (`type_checker_program.inc` 에서 body 추출) | 215 |
| 4-D | program orchestration | `src/semantic/type_checker_program.c` | (`type_checker_program.inc` 에서 body 추출) | 381 |
| 4-E | type-resolution stage | `src/semantic/type_checker_resolution_stage.c` | `type_checker_resolution_stage.inc` | 999 |
| 4-F | builtin projection | `src/semantic/type_checker_builtins_projection.c` | (`type_checker_builtins_nominal.inc` 에서 body 추출) | 167 |
| 4-G | expression operators/index access | `src/semantic/type_checker_expr_ops.c` | (`type_checker_expr.inc` 에서 body 추출) | 141 |
| 4-H | expression name helpers | `src/semantic/type_checker_expr_names.c` | (`type_checker_helpers_late.inc` 에서 helper 추출) | 73 |

**3-B 에서 추가로 externalize 된 5 helper**: `any_subject_role_has_ability`, `any_subject_role_find_base_ability_impl`, `validate_ability_require_fields_for_role`, `find_generic_param_index`, `concrete_type_satisfies_bound`. `static` 제거 site 7 개 + 선언 승격 (`type_checker_decls_a_helpers_internal.h` 신설 + `type_checker_internal.h` 확장).

**4-B / 3-B 에서 확립된 helper externalization 패턴**: body 를 TU 로 추출하기 전에 해당 body 가 호출하던 static 함수를 (1) internal header 에 선언 추가, (2) 원 정의의 `static` 제거 로 외부 linkage 로 전환한다. 정의 위치는 그대로 유지. 상세: [docs/101_semantic_split_template.md](101_semantic_split_template.md) §8.

**빌드 가드 추가**: 4-B 과정에서 포인터 반환 helper의 implicit declaration 이 런타임 세그폴트로 이어질 수 있음을 확인했다. 기본 `CFLAGS`에 `-Werror=implicit-function-declaration -Werror=implicit-int`를 추가해, 이후 `.inc` → `.c` 분리 중 숨은 helper 의존이 테스트 실행 전 컴파일 단계에서 실패하도록 고정했다.

**DAG zone slice**: zone refresh projection field-map edge collector를 `src/semantic/type_checker_resolution_graph_zone.c`로 이동했다. 이후 graph inventory body는 `src/semantic/type_checker_resolution_graph_inventory.c`로 승격되어 include-order debt에서 빠졌다.

**Stage domain slice**: world/zone local-contract replay를 `src/semantic/type_checker_resolution_stage_domain.c`로 이동했다. `type_checker_resolution_stage.inc`는 969 LOC가 되었고, 남은 stage debt는 top-level declaration staging과 generic/function/event staging 쪽으로 압축됐다.

**Stage TU closure**: `type_checker_resolution_stage.inc`를 제거하고 `src/semantic/type_checker_resolution_stage.c`로 승격했다. top-level declaration staging, generic/function/event signature staging, graph host lookup이 실제 TU에서 빌드되며, `type_checker_resolution.inc`는 graph include shim만 남았다.

**Program declaration/orchestration slice**: class/extern declaration checking을 `src/semantic/type_checker_class_decl.c`로, top-level program orchestration을 `src/semantic/type_checker_program.c`로 이동했다. `nominal_flavor_from_decl`, `declared_effects_from_function_node`, graph topo/validation, program precollect/worklist, resolver stats는 include-order static helper에서 internal API로 승격했다. `type_checker_program.inc`는 624 LOC가 되어 semantic 800 LOC stop condition 아래로 내려갔다.

**Builtin projection slice**: `ToObject` / `ToTObject` semantic checker를 `src/semantic/type_checker_builtins_projection.c`로 이동했다. `type_checker_builtins_nominal.inc`는 659 LOC가 되어 semantic 800 LOC stop condition 아래로 내려갔고, projection diagnostics helper boundary가 builtin dispatcher에서 분리됐다.

**Expression operator/name slices**: binary/unary operator checker, array literal/indexed access checker를 `src/semantic/type_checker_expr_ops.c`로 이동했다. static member path flattening과 consumed-boundary identifier lookup은 `src/semantic/type_checker_expr_names.c`로 이동했다. `type_checker_expr.inc`는 758 LOC, `type_checker_helpers_late.inc`는 773 LOC가 되어 semantic 800 LOC stop condition 아래로 내려갔다.

**Ownership consumer slices**: return, assignment rebind, array literal store, boundary validation, call-argument, destructuring, let-binding, and parameter escape-summary checks now live in
`src/semantic/type_checker_ownership_return.c`,
`src/semantic/type_checker_ownership_assign.c`,
`src/semantic/type_checker_ownership_array_store.c`,
`src/semantic/type_checker_ownership_boundaries.c`,
`src/semantic/type_checker_ownership_call.c`,
`src/semantic/type_checker_ownership_destructure.c`,
`src/semantic/type_checker_ownership_let.c`, and
`src/semantic/type_checker_ownership_param_summary.c`. The old behavior-owning `.inc`
files were deleted. Current `src/semantic/**/*.inc` total is 8,215 LOC, and
`src/semantic/type_checker_ownership_*.inc` is zero.

남은 semantic 800+ `.inc`:

- 없음. `make semantic-inc-size-test-smoke`가 `src/semantic/**/*.inc <= 800 LOC`를 강제한다.

§1 체크리스트의 "800 LOC 초과 `.inc` 없음" 조건은 닫혔다. 모든 declaration body 는 kind/helper 별 TU 로 승격되어 owner boundary 가 명확해졌고, 새 declaration kind 추가 시 `.inc` aggregator 를 건드릴 필요 없음 — 사용자가 말한 "다른 지향 쉽게 추가" 의 구체적 장치 확보.

Backend 진행:
- `transpiler_emitters_mir_inventory_ssa.inc`는 1,998 LOC 단일 include에서 5 LOC shim + 760/626/610 LOC 하위 slice로 분리됐다.
- `transpiler_expr_emitters.inc` pass-through shim은 제거됐고, `transpiler.c`가 concrete emitter chunks를 직접 include한다. 남은 과제는 call/member/builtin owner 경계를 실제 `.c` 또는 더 작은 feature include로 정리하는 것이다.
- `llvm_expr_calls.inc`는 3,129 LOC 단일 include에서 17 LOC shim + feature owners + 51/387/380/360 LOC 하위 slice로 분리됐다. `llvm_emit_call` 앞단의 enum/class constructor lowering은 `llvm_expr_call_constructors.inc`로, `ArrayLength` / `ArrayPush` / `ArraySet` / `ArrayPop`은 `llvm_expr_call_arrays.inc`로, `ListNew` / `SetNew` / `SetAdd` / `SetHas` / `SetRemove` / `SetSize`는 `llvm_expr_call_collections_base.inc`로, `HasProjection` / `HasLayer` / `HasState` / `HasZone*` lowering은 `llvm_expr_call_domain_queries.inc`로, event invocation은 `llvm_expr_call_events.inc`로, `IntentLast*` / `IntentHistory*` / `IntentActive*` / `IntentRecent*` observability runtime calls는 `llvm_expr_call_intent_observability.inc`로, `Log*`는 `llvm_expr_call_log.inc`로, `Abs` / `Min` / `Max`는 `llvm_expr_call_math.inc`로, `Ok` / `Err` / `IsOk` / `IsErr` / `Unwrap` / `UnwrapOr` / `Some` / `None` / `IsSome` / `IsNone` / `UnwrapOption` lowering은 `llvm_expr_call_result_option.inc`로, `ClaimSlot` / `Write` / `Read` / `Release` / `Device*` lowering은 `llvm_expr_call_slots.inc`로, task cancellation/channel operations는 `llvm_expr_call_task_channel.inc`로 이동했다. 남은 dispatcher debt는 list/map/queue continuation 일부, stdlib IO/file/time, host/user function fallback owner 추출이다.
- `transpiler_emitters_base_b.inc`는 3,068 LOC 단일 include에서 6 LOC shim + 800/800/800/668 LOC 하위 slice로 분리됐다. `emit_statement`, `emit_block`, intent forward declaration family는 아직 include-order preserved 상태라서 다음 실제 owner extraction 후보로 남는다.
- Tier 1 remaining split은 runtime/codegen/compiler 전체 1,000 LOC gate를 닫는 데 집중했다. Pass-through shim `.inc` files for runtime part B, LLVM expr helpers, LLVM method calls, LLVM domain helpers, MIR public API, and C transpiler emitter/helper seams have now been removed; owning `.c` / `.h` files include concrete sub-1,000 LOC chunks directly.
- 검증: `make backend-inc-size-test-smoke`, `make test-mir test-transpile test-abi -j2`, `make test-semantic -j2`, `make llvm-test-backend-compare -j2`.

---

## 장기 목표선 — 어디까지 모듈화해야 충분한가

목표는 `.inc` 개수를 0으로 만드는 것이 아니라, **언어 축별 owner와 dependency direction이 명확한 translation unit 구조**를 만드는 것이다. 베타 이후에도 기능을 계속 추가하려면 최소한 다음 상태까지 가야 한다.

### Target State A — Semantic Core

- 2026-04-25 DAG metadata slice: `type_checker_resolution_graph_core.c` records context-independent builtin type refs into `SemanticContext.type_resolution_metadata`, and owner resolver seams query that metadata before recursive fallback.
- 2026-04-25 stable constructed metadata slice: graph metadata can materialize `List<T>`, `Set<T>`, `HashMap<String|Int, T>`, `Option<T>`, and `Result<T,E>` when argument facts are present. Owned constructed `Type` shells are freed with the semantic context.
- 2026-04-25 tuple/function metadata slice: graph collect now records tuple shells, event-handler/function shells, and channel/future constructed shells when all child facts are already available.
- `resolve_generic_type_arg(...)` now queries graph metadata before recursive fallback, so constructed builtin and generic consumer paths share the same DAG metadata seam.
- `make type-resolution-dag-test-smoke` now fails if graph-backed skips, metadata entries, metadata owned entries, metadata hits, zero non-alias stage compatibility fallback, or alias-stage split accounting regress. This keeps the DAG migration honest: graph inventory must produce reusable materialized facts, not just skip compatibility staging.
- `type_checker.c`는 orchestration만 담당한다.
- `type_checker_resolution_graph_inventory.c`가 graph inventory pass를 소유한다.
- `type_checker_resolution_stage.c`는 DAG stage source of truth로 유지하고 include shim으로 되돌리지 않는다.
- DAG stage 안의 retired resolver compatibility surface는 숨기지 않는다. `PGY_TYPE_RES_STATS=1`의 `stage-graph-backed` / `stage-compat-resolve` / `stage-compat-family` / `stage-alias` 통계와 `make type-resolution-dag-test-smoke`가 남은 migration debt를 공개 지표로 고정한다.
- 현재 stage compatibility surface는 alias-only diagnostic inventory로 고정됐고 최신 local stats는 `compat_alias=83 compat_non_alias=0 alias_materialized=5 alias_diagnostic_fallback=78 alias_fallback_resolved=0 alias_fallback_unresolved=78`이다. Valid alias fallback은 0으로 gate되며, unresolved fallback은 alias-cycle diagnostic coverage 경로다.
- Program-level symbol inventory now predeclares ability declarations, and the ability checker reuses only its own predeclare. This closes the provider-after-consumer order gap for a frozen DAG slice covering generic default/where, zone authority, and party role-slot ability consumers.
- `type_checker_resolution_stage_lookup.c`는 stage lookup/host-label mapping을 소유하고, `type_checker_resolution_stage_stats.c`는 graph-backed skip 판정과 compatibility-family 계측을 소유한다.
- graph precollect TU는 stage runner를 호출하지 않는다. enum method inventory도 precollect action contract 경로로만 edge를 만든다.
- `type_checker_generic_validation.c`는 generic where/default validation을 소유한다. graph core/precollect layer는 `resolve_type_node(...)`를 직접 호출하지 않는 resolver-free inventory/graph primitive layer로 고정하고, `semantic-core-shape-test-smoke`가 이를 검사한다.
- `type_checker_intent_decl.c`의 participant/value/where type materialization은 local seam 3개로 수렴했고, 이제 graph-backed resolved metadata를 먼저 조회한 뒤 recursive fallback으로 내려간다.
- `type_checker_decls_domain_helpers.c`의 projection/relation/effect contract type materialization은 slot/shared/named-ref seam 3개로 수렴했고, domain contract checks는 graph-backed resolved metadata를 먼저 재사용한다. Zone authority participant resolution also now treats exact/qualified-tail direct slot aliases as concrete before same-type ambiguity, and clears stale ambiguity when returning a direct match.
- `type_checker_intent_helpers.c`의 transfer-derived using/where, ability generic arg, role-field checks는 `intent_helper_resolve_type_ref(...)` 단일 seam으로 수렴했다. 다음 DAG slice는 이 seam을 graph-backed metadata reader로 교체하는 것이다.
- `type_checker_host_helpers.h`의 projection source field, hosted method return/param, zone authority/domain slot checks는 `host_helper_resolve_type_ref(...)` 단일 seam으로 수렴했다. 다음 DAG slice는 host helper header의 마지막 resolver seam을 `.c` owner로 추출한 뒤 graph-backed metadata reader로 교체하는 것이다.
- `type_checker_program.c` / `type_checker_program.inc`의 declaration/body type materialization은 quiet/body resolver seam으로 수렴했다. function body materialization은 graph-backed resolved metadata를 먼저 재사용한다.
- `type_checker_event.c`의 event declaration/subscription signature materialization은 `semantic_event_resolve_type_ref(...)` 단일 seam으로 수렴했다. 다음 DAG slice는 event signature metadata reader로 교체하는 것이다.
- `type_checker_world_decl.c`의 shared field/domain slot materialization은 `world_resolve_type_ref(...)` / `world_resolve_domain_slot_type(...)` seam으로 수렴했다. 다음 DAG slice는 world shared/slot checks가 graph-backed resolved metadata를 재사용하게 만드는 것이다.
- `type_checker_role_decl.c`, `type_checker_generic_contracts.h`, `type_checker_helpers_late.c`, `type_checker_expr.inc`는 각각 local resolver seam 1개로 수렴했다. 다음 DAG slice는 role include/impl, generic default/bound, call default, lambda/member metadata를 graph-backed result로 교체하는 것이다.
- `type_checker_generic_validation.c`, `type_checker_ability_where.c`, `type_checker_module_contract.c`, `type_checker_ability_decl.c`, `type_checker_class_decl.c`, `type_checker_operator_expr.h`, `type_checker_ownership_destructure_stmt.inc`도 local resolver seam으로 수렴했다. 다음 DAG slice는 이 seam들을 graph-backed metadata reader로 교체하고 remaining direct count를 implementation/comment/seam만 남기는 것이다.
- statement/type-alias, ability fields, projection/query builtins, flow with-slot, generic support, helper effects, ownership let, party/roster/zone single-call resolver paths도 local seam으로 수렴했다. zone domain-slot seam은 graph metadata-first 조회를 사용하며, `type-resolution-resolver-inventory-test-smoke`가 새 direct resolver 호출을 allowlist 밖에서 금지한다.
- declaration validators는 `subject/class`, `zone`, `world`, `intent`, `relation/effect/projection`, `ability/role/party/roster` 단위의 `.c`로 분리한다.
- `find_*_decl`, `find_*_slot`, label/format, dependency record API는 static include-order가 아니라 internal header 계약으로만 사용한다.
- type-resolution DAG는 recursive resolver의 보조 자료가 아니라 provider/consumer validation schedule의 source of truth가 된다.

Semantic stop condition:
- `src/semantic`에는 800 LOC 초과 `.inc`가 없다.
- Stricter beta rule: behavior-owning `.inc` files are blockers, not beta+1 cleanup.
  Generated tables and local macro tables must use named `.h` / `.c` owner
  files, and test fragments must use `.cases.h`.
- TU mixing rule: moving `.inc` code into `.c` must not create a new mega-TU.
  New semantic owner TUs should stay under 1,000 LOC. Existing oversized TUs are
  capped by `make semantic-tu-size-test-smoke` and must shrink by owner axis
  instead of growing.
- Closed risky seam: the former `type_checker_builtins_query.inc` body now
  lives in `type_checker_builtins_query.h`, and
  `type_checker_builtins_slotops.h` owns the complete
  `BuiltinKind builtin_resolve(...)` signature. The remaining semantic builtin
  cleanup target is nominal/slotops ownership, not a cross-file dangling
  return-type boundary.
- `type_checker.c`는 600 LOC 이하이며 include aggregator가 아니다.
- 현재 상태: `type_checker_event.c`와 `type_checker_qubit.c` owner 추출 후 `type_checker.c`는 481 LOC다. 남은 include는 helper shims와 statement/program orchestration 경계다.
- 현재 상태: DAG graph stats, graph-backed stage skip, stage compatibility fallback inventory는 `type-resolution-dag-test-smoke`로 CI에 연결됐다. named type-ref는 generic argument를 포함해 graph-backed skip 경로로 들어가며, smoke는 skip 합계가 0으로 퇴행하면 실패한다. 다음 closure slice는 generic/default/bound validation 자체와 nested consumer metadata를 graph-backed result로 재사용해 compatibility 호출량을 더 줄이는 것이다.
- semantic 신규 기능은 `.inc` 수정 없이 해당 axis `.c`와 internal header만 수정해서 추가 가능해야 한다.

### Target State B — Backend Emitters

- C transpiler와 LLVM은 각각 expression/statement/declaration/domain/runtime-call emitter를 실제 `.c` 단위로 소유한다.
- `llvm_expr_calls.inc` and the concrete transpiler/LLVM expr helper chunks should continue shrinking toward 500-800 LOC feature modules; pass-through shim files should not be reintroduced.
- backend declaration inventory는 AST-carried helper에 직접 기대지 않고 MIR/DIR/RIR metadata reader API를 통한다.
- C/LLVM 공통 ABI naming, projection labels, runtime symbol lookup은 중복 helper가 아니라 공유 contract module에서 읽는다.

Backend stop condition:
- `src/codegen`에는 1,000 LOC 초과 `.inc`가 없다.
- C/LLVM parity test에 새 frozen surface를 추가할 때 emitter helper를 두 곳에 복붙하지 않는다.
- declaration emit path에서 raw AST fallback은 hard error 또는 dedicated inventory reader로만 흐른다.

### Target State C — Runtime / ABI

- `pgy_runtime_part_*.inc`와 `pgy_runtime_lib_part_*.inc`는 domain별 `.c`로 분리한다: slot/resource, intent observability, zone/world frontier, projection propagation, authority failure, async/parallel.
- runtime public ABI와 internal scheduler/storage helper를 같은 파일에 섞지 않는다.
- recoverable failure state, hard-fail invariant, queryable observability state를 파일 경계에서도 분리한다.

Runtime stop condition:
- `src/runtime`에는 1,000 LOC 초과 `.inc`가 없다.
- ABI struct/header 변경은 `pgy_abi_spec.h`와 해당 runtime `.c`만 보면 audit 가능하다.
- world/zone/projection propagation의 bounded scheduler는 single source of truth로 존재하고 C/LLVM generated calls가 같은 runtime entrypoint를 호출한다.

### Target State D — Tests

- 대형 semantic test `.inc`도 axis별 executable 또는 fixture module로 분리한다.
- `test_semantic.c`가 10개 이상의 대형 `.inc`를 include하는 구조는 장기적으로 유지하지 않는다.
- beta frozen subset은 semantic/unit, C smoke, LLVM smoke, backend compare에 같은 case id로 연결한다.

Test stop condition:
- 1,500 LOC 초과 test `.inc`가 없다.
- 실패 로그에서 case id만으로 feature axis와 backend parity 여부를 추적할 수 있다.

### Target State E — Speed / Build Performance

- 장기 모듈화는 include-order debt를 줄이되, frontend/backend latency를 숨은 비용으로 늘리면 안 된다.
- `make test-abi-perf`는 beta frozen ABI/runtime surface의 benchmark-only baseline으로 유지한다.
- `tests/perf_summary.sh <log>` 또는 `make perf-summary PERF_LOG=<log>`로 C/LLVM compile/run 평균과 worst-case를 요약한다.
- representative backend benchmark는 `tests/bench_backend.sh <source.pgy> dev`로 C/LLVM wall time과 peak RSS를 같이 본다.
- generated/native compile warning은 performance noise로 취급하지 않고 build hygiene bug로 즉시 닫는다.

Speed stop condition:
- `test-abi-perf`는 `320 passed, 0 failed` 기준을 유지한다.
- 대표 backend compare case의 generated compile warning은 0개여야 한다. 소스 semantic warning은 expected case일 때만 허용한다.
- 모듈화 slice 후 `perf_summary` worst-case compile time이 이전 baseline 대비 2배 이상 튀면 해당 slice를 성능 회귀 후보로 기록한다.
- CI hard bound는 `test_abi_pipeline`의 latency floor를 사용하고, 로컬 benchmark는 비교용 수치로만 다룬다.

---

## 장기 단계화

1. Beta closure phase:
   - semantic DAG inventory/stage 본체를 먼저 줄인다.
   - declaration-side MIR inventory bootstrap debt와 runtime propagation blocker만 건드린다.
   - codegen/runtime 대형 split은 parity를 깨지 않는 leaf helper 위주로 제한한다.

2. Beta+1 architecture phase:
   - C/LLVM emitter include tree를 실제 TU로 전환한다.
   - runtime propagation/observability/failure를 domain별 `.c`로 분리한다.
   - test include bundle을 feature fixture runner로 바꾼다.
   - perf summary baseline을 release artifact에 첨부할 수 있게 자동화한다.

3. v1 readiness phase:
   - `src` tree 안에는 `.inc`를 남기지 않는다. generated table, local macro table, private test fixture는 named `.h` / `.c` owner 또는 `.cases.h` test fragment로 둔다.
   - 모든 core language axis는 owner module, internal header, regression matrix, docs entry를 가진다.

---

## 절단의 어려움 — Static Cascade

단순히 `.inc` 하나를 `.c` 로 옮기는 것은 위험하다. 예: ownership classifier (`semantic_classify_ownership_type`)를 `type_checker_ownership_boundaries.inc` 에서 새 `.c` 로 옮기려면:

```
semantic_classify_ownership_type
└─ calls type_is_subject_type   ← static in type_checker_host_helpers.h
   └─ helper에서 caller로 cascade 필요
```

`type_is_subject_type`은 `type_checker_host_helpers.h` 에 **static**으로 존재. .c 분리 시 cascade promote 필요.

**경험칙**: 한 axis 절단은 평균 3~5개의 cascade promote를 동반.

---

## Axis 절단 우선순위

축 단위로 `.c+.h` 절단. 의존성이 가장 낮은 → 높은 순:

### 1. **diagnostic helpers** (P0 — 다음 sprint 시작점)
- 대상: `type_checker_context_helpers.h` (emit_diagnostic_full 등)
- 종속: `Type`, `SemanticContext`, `ASTNode` 만 사용 — leaf
- 출력: `type_checker_diag.c`; public declarations remain in `type_checker.h` / `diag_payload.h`
- 상태: DONE — `type_checker_context_helpers.h`에서 diagnostic snapshot/emission/printing 243 LOC 제거
- 검증: `make test-semantic`, `make test-all`

### 2. **ownership classifier + labels**
- 대상: `type_checker_ownership_boundaries.inc:14-82` (5개 함수)
- 종속: `type_is_*` predicate 4개 (그 중 `type_is_subject_type`이 static)
- 출력: `type_checker_ownership_classify.c` + 기존 `type_checker_ownership_internal.h` 갱신
- 상태: DONE — classifier/value/provenance/replacement labels를 별도 TU로 이동
- cascade: `type_is_subject_type` promoted to extern in `type_checker_internal.h`
- 검증: `make test-semantic`

### 3. **channel transport validator**
- 대상: `type_checker_async_channel.h:11-217` (validator + reporters)
- 종속: ownership classifier (위 axis 2 선행 필요), `OwnershipConsumerKind`
- 출력: `type_checker_channel_transport.c` + 기존 `type_checker_channel_transport_internal.h` 갱신
- 상태: DONE — borrowed transfer, named-binding transfer, transport mismatch/policy reporters를 별도 TU로 이동
- include hygiene: `type_checker_builtins.c` 수동 forward declaration 제거, channel header의 ownership diagnostic include 제거
- 검증: `make test-semantic`

### 4. **generic contract diagnostics**
- 대상: `type_checker_helpers_late.inc:40-79` 외 generic mismatch helpers
- 종속: `Type`, `SemanticContext`, `GenericParams`, ability metadata
- 출력: `type_checker_generic_diag.c` + 기존 `type_checker_generic_diag_internal.h` 갱신
- 상태: DONE — function/class/ability generic bound failure renderers를 별도 TU로 이동
- 검증: `make test-semantic`

### 5. **ownership consumers (escape diagnostics)**
- 대상: `type_checker_ownership_diag_internal.h` 기반 helper family (10-param 진단)
- 종속: ownership classifier, `OwnershipConsumerKind`
- 출력: `type_checker_ownership_diag.c`
- 상태: DONE — ownership escape diagnostic renderer/helper family를 별도 TU로 이동
- 결과: `type_checker_ownership_boundaries.inc`는 validation switch + ownership let/return includes 중심으로 축소
- 검증: `make test-semantic`

### 6. **module contract / authority consumer**
- 대상: `type_checker_module_contracts.inc` 일부
- 종속: ability metadata, generic params
- 출력: `type_checker_module_contract.c`
- 선행 seam: DONE — ability reference display/name/signature helpers를 `type_checker_ability_ref.c`로 이동
- 선행 seam: DONE — stdlib use validation을 `type_checker_stdlib_use.c`로 이동
- 선행 seam: DONE — subject ability mismatch diagnostic을 `type_checker_module_contract_diag.c`로 이동
- 선행 seam: DONE — ability `fields` validator를 `type_checker_ability_fields.c`로 이동
- 선행 seam: DONE — ability ref matching / role ability lookup / subject ability lookup을 `type_checker_ability_match.c`로 이동
- 선행 seam: DONE — ability where-bound consumer validation을 `type_checker_ability_where.c`로 이동
- 본체: DONE — required ability resolver와 action required-ability validator를 `type_checker_module_contract.c`로 이동
- cascade: `find_type_decl_by_name`를 `type_checker_internal.h` internal API로 승격
- cascade: `find_ability_decl_by_name`와 `collect_effective_generic_arg_nodes`를 internal API로 승격
- cascade: `generic_params_required_count`를 internal API로 승격
- cascade: `format_type_constraint_bounds`와 `semantic_type_resolution_record_type_ref_dependency`를 internal API로 승격
- 후속: `semantic_type_resolution_record_type_ref_dependency`는 graph core TU로 이동 완료
- 결과: `type_checker_module_contracts.inc` 제거
- 남음: authority 의미론 자체는 베타 보드에서 계속 관리하지만, module contract include-order 구조 debt는 닫힘
- 예상 cascade: 5+ (가장 무거움)

### 7. **resolution graph / stage** (마지막 — 가장 큰 DAG 축)
- 대상: `type_checker_resolution_graph_inventory.c`, `type_checker_resolution_stage.c`
- 종속: 거의 모든 type/context 인프라
- 선행 seam: DONE — type constraint bound formatter를 `type_checker_type_constraint.c`로 이동
- 선행 seam: DONE — graph node/edge/path/cycle-format primitive를 `type_checker_resolution_graph_core.c`로 이동
- 선행 seam: DONE — named dependency edge recorder와 즉시 cycle diagnostic 발행 경로를 `type_checker_resolution_graph_core.c`로 이동
- 선행 seam: DONE — type-ref dependency recorder를 `type_checker_resolution_graph_core.c`로 이동
- 선행 seam: DONE — type-ref collector를 `type_checker_resolution_graph_collect.c`로 이동
- 선행 seam: DONE — generic contract inventory / string dependency / required ability collector helpers를 `type_checker_resolution_graph_collect.c`로 이동
- 선행 seam: DONE — top-level declaration graph registration을 `type_checker_resolution_graph_collect.c`로 이동
- 선행 seam: DONE — local-contract graph node/dependency + zone/world/projection label formatters를 `type_checker_resolution_graph_labels.c`로 이동
- 선행 seam: DONE — event declaration precollector를 `type_checker_resolution_graph_decl.c`로 이동
- 선행 seam: DONE — enum declaration precollector를 `type_checker_resolution_graph_decl.c`로 이동하고 `semantic_stage_method_array`를 internal API로 승격
- 선행 seam: DONE — ability declaration precollector와 action-contract precollector를 `type_checker_resolution_graph_decl.c`로 이동
- 선행 seam: DONE — role/class/party/roster declaration precollector를 `type_checker_resolution_graph_decl.c`로 이동
- 선행 seam: DONE — projection source resolver를 `type_checker_resolution_graph_domain.c`로 이동하고 `find_zone_domain_slot`을 internal API로 승격
- 선행 seam: DONE — relation/effect domain inventory precollector를 `type_checker_resolution_graph_domain.c`로 이동
- 선행 seam: DONE — intent declaration precollector를 `type_checker_resolution_graph_decl.c`로 이동
- 선행 seam: DONE — world inventory precollector를 `type_checker_resolution_graph_world.c`로 이동
- cleanup: DONE — `find_type_alias_decl`의 cross-include dangling return-type seam을 명시 선언으로 정리
- cleanup: DONE — `type_checker_resolution_graph_core.h` → inventory include 경계의 dangling `static void` seam 2개를 명시 return type으로 정리
- cleanup: DONE — `type_checker_decls_a.inc -> type_checker_decls_domain_helpers.inc`, `type_checker_decls_intent.inc -> type_checker_world_decl.c`, `type_checker_helpers_effects.inc -> type_checker_helpers_host.inc` 사이의 dangling return-type seams 제거
- cleanup: DONE — `type_checker_ability_decl.c`, `type_checker_zone_decl.c`, `type_checker_world_decl.c`를 standalone semantic TU로 빌드 가능하게 만들고 hidden helper 의존을 internal/header 계약으로 승격
- cascade: `type_resolution_intern_node`, `type_resolution_add_edge`, `type_resolution_find_path`, `type_resolution_format_cycle`, `semantic_type_resolution_record_named_dependency`, `semantic_type_resolution_record_type_ref_dependency`, `semantic_type_resolution_collect_type_refs`, `find_type_alias_decl`, `find_domain_decl_by_name`, `semantic_world_find_zone_slot_local`, `create_overlay_nominal_type`를 internal API로 승격
- 현재 크기: `type_checker_resolution_graph_collect.c` 285 LOC, `type_checker_resolution_graph_labels.c` 164 LOC, `type_checker_resolution_graph_domain.c` 136 LOC, `type_checker_resolution_graph_decl.c` 554 LOC, `type_checker_resolution_graph_world.c` 381 LOC, `type_checker_resolution_graph_inventory.c` 764 LOC, `type_checker_resolution_stage.c` 999 LOC, `type_checker_resolution_stage_domain.c` 520 LOC
- 예상 cascade: 10+ — full audit 필요

---

## 절단 워크플로 (axis 1개 기준)

각 axis는 다음 6단계로 진행:

1. **Pre-cut audit**
   - 대상 .inc의 모든 함수에 대해 in/out 의존성 grep
   - static helper cascade list 작성
   - 테스트 baseline 캡처 (`test_semantic` count)

2. **Header 갱신**
   - 기존 `*_internal.h`에 extern 선언 추가
   - cascade되는 static helper도 동시에 extern 승격 (별도 commit으로 분리)

3. **`.c` 신설**
   - `.inc` 본문 → 새 `.c` 로 이동
   - include chain 정리 (header만, 다른 .inc 직접 include 금지)

4. **`type_checker.c` 갱신**
   - 해당 `.inc` include 제거
   - 새 헤더 include 추가
   - Makefile에 `.c` 등록

5. **빌드 + 회귀**
   - `make rebuild` → object 빌드 확인
   - `test_semantic` count 동일 확인
   - `test_transpile` count 동일 확인
   - smoke test 8/8 OK

6. **Cascade fix-up**
   - cascade promote된 static helper들의 다른 사용처가 깨지지 않는지 확인
   - 필요시 fwd decl 정리

---

## 진행 트래킹

| Axis | 상태 | 비고 |
|---|---|---|
| 1. diagnostic helpers | DONE | `type_checker_diag.c`; semantic/test-all 통과 |
| 2. ownership classifier | DONE | `type_checker_ownership_classify.c`; `type_is_subject_type` extern 승격 |
| 3. channel transport validator | DONE | `type_checker_channel_transport.c`; channel include 경계 축소 |
| 4. generic contract diagnostics | DONE | `type_checker_generic_diag.c`; function/class/ability generic bound diagnostics 분리 |
| 5. ownership consumers | DONE | `type_checker_ownership_diag.c`; escape diagnostic renderer 분리 |
| 6. module contract consumer | DONE | `type_checker_module_contract.c`; `type_checker_module_contracts.inc` 제거 |
| 7. resolution graph / stage | IN PROGRESS | formatter + graph primitive + dependency recorder + collector-helper + event precollector seams 분리 완료; inventory/stage 본체 남음 |

7개 axis 중 6개가 완료됐고, 7번째 axis도 leaf/primitive/dependency-recorder/collector seam 분리는 시작됐다. `.inc` 카운트 자체는 shim/include 추가 때문에 단순 감소하지 않지만, leaf helper와 module-contract consumer는 실제 translation unit으로 이동했다. 남은 큰 축은 resolution graph inventory/stage 본체이며, 이 축은 full audit 후 좁은 handler 단위로 더 절단해야 한다.

---

## 참고

- [TODO.md P1 (구조/운영 폐인 포인트)](../TODO.md)
- [`docs/91_build_troubleshooting.md`](91_build_troubleshooting.md) — `make rebuild` 작동 메커니즘
- [`src/semantic/type_checker_internal.h`](../src/semantic/type_checker_internal.h) — 현재 extern 인터페이스
