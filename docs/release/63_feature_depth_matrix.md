# Feature Depth Matrix

마지막 업데이트: 2026-04-23

이 문서는 PergyraLang의 기능별 구현 깊이를 코드 기준으로 기록한다.
설계 문서상 존재하는 개념이 아니라, 현재 저장소에서 실제로 확인된 depth만 적는다.

판정 원칙:
- `파싱`만 있으면 surface만 있는 것이다.
- `시맨틱`이 비면 안전하지 않은 컴파일이다.
- `C`와 `LLVM`이 모두 안 맞으면 백엔드 debt다.
- `런타임`이 비면 도메인 키워드는 형식만 있고 실행 의미가 얇다.
- `테스트`가 약하면 구현이 아니라 우연히 돌아가는 상태다.

상태 범례:
- `✅` 알파 완료 범위
- `◐` 실험적 또는 closure 진행 중
- `❌` 제거 또는 비활성화 대상

---

## 0.5 베타 게이트 해석 규칙

이 문서에서 `◐`로 남는 항목은 베타 전에 둘 중 하나가 되어야 한다.

- `✅`로 승격
- surface에서 내리고 `❌` 또는 explicit experimental로 격리

즉 `◐`는 최종 상태가 아니라 작업 큐다.
베타 목표에서는 `◐`를 설명하는 것이 아니라 없애는 것이 목적이다.

### Stable subset / explicit reject / beta-out-of-scope

subset surface는 아래 세 분류를 같이 써야 한다.

- stable subset
  - 지금 바로 문서/예제/회귀의 기준으로 삼는 범위
- explicit reject
  - parser가 받아들이더라도 semantic에서 명시적으로 거부해야 하는 범위
- beta-out-of-scope
  - 장기 방향일 수는 있지만 현재 베타 표면으로 약속하지 않는 범위

현재 핵심 축의 적용:

- generics
  - current stable subset: exact/ability/multi-bound baseline + implemented declaration/call/module-consumer path의 default type argument actual resolution
  - strict closure target: richer mismatch provenance와 broader instantiation-path parity
  - beta-out-of-scope: broader generic generalization
- own/ref
  - stable subset: classifier-backed copy-value trivial own/ref + boundary-visible aggregate provenance + movable value transfer/borrow + slot-handle boundary rule on the closed consumer paths
  - explicit reject: authority-bearing `Token<T>` escape/transport
  - beta-out-of-scope: ownership lattice beyond the current classifier/summary model
- collections
  - stable subset: `List<T>`, `Set<T>`, `HashMap<String, T>`, `HashMap<Int, T>`, `HashMap<Long, T>`, `HashMap<Bool, T>`
  - explicit reject: unsupported map key kinds
  - beta-out-of-scope: arbitrary key-universal collection contracts
- runtime observability
  - stable subset: `last / history / active / recent`
  - explicit reject: 없음
  - beta-out-of-scope: richer multi-instance timeline query와 deeper failure provenance query

---

## 0. 최근 수정 사항

2026-04-11 기준 이번 정리에서 반영한 변경:
- 기존 "넓지만 얕다" 초안을 실제 코드 기준 depth matrix로 재작성
- `파싱/시맨틱/C/LLVM`만 보던 표를 `MIR/하강`, `런타임`, `테스트`까지 확장
- `Intent`, `Zone`, `Channel`, `Slot`은 최근 구현 상태를 반영해 상향 판정
- `Event semantic`, `Set/Map/List`, `runtime observability`, `relation/effect/projection`을 핵심 depth gap으로 재고정
- `Event` stable subset에 unsubscribe mismatch / invoke count mismatch negative semantic test와 positive smoke를 추가
- `Set/Map/List` stable subset에 unsupported key kind / wrong set element / wrong list container negative semantic test와 positive smoke를 추가
- tooling은 `있다`와 `완성됐다`를 분리해 `debugger/formatter/LSP`를 별도 축으로 분리
- `debugger`는 단순 스텁이 아니라 `AST-walking source debugger`로, `formatter`는 Windows LF 안정성까지 포함한 basic formatter로, `LSP`는 lightweight semantic tooling으로 재분류
- authoring compression 중 일부는 더 이상 "나중 sugar"가 아니라 실제 depth-closure 수단으로 반영
  - `using <-> where` intent step 상호 유도
  - `refresh/publish/bind ... map { target <- source; }`
  - explicit `Clone(...)` world embedding surface

