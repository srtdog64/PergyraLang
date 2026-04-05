# Hands-On Language Assessment

마지막 업데이트: 2026-04-06

이 문서는 실제로 `intent`, `zone/world`, 대형 시나리오, pattern 예제,
C/LLVM parity를 직접 구현하면서 드러난 Pergyra의 강점과 약점을 정리한다.

## 우선순위 정리

### P0 — 당장 막는 허점

이 단계의 기준은 단순하다.

- 실전 예제를 쓰다가 바로 막히는가
- 문법/시맨틱/코드젠 중 하나가 비어 있어서 일반적인 앱 코드를 못 쓰는가
- C/LLVM practical parity가 깨져서 기본 회귀가 흔들리는가

현재 P0는 이렇다.

| 항목 | 상태 | 메모 |
|------|------|------|
| `for-in` / `List<T>` / `${expr}` | 해결 | 캘린더/실전 예제로 확인 |
| `HashMap<String, Subject/Class>` | 해결 | practical C/LLVM parity 확보 |
| `use datetime;` | 해결 | 실제 stdlib module화 완료 |
| `type alias` (`type UserId = Int`) | 해결 | parser/semantic/C/LLVM lowering 연결 |
| `page/report/api adapter` 최소 표면 | 해결 | 쇼핑몰 예제로 실제 사용 |
| collection/runtime practical LLVM parity | 해결 | practical examples 기준 닫힘 |

즉 지금의 P0는 더 이상 “캘린더조차 못 만든다” 류가 아니다.
이번 패스에서 `type alias`까지 들어가면서, 일반적인 앱/시뮬레이터를 막는
직접적인 문법 공백은 많이 줄었다.

### P1 — 6개월 안에 채워야 할 공백

이 단계는 “돌아가긴 하지만 계속 아프다”에 해당한다.

- richer stdlib (`http/storage/page/datetime`) 두께
- iterator/protocol/collection ergonomics
- generic constraint depth
- function value / higher-order ergonomics 고도화
- diagnostics / tooling / scaffold 성숙도
- projection/binding grouped surface
- richer app adapter patterns

즉 P1은 존재론이 아니라 생산성과 라이브러리 두께를 끌어올리는 단계다.

### P2 — 장기 연구 과제

이 단계는 언어 철학을 더 밀어붙이는 연구 주제다.

- richer intent orchestration engine
- deeper rollback / compensation model
- richer cross-world identity semantics
- long-running multi-instance orchestration
- deeper effect/cost model
- distributed world semantics
- generalized resource protocol beyond current slot/future/channel surface

즉 P2는 “당장 못 써서 아픈 것”이 아니라,
Pergyra가 어디까지 자기 철학을 밀고 갈 수 있는가의 문제다.

## 강점

### 1. 존재론이 크고 복잡한 코드를 덜 섞이게 한다

`subject / class / vessel / object / tobject / relation / effect / zone / world`
구분은 실제로 효과가 있다.

- `subject`: 누가 움직이는가
- `class`: 무엇을 들고 쓰는가
- `vessel`: subject 내부 피동 수용체
- `object/tobject`: projection
- `zone/world`: 규칙과 실행 경계

게임/시뮬레이터처럼 상태가 많은 코드를 만들 때,
이 구분이 없으면 결국 `god object`나 무정형 service 더미가 생긴다.
Pergyra는 이 부분이 비교적 잘 버틴다.

### 2. intent-first 설계는 프로젝트의 목차를 만든다

`intents/`에서 먼저 “무엇을 하려 하는가”를 선언하고,
거기서 `subject`, `zone`, `ability`, `effect`를 역산하는 방식은
실제로 TODO 생성기처럼 작동한다.

이 점은 일반 OOP보다 강하다.

- class 먼저 만들면 나머지 필요 요소가 잘 안 드러난다
- intent 먼저 만들면 필요한 actor/zone/ability/effect가 자동으로 보인다

### 3. domain contract를 정적으로 검증하는 방향이 실전 가치가 있다

다음 계열은 실제로 실수를 줄였다.

- `requires`
- `authorized by`
- `causes`
- `within/where`
- `who -> zone subject slot`

특히 intent, action, zone authority가 연결되기 시작하면서
“문서에만 있는 규칙”이 아니라 컴파일러가 이해하는 규칙으로 변했다.

### 4. C/LLVM 이중 백엔드가 설계 오류를 빨리 드러낸다

backend parity를 계속 맞추는 과정에서

- stale nominal tracking
- string equality pointer compare
- constructor/default-state parity
- slot sugar / builtin divergence
- intent zone binding

