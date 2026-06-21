# 134. 외부 레드팀 리뷰와 프로젝트 입장 (2026-06-21)

외부 리뷰어(친구)가 준 7개 비판에 대한 정직한 판정 + 현재 기제 + 남은 작업 +
게이트를 한 곳에 고정한다. 이 문서는 마케팅 반박이 아니라 **리스크 계약서**다.
핵심 메타-관찰: 이 레드팀은 ~90% 정확하고, 그 비판이 *프로젝트 자신의
beta-closure 계약(docs/100a)과 수렴*한다 — 외부인이 독립적으로 같은
load-bearing 리스크를 짚었다 = 리스크 모델이 옳다는 신호다. 단, **등록 ≠ 닫힘.**

판정 범례: ✅동의·기제있음 / 🟡동의·부분 / 🆕새 신호 / ↩️반박(부분)

---

## R1. SoT drift — ✅ 동의, 이미 #1 closure 목표

"AST/MIR/AIR/runtime/doc/golden이 같은 사실을 여러 이름으로 들면 반드시
깨진다. `_dbg/_rel`, fallback, alias, source_ast semantic read가 공격면.
보안 취약점 이전에 *컴파일러가 거짓말할 수 있는 구조*."

- **판정:** 옳다. 구조적 주장 그대로 참. docs/100a의 5대 closure target *전체
  주제*가 이것(CFG/MIR fact를 SoT로, AST fallback 제거, DAG compat seam 제거).
- **기제:** source-inventory / resolver-inventory / mir-declaration-inventory
  스모크가 compat seam 재오픈을 *거부* → "깨진다"를 "조용히 재도입 못 한다"로.
- **worked example (오늘 닫음):** `_dbg/_rel` 쌍둥이 ABI를 canonical 1개 +
  alias로 접음(parity 64/0 + C/LLVM 26/26 검증). 그리고 evidence refresh 중
  `build-source-inventory` 게이트가 `lifecycle_state.c`의 top-level mutable
  static 레지스트리(`g_lc_specs`/`g_lc_guards`)를 잡아냄 → 단일 owner accessor로
  교체(게이트 green). 게이트가 실제로 SoT drift를 잡는다는 산 증거.
- **남은 작업:** 83%. fallback에 남은 consumer 다수(특히 declaration/projection
  emitter, DAG zone authority/generic provenance). **게이트:** 위 3개 inventory
  스모크.

## R2. C/LLVM 비대칭 — ✅ 동의, 가장 깊은 진짜 리스크

"C는 조용히 fallback, LLVM은 fail-closed → 둘 중 하나가 언어가 아니라 구현
사고. AOT 신뢰엔 C/LLVM/self-hosted가 *같은 MIR fact*만 소비해야."

- **판정:** 옳고 가장 날카롭다. 반복 parity failure = 미성숙 신호 정확.
- **증거:** bitcode-strip(LLVM 인라인 abort mis-lower), "C는 양 블록 / LLVM은
  use-block만" parity 버그(2026-06-10 수정) — 둘 다 실제 C/LLVM divergence였다.
- **정직한 한계:** parity gate(backend_compare 814 fixture, tri-compare)는
  **필요조건이지 충분조건 아님**. 진짜 치료는 target #4(양 백엔드가 같은 MIR
  declaration inventory만 소비)이고 ~83% — 오늘도 *모든* 경로에서 동일 fact를
  소비하진 않음(문서 인정).
- **게이트:** `air-strict-backend-compare-test-smoke`,
  `backend-output-tri-compare`, `mir-declaration-inventory-test-smoke`.

## R3. 안전성 모델의 착시 — ✅ 동의, 정직의 핵심

"Slot/capability/authority가 있다고 자동으로 메모리 안전/보안이 되진 않는다.
raw/FFI/parallel/async/container ptr/ABI 중 한 번만 fail-open이면 철학 전체
뚫림. Rust가 borrow checker로 막은 걸 evidence+fail-closed+runtime-guard로
막으려면 verifier가 훨씬 더 단단해야."

- **판정:** 옳다. **현재 Pergyra는 메모리 안전 언어가 아니다** —
  fail-closed-by-design + *불완전* 정적 verifier + 런타임 backstop.
- **증거:** 이번 세션에 보고된 slot UAF fix를 감사했더니 "완벽 보장"이 과장 +
  버그였음 → 안전 verifier에 실제 구멍이 있었던 직접 증거.
- **계산이론적 바닥(docs/19 §✦):** Rice 정리상 verifier는 원리적으로 complete
  불가 → runtime guard가 Rust의 정적 보장보다 더 많은 일을 짊어진다. 그래서
  *기본값 fail-closed*가 load-bearing.
- **정직한 비교:** Rust도 unsafe/FFI는 fail-open 구멍 — 둘 다 trusted 경계 있음.
  차이는 Rust의 *safe subset*이 정적으로 total(borrow checker sound)이라는 것.
  그 차이가 결정적이고, 이 항목은 친구 편이다.
- **남은 작업:** raw/FFI/parallel/async/ABI 경계의 fail-open 감사를 1급
  closure 항목으로(현재 분산). 마케팅에서 "메모리 안전"·"Rust-equivalent"
  표현 금지(vision 라벨만).

## R4. 개념 수 → verifier 부담 — ↩️ 부분 반박 (BDFL)

