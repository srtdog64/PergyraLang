# Beta Readiness Checklist

마지막 업데이트: 2026-04-25

이 문서는 베타 진입 전 반드시 닫아야 하는 실행 체크리스트다. 기준은 기능 개수가 아니라 **surface trust + 구조 지속 가능성 + C/LLVM parity**다. 현재 공식 beta readiness는 약 70%로 본다.

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
- The proof scope is intentionally narrow: core declarations, stable generics, anchored own/ref, stable collections, observability baseline, `parallel` baseline, runtime propagation, DAG, ABI ownership, and C/LLVM parity.

Closed now:

- `docs/semantics/` is the source of truth for beta proof vocabulary, judgments, theorem statements, and remaining proof obligations.
- `docs/102_formal_semantics_and_proof_obligations.md` now points to the split proof pack and remains as the stable English index.
- The doc explicitly separates language proof obligations from the math library design in `docs/45_math_layer_design.md`.
- Out-of-beta proof claims are sealed for full quantum, full FP/HKT/functor algebra, arbitrary ownership, arbitrary map keys, GPU/Spray, Skia/render, package manager, and advanced debugger semantics.

Remaining:

- Tie each B0 closure item to a theorem/invariant row before calling that item beta-complete.
- Keep DAG, runtime propagation, MIR declaration inventory, ABI ownership, and backend parity blockers open until their theorem statements and regression evidence match.
- Do not advertise mechanized proof for beta unless a separate Lean/Coq or executable small-step model is actually added.

Evidence command:

```sh
make formal-semantics-test-smoke
```

## 1. Core Module / Module Boundary Closure

상태: `IN PROGRESS / BLOCKER`

목표:

- core / foundation / execution / compatibility surface가 문서와 테스트에서 같은 경계로 유지된다.
- 장기 모듈화는 `.inc` 제거율 자체가 아니라 owner boundary와 dependency direction을 명확히 한다.
- 신규 core 변경이 multi-thousand-line include fragment를 직접 수정하지 않아도 된다.

현재 닫힌 것:

- 2026-04-25 local acceptance: `make ci-linux` completed green on WSL/Linux after the DAG metadata and authority direct-slot fixes. This covers `test-all`, LLVM smoke, fmt/stdlib/module/example smoke, taxonomy/inc-size/core-shape/DAG/inventory/diagnostic/parser-lexer/JSON/IR/AST gates, ABI same-process, and backend compare.
- `docs/99_language_module_taxonomy.md`로 core/foundation/execution/compat layer를 고정했다.
- `docs/language_module_manifest.json`, `docs/language_module_cases.json`가 machine-readable source다.
- `make module-taxonomy-test-smoke`가 taxonomy drift를 검사한다.
- semantic leaf/helper split이 진행되어 diagnostics, ownership, generic, ability, module contract, DAG primitive/collector/label/domain/decl/world 일부가 실제 `.c` translation unit으로 이동했다.
- `type_checker_ability_decl.c`, `type_checker_zone_decl.c`, `type_checker_world_decl.c`는 standalone semantic TU로 빌드된다.
- `type_checker_intent_decl.c`도 standalone semantic TU로 빌드되며, helper boundary 누락은 기본 CFLAGS의 implicit-declaration hard error로 차단된다.
- `type_checker_role_decl.c`, `type_checker_party_decl.c`, `type_checker_roster_decl.c`도 standalone semantic TU hard-CFLAGS path에서 빌드된다.
- `type_checker_resolution_graph_inventory.c`가 graph inventory axis를 소유한다. 기존 `type_checker_resolution_graph_inventory.inc`는 제거됐다.
- `type_checker_resolution_stage_domain.c`가 world/zone local-contract stage replay를 소유하고, `type_checker_resolution_stage.c`가 top-level DAG stage replay를 소유한다. `type_checker_resolution_stage.inc`는 제거됐다.
- `type_checker_class_decl.c`가 class/extern declaration checking을 소유하고, `type_checker_program.c`가 top-level semantic orchestration을 소유한다. `type_checker_program.inc`는 624 LOC까지 줄어 semantic 800 LOC stop condition 아래로 내려갔다.
- `type_checker_builtins_projection.c`가 `ToObject` / `ToTObject` projection diagnostics를 소유하며, `type_checker_builtins_nominal.inc`는 659 LOC까지 줄어 semantic 800 LOC stop condition 아래로 내려갔다.
- `type_checker_expr_ops.c`가 binary/unary/array literal/indexed access를 소유하고, `type_checker_expr_names.c`가 static member path / consumed-boundary name helper를 소유한다. `type_checker_expr.inc`는 758 LOC, `type_checker_helpers_late.inc`는 773 LOC까지 줄어 semantic 800 LOC stop condition 아래로 내려갔다.
- `type_checker_decls_domain_helpers.c`가 domain slot/projection/overlay helper body를 소유한다. `type_checker_decls_domain_helpers.inc`는 제거됐다.
- `type_checker_intent_helpers.c`가 intent inheritance/derivation/helper body를 소유한다. `type_checker_decls_a.inc`는 1-line forwarding stub으로 축소됐다.
- `type_checker_event.c`가 event declaration/subscription/invoke semantic을 소유한다.
- `type_checker_qubit.c`가 QubitSlot compile-time state, entangle pool, movable-resource-use validation을 소유한다.
- `type_checker.c`는 481 LOC로 내려갔고, semantic stop condition의 600 LOC 이하 조건을 만족한다.
- `make semantic-inc-size-test-smoke`가 `src/semantic/**/*.inc <= 800 LOC`를 검사한다.
- `make semantic-core-shape-test-smoke`가 `type_checker.c <= 600 LOC`, DAG inventory `.c` ownership, event/qubit owner TU 존재를 검사한다.
- `transpiler_emitters_mir_inventory_ssa.inc`는 5-line shim으로 축소됐고, MIR intent inventory / SSA name / SSA emit slice가 각각 1,000 LOC 아래 하위 include로 분리됐다.
- `transpiler_expr_emitters.inc`는 7-line shim으로 축소됐고, builtin / call A / call B / member / tail slice가 각각 1,000 LOC 아래 하위 include로 분리됐다. 현재는 include-order 보존 split이며, beta+1 수준의 실제 TU/owner 추출은 별도 과제다.
- `llvm_expr_calls.inc`는 17-line shim + constructor / array / collection-base / domain query / event invocation / intent observability / log / scalar math / result-option / slot-device / task-channel owner + 네 개의 call slice로 축소됐고, 각 slice가 1,000 LOC 아래로 내려갔다. `llvm_emit_call` 앞단의 enum/class constructor lowering은 `llvm_expr_call_constructors.inc`로, array builtin은 `llvm_expr_call_arrays.inc`로, `ListNew`/`Set*` base collection family는 `llvm_expr_call_collections_base.inc`로, `HasProjection` / `HasLayer` / `HasState` / `HasZone*` lowering은 `llvm_expr_call_domain_queries.inc`로, event invocation은 `llvm_expr_call_events.inc`로, intent observability runtime calls는 `llvm_expr_call_intent_observability.inc`로, `Log*`는 `llvm_expr_call_log.inc`로, `Abs`/`Min`/`Max`는 `llvm_expr_call_math.inc`로, Result/Option builtin lowering은 `llvm_expr_call_result_option.inc`로, `ClaimSlot` / `Write` / `Read` / `Release` / `Device*` lowering은 `llvm_expr_call_slots.inc`로, task cancellation/channel operations는 `llvm_expr_call_task_channel.inc`로 이동했다. 단, list/map/queue continuation / stdlib IO/file/time / user function fallback owner 추출은 남아 있다.
- `transpiler_emitters_base_b.inc`는 6-line shim으로 축소됐고, 네 개의 mechanical slice가 각각 1,000 LOC 아래로 내려갔다. 단, statement/block/intent forward-declare owner extraction은 남아 있다.
- Tier 1 runtime/codegen/compiler `.inc` split gate를 닫았다. `pgy_runtime_part_ba.inc`, `pgy_runtime_lib_part_b.inc`, `transpiler_emitters_base_a.inc`, `transpiler_helpers_core_a.inc`, `transpiler_helpers_core_b.inc`, `transpiler_domain_role.inc`, `llvm_expr_helpers.inc`, `mir_public.inc`, `llvm_expr_call_methods.inc`, `llvm_domain_helpers.inc`는 모두 shim + sub-1,000 LOC slice로 내려갔다.
- `make backend-inc-size-test-smoke`가 `src/runtime`, `src/codegen`, `src/compiler`의 `.inc <= 1000 LOC`를 검사한다.
- `type_checker_helpers_late.c` standalone TU가 hidden include-order helper 없이 빌드되도록 call-path helper prototypes와 slot analyzer / visibility / generic diagnostic include 계약을 명시했다.
- string literal / interpolation stable subset을 grammar docs에 고정했다. Stable은 `"..."`, `"""..."""`, `"${expr}"`, `f"{expr}"`, escaped f-string brace까지이며 nested brace matching / format specifier / multiline interpolation은 beta-out-of-scope다.
- `diagnostic-registry-test-smoke`가 `diag_codes.h` / `docs/72_diagnostic_codes.md` code sync와 `semantic_error_with_hints` / `semantic_warning_with_hints` macro usage를 검사한다.
- runtime authority failure surface는 `pgy_runtime_authority_contract.h`를 공통 source-of-truth로 사용한다. inline C runtime과 LLVM runtime library export가 같은 `missing-zone` / `missing-participant` code, reason, stderr format을 쓰며, `runtime-authority-contract-test-smoke`가 literal drift를 막는다.
- projection diagnostics는 missing source field / ambiguous source path / wrong projection kind / duplicate field map 4개 베타 필수 케이스를 `projection-diagnostic-contract-test-smoke`로 고정한다.
- AI-first/GPU 방향은 `pgy.accel.spray`로 module taxonomy와 manifest에 예약했다. 이는 post-beta accelerator library/runtime surface이며 core keyword나 beta blocker가 아니다.
- Skia/shader/render 방향은 `pgy.render.skia`로, DOP style은 `pgy.compat.dop`로 module taxonomy와 manifest에 예약했다. 둘 다 post-beta ecosystem surface이며 core keyword나 beta blocker가 아니다.
- module ecosystem update policy를 taxonomy에 고정했다. `pgy.core`는 가장 자주 개선하되 가장 강하게 검증하고, OOP/FP/DOP/GPU/render/std/kit은 모듈 생태계로 진화한다.

