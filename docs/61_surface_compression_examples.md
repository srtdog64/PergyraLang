# Surface Compression Examples

마지막 업데이트: 2026-04-11

이 문서는 Pergyra의 authoring pain point를 줄이기 위해,
이미 구현된 압축과 아직 설계 단계인 압축을 예제로 나눠 보여준다.

관련 문서:

- [58_keyword_authorship_pain_points.md](/mnt/e/PergyraLang/docs/58_keyword_authorship_pain_points.md)
- [59_authoring_surface_compression_plan.md](/mnt/e/PergyraLang/docs/59_authoring_surface_compression_plan.md)
- [60_zone_context_and_transfer_inference.md](/mnt/e/PergyraLang/docs/60_zone_context_and_transfer_inference.md)

## 1. 현재 이미 구현된 압축

### 1.1 action contract inference / transfer inference

긴 표면:

```pgy
action Promote(self) -> Void
    requires Payable
    within PaymentZone
    authorized by self
    causes Charged
{
}

intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {
    step Promote {
        where: PaymentZone;
        using: payment;
        who: buyer;
        requires: Payable;
        authorized by: buyer;
        causes: Charged;
        on: buyer.Promote();
        expect: true;
    }
}
```

목표 축약:

```pgy
action Promote(self) -> Void
    requires Payable
    within PaymentZone
    authorized by self
    causes Charged
{
}

intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {
    step Promote {
        transfer: cart -> payment;
        who: buyer;
        on: buyer.Promote();
        expect: true;
    }
}
```

현재 구현 상태:

- `where / requires / causes / authorized by`는 matching action contract에서 기본 추론된다
- `transfer target -> using/where inference`도 구현돼 있다

설명:

- `transfer: cart -> payment;`

가 있으면:

- `using: payment;`
- `where: PaymentZone;`

가 기본 추론된다.

실제 예제 파일:

- [action_contract_inference_minimal.pgy](/mnt/e/PergyraLang/examples/action_contract_inference_minimal.pgy)
- [intent_inference_minimal.pgy](/mnt/e/PergyraLang/examples/intent_inference_minimal.pgy)
- [transfer_move_minimal.pgy](/mnt/e/PergyraLang/examples/transfer_move_minimal.pgy)
- [transfer_move_typed_minimal.pgy](/mnt/e/PergyraLang/examples/transfer_move_typed_minimal.pgy)
- [function_clause_order_minimal.pgy](/mnt/e/PergyraLang/examples/function_clause_order_minimal.pgy)
- [generic_ability_requires_minimal.pgy](/mnt/e/PergyraLang/examples/generic_ability_requires_minimal.pgy)

주의:

- [intent_inference_minimal.pgy](/mnt/e/PergyraLang/examples/intent_inference_minimal.pgy)는
  현재 실제로 되는 최소 surface만 보여준다
- [action_contract_inference_minimal.pgy](/mnt/e/PergyraLang/examples/action_contract_inference_minimal.pgy)는
  action contract 기반 추론을 보여준다
- [transfer_move_minimal.pgy](/mnt/e/PergyraLang/examples/transfer_move_minimal.pgy)는
  `move <from> to <to>;`와 transfer inference를 함께 보여준다
- [transfer_move_typed_minimal.pgy](/mnt/e/PergyraLang/examples/transfer_move_typed_minimal.pgy)는
  `move <from> to <ZoneType>;` target-zone 추론을 보여준다
- 즉 지금 smoke에 박힌 것은 `transfer -> using/where` 추론이다

### 1.2 clause 순서 자유화

이전 pain point:

- `where`
- `with effects`
- `requires`
- `within`
- `causes`
- `authorized by`

를 고정 순서로 써야 한다고 느껴지기 쉬웠다.

현재 상태:

- parser는 function/action clause를 table-driven으로 처리한다
- clause 순서는 고정이 아니다
- duplicate clause는 explicit diagnostic이 난다