같은 문제가 빨리 드러났다.

즉 두 백엔드를 유지하는 비용은 크지만,
언어 의미론 검증기로도 작동한다.

## 약점

### 1. 일반 프로그래밍 인프라 — 대부분 해결됨 (2026-04-06 재평가)

이전에는 "일반 언어 인프라가 부족하다"고 판단했으나, 실제 검증 결과:

- `for-in` 루프: **동작** (Array, Slice, List 순회)
- 제네릭 컬렉션: **동작** (`List<Event>`, `HashMap<String, Subject>` 등)
- 문자열 보간: **동작** (`"${expr}"`)
- 고차함수/함수값: **동작** (function-typed local, return)
- nested generic: **동작** (`HashMap<String, List<String>>`)
- datetime: **동작** (`use datetime;`)

**남은 약점:**
- `Optional<T>` / nullable 패턴이 아직 없음
- iterator protocol이 사용자 정의 타입으로 확장되지 않음 (built-in만)

### 2. projection/binding은 여전히 wiring-heavy하다

`bind`로 많이 나아졌지만, 큰 시나리오를 작성하면
projection/state propagation은 아직 수작업이 많다.

남은 과제:

- grouped binding
- 더 높은 수준의 propagation surface
- structured trace/history API

### 2.5. async resource discipline은 강해졌지만 아직 sharp edge가 있다

`examples/resource_scheduler_async_probe/`를 만들면서 확인한 점은 분명하다.

- `Channel<Int>` + `parallel` + `Slot<subject>` + `DeviceSlot<Int>` +
  `RemoteFuture<Int>`를 한 시나리오에서 실제로 돌릴 수 있다
- helper function이 `ref Slot<subject>`를 받는 경우까지 병렬 conflict를
  추적하게 되면서, "겉으로는 함수 호출"인 변이도 보수적으로 막을 수 있다

하지만 아직 남아 있는 sharp edge도 있다.

- `spawn` + `ref Slot<subject>` async parameter wrapper path
- `SecureSlot` view mutation across `await`

즉 "엄격한 자원관리"의 방향은 맞지만, 가장 거친 경계는 아직 async wrapper와
secure-view 쪽이다.

### 3. intent runtime은 강해졌지만 아직 fully rich engine은 아니다

현재는 이미 다음이 된다.

- callable intent
- repeated `on`
- `pre/guard/post/invariant/expect`
- `exclusive/concurrent/priority`
- reverse-order `compensate`
- live zone binding
- actor-to-zone-slot materialization

하지만 아직 남은 것이 있다.

- cross-world transfer
- richer rollback policy
- typed trace/history beyond the current last-intent step surface
- long-running intent instance orchestration

즉 “선언 + 실행”은 되었지만,
“완성형 orchestration engine”은 아직 아니다.

### 4. compiler complexity가 빠르게 커진다

Pergyra는 좋은 아이디어를 넣을수록
parser/semantic/codegen/runtime/LSP/doc/scaffold를 같이 흔든다.

특히 intent는 그 대표 사례였다.

- declaration만 있을 때는 쉬움
- executable intent가 되면 HIR/codegen/runtime까지 필요
- conflict scheduler를 넣으면 runtime registry 필요
- zone binding을 넣으면 backend lowering이 같이 필요

즉 철학적으로 강한 기능은 구현 비용도 크다.

### 5. 표면 문법은 강하지만, 문서와 예전 설명이 아직 완전히 수렴하지 않았다

실제 구현을 계속 만지다 보니 "언어 표면이 이상하다"기보다
"오래된 문서가 최신 surface를 완전히 따라오지 못하는" 지점이 더 눈에 띄었다.

대표적으로:

- 일부 오래된 문서는 아직 `subject`와 `class`를 같은 선언처럼 설명한다
- page/zone 관계를 예전에는 1:1처럼 설명한 흔적이 남아 있다
- `intent`는 이제 executable orchestration인데, 몇몇 설명은 여전히 declaration-only처럼 읽힌다

즉 현재의 surface weirdness는 parser보다 문서 동기화 문제에 더 가깝다.
이건 언어 설계 자체의 허점이라기보다, 진화 속도를 문서가 못 따라온 결과다.

### 6. debug/AST printer 표면은 아직 군데군데 거칠다

이번 패스에서 escaped string literal과 `enum` / `break` / `continue`
pretty-print는 바로 고쳤지만, 아직 AST debug 출력 자체는 완전히 매끈하지 않다.

대표적으로:

- nested generic type pretty-print가 너무 장황하다
- 일부 node는 여전히 compact printer에서 `<node:...>` 같은 내부 표현을 노출한다
- event parameter나 array literal 일부 표현은 surface syntax보다 AST storage 형태를 더 드러낸다

즉 parser/semantic이 틀린 것이 아니라, debug view가 언어 표면과 1:1로 대응하지 않는
부분이 남아 있다. 이건 diagnostics와 학습 경험에 직접 영향을 주므로
계속 다듬어야 한다.

## 직접 구현해보며 느낀 결론

### 잘하는 것

Pergyra는 지금

- 상태가 많은 domain code
- orchestration이 중요한 code
- 게임/시뮬레이터/transaction flow
- “누가/어디서/어떤 자격으로”가 중요한 code

를 구조적으로 다루는 데 강점이 있다.

### 약한 것 (2026-04-06 재평가)

이전에 "일반 인프라 약함"으로 판단했던 항목 대부분이 해결됨.
현재 실제로 약한 부분:

- Optional/nullable 패턴 부재
- 사용자 정의 iterator protocol 없음
- cross-world intent transfer 미구현
- long-running intent orchestration 미구현

## 캘린더 예제 — 구체적 발견 (2026-04-05)

### 예제: `examples/calendar/`

intent-first로 캘린더 앱을 설계하면서 발견한 것.

### 강점 확인

**1. intents/ 폴더만 읽으면 앱 목적이 보인다**

```
intents/manage_event.pgy  → "일정을 만들고 수정하고 삭제하고 싶다"
intents/view_schedule.pgy → "일정을 보고 싶다"
```

OOP라면 CalendarService, EventRepository, CalendarController를 다 읽어야 알 수 있는 걸, 2개 파일 이름만으로 안다.

**2. action 계약이 자기 문서화**

```pergyra
action DeleteEvent(self, event_id: Int)
    requires CalendarOwner
    within CalendarZone
    authorized by owner
```

별도 권한 문서 없이 "누가, 어디서, 어떤 자격으로" 삭제할 수 있는지 선언에 보인다.

**3. concurrent/exclusive가 읽기/쓰기 정책을 선언**

```pergyra
intent ManageEvent { exclusive; }     // 수정 중 다른 수정 불가
intent ViewSchedule { concurrent; }   // 보기는 동시에 가능
```

### 약점 발견 — P0 (이전 상태, 2026-04-05 이전)

이 항목들은 직접 캘린더를 만들어보며 실제로 막혔던 지점이었지만,
지금은 일부가 해결되었다.

| 해결된 기능 | 현재 상태 |
|------------|----------|
| **while** | 이미 지원됨 |
| **for-in** | `Array<T>`, `Slice<T>`, `List<T>` 순회 지원 |
| **제네릭 컬렉션** | C backend 기준 `List<Event>`, `HashMap<String, Subject>` 등 가능 |
| **문자열 보간** | `${expr}` 지원, embedded expression은 `ToString(...)`으로 lowering |

현재 캘린더를 막는 최상위 P0는 거의 닫혔고,
남은 문제는 “문법 부재”보다 richer stdlib와 app ergonomics 쪽이다.

| 남은 P0 | 현재 상태 | 필요한 것 |
|--------|----------|----------|
| **LLVM 컬렉션 parity** | 실전 예제 기준 해결 | `List<T>`와 `HashMap<String, Int/Class/Subject>`가 practical examples에서 C/LLVM 둘 다 동작 |
| **datetime 표준층 부재** | 해결 | `use datetime;`가 실제 stdlib module로 동작 |
| **UI/report ergonomics 부족** | 부분 해결 | `shopping_mall_checkout_refund`에 `api/` / `report/` adapter layer 추가, page/report library는 더 두꺼워질 필요 |

즉 현재 판단은 이렇다.

- `for-in` / `List<T>` / `${expr}` 자체는 더 이상 P0가 아니다
- `HashMap<String, Subject>`도 더 이상 C-only가 아니고, 실전 예제 기준으로 LLVM까지 닫혔다
- `use datetime;`도 문서 placeholder가 아니라 실제 stdlib surface가 되었다
- `type alias`도 이제 실제 parser/semantic/C/LLVM vertical slice로 들어왔다
- 이제 남은 문제는 “캘린더조차 못 만든다” 류의 P0라기보다, richer stdlib와 app/report ergonomics다

## Adapter-heavy 예제 — 추가 확인 (2026-04-05)

### 예제: `examples/adapter_policy_stack/`