남은 것:

- Tier 1 파일 크기 gate는 닫혔지만, 여러 slice는 include-order 보존 mechanical split이다. LLVM constructor owner처럼 일부 semantic-owner 추출은 시작됐고, 나머지 실제 TU/owner extraction은 아직 Tier 2 구조 부채다.
- `type_checker.c`는 600 LOC 이하로 내려갔지만, 아직 일부 helper shim include가 남아 있어 완전한 orchestration-only는 아니다.
- core module boundary와 compiler implementation module boundary가 아직 완전히 대응하지 않는다.
- parser/lex error code routing은 아직 semantic diagnostic registry만큼 강하게 닫히지 않았다.
- `pgy.accel.spray`는 아직 구현/stdlib/API가 없다. 베타 전에는 설계 경계만 유지하고, 베타 이후 CPU fallback + explicit device/context + owned buffer/tensor API부터 별도 closure로 진행한다.
- `pgy.render.skia`와 `pgy.compat.dop`도 아직 구현/stdlib/API가 없다. 베타 전에는 module boundary만 유지하고, 베타 이후 render/shader graph 및 data-layout helper를 별도 closure로 진행한다.

완료 조건:

- `src/semantic`에 800 LOC 초과 `.inc`가 없다. 현재 `make semantic-inc-size-test-smoke`로 고정한다.
- `src/codegen`, `src/runtime`, `src/compiler`에 1,000 LOC 초과 `.inc`가 없다. 현재 `make backend-inc-size-test-smoke`로 고정한다.
- core semantic/DAG/backend/runtime owner boundary가 문서와 파일 구조에서 추적 가능하다.
- `.inc`는 generated table, local macro table, private test fixture 용도로만 남는다.

