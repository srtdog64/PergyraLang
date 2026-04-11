# Surface Compression Examples

마지막 업데이트: 2026-04-12

이 문서는 Pergyra의 authoring pain point를 줄이기 위해,
이미 구현된 압축과 아직 설계 단계인 압축을 예제로 나눠 보여준다.

관련 문서:

- [58_keyword_authorship_pain_points.md](/mnt/e/PergyraLang/docs/58_keyword_authorship_pain_points.md)
- [59_authoring_surface_compression_plan.md](/mnt/e/PergyraLang/docs/59_authoring_surface_compression_plan.md)
- [60_zone_context_and_transfer_inference.md](/mnt/e/PergyraLang/docs/60_zone_context_and_transfer_inference.md)
- [65_stable_example_surface_board.md](/mnt/e/PergyraLang/docs/65_stable_example_surface_board.md)

이 문서의 운영 규칙:

- `compile-smoke covered` 예제를 stable source of truth로 본다
- `reference` 예제는 현재 구현과 맞는 real surface지만 아직 smoke source of truth는 아니다
- `design sketch` 예제는 방향 설명용이지 현재 권장 surface가 아니다

즉 이 문서에서 `긴 버전 vs 압축 버전`을 설명할 때는 sketch 예제가 아니라 smoke-covered canonical pair를 우선 사용한다.

## 1. 현재 이미 구현된 압축

### 1.1 canonical pair: action contract inference / transfer inference

우선 봐야 할 stable canonical pair:

- [intent_contract_pair_minimal.pgy](/mnt/e/PergyraLang/examples/intent_contract_pair_minimal.pgy)
- [authority_contract_pair_minimal.pgy](/mnt/e/PergyraLang/examples/authority_contract_pair_minimal.pgy)
- [transfer_contract_pair_minimal.pgy](/mnt/e/PergyraLang/examples/transfer_contract_pair_minimal.pgy)

이 세 파일은 같은 의미를 `긴 버전`과 `압축 버전`으로 나란히 보여 주는 canonical pair다.
이제 smoke source of truth에도 포함되므로, contract compression 설명의 기준 예제로 먼저 링크해야 한다.

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

- `who / where / requires / causes / authorized by`는 matching action contract과 유일 subject participant에서 기본 추론된다
- `transfer target -> using/where inference`도 구현돼 있다
- explicit `using:`이 zone binding이면 `where:`도 거기서 추론된다
- explicit `where:`가 있고 matching zone participant가 유일하면 `using:`도 추론된다

설명:

- `transfer: cart -> payment;`

가 있으면:

- `using: payment;`
- `where: PaymentZone;`

가 기본 추론된다.

stable example source of truth:

- [action_contract_inference_minimal.pgy](/mnt/e/PergyraLang/examples/action_contract_inference_minimal.pgy)
- [intent_inference_minimal.pgy](/mnt/e/PergyraLang/examples/intent_inference_minimal.pgy)
- [transfer_move_minimal.pgy](/mnt/e/PergyraLang/examples/transfer_move_minimal.pgy)
- [transfer_move_typed_minimal.pgy](/mnt/e/PergyraLang/examples/transfer_move_typed_minimal.pgy)
- [surface_compression_maximal.pgy](/mnt/e/PergyraLang/examples/surface_compression_maximal.pgy)
- [function_clause_order_minimal.pgy](/mnt/e/PergyraLang/examples/function_clause_order_minimal.pgy)
- [generic_ability_requires_minimal.pgy](/mnt/e/PergyraLang/examples/generic_ability_requires_minimal.pgy)
- [projection_bind_group_minimal.pgy](/mnt/e/PergyraLang/examples/projection_bind_group_minimal.pgy)
- [projection_refresh_publish_group_minimal.pgy](/mnt/e/PergyraLang/examples/projection_refresh_publish_group_minimal.pgy)
- [six_item_alignment_demo.pgy](/mnt/e/PergyraLang/examples/six_item_alignment_demo.pgy)

stable canonical pair:

- [intent_contract_pair_minimal.pgy](/mnt/e/PergyraLang/examples/intent_contract_pair_minimal.pgy)
- [authority_contract_pair_minimal.pgy](/mnt/e/PergyraLang/examples/authority_contract_pair_minimal.pgy)
- [transfer_contract_pair_minimal.pgy](/mnt/e/PergyraLang/examples/transfer_contract_pair_minimal.pgy)

현재 권장 읽기 순서:

1. canonical pair로 긴 버전과 압축 버전의 의미 동등성을 먼저 본다
2. smoke-covered minimal example으로 현재 stable syntax subset을 확인한다
3. maximal/composite example은 마지막에 본다

주의:

- [intent_inference_minimal.pgy](/mnt/e/PergyraLang/examples/intent_inference_minimal.pgy)는
  현재 실제로 되는 최소 surface만 보여준다
