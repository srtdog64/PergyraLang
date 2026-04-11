# Feature Depth Matrix

마지막 업데이트: 2026-04-11

이 문서는 PergyraLang의 기능별 구현 깊이를 코드 기준으로 기록한다.
설계 문서상 존재하는 개념이 아니라, 현재 저장소에서 실제로 확인된 depth만 적는다.

판정 원칙:
- `파싱`만 있으면 surface만 있는 것이다.
- `시맨틱`이 비면 안전하지 않은 컴파일이다.
- `C`와 `LLVM`이 모두 안 맞으면 백엔드 debt다.
- `런타임`이 비면 도메인 키워드는 형식만 있고 실행 의미가 얇다.
- `테스트`가 약하면 구현이 아니라 우연히 돌아가는 상태다.

상태 범례:
- `✅` 실사용 가능
- `◐` 부분 구현 또는 thin
- `❌` 사실상 비어 있음

---

## 0. 최근 수정 사항

2026-04-11 기준 이번 정리에서 반영한 변경:
- 기존 "넓지만 얕다" 초안을 실제 코드 기준 depth matrix로 재작성
- `파싱/시맨틱/C/LLVM`만 보던 표를 `MIR/하강`, `런타임`, `테스트`까지 확장
- `Intent`, `Zone`, `Channel`, `Slot`은 최근 구현 상태를 반영해 상향 판정
- `Event semantic`, `Set/Map/List`, `runtime observability`, `relation/effect/projection`을 핵심 depth gap으로 재고정
- tooling은 `있다`와 `완성됐다`를 분리해 `debugger/formatter/LSP`를 별도 축으로 분리
- `debugger`는 단순 스텁이 아니라 `AST-walking source debugger`로, `formatter`는 Windows LF 안정성까지 포함한 basic formatter로, `LSP`는 lightweight semantic tooling으로 재분류
- authoring compression 중 일부는 더 이상 "나중 sugar"가 아니라 실제 depth-closure 수단으로 반영
  - `using <-> where` intent step 상호 추론
  - `refresh/publish/bind ... map { target <- source; }`
  - explicit `Clone(...)` world embedding surface

이번 문서의 의도:
- "무슨 기능이 있나"를 보여주기보다
- "어디가 실제로 닫혀 있고 어디가 비어 있는가"를 보여주는 것

---

## 1. 요약 매트릭스

| 영역 | 파싱 | 시맨틱 | MIR/하강 | C | LLVM | 런타임 | 테스트 | 깊이 판정 | 핵심 메모 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|---|---|
| 기본 코어 (`let/func/if/for/while/match`) | ✅ | ✅ | ✅ | ✅ | ✅ | 해당 없음 | ✅ | 깊음 | 현재 언어의 가장 안정된 축 |
| 타입/제네릭 surface | ✅ | ◐ | ◐ | ◐ | ◐ | 해당 없음 | ◐ | 중간 | surface는 넓지만 generic contract는 아직 얕음 |
| `subject/class/object/tobject/enum` | ✅ | ✅ | ✅ | ✅ | ✅ | 해당 없음 | ✅ | 깊음 | 존재론 surface는 동작하나 일부 명명/표면 정책 정리 중 |
| `ability/role/require/use` 계약 | ✅ | ◐ | ◐ | ✅ | ✅ | 해당 없음 | ◐ | 중간 | generic ability ref, richer contract validation은 남음 |
| `Slot/SecureSlot/DeviceSlot/QubitSlot` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | 깊음 | 현재 가장 완성도 높은 도메인 축 |
| `Intent` | ✅ | ✅ | ✅ | ✅ | ✅ | ◐ | ✅ | 중상 | 오케스트레이션은 강하고 `who/where/using` 추론도 있음, 기록계층은 얇음 |
| `Zone` | ✅ | ✅ | ✅ | ✅ | ✅ | ◐ | ✅ | 중상 | authority/runtime contract는 실제 연결, transaction/world policy는 더 얇음 |
| `World` | ✅ | ✅ | ✅ | ✅ | ✅ | ◐ | ✅ | 중상 | C/LLVM smoke는 검증됐고 direct zone embedding은 explicit-copy 경고로 정리됨 |
| `relation/effect/projection` | ✅ | ◐ | ◐ | ✅ | ◐ | ◐ | ✅ | 중간 | field-map/projection shorthand는 닫혔지만 lattice/authority 통합은 미완 |
| `Channel/select` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | 깊음 | 실제 런타임이 있고 최근 진단도 보강됨 |
| `Event` | ✅ | ◐ | ◐ | ✅ | ✅ | ◐ | ◐ | 중간 | 코드젠은 존재, semantic closure와 문서 정합성이 부족 |
| `Set/Map/List` | ✅ | ◐ | ◐ | ◐ | ✅ | ◐ | ◐ | 중간 | LLVM 경로는 존재, 핵심 gap은 semantic closure와 coverage |
| 디버거 | ✅ | ◐ | ❌ | ❌ | ❌ | ◐ | ❌ | 얕음 | AST-walking source debugger는 있으나 compiled runtime debug는 없음 |
| 포매터 | ✅ | ✅ | 해당 없음 | 해당 없음 | 해당 없음 | 해당 없음 | ◐ | 기본 구현 | stable/idempotent formatter와 smoke는 있으나 style/product depth는 얕음 |
| LSP | ✅ | ◐ | 해당 없음 | 해당 없음 | 해당 없음 | ◐ | ◐ | 기본 구현 | diagnostics/hover/completion/symbol/definition/reference/rename까지는 있음 |

