# Pergyra World-Systemic Architecture

## 🌍 The Complete Hierarchy

```
        [WORLD]
           ↓
      [SYSTEMIC] ← 아키텍처 단위, 전체 시스템 조합
           ↓
        [PARTY] ← 실행 단위
           ↓
        [ROLE] ← 기능 단위
           ↓
      [ABILITY] ← 요구 조건 (데이터/슬롯)
```

## 📋 계층별 정의

### 1. **ABILITY** - 요구 조건
가장 기본적인 단위. 어떤 데이터와 동작이 필요한지 정의.

```pergyra
ability Damageable {
    require _healthSlot: Slot<Int>
    func TakeDamage(&mut self, amount: Int)
    func GetHealth(&self) -> Int
}
```

### 2. **ROLE** - 기능 단위
특정 struct가 ability를 어떻게 구현하는지 정의. 병렬 실행 로직 포함.

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
여러 role이 협력하는 실행 가능한 단위. 병렬 실행의 기본 단위.

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
    func Craft(recipe: Recipe) -> Option<Item>
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
