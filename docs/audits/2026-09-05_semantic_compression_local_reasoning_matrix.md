# Pergyra semantic-compression and local-reasoning stress matrix

Date: 2026-09-05

Status: `READ-ONLY DESIGN AUDIT`

Coordination base: `01f280a3566fb581d0f65d7783e678eee6c987d9`

이 문서는 `Slot`, `Zone`, `Capability`, scope, `Intent`를 함께 썼을 때의
**의미 압축률**과 **국소 추론 가능성**만 공격한다. 삭제 가능성의 최종 판정,
새 문법 채택, 구현 우선순위, SoT 상태 변경은 이 문서의 권한 밖이다.

검사 중 작업 트리에는 다른 작업자의 enum/self-host 변경이 진행 중이었다. 이
보고서는 그 변경 파일을 수정하지 않았고, 아래 언어 판정에도 의존하지 않는다.

## 판정부터

1. **`parallel`은 현재 가장 높은 압축률을 보인다.** 단순히 task를 두 개
   시작하는 표기가 아니라, 구조적 join, 가시성, 정적 read/write 충돌 거절,
   증거가 있는 서로소 분할, 고정 순서 수집/reduction을 함께 건다. 다만 순수
   메모리 병렬성만 비교하면 Rust의 scoped thread + borrow/`Send`/`Sync`도
   비슷하거나 더 강한 국소 보장을 준다. Pergyra의 추가 이득은 Zone, Slot,
   capability 경계까지 같은 admission에 결합할 때 생긴다.
2. **`Slot`은 ordinary local variable의 대체재일 때는 비율이 낮고, incarnation이
   바뀔 수 있는 resource identity일 때는 높다.** `Slot<T>` 자체가 정적 lifetime
   증명은 아니다. generation/token/pin-state는 런타임 계약이고, view escape와
   suspension 차단은 별도 CFG/경계 검사다.
3. **capability는 untrusted-content/host boundary에서 매우 높은 비율을 보인다.**
   `with caps`의 interprocedural `declared >= used`, 프로그램 manifest, 런타임
   fail-closed gate, spawn 시 context carriage가 일반 언어의 DI 객체 + 정책 파일
   + loader/OS sandbox 조합을 한 계약으로 묶는다. 반대로 `with caps`를 쓰지 않은
   함수도 capability가 추론되므로, **절이 없다는 사실만으로 무권한 함수라고
   국소 추론할 수는 없다.**
4. **`Zone`은 lock의 짧은 이름이 아니라 resource universe일 때만 값어치가
   크다.** `zone`이라는 말만 보고 lock, thread affinity, exclusive access를
   추론하면 틀린다. authority, transfer, frontier/lifecycle과 결합될 때 강하고,
   namespace처럼 쓰면 개념 비용만 남는다.
5. **`Intent`는 장식이라고 단정할 수 없지만, 현재의 가장 강한 마케팅 문장은
   아직 성립하지 않는다.** purpose identity에 coordination/authority/effect/
   boundary/compensation/trace를 귀속시키는 binder와 bounded typed-transition
   실행은 실물이다. 그러나 일반 participant coverage(INT-1), full-rollback
   compensation coverage(INT-2), dependency closure(INT-3), cross-intent static
   conflict(INT-4)는 설계/잔여 rung이다. 그러므로 현재 평가는 `조건부 중상`,
   그 네 의무가 실행 게이트로 닫힌 뒤에만 `높음`으로 승격할 수 있다.
6. **`scope simulation { ... }`과 `parallel enemies in zone combat`는 현재
   Pergyra 문법의 증거가 아니다.** language-word registry에는 `scope` 행이 없고,
   현재 수명 소유자는 `parallel` 블록, affine `Future`의 lexical owner, intent
   step으로 분산돼 있다. 이 의사문법을 현재 압축률 사례로 세면 결과가 부풀려진다.

## 평가 방법

### 증거 표기

- **O (Observed):** 현재 저장소의 source/contract/gate에서 직접 확인했다.
  등록된 gate가 있다는 뜻이며, 이 감사에서 해당 gate를 다시 실행했다는 뜻은
  아니다.
- **I (Inferred):** 둘 이상의 관찰을 합쳐 내린 설계 평가다.
- **P (Proposed):** 문서에 방향 또는 future rung으로만 존재한다.

### `semanticPower / conceptualCost`

줄 수를 세는 대신 한 workload의 **완전한 의무 집합**을 먼저 적었다.

```text
semanticPower = 언어/런타임 계약이 소유하여 사람이 재구성하지 않아도 되는 의무
conceptualCost = 해당 지점의 동작을 정확히 예측하기 위해 배워야 하는 독립 개념
```

