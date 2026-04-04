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
- 블록은 `{ ... }` — **BSD (Allman) 스타일 기본**, K&R도 파서가 허용
- 키워드는 소문자
- 타입과 내장 API는 PascalCase
- 구조화된 주석 `/// @effects ...` 같은 doc comment를 파서가 읽는다
- identity-bearing 타입 (`subject`, `relation`, `effect`, `zone`, `world`)은 함수 파라미터로 **자동 참조 전달** (포인터 은닉)
- value 타입 (`struct`, `vessel`, `class`, `object`, `dto`)은 복사 전달

대표 키워드:
`let`, `func`, `async`, `await`, `spawn`, `with`, `parallel`, `if`, `else`, `for`, `while`, `match`, `select`, `case`, `default`, `return`, `break`, `continue`, `import`, `namespace`, `export`, `extern`, `subject`, `class`, `struct`, `object`, `dto`, `enum`, `event`, `actor`, `ability`, `role`, `party`, `relation`, `effect`, `zone`, `systemic`, `world`

주의:
- `world`, `systemic`, `relation`, `effect`, `zone`은 contextual keyword다. 선언 위치에서는 키워드처럼 동작하지만, 지역 변수나 일반 표현식 자리에서는 식별자로 쓸 수 있다.
- `HasProjection(slotName)`는 현재 relation/effect/zone 문맥에서만 유효한 projection sync-ready query다.
- `world state`의 `projection` / `layer` / `state` suffix는 같은 줄에서만 해석된다. 다음 줄의 `state` 선언 시작을 suffix로 삼키지 않는다.

## 2. 선언

### 2.1 값 선언 (`let` / `:=`)

```pergyra
let x = 42;                        // let 키워드
let name: String = "Pergyra";      // 타입 명시
x := 42;                           // := 단축 선언
```

- `let name = expr;` — 기본 선언 (타입 추론)
- `let name: Type = expr;` — 타입 명시
- `name := expr;` — `let`의 축약 (타입 추론 전용)
- `let`은 가변(mutable). 재할당 가능.
- `let`이 필요한 이유: 제네릭 `<>` 파서 모호성 해결 (`Array<Int> x` → 비교? 타입?)

구조 분해(destructuring):

```pergyra
let (a, b, c) = Split("x y z", " ");
```

- 튜플/배열 반환값을 여러 변수에 동시 바인딩할 수 있다.

### 2.2 함수와 action

"method"라는 용어는 Pergyra에서 쓰지 않는다. 용어 체계:

- **free func** -- top-level 함수
- **hosted func** (귀속 func) -- 타입 안에 선언, `self`를 받음
- **general func** -- subject 안의 일반 func (사적 판단)
- **action** -- subject 전용, zone/effect 연동 (공적 행위)

```pergyra
func Add(a: Int, b: Int) -> Int { return a + b; }   // free func

vessel HP {
    func Percentage(self) -> Int { ... }              // hosted func
}

subject Fighter {
    func IsAlive(self) -> Bool { return hp > 0; }     // general func
    action Attack(self, target: Fighter) -> Int        // action
        requires Combatable within BattleZone
        causes DamageEffect { ... }
}
```

`->` 는 함수 반환 타입을 지정하는 문법이다.

```pergyra
func Identity<T>(value: T) -> T { return value; }
func Sort<T>(items: Array<T>) -> Array<T> where T: Comparable { return items; }
```

지원:
- free func, hosted func, general func, action
- 제네릭 함수, `where` 제약
- `async func`

주의:
- `async func`는 현재 제네릭/`where` 절을 지원하지 않는다.
- C lowering: hosted func -> `TypeName_Func(Type *self, ...)`, JS -> `class { Func() { this.xxx } }`

#### `->` 반환 타입 문법

`->`는 함수/람다의 반환 타입을 지정한다.

```pergyra
func Add(a: Int, b: Int) -> Int { ... }
```

람다 반환 타입 주석에도 사용된다:

```pergyra
let f = (x: Int) -> Int => { return x + 1; };
```

#### `Void`와 `return`

Pergyra에서 `Void`와 `return`은 같은 개념이 아니다.

- `Void`는 **반환 결과가 없음**을 나타내는 결과 타입이다.
- `return`은 **현재 실행을 즉시 종료하는 제어 문장**이다.

즉:

```pergyra
func Tick() -> Void {
    if done {
        return;
    }
}
```

위 코드는:

- `-> Void`로 인해 "이 hosted func/action은 결과값을 돌려주지 않는다"를 선언하고
- `return;`으로 인해 "여기서 실행을 끝내고 빠져나간다"를 표현한다.

규칙은 다음처럼 이해하면 된다.

- `-> Void`
  - 결과 타입이 없다.
  - 블록 끝 자연 종료가 허용된다.
- `return;`
  - 값을 주지 않고 현재 실행만 종료한다.
  - `Void` 반환 경로에서만 쓴다.
- `return expr;`
  - 값을 돌려주면서 현재 실행을 종료한다.
  - non-`Void` 반환 경로에서만 쓴다.

예:

```pergyra
func Heal(amount: Int) -> Void {
    if amount <= 0 {
        return;
    }
    hp = hp + amount;
}

func Score() -> Int {
    if hp <= 0 {
        return 0;
    }
    return hp + bonus;
}
```

설계 원칙:

- `Void`는 빈 값을 뜻하는 일반 값 타입처럼 취급하지 않는다.
- `return`은 값의 유무와 무관하게 실행을 종료하는 문장이다.
- `Void` 함수/func/action에서 `return;`은 **선택적 조기 종료**이고, 블록 끝 자연 종료와 함께 쓸 수 있다.
- non-`Void` 함수/func/action은 모든 경로가 값을 반환해야 한다.

현재 용어 체계:

- `func` / `action`은 `-> Void` 또는 `-> T`를 가질 수 있다.
- `action`이라고 해서 자동으로 `Void`인 것은 아니다.
- `Void`와 `action`은 별도 개념이다. `action`은 공적 행위의 의미론이고, `Void`는 결과 타입이다.

### 2.3 타입 선언

```pergyra
struct Vec3 {
    x: Float;
    y: Float;
    z: Float;
}

object PlayerView {
    hp: Int;
    name: String;
}

dto PlayerDto {
    hp: Int;
    name: String;
}

subject Player<T> where T: Serializable {
    private let name: String;
    public let health: Int;
}

enum Color { Red, Green, Blue }
```

지원:
- `struct`
- `object`
- `dto`
- `subject`
- `class`
- `enum`
- `actor`
- `subject Name actor { ... }`
- `relation`
- `effect`
- `zone`

주의:
- `subject`와 `class`는 현재 서로 다른 nominal declaration flavor로 파싱되고 semantic도 둘을 구분한다.
- `object`, `dto`, `struct`는 현재 같은 value/projection declaration으로 파싱된다.
- `relation`, `effect`, `zone`은 현재 `subject slot` / `object slot` / `dto slot` / `refresh` / `publish` / `bind` / `shared` / `func`까지의 최소 body surface를 가진다.
- `shared`는 `public`의 대체물이 아니다. `shared`는 `party` / `relation` / `effect` / `zone` / `world` 같은 host 내부에서 여러 rule, func, lifecycle이 공동으로 읽고 갱신하는 **host-local contextual state**를 뜻한다.
- 즉 `shared`는 "그 host가 들고 있는 문맥 전역 상태"에 가깝고, 프로그램 전체 global이나 개별 subject private field와는 다르다.
- `zone`은 `authority subjectSlot`, `state name: effect ... on ...`, `state name: relation ... between ..., ...`를 지원한다.
- `authority subjectSlot`은 optional `requires Ability[, Ability]` 절을 붙일 수 있다.
- `zone`은 `apply/detach/link/unlink/refresh/publish/bind/maintain` 뒤에 optional `by subjectSlot` authority annotation을 붙일 수 있다.
- `zone`은 `apply stateName`, `link stateName`, `detach stateName`, `unlink stateName`, `maintain stateName` shorthand를 지원한다.
- `zone`은 `publish dtoSlot from subjectSlot`로 dto projection 갱신을 명시할 수 있다.
- `zone`은 `bind slotName from sourceSlot`로 projection target kind를 slot declaration에서 자동 추론할 수 있다. object slot이면 `refresh`, dto slot이면 `publish`와 같은 semantic 계약으로 해석된다.
- `HasLayer(layerSlot)`는 zone declaration / zone method 안에서 선언된 relation/effect layer slot 활성 여부를 Bool로 질의한다. C backend는 generated helper를 통해 zone rdlock과 generation stale-warning을 자동 삽입한다.
- `HasState(stateName)`는 zone declaration / zone method 안에서 선언된 state alias를 Bool로 질의한다.
- `HasState(effectState, targetSlot)` / `HasState(relationState, leftSlot, rightSlot)`는 state와 slot 조합이 선언과 맞는지까지 질의한다.
- `world`는 `state name: zone zoneSlot`, `state name: zone zoneSlot projection projectionSlot`, `state name: zone zoneSlot layer layerSlot`, `state name: zone zoneSlot state zoneStateName`, `activate/deactivate/maintain`, `HasZone(zoneOrState)`, `HasZoneProjection(zoneSlot, projectionSlot)`, `HasZoneLayer(zoneSlot, layerSlot)`, `HasZoneState(zoneSlot, stateName)`를 지원한다.
- subject/class/구조체의 필드/메서드 문법은 존재하지만, 일부 고급 OOP 설계 문법은 아직 문서보다 구현 범위가 좁다.

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

가시성과 `shared`의 차이:

- `public/private`는 **누가 볼 수 있는가**를 나타내는 visibility 축이다.
- `shared`는 **누가 함께 들고 갱신하는가**를 나타내는 contextual-state 축이다.
- 그래서 `shared`는 `public field`와 같지 않다. `shared round: Int`는 "공개 필드"보다 "`zone` 자체가 유지하는 공용 상태"에 더 가깝다.

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
Option<Int>
Result<Int>
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
- `|>` 파이프 연산자: `data |> Transform |> Validate`
- `?` postfix try: `let val = riskyFunc()?;` (Result<T> unwrap + early return)
- `${}` 문자열 보간: `"hello ${name}"`

