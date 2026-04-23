# Pergyra Beta Closure Master Board

마지막 업데이트: 2026-04-24

## 목적

이 문서는 Pergyra를 `late-stage alpha`에서 `beta-closure` 상태로 밀어 올리기 위한 단일 기준판이다.

원칙:

- parser가 받는 surface는 semantic/C/LLVM/runtime/test/documentation까지 닫는다
- `부분 구현`을 stable surface로 남기지 않는다
- 새 ontology/키워드를 늘리기보다 기존 surface를 닫는다
- 조용한 fallback보다 explicit contract와 explicit failure를 우선한다
- beta 전에는 기능 추가보다 구조 debt 제거를 우선한다
- beta 이후에 debt가 터지는 구조는 지금 blocker로 본다

## 현재 판정

- 현재 단계: `late-stage alpha / beta-closure sprint`
- 베타 진행률 추정: `약 94-95%`
- 핵심 판단:
  - 표현력 부족보다 `closure depth`와 `surface trust`가 남은 문제다
  - 베타 차단축은 키워드 수가 아니라 `B0 의미론 + declaration-side MIR-only debt + type-resolution DAG closure + memory/lifetime debt`다
  - runtime propagation은 이제 C/LLVM 공통 `dirty/ready + epoch/cause` provenance baseline까지 닫혔고, 남은 차단점은 `bounded fixpoint / transitive frontier scheduler`다

## Beta Acceptance Line

베타로 부를 수 있는 최소 조건:

1. B0 네 축이 전부 닫힌다
2. declaration-side MIR emission이 HIR/AST fallback 없이 의미적으로 충분해진다
3. C/LLVM이 domain semantics 기준 parity를 유지한다
4. runtime observability가 디버깅 가능한 structured state를 제공한다
5. semantic type resolution이 ad-hoc recursive lookup만이 아니라 graph inventory / cycle diagnostic 기준으로 닫히기 시작한다
6. Linux/Windows CI가 parser/semantic/transpile/abi/backend-compare/example-smoke까지 녹색이다
7. 문서가 구현보다 앞서가지 않는다
8. scratch/result lifetime과 cache boundary가 문서/구현 기준으로 설명 가능하다

## Master Status Board

