# Intent 지향 오케스트레이션 초안 (2026-04-05)

이 문서는 intent 개념과 사용자-facing 의미를 설명한다. compensation, rollback, observability, authority/capability의 고정 계약은 [`37_compiler_contracts.md`](./37_compiler_contracts.md)를 기준으로 한다.

## 풀고자 하는 문제

> **구현이 이종 파편화되어도, 의도만 명확하면 구현할 수 있어야 한다.**

현실의 구현은 파편화된다. 5개 서비스, 3개 DB, 2개 외부 API, N개 zone, M개 world. 하지만 사용자의 의도는 하나다. "이걸 사고 싶다." "이 캐릭터를 살리고 싶다." "가입하고 싶다."

기존 접근의 한계:

OOP와 FP도 의도를 표현할 수 있다. OOP는 use-case layer, application service로, FP는 algebra, free monad, effect system으로 의도를 모델링한다. 하지만 공통적으로, **의도는 객체/함수/서비스 경계에 분산되며 언어 차원에서 1급 구조로 드러나지 않는다.**

| 접근 | 의도 표현 방식 | 한계 |
|------|--------------|------|
| OOP | use-case layer, domain service | 의도가 서비스/계층에 분산. 언어가 의도를 모른다 |
| FP | algebra, free monad, effect system | 의도가 타입/함수 합성에 인코딩. 선언적이지만 암시적 |
| MSA | saga, choreography | 보상 트랜잭션. "실패하면 되돌려"지, "왜 이걸 하는가"가 아님 |

Pergyra의 접근: **의도를 1급 구조로 선언한다.** 구현이 어떻게 파편화되든 — 다른 world, 다른 zone, 다른 subject — intent가 하나의 선언으로 전체를 관통한다. 파편은 intent의 step일 뿐이다. 이것은 OOP/FP가 패턴으로 하는 것을 **구조로 강제**하는 것이다.

## 한 줄 요약

> Intent는 세계(world) 위에서 경계를 가로지르는 사용자의 의지다.
> 선언은 언어가 이해하고, 실행은 별도 엔진이 맡는다.

### 상태

```
현재 단계: 언어 코어 v0.3
구현 형태: parser/semantic/HIR/codegen이 `intent` declaration을 직접 이해함
성숙도:    executable runtime lowering + basic conflict scheduler + minimal trace/history + rollback/compensation + live zone-instance binding 존재
```

---

## 1. 위치 — 패러다임이 아니라 orchestration 레이어

### 기존 패러다임의 "첫 질문"

| 패러다임 | 첫 질문 | 프로그램 구조 |
|----------|---------|-------------|
| 절차형 | "어떤 순서로 실행하는가?" | step 1 → step 2 → step 3 |
| 객체형 | "세계에 어떤 사물이 있는가?" | class Dog, class Car |
| 함수형 | "어떤 변환을 적용하는가?" | map, filter, reduce |
| 데이터형 | "데이터가 어디로 흐르는가?" | table → transform → table |
| 소유권형 | "누가 이 자원을 소유하는가?" | own, borrow, lifetime |

### intent가 묻는 질문

```
"사용자가 무엇을 하려 하는가?"
→ intent → step → zone → action
```

이것은 새로운 **보편 패러다임**이 아니다.
Pergyra 언어의 프리미티브(subject, zone, action, ability, effect) 위에 얹는
**cross-world intent orchestration 레이어**다.

### 왜 "절차적"이 아닌가

intent는 step이 있어서 절차적으로 **보이지만**, 본질이 다르다:

```
절차형:
  step 1: read file
  step 2: parse data
  step 3: write result
  → "실행 순서"가 핵심. 맥락 없음. 누가 왜 하는지 모름.

의도형:
  intent Purchase {
      step browse  { where: ProductView;  who: buyer; }
      step select  { where: CartSection;  who: buyer;  requires: Purchasable; }
      step pay     { where: PaymentZone;  who: buyer;  authorized by: buyer; }
  }
  → "사용자의 목적"이 핵심. 각 step에 맥락(누가, 어디서, 자격, 승인)이 있다.
  → 순서는 목적의 부산물이지, 목적 자체가 아니다.

여기서 `ProductView` 같은 이름은 "페이지"를 뜻하지 않는다.
Pergyra에서 page/route는 보통 projection surface이고,
`where`는 execution/authority boundary인 zone을 가리킨다.
```

절차형은 **"어떻게"**를 기술한다. intent는 **"왜"**를 기술하고, "어떻게"는 언어의 프리미티브(zone, action, ability)가 이행한다.

---

## 2. 계층 구조 — 현재 방향

```
intent (언어 선언 — 세계 위의 의지)
  │
  ├── world (언어 — 실행/신뢰/실패 경계)
  │     ├── roster (관리 시스템)
  │     │     └── party slot
  │     └── zone (행위 구역)
  │           ├── subject slot
  │           ├── relation slot
  │           └── effect slot
  │
  └── (다른 world도 가로지를 수 있음)
```

### 각 계층의 역할

