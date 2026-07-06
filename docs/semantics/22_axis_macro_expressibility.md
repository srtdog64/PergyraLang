# 22. 축 비표현성 논증 — 라이브러리로 내리면 무엇이 깨지나

Status: `theory-argument` (M1 of docs/172). 작성 2026-07-05. **논증이지 정리가
아니다** — Felleisen의 macro-expressibility 프레임(1991)을 차용하되, 원 프레임은
동적 관찰 기반이고 우리의 구별 관찰은 정적(컴파일러 수용/거절)이므로 "프레임의
정신을 따르는 산문 논증"으로 정확히 표기한다. 기계화하려면 축별 구별 프로그램
쌍을 형식화해야 하며 그건 후속 rung.

## 0. 주장의 정확한 형태 (host-상대성)

Felleisen 표현성은 **호스트 상대적**이다. 우리의 주장:

> Pergyra의 도메인 축(capability/effect, slot own-ref, lifecycle, zone/world,
> authority, intent)은 **Pergyra 자신의 코어**(C#-계열: 값/함수/클래스/제네릭/
> Result — 의존 타입 없음, 매크로 없음, 타입계 플러그인 없음) 위에서 국소
> 매크로로 제거 불가능하다.

Agda/F*/Idris 같은 의존-타입 호스트나 Racket 같은 매크로 호스트에 대한 주장이
**아니다** — 거기선 다수 축이 임베딩 가능하다(그게 바로 "왜 라이브러리가
아니라 언어인가"의 답이 host 선택과 결부되는 이유다).

**구별 관찰(distinguishing observation)**: 잘못된 프로그램에 대한 **컴파일러의
거절**. 라이브러리 인코딩은 정적 의무를 런타임 실패로 미룰 수밖에 없고(코어에
타입계 훅이 없으므로), "컴파일 시점 거절 vs 런타임 실패"는 관찰 가능한 차이다.
따라서 각 축에 대해 보일 것은 하나다: **오늘 실제로 컴파일 거절을 일으키는
정적 의무**의 존재와, 그 의무가 국소 재작성으로 재현 불가능한 이유.

## 1. 축별 논증

각 절: (a) 라이브러리 강등 시나리오 → (b) 오늘 실존하는 정적 의무(파일/게이트)
→ (c) 왜 국소 매크로가 그 의무를 재현 못 하는가 → (d) 정직한 약점.

### 1.1 capability / effect — 가장 강한 논증

- (a) 강등: `Cap.require(FILE_WRITE, () => ...)` 런타임 게이트 라이브러리.
- (b) 정적 의무: 함수 단위 `declared ⊇ used`를 **의미 에러**로 강제. used는
  interprocedural 전파로 계산(`src/semantic/capability_analyze*` + type_checker,
  `make test-capability`; entry가 선언 안 한 CLOCK을 helper 경유로 쓰면 거절되는
  interproc fixture 실증). 정적 manifest(`--capability-manifest`)와 AIR
  declared⊇used 증명.
- (c) 국소 매크로는 **호출 그래프를 모른다**. `used`는 전역(interprocedural)
  사실이고, 매크로 전개는 정의상 국소다. 코어에 effect-row도 타입계 훅도 없으니
  타입 인코딩 경로도 없다. 라이브러리 버전의 관찰: 잘못된 프로그램이 컴파일되고
  런타임에 죽는다 — 구별 관찰 성립.
- (d) 약점: 없음에 가깝다. 이 축이 M1의 anchor.

### 1.2 slot / own-ref — 강함

- (a) 강등: `Slot<T>` 클래스 + 런타임 상태 태그.
- (b) 정적 의무: **정적 UAF가 interprocedural로 완전**(own/ref 명시 강제 +
  release 추적 — memory `project_slot_safety_consistency`), released 슬롯
  Move/Read 컴파일 거절(`type_checker_ownership_let.c`의 SLOT_STATE 추적,
  `PGY_CODE_SEM_MOVE_FROM_RELEASED` 등), pinned-view 충돌 거절.
- (c) 소유권 이동은 **선형성**이다 — 코어 타입계에 선형/affine 자원이 없으므로
  라이브러리는 "이동 후 원본 사용"을 컴파일 시점에 거절할 수 없다(런타임 태그가
  최선). Rust가 이걸 라이브러리가 아니라 언어로 만든 것과 같은 이유.
- (d) 약점: 없음에 가깝다. (borrow-checker 동급 주장은 금지 — 주장은 "우리
  코어 위 비표현성"뿐이다.)

### 1.3 lifecycle / vessel — 강함

- (a) 강등: 상태 enum + 매 연산 `if (state != VALID) throw`.
- (b) 정적 의무: taint 분석 + valid-from mask가 도메인-무효 연산을 정적으로
  잡고(semantic taint pass), 잔차는 양 백엔드 fail-closed 가드(런타임 state
  tag). 정적으로 증명되는 경로는 가드가 소거된다(air_erasure의 lifecycle 행:
  provable 픽스처 = 0 runtime call).
- (c) typestate는 **흐름-민감 타입**이다 — 코어에 흐름-민감성이 없으니
  라이브러리는 "Empty 상태 vessel에 Pour" 를 컴파일 거절 못 한다. 게다가
  라이브러리 버전은 소거 불가(모든 호출에 태그 검사 상존) — 거절 관찰에 더해
  **소거 관찰**로도 구별된다.
- (d) 약점: 정적 커버리지가 taint 완전성에 의존 — 논증은 유효하나 "정적 의무의
  면적"은 slot보다 얇다.

### 1.4 zone / world — 강함 (이번 반기 보강됨)

- (b) 정적 의무: AC-3 정리 T — 격리-경계 넘는 live binding은 Clone/Channel
  선언 없으면 **REJECT**(`semantic_reject_zone_subject_embedding` S1 +
  `semantic_reject_world_zone_member_escape` S2, docs/157; corpus 28사이트가
  실제로 거절돼 Clone 전환됨). zone authority requires(능력 요구) 정적 검사.
  기계화 대응: `BasisCompleteness.v`의 `world_separation`(크로스월드=채널-only의
  정적 그림자) + `ZoneCrossingCore.v`.
- (c) 경계는 **구문적 스코프 사실**이다 — 라이브러리는 자기 호출자가 어느
  zone/world 블록 안에 있는지 볼 수 없다(코어에 리플렉션/매크로 없음). "경계를
  넘는 참조"라는 판단 자체가 국소 전개에 주어지지 않는 정보다.
- (d) 약점: zone의 *스케줄링* 의미(PinnedZone 등)는 런타임 성분이 큼 — 논증은
  격리/거절 의무에 한정.

### 1.5 authority — 중간

- (b) 정적 의무: `authority X requires Ability` 정적 검사, witness-guard 결합
  (GuardWitnessBinding.v), capability와의 결합(1.1로 부분 환원).
- (c) 요구-능력 검사는 선언 위치(zone 멤버십)에 결부 — 1.4와 같은 구문-사실
  논거.
- (d) **정직한 약점**: authority의 상당 부분이 capability(1.1)와 zone(1.4)의
  합성으로 설명될 수 있다 — 즉 이 축의 *독립* 비표현성 논증은 나머지 두 축보다
  약하고, "authority = cap × zone 위의 표기"라는 반론이 가능하다. 반박하려면
  둘로 환원 안 되는 authority 고유 정적 의무(위임 체인 등 —
  AuthorityDelegationCore.v가 후보)를 지목해야 한다. **미완**.

### 1.6 intent — 가장 약함 (명시)

- (b) 현재 정적 의무: subject-충돌 검사와 parent 추적은 **런타임**이다(이 세션
  F1에서 그 런타임 코드를 고쳤다). 정적 쪽은 AIR intent-topology fact 방출
  뿐 — 방출은 거절이 아니다.
- (c→d) 따라서 오늘 기준 intent의 비표현성 논증은 **성립이 얇다**: 런타임
  레지스트리 라이브러리로 상당 부분 재현 가능하다는 반론을 현재는 못 꺾는다.
  솔직한 상태: intent가 primitive인 근거는 지금은 표현성이 아니라 (i) AIR
  검증·관측 표면과 (ii) 향후 정적 충돌 분석의 자리라는 것. **보강 경로가 이미
  설계돼 있다**: docs/167 B축(충돌그래프 정적 성분/채색 → ExecutionLaneFact
  증거)이 착지하면 intent도 "정적 거절/정적 스케줄 사실"을 갖게 되어 이 논증이
  1.1급으로 승격된다. 그 전까지 이 절은 **부분-미완으로 정직하게 남긴다**.

## 2. 요약 판정표

| 축 | 비표현성 논증 | 근거 강도 |
|---|---|---|
| capability/effect | 성립 | ★★★ (interproc 정적 거절 실증) |
| slot/own-ref | 성립 | ★★★ (선형성 — 코어에 부재) |
| lifecycle/vessel | 성립 | ★★☆ (+소거 관찰) |
| zone/world | 성립 | ★★☆ (AC-3 거절 + 기계화 그림자) |
| authority | 부분 | ★☆☆ (cap×zone 환원 반론 미해소) |
| intent | 얇음 | ☆☆☆ (정적 의무 부족 — docs/167 B축이 보강 경로) |

이 표의 낮은 행들은 숨길 것이 아니라 **작업 지시**다: authority 고유 의무 지목,
intent 정적 충돌 분석. 둘 다 기존 설계(AuthorityDelegationCore.v, docs/167)에
착지 지점이 있다.

## Related

docs/172(M1 자리) · Felleisen 1991(프레임 출처 — 차용이지 정리 아님) ·
docs/semantics/21(M3) · BasisCompleteness.v(M2) · AxisOwnership.v(직교성 —
비표현성과 상보: 직교성은 "서로 안 겹침", 비표현성은 "코어로 안 내려감") ·
docs/157(AC-3) · docs/167 B축(intent 보강 경로) · memory
`project_slot_safety_consistency`, `project_capability_sandbox_vision`.
