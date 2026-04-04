# Pergyra World-Systemic Architecture

## 🌍 Target Hierarchy

```
        [WORLD]    ← 최상위 실행/신뢰/실패 경계
           ↓
         [ZONE]    ← world 내부의 지역 규칙/서브시스템 공간
           ↓
       [EFFECT]    ← 현재 적용 중인 지속 규칙/상태 오버레이
           ↓
      [RELATION]   ← subject/party 사이의 관계 규칙
           ↓
        [PARTY]    ← 협력 단위
           ↓
        [ROLE]     ← 어떤 자격으로 수행하는가
           ↓
      [ABILITY]    ← 무엇을 할 수 있는가
           ↓
       [SUBJECT]   ← 상태와 identity를 가진 주체 타입
           ↓
        [STRUCT]   ← 값 타입

현재 surface syntax에서는 `subject`와 `class`가 같은 subject declaration으로 파싱된다.
```

핵심 원칙은 다음이다.

- 아래 레이어일수록 더 타입적이고 더 구속적이다
- 위 레이어일수록 더 문맥적이고 더 덜 구속적이다
- `world`는 단순 그룹이 아니라 최상위 실행 의미 경계다
- `zone`은 같은 `world` 내부의 지역 규칙 공간이다
- `entity`는 코어 언어 존재론이 아니라 프레임워크/도메인 용어로 남긴다

## Current Implementation Surface (2026-04-04)

```pergyra
ability Damageable {
    require health: Int;
    func TakeDamage(amount: Int) -> Void;
}

role WarriorTank for Warrior {
    impl ability Damageable {
        func TakeDamage(amount: Int) -> Void {
            Log(amount);
        }
    }
}

party DungeonTeam {
    role slot tank: Damageable;
    shared strategy: Int = 0;
}

systemic CombatSystem {
    party slot team1: DungeonTeam;
}

world GameWorld {
    systemic combat: CombatSystem;
}
```

현재 stable current surface는 대체로 `subject/class / ability / role / party / relation / effect / zone / systemic / world`까지다.
장기 의미론 이름은 `class`보다 `subject`가 더 정확하다고 본다.
`relation`, `effect`, `zone`은 이제 `for ...` header와 `subject slot` / `object slot` / `shared` / `func` 수준의 최소 body surface까지 올라왔고, `zone`은 `relation slot` / `effect slot` / `authority subjectSlot` / `state name: effect ... on ...` / `state name: relation ... between ..., ...` / `apply effectSlot to targetSlot` / `apply stateName` / `detach effectSlot from targetSlot` / `detach stateName` / `link relationSlot between left, right` / `link stateName` / `unlink relationSlot between left, right` / `unlink stateName` / `refresh objectSlot from subjectSlot` / `maintain effectSlot on targetSlot` / `maintain relationSlot between left, right` / `maintain stateName`과 optional `by subjectSlot` authority annotation까지 가진다. `world`는 `zone` slot으로 최소 조립이 가능하다. 또한 `apply/detach`는 `effect`의 subject target contract와, `link/unlink`는 `relation`의 two-endpoint contract와 기본 타입 정합성을 검사하고, `refresh`는 projection field 정합성을 검사하며, `maintain`은 duplicate/conflicting lifecycle rule에 warning을 낸다. `authority`는 mutable rule의 승인 주체를 검사하고, state shorthand는 kind mismatch를 semantic error로 보고한다. 아직 deeper propagation semantics는 얕다.
아래 섹션은 최종 목표 계층을 설명하며, 일부 예시는 현재 문법과 다를 수 있다.
특히 `relation`, `effect`, `zone`의 직접 문법, `actor` profile surface, `&mut self`, `impl Trait`, thread affinity 표기 예시는 현재 stable current surface를 직접 설명하지 않는다.

## Layer Semantics

### Ability

- 가장 강한 구속
- 타입 시스템과 가장 가까운 계약
- “무엇을 할 수 있는가”를 닫는 층

### Role

- ability를 특정 subject 문맥에 묶는 층
- “어떤 자격으로 수행하는가”를 나타냄

### Party

- 여러 subject가 role을 통해 협력하는 단위
- “누구와 협력하는가”를 나타냄

### Relation

- subject나 party 사이의 선형 관계를 표현하는 층
- 예: ally/enemy, producer/consumer, trusted/untrusted
- 최종 목표 모델에는 포함되지만 현재 stable syntax는 아님

### Effect

