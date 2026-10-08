# 증명 모델 레드팀 통합 수정 영수증 — 2026-10-08

상태: **요청된 감사 항목 처리·독립 로컬 통합 검증 완료**.
전체 언어 메모리 안전성, 물리 할당기, SoT/self-host 또는 원격 CI CLOSED가 아니다.
이 문서는 영수증이며 의미 소유자는 기존 모델·런타임·MIR 계약이다.

기준 HEAD: `3658548d24bca3d721e4f1974ac7a10da99f7aa8`, 보존된 dirty `main`.
진입 시 173개 status entry와 빈 index였다. 최종 검증 snapshot은 300개 entry,
빈 index였다. 이 숫자는 공유 트리 상태이며
이 작업만의 변경 수가 아니다. stage/commit/push/install은 하지 않았다.

- 원 감사: [proof_model_redteam_2026-10-08.md](proof_model_redteam_2026-10-08.md).
- 전체 사슬·분담·도달 소비자 계획:
  [공유 지시문](../agent_work_directives/proof_redteam_reuse_remediation_2026-10-08.md).
- 식별자 레인의 집중 검증 기록:
  [재사용 영수증](proof_reuse_identity_remediation_2026-10-08.md).

## 결과와 집계

35건 모두 처분했다. **수정 24건, 주장 교정 9건, 기존 수정 재검증 2건**이다.
수정에는 실제 제품 경로 2건(C1, A-F12)과 제한된 모델의 규칙/판정 수정이
함께 들어 있다. 주장 교정은 과장된 정리를 구현한 것이 아니라, 그 주장을
철회하거나 실제 성립하는 조건으로 좁힌 것이다.

High 7건 중 M1은 기존 수정을 재검증했고, C1/A-F1/B-F1/B-F2/B-F3은
해당 경로·모델을 수정했다. A-F4의 무조건적 진행 주장은 철회했다.
따라서 "35개의 제품 버그를 닫았다" 또는 "63개 증명이 전부 제품을 증명한다"
라고 읽으면 안 된다.