| 트랙 | 상태 | 진행률 | 베타 차단 여부 | 핵심 메모 |
|------|------|--------|----------------|----------|
| B0-1 Intent / Zone / World | 진행 중 | 88% | 차단 | observability baseline과 C/LLVM runtime provenance baseline은 닫혔지만 embedding/handoff rule, authority rejection surface, bounded fixpoint 기반 cross-layer propagation이 더 남음 |
| B0-2 relation / effect / projection | 진행 중 | 87% | 차단 | refresh/publish/bind baseline과 `dirty/ready + epoch/cause` provenance baseline은 닫혔지만 transitive propagation, effect partial order, scheduler depth가 더 남음 |
| B0-3 generic contract | 완료 | 100% | 비차단 | default arg, omitted trailing default, multi-bound, ability/authority/party/action/intent consumer, cross-module imported consumer가 semantic 회귀 기준으로 닫혔다 |
| B0-4 own/ref | 완료 | 100% | 비차단 | ownership classifier 기준 stable subset으로 닫힘. copy-value trivial own/ref, boundary-visible aggregate provenance, movable value transfer/borrow, slot-handle boundary, direct/summary helper-chain, destructure/member/container/return/channel 경로가 semantic 회귀로 고정됐다. `Token<T>` transport는 explicit reject, universal ownership lattice는 beta-out-of-scope다 |
| MIR-only declaration debt | 진행 중 | 97% | 차단 | host context는 inventory-backed handle 쪽으로 이동했고 function/method/intent emit state는 `TranspilerMirEmitState` snapshot helper로 수렴됐다. generic class specialization method도 MIR routine gate를 탄다. party/roster/relation/effect/zone/world hosted method emission은 공용 MIR helper로 수렴했고, declaration emit entrypoint도 inventory decl을 우선 사용한다. dead AST fallback은 제거되어 MIR routine 부재 시 partial C surface 없이 즉시 backend error로 실패한다. 남은 것은 declaration inventory bootstrap 잔여다 |
| Type-resolution DAG | 진행 중 | 70% | 차단 | graph inventory / cycle diagnostic / topo derivation 위에 provider-first staged worklist, local contract/projection synthetic node handler, generic default/constraint/where-bound staged resolution, role-action-intent-zone-party ability consumer pre-stage가 올라왔다. graph cycle과 legacy alias cycle 모두 `Contract source` / `Reason` / `Fix` vocabulary로 정렬됐고, full graph-backed evaluator는 beta-out-of-scope로 두고 stage-2 source-of-truth 승격이 남음 |
| Arena / lifetime discipline | 진행 중 | 81% | 차단 | 방향은 `Arena + Index 참조 + 역할별 arena 분리`로 고정했다. 규칙 문서화는 끝났고 transpiler scratch-only temporary의 첫 safe vertical slice, semantic result-owned diagnostic payload seam, semantic scratch arena가 ownership path 조립 / stdlib preload / enum method mangling / parallel task metadata / type-resolution cycle detection / match redundancy coverage까지 확장됐다. HIR/MIR에는 routine-scope `scratch` arena가, LLVM은 `scratch + persistent + result-owned` lane으로 정리되어 event invoke, intent collector, projection path, local grow array, type render helper, callable signature metadata까지 arena 경계가 올라왔다. 남은 것은 owner shell과 runtime ABI contract, 반환 ownership이 섞인 일부 helper다 |
| C/LLVM parity | 진행 중 | 89% | 차단 | LLVM stmt/expr fallback은 warning-only가 아니라 structured backend error로 고정됐고 AST dispatch partition smoke가 CI gate에 들어갔다. domain method MIR-missing 경로도 partial emit 없이 explicit backend error로 정렬됐다. Windows full green은 plain Linux host가 아니라 MSYS2/MinGW + LLVM runner truth로 분리했다 |
| runtime observability | 진행 중 | 82% | 차단 | last/history/active/recent baseline과 propagation provenance stamp는 있으나 queryable failure state와 bounded recompute provenance가 더 남음 |
| surface trust docs | 진행 중 | 87% | 차단 | 주요 surface는 정렬됐고 own/ref baseline도 넓어졌지만 B0 잔여에 맞춘 최종 재분류와 acceptance wording 고정이 남음 |

최근 고정:

- ownership diagnostics helper 9종이 `DiagPayload` 패턴으로 정렬됨
- ownership vocabulary 1차 sweep 완료:
  - `slot handle (anchored)`
  - `slot handle (movable)`
  - `authority-bearing`
- semantic regression은 현재 기준으로 `2132 passed, 0 failed`
- transpile regression은 현재 기준으로 `625 passed, 0 failed`
- runtime propagation provenance baseline closure:
  - relation/effect/zone/world hidden cell이 `dirty/ready + epoch/cause` schema로 C/LLVM parity를 갖는다
  - LLVM `__projection_dirty_*` layout debt와 host-field assignment invalidation drift를 제거했다
  - intent rebound-zone projection invalidation도 다시 연결되어 backend compare drift가 줄었다
  - 현재 propagation blocker는 helper flag 부재가 아니라 `bounded fixpoint / transitive frontier scheduler`다
- AST 타입 디스패치 partition 규칙 문서화 완료 — `docs/95_ast_dispatch_partition.md`. 4 카테고리 (type annotation / decl sub-metadata / top-level decl / root) 로 전체 AST 타입이 disjoint 분할되고, 각 카테고리별로 case label 추가/금지/safety-net 판단 기준이 고정됨. `llvm_stmt.c` skip 리스트 + Zone/World safety-net forward 가 이 문서 기준으로 정렬됨
- AST dispatch partition smoke 추가 — `tests/ast_dispatch_partition_smoke.sh`, `make ast-dispatch-test-smoke`. LLVM `stmt/expr`의 unknown/default path가 warning-only나 silent `0/null` fallback으로 회귀하지 못하게 Linux CI acceptance line에 연결됨
- type-resolution DAG cycle provenance 강화 — graph validator cycle과 legacy alias-resolution cycle 모두 `Contract source:` / `Reason:` / `Fix:` 구조를 갖도록 정렬. semantic graph regression은 해당 vocabulary를 요구하며 `test-semantic 2019/0`으로 검증됨
- C/LLVM init idiom 축 1차 감사 + 1차 정비 완료 — `docs/93_codegen_idiom_audit.md` 참조
  - Case 1 (uninit local) HIGH divergence는 semantic 레벨 차단으로 **해소** (`PGY_CODE_SEM_UNINIT_LOCAL`): 함수-바디 `let x: T;` 거부. 관련 회귀 3종 추가
  - Case 2 (C backend aggregate fallback) 경로 `L815`는 `transpiler_c_type_uses_scalar_zero` helper로 scalar/aggregate 분기. defense in depth
  - Case 3 (slot claim) MEDIUM 비대칭은 **의도된 비대칭으로 확정** — runtime observability 확장 시 재감사
