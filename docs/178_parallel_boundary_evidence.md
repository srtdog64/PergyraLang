# 178. 병렬 경계 = 증거 문제 — 한 규율, 세 지향의 사영

Status: `design-blueprint` (구현 0 — F2 정책의 상위 설계 + DOP 갭 WO). 작성
2026-07-06. 계기: BDFL — "우리는 변형 OOP + FP + DOP 다중 지향이니, 병렬(캡처/
경계) 정책이 한 지향의 답이면 안 된다." 상위: docs/177(F1/F2 실측), docs/113
(교리), docs/146(SEA — "lane은 증거로 결정"), docs/157(AC-3 정리 T),
IntentConflict.v(`sep_when_active`), project_core_module_layering(★패러다임은
축이 아니다 — 이 문서는 그 canon을 지킨다).

---

## 0. 한 장 요약

- **원칙**: 패러다임별 병렬 규칙 3벌을 만들지 않는다(축 아님 canon). 대신
  **경계 규율은 하나** — *"parallel 경계를 건너는 모든 접근은 증거가 필요하고,
  증거 없으면 거절"* — 이고, 세 지향은 그 규율 위의 **관용 사영(idiom
  projection)**이다. 이는 새 발명이 아니라 기존 canon의 확장이다: SEA가 "lane
  은 증거로 결정"이라 했고, AC-3 정리 T가 이미 격리-경계에 Clone/Channel
  증거를 요구한다. 이 문서는 그 증거 어휘를 병렬 경계에 대해 **완결**한다.
- **증거 4종**(§1): Copy(Clone) · Channel · Exclusivity(단일 작성자/intent
  exclusive) · **Disjointness(서로소 분할 — 신규)**.
- **지향 사영**(§2): FP→Copy/Channel, 변형 OOP→Exclusivity(intent 계보),
  DOP→Disjointness. 실측 커버리지: FP 거의 완비, OOP는 intent-수준만(문장-수준
  = F2), **DOP는 통로 부재**(분할 primitive·병렬-for 없음, 컬렉션 통짜 거절).
- **F2의 재정의**: 태스크 #2(스칼라 캡처 정책)는 이 분류의 문장-수준 집행이다
  — "다중 작성자 거절"= Exclusivity 증거 부재의 거절, "copy-in 기본"= 스칼라의
  기본 증거를 Copy로 지정하는 것.

## 1. 증거 4종 (경계 통과의 어휘)

| 증거 | 의미 | 검사 형태 | 기존 canon 대응 |
|---|---|---|---|
| **Copy** | 값 스냅샷이 건너감 — 원본과 절연 | Clone/copy-capture (클로저 Stage A와 동일 교리) | AC-3 T의 Clone leg |
| **Channel** | 소유권/데이터가 프로토콜로 이동 | 스레드-안전 런타임 (실측 green) | AC-3 T의 Channel leg, 크로스-World 유일 통로 |
| **Exclusivity** | 공유하되 동시 접근이 배제됨 | 단일-작성자+join(구조적), intent `exclusive`/admission | IntentConflict.v `sep_when_active`(양보 조건들), 단일-arm 쓰기 p2 패턴 |
| **Disjointness** ★신규 | 공유하되 접근 영역이 서로소 | 분할 fact(slice split의 범위 비중첩) | `sep_when_active`의 subject-서로소 leg의 **데이터 판**; BasisCompleteness의 분리 정리 계보 |

무증거 접근 = 거절(fail-closed). F1의 무음 폴백 제거와 함께, "왜 이 접근이
허용되는가"가 항상 4증거 중 하나로 답해진다 — 관측 가능성(§1.1) 충족.

## 2. 세 지향의 사영 + 실측 커버리지

| 지향 | 관용 병렬 패턴 | 필요 증거 | 현 상태 (docs/177 실측) |
|---|---|---|---|
| **FP** | 불변 값 fan-out, 결과는 채널/Future로 | Copy + Channel | **거의 완비** — spawn 인자 copy-only, 컬렉션 공유 거절, Channel/Slot green. 잔여: 스칼라 copy-in 기본(F2) |
| **변형 OOP** (subject/vessel/intent) | 공유 개체를 배타 undertaking으로 | Exclusivity | intent 수준은 실물(admission/exclusive/priority + IntentConflict.v). **문장 수준이 구멍**(스칼라 포인터 공유 무가드 = F2) |
| **DOP** (데이터 지향) | 데이터 테이블을 서로소 구간으로 갈라 일괄 처리 (게임 ECS/SoA 배치) | Disjointness | **통로 부재**: SplitAt/Chunk류 분할 primitive 없음(실측 — Slice/SliceCopy만), 병렬-for 표면 없음, 가변 컬렉션은 경계에서 통짜 거절 → 서로소 절반 2개에 병렬 쓰기를 표현할 방법이 없다 |

DOP가 가장 병렬-친화적 지향(그래서 게임 엔진이 ECS로 감)인데 우리 통로가
0이라는 게 이 감사의 설계-측 핵심 발견이다. 던전크롤러(킬러 유즈케이스)의
엔티티 배치 처리 = 정확히 이 관용구.

## 3. DOP 갭의 최소 채움 (WO-DOP-1, 설계만)

- **분할 fact**: `SplitAt(slice, i) -> (Slice, Slice)` 류 — 두 결과의 범위
  비중첩이 **구성상 보장**(rayon `split_at_mut` 계보). 컴파일러는 분할 산출물
  임을 fact로 알고, 경계 검사에서 Disjointness 증거로 인정.
- **표면**: 신규 구문 최소화 — 1차는 분할 산출물을 기존 parallel arm에 하나씩
  캡처(arm당 하나의 서로소 조각 = 증거 자명). 병렬-for(`parallel for chunk in
  Split(...)`)는 그 다음 rung(표면 결정 = BDFL).
- **거절 유지**: 분할 아닌 컬렉션 공유는 지금처럼 거절 — 이 문서는 거절을
  느슨하게 만드는 게 아니라 **증거 있는 통로 하나를 여는 것**.
- 순서: F2(#2) → F1(#1) 뒤. ExecutionLaneFact에 Disjointness가 데이터-병렬
  lane 증거로 합류(docs/146).

## 4. 반-확산 가드

- 패러다임 키워드/모드 스위치 금지 — "FP 모드/OOP 모드" 같은 표면은 이 문서가
  명시적으로 거절한다(core/module canon). 지향은 **증거 선택으로 표현**된다.
- 증거 5번째 후보(atomic 등)는 실수요(탈출구 실측) 전 추가 금지 — 4종이 세
  지향을 덮는다는 게 본 설계의 주장이고, 못 덮는 사례가 나오면 그게 반증.

## Related

docs/177(F1/F2 실측 — 본 문서가 F2의 상위 프레임) · docs/157+ZoneCrossingCore
(Clone/Channel 증거의 정리) · IntentConflict.v(Exclusivity/서로소의 기계화 —
Disjointness는 그 데이터 판) · docs/146(증거-기반 lane — 같은 문장의 데이터
확장) · project_core_module_layering(패러다임≠축 canon) · 클로저 Stage A
(copy 교리 선례) · rayon split_at_mut / ECS-SoA(DOP 계보) · 태스크 #1/#2.
