# Clang + LLVM Architecture

Updated: 2026-08-26

## 한 문장 요약

Clang은 사용자가 쓴 소스와 의미 분석의 흔적을 오래 보존하고, LLVM은 변환이 어떤
분석 결과를 깨뜨렸는지 명시적으로 관리한다. Pergyra가 가져올 핵심은 거대한 AST나
LLVM식 pass framework가 아니라 **source fidelity**와 **preservation/invalidation
계약**이다.

## Compilation spine

```text
source
  -> lexer / preprocessor
  -> parser <-> Sema
  -> source-faithful Clang AST
  -> Clang CodeGen
  -> LLVM IR
  -> LLVM analysis / transform passes
  -> target code generation
  -> machine code / object
```

Clang parser는 Sema와 함께 AST를 만든다. Clang CodeGen은 AST를 소비해 LLVM IR을
생성한다. 이 경계 이후 C/C++의 source-level 의미를 backend가 다시 복원하는 것이
기본 모델이 아니다.

## Fact identity와 정보 수명

Clang AST는 가능한 한 원래 source construct를 보존한다. 단순히 모두 desugar한
트리만 두지 않고 source form과 semantic form을 함께 표현하며, `SourceLocation`은
include stack과 macro expansion 위치까지 진단에 필요한 provenance를 보존한다.
AST node는 일반적으로 publish 뒤 immutable하게 설계되며 canonicalization이 뒤의
임의 mutation 때문에 무효화되지 않게 한다.

이 구조의 장점은 refactoring, IDE, error recovery가 codegen과 별개 consumer로
source evidence를 사용할 수 있다는 점이다. 비용은 AST가 syntax와 semantics를
함께 담아 크고, C++ template처럼 불가피한 mutation 지점이 복잡하다는 점이다.

## Analysis와 invalidation

LLVM New Pass Manager는 IR unit별 analysis manager를 두고 분석 결과를 재사용한다.
변환 pass는 `PreservedAnalyses`로 무엇이 유지됐는지 선언한다. IR object를 삭제했으면
해당 key의 cached result를 clear해야 하고, 상위·하위 analysis dependency도
invalidation에 반영해야 한다.

여기서 중요한 성질은 cache 자체가 아니다.

- 변환 전의 분석 결과가 변환 뒤에도 유효하다는 가정을 묵시적으로 하지 않는다.
- 보존을 주장하는 pass가 그 근거를 소유한다.
- 삭제된 IR identity를 cache key로 계속 쓰지 않는다.
- 선택적 보존은 측정 가능한 compile-time 이익이 있을 때만 복잡성을 감수한다.

## 실패와 진단

Clang은 오류가 있는 입력도 가능한 범위에서 recovery AST로 남긴다. recovery node는
위치와 대략적인 구조를 보존하지만 유효한 language semantics가 있는 것처럼 가장하지
않는다. 따라서 “진단을 계속하기 위한 불완전 구조”와 “codegen 가능한 semantic
fact”가 구분된다.

LLVM pass는 preservation을 과장하면 stale analysis를 소비할 수 있다. 반대로 모든
것을 무조건 invalidate하면 안전하지만 compile time을 잃는다. 정확성 우선의
conservative invalidation과 측정 뒤의 선택적 preservation이 기본 tradeoff다.

## Pergyra에 가져올 것

| 불변식 | Pergyra 매핑 | 판정 |
|---|---|---|
| source form과 semantic fact의 provenance를 함께 보존 | parser/semantic owner가 diagnostic projection에 stable source identity 제공 | 채택 |
| 변환 consumer가 보존한 fact를 명시 | MIR/DIR/AIR owner view를 변형하는 stage의 input/output contract | 채택 |
| 삭제된 identity의 분석 결과를 소비하지 않음 | stable handle epoch/kind negative gate | 채택 |
| recovery structure와 executable fact를 구분 | diagnostic-only carrier는 codegen owner가 읽지 못하게 함 | 채택 |
| 범용 analysis cache | 측정된 반복 owner operation이 생기기 전에는 열지 않음 | 조건부 |
| Clang AST를 Pergyra semantic SoT로 모사 | 이미 분리된 owner spine과 충돌 | 거부 |
| LLVM IR에서 language semantics 재구성 | backend guess와 dual authority를 만듦 | 거부 |

Pergyra의 AIR은 verification-only sibling이므로 LLVM IR처럼 codegen SoT가 되어서는
안 된다. 반대로 LLVM의 `PreservedAnalyses`에서 배울 것은 AIR 자체가 아니라,
**어떤 변환 뒤 어떤 owner fact를 계속 믿어도 되는가를 선언하고 검증하는 방식**이다.

## 공식 자료

- [Clang CFE Internals Manual](https://clang.llvm.org/docs/InternalsManual.html)
- [Introduction to the Clang AST](https://clang.llvm.org/docs/IntroductionToTheClangAST.html)
- [LLVM New Pass Manager](https://llvm.org/docs/NewPassManager.html)
- [LLVM Code Generator](https://llvm.org/docs/CodeGenerator.html)
