# Initial Intrinsic Template Catalog

## 1. 목적

초기 카탈로그는 "지금 당장 전부 구현할 목록"이 아니라, AI 사용성과 엔진 코어 방향을 기준으로 우선순위를 고정하는 문서다.

## 2. 선정 기준

- AI가 반복적으로 생성할 가능성이 높다.
- 호출 규약이 비교적 안정적이다.
- 확장 후 일반 코드로 환원하기 쉽다.
- 런타임 또는 엔진 통합 규약을 숨기되 의미를 파괴하지 않는다.

## 3. 1차 후보

### 3.1 `HttpGetJson<T>(url)`

의도:

- HTTP GET
- 응답 상태 확인
- JSON 디코드
- `Result<T, Error>`로 포장

표면 예시:

```pergyra
let profile = await HttpGetJson<PlayerProfile>(profileUrl);
```

적합한 이유:

- AI가 자주 쓰는 패턴이다.
- 보일러플레이트가 길다.
- async, 오류 처리, 디코드 규약을 함께 묶기 좋다.

### 3.2 `EventHandler<E>(handler)`

의도:

- 이벤트 핸들러 등록 구조 생성
- 이벤트 payload 타입 연결
- 디스패치 테이블 또는 래퍼 생성

표면 예시:

```pergyra
let onHit = EventHandler<PlayerHit>(HandlePlayerHit);
```

적합한 이유:

- 엔진 코드에서 반복 빈도가 높다.
- 타입 연결이 중요하다.
- 핸들러 래핑 규약을 중앙에서 관리할 수 있다.

### 3.3 `SystemQuery<C1, C2, ...>()`

의도:

- 시스템이 요구하는 컴포넌트 셋 선언
- 내부 반복 구조 또는 뷰 구성

표면 예시:

```pergyra
let movers = SystemQuery<Position, Velocity>();
```

적합한 이유:

- ECS 계열 코드에서 패턴이 반복된다.
- AI가 직접 반복문, 필터, 바인딩 구조를 매번 쓰는 것보다 안정적이다.

### 3.4 `TaskGroup<T>()`

의도:

- 구조화된 동시성 작업 그룹 생성
- 수집/조인 기본 규약 연결

표면 예시:

```pergyra
let group = TaskGroup<Result<Int, Error>>();
```

적합한 이유:

- async 설계와 맞닿아 있다.
- 병렬/비동기 보일러플레이트를 줄일 수 있다.

## 4. 보류 후보

다음 후보는 매력적이지만 초기에 넣기엔 범위가 크다.

- `Component<T>()`
- `Service<T>()`
- `Repository<T>()`
- `Derive*` 계열 자동 생성

이들은 범용 메타프로그래밍 또는 런타임/프레임워크 결합도가 높아 초기 intrinsic 축을 불필요하게 무겁게 만들 수 있다.

## 5. 첫 구현 우선순위

문서 기준 첫 구현 순서는 다음을 권장한다.

1. `HttpGetJson<T>`
2. `EventHandler<E>`
3. `SystemQuery<C...>`

이 순서는 다음 이유를 가진다.

- AI 사용성이 크다.
- 표면 API가 짧다.
- 성공 시 intrinsic template 축의 유효성을 빠르게 검증할 수 있다.

## 6. Registry 초안

향후 compiler registry는 최소한 아래 메타데이터를 가져야 한다.

- 이름
- kind
- generic parameter count
- value parameter count
- required context
- expansion strategy
- diagnostics label

예시 개념:

```text
HttpGetJson
  kind: network
  generics: 1
  args: 1
  context: async
  expands_to: request + decode + result-wrap
```
