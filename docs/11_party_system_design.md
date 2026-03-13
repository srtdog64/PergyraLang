# Pergyra Party System Design
## 역할의 협력을 위한 혁신적인 추상화

## 🎯 핵심 철학: "개별에서 협력으로"

Party는 단순한 객체 컨테이너가 아닙니다. 이는 **역할들의 협력적 실행 단위**로, 복잡한 다중 에이전트 시스템을 직관적으로 모델링하는 Pergyra만의 독창적인 개념입니다.

## 📋 Party의 3대 핵심 요소

### 1. **Role Slot (역할 슬롯)**
- Party가 요구하는 역할의 명세
- 컴파일 타임에 타입 안전성 보장
- 다중 ability 요구사항 지원

### 2. **Context (컨텍스트)**
- Party 내 역할 간 안전한 상호작용
- 런타임 서비스 로케이터 패턴
- 순환 참조 방지

### 3. **Parallel Orchestration (병렬 조율)**
- Party 단위의 병렬 실행
- 각 역할의 parallel on 블록 자동 실행
- 동기화와 통신 자동 관리

## 🏗️ Party 시스템 아키텍처

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

### Party 인스턴스 생성

```pergyra
// 각 역할을 수행할 객체들
let warrior = Warrior { ... }  // has Damageable & Taunting
let priest = Priest { ... }    // has Healing & Cleansing
let mage = Mage { ... }       // has DamageDealing

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

## 🔒 안전성 보장 메커니즘

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

## 🚀 고급 기능

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

## 📊 Party와 Effect System 통합

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

## 🎮 실제 사용 예제: MOBA 게임

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

## 🔧 구현 로드맵

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

## 💡 Party System의 혁신성

1. **세계 최초의 역할 기반 협력 모델**: 객체 지향의 한계를 넘어선 새로운 패러다임
2. **컴파일 타임 협력 검증**: 팀 구성의 타당성을 컴파일러가 보장
3. **자동 병렬 오케스트레이션**: 복잡한 병렬 로직을 단순하게 추상화
4. **컨텍스트 기반 통신**: 안전하고 직관적인 역할 간 상호작용

Party 시스템은 Pergyra를 단순한 시스템 프로그래밍 언어를 넘어, **복잡한 협력 시스템을 모델링하는 최고의 도구**로 만들 것입니다.
