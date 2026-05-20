# Pergyra Programming Language

> 현재 상태: 실행 가능한 experimental alpha이며, 기능 체감 진행도는 약 70%,
> strict beta readiness는 기준값 약 60%, 현재 실무 판단은 약 68%로 본다.
> 이 문서는 소개용 문서이고, 베타 계약의 최종 기준은 `docs/107_beta_stable_subset.md`,
> `docs/100_beta_readiness_checklist.md`, `docs/118_slot_model_rigor_audit.md`,
> `docs/119_pergyra_lineage_positioning.md`, `docs/120_vision_and_capability_audit.md`다.
> Pergyra를 production-ready, Rust-level memory safe, AI-first, quantum-ready,
> zero-cost, fully proven 언어로 설명하지 않는다.

> 서로 다른 자원을 같은 사고 체계로 다루기 위한 의미 통일 언어

## 사명

Pergyra는 자원의 세부 구현을 직접 드러내기보다, 서로 다른 자원들을 같은 사고 체계로 다룰 수 있게 만드는 **의미 통일 언어**입니다.

PergyraLang은 자원을 **Slot 계열 핸들 + 비동기 경계**라는 공통 모델로 다룬다.

`Slot<T>`, `SecureSlot<T>`, `DeviceSlot<T>`는 로컬에 고정된 anchored 자원 핸들이고,
`QubitSlot`은 현재 partial quantum surface 위의 복사 불가 move-only 자원 핸들이다.
원격 작업은 슬롯 자체를 직접 넘기기보다 `RemoteFuture<T>`를 `await`해 `Result<T>`로 회수한다.

이로써 언어는 최고 성능 대신, **이종 자원 통합**과 **도메인 파편화 감소**를 목표로 한다.

| Slot이 담는 것 | 예시 |
|----------------|------|
| 수명(Lifetime) | 스코프 진입 시 점유, 탈출 시 반환 |
| 소유권(Ownership) | anchored handle은 단일 바인딩 유지, movable handle은 명시적 이동만 허용 |
| 접근 권한(Permission) | SecureSlot의 토큰 기반 읽기/쓰기 제어 |
| 해제 의미론(Release) | `with` 블록 자동 해제, 명시적 `Release` |
| 보호 의미론(Protection) | 관측이 상태를 파괴하는 양자 자원도 동일 모델 |
| 전이 의미론(Transfer) | `QubitSlot` 이동, `recv/await` 경계 전달, 원격 결과는 `RemoteFuture<T>` → `Result<T>` |

## 개요

Pergyra는 포인터나 도메인별 핸들 API를 언어의 중심에 두기보다, **Slot**과 **병렬 오케스트레이션**으로 이종 자원의 수명과 흐름을 통일하려는 언어입니다.
LLVM 지원 빌드에서는 LLVM을 기본 백엔드로 사용하고, 그렇지 않은 경우 C 백엔드로 폴백합니다.

```
.pgy  -->  Lexer  -->  Parser  -->  Semantic  -->  HIR  -->  DIR  -->  RIR  -->  MIR  -->  Backend
                                                    │                  │                   ├→ LLVM → Object → Binary
                                                    │                  │                   └→ C    → C → GCC/Clang
                                                    ↓                  ↓
                                                    AIR (read-only synthesis, verification-only)
                                                                       ↓
                                                    drift / abstraction-safety 검증
```

> `AIR`는 codegen path에 얹지 않고 옆에 합성되는 verification-only IR입니다 (`HIR`/`RIR`로부터 단방향 합성, backend로 lowering 안 됨). 자세한 내용은 `docs/104_air_compiler_architecture.md`.

### 핵심 특징

- **Slot 기반 자원 모델**: `Slot<T>`/`SecureSlot<T>`/`DeviceSlot<T>`는 anchored handle, `QubitSlot`은 move-only handle로 구분
- **보안 슬롯**: `SecureSlot<T>`에 토큰 기반 접근 제어
- **Subject-first 철학**: `subject`는 코어 host, `class`는 보조 nominal value 축으로 semantic/codegen에서 실제로 구분됨
- **제네릭 클래스**: `class Pair<T>` 단형화 기반 제네릭 (Pair<Int> → Pair_Int)
- **비동기 오케스트레이션**: `async/await`, `spawn`, `Channel<T>`, `select`, `parallel`
- **원격 결과 의미론**: `RemoteFuture<T>`를 `await`하면 `Result<T>`가 되어 실패 가능성을 타입에 남김
- **내장 병렬성**: `parallel` 블록으로 선언적 병렬 처리
- **스코프 기반 해제**: `with` 블록으로 자동 자원 반환
- **양자 자원 표면**: `QubitSlot`/`ClaimQubit`/`Measure`/`Entangle` 표면은 존재하지만, 전체 quantum resource semantics는 아직 v2 작업

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

