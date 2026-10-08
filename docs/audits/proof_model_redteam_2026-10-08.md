# 증명 모델 레드팀 (2026-10-08)

상태: **READ-ONLY 감사 결과**. 이 문서는 의미 소유자가 아니며 SoT 상태,
레지스트리, 구현 사다리를 바꾸지 않는다. 결함 수정은 각 파일의 소유
레인이 정한다. 예외로 SlotCalculus의 unpin 결함은 같은 날 사용자 요청
작업 범위 안에서 고쳤다(아래 M1).

후속 작업: 사용자 요청으로 아래 35건을 처리한
[통합 수정 영수증](proof_model_redteam_remediation_2026-10-08.md)이 있다.
이 문서의 판정·소스 인용·probe는 감사 당시의 기록으로 유지한다.
현재 수정 상태와 검증 범위는 후속 영수증을 확인한다.

## 범위와 방법

- 대상: `docs/semantics/proofs/*.v` 63개(약 2만 3천 줄)와 `tests/coq` 감사
  파일. 메모리/수명 계열은 Claude가 직접 봤고, 나머지 세 묶음은 읽기 전용
  에이전트가 봤다. 묶음은 async/병렬, 식별자/권한/머신, intent/IR/산술이다.
- 기준: 같은 날 실제로 나온 결함 유형.
  1. 재사용이 구조적으로 불가능한 모델. 예: SlotCalculus가 재claim을 늘
     세대 1로 했다.
  2. 정의상 참이거나 공허한 정리.
  3. 모델과 런타임 또는 문서의 불일치.
  4. 실제 프로그램에 적용되지 않을 만큼 강한 전제.
  5. 추상 파라미터나 가정이 주장을 대신하는 경우.
  6. 정리보다 강한 문서 주장.
  7. 한 단계 불변식을 여러 단계 보장처럼 쓰는 경우.
- 증거: 모든 항목에 반례 probe가 있다. probe 29개를 Claude가 다시
  컴파일해 모두 `rc=0`을 확인했다. Admitted나 Axiom은 없다. 런타임 인용은
  해당 소스를 직접 열어 확인했다.
- probe 위치: `.tmp/proof-redteam-2026-10-08/{me,a,b,c}`. 재실행 명령:

  ```bash
  wsl -d Ubuntu-E-WSL --exec env OPAMROOT=/home/c/.local/share/pergyra-rocq/opam bash scripts/run_rocq_toolchain.sh bash .tmp/proof-redteam-2026-10-08/probe_dir.sh /mnt/d/PergyraLang/.tmp/proof-redteam-2026-10-08/a
  ```

심각도:

- **High**: 정리가 실제 안전성이나 정확성을 오해하게 만든다.
- **Medium**: 런타임이나 문서와 어긋난다.
- **Low**: 정의상 참인 증거다.

## 결과 요약

| 심각도 | 건수 | 비고 |
|---|---|---|
| High | 7 | 1건 수정(M1) |
| Medium | 17 | 다른 레인이 고치는 중인 1건 포함 |
| Low | 11 | |

## 재사용 계열 (사용자가 지정한 핵심 기준)

| ID | 파일 | 결함 | 심각도 |
|---|---|---|---|
| M1 | SlotCalculus.v | Unpin에 가드가 없었다. capability가 하나도 없는 문맥이 pin된 슬롯을 unpin한 뒤 release해서, 두 단계 만에 쫓아냈다. 문서 08과 런타임(`slot_manager_pin.c`)은 pin view나 capability를 요구한다. | High, **수정됨** |
| C1 | IntentConflict.v:62,68 | 같은 `anc`를 정적 분리 증거와 런타임 면제에 함께 쓴다. 런타임은 부모 handle **값**을 따라가는데, handle은 INT32_MAX에서 되감겨 재사용된다(`pgy_runtime_lib_set_intent_trace_exports.c:225-251,340-366`). 비LIFO leave가 겹치면 다른 스레드의 무관한 intent에 맞아 진짜 충돌을 면제한다(fail-open). **런타임 버그 후보.** | High |
| A-F2 | AsyncScopeCore.v:134 | 닫힌 scope id를 다시 열어도 옛 부모 간선이 남는다. 그래서 다른 scope의 task가 취소되고 닫기가 막힌다. | Medium |
| C2 | IntentSpine.v:29-31,252 | `no_dep_cycle`가 F1 livelock(handle 재사용으로 생긴 부모 사슬 순환)을 정적으로 배제한다고 주장한다. 실제로는 intent 안의 의존 관계만 제약한다. | Medium |
| M2 | SlotLifecycleCore.v | `Released`가 종착 상태라 재사용이 불가능하다. 재사용을 넣으면 옛 보유자의 사용과 새 보유자의 사용을 구분할 수 없다. 정리들은 규칙을 뒤집은 것(inversion)뿐이다. 문서 19와 README는 affine safety를 주장한다. | Medium |
| B-F1 | PergyraCore.v:147, UnifiedCore.v:31 | Rollback이 Released 슬롯을 스냅샷 상태로 되살려, 해제 후 사용과 이중 해제를 도출할 수 있다. 반대로 Filled를 Empty로 되돌려 두 번 acquire할 수도 있다. | High |
| (다른 레인) | OwnershipGraphLinks.v | 합성 단계에서 그래프 할당이 그래프 힙만 검사하고, 다른 소유자의 footprint(Ho)는 검사하지 않는다. 다른 레인이 `gexec_framed`로 고치는 중이다(18:03 기준, 아직 컴파일 안 됨). | Medium |

