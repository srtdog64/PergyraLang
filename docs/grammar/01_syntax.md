# Pergyra 구현 문법 레퍼런스

이 문서는 현재 저장소의 `lexer`, `parser`, `semantic`, `tests`, `examples`를 기준으로 정리한 **실구현 문법 레퍼런스**다.
설계 초안이나 미래 문법은 넣지 않는다.

구현 근거:
- 파서 테스트: [src/test_parser.c](/mnt/e/PergyraLang/src/test_parser.c)
- 시맨틱 테스트: [src/test_semantic.c](/mnt/e/PergyraLang/src/test_semantic.c)
- 예제 모음: [examples](/mnt/e/PergyraLang/examples)

## 상태 표

| 구분 | 현재 상태 | 예시 |
|---|---|---|
| Stable | parser/semantic/examples/backend smoke로 계속 검증되는 핵심 문법 | `let`, `func`, `if/else`, `for`, `while`, `match`, 배열, 문자열, `slot/view/move`, `spawn/await`, `Channel`, `import/export/namespace`, `enum` |
| Supported but Evolving | 구현은 있지만 조합/의미론이 더 변할 수 있는 문법 | `select`, `actor`, `event + lambda`, `ability/role`, `party/systemic/world`, structured comment `@effects`, `defer`, `unsafe` |
| Not Current Surface | AST 흔적이나 설계 문서만 있고 공식 문법으로 보면 안 되는 것 | `type alias`, 고급 DSL 확장 초안, 미문서 실험 노드 |

규칙:
- 이 문서는 `Stable`과 `Supported but Evolving`만 다룬다.
- parser/semantic/examples/backend smoke에 없는 문법은 일상 문법처럼 적지 않는다.

## 1. 기본 규칙

- 문장 종료는 기본적으로 세미콜론 `;`
- 블록은 `{ ... }`
- 키워드는 소문자
- 타입과 내장 API는 PascalCase
- 구조화된 주석 `/// @effects ...` 같은 doc comment를 파서가 읽는다

대표 키워드:
`let`, `func`, `async`, `await`, `spawn`, `with`, `parallel`, `if`, `else`, `for`, `while`, `match`, `select`, `case`, `default`, `return`, `break`, `continue`, `import`, `namespace`, `export`, `extern`, `class`, `struct`, `enum`, `event`, `actor`, `ability`, `role`, `party`, `systemic`, `world`

## 2. 선언

### 2.1 값 선언

```pergyra
let x = 42;
let name: String = "Pergyra";
let values: Array<Int> = [1, 2, 3];
```

- `let name = expr;`
- `let name: Type = expr;`
- 타입 추론과 명시 타입 주석을 둘 다 지원

### 2.2 함수

```pergyra
func Add(a: Int, b: Int) -> Int {
    return a + b;
}

func Identity<T>(value: T) -> T {
    return value;
}

func Sort<T>(items: Array<T>) -> Array<T>
where T: Comparable {
    return items;
}
```

지원:
- 일반 함수
- 제네릭 함수
- `where` 제약
- `async func`

### 2.3 타입 선언

```pergyra
struct Vec3 {
    x: Float;
    y: Float;
    z: Float;
}

class Player<T> where T: Serializable {
    private let name: String;
    public let health: Int;
}

enum Color { Red, Green, Blue }
```

지원:
- `struct`
- `class`
- `enum`
- `actor`

주의:
- 클래스/구조체의 필드/메서드 문법은 존재하지만, 일부 고급 OOP 설계 문법은 아직 문서보다 구현 범위가 좁다.

### 2.4 모듈/가시성

```pergyra
import "math.pgy";

namespace Math {
    export func Add(a: Int, b: Int) -> Int {
        return a + b;
    }
}
```

지원:
- `import "file.pgy";`
- `namespace Name { ... }`
- `export` 선언
- `extern "C" { ... }`

## 3. 타입 계열

### 3.1 기본 타입

`Int`, `Long`, `Float`, `Double`, `Bool`, `String`, `Void`

### 3.2 컬렉션 / 메모리 타입

```pergyra
Array<T>
Slice<T>
Rc<T>
Weak<T>
Box<Array<Int>>
Allocator
```

### 3.3 자원 타입

```pergyra
Slot<Int>
SecureSlot<Int>
ReadView<Int>
WriteView<Int>
MoveToken<Int>
QubitSlot
DeviceSlot<Int>
Channel<Int>
Future<Int>
RemoteFuture<Int>
```

## 4. 표현식

```pergyra
a + b * c
a == b
a && b || !c
target = value
object.Method(42)
values[index]
spawn Work(1, 2)
await pending
<-ch
```

지원되는 큰 축:
- 산술/비교/논리 연산
- 할당식
- 함수 호출
- 멤버 접근
- 배열 인덱싱
- 배열 리터럴
- `spawn`
- `await`
- 채널 송수신 연산자