| 계층 | 존재 위치 | 역할 | 비유 |
|------|----------|------|------|
| **intent** | 언어 1급 declaration | 경계를 관통하는 사용자의 의지 | 독자의 의지 / 플레이어의 목적 |
| **world** | 언어 | 실행/신뢰/실패 경계 | 소설 한 권 / 게임 서버 하나 |
| **roster** | 언어 | 관리 시스템 (party 운영) | 생태계 순환 시스템 |
| **zone** | 언어 | 행위가 허용되는 구역 | 장(章) / 무대 |
| **subject** | 언어 | 행동 주체 | 등장인물 |
| **class** | 언어 | 도구/사물 | 소도구 |
| **vessel** | 언어 | 내부 상태 수용체 | 등장인물의 내면 |
| **action** | 언어 | 맥락 검증된 행위 | 플롯 비트 |
| **ability** | 언어 | 행위 자격 | 유전형질 |
| **role** | 언어 | 자격의 구체적 이행 | 표현형 |
| **relation** | 언어 | 주체 간 관계 | 동맹/적대/사제 |
| **effect** | 언어 | 주체에게 닥치는 결과 | 독/저주/강화 |

### intent가 world 위인 이유

```
world = 경계 (울타리)
intent = 관통 (울타리를 넘는 의지)

world 안에 intent를 넣으면:
  → intent가 하나의 world에 갇힌다
  → "결제는 PaymentWorld에서, 배송은 ShippingWorld에서"가 불가능

intent가 world 위에 있으면:
  → intent가 여러 world를 가로지른다
  → Purchase intent = ShopWorld → PaymentWorld → ShippingWorld
  → 사용자의 의지는 시스템 경계에 갇히지 않는다
```

---

## 3. 구현 형태 — 언어 선언 + 실행 엔진 분리

### 핵심 결정: intent declaration은 언어가 직접 이해한다

intent는 언어의 핵심 셀링포인트다. 라이브러리로 숨기지 않는다.

```
src/parser/parser_intent.c
src/parser/ast.h
src/semantic/type_checker_decls.inc
```

현재 구현은 `intent`를 parser/AST/semantic/HIR/codegen이 직접 이해한다.
`Intent(args...)` 호출은 현재 generated runtime function으로 lowering되며, 각 step의
`pre` / `invariant(pre)` / repeated `on` / `guard` / `expect` / `post` / `invariant(post)`를 순서대로 실행한다.
runtime은 same-subject conflict scheduler, last-trace/last-failure history,
그리고 실패 시 reverse-order `compensate:` rollback까지 가진다.

중요한 경계:

- intent는 orchestration declaration이다
- `parallel`은 core execution primitive다
- `spawn/select/async/await`는 `parallel` 아래의 execution surface다

따라서 intent clause 안에 `await`, `spawn`, `async`, `parallel`, `select`, channel send/recv를 직접 넣지 않는다.
비동기 작업은 adapter, worker, hosted action 밖에서 수행하고, intent는 그 결과를 관찰하거나 다음 state transition을 선언하는 쪽에 머문다.

정리:

- `intent`는 왜/무엇을 한다
- `parallel`은 동시에 어떻게 산다
- `async`는 언제 멈추고 재개한다

즉 intent와 parallel은 둘 다 1급시민이지만, 서로 다른 층의 1급시민이다.

```
언어:
  subject, zone, action, effect, relation, world, intent
  → 무엇이 존재하고 의도가 어떤 정적 계약을 가지는가

런타임 엔진:
  parallel scheduling, step ordering, success/failure, conflict arbitration, trace, compensation
  → 그 의도를 실제로 어떻게 진행하고 실패를 다루는가
```

### 현재 컴파일러가 보장하는 것

```
intent(args...) → callable symbol로 등록
involves        → subject type만 허용
where           → zone declaration이어야 함
who             → 선언된 participant alias여야 함
requires        → 알려진 ability여야 함
authorized by   → 선언된 participant alias여야 함
causes          → 알려진 effect여야 함
pre/guard/post/invariant/expect → Bool이어야 함
success/failure → Bool이어야 함
priority        → Int여야 함
```

현재는 step 이름과 같은 `subject action`이 참여 주체 쪽에 없으면 warning을 낸다.
또 `on:`이 없을 때만 zero-user-param same-name action auto-dispatch를 시도한다.
여기에 더해 runtime은 이제 active intent registry를 두고 같은 subject에 대한
`exclusive` 충돌을 막고, `concurrent` / `concurrent`는 허용하며,
더 높은 `priority` intent는 낮은 priority active intent 위로 중첩 진입할 수 있다.
또 failed step은 reverse-order `compensate:` expression을 실행하고,
`IntentLastTrace()` / `IntentLastFailure()` / `IntentLastName()` /
`IntentLastHandle()` / `IntentLastTraceId()` / `IntentLastStepCount()` /
`IntentLastFailed()`로 마지막 실행 기록의 핵심 요약을 읽을 수 있고,
`IntentHistoryCount()` / `IntentHistoryStepName(i)` /
`IntentHistoryStepZone(i)` / `IntentHistoryStepPhase(i)` /
`IntentHistoryStepParticipant(i)` / `IntentHistoryStepSlot(i)` /
`IntentHistoryStepFromZone(i)` / `IntentHistoryStepFromSlot(i)` /
`IntentHistoryStepToZone(i)` / `IntentHistoryStepToSlot(i)` /
`IntentHistoryStepOk(i)` / `IntentHistoryStepFailure(i)`로 step-level typed
history를 읽을 수 있다.
또 `IntentActiveCount()` / `IntentActiveName(i)` /
`IntentActiveHandle(i)` / `IntentActiveTraceId(i)` /
`IntentActivePriority(i)` /
`IntentActiveConcurrent(i)` / `IntentActiveTrace(i)`로 현재 active intent
registry를 직접 읽을 수 있다.
`using:` bound zone이 있으면 현재 `who` participant를 matching subject slot에
실제로 materialize한 뒤 sync를 돈다. `transfer: source -> target;`가 붙으면
source/target zone을 둘 다 live sync하고 `[transfer] ...` trace를 남기며
target zone 쪽으로 handoff materialization을 수행한다. 이제 step body는
`using:` zone의 live subject slot pointer에 `who` participant alias를 재바인딩한 뒤
실행되고, sync 후 canonical participant로 복구된다. 그래서 zone method가 deep nested
participant state를 직접 바꿔도 clause evaluation, rollback, final participant state가 맞는다.
rollback도 이제 intent-level policy를 가진다:

- `rollback: full` = completed step 전체를 reverse-order로 보상
- `rollback: current` = 가장 최근 completed step만 보상
- `rollback: none` = compensate 생략

즉 v0.3의 intent는 **실행 가능한 declaration + conflict scheduler +
trace/rollback runtime + trace-id/history observability +
live zone-instance binding + participant-slot materialization + cross-zone handoff**
까지는 들어왔고, 남은 것은 richer multi-instance timeline query와
richer rollback policy detail이다.

---

## 4. 검증의 이중 구조

### 컴파일 타임 — 언어가 이미 보장하는 것

```pergyra
action ProcessPayment(self)
    requires Payable           // Payable 없으면 → 컴파일 에러
    within PaymentZone         // PaymentZone 밖이면 → 컴파일 에러
    authorized by buyer        // 승인 없으면 → 컴파일 에러
    causes PaymentEffect       // 효과 선언 누락 → 컴파일 에러
{
    ...
}
```

### 런타임 — 현재 있는 것과 다음 단계

```pergyra
intent Purchase
{
    step browse { ... }
    step select { ... }
    step pay { ... }

    // 현재:
    // pre false               → failure 반환
    // invariant(pre) false    → failure 반환
    // on 실행                 → 선언 순서대로 실행
    // guard/expect/post false → failure 반환
    // invariant(post) false   → failure 반환
    //
    // 현재:
    // failure 시 reverse-order compensate 실행
// last trace / last failure / last handle / last step count 조회 가능
// step-level typed history 조회 가능
    // using zoneAlias가 있으면 participant -> zone subject slot materialization 수행
    //
    // 다음 단계:
    // richer trace id / step history API
    // cross-world 전이 엔진
    // richer rollback policy
}
```

### 연결 방식 — 일부는 생겼고, 일부는 아직 미정의

intent가 `where: PaymentZone`으로 zone 타입을 참조하면, 그 zone 안의 action 계약은 언어가 이미 검증한다.
현재는 `who`/`authorized by`와 zone subject slot 타입 적합성, `requires` 능력 구현 여부,
`causes` effect 존재까지는 정적으로 묶이고, runtime은 실제 `who -> zone subject slot`
materialization과 trace line까지 수행한다.
다만 richer trace id/history model과 richer rollback policy는 아직 완전히
명세되지 않았다. cross-world transfer v1은 이제 구현되어, `transfer: cart -> payment;`
같이 source/target zone binding을 선언하면 runtime이 source/target 양쪽을 live sync하고
participant를 target zone slot으로 handoff materialization한다.

구체적으로 다음이 정의되어야 한다:

```
Q1. step이 실행될 때, 해당 zone의 action만 호출 가능하도록 어떻게 제한하는가?
Q2. step이 여러 zone/world 경계를 넘을 때 transfer identity를 어떻게 유지하는가?
   → v1은 "consume"이 아니라 handoff materialization이다.
   → source/target zone 둘 다 sync되고 trace에 `[transfer] participant: From.slot -> To.slot`가 남는다.
Q3. step 간 전이 history를 typed API로 어떻게 노출하는가?
```

이것이 정의되기 전까지, "언어의 검증을 탄다"는 표현은 다음으로 교정한다:

> intent는 언어가 이미 보장한 zone/action/ability/authority 계약을 **참조**한다.
> 다만 그 계약이 step 수준에서 어떻게 연결되는지는
> lowering 규칙 또는 runtime binding 규칙으로 **별도 명시되어야** 한다.

### 정리

```
컴파일 타임 (언어):          런타임 (intent DSL):
─────────────────────        ─────────────────────────
requires ✓                   step 순서 검증
within ✓                     condition 검증
authorized by ✓              cross-world 전이
causes ✓                     intent 성공/실패 판정

연결 지점 (부분 구현 / 남은 것):
  step ↔ zone action 바인딩
  step ↔ concrete zone instance 바인딩 (`using:` + participant-slot materialization 있음)
  step ↔ cross-world zone handoff (`transfer:` v1 있음)
  step ↔ step 상태 전달
  typed trace/history API (v1 step surface는 구현됨)
```

---

## 5. intent의 구성 요소

```pergyra
intent Purchase
{
    // --- 참여 주체 (world 무관) ---
    involves buyer: Member;
    involves seller: Merchant;

    // --- 단계 (zone 타입 참조) ---
    step browse
    {
        where: ProductView;
        who: buyer;
        post: buyer.viewed_count > 0;
    }

    step select
    {
        where: CartSection;
        who: buyer;
        requires: Purchasable;
        pre: buyer.session.is_alive;
        post: buyer.cart.size > 0;
    }

    step negotiate
    {
        where: TradeZone;
        who: buyer, seller;
        requires: Negotiable;
        authorized by: buyer, seller;
        guard: deal.price > 0;
        post: deal.price_agreed;
    }

    step pay
    {
        where: PaymentZone;
        using: payment;
        transfer: cart -> payment;
        who: buyer;
        requires: Payable;
        causes: PaymentEffect;
        authorized by: buyer;
        pre: buyer.wallet.balance >= deal.price;
        post: payment.status == Approved;
        invariant: buyer.session.is_alive;
    }

    // --- 의도의 결과 ---
    success: buyer owns items AND seller received payment;
    failure: rollback to browse;
}
```

