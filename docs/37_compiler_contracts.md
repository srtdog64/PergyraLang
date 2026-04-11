# Pergyra 컴파일러 계약 고정안

마지막 업데이트: 2026-04-06

이 문서는 앞으로 흔들리면 안 되는 다섯 가지 핵심 계약을 고정한다.

- `HIR / DIR / RIR / MIR` 계층 책임
- resource state lattice
- intent compensation model
- projection sync semantics
- authority / capability model

그리고 아래 두 축도 함께 고정한다.

- 각 핵심 키워드가 어느 IR 계층에서 최종 확정되는가
- 각 키워드가 표면 문법 표식인지, 실행 의미론을 가지는지

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
- `subject / class / object / tobject / ability / role / relation / effect / zone / world / intent`를 first-class로 유지

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
- slot-contract graph

DIR의 slot-contract graph는 최소한 아래 네 계약을 분리해야 한다.

- `party slot`
  - 협력/역할 조합 계약
  - ability 요구를 통해 role 조합의 최소 조건을 선언한다
- `zone slot`
  - state/authority/lifecycle 경계 안의 host slot 계약
  - subject/vessel/class host를 zone execution boundary에 배치한다
- `projection slot`
  - `object/tobject` projection target 계약
  - source slot과 `refresh/publish/bind` declaration을 통해 읽기 모델 vs 경계 전송 모델을 구분한다
- `authority slot`
  - zone 안에서 어떤 subject slot이 어떤 ability를 통해 mutation authority를 가지는지 선언한다

최소 edge 예시:

- `role -> for type`
- `role -> impl ability`
- `party -> party slot`
- `party slot -> required ability`
- `world -> zone`
- `zone -> zone slot`
- `owner(relation/effect/zone) -> projection slot`
- `projection slot -> source slot`
- `zone -> authority slot`
- `authority slot -> subject slot`
- `authority slot -> ability`
- `intent participant -> bound type`
- `intent step -> zone`
- `intent step -> who participant alias`
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

그리고 이 op/fact/state는 가능한 한 `slot`을 공통 anchor로 가져야 한다.

- `Slot / SecureSlot / DeviceSlot / QubitSlot / RemoteFuture`는 직접 slot anchor다
- projection fact는 projection target slot을 anchor로 가진다
- authority / capability fact는 승인 주체 subject slot을 anchor로 가진다
- relation / effect / zone / world handle fact도 해당 layer/zone slot 이름을 anchor로 가진다
- intent `using:` / `transfer:` / `authorize:` op는 source/target/participant slot anchor를 잃지 않아야 한다

즉 RIR는 "slot을 포함하는 일부 기능"이 아니라, 가능한 모든 자원 의미를 slot anchor 위에 정규화하는 계층이어야 한다.

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
- flow block은 resource fact가 없어도 버리지 않고, 최소한 `authority`, `projection`, `world-handoff`, `invalidation`, `authority-loss`, `projection-invalidation` conservative semantic flag를 `sem-entry/sem-exit`로 보존한다.
- handle merge는 resource kind를 함께 읽는다.
  - `zone/world handle`은 ownership/borrow 중심
  - `relation/effect handle`은 detach/sync/dirty lifecycle 중심
- authority/capability는 `AuthorityHandle` / `CapabilityToken` kind와 `Authorized` / `AuthorityLost` 상태로 분리한다.
- projection은 `Synced/Dirty/Published`뿐 아니라 `Stale` 상태로 invalidation 이후의 보수 상태를 남긴다.
- `Published`는 `tobject` projection 전용 boundary state다. `object` projection은 `Published`로 승격될 수 없다.
- `bind`는 target slot kind를 따른다. object slot이면 internal `refresh` contract, tobject slot이면 boundary `publish` contract로 확정된다.
- zone/world handoff는 `HandoffPending` / `HandedOff` 상태로 요약될 수 있어야 한다.
- relation/effect rollback은 `Compensated` 상태를 통해 detach와 구분될 수 있어야 한다.
- fact/op/state summary/flow fact는 이제 모두 slot anchor를 보존하며, validator도 `intent-policy`를 제외한 모든 fact/op와 모든 state summary/flow fact에 slot anchor가 있음을 요구한다

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
- `resource-op` / `cleanup` instruction은 matching `RIR` op의 `slot_anchor`를 그대로 유지
- `def` / `phi`와 routine-level value summary도 base local 이름을 `slot_anchor`로 유지
- validator는 cleanup/resource instruction과 value summary가 slot anchor를 잃는 것을 허용하지 않는다
- block별 `ssa_entry_versions` / `ssa_exit_versions`를 저장해 phi incoming과 use edge가 predecessor exit map을 직접 참조한다
- reachable intent block마다 cleanup successor edge를 두고, cleanup convergence root 아래에 rollback block과 invalidation block을 분리한다
- routine-level value summary를 만들어 def/use/live/cleanup 도달 여부를 후속 pass가 재사용할 수 있게 한다
- lowering 안에서 실제 liveness 재계산과 dead `def/phi` 제거 DCE pass를 수행한다

