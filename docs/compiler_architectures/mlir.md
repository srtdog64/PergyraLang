# MLIR Architecture

Updated: 2026-08-26

## 한 문장 요약

MLIR의 가치가 “IR을 많이 둔다”는 데 있는 것은 아니다. operation, trait,
interface, verifier와 operation-anchored pass가 서로의 계약을 알게 하는 데 있다.
Pergyra는 이 **검증 가능한 interface discipline**을 배울 수 있지만 dialect framework
자체를 도입할 필요는 없다.

## Core model

MLIR은 operation을 공통 단위로 삼고 operation 안에 region, block, value를 중첩할 수
있다. dialect는 operation, type, attribute의 vocabulary를 확장한다. operation의
semantics는 concrete kind만으로 분기하지 않고 trait와 interface를 통해 generic
analysis/transform에 노출할 수 있다.

```text
dialect operations
  -> traits / interfaces / verifier
  -> nested operation pass pipeline
  -> dialect conversion or lowering
  -> lower-level dialects
  -> LLVM IR or target-specific output
```

이는 하나의 고정 compiler pipeline이라기보다 여러 compiler가 domain별 abstraction을
정의하고 lowering할 수 있는 framework다.

## Pass와 analysis contract

MLIR pass는 특정 operation type 또는 viable operation에 anchor된다. nested pass
manager는 operation hierarchy의 어느 수준에서 pass가 실행되는지 명시한다.
`IsolatedFromAbove` 같은 경계는 pass가 바깥/sibling state를 임의로 바꾸지 못하게 해
병렬 실행과 analysis validity를 지키는 기반이 된다.

pass는 분석 결과를 보존할 수 있고, broken invariant를 만나면
`signalPassFailure()`로 pipeline을 중단한다. pass manager는 각 pass 뒤 verifier를
실행하도록 구성할 수 있으며 timing, IR printing, crash reproducer 같은 instrumentation도
제공한다.

## Interface와 verifier

새 operation을 추가할 때 모든 transform에 concrete special case를 넣으면 확장성이
무너진다. MLIR interface는 operation/type/attribute가 특정 semantic capability를
어떤 contract로 제공하는지 표현한다. trait와 interface verifier는 해당 contract가
IR 안에서 성립하는지 검사한다.

좋은 점은 extension과 generic transform 사이의 계약이 명시된다는 것이다. 비용은
dialect, conversion, interface, rewrite와 pass scheduling 자체가 큰 meta-framework가
된다는 점이다. 작은 언어에 그대로 도입하면 semantic owner보다 framework plumbing이
더 커질 수 있다.

## Pergyra AIR과의 관계

Pergyra AIR은 codegen IR이 아니라 cross-layer evidence를 검증하는 sibling IR이다.
따라서 MLIR의 “multi-level” 이름만 보고 AIR, MIR, RIR 등을 자유롭게 dialect로 섞는
것은 맞지 않는다. Pergyra owner registry가 semantic identity를 계속 소유해야 한다.

가져올 수 있는 부분은 더 작다.

- verifier가 concrete producer 이름을 전부 special-case하지 않고 declared capability
  contract를 읽는다.
- pass/validator가 자신의 anchor 밖을 몰래 재탐색하지 않는다.
- 실패는 pipeline을 중단하고 invalid IR을 다음 consumer에 넘기지 않는다.
- reproducer는 시작 IR, pipeline identity와 option을 함께 고정한다.

## Pergyra에 가져올 것

| 불변식 | Pergyra 매핑 | 판정 |
|---|---|---|
| interface가 semantic capability를 선언 | owner registry의 typed consumer contract | 채택 |
| verifier가 publication boundary에서 invariant 검사 | AIR/ABI/MIR focused verifier | 채택 |
| pass가 explicit operation/owner anchor를 가짐 | typed owner view 밖 root re-scan 금지 | 채택 |
| pass failure 뒤 pipeline 중단 | missing/corrupt fact fail-closed | 이미 방향 일치 |
| crash reproducer가 input+pipeline을 고정 | falsifying fixture와 exact compiler mode receipt | 채택 |
| dialect framework 전면 도입 | 현재 compiler 규모와 owner model에 과함 | 거부 |
| 모든 Pergyra fact를 generic operation으로 환원 | domain authority와 stable identity를 약화 | 거부 |
| 자동 parallel pass expansion | independent owner boundaries와 측정 전에는 위험 | 조건부 |

## 공식 자료

- [MLIR Language Reference](https://mlir.llvm.org/docs/LangRef/)
- [MLIR Pass Infrastructure](https://mlir.llvm.org/docs/PassManagement/)
- [MLIR Interfaces](https://mlir.llvm.org/docs/Interfaces/)
- [MLIR documentation index](https://mlir.llvm.org/docs/)
