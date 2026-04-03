# Pergyra 언어 상태 평가

마지막 업데이트: 2026-04-03

## 요약

Pergyra는 **실행 가능한 실험 언어 알파** 단계다.  
문법과 런타임이 실제로 동작하며 C/LLVM 양쪽에서 테스트가 돌아간다.  
다만 “범용 언어”나 “상용 안정성”을 기대하기엔 아직 이르다.

## 현재 강점

- Slot/Resource/Orchestration 중심 의미론이 코드와 런타임에 연결됨
- `async/await`, `channel`, `select`, `parallel`의 실행 경로 존재
- `RemoteFuture<T> → await → Result<T>` 규칙이 시맨틱/코드젠에 반영됨
- Role/Party/World 문법과 코드젠이 C/LLVM 양쪽에 존재
- 문서에 쓰던 `.Some/.None/.Ok/.Err` shorthand가 현재 파서에도 반영됨

## 현재 한계

- 문서/설계가 많아 표면이 커 보이지만, 실제로는 일부 영역이 “supported but evolving”
- 클래식 OOP 계층(상속, super, 제네릭 클래스)은 미지원
- 패키지 매니저, WASM, 디버거 등 생태계 영역은 미완성

## 2026-04-03 기준 확인된 상태

- `make test-all` 통과
- `make llvm-test-smoke` 통과
- `make stdlib-test-smoke` 통과
- `make module-test-smoke` 통과
- `make llvm-test-backend-compare` 통과
- `make example-test-smoke` 통과

## 다음 기준

1. 문서와 구현 표면의 1:1 정렬
2. orchestration/slot 의미론 고정
3. stable stdlib surface 확정
4. toolchain 품질 개선 (LSP/formatter/debugger)

## 단계 결론

Pergyra는 “돌아가는 철학 실험”을 넘어, **자원 의미론을 가진 실행 가능한 언어 프로토타입**이다.  
다만 아직 베타 수준의 안정성과 생태계 준비가 필요하다.