- subject나 협력 단위 위에 덧씌워지는 동적 상태 오버레이
- 예: poisoned, throttled, readOnly, maintenanceMode
- 현재 구현의 함수 effect system과는 연결되지만 같은 층으로 완전히 통합된 것은 아님

### Zone

- 같은 world 내부의 지역 규칙/서브시스템 공간
- 프론트엔드/백엔드/워커처럼 하나의 제품 안 문맥을 자를 때 기본 단위
- 최종 목표 모델에는 포함되지만 현재 stable syntax는 아님

### World

- 가장 덜 구속적이고 가장 큰 경계
- 실행 경계, 실패 전파 경계, 신뢰 경계, 배포 경계를 표현하는 최상위 단위
- Pergyra의 장기 모델에서 전체 시스템 바깥선을 담당

### Subject and Actor

- `subject`는 상태와 identity를 가진 주체 타입이다
- 현재 구현에서는 `subject`와 `class`가 같은 declaration surface로 동작한다
- `actor`는 subject와 병렬인 존재론적 종류가 아니라, simulation loop / mailbox / scheduler semantics가 붙은 subject profile로 보는 것이 목표다

### Object, DTO, and Entity

- `object`는 `subject`와 병렬인 새 존재론 계층이 아니다
- `subject`가 transfer / DTO / view / serialization 문맥으로 들어가면 수동적으로 다뤄지는 `object`처럼 해석될 수 있다
- 즉 `subject`는 본질적으로 능동적이지만, 문맥에 따라 object화될 수 있다
- `dto`는 그 object 표현 중 외부 API / IPC / persistence 경계를 넘기기 위해 축약된 projection이다
- `entity`는 이런 해석을 묶는 프레임워크 용어일 수는 있지만, Pergyra 코어 존재론에는 넣지 않는다

## World vs Zone

- 기본값: 전체 프로그램은 하나의 `world`
- 그 안의 UI/API/Worker/Admin 같은 하위 문맥은 `zone`
- 브라우저와 서버처럼 신뢰 경계와 실패 경계가 분리되면 `world`를 나누는 것이 맞다

즉 기본 권장은 다음과 같다.

```text
AppWorld
  ├─ FrontendZone
  ├─ BackendZone
  ├─ WorkerZone
  └─ AdminZone
```

분산/신뢰 경계를 더 강하게 표현하고 싶다면 다음도 가능하다.

```text
BrowserWorld
  └─ UiZone

ServerWorld
  ├─ ApiZone
  ├─ JobZone
  └─ DbAccessZone
```

## 📋 계층별 정의 (Design Notes)

### 0. **STRUCT / SUBJECT** - 값과 주체의 분리

- `struct`: 최소 값 타입. 복사/비교가 자연스러운 데이터
- `subject`: 상태와 identity를 가진 주체. ability의 수행 주체

현재 syntax 예시에서는 호환성 때문에 `class`를 자주 쓰지만, `subject`도 같은 의미로 허용된다.

즉 이 계층도는 사실
`subject` 위에 `ability`, `role`, `party`가 올라가는 구조다.

### 1. **ABILITY** - 요구 조건
가장 기본적인 행위 계약. 어떤 데이터/자원 셀과 동작이 필요한지 정의.

```pergyra
ability Damageable {
    require _healthSlot: Slot<Int>
    func TakeDamage(&mut self, amount: Int)
    func GetHealth(&self) -> Int
}
```

### 2. **ROLE** - 기능 단위
특정 subject가 ability를 어떻게 구현하는지 정의. 병렬 실행 로직 포함.

```pergyra
role WarriorTank for Warrior {
    impl ability Damageable { ... }
    impl ability Taunting { ... }
    
    parallel on (mainThread) {
        every (1000ms) { UpdateThreat() }
    }
}
```

### 3. **PARTY** - 실행 단위
여러 subject가 role slot을 통해 협력하는 실행 가능한 단위. 병렬 실행의 기본 단위.

```pergyra
party DungeonTeam {
    role slot tank: Damageable & Taunting
    role slot healer: Healing
    role slot dps: DamageDealing
    
    shared strategy: TeamStrategy
}
```

### 4. **SYSTEMIC** - 아키텍처 단위
관련된 party들을 모아서 하나의 시스템을 구성. 전체 시스템의 조합.

현재 구현에는 존재하지만, 장기적으로는 `zone`/`world` 층과 역할이 재정리될 수 있다.
즉 `systemic`은 현 단계의 조율/구성 단위이며, 최종 목표 계층의 절대적 중심축이라고 고정하지 않는다.