두 축은 동일 가중치의 숫자가 아니므로 `높음 / 조건부 / 낮음`으로만 판정한다.
특히 library로도 구현할 수 있다는 사실은 semantic power를 0으로 만들지 않는다.
반대로 library convention을 compiler guarantee와 같은 칸에 세지도 않는다.

### 비교 기준

비교 대상은 각 언어의 일반적인 언어/표준 또는 대표 공식 concurrency surface다.
프로젝트별 custom framework를 허용하면 어느 언어든 Pergyra와 비슷한 API를 만들
수 있으므로, 그 경우에는 **그 framework의 구현·감사 비용도 conceptual cost에
포함**한다.

- Rust: `std::thread::scope`, borrow rules, `Send`/`Sync`.
- Kotlin: `kotlinx.coroutines.coroutineScope`. 이것은 Kotlin의 coroutine
  저수준 언어 지원 위에 놓인 library contract다.
- C#/.NET: `Task.WhenAll`, `CancellationToken`, `lock`, TPL.
- Go: goroutine, channel, `sync.WaitGroup`, memory model/race detector.

공식 비교 근거는 문서 끝에 모았다. 외부 문서는 Pergyra 의미론의 권위가 아니다.

## 먼저 바로잡아야 할 예시

다음은 좋은 **목표 그림**이지만 현재 실행 가능한 표면이라고 확인되지 않았다.

```pergyra
scope simulation {
    parallel enemies in zone combat {
        update(enemy)
    }
}
```

**O:** `src/lexer/language_keyword_registry.def`에는 `parallel`, `intent`, `zone`,
`slot`, `spawn`, `await`, `pin`, `within`이 있지만 `scope`는 없다. 현재 계약은
`parallel`이 자기 블록에서 join하고, named `spawn`의 `Future`를 lexical
owner가 모든 정상 경로에서 retire하게 하며, intent step도 작업 범위를
제공한다고 분리한다(`docs/05_async_concurrency.md:25`,
`docs/113_memory_concurrency_model.md:238`,
`docs/204_concurrency_direction_pscc_review.md:99`).

**I:** 따라서 현재의 정직한 비교 단위는 한 줄짜리 가상 구문이 아니라 다음
선언들의 합이다.

```text
parallel body
+ capture/ownership facts
+ Zone/action or intent where/using contract
+ Slot/view rules when resource handles are used
+ Future retirement if named tasks are spawned
+ capability manifest/context when ambient authority is used
```

이 합이 C#/Rust/Kotlin/Go의 무엇을 대체하는지는 아래에서 비교한다.

## 조합 압력 매트릭스

| 조합 | 현재 얻는 의무 | 빠져 있거나 별도인 의무 | power/cost | 국소 추론 판정 |
|---|---|---|---|---|
| `parallel { ... }` + 서로 다른 `Slot` 쓰기 | **O:** block join, join 뒤 가시성, sibling read/write 및 write/write 충돌 거절, 각 Slot runtime validity | Slot의 현재 generation/token 성공은 실행 시점 사실 | **높음** | `parallel`과 각 Slot identity가 보이면 강함 |
| `parallel (i in lo..hi)` + `arr[i]` | **O:** index-disjoint admission, base/비정칙 index 거절, all 수집의 index order | 일반 alias/field domain, dynamic disjointness | **높음** | body 한정으로 판단 가능; conservative reject 존재 |
| `parallel ... join with sum/min/max` | **O:** structured join, fixed index-order fold, checked Int/Long arithmetic, empty min/max fail-closed | user monoid, numeric profile에 따른 FP backend 차이 | **높음** | operator가 결과 의미를 잘 드러냄 |
| `parallel` + `Zone` | **O:** 각 축은 존재하지만 `parallel ... in zone ...` 결합 문법은 없음 | 어느 Zone fact가 capture/admission에 기여했는지는 선언/IR lookup 필요 | **조건부** | 한 줄만 보면 Zone authority/sync를 알 수 없음 |
| named `spawn` + `Future` + `await` | **O:** immutable single owner, 모든 정상 exit에서 await/`own` transfer, double-use/branch divergence 거절 | anonymous captured task, detach | **높음** | binding에서 lifetime이 드러남; `Cancel`은 retire가 아님 |
| `spawn` + capability context + budget | **O:** parent mask/instance snapshot과 exact shared budget owner carriage | lend/move로 mask 축소, detach authority | **높음** | source 한 줄만이 아니라 caller manifest/context도 봐야 함 |
| `await` + `Slot` | **O:** live pin/view는 suspension을 못 넘음; generation runtime check 존재 | **P:** resume 시 automatic `SlotRef{slot,generation}` revalidation | **조건부** | `Slot`만 보고 await 후 동일 incarnation이라고 믿으면 실패 |
| `SecureSlot` + token + `pin WriteView` | **O:** issued/matching token, generation, pin non-eviction, write-view exclusivity의 covered subset | exceptional/cancellation cleanup 전 범위, whole-language borrow proof | **높음**(보안 resource) | `SecureSlot`/view/token 세 개를 알아야 하지만 의무도 세 축임 |
| `Channel<owned T>` + `parallel` | **O:** blocking path ownership transfer, join; nonblocking/timeout은 copy-only | buffer fairness 일반 보장, ownership-bearing nonblocking receive | **높음** | API variant가 carriage 의미를 바꾸므로 medium learning cost |
| `intent` + `Zone` + `authorized by` + `causes` | **O:** purpose spine, explicit participant/zone/authority/effect attribution, runtime authority/trace, bounded lowering | 일반 INT-1 participant coverage와 INT-4 co-activity는 잔여 | **조건부 중상** | explicit form은 강함; derived form은 action 선언까지 두 홉 |
| typed `intent` + `after` + compensation + terminal outcomes | **O:** bounded plan에서 exact predecessor, typed variant/payload, success-only completion, predecessor-only reverse compensation | 일반 full-rollback effect coverage(INT-2), general dep-closure(INT-3) | **조건부 중상** | typed transition subset 안에서는 강함; `intent` 전체로 일반화 금지 |
| `parallel` + same-subject intents | **O:** runtime intent conflict/admission surface | **P:** static co-activity/ordering evidence(INT-4) | **중간 이하** | `parallel`만 보고 intent conflict-free라고 결론 내릴 수 없음 |
| `unsafe(raw)` + Slot/parallel | **O:** 현재 raw Slot escape는 명시 거절 | **P:** scoped unsafe capability와 AIR evidence | **현재 claim 불가** | 제안 철자를 현재 보장처럼 가르치면 안 됨 |

