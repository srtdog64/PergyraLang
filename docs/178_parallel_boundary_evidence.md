# 178. 병렬 경계 = 증거 문제 — 한 규율, 세 지향의 사영

Status: `partially-landed` (F2 Exclusivity `377bb524` + WO-DOP-1 rung 0
`c994f39b`/`9bf946d7` 착지 2026-07-08/09 — §5 착지 기록. §3의 병렬-for는
여전히 설계만). 작성 2026-07-06. 계기: BDFL — "우리는 변형 OOP + FP + DOP 다중 지향이니, 병렬(캡처/
경계) 정책이 한 지향의 답이면 안 된다." 상위: docs/177(F1/F2 실측), docs/113
(교리), docs/146(SEA — "lane은 증거로 결정"), docs/157(AC-3 정리 T),
IntentConflict.v(`sep_when_active`), project_core_module_layering(★패러다임은
축이 아니다 — 이 문서는 그 canon을 지킨다).

---

## 0. 한 장 요약

- **원칙**: 패러다임별 병렬 규칙 3벌을 만들지 않는다(축 아님 canon). 대신
  **경계 규율은 하나** — *"parallel 경계를 건너는 모든 접근은 증거가 필요하고,
  증거 없으면 거절"* — 이고, 세 지향은 그 규율 위의 **관용 사영(idiom
  projection)**이다. 이는 새 발명이 아니라 기존 canon의 확장이다: SEA가 "lane
  은 증거로 결정"이라 했고, AC-3 정리 T가 이미 격리-경계에 Clone/Channel
  증거를 요구한다. 이 문서는 그 증거 어휘를 병렬 경계에 대해 **완결**한다.
- **증거 4종**(§1): Copy(Clone) · Channel · Exclusivity(단일 작성자/intent
  exclusive) · **Disjointness(서로소 분할 — 신규)**.
- **지향 사영**(§2): FP→Copy/Channel, 변형 OOP→Exclusivity(intent 계보),
  DOP→Disjointness. 실측 커버리지: FP 거의 완비, OOP는 intent-수준만(문장-수준
  = F2), **DOP는 통로 부재**(분할 primitive·병렬-for 없음, 컬렉션 통짜 거절).
- **F2의 재정의**: 태스크 #2(스칼라 캡처 정책)는 이 분류의 문장-수준 집행이다
  — "다중 작성자 거절"= Exclusivity 증거 부재의 거절, "copy-in 기본"= 스칼라의
  기본 증거를 Copy로 지정하는 것.

## 1. 증거 4종 (경계 통과의 어휘)

| 증거 | 의미 | 검사 형태 | 기존 canon 대응 |
|---|---|---|---|
| **Copy** | 값 스냅샷이 건너감 — 원본과 절연 | Clone/copy-capture (클로저 Stage A와 동일 교리) | AC-3 T의 Clone leg |
| **Channel** | 소유권/데이터가 프로토콜로 이동 | 스레드-안전 런타임 (실측 green) | AC-3 T의 Channel leg, 크로스-World 유일 통로 |
| **Exclusivity** | 공유하되 동시 접근이 배제됨 | 단일-작성자+join(구조적), intent `exclusive`/admission | IntentConflict.v `sep_when_active`(양보 조건들), 단일-arm 쓰기 p2 패턴 |
| **Disjointness** ★신규 | 공유하되 접근 영역이 서로소 | 분할 fact(slice split의 범위 비중첩) | `sep_when_active`의 subject-서로소 leg의 **데이터 판**; BasisCompleteness의 분리 정리 계보 |

무증거 접근 = 거절(fail-closed). F1의 무음 폴백 제거와 함께, "왜 이 접근이
허용되는가"가 항상 4증거 중 하나로 답해진다 — 관측 가능성(§1.1) 충족.

## 2. 세 지향의 사영 + 실측 커버리지

