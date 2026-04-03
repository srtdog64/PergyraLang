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
- enum/result 패턴 shorthand: `Some(x)`와 `.Some(x)` 둘 다 파싱됨. `case .Ok(v):`, `return .None;` 같은 문서 표기도 현재 파서 기준으로 허용됨
- `Option<T>` 표면: `Some/None`, `IsSome/IsNone`, `UnwrapOption`, `match` destructuring이 semantic/C/LLVM 경로에 연결됨
- `match` 시맨틱: `Option/Result/tagged enum` destructuring 바인딩과 제한된 exhaustiveness check가 동작함
- `match` 품질 진단: duplicate variant case와 redundant default를 warning으로 보고함
- `@effects` 계약: structured comment 기반 선언이 있으면 body inferred effect와 mismatch를 semantic error로 보고함
- 채널 convenience surface: `TryRecv -> Option<T>`, `RecvTimeout -> Option<T>`, `TrySend/SendTimeout -> Bool`이 C/LLVM 경로에 연결됨
- 채널 backpressure observation surface: `ChannelLength/ChannelCapacity/ChannelSpace -> Int`, `ChannelFull/ChannelClosed -> Bool`이 C/LLVM/runtime에 연결됨
- 현재 `TryRecv/RecvTimeout/TrySend/SendTimeout`은 plain-value channel 중심이며 movable resource channel은 의도적으로 제외됨

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
- channel non-blocking/timeout helper 지원
- `select`는 round-robin 시작 인덱스로 readiness를 검사해 단순 고정 순서 starvation을 줄임
- task cancellation surface: `Cancel(task)` / `IsCancelled()`가 C/LLVM/runtime에 연결됨
- spawned descendant는 부모 task의 cancellation chain을 상속함
- 현재 cancellation은 cooperative/best-effort이며, preemptive interruption은 아직 아님

### 도구
- LSP 서버 구현 존재 (`src/lsp/pgy_lsp.c`)

## 테스트 현황

2026-04-03 현재 직접 확인한 기준:

| 스위트 | 결과 |
|---|---|
| concurrency | 4 passed |
| semantic | 257 passed |
| transpile | 172 passed |
| llvm smoke | 통과 (`cancel_propagation`, `channel_pressure` 포함) |

추가 회귀:
- `make test-semantic` 통과
- `make llvm-test-smoke` 통과 (async, select, tagged-union, RemoteFuture, device slot, generics, channel pressure 등)

## 남은 주요 작업

### 단기
- orchestration 고도화 (`select` 공정성, timeout, cancellation, backpressure)
- partial 완료: channel timeout/non-blocking built-in surface 추가
- partial 완료: channel backpressure observation surface (`ChannelLength`, `ChannelCapacity`, `ChannelSpace`, `ChannelFull`, `ChannelClosed`)
- partial 완료: `select` round-robin fairness
- partial 완료: cooperative cancellation surface (`Cancel`, `IsCancelled`)
- partial 완료: spawned descendant cancellation propagation
- 완료: semantic O2 crash root-cause 정리 및 회귀 고정 (`Channel*` diagnostic format bug)
- effect system 2단계 (선언적 effect, mismatch 진단)
- 안정화 문서 갱신 및 표면 문법 정리

### 중기
- stable stdlib surface 고정
- toolchain 강화 (formatter, LSP 진단 품질, debugger)

### 장기
- WebAssembly 타겟
- 패키지 매니저
- 성능 최적화 (LLVM 쪽 집중)
