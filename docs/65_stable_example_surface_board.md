# Stable Example Surface Board

마지막 업데이트: 2026-04-12

이 문서는 예제를 세 가지로 분리한다.

- `compile-smoke covered`: 현재 구현에서 직접 돌고, surface trust를 줘도 되는 예제
- `design sketch`: 미래 표면/아이디어를 보여 주지만 현재 stable syntax reference로 쓰면 안 되는 예제
- `unclassified`: 아직 이 보드에 올리지 않은 예제

핵심 원칙:

- 예제는 문서보다 먼저 사용자에게 "무엇이 진짜 되는가"를 가르친다
- 따라서 sketch 예제와 stable 예제를 같은 톤으로 두면 surface trust가 깨진다
- 현재는 `tests/example_contract_smoke.sh`를 stable example source of truth로 본다

## 1. Stable examples (`compile-smoke covered`)

다음 예제는 현재 smoke에서 직접 밟히는 예제들이다.

| 예제 | 상태 | 비고 |
| --- | --- | --- |
| `examples/beta_resource_slots.pgy` | stable | resource/slot baseline |
| `examples/beta_modules_generics.pgy` | stable | module + generics baseline |
| `examples/battle_simulator/` | stable | larger simulation corpus |
| `examples/biome_simulator/` | stable | world/zone-heavy simulation corpus |
| `examples/fsm_factory/` | stable | FSM baseline |
| `examples/raid_graph_fsm/` | stable | graph + FSM corpus |
| `examples/campaign_graph_fsm/` | stable | graph + FSM corpus |
| `examples/dnd_tavern_campaign/` | stable | large example corpus |
| `examples/shopping_mall_checkout_refund/` | stable | page/http/storage adapter path |
| `examples/logistics_intent_probe/` | stable | DIR/RIR/MIR probe |
| `examples/composite_intent_orchestration/` | stable | nested/orchestrated intent path |
| `examples/resource_scheduler_async_probe/` | stable | async/parallel/resource probe |
| `examples/spray_device_probe/` | stable | device/runtime probe |
| `examples/calendar_working/` | stable | working calendar subset |
| `examples/subject_object_tobject/` | stable | nominal/projection baseline |
| `examples/adapter_policy_stack/` | stable | adapter/policy layering |
| `examples/pattern_library_basics/` | stable | basic pattern surface |
| `examples/function_clause_order_minimal/` | stable | clause reordering subset |
| `examples/generic_ability_requires_minimal/` | stable | generic ability baseline |
| `examples/action_contract_inference_minimal/` | stable | action contract inference |
| `examples/intent_inference_minimal/` | stable | intent inference subset |
| `examples/intent_contract_pair_minimal.pgy` | stable | verbose/compressed action-contract pair |
| `examples/authority_contract_pair_minimal.pgy` | stable | verbose/compressed authority-contract pair |
| `examples/transfer_contract_pair_minimal.pgy` | stable | verbose/compressed transfer-contract pair |
| `examples/transfer_move_minimal/` | stable | transfer shorthand subset |
| `examples/transfer_move_typed_minimal/` | stable | typed transfer subset |
| `examples/surface_compression_maximal/` | stable | compressed authoring surface |
| `examples/zone_context_minimal/` | stable | lexical zone context subset |
| `examples/projection_bind_group_minimal/` | stable | bind-group subset |
| `examples/projection_refresh_publish_group_minimal/` | stable | refresh/publish-group subset |
| `examples/six_item_alignment_demo/` | stable | alignment/authoring demo |
| `examples/ownership_forwarding_probe/` | stable | `own/ref` anchored-slot boundary subset |
| `examples/order_analytics/` | stable | compile-smoke covered analytics example |
| `examples/beta_qubit_experimental.pgy` | stable | quantum surface probe within current partial scope |

이 목록의 의미:

- "언어 전체가 완성됐다"는 뜻은 아니다
- "이 예제가 사용하는 표면은 현재 회귀 테스트가 직접 밟는다"는 뜻이다

## 2. Design sketch examples

다음 예제는 현재 구현보다 앞선 표면을 포함한 설계 스케치다.

| 예제 | 상태 | 주의 |
| --- | --- | --- |
| `examples/party_system_demo.pgy` | design sketch | stable syntax reference로 사용 금지 |
| `examples/world_roster_city.pgy` | design sketch | stable syntax reference로 사용 금지 |

이 예제들은 다음 같은 미래 표면을 보여 줄 수 있다.

- Rust-like receiver syntax (`&self`, `&mut self`)
- richer collection combinator surface (`Map`, `Filter`, `MaxBy`, `MapValues`)
- broader `party/world/roster/context` orchestration
- scheduler-flavored `parallel on (...)`, `every (...)`

즉 이 파일들은 "방향"을 보여 주는 문서형 예제이지,
현재 문법/semantic/codegen 계약의 기준 예제가 아니다.

## 3. Reference examples (real surface, not smoke-covered yet)

다음 예제는 현재 구현과 맞는 real surface를 설명하지만,
아직 stable smoke source of truth에는 포함되지 않았다.

| 예제 | 상태 | 주의 |
| --- | --- | --- |
현재 reference tier에 남는 예제는 없다.

방금 stable로 승격된 canonical pair는 다음 셋이다.

- `examples/intent_contract_pair_minimal.pgy`
- `examples/authority_contract_pair_minimal.pgy`
- `examples/transfer_contract_pair_minimal.pgy`

즉 `긴 버전 vs 압축 버전`을 보여 줄 때는 sketch 예제가 아니라 이 stable pair를 먼저 사용한다.

## 4. 운영 규칙

1. 새 stable 예제를 추가할 때
- `tests/example_contract_smoke.sh` 또는 동급 smoke에서 직접 밟혀야 한다
- 가능하면 파일 헤더에 `compile-smoke covered example`를 명시한다

2. sketch 예제를 추가할 때
- 파일 헤더에 반드시 `design sketch / not compile-smoke covered`를 명시한다
- stable syntax reference처럼 읽히는 주석을 금지한다

3. 문서에서 예제를 링크할 때
- stable surface 설명에는 stable example만 우선 링크한다
- sketch example은 "future surface" 또는 "design sketch" 문맥에서만 링크한다

4. reference example을 추가할 때
- 현재 구현과 맞는 real surface여야 한다
- 파일 헤더에 `not compile-smoke covered yet`를 명시한다
- stable source of truth와 혼동되지 않게 별도 레벨로 둔다

## 5. 다음 정리 대상

- 아직 self-label이 없는 예제를 stable/sketch/unclassified로 전수 분류
- `README`와 핵심 모델 문서에서 stable example 우선 링크
- sketch example header wording 표준화
- contract compression 본문 문서에서 canonical pair 우선 링크
