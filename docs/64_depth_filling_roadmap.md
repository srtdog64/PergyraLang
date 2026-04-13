# Depth Filling Roadmap

마지막 업데이트: 2026-04-11

이 문서는 [63_feature_depth_matrix.md](63_feature_depth_matrix.md)의 빈 칸을 실제 구현 작업으로 바꾼 로드맵이다.
핵심은 "새 surface를 더하는 것"이 아니라 "이미 선언한 surface를 끝까지 닫는 것"이다.

기본 전략:
- 새 키워드는 보류한다.
- empty cell을 줄이는 작업만 우선한다.
- 작업 단위는 기능명이 아니라 `파싱/시맨틱/MIR/C/LLVM/런타임/테스트` 칸이다.
- authoring shorthand는 원칙적으로 후순위지만,
  hidden semantics를 줄이거나 실제 depth gap을 닫는 경우에는 예외적으로 먼저 넣는다.

---

## 0. 수정사항 기록

### 2026-04-11

이번 개정에서 바뀐 점:
- 기존 포괄 TODO 문서를 `phase-based depth filling` 문서로 재작성
- `P0`를 `Event semantic`, `Collections semantic`, `silent fallback 제거`로 고정
- ergonomics 개선은 depth closure 이후 단계로 명시
- 각 phase의 완료 기준을 "문서상 존재"가 아니라 `negative semantic / C / LLVM / 문서` 4조건으로 재정의

이번 개정의 결정:
- 지금은 기능 확장 중단
- 지금은 empty cell 제거 우선
- scaffold와 pure sugar는 마지막 단계
- 다만 아래 항목은 이미 depth-closure 목적의 shorthand로 구현 반영됨
  - `using <-> where` intent step 상호 유도
  - `refresh/publish/bind ... map { target <- source; }`
  - explicit `Clone(...)` world embedding surface

---

## 1. 우선순위 원칙

### 원칙 1. 사용자를 속이는 surface부터 닫는다

다음은 가장 위험하다.
- 문법은 있는데 시맨틱이 약한 것
- C는 되는데 LLVM이 안 되는 것
- 런타임이 조용히 no-op인 것

즉 현재 P0는 다음 셋이다.
- `Event semantic`
- `Set/Map/List semantic`
- `silent fallback / observability`

### 원칙 2. 완성 기준을 코드 기준으로 잡는다

각 작업은 아래 네 가지를 동시에 만족해야 완료다.
- negative semantic test가 추가됨
- C smoke/example가 추가됨
- LLVM smoke/example가 추가됨
- 문서가 현재 surface와 동일하게 갱신됨

### 원칙 3. ergonomics는 depth 이후에 올린다

다음은 중요하지만 P0는 아니다.
- intent shorthand
- lexical zone context
- transfer 축약 surface
- projection/group bind ergonomics

다만 예외가 있다.
- hidden copy/reference 오해를 줄이는 표면
- 이미 있는 semantic contract를 더 직접적으로 드러내는 표면
- 반복 서술을 줄이면서도 새 의미론을 추가하지 않는 표면

이 조건을 만족하는 shorthand는 depth closure의 일부로 본다.

이미 반영된 예:
- `move <from> to <to>;`
- `using <-> where` intent step derivation
- `refresh/publish/bind ... map { ... }`
- explicit `Clone(...)` for world embedding

### 원칙 4. TODO는 resolved/open을 섞지 않는다

이미 닫힌 문제를 TODO에 계속 남겨두면 surface trust를 해친다.

2026-04-11 기준 이 문서에서 분리해야 할 항목:
- `HashMap<Int, V>`: resolved
- `intent with value: Type;` 값 파라미터: resolved
- `ToString(Float)` LLVM parity: resolved
- lambda / function value baseline: resolved

반대로 아직 open인 항목:
- collection key 지원 범위는 아직 `String` / `Int`까지만 닫힘
- collection combinator ergonomics (`.map/.filter` style stdlib surface)는 미완
- intent step 반복 선언/authoring friction은 여전히 남음
- `own/ref`는 일반 규칙이 아니라 anchored slot handle에 한정된 partial rule

---

## 1.5 현실 TODO 정리

