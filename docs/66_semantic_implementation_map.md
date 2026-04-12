# Semantic Implementation Map

마지막 업데이트: 2026-04-12

이 문서는 Pergyra의 의미론을 `현재 실제 구현` 기준으로 정리한다.
설계 목표가 아니라, 현재 저장소에서 확인되는 규칙과 구현 깊이를 기준으로 적는다.

관련 문서:

- [18_language_status.md](/mnt/e/PergyraLang/docs/18_language_status.md)
- [37_compiler_contracts.md](/mnt/e/PergyraLang/docs/37_compiler_contracts.md)
- [63_feature_depth_matrix.md](/mnt/e/PergyraLang/docs/63_feature_depth_matrix.md)
- [58_keyword_authorship_pain_points.md](/mnt/e/PergyraLang/docs/58_keyword_authorship_pain_points.md)
- [59_authoring_surface_compression_plan.md](/mnt/e/PergyraLang/docs/59_authoring_surface_compression_plan.md)
- [61_surface_compression_examples.md](/mnt/e/PergyraLang/docs/61_surface_compression_examples.md)

---

## 1. 한 줄 요약

현재 Pergyra의 의미론은 다음처럼 분류해야 한다.

- `알파 완료`: 코어 실행 의미론, slot/resource/authority, async/channel/select
- `알파 범위에서 이번에 닫아야 할 축`: intent/zone/world orchestration, relation/effect/projection, generic contract, own/ref
- `베타 전까지 추가 금지`: 새 surface, 새 ontology, 새 runtime subsystem

즉 지금의 Pergyra는 단순한 문법 실험이 아니라,
`slot anchor를 중심으로 한 실행 의미론`이 실제 컴파일러와 런타임까지 연결된 상태다.
다만 베타로 가려면 남은 축을 `완료`시키거나 `표면에서 내려야` 한다.

---

## 2. 의미론을 실제로 고정하는 계층

### 2.1 AST

역할:
- 표면 문법 보존
- sugar 보존
- 원래 작성 형태 보존

현재 판단:
- AST는 설명 계층이지 실행 계층이 아니다
- semantics의 최종 근거는 AST가 아니다

### 2.2 HIR

역할:
- typed language tree 정규화
- 선언/루틴/CFG 준비
- frontend diagnostics와 bridge용 구조 제공

현재 판단:
- 선언과 제어 흐름의 언어적 모양은 여기서 안정된다
- domain resource semantics의 최종 해석은 아직 여기서 하지 않는다

### 2.3 DIR

역할:
- ability/role/zone/world/intent/relation/effect 같은 도메인 존재론 정규화
- declaration graph와 contract edge 고정

현재 판단:
- `누가 누구를 필요로 하는가`는 DIR에서 거의 정리된다
- action/step/authority/projection의 declaration-level contract는 여기까지가 첫 고정점이다

### 2.4 RIR

역할:
- slot anchor 위에서 자원 의미론 정규화
- claim/read/write/release/move/refresh/publish/authorize/transfer 등을 공통 자원 op로 정리

현재 판단:
- Pergyra 의미론의 실질적 핵심은 여기다
- 단순 타입체커가 아니라 `자원 상태 전이 시스템`으로 프로그램을 읽는 층이 이미 존재한다

### 2.5 MIR

역할:
- CFG/SSA/phi/cleanup/rollback edge 정규화
- backend가 읽을 실행 그래프 확정

현재 판단:
- intent cleanup, merge, exceptional topology, SSA merge는 실제 구현 축으로 들어와 있다
- 의미론이 backend마다 따로 갈라지는 구조를 줄이는 기준선이 MIR다

---

## 3. 현재 실제로 강한 의미론

### 3.1 Core execution semantics

포함:
- `let`
- `func`
- `if/else`
- `for/while`
- `break/continue`
- `match`
- `enum`
- 일반 호출/메서드 호출

상태:
- parser/semantic/MIR/C/LLVM 전부 연결
- 현재 언어에서 가장 안정된 축

판단:
- Pergyra는 더 이상 “도메인 키워드만 많은 언어”가 아니다
- 일반 실행 코어는 이미 알파 기준으로 충분히 닫혀 있다

### 3.2 Slot/resource semantics

포함:
- `Slot<T>`
- `SecureSlot<T>`
- `DeviceSlot<T>`
- `QubitSlot<T>` 표면
- claim/read/write/release
- secure pairing/token 규칙

상태:
- semantic/C/LLVM/runtime/test 전부 강함
- 현재 구현 중 가장 완성도 높은 축

판단:
- 이 축이 Pergyra의 진짜 기준선이다
- 다른 도메인 기능도 최소한 이 정도의 설명력과 회귀를 가져야 한다

### 3.3 Async/channel/select semantics

포함:
- `async/await`
- `spawn`
- `channel`
- `select`
- cooperative cancellation

상태:
- pthread/fiber 기반 runtime 경로 존재
- C/LLVM/runtime/test 모두 연결

