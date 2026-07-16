# 186. 병렬 전체 구현 계획 — 실행기 세대교체와 남은 구멍들 (2026-07-17)

BDFL 지시 "병렬 전체 구현을 생각해보자"의 답. docs/177(감사)→178(경계 증거)
→181(표면 설계)→182(남은 뼈) 위에 서고, **fresh 재유도가 아니라 그 코퍼스의
다음 층**이다. 이 문서는 (1) 실행기 실측 지도, (2) 구멍의 정직한 목록,
(3) 단계별 구현 계획(P-A~P-D)을 고정한다.

전제 교정 하나: "병렬이 가장 큰 약점"에서 **성능은 이제 약점이 아니다** —
pool=auto 수정(`b0b5c614`) 후 B_n n=10에서 Fortran OpenMP를 이기고(47.6s vs
61.0s) hand-C OpenMP의 1.18x 이내다. 진짜 약한 곳은 아래 5개다.

## 1. 실행기 실측 지도 (2026-07-17)

방출 코드의 실제 경로:

```
parallel (i in lo..hi) join with sum
  → C: 인덱스당 ctx 1개 + 태스크 1개 (chunking 없음)
  → pgy_lane_spawn_dispatch(PGY_LANE_WORKER_POOL, ...)   ← 얇은 라우터
  → pgy_spawn → g_pgy_pool                                ← 진짜 실행기
  → 전 핸들 pgy_await → 인덱스-순서 checked left-fold     ← 결정적 reduce
```

- **g_pgy_pool** (`pgy_parallel.h`): 단일 전역 FIFO 큐 + 큐 mutex/cond 1쌍,
  **태스크당 calloc + mutex + cond**. deque 없음, work-stealing 없음.
  worker 수는 하드웨어 코어수(`pgy_default_worker_count`).