친구: "World/Zone/Role/Roster/Intent/Slot 자체는 과잉 아님. 단 직교성 증명 +
소거/압축 조건이 없으면 *모든 조합*이 버그 표면."

- **반박(BDFL):** "쓸 수 있다 ≠ 반드시"가 결정적이다. 조합 버그 표면은
  *mandatory*일 때만 단조 증가한다. 프로젝트엔 이를 받치는 **3중 기제**:
  1. **Optionality** — 코어축 1개 + 나머지 mandatory/optional 태그
     (project_core_module_layering). 안 쓰면 그 조합은 그 프로그램에 부재 →
     verifier 부담 pay-per-use.
  2. **Erasure** — AIR 소거 계기판: 축 어휘 100% 소거 *실측*(docs/14). 안 쓴
     개념은 dormant이 아니라 컴파일타임에 0으로 사라진다. "안 쓰면 괜찮다"의
     강한 버전 = "안 쓰면 측정상 없다".
  3. **Orthogonality** — Coq `AxisOwnership.v` 기계검증(exactly-one-owner /
     no-silent-override / confluence / surface-adequacy). 실제로 조합한 축은
     emergent 상호작용 없이 clean 합성.
- **정직한 잔여 갭:** (3)의 커버리지. 축-ownership은 증명됐지만 모든 *쌍별
  feature 상호작용*은 아직. → 친구 worry는 "유효하나 설계가 이미 대부분 답을
  들고 있고, orthogonality 증명을 전 조합으로 넓혀야 한다"가 정확한 판정.
- **남은 작업:** orthogonality 증명 커버리지를 축-ownership → 주요 쌍별 조합
  (World×Zone, Zone×Intent, Intent×Slot)으로 확장. 참조: docs/42, Coq 트랙.

## R5. self-hosting = 위험 구간 — ✅ 동의, 이미 BDFL 결정

"지금 밀면 자기 증명이 아니라 두 번째 미완성 컴파일러. runtime/diagnostic/
deterministic collections/AST-like mixed tree/raw-FFI 닫히기 전 완성 주장 금지."

- **판정:** 옳고, 친구가 독립적으로 프로젝트 결정에 도착함:
  self-host parser 86/117에서 정지(2026-05-29 pivot), C+LLVM backend 먼저,
  self-host는 post-BETA partial only(project_self_host_pause_backend_first).
- **드리프트 없음.** 게이트: `self-host-preparation-test-smoke`(완성 주장 아닌
  준비 evidence로만).

## R6. 보안 언어로 팔기엔 부족 — 🆕 친구가 진짜 새 신호를 더함

"'안전한 Flash 부활' = sandbox + deterministic asset/runtime boundary +
capability-sealed host API + wasm/native equivalence + resource budget +
DoS 모델. Slot/authority만으론 부족."

- **판정:** 옳고, *이 문서에서 가장 가치 있는 추가*다. 우리가 만든
  capability-sealed host API는 친구 리스트 중 **하나**뿐.
- **핵심 빈틈:** **capability는 정성적(X를 할 수 있나 yes/no)인데, sandbox는
  정량적 한계(CPU/메모리/시간 budget)도 필요하다.** capability를 grant해도
  무한 루프·메모리 고갈·fork bomb은 못 막는다. Slot/authority/capability가
  *얼마나*를 손도 안 댐. 이건 capability 모델이 비운 통째 축.
- **남은 작업(미구현, 우선순위순):**
  - **resource budget / DoS 모델** — capability의 정량 축(CPU/mem/time/alloc
    예산 + 초과 시 fail-closed). docs/15 §2에 신규 항목으로 등록.
  - deterministic asset/runtime boundary.
  - wasm/native equivalence 증명(같은 manifest·같은 행동).
  - 서명 manifest + 로더 self-impose(`pgy_cap_set_manifest_export`는 있음).
- **마케팅 규율:** budget+determinism+wasm-equiv 닫히기 전 "안전한 Flash"는
  vision 라벨만(capability overclaim 가드).

---

## 종합 판정

| # | 항목 | 판정 | 상태 |
|---|---|---|---|
| R1 | SoT drift | ✅ | 5대 target, 83%, 게이트 강제 중. 오늘 ABI fold + lifecycle SoT 닫음 |
| R2 | C/LLVM 비대칭 | ✅ | parity gate = 필요조건. 구조적 치료 target #4 ~83% |
| R3 | 안전성 착시 | ✅ | 메모리 안전 언어 아님. fail-closed + 불완전 verifier. 경계 감사 필요 |
| R4 | 개념 수 | ↩️ | optionality+erasure+orthogonality 3중 방어. 증명 커버리지만 부분 |
| R5 | self-host | ✅ | 이미 동결(post-BETA partial). 드리프트 없음 |
| R6 | 보안 엄격성 | 🆕 | resource budget/DoS = 비운 축. docs/15 §2 신규 등록 |

**한 줄:** 친구는 약점을 정확히 봤고, 프로젝트는 그 약점들을 *이미 리스크로
등록하고 closure 중*이다. 위험한 건 약점의 존재가 아니라 약점을 *모르는* 것 —
우리는 안다. 다만 등록 ≠ 닫힘이고, 마케팅을 그 숫자(83%, budget/DoS 0%)보다
앞세우는 순간 이 리뷰가 치명타가 된다. **그래서 닫기 전엔 vision 라벨만.**
