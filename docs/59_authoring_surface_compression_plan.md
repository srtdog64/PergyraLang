# Authoring Surface Compression Plan

마지막 업데이트: 2026-04-12

이 문서는 Pergyra의 강한 의미론을 줄이지 않고, 작성 경로를 압축해
authoring pain point를 줄이기 위한 표면 설계 방향을 고정한다.

핵심 원칙:

- 개념을 지우는 것이 아니라 반복 기술을 줄인다.
- 규칙은 유지하되, 자주 반복되는 선언은 상속/파생/preset으로 압축한다.
- diagnostics는 문법 부속물이 아니라 제품 기능으로 취급한다.

## 1. 가장 큰 pain point

### 1.1 선언 과잉

작은 문제를 풀 때도 `subject`, `zone`, `world`, `intent`, `effect`, `relation`
전체를 동시에 의식하게 만들면 작성 피로가 급격히 오른다.

### 1.2 경계 중복 기술

`where`, `who`, `authorized by`, `requires`, `within`, `causes`,
`using`, `transfer`가 이미 zone/action/ability 선언에 있는데
step에서 다시 반복되면 작성자가 중복을 강하게 느낀다.

### 1.3 projection / sync / transfer의 정신적 비용

projection과 transfer는 언어의 강점이지만, 지금 표면은 wiring-heavy하다.
강한 모델을 유지하되 흔한 패턴은 더 짧아져야 한다.

### 1.4 권장 surface의 흔들림

`subject` vs `class`, `stable surface` vs `design target`이 문서마다 섞이면
사용자는 현재 무엇이 권장되는지 헷갈리게 된다.

### 1.5 surface trust 부채

반복 선언 자체만큼 아픈 축이다.

사용자는 다음 셋을 즉시 구분할 수 있어야 한다.

- 지금 바로 믿고 써도 되는 stable surface
- smoke-covered subset이지만 범위가 제한된 surface
- design sketch / aspirational demo

이 구분이 흐려지면 문법 pain point보다 더 큰 trust pain point가 생긴다.

대표 사례:

- `HashMap<Int, V>`는 이제 정렬됐지만, 이런 종류의 mismatch는 한 번만 나와도 체감 신뢰도를 크게 깎는다
- `party_system_demo`, `world_roster_city` 같은 예제는 현재도 design sketch인데 stable syntax reference처럼 읽히기 쉽다
- compile-smoke covered example과 sketch example을 문서/헤더에서 같은 톤으로 다루면 사용자가 잘못 배운다

### 1.6 `own/ref`의 과잉 일반화

`own` / `ref`는 이름만 보면 전면 ownership system처럼 읽힌다.

하지만 현재 실제로 닫힌 구현은 더 좁다.

- `ref Slot<subject-host>`
- `own SecureSlot<subject-host>`

즉 현재 단계에서 필요한 것은 "더 큰 ownership vocabulary"보다
"이 vocabulary가 어디까지 닫혀 있는지 더 정확히 보여 주는 것"이다.

### 1.5 실제 구현에서 가장 자주 반복되는 cluster

현재 코드/예제 기준으로는 아래 다섯 묶음이 가장 손이 아프다.

1. action clause cluster
   - `requires / within / authorized by / causes`
2. intent step boundary cluster
   - `who / where / using / transfer / requires / authorized by / causes`
3. authority / ability cluster
   - `authority ... requires ...`
   - `action ... requires ...`
   - `step requires: ...`
4. projection / domain wiring cluster
   - `refresh / publish / bind / HasProjection / HasLayer / HasState / HasZone*`
5. built-in capability strictness cluster
   - named binding
   - exact slot kind
   - token pairing
6. trust-signaling cluster
   - compile-smoke covered example vs design sketch
   - stable surface vs future surface
   - currently closed subset vs long-term model

즉 지금 가장 시급한 것은 "새 개념 추가"가 아니라,
"이미 있는 개념 묶음의 반복 서술"을 압축하는 것이다.

## 2. 제안된 surface 압축 방향

### 2.1 intent profile

반복되는 intent clause를 단순 preset으로 묶는다.

```pgy
intent profile OwnerWriteIntent {
    exclusive;
    authorized by owner;
    within CalendarZone;
}

intent DeleteEvent uses OwnerWriteIntent {
    who owner: CalendarOwner;
    step delete;
}
```

