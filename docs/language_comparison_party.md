# Pergyra Party System: 언어 비교 분석

## 🎯 핵심 차별성: "객체에서 협력으로의 패러다임 전환"

Pergyra의 Party 시스템은 기존 언어들이 '객체'를 중심으로 사고하는 것과 달리, **'역할의 협력'**을 중심으로 시스템을 설계합니다.

## 📊 기존 언어와의 차별성 매핑

| 기준 | Rust/Java/C# | Pergyra (Party 기준) | 
|------|--------------|---------------------|
| **인터페이스 정의** | `trait` / `interface` | `role` (실행 + 조건 명세) |
| **구현 연결** | `impl` / `class implements` | `role slot` → 역할 조건 선언 |
| **구성 단위** | `object` / `class` | `party` = 의미 단위 협력 구조 |
| **내부 호출** | `object.method()` | `context.FindRole(Ability).action()` |
| **병렬 실행** | `async/await` / `task`, `thread` | `parallel (party)` 역할 단위 스케줄 |
| **검증** | 타입 시스템 + 수동 의존성 관리 | 타입 시스템 + 역할 조건 자동 검증 |
| **DI 및 메시지 전달** | 프레임워크 의존 (Spring, Guice 등) | 언어 내장 `context`, `role slot` 기반 연결 |

## 🏗️ 아키텍처 설계의 혁신

### 1. **4단계 추상화 계층**

```pergyra
// 1. struct: 순수 데이터
struct Warrior {
    _healthSlot: Slot<Int>
    _armor: Int
}

// 2. ability: 행위 명세
ability Damageable {
    require _healthSlot: Slot<Int>
    func TakeDamage(&mut self, amount: Int)
}

// 3. role: 구현 + 병렬 실행 정의
role WarriorTank for Warrior {
    impl ability Damageable { ... }
    
    parallel on (mainThread) {
        every (1000ms) { UpdateThreat() }
    }
}

// 4. party: 역할의 협력 단위
party DungeonTeam {
    role slot tank: Damageable & Taunting
    role slot healer: Healing
    role slot dps: DamageDealing
}
```

### 기존 언어의 한계

**Java/C#:**
```java
// 인터페이스와 구현이 분리되지만 협력은 수동으로 관리
interface Damageable 
{
    void takeDamage(int amount);
}

class Warrior implements Damageable, Taunting 
{
    // 병렬 실행? 별도의 ExecutorService 설정 필요
    // 다른 객체와의 협력? DI 컨테이너나 수동 주입
}
```

**Rust:**
```rust
// Trait은 강력하지만 협력 구조는 표현 못함
trait Damageable 
{
    fn take_damage(&mut self, amount: i32);
}

// 병렬 실행은 tokio 등 외부 런타임 필요
// Actor 패턴도 수동으로 구현해야 함
```

## 🚀 병렬 실행의 우아함

### Pergyra의 접근
```pergyra
// 한 줄로 모든 역할의 병렬 실행 오케스트레이션
let result = parallel (dungeonTeam) join with all
```

이 한 줄이 자동으로:
- ✅ Tank는 메인 스레드에서 위협 관리
- ✅ Healer는 백그라운드 스레드에서 치유 모니터링  
- ✅ DPS는 컴퓨트 스레드에서 최적 로테이션 계산
- ✅ 모든 결과를 수집하고 동기화

### 기존 언어의 복잡성

**Java (CompletableFuture):**
```java
CompletableFuture<Void> tankFuture = 
    CompletableFuture.runAsync(() -> tank.updateThreat(), mainExecutor);
    
CompletableFuture<Void> healerFuture = 
    CompletableFuture.runAsync(() -> healer.scanAndHeal(), backgroundExecutor);
    
CompletableFuture<Void> dpsFuture = 
    CompletableFuture.runAsync(() -> dps.optimizeRotation(), computeExecutor);

CompletableFuture.allOf(tankFuture, healerFuture, dpsFuture).join();
```

**Rust (Tokio):**
```rust
let tank_handle = tokio::spawn(async move {
    tank.update_threat().await
});

let healer_handle = tokio::spawn(async move {
    healer.scan_and_heal().await  
});

let dps_handle = tokio::spawn(async move {
    dps.optimize_rotation().await
});

let _ = tokio::join!(tank_handle, healer_handle, dps_handle);
```

## 🔗 역할 간 통신의 안전성

