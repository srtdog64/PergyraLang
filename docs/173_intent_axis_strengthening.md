# 173. intent 축 강화 — 쪼개지 말고 번들 의무를 강제하라

Status: `design-blueprint` (구현 금지 — rung/게이트/정리 목표만 고정). 작성
2026-07-05. 계기: BDFL — "intent는 (M1 판정에서) 너무 약한데 방법 없나? 증명이
가능한가, 아니면 원자 primitive로 쪼갤까?" 상위: docs/semantics/22 §1.6(약함
판정의 정본), docs/172(M1), CoordinationCore.v + CompensationCore.v(이미
기계화된 intent 분해), docs/167 B축(정적 충돌그래프), docs/142(guard 상각).

---

## 0. 세 질문에 대한 한 장 답

1. **방법 있나 — 있다.** 결정적 사실(2026-07-05 문법 실측): intent 표면은
   이미 전부 **선언**한다 — 헤더가 참여자(`intent I(z: Z, p: P)`), step이
   사용(`using:`/`who:`/`where:`), 정책(`exclusive; priority; rollback:`).
   약한 것은 어휘가 아니라 **강제**다: 이 선언들에 대해 declared⊇used를
   아무도 검사하지 않는다. 즉 intent를 ★★★로 올리는 길은 새 발명이 아니라
   **capability 축이 밟은 경로의 이식**(선언은 있음 → 검사 pass만 부재).
2. **증명 가능한가 — 가능하되, 어떤 증명인지 정확히.** Felleisen 비표현성
   *정리*(모든 국소 번역에 대한 전칭)는 확립 구문들도 안 가진 research-grade
   — 약속하지 않는다. 약속하는 것: (i) 번들 의무들이 **컴파일 거절**이 되는
   것(관찰 구별 성립 = semantics/22의 실용 기준), (ii) 기계화 가능한 목표 정리
   **`checked_intent_guard_free`**: 정적 의무를 통과한 intent는 런타임 가드
   (충돌 abort / missing-comp / stuck-step)가 **결코 발화하지 않는다** — 이미
   증명된 CoordinationCore(`run_requires_deps`)·CompensationCore
   (`rollback_requires_log`)의 **전제를 정적 검사가 방출(discharge)**하는
   구조(AIRBinding의 guard_air_faithful 패턴 재사용). 따름: 그 가드는 소거/
   상각 가능(docs/142) = **intent 축의 erasure 정리**.
3. **원자로 쪼갤까 — calculus에서는 이미 쪼갰고, 표면에서는 쪼개면 손해.**
   이론층은 이미 원자다: Coordination(의존 DAG) / Compensation(eff↔slot) /
   충돌(레지스트리) — 별도 .v 파일·별도 정리. 표면 `intent`는 그 원자들의
   **번들**인데, 이건 결함이 아니라 확립된 패턴이다:
   - **선례 = `match`**: 코어에서는 중첩 case로 전개되는 composite지만, 표면
     구문이 **exhaustiveness라는 번들-수준 정적 의무**를 나르기 때문에 1급
     구문이다. intent도 동일 — 번들 의무(§2)가 표면 primitive 자격의 근거.
   - **이론 anchor = BDI**: Cohen-Levesque "Intention is *choice with
     commitment*" — intention의 형식 구조 자체가 {지속성(persistence =
     exclusive/priority) + 계획(plan = steps) + 재고(reconsideration =
     rollback)}의 번들이다. 표면 번들이 임의 접착이 아니라 **이론의 모양**.
   - **쪼개기의 비용**: intent는 thesis의 "왜" 축(lost-meaning의 core),
     AIR 자체가 intent-topology IR, A-15/AC-3/observability ABI가 전부
     intent를 말한다. 표면 해체는 도메인 의미 회복이라는 존재 이유를 깎고,
     남는 원자(steps/saga/mutex)는 각각 기존물이라 novelty도 잃는다.

**판정: 쪼개지 않는다. 대신 번들에 정적 의무 4개를 강제해 M1 판정을 ☆☆☆ →
★★★로 올린다.** 단 §4의 반증 조건이 실패하면 쪼개기 재론(증거 기반으로).

---

## 1. 현 상태 정밀 진단 (왜 ☆☆☆인가)

- 런타임: subject-충돌 진입 거절(fingerprint), parent forest(F1 depth-bound),
  trace/observability ABI — **전부 실행 시점**.
- 정적: AIR가 intent-topology fact를 **방출**하지만, 방출은 거절이 아니다.
- 표면: 선언 어휘 완비(헤더 참여자, step `using:/who:/where:`, `exclusive`,
  `priority`, `rollback`) — **검사 없는 선언**.
- 결과: "런타임 레지스트리 라이브러리로 재현 가능하다"는 반론을 못 꺾음
  (semantics/22 §1.6).

## 2. 강화 사다리 (INT rungs — 전부 기존 증명 패턴의 이식)