판단:
- 이 영역은 `표면만 있음`이 아니라 실제 실행 의미론이 있다
- 남은 문제는 기능 존재 여부보다 type-system integration과 observability 품질이다

---

## 4. 알파 범위에서 이번에 닫아야 하는 의미론

### 4.1 Ability / role / require contract

현재 되는 것:
- ability declaration
- role impl
- zone authority requires
- action requires
- generic ability baseline
- canonical `fields` ability contract surface
- action/step `requires` / `within` / `authorized by` / `causes` inheritance diagnostics

이번 알파 범위에서 끝내야 할 것:
- richer mismatch diagnostics
- module contract 수준의 `use/require`
- generic bound와 contract summary의 전면 정리

판단:
- 철학의 핵심 축이지만, 구현 깊이는 아직 `중간`이다
- 존재는 분명하지만 “컴파일러가 항상 완전히 설명해 준다” 수준은 아니다
- 단, `override` / `extends` / `dyn` 같은 보조 표면은 이 축의 core-closure 판정에서 분리해서 봐야 한다

### 4.2 Intent orchestration semantics

현재 되는 것:
- intent declaration
- participant binding
- step contract validation
- rollback/compensate/cleanup 경로
- action contract inheritance
- transfer derivation
- history 일부

이번 알파 범위에서 끝내야 할 것:
- richer runtime observability
- distributed runtime 모델
- deeper policy surface

판단:
- intent는 이미 executable orchestration이다
- 다만 runtime introspection과 더 깊은 execution model은 아직 얕다

### 4.3 Zone / World semantics

현재 되는 것:
- zone slot/authority/state/projection/lifecycle
- world zone/state composition
- world embedded zone query surface
- C/LLVM lowering과 테스트 범위 존재

이번 알파 범위에서 끝내야 할 것:
- multi-zone coordination policy의 깊이
- embedding ownership story의 더 넓은 정리
- richer runtime propagation

판단:
- zone/world는 더 이상 parser decoration이 아니다
- 정확한 평가는 `중상`이며, “없다”가 아니라 “runtime depth가 더 남음”이다

---

## 5. 아직 완료라고 부르면 안 되는 의미론

### 5.1 relation / effect / projection

현재 되는 것:
- declaration surface
- slot wiring
- lifecycle shorthand
- refresh/publish/bind
- object/tobject projection contract 분리
- 일부 query surface (`HasProjection`, `HasLayer`, `HasState`)

이번 알파 범위에서 끝내야 할 것:
- full effect lattice
- authority/resource partial order와의 완전 통합
- richer propagation model
- ergonomics 전체 정리

판단:
- 이 축은 “설계의 힘”은 강하지만 “구현 depth”는 아직 중간이다
- 과장해서 완료라고 부르면 surface trust를 깎는다

### 5.2 Generic surface

현재 되는 것:
- generic declaration baseline
- generic ability baseline
- 일부 bound validation
- default type argument explicit reject
- exact bound / ability-style bound / multi-bound baseline
- `ability<T> where ...` bound의 reference/impl revalidation

이번 알파/베타 범위에서 끝내야 할 것:
- default type arg를 영구 불지원 stable policy로 문서화할지 결정
- broader type-family generalization 여부를 결정
- parser가 받는 표면 대비 beta-stable semantic closure 범위를 고정

판단:
- parser가 넓게 받아들인다고 구현이 깊은 것은 아니다
- generic은 stable subset이 생겼고, 남은 것은 주로 policy와 broader generalization 범위다

### 5.3 own/ref ownership surface

현재 되는 것:
- anchored slot 전달 subset
- 일부 forwarding 경로
- secure boundary paired-token 경로

베타 기준 고정:
- stable subset은 `ref Slot<subject-host>` / `own SecureSlot<subject-host>`다
- 일반 ownership system은 베타 범위 밖이다
- unsupported 조합은 explicit semantic error로 유지한다

판단:
- `own/ref`는 존재하지만 아직 일반 목적 ownership system이라고 부르기 어렵다
- 정확한 표현은 `anchored-slot subset이 stable surface로 닫힌 상태`다

---

## 6. 현재 authoring surface에서 실제로 고정된 의미 규칙

### 6.1 Matching action contract pack

현재 action에서 step으로 상속되는 기본 계약 묶음은 다음이다.

### 6.2 relation/effect/projection stable subset

베타 기준으로 지금 고정해서 읽어야 하는 surface:

- `relation` / `effect` / `zone` / `world` declaration baseline
- positional constructor baseline
- `subject slot` / `object slot` / `tobject slot`
- `refresh objectSlot from subjectSlot`
- `publish dtoSlot from subjectSlot`
- `bind slotName from sourceSlot`
- `HasProjection` / `HasLayer` / `HasState` / world-side zone query family
- C/LLVM 공통의 sync-helper 기반 incremental parity

아직 과장하면 안 되는 것:

