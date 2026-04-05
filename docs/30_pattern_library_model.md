# Pergyra Pattern Library Model

## 핵심 방향

Pergyra의 라이브러리는 단순 API 모음이 아니라 **패턴 라이브러리**를 지향한다.

즉 방향은:

- 도메인 특화 코드를 코어 언어나 stdlib에 직접 넣지 않는다
- 먼저 **도메인을 제거한 제네릭 패턴**을 만든다
- 실제 게임/앱에서는 **사용 시점에 도메인을 주입**한다

이 문서는 그 원칙을 `pgy` 기준으로 고정한다.

라이브러리 authoring도 동일하다. generic pattern을 만들기 전에 먼저 host ontology를 고른다.

- `subject`: 패턴의 능동 orchestrator
- `class`: 패턴이 소비하는 도구/정책/카드/설정 값
- `object`: 패턴의 수동 view/state target
- `tobject`: 패턴의 외부 전송 packet

즉 scaffold와 라이브러리 설계의 첫 단계는 "무슨 패턴인가"보다 "이 host가 `subject/class/object/tobject` 중 무엇인가"이다.

## Generic-to-Domain Injection

기본 아이디어는 다음과 같다.

```pergyra
use pool;
use fsm;
use encounter;
use strategy;
use tables;
```

라이브러리는 먼저 generic pattern을 제공한다.

```pergyra
let enemyPool = Pool<EnemySpec>.New(32);
let turnFlow = StateMachine<TurnState, TurnEvent>.New();
let aiPolicy = StrategyTable<BattleContext, ActionChoice>.New();
let lootTable = WeightedTable<LootDrop>.New();
```

이 generic pattern은 장기적으로 단순 data table에 머물지 않고
**function injection**까지 함께 가져가는 방향이 맞다.

```pergyra
func AggressivePolicy(pulse: Int) -> Int { ... }
// 목표 shape
let score = DynamicStrategyResolve(stateId, threat, pulse, AggressivePolicy);
```

그리고 실제 시나리오에서 domain을 주입한다.

```pergyra
let goblinPool = Pool<GoblinSpawn>.New(24);
let dragonFlow = StateMachine<DragonPhase, DragonEvent>.New();
let rogueAi = StrategyTable<RaidContext, SkillChoice>.New();
let relicLoot = WeightedTable<RelicDrop>.New();
```

즉:

- `Pool<T>`는 object/entity pool 패턴
- `StateMachine<TState, TEvent>`는 FSM 패턴
- `StrategyTable<TContext, TChoice>`는 AI/행동 전략 패턴
- `WeightedTable<T>`는 콘텐츠/확률 테이블 패턴
- 필요할 때는 `Resolver<TContext, TResult>`나 `Picker<TInput, TChoice>` 같은
  **고차함수형 주입 표면**도 패턴 라이브러리의 다음 단계로 들어가야 한다

그 직전 안정 단계는:

- `StrategyCard`
- `StrategyContext`
- `ApplyStrategy(card, context)`

같은 **context-to-outcome layer**다.

이고, `Goblin`, `Dragon`, `Relic`은 라이브러리가 아니라 사용자 도메인이다.

## 왜 이렇게 가는가

Pergyra 코어는 이미 다음 존재론을 갖고 있다.

- `subject`
- `vessel`
- `object`
- `tobject`
- `ability`
- `role`
- `relation`
- `effect`
- `zone`
- `world`
- `Slot<T>`

여기에 게임용 키워드까지 더 넣으면 언어가 빨리 굳는다.

특히 다음은 프로젝트마다 모양이 크게 다르다.

- entity/object pool
- encounter/turn flow
- AI strategy
- content/loot/event tables

그래서 이 계층은 코어 언어가 아니라 **패턴 라이브러리**로 두는 편이 맞다.

## 코어와 라이브러리의 경계

### 코어 언어가 담당하는 것

- 주체와 대상의 분리
- 상태와 투영의 분리
- 규칙 문맥(`relation/effect/zone/world`)
- 자원 점유권(`Slot<T>`)
- hosted `func` / `action`
- projection / binding / lifecycle