## Workload A — 병렬 simulation / ETL / reduction

### 완전한 의무 집합

병렬 업데이트가 실제로 올바르려면 적어도 다음을 답해야 한다.

```text
task containment
join-before-continuation
read/write overlap
alias/disjointness
mutation visibility after join
result ordering/reduction law
failure/cancellation behavior
resource lifetime and authority, if handles are captured
```

### Pergyra에서 확인된 세 가지 서로 다른 workload

1. **In-place partition update.**
   `tests/cases/parallel_disjoint_split/admit_pair.pgy`는 한 배열에서 만든 인접
   두 Slice를 두 arm이 각각 쓴다. `reject_overlap_views.pgy`는 겹치는 임의
   view를 거절한다. 이것은 단순 lock convention이 아니라 construction 기반
   Disjointness admission이다.
2. **Snapshot read beside a writer.**
   `tests/cases/parallel_snapshot/snapshot_read.pgy`는 channel로 reader를 writer
   뒤에 실행시켜도 reader가 pre-parallel snapshot `1`을 보게 한다.
   `reject_write_write.pgy`는 같은 scalar의 sibling write를 거절한다.
3. **Deterministic reduction.**
   `tests/cases/backend_compare/parallel_join_reduce/main.pgy`는
   `sum/product/min/max`를 completion order가 아니라 index-order fixed fold로
   정의하고 checked arithmetic을 사용한다. `docs/181_parallel_surface_full_design.md:64`
   및 `:101`이 순서 계약을 기록한다.

세 workload가 sharing, partitioned mutation, aggregation을 각각 건드리므로
`parallel`의 높은 판정은 한 showcase에만 의존하지 않는다.

### 다른 언어에서 같은 의무를 어디에 둬야 하는가

| 기준 | containment/join | overlap/alias | ordering/result | Zone/Slot/capability |
|---|---|---|---|---|
| Pergyra | `parallel` 자체가 join; named task는 affine Future flow | frozen subset은 compiler reject; split/snapshot evidence | `all`/reduce는 index-order, `any`만 명시 비결정 | 별도 owner지만 같은 semantic admission에 결합 가능 |
| Rust std | `thread::scope`가 scope 종료 전 join | borrow checker + `Send`/`Sync`; `split_at_mut`류가 disjoint borrow를 구성 | handle join/수집 코드를 작성; reduction policy는 선택한 iterator/library 계약 | generational resource, domain Zone, ambient caps는 application type/policy |
| Kotlin coroutines | `coroutineScope`가 child 완료를 기다리고 child failure 시 sibling 취소 | shared mutable state에는 Mutex/atomic/confinement 같은 별도 규율 | `awaitAll`/collection order와 선택한 API 계약을 확인 | application object/policy |
| C#/.NET | `Task.WhenAll`을 명시하고 returned task를 await | `Parallel.For` shared state는 회피/lock/thread-local convention | `WhenAll` 결과/exception contract + 별도 deterministic aggregation | application handle, lock, cancellation token, policy/host sandbox |
| Go | goroutine 뒤 `WaitGroup.Wait` 또는 channel protocol | compiler가 일반 race를 막지 않음; sync/channel + race detector | result channel/index bookkeeping | application handle/context/policy |

