# 병렬 벤치마크 스위트 — 같은-머신 3축 비교 (2026-07-17)

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
