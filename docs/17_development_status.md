# Pergyra 개발 현황

마지막 업데이트: 2026-04-04

## 요약

- 컴파일러 파이프라인은 `Lexer → Parser → Semantic → HIR → Backend`로 고정됨.
- LLVM이 기본 백엔드이며, C 백엔드는 폴백/reference 경로로 유지됨.
- async/await는 coroutine runtime을 통해 동작하며, channel/select/parallel이 동작함.
- 최종 목표 계층은 `ability -> role -> party -> relation -> effect -> zone -> world`로 문서화됨.
- 최종 존재론은 `struct`와 `subject`를 분리하며, 현재 surface는 `subject`와 `class`를 같은 subject declaration으로 해석함.
- `dto`는 현재 `struct` 호환 projection value declaration alias로 동작함.
- `ToObject(TargetStruct, subjectBinding)` 최소 passive projection surface가 semantic/C/LLVM backend에 반영됨.
- `ToDto(TargetDto, subjectBinding)` 최소 projection surface가 semantic/C/LLVM backend에 반영됨.
- `entity`는 코어 언어 존재론에 넣지 않고, 필요하면 프레임워크/도메인 용어로만 취급함.
- 상위 레이어로 갈수록 더 덜 구속적인 문맥 계층이라는 원칙을 채택함.

## 구현된 컴포넌트

### 렉서 / 파서
- 파서는 파일 분할 구조: `parser.c`, `parser_expr.c`, `parser_stmt.c`, `parser_decl.c`, `parser_domain.c`, `parser_async.c`
- 문법 표면: `let`, `func`, `async`, `spawn/await`, `if/for/while/match/select`, `slot/view/move`, `subject/class`, `ability/role/party/relation/effect/zone/systemic/world`, `event`, `actor`, `import/export/namespace`
- 현재 domain 표면은 `ability/role/party/systemic/world`에 더해 `relation/effect/zone`의 최소 body surface까지 parser/semantic에 연결됨
- `relation`, `effect`, `zone`은 `subject slot` / `object slot` / `shared` / `func`까지의 최소 표면이 parser/semantic에 연결됨
- `relation`, `effect`, `zone`의 domain slot은 optional initializer를 받아 `object slot view: PlayerView = ToObject(PlayerView, player)` 같은 local projection wiring을 직접 표현할 수 있음
- `relation`, `effect`는 optional `for ...` header로 subject endpoint/target을 declaration header에 고정할 수 있음
- `zone`은 `relation slot` / `effect slot`으로 overlay type을 참조할 수 있고, `world`는 `zone` slot으로 하위 지역 규칙을 참조할 수 있음
- `zone`은 `apply effectSlot to targetSlot`, `detach effectSlot from targetSlot`으로 local effect attachment/detachment를 최소 surface로 표현할 수 있음
- `zone`은 `link relationSlot between left, right`, `unlink relationSlot between left, right`로 local relation wiring을 최소 surface로 표현할 수 있음
- `zone`의 `apply/detach`는 `effect` declaration의 subject target 수와 타입을 검사함
- `zone`의 `link/unlink`는 `relation` declaration의 subject endpoint 수와 타입을 검사함
- `zone`은 현재 subject가 0개이거나 4개를 크게 넘는 형태에 대해 운영 lint를 냄
- `relation/effect/zone`은 여전히 계층 간 구조적 의미론이 더 필요함
- `actor`는 현재 별도 surface가 있지만 장기 철학에서는 subject의 실행 profile/sugar로 정리할 계획
- `object`는 별도 코어 타입이 아니라, subject가 transfer/DTO/view 문맥에서 수동적으로 해석된 모습으로 정리됨
- enum/result 패턴 shorthand: `Some(x)`와 `.Some(x)` 둘 다 파싱됨. `case .Ok(v):`, `return .None;` 같은 문서 표기도 현재 파서 기준으로 허용됨
- `Option<T>` 표면: `Some/None`, `IsSome/IsNone`, `UnwrapOption`, `match` destructuring이 semantic/C/LLVM 경로에 연결됨
- `match` 시맨틱: `Option/Result/tagged enum` destructuring 바인딩과 제한된 exhaustiveness check가 동작함
- `match` 품질 진단: duplicate variant case와 redundant default를 warning으로 보고함
- `with effects ...` / `/// @effects ...` 계약: 선언이 있으면 body inferred effect와 mismatch를 semantic error로 보고함
- `Box<T>` explicit handle surface: `Box`, `BoxGet`, `BoxSet`, `BoxDrop`, `BoxIsValid`
- `Box<class>`는 현재 object handle 경로로 허용되며, plain class value parameter/return 제한을 우회하는 명시적 저장/전달 표면으로 사용 가능
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
| semantic | 272 passed |
| transpile | 181 passed |
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
- partial 완료: source-level `with effects ...` signature surface
- partial 완료: `Box<class>` explicit handle surface (`BoxGet/BoxSet/BoxDrop/BoxIsValid`)
- effect system 2단계 (더 정교한 effect lattice, call-site contract)
- relation/effect/zone declaration 이후의 구조적 의미론 고도화
- 안정화 문서 갱신 및 표면 문법 정리

### 중기
- stable stdlib surface 고정
- toolchain 강화 (formatter, LSP 진단 품질, debugger)

### 장기
- WebAssembly 타겟
- 패키지 매니저
- 성능 최적화 (LLVM 쪽 집중)