**I:** pure in-memory partition만 놓으면 Rust가 Pergyra보다 개념 비용이 낮거나
비슷할 수 있다. 반면 dynamic Slot identity, Zone authority, capability context까지
필요한 simulation이면 Rust/C#/Kotlin/Go에서는 별도 application protocol이
늘어나고 Pergyra의 결합 압축 이득이 커진다. 즉 `parallel`의 차별점은
"task 시작 글자 수"가 아니라 **admission에 연결되는 fact family의 수**다.

**반례:** 두 independent pure function을 동시에 실행하고 끝만 기다리는 경우,
Kotlin `coroutineScope`나 Rust `thread::scope`도 적은 개념으로 충분하다. 이
workload에서 Pergyra가 `Zone + Slot + Capability + Intent`를 요구한다면 비율은
오히려 나빠진다. 현재 문서는 pure computation을 `func`에 두므로 그런 강제를
정당화하지 않는다.

## Workload B — temporal Slot validity

### 완전한 의무 집합

```text
handle identifies an incarnation, not only an address/id
released/reused resource cannot be read through a stale handle
authority token matches slot generation and access mode
pin prevents release/eviction
write view excludes conflicting access
all exits clean up the view exactly once
suspension/task/channel boundaries do not smuggle a live view
```

### 세 가지 서로 다른 workload

1. **Entity/cache incarnation reuse.** Slot id가 해제 뒤 재사용돼도 generation이
   다른 old handle은 거절돼야 한다. `docs/semantics/08_slot_capability_calculus.md:252`
   이후 ABA theorem과 `tests/cases/slot_contract/reject/released_slot_read/main.pgy`
   가 이 실패 종류를 고정한다.
2. **Hot mutable lease.**
   `tests/cases/slot_contract/positive/pin_read_write_cleanup/main.pgy`는 read/write
   view와 normal/continue/break cleanup을 사용한다. Slot을 매 접근 검증하는
   대신 pin entry에서 lease를 확인하고 lexical body에서 view를 쓴다.
3. **Secure/device boundary.**
   `tests/cases/slot_contract/positive/secure_token_and_view/main.pgy`는 token +
   generation + WriteView를, `positive/device_slot_async_read/main.pgy`는
   `RemoteFuture<Int> -> Result<Int>`와 명시 release를 결합한다.

### 압축 판정

| workload | Pergyra가 소유하는 것 | baseline에서 추가할 것 | 판정 |
|---|---|---|---|
| 평범한 lexical local | Slot runtime registry/generation까지 생겨 과함 | Rust local/borrow, C#/Kotlin/Go GC value로 충분 | **낮음** |
| 장기 entity/resource handle | generation, token, pin state와 stable failure class | generational arena/handle wrapper + validation policy | **높음** |
| async device/remote resource | typed DeviceSlot, fallible RemoteFuture result, release boundary | safe handle/RAII, async result, version check, cleanup protocol | **높음**, 단 resume auto-revalidation은 아직 아님 |

**O:** Slot calculus 스스로 `Slot Is Not A Borrow Checker`라고 못 박는다
(`docs/semantics/08_slot_capability_calculus.md:51`). 안정된 주장도
`runtime capability + generation + token + pin-state safety`다(`:77`).

**I:** 따라서 local reviewer가 `Slot<T>`에서 알 수 있는 것은 "address가 아닌
검증되는 resource handle"이지 "이 줄에서 무조건 유효"가 아니다. 정적 claim은
`pin ... ReadView/WriteView`와 주변 boundary rule을 함께 봐야 한다. 이 구별을
가르치지 않으면 Pergyra 코드는 짧지만 추론은 틀린다.

**P:** await resume에서 automatic generation revalidation은
`docs/204_concurrency_direction_pscc_review.md:307`의 future rung이다. 현재
보장은 live view가 await를 넘지 못한다는 데까지다. 이를 이미 착지한 temporal
world safety라고 세면 semantic power가 과대 계산된다.

## Workload C — capability authority와 structured task

Capability는 세 가지 서로 다른 질문을 분리한다.

```text
effect: 실제로 무엇을 하는가
capability: 그 ambient operation을 할 수 있는가
authority: 이 domain participant가 이 transition을 승인할 수 있는가
```

### 세 가지 서로 다른 workload

1. **Interprocedural clock access.**
   `tests/capability/manifest_declared_ok.pgy`의 `with caps clock`은 성공
   계약이고, `manifest_interproc.pgy`는 `io_read`만 선언한 caller가 내부
   helper의 `Now()`를 숨길 수 없게 한다.
