# 174. 게임 어휘 완전성 감사 — 채굴 광산에 남은 광맥

Status: `audit + gap-ledger` (구현 0 — 갭 후보와 결정 소유자만 고정). 작성
2026-07-06. 계기: BDFL — "게임 휴리스틱 모델에서 가져왔는데 빠진 거 있나?"
= docs/172 **M2(완전성)의 소스-도메인 실전판**: 참조 프레임(bigraph)이 아니라
**채굴 원천(게임 세계-묘사 온톨로지)** 대비 전수 대조. 방법: 게임 개발이 세계를
묘사할 때 쓰는 범주를 전수 나열 → 현재 축/자산에 매핑 → 4-버킷 판정
{커버 / 조합-배당금 / library / **기저 후보 갭**}.

선행 canon 확인(2026-07-06 grep — orthogonality 메모리 규율): 관측 축은
**미심의**(docs/24는 lexical 가시성 = 모듈 접근이지 도메인 관측 아님, 런타임
관측 힛 0). 시간 축도 전용 심의 없음. 충돌 없음 확인 후 작성.

---

## 0. 한 장 요약

- **대부분 커버된다** (§1) — party·intent(GOAP)·authority(서버 권위)·slot
  affine(아이템 복제 불가)·zone/world가 게임 온톨로지의 중핵을 이미 쥔다.
- **진짜 기저 후보 갭 1개**: **관측 축(who-sees-what)** — fog of war/은닉
  정보/interest management/anti-cheat (§2). 5-전통 삼각측량이 성립하는데
  semantics/21의 9행 표에 **행 자체가 없다** = 삼각측량 표의 누락 발견.
- **2순위 갭 1개**: **시간 구조(turn/phase/cooldown/deadline)** (§3) — 게임과
  **BDFL 본업(공장 PLC)이 동시에 요구**하는 유일한 미보유 범주.
- **갭이 아닌 배당금 1개**: **결정론/리플레이**는 새 축이 필요 없다 — 기존
  fact 3개의 조합으로 도출되는 **인증서**다 (§4). 곱공간 논증(docs/172 §3)의
  실증 사례.
- 나머지(공간 메트릭/스탯 스태킹/세이브/난이도)는 library 또는 기존 축 조합 (§5).

---

## 1. 커버 확인 표 (게임 범주 → 현재 자산)

| 게임 범주 | 현재 자산 | 판정 |
|---|---|---|
| 개체/행위자 | subject (func/action) | 커버 |
| 집단(파티/팩션) | **party** (party Squad\<T\> where T: CombatReady) | 커버 — 고유 강점 |
| 상태/변신 | vessel/lifecycle (typestate) | 커버 |
| 공간 포함(방/구역) | zone/world (+BasisCompleteness place-graph 동형) | 커버 |
| 소지/장비 | slot own/ref, Token | 커버 |
| **아이템 복제 버그** | slot affine = 자원 비복제 | 커버 — **경제 게임 셀링포인트로 문서화 가치**(MMO 듀핑 = 재앙 클래스) |
| 목표/퀘스트 체인 | intent + coordination(dep DAG) — GOAP과 1:1 | 커버 |
| 거래/교환 원자성 | intent compensation + transfer | 커버 |
| 서버 권위/판정 | authority (+AuthorityIrreducibility: 권위=grant 역사) | 커버 |
| 이벤트/트리거 | event + channel | 커버 |
| 스폰/풀링 | spawn + slot claim/release | 커버 |
| 확률/주사위 | Random + PGY_CAP_RANDOM 게이트 | 커버(capability로) |
| AI 행동(BT/GOAP) | intent(BDI anchor)가 GOAP 구조 그대로 | 커버 |
| 속성표(상성 등) | 데이터 | library |
| **누가 무엇을 보는가** | ★없음 (§2) | **기저 후보 갭** |
| **턴/페이즈/쿨다운** | ★없음 (§3) | **2순위 갭** |
| 리플레이/결정론 | 조합으로 도출 (§4) | 배당금 |
| 공간 메트릭(거리/사거리) | 없음 — 데이터로 충분 | library (§5) |
| 파생 스탯/버프 스택 | relation+event+trace 조합 | 조합+library (§5) |
| 세이브/스냅샷/롤백넷코드 | world 격리가 enabling, 미완 | 미래 조합 (§5) |

