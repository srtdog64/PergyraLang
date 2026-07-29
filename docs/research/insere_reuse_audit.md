# Insere 전수 재사용 감사 — Pergyra 채택 후보와 금지 경계

Updated: 2026-07-30 (Asia/Seoul)

Status: research/adoption dossier. 이 문서는
`docs/201_insere_zeno_lineage_and_library_adoption.md`의 canonical 채택 계약을
대체하지 않는다. Insere는 provenance와 falsifier이며 Pergyra source, owner,
registry와 executable gate가 의미 권위다.

## 0. 결론

`F:/insere`는 Pergyra에 가져올 가치가 크다. 다만 가치의 중심은 TypeScript
scheduler 구현 자체가 아니라 다음 세 불변식이다.

1. stable key의 **현재 occupant만** wait, final, cleanup, 결과 publication을
   commit할 수 있다.
2. host가 clock과 실행 환경을 소유하고, scheduler는 숨은 thread나 background
   실행을 만들지 않는다.
3. start policy, post-failure policy, buffering/loss policy와 unsafe boundary를
   서로 다른 결정으로 드러내고 negative/stress gate로 고정한다.

첫 번째 불변식과 `spawn`/`restart`/`skip` start admission은 이미
`D:/PergyraLang/stdlib/host_task_slot.pgy`에 Pergyra 방식으로 번역되어 있다.
현재 C/LLVM focused gate도 green이다. 이 이상 Insere runtime을 stdlib에 통째로
옮기는 것은 채택이 아니라 Pergyra의 `async`, `parallel`, `Channel<T>`, Slot,
SEA lane scheduler 옆에 두 번째 실행 모델을 만드는 일이다.

다음으로 가치가 가장 큰 실제 seam은 generic scheduler가 아니라 기존 self-host
LSP document store의 **URI + monotonic version + latest-only publication admission**이다.
`D:/PergyraLang/src/self_hosted/lsp/document_store_owner.pgy`는 여러 URI와 version을
운반하지만 version을 문자열로 저장하고, 누락·역행·동일 version의 다른 text를
거부하지 않는다. Native C LSP는 version을 읽지 않고 단일 document 전역만
보유한다. Insere의 occupant invariant를 기존 LSP owner에 번역하면 실제 VS Code
편집 경계의 stale result 문제를 닫을 수 있다.

단, 현재 활성 self-host semantic-admission rung을 중간에 바꾸지 않는다. 아래 LSP
slice는 그 rung이 commit된 뒤의 다음 eligible tooling rung이다. Self-host probe만
green인 상태는 `REACHABLE` supporting evidence이며 live C LSP bypass가 삭제되기
전에는 `SUBSTITUTING`이 아니다.

## 1. 감사 범위와 관찰 증거

### 1.1 Provenance

- Source root: `F:/insere`
- Revision: `997287030f3cbc64c6f5f8f15053a67cdae4e9a9`
- Git state: `main == origin/main`, clean
- Package: `@exornea/insere` `0.2.3`, public pre-release
- License: `F:/insere/LICENSE`, MIT, copyright 2026 Exornea
- Runtime dependency: 없음. `F:/insere/package.json`은 TypeScript, Vitest,
  Node type, rimraf만 dev dependency로 둔다.
- Compiler posture: `strict`, `exactOptionalPropertyTypes`,
  `noUncheckedIndexedAccess`, ESM/NodeNext

Git의 dubious-ownership 경고는 command-local
`git -c safe.directory=F:/insere`로만 우회해 읽었다. 전역 Git 설정과 Insere tree는
수정하지 않았다.

MIT이므로 이식은 가능하지만 substantial source copy에는 license notice를
보존해야 한다. 이번 채택 판단은 구현을 복사하지 않고 invariant를 Pergyra owner로
번역한다. 이후 실제 코드를 복사한다면 provenance와 notice를 함께 남긴다.

### 1.2 이번 감사에서 직접 관찰한 gate

- `npm run typecheck:test`: PASS
- `npm test`: 7 files, 147 tests PASS
- `node benchmark/restart-storm.mjs --gate`: PASS on Node `v24.18.0`
  - 100,000 restarts, warmup 뒤 30 samples
  - `DirectInsereTask.restart`: median `6.55ms`, p75 `6.97ms`
  - Promise+Map+Abort 대비 median throughput ratio `153.55x`
