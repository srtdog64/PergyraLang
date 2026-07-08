# 176. Mathlib 실행 설계도 — 논문이 스펙이다

Status: `design-blueprint` (구현 0). 작성 2026-07-06. 계기: BDFL — "수학/mathlib,
알고리즘이 현실적으로 제일 중요하다. lib 구현도 논문위주로." 상위: docs/138
(scope 원장 — 이미 pdqsort/AlphaDev/Cowlishaw 행 보유), docs/148(L0/L1/L2 +
inventory 게이트), docs/159(doctrine-pass — 본 문서가 8번 항을 추가),
CheckedArith.v(산술 기반), project_signed_default_decision(★UInt 비노출 canon).

---

## 0. 한 장 요약

- **재서열 감사 동의**: stdlib을 두 트랙으로 나눈다 — **mathlib = 기반 트랙**
  (table stakes: 이것 없으면 어떤 버티컬도 못 섬), **도메인 삼형제 = 전시 트랙**
  (thesis 차별화). "현실적으로"의 정확한 내용: 던전크롤러(A*/FOV/noise/PRNG),
  self-host(hash/sort/float 렌더), 공장(Kahan 합산) — **세 견인처가 전부
  mathlib를 먼저 당긴다.** money 1호는 전시 트랙 1호로 유지하되 기반 트랙 뒤.
- **논문위주 = doctrine-pass 8번 항으로 공식화**(§3): 모든 mathlib 모듈은
  canonical reference(논문, 없으면 정본 스펙)를 헤더에 달고, **논문의 정리 →
  모듈 불변식 → property 테스트**로 내려온다. 논문 부록의 test vector가
  backend-parity의 공짜 oracle이 된다(SipHash/PCG/Ryū 전부 보유). 이건 새
  발명이 아니라 docs/138의 "ADOPT published networks, do not re-search"를 전
  모듈로 일반화한 것 — 그리고 **anti-slop의 구조적 답**: 구현의 결정자가
  모델 기억이 아니라 인용 가능한 문헌이 된다.
- **선행작업 1개 발견(실측)**: 스칼라 builtin 14개(Sqrt/Sin/…/Random)는 있으나
  **bit/wrapping 연산 0개**. PRNG/hash는 64-bit 모듈러 산술이 정의라 L0 builtin
  6~8개가 필요하다 — §2에 signed-default canon을 **위반하지 않는** 해법.

## 1. 두 개의 킬러 스토리 (왜 논문위주가 정확히 우리 방식인가)

1. **Timsort 사건**: Java/Python의 stdlib sort에 수년간 살아있던 버그를 찾은 건
   퍼저가 아니라 **논문의 불변식을 기계검증하던 팀**이다(de Gouw et al.,
   "OpenJDK's java.utils.Collection.sort() is broken", CAV 2015 — invariant
   위반으로 ArrayIndexOOB 재현). 논문의 정리를 스펙으로 삼는 구현은 이 클래스를
   구조적으로 방어한다 — 우리의 proof-pack 방법론과 동일 계보.
2. **이진탐색 overflow**: `mid=(lo+hi)/2`의 overflow는 "거의 모든 이진탐색이
   깨져 있다"(Bloch 2006)의 그 버그인데, **Pergyra에서는 checked arith가
   클래스째 fail-closed**(CheckedArith.v + 양 백엔드 가드) — mathlib이 우리
   기반 위에서 남들보다 안전하게 시작한다는 실증.

## 2. 선행 L0: 명시적 wrapping/bit builtin (canon-호환 해법)

**문제**: xoshiro/PCG/splitmix/SipHash/FNV는 uint64 모듈러 산술(wrap 곱/덧셈,
rotl, xor, shift)이 **정의 그 자체**다. 우리는 (a) UInt 표면 비노출(canon,
재도입 금지), (b) 기본 산술 overflow = fail-closed panic(docs/11)이라 이대로는
PRNG를 쓸 수 없다.

**해법 — 명시적 이름의 wrapping 연산을 L0 builtin으로** (UInt 도입 없이 Long의
비트패턴 위에서 동작, wrap이 **이름에 드러나므로** 무음 아님 = §1.1 정합):

`WrapAdd64 / WrapMul64 / Rotl64 / Xor64 / ShiftR64 / ShiftL64 / PopCount64 /
Clz64` (+ 필요 시 `WrapAdd32/Rotl32` — PCG32용). 의미: two's-complement 비트
패턴 연산, overflow는 정의된 wrap(모듈러) — "명시적 opt-in wrap은 UB가 아니라
사양"(docs/11의 2-레이어 모델과 일치: 표면 기본은 checked, 이름 붙은 wrap만
모듈러). 양 백엔드 parity fixture 필수(C: uint64_t 캐스트 연산 / LLVM: 정수
명령 직접 — 이미 checked-arith 가드 인프라와 같은 자리).

