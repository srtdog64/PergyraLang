# Beta Readiness Checklist

마지막 업데이트: 2026-04-24

이 문서는 베타 진입 전 반드시 닫아야 하는 실행 체크리스트다. 기준은 기능 개수가 아니라 **surface trust + 구조 지속 가능성 + C/LLVM parity**다. 현재 공식 beta readiness는 약 70%로 본다.

상태 표기:

- `DONE`: 구현/문서/회귀가 같은 말을 한다.
- `IN PROGRESS`: 핵심 경로는 있으나 source-of-truth 또는 coverage가 부족하다.
- `BLOCKER`: 베타 이름을 붙이기 전 반드시 닫아야 한다.
- `OUT OF BETA`: 베타 뒤로 명시 이동한다.

---

## 1. Core Module / Module Boundary Closure

상태: `IN PROGRESS / BLOCKER`

목표:

- core / foundation / execution / compatibility surface가 문서와 테스트에서 같은 경계로 유지된다.
- 장기 모듈화는 `.inc` 제거율 자체가 아니라 owner boundary와 dependency direction을 명확히 한다.
- 신규 core 변경이 multi-thousand-line include fragment를 직접 수정하지 않아도 된다.

현재 닫힌 것:

- `docs/99_language_module_taxonomy.md`로 core/foundation/execution/compat layer를 고정했다.
- `docs/language_module_manifest.json`, `docs/language_module_cases.json`가 machine-readable source다.
- `make module-taxonomy-test-smoke`가 taxonomy drift를 검사한다.
- semantic leaf/helper split이 진행되어 diagnostics, ownership, generic, ability, module contract, DAG primitive/collector/label/domain/decl/world 일부가 실제 `.c` translation unit으로 이동했다.
- `type_checker_ability_decl.c`, `type_checker_zone_decl.c`, `type_checker_world_decl.c`는 standalone semantic TU로 빌드된다.
- `type_checker_intent_decl.c`도 standalone semantic TU로 빌드되며, helper boundary 누락은 기본 CFLAGS의 implicit-declaration hard error로 차단된다.
- `type_checker_role_decl.c`, `type_checker_party_decl.c`, `type_checker_roster_decl.c`도 standalone semantic TU hard-CFLAGS path에서 빌드된다.
- `type_checker_resolution_graph_inventory.inc`는 737 LOC까지 줄었고, graph inventory axis는 semantic 800 LOC stop condition 아래로 내려갔다.
- `type_checker_resolution_stage_domain.c`가 world/zone local-contract stage replay를 소유하며, `type_checker_resolution_stage.inc`는 969 LOC까지 줄었다.

남은 것:

- semantic에는 아직 800 LOC 초과 `.inc`가 남아 있다.
- codegen/runtime에는 1,000 LOC를 크게 넘는 `.inc`가 남아 있다.
- `type_checker.c`는 아직 orchestration-only가 아니라 include aggregator 성격이 남아 있다.
- core module boundary와 compiler implementation module boundary가 아직 완전히 대응하지 않는다.

완료 조건:

- `src/semantic`에 800 LOC 초과 `.inc`가 없다.
- `src/codegen`과 `src/runtime`에 1,000 LOC 초과 `.inc`가 없다.
- core semantic/DAG/backend/runtime owner boundary가 문서와 파일 구조에서 추적 가능하다.
- `.inc`는 generated table, local macro table, private test fixture 용도로만 남는다.

증거 명령:

```sh
make module-taxonomy-test-smoke
make test-semantic
make test-all
make llvm-test-backend-compare
find src/semantic src/codegen src/runtime -name '*.inc' -print0 | xargs -0 wc -l | sort -nr | head
```

---

## 2. Type-Resolution DAG Closure

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
- intent/standalone helper dependency는 internal headers와 hard CFLAGS로 고정되어 DAG split 중 hidden include-order failure를 즉시 잡는다.
- graph cycle과 legacy alias cycle 모두 `Contract source`, `Reason`, `Fix` vocabulary를 쓴다.

남은 것:

- `resolve_type_node` 중심 recursive resolver가 여전히 semantic source-of-truth 일부다.
- remaining consumers를 `graph-backed`, `namespace-only`, `legacy`로 분류해야 한다.
- frozen subset에서 declaration order에만 기대는 type dependency가 없어야 한다.
- graph inventory metadata를 backend/declaration inventory와 더 잘 연결해야 한다.

완료 조건:

- frozen subset의 known type dependency가 declaration order에만 의존하지 않는다.
- DAG staged schedule이 generic/module/authority/party/local projection path에서 source-of-truth로 쓰인다.
- cycle/provenance diagnostics가 graph path 기준으로 안정적이다.
- docs가 “full rewrite”가 아니라 남은 migration path를 정확히 부른다.

증거 명령:

```sh
make test-semantic
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

남은 것:

- owner shell과 runtime ABI contract가 섞인 helper가 남아 있다.
- `char *`, `const char *`, grow-array payload 반환 helper의 ownership audit가 필요하다.
- runtime query/diagnostic string이 scratch teardown 이후에도 안전한지 회귀가 더 필요하다.

완료 조건:

- helper ownership이 `borrowed`, `scratch-owned`, `result-owned`, `persistent-owned`, `runtime-owned` 중 하나로 분류된다.
- runtime ABI return ownership이 문서화되고 테스트된다.
- frozen subset diagnostic/runtime query가 scratch lifetime 이후 dangling되지 않는다.

증거 명령:

```sh
make test-abi
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