---

## 2. 행별 해설

### 2.1 기본 코어

현재 Pergyra의 실질적 기반이다.
제어 흐름, 함수, 기본 타입, 패턴 매칭, 메서드 호출, enum 경로는 C/LLVM 양쪽에서 실제로 버틴다.

판정:
- 컴파일러의 "알파 코어"는 이미 존재한다.
- 새 문법을 더 늘리기보다 이 축과 같은 깊이로 다른 도메인 기능을 끌어올리는 쪽이 맞다.

### 2.2 타입/제네릭 surface

파서는 많이 받아들이지만, 시맨틱과 lower 단계가 아직 surface를 다 따라가지 못한다.

대표 gap:
- default type arg는 surface 대비 시맨틱 뒷받침이 약하다.
- `where T: A + B` 같은 richer bound는 parser가 받을 수 있어도 검증과 활용이 얕다.
- generic ability reference는 구조는 올라왔지만 완전한 declaration/validation 체인은 닫히지 않았다.

판정:
- "쓸 수 있는 것처럼 보이는 generic surface"가 남아 있다.
- 이 영역은 새 문법 추가보다, 닫지 못한 surface를 줄이거나 완성하는 쪽이 먼저다.

### 2.3 존재론 축 (`subject/class/object/tobject/enum`)

파서, 시맨틱, C/LLVM 코드젠까지는 강하다.
다만 표면 설계에서 pain point가 남아 있다.

대표 pain point:
- `subject`와 `class`가 lexer 단계에서 완전히 분리되지 않았던 흔적
- `object/tobject` surface와 token 정책의 일관성 문제
- `ability` 기본 export 정책 같은 surface ergonomics 정리 필요

판정:
- "구현 depth" 자체는 나쁘지 않다.
- 문제는 correctness보다 surface consistency 쪽이다.

### 2.4 `ability/role/require/use` 계약 축

언어 철학상 핵심인데, 실제 depth는 "중간"이다.
기본 계약 검증과 코드젠은 있으나, richer contract system은 아직 partially closed 상태다.

남은 gap:
- `ability<T>` 선언과 참조 전체 체인
- `requires Ability<T>`의 풍부한 mismatch 진단
- `use/require`를 module contract까지 일관되게 올리는 작업
- hidden/default-export 규칙과 generic 해석의 일관화

판정:
- 방향은 맞다.
- 아직 "컴파일러가 완전히 설명 가능한 계약 시스템" 단계는 아니다.

### 2.5 Slot 축

현재 저장소에서 가장 잘 닫힌 도메인 기능이다.
특히 secure 쪽은 단순 키워드가 아니라 실제 runtime rule로 이어진다.

현재 강한 점:
- claim/write/read/release lifecycle
- secure pairing/token 검증
- unsupported platform을 조용히 성공 처리하지 않고 명시 오류로 돌림
- C/LLVM 모두 같은 런타임 계약에 연결

판정:
- depth를 채우는 기준선으로 삼아야 할 축이다.

### 2.6 Intent

최근 기준으로 intent는 "문법만 있는 기능"이 아니다.
실제로 semantic inference와 codegen이 붙어 있다.

현재 강한 점:
- orchestration surface 존재
- rollback/cleanup 경로 실구현
- C/LLVM 양쪽 경로 존재
- 예제와 smoke 범위가 실제로 있다
- `transfer -> using/where`, `using -> where`, `where -> using` 추론이 이미 연결돼 있다

남은 gap:
- intent history/storage는 여전히 thin
- runtime inspection은 최소 정보 중심
- distributed/multi-process intent runtime은 아직 범위 밖

