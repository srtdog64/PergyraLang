# Pergyra 컴파일러 계약 고정안

마지막 업데이트: 2026-04-06

이 문서는 앞으로 흔들리면 안 되는 다섯 가지 핵심 계약을 고정한다.

- `HIR / DIR / RIR / MIR` 계층 책임
- resource state lattice
- intent compensation model
- projection sync semantics
- authority / capability model

이 문서는 "현재 구현이 완전히 여기까지 왔다"는 보고서가 아니다.
반대로, 구현이 앞으로 맞춰야 할 **컴파일러 계약**을 고정하는 문서다.

## 1. IR 계층 계약

## 1.1 AST

역할:

- raw parse tree
- 표면 문법 보존
- sugar 보존
- 원래 선언 위치와 source mapping 보존

금지:

- 이름 바인딩의 최종 근거가 되면 안 됨
- 타입 확정 결과의 최종 근거가 되면 안 됨
- backend 입력이 되면 안 됨

## 1.2 HIR

역할:

- 표면 문법 정리
- sugar 제거
- import splice 흡수
- 이름 바인딩 완료
- 타입 추론/어노테이션 부착
- `subject / class / object / dto / ability / role / relation / effect / zone / world / intent`를 first-class로 유지

출력:

- semantic typed tree에 가까운 정규화 언어 트리
- frontend / diagnostics / bridge / indexing용 view

허용:

- AST 참조
- declaration index
- routine summary
- CFG/dominance 같은 pass-friendly 보조 뷰

금지:

- 자원 의미론의 최종 해석
- flow-sensitive resource merge의 최종 근거
- backend 의미론을 HIR에 박아넣기

한 줄 정의:

> HIR는 "문법 노이즈가 제거된 typed language tree"다.

## 1.3 DIR

역할:

- 도메인 존재론 정규화
- declaration-level graph
- 실행 흐름 전 단계에서 끝낼 수 있는 도메인 정합성 검사

다루는 것:

- ability 계약과 role 구현 완전성
- party composition
- zone/world membership
- relation/effect declaration consistency
- intent participant / step dependency graph
- authority declaration consistency
- projection declaration consistency

최소 edge 예시:

- `role -> for type`
- `role -> impl ability`
- `party slot -> required ability`
- `world -> zone`
- `zone authority -> ability`
- `intent participant -> bound type`
- `intent step -> zone`
- `intent step -> who actor alias`
- `intent step -> required ability`
- `intent step -> authorized-by alias`
- `intent step -> causes effect`
- `intent step(n) -> step(n+1)` dependency

출력:

- domain graph
- declaration metadata
- dependency edge

금지:

- CFG
- SSA
- liveness
- resource state merge

한 줄 정의:

> DIR는 "존재론과 선언 관계의 그래프"다.

## 1.4 RIR

역할:

- 자원 의미론 해석
- 값 계산보다 "누가 무엇을 언제 합법적으로 쥐고 있나"를 정규형으로 표현
- CFG 이전에 확정 가능한 ownership/capability/projection/lifecycle 사실을 고정

출력:

> `Resource Graph + Transfer Ops + Static Ownership Facts`

그리고 최소한 scope별 normalized summary를 가져야 한다.

- tracked resource/projection별 `initial_state`
- linear scan 이후 `final_state`
- 마지막 관련 op
- transition error 여부

RIR는 단순 수명 맵이 아니다. 최소한 다음을 explicit op로 가진다.

- `Claim`
- `Read`
- `Write`
- `Release`
- `Move`
- `BorrowRead`
- `BorrowWrite`
- `ProjectRefresh`
- `ProjectPublish`
- `AttachEffect`
- `DetachEffect`
- `LinkRelation`
- `UnlinkRelation`
- `Authorize`
- `AwaitRemote`
- `CommitIntent`
- `AbortIntent`
- `CompensateIntentStep`

다루는 것:

- `Slot / SecureSlot / DeviceSlot / QubitSlot / RemoteFuture`
- projection validity
- authority-bound mutation
- relation/effect lifecycle
- intent compensation resource edge
- cross-zone/world handoff resource fact
- nominal `relation/effect/zone/world` handle fact와 handoff op

현재 구현 메모:

- RIR는 이미 scope-level summary를 넘어서 HIR CFG 기반 `flow-block[...]`를 materialize한다.
- 각 flow fact는 `entry/exit`, `merged_from_join`, `widened_by_loop`, `entry_conflict`, `exit_conflict`를 가진다.
- flow block은 resource fact가 없어도 버리지 않고, 최소한 `authority`, `projection`, `world-handoff`, `invalidation` conservative semantic flag를 `sem-entry/sem-exit`로 보존한다.
- handle merge는 resource kind를 함께 읽는다.
  - `zone/world handle`은 ownership/borrow 중심
  - `relation/effect handle`은 detach/sync/dirty lifecycle 중심

MIR로 이월하는 것:

- branch/join/loop/phi merge
- conditional lifetime
- cleanup edge
- rollback path convergence

한 줄 정의:

> RIR는 "프로그램을 자원 상태 전이 시스템으로 본 결과"다.

## 1.5 MIR

역할:

- 실행 구조 정규화
- CFG
- basic block
- explicit instruction
- SSA / phi
- liveness
- `RIR-flow` merge
- cleanup edge / rollback edge / detach-invalidation edge
- traditional optimization

다루는 것:

- branch/join/loop
- resource lattice merge
- intent step / compensate complete-path validation
- constant propagation
- DCE

한 줄 정의:

> MIR는 "RIR가 해석된 뒤의 실행 그래프"다.

현재 코드 계층 최소 구현:

- HIR CFG block을 MIR block으로 복사
- HIR phi skeleton을 MIR phi node로 materialize
- HIR local def를 `def` instruction + block-local SSA rename 형태로 materialize
- branch/return/resource-op/cleanup instruction use를 versioned name으로 기록
- block별 `ssa_entry_versions` / `ssa_exit_versions`를 저장해 phi incoming과 use edge가 predecessor exit map을 직접 참조한다
- reachable intent block마다 cleanup successor edge를 두고, cleanup convergence root 아래에 rollback block과 invalidation block을 분리한다
- routine-level value summary를 만들어 def/use/live/cleanup 도달 여부를 후속 pass가 재사용할 수 있게 한다

즉 MIR는 아직 full optimizer IR은 아니지만, pure placeholder를 넘어서
`phi`, versioned local value, instruction-level use, routine-level value summary, cleanup convergence, rollback/invalidation edge를 직접 가지는 실행 그래프다.

## 2. Resource State Lattice 계약

Pergyra의 resource lattice는 최소한 아래 상태를 가진다.

## 2.1 상태 집합

- `Uninit`
  - 아직 claim/bind/materialize되지 않음
- `Owned`
  - 현재 경로에서 배타적으로 소유됨
- `BorrowedRead`
  - 읽기 전용 차용 중
- `BorrowedWrite`
  - 배타 쓰기 차용 중
- `Moved`
  - ownership가 다른 값/슬롯/경계로 이전됨
- `Released`
  - 명시적으로 해제됨
- `Invalid`
  - projection invalidation, detach, authority loss 등으로 더 이상 유효하지 않음
- `Measured`
  - `QubitSlot`처럼 측정 이후 원래 읽기 규칙이 사라진 상태
- `RemotePending`
  - remote completion이 확정되지 않은 상태

## 2.2 병합 원칙

이 lattice는 "성공적인 실행 가능성"이 아니라 "안전한 공통 보수 상태"를 택한다.

예:

- `Owned + Owned -> Owned`
- `Owned + Moved -> Invalid`
- `Owned + Released -> Invalid`
- `BorrowedRead + BorrowedRead -> BorrowedRead`
- `BorrowedRead + BorrowedWrite -> Invalid`
- `Moved + Released -> Invalid`
- `RemotePending + Owned -> RemotePending`가 아니라, remote completion을 기다리는 경계에서는 `UnknownRemote`류가 필요하면 후속 확장

현재 고정 원칙:

> merge는 낙관적이지 않고 보수적이어야 한다.

## 2.3 Resource Kind 축

resource lattice는 값 상태만이 아니라 resource kind와 함께 읽힌다.

최소 resource kind:

- `LocalSlot`
- `SecureSlot`
- `DeviceSlot`
- `QubitHandle`
- `RemoteFutureHandle`
- `ProjectionObject`
- `ProjectionDto`
- `EffectInstance`
- `RelationInstance`
- `ZoneHandle`
- `WorldHandle`

## 3. Intent Compensation Model 계약

Intent는 함수 호출처럼 보이지만, 실제로는 runtime orchestration instance를 만든다.