연산자 우선순위는 대체로 다음과 같다.
1. 호출 / 멤버 접근 / 인덱싱
2. 단항 `!`, `-`
3. `*`, `/`, `%`
4. `+`, `-`
5. 비교 `< <= > >=`
6. 동등 `== !=`
7. 논리 `&&`
8. 논리 `||`
9. 할당 `=`

## 5. 제어문

### 5.1 조건/반복

```pergyra
if cond {
    Log(1);
} else {
    Log(0);
}

for i in 0..10 {
    Log(i);
}

while running {
    if stop { break; }
    continue;
}
```

지원:
- `if / else`
- `else if` 체인
- `for name in start..end`
- `while`
- `break`
- `continue`

주의:
- `else if`는 별도 키워드가 아니라 `else` 뒤에 중첩 `if`를 붙이는 형태로 파싱된다.

### 5.2 match

```pergyra
match value {
    case 0:
        Log("zero");
    case 1:
        Log("one");
    default:
        Log("other");
}
```

지원:
- `match`
- `case`
- `default`
- guard가 있는 `case value if cond:`

## 6. 슬롯과 view

### 6.1 기본 슬롯

```pergyra
let slot: Slot<Int> = ClaimSlot<Int>();
Write(slot, 42);
let value: Int = Read(slot);
Release(slot);
```

문법 설탕:

```pergyra
let x: Slot<Int> = 42;
Log(x);
x = 7;
```

### 6.2 with 블록

```pergyra
with slot<Int> as counter {
    counter.Write(100);
    Log(counter.Read());
}

with SecureSlot<Int>(SECURITY_LEVEL_HARDWARE) as hp {
    hp.Write(100);
}
```

### 6.3 view / move

```pergyra
let rv: ReadView<Int> = ViewRead(slot);
let wv: WriteView<Int> = ViewWrite(slot);
let mt: MoveToken<Int> = Move(slot);
let dst: Slot<Int> = mt;
```

## 7. 병렬 / 비동기 / 채널

### 7.1 parallel

```pergyra
parallel {
    TaskA();
    TaskB();
}
```

- `parallel`은 pthread 기반 실제 병렬 실행 경로
- 슬롯 충돌 분석이 시맨틱 단계에 들어가 있음

### 7.2 async / spawn / await

```pergyra
async func Fetch() -> Int {
    let task = spawn Work(1);
    let value: Int = await task;
    return value;
}

async {
    ch <- 11;
}
```

지원:
- `async func`
- `spawn expr`
- `await expr`
- `async { ... }` 블록

### 7.3 채널 / select

```pergyra
let ch: Channel<Int> = Channel(4);
ch <- 10;
let value: Int = <- ch;

select {
    case v = <-ch:
        Log(v);
    default:
        Log(0);
}
```

지원:
- `Channel<T>`
- `ch <- value`
- `let v = <- ch`
- `select { case ... default ... }`

## 8. 도메인 문법

### 8.1 event / lambda

```pergyra
event OnHit(damage: Int);

func Main() -> Void {
    OnHit += (d: Int) => { Log(d); };
    OnHit(77);
}
```

### 8.2 ability / role

```pergyra
ability Arithmetic {
    func Add(other: Int) -> Int;
}

role IntMath for Int {
    impl ability Arithmetic {
        func Add(other: Int) -> Int {
            return 123;
        }
    }
}
```

### 8.3 party / systemic / world

```pergyra
party DungeonTeam {
    role slot tank: Damageable
}

systemic CombatSystem {
    party slot team1: DungeonTeam
}

world GameWorld {
    systemic combat: CombatSystem
}
```

이 축은 파서/시맨틱에 들어와 있지만, 일반 문법보다 실험성이 더 높다.

## 9. 구현 기준 네이밍

- 키워드: 소문자
- 타입: PascalCase
- 내장 API: PascalCase
- 파일명: `snake_case.pgy`

대표 내장 API:
`ClaimSlot`, `ClaimSecureSlot`, `Write`, `Read`, `Release`, `ViewRead`, `ViewWrite`, `Move`, `Log`

## 10. 문서 사용법

- 실제 구현 문법 확인: 이 문서
- 설계/비전 확인: [00_vision.md](/mnt/e/PergyraLang/docs/00_vision.md)
- 오래된 설계안/확장 구문 참고: [02_grammar.md](/mnt/e/PergyraLang/docs/grammar/02_grammar.md)
- 네이밍 규칙: [03_naming.md](/mnt/e/PergyraLang/docs/grammar/03_naming.md)

## 11. 현재 미지원 / AST 흔적만 있는 것

- `type alias`
- `task group` 표면 문법
- standalone `event handler type` 표면 문법

이 항목들은 AST enum이나 내부 코드 경로 흔적이 있을 수 있어도, 현재는 공식 문법 레퍼런스로 보면 안 된다.
