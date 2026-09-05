# Zig Compiler Architecture

Updated: 2026-08-26

## 한 문장 요약

현재 Zig compiler는 source-file 단위 ZIR, function 단위 AIR, central InternPool과
semantic-analysis/codegen job을 조합한다. Pergyra에 유용한 것은 **artifact granularity와
typed work ownership**이며, central intern pool이나 lazy model을 그대로 복사하는 것은
아니다.

> 주의: Zig compiler internals는 rolling `master` source가 가장 직접적인 공식
> 자료이며 안정된 architecture specification이 아니다. 아래 내용의 확인 날짜
> 2026-08-26은 원 작성자의 source comment/declaration 조사 기록이다.
> 2026-09-05 통합 검토에서 upstream 내용을 재검증한 것은 아니다.

## Compilation spine

```text
source file
  -> parse / AstGen
  -> file-level ZIR
  -> Sema + InternPool/ZCU facts
  -> function-level AIR
  -> liveness / legalization
  -> LLVM, C, or native backend path
  -> link
```

`Air.zig`는 AIR을 Sema가 만들고 codegen이 소비한다고 정의한다. ZIR은 source file마다
하나지만 AIR은 function마다 하나다. 이 granularity 차이는 source discovery용
artifact와 executable routine artifact의 수명이 다름을 보여 준다.

Pergyra의 AIR은 abstraction intent verification IR이므로 Zig AIR과 책임이 전혀
다르다. 같은 약어를 설계 유사성의 증거로 사용하면 안 된다.

## Identity, discovery와 scheduling

Zig source는 named declaration이 실제 참조될 때 분석되는 lazy discovery model을
사용한다. compiler source는 `InternPool` index와 tracked instruction index를 여러
semantic/codegen identity에 사용한다. `Compilation`의 job union은 function analysis,
comptime unit analysis, type resolution, function codegen과 link work를 서로 다른 typed
job으로 구분한다.

특히 `codegen_func` job은 분석된 function identity와 그 function이 소유한 AIR을
함께 전달한다. 분석이 필요한 job과 codegen/link job을 분리해 type resolution이
준비되기 전에 backend work가 시작되지 않도록 한다.

## 강점과 실패 모드

강점:

- file fact와 routine fact의 물리적 granularity가 책임에 맞는다.
- compact interned identity로 type/value/declaration lookup을 통일한다.
- work queue가 raw callback이 아니라 typed job variant다.
- C, LLVM, native backend가 같은 semantic analysis 결과에서 출발할 수 있다.
- 최소 외부 의존성에서 compiler를 만드는 별도 bootstrap 경로가 존재한다.

비용과 위험:

- central pool index는 kind/epoch/lifetime validation이 약하면 wrong-kind 또는 stale
  identity bug를 크게 확산시킬 수 있다.
- lazy discovery는 오류를 내야 하는 unreachable declaration과 codegen-reachable work를
  구분해야 한다.
- 여러 backend가 서로 다른 semantic fallback을 가지면 parity가 깨진다.
- compiler internals가 빠르게 변하므로 구조를 안정 contract로 오인하면 안 된다.

## Pergyra에 가져올 것

| 불변식 | Pergyra 매핑 | 판정 |
|---|---|---|
| artifact granularity가 last consumer와 일치 | global immutable view + routine-local rows 분리 | 채택 |
| job이 typed variant와 owned payload를 가짐 | self-host compiler action/result boundary | 채택 |
| analysis와 codegen work 사이 준비 조건이 명시 | materialization/ABI readiness receipt | 채택 |
| compact identity를 kind와 함께 검증 | stable logical handle + wrong-kind/stale gate | 채택 |
| lazy declaration discovery | 진단 completeness가 유지되는 bounded source owner에서만 | 조건부 |
| global InternPool을 새 semantic authority로 사용 | registry owner identity를 덮어씀 | 거부 |
| backend별 독자 fallback | C/LLVM one-plan parity와 충돌 | 거부 |
| Zig bootstrap 절차를 그대로 복제 | Pergyra fixed-point evidence owner가 이미 별도 | 거부 |

Zig에서 가장 직접적으로 가져올 수 있는 것은 `job`의 모양이다. Pergyra의 worker나
parallel boundary를 늘리는 것이 아니라, 현재 한 active rung 안에서 전달되는 work가
**무슨 owner fact를 소비하고, 어떤 typed payload를 소유하며, 어느 readiness fact
없이는 실행될 수 없는지**를 variant로 봉인하는 것이다.

## 공식 자료

- [Zig Compilation source](https://github.com/ziglang/zig/blob/master/src/Compilation.zig)
- [Zig AIR source and ownership comment](https://github.com/ziglang/zig/blob/master/src/Air.zig)
- [Zig language compilation model](https://ziglang.org/documentation/master/)
- [zig-bootstrap](https://github.com/ziglang/zig-bootstrap)
