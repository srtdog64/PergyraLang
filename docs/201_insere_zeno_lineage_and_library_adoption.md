# 201. Insere·Zeno lineage와 Pergyra library 채택 계약

Updated: 2026-07-30 (Asia/Seoul)

Status: canonical lineage/adoption plan. 구현 상태는 source와 executable gate가
우선하며, 이 문서는 외부 프로젝트를 새 semantic authority로 만들지 않는다.

이 문서는 사용자가 직접 만든 `F:/insere`와 `F:/zeno`에서 Pergyra로 가져올
핵심 불변식을 기록한다. 두 프로젝트는 실제 압력 아래 다듬어진 설계 자산이다.
그렇다고 TypeScript 구현이나 package 구조를 그대로 이식하지는 않는다.
Pergyra의 기존 Slot, MIR ABI/layout, ownership 경계에 맞춰 의미를 번역한다.

현재 `HostTaskSlot`과 그 typed `spawn`/`restart`/`skip` admission policy는
C/LLVM으로 실행되는 official-library slice로 구현되어 `REACHABLE`이고,
`SnapshotTicket`과 `BinaryProjectionPreflight`도 Pergyra로 컴파일되는 internal
library/tooling slice에서 `REACHABLE`이다. Insere-derived URI revision/ticket은
production self-host LSP `Main --document-store-probe`에서 stale diagnostics
publication을 거부하는 bounded `REACHABLE` consumer까지 연결됐다. 이 완료
표시는 각 bounded slice의 구현과 focused gate 완료를 뜻한다. 어느 경로도
C-owned compiler/LSP production path를 삭제하지 않았으므로 `SUBSTITUTING`이
아니다.

검토 기준 snapshot은 Insere `997287030f3cbc64c6f5f8f15053a67cdae4e9a9`
(`main == origin/main`, clean)과 Zeno
`2865f670f711460488db05d29f3bcc0d42d92bfa` (`main`, `origin/main`보다 1 commit
앞섬)이다. Zeno에는 package metadata/example package와 `llms.txt`에 기존 dirty
work가 있으므로 Pergyra 작업이 수정·정리·커밋하지 않는다. 아래에서 인용한
architecture/schema/compiler/runtime source는 그 dirty path에 포함되지 않았다.
이 revision은 provenance 재현용이며 Pergyra 의미 SoT를 외부 저장소에 넘기지
않는다.

상세 전수 감사 근거는 다음 research dossier에 분리했다.

- `docs/research/insere_reuse_audit.md`: typecheck, Vitest 147개,
  restart-storm gate와 current-occupant/latest-generation 불변식.
- `docs/research/zeno_reuse_audit.md`: 26 test files/176 tests, package policy,
  version consistency와 validate-once/immutable-plan, ABI inspect/diff 후보.

두 track은 2026-07-30에 **research intake를 시작해 완료**했다. 이는 두 구현
rung을 동시에 활성화했다는 뜻이 아니다. 현재 executable owner는
`selfhost.semantic_artifact_admission` 하나이며, Insere-derived LSP admission과
Zeno-derived ABI inspect/diff는 그 rung 뒤의 독립된 eligible slice다.

## 0. 공통 채택 원칙

```text
source project pressure
  -> invariant
  -> existing Pergyra fact owner
  -> smallest library/tooling surface
  -> exact negative gate
  -> production consumer
  -> old path deletion
```

- 외부 source는 provenance와 falsifier를 제공한다. Pergyra semantic SoT는
  Pergyra source, registry, ABI/runtime owner, executable gate에 남는다.
- Pergyra에 이미 owner가 있으면 새 runtime, scheduler, layout calculator를
  만들지 않는다.
- stdlib core는 모든 프로그램이 지불할 보편 primitive만 소유한다. 특정 host
  orchestration과 compiler 검사는 official library와 tooling에 둔다.
- 이름과 출력 byte가 같다는 이유로 generation, offset, alignment,
  endianness, layout identity를 생략하지 않는다.
