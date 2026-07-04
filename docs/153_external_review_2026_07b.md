# 153. 외부 기술 리뷰 #3 판정 (2026-07-04, "최신 관측판")

Status: `judged`. docs/137(#1)·docs/149(#2)에 이은 같은 계열 리뷰의
세 번째. 판정 프로토콜: 사실 검증 → #2 대비 델타 → stale 정정 →
신규 신호 등재. **공유 신호 등기부는 docs/149가 원본**이며 이 문서는
차분만 기록한다.

## 0. 판정 요약

- **정확도: #2와 동급(≈95%), 사실 오류 0에 수렴.** 진술 대부분이 우리
  문서(README·docs/146·148·150·140·142, PROGRESS)의 충실한 재진술이다
  — 숫자(83%/6.57%/993·120/121/86 PASS/22/22/20·86·12/0.280x·0.144x)
  전부 우리 장부와 일치. 자기-문서-기반 리뷰의 장점이자 한계.
- **최대 가치 = 수렴 3연속.** 외부 P0(BoundaryCaptureFact precise
  value-capture producer coverage)가 내부 보드 frontier와 세 번 연속
  일치했다. 관측이 독립적이지 않다는 할인(우리 문서를 읽는다)을
  적용해도, 우선순위 산정 로직의 수렴은 유효한 건강 신호다.
- **신규 신호 ≈ 0.** 참고문헌·P0 목록이 #2와 동일 계열(MLIR/WIT/WASI
  0.3.0/fuel·epoch/WASM isolation/SE-0304/Luau×2/SES/AAM — 전부
  docs/149에서 등재 완료, relation export는 A-12로 이미 보드행).

## 1. Stale 정정 2건

- **(정정-1) "GuardCalculus는 proof-sketch, CI coqc 확인 전까지 beta
  evidence 아님"** → 시점이 늦다. 실태(2026-07-04 검증):
  `formal-semantics-test-smoke`가 **ci-linux 레시피에 포함**되고
  ci.yml이 coq를 설치하며, coqc 루프가 GuardCalculus.v 포함 **21개
  증명 파일**을 0 admits로 컴파일한다(오늘 GenericAxisCarriage.v까지
  포함해 로컬 green 확인). 단 리뷰의 *취지*(모델 증명 ≠ 구현 일치)는
  우리 스스로의 라벨이며 유지한다 —
  VerificationMethodology.v가 그 갭 자체를 정리로 증명해 둔 상태.
- **(정정-2) 관측창이 docs/151 이전에서 끝난다.** 리뷰에는 generic
  의미축 조약 전체가 없다: 6축 재심, Decision-0(positional)·GATE 5값
  닫힘, GenericAxisCarriage.v 5정리, 소·중·대 실측, 반증 배터리
  (G-2 C-측·G-6 landed, G-2L 등재), matrix-lock 게이트. 리뷰 결론
  ("새 키워드가 아니라 증거 체인의 정밀도")은 여전히 옳지만, 등급표 중
  Language identity·Stdlib discipline·수학화 밀도는 관측 이후 상태가
  더 진전됐다. 리뷰 귀책 아님 — 관측 시점 기록.

## 2. 리뷰가 옳아서 아픈 곳 (내부 장부와 일치 재확인)

- **Runtime executor depth C+** — lane facade는 계약이지 production
  런타임이 아니다(4개 lane이 단일 worker-thread executor 공유). 내부
  기록과 문자 그대로 일치. 리뷰의 lane→executor 매핑 표는 SEA facade
  작업의 **구체 사양으로 채택 가치가 있다**(아래 §3-i).
- BLUE noise policy(A-3 계열), L2 sketch 승격 금지(docs/148 G1-G5가
  이미 게이트), over-pinning P0(SEA 스트림 소유), beta 83% 비완료 —
  전부 재확인.

## 3. 채택분 (소수)

- **(i) lane→executor 목표 매핑 표**를 SEA facade 작업의 수용 기준
  초안으로 등재: Inline→direct / PinnedZone→owner-zone queue /
  WorkerPool→bounded queue / BlockingPool→dedicated pool /
  LocalAsync→fiber / MovableScheduler→work-stealing / Reject→
  fail-closed. 원칙 문장 채택: **"ExecutionLaneFact가 의미고 executor는
  구현이다 — executor를 갈아도 언어 계약은 불변."**
- **(ii) 인용구 등재(drift 방지)**: "Intent는 runtime object로 항상
  남아야 한다는 뜻이 아니다 — Intent는 source-level semantic owner이고
  runtime에는 evidence가 요구하는 구조만 남는다." 우리 소거 교리의
  정확한 외부 재진술 — runtime object bloat 리스크의 방어 문장으로.
- **(iii) guest frame command-buffer 패턴**(per-frame quota 상세)은
  #2에서 등재한 frame-budget 계열의 구체화로 킬러-유즈케이스(sandbox)
  설계 시 참조.

## 4. 리뷰 P0 → 내부 보드 매핑 (신규 항목 없음 확인)

| 리뷰 P0 | 내부 소유 |
|---|---|
| BoundaryCaptureFact precise coverage | SEA 스트림(동시세션 소유) — 3연속 수렴 |
| AIR JSON lane matrix 확장(합성 금지) | machine-neutral 규율(GREEN 유지 중) |
| BLUE noise policy | squiggle advisory 결정 표(A-3 계열) |
| AIR/backend access lint | A-3 원칙 + 기존 smoke |
| L2 sketch 승격 금지 | docs/148 G1-G5 게이트 실존 |
| typed payload ratchet | WO-S 트랙 |
| GuardCalculus CI화 | **이미 완료** (§1 정정-1) |

## Related

docs/149(#2 — 신호 등기부 원본) · docs/137(#1) · docs/151(관측창 밖
조약) · docs/146·148·150(리뷰가 관측한 대상) · TODO 보드 닫힘 정정 기록
