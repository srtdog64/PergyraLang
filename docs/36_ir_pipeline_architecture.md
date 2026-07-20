# Pergyra IR 파이프라인 설계

마지막 업데이트: 2026-07-20 (현재 상태 절 갱신 — MIR-only 이주와 Verified Projection Plan 반영)

이 문서는 "현재 저장소가 이미 그렇게 동작한다"는 설명이 아니라, 앞으로 Pergyra 컴파일러가 정리돼야 할 IR 계층 구조를 적는다.

세부 계약은 [`37_compiler_contracts.md`](./37_compiler_contracts.md)를 기준으로 한다.

## 목표 파이프라인

```text
AST (raw parse tree)
  ->
HIR (High-level IR)
  ->
DIR (Domain IR)
  ->
RIR (Resource IR)
  ->
MIR (Machine / Execution IR)
  ->
C / LLVM backend
```

## 계층별 책임

### AST

- parser 결과
- 표면 문법과 원래 선언/식 표현 유지

### HIR

역할:

- 언어 표면 정리
- syntactic sugar 제거
- import inline splice 흡수
- 이름 바인딩 완료
- 타입 추론/어노테이션 부착
- 문법 노이즈가 제거된 정규화 언어 트리
- `subject / zone / ability / intent` 같은 Pergyra 표면 개념은 아직 first-class로 유지

출력:

- 문법 노이즈가 제거된 정규화 언어 트리
- routine / CFG / dominance 같은 backend/pass-friendly indexed view

### DIR

역할:

- 도메인 관계 검증
- declaration 중심 graph 정규화
- flow-sensitive 분석 전에 Pergyra 존재론을 공통 연산으로 정리

예시:

- ability 계약 -> role 구현 완전성
- zone/world 멤버십
- party 합성 규칙
- relation/effect 의미 규칙
- intent participant / step dependency graph

출력:

- 도메인 관계 그래프
- declaration 단위 메타
- 아직 CFG나 수명 정보는 없음
- intent participant/step edge
- step predecessor dependency

### RIR

역할:

- 자원 의미론 해석
- 값 계산보다 "누가 무엇을 언제 합법적으로 쥐는가"를 표현
- CFG 이전에 확정 가능한 자원권 사실과 자원 전이 연산을 정규형으로 고정

예시:

- `Slot` claim/read/write/release
- `SecureSlot` token capability
- `QubitSlot` no-cloning / measured-state rule
- `RemoteFuture await` 의미론
- projection validity / sync state
- relation/effect attach/detach/lifecycle
- authority / role / capability binding
- intent compensation / rollback resource edge
- authority/capability의 `Authorized` / `AuthorityLost`
- projection의 `Synced` / `Dirty` / `Stale` / `Published`
- world handoff의 `HandoffPending` / `HandedOff`
- lifecycle rollback의 `Compensated`

출력:

- `Resource Graph + Transfer Ops + Static Ownership Facts`
- 자원 상태 전이 그래프
- explicit resource op / projection op / authority op / remote-boundary op / compensation hook
- CFG 무관하게 확정되는 자원권/유효성/소모 사실
- scope별 normalized state summary (`initial_state`, `final_state`, `last_op`, `transition error`)
- 복잡한 branch/join/loop/phi 병합 케이스는 MIR로 이월

RIR는 단순 "맵"이 아니다. 최소한 아래 연산은 표면 sugar가 아니라 explicit op로 정규화돼야 한다.

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
- `AwaitLocal`
- `AwaitRemote`
- `CommitIntent`
- `AbortIntent`
- `CompensateIntentStep`

### MIR

역할:

- 실행 구조 정규화
- CFG
- basic block
- SSA / phi
- liveness
- CFG 의존 자원 상태 merge
- intent 실행/보상 전체 경로 검증
- RIR-flow merge (`Slot`/projection/authority/resource state lattice)
- 상수 전파 / DCE 같은 전통 최적화

출력:

- backend 입력용 실행 IR

## 현재 상태

2026-07 기준 실제 구현 파이프라인은 다음과 같다:

```text
AST (parse)
-> Semantic typed AST
-> HIR / DIR / RIR
-> AIR (evidence DAG 검증)
-> MIR lowering / validation
-> AIR에 MIR evidence 결합 / 재검증 (lane fact 등 최종 분류)
-> Verified Projection Plan (검증된 사실의 유일한 백엔드 운반체)
-> C / LLVM backend
```

- **MIR-only 이주는 완료됐다 (2026-06-23).** 비-MIR AST-compat 선언 fallback
  3패밀리(decl/methods/slots)는 폐기됐고, driver는 `mir==NULL`이면 hard-fail
  한다. codegen 디렉터리는 HIR 프로그램을 소비하지 않는다 — 과거의
  "MIR 주도 + HIR 보조 하이브리드" 기술은 이 시점 이전의 상태다.