| rung | 의무 | 이식 원본 | 거절 예 | 게이트 |
|---|---|---|---|---|
| **INT-1** | **참여자 declared⊇used**: step의 `using:/who:/where:`와 step 본문이 만지는 subject/zone은 intent 헤더 선언 집합의 원소여야 함. 본문 호출은 interprocedural 전파(정적 한계 = 동적 디스패치/FFI → 런타임 레지스트리가 backstop — capability와 동일 잔차 구조) | capability `declared ⊇ used`(★★★ anchor) | 헤더에 없는 제3 subject를 step이 touch → semantic error | reject fixture + interproc fixture |
| **INT-2** | **보상 커버리지**: `rollback: full` intent의 모든 effectful step은 보상 바인딩 또는 명시 `irreversible` 마커 필요. 없으면 거절(fail-closed) | match exhaustiveness / `rollback_requires_log`의 정적 얼굴 | comp 없는 eff step in full-rollback intent → error | reject fixture |
| **INT-3** | **step 의존 DAG**: 선언 의존은 acyclic(사이클=거절 — F1 사이클 클래스의 intent-수준 정적 차단) + 읽는 데이터는 의존으로 커버(`reachable_dep_closed`의 정적 얼굴) | CoordinationCore 전제 | `step A after B; step B after A;` → error | reject fixture |
| **INT-4** | **cross-intent 정적 충돌**: 정적으로 같은 subject를 쥐는 두 intent가 동시-가능 위치(parallel/spawn)에 있으면 ordering/priority 증거 없이는 거절 또는 직렬화 → ExecutionLaneFact 증거 공급 | docs/167 B축(성분/채색) + SEA 증거 계약 | `parallel { RunIntent(A(x)); RunIntent(B(x)); }` 무증거 → error | SEA 게이트 확장 |

**비표현성 논증의 형태(semantics/22 갱신용)**: INT-1~3은 전부 **본문-전체
(quantify-over-body) 의무**다 — exhaustiveness처럼, 국소 매크로 전개가 원리적으로
강제할 수 없는 종류. INT-1 하나만으로 ☆☆☆→★★☆(capability와 동형), 1+2+3으로
★★★.

## 3. 증명 목표 (INT-5) — intent 축의 erasure 정리

`IntentObligations.v` (신규, AIRBinding 계보):

- 정적 well-formed 술어 `intent_checked` = INT-1(참여자 커버) ∧ INT-2(보상
  커버) ∧ INT-3(의존 DAG).
- **정리 `checked_intent_guard_free`**: `intent_checked i` ⇒ whole-program
  머신에서 i의 실행 중 {충돌 abort, missing-comp 거절, stuck-step}이 발화하는
  도달 가능 상태가 없다. 증명 구조 = 기존 정리의 전제 방출: `run_requires_deps`
  의 deps-완료 전제는 INT-3이, `rollback_requires_log`의 log-존재 전제는
  INT-2가, 충돌-부재는 INT-1+INT-4의 분리 증거가 공급.
- **따름 `checked_intent_erasable`**: 그 가드들은 checked intent에 대해 소거/
  호이스트 가능 — docs/142 상각 프레임의 intent 인스턴스, air_erasure 계기판에
  intent 버킷 추가 근거.
- 정직 표기: 이것은 "intent가 비표현적"이라는 정리가 아니라 "정적 의무가
  런타임 가드를 대체한다"는 정리다 — 그러나 그게 정확히 capability/slot이
  ★★★인 이유와 같은 종류의 사실이고, 이 언어의 amortized-cost 포지션을
  intent 축까지 확장한다.

## 4. 반증 조건 (쪼개기 재론의 트리거 — 선기록)

- **R-1**: 실코퍼스(던전크롤러 + 비게임 1개)에서 intent 참여자가 정적으로
  해석 불가한 비율이 지배적(예: 대부분 동적 subject)이면 INT-1의 정적 가치가
  붕괴 → 번들 의무 전략 실패 → 표면 분해(saga/steps/exclusive 원자) 재론.
- **R-2**: INT-2의 `irreversible` 마커가 코퍼스에서 default처럼 남발되면
  보상 커버리지는 의무가 아니라 소음 → INT-2 재설계.
- 어느 쪽도 발화 전엔 분해 논의 금지(이 문서가 그 결정의 기록).

## 5. WO 등록 + 시퀀스

- **WO-INT-1** — 참여자 declared⊇used semantic pass(+interproc, reject/
  interproc fixture). ★최대 레버리지·기존 capability pass 패턴 이식.
- **WO-INT-2** — 보상 커버리지 검사 + `irreversible` 표면 마커.
- **WO-INT-3** — step 의존 DAG 정적 검사(acyclic + dep-closed).
- **WO-INT-4** — 정적 충돌그래프 → lane 증거(docs/167 WO-N3과 동일 작업 —
  중복 등록 아님, 같은 일의 두 문서 참조).
- **WO-INT-5** — `IntentObligations.v`(`checked_intent_guard_free` +
  erasable 따름) + formal/proof-spine smoke 등록.
- 시퀀스: INT-1 → INT-3 → INT-2 → INT-5(정리는 의무 셋이 실물이 된 후) →
  INT-4(SEA 트랙과 합류). 전부 self-host M2와 독립(semantic pass 계열이라
  병행 가능하나, 우선순위 경쟁은 BDFL 몫).

## Related

docs/semantics/22 §1.6(약함 판정 — 이 문서가 그 행의 작업지시) · docs/172
(M1 원장) · CoordinationCore.v / CompensationCore.v(전제를 방출당할 기존
정리들) · AIRBinding.v(guard↔fact 충실성 패턴) · docs/167 B축(INT-4 동일
작업) · docs/142(상각 프레임) · docs/grammar/00_cheatsheet(§표면 선언 실측:
헤더 참여자 + step `using:/who:/where:` + `exclusive/priority/rollback`) ·
Cohen-Levesque 1990(번들의 이론 anchor — intention = choice with commitment).