- production bypass가 삭제되기 전까지 hard self-host progress가 아니다.

## 1. Provenance와 source evidence

### 1.1 Insere

Insere의 canonical position은 작은 cooperative scheduler다. host가 clock과
event loop를 소유하고, Insere는 keyed work의 restart/cancel/latest-only 경계를
소유한다.

| Source evidence | 관찰된 불변식 | Pergyra 번역 |
| --- | --- | --- |
| `F:/insere/docs/for-human/README.md` | stable key의 새 작업이 이전 작업을 supersede하며 일반 Promise/worker replacement가 아님 | keyed latest-only commit protocol |
| `F:/insere/docs/for-llm/reference/atomicity.md:31` | restart 뒤 이전 occupant가 새 wait state를 덮거나 새 occupant를 삭제하면 안 됨 | stale ticket의 wait/final/cleanup 거부 |
| `F:/insere/src/core.ts:133` | restart는 active occupant를 취소하고 replacement를 설치 | one-key one-current-generation transition |
| `F:/insere/src/core.ts:548` | aborted 또는 제거된 entry는 결과를 commit하지 않음 | commit 전 current ticket 확인 |
| `F:/insere/src/core.ts:612` | Map의 현재 entry object와 같은 경우에만 삭제 | late cleanup이 replacement를 삭제하지 못함 |
| `F:/insere/src/runtime.ts:403` | 이전 routine이 resume되어도 aborted/index guard 뒤 wait를 쓰지 않음 | late resume/finalizer fail-closed |

중요한 번역 차이가 있다. 현재 Insere core의 직접 guard는 숫자 generation이
아니라 **현재 key occupant의 object identity**다. Pergyra는 이미 runtime
`SlotHandle = <slot_id, generation>`과 release/recycle generation advance를
가지므로 같은 불변식을 explicit generation ticket으로 표현한다. 이는 Insere
코드를 복사한 것이 아니라 atomicity invariant를 Pergyra owner에 맞춰
재표현한 것이다.

### 1.2 Zeno

Zeno의 핵심은 raw buffer API가 아니라 normalized Layout IR 하나가 analysis,
validation, emission, runtime projection, inspection, diff의 입력을 소유하는
구조다.

| Source evidence | 관찰된 불변식 | Pergyra 번역 |
| --- | --- | --- |
| `F:/zeno/docs/reference/architecture.md:75` | normalized Layout IR를 검증한 뒤 emit | AST 직접 projection 금지 |
| `F:/zeno/docs/reference/architecture.md:91` | offsets/strides/descriptor가 consumer에 퍼지는 것이 핵심 실패 | one layout SoT와 derived tooling |
| `F:/zeno/packages/schema/src/index.ts:40` | field name/offset/byteLength/alignment가 한 row에 있음 | 기존 MIR field tuple 재사용 |
| `F:/zeno/packages/schema/src/index.ts:204` | struct layout이 byteLength/alignment/endianness/fields를 소유 | layout identity와 explicit endian 결속 |
| `F:/zeno/packages/compiler/src/layout-manifest.ts:8` | manifest가 field layout과 hash를 inspect/diff 가능하게 투영 | 기존 fact에서만 파생되는 manifest |
| `F:/zeno/packages/compiler/src/layout-manifest.ts:155` | size/alignment/endian/field offset 차이는 breaking | same-name mismatch fail-closed |
| `F:/zeno/packages/runtime/src/lifetime.ts:3` | generation handle이 slot과 generation을 함께 소유 | stale reuse falsifier |
| `F:/zeno/packages/runtime/src/frame.ts:125` | endian/layout hash mismatch를 사용 전에 거부 | projection preflight receipt |

Pergyra에서는 새 Layout IR를 만들지 않는다.
`src/self_hosted/mir_lower/abi_layout_fact_owner.pgy`의
`MirAbiLayoutRowCapture`가 type, size, alignment, field name/offset/size/align을
이미 운반하고 `MirAbiLayoutIdFromCapture`가 tuple identity를 소유한다.
Binary projection은 이 owner를 소비하며 offset을 다시 계산하지 않는다.