### 조건 분류 (expect를 4개로 분리)

기존의 `expect`는 너무 넓다. 다음으로 분리한다:

| 조건 | 역할 | 시점 | 실패 시 |
|------|------|------|--------|
| `pre` | step 진입 전 충족 필수 | step 시작 전 | step 진입 거부 |
| `post` | step 완료 후 만족 필수 | step 완료 후 | step 실패 처리 |
| `guard` | step 진행 중 지속 검사 | step 실행 중 | step 중단 |
| `invariant` | intent 전체에서 항상 참 | 모든 step | intent 전체 실패 |

### 구성 요소 정리

| 요소 | 역할 | 검증 시점 |
|------|------|----------|
| `involves` | 이 의도에 참여하는 subject 타입 | intent 시맨틱 검증 시 타입 존재 확인 |
| `step` | 의도의 단계 | 런타임 (FSM 순서) |
| `where` | step이 일어나는 zone 타입 | intent 시맨틱 검증 시 zone 타입 참조 → 런타임 인스턴스 바인딩 |
| `who` | 이 step에서 행동하는 subject | intent 시맨틱 검증 시 involves 참조 확인 |
| `requires` | 자격 조건 | 언어 수준 ability 검증 (컴파일) |
| `authorized by` | 승인 주체 | 언어 수준 authority 검증 (컴파일) |
| `causes` | 발생하는 효과 | 언어 수준 effect 검증 (컴파일) |
| `pre` | 사전 조건 | 런타임 |
| `post` | 사후 조건 | 런타임 |
| `guard` | 진행 중 조건 | 런타임 |
| `invariant` | 전역 불변 조건 | 런타임 |
| `success` | 의도 성공 판정 | 런타임 |
| `failure` | 의도 실패 시 행동 | 런타임 |

---

## 6. cross-world 관통

### 기본 예시

```pergyra
world ShopWorld
{
    zone browse: ProductView;
    zone cart: CartSection;
}

world PaymentWorld
{
    zone payment: PaymentZone;
    zone receipt: ReceiptZone;
}

intent Purchase
{
    involves buyer: Member;

    step browse  { where: ProductView;  who: buyer; }
    step select  { where: CartSection;  who: buyer; }
    step pay     { where: PaymentZone;  who: buyer; }   // world 경계 넘음
    step confirm { where: ReceiptZone;  who: buyer; }   // 여전히 다른 world
}
```

### cross-world 전이 규칙

```
1. 기본 cross-world orchestration의 1급 추상은 intent다.
   → subject가 단독으로 world를 넘는 것은 기본적으로 불가.
   → intent의 step이 다른 world의 zone을 참조할 때 전이 발생.

2. 특수 전이 채널은 별도로 허용한다.
   → system event (이벤트 버스, 타이머)
   → recovery flow (장애 복구 replay)
   → admin override (관리자 강제 개입)
   → 이것들은 intent와 다른 메커니즘이며, 별도 primitive로 정의 필요.

3. 전이 시 subject handoff
   → v1 구현은 `transfer: source -> target;`로 concrete zone binding을 잡고,
     `who` participant를 source/target zone의 matching subject slot에 materialize한 뒤
     양쪽 zone을 sync한다.
   → 즉 현재는 "cross-world consume-move"가 아니라 "live handoff materialization"이다.
   → tobject는 여전히 경계 투영 역할을 맡고, richer identity handoff는 다음 단계다.

4. 각 world의 컴파일 타임 계약은 유지
   → PaymentWorld 안의 action 제약은 여전히 컴파일러가 검증.
   → intent는 "어떤 순서로 어떤 world를 방문하는가"만 관리.
```

---

## 7. 기존 패러다임 대비

같은 "쇼핑" 기능:

```java
// OOP — use-case layer로 의도 표현 가능하지만 언어가 강제하지 않음
class ShoppingCart {
    List<Item> items;
    void addItem(Item item) { ... }
    void checkout() { ... }
    // use-case service에서 의도를 조직할 수 있지만
    // 언어 수준에서 "누가, 어디서, 자격, 승인"은 보이지 않음
}
```

```haskell
-- FP — algebra/effect system으로 의도 인코딩 가능하지만 암시적
addItem :: Cart -> Item -> Cart
checkout :: Cart -> Payment -> Result Order
-- free monad 등으로 의도를 표현할 수 있지만
-- 맥락(누가, 어디서)은 타입에 직접 드러나지 않음
```

---

## 8. 프로젝트 구조 — intent가 목차가 된다

intent가 1급 언어 요소가 되면, 프로젝트 구성도 intent 중심으로 읽히는 편이 맞다.

권장 예시:

```text
project/
  intents/              ← FIRST: what does the user want?
    purchase.pgy
    refund.pgy
    onboarding.pgy

  subjects/             ← SECOND: who acts?
    member.pgy
    merchant.pgy

  zones/                ← THIRD: where do they act?
    shop.pgy
    payment.pgy

  world.pgy             ← FOURTH: execution boundary
  main.pgy              ← entry point
```

### 왜 `intents/`가 먼저인가