즉 MIR는 아직 full optimizer IR은 아니지만, pure placeholder를 넘어서
`phi`, versioned local value, instruction-level use, routine-level value summary, cleanup convergence, rollback/invalidation edge를 직접 가지는 실행 그래프다.

## 1.6 키워드별 확정 계층

이 표는 "이 키워드의 최종 책임 계층"을 잠근다.

- `도입`: parser/HIR가 표면 문법으로 인식하는 계층
- `정합성`: 선언/계약이 구조적으로 확정되는 계층
- `자원`: slot/resource/capability/lifecycle 의미가 확정되는 계층
- `흐름`: CFG/cleanup/phi/rollback path를 포함한 실행 의미가 확정되는 계층

| 키워드/개념 | 도입 | 정합성 확정 | 자원 의미 확정 | 흐름 의미 확정 |
| --- | --- | --- | --- | --- |
| `subject` | HIR | DIR | RIR | MIR |
| `class` | HIR | HIR | - | - |
| `struct` | HIR | HIR | - | - |
| `vessel` | HIR | DIR | RIR | MIR |
| `object` | HIR | DIR | RIR | MIR |
| `tobject` | HIR | DIR | RIR | MIR |
| `ability` | HIR | DIR | - | - |
| `role` | HIR | DIR | RIR | - |
| `party` | HIR | DIR | RIR | MIR |
| `relation` | HIR | DIR | RIR | MIR |
| `effect` | HIR | DIR | RIR | MIR |
| `zone` | HIR | DIR | RIR | MIR |
| `world` | HIR | DIR | RIR | MIR |
| `slot` (`Slot/SecureSlot/...`) | HIR | HIR | RIR | MIR |
| projection (`refresh/publish/bind`) | HIR | DIR | RIR | MIR |
| authority (`authority`, `authorized by`) | HIR | DIR | RIR | MIR |
| capability (`ability`/token/role-bound permission) | HIR | DIR | RIR | MIR |
| `intent` | HIR | DIR | RIR | MIR |
| `compensate` / `rollback` | HIR | DIR | RIR | MIR |
| `using` / `transfer` | HIR | DIR | RIR | MIR |

## 1.6.A 실행 계약 표

이 표는 concurrency / execution family의 역할을 고정한다.

핵심 원칙:

- `parallel`은 core execution primitive다.
- `async`는 suspension/coroutine surface다.
- `spawn`은 task-producing surface다.
- `await`는 completion join surface다.
- `select`는 readiness arbitration surface다.

즉:

- `parallel`은 “동시에 살아도 되는 실행 관계”를 정한다.
- `async`는 “그 실행 하나가 멈췄다가 재개될 수 있는가”를 정한다.

| 표면 | 역할 | 최종 고정 계층 | 직접 바꾸는 것 | 비고 |
| --- | --- | --- | --- | --- |
| `parallel` | core execution primitive | `HIR -> MIR` | 동시 실행 관계, slot/resource conflict, join/cancel/fairness family | execution 최상위 축 |
| `spawn` | task-producing surface | `HIR -> MIR` | 새 task 생성, future 반환 | `parallel` 아래 surface |
| `async` | suspension/coroutine surface | `HIR -> MIR` | suspend/resume, async context | 병렬성 그 자체는 아님 |
| `await` | completion join surface | `HIR -> RIR -> MIR` | future/result 합류, remote future unwrap | async completion 관측 |
| `select` | readiness arbitration surface | `HIR -> MIR` | ready case 선택, fairness 시작점 | channel/parallel 하위 surface |
| `channel` | execution dataflow surface | `HIR -> RIR -> MIR` | send/recv/backpressure | `select`와 함께 사용 |
| `cancel` family | execution control surface | `HIR -> MIR` | cancellation chain, cooperative stop | runtime propagation contract |

한 줄 정의:

> `parallel`은 실행 관계의 코어이고, `async/spawn/await/select`는 그 관계 위에서 동작하는 표면이다.

## 1.6.1 전체 키워드 인벤토리

위 표는 "backend/IR 책임이 큰 키워드" 중심이다.
하지만 migration과 구현 점검을 위해서는 실제 lexer/contextual keyword 전체 목록도 고정해야 한다.

이 절은 현재 언어가 문법적으로 인식하는 키워드를 빠짐없이 적어 둔 인벤토리다.

### 예약 키워드

선언 / 타입 / 도메인:

- `let`
- `func`
- `class`
- `subject`
- `struct`
- `tobject`
- `enum`
- `type`
- `ability`
- `role`
- `party`
- `subject`
- `channel`

모듈 / 가시성 / 선언 수식:

- `import`
- `use`
- `export`
- `namespace`
- `extern`
- `public`
- `private`
- `where`
- `as`
- `impl`
- `include`
- `fields`
- `override`
- `extends`

제어 흐름 / 실행:

- `if`
- `else`
- `for`
- `in`
- `while`
- `return`
- `break`
- `continue`
- `match`
- `case`
- `default`
- `with`
- `parallel`
- `async`
- `await`
- `spawn`
- `select`
- `defer`
- `unsafe`

리소스 / ownership / dispatch:

- `bind`
- `secure`
- `slot`
- `shared`
- `dyn`
- `own`
- `ref`

리터럴:

- `true`
- `false`

