# Intent-First 설계 철학

Status: canonical design and authoring guidance

> **프로그램의 핵심은 자료구조를 잘 만드는 것이 아니라<br>현실의 의도를 정확한 실행 단위로 닫는 것이다.**

---

## 한 문장 정의

**Pergyra는 intent를 최상위 설계 축으로 잡아, 현실의 복잡한 행위를 목적 단위로 닫고 나머지 구조를 유도하는 언어다.**

이 정의에서 핵심어는 **"유도한다"** 다. intent가 정해지면 설계자는
world, zone, subject, ability, effect, object/tobject 경계를 그 목적에서 역산한다.
이는 현재 컴파일러가 선언을 자동 생성한다는 뜻이 아니다.

---

## 왜 Intent가 1차인가

대부분의 언어는 이것들을 1차로 둔다:

```
데이터 구조    → 타입
실행 단위      → 함수
모듈화         → 모듈/클래스
```

Pergyra는 순서를 뒤집는다:

```
무엇을 위해        → intent (1차)
어떤 세계/장면에서 → world/zone
누가 수행하는가    → subject (2차 host)
어떤 조건에서      → requires/authorized by/guard
무슨 결과를        → effect/success/failure
```

이 순서가 다른 이유가 있다.

**프로그래밍을 "계산"보다 "행위의 조직화"로 보기 때문이다.**

## 예제를 보여 주는 순서도 intent-first여야 한다

여기서 자주 생기는 오해가 있다.

- `subject`가 core host라는 사실
- `intent`가 설계의 1차 계약이라는 사실

이 둘을 섞으면 예제가 다시 주체 정의부터 시작하는 문서처럼 보인다.

하지만 문서가 가르쳐야 하는 것은 다음이다.

1. 먼저 `intent`를 본다
2. 그 intent가 놓일 `world` / `zone` 경계를 본다
3. 그 다음 그 계약을 실행할 `subject`를 본다

즉:

```text
읽기/설계 순서: intent -> world -> zone -> subject
구현/host 축:   subject-core
```

문서 예제는 이 두 축을 분리해서 보여 줘야 한다.
`subject`를 먼저 소개하면 독자는 Pergyra를 `subject`부터 정의하는 언어로 배우게 되는데,
그건 lowering/host 관점의 일부만 보여 주는 것이고 설계 관점의 학습 순서로는 틀리다.

---

## 함수가 아니라 Intent가 1차인 이유

### 함수는 이미 쪼개진 조각이다

```c
// 함수는 "어떻게"에 답한다
int CalculateDamage(int atk, int def) {
    return atk - def;
}
```

이 함수는 다음을 알 수 없다:
- **누가** 왜 이 계산을 하는가
- 이 계산이 **어떤 맥락**에서 발생하는가
- 계산 실패 시 **무엇을 보상**해야 하는가
- 이 계산이 **어떤 경계** 안에서 유효한가

### Intent는 현실의 목적이다

```pergyra
// Intent는 "왜"에 답한다
intent SlayDragon(hero: Player, dragon: Monster)
{
    who: hero;

    step attack
    {
        where: BattleZone;
        requires: Combatable;
        authorized by: hero;
        on: hero.Attack(dragon);
        compensate: hero.Retreat();
        post: dragon.hp <= 0 || hero.hp > 0;
    }

    success: dragon.hp <= 0;
    failure: hero.fled = true;
}
```

이 Intent는 다음을 **표면에 올린다**:
- **주체**: hero
- **자격**: Combatable ability 필요
- **승인**: hero 자신이 승인
- **실행**: Attack action
- **보상**: 실패 시 Retreat
- **성공 조건**: dragon이 죽거나 hero가 생존
- **실패 처리**: hero가 도주

---

## Intent가 틀리면 나머지 구조가 다 흔들린다

### Intent가 정확할 때 — 나머지가 따라온다

```
intent 정의
  ↓
필요한 world/zone이 보임   → BattleZone, RaidWorld
필요한 subject가 보임      → Player, Monster
필요한 ability가 보임      → Combatable
object/tobject이 보임     → PlayerView, BattleReport
effect와 relation이 보임  → DamageEffect, AggroRelation
rollback/compensate 지점  → Retreat()
```

### Intent가 애매할 때 — 전부 흔들린다

```
intent가 애매
  ↓
subject가 이상해짐         → Player인가 Character인가 Entity인가
ability가 과하거나 부족함  → Combatable + Attackable + Targetable... (과다)
zone이 뒤틀림              → BattleZone인가 WorldZone인가 DungeonZone인가
effect가 중복됨            → DamageEffect, AttackEffect, HitEffect... (중복)
generic contract가 붕 뜸   → <T>의 의미가 불명확해짐
```

