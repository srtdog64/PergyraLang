# Parallel Execution Relayout Board

마지막 업데이트: 2026-04-10

이 문서는 현재 `spawn/select/async` surface를 `parallel` 아래 실행 계약으로 재정렬하는 작업 보드다.

## 1. 목표

최종 계층:

1. `parallel`
2. `spawn`
3. `async`
4. `await`
5. `select`
6. `channel`
7. `cancel`

고정 원칙:

- `parallel`은 core execution primitive
- `spawn/select/async`는 그 아래 surface
- `intent`는 orchestration core이며, execution primitive와 직접 혼합하지 않음

## 2. 현재 상태

| 항목 | 현재 상태 | 판단 | 다음 작업 |
|---|---|---|---|
| `parallel` | 구현/문서 존재 | 코어로 승격 필요 | 코어 primitive로 문서 고정 |
| `spawn` | 구현됨 | task-producing surface | `parallel` 아래로 설명 재배치 |
| `async` | 구현됨 | suspension surface | 병렬성과 분리해 설명 |
| `await` | 구현됨 | join/completion surface | async completion 계약으로 정리 |
| `select` | 구현됨 | readiness arbitration | parallel/channel 하위로 정리 |
| channel convenience | 구현됨 | dataflow support | parallel/select 문맥으로 재배치 |
| cancellation | 구현됨 | execution control | parallel tree/cancellation chain으로 정리 |

## 2.A 진행률

현재 라운드 기준 추정치다. 구현, 문서, 테스트 표면, 용어 정렬을 함께 본 값이다.

| 항목 | 진행률 | 상태 메모 |
|---|---:|---|
| `parallel` 코어 실행 primitive 문서화 | 90% | 정책 문서와 계약 표는 고정됨. 남은 것은 일부 주변 문서 정리 |
| `spawn/select/async` 재배치 문서화 | 75% | 핵심 문서와 보드는 정리됨. intent/paradigm 문서 쪽이 더 남음 |
| semantic `parallel context` 용어 정렬 | 82% | 핵심 진단과 주석 반영, SecureSlot/DeviceSlot 병렬 문맥 거부 추가 |
| 테스트 분류 재작성 | 68% | suite 라벨 재정렬, canonical parallel family/context 테스트를 별도 파일로 분리 시작 |
| backend/runtime 설명 재정렬 | 35% | 정책은 잡혔지만 codegen/runtime 문서와 주석은 아직 부분적 |
| `parallel` 문맥 전용 규칙 확장 | 32% | slot conflict + SecureSlot/DeviceSlot 직렬화 규칙 추가. lattice/cancellation은 아직 partial |

## 2.B 현재 프로세스

1. 용어 고정
   - `parallel`은 core execution primitive
   - `async`는 suspension surface
   - `spawn`은 task-producing surface
   - `select`는 readiness arbitration surface
2. 문서 재배열
   - core policy
   - 실행 family 보드
   - async/concurrency 문서 재배열
3. semantic 진단 정렬
   - `parallel context` 용어 적용
4. 테스트 표면 재작성
   - `parallel-safe`
   - `parallel-rejected`
   - `async-suspension`
   - `select-readiness`
5. 남은 단계
   - 테스트 파일 구조 자체 분리
   - effect/capability 규칙을 `parallel context` 기준으로 확장
   - runtime/backend 설명을 같은 family 기준으로 재정렬

## 3. 문서 재배치 작업

### 3.1 완료

- `parallel`을 코어 실행 primitive로 고정
  - [53_parallel_core_policy.md](/mnt/e/PergyraLang/docs/53_parallel_core_policy.md)
- [05_async_concurrency.md](/mnt/e/PergyraLang/docs/05_async_concurrency.md)를 `parallel -> spawn -> async/await -> select/channel -> cancellation` 순서로 재배열
- semantic/transpile 테스트 섹션 라벨을 `parallel execution` 기준으로 정렬 시작
- runtime/effects 스위트에 중복으로 남아 있던 `parallel`/`async`/`select` canonical 테스트 제거
- semantic/transpile에 canonical `parallel family` / `parallel context` 전용 include 파일 추가

### 3.2 다음 문서 정리 대상

