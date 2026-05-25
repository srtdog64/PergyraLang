# Pergyra Party System Design
## 역할 협력 모델

## 설계 방향

Party는 역할들의 협력적 실행 단위를 표현하기 위한 설계 요소다.
이 문서는 role slot, context, 병렬 실행 모델을 한 단위로 다루는 방법을 정리한다.

여기서 중요한 전제는 다음과 같다.

- `struct`는 값 타입이다
- `subject`는 party에 참여하는 실제 주체 타입이다
- 현재 surface syntax에서는 `subject`와 `class`가 같은 declaration으로 동작한다
- `role`은 subject가 특정 ability 묶음을 수행하도록 바인딩한다
- `party`는 role slot에 subject를 꽂아 협력시키는 실행 단위다

중요:

- `party`는 collaboration unit이지 state boundary가 아니다
- `zone/world`는 authority/projection/lifecycle을 가진 state boundary다
- `vessel`은 subject-local state container다

즉 A와 B가 협력해서 무언가를 수행하면 먼저 `party`를 의심하고,
A와 B 사이의 공유 상태/관계/효과/승인/전이를 다루면 `zone/relation/effect`를 의심하는 것이 맞다.

## Current Implementation Surface (2026-04-03)

현재 파서/시맨틱/백엔드에서 지원하는 문법:

```pergyra
party Team {
    role slot tank: Damageable;
    role slot healer: Healing;
    shared formation: String = "standard";
    func Execute() -> Void {
        Log(formation);
    }
}

bind team.tank = Warrior;
```

지원되는 요소:
- `party { role slot ...; shared ...; func ... }`
- `dyn role slot` (동적 바인딩 가능)
- top-level `bind party.slot = ClassName`

아래 섹션은 설계 방향 설명이며, 일부 예시는 현재 문법과 다를 수 있다.
특히 role slot의 복잡한 제약식과 advanced orchestration 예시는 장기 설계 메모로 읽는 편이 맞다.
또한 `entity`는 party 존재론의 코어 용어가 아니며, core에서는 `subject`를 기준으로 설명한다.
베타 안정 표면에서 role slot ability 조합은 최상위 `A & B` 교차만 지원한다.
`A | B` OR 조합과 `Array<A & B>` 같은 컨테이너 내부 교차 계약은 아직 안정 문법이 아니다.

## Party의 핵심 요소 (Design Notes)

### 1. **Role Slot (역할 슬롯)**
- Party가 요구하는 역할의 명세
- 컴파일 타임에 타입 안전성 보장
- 다중 ability 요구사항 지원
- 실제로는 "이 slot에 들어올 subject가 어떤 ability를 만족해야 하는가"를 뜻한다

### 2. **Context (컨텍스트)**
- Party 내 역할 간 안전한 상호작용
- 런타임 서비스 로케이터 패턴
- 순환 참조 방지

### 3. **Parallel Orchestration (병렬 조율)**
- Party 단위의 병렬 실행
- 각 역할의 parallel on 블록 자동 실행
- 동기화와 통신 자동 관리

## Party 시스템 아키텍처

### 기본 문법

```pergyra
// Party 정의: 역할의 청사진
party HolyPaladin
{
    // 역할 슬롯 정의 (ability 조합 요구)
    role slot tank: Damageable & Taunting
    role slot healer: Healing & Cleansing  
    role slot dps: DamageDealing
    
    // Party 공유 데이터
    shared formation: String = "Defensive"
    shared morale: Int = 100
    
    // Party 레벨 메서드
    func ExecuteStrategy()
    {
        // 모든 역할이 협력하는 전략 실행
    }
}
```

여기서 `tank`, `healer`, `dps`는 값 타입이 아니라
ability를 수행하는 subject slot이다.
즉 party는 struct 값을 담는 컨테이너가 아니라
subject들의 협력 단위다.

반대로 party가 직접 projection sync, lifecycle state, effect attachment, authority boundary를 소유하려 들면 zone/world와 책임이 겹친다. 그쪽은 party가 아니라 zone/relation/effect가 맡아야 한다.

