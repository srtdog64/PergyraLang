# Swift Compiler Architecture

Updated: 2026-08-26

## 한 문장 요약

Swift compiler는 mutable AST와 ad-hoc lazy resolution에서 생기는 dependency와
cycle 문제를 request evaluator로 명시하려 하고, Swift-specific 의미는 SIL까지
보존한 뒤 LLVM으로 내린다. Pergyra가 배울 지점은 **typed request identity, cycle
diagnostic, semantic IR verifier**다.

## Compilation spine

```text
source / imported module
  -> parser / AST
  -> name lookup and Sema requests
  -> SILGen
  -> raw/canonical SIL
  -> SIL optimization and ownership verification
  -> lowered SIL
  -> IRGen
  -> LLVM IR
  -> object
```

Swift AST는 source와 high-level semantics를 다루고, SIL은 AST보다 명시적이지만
LLVM IR보다 Swift semantics를 더 많이 보존한다. IRGen 뒤 LLVM IR에는 Swift-specific
knowledge를 다시 소유시키지 않는다.

## Request evaluator

request는 입력 parameter, result type과 evaluation function을 가진 value-like key다.
evaluator만 evaluation function을 호출하며 선택적으로 결과를 memoize한다. request가
다른 request를 호출하면 dependency stack으로 관계가 기록되고 cycle을 탐지할 수
있다.

이 구조가 겨냥한 문제는 단순 compile-time 속도가 아니다.

- mutable AST field가 언제, 무엇 때문에 채워졌는지 알기 어려움
- multi-file lazy type checking이 실행 순서에 따라 달라지는 문제
- ad-hoc callback recursion의 cycle과 infinite recursion
- coarse callback이 너무 많은 선언을 해결하는 문제

공식 문서는 request evaluator가 incremental adoption 중이며 mutable AST와 separate
cache가 공존할 수 있음을 명시한다. 이 과도기 자체가 dual state의 비용과 cycle
diagnostic 품질 문제를 보여 준다.

## SIL과 verifier

SIL은 function, basic block, typed instruction로 구성되는 Swift 전용 IR이다.
Ownership SSA에서는 non-trivial value에 `owned`, `guaranteed`, `unowned` 같은 ownership
kind를 부여하고 operand constraint와 consuming use를 검증한다. SILVerifier는 잘못된
def-use, lifetime-ending use, interior pointer liveness 같은 compiler-generated 오류를
pipeline 안에서 잡는다.

중요한 점은 ownership을 source syntax 설명으로만 두지 않고, optimizer와 codegen이
소비하는 IR invariant로 낮춰 verifier가 확인한다는 것이다.

## 강점과 실패 모드

강점:

- request dependency와 cycle을 inspectable graph로 만든다.
- semantic operation을 LLVM까지 미루지 않고 SIL에서 명시한다.
- compiler transform 자체가 만든 ownership 오류를 verifier가 조기에 차단한다.
- AST, SIL, lowered SIL, LLVM IR 사이 정보 손실 시점이 비교적 선명하다.

비용과 위험:

- evaluator와 mutable AST cache가 공존하는 동안 두 state path가 생긴다.
- request 수가 커지면 boilerplate, key design, cache copy 비용이 증가한다.
- cycle 탐지와 좋은 user diagnostic은 별개의 문제다.
- 풍부한 SIL은 frontend/backend 사이에 큰 최적화 substrate를 유지해야 한다.

## Pergyra에 가져올 것

| 불변식 | Pergyra 매핑 | 판정 |
|---|---|---|
| request가 typed input/result identity를 가짐 | compiler-world action/intent request variant | 채택 |
| dependency stack으로 cycle을 명시 | orchestration owner의 exact cycle path diagnostic | 채택 |
| semantic property가 IR invariant가 됨 | MIR/DIR fact와 AIR cross-layer verification | 채택 |
| lowering stage마다 verifier를 둠 | owner publication 뒤 focused verifier/negative gate | 채택 |
| evaluator cache와 AST field의 과도기 dual read | Pergyra `new ? old` 금지와 충돌 | 거부 |
| compiler-wide request evaluator | 현재 owner seams만으로 부족하다는 측정 전에는 과함 | 조건부 |
| SIL 규모의 새 codegen IR | Pergyra MIR/DIR owner와 중복 | 거부 |

Pergyra의 `intent`는 Swift request와 같은 계산 API가 아니다. 다만 production compiler
purpose intent가 여러 action을 조정할 때, 각 action의 typed request와 terminal result,
dependency cycle을 명시하는 규율은 직접 가져올 수 있다. 이때 evaluator가 새 fact
owner가 되면 안 되고 기존 registry owner를 호출하는 orchestration이어야 한다.

## 공식 자료

- [Swift Request Evaluator](https://github.com/swiftlang/swift/blob/main/docs/RequestEvaluator.md)
- [Swift compiler representations and performance](https://github.com/swiftlang/swift/blob/main/docs/CompilerPerformance.md)
- [Swift Intermediate Language](https://github.com/swiftlang/swift/blob/main/docs/SIL/SIL.md)
- [SIL Ownership SSA](https://github.com/swiftlang/swift/blob/main/docs/SIL/Ownership.md)
- [Swift compiler documentation index](https://github.com/swiftlang/swift/blob/main/docs/README.md)