### 컨텍스트 키워드

도메인 / host:

- `object`
- `vessel`
- `relation`
- `effect`
- `zone`
- `roster`
- `world`
- `event`
- `action`

intent / orchestration:

- `intent`
- `involves`
- `step`
- `who`
- `using`
- `where`          ← intent의 `where: ZoneName;` 절. lexer는 TOKEN_WHERE로 토크나이징하며, 문맥 식별자가 아님
- `requires`
- `authorized`
- `by`
- `within`
- `causes`
- `expect`
- `success`
- `failure`
- `rollback`
- `cleanup`
- `compensate`
- `exclusive`
- `concurrent`
- `priority`
- `on`
- `pre`
- `guard`
- `post`
- `invariant`
- `transfer`

zone / world / projection / authority surface:

- `refresh`
- `publish`
- `authority`
- `apply`
- `detach`
- `link`
- `unlink`
- `maintain`
- `state`
- `layer`
- `projection`
- `between`
- `from`
- `to`
- `capacity`
- `pool`
- `activate`
- `deactivate`

### 규칙

- 이 인벤토리에 있는 키워드는 문서/semantic/parser/backend 계약의 관리 대상이다.
- 새 키워드를 추가할 때는 이 절과 `1.6 키워드별 확정 계층`을 함께 갱신해야 한다.
- 모든 키워드가 같은 무게를 가지는 것은 아니다.
- syntax-only 키워드도 인벤토리에는 남겨야 하며, IR 책임표에는 필요할 때만 올린다.
- `lexer` 예약 키워드와 `parser` contextual keyword가 어긋나면 버그로 본다.

## 1.6.2 키워드 Taxonomy

키워드 인벤토리만으로는 충분하지 않다.
실제 구현과 리팩터링에서는 "이 단어가 어느 층에서 키워드인가"를 구분해야 한다.

Pergyra는 키워드를 아래 네 층으로 분류한다.

### A. Reserved Token Keyword

정의:

- lexer 단계에서 식별자가 아니라 전용 토큰으로 고정되는 키워드
- parser는 이 토큰을 직접 소비한다
- 이름이 같아도 일반 identifier로 사용할 수 없다

예시:

- `let`
- `func`
- `class`
- `subject`
- `struct`
- `tobject`
- `where`
- `ability`
- `role`
- `party`
- `async`
- `await`
- `spawn`

판정 규칙:

- `lexer.c` keyword table에 직접 들어가 있으면 이 분류다
- 문맥에 따라 의미가 달라질 수는 있어도, 토큰은 예약되어 있다

주의:

- `where`는 이 분류다
- intent step의 `where:` clause에서 쓰이더라도 contextual identifier keyword가 아니라 reserved token의 문법 재사용이다

### B. Declaration-Context Keyword

정의:

- lexer는 일반 `TOKEN_IDENTIFIER`로 두지만
- parser가 declaration 시작 문맥에서 특정 식별자 문자열을 특별 취급하는 키워드

예시:

- `object`
- `vessel`
- `intent`
- `world`
- `roster`
- `roster`
- `relation`
- `effect`
- `zone`
- `event`

판정 규칙:

- parser가 `parser_match_contextual_keyword(...)` 또는 유사 helper로 소비하면 이 분류다
- 선언 시작 위치가 아니면 일반 identifier로 남을 수 있다

주의:

- 현재 구현에서 `object`와 `tobject`는 모두 reserved token이며, 차이는 token이 아니라 projection contract다

### C. Clause Keyword

정의:

- 특정 declaration/body 안에서 clause head 또는 clause connector로 쓰이는 키워드
- parser가 dedicated clause parser나 body parser에서 문법적으로 해석한다
- token 단계 예약일 수도 있고 아닐 수도 있다

예시:

- `where`
- `who`
- `using`
- `requires`
- `authorized`
- `by`
- `success`
- `failure`
- `rollback`
- `cleanup`
- `compensate`
- `transfer`
- `on`
- `pre`
- `guard`
- `post`
- `invariant`
- `refresh`
- `publish`
- `bind`
- `apply`
- `detach`
- `link`
- `unlink`
- `maintain`
- `authority`
- `state`
- `layer`
- `projection`

판정 규칙:

- body 내부에서 `keyword:` 또는 `keyword ...` clause head로 쓰이면 이 분류다
- declaration-context keyword와 중복될 수 있고, reserved token과도 중복될 수 있다

중요:

- 하나의 단어가 여러 taxonomy에 동시에 속할 수 있다
- 예: `where`
  - token 층에서는 reserved token
  - 문법 층에서는 clause keyword

### D. Semantic-Contract Keyword

정의:

- parser 인식만으로 끝나지 않고
- `HIR / DIR / RIR / MIR` 중 하나 이상에서 고정 계약을 가지는 키워드

예시:

- `object`
- `tobject`
- `slot`
- `refresh`
- `publish`
- `bind`
- `authority`
- `authorized by`
- `intent`
- `step`
- `rollback`
- `compensate`
- `using`
- `transfer`
- `zone`
- `world`

판정 규칙:

- declaration shape를 넘어서 IR 책임표에 올라가면 이 분류다
- backend, validator, runtime observability, cleanup path 중 하나라도 직접 영향을 주면 semantic-contract keyword다

### 핵심 원칙

- 모든 reserved token이 semantic-contract keyword는 아니다
- 모든 declaration-context keyword가 runtime 의미를 가지는 것은 아니다
- clause keyword는 syntax 층 분류이고, 별도로 semantic-contract 여부를 판정해야 한다
- 문서/코드에서 "contextual keyword"라는 표현을 쓸 때는
  - token-reserved인지
  - declaration-context인지
  - clause-level인지
  를 구분해서 써야 한다

### 현재 주의 대상

- `subject` / `class`
  - lexer token split은 닫혔지만 semantic/runtime 계약과 진단 톤 차이는 계속 관리해야 한다
- `tobject` / `struct`
  - lexer token split은 닫혔지만 projection/boundary contract 설명 밀도는 더 올려야 한다
- `object`
  - reserved token으로 올라왔고, 이제 핵심은 token이 아니라 projection contract 설명 일관성이다
- `where`
  - reserved token이면서 clause keyword다
- `intent`
  - reserved token이면서 semantic-contract keyword다

한 줄 요약:

> 키워드는 "토큰", "선언 시작", "절 문법", "의미론 계약"의 네 층으로 분리해서 봐야 한다.

## 1.6.3 핵심 키워드 분류표

아래 표는 실제 migration과 compiler work에서 먼저 붙잡아야 하는 핵심 키워드만 추린 것이다.

| 키워드 | Token 분류 | Parser 분류 | 계약 무게 | 최종 고정 계층 | 메모 |
| --- | --- | --- | --- | --- | --- |
| `subject` | reserved (`TOKEN_SUBJECT`) | declaration | 매우 큼 | `HIR -> DIR -> RIR -> MIR` | active host / participant type |
| `class` | reserved (`TOKEN_CLASS`) | declaration | 중간 | `HIR` 중심 | passive nominal host |
| `struct` | reserved (`TOKEN_STRUCT`) | declaration | 낮음 | `HIR` 중심 | pure value type |
| `object` | reserved (`TOKEN_OBJECT`) | declaration | 큼 | `HIR -> DIR -> RIR -> MIR` | local/internal projection contract |
| `tobject` | reserved (`TOKEN_TOBJECT`) | declaration | 큼 | `HIR -> DIR -> RIR -> MIR` | transfer/boundary projection contract |
| `vessel` | reserved (`TOKEN_VESSEL`) | declaration | 중간 | `HIR -> DIR -> RIR -> MIR` | subject 내부 상태 수용체 |
| `ability` | reserved | declaration | 중간 | `HIR -> DIR` | capability contract surface |
| `role` | reserved | declaration | 큼 | `HIR -> DIR -> RIR` | ability 구현과 binding |
| `party` | reserved | declaration | 큼 | `HIR -> DIR -> RIR -> MIR` | collaboration slot contract |
| `relation` | reserved (`TOKEN_RELATION`) | declaration | 큼 | `HIR -> DIR -> RIR -> MIR` | shared relation/lifecycle |
| `effect` | reserved (`TOKEN_EFFECT`) | declaration | 큼 | `HIR -> DIR -> RIR -> MIR` | attached state/lifecycle |
| `zone` | reserved (`TOKEN_ZONE`) | declaration | 매우 큼 | `HIR -> DIR -> RIR -> MIR` | execution / authority boundary |
| `world` | reserved (`TOKEN_WORLD`) | declaration | 매우 큼 | `HIR -> DIR -> RIR -> MIR` | top execution boundary |
| `intent` | reserved (`TOKEN_INTENT`) | declaration + clause host | 매우 큼 | `HIR -> DIR -> RIR -> MIR` | orchestration contract root |
| `step` | identifier | clause keyword | 매우 큼 | `DIR -> RIR -> MIR` | intent execution unit |
| `where` | reserved (`TOKEN_WHERE`) | clause keyword | 큼 | `HIR -> DIR -> MIR` | reserved token reused in generics and intent clauses |
| `using` | identifier | clause keyword | 큼 | `HIR -> DIR -> RIR -> MIR` | live zone-instance binding |
| `transfer` | identifier | clause keyword | 큼 | `HIR -> DIR -> RIR -> MIR` | cross-boundary handoff |
| `rollback` | identifier | clause keyword | 큼 | `DIR -> RIR -> MIR` | cleanup policy root |
| `compensate` | identifier | clause keyword | 큼 | `DIR -> RIR -> MIR` | reverse-order recovery path |
| `authority` | identifier | clause keyword | 큼 | `DIR -> RIR -> MIR` | zone mutation authority declaration |
| `authorized by` | identifier pair | clause keyword | 큼 | `DIR -> RIR -> MIR` | subject-alias authorization binding |
| `refresh` | identifier | clause keyword | 큼 | `DIR -> RIR -> MIR` | object projection sync |
| `publish` | identifier | clause keyword | 큼 | `DIR -> RIR -> MIR` | tobject boundary sync |
| `bind` | reserved | clause/declaration keyword | 큼 | `HIR -> DIR -> RIR -> MIR` | target slot kind-sensitive projection contract |
| `slot` | reserved | declaration/type keyword | 매우 큼 | `HIR -> RIR -> MIR` | common resource anchor |

