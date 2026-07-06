# 172. 기저 선택 정당화 — 휴리스틱 어휘를 정리(定理)로 바꾸는 플레이북

Status: `design-blueprint` (구현 금지 — 이론 트랙 WO만 등록). 작성 2026-07-05.
계기: BDFL 질문 — "subject/intent/world는 프로그래밍에 없던 명사고, 게임의 세계
묘사에서 휴리스틱으로 뽑아 권한을 얹은 것이다. 자유 조합(범용 언어) 대비
조합론적으로 약한 이 이론적 약점을 어떻게 극복하나."
상위: docs/semantics/19(이론 대응 + core calculus — **개별 primitive의 이론
정당화는 그쪽이 정본**), docs/170(키워드 계보), docs/171(CS 이론 참조), docs/42
(직교성 canon), AxisOwnership.v. 이 문서는 그들이 안 다루는 한 가지만 다룬다:
**집합 선택(basis selection) 자체의 정당화 방법론.**

---

## 0. 한 장 요약

- 문제의 정확한 이름: **기저 선택 문제.** 개별 primitive가 이론 대응을 가진다는
  것(docs/19)과, *그 집합이 올바른 기저*라는 것은 다른 주장이다. 후자를 파생
  (derivation)으로 증명한 언어는 **역사상 없다** — λ-calculus도 관계대수도
  휴리스틱으로 태어나 **사후(post hoc) 정리**로 정당화됐다.
- 따라서 극복 경로는 "휴리스틱을 파생으로 대체"가 아니라 **사후 정당화 5무브**:
  M1 독립성(Felleisen) · M2 상대적 완전성(Codd) · M3 수렴 삼각측량(Church-Turing
  구조) · M4 보존적 소거(이미 게이트) · M5 경험 반증(dogfood).
- 조합론 반격: 직교 축은 고정 어휘 점이 아니라 **곱공간(생성 기저)**다. 자유
  조합 언어는 항(term) 수준 조합론만 갖고, Pergyra는 축 수준 조합론을 추가로
  갖는다(A-15 매트릭스가 그 조합론). 약점이 실재하는 조건은 축의 비직교/종속
  — 정확히 AxisOwnership.v + A-15가 지키는 것.
- 전제 교정: intent/world는 "프로그래밍에 아예 없는" 명사가 아니다 — intent는
  BDI/AOP 30년(1급 intention 언어 실존), world는 ML5(위치가 타입인 언어),
  vessel은 typestate-oriented programming(Plaid)의 직계. **없는 것은 한 언어로의
  synthesis**(기존 canon과 일치).

---

## 1. 역사적 사실 — 성공한 기저는 전부 휴리스틱으로 태어났다

| 기저 | 출생 | 사후 정당화 |
|---|---|---|
| λ-calculus | Church의 표기 선택 | Church-Turing **thesis**(독립 형식화들의 수렴 — 정리 아님, 삼각측량) |
| 관계대수 5-6 연산 | Codd의 실용 선택 | **Relational completeness**(1972, 1차논리 동치 정리) |
| 순차/분기/반복 | 구조적 프로그래밍 휴리스틱 | **Böhm–Jacopini**(1966, 모든 flowchart 표현 가능) |
| π-calculus | Milner 스스로 "canonical 프로세스 calculus는 없다" 인정 | 표현력 결과(λ 인코딩 등)로 부분 정당화 |

교훈: "휴리스틱이라서 약하다"는 상태 서술이 아니다. 정확한 상태는 **"사후 정당화
정리를 아직 다 안 쌓았다"**이고, 그건 작업 목록으로 변환 가능하다(§2).

---

## 2. 사후 정당화 5무브 + 현황 원장

### M1 — 독립성 (Felleisen macro-expressibility)
각 축이 "코어+나머지 축"으로 **국소 매크로 표현 불가**함을 보여야 primitive
자격. 표현 가능하면 라이브러리로 강등(그게 정직). Pergyra의 강점: 축들은 런타임
행동이 아니라 **컴파일 타임 검증 의무**(capability declared⊇used, lifecycle
typestate, parity fact)를 나른다 — Felleisen 프레임에서 정적 의미는 국소 확장이
못 담는 대표 사례라 통과 전망이 높다. 단 축별로 실제 논증을 써야 함.
**현황: 논증 landed (2026-07-05)** — docs/semantics/22. 판정: capability/slot/
lifecycle/zone 성립, authority 부분(cap×zone 환원 반론 미해소), **intent 얇음**
(정적 의무 부족 — docs/167 B축이 보강 경로). 낮은 행 2개가 남은 작업 지시.

### M2 — 상대적 완전성 (Codd 무브) ★최대 이론 갭
참조 프레임을 고정하고 "그 프레임의 모든 문장을 축 조합으로 표현 가능"을 증명.
후보 프레임(정확성 순 아님, 비용 순):
1. **Bigraph 단편**(Milner): place graph(공간 포함 = zone/world) + link graph
   (연결 = channel/relation)의 2-구조가 Pergyra 공간 축과 정확히 동형 후보.
   "world-description completeness" = bigraph 반응계의 유계 단편을 축 조합으로
   인코딩.
