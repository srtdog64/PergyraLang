# Pergyra Pattern Vault

Pergyra Pattern Vault는 도메인을 제거한 재사용 가능한 프로그래밍 패턴을 저장하고, 사용 시점에 타입과 정책을 주입하는 라이브러리 계층이다.

핵심 철학은 `generic-to-domain injection`이다.

```pergyra
// 제네릭 패턴
AsyncHandler<TInput, TOutput, TError>
ThreadSafeCache<TKey, TValue>
StateMachine<TState, TEvent, TContext>

// 도메인 주입
AsyncHandler<UserId, UserProfile, FetchError>
ThreadSafeCache<ProductId, Product>
StateMachine<OrderState, OrderEvent, OrderContext>
```

## 원칙

- 도메인 이름보다 기능 이름으로 분류한다.
- 패턴은 문서와 구현 스켈레톤을 함께 둔다.
- 작은 패턴을 조합해 큰 시나리오를 만든다.
- 모든 패턴을 intrinsic으로 만들지 않는다.
- 컴파일러 intrinsic은 vault 패턴 중 일부만 승격한다.

## 구조

```text
vault/
  patterns/
    handle-async/
      handle-async.md
      pgy/
  combinations/
    resilient-client/
      resilient-client.md
  registry/
    patterns.json
    combinations.json
```

## 초기 우선순위

1. `handle-async`
2. `cache-data`
3. `track-state`
4. `retry-operations`
5. `route-requests`

## intrinsic과의 관계

- vault는 패턴 지식 베이스다.
- stdlib은 안정화된 일반 표면 API다.
- intrinsic template는 AI 사용성이 높고 확장 가치가 큰 일부 패턴만 컴파일러가 직접 인식한다.