| 지향 | 관용 병렬 패턴 | 필요 증거 | 현 상태 (docs/177 실측) |
|---|---|---|---|
| **FP** | 불변 값 fan-out, 결과는 채널/Future로 | Copy + Channel | **완비(2026-07-09)** — spawn 인자 copy-only, 컬렉션 공유 거절, Channel/Slot green, 스칼라 reader-snapshot(§5) |
| **변형 OOP** (subject/vessel/intent) | 공유 개체를 배타 undertaking으로 | Exclusivity | intent 수준은 실물(admission/exclusive/priority + IntentConflict.v). **문장 수준이 구멍**(스칼라 포인터 공유 무가드 = F2) |
| **DOP** (데이터 지향) | 데이터 테이블을 서로소 구간으로 갈라 일괄 처리 (게임 ECS/SoA 배치) | Disjointness | **rung 0 개통(2026-07-09, §5)**: 분할 slice 쌍의 병렬 쓰기가 admission으로 열림. 잔여 = 병렬-for 표면(설계만), 2-분할 초과 chunk |

DOP가 가장 병렬-친화적 지향(그래서 게임 엔진이 ECS로 감)인데 우리 통로가
0이라는 게 이 감사의 설계-측 핵심 발견이다. 던전크롤러(킬러 유즈케이스)의
엔티티 배치 처리 = 정확히 이 관용구.

## 3. DOP 갭의 최소 채움 (WO-DOP-1, 설계만)

- **분할 fact**: `SplitAt(slice, i) -> (Slice, Slice)` 류 — 두 결과의 범위
  비중첩이 **구성상 보장**(rayon `split_at_mut` 계보). 컴파일러는 분할 산출물
  임을 fact로 알고, 경계 검사에서 Disjointness 증거로 인정.
- **표면**: 신규 구문 최소화 — 1차는 분할 산출물을 기존 parallel arm에 하나씩
  캡처(arm당 하나의 서로소 조각 = 증거 자명). 병렬-for(`parallel for chunk in
  Split(...)`)는 그 다음 rung(표면 결정 = BDFL).
- **거절 유지**: 분할 아닌 컬렉션 공유는 지금처럼 거절 — 이 문서는 거절을
  느슨하게 만드는 게 아니라 **증거 있는 통로 하나를 여는 것**.
