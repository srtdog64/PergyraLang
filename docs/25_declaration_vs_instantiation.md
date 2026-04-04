# Pergyra 선언과 인스턴스화 모델

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
| `subject Player { ... }` | 개체 타입 | 종(種) 정의 |
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
    subject (개체)
      ability (유전형질)
      role (표현형)
      party (무리)
```

코드 구조 (실제 구현):
```
프로그램 최상위:
  subject, ability, role, party        -- 개체 계층 선언
  relation, effect                     -- 상호작용 선언
  zone, systemic                       -- 구역/시스템 선언
  world                                -- 생태계 선언

world 내부:
  systemic slot, zone slot             -- 인스턴스화

zone 내부:
  subject slot, relation slot,         -- 인스턴스화
  effect slot, authority, state        -- 규칙 정의
```

**개념 계층은 맞다. 코드에서는 "선언은 평탄하게, 조합은 slot으로"라는 원칙으로 구현한다.**

## subject-first 원칙

Pergyra는 **subject-first** 언어다. 구조적으로 world가 최상위이지만, **설계의 출발점은 항상 subject**다.

```
"이 세계에 어떤 존재가 있는가?"          → subject
"이 존재는 무엇을 할 수 있는가?"         → ability
"구체적으로 어떻게 하는가?"              → role
"누구와 협력하는가?"                     → party
"존재 간에 어떤 관계가 있는가?"          → relation
"환경이 존재에 어떤 영향을 주는가?"       → effect
"어디서 일어나는가?"                     → zone
"전체 생태계는 무엇인가?"               → world
```

world가 컨테이너이고 zone이 구역이지만, 이것들은 **subject를 담기 위해 존재한다.** 집을 지을 때 도시 계획(world)이 상위이지만, 설계는 "누가 살 것인가(subject)"에서 시작하는 것과 같다.

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

## 결정 이력

- 설계: 선언 + 참조 모델 채택, 중첩 모델 비채택
- 근거: 재사용성, 관심사 분리, 컴파일 타임 계약 검증
- 비유: "타입은 종이고, slot은 개체다. zone은 서식지다."
