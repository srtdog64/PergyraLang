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

## 로드맵 (다음 단계, prover 루프 필요)

1. **`UnifiedCore.v` 이관**: 로컬 모델 복제를 삭제하고 `Require Import PergyraCore`.
   기존 synthesis 정리(capability_soundness, authority_conservation, rollback_*)는
   그대로 유지 — 이제 공용 뿌리 위에서. (검증 없이 green capstone 리팩터는 금지라
   이번엔 보류.)
2. **네 corner 이관**: `ZoneCrossingCore`/`EffectAuthorityCore`/`SlotLifecycleCore`/
   `AuthorityDelegationCore`를 PergyraCore의 `step`의 부분관계로 재정의하고,
   corner 정리를 공용 관계로 다시 진술 → UnifiedCore가 corner 정리를 **합성**하도록
   (현재는 재증명).
2. **refinement bridge**: gen2가 소비하는 live semantic/AIR/MIR owner fact와
   PergyraCore 모델을 잇는 refinement 의무. (`AIRBinding.v`가 "gate가 특정 fact만
   읽는다"는 모형을 넘어, 실제 AIR/MIR 구현이 그 모형을 구현한다는 연결.)
3. **보존 정리**: parser → AST → semantic → MIR 전체 보존. 그리고 exceptional/
   cancellation cleanup, transitive scheduler 종료성 (현재 열림).

## 순서 주의

`FOUNDATION_FIRST`는 현재 `PergyraCore.v` 하나. 이관이 진행되어 importer가 늘면
(특히 알파벳상 뿌리보다 앞서는 파일이 PergyraCore를 Require하면) 이 리스트에
추가하거나 `coqdep` 기반 위상정렬로 승격할 것. 지금은 뿌리 하나라 단순 우선컴파일로 충분.
