# Pergyra vs OOP: 공통점과 차이점 (2026-04-05)

## 한 줄 요약

> Pergyra는 OOP의 `.` 접근 문법과 캡슐화를 공유하지만, 상속을 제거하고 행위를 3자 계약으로 대체했다.

## 공통점

### 1. 타입 안에 상태와 행위를 묶는다

```java
// Java
class Player {
    int hp;
    void attack() { ... }
}

// Pergyra
subject Player {
    let hp: Int;
    func Calculate(self) -> Int { ... }
}
```

캡슐화의 기본 형태는 동일하다 -- 데이터와 그 데이터를 다루는 함수가 한 선언 안에 있다.

### 2. `.` 접근 문법

```java
player.hp
player.attack()
```

```pergyra
player.hp
player.Calculate()
```

멤버 접근과 함수 호출이 `.`으로 동일하다.

### 3. 생성자 (positional constructor)

```java
new Player("Hero", 100)
```

```pergyra
Player("Hero", 100)
```

`new` 키워드 없이 타입 이름으로 직접 생성.

### 4. self/this

```java
this.hp = 100;
```

```pergyra
self.hp = 100;
// 또는 bare field: hp = 100;
```

현재 인스턴스를 가리키는 self 참조.

### 5. 인터페이스/프로토콜

```java
interface Damageable {
    void takeDamage(int amount);
}
```

```pergyra
ability Damageable {
    func TakeDamage(self, amount: Int) -> Void;
}
```

행위 계약을 타입과 분리해서 선언.

---

## 차이점

### 1. 상속이 없다

```java
// OOP -- 상속 계층
class Animal { }
class Dog extends Animal { }
class GoldenRetriever extends Dog { }
// 3단 상속, 다이아몬드 문제, fragile base class
```

```pergyra
// Pergyra -- ability + role 조합
ability Movable { func Move(self) -> Void; }
ability Feedable { func Feed(self) -> Void; }

subject Dog { ... }
role DogMover for Dog {
    impl Movable { func Move(self) -> Void { ... } }
}
role DogFeeder for Dog {
    impl Feedable { func Feed(self) -> Void { ... } }
}
```

상속 대신 **ability(계약) + role(이행)**으로 행위를 조합한다.
다이아몬드 문제가 구조적으로 불가능하다.

### 2. 행위의 소유권이 다르다

```java
// OOP -- 객체가 행위를 "소유"
player.attack(enemy);     // Player 안에 attack이 갇혀 있음
// 어디서든 호출 가능, 제약 없음
```

```pergyra
// Pergyra -- 행위는 3자 계약
action Attack(self, target: Player) -> Int
    requires Combatable        // 자격: ability가 있는가?
    within BattleZone          // 장소: 이 zone에서 허용되는가?
    causes DamageEffect        // 결과: 어떤 effect가 발생하는가?
    authorized by self, target // 승인: 누가 동의해야 하는가?
{
    ...
}
```

OOP는 `player.attack()`이면 끝.
Pergyra는 "자격 + 장소 + 결과 + 승인"을 **컴파일 타임에** 검증한다.

### 3. 상태와 정체성의 분리

```java
// OOP -- 한 class 안에 전부
class Player {
    int hp;              // 상태
    String name;         // 정체성
    Weapon weapon;       // 소유물
    void attack() { }    // 행위
    // 전부 한 덩어리
}
```

```pergyra
// Pergyra -- 키워드로 관심사 분리
vessel HealthState {             // 상태 -> vessel
    current: Int;
    max: Int;
}

class Item {                     // 도구 -> class
    let name: String;
    let damage: Int;
}

subject Player {                 // 정체성 + 오케스트레이션 -> subject
    let name: String;
    vessel health: HealthState;  // "내 내부 상태"
    let weapon: Item;            // "내가 가진 사물"
    action Attack(self, ...) {}  // "내가 하는 행동"
}

role Warrior for Player {        // 자격 이행 -> role
    impl Combatable { ... }
}
```

OOP는 한 class에 넣고 주석으로 구분.
Pergyra는 **키워드가 곧 관심사 경계**.

### 4. 선언 키워드가 의미론을 결정한다

```java
// Java -- class 하나로 전부
class Player { }      // 도구/사물/value host인가, 주체인가가 이름만으로는 덜 드러남
class Vec2 { }        // 위와 문법적으로 동일
class PlayerView { }  // 위와 문법적으로 동일
```