2. **Host-restricted nondeterminism/content.**
   `tests/capability/cap_random_demo.pgy`의 `Random`은 host grant가 random을
   빼면 runtime에서 `capability-denied`로 닫힌다. `docs/semantics/15_capability_sandbox.md:72`
   는 static `declared >= used`, `:100` 이하는 dynamic/FFI residual에서 runtime
   gate가 ground truth임을 구분한다.
3. **Spawned work under a shared ceiling.**
   `tests/capability/budget_spawn_demo.pgy`는 task 수 budget의 runtime
   chokepoint를 건드린다. `docs/113_memory_concurrency_model.md:38` 이후 계약은
   child가 parent capability snapshot과 exact shared budget owner를 받고,
   executor thread의 default TLS를 fallback으로 쓰지 못하게 한다.

### baseline 대비

Rust에서는 capability object를 parameter로 넘기고 `Send`/`Sync`로 carriage를
통제할 수 있다. C#/Kotlin에서도 interface/DI, Go에서도 narrow interface와
`context.Context`로 비슷한 프로그래밍 규율을 만들 수 있다. 네 언어 모두
host/container/OS sandbox를 조합할 수도 있다.

그러나 그 조합은 보통 다음을 project/framework가 따로 소유해야 한다.

```text
ambient-op inventory
transitive call-graph propagation
declared >= used validation
whole-program manifest serialization
host grant interpretation
runtime chokepoint enforcement
child-task context carriage and budget identity
diagnostic provenance
```

**I:** 이 workload에서는 Pergyra의 2개 사용자 개념(`with caps`, host grant/
manifest) 뒤에 위 의무가 묶이므로 비율이 높다. 그 반대 반례도 분명하다.
trusted closed application에서 clock 한 번 읽는 함수라면 explicit cap 절은
오히려 소음일 수 있다. 현재 절이 optional이고 manifest는 계속 추론되는 이유가
그 반례와 맞는다.

### local reasoning의 날카로운 모서리

- `with caps io_read, clock`이 있으면 **declared upper bound**와 누락 거절을
  국소적으로 읽을 수 있다.
- 절이 없으면 "capability 없음"이 아니라 "declaration check를 요청하지 않음,
  manifest에는 여전히 inferred use가 들어감"이다.
- child task는 parent context를 정확히 capture하지만, 향후 lend/move처럼 더
  좁히는 edge는 아직 방향 모델이다.
- `authorized by:`는 host capability mask가 아니다. zone participant와 token/
  delegation 상태를 검증하는 별도 domain authority다.

이 네 문장이 한 화면의 mental-model 표에 없으면 capability는 구현상 강해도
사용자 추론 비용이 높다.

## Workload D — Intent as workflow binder

### 완전한 의무 집합

```text
one purpose identity
declared participants cover actual participants
step dependency topology is acyclic and data-dependency closed
zone/boundary placement and transfer are valid
authority and required ability are valid
effects are attributed
guards/postconditions/terminal outcomes are typed
full rollback covers every effectful step or marks irreversibility
only successfully completed predecessors are compensated, in the right order
trace/history remains attributable to the same purpose identity
```

### 세 가지 materially different workload

1. **Calendar authoring.**
   `examples/calendar_manage_event_{explicit,compressed}.pgy`는 create/edit/delete
   action contracts를 한 intent에서 재사용한다.
2. **Commerce boundary transfer.**
   `examples/shopping_mall_checkout_refund/intents/commerce_intents.pgy`는 cart ->
   payment -> refund/account Zone, buyer/merchant authority, effects와 guard/post를
   결합한다.
3. **Typed multi-step rollback.**
   `tests/self_hosted/parity/fixture/intent_typed_outcome_compensation.pgy`와
   `docs/self_hosted/19_intent_execution_transition_contract.md:194` 이후 bounded
   seam은 exact typed outcome, predecessor identity, success-only completion,
   predecessor-only reverse compensation을 한 admitted plan으로 묶는다.

게임 쪽에는 `examples/dnd_tavern_campaign/intents/campaign_intents.pgy`, 물류에는
`examples/logistics_intent_probe/intents/logistics_intents.pgy`도 있어 domain
다양성은 3개를 넘는다. 다만 example 수는 compiler guarantee 수가 아니다.

### 실제 압축 측정

**O:** calendar explicit/compact pair의 diff에서 compact form은 세 step마다
`who`, `where`, `requires`, `authorized by`, `causes` 5개 절, 총 15개 반복 절을
지운다. 계약은 각 action 선언에 남는다
(`examples/calendar_manage_event_compressed.pgy:23-45`), intent body는
`using`과 `on` 중심으로 줄어든다(`:81-100`).

`tests/intent_compression_contract_smoke.sh`는 이 유도를 codegen 재추론으로
미루지 않고 provenance와 explicit/derived conflict를 DIR/AIR/RIR까지
보존하도록 정적 ratchet을 둔다. `tests/example_contract_smoke.sh:352-359`는
explicit/compact calendar 및 orchestration 예제를 양 backend 실행 목록에 둔다.
이 감사에서는 두 gate를 재실행하지 않았다.