---

## Intent 설계의 균형 — 너무 작아도, 너무 커도 안 된다

### 너무 작게 잡으면: 단순 명령 나열

```pergyra
// 나쁜 예 — intent가 너무 작음
intent SwingSword(hero: Player, target: Monster)
{
    step swing
    {
        on: hero.SwingWeapon();
    }
    success: true;
}

intent MoveForward(hero: Player)
{
    step move
    {
        on: hero.Move(1);
    }
    success: true;
}
```

이것은 intent가 아니라 함수 호출이다. "왜"가 없다.

### 너무 크게 잡으면: 현실을 다루지 못함

```pergyra
// 나쁜 예 — intent가 너무 큼
intent CompleteEntireGame(hero: Player)
{
    step do_everything
    {
        where: EntireWorld;
        on: hero.PlayGame();
    }
    success: hero.won;
}
```

이것은 intent가 아니라 main 함수다. 실패 지점이 보이지 않는다.

### 적절한 크기: 목적을 잃지 않으면서 감당 가능한 단위

```pergyra
// 좋은 예 — 목적 단위
intent ClearDungeon(hero: Player, dungeon: DungeonZone)
{
    who: hero;

    step enter
    {
        where: dungeon;
        on: hero.EnterDungeon();
        compensate: hero.ExitDungeon();
        pre: hero.level >= dungeon.min_level;
    }

    step fight_boss
    {
        where: dungeon;
        requires: Combatable;
        on: hero.Attack(dungeon.boss);
        compensate: hero.FleeDungeon();
        post: dungeon.boss.hp <= 0;
    }

    step collect_reward
    {
        where: dungeon;
        on: hero.CollectTreasure();
        pre: dungeon.boss.hp <= 0;
    }

    success: dungeon.cleared;
    failure: hero.fled;
}
```

이 intent는:
- **명확한 목적**: 던전 클리어
- **감당 가능한 범위**: enter → fight → collect
- **실패 지점 보임**: 각 step에 compensate 정의
- **성공/실패 조건 명시**: cleared vs fled

---

## Intent를 정의하는 4가지 질문

좋은 intent를 찾으려면 다음 질문을 순서대로 답해야 한다:

### 1. "누가 무엇을 이루려 하는가?" (Purpose)

```
질문: 이 시스템(또는 기능)의 궁극적 목적은 무엇인가?
검사: "이 intent가 성공하면 무엇이 달라지는가?"에 한 문장으로 답해야 한다.
```

```pergyra
// 명확한 목적
intent PurchaseItem(buyer: Member, item: Item)
// → "구매자가 아이템을 소유하게 된다"

// 모호한 목적
intent ProcessData(data: Data)
// → "데이터가 처리된다" (무엇이 달라지는가?)
```

### 2. "어디까지가 한 행위인가?" (Boundary)

```
질문: 어디서 시작해서 어디서 끝나는가?
검사: step 수는 경계 재검토 신호일 뿐 semantic 판정이나 gate가 아니다.
      여러 현실 목적이나 fact 귀속이 섞였을 때 목적별로 분리한다.
```

```pergyra
// 적절한 경계 — 4단계
intent Checkout(buyer: Member, cart: Cart)
{
    step validate_cart   { ... }   // 1
    step calculate_total { ... }   // 2
    step process_payment { ... }   // 3
    step confirm_order   { ... }   // 4
}

// 경계 초과 — 10단계 (쪼개야 함)
intent Checkout(buyer: Member, cart: Cart)
{
    step validate_cart       { ... }  // → ValidateCart로 분리
    step check_stock         { ... }  // → ValidateCart에 포함
    step apply_discount      { ... }  // → CalculateTotal에 포함
    step calculate_subtotal  { ... }  // → CalculateTotal에 포함
    step calculate_tax       { ... }  // → CalculateTotal에 포함
    step calculate_shipping  { ... }  // → CalculateTotal에 포함
    step process_payment     { ... }  // → ProcessPayment로 분리
    step verify_fraud        { ... }  // → ProcessPayment에 포함
    step confirm_order       { ... }  // → ConfirmOrder로 분리
    step send_email          { ... }  // → ConfirmOrder에 포함
}
```

### 3. "실패를 어디서 끊을 것인가?" (Failure Point)

```
질문: 각 step이 실패하면 무엇을 보상해야 하는가?
검사: `rollback: full`인 effectful step은 `compensate` 또는 명시적
      `irreversible`을 가져야 한다. 순수·무상태 step에는 의례적으로 강제하지 않는다.
```