```pergyra
// Pergyra -- 키워드 = 의미론
subject Player { }    // 참조 타입, action 가능, zone 통합
class Item { }        // 값 타입, func만, 도구/사물
struct Vec2 { }       // 순수 데이터, 최소 타입
vessel HP { }         // 피동 수용체, subject 내부
object PlayerView { } // 내부 관찰/조회용
tobject PlayerPacket { }  // 경계 밖 전송용
```

| 키워드 | self | 전달 방식 | action | zone 통합 | 용도 |
|--------|------|----------|--------|----------|------|
| subject | pointer | 참조 | O | O | 행동 주체 |
| class | value | 복사 | X | X | 도구/사물 |
| struct | value | 복사 | X | X | 순수 데이터 |
| vessel | pointer | canonical 복사; 현재 C/LLVM 일반 인자 자동 간접 결함 | X | X | 내부 수용체 |
| object | value | 복사 | X | X | 읽기 스냅샷 |
| tobject | value | 복사 | X | X | 경계 전송 |

메모:

- `object`는 내부 관찰 모델이다.
- `tobject`는 boundary transfer 모델이다.
- 표의 `self`와 전달 방식은 서로 다른 축이다. vessel hosted receiver는 원본
  내부 상태 갱신을 위해 pointer-self지만 일반 파라미터는 값이어야 한다. 현재
  backend가 두 축을 `uses_pointer_self` 하나로 합친 동작은 언어 철학이 아니라
  열린 ABI 결함이다.
- 큰 telemetry를 zero-copy로 넘기는 정책은 여기 포함되지 않는다. 그건 snapshot/generation/lease 계층의 일이다.

### 5. 세계 모델이 언어에 내장되어 있다

```java
// OOP -- 프레임워크/패턴으로 구현
class BattleZone { ... }  // 그냥 class, 특별한 의미 없음
class DamageEffect { ... } // 그냥 class
// zone/effect/relation은 프로그래머가 패턴으로 직접 구축
```

```pergyra
// Pergyra -- 언어가 세계 모델을 안다
zone BattleZone {
    subject slot attacker: Player
    subject slot defender: Player
    effect slot damage: DamageEffect
    authority attacker requires Combatable
}

world GameWorld {
    zone battle: BattleZone
    state battleReady: zone battle
    activate battle
}
```

zone, effect, relation, world가 **1등 시민(first-class citizen)**이다.
컴파일러가 "이 zone에 이 subject가 들어갈 수 있는가", "이 effect가 이 대상에 적용 가능한가"를 검증한다.

OOP에서는 이런 검증을 런타임 assertion이나 프레임워크 규칙으로 한다.
Pergyra에서는 **컴파일 타임에** 한다.

### 6. "method"가 없다

```java
// OOP -- method는 핵심 개념
public void attack(Enemy target) { ... }
```

```pergyra
// Pergyra -- "method"라는 용어 자체가 없음
// 대신:
//   free func     = top-level 함수
//   hosted func   = 타입에 귀속된 func (OOP의 method에 해당)
//   general func  = subject 안의 사적 계산
//   action        = subject 전용 공적 행위
```

"method"가 암시하는 상속/오버라이딩/가상 디스패치를 의도적으로 제거.
대신 4종류의 func/action 분류로 행위의 **성격**을 명시.

---

## 비유로 정리

```
OOP:
  "세계는 객체로 구성된다. 객체가 상태와 행위를 가진다.
   객체끼리 메시지를 보내고, 상속으로 확장한다."

Pergyra:
  "세계는 주인공(subject)과 사물(class)로 구성된다.
   주인공은 무대(zone) 위에서 자격(ability)을 가지고 행동(action)한다.
   행동의 결과(effect)와 관계(relation)는 세계(world)가 추적한다.
   상속은 없다. 조합(role)으로 확장한다."
```

---

## 수치 비교

| 측면 | OOP (Java/C#) | Pergyra |
|------|---------------|---------|
| 타입 키워드 | 1 (`class`) | 6 (`subject`, `class`, `struct`, `vessel`, `object`, `tobject`) |
| 상속 | 있음 | 없음 |
| 인터페이스 | 1 (`interface`) | 2 (`ability` + `role`) |
| 행위 제약 | 없음 (런타임 assertion) | 4 (`requires`, `within`, `causes`, `authorized by`) |
| 세계 모델 | 프레임워크 의존 | 언어 내장 (`zone`, `effect`, `relation`, `world`) |
| 메서드 | 1종 (`method`) | 4종 (`free func`, `hosted func`, `general func`, `action`) |
| 가시성 | 3+ (`public`, `private`, `protected`) | `public`, `private`, `protected` + ability 전용 `innate` |