**O:** 더 큰 `composite_intent_orchestration_{explicit,compressed}.pgy` 쌍은
세 개의 `where`만 줄인다. 즉 압축 이득은 계약 밀도와 실제로 유도 가능한
축에 따라 크게 달라진다. "Intent면 항상 7개 정책이 사라진다"는 주장은
이 두 번째 workload가 반증한다.

### local reasoning trade-off

Compact calendar는 authoring repetition을 줄였지만, reviewer가 `on:
owner.CreateEvent()` 한 줄에서 authority/effect/zone/ability를 모두 알려면
action 선언으로 이동해야 한다. compiler fact는 잃지 않았지만 **사람의
한 화면 추론은 약해졌다.** 이는 의미 손실이 아니라 lookup 비용 이동이다.

따라서 압축 형식은 다음 조건일 때만 승리한다.

```text
derived fact가 정확히 한 owner에서 온다
explicit override와 충돌하면 fail closed한다
diagnostic/AST dump/IDE hover가 resolved contract와 provenance를 보여 준다
reviewer가 원하면 explicit form으로 펼쳐 볼 수 있다
```

앞의 세 조건은 저장소 계약/ratchet에 상당 부분 보인다. 마지막 "resolved
contract lens"는 현재 이 감사에서 user-facing tool로 확인하지 못했다.
새 문법을 늘리기보다 이 projection을 제공하는 편이 conceptual cost를 더 잘
줄인다.

### 현재 Intent claim의 경계

**O:** `docs/self_hosted/19_intent_execution_transition_contract.md:35-40`은
Intent를 source-level cross-axis binder로 정의하면서, 현 typed transition을
coordination/boundary/compensation의 **bounded projection**이라고 제한한다.

**O/P:** `docs/173_intent_axis_strengthening.md:133-135`의 INT-1/2/3은 명확한
compiler rejection 목표지만, 같은 문서 `:187-210`은 semantic pass 잔여를
명시한다. Coq kernel이 obligation shape와 guard-free theorem을 갖는 것과,
현재 compiler가 모든 source intent에 그 전제를 방출하는 것은 다른 주장이다.

**I:** 그래서 Intent의 현재 power/cost는 다음처럼 나뉜다.

- bounded typed-transition / authority / trace workload: **중상**;
- 일반 workflow의 compile-time participant/compensation/dependency closure:
  **아직 claim 불가**;
- pure calculation, one-call handler, `success: true`뿐인 wrapper: **낮음**.

Intent를 지켜야 할 근거는 이름 자체가 아니라, 같은 purpose identity에
검증된 subfacts를 귀속시키고 bounded compensation/trace 실행을 제공한다는
점이다. INT-1~4가 닫히지 않으면 이 근거가 "강한 정적 workflow 언어"까지
확장되지는 않는다.

## Local-reasoning 시험표

| 보이는 표면 | 독자가 안전하게 알 수 있는 것 | 알 수 없는 것 / 흔한 오독 | 판정 |
|---|---|---|---|
| `parallel { ... }` | block exit 전에 join; covered capture overlap은 semantic admission 대상 | 모든 동적 alias가 증명됨, 항상 실제 thread에서 실행됨 | **강함** |
| `parallel (...) join with all` | fan-out + all join; expression 결과는 index order | completion order가 result order임 | **강함** |
| `join with sum/product/min/max` | fixed index-order fold, numeric restrictions | arbitrary associative/user reducer, schedule-adaptive tree | **강함** |
| `join with any` | 명시적으로 first-completion 계열 비결정 | deterministic winner | **강함** |
| `spawn Worker()` bound to immutable `Future` | handle 하나를 현재 lexical owner가 retire해야 함 | scope exit implicit drain, Cancel이 join/free함 | **강함** |
| `async func` | suspension-capable declaration | lifetime, cancellation, parallelism, error policy를 async가 소유함 | **약함, 문서 교육 필수** |
| `await future` | completion join + affine handle consume | resource revalidation, cancellation, error aggregation 전부 | **중간** |
| `Slot<T>` | replaceable backend handle를 감싼 generation/capability resource boundary | compile-time liveness, raw address stability, general borrow proof | **중간** |
| `pin s as v: WriteView<T>` | covered lexical region의 exclusive mutable view, boundary crossing rejects | 모든 exceptional/cancellation exit의 whole-language theorem | **강함(범위 표기 필요)** |
| `Zone` | domain resource universe/lifecycle/authority facts의 owner가 될 수 있음 | mutex, scheduler lane, exclusive scope 그 자체 | **중간** |
| `with caps ...` | declared set이 transitive static use를 덮어야 함 | 절 없는 함수가 capability-free임; dynamic/FFI까지 static complete | **강함 when present** |
| `authorized by: p` | p를 domain authority participant로 검사/귀속 | host capability grant 또는 성공이 미리 보장됨 | **강함** |
| explicit `intent step` clauses | participant/boundary/authority/effect/guard/compensation의 선언 위치 | 모든 bundle-wide 의무가 이미 static closed | **중간** |
| derived/compact intent step | facts는 action/default/transfer owner에서 유도되고 provenance가 남음 | 한 화면만 보고 정확한 resolved contract를 모두 앎 | **중간 이하 without lens** |
| `scope ...` | 현재 아무것도 아님: 등록된 표면 word가 아님 | current Pergyra가 general TaskScope syntax를 제공함 | **해당 없음** |