**WO-MATH-0**: 이 builtin 8개 + parity fixture + docs/11에 한 절("named wrap =
defined modular semantics"). mathlib 전체의 유일한 컴파일러-측 선행작업.

## 3. Doctrine-pass 8번 항 (docs/159 템플릿 확장)

> **8. canonical reference** `[gate: 헤더 grep + vector fixture]` — 모듈 헤더에
> 논문/정본 스펙 인용 필수. 논문이 주는 정리·불변식을 모듈 불변식으로 선언하고,
> **논문/레퍼런스 구현의 test vector를 fixture로 채택**(backend-compare oracle).
> 논문이 없는 관용 알고리즘(FOV 등)은 정본 스펙(RogueBasin/RFC)을 인용 — 규칙은
> "인용 가능한 레퍼런스", 우선순위는 논문.

## 4. 모듈-논문 표 (기반 트랙의 본체)

| 모듈 | canonical reference | 논문이 주는 불변식 → 테스트 | 견인 |
|---|---|---|---|
| **random** | PCG(O'Neill 2014) 또는 xoshiro256**(Blackman-Vigna, TOMS 2021) + splitmix64(Steele-Lea-Flood, OOPSLA 2014 — 스트림 분할) | 주기·jump 함수·레퍼런스 출력 vector; 분할 스트림 독립 | ★삼중: 퍼저 choice-seq(docs/175)·게임·replay 배지(docs/174) |
| **hash** | SipHash-1-3(Aumasson-Bernstein 2012 — **논문 부록에 test vector**) + FNV-1a(스펙) | PRF 성질, HashDoS 방어(R6 DoS 계보), vector 일치 | self-host HashMap 키·R6 |
| **sort** | pdqsort(Peters 2021, arXiv) + 소형 커널 = AlphaDev 네트워크(Mankowitz+ Nature 2023 — docs/138 기존 행) / stable: Timsort(+CAV 2015 불변식!) | 정렬성+순열성(property), Timsort 불변식 그대로 테스트 | self-host·전 도메인 |
| **search** | Bloch 2006(overflow 사건) + Bentley | 경계 정확성; overflow는 checked-arith가 기본 방어 | 전 도메인 |
| **intmath** | Stein binary GCD(1967); pow-by-squaring | 수론 항등식 property | money·versioning |
| **floatconv** | Ryū(Adams, PLDI 2018 — float→str) + Eisel-Lemire(Lemire, SP&E 2021 — str→float) | **shortest round-trip 정리** → 왕복 fixture | ★self-host json이 지금 필요(float 렌더) |
| **numsum** | Kahan(1965)/Neumaier(1974) 보상 합산 | 오차 상한 정리 → 상한 테스트 | 공장 센서 합산(2차 측점) |
| **decimal** | Cowlishaw, General Decimal Arithmetic(docs/138 기존 행) | 정확 십진 산술 | money 기반(전시 트랙 하부) |
| **grid/geom** | Bresenham(IBM SysJ 1965)·A*(Hart-Nilsson-Raphael 1968)·JPS(Harabor-Grastien, AAAI 2011) | 최단성/admissibility → 최적성 테스트 | ★던전크롤러 |
| **fov** | recursive shadowcasting(RogueBasin — 정본 스펙, 논문 아님 명시) + symmetric 변형 | 대칭성 property | 던전크롤러(+관측 축 docs/174 Gap A와 접점) |
| **noise** | Perlin, "Improving Noise"(SIGGRAPH 2002) | 레퍼런스 vector | 던전크롤러 절차 생성 |

전부 L1(순수, caps 0) — WO-MATH-0 이후엔 컴파일러 무변경 .pgy 슬라이스.
명명은 개념=모듈 원칙(전 턴 결정) 그대로: random/hash/sort/search/…, "math"는
훗날 네임스페이스 가족명.

## 5. 순서 + WO

1. **WO-MATH-0** — L0 wrapping/bit builtin(§2, 유일한 컴파일러 작업).
2. **WO-MATH-1 random** — 퍼저·게임·replay 삼중 견인. splitmix64부터(가장 작고
   스트림 분할이 퍼저 요구).
3. **WO-MATH-2 hash** — SipHash(vector 공짜) + HashMap 키 경로 접속.
4. **WO-MATH-3 floatconv** — self-host json의 현재 갭. Ryū 왕복 fixture.
5. **WO-MATH-4 sort/search** — pdqsort+AlphaDev 커널.
6. **WO-MATH-5 grid/geom/fov/noise** — 던전크롤러 팩(dogfood 직전에).
7. **WO-MATH-6 numsum/decimal** — 공장·money 기반.
8. 전시 트랙(money 1호 doctrine-pass)은 **WO-MATH-1~3 뒤** 재개 — 재서열의
   정확한 의미.

## 6. Negative space (거절 목록 — 블랙홀 방지)

- **BLAS/LAPACK 야심 없음** — 소형 고정 vec2/3만(게임). 선형대수 생태계는 모듈
  생태계 몫.
- **빅인트/임의정밀 없음**(P3 유지), **심볼릭 수학 없음**, **통계 패키지 없음**.
- SIMD 명시 표면 없음(백엔드 최적화 여지로만).
- 이 목록 자체가 docs/138 scope 원장의 tier 규율 재확인 — mathlib은 "수학
  라이브러리"가 아니라 **알고리즘 기반층**이다.

## Related

docs/138(scope 원장 — pdqsort/AlphaDev/Cowlishaw 씨앗) · docs/148(inventory
게이트) · docs/159(doctrine-pass — 8번 항 본 문서) · docs/11+CheckedArith.v
(named-wrap의 2-레이어 정합) · project_signed_default_decision(UInt 비노출 —
§2가 존중) · docs/174 §4(replay 배지)·docs/175(퍼저 choice-seq) — random의
견인처 · de Gouw+ CAV 2015 · Bloch 2006 · O'Neill 2014 · Blackman-Vigna 2021 ·
Steele-Lea-Flood 2014 · Aumasson-Bernstein 2012 · Adams PLDI 2018 · Lemire 2021 ·
Kahan 1965/Neumaier 1974 · Hart-Nilsson-Raphael 1968 · Harabor-Grastien 2011 ·
Perlin 2002 · Mankowitz+ Nature 2023.