---

## 2. Gap A — 관측 축 (who-sees-what) ★기저 후보

**게임 증거**: fog of war, 은닉 정보(포커 핸드), interest management(멀티플레이어
가시성 컬링), **anti-cheat의 절반**("클라이언트에 보이면 치터에게 보인다" —
정보를 안 보내는 것이 유일한 방어). 세계 묘사에서 "존재"와 "관측 가능"은 게임이
항상 구분하는 두 사실이다.

**표면에 이미 있는 제스처(실측)**: `func` = "private internal computation
(**audience can't see**)", `action` = "public plot behavior (**audience sees**)"
— battle_sim의 주석이 문자 그대로 관객-관측 어휘를 쓴다. 슬롯층엔
ViewRead/ViewWrite(읽기 lease)가 있다. 그러나 도메인-상태 관측("이 subject의
hp는 어느 zone/audience에 보이는가")을 **선언·검사하는 축은 없다**. docs/24의
가시성은 모듈 접근(누가 impl/참조 가능)이지 런타임 관측이 아니다 — 다른 평면.

**삼각측량 (semantics/21 표의 누락 행 — 5전통 성립)**:
| 전통 | 대응 |
|---|---|
| 게임 | fog of war / interest mgmt ★ |
| DDD | **CQRS read model / projection ★** |
| BDI/MAS | **epistemic logic(지식 연산자) ★** |
| 상위 온톨로지 | (약 — 인식론적 범주) |
| 논리/PL | **IFC/noninterference ★** (Denning; Jif/FlowCaml), epistemic 양상논리 |

**thesis 함의(BDFL 결정 사항)**: 7 lost meanings(누가/왜/어디까지/자격/세계/
책임/전이)에 "**누가 아는가**"가 없다 — "누가 하는가(authority)"와 별개 질문.
채택 시 8번째 의미이거나 '어디까지'의 세분. **기저 수정이므로 BDFL 전결** —
M4(소거 게이트)가 수정 비용의 보험이라는 것까지가 이 문서의 몫.

**킬러 유즈케이스 직결**: 던전크롤러 fog of war = 1차 dogfood에서 즉시 필요;
"신뢰로 승부"(sandbox 비전)의 멀티플레이어 판 = 관측 통제.

**정직한 비용**: IFC는 무겁기로 악명 높다(noninterference 정적 증명은 실패한
언어 이력이 많음). Pergyra다운 경로는 완전 정적 IFC가 아니라 **선언 + fail-closed
게이트**(capability 축과 동일 구조: 선언된 관측 경계 밖 read를 정적으로 잡고,
잔차는 런타임 게이트) — docs/19 "판정이 아니라 선언"의 관측판.

## 3. Gap B — 시간 구조 (turn/phase/cooldown/deadline) — 2순위

**게임 증거**: 게임 루프/턴 순서/이니셔티브/쿨다운/지속시간 — 게임의 시간은
공간만큼 구조적이다. "네 턴 동안만 행동 가능" = **시간-스코프 authority**.

**★유일한 이중-도메인 신호**: 공장 PLC(BDFL 본업)도 정확히 같은 범주를 요구
한다 — scan cycle/deadline/watchdog(IEC 61131). 두 dogfood 측점이 **모두**
요구하는 미보유 범주는 이것뿐 — 삼각측량상 가장 강한 "다음 축" 신호.

**현존 자산**: R6 wall-time budget(watchdog), PgyTimer, coordination(순서),
SEA lane — 조각은 있으나 선언 어휘(phase/deadline/cooldown) 없음.

**이론 anchor**: 동기 언어(Esterel/Lustre — 산업 제어 실적), timed automata
(UPPAAL). **위험**: 실시간은 블랙홀 — 착수 전 negative-space 표(무엇을 안
할지: 스케줄러 증명? WCET? 거절 목록) 필수. **판정: dogfood에서 시간 어휘의
탈출구(주석/컨벤션으로 새는 시간 의미)를 관측한 후 착수** — semantics/21 §4
스트레스 테스트의 1호 예상 발화 지점.

## 4. 결정론/리플레이 — 갭이 아니라 곱공간 배당금

리플레이/락스텝/스피드런 검증이 요구하는 "seed가 같으면 세계가 같다"는 **새
축이 필요 없다**: `결정론 = ¬RANDOM cap ∧ ¬CLOCK cap ∧ KPN coordination
(reachable_dep_closed)` — 이미 있는 fact 3개의 조합. `--capability-manifest`에
**"replay-safe" 배지**로 도출 가능(구현은 manifest 확장 한 조각). docs/172 §3
곱공간 논증("축 조합이 새 의미를 생성한다")의 첫 실증 사례라는 점에서 이론
가치도 있다. **WO-CERT-DET로 등록 — 싸고 즉시 가능.**

## 5. Library 버킷 확정 (기저 아님)

- **공간 메트릭**(거리/사거리/시야선): 데이터+함수로 충분. 선택적으로 zone
  인접성만 relation fact로(관측 축 채택 시 시야선과 결합). 축 아님.
- **파생 스탯/버프 스택**: relation+event 조합. 남는 진짜 질문은 "왜 데미지가
  47인가"의 **귀속(attribution)** — trace 축의 확장이지 새 축 아님.
- **세이브/스냅샷/롤백 넷코드**: world 격리(채널-only)가 이미 구조적 enabling.
  스냅샷 primitive는 분산 vision과 함께 서랍에.
- **난이도/밸런스**: 순수 데이터.

## 6. WO 등록 + 권고

- **WO-GAP-A** — 관측 축 설계 심의(BDFL 결정 선행: 8번째 lost meaning인가).
  채택 시 경로: 선언(`observable by` 류) + declared⊇used 스타일 검사 + 런타임
  게이트 backstop — capability 축 이식 4번째 반복.
- **WO-GAP-B** — 시간 구조: dogfood 탈출구 관측 후. 착수 조건 = 던전크롤러
  or 공장 픽스처에서 시간 의미가 주석/컨벤션으로 새는 실측.
- **WO-CERT-DET** — replay-safe 배지(manifest 확장). 즉시 가능, 저비용.
- semantics/21 표에 perception 행 추가(누락 교정)는 이 문서 착지와 함께.

**감사 총평**: 채굴은 놀랄 만큼 완전했다 — 남은 광맥은 "존재하는 것"이 아니라
"**보이는 것**"(관측)과 "**언제**"(시간)라는, 세계 묘사의 두 비-공간 차원이다.
전자는 5-전통 삼각측량이 이미 성립하고, 후자는 두 dogfood 도메인이 동시에
가리킨다. 어느 쪽도 지금 착수 대상은 아니고(자기 규율: 기저 수정은 BDFL 전결
+ M4 보험 하에서만), 이 원장이 그 결정의 입력이다.

## Related

docs/172(M2 — 이 문서는 소스-도메인 완전성 sweep) · docs/semantics/21(삼각측량
— perception 행 누락 교정 대상) · docs/24(lexical 가시성 — **다른 평면**, 충돌
아님) · project_killer_usecase(fog of war 1차 수요) ·
project_capability_sandbox_vision(anti-cheat = 신뢰 스토리의 멀티 판) ·
project_industrial_software_context(시간 축의 2차 측점) · docs/167 B축+
IntentConflict.v(시간-스코프 authority의 인접 기계) · Denning 1976 / Jif /
FlowCaml(IFC) · Esterel/Lustre(동기 언어) · CQRS/read model(Evans-Fowler 계보).
