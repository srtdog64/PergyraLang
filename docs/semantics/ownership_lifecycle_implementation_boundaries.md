# 소유권 생애 알고리즘: 구현 경계와 착수 조건

상태: **구현 전 경계 지도. production 연결 OPEN.** 2026-10-09.
이 문서는 새 의미 owner, 새 SoT 행, 다음 self-host rung을 만들지 않는다.
[27](27_ownership_clean.md)·[28](28_memory_boundary_composition.md)·
[29](29_action_scoped_references.md)와 각 Rocq 모델이 의미를 소유한다.
[209번 알고리즘 설명](../209_ownership_lifecycle_algorithm_code.md)의 각 화살표를
실제로 연결하기 전에 필요한 증거를 모은다. 항목 수는 진척 지표가 아니다.
graph 모델의 slot/index는 내부 저장소 좌표다. 기존 Pergyra `Slot` 경계 의미를
대체하거나 일반 값·링크마다 새 `Slot`을 요구하지 않는다.

## 1. 무엇을 자동화하는가

기본 정책은 **소유권 기반 자동 메모리 관리**다. 사용자가 매번 수명을 쓰거나
deep-drop을 연결하지 않고, 컴파일러가 사용·이전·반환·실패 경로에서 정리를 만든다.
보장과 비용을 구분하면 다음과 같다.

| 층 | 판단의 주인 | 보장 범위 | 여기서 뜻하지 않는 것 |
|---|---|---|---|
| 일반 값 정리 | MIR 소유권 사실 / `OwnershipClean*` | 채택된 모델의 move/copy·마지막 사용·출구 정리 | 모든 production 경로 구현 완료 |
| 트리·store 종료 | cleanup 권한 / 정확한 해체 단위 | 소유자 사건으로 정확한 단위를 끝냄 | 비소유 링크가 없어지자마자 회수 |
| 행동 범위 참조 | `OwnershipGraphActionScope` | 붙잡은 동안 destroyer 거절, 단계가 획득한 hold 반환 | 본문 연산이 모두 성공하거나 실패 효과가 취소됨 |
| 선택적 store 국소 회수 | checked roots / maintained counts / authority | 전제 아래 도달 가능한 대상을 회수하지 않음 | 기본 정책이 tracing GC, 물리적 후보 전용 순회, 총 작업량 상한 |
| 효과 복구 | 도메인 효과 계약 / compensation | 해당 계약으로 별도 입증해야 함 | 메모리 안전 증명이나 보상 함수 종료가 복구 증명 |

선택적 도달성 회수는 **GC 계열 기능**이다. 이를 숨기지 않는다. 일반 값 정리가
tracing GC가 아니라는 말과 모순되지 않으며, 이 선택 기능을 기본 정책의 숨은
RC/GC 폴백으로 쓰지 않는다. 어느 쪽의 보편적인 속도 우위도 주장하지 않는다.

```text
source의 값·호출·제어 흐름
          │ 컴파일러가 한 번 발급한 소유권/루트/hold 사실
          ▼
  MIR 계약 ────────► C / LLVM 소비자
          │              │
          │              └─ 누락 시 재추론·기본값 폴백 금지
          ▼
  획득 → 본문 → 자기 hold 반환 → 결과와 실행 prefix
                   │                    │
                   │                    └─ 효과 복구는 별도 계약
                   ▼
  권한 + 정확한 단위 + lease/pin + 현재 세대
                   │
          전체 사전 검사 → 안정된 커밋 → 은퇴/재사용 공개
```

이 도표는 현재 production 배선도가 아니라 필요한 연결이다. 범위·세대가 다른
사실을 한 묶음으로 읽거나 예전 사실을 `new ? old`로 대체할 수 없다.

## 2. 실패와 복구의 경계

### 2.1 단계 결과는 트랜잭션 영수증이 아니다

`run_step`은 `StepReceipt { step_out, completed }`를 낸다. `completed`는
**성공한 본문 접두부 길이**다. 성공한 읽기도 센다. 순수 읽기 3개와 외부 효과
3개를 같은 것으로 분류하지 않으며, 이 수만으로 남은 효과량을 계산할 수 없다.
본문 정체성과 그 실행 세대에 묶어 해석해야 한다. 재실행 명령이나 undo log도 아니다.

- `AcqFailed`: 본문 접두부 0. 부분 획득한 hold를 놓는다.
- `BodyRefused`: 해당 graph 원시 연산은 상태를 바꾸지 않지만 앞서 성공한 본문은
  이미 실행됐다. 본문 전체의 rollback을 뜻하지 않는다.
- `DerefFailed`: 거절 사실과 성공한 prefix를 남긴다. `GInv`와 `step_ok`를 만족한
  단계에서는 이 결과가 나오지 않는다는 것이 현재 정리다.
