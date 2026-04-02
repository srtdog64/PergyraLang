# Pergyra Party System Design
## 역할 협력 모델

## 설계 방향

Party는 역할들의 협력적 실행 단위를 표현하기 위한 설계 요소다.
이 문서는 role slot, context, 병렬 실행 모델을 한 단위로 다루는 방법을 정리한다.

여기서 중요한 전제는 다음과 같다.

- `struct`는 값 타입이다
- `class`는 party에 참여하는 실제 객체 타입이다
- `role`은 class가 특정 ability 묶음을 수행하도록 바인딩한다
- `party`는 role slot에 class 객체를 꽂아 협력시키는 실행 단위다

## Party의 핵심 요소

### 1. **Role Slot (역할 슬롯)**
- Party가 요구하는 역할의 명세
- 컴파일 타임에 타입 안전성 보장
- 다중 ability 요구사항 지원
- 실제로는 "이 slot에 들어올 class 객체가 어떤 ability를 만족해야 하는가"를 뜻한다

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
ability를 수행하는 객체 slot이다.
즉 party는 struct 값을 담는 컨테이너가 아니라
class 객체들의 협력 단위다.

현재 구현은 이 철학을 향해 가는 중이지만, 아직 완전히 닫히진 않았다.
- role slot은 ability 계약을 표현한다
- role이 `struct` 값 타입에 바인딩되면 시맨틱 경고가 난다
- 하지만 party instance에 실제 class object를 꽂는 경로를 강하게 검증하는 단계까지는 아직 아니다
- 즉 현재 party는 "class collaboration model"을 향한 표면과 계약이 먼저 고정된 상태다

### Party 인스턴스 생성

```pergyra
// 각 역할을 수행할 객체들
let warrior = Warrior()  // class object, has Damageable & Taunting
let priest = Priest()    // class object, has Healing & Cleansing
let mage = Mage()        // class object, has DamageDealing

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
        func HealLowestHP(&mut self)
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
현재 party 안에서 협력 중인 class 객체들에 대한 능력 기반 접근이다.

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
실제로 수행할 수 있는 class인가를 보는 것이다.

### 2. **Context 접근 제어**
```pergyra
// context는 role 구현 내부에서만 사용 가능
role MageDPS for Mage
{
    impl ability DamageDealing
    {
        func CastFireball(&mut self)
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
    func MethodA(&self)
    {
        let b = context.GetRole<AbilityB>("roleB")  // &B (불변 참조)
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
    role slot support: Healing | Buffing  // OR 조합
    
    // 멤버 교체 메서드
    func SwapSupport(&mut self, newSupport: impl Healing | Buffing)
    {
        self.support = newSupport
        Log($"Support role swapped to {newSupport.GetName()}")
    }
}
```

이때도 교체 대상은 plain struct 값이 아니라
새 role binding을 가진 class 객체다.

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
- [ ] AST에 `AST_PARTY_DECL` 노드 추가
- [ ] `role slot` 문법 파싱
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