증거 명령:

```sh
make module-taxonomy-test-smoke
make test-semantic
make test-all
make llvm-test-backend-compare
make backend-inc-size-test-smoke
find src/semantic src/codegen src/runtime -name '*.inc' -print0 | xargs -0 wc -l | sort -nr | head
```

---

## 2. Type-Resolution DAG Closure

2026-04-25 update:

- Graph precollect now materializes context-independent builtin type refs (`Int`, `Long`, `Float`, `Double`, `Bool`, `String`, `QubitSlot`, `Void`) into `SemanticContext.type_resolution_metadata`.
- Graph metadata now materializes a narrow stable constructed subset (`List<T>`, `Set<T>`, `HashMap<String|Int, T>`, `Option<T>`, `Result<T,E>`) when the argument facts are already available. Constructed `Type` shells are owned by the semantic context metadata lane and released on context destroy.
- Pass-2 owner resolver seams now query `semantic_type_resolution_lookup_resolved_type(...)` before falling back to recursive `resolve_type_node(...)`.
- `resolve_generic_type_arg(...)` is also metadata-first, so constructed builtin and generic consumer paths reuse graph facts before recursive fallback.
- `make type-resolution-dag-test-smoke` now gates graph-backed stage skips, metadata entries, metadata owned entries, and metadata hits. Latest local smoke: `graph-backed skips=3079 metadata_entries=1527 metadata_owned=1 metadata_hits=2380`.
- This is not full DAG source-of-truth yet. The remaining closure is graph/topo materialization for generic/default/bound/module/nominal references, then shrinking recursive fallback to explicit legacy-only seams.
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
- relation/effect domain inventory precollector는 graph domain TU로 이동했다.
- world inventory precollector는 graph world TU로 이동했다.
- zone refresh projection field-map collector는 graph zone TU로 이동했다.
- world/zone local-contract stage replay는 stage domain TU로 이동했다.
- DAG stage 내부의 legacy `resolve_type_node(...)` fallback은 `PGY_TYPE_RES_STATS=1`에서 `stage-legacy-resolve: calls/failed/suppressed_diagnostics`와 `stage-legacy-family: generic_contract/signature/ability_consumer/domain_contract/alias/other`로 노출된다.
- DAG edge가 이미 있는 named type-ref는 generic argument를 포함해 stage에서 다시 materialize하지 않고 graph-backed skip으로 처리한다. `stage-graph-backed: skips=N`이 이 경로의 공개 지표이며 `type-resolution-dag-test-smoke`는 skip 합계가 0으로 퇴행하면 실패한다.
- graph precollect TU는 더 이상 stage runner를 호출하지 않는다. enum methods도 `semantic_stage_method_array(...)`가 아니라 precollect action contract 경로로 edge를 수집한다.
- stage lookup과 stage stats helper는 `type_checker_resolution_stage_lookup.c` / `type_checker_resolution_stage_stats.c`로 분리됐다. `type_checker_resolution_stage.c`는 895 LOC로 내려가 stage replay 본체만 소유한다.
- generic where/default validation은 `type_checker_generic_validation.c`가 소유한다. `type_checker_resolution_graph_*.c`와 `type_checker_resolution_graph_core.inc`는 resolver-free graph layer로 고정됐고, `semantic-core-shape-test-smoke`가 graph layer의 직접 `resolve_type_node(...)` 호출을 금지한다.
- intent declaration resolution은 participant/value/where local seam 3개로 수렴했고, 이제 graph metadata-first 조회 후 recursive fallback으로 내려간다.
- domain contract resolution은 slot/shared/named-ref local seam 3개로 수렴했고, projection/relation/effect contract도 graph metadata-first 조회 후 fallback으로 내려간다.
- intent helper resolution은 `intent_helper_resolve_type_ref(...)` 단일 seam으로 수렴했다. transfer-derived using/where, ability generic arg, role-field checks는 이 seam에서 graph-backed metadata로 교체할 수 있다.
- host helper resolution은 `host_helper_resolve_type_ref(...)` 단일 seam으로 수렴했다. projection source fields, hosted method return/param, zone authority/domain slot checks는 이 seam에서 graph-backed metadata로 교체할 수 있다.
- program declaration/body resolution은 quiet/body resolver seam으로 수렴했다. function-body materialization seam은 graph metadata-first 조회 후 fallback으로 내려간다.
- event signature resolution은 `semantic_event_resolve_type_ref(...)` 단일 seam으로 수렴했다. event params, return type, lambda handler signature를 graph-backed signature metadata로 교체할 수 있다.
- world shared/domain-slot resolution은 `world_resolve_type_ref(...)` / `world_resolve_domain_slot_type(...)` seam으로 수렴했다. world shared fields와 slot initializer checks는 이 seam에서 graph-backed metadata를 재사용할 수 있다.
- role/generic-contract/late-helper/expr resolution은 각각 local seam 1개로 수렴했다. remaining direct resolver inventory에서 이 파일들은 이제 metadata replacement owner를 명확히 가진다.
- generic validation, ability where/module contract/declaration, class field, operator overload, ownership destructure resolution도 local seam으로 수렴했다. remaining direct resolver inventory는 resolver implementation, comments, or explicit seam sites로 압축됐다.
- statement, ability field, builtin projection/query, flow, generic support, helper effects, ownership let, party/roster/zone single-call resolver paths도 local seam으로 수렴했다.
- `make type-resolution-resolver-inventory-test-smoke`가 새 direct `resolve_type_node(...)` 호출을 resolver implementation/stage legacy fallback/core fallback/local seam allowlist 밖에서 금지한다.
- `type_checker_decls_domain_helpers.c`의 zone authority participant resolver가 exact/qualified-tail direct slot match를 먼저 인정하고, direct match 반환 시 stale ambiguity flag를 지운다. `dnd_tavern_campaign` 같은 multi-slot same-type zone에서 concrete participant alias가 false-positive ambiguous로 떨어지는 경로를 닫았다.
- `make type-resolution-dag-test-smoke`가 graph stats, topo validation, stage legacy fallback inventory를 CI gate로 검사한다.
- intent/standalone helper dependency는 internal headers와 hard CFLAGS로 고정되어 DAG split 중 hidden include-order failure를 즉시 잡는다.
- graph cycle과 legacy alias cycle 모두 `Contract source`, `Reason`, `Fix` vocabulary를 쓴다.

