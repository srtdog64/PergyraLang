# 177. 병렬성 실측 감사 — "직렬성의 착시" 발견

Status: `audit-findings` (수정 0 — 발견·근인·수정 순서 제약만 고정. 수정은
태스크 #1/#2). 작성 2026-07-06. 계기: BDFL — "FP가 병렬에 유리한 건 불변이라서
인데 우리도 구현했나? 병렬성 체크해서 이치에 맞게 값 나오는지 보자. 여러 문제점
체크해야 함." 방법: 프로브 8종(.tmp/par_probe/) × 양 백엔드 × 10회 반복 +
codegen/런타임 소스 추적. 상위: docs/113(메모리·동시성 모델), docs/146(SEA),
docs/114(async 포지셔닝).

---

## 0. 한 장 요약

- **개념 교정**: FP의 병렬 우위는 스택 배치가 아니라 **공유 가변 상태의 부재**
  (불변성)다. 그 기준으로 감사한 결과 —
- **교리는 절반 구현돼 있다**: copy-only async 표면(docs/113), 가변 컬렉션
  캡처 fail-closed 거절(실측), Channel/Slot 스레드-안전 런타임(실측), spawn
  인자의 Channel 수송 거절(실측). 게이트 3종(worker-boundary-ub / memory-
  concurrency-model / parallel-core-contract) 전부 green.
- **★F1 (major)**: 그러나 **parallel/spawn은 현재 전부 무음 직렬 실행**이다.
  10/10 결정론은 병렬이 이치에 맞아서가 아니라 **병렬이 없어서**였다 —
  직렬성의 착시.
- **★F2 (major, F1이 가림)**: 캡처된 가변 **스칼라는 포인터로 공유**되고
  아무 가드가 없다. 풀이 켜지는 순간 실 데이터레이스(UB)로 승격 →
  **수정 순서 제약: F2 정책 없이 F1을 고치면 안 된다.**

## 1. 프로브 매트릭스 (전부 양 백엔드 × 10회)

| 프로브 | 내용 | 결과 |
|---|---|---|
| p1 | 채널 4-send parallel 후 합산 | 10/10 = "10", 백엔드 일치 |
| p2 | `let mut x=1; parallel { x=x+1; } Log(x)` | **2** — 캡처 돌연변이가 join 후 보임(복사 아님) |
| p3 | 두 arm이 같은 x에 `+1`/`+10` | 10/10 = "11" 결정론 |
| p4 | spawn×2 + await | 10/10 = 42/25 |
| p5 | 서로소 슬롯 2개 병렬 쓰기 | 10/10 = "10" |
| p6 | 두 arm이 같은 x에 각 10만 증분(레이스 압력) | 10/10 = **200000, 손실 0** |
| p7 | 교대 필수 핑퐁(직렬이면 어떤 순서로도 deadlock) | 블록-arm **파싱 불가**; spawn판은 Channel 인자 수송 **거절** |
| p8 | **backpressure 목격자**: 용량 8 채널에 10만 send + 동시 소비 arm | **HANG 양 백엔드** — arm이 겹치지 않음 |

p1~p6의 "이치에 맞는 값"은 전부 직렬 실행과 일치하는 값이다. p8이 판별자였다.

## 2. 근인 사슬 (F1 — 소스 추적, 전 고리 실측)

1. 방출은 올바르다: LLVM parallel 방출(llvm_stmt_parallel_async.c:369-392)은
   전부-spawn 후 전부-await(fan-out/join), `pgy_lane_spawn_dispatch(WORKER_POOL)`.
   런타임도 올바르다: lane dispatch→`pgy_spawn`(풀 제출), `pgy_parallel_run`
   (제출 전체 후 조인), pthread 워커 풀(기본 4).
2. **그러나 생성된 main에 `pgy_pool_init`이 없다**: transpiler.c:440은
   `transpiler_requires_thread_pool()`일 때만 방출 → thread_pool_usage.c
   ("Shared C/LLVM") → `pgy_mir_program_uses_thread_pool` → **MIR surface-usage
   fact(`uses_thread_pool_surface`)가 parallel/spawn 프로그램에서 참이 되지
   않는다**(p1/p4/p8 방출 C에서 pool_init 0회 실측 — dispatch 호출은 있음).
3. 풀 비활성 → `pgy_spawn`이 **`pgy_spawn_inline_completed(fn, arg, "spawn",
   false, …)`로 무음 폴백**(pgy_parallel.h:455 — warn 플래그가 문자 그대로
   `false`). 첫 spawn 호출 안에서 태스크가 완주 실행 → 직렬. p8에선 생산자가
   9번째 send에서 영원히 대기.
4. **왜 아무 게이트도 못 잡았나**: 기존 corpus(producer_consumer, async_demo,
   backend_compare fixture)는 전부 buffer-fitting/overlap-불요 워크로드 —
   직렬 실행과 관찰 동치. **동시성 목격자 fixture가 코퍼스에 없다.**
   docs/146의 SEA 계약("executor must not change the program's meaning")은
   이 예제들에 대해선 공허하게 성립해왔다.

무음 폴백 자체가 CLAUDE §1.1(hidden control flow) 위반이라는 점이 이중 결함:
fact가 고쳐져도, 풀 초기화 실패 시 같은 무음 직렬화가 재발한다. 폴백은 warn
또는 fail이어야 한다.

## 3. F2 — 스칼라 캡처는 포인터 공유 (교리 갭, F1이 가림)

- llvm_stmt_parallel_async.c:256 — 캡처 ctx struct에 **값이 아니라 alloca
  포인터**를 저장. 모든 arm이 부모 스택 슬롯 공유.
- 가드는 가변 **컬렉션**만 거절("cannot share mutable collection by pointer;
  use a channel/result boundary or copy before spawning") — **스칼라 무가드.**
- 현재는 F1(직렬)이 가려서 p3/p6이 결정론으로 보인다. 풀이 켜지면 같은 프로브가
  unsynchronized RMW = C-수준 UB. docs/113 교리("worker-local/lock/atomic/
  immutable-snapshot 아니면 rejected or unsafe")를 스칼라가 빠져나가는 구멍.
- 비대칭 노트: **클로저는 copy-capture(Stage A), parallel은 포인터-캡처** —
  두 경계의 캡처 교리가 갈라져 있다. 단일-arm 쓰기+join-후 읽기(p2)는 구조적
  동기화로 건전할 수 있으나(Rust scoped-thread 유사), 다중-arm 쓰기(p3)는
  정책이 필요하다 — IntentConflict.v `sep_when_active`의 문장-수준 미시판.
- **★수정 순서 제약: F2 정책(거절/copy-in/명시 마커) 착지 전에 F1(풀 활성화)을
  먼저 고치면 안 된다.** 태스크 #1에 명기.

## 4. F3 — 표면 갭 (minor)

- **bare 블록이 parallel arm으로 파싱 불가** → 다중-문장 arm은 while-트릭
  강제. 핑퐁 같은 실 프로토콜 arm 표현 제약.
- **spawn 인자 Channel 수송 거절 vs parallel 캡처 Channel 허용** 비대칭 —
  copy-only 인자 교리로는 정합이나, 결과적으로 "채널로 통신하는 두 named task"
  를 spawn으로 표현할 수 없다(parallel 블록 캡처만 가능한데 F3-1로 arm 표현이
  제한). 동시성 표현의 실질 통로가 좁다.

## 5. F4 — 긍정 실측 (지킬 것)

Channel/Slot/Future 값 흐름 10/10 결정론·백엔드 일치, 가변 컬렉션 거절 발화,
게이트 3종 green, spawn 인자 copy-only 강제. **불변-경계 교리의 절반은 실물**
이다 — 나머지 절반(스칼라, 그리고 진짜 병렬)이 태스크 #1/#2.

## 6. 등록

- 태스크 #1 — F1: fact 수리 + 무음 폴백 제거 + **p8-형 backpressure 목격자
  fixture를 게이트로 승격**(재발 방지의 본체).
- 태스크 #2 — F2: 스칼라 캡처 정책(권고: 다중-작성자 거절 + copy-in 기본,
  클로저 교리와 정렬). #1보다 선행 또는 동시.
- F3은 표면 결정(BDFL)이라 WO 미등록 — 이 문서가 결정 입력.
- WitnessDataRace.v의 well-typed 전제가 스칼라 캡처를 모델하는지 확인해 정리
  범위를 명시할 것(#2에 포함) — 정리가 틀린 게 아니라 전제 밖일 것으로 추정
  되나 실측 전 단정 금지.

## Related

.tmp/par_probe/(프로브 8종+로그 — 미커밋 실험물) · docs/113(교리 — F2가 집행
갭) · docs/146(SEA — lane 계약은 옳고 실행이 미완) · docs/114 · pgy_parallel.h
:455(무음 폴백) · llvm_stmt_parallel_async.c:256(포인터 캡처)/:369(fan-out
join 올바름) · thread_pool_usage.c(Shared C/LLVM 탐지 — fact 미설정 근인) ·
transpiler.c:440(조건부 pool_init) · IntentConflict.v(다중-작성자 정책의 이론
착지점) · 클로저 Stage A(copy-capture 선례 — 교리 정렬 대상).
