# Pergyra 선언과 인스턴스화 모델

## 첫 번째 질문

> **"함수부터 만들자"가 아니라 "이 프로그램에서 움직이는 존재는 누구인가?"부터 시작한다.**

## 핵심 원칙

> **타입은 종(種)이고, slot은 개체(個體)다. zone은 서식지(棲息地)다.**

Pergyra의 도메인 계층은 **중첩(nesting)**이 아니라 **선언(declaration) + 참조(instantiation)**로 구성된다.

## 생태계 비유

생물학에서:
- 포식(predation)은 사바나에도, 바다에도, 숲에도 존재한다
- 공생(symbiosis)은 특정 바이옴에 귀속되지 않는다
- 바이옴은 "어떤 관계가 여기서 활성화되는가"를 결정할 뿐이다

Pergyra에서:
- `relation Alliance { ... }` -- 관계 **유형**을 정의 (종의 상호작용 패턴)
- `effect Poisoned { ... }` -- 환경 효과 **유형**을 정의 (환경 압력의 종류)
- `zone BattleZone { relation slot alliance: Alliance; }` -- 이 바이옴에서 이 관계가 **활성화**됨

## 왜 중첩이 아닌가

중첩 모델의 문제:

```pergyra
// 중첩 모델 (채택하지 않음) -- relation이 zone 안에 갇힘
zone BattleZone {
    relation Alliance {       // 여기서만 존재
        shared trust: Int;
    }
}

zone TradeZone {
    // Alliance를 재사용하려면? 다시 선언해야 한다
    // 코드 중복, 계약 분리 불가
}
```

선언 + 참조 모델:

```pergyra
// 유전자 라이브러리 -- 어디서든 참조 가능
relation Alliance for source: Player, dest: Player {
    shared trust: Int;
}

effect Poisoned for bearer: Player {
    shared damage: Int;
}

// 바이옴 A -- "이 구역에서 이것들이 작동한다"
zone BattleZone {
    subject slot attacker: Player;
    subject slot defender: Player;
    relation slot alliance: Alliance;    // 참조
    effect slot poison: Poisoned;        // 참조
    apply poison to defender;
    link alliance between attacker, defender;
}

// 바이옴 B -- 같은 관계를 다른 맥락에서 재사용
zone TradeZone {
    subject slot buyer: Player;
    subject slot seller: Player;
    relation slot alliance: Alliance;    // 같은 유형, 다른 개체
}
```

## 실제 구조

### top-level 선언 (종/유전자 정의)

모든 타입 정의는 프로그램 최상위에 선언된다:

| 선언 | 역할 | 비유 |
|------|------|------|
| `struct Vec2 { ... }` | 순수 값 타입 | 광물/분자 |
| `vessel HealthState { ... }` | 피동 상태 수용체 | 기관(organ) |
| `subject Player { ... }` | 능동 오케스트레이터 | 유기체/종(種) |
| `ability Combatable { ... }` | 자격 조건 | 유전형질 |
| `role Warrior for Player { ... }` | 자격의 구체적 이행 | 표현형 |
| `party Team { ... }` | 협력 단위 조합 | 무리/군집 구조 |
| `relation Alliance { ... }` | 개체 간 관계 유형 | 종 간 상호작용 패턴 |
| `effect Poisoned { ... }` | 환경 효과 유형 | 환경 압력의 종류 |
| `zone BattleZone { ... }` | 규칙 구역 | 바이옴 정의 |
| `systemic CombatSystem { ... }` | 관리 하위 시스템 | 생태계 순환 시스템 |
| `world GameWorld { ... }` | 실행/신뢰/실패 경계 | 생태계 전체 |

### 인스턴스화 (개체/서식)

zone, systemic, world 내부에서 slot으로 인스턴스화한다:

```
world GameWorld {
    systemic combat: CombatSystem;     // 관리 시스템 인스턴스
    zone battle: BattleZone;           // 바이옴 인스턴스
}

zone BattleZone {
    subject slot player: Player;       // 개체 인스턴스
    relation slot alliance: Alliance;  // 관계 인스턴스
    effect slot poison: Poisoned;      // 효과 인스턴스
}

systemic CombatSystem {
    party slot team: Team;             // 무리 인스턴스
}
```

### 개념 계층 vs 코드 구조

개념적 소속 관계 (사고 모델):
```
world (생태계)
  zone (바이옴)
    relation (개체 간 관계)
    effect (환경 효과)
    subject (유기체/오케스트레이터)
      vessel (기관/수용체)
      ability (유전형질)
      role (표현형)
      action (플롯 행위)
      object/dto (피동 대상/투영)
      party (무리)
```