- [action_contract_inference_minimal.pgy](/mnt/e/PergyraLang/examples/action_contract_inference_minimal.pgy)는
  action contract 기반 `who / where / requires / causes / authorized by` 추론을 보여준다
- [transfer_move_minimal.pgy](/mnt/e/PergyraLang/examples/transfer_move_minimal.pgy)는
  `move <from> to <to>;`와 transfer inference를 함께 보여준다
- [transfer_move_typed_minimal.pgy](/mnt/e/PergyraLang/examples/transfer_move_typed_minimal.pgy)는
  `move <from> to <ZoneType>;` target-zone 추론을 보여준다
- [surface_compression_maximal.pgy](/mnt/e/PergyraLang/examples/surface_compression_maximal.pgy)는
  현재 구현된 축소 표면을 한 번에 묶은 최대치 예제다
- [projection_bind_group_minimal.pgy](/mnt/e/PergyraLang/examples/projection_bind_group_minimal.pgy)는
  `bind [view, dto] from source`처럼 projection wiring 여러 줄을 한 줄로 줄이는 group bind 표면을 보여준다
- [projection_refresh_publish_group_minimal.pgy](/mnt/e/PergyraLang/examples/projection_refresh_publish_group_minimal.pgy)는
  `refresh [a, b] from source`와 `publish [x, y] from source` 그룹 표면을 보여준다
- [six_item_alignment_demo.pgy](/mnt/e/PergyraLang/examples/six_item_alignment_demo.pgy)는
  action contract 추론, `where/use` 상호 추론, projection field map, explicit `Clone` world embedding을 함께 보여준다
- diagnostics는 이제 같은 vocabulary를 쓴다:
  - `locally declared ...`
  - `inherited ... from matching action`
  - `inferred ... from transfer target`
- 즉 압축이 실패해도 사용자는 local/inherited/inferred provenance를 같은 용어로 읽을 수 있어야 한다

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
- 같은 파일/블록에서 `within CalendarZone`를 반복해서 쓰는 부담을 줄이는 용도다

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
- `self.` 반복을 무작정 없애는 것이 아니라, 제어 가능한 축약만 허용하는 방향이다

### 2.3 group bind / projection scaffold

현재 구현:

```pgy
zone BattleZone {
    subject slot player: Player
    object slot playerView: PlayerView
    tobject slot snapshot: PlayerDto
    bind [playerView, snapshot] from player
}
```

이 표면은 기존의

```pgy
bind playerView from player
bind snapshot from player
```

를 그대로 확장한다.

핵심:

- 새 projection 의미론을 추가하지 않는다
- parser가 기존 `bind` 두 줄로 펼치기 때문에 semantic/codegen debt가 늘지 않는다
- object/tobject target 혼합도 기존 `bind`의 target-kind inference를 그대로 탄다
- 즉 surface만 줄이고 의미론은 넓히지 않는다

같은 패턴으로 다음도 된다:

```pgy
refresh [playerView, playerCard] from player by player
publish [snapshot, packet] from player by player
```

이 역시 parser가 기존 개별 `refresh` / `publish` statement 여러 개로 펼친다.

### 2.4 projection field map

projection target과 source의 필드명이 완전히 같지 않아도,
지금은 field map으로 연결을 명시할 수 있다.

```pgy
refresh buyerView from buyer by buyer map {
    displayName <- name;
    levelText <- tier;
}

publish buyerPacket from buyer by buyer map {
    snapshotName <- name;
    paidState <- paid;
}
```

핵심:

- `refresh/publish/bind ... map { target <- source; }` 지원
- unknown target field, missing source field, duplicate mapped target field는 semantic error다
- authority-bearing zone에서는 기존과 동일하게 explicit `by` 규칙을 따른다

### 2.5 transfer short surface

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

### 2.6 explicit `Clone(...)` for world embedding

현재 world constructor에 기존 zone binding을 바로 넘기면
참조 공유가 아니라 값 복사에 가깝게 동작한다.

문제 표면:

```pgy
let world = PacketWorld(zone);
```

권장 표면:

```pgy
let world = PacketWorld(Clone(zone));
```

또는:

```pgy
let world = PacketWorld(ConnectionZone(...));
```

핵심:

- direct binding world-embedding은 compiler warning 대상이다
- `Clone(...)`은 복사 의도를 surface에서 명시한다
- world embedding은 hidden alias/reference semantics를 만들지 않는다

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
- explicit `using` -> `where` inference
- explicit `where` -> `using` inference
- clause order 자유화
- `refresh/publish/bind ... map { ... }`
- domain-first diagnostics 1차

### 아직 설계 단계

- file-global `zone context`
- `move <value> to <ZoneType>;` 같은 type-directed transfer short surface
- group bind
- subject factory surface
