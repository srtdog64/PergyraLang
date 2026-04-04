# Pergyra 현재 구현 문법 레퍼런스

이 문서는 `lexer`, `parser`, `semantic`, `tests`, `examples` 기준의 **실제 구현 문법**을 정리한다.
설계 아이디어와 장기 비전은 별도 문서에 둘 수 있지만, 이 문서는 “지금 파서가 읽고 컴파일러가 처리하는 형태”를 기준으로 한다.

관련 문서:
- 최소 요약: [01_syntax.md](/mnt/e/PergyraLang/docs/grammar/01_syntax.md)
- 네이밍 규칙: [03_naming.md](/mnt/e/PergyraLang/docs/grammar/03_naming.md)
- 비전: [00_vision.md](/mnt/e/PergyraLang/docs/00_vision.md)

## 1. 기본 규칙

- 키워드는 소문자 기준이다.
  예: `let`, `func`, `with`, `parallel`, `if`, `for`, `async`, `await`
- 내장 API와 타입은 PascalCase 기준이다.
  예: `Int`, `String`, `ClaimSlot`, `Read`, `Write`, `Release`
- 문장 종료는 세미콜론 `;` 이다.
- 블록은 `{ ... }` 로 쓴다.
- 현재 구현 기준으로 “문서상 제안만 있고 미구현인 문법”은 이 문서에 실지 않는다.

## 2. 리터럴과 표현식

### 2.1 리터럴

```pergyra
42;
"hello";
true;
[1, 2, 3];
```

지원되는 리터럴:
- 정수
- 문자열
- 불리언
- 배열 리터럴

### 2.2 기본 표현식

```pergyra
a + b * c;
a == b;
a && b || !c;
obj.Method(42);
array[i + 1];
value = other;
pipeline = x |> F |> G;
let y = Validate(x)?;
```

주요 표현식 종류:
- 이항/단항 연산
- 함수 호출
- 멤버 접근
- 배열 인덱싱
- 배열 리터럴
- 대입
- `await`
- `spawn`
- 채널 송수신
- 파이프 `|>`
- postfix `?` (Result<T> unwrap + early return)

### 2.3 우선순위

대체로 다음 순서를 따른다.

1. 호출, 멤버 접근, 배열 접근
2. 단항 `!`, `-`
3. `*`, `/`, `%`
4. `+`, `-`
5. 비교 `< <= > >=`
6. 동등 `== !=`
7. 논리 `&&`
8. 논리 `||`
9. 할당 `=`

## 3. 선언

### 3.1 `let`

```pergyra
let x = 42;
let name: String = "Pergyra";
let values: Array<Int> = [1, 2, 3];
```

- 타입 주석은 선택 사항이다.
- 일부 자원 타입은 `let` 초기화 규칙이 더 엄격하다.
  예: `QubitSlot`, `DeviceSlot`, `ReadView<T>`, `WriteView<T>`, `MoveToken<T>`

### 3.2 함수

```pergyra
func Add(a: Int, b: Int) -> Int {
    return a + b;
}
```

제네릭과 `where` 절:

```pergyra
func Identity<T>(value: T) -> T {
    return value;
}

func Sort<T>(items: Array<T>) -> Array<T>
where T: Comparable {
    return items;
}
```

지원되는 요소:
- 일반 함수
- 제네릭 함수
- `where` 제약
- `async func`
- `export func`

주의:
- `async func`는 현재 제네릭/`where` 절을 지원하지 않는다.

### 3.3 타입 선언

```pergyra
struct Vec3 {
    x: Float;
    y: Float;
    z: Float;
}

subject Player {
    private let name: String;
    public let health: Int;
}

enum Color { Red, Green, Blue }

enum Shape {
    Circle(Int),
    Rect(Int, Int),
    None
}
```

지원되는 선언:
- `struct`
- `object` (`struct` alias)
- `subject` (`class` alias)
- `class`
- `dto` (`struct` alias)
- `enum`
- `relation`
- `effect`
- `zone`
- `extern "C"` block