코드 구조 (실제 구현):
```
프로그램 최상위:
  struct, vessel                       -- 값/수용체 선언
  subject, ability, role, party        -- 개체 계층 선언
  relation, effect                     -- 상호작용 선언
  zone, systemic                       -- 구역/시스템 선언
  world                                -- 생태계 선언

subject 내부:
  vessel slot                          -- 피동 수용체 소유
  action                               -- 플롯 행위 선언 (func 아님)

world 내부:
  systemic slot, zone slot             -- 인스턴스화

zone 내부:
  subject slot, relation slot,         -- 인스턴스화
  effect slot, authority, state        -- 규칙 정의
```

**개념 계층은 맞다. 코드에서는 "선언은 평탄하게, 조합은 slot으로"라는 원칙으로 구현한다.**

## subject-first 원칙

Pergyra는 **subject-first** 언어다. 프로그램 설계는 소설을 쓰는 것과 같다.

**subject는 주인공이다.** 모든 것은 주인공에서 시작한다:

```
subject  = 주인공 (의사결정, 오케스트레이션, 승인)
vessel   = 주인공의 내면/소지품/신체 기관 (피동적 수용체)
ability  = 주인공의 재능/자질 (선천적 자격)
role     = 주인공이 맡는 역할 (전사, 치유사, 상인)
action   = 주인공의 행동 (플롯 비트, zone/effect와 연동)
party    = 주인공의 일행 (협력 단위)

object   = 주인공에 의해 영향받는 존재/사물 (수동 투영)
dto      = 외부에 전달되는 주인공의 소식/평판 (경계 투영)

relation = 주인공과 다른 주인공 사이의 관계 (동맹, 적대, 사제)
effect   = 주인공에게 닥치는 시련/축복 (독, 저주, 강화)

zone     = 이야기의 무대/장(章) (전투, 교역, 탐험)
world    = 소설 전체
```

설계 순서는 이 서사를 따른다:

1. **주인공을 만든다** -- `subject Player { ... }`
2. **주인공의 내면을 구성한다** -- `vessel HealthState { ... }`, `vessel Inventory { ... }`
3. **주인공에게 자질을 부여한다** -- `ability Combatable { ... }`
4. **자질을 구체적 역할로 발현한다** -- `role Warrior for Player impl Combatable { ... }`
5. **주인공의 행동을 선언한다** -- `action Attack(self, target) requires Combatable within BattleZone { ... }`
6. **주인공에게 시련과 관계를 부여한다** -- `effect Poisoned { ... }`, `relation Alliance { ... }`
7. **주인공이 영향을 미치는 존재를 정의한다** -- `object PlayerView { ... }`
8. **이야기의 무대를 연다** -- `zone BattleZone { subject slot player: Player; ... }`
9. **소설을 완성한다** -- `world GameWorld { zone battle: BattleZone; ... }`

**world가 컨테이너이고 zone이 무대이지만, 이것들은 주인공(subject)의 이야기를 담기 위해 존재한다.** 무대 없는 주인공은 가능하지만, 주인공 없는 무대는 의미가 없다.

## 이 설계의 장점

1. **재사용성** -- 같은 relation/effect를 여러 zone에서 사용 가능
2. **관심사 분리** -- 타입 정의와 인스턴스화가 분리되어 각각 독립 테스트 가능
3. **컴파일 타임 계약 검증** -- zone이 relation/effect의 타입 계약을 참조 시점에 검증
4. **점진적 조합** -- 작은 단위(ability)부터 큰 단위(world)까지 단계적으로 조립

## systemic vs zone

둘 다 world의 직속이지만 역할이 다르다:

- **zone** = 물리적 구역. subject, relation, effect가 활성화되는 공간. "어디서 무슨 일이 일어나는가"
- **systemic** = 관리 시스템. party를 조직하고 운영하는 로직. "누가 어떻게 협력하는가"

생태계 비유:
- zone = 사바나, 심해, 열대우림 (서식 환경)
- systemic = 먹이 사슬 시스템, 번식 시스템, 이주 시스템 (생태 순환)

## vessel과 action의 위치 (참조: docs/26)

vessel-action 모델의 상세 설계는 [26_vessel_action_model.md](26_vessel_action_model.md)에 정리되어 있다.

핵심 요약:

- **vessel**: subject 안에서 상태/자원/행위를 피동적으로 담는 수용체. func를 가진다.
- **action**: subject 전용 공적 동사. zone/ability/effect와 연동되는 오케스트레이션 행위다.
- **subject는 func와 action을 모두 가진다.** `func`는 계산/보조 판단/국소 상태 갱신용 hosted func이고, `action`은 공적 행위다.

## 결정 이력

- 선언 + 참조 모델 채택, 중첩 모델 비채택
- 근거: 재사용성, 관심사 분리, 컴파일 타임 계약 검증
- 비유: "타입은 종이고, slot은 개체다. zone은 서식지다."
- 2026-04-04: vessel, action 키워드 채택 (god subject 방지, subject-first 유지)
