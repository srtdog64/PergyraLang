# 21. 기저 수렴 삼각측량 — 다섯 독립 전통의 범주 대응

Status: `theory-argument` (M3 of docs/172). 작성 2026-07-05. 이 문서는 **증명이
아니라 thesis 논증**이다 — Church-Turing과 같은 인식적 지위. 개별 primitive의
형식 대응은 docs/semantics/19가 정본이고, 여기서는 한 가지만 논증한다:
**기저 어휘의 선택이 임의(arbitrary)가 아니라 자연 종(natural kind)에
수렴했다**는 증거.

BDFL 결정 기록(2026-07-05): 경험 지표(구 M5)는 descope — "이 조합이 맞다"는
경험적으로도 증명 불가능한 계열이라는 판정. 동의하며, 같은 정직성을 이 문서에도
적용한다: 삼각측량 역시 옳음을 증명하지 않는다. 그것이 확립하는 것은 §3에 정확히
한정한다.

---

## 1. 논증의 구조

**관찰**: 서로 다른 선택압 아래서 독립적으로 발달한 다섯 전통이, "세계와 그 안의
행위를 묘사하라"는 문제에 대해 **같은 범주 집합**에 도달했다.

**추론**: 기저 어휘가 임의 표집이었다면 이 수렴은 설명 불가능하다. 수렴은 해당
범주들이 도메인 묘사 문제의 구조에 내재함을 시사한다 — λ-calculus의 정당화가
파생이 아니라 독립 형식화들(Turing 기계/재귀함수/λ)의 수렴이었던 것과 동형.

**다섯 전통과 각자의 선택압**:

| 전통 | 선택압 (왜 독립인가) |
|---|---|
| 게임 개발 | 시뮬레이션 효율 + 플레이어 인지 부하. 수십 년 상업적 도태압 |
| DDD (Evans 2003) | 조직·엔터프라이즈 복잡도 관리. 게임과 무교류로 발달 |
| BDI/MAS (AI) | 자율 에이전트의 계획·합리성 형식화 (Bratman 철학 → Rao-Georgeff 논리) |
| 상위 온톨로지 (UFO/DOLCE/BFO) | 범주의 철학적 정합성. 소프트웨어 아닌 존재론에서 출발 |
| 논리/PL 이론 | 증명 가능성·건전성 (Kripke, Cardelli-Gordon, Honda, Abadi 등) |

Pergyra의 출처는 이 중 첫째(게임)의 채굴이었다(BDFL 자술). 삼각측량의 요지:
**나머지 네 전통이 같은 지점을 독립적으로 가리킨다.**

---

## 2. 범주 대응표

★ = 그 전통의 1급 개념. (약) = 근사물만 존재. — = 부재(정직 표기).
빈칸을 강한 대응으로 위장하는 순간 이 논증은 slop이 된다 — 부재 셀도 데이터다.

| Pergyra | 게임 관용어 | DDD | BDI/MAS | 상위 온톨로지 | 논리/PL |
|---|---|---|---|---|---|
| **subject** | actor/entity ★ | Entity/Aggregate ★ | agent ★ (Shoham AOP) | Agent (UFO-C) ★ | actor 모델(Hewitt), π 프로세스 |
| **intent** | AI goal/behavior-tree task/quest ★ | (약: command/use case) | **intention ★** (Cohen-Levesque; Rao-Georgeff; AgentSpeak에서 1급 실행 개념) | intentional moment (UFO-C) ★ | BDI 논리 |
| **zone** | trigger volume/area/room ★ | **Bounded Context ★** | institution/context ★ | situation/context | ambient ★ (Cardelli-Gordon), region (Tofte-Talpin) |
| **world** | world/level/shard ★ | (약: 배포 경계) | environment | (약: possibilia 논의) | **possible world ★** (Kripke), **ML5 world ★** (타입 내 위치) |
| **role** | class/job/faction ★ | role 패턴 | role ★ (Boella-van der Torre) | **Role ★ (UFO 1급 범주)** | (사상은 ability=typeclass, Wadler-Blott) |
| **vessel/lifecycle** | state machine/anim state ★ | aggregate 상태 | agent lifecycle | **Phase ★ (UFO 1급 범주)** | typestate ★ (Strom-Yemini; Plaid) |
| **authority** | server authority/permission ★ | (약: policy) | **institutional power ★** (Jones-Sergot counts-as; Searle 구성 규칙) | social/deontic 범주 | authz 논리(ABLP), ocap(Miller), deontic |
| **channel** | event bus/netcode channel ★ | domain event | speech act/ACL (FIPA) ★ | — | session types ★ (Honda), π 이름 |
| **effect** | side effect/script hook | domain event(중복) | action | perdurant/event | effect system ★ (Lucassen-Gifford; Koka) |