- arena 방향 고정 — `docs/94_arena_index_lifetime_plan.md`
  - `Arena + Index 참조 + 역할별 arena 분리` 채택
  - cache에 arena-owned pointer 저장 금지
  - transpiler scratch-only temporary의 첫 vertical slice 완료
    - zone authority temp
    - intent priority default literal
    - projection refresh `source_expr`
    - event declaration `event_type`
  - semantic diagnostics result seam 1차 도입
    - `DiagPayload` emit 경로는 result-owned payload snapshot을 `Diagnostic`에 보존
    - semantic JSON 출력은 payload field를 함께 노출 가능
  - semantic scratch arena 1차 도입
    - `SemanticContext`에 scratch arena 추가
    - ownership diagnostic path string은 scratch arena에서 조립
  - `slot_ref_expr(...)` 같은 반환 string helper는 아직 전환 보류
  - beta 전에 첫 vertical slice를 반드시 착수

## Debt-first Scheduling Rule

beta 직전 운영 규칙:

1. 의미론/백엔드 bug fix
2. declaration-side MIR-only debt 제거
3. own/ref / generic / provenance closure
4. parity / CI closure
5. arena/lifetime debt vertical slice
6. 문서/예제/source-of-truth 정렬

즉, 기능이 조금 더 늘어나는 것보다 “베타 이후에 구조 debt가 폭발하지 않게 만드는 것”을 우선한다.

## Structural Closure — Type-resolution DAG

판정:

- module import graph는 이미 존재하지만, type resolution 자체는 아직 `resolve_type_node(...)` 중심의 recursive evaluator가 주축이다
- beta는 이제 generic contract / module contract / authority consumer까지 포함한 type dependency graph closure를 목표로 삼는다
- 즉, ecosystem 확장 전의 beta 기준에는 `type-resolution DAG inventory + cycle diagnostic + graph-backed migration entrypoint`가 포함된다

닫힌 것:

- import resolver의 canonical path + cycle detection baseline
- semantic named-type lookup baseline
- generic default / where-bound / alias target / ability consumer / class specialization 일부 consumer가 graph inventory에 올라오기 시작함
- provider-first topo-driven staged resolution worklist가 실제로 활성화되어 top-level declaration inventory와 local/projection synthetic node를 소비함
- generic `default_type` / generic constraint / `where` bound가 staged DAG resolver 경로를 통과하며 semantic 회귀와 Linux CI에서 검증됨
- role impl / action / intent step / zone authority / party role slot ability consumer가 provider pre-stage와 cycle provenance 회귀를 통해 같은 DAG 경로로 정렬됨
- graph regression이 world lifecycle / relation-effect propagation / generic consumer schedule / alias cycle provenance / generic default-bound cycle provenance / role-action-intent-zone-party ability consumer provenance까지 확장됨
- graph validator cycle과 legacy alias cycle diagnostic이 모두 `Contract source` / `Reason` / `Fix` vocabulary로 정렬됨

남은 것:

- provider/consumer inventory를 generic/module/authority/party 경로까지 더 확장
- world/zone local contract와 projection path도 graph vocabulary로 계속 끌어올림
- SCC/cycle diagnostic을 alias depth fallback보다 신뢰 가능한 기준으로 승격
- legacy alias cycle fallback도 graph cycle과 같은 provenance vocabulary로 정렬해 migration 중에도 사용자-facing 진단 구조가 갈라지지 않게 유지
- topo scheduling과 staged declaration worklist를 top-level decl에서 local/projection synthetic node, ability/authority/party consumer를 넘어 더 넓게 연결
- graph-backed evaluator entrypoint를 declaration prepass에서 semantic source-of-truth로 승격
- namespace-only lookup과 full concrete materialization을 분리
- backend가 resolved-type metadata를 재사용할 수 있게 inventory를 정렬