원칙:

- profile은 preset이어야 한다.
- 상속/다중 병합처럼 복잡해지면 안 된다.

### 2.2 action 계약의 step 기본 상속

`action`에 이미 `requires`, `within`, `authorized by`가 있으면
`intent step`은 기본적으로 그 계약을 상속해 채우고, 필요할 때만 override한다.

현재 고정한 최소 surface는 `matching action contract pack`이다.

- matching subject action이 있으면 `step`은 아래 계약을 한 묶음으로 상속할 수 있다
- 현재 pack에 들어가는 항목:
  - `who`
  - `where/within`
  - `requires`
  - `authorized by`
  - `causes`
- 현재 pack에 **들어가지 않는** 항목:
  - `with effects`
- 이유:
  - `with effects`는 declaration-local effect contract다
  - step orchestration contract와는 결이 다르고, 현재 구현도 step-level effect clause를 따로 가지지 않는다
  - 따라서 `with effects`까지 자동 상속시키면 contract source가 흐려진다
- diagnostics와 AST print는 이 계약이 어디서 상속됐는지 직접 드러내야 한다

즉 아래 압축 표면은:

```pgy
step Guard {
    using: battle;
    on: hero.Guard();
    expect: true;
}
```

실제로는 아래 장문 표면과 같은 계약을 담을 수 있다.

```pgy
step Guard {
    who: hero;
    where: BattleZone;
    using: battle;
    requires: Prepared;
    authorized by: hero;
    causes: Guarded;
    on: hero.Guard();
    expect: true;
}
```

```pgy
action DeleteEvent(self, event_id: Int)
    requires CalendarOwner
    within CalendarZone
    authorized by owner
{
}

intent ManageEvent {
    who owner: CalendarOwner;
    step DeleteEvent(event_id);
}
```

필수 조건:

- diagnostics는 반드시 "이 값은 matching action contract에서 상속됨"을 보여줘야 한다.
- `transfer target -> using/where`처럼 자동 파생된 값도
  diagnostics에서 파생 출처를 직접 보여줘야 한다.

현재 정책은 아래처럼 고정한다.

1. `matching action contract pack`
- `who / where / requires / authorized by / causes`

2. `transfer derivation pack`
- `where / using`

3. declaration-local only
- `with effects`

용어:

- `action -> step`은 nominal/object hierarchy 상속이 아니라 contract inheritance다.
- `transfer target -> where/using`은 이미 주어진 target에서 값을 채우는 derivation이다.

### 2.3 lexical default zone

zone-heavy 코드는 파일/블록 단위 기본 zone 문맥을 허용한다.

```pgy
zone context CalendarZone;

action AddEvent(self, event: Event) {
}
```

또는:

```pgy
within CalendarZone {
    action AddEvent(self, event: Event) { }
    action UpdateEvent(self, event: Event) { }
}
```

원칙:

- nested zone context는 금지하거나 1단계로 제한한다.

상세 설계:

- [60_zone_context_and_transfer_derivation.md](/mnt/e/PergyraLang/docs/60_zone_context_and_transfer_derivation.md)

현재 상태:

- top-level `within Zone { ... }` block 1차 구현 완료
- nested lexical zone context는 아직 금지
- file-global `zone context`는 아직 설계 단계
- 현재 구현된 것은
  - `transfer target -> using/where derivation`
  - `within Zone { ... }` lexical zone context 1차

### 2.4 relation/effect alias 또는 implicit member resolution

relation/effect slot은 내부 맥락에서 계속 `self.`를 요구하면 noise가 커진다.

두 방향 중 하나를 선택한다.

1. implicit member resolution

```pgy
route.MarkReady();
seal.Validate();
```

2. explicit alias

```pgy
using self.route as route;
using self.seal as seal;
```

원칙:

- 로컬 변수와 멤버가 충돌할 때 규칙은 단순해야 한다.

### 2.5 transfer short surface

강한 이동 의미론은 유지하되, 흔한 이동 케이스는 짧은 표면을 제공한다.

```pgy
move loading to delivery;
```

원칙:

- 축약 표면은 가장 보수적인 의미만 가져야 한다.
- 고급 옵션은 기존 선언형 `transfer:`가 담당한다.