## 종합 scorecard

| 개념/조합 | 의무 밀도가 높은 적합 workload | conceptual cost | semanticPower/cost | 승격 또는 유지 조건 |
|---|---|---:|---:|---|
| `parallel` + evidence | simulation tick, ETL partition, numeric reduction | 중 | **높음** | general alias를 과대 주장하지 않고 fail-closed 유지 |
| affine spawn scope | request fan-out, bounded worker tree, cancellable task | 중 | **높음** | anonymous capture/detach를 별도 rung으로 유지 |
| `Slot` + view | entity incarnation, cache/device handle, secure resource | 중상 | **높음** | ordinary value에 강제하지 않기; static/runtime 문구 분리 |
| bare `Slot` for local scalar | local counter/temporary | 중 | **낮음** | `func` local/value를 기본으로 유지 |
| `Zone` + authority/transfer/frontier | combat resource universe, payment boundary, compiler world | 중상 | **조건부 높음** | namespace/lock 용법 금지; resolved boundary inspection |
| `with caps` + manifest/gate | untrusted content, plugin, task authority/budget | 중 | **높음** | absent-clause 의미와 dynamic residual을 명확히 교육 |
| explicit Intent bundle | commerce saga, game quest, IoT workflow | 높음 | **조건부 중상** | INT-1~4 실행 강제 + 3-domain counterexample corpus |
| tiny Intent wrapper | pure calculation, one event handler | 높음 | **낮음** | `func`/action을 기본으로 유지 |
| general `scope` keyword 추가 | 현재 parallel/Future/intent-step이 이미 범위를 소유 | 추가 개념 | **현재 낮음** | 기존 세 owner로 표현 못 하는 독립 정적 보장을 먼저 제시 |

## 새 abstraction 진입 장벽에 대한 결과

사용자가 제안한 다섯 조건을 이 matrix에 적용하면 다음이 나온다.

1. **새 `scope` 문법:** 현재는 보류가 맞다. parallel block, affine Future,
   intent step이 이미 수명 owner이고, 새 keyword가 제공할 독립 guarantee가
   아직 제시되지 않았다. 일반 scope tree는 formal direction으로 유용하지만
   곧바로 surface noun이어야 하는 것은 아니다.
2. **`LiveRef` 같은 await 전용 표면:** resume revalidation은 새 보장이지만,
   기존 `SlotHandle{slot,generation}`과 checked suspension fact로 표현 가능한지
   먼저 시험해야 한다. 내부 fact면 충분하다면 surface noun을 만들지 않는다.
3. **Dynamic Disjointness:** 새 사용자 noun보다 admission fact로서 강하다.
   정적으로 모르는 동적 집합을 진입 전 한 번 검사해 hot loop 검사와 무음
   sequential fallback을 없앤다면 독립 보장과 비용 절감이 모두 있다.
4. **Intent:** 세 domain 사례와 binder attribution은 통과한다. 그러나 "새로운
   정적 보장" 조건은 전체가 아니라 bounded subset만 통과했다. INT-1~4가
   닫히기 전에는 조건부 채택 상태가 정직하다.

## 다음에 실제로 깨야 할 falsifier

이 보고서는 구현 rung을 열지 않는다. 향후 언어 설계 결정이 열릴 때 다음
반례부터 executable corpus로 만드는 것이 가장 정보량이 높다.

1. **Simulation composite:** 같은 CombatZone의 서로소 entity Slice 두 개는
   병렬 admit, 한 entity가 양쪽에 들어가면 entry-before-work reject. join 뒤
   Slot generation이 바뀐 entity는 stale로 처리한다.
2. **Await temporal:** task가 SlotRef를 잡고 suspend한 동안 다른 task가
   release/reclaim한다. resume unchecked read는 반드시 불가능하고, checked
   resolve는 `None`/typed failure여야 한다.
3. **Capability narrowing:** parent가 `io_read`만 가진 상태에서 worker/default
   TLS가 `network`를 얻는 mutation은 두 backend에서 같은 class로 fail closed.