현재 MIR layout row에는 concrete endianness가 없다. 첫 slice는 host endian이나
little-endian을 추측하지 않고 boundary가 endianness를 명시하게 한다. 장래
target envelope가 concrete endian SoT를 소유하면 그 fact를 ticket 발급자에게
연결한다. 그 전의 default는 closure가 아니라 숨은 ABI 선택이다.

## 2. Pergyra 계층 배치

| 계층 | 채택 항목 | 현재/목표 owner | 포함하지 않는 것 |
| --- | --- | --- | --- |
| runtime/core | 기존 SlotHandle generation과 MIR layout identity | `src/runtime/slot_manager*`, MIR layout owner | 새 scheduler/layout calculator |
| official library | keyed latest-only `HostTaskSlot` | `stdlib/host_task_slot.pgy`, active core module | Promise runtime, frame clock, mailbox |
| internal self-host library | immutable ticket와 projection receipt | `src/self_hosted/lib/snapshot_ticket.pgy`, `binary_projection_preflight_owner.pgy` | public raw buffer API |
| tooling | executable preflight; 이후 manifest inspect/diff | `src/self_hosted/tools/binary_projection_preflight_probe/` | backend별 layout table |
| future production consumer | compiler artifact/FFI/persistence boundary | 실제 workload 전에는 미선정 | fixture-only 승격 |

`stdlib/host_task_slot.pgy`는 `docs/148_stdlib_architecture.md`의 active core
module이고 beta-stable `use host_task_slot;` surface다. import 계약, API naming,
C/LLVM parity는 닫혔다. 다만 generic `Future<T>` 밖의 source-level task handle을
새로 만들지 않았고 실제 host adapter도 아직 소비하지 않으므로 grade는
official-library `REACHABLE`에서 멈춘다.

`SnapshotTicket`은 compiler owner를 import하지 않고 caller-provided slot,
generation, layout id, endian을 immutable value로 결속한다.
`BinaryProjectionPreflight`만 `abi_layout_fact_owner.pgy`를 import해 기존 layout
identity를 검증한다. 현재 형태는 user stdlib가 아니라 internal self-host
library/tooling이다. stdlib이 `src/self_hosted`를 역방향 import하게 만들지 않는다.

## 3. Objective card A — HostTaskSlot

- Objective: stable key의 현재 작업만 wait/final/cleanup을 commit하게 하고
  교체된 작업이 새 occupant를 덮거나 지우지 못하게 한다.
- Priority: current identity, stale rejection, transition result, cleanup safety,
  host ergonomics, scheduler 기능 수.
- Fact owner: `HostTaskSlot(key, generation, phase, outcomes)`와
  `HostTaskTicket(key, generation)`.
- Last consumer: host adapter의 `PublishWait`, `PublishFinal`, `Cleanup`.
- Forbidden fallback: key만 비교, latest overwrite, stale cleanup,
  cancel-ignore-and-continue, 별도 global scheduler registry.
- Verification: official-library focused C/LLVM parity.

Exact falsifier:

1. key K를 generation N으로 열고 ticket A를 발급한다.
2. K를 N+1로 replace하고 ticket B를 발급한다.
3. A의 `PublishWait`, `PublishFinal`, `Cleanup`은 모두
   `stale_generation`으로 거부되어야 한다.
4. B의 transition만 적용되어야 한다.
5. wrong key, vacant slot, invalid phase는 서로 다른 reason으로 실패해야 한다.

Insere의 worker-result guard는 host adapter 책임이다. `HostTaskSlot`을 worker
pool이나 distributed transaction으로 확대하지 않는다.

## 4. Objective card B — SnapshotTicket

- Objective: immutable snapshot의 slot generation, ABI layout identity,
  endianness를 하나의 admission ticket에 결속한다.