| ID | 처분 | 실제 변경 / 반례 경계 |
| --- | --- | --- |
| A-F1 | 수정 | cancel은 요청일 뿐이다. 모든 non-Done task가 close를 막으며 실제 완료 후에만 닫힌다. |
| A-F2 | 수정 | scope close가 부모 간선을 지운다. 재open으로 옛 자손이 새 scope에 붙지 않는다. |
| A-F3 | 수정 | 병렬 merge가 before/left/right를 받아 같은 Live handle의 양쪽 신규 소비를 거부한다. 이미 Retired였던 값과 구분한다. |
| A-F4 | 주장 교정 | 실제 empty-queue 순환 park 도달 반례를 보존한다. 보상은 모든 순환 대기를 풀지 않는다. queued-work 구조의 제한된 진행만 증명한다. |
| A-F5 | 수정 | 전체 worker 수는 base + 4×base 이하이다. 실행 사슬에서도 상한을 보존한다. |
| A-F6 | 수정 | park 대상은 발급된 task여야 한다. 없는 대상을 기다리는 전이는 거부한다. |
| A-F7 | 수정 | release가 context와 slot을 함께 받아 외부 context의 reader/capability를 유지한다. |
| A-F8 | 주장 교정 | pin xor는 인터페이스 제약임을 명시하고 존재하지 않는 step_move의 문서 인용을 삭제했다. |
| A-F9 | 주장 교정 | 항등 capture/resume는 계약 고정이지 실제 task-context 복원 구현의 증거가 아니다. |
| A-F10 | 수정 | Coordination뿐 아니라 WholeProgram.ready → AIR → Binary.accept에서도 완료 step 재실행을 거부한다. run의 done 단조성/NoDup을 검증한다. |
| A-F11 | 주장 교정 | snapshot compensation 모델은 사용자 compensate 식의 실제 역순 실행을 증명하지 않는다. |
| A-F12 | 수정(제품) | Now는 CLOCK_MONOTONIC / GetTickCount64 기반 Long ms이다. 실패·범위 초과·잘못된 단위는 명시적으로 panic한다. |
| B-F1 | 수정 | Core/Unified rollback은 현재 lifecycle store를 유지한다. Released 부활 및 Filled → Empty 회귀를 막는다. 사용자 보상 효과의 refinement는 별도다. |
| B-F2 | 수정 | WF=dep_closed라는 실제 정의를 명시한다. 별도의 step/run 정리로 Released 보존을 증명하며 WF에 없는 계약을 주장하지 않는다. |
| B-F3 | 수정 | addressed read/write는 양수 region extent와 실제 contact의 grant 포함을 요구한다. 바깥 메모리는 보존하며 Fence는 contact가 없다. |
| B-F4 | 수정 | share는 outright mask만 복사한다. borrowed capability 복제와 손주 loan이 남은 반환을 거부한다. |
| B-F5 | 수정 | admitted registry link의 requester self/direct-import exports만 resolve한다. 전역 inventory를 공개 resolution fallback으로 쓰지 않는다. |
| B-F6 | 수정 | 실제 SDelegate 전이로 권한을 만든다. 임의 initial holding과 with_target_deleg 우회 증거를 제거했다. |
| B-F7 | 수정 | scope depth가 아니라 실제 부모 ancestry로 포함 관계를 판정한다. 형제 scope는 거부한다. |
| B-F8 | 수정 | receipt 생성이 verdict를 읽는다. 생성 시점과 재판정 가능 조건을 구분한다. |
| B-F9 | 주장 교정 | ResourceMachineBridge/DelegationBoundary/Axis의 항등·상수 정리는 경계 인터페이스 고정으로만 인용한다. |
| B-F10 | 수정 | actual bounded ancestry가 visible_ids와 visible의 공동 근거다. hardcoded ID 집합을 제거했다. |
| C1 | 수정(제품) | 공개 int32 handle은 다시 발급하지 않는다. INT32_MAX 후 exhaustion을 latch하고, ancestry는 live 조회 후 parent 순서를 검사한다. registry 저장소 재사용은 허용한다. |
| C2 | 주장 교정 | no_dep_cycle은 한 intent 내부 dependency DAG에 관한 정리이며 runtime parent reuse/cycle을 배제하지 않는다. |
| C3 | 수정 | OpDiv의 divide-by-zero와 INT_MIN/-1 overflow를 모두 연결한다. panic class가 operation을 유일하게 식별한다는 잘못된 disjoint 주장을 제거했다. |
| C4 | 수정 | 실제 DIR←HIR, MIR←DIR 간선을 넣었다. 고정 그래프는 3층을 요구하며 RIR 하나를 늦춰도 DIR 사슬은 남는다. |
| C5 | 주장 교정 | 예전 authority/cap 반례 쌍의 grant 불일치를 드러냈다. grant-consistent 구성에서는 authority가 cap projection으로 계산된다. 문서의 축 판정 승격을 철회했다. |
| C6 | 수정 | canonical AxisOwnership과 producer별 실제 값을 읽는 admission으로 순서 무관성을 정리했다. 값이 다른 duplicate는 거부한다. |
| C7 | 주장 교정·판정 강화 | HKT/AIR/intent 정리의 범위를 명시한다. intent는 실제 6개 obligation family의 유무를 검사하고 compensation 누락을 거부한다. taxonomy는 비표현가능성 증명이 아니다. |
| M1 | 기존 수정 재검증 | SlotCalculus의 Pin capability 없는 unpin 거부와 여러 정리 단계의 pin 보호를 전체 fresh kernel에서 확인했다. |
| M2 | 수정 | SlotLifecycle의 (slot,generation) 식별과 reclaim을 도입했다. 임의 길이 실행에서도 retired identity는 영구 stale이다. |
| M3 | 수정 | 자기 move는 원본을 유지한다. missing/occupied/repeated retire를 구분하며 arbitrary transfer/drop ledger의 unique retirement를 증명한다. |
| M4 | 수정 | 복사된 StringValue와 raw 주소 설계를 write/free/reuse 전이로 비교한다. 재사용된 raw 주소는 새 데이터를 읽고 boundary copy는 원래 값을 유지한다. |
| M5 | 주장 교정 | GC/ownership 비용을 독립 정책 항으로 계산한다. 공유 GC가 이기는 사례와 ownership이 이기는 사례를 모두 둔다. 보편적 속도 우위는 증명하지 않는다. |
| 그래프 외부 footprint | 기존 수정 재검증 | gexec_framed와 MemoryBoundaryCompositionAudit의 외부 소유자 침범 거부를 전체 fresh kernel에 포함했다. 실제 issuer/주소 배치 알고리즘은 아직 없다. |

