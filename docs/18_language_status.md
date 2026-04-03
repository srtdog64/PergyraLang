# Pergyra 언어 상태 평가

마지막 업데이트: 2026-04-04

## 요약

Pergyra는 **실행 가능한 실험 언어 알파** 단계다.  
문법과 런타임이 실제로 동작하며 C/LLVM 양쪽에서 테스트가 돌아간다.  
다만 “범용 언어”나 “상용 안정성”을 기대하기엔 아직 이르다.

## 현재 강점

- Slot/Resource/Orchestration 중심 의미론이 코드와 런타임에 연결됨
- `async/await`, `channel`, `select`, `parallel`의 실행 경로 존재
- `RemoteFuture<T> → await → Result<T>` 규칙이 시맨틱/코드젠에 반영됨
- `Option<T>` + `Some/None` + `match` destructuring이 시맨틱/C/LLVM에 반영됨
- 채널의 non-blocking/timeout convenience surface가 추가됨: `TryRecv`, `RecvTimeout`, `TrySend`, `SendTimeout`
- 채널의 backpressure observation surface가 추가됨: `ChannelLength`, `ChannelCapacity`, `ChannelSpace`, `ChannelFull`, `ChannelClosed`
- `select`가 fixed-order readiness check에서 round-robin 시작 인덱스 기반 검사로 올라옴
- `Cancel(task)` / `IsCancelled()` cooperative cancellation surface가 C/LLVM/runtime에 연결됨
- spawned descendant가 부모 cancellation chain을 상속함
- `match`의 제한된 exhaustiveness check가 `Option/Result/enum`에 반영됨
- `match`의 redundant branch 경고가 `Option/Result/enum`에 반영됨
- `with effects ...` / `/// @effects ...` 선언과 body inferred effect 사이의 mismatch 진단이 반영됨
- `Box<T>` explicit handle surface와 `Box<class>` object-handle 경로가 semantic/C backend에 반영됨
- Role/Party/World 문법과 코드젠이 C/LLVM 양쪽에 존재
- 장기 목표 계층 `ability -> role -> party -> relation -> effect -> zone -> world`가 문서상 고정됨
- 장기 존재론 `struct` vs `subject` 분리와 `actor = subject profile` 방향이 문서상 고정됨
- `subject` keyword alias가 parser surface에 반영되어 `subject`와 `class`가 같은 선언으로 파싱됨
- `entity`는 코어 존재론 바깥의 프레임워크 어휘로 밀어두고, `object`는 subject의 수동 해석 모드로 정리됨
- 문서에 쓰던 `.Some/.None/.Ok/.Err` shorthand가 현재 파서에도 반영됨

## 현재 한계

- 문서/설계가 많아 표면이 커 보이지만, 실제로는 일부 영역이 “supported but evolving”
- `relation`, `zone`, 구조적 `effect`는 목표 계층으로 정의됐지만 아직 stable syntax/semantics는 아님
- `subject` keyword는 alias로 올라왔지만, `class`와의 장기 alias/deprecation policy는 아직 미정
- 클래식 OOP 계층(상속, super)은 미지원
- 패키지 매니저, WASM, 디버거 등 생태계 영역은 미완성
- backpressure는 관측 surface까지는 올라왔지만, bounded policy/backpressure protocol 자체는 아직 미완성
- cancellation은 cooperative + descendant propagation 수준, fairness는 round-robin 시작 인덱스 수준까지 올라온 상태

## 2026-04-03 기준 확인된 상태

- `make test-semantic` 통과
- `make test-transpile` 통과
- `make llvm-test-smoke` 통과

## 다음 기준

1. 문서와 구현 표면의 1:1 정렬
2. orchestration/slot 의미론 고정
3. stable stdlib surface 확정
4. toolchain 품질 개선 (LSP/formatter/debugger)

## 단계 결론

Pergyra는 “돌아가는 철학 실험”을 넘어, **자원 의미론을 가진 실행 가능한 언어 프로토타입**이다.  
다만 아직 베타 수준의 안정성과 생태계 준비가 필요하다.