- **AIR는 off-path 검증 IR이다** (IRMinimality.v: codegen 최소 척추는
  HIR→RIR→MIR 3층). AIR는 boundary/evidence/intent 사실을 HIR·RIR에서
  수집해 DAG로 검증하고, MIR lowering 후 MIR evidence를 결합해 재검증한다
  (`air_collect_mir_evidence`, `air_refresh_execution_lane_facts`).
- **검증된 사실이 백엔드로 가는 유일한 통로는 Verified Projection Plan이다**
  (`src/compiler/verified_projection_plan.{h,c}`). driver가 인증된 AIR와 MIR로
  plan row(관측성 축 + machine-layer/target fingerprint)와 **spawn-lane
  plan**(per-site ExecutionLane fact, 2026-07-20)을 만들어 양 백엔드에 값으로
  넘긴다. 백엔드는 이 운반체 밖에서 AIR를 읽거나 source spelling에서 사실을
  재유도할 수 없다 (emitter 금지 pin + reachability 계약이 강제).
- DIR/RIR/MIR는 각각 `--dir`/`--rir`/`--mir`로 관찰 가능하고, compile driver는
  backend dispatch 전에 항상 lowering/validation을 수행한다. AIR는
  `--air-json`(pgy.air.graph.v1)으로 관찰 가능하다.

목표 구조와의 남은 거리:

```text
AST -> HIR -> DIR -> RIR -> MIR -> Backend
```

- HIR가 목표 HIR 전체(문법 노이즈 제거·이름 바인딩·타입 부착의 단일 홈)를
  다 먹지는 않았다 — semantic typed AST 위의 indexed/pass-friendly view에
  더 가깝고, semantic 단계가 여전히 별도 층으로 산다 (F2: pre-semantic
  declaration-field metadata layer가 docs/125의 남은 feature-work).
- evidence fact의 생산자 커버리지(정밀 capture plumbing)는 docs/146의
  Remaining 절이 원장이다. 소비 축은 reachability 계약이 감시한다.

## 왜 HIR 하나로는 부족한가

Pergyra는 일반적인 expression language가 아니라:

- `subject/class/object/tobject`
- `relation/effect/zone/world`
- `intent`
- `Slot/SecureSlot/DeviceSlot`
- projection / authority / lifecycle

특히 여기서 `slot`은 예제용 gimmick이 아니라 최저 공통 추상화다.

- resource fact는 slot anchor를 가진다
- projection / authority / capability도 slot anchor를 가진다
- relation / effect / zone / world handle도 slot anchor로 정규화된다
- intent `using` / `transfer` / `authorize`는 slot anchor를 유지한 op로 내려간다

를 코어 의미론으로 가진다.

따라서 단일 MIR만으로는:

- 도메인 존재론 정규화
- 자원권 해석
- 실행 CFG

를 한 층에 다 밀어넣게 된다.

그래서:

- `DIR`는 존재론/도메인 관계용
- `RIR`는 자원권/수명용
- `MIR`는 실행 구조/최적화용

으로 나눠야 한다.

## 시작 순서

권장 구현 순서:

1. HIR는 indexed frontend/pass view로 유지
2. DIR를 declaration graph 기준으로 도입
3. intent / zone / relation / effect부터 DIR lowering
4. Slot / projection / authority / lifecycle를 RIR로 내리기
5. CFG/SSA/liveness는 MIR에서 담당

## 즉시 해야 할 일

- DIR node/edge 스키마 확장
- DIR에서 intent step dependency graph 구체화
- DIR edge 확장
- RIR normalize/lattice propagation 고도화
- MIR phi merge / cleanup edge 정책 구현 심화
- MIR rename을 full def-use chain / liveness 단계까지 밀기
- MIR 도입 전 HIR의 역할 경계 명시

## Rust와 비교 가능한 지점

이 파이프라인은 곧바로 "Rust와 경쟁"을 의미하지 않는다. 다만 다음을 만족해야 비로소 Rust와 비교 가능한 출발선에 선다.

- 자원 규칙이 backend에 숨어 있지 않음
- flow-sensitive resource checking이 MIR 위에서 가능함
- domain 규칙(DIR)과 resource 규칙(RIR)이 분리됨
- phi merge / cleanup / invalidation / compensation 경로가 명시적임

즉 이 구조는 "경쟁 가능한 아키텍처"를 만들고, 그 위에 alias model / region model / diagnostics / tooling / spec 안정화가 더 얹혀야 "사용 가능한 경쟁력"이 된다.