- 순서: F2(#2) → F1(#1) 뒤. ExecutionLaneFact에 Disjointness가 데이터-병렬
  lane 증거로 합류(docs/146).

## 4. 반-확산 가드

- 패러다임 키워드/모드 스위치 금지 — "FP 모드/OOP 모드" 같은 표면은 이 문서가
  명시적으로 거절한다(core/module canon). 지향은 **증거 선택으로 표현**된다.
- 증거 5번째 후보(atomic 등)는 실수요(탈출구 실측) 전 추가 금지 — 4종이 세
  지향을 덮는다는 게 본 설계의 주장이고, 못 덮는 사례가 나오면 그게 반증.

## 5. 착지 기록 (2026-07-08/09)

- **Exclusivity 문장-수준(`377bb524`)**: 무증거 스칼라 write-race 거절 —
  §1 표의 Exclusivity leg가 문장 수준에서 실물이 됨(단일-작성자+join /
  전부-읽기 허용, 그 외 거절).
- **Disjointness rung 0(`c994f39b`, 전제 rung=slice 쓰기 표면 `9bf946d7`)**:
  §3의 1차 설계 그대로 — 신규 구문 0. 기존 `base.Slice(0, B)` /
  `base.Slice(B, LEN)` 쌍(불변 경계 B: Int 리터럴 or 불변 비-param 로컬)을
  구성-보장 분할로 인식해 parallel 캡처 admission. [0,B)∩[B,B+LEN)=∅가
  값-무관 정리라는 점이 핵심 — 분석이 아니라 선언(decide→declare canon).
  admission 조건: 같은 base의 캡처된 fact-slice가 정확히 lower/upper 한 쌍,
  경계 동일(심볼 identity or 리터럴 등가), 절반당 정확히 1개 arm 참조,
  base는 어떤 arm에도 미참조. 실패 시 기존 컬렉션 거절로 fail-closed.
  게이트: `parallel-disjoint-test-smoke`(admit=110 양 백엔드, negative 4종)
  + backend_compare `parallel_disjoint_split_write`.
- **비대칭 해소 기록**: spawn의 Channel 거절 vs parallel의 Channel 공유는
  같은 증거 규율의 두 경계 읽기다 — spawn=Copy 경계(inline-mutex Channel은
  복사 불가 → 거절), parallel=공유 경계(동일 객체 → Channel 증거로 안전).
  근인/미래 lever(opaque handle lowering)는 docs/177 §8.
- **Copy 문장-수준(`48689eec`, 2026-07-09)**: 단일-writer 프리미티브 스칼라의
  reader arm이 pre-parallel 스냅샷을 복사로 수령(writer는 배타 포인터 유지) —
  종전 read-write 거절이 결정론적 의미로 승격. §2 FP row의 마지막 잔여가
  닫혀 FP 사영 완비. write-write/비-프리미티브 read-write는 fail-closed 유지.
  writer 분석 단일 소유(`ast_statement_assigns_identifier`) — checker와 양
  백엔드가 같은 walk를 소비해 admission↔materialization drift 불가. 게이트:
  `parallel-snapshot-test-smoke` + backend_compare `parallel_snapshot_read`
  (채널-순서화 판별자: 포인터 의미=42/42 강제, 스냅샷=42/1).
- **bare-block arm(`39da6046`, 2026-07-09, docs/177 F3(a))**: 다중-문장 arm
  표면이 열려 §2의 관용구들이 실제로 작성 가능해짐. 목격자 =
  `parallel_pingpong_witness`(교대-강제 프로토콜, 직렬=deadlock=RED).
- **evidence lifetime으로 읽기**(docs/semantics/09 압축 예산의 인스턴스):
  Disjointness = 마지막 소비자가 semantic admission → **erase**(런타임 잔존
  0) · Copy(snapshot) = ctx 복사 후 원본과 절연 → **erase** · Channel =
  런타임 **retain**(동기화 상태 자체가 증거) · 무증거 공유 = **reject**.
  "Evidence-carrying compiler, not evidence-hoarding runtime."
- **캡스톤 `parallel_scheduler_showcase` (2026-07-11)**: 단일-기능 격리
  목격자들과 달리 **증거 4종 + select + spawn/await + for-in + 함수 호출을
  한 프로그램에 합성** — Linux 풍 스케줄러 모델(bounded runq 백프레셔,
  parity 마이그레이션, per-CPU vruntime을 disjoint 분할에, boot-epoch
  스냅샷, select ack drain, spawn 감사자와 이중 검증; nice→weight는
  sched_prio_to_weight[] 머리값). 5-arm/4-worker 데드락 분석 포함, 10회
  반복 결정성 실측, 양 백엔드 byte-equal. **착지 과정에서 실버그 2건
  적발**(합성 테스트의 가치 실증 — 기존 corpus는 전부 Main-내부·단일
  기능이라 못 밟던 조합): ① C select case guard가 채널을 raw AST 이름으로
  방출 → wrapper 캡처(`(*_pctx->ch)`) 미해석(모든 arm-내 select 불능),
  ② 비-Main 함수에서 채널 let의 **죽은 SSA 쌍둥이**(`_pgy_ssa_ch_1={0}`)에
  캡처 주소가 배선 → 미초기화 채널 무한 스핀. 둘 다 채널-lvalue canon
  (SSA-bypass)으로 교정 + try_recv의 "empty" 계약-결과 경고 소음 제거
  (비결정 stderr가 compare를 오염시키던 것).

## 6. 자기-신고: capture-disposition은 docs/180 §6의 이주 신호를 울린다

§5의 착지 형태는 per-(binding, arm) capture 처분(PTR/SNAPSHOT + admission
증거)을 **세 소비자가 각자 도출**한다: semantic 검사기, C 캡처 이미터, LLVM
캡처 이미터. 도출 함수 자체는 단일 소유(`ast_statement_assigns_identifier`,
AST 층)라 *drift*는 불가능하지만, docs/180 §6의 첫 이주 신호("two or more
consumers repeat the same derivation")와 backend-dumb-emitter 방향("backends
are progressively losing permission to reconstruct semantic facts")에
정면으로 걸린다 — 이것은 은닉하지 않고 선언해 둔다.

목표 형태(이주 시): semantic이 parallel 노드당 **capture-disposition fact
row**(binding × arm → PTR/SNAPSHOT/VALUE + 증거 종류)를 생산하고, 양 백엔드
이미터는 row를 소비만 하며, row 부재 시 fail-closed(재도출 금지). 새 owner가
실물이 되기 전에는 boundary_migration_manifest에 row를 만들 수 없으므로
(shadow는 owner 실존 요구), 이 절 + TODO 보드 WO가 Declare 단계를 대신한다.
그 시점까지의 정합성은 단일 도출 함수 + 판별 목격자(`parallel_snapshot_read`
— 처분이 갈라지면 42/42 or 발산으로 즉시 RED)가 지킨다.

**✅ MIR 소유 이주 완료 (2026-07-12, WO-PAR-1).** 착지 형태:

- **semantic owner**: 검사기는 stable parallel-boundary ID별
  `SemanticParallelCaptureBoundaryFact`를 만들고 admitted snapshot마다
  `{name, kind, writer_task}`를 기록한 뒤 봉인한다. AST에는 처분 행이나
  봉인 비트를 저장하지 않는다.
- **MIR projection**: `mir_lower`가 봉인·stable ID·task 수·행 종류·writer
  범위·중복을 fail-closed 검증하고 `MIRParallelCaptureBoundaryFact`로
  복사한다. MIR verifier와 MIR JSON이 같은 테이블을 소유한다.
- **소비**: C·LLVM 캡처 이미터는 AST node의 stable ID를 provenance key로만
  사용해 `mir_parallel_capture_boundary_find`를 호출한다. reader/writer arm
  판별은 MIR row의 `writer_task`만 소비하며 AST나 source text에서 처분을
  재도출하지 않는다.
- **fail-closed**: MIR row 부재, 미봉인, task 수 불일치, 잘못된 writer,
  중복 boundary/row는 lowering 또는 emission을 중단한다. LLVM이 admitted
  snapshot을 스칼라로 lower하지 못하는 기존 hard error도 유지한다.
- **검증**: MIR owner/mutation test, AST 호환 저장 재도입 금지 ratchet,
  MIR JSON 표면 확인, C·LLVM positive snapshot과 write-write/read-write
  negative fixtures를 `parallel-snapshot-test-smoke`가 함께 잠근다.
  Disjointness(Slice 허용)는 semantic 판정 소유이므로 별도 backend
  재도출 대상이 아니다.

## Related

docs/177(F1/F2 실측 + §8 copy-only 판정 — 본 문서가 F2의 상위 프레임) ·
docs/157+ZoneCrossingCore(Clone/Channel 증거의 정리) · IntentConflict.v
(Exclusivity/서로소의 기계화 — Disjointness는 그 데이터 판) · docs/146
(증거-기반 lane — 같은 문장의 데이터 확장) · docs/semantics/09(증거 압축
예산 — §5가 그 인스턴스) · project_core_module_layering(패러다임≠축 canon) ·
클로저 Stage A(copy 교리 선례) · rayon split_at_mut / ECS-SoA(DOP 계보) ·
태스크 #1/#2 · WO-A3(evidence-lifetime 커버리지 메타게이트, TODO 보드).