읽는 법:

- `Token 분류`는 lexer가 전용 토큰으로 고정했는지 여부다
- `Parser 분류`는 declaration-context인지 clause keyword인지 보여준다
- `계약 무게`는 migration 우선순위를 의미한다
- `최종 고정 계층`은 backend가 의존해야 하는 마지막 IR 경계를 뜻한다

## 1.6.4 전체 키워드 매트릭스

이 절은 "현재 언어가 인식하는 키워드 전체"를 표로 고정한다.
목적은 단순 문법 목록이 아니라, 각 키워드가 어느 층의 책임을 가지는지 한눈에 보이게 하는 것이다.

### A. Reserved Token Keywords

| 키워드 | Lexer 토큰 | Parser 역할 | 계약 무게 | 최종 고정 계층 | 비고 |
| --- | --- | --- | --- | --- | --- |
| `let` | `TOKEN_LET` | declaration / statement | 중간 | `HIR` | local binding |
| `func` | `TOKEN_FUNC` | declaration | 큼 | `HIR -> MIR` | routine root |
| `class` | `TOKEN_CLASS` | declaration | 중간 | `HIR` | passive nominal host |
| `subject` | `TOKEN_SUBJECT` | declaration | 매우 큼 | `HIR -> DIR -> RIR -> MIR` | active host type |
| `struct` | `TOKEN_STRUCT` | declaration | 낮음 | `HIR` | pure value type |
| `tobject` | `TOKEN_TOBJECT` | declaration | 큼 | `HIR -> DIR -> RIR -> MIR` | boundary projection |
| `extern` | `TOKEN_EXTERN` | declaration | 낮음 | `HIR` | foreign block surface |
| `with` | `TOKEN_WITH` | statement / clause | 중간 | `HIR -> RIR -> MIR` | scoped resource binding |
| `as` | `TOKEN_AS` | type / alias / clause | 낮음 | `HIR` | naming/typing helper |
| `parallel` | `TOKEN_PARALLEL` | statement | 큼 | `HIR -> MIR` | core execution primitive |
| `for` | `TOKEN_FOR` | statement | 중간 | `HIR -> MIR` | loop surface |
| `in` | `TOKEN_IN` | statement / type syntax | 낮음 | `HIR` | iterator/member syntax helper |
| `if` | `TOKEN_IF` | statement | 중간 | `HIR -> MIR` | branch surface |
| `else` | `TOKEN_ELSE` | statement | 낮음 | `HIR -> MIR` | branch continuation |
| `while` | `TOKEN_WHILE` | statement | 중간 | `HIR -> MIR` | loop surface |
| `return` | `TOKEN_RETURN` | statement | 중간 | `HIR -> MIR` | routine exit |
| `break` | `TOKEN_BREAK` | statement | 중간 | `HIR -> MIR` | loop exit |
| `continue` | `TOKEN_CONTINUE` | statement | 중간 | `HIR -> MIR` | loop back-edge |
| `enum` | `TOKEN_ENUM` | declaration | 중간 | `HIR` | tagged/value sum type |
| `export` | `TOKEN_EXPORT` | declaration modifier | 낮음 | `HIR` | module surface |
| `namespace` | `TOKEN_NAMESPACE` | declaration | 낮음 | `HIR` | module grouping |
| `true` | `TOKEN_TRUE` | literal | 낮음 | `HIR` | boolean literal |
| `false` | `TOKEN_FALSE` | literal | 낮음 | `HIR` | boolean literal |
| `public` | `TOKEN_PUBLIC` | declaration modifier | 낮음 | `HIR` | visibility |
| `private` | `TOKEN_PRIVATE` | declaration modifier | 낮음 | `HIR` | visibility |
| `where` | `TOKEN_WHERE` | clause / generic constraint | 큼 | `HIR -> DIR -> MIR` | reserved token reused by generic and intent/action clauses |
| `type` | `PGY_TOKEN_TYPE` | declaration | 중간 | `HIR` | alias/type declaration |
| `impl` | `TOKEN_IMPL` | declaration helper | 중간 | `HIR -> DIR` | role/ability implementation |
| `async` | `TOKEN_ASYNC` | declaration / statement | 중간 | `HIR -> MIR` | suspension/coroutine surface |
| `await` | `TOKEN_AWAIT` | expression | 중간 | `HIR -> RIR -> MIR` | future synchronization |
| `subject` | reserved (`TOKEN_CLASS`) | declaration | 매우 큼 | `HIR -> DIR -> RIR -> MIR` | active host type |
| `channel` | `TOKEN_CHANNEL` | type/declaration surface | 중간 | `HIR -> RIR -> MIR` | channel resource surface |
| `select` | `TOKEN_SELECT` | statement | 중간 | `HIR -> MIR` | readiness arbitration surface |
| `case` | `TOKEN_CASE` | statement | 낮음 | `HIR -> MIR` | match/select arm |

메모:

- `export`는 module-boundary modifier다.
- `public/private`는 nominal/member visibility modifier다.
- `ability`는 기본 공개 계약이므로 `export ability`는 중복 표기이며, 숨김은 `private ability`로 표현한다.
| `default` | `TOKEN_DEFAULT` | statement | 낮음 | `HIR -> MIR` | fallback arm |
| `spawn` | `TOKEN_SPAWN` | expression | 중간 | `HIR -> MIR` | task-producing surface |
| `match` | `TOKEN_MATCH` | statement / expression | 중간 | `HIR -> MIR` | pattern dispatch |
| `import` | `TOKEN_IMPORT` | declaration | 낮음 | `HIR` | module import |
| `use` | `TOKEN_USE` | declaration surface | 낮음 | `HIR` | module binding helper |
| `unsafe` | `TOKEN_UNSAFE` | statement modifier | 낮음 | `HIR` | unsafe region marker |
| `defer` | `TOKEN_DEFER` | statement | 중간 | `HIR -> MIR` | deferred cleanup surface |
| `bind` | `TOKEN_BIND` | clause / declaration | 큼 | `HIR -> DIR -> RIR -> MIR` | projection contract by target kind |
| `ability` | `TOKEN_ABILITY` | declaration | 중간 | `HIR -> DIR` | capability contract |
| `role` | `TOKEN_ROLE` | declaration | 큼 | `HIR -> DIR -> RIR` | ability implementation/binding |
| `include` | `TOKEN_INCLUDE` | declaration clause | 낮음 | `HIR -> DIR` | role composition helper |
| `fields` | contextual identifier | ability declaration clause | 낮음 | `HIR -> DIR` | ability host-field contract |
| `override` | `TOKEN_OVERRIDE` | declaration modifier | 낮음 | `HIR` | method override surface |
| `secure` | `TOKEN_SECURE` | type/resource modifier | 중간 | `HIR -> RIR -> MIR` | secure resource surface |
| `party` | `TOKEN_PARTY` | declaration | 큼 | `HIR -> DIR -> RIR -> MIR` | collaboration contract |
| `slot` | `TOKEN_SLOT` | declaration / type keyword | 매우 큼 | `HIR -> RIR -> MIR` | common resource anchor |
| `shared` | `TOKEN_SHARED` | declaration/body keyword | 중간 | `HIR -> DIR -> RIR` | host-local contextual state |
| `extends` | `TOKEN_EXTENDS` | declaration clause | 낮음 | `HIR` | inheritance/type relation |
| `dyn` | `TOKEN_DYN` | type modifier | 중간 | `HIR -> DIR -> RIR` | dynamic dispatch surface |
| `own` | `TOKEN_OWN` | parameter/type modifier | 중간 | `HIR -> RIR` | ownership mode |
| `ref` | `TOKEN_REF` | parameter/type modifier | 중간 | `HIR -> RIR` | reference mode |

### B. Declaration-Context Keywords

| 키워드 | Lexer 토큰 | Parser 역할 | 계약 무게 | 최종 고정 계층 | 비고 |
| --- | --- | --- | --- | --- | --- |
| `object` | `TOKEN_IDENTIFIER` | declaration-context | 큼 | `HIR -> DIR -> RIR -> MIR` | local/internal projection contract |
| `vessel` | `TOKEN_IDENTIFIER` | declaration-context | 중간 | `HIR -> DIR -> RIR -> MIR` | subject internal state vessel |
| `intent` | `TOKEN_IDENTIFIER` | declaration-context | 매우 큼 | `HIR -> DIR -> RIR -> MIR` | orchestration declaration root |
| `world` | `TOKEN_IDENTIFIER` | declaration-context | 매우 큼 | `HIR -> DIR -> RIR -> MIR` | top execution boundary |
| `roster` | `TOKEN_IDENTIFIER` | declaration-context | 낮음 | `HIR -> DIR` | roster alias surface |
| `roster` | `TOKEN_IDENTIFIER` | declaration-context | 중간 | `HIR -> DIR -> RIR -> MIR` | deprecated but active host keyword |
| `relation` | `TOKEN_IDENTIFIER` | declaration-context | 큼 | `HIR -> DIR -> RIR -> MIR` | relation contract root |
| `effect` | `TOKEN_IDENTIFIER` | declaration-context | 큼 | `HIR -> DIR -> RIR -> MIR` | effect contract root |
| `zone` | `TOKEN_IDENTIFIER` | declaration-context | 매우 큼 | `HIR -> DIR -> RIR -> MIR` | execution / authority boundary |
| `event` | `TOKEN_IDENTIFIER` | declaration-context | 중간 | `HIR -> MIR` | event declaration surface |

### C. High-Value Clause Keywords

이 표는 전체 clause 키워드 중 compiler contract에 직접 영향이 큰 것만 다시 뽑은 것이다.