판정:
- core compile path는 강하다.
- runtime observability가 아직 얕다.

### 2.7 Zone

Zone은 현재 "시맨틱 없는 장식" 단계는 벗어났다.
권한/authority/slot/state/projection 구조가 컴파일러 경로에 걸쳐 연결되어 있다.

최근 보강된 점:
- zone authority runtime validation이 더 이상 debug-only placeholder가 아니다
- C와 LLVM 모두 실제 runtime contract 호출로 연결됨
- silent no-op 대신 진단이 남는다
- top-level lexical `within Zone { ... }` context가 있어 반복 선언을 줄일 수 있다

남은 gap:
- multi-slot transaction semantics
- richer projection path validation
- world와 묶인 higher-order runtime policy

판정:
- compile-time depth는 꽤 올라왔다.
- runtime coordination depth가 남아 있다.

### 2.8 World

World는 "없다"라고 말할 정도는 아니지만, 가장 자신 있게 완료 판정을 내릴 수준도 아니다.

현재 상태:
- parser/semantic/C 쪽은 존재
- LLVM 쪽도 world 전용 smoke 범위에서 검증됐다
- runtime은 zone 메커니즘에 많이 기대고 있다
- direct zone binding world-embedding은 warning 대상이고 `Clone(...)`이 권장 surface다

판정:
- world는 `C/LLVM 모두 중상` 정도로 보는 것이 맞다.
- 실제 debt는 "동작 안 함"이 아니라 "MIR-led 구조 정리와 observability가 덜 닫힘" 쪽이다.

### 2.9 relation / effect / projection

언어 차별점이지만 아직 가장 위험한 오해 지점이기도 하다.
surface는 풍부한데, writer ergonomics와 semantic closure가 완전히 일치하지 않는다.

남은 gap:
- effect lattice 완전판
- authority/resource partial order와의 통합
- projection wiring 축약과 진단 개선
- relation/effect 접근 ergonomics
- field-name mismatch를 위한 projection map은 구현됐지만 group-map/named multi-target surface는 아직 아니다

판정:
- 설계는 강하다.
- 구현 깊이는 아직 중간이다.

### 2.10 Channel / select

실제 runtime이 있는 몇 안 되는 강한 축이다.
bounded ring buffer, send/recv/select 경로가 있고 최근에는 잘못된 사용에 대한 진단도 보강됐다.

남은 gap:
- constructed type로서의 더 정교한 semantic model
- select case 진단 강화
- author-facing cancellation story와의 통합

판정:
- runtime depth는 높다.
- type-system integration은 아직 더 다듬을 여지가 있다.

### 2.11 Event

기존 문서에서 가장 과소평가된 축 중 하나다.

현재 확인된 점:
- parser surface는 있다.
- C 코드젠은 event helper를 생성한다.
- LLVM 코드젠도 `INIT/SUBSCRIBE/UNSUBSCRIBE/INVOKE` helper와 global event storage를 만든다.
- smoke surface가 이미 존재한다.

실제 gap:
- semantic validation이 약했고, 이번에 1차 closure를 시작했다.
- runtime이 별도 독립 subsystem이라기보다 generated helper 중심이라 설명력이 약했다.
- parser surface는 `OnEvent(x)`를 일반 `AST_CALL`로 파싱하고, `AST_EVENT_INVOKE`는 내부 carrier 성격이 더 강하다.

판정:
- event는 더 이상 "surface만 있다" 수준은 아니다.
- 정확한 평가는 "코드젠은 존재하지만 semantic closure와 runtime 설명이 덜 닫힌 중간 축"이다.

### 2.12 Set / Map / List

컬렉션은 넓지만 얕다의 대표 사례다.

핵심 문제:
- 타입 시스템과 generic validation이 약하다
- C 경로는 부분 동작하지만 제한이 많다
- LLVM 경로는 실제로 존재하지만 coverage와 타입 coercion 정합성이 불균형했다
- runtime helper는 있어도 컴파일러 depth가 못 따라간다

판정:
- 새 컬렉션 surface를 더 추가할 때가 아니다.
- 이미 있는 `Set/List/Map`을 먼저 제대로 닫아야 한다.

### 2.13 Tooling

디버거, formatter, LSP는 "있다"와 "완성됐다"를 구분해야 한다.

현재 더 정확한 상태는 다음과 같다.

- debugger:
  AST를 직접 걷는 source-level stepping debugger는 있다.
  breakpoint/list/backtrace 같은 최소 인터랙션도 있다.
  하지만 compiled binary, DWARF, runtime state inspection debugger는 아니다.
