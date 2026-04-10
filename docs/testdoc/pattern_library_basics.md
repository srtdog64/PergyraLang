# Testdoc: pattern_library_basics

## 목적

`examples/pattern_library_basics/`는 GOF 기초 패턴을 Pergyra식으로 어떻게 번역하는지 보여주는 최소 기준 예제다.

이 예제는 inheritance-heavy OOP를 재현하려는 것이 아니라, 다음 원칙을 검증한다.

- singleton -> contextual runtime registry
- factory -> staged template/spec builder
- strategy -> card + resolver + injected policy func
- state -> explicit transition table
- observer -> relay bundle + sink spec

## 파일 구성

- `common.pgy` — 예제 헤더
- `singleton.pgy` — `world RuntimeRegistry`
- `factory.pgy` — `RoleTemplate` / `OriginTemplate` / `UnitDraft` / `UnitSpec` staged factory
- `strategy.pgy` — `StrategyCard` / resolver / injected policy surface
- `state_flow.pgy` — `TransitionRule` / `TransitionContext` / explicit transition table
- `observer.pgy` — `RelaySinkSpec` / `RelayPacket` / `RelayBundle` dispatch
- `main.pgy` — 전체 데모 실행
- `expected_stdout.txt` — exact stdout golden

## 왜 이렇게 생겼는가

### Singleton

전통 static-global singleton 대신 `world`가 가진 `shared` 상태로 번역했다.
핵심은 “프로세스 하나에 하나”보다 “문맥 하나에 하나”다.

### Factory

생성 다형성 대신 `RoleTemplateFactory(...)`, `OriginTemplateFactory(...)`, `BeginUnitDraft(...)`,
`ApplyRoleTemplate(...)`, `ApplyOriginTemplate(...)`, `FinalizeUnitSpec(...)`처럼
중간 조립 단계가 드러나는 spec builder 경로를 사용했다.

즉 factory는 단순 `UnitFactory(...) -> UnitSpec` 한 단계가 아니라
`template -> draft -> finalize` shape를 가진다.
이 경로가 있어야 이후 `loadout`, `ability`, `policy`, `content table` 주입점도 같은 패턴 위에 올릴 수 있다.

이번 작업에서 실제 언어 문제도 하나 드러나서 고쳤다.

- C backend가 `let title = a + "..." + b` 같은 string concat local을 `int32_t`로 잘못 추론하던 버그가 있었다
- 이건 pattern example 자체를 우회하지 않고 transpiler의 local type inference를 수정해 해결했다
- 회귀는 `test-transpile`에 추가했다

### Strategy

전략 객체보다 `StrategyCard`와 `ResolveStrategy(...)`로 policy를 data + resolver 조합으로 표현했다.
이번 턴부터는 stable path를 한 단계 더 올려
`StrategyContext` / `ApplyStrategyScore(card, context)` / `ApplyStrategyLine(card, context)` 위에
`func(...) -> ...` 타입 파라미터를 받는 injected score policy / line policy를 추가했다.
즉 strategy는 이제 단순 카드 조회가 아니라
`context -> applied score/result line -> injected policy override` 계층을 가진다.
그리고 같은 resolver를 `raid`, `dispatch` 같은 서로 다른 domain에 재사용하면서,
named function과 lambda 둘 다 policy injection으로 쓸 수 있음을 보여준다.

현재 예제는 여기까지를 V1 stable path로 본다.
다음 단계는 `picker` / `resolver`를 `use strategy;` 같은 실제 라이브러리 모듈로 승격하는 것이다.

이번 작업에서 드러난 sharp edge도 바로 고쳤다.

- `context`는 더 이상 전역 예약어처럼 막히지 않는다
- 일반 로컬 변수/파라미터 이름으로 사용할 수 있다

### State

state object 교체 대신 `TransitionRuleFactory(current, event)`와
`ApplyTransitionRule(rule, context)`를 사용해 상태 전이를 명시했다.

즉 state는 단순 `TransitionState(...) -> next`에서 한 단계 올라가
`rule table -> context -> applied next state` shape를 가진다.
이렇게 해야 이후 `encounter`, `campaign`, `boss phase machine`처럼
같은 FSM을 서로 다른 상황 문맥에 재주입하는 구조로 자연스럽게 이어질 수 있다.

### Observer

hidden callback list 대신 `RelaySinkSpec`, `RelayPacket`, `RelayBundle`,
`DispatchPacket(...)`을 사용해 fan-out을 드러냈다.

즉 observer는 단순 `PublishCombat(...)` 같은 하드코딩 sink 함수에서 한 단계 올라가
`sink spec -> packet -> explicit dispatch` shape를 가진다.
이 구조가 있어야 이후 `report`, `event bus`, `transcript`, `UI relay` 같은 표면도
같은 패턴 위에서 확장할 수 있다.

이번 작업에서도 실제 언어 문제를 하나 더 고쳤다.

- `String == "literal"`이 C backend에서 포인터 비교로 내려가 경고와 잘못된 의미론을 만들고 있었다
- 이제 C/LLVM 둘 다 `pgy_string_equals(...)` helper를 통해 lowering된다
- 회귀는 `test-transpile`에 추가했다

## 검증 상태

- `./bin/pgy examples/pattern_library_basics/main.pgy` 컴파일 통과
- generated binary 실행 통과
- `tests/example_contract_smoke.sh`의 exact stdout 대상으로 등록

## 다음 단계

이 예제는 catalog 기준점이다. 다음 단계는 여기서 바로 generic 라이브러리 표면으로 뽑는 것이다.

- `use fsm;`
- `use strategy;`
- `use tables;`
- `use campaign;`
- `use report;`