현재 구현은 이 철학을 향해 가는 중이지만, 아직 완전히 닫히진 않았다.
- role slot은 ability 계약을 표현한다
- role이 non-subject nominal declaration에 바인딩되면 semantic error가 난다
- party role slot은 subject-bound role impl이 실제로 존재하는 ability만 받는다
- 즉 현재 party는 "subject collaboration model"을 향한 표면과 계약이 먼저 고정된 상태다

### Party 인스턴스 생성

```pergyra
// 각 역할을 수행할 주체들
let warrior = Warrior()  // subject instance, current syntax accepts class/subject declarations
let priest = Priest()
let mage = Mage()

// Party 구성
let raid = HolyPaladin
{
    tank: warrior,
    healer: priest,
    dps: mage
}
```

### Context를 통한 역할 간 상호작용

```pergyra
role PriestHealer for Priest
{
    impl ability Healing
    {
        func HealLowestHP() -> Void
        {
            // context로 party 멤버 접근
            let tank = context.GetRole<Damageable>("tank")
            let dps = context.GetRole<Damageable>("dps")
            
            // 가장 체력이 낮은 멤버 찾기
            let targets = [tank, dps].SortBy(|t| t.GetHealth())
            let lowest = targets.First()
            
            // 치유 시전
            if lowest.GetHealthPercent() < 50
            {
                lowest.Heal(CalculateHealAmount())
            }
        }
    }
}
```

`context`는 값 타입 모음에 대한 접근이 아니라,
현재 party 안에서 협력 중인 subject들에 대한 능력 기반 접근이다.

### Party 병렬 실행

```pergyra
// 각 역할의 병렬 로직 정의
role WarriorTank for Warrior
{
    parallel on (mainThread)
    {
        // 1초마다 위협 수준 갱신
        every (1000ms)
        {
            UpdateThreatLevel()
            CheckForIncomingAttacks()
        }
    }
}

role PriestHealer for Priest  
{
    parallel on (backgroundThread)
    {
        // 500ms마다 파티원 체력 체크
        every (500ms)
        {
            ScanPartyHealth()
            if context.AnyRole<Damageable>().NeedsHealing()
            {
                HealLowestHP()
            }
        }
    }
}

role MageDPS for Mage
{
    parallel on (computeThread)
    {
        // 최적 스킬 로테이션 계산
        continuous
        {
            let optimalRotation = CalculateOptimalRotation()
            ExecuteRotation(optimalRotation)
        }
    }
}

// 메인 게임 루프
async func GameLoop()
{
    let raid = CreateRaidParty()
    
    // Party 전체를 병렬 실행
    // 각 역할의 parallel on 블록이 지정된 스레드에서 동시 실행됨
    parallel (raid) join with all
    {
        // Warrior는 mainThread에서
        // Priest는 backgroundThread에서  
        // Mage는 computeThread에서
        // 모두 동시에 실행됨
    }
}
```

## 안전성 보장 메커니즘

### 1. **컴파일 타임 검증**
```pergyra
// 컴파일 에러: Warrior가 Cleansing ability를 구현하지 않음
let invalidParty = HolyPaladin
{
    tank: warrior,
    healer: warrior,  // Error: Warrior doesn't implement Cleansing
    dps: mage
}
```

이 검증의 의미는 단순 "메서드가 있나?"가 아니다.
`healer` slot에 들어갈 객체가 `Healing & Cleansing` role/ability 조합을
실제로 수행할 수 있는 subject인가를 보는 것이다.

### 2. **Context 접근 제어**
```pergyra
// context는 role 구현 내부에서만 사용 가능
role MageDPS for Mage
{
    impl ability DamageDealing
    {
        func CastFireball() -> Void
        {
            // ✅ OK: role 내부에서 context 사용
            let tank = context.GetRole<Taunting>("tank")
            let target = tank.GetCurrentTarget()
            
            DealDamage(target, CalculateFireballDamage())
        }
    }
}

// ❌ Error: 일반 함수에서는 context 사용 불가
func InvalidFunction()
{
    let tank = context.GetRole<Taunting>("tank")  // Compile Error!
}
```

### 3. **순환 참조 방지**
```pergyra
// Party는 불변 참조만 제공
role A for StructA
{
    func MethodA() -> Void
    {
        let b = context.GetRole<AbilityB>("roleB")
        b.ReadOnlyMethod()  // OK
        // b.MutatingMethod()  // Error: cannot mutate through & reference
    }
}
```