- formatter:
  token-stream 기반 formatter가 있고, parseable/stable/idempotent check와
  smoke도 있다. 최근에는 Windows line-ending 차이도 정리됐다.
  다만 style configurability나 product-grade formatting depth는 아직 얕다.
- LSP:
  diagnostics, hover, completion, document symbols, definition, references,
  rename까지는 있다.
  다만 깊은 semantic index, project-wide intelligence, 높은 정확도의 refactor
  품질까지 닫힌 상태는 아니다.

판정:
- debugger: `스텁`보다는 `얕은 구현`
- formatter: `초안`보다는 `basic product surface`
- LSP: `partial`이지만 최소 editor integration은 이미 가능

즉 tooling은 과장하면 안 되지만, 더 이상 "아무것도 없다"라고 쓰는 것도 틀리다.

---

## 3. 현재 전체 판정

한 줄 요약:

> PergyraLang은 더 이상 단순 parser project는 아니다.
> 하지만 여전히 기능별 depth 편차가 크고, 특히 `Event semantic`, `Set/Map/List semantic`, `runtime observability`, `effect/projection` 쪽에 빈 칸이 남아 있다.

조금 더 정확히 말하면:
- 코어 언어, slot, channel, intent/zone 일부는 이미 "깊은 축"이다.
- world, relation/effect, generic contract, event는 "중간 이상 축"이다.
- collections는 "중간 축", debugger는 "얕은 축", formatter/LSP는 "기본 구현 축"이다.

---

## 4. 우선순위

### P0

지금 즉시 depth를 채워야 하는 축:
- `Event semantic`
- `Set/Map/List`
- `runtime silent fallback / observability`
 
이 중 `Set/Map/List`는 이제 "LLVM 자체 부재"보다 "semantic/coverage 부족" 쪽이 더 정확한 평가다.

이 셋은 빈 칸이 실제 사용자를 속이는 영역이다.
"문법이 있으니 된다"라고 느끼게 하지만, 실제로는 파이프라인이 끝까지 안 닫혀 있다.

### P1

다음으로 닫아야 하는 축:
- `ability/require/use` 계약의 richer semantic closure
- `relation/effect/projection`의 lattice 및 authority 통합
- intent/zone/world runtime observability

### P2

코어 depth 이후에 가야 하는 축:
- formatter/LSP/debugger 완성
- authoring shorthand와 scaffolding
- 표면 ergonomics 고도화

핵심은 순서다.
지금 필요한 것은 surface expansion이 아니라 empty cell removal이다.

---

## 5. 완료 기준

이 문서는 아래 조건을 만족할 때 사실상 종료된다.

- `Event`가 `파싱/시맨틱/MIR/C/LLVM/런타임/테스트`를 모두 채운다.
- `Set/Map/List`가 LLVM까지 닫히고 generic validation이 들어간다.
- `World`가 LLVM에서도 debt 항목이 아니라 정상 축으로 내려온다.
- `relation/effect/projection`이 "설계는 강함"이 아니라 "구현도 강함"으로 바뀐다.
- tooling 문서가 현재 구현 수준에 맞는 product-level 상태를 가진다.

---

## 6. 상태 변경 기록

### 2026-04-11

상태 조정:
- `Slot`: `완성` 유지
- `Channel/select`: `중상`에서 `깊음` 쪽으로 정리
- `Intent`: 단순 thin 기능이 아니라 `compile path 강함 + runtime thin`으로 정정
- `Zone`: debug-only placeholder 평가를 제거하고 `runtime contract 일부 실체화`로 정정
- `World`: `없음` 또는 `미구현` 평가를 배제하고 `LLVM smoke 검증 + 구조 debt 잔존`으로 정정
- `Event`: `LLVM/runtime 부재` 평가를 제거하고 `semantic closure 부족` 중심으로 정정
- `Set/Map/List`: `LLVM 부재` 평가를 제거하고 `semantic/coverage 부족` 중심으로 정정
- `debugger`: `부재/스텁` 평가를 제거하고 `AST-walking source debugger`로 정정
- `formatter`: line-ending 안정성까지 반영해 `basic formatter`로 정정
- `LSP`: lightweight language tooling 범위를 명시하고 `partial but usable`로 정정

새 기준선:
- P0는 `Event semantic`, `Set/Map/List semantic`, `runtime fallback/observability`
- P1은 `ability/require/use`, `relation/effect/projection`, `observability`
- P2는 `tooling`과 `authoring shorthand`
