# Pergyra 개발 현황

## 완료된 컴포넌트

### 렉서 (Lexer)
- `src/lexer/lexer.h`, `src/lexer/lexer.c`
- 키워드, 연산자, 리터럴, 식별자 인식
- 비동기 키워드 지원: `async`, `await`, `actor`, `channel`, `select`, `spawn`
- 에러 위치 추적

### 파서 (Parser)
- `src/parser/ast.h`, `src/parser/ast.c`, `src/parser/parser.c`, `src/parser/parser_async.c`
- 재귀 하향 파서
- 슬롯/비동기/병렬/역할/파티/월드 구문 파싱
- 제네릭 타입 파라미터, where 절 지원

### 시맨틱 분석 (Semantic Analyzer)
- `src/semantic/type_system.c` -- 타입 시스템 (Primitive, Generic, Constructed, Function, Slot)
- `src/semantic/symbol_table.c` -- 스코프 기반 심볼 테이블
- `src/semantic/type_checker.c` -- 타입 검사, 슬롯 규칙 검증
- `src/semantic/slot_analyzer.c` -- 슬롯 생명주기 분석
- `src/semantic/semantic.c` -- 통합 진입점

슬롯 규칙 검증:
- R1: 슬롯 inner type과 값 타입 일치 확인
- R2: SecureSlot은 토큰 필수
- R3: 토큰은 해당 슬롯과 페어링 확인
- R4: 해제된 슬롯 접근 차단

### C 트랜스파일러 (Code Generator)
- `src/codegen/transpiler.h`, `src/codegen/transpiler.c`
- AST -> C 소스 변환
- 슬롯 변수 타입 추적 및 올바른 `pgy_*` 함수 호출 생성
- `func Main()` -> `int main(void)` 자동 래핑
- `with`, `parallel`, `for`, `while`, `if/else` 지원

### 런타임 (Runtime)
- `src/runtime/pgy_runtime.h` -- 헤더 온리 런타임
- C11 `_Generic` 기반 `pgy_log()` 매크로
- `PGY_SLOT_DEFINE` / `PGY_SECURE_SLOT_DEFINE` 매크로로 6개 타입 인스턴스화
  - Int (int32_t), Long (int64_t), Float (float), Double (double), Bool (bool), String (char*)
- `PGY_PARALLEL_BEGIN/TASK/END` OpenMP 매크로
- `PGY_PANIC`으로 use-after-release, double-release, 잘못된 토큰 감지

### 컴파일러 드라이버
- `src/pgy_driver.c`
- 파이프라인: 파일 읽기 -> 렉서 -> 파서 -> 시맨틱 -> 트랜스파일 -> GCC -> 실행
- 옵션: `--compile`, `--run`, `--tokens`, `--ast`, `-v`, `-o`

## 테스트

| 스위트 | 파일 | 테스트 수 |
|--------|------|-----------|
| 시맨틱 분석 | `src/test_semantic.c` | 29 |
| 트랜스파일러 | `src/test_transpile.c` | 34 |
| 메모리 레이아웃 | `src/test_memory_layout.c` | 44 |
| **합계** | | **107** |

## End-to-End 검증 완료

```
examples/hello.pgy  ->  "Hello, Pergyra!"
examples/slots.pgy  ->  45, 100, 30  (for loop, with 블록, parallel 블록)
```

## 코드 통계

| 컴포넌트 | 파일 수 | 상태 |
|----------|---------|------|
| Lexer | 2 | 완료 |
| Parser + AST | 4 | 완료 |
| Semantic | 8 | 완료 |
| Codegen | 2 | 완료 |
| Runtime | 1 | 완료 |
| Driver | 1 | 완료 |
| Tests | 3 | 완료 |

## 향후 작업

### 단기
- 패턴 매칭 트랜스파일 지원
- 역할(Role)/파티(Party) 시스템 코드 생성
- 에러 메시지 개선

### 중기
- 표준 라이브러리 기초
- 비동기 런타임 통합 (Fiber, Channel)
- 최적화 패스

### 장기
- LLVM 백엔드
- 패키지 시스템
- LSP 서버 (IDE 지원)