- Pergyra `tests/host_task_slot_smoke.sh`: C/LLVM PASS
- Pergyra `tests/host_task_policy_smoke.sh`: C/LLVM PASS

Restart 수치는 기존 `dist`의 v0.2.3 artifact를 실행한 이 장비의 microbenchmark다.
Pergyra runtime 성능 증거나 언어 간 절대 비교가 아니다. `npm run check`와 release
matrix는 `dist` rebuild/pack을 포함하므로 외부 source를 수정하지 말라는 이번 감사
범위에서는 실행하지 않았다.

## 2. Insere source 전수 분류

| Source | 관찰한 핵심 | Pergyra 판정 | 배치 |
| --- | --- | --- | --- |
| `F:/insere/src/core.ts` | direct keyed slot, reentrant restart/cancel, current entry identity guard, frame double buffer, group index | occupant/negative gate는 채택. runtime 구현은 이식 금지 | existing stdlib contract + test lineage |
| `F:/insere/src/runtime.ts` | generator routine, frame/delay/idle/Promise wait, cancellation finalizer | stale resume falsifier만 채택. Generator/Promise runtime은 금지 | reference only |
| `F:/insere/src/task.ts` | typed `spawn/restart/skip`과 applied/status report | 이미 HostTask admission으로 채택. string key namespace는 이식하지 않음 | stdlib core, `REACHABLE` |
| `F:/insere/src/clock.ts` | host가 `now`를 제공하고 runtime이 frame/delta를 관측 | clock ownership 원칙만 채택. 새 stdlib clock 금지 | runtime contract reference |
| `F:/insere/src/instruction.ts` | frame/idle/delay/Promise의 작은 instruction vocabulary | Pergyra MIR/SEA를 대신하지 않음 | do not port |
| `F:/insere/src/context.ts` | task-local key, state, dispatch, cancellation context | Pergyra의 task/capability/ownership contract가 이미 더 강함 | do not port |
| `F:/insere/src/effect.ts` | Result helpers, composition, ensuring, acquire/use/release | cleanup/compensation falsifier만 채택. effect monad를 stdlib에 만들지 않음 | semantic/test reference |
| `F:/insere/src/api.ts` | direct/effect runtime을 한 key space로 감싸고 Result boundary와 supervision을 분리 | one-key-space와 boundary 분리는 참고. facade/runtime은 금지 | reference only |
| `F:/insere/src/supervision.ts` | start policy와 분리된 bubble/stop/result/restart policy | 분리 원칙은 채택. 현재 restart 구현은 Pergyra retry 계약을 충족하지 못해 보류 | future official-library policy |
| `F:/insere/src/mailbox.ts` | drop/latest/queue/bounded, consume-one/fanout, abort-heavy waiter compaction | loss vocabulary와 tests만 후보. `Channel<T>` 옆 generic mailbox는 금지 | deferred recipe |
| `F:/insere/src/event-bus.ts` | keyed wait, unique wait, subscription, buffering | Pergyra event/Channel/domain topology와 중복. 실제 zone ingress 전에는 금지 | do not port |
| `F:/insere/src/host.ts` | clock, task facade, mailbox, event bus 조합 | host application adapter 예시일 뿐 언어 runtime owner가 아님 | do not port |
| `F:/insere/src/logging.ts` | bounded logger와 original failure 보존 | schema idea만 참고. raw cause와 schema drift 때문에 코드 이식 금지 | diagnostics reference |
| `F:/insere/src/index.ts` | package export surface | npm package 구조는 Pergyra module/stdlib SoT가 아님 | no adoption |

### 2.1 가장 강한 구현 증거

- `F:/insere/src/core.ts:133`의 restart는 이전 occupant를 취소한 뒤 replacement를
  설치한다.
- `F:/insere/src/core.ts:548`은 aborted/removed entry가 step 뒤 상태를 commit하지
  못하게 한다.
- `F:/insere/src/core.ts:612`는 map의 현재 entry object와 동일할 때만 삭제한다.
- `F:/insere/src/runtime.ts:403`은 old routine resume 뒤 aborted/index를 확인하고
  wait state를 쓰지 않는다.
