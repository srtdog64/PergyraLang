# 세로 스파인 이관 원장 (PergyraCore)

## 진단 (확증됨)

증명 코퍼스는 **가로로 넓고 세로로 성긴** 상태다.

- Coq 파일 40개, `Admitted`/`Axiom` 0개, 의도된 추상 `Parameter` 2개(`SlotCalculus`)만.
- 커널 위생(`coqchk` axiom budget + 오염 반례 self-test)은 강함.
- **그러나** 파일 간 `Require Import`가 **0개**였다. 40개(당초 38개)가 각자
  `principal/zone/cap/slot/config/step`을 로컬 재정의하고 Coq 표준 라이브러리만
  가져오는 독립 모델이었다. capstone인 `UnifiedCore.v`조차 네 corner를
  `Require Import`하지 않고 사설 복제 위에서 재증명한다.
- 결과: 한 파일의 정리가 다른 파일의 정리와 **합성되지 않는다**. 개별 불변식
  팩은 강하지만, 컴파일러 전체를 잇는 통합 soundness는 없다.

## 이번에 착지한 것 (첫 세로 엣지)

세션에 prover가 없어(Coq 검증은 Linux/CI 전용) 기존 green 파일은 **건드리지 않고**
추가만 했다. 최종 검증 권위는 rocq9 CI의 `coq_kernel_check.sh`.

- **`PergyraCore.v`** (신규, 뿌리): `UnifiedCore.v`의 검증된 추상기계 모델
  (state + `step`/`steps` 관계 + `slot_in_true`/`cmap_circulation`)을 **그대로**
  공용 파일로 추출. 수학은 한 줄도 안 바꿈, 위치만 이동. axiom 0개 유지.
- **`PergyraCoreComposition.v`** (신규, 첫 엣지): `Require Import PergyraCore`로
  공용 `step`/`steps` 위에서 **새 합성 정리** 증명 (`acquire_then_use`,
  `acquire_then_release_steps`). 코퍼스 최초의 파일 간 증명 엣지. 재서술이 아니라
  imported 관계의 실제 다단계 합성.
- **게이트 인프라** (`coq_kernel_check.sh`): 이 flat 코퍼스에서 cross-file
  `Require`가 되도록 (1) `-Q . ""` load-path, (2) foundation-first 컴파일 순서
  (`PergyraCore.v`를 알파벳 순서와 무관하게 먼저 빌드 — 이름이 앞서는 importer가
  생겨도 안전). 기존 파일은 `Coq.*`만 Require하므로 무영향. bash 배관은 가짜-prover로
  로컬 검증(40 proofs, budget 2, 순서 확인).

### 검증 상태 (정직)

- 게이트 bash 배관: **로컬 green** (가짜-prover 하네스).
- `PergyraCore.v`: verbatim 추출이라 컴파일 확률 높으나 **로컬 coqc 미실행**.
- `PergyraCoreComposition.v`: **신규 증명**, 로컬 미검증. tactic 세부
  (`store_after_fill`의 `simpl`/`rewrite Nat.eqb_refl`, `eapply SStep` 메타변수
  해소)가 CI에서 조정 필요할 수 있음.
- → **rocq9 CI가 첫 실검증**. red면 tactic 한두 줄 조정 예상.

## 2단계에 착지한 것 (UnifiedCore 이관 + zone corner 엣지)

- **`UnifiedCore.v` 이관 완료**: 사설 모델 복제(원래 46-179줄)를 삭제하고
  `Require Import PergyraCore`. synthesis 정리(capability_soundness,
  authority_conservation, rollback_restores, delegate_*_sound,
  delegation_furnishes_gated_rollback 등)는 처음 이관할 때 그대로 보존했다.
  2026-10-08 레드팀 수정에서는 이 보존을 철회했다: rollback은 현재 수명을
  유지하고 snapshot으로 Released/Empty를 복원하지 않는다. bulk 생성자로
  발급된 capability 사본도 제거하고 실제 SDelegate edge를 합성의 전제로
  받는다. 수명은 공용 뿌리의 step/run 보존 정리로 유도한다. 로컬 관측은
  tests/proof_redteam_lifecycle_authority_smoke.sh의 fresh kernel gate로
  구분하며, 원격 CI 및 값 보상/실제 compensate 순서의 refinement는 별도다.
- **`PergyraCoreZoneBridge.v` 추가**(zone corner → 뿌리 엣지, additive):
  `ZoneCrossingCore`의 세 보장(무앰비언트 권한·capability soundness·fail-closed)을
  PergyraCore `step`의 `ActCross` 제한에서 재유도. green corner 파일은 안 건드림
  (그 모델은 2필드 config라 rename이 아니라 refinement). corner→뿌리 연결을
  지금 세우고, corner 파일 자체의 rewrite는 prover 루프에서.

## 로드맵 (남은 단계, prover 루프 필요)

1. **네 corner 파일 rewrite**: `ZoneCrossingCore`(→ ZoneBridge 정리가 실내용이 됨)/
   `EffectAuthorityCore`/`SlotLifecycleCore`/`AuthorityDelegationCore`를 PergyraCore
   `step`의 부분관계로 재정의 → UnifiedCore가 corner 정리를 **합성**(현재는 재증명).
   나머지 세 corner의 bridge(effect/slot/delegation)도 zone과 같은 패턴으로 추가.
2. **refinement bridge**: gen2가 소비하는 live semantic/AIR/MIR owner fact와
   PergyraCore 모델을 잇는 refinement 의무. (`AIRBinding.v`가 "gate가 특정 fact만
   읽는다"는 모형을 넘어, 실제 AIR/MIR 구현이 그 모형을 구현한다는 연결.)
3. **보존 정리**: parser → AST → semantic → MIR 전체 보존. 그리고 exceptional/
   cancellation cleanup, transitive scheduler 종료성 (현재 열림).

## 순서 주의

초기 `FOUNDATION_FIRST`는 `PergyraCore.v` 하나였다. 현재 목록은
`tests/coq_kernel_check.sh`가 소유한다. import가 늘어나면 실제 의존 순서를
반영해야 하며, 파일명 정렬을 의존 순서로 가정하지 않는다.

## 2026-10-06: 조정 확장 기계의 실제 import 연결

- `WholeProgramCore`의 task/done을 포함한 확장 기계는 7개 연산의
  `PergyraCore`와 아직 다른 모델이다. 둘의 동일성을 이번 작업에서 주장하지 않는다.
- `AIRBinding`의 사설 config/action/guard_machine을 삭제했다. AIR record의
  guard는 실제 `WholeProgramCore.guard`에 fact를 공급하는 투영이다.
- 마지막 boolean 판정 소비자 `BinaryAdequacy`도 사설 기계/AIRFacts/guard
  복제를 삭제하고 두 owner를 import한다. 기존 adequacy/locality 정리가
  이제 같은 imported 타입 위에서 검사된다.
- 이 세 파일의 변경본은 WSL Coq 8.18에서 실제 컴파일·커널 검사했다.
  추가 axiom은 없다. 구현 AIR/MIR producer refinement나 Rocq 9.0.1의
  변경본 CI 검증을 대신하지 않는다. 위의 역사적 첫 단계 미실행 기록은
  당시 기록으로 유지한다.
