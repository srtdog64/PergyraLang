# Pergyra — 현재 진행 상황

마지막 업데이트: 2026-04-03

## 컴파일러 파이프라인

```text
.pgy → Lexer → Parser → Semantic → HIR → LLVM Backend → Object → Binary
                                     └→ C Backend     → C → GCC/Clang
```

- LLVM이 기본 백엔드
- C 백엔드는 폴백/reference 경로

## 현재 구현 요약

### 문법/시맨틱
- `let`, `func`, `async`, `spawn/await`, `if/for/while/match/select`
- `slot/view/move`, `SecureSlot`, `DeviceSlot`, `QubitSlot`
- `ability/role/party/systemic/world`, `event`, `actor`
- `import/export/namespace`, `extern "C"`
- `RemoteFuture<T>`의 `await` 결과는 `Result<T>`

### 백엔드/런타임
- C/LLVM 백엔드 둘 다 동작
- coroutine runtime (POSIX ucontext + Windows Fiber)
- channel/parallel/select 동작
- Result/enum/array/string built-in 경로 동작

### 테스트
`make test-all` 기준:
- semantic 197, transpile 141, memory 54, concurrency 2, HIR 3

추가 회귀:
- `llvm-test-smoke`
- `llvm-test-backend-compare`
- `stdlib-test-smoke`, `module-test-smoke`, `example-test-smoke`

## 미완성 / 다음 단계

- orchestration 고도화 (select 공정성, timeout, cancellation)
- effect system 2단계 (선언적 effect + mismatch 진단)
- stable stdlib surface 고정
- 패키지 매니저 / WebAssembly / 디버거