이 문단은 "표현력이 부족하다"는 막연한 평가를 TODO로 바꾸기 위한 고정판이다.

### A. 범용 프로그래밍 기본기

resolved:
- `HashMap<Int, V>`
- `intent with price: Int;`
- lambda / function value baseline
- `ToString(Float)` parity

open:
- `HashMap<K, V>`의 key 범위는 현재 `String` / `Int`로 제한
- stdlib-level collection combinator (`Map/Filter/Fold`)은 아직 부족
- author-facing iterator / callable ergonomics는 더 다듬어야 함

### B. 쓰기 피로

문제:
- 같은 `where/using/who`를 step마다 반복 서술하는 friction이 남아 있다.

현재 이미 있는 압축 수단:
- intent-level `who:`
- intent-level `where:`
- `using <-> where` step derivation

open:
- step profile/preset 없이 큰 intent에서 반복 선언이 여전히 크다
- projection/binding 쪽도 large scenario에서는 wiring-heavy 하다

### C. 표면 신뢰도

원칙:
- "컴파일되지만 실제로는 안 됨" surface를 허용하지 않는다.

resolved:
- `HashMap<Int, V>`는 이제 실제로 C/LLVM 경로가 있다
- mixed `ability + zone` module의 explicit export 판정은 실제 import/module smoke까지 닫혔다
- `order_analytics`는 이제 compile-smoke covered example로 승격되었다

open:
- unsupported 조합은 parser acceptance가 아니라 explicit semantic error로 고정해야 한다
- depth matrix / roadmap / hands-on assessment가 구현 상태를 즉시 반영해야 한다

### D. `own/ref` 규칙 closure

resolved:
- 일반 타입에 붙는 `own/ref`는 명시 오류
- `ref -> ref` helper forwarding 중 callee가 borrow를 다시 escape하지 않으면 허용
- borrowed `ref` slot의 `return` escape 금지
- borrowed `ref` slot의 `channel send` escape 금지
- borrowed `ref` slot의 alias/new-binding 패턴 금지
- borrowed `ref` slot의 `=` rebinding 금지
- borrowed `ref` slot의 `Move(...)` helper forwarding은 현재 보수적 helper-call escape 규칙으로 차단
- `own SecureSlot<subject-host>`도 새 binding alias는 금지
- local slot의 borrow-after-own-move는 금지
- `own SecureSlot<subject-host>`의 transitive own-helper forwarding은 semantic/C/backend-compare까지 고정

beta decision:
- 베타 stable surface는 `ref Slot<subject-host>` / `own SecureSlot<subject-host>`에 한정한다
- 일반 ownership system은 이 roadmap의 후속 축으로 분리한다

post-beta / follow-up:
- helper/function call 경유 escape를 다단계 alias/summary 수준까지 더 정교하게 판정
- move-after-borrow를 지역/다단계 ownership 규칙으로 더 정교하게 정리

---

## 2. Phase 1 — 위험한 빈 칸 제거

목표:
- 잘못된 코드가 조용히 통과하는 경로 제거
- LLVM에서 죽는 대표 surface 제거
- thin runtime을 최소한 contract-carrying runtime으로 올리기

실행 순서:
1. `Event` semantic closure
2. `Set/Map/List` semantic closure
3. silent fallback 전수 점검

병행 반영 완료:
- misleading surface를 줄이는 authoring fixes는 이미 일부 선반영했다.
- 따라서 아래 phase는 "새 shorthand 추가"가 아니라 "남은 빈 칸 closure"에 집중한다.

### 2.1 Event semantic closure

대상:
- `Event`의 시맨틱
- generated helper와 surface의 정합성

작업:
1. event declaration이 파라미터 시그니처를 실제 semantic symbol로 저장하게 만든다.
2. subscribe/unsubscribe/invoke를 type checker switch에서 직접 검증한다.
3. handler 시그니처 mismatch에 대한 구체 진단을 추가한다.
4. direct `AST_EVENT_INVOKE` 경로와 parser surface의 정합성을 확인하고 문서화한다.
5. generated helper 기반 event model을 문서에 반영한다.

완료 기준:
- 잘못된 handler 시그니처가 semantic error로 막힌다.
- 같은 event 예제가 C/LLVM 양쪽에서 동일하게 돈다는 현재 사실이 문서에 반영된다.
- event surface 설명과 실제 generated helper 모델이 어긋나지 않는다.

