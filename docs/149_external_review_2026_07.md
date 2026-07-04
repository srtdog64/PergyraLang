# 149. 외부 기술 리뷰 2호 판정 (2026-07-04)

Status: `judged, new-signals-registered`. docs/137(친구 레드팀, 2026-06)의
후속 외부 관측. 프로토콜 동일: **레포 실측 대조 → 정직 판정 → 신규 신호만
등록 — 등록 ≠ 닫힘.** 이 문서는 리뷰 요약이 아니라 판정 기록이다.

## 0. 메타 판정

정확도 **~95%** — docs/137의 ~90%를 상회. 리뷰어가 README/docs/146/142/
PROGRESS를 실제로 읽었고, 주장 4건을 스팟 검증한 결과 전부 레포와 일치:

| 리뷰 주장 | 실측 | 판정 |
|---|---|---|
| source-kind/boundary-kind pin 추측이 lane 분류에서 제거됨 | docs/146:3·126·165 — `air_boundary_source_kind` 참여 금지 명문 | ✓ |
| codegen gen2==gen3 fixpoint | PROGRESS:161 — 그리고 리뷰보다 큼: **22/22 self-host 컴포넌트 전체가 Pergyra-작성 codegen으로 자기-빌드**, `codegen_bootstrap.sh` 게이트 | ✓+ |
| Worker/Blocking/LocalAsync/Movable이 단일 worker-thread executor 공유 | docs/146:113·202 | ✓ |
| P0 = BoundaryCaptureFact 정밀도 | docs/146:28 — 계약 명칭까지 일치. 리뷰의 P0 = 우리 문서의 자기-선언 frontier | ✓ |

가장 건강한 신호는 docs/137 때와 같다: **외부 관측의 P0가 내부 보드의
frontier와 수렴한다** — 리스크 모델이 옳다는 재확인.

## 1. 이미 게이트로 닫혀 있는 것 (리뷰가 옳고, 우리가 이미 물었다)

- **AIR verification-only** — 리뷰의 "backend가 AIR을 읽으면 second truth"
  경고는 정확하고, 이미 3중으로 물려 있다: backend AIR-미소비 grep 게이트 +
  AIRBinding.v(게이트 인터페이스의 기계증명) + machine-neutral 게이트.
- **anti-hype / 83%** — 리뷰가 인용한 그 문장이 우리 앵커다. WO-B1
  (full-suite 증거 갱신) 전 beta-complete 선언 금지는 보드 규칙.
- **lifetime 금지 + 진단 책임** — 금지는 BDFL 확정(docs/118 §2.1)이고,
  리뷰의 "금지했으면 진단이 더 좋아야" 지적은 semantic squiggle(docs/140)
  + slot/zone provenance 진단이 정확히 그 채무의 지불 수단.
- **guard = evidence-amortized cost** — docs/142가 원 소유자. 리뷰의 수치
  인용(0.280x/0.144x)과 "fixture-한정, whole-language 주장 아님" 제한까지
  우리 문서의 자기-제한과 동일.
- **dual-emit = abstraction portability gate** — docs/119/백엔드 전략 그대로.

## 2. 신규 신호 — 등록 (≠ 닫힘)