완료 기준:

- type dependency cycle이 path-aware diagnostic으로 보고된다
- generic default / multi-bound / ability consumer / zone authority consumer가 같은 graph vocabulary로 추적된다
- `topo_order`가 declaration staged worklist를 실제로 구동한다
- recursive resolver가 남아도 graph-backed inventory가 source-of-truth가 되기 시작한다

## B0 Closure Board

### B0-1. Intent / Zone / World

판정:

- intent orchestration, inherited/derived contract, zone/world query, observability baseline은 이미 존재한다
- runtime provenance baseline(`dirty/ready + epoch/cause`)도 이제 C/LLVM parity로 들어왔다
- 남은 일은 embedding ownership/handoff policy, bounded fixpoint 기반 cross-layer propagation policy, richer authority rejection surface, declaration/runtime/diagnostic까지의 C/LLVM parity를 닫는 것이다
- 이 축은 언어 정체성 자체이므로 beta 직전까지 열어두면 안 된다

닫힌 것:

- intent orchestration baseline
- active/recent/last/history query baseline
- zone/world state/layer/projection query baseline
- embedded world -> zone projection visibility baseline
- relation/effect/zone/world hidden provenance baseline (`dirty/ready + epoch/cause`)

남은 것:

- embedding ownership와 handoff policy를 한 규칙으로 고정
- mutation visibility를 value-copy 오해 없이 handle/reference 의미로 정렬
- intent failure/authority/boundary mismatch provenance를 더 깊게 연결
- single-pass sync를 넘는 bounded fixpoint / transitive frontier scheduler를 도입
- multi-instance timeline과 recent/history structured query를 보강
- C/LLVM/runtime diagnostics 품질을 같은 수준으로 정렬

직접 마감 항목:

- declaration parity
- runtime parity
- diagnostic parity
- embedding ownership rule
- handoff visibility rule
- cross-layer propagation rule
- bounded recompute rule

완료 기준:

- world embedding/handoff가 semantic/runtime/backend compare에서 같은 결과를 낸다
- failure diagnostics가 `contract provenance + reason + fix` 구조를 유지한다
- runtime propagation이 single-pass helper replay가 아니라 bounded recompute 규칙으로 설명 가능하다
- active/recent/history 관측이 example smoke와 ABI 경로에서 안정적으로 검증된다

### B0-2. relation / effect / projection

판정:

- declaration, lifecycle shorthand, `refresh/publish/bind`, layer/state query, overlay sync baseline은 이미 존재한다
- projection contract diagnostics baseline도 이미 존재한다
- effect join/meet/conflict baseline도 이미 존재한다
- runtime provenance baseline(`dirty/ready + epoch/cause`)도 이제 C/LLVM parity로 닫혔다
- 남은 일은 authority-resource partial order 통합, transitive projection propagation policy, bounded recompute scheduler, deeper runtime contract provenance, helper-heavy edge path 감소와 parity를 닫는 것이다
- 이 축은 Pergyra의 domain semantics 핵심이므로 partial 상태로 beta에 올리면 안 된다
- projection은 언어 강점이므로 실패 이유가 약하면 가장 먼저 authoring friction을 만든다

닫힌 것:

- declaration baseline
- refresh/publish/bind baseline
- layer/state query baseline
- projection contract structured diagnostics baseline
- effect join/meet/conflict baseline
- relation/effect/zone/world hidden provenance baseline (`dirty/ready + epoch/cause`)

남은 것:

- authority/resource/effect partial order를 semantic contract로 더 명확히 승격
- projection propagation policy를 branch/join/handoff/embedded zone-world path까지 더 조밀하게 검증
- single-pass helper replay를 bounded fixpoint / transitive frontier scheduler로 승격
- helper-heavy best-effort sync를 줄이고 explicit backend/runtime failure로 승격
- runtime contract provenance를 edge path까지 일관화
- helper-heavy edge path를 줄여 declaration/runtime/diagnostic/backend parity를 더 직접적으로 맞춘다

최근 진전:

- authority-bearing zone lifecycle clause(`apply/link/detach/unlink/maintain`)는 이제 `by <subjectSlot>` 없이 경고로 남지 않고 hard error로 승격되기 시작했다
- `intent step causes`와 zone effect slot materialization mismatch도 hard error로 올려 surface trust를 강화했다
- duplicate authority / unknown layer relation/effect type도 benign warning 대신 contract violation으로 다루기 시작했다

직접 마감 항목:

- authority-resource-effect ordering rule
- projection branch/join propagation rule
- projection handoff propagation rule
- projection embedded zone/world propagation rule
- runtime contract provenance visibility
- bounded recompute / fixpoint rule
- helper-heavy edge path 감소
- declaration/runtime/diagnostic/backend parity

diagnostic 고정 규칙:

- projection failure는 최소한 다음 정보를 포함해야 한다
  - `target`
  - `source`
  - `projection kind`
  - `field path`
  - `fix`
- 진단 포맷은 `Reason:` / `Fix:` 구조로 고정한다
- unsupported/ambiguous/missing projection 경로 모두 같은 구조를 유지한다

완료 기준:

- relation/effect propagation regression이 branch/join/handoff path까지 고정된다
- C/LLVM compare가 propagation과 refresh/publish visibility를 같은 결과로 보여준다
- runtime propagation이 single-pass helper replay가 아니라 bounded recompute 규칙으로 설명 가능하다
- unsupported projection surface는 parser/semantic에서 명시 거부된다
- projection diagnostics가 `target/source/projection kind/field path/fix`를 모두 포함하고 `Reason:` / `Fix:` 포맷을 유지한다

### B0-3. generic contract

판정:

- `ability<T>` baseline, default type argument baseline, omitted trailing default resolution, generic mismatch provenance baseline은 이미 존재한다
- 남은 일은 multi-bound 전경로 enforcement, module-contract propagation, instantiation-path parity, richer mismatch diagnostics를 닫는 것이다
- generic은 parser가 받는 surface와 실제 contract가 어긋나기 가장 쉬운 축이므로 partial acceptance를 beta에 올리면 안 된다

닫힌 것:

- generic ability baseline
- default type arg baseline resolution
- omitted trailing default resolution
- generic ability impl-reference omission baseline
- generic mismatch provenance baseline

남은 것:

- `where T: A + B` multi-bound 전경로 enforcement
- function/class/ability/requires/authority/module contract 경로 정렬
- declaration site뿐 아니라 모든 instantiation path에서 같은 constraint enforcement 보장
- richer expected/actual/bound/consumer-path provenance diagnostics
- cross-module consumer path에서도 generic contract가 동일하게 유지되도록 정렬
- C/LLVM/test parity를 generic path 기준으로 확대
- `Map<K, V>`는 현재 hardcoded stable key subset(`String | Int | Long | Bool`)까지 닫혔고, arbitrary `K` 일반화는 explicit debt로 남긴다

최근 진전:

- ability consumer path에서 unresolved effective generic arg를 더 이상 silent skip하지 않고 structured error로 승격했다
- class instantiation/specialization where-clause도 unresolved effective arg를 그냥 넘기지 않도록 hardening했다
- role include / roster party slot 같은 declaration entrypoint도 unresolved declaration이면 warning이 아니라 error로 승격되기 시작했다
- role-side ability require-field type resolution도 unresolved effective generic arg를 그냥 넘기지 않도록 hardening했다
- malformed impl ability generic arg가 있어도 뒤쪽 where/require-field 검증으로 partial 진행하던 경로를 차단했다
- default generic bound validation에서 unknown parameter / unresolved default type도 structured error로 승격했다
- generic function call-site where-clause validation도 missing/unresolved effective arg를 그냥 넘기지 않도록 hardening했다
- collection generic surface도 stable key subset(`String | Int | Long | Bool`) 기준으로 semantic/C/LLVM/runtime/documentation을 다시 정렬했다
- multi-bound generic mismatch diagnostics가 ability/class/function consumer path에서 `broken bound`뿐 아니라 `full bound set`까지 노출하도록 정렬됐다
- module-contract `requires Ability<T>` 경로도 arity/malformed-arg 진단에서 `expected type args` / `actual type args` / `consumer path`를 함께 노출하도록 정렬됐다
- `impl Ability<T>` / party-role consumer path도 old wording을 제거하고 `expected type args` / `actual type args` / `consumer path` vocabulary로 다시 맞췄다
- ability require-field generic materialization/resolution failure도 `generic subject` / `consumer path` / `actual type args` vocabulary로 정렬됐다