## 3.1 step 실행 순서

현재 고정 순서:

1. `pre`
2. `invariant(pre)`
3. repeated `on`
4. `guard`
5. `expect`
6. `post`
7. `invariant(post)`

## 3.2 rollback 정책

- `rollback: full`
  - 이미 완료된 step 전부를 reverse-order로 보상
- `rollback: current`
  - 가장 최근 completed step만 보상
- `rollback: none`
  - compensate를 자동 실행하지 않음

## 3.3 compensation 원칙

- compensation은 선언된 역순으로 실행된다
- compensation도 explicit step effect로 취급된다
- compensation 실패는 원래 failure를 덮어쓰지 않고 별도 상태로 남겨야 한다

최소 상태 모델:

- `IntentSucceeded`
- `IntentFailed`
- `IntentCompensationFailed`
- `IntentPending`
- `IntentUnknown`

현재 구현은 일부가 `Bool`/last-failure 문자열 중심이지만, 장기 계약은 위 상태 집합을 향한다.

## 3.4 cross-world 원칙

- `transfer:`는 source/target world 또는 zone 경계를 명시하는 orchestration edge다
- transfer 이후 identity handoff와 resource handoff는 trace/history에 남아야 한다
- compensation은 단순 역순 호출이 아니라 "관찰 가능한 상태 전이"여야 한다

한 줄 정의:

> intent compensation은 helper call 모음이 아니라 stateful rollback graph다.

## 4. Projection Sync Semantics 계약

projection sync는 부수 효과가 아니라 언어 계약이다.

## 4.1 projection 종류

- `object`
  - 읽기 전용 스냅샷
- `dto`
  - 경계 밖 전달용 projection

## 4.2 projection 연산

- `refresh`
  - subject/object source에서 object target을 갱신
- `publish`
  - subject/object source에서 dto target을 갱신
- `bind`
  - declaration site에서 object/dto target kind를 유지한 채 source를 연결

## 4.3 validity 상태

projection은 최소한 아래 상태를 가진다.

- `Synced`
- `Dirty`
- `Invalid`
- `Detached`
- `Published`

원칙:

- source subject/object가 mutate되면 projection은 자동 또는 명시 sync 전까지 `Dirty`
- detach/unlink/authority loss/lifecycle end가 projection source를 끊으면 `Invalid` 또는 `Detached`
- `publish`는 dto 전달 완료를 의미하지만 source mutation을 막지는 않음

## 4.4 zone/world 계약

- page는 projection surface다
- zone/world는 projection sync의 실행 경계다
- projection sync 여부는 `HasProjection` / `HasZoneProjection` 같은 query로 관찰 가능해야 한다

한 줄 정의:

> projection은 값 복사가 아니라 validity를 가진 view contract다.

## 5. Authority / Capability Model 계약

authority와 capability는 같은 것이 아니다.

## 5.1 capability

capability는 "무엇을 할 수 있는가"다.

예:

- ability contract
- token possession
- role-bound permission
- secure/device boundary capability

## 5.2 authority

authority는 "누가 승인하는가"다.

예:

- `authority subjectSlot`
- `authorized by actor`
- zone-local approval boundary

## 5.3 고정 원칙

- capability 없이 authority만 있으면 실행 불가
- authority 없이 capability만 있으면 승인 필요한 mutation은 불가
- authority는 보통 zone/world scoped
- capability는 type/role/token 기반으로 더 구조적

즉:

- capability = 실행 자격
- authority = 승인 주체

## 5.4 capability mode 축

최소 capability mode:

- `None`
- `TokenBound`
- `AuthorityBound`
- `RoleBound`
- `ZoneScoped`
- `WorldScoped`

## 6. 구현 순서 고정

앞으로 이 순서를 뒤집지 않는다.

1. HIR는 typed normalized tree + bridge view로 유지
2. DIR는 declaration/domain graph로 확장
3. RIR는 explicit resource op와 static ownership fact를 만든다
4. MIR는 CFG/SSA와 `RIR-flow` merge를 맡는다
5. backend는 MIR 또는 MIR-friendly lowering 결과를 받는다

## 7. 한 줄 결론

> Pergyra는 단순 transpiler가 아니라,  
> 도메인 의미론과 자원 의미론을 분리한 뒤 실행 의미론으로 내리는 언어다.