체크리스트:
- [ ] `event` 선언 시 파라미터 시그니처 저장
- [ ] `subscribe` 시 핸들러 시그니처 검증
- [ ] `unsubscribe` 시 동일 규칙 적용
- [ ] `invoke` 인자 개수/타입 검증
- [x] parser surface와 direct event AST 경로 정합성 점검
- [ ] generated helper 기반 runtime 설명 문서 반영
- [ ] C/LLVM 공통 smoke 예제 추가
- [ ] negative semantic test 추가

권장 파일 축:
- `src/semantic/type_checker.c`
- `src/semantic/type_checker_decls.inc`
- `src/codegen/llvm_domain.c`
- `src/test_semantic.c`

### 2.2 Collections 전축 closure

대상:
- `Set/Map/List` 시맨틱
- existing LLVM collection path의 coverage 정리

작업:
1. `TYPE_SET`, `TYPE_LIST`, `TYPE_HASHMAP`를 진짜 constructed type로 올린다.
2. generic parameter validation을 넣는다.
3. collection method call을 타입 시스템에 직접 연결한다.
4. existing LLVM 생성자/메서드 lowering의 coverage와 타입 coercion 불균형을 정리한다.
5. C 경로의 현재 제한 사항을 문서상 명시하고, 실제로 지원 안 되는 조합은 조용히 통과시키지 않게 한다.

완료 기준:
- `Set<Int>`, `List<String>`, `Map<String, Int>`, `Map<Int, Int>` 수준의 정상 예제가 C/LLVM 모두 통과한다.
- 잘못된 key/value/element 호출이 semantic error로 막힌다.
- 현재 미지원 조합은 명시적 에러가 난다.
- LLVM 컬렉션 경로가 "없음"이 아니라 "기존 경로 보강"이라는 사실이 문서에 반영된다.

체크리스트:
- [ ] collection type descriptor 정식화
- [ ] generic parameter validation
- [ ] `.add/.push/.get/.set/.has/.remove/.size` 타입 체크
- [ ] LLVM collection coercion/coverage 정리
- [x] `HashMap<String, T>` / `HashMap<Int, T>` 외 key 조합은 명시 오류
- [ ] C/LLVM positive smoke 추가
- [ ] negative semantic test 추가

권장 파일 축:
- `src/semantic/type_system.c`
- `src/semantic/type_system.h`
- `src/semantic/type_checker.c`
- `src/codegen/llvm_expr.c`
- `src/codegen/llvm_stmt.c`
- 필요 시 `src/codegen/llvm_collection.c`

### 2.3 Runtime fallback / observability

대상:
- runtime no-op / placeholder / misleading success
- intent/zone/world inspection surface

작업:
1. `NULL/0/false`를 조용히 반환하는 exported runtime path를 전수 점검한다.
2. runtime warning과 hard contract failure의 경계를 문서화한다.
3. intent/zone/world inspection surface를 최소 조회 가능 형태로 정리한다.
4. "지금은 구현 안 됨" 경로는 성공처럼 보이지 않게 정리한다.

완료 기준:
- placeholder가 있더라도 침묵하지 않는다.
- intent/zone/world 상태를 디버거 없이도 최소 추적할 수 있다.

체크리스트:
- [ ] exported runtime의 `NULL/0/false` 조용한 반환 전수 점검
- [ ] warning vs hard-fail 기준 문서화
- [ ] no-op placeholder를 진단 가능 상태로 교체
- [ ] intent/zone/world inspection surface 정리

### 2.4 World / LLVM 구조 debt

대상:
- MIR body + inventory-backed decl 구조 debt

현재 관찰:
- world는 llvm-smoke 전용 케이스에서 이미 동작이 검증됐다.
- 따라서 남은 문제는 "동작 자체"보다 "구조 debt와 설명력" 쪽이다.

남은 작업:
1. llvm backend 문서와 실제 smoke 범위를 정렬한다.
2. world/domain 경로의 inventory-backed declaration debt를 debt ledger와 연결한다.
3. 필요 시 file split은 구조 정리 수단으로만 사용한다.

