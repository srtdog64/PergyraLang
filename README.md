# Pergyra Programming Language

> 메모리를 슬롯으로, 실행을 의도로 -- 슬롯 기반 시스템 프로그래밍 언어

## 개요

Pergyra는 포인터 대신 **슬롯 기반 메모리 관리**를 채택한 시스템 프로그래밍 언어입니다.
`.pgy` 소스를 C로 트랜스파일하여 네이티브 바이너리를 생성합니다.

```
.pgy  -->  Lexer  -->  Parser  -->  Semantic  -->  C Transpiler  -->  GCC  -->  Binary
```

### 핵심 특징

- **슬롯 기반 메모리**: 포인터 없이 `Slot<T>`로 타입 안전한 메모리 관리
- **보안 슬롯**: `SecureSlot<T>`에 토큰 기반 접근 제어
- **내장 병렬성**: `parallel` 블록으로 선언적 병렬 처리
- **스코프 기반 해제**: `with` 블록으로 자동 메모리 해제

## 빠른 시작

### 필요 조건

- GCC (C11 지원)
- GNU Make

### 빌드 및 실행

```bash
# 전체 빌드
make all

# Hello World 실행
./bin/pgy examples/hello.pgy --compile --run -v

# 슬롯 데모 실행
./bin/pgy examples/slots.pgy --compile --run -v
```

## 문법 예제

```pergyra
func Main() -> Void {
    // 슬롯 기반 메모리 관리
    let msg: Slot<String> = ClaimSlot<String>();
    Write(msg, "Hello, Pergyra!");
    Log(Read(msg));
    Release(msg);
}
```

```pergyra
// 스코프 기반 자동 해제
with slot<Int> as counter {
    Write(counter, 100);
    Log(Read(counter));
}  // counter 자동 해제

// 병렬 처리
parallel {
    Write(a, 10);
    Write(b, 20);
}

// 보안 슬롯
let (slot, token) = ClaimSecureSlot<Int>();
Write(slot, 42, token);   // 토큰 필요
Release(slot, token);
```

## 프로젝트 구조

```
PergyraLang/
  src/
    lexer/          # 토크나이저
    parser/         # AST 생성
    semantic/       # 타입 검사, 슬롯 분석
    codegen/        # C 트랜스파일러
    runtime/        # pgy_runtime.h (슬롯 매크로)
    pgy_driver.c    # 컴파일러 드라이버
  examples/         # .pgy 예제 파일
  docs/             # 언어 설계 문서
  Makefile
```

## 테스트

```bash
# 전체 테스트
make test-all

# 개별 테스트
make test           # 렉서
make test-parser    # 파서
make test-semantic  # 시맨틱 분석
make test-transpile # 트랜스파일러
make test-memory    # 메모리 레이아웃
```

현재 107개 테스트 통과 (시맨틱 29 + 트랜스파일러 34 + 메모리 44).

## 슬롯 시스템

| 타입 | 설명 |
|------|------|
| `Slot<T>` | 기본 타입 안전 슬롯 |
| `SecureSlot<T>` | 토큰 기반 접근 제어 슬롯 |

| 연산 | 설명 |
|------|------|
| `ClaimSlot<T>()` | 슬롯 할당 |
| `Write(slot, value)` | 값 쓰기 |
| `Read(slot)` | 값 읽기 |
| `Release(slot)` | 슬롯 해제 |

런타임은 `PGY_PANIC`으로 다음을 방지합니다:
- 해제 후 읽기/쓰기
- 이중 해제
- 잘못된 토큰 접근

## 문서

- [문법 정의](docs/grammar.md)
- [비동기/동시성](docs/async_concurrency.md)
- [제네릭 설계](docs/generic_design.md)
- [보안 모드 설계](docs/security_mode_design.md)
- [네이밍 규칙](docs/naming_conventions.md)
- [구문 레퍼런스](doc/syntax.md)

## 라이센스

BSD 3-Clause License. [LICENSE](LICENSE) 참조.