이 예제는 세 가지를 동시에 밀어본 회귀 자산이다.

- function-typed local / return
- nested generic parsing and lowering
- `page / api / report`의 라이브러리형 분리

### 이번에 닫힌 실제 결함

- C backend:
  - function-typed local과 function-returning-function 선언이 `void *`/`int32_t`로 잘못 내려가던 문제를 수정
- parser:
  - `HashMap<String, List<String>>` 같은 nested type arguments가 선언용 generic parser를 잘못 타던 문제를 수정
- LLVM backend:
  - collection-typed / function-typed parameter가 scope에 들어올 때 메타 등록이 누락되어 `ListGet(lines, 0)`나 local function call이 잘못 추론되던 문제를 수정
- C backend:
  - nested specialized collection type이 함수 시그니처에 직접 들어갈 때
    forward declaration이 specialized typedef보다 먼저 나와 깨지던 경로를 수정

### 현재 판단

이제 practical example 기준으로는

- function value
- nested generic collection
- adapter-heavy composition

까지도 C/LLVM 둘 다 “돌아가는 수준”에는 올라왔다.

### 이전에 "약점"으로 잘못 판정했던 항목 — 실제로는 동작함 (2026-04-06 검증)

`examples/calendar_working/main.pgy`에서 전부 동작 확인:

| 항목 | 이전 판정 | 실제 상태 |
|------|----------|----------|
| **for-in 루프** | P0 — 안 됨 | `for event in events { }` 동작 |
| **List\<Event\>** | P0 — 안 됨 | `let events: List<Event> = ListNew();` 동작 |
| **문자열 보간** | P0 — 안 됨 | `"total: ${ListSize(events)}"` 동작 |
| **Date/Time** | P1 — 없음 | `use datetime;` + `LocalDate`, `DateTime` 동작 |
| **Bool + 논리연산** | P1 — 미확인 | `event.IsOnDate(today)` 동작 |
| **생성자 문법** | P2 — 불명확 | `Event("title", "note", dt, 60)` 동작 |
| **중첩 접근** | P2 — 불명확 | `at.IsOnDate(date)` 동작 |

### 이전 P1/P2 — 전부 이미 구현됨 (2026-04-06 검증)

| 항목 | 이전 판정 | 실제 상태 |
|------|----------|----------|
| **Optional/Nullable** | P1 — 없음 | `Option<T>`, `Some()`, `None()`, `?` 연산자, match 패턴 전부 동작 |
| **struct/class 생성** | P2 — 불명확 | `Event("title", "note", dt, 60)` 생성자 동작 |
| **중첩 접근 a.b.c** | P2 — 불명확 | `hero.health.current`, `self.defender.health.current` 동작 |

**Optional 구현 상세:**
- `Option<T>` 타입, `Some(value)`, `None()` 생성자
- `IsSome()`, `IsNone()`, `UnwrapOption()` 함수
- `match` 패턴: `Some(v)` / `None` 분기
- `?` 연산자 (postfix try — early return)
- `Result<T>` + `Ok(v)` / `Err(msg)` / `Unwrap()` / `UnwrapOr()` 도 있음
- 예제: `examples/option_test.pgy`, `examples/pipe_and_try.pgy`

**이 섹션에 나열된 약점은 전부 해소됨. 더 이상 P0/P1/P2 약점 없음.**

### 핵심 관찰

```
Pergyra로 캘린더 예제를 만들면:

선언 단계:
  "이 앱은 캘린더다. 사용자가 일정을 만들고 수정하고 삭제할 수 있다.
   CalendarZone에서 CalendarOwner 자격으로 소유자 승인 하에."
  → 선언할 수 있다. 기존 어떤 언어보다 명확하다.

구현 단계:
  "이벤트 목록을 순회하며 오늘 날짜의 이벤트를 필터링해서
   '2026-04-05 팀 회의 (14:00-15:00)' 형식으로 출력한다."
  → 이제 C backend 기준으로는 구현 가능하다.

실제 working 예제:
- [`examples/calendar_working/main.pgy`](/mnt/e/PergyraLang/examples/calendar_working/main.pgy)

선언도 되고, 구현도 된다.
```

---

## 현재 판단 (2026-04-06 재평가)

> Pergyra는 도메인 선언과 일반 프로그래밍 인프라 양쪽이 실용 수준에 도달했다.
> for-in, 제네릭 컬렉션, 문자열 보간, datetime, 고차함수가 전부 동작한다.
> 남은 과제는 Optional/nullable, 사용자 정의 iterator, cross-world intent다.