완료 기준:
- "world LLVM 미구현" 같은 잘못된 문장이 더 이상 문서에 남지 않는다.
- debt ledger가 기능 누락과 구조 debt를 구분한다.
- world embedding copy semantics가 문서와 진단에서 같은 말로 설명된다

체크리스트:
- [ ] world smoke 근거를 depth 문서에 반영
- [ ] debt ledger와 depth matrix 표현 정렬
- [ ] inventory-backed declaration 경로를 구조 debt로 분류

### 2.4 Silent fallback 제거

대상:
- runtime no-op / placeholder / misleading success

현재 일부는 이미 개선됐다.
- zone authority runtime validation은 실계약으로 올라왔다.
- secure memory unsupported platform은 더 이상 성공을 가장하지 않는다.
- channel/intent 일부 경로는 최소 진단을 남긴다.

남은 작업:
1. `NULL/0/false`를 조용히 반환하는 exported runtime path를 전수 점검한다.
2. runtime warning과 hard contract failure의 경계를 문서화한다.
3. "지금은 구현 안 됨" 경로는 성공처럼 보이지 않게 정리한다.

완료 기준:
- placeholder가 있더라도 침묵하지 않는다.
- 문서와 실제 동작이 어긋나지 않는다.

체크리스트:
- [ ] exported runtime의 `NULL/0/false` 조용한 반환 전수 점검
- [ ] warning vs hard-fail 기준 문서화
- [ ] no-op placeholder를 진단 가능 상태로 교체
- [ ] 최근 실교체된 항목을 문서에 연결

---

## 3. Phase 2 — 중간 깊이 축 닫기

목표:
- "동작은 하지만 설명력이 부족한 축"을 깊게 만든다.

### 3.1 `ability/require/use` 계약 닫기

대상:
- generic ability declaration/reference
- richer mismatch diagnostics
- module contract extension

작업:
1. `ability<T>` 선언 허용 범위를 분명히 정한다.
2. `requires Ability<T>` mismatch 시 실제 기대/실제 인자를 진단한다.
3. default type arg는 beta-stable generic surface에서 unsupported로 고정하고, parser가 받는 표면과 semantic 진단을 일치시킨다.
4. `use/require`를 모듈 경계까지 올리되, surface expansion 없이 기존 규칙을 닫는 방향으로 간다.
5. hidden/default-export policy를 generic 해석과 같이 정렬한다.

완료 기준:
- 계약 시스템이 "구조는 있음"이 아니라 "컴파일러가 설명 가능"한 상태가 된다.
- beta-stable generic surface는 `ability<T>` / `requires Ability<T>` / multi-bound baseline / explicit default-type-arg reject로 고정된다.

### 3.2 `relation/effect/projection` 심화

대상:
- effect lattice
- authority/resource partial order
- projection wiring validation

작업:
1. effect partial order를 문서상의 말뿐이 아니라 semantic relation으로 만든다.
2. authority/resource 규칙과 같은 진단 체계로 묶는다.
3. projection 경로 실패 시 source/target/boundary를 포함한 진단을 낸다.
4. relation/effect 접근 규칙을 단순화하되 implicit rule 폭증은 막는다.

완료 기준:
- effect/relation 쪽이 더 이상 "설계는 강한데 구현은 중간" 상태가 아니다.

### 3.3 Intent / Zone / World observability

대상:
- runtime history
- inspection API
- failure surface

작업:
1. intent history baseline(`last` / `history` / `active` / `recent`)을 source-of-truth로 문서화하고 richer timeline query를 확장한다.

현재 subset 분류:

- stable subset: `last / history / active / recent`
- explicit reject: 없음
- beta-out-of-scope: richer multi-instance timeline query와 deeper failure provenance query
2. zone/world runtime 상태를 최소 조회 가능 형태에서 더 설명 가능한 inspection surface로 올린다.
3. 실패 원인을 authority, boundary, slot, projection 단위로 진단에 노출한다.

완료 기준:
- 디버거가 없어도 runtime state를 사람이 따라갈 수 있다.

---

## 4. Phase 3 — 구조 정리와 productization

목표:
- depth를 깎지 않으면서 유지보수 비용을 줄인다.

### 4.1 LLVM debt ledger를 실제 정리로 연결