```pergyra
// 실패 지점이 명확
intent TransferFunds(from: Account, to: Account)
{
    step withdraw
    {
        on: from.Withdraw(amount);
        compensate: from.Deposit(amount);  // 출금 취소
    }

    step deposit
    {
        on: to.Deposit(amount);
        compensate: from.Deposit(amount);  // 입금 취소 (출금도 취소)
    }

    success: from.balance >= 0 && to.balance > 0;
    failure: rollback;
}
```

### 4. "누가 책임을 질 것인가?" (Authority)

```
질문: 이 intent의 각 step을 누가 승인하는가?
검사: authorized by가 없는 step이 있으면 권한 모델이 불명확.
```

```pergyra
// 권한이 명확
intent ApproveLoan(officer: LoanOfficer, application: LoanApplication)
{
    step review
    {
        who: officer;
        authorized by: officer;        // 담당자가 검토 승인
        requires: LoanReviewer;
    }

    step escalate
    {
        who: officer;
        authorized by: manager;        // 금액 초과 시 관리자 승인
        pre: application.amount > 100000;
    }

    step disburse
    {
        who: officer;
        authorized by: system;         // 시스템 자동 승인
        post: application.status == Approved;
    }
}
```

---

## Intent가 나머지를 유도하는 구조

### Intent → Subject

```pergyra
// Intent를 먼저 쓰면
intent TradeGoods(seller: Merchant, buyer: Customer)
{
    step list_item    { who: seller; }
    step browse_items { who: buyer; }
    step make_offer   { who: buyer; }
    step accept_offer { who: seller; }
}

// Subject가 자동 유도
subject Merchant { ... }   // seller
subject Customer { ... }   // buyer
```

### Intent → Ability

```pergyra
// Intent에 requires가 있으면
intent SlayDragon(hero: Player)
{
    step attack
    {
        requires: Combatable;    // 이 ability가 필요함
    }
}

// Ability가 자동 유도
ability Combatable {
    func CanFight(self) -> Bool;
}

// 그리고 Role 구현
role Combatable for Player {
    func CanFight(self) -> Bool {
        return self.hp > 0;
    }
}
```

### Intent → Zone

```pergyra
// Intent에 where가 있으면
intent Battle(hero: Player, enemy: Monster)
{
    step attack { where: Arena; }
    step flee   { where: Arena; }
}

// Zone이 자동 유도
zone Arena {
    subject slot challenger: Player;
    subject slot defender: Monster;
}
```

### Intent → Effect/Relation

```pergyra
// Intent에 causes가 있으면
intent CastSpell(caster: Wizard, target: Creature)
{
    step cast
    {
        causes: FireDamage;      // 이 effect가 발생
    }
}

// Effect가 자동 유도
effect FireDamage for bearer: Creature {
    let intensity: Int;
    let duration: Int;
}
```

### Intent → Object/TObject

```pergyra
// Intent의 성공/실패가 투영을 필요로 함
intent GenerateReport(admin: Admin)
{
    step collect_data { ... }
    step format_report  { ... }

    success: report.ready;       // 내부 투영
    failure: report.error;       // 내부 투영
}

// Object/TObject이 자동 유도
object ReportView {
    let title: String;
    let data: Array<String>;
    let status: String;
}

tobject ReportExport {
    let title: String;
    let data: String;   // 직렬화된 형태
    let generated_at: DateTime;
}
```

---

## Intent-First 프로젝트 구조

```
project/
  intents/              ← FIRST: 이 프로그램이 하려는 일
    purchase.pgy        → "사용자가 상품을 구매한다"
    refund.pgy          → "사용자가 상품을 환불한다"
    onboarding.pgy      → "새 사용자가 시스템을 배운다"

  world.pgy             ← SECOND: 실행/신뢰/실패 경계

  zones/                ← THIRD: intent step이 일어나는 문맥
    shop.pgy            → Purchase step들의 무대
    payment.pgy         → Purchase, Refund step들의 무대
    tutorial.pgy        → Onboarding step들의 무대

  subjects/             ← FOURTH: 그 계약을 실제로 수행하는 host
    member.pgy          → Purchase, Refund, Onboarding에 모두 등장
    merchant.pgy        → Purchase, Refund에 등장
    admin.pgy           → Onboarding에 등장

  main.pgy              ← entry point
```

### 의도 목차 (Table of Contents)

`intents/` 폴더만 읽어도 **"이 프로그램이 무엇을 하려는 프로그램인가"** 를 먼저 이해할 수 있어야 한다.

```
기존 구조:
  models/
  services/
  controllers/
  → "무엇으로 구현했는가"가 먼저 보임

Intent-First 구조:
  intents/
    purchase.pgy
    refund.pgy
    onboarding.pgy
  → "무엇을 하려는가"가 먼저 보임
```

---

## Intent 설계 체크리스트