추가 메모:
- `relation`, `effect`는 현재 optional `for name: Type[, ...]` header와 `subject slot`, `object slot`, `dto slot`, `refresh`, `publish`, `shared`, `func`의 최소 조합을 지원한다.
- `relation` / `effect` / `zone`의 domain slot은 optional initializer를 받을 수 있다.
- `zone` body는 현재 `subject slot`, `object slot`, `dto slot`, `relation slot`, `effect slot`, `authority <subjectSlot> [requires <Ability>[, ...]]`, `state <name>: effect <effectSlot> on <targetSlot>`, `state <name>: relation <relationSlot> between <left>, <right>`, `apply <effectSlot> to <targetSlot>`, `apply <stateName>`, `detach <effectSlot> from <targetSlot>`, `detach <stateName>`, `link <relationSlot> between <left>, <right>`, `link <stateName>`, `unlink <relationSlot> between <left>, <right>`, `unlink <stateName>`, `refresh <objectSlot> from <subjectSlot>`, `publish <dtoSlot> from <subjectSlot>`, `maintain <effectSlot> on <targetSlot>`, `maintain <relationSlot> between <left>, <right>`, `maintain <stateName>`, `shared`, `func`를 지원한다.
- `zone`의 `apply/link/detach/unlink/refresh/publish/maintain`은 optional `by <subjectSlot>` authority annotation을 받을 수 있다.
- `HasState(<stateName>)`는 zone declaration / zone method 안에서만 유효하며, 선언된 zone state alias를 Bool로 조회한다. 인자는 identifier나 string literal을 받을 수 있다.
- `HasState(<effectState>, <targetSlot>)`와 `HasState(<relationState>, <leftSlot>, <rightSlot>)`는 선언된 state alias와 slot 조합이 정확히 맞는지까지 검증한다.
- `zone` body는 여기에 더해 `relation slot`, `effect slot`을 지원한다.
- `world` body는 `systemic`, `zone`, `state <name>: zone <zoneSlot>`, `activate <zoneOrState>`, `deactivate <zoneOrState>`, `maintain <zoneOrState>`, `shared`, `func`를 지원한다.
- `HasZone(<zoneOrState>)`는 world declaration / world method 안에서만 유효하며, 선언된 zone slot 또는 world state alias를 Bool로 조회한다.

미지원:
- `type` alias (파서/시맨틱 미구현)

### 3.4 모듈

```pergyra
import "math.pgy";

namespace Math {
    export func Add(a: Int, b: Int) -> Int {
        return a + b;
    }
}
```

지원되는 요소:
- `import`
- `namespace`
- `export`

## 4. 타입 표기

### 4.1 기본 타입

- `Int`
- `Long`
- `Float`
- `Double`
- `Bool`
- `String`
- `Void`

### 4.2 컬렉션/자원 타입

```pergyra
Array<Int>
Slice<Float>
Channel<Int>
Future<Int>
RemoteFuture<Int>
Slot<Int>
SecureSlot<Int>
ReadView<Int>
WriteView<Int>
MoveToken<Int>
QubitSlot
DeviceSlot<Int>
Rc<Int>
Weak<Int>
Box<Array<Int>>
Allocator
```

현재 구현이 해석하는 대표 타입군:
- 배열/슬라이스
- 채널/퓨처
- 슬롯 계열
- 양자/디바이스 자원
- 공유 소유권/박스/할당기

## 5. 제어문

### 5.1 조건문

```pergyra
if x > 10 {
    Log("big");
} else {
    Log("small");
}
```

### 5.2 반복문

```pergyra
for i in 0..10 {
    Log(i);
}

while cond {
    if stop { break; }
    continue;
}
```

지원:
- `for i in start..end`
- `while`
- `break`
- `continue`

### 5.3 패턴/선택

```pergyra
match value {
    case 0:
        Log("zero");
    case 1 if value > 0:
        Log("one");
    default:
        Log("other");
}

select {
    case v = <-ch:
        Log(v);
    default:
        Log(0);
}
```