### 패턴 라이브러리가 담당하는 것

- object/entity pool
- state machine
- encounter loop
- turn resolver
- strategy/policy
- weighted tables
- event composition

즉:

```text
language core = 의미론
pattern library = 재사용 가능한 구조
domain code = 실제 게임/앱 내용
```

즉 Pergyra 패턴 라이브러리는:

- **data injection**
- **function injection**

둘 다 가져야 한다. 다만 현재 저장소 기준으로는 `data/card/table` 경로가 안정 경로이고,
`function injection`은 다음 단계 목표다.

## `use` 계층의 의미

`use`는 단순히 “stdlib 가져오기”가 아니라,
**generic pattern을 프로젝트에 채택한다**는 의미로 본다.

예:

```pergyra
use pool;
use fsm;
use strategy;
use tables;
```

이후 프로젝트는 이 패턴 위에 자신의 도메인을 주입한다.

```pergyra
let campState = StateMachine<CampState, CampEvent>.New();
let monsterPool = Pool<MonsterSpawn>.New(48);
let dragonPolicy = StrategyTable<DragonContext, DragonMove>.New();
```

## 파일 구성 원칙

Pergyra 패턴 라이브러리는 **패턴의 응집성 우선**으로 둔다.

즉 지나친 파일 분리보다, 하나의 패턴을 이해하고 가져가기 쉬운 구성을 우선한다.

권장:

- 하나의 패턴을 이루는 작은 helper / result / option / config는 한 파일에 둔다
- 독립 재사용 단위가 크거나, 여러 패턴이 공유하는 것은 분리한다

예:

```text
stdlib/
  pool/
    pool.pgy
  fsm/
    state_machine.pgy
  strategy/
    strategy_table.pgy
  tables/
    weighted_table.pgy
```

혹은 더 응집적으로:

```text
stdlib/game/
  encounter_state_machine.pgy
  strategy_policy_table.pgy
  pooled_spawn_set.pgy
```

핵심은 “패턴 전체를 이해하기 쉬운가”다.

## 네이밍 원칙

도메인 이름보다 기능 이름을 우선한다.

좋은 예:

- `Pool<T>`
- `StateMachine<TState, TEvent>`
- `StrategyTable<TContext, TChoice>`
- `WeightedTable<T>`
- `EventQueue<T>`
- `TurnResolver<TState, TAction>`

나쁜 예:

- `MonsterFactory`
- `DungeonBattlePool`
- `DragonDecisionCore`

이런 이름은 최종 게임 프로젝트에서는 괜찮지만,
패턴 라이브러리 자체의 이름으로는 너무 도메인에 묶인다.

## DND 시나리오와의 관계

`examples/dnd_tavern_campaign/`는 이 철학을 검증하는 대표 예제가 된다.

의도는 다음과 같다.

- 언어 코어가 게임을 만들 수 있을 만큼 충분한가
- pool/fsm/strategy/table을 코어가 아니라 library layer로 올려도 충분한가
- domain은 DND지만, 패턴은 generic하게 뽑을 수 있는가

즉 DND 예제는 “게임 구현”이면서 동시에 “패턴 라이브러리 검증장”이다.

`examples/pattern_library_basics/`는 그보다 더 작은 기준 예제다.

- `singleton` -> contextual runtime registry
- `factory` -> staged template/spec builder
- `strategy` -> card + resolver
- `dynamic strategy` -> policy func injection
- `state` -> transition rule + context application
- `observer` -> relay bundle + sink spec

즉 이 예제는 GOF의 “이름”을 그대로 두더라도,
Pergyra가 그것을 어떤 shape로 다시 쓰는지 보여준다.

## 결론

Pergyra의 라이브러리는 이렇게 정리한다.

```text
stdlib/use layer = pattern library
pattern library = generic first
project code = domain injection
```

한 줄로 쓰면:

**Pergyra 라이브러리는 도메인 라이브러리가 아니라, 도메인을 나중에 주입하는 제네릭 패턴 라이브러리다.**
