# Pergyra Vessel-Action 모델 (설계 결정)

## 핵심 문제: God Subject 방지

subject-first 철학을 그대로 밀면 가장 먼저 생기는 위험이 **god subject**다.
subject가 모든 상태, 모든 자원, 모든 메서드를 직접 들고 있으면 OOP의 god object와 같은 문제가 발생한다.

해법:

> **subject-first는 맞지만, subject-big가 되면 안 된다.**

## 설계 원칙

### subject는 orchestrator shell이다

subject가 직접 맡는 것:

- **의사결정** (왜, 언제)
- **오케스트레이션** (누구에게 시킬지)
- **승인** (허용/거부)
- **전이 시작점** (상태 변경의 트리거)

subject가 직접 맡지 않는 것:

- 큰 상태 보유 --> vessel로 분리
- 계산 세부사항 --> vessel의 func로 위임
- 데이터 변환과 외부 표면 --> object/dto로 위임

### vessel은 피동적 수용체다

vessel은 subject 안에서 운용되는 내부 수용체로, 다섯 가지 피동성을 담는다:

| 피동 축 | 역할 | 예시 |
|---------|------|------|
| 상태 피동 | HP, inventory, cooldown, status stack | `vessel HealthState { current: Int; max: Int; }` |
| 행위 피동 | 스스로 결정하지 않지만 호출되면 수행 | `func ApplyDamage(self, amount: Int)` |
| 자원 피동 | handle, slot, connection, device | `vessel ResourceHolder { conn: Slot<Connection>; }` |
| 투영 피동 | object/dto로 나가는 projection source | vessel 기반 projection이 자연스러움 |
| 규칙 피동 | effect 적용 대상, relation 연결 대상 | zone/world 규칙이 덮이는 지점 |

한 줄 정의:

> **vessel은 subject가 상태, 자원, 수동 실행, projection, 규칙 적용점을 담아 운용하는 내부 수용체다.**

## action vs func: 두 종류의 동사

### 핵심 구분

```
func   = "어떻게"를 안다 (계산/실행)
action = "왜, 누구에게, 어디서"를 안다 (의사결정/오케스트레이션)
```

### func -- hosted func (귀속 func)

Pergyra에서 "method"라는 용어는 쓰지 않는다. 타입 안에 선언되고 `self`를 받는 func를 **hosted func (귀속 func)**이라 부른다.

- **free func**: top-level 함수
- **hosted func**: 타입에 귀속된 func (self 바인딩)
- **general func**: subject 안의 일반 func (사적 판단)

func는 데이터에 붙은 계산이다. struct, vessel, object, dto, role, subject 안에서 사용된다.

```pergyra
vessel HealthState {
    current: Int;
    max: Int;

    func ApplyDamage(self, amount: Int) -> Void {
        self.current = Max(self.current - amount, 0);
    }

    func Heal(self, amount: Int) -> Void {
        self.current = Min(self.current + amount, self.max);
    }

    func IsDead(self) -> Bool {
        return self.current <= 0;
    }
}
```

func의 특성:

- 누구나 호출 가능
- zone/authority 제약 없음
- effect 추적 없음
- 순수 계산 또는 수동 상태 변경

### action -- 플롯 행위

action은 subject 전용이다. zone, ability, effect와 연동되는 선언적 행위.

```pergyra
action Attack(self, target: Player) -> Result<Void>
    requires Combatable
    within BattleZone
    causes DamageEffect {

    let power = self.combat.Calculate(self.power, target.defense);
    target.health.ApplyDamage(power);
}
```

action의 5가지 속성:

| 속성 | 키워드 | 의미 | 컴파일 타임 검증 |
|------|--------|------|-----------------|
| 자격 | `requires` | ability 필요 | subject가 해당 ability를 가졌는지 |
| 무대 | `within` | zone 제약 | 이 action이 해당 zone에서 호출 가능한지 |
| 결과 | `causes` | effect 선언 | effect가 정의되어 있는지, target이 맞는지 |
| 승인 | `authorized by` | 승인 주체 | 해당 subject들이 zone authority를 가졌는지 |
| 본문 | body | 오케스트레이션 | vessel/role 위임 코드 |

