# GHC Architecture

Updated: 2026-08-26

## 한 문장 요약

GHC의 Core, STG, Cmm spine은 각 단계가 서로 다른 의미와 실행 모델을 책임질 때
다단계 IR이 정당화된다는 사례다. Pergyra가 가져올 것은 IR 개수가 아니라 **각
하강 단계에서 보존·소실되는 사실을 명확히 하는 법**이다.

## Compilation spine

```text
Haskell source
  -> parse / rename / typecheck
  -> desugar
  -> typed Core
  -> Core-to-Core optimization and simplification
  -> CorePrep
  -> STG
  -> STG passes and codegen annotations
  -> Cmm
  -> native / LLVM / other backend
```

Core는 작은 typed functional language로 high-level optimization의 중심이다. STG는
closure, thunk와 evaluation model에 더 가까운 표현이다. Cmm은 data representation,
control flow와 machine-oriented backend 경계에 가깝다.

## Fact identity와 정보 수명

GHC의 핵심은 같은 프로그램을 세 이름으로 복사해 두는 것이 아니다.

- Core에는 type/coercion과 high-level rewrite에 필요한 의미가 남는다.
- STG에는 lazy evaluation과 closure code generation에 필요한 구조가 드러난다.
- Cmm에는 concrete runtime convention과 backend가 소비할 low-level control/data
  representation이 나타난다.

하위 단계가 상위 의미를 다시 추측하기보다 lowering이 다음 consumer에 필요한
형태를 만들어 준다. 반대로 한 단계에서만 필요한 annotation은 phase-indexed STG
형태처럼 해당 pass 수명에 맞춰 붙일 수 있다.

## Pass model

GHC는 Core 위에서 simplifier, demand analysis, worker/wrapper, specialization,
float-in/out 등 여러 분석과 rewrite를 실행한다. pipeline은 `CoreToDo` 같은 명시적
pass description으로 구성된다. 일부 최적화는 서로 기회를 열어 주기 때문에 여러
phase에서 simplification이 반복된다.

이 설계는 typed semantic core에서 강력한 최적화를 할 수 있다는 장점이 있지만,
rewrite phase ordering과 inlining rule interaction이 매우 복잡해질 수 있다. “한 번
더 simplifier를 돌리면 좋아진다”는 방식은 명시적인 종료 조건과 phase contract가
없으면 구조를 이해하기 어렵게 만든다.

## Backend와 bootstrap 관점

GHC backend는 Core, STG 또는 Cmm 중 허용된 입력 단계에서 최종 artifact로 가는
책임을 캡슐화한다. 그러나 각 backend가 상위 의미를 독자적으로 재구성하면 parity가
깨질 수 있으므로, 공유 lowering owner와 backend-neutral fact의 범위가 중요하다.

GHC는 대형 self-host compiler의 사례이지만, self-host라는 사실 자체가 pipeline의
정당성을 증명하지 않는다. 단계별 dump, verifier, tests와 bootstrapping 결과가 함께
있어야 implementation evidence가 된다.

## Pergyra에 가져올 것

| 불변식 | Pergyra 매핑 | 판정 |
|---|---|---|
| IR 단계마다 고유한 consumer와 information lifetime이 있음 | HIR/DIR/RIR/MIR/AIR abstraction-loss contract | 채택 |
| typed semantic core에서 의미 보존 최적화 | semantic owner fact가 target projection 전까지 유지 | 채택 |
| runtime execution model은 별도 lowering에서 명시 | resource/effect/cleanup/ABI owner boundary | 채택 |
| phase-local annotation은 phase type에 묶임 | owner artifact kind/epoch와 last consumer | 채택 |
| 반복 rewrite는 phase와 종료 조건을 가짐 | fixed point를 쓸 경우 bounded metric과 falsifier 필요 | 조건부 |
| Core/STG/Cmm와 비슷한 수의 새 IR 추가 | consumer가 없는 IR은 문서·copy 비용만 만듦 | 거부 |
| backend별로 상위 semantic fact 재구성 | one SoT 및 C/LLVM parity와 충돌 | 거부 |

Pergyra는 이미 여러 fact family와 verification-only AIR을 가진다. GHC에서 배운다고
새 IR을 더 만드는 것은 반대 방향이다. 대신 현재 각 IR 문서에 다음 네 항목이 없으면
그것을 보강해야 한다: **입력 owner, 보존 fact, 의도적으로 소실하는 fact, 마지막
합법 consumer**.

## 공식 자료

- [GHC STG syntax and current STG pipeline](https://ghc.gitlab.haskell.org/ghc/doc/libraries/ghc-9.15-inplace/src/GHC.Stg.Syntax.html)
- [GHC Core optimization pipeline types](https://ghc.gitlab.haskell.org/ghc/doc/libraries/ghc-9.15-inplace/GHC-Core-Opt-Pipeline-Types.html)
- [GHC backend interface](https://ghc.gitlab.haskell.org/ghc/doc/libraries/ghc-9.15-inplace/GHC-Driver-Backend.html)
- [GHC compiler debugging and IR dumps](https://ghc.gitlab.haskell.org/ghc/doc/users_guide/debugging.html)