- projection propagation 전체가 깊게 닫혔다고 말하는 것
- authority/resource/effect unified lattice가 완성됐다고 말하는 것
- zone/world runtime policy가 fully compositional하다고 말하는 것

정확한 표현:

- projection sync baseline은 stable하다
- deeper propagation model은 아직 진행 중이다

- `who`
- `where`
- `requires`
- `authorized by`
- `causes`

현재 판단:
- 이 다섯 개는 step에서 반복 서술하지 않는 방향이 현재 권장 surface다
- step은 orchestration과 override 중심으로 쓰는 것이 맞다

### 6.2 Transfer derivation pack

현재 transfer target에서 기본 유도되는 묶음은 다음이다.

- `where`
- `using`

현재 판단:
- `transfer: source -> target;`은 단순 문법 sugar가 아니라
  step zone/binding contract를 압축하는 실제 의미론적 축약이다

### 6.3 Declaration-local clause

현재 `with effects`는 matching action contract pack에 포함되지 않는다.

판단:
- 이 절은 declaration-local only로 보는 것이 맞다
- action signature 뒤에 붙는다고 해서 step contract inheritance까지 가져가는 절이 아니다

### 6.4 Provenance vocabulary

현재 diagnostics/tooling/docs에서 맞춰야 하는 용어는 다음 셋이다.

- `locally declared ...`
- `inherited ... from matching action contract`
- `derived ... from transfer target`

판단:
- compression이 강해질수록, 이 provenance vocabulary가 surface trust의 핵심이 된다
- 사용자가 상속/유도를 머리로 재구성하게 만들면 안 된다

---

## 7. 현재 구현을 설명할 때 피해야 할 오판

### 7.1 틀린 표현

- `zone/world는 아직 없다`
- `intent는 문법만 있다`
- `event는 parser만 있다`
- `LLVM에서 주요 축이 전반적으로 안 돈다`
- `object/tobject는 아직 lexer alias 수준이다`

### 7.2 더 정확한 표현

- `zone/world는 compile-time contract와 backend lowering이 있으며 runtime depth가 더 남아 있다`
- `intent는 executable orchestration이며 observability/runtime depth가 더 남아 있다`
- `event는 semantic/codegen 경로가 있으나 subsystem 설명력이 아직 얕다`
- `LLVM은 광범위하게 동작하며 남은 debt는 특정 축의 정렬 문제다`
- `object/tobject는 distinct nominal/projection contract로 실제 구현돼 있다`

---

## 8. 현재 구현 정리 기준

이제부터 문서와 상태판은 `부분 구현`이라는 표현을 줄이고 아래 세 분류를 우선 사용한다.

지금 저장소를 정리할 때는 기능을 다음 셋으로 나누는 것이 맞다.

### 8.1 Alpha-complete semantics

조건:
- parser/semantic/MIR/C/LLVM 중 핵심 경로가 닫혀 있음
- runtime 또는 lowering explanation이 있음
- regression/test가 실제 존재함

현재 대표 예:
- core execution
- slot/resource
- async/channel/select
- nominal core (`subject/class/object/tobject/...`)
- compressed action/transfer authoring path의 stable subset

### 8.2 Experimental semantics

조건:
- 실행 경로는 있으나
- diagnostics, runtime depth, richer contract, broader coverage가 부족함

현재 대표 예:
- ability/role/generic contract
- intent runtime observability
- zone/world richer propagation
- relation/effect/projection
- collections

### 8.3 Removed or explicitly closed surface

조건:
- parser가 받거나 일부 경로가 있더라도
- 아직 일반 사용자에게 “완성된 계약”처럼 소개하면 안 되는 영역

현재 대표 예:
- richer generic surface 전체
- own/ref 일반 ownership system
- full effect lattice
- fully productized debugger/LSP/runtime observability

---

## 9. 지금부터의 정리 원칙

1. 새 의미론을 추가하기 전에 현재 semantics를 `stable / partial / do-not-oversell`로 먼저 분류한다.
2. parser가 받는 표면을 곧바로 지원 완료로 문서화하지 않는다.
3. authoring compression은 sugar 추가가 아니라 contract provenance와 함께 가야 한다.
4. relation/effect/projection은 설계 표현력보다 runtime/diagnostic closure를 먼저 채운다.
5. `slot anchor`를 잃는 설명은 피한다. 현재 Pergyra의 중심 의미론은 slot/resource anchor 위에 서 있다.

---

## 10. 결론

현재 Pergyra는 다음처럼 정리하는 것이 가장 정확하다.

- 코어 언어와 slot/resource 의미론은 이미 알파 완료 범위다.
- intent/zone/world, relation/effect/projection, generic contract, own/ref는 베타 전까지 완료시키거나 surface를 내려야 한다.
- 현재 제품성의 핵심은 새 개념 추가보다 `closure`, `diagnostics provenance`, `surface trust`다.

즉 지금 해야 할 일은
`언어가 무엇을 표현할 수 있는가`를 더 넓히는 것보다,
`이미 표면에 올린 의미론을 전부 닫는 일`이다.
