# 179. 강함 원장 — "이 언어는 강한가"의 상비 답변 (2026-07-11)

BDFL 질문 "이 언어는 강한가?"에 대한 판정을 축별 원장으로 고정한다. 이
문서는 자랑이 아니라 **부족분 대장(臺帳)**이다 — 각 축의 정직한 현재 수준,
열린 부족분, 그 부족분을 지키는(또는 지어야 하는) 게이트를 한 곳에 둔다.
docs/137(레드팀 계약서)·docs/166(프로덕션 바)과 같은 계열이며, 미래 세션이
"강한가/약점이 뭔가"를 다시 물으면 **fresh 분석 전에 이 문서부터** 읽는다.

판정 요약(2026-07-11): **지금 센 언어는 아니고, 강해지는 것 말고는 못 하게
만들어진 언어다.** 상태-강함(지금 세냐)과 구조-강함(강해지는 방향으로만
움직이게 돼 있냐)을 구분하는 것이 이 원장의 뼈대다.

## 축 1. 이론 — 강함 (기계증명 수준)

- **현재**: primitive마다 확립 이론 대응(docs/19 계보), 다형성=Wadler-Blott
  계보 감사 완료(docs/semantics/10 §9), Coq 기계증명 4트랙(AxisOwnership /
  BasisCompleteness / IRMinimality / CheckedArith), Rice 정리 정면 수용
  ("판정이 아니라 선언", docs/19 §✦).
- **부족분**:
  - 직교성 증명 커버리지: 축-ownership은 증명, **쌍별 feature 조합**
    (World×Zone, Zone×Intent, Intent×Slot)은 미증명 — docs/137 R4 잔여.
  - AxisOwnership 후속: reading-confluence / binary-adequacy.
- **게이트**: formal-semantics-test-smoke(존재) · Coq 재검증은 수동
  (ROCQLIB 로컬) — CI 승격 미정.

## 축 2. 보장 — 중간-강 (측정된 등급)

- **현재**: 강타입+기본 fail-closed. **메모리 안전 언어 아님을 자인**
  (docs/137 R3) — own/ref interprocedural UAF + 런타임 슬롯 태그 backstop,
  산술 UB 양 백엔드 폐쇄(CheckedArith), capability+budget 양축 런타임 강제
  (R6 core), 병렬 경계 증거 4종(docs/178, 전부 statement-level 실물 +
  CI 게이트), 소거 손실 bounded·measured·attributed(docs/14, air-erasure-gate).
- **부족분** (우선순위순):
  1. **R3 경계 감사 1급화**: raw/FFI/parallel/async/ABI의 fail-open 전수
     감사가 아직 분산 상태 — 단일 감사 문서+게이트로 승격 필요.
  2. **R6 잔여 3종**: deterministic asset/runtime boundary · wasm/native
     equivalence(기반=docs/161) · 서명 manifest 로더 self-impose.
  3. **런타임 행동 게이트 얇음**: 구조 게이트(inventory/parity) 대비 행동
     목격자(witness) 부족 — with-release 회귀가 직선-body 테스트를 통과한
     사건, backpressure 간헐 hang이 그 증거. 목격자 corpus의 체계적 확충
     (blocked-send/recv, spawn lifecycle, release 순서, budget 고갈,
     cap 거부 × 양 백엔드)이 다음 rung.
- **게이트**: backend-compare(914+) · test-sandbox-gates · air-erasure-gate
  · parallel-disjoint/snapshot · ability-coherence · evidence-lifetime
  (2026-07-11 CI 승격 완료).

## 축 3. 정체성 — 유일 (비교 불가 좌표)

- **현재**: lost-meaning recovery 좌표에 경쟁 언어 없음. 거절 능력이 형태를
  지킴(lifetime 주석 영구금지 · UInt 비노출 · HKT soft-no · walrus 제거 —
  전부 판정 기록 존재). C# 아버지 계보(docs/119) + 기저 선택 정당화
  (docs/172).
- **부족분**: 구조적 부족 없음. 리스크는 **마케팅 drift** 단 하나 —
  capability overclaim 가드(docs/120)와 negative-space 표(docs/118 §8)가
  이미 서 있고, 닫히기 전 표현은 vision 라벨만.
- **게이트**: documentation-quality + 가드 메모리(정성).

## 축 4. 구현 — 약함 (이 원장에서 가장 냉정해야 하는 축)

- **현재**: C 325k(dual backend 113k 세금), CI red 5계열(2026-07-10 전수
  진단, TODO 진행 노트), 채널 런타임 간헐 blocked-send deadlock 실존(칩
  task_863abddf), with-slot 조기 release 회귀 open(칩 task_8694fbe9),
  **사용자 실증 0**(thesis 앱 미완).
- **부족분** (닫는 순서):
  1. CI green 복구 — 칩 2건 + 스트림 몫 3건(likeness 110 복귀 포함).
  2. 런타임 행동 목격자 확충(축 2-3과 동일 항목, 구현 측 표현).
  3. 문자열 할당 구조 갭 — StrView rung(성능 유일 구조 갭).
  4. **검증 마일스톤 실행** — post_selfhost_validation_milestone.md V1~V5.
     V1 버그클래스 목록은 2026-07-11 동결(더 이상 "etc." 아님).
- **게이트**: ci 3플랫폼 + ratchet 군(likeness/inc-sentinel) + 진행 노트.

## 축 5. 구조(시간) — 강함 (진짜 답이 있는 곳)

