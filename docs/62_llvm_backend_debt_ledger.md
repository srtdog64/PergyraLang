# LLVM Backend Debt Ledger

마지막 업데이트: 2026-04-11

목적:

- “LLVM에서 핵심 축이 대거 누락됐다”는 식의 과장된 표현 대신
- 현재 검증된 범위와 실제 남은 debt를 분리해서 기록한다.

## 1. 검증된 사실

직접 확인:

- `make llvm-test-smoke` 통과
- `make test-abi` 통과

검증된 축:

- while / nested loop
- if / else-if
- recursion
- defer
- intent runtime trace
- subject/class dispatch
- projection / relation / effect sync
- zone/world runtime mutation
- world cross-query / derived state / composed state
- generic collection runtime path (`List/Set/Queue/HashMap`)
- select / cancel / async block

즉 다음 주장은 현재 로컬 상태와 맞지 않는다.

- “LLVM backend에서 while/for, 재귀, if/else, defer, intent, subject method가 동작 안 한다”

## 2. 왜 그런 말이 나왔을 수 있나

원인 후보는 세 가지다.

### 2.1 stale 문서

이전 coverage 문서는 구현 완료 이후에도 오래 갱신되지 않아
`intent`, `zone`, `relation/effect` 일부를 미구현처럼 읽히게 만들었다.

### 2.2 fallback debt와 미구현의 혼동

LLVM backend는 아직 `MIR-only complete`가 아니다.
하지만 fallback debt가 있다는 사실과, 기능이 통째로 안 된다는 사실은 다르다.

### 2.3 이전 세션의 부분 수정 목록

이전 세션에서 while/for/if-else/defer/intent/subject method 관련 수정이 실제로 있었더라도,
현재 상태에서 중요한 질문은 “그 수정이 지금 반영돼서 회귀가 통과하느냐”다.
지금 기준으로는 통과한다.

## 3. 현재 실제 debt

### 3.1 MIR-only completion debt

- domain / intent 일부는 HIR-assisted
- MIR routine/sequence가 없으면 LLVM은 hard error
- intent step의 `pre/guard/post/expect/invariant/on/subintent/compensate`
  및 `zone/who/transfer` metadata carrier는 이제 MIR-only로 강제된다

근거:

- [llvm_backend.h](/mnt/e/PergyraLang/src/codegen/llvm_backend.h)
- [llvm_pipeline.c](/mnt/e/PergyraLang/src/codegen/llvm_pipeline.c)
- [llvm_intent.c](/mnt/e/PergyraLang/src/codegen/llvm_intent.c)

### 3.2 expression type exactness debt

- party/vtable dispatch의 hardcoded arg type debt는 제거됐다
- 남은 debt는 domain declaration emission과 intent/domain 내부의 일부 AST-assisted 해석 쪽이다
- collections도 LLVM 경로 자체는 있으나, semantic/type exactness와 coverage 균형이 아직 완전히 닫히지 않았다

근거:

- [llvm_expr_call_methods.inc](/mnt/e/PergyraLang/src/codegen/llvm_expr_call_methods.inc)

### 3.3 local placement / escape debt

- AST/method path local sinking은 시작됨
- MIR temporary placement 완결은 아직 아님

근거:

- [llvm_mir_emit.c](/mnt/e/PergyraLang/src/codegen/llvm_mir_emit.c)
- [slot_analyzer.c](/mnt/e/PergyraLang/src/semantic/slot_analyzer.c)

## 4. “11건 누락” 주장과 현재 상태 대조

| 주장 축 | 현재 상태 |
|---|---|
| while/for loop | `llvm-smoke`에서 직접 PASS |
| recursion | `llvm-smoke`에서 직접 PASS |
| if/else | `llvm-smoke`에서 직접 PASS |
| defer | `llvm-smoke`에서 직접 PASS |
| intent | `llvm-smoke`와 `test-abi`에서 PASS |
| subject method / dispatch | `llvm-smoke`에서 직접 PASS |
| nested call | `llvm-smoke`에서 직접 PASS |
| world/zone runtime | `llvm-smoke`에서 직접 PASS |
| world cross-query / derived / composed state | `llvm-smoke`에서 직접 PASS |
| generic collections (`List/Set/Queue/HashMap`) | `llvm-smoke` 경로 존재, semantic/coverage debt 잔존 |
| top-level executable main-wrapper | synthetic executable MIR routine + `llvm-smoke` / `test-abi` 통과 |

결론:

- blanket claim은 틀림
- fallback debt가 남았다는 형태로 다시 써야 맞음

## 5. 지금부터의 기준

LLVM 관련 상태는 앞으로 이렇게 적는다.

1. `validated by llvm-smoke`
2. `validated by ABI pipeline`
3. `MIR-led with HIR fallback debt`
4. `remaining debt`는 구체 항목만 적기

이 기준을 쓰면 “부분 debt”와 “기능 누락”을 섞지 않게 된다.