## 고급 기능

### 1. **Dynamic Party Composition**
```pergyra
// 런타임에 party 멤버 교체
party FlexibleTeam
{
    role slot attacker: DamageDealing
    role slot support: Healing  // future: Healing | Buffing OR 조합
    
    // 멤버 교체 메서드
    func SwapSupport(newSupport: Healing) -> Void
    {
        self.support = newSupport
        Log($"Support role swapped to {newSupport.GetName()}")
    }
}
```

이때도 교체 대상은 plain struct 값이 아니라
새 role binding을 가진 subject다.

### 2. **Party Inheritance**
```pergyra
// 기본 party 정의
party BasicDungeon
{
    role slot tank: Damageable & Taunting
    role slot healer: Healing
}

// 확장된 party
party AdvancedRaid extends BasicDungeon
{
    // 기존 role slot 상속됨
    role slot dps1: DamageDealing
    role slot dps2: DamageDealing
    role slot support: Buffing & Debuffing
}
```

### 3. **Party Templates with Generics**
```pergyra
// 제네릭 party
party Squad<T> where T: CombatReady
{
    role slot leader: T & Leadership
    role slot members: Array<T>
    
    shared objective: String
    
    func ExecuteMission()
    {
        leader.GiveOrders()
        parallel (members) join with all
        {
            // 모든 멤버가 병렬로 임무 수행
        }
    }
}

// 구체적 사용
let eliteSquad = Squad<Soldier>
{
    leader: commanderJohn,
    members: [soldier1, soldier2, soldier3]
}
```

## Party와 Effect System 통합

```pergyra
// Party 실행에 필요한 효과 명시
party NetworkedTeam
{
    role slot server: ServerRole
    role slot clients: Array<ClientRole>
    
    func Synchronize()
        with effects Network, Time
    {
        let timestamp = Time.Now()
        
        parallel (clients) join with all
        {
            Network.SendUpdate(timestamp)
        }
    }
}
```

## 실제 사용 예제: MOBA 게임

```pergyra
// MOBA 팀 구성
party MOBATeam
{
    role slot top: Fighter & Tanky
    role slot jungle: Mobile & DamageDealing
    role slot mid: BurstDamage & WaveClear
    role slot adc: SustainedDamage & Fragile
    role slot support: Healing & CrowdControl
    
    shared teamGold: Int = 0
    shared objectives: Set<Objective> = []
    
    // 팀 전략 실행
    func ExecuteTeamfight()
    {
        // 탱커가 먼저 진입
        context.GetRole("top").Engage()
        
        // 0.5초 후 딜러들 진입
        await Delay(500ms)
        
        parallel
        {
            context.GetRole("mid").BurstCombo()
            context.GetRole("adc").FocusPriority()
            context.GetRole("jungle").FlankTarget()
        }
        
        // 서포터는 지속적으로 아군 지원
        context.GetRole("support").ProtectCarries()
    }
}
```

## 구현 로드맵

### Phase 1: Core Party System
- [x] AST에 `AST_PARTY_DECL` 노드 추가
- [x] `role slot` 문법 파싱
- [x] 최상위 role slot ability 교차 `A & B` 파싱 및 semantic validation
- [ ] role slot OR 조합 `A | B`
- [ ] 컨테이너 내부 ability 교차 `Array<A & B>`
- [ ] Party 인스턴스 생성 검증

### Phase 2: Context System  
- [ ] Context 런타임 구현
- [ ] Role 접근 제어
- [ ] 타입 안전성 보장

### Phase 3: Parallel Integration
- [ ] Party 병렬 실행 엔진
- [ ] Role parallel on 수집
- [ ] 동기화 메커니즘

### Phase 4: Advanced Features
- [ ] Dynamic composition
- [ ] Party inheritance
- [ ] Generic parties

## 설계 요약

1. `role slot`은 Party가 요구하는 역할 계약을 표현한다.
2. `context`는 Party 내부 역할 간 참조 경로를 제공한다.
3. `parallel` 실행은 Party 단위 작업 분해와 연결된다.
4. 현재 문서는 설계 초안이며, 구현 전 로드맵과 예시 문법을 함께 포함한다.