- Priority: runtime Slot identity, stale rejection, existing layout identity,
  explicit endian, convenience.
- Fact owner: runtime `SlotHandle`와 `MirAbiLayoutIdFromCapture`.
- Last consumer: `BinaryProjectionPreflight`.
- Forbidden fallback: slot id만 확인, generation refresh, layout id 0,
  빈/unknown endian default.
- Verification: generation N ticket을 같은 slot N+1에 제시하면 거부.

현재 source의 모든 field는 `let`이다. 발급은 invalid slot/generation/layout id와
`little|big` 이외 endian에 `None`을 반환한다. 이 owner는 slot을
claim/release/recycle하지 않는다.

현재 self-host Pergyra surface에는 `Slot<T>`의 runtime handle identity를 읽는
public typed view가 없다. 따라서 이 slice는 명시적 current slot/generation
fact를 받는 internal protocol이며 public `Slot<T>` API가 아니다. reflection이나
generated-C field access로 이 seam을 우회하지 않는다. 현재 probe에서는 ticket과
current slot/generation을 같은 caller가 제공하므로 live `SlotHandle` 진위나
last-consumer 연동을 증명하지 않는다. 이것은 receipt lineage fixture이지 runtime
handle authenticity 증거가 아니다.

## 5. Objective card C — BinaryProjection

- Objective: byte projection 전에 current snapshot, complete MIR layout identity,
  explicit endian을 검증하고 typed receipt만 발급한다.
- Priority: one layout SoT, mismatch fail-closed, freshness, inspectability,
  hot-path 비용.
- Fact owner: `MirAbiLayoutRowCapture`와 `MirAbiLayoutIdFromCapture`.
- Last consumer: future binary boundary의 `BinaryProjectionReceipt`.
- Forbidden fallback: field offset 재계산, type/name-only equality,
  target-C-default 문자열, implicit little-endian, unchecked entrypoint.
- Verification: changed offset, endian mismatch, stale generation, missing
  endian은 모두 `None`.

Preflight는 layout field array를 직접 읽어 비교하지 않는다. 기존 owner가 계산한
identity를 비교하므로 offset/size/alignment/representation tuple이 바뀌면 같은
type spelling이어도 거부된다. endian은 현재 layout hash에 없으므로 explicit
boundary fact로 별도 비교한다.

## 5.1 Objective card D — HostTask admission policy

- Objective: Insere의 `spawn`/`restart`/`skip` 시작 정책을 기존
  `HostTaskSlot`의 key, generation, phase 사실 위에서 한 번 결정하고, host가
  분기 가능한 typed decision으로 돌려준다.
- Priority: current generation 보존, active/vacant 구분, normal skip과 duplicate
  spawn의 구분, explicit policy, 두 번째 scheduler를 만들지 않는 것.
- Fact owner: `HostTaskSlot`이 current generation과 phase를 계속 소유하고,
  `HostTaskApplyPolicy`와 `HostTasks.ApplyPolicy`가 해당 current slot에 대한 단일
  admission transition을 소유한다. policy decision은 runtime task handle이나
  worker 실행을 소유하지 않는다.
- Last consumer: future host adapter가 existing task/future handle을 시작·교체할지
  결정하는 publication 직전 경계다.
- Forbidden fallback: string policy, key-only presence 판정, duplicate `spawn`을
  restart로 처리, active `skip`에서 generation 증가, rejected decision 뒤 host
  task 시작, Bool-only 결과, Promise/Generator/AbortController나 별도 scheduler
  도입이다.
- Verification: active generation N에서 `skip`은 slot/ticket을 보존하고 normal
  no-op으로, `spawn`은 `task_already_exists`로 거부되며, `restart`만 N+1을
  발급해야 한다. cleanup 뒤 vacant slot에서는 세 정책 모두 같은 N+1 start를
  결정해야 한다. generation 0/negative, unknown phase, 증가 상한은 변화 없이
  거부되고 unknown phase는 `vacant`로 보이면 안 된다. 상한의 active `skip`은
  증가하지 않으므로 계속 허용된다. focused C/LLVM gate가 동일한
  decision/status, phase, generation/ticket 결과를 실행한다.

