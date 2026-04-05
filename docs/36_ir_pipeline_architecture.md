# Pergyra IR 파이프라인 설계

마지막 업데이트: 2026-04-06

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

현재 저장소는 완전히 이 구조로 넘어간 상태가 아니다.

- AST / Semantic / HIR / DIR / RIR / MIR / Backend dispatch는 실제로 동작 중
- 현재 구현 HIR는 목표 HIR 전체를 다 먹은 상태는 아니고, semantic typed AST 위의 indexed/pass-friendly view에 더 가깝다
- HIR는 이제 단순 bucket classifier를 넘어서 CFG/dominance/phi-skeleton까지 갖고 있음
- DIR는 `--dir`로 domain declaration graph를 볼 수 있고, compile driver는 backend dispatch 전에 항상 structural validation까지 수행한다.
- RIR는 `--rir`로 explicit resource op, static fact, normalized state summary, branch/join `flow-block[...]` lattice summary를 볼 수 있고, compile driver는 backend dispatch 전에 항상 lowering/enrich/validation을 수행한다. 최근에는 `relation/effect/zone/world` nominal handle을 function param, intent participant, `using`, `transfer` 경로에서 explicit fact/op로 정규화한다.
- MIR는 `--mir`로 routine/block/instruction/cleanup 구조를 볼 수 있고, compile driver는 backend dispatch 전에 항상 lowering/validation을 수행한다. 최근 패스로 phi materialization, instruction-level `def/use`, block entry/exit SSA version, cleanup convergence root, rollback/invalidation exceptional CFG까지 올라왔고, C backend에는 simple top-level function CFG subset을 MIR block/terminator에서 직접 emit하는 첫 vertical slice가 들어갔다. 다만 full liveness/DCE와 최종 MIR-level `RIR-flow` merge는 아직 없다.

즉 현재 구현 현실은:

```text
AST
-> Semantic typed AST
-> HIR
-> DIR
-> RIR
-> MIR
-> Backend dispatch
```

다만 backend emission의 중심 자료구조는 아직 `HIR`다. 현재 backend runner는 `CompilerIRBundle`을 받아 structural validation을 강제하고, C backend에는 simple top-level function CFG subset을 `bundle->mir`에서 직접 emit하는 첫 vertical slice가 들어가 있지만, 실제 C/LLVM codegen의 대부분은 아직 `bundle->hir`를 기준으로 수행한다.

장기 목표 구조는:

```text
AST
-> HIR
-> DIR
-> RIR
-> MIR
-> Backend
```

이다.

## 왜 HIR 하나로는 부족한가

Pergyra는 일반적인 expression language가 아니라:

- `subject/class/object/dto`
- `relation/effect/zone/world`
- `intent`
- `Slot/SecureSlot/DeviceSlot`
- projection / authority / lifecycle

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