이번 문서의 의도:
- "무슨 기능이 있나"를 보여주기보다
- "어디가 실제로 닫혀 있고 어디가 비어 있는가"를 보여주는 것

---

## 1.5 베타 전에 반드시 닫아야 하는 축

현재 `◐` 중에서도 다음 네 축은 베타 전 필수 closure 대상이다.

- `Intent/Zone/World orchestration`
- `relation/effect/projection`
- `generic contract`
- `own/ref`

처리 원칙:
- parser가 받는 surface는 semantic/C/LLVM/runtime/test까지 닫는다
- 못 닫는 표면은 문법/시맨틱/문서에서 내린다
- 문서상 지원과 실제 구현이 다르면 구현이 아니라 문서를 내린다

---

## 1. 요약 매트릭스

| 영역 | 파싱 | 시맨틱 | MIR/하강 | C | LLVM | 런타임 | 테스트 | 깊이 판정 | 핵심 메모 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|---|---|
| 기본 코어 (`let/func/if/for/while/match`) | ✅ | ✅ | ✅ | ✅ | ✅ | 해당 없음 | ✅ | 깊음 | 현재 언어의 가장 안정된 축 |
| 타입/제네릭 surface | ✅ | ◐ | ◐ | ◐ | ◐ | 해당 없음 | ◐ | 중간 | exact/ability/multi-bound baseline은 동작하고, default type arg actual resolution도 implemented declaration/call/module-consumer path에서는 회귀로 고정됐다. 남은 것은 broader generic generalization이 아니라 richer mismatch provenance와 broader instantiation-path parity 정렬이다 |
| `subject/class/object/tobject/enum/vessel` | ✅ | ✅ | ✅ | ✅ | ✅ | 해당 없음 | ✅ | 깊음 | 6종 존재론 전부 동작 확인 |
| `ability/role/require/use` 계약 | ✅ | ◐ | ◐ | ✅ | ✅ | 해당 없음 | ◐ | 중상 | `fields` canonical surface, generic ability ref, action/step/zone/party-role contract derivation·diagnostics, ability-bound revalidation은 정렬됨. `override/dyn/extends`는 비코어 축으로 분리 |
| `Slot/SecureSlot/DeviceSlot/QubitSlot` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | 깊음 | 현재 가장 완성도 높은 도메인 축 |
| `Intent` | ✅ | ✅ | ✅ | ✅ | ✅ | ◐ | ✅ | 중상 | 오케스트레이션+상속/유도 강함, `with name: Type;` 값 파라미터 지원 |
| `Zone` | ✅ | ✅ | ✅ | ✅ | ✅ | ◐ | ✅ | 중상 | authority/contract 연결됨, move/clone ownership 정리 중 |
| `World` | ✅ | ✅ | ✅ | ✅ | ✅ | ◐ | ✅ | 중상 | C/LLVM 검증됨, zone embedding ownership 정리 중 |
| `relation/effect/projection` | ✅ | ◐ | ◐ | ✅ | ◐ | ◐ | ✅ | 중상 | stable subset은 declaration/constructor, projection slot family, `refresh/publish/bind`, query family, incremental sync parity, RIR projection/authority/handoff conservative merge helper, authority-bearing lifecycle/projection `Contract source` diagnostics까지 포함한다. 남은 것은 authority-resource-effect 통합과 deeper propagation이다 |
| `Channel/select` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | 깊음 | MPMC+SPSC 런타임, send/recv/select 전부 동작 |
| `Event` | ✅ | ✅ | ◐ | ✅ | ✅ | ◐ | ✅ | 중상 | declaration/subscribe/unsubscribe/invoke 시맨틱과 negative path, positive smoke까지 정렬됐다. invoke의 canonical surface는 parser상 `AST_CALL` 경로다 |
| `Set/Map/List` | ✅ | ✅ | ◐ | ✅ | ✅ | ✅ | ✅ | 중상 | stable subset은 `List<T>`, `Set<T>`, `HashMap<String, T>`, `HashMap<Int, T>`, `HashMap<Long, T>`, `HashMap<Bool, T>`로 닫혔고, 그 외 key 조합은 explicit error다 |
| `Math stdlib` | 해당 없음 | ✅ | 해당 없음 | ✅ | ◐ | ✅ | ◐ | 중상 | Sin/Cos/Sqrt/Pow/Exp/Log/Round/Clamp/PI/E 등 22개 빌트인 |
| `String stdlib` | 해당 없음 | ✅ | 해당 없음 | ✅ | ◐ | ✅ | ◐ | 중상 | Length/Contains/Replace/Substring/Trim/Split/Join/Upper/Lower 10개 |
| `Async/spawn/await` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | 깊음 | pthread 스케줄러+fiber, Future/RemoteFuture 동작 |
| `own/ref` 소유권 | ✅ | ✅ | ✅ | ✅ | ✅ | 해당 없음 | ✅ | 깊음 | 베타 stable subset은 ownership classifier 기준으로 닫혔다. copy-value trivial own/ref, boundary-visible aggregate provenance, movable value transfer/borrow, slot-handle boundary rule, direct/summary helper-chain, destructure/member/container/return/channel 경로가 semantic 회귀로 고정됐다. `Token<T>` escape/transport와 universal ownership lattice만 explicit reject / beta-out-of-scope다 |
| AST dispatch / backend fallback trust | ✅ | 해당 없음 | ✅ | ◐ | ✅ | 해당 없음 | ✅ | 중상 | AST 타입 partition은 문서와 smoke로 고정됐다. LLVM stmt/expr fallback은 structured backend error로 닫혔고, C backend의 동일 수준 fallback audit는 계속 진행한다 |
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
- default type arg는 beta-stable generic surface에서 declaration acceptance가 아니라 actual resolution baseline까지 연결됐다.
- `where T: A + B` 같은 richer bound는 parser가 받을 수 있어도 검증과 활용이 얕다.
- generic ability reference는 구조는 올라왔지만 완전한 declaration/validation 체인은 닫히지 않았다.

