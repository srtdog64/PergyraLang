# Pergyra 에러 처리 시스템

마지막 업데이트: 2026-04-03

이 문서는 현재 구현 기준으로 정리한 에러 처리 표면이다. 과거 설계 문서에 있던 `Result<T, E>`, `try/catch`, 에러 매크로 시스템은 아직 현재 구현 기준의 stable feature가 아니다.

## 0. 실패 분류 기준

Pergyra는 모든 실패를 같은 방식으로 처리하지 않는다.

- `recoverable failure`
  - 사용자 코드가 예상 가능한 실패
  - 기본 정책: 값을 통해 반환하고, 필요하면 reason/state를 조회 가능하게 남긴다
  - 예: intent failure, authority rejection, timeout, remote failure
- `contract violation`
  - semantic 단계에서 막는 것이 원칙
  - 런타임까지 도달하면 structured panic 대상이다
  - 예: released slot access, invalid secure token
- `internal bug`
  - 컴파일러/런타임 자체 불변식 파손
  - 즉시 중단하며, 사용자 도메인 실패처럼 위장하지 않는다

중요:
- `Result<T>`는 recoverable path의 기본 수단이다
- `Unwrap(...)`는 recoverable failure를 조용히 삼키는 API가 아니라 panic 성격의 sharp tool이다

## 1. 현재 구현 중심

현재 코드와 테스트가 실제로 보장하는 축은 다음이다.

- `Result<T>` 타입
- `Ok(...)`, `Err(...)`
- `IsOk`, `IsErr`, `Unwrap`, `UnwrapOr`
- `Option<T>` 타입
- `Some(...)`, `None()`
- `IsSome`, `IsNone`, `UnwrapOption`
- postfix `?` 조기 반환
- `RemoteFuture<T>`를 `await`했을 때 `Result<T>`가 되는 규칙

## 2. Result<T>

현재 예제와 프론트엔드 경로는 `Result<T>`를 중심으로 사용한다.

canonical 이름 규칙:
- `Unwrap(result)` — 성공값 추출
- `UnwrapOr(result, fallback)` — 실패 시 대체값
- `expr?` — 실패 시 현재 함수에서 즉시 반환
- `UnwrapResult(...)`는 현재 surface에 없다

권장:
- recoverable flow의 기본은 `IsOk/IsErr`, `UnwrapOr`, `?`
- `Unwrap(...)`는 실패가 논리적으로 불가능하거나 개발용 crash-fast가 필요한 지점에서만 사용

```pergyra
func SafeDiv(a: Int, b: Int) -> Result<Int> {
    if b == 0 {
        return Err("division by zero");
    }
    return Ok(a / b);
}

func Main() -> Void {
    let r1: Result<Int> = SafeDiv(10, 2);
    let r2: Result<Int> = SafeDiv(10, 0);

    Log(Unwrap(r1));
    Log(UnwrapOr(r2, -1));
}
```

현재 구현에서 에러 값은 런타임/코드젠 경로상 문자열 기반 `Err(...)` 사용이 중심이다.

## 3. Try 연산자 `?`

`expr?`는 `Result<T>`에서 성공값을 꺼내고, 실패면 현재 함수를 즉시 반환한다.

```pergyra
func Validate(x: Int) -> Result<Int> {
    if x > 100 {
        return Err("too large");
    }
    return Ok(x);
}

func Process(x: Int) -> Result<Int> {
    let doubled = x * 2;
    let validated = Validate(doubled)?;
    return Ok(validated + 10);
}
```

이 경로는 semantic, C backend, LLVM backend 테스트에 반영되어 있다.

## 4. RemoteFuture<T> 와 Result<T>

로컬 `Future<T>`와 원격 `RemoteFuture<T>`는 `await` 결과가 다르다.

- `Future<T>` → `await` → `T`
- `RemoteFuture<T>` → `await` → `Result<T>`

```pergyra
async func ConsumeRemote(pending: RemoteFuture<Int>) -> Void {
    let result: Result<Int> = await pending;

    if IsOk(result) {
        Log(Unwrap(result));
    } else {
        Log(UnwrapOr(result, -1));
    }
}
```

이 규칙은 현재 Pergyra의 중요한 안정된 의미론 중 하나다.

## 5. match 와 Result 패턴

`Result<T>`는 `match`에서 `Ok(...)` / `Err(...)`로 destructuring 가능하다.

```pergyra
func Handle(result: Result<Int>) -> Void {
    match result {
        case .Ok(value):
            Log(value);
        case .Err(error):
            Log(error);
    }
}
```

## 6. Option<T>

`Option<T>`는 현재 source-level 표면에 포함된다. 기본 축은 `Some(...)`, `None()`, `IsSome`, `IsNone`, `UnwrapOption`, 그리고 `match` destructuring이다.

```pergyra
func FindPositive(n: Int) -> Option<Int> {
    if n > 0 {
        return Some(n);
    }
    return None();
}

func Main() -> Void {
    let value: Option<Int> = FindPositive(7);

    match value {
        case .Some(v):
            Log(v);
        case .None:
            Log(0);
    }
}
```

현재 구현 기준에서 다음이 연결되어 있다.

- semantic: `Option<T>` 타입, built-in 함수 검증
- C backend: `PgyOption_*` lowering
- LLVM backend: option struct lowering
- `match`: `Some(...)` / `None()` destructuring

## 7. 아직 stable 하지 않은 항목

다음 표면은 설계 문서에는 있었지만 현재 구현 기준의 stable feature로 문서화하기엔 이르다.

- `Result<T, E>` 두 개의 타입 파라미터를 가진 완전한 표면
- `try { } catch { }`
- ability/class 기반 에러 계층
- `@[error_type]` 같은 메타프로그래밍 표면
- `MapErr`, `AndThen` 같은 고수준 combinator 체인

현재 프로젝트 상태를 정확히 표현하려면, 에러 처리는 우선 `Result<T>` + `?` + `await RemoteFuture -> Result<T>`를 중심으로 이해하고, `Option<T>`는 "값의 부재"를 표현하는 현재 표준 표면으로 보면 된다.
