# 167. 네트워크 이론 적용 지도 — 선언된 위상 위의 분석

Status: `design-blueprint` (★구현 금지 — 실행은 별도 세션/에이전트 몫. 이 문서는
그 세션이 나 없이 착수할 수 있게 hook·알고리즘·게이트·반증 기준까지 고정한다).
작성 2026-07-05. 상위: docs/19 §✦(판정이 아니라 선언), docs/140(semantic
squiggle), docs/146(SEA lanes), docs/15(capability sandbox), docs/160(M2 포팅
사다리), docs/162(T4 divergence 측정).

용어 정직성: 이 문서에서 "네트워크 이론"은 **그래프 알고리즘**(도달 폐포/연결
성분/채색/critical path)과 **네트워크 과학**(모듈러리티 커뮤니티/중심성/
percolation)을 함께 가리킨다. 값의 다수는 전자에서 나온다. 후자가 진짜 쓰이는
곳은 A·D축뿐 — 과대포장하지 않는다.

---

## 0. 한 장 요약

- 보통 언어에서 도메인 위상(누가-무엇에-참여하나, 권한이-어디까지-가나)은 Rice
  정리에 막혀 **복원 불가능**하다. Pergyra는 subject/intent/zone/world/channel/
  capability를 **구문으로 선언**하므로 그 위상이 컴파일 타임 아티팩트로 **공짜로
  생긴다**. 네트워크 이론은 그 수확물 위의 도구상자다 — 즉 이 질문 자체가
  thesis(선언으로 결정가능화)의 배당금이다.
- 입력 그래프는 **이미 직렬화돼 있다**(§2 실측): `pgy.air.graph.v1`이
  intents/boundaries/evidence/drifts/effects_by_op/slots를 JSON으로 방출.
  분석기는 컴파일러 무변경으로 이 JSON만 소비해도 시작 가능.
- 적용 축 5개: **(A)** 도메인 위상 진단(bounded-context advisory — thesis
  flagship), **(B)** lane 추론 = 충돌그래프 성분/채색(SEA 증거 공급), **(C)**
  capability blast radius(sandbox 킬러비전의 정량 얼굴), **(D)** 컴파일러
  자기관리(포팅 슬라이스·게이트 배치 — 지금 당장 유용), **(E)** channel/world
  흐름(vision-gated).
- 반-slop 규율(§4): 게이트/advisory/WO-결정에 연결 안 되는 메트릭 금지, 기술
  통계(scale-free 등) 거절, 축마다 반증 기준 선기록.

---

## 1. 왜 성립하는가 — 선언이 위상을 만든다

docs/19 §✦의 계산이론 프레임 그대로: 일반 코드에서 "이 두 연산이 같은 도메인
자원을 다투는가"는 결정 불가(Rice-손실)라 분석기가 근사·휴리스틱으로 후퇴한다.
Pergyra는 그 사실을 **선언**시킨다 — intent가 subject 목록을 문법으로 들고,
zone이 경계를 문법으로 들고, capability가 `with caps`로 문법에 있다. 그래서
도메인 그래프의 노드·엣지가 파서/의미분석 수준에서 **결정가능한 사실**이다.

실물 증거 (2026-07-05, 이 세션): intent 런타임 레지스트리의 parent_handle
체인에서 multi-node 사이클이 livelock(DoS)을 만들 수 있었다(F1, `fe70f180`으로
depth-bound 착지). 이건 "intent parent 그래프는 forest다"라는 **위상 불변식**이
1급으로 선언·검사됐다면 클래스째 막혔을 버그다. 네트워크 프레임의 첫 효용은
이런 불변식(forest-ness, acyclicity, reachability 상한)을 이름 붙여 게이트로
만드는 것이다.

---

## 2. 이미 있는 그래프 자산 (실측 인벤토리)

| 자산 | 위치 | 노드/엣지 | 상태 |
|---|---|---|---|
| AIR 그래프 | `src/compiler/air_dump_json.c`, schema `pgy.air.graph.v1` | intents · boundaries · evidence(+provider/subject/boundary 참조) · drifts | **직렬화 됨** — 이 세션에서 `pgy --air-json` 실출력 확인 |
| type-resolution DAG | dag_metadata evidence (AIR에 fact_count로 노출) | 선언 의존 | 존재, JSON엔 요약만 |
| capability call graph | `src/semantic/capability_analyze*` | 함수 노드, cap-mask 전파 엣지 | interproc 전파 **구현됨**(정적 call graph에 sound, 잔차=동적 디스패치/FFI) |
| intent parent forest | 런타임 레지스트리 `parent_handle` | intent 트리 | F1 depth-bound 착지(2026-07-05) |
| subject 겹침 | `pgy_intent_subject_fingerprint_export` (64-bit bloom) | intent×subject | **런타임 쌍별** 검사만 — 정적 승격 여지(B축) |
| ExecutionLaneFact | SEA IR (docs/146) | lane 배정 사실 | populate/emit+게이트 landed, 증거 공급원이 비어 있음 |
| MIR CFG | cfg-body-dataflow 게이트 계열 | 블록/엣지 | 존재 |