- `F:/insere/test/runtime.test.ts:40`과
  `F:/insere/test/core.test.ts:243`은 self-restart 중 이전 실행이 새 wait state를
  덮거나 부활하지 못하는 exact reentrancy falsifier다.
- `F:/insere/test/runtime.test.ts:68`은 이전 finalizer가 새 occupant 시작 전에
  끝나는 순서를 고정한다.
- `F:/insere/test/core.test.ts:217`은 frame queue drain 중 reentrant group cancel을
  실행한다.
- `F:/insere/test/core.test.ts:273`은 cancellation finalizer의 LIFO 순서를
  실행한다.

이 증거는 `HostTaskSlot`의 generation ticket과 실제 host publication 경계에
재사용할 가치가 있다. JS object identity 자체를 복사하지 않는다. Pergyra에서는
이미 존재하는 explicit `<key, generation>` identity가 더 적절하다.

### 2.2 성능에서 가져올 교훈

`F:/insere/benchmark/geukbit-scale.mjs`는 기능보다 배치 단위를 잘 보여준다.

- `F:/insere/benchmark/geukbit-scale.mjs:288`의 per-entity task는 의도적으로
  discouraged case다.
- `F:/insere/benchmark/geukbit-scale.mjs:307`의 one system task는 같은 작업을
  system/phase 경계에 둔다.
- `F:/insere/benchmark/geukbit-scale.mjs:392`의 projection restart는 stable key
  하나에 latest-only policy를 적용한다.
- `F:/insere/benchmark/restart-storm.mjs:53`은 best 한 번이 아니라 median/p75/p90과
  raw samples를 기록하며 ratio와 absolute cap을 함께 gate한다.

Pergyra 번역은 “모든 entity를 subject/action/task로 감싼다”가 아니다. 실제
identity, authority, state/stage 또는 resource handoff가 있는 system/phase/resource
경계에만 subject/action/zone을 둔다. 순수 hot loop는 `func`와 data owner에 남긴다.
성능 gate도 ratio만 두지 말고 absolute budget과 raw sample을 함께 보존한다.

## 3. Pergyra 계층별 채택 판단

### 3.1 stdlib core

채택 완료 항목은 `HostTaskSlot` 하나다.

- Owner: `D:/PergyraLang/stdlib/host_task_slot.pgy`
- Surface: stable key, generation, phase, ticket, typed start policy,
  wait/final/cleanup transition
- Why stdlib: host task publication의 작은 immutable protocol이며 scheduler,
  thread, Promise나 compiler owner를 import하지 않는다.
- Grade: official-library `REACHABLE`; real host adapter consumer는 아직 없음

추가 generic scheduler, clock, mailbox, event bus, effect combinator는 stdlib core로
채택하지 않는다. `D:/PergyraLang/docs/138_standard_library_scope.md`의 generic
callable/monomorphization 제약과 기존 `Channel<T>`/runtime mechanism 소유권에도
충돌한다.

### 3.2 일반 official library

다음은 실제 host workload가 생길 때만 연다.

1. `HostTaskPublicationAdapter`: existing task/future handle 옆에
   `HostTaskTicket`을 보관하고 publish/cleanup 직전 current generation을 다시 읽는
   얇은 adapter.
2. Typed loss admission: 특정 zone ingress가 `latest`, bounded queue,
   drop-oldest/newest를 정말 필요로 할 때만 해당 domain의 loss contract로 추가.
   silent drop이나 delivered-count `0`만으로 손실을 숨기지 않는다.
3. `HostTaskFailureDecision`: start admission과 분리하되 retryable condition,
   max attempts, backoff/budget, idempotency와 remembered execution source가 모두
   owner에 있을 때만 추가한다.

실제 consumer 없이 이 셋을 먼저 만들면 speculative API다.

### 3.3 compiler-internal/tooling

가장 좋은 다음 후보는 LSP document revision admission이다.

현재 증거:

- `D:/PergyraLang/src/self_hosted/lsp/document_store_owner.pgy:111`은 version을
  JSON number spelling의 `String`으로 받는다.