## 실제 제품 사슬

### Intent 식별자

공통 owner `src/runtime/pgy_runtime_intent_identity.h` → inline/linked enter →
잠긴 active registry → ancestry/conflict 면제 → exit/trace가 같은 발급 규칙을
쓴다. 공개 ABI는 handle 값만 받으므로 내부 generation만 더하는 것으로는
stale public caller를 구별할 수 없다. ABI를 바꾸지 않고 nonrecycling을 택했다.
물리 registry 칸은 재사용하되 권한 식별자는 재사용하지 않는다.
Exhaustion의 실제 발생률·비용은 측정하지 않았다.

### Clock 타입과 호출 ABI

공통 owner `src/runtime/pgy_runtime_host_clock.h` → inline/linked Now와 real ns
clock → native builtin/LLVM declaration/self-host signature·C ABI projection →
datetime.TimeSpan/Instant → timer/obligation/device timestamp를 Long으로 맞췄다.
calendar/device value/address와 Sleep은 Int 그대로다. 가상 시계는 기존 명시적
advance 계약을 유지한다. Now는 Unix timestamp가 아니다.

전체 semantic census에서 5개 실패를 모두 열거한 뒤 수정했다. 이어 같은
stdlib 입력의 LLVM leg가 허용된 Int → Long 인자를 i32로 남기는 문제를
드러냈다. 기존 MIR parameter signature와 numeric store coercion을 연결해
boundary/member/hosted/ordered intent value call을 고쳤다. callable/closure는
이미 같은 coercion owner를 썼다. 사용자에게 L suffix나 표기를 추가시키는
우회는 쓰지 않았다. Slot/inout/participant 주소 전달과 raw runtime ABI는
그대로 유지했다.

### Strict C11 런타임 소비자

새 clock owner를 포함하는 TU는 이미 정해진 POSIX/thread feature profile을
받아야 한다. census에서 Make의 standalone recipe 3개와 harness 9개를
확인했다. Make는 기존 PLATFORM_CFLAGS를 전달하고, linked-runtime harness
7개는 기존 linked_runtime_compile_profile_owner를 소비한다. lane scheduler의
별도 TU와 source-test fan-out도 thread/profile 경계를 연결했다. clock ID를
숫자로 추측하거나 CLOCK_REALTIME으로 돌아가는 fallback은 없다.

공통 linked runtime TU는 한 번 컴파일하고 그 결과를 7개 profile 소비자에
귀속했다. 이는 각 self-host 행의 installed-driver matrix 실행이나 대체
진척이 아니다. 지원하지 않는 플랫폼과 실행 불가능한 C compiler는 거부한다.
source-test harness의 compiler failure를 skip-success로 숨기던 경로도 제거했다.

## Main이 독립 관찰한 검증

고정 source snapshot, Rocq **9.3.0 / Stdlib 9.2.0**:

- `tests/formal_semantics_smoke.sh`: 63 owner + 영구 regression consumer 6개,
  총 69개와 별도 assumption approval consumer를 fresh compile / rocqchk PASS.
  69개 SHA256를 현재 파일과 다시 비교해 모두 일치했다.