| # | 신호 | 등록처 | 비고 |
|---|---|---|---|
| N1 | **frame-budget quota족** — perFrameFuel/HostCallCount/DrawCommands/AudioCommands/StorageOps/StreamBytes/QueuedEvents. guest는 command-buffer로 배치, host는 flush 시 검증. "진짜 병목은 guest 산술이 아니라 host boundary" | 보드 A-7 확장 (R6 정량축의 프레임-단위 후속) | 기존 R6 5축(alloc/spawn/channel/wall)은 프로세스-수명 단위 — 프레임 단위는 새 축이 맞다 |
| N2 | **WASIp3/WASI 0.3 async 방향** — component-native async(`future<T>`/`stream<u8>`/`async func`)가 SEA async와 sandbox ABI에 직결 | 보드 A-7 참고자료 | 리뷰 제공 날짜/버전명은 착수 시 공식 WASI/WIT 자료로 재검증. WASM lane은 0.2 pollable 패턴 고정 대신 0.3 async 방향을 기준 후보로 둔다 |
| N3 | **Wasmtime fuel vs epoch interruption** — fuel=결정적이나 instrumentation 비용, epoch=협조적 timeslice로 저렴. blocking host-call timeout은 별도 설계 필요 | 보드 A-7 참고자료 | 우리 wall-time watchdog과 상보 — epoch 방식은 R6 확장 시 검토 |
| N4 | **WASM 격리 과신 금지 근거(arXiv 2509.11242)** — WASI/WASIX 경유 host 자원고갈·인스턴스 간 성능저하 실증 연구 | docs/15 R6 절 각주 후보 | 우리 "WASM ≠ 자동 안전" 입장의 외부 실증 — 방향 일치 |
| N5 | **AIR relation-table export + Datalog/Soufflé CI verifier** — boundary/capture/effect/authority/lane/compression을 관계로 내보내고 위반 규칙을 관계 질의로 | 보드 신규 P2 | 컴파일러 본체 아님, **CI verifier 한정** — AIR-as-truth 재개방 금지 조건부 |
| N6 | **Swift SE-0304** — task tree/child lifetime/cancellation/priority/executor 모델 | A-3 executor 분리 참고 | executor 교체 시 계약 불변 원칙("LaneFact=의미, executor=구현")의 선례 |
| N7 | **Luau telemetry 연구** — creator 규모에서 false-positive 관리 + privacy-preserving 진단 텔레메트리 | A-4(squiggle) 참고 | BLUE noise policy와 정확히 동주제 |
| N8 | **WIT `world` ↔ capability manifest projection** — imports/exports가 interface-level 계약 | A-7/P2 프로토타입 후보 | zone/authority fact → WIT world 매핑은 자연스러움 |
| N9 | **SES/HardenedJS, AAM, Pony/Dala refcap** | 참고문헌 원장(본 문서 §5) | P2 독서 자료 — beta에 반입 금지(리뷰도 동의) |

## 3. 기각/정정 — 소수

- **등급표(A-/B+/...)**: 외부 관점 스냅샷으로 기록만. 우리 내부 계기판은
  게이트/ratchet 수치이지 문자 등급이 아니다 — 등급을 목표로 삼지 않는다.
- **"Self-host trajectory B+"**: 리뷰 시점보다 이미 전진(22/22 자기-빌드,
  §0). 착시 경고 자체는 유효 — LOC 6.57%와 runtime kernel·released
  compiler driver replacement·LSP 0% 명시는 우리 문서가 원 출처다.
  DRV-0/DRV-1 artifact rung과 LSP-0 payload rung(docs/150)은 이 0%를
  뒤집는 수치가 아니라, released replacement 전에 치환 경로를 검증하는
  artifact 사다리다.
- 사실 오류: **발견되지 않음.** (docs/137 때의 R4류 반박 필요 항목 없음.)

## 4. 실행 반영 — 보드 diff 요약

- A-7(sandbox 램프)에 N1 frame-budget quota족 + N2/N3/N4/N8 참고자료.
- A-3(SEA)에 executor 분리 P1 명시(리뷰 §6.2 매핑표 채택) — 단
  **BoundaryCaptureFact 정밀도(P0)는 동시 SEA 세션 소유** 재확인.
- A-4(squiggle)에 N7 — BLUE noise policy는 docs/140의 기존 인식과 합류.
- 신규 P2: N5 relation export.
- 리뷰의 P0 목록 중 "self-host typed payload ratchet 강화"와 "AIR/backend
  access lint 유지"는 **이미 존재하는 게이트의 유지 항목** — 신규 아님.

## 5. 참고문헌 원장 (리뷰 제공, 우선순위 보존)

P0: MLIR LangRef(owner-fact discipline만 차용) · WIT/Component Model ·
WASIp3/WASI 0.3 async 방향 · Wasmtime fuel/epoch · WASM isolation attacks(2509.11242)
P1: Swift SE-0304 · Luau telemetry(2403.02409) · SES/HardenedJS ·
AAM(1105.1743)
P2: Pony refcaps · Dala(2109.07541) · RefCaps for Safe Parallel Arrays
(1905.13716)

## Related
docs/137(리뷰 1호 판정) · docs/146(SEA — 리뷰 P0의 원 소유자) ·
docs/142(guard amortization) · docs/15(sandbox) · docs/140(squiggle) ·
TODO 보드 A-3/A-4/A-7
