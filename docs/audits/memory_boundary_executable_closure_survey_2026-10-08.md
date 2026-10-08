# 메모리 경계 실행 모델 폐쇄 — 읽기 전용 chain survey

상태: **READ-ONLY 관찰과 설계 후보; 구현 승인 또는 CLOSED 아님**.
기준 HEAD `3658548d24bca3d721e4f1974ac7a10da99f7aa8`, 공유 dirty tree.
사용자의 원요청은 footprint 발급 → 물리 주소 배치 → 성장 → 정리를
실행 알고리즘과 증명으로 연결하는 것이다. 현재 모델/소스만 조사했고
이 문서의 후보 알고리즘은 구현하지 않았다.

## 실제 소유 경로

- `OwnershipCleanCore`: 유일한 backing 어휘 `Block = (allocation step,node
  index)`, `TEnv`의 owned footprint, `H`, `free`와 `INV`.
- `OwnershipGraphLinks`: 기존 `Store`, `store_fp`, 전체 `gheap` projection,
  node/store identity와 `gexec_framed`. 추가 interpreter를 만들 필요 없다.
- `SlotCalculus`: root incarnation, live/pin/token admission와 tombstone.
  `MaxSlotId`/`verify_token` 이외의 새 추상 가정은 필요하지 않다.
- `MemoryBoundaryCompositionAudit`: 합성 consumer다. issuer/물리 allocator가
  없으며, `RootBinding`, `HeapSplit`, `placement`, `RetireReady`를 전제로 쓴다.

소스 확인 위치: Core `Block`/`heap_of`/`free`/`INV`(151,420,424,892),
Graph `g_insert`/`gexec_framed`/`GInv`(202,350,882), Audit
`RootBinding`/`placement`/`AccessValid`/`HeapSplit`/`RetireReady`/
`growth_allocation_agrees_split`/`retirement_agrees_split`
(40,60,158,259,262,353,384), Slot `Step_Reclaim`/`HandleRelease`(120,201).

## 구현 전에 정해야 하는 두 경계

1. **실제 root 결합 발급.** 현재 binding은 `(Handle,TVar)`다. AccessValid는
   그 handle이 읽기 가능하고 canonical owner footprint가 store footprint와
   같다고 검사하지만, 그 Slot incarnation 자체가 그 graph sid의 root로
   발급됐다는 관계를 검사하지 않는다. Slot의 `s_val`을 sid로 해석할지,
   실제 mint/registration transition이 만든 immutable root fact를 쓸지는
   선택이 필요하다. 일반 Slot value를 임의로 sid로 재해석하면 안 된다.
   발급 이후 Slot.Write나 owner replacement가 이 관계를 깨면 재검증·퇴역
   또는 명시적 거부여야 한다. live generation만으로 binding을 추정하지 않는다.
2. **frame의 granularity.** Audit `HeapSplit`은 모든 graph store를 포함하는
   `GL.gheap`를 사용하지만 root binding은 `store_fp sid` 하나만 소유한다.
   `H minus one root footprint`가 지금 HeapSplit을 만들려면 single-root/
   single-store profile을 명시해야 한다. 여러 store면 전체 admitted root
   inventory가 필요하거나 per-store graph projection으로 경계를 바꿔야 한다.
   몰래 single-store를 일반화해서 증명하면 안 된다.

추가 shape 경계도 있다. Core `laid`는 footprint 길이 = `vsize c`를 요구한다.
Graph node payload는 임의 길이 `nblocks`를 받는다. 일반 graph mutation
후 canonical root 내용을 어떻게 만들어 `laid`를 유지하는지는 아직 owner가
없다. 최소 profile은 node당 한 unit block과 그에 맞는 explicit value/layout
projection을 택할 수 있다. 그 경우 byte layout 일반성은 주장하지 않는다.

## 남은 전제와 최소 실행 인터페이스 후보

| 구간 | 현재 증거 | 아직 실행으로 도출하지 않는 것 |
| --- | --- | --- |
| 발급 | GInv, canonical INV, HeapSplit에 따른 framed guard | root mint/binding, 누락 없는 Ho, snapshot lifetime |
| 배치 | placement를 전제로 이름 alias가 없다는 정리 | finite arena의 빈 주소 선택, 범위·비중첩, 실패 시 무변경 |
| 성장 | graph delta와 canonical `free`를 합성해 HeapSplit 보존 | old contents 복사 순서, staging reservation, 실패 원자성 |
| 정리 | canonical/graph/Slot의 같은 footprint 퇴역을 합성 | 하나의 payload retirement commit/event와 stale/double 호출 거부 |

### 1. checked footprint issuer

single-root profile 후보에서는 다음을 검사하고 성공 결과를 발급한다.

```text
issue_boundary(current owners, root incarnation)
  owned tlookup(root) + actual minted graph/root identity
  exact graph-root footprint + complete canonical live H
  Ho := canonical free(graph footprint, H)
  -> admitted boundary | typed refusal
```

`OC.INV`의 전체 H는 `heap_of rho ++ R`다. R은 suspended caller의 소유이고
borrowed beta footprint는 그 안에 있다. 따라서 Ho를 단순히
`heap_of(tremove root rho)`로 발급하면 안 된다. H에서 graph footprint를
제외해 R까지 남기고, INV+GInv+root correspondence로 graph footprint ⊆ H와
HeapSplit을 **도출**한다. 없는 root/shape/identity/footprint가 `[]`를 발급하는
fallback은 금지한다. serial profile에서는 매 연산 current snapshot에서
검사하는 것이 가장 작다. 준비/commit 사이 변화를 허용하면 epoch가 필요하다.