### 각 키워드의 동사 형태

| 키워드 | 동사 | 성격 |
|--------|------|------|
| struct / dto | func | 순수 계산 (상태 변이 없음) |
| object | func | 피동 반응과 helper 계산 |
| vessel | func | 읽기 전용 계산 (value-self, mutation 없음) |
| role | func | ability 이행 (계약의 구체화) |
| subject | **func + action** | func = 사적 판단, action = 공적 행위 |
| ability | func 시그니처 | 계약 (구현 없음) |
| zone | -- | action의 허용/거부 판정 |

**subject는 func와 action을 모두 가진다.**

- `func` = 주인공의 **사적 판단** (관객이 안 본다, 내부 계산, zone/effect 연동 없음)
- `action` = 주인공의 **공적 행동** (관객이 본다, zone/effect/authority 연동)

주인공이 머릿속으로 계산도 못 하면 소설이 안 된다.

### subject 참조 전달

subject는 identity-bearing 타입이므로, 함수 파라미터로 전달할 때 **언어가 자동으로 reference 전달**한다. 사용자는 포인터를 의식하지 않는다.

```pergyra
func DoAttack(attacker: Fighter, target: Fighter) -> Void {
    let dmg = attacker.Attack(target);  // 원본 subject 참조
    Log(ToString(dmg));
}
```

## 전체 예시

```pergyra
// -- 값 타입 --
struct Vec2 {
    x: Float;
    y: Float;
}

struct Item {
    name: String;
    weight: Int;
}

// -- vessel: 피동적 수용체 --
vessel HealthState {
    current: Int;
    max: Int;

    func ApplyDamage(self, amount: Int) -> Void {
        self.current = Max(self.current - amount, 0);
    }

    func Heal(self, amount: Int) -> Void {
        self.current = Min(self.current + amount, self.max);
    }

    func IsDead(self) -> Bool {
        return self.current <= 0;
    }
}

vessel Inventory {
    items: Array<Item>;
    capacity: Int;

    func Add(self, item: Item) -> Result<Void> {
        if Len(self.items) >= self.capacity {
            return Err("inventory full");
        }
        ArrayPush(self.items, item);
        return Ok(());
    }

    func Remove(self, item: Item) -> Result<Item> {
        // ...
    }

    func IsFull(self) -> Bool {
        return Len(self.items) >= self.capacity;
    }
}

vessel CombatStats {
    power: Int;
    defense: Int;

    func Calculate(self, target_def: Int) -> Int {
        return Max(self.power - target_def, 0);
    }
}

// -- ability: 행위 계약 --
ability Combatable {
    func GetPower(self) -> Int;
}

ability Tradeable {
    func CanTrade(self) -> Bool;
}

// -- role: 계약 이행 --
role Warrior for Player impl Combatable {
    func GetPower(self) -> Int {
        return self.combat.power;
    }
}

// -- subject: 능동 오케스트레이터 --
subject Player {
    // vessel 소유 (상태는 여기에)
    vessel health: HealthState;
    vessel inventory: Inventory;
    vessel combat: CombatStats;

    // subject는 func가 아니라 action으로 행동한다
    action Attack(self, target: Player) -> Result<Void>
        requires Combatable
        within BattleZone
        causes DamageEffect {

        let dmg = self.combat.Calculate(target.combat.defense);
        target.health.ApplyDamage(dmg);
    }

    action Trade(self, other: Player, item: Item) -> Result<Void>
        requires Tradeable
        within TradeZone
        authorized by self, other {

        let removed = self.inventory.Remove(item)?;
        other.inventory.Add(removed)?;
    }
}

// -- object: 투영 --
object PlayerView {
    name: String;
    health: Int;
    level: Int;
}

// -- effect, relation, zone, world --
effect DamageEffect for target: Player {
    shared damage: Int;
}

relation Alliance for source: Player, dest: Player {
    shared trust: Int;
}

zone BattleZone {
    subject slot attacker: Player;
    subject slot defender: Player;
    effect slot damage: DamageEffect;
    relation slot alliance: Alliance;
}

world GameWorld {
    zone battle: BattleZone;
}
```