완료 기준:

- parser가 받는 generic surface는 모두 semantic/backend/test closure를 가진다
- default arg와 multi-bound가 declaration/use/consumer path에서 동일 규칙으로 동작한다
- generic mismatch는 `generic subject / expected type args / actual type args / broken bound / consumer path / fix`를 함께 보여준다

diagnostic 고정 규칙:

- generic mismatch는 최소한 다음 정보를 포함해야 한다
  - `generic subject`
  - `expected type args`
  - `actual type args`
  - `broken bound`
  - `full bound set`
  - `consumer path`
  - `fix`
- 진단 포맷은 `Reason:` / `Fix:` 구조로 고정한다
- declaration-site mismatch와 consumer-site mismatch 모두 같은 구조를 유지한다

### B0-4. own/ref

판정:

- beta stable subset은 ownership classifier 기준으로 닫혔다
- `own/ref`는 anchored-only 또는 boundary-only 실험 surface가 아니다
- copy-only 값은 trivial semantics로 허용하고, borrow-tracked/move-only/subject identity/slot-handle branch는 escape/rebind/store/send/return/helper boundary에서 추적한다
- `Token<T>` transport는 authority-bearing explicit reject로 유지한다
- region/lifetime solver와 universal ownership lattice는 beta-out-of-scope다

닫힌 것:

- copy-value trivial own/ref + boundary-visible aggregate provenance + slot-handle boundary rule
- constructor field store / transitive helper return / channel send / direct return / nested projection provenance 회귀
- helper/return/channel wording family의 공용 정렬
- direct/summary helper-chain coverage
- destructure binding / member rebind / container store / array store/literal 경로
- class/subject/tuple/object/array aggregate matrix

최근 진전:

- return/channel boundary ownership diagnostics는 `Reason:` / `Fix:` 구조로 정렬되기 시작했다
- anchored handle return signature rejection도 provenance형 hard error로 정렬됐다
- movable resource(`QubitSlot`)는 explicit `own` parameter transfer path를 부분적으로 열기 시작했다
- local binding 단계에서도 unnamed `recv/await` use, subject rebinding, released-slot move, anchored-handle rebinding이 provenance형 hard error로 정렬되기 시작했다
- slot escape analyzer도 return/helper-call/channel/unterminated local claim 경로를 provenance형 `Reason:` / `Fix:` 경고로 정렬하기 시작했다
- nested projection source(`cargo.wrapper.packet`)는 constructor field store / member rebind / list/set/queue/map store / array overwrite / helper return summary / channel send / direct return까지 회귀로 고정됐다
- class/subject consumer matrix는 return / channel / helper / list / set / queue / map / array push / array overwrite / member rebind / constructor field store까지 거의 동형으로 정렬됐다
- tuple/object 경로도 `test_semantic.c`의 기존 regression 축에서 channel/new-binding/rebind/return/helper forwarding/queue-map-array overwrite/projection provenance coverage가 유지된다
- `QubitSlot`/class helper-chain 회귀도 ownership-boundaries 계열에 추가돼 direct helper/function call family가 transitive chain까지 고정됐다

직접 마감 항목:

- 완료. 새 ownership 의미론은 베타 범위에서 추가하지 않는다
- 향후 작업은 universal ownership lattice / region solver / richer lifetime inference로 분리한다

diagnostic 고정 규칙:

- ownership failure는 최소한 다음 정보를 포함해야 한다
  - `value`
  - `ownership mode`
  - `moved here` 또는 `borrowed here`
  - `escaped here` 또는 `rebound here`
  - `consumer path`
  - `fix`
- 진단 포맷은 `Reason:` / `Fix:` 구조로 고정한다
- assignment/call/return/channel/container path 모두 같은 구조를 유지한다

완료 기준:

- classifier-backed stable subset이 존재한다
- runtime 보정이 아니라 semantic 단계에서 위반을 차단한다
- channel/return/helper call escape path가 회귀로 고정된다
- ownership diagnostics가 `value/mode/provenance/consumer-path/fix`를 포함하고 `Reason:` / `Fix:` 포맷을 유지한다
- `Token<T>` transport는 explicit reject로 남는다

## MIR-only Declaration Closure Board

현재 방향:

- routine emission은 MIR 중심으로 정렬 중이다
- declaration-side intent inventory는 explicit MIR metadata를 더 많이 사용하도록 이동 중이다
- transpiler host context 복원은 `current_host_decl`를 우선 truth로 두고, 남은 fallback도 inventory lookup 쪽으로 더 밀려서 role-owner direct AST lookup이 빠졌다
- transpiler declaration/method emission의 direct `current_*_name` restore chain 일부가 공용 host-context restore helper로 접혔다
- C backend의 direct `current_*_name` 사용은 emitter hot path보다 helper/restore layer에 더 집중되도록 정리됐다
- LLVM declaration helper도 current host lookup을 공용 active-inventory host helper로 접어 direct naming chain을 한 단계 줄였다
- LLVM MIR/domain emission의 direct `current_class_name` save/restore도 공용 host-name bind/restore helper로 이동했다
- LLVM expr/stmt hot path도 `llvm_current_host_decl_name(...)` helper를 통과하도록 정렬돼 raw host-name read가 더 줄었다
- `HasProjection/HasLayer/HasState/HasZone*` 계열 builtin과 method/field helper도 raw host-name state 대신 host helper를 통과하도록 정렬됐다
- LLVM pipeline의 nominal registration / class method emission도 nominal AST array보다 `mir->decl_headers`를 직접 순회하도록 정렬됐다
- LLVM domain pass도 raw `ctx->mir->{relations,effects,zones,...}` 접근 대신 `llvm_active_domain_inventory(...)` helper를 통과하도록 정렬됐다
- 최근 검증선: `test-transpile 529/0`, `test-abi 84/0`이 host-helper migration 이후에도 유지됐다

남은 핵심 debt:

- zone/world/relation/effect declaration emission의 HIR/AST inventory fallback 제거
- executable/declaration inventory를 MIR entry metadata로 대체
- naming helper와 owner/self typing fallback 제거
- helper/restore layer 바깥의 raw host-name read를 더 제거하고 회귀로 고정
- block emission failure를 comment/skip가 아니라 backend error로 승격
- `Unknown` / `Int` / `int32_t` fallback emission 제거

완료 기준:

- declaration emission과 routine emission 모두 MIR inventory만으로 충분하다
- backend가 부족한 정보를 조용히 추측하지 않는다
- fallback comment 대신 explicit backend error가 나온다

## Runtime Observability Board

현재 있는 것:

- `IntentLast*`
- `IntentActive*`
- `IntentRecent*`
- `IntentHistoryStep*`
- zone/world state query baseline

남은 것:

- richer structured recent/history storage
- failure provenance visibility
- zone/world runtime state를 더 직접적으로 설명 가능한 형태로 노출
- same-process compile/runtime stability 회귀 고정

완료 기준:

- 디버깅 가능한 최소 structured state를 제공한다
- ABI/runtime smoke에서 same-process 재실행 안정성이 보장된다
- C/LLVM 양쪽 모두 관측 surface가 같은 사실을 보여준다

## Parity / CI Board

필수 라인:

- parser
- semantic
- transpile
- ABI
- backend compare
- llvm smoke
- example smoke
- Linux
- Windows

### 9. Backend parity final closure

목표:

- C/LLVM이 기본 제어 흐름뿐 아니라 domain semantics에서도 같은 결과를 낸다

대상:

- intent / zone / world
- relation / effect / projection
- ownership boundary
- refresh / publish / bind / propagation
- world embedding / handoff

원칙:

- 한쪽 backend만 통과하는 surface는 stable로 간주하지 않는다
- backend 차이는 조용한 fallback이 아니라 explicit error 또는 explicit unsupported contract로 드러낸다
- parity는 stdout만이 아니라 runtime state / diagnostics / ABI shape까지 본다

완료 기준:

- `backend compare`
- `llvm smoke`
- `example smoke`
- `ABI/runtime probe`

가 Linux/Windows 모두 녹색이다