작업:
1. `docs/62_llvm_backend_debt_ledger.md`의 항목을 코드/테스트와 연결한다.
2. LLVM에서 남은 inventory-backed declaration helper debt를 줄인다.
3. MIR-first 경로와 LLVM emission의 책임 경계를 더 분명히 한다.

완료 기준:
- "LLVM도 된다"가 smoke 수준이 아니라 구조적으로 설명 가능해진다.

### 4.2 Tooling closure

대상:
- debugger
- formatter
- LSP

작업:
1. debugger는 스텁이 아닌 최소 stepping/introspection 단위로 올린다.
2. formatter는 stable surface subset을 먼저 완성한다.
3. LSP는 parser completion 수준을 넘어 semantic diagnostics와 연결한다.

완료 기준:
- tooling 문서 상태가 `stub/basic/partial`에서 벗어난다.

### 4.3 Authoring compression

이 단계는 마지막이다.

후보:
- intent profile/preset
- lexical zone context
- transfer shorthand
- bind group / projection shorthand
- scaffold/template

주의:
- 이 단계는 "좋아 보이는 문법"을 늘리는 작업이 아니다.
- 앞선 phase에서 닫힌 규칙을 더 짧게 쓰게 만드는 작업이어야 한다.

---

## 5. 이번 로드맵에서 하지 않을 것

다음은 지금 당장 우선순위가 아니다.
- 새 존재론 키워드 추가
- debugger가 비어 있는데 문법부터 늘리는 일
- collection/runtime가 덜 닫혔는데 새 stdlib 타입 늘리기
- generic contract가 덜 닫혔는데 더 복잡한 generic surface 확장

이 문서의 목적은 넓이를 늘리는 것이 아니라, 이미 열어둔 문을 닫는 것이다.

---

## 6. 완료 순서 제안

실행 순서는 아래가 가장 안전하다.

1. `Event` semantic closure
2. `Set/Map/List` semantic closure
3. silent runtime fallback 전수 정리
4. generic contract / module contract 정리
5. effect-resource-authority lattice 정리
6. runtime observability 강화
7. tooling 완성
8. authoring shorthand

이 순서를 뒤집으면, 사용성은 잠깐 좋아 보여도 내부 부채가 더 커진다.

---

## 7. 진행 기록

### 완료

- [x] depth filling 우선순위를 `Event semantic → Collections semantic → fallback 제거`로 고정
- [x] ergonomics보다 depth closure 우선 원칙 명시
- [x] phase별 완료 기준을 테스트 포함 기준으로 재정의
- [x] `Event` LLVM/helper 경로 존재 확인 및 문서 기준 정정 필요성 반영
- [x] `Collections` LLVM 경로 존재 확인 및 문서 기준 정정 필요성 반영
- [x] `World` llvm-smoke 검증 범위 확인 및 문서 기준 정정 필요성 반영

### 다음 실행

- [x] `Event` semantic closure 착수
- [x] `Event` semantic closure 마무리 및 문서 정합성 반영
- [x] `Set/Map/List` semantic closure

### 2026-04-11 진행 메모

- `Event` semantic closure 1차 착수
- 범위: event declaration validation, subscribe/unsubscribe signature validation, direct `AST_EVENT_INVOKE` argument validation
- 테스트 추가: 정상 handler, arity mismatch, lambda typed mismatch
- 추가 확인: LLVM은 이미 `INIT/SUBSCRIBE/UNSUBSCRIBE/INVOKE` helper와 global event storage를 생성함
- 정합성 확인: parser surface의 `OnEvent(x)`는 일반 `AST_CALL`로 들어가고, `AST_EVENT_INVOKE`는 내부 carrier 성격이 더 강함
- 마감: unsubscribe mismatch, invoke count mismatch negative semantic test 추가
- 마감: `examples/event_closure_probe.pgy` example smoke 연결
- 마감: generated helper 기반 runtime 설명/정합성 반영
- `Collections` semantic closure 1차 착수
- 범위: `List/Map/Set/Queue` builtin의 arity, container kind, key/value/element/index 타입 검증 강화
- 테스트 추가: positive generic collections flow, wrong list element, wrong map key, wrong list index
- `Collections` LLVM 경로 재평가
- 확인: `List/Set/Queue/HashMap`는 생성자와 주요 builtin 호출 lowering이 이미 존재함
- 보강: `SetAdd/SetHas/SetRemove/QueuePush/MapSet`의 값 coercion 경로를 `ListPush/ListSet`과 맞춤
- 마감: unsupported key kind / wrong set element / wrong list container negative semantic test 추가
- 마감: `examples/collections_closure_probe.pgy` example smoke 연결
- `runtime fallback / observability` 1차 착수
- 범위: collection raw export의 null/invalid argument, bounds, empty-pop 경로에 최소 runtime warning 추가
- intent active inspection surface 보강
- 추가 export: `parent_handle`, `subject_count`, `step_count`, `failed`, `failure_reason`
- `World` LLVM 재평가
- 확인: `llvm_smoke`에 world 전용 케이스가 다수 존재하며, 현재 정확한 debt는 기능 미구현이 아니라 구조/설명력 debt임