### Pergyra: Context 기반 안전한 통신
```pergyra
role PriestHealer for Priest {
    func HealLowestHP(&mut self) {
        // 컴파일 타임에 검증된 안전한 역할 접근
        let tank = context.GetRole<Damageable>("tank")
        let dps = context.GetRole<Damageable>("dps")
        
        // 가장 체력이 낮은 멤버 치유
        let lowest = [tank, dps].MinBy(|m| m.GetHealth())
        lowest.Heal(CalculateHealAmount())
    }
}
```

### 기존 언어: 수동 의존성 관리
```java
// Spring의 경우
@Component
class PriestHealer {
    @Autowired private Warrior tank;  // 수동 주입
    @Autowired private Mage dps;       // 타입 안전성 부족
    
    void healLowestHP() {
        // NullPointerException 위험
        // 순환 참조 가능성
    }
}
```

## 💎 시스템 설계의 표현력

### 복잡한 게임 시스템 예제

**Pergyra:**
```pergyra
// 명확한 역할 정의와 협력 구조
party BossRaid {
    // 메인 탱커와 서브 탱커
    role slot mainTank: Damageable & Taunting & Positioning
    role slot offTank: Damageable & Taunting
    
    // 힐러 조합
    role slot mainHealer: Healing & Cleansing
    role slot raidHealer: AreaHealing & Buffing
    
    // DPS 조합  
    role slot meleeDps: MeleeDamage & Interrupting
    role slot rangedDps: RangedDamage & Kiting
    role slot magicDps: MagicDamage & Debuffing
    
    // 공유 전략
    shared strategy: RaidStrategy
    shared phase: BossPhase = Phase1
    
    // 페이즈별 전략 실행
    func ExecutePhaseStrategy() {
        match phase {
            case Phase1:
                // Tank & Spank
                parallel {
                    mainTank.HoldAggro()
                    [meleeDps, rangedDps, magicDps].ForEach(|d| d.MaximizeDPS())
                }
                
            case Phase2:
                // 탱크 스왑 필요
                context.GetRole("offTank").Taunt()
                context.GetRole("mainTank").DropAggro()
                
            case Phase3:
                // 전원 생존 우선
                context.GetRole("raidHealer").CastRaidWideHeal()
        }
    }
}

// 실행은 단순하게
let raid = CreateBossRaid(...)
parallel (raid) join with all
```

**기존 언어로는?**
- 수십 개의 클래스와 인터페이스
- 복잡한 의존성 주입 설정
- 별도의 스레드 풀 관리
- 수동 동기화 코드
- 프레임워크 종속적인 설계

## 🎨 실제 응용 분야별 비교

### 1. **게임 개발**

| 측면 | 기존 언어 | Pergyra Party |
|-----|----------|--------------|
| AI 설계 | 상태 머신 + 수동 협력 | Role 기반 자율 협력 |
| 병렬 처리 | 복잡한 스레드 관리 | `parallel on` 자동 배치 |
| 팀 AI | 하드코딩된 협력 로직 | Context 기반 동적 협력 |

### 2. **마이크로서비스**

| 측면 | 기존 언어 | Pergyra Party |
|-----|----------|--------------|
| 서비스 조합 | API Gateway + 오케스트레이션 | Party로 서비스 그룹 정의 |
| 통신 | REST/gRPC + 수동 에러 처리 | Context 기반 타입 안전 통신 |
| 병렬 호출 | Promise.all() / CompletableFuture | `parallel (services)` |

### 3. **로봇/IoT**

| 측면 | 기존 언어 | Pergyra Party |
|-----|----------|--------------|
| 센서 융합 | 콜백 지옥 | Role별 독립 처리 |
| 액추에이터 제어 | 동기화 복잡성 | Party 수준 조율 |
| 실시간 처리 | RTOS 의존 | 스케줄러 태그로 명시 |

## 🔮 결론: 프로그래밍 패러다임의 진화

Pergyra의 Party 시스템은 단순히 새로운 문법이 아니라, **소프트웨어를 바라보는 관점의 전환**입니다:

### 기존 패러다임
```
Objects → Methods → Frameworks → Systems
(객체가 메서드를 가지고, 프레임워크가 연결하여 시스템 구성)
```

### Pergyra 패러다임
```
Data → Abilities → Roles → Parties → Collaborative Systems
(데이터가 능력을 통해 역할을 수행하고, 파티로 협력하여 시스템 구성)
```

이는 **인간이 실제로 협력 시스템을 설계하는 방식**과 일치하며, 복잡한 시스템을 더 직관적이고 안전하게 구현할 수 있게 합니다.

**"In Pergyra, we don't build objects. We compose collaborations."** 🚀
