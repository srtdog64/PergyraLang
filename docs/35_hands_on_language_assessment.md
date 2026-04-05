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

## 현재 판단

한 줄로 정리하면 이렇다.

> Pergyra는 이미 “도메인 오케스트레이션 언어”로서의 축은 분명하다.
> 다만 그 위상을 완전히 살리려면, 일반 프로그래밍 인프라와
> structured runtime observability를 더 보강해야 한다.
