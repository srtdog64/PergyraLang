# Pergyra 패턴 매칭 시스템

마지막 업데이트: 2026-04-03

이 문서는 현재 구현 기준으로 정리한 패턴 매칭 표면이다. 예전 설계 문서에 있던 구조체 패턴, 배열 패턴, 중첩 패턴, exhaustiveness check는 아직 현재 구현 기준의 stable surface가 아니다.

## 1. 현재 지원 범위

- 값 비교 기반 `match`
- `case ... if ...` 형태의 guard
- `Result<T>`의 `Ok(...)` / `Err(...)` destructuring
- tagged union enum의 variant destructuring
- leading-dot shorthand: `.Ok(x)`, `.Err(e)`, `.Some(v)`, `.None`

## 2. 기본 값 매칭

```pergyra
func Main() -> Void {
    let x: Int = 3;

    match x {
        case 1:
            Log(10);
        case 2:
            Log(20);
        case 3:
            Log(30);
        default:
            Log(0);
    }
}
```

현재 구현은 일반적으로 `subject == pattern` 비교로 내려간다. 따라서 리터럴, enum variant 상수, 단순 식별자 기반 패턴이 현재 표면의 중심이다.

## 3. Guard

```pergyra
func Classify(n: Int) -> Void {
    match n {
        case 0:
            Log("zero");
        case 2 if n > 0:
            Log("two positive");
        default:
            Log("other");
    }
}
```

guard 식은 `Bool`이어야 하며, 현재 시맨틱 테스트에도 포함되어 있다.

## 4. Result 패턴

현재 구현된 에러 처리 표면은 `Result<T>` 중심이다. `Ok(value)` / `Err(error)` 패턴은 `match`에서 destructuring 된다.

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

`Ok(value)`와 `.Ok(value)`는 같은 패턴으로 파싱된다.

## 5. Tagged Union Enum 패턴

데이터를 가진 enum variant는 `match`에서 destructuring 가능하다.

```pergyra
enum Shape {
    Circle(Int),
    Rect(Int, Int),
    None
}

func Print(shape: Shape) -> Void {
    match shape {
        case .Circle(r):
            Log(r);
        case .Rect(w, h):
            Log(w);
            Log(h);
        case .None:
            Log(0);
    }
}
```

이 경로는 C/LLVM 코드젠 양쪽에 연결되어 있다.

## 6. Leading-Dot Shorthand

문서와 예제에서 자주 쓰는 leading-dot variant shorthand가 현재 파서에서 허용된다.

```pergyra
enum OptionInt {
    Some(Int),
    None
}

func Wrap(n: Int) -> OptionInt {
    if n > 0 {
        return .Some(n);
    }
    return .None;
}
```

## 7. 아직 stable 하지 않은 항목

다음 표면은 예전 설계 문서에는 있었지만, 현재 구현 기준의 stable surface로 문서화하기엔 이르다.

- 구조체 패턴: `Player { health: 0 }`
- 배열/컬렉션 패턴: `[]`, `[head, ...tail]`
- 중첩 패턴: `.Request(.Login(...))`
- `@` 바인딩 패턴
- 완전성 검사(exhaustiveness checking)

이 항목들은 설계 방향이나 장기 목표로는 남아 있지만, 현재 문법/시맨틱/코드젠이 전부 보장하는 표면으로 보지 않는 편이 맞다.