| 키워드 | Token 형태 | 주 사용 문맥 | 계약 무게 | 최종 고정 계층 | 비고 |
| --- | --- | --- | --- | --- | --- |
| `where` | reserved token | generic constraint / action / intent step | 큼 | `HIR -> DIR -> MIR` | zone or type constraint |
| `who` | identifier keyword | intent step | 큼 | `DIR -> RIR -> MIR` | participant binding |
| `using` | identifier keyword | intent step | 큼 | `DIR -> RIR -> MIR` | live zone instance sync |
| `requires` | identifier keyword | action / intent / authority | 큼 | `DIR -> RIR` | ability requirement |
| `authorized by` | identifier pair | action / intent / zone ops | 큼 | `DIR -> RIR -> MIR` | authority participant binding |
| `transfer` | identifier keyword | intent step | 큼 | `DIR -> RIR -> MIR` | cross-boundary handoff |
| `success` | identifier keyword | intent | 중간 | `DIR -> MIR` | success exit contract |
| `failure` | identifier keyword | intent | 중간 | `DIR -> MIR` | failure exit contract |
| `rollback` | identifier keyword | intent | 큼 | `DIR -> RIR -> MIR` | compensation policy |
| `cleanup` | identifier keyword | intent/runtime docs | 중간 | `RIR -> MIR` | exceptional cleanup path |
| `compensate` | identifier keyword | intent step | 큼 | `DIR -> RIR -> MIR` | reverse recovery op set |
| `refresh` | identifier keyword | relation/effect/zone | 큼 | `DIR -> RIR -> MIR` | object projection sync |
| `publish` | identifier keyword | relation/effect/zone | 큼 | `DIR -> RIR -> MIR` | tobject boundary sync |
| `authority` | identifier keyword | zone | 큼 | `DIR -> RIR -> MIR` | mutation authority declaration |
| `state` | identifier keyword | zone / world | 큼 | `DIR -> RIR -> MIR` | derived state contract |
| `layer` | identifier keyword | world state suffix / zone docs | 중간 | `DIR -> RIR -> MIR` | relation/effect layer contract |

### 사용 규칙

- 새 키워드를 추가할 때는 이 절을 함께 갱신한다
- 기존 키워드의 `Lexer 토큰`과 `Parser 역할`이 바뀌면 breaking parser contract로 본다
- `계약 무게`가 `큼` 이상인 키워드는 backend migration과 IR validation에서 별도 추적 대상이다
- `subject`, `object`, `tobject`, `zone`, `world`, `intent`, `slot`은 핵심 축이므로 임시 alias 취급을 금지한다

규칙:

- `ability / role`의 계약 완전성은 `DIR`에서 잠긴다.
- `slot`은 `RIR`에서 공통 anchor로 잠긴다.
- projection validity는 `RIR`에서 상태로, `MIR`에서 cleanup/merge path로 잠긴다.
- authority/capability는 `DIR`에서 선언 정합성을, `RIR`에서 실제 자원/권한 상태를 잠근다.
- intent compensation은 `DIR`에서 step graph를, `RIR`에서 compensation resource edge를, `MIR`에서 full cleanup/rollback path를 잠근다.

한 줄 요약:

> HIR는 표면을 정리하고, DIR는 계약을 잠그고, RIR는 자원 의미를 잠그고, MIR는 실행 경로를 잠근다.

## 1.7 표면 문법 vs 실행 의미론

모든 키워드가 같은 종류는 아니다. 어떤 것은 표면 분류 표식에 가깝고, 어떤 것은 실제 runtime/codegen 의미를 가진다.

### 표면 문법 중심

아래는 주로 존재론 분류나 선언 표식이다. runtime helper나 cleanup graph를 직접 만들 책임은 없다.

- `class`
- `struct`
- `ability`
- `role`
- `party` 선언 자체

이들은 주로 `HIR/DIR`에서 의미가 대부분 결정된다.

### 표면 + 자원 의미론

아래는 문법 표식이면서 동시에 `RIR`에서 실제 자원/lifecycle 의미를 갖는다.

- `vessel`
- `object`
- `tobject`
- `slot`
- projection (`refresh`, `publish`, `bind`)
- authority / capability
- `relation`
- `effect`
- `zone`
- `world`

이들은 runtime에 무거운 heap graph를 남기는 것이 목적이 아니라, 컴파일러가 자원 계약을 정규화하기 위한 표면이어야 한다.

### 표면 + 실행 의미론

아래는 `MIR` cleanup/CFG/rollback까지 포함하는 실행 단위다.

- `intent`
- `step`
- `on`
- `pre`
- `guard`
- `post`
- `success`
- `failure`
- `compensate`
- `rollback`
- `using`
- `transfer`
- `intent:` subintent orchestration

이들은 단순 annotation이 아니라 실제 실행 경로를 만든다.

규칙:

- 표면 문법 중심 키워드는 backend가 별도 VM 객체를 만들면 안 된다.
- 자원 의미론 키워드는 가능한 한 `RIR` fact/op/state로 정규화되어야 한다.
- 실행 의미론 키워드는 `MIR` CFG/cleanup/rollback edge로 떨어져야 한다.
- "문법만 있고 의미론이 없는 키워드"와 "runtime까지 가는 키워드"를 혼합하면 안 된다.

위험 신호:

- `slot`이 런타임 무거운 객체로 남는 경우
- projection이 반응형 VM처럼 비대해지는 경우
- authority/capability가 정적으로 제거 가능해도 전부 런타임까지 들고 가는 경우
- `intent`가 작은 orchestration 단위를 넘어서 범용 workflow VM처럼 커지는 경우