기존 구조는 보통 `models/`, `services/`, `controllers/`부터 시작한다.
그러면 프로그램이 "무엇을 하려는가"보다 "무엇으로 구현했는가"가 먼저 보인다.

intent-first 구조는 반대다.

```text
intents/ 폴더만 읽어도
"이 프로그램이 무엇을 하려는 프로그램인가"
를 먼저 이해할 수 있어야 한다.
```

소설 비유로 보면:

- `intents/` = 줄거리 요약
- `subjects/` = 등장인물
- `zones/` = 장면/무대
- `world.pgy` = 작품이 실제로 굴러가는 세계 경계

즉 Pergyra 프로젝트에서 `intents/`는 단순 기능 폴더가 아니라,
프로그램 전체의 **의도 목차(table of contents)** 역할을 한다.

### 실행 순서가 아니라 설계 순서

이 구조는 “실행이 intents에서 시작한다”는 뜻이 아니다.
실행은 여전히 `main.pgy`와 `world.pgy`가 맡는다.

의미는 이것이다:

```text
설계를 시작할 때 intent를 먼저 쓴다.
그리고 그 intent가 요구하는 subject / zone / ability / effect를
역산(backward derivation)해서 채운다.
```

---

## 9. 역산 패턴 — intent가 TODO를 만든다

intent를 먼저 선언하면, 필요한 언어 구성요소가 자동으로 드러난다.

```pergyra
intent Purchase(buyer: Member, seller: Merchant)
{
    step browse
    {
        where: ProductView;
        who: buyer;
    }

    step pay
    {
        where: PaymentZone;
        who: buyer;
        requires: Purchasable;
        causes: PaymentEffect;
    }
}
```

이 선언 하나만으로 체크리스트가 생긴다.

```text
buyer는 Member 타입이어야 함
→ subject Member 필요

seller는 Merchant 타입이어야 함
→ subject Merchant 필요

where: ProductView
→ ProductView zone 필요

where: PaymentZone
→ PaymentZone zone 필요

requires: Purchasable
→ ability Purchasable 필요

causes: PaymentEffect
→ effect PaymentEffect 필요
```

즉 intent는 단순 orchestrator가 아니라,
프로젝트의 **역산 가능한 요구 명세(checklist generator)** 다.

### 컴파일러가 TODO를 만든다

이 구조의 중요한 장점은 missing dependency가 곧 TODO가 된다는 점이다.

예:

```text
error: zone 'ProductView' not found
error: ability 'Purchasable' not found
error: effect 'PaymentEffect' not found
```

이 메시지는 단순 오류가 아니라, 아직 만들어야 할 조각 목록이다.

기존 OOP에서:

```text
class 하나를 만들었다고 해서
다음에 무엇을 만들어야 하는지는 바로 드러나지 않는다.
```

intent-first에서는:

```text
intent 하나를 쓰면
필요한 subject, zone, ability, effect가 자동으로 드러난다.
```

이건 단순 폴더 정리가 아니라 설계 방법론이다.

### 권장 역산 순서

```text
1. intents/
   - 사용자/시스템의 의도를 적는다

2. subjects/
   - 그 intent에 참여하는 능동 주체를 만든다

3. zones/
   - step이 실제로 일어날 문맥/무대를 만든다

4. abilities / effects / relations
   - intent step의 자격, 결과, 관계를 채운다

5. world.pgy
   - 그 모든 것을 실제 실행 경계로 묶는다

6. main.pgy
   - entry와 seed/runtime wiring을 붙인다
```

### 요약

```text
기존 구조:
  구현 단위가 먼저 보인다

intent-first 구조:
  의도가 먼저 보인다

기존 설계:
  필요한 조각을 사람이 추론한다

intent-first 설계:
  intent 선언이 필요한 조각을 역산해 준다
```

```pergyra
// Intent DSL — "사용자가 무엇을 하려 하는가?"
intent Purchase
{
    involves buyer: Member;
    involves seller: Merchant;

    step browse  { where: ProductView; who: buyer; }
    step select  { where: CartSection; who: buyer; requires: Purchasable; }
    step pay     { where: PaymentZone; who: buyer; authorized by: buyer; }

    // 읽는 순간 안다:
    // - 누가: buyer, seller
    // - 무엇을: browse → select → pay
    // - 어디서: ProductView → CartSection → PaymentZone
    // - 자격: Purchasable
    // - 승인: buyer
}
```

---

## 8. 미정의 사항 — 최소 명세 필요 목록

이 설계가 실체화되려면 다음 6개가 명문화되어야 한다:

### 8.1 step의 실행 단위

```
Q. step이 action 1개에 대응하는가?
   action 여러 개의 묶음인가?
   단지 FSM 상태 이름인가?
→ 미정의
```

### 8.2 lowering 규칙

```
Q. intent DSL이 무엇으로 변환되는가?
   FSM 구조체? action graph? 런타임 바이트코드?
→ 미정의
```

### 8.3 world 전이 모델

```
Q. 직렬화 포맷은?
   subject identity 유지 방식은?
   rollback/compensation 규칙은?
→ 미정의
```

### 8.4 step ↔ zone 바인딩

```
Q. step의 where가 zone 타입일 때,
   런타임에서 어떤 world의 어떤 인스턴스에 바인딩되는가?
   바인딩 실패 시 어떻게 되는가?
→ 미정의
```

### 8.5 에러 모델

```
Q. step rejected, authorization denied,
   world transition failed, compensation failed
   각각의 에러 타입과 전파 규칙은?
→ 미정의
```