**함의: 분석 substrate를 새로 짓는 축은 없다.** 전부 기존 그래프의 소비다.

---

## 3. 적용 축

### A축 — 도메인 위상 진단 (thesis flagship)

- **그래프**: Subject–Intent 이분그래프 + zone 소속. AIR JSON에서 투영(컴파일러
  무변경 가능).
- **도구**: 모듈러리티 커뮤니티 검출(Leiden — 스펙트럴 불요), 참여 degree/
  betweenness 중심성.
- **산출**: `DIAG_ADVISORY` 신규 생산자 2종 —
  ① **bounded-context 이탈**: 한 zone/world 안 subject들이 2개 이상 약결합
  커뮤니티로 갈라짐 → "이 zone은 사실상 두 도메인이다" advisory.
  ② **god-subject**: intent 참여 degree·betweenness 상위 outlier → DDD
  god-object의 도메인-축 판.
- **Hook**: docs/140의 4색 advisory 채널이 **이미 배관돼 있다**(제3상태+첫
  생산자+LSP+VS Code landed). 그 문서의 병목 진단("배관이 아니라 어떤 drift를
  advisory로 승격하나의 언어 결정")에 대한 답이 정확히 이 두 smell.
- **게이트**: advisory golden fixture (기존 semantic-squiggle 게이트 계보).
- **정직 판정**: 경쟁 언어가 **원리적으로 못 주는** 진단(선언이 없어 그래프가
  없다) — 유일한 thesis-전용 축. 단 smell 임계값의 유용성은 던전크롤러 규모
  실코퍼스에서만 검증 가능. 그 전 구현은 사변 → **post-dogfood**.

### B축 — lane 추론 = 충돌그래프 성분/채색 (SEA 증거 공급)

- **그래프**: intent 노드, 엣지 = 정적으로 알 수 있는 subject 겹침(런타임
  fingerprint 검사의 정적 승격) ∪ 선언된 순서 제약.
- **도구** (정확한 대응 — 채색 하나로 뭉개지 말 것):
  - **연결 성분** = lane 분해: 충돌 쌍은 같은 lane(직렬)이어야 하므로, 안전한
    최대 lane 수 = 충돌그래프의 성분 수.
  - **채색** = wave 스케줄: 색 클래스(독립집합) = 동시 실행 가능한 배치, 색
    수 = 직렬 wave 깊이. χ는 NP-hard — greedy/DSATUR 상한 + clique 하한만
    보고(정직).
  - **work/span**(intent DAG critical path) = 이론 speedup 상한 → AIR fact
    `parallel_span` 후보.
- **산출**: ExecutionLaneFact의 **정적 증거 생산자**. lane은 증거로 결정된다는
  SEA 계약(docs/146)의 비어 있던 공급원을 채움.
- **게이트**: 기존 SEA 게이트 + golden(zone→PinnedZone 계보) 확장.
- **정직 판정**: 이론적 novelty 낮음(고전 그래프 알고리즘) — 값은 런타임 검사의
  정적 승격이라는 **엔지니어링 정합**. 성능 클레임은 실측 전 금지(docs/142
  amortized-cost 포지션 유지).
- **반증 기준**: 정적 충돌그래프가 무충돌이라 판정한 쌍이 런타임 fingerprint
  충돌을 실측으로 내면 모델 위상이 틀린 것 → RED, 승격 중단.

### C축 — capability blast radius (sandbox 킬러비전의 정량 얼굴)

- **그래프**: authority 그래프 — 노드 = 함수/zone 경계, 엣지 = cap-carrying
  호출/전파(capability_analyze의 전파 사실 **재사용**, 신규 분석 아님).
- **도구**: per-capability 도달 폐포(O(V+E)) = **blast radius**(이 cap이 새면
  도달 가능한 함수/zone 수), 금지-경로 질의(confused-deputy: 이 경로로는 이
  권한이 도달하면 안 된다 — authz-logic 계보). percolation 프레임(몇 개 grant
  부터 사실상 전역 도달인가)은 **연구 딱지** — 폐포+임계 관찰로 충분하면 걷어냄.
- **산출**: `--capability-manifest` 확장 — capability별 도달 수치 + **잔차
  명기**(동적 디스패치/FFI는 정적 폐포 밖, 런타임 게이트가 backstop — docs/15의
  기존 한계 서술을 매니페스트에 그대로 실을 것). W-4 서명 로더(docs/161)의
  심사 자료 = "그래픽 아닌 신뢰로 승부"의 숫자.
- **게이트**: manifest golden + (선택) blast-radius 단조 ratchet.
- **정직 판정**: **가장 저비용-고신뢰 축.** 전파 그래프가 이미 있고 폐포는
  자명한 알고리즘. 네트워크 "과학"이라기보다 도달성 — 그러나 사용자-표면
  가치(신뢰 수치)는 5축 중 최상위.

### D축 — 컴파일러 자기관리 (지금 당장, M2 보조)

- **그래프**: ① C 소스 include/호출 그래프(325k LOC), ② MIR fact × 소비자
  이분그래프(docs/162 T4-0의 측정 대상 그 자체).
- **도구**:
  - 커뮤니티 검출로 **SEM-3..11 포팅 rung 경계 검증**(docs/160): rung 경계가
    모듈러리티 절단선과 어긋나면 포팅 중 절단면 출혈 예고 → rung 재조정 근거.
  - min-cut = 포팅 슬라이스 순서(경계 churn 최소화).
  - 매개 중심성 = **게이트 배치 우선순위**: 중심성 높은 파일의 회귀가 광역
    파급. 실례(이 세션): `transpiler_type_name_utils.c` 렌더 owner 하나가
    codegen 전역에 파급 — 중심성 상위였을 파일이다.
  - 부수 검증: docs/160 STEP 0의 "typed AST가 linchpin" 주장을 self-host 의존
    그래프 중심성으로 **실측 반증 가능**하게(linchpin인데 중심성이 낮으면 주장
    재검토).
- **산출**: 순수 텍스트 census 스크립트(컴파일러 무변경) + rung 검증 리포트 +
  T4-0 계기판 합류.
- **정직 판정**: **가장 싸고 즉시 유용**(M2 진행 중인 지금). 이건 네트워크
  과학의 모듈러리티를 실제로 쓰는 축이나, 본질은 리팩터링 보조 — 언어 기능
  아님. 1회성 분석이라 게이트 없음이 정당(T4-0 스모크와 합류는 가능).

### E축 — channel/world 흐름 (vision-gated)

- **그래프**: world 노드, channel 엣지(크로스-world는 channel-only가 이미 설계
  결정), wait-for 그래프.
- **도구**: wait-for 사이클 검출 = 데드락 후보 advisory(session-type 계보,
  theoretical_foundations의 기존 대응 활용), R6 예산을 엣지 용량으로 한
  flow/min-cut = 파티션 간 처리량 상한.
- **정직 판정**: 분산은 **vision**(capability-overclaim 규율 — 현재 capability로
  인용 금지). 근미래 후보는 **단일 프로세스 channel 데드락 advisory 하나**뿐.
  flow/min-cut은 분산이 실물이 되기 전엔 서랍에.

---

## 4. 반-slop 가드 (거절 목록 + 원칙)

- **거절**: scale-free/small-world 기술통계(서술만 있고 행동 없음), PageRank
  장식(중심성으로 충분), 스펙트럴 과잉(Leiden으로 충분), 게이트 없는 계기판.
- **원칙**: 모든 메트릭은 (a) 게이트를 잠그거나 (b) advisory를 방출하거나
  (c) WO 결정을 바꿔야 한다. 셋 다 아니면 폐기.
- **반증 선기록**(machine-neutral full-cycle 계보): 각 축 착수 시 "이 분석이
  틀렸다면 보일 신호"를 먼저 적는다. B축은 §3에 기록했고, C축은 "정적 폐포
  밖에서 런타임 cap-deny가 발화하면 잔차 목록이 불완전한 것", D축은 "커뮤니티
  경계대로 포팅했는데 절단면 diff가 더 커지면 모델이 틀린 것".
- **3-pair**: 이 문서 전체가 설계도다 — 어느 축도 현재 capability로 인용 금지.

---

## 5. 시퀀싱 + WO 등록

권고 순서: **D(지금 — M2 보조, 순수 텍스트) → C(post-M2 첫 언어 축, 최저비용
최고신뢰) → B(SEA 증거 공급) → A(post-dogfood flagship) → E(vision, 데드락
advisory만 근미래)**.

- **WO-N1** — 소스 위상 census + SEM rung 모듈러리티 검증 + linchpin 중심성
  실측 (D). 순수 텍스트, 컴파일러 무변경.
- **WO-N2** — capability blast-radius manifest 확장 + 잔차 명기 (C).
- **WO-N3** — intent 충돌그래프 정적 성분/채색 → ExecutionLaneFact 증거 공급 (B).
- **WO-N4** — 도메인 위상 advisory 1호: god-subject (A, post-dogfood).
- **WO-N5** — 단일 프로세스 channel wait-for 사이클 advisory (E의 근미래 절편).

보드(TODO ★★) 등록은 이 문서에 안 했다 — 실행 세션이 착수 시점에 등록하는 게
보드 규율에 맞다.

## Related

docs/19 §✦(선언으로 결정가능화 — 이 문서의 존재 근거) · `pgy.air.graph.v1`
(`src/compiler/air_dump_json.c`) · docs/140(advisory 배관 + "어떤 smell을
승격하나" 병목) · docs/146(SEA lane = 증거-gated) · docs/15+161(capability
sandbox / W-4 서명 로더) · docs/160(SEM rung — D축의 검증 대상) · docs/162
(T4-0 divergence 측정 — D축 합류점) · docs/142(amortized-cost — 성능 클레임
규율) · F1 커밋 `fe70f180`(위상 불변식 부재의 실물 비용) ·
project_theoretical_foundations(session/authz-logic 대응).
