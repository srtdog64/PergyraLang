# Pergyra GOF Pattern Catalog

## 목적

이 문서는 GOF 패턴을 그대로 복제하지 않고, **Pergyra 존재론에 맞게 번역**하는 기준을 정리한다.

핵심 원칙은 다음과 같다.

- 패턴은 코어 문법이 아니라 `use` 계층의 **pattern library**로 간다
- inheritance / `super` / hidden callback graph에 의존하는 형태는 피한다
- `subject / vessel / object / tobject / relation / effect / zone / world / Slot<T>` 위에 다시 해석한다
- 도메인 코드는 pattern library를 소비하고, 사용 시점에 도메인을 주입한다

## 번역 원칙

### Singleton

전통 GOF:

- process-global instance
- static accessor

Pergyra식:

- **host-local contextual singleton**
- 보통 `world`, `zone`, `relation`, `effect`, `party`가 가진 `shared` 상태나
  factory-opened runtime host로 표현
- “프로그램 전체에 하나”보다 “이 문맥에 하나”가 더 중요

예:

```pergyra
world RuntimeRegistry
{
    shared nextId: Int = 1000
    shared prefix: String = "QUEST"
}
```

즉 `shared`는 public field가 아니라, host가 유지하는 contextual singleton state다.

### Factory / Abstract Factory

전통 GOF:

- subclass override로 생성 정책 분리

Pergyra식:

- **template/spec builder**
- `func TemplateFactory(...) -> Template`
- `func BeginDraft(...) -> Draft`
- `func ApplyTemplate(draft, template) -> Draft`
- `func FinalizeSpec(draft) -> SubjectSpec`
- 최종 host 생성은 spec를 통해 이뤄짐

예:

```pergyra
let draft0 = BeginUnitDraft("Kesh", rank)
let draft1 = ApplyRoleTemplate(draft0, RoleTemplateFactory(classCode))
let draft2 = ApplyOriginTemplate(draft1, OriginTemplateFactory(originCode))
let spec = FinalizeUnitSpec(draft2)
let unit = Adventurer(spec.name, spec.title, ...)
```

즉 상속 트리보다 **spec 조립 경로**가 중심이고,
실전에서는 `template -> draft -> finalize` 같은 staged builder가 기본이 된다.

### Strategy

전통 GOF:

- strategy interface + concrete strategy class

Pergyra식:

- **card / table / resolver**
- `StrategyCard`
- `StrategyTable<TContext, TChoice>`
- `StrategyResolve(card, context)`
- 장기적으로 `Picker<TInput, TChoice>` / `Resolver<TContext, TResult>` 주입
- 현재 안정 경로에서는 `StrategyContext` / `ApplyStrategy(card, context)` 계층을 먼저 갖는다

예:

```pergyra
let card = StrategyFactory(stateId, option)
let outcome = ResolveStrategy(card, threat)
```

혹은 더 강하게, 다음 단계 목표로:

```pergyra
func AggressivePolicy(pulse: Int) -> Int { ... }
let outcome = DynamicStrategyResolve(stateId, threat, pulse, AggressivePolicy)
```

즉 “객체 다형성”보다 **선택 가능한 policy data + resolver** 쪽이 기본값이다.
그리고 더 고도화되면 **고차함수형 policy injection**까지 포함한다.

### State

전통 GOF:

- state object 교체

Pergyra식:

- **explicit FSM / transition table**
- `StateMachine<TState, TEvent>`
- `TransitionRule`
- `TransitionContext`
- `ApplyTransition(rule, context) -> next`
- `world/zone`는 상태와 상태 파생 규칙을 직접 가진다

예:

```pergyra
let rule = TransitionRuleFactory(stateId, eventId)
let next = ApplyTransitionRule(rule, context)
```

즉 hidden object graph보다 **명시적 state id, transition rule, context application**이 기본이다.

### Observer

전통 GOF:

- subject keeps observer list
- hidden callback propagation

Pergyra식:

- **explicit relay / bus / transcript sink**
- `RelaySinkSpec`
- `RelayPacket`
- `RelayBundle`
- `DispatchPacket(...)`
- 혹은 하나의 host가 여러 sink로 fan-out

예:

```pergyra
let sink = CombatSinkFactory()
let packet = PacketFactory("combat", "dragon phase changed", 2)
let line = DispatchPacket(sink, packet)
```

즉 Pergyra는 hidden callback chain보다 **명시적 relay bundle과 packet dispatch**를 선호한다.

## 추가 패턴의 번역

### Builder

- staged spec composer
- `QuestBuilder`, `ReportBuilder` 같은 이름은 가능
- 하지만 핵심은 chained object mutation보다 **spec 누적 + finalize**다

### Adapter

- foreign/runtime/FFI wrapper
- `extern` 계층이나 compatibility helper로 표현

### Decorator

- inheritance-based wrapper보다
- `vessel` 조합, wrapper `subject`, projection layer로 번역

### Command

- callable object보다
- `action ledger`, `queued intent`, `event packet` 쪽이 더 자연스럽다

## 라이브러리화 우선순위

우선순위가 높은 패턴:

- Singleton -> contextual singleton helper / runtime registry
- Factory -> spec/template factory
- Strategy -> policy card / policy table
- Strategy -> policy func / dynamic resolver injection
- State -> FSM / transition table
- Observer -> relay / report sink / event bus

나중 우선순위:

- Builder
- Adapter
- Command
- Decorator

이유는 앞의 다섯 개가 이미 `campaign`, `DND`, `FSM`, `graph`, `report` 예제에서 실제로 반복 등장했기 때문이다.

## 예제 기준

`examples/pattern_library_basics/`는 다음을 보여주는 기준 예제다.

- singleton = contextual runtime registry
- factory = template/spec builder
- strategy = card + resolver + injected policy func
- state = explicit transition
- observer = explicit relay sink

즉 이 예제는 “GOF를 Pergyra식으로 번역하면 어떤 모양이 되는가”를 보여준다.

## 결론

한 줄로 정리하면:

**Pergyra는 GOF를 inheritance/object hierarchy로 재현하지 않고, contextual state + spec factory + policy table + explicit relay로 다시 쓴다.**