- 같은 파일 `:116`은 version 누락을 빈 문자열로 허용한다.
- 같은 파일 `:209`는 existing URI의 version/text를 순서 검증 없이 덮어쓴다.
- `D:/PergyraLang/src/lsp/pgy_lsp.c:181`의 native store는 URI/content 하나만
  보유한다.
- 같은 파일의 didChange path `:310`은 LSP version을 읽지 않는다.
- `D:/PergyraLang/tests/tooling_conformance_smoke.sh:214`는 version 2를 보내지만
  version carriage, rollback rejection 또는 multi-document isolation을 assert하지
  않는다.

이 축은 `HostTaskSlot`을 import해 LSP 의미를 숨길 일이 아니다. 기존 LSP document
store owner가 URI/version/text와 admission을 소유하고, HostTask가 제공한
latest-only invariant만 번역한다.

### 3.4 runtime core

Insere에서 새 runtime code를 가져오지 않는다.

- Pergyra는 이미 `PgyLaneScheduler`, `spawn`/`await`, cooperative cancellation,
  `parallel`, `Channel<T>`, `select`, timer/clock lowering을 소유한다.
- Insere는 JavaScript event-loop reentrancy 모델이고 Pergyra runtime은 실제
  multi-thread/lane memory model을 다룬다. lock-free 또는 thread-safety 증거로
  전용할 수 없다.
- Direct core의 swap-delete, single-entry specialization, `:` prefix group index,
  frame double buffering은 프로파일과 ordering contract가 있는 owner 안에서만
  참고할 수 있다. compiler/runtime 공통 helper로 만들지 않는다.

### 3.5 개발 도구와 gate

재사용 가치가 높다.

- reentrant self-restart mutation corpus
- stale wait/final/cleanup/result publication negatives
- finalizer ordering과 callback-failure isolation
- warmup 뒤 median/p75/p90 + raw samples
- relative ratio와 absolute time/memory cap의 동시 gate
- 좋지 않은 사용법(per-entity task)을 positive benchmark와 함께 보존하는 방식

이는 library API가 아니라 test methodology다. Pergyra의 build-pressure owner나
LSP burst gate에 필요한 부분만 옮긴다.

## 4. Pergyra 구성체로 번역하는 규칙

Insere API 이름을 Pergyra keyword로 기계적으로 감싸지 않는다.

| 상황 | Pergyra 구성체 | 이유 |
| --- | --- | --- |
| generation 비교, admission 계산, buffer policy 계산 | `struct`/`class` + `func` | identity 없는 순수 값/정책 계산 |
| live LSP session이 URI별 document identity와 mutable lifecycle을 실제 소유 | future `subject` + subject-owned `vessel` 후보 | production identity와 state가 실제로 생긴 뒤에만 |
| document change 적용, diagnostic publication, artifact commit | future `action` 후보 | caller가 소비하는 성공/실패와 public transition이 있을 때만 |
| stdin/stdout session, workspace, artifact resource frontier | `zone` 후보 | 실제 capability/lifetime/authority boundary가 있을 때만 |
| 독립적인 현실 목적과 participant/authority/effect/boundary/compensation/trace 의무 | `intent` | task/action 수가 아니라 purpose bundle이 근거 |
| source lifecycle에서 분리해 전달하는 immutable diagnostic/artifact batch | `tobject` 후보 | 실제 publish/handoff가 생긴 뒤에만 |

현재 `HostTaskSlot`은 immutable policy computation이므로 `subject/action`을 쓰지
않는 것이 Pergyra답다. 현재 self-host LSP document store도 buffered text projection
단계라 `func` owner가 맞다. Live session이 production entrypoint에서 document
identity를 소유하고 change 결과를 소비하기 전에는 ceremonial `subject/action`을
추가하지 않는다.

향후 `tobject LspDiagnosticBatch`를 만들 수는 있다. 단 그 값은 URI, admitted
version, immutable diagnostics와 producer identity를 운반하는 detached transfer일
뿐이다. freshness, document identity, publication authority를 소유하지 않는다.
그 사실은 document slot/admission과 action이 소유한다. 현재 result를 `tobject`로
이름만 바꾸는 것은 채택 증거가 아니다.

## 5. 즉시 착수 가능한 최소 slice — LSP monotonic document admission

현재 semantic-admission rung 다음에 열 수 있는 objective card다.

