# 병렬 벤치마크 스위트 — 같은-머신 3축 비교 (2026-07-17)

> **갱신 (같은 날, B2/B3 이후)**: 아래 원본 표의 축 3 "64x 열위"는
> **auto-chunking(WO-RT-4 B3, `6f5f29c0`)으로 닫혔다** — 문서 끝
> "2026-07-17 갱신" 절이 최신 판정. 원본 표는 B3를 정당화한 측정
> 기록으로 보존.

docs/186의 벤치 축을 실측으로 채운 결과. **같은 머신**(이 Windows 박스,
16 논리코어, mingw gcc/gfortran -O2, go 1.x)에서만 유효 — 타 머신 숫자와
비교 금지. best-of-3, 전 대상 동일 산술(32-bit-safe), **전 대상 출력 일치
검증** (fine 9,900,000 / 32M 1,584,000,000 / fib 39,088,169).

측정일 주의: 동시 세션이 활동 중인 박스라 노이즈 있음(직렬 트윈 변동
~±20% 관측). 격차가 2x 미만인 행은 "동급"으로 읽을 것. 64x 같은 격차는
노이즈로 설명 불가 — 실재.

## 축 1. 처리량 (32M 원소, coarse 병렬)

같은 원소 바디를 16개 청크로 나눠 병렬 합산. Pergyra는 컴파일러
auto-chunking이 없어 **손-청크**(B_n 패턴 — 표면으로 오늘 표현 가능한 것).

| 대상 | best | 비고 |
|---|---|---|
| **Pergyra chunked ×16** | **18.5 ms** | `parallel (c in 0..16) join with sum` + 청크 함수 |
| C OpenMP `parallel for` | 21.9 ms | 런타임 자동 청크 |
| Go 16 goroutines | 22.9 ms | 파셜 배열 + WaitGroup |
| Fortran OpenMP do | 23.9 ms | reduction |
| C serial | 61.1 ms | 분모 |
| Pergyra serial | 49.3 ms | 분모 (직렬도 C-parity급, ±노이즈) |

**판정: 동급~우위.** 이 스케일에선 전 대상이 고정비(풀 시동)+work/16에
수렴 — Pergyra가 최속이지만 노이즈 밴드 감안 "동급 이상"이 정직한 표현.

## 축 2. 중첩 fork-join — fib(38), cutoff 28

재귀 병렬(~10단 중첩, 수백 태스크의 worker-await). **이 축은 WO-RT-3
(help-first await, `740f5a68`) 이전엔 벤치가 아니라 데드락이었다.**

| 대상 | best |
|---|---|
| C OpenMP task | 23.1 ms |
| **Pergyra nested `parallel..join`** | **23.7 ms** |
| Go goroutine 재귀 | 35.3 ms |
| C serial fib | 68.1 ms (분모) |

**판정: OpenMP task와 parity, Go보다 빠름.** 문법은 3항 중 최단
(`parallel (i in 0..2) join with sum { give Fib(n-1-i); }`).

## 축 3. fine-grain — 200k near-empty 태스크 (스케줄링 오버헤드 노출)

인덱스당 태스크 1개(현 lowering 그대로). wall ≈ 태스크당 오버헤드 × N.

| 대상 | best | 태스크당 |
|---|---|---|
| C OpenMP taskloop grainsize(1) | 35.2 ms | ~0.18 µs |
| Go goroutine/elem + atomic | 57.3 ms | ~0.29 µs |
| **Pergyra task/index** | **2,239 ms** | **~11 µs** |

**판정: 유일한 실질 열위 — 64x.** 원인은 측정으로 분해 완료(WO-RT-4):
할당/초기화 몫 1.7µs뿐, 무경합 signaling 0.02µs → **지배 비용 = 단일 큐
mutex + 태스크 상태 mutex의 경합**(17 경쟁자 × 짧은 임계구역 × 20만).
main park/wake 가설은 반증됨(main-help 확장 `e6e164c2`는 micro 중립,
coarse에서만 이득 B_n 2.42→2.21s). 해법은 등록된 사다리 그대로:
**B2 per-worker deque + stealing, B3 auto-chunking** (TODO 보드 WO-RT-4).
주: 축 1이 보여주듯 같은 워크로드를 손-청크하면 오늘도 1위 — 갭은
"표현 불가"가 아니라 "자동화 미구현".

## 종합 — "우리 병렬이 우위인가"의 정직한 답

- **coarse/처리량: 동급 이상.** OpenMP/Go/Fortran과 같은 줄, 오늘 최속.
- **중첩 fork-join: 동급(OpenMP)~우위(Go).** 24시간 전엔 데드락이던 축.
- **fine-grain 자동 분해: 열위 64x.** 유일한 진짜 갭, 원인·해법 등록 완료.
- **표현력/안전 축(숫자 밖)**: 한 줄 `join with sum`, 공유변이 캡처 정적
  거절(data-race-free by construction), 인덱스-순서 fold(결정적 reduce),
  budget/cancel 통합 — OpenMP/Go에 없는 계약들. 이 계약을 유지한 채 위
  숫자를 낸 것이 요지.

## 재현

`bash benchmarks/run_parallel_suite.sh` (PGY_BIN 지정 가능). 파일:
`perf_parallel_map_{fine,chunked,serial}.pgy`, `perf_parallel_fib.pgy`,
`baseline_par_map.{c,go,f90}`, `baseline_par_fib.{c,go}`,
`perf_parallel_task_overhead[_serial].pgy` (µs/task 마이크로).