Intent를 선언한 후 다음을 점검하라:

| 질문 | 검사 방법 | 위험 신호 |
|------|-----------|-----------|
| **목적이 명확한가?** | "성공하면 무엇이 달라지는가?" 한 문장 답 | "데이터를 처리한다"류의 모호한 답 |
| **경계가 적당한가?** | 한 현실 목적과 fact 귀속이 한 binder로 닫히는지 확인 | step 수만으로 intent 여부나 분할을 결정 |
| **실패 지점이 보이는가?** | full rollback의 effectful step마다 compensate 또는 irreversible 확인 | 되돌릴 수 없는 효과를 암묵적으로 숨김 |
| **권한이 명확한가?** | 모든 step에 authorized by 또는 requires | 권한 주체 불명확 |
| **Subject가 유도되는가?** | intent에서 등장하는 who/involves로 subject 목록 작성 | intent와 무관한 subject 존재 |
| **Zone이 유도되는가?** | where로 참조하는 zone이 실제로 존재 | zone이 namespace 정도로 전락 |
| **Ability가 유도되는가?** | requires로 참조하는 ability가 실제로 구현됨 | ability가 interface 정도로 전락 |
| **C/LLVM parity가 되는가?** | `pgy --run` + `pgy --run --llvm` 동일 결과 | 백엔드별 다른 동작 |

---

## Intent의 역산 패턴 (Backward Derivation)

Intent를 선언하면 설계자는 필요한 축을 역산한다. 아래의 자동 TODO 진단은
목표 개발자 경험을 설명한다. 실제 구현으로 주장하려면 해당 진단의 executable
gate와 owner registry 근거가 있어야 한다.

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

이 선언에서 다음 설계 체크리스트를 역산할 수 있다. 컴파일러가 같은 진단을
자동으로 생성하는 것은 구현 목표다.

```
error: subject 'Member' not found       → subjects/member.pgy 생성
error: subject 'Merchant' not found     → subjects/merchant.pgy 생성
error: zone 'ProductView' not found     → zones/shop.pgy에 ProductView 추가
error: zone 'PaymentZone' not found     → zones/payment.pgy에 PaymentZone 추가
error: ability 'Purchasable' not found  → ability Purchasable 생성
error: effect 'PaymentEffect' not found → effect PaymentEffect 생성
```

이것은 단순한 오류가 아니라 **아직 만들어야 할 조각 목록**이다.

```
기존 OOP:
  class 하나를 만들었다고 해서
  다음에 무엇을 만들어야 하는지는 바로 드러나지 않는다.

Intent-First:
  intent 하나를 쓰면
  필요한 world, zone, subject, ability, effect를 한 목적에서 추적할 수 있다.
```

---

## Intent vs Function — 언제 무엇을 쓸 것인가

### Intent가 적합한 경우

| 특징 | 예시 |
|------|------|
| **여러 주체가 관여** | 구매자 + 판매자 + 결제대행사 |
| **경계를 넘나듦** | ShopWorld → PaymentWorld → ShippingWorld |
| **실패 시 보상 필요** | 환불, 롤백, 취소 |
| **권한/자격 검증** | 관리자 승인, 운영자 자격 |
| **상태 전이 추적** | 주문 → 결제 → 배송 → 완료 |

### Function이 적합한 경우

| 특징 | 예시 |
|------|------|
| **순수 계산** | `CalculateTax(amount, rate)` |
| **데이터 변환** | `FormatDate(date, format)` |
| **알고리즘** | `SortArray(arr)`, `BinarySearch(arr, key)` |
| **유틸리티** | `StringSplit(s, delim)`, `Max(a, b)` |

### 판단 기준

한 현실 목적의 participant, coordination, authority, effect, boundary,
compensation, trace 의무를 하나의 source-level binder에 귀속해야 하면
`intent`가 적합하다. 순수 계산이나 값 변환 책임이면 `func`가 적합하다.
보상 유무, action 수, step 수는 어느 쪽도 단독 판정 기준이 아니다.

---

## Intent의 계층 구조 — 대목차와 소목차

Intent는 중첩 가능하다. 대목차 안에 소목차가 있는 것과 같다.