2. **Situation calculus 단편**(McCarthy-Hayes/Reiter): fluent/action/situation을
   vessel/action/zone으로 — 게임 세계 묘사와 가장 가까운 논리.
3. 인가 논리 단편(ABLP says-계열)은 authority 축 단독으론 이미 docs/19 계보.
**현황: 첫 단편 기계화 landed (2026-07-05)** — `BasisCompleteness.v`(coqc
PASS, admit/axiom 0): bigraph **정적** 단편(place forest + link) 대비 완전성
(encode_wf/parent/link) + 보존성(decode_encode 전단사 — 공간 축이 place graph
초과분을 안 가짐) + **world_separation**(채널-프리 연결은 world root 보존 =
"크로스월드는 채널-only"의 기계증명 그림자, AC-3/T의 정적 얼굴). 잔여: bigraph
*reaction*(동역학) ↔ intent step 대응 = 다음 rung. CI(formal-semantics-smoke)
등록은 실행 세션의 one-liner로 남김(공유 게이트 파일이라 동시세션 충돌 회피).

### M3 — 수렴 삼각측량 (Church-Turing 구조)
독립 전통들이 같은 범주에 도달했음을 문서화. 이미 5개 독립 수렴점이 실존:
- **게임**(BDFL의 휴리스틱 출처): 수십 년 "세계를 효율 묘사하라" 선택압의 수렴.
- **DDD**(Evans): 엔터프라이즈에서 같은 압력 — entity/aggregate/bounded context.
- **BDI/MAS**(AI): belief-desire-**intention**, agent, role, institution.
- **상위 온톨로지**(철학/정보과학): UFO의 Kind/**Role**/**Phase**(=vessel),
  DOLCE/BFO의 endurant/perdurant/agent.
- **양상논리/PL**: Kripke **world**, ML5의 위치 타입, ambient의 공간.

게임은 여기서 임의 표집이 아니다. 게임은 "세계를 효율적으로 묘사하라"는
생존 압력을 수십 년 동안 받은 거의 유일한 소프트웨어 장르다. Pergyra의
`world`/`zone`/`role`/`party`/`intent` 계열 어휘는 이 선택압이 수렴시킨
세계-묘사 어휘를 채굴한 것이다. 같은 명사군에 AI의 BDI, Searle식 제도/맥락
철학, UFO/OntoUML 온톨로지, Kripke 가능세계 논리가 독립적으로 도달했다는
사실이 삼각측량 증거다. 임의 휴리스틱이었다면 이 수렴은 설명되지 않는다.

휴리스틱 표집이 임의였다면 이 수렴은 설명 불가 — 이게 "자연 종(natural kind)"
논증의 표준 구조다(증명 아님, thesis. Church-Turing과 동형).
**현황: 삼각측량 문서 landed (2026-07-05)** — docs/semantics/21(5전통 × 9범주
대응표, 전 행 3★+ 수렴, 최약 행 world·channel-온톨로지 셀 정직 표기, 독립성
보수 판정 3~4-독립, 게임-편향 §4).
**정직한 편향 경고**: 게임 어휘는 이산적·시뮬레이션 가능 세계에 편향. 금융/세무
/공장(BDFL 본업) 같은 연속·규제 도메인이 다른 명사를 요구하면 기저 수정 신호다
— 범주 스트레스 테스트(docs/semantics/21 §4)에 **비게임 도메인 1개 필수**
(공장 장비 SW가 이상적 2번째 점).

### M4 — 보존적 소거 (fail-safe 베팅) ✅
어휘가 틀려도 코어가 안 깨진다: 축은 검사 후 소거되고(air_erasure 하드게이트,
provable=0), 도메인 층은 시스템 코어의 보존적 확장. 즉 기저 선택 오류의 비용은
**표현력 손해로 유계**되지 건전성 손해가 아니다. **현황: done**(게이트 가동).

### M5 — 경험 반증 (dogfood 지표) — ★DESCOPED
**BDFL 결정(2026-07-05): descope.** "이 조합이 맞냐 아니냐"는 경험 지표로도
증명 불가능한 계열이라는 판정 — 동의(지표는 애초에 옳음의 증명이 아니라 반증
신호였고, 그 반증조차 임계값 선택이 자의적). 잔존물 하나만 남긴다: docs/
semantics/21 §4의 **범주 스트레스 테스트**(비게임 도메인에서 새 1급 명사가
필요해지는가)는 지표 측정이 아니라 M3의 편향 보정이므로 유지.

---

## 3. 조합론 반격 — 곱공간 논증

비판의 형식화: 범용 언어는 작은 직교 코어 → 항 수준 자유 조합으로 무한 생성
(Scheme 서문의 "피처를 쌓지 말라"가 이 입장의 정본). Pergyra는 도메인 명사를
고정했으니 생성력이 죽는다는 것.