- **현재**: ratchet은 후퇴를 기계적으로 거부, machine-neutral은
  FALSIFIED→재건→GREEN을 게이트 완화 0으로 통과, 레드팀 7비판이 자체
  리스크 등록과 수렴(~90%), 자기 목격자가 자기 런타임의 데드락을 잡음
  (2026-07-10). "깨짐을 숨길 수 없는 언어"라는 정의를 스스로 채택했고 그
  정의로는 이미 강하다.
- **부족분**: red-위-달리기 습관 — 13:07 red 위로 2회 추가 push되며 신규
  실패 3계열 유입(2026-07-10 실측). CI red = 스트림 정지 신호 규율이
  게이트가 아니라 관행에 머묾. (기계화 후보: red 상태에서 push 시 로컬
  훅 경고 — 강제는 BDFL 결정 사항.)

## 채움 순서 (2026-07-11 기준 단일 큐)

1. CI green 복구(칩 2 + 스트림 3) — 다른 모든 신호의 노이즈 바닥.
2. WO-PAR-1 manifest row(동시 세션 clean 후) + R3 경계 감사 1급화.
3. 런타임 행동 목격자 corpus 확충(설계는 축 2-3 항목).
3a. **런타임 자기적용 갭 청산 (2026-07-12 유형 확정)** — V1 동결표
   **#8(도메인 라이프사이클, S+R)의 R-측 반례 실측**: 채널 미초기화
   send가 warn+false-반환 후 계속 진행 → 수정-전 바이너리가 4.7GB
   warn-spam(사건 기록: 메모리 dev-pain 1f). 유형 = **계약-위반 연산의
   warn-and-continue**(3겹: 판정층=실패가 로그로만 관측되고 제어흐름에선
   소멸 / 진행층=무한 재시도 + budget 미선언 시 무한 자원 소모(R6
   default-open 실증) / 증폭층=시도당 warn 1줄 = 로그 O(시도수) DoS).
   메타-유형 = **자기적용 갭**: 사용자 코드에 강제하는 교리(라이프사이클
   fail-closed·§4 무한재시도 금지·budget)를 자기 런타임 primitive에
   미적용. 채널 fix=칩 task_a7b3717b, 전수 census 게이트=WO-RT-1(보드).
   **V1 본 실행 전 필수** — #8-R PASS 기준("fail closed deterministically,
   same C/LLVM observable")이 직접 걸림.
4. R6 잔여 3종(asset boundary → wasm-equiv → 서명 로더).
5. 검증 마일스톤 pre-work 소진 후 본 실행(V1 동결 목록 기준).

## 2026-07-17 델타 (세션 실측 — 원장 판정은 유지, 칸에 숫자가 붙음)

- **축1 이론 ↑**: Coq 4→5트랙 — MachineLayerCore.v 47정리(0 admits/0
  axioms), 기계층 스모크 4종+parity 3종 전부 GREEN(동시 세션 수직 슬라이스,
  host-sim envelope 명시 유지). negative scope 보존 확인.
- **축2 보장 ↑**: 방출 결정성 **909/911 byte-identical** 게이트 CI 승격
  (`df3c723c`, 잔여 2는 동시 WIP 컴파일 실패지 비결정 아님) +
  cross-manager slot alias fail-close(`6737f3d6`) + **중첩 병렬 데드락
  클래스 제거**(help-first await, `740f5a68`, RED 3/3→GREEN 4/4 목격자).
- **축3 정체성**: 외부 확증 — Roc의 Rust→Zig 리라이트 에세이(2026-07-15)가
  "which index goes with which array?"를 Rust/Zig 공통 미해결로 지목 =
  우리 좌표(WO-B4 handle@zone)가 실제 프로덕션 통증임을 제3자가 진술.
- **축4 구현**: **첫 외부 워크로드 성능 실증** — B_n 직렬 C-parity(best-of-3
  n=9: 14.39 vs 14.45s), 병렬 pool=auto 후 **Fortran OpenMP 이김**(n=10:
  47.6 vs 61.0s), hand-C OpenMP 1.18x 이내. 태스크 부기 기준선 10.7µs/task
  실측(WO-RT-4 B1). **사용자 실증 0은 그대로 — 여전히 최약점.**
- **축5 구조 재확인**: 이 세션 계열에서만 자기-오진 3회(직렬 경계검사 →
  병렬 "스케줄러 오버헤드" → "index-owner 답 보유")를 전부 측정이 잡아
  정정 커밋 — "깨짐을 숨길 수 없다" 정의가 작동하는 실황.
- 인프라 실사(16항목, 2026-07-17): EXISTS 5(디버그정보 양백엔드·LSP·fmt·
  REPL·진단 70/122/108) / PARTIAL 4(자체 opt=DCE만·pkg 로컬·크로스컴파일
  passthrough·WASM 간접) / ABSENT 7(**증분 컴파일**·핫리로드·docgen·
  프로파일러·compile_commands·유저 sanitizer). 개발자-대면 도구는 성숙
  언어급, 갭은 빌드-스케일(증분)과 생태계 축.

## Related

- docs/137 (레드팀 리스크 계약) · docs/166 (프로덕션 바) · docs/180 (논리
  척추) · docs/178 (병렬 증거) · docs/14 (소거 계기판) ·
  docs/post_selfhost_validation_milestone.md (반증 가능한 다음 증명) ·
  docs/186 (병렬 전체 구현 계획)