### 8.6 observability

```
Q. trace id, current intent state, step history,
   failure reason을 어떻게 노출하는가?
→ 미정의
```

---

## 9. 설계 원칙

### 9.1 조합 가능하다

```pergyra
// 독립된 intent 조합
intent Purchase { ... }
intent Refund { ... }
intent CustomerSupport { ... }

// intent는 서로 독립 — 각자의 step/flow를 가짐
// 하나의 subject가 여러 intent에 참여 가능
// Member는 Purchase에도, Refund에도 참여
```

### 9.2 Intent 오케스트레이션 — intent 안에 intent

intent는 step만 가지는 게 아니라 **하위 intent를 포함**할 수 있다. 대목차 안에 소목차가 있는 것과 같다.

```pergyra
// 소목차 intent들
intent BrowseCatalog(buyer: Member)
{
    who: buyer;
    step search { where: SearchZone; }
    step filter { where: SearchZone; }
    success: true;
}

intent ProcessPayment(buyer: Member)
{
    who: buyer;
    step verify { where: PaymentZone; requires: Payable; }
    step charge { where: PaymentZone; authorized by: buyer; }
    success: true;
}

intent ArrangeShipping(buyer: Member)
{
    who: buyer;
    step address { where: ShippingZone; }
    step dispatch { where: ShippingZone; }
    success: true;
}

// 대목차 — 하위 intent들을 오케스트레이션
intent CompletePurchase(buyer: Member)
{
    exclusive;
    who: buyer;

    step browse
    {
        intent: BrowseCatalog(buyer);       // 하위 intent 호출
        post: buyer.cart.size > 0;
    }

    step pay
    {
        intent: ProcessPayment(buyer);      // 하위 intent 호출
        post: buyer.payment_done;
    }

    step ship
    {
        intent: ArrangeShipping(buyer);     // 하위 intent 호출
        post: buyer.shipping_arranged;
    }

    success: buyer.order_complete;
    failure: rollback;
}
```

#### 구조

```
CompletePurchase (대목차)
  ├── BrowseCatalog (소목차)
  │     ├── step search
  │     └── step filter
  ├── ProcessPayment (소목차)
  │     ├── step verify
  │     └── step charge
  └── ArrangeShipping (소목차)
        ├── step address
        └── step dispatch
```

#### 규칙

```
1. step 안에서 intent: 로 하위 intent를 호출할 수 있다
2. 하위 intent가 실패하면 상위 step도 실패한다
3. 상위 intent의 compensate는 하위 intent의 compensate를 역순으로 호출한다
4. 하위 intent는 독립적으로도 호출 가능하다 (소목차만 단독 실행)
5. 깊이 제한은 없지만, 3단 이상 중첩은 설계 냄새
```

#### DI(Dependency Injection)와의 비교

```
DI:              "이 클래스가 필요로 하는 의존성을 외부에서 주입한다"
                 → 객체 단위 조립

Intent 오케스트레이션: "이 의도가 필요로 하는 하위 의도를 조립한다"
                 → 목적 단위 조립
```

하위 intent는 교체 가능하다. 구현이 아니라 계약(step 구조)에 의존하기 때문:

```pergyra
// 프로덕션
intent CompletePurchase(buyer: Member)
{
    step pay { intent: ProcessPayment(buyer); }
}

// 테스트 — 결제를 모킹
intent CompletePurchase(buyer: Member)
{
    step pay { intent: MockPayment(buyer); }   // 교체
}
```

DI가 객체의 조립이라면, intent 오케스트레이션은 **목적의 조립**이다.

#### 구현 상태

`step` 안의 `intent:` 하위 호출은 이제 구현됐다.

```pergyra
zone PaymentZone {
    subject slot buyer: Member
}

intent Charge(payment: PaymentZone, buyer: Member)
{
    step verify
    {
        where: PaymentZone;
        using: payment;
        who: buyer;
        expect: true;
    }
    success: true;
    failure: false;
}

intent CompletePurchase(payment: PaymentZone, buyer: Member)
{
    step pay
    {
        intent: Charge(payment, buyer);
        expect: true;
    }
}
```

현재 의미:
- `intent:`는 named intent call이어야 한다
- 하위 intent는 `Bool`을 반환해야 한다
- 하위 intent가 `false`를 반환하면 부모 step은 실패로 간주된다
- orchestration step은 `where` / `who` 없이도 하위 intent 호출만으로 합법이다
- 호출되는 하위 intent 자체는 여전히 자기 `where` / `using` / `who` 계약을 만족해야 한다
- `on:`과 `intent:`를 함께 둘 수 있고, 둘 다 있으면 현재 구현은 `on:`을 먼저 실행한 뒤 하위 intent 호출을 평가한다

### 9.3 닫힌 시스템 철학

Pergyra 프로그램은 **업데이트를 전제하지 않는 닫힌 시스템**을 지향한다.

```
기존 사고: "이 시스템은 계속 업데이트될 것이다"
  → 확장 포인트, 플러그인 인터페이스, feature flag
  → 필요하지 않은 추상화가 쌓인다

Pergyra 사고: "이 시스템은 이 문제를 완결한다"
  → 딱 필요한 intent만 선언
  → 필요한 subject, zone, action만 구현
  → 업데이트는 새 intent 추가로 — 기존 코드를 변경하지 않는다
```

intent가 이 철학을 가능하게 한다:

```
v1.0:
  intents/
    purchase.pgy
    refund.pgy

v1.1 (새 기능 추가):
  intents/
    purchase.pgy      ← 변경 없음
    refund.pgy        ← 변경 없음
    gift_card.pgy     ← 새 intent 추가

기존 intent는 닫혀 있다. 새 intent가 추가될 뿐이다.
Open-Closed Principle이 intent 단위로 자연스럽게 적용된다.
```

### 9.4 선언적이다

intent의 step은 **실행 순서가 아니라 의미론적 단계**다.

```
절차형의 step: "이걸 먼저 하고 저걸 나중에 해"
intent의 step:  "이 의도를 달성하려면 이 단계들이 필요하다"

순서는 의도의 논리적 구조에서 나오지,
프로그래머가 강제하는 것이 아니다.
```

### 9.3 intent가 subject를 움직인다

이것이 핵심 의미론이다.

```
기존: subject가 action을 가지고 있다. 그런데 "왜 움직이는가?"가 없다.
지금: intent가 subject를 움직인다. subject는 intent의 step 안에서 행동한다.

subject가 "무엇인가"는 언어가 정의한다.
subject가 "왜 움직이는가"는 intent가 정의한다.
```

---

## 10. Intent-First 개발 방법론

### 10.1 프로젝트 구조

intent가 1급이면, 프로젝트 구조가 바뀐다. **intents/ 폴더가 프로젝트의 목차**다.

```
project/
  domains/                        ← 바운디드 컨텍스트 (도메인 분리)
    commerce/
      intents/                    ← 1. 왜 하는가? (purchase, refund)
        purchase.pgy
        refund.pgy
      zones/                      ← 2. 어디서 하는가?
        shop_zone.pgy
        payment_zone.pgy
      subjects/                   ← 3. 누가 하는가?
        buyer.pgy
        merchant.pgy
      abilities/                  ← 4. 자격은?
        purchasable.pgy
      effects/                    ← 5. 결과는?
        payment_effect.pgy

    onboarding/
      intents/
        signup.pgy
      zones/
        registration_zone.pgy
      subjects/
        applicant.pgy

  commons/                        ← 도메인 무관 공유 자원
    types/                        ← 수동 데이터 (class, struct, object, tobject)
      product.pgy                 ← class Product { ... }
      payment_payload.pgy         ← struct PaymentPayload { ... }
      order_receipt.pgy           ← tobject OrderReceipt { ... }
    infra/                        ← 외부 세계와의 I/O (FFI, DB, HTTP)
      stripe_client.pgy           ← extern "C" { ... }

  world.pgy                       ← 도메인들을 엮는 실행/신뢰 경계
  main.pgy                        ← 진입점
```

구조의 원칙:

```
1. intents/는 프로젝트의 사용자 목적을 모은다.
   이 디렉터리는 프로그램의 목차이며,
   시스템이 무엇을 하려는지 가장 먼저 드러내는 진입점이다.

2. 각 intent는 자신이 요구하는 subject, zone, ability, effect를 참조한다.
   따라서 개발자는 intent를 통해
   필요한 도메인 프리미티브를 역으로 탐색할 수 있다.

3. subjects, zones, abilities, effects는
   intent가 사용하는 재사용 가능한 의미론적 구성요소다.
   이들은 intent의 구현 대상이지,
   특정 intent 하나의 부산물로만 취급되지 않는다.

4. commons/는 intent를 발생시키지 않는 수동 데이터와 인프라다.
   class, struct, tobject, object는 types/에,
   FFI, DB, HTTP 클라이언트는 infra/에 격리한다.

5. 독해의 시작점은 intent다.
   의미론의 기반은 world/subject/zone/action이다.
   이 둘은 구분된다.
```

### intent 단위 기준 — workflow fragment와 구분

좋은 intent (사용자 목적 단위):
- `Purchase` — "물건을 사고 싶다"
- `Refund` — "환불받고 싶다"
- `Onboarding` — "가입하고 싶다"

나쁜 intent (내부 구현 단위):
- `PurchaseConfirmStock` — 이건 Purchase의 step이지 별도 intent가 아님
- `RetryFailedPayment` — 이건 시스템 보상 로직이지 사용자 의도가 아님
- `SyncInventory` — 이건 인프라 작업이지 사용자 목적이 아님

```
intent의 단위 = 사용자가 한 문장으로 말할 수 있는 목적
"나는 물건을 사고 싶다" → Purchase ✓
"나는 재고를 확인하고 싶다" → 이건 Purchase의 step ✗
```

### 10.2 독해 순서와 구현 순서

```
기존 개발:
  "데이터 구조 설계 → 함수 작성 → 연결 → 테스트"
  Bottom-up. 만들고 나서 "이게 뭘 하는 건지" 맞춤.

Intent-first 개발:
  "사용자 의도 정의 → 필요한 subject 역산 → zone/action 구현"
  Top-down. 의도에서 출발해서 아래로 내려감.
```

intent 하나 쓰면 필요한 모든 것의 체크리스트가 자동으로 나온다:

```pergyra
intent Purchase(buyer: Member, seller: Merchant)
{
    step browse
    {
        where: ProductView;                // → zone ProductView 필요
        who: buyer;                        // → subject Member 필요
    }

    step select
    {
        where: CartSection;                // → zone CartSection 필요
        who: buyer;
        requires: Purchasable;             // → ability Purchasable 필요
    }

    step pay
    {
        where: PaymentZone;                // → zone PaymentZone 필요
        who: buyer;
        requires: Payable;                 // → ability Payable 필요
        authorized by: buyer;
        causes: PaymentEffect;             // → effect PaymentEffect 필요
    }
}
```