- Objective: URI별로 정확히 하나의 current document epoch만 state와 diagnostic
  publication을 갱신하게 한다.
- Priority: URI identity, monotonic version, same-version payload identity,
  multi-document isolation, stale publication rejection, coalescing 성능.
- Fact owner: existing
  `D:/PergyraLang/src/self_hosted/lsp/document_store_owner.pgy`. Version은 비교 가능한
  typed integer fact로 admission되고 state row와 candidate diagnostic에 함께
  운반돼야 한다.
- Last legitimate consumer: self-host `session_state_owner.pgy`와 future live
  diagnostic/response publication owner. Native substitution 시 마지막 C consumer는
  `D:/PergyraLang/src/lsp/pgy_lsp.c`의 `publish_diagnostics` call edge다.
- Forbidden fallback: missing version을 `""`로 허용, lexical string compare,
  lower/equal version overwrite, URI-only current check, result completion 시점에
  latest document를 다시 읽어 old result를 current로 위장, native single-document
  global과 self document store의 dual authority.
- First gate: existing
  `D:/PergyraLang/tests/self_hosted/parity/lsp_document_store_parity.sh`를 schema-v2
  revision admission corpus로 확장한다. C/LLVM-built self-host output parity를
  유지한다.
- Grade: probe/fixture는 `REACHABLE`; live installed LSP가 이 owner를 호출하고 old C
  store/publish bypass가 삭제된 뒤에만 해당 slice를 `SUBSTITUTING`으로 검토한다.

### 5.1 Exact falsifying trace

한 input stream에 다음 frame을 넣는다.

1. `didOpen(A, version=10, text="A10")` — applied.
2. `didOpen(B, version=3, text="B3")` — applied.
3. `didChange(A, version=12, text="A12")` — applied.
4. `didChange(A, version=11, text="A11-late")` — `stale_version`, state 불변.
5. `didChange(A, version=12, text="A12-conflict")` —
   `version_payload_conflict`, state 불변.
6. version 누락, 음수, fractional/non-canonical number — 서로 식별 가능한
   fail-closed rejection.
7. 최종 store는 `A@12/A12`, `B@3/B3` 두 row만 가진다.
8. A@11에서 시작한 diagnostic candidate를 A@12 뒤 publish하려 하면
   `stale_generation` 또는 LSP-owned 동등한 typed reason으로 거부한다.

마지막 항목이 핵심이다. input overwrite만 막고 async/worker 결과 publication을
current identity와 비교하지 않으면 Insere atomicity의 절반만 옮긴 것이다.

### 5.2 Negative ratchet

- document store 내부의 `version = ""` fallback 금지
- version을 string lexical order로 비교하는 경로 금지
- `didChange` unknown URI 거부 유지
- state row array cardinality 불일치 거부
- candidate URI/version과 current URI/version cross-wire 거부
- production consumer의 URI-only publish/delete 금지
- native C store와 self-host store를 `new ? old`로 함께 읽는 compatibility 경로 금지

## 6. 후속 후보의 우선순위

| Priority | 후보 | 상태 | 여는 조건 |
| ---: | --- | --- | --- |
| 0 | existing HostTaskSlot start admission | implemented/green | real host adapter consumer 필요 |
| 1 | LSP URI/version latest-only admission과 stale publication guard | next eligible | active semantic rung 종료 뒤 |
| 2 | HostTask publication adapter | proposed | 실제 future/task handle과 publish/delete consumer 선정 |
| 3 | benchmark percentile/raw-sample pattern | tooling candidate | 반복 비용이 감당되는 focused hot path 선정 |
| 4 | post-failure decision | blocked by policy facts | retryable condition, backoff/budget, idempotency, source owner |
| 5 | typed loss/buffer policy | deferred | concrete zone ingress와 손실 허용 계약 |
| reject | scheduler/effect/mailbox/event-bus runtime 전체 | do not port | 두 번째 실행 모델이므로 조건 없음 |

## 7. 그대로 가져오면 안 되는 부분

### 7.1 Retry/supervision은 현재 Pergyra 계약보다 약하다