상세 설계:

- [60_zone_context_and_transfer_derivation.md](/mnt/e/PergyraLang/docs/60_zone_context_and_transfer_derivation.md)

현재 상태:

- 1차 구현 완료
- `using self.route as route;` 형태를 statement-start alias surface로 지원
- `move <from-alias> to <to-alias>;`를 `transfer: <from-alias> -> <to-alias>;`로 낮춘다
- `transfer target -> using/where derivation`도 이미 구현됐다
- type-directed `move <value> to <ZoneType>;`는 아직 설계 단계다

### 2.6 group bind / projection scaffold

복수 projection을 반복 wiring하지 않도록 group bind를 제공한다.

```pgy
bind group CalendarView {
    title <- event.title;
    date <- event.date;
    owner <- owner.name;
}
```

또는:

```pgy
bind CalendarView from event, owner;
```

원칙:

- explicit bind와 auto bind는 분리한다.
- 값 출처가 모호해지면 안 된다.

### 2.7 subject factory surface

subject 생성 경로를 표준화한다.

```pgy
subject Courier {
    factory Create(name: String, level: Int) within LoadingZone;
}

let courier = Courier.Create("Min", 3);
```

원칙:

- subject와 일반 값 타입을 구분하는 surface여야 한다.

### 2.8 scaffold / CLI template

언어 표면과 함께 작성 scaffold를 제공한다.

예:

```sh
pgy new intent ManageEvent --who owner:CalendarOwner --within CalendarZone --exclusive
```

생성 예:

```pgy
intent ManageEvent {
    who owner: CalendarOwner;
    exclusive;
    within CalendarZone;

    pre { }
    guard { }
    step { }
    post { }
    compensate { }
}
```

원칙:

- scaffold는 `minimal`과 `full` 두 종류 정도로 제한한다.

### 2.9 compact domain block

작은 예제/프로토타입에서는 물리적 파일 분산을 줄이는 compact block을 허용한다.

```pgy
domain CalendarDomain {
    ability CalendarOwner;
    effect EventCreated;
    relation OwnsEvent(owner, event);
    zone CalendarZone;
}
```

원칙:

- 소형 모듈 전용으로 제한한다.
- 장기 대형 프로젝트의 기본 구조로 밀지 않는다.

### 2.10 domain-first diagnostics

오류 메시지를 문법 중심이 아니라 domain rule 중심으로 재설계한다.

예:

```text
Intent step 'DeliverCargo' cannot run in 'LoadingZone'.

Reason:
- action requires zone 'DeliveryZone'
- current step inherited zone 'LoadingZone'

Fix:
- move step to DeliveryZone
- or override step zone explicitly
```

또는:

```text
Cannot return subject 'Courier' by value.

Reason:
- subject values are zone/world anchored handles

Fix:
- construct in zone builder
- or return projection/object instead
```

원칙:

- `why failed`
- `which boundary`
- `which authority`
- `which slot`
- `which projection`

이 다섯 축이 항상 보여야 한다.

현재 진척:

- `subject` by-value return은 `Reason` / `Fix` 형식으로 시작했다.
- authority-bearing `intent step`의 missing `authorized by`는 inherited action zone 여부를 같이 보여준다.
- transfer target / `using` mismatch도 같은 형식으로 올리는 중이다.

### 2.11 trust-signaling compression

이건 새 문법 추가가 아니라 사용자의 판단 비용을 줄이는 압축이다.

원칙:

- 모든 예제는 `compile-smoke covered` 또는 `design sketch`를 명시한다
- README와 핵심 모델 문서는 "현재 닫힌 subset"을 먼저 말한다
- long-term surface는 현재 surface와 같은 톤으로 서술하지 않는다

실천 항목:

- sketch example header 표준화
- stable example 목록 고정
- `own/ref`처럼 이름이 큰 축은 "현재 닫힌 구현 범위"를 먼저 설명
- token split / nominal family도 "현재 stable interpretation"을 문서 첫머리에 명시

## 3. 우선순위

### P0

- intent 계약 상속/파생
- relation/effect 접근 완화
- transfer 축약 표면
- domain-first diagnostics 개선
- action/authority contract 중복 축약
- surface trust 정리
  - stable example vs sketch example 라벨 고정
  - README / 핵심 모델 문서의 "현재 닫힌 subset" 우선 설명
  - `own/ref` 기대 범위 축소 및 경계 명시