- **pgy_lane_scheduler.c의 `lane_run_on_worker`**(태스크당 pthread
  create+join)는 **방출 경로에 없다** — facade 계약 테스트 전용. 방출 코드는
  `pgy_lane_spawn_dispatch`만 탄다. (오해 주의: 이 파일을 보고 "태스크당
  스레드 생성"이라 진단하면 오진.)
- **`src/runtime/async/scheduler.c` = 잠자는 2호 스케줄러**: per-worker
  로컬 큐 + 전역 큐 + **work-stealing**(`SchedulerStealWork`) + fiber 실행
  + epoll IO worker. `SchedulerCreate/Start`의 호출자가 src 어디에도 없음 —
  **완성돼 있으나 미배선**. lane facade 주석이 가리키는 "production
  refinement"의 실물이 이미 있는 것.

## 2. 구멍의 정직한 목록 (우선순위순)

1. **중첩 병렬 = 도달 가능한 데드락 클래스 (미방어)**. pool worker가
   유저 코드 안에서 같은 풀로 fan-out 후 `pgy_await`하면
   `pthread_cond_wait`로 블록 — 일 돕기(help)도 큐 드레인도 없다. worker
   N개가 전부 그렇게 블록하면 서브태스크는 큐에서 영원히 대기 = 고전적
   pool-starvation 데드락. 정적 거절도 미확인, 런타임 가드도 없음.
   docs/181의 sibling channel-dep 거절은 이 클래스를 안 덮는다.
2. **채널 blocked-send 간헐 hang (LLVM leg) — open**. 칩 task_863abddf.
   stress 게이트(`parallel-backpressure-stress-test-smoke`)는 64/64 green이나
   root cause 미상, "repeated CI green 전까지 닫지 않음" 상태.
3. **실행기 부기 비용**: 태스크당 calloc+mutex+cond + 단일 큐 경합이 측정된
   잔여 갭(5.93x vs hand-C OpenMP 7.17x, n=9 best-of-3)의 주범 후보.
   ※ 후보다 — P-B에서 **측정으로 확정 후** 수술 (measure-first).
4. **반응형 표면 미구현**: docs/181 Form B `parallel on (lane) { every /
   continuous }`는 R2 실행 의미론부터 미착수 — BDFL 결정 3건 대기
   (docs/182 §3: 시작 의미론 / cancel 표면 / 단일-lane).
5. **행동 목격자 얇음**(docs/179 축2-3): blocked-send/recv, spawn lifecycle,
   release 순서, budget 고갈, cap 거부 × 양 백엔드의 체계적 corpus 부재.

+ 별개 칩: §7 RED-1 — generic specialization 안의 `parallel`이 C leg에서
  "invalid storage class"(task_6fd52632, noisy-fail이라 silent-wrong 아님).

## 3. 설계 원칙 (SEA 불변 — 전 Phase 공통)

- **lane 결정은 증거로, 실행기는 그 아래서 깊어진다**: 관측 결과는
  executor-invariant (pgy_lane_scheduler.c 헤더 계약). 실행기 교체가 결과를
  바꾸면 그건 실행기 버그다.
- **무음 직렬화 금지**(docs/177 F1의 교훈): 실행기 전환/폴백은 전부 관측
  가능해야 한다.
- **결정적 reduce 유지**: `join with sum`의 인덱스-순서 left-fold는 계약이다
  — chunking을 넣어도 fold 순서는 인덱스 순.
- **트윈 lockstep + `.bc` 재생성**: 런타임 헤더 수술마다 (WO-0-4).
- **measure-first**: 성능 수술은 B_n(+마이크로) before/after 없이 착수 금지.

## 4. 단계별 계획

### P-A. 정확성 먼저 (fail-close)

**P-A1. 중첩 병렬 데드락 닫기 — 2중 방어.**
- (a) **런타임 help-first await**: `pgy_await`가 pool worker 스레드에서
  불렸고 대상 태스크가 PENDING이면, cond_wait 대신 **큐의 태스크를 꺼내
  실행하며 대기**(work-donation). fork-join 풀의 표준 해법이고 데드락
  클래스 자체를 제거한다. worker 판별은 기존 `g_pgy_thread_current`로 충분.
- (b) **semantic 인지**: parallel-inside-parallel을 정적으로 탐지(기존
  capability/effect interprocedural 전파 패턴 재사용). 1차 rung은 진단
  (ADVISORY 또는 fact 기록)이지 거절이 아님 — (a)가 실행을 안전하게 만들기
  때문. 거절은 (a) 불가 플랫폼이 생길 때만.
- **목격자**: 오늘 데드락나는 fixture(중첩 fan-out, worker수보다 깊게)를
  timeout 하네스로 — (a) 전 RED, 후 GREEN. 양 백엔드.

**P-A2. 채널 blocked-send root cause** — 기존 칩 계속(Linux stress repro).
stress 게이트는 유지, root cause 확정 전 "닫힘" 선언 금지.

**P-A3. §7 RED-1 칩** — generic specialization × parallel C-leg 수리.

### P-B. 실행기 세대교체 (측정 사다리)

한 번에 2호 스케줄러로 뛰지 않는다. 각 rung은 독립 측정+독립 되돌림 가능.

- **B1. join 부기 다이어트**: join은 N을 안다 — 태스크당 mutex/cond 대신
  **join당 완료 카운터 1개 + cond 1개**(atomic 감소, 0에서 signal).
  태스크 구조체는 ctx 배열처럼 **배치 할당**(이미 join_emit이 ctx를 한 번에
  malloc — 같은 모양). 예상 효과: 태스크당 calloc 3회+mutex/cond init 제거.
  측정: B_n n=9 best-of-3 (현 2.42s) + 마이크로(작은 태스크 1만개 fan-out).
- **B2. per-worker deque + stealing**: 단일 전역 큐 → worker당 로컬 deque
  (owner는 LIFO push/pop, thief는 FIFO steal). **잠자는 scheduler.c의
  ConcurrentQueue/StealWork 로직을 THREAD-model로 이식**하는 것이 첫 후보
  (fiber는 이 rung에서 안 가져옴). P-A1(a)의 help-first가 이 위에서 자연히
  "자기 deque부터 드레인"이 된다.
- **B3. chunking/grain** (docs/182 §6 doctrine-gate 준수): 인덱스-병렬을
  태스크 min(N, k×workers)개의 청크로. **claim-gate**: 세밀-그레인 워크로드
  (per-index 코스트가 작은 array-map형)에서 측정 이득을 먼저 보이고 착수.
  reduce는 청크 부분합→인덱스-순서 fold로 결정성 유지.
- **B4. (결정 대기) fiber/LOCAL_ASYNC 통합**: 2호 스케줄러의 fiber 실행을
  LOCAL_ASYNC lane 실행기로 채택할지 — B1~B3 측정치가 나온 뒤 BDFL 결정.
  M:N은 여전히 evidence-gated 한 lane (project 교리).

**성공 기준(정량)**: n=9 best-of-3에서 hand-C OpenMP 대비 1.10x 이내
(현 1.21x). 미달이어도 각 rung의 측정 기록이 성과다.

### P-C. 표면 완성 (docs/181 잔여)

- **C1. Form B every/continuous**: BDFL 결정 3건의 결정 메모 초안 작성 →
  결정 후 docs/182 §3 절차대로. (이 문서는 결정을 대신하지 않는다.)
- **C2. Duration 산술/비교** 잔여 + `every(d)` 소비.
- **C3. true waitany** (any-join의 폴링 제거 — 순수 perf, B2 이후).

### P-D. 행동 목격자 corpus (병렬 전용 절)

docs/179 축2-3의 병렬 몫을 체계화: blocked-send/recv × budget 고갈 ×
cancel-중-대기 × spawn lifecycle × release 순서, 전부 양 백엔드. 추가 2종:
- **stealing 불변**: 태스크 유실/중복 0 (완료 카운터 목격자).
- **병렬 결정성**: `join with sum` 결과가 worker 수/실행 순서와 무관
  (인덱스-fold 계약의 실행 목격자) — codegen 결정성 게이트(WO-A4)의
  런타임 자매.

## 5. 순서와 금지

순서: **P-A1 → (P-B1 ∥ P-A2/A3) → P-B2 → P-B3 → P-D 상시 → P-C는 BDFL 결정 후**.
근거: 도달 가능한 정확성 구멍(데드락)이 성능보다 먼저; 실행기 수술은 매
rung 측정; 표면은 결정 대기라 병렬화 불가.

금지:
- 측정 없는 실행기 수술 (B_n before/after 필수).
- lane collapse — 3층(계약/fact/런타임) 유지, 실행기는 런타임 층 안에서만
  교체.
- 무음 폴백/무음 직렬화 재도입 (F1 재발 금지).
- 이 계획을 이유로 한 lifetime/M:N-default 재논의 (기존 판정 유지).

## Related

- docs/177 (감사) · docs/178 (경계 증거 4종) · docs/181 (표면 설계) ·
  docs/182 (남은 뼈 WO) · docs/179 축2-4 (원장) ·
  benchmarks/BN_RESULTS.md (측정 기준선) · TODO 보드 WO-RT-3/RT-4 (실행 항목)