### 2. finite placement owner

backing H를 복제하지 않는다. `Block -> optional address`는 H의 물리
materialization metadata이고 occupancy는 live H와 준비 예약에서 유도한다.
유한 주소 `0..capacity-1` 중 사용하지 않은 위치를 결정적으로 선택한다.
빈 위치가 모자라면 unchanged + typed exhaustion이다. dead 주소는 재사용한다.

필수 정리는 successful issuance의 coverage/range/injectivity, live 기존
placement 보존, failure equality, free가 다른 owner placement를 보존함이다.
다중 block allocation은 전체 계획을 만든 후 commit해야 중간 부족이
절반만 할당한 상태를 남기지 않는다. 새 전체 H나 graph interpreter는 없다.

주의: unit-block object arena와 byte arena는 다른 claim이다. 실제 byte
주소·extent까지 닫으려면 layout owner가 양의 extent를 발급하고 interval
범위·비중첩을 증명해야 한다. 시작 주소 injectivity만으로 byte overlap은
배제되지 않는다. 가장 작은 bounded profile이 unit arena라면 그 제한을
명시해야 한다. `placement`를 checker가 원하는 값으로 선언하는 것은 알고리즘이 아니다.

### 3. prepare/copy/commit growth

지금 `g_insert`는 logical old table을 `free`한 뒤 payload/table이 그 Block을
재사용하도록 허용한다. 이것만으로 실제 old table read가 free 전에 끝났다는
주장은 나오지 않는다. 현재 성공 fixture도 old table Block을 새 node로 즉시
재사용하므로 아래 두 선택 중 하나가 필요하다.

- 가장 작은 refinement: old table이 아직 살아 있을 때 새 table/payload의
  주소를 확보하고 copy를 끝낸 후 publish/retire한다. 현재 old-table Block
  재사용 후보는 준비 중 거부하며 **다음 연산**에서 주소 재사용은 허용한다.
  spare storage가 없으면 post-state가 들어갈 수 있어도 growth는 원자적으로
  실패한다. 그 비용을 숨기지 않는다.
- 같은 연산 안의 재사용까지 유지: old table의 실제 owned snapshot이나 새
  table copy가 살아 있는 staging을 먼저 만들고, old table을 퇴역한 후
  해당 주소를 새 node/table에 배정한다. snapshot을 '그냥 ghost list'로
  가정하면 실제 copy-before-free 증명이 아니다. staging footprint와 정리
  책임도 canonical owner/place 계약 안에 들어가야 한다.

어느 쪽이든 wrapper는 existing `gexec_framed`를 authoritative transition으로
사용하고 그 pre/post delta를 실제 배치와 연결한다. 필수 정리는 old table
read의 live witness, copy snapshot 동일성, unchanged failure, external frame
보존, committed result의 canonical/graph/placement correspondence다.
full scalar payload/byte copy refinement는 선택한 profile 밖이면 OPEN이다.

### 4. one retirement obligation

실행 인터페이스는 root/store/binding/generation/token/pin/borrow를 모두
확인한 뒤 하나의 payload retirement를 commit한다. `OC.free`는 whole H에
한 번 적용하고 Graph drop은 그 projection임을 증명한다. Graph free와
canonical free를 두 번의 실제 payload deallocation으로 번역하지 않는다.
Slot tombstone과 root fact 소비도 같은 serial commit에 속한다.

old 주소/Block은 나중에 다시 할당될 수 있으므로 전체 역사에서 단순
`NoDup freed Block`를 주장하면 안 된다. '한 번'의 단위는 **현재 살아 있는
allocation/owner incarnation의 정리 의무**다. 새 materialization이 그 주소를
재사용해 얻은 의무와 이전 의무를 구별하고, released binding/Slot handle의
두 번째 소비는 before-state 그대로 거부한다. existing stale-handle theorem을
이 consumer에서 사용한다. 검사와 commit 사이 개입은 serial profile에서
제외하거나 실제 locking/epoch protocol로 정해야 한다.

## 가장 작은 정직한 CLOSED 경계 후보

한 serial root, canonical ordinary frame R, finite **unit-object** arena,
명시적 root mint, node/root generations와 exhaustion, observable/fallible
finalizer 없음, async/worker/FFI 없음이라는 profile은 구현 가능한 최소 후보다.
두 기존 Slot abstractions에 상대적인 결과임을 적는다. 세 모델의 product
state와 address metadata는 refinement carrier이며 별도 backing heap의 소유자가 아니다.

admit → place → insert/grow → release를 실제 함수로 한 번 연결하고, 성공
및 missing/wrong-root/forged-frame/stale-generation/live-pin/live-borrow/
finite exhaustion/failing-prepare/double-retire 반례를 permanent typed consumer와
fresh kernel에서 실행·검증해야 한다. 필요한 새로운 의미 owner는 existing
Graph composition/refinement owner 한 곳으로 고정하고 Audit은 consumer로
남기는 것이 최소다. 구체적인 byte allocator나 C/LLVM 구현, 메모리 성능
우위, whole-language safety 또는 SoT closure로 확대하면 안 된다.