4. **Intent participant coverage:** header 밖 subject를 nested helper가 만지는
   세 workload(게임, 결제, IoT)가 모두 INT-1에서 같은 provenance로 거절돼야
   한다.
5. **Intent compensation coverage:** `rollback: full`에서 effectful step 하나의
   compensate를 지우면 compile reject; `irreversible`이 세 corpus에서 default로
   남발되면 abstraction 실패로 판정한다.
6. **Intent dependency closure:** `after` cycle뿐 아니라 B가 A의 result를 읽지만
   dependency edge가 없는 경우를 거절한다.
7. **Local lens test:** compact step 하나에서 IDE/diagnostic가 resolved
   `who/where/requires/authorized/causes`와 각 provenance를 한 요청으로 보여
   주지 못하면 compact surface의 local-reasoning 점수를 낮춘다.
8. **No-concept control:** pure map/reduce, 단일 async read, 단일 action을
   Zone/Slot/Intent 없이 `func` 중심으로 쓴 control corpus가 더 짧고 동일하게
   안전해야 한다. 그렇지 않으면 Pergyra가 domain nouns를 강제하는 것이다.

## 저장소 근거

- Parallel/future freeze:
  `docs/113_memory_concurrency_model.md:19-80`, `:232-279`.
- Current async mental model:
  `docs/05_async_concurrency.md:10-18`, `:23-48`, `:84-110`, `:206-256`.
- Slot static/runtime boundary:
  `docs/semantics/08_slot_capability_calculus.md:22-99`, `:101-163`,
  `:348-418`.
- Capability declare/manifest/gate:
  `docs/semantics/15_capability_sandbox.md:9-110`.
- Intent bundle and unlanded general obligations:
  `docs/173_intent_axis_strengthening.md:50-115`, `:129-210`.
- Bounded typed Intent execution:
  `docs/self_hosted/19_intent_execution_transition_contract.md:8-41`,
  `:91-224`.
- Concurrency corrections and status:
  `docs/204_concurrency_direction_pscc_review.md:55-252`, `:254-478`.
- Intent derivation boundary:
  `docs/60_zone_context_and_transfer_derivation.md:68-119`, `:220-236`.
- Exact current word registry:
  `src/lexer/language_keyword_registry.def` (`scope` row absent at inspection).

## 외부 baseline 근거

- Rust scoped threads join before returning and may borrow non-`'static` data:
  [Rust `std::thread::scope`](https://doc.rust-lang.org/std/thread/fn.scope.html).
- Rust uses `Send` for transfer and `Sync` for shared reference safety:
  [Rust Book: Extensible Concurrency with Send and Sync](https://doc.rust-lang.org/book/ch16-04-extensible-concurrency-sync-and-send.html).
- Kotlin lexical coroutine scopes wait for all children and define sibling
  cancellation/failure propagation:
  [Kotlin `coroutineScope`](https://kotlinlang.org/api/kotlinx.coroutines/kotlinx-coroutines-core/kotlinx.coroutines/coroutine-scope.html).
- .NET `Task.WhenAll` returns one task whose state represents all supplied tasks:
  [.NET `Task.WhenAll`](https://learn.microsoft.com/en-us/dotnet/api/system.threading.tasks.task.whenall).
- .NET documents that shared state in `Parallel.For` needs avoidance,
  thread-local state, or synchronization:
  [.NET data/task parallelism pitfalls](https://learn.microsoft.com/en-us/dotnet/standard/parallel-programming/potential-pitfalls-in-data-and-task-parallelism).
- Go requires synchronization for concurrently accessed mutable data and does
  not make goroutine exit itself a synchronization event:
  [Go memory model](https://go.dev/ref/mem).
- Go's explicit group join is `sync.WaitGroup`:
  [Go `sync.WaitGroup`](https://pkg.go.dev/sync#WaitGroup).

## 감사 결론

Pergyra의 가장 좋은 모습은 "새 명사가 많은 언어"가 아니라 다음 식이다.

```text
한 개의 visible boundary
  -> 여러 owner fact를 compiler가 결합
  -> 불가능한 조합은 fail closed
  -> 실행에는 compact plan/handle만 남김
```

현재 이 식을 가장 잘 만족하는 것은 `parallel` + evidence, affine Future,
resource-grade Slot/view, host capability manifest다. Zone은 실제 resource
universe에서 강하고, Intent는 bounded execution에서는 실체가 있으나 일반
정적 bundle claim은 아직 조건부다.

따라서 다음 미학 규칙이 적합하다.

> 새 syntax를 추가해서 semantic power를 늘리기 전에, 이미 존재하는 owner
> facts가 한 지점에서 보이게 하라. 압축이 사실을 없애면 실패이고, 사실을
> 보존한 채 반복만 없애면 성공이다.
