# Pergyra Programming Language

> 메모리를 슬롯으로, 실행을 의도로 -- 슬롯 기반 시스템 프로그래밍 언어

## 개요

Pergyra는 포인터 대신 **슬롯 기반 메모리 관리**를 채택한 시스템 프로그래밍 언어입니다.
LLVM 지원 빌드에서는 LLVM을 기본 백엔드로 사용하고, 그렇지 않은 경우 C 백엔드로 폴백합니다.

```
.pgy  -->  Lexer  -->  Parser  -->  Semantic  -->  HIR  -->  LLVM Backend  -->  Object  -->  Binary
                                                      \\-> C Backend     -->  C       -->  GCC --> Binary
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
# LLVM 기본 백엔드 빌드
make LLVM_ENABLED=1 all

# 전체 빌드(C 폴백)
make all

# Hello World 실행
./bin/pgy examples/hello.pgy --run -v

# LLVM IR 출력
./bin/pgy examples/hello.pgy --emit-llvm -o hello.ll

# 슬롯 데모 실행
./bin/pgy examples/slots.pgy --run -v
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
    codegen/        # C 백엔드
    compiler/       # 컴파일 파사드와 네이티브 빌드
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

# LLVM 백엔드 빌드 + 테스트
make llvm-test-all

# C/LLVM 결과 비교 회귀 테스트
make llvm-test-backend-compare

# 개별 테스트
make test           # 렉서
make test-parser    # 파서
make test-semantic  # 시맨틱 분석
make test-transpile # C 백엔드
make test-memory    # 메모리 레이아웃
```

현재 `make test-semantic`, `make test-transpile` 기준 핵심 프론트엔드 테스트가 통과합니다.

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

## TODO

- [ ] 어셈블리 최적화 런타임 (x86-64 슬롯 연산)
- [ ] 비동기 런타임 (Fiber, M:N 스케줄러, Channel)
- [ ] Effect System (I/O, Timer 등 부작용 타입화)
- [ ] LLVM 백엔드 안정화 및 최적화
- [ ] JVM 연동 (JNI 브릿지)
- [ ] 패턴 매칭 코드 생성
- [ ] Role/Party/World 시스템 코드 생성
- [ ] 표준 라이브러리
- [ ] 패키지 매니저 / 모듈 시스템
- [ ] WebAssembly 타겟
- [ ] LSP 서버 (IDE 지원)
- [ ] 디버거

## 문서

- [컴파일러 파이프라인 가이드](docs/20_compiler_pipeline_guide.md)
- [현재 진행 상황](docs/00_progress.md)
- [구문 레퍼런스](docs/grammar/01_syntax.md)
- [문법 정의](docs/grammar/02_grammar.md)
- [네이밍 규칙](docs/grammar/03_naming.md)
- [언어 상태 평가](docs/18_language_status.md)
- [보안 모드 설계](docs/03_security_mode_design.md)
- [제네릭 설계](docs/04_generic_design.md)
- [비동기/동시성 설계](docs/05_async_concurrency.md)
- [개발 현황](docs/17_development_status.md)
- [Intrinsic Template 개요](docs/intrinsic_templates/README.md)

## 라이센스

BSD 3-Clause License. [LICENSE](LICENSE) 참조.