```pergyra
// 소목차 intent들
intent BrowseCatalog(buyer: Member)
{
    step search  { where: SearchZone; who: buyer; }
    step filter  { where: SearchZone; who: buyer; }
    success: buyer.cart.size > 0;
}

intent ProcessPayment(buyer: Member)
{
    step verify  { where: PaymentZone; requires: Payable; }
    step charge  { where: PaymentZone; authorized by: buyer; }
    success: buyer.payment_done;
}

intent ArrangeShipping(buyer: Member)
{
    step address  { where: ShippingZone; who: buyer; }
    step dispatch { where: ShippingZone; who: buyer; }
    success: buyer.shipping_arranged;
}

// 대목차 — 하위 intent들을 오케스트레이션
intent CompletePurchase(buyer: Member)
{
    exclusive;
    who: buyer;

    step browse
    {
        intent: BrowseCatalog(buyer);
        post: buyer.cart.size > 0;
    }

    step pay
    {
        intent: ProcessPayment(buyer);
        post: buyer.payment_done;
    }

    step ship
    {
        intent: ArrangeShipping(buyer);
        post: buyer.shipping_arranged;
    }

    success: buyer.order_complete;
    failure: rollback;
}
```

### 규칙

1. `step` 안에서 `intent:` 로 하위 intent를 호출할 수 있다
2. 하위 intent가 실패하면 상위 step도 실패한다
3. 상위 intent의 `compensate`는 하위 intent의 `compensate`를 역순으로 호출한다
4. 하위 intent는 독립적으로도 호출 가능하다 (소목차만 단독 실행)
5. 깊이 제한은 없지만, 3단 이상 중첩은 설계 냄새

---

## Intent-First의 장점

### 1. 현실과 코드의 거리가 짧아진다

대부분의 언어는 현실의 목적을 함수, 클래스, DTO, 서비스, 컨트롤러로 흩어놓고 나중에 사람이 다시 조립해야 한다.

Intent-First는 애초에 "이 시스템이 하려는 일"을 먼저 표면에 올린다. 코드가 구현 이전에 **설명 가능한 구조**가 된다.

### 2. 복잡한 워크플로우에 강해진다

현실의 복잡한 시스템은 단순 계산보다 다음이 중요하다:
- 누가 시작하는가
- 어떤 권한이 필요한가
- 어디서 실패하는가
- 무엇이 보상되어야 하는가
- 어떤 상태 변화가 허용되는가

Intent-First는 이런 문제를 **컴파일타임에 검증**한다.

### 3. 시스템의 "왜"가 안 사라진다

구현을 오래 하다 보면 "왜 이 코드가 존재하는가"가 사라진다.

Intent가 구조의 중심이면 핵심 유스케이스의 "왜"가 언어 표면에 남는다. 장기 유지보수에서 꽤 큰 힘이다.

---

## Intent-First의 단점과 대처

### 1. 작은 문제에도 의미론이 무거워질 수 있다

**문제:** 간단한 유틸 코드, 알고리즘, 데이터 변환에도 Intent 구조가 과할 수 있다.

**대처:** 순수 계산은 Function으로 둔다. Intent는 행위와 경계가 있을 때만 쓴다.

```pergyra
// Function — 가벼운 계산
func CalculateTax(amount: Int, rate: Float) -> Float
{
    return amount * rate;
}

// Intent — 행위와 경계
intent ProcessRefund(buyer: Member, order: Order)
{
    step return_item   { ... }
    step refund_payment { ... }
    compensate: ...
}
```

### 2. 순수 함수 조합에는 부자연스러울 수 있다

**문제:** Intent는 행위와 경계에 강하지만, 순수 함수 조합이나 자료구조 중심 abstraction에는 최적이 아닐 수 있다.

**대처:** Pergyra는 intent-first이지 intent-only가 아니다. Function, Class, Struct는 여전히 유용하다.

### 3. 설계자의 해석 부담이 크다

**문제:** Intent는 자동으로 주어지지 않는다. 설계자가 현실을 해석해 "무엇이 한 intent인가"를 정해야 한다.

**대처:** 4가지 질문(Purpose, Boundary, Failure, Authority)을 사용하면 체계적으로 접근 가능하다.

---

## Intent-First vs DDD — 결과적 유사성, 근본적 차이

Pergyra를 보면 "이거 DDD 아닌가?"라는 생각이 들 수 있다.
표면적으로는 유사해 보이지만, 동기가 다르다.

### DDD에서 시작한 경우

```
DDD:
  "소프트웨어 복잡도를 관리하려면 도메인 모델을 잘 만들어야 한다"
  → bounded context, aggregate, entity, value object
  → 도메인 전문가와 개발자의 소통 언어 (ubiquitous language)
  → 결과: 코드 구조가 도메인 개념을 반영
```

### Intent-First에서 시작한 경우

```
Pergyra:
  "프로그램의 핵심은 현실의 의도를 실행 단위로 닫는 것이다"
  → intent가 1차, 나머지는 역산으로 유도
  → 결과: 코드 구조가 도메인 개념을 반영
```

**같은 결과가 다른 원인에서 나왔다.**

### 핵심 차이

