# Hands-On Language Assessment

마지막 업데이트: 2026-04-05

이 문서는 실제로 `intent`, `zone/world`, 대형 시나리오, pattern 예제,
C/LLVM parity를 직접 구현하면서 드러난 Pergyra의 강점과 약점을 정리한다.

## 강점

### 1. 존재론이 크고 복잡한 코드를 덜 섞이게 한다

`subject / class / vessel / object / dto / relation / effect / zone / world`
구분은 실제로 효과가 있다.

- `subject`: 누가 움직이는가
- `class`: 무엇을 들고 쓰는가
- `vessel`: subject 내부 피동 수용체
- `object/dto`: projection
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

### 1. 일반 프로그래밍 인프라가 아직 약하다

직접 큰 예제와 패턴을 만들수록 이 점이 반복해서 드러났다.

- 고차함수/함수값은 이제 막 올라오기 시작한 수준
- 제네릭은 깊게 들어가면 금방 제약이 보인다
- 문자열/컬렉션 ergonomics가 아직 거칠다
- iterator/protocol 류는 얕다

즉 domain DSL은 점점 강해지는데,
그 밑에서 받쳐주는 일반 언어 인프라는 아직 덜 성숙했다.

### 2. projection/binding은 여전히 wiring-heavy하다

`bind`로 많이 나아졌지만, 큰 시나리오를 작성하면
projection/state propagation은 아직 수작업이 많다.

남은 과제:

- grouped binding
- 더 높은 수준의 propagation surface
- structured trace/history API

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

## 직접 구현해보며 느낀 결론

### 잘하는 것

Pergyra는 지금

- 상태가 많은 domain code
- orchestration이 중요한 code
- 게임/시뮬레이터/transaction flow
- “누가/어디서/어떤 자격으로”가 중요한 code

를 구조적으로 다루는 데 강점이 있다.

### 약한 것

반대로 지금은

- 일반-purpose application language
- 고차함수 중심의 추상화
- 데이터 구조/알고리즘 authoring ergonomics
- 작은 코드도 아주 간단하게 쓰는 생산성

에서는 아직 거친 부분이 있다.

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

현재 캘린더를 막는 최상위 P0는 “문법 부재”보다 backend parity와 richer stdlib 쪽이다.

| 남은 P0 | 현재 상태 | 필요한 것 |
|--------|----------|----------|
| **LLVM 컬렉션 parity** | 실전 예제 기준 해결 | `List<T>`와 `HashMap<String, Int/Class/Subject>`가 practical examples에서 C/LLVM 둘 다 동작 |
| **datetime 표준층 부재** | 해결 | `use datetime;`가 실제 stdlib module로 동작 |
| **UI/report ergonomics 부족** | 부분 해결 | `shopping_mall_checkout_refund`에 `api/` / `report/` adapter layer 추가, page/report library는 더 두꺼워질 필요 |

즉 현재 판단은 이렇다.

- `for-in` / `List<T>` / `${expr}` 자체는 더 이상 P0가 아니다
- `HashMap<String, Subject>`도 더 이상 C-only가 아니고, 실전 예제 기준으로 LLVM까지 닫혔다
- `use datetime;`도 문서 placeholder가 아니라 실제 stdlib surface가 되었다
- 이제 남은 문제는 “캘린더조차 못 만든다” 류의 P0라기보다, richer stdlib와 app/report ergonomics다

### 약점 발견 — P1 (캘린더는 만들 수 있지만 고통스러움)

| 부족한 기능 | 구체적 장면 | 필요한 것 |
|------------|-----------|----------|
| **Date/Time 타입** | `use datetime;`로 해결 | richer date arithmetic는 차후 |
| **Optional/Nullable** | "선택된 이벤트 없음" 표현 불가 | `T?` 또는 `Optional<T>` |
| **Bool + 논리연산** | `&&`, `\|\|`, `!` 미확인 | 조건 결합 |

### 약점 발견 — P2 (불편하지만 우회 가능)

| 부족한 기능 | 구체적 장면 | 필요한 것 |
|------------|-----------|----------|
| struct/class 리터럴 생성 | `Event { title: "회의" }` 가능한가 | 구조체 리터럴 |
| 중첩 접근 | `event.date.year` 가능한가 | a.b.c 패턴 |

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

설계도는 완벽한데 벽돌이 없는 상태.
```

---

## 현재 판단

한 줄로 정리하면 이렇다.

> Pergyra는 이미 “도메인 오케스트레이션 언어”로서의 축은 분명하다.
> 다만 그 위상을 완전히 살리려면, 일반 프로그래밍 인프라와
> structured runtime observability를 더 보강해야 한다.
