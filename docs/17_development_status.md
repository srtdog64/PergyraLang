# Pergyra 개발 현황

마지막 업데이트: 2026-04-03

## 요약

- 컴파일러 파이프라인은 `Lexer → Parser → Semantic → HIR → Backend`로 고정됨.
- LLVM이 기본 백엔드이며, C 백엔드는 폴백/reference 경로로 유지됨.
- async/await는 coroutine runtime을 통해 동작하며, channel/select/parallel이 동작함.

## 구현된 컴포넌트

### 렉서 / 파서
- 파서는 파일 분할 구조: `parser.c`, `parser_expr.c`, `parser_stmt.c`, `parser_decl.c`, `parser_domain.c`, `parser_async.c`
- 문법 표면: `let`, `func`, `async`, `spawn/await`, `if/for/while/match/select`, `slot/view/move`, `ability/role/party/systemic/world`, `event`, `actor`, `import/export/namespace`

### 시맨틱
- 타입 시스템 + 슬롯 규칙 + move/consume 추적
- anchored handle(`Slot/SecureSlot/DeviceSlot`)와 movable handle(`QubitSlot`)의 경계 규칙 반영
- `RemoteFuture<T>`의 `await`가 `Result<T>`를 반환하도록 강제

### 백엔드
- C 백엔드: reference/fallback, 스모크와 트랜스파일 테스트로 유지
- LLVM 백엔드: 기본 실행 경로, backend-compare 및 llvm-smoke로 회귀 체크

### 런타임
- slot/secure slot/device slot/qubit slot 런타임
- coroutine runtime (POSIX ucontext + Windows Fiber)
- channel/parallel/select 지원

### 도구
- LSP 서버 구현 존재 (`src/lsp/pgy_lsp.c`)

## 테스트 현황

`make test-all` 기준:

| 스위트 | 결과 |
|---|---|
| semantic | 197 passed |
| transpile | 141 passed |
| memory | 54 passed |
| concurrency | 2 passed |
| HIR | 3 passed |
| lexer/parser | 통과 |

추가 회귀:
- `make llvm-test-smoke` (async, select, tagged-union, RemoteFuture, device slot 등)
- `make llvm-test-backend-compare` (대표 예제 C/LLVM 비교)
- `make stdlib-test-smoke`, `make module-test-smoke`, `make example-test-smoke`

## 남은 주요 작업

### 단기
- orchestration 고도화 (`select` 공정성, timeout, cancellation, backpressure)
- effect system 2단계 (선언적 effect, mismatch 진단)
- 안정화 문서 갱신 및 표면 문법 정리

### 중기
- stable stdlib surface 고정
- toolchain 강화 (formatter, LSP 진단 품질, debugger)

### 장기
- WebAssembly 타겟
- 패키지 매니저
- 성능 최적화 (LLVM 쪽 집중)