# 직접 회귀 진입점
make test-transpile
make test-abi
make llvm-test-backend-compare
make example-test-smoke
make ir-pipeline-test-smoke
make fmt-test-smoke
make dogfood-webgl-test-smoke
```

WebGL note: `examples/wasm_hello/` is a C `--emit-c` host-bridge dogfood gate,
not stable WebGL language surface. `pgy.render.webgl` is post-beta module work.

안정 예제 가이드:

- smoke-covered 예제 목록은 [docs/65_stable_example_surface_board.md](65_stable_example_surface_board.md)를 기준으로 본다
- 현재 바로 따라 써도 되는 대표 예제:
  - `examples/logistics_intent_probe/`
  - `examples/resource_scheduler_async_probe/`
  - `examples/order_analytics/`
  - `examples/battle_simulator/`
  - `examples/biome_simulator/`
  - `examples/wasm_hello/`
- `examples/party_system_demo.pgy`, `examples/world_roster_city.pgy` 같은 예제는 design sketch이며 stable syntax reference가 아니다

## 문법 예제

```pergyra
func Main() -> Void {
    let msg: Slot<String> = "Hello, Pergyra!";
    Log(msg);
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
let ss: SecureSlot<Int> = ClaimSecureSlot();
let rv: ReadView<Int> = ViewRead(ss);
let wv: WriteView<Int> = ViewWrite(ss);
Write(wv, 42);
Log(Read(rv));
```

## 프로젝트 구조

```
PergyraLang/
  src/
    lexer/          # 토크나이저
    parser/         # AST 생성
    semantic/       # 타입 검사, 슬롯 분석
    codegen/        # C/LLVM 백엔드
    compiler/       # 컴파일 파사드와 네이티브 빌드
    runtime/        # 슬롯/병렬/채널/큐비트 런타임
    pgy_driver.c    # 컴파일러 드라이버
  examples/         # .pgy 예제 파일
  docs/             # 언어 설계 문서
  Makefile
```

## 테스트

```bash
# C/LLVM 결과 비교 회귀 테스트
make llvm-test-backend-compare

# 개별 테스트
make test           # 렉서
make test-parser    # 파서
make test-semantic  # 시맨틱 분석
make test-transpile # C 백엔드
make test-memory    # 메모리 레이아웃
```

현재 직접 확인한 범위는 다음과 같습니다.

- `make test-transpile` 통과 (`470 passed`)
- `make test-abi` 통과 (`56 passed`)
- `make llvm-test-backend-compare` 통과
- `make example-test-smoke` 통과
- `make ir-pipeline-test-smoke` 통과
- `make fmt-test-smoke` 통과

`make llvm-test-backend-compare`는 대표 예제 코퍼스에 대해 C/LLVM 결과를 비교하지만, 이것만으로 전체 기능 parity를 선언하는 문서는 아닙니다.

## 예제 신뢰도

모든 예제를 같은 강도로 믿으면 안 됩니다.

- `compile-smoke covered`: 현재 회귀 smoke가 직접 밟는 예제, stable reference로 우선 추천
- `design sketch`: 미래 표면이나 aspirational syntax를 보여 주는 예제, stable reference로 사용 금지

source of truth:

- [Stable Example Surface Board](65_stable_example_surface_board.md)

추천 stable 예제:

- `examples/logistics_intent_probe/`
- `examples/resource_scheduler_async_probe/`
- `examples/order_analytics/`
- `examples/subject_object_tobject/`
- `examples/ownership_forwarding_probe/`

design sketch 예제:

- `examples/party_system_demo.pgy`
- `examples/world_roster_city.pgy`

## Slot 시스템

Slot은 "무엇을 가리키는가(handle)"가 아니라, **"어떻게 다뤄야 하는가(규율)"**를 정의한다.

| 타입 | 자원 | 규율 |
|------|------|------|
| `Slot<T>` | 메모리 | anchored handle: 점유 → 읽기/쓰기 → 반환 |
| `SecureSlot<T>` | 보안 메모리 | anchored handle: 토큰 없이 접근 불가 |
| `DeviceSlot<T>` | 디바이스/가속기 자원 | anchored handle: device read/write/submit/release |
| `QubitSlot` | 양자 큐비트 | movable handle: 복사 금지, partial quantum surface 위에서 move/measure/entangle 표면 제공 |

현재 `own/ref` 함수 경계 규칙은 일반 자원 전체에 열린 것이 아닙니다.
직접 확인 가능한 안정 범위는 `ref Slot<subject-host>` / `own SecureSlot<subject-host>` 입니다.
즉 `own/ref`는 현재 "전체 ownership 시스템"이라기보다 anchored subject-slot boundary subset입니다.

| 연산 | 설명 |
|------|------|
| `ClaimSlot<T>()` | 자원 점유 |
| `Write(slot, value)` | 값 쓰기 |
| `Read(slot)` | 값 읽기 |
| `Release(slot)` | 자원 반환 |
| `SubmitDeviceRead(slot)` | 원격/디바이스 읽기 제출 → `RemoteFuture<T>` |

`await` 결과는 경계 종류에 따라 다릅니다: `Future<T>`는 `T`를, `RemoteFuture<T>`는 `Result<T>`를 돌려줍니다.

런타임은 `PGY_PANIC`으로 다음을 방지합니다:
- 해제 후 읽기/쓰기
- 이중 해제
- 잘못된 토큰 접근
- 붕괴된 큐비트에 게이트 연산 시도

## TODO

- [ ] 어셈블리 최적화 런타임 (x86-64 슬롯 연산)
- [ ] 오케스트레이션 모델 강화 (`select` 공정성, timeout, cancellation, backpressure)
- [ ] Effect System (I/O, Timer 등 부작용 타입화)
- [ ] LLVM 백엔드 최적화 및 coverage 확장
- [ ] JVM 연동 (JNI 브릿지)
- [ ] 패턴 매칭 고도화
- [ ] 표준 라이브러리 안정화
- [ ] 패키지 매니저
- [ ] 모듈 시스템 안정화
- [ ] WebAssembly 타겟
- [ ] product-grade debugger/runtime integration

## 문서

- [컴파일러 파이프라인 가이드](docs/20_compiler_pipeline_guide.md)
- [현재 진행 상황](docs/00_progress.md)
- [구문 레퍼런스](docs/grammar/01_syntax.md)
- [문법 정의](docs/grammar/02_grammar.md)
- [네이밍 규칙](docs/grammar/03_naming.md)
- [언어 상태 평가](docs/18_language_status.md)
- [보안 모드 설계](docs/03_security_mode_design.md)
- [제네릭 설계](docs/04_generic_design.md)
- [언어 비전](docs/00_vision.md)
- [비동기/동시성 설계](docs/05_async_concurrency.md)
- [개발 현황](docs/17_development_status.md)
- [Class 객체 모델](docs/22_class_object_model.md)
- [Intrinsic Template 개요](docs/intrinsic_templates/README.md)

## 양자 컴퓨팅 대응 설계

Pergyra의 Slot 모델은 클래식 메모리 관리를 넘어, 양자 자원 제어의 기반으로 설계되었습니다.

### 1. 복제 불가 정리(No-Cloning Theorem)와 Slot

고전 컴퓨터에서는 포인터로 메모리를 무한정 복사(`copy = *ptr`)할 수 있지만,
양자 역학의 복제 불가 정리에 의해 임의의 큐비트 상태는 완벽하게 복사할 수 없습니다.

Pergyra가 메모리 주소를 숨기고 자원을 Slot 단위로 추상화하여,
복사 대신 점유(Claim)하고 반환(Release)하도록 강제한 구조는
양자 정보 이론의 선형 논리(Linear Logic)와 일치합니다.
큐비트는 변수처럼 대입되는 것이 아니라, Slot처럼 물리적 흐름으로 전달되어야 합니다.

### 2. 관측에 의한 붕괴(Measurement Collapse)와 권한 제어

큐비트는 관측(Read)하는 순간 중첩 상태가 붕괴되어 0 또는 1로 확정됩니다.
"누가 언제 자원을 읽느냐"가 데이터의 상태를 영구적으로 파괴하는 물리적 행위가 됩니다.

SecureSlot과 Party 시스템이 자원 접근을 토큰과 권한 기반으로 통제하는 것은,
큐비트의 관측 시점을 컴파일 타임에 엄격하게 제어하고
의도치 않은 상태 붕괴를 막기 위한 방어 메커니즘으로 발전할 수 있습니다.

### 3. 양자 얽힘(Entanglement)과 댕글링 문제

두 큐비트가 얽혀 있으면, A에 대한 측정이 B의 상태를 즉각 변화시킵니다.
이것은 고전 컴퓨팅의 댕글링 포인터와 구조적으로 동일한 문제입니다:

| 고전 (포인터) | 양자 (큐비트) |
|--------------|--------------|
| `free(a)` 후 `*b`가 댕글링 | `Measure(a)` 후 `b`가 붕괴됨을 모르는 상태 |
| 해결: Slot의 Release 추적 | 해결: 얽힘 관계 그래프 + Measure 전파 추적 |

Pergyra의 Party 시스템이 이 문제를 풀 수 있는 구조를 가지고 있습니다:

```pergyra
// v2 방향 — 얽힘 = Party 관계로 추적
party EntangledPair {
    role qubitA: QubitSlot;
    role qubitB: QubitSlot;
}

let pair: EntangledPair = Entangle(ClaimQubit(), ClaimQubit());
let resultA: Bool = Measure(pair.qubitA);
// pair.qubitB는 이제 COLLAPSED — 게이트 연산 불가
// 컴파일러가 Measure 전파를 추적하여 댕글링 얽힘을 방지
```

### 4. 한계와 도전 과제

- **얽힘 상태 추적**: `slot_analyzer`가 독립 자원의 경계 상태는 추적하지만, 얽힌 자원 간 상태 전파 모델링이 필요
- **가역 연산**: 관측 외의 모든 양자 게이트는 유니터리(가역적)여야 함 — 파괴적 대입 연산은 양자에서 불가능
- **선형 타입 시스템**: Slot의 단일 소유권을 Linear/Affine 타입으로 정식화해야 컴파일 타임 검증이 완전해짐

자세한 내용은 [설계 비전 문서](docs/00_vision.md)를 참조하세요.

## 라이센스

BSD 3-Clause License. [LICENSE](LICENSE) 참조.
