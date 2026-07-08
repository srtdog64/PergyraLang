# 175. 상태공간 탐색/퍼징 설계도 — 선언이 탐색공간을 공짜로 준다

Status: `design-blueprint + FUZZ-2 partial` (AIR lifecycle manifest landed; explorer not yet). 작성 2026-07-06. 계기: BDFL — "코드 자체가
탐색공간이 되어 늘리고 줄이며 fuzz할 수 있나? 라이브러리로 들어가야 할 것 같다
— 우리 코드는 상태공간 정의가 비교적 명확하니." 상위: docs/19 §✦(판정이 아니라
선언), docs/167(위상 분석), docs/174 §4(결정론 배당금 — 이 설계도의 전제),
IntentSpine.v/IntentConflict.v(탐색공간의 기계화 모델), docs/159(stdlib 승격 —
라이브러리 착지 경로).

---

## 0. 한 장 요약

- **가능하고, 유일하게 잘 위치한다.** 보통 언어에서 "프로그램의 상태공간"은
  Rice-차단(힙 앨리어싱/임의 제어흐름에서 FSM 복원 불가)이라 퍼저가 브랜치
  커버리지 같은 **기계 근사**를 쓴다. Pergyra는 상태공간이 **선언 그 자체**다:
  vessel valid-from mask(유한 FSM), slot affine(3-상태), intent step DAG +
  admission 규칙, zone 트리, cap lattice. 탐색기는 소스 분석 없이 **AIR fact를
  읽고 열거**하면 된다 — "판정이 아니라 선언"의 테스팅 배당금.
- **BDFL 판정(라이브러리) 동의 + 경계 정밀화**: 탐색기/생성기/shrinker/coverage
  는 전부 라이브러리(축 추가 0 — Functor soft-no 규율과 정합). 단 라이브러리가
  서려면 언어·컴파일러 측 **인터페이스 3개**가 전제인데, 실측 결과 2개는 있거나
  예약됐고 1개만 실작업이다(§3).
- **탐색 모드 4개**(§2): 값 fuzz+shrink / **스케줄 탐색(킬러)** / 프로그램 공간
  grow-shrink(이미 배아 존재) / 유한 모델체킹. 스케줄 탐색에서 방금 증명한
  `sep_when_active`(IntentConflict.v)가 **partial-order reduction의 independence
  관계로 그대로 재사용**된다.
- **Pergyra 고유 기여 = semantic coverage**(§2-e): 브랜치 카운터가 아니라
  **선언 공간 위의 커버리지**(방문한 vessel 상태×전이 / 발화한 guard 클래스 /
  intent step·conflict 분기) — 커버리지 지표 자체가 도메인 의미를 갖는다.

---

## 1. 왜 여기서 특별히 성립하는가 (실측 근거)

| 선언 자산 | 열거 가능한 공간 | 상태 |
|---|---|---|
| vessel valid-from mask | 유한 FSM(상태×허용 전이) | semantic lifecycle registry → AIR `lifecycle_state_spaces[]` → AIR JSON summary로 노출. 남은 일: semantic coverage consumer |
| slot own/ref/release | affine 3-상태 체인 | AIR slots[] 노출됨 |
| intent 헤더+step+dep | 스케줄 = 선언 DAG의 interleaving (IntentSpine `sched_ok`가 그 공간의 Coq 모델) | AIR intents[] step-단위 노출 |
| intent admission | enter/leave trace 공간 (IntentConflict `trace_ok`) | 런타임 규칙 — Coq 전사 완료 |
| zone/world 트리+channel | place/link 그래프 (BasisCompleteness) | AIR boundaries 노출 |
| capability mask | 유한 lattice | manifest 노출 |

**탐색공간의 형식 모델이 이미 기계화돼 있다**는 것이 결정적이다 — 탐색기가
열거할 대상(sched_ok trace, trace_ok admission 순서)이 이번 주 Coq 파일의
inductive 그 자체다. 탐색기는 그 모델의 실행기다.

**기존 배아(실측)**: `src/self_hosted/fuzz/backend_parity_generator/main.pgy` —
"Deterministic backend-parity fuzz corpus generator, `<seed> <count>`" — 이미
seed-결정론적 프로그램 생성기(Csmith 계보)가 self-hosted로 존재. §2-c는 신규
발명이 아니라 이것의 일반화다.

## 2. 탐색 모드 4 + 커버리지

- **(a) 값 fuzz + shrink** (QuickCheck/Hypothesis 계보): 도메인 타입에서 생성,
  반례 축소. Hypothesis의 핵심 발명(internal shrinking = 값이 아니라 **choice
  sequence를 축소**)이 우리와 정확히 맞물린다: choice sequence = seed + 결정론
  배지(§3-②)면 재현이 공짜.
- **(b) 스케줄 탐색 ★킬러 모드** (P 언어/Coyote 계보): intent/coordination
  선언이 P의 state-machine 선언과 동형이므로, 동시성 버그 사냥이 "선언된
  interleaving 공간의 체계적 열거"가 된다. **POR(partial-order reduction)의
  independence 관계 = `sep_when_active`**(subject-서로소면 순서 교환 가능) —
  방금 증명한 분리 술어가 탐색 가지치기의 건전성 근거로 재사용. 탐색 대상은
  checked 프로그램이 아니라 **runtime-residue 경계**(동적 디스패치/FFI —
  정리가 못 덮는 정직한 잔차)다: 정리와 퍼저가 같은 경계선의 양쪽을 분담.