1. [05_async_concurrency.md](/mnt/e/PergyraLang/docs/05_async_concurrency.md)
   - `parallel -> spawn -> async/await -> select/channel -> cancellation` 순서로 이미 재배열됨
   - 남은 것은 주변 문서와 용어를 같은 계층으로 맞추는 작업

2. [34_intent_oriented_paradigm.md](/mnt/e/PergyraLang/docs/34_intent_oriented_paradigm.md)
   - intent clause가 execution primitive를 직접 담지 않는다는 점을 유지
   - `parallel`을 intent 바깥의 core execution layer로 명시하는 정리는 반영됨
   - 남은 것은 주변 문서와 테스트 이름의 추가 정렬

3. [37_compiler_contracts.md](/mnt/e/PergyraLang/docs/37_compiler_contracts.md)
   - 실행 계약 표에 `parallel / spawn / async / await / select / channel / cancel`을 반영 완료
   - 남은 것은 backend/runtime 설명과 세부 진단 정렬

## 4. 구현 정렬 작업

### 4.1 semantic

현재:

- `parallel` slot conflict 검출 존재
- helper body 경유 conflict detection 존재
- cancellation/cooperative runtime surface 존재

다음:

1. `parallel context`를 semantic contract 용어로 명시
2. `parallel` 허용/거부 규칙을 `spawn/select/async`보다 상위에 재정렬
3. capability/effect 규칙에서 `parallel` 문맥을 별도 검사 축으로 승격

진입점:

- [slot_analyzer.c](/mnt/e/PergyraLang/src/semantic/slot_analyzer.c)
- [type_checker_flow.c](/mnt/e/PergyraLang/src/semantic/type_checker_flow.c)

### 4.2 MIR / backend

현재:

- `parallel`, `spawn`, `select`, `async block` lowering이 각각 존재

다음:

1. backend 설명과 내부 주석에서 `parallel`을 상위 primitive로 정리
2. `spawn/select` lowering helper를 `parallel execution family`로 문서화
3. 성능 계측도 `parallel family` 기준으로 묶을 수 있게 정리

진입점:

- [transpiler.c](/mnt/e/PergyraLang/src/codegen/transpiler.c)
- [llvm_stmt.c](/mnt/e/PergyraLang/src/codegen/llvm_stmt.c)
- [llvm_expr.c](/mnt/e/PergyraLang/src/codegen/llvm_expr.c)

### 4.3 runtime

현재:

- channel/select/parallel/cancellation runtime 존재

다음:

1. runtime 문서에서도 `parallel`을 최상위 실행 primitive로 설명
2. task tree / cancellation chain / select fairness / channel backpressure를 한 execution family로 묶기

## 5. 테스트 재정렬

현재 스위트는 기능별로 분산되어 있다. 다만 canonical `parallel`/`async`/`select` 테스트는 이미 중복 제거를 시작했다.

다음 목표 분류:

1. sequential baseline
2. parallel-safe
3. parallel-rejected
4. async-suspension
5. select-readiness
6. cancellation-propagation

현재 관련 진입점:

- [test_semantic_async.inc](/mnt/e/PergyraLang/src/tests/semantic/test_semantic_async.inc)
- [test_semantic_runtime.inc](/mnt/e/PergyraLang/src/tests/semantic/test_semantic_runtime.inc)
- [test_transpile_domain_async.inc](/mnt/e/PergyraLang/src/tests/transpile/test_transpile_domain_async.inc)
- [llvm_smoke.sh](/mnt/e/PergyraLang/tests/llvm_smoke.sh)

## 6. 남은 빈 부분

아직 partial인 것:

- `parallel` 문맥 전용 effect/capability 규칙
- structured cancellation scope/lattice
- `parallel` 중심 문서 재배열
- backend/perf 설명의 family-level 정리

추가 메모:

- semantic 쪽은 이제 `parallel context` 용어, slot conflict 진단, secure/token 직렬화 거부까지 정렬됨
- canonical 테스트는 `parallel_family_semantics`, `parallel_context_semantics`, `parallel_family_emit` 기준으로 분리됐다

## 7. 한 줄 요약

지금 구현은 이미 `parallel` family를 상당 부분 가지고 있다.
남은 일은 기능 추가보다, 이를 문서/semantic/backend/test에서 “코어 primitive” 기준으로 다시 정렬하는 것이다.