| 차원 | DDD | Pergyra Intent-First |
|------|-----|---------------------|
| **출발점** | 도메인 모델링 | 의도(Intent) 닫기 |
| **1차 요소** | Aggregate / Entity | Intent |
| **검증** | 사람이 코드 리뷰로 | 컴파일러가 정적으로 |
| **실패 처리** | 명시적 아님 | Intent 수준에서 compensate/rollback |
| **경계** | Bounded Context (개념적) | Zone/World (실행적) |
| **도구** | 패턴, 원칙, 소통 | 언어 구문, 타입 시스템 |

### 요약

```
DDD를 흉내 낸 것이 아니라
intent를 최상위 설계 축으로 잡았고
그 결과 도메인 중심 구조가 자연히 따라 나온 것

즉 핵심은 DDD가 아니라 intent-first ontology다.
DDD스러움은 결과이고
원인은 현실의 복잡계를 목적 단위로 닫아야 한다는 믿음이다.
```

---

## Intent의 3가지 크기 — Macro / Meso / Micro

Intent는 크기에 따라 역할이 다르다.

### Macro Intent — 시스템 전체의 목적

```
시스템 수준에서 "이 프로그램이 무엇을 하려는가"
→ 1-3개 정도가 적정
→ 예: CompletePurchase, OnboardUser, RunCampaign
```

```pergyra
intent CompletePurchase(buyer: Member, cart: Cart)
{
    // 전체 구매 플로우를 오케스트레이션
    step browse    { intent: BrowseCatalog(buyer); }
    step pay       { intent: ProcessPayment(buyer); }
    step ship      { intent: ArrangeShipping(buyer); }
    
    success: buyer.order_complete;
}
```

### Meso Intent — 서브시스템의 목적

```
Macro intent의 하위 단계, 또는 독립 실행 가능한 단위
→ 5-15개 정도
→ 예: BrowseCatalog, ProcessPayment, ArrangeShipping
```

```pergyra
intent ProcessPayment(buyer: Member)
{
    step verify  { where: PaymentZone; requires: Payable; }
    step charge  { where: PaymentZone; authorized by: buyer; }
    step confirm { where: PaymentZone; }
    
    success: buyer.payment_done;
}
```

### Micro Intent — 단일 행위의 목적

```
Meso intent 내부의 step, 또는 매우 작은 독립 행위
→ 10-30개 정도
→ 예: VerifyCard, ChargeAmount, SendReceipt
```

```pergyra
intent VerifyCard(buyer: Member)
{
    step check_funds
    {
        where: PaymentZone;
        on: PaymentGateway.VerifyFunds(buyer.card);
        post: buyer.funds_verified;
    }
    success: buyer.funds_sufficient;
}
```

### 크기별 사용 패턴

```
Macro Intent (1-3개)
  └── Meso Intent (5-15개)
        └── Micro Intent (10-30개)
              └── Function (계산/변환/유틸)
```

**규칙:**
- Macro는 Meso를 오케스트레이션만 한다 (직접 행동 안 함)
- Meso는 독립 실행 가능해야 한다 (재사용성)
- Micro는 보상(compensate)이 필요할 때만 Intent로, 아니면 Function으로

---

## Intent 설계 안티패턴

### 1. Intent가 함수가 된 경우

```pergyra
// 나쁜 예 — intent가 단순 함수 호출
intent CalculateTotal(cart: Cart) -> Float
{
    step sum
    {
        on: cart.CalculateSum();
    }
    success: true;
}

// 좋은 예 — function으로
func CalculateTotal(cart: Cart) -> Float
{
    return cart.SumItems() + cart.CalculateTax();
}
```

**검사:** `compensate`가 비어있거나 `success: true`만 있으면 함수일 가능성 높음.

### 2. Intent가 main 함수가 된 경우

```pergyra
// 나쁜 예 — intent가 너무 큼
intent RunEntireSystem()
{
    step init        { ... }
    step run_game    { ... }   // 50줄
    step cleanup     { ... }
    success: true;
}

// 좋은 예 — macro/meso로 분리
intent StartGameSession(player: Player)
{
    step load_profile  { ... }
    step init_world    { ... }
    step spawn_player  { ... }
    success: player.in_world;
}

intent RunGameLoop(world: GameWorld)
{
    step process_input { ... }
    step update_state  { ... }
    step render        { ... }
    success: world.running;
}
```

**검사:** step이 10개 이상이거나 step body가 20줄 이상이면 쪼갤 것.

### 3. Intent가 상태 머신이 된 경우