판정:
- "쓸 수 있는 것처럼 보이는 generic surface"가 남아 있다.
- 이 영역은 새 문법 추가보다, 닫지 못한 surface를 줄이거나 완성하는 쪽이 먼저다.

### 2.3 존재론 축 (`subject/class/object/tobject/enum`)

파서, 시맨틱, C/LLVM 코드젠까지는 강하다.
다만 표면 설계에서 pain point가 남아 있다.

대표 pain point:
- nominal family token split은 닫혔지만, 예전 alias/shared-token 설명이 문서에 남으면 surface trust를 다시 깎는다
- `object/tobject` surface는 token 문제가 아니라 projection/boundary contract 설명 밀도의 문제다
- nominal family별 진단 문구와 문서 설명의 밀도를 계속 맞춰야 함

판정:
- "구현 depth" 자체는 나쁘지 않다.
- 문제는 correctness보다 surface consistency 쪽이다.

### 2.4 `ability/role/require/use` 계약 축

언어 철학상 핵심인데, 실제 depth는 이제 "중상"으로 봐야 한다.
기본 계약 검증과 코드젠은 있으나, richer contract system은 아직 partially closed 상태다.

최근 닫힌 점:
- `ability<T>` 선언과 참조 baseline은 parser/semantic/DIR 경로까지 연결됐다
- hidden/default-export 규칙과 generic ability 해석의 첫 정렬이 끝났다
- mixed `ability + zone` module에서 explicit export 판정 충돌도 정리됐다
- ability field surface는 `fields`로 완전히 canonicalized 되었고 parser/docs/tests/examples/smoke가 정렬됐다
- action/step/zone authority 계약 키워드군(`requires`, `within`, `authorized by`, `causes`)은 inherited diagnostics와 LSP hover/completion까지 연결됐다
- `public/private`는 이제 nominal 선언을 넘어 `party/roster/world/relation/effect/zone`의 top-level visibility, imported action contract leakage 차단, 그리고 `func/intent/event` callable boundary 차단까지 반영된다

남은 gap:
- 남은 핵심은 `use/require`를 module contract까지 일관되게 올리고 diagnostics/tooling 표현을 정렬하는 일이다
- richer contract summary를 diagnostics/tooling까지 노출하는 작업
- `override/dyn/extends`를 코어 closure와 별도 experimental 축으로 더 명확히 분리하는 문서/정책 정렬

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
실제로 semantic contract derivation/inheritance와 codegen이 붙어 있다.

