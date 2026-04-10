# Authoring Surface Compression Plan

마지막 업데이트: 2026-04-10

이 문서는 Pergyra의 강한 의미론을 줄이지 않고, 작성 경로를 압축해
authoring pain point를 줄이기 위한 표면 설계 방향을 고정한다.

핵심 원칙:

- 개념을 지우는 것이 아니라 반복 기술을 줄인다.
- 규칙은 유지하되, 자주 반복되는 선언은 추론/승계/preset으로 압축한다.
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

### 2.2 action 계약의 step 기본 추론

`action`에 이미 `requires`, `within`, `authorized by`가 있으면
`intent step`은 기본적으로 그 계약을 상속하고, 필요할 때만 override한다.

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

- diagnostics는 반드시 "이 값은 action 선언에서 상속됨"을 보여줘야 한다.

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
let delivered = cargo.transfer(to: DeliveryZone, as: DeliveredCargo);
```

또는:

```pgy
move cargo to DeliveryZone;
```

원칙:

- 축약 표면은 가장 보수적인 의미만 가져야 한다.
- 고급 옵션은 기존 선언형 `transfer:`가 담당한다.

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

## 3. 우선순위

### P0

- intent 계약 추론
- relation/effect 접근 완화
- transfer 축약 표면
- domain-first diagnostics 개선

### P1

- group bind / projection 축약
- lexical zone context
- subject factory 표준화
- scaffold/template 자동 생성

### P2

- compact domain block
- intent profile/preset 정교화

## 4. 결정

현재 방향은 다음과 같다.

- 언어 철학은 유지한다.
- authoring pressure는 surface compression으로 줄인다.
- 반복 선언은 추론과 preset으로 줄인다.
- diagnostics 품질을 제품 기능으로 끌어올린다.