```pergyra
// 나쁜 예 — intent가 상태 전이만 관리
intent ManageTrafficLight(light: TrafficLight)
{
    step red_to_green    { on: light.SetGreen(); }
    step green_to_yellow { on: light.SetYellow(); }
    step yellow_to_red   { on: light.SetRed(); }
    success: true;  // 무한 루프?
}

// 좋은 예 — intent가 목적을 가짐
intent AllowCrosswalk(pedestrian: Pedestrian, light: TrafficLight)
{
    step stop_traffic
    {
        on: light.SetRed();
        post: light.status == Red;
    }
    step allow_walk
    {
        where: Crosswalk;
        who: pedestrian;
        post: pedestrian.crossed;
    }
    step resume_traffic
    {
        on: light.SetGreen();
        compensate: light.SetRed();  // 보행자 안전
    }
    success: pedestrian.crossed && light.status == Green;
}
```

**검사:** `success` 조건이 모호하거나 intent에 "누가" 없으면 상태 머신일 가능성.

### 4. Intent가 이벤트 핸들러가 된 경우

```pergyra
// 나쁜 예 — intent가 이벤트 반응형
intent OnButtonClick(button: Button)
{
    step handle
    {
        on: button.ProcessClick();
    }
    success: true;
}

// 좋은 예 — intent가 목적 기반
intent SubmitOrder(buyer: Member, cart: Cart)
{
    step validate  { ... }
    step process   { ... }
    compensate: ...
    success: buyer.order_submitted;
}
```

**검사:** Intent 이름이 `OnXxx`, `HandleXxx`, `WhenXxx`면 이벤트 핸들러일 가능성.

---

## Intent에서 Function으로 내려가는 경계

```
질문: "이것을 Intent로 해야 하나, Function으로 해야 하나?"
```

### 결정 트리

```
1. 실패 시 보상(compensate)이 필요한가?
   YES → Intent
   NO  → 2번으로
   
2. 여러 주체(subject)가 관여하는가?
   YES → Intent
   NO  → 3번으로
   
3. 경계(zone/world)를 넘나드는가?
   YES → Intent
   NO  → 4번으로
   
4. 권한/자격 검증이 필요한가?
   YES → Intent (또는 Function + ability 체크)
   NO  → Function
```

### 실전 예시

| 작업 | Intent | Function | 이유 |
|------|--------|----------|------|
| 사용자 로그인 | ✅ | | 세션, 권한, 여러 주체 |
| 비밀번호 해싱 | | ✅ | 순수 계산 |
| 주문 처리 | ✅ | | 결제, 배송, 여러 경계 |
| 세금 계산 | | ✅ | 수식 적용 |
| 재고 업데이트 | ✅ | | 트랜잭션, 보상 필요 |
| 문자열 포맷팅 | | ✅ | 데이터 변환 |
| 파일 업로드 | ✅ | | 외부 시스템, 실툴 가능 |
| 배열 정렬 | | ✅ | 알고리즘 |

---

## Intent 설계 리뷰 프로세스

팀에서 Intent를 검토할 때 사용할 체크리스트:

### 1단계: 목적 검토

```
□ "이 intent가 성공하면 무엇이 달라지는가?"에 한 문장으로 답되는가?
□ intent 이름이 목적을 명확히 표현하는가? (동사+명사 권장)
□ success/failure 조건이 구체적인 상태 변화를 나타내는가?
```

### 2단계: 경계 검토

```
□ 한 현실 목적과 fact 귀속이 이 binder 안에서 닫히는가?
□ 각 step이 단일 책임을 가지는가?
□ step 수나 줄 수가 아니라 authority/effect/success/failure 경계가 응집돼 있는가?
```

### 3단계: 실패 검토

```
□ full rollback의 effectful step마다 compensate 또는 irreversible이 명시됐는가?
□ compensate가 유효한 이전 상태로 복구하는가?
□ failure 조건이 모든 실패 케이스를 커버하는가?
```

### 4단계: 권한 검토

```
□ 모든 step에 who가 정의되었는가?
□ requires/authorized by가 필요한 곳에 정의되었는가?
□ 권한 주체가 실제 존재하는 subject인가?
```

### 5단계: 유도 검토

```
□ intent에서 필요한 subject가 모두 유도되는가?
□ intent에서 필요한 zone이 모두 유도되는가?
□ intent에서 필요한 ability/effect가 모두 유도되는가?
□ intent와 무관한 subject/zone이 프로젝트에 없는가?
```

---

## 요약

