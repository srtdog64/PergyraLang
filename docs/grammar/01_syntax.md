# Pergyra 코드 문법 정리

이 문서는 현재 저장소의 `lexer`와 `parser` 구현을 기준으로 정리한 최소 문법 요약이다. 기존 `docs/grammar.md`는 설계안이 많이 섞여 있어서, 여기서는 실제 코드가 해석하려는 형태만 따로 적는다.

엔진 목표 기준의 코어 스펙은 [00_engine_core_spec.md](/mnt/e/PergyraLang/docs/00_engine_core_spec.md)에 정리한다.

## 기본 선언

```pergyra
let x = 42;
let name: String = "Pergyra";
let ok = true;
```

- `let` 선언은 세미콜론 `;` 으로 끝난다.
- 타입 주석은 선택 사항이다.

## 함수

```pergyra
func Add(a: Int, b: Int) -> Int {
    return a + b;
}
```

제네릭과 `where` 절도 지원 대상이다.

```pergyra
func Identity<T>(value: T) -> T {
    return value;
}

func Sort<T>(items: Array<T>) -> Array<T>
where T: Comparable {
    return items;
}
```

## 배열과 슬라이스 타입

```pergyra
let vertices: Array<Vertex> = meshData;
let view: Slice<Vertex> = window;

let first = vertices[0];
let count = view.Length;
```

- 현재 구현은 `Array<T>`와 `Slice<T>` 타입 주석을 해석한다.
- `arr[index]` 형태의 인덱싱을 파싱하고 타입 검사한다.
- `Array<T>`와 `Slice<T>`의 `Length` 멤버 접근을 `Int`로 본다.

## 공유 메모리와 할당기

```pergyra
let shared: Rc<Int> = RcNew(42);
let weak: Weak<Int> = RcDowngrade(shared);
let again: Rc<Int> = WeakUpgrade(weak);

let alloc: Allocator = AllocatorPool(4096);
let trace: Allocator = AllocatorTracing();

let numbers: Box<Array<Int>> = BoxArray(128, alloc);
```

- 공유 소유권은 `Rc<T>`와 `Weak<T>` 타입으로 표현한다.
- 현재 구현 기준 빌트인:
  `RcNew`, `RcClone`, `RcDrop`, `RcGet`, `RcDowngrade`, `WeakUpgrade`, `WeakDrop`
- 할당기 생성 빌트인:
  `AllocatorSystem`, `AllocatorTracing`, `AllocatorDebug`, `AllocatorPool`
- `BoxArray(capacity[, allocator])`는 `Box<Array<T>>`용 단일 할당 경로로 처리한다.

## FFI

```pergyra
extern "C" {
    func SDL_Init(flags: Int) -> Int;
    func SDL_Quit();
}
```

- `extern "C"` 블록 안에서는 함수 선언만 둔다.
- 외부 함수 선언은 본문 없이 `;`로 끝난다.
- 현재 구현은 ABI 문자열 `"C"`를 기준으로 동작한다.

## 클래스

```pergyra
class Player<T> where T: Serializable {
    private let name: String;
    public let health: Int;

    public func TakeDamage(amount: Int) {
        health = health - amount;
    }
}
```

## 구조체

```pergyra
struct Vec3 {
    x: Float;
    y: Float;
    z: Float;
}
```

- `struct`는 bare field 선언을 사용한다.
- 필드는 `name: Type;` 형식이다.
- 메서드도 같은 블록 안에 둘 수 있다.

## 슬롯

일반 슬롯 블록:

```pergyra
with slot<Int> as hp {
    hp.Write(100);
    Log(hp.Read());
}
```

보안 슬롯 블록:

```pergyra
with SecureSlot<Int>(SECURITY_LEVEL_HARDWARE) as hp {
    hp.Write(100);
}
```

- `with slot<T> as name { ... }`
- `with SecureSlot<T>(LEVEL) as name { ... }`

## 병렬 블록

문장으로 사용:

```pergyra
parallel {
    ProcessA();
    ProcessB();
}
```

식으로도 사용 가능:

```pergyra
let result = parallel {
    ProcessA();
    ProcessB();
};
```

- 현재 구현은 `parallel`만 정식 키워드로 취급한다.

## 제어문

`if`:

```pergyra
if x > 10 {
    Log("big");
} else {
    Log("small");
}
```

`for` 범위:

```pergyra
for i in 1..10 {
    Log(i);
}
```

- 범위 연산자는 `..` 이다.

## 표현식

지원되는 주요 표현식:

```pergyra
a + b * c
a == b
a && b || !c
object.Method(42)
array[index + 1]
target = value
```

우선순위는 대체로 다음 순서다.

1. 호출, 멤버 접근, 배열 접근
2. 단항 `!`, `-`
3. `*`, `/`, `%`
4. `+`, `-`
5. 비교 `< <= > >=`
6. 동등 `== !=`
7. 논리 `&&`
8. 논리 `||`
9. 할당 `=`

## 비동기와 채널

코드베이스에는 다음 구문을 위한 진입점이 있다.

```pergyra
async func Fetch() -> Int {
    return await work();
}

let task = spawn RunJob();
let value = <-channel;
channel <- value;
```

- 이 영역은 일반 함수/클래스 문법보다 완성도가 낮다.
- 제네릭 비동기 타입 등 확장 문법은 아직 부분 구현 상태다.

## 현재 기준 네이밍 규칙

- 키워드는 소문자 기준: `let`, `func`, `with`, `parallel`, `for`, `if`, `return`
- 타입과 내장 API는 PascalCase 기준: `Int`, `String`, `ClaimSlot`, `Write`, `Read`, `Release`, `Log`