## 그 밖의 High

| ID | 파일 | 결함 |
|---|---|---|
| A-F1 | AsyncScopeCore.v:116-149 | 취소된 task를 이미 멈춘 것으로 본다. 그래서 `cancel; close`가 기다리지 않고 닫는다. 런타임 `AsyncScopeDestroy`는 `AsyncScopeWaitAll`로 기다리고, 형제 모델 AsyncLifecycleCore도 반대로 말한다. |
| A-F4 | ParallelSchedulingCore.v:192,728 | scorecard는 순환 대기에서 보상(compensation)이 진행을 준다고 주장한다. 실제로는 큐가 비고 두 worker가 park한 상태에 도달할 수 있고, 거기서 멈춘다. 근거 정리는 손으로 고른 한 단계다. |
| B-F2 | WholeProgramCore.v:240 | 헤더는 WF가 "해제된 슬롯은 기록된 rollback으로만 되살아난다"를 포함한다고 말한다. 실제 WF는 `dep_closed`뿐이고 store를 보지 않는다. capability 절도 없다. |
| B-F3 | MachineLayerCore.v:233,498,547 | 크기 0인 region이 grant 끝 바로 다음 칸에 허용된다. 그 region으로 쓰면 다른 grant의 첫 칸을 쓴다. 문서는 이 정리를 진짜 no-ambient-contact로 부른다. |

## Medium

| ID | 파일 | 결함 |
|---|---|---|
| A-F3 | AsyncLifecycleCore.v:181 | 두 병렬 가지가 같은 handle을 await하면 병합 결과가 Retired다. 체커는 이 프로그램을 거부하지만 모델에는 그 규칙이 없다. |
| A-F5 | ParallelSchedulingCore.v:192 | spare worker 수에 상한이 없다. 런타임은 worker 수 × 4로 막는다(`pgy_parallel_pool_lifecycle.h`). |
| A-F6 | ParallelSchedulingCore.v:177,355 | Park가 기다리는 task가 spawn됐는지 확인하지 않는다. 그래서 파일 자신의 인스턴스에서도 두 단계 만에 멈춘 구성에 도달한다. |
| A-F7 | WitnessDataRace.v:100,217 | release가 문맥을 받지 않아, 한 번에 모든 문맥의 capability를 지운다. |
| A-F10 | CoordinationCore.v:46 | 끝난 step을 다시 실행할 수 있다. "결정성 불변식"이라는 문구를 뒷받침하는 증명이 없다. |
| A-F12 | ClockDomains.v:6,21 | 단조 `Long`의 `Now()`가 결정 사항으로 적혀 있다. 그러나 실제 `pgy_now_ms()`는 Linux에서 CLOCK_REALTIME을 `int32_t`로 좁혀서, 시간이 거꾸로 갈 수 있다(`pgy_runtime_lib_core_exports.h:34-44`). |
| B-F4 | CapabilityFlowCore.v:89 | 빌린 capability를 share로 복사할 수 있다. 그래서 대여 중 두 task가 같은 capability를 갖고, 반환 뒤에도 손주 task가 계속 갖는다. |
| B-F5 | ModuleAuthority.v:143,340 | `resolve`가 요청 모듈을 받지 않는다. `m_owns`는 자기 선언이라, 두 모듈이 한 authority를 소유할 수 있다. |
| B-F6 | UnifiedCore.v:258 | 정리의 시작 상태가 step이 아닌 생성으로 만든, 아무도 갖지 않은 capability를 쓴다. |
| C3 | GuardWitnessBinding.v:105,141 | OpDiv를 divide-by-zero에만 묶는다. 런타임은 INT_MIN/-1에서 arithmetic-overflow로 panic한다(`pgy_runtime_lib_checked_arith_core.h:10-30`). 실제 표에서는 1:1 분리가 성립하지 않는다. |
| C4 | IRMinimality.v:44,91,119 | 드라이버에 DIR←HIR(`driver_app.c:352`)와 MIR←DIR(`:446`) 간선이 있는데 모델에는 없다. 그래서 "단일 pivot" 주장이 틀린다. |
| C5 | AuthorityIrreducibility.v:46,87 | 반례 쌍이 "capability는 grant를 따른다"를 어긴다. 일관된 구성에서는 cap 투영으로 authority를 계산할 수 있다. 그런데도 문서 22는 이 정리를 근거로 축 판정을 올렸다. |
| M5 | OwnershipCleanGCComparison.v | GC 비용을 "소유권 비용 + sweep"으로 정의했으므로, 우위는 정의 그 자체다. 값 의미론이 강제하는 복사(공유 GC는 하지 않는 복사)도 GC 쪽에 똑같이 청구한다. |