실제 예제 파일:

- [function_clause_order_minimal.pgy](/mnt/e/PergyraLang/examples/function_clause_order_minimal.pgy)

### 1.3 generic ability requires

현재 상태:

- `ability<T>` 선언 가능
- `impl ability Ability<T>` 가능
- `action requires Ability<T>` 가능
- `zone authority requires Ability<T>` 가능

실제 예제 파일:

- [generic_ability_requires_minimal.pgy](/mnt/e/PergyraLang/examples/generic_ability_requires_minimal.pgy)

즉 지금은 "어떤 순서로 써야 하지?"보다
"무엇을 생략/추론할 수 있지?"가 더 큰 문제다.

## 2. 다음으로 줄여야 할 표면

### 2.1 lexical zone context

현재 반복:

```pgy
action AddEvent(self, event: Event)
    within CalendarZone
{
}

action UpdateEvent(self, event: Event)
    within CalendarZone
{
}
```

현재 구현:

```pgy
within CalendarZone {
    action AddEvent(self, event: Event) { }
    action UpdateEvent(self, event: Event) { }
}
```

핵심:

- inheritance가 아니라 zone context inference다
- 현재는 top-level block 1차 구현이다

실제 예제 파일:

- [zone_context_minimal.pgy](/mnt/e/PergyraLang/examples/zone_context_minimal.pgy)

### 2.2 relation/effect explicit alias

현재 구현:

```pgy
using self.route as route;
using self.seal as seal;

route.MarkReady();
seal.Validate();
```

실제 적용 예:

- [loading.pgy](/mnt/e/PergyraLang/examples/logistics_intent_probe/zones/loading.pgy)
- [delivery.pgy](/mnt/e/PergyraLang/examples/logistics_intent_probe/zones/delivery.pgy)

핵심:

- implicit magic보다 explicit alias가 먼저다
- 이름 충돌 규칙을 단순하게 유지할 수 있다

### 2.3 transfer short surface

이전 긴 표면:

```pgy
step Deliver {
    where: DeliveryZone;
    using: delivery;
    transfer: loading -> delivery;
    who: courier;
}
```

현재 구현:

```pgy
move loading to delivery;
```

핵심:

- `move <from-alias> to <to-alias>;`는
  `transfer: <from-alias> -> <to-alias>;`로 낮아진다
- 기존 `transfer target -> using/where inference`도 그대로 적용된다
- `move <from-alias> to <ZoneType>;`는
  intent participant 중 해당 zone type이 유일하면 그 binding alias로 정규화된다

실제 예제 파일:

- [transfer_move_minimal.pgy](/mnt/e/PergyraLang/examples/transfer_move_minimal.pgy)
- [transfer_move_typed_minimal.pgy](/mnt/e/PergyraLang/examples/transfer_move_typed_minimal.pgy)

아직 설계 단계:

- `let delivered = cargo.transfer(to: DeliveryZone, as: DeliveredCargo);`

## 3. 가장 큰 보일러플레이트 묶음

현재 체감상 가장 큰 묶음은 이 순서다.

1. intent step boundary cluster
- `who`
- `where`
- `using`
- `transfer`
- `requires`
- `authorized by`
- `causes`

2. action clause cluster
- `requires`
- `within`
- `authorized by`
- `causes`

3. authority / ability cluster
- `authority ... requires ...`
- `action ... requires ...`
- `step requires: ...`

즉 지금은 키워드를 줄이는 단계가 아니라,
반복되는 cluster를 inference와 shorter surface로 압축하는 단계다.

## 4. 구현/설계 경계

### 이미 구현됨

- action contract inference -> intent step
- transfer target -> using inference
- transfer target -> where inference
- clause order 자유화
- domain-first diagnostics 1차

### 아직 설계 단계

- file-global `zone context`
- `move <value> to <ZoneType>;` 같은 type-directed transfer short surface
- group bind
- subject factory surface
