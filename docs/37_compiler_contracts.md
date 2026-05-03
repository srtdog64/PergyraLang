# Pergyra Compiler Contracts

Last updated: 2026-05-04

이 문서는 Pergyra compiler pipeline의 source-of-truth 계약을 고정한다. 목적은
“현재 구현이 완성됐다”는 선언이 아니라, 구현이 어느 방향으로 수렴해야 하는지
정확히 제한하는 것이다.

## 1. IR Layer Contracts

### AST

AST는 raw parse tree와 source span을 보존한다. AST는 사용자-facing 진단 문맥으로
사용할 수 있지만, backend semantic 판단의 source-of-truth가 되면 안 된다.

금지:

- backend가 AST를 다시 걸어 semantic feature를 재발견하는 것
- ownership, authority, effect, zone safety를 AST helper가 최종 판정하는 것
- C/LLVM backend가 서로 다른 AST-carried inventory를 읽는 것

### HIR

HIR은 sugar가 제거된 typed language tree와 function/body CFG view를 제공한다.
all-path return, unreachable flow, CFG-owned control lowering의 근거는 HIR CFG에서
시작한다.

### DIR

DIR은 declaration/domain graph다. `subject`, `ability`, `role`, `party`, `zone`,
`world`, `relation`, `effect`, `projection`, `intent`의 선언 관계를 graph edge로
소유한다.

### RIR

RIR은 resource/resource-flow IR이다. Slot, projection, authority, relation/effect
전파, rollback/invalidation 후보 같은 runtime-relevant resource facts를 소유한다.

### MIR

MIR은 backend가 소비하는 execution IR이다. CFG block, instruction, cleanup root,
rollback/invalidation root, pin cleanup fact, direct call fact, terminator provenance를
명시적으로 소유한다.

### AIR

AIR는 codegen IR이 아니다. AIR는 abstraction-boundary verification layer다.
HIR/RIR/MIR/DAG evidence를 모아 intent/zone/world/authority/effect boundary drift를
검증한다. AIR가 backend text를 바꾸면 설계 위반이다.

## 2. Compiler-Facing Orthogonality Rule

Compiler-facing orthogonality rule:

> 각 semantic 축은 자기 owner IR에서 한 번만 확정되고, 이후 단계는 그 fact를 소비한다.

구체 규칙:

- backend가 AST를 다시 걸어 semantic feature를 재발견하는 것을 금지한다.
- authority/effect/zone의 최종 판정은 DIR/RIR/AIR evidence로 고정한다.
- Slot / Pin vs Static Lifetime: Slot은 runtime-validated handle이고, pin/cleanup은
  MIR cleanup fact와 CFG/AIR verifier가 보강한다.
- 이전 문서에서 이들을 `TOKEN_IDENTIFIER`로만 표기한 경우는 lexer/parser 계약
  drift로 본다. keyword contract 문서는 semantic axis를 기준으로 표기해야 한다.

## 3. Cleanup And Pin Contract

MIR cleanup fact names are owned by `src/compiler/mir_cleanup_fact_names.h`.
Consumers must use that vocabulary instead of duplicating literals.

Required facts:

- normal cleanup edge: `MIR_CLEANUP_FACT_EDGE`
- rollback cleanup edge: `MIR_CLEANUP_FACT_EDGE_FROM_ROLLBACK`
- invalidation cleanup edge: `MIR_CLEANUP_FACT_EDGE_FROM_INVALIDATION`
- pin cleanup edge: `MIR_CLEANUP_FACT_PIN_UNPIN_EDGE`

MIR validation must reject topology-only cleanup claims when the matching fact is
missing. AIR may audit MIR cleanup evidence, but MIR remains the cleanup owner.

## 4. Type Resolution Contract

DAG metadata is the beta source-of-truth for type dependency ordering. Recursive
resolver fallback is retired for the frozen beta surface.

Allowed:

- metadata lookup
- owner-local materialization through the central metadata API
- explicit DAG dead-end diagnostics

Forbidden:

- direct `resolve_type_node(...)` use outside the metadata owner
- hidden recursive fallback
- declaration-order-only type success for frozen subset paths

## 5. Backend Contract

C and LLVM may use different implementation techniques, but for the frozen subset
they must consume the same MIR/DIR/RIR inventory and produce equivalent behavior.

The beta rule is not “LLVM is fully refactored.” The beta rule is:

> frozen subset parity is locked, and declaration/top-level inventory seams are
> narrowed until backend truth drift is no longer observable.