## Low (정의상 참인 증거 등)

| ID | 파일 | 결함 |
|---|---|---|
| A-F8 | WitnessDataRace.v:180 | `pin_exclusive`는 `xor_mut`의 재배열이다. 문서 10 §7이 인용하는 `step_move` 규칙은 존재하지 않는다. |
| A-F9 | AsyncContextCore.v | resume과 capture가 정의상 항등이다. |
| A-F11 | CompensationCore.v:93,112 | rollback 정리가 정의 그대로다. codegen은 사용자 compensate 식을 역순으로 실행하는데, 이것은 스냅샷과 대조되지 않는다. |
| B-F7 | PartySlotBinding.v:117 | 깊이 비교로 scope 포함 관계를 대신하므로, 형제 scope가 통과한다. |
| B-F8 | EvidenceLifecycleCore.v | 영수증 생성이 판정 필드를 읽지 않는다. |
| B-F9 | ResourceMachineBridge, DelegationBoundaryCore, AxisOwnership | "결정하지 않는다" 정리들이 상수 부등식이나 항등 함수로 끝난다. |
| B-F10 | BindingIdentityScope.v:188 | `visible_ids`가 자기 `visible`과 다르게 하드코딩되어 있다. |
| C6 | ReadingConfluence.v:190 | 순서 무관성이 소유 관계가 유일하지 않아도 성립한다. 그래서 이중 소유를 검출하지 못한다. |
| C7 | IRMinimality:187, IntentObligations:63, AIRBinding:75 | 최소성, 상속, 필요충분 주장이 전부 정의상 참이다. |
| M3 | CollectionOwnershipTransfer.v | 소유자·gate 정리는 어떤 함수에서도 참이다. 자기 move가 소유를 잃는다. 해제 1회 정리는 서로 다른 두 바인딩에만 증명되어 있다. |
| M4 | ForeignStringOwnership.v | `copy_survives_foreign_change`는 전제를 다시 쓴 것이다. 고쳐야 할 설계(문자열 = 포인터)는 모델에 없다. 재사용하면 그 설계는 새 데이터를 읽는다. |

## 결함 없음으로 확인

- 메모리 계열의 Core, Composition, ReadOnly, Exits는 재검토에서 새 결함이
  없었다. 그래프 모델의 저장소 id 무한 카운터는 반례
  `store_id_reuse_resurrects`와 설계 요구로 이미 기록되어 있다.
- 에이전트들이 결함 없음으로 판정한 파일:
  - SuspensionRevalidationCore, ParallelReductionCore, DeterministicSubsetCore
  - ZoneCrossingCore, PergyraCoreZoneBridge, PergyraCoreComposition, GenericAxisCarriage
  - AuthorityDelegationCore, AuthorityRequiresWitness, EffectAuthorityCore, IntentStepSoundness, LossCompositionCore
  - GuardCalculus, BinaryAdequacy, ProofCarryingIR, FormalKernel, BasisCompleteness, VerificationMethodology, ProofSpine, SoTAuthority, AssumptionBudget, PergyraMulCost
  - OptionTry, CheckedArith. CheckedArith는 헤더의 런타임 파일 경로만 틀렸다.

## 운영 메모

- 감사 중 에이전트 하나가 공유 WSL에서 멈춘 probe를 끄려고
  `pkill -f "rocq compile"`를 한 번 실행했다. 그 순간 다른 레인의 Rocq
  컴파일이 함께 죽었을 수 있다. 같은 시각 무렵의 원인 모를 Rocq 실패는
  이것으로 설명될 수 있다.
- `OwnershipGraphLinks.v`는 다른 레인이 수정 중이다(위 표의 "다른 레인"
  항목). 이 감사는 그 직전 판(`8dba4bc6...`)으로 probe를 검증했다.

## 권장 순서

1. **런타임 버그 후보부터**: C1(intent handle에 세대 또는 incarnation
   비교 추가)과 A-F12(`Now()`가 int32 realtime). 이 둘은 모델 문제가 아니라
   제품 동작 문제다.
2. **High 모델 수정**: B-F1(rollback이 Released를 되살림), B-F2(WF),
   B-F3(크기 0 region), A-F1(cancel 대기), A-F4(scorecard 주장 축소).
3. **재사용 계열**: M2, A-F2, C2를 세대 기준으로 보강한다.
4. **Low**: 정의상 참인 정리를 "인터페이스 고정"으로 표시하고, 문서
   인용(19, 22, 155, 173, 206, 10 §7)을 정리 수준에 맞춘다.
