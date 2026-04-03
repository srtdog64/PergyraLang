# Pergyra 에러 처리 시스템

마지막 업데이트: 2026-04-03

이 문서는 현재 구현 기준으로 정리한 에러 처리 표면이다. 과거 설계 문서에 있던 `Result<T, E>`, `Option<T>` full surface, `try/catch`, 에러 매크로 시스템은 아직 현재 구현 기준의 stable feature가 아니다.

## 1. 현재 구현 중심

현재 코드와 테스트가 실제로 보장하는 축은 다음이다.

- `Result<T>` 타입
- `Ok(...)`, `Err(...)`
- `IsOk`, `IsErr`, `Unwrap`, `UnwrapOr`
- postfix `?` 조기 반환
- `RemoteFuture<T>`를 `await`했을 때 `Result<T>`가 되는 규칙

## 2. Result<T>

현재 예제와 프론트엔드 경로는 `Result<T>`를 중심으로 사용한다.

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

## 6. Option<T> 상태

`Option`은 런타임 매크로와 설계 문서에는 존재하지만, 현재 프로젝트에서 `Result<T>`만큼 고정된 source-level stable surface로 보긴 어렵다.

- 런타임에는 `PgyOption_*` 지원이 존재
- 문법적으로는 tagged union enum으로 `Some/None` 형태를 직접 정의해서 사용할 수 있음
- TODO 기준으로 `Option<T> / None`은 아직 고정 대상

따라서 현재 문서에서는 `Option<T>`를 "이미 완성된 언어 내장"으로 보기보다, "부분 준비된 표면"으로 보는 편이 정확하다.

## 7. 아직 stable 하지 않은 항목

다음 표면은 설계 문서에는 있었지만 현재 구현 기준의 stable feature로 문서화하기엔 이르다.

- `Result<T, E>` 두 개의 타입 파라미터를 가진 완전한 표면
- `Option<T>` full stdlib surface
- `try { } catch { }`
- trait/class 기반 에러 계층
- `@[error_type]` 같은 메타프로그래밍 표면
- `MapErr`, `AndThen` 같은 고수준 combinator 체인

현재 프로젝트 상태를 정확히 표현하려면, 에러 처리는 우선 `Result<T>` + `?` + `await RemoteFuture -> Result<T>` 조합으로 이해하는 것이 맞다.