지원:
- `match`
- `case`
- `default`
- `guard` (`case x if cond:`)
- `select`
- `case <-ch:`
- `case v = <-ch:`

## 6. 슬롯과 자원 셀

### 6.1 기본 슬롯

```pergyra
let s: Slot<Int> = 42;
Log(s);
s = 7;
Release(s);
```

또는 명시형:

```pergyra
let s: Slot<Int> = ClaimSlot();
Write(s, 42);
let v: Int = Read(s);
Release(s);
```

### 6.2 `with`

```pergyra
with slot<Int> as hp {
    hp.Write(100);
    Log(hp.Read());
}

with SecureSlot<Int>(SECURITY_LEVEL_HARDWARE) as hp {
    hp.Write(100);
}
```

주의:
- `SECURITY_LEVEL_*`는 현재 **파싱만** 되며 시맨틱 의미는 적용되지 않는다.

### 6.3 View / Move

```pergyra
let rv: ReadView<Int> = ViewRead(s);
let wv: WriteView<Int> = ViewWrite(s);
let mt: MoveToken<Int> = Move(s);
let dst: Slot<Int> = mt;
```

## 7. 병렬 / 비동기 / 채널

### 7.1 `parallel`

```pergyra
parallel {
    WorkA();
    WorkB();
}
```

- `parallel`은 pthread 기반 병렬 실행 경로다.

### 7.2 `spawn` / `await`

```pergyra
async func Fetch() -> Int {
    let task = spawn Compute(42);
    let value: Int = await task;
    return value;
}
```

- `spawn`은 `Future<T>` 계열을 만든다.
- `await`는 async 문맥 안에서 사용한다.
- 현재 구현은 코루틴 런타임 경로를 사용한다.

`RemoteFuture<T>`는 `await` 결과가 `Result<T>`가 된다.

### 7.3 `async` 블록

```pergyra
async {
    ch <- 11;
}
```

- `async { ... }` 는 detached async block으로 해석된다.

### 7.4 채널

```pergyra
let ch: Channel<Int> = Channel(4);
ch <- 42;
let value: Int = <-ch;
```

## 8. 도메인 계층

### 8.1 ability / role

```pergyra
ability Damageable {
    require health: Int;
    func TakeDamage(amount: Int) -> Void;
}

role PlayerDamageable for Player {
    impl ability Damageable {
        func TakeDamage(amount: Int) -> Void {
            Log(amount);
        }
    }
}
```

### 8.2 party / systemic / world

```pergyra
party Team {
    role slot tank: Damageable;
    shared formation: String = "standard";
}

systemic CombatSystem {
    party slot team1: Team;
}

world GameWorld {
    systemic combat: CombatSystem;
}
```

### 8.3 actor / event / lambda

```pergyra
actor Counter {
    let count: Int;
}

event OnHit(damage: Int);

OnHit += (d: Int) => { Log(d); };
OnHit(77);
```

## 9. 기타 문법

### 9.1 안전하지 않은 블록 / defer / bind

AST와 parser 기준으로 다음 문법 진입점이 있다.

```pergyra
unsafe {
    Dangerous();
}

defer {
    Cleanup();
};
```

주의:
- `unsafe`와 `defer`는 시맨틱/백엔드 경로가 존재하며 회귀 테스트로 검증된다.
- `bind`는 파서/시맨틱/코드젠 경로가 있으나 의미론은 얕다 (동적 바인딩 확인 수준).

## 10. 상태 구분

이 문서 기준 구분:

- **지원됨**: parser + semantic + backend/tests/examples로 확인된 문법
- **부분 지원**: parser/semantic은 있으나 제한이 크거나 backend 차이가 있는 문법
- **실험적**: 설계 방향은 있으나 문서상 고정 문법으로 간주하면 안 되는 영역

현재 특히 주의할 영역:
- `defer`
- 일부 고수준 domain 문법의 세부 의미론
- effect 표기 문법의 확장
- backend별 세부 동작 차이