이 slice에는 `subject`, `action`, `intent`를 추가하지 않는다. library가 실제 host
task identity나 실행 authority를 소유하지 않고 immutable admission decision만
계산하기 때문이다. decision/report를 `tobject`로 만드는 것도 아직 이르다. 실제
host boundary publication과 receipt lifecycle이 없으므로 현재 결과는 local policy
value이며, future adapter가 detached handoff를 요구할 때 별도의 materialization
owner를 증명해야 한다.

## 5.2 Objective card E — self-host LSP latest document publication

- Objective: production self-host LSP entrypoint의 `--document-store-probe`가
  URI별 문서 revision을 typed monotonic fact로 admission하고, superseded
  generation에서 끝난 diagnostics 후보가 최신 문서 publication을 덮지 못하게
  한다.
- Priority: URI별 version monotonicity, same-version payload identity, current
  HostTask generation, stale publication rejection, 기존 LSP transport/JSON fact
  owner 재사용, output 편의성.
- Fact owner: `LspDocumentRevision`이 URI, numeric version, exact text,
  `HostTaskSlot`, current ticket을 한 record로 소유한다.
  `LspDocumentRevisionChange`가 version/payload admission을,
  `LspDocumentPublicationAdmissionFor`가 마지막 publication 결정을 소유한다.
  `HostTaskSlot`은 generation identity만 소유하며 LSP version이나 text를
  재판단하지 않는다.
- Production entrypoint / last consumer: `src/self_hosted/lsp/main.pgy`의 기존
  `--document-store-probe` route와 그 `LspDocumentStoreJson` publication-admission
  artifact다.
- Direct bypass to delete: `document_store_owner.pgy`가 URI/version/text 평행
  배열을 직접 `ArraySet`하고 version order나 current ticket 없이 mutation을
  성공 처리하는 경로.
- Forbidden fallback: version 문자열 비교, lower/equal version overwrite,
  same-version different-text acceptance, URI-only latest 판정, stale candidate의
  payload equality만으로 publication 허용, rejected change 뒤 partial document
  mutation, 별도 scheduler/runtime 도입이다.
- Verification: `A@10`, `B@3`, `A@12`, stale `A@11`, conflicting
  `A@12(other text)` 순서에서 최종 state는 정확히 A@12/B@3이고 errors는 두
  종류로 구분된다. 지연 완료 후보 A@10은 `stale_generation`, B@3과 A@12만
  publication admission을 얻어야 하며 C/LLVM artifact가 byte-equal해야 한다.

이 경계는 실제 self-host LSP `Main`에서 실행되므로 bounded `REACHABLE`이다.
하지만 buffered probe가 live editor loop나 C-owned released LSP path를 삭제하지
않으므로 `SUBSTITUTING`은 아니다. `tobject` publication receipt도 아직 만들지
않는다. 현재 artifact는 admission 관측값이며 실제 JSON-RPC
`publishDiagnostics` 송신 transaction이 생길 때 detached receipt owner를 연다.

현재 MIR-only self-host C path는 외부 stdlib import 안의 namespace-qualified
`HostTasks.*` call을 nested owner body에서 아직 resolve하지 못한다. 따라서 이
internal owner는 그 parser-owned normalized callable identity인
`HostTasks_Open`/`HostTasks_ApplyPolicy`/`HostTasks_IsCurrent`를 직접 소비한다.
이는 policy 재구현이나 두 번째 API owner가 아니라 현재 call-target carriage
blocker다. namespace call-target가 닫히면 의미 owner 변경 없이 source spelling만
canonical `HostTasks.*`로 되돌리는 것이 다음 DX ratchet이다.

## 6. 단계별 구현 계획

### Phase 0 — 현재 구현된 bounded slice