---

## 작업 순서 요약

```
Phase 1 (P0) — 안전하지 않은 컴파일 방지
  1.1  Event 시맨틱 검증
  1.2  Set/Map/List 시맨틱 검증
  1.3  runtime fallback / observability

Phase 2 (P1) — 기능 완성
  2.1  Event surface/runtime 설명 정리
  2.2  World LLVM 구조 debt 정리
  2.3  Channel 시맨틱 보강

Phase 3 (P2) — 품질
  3.1  Intent 런타임 히스토리
  3.2  Zone 런타임 authority 강화
  3.3  Set/Map/List 제네릭 일반화
```

---

## 진행 추적

각 작업 완료 시 [63_feature_depth_matrix.md](63_feature_depth_matrix.md)의 해당 칸을 업데이트한다.

| # | 작업 | 매트릭스 칸 변화 | 상태 |
|---|------|-----------------|------|
| 1.1 | Event 시맨틱 | Event 시맨틱 ◐→✅ | 진행 중 |
| 1.2 | Collection 시맨틱 | Set/Map/List 시맨틱 ❌→✅ | 미착수 |
| 1.3 | Runtime fallback/observability | silent fallback/inspection gap 축소 | 진행 중 |
| 2.1 | Event LLVM | Event LLVM ⚠️→✅ | 미착수 |
| 2.2 | Event 런타임 | Event 런타임 ❌→✅ | 미착수 |
| 2.3 | World LLVM | World LLVM ⚠️→✅ | 미착수 |
| 2.4 | Channel 시맨틱 | Channel 시맨틱 강화 | 미착수 |
| 3.1 | Intent 런타임 | Intent 런타임 스텁→✅ | 미착수 |
| 3.2 | Zone 런타임 | Zone 런타임 스텁→✅ | 미착수 |
| 3.3 | Collection 일반화 | Set/Map/List C ⚠️→✅ | 미착수 |

## Authoring-surface fill track (current)

Depth filling is no longer only about backend/runtime gaps. A parallel fill track is now required for surface trust and authoring compression.

### Track A. Contract compression

Focus:
- remove repeated contract spelling between actions and intent steps
- keep the reusable contract anchored at the action declaration
- limit step-local spelling to true overrides and orchestration-only data

Immediate artifacts:
- semantic inherited-contract diagnostics
- AST/debug provenance markers
- paired long-vs-compressed reference examples

### Track B. Surface trust

Focus:
- keep docs aligned with what is implemented
- separate smoke-covered stable examples from reference-only examples
- close any gap where the short surface exists but is not clearly documented as the preferred path

Immediate artifacts:
- stable example surface board updates
- pain-point and compression-plan updates that use the same clause-family vocabulary as the compiler

### Track C. Dense-form sharpness

Focus:
- the language still permits dense explicit forms, so failures in those forms must stay precise
- explicit declarations should fail locally instead of turning into vague downstream errors

Immediate artifacts:
- parser clause-family diagnostics
- semantic tests for inherited-vs-local contract confusion
- reference examples that show the exact crossover point between explicit and compressed forms

## Current rule for roadmap ordering

When choosing between a new feature and filling an authoring-surface trust gap, prefer the trust gap if the feature would create another surface the user can write but cannot confidently reason about.