- 전체 assumption budget은 기존 `SlotCalculus.MaxSlotId`, `verify_token`의
  승인된 타입 2개다. 추가 axiom/admit/unsafe kernel feature는 없다.
  기존 masking/deprecated 및 indexed-inductive dependency 보고는 숨기지 않았다.
- `tests/coq_kernel_check_selftest.sh`: planted admit, 같은 이름의 승인 API
  타입 변경 3종, 빈/missing approval 거부 PASS.
- main의 arithmetic/IR/emission/config regression: fresh 7개 module, axiom 0 PASS.
- GC 비교: fresh 3개 module, axiom 0 PASS. 이는 추상 정책 비용이지 실측이 아니다.
- IR/evidence-lifecycle/proof-spine 정합성 gate PASS.
- certificate adequacy: fresh kernel-checked checker와 실제 extraction의
  **262144개 finite valuation + 4개 length control + 41개 envelope/owner control**
  일치 PASS. 이 결과를 다른 모델의 production refinement로 확대하지 않는다.

실제 source-bound runtime과 새 native compiler:

- intent inline/linked × trace 0/1의 4조합 PASS: 실제 nesting, non-LIFO leave,
  다른 thread 충돌, 같은 물리 registry 칸 재사용, stale exit/trace, 지속되는
  exhaustion을 검증한다. runtime observability contract도 PASS.
- host clock inline/linked의 mock/real PASS: 32bit 초과 Long 값, 단조 clock 선택,
  system failure/negative time/invalid ns/overflow/capability denial/invalid unit을
  명시적 panic class로 검증한다. linked ns-overflow·real advance 거부와 명시적
  virtual advance도 PASS. mock gate는 POSIX 대상이다.
- Windows gcc로 real-clock inline/linked positive probe 2개 PASS. Windows
  전체 compiler/backend matrix나 syscall failure injection은 실행하지 않았다.
- 격리된 native Linux/LLVM18 compiler build PASS; 설치 bin/driver는 바꾸지 않았다.
- `tests/runtime_now_compiler_smoke.sh`: 새 compiler의 C/LLVM 실행 및 i64 Now
  declaration PASS.
- `tests/call_scalar_widening_smoke.sh`: signed Int → Long, Float → Double,
  direct/member/subject/hosted/callable/intent C/LLVM 실행 PASS. Long → Int와
  String → Long은 structured semantic error로 거부하고 executable을 만들지 않는다.
- 변경하지 않은 stdlib surface 입력: C/LLVM 양쪽 PASS.
- 전체 native semantic **3107 passed / 0 failed**, transpile **1006 / 0**.
- native MIR **217 / 0** 및 그 Make target의 topology/destructure/match/render/
  speculation gate PASS. render fixture의 기존 unreachable warning 15개는 남아 있다.
- standalone slot-scope/capability/budget와 lane-scheduler 실제 실행 PASS.
- runtime C profile 공통 TU/7개 소비자 ratchet, 미지원 플랫폼 및 unusable compiler
  거부 PASS. root source-test harness **18개 TU compile PASS**이며 test 실행 결과가
  아니다. 기존 parser clipping fixture의 snprintf truncation warning은 남아 있다.
- runtime bitcode source-freshness 계약, 문서 품질, skill validator PASS.

CI에는 production fresh kernel의 6개 regression consumer, source-bound intent/
clock/profile probe와 native clock/call ABI gate를 연결했다.
**원격 실행은 관찰하지 않았다.**

전체 `self_hosted_component_contract_smoke.sh`는 14개 checker subtest PASS 후
`rc=1`이다. 기존 handoff에도 열린 같은 structural gate가 기록돼 있다.
현재 전체 self-host structural/installed-driver/CI가 green이라는 증거로
사용하지 않는다. 이 감사 작업은 그 별도 실행 사다리를 재개하지 않았다.

## 재실행과 checkpoint