```
Pergyra는:
  intent를 최상위 설계 축으로 잡아
  현실의 복잡한 행위를 목적 단위로 닫고
  나머지 구조를 유도하는 언어

그 근거는:
  intent가 정해지면 subject, zone, ability, effect가 자동 유도됨
  intent가 틀리면 전부 흔들림
  intent만 정확하면 나머지는 붙이기 쉬움

그 방법은:
  1. "누가 무엇을 이루려 하는가?" (Purpose)
  2. "어디까지가 한 행위인가?" (Boundary)
  3. "실패를 어디서 끊을 것인가?" (Failure Point)
  4. "누가 책임을 질 것인가?" (Authority)

그 결과는:
  코드가 구현 이전에 설명 가능한 구조가 됨
  시스템의 "왜"가 안 사라짐
  복잡한 워크플로우를 컴파일타임에 검증

그 주의점은:
  intent-only가 아님 — 순수 계산은 function으로
  intent 크기가 중요함 — macro/meso/micro 구분
  설계자의 해석 부담이 큼 — 4질문으로 체계화
```


---

## Intent 설계 가이드 — 실전 예제

### 예제 1: 게임 퀘스트

```pergyra
// 1. Purpose: "플레이어가 퀘스트를 완료하고 보상을 받는다"
// 2. Boundary: 퀘스트 시작 → 목표 달성 → 보상 수령 (4단계)
// 3. Failure: 각 단계 실패 시 보상 정의
// 4. Authority: 플레이어가 자신의 행동 승인

intent CompleteQuest(player: Player, quest: Quest)
{
    exclusive;
    who: player;

    step accept_quest
    {
        where: QuestBoard;
        on: player.AcceptQuest(quest);
        pre: player.level >= quest.min_level;
        compensate: player.DeclineQuest(quest);
    }

    step achieve_goal
    {
        where: QuestZone;
        requires: Questable;
        on: player.CompleteObjective(quest);
        post: quest.is_complete;
        compensate: player.AbandonQuest(quest);
    }

    step report_completion
    {
        where: QuestBoard;
        on: player.ReportToNPC(quest.npc);
        pre: quest.is_complete;
    }

    step receive_reward
    {
        where: QuestBoard;
        on: player.ClaimReward(quest.reward);
        post: player.HasItem(quest.reward);
    }

    success: quest.complete && player.HasItem(quest.reward);
    failure: quest.abandoned;
}
```

### 예제 2: 쇼핑몰 환불

```pergyra
// 1. Purpose: "구매자가 상품을 환불하고 돈을 돌려받는다"
// 2. Boundary: 환불 요청 → 검증 → 환불 처리 → 완료 (4단계)
// 3. Failure: 각 단계 실패 시 원래 상태로 복구
// 4. Authority: 구매자가 환불 승인, 관리자가 고가 환불 승인

intent RefundOrder(buyer: Member, order: Order)
{
    who: buyer;

    step request_refund
    {
        where: RefundCenter;
        on: buyer.SubmitRefundRequest(order);
        pre: order.status == Delivered;
        pre: order.days_since_delivery <= 30;
    }

    step verify_condition
    {
        where: RefundCenter;
        requires: Refundable;
        on: RefundInspector.Verify(order);
        post: order.return_condition == Approved;
    }

    step process_refund
    {
        where: PaymentZone;
        authorized by: buyer;
        on: PaymentGateway.Refund(order.payment_id);
        compensate: PaymentGateway.ReverseRefund(order.payment_id);
    }

    step confirm_refund
    {
        where: RefundCenter;
        on: order.MarkRefunded();
        post: order.status == Refunded;
    }

    success: order.status == Refunded && buyer.refund_received;
    failure: order.refund_rejected;
}
```

### 예제 3: IoT 장치 제어 (A2M)

```pergyra
// 1. Purpose: "AI 에이전트가 공작기계를 가동하고 생산 목표를 달성한다"
// 2. Boundary: 준비 → 가동 → 모니터링 → 완료 (4단계)
// 3. Failure: 각 단계 실패 시 긴급 정지
// 4. Authority: AI가 행동, 인간이 승인

intent StartProduction(agent: AIAgent)
{
    exclusive;
    who: agent;

    step prepare_machine
    {
        where: FactoryFloor;
        requires: MachineOperator;
        authorized by: supervisor;          // 인간 승인 필요
        on: agent.InitMachine();
        compensate: agent.EmergencyStop();
        pre: machine.status == Ready;
    }

    step run_cycle
    {
        where: FactoryFloor;
        on: agent.RunCycle();
        guard: machine.temperature < 100;   // 실시간 안전 조건
        post: machine.output > 0;
        compensate: agent.EmergencyStop();
    }

    step monitor_output
    {
        where: FactoryFloor;
        on: agent.CheckQuality();
        post: machine.quality_score >= threshold;
    }

    step complete_batch
    {
        where: FactoryFloor;
        on: machine.FinishBatch();
        post: machine.produced >= target;
    }

    success: machine.produced >= target && machine.quality_score >= threshold;
    failure: machine.emergency_stopped;
}
```
