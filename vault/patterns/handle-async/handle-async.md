# handle-async

## 목적

비동기 입력을 받아 처리한 뒤 결과 또는 오류를 반환하는 일반 패턴을 정의한다.

이 패턴은 특정 도메인에 묶이지 않는다.

## 제네릭 형태

```pergyra
AsyncHandler<TInput, TOutput, TError>
```

## 의미

- `TInput` -- 처리 대상 입력
- `TOutput` -- 성공 시 결과
- `TError` -- 실패 시 오류 타입

## 도메인 주입 예시

```pergyra
AsyncHandler<UserId, UserProfile, FetchError>
AsyncHandler<ProductId, Product, LookupError>
AsyncHandler<OrderRequest, OrderReceipt, OrderError>
```

## 기대 동작

- 비동기 처리 진입점 제공
- 성공/실패 결과를 명확히 구분
- 재시도, 캐시, 로깅 패턴과 조합 가능

## 조합 후보

- `retry-operations`
- `cache-data`
- `monitor-health`

## Pergyra 표면 예시

```pergyra
let handler: AsyncHandler<UserId, UserProfile, FetchError>;
```

이 패턴은 이후 stdlib 또는 intrinsic template로 승격될 수 있다.