### P1

- group bind / projection 축약
- lexical zone context
- subject factory 표준화

### 현재 압축 우선순위 재고정

token split, declaration trust, stable-vs-sketch trust는 많이 닫혔다.
이제 surface compression의 우선순위는 다음 셋으로 재고정한다.

1. clause density 압축
- `where / with effects / requires / within / causes / authorized by`
  family의 동시 서술량을 줄이는 방향

2. contract duplication 제거
- action 선언에 있는 계약을 intent/zone 쪽이 자연스럽게 상속/파생하게 만들고,
  override가 있을 때만 더 쓰게 하는 방향

3. inherited/derived contract diagnostics 강화
- 상속/파생이 강할수록 "무엇이 어디서 상속되었고 파생되었는가"를
  오류 메시지와 tooling이 먼저 설명하게 만드는 방향

현재 기준으로는 새로운 키워드 추가보다
**이미 있는 계약 surface를 더 적게 쓰고 더 잘 설명하게 만드는 것**이 우선이다.
- scaffold/template 자동 생성
- nominal family trust-signaling 정리
  - `subject/class`
  - `object/tobject/struct`

### P2

- compact domain block
- intent profile/preset 정교화

## 4. 결정

현재 방향은 다음과 같다.

- 언어 철학은 유지한다.
- authoring pressure는 surface compression으로 줄인다.
- 반복 선언은 상속/파생과 preset으로 줄인다.
- diagnostics 품질을 제품 기능으로 끌어올린다.
- surface trust는 기능 completeness와 별도 축으로 계속 관리한다.

추가 결정:

- 명목 타입 계층 의미의 `상속`은 여기서 쓰지 않는다.
- `zone/world/action/authority`에서 step/body로 내려오는 계약 기본값은 `계약 상속`으로 부른다.
- transfer target이나 zone binding처럼 이미 주어진 정보에서 채워지는 값은 `파생`으로 부른다.

## P0 execution order reset

The remaining compression work should not branch out into new feature families. The current execution order is fixed below.

### P0.1 Contract provenance everywhere inheritance/derivation exists

Goal:
Whenever the compiler inherits or derives a step contract, the provenance must be visible in diagnostics, debug output, and docs.

Done means:
- step diagnostics mention matching-action inheritance where relevant
- step diagnostics expose the concrete matching action contract when one exists
- transfer diagnostics mention transfer-target derivation where relevant
- AST/debug output shows contract-source markers

### P0.2 Canonical short surface for the common contract path

Goal:
The common path should be: put the reusable contract on the action, keep the step orchestration-focused, spell overrides only when behavior diverges.

Done means:
- docs describe this as the preferred authoring path
- paired examples show long vs compressed forms with equivalent meaning
- hover text and examples use the same vocabulary

### P0.3 Clause-family boundary cleanup

Goal:
Make the boundary between reusable contract clauses and declaration-local clauses impossible to miss.

Done means:
- `who / where / requires / authorized by / causes` are treated as the matching action contract pack
- `where / using` are treated as the transfer derivation pack
- `with effects` is documented and surfaced as declaration-local only

### P0.4 Stable-reference example split

Goal:
Users need to know which examples are smoke-covered and which are reference-only.

Done means:
- stable examples remain the copy-first surface
- paired compression examples are listed as real surface but not yet smoke-covered when that is the current truth
- docs do not blur the distinction

### P0.5 Parser and diagnostic sharpness for dense signatures

Goal:
If the long form is used, failures should still be local and understandable.

Done means:
- dense clause ordering errors point to the actual clause-family problem
- negative parser/semantic tests cover the common misuse patterns
- the long form remains valid, but the short form remains the recommended route

## What is explicitly out of scope for this pass

The following are not part of the current compression pass unless they directly unblock one of the five items above:
- new ontology keywords
- profile/inheritance systems beyond today's simple derivation rules
- new transfer semantics
- new runtime subsystems

## Operating rule

If a proposed change increases authoring power but also increases the number of equally-valid ways to spell the same contract, reject it for this pass. The current goal is fewer repeated words and fewer competing surfaces, not more expressive branching.