저장 로그: `.tmp/proof-redteam-repair-2026-10-08/`의 `formal.log`,
`kernel-selftest.log`, `main-focused-final.log`, `gc-final.log`, `intent.log`,
`host-clock.log`, `build-call-widening.log`, `now-compiler-final.log`,
`call-widening.log`, `stdlib-final.log`, `unit-with-call.log`, `mir-final.log`,
`capability-final.log`, `runtime-profile-final.log`, `lane-final.log`,
`source-harness-final.log`, `certificate-final.log`, `spine-final.log`,
`component-final.log`.
scratch log와 원 감사 probe는 의미 owner가 아니다.

Linux/WSL에서 다음처럼 실행한다. PGY_BIN은 **검증할 실제 binary**를 지정한다.

```bash
env OPAMROOT=/home/c/.local/share/pergyra-rocq/opam \
  bash scripts/run_rocq_toolchain.sh bash tests/formal_semantics_smoke.sh
bash tests/runtime_intent_identity_reuse_smoke.sh
bash tests/runtime_host_clock_smoke.sh
bash tests/runtime_c_profile_smoke.sh
env OPAMROOT=/home/c/.local/share/pergyra-rocq/opam \
  bash scripts/run_rocq_toolchain.sh bash tests/proof_carrying_adequacy_smoke.sh
PGY_BIN="$PWD/.tmp/proof-redteam-repair-2026-10-08/bin/pgy" \
  bash tests/runtime_now_compiler_smoke.sh
PGY_BIN="$PWD/.tmp/proof-redteam-repair-2026-10-08/bin/pgy" \
  bash tests/call_scalar_widening_smoke.sh
PGY_BIN="$PWD/.tmp/proof-redteam-repair-2026-10-08/bin/pgy" \
  bash tests/stdlib_surface_smoke.sh
```

| 검증 대상 | SHA256 |
| --- | --- |
| 새 private native compiler | `bccaacdf605518d184eaf23594a18bf8b5e7a50b4f606b6b48a48f602f531224` |
| 공통 intent identity owner | `97320a40cd257a2374757971e0c6013a084f400f3dd87349e4c5d59f546c7303` |
| 공통 host clock owner | `00a9c9ea79491d368f00b57e92ce59401d3b030261d49d1c9b470f8685bb12b0` |

## 열린 경계와 다음 falsifier

- 모델 scope generation/clone storage/admitted authority registry/bounded ancestry는
  production issuer 및 실제 제한 정수/메모리 모델과의 전면 simulation이 아니다.
  completed task ID는 재발급하지 않는다. 실제 task ID recycling refinement는 OPEN.
- 사용자 compensate 효과/값 복구, scope/task 실제 drain, foreign callee의 arbitrary
  동작, fairness 및 순환 대기 회복은 각 실제 owner의 refinement가 필요하다.
- [메모리 실행 폐쇄 조사](memory_boundary_executable_closure_survey_2026-10-08.md)에
  남은 Slot↔root↔graph issuer, 완전한 외부 root inventory, 실제 주소 배치,
  copy-before-free, 실패 원자성과 단일 물리 retirement를 구현하지 않았다.
  조건부 frame 합성만으로 요청된 **Rocq 실행 알고리즘 CLOSED**를 선언하지 않는다.
- GC 우위는 독립 정책·가중치·동일 의미 workload의 실측이 필요하다. 그래프 저장소
  owner-end 정리는 live owner 내부의 unreachable cycle 즉시 회수가 아니다.
- local `pergyra-authoring` skill에 이 범위 구분, whole-chain 계획, 자동 정리 목표,
  저장소 재사용/권한 식별 분리, async/공유 그래프/Alrescha 경계를 반영했다.
  skill은 Git-ignored이며 local validator PASS다.
- self-host ownership/DX IMPLEMENTATION HOLD와 기존 rung은 유지한다. 자동 cleanup
  compiler 구현, 설치 교체, CI/SoT CLOSED, commit/push 및 GUI readiness 전달은 하지 않았다.

이번 공유 WSL에서 이름 기반 `pkill`/`killall`은 사용하지 않았다. 각 검증은
격리된 scratch와 유한 timeout으로 실행했고 다른 레인의 프로세스를 종료하지 않았다.