- 모든 경로에서 자기 hold만 반환한다. 들어오기 전부터 존재한 바깥 hold를 없애지 않는다.

`run_body_receipt_sound`는 접두부가 거절을 무시하지 않고 성공한 연산들로 실제
도달한 상태임과, 나머지 첫 연산의 실패 또는 정상 끝을 증명한다. 기록을 단지
0으로 채우는 구현은 이 정리를 만족하지 못한다. runtime에서 모든 연산을 로그로
저장하라는 요구가 아니다. 더 작은 표현은 그 관측을 보존하는 정제로 선택한다.

### 2.2 보상 종료와 복구 성공을 분리한다

`SagaCompensationFinished(k, failed_forward)`는 **완료했던 단계의 보상 실행이
끝났다**는 뜻일 뿐, `SagaDone`이나 복구 성공이 아니다. 실패한 현재 단계의
부분 효과는 남을 수 있다. 그 단계의 전체 보상을 무조건 실행하면, 아직 수행하지
않은 효과까지 되돌리는 별도 결함이 생기므로 자동 호출하지 않는다.

`SagaStuck(k, j, failed_forward, failed_compensation)`은 두 실패 영수증을 모두
보존한다. 보상 자체도 일부 실행 후 실패할 수 있다. 최초 실패를 보상 실패로
덮어쓰거나, 이 결과를 성공/빈 no-op로 번역하거나, 자동 재시도하면 안 된다.

복구가 필요한 production 소비자는 **명시적 미복구/복구 미확인 분기**를 가져야 한다.
그것을 넘어서 성공으로 내보내려면 다음 중 실제 도메인 계약을 먼저 선택·증명한다.

| 구현하려는 계약 | 선행 의무 | 금지 |
|---|---|---|
| 단계 내부 원자성 | 준비 중인 쓰기·할당·외부 효과를 공개하지 않음; 모든 실패 지점의 사전 상태 보존 | 물리적 free 후 옛 `GS` 값을 돌려 rollback이라고 부르기 |
| 부분 진행 보상 | 실행 세대·effect identity에 묶인 내역, 실제 수행분만 보상, 중복 실행/실패 처리 | 전체 단계용 보상을 임의 prefix에 적용 |
| 도메인 상태 복구 | 관측 대상·허용 손실·외부 효과·간섭 가정·사후조건과 보상 코드 정제 | 함수 이름/종료/`step_ok`만으로 inverse 승인 |

현재는 위 세 방식 중 하나를 전 언어 기본으로 채택하지 않았다. 메모리 정리는
자동으로 하되 임의의 결제·파일 쓰기·네트워크 효과를 자동 취소했다고 주장하지 않는다.
`CompensationCore.v`의 snapshot 정리는 이상화된 별도 인터페이스다. 이 모델의
임의 compensation 프로그램을 거기에 연결하는 증명은 OPEN이다.

## 3. 반드시 연결해야 하는 경계

모든 행에서 **producer가 발급한 현재 사실 → 마지막 consumer → 소비/은퇴**를
확인한다. 모델의 호출 인자가 존재한다는 이유로 production producer가 생기지 않는다.