남은 것:

- `resolve_type_node` 중심 recursive resolver가 여전히 semantic source-of-truth 일부다.
- remaining consumers를 `graph-backed`, `namespace-only`, `legacy`로 분류해야 한다.
- `stage-legacy-resolve` 호출량과 family별 호출량을 더 줄여야 한다. `stage-graph-backed` skip 수는 DAG가 실제 stage source-of-truth로 옮겨간 양을 보여주는 공개 지표다.
- frozen subset에서 declaration order에만 기대는 type dependency가 없어야 한다.
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

남은 것:

- declaration inventory bootstrap은 아직 AST-shaped metadata를 많이 들고 있다.
- zone/world/relation/effect declaration metadata를 dedicated view로 잘라야 한다.
- raw host-name state와 duplicated named-decl lookup helper를 더 줄여야 한다.

완료 조건:

- `MIRDeclInventory` 또는 equivalent dedicated declaration metadata view가 있다.
- C/LLVM이 frozen declaration metadata를 같은 reader에서 소비한다.
- 누락된 declaration inventory는 backend error로 실패한다.
- docs는 남은 AST reference를 internal representation debt로만 설명한다.

증거 명령:

```sh
make test-mir
make llvm-test-backend-compare
make ast-dispatch-test-smoke
```

---