현재 강한 점:
- orchestration surface 존재
- rollback/cleanup 경로 실구현
- MIR cleanup/rollback/invalidation exceptional topology와 회귀 테스트가 다시 정렬됨
- C/LLVM 양쪽 경로 존재
- 예제와 smoke 범위가 실제로 있다
- `transfer -> using/where`, `using -> where`, `where -> using` 유도가 이미 연결돼 있다

남은 gap:
- `IntentLast*`, `IntentHistoryStep*`, `IntentActive*`, `IntentRecent*`, `IntentCurrentHandle()` / `IntentRecentHandle()` / `IntentRecentTraceId()`, `IntentActiveStep*()`까지의 baseline은 이미 존재한다
- 남은 것은 richer multi-instance timeline query와 failure provenance 정교화다
- distributed/multi-process intent runtime은 아직 범위 밖

판정:
- core compile path는 강하다.
- runtime observability baseline은 존재하며, 더 깊은 query/diagnostic closure가 남아 있다.

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

> PergyraLang은 이제 "넓지만 얕다"가 아니라 "넓고, 핵심은 깊고, 가장자리가 얕다"에 가깝다.

2026-04-11 검증 결과:
- **깊은 축 (7개)**: 기본 코어, 존재론 6종, Slot, Channel, Intent, Zone, Async
- **중상 축 (4개)**: World, Event, Math/String stdlib, relation/effect
- **중간 축 (3개)**: 타입/제네릭, ability/role 계약, Set/Map/List
- **얕은 축 (0개)**: 없음
- **기본 구현 축 (3개)**: debugger, formatter, LSP

이전 평가에서 "❌ 부재"로 표시했던 많은 항목이 실제로는 동작하고 있었다:
- Event C+LLVM 코드젠 완성, arity/type 체크 동작
- Collection LLVM raw_export 전부 구현
- Async pthread 스케줄러+fiber 실구현
- Math/String stdlib 빌트인 30개 이상

---

## 4. 우선순위

### P0 — 실제 남은 빈 칸

| 항목 | 현재 | 목표 |
|------|------|------|
| Map<K,V> Int 키 | String 키만 | Int/enum 키 지원 |
| Intent 값 파라미터 | subject/zone만 | `with price: Int` 지원 |
| refresh map {} | 필드명 정확 일치 | 매핑 문법 |
| Zone→World ownership | 암묵 복사 | move/clone 명시 |
| own/ref 강제 | classifier-backed stable subset | Token<T> transport explicit reject, universal ownership lattice는 beta-out-of-scope |

### P1 — 깊이 보강

- ability/role 계약의 richer diagnostic
- effect lattice 완전판
- relation/effect projection 검증 강화
- intent/zone/world runtime observability

### P2 — 도구 + 편의

- formatter/LSP/debugger 고도화
- authoring scaffold
- 표면 ergonomics

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

상태 조정 (2차):
- `Event`: arity/type 체크가 AST_CALL 경로에서 동작 확인 → 시맨틱 ◐→✅ 상향
- `Set/Map/List`: C+LLVM raw_export 전부 동작 확인 → LLVM ◐→✅ 상향
- `Async/spawn`: pthread 스케줄러+fiber 실구현 확인 → 행 추가, 깊음 판정
- `Math/String stdlib`: 빌트인 30+개 존재 확인 → 행 추가, 중상 판정
- `own/ref`: anchored slot handle 한정 단계는 지났고, classifier-backed stable subset으로 닫혔다. 남은 것은 새 의미론이 아니라 future ownership lattice와 `Token<T>` explicit reject 문서 유지다
- `vessel`: 동작 확인 → 존재론 축에 포함

키워드 감사 (72개):
- 동작 확인: 33+ ALIVE
- 부분 동작: ~30 PARTIAL (contextual keyword 포함)
- 파싱만: dyn, extends, include (contextual, 단독 사용 불가)
- 미구현: embed (토큰 자체 없음)

P0 재정의:
- refresh map, Zone ownership
- P2는 `tooling`과 `authoring shorthand`