```pergyra
systemic CombatSystem {
    // 던전 팀들
    party slot dungeonTeam1: DungeonTeam
    party slot dungeonTeam2: DungeonTeam
    
    // 레이드 팀
    party slot raidTeam: RaidParty
    
    // PvP 매치
    party slot pvpMatch: PvPBattle
    
    // 시스템 전체 설정
    shared combatRules: CombatRules
    shared matchmaking: MatchmakingService
    
    // 시스템 레벨 조율
    func ScheduleMatches() {
        parallel {
            dungeonTeam1.StartDungeon()
            dungeonTeam2.StartDungeon()
            
            if pvpMatch.IsReady() {
                pvpMatch.StartBattle()
            }
        }
    }
}
```

### 5. **WORLD** - 최상위 컨테이너
모든 systemic의 모음. 전체 애플리케이션/게임 세계를 표현.

```pergyra
world GameWorld {
    // 핵심 시스템들
    systemic combat: CombatSystem
    systemic economy: EconomySystem
    systemic social: SocialSystem
    systemic crafting: CraftingSystem
    
    // 월드 전체 상태
    shared worldTime: GameTime
    shared activeEvents: Array<WorldEvent>
    
    // 월드 레벨 실행
    func RunWorld() {
        // 모든 시스템을 병렬로 실행
        parallel {
            combat.Update()
            economy.ProcessTransactions()
            social.UpdateRelationships()
            crafting.ProcessQueues()
        }
    }
    
    // 시스템 간 통신
    func OnPlayerTrade(player1: PlayerID, player2: PlayerID) {
        // 경제 시스템과 소셜 시스템이 협력
        economy.ProcessTrade(player1, player2)
        social.IncreaseReputation(player1, player2)
    }
}
```

## 🎯 실제 예제: MMORPG

### 이 계층에서 subject는 어디에 있나

- ability는 subject가 수행하는 계약이다
- role은 subject에 ability를 붙인다
- party는 role slot에 subject를 배치한다
- relation/effect/zone/world는 그 협력 위에 더 느슨한 규칙과 경계를 덧씌운다

즉 최종 목표의 `world -> zone -> effect -> relation -> party -> role -> ability -> subject` 계층은
사실상 subject를 중심으로 돌아간다.
반대로 `struct`는 이 계층의 leaf data로 쓰인다.

### Ability 레벨
```pergyra
ability Attackable {
    require _attackPower: Int
    func Attack(target: &mut impl Damageable) -> Int
}

ability Tradeable {
    require _inventory: Slot<Inventory>
    func Trade(other: &mut impl Tradeable, items: Array<Item>)
}

ability Craftable {
    require _craftingSkill: Int
    // 장기적으로는 Option/Result 계열 반환을 원하지만,
    // 현재 stable current surface로는 tagged union enum 또는 Result<T> 쪽이 더 가깝다.
    func Craft(recipe: Recipe) -> Result<Item>
}
```

### Role 레벨
```pergyra
role PlayerCharacter for Player {
    impl ability Damageable { ... }
    impl ability Attackable { ... }
    impl ability Tradeable { ... }
    impl ability Craftable { ... }
    
    parallel on (playerThread) {
        continuous { ProcessInput() }
    }
}

role ShopKeeper for NPC {
    impl ability Tradeable { ... }
    
    parallel on (aiThread) {
        every (5000ms) { UpdatePrices() }
    }
}
```

### Party 레벨
```pergyra
party TradingPost {
    role slot merchant: Tradeable & Conversable
    role slot customers: Array<Tradeable>
    
    shared marketPrices: PriceTable
    
    func ExecuteTrade(customer: Int, items: Array<Item>) {
        let buyer = context.GetRole<Tradeable>($"customers[{customer}]")
        let seller = context.GetRole<Tradeable>("merchant")
        
        seller.Trade(buyer, items)
    }
}

party CraftingGuild {
    role slot master: Craftable & Teaching
    role slot apprentices: Array<Craftable & Learning>
    
    shared recipes: RecipeBook
    shared materials: MaterialStorage
}
```

