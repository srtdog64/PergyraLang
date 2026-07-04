# 155. 삼률 — 의미를 선언하라, 드리프트를 게이트하라, 잔차는 fail-close하라

Status: `canon`. BDFL 채택 2026-07-04 (메타 평가 대화에서 정식화·명령).
이 문서는 프로젝트 전체가 이미 따르고 있던 운영 법칙을 한 문장으로
접은 것이며, `formal-semantics-smoke`가 삼률 문구와 §3 정의를 잠근다.

> **의미를 선언하라. 드리프트를 게이트하라. 잔차는 fail-close하라.**

## 0. 세 명령의 근거

- **의미를 선언하라.** Rice 정리상 의미는 분석으로 복원 불가능하다 —
  그래서 이 언어는 판정기가 아니라 **선언 primitive의 집합**이다
  (docs/semantics/19 §✦: 의미를 구문으로 끌어내려 결정가능화). 시행령:
  축 입장 조건(docs/151 §1 — 표면 착지점 없는 개념은 축이 아니다),
  intent/world/zone/authority/caps가 전부 선언 구문인 이유.
- **드리프트를 게이트하라.** 의도와 추상화는 반드시 깨진다 — 깨짐은
  사고가 아니라 정상 상태다(docs/122). 그러므로 방어는 "안 깨지게"가
  아니라 **깨짐이 무음일 수 없게**: 측정된 목소리를 게이트가 잠그고
  (parity·probe·matrix-lock·border-registry), 닫힘 정정 기록이 옛
  노트를 이긴다. 게이트는 세션-독립 제도 기억이다 — 컨텍스트가
  소멸하는 협업자(사람이든 AI든)를 가로질러 사실을 보존한다.
- **잔차는 fail-close하라.** 선언과 게이트가 덮지 못하는 나머지 —
  결정 불가능(Rice 잔차)이거나 아직 미구현이거나 — 는 무음 통과가
  아니라 **거절(REJECT/DEFER) 또는 런타임 fail-closed(GATE·budget·
  caps·panic 클래스)**다. Unhandled 금지(GuardCalculus의
  no_silent_ub). GATE 남용 금지 조항(docs/151 §3)이 이 명령의 경계다:
  fail-close는 결정-불가능의 짝이지 미구현의 변명이 아니다.

## 1. 자기적용 — 삼률의 가장 희귀한 성질

언어가 프로그램에게 요구하는 것을 프로젝트가 자신에게 적용한다:

| 층 | 선언 | 게이트 | fail-close |
|---|---|---|---|
| 언어 | intent/zone/caps 선언 구문 | parity·소거 계기판 | panic 클래스·budget·GATE |
| 설계 결정 | 결정표(docs/151)·간선 등록부 | matrix-lock·probe 잠금 | 미결정 셀=REJECT 래칫 |
| 코드베이스 | 경계 등기부(docs/154) | border-registry smoke | 미등재 교차=FAIL |
| 프로세스 | 보드 WO·닫힘 정정 기록 | CI smoke 패밀리 | "없다" 전제 금지·재심 기록 |

대부분의 언어는 자기 철학과 모순되는 프로세스로 만들어진다. 이
프로젝트의 세 결과물(논제·컴파일러·방법론)이 서로를 강화하는 이유는
이 재귀에 있다.

## 2. BDFL 시퀀스 결정 (2026-07-04)

> **이 검증을 완성하고, 그다음에 조합 안전성을 본다. 언어는 C로써
> 이미 돈다 — 열린 것은 SoT의 완전 닫힘이지 언어의 작동이 아니다.
> 핵심 문제는 셀프 부트스트래핑 — self-host가 끝나지 않은 것.**

- 순서: **① 검증 프로그램 완성(§3의 조작적 정의) → ② 조합
  안전성(A-15) → 핵심 목적지 = self-host/자기 부트스트랩**(North Star
  = self-eating bootstrap, docs/self_hosted/01 Stage 5 — 기존 결정
  유지·재확인).
- 상태 인식의 정정: "돌아가는가"는 열린 문제가 아니다 — C-backed
  언어는 실행·강제·parity까지 작동한다. 열린 것은 (a) SoT 잔여 닫힘,
  (b) 자기 자신을 컴파일하는 능력.