| Track | 상태 | Evidence | Grade |
| --- | --- | --- | --- |
| HostTaskSlot | generation guard와 typed admission policy 구현·배치·focused parity 완료 | `stdlib/host_task_slot.pgy`, `tests/host_task_slot_smoke.sh`, `tests/host_task_policy_smoke.sh`, `docs/stdlib/host_task_slot.md`, stdlib inventory/stable-use wiring | `REACHABLE` official library |
| LSP latest publication | URI/version/text/ticket revision admission과 stale diagnostics publication rejection이 production self-host LSP Main에 연결됨 | `src/self_hosted/lsp/document_revision_owner.pgy`, `document_store_owner.pgy`, `tests/self_hosted/parity/lsp_document_latest_publication_parity.sh` | bounded `REACHABLE` self-host LSP |
| SnapshotTicket | caller-provided identity의 immutable 결속과 generation rejection 구현; live handle authenticity는 미연결 | `src/self_hosted/lib/snapshot_ticket.pgy` | `REACHABLE` tooling/library |
| BinaryProjection | existing layout id + explicit endian preflight 구현 | `src/self_hosted/lib/binary_projection_preflight_owner.pgy` | `REACHABLE` tooling/library |
| executable probe | C와 LLVM 동일 positive/negative 실행 | `src/self_hosted/tools/binary_projection_preflight_probe/main.pgy`와 `intent.md` | focused evidence |
| parity owner | N/N+1, offset, endian, missing-endian negative | `tests/self_hosted/parity/binary_projection_preflight_probe_parity.sh` | PASS |

`REACHABLE`은 official-library fixture 또는 tooling entrypoint가 실제 Pergyra
C/LLVM 프로그램으로 실행된다는 뜻이다. production compiler root가 소비한다는
뜻이 아니다. LSP latest slice도 production self-host LSP `Main`의 buffered mode가
소비하지만 C-owned released LSP를 대체하지 않으므로, 어느 track도 hard self-host
`SUBSTITUTING` 진척으로 세지 않는다.

### Phase 1 — HostTaskSlot production adoption

- 완료: source C/LLVM parity와 A(N) → B(N+1) 뒤 A의
  wait/final/cleanup 거부.
- 완료: active `skip`의 normal no-op, duplicate `spawn`의 typed rejection,
  active `restart`의 generation advance, vacant 세 정책의 새 generation 발급,
  generation 0/negative·unknown phase·증가 상한의 fail-closed 처리가 하나의
  `ApplyPolicy` owner와 C/LLVM parity로 고정됐다.
- 완료: Result/reason vocabulary, active module inventory, stable-use wiring.
- 완료: self-host LSP `Main --document-store-probe`가 URI별 typed revision 옆의
  ticket을 소비하고 diagnostics publication 직전에 current slot을 재확인한다.
  A@10/B@3/A@12 뒤 A@10 publication과 stale/conflicting change가 거부된다.
- 다음: live LSP read-exact loop가 동일 owner를 소비하고, 실제 diagnostics
  computation completion이 candidate ticket을 운반한 뒤 released C LSP direct
  document mutation을 삭제한다.
- Insere scheduler 전체를 재작성하지 않는다.

### Phase 1.1 — post-failure supervision (`PROPOSED`)

- Insere의 `spawn`/`restart`/`skip` start admission과 실패 뒤의
  `bubble`/`stop`/`result`/bounded restart를 같은 policy로 합치지 않는다.
- 미래 `HostTaskFailureDecision`은 attempt와 explicit max를 순수하게 판정한다.
  Restart는 remembered execution source와 bounded max가 모두 있을 때만 후보며,
  기본값은 stop이다.
- 실제 host adapter가 source/attempt를 소유하기 전에는 fixture를 추가해도
  `REACHABLE` 이상으로 세지 않는다.
- Promise/AbortController callback, 두 번째 scheduler, unbounded retry,
  `ApplyPolicy` 재사용은 금지한다.

