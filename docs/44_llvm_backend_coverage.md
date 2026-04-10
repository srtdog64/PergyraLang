# LLVM 백엔드 커버리지 현황

마지막 업데이트: 2026-04-11

이 문서는 현재 LLVM 백엔드의 실제 검증 범위와 남은 debt만 짧게 정리한다.
이전의 AST inventory 중심 문서는 구현 진척을 따라가지 못해 stale해졌고,
현재 상태는 [62_llvm_backend_debt_ledger.md](/mnt/e/PergyraLang/docs/62_llvm_backend_debt_ledger.md)와 함께 읽는 것을 기준으로 한다.

## 현재 판단

- LLVM backend는 `누락돼서 전반적으로 동작 안 하는 상태`가 아니다.
- 실제 경로는 `MIR-led / HIR-assisted hybrid`다.
- ordinary/async function과 subject/class method의 주 경로는 MIR emission을 탄다.
- 일부 표면은 여전히 HIR-assisted debt를 가진다.

## 직접 검증된 범위

기준:

- `make llvm-test-smoke`
- `make test-abi`

### llvm-smoke에서 직접 검증되는 축

- control flow:
  - `while`
  - nested loop
  - `if / else if`
  - `match`
  - `break / continue`
  - recursion
  - nested call
  - `defer`
- async / parallel:
  - `async block`
  - `channel`
  - `select`
  - fairness
  - cancellation
  - generic spawn
- domain / projection:
  - subject projection
  - relation/effect projection sync
  - zone action/effect runtime
  - zone layer projection runtime
  - world derived/composed states
  - nested world member mutation
  - object layer binding
  - subject/class dispatch
- runtime / collections:
  - string/file I/O
  - array/list/queue/map/set
  - lambda expression
  - event system
  - slot / secure slot / device slot

### ABI pipeline에서 직접 검증되는 축

- `llvm/projection_abi`
- `llvm/zone_projection_abi`
- `llvm/intent_trace_abi`
- `llvm/runtime_floor`

즉 다음 표현은 현재 기준으로 틀리다.

- “LLVM에서 while/for, 재귀, if/else, defer, intent, subject method가 동작 안 한다”

더 정확한 표현은 이렇다.

- “LLVM backend에는 fallback debt가 남아 있지만, 위 축들은 이미 회귀 범위에 들어가 있고 동작 안 하는 상태는 아니다.”

## 현재 실제 debt

### 1. MIR-only 미완료

- intent/domain 일부는 여전히 HIR-assisted path를 쓴다
- MIR routine/step sequence가 비어 있으면 LLVM은 hard error로 실패한다
- intent step의 check/eval/meta carrier는 이제 MIR-only로 검증된다

관련 파일:

- [llvm_backend.h](/mnt/e/PergyraLang/src/codegen/llvm_backend.h)
- [llvm_pipeline.c](/mnt/e/PergyraLang/src/codegen/llvm_pipeline.c)
- [llvm_intent.c](/mnt/e/PergyraLang/src/codegen/llvm_intent.c)

### 2. expression-level type exactness debt

- party/vtable dispatch의 hardcoded arg type debt는 제거됐다
- 남은 debt는 domain declaration emission과 intent/domain 내부의 일부 AST-assisted 경로 쪽이다

관련 파일:

- [llvm_expr_call_methods.inc](/mnt/e/PergyraLang/src/codegen/llvm_expr_call_methods.inc)

### 3. escape/local placement debt

- LLVM AST emission path와 hosted method path에는 local sinking이 들어갔다
- 하지만 MIR temporary/storage placement 전체를 다 닫은 상태는 아니다

관련 파일:

- [llvm_mir_emit.c](/mnt/e/PergyraLang/src/codegen/llvm_mir_emit.c)
- [slot_analyzer.c](/mnt/e/PergyraLang/src/semantic/slot_analyzer.c)

### 4. coverage 문서 debt

- 이전 문서가 `미구현`으로 적어 둔 여러 AST 항목은 현재 구현과 맞지 않았다
- 특히 `intent`, `zone`, `relation/effect`, `subject dispatch` 쪽이 stale했다

이 문서는 그 stale inventory를 대체하는 현재 기준 문서다.

## 추천 표현

다음과 같이 적는 것이 맞다.

- LLVM backend: `MIR-led / HIR-assisted hybrid`
- validated:
  - loops
  - recursion
  - if/else
  - defer
  - intent trace
  - subject/class dispatch
- remaining debt:
  - domain/intent HIR-assisted paths
  - remaining AST-assisted expression lowering inside domain/intent
  - MIR-only completion