- 기록: 직전 메타 평가의 권고(dogfood-first)는 이 시퀀스에 종속된다 —
  dogfood는 표준 시퀀스(BETA closure → dogfood → Intent-Compress →
  BETA+ self-host)의 제자리를 유지하며, 검증 완성이 그보다 앞선다.

## 3. "검증 완성"의 조작적 정의 (분위기 금지 — 체크리스트)

아래 전부가 닫혀야 ①이 완성이고 ②(조합 안전성)에 진입한다. 각 항목은
보드의 실존 WO다. **2026-07-04 전 항목 닫힘 — ②(A-15) 진입 조건 성립.**

- [x] **WO-F1** — AxisOwnership 후속 두 정리 닫힘(2026-07-04):
  `ReadingConfluence.v`(read_order_irrelevant — 완전한 읽기는 순서 무관
  수렴, 불완전 읽기는 발산 가능 witness 포함) + `BinaryAdequacy.v`
  (accept_adequate/reject_adequate/guard_decidable — 2진 표면 판정이
  calculus guard와 정확히 일치, 제3상태 없음). coqc green(0 admits/
  0 axioms) + formal smoke 배선 + docs/semantics/19 corpus 표 갱신.
- [x] **WO-A1** — machine-neutral 게이트 test-all 승격(2026-07-04):
  5회 연속 byte-identical 결정성 실측 후 test-all 편입(ci-linux/
  ci-windows 양쪽 전파). 기동 실패도 RED(무음 skip 없음), python 부재
  fallback 셸 게이트도 hard-fail 확인.
- [x] **WO-A2** — erasure dashboard CI 게이트화(2026-07-04):
  `make air-erasure-gate`(재실측+판정) + ci-windows 배선. 승격 과정에서
  게이트가 진짜 드리프트를 적발 — R6 워치독이 전 프로그램 기저를
  Sync+2/Abort+1로 올림 → **substrate floor를 심볼 이름으로 baseline에
  고정**(floor 성장도 RED). RED 양방향 실증(축 심볼 주입/floor 심볼
  주입) + 복원 GREEN.
- [x] **GuardCalculus↔구현 연결 확장**(2026-07-04):
  `GuardWitnessBinding.v` — guard 정책의 op class를 런타임 panic-class
  레지스트리(pgy_runtime_panic_contract.h)의 명명 witness에 결속.
  can_be_bad_has_witness(Proven인 OpSlotRelease도 backstop 보유 —
  always-on defense-in-depth 기계화) + witness_disjoint(진단가능성 1:1).
  smoke가 모델·코드 양쪽에서 같은 class 문자열을 요구 — 어휘 드리프트
  무음 불가. 갭 축소이지 제거 아님(guard 발화 정확성=failclosed 픽스처,
  방출 커버리지=twin parity 소관 — negative scope 명기).
- [x] **G-rung 중 검증 성격 잔여** — 상시 조건. 2026-07-04 기준
  matrix-lock green 재확인(반증 배터리·probe는 landing 커밋 게이트로
  잠김; 이후 회귀는 해당 스모크가 RED로 표면화).

## 4. 조합 안전성 스코프 (②, A-15로 등재)

R4 잔여가 정확한 정의다: AxisOwnership.v는 **축-소유권**(exactly-one-
owner·no-silent-override·confluence)만 기계화했고, **전 쌍별 축-조합**
(6축 → 15쌍)의 안전은 미증명이다. 방법은 이번 주에 검증된 것의
재적용:

- 쌍마다: 간선 등록부(docs/151 §4 + docs/154)에 등재된 간선이면 그
  간선의 정리/커널, 미등재 쌍이면 **비간섭(non-interference) 실측
  커널**(axis-carriage probe의 축×축 판) + 필요 시 Coq 조각.
- G-4(생성자 경계 검사 — Slot/Channel 행 개방)와 합류: 조합 안전이
  서야 축-합성 셀을 열 수 있다.
- 산출물: 15쌍 × {정리|커널|간선} 매트릭스, matrix-lock 계보의 잠금.

## Related

docs/semantics/19 §✦(Rice — 선언의 근거) · docs/122(드리프트 정상
상태) · docs/semantics/20(정직 원장·no_silent_ub) · docs/151(결정표 —
삼률의 결정 층 적용) · docs/154(경계 등기부 — 코드베이스 층 적용) ·
docs/self_hosted/01(North Star — 시퀀스의 목적지) · TODO 보드
WO-F1/WO-A1/WO-A2/A-15