| 경계 / 기존 주인 | 필요한 입력·수명 / 마지막 소비자 | 누락·실패 처리 / 반증 입력 | 현 상태 |
|---|---|---|---|
| 일반 값 → MIR 정리: 27, `OwnershipClean*` | binding identity, mode, call/return/inout, 모든 계속 가능한 출구의 live/borrow 사실; cleanup lowering | 누락 사실 진단. 분기 한쪽 이동, 에러 반환·break/continue, alias/view를 같은 입력으로 검사 | 값 정리 모델 존재; 실제 P1은 기존 active handoff가 관리 |
| 본문 → 획득 집합: 29, `step_ok` | 본문이 읽는 각 링크와 읽기/쓰기 모드; 단계 실행과 release | 획득되지 않은 이웃 읽기 거절. A를 붙잡았다고 A의 edge 대상 C가 보호되지 않음 | 집합은 모델 입력; source/MIR 추론 OPEN |
| 획득 → borrowed storage: `GraphLinks` | 정확한 store/slot/generation과 유효 borrowed table; 마지막 사용까지 | 충돌하는 write/delete/drop/table growth 거절. 바깥 hold를 잘못 pop하는 경우 반증 | `gbor`는 ghost state; runtime counter/table을 의무화하지 않음; 표현 정제 OPEN |
| 단계 실패 → recovery dispatch: `ActionScope` | 동일 본문의 prefix/실패 영수증; saga와 호출자 | 부분 insert 후 실패, no-op 보상, 보상 중 부분 실패를 성공으로 번역하지 않음 | 모델·독립 소비자에서 검사; source effect 발급/도메인 복구 OPEN |
| cleanup 권한 → graph retirement: 28, `TeardownAuthority`, `ledger_rights` | store identity ↔ root/epoch/holder의 권위 있는 binding; lease/pin·정확한 단위; 마지막 retire | 외부에서 만든 bindings/복사 핸들/권한 이전 전 holder 거절; rooted/pinned 후손을 후보 확장으로 끼워 넣지 않음 | 권한 투영만 존재; binding issuer·forest/graph 동시 은퇴·lease 연결 OPEN |
| 루트 → 선택적 회수: `RootCompleteness` | `rcheck`/`SimInv`에서 발급한 L, R, caller frames, 결과·aggregate/view/closure 링크; 같은 generation의 회수 | L/R을 모두 임의 빈 목록으로 주지 못함. 반환/중단 caller/aggregate 안의 단독 링크 누락 반증 | 작은 검사 언어의 정리; 실제 MIR/layout producer OPEN |
| 카운터 → 선택: `CycleReclaim` | `CountsExact`, `AllEdgesIssued`, `op_admitted`; identity별 갱신; 다음 선택 소비 | 세대 다른 슬롯으로 decrement 금지. overflow 전에 거절/더 강한 표현을 계약으로 정함; 포화 under-count 금지 | 자연수 모델; 유한 카운터·overflow 정책/정제 OPEN |
| graph fragment → 전체 힙: 28, `gexec_framed` | 다른 owner의 유효 footprint frame과 서로 겹치지 않는 새 블록; 할당/공개 | frame 없음이나 다른 owner 블록 재할당 거절. 실패한 목적지 정리는 원본 보존 | frame 경계 모델 존재; 실제 allocator/payload glue OPEN |
| batch 판정 → 물리적 해체: `TeardownAtomicBatch`, counted batch | 동일 원래 상태의 모든 요청, 중복 대상/겹침 없는 정확한 단위, 안정된 권한/lease/heap generation; commit/publication | 뒤 요청 거절·할당 실패·preflight 후 간섭에도 부분 free가 공개되지 않아야 함 | 함수형 값 원자성만 증명; native non-refusing commit OPEN |
| identity 발급 → 재사용: `GraphLinks`, tree authority | store id, slot generation, root epoch, lease id의 비재발급 수명; resolve/retire | wrap/reuse로 과거 핸들 부활 금지. exhaustion은 명시적 거절/영구 은퇴 | graph slot gmax 검사와 store id 단조 nat 발급 존재; 유한 store/root/lease ID 정제 OPEN |
| 선택·색인 → 작업량: 28 / 기존 해체·회수 모델 | 방문·edge·membership·검증·할당·commit 전체 비용; 동일 고정 입력의 관측 | `fuel=1`인데 큰 C/store/edge로 비용 증가, 다른 store만 키워 국소성 반증 | `fuel`은 closure round 수; 총 상한·지연·물리적 국소성 OPEN |
| 단계 → worker/async/FFI: 기존 동시성·ABI owner | 소유자 이전/복사 또는 증명된 pinned read-only view, suspend/cancel 규칙; 마지막 외부 사용 | growable container raw pointer 공유·취소 뒤 사용·FFI 재진입 중 free 금지 | 현재 action 모델은 순차; 내부 간섭/중단 정제 OPEN |

`checked_ledger_reclaim`를 부른다는 사실만으로 이 모든 행을 통과하지 않는다.
그 정리는 특히 `CountsExact`, `AllEdgesIssued`, `SimInv`를 요구한다. graph와 tree의
논리 번호가 우연히 같은 것도 동일한 권한·물리 저장소의 증거가 아니다.

## 4. 물리적 원자 해체의 순서

```text
현재 owner 사실 확보
    ↓
전체 후보/단위/권한/lease/pin/epoch 검사
    ↓
필요한 임시 공간과 failure 지점 해결 ── 거절 → 공개 상태 불변
    ↓
검사한 generation의 안정성 확보         ← 여기부터 다시 거절하면 안 됨
    ↓
들어오는 링크 정리 → 정확한 단위 해제 → forest/graph/권한 은퇴 동기화
    ↓
재사용 가능한 identity/저장소 공개
```

이는 필요한 구현 의무이지, lock·트랜잭션 엔진·새 arena 채택 지시가 아니다.
순차 실행으로 안정성이 성립하면 그 전제를 증명하고, 재진입/동시성이 있으면 기존
권한 경계에서 안정성을 보장한다. 사전 검사 뒤 재할당·콜백·추가 권한 검사로 새 실패
가능성을 만들면 non-refusing commit 의무를 다시 충족해야 한다. 중간 free를
되돌릴 수 없으므로 함수형 `Refused(original)` 구현을 그대로 번역해서는 안 된다.