읽는 법: 행마다 ★가 3개 이상이면 그 범주는 다중-독립 수렴. 전 행이 3★ 이상이다.
가장 약한 행은 **world**(게임·논리에서 강하고 DDD·MAS에서 약함)와 **channel의
온톨로지 셀**(부재) — 이 둘이 기저에서 가장 "발명"에 가까운 부분이라는 정직한
지도이기도 하다.

---

## 3. 이 논증이 확립하는 것 / 못 하는 것

**확립**: 어휘 선택의 **비임의성(non-arbitrariness)**. "게임에서 휴리스틱으로
뽑았다"는 출생 서사는 "임의로 뽑았다"와 다르다 — 게임은 세계 묘사에 최대 도태압이
걸린 장르라, 그 관용어 채굴은 **수렴점 하나를 읽은 것**이고, 다른 네 전통이 같은
점을 독립 확인한다.

**확립 못 함** (각각의 담당자):
- **완전성** (빠진 범주가 없다) → M2: 참조 프레임 대비 정리.
  첫 단편 기계화 완료 — `BasisCompleteness.v`(bigraph 정적 단편: 완전성 +
  보존성 + world-separation, coqc PASS 2026-07-05).
- **최소성/독립성** (군더더기 범주가 없다) → M1: docs/semantics/22.
- **옳음** — 어떤 방법으로도 증명 불가(BDFL 판정과 일치). 삼각측량은 증거의
  무게지 증명이 아니다.

---

## 4. 편향 경고 (이 논증의 자기-한계)

1. **게임 편향**: 표의 첫 열은 이산적·시뮬레이션 가능·실시간 세계에 최적화된
   어휘다. 연속 프로세스(공정 제어), 규제 문서 중심(세무/법무), 장수명 트랜잭션
   (금융 정산) 도메인이 다른 1급 명사를 요구할 수 있다.
2. **2차 측점 의무**: 따라서 dogfood는 게임(던전크롤러) 하나로 부족하고, 비게임
   도메인 1개가 필요하다 — BDFL 본업(공장 장비 SW: 추적성·라이프사이클·권한이
   전부 1급)이 이상적 후보. 이는 descope된 경험 *지표*와 다르다: 지표 측정이
   아니라 **범주 스트레스 테스트**(새 1급 명사가 필요해지는가)다.
3. **survivorship**: 다섯 전통이 서로를 인용하며 오염됐을 가능성(완전 독립이
   아닐 수 있음). 특히 DDD와 MAS는 OO 전통을 공유. 삼각측량 강도를 5-독립이
   아니라 보수적으로 3~4-독립으로 읽는 것이 정직하다.

---

## Related

docs/172(플레이북 — 이 문서는 M3) · docs/semantics/19(개별 primitive 형식 대응
정본) · docs/semantics/22(M1 — 독립성 논증) · BasisCompleteness.v(M2 첫 단편) ·
docs/170(키워드 계보 지도 — 표의 PL 열과 상보) · project_lineage_synthesis
(C# 아버지 — 코어 어휘의 계보는 별도) · project_killer_usecase(1차 측점) ·
project_industrial_software_context(2차 측점 후보).
