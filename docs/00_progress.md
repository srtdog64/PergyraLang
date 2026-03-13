# Pergyra -- 현재 진행 상황

마지막 업데이트: 2026-03-13

## 구현 완료

### 컴파일러 파이프라인

```
.pgy --> Lexer --> Parser --> AST --> Semantic --> C Transpiler --> GCC --> Binary
```

전체 파이프라인이 동작하며, `.pgy` 파일에서 네이티브 바이너리까지 end-to-end 실행 확인됨.

### 렉서 (Lexer)
- 키워드, 연산자, 리터럴, 식별자 인식
- 비동기 키워드: `async`, `await`, `actor`, `channel`, `select`, `spawn`
- 에러 위치 추적 (행/열)

### 파서 (Parser)
- 재귀 하향 파서
- 지원 구문: 함수 선언, 변수 선언, if/else, for, while, parallel, with
- 슬롯/비동기/병렬/역할/파티/월드 구문 파싱
- 제네릭 타입 파라미터, where 절

### 시맨틱 분석 (Semantic Analyzer)
- 타입 시스템: Primitive, Generic, Constructed, Function, Slot
- 스코프 기반 심볼 테이블
- 타입 검사 및 슬롯 규칙 검증 (R1-R4)
- 슬롯 생명주기 분석 (claim/write/read/release)

### C 트랜스파일러 (Code Generator)
- AST -> C 소스 변환
- 슬롯 변수 타입 추적 (`Slot<Int>`, `Slot<String>` 등)
- `func Main()` -> `int main(void)` 자동 래핑
- 빌트인 함수 변환: ClaimSlot, Write, Read, Release, Log
- with/parallel/for/while/if-else 지원

### 런타임
- 헤더 온리 `pgy_runtime.h`
- C11 `_Generic` 기반 타입 디스패치
- 6개 슬롯 타입: Int, Long, Float, Double, Bool, String
- SecureSlot 토큰 기반 접근 제어
- PGY_PANIC으로 안전성 보장 (use-after-release, double-release 감지)
- OpenMP 기반 parallel 매크로

### 컴파일러 드라이버
- `src/pgy_driver.c`
- 옵션: `--compile`, `--run`, `--tokens`, `--ast`, `-v`, `-o`
- Windows/Linux 크로스 플랫폼

### 테스트
- 시맨틱 분석: 29개
- 트랜스파일러: 34개
- 메모리 레이아웃: 44개
- **총 107개 테스트 통과**

## End-to-End 검증 완료

| 예제 | 출력 | 상태 |
|------|------|------|
| `examples/hello.pgy` | "Hello, Pergyra!" | 통과 |
| `examples/slots.pgy` | 45, 100, 30 | 통과 |
| `examples/minimal.pgy` | 42 | 통과 |

## 미구현 (TODO)

- 어셈블리 최적화 런타임
- 비동기 런타임 (Fiber, M:N 스케줄러, Channel)
- Effect System
- LLVM 백엔드
- JVM 연동
- 패턴 매칭 코드 생성
- Role/Party/World 시스템 코드 생성
- 표준 라이브러리
- 패키지 매니저 / 모듈 시스템
- WebAssembly 타겟
- LSP 서버
- 디버거

## 문서 구조

```
docs/
  00_syntax.md              -- 문법 레퍼런스
  00_engine_core_spec.md    -- 엔진 코어 스펙
  00_progress.md            -- 현재 진행 상황 (이 문서)
  01_grammar.md             -- 문법 정의
  02_naming_conventions.md  -- 네이밍 규칙
  03_security_mode_design.md
  04_generic_design.md
  05_async_concurrency.md
  06_structured_comments.md
  07_error_handling.md
  08_module_system.md
  09_pattern_matching.md
  10_role_interface_design.md
  11_party_system_design.md
  12_party_compiler_integration.md
  13_world_systemic_architecture.md
  14_semantic_analyzer_design.md
  15_compiler_security_modifications.md
  16_security_implementation_report.md
  17_development_status.md
```