## 5. 국소성·예산·DX의 수용 기준

`internal(C, i)`는 현재 store 슬롯 목록의 spine 전체를 접고, 리스트 lookup과
membership도 비용이 든다. `close(fuel, ...)`의 round 수만으로 시간·메모리·방문 수를
제한했다고 말하지 않는다. 안전한 선택 실패와 성능 예산 초과는 서로 다른 관측이다.

production 정책을 고르기 전에 고정 semantic input에서 다음을 함께 기록한다.

- 후보 크기, 전체 inventory와 살아 있는 노드/edge 수, 외부 store 크기.
- 슬롯·edge 방문, membership/lookup, closure 반복, 단위 검증, 임시 할당,
  링크 정리와 실제 해제 비용. 준비 단계와 commit을 모두 포함한다.
- 유지 메모리, 최대 임시 메모리, refusal/deferred, 실제 회수량과 결과 동등성.
- C/LLVM별 결과와 비용. 두 backend가 같은 오답일 수 있으므로 모델/관측 oracle도 둔다.

여기서 일반 벤치마크 트랙을 새로 열지 않는다. active rung의 다음 폐쇄를 막는
실측 병목일 때만 기존 owner 뒤의 그 연산을 개선한다.

기본 사용자는 lifetime·수동 root 목록·field carrier·deep-drop·추론 보완용 own/ref를
추가하지 않는다. 새 resource/aggregate를 추가할 때 semantic owner 한 곳에서
소유/방문/해체 계약을 선언하고 소비자가 그 사실을 공유하도록 연결한다. 아직 자동
발급자가 없는 상태를 “모든 새 타입에 자동 전파된다”고 표현하지 않는다. 고정된
source/AST 경계에서 추가 표기·복사·직접 연결 지점을 측정하며, 새 타입 추가가 여러
backend의 별도 판단표 수정으로 번지면 중복 authority가 남아 있는 것이다.

처음 만난 링크의 acquire-on-first-touch는 별도 경계다. 처음 실패했을 때 앞선
효과가 남는다는 계약부터 필요하다. 지금의 시작 시 일괄 획득 증명을 그대로
늘이거나 사용자에게 수명 주석을 요구해 해결했다고 하지 않는다.

## 6. 착수·폐쇄 판정과 반례

구현에 착수할 때는 active rung이 실제로 도달한 행만 골라 생산자·모든 소비자·
마지막 소비자와 지울 우회 경로를 source로 확인한다. 위 표 전체를 새로운 P1
선행 조건으로 바꾸지 않는다. 임의로 경계를 넘어 구현하지 않는다.

현재 실행 가능한 검사는 다음과 같다.

| 검사 | 무엇을 확인하는가 | 확인하지 않는 것 |
|---|---|---|
| `tests/graph_action_scope_smoke.sh` + 독립 consumer | no-deref/hold 반환, 실제 성공 prefix, 원래 실패와 보상 실패 보존, 부분 효과·no-op 보상 반례 | 도메인 복구, 실제 compiler/runtime |
| `tests/graph_action_scope_selftest.sh` | proof/receipt 약화, 실제 실행 prefix 삭제, assumption/source drift 등의 planted mutation 거절 | 모든 결함 종류의 완전 탐지 |
| `tests/graph_cycle_reclaim_smoke.sh` + 독립 consumer | checked batch, canonical 권한 투영, checked root/count 전제와 반례 | 물리 commit, binding issuer, 전체 비용 |
| 전체 Rocq kernel + formal inventory | 현재 snapshot의 등록·타입·승인된 가정 | native 테스트나 exact-SHA CI |

후속 구현 반증에는 반드시 late-borrowed batch, root/후손 overlap, 오래된 epoch,
counter exhaustion, 외부 footprint overlap, 누락 caller/result/view root,
부분 효과 후 실패·부분 보상 후 실패, preflight 뒤 간섭/할당 실패가 포함돼야 한다.
실제 지원하지 않는 경계는 앞에서 명시적으로 거절한다.

**CLOSED 조건:** admitted owner의 사실이 마지막 소비자까지 쓰이고, 누락 사실은
거절되며, 기존 우회/재추론 경로가 삭제되고, 관련 native C/LLVM 음성·통합·설치본
및 정확한 커밋의 CI 증거가 해당 rung 계약을 만족해야 한다. 이 문서나 모델 게이트만
있다는 이유로 SoT, 전체 self-host, 생산 환경의 메모리 안전을 닫지 않는다.

이번 모델 보완의 입력 해시와 실제 검사 결과는
[2026-10-09 검증 기록](../audits/ownership_lifecycle_review_hardening_2026-10-09.md)에 있다.