한 줄 요약:

> 키워드는 "문법 표식", "자원 의미", "실행 의미" 중 어디까지 책임지는지 반드시 고정돼야 한다.

## 1.8 직교성 경계

Pergyra는 존재론이 많기 때문에, 서로 다른 축의 책임이 겹치지 않도록 아래 경계를 고정한다.

### Party vs Zone / Vessel

- `party`는 협력 슬롯과 역할 조합을 다룬다.
- `vessel`은 subject 내부 상태 수용체다.
- `zone/world`는 상태 전이, authority, projection, lifecycle의 실행 경계다.

따라서:

- `party`는 "누가 어떤 역할로 협력하는가"를 잠근다.
- `zone/world`는 "어디서 어떤 상태 전이가 합법인가"를 잠근다.
- `vessel`은 "subject 안의 상태가 어디 담기는가"를 잠근다.

`party`는 state boundary가 아니다.
`zone/world`는 collaboration roster가 아니다.

실무 감각:

- subject 둘이 탱커/힐러/딜러처럼 협력한다 → `party`
- subject 둘이 동맹/결혼/독/결제중/배송중 같은 상태를 공유한다 → `relation/effect/zone`
- HP, inventory, cooldown, token holder 같은 내부 상태를 담는다 → `vessel`

### Intent vs Async / Fiber

`intent`는 결정적 orchestration 계약이고, `async/fiber`는 제어권 양보와 동시 실행을 다룬다.

따라서:

- `intent`는 workflow VM이 아니라 stateful orchestration declaration이다.
- `async/fiber`는 adapter, worker, runtime helper, hosted action 바깥에서 다뤄야 한다.
- `intent` clause 자체는 suspension point를 포함하면 안 된다.

고정 규칙:

- `await`
- `spawn`
- `async`
- `parallel`
- `select`
- channel send/recv

는 intent clause 안에 직접 들어갈 수 없다.

즉:

- intent는 "무슨 순서와 계약으로 상태를 옮길 것인가"
- async는 "작업을 언제 멈추고 재개할 것인가"

를 담당한다.

둘을 같은 계층에서 섞지 않는다.

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
- `Authorized`
  - authority/capability가 현재 경로에서 유효함
- `AuthorityLost`
  - authority/capability가 handoff / abort / invalidation으로 소실됨
- `Synced`
  - projection 또는 lifecycle handle이 최근 source와 동기화된 상태
- `Dirty`
  - projection 또는 lifecycle handle이 source 대비 갱신 필요 상태
- `Stale`
  - projection invalidation 이후 더 이상 신뢰할 수 없는 보수 상태
- `Detached`
  - relation/effect/layer가 분리된 상태
- `Published`
  - tobject projection이 boundary publish 완료된 상태
- `HandoffPending`
  - zone/world handle이 transfer 중간 상태에 있음
- `HandedOff`
  - zone/world handle이 새 경계로 넘어간 뒤 기존 경로에서 직접 소유하지 않음
- `Compensated`
  - rollback/compensate가 lifecycle handle에 적용된 상태

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
- `AuthorityHandle`
- `CapabilityToken`
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
  - local/internal projection contract
  - source subject/object의 현재 상태를 zone/world 실행 경계 안에서 읽기 위한 projection surface다
  - 기본 연산은 `refresh`다
  - `object`는 local view/snapshot 모델이지 boundary publish artifact가 아니다
  - `object`는 `Published` 상태로 승격되지 않는다
  - world가 embedded zone의 projection slot을 관찰할 수는 있지만, 그것은 local projection 관찰이지 boundary transfer가 아니다
- `tobject`
  - transfer/boundary projection contract
  - zone/world/authority/transport/export boundary를 넘기 위해 명시적으로 만든 전달 모델이다
  - 기본 연산은 `publish`다
  - receipt/export/packet/history/public API 같은 "바깥으로 넘기는 값"은 `tobject`로 표현해야 한다
  - `tobject`는 `object`의 별칭이 아니며, projection sync contract에서 별도의 상태 축을 가진다
  - `tobject`는 예외 객체나 우회 수단이 아니라, 의도적인 boundary projection을 위한 정적 타입 계약이다

정리:

- `object` = 내부 읽기/관찰/view
- `tobject` = 외부 전달/export/handoff
- 둘은 같은 데이터의 다른 표현이 아니라, 서로 다른 projection contract다

선택 규칙:

- 같은 실행 경계 안에서 source를 읽고 반영한다면 `object`
- zone/world 경계를 넘기는 publish/handoff/export 의미가 있으면 `tobject`
- world가 zone 내부 projection을 조회하는 것만으로는 `tobject`가 되지 않는다
- 외부 전달 계약이 붙는 순간 `object`가 아니라 `tobject`를 써야 한다

## 4.2 projection 연산

- `refresh`
  - subject/object source에서 object target을 갱신
- `publish`
  - subject/object source에서 tobject target을 갱신
- `bind`
  - declaration site에서 object/tobject target kind를 유지한 채 source를 연결

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
- `publish`는 tobject 전달 완료를 의미하지만 source mutation을 막지는 않음

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
- `authorized by subject alias`
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