### Systemic 레벨
```pergyra
systemic EconomySystem {
    // 거래소들
    party slot mainTradingPost: TradingPost
    party slot auctionHouse: AuctionHouse
    
    // 생산 시설들
    party slot craftingGuilds: Array<CraftingGuild>
    party slot farmingCoops: Array<FarmingCoop>
    
    // 시스템 전체 경제 상태
    shared globalEconomy: EconomyState
    shared inflationRate: Float
    
    // 경제 시뮬레이션
    func SimulateEconomy() {
        parallel {
            // 모든 거래소 운영
            mainTradingPost.ProcessTrades()
            auctionHouse.ProcessBids()
            
            // 생산 활동
            parallel (craftingGuilds) join with all
            parallel (farmingCoops) join with all
        }
        
        // 경제 지표 업데이트
        UpdateInflation()
        BalanceSupplyDemand()
    }
}

systemic CombatSystem {
    party slot pveBattles: Array<DungeonTeam>
    party slot pvpArenas: Array<PvPMatch>
    party slot worldBosses: Array<RaidEncounter>
    
    shared combatStats: CombatStatistics
    shared balanceConfig: BalanceConfiguration
}
```

### World 레벨
```pergyra
world MMORPGWorld {
    // 모든 주요 시스템
    systemic economy: EconomySystem
    systemic combat: CombatSystem
    systemic social: SocialSystem
    systemic quests: QuestSystem
    systemic housing: HousingSystem
    
    // 월드 상태
    shared worldClock: GameTime
    shared seasonalEvents: EventCalendar
    shared serverPopulation: Int
    
    // 메인 게임 루프
    func MainGameLoop() {
        loop {
            let frameStart = Time.Now()
            
            // 모든 시스템 병렬 업데이트
            parallel {
                economy.SimulateEconomy()
                combat.ProcessAllBattles()
                social.UpdateRelationships()
                quests.ProgressQuests()
                housing.UpdateHouses()
            }
            
            // 월드 이벤트 처리
            ProcessWorldEvents()
            
            // 프레임 동기화
            let frameTime = Time.Now() - frameStart
            if frameTime < TARGET_FRAME_TIME {
                Sleep(TARGET_FRAME_TIME - frameTime)
            }
        }
    }
}
```

## 정리

- `struct`는 값이다
- `subject`는 상태와 identity를 가진 주체다
- 현재 `subject`와 `class`는 같은 subject surface다
- `role`은 subject와 ability를 묶는다
- `party`는 subject들의 협력 단위다
- `object`는 subject가 수동 문맥으로 해석된 모습이다
- `systemic/world`는 그 협력을 더 큰 단위로 조율한다

## 🚀 장점

### 1. **명확한 책임 분리**
- Ability: "무엇을 할 수 있는가?"
- Role: "어떻게 하는가?"
- Party: "누가 함께 하는가?"
- Systemic: "어떤 시스템인가?"
- World: "전체가 어떻게 동작하는가?"

### 2. **확장성**
- 새로운 ability 추가 → 기존 시스템 영향 없음
- 새로운 party 추가 → systemic에 슬롯만 추가
- 새로운 systemic 추가 → world에 등록만

### 3. **병렬성의 계층적 관리**
```pergyra
// World 레벨: 시스템들을 병렬로
parallel { economy, combat, social }

// Systemic 레벨: 파티들을 병렬로
parallel (allDungeonTeams)

// Party 레벨: 역할들을 병렬로
parallel (dungeonTeam)

// Role 레벨: 개별 태스크 병렬 실행
parallel on (gpuThread) { ... }
```

### 4. **테스트와 디버깅**
- 각 레벨을 독립적으로 테스트 가능
- 문제 발생 시 정확한 계층 파악 가능
- 성능 프로파일링도 계층별로 가능

## 💡 구현 로드맵

### Phase 1: Systemic 구현
- [ ] AST에 `AST_SYSTEMIC_DECL` 추가
- [ ] `systemic` 키워드 파싱
- [ ] Party 슬롯 관리 시스템

### Phase 2: World 구현
- [ ] AST에 `AST_WORLD_DECL` 추가
- [ ] `world` 키워드 파싱
- [ ] Systemic 조합 관리

### Phase 3: 런타임 통합
- [ ] 계층적 FiberMap 생성
- [ ] World-level 스케줄러
- [ ] Cross-systemic 통신

### Phase 4: 최적화
- [ ] Static systemic 분석
- [ ] World-wide 병렬 최적화
- [ ] 메모리 레이아웃 최적화

## 🎨 비전

이 5단계 계층 구조는 Pergyra를 단순한 프로그래밍 언어를 넘어, **복잡한 시스템을 자연스럽게 표현하는 도구**로 만듭니다.

```
작은 것부터 시작해서 (Ability)
점점 더 큰 그림으로 (World)
```

이는 인간이 복잡한 시스템을 이해하는 자연스러운 방식과 일치합니다.

**"In Pergyra, we don't just code. We architect worlds."** 🌍