- **(c) 프로그램 공간 grow/shrink** (BDFL이 말한 "코드 자체가 탐색공간"):
  생성기(존재)를 늘리고, delta-debugging(Zeller/C-Reduce 계보)으로 줄인다.
  **Pergyra 고유 우위 = shrink 건전성**: C에서 축소는 UB를 새로 만들어 버그를
  가릴 수 있지만, 우리는 UB 표면이 닫혀 있어(fail-closed) 축소 후보가
  ① 컴파일 거절(skip) ② 같은 panic class(keep) ③ 통과(discard)의 3-버킷뿐 —
  **무음-오류 버킷이 없다.** 축소가 신뢰 가능한 언어는 드물다.
- **(d) 유한 모델체킹**: vessel FSM × guard = 소형 explicit-state 탐색(SPIN/
  TLA+ 계보의 아주 작은 판). checked 프로그램은 정리(guard_free)가 커버하므로,
  체커의 값은 **미검사 조합**과 회귀 탐지.
- **(e) semantic coverage**: 커버리지 = {방문 vessel 상태·전이} ∪ {발화 guard
  클래스} ∪ {실행 intent step·admission 분기}. 브랜치 카운터와 달리 **미커버
  항목이 도메인 문장**("Vessel이 Empty→Poured 전이를 한 번도 안 탐")이라
  리포트가 곧 명세 리뷰가 된다. air_erasure 계기판과 같은 계보의 fact-계기판.

## 3. 라이브러리/언어 경계 (BDFL 입장의 감사)

**탐색기 전체 = 라이브러리가 맞다** (stdlib 승격 경로는 docs/159 doctrine-pass).
축 추가 0. 단 전제 인터페이스 3:

착지 경로는 바로 stdlib이 아니다. 실험/자립 단계의 소유자는
`src/self_hosted/fuzz/state_space/`이며, stdlib 승격은 AIR-fact manifest,
deterministic replay badge, shrink policy, parity driver가 모두 존재할 때만
가능하다. 이 경계는 `src/self_hosted/README.md`의 non-negotiable rule로도
잠근다.

| # | 인터페이스 | 상태 |
|---|---|---|
| ① | **상태공간 manifest** = AIR JSON | lifecycle FSM까지 1차 노출됨(`lifecycle_state_spaces[]`: subject/states/ops/valid_from_mask). 남은 갭은 coverage consumer와 explorer manifest 통합 |
| ② | **결정론 재현** = replay-safe 배지 | docs/174 §4 WO-CERT-DET — 이 설계도의 전제. 퍼저의 재현 요구가 그 배지의 첫 소비자 |
| ③ | **스케줄 주입 훅** | ★유일한 실작업: 런타임이 외부 스케줄(admission/lane 순서)을 받는 driven-mode. 선례 있음 — `PGY_CAP_GRANT`/`PGY_BUDGET_*` env 채널과 동일 패턴(`PGY_SCHED_TRACE=<seed|trace>`), capability-gated(퍼저 권한도 선언되어야 — sandbox 규율 유지) |

## 4. WO 등록 + 시퀀스

- **WO-FUZZ-1** — 스케줄 주입 훅(런타임 driven-mode, env 채널 + cap 게이트).
- **WO-FUZZ-2** — partial landed: vessel FSM fact가 AIR JSON에 노출됨. Remaining: semantic coverage fact 정의와 explorer-side consumer.
- **WO-FUZZ-3** — 탐색 라이브러리 본체(생성/shrink/POR 스케줄러/coverage 리포트)
  — post-M2, stdlib 승격 흐름(docs/159)으로. 기존 backend_parity_generator의
  일반화로 시작.
- 전제: **WO-CERT-DET**(docs/174) 선행.
- 시퀀스: CERT-DET → FUZZ-2(fact) → FUZZ-1(훅) → FUZZ-3(라이브러리, 던전크롤러
  dogfood와 결합 — 게임 상태공간이 첫 실전 코퍼스).

**정직한 한계**: 상태공간이 유한한 건 **선언된 축**뿐이다 — 값 공간(Int/String/
배열)은 여전히 무한하고 거기선 우리도 남들과 같은 휴리스틱 퍼저다. 우위 주장은
정확히 "선언 축의 곱공간에 한정"으로 유지할 것(3-pair 규율).

## Related

docs/19 §✦(선언→결정가능 — 이 설계도의 존재 근거) · docs/174 §4(결정론 배지 =
전제) · IntentSpine.v `sched_ok` / IntentConflict.v `trace_ok`+`sep_when_active`
(탐색공간의 기계화 모델 + POR independence) · `src/self_hosted/fuzz/backend_
parity_generator`(존재하는 배아) · docs/159(stdlib 승격 경로) · docs/167
(위상 분석 — coverage 계기판의 인접) · QuickCheck(Claessen-Hughes 2000) ·
Hypothesis internal shrinking(MacIver) · P 언어(Desai+ PLDI'13)/Coyote ·
delta debugging(Zeller)/C-Reduce · Csmith(차등 컴파일러 퍼징 — 기존 gate 계보).