### Phase 2 — normalized layout manifest tooling

- MIR ABI capture에서 manifest를 projection하고 layout을 계산하지 않는다.
- schema/version, target identity, explicit endian, layout id를 포함한다.
- inspect는 presentation이며 manifest/fact가 authority다.
- same-name byteLength/alignment/endian/field offset 변화는 breaking이다.
- 추가 type/field는 explicit version routing policy가 있을 때만 safe 후보다.

### Phase 3 — real BinaryProjection consumer

- compiler artifact, FFI packet, persistence frame 중 실제 workload 뒤 consumer를
  고른다.
- receipt 없이는 buffer를 열거나 output을 truncate하지 않는다.
- zero-copy이면 Slot pin/read-only view owner와 lifetime을 결속한다. ticket만으로
  concurrent mutation 안전을 주장하지 않는다.
- production이 receipt path를 사용하고 unchecked bypass가 삭제된 뒤에만
  `SUBSTITUTING` 승격을 검토한다.

### Phase 4 — resolution/loss-aware diagnostic (`PARTIAL/SURFACE`)

- 새 error taxonomy를 만들지 않고 기존 self semantic diagnostic fact에
  loss-contract row id, observed/required stage와 phase를 결속한다.
- `ambiguous`, `insufficient resolution`, `unsupported at phase`,
  `produced-IR invariant violation`을 precise code와 별도 축으로 구분한다.
- initializer/assignment/statement unresolved fixture가 AST 재탐색이나 fake line
  0 없이 같은 diagnostic owner를 소비해야 한다. Production self-host driver가
  소비하기 전에는 hard self-host 진척이 아니다.

## 7. Do not port

Insere에서 가져오지 않는다:

- Generator/Promise/AbortController runtime
- host frame clock과 `tick()` ownership
- mailbox, event bus, group-prefix scheduler 전체
- worker pool, CPU parallel/shared-memory coordination
- editor/document/cursor/CRDT/product policy
- JS object identity 구현 자체와 무제한 retry
- start admission과 post-failure supervision을 한 policy로 합치는 것

Pergyra는 이미 `async`, `parallel`, `Channel<T>`, Slot, zone, intent, runtime
scheduler owner를 가진다. `HostTaskSlot`은 작은 latest-only commit protocol이며
두 번째 scheduler가 아니다.

Zeno에서 가져오지 않는다:

- TypeScript AST analyzer와 `.zeno.ts` grammar
- DataView/ArrayBuffer/TypedArray API의 semantic-core 노출
- SharedArrayBuffer, Atomics, shared tail arena
- pointer32, descriptor32, mutable rebase view
- Zeno hash를 두 번째 layout identity로 복사
- emitted accessor의 독립 raw offset table
- implicit little-endian default
- renderer/WebGPU/Three.js/scene/ECS behavior

가져올 것은 Layout IR의 **소유권 구조**와 generation-bound projection
admission이지 JavaScript memory API가 아니다.

Zeno의 capability-derived runtime import/emission 원칙은 이미 Pergyra의
`CodegenRuntimeUsageFactsFromSemantic`과 선택적 runtime emission owner에 대응한다.
따라서 이 축은 새 registry 채택 대상이 아니며 기존 owner의 negative gate만
유지한다.

## 8. Gate와 완료 정의

현재 관찰된 focused evidence:

- `bash tests/self_hosted_scaffold_smoke.sh`: `35 tool(s) gated`.
- `PGY_HOST_TASK_SLOT_BACKENDS=c bash tests/host_task_slot_smoke.sh`: C
  compile/run PASS.
- `PGY_HOST_TASK_SLOT_BACKENDS=llvm bash tests/host_task_slot_smoke.sh`: LLVM
  compile/run PASS.
- `PGY_HOST_TASK_POLICY_BACKENDS=c bash tests/host_task_policy_smoke.sh`: active
  skip/spawn/restart, vacant start, malformed phase/generation C 실행 PASS.
