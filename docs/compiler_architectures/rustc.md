# rustc Architecture

Updated: 2026-08-26

## 한 문장 요약

rustc의 강점은 “모든 것을 query로 만들었다”가 아니라, 계산에 key와 owner를 주고
의존성을 기록해 다시 계산할 범위를 증명하려는 데 있다. Pergyra는 이 **query
discipline**을 배울 수 있지만, 현재 active rung과 무관한 범용 query engine이나
persistent cache를 먼저 만들 이유는 없다.

## Compilation spine

```text
source -> tokens -> AST
       -> expansion / name resolution
       -> HIR
       -> typed HIR (THIR)
       -> MIR
       -> monomorphization / codegen units
       -> LLVM IR or another codegen backend
       -> object
```

AST는 source와 가깝고, HIR은 분석하기 쉽게 일부 surface construct를 desugar한다.
THIR은 더 명시적인 typed form이며 MIR lowering의 입력이다. MIR은 단순한 typed CFG로
borrow checking, dataflow, constant evaluation, optimization과 codegen 준비에 쓰인다.

## Query model

rustc의 주요 중간 계산은 `TyCtxt`를 통해 호출되는 typed query로 조직된다. query는
key와 result type을 가지며 provider가 계산을 소유한다. 실행 결과는 memoize할 수 있고,
query가 부른 다른 query는 dependency가 된다.

incremental compilation의 red-green model은 이전 실행의 query graph와 stable
fingerprint를 사용한다.

- 입력이 모두 unchanged임을 증명하면 query를 다시 실행하지 않는다.
- 입력 일부가 바뀌었어도 재계산 결과가 동일하면 dependent query를 green으로 유지할
  수 있다.
- 결과 equality와 deterministic computation이 cache reuse의 전제다.
- 모든 query 결과를 반드시 disk에 저장하지는 않는다.

## Identity와 ownership

HIR item과 definition은 typed ID 계열로 참조되고, 많은 type/value는 intern된다.
이 덕분에 큰 구조를 계속 복사하지 않고 compact handle로 전달할 수 있다. 다만 중심
context와 interned lifetime에 많은 compiler data가 묶인다.

공식 rustc 개발 문서도 query system이 위치한 `rustc_middle`이 매우 큰 crate가 되고,
관련 기능이 여러 crate에 흩어질 수 있다는 구조적 비용을 지적한다. 즉 중앙 query
context는 dependency를 보이게 하지만 물리적 모듈 응집성을 자동으로 해결하지 않는다.

## 강점과 실패 모드

강점:

- 계산의 입력, output, dependency와 cacheability를 한 protocol로 표현한다.
- item 단위 재계산과 incremental reuse가 가능하다.
- HIR/MIR 각 단계가 서로 다른 분석 목적과 정보 수명을 가진다.
- query key가 ad-hoc program scan 대신 stable lookup seam을 제공한다.

비용과 위험:

- query granularity가 잘못되면 coarse invalidation 또는 query explosion이 생긴다.
- central context가 사실상 모든 것의 물리적 허브가 될 수 있다.
- interning은 identity와 lifetime을 단단히 묶고 memory reclamation을 어렵게 한다.
- query로 이행 중인 코드와 전통적인 pass가 함께 있으면 경계가 복잡해진다.
- cache correctness는 stable hashing, determinism, dependency completeness에 달려 있다.

## Pergyra에 가져올 것

| 불변식 | Pergyra 매핑 | 판정 |
|---|---|---|
| 계산마다 typed key와 named provider가 있음 | SoT registry의 identity/owner/consumer row | 이미 방향 일치 |
| dependency가 호출 과정에서 추적됨 | owner view가 실제로 읽은 upstream fact 목록과 falsifier | 채택 |
| result equality가 dependent invalidation을 제한 | immutable owner artifact의 digest/epoch | 조건부 |
| stable handle로 large fact를 참조 | logical spine handle + kind/epoch validation | 채택 |
| HIR/MIR가 서로 다른 정보 수명을 가짐 | Pergyra HIR/DIR/RIR/MIR/AIR loss contract | 채택 |
| compiler-wide query engine | 현재 active self-host rung 밖의 새 architecture | 거부(현재) |
| persistent incremental cache | 반복 비용, stable key와 crash recovery가 측정된 뒤 | 조건부 |
| giant context를 새 semantic owner로 둠 | 기존 owner registry를 덮어씀 | 거부 |

Pergyra에 필요한 첫 단계는 `PgyTyCtxt` 같은 새 중심 객체가 아니다. 기존 owner가
**어떤 upstream identity를 읽었고, 어떤 변화가 자신의 결과를 invalidate하는가**를
작은 contract와 negative test로 고정하는 것이다. 그 계약으로도 실제 반복 비용을
줄일 수 없다는 측정이 나온 뒤에만 shared scheduler/cache를 검토한다.

## 공식 자료

- [rustc overview](https://rustc-dev-guide.rust-lang.org/overview.html)
- [Queries: demand-driven compilation](https://rustc-dev-guide.rust-lang.org/query.html)
- [Incremental compilation](https://rustc-dev-guide.rust-lang.org/queries/incremental-compilation.html)
- [HIR](https://rustc-dev-guide.rust-lang.org/hir.html)
- [MIR](https://rustc-dev-guide.rust-lang.org/mir/index.html)
- [Compiler source organization](https://rustc-dev-guide.rust-lang.org/compiler-src.html)
