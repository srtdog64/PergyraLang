# PergyraLang 작성 비교 예제 보드

마지막 업데이트: 2026-04-14

## 목적

이 문서는 같은 의미를 `명시형(explicit)`과 `압축형(compressed)`으로 각각 어떻게 쓰는지 비교하는 source-of-truth 보드다.

원칙:
- 새 문법 실험이 아니라 현재 `stable / smoke-covered` surface만 사용한다
- 각 pair는 같은 의미를 두 표면으로 보여준다
- pain point 평가는 철학이 아니라 실제 작성량, 중복 계약, provenance 가독성 기준으로 한다

## Pair 1. Intent contract 반복 vs action contract 재사용

- explicit: `examples/intent_contract_pair_minimal.pgy`
- compressed: 같은 파일 안의 `PatrolCompressed`
- 핵심 비교:
  - explicit는 `who / where / using / requires / authorized by / causes`를 step에 다시 쓴다
  - compressed는 `using + on`만 남기고 action contract pack을 재사용한다
- 관찰 포인트:
  - intent authoring의 중복 선언 피로
  - diagnostics가 `locally declared`와 `reused from matching action contract`를 구분해서 보여주는지

## Pair 2. Zone authority 반복 vs authority contract 재사용

- explicit: `examples/authority_contract_pair_minimal.pgy`
- compressed: 같은 파일 안의 `PatrolCompressed`
- 핵심 비교:
  - explicit는 `requires / authorized by`를 step에 다시 쓴다
  - compressed는 zone authority + action contract를 그대로 사용한다
- 관찰 포인트:
  - authority-bearing zone에서 step boilerplate가 얼마나 줄어드는지
  - 실패 시 어떤 authority contract가 어디서 왔는지 provenance가 충분한지

## Pair 3. Transfer target 명시 vs transfer-derived zone/using

- explicit: `examples/transfer_contract_pair_minimal.pgy`
- compressed: 같은 파일 안의 `MoveCompressed`
- 핵심 비교:
  - explicit는 `where: DeliveryZone; using: deliver; transfer: load -> deliver;`
  - compressed는 `transfer: load -> deliver;`만 쓰고 `where/using`을 transfer target에서 derive한다
- 관찰 포인트:
  - cross-zone orchestration에서 `where/using` 중복 제거 효과
  - 실패 시 `derived zone from transfer target`, `derived using from transfer target`가 그대로 드러나는지

## Pair 4. Large workflow explicit vs compressed

- explicit: `examples/calendar_manage_event_explicit.pgy`
- compressed: `examples/calendar_manage_event_compressed.pgy`
- 핵심 비교:
  - explicit는 3개 step에서 `who / where / using / requires / authorized by / causes`를 반복한다
  - compressed는 각 step을 `using + on + expect` 중심으로 줄이고 action contract를 재사용한다
- 관찰 포인트:
  - 큰 intent에서 clause density가 실제로 얼마나 줄어드는지
  - 긴 워크플로에서도 compressed surface가 흐름 가독성을 유지하는지

## Pair 5. Composite orchestration explicit vs compressed

- explicit: `examples/composite_intent_orchestration_explicit.pgy`
- compressed: `examples/composite_intent_orchestration_compressed.pgy`
- 핵심 비교:
  - explicit는 orchestration step마다 boundary/authority/post-condition을 더 직접 드러낸다
  - compressed는 repeated zone/action contract를 최대한 reuse한다
- 관찰 포인트:
  - nested intent orchestration에서 반복 계약을 얼마나 덜 쓰게 되는지
  - compensate/post/success/failure가 compressed surface에서도 충분히 읽히는지

## Supplemental pair. Projection wiring 개별 선언 vs group surface

- explicit baseline: `examples/projection_refresh_publish_group_minimal.pgy`
- compressed baseline: `examples/projection_bind_group_minimal.pgy`
- 보조 비교: `examples/zone_context_minimal.pgy`
- 핵심 비교:
  - explicit baseline은 `refresh [..]` / `publish [..]`로 projection family를 분리해 유지한다
  - compressed baseline은 `bind [playerView, snapshot] from player`로 object/tobject projection을 한 번에 묶는다
  - `zone_context_minimal`은 projection pair는 아니지만 block-level default context가 authoring density를 줄이는 기준 예제로 본다
- 관찰 포인트:
  - projection wiring boilerplate
  - local projection vs boundary publication 구분이 여전히 읽히는지
  - compressed surface가 provenance를 숨기지 않는지

## Maximal compression reference

- reference: `examples/surface_compression_maximal.pgy`

이 예제는 pair라기보다 현재 구현된 압축 surface를 한 파일에 모은 stress reference다.

포함 내용:
- top-level lexical zone context
- action contract reuse
- unique matching subject action 기반 `who` derivation
- transfer target 기반 `using/where` derivation
- `move <from-alias> to <ZoneType>` shorthand

사용 목적:
- 새 압축 규칙을 넣을 때 이 파일보다 더 조용하고 더 읽기 쉬운지 비교
- diagnostics가 compressed surface에서도 provenance를 잃지 않는지 확인

## 앞으로 추가할 canonical pair

현재 smoke-covered pair:
- intent contract pair
- authority contract pair
- transfer contract pair
- calendar workflow pair
- composite orchestration pair

베타 전 추가 대상:
- game/simulation pair
- async/worker/device pair
- world handoff / relation-effect propagation pair

조건:
- pair마다 `explicit`과 `compressed`가 같은 결과를 내야 한다
- smoke나 semantic regression 중 최소 하나와 직접 연결한다