`F:/insere/AGENT.md`는 retry에 max attempts, retry condition, backoff와 idempotency를
요구한다. 그러나 `F:/insere/src/supervision.ts:61`은 `maxRestarts`만 normalize하고,
`F:/insere/src/api.ts:871`은 error의 retryability, backoff/time budget,
idempotency를 검사하지 않고 remembered task를 다시 시작한다.

따라서 Insere의 “start policy와 supervision policy 분리”는 채택하지만 restart
구현은 Pergyra RetryPolicy의 근거로 사용하지 않는다. Pergyra에서는 기본 stop,
explicit retryable condition, bounded attempts, backoff/budget, idempotency proof가
모두 있어야 한다.

### 7.2 Error/logging 구현은 새 SoT가 될 수 없다

- `F:/insere/src/task.ts:77`은 exception message에 `already exists`가 포함됐는지로
  error code를 고른다. Stable typed identity가 아니다.
- `F:/insere/AGENT.md`는 log에 `errorCode`를 요구하지만
  `F:/insere/src/logging.ts:7`의 record에는 해당 field가 없다.
- `F:/insere/src/logging.ts:58`은 raw `cause`를 record에 싣는다. Pergyra에서는
  secret/PII 가능성을 검토하고 owned diagnostic identity와 safe metadata만
  노출해야 한다.
- disabled logger, reporter failure와 request-id provider failure를 다루는 catch는
  original failure 보존이라는 목적은 좋지만, Pergyra의 established diagnostic
  boundary를 우회하는 generic catch-and-ignore로 복사하면 안 된다.

Pergyra는 기존 compiler diagnostic/runtime observability schema를 계속 소유한다.

### 7.3 Clock과 buffer의 숨은 선택을 복사하지 않는다

- `F:/insere/src/clock.ts:25`는 finite time만 검사하고 역행 time을 허용하므로
  negative delta가 가능하다. Pergyra host clock의 monotonicity 여부는 해당 owner가
  명시해야 한다.
- `F:/insere/src/mailbox.ts:239`의 `queue`는 unbounded다.
- `drop`과 `drop-newest`는 손실을 typed reason으로 반환하지 않는다.
- `F:/insere/src/event-bus.ts:583`의 bounded `drop-oldest`는 첫 Map bucket에서
  제거하므로 cross-key global FIFO를 보장하지 않는다. 그 의미가 필요한 domain에
  일반 queue로 전용하면 안 된다.
- synchronous listener failure가 이후 listener delivery를 중단할 수 있는 것은
  명시된 host-bubble semantics다. Pergyra event 의미로 암묵 복사하지 않는다.

### 7.4 Effect DSL은 Pergyra syntax를 다시 감싸는 계층이 된다

`F:/insere/src/effect.ts:385`의 `ensuring`과 `:398`의
`acquireUseRelease`는 좋은 cleanup test oracle이다. 하지만 Pergyra에는 `defer`,
ownership cleanup, Result propagation과 intent compensation이 있다. `map`,
`flatMap`, `attempt`, `recover`, `ensuring` runtime DSL을 다시 만들면 사용자가 언어
문법과 library effect graph 중 하나를 선택해야 한다.

가져올 것은 다음 falsifier다.

- acquire 실패 시 release하지 않는다.
- acquire 성공/use 실패 시 release한다.
- 여러 cleanup은 정해진 LIFO 순서를 지킨다.
- cancellation cleanup과 intent compensation을 같은 의미로 합치지 않는다.
- cleanup 실패가 original failure를 어떻게 보존/대체하는지는 MIR owner가
  명시한다.

## 8. 완료 판정

이번 감사로 완료된 것은 source 분류와 next slice의 owner/falsifier 선정이다.
다음은 완료되지 않았다.

- Insere scheduler의 Pergyra port
- live host adapter의 HostTask ticket 소비
- self-host LSP live stream과 native C LSP 대체
- post-failure retry policy
- mailbox/event bus stdlib 추가
- hard self-host substitution

Insere의 가장 좋은 설계는 이미 한 번 Pergyra화됐고, 다음 실제 적용점도 기존 LSP
owner 안에서 확인됐다. 앞으로도 “기능을 전부 옮긴다”가 아니라 “실제 consumer가
있는 invariant를 하나의 owner와 negative gate로 닫는다”는 순서를 유지한다.