반격: 축이 **직교**하면 어휘 집합은 점이 아니라 **곱공간의 기저**다.
subject×zone×intent×authority×lifecycle×effect의 자유 조합이 도메인 기술 공간을
생성한다 — A-15의 15쌍 조합 매트릭스와 AC-3 정리 T가 바로 그 곱공간의 안전
조합론이다. 즉 Pergyra는 항 수준 조합론(함수/타입 — 그대로 보유) **위에** 축
수준 조합론을 얹은 것. 생성력 상실이 실재하는 유일한 조건은 축의 비직교/종속
— 그래서 직교성 기계증명(AxisOwnership.v)이 이 언어에서 장식이 아니라
**조합론적 생존 조건**이다.

잔차(정직): 곱공간이 생성적이어도 **축의 개수와 이름 자체**가 옳다는 보장은
없다 — 그건 M2(완전성)+M3(수렴)의 몫(M5 반증은 descope — §2). 두 무브가 다
실패하면 기저를 고쳐야 하고, M4가 그 수정 비용을 유계로 만든다.

---

## 4. 논문 지도 — 델타만 (docs/19·170·171 미수록분)

**전제 교정용 (keyword가 선례 없다는 오해 반박):**
- intent: Cohen & Levesque, "Intention Is Choice with Commitment" (AIJ 1990);
  Rao & Georgeff, "Modeling Rational Agents within a BDI-Architecture" (KR'91);
  Rao, "AgentSpeak(L)" (MAAMAW'96 — intention이 1급인 실행 언어); Shoham,
  "Agent-Oriented Programming" (AIJ 1993 — 정신 상태를 언어 primitive로);
  철학 뿌리 Bratman, *Intention, Plans, and Practical Reason* (1987).
- world: Murphy-Crary-Harper, ML5 (LICS 2004; Murphy 박사논문 "Modal Types for
  Mobile Code" 2008 — **world가 타입에 있는 실존 언어**); Kripke 가능세계 의미론.
- vessel/lifecycle: Aldrich et al., "Typestate-Oriented Programming" (Onward!
  2009, Plaid — 상태가 1급인 언어; Strom-Yemini는 docs/19에 이미 있음).
- zone-상대 권한: Searle, *The Construction of Social Reality* (1995 —
  "X counts as Y **in context C**" 구성 규칙 = zone-상대 의미의 철학 정본);
  Jones & Sergot, "A Formal Characterisation of Institutionalised Power" (1996,
  counts-as 조건문의 형식화).
- role: Guizzardi, *Ontological Foundations for Structural Conceptual Models*
  (2005, UFO/OntoUML — Kind/Role/Phase 형식 온톨로지. **도메인 묘사 어휘의
  이론에 가장 근접한 기존물**); Boella & van der Torre (roles in MAS).

**방법론용 (5무브의 도구):**
- M1: Felleisen, "On the Expressive Power of Programming Languages" (Sci. Comp.
  Prog. 1991) — macro-expressibility, primitive 자격 판정 도구.
- M2: Codd, "Relational Completeness of Data Base Sublanguages" (1972) —
  플레이북 원형; Milner, *The Space and Motion of Communicating Agents* (2009,
  bigraphs — place+link 참조 프레임 후보); Reiter, *Knowledge in Action* (2001,
  situation calculus 정본).
- M3: 상위 온톨로지(위 UFO/DOLCE/BFO) + Landin, "The Next 700 Programming
  Languages" (1966 — 기저+설탕 프레임의 원조); Van Roy & Haridi, *CTM* (2004,
  kernel language 방법론).
- 정당화 구조 전반: Böhm-Jacopini (1966).

---

## 5. WO 등록 (이론 트랙 — 전부 문서/증명 작업, 구현 0)

- **WO-BASIS-1** (M3) — ✅ landed 2026-07-05: docs/semantics/21.
- **WO-BASIS-2** (M1) — ✅ 논증 landed 2026-07-05: docs/semantics/22.
  잔여 후속 2개(문서의 낮은 행): authority 고유 의무 지목(Authority
  DelegationCore.v 착지점), intent 정적 충돌 분석(docs/167 B축과 동일 작업).
- **WO-BASIS-3** (M2) — ✅ 첫 단편 landed 2026-07-05: BasisCompleteness.v
  (coqc PASS). 잔여 rung: ① CI 등록(formal_semantics_smoke.sh one-liner),
  ② bigraph reaction ↔ intent step(동역학 대응 — 진짜 연구, post-self-host).
- **WO-BASIS-4** (M5) — ✂ descoped(BDFL 2026-07-05). 범주 스트레스 테스트만
  M3 §4로 이관.
- 시퀀스 권고: BASIS-1(저술, 지금 가능) → BASIS-4(지표 정의만 선행) →
  BASIS-2 → BASIS-3(post-M2-selfhost, 진짜 연구).

## Related

docs/semantics/19(개별 primitive 정당화 + core calculus 정본 — 이 문서는 그
위의 "집합" 층) · docs/170·171(계보/참조 지도 — 중복 0 확인 2026-07-05) ·
docs/42+AxisOwnership.v(직교성 = §3의 생존 조건) · A-15/AC-3(축 조합론 실물) ·
tests/air_erasure(M4 게이트) · project_decide_vs_declare_rice(선언이 어휘를
강제하는 이유 — 이 문서의 존재 전제) · project_killer_usecase(M5 실험장).
