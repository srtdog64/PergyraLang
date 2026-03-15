# Pattern Vault Strategy

## 1. 왜 별도 Vault가 필요한가

intrinsic template만으로는 AI 친화 표면 API를 만들 수 있지만, 재사용 가능한 프로그래밍 패턴 자체를 체계적으로 쌓기에는 부족하다.

따라서 Pergyra는 다음 두 축을 분리한다.

- `vault/` -- 도메인 독립적인 제네릭 패턴 저장소
- `intrinsic template` -- AI가 짧게 호출할 수 있는 컴파일러 인지 표면 API

이 분리는 역할을 명확하게 만든다.

- vault는 지식과 구조를 저장한다.
- intrinsic은 자주 쓰는 일부 vault 패턴을 언어 표면으로 승격한다.

## 2. 핵심 철학

Pergyra Pattern Vault는 다음 원칙을 따른다.

### 2.1 Generic-to-Domain Injection

먼저 도메인을 제거한 패턴을 만든다.
그 다음 사용 시점에 타입과 정책을 주입한다.

예:

```pergyra
// 제네릭 패턴
AsyncHandler<TInput, TOutput, TError>
StateMachine<TState, TEvent, TContext>
ThreadSafeCache<TKey, TValue>

// 도메인 주입
AsyncHandler<UserId, UserProfile, FetchError>
StateMachine<OrderState, OrderEvent, OrderContext>
ThreadSafeCache<ProductId, Product>
```

### 2.2 개념 우선 분류

Finance, User, Order 같은 도메인 이름으로 분류하지 않는다.
`handle-async`, `cache-data`, `track-state`, `route-requests` 같은 개념 이름으로 저장한다.

### 2.3 언어 라이브러리와 문서를 함께 관리

패턴은 코드 조각만 보관하지 않는다.
다음이 함께 있어야 한다.

- 패턴 설명
- 제네릭 시그니처
- 도메인 주입 예시
- 조합 규칙
- 제약사항

## 3. 저장소 내 역할 분리

### 3.1 `vault/`

패턴 저장소 본체다.

- 개념별 패턴 문서
- 언어별 구현 또는 스켈레톤
- 조합 시나리오
- registry 메타데이터

### 3.2 `docs/intrinsic_templates/`

컴파일러가 어떤 패턴을 intrinsic으로 끌어올릴지 문서화하는 곳이다.

### 3.3 컴파일러

특정 vault 패턴을 intrinsic template 또는 stdlib surface로 연결한다.

즉, 모든 vault 패턴이 intrinsic이 되는 것은 아니다.

## 4. 권장 구조

```text
vault/
  patterns/
    handle-async/
      handle-async.md
      pgy/
    cache-data/
      cache-data.md
      pgy/
  combinations/
    resilient-client/
      resilient-client.md
  registry/
    patterns.json
    combinations.json
```

## 5. 어떤 패턴을 먼저 넣을 것인가

초기에는 Pergyra와 결이 맞는 패턴부터 넣는다.

1. `handle-async`
2. `cache-data`
3. `track-state`
4. `retry-operations`
5. `route-requests`

이 다섯 개는 다음 장점이 있다.

- 도메인 독립적이다.
- 제네릭 추상화가 자연스럽다.
- AI가 자주 생성하는 보일러플레이트를 줄일 수 있다.
- 나중에 intrinsic template로 승격할 후보가 된다.

## 6. intrinsic과의 연결 방식

연결 방식은 세 단계로 나눈다.

### 6.1 Vault Pattern

사람과 AI가 참고하는 일반 패턴 문서/라이브러리다.

예:

- `vault/patterns/handle-async`

### 6.2 Standard Library Surface

충분히 안정화된 패턴은 일반 라이브러리 API로 제공한다.

예:

```pergyra
let handler = AsyncHandler<UserId, UserProfile, FetchError>(FetchUser);
```

### 6.3 Intrinsic Template

호출 빈도가 높고 확장 가치가 큰 패턴만 컴파일러 intrinsic으로 승격한다.

예:

```pergyra
let profile = await HttpGetJson<UserProfile>(url);
```

즉, vault가 상위 집합이고 intrinsic은 선택된 하위 집합이다.

## 7. 설계상 결론

Pergyra는 "언어 하나"가 아니라 다음 세 층으로 간다.

1. 컴파일러와 런타임
2. intrinsic template
3. generic-to-domain injection pattern vault

이 구조가 있어야 AI 친화성, 재사용성, 도메인 독립성을 동시에 잡을 수 있다.