- `PGY_HOST_TASK_POLICY_BACKENDS=llvm bash tests/host_task_policy_smoke.sh`:
  동일 decision/status와 generation/ticket 결과 LLVM 실행 PASS.
- `PGY_SELFHOST_LSP_BACKENDS=c bash
  tests/self_hosted/parity/lsp_document_latest_publication_parity.sh`: production
  `Main --document-store-probe`의 monotonic revision과 stale publication C PASS.
- `PGY_SELFHOST_LSP_BACKENDS=llvm bash
  tests/self_hosted/parity/lsp_document_latest_publication_parity.sh`: 동일 artifact
  LLVM PASS; C/LLVM byte parity PASS.
- 기존 `lsp_document_store_parity.sh`와 `lsp_session_state_parity.sh`도 갱신된
  revision/publication artifact로 C/LLVM PASS.
- `make stdlib-test-smoke`가 lifecycle과 policy gate를 모두 active stdlib CI
  surface에 결속한다. `host-task-slot-test-smoke`와
  `host-task-policy-test-smoke`는 focused target이다. 두 focused fixture가
  stable `use host_task_slot;` 경계에서 lifecycle과 read-only skip decision을
  각각 실제 소비한다.
- `bash tests/stdlib_inventory_smoke.sh`: inventory/contracts PASS.
- `PGY_BIN=bin/pgy.exe bash
  tests/self_hosted/parity/binary_projection_preflight_probe_parity.sh`:
  C compile/run PASS, LLVM compile/run PASS, output parity PASS.
- positive: current generation, exact existing layout id, matching explicit
  endian은 receipt를 발급한다.
- negative: N ticket after N+1, changed offset, endian mismatch, missing endian은
  receipt를 발급하지 않는다.

Track 하나를 complete로 부르려면 named owner, missing-fact failure, old-path
deletion, negative ratchet, real last consumer가 모두 필요하다. self-host track은
추가로 Pergyra implementation이 C-owned production path를 대체해야 한다.

현재는 focused library/tooling evidence와 buffered self-host LSP entrypoint의
bounded latest-only evidence가 존재한다. 두 역작의 좋은 설계를 Pergyra 방식으로
채택하기 시작했지만 released LSP, compiler 전체나 stdlib가 이 설계로 완성됐다고
주장하지 않는다.

## 9. 다음 falsifier 순서

1. 현재 `selfhost.semantic_artifact_admission`의 2.9MB/5.1MB fixed-cap pressure와
   immutable-after-admission caller ratchet을 닫는다.
2. self-host LSP의 live read-exact loop와 실제 diagnostics completion이 현재
   revision ticket을 운반하게 하고 released C LSP의 direct document mutation을
   삭제한다. Buffered `A@10/B@3/A@12/stale A@11/conflicting A@12` corpus는 완료다.
3. Zeno-derived slice로 기존 `MirAbiLayoutRowCapture` exact tuple에서만 ABI
   inspect/diff를 파생하고 offset/size/alignment/endian mutation과 ID collision
   drift를 fail-closed한다. 새 layout IR/hash는 만들지 않는다.
4. 실제 host adapter가 `HostTaskSlot` ticket을 publication/cleanup 경계에서
   소비하고 key-only direct commit/delete 경로가 없음을 gate로 고정한다.
5. public `Slot<T>` generation view는 실제 workload가 필요성을 증명할 때만
   설계한다. raw field 노출은 금지한다.
6. post-failure supervision은 explicit attempt/max/source owner가 생긴 뒤에만
   bounded decision fixture로 연다. Start admission을 retry policy로 재사용하지
   않는다.
7. 첫 real consumer에서 receipt 없는 direct open/truncate/read를 차단한다.
8. loss-aware diagnostic은 기존 diagnostic/loss-contract owner에 붙이고 새 오류
   taxonomy나 AST reread를 만들지 않는다.
9. production consumer와 old bypass 삭제 뒤에만 `SUBSTITUTING`을 검토한다.