컴파일러가 내는 에러가 곧 TODO 목록:

```
error: zone 'ProductView' not found     → ProductView 구현하라
error: ability 'Purchasable' not found  → Purchasable 정의하라
error: effect 'PaymentEffect' not found → PaymentEffect 정의하라
error: subject 'Member' not found       → Member 선언하라
```

구현의 탐색 순서는 보통 intent에서 시작된다. 다만 재사용 가능한 공통 프리미티브(AuthenticatedMember, TransactionalZone 등)는 별도 코어 계층에서 독립적으로 선행 정의될 수 있다.

### 10.3 설계 순서

```
1단계: intents/ 작성
   "이 프로그램의 사용자는 뭘 하려 하는가?"
   → Purchase, Refund, Onboarding, ...

2단계: 컴파일 → 에러 목록 = TODO
   "Purchase에 Member가 필요하다, ProductView가 필요하다, ..."

3단계: subjects/ 작성
   "누가 움직이는가? Member, Merchant, ..."

4단계: zones/ 작성
   "어디서 움직이는가? ProductView, CartSection, PaymentZone, ..."

5단계: abilities/, effects/ 작성
   "자격과 결과는? Purchasable, PaymentEffect, ..."

6단계: world.pgy 작성
   "실행 경계를 구성한다"

7단계: 다시 컴파일 → 에러 0 = 설계 완료
```

### 10.4 intent 실행 모델

#### 호출 방식 — 함수처럼

```pergyra
// 선언이 곧 실행 가능한 단위
intent Purchase(buyer: Member, seller: Merchant)
{
    step browse { ... }
    step pay { ... }
}

// 호출
Purchase(hero, merchant);
```

intent 이름이 곧 실행 트리거. `Purchase(hero, merchant)`가 의도를 활성화한다.

#### step 전이 — 혼합 (on + 자동)

```pergyra
intent DriveCar(driver: Person)
{
    step ignition
    {
        on: driver.TurnKey();         // 이벤트 대기 — 키를 돌려야 함
        post: engine.running;
    }

    step warmup
    {
        // on 없음 → post 충족 시 자동 전이 — 예열은 알아서 됨
        post: engine.temp > 60;
    }

    step drive
    {
        on: driver.ShiftGear();       // 이벤트 대기 — 기어는 넣어야 함
        requires: Licensed;
        post: car.moving;
    }
}
```

규칙:
- `on`이 있으면 → 해당 이벤트까지 대기
- `on`이 없으면 → post 조건 충족 시 자동 진행

#### 충돌 처리 — 기본 exclusive

```pergyra
intent Purchase(buyer: Member)
{
    exclusive;                 // 기본: 실행 중 같은 subject의 다른 intent 대기
    ...
}

intent Browse(user: Member)
{
    concurrent;                // 명시: 다른 intent와 동시 가능
    ...
}
```

규���:
- 기본은 `exclusive` — 한 subject가 이 intent 실행 중이면 다른 intent 대기
- `concurrent`를 명시하면 — 다른 intent와 동시 실��� 가능

---

## 11. 결정 이력

| 결정 | 선택 | 이유 |
|------|------|------|
| intent 위치 | 언어 1급 declaration + runtime engine 분리 | 정적 계약은 컴파일러가 이해, 실행 정책은 엔진에 남김 |
| intent 계층 | world 위 (최상위) | intent는 world를 관통. world = 경계, intent = 관통 |
| 호출 방식 | 함수처럼 `Purchase(buyer, seller)` | 선언이 곧 실행 단위. 가장 직관적 |
| step 전이 | 혼합 (on + 자동) | on 있으면 이벤트 대기, 없으면 post 충족 시 자동 전이 |
| 충돌 처리 | 기본 exclusive, 명시 concurrent | 안전한 기본값 + 필요 시 열기 |
| step.where | zone 타입 | 인스턴스면 특정 world에 묶임. 타입이면 cross-world 가능 |
| 조건 분류 | pre/post/guard/invariant | 단일 expect는 너무 넓음. 4분류로 정밀 제어 |
| cross-world 독점 | 완화 | intent만 cross-world 가능은 너무 절대적. 특수 전이 채널 허용 |
| step ↔ zone 연결 | 미정의 (정직) | 바인딩 규칙이 없으면 정적 검증 주장은 과장 |
| OOP/FP 비교 | 공정하게 | OOP/FP도 의도 표현 가능. 다만 1급 구조가 아닐 뿐 |
| 개발 방법론 | Intent-first (Top-down) | 독해 시작점 = intent, 의미론 기반 = domain |
| intent 단위 | 사용자 목적 단위 | workflow fragment나 기술 이벤트는 intent가 아님 |
| 프리미티브 독립성 | intent 종속이 아닌 재사용 | 공통 subject/zone은 코어에서 선행 정의 가능 |
| 폴더 구조 | domains/ + commons/ | 도메인 경계 분리 + 수동 데이터/인프라 격리 |

---

## 12. 다음 단계

```
1. intent 파서 구현 — lexer 키워드 + parser + AST 노드
2. intent 시맨틱 검증 — involves/step의 타입 참조 확인
3. intent 코드젠 — FSM 구조체 + step 전이 함수
4. 단일 world intent 예제 — 파싱 → 실행 → 디버깅
5. 에러 모델 + observability 정의
6. cross-world intent 예제
```