## 4. ABI Ownership / Arena Lifetime Closure

상태: `IN PROGRESS / BLOCKER`

목표:

- scratch/result/persistent/runtime ownership lane이 review 가능해야 한다.
- returned string/helper payload ownership이 함수명과 문서로 구분된다.
- runtime ABI returned values가 scratch pointer에 기대지 않는다.

현재 닫힌 것:

- `docs/94_arena_index_lifetime_plan.md`로 `Arena + Index + 역할별 arena` 방향을 고정했다.
- semantic scratch arena, diagnostic result-owned payload seam, HIR/MIR routine scratch, LLVM scratch/result-owned lane이 들어왔다.
- `make test-abi-perf`와 `make perf-summary`로 speed baseline도 관리한다.
- POSIX `realpath` implicit declaration warning을 제거했다.
- intent observability and authority failure stable string exports are `runtime-borrowed string` ABI values: callers must not free them, and values are valid until the next mutation of the corresponding runtime registry/snapshot.
- `runtime-abi-lifetime-test-smoke` gates stable intent/authority string export bodies so they do not allocate/free/strdup in the ABI return path.

남은 것:

- owner shell과 runtime ABI contract가 섞인 helper가 남아 있다.
- helper payload, runtime-owned handle, and grow-array payload return helpers still need the same ownership audit.
- runtime query/diagnostic string이 scratch teardown 이후에도 안전한지 회귀가 더 필요하다.

완료 조건:

- helper ownership이 `borrowed`, `scratch-owned`, `result-owned`, `persistent-owned`, `runtime-owned` 중 하나로 분류된다.
- runtime ABI return ownership이 문서화되고 테스트된다.
- frozen subset diagnostic/runtime query가 scratch lifetime 이후 dangling되지 않는다.

증거 명령:

```sh
make test-abi
make runtime-abi-lifetime-test-smoke
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

- backend compare에 `parallel_channel_sum`, `parallel_channel_dual`, `triple_paradigm`이 있다.
- `llvm_smoke.sh`는 `select_ready`, `select_fairness`, channel pressure, spawn/generic spawn/string spawn을 가진다.
- `test-concurrency`는 worker spawn, channel send/recv, cancellation, descendant cancellation, zone HasLayer stress를 검증한다.
- module smoke에는 `parallel_ref_slot_conflict` semantic rejection이 있다.

남은 것:

- core keyword별 stable/reject/out-of-beta matrix가 아직 하나의 체크리스트로 묶여 있지 않다.
- `parallel`과 zone/effect/resource conflict의 C/LLVM parity coverage를 더 명시해야 한다.
- execution family가 core identity로 과장되지 않도록 README/status docs wording을 마지막에 다시 맞춰야 한다.

완료 조건:

- core keyword matrix가 parser/semantic/runtime/C/LLVM/doc status를 가진다.
- `parallel` core path와 execution family path가 다른 layer로 문서화된다.
- C/LLVM backend compare에 대표 `parallel + resource/effect/channel` cases가 있다.

증거 명령:

```sh
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

1. DAG source-of-truth audit and migration.
2. DAG-adjacent modularization split.
3. MIR declaration inventory view.
4. ABI ownership audit.
5. parallel/core keyword matrix.
6. pain point sweep and beta wording freeze.
## Progress Log — 2026-04-24 Parser/Lexer Diagnostic Routing

- `parser_error`와 lexer error token이 stage code, reason, fix를 갖도록 1차 routing gate를 닫았다.
- 새 코드: `PGY_PARSE_SYNTAX`, `PGY_LEX_INVALID_TOKEN`.
- 새 gate: `make parser-lexer-diagnostic-test-smoke`.
- CI 연결: `ci-linux`가 parser/lexer diagnostic gate를 실행한다.
- 남은 beta debt: parse/lex message surface는 routable하지만, full JSON diagnostic object routing은 아직 driver/parser refactor가 필요하다.
