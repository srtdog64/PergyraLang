# Pergyra 네이밍 규칙

이 문서는 현재 저장소의 예제, parser, built-in API 이름을 기준으로 정리한 **실사용 네이밍 규칙**이다.

## 1. 요약

- 키워드: 소문자
- 타입/내장 함수: PascalCase
- 지역 변수: camelCase
- 상수: UPPER_SNAKE_CASE
- 파일명: `snake_case.pgy`

## 2. 키워드

키워드는 소문자다.

예:

```pergyra
let
func
with
parallel
if
else
for
while
match
case
default
async
await
spawn
select
import
namespace
export
subject
class
struct
tobject
enum
participant
ability
role
party
relation
effect
zone
roster
world
```

## 3. 변수와 바인딩

지역 변수와 일반 바인딩은 camelCase 기준이다.

```pergyra
let counter = 0;
let userName = "Alice";
let pendingTask = spawn Work();
let readView = ViewRead(slot);
```

`with ... as name` 별칭도 같은 규칙을 따른다.

```pergyra
with slot<Int> as counterSlot {
    counterSlot.Write(1);
}
```

## 4. 함수와 메서드

현재 저장소의 public-facing 함수 스타일은 PascalCase가 기준이다.

```pergyra
func Add(a: Int, b: Int) -> Int
func FetchData() -> Int
func TakeDamage(amount: Int) -> Void
```

내장 함수도 PascalCase다.

```pergyra
ClaimSlot()
ClaimSecureSlot()
Read(slot)
Write(slot, 42)
Release(slot)
Log(value)
ArrayPush(values, 10)
Measure(q)
```

연산자 오버로드 alias는 예외적으로 lower/snake 조합을 사용한다.

```pergyra
operator_add_Int
operator_add_Vec2
```

이 이름은 사용자 표면 API보다 코드젠/alias 이름에 가깝다.

## 5. 타입

타입 이름은 PascalCase다.

```pergyra
Int
String
Player
Vec3
Color
Damageable
GameWorld
```

제네릭 타입도 같은 규칙을 따른다.

```pergyra
Array<Int>
Slice<Float>
Slot<Int>
SecureSlot<Int>
ReadView<Int>
WriteView<Int>
MoveToken<Int>
Future<Int>
RemoteFuture<Int>
DeviceSlot<Int>
Rc<Int>
Weak<Int>
```

## 6. 제네릭 파라미터

단일 대문자 또는 설명적인 PascalCase를 쓴다.

```pergyra
func Identity<T>(value: T) -> T
func Map<T, U>(items: Array<T>) -> Array<U>
class Container<TData> { }
```

## 7. 필드

현재 코드베이스 기준으로:

- 일반 필드: `camelCase` 또는 문맥상 짧은 lower form
- 공개 예제에서는 `public let health: Int;` 같은 lower-case field도 많이 사용
- 과거 문서에 있던 “public PascalCase / private `_name`” 규칙은 현재 구현의 강제 규칙이 아니다

즉, 필드명은 다음을 권장한다.

```pergyra
class Player {
    private let name: String;
    public let health: Int;
}
```

언어가 강제하는 규칙보다 **프로젝트 일관성**이 더 중요하다.

## 8. enum 값

enum 타입과 variant는 PascalCase가 기준이다.

```pergyra
enum SecurityLevel { Basic, Hardware, Encrypted }
enum Color { Red, Green, Blue }
```

## 9. 모듈과 네임스페이스

네임스페이스 이름은 PascalCase가 자연스럽다.

```pergyra
namespace Math {
    export func Add(a: Int, b: Int) -> Int {
        return a + b;
    }
}
```

파일명은 `snake_case.pgy`를 권장한다.

예:
- `math_lib.pgy`
- `channel_test.pgy`
- `beta_modules_generics.pgy`

## 10. 도메인 계층 이름

고수준 도메인 선언은 PascalCase가 기준이다.

```pergyra
ability Damageable { }
role PlayerDamageable for Player { }
party DungeonTeam { }
roster CombatSystem { }
world GameWorld { }
subject Counter { }
```

## 11. 현재 주의할 점

과거 문서에는 다음이 섞여 있었다.

- 속성/프로퍼티 중심 OOP 규칙
- C# 스타일 getter/setter 문법
- package-style dotted naming

이들은 현재 구현 기준의 핵심 네이밍 규칙이 아니다.

현재 문서화 기준은:

1. 키워드는 소문자
2. 타입과 built-in은 PascalCase
3. 지역 변수는 camelCase
4. 파일명은 `snake_case.pgy`
5. 도메인 선언 이름은 PascalCase