## 2026-07-17 갱신 — B2(shard)/B3(auto-chunk) 이후: 축 3 열위 닫힘

같은 날 두 rung이 착지한 뒤의 재측정. 판정 근거는 두 층:

**(1) old-vs-new interleave (solo, 기계 드리프트 상쇄 — 가장 깨끗한 수)**
`parallel (i in 0..200000) join with sum` task/index 경로,
best-of-3 ×3라운드 교차 실행:

| | pre-B3 | post-B3 |
|---|---|---|
| fine 200k | 2,174–2,596 ms | **64–71 ms** (**~34x**) |
| micro 20k | 238–267 ms | 58–64 ms (직렬 트윈 ~62ms = 하네스 바닥 도달) |

**(2) 스위트 재실행 2회 (동시 세션 빌드 부하 有 — 절대값 부풀고
상대 위치만 유효; 두 실행 범위 병기)**

| 축 | pgy | C OpenMP | Go | Fortran |
|---|---|---|---|---|
| 처리량 32M | 70–82 | 78–90 | 79 | 86–98 |
| 중첩 fib(38) | 79–92 | 84–96 | 92 | — |
| **fine 200k (auto-chunk)** | **83–177** | 66–72 | 118–203 | — |

**축 3 판정 교체: 64x 열위 → OpenMP taskloop과 같은 대역, Go
goroutine/elem보다 우위.** 원리: emitter가 인덱스당 태스크 대신
`chunk_count(n) = min(n, workers×4)`개의 chunk driver를 fan-out —
스폰 스레드의 태스크당 부기(~11µs, 분해측정으로 확정)가 N이 아니라
chunk 수에만 붙는다. **의미론은 구성적으로 보존**: 인덱스별 ctx 슬롯
전체가 유지되고 reduce/materialize는 그대로 전체 N을 인덱스-순서
left-fold — 결과·checked-arith panic·Float fold 순서가 unchunked
lowering과 byte-동일. 분할 산술은 런타임 단일 소스(양 백엔드 동일
경계), **정책 SoT는 self-host owner**
(`src/self_hosted/parallel/chunk_policy_owner.pgy` — golden diff +
C==LLVM leg parity + required projection pin 10개 + legacy rejection 1개,
`tests/selfhost_parallel_chunk_policy_smoke.sh`).

같은 캠페인의 반증 기록(측정이 수술을 두 번 재조준): B2 shard화
자체는 fine에 **중립**(단일 큐 경합 가설 반증 — 진범은 스폰 스레드
자신의 태스크당 비용), pre-park spin은 최고 2.2x·분산 폭발(981–2425ms,
yield 폭풍이 producer 선점)로 **되돌림**. 게이트: join 전 rung+17거절,
중첩 목격자 4/4, backpressure 64/64×2, disjoint/snapshot/vision/lane,
병렬 fixture 6종 codegen 결정성(double-emit byte-identity) — 전부
GREEN. B_n n=10 실워크로드 45.4s (pre 47.6s, 무회귀).

**종합 판정(갱신)**: 세 축 전부 동급 이상 — coarse 최속권, 중첩
OpenMP-parity·Go 우위, fine OpenMP-대역·Go 우위. 데이터레이스-프리
캡처 계약·결정적 reduce·budget/cancel 통합을 유지한 채 낸 숫자다.

## 2026-07-19 갱신 — site-specialized chunk driver

Generic runtime chunk driver가 매 인덱스마다 function pointer로 item
wrapper를 부르던 경로를 삭제했다. C/LLVM emitter가 join site마다
`_pgy_pjoin_chunk_N`을 만들고 item wrapper를 직접 호출하며, 런타임은
remainder-balanced `[lo, hi)` 분할과 chunk task dispatch만 수행한다.

WSL/Linux x86_64, GCC/LLVM-enabled compiler, best-of-3, 같은 실행의 수치:

| 축 | Pergyra | 비교 대상 |
|---|---:|---:|
| 처리량 32M | **11 ms** | C OpenMP for 19 ms, Fortran OMP do 21 ms |
| 중첩 fib(38) | **15 ms** | C OpenMP task 24 ms |
| fine 200k | **7 ms** | C OpenMP taskloop grainsize(1) 17 ms |

이 실행은 post-change 절대 측정이며 같은 바이너리의 generic-driver
A/B가 아니다. 따라서 과거 64-71 ms와의 차이 전체를 이번 직접 호출화의
효과라고 귀속하지 않는다. 이번 rung이 직접 증명하는 것은 산출물에서
per-item indirect call이 사라졌고, C/LLVM shape, worker-count invariance,
chunk budget charge, nested progress, backpressure가 유지되었다는 점이다.

빌드 캐시 관측도 분리한다. 저장소 `.tmp`의 WSL 증분 빌드에서 변경된
translation unit 2개 재컴파일+링크는 28.53 s / peak RSS 164,824 KiB,
완전 no-op make는 9.94 s / peak RSS 7,916 KiB였다. 캐시는 재컴파일 범위를
2개로 줄였지만 `/mnt/e` 위 수천 dependency stat과 Makefile 평가의 약
10초 고정비는 남는다. 이 수치는 런타임 성능이 아니라 빌드 시스템의
별도 최적화 대상으로 취급한다.
