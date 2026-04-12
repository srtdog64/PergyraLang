# Zone Context and Transfer Derivation Plan

마지막 업데이트: 2026-04-11

이 문서는 Pergyra의 authoring pain point를 줄이기 위해
`zone/world/context derivation`와 `using/transfer` 자동 정렬 규칙을 고정한다.

핵심 목표:

- 개념을 줄이지 않는다.
- 같은 zone/world 문맥을 반복 서술하지 않게 한다.
- `using:` / `transfer:`의 rigid surface를 그대로 두되,
  흔한 경우는 유도로 압축한다.
- diagnostics는 반드시 "무엇이 유도됐고 무엇이 충돌했는가"를 보여준다.

## 1. lexical zone context

### 1.1 문제

지금은 같은 zone 안에 오래 머무는 코드에서도
`within ZoneName`을 계속 반복하게 된다.

이건 특히:

- zone-heavy action 모음
- 같은 zone에 속한 workflow 파일
- simulator/game domain

에서 작성 피로가 크다.

### 1.2 목표 표면

두 표면 중 하나만 채택한다.

#### A. file/block zone context

```pgy
zone context CalendarZone;

action AddEvent(self, event: Event) {
}

action UpdateEvent(self, event: Event) {
}
```

또는:

```pgy
within CalendarZone {
    action AddEvent(self, event: Event) { }
    action UpdateEvent(self, event: Event) { }
}
```

#### B. domain-local lexical block only

```pgy
within CalendarZone {
    action AddEvent(self, event: Event) { }
    action UpdateEvent(self, event: Event) { }
}
```

현재 구현/추천:

- `within ZoneName { ... }` top-level block부터 시작
- nested lexical zone context는 현재 금지
- file-global `zone context`는 그 다음 단계

이유:

- block은 스코프가 눈에 보인다
- nested conflict를 더 쉽게 제한할 수 있다
- parser/semantic 진입이 더 단순하다

### 1.3 규칙

1. 현재 구현은 top-level `within Zone { ... }` block 내부 선언에 기본 zone을 제공한다
2. body 안의 explicit `within X`가 있으면 explicit가 우선한다
3. nested lexical zone context는 현재 금지한다
4. zone context는 zone type만 받는다
5. world context는 별도 축으로 분리한다. zone context와 혼합하지 않는다

### 1.4 diagnostics

오류는 반드시 아래를 보여준다.

- 현재 lexical zone
- explicit override 여부
- action/step이 실제로 요구한 zone

예:

```text
Action 'Pay' cannot run in lexical zone 'CartZone'.

Reason:
- current block provides zone 'CartZone'
- action requires zone 'PaymentZone'

Fix:
- move this declaration into 'within PaymentZone { ... }'
- or add an explicit 'within PaymentZone'
```

## 2. using / transfer derivation

### 2.1 문제

현재 `using:` 과 `transfer:` 는 의미는 맞지만 rigid하다.

흔한 패턴:

```pgy
step Pay {
    where: PaymentZone;
    using: payment;
    transfer: cart -> payment;
    who: buyer;
    authorized by: buyer;
}
```

여기서 작성자가 반복 서술로 느끼는 것은:

- `where`와 `transfer target`이 사실상 같은 zone을 가리킴
- `using`도 대부분 transfer target과 동일

### 2.2 기본 유도 규칙

#### Rule A. transfer target -> using

조건:

- step에 `transfer: source -> target;`가 있음
- `using:` 이 없음

동작:

- `using: target;`을 자동 유도

#### Rule B. transfer target -> where

조건:

- step에 `transfer: source -> target;`가 있음
- explicit `where:`가 없음
- target alias의 zone type이 명확함

동작:

- `where: <target-zone-type>;`를 자동 유도

#### Rule C. explicit wins

조건:

- `where:` 또는 `using:`이 명시돼 있음

동작:

- explicit가 우선
- 유도값과 충돌하면 domain-first diagnostic

#### Rule D. no multi-hop inference

금지:

- `source -> target`만으로 authority, requires, causes까지 자동 유도

이유:

- 그 수준까지 가면 규칙이 숨는다
- P0에서는 zone alignment까지만 유도한다

### 2.3 diagnostics

예:

```text
Intent step 'Deliver' has inconsistent transfer context.

Reason:
- transfer target 'delivery' implies zone 'DeliveryZone'
- current using binding is 'loading'
- current explicit where zone is 'LoadingZone'

Fix:
- change 'using' to 'delivery'
- or change the transfer target to 'loading'
- or override the step zone to 'DeliveryZone'
```

### 2.4 현재 구현 우선순위

P0:

1. `transfer target -> using` 유도
2. `transfer target -> where` 유도
3. mismatch diagnostics를 `Reason / Fix` 형식으로 통일

P1:

1. intent-level default zone과 lexical zone context 연결
2. world-local context derivation 분리 설계

P2:

1. more advanced routing presets
2. profile/preset과 derivation의 결합

## 3. boundary

이 문서에서 `derivation`은 아래 범위만 뜻한다.

- 이미 선언된 zone/world/action/authority 계약에서
- 빠진 기본값을 채우는 것

이 문서는 아래는 아직 포함하지 않는다.

- implicit authority derivation
- implicit effect derivation
- implicit ability satisfaction derivation
- compact domain block

## 4. 결정

현재 방향:

1. `lexical zone context`는 block surface부터 시작하고, top-level block 1차 구현이 들어갔다
2. `using/transfer`는 target-alignment derivation만 먼저 넣는다
3. explicit clause는 항상 derivation보다 우선한다
4. diagnostics는 "유도된 값"과 "명시된 값"의 충돌을 직접 보여준다