추가 회귀 축:

- explicit surface vs compressed surface가 backend별로 같은 결과를 낸다
- intent history / active / recent observability가 backend별로 같은 사실을 보여준다
- projection refresh/publish visibility가 backend별로 같은 사실을 보여준다

완료 기준:

- warning-only 성공이 아니라 expected stdout/stderr와 parity까지 고정
- domain semantics 기준 compare 케이스가 지속적으로 녹색
- same-process LLVM 재진입/ABI 불안정이 남지 않는다

## Pain Point Board

현재 가장 큰 작성 pain point:

1. clause density
2. action 계약과 intent/zone 쪽의 반복 기술
3. derived/inherited contract가 실패 메시지에서 충분히 보이지 않는 문제
4. explicit surface와 compressed surface의 정석 작성 경로가 아직 약한 점

원칙:

- pain point 완화는 closure 이후에만 공격적으로 늘린다
- 단, provenance visibility와 pair example은 closure와 동시에 강화한다

고정 작업:

- `derived_* / inherited_*` vocabulary 전면 정렬
- explicit vs compressed pair example 최소 4쌍 유지
- long-form과 compressed-form이 같은 의미를 가진다는 regression/source-of-truth 확보

## Next Locked Sequence

1. B0-1 Intent / Zone / World 잔여 좁히기
   - embedding ownership / handoff
   - cross-layer propagation
   - richer provenance
   - declaration/runtime/diagnostic parity
2. B0-2 relation / effect / projection closure
   - authority-resource-effect partial order
   - branch/join/handoff/embedded propagation
   - projection propagation policy
   - runtime contract provenance
   - helper-heavy edge path 감소
   - declaration/runtime/diagnostic/backend parity
3. B0-3 generic multi-bound/module-contract closure
   - multi-bound 전경로 enforcement
   - module-contract propagation
   - instantiation-path parity
   - expected/actual/bound/consumer-path diagnostics
4. B0-4 own/ref general movable rule 확장
   - movable vs copy type rule
   - assignment/call/return/channel/container/rebind ownership
   - helper-call escape analysis
   - ownership provenance diagnostics
5. declaration-side MIR-only debt 제거
   - 진행: domain method emission이 MIR inventory 존재 시 AST fallback으로 조용히 내려가지 않도록 C/LLVM gate를 정렬
6. Backend parity final closure
7. runtime observability structured state 보강
8. surface trust 문서 최종 정렬

## Exit Rule

다음 중 하나라도 남아 있으면 베타라고 부르지 않는다:

- parser가 받는 surface 중 semantic/runtime/backend/test/documentation이 닫히지 않은 것
- declaration-side backend가 AST/HIR fallback 없이는 유지되지 않는 것
- C/LLVM 중 하나만 되는 domain semantics
- runtime observability가 얇아서 failure provenance를 설명하지 못하는 것
- 문서가 stable이라고 주장하지만 실제 구현은 partial인 것

## Contract provenance vocabulary

베타 문서/진단/AST print에서 contract provenance 표준어는 아래 둘로 고정한다.

- `inherited`
  - action contract, 상위 declaration contract, 이미 선언된 zone/world contract에서 그대로 재사용된 clause
  - 예: `who/where/requires/causes/authorized by`가 action에서 intent step으로 재사용될 때
- `derived`
  - 현재 step/zone/world 문맥에서 `using`, `transfer`, local binding, contract wiring으로 계산된 clause
  - 예: `where`가 `using`에서 계산되거나, `using/where`가 transfer edge에서 계산될 때

규칙:

- contract source 설명에서는 `inferred`를 쓰지 않는다
- `inferred`는 일반 타입 계산, internal analysis, backend-local helper naming 같은 non-contract 문맥에만 남긴다
- 사용자-facing wording은 항상
  - `inherited contract`
  - `derived contract`
  - `contract provenance`
  로 통일한다

독해 규칙:

- `inherited`는 "이미 있던 계약을 가져왔다"
- `derived`는 "현재 문맥에서 계산했다"

즉, contract failure를 설명할 때는

- 어디서 `inherited` 되었는지
- 무엇이 현재 문맥에서 `derived` 되었는지
- 그 결과 왜 충돌했는지

를 같은 vocabulary로 보여줘야 한다.