연산자 우선순위는 대체로 다음과 같다.
1. 호출 / 멤버 접근 / 인덱싱
2. 단항 `!`, `-`
3. `*`, `/`, `%`
4. `+`, `-`
5. 비교 `< <= > >=`
6. 동등 `== !=`
7. 논리 `&&`
8. 논리 `||`
9. 파이프 `|>`
10. 할당 `=`

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

for item in array {
    Log(item);
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
- `for name in collection` (배열 등 컬렉션 순회)
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

### 5.3 Option 패턴 매칭

`Option<T>` 타입은 `match`에서 `Some`/`None` 패턴으로 분해할 수 있다.

```pergyra
match opt {
    case Some(v):
        Log(v);
    case None:
        Log("empty");
}
```

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

주의:
- `SECURITY_LEVEL_*`는 현재 파싱만 되며 시맨틱 의미는 적용되지 않는다.

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
- `spawn blocking expr`
- `await expr`
- `async { ... }` 블록

`spawn blocking`은 블로킹 작업을 별도 스레드에서 실행한다:

```pergyra
let task = spawn blocking HeavyWork();
let result: Int = await task;
```

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

### 8.3 party / relation / effect / zone / systemic / world

```pergyra
party DungeonTeam {
    role slot tank: Damageable
}

relation TrustedLink for source: Player, target: Player {
    object slot snapshot: PlayerView
    shared trust: Int = 100
}

effect Poisoned for bearer: Player {
    object slot view: PlayerView
    shared stacks: Int = 1
}

zone BattleZone {
    subject slot player: Player
    subject slot enemy: Player
    object slot playerView: PlayerView = ToObject(PlayerView, player)
    relation slot trust: TrustedLink
    effect slot poison: Poisoned
    apply poison to player
    link trust between player, enemy
    detach poison from enemy
    unlink trust between player, enemy
    refresh playerView from player
    maintain poison on player
    maintain trust between player, enemy
    shared round: Int = 1
}

systemic CombatSystem {
    party slot team1: DungeonTeam
}

world GameWorld {
    systemic combat: CombatSystem
    zone battle: BattleZone
}
```

이 축은 파서/시맨틱에 들어와 있지만, 일반 문법보다 실험성이 더 높다.
현재 `relation`, `effect`, `zone`은 `for ...` header와 `subject slot`/`object slot`/`dto slot`/`refresh`/`publish`/`bind`/`shared`/`func`까지의 최소 표면이 구현돼 있고, domain slot은 optional initializer를 받을 수 있다. `relation` / `effect` / `zone`은 projection sync를 공유하고, `zone`은 추가로 `relation slot`/`effect slot`, `effect pool damage: DamageEffect capacity 8` 같은 fixed-capacity effect pool slot, `apply effectSlot to targetSlot`, `detach effectSlot from targetSlot`, `link relationSlot between left, right`, `unlink relationSlot between left, right`, `maintain effectSlot on targetSlot`, `maintain relationSlot between left, right`를 가진다. `world`는 `zone` slot까지 최소 조립 표면이 구현돼 있다.

## 9. 구현 기준 네이밍

- 키워드: 소문자
- 타입: PascalCase
- 내장 API: PascalCase
- 파일명: `snake_case.pgy`

대표 내장 API:
`ClaimSlot`, `ClaimSecureSlot`, `Write`, `Read`, `Release`, `ViewRead`, `ViewWrite`, `Move`, `Log`, `Split`, `Join`, `ToInt`, `ToFloat`, `Sqrt`, `Pow`, `Floor`, `Ceil`, `Random`, `ArraySort`, `ArrayMap`, `ArrayFilter`, `ArrayReverse`, `ArrayLength`, `ArrayPush`, `ArrayPop`, `ArraySet`, `Some`, `None`, `IsSome`, `IsNone`, `UnwrapOption`, `ChannelSpace`, `ChannelFull`, `ChannelClosed`, `ChannelLength`, `ChannelCapacity`, `ChannelReady`, `TryRecv`, `TrySend`, `Cancel`, `IsCancelled`, `SpawnBlocking`, `ToObject`, `ToDto`, `HasState`, `HasZone`

## 10. 문서 사용법

- 실제 구현 문법 확인: 이 문서
- 설계/비전 확인: [00_vision.md](/mnt/e/PergyraLang/docs/00_vision.md)
- 상세 문법 레퍼런스: [02_grammar.md](/mnt/e/PergyraLang/docs/grammar/02_grammar.md)
- 네이밍 규칙: [03_naming.md](/mnt/e/PergyraLang/docs/grammar/03_naming.md)

## 11. 현재 미지원 / AST 흔적만 있는 것

- `type alias`
- `task group` 표면 문법
- standalone `event handler type` 표면 문법

이 항목들은 AST enum이나 내부 코드 경로 흔적이 있을 수 있어도, 현재는 공식 문법 레퍼런스로 보면 안 된다.