## 존재론 계층 (최종)

```
struct    = 순수 값 (광물/분자)
vessel    = 피동 상태+메서드 (기관/organ)
subject   = 능동 오케스트레이터 (유기체)
object    = 읽기 전용 투영 (그림자)
dto       = 경계 밖 전송 투영 (소식/평판)
ability   = 행위 계약 (유전형질)
role      = 행위 이행 (표현형)
action    = 플롯 행위 (주인공의 행동)
relation  = 주체 간 관계 (종 간 상호작용)
effect    = 환경 효과 (시련/축복)
zone      = 규칙 구역/무대 (바이옴)
world     = 생태계 전체 (소설)
```

## 소설 비유 (완성)

```
subject  = 주인공
vessel   = 주인공의 내면/소지품/신체 기관
ability  = 주인공의 재능
role     = 주인공이 맡은 역할
action   = 주인공의 행동 (플롯 비트)
object   = 주인공의 모습 (스냅샷)
dto      = 주인공에 대한 소문/편지
relation = 주인공 간의 관계
effect   = 행동의 결과/시련
zone     = 장면/무대
world    = 소설 전체
```

## subject와 vessel의 경계

```
subject = "왜/언제/무엇을 한다" (의사결정)
vessel  = "무엇을 들고 있고 어떻게 갱신된다" (상태 관리)
role    = "어떤 자격으로 행동하는가" (행위 자격)
```

세 가지는 대체재가 아니라 직교하는 축이다:

- vessel이 없으면 subject가 모든 상태를 직접 들고 god subject가 된다
- role이 없으면 subject가 모든 ability를 직접 구현해 god subject가 된다
- action이 없으면 subject가 func 덩어리가 되어 object와 구분이 없다

## 타입별 2x2 매트릭스

```
           상태 없음          상태 있음
         +----------------+----------------+
행위 없음 | struct / dto    |                |
         | 순수 값          |  (해당 없음)    |
         +----------------+----------------+
행위 있음 | ability         | vessel         |
(피동)   | (계약만, 구현X)  | 피동 상태+메서드 |
         +----------------+----------------+
행위 있음 |                | subject        |
(능동)   |  (해당 없음)    | action 오케스트레이터 |
         +----------------+----------------+
```

## func가 허용되는 곳 / action이 허용되는 곳

| 키워드 | func 허용 | action 허용 |
|--------|----------|-------------|
| struct | O | X |
| dto | O | X |
| object | O | X |
| vessel | O | X |
| role | O (ability 이행) | X |
| ability | O (시그니처만) | X |
| subject | **O** (사적 판단) | **O** (공적 행위) |
| zone | func (내부 규칙 계산) | X |

## 결정 이력

- 2026-04-04: vessel 키워드 채택 (container, component, cell 비채택)
- 2026-04-04: action 키워드 채택 (subject 전용, func와 분리)
- 2026-04-04: 5대 피동 축 정의 (상태, 행위, 자원, 투영, 규칙)
- 2026-04-04: subject에 func 재허용 (실전 battle sim 구현 후 Anemic Domain Model 위험 확인)
- 2026-04-04: subject 참조 전달 허용 (포인터 숨김, 언어가 자동 reference 처리)
- 2026-04-04: vessel을 value-self로 확정 (순수 상태 묶음 + 읽기 전용 계산, mutation 없음)
- 근거: god subject 방지, subject-first 유지, 포인터 은닉, 오케스트레이터 패턴
- 구현 상태: core parser/semantic/C/LLVM surface 반영 완료. `vessel` 선언, subject-local `vessel` field, subject-only `action`, `requires/within/causes/authorized by` clause가 현재 구현에 연결되어 있고, `authorized by` subject-host 검증, `within` zone slot/authority 적합성, `causes` effect target/zone layer 적합성까지 semantic에 반영됨. 또한 zone method 안의 subject `action` call은 현재 C/LLVM에서 matching `effect slot` runtime activation과 embedded layer sync로 이어진다.
